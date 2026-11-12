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

// 30 fps part of mag320_dark_flare.cpp (Dark Flare, effect 320). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the main counter after its draws, a spark after its
// draw). A real tick records its layout plays and spark draws in order with the GTE state and the Field_Alloc scratch
// each one started from; a held frame replays them in the same order on private packets, with the current camera (the
// frame gate's), each layout through prim::play_held (every object's exact in-between record):
//   Layout 1, counters 0..0x23 - S = the caster's point at the caster's pose midpoint (pose_midpoint, the bones rebuilt
//                from the real pose afterwards), the basis of T - S at S (at 0x23 the next tick keeps tick N's frame:
//                held);
//   Layout 1, counters 0x24..0x2E - the point L half way between this tick's and the next tick's (the weight of T steps
//                by 1/10 a tick; M fixed);
//   Layout 2   - its frame is fixed (M at S);
//   Layout 3   - its frame is fixed (T); the texture v scroll (16 a tick) half way;
//   Tiles      - the grey tiles of counters 0x24 / 0x2E..0x31: a tile followed by a plain tile on the next tick fades its
//                grey half way; else tick N's tile holds below fraction 1; at 1 the next tick's tile;
//   Sparks     - their position half way along the velocity they move by (the one before the fall); flipbook frame N held
//                (at fraction 1 the next tick's frame).
// A part the next tick no longer draws (its counter range ends) holds its picture below fraction 1 and is gone at 1 (the
// next tick); an ended spark holds its last frame. Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The
// module packet arena alternates per tick: the held packets start where the tick's draws started; the blend buffer
// (magic buffer + 0xF54) lies 0x3C0 bytes before arena A (+0x1314): on arena-A ticks the held blend buffer sits 0x3C0
// bytes before the held packets, as in the game. The GTE, the Field_Alloc scratch, the caster's bone matrices and the
// engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace flare320
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		int16_t counter;          // main: the counter as drawn; sparks: the frame
		int16_t S[3];             // layout 1 (counters 0..0x23): the caster's point as drawn
		int16_t pos[3];           // sparks: the position, scale and velocity as drawn
		int16_t scale;
		int16_t vel[3];
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's start (the arena)
	static Rec g_recs[4 + 0x40];
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
	}

	static Rec *NewRec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		Rec &r = g_recs[g_nrec++];
		memset(&r, 0, offsetof(Rec, gte));
		r.kind = kind;
		r.node = n;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_main(int kind, TaskNode *n)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		const MainNode *m = (const MainNode *)n;
		r->counter = m->counter;
		memcpy(r->S, m->S, sizeof(r->S));
	}

	static void held_note_spark(TaskNode *n)
	{
		Rec *r = NewRec(SPARK, n);
		if (!r) return;
		const SparkNode *s = (const SparkNode *)n;
		r->counter = s->counter;
		memcpy(r->pos, s->pos, sizeof(r->pos));
		r->scale = s->scale;
		memcpy(r->vel, s->vel, sizeof(r->vel));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again (the main task ends after counter 0x61)
	static bool NextDraws(const Rec &r)
	{
		const int32_t c1 = r.counter + 1;
		switch (r.kind)
		{
		case PLAY_1: return c1 <= 0x24; // at 0x24 the same layout goes on (counters 0x24..0x2E)
		case PLAY_1L: return c1 <= 0x2E;
		case PLAY_2: return c1 < 0x34;
		case PLAY_3: return c1 < 0x62;
		default: return (r.node->flags & 1) != 0;
		}
	}

	// the main's tiles: before layout 1 (counter 0x24) / before layout 3 (counters 0x2E..0x31); 0 none, 1 plain, 2 between
	// the draw modes
	static int TileBefore1(int32_t c, uint32_t *rgb)
	{
		*rgb = 0x808080;
		return c == 0x24 ? 1 : 0;
	}
	static int TileBefore3(int32_t c, uint32_t *rgb)
	{
		switch (c - 0x2D)
		{
		case 1: *rgb = 0x808080; return 2;
		case 2: *rgb = 0xF0F0F0; return 1;
		case 3: *rgb = 0xA0A0A0; return 1;
		case 4: *rgb = 0x505050; return 1;
		}
		return 0;
	}

	// a tile at k/den from counter c: plain -> plain fades half way, else tick N's below 1; at 1 the next tick's
	static void HeldTile(int (*tile)(int32_t, uint32_t *), int32_t c, int num, int den)
	{
		uint32_t a, b;
		const int ka = tile(c, &a), kb = tile(c + 1, &b);
		if (num >= den)
		{
			if (kb == 1) Tile(b);
			else if (kb == 2) TileModes(b);
			return;
		}
		if (ka == 0) return;
		if (ka == 2)
		{
			TileModes(a);
			return;
		}
		if (kb == 1 && num > 0)
		{
			uint32_t rgb = 0;
			for (int j = 0; j < 24; j += 8) rgb |= (uint32_t)lerp_i((a >> j) & 0xFF, (b >> j) & 0xFF, num, den) << j;
			a = rgb;
		}
		Tile(a);
	}

	// layout 1's frame (counters 0..0x23) at k/den: S through the caster's bone at the caster's pose midpoint (the bones
	// rebuilt from the pose afterwards), the basis of T - S at S
	static void HeldFrame1(MainNode *m, const Rec &r, int k, int den, Mat4x3 *M)
	{
		if (k == 0 || r.counter == 0x23)
		{
			// tick N's frame (at 0x23 the next tick keeps it)
			*M = m->M;
			M->t[0] = r.S[0];
			M->t[1] = r.S[1];
			M->t[2] = r.S[2];
			return;
		}
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		int16_t S[4] = {};
		CasterBonePoint(POINT_S, S);
		BuildBoneMatricesFromPose(c + 0x60);
		MainFrame(m, S, M);
	}

	// layout 1's point L (counters 0x24..0x2E) at k/den: half way between this tick's and the next tick's
	static void HeldPoint1(MainNode *m, int32_t c, int k, int den, int16_t *L)
	{
		int16_t L0[4] = {}, L1[4] = {};
		VecLerp(m->T, m->S, LerpWeight(c), 0x1000 - LerpWeight(c), L0);
		if (k == 0)
		{
			memcpy(L, L0, 6);
			return;
		}
		VecLerp(m->T, m->S, LerpWeight(c + 1), 0x1000 - LerpWeight(c + 1), L1);
		for (int j = 0; j < 3; j++) L[j] = lerp16(L0[j], L1[j], k, den);
	}

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0x1314) follows the blend buffer (+ 0xF54) at 0x3C0 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		// GTE data registers 28..31 (the basis' square root sets LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const bool next = NextDraws(r);
			if (!next && num >= den && r.kind != SPARK) continue; // fraction 1 = the next tick: a part it no longer draws is gone (an ended spark holds its last frame)
			const int k = next ? num : 0; // the last picture holds
			PrimArg arg = {}; // (the matrix pad +0x12 is never read)
			MainNode *m = (MainNode *)r.node;
			int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
			prim::Layout *layout = nullptr;
			if (r.kind == PLAY_1)
			{
				if (num >= den && r.counter + 1 == 0x24) Tile(0x808080); // the next tick's first draw
				Mat4x3 M;
				HeldFrame1(m, r, k, den, &M);
				ComposeAffineTransform(&Camera(), &M, &arg.m);
				SetArg(&arg, KINDS_1, DRAWS_1, -0x10);
				layout = (prim::Layout *)m->layout_1;
			}
			else if (r.kind == PLAY_1L)
			{
				HeldTile(TileBefore1, r.counter, num, den);
				int16_t L[4] = {};
				HeldPoint1(m, r.counter, k, den, L);
				Mat4x3 M = m->M;
				M.t[0] = L[0];
				M.t[1] = L[1];
				M.t[2] = L[2];
				ComposeAffineTransform(&Camera(), &M, &arg.m);
				SetArg(&arg, KINDS_1, DRAWS_1, -0x10);
				layout = (prim::Layout *)m->layout_1;
			}
			else if (r.kind == PLAY_2)
			{
				Mat4x3 M = m->M;
				M.t[0] = m->S[0];
				M.t[1] = m->S[1];
				M.t[2] = m->S[2];
				ComposeAffineTransform(&Camera(), &M, &arg.m);
				SetArg(&arg, KINDS_2, DRAWS_2, -0x10);
				layout = (prim::Layout *)m->layout_2;
			}
			else if (r.kind == PLAY_3)
			{
				HeldTile(TileBefore3, r.counter, num, den);
				RotY(0, &arg.m);
				arg.m.t[0] = m->T[0];
				arg.m.t[1] = m->T[1];
				arg.m.t[2] = m->T[2];
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				SetArg(&arg, KINDS_3, DRAWS_3, -0x200);
				arg.scroll = (int16_t)(((r.counter - 0x2D) << 4) + 0x10 * k / den);
				layout = (prim::Layout *)m->layout_3;
			}
			else
			{
				for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
			}
			arg.morph = morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == SPARK) SparkDraw(pos, r.scale, (int16_t)(r.counter + (next && num >= den ? 1 : 0))); // at 1 the next tick's frame
			else prim::play_held(layout, PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag320_held()
	{
		mark_held(flare320::ORIG_MainTask);
		mark_held(flare320::ORIG_SparkTask);
		register_module_held(320, flare320::HeldReady, flare320::HeldDraw);
	}
}
