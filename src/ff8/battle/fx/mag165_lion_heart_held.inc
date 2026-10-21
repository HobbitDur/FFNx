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

// 30 fps layer of Lion Heart (effect 165), included at the end of mag165_lion_heart.cpp.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw header, and for the pool
// tasks the pool as drawn (after the tick's spawns, before the update) and their header as set up. A held frame replays
// the log into private packet buffers (module cursor 0x23C6948 and frame arena 0x1D8E054, each holding the real tick's
// packets under the replay), each draw at its exact in-between state (logic stays 15 Hz), with the LIVE camera (the held
// frame's view in 0x1D97778: every draw composes with it, as the real draws do with theirs):
//   Trail - the next tick's ring entry from the weapon's bone 1 at the NEXT pose (pose_midpoint at 1 of the weapon model,
//     the real pose rebuilt afterwards) through the caster's matrix of the next tick (the frame gate moves a moving caster
//     to its in-between position on held frames: its next translation is tick N's + (now - tick N's) * den / num; held
//     when it did not move), the newest min(6, 0x12E - counter) entries splined by the module's own float spline on
//     private buffers (the same Field_Alloc blocks as the real tick's, whose never-written slots it reads), each spline
//     point half way between the real tick's and the next tick's. Holds the real tick's splines when the trail ends on
//     this tick, when the tick was draw-only, or when the next tick would draw no trail.
//   Single models - angles, scales, the y scale (GroundRing, FlatRing, Streak) and the v scroll (GroundRing, ScreenWave)
//     half way to the values the update left (the next draw's); fades half way while the draw mode stays (a mode switch
//     on the next tick: held); ScreenWave's colour (table 0xD88808 by counter) half way per channel to the next tick's.
//     The tip / hit points are the nodes' own (fixed); Streak's screen-space direction is rebuilt from the live camera.
//   Particles (pools A / B / C) - each task's pool as drawn: the entries that advanced on the tick (same node id, life + 1)
//     half way to their post-update scale (RockBurst, Debris, TipRocks), angles (Debris' y, TipShards' phase / roll,
//     TipRocks' x / z), position (HitSparkB, HitShards, FinalShards, TargetSparks) or polar position (TipSmoke's radius,
//     height and angle); fades half way while the draw mode stays; flipbook frames (sprite lives) switch on the real frame;
//     an entry that died on the tick holds its last picture; the tick's spawns are drawn at birth (they were drawn on the
//     real tick). The static sparks (BoneSparks, BladeSparks, HitSparkA, FinalSparkA, FinalRingSparks) only follow the
//     live camera.
//   TargetSink - the target model at its pose half way (pose_midpoint, the real pose rebuilt by the draw's own
//     BuildBoneMatricesFromPose), through its live matrix (the frame gate's in-between entity), clipped at the floor; its
//     shadow as the engine draws it. The clipping renderer's vertex buffer (*0x1D98B3C) is put back after the draw.
// Camera: the shared camera-script task 0x63E9C0 (camscript_held_camera, Doomtrain's port; the effect frame it follows
// moves at 0x5A / 0xF2 / 0x14C: a cut held by the script's prediction). The camera shake word 0x1D97712 (Shake) is a 15 Hz
// sign-alternating step: the frame gate blends the real ticks' offsets itself.
// The director, HitDirector, the bounces, the wave spawner, Flash, Shake and the texture restore draw nothing; the caster's
// animation and the entity positions / flags (the lift, the z steps, the bounces, the sink's y) and the screen flash /
// fades are the engine's. Nothing here writes game state: the scope puts back the cursors, the GTE registers, the
// Field_Alloc scratch and the shadow-rotation scratch; the bone matrices are rebuilt from the real pose after each
// look-ahead.

namespace ff8fx
{
namespace lion165
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // the cursors when the root started the tick's queue
	static uint8_t g_held_packets[0x40000], g_held_fa[0x40000];

