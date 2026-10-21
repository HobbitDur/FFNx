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

// 30 fps part of mag306_degenerator_quistis.cpp (Quistis' Degenerator): the draw log of the real tick and the
// held-frame drawing (included at the end of mag306_degenerator_quistis.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records every drawing task in queue order: the node as
// drawn, the GTE registers and the Field_Alloc scratch its draw started from (header fields a draw does not
// set keep what the scratch held), the Debris pool as drawn, the Swallow target's world matrix (+0x40) as
// drawn. A held frame replays the log in the same order, each part at fraction num / den of the way to the
// next tick's draw (the live node one update later; nothing random is drawn, nothing is written back):
//   Core / Pulse     - scale half way to the next tick's (Core shrinks by a constant step, Pulse grows by a
//                      decaying one: the live node's value), depth cue half way (a cue on one side only: from
//                      / to level 0).
//   ScreenRing       - spin angle and depth cue half way.
//   TargetRing       - spin angle, scale and depth cue half way.
//   Vortex           - spin angle, scale, depth cue and the colour channels half way to the next entry of the
//                      12-colour cycle.
//   Flicker          - drawn on odd counters only (a discrete switch): a held frame below 1 shows the real
//                      tick's picture; at 1 the next tick's (drawn when the next counter is odd).
//   Swallow          - the target at its in-between pose (pose_midpoint, body and second model), the pull
//                      factor half way (c / 30 -> (c + 1) / 30), its world matrix (+0x40) half way from the one
//                      the real draw used to the one the frame holds (the Pull task turns and moves the target
//                      at 15 Hz; the engine rebuilds +0x40 from the entity's rotation / position every frame:
//                      element-wise in-between, equal ends = exactly as drawn); vertices into a private vertex
//                      buffer, packets into a private copy of the frame arena, the shadow temporaries put back.
//   Debris           - per pool slot, in pool order: a particle the tick moved (same slot, frame + 1) at the
//                      int16 lerp of position and size, its flipbook frame held below 1; one the tick freed
//                      (sequence ended) holds below 1; one the tick spawned is drawn at 1 only.
// A task whose next tick draws nothing (its last draw; Swallow's counter 0x1D) holds below 1 and is not drawn
// at 1. Parts the Controller spawns on the next tick are not drawn before they exist (fraction 1 = a subsequence
// of the next tick). Draw-only ticks (battle_to_update_flags & 0x201) do not move: everything holds.
// Not drawn by the module (held by the engine at 15 Hz, as the real ticks set them): the faded models' colour
// words (FadeIn / FadeOut), the Pull's target rotation / position, the stage darkening (Effect_SetScreenFlash)
// and the stage groups' draw bit. The hand / target points are taken once (Controller counters 0xA / 0x2A):
// no part follows an animating anchor; only Swallow draws an animating model.
// The held frame draws into private packet buffers (module arena, frame arena, vertex buffer) and puts back
// the Field_Alloc scratch, the shadow scratches, RenderGeometry's words, the target's matrix / flags and the
// GTE.

namespace ff8fx
{
namespace degen306
{
	static const uint32_t SCRATCH = 0x400;   // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 0x40, MAX_POOLS = 2;

	struct Rec
	{
		uint32_t fn;                           // task function (kind)
		uint32_t node_addr;                    // the live node
		uint8_t node[0x24];                    // node as drawn
		bool draw_only;
		int pool;                              // Debris: index of its pool copy
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];              // Field_Alloc scratch under the draw's headers
		uint8_t mat[0x20];                     // Swallow: the target's world matrix as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npool = 0;
	static uint32_t g_packets = 0;             // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0;       // frame arena cursor at the tick's first Swallow draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_pools[MAX_POOLS][PART_COUNT * 0x18];
	static uint8_t g_held_packets[0xC000];     // one module packet arena
	static uint8_t g_held_frame[0x40000];      // shadow + swallowed model packets
	static uint8_t g_held_verts[0x10000];      // swallowed model vertices (12-bit indices: 0x8000)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npool = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static void held_note_controller(const TaskNode *node) { (void)node; }

