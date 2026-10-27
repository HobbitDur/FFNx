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

// 30 fps part of mag110_biggs_wedge_death.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag110_biggs_wedge_death.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// A real tick records every draw in order (the stream, every particle, debris and ring): the node
// as drawn, the GTE state and the Field_Alloc scratch under its header (the draw code leaves some
// header fields unset), and the stream's aim point. A held frame replays the log in the same order,
// each draw at the exact in-between state:
//   stream: wobble angles A / B half a step on, the aim point half way between this tick's and the
//     next tick's (the sweep's own next step from the stream node the tick left, the target
//     positions the next tick reads: the actor tasks run after the stream, so they are the
//     positions now), the source (aim x, -4000, -4000), distance and orientation following from
//     it, the grow / fade ramp half way, the texture scroll half a step on. The wobbled vertices go
//     to a private copy of the model's vertex table (the game rewrites 0x12E0658 every tick). The
//     stream's last draw (counter 0x45) holds.
//   sparks / hits: position + velocity / 2; debris: half way to its own next state (fall,
//     bounce) with the scale half way, turned half way between the two ticks' rotations (shortest
//     way); rings: growth half way on the in-between stream (sway phase B, distance, orientation,
//     source). Flipbook frames hold; a particle / debris / ring whose task ends after this draw
//     holds its last picture.
// Spawns (random) stay at 15 Hz. Not interpolated (the module writes them, the engine draws them):
// the targets' positions / rotation (blown away by the actor tasks: the entity-position held API is
// not integrated, the engine draws Biggs and Wedge at their real-tick positions), their flags.
// No camera of its own. No Biggs / Wedge task has a draw-only mode.

namespace ff8fx
{
namespace bw110
{
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const int MAXREC = 256;

	struct Rec
	{
		int kind;
		uint8_t node[0x64];        // the node as drawn
		uint8_t hdr[0xB4];         // scratch under the draw header when it was allocated
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const StreamNode *live;    // stream: the live node (its state after the tick)
		int16_t aim[4];            // stream: the tick's aim point
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_stream = -1;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAXREC];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_verts[STREAM_NVERTS * 8];
	static StreamNode g_held_sn;            // in-between copy of the stream node
	// the in-between stream of this held frame (rings follow it)
	static bool g_mid_valid = false;
	static int32_t g_mid_dist = 0;
	static Mat4x3 g_mid_basis;
	static int16_t g_mid_mouth[3];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
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

	static void held_note_stream(const StreamNode *s)
	{
		Rec *r = held_add(K_STREAM, s, 0x64);
		g_stream = r ? g_nrec - 1 : -1;
		if (r)
		{
			r->live = s;
			g_hdr_slot = r->hdr;
		}
	}

