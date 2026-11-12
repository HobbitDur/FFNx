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

// 30 fps part of mag322_zan.cpp (Zan, effect 322). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws
// the state num/den of the way from the last real tick to the next one (Boomerang Sword's held part, 304).
//
// The main task updates THEN draws; trails, the hit, streaks and dust draw THEN update. A real tick records its draws
// in order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in the same
// order on private packets, with the current camera (the frame gate's):
//   Blade   - the main task's own state at counter + num/den: the path parameter of that fractional counter (the
//             tick's formula with the counter num/den further, squared on segment 0 as the tick squares it; at 1 the
//             next tick's own parameter), the point on the path there, its last point of a different x / z as the
//             task's update keeps it, the spin the short way between this tick's and the next tick's; drawn by the
//             tick's code.
//   Trails  - their frame, fade angle (counter + num/den) x 0x80.
//   Hit     - prim::play_held at the node's position (every object's exact in-between record).
//   Streaks - their position num/den along the velocity they move by (the one before the drag), turned by the tick's
//             code at that position; flipbook frame N held.
//   Dust    - still (flipbook frame N held).
// A part the next tick no longer draws (the main task's last counter, a trail's 8th draw, a streak's 8th, a dust
// sprite's 12th, the ended hit layout, every part when the master ends) holds its picture (state N). Spawns (CRT
// rand) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates per tick: the held packets
// start where the tick's first draw started; the blend buffer (magic buffer + 0x1CCF0) lies 0xE004 bytes into arena
// B (+0xECEC): on arena-B ticks the held blend buffer sits at that offset of the held packets, as in the game. The
// GTE, the Field_Alloc scratch, the word 0x1D9770C and the engine words the sprite draws touch are put back; no node,
// module global or entity word is written (the caster's visible-object bit is the real tick's).

namespace ff8fx
{
namespace zan322
{
	enum DrawKind { K_BLADE = 1, K_TRAIL = 2, K_HIT = 3, K_STREAK = 4, K_DUST = 5 };
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;            // module packet arena per tick
	static const uint32_t MORPH_IN_B = MORPH - ARENA_B; // blend buffer offset in arena B

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		int16_t counter;          // the counter as drawn (streaks / dust: the frame)
		int16_t pos[3];           // streaks: the position as drawn
		int16_t vel[3];           // streaks: the velocity they move by after the draw
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_end_tick = 0xFFFFFFFF;  // the master ended on this tick (every queue is dropped)
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 8 + 1 + 0x20 + 0x20];
	static uint8_t g_held_arena[ARENA];     // packets (arena B layout: the blend buffer at MORPH_IN_B)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-A ticks

	static void held_note_master()
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

	static Rec *NewRec(int kind, TaskNode *n, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = counter;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_blade(TaskNode *n) { NewRec(K_BLADE, n, ((const MainNode *)n)->counter); }
	static void held_note_trail(TaskNode *n) { NewRec(K_TRAIL, n, ((const TrailNode *)n)->counter); }
	static void held_note_hit(TaskNode *n) { NewRec(K_HIT, n, 0); }
	static void held_note_dust(TaskNode *n) { NewRec(K_DUST, n, ((const DustNode *)n)->counter); }

	static void held_note_streak(TaskNode *n)
	{
		const StreakNode *s = (const StreakNode *)n;
		Rec *r = NewRec(K_STREAK, n, s->counter);
		if (!r) return;
		memcpy(r->pos, s->pos, sizeof(r->pos));
		memcpy(r->vel, s->vel, sizeof(r->vel));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		if (g_end_tick == g_real_tick) return false;
		switch (r.kind)
		{
		case K_BLADE: return r.counter + 1 < 0x39;
		case K_TRAIL: return r.counter + 1 < 8;
		case K_STREAK: return r.counter + 1 < 8;
		case K_DUST: return r.counter + 1 < 0xC;
		default: return (r.node->flags & 1) != 0;
		}
	}

	// the main task's path parameter at counter c + num/den (num < den; num 0 = the task's own formula)
	static void HeldParam(int32_t c, int num, int den, int32_t &seg, int32_t &t)
	{
		const int32_t k = c - 0xE;
		seg = 0;
		t = 0;
		if ((uint32_t)k < 0x14)
		{
			const int32_t u = (int32_t)((uint32_t)((k * den + num) << 12) / (uint32_t)(20 * den)) + 0x66;
			t = u * u >> 12;
		}
		else if ((uint32_t)(c - 0x22) < 4)
		{
			const int32_t w = (int32_t)((uint32_t)(((c - 0x22) * den + num) << 11) / (uint32_t)den) + 0x200;
			seg = (w >> 12) + 1;
			t = w & 0xFFF;
		}
		else if ((uint32_t)(c - 0x26) < 0x14)
		{
			t = (int32_t)((uint32_t)(((c - 0x26) * den + num) << 12) / (uint32_t)(20 * den)) + 0x66;
			seg = 3;
		}
	}

	// the blade at counter + k/den
	static void HeldBlade(const Rec &r, int k, int den)
	{
		const MainNode *m = (const MainNode *)r.node;
		int32_t c = r.counter, num = k;
		if (num == den)
		{
			c++;
			num = 0;
		}
		int32_t seg, t;
		HeldParam(c, num, den, seg, t);
		int16_t cur[4] = {};
		PathPoint(m, seg, t, cur);
		const int16_t *prev = (m->cur[0] != cur[0] || m->cur[2] != cur[2]) ? m->cur : m->prev;
		const int16_t spin = lerp_angle(m->spin, (int16_t)(m->spin + 0x400), k, den);
		int32_t dir[3] = { (int32_t)prev[0] - cur[0], 0, (int32_t)cur[2] - prev[2] };
		Mat4x3 frame;
		BladeDraw(cur, dir, spin, &frame);
	}

	static void HeldDraw(int num, int den)
	{
		// the blend buffer (magic buffer + 0x1CCF0) lies 0xE004 bytes into arena B (+0xECEC)
		const bool arena_b = g_packets == TexBase() + ARENA_B;
		const uint32_t morph = arena_b ? (uint32_t)(g_held_arena + MORPH_IN_B) : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
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
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == K_BLADE) HeldBlade(r, k, den);
			else if (r.kind == K_TRAIL) TrailDraw((const TrailNode *)r.node, (r.counter * den + k) * 0x80 / den);
			else if (r.kind == K_HIT)
			{
				const HitNode *h = (const HitNode *)r.node;
				PrimArg arg;
				memcpy(arg.pos, h->pos, 8);
				arg.depth = -0x80;
				arg.morph = morph;
				prim::play_held((prim::Layout *)h->layout, PrimCallback, (int)&arg, k, den);
			}
			else if (r.kind == K_STREAK)
			{
				const StreakNode *s = (const StreakNode *)r.node;
				int16_t pos[4];
				for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
				pos[3] = r.counter; // (the node's frame word follows its position: the shadow camera's V0 pad)
				StreakDraw(pos, s->v0, r.counter);
			}
			else
			{
				const DustNode *d = (const DustNode *)r.node;
				const int16_t pos[4] = { d->pos[0], d->pos[1], d->pos[2], r.counter }; // (frame word: the V0 pad)
				DustDraw(pos, d->height, r.counter);
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag322_held()
	{
		mark_held(zan322::ORIG_MainTask);
		mark_held(zan322::ORIG_TrailTask);
		mark_held(zan322::ORIG_HitTask);
		mark_held(zan322::ORIG_StreakTask);
		mark_held(zan322::ORIG_DustTask);
		register_module_held(322, zan322::HeldReady, zan322::HeldDraw);
	}
}
