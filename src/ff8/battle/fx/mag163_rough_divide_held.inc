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

// 30 fps layer of Rough Divide (effect 163), included at the end of mag163_rough_divide.cpp.
//
// Every drawing task of the main queue draws THEN updates. A real tick records its draws in order (the master's seam
// starts the log): the node as drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw
// header(s), and for the particle tasks the pool as drawn (after the tick's spawns, before the update). A held frame
// replays the log into a private packet buffer, each draw at its exact in-between state (logic stays 15 Hz):
//   Trail - Renzokuken's (fx160): the next tick's ring entry from the weapon's bone 1 at the NEXT pose (pose_midpoint at
//     1 of the weapon model, the real pose rebuilt afterwards) through the caster's matrix of the next tick (the frame
//     gate moves a moving caster to its in-between position on held frames: its next translation is tick N's + (now -
//     tick N's) * den / num; held when it did not move), the newest min(7, 0x2A - counter) entries splined on private
//     buffers, each spline point half way between the real tick's and the next tick's. Holds the real tick's splines
//     when the trail ends on this tick, when the tick was draw-only, or when the next tick would draw nothing.
//   Glow - its anchor (the caster's effect bone 0xF1) half way to the next tick's (the body at the NEXT pose through the
//     next matrix, as the trail; the word GetEffectSpawnPosition writes, 0x1D9770C, put back), its y scale half way,
//     the fade half way from counter 7 on (the mode switch before: held).
//   Flash / ring / shock / flare - y angle and scales half way to the update's values, the fade half way after its
//     mode switch (held on the switch); wave - the u scroll half way, the fade half way inside the fade-in (counters
//     0..3) and the fade-out (10..), held on the two switches.
//   Particles (pools A / B / C) - each task's pool as drawn: the particles that advanced on the tick (same type, life
//     + 1) half way to their post-update position / scale / angles / fade; flipbook frames (sprite lives) switch on the
//     real frame; a particle that died on the tick holds its last picture; the tick's spawns are drawn at birth (they
//     were drawn on the real tick).
//   Streak - its width half way to the next tick's, the fade (DPCT IR0) half way from counter 0x20; the records (one
//     more each tick up to 10: a discrete growth) as drawn; the vertices on a private buffer.
// The director and the texture restore draw nothing; the caster's animation, position and tint and the screen flash are
// drawn by the engine. Nothing here writes game state: the gate puts back the GTE registers and the scratch; the bone
// matrices are rebuilt from the real pose after each look-ahead.

namespace ff8fx
{
namespace rough163
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_packets = 0;        // the module's packet cursor when the tick started drawing
	static uint8_t g_held_packets[0x20000];

