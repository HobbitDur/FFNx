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

// 30 fps part of mag011_storm_breath.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag011_storm_breath.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in order (the stream, the layout play, every particle): the node
// as drawn, the GTE state and the Field_Alloc scratch under its header (the draw code leaves some
// header fields unset). A held frame replays the log in the same order, each draw at the exact
// in-between state:
//   stream: wobble angles A / B half a step on, the aim point half way between this tick's and the
//     next tick's (the sweep's own step, next target included), the grow / fade ramp half way;
//     distance and orientation follow from the aim point; the mouth and the target anchors hold
//     (engine-driven poses); the texture scroll moves half a step. The wobbled vertices go to a
//     private copy of the model's vertex table (the game rewrites 0x1365728 every tick).
//   layouts: prim::play_held (every object's in-between record); the mouth layout's frame holds
//     (mouth + caster facing), the cone's orientation is the in-between stream's.
//   sparks / mist / hits: position + velocity / 2; puffs: blend weight half way; debris: half way
//     to its own next state (fall, bounce) with the scale half way, turned half way between the two
//     ticks' rotations (shortest way, not the Euler angles half way); rings: growth half way on
//     the in-between stream (sway phase B, distance, orientation). Flipbook frames hold.
// Spawns (random) stay at 15 Hz. Blended vertex frames go to a private buffer.
// No Storm Breath task has a draw-only mode.

namespace ff8fx
{
namespace storm011
{
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const int MAXREC = 320;
	static const int SCRATCH = 0x400;

	struct Rec
	{
		int kind;
		uint8_t node[0x64];        // the node as drawn (target node: up to its layout)
		uint8_t hdr[0xB4];         // scratch under the draw header when it was allocated
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		int16_t p0[4], p1[4];      // stream: the target anchors of the tick
		Mat4x3 m;                  // layout: its parameter block's matrix
		uint32_t flags;            // layout: draw flag table
		prim::Layout *layout;      // layout: the live layout (prim::play_held knows what it drew)
		int scratch;               // layout: index of its Field_Alloc scratch copy
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nscratch = 0, g_stream = -1;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAXREC];
	static uint8_t g_scratch[4][SCRATCH];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_morph[0x800];     // blended vertex frames (64-vertex objects: 0x200)
	static uint8_t g_held_verts[STREAM_NVERTS * 8];
	static TargetNode g_held_tn;            // in-between copy of the target node (first 0x64 bytes)
	// the in-between stream of this held frame (rings and the cone follow it)
	static bool g_mid_valid = false;
	static int32_t g_mid_dist = 0;
	static Mat4x3 g_mid_basis;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
		g_stream = -1;
		g_packets = 0;
	}

