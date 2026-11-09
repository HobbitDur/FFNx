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

// 30 fps layer of the boss Renzokuken modules' common part (renzo_boss.inc), included INSIDE the module's namespace by the
// module's _held.inc. The 160 / 332 layer (mag332_renzokuken_7hits_held.inc): every drawing task draws THEN updates; a
// real tick records its draws in order (the master's seam starts the log): the node / record as drawn, the GTE state the
// draw started with and the Field_Alloc scratch under its draw header. A held frame replays the log into a private packet
// buffer, each draw at its exact in-between state (logic stays 15 Hz):
//   Trail - the next tick's ring predicted exactly (the weapon's bone 1 at the NEXT pose: pose_midpoint at 1 of the weapon
//     model, the engine's own reader one frame ahead, the real pose rebuilt afterwards; the caster's matrix of the next
//     tick: a moving caster is drawn at its in-between position on held frames, so its next translation is tick N's +
//     (now - tick N's) * den / num); every spline point half way. Held when the trail ends on this tick, when the tick
//     was draw-only, or when the next tick would draw nothing.
//   Slash - scale and the 16 streak offsets half way; the fade half way from tick 2 on (mode switch before: held).
//   Particles - the three pools as drawn, positions / scales half way to the post-update values of the particles that
//     moved; flipbook frames and the spark frame switch on the real frame; a particle that died holds its last picture.
//   Slot models (SLOTMODELS of the module, RENZO_HAS_SLOTMODELS) - scales half way (sep_y: the y scale, x / z fixed), the
//     fade half way from its first fading tick (mode switch before: held); the record (point, direction) as drawn.
//   Node models (NODEMODELS, RENZO_HAS_NODEMODELS) - scale half way, the fade half way from its first fading tick (mode
//     switch before: held); a caster model takes the caster's matrix of the held frame (read by the draw, as on a tick).
//   Dust - its draw replayed from the kind-4 SLOTS records as drawn (a snapshot at its draw) through the held frame's
//     camera (the draw builds its matrix from the camera matrix itself) at the node's x / z; x / z half way to the
//     post-update values of the records that moved (life + 1); sequence frames on the real frame, births on the real
//     tick; a record freed on the tick holds its last picture.
//   Emitters - each emitter's draw replayed from the SLOTS records of its kind as drawn (a snapshot at its draw) with the
//     camera of the held frame (the draw sets its light matrix / back colour from the camera matrix itself); positions /
//     scales half way to the post-update values of the records that moved (life + 1, same kind); sequence frames on the
//     real frame, births on the real tick; a record freed on the tick holds its last picture.
// The module's own tasks: kinds from REC_MODULE, drawn by its ModuleHeld. The hit-effect queue tasks (hit sparks /
// sparkles) are not drawn here: the frame gate replays that queue itself. Nothing here writes game state.
// The module declares before the include: static void ModuleHeld(const Rec &, int, int) is declared here (define it
// after the include).

	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_packets = 0;        // the module's packet cursor when the tick started drawing
	static uint8_t g_held_packets[0x10000];

	enum RecKind { REC_TRAIL, REC_SLASH, REC_PARTICLES, REC_EMIT, REC_SLOTMODEL, REC_DUST, REC_NODEMODEL, REC_MODULE };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct Rec
	{
		int kind;
		int sub;                   // emitters: their SLOTS snapshot / module kinds: their own index
		int32_t kind_arg;          // emitters: the kind drawn
		uint32_t seq_arg;          // emitters: the sequence
		const Node24 *node;
		Node24 copy;               // the node as drawn
		HitRec hit;                // slash: its hit record as drawn
		SlotRec slot;              // slot models: their record as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated
	};
	static const int REC_MAX = 48;
	static Rec g_rec[REC_MAX];
	static int g_nrec = 0;
	// trail: the splines of the real tick, the caster's matrix it was sampled with
	static uint8_t g_trail_sa[0x100], g_trail_sb[0x100];
	static Mat4x3 g_trail_mat;
	// particles: the pools as drawn (before their update)
	static uint8_t g_pool_a[POOL_A_END - POOL_A], g_pool_b[POOL_B_END - POOL_B], g_pool_c[POOL_C_END - POOL_C];

	static void ModuleHeld(const Rec &r, int num, int den);

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
		r.sub = 0;
		r.kind_arg = 0;
		r.seq_arg = 0;
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

	static void held_note_particles()
	{
		if (!NoteRec(REC_PARTICLES, nullptr, true)) return;
		memcpy(g_pool_a, (const void *)POOL_A, sizeof(g_pool_a));
		memcpy(g_pool_b, (const void *)POOL_B, sizeof(g_pool_b));
		memcpy(g_pool_c, (const void *)POOL_C, sizeof(g_pool_c));
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

	// ---- emitters: the draw of EmitterDraw from the SLOTS records of its kind as drawn ----
	static const int EMIT_MAX = 8;  // (342: up to 6 hit emitters at once)
	static uint8_t g_slots[EMIT_MAX][SLOTS_END - SLOTS]; // SLOTS as each emitter drew it

	static void held_note_emitter(const Node24 *n, int32_t kind, uint32_t seq)
	{
		int k = 0;
		for (int i = 0; i < g_nrec; i++) if (g_rec[i].kind == REC_EMIT) k++;
		if (k >= EMIT_MAX) return;
		Rec *r = NoteRec(REC_EMIT, n, true);
		if (!r) return;
		r->sub = k;
		r->kind_arg = kind;
		r->seq_arg = seq;
		memcpy(g_slots[k], (const void *)SLOTS, sizeof(g_slots[k]));
	}

	static void EmitHeld(const Rec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		Replay(r, true);
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		uint8_t *h = AllocHeader(0xB4);
		*(int16_t *)(h + 0x24) = 0;
		memcpy(&w->cam, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = r.seq_arg;
		GteSetLightMatrix(&w->cam);
		GteSetBackColorFromTrans(&w->cam);
		const int32_t kind = r.kind_arg;
		for (uint32_t o = 0; o < sizeof(g_slots[0]); o += 0x20)
		{
			const SlotRec *p = (const SlotRec *)(g_slots[r.sub] + o);
			if (p->alive != kind) continue;
			const SlotRec *now = (const SlotRec *)(SLOTS + o);
			SlotRec q = *p;
			if (move && now->alive == kind && now->life == p->life + 1)
			{
				q.scale = Lerp16(p->scale, now->scale, num, den);
				for (int k = 0; k < 3; k++) q.pos[k] = Lerp16(p->pos[k], now->pos[k], num, den);
			}
			EmitDraw(w, h, &q);
		}
		FieldFree(0xB4);
		FieldFree(0x50);
	}

	// ---- slot models: the draw of SlotModelTask without its update ----
	static void held_note_slotmodel(int kind, const Node24 *n)
	{
#ifdef RENZO_HAS_SLOTMODELS
		if (Rec *r = NoteRec(REC_SLOTMODEL, n, true))
		{
			r->sub = kind;
			memcpy(&r->slot, Slot(n->e), sizeof(SlotRec));
		}
#else
		(void)kind; (void)n;
#endif
	}

#ifdef RENZO_HAS_SLOTMODELS
	static void SlotModelHeld(const Rec &r, int num, int den)
	{
		const SlotModelDef &d = SLOTMODELS[r.sub];
		const Node24 &c = r.copy;
		int32_t sx = SlotModelSX(d, &c), sy = SlotModelSY(d, &c);
		int32_t fade = SlotModelFade(d, c.c);
		if (num != 0 && !g_tick_paused && Alive(r.node, d.orig))
		{
			sx = Lerp16((int16_t)sx, (int16_t)SlotModelSX(d, r.node), num, den);
			sy = d.sep_y ? Lerp16((int16_t)sy, (int16_t)SlotModelSY(d, r.node), num, den) : sx;
			if (c.c >= d.fade_from) fade = lerp_i(fade, SlotModelFade(d, r.node->c), num, den);
		}
		Replay(r, true);
		SlotModelDraw(d, &c, &r.slot, sx, sy, fade);
	}
#endif

	// ---- dust: the draw of DustRun from the kind-4 SLOTS records as drawn ----
	static uint8_t g_dust[SLOTS_END - SLOTS]; // SLOTS as the dust drew it

	static void held_note_dust(const Node24 *n)
	{
		if (NoteRec(REC_DUST, n, true)) memcpy(g_dust, (const void *)SLOTS, sizeof(g_dust));
	}

	static void DustHeld(const Rec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		Replay(r, true);
		DustWork *w = (DustWork *)FieldAlloc(0x48);
		uint8_t *h = AllocHeader(0xB4);
		DustBegin(w, h, &r.copy);
		for (uint32_t o = 0; o < sizeof(g_dust); o += 0x20)
		{
			const SlotRec *p = (const SlotRec *)(g_dust + o);
			if (p->alive != 4) continue;
			const SlotRec *now = (const SlotRec *)(SLOTS + o);
			SlotRec q = *p;
			if (move && now->alive == 4 && now->life == p->life + 1)
			{
				q.pos[0] = Lerp16(p->pos[0], now->pos[0], num, den);
				q.pos[2] = Lerp16(p->pos[2], now->pos[2], num, den);
			}
			DustDraw(w, h, &q);
		}
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	// ---- node models (NODEMODELS of the module, RENZO_HAS_NODEMODELS): the draw of NodeModelTask without its update ----
	static void held_note_nodemodel(int kind, const Node24 *n)
	{
#ifdef RENZO_HAS_NODEMODELS
		if (Rec *r = NoteRec(REC_NODEMODEL, n, true)) r->sub = kind;
#else
		(void)kind; (void)n;
#endif
	}

#ifdef RENZO_HAS_NODEMODELS
	static void NodeModelHeld(const Rec &r, int num, int den)
	{
		const NodeModelDef &d = NODEMODELS[r.sub];
		const Node24 &c = r.copy;
		int32_t scale = c.w1C, sy = d.sep_y ? c.w20 : c.w1C;
		int32_t fade = NodeModelFade(d, c.c);
		if (num != 0 && !g_tick_paused && Alive(r.node, d.orig))
		{
			scale = Lerp16(c.w1C, r.node->w1C, num, den);
			sy = d.sep_y ? Lerp16(c.w20, r.node->w20, num, den) : scale;
			if (c.c >= d.fade_from) fade = lerp_i(fade, NodeModelFade(d, r.node->c), num, den);
		}
		Replay(r, true);
		NodeModelDraw(d, &c, scale, sy, fade);
	}
#endif

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
			case REC_PARTICLES: ParticlesHeld(r, num, den); break;
			case REC_EMIT: EmitHeld(r, num, den); break;
			case REC_DUST: DustHeld(r, num, den); break;
#ifdef RENZO_HAS_NODEMODELS
			case REC_NODEMODEL: NodeModelHeld(r, num, den); break;
#endif
#ifdef RENZO_HAS_SLOTMODELS
			case REC_SLOTMODEL: SlotModelHeld(r, num, den); break;
#endif
			default: ModuleHeld(r, num, den); break;
			}
		}
	}

	// the common tasks with a held part (the module marks its own)
	static void MarkHeldCommon()
	{
		mark_held(ORIG_Master);
		mark_held(ORIG_Trail);
		mark_held(ORIG_Slash);
	}