	static void held_note_aim(const StreamNode *, const int16_t *aim)
	{
		if (g_stream < 0 || g_log_tick != g_real_tick) return;
		memcpy(g_recs[g_stream].aim, aim, 6);
		g_recs[g_stream].aim[3] = 0;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick's aim point from the stream node the tick left (counter, sweep, kept position)
	// and the target positions now (the next tick's stream runs before its actor tasks); false when
	// the next tick draws no stream
	static bool NextAim(const StreamNode *s, int16_t *aim)
	{
		const uint32_t c = (uint32_t)(int32_t)s->counter;
		if (c >= 0x46) return false;
		int16_t t = s->t, cur = s->cur, next = s->next;
		int16_t kept[4];
		memcpy(kept, s->kept, 8);
		if (c >= 0xF)
		{
			t = (int16_t)(t + s->dt);
			if (t >= 0x1000)
			{
				t = (int16_t)(t + 0xF000);
				if (cur != next) memcpy(kept, s->ent[next] + 0x1C, 8);
				cur = next;
				next = (int16_t)(cur + 1);
				if (next >= s->count) next = (int16_t)(s->count - 1);
			}
		}
		if (cur == 1) StreamAimKept((const int16_t *)(s->ent[next] + 0x1C), kept, aim);
		else StreamAim((const int16_t *)(s->ent[cur] + 0x1C), (const int16_t *)(s->ent[next] + 0x1C), t, aim);
		return true;
	}

	static void HeldStream(const Rec &r, int num, int den)
	{
		StreamNode &c = g_held_sn;
		memcpy(&c, r.node, 0x64);
		const uint32_t cc = (uint32_t)(int32_t)c.counter;
		int16_t a1[3], aim[3];
		memcpy(a1, r.aim, 6);
		const bool moves = num != 0 && NextAim(r.live, a1);
		const int n = moves ? num : 0;
		for (int k = 0; k < 3; k++) aim[k] = (int16_t)lerp_i(r.aim[k], a1[k], n, den);
		Ramp rm = StreamRamp(cc);
		const Ramp rn = StreamRamp(cc + 1);
		rm.s = lerp_i(rm.s, rn.s, n, den);
		rm.h8 = lerp_i(rm.h8, rn.h8, n, den);
		// steady -> fading out: the fade starts on the way (header +4 = 0 like the next tick's)
		if (rm.kind == 2 && rn.kind == 1 && n > 0) rm.kind = 1;
		// texture scroll: 0x10 per tick, wrapping in the 0x80 texel window the lists wrap v in
		rm.scroll += 0x10 * n / den;
		if (rm.scroll >= 0x80) rm.scroll -= 0x80;
		g_hdr_slot = (uint8_t *)r.hdr;
		uint8_t *h = AllocHeader(0x5C);
		// the wobble at the in-between angles, into a private copy of the vertex table
		memcpy(g_held_verts, (const void *)STREAM_VERTS, sizeof(g_held_verts));
		StreamWobble(lerp16(c.a, (int16_t)(c.a - 100), n, den), lerp16(c.b, (int16_t)(c.b + 100), n, den), g_held_verts);
		Mat4x3 m1;
		StreamDraw(h, &c, cc, &rm, aim, &m1, (uint32_t)g_held_verts);
		g_mid_valid = true;
		g_mid_dist = c.dist;
		g_mid_basis = c.basis;
		memcpy(g_mid_mouth, c.mouth, 6);
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

	static void HeldSprite(const Rec &r, int num, int den)
	{
		SpriteNode c;
		memcpy(&c, r.node, sizeof(c));
		const int16_t frames = r.kind == K_SPARK ? 0xD : 8;
		if (c.counter + 1 < frames)
			for (int k = 0; k < 3; k++) c.pos[k] = lerp16(c.pos[k], (int16_t)(c.pos[k] + c.vel[k]), num, den);
		g_hdr_slot = (uint8_t *)r.hdr;
		SpriteDraw(&c, r.kind == K_SPARK ? SEQ_Spark : SEQ_Hit);
	}

	static void HeldDebris(const Rec &r, int num, int den)
	{
		DebrisNode c, nx;
		memcpy(&c, r.node, sizeof(c));
		nx = c;
		const bool lives = DebrisMove(&nx) == 0;
		const int n = lives ? num : 0;
		const bool scaled = c.counter >= 0x16;
		const int32_t s = scaled ? lerp_i(DebrisScale(c.counter), DebrisScale((int16_t)(c.counter + 1)), n, den) : 0;
		for (int k = 0; k < 3; k++) c.pos[k] = lerp16(c.pos[k], nx.pos[k], n, den);
		if (n == 0 || n == den)
		{
			// the tick's own angles
			if (n == den) memcpy(c.ang, nx.ang, sizeof(c.ang));
			g_hdr_slot = (uint8_t *)r.hdr;
			DebrisDraw(&c, scaled, s);
			return;
		}
		// rotation half way (matrices of both ticks' angles: pure math)
		Mat4x3 ra = {}, rb = {}, rmid;
		BuildRotationMatrixFromAngles(c.ang, &ra);
		BuildRotationMatrixFromAngles(nx.ang, &rb);
		RotationBetween(ra, rb, n, den, rmid);
		g_hdr_slot = (uint8_t *)r.hdr;
		HeldDebrisDraw(&c, rmid, scaled, s);
	}

	static void HeldRing(const Rec &r, int num, int den)
	{
		RingNode c;
		memcpy(&c, r.node, sizeof(c));
		StreamNode &sn = g_held_sn;
		memcpy(&sn, c.sn, 0x64);
		if ((int16_t)(c.e + 0x80) < 0x1000)
		{
			c.e = lerp16(c.e, (int16_t)(c.e + 0x80), num, den);
			sn.b = lerp16(sn.b, (int16_t)(sn.b + 100), num, den);
			if (g_mid_valid)
			{
				sn.dist = g_mid_dist;
				sn.basis = g_mid_basis;
				memcpy(sn.mouth, g_mid_mouth, 6);
			}
		}
		g_hdr_slot = (uint8_t *)r.hdr;
		RingDraw(&c, &sn);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		g_mid_valid = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			switch (r.kind)
			{
			case K_STREAM: HeldStream(r, num, den); break;
			case K_SPARK:
			case K_HIT: HeldSprite(r, num, den); break;
			case K_DEBRIS: HeldDebris(r, num, den); break;
			case K_RING: HeldRing(r, num, den); break;
			}
		}
	}
}

	static void register_mag110_held()
	{
		mark_held(bw110::ORIG_StreamTask);
		mark_held(bw110::ORIG_HitTask);
		mark_held(bw110::ORIG_DebrisTask);
		mark_held(bw110::ORIG_RingTask);
		mark_held(bw110::ORIG_SparkTask);
		register_module_held(110, bw110::HeldReady, bw110::HeldFrame);
	}
}
