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

// 30 fps part of mag178_10000_needles.cpp: the tables as the real tick drew them and the held-frame
// drawing (included at the end of mag178_10000_needles.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// The needle task draws THEN updates. A real tick keeps the needle table as it was drawn (after the
// tick's spawns), the spark table as it was drawn (after the needles' updates started their sparks),
// and the GTE registers and Field_Alloc scratch each phase started from. A held frame replays both
// phases in the same order, each entry at its exact in-between state:
//   needles - the position half way to the one the update gave it (its step); a needle whose steps
//             ran out on the tick (the next tick does not draw it) holds up to half way and is gone
//             after; needles spawned by the next tick (random) appear with it.
//   sparks  - the y offset (table 0xD44C34 by the ticks left) half way to the next tick's; a spark
//             ending on the tick holds up to half way and is gone after.
// The module packet arena alternates with the tick's parity: the held packets start where the
// tick's first draw started (and past half way sit on the other arena's bytes, the next tick's).
// In the draw-only mode nothing moves: every part holds.

namespace ff8fx
{
namespace needles178
{
	static const uint32_t SCRATCH = 0x100;

	struct Rec
	{
		Needle needles[NEEDLE_COUNT];   // the needle table as drawn
		Spark sparks[SPARK_COUNT];      // the spark table as drawn
		bool has_sparks;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];        // at the needles' first draw
		uint8_t gte_sparks[GTE_DATA_SIZE + GTE_CTRL_SIZE]; // at the sparks' first draw
		uint8_t scratch[SCRATCH];
		uint8_t scratch_sparks[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[2];
	static uint8_t g_held_packets[ARENA_SIZE];

	static void KeepGte(uint8_t *dst)
	{
		memcpy(dst, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(dst + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}
	static void PutGte(const uint8_t *src)
	{
		memcpy((void *)GTE_DATA, src, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, src + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_needles(const EffectNode *)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		memcpy(r.needles, Needles(), sizeof(r.needles));
		r.has_sparks = false;
		KeepGte(r.gte);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_sparks()
	{
		if (g_log_tick != g_real_tick || g_nrec == 0) return;
		Rec &r = g_recs[g_nrec - 1];
		memcpy(r.sparks, Sparks(), sizeof(r.sparks));
		r.has_sparks = true;
		KeepGte(r.gte_sparks);
		memcpy(r.scratch_sparks, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void NeedlesHeld(const Rec &r, int num, int den)
	{
		const bool late = 2 * num > den;
		// the task's first block (its spawn work area) as the real tick left it
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = 0;
		*(int32_t *)(s + 0x28) = -0x1000;
		FieldFree(0x50);
		uint8_t *b = NeedleBlock();
		for (int i = 0; i < NEEDLE_COUNT; i++)
		{
			const Needle &pre = r.needles[i], &post = Needles()[i];
			if (!(pre.active & 1)) continue;
			Needle q = pre;
			if (post.active & 1)
				for (int k = 0; k < 3; k++) q.m.t[k] = lerp_i(pre.m.t[k], post.m.t[k], num, den);
			else if (late) continue; // its steps ran out: the next tick does not draw it
			DrawNeedle(b, &q);
		}
		FieldFree(0xDC);
	}

	static void SparksHeld(const Rec &r, int num, int den)
	{
		const bool late = 2 * num > den;
		uint8_t *h = SparkHeader();
		for (int i = 0; i < SPARK_COUNT; i++)
		{
			const Spark &pre = r.sparks[i], &post = Sparks()[i];
			if (!(pre.active & 1)) continue;
			int32_t lift = SparkLift(pre.left);
			if (post.active & 1) lift = lerp_i(lift, SparkLift(post.left), num, den);
			else if (late) continue; // its last tick: the next tick does not draw it
			DrawSpark(h, &pre, lift);
		}
		FieldFree(0xB4);
		FieldFree(0x28);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// past half way the draws are the next tick's: under them, what the next tick's arena (the
		// other one of the pair) holds, as the bytes its packets do not write
		if (2 * num > den && g_packets)
		{
			const uint32_t other = g_packets >= ARENA1 ? g_packets - ARENA_SIZE : g_packets + ARENA_SIZE;
			memcpy(g_held_packets, (const void *)other, sizeof(g_held_packets));
		}
		const Mat4x3 keep_sprite = *(const Mat4x3 *)SPRITE_MATRIX; // module data the sprite draws write: put back after
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// each phase starts from the GTE state and scratch of the real one
			PutGte(r.gte);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			NeedlesHeld(r, num, den);
			if (!r.has_sparks) continue;
			PutGte(r.gte_sparks);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch_sparks, SCRATCH);
			SparksHeld(r, num, den);
		}
		*(Mat4x3 *)SPRITE_MATRIX = keep_sprite;
	}
}

	static void register_mag178_held()
	{
		mark_held(needles178::ORIG_NeedleTask);
		register_module_held(178, needles178::HeldReady, needles178::HeldFrame);
	}
}
