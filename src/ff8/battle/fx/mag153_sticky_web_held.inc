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

// 30 fps part of mag153_sticky_web.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag153_sticky_web.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick logs each task's draw in queue order: the
// node as drawn, the GTE registers and Field_Alloc scratch it started from, and per task:
//   thread    - the anchor, the history ring and the spline samples it drew. The next tick's
//               state is predicted exactly (angle / height step, the new ring entry, the splines
//               of the new history: engine spline code on private copies); the held frame draws
//               the line to the in-between anchor and the strands through the in-between
//               samples (per-sample lerp of the two curves), the depth-cued colour and the
//               GTE clipping as the game computes them for those points.
//   web       - clip height (the thread's anchor height, next = the thread's predicted one) and
//               the fade ramp lerped; the clipped -> whole switch holds.
//   burst     - scale (s + step) and the fade ramp lerped; the fade switch at 6 holds.
//   particles - each particle's position lerped to the one its update computed (the update
//               records it); flipbook frames stay 15 Hz; a particle that died holds.
// The caster position (engine-driven bone) and the target position hold. At fraction 0 a held
// frame draws exactly the real tick's primitives, at fraction 1 exactly the next tick's (new
// particles, a task that ends: the held frame keeps its last state). Draw-only ticks hold.

namespace ff8fx
{
namespace web153
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x8000;  // module packet arena per tick

	struct Rec
	{
		uint32_t fn;
		Node node;          // as drawn
		bool paused;        // draw-only tick
		int16_t caster[4];  // caster position (held)
		int16_t anchor_y;   // thread anchor height the web clipped at
		// thread
		int16_t anchor[4];
		int32_t npts;
		RingEntry ring[5];
		uint8_t a[16 * 8], b[16 * 8];
		// particles
		int first, count;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PartRec
	{
		Particle drawn, next;
		bool alive;
	};
	struct ThreadPred
	{
		bool valid;
		int16_t anchor[4];
		int32_t npts;
		uint8_t a[16 * 8], b[16 * 8];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0, g_cur_part = -1;
	static uint32_t g_packets = 0;
	static Rec g_recs[8];
	static PartRec g_parts[200];
	static ThreadPred g_pred;
	static uint8_t g_held_arena[ARENA];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_cur_part = -1;
		g_packets = 0;
	}