	enum RecKind { REC_TRAIL, REC_NODE, REC_POOL, REC_STREAK };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its (first) draw header when it was allocated
		uint8_t hdr2[0x58];        // pillar: scratch under its model header
		int16_t anchor[4];         // glow: the anchor it was drawn at
		Mat4x3 mat;                // glow: the caster's matrix it was drawn with
		uint32_t pool, pool_end;   // particles: the pool
		uint32_t snap;             // particles: offset of the pool as drawn in g_snap
	};
	static const int REC_MAX = 48;
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
		if (n) memcpy(&r.copy, n, sizeof(Node24));
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		r.mat = *(const Mat4x3 *)(Caster() + 0x40);
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

	static void held_note_glow(const Node24 *n, const int16_t *anchor)
	{
		if (HRec *r = NoteRec(REC_NODE, ORIG_Glow, n, true)) memcpy(r->anchor, anchor, sizeof(r->anchor));
	}

	static void held_note_pool(uint32_t fn, const Node24 *n)
	{
		uint32_t pool = POOL_B, end = POOL_B_END;
		if (fn == ORIG_Sparkle || fn == ORIG_HitSpark || fn == ORIG_Burst) { pool = POOL_A; end = POOL_A_END; }
		else if (fn == ORIG_Debris) { pool = POOL_C; end = POOL_C_END; }
		if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
		HRec *r = NoteRec(REC_POOL, fn, n, true);
		if (!r) return;
		r->pool = pool;
		r->pool_end = end;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
		g_nsnap += end - pool;
	}

	static void held_note_header2(const uint8_t *h, int size)
	{
		if (g_ported_tick != g_real_tick || g_nrec == 0) return;
		HRec &r = g_rec[g_nrec - 1];
		if (r.kind == REC_POOL && r.fn == ORIG_Pillar) memcpy(r.hdr2, h, size < (int)sizeof(r.hdr2) ? size : sizeof(r.hdr2));
	}

	static void held_note_streak(const Node24 *n, const StreakWork *w)
	{
		(void)w;
		NoteRec(REC_STREAK, ORIG_Streak, n, false);
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
		const int32_t want = c >= TRAIL_LENGTH - 7 ? TRAIL_LENGTH - c : 7;
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

	// the glow's anchor on the next tick: the body at its next pose through the next matrix
	static bool GlowNextAnchor(const HRec &r, int num, int den, int16_t *q)
	{
		uint8_t *e = Caster();
		if (!*(uint32_t *)(e + 0x64)) return false;
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		Mat4x3 *em = (Mat4x3 *)(e + 0x40);
		const Mat4x3 now = *em;
		NextCasterMatrix(em, r.mat, num, den);
		pose_midpoint((void *)(e + 0x60), (void *)(e + 0x6C), 1, 1);
		GetEffectSpawnPosition(e, 0xF1, 0, q);
		BuildBoneMatricesFromPose(e + 0x60);
		*em = now;
		var<uint16_t>(0x1D9770C) = w70c;
		return true;
	}

	static void NodeHeld(const HRec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const Node24 *now = r.node;
		const int32_t c = d.c;
		if (r.fn == ORIG_Glow)
		{
			int16_t q[4];
			memcpy(q, r.anchor, sizeof(q));
			int16_t sy = d.w20;
			const int32_t f0 = shl32(c - 7, 9);
			int32_t fade = f0;
			int16_t qn[4];
			if (next && GlowNextAnchor(r, num, den, qn))
			{
				for (int k = 0; k < 3; k++) q[k] = Lerp16(r.anchor[k], qn[k], num, den);
				sy = Lerp16(d.w20, now->w20, num, den);
				fade = FadeLerp(c, 7, f0, shl32(c + 1 - 7, 9), true, num, den);
			}
			Replay(r, true);
			GlowDraw(&d, sy, c, fade, q);
			return;
		}
		Node24 m = d;
		int32_t f0 = 0, f1 = 0, from = 0;
		uint32_t model = 0;
		if (r.fn == ORIG_Flash) { model = MODEL_FLASH; from = 1; f0 = mul32(c - 1, 682); f1 = mul32(c, 682); }
		else if (r.fn == ORIG_Ring) { model = MODEL_RING; from = 0xC; f0 = shl32(c - 0xC, 9); f1 = shl32(c + 1 - 0xC, 9); }
		else if (r.fn == ORIG_Shock) { model = MODEL_SHOCK; from = 8; f0 = shl32(c - 8, 9); f1 = shl32(c + 1 - 8, 9); }
		else if (r.fn == ORIG_Flare) { model = MODEL_FLARE; from = 8; f0 = shl32(c - 8, 9); f1 = shl32(c + 1 - 8, 9); }
		if (next)
		{
			m.w18 = Lerp16(d.w18, now->w18, num, den);
			m.w1C = Lerp16(d.w1C, now->w1C, num, den);
			m.w20 = Lerp16(d.w20, now->w20, num, den);
		}
		Replay(r, true);
		if (r.fn == ORIG_Wave)
		{
			// u scroll half way; the fade half way inside the fade-in (counters 0..2 -> 1..3) and the fade-out (10..)
			int32_t scroll = mul32(d.w22, c);
			int32_t fade = c < 4 ? shl32(4 - c, 0xA) : shl32(c - 0xA, 9);
			if (next)
			{
				scroll = lerp_i(scroll, mul32(d.w22, c + 1), num, den);
				if (c < 3) fade = lerp_i(fade, shl32(3 - c, 0xA), num, den);
				else if (c >= 0xA) fade = lerp_i(fade, shl32(c + 1 - 0xA, 9), num, den);
			}
			ModelMatrix(&m, m.w18, m.w1C, m.w20);
			WaveRender(c, scroll, fade);
			return;
		}
		const int16_t sy = r.fn == ORIG_Flare ? m.w1C : m.w20;
		const int32_t fade = FadeLerp(c, from, f0, f1, next, num, den);
		ModelMatrix(&m, m.w18, m.w1C, sy);
		ModelRender(model, c, from, fade);
	}

	// ---- particles: the draws of the pool tasks from the pools as drawn ----
	// the particle advanced on the tick (still the task's type, life + 1): its post-update state is the next draw's
	static const ModelP *NextModel(const HRec &r, uint32_t o, uint32_t type, const ModelP *p, bool move)
	{
		if (!move) return nullptr;
		const ModelP *now = (const ModelP *)(r.pool + o);
		return (now->flags & type) && now->life == (int16_t)(p->life + 1) ? now : nullptr;
	}

	static void PoolHeld(const HRec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		const uint8_t *snap = g_snap + r.snap;
		const uint32_t size = r.pool_end - r.pool;
		Replay(r, true);
		if (r.fn == ORIG_Sparkle || r.fn == ORIG_HitSpark || r.fn == ORIG_Burst)
		{
			const uint32_t type = r.fn == ORIG_Sparkle ? 1 : r.fn == ORIG_HitSpark ? 2 : 4;
			SpriteWork *w = (SpriteWork *)FieldAlloc(0x48);
			uint8_t *h = AllocHeader(0xB4);
			w->cam = Camera();
			*(int16_t *)(h + 0x24) = 8;
			*(uint32_t *)h = SEQ_SPARK;
			GteSetLightMatrix(&w->cam);
			GteSetBackColorFromTrans(&w->cam);
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const SpriteP *p = (const SpriteP *)(snap + o);
				if (!(p->flags & type)) continue;
				int16_t pos[4] = { p->pos[0], p->pos[1], p->pos[2], p->pos[3] };
				const SpriteP *now = (const SpriteP *)(r.pool + o);
				if (move && type == 4 && (now->flags & type) && now->life == (int16_t)(p->life + 1))
					for (int k = 0; k < 3; k++) pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
				SpriteDraw(w, h, pos, p->scale, p->life);
			}
			FieldFree(0xB4);
			FieldFree(0x48);
			return;
		}
		if (r.fn == ORIG_Pillar)
		{
			PillarWork *w = (PillarWork *)FieldAlloc(0x68);
			uint8_t *hs = AllocHeader(0xB4);
			*(uint32_t *)hs = SEQ_PILLAR;
			*(int16_t *)(hs + 0x24) = 8;
			uint8_t *hm = (uint8_t *)FieldAlloc(0x58);
			memcpy(hm, r.hdr2, 0x58);
			*(uint32_t *)(hm + 8) = 0;
			w->cam = Camera();
			*(uint32_t *)hm = MODEL_PILLAR;
			GteSetLightMatrix(&w->cam);
			GteSetBackColorFromTrans(&w->cam);
			w->ang[0] = 0x200;
			w->ang[1] = (int16_t)0xFE00;
			for (uint32_t o = 0; o < size; o += 0x20)
			{
				const ModelP *p = (const ModelP *)(snap + o);
				if (!(p->flags & 4)) continue;
				int16_t scale = p->w10, spin = p->w14;
				int32_t fade = mul32(p->life, 682);
				const ModelP *now = NextModel(r, o, 4, p, move);
				if (now && now->life < 6) // (the model's last tick, life 5: held, it is not drawn on the next)
				{
					scale = Lerp16(p->w10, now->w10, num, den);
					spin = Lerp16(p->w14, now->w14, num, den);
					fade = lerp_i(fade, mul32(now->life, 682), num, den);
				}
				PillarDraw(w, hs, hm, p, scale, spin, fade);
			}
			FieldFree(0x58);
			FieldFree(0xB4);
			FieldFree(0x68);
			return;
		}
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)(h + 8) = 0;
		w->cam = Camera();
		if (r.fn == ORIG_Shard)
		{
			*(uint32_t *)h = MODEL_SHARD;
			for (uint32_t o = 0; o < size; o += 0x20)
			{
				const ModelP *p = (const ModelP *)(snap + o);
				if (!(p->flags & 1)) continue;
				ModelP q = *p;
				int32_t fade = mul32(p->life - 1, 682);
				if (const ModelP *now = NextModel(r, o, 1, p, move))
				{
					for (int k = 0; k < 3; k++) q.pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
					if (p->life >= 1) fade = lerp_i(fade, mul32(now->life - 1, 682), num, den);
				}
				ShardDraw(w, h, &q, fade);
			}
		}
		else if (r.fn == ORIG_Chip || r.fn == ORIG_Debris)
		{
			const uint32_t type = r.fn == ORIG_Chip ? 2 : 4;
			*(uint32_t *)h = MODEL_CHIP;
			*(uint32_t *)(h + 0x1C) = 0;
			w->ang[1] = 0;
			for (uint32_t o = 0; o < size; o += 0x20)
			{
				const ModelP *p = (const ModelP *)(snap + o);
				if (!(p->flags & type)) continue;
				ModelP q = *p;
				int16_t scale = p->scale;
				if (const ModelP *now = NextModel(r, o, type, p, move))
				{
					for (int k = 0; k < 3; k++) q.pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
					q.w18 = Lerp16(p->w18, now->w18, num, den);
					q.w1C = Lerp16(p->w1C, now->w1C, num, den);
					scale = Lerp16(p->scale, now->scale, num, den);
				}
				ChipDraw(w, h, &q, scale);
				PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
			}
		}
		else if (r.fn == ORIG_Blade)
		{
			*(uint32_t *)h = MODEL_BLADE;
			w->v[2] = 0;
			for (uint32_t o = 0; o < size; o += 0x20)
			{
				const ModelP *p = (const ModelP *)(snap + o);
				if (!(p->flags & 8)) continue;
				int16_t sy = p->w12;
				int32_t fade = mul32(p->life - 6, 682);
				if (const ModelP *now = NextModel(r, o, 8, p, move))
				{
					sy = Lerp16(p->w12, now->w12, num, den);
					if (p->life >= 6) fade = lerp_i(fade, mul32(now->life - 6, 682), num, den);
				}
				BladeDraw(w, h, p, sy, fade);
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
	}

	// ---- streak ----
	static uint8_t g_streak_vtx[0x18 * 11];

	static void StreakHeld(const HRec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		const int32_t c = d.c;
		const int32_t count = c >= 0xB ? 0xB : c + 1;
		int16_t width = d.w1C;
		int32_t fade = shl32(c - 0x20, 9);
		if (num != 0 && !g_tick_paused && Alive(r))
		{
			width = Lerp16(d.w1C, r.node->w1C, num, den);
			if (c >= 0x20) fade = lerp_i(fade, shl32(c + 1 - 0x20, 9), num, den);
		}
		Replay(r, false);
		StreakWork *w = (StreakWork *)FieldAlloc(0x90);
		memcpy(g_streak_vtx, (const void *)STREAK_VTX, sizeof(g_streak_vtx));
		StreakBuild(w, count, width, g_streak_vtx);
		StreakDraw(w, count, c, fade, g_streak_vtx);
		FieldFree(0x90);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		magc::HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			switch (r.kind)
			{
			case REC_TRAIL: TrailHeld(r, num, den); break;
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_POOL: PoolHeld(r, num, den); break;
			case REC_STREAK: StreakHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag163_held()
	{
		mark_held(rough163::ORIG_Master);
		mark_held(rough163::ORIG_Trail);
		mark_held(rough163::ORIG_Glow);
		mark_held(rough163::ORIG_Sparkle);
		mark_held(rough163::ORIG_HitSpark);
		mark_held(rough163::ORIG_Shard);
		mark_held(rough163::ORIG_Chip);
		mark_held(rough163::ORIG_Streak);
		mark_held(rough163::ORIG_Flash);
		mark_held(rough163::ORIG_Burst);
		mark_held(rough163::ORIG_Pillar);
		mark_held(rough163::ORIG_Blade);
		mark_held(rough163::ORIG_Ring);
		mark_held(rough163::ORIG_Shock);
		mark_held(rough163::ORIG_Flare);
		mark_held(rough163::ORIG_Wave);
		mark_held(rough163::ORIG_Debris);
		register_module_held(163, rough163::HeldReady, rough163::HeldFrame);
	}
}