	enum RecKind { REC_TRAIL, REC_NODE, REC_POOL, REC_SINK };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated (pools: the header as set up)
		Mat4x3 mat;                // the caster's matrix at the draw
		int16_t tip[4];            // Streak: the tip point it was drawn with
		uint32_t pool, pool_end, stride; // pools
		uint32_t snap;             // pools: offset of the pool as drawn in g_snap
		uint8_t block[0xC8];       // TargetSink: its renderer block as set up (the plane)
	};
	static const int REC_MAX = 160;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint8_t g_snap[0x60000];
	static uint32_t g_nsnap = 0;
	// trail: the splines of the real tick, the ring and the caster's matrix it was sampled with
	static uint8_t g_trail_sa[0x100], g_trail_sb[0x100];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_pkt_start = PacketCursor();
		g_fa_start = FrameArena();
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
		if (header) magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_trail(const Node24 *n)
	{
		if (!NoteRec(REC_TRAIL, ORIG_Trail, n, false)) return;
		memcpy(g_trail_sa, (const void *)SPL_BASE, sizeof(g_trail_sa));
		memcpy(g_trail_sb, (const void *)SPL_TIP, sizeof(g_trail_sb));
	}

	static void held_note_node(uint32_t fn, const Node24 *n)
	{
		HRec *r = NoteRec(REC_NODE, fn, n, true);
		if (r && fn == ORIG_Streak) memcpy(r->tip, (const void *)TIP, sizeof(r->tip));
	}

	// the pool a task draws: A (0x18), B (0x20; FinalShards walks 200 entries) or C (at the texture base)
	static uint32_t PoolOf(uint32_t fn, uint32_t *end, uint32_t *stride)
	{
		*stride = 0x18;
		if (fn == ORIG_HitShards || fn == ORIG_TipRocks) { *stride = 0x20; *end = POOL_B_END; return POOL_B; }
		if (fn == ORIG_FinalShards) { *stride = 0x20; *end = FINAL_SHARDS_END; return POOL_B; }
		if (fn == ORIG_FinalRingSparks) { *end = PoolCBase() + 0x1F4 * 0x18; return PoolCBase(); }
		*end = POOL_A_END;
		return POOL_A;
	}

	static uint32_t PoolHeaderSize(uint32_t fn)
	{
		if (fn == ORIG_RockBurst || fn == ORIG_Debris || fn == ORIG_TipShards || fn == ORIG_HitShards || fn == ORIG_TipRocks || fn == ORIG_FinalShards) return 0x58;
		return 0xB4;
	}

	static void held_note_pool(uint32_t fn, const Node24 *n, const uint8_t *h)
	{
		uint32_t end, stride;
		const uint32_t pool = PoolOf(fn, &end, &stride);
		if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
		HRec *r = NoteRec(REC_POOL, fn, n, false);
		if (!r) return;
		r->pool = pool;
		r->pool_end = end;
		r->stride = stride;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
		g_nsnap += end - pool;
		memcpy(r->hdr, h, PoolHeaderSize(fn));
	}

	static void held_note_sink(const Node24 *n, const uint8_t *h)
	{
		HRec *r = NoteRec(REC_SINK, ORIG_TargetSink, n, false);
		if (r) memcpy(r->block, h, sizeof(r->block));
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

	// ---- held frame surroundings: the module cursor (magc::HeldScope: + scratch, shadow scratch, GTE) and the frame arena
	struct HeldScope165
	{
		magc::HeldScope base;
		uint32_t fa_saved;
		HeldScope165() : base(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start)
		{
			fa_saved = FrameArena();
			if (g_fa_start && fa_saved > g_fa_start)
			{
				const uint32_t used = fa_saved - g_fa_start;
				memcpy(g_held_fa, (const void *)g_fa_start, used < sizeof(g_held_fa) ? used : sizeof(g_held_fa));
			}
			FrameArena() = (uint32_t)g_held_fa;
		}
		~HeldScope165() { FrameArena() = fa_saved; }
	};

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
	// fade half way when the draw mode stays (the next tick in the same fade ramp), else tick N's
	static int32_t FadeLerp(bool same, int32_t f0, int32_t f1, bool next, int num, int den) { return next && same ? lerp_i(f0, f1, num, den) : f0; }

	static uint32_t ColourLerp(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000u;
		for (int k = 0; k < 24; k += 8)
		{
			const int32_t x = (int32_t)(a >> k & 0xFF), y = (int32_t)(b >> k & 0xFF);
			r |= (uint32_t)(lerp_i(x, y, num, den) & 0xFF) << k;
		}
		return r;
	}

	// a single model's held draw inputs
	enum { IN_A, IN_S, IN_SY, IN_SCROLL, IN_FADE, IN_COLOUR, IN_N };
	struct NodeIn { int32_t v[IN_N]; };

	static void NodeInputs(const HRec &r, int num, int den, NodeIn *in)
	{
		const Node24 &d = r.copy;
		const Node24 *now = r.node;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const int32_t c = d.c;
		auto L = [&](int16_t a, int16_t b) { return (int32_t)(next ? Lerp16(a, b, num, den) : a); };
		in->v[IN_A] = d.w18;
		in->v[IN_S] = d.w1C;
		in->v[IN_SY] = d.w20;
		in->v[IN_SCROLL] = 0;
		in->v[IN_FADE] = 0;
		in->v[IN_COLOUR] = 0;
		switch (r.fn)
		{
		case ORIG_GroundRing:
			in->v[IN_SY] = L(d.w20, now->w20);
			in->v[IN_SCROLL] = FadeLerp(true, shl32(c, 4), shl32(c + 1, 4), next, num, den);
			in->v[IN_FADE] = FadeLerp(c + 1 < 8, GroundRingFade(c), GroundRingFade(c + 1), next, num, den);
			break;
		case ORIG_TipFlash:
			in->v[IN_S] = L(d.w1C, now->w1C);
			break;
		case ORIG_TipRing:
			in->v[IN_A] = L(d.w18, now->w18);
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 4, mul32(c - 4, 682), mul32(c + 1 - 4, 682), next, num, den);
			break;
		case ORIG_HitSlashA:
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 4, mul32(c - 4, 682), mul32(c + 1 - 4, 682), next, num, den);
			break;
		case ORIG_HitSlashB:
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 2, shl32(c - 2, 9), shl32(c + 1 - 2, 9), next, num, den);
			break;
		case ORIG_FlatRing:
			in->v[IN_SY] = L(d.w20, now->w20);
			in->v[IN_FADE] = FadeLerp(c >= 2, shl32(c - 2, 9), shl32(c + 1 - 2, 9), next, num, den);
			break;
		case ORIG_ScreenWave:
			in->v[IN_SCROLL] = FadeLerp(true, mul32(d.w20, c), mul32(d.w20, c + 1), next, num, den);
			in->v[IN_FADE] = FadeLerp(true, ScreenWaveFade(c), ScreenWaveFade(c + 1), next, num, den);
			in->v[IN_COLOUR] = (int32_t)(next ? ColourLerp(ScreenWaveColour(c), ScreenWaveColour(c + 1), num, den) : ScreenWaveColour(c));
			break;
		case ORIG_TipFlash2:
			in->v[IN_A] = L(d.w18, now->w18);
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c + 1 < 8 || c >= 0x20, TipFlash2Fade(c), TipFlash2Fade(c + 1), next, num, den);
			break;
		case ORIG_Streak:
			in->v[IN_SY] = L(d.w20, now->w20);
			in->v[IN_FADE] = FadeLerp(c >= 0x20, shl32(c - 0x20, 9), shl32(c + 1 - 0x20, 9), next, num, den);
			break;
		case ORIG_FinalRing:
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 0xC, mul32(c - 0xC, 682), mul32(c + 1 - 0xC, 682), next, num, den);
			break;
		case ORIG_FinalFlash:
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 0x10, shl32(c - 0x10, 9), shl32(c + 1 - 0x10, 9), next, num, den);
			break;
		case ORIG_TargetRing:
			in->v[IN_S] = L(d.w1C, now->w1C);
			in->v[IN_FADE] = FadeLerp(c >= 6, mul32(c - 6, 409), mul32(c + 1 - 6, 409), next, num, den);
			break;
		default:
			break;
		}
	}

	static void NodeDraw(const HRec &r, const NodeIn &in)
	{
		const Node24 &d = r.copy;
		const int32_t c = d.c;
		const int16_t a = (int16_t)in.v[IN_A], s = (int16_t)in.v[IN_S], sy = (int16_t)in.v[IN_SY];
		const int32_t fade = in.v[IN_FADE];
		Replay(r, true);
		switch (r.fn)
		{
		case ORIG_GroundRing: GroundRingDraw(&d, sy, in.v[IN_SCROLL], c, fade); break;
		case ORIG_TipFlash: TipFlashDraw(&d, s); break;
		case ORIG_TipRing: TipRingDraw(&d, a, s, c, fade); break;
		case ORIG_HitSlashA: HitSlashDraw(&d, MODEL_SLASHA, s, c >= 4, fade); break;
		case ORIG_HitSlashB: HitSlashDraw(&d, MODEL_SLASHB, s, c >= 2, fade); break;
		case ORIG_FlatRing: FlatRingDraw(&d, sy, c, fade); break;
		case ORIG_ScreenWave: ScreenWaveDraw(&d, in.v[IN_SCROLL], fade, (uint32_t)in.v[IN_COLOUR]); break;
		case ORIG_TipFlash2: TipFlash2Draw(&d, a, s, c, fade); break;
		case ORIG_Streak: StreakDraw(&d, r.tip, sy, c, fade); break;
		case ORIG_FinalRing: FinalRingDraw(&d, s, c, fade); break;
		case ORIG_FinalFlash: FinalFlashDraw(&d, s, c, fade); break;
		case ORIG_TargetRing: TargetRingDraw(&d, s, c, fade); break;
		default: break;
		}
	}

	static void NodeHeld(const HRec &r, int num, int den)
	{
		NodeIn in;
		NodeInputs(r, num, den, &in);
		NodeDraw(r, in);
	}

	// ---- particles: the draws of the pool tasks from the pools as drawn ----
	// the entry advanced on the tick (still the node's id, life + 1): its post-update state is the next draw's
	static const uint8_t *NextEntry(const HRec &r, uint32_t o, const uint8_t *p, bool move)
	{
		if (!move) return nullptr;
		const uint8_t *now = (const uint8_t *)(r.pool + o);
		return *(const int32_t *)now == *(const int32_t *)p && *(const int16_t *)(now + 4) == (int16_t)(*(const int16_t *)(p + 4) + 1) ? now : nullptr;
	}

	// one entry's held state: the entry copy q with the words that move half way, its fade
	static int32_t EntryHeld(const HRec &r, uint8_t *q, const uint8_t *now, int num, int den)
	{
		int16_t *a = (int16_t *)(q + 8);
		const int16_t *b = now ? (const int16_t *)(now + 8) : nullptr;
		const int32_t life = *(const int16_t *)(q + 4);
		int16_t &scale = *(int16_t *)(q + 6);
		auto L = [&](int k) { if (b) a[k] = Lerp16(a[k], b[k], num, den); };
		auto F = [&](bool same, int32_t f0, int32_t f1) { return b && same ? lerp_i(f0, f1, num, den) : f0; };
		switch (r.fn)
		{
		case ORIG_RockBurst: L(4); return F(life >= 4, RockFade(life), RockFade(life + 1));
		case ORIG_Debris: L(3); L(4); return F(life + 1 < 6 || life >= 8, DebrisFade(life), DebrisFade(life + 1));
		case ORIG_TipShards: L(2); L(6); return F(life + 1 < 4 || life >= 0xA, TipShardFade(life), TipShardFade(life + 1));
		case ORIG_TipSmoke: L(0); L(1); L(2); return 0;
		case ORIG_HitSparkB: L(0); L(1); L(2); return 0;
		case ORIG_TargetSparks: L(0); L(2); return 0;
		case ORIG_HitShards: L(0); L(1); return F(life >= 4, shl32(life - 4, 9), shl32(life + 1 - 4, 9));
		case ORIG_TipRocks:
			if (now) scale = Lerp16(scale, *(const int16_t *)(now + 6), num, den);
			L(4); L(6);
			return F(life >= 0x10, shl32(life - 0x10, 9), shl32(life + 1 - 0x10, 9));
		case ORIG_FinalShards: L(0); L(1); L(2); return F(life >= 6, mul32(life - 6, 682), mul32(life + 1 - 6, 682));
		default: return 0;
		}
	}

	// one entry drawn as its task draws it (work block / header set up by PoolHeld)
	static void EntryDraw(uint32_t fn, ModelWork *w, uint8_t *h, uint8_t *q, int32_t fade)
	{
		PoolE *p = (PoolE *)q;
		PoolE2 *p2 = (PoolE2 *)q;
		switch (fn)
		{
		case ORIG_RockBurst: RockDraw(w, h, p, p->a[4], p->life, fade); break;
		case ORIG_Debris: DebrisDraw(w, h, p, p->a[3], p->a[4], p->life, fade); break;
		case ORIG_TipShards: TipShardDraw(w, h, p->scale, p->a[2], p->a[6], p->life, fade); break;
		case ORIG_HitShards: HitShardDraw(w, h, p2, &p2->a[0], p2->life, fade); break;
		case ORIG_TipRocks: TipRockDraw(w, h, p2, p2->scale, p2->a[4], p2->a[6], p2->life, fade); break;
		case ORIG_FinalShards: FinalShardDraw(w, h, p2, &p2->a[0], p2->life, fade); break;
		case ORIG_BladeSparks: SmokeSprite(h, (int32_t *)((uint8_t *)w + 0x40), (int32_t *)((uint8_t *)w + 0x30), &p->a[0], p->scale, p->life); break;
		case ORIG_TipSmoke: TipSmokeSprite(w, h, p->a[0], p->a[1], p->a[2], p->scale, p->life); break;
		case ORIG_FinalRingSparks:
			PlainSprite(h, &p->a[0], p->scale, p->life);
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			break;
		default: SmokeSprite(h, Mac(w), Dir(w), &p->a[0], p->scale, p->life); break; // BoneSparks, HitSparkA / B, FinalSparkA, TargetSparks
		}
	}

	// the task's work block size (Field_Alloc, before its header)
	static uint32_t WorkSize(uint32_t fn)
	{
		if (fn == ORIG_FinalShards) return 0x78;
		if (fn == ORIG_RockBurst || fn == ORIG_Debris || fn == ORIG_TipShards || fn == ORIG_HitShards || fn == ORIG_TipRocks) return 0x68;
		if (fn == ORIG_BladeSparks || fn == ORIG_TipSmoke) return 0x50;
		return 0x48;
	}

	// the task's frame before its draw loop, with the live camera
	static void PoolFrame(const HRec &r, ModelWork *w)
	{
		const Node24 *n = &r.copy;
		switch (r.fn)
		{
		case ORIG_RockBurst: case ORIG_Debris: case ORIG_TipRocks:
			w->m1 = Camera();
			w->ang[0] = 0;
			w->ang[1] = 0;
			w->ang[2] = 0;
			return;
		case ORIG_TipShards:
			TipShardFrame(w, n, &Camera());
			w->v[1] = 0x3000;
			w->v[2] = 0;
			return;
		case ORIG_HitShards: HitShardFrame(w, n, Hit(n->w1A), &Camera()); return;
		case ORIG_FinalShards: FinalShardFrame(w, n, &Camera()); return;
		case ORIG_BladeSparks:
		{
			Mat4x3 *cam = (Mat4x3 *)((uint8_t *)w + 0x10);
			*cam = Camera();
			SpriteLight(cam);
			return;
		}
		case ORIG_TipSmoke: TipSmokeFrame(w, n, &Camera()); return;
		case ORIG_TargetSparks:
			w->ang[0] = 0;
			w->ang[1] = 0;
			w->ang[2] = 0;
			ComposeZYXRotationMatrix(w->ang, &w->m1);
			w->m1.t[0] = n->w10;
			w->m1.t[1] = 0;
			w->m1.t[2] = n->w14;
			ComposeAffineTransform(&Camera(), &w->m1, &w->m1);
			SpriteLight(&w->m1);
			return;
		case ORIG_FinalRingSparks:
			w->ang[0] = 0x100;
			w->ang[1] = 0;
			w->ang[2] = 0x100;
			ComposeZYXRotationMatrix(w->ang, &w->m1);
			w->m1.t[0] = n->w10;
			w->m1.t[1] = n->w12;
			w->m1.t[2] = n->w14;
			TransformVectorBy3x3Matrix(&Camera(), w->m1.t, w->m1.t);
			w->m1.t[0] += Camera().t[0];
			w->m1.t[1] += Camera().t[1];
			w->m1.t[2] += Camera().t[2];
			SpriteLight(&w->m1);
			return;
		default: // BoneSparks, HitSparkA / B, FinalSparkA
			w->m1 = Camera();
			SpriteLight(&w->m1);
			return;
		}
	}

	static void PoolHeld(const HRec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		const uint8_t *snap = g_snap + r.snap;
		const uint32_t size = r.pool_end - r.pool;
		const int32_t id = r.copy.e;
		Replay(r, false);
		const uint32_t wsize = WorkSize(r.fn);
		ModelWork *w = (ModelWork *)FieldAlloc(wsize);
		const uint32_t hsize = PoolHeaderSize(r.fn);
		uint8_t *h = (uint8_t *)FieldAlloc(hsize);
		memcpy(h, r.hdr, hsize);
		PoolFrame(r, w);
		uint8_t q[0x20];
		for (uint32_t o = 0; o < size; o += r.stride)
		{
			const uint8_t *p = snap + o;
			if (*(const int32_t *)p != id) continue;
			memcpy(q, p, r.stride);
			const int32_t fade = EntryHeld(r, q, NextEntry(r, o, p, move), num, den);
			EntryDraw(r.fn, w, h, q, fade);
		}
		FieldFree(hsize);
		FieldFree(wsize);
	}

	// ---- TargetSink: the target at its pose half way through its live matrix ----
	// the bytes of the clipping renderer's vertex buffer (*0x1D98B3C) a model's draw writes (ClipRenderModel: each part
	// in the mask from the buffer start, 8 bytes per vertex)
	static uint32_t ClipVertexBytes(uint32_t model, uint32_t mask)
	{
		if (!model) return 0;
		uint32_t offs = At<uint32_t>(model, 4), most = 0;
		const int32_t nparts = At<int32_t>(offs, 0);
		offs += 4;
		for (int32_t i = 0; i < nparts; i++, offs += 4)
		{
			if (!(mask & (uint32_t)shl32(1, i))) continue;
			uint32_t s = At<uint32_t>(offs, 0) + At<uint32_t>(model, 4);
			int32_t groups = At<int16_t>(s, 0), n = 0;
			s += 2;
			for (; groups > 0; groups--)
			{
				const int32_t k = At<int16_t>(s, 2);
				n += k;
				s += 4 + 6 * k;
			}
			if ((uint32_t)n * 8 > most) most = (uint32_t)n * 8;
		}
		return most;
	}

	static void SinkHeld(const HRec &r, int num, int den)
	{
		uint8_t *ent = Entity(r.copy.e);
		const bool next = num != 0 && !g_tick_paused && Alive(r) && *(const uint32_t *)(ent + 0x64);
		Replay(r, false);
		ClipHeader *h = (ClipHeader *)FieldAlloc(0x1A8);
		memcpy(h, r.block, sizeof(r.block));
		// the vertex buffer is put back as the real tick left it (the in-between pose's vertices are the held frame's own)
		static uint8_t vsave[0x10000];
		const uint8_t *wpn = *(const uint8_t *const *)(ent + 0x78);
		uint32_t vbytes = ClipVertexBytes(*(const uint32_t *)(ent + 0x64), *(const uint32_t *)(ent + 0x7C));
		if (wpn && !(ent[1] & 2)) { const uint32_t wb = ClipVertexBytes(*(const uint32_t *)(wpn + 4), 0xFFFFFFFFu); if (wb > vbytes) vbytes = wb; }
		uint8_t *vbuf = var<uint8_t *>(0x1D98B3C);
		if (!vbuf || vbytes > sizeof(vsave)) vbytes = 0;
		if (vbytes) memcpy(vsave, vbuf, vbytes);
		if (next) pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), num, den);
		DrawClipped(h, ent);
		if (vbytes) memcpy(vbuf, vsave, vbytes);
		FieldFree(0x1A8);
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
		HeldScope165 scope;
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			switch (r.kind)
			{
			case REC_TRAIL: TrailHeld(r, num, den); break;
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_POOL: PoolHeld(r, num, den); break;
			case REC_SINK: SinkHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag165_held()
	{
		using namespace lion165;
		mark_held(ORIG_Root);
		mark_held(ORIG_Trail);
		mark_held(ORIG_GroundRing);
		mark_held(ORIG_RockBurst);
		mark_held(ORIG_Debris);
		mark_held(ORIG_BoneSparks);
		mark_held(ORIG_BladeSparks);
		mark_held(ORIG_TipFlash);
		mark_held(ORIG_TipShards);
		mark_held(ORIG_TipSmoke);
		mark_held(ORIG_TipRing);
		mark_held(ORIG_HitSparkA);
		mark_held(ORIG_HitSparkB);
		mark_held(ORIG_HitSlashA);
		mark_held(ORIG_HitSlashB);
		mark_held(ORIG_HitShards);
		mark_held(ORIG_FlatRing);
		mark_held(ORIG_ScreenWave);
		mark_held(ORIG_TipFlash2);
		mark_held(ORIG_TipRocks);
		mark_held(ORIG_Streak);
		mark_held(ORIG_FinalRing);
		mark_held(ORIG_FinalSparkA);
		mark_held(ORIG_FinalShards);
		mark_held(ORIG_FinalFlash);
		mark_held(ORIG_FinalRingSparks);
		mark_held(ORIG_TargetSink);
		mark_held(ORIG_TargetSparks);
		mark_held(ORIG_TargetRing);
		register_module_held(165, HeldReady, HeldFrame);
		// the camera: the shared camera-script task 0x63E9C0 (Doomtrain's port), scratch put back
		register_module_camera(165, HeldCamera);
	}
}