	static void held_begin(uint32_t fn, const Node *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = Cursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = *n;
		r.paused = (UpdateFlags() & 0x201) != 0;
		memcpy(r.caster, CasterPos(), 8);
		r.anchor_y = Anchor()[1];
		r.npts = 0;
		r.first = g_npart;
		r.count = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static Rec *last_rec(uint32_t fn)
	{
		if (g_log_tick != g_real_tick || g_nrec == 0 || g_recs[g_nrec - 1].fn != fn) return nullptr;
		return &g_recs[g_nrec - 1];
	}

	static void held_note_thread(int32_t points)
	{
		Rec *r = last_rec(ORIG_ThreadTask);
		if (!r) return;
		memcpy(r->anchor, Anchor(), 8);
		r->npts = points;
		memcpy(r->ring, Ring(), sizeof(r->ring));
		if (points > 1)
		{
			memcpy(r->a, (const void *)SPL_A, sizeof(r->a));
			memcpy(r->b, (const void *)SPL_B, sizeof(r->b));
		}
	}

	static void held_note_part(const Particle *e)
	{
		g_cur_part = -1;
		Rec *r = g_nrec ? &g_recs[g_nrec - 1] : nullptr;
		if (g_log_tick != g_real_tick || !r || (r->fn != ORIG_Particles1Task && r->fn != ORIG_Particles2Task)) return;
		if (g_npart >= (int)(sizeof(g_parts) / sizeof(g_parts[0]))) return;
		g_cur_part = g_npart++;
		g_parts[g_cur_part].drawn = *e;
		r->count++;
	}

	static void held_note_part_next(const Particle *e)
	{
		if (g_cur_part < 0) return;
		g_parts[g_cur_part].next = *e;
		g_parts[g_cur_part].alive = e->type != 0;
		g_cur_part = -1;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the thread's next tick (its update, then its draw's computations on private copies)
	static void PredictThread(const Rec &r, ThreadPred &p)
	{
		p.valid = false;
		if (r.paused || r.node.counter + 1 >= 0x1E) return;
		Node m = r.node;
		if (m.counter < 0x19)
		{
			m.f10 = (int16_t)(m.f10 + m.f14);
			m.f12 = (int16_t)(m.f12 + m.f16);
		}
		m.counter++;
		int16_t v[4] = { (int16_t)0xF000, 0, 0, 0 };
		MatrixMulVector((const Mat4x3 *)(Entity(CasterSlot()) + 0x40), v, v);
		const int32_t k = mul32(ComputeSin(m.f10), 600) >> 12;
		p.anchor[0] = (int16_t)((mul32(v[0], k) >> 12) + TargetPos()[0]);
		p.anchor[1] = m.f12;
		p.anchor[2] = (int16_t)((mul32(v[2], k) >> 12) + TargetPos()[2]);
		p.anchor[3] = r.anchor[3];
		RingEntry ring[5];
		memcpy(ring, r.ring, sizeof(ring));
		int32_t slot = m.counter % 5;
		ring[slot].valid = 1;
		ring[slot].p0 = rd32(&r.caster[0]);
		ring[slot].p1 = rd32(&r.caster[2]);
		ring[slot].q0 = rd32(&p.anchor[0]);
		ring[slot].q1 = rd32(&p.anchor[2]);
		const int32_t cnt = m.counter >= 0x19 ? 0x1E - m.counter : 5;
		uint32_t pa[10], pb[10];
		int32_t i = 0;
		while (i < cnt && ring[slot].valid)
		{
			pa[i * 2] = ring[slot].p0;
			pa[i * 2 + 1] = ring[slot].p1;
			pb[i * 2] = ring[slot].q0;
			pb[i * 2 + 1] = ring[slot].q1;
			slot--;
			if (slot < 0) slot = 4;
			i++;
		}
		p.npts = i;
		if (i > 1)
		{
			void *work = FieldAlloc(0x190);
			SplineSetup(i, (uint32_t)pa, work);
			for (int32_t j = 0, t = 0; j < 16; j++, t += 0x1000) SplineEval(i, work, (uint32_t)(p.a + j * 8), t / 15);
			SplineSetup(i, (uint32_t)pb, work);
			for (int32_t j = 0, t = 0; j < 16; j++, t += 0x1000) SplineEval(i, work, (uint32_t)(p.b + j * 8), t / 15);
			FieldFree(0x190);
		}
		p.valid = true;
	}

	static void DrawThreadHeld(const int16_t *caster, const int16_t *anchor, bool strips, const uint8_t *a, const uint8_t *b)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x40);
		DrawLine(caster, anchor, 0x808080);
		if (strips) DrawStrips(h, a, b);
		FieldFree(0x40);
	}

	static void LerpSamples(const uint8_t *x, const uint8_t *y, uint8_t *out, int num, int den)
	{
		const int16_t *p = (const int16_t *)x, *q = (const int16_t *)y;
		int16_t *o = (int16_t *)out;
		for (int k = 0; k < 16; k++)
		{
			for (int c = 0; c < 3; c++) o[k * 4 + c] = lerp16(p[k * 4 + c], q[k * 4 + c], num, den);
			o[k * 4 + 3] = p[k * 4 + 3];
		}
	}

	static void HeldThread(const Rec &r, int num, int den)
	{
		const ThreadPred &p = g_pred;
		if (!p.valid) { DrawThreadHeld(r.caster, r.anchor, r.npts > 1, r.a, r.b); return; }
		if (num >= den) { DrawThreadHeld(r.caster, p.anchor, p.npts > 1, p.a, p.b); return; }
		int16_t anchor[4];
		for (int c = 0; c < 3; c++) anchor[c] = lerp16(r.anchor[c], p.anchor[c], num, den);
		anchor[3] = r.anchor[3];
		static uint8_t a[16 * 8], b[16 * 8];
		if (r.npts > 1 && p.npts > 1)
		{
			LerpSamples(r.a, p.a, a, num, den);
			LerpSamples(r.b, p.b, b, num, den);
		}
		else
		{
			memcpy(a, r.a, sizeof(a));
			memcpy(b, r.b, sizeof(b));
		}
		DrawThreadHeld(r.caster, anchor, r.npts > 1, a, b);
	}

