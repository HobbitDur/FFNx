/****************************************************************************/
//    Copyright (C) 2026 HobbitDur                                          //
//                                                                          //
//    This file is part of FFNx                                             //
//                                                                          //
//    FFNx is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU General Public License as published by  //
//    the Free Software Foundation, either version 3 of the License         //
//                                                                          //
//    FFNx is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//    GNU General Public License for more details.                          //
/****************************************************************************/

// 30 fps part of mag179_bomb_glow.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag179_bomb_glow.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Both drawing tasks draw THEN update. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw blocks keep the scratch bytes where
// the code does not write). A held frame replays the log in the same order, each part at its
// exact in-between state (a + (b - a) * num / den between what tick N drew and what tick N+1
// draws); discrete things (flipbook frames, particles born or gone) are those of tick N up to half
// way and those of tick N+1 after (a held frame at fraction 1/2 keeps tick N's):
//   CasterGlow - the caster at its in-between pose (pose_midpoint), the overlay colour (the depth-
//                cued colours of both ticks, channel by channel) and the v scroll half way to the
//                next tick's; the last draw holds. Vertices go to a private copy of the vertex
//                workspace, packets to a private copy of the frame arena.
//   FuseSparks - fixed particles; flipbook frame discrete; particles born on the tick (spawns
//                after the draws, 15 Hz batches) appear past half way.
// The module packet arena alternates with the tick's parity: the held packets start where the
// tick's first draw started (and past half way sit on the other arena's bytes, the next tick's).
// In the draw-only mode nothing moves: every part holds.

namespace ff8fx
{
namespace bombglow179
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t POOL_BYTES = POOL_END - POOL;
	enum : uint32_t { REC_GLOW = 1, REC_POOL = 2 };

	struct Rec
	{
		uint32_t kind;
		EffectNode node;              // the node as drawn
		Particle pool[POOL_COUNT];    // pool: the particles as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0;    // frame arena cursor at the glow's draw
	static Rec g_recs[4];
	static uint8_t g_held_packets[ARENA_SIZE]; // module arena
	static uint8_t g_held_frame[0x40000];   // frame arena (caster model packets)
	static uint8_t g_held_verts[0x10000];   // vertex workspace (indices are 12-bit: 0x8000)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(uint32_t kind, const EffectNode *t)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = *t;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_glow(const EffectNode *t)
	{
		if (!NewRec(REC_GLOW, t)) return;
		g_frame_packets = FrameCursor();
	}

	static void held_note_pool(const EffectNode *t)
	{
		Rec *r = NewRec(REC_POOL, t);
		if (r) memcpy(r->pool, Pool(), POOL_BYTES);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void GlowHeld(const Rec &r, int num, int den, bool moving)
	{
		const int16_t c = r.node.counter;
		const int16_t nc = moving ? (int16_t)(c + 1) : c;
		const bool next = nc < 0x1E;
		if (!next && 2 * num > den) return; // the next tick draws nothing
		uint8_t *entity = Caster();
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		// the draw starts from the real one's GTE state; the next tick's colour on the side
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		uint8_t next_colour[0x70];
		if (next)
		{
			GlowColour((uint32_t)next_colour, nc, GlowLevel(nc));
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		}
		// the frame arena and the vertex workspace go to private copies
		const uint32_t frame = FrameCursor(), verts = BoneWorkspace();
		if (g_frame_packets && frame > g_frame_packets)
		{
			const uint32_t used = frame - g_frame_packets;
			memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
		}
		if (verts) memcpy(g_held_verts, (const void *)verts, sizeof(g_held_verts));
		FrameCursor() = (uint32_t)g_held_frame;
		BoneWorkspace() = (uint32_t)g_held_verts;
		const uint32_t h = (uint32_t)FieldAlloc(0xB0);
		GlowColour(h, c, GlowLevel(c));
		int32_t scroll = GlowScroll(c);
		if (next)
		{
			for (int k = 0; k < 4; k++) AT<uint8_t>(h, 0x6C + k) = (uint8_t)lerp_i(AT<uint8_t>(h, 0x6C + k), next_colour[0x6C + k], num, den);
			scroll = lerp_i(scroll, GlowScroll(nc), num, den);
		}
		GlowBlock(h, scroll);
		entity[0] |= 4;
		DrawCasterModel(h, entity);
		FieldFree(0xB0);
		BoneWorkspace() = verts;
		FrameCursor() = frame;
	}

	static void PoolHeld(const Rec &r, int num, int den, bool moving)
	{
		const Particle *pre = r.pool, *post = Pool();
		static Particle q[POOL_COUNT];
		const bool late = 2 * num > den;
		int alive = 0;
		for (int i = 0; i < POOL_COUNT; i++)
		{
			const bool pa = (pre[i].type & 1) != 0, pb = (post[i].type & 1) != 0;
			// the same particle on both ticks: the update kept it (a particle born after the draws
			// starts at frame 0)
			const bool cont = pa && pb && (!moving || post[i].frame == (int16_t)(pre[i].frame + 1));
			if (cont) alive++;
			q[i] = late ? post[i] : pre[i];
			if (!(late ? pb : pa)) q[i].type = 0;
		}
		// the task ended on the tick: the next tick draws nothing
		if (late && moving && (int16_t)(r.node.counter + 1) >= 4 && alive == 0) return;
		uint8_t *h = SparkHeader();
		for (int i = 0; i < POOL_COUNT; i++)
			if (q[i].type & 1) DrawFuseParticle(h, &q[i]);
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// past half way the draws are the next tick's: under them, what the next tick's arena (the
		// other one of the pair) holds, as the bytes its packets do not write (pad halves)
		if (2 * num > den && g_packets)
		{
			const uint32_t other = g_packets >= ARENA1 ? g_packets - ARENA_SIZE : g_packets + ARENA_SIZE;
			memcpy(g_held_packets, (const void *)other, sizeof(g_held_packets));
		}
		const bool moving = !DrawOnly();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_GLOW: GlowHeld(r, num, den, moving); break;
			case REC_POOL: PoolHeld(r, num, den, moving); break;
			}
		}
	}
}

	static void register_mag179_held()
	{
		mark_held(bombglow179::ORIG_CasterGlow);
		mark_held(bombglow179::ORIG_FuseSparks);
		register_module_held(179, bombglow179::HeldReady, bombglow179::HeldFrame);
	}
}
