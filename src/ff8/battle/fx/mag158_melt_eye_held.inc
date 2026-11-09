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

// 30 fps part of mag158_melt_eye.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag158_melt_eye.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick logs each draw in order: the node as drawn
// (and where it lives), the GTE registers and the Field_Alloc scratch it started from, and for the
// table-drawing tasks the table as drawn. A held frame replays the log in the same order, each part
// at its exact in-between state between what tick N drew (the log) and what tick N+1 draws (the
// node / table after the update); discrete things (flipbook frames, model flags) are tick N's up to
// half way and tick N+1's after; a part the next tick does not draw (its task ended, its entry was
// freed) holds up to half way and is gone after:
//   Ring        - scale half way (+0x1C), fade half way (from counter 8).
//   SplashModel - angle and scale half way, fade half way (from counter 6).
//   Splash      - fixed sprite; frame discrete.
//   Trail       - each of the 7 path sprites half way between its path point on both ticks (the head
//                 advances several points a tick); frame discrete; its sparks fixed, frame discrete.
//   Drops       - position and size half way, frame discrete; drops born on the tick (after the
//                 draws) appear past half way.
// Trail sparks the next tick spawns (before its draws) appear with it. The module packet arena
// alternates with the tick's parity: the held packets start where the tick's first draw started
// (and past half way sit on the other arena's bytes, the next tick's). In the draw-only mode
// nothing moves: every part holds.

namespace ff8fx
{
namespace melteye158
{
	static const uint32_t SCRATCH = 0x200;
	static const int MAX_RECS = 72;
	static const uint32_t TABLE_BYTES = SPARKS_END - SPARKS; // the larger of the two tables

