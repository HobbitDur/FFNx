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

// 30 fps part of mag168_sleeping_gas.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag168_sleeping_gas.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every drawing task in order: the gas clouds (after their shape update, with
// the vertices as drawn and the cloud record the next tick's update starts from) and the sparkle
// tasks (before their loop, with the whole pool as drawn), each with the GTE registers and the
// Field_Alloc scratch it started from (the sparkle header keeps scratch bytes the code does not
// write). A held frame at tick + num / den replays the log in the same order:
//   Gas cloud     - the next tick's shape is computed from the recorded cloud record (the wave update
//                   and the vertex build of 0x5E2220 / 0x5E22E0 on copies, into a private buffer);
//                   every vertex moves num / den of the way to it (the length growth included: the
//                   collapsed pairs unfold); the fade ramp at tick + num / den (held across the
//                   ramp ends); the matrix as drawn. A cloud that ended on the tick holds.
//   Sparkles      - every particle drawn moves num / den of the way to where its update put it
//                   (the live pool entry); the flipbook time holds; the target sparkles' fade-in
//                   colour at tick + num / den (held when it ends on the next tick). Particles that
//                   ended hold; new particles appear on the next tick.
//   At fraction 1 (num == den) the frame is the next tick's: the predicted cloud shapes and fades,
//   the sparkle pools as the next tick will draw them (live entries, flipbook times included);
//   ended clouds / tasks draw nothing.
// The module packets go to a private buffer (HeldScope). The director and the root draw nothing.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing but the gas wave moves: the
// sparkles and fades hold, the cloud shapes move toward the next update.

namespace ff8fx
{
namespace gas168
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x14000;  // module packet arena per tick
	static const uint32_t POOL_BYTES = POOL_A_END - POOL_A;

	struct Rec
	{
		uint32_t fn;
		const void *live;              // the node itself
		uint8_t node[0x24];            // the node as drawn
		uint8_t pool[POOL_BYTES];      // sparkles: the pool as drawn
		uint8_t verts[CLOUD_VERTS];    // gas: the vertices as drawn
		Cloud cloud;                   // gas: the record the next update starts from
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static bool g_overflow = false;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[32];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_verts[CLOUD_VERTS]; // the held cloud shape
	static uint8_t g_next_verts[CLOUD_VERTS]; // the next tick's cloud shape
	static uint8_t g_held_pool[POOL_BYTES];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_overflow = false;
		g_packets = 0;
	}

