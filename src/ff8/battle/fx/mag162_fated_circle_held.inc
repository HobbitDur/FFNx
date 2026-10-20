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

// 30 fps layer of Fated Circle (effect 162), included at the end of mag162_fated_circle.cpp.
//
// Every drawing task of the main queue draws THEN updates (the particle tasks draw and update each particle, then spawn).
// A real tick records its draws in order (the master's seam starts the log): the node as drawn, the GTE state the draw
// started with, the Field_Alloc scratch under its draw header, and for the particle tasks the pool as drawn (the task's
// entry). A held frame replays the log into a private packet buffer, each draw at its exact in-between state (logic
// stays 15 Hz):
//   Trail - Renzokuken's (fx160, as Rough Divide's): the next tick's ring entry from the weapon's bone 1 at the NEXT pose
//     (pose_midpoint at 1 of the weapon model, the real pose rebuilt afterwards) through the caster's matrix of the next
//     tick (moved by the frame gate's in-between position when the caster moves; held otherwise), the newest entries
//     splined on private buffers, each spline point half way. Holds the real tick's splines when the trail ends on this
//     tick, when the tick was draw-only, or when the next tick would draw no trail.
//   Swipe - its yaw half way, the fade half way from counter 3 (the mode switch: held); shock / glow / dome - the scale
//     half way, the fade half way after the mode switch (4 / 0x1C / 0xE); shell - the scale half way (until 8), the fade
//     half way inside the fade-in (0..7) and the fade-out (0xC..), the morph angle half way (8..), its vertices on a
//     private buffer; circle - the scale, the v scroll and the fade (0x20..) half way, the morph half way between its two
//     steps (0x10..) on a private buffer; pillars - height, z, the x / y angles and the z angle (counter * 2) half way;
//     rings - the fade half way.
//   Particles (pools A / B) - each task's pool as drawn: the particles that advanced on the tick (same type, life + 1)
//     half way to their post-update position / scale / angles / radius / velocity / fade (the spark clouds, one task per
//     target, share their pool: each spark task updates every spark, so a spark's next draw by a task is after one update
//     per spark task run in between: its scale half way to that); flipbook frames (sprite lives)
//     switch on the real frame; a particle that died on the tick holds its last picture; the tick's spawns appear on the
//     next real tick (as in the original). The swirl's sprites do not move (their life counts down: flipbook); its
//     scale vector's third word is the one its real draw used.
// The director's stage capture (tick 0x47: the stage drawn off-screen into the texture area) is not redrawn on held frames
// (no held task claims it; its texture stays, as at 15 fps); the master, the restore and the director draw nothing else;
// the caster's animation and the screen flash / fade are the engine's.
// Nothing here writes game state: the gate puts back the GTE registers and the scratch; the bone matrices are rebuilt
// from the real pose after each look-ahead.

namespace ff8fx
{
namespace circle162
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_packets = 0;        // the module's packet cursor when the tick started drawing
	static uint8_t g_held_packets[0x20000];