	struct Rec
	{
		uint32_t kind;
		EffectNode *at;               // the live node
		EffectNode node;              // the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
		uint8_t table[TABLE_BYTES];   // trail sparks / drops as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_held_packets[ARENA_SIZE];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note(uint32_t kind, const EffectNode *t)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.at = (EffectNode *)t;
		r.node = *t;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_table(uint32_t table)
	{
		if (g_log_tick != g_real_tick || g_nrec == 0) return;
		Rec &r = g_recs[g_nrec - 1];
		memcpy(r.table, (const void *)table, table == SPARKS ? SPARKS_END - SPARKS : DROPS_END - DROPS);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the node the task left for the next tick, or nullptr when it ended on the tick
	static const EffectNode *NextNode(const Rec &r, bool moving)
	{
		const EffectNode *a = r.at;
		if (!(a->hdr.flags & 1) || a->hdr.func != r.node.hdr.func) return nullptr;
		if (a->counter != (int16_t)(r.node.counter + (moving ? 1 : 0))) return nullptr;
		return a;
	}

	static void RingHeld(const Rec &r, const EffectNode *nx, int num, int den, bool late)
	{
		EffectNode q = r.node;
		const int16_t c = r.node.counter;
		if (nx) q.w1C = lerp16(r.node.w1C, nx->w1C, num, den);
		const int16_t hc = (late && nx) ? nx->counter : c;
		RingMatrix(&q);
		uint8_t *h = ModelHeader(MODEL_Ring, hc, 8, 682);
		if (nx && c >= 8 && nx->counter >= 8) *(int32_t *)(h + 0xC) = lerp_i(((int32_t)c - 8) * 682, ((int32_t)nx->counter - 8) * 682, num, den);
		RenderModel(h, -2);
	}

	static void SplashModelHeld(const Rec &r, const EffectNode *nx, int num, int den, bool late)
	{
		EffectNode q = r.node;
		const int16_t c = r.node.counter;
		if (nx)
		{
			q.w18 = lerp16(r.node.w18, nx->w18, num, den);
			q.w1C = lerp16(r.node.w1C, nx->w1C, num, den);
		}
		const int16_t hc = (late && nx) ? nx->counter : c;
		SplashModelMatrix(&q);
		uint8_t *h = ModelHeader(MODEL_Splash, hc, 6, 0x200);
		if (nx && c >= 6 && nx->counter >= 6) *(int32_t *)(h + 0xC) = lerp_i(((int32_t)c - 6) * 0x200, ((int32_t)nx->counter - 6) * 0x200, num, den);
		RenderModel(h, 2);
	}

	static void TrailHeld(const Rec &r, const EffectNode *nx, int num, int den, bool late, bool moving)
	{
		const EffectNode &n = r.node;
		const uint32_t mask = 1u << (n.w0E & 31);
		const Beam *b = BeamRec(n.w0E);
		uint8_t *h = TrailHeader(late && nx ? nx : &n);
		int32_t size = n.w1C;
		for (int k = 0; k < 7; k++, size -= 0x30)
		{
			const int32_t i0 = n.w18 - k, i1 = nx ? nx->w18 - k : -1;
			const bool v0 = i0 >= 0 && i0 < 0x30, v1 = nx && i1 >= 0 && i1 < 0x30;
			int16_t pos[4];
			if (v0 && v1)
			{
				for (int c = 0; c < 3; c++) pos[c] = lerp16(b->path[i0][c], b->path[i1][c], num, den);
				pos[3] = (late ? b->path[i1] : b->path[i0])[3];
			}
			else if (v0 && !late) memcpy(pos, b->path[i0], 8);
			else if (v1 && late) memcpy(pos, b->path[i1], 8);
			else continue;
			SpriteAt(h, pos, size, 3);
		}
		const Spark *pre = (const Spark *)r.table, *post = Sparks();
		for (int i = 0; i < SPARK_COUNT; i++)
		{
			if (!(pre[i].mask & mask)) continue;
			const bool cont = (post[i].mask & mask) && post[i].frame == (int16_t)(pre[i].frame + (moving ? 2 : 0));
			if (late && !cont) continue;
			*(int16_t *)(h + 4) = late ? post[i].frame : pre[i].frame;
			SpriteAt(h, pre[i].pos, pre[i].size, 4);
		}
		FieldFree(0xB4);
	}

	static void DropsHeld(const Rec &r, int num, int den, bool late, bool moving)
	{
		const uint32_t mask = (uint32_t)(int32_t)r.node.w0E;
		const Drop *pre = (const Drop *)r.table, *post = Drops();
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Drop;
		*(int16_t *)(h + 0x24) = 0;
		for (int i = 0; i < DROP_COUNT; i++)
		{
			const bool pa = (pre[i].mask & mask) != 0, pb = (post[i].mask & mask) != 0;
			const bool cont = pa && pb && post[i].frame == (int16_t)(pre[i].frame + (moving ? 1 : 0));
			Drop q;
			if (cont)
			{
				q = late ? post[i] : pre[i];
				for (int c = 0; c < 3; c++) q.pos[c] = lerp16(pre[i].pos[c], post[i].pos[c], num, den);
				q.size = lerp16(pre[i].size, post[i].size, num, den);
			}
			else if (pa && !late) q = pre[i];
			else if (pb && late && moving && post[i].frame == 0) q = post[i]; // born on the tick, after the draws
			else continue;
			*(int16_t *)(h + 4) = q.frame;
			SpriteAt(h, q.pos, q.size, 4);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const bool late = 2 * num > den;
		// past half way the draws are the next tick's: under them, what the next tick's arena (the
		// other one of the pair) holds, as the bytes its packets do not write
		if (late && g_packets)
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
			const EffectNode *nx = NextNode(r, moving);
			if (r.kind == REC_DROPS)
			{
				DropsHeld(r, num, den, late, moving);
				continue;
			}
			if (r.kind == REC_TRAIL)
			{
				TrailHeld(r, nx, num, den, late, moving);
				continue;
			}
			if (!nx && late) continue; // the task ended on the tick: the next tick does not draw it
			switch (r.kind)
			{
			case REC_RING: RingHeld(r, nx, num, den, late); break;
			case REC_SPLASHMODEL: SplashModelHeld(r, nx, num, den, late); break;
			case REC_SPLASH:
			{
				EffectNode q = r.node;
				if (late) q.counter = nx->counter;
				SplashDraw(&q);
				break;
			}
			}
		}
	}
}

	static void register_mag158_held()
	{
		mark_held(melteye158::ORIG_Ring);
		mark_held(melteye158::ORIG_SplashModel);
		mark_held(melteye158::ORIG_Splash);
		mark_held(melteye158::ORIG_Trail);
		mark_held(melteye158::ORIG_Drops);
		register_module_held(158, melteye158::HeldReady, melteye158::HeldFrame);
	}
}
