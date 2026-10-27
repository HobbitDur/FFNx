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

// 30 fps layer of Renzokuken vs Elvoret (effect 161), included at the end of mag161_renzokuken_elvoret.cpp.
// The same design as effect 160's (mag160_renzokuken_held.inc) on this module's globals, plus its own ring and two pools.
//
// Every drawing task of the effect queue draws THEN updates. A real tick records its draws in order (the master's
// seam starts the log): the node / record as drawn, the GTE state the draw started with and the Field_Alloc scratch
// under its draw header (fields the draw does not set keep what the scratch held). A held frame replays the log into a
// private packet buffer, each draw at its exact in-between state (logic stays 15 Hz):
//   Trail - the next tick's ring is predicted exactly: the weapon's bone 1 at the NEXT pose (pose_midpoint at 1 of the
//     weapon model, the engine's own reader one frame ahead; the real pose rebuilt afterwards) through the caster's
//     matrix of the next tick (a moving caster is drawn at its in-between position on held frames: the frame gate
//     moves it, so its next translation is tick N's + (now - tick N's) * den / num; held when it did not move), the
//     base / tip into slot counter % 7, the newest min(7, length - counter) entries splined on private buffers; each of
//     the 2 x 32 spline points is drawn half way between the real tick's and the next tick's (the quads' colours are
//     per index: fixed).
//     Holds the real tick's splines when the trail ends on this tick, when the tick was draw-only, or when the next
//     tick would draw nothing.
//   Slash - scale and the 16 streak offsets half way to the values the update left (the next draw's); the fade half way
//     along its ramp from tick 2 on (before that the next tick switches the draw mode: held); angles / widths fixed.
//   Final ring - scale half way (x / z), the fade half way from tick 4 on (mode switch before: held).
//   Particles - the five pools as drawn (snapshot before the update), positions / scales half way to the post-update
//     values of the particles that moved (debris: position, scale; stream: position, scale; burst: x / z, scale;
//     sparks: y); flipbook frames (life) and the spark frame switch on the real frame; a particle that died on the
//     tick (its sequence ended, a stream particle below the ground) holds its last picture; particles born on the tick
//     (random) appear on the real frame.
// The hit-effect queue tasks (hit sparks / sparkles, 0x5FD810 / 0x5FDAA0) are not drawn here: the frame gate replays
// the hit-effect queue itself. The caster's and the boss's animations, positions and the caster's tint are drawn by the
// engine; the camera shake word (0x1D97712, ticks 0x2E..0x32) is the camera's (the gate's held camera).
// Nothing here writes game state: the gate puts back the GTE registers and the scratch; the weapon's bone matrices
// are rebuilt from its real pose after the look-ahead.

namespace ff8fx
{
namespace renzo161
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_packets = 0;        // the module's packet cursor when the tick started drawing
	static uint8_t g_held_packets[0x10000];