	enum RecKind { REC_TRAIL, REC_NODE, REC_POOL };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xCC];         // scratch under its draw header when it was allocated
		Mat4x3 mat;                // the caster's matrix
		uint32_t pool, pool_end;   // particles: the pool
		uint32_t snap;             // particles: offset of the pool as drawn in g_snap
		uint32_t residue;          // swirl: its scale vector's third word
	};
	static const int REC_MAX = 64;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint8_t g_snap[0x10000];
	static uint32_t g_nsnap = 0;
	// trail: the splines of the real tick, the caster's matrix it was sampled with
	static uint8_t g_trail_sa[0x100], g_trail_sb[0x100];
	static Mat4x3 g_trail_mat;

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_packets = PacketCursor();
		g_nrec = 0;
		g_nsnap = 0;
	}

	static HRec *NoteRec(int kind, uint32_t fn, const Node24 *n, bool header)
	{
		magc::g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		HRec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.fn = fn;
		r.node = n;
		memcpy(&r.copy, n, sizeof(Node24));
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		r.mat = *(const Mat4x3 *)(Caster() + 0x40);
		r.residue = 0;
		if (header) magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_trail(const Node24 *n)
	{
		if (!NoteRec(REC_TRAIL, ORIG_Trail, n, false)) return;
		memcpy(g_trail_sa, (const void *)SPL_A, sizeof(g_trail_sa));
		memcpy(g_trail_sb, (const void *)SPL_B, sizeof(g_trail_sb));
		g_trail_mat = *(const Mat4x3 *)(Caster() + 0x40);
	}

	static void held_note_node(uint32_t fn, const Node24 *n) { NoteRec(REC_NODE, fn, n, true); }

	static HRec *NotePool(uint32_t fn, const Node24 *n)
	{
		uint32_t pool = POOL_B, end = POOL_B_END;
		if (fn == ORIG_Orbit || fn == ORIG_Ember) { pool = POOL_A; end = POOL_A_END; }
		if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return nullptr; }
		HRec *r = NoteRec(REC_POOL, fn, n, true);
		if (!r) return nullptr;
		r->pool = pool;
		r->pool_end = end;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
		g_nsnap += end - pool;
		return r;
	}

	static void held_note_pool(uint32_t fn, const Node24 *n) { NotePool(fn, n); }

	static void held_note_swirl(const Node24 *n, uint32_t residue)
	{
		if (HRec *r = NotePool(ORIG_Swirl, n)) r->residue = residue;
	}

	// a held draw starts like the real one: its GTE state and header scratch
	static void Replay(const HRec &r, bool header)
	{
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = header ? (uint8_t *)r.hdr : nullptr;
	}

	// the node still runs the same task on the next tick (it did not end on this one) and advanced
	static bool Alive(const HRec &r) { return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == r.fn && r.node->c == (int16_t)(r.copy.c + 1); }

	static inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return magc::lerp16(a, b, num, den); }

	// the caster's matrix translation of the next tick (the frame gate moved a moving caster to its in-between position:
	// tick N's + (now - tick N's) * den / num), put in place for a look-ahead; restored by the caller
	static void NextCasterMatrix(Mat4x3 *em, const Mat4x3 &at_tick, int num, int den)
	{
		const Mat4x3 now = *em;
		for (int k = 0; k < 3; k++) em->t[k] = at_tick.t[k] + (int32_t)((int64_t)(now.t[k] - at_tick.t[k]) * den / num);
	}

	// ---- trail ----
	static uint8_t g_ring[RING_END - RING], g_pa[0x38], g_pb[0x38];
	static uint8_t g_na[0x100], g_nb[0x100], g_ma[0x100], g_mb[0x100];

	// the next tick's splines (false: the next tick draws no trail)
	static bool TrailNext(const Node24 *n, int num, int den)
	{
		uint8_t *e = Caster();
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (!w) return false;
		memcpy(g_ring, (const void *)RING, sizeof(g_ring));
		TrailWork *tw = (TrailWork *)FieldAlloc(0x40);
		Mat4x3 *em = (Mat4x3 *)(e + 0x40);
		const Mat4x3 now = *em;
		NextCasterMatrix(em, g_trail_mat, num, den);
		pose_midpoint((void *)w, (void *)(w + 0xC), 1, 1);
		TrailPoints(tw);
		BuildBoneMatricesFromPose(w);
		*em = now;
		const int32_t c = n->c;
		const int32_t idx = c % 7;
		RingEntry *re = (RingEntry *)(g_ring + 0x14 * idx);
		re->alive = 1;
		re->base[0] = *(const uint32_t *)&tw->base[0];
		re->base[1] = *(const uint32_t *)&tw->base[2];
		re->tip[0] = *(const uint32_t *)&tw->tip[0];
		re->tip[1] = *(const uint32_t *)&tw->tip[2];
		FieldFree(0x40);
		const int32_t want = c >= Length() - 7 ? Length() - c : 7;
		const int32_t cnt = TrailGather(g_ring, idx, want, g_pa, g_pb);
		if (cnt <= 1) return false;
		TrailSplines(cnt, g_pa, g_pb, g_na, g_nb);
		return true;
	}

	static void TrailHeld(const HRec &r, int num, int den)
	{
		const uint8_t *sa = g_trail_sa, *sb = g_trail_sb;
		if (num != 0 && !g_tick_paused && Alive(r) && TrailNext(r.node, num, den))
		{
			const int16_t *a0 = (const int16_t *)g_trail_sa, *a1 = (const int16_t *)g_na;
			const int16_t *b0 = (const int16_t *)g_trail_sb, *b1 = (const int16_t *)g_nb;
			int16_t *ma = (int16_t *)g_ma, *mb = (int16_t *)g_mb;
			for (int k = 0; k < 0x80; k++)
			{
				ma[k] = Lerp16(a0[k], a1[k], num, den);
				mb[k] = Lerp16(b0[k], b1[k], num, den);
			}
			sa = g_ma;
			sb = g_mb;
		}
		Replay(r, false);
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailDraw(sa, sb, w);
		FieldFree(0x40);
	}

	// ---- single models ----
	// fade half way when the draw mode stays (both ticks at or after the switch), else tick N's
	static int32_t FadeLerp(int32_t c, int32_t from, int32_t f0, int32_t f1, bool next, int num, int den)
	{
		return next && c >= from ? lerp_i(f0, f1, num, den) : f0;
	}

	static int16_t g_shell_vtx[168 * 4];
	static int16_t g_circle_vtx[0x5D * 4], g_circle_a[0x5D * 4], g_circle_b[0x5D * 4];

	static void NodeHeld(const HRec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const Node24 *now = r.node;
		const int32_t c = d.c;
		Node24 m = d;
		Replay(r, true);
		switch (r.fn)
		{
		case ORIG_Swipe:
		{
			if (next) m.w18 = Lerp16(d.w18, now->w18, num, den);
			const int32_t fade = FadeLerp(c, 3, mul32(c - 3, 409), mul32(c + 1 - 3, 409), next, num, den);
			SwipeDraw(&m, m.w18, c, fade);
			return;
		}
		case ORIG_Shock:
		{
			if (next) m.w1C = Lerp16(d.w1C, now->w1C, num, den);
			const int32_t fade = FadeLerp(c, 4, shl32(c - 4, 9), shl32(c + 1 - 4, 9), next, num, den);
			ShockDraw(&m, m.w1C, c, fade);
			return;
		}
		case ORIG_Glow:
		{
			if (next) m.w1C = Lerp16(d.w1C, now->w1C, num, den);
			const int32_t fade = FadeLerp(c, 0x1C, shl32(c - 0x1C, 9), shl32(c + 1 - 0x1C, 9), next, num, den);
			GlowDraw(&m, m.w1C, c, fade);
			return;
		}
		case ORIG_Shell:
		{
			int32_t fade = ShellFade(c), angle = mul32(c - 8, 56);
			if (next)
			{
				m.w1C = Lerp16(d.w1C, now->w1C, num, den);
				if (c < 7 || c >= 0xC) fade = lerp_i(fade, ShellFade(c + 1), num, den);
				if (c >= 8) angle = lerp_i(angle, mul32(c + 1 - 8, 56), num, den);
			}
			ShellDraw(&m, m.w1C, c, fade, angle, g_shell_vtx);
			return;
		}
		case ORIG_Dome:
		{
			if (next) m.w1C = Lerp16(d.w1C, now->w1C, num, den);
			const int32_t fade = FadeLerp(c, 0xE, shl32(c - 0xE, 9), shl32(c + 1 - 0xE, 9), next, num, den);
			DomeDraw(&m, m.w1C, c, fade);
			return;
		}
		case ORIG_Circle:
		{
			int32_t vscroll = mul32(c, 10);
			const int16_t *vtx = nullptr;
			if (c >= 0x10)
			{
				CircleMorph(g_circle_a, (int16_t)(c - 0x10));
				vtx = g_circle_a;
			}
			if (next)
			{
				m.w1C = Lerp16(d.w1C, now->w1C, num, den);
				vscroll = lerp_i(vscroll, mul32(c + 1, 10), num, den);
				if (c >= 0x10)
				{
					CircleMorph(g_circle_b, (int16_t)(c + 1 - 0x10));
					for (int k = 0; k < 0x5D * 4; k++) g_circle_vtx[k] = Lerp16(g_circle_a[k], g_circle_b[k], num, den);
					vtx = g_circle_vtx;
				}
			}
			const int32_t fade = FadeLerp(c, 0x20, shl32(c - 0x20, 9), shl32(c + 1 - 0x20, 9), next, num, den);
			CircleDraw(&m, m.w1C, c, vscroll, fade, vtx);
			return;
		}
		case ORIG_Pillar:
		{
			int16_t ang2 = (int16_t)(c << 1);
			if (next)
			{
				m.w12 = Lerp16(d.w12, now->w12, num, den);
				m.w14 = Lerp16(d.w14, now->w14, num, den);
				m.w1C = Lerp16(d.w1C, now->w1C, num, den);
				m.w20 = Lerp16(d.w20, now->w20, num, den);
				ang2 = Lerp16(ang2, (int16_t)((c + 1) << 1), num, den);
			}
			PillarDraw(&m, ang2);
			return;
		}
		case ORIG_Ring:
		{
			const int32_t f0 = mul32(0x14 - c, 384), f1 = c + 1 >= 0xC ? mul32(0x14 - (c + 1), 384) : 0xC00;
			const int32_t fade = next && c >= 0xC ? lerp_i(f0, f1, num, den) : f0;
			RingDraw(&m, c, fade);
			return;
		}
		}
	}

	// ---- particles: the draws of the pool tasks from the pools as drawn ----
	// the particle advanced on the tick (still the task's type, life + 1): its post-update state is the next draw's
	static const Part *NextPart(const HRec &r, uint32_t o, uint32_t type, const Part *p, bool move)
	{
		if (!move) return nullptr;
		const Part *now = (const Part *)(r.pool + o);
		return (now->flags & type) && now->life == (int16_t)(p->life + 1) ? now : nullptr;
	}

	// the spark clouds (one task per target) share pool B type 1: every spark task draws and updates every spark, so a
	// spark ages once per live spark task a tick. Record i's sparks: g_spark_after[i] updates after its draw on the real
	// tick (its own and the later spark tasks'), g_spark_upd[i] updates until its next draw (+ the earlier spark tasks that
	// run on the next tick); 0 = the task does not draw on the next tick
	static int g_spark_after[REC_MAX], g_spark_upd[REC_MAX];
	static void SparkCounts()
	{
		int k = 0;
		for (int i = 0; i < g_nrec; i++) if (g_rec[i].kind == REC_POOL && g_rec[i].fn == ORIG_Spark) k++;
		for (int i = 0, j = 0, alive_before = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			if (r.kind != REC_POOL || r.fn != ORIG_Spark) continue;
			g_spark_after[i] = k - j;
			g_spark_upd[i] = Alive(r) ? k - j + alive_before : 0;
			if (Alive(r)) alive_before++;
			j++;
		}
	}

	// a spark's scale after n updates (scale -= scale >> 5 each)
	static int16_t SparkScale(int16_t s, int n)
	{
		for (; n > 0; n--) s = (int16_t)(s - (int16_t)(s >> 5));
		return s;
	}

	static void PoolHeld(const HRec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		const uint8_t *snap = g_snap + r.snap;
		const uint32_t size = r.pool_end - r.pool;
		const Node24 *n = &r.copy;
		Replay(r, true);
		switch (r.fn)
		{
		case ORIG_Dust:
		case ORIG_Ember:
		case ORIG_Spark:
		{
			uint8_t *h = AllocHeader(0xB4);
			*(uint32_t *)h = r.fn == ORIG_Dust ? SEQ_DUST : r.fn == ORIG_Ember ? SEQ_EMBER : SEQ_SPARK;
			*(int16_t *)(h + 0x24) = r.fn == ORIG_Spark ? 8 : 0;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 1)) continue;
				int16_t pos[4] = { p->pos[0], p->pos[1], p->pos[2], p->w0E };
				int16_t scale = p->scale;
				if (r.fn == ORIG_Spark)
				{
					// (still a spark after the tick's remaining updates; its next draw after g_spark_upd updates)
					const int i = (int)(&r - g_rec);
					const Part *nx = (const Part *)(r.pool + o);
					if (move && g_spark_upd[i] && (nx->flags & 1) && nx->life == (int16_t)(p->life + g_spark_after[i]))
						scale = Lerp16(p->scale, SparkScale(p->scale, g_spark_upd[i]), num, den);
				}
				else if (const Part *nx = NextPart(r, o, 1, p, move))
					for (int k = 0; k < 3; k++) pos[k] = Lerp16(p->pos[k], nx->pos[k], num, den);
				ShadowSprite(h, pos, scale, p->life);
			}
			FieldFree(0xB4);
			return;
		}
		case ORIG_Orbit:
		{
			uint8_t *h = AllocHeader(0xB4);
			*(uint32_t *)h = SEQ_ORBIT;
			*(int16_t *)(h + 0x24) = 0;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 1)) continue;
				int16_t angle = p->w0E, radius = p->v[0];
				if (const Part *nx = NextPart(r, o, 1, p, move))
				{
					angle = Lerp16(p->w0E, nx->w0E, num, den);
					radius = Lerp16(p->v[0], nx->v[0], num, den);
				}
				OrbitSprite(h, n, p, angle, radius);
			}
			FieldFree(0xB4);
			return;
		}
		case ORIG_Swirl:
		{
			uint8_t *h = AllocHeader(0xB4);
			*(uint32_t *)h = SEQ_SWIRL;
			*(int16_t *)(h + 0x24) = 8;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 2)) continue;
				SwirlSprite(h, p, p->life, (int32_t)r.residue);
			}
			FieldFree(0xB4);
			return;
		}
		case ORIG_Shard:
		{
			uint8_t *h = AllocHeader(0x58);
			*(uint32_t *)h = MODEL_SHARD;
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 3;
			ShardWork w;
			w.ref[0] = 0;
			w.ref[1] = -0x1000;
			w.ref[2] = 0;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 2)) continue;
				Part q = *p;
				int16_t scale = p->scale;
				int32_t fade = shl32(p->life - 4, 0xA);
				if (const Part *nx = NextPart(r, o, 2, p, move))
				{
					for (int k = 0; k < 3; k++) q.pos[k] = Lerp16(p->pos[k], nx->pos[k], num, den);
					scale = Lerp16(p->scale, nx->scale, num, den);
					if (p->life >= 4) fade = lerp_i(fade, shl32(nx->life - 4, 0xA), num, den);
				}
				ShardDraw(&w, h, &q, scale, fade);
			}
			FieldFree(0x58);
			return;
		}
		case ORIG_Burst:
		{
			uint8_t *h = AllocHeader(0x58);
			*(uint32_t *)h = MODEL_BURST;
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0x33;
			BurstWork w;
			w.ref[0] = 0;
			w.ref[1] = -0x1000;
			w.ref[2] = 0;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 4)) continue;
				int16_t spin = p->w0E, scale = p->scale;
				int32_t fade = shl32(p->life - 4, 0xA);
				if (const Part *nx = NextPart(r, o, 4, p, move))
				{
					spin = Lerp16(p->w0E, nx->w0E, num, den);
					scale = Lerp16(p->scale, nx->scale, num, den);
					if (p->life >= 4) fade = lerp_i(fade, shl32(nx->life - 4, 0xA), num, den);
				}
				BurstDraw(&w, h, n, p, spin, scale, fade);
			}
			FieldFree(0x58);
			return;
		}
		case ORIG_Rain:
		{
			uint8_t *h = AllocHeader(0x58);
			RainWork w;
			*(uint32_t *)h = MODEL_RAIN;
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0x33;
			w.sv[2] = 0x580;
			w.sv[1] = 0x580;
			w.ref[0] = -0x1000;
			w.ref[1] = 0;
			w.ref[2] = 0;
			RainLook(&w);
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const Part *p = (const Part *)(snap + o);
				if (!(p->flags & 8)) continue;
				Part q = *p;
				int32_t fade = (int32_t)shl32(p->life, 0xB) - 0x800;
				if (const Part *nx = NextPart(r, o, 8, p, move))
				{
					for (int k = 0; k < 3; k++)
					{
						q.pos[k] = Lerp16(p->pos[k], nx->pos[k], num, den);
						q.v[k] = Lerp16(p->v[k], nx->v[k], num, den);
					}
					if (p->life >= 1) fade = lerp_i(fade, (int32_t)shl32(nx->life, 0xB) - 0x800, num, den);
				}
				RainDraw(&w, h, &q, q.pos, fade);
			}
			FieldFree(0x58);
			return;
		}
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		magc::HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		SparkCounts();
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			switch (r.kind)
			{
			case REC_TRAIL: TrailHeld(r, num, den); break;
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_POOL: PoolHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag162_held()
	{
		mark_held(circle162::ORIG_Master);
		mark_held(circle162::ORIG_Trail);
		mark_held(circle162::ORIG_Swipe);
		mark_held(circle162::ORIG_Dust);
		mark_held(circle162::ORIG_Shock);
		mark_held(circle162::ORIG_Glow);
		mark_held(circle162::ORIG_Shard);
		mark_held(circle162::ORIG_Orbit);
		mark_held(circle162::ORIG_Burst);
		mark_held(circle162::ORIG_Rain);
		mark_held(circle162::ORIG_Shell);
		mark_held(circle162::ORIG_Dome);
		mark_held(circle162::ORIG_Circle);
		mark_held(circle162::ORIG_Pillar);
		mark_held(circle162::ORIG_Ring);
		mark_held(circle162::ORIG_Ember);
		mark_held(circle162::ORIG_Spark);
		mark_held(circle162::ORIG_Swirl);
		register_module_held(162, circle162::HeldReady, circle162::HeldFrame);
	}
}
