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

// 30 fps part of mag180_suicide.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag180_suicide.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Suicide task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes
// where the code does not write). A held frame replays the log in the same order, each part at
// its exact in-between state (a + (b - a) * num / den between what tick N drew and what tick N+1
// draws); discrete things (flipbook frames, fade flags, particles born or gone) are those of
// tick N up to half way and those of tick N+1 after (a held frame at fraction 1/2 keeps tick N's):
//   CasterGlow - the caster at its in-between pose (pose_midpoint), overlay level (IR0) half way
//                to the next tick's; the last draw holds. Vertices go to a private copy of the
//                vertex workspace, packets to a private copy of the frame arena.
//   Explosion  - every type-1 particle of tick N (its pool copy) that the update kept: position,
//                velocity (the stretch direction), size and fade half way to the next tick's (the
//                pool after the tick); frame (fade flags) discrete. A particle that ended holds.
//   Sparks     - type 2: position half way; flipbook frame discrete; an ended one holds.
//   Smoke      - type 4: fixed; flipbook frame discrete.
//   FuseSparks - type 8: fixed; flipbook frame discrete.
//   Ring/Ring2 - spin and size half way (the node's own update on a copy), fade half way inside
//                the fade ramp; the ring's last draw holds.
// Particles born on the tick (spawns after the draws, 15 Hz batches) appear on the next tick.
// In the draw-only mode nothing moves: every part holds.

namespace ff8fx
{
namespace suicide180
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t POOL_BYTES = POOL_END - POOL;
	enum : uint32_t { REC_GLOW = 1, REC_POOL = 2, REC_RING = 3 };

	struct Rec
	{
		uint32_t kind;
		uint32_t fn;                  // original task function
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
	static Rec g_recs[12];
	static uint8_t g_held_packets[0xC000];  // module arena (0xC000 per tick)
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

	static Rec *NewRec(uint32_t kind, uint32_t fn, const EffectNode *t)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.fn = fn;
		r.node = *t;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_glow(const EffectNode *t)
	{
		if (!NewRec(REC_GLOW, ORIG_CasterGlow, t)) return;
		g_frame_packets = FrameCursor();
	}

	static void held_note_pool(uint32_t fn, const EffectNode *t)
	{
		Rec *r = NewRec(REC_POOL, fn, t);
		if (r) memcpy(r->pool, Pool(), POOL_BYTES);
	}

