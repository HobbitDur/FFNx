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

// 30 fps part of mag217_energy_bomber.cpp (Energy Bomber, effect 217). Exact vanilla shapes: the logic stays at 15 Hz;
// a held frame redraws the real tick's draws (logged in order with the GTE registers and the Field_Alloc scratch they
// started from) at num / den of the way to the next tick's draw:
//   ball     (updated THEN drawn) prim::play_held at the ball point half way to the next tick's (the node stepped on a
//            copy as that tick does: the caster's bone 0x27 on her next pose (pose_midpoint at 1, her real bone matrices
//            rebuilt after), the throw, the hops). The next point of counter 0x37 is a random hop end (CRT rand, made by
//            that tick): the ball holds below 1 and is not drawn at 1. The last ball draw (0x42) likewise.
//   trail    each point half way to the point the next tick draws at that index (the node's trail after this tick's
//            push); the fade is the index's; a point the next tick adds appears on that tick.
//   burst    prim::play_held at the target (fixed); the last draw holds below 1, none at 1.
//   sparks   (draw THEN update) position half way along the next step (velocity + drag), flipbook frame N (N + 1's at
//            1); a spark whose task ends with this tick (or dropped by the root's end) holds below 1, none at 1.
// The screen flash (engine state, set by the main task every tick) stays at 15 Hz. Spawns, the sound and the action
// result stay on the real ticks. No task has a draw-only mode. The camera is the engine's camera animation. The module
// packet arena alternates per tick: the held packets start where the tick's first draw started.

namespace ff8fx
{
namespace bomber217
{
	enum DrawKind { K_BALL = 1, K_TRAIL = 2, K_BURST = 3, K_SPARK = 4 };
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x12000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		MainNode *main;           // ball / trail / burst: the main node
		int16_t counter;          // ball / trail / burst: the main counter as drawn
		int16_t P[4];             // ball: its point as drawn
		int16_t index, count;     // trail: the points drawn
		int16_t trail[2][4];      // trail: as drawn
		SparkNode spark;          // spark: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_end_tick = 0xFFFFFFFF;  // the root ended on this tick (its spark queue is dropped)
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 0x40];
	static MainNode g_next;                 // the main node stepped to the next tick (ball point)
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_end()
	{
		g_end_tick = g_real_tick;
	}

	static Rec *NewRec(int kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_ball(MainNode *m, const int16_t *P)
	{
		Rec *r = NewRec(K_BALL);
		if (!r) return;
		r->main = m;
		r->counter = m->counter;
		memcpy(r->P, P, sizeof(r->P));
	}

	static void held_note_trail(MainNode *m)
	{
		Rec *r = NewRec(K_TRAIL);
		if (!r) return;
		r->main = m;
		r->counter = m->counter;
		r->index = m->index;
		r->count = m->count;
		memcpy(r->trail, m->trail, sizeof(r->trail));
	}

	static void held_note_burst(MainNode *m)
	{
		Rec *r = NewRec(K_BURST);
		if (!r) return;
		r->main = m;
		r->counter = m->counter;
	}

	static void held_note_spark(TaskNode *n)
	{
		Rec *r = NewRec(K_SPARK);
		if (r) r->spark = *(const SparkNode *)n;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the ball point the next tick draws (false: not drawn, or a random hop end)
	static bool BallNext(const Rec &r, int16_t *out)
	{
		const int32_t c = r.counter + 1;
		if (c >= 0x43) return false;
		memcpy(&g_next, r.main, sizeof(g_next));
		if (c < 0x2F)
		{
			// the caster's bone 0x27 on her next pose; her real bone matrices back after
			uint8_t *caster = Caster();
			pose_midpoint(caster + 0x60, caster + 0x6C, 1, 1);
			const bool ok = BallPoint(&g_next, c, out, false);
			BuildBoneMatricesFromPose(caster + 0x60);
			return ok;
		}
		return BallPoint(&g_next, c, out, false);
	}

	static void HeldBall(const Rec &r, uint32_t morph, int num, int den)
	{
		int16_t next[4];
		const bool nd = BallNext(r, next);
		if (!nd && num == den) return;
		const int k = nd ? num : 0; // the last picture holds
		layouts::AnchorArg arg;
		for (int j = 0; j < 3; j++) arg.anchor[j] = k ? lerp16(r.P[j], next[j], k, den) : r.P[j];
		arg.anchor[3] = r.P[3];
		arg.depth = -0x80;
		arg.morph = morph;
		arg.honoured = layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS;
		arg.cursor = &PacketCursor();
		prim::play_held((prim::Layout *)r.main->layout_ball, layouts::AnchorPrimCallback, (int)&arg, k, den);
	}

	static void HeldTrail(const Rec &r, int num, int den)
	{
		const bool nd = r.counter + 1 < 0x43;
		if (!nd && num == den) return;
		const int k = nd ? num : 0; // the last picture holds
		uint8_t *h = (uint8_t *)FieldAlloc(0x78);
		for (int32_t i = r.index; i < r.count; i++)
		{
			int16_t p[4];
			memcpy(p, r.trail[i], sizeof(p));
			if (k && i < r.main->count)
				for (int j = 0; j < 3; j++) p[j] = lerp16(r.trail[i][j], r.main->trail[i][j], k, den);
			TrailDraw(h, p, i);
		}
		FieldFree(0x78);
	}

	static void HeldBurst(const Rec &r, uint32_t morph, int num, int den)
	{
		const bool nd = r.counter + 1 <= 0x55;
		if (!nd && num == den) return;
		const int k = nd ? num : 0; // the last picture holds
		layouts::AnchorArg arg;
		memcpy(arg.anchor, r.main->target, 8);
		arg.depth = -0x80;
		arg.morph = morph;
		arg.honoured = layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS;
		arg.cursor = &PacketCursor();
		prim::play_held((prim::Layout *)r.main->layout_burst, layouts::AnchorPrimCallback, (int)&arg, k, den);
	}

	static void HeldSpark(const Rec &r, int num, int den)
	{
		SparkNode s = r.spark;
		const bool nd = s.counter + 1 < 0xC && g_end_tick != g_real_tick;
		if (!nd && num == den) return;
		if (nd)
		{
			for (int j = 0; j < 3; j++)
			{
				const int16_t v = (int16_t)(s.vel[j] + s.drag[j]);
				s.pos[j] = lerp16(s.pos[j], (int16_t)(s.pos[j] + v), num, den);
			}
			if (num == den) s.counter++;
		}
		SparkDraw(s.pos, s.counter);
	}

	static void HeldDraw(int num, int den)
	{
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == K_BALL) HeldBall(r, morph, num, den);
			else if (r.kind == K_TRAIL) HeldTrail(r, num, den);
			else if (r.kind == K_BURST) HeldBurst(r, morph, num, den);
			else HeldSpark(r, num, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag217_held()
	{
		mark_held(bomber217::ORIG_MainTask);
		mark_held(bomber217::ORIG_SparkTask);
		register_module_held(217, bomber217::HeldReady, bomber217::HeldDraw);
	}
}
