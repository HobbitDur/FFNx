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

// 30 fps layer of No Mercy (effect 200), included at the end of mag200_no_mercy.cpp.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw header, the tip point it
// was drawn at (TipShards / Slash), and for the particle tasks the pool as drawn (after the tick's spawns, before the
// update) and their header as set up. A held frame replays the log into private packet buffers (module cursor
// 0x24FBD64 and frame arena 0x1D8E054, each holding the real tick's packets under the replay), each draw at its exact
// in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every draw composes
// with it, as the real draws do with theirs):
//   Trail - Lion Heart's: the next tick's ring entry from the weapon's bone 1 at the NEXT pose (pose_midpoint at 1 of
//     the weapon model, the real pose rebuilt afterwards) through the caster's matrix of the next tick (the frame gate
//     moves a moving caster to its in-between position on held frames: its next translation is tick N's + (now - tick
//     N's) * den / num), the newest min(6, 0x99 - counter) entries splined by the module's own float spline on private
//     buffers, each spline point half way between the real tick's and the next tick's. Holds the real tick's splines
//     when the trail ends on this tick, when the tick was draw-only, or when the next tick would draw no trail. The next
//     splines also give the next tick's blade (first point / unit direction): the tip point the first Slash moves.
//   Flame - Fire's: flipbook at a fixed position, ground glow from its fixed height: holds.
//   Spark / Ember - Fire's: position half way along the velocity, size half way to the next shrink step; the flipbook
//     frame holds.
//   Single models (CasterRing, TipFlash, Slash, FrameModel, WaveRing, ImpactFlash, TargetRing) - angles, scales, the v
//     scroll (CasterRing) half way to the values the update left (the next draw's); fades half way while the draw mode
//     stays (a mode switch on the next tick: held). FrameModel's camera x frame is rebuilt from the live camera. A
//     WaveRing's position is half way between its spline point and the point it steps to. The Slashes' tip point is
//     half way to the one the next tick draws them at: the director's next write (0x48: the blade point, 0x78: the
//     shock waves' point), then the first Slash's own blade point at its ticks 0x1E..0x23 (from the next splines).
//   Particles - each task's pool as drawn: the entries that advanced on the tick (same type, life + 1) half way to
//     their post-update roll (TipShards), length (WaveTrail) or position (TargetBurst); fades half way while the draw
//     mode stays; flipbook frames (sprite lives) and TipShards' colour index switch on the real frame; an entry that
//     died on the tick holds its last picture; the tick's spawns are drawn at birth (they were drawn on the real tick).
//     TipShards are drawn at the tip point half way to the one the next tick draws them at. The static sparks
//     (SparkBurst, TargetSparks) only follow the live camera.
// A task that ends on the tick holds its last picture; nothing moves after a draw-only tick (everything holds).
// Camera: the shared camera-script task 0x63E9C0 (camscript_held_camera, Doomtrain's port; the Field_Alloc scratch put
// back as 164's). The director, the flame directors and the texture restore draw nothing; Seifer's animation, the
// targets' sequences, the screen flash / fade are the engine's. Nothing here writes game state: the scope puts back the
// cursors, the GTE registers, the Field_Alloc scratch and the shadow-rotation scratch; the weapon's bone matrices are
// rebuilt from the real pose after the look-ahead.

namespace ff8fx
{
namespace mercy200
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // the cursors when the root started the tick's queue
	static uint8_t g_held_packets[0x20000], g_held_fa[0x20000];
	static const Node24 *g_dir = nullptr; // the director node (its counter is the next tick's when it ran)