	static void held_note_ring(uint32_t fn, const EffectNode *t) { NewRec(REC_RING, fn, t); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void GlowHeld(const Rec &r, int num, int den, bool moving)
	{
		const int16_t c = r.node.counter;
		const int16_t nc = moving ? (int16_t)(c + 1) : c;
		const bool next = nc < 0x18;
		if (!next && 2 * num > den) return; // the next tick draws nothing
		const int32_t level = next ? lerp_i(GlowLevel(c), GlowLevel(nc), num, den) : GlowLevel(c);
		uint8_t *entity = Caster();
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
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
		DrawGlow(entity, level);
		BoneWorkspace() = verts;
		FrameCursor() = frame;
	}

	static void RingHeld(const Rec &r, int num, int den, bool moving)
	{
		const bool ring1 = r.fn == ORIG_Ring;
		const int16_t fs = ring1 ? 8 : 0xA;
		const EffectNode &a = r.node;
		EffectNode b = a;
		if (moving)
		{
			if (ring1) RingUpdate(&b);
			else Ring2Update(&b);
		}
		const bool ended = moving && b.counter >= (ring1 ? 0x10 : 0x12);
		const bool late = 2 * num > den;
		if (ended && late) return; // the next tick draws nothing
		EffectNode m = late ? b : a;
		m.spin = lerp16(a.spin, b.spin, num, den);
		m.scale = lerp16(a.scale, b.scale, num, den);
		int32_t fade = shl32(m.counter - fs, 9);
		if (a.counter >= fs && b.counter >= fs) fade = lerp_i(shl32(a.counter - fs, 9), shl32(b.counter - fs, 9), num, den);
		DrawRing(&m, ring1 ? MODEL_Ring : MODEL_Ring2, fs, fade);
	}

	static void PoolHeld(const Rec &r, int num, int den, bool moving)
	{
		uint32_t type = 0;
		int16_t end_counter = 4;
		switch (r.fn)
		{
		case ORIG_Explosion: type = 1; end_counter = 6; break;
		case ORIG_Sparks: type = 2; break;
		case ORIG_Smoke: type = 4; break;
		case ORIG_FuseSparks: type = 8; break;
		}
		const Particle *pre = r.pool, *post = Pool();
		static Particle q[POOL_COUNT];
		int32_t fade[POOL_COUNT];
		const bool late = 2 * num > den;
		int alive = 0;
		for (int i = 0; i < POOL_COUNT; i++)
		{
			const bool pa = (pre[i].type & type) != 0, pb = (post[i].type & type) != 0;
			// the same particle on both ticks: the update kept it (a particle born after the draws
			// starts at frame 0)
			const bool cont = pa && pb && (!moving || post[i].frame == (int16_t)(pre[i].frame + 1));
			if (cont) alive++;
			q[i] = late ? post[i] : pre[i];
			if (!(late ? pb : pa)) q[i].type = 0;
			fade[i] = shl32(q[i].frame - 2, 10);
			if (!cont) continue;
			for (int k = 0; k < 3; k++)
			{
				q[i].pos[k] = lerp16(pre[i].pos[k], post[i].pos[k], num, den);
				q[i].vel[k] = lerp16(pre[i].vel[k], post[i].vel[k], num, den);
			}
			q[i].scale = lerp16(pre[i].scale, post[i].scale, num, den);
			if (pre[i].frame >= 2 && post[i].frame >= 2) fade[i] = lerp_i(shl32(pre[i].frame - 2, 10), shl32(post[i].frame - 2, 10), num, den);
		}
		// the task ended on the tick: the next tick draws nothing
		if (late && moving && (int16_t)(r.node.counter + 1) >= end_counter && alive == 0) return;
		if (type == 1)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x58);
			uint8_t *m = (uint8_t *)FieldAlloc(0x88);
			ExplosionHeader(h);
			ExplosionBlock(m);
			for (int i = 0; i < POOL_COUNT; i++)
				if (q[i].type & 1) DrawExplosionParticle(h, m, &q[i], fade[i]);
			FieldFree(0x88);
			FieldFree(0x58);
			return;
		}
		uint8_t *h = SpriteHeader(type == 4 ? SEQ_Smoke : SEQ_Spark);
		if (type == 8) *(int32_t *)(h + 0x14) = 0x1000;
		for (int i = 0; i < POOL_COUNT; i++)
		{
			if (!(q[i].type & type)) continue;
			if (type == 8) DrawFuseParticle(h, &q[i]);
			else DrawSpriteParticle(h, &q[i]);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// past half way the draws are the next tick's: under them, what the next tick's arena (the
		// other one of the pair) holds, as the bytes its packets do not write (pad halves)
		if (2 * num > den && g_packets)
		{
			const uint32_t other = g_packets >= TexBase() + 0xC000 ? g_packets - 0xC000 : g_packets + 0xC000;
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
			case REC_RING: RingHeld(r, num, den, moving); break;
			}
		}
	}
}

	static void register_mag180_held()
	{
		mark_held(suicide180::ORIG_CasterGlow);
		mark_held(suicide180::ORIG_Explosion);
		mark_held(suicide180::ORIG_Ring);
		mark_held(suicide180::ORIG_Ring2);
		mark_held(suicide180::ORIG_Sparks);
		mark_held(suicide180::ORIG_Smoke);
		mark_held(suicide180::ORIG_FuseSparks);
		register_module_held(180, suicide180::HeldReady, suicide180::HeldFrame);
	}
}
