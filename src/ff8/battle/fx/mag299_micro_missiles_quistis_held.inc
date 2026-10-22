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

// 30 fps part of mag299_micro_missiles_quistis.cpp (Quistis' Micro Missiles, effect 299). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// A real tick records its draws in order (the node as the task started, the GTE state and the Field_Alloc scratch each draw
// started from); a held frame replays them in the same order on private packets:
//   Missiles - the progress t steps by +0x32 (0xCC) a tick and every drawn point is a function of it and of the missile's
//              fixed frame / length / launch point: each trail sprite (slot k, frame 0xB - k: its sample s = t - 0x990 + 0xCC k)
//              half way between its world point at s and at s + step (the GTE path of 0x6700C0), its frame held; a sprite the
//              next tick does not draw (s + step past 0x1000, the missile ended) holds its point; a slot entering the trail on
//              the next tick is not drawn (15 Hz growth). The head (while t <= 0x1000): its point (the float path) and the step
//              vector its basis is built from half way between this tick's and the next tick's (the next step = the next point
//              minus this tick's point, with the task's own y sign rule), the basis rebuilt from the in-between step, composed
//              with the frame's camera, the layout through prim::play_held; the head's last draw (t + step > 0x1000) holds.
//   Hits     - the layout through prim::play_held at its fixed frame (composed with the frame's camera); a hit that ends on
//              this tick holds; the flash (counters 0..0xD) at the impact point, frame N held.
//   Sparks   - half way to the next position (pos + velocity), frame N held; the last frame (0xB) holds.
// Spawns (missiles, hits, sparks) stay at 15 Hz. No task has a draw-only mode and no part follows an animating model (the launch
// point is the caster's default effect position at the spawn, the aim a target bone at the spawn). The module packet arena
// alternates per tick: the held packets start where the tick's first draw started. The camera is the frame gate's (every draw
// composes with it when it draws). The GTE, the Field_Alloc scratch and the engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace micromissiles299
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	enum : uint32_t { REC_MISSILE = 1, REC_HIT = 2, REC_FLASH = 3, REC_SPARK = 4 };

	struct Rec
	{
		uint32_t kind;
		uint32_t node;            // the live node
		uint8_t bytes[0x34];      // missile: the node's header as the task started (layout excluded)
		HitNode hit;              // hit / flash: the node as drawn (its layout pointer: the live node's)
		SparkNode spark;          // spark: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[6 + 2 * 6 + 0x60];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x4580)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = (uint32_t)node;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_missile(const MissileNode *m)
	{
		Rec *r = NewRec(REC_MISSILE, m);
		if (r) memcpy(r->bytes, m, sizeof(r->bytes));
	}

	static void held_note_hit(const HitNode *h)
	{
		Rec *r = NewRec(REC_HIT, h);
		if (r) memcpy(&r->hit, h, 0x30);
	}

	static void held_note_flash(const HitNode *h)
	{
		Rec *r = NewRec(REC_FLASH, h);
		if (r) memcpy(&r->hit, h, 0x30);
	}

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK, s);
		if (r) r->spark = *s;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the node runs its task on the next tick (the executor clears word +0 of a node that ended; nodes are only reused by spawns
	// of a later tick)
	static bool Live(uint32_t node, uint32_t task) { return (var<uint16_t>(node) & 1) != 0 && var<uint32_t>(node + 8) == task; }

	// ------------------------------------------------------------------
	// Held frame
	// ------------------------------------------------------------------
	static int32_t Lerp32(int32_t a, int32_t b, int num, int den) { return (int32_t)((uint32_t)a + (uint32_t)(int32_t)((int64_t)((int32_t)((uint32_t)b - (uint32_t)a)) * num / den)); }

	// the missile's draw state num/den of the way to the next tick (trail sprites, head), drawn
	static void MissileHeld(const Rec &r, int num, int den)
	{
		alignas(4) uint8_t copy[sizeof(MissileNode)];
		memcpy(copy, r.bytes, sizeof(r.bytes));
		const MissileNode *m = (const MissileNode *)copy;
		const bool alive = Live(r.node, ORIG_MissileTask);
		const int32_t step = m->step;
		uint8_t *h = AllocHeader(0xB4);
		int32_t s = m->t - 0x990;
		for (int16_t frame = 0xB; ; frame--)
		{
			if (s >= 0 && s < 0x1000)
			{
				int32_t p[3];
				TrailPoint(m, s, p);
				const int32_t s1 = s + step;
				if (alive && s1 >= 0 && s1 < 0x1000 && num)
				{
					int32_t q[3];
					TrailPoint(m, s1, q);
					for (int c = 0; c < 3; c++) p[c] = Lerp32(p[c], q[c], num, den);
				}
				TrailDraw(h, p, frame);
			}
			s += 0xCC;
			if (s >= 0x1000 || frame == 0) break;
		}
		FieldFree(0xB4);
		const int32_t t = m->t;
		if (t <= 0x1000)
		{
			int32_t p[3], v[3];
			HeadPoint(m, t, p);
			HeadStep(m->cur, p, v);
			const int32_t t1 = (int16_t)(m->t + m->step);
			const bool next = alive && t1 <= 0x1000 && num;
			if (next)
			{
				int16_t cur[3] = { (int16_t)p[0], (int16_t)p[1], (int16_t)p[2] };
				int32_t p1[3], v1[3];
				HeadPoint(m, t1, p1);
				HeadStep(cur, p1, v1);
				for (int c = 0; c < 3; c++)
				{
					p[c] = Lerp32(p[c], p1[c], num, den);
					v[c] = Lerp32(v[c], v1[c], num, den);
				}
			}
			PrimArg arg;
			HeadArg(p, v, &arg);
			arg.morph = (uint32_t)g_held_morph;
			prim::play_held((prim::Layout *)((MissileNode *)r.node)->layout, PrimCallback, (int)&arg, next ? num : 0, den);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
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
			switch (r.kind)
			{
			case REC_MISSILE:
				MissileHeld(r, num, den);
				break;
			case REC_HIT:
			{
				PrimArg arg;
				HitArg(&r.hit, &arg);
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)((HitNode *)r.node)->layout, PrimCallback, (int)&arg, Live(r.node, ORIG_HitTask) ? num : 0, den);
				break;
			}
			case REC_FLASH:
			{
				PrimArg arg;
				HitArg(&r.hit, &arg);
				FlashDraw(&arg, r.hit.counter);
				break;
			}
			case REC_SPARK:
			{
				SparkNode p = r.spark;
				// half way to the next position (its last frame holds)
				if (p.counter + 1 < 0xC)
					for (int c = 0; c < 3; c++) p.pos[c] = lerp_angle(p.pos[c], (int16_t)(p.pos[c] + p.vel[c]), num, den);
				SparkDraw(&p);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag299_held()
	{
		mark_held(micromissiles299::ORIG_MissileTask);
		mark_held(micromissiles299::ORIG_HitTask);
		mark_held(micromissiles299::ORIG_SparkTask);
		register_module_held(299, micromissiles299::HeldReady, micromissiles299::HeldFrame);
	}
}