	static Rec *held_add(uint32_t fn, const void *node)
	{
		if (g_log_tick != g_real_tick) return nullptr;
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0])))
		{
			g_overflow = true;
			return nullptr;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = node;
		memcpy(r.node, node, sizeof(r.node));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_gas(const GasNode *g)
	{
		Rec *r = held_add(ORIG_Gas, g);
		if (!r) return;
		memcpy(r->verts, (const void *)CloudVerts(g->cloud), CLOUD_VERTS);
		r->cloud = *CloudRec(g->cloud);
	}

	static void held_note_spark(uint32_t fn, const SparkNode *s)
	{
		Rec *r = held_add(fn, s);
		if (!r) return;
		if (fn == ORIG_SparkTarget) memcpy(r->pool, (const void *)POOL_A, POOL_A_END - POOL_A);
		else memcpy(r->pool, (const void *)POOL_B, POOL_B_END - POOL_B);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick && !g_overflow; }

	// a task node that runs again on the next tick (not unlinked, not reused)
	static bool Runs(const Rec &r)
	{
		const TaskNode *n = (const TaskNode *)r.live;
		return (n->flags & 1) && (uint32_t)n->func == r.fn;
	}

	static void HeldGas(const Rec &r, int num, int den, bool moving)
	{
		const GasNode *a = (const GasNode *)r.node;
		const bool next = !moving || a->counter + 1 < 0x3C; // the cloud is drawn on the next tick
		const bool one = num >= den;
		if (one && !next) return;
		if (next)
		{
			// the next tick's update on copies (the vertex buffer's never-written pad words as drawn)
			Cloud c = r.cloud;
			int32_t offs[WAVE * 2];
			memcpy(g_next_verts, r.verts, CLOUD_VERTS);
			WaveStep(&c, offs);
			WaveBuild(&c, offs, P(g_next_verts), P(r.live));
		}
		if (one) memcpy(g_held_verts, g_next_verts, CLOUD_VERTS);
		else
		{
			memcpy(g_held_verts, r.verts, CLOUD_VERTS);
			if (next)
				for (uint32_t k = 0; k < CLOUD_VERTS; k += 8)
					for (uint32_t w = 0; w < 6; w += 2)
						S16(P(g_held_verts) + k, w) = lerp16(S16(P(r.verts) + k, w), S16(P(g_next_verts) + k, w), num, den);
		}
		GasNode m = *a;
		if (one && moving) m.counter++;
		GasMatrix(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		GasHeader(h, &m, P(g_held_verts));
		if (!one && next && moving)
		{
			// the fade at tick + num / den (a ramp; held where the next tick changes the mode)
			uint8_t nh[0x58];
			memcpy(nh, h, sizeof(nh));
			GasNode m1 = m;
			m1.counter++;
			GasHeader(nh, &m1, P(g_held_verts));
			LerpPrimFade(h, nh, num, den);
		}
		PacketCursor() = QuadModel(P(h), RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// a particle drawn on the real tick that its update moved (the live entry is its next state)
	static bool Continued(const Spark &e, const Spark &l, int32_t mask)
	{
		return (l.owner & mask) && l.frame == (int16_t)(e.frame + 1) && l.scale == e.scale && l.vel[0] == e.vel[0] && l.vel[2] == e.vel[2];
	}

	static void HeldSparks(const Rec &r, int num, int den, bool moving)
	{
		const SparkNode *a = (const SparkNode *)r.node;
		const bool target = r.fn == ORIG_SparkTarget;
		const uint32_t pool = target ? POOL_A : POOL_B, end = target ? POOL_A_END : POOL_B_END;
		const int32_t mask = target ? (int32_t)a->mask : 1;
		const bool one = num >= den;
		int16_t counter = a->counter;
		if (one)
		{
			// the next tick's draw: the live pool, if the task runs again
			if (!Runs(r)) return;
			memcpy(g_held_pool, (const void *)pool, end - pool);
			counter = ((const SparkNode *)r.live)->counter;
		}
		else memcpy(g_held_pool, r.pool, end - pool);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Sparkle;
		if (!target) *(uint16_t *)(h + 0x24) = 8;
		for (uint32_t off = 0; off < end - pool; off += 0x18)
		{
			Spark e = *(const Spark *)(g_held_pool + off);
			if (!(e.owner & mask)) continue;
			const Spark &l = *(const Spark *)(pool + off);
			const bool cont = !one && moving && Continued(e, l, mask);
			if (cont)
				for (int k = 0; k < 3; k++) e.pos[k] = lerp16(e.pos[k], l.pos[k], num, den);
			if (target)
			{
				const Spark drawn = e;
				SparkTargetFade(h, &e, counter);
				if (cont && drawn.fade != 0 && counter + 1 < drawn.fade_len)
				{
					const int32_t q = 0x80 / drawn.fade_len;
					const int32_t v = lerp_i(q * counter, q * (counter + 1), num, den);
					*(uint32_t *)(h + 0x1C) = (uint32_t)(shl32(shl32(v, 8) | v, 8) | v);
				}
			}
			else *(int16_t *)(h + 4) = e.frame;
			SparkDraw(h, &e, target ? 4 : 3);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const bool moving = !DrawOnly();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.fn == ORIG_Gas) HeldGas(r, num, den, moving);
			else HeldSparks(r, num, den, moving);
		}
	}
}

	static void register_mag168_held()
	{
		mark_held(gas168::ORIG_Gas);
		mark_held(gas168::ORIG_SparkTarget);
		mark_held(gas168::ORIG_SparkCaster);
		register_module_held(168, gas168::HeldReady, gas168::HeldFrame);
	}
}
