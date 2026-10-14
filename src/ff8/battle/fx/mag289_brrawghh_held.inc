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

// 30 fps part of mag289_brrawghh.cpp ("Brrawghh!", effect 289). Exact vanilla shapes: the logic stays
// at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the main counter after its draws, a layout in its play, a sprite
// after its draw). A real tick records its draws in order with the GTE state and the Field_Alloc
// scratch each one started from (the tube header's flag byte +0x1C is never written: scratch); a held
// frame replays them in the same order on private packets, with the current camera (the frame
// gate's), each layout through prim::play_held (every object's exact in-between record):
//   Tubes (3 per tick, counters 0..0x15) - every TubeDraw argument (angle, wave amplitudes, fade
//                  offset, U scroll, x / y scale) half way to the next tick's; the moved target point
//                  P half way to the next tick's; the caster's bone 9 point B at the caster's pose
//                  midpoint (pose_midpoint, the bones rebuilt from the pose afterwards); the basis of
//                  P - B scaled by its length at B, composed with the camera.
//   Target layout (counters 0xA..0x1D) - the target point (fixed).
//   Splash layouts - their point (fixed).
//   Sprites      - position half way to the next tick's (+ velocity); flipbook frame N held.
// A part the next tick no longer draws (its counter range, its layout or its task ended on the tick;
// every part on the tick the master ends: the main and splash queues empty) holds its picture (state
// N: records at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The
// module packet arena alternates per tick: the held packets start where the tick's first draw
// started; the vertex / blend buffer (magic buffer + 0x1520) lies 0x520 bytes before arena A
// (+0x1A40): on arena-A ticks the held buffer sits 0x520 bytes before the held packets, as in the
// game. The GTE, the Field_Alloc scratch, the engine words the sprite draws touch and the caster's
// bone matrices are put back.

namespace ff8fx
{
namespace brr289
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = ARENA_B - ARENA_A;   // module packet arena per tick
	static const uint32_t VERTS_GAP = ARENA_A - VERTS; // vertex / blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // main: the counter as drawn; sprites: the frame
		int16_t k;                // tube: which of the three draws
		int16_t B[4];             // tube: the bone point as drawn
		int16_t P[4];             // tube: the moved target point as drawn; sprites: the position as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 1 + 0x20 + 0x40];
	static uint8_t g_held_arena[VERTS_GAP + ARENA]; // [vertex / blend buffer][packets] (arena A layout)
	static uint8_t g_held_verts[0x4000];    // vertex / blend buffer on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = nullptr;
		r.counter = counter;
		r.k = 0;
		memset(r.B, 0, sizeof(r.B));
		memset(r.P, 0, sizeof(r.P));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a tube is about to be drawn: the points it is drawn from
	static void held_note_tube(TaskNode *n, int k, const int16_t *B, const int16_t *P)
	{
		Rec *r = NewRec(TUBE, n, ((const MainNode *)n)->counter);
		if (!r) return;
		r->k = (int16_t)k;
		memcpy(r->B, B, 6);
		memcpy(r->P, P, 6);
	}

	// a layout is about to play (the target layout of the main task / a splash task's own)
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		Rec *r = NewRec(kind, n, kind == TARGET ? ((const MainNode *)n)->counter : 0);
		if (r) r->layout = l;
	}

	static void held_note_sprite(TaskNode *n)
	{
		const SpriteNode *s = (const SpriteNode *)n;
		Rec *r = NewRec(SPRITE, n, s->frame);
		if (r) memcpy(r->P, s->pos, 6);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static bool Alive(const Rec &r) { return (r.node->flags & 1) != 0; }
	// the layout has frames left after this tick's play (it draws again on the next tick)
	static bool LayoutLive(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}
	static bool TubesDraw(int32_t c) { return c < 0x16; }
	static bool TargetDraws(int32_t c) { return (uint32_t)(c - 0xA) < 0x14; }

	// the caster's bone point at pose k/den (the bones rebuilt from the pose afterwards)
	static void PointAt(int k, int den, int16_t *B)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		BonePoint(POINT, B);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldDraw(int num, int den)
	{
		// the master ended on this tick (the main and splash queues are empty): no task draws again, every part holds
		if (!QMain()->head && !QSplashes()->head) num = 0;
		// arena A (magic buffer + 0x1A40) follows the vertex / blend buffer (+ 0x1520) at 0x520 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t verts = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_verts;
		HeldScope scope(PacketCursor(), g_held_arena + VERTS_GAP, ARENA, g_packets);
		// GTE data registers 28..31 (square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		bool have_b = false;
		int16_t Bk[4] = {};
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			switch (r.kind)
			{
			case SPRITE:
			{
				const SpriteNode *s = (const SpriteNode *)r.node;
				int16_t pos[3] = { r.P[0], r.P[1], r.P[2] };
				if (num && Alive(r))
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.P[j], s->pos[j], num, den);
				Replay(r);
				SpriteDraw(s->seq, pos, s->scale, r.counter);
				continue;
			}
			case TUBE:
			{
				const MainNode *w = (const MainNode *)r.node;
				const int32_t c = r.counter;
				const int k = num && TubesDraw(c + 1) ? num : 0;
				int16_t B[4] = { r.B[0], r.B[1], r.B[2], 0 }, P[4] = { r.P[0], r.P[1], r.P[2], 0 };
				int32_t x[6];
				TubeArgs(c, r.k, x);
				if (k)
				{
					if (!have_b)
					{
						PointAt(k, den, Bk);
						have_b = true;
					}
					memcpy(B, Bk, 6);
					int16_t P1[4] = {};
					TubeTarget(w, c + 1, P1);
					for (int j = 0; j < 3; j++) P[j] = lerp16(r.P[j], P1[j], k, den);
					int32_t x1[6];
					TubeArgs(c + 1, r.k, x1);
					for (int j = 0; j < 6; j++) x[j] = lerp_i(x[j], x1[j], k, den);
				}
				Mat4x3 M = {}, CM = {};
				TubeBasis(P, B, &M);
				ComposeAffineTransform(&Camera(), &M, &CM);
				Replay(r);
				TubeDraw(&CM, TUBE_MESH[r.k], x[0], x[1], x[2], x[3], x[4], x[5], verts);
				continue;
			}
			case TARGET:
			case SPLASH:
			{
				int k = num;
				if (r.kind == TARGET) { if (!TargetDraws(r.counter + 1) || !LayoutLive(r.layout)) k = 0; }
				else if (!Alive(r) || !LayoutLive(r.layout)) k = 0;
				PrimArg arg = {};
				BlockAt(&arg, r.kind == TARGET ? ((const MainNode *)r.node)->pos : ((const SplashNode *)r.node)->pos);
				arg.morph = verts;
				arg.depth = -0x80;
				Replay(r);
				prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
				continue;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag289_held()
	{
		mark_held(brr289::ORIG_MainTask);
		mark_held(brr289::ORIG_SpriteTask);
		mark_held(brr289::ORIG_SplashTask);
		register_module_held(289, brr289::HeldReady, brr289::HeldDraw);
	}
}