	static void HeldWeb(const Rec &r, int16_t anchor_next, int num, int den)
	{
		const int32_t c = r.node.counter;
		const int16_t ay = r.anchor_y;
		if (r.paused || c + 1 >= 0x2D) WebDraw(&r.node, c, ay, WebFade(c));
		else if (num >= den) WebDraw(&r.node, c + 1, anchor_next, WebFade(c + 1));
		else if (c + 1 < 0x1E) WebDraw(&r.node, c, lerp16(ay, anchor_next, num, den), WebFade(c));
		else if (c >= 0x1E && (c >= 0x25) == (c + 1 >= 0x25)) WebDraw(&r.node, c, ay, lerp_i(WebFade(c), WebFade(c + 1), num, den));
		else WebDraw(&r.node, c, ay, WebFade(c));
	}

	static void HeldBurst(const Rec &r, int num, int den)
	{
		const int32_t c = r.node.counter;
		if (r.paused || c + 1 >= 0xE) { BurstDraw(&r.node, c, BurstFade(c)); return; }
		Node m = r.node;
		const int16_t s1 = (int16_t)(m.f1C + m.f1E);
		if (num >= den)
		{
			m.f1C = s1;
			BurstDraw(&m, c + 1, BurstFade(c + 1));
			return;
		}
		m.f1C = lerp16(r.node.f1C, s1, num, den);
		const int32_t fade = (c >= 6) == (c + 1 >= 6) ? lerp_i(BurstFade(c), BurstFade(c + 1), num, den) : BurstFade(c);
		BurstDraw(&m, c, fade);
	}

	static void HeldParticles(const Rec &r, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 0x24) = 8;
		for (int i = r.first; i < r.first + r.count; i++)
		{
			const PartRec &p = g_parts[i];
			const bool moves = p.alive && !r.paused;
			int16_t pos[4];
			for (int c = 0; c < 3; c++) pos[c] = moves ? lerp16(p.drawn.pos[c], p.next.pos[c], num, den) : p.drawn.pos[c];
			pos[3] = p.drawn.pos[3];
			ParticleDraw(h, moves && num >= den ? p.next.frame : p.drawn.frame, pos, p.drawn.angle);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		// packet bytes a draw does not write (pad halves) as the real tick left them; at fraction 1
		// as the next tick will find them (it draws into the other arena)
		HeldScope scope(Cursor(), g_held_arena, ARENA, num >= den ? 0 : g_packets);
		if (num >= den && g_packets) memcpy(g_held_arena, (const void *)(g_packets == ARENA_A ? ARENA_B : ARENA_A), ARENA);
		// the thread's next state first (it uses the scratch stack; the scope puts it back)
		g_pred.valid = false;
		bool thread = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].fn == ORIG_ThreadTask) { PredictThread(g_recs[i], g_pred); thread = true; }
		const int16_t anchor_next = thread && g_pred.valid ? g_pred.anchor[1] : Anchor()[1];
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_ThreadTask: HeldThread(r, num, den); break;
			case ORIG_WebTask: HeldWeb(r, anchor_next, num, den); break;
			case ORIG_BurstTask: HeldBurst(r, num, den); break;
			case ORIG_Particles1Task:
			case ORIG_Particles2Task: HeldParticles(r, num, den); break;
			}
		}
	}
}

	static void register_mag153_held()
	{
		mark_held(web153::ORIG_ThreadTask);
		mark_held(web153::ORIG_WebTask);
		mark_held(web153::ORIG_Particles1Task);
		mark_held(web153::ORIG_Particles2Task);
		mark_held(web153::ORIG_BurstTask);
		register_module_held(153, web153::HeldReady, web153::HeldFrame);
	}
}
