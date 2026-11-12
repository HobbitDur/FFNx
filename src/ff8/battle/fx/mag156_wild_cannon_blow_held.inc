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

// 30 fps part of mag156_wild_cannon_blow.cpp (Wild Cannon Blow, effect 156). Exact vanilla shapes: the logic stays at 15 Hz, a held frame
// draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the particle tasks spawn their new particles AFTER their draw). A real tick records its draws in
// order (the root's seam starts the log) with the GTE registers and the Field_Alloc scratch each one started from; a held frame
// replays them in the same order with the vanilla draw code, each at its in-between state, under the current camera:
//   Flash / impact ring - scale num/den of the way to scale + step; fade half way where both ticks fade (the switch to faded on
//             the real frame).
//   Ring      - fade half way to the next counter's.
//   Impact    - angle to angle + spin, scale to scale + step, fade as above.
//   Ball      - num/den of the way along the curve from its point to the next tick's (between curve points: the straight line
//             between the two neighbours); its flipbook frame (counter & 3) held.
//   Particles - burst: their point half way to point + velocity (through the shot's rotation); the trails: their size half way
//             to size + size >> 5; the emitters': fixed; flipbook frames held; a particle whose sequence ends holds below 1; the
//             new ones (CRT rand) appear on the real frame.
// A part the next tick no longer runs holds its picture (state N); a draw-only (paused) tick holds everything (a node whose
// counter did not move holds). No task draws
// differently on a draw-only tick (the master and the shots do nothing on them). Not drawn by the module (held by the engine at 15 Hz): the camera shake word 0x1D97712
// (a per-tick +-0x40 jitter). The module packet arena alternates per tick: the held packets start where the tick's first draw
// started. The GTE, the Field_Alloc scratch, the shadow-rotation scratch and the engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace wildcannon156
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xC000;   // one module packet arena (texture base / + 0xC000)
	static const int MAX_RECS = 0x80, MAX_PARTS = 0x400;

	struct Rec
	{
		int kind;
		uint32_t node;
		uint8_t copy[0x24];       // the node as drawn
		int first, count;         // particle tasks: their particles' records
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PRec { uint32_t addr; uint8_t copy[0x18]; };

	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0;
	static Rec *g_cur = nullptr;            // the particle task being logged
	static bool g_overflow = false;      // a record or particle past the caps: the tick is not replayed
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static PRec g_parts[MAX_PARTS];
	static uint8_t g_held_packets[ARENA];

	static void held_note_root()
	{
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_overflow = false;
		g_cur = nullptr;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, uint32_t n)
	{
		g_cur = nullptr;
		if (g_log_tick != g_real_tick) return nullptr;
		if (g_nrec >= MAX_RECS)
		{
			g_overflow = true;
			return nullptr;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.copy, (const void *)n, sizeof(r.copy));
		r.first = g_npart;
		r.count = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_node(int kind, uint32_t n) { NewRec(kind, n); }

	static void held_note_parts(int kind, uint32_t n) { g_cur = NewRec(kind, n); }

	static void held_note_particle(uint32_t p)
	{
		if (!g_cur) return;
		if (g_npart >= MAX_PARTS)
		{
			g_overflow = true;
			return;
		}
		PRec &q = g_parts[g_npart++];
		q.addr = p;
		memcpy(q.copy, (const void *)p, sizeof(q.copy));
		g_cur->count++;
	}

	static bool HeldReady() { return g_log_tick == g_real_tick && !g_overflow; }

	// a node still running after the tick: its task did not end and the shot queue still holds it
	static bool Alive(uint32_t n, uint32_t orig)
	{
		const TaskNode *t = (const TaskNode *)n;
		if (!(t->flags & 1) || (uint32_t)t->func != orig) return false;
		int guard = 0;
		for (const TaskNode *x = ShotQueue()->head; x && guard < 0x100; x = x->next, guard++)
			if (x == t) return true;
		return false;
	}

	// a particle the tick drew that lives on (same owner, its frame one on: not a new one in a freed slot)
	static bool PartAlive(const PRec &q)
	{
		return U32(q.addr, P_MASK) == *(const uint32_t *)(q.copy + P_MASK) && S16(q.addr, P_FRAME) == (int16_t)(*(const int16_t *)(q.copy + P_FRAME) + 1);
	}

	static int32_t lerp_fade(int32_t a, int32_t b, int num, int den) { return a + (int32_t)((int64_t)(b - a) * num / den); }

	// fade of a counter ramp: half way when both ticks fade
	static int32_t HeldFade(int16_t c, int k, int den, int16_t from, int32_t (*ramp)(int16_t))
	{
		if (c < from) return -1;
		return k ? lerp_fade(ramp(c), ramp((int16_t)(c + 1)), k, den) : ramp(c);
	}
	static int32_t RampFlash(int16_t c) { return shl32(c - 4, 9); }
	static int32_t RampRing(int16_t c) { return shl32(c, 10); }
	static int32_t RampIRing(int16_t c) { return mul32(c - 3, 0x199); }
	static int32_t RampImpact(int16_t c) { return shl32(c - 8, 9); }

	static void NodeHeld(const Rec &r, int num, int den)
	{
		uint8_t cp[0x24];
		memcpy(cp, r.copy, sizeof(cp));
		const uint32_t c = (uint32_t)cp, n = r.node;
		const int16_t cnt = S16(c, 0xC);
		switch (r.kind)
		{
		case REC_FLASH:
		{
			const int k = Alive(n, ORIG_Flash) && S16(n, 0xC) != cnt ? num : 0;
			if (k) S16(c, 0x1C) = lerp16(S16(c, 0x1C), S16(n, 0x1C), k, den);
			FlashDraw(c, HeldFade(cnt, k, den, 4, RampFlash));
			break;
		}
		case REC_RING:
		{
			const int k = Alive(n, ORIG_Ring) && S16(n, 0xC) != cnt ? num : 0;
			RingDraw(c, HeldFade(cnt, k, den, 0, RampRing));
			break;
		}
		case REC_IRING:
		{
			const int k = Alive(n, ORIG_ImpactRing) && S16(n, 0xC) != cnt ? num : 0;
			if (k) S16(c, 0x1C) = lerp16(S16(c, 0x1C), S16(n, 0x1C), k, den);
			ImpactRingDraw(c, HeldFade(cnt, k, den, 3, RampIRing));
			break;
		}
		case REC_IMPACT:
		{
			const int k = Alive(n, ORIG_Impact) && S16(n, 0xC) != cnt ? num : 0;
			if (k)
			{
				S16(c, 0x18) = lerp16(S16(c, 0x18), S16(n, 0x18), k, den);
				S16(c, 0x1C) = lerp16(S16(c, 0x1C), S16(n, 0x1C), k, den);
			}
			ImpactDraw(c, HeldFade(cnt, k, den, 8, RampImpact));
			break;
		}
		case REC_BALL:
		{
			const int k = Alive(n, ORIG_Ball) && S16(n, 0xC) != cnt ? num : 0;
			const int i = S16(c, 0xE);
			const int32_t a = S16(c, 0x18), b = k ? S16(n, 0x18) : a;
			// the point num/den of the way from curve point a to b, between the two curve points around it
			const int32_t f = a * den + (b - a) * k;
			const int32_t j = f / den, fr = f % den;
			int16_t pos[4];
			memcpy(pos, (const void *)Point(i, j), 8);
			if (fr)
				for (int q = 0; q < 3; q++) pos[q] = lerp16(S16(Point(i, j), 2 * q), S16(Point(i, j + 1), 2 * q), fr, den);
			BallDraw(c, pos);
			break;
		}
		}
	}

	static void PartsHeld(const Rec &r, int num, int den)
	{
		const uint32_t c = (uint32_t)r.copy;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		const bool burst = r.kind == REC_BURST;
		U32(h, 0) = burst ? SEQ_Burst : r.kind == REC_TRAIL2 ? SEQ_Trail2 : r.kind == REC_EMITTER ? SEQ_Emitter : SEQ_Trail;
		U16(h, 0x24) = 0;
		for (int q = r.first; q < r.first + r.count; q++)
		{
			const PRec &pr = g_parts[q];
			uint8_t pc[0x18];
			memcpy(pc, pr.copy, sizeof(pc));
			const uint32_t p = (uint32_t)pc;
			const int k = PartAlive(pr) ? num : 0;
			if (burst)
			{
				if (k)
					for (int j = 0; j < 3; j++) S16(p, P_POS + 2 * j) = lerp16(S16(p, P_POS + 2 * j), S16(pr.addr, P_POS + 2 * j), k, den);
				int16_t w[3];
				BurstPoint(S16(c, 0xE), (const int16_t *)(p + P_POS), w);
				const int16_t size = S16(p, P_SIZE);
				SeqDraw(h, w, size, -(size >> 4), S16(p, P_FRAME));
			}
			else
			{
				if (k) S16(p, P_SIZE) = lerp16(S16(p, P_SIZE), S16(pr.addr, P_SIZE), k, den);
				const int16_t size = S16(p, P_SIZE);
				SeqDraw(h, (const void *)(p + P_POS), size, -(size >> 4), S16(p, P_FRAME));
			}
		}
		FieldFree(0xB4);
	}

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
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
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == REC_BURST || r.kind == REC_TRAIL || r.kind == REC_TRAIL2 || r.kind == REC_EMITTER) PartsHeld(r, num, den);
			else NodeHeld(r, num, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}

	static void register_mag156_held()
	{
		mark_held(ORIG_Flash);
		mark_held(ORIG_Burst);
		mark_held(ORIG_Ring);
		mark_held(ORIG_Ball);
		mark_held(ORIG_Trail);
		mark_held(ORIG_Trail2);
		mark_held(ORIG_Impact);
		mark_held(ORIG_ImpactRing);
		mark_held(ORIG_Emitter);
		register_module_held(156, HeldReady, HeldDraw);
	}
}
}