	static Rec *NewRec(uint32_t fn, uint32_t n, uint32_t scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node_addr = n;
		memcpy(r.node, (const void *)n, sizeof(r.node));
		r.draw_only = DrawOnly();
		r.pool = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)scratch, SCRATCH);
		return &r;
	}

	// a drawing task about to draw (Flicker: every run, it draws on odd counters)
	static void held_note_draw(uint32_t fn, uint32_t n)
	{
		Rec *r = NewRec(fn, n, var<uint32_t>(0x1D999C4));
		if (!r || fn != ORIG_Swallow) return;
		memcpy(r->mat, (const void *)(Ent(TargetSlot()) + 0x40), sizeof(r->mat));
		if (!g_frame_packets) g_frame_packets = FrameCursor();
	}

	// Debris about to draw its particles (h = its sequence header, allocated first)
	static void held_note_debris(uint32_t n, uint32_t h)
	{
		if (g_npool >= MAX_POOLS) return;
		Rec *r = NewRec(ORIG_Debris, n, h);
		if (!r) return;
		r->pool = g_npool++;
		memcpy(g_pools[r->pool], (const void *)PARTS, sizeof(g_pools[0]));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// in-between helpers
	// ------------------------------------------------------------------
	// the live node is the drawn one one update later (the next tick draws it again when its counter
	// stays below `end`)
	static bool NodeNext(const Rec &r, int32_t end)
	{
		if (r.draw_only) return false;
		const int32_t c = *(const int16_t *)(r.node + 0xC);
		return S16(r.node_addr, 0xC) == c + 1 && c + 1 < end;
	}

	static Cue LerpCue(Cue a, Cue b, int num, int den)
	{
		if (num <= 0) return a;
		if (num >= den) return b;
		const bool ca = a.flags == 0xF3, cb = b.flags == 0xF3;
		if (!ca && !cb) return a;
		return { 0xF3u, lerp_i(ca ? a.fade : 0, cb ? b.fade : 0, num, den) };
	}

	// colour channels (bytes 0..2) half way, the code byte of a
	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t c = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			c |= (uint32_t)(lerp_i((int32_t)((a >> k) & 0xFF), (int32_t)((b >> k) & 0xFF), num, den) & 0xFF) << k;
		return c;
	}

	// the frame arena, vertex buffer and shadow temporaries a held Swallow draw writes: private copies, all
	// put back
	struct FrameScope
	{
		uint32_t cursor, verts, words[4];
		uint8_t shadow[0x80];
		FrameScope()
		{
			verts = VertexBuffer();
			cursor = FrameCursor();
			words[0] = var<uint32_t>(0xB8B9E4);
			words[1] = var<uint32_t>(0xB8B9EC);
			words[2] = var<uint32_t>(0x1D99C18);
			words[3] = var<uint32_t>(0x1D99C1C);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			if (g_frame_packets && cursor > g_frame_packets)
			{
				const uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			VertexBuffer() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0x1D99C1C) = words[3];
			var<uint32_t>(0x1D99C18) = words[2];
			var<uint32_t>(0xB8B9EC) = words[1];
			var<uint32_t>(0xB8B9E4) = words[0];
			VertexBuffer() = verts;
			FrameCursor() = cursor;
		}
	};

	// ------------------------------------------------------------------
	// held draws (n = a copy of the node as drawn)
	// ------------------------------------------------------------------
	// prim-model parts: the moving words half way to the live node's, the cue half way
	static void ModelHeld(const Rec &r, int num, int den)
	{
		uint8_t copy[0x24];
		memcpy(copy, r.node, sizeof(copy));
		const uint32_t n = (uint32_t)copy, l = r.node_addr;
		const int32_t c = S16(n, 0xC);
		const bool end = num >= den;
		switch (r.fn)
		{
		case ORIG_Core:
		case ORIG_Pulse:
		{
			const bool core = r.fn == ORIG_Core;
			const bool next = NodeNext(r, core ? 0x62 : 0xA);
			if (end && !next && !r.draw_only) return; // the next tick draws none
			Cue cue = core ? CoreCue(c) : PulseCue(c);
			if (next)
			{
				cue = LerpCue(cue, core ? CoreCue(c + 1) : PulseCue(c + 1), num, den);
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(l, 0x1C), num, den);
			}
			HandModelDraw(n, core ? MODEL_Core : MODEL_Pulse, cue);
			break;
		}
		case ORIG_ScreenRing:
		{
			const bool next = NodeNext(r, 0x3A);
			if (end && !next && !r.draw_only) return;
			Cue cue = ScreenRingCue(c);
			if (next)
			{
				cue = LerpCue(cue, ScreenRingCue(c + 1), num, den);
				S16(n, 0x18) = lerp16(S16(n, 0x18), S16(l, 0x18), num, den);
			}
			ScreenRingDraw(n, cue);
			break;
		}
		case ORIG_TargetRing:
		{
			const bool next = NodeNext(r, 0x1C);
			if (end && !next && !r.draw_only) return;
			Cue cue = TargetRingCue(c);
			if (next)
			{
				cue = LerpCue(cue, TargetRingCue(c + 1), num, den);
				S16(n, 0x18) = lerp16(S16(n, 0x18), S16(l, 0x18), num, den);
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(l, 0x1C), num, den);
			}
			TargetRingDraw(n, cue);
			break;
		}
		case ORIG_Vortex:
		{
			const bool next = NodeNext(r, 0x28);
			if (end && !next && !r.draw_only) return;
			Cue cue = VortexCue(c);
			uint32_t colour = VortexColours[c % 12];
			if (next)
			{
				cue = LerpCue(cue, VortexCue(c + 1), num, den);
				colour = end ? VortexColours[(c + 1) % 12] : LerpColour(colour, VortexColours[(c + 1) % 12], num, den);
				S16(n, 0x18) = lerp16(S16(n, 0x18), S16(l, 0x18), num, den);
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(l, 0x1C), num, den);
			}
			VortexDraw(n, colour, cue);
			break;
		}
		}
	}

	// Flicker: the real tick's picture below 1, the next tick's at 1 (drawn on odd counters)
	static void FlickerHeld(const Rec &r, int num, int den)
	{
		const int32_t c = *(const int16_t *)(r.node + 0xC);
		bool drawn = (c & 1) != 0;
		if (num >= den && !r.draw_only)
		{
			if (S16(r.node_addr, 0xC) != c + 1 || c + 1 >= 0x1E) return; // ended: the next tick draws none
			drawn = ((c + 1) & 1) != 0;
		}
		if (drawn) FlickerDraw();
	}

	// Swallow: pose, factor and world matrix half way
	static void SwallowHeld(const Rec &r, int num, int den)
	{
		uint8_t copy[0x24];
		memcpy(copy, r.node, sizeof(copy));
		const uint32_t n = (uint32_t)copy;
		const int32_t c = S16(n, 0xC);
		const bool next = NodeNext(r, 0x1E);
		if (num >= den && !next && !r.draw_only) return; // the last draw: the next tick draws none
		const uint32_t e = Ent(TargetSlot());
		int32_t factor = SwallowFactor(c);
		int pnum = 0;
		if (next)
		{
			factor = lerp_i(factor, SwallowFactor(c + 1), num, den);
			pnum = num;
		}
		// the world matrix: from the one drawn to the one the frame holds
		uint8_t mat[0x20];
		memcpy(mat, (const void *)(e + 0x40), sizeof(mat));
		const uint8_t flags = U8(e, 0);
		if (memcmp(mat, r.mat, sizeof(mat)))
		{
			Mat4x3 &m = *(Mat4x3 *)(e + 0x40);
			const Mat4x3 &a = *(const Mat4x3 *)r.mat, &b = *(const Mat4x3 *)mat;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++) m.m[i][j] = lerp16(a.m[i][j], b.m[i][j], num, den);
				m.t[i] = lerp_i(a.t[i], b.t[i], num, den);
			}
		}
		pose_midpoint((void *)(e + 0x60), (void *)(e + 0x6C), pnum, den);
		const uint32_t second = U32(e, 0x78);
		if (second && !(U8(e, 1) & 2)) pose_midpoint((void *)second, (void *)(second + 0xC), pnum, den);
		SwallowDraw(n, factor);
		U8(e, 0) = flags;
		memcpy((void *)(e + 0x40), mat, sizeof(mat));
	}

	// Debris: a = the pool as drawn, b = the live pool (the next tick's draw)
	static void DebrisHeld(const Rec &r, int num, int den)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		const bool end = num >= den;
		uint8_t buf[0x18];
		const uint32_t d = (uint32_t)buf;
		for (int i = 0; i < PART_COUNT; i++)
		{
			const uint32_t a = (uint32_t)(g_pools[r.pool] + 0x18 * i), b = PARTS + 0x18 * (uint32_t)i;
			const bool la = (U8(a, 0) & 2) != 0, lb = (U8(b, 0) & 2) != 0;
			const bool same = !r.draw_only && la && lb && S16(b, 4) == (int16_t)(S16(a, 4) + 1);
			if (r.draw_only)
			{
				if (la) DebrisDraw(h, a);
				continue;
			}
			if (same)
			{
				memcpy(buf, end ? (const void *)b : (const void *)a, sizeof(buf));
				for (int k = 0; k < 3; k++) S16(d, 8 + 2 * k) = lerp16(S16(a, 8 + 2 * k), S16(b, 8 + 2 * k), num, den);
				S16(d, 6) = lerp16(S16(a, 6), S16(b, 6), num, den);
				DebrisDraw(h, d);
				continue;
			}
			// freed by the tick: holds below 1; spawned by the tick: from the next tick on
			if (la && !end) DebrisDraw(h, a);
			if (lb && end) DebrisDraw(h, b);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		if (g_nrec == 0) return;
		if (PacketCursor() - g_packets > sizeof(g_held_packets)) return; // larger than the private buffer (never seen)
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_Flicker: FlickerHeld(r, num, den); break;
			case ORIG_Swallow: SwallowHeld(r, num, den); break;
			case ORIG_Debris: if (r.pool >= 0) DebrisHeld(r, num, den); break;
			default: ModelHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag306_held()
	{
		mark_held(degen306::ORIG_Core);
		mark_held(degen306::ORIG_Pulse);
		mark_held(degen306::ORIG_Flicker);
		mark_held(degen306::ORIG_ScreenRing);
		mark_held(degen306::ORIG_TargetRing);
		mark_held(degen306::ORIG_Vortex);
		mark_held(degen306::ORIG_Swallow);
		mark_held(degen306::ORIG_Debris);
		register_module_held(306, degen306::HeldReady, degen306::HeldFrame);
	}
}