	enum RecKind { REC_TRAIL, REC_NODE, REC_POOL };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated (pools: the header as set up)
		Mat4x3 mat;                // the caster's matrix at the draw (Trail)
		int16_t point[4];          // the tip point at the draw (TipShards, Slash)
		uint32_t snap;             // pools: offset of the pool as drawn in g_snap
	};
	static const int REC_MAX = 192;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint8_t g_snap[0x40000];
	static uint32_t g_nsnap = 0;
	static uint8_t g_trail_sa[0x100], g_trail_sb[0x100]; // the trail's splines of the real tick

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_pkt_start = PacketCursor();
		g_fa_start = FrameArena();
		g_nrec = 0;
		g_nsnap = 0;
		g_dir = nullptr;
	}

	static void held_note_director(const Node24 *n) { g_dir = n; }

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
		memcpy(r.point, (const void *)POINT, sizeof(r.point));
		if (header) magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_trail(const Node24 *n)
	{
		if (!NoteRec(REC_TRAIL, ORIG_Trail, n, false)) return;
		memcpy(g_trail_sa, (const void *)SPL_BASE, sizeof(g_trail_sa));
		memcpy(g_trail_sb, (const void *)SPL_TIP, sizeof(g_trail_sb));
	}

	static void held_note_node(uint32_t fn, const Node24 *n) { NoteRec(REC_NODE, fn, n, true); }

	static uint32_t PoolHeaderSize(uint32_t fn)
	{
		if (fn == ORIG_TipShards) return 0x6C;
		if (fn == ORIG_WaveTrail) return 0x58;
		return 0xB4;
	}

	static void held_note_pool(uint32_t fn, const Node24 *n, const uint8_t *h)
	{
		const uint32_t size = POOL_END - POOL;
		if (g_nsnap + size > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
		HRec *r = NoteRec(REC_POOL, fn, n, false);
		if (!r) return;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)POOL, size);
		g_nsnap += size;
		memcpy(r->hdr, h, PoolHeaderSize(fn));
	}

	// a held draw starts like the real one: its GTE state and header scratch
	static void Replay(const HRec &r, bool header)
	{
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = header ? (uint8_t *)r.hdr : nullptr;
	}

	// the node still runs the same task on the next tick (it did not end on this one) and advanced (Spark: its life
	// counts down)
	static bool Alive(const HRec &r)
	{
		const int16_t next = (int16_t)(r.fn == ORIG_Spark ? r.copy.c - 1 : r.copy.c + 1);
		return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == r.fn && r.node->c == next;
	}

	static inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return magc::lerp16(a, b, num, den); }
	// fade half way when the draw mode stays (the next tick in the same fade ramp), else tick N's
	static int32_t FadeLerp(bool same, int32_t f0, int32_t f1, bool next, int num, int den) { return next && same ? lerp_i(f0, f1, num, den) : f0; }

	// the caster's matrix translation of the next tick (the frame gate moved a moving caster to its in-between position:
	// tick N's + (now - tick N's) * den / num), put in place for a look-ahead; restored by the caller
	static void NextCasterMatrix(Mat4x3 *em, const Mat4x3 &at_tick, int num, int den)
	{
		const Mat4x3 now = *em;
		for (int k = 0; k < 3; k++) em->t[k] = at_tick.t[k] + (int32_t)((int64_t)(now.t[k] - at_tick.t[k]) * den / num);
	}

	// ---- held frame surroundings: the module cursor (magc::HeldScope: + scratch, shadow scratch, GTE) and the frame arena
	struct HeldScope200
	{
		magc::HeldScope base;
		uint32_t fa_saved;
		HeldScope200() : base(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start)
		{
			fa_saved = FrameArena();
			if (g_fa_start && fa_saved > g_fa_start)
			{
				const uint32_t used = fa_saved - g_fa_start;
				memcpy(g_held_fa, (const void *)g_fa_start, used < sizeof(g_held_fa) ? used : sizeof(g_held_fa));
			}
			FrameArena() = (uint32_t)g_held_fa;
		}
		~HeldScope200() { FrameArena() = fa_saved; }
	};

	// ---- the tip point / blade of the next tick ----
	struct Blade { int32_t base[2]; int32_t dir[4]; }; // the first base point (dwords: x, y | z, pad), the unit blade direction
	static Blade g_next_blade;        // the trail's blade on the next tick (the live globals when no trail record predicts it)
	static bool g_next_blade_set = false;

	static void LiveBlade(Blade *b)
	{
		memcpy(b->base, (const void *)BASE0, 8);
		memcpy(b->dir, (const void *)BLADE_DIR, 16);
	}

	// TrailDirections' first point and blade direction of the splines sa / sb (the tip TIP0 = sb's first point)
	static void BladeOf(const uint8_t *sa, const uint8_t *sb, Blade *b)
	{
		memcpy(b->base, sa, 8);
		b->dir[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)sa;
		b->dir[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sa + 2);
		b->dir[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sa + 4);
		Normalize(b->dir, b->dir);
	}

	// SlashPoint's formula on a blade
	static void BladePoint(const Blade &b, int16_t *p)
	{
		p[0] = (int16_t)((mul32(b.dir[0], 400) >> 12) + b.base[0]);
		p[1] = (int16_t)((int16_t)(mul32(b.dir[1], 400) >> 12) + *(const int16_t *)((const uint8_t *)b.base + 2));
		p[2] = (int16_t)((mul32(b.dir[2], 400) >> 12) + b.base[1]);
	}

	// the tip point at the start of the next tick, after the director's write (0x48: the blade point from the live
	// globals; 0x78: the effect frame x (-400, -500, -500))
	static void NextPointStart(int16_t *p)
	{
		memcpy(p, (const void *)POINT, 6);
		if (!g_dir || (uint32_t)g_dir->hdr.func != ORIG_Director || !(g_dir->hdr.flags & 1)) return;
		if (g_dir->c == 0x48)
		{
			Blade b;
			LiveBlade(&b);
			BladePoint(b, p);
		}
		else if (g_dir->c == 0x78)
		{
			p[0] = -400;
			p[1] = -500;
			p[2] = -500;
			MatVec((const Mat4x3 *)FRAME, p, p);
			const Mat4x3 *fr = (const Mat4x3 *)FRAME;
			p[0] = (int16_t)(p[0] + (int16_t)fr->t[0]);
			p[1] = (int16_t)(p[1] + (int16_t)fr->t[1]);
			p[2] = (int16_t)(p[2] + (int16_t)fr->t[2]);
		}
	}

	// the tip point the next tick draws the first slash's record r at (its own write at its ticks 0x1E..0x23)
	static void NextSlashPoint(int16_t *p, int32_t c_next, bool slash0_alive)
	{
		NextPointStart(p);
		if (slash0_alive && c_next >= 0x1E && c_next <= 0x23)
		{
			Blade b;
			if (g_next_blade_set) b = g_next_blade;
			else LiveBlade(&b);
			BladePoint(b, p);
		}
	}

	// the first slash of the log (index 0) and whether it lives on (its counter on the next tick)
	static const HRec *FirstSlash()
	{
		for (int i = 0; i < g_nrec; i++)
			if (g_rec[i].fn == ORIG_Slash && g_rec[i].copy.e == 0) return &g_rec[i];
		return nullptr;
	}

	// the tip point a task drawn at record r draws at on the next tick
	static void NextPointFor(const HRec &r, int16_t *p)
	{
		if (r.fn == ORIG_TipShards) { NextPointStart(p); return; }
		const HRec *s0 = FirstSlash();
		const bool alive0 = s0 && Alive(*s0) && s0->copy.c + 1 < 0x25;
		NextSlashPoint(p, s0 ? s0->copy.c + 1 : 0, alive0);
	}

	static void PointLerp(const HRec &r, int num, int den, bool move, int16_t *p)
	{
		memcpy(p, r.point, 8);
		if (!move) return;
		int16_t n[4];
		NextPointFor(r, n);
		for (int k = 0; k < 3; k++) p[k] = Lerp16(r.point[k], n[k], num, den);
	}

	// ---- trail ----
	static uint8_t g_ring[6 * 0x14], g_pa[0x30], g_pb[0x30];
	static uint8_t g_na[0x100], g_nb[0x100], g_ma[0x100], g_mb[0x100];

	// the next tick's splines (false: the next tick draws no trail)
	static bool TrailNext(const HRec &r, int num, int den)
	{
		uint8_t *e = Caster();
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (!w) return false;
		memcpy(g_ring, (const void *)RING, sizeof(g_ring));
		TrailWork *tw = (TrailWork *)FieldAlloc(0x40);
		Mat4x3 *em = (Mat4x3 *)(e + 0x40);
		const Mat4x3 now = *em;
		NextCasterMatrix(em, r.mat, num, den);
		pose_midpoint((void *)w, (void *)(w + 0xC), 1, 1);
		TrailPoints(tw);
		BuildBoneMatricesFromPose(w);
		*em = now;
		const int32_t c = r.node->c;
		const int32_t idx = c % 6;
		RingEntry *re = (RingEntry *)(g_ring + 0x14 * idx);
		re->alive = 1;
		re->base[0] = *(const uint32_t *)&tw->base[0];
		re->base[1] = *(const uint32_t *)&tw->base[2];
		re->tip[0] = *(const uint32_t *)&tw->tip[0];
		re->tip[1] = *(const uint32_t *)&tw->tip[2];
		const int32_t cnt = TrailGather(g_ring, idx, TrailWant(c), g_pa, g_pb);
		if (cnt > 1)
		{
			memcpy(g_na, g_trail_sa, sizeof(g_na));
			memcpy(g_nb, g_trail_sb, sizeof(g_nb));
			SplineCurve(cnt, (const int16_t *)g_pa, 0x20, (int16_t *)g_na);
			SplineCurve(cnt, (const int16_t *)g_pb, 0x20, (int16_t *)g_nb);
		}
		FieldFree(0x40);
		return cnt > 1;
	}

	static void TrailHeld(const HRec &r, int num, int den)
	{
		const uint8_t *sa = g_trail_sa, *sb = g_trail_sb;
		if (num != 0 && !g_tick_paused && Alive(r) && TrailNext(r, num, den))
		{
			BladeOf(g_na, g_nb, &g_next_blade);
			g_next_blade_set = true;
			const int16_t *a0 = (const int16_t *)g_trail_sa, *a1 = (const int16_t *)g_na;
			const int16_t *b0 = (const int16_t *)g_trail_sb, *b1 = (const int16_t *)g_nb;
			int16_t *ma = (int16_t *)g_ma, *mb = (int16_t *)g_mb;
			for (int k = 0; k < 0x80; k++)
			{
				const bool pad = (k & 3) == 3; // (the 4th words: the real tick's)
				ma[k] = pad ? a0[k] : Lerp16(a0[k], a1[k], num, den);
				mb[k] = pad ? b0[k] : Lerp16(b0[k], b1[k], num, den);
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
	// a single model's held draw inputs (angles A / B, scale, position, v scroll, fade)
	enum { IN_A, IN_B, IN_S, IN_X, IN_Y, IN_Z, IN_SCROLL, IN_FADE, IN_N };
	struct NodeIn { int32_t v[IN_N]; };

	static void NodeInputs(const HRec &r, int num, int den, NodeIn *in)
	{
		const Node24 &d = r.copy;
		const bool move = num != 0 && !g_tick_paused && Alive(r);
		const int32_t c = d.c;
		int32_t *v = in->v;
		for (int k = 0; k < IN_N; k++) v[k] = 0;
		v[IN_A] = d.w18;
		v[IN_S] = d.w1C;
		v[IN_X] = d.w10;
		v[IN_Y] = d.w12;
		v[IN_Z] = d.w14;
		auto L = [&](int16_t a, int16_t b) { return (int32_t)(move ? Lerp16(a, b, num, den) : a); };
		switch (r.fn)
		{
		case ORIG_Spark:
		{
			Node24 x = d;
			SparkUpdate(&x);
			v[IN_X] = L(d.w10, x.w10);
			v[IN_Y] = L(d.w12, x.w12);
			v[IN_Z] = L(d.w14, x.w14);
			v[IN_S] = L(d.w20, x.w20);
			break;
		}
		case ORIG_Ember:
		{
			Node24 x = d;
			EmberUpdate(&x);
			v[IN_Y] = L(d.w12, x.w12);
			v[IN_S] = L(d.w1C, x.w1C);
			break;
		}
		case ORIG_CasterRing:
			v[IN_SCROLL] = move ? lerp_i(shl32(c, 3), shl32(c + 1, 3), num, den) : shl32(c, 3);
			v[IN_FADE] = FadeLerp(CasterRingFaded(c) == CasterRingFaded(c + 1), CasterRingFade(c), CasterRingFade(c + 1), move, num, den);
			break;
		case ORIG_TipFlash:
			v[IN_A] = L(d.w18, (int16_t)(d.w18 + d.w1A));
			v[IN_S] = L(d.w1C, (int16_t)(d.w1C + d.w1E));
			v[IN_FADE] = FadeLerp((c >= 0xA) == (c + 1 >= 0xA), mul32(c - 0xA, 682), mul32(c + 1 - 0xA, 682), move, num, den);
			break;
		case ORIG_Slash:
		{
			int16_t p[4];
			PointLerp(r, num, den, move, p);
			v[IN_X] = p[0];
			v[IN_Y] = p[1];
			v[IN_Z] = p[2];
			v[IN_A] = L(d.w18, (int16_t)(d.w18 + d.w1A));
			v[IN_B] = L(d.w20, (int16_t)(d.w20 + d.w22));
			v[IN_S] = L(d.w1C, (int16_t)(d.w1C + d.w1E));
			v[IN_FADE] = FadeLerp((c < 0xC) == (c + 1 < 0xC), 0x1000 - mul32(c, 341), 0x1000 - mul32(c + 1, 341), move, num, den);
			break;
		}
		case ORIG_FrameModel:
			v[IN_S] = L(d.w1C, (int16_t)(d.w1C + d.w1E));
			v[IN_FADE] = FadeLerp((c >= 8) == (c + 1 >= 8), shl32(c - 8, 9), shl32(c + 1 - 8, 9), move, num, den);
			break;
		case ORIG_WaveRing:
		{
			const WaveRec *rec = Wave(d.e);
			const int16_t *p0 = rec->pt[d.w20];
			const int32_t i1 = d.w20 + d.w22;
			const bool step = move && i1 < 0x40;
			const int16_t *p1 = step ? rec->pt[i1] : p0;
			v[IN_X] = Lerp16(p0[0], p1[0], num, den);
			v[IN_Y] = Lerp16(p0[1], p1[1], num, den);
			v[IN_Z] = Lerp16(p0[2], p1[2], num, den);
			v[IN_A] = step ? Lerp16(d.w18, (int16_t)(d.w18 + d.w1A), num, den) : d.w18;
			v[IN_FADE] = mul32(d.w14 * 3, 0x100);
			break;
		}
		case ORIG_ImpactFlash:
			v[IN_S] = L(d.w1C, (int16_t)(d.w1C + d.w1E));
			v[IN_FADE] = FadeLerp((c >= 6) == (c + 1 >= 6), shl32(c - 6, 9), shl32(c + 1 - 6, 9), move, num, den);
			break;
		case ORIG_TargetRing:
			v[IN_A] = L(d.w18, (int16_t)(d.w18 + d.w1A));
			v[IN_S] = L(d.w1C, (int16_t)(d.w1C + d.w1E));
			v[IN_FADE] = FadeLerp((c >= 8) == (c + 1 >= 8), shl32(c - 8, 9), shl32(c + 1 - 8, 9), move, num, den);
			break;
		}
	}

	static void NodeDraw(const HRec &r, const NodeIn &in)
	{
		const Node24 &d = r.copy;
		const int32_t *v = in.v;
		const int32_t c = d.c;
		int16_t p[4] = { (int16_t)v[IN_X], (int16_t)v[IN_Y], (int16_t)v[IN_Z], 0 };
		Node24 m = d;
		m.w10 = p[0];
		m.w12 = p[1];
		m.w14 = p[2];
		switch (r.fn)
		{
		case ORIG_Flame: FlameDraw(&m); break;
		case ORIG_Spark: m.w20 = (int16_t)v[IN_S]; SparkDraw(&m); break;
		case ORIG_Ember: m.w1C = (int16_t)v[IN_S]; EmberDraw(&m); break;
		case ORIG_CasterRing: CasterRingDraw(&d, v[IN_SCROLL], CasterRingFaded(c), v[IN_FADE]); break;
		case ORIG_TipFlash: TipFlashDraw(&d, (int16_t)v[IN_A], (int16_t)v[IN_S], c >= 0xA, v[IN_FADE]); break;
		case ORIG_Slash: SlashDraw(&d, p, (int16_t)v[IN_A], (int16_t)v[IN_B], (int16_t)v[IN_S], c < 0xC, v[IN_FADE]); break;
		case ORIG_FrameModel:
		{
			Mat4x3 cf; // the camera x effect frame with the held frame's camera
			ComposeAffineTransform(&Camera(), (const Mat4x3 *)FRAME, &cf);
			FrameModelDraw(&cf, &d, (int16_t)v[IN_S], c >= 8, v[IN_FADE]);
			break;
		}
		case ORIG_WaveRing: WaveRingDraw(&d, (int16_t)v[IN_A], p, v[IN_FADE]); break;
		case ORIG_ImpactFlash: ImpactFlashDraw(&d, (int16_t)v[IN_S], c >= 6, v[IN_FADE]); break;
		case ORIG_TargetRing: TargetRingDraw(&d, (int16_t)v[IN_A], (int16_t)v[IN_S], c >= 8, v[IN_FADE]); break;
		default: magc::g_hdr_slot = nullptr; break;
		}
	}

	static void NodeHeld(const HRec &r, int num, int den)
	{
		NodeIn in;
		NodeInputs(r, num, den, &in);
		Replay(r, true);
		NodeDraw(r, in);
	}

	// ---- particles: the draws of the pool tasks from the pools as drawn ----
	// the entry advanced on the tick (same type, life + 1): its post-update state is the next draw's
	static const PoolE *NextEntry(uint32_t o, const PoolE *p, bool move)
	{
		if (!move) return nullptr;
		const PoolE *now = (const PoolE *)(POOL + o);
		if (now->type != p->type || now->life != (int16_t)(p->life + 1)) return nullptr;
		return now;
	}

	// the entry drawn by the task (TipShards: type bit 1, SparkBurst: bit 2, the per-target tasks: their exact type)
	static bool EntryOf(const HRec &r, const PoolE *e)
	{
		switch (r.fn)
		{
		case ORIG_TipShards: return (e->type & 1) != 0;
		case ORIG_SparkBurst: return (e->type & 2) != 0;
		case ORIG_WaveTrail: return e->type == (uint32_t)(r.copy.e + 0x20);
		case ORIG_TargetSparks: return e->type == (uint32_t)(r.copy.e + 0x40);
		default: return e->type == (uint32_t)(r.copy.e + 0x80); // TargetBurst
		}
	}

	// one entry's held state: the copy q with the words that move half way (TipShards' roll, WaveTrail's length,
	// TargetBurst's position); returns its fade (TipShards / WaveTrail)
	static int32_t EntryHeld(const HRec &r, uint32_t o, PoolE *q, bool move, int num, int den)
	{
		const int32_t life = q->life;
		const PoolE *now = NextEntry(o, q, move);
		switch (r.fn)
		{
		case ORIG_TipShards:
		{
			int32_t fade = ShardFade(life);
			if (now)
			{
				q->a[2] = Lerp16(q->a[2], now->a[2], num, den);
				if (ShardFaded(life) && ShardFaded(life + 1)) fade = lerp_i(fade, ShardFade(life + 1), num, den);
			}
			return fade;
		}
		case ORIG_WaveTrail:
		{
			int32_t fade = mul32(life, 682);
			if (now)
			{
				q->a[0] = Lerp16(q->a[0], now->a[0], num, den);
				fade = lerp_i(fade, mul32(life + 1, 682), num, den);
			}
			return fade;
		}
		case ORIG_TargetBurst:
			if (now) for (int k = 0; k < 3; k++) q->a[k] = Lerp16(q->a[k], now->a[k], num, den);
			return 0;
		}
		return 0;
	}

	// one entry drawn as its task draws it (work block / header set up by PoolHeld)
	static void EntryDraw(const HRec &r, ModelWork *w, uint8_t *h, const PoolE *q, int32_t fade)
	{
		switch (r.fn)
		{
		case ORIG_TipShards: ShardDraw(w, (RenderHeader *)h, q, q->a[2], ShardFaded(q->life), fade); break;
		case ORIG_WaveTrail: WaveTrailDraw(w, h, Wave(r.copy.e), q, q->life >= 0, fade, q->a[0]); break;
		case ORIG_SparkBurst: SpriteDraw(w, h, q, q->life, &q->a[0], -(q->scale >> 3)); break;
		default: SpriteDraw(w, h, q, q->life, &q->a[0], -r.copy.w18); break; // TargetSparks / TargetBurst
		}
	}

	// the task's frame before its draw loop, with the live camera (TipShards: at its tip point p)
	static void PoolFrame(const HRec &r, ModelWork *w, const int16_t *p)
	{
		if (r.fn == ORIG_TipShards)
		{
			TransformCameraByShadowRotation(p, 0x1000, 0);
			w->v[2] = 0x1000;
			w->v[1] = 0x1000;
			w->ang[1] = 0;
			w->ang[0] = 0;
		}
		else if (r.fn == ORIG_WaveTrail)
		{
			memcpy(&w->m1, &Wave(r.copy.e)->mat, sizeof(Mat4x3));
			w->m1.t[0] = 0;
			w->m1.t[1] = 0;
			w->m1.t[2] = 0;
			ComposeAffineTransform(&Camera(), &w->m1, &w->m1);
		}
		else SpriteFrame(w);
	}

	static void PoolHeld(const HRec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		const uint8_t *snap = g_snap + r.snap;
		const uint32_t size = POOL_END - POOL;
		const uint32_t hsize = PoolHeaderSize(r.fn);
		Replay(r, false);
		const uint32_t wsize = (r.fn == ORIG_TipShards || r.fn == ORIG_WaveTrail) ? 0x68 : 0x48;
		ModelWork *w = (ModelWork *)FieldAlloc(wsize);
		uint8_t *h = (uint8_t *)FieldAlloc(hsize);
		memcpy(h, r.hdr, hsize);
		int16_t p[4];
		PointLerp(r, num, den, move && r.fn == ORIG_TipShards, p);
		PoolFrame(r, w, p);
		for (uint32_t o = 0; o < size; o += 0x18)
		{
			const PoolE *e = (const PoolE *)(snap + o);
			if (!EntryOf(r, e)) continue;
			PoolE q = *e;
			const int32_t fade = EntryHeld(r, o, &q, move, num, den);
			EntryDraw(r, w, h, &q, fade);
		}
		FieldFree(hsize);
		FieldFree(wsize);
	}

	// the held-frame camera: the shared camera-script task's prediction (Doomtrain's port) runs the script's maths on the
	// Field_Alloc scratch stack (its 0x1B0-byte work block): the scratch above the pointer is put back as the real tick
	// left it (the next real tick's draw headers read what their fields were not set to)
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		static uint8_t save[0x1000];
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		if (scratch) memcpy(save, scratch, sizeof(save));
		const bool r = camscript_held_camera(num, den, world, lookat);
		if (scratch) memcpy(scratch, save, sizeof(save));
		return r;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		HeldScope200 scope;
		g_next_blade_set = false;
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

	static void register_mag200_held()
	{
		using namespace mercy200;
		mark_held(ORIG_Root);
		mark_held(ORIG_Flame);
		mark_held(ORIG_Spark);
		mark_held(ORIG_Ember);
		mark_held(ORIG_Trail);
		mark_held(ORIG_CasterRing);
		mark_held(ORIG_TipFlash);
		mark_held(ORIG_TipShards);
		mark_held(ORIG_Slash);
		mark_held(ORIG_SparkBurst);
		mark_held(ORIG_FrameModel);
		mark_held(ORIG_WaveRing);
		mark_held(ORIG_WaveTrail);
		mark_held(ORIG_ImpactFlash);
		mark_held(ORIG_TargetSparks);
		mark_held(ORIG_TargetBurst);
		mark_held(ORIG_TargetRing);
		register_module_held(200, HeldReady, HeldFrame);
		// the camera: the shared camera-script task 0x63E9C0 (Doomtrain's port), scratch put back
		register_module_camera(200, HeldCamera);
	}
}
