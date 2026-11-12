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

// 30 fps part of mag321_ker_plunk.cpp (effect 321). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws the state
// num/den of the way from the last real tick to the next one.
//
// A real tick records its draws in order (the root's seam starts the log) with the GTE registers and the Field_Alloc scratch
// each one started from; a held frame replays them in the same order with the vanilla draw code, each at its in-between state:
//   Rings        - size (point ring) / height (target ring) half way along their growth, fade along its ramp (the switch
//                  holds).
//   Dust         - height half way to the update's (rise); flipbook frames held; the new ones (CRT rand) appear on the real
//                  frame; a mote whose sequence ends holds below 1.
//   Burst        - x / z half way to the update's; as the dust otherwise.
//   Debris       - angles (short way), height and the shrink of the frame half way; a piece at its last frame holds.
//   Lifted targets - listed to the frame gate (HeldEntities): position / angles continuous while lifted, the put-back of
//                  tick 0x42 a jump (held frames keep the lifted pose until the real frame shows the target back).
// A part the next tick no longer runs (or whose counter did not move: draw-only tick) holds its picture (state N). The module
// packets go to a private buffer (HeldScope); the engine words the sprite draws touch are put back. The white flash (screen
// feedback) is the engine's at 15 Hz.

namespace ff8fx
{
namespace kerplunk321
{
	static const uint32_t SCRATCH = 0x400;
	static const int MAX_RECS = 0x80, MAX_PARTS = 0x200;

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
	static bool g_overflow = false;
	static Rec *g_cur = nullptr;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static PRec g_parts[MAX_PARTS];
	static uint8_t g_held_packets[0xE000];  // one module arena
	static bool g_held = false;

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
		if (g_held || g_log_tick != g_real_tick) return nullptr;
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

	// the node runs on: same task, its counter one on (not on a draw-only tick)
	static bool Next(const Rec &r)
	{
		const TaskNode *t = (const TaskNode *)r.node;
		return !DrawOnly() && (t->flags & 1) && t->func == ((const TaskNode *)r.copy)->func && S16(r.node, 0xC) == (int16_t)(*(const int16_t *)(r.copy + 0xC) + 1);
	}

	// a particle the tick drew that lives on (same owner, its frame one on: not a new one in a freed slot)
	static bool PartAlive(const PRec &q)
	{
		return U32(q.addr, P_MASK) == *(const uint32_t *)(q.copy + P_MASK) && S16(q.addr, P_FRAME) == (int16_t)(*(const int16_t *)(q.copy + P_FRAME) + 1);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		g_held = true;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_LIFT) continue;
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			uint8_t cp[0x24];
			memcpy(cp, r.copy, sizeof(cp));
			const uint32_t c = P(cp), n = r.node;
			const int16_t cnt = S16(c, 0xC);
			const int k = Next(r) ? num : 0;
			if (r.kind == REC_RING || r.kind == REC_TRING)
			{
				memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
				const bool ring = r.kind == REC_RING;
				const int32_t from = ring ? 0xC : 6;
				int32_t fade = RingFade(cnt, from);
				if (k)
				{
					const int o = ring ? 0x1C : 0x20;
					S16(c, o) = lerp16(S16(c, o), S16(n, o), k, den);
					if (cnt >= from) fade = lerp_i(fade, RingFade(cnt + 1, from), k, den);
				}
				if (ring) PointRingDraw(c, fade);
				else TargetRingDraw(c, fade);
				continue;
			}
			// particle tasks: the work block first (allocated before the record), then the scratch the draws started from
			const bool debris = r.kind == REC_DEBRIS;
			const uint32_t w = P(FieldAlloc(debris ? 0x68 : 0x48));
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			const uint32_t h = P(FieldAlloc(debris ? 0x58 : 0xB4));
			if (debris) DebrisSetup(h, w);
			else SeqSetup(h, w);
			for (int q = r.first; q < r.first + r.count; q++)
			{
				const PRec &pr = g_parts[q];
				uint8_t pc[0x18];
				memcpy(pc, pr.copy, sizeof(pc));
				const uint32_t p = P(pc);
				const int kp = !DrawOnly() && PartAlive(pr) ? num : 0;
				if (debris)
				{
					const int16_t f = S16(p, P_FRAME);
					int32_t s = DebrisScale(S16(p, P_SIZE), f);
					if (kp)
					{
						S16(p, 0x10) = lerp16(S16(p, 0x10), S16(pr.addr, 0x10), kp, den);
						S16(p, 0x14) = lerp16(S16(p, 0x14), S16(pr.addr, 0x14), kp, den);
						S16(p, P_POS + 2) = lerp16(S16(p, P_POS + 2), S16(pr.addr, P_POS + 2), kp, den);
						s = lerp_i(s, DebrisScale(S16(p, P_SIZE), f + 1), kp, den);
					}
					DebrisDraw(h, w, p, s);
				}
				else
				{
					if (kp)
						for (int j = 0; j < 3; j++) S16(p, P_POS + 2 * j) = lerp16(S16(p, P_POS + 2 * j), S16(pr.addr, P_POS + 2 * j), kp, den);
					SeqDraw(h, w, p);
				}
			}
			FieldFree(debris ? 0x58 : 0xB4);
			FieldFree(debris ? 0x68 : 0x48);
		}
		g_held = false;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
	}

	// held-frame entities (fx_held.h HeldEntity, drawn by the frame gate): every lifted target; position / angles continuous
	// while lifted, the put-back (counter 0x42 on) a jump
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (!HeldReady()) return 0;
		int k = 0;
		for (int i = 0; i < g_nrec && k < max; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind != REC_LIFT) continue;
			const int16_t cnt = *(const int16_t *)(r.copy + 0xC);
			held_entity_init(out[k], Entity(*(const int16_t *)(r.copy + 0xE)));
			if (cnt >= 0x42)
				for (int c = 0; c < 6; c++) held_entity_jump(out[k], c);
			k++;
		}
		return k;
	}

	static void register_mag321_held()
	{
		mark_held(ORIG_Ring);
		mark_held(ORIG_TargetRing);
		mark_held(ORIG_Dust);
		mark_held(ORIG_Burst);
		mark_held(ORIG_Debris);
		register_module_held(321, HeldReady, HeldFrame);
		register_module_entity(321, HeldEntities);
	}
}
}