	enum RecKind { REC_TRAIL, REC_SLASH, REC_FINAL, REC_PARTICLES };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct Rec
	{
		int kind;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		HitRec hit;                // slash: its hit record as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated
	};
	static const int REC_MAX = 32;
	static Rec g_rec[REC_MAX];
	static int g_nrec = 0;
	// trail: the splines of the real tick, the caster's matrix it was sampled with
	static uint8_t g_trail_sa[0x100], g_trail_sb[0x100];
	static Mat4x3 g_trail_mat;
	// particles: the pools as drawn (before their update)
	static uint8_t g_pool_a[POOL_A_END - POOL_A], g_pool_b[POOL_B_END - POOL_B], g_pool_c[POOL_C_END - POOL_C];
	static uint8_t g_pool_d[POOL_D_END - POOL_D], g_pool_e[POOL_E_END - POOL_E];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_packets = PacketCursor();
		g_nrec = 0;
	}

	static Rec *NoteRec(int kind, const Node24 *n, bool header)
	{
		magc::g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = n;
		if (n) memcpy(&r.copy, n, sizeof(Node24));
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		if (header) magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_trail(const Node24 *n)
	{
		if (!NoteRec(REC_TRAIL, n, false)) return;
		memcpy(g_trail_sa, (const void *)SPL_A, sizeof(g_trail_sa));
		memcpy(g_trail_sb, (const void *)SPL_B, sizeof(g_trail_sb));
		g_trail_mat = *(const Mat4x3 *)(Caster() + 0x40);
	}

	static void held_note_slash(const Node24 *n, const HitRec *r)
	{
		if (Rec *rec = NoteRec(REC_SLASH, n, true)) memcpy(&rec->hit, r, sizeof(HitRec));
	}

	static void held_note_final(const Node24 *n) { NoteRec(REC_FINAL, n, true); }

	static void held_note_particles()
	{
		if (!NoteRec(REC_PARTICLES, nullptr, true)) return;
		memcpy(g_pool_a, (const void *)POOL_A, sizeof(g_pool_a));
		memcpy(g_pool_b, (const void *)POOL_B, sizeof(g_pool_b));
		memcpy(g_pool_c, (const void *)POOL_C, sizeof(g_pool_c));
		memcpy(g_pool_d, (const void *)POOL_D, sizeof(g_pool_d));
		memcpy(g_pool_e, (const void *)POOL_E, sizeof(g_pool_e));
	}

	// a held draw starts like the real one: its GTE state and header scratch
	static void Replay(const Rec &r, bool header)
	{
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = header ? (uint8_t *)r.hdr : nullptr;
	}

	// the node still runs the same task on the next tick (it did not end on this one)
	static bool Alive(const Node24 *n, uint32_t fn) { return (n->hdr.flags & 1) != 0 && (uint32_t)n->hdr.func == fn; }

	static inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return magc::lerp16(a, b, num, den); }

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
		for (int k = 0; k < 3; k++) em->t[k] = g_trail_mat.t[k] + (int32_t)((int64_t)(now.t[k] - g_trail_mat.t[k]) * den / num);
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
		const int32_t want = c < Length() - 7 ? 7 : Length() - c;
		const int32_t cnt = TrailGather(g_ring, idx, want, g_pa, g_pb);
		if (cnt <= 1) return false;
		TrailSplines(cnt, g_pa, g_pb, g_na, g_nb);
		return true;
	}

	static void TrailHeld(const Rec &r, int num, int den)
	{
		const uint8_t *sa = g_trail_sa, *sb = g_trail_sb;
		if (num != 0 && !g_tick_paused && Alive(r.node, ORIG_Trail) && TrailNext(r.node, num, den))
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

	// ---- slash: the draw of SlashTask without its updates ----
	static void SlashDraw(const HitRec *r, int16_t scale, int32_t c, int32_t fade)
	{
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)SLASH_REF, r->dir, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = r->pos[0];
		m.t[1] = r->pos[1];
		m.t[2] = r->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m3;
		BuildRotationMatrixFromAngles(ang, &m3);
		int32_t sv[3];
		m3.t[0] = 0;
		m3.t[1] = 0;
		m3.t[2] = 0;
		sv[1] = scale;
		sv[0] = scale;
		sv[2] = 0x1000;
		Scale3DMatrix(&m3, sv);
		ComposeAffineTransform(&m, &m3, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_SLASH;
		*(uint32_t *)(h + 0x1C) = 0x30;
		if (c >= 2)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		*(uint32_t *)h = MODEL_STREAK;
		*(uint32_t *)(h + 0x1C) |= 3;
		for (int32_t k = 0; k < r->count; k++)
		{
			ang[2] = r->angle[k];
			BuildRotationMatrixFromAngles(ang, &m3);
			int16_t v[4];
			v[3] = 0;
			v[2] = 0;
			v[0] = 0;
			v[1] = r->off[k];
			GteLoadV0(v);
			GteSetRotMatrix(&m3);
			GteMVMVA_RotV0();
			GteReadMAC123(m3.t);
			const int32_t wd = r->width[k];
			sv[1] = wd;
			sv[0] = (wd >> 1) + wd;
			Scale3DMatrix(&m3, sv);
			ComposeAffineTransform(&m, &m3, &m3);
			GteSetRotMatrix(&m3);
			GteSetTransVector(&m3);
			PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x58);
	}

	static void SlashHeld(const Rec &r, int num, int den)
	{
		static HitRec hit;
		hit = r.hit;
		const Node24 &d = r.copy;
		int16_t scale = d.w1C;
		int32_t fade = mul32(d.c - 2, 682);
		if (num != 0 && !g_tick_paused && Alive(r.node, ORIG_Slash))
		{
			const HitRec *now = (const HitRec *)(HIT_REC + 0x9C * d.e);
			scale = Lerp16(d.w1C, r.node->w1C, num, den);
			for (int k = 0; k < 16; k++) hit.off[k] = Lerp16(r.hit.off[k], now->off[k], num, den);
			if (d.c >= 2) fade = lerp_i(fade, mul32(r.node->c - 2, 682), num, den);
		}
		Replay(r, true);
		SlashDraw(&hit, scale, d.c, fade);
	}

	// ---- final ring: the draw of FinalTask without its update ----
	static void FinalDraw(const Node24 *n, int16_t scale, int32_t fade)
	{
		int16_t a[3] = { 0, 0, 0 };
		Mat4x3 m;
		BuildRotationMatrixFromAngles(a, &m);
		int32_t sv[3];
		sv[1] = 0x1000;
		m.t[1] = n->w12;
		m.t[0] = n->w10;
		sv[2] = scale;
		sv[0] = scale;
		m.t[2] = n->w14;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_RING;
		*(uint32_t *)(h + 0x1C) = 3;
		if (n->c >= 4)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xC3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void FinalHeld(const Rec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		int16_t scale = d.w1C;
		int32_t fade = mul32(d.c - 4, 682);
		if (num != 0 && !g_tick_paused && Alive(r.node, ORIG_Final))
		{
			scale = Lerp16(d.w1C, r.node->w1C, num, den);
			if (d.c >= 4) fade = lerp_i(fade, mul32(r.node->c - 4, 682), num, den);
		}
		Replay(r, true);
		FinalDraw(&d, scale, fade);
	}

	// ---- particles: the draws of ParticleDraws from the pools as drawn ----
	static void ParticlesHeld(const Rec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		Replay(r, true);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_FLASH;
		*(int16_t *)(h + 0x24) = 0;
		for (uint32_t o = 0; o < sizeof(g_pool_a); o += 0xC)
		{
			const FlashP *p = (const FlashP *)(g_pool_a + o);
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		*(uint32_t *)h = SEQ_DEBRIS;
		for (uint32_t o = 0; o < sizeof(g_pool_c); o += 0x14)
		{
			const DebrisP *p = (const DebrisP *)(g_pool_c + o);
			if (p->life < 0) continue;
			const DebrisP *now = (const DebrisP *)(POOL_C + o);
			int16_t pos[4] = { p->pos[0], p->pos[1], p->pos[2], p->pos[3] };
			int16_t scale = p->scale;
			if (move && now->life == p->life + 1)
			{
				scale = Lerp16(p->scale, now->scale, num, den);
				for (int k = 0; k < 3; k++) pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
			}
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(pos, scale, -(scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		*(uint32_t *)h = SEQ_STREAM;
		for (uint32_t o = 0; o < sizeof(g_pool_d); o += 0x14)
		{
			const StreamP *p = (const StreamP *)(g_pool_d + o);
			if (p->life < 0) continue;
			const StreamP *now = (const StreamP *)(POOL_D + o);
			int16_t pos[4] = { p->pos[0], p->pos[1], p->pos[2], p->pos[3] };
			int16_t scale = p->scale;
			if (move && now->life == p->life + 1)
			{
				scale = Lerp16(p->scale, now->scale, num, den);
				for (int k = 0; k < 3; k++) pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
			}
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(pos, scale, -(scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		*(void **)h = EffectSpriteSharedSequence(SHARED_SEQ_BURST);
		for (uint32_t o = 0; o < sizeof(g_pool_e); o += 0x14)
		{
			const BurstP *p = (const BurstP *)(g_pool_e + o);
			if (p->life < 0) continue;
			const BurstP *now = (const BurstP *)(POOL_E + o);
			int16_t pos[4] = { p->pos[0], p->pos[1], p->pos[2], p->pos[3] };
			int16_t scale = p->scale;
			if (move && now->life == p->life + 1)
			{
				scale = Lerp16(p->scale, now->scale, num, den);
				pos[0] = Lerp16(p->pos[0], now->pos[0], num, den);
				pos[2] = Lerp16(p->pos[2], now->pos[2], num, den);
			}
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(pos, scale, -(scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		*(uint32_t *)h = SEQ_SPARK;
		*(int16_t *)(h + 0x24) = 4;
		for (uint32_t o = 0; o < sizeof(g_pool_b); o += 0x10)
		{
			const SparkP *p = (const SparkP *)(g_pool_b + o);
			if (p->life < 0) continue;
			const SparkP *now = (const SparkP *)(POOL_B + o);
			int16_t pos[3] = { p->pos[0], p->pos[1], p->pos[2] };
			if (move && now->life == p->life + 1) pos[1] = Lerp16(p->pos[1], now->pos[1], num, den);
			*(int16_t *)(h + 4) = p->life;
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(SPARK_FRAMES + 4 * p->frame);
			TransformCameraByShadowRotation(pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		magc::HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			switch (r.kind)
			{
			case REC_TRAIL: TrailHeld(r, num, den); break;
			case REC_SLASH: SlashHeld(r, num, den); break;
			case REC_FINAL: FinalHeld(r, num, den); break;
			case REC_PARTICLES: ParticlesHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag161_held()
	{
		mark_held(renzo161::ORIG_Master);
		mark_held(renzo161::ORIG_Trail);
		mark_held(renzo161::ORIG_Slash);
		mark_held(renzo161::ORIG_Final);
		register_module_held(161, renzo161::HeldReady, renzo161::HeldFrame);
	}
}