	static Rec *held_add(int kind, const void *node, int size)
	{
		g_hdr_slot = nullptr;
		if (g_log_tick != g_real_tick || g_nrec >= MAXREC) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.node, node, size);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		return &r;
	}

	// a task is about to draw: its node, its header scratch goes to the record
	static void held_note(int kind, const void *node, int size)
	{
		Rec *r = held_add(kind, node, size);
		if (r) g_hdr_slot = r->hdr;
	}

	static void held_note_stream(const TargetNode *t)
	{
		Rec *r = held_add(K_STREAM, t, 0x64);
		g_stream = r ? g_nrec - 1 : -1;
		if (r) g_hdr_slot = r->hdr;
	}

	static void held_note_aim(const int16_t *p0, const int16_t *p1)
	{
		if (g_stream < 0 || g_log_tick != g_real_tick) return;
		memcpy(g_recs[g_stream].p0, p0, 6);
		memcpy(g_recs[g_stream].p1, p1, 6);
		g_recs[g_stream].p0[3] = g_recs[g_stream].p1[3] = 0;
	}

	static void held_note_play(int kind, const TargetNode *t, const PrimArg *arg)
	{
		if (g_nscratch >= 4) { g_hdr_slot = nullptr; return; }
		Rec *r = held_add(kind, t, 0x64);
		if (!r) return;
		r->m = *arg->m;
		r->flags = arg->flags;
		r->layout = (prim::Layout *)t->layout;
		r->scratch = g_nscratch++;
		memcpy(g_scratch[r->scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a target anchor (anchor 0xF1) the real tick did not fetch: the engine call has no other effect
	// than the GTE, the Field_Alloc scratch and the word 0x1D9770C, all put back
	static void HeldAnchor(uint8_t *entity, int16_t *out, const Rec &r)
	{
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		uint8_t save[0x100];
		memcpy(save, scratch, sizeof(save));
		const int16_t w = var<int16_t>(0x1D9770C);
		GetEffectSpawnPosition(entity, 0xF1, 0, out);
		var<int16_t>(0x1D9770C) = w;
		memcpy(scratch, save, sizeof(save));
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static void HeldStream(const Rec &r, int num, int den)
	{
		TargetNode &c = g_held_tn;
		memcpy(&c, r.node, 0x64);
		const uint32_t phase = (uint32_t)((int32_t)c.counter - 0x19);
		// the aim point now and after the sweep's next step
		int16_t a0[3], a1[3], aim[3];
		StreamAim(r.p0, r.p1, c.t, a0);
		memcpy(a1, a0, sizeof(a1));
		if (num != 0)
		{
			int16_t t1 = c.t, cur1 = c.cur, next1 = c.next;
			if (phase >= 5)
			{
				t1 = (int16_t)(t1 + c.dt);
				if (t1 >= 0x1000)
				{
					t1 = (int16_t)(t1 + 0xF000);
					cur1 = c.next;
					next1 = (int16_t)(cur1 + 1);
					if (next1 >= c.count) next1 = (int16_t)(c.count - 1);
				}
			}
			int16_t q0[4] = {}, q1[4] = {};
			const int16_t idx[2] = { cur1, next1 };
			int16_t *q[2] = { q0, q1 };
			for (int k = 0; k < 2; k++)
			{
				if (idx[k] == c.cur) memcpy(q[k], r.p0, 8);
				else if (idx[k] == c.next) memcpy(q[k], r.p1, 8);
				else HeldAnchor(c.ent[idx[k]], q[k], r);
			}
			StreamAim(q0, q1, t1, a1);
		}
		for (int k = 0; k < 3; k++) aim[k] = (int16_t)lerp_i(a0[k], a1[k], num, den);
		Ramp rm = StreamRamp(phase);
		const Ramp rn = StreamRamp(phase + 1);
		rm.s = lerp_i(rm.s, rn.s, num, den);
		rm.h8 = lerp_i(rm.h8, rn.h8, num, den);
		// steady -> fading out: the fade starts on the way (header +4 = 0 like the next tick's)
		if (rm.kind == 2 && rn.kind == 1 && num > 0) rm.kind = 1;
		// texture scroll: 0x10 per tick, wrapping in the 0x80 texel window the lists wrap v in
		rm.scroll += 0x10 * num / den;
		if (rm.scroll >= 0x80) rm.scroll -= 0x80;
		g_hdr_slot = (uint8_t *)r.hdr;
		uint8_t *h = AllocHeader(0x5C);
		// the wobble at the in-between angles, into a private copy of the vertex table
		memcpy(g_held_verts, (const void *)STREAM_VERTS, sizeof(g_held_verts));
		StreamWobble(lerp16(c.a, (int16_t)(c.a - 100), num, den), lerp16(c.b, (int16_t)(c.b + 100), num, den), g_held_verts);
		Mat4x3 m1;
		int32_t d[3];
		StreamDraw(h, phase, &rm, aim, c.mouth, &c.dist, &c.basis, &m1, d, (uint32_t)g_held_verts);
		g_mid_valid = true;
		g_mid_dist = c.dist;
		g_mid_basis = c.basis;
	}

	// the rotation half way from ra to rb along the shortest way (a debris spins up to 45 degrees
	// per axis and tick: the half-way Euler angles would not turn it half way): polar factor of
	// ra + (rb - ra) * num / den (Newton iteration X = (X + X^-T) / 2), exact rotation midpoint at 1/2
	static void RotationBetween(const Mat4x3 &ra, const Mat4x3 &rb, int num, int den, Mat4x3 &out)
	{
		const double f = (double)num / den;
		double x[3][3];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) x[i][j] = ra.m[i][j] + (rb.m[i][j] - ra.m[i][j]) * f;
		for (int it = 0; it < 12; it++)
		{
			const double det = x[0][0] * (x[1][1] * x[2][2] - x[1][2] * x[2][1]) - x[0][1] * (x[1][0] * x[2][2] - x[1][2] * x[2][0]) + x[0][2] * (x[1][0] * x[2][1] - x[1][1] * x[2][0]);
			if (det == 0) break;
			double c[3][3]; // cofactors / det = inverse transposed
			c[0][0] = (x[1][1] * x[2][2] - x[1][2] * x[2][1]) / det; c[0][1] = -(x[1][0] * x[2][2] - x[1][2] * x[2][0]) / det; c[0][2] = (x[1][0] * x[2][1] - x[1][1] * x[2][0]) / det;
			c[1][0] = -(x[0][1] * x[2][2] - x[0][2] * x[2][1]) / det; c[1][1] = (x[0][0] * x[2][2] - x[0][2] * x[2][0]) / det; c[1][2] = -(x[0][0] * x[2][1] - x[0][1] * x[2][0]) / det;
			c[2][0] = (x[0][1] * x[1][2] - x[0][2] * x[1][1]) / det; c[2][1] = -(x[0][0] * x[1][2] - x[0][2] * x[1][0]) / det; c[2][2] = (x[0][0] * x[1][1] - x[0][1] * x[1][0]) / det;
			// x is in 4.12: X^-T carries 1/4096^2 scale back to 4.12 by * 4096^2
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 3; j++) x[i][j] = (x[i][j] + c[i][j] * 4096.0 * 4096.0) / 2;
		}
		out = ra;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out.m[i][j] = (int16_t)floor(x[i][j] + 0.5);
	}

	// DebrisDraw with a given rotation (the in-between one)
	static void HeldDebrisDraw(const DebrisNode *d, const Mat4x3 &rot, bool scaled, int32_t s)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 m = rot;
		if (scaled)
		{
			const int32_t v[3] = { s, s, s };
			Scale3DMatrix(&m, v);
		}
		m.t[0] = d->pos[0];
		m.t[1] = d->pos[1];
		m.t[2] = d->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = MODEL_Debris;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		g_mid_valid = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			switch (r.kind)
			{
			case K_STREAM:
				HeldStream(r, num, den);
				break;
			case K_MOUTH:
			case K_CONE:
			{
				Mat4x3 m = r.m;
				if (r.kind == K_CONE && g_mid_valid)
				{
					const TargetNode *c = (const TargetNode *)r.node;
					m = g_mid_basis;
					m.t[0] = c->frozen[0];
					m.t[1] = c->frozen[1];
					m.t[2] = c->frozen[2];
					ComposeAffineTransform(&Camera(), &m, &m);
				}
				memcpy(scratch, g_scratch[r.scratch], SCRATCH);
				PrimArg arg;
				arg.m = &m;
				arg.flags = r.flags;
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				break;
			}
			case K_SPARK:
			case K_MIST:
			case K_HIT:
			{
				SpriteNode c;
				memcpy(&c, r.node, sizeof(c));
				for (int k = 0; k < 3; k++) c.pos[k] = lerp16(c.pos[k], (int16_t)(c.pos[k] + c.vel[k]), num, den);
				g_hdr_slot = (uint8_t *)r.hdr;
				SpriteDraw(&c, r.kind == K_SPARK ? SEQ_Spark : r.kind == K_MIST ? SEQ_Mist : SEQ_Hit);
				break;
			}
			case K_PUFF:
			{
				SpriteNode c;
				memcpy(&c, r.node, sizeof(c));
				const int32_t w = lerp_i(PuffWeight(c.counter), PuffWeight((int16_t)(c.counter + 1)), num, den);
				g_hdr_slot = (uint8_t *)r.hdr;
				PuffDraw(&c, w);
				break;
			}
			case K_DEBRIS:
			{
				DebrisNode c, nx;
				memcpy(&c, r.node, sizeof(c));
				nx = c;
				DebrisMove(&nx);
				const bool scaled = c.counter >= 0x16;
				const int32_t s = scaled ? lerp_i(DebrisScale(c.counter), DebrisScale((int16_t)(c.counter + 1)), num, den) : 0;
				for (int k = 0; k < 3; k++) c.pos[k] = lerp16(c.pos[k], nx.pos[k], num, den);
				if (num == 0 || num == den)
				{
					// the tick's own angles
					if (num == den) memcpy(c.ang, nx.ang, sizeof(c.ang));
					g_hdr_slot = (uint8_t *)r.hdr;
					DebrisDraw(&c, scaled, s);
					break;
				}
				// rotation half way (matrices of both ticks' angles: pure math)
				Mat4x3 ra = {}, rb = {}, rm;
				BuildRotationMatrixFromAngles(c.ang, &ra);
				BuildRotationMatrixFromAngles(nx.ang, &rb);
				RotationBetween(ra, rb, num, den, rm);
				g_hdr_slot = (uint8_t *)r.hdr;
				HeldDebrisDraw(&c, rm, scaled, s);
				break;
			}
			case K_RING:
			{
				RingNode c;
				memcpy(&c, r.node, sizeof(c));
				c.e = lerp16(c.e, (int16_t)(c.e + 0x80), num, den);
				TargetNode &tn = g_held_tn;
				memcpy(&tn, c.tn, 0x64);
				tn.b = lerp16(tn.b, (int16_t)(tn.b + 100), num, den);
				if (g_mid_valid)
				{
					tn.dist = g_mid_dist;
					tn.basis = g_mid_basis;
				}
				g_hdr_slot = (uint8_t *)r.hdr;
				RingDraw(&c, &tn);
				break;
			}
			}
		}
	}
}

	static void register_mag011_held()
	{
		mark_held(storm011::ORIG_TargetTask);
		mark_held(storm011::ORIG_DebrisTask);
		mark_held(storm011::ORIG_RingTask);
		mark_held(storm011::ORIG_SparkTask);
		mark_held(storm011::ORIG_PuffTask);
		mark_held(storm011::ORIG_MistTask);
		mark_held(storm011::ORIG_HitTask);
		register_module_held(11, storm011::HeldReady, storm011::HeldFrame);
	}
}
