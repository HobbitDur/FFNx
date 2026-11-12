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

// Squall's boss-specific Renzokuken modules: their common part, second half (the tasks every one of them has, byte for
// byte the same code on the module's own addresses). Included INSIDE the module's namespace by the module file after
// renzo_boss_defs.inc and the module's _held.h, with
//   static void Events(Node24 *n);      the module's own choreography: called by the controller every tick after the
//                                       hit-spark start, before the screen flash (forward declaration is enough)
//   RENZO_ID / RENZO_IDS                the effect id (number / string, for the port names)
//   RENZO_HITSPARK_NULLCHECK            (343) the hit spark tests AddTaskToQueue's result
//   RENZO_CUSTOM_BIND                   the module has its own BindBoth (same arguments)
//   RENZO_LENGTH_EXTRA                  (341 / 342) static int32_t LengthExtra(): added to the length on tick 2
//   RENZO_LATE_EVENTS                   (341) static void LateEvents(Node24 *n): after the sound, before the counter step
//   RENZO_CUSTOM_START                  (339 / 340) the module's static void Start(Node24 *n) replaces the controller's ticks 1 / 2
//                                       (binds, chain starts, ComputeLength, StartTasks)
// Tasks: Controller (the choreography frame: caster flag 0x800, both animation sets, length, trail / spark spawner / hit
// scheduler, hit spark, Events, flash, tint, sound, the end), HitSpark / Sparkle (hit-effect queue, with a finisher), Trail,
// SparkSpawn, Hit, Slash, Master and its ParticleDraws, AddSlotTask (the module's SLOTS records).
// Fujin's programs also run the file loader and the setup (FileLoader / Setup: the game runs its own) and read the
// parameters (P(): the module's GAME unless edited); Port() notes a port for them and registers it.

	// ------------------------------------------------------------------
	// Parameters: the live ones (the module's GAME until edited) and the data they name (an edited one when it reads as
	// one, else the game's)
	// ------------------------------------------------------------------
	static RenzoBossParams live = GAME;
	static const RenzoBossParams &P() { return live; }
	// a module's own model (by its game address: the parameter in its own[] slot)
	static uint32_t OwnModel(uint32_t game)
	{
		for (int k = 0; k < 4; k++)
			if (GAME.models.own[k] == game) return fxp::model(P().models.own[k], game);
		return game;
	}
	// the emitters' sprite sequence (by its game address)
	static uint32_t EmitSeq(uint32_t game) { return game == GAME.seqs.emit ? fxp::sequence(P().seqs.emit, game) : game; }
	// the hit offsets (8 bytes a hit): the parameter, or the module's own zero table (GAME 0)
	static uint32_t HitOffs() { return GAME.tables.hit_offs ? fxp::data(P().tables.hit_offs, GAME.tables.hit_offs, 96) : own::R(HIT_OFFS); }
	static uint32_t AnimSets() { return fxp::anim_set(P().res.anim_sets, ANIM_SETS); }

	// ------------------------------------------------------------------
	// File loader and setup (not a task): the game runs its own; Fujin's programs these (their memory may be a copy's):
	// the texture file; the magic buffer, the cast, its first target and its caster, the master and the controller, the
	// length (4 until the sequence's tick 2), the finisher flag, the three particle pools and SLOTS freed, the camera, the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = (uint32_t)layouts::MagicBuffer();
		var<CastContext *>(own::R(CONTEXT)) = ctx;
		TargetSlot() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(QMaster(), (void *)own::R(MASTER_POOL), 0x10, 1);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_Master)) + 0xC) = 0;
		InitTaskQueuePool(QMain(), (void *)own::R(MAIN_POOL), 0x24, 100);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(QMain(), own::Fn(ORIG_Controller)) + 0xC) = 0;
		Length() = 4;
		Finisher() = *(const uint16_t *)((const uint8_t *)ctx->actions + 4) == 0xFFFE;
		for (uint32_t a = own::R(POOL_A); a < own::R(POOL_A_END); a += 0xC) *(int16_t *)a = -1;
		for (uint32_t a = own::R(POOL_C); a < own::R(POOL_C_END); a += 0x14) *(int16_t *)a = -1;
		for (uint32_t a = own::R(POOL_B); a < own::R(POOL_B_END); a += 0x10) *(int16_t *)a = -1;
		for (uint32_t a = own::R(SLOTS); a < own::R(SLOTS_END); a += 0x20) *(int32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA));
		QueueTimUpload(TexFile());
		return QMaster();
	}

	// ------------------------------------------------------------------
	// BindBoth: the animation container's two com-data blocks bound to the caster and to the boss, both sequences
	// started (the caster's `seq`, the boss's 1)
	// ------------------------------------------------------------------
#ifndef RENZO_CUSTOM_BIND
	static void BindBoth(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		const uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		QueueChainTransformation(caster, seq);
		QueueChainTransformation(boss, 1);
	}
#endif

	// first free entry (life < 0) of a pool, or -1 (none in the scanned part / index >= 0x28)
	static int32_t FreeSlot(uint32_t pool, uint32_t end, uint32_t stride)
	{
		int32_t i = 0;
		for (uint32_t a = pool; *(const int16_t *)a >= 0; i++)
		{
			a += stride;
			if (a >= end) return -1;
		}
		return i >= 0x28 ? -1 : i;
	}

	// the first free SLOTS record (alive 0), or -1 (none / index >= 100)
	static int32_t FreeRecord()
	{
		int32_t i = 0;
		for (uint32_t a = own::R(SLOTS); *(const int32_t *)a != 0; i++)
		{
			a += 0x20;
			if (a >= own::R(SLOTS_END)) return -1;
		}
		return i >= 100 ? -1 : i;
	}

	// AddSlotTask (the module's au_re_BdLinkTask_N): a task in the main queue with its own SLOTS record (marked -1, its
	// index in the node +0xE, counter 0); nullptr when every record is taken (no task then)
	static Node24 *AddSlotTask(uint32_t orig)
	{
		const int32_t i = FreeRecord();
		if (i < 0) return nullptr;
		Slot(i)->alive = -1;
		Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(orig));
		t->e = (int16_t)i;
		t->c = 0;
		return t;
	}

	// the length: the n + 1 stage durations of the sequence's opcode AB
	static void ComputeLength()
	{
		const int32_t last = (int32_t)StageCount() + 1;
		Length() = 0;
		for (int32_t i = 1; i <= last; i++) Length() = Length() + var<uint8_t>(0x1D98214 + i);
	}

	// the trail (its ring cleared), the spark spawner (frame phase rand % 24) and the hit scheduler
	static void StartTasks()
	{
		Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_Trail));
		t->c = 0;
		for (uint32_t a = own::R(RING); a < own::R(RING_END); a += 0x14) var<int32_t>(a) = 0;
		t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_SparkSpawn));
		t->c = 0;
		t->e = (int16_t)(CrtRand() % 0x18);
		t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_Hit));
		t->c = 0;
	}

	// ------------------------------------------------------------------
	// Controller: the choreography frame (the module's own events: Events)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ControllerTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			uint16_t *flags = (uint16_t *)Caster();
			const uint16_t f = *flags;
			n->e = (int16_t)(f & 0x800);
			*flags = (uint16_t)(f | 0x800);
		}
#ifndef RENZO_CUSTOM_START
		if (n->c == 1) BindBoth(Caster(), Target(), BIND_OUT, AnimSets(), (Finisher() != 0) + 1);
		if (n->c == 2)
		{
			ComputeLength();
#ifdef RENZO_LENGTH_EXTRA
			Length() = Length() + LengthExtra(); // (341 / 342: the length runs past the sequence's stages)
#endif
			StartTasks();
		}
#else
		Start(n); // (339 / 340: their own binds / chain starts / length tick)
#endif
		if (Finisher() != 0 && (int32_t)n->c == Length())
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QHit(), own::Fn(ORIG_HitSpark));
			t->c = 0;
			t->w1A = (int16_t)CasterSlot();
		}
		Events(n);
		// screen flash: up over ticks 0..0x10, down over the last 8 (not before a finisher)
		if (n->c <= 0x10)
			SetScreenFlash((uint32_t)shl32(n->c, 7), 0);
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4 && Finisher() == 0)
			SetScreenFlash((uint32_t)shl32(Length() - n->c, 8), 0);
		// the caster's model tint: toward the far colour over ticks 0..8, back over the last 8
		bool tint = false;
		int32_t k = 0;
		if (n->c <= 8)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = n->c;
			tint = true;
		}
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = Length() - n->c;
			tint = true;
		}
		if (tint)
		{
			uint32_t colour = var<uint32_t>(0xB8B7D8); // (the original keeps it in a local)
			GteLoadRGBC(&colour);
			GteSetIR0(shl32(k, 9));
			GteDPCS();
			GteStoreRGB2(Caster() + 0x28);
		}
		if (n->c == 1) BdPlaySE((const void *)fxp::sound(P().sound, (uint32_t)SOUND), 0x8000, 0x80);
#ifdef RENZO_LATE_EVENTS
		LateEvents(n); // (341: the caster's flag 8 around the finisher)
#endif
		n->c++;
		if ((int32_t)n->c <= Length()) return 0;
		if (Finisher() == 0) SetScreenFlash(0, 0);
		*(uint32_t *)(Caster() + 0x28) = var<uint32_t>(0xB8B7D8);
		uint16_t *flags = (uint16_t *)Caster();
		*flags = (uint16_t)((*flags & 0xF7FF) | (uint16_t)n->e);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Hit sparks (hit-effect queue): 6 sparkles per tick on ticks 4..8
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitSparkTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c >= 4 && n->c <= 8)
		{
			// the entity's default effect position with its height and y swapped: x, height, z, y
			int16_t p[4];
			GetDefaultEffectPosition(Entity(n->w1A), p);
			const int16_t h = p[3];
			p[3] = p[1];
			p[1] = h;
			for (int32_t i = 0; i < 6; i++)
			{
				Node24 *s = (Node24 *)AddTaskToQueue(QHit(), own::Fn(ORIG_Sparkle));
#ifdef RENZO_HITSPARK_NULLCHECK
				if (!s) continue; // (343 tests the queue's answer)
#endif
				s->c = 0;
				s->e = (int16_t)(CrtRand() % 6 + i);
				const uint8_t *sk = **(uint8_t ***)(Entity(n->w1A) + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				int16_t q[4];
				GetEffectSpawnPosition(Entity(n->w1A), bone, 0, q);
				q[0] = (int16_t)(q[0] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[1] = (int16_t)(q[1] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[2] = (int16_t)(q[2] + (int16_t)(CrtRand() % 0xAF - 0x57));
				int16_t y = q[1];
				if (y > -600)
				{
					y = (int16_t)(y - 600);
					q[1] = y;
				}
				int32_t v[3] = { (int32_t)q[0] - p[0], (int32_t)y - p[1], (int32_t)q[2] - p[2] };
				Normalize(v, v);
				v[0] = shl32(v[0], 2);
				v[2] = shl32(v[2], 2);
				Normalize(v, v);
				const int32_t len = CrtRand() % 0x258 + 0x15E;
				s->w10 = (int16_t)((mul32(len, v[0]) >> 12) + q[0]);
				s->w12 = (int16_t)((mul32(len, v[1]) >> 12) + q[1]);
				s->w18 = (int16_t)bone;
				s->w14 = (int16_t)((mul32(len, v[2]) >> 12) + q[2]);
				s->w1A = n->w1A;
				s->w1C = (int16_t)(CrtRand() % 8);
				s->w20 = (int16_t)(CrtRand() % 0x680 + 0x80);
			}
		}
		if (n->c == 1) AddAnimSeqTask(Entity(n->w1A), 3);
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparkle (hit-effect queue): one model facing the camera, spinning, stretched toward its point
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		int16_t q[4];
		GetEffectSpawnPosition(Entity(n->w1A), n->w18, shl32(n->c, 9), q);
		int32_t d[3] = { (int32_t)var<int16_t>(0xB8B7F0) - q[0], 0, (int32_t)var<int16_t>(0xB8B7F4) - q[2] };
		int32_t u[3] = { 0, 0, -0x1000 };
		Normalize(d, d);
		int32_t axis[3];
		int32_t angle = RotationBetweenVectors(u, d, axis);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m1; // (pad +0x12 never written in the original: GTE R33 high half only)
		BuildRotationMatrixFromAngles(ang, &m1);
		Mat4x3 m2;
		m1.t[0] = 0xF0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		MatrixMultiply(&m2, &m1);
		// spin about the view direction, 0x40 per tick, its sense from the turn's axis
		int32_t spin = -shl32(n->c, 6);
		if (axis[1] < 0) spin = -spin;
		BuildAxisAngleRotationMatrix(spin, &m2, d);
		MatrixMultiply(&m2, &m1);
		// stretched toward the sparkle's point: up (0, -0x1000, 0) turned onto it, length * 0x1000 / 2000
		int32_t e[3] = { (int32_t)n->w10 - q[0], (int32_t)n->w12 - q[1], (int32_t)n->w14 - q[2] };
		u[0] = 0;
		u[1] = -0x1000;
		u[2] = 0;
		const int32_t len = SqrtInt(Normalize(e, e));
		angle = RotationBetweenVectors(u, e, axis);
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		m2.t[0] = q[0];
		m2.t[2] = q[2];
		int32_t sc[3];
		sc[2] = n->w20;
		sc[0] = n->w20;
		m2.t[1] = q[1];
		sc[1] = shl32(len, 12) / 2000;
		Scale3DMatrix(&m2, sc);
		ComposeAffineTransform(&Camera(), &m2, &m2);
		ComposeAffineTransform(&m2, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = fxp::model(P().models.sparkle, MODEL_SPARKLE);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0xC) = var<uint16_t>(fxp::data(P().tables.sparkle_fade, SPARKLE_FADE, 16) + 2 * n->c);
		*(uint32_t *)(h + 0x18) = var<uint16_t>(fxp::data(P().tables.sparkle_depth, SPARKLE_DEPTH, 16) + 2 * n->w1C);
		*(uint32_t *)(h + 0x1C) = 0x10F3;
		var<uint32_t>(0x1D8E054) = RenderPrimModel(h, RenderOT(), 2, var<uint32_t>(0x1D8E054));
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		n->w1C++;
		if (n->w1C >= 8) n->w1C = 0;
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Trail
	// ------------------------------------------------------------------
	// the weapon point and the tip 666 along the blade into the work block
	static void TrailPoints(TrailWork *w)
	{
		WeaponBone(Caster(), 1, w->dir, w->base);
		w->tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 666) >> 12) + w->base[0]);
		w->tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 666) >> 12) + w->base[1]);
		w->tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 666) >> 12) + w->base[2]);
	}

	// newest-first gather of at most `want` ring entries from slot idx into the two point lists; returns the count
	static int32_t TrailGather(const uint8_t *ring, int32_t idx, int32_t want, uint8_t *pa, uint8_t *pb)
	{
		int32_t cnt = 0;
		if (want <= 0) return 0;
		do
		{
			const RingEntry *e = (const RingEntry *)(ring + 0x14 * idx);
			if (!e->alive) break;
			idx--;
			*(uint32_t *)(pa + 8 * cnt) = e->base[0];
			*(uint32_t *)(pa + 8 * cnt + 4) = e->base[1];
			*(uint32_t *)(pb + 8 * cnt) = e->tip[0];
			*(uint32_t *)(pb + 8 * cnt + 4) = e->tip[1];
			if (idx < 0) idx = 6;
			cnt++;
		} while (cnt < want);
		return cnt;
	}

	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb)
	{
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sa + 8 * k, t / 31);
		SplineSetup(cnt, pb, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sb + 8 * k, t / 31);
		FieldFree(0x190);
	}

	// the previous tick's hit point / direction kept (PREV_HIT: 8 + 16 bytes), then the new hit point (the tip spline's
	// 2nd point) and the unit direction from it to the 1st
	static void TrailHitPoint(const uint8_t *sb, uint8_t *pos, int32_t *dir)
	{
		memcpy((void *)own::R(PREV_HIT), pos, 8);
		memcpy((void *)(own::R(PREV_HIT) + 8), dir, 16);
		*(uint32_t *)pos = *(const uint32_t *)(sb + 8);
		*(uint32_t *)(pos + 4) = *(const uint32_t *)(sb + 0xC);
		dir[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)(sb + 8);
		dir[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sb + 0xA);
		dir[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sb + 0xC);
		Normalize(dir, dir);
	}

	// 30 quad pairs between the two splines: gouraud from the base colour 0x3A903030 fading toward the far colour
	// (black) along the trail, and a copy whose leading edge is 0x3A000000
	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk + 9;
		w->col1 = 0x3A903030;
		w->base_col = 0x3A903030;
		int32_t tacc = 0;
		for (int32_t off = 0; off < 0xF0; off += 8, tacc += 0x1000)
		{
			w->col0 = w->col1;
			pk[0] = 0x8000000;
			GteLoadV012(sa + off, sa + off + 8, sb + off);
			GteRTPT();
			GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb + off + 8);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(&w->otz);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(&w->base_col);
			GteDPCS();
			GteStoreRGB2(&w->col1);
			pk[5] = pk[1] = w->col0;
			pk[7] = pk[3] = w->col1;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk);
			pk2[5] = w->col0;
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = w->col1;
			pk2[4] = pk[4];
			pk2[2] = pk[2];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailPoints(w);
		const int32_t c = n->c;
		const int32_t idx = c % 7;
		RingEntry *e = (RingEntry *)(own::R(RING) + 0x14 * idx);
		e->alive = 1;
		e->base[0] = *(const uint32_t *)&w->base[0];
		e->base[1] = *(const uint32_t *)&w->base[2];
		e->tip[0] = *(const uint32_t *)&w->tip[0];
		e->tip[1] = *(const uint32_t *)&w->tip[2];
		const int32_t want = c < Length() - 7 ? 7 : Length() - c;
		const int32_t cnt = TrailGather((const uint8_t *)own::R(RING), idx, want, (uint8_t *)own::R(PTS_A), (uint8_t *)own::R(PTS_B));
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)own::R(PTS_A), (const uint8_t *)own::R(PTS_B), (uint8_t *)own::R(SPL_A), (uint8_t *)own::R(SPL_B));
			TrailHitPoint((const uint8_t *)own::R(SPL_B), (uint8_t *)own::R(HIT_POS), (int32_t *)own::R(HIT_DIR));
			// 30 fps layer: see renzo_boss_held.inc
			FX_HELD(held_note_trail(n);)
			TrailDraw((const uint8_t *)own::R(SPL_A), (const uint8_t *)own::R(SPL_B), w);
		}
		FieldFree(0x40);
		if (DrawOnly()) return 0;
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark spawner: 3 sparks per tick at random bones of the caster
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkSpawnTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if ((int32_t)n->c < Length() - 8)
		{
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(own::R(POOL_B), own::R(POOL_B_SPAWN_END), 0x10);
				if (i < 0) break;
				SparkP *p = (SparkP *)(own::R(POOL_B) + 0x10 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x300 + 0x400);
				const uint8_t *sk = **(uint8_t ***)(Caster() + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				GetEffectSpawnPosition(Caster(), bone, 0, p->pos);
				p->vy = (int16_t)(CrtRand() % 10 + 2);
				p->frame = (int16_t)((int32_t)n->e % 0x18);
			}
		}
		n->c++;
		n->e++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Hits: per hit, a slash, 3 flashes and 12 debris at the trail's hit point + the hit's offset (HitOffs: zero in most)
	// through the caster's matrix
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		int32_t sum = 0;
		for (int32_t b = 0; b < (int32_t)StageCount(); b++)
		{
			const int32_t lead = var<int32_t>(fxp::data(P().tables.hit_lead, HIT_LEAD, 48, 4) + 4 * b);
			sum += StageLen(b);
			if ((int32_t)n->c != sum - lead) continue;
			Node24 *s = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_Slash));
			s->c = 0;
			s->e = (int16_t)b;
			const int16_t sc = (int16_t)(CrtRand() % 0x280 + 0x400);
			s->w1E = sc;
			s->w1C = sc;
			// the hit point: the offset through the caster's matrix + the trail's hit point (16-bit adds). Its 4th word is a
			// stack word the original never writes (entry esp - 2, the high half of the dword the queue's previous task, the
			// spark spawner, pushed first: the game executor 0x508420's ebx = the count of tasks kept so far in this pass, a
			// small number): 0 (as effects 332 / 333).
			int16_t hp[4];
			MatVec((const Mat4x3 *)(Caster() + 0x40), (const int16_t *)(HitOffs() + 8 * b), hp);
			hp[0] = (int16_t)(hp[0] + var<int16_t>(own::R(HIT_POS)));
			hp[1] = (int16_t)(hp[1] + var<int16_t>(own::R(HIT_POS) + 2));
			hp[2] = (int16_t)(hp[2] + var<int16_t>(own::R(HIT_POS) + 4));
			hp[3] = 0;
			HitRec *r = (HitRec *)(own::R(HIT_REC) + 0x9C * b);
			memcpy(r->pos, hp, 8);
			memcpy(r->dir, (const void *)own::R(HIT_DIR), 16);
			r->count = 0x10;
			for (int32_t k = 0; k < r->count; k++)
			{
				r->angle[k] = (int16_t)(CrtRand() % 0x1000);
				const int32_t wv = CrtRand() % 0x800 + 0x800;
				r->width[k] = (int16_t)wv;
				r->off[k] = (int16_t)((mul32(wv, 1000) - 300) >> 12);
				r->speed[k] = (int16_t)(CrtRand() % 200 + 0x78);
			}
			// 3 flashes at the hit point
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(own::R(POOL_A), own::R(POOL_A_END), 0xC);
				if (i < 0) break;
				FlashP *p = (FlashP *)(own::R(POOL_A) + 0xC * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x600 + 0xA00);
				memcpy(p->pos, hp, 8);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x15E - 0xAF));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x15E - 0xAF));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x15E - 0xAF));
			}
			// 12 debris thrown along the direction (back the other way on hit 3)
			for (int32_t j = 0; j < 0xC; j++)
			{
				const int32_t i = FreeSlot(own::R(POOL_C), own::R(POOL_C_END), 0x14);
				if (i < 0) break;
				DebrisP *p = (DebrisP *)(own::R(POOL_C) + 0x14 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x800 + 0xC00);
				memcpy(p->pos, hp, 8);
				const int32_t sp = CrtRand() % 200 + 0x8C;
				const int32_t *d = (const int32_t *)own::R(HIT_DIR);
				const int32_t r0 = CrtRand() % 200;
				p->vel[0] = (int16_t)(r0 + (mul32(d[0], sp) >> 12) - 0x64);
				const int32_t r1 = CrtRand() % 200;
				p->vel[1] = (int16_t)(r1 + (mul32(d[1], sp) >> 12) - 0x64);
				const int32_t r2 = CrtRand() % 200;
				p->vel[2] = (int16_t)(r2 + (mul32(d[2], sp) >> 12) - 0x64);
				if (b == 3)
				{
					p->vel[0] = (int16_t)-p->vel[0];
					p->vel[1] = (int16_t)-p->vel[1];
					p->vel[2] = (int16_t)-p->vel[2];
				}
			}
#ifdef RENZO_HIT_EXTRA
			HitExtra(b); // (342: its own task per hit)
#endif
		}
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Slash: the flash along the hit direction and its 16 streaks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		HitRec *r = (HitRec *)(own::R(HIT_REC) + 0x9C * n->e);
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_slash(n, r);)
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)SLASH_REF, r->dir, axis);
		Mat4x3 m; // the hit frame through the camera
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
		sv[1] = n->w1C;
		sv[0] = n->w1C;
		sv[2] = 0x1000;
		Scale3DMatrix(&m3, sv);
		ComposeAffineTransform(&m, &m3, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = fxp::model(P().models.slash, MODEL_SLASH);
		*(uint32_t *)(h + 0x1C) = 0x30;
		if (n->c >= 2)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(int32_t *)(h + 0xC) = mul32(n->c - 2, 682);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		*(uint32_t *)h = fxp::model(P().models.streak, MODEL_STREAK);
		*(uint32_t *)(h + 0x1C) |= 3;
		for (int32_t k = 0; k < r->count; k++)
		{
			ang[2] = r->angle[k];
			BuildRotationMatrixFromAngles(ang, &m3);
			int16_t v[4]; // (4th word never written in the original: GTE VZ0 high half only)
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
			if (!DrawOnly())
			{
				r->off[k] = (int16_t)(r->off[k] + r->speed[k]);
				r->speed[k] = (int16_t)(r->speed[k] - (int16_t)(r->speed[k] >> 4));
			}
		}
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - rate / 6);
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Particles: flashes (pool A), debris (pool C), sparks (pool B)
	// ------------------------------------------------------------------
	static void ParticleDraws()
	{
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_particles();)
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = fxp::sequence(P().seqs.flash, SEQ_FLASH);
		*(int16_t *)(h + 0x24) = 0;
		for (uint32_t a = own::R(POOL_A); a < own::R(POOL_A_END); a += 0xC)
		{
			FlashP *p = (FlashP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0) p->life = -1;
		}
		*(uint32_t *)h = fxp::sequence(P().seqs.debris, SEQ_DEBRIS);
		for (uint32_t a = own::R(POOL_C); a < own::R(POOL_C_END); a += 0x14)
		{
			DebrisP *p = (DebrisP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->scale = (int16_t)(p->scale - (int16_t)(p->scale >> 5));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 3));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 3));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 3));
		}
		*(uint32_t *)h = fxp::sequence(P().seqs.spark, SEQ_SPARK);
		*(int16_t *)(h + 0x24) = 4;
		for (uint32_t a = own::R(POOL_B); a < own::R(POOL_B_END); a += 0x10)
		{
			SparkP *p = (SparkP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(SPARK_FRAMES + 4 * p->frame);
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->pos[1] = (int16_t)(p->pos[1] + p->vy);
			p->frame++;
			if (p->frame >= 0x18) p->frame = 0;
		}
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Master: packet arena parity, the main queue, the particles
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter & 1) PacketCursor() = TexBase() + PARITY;
		else PacketCursor() = TexBase();
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_master();)
		const int alive = ExecuteTaskQueue(QMain());
		ParticleDraws();
		r->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Emitters (335 / 339 / 340 / 343 ...): a task that spawns particles of one kind into SLOTS (the module's own spawn
	// part) and draws them every tick (EmitterDraw: the same code in every module but the kind and the sequence)
	// ------------------------------------------------------------------
	struct EmitWork    // the emitter's Field_Alloc block (0x50 bytes)
	{
		int16_t pos[4];    // +0x00 the spawn point (record emitters)
		int16_t pos2[4];   // +0x08 the spawn point (335's boss emitter)
		Mat4x3 cam;        // +0x10 the camera matrix (light matrix / back colour)
		int32_t dir[4];    // +0x30 the spawn direction / the draw's unit eye vector
		int32_t v[4];      // +0x40 the view-space point
	};
	static_assert(sizeof(EmitWork) == 0x50, "Renzokuken emitter block");

	// one particle: its point into view space (light matrix = the camera, back colour = its translation), moved
	// scale / 8 toward the eye, the sequence at its life drawn there at its scale
	static void EmitDraw(EmitWork *w, uint8_t *h, const SlotRec *p)
	{
		GteLoadV0(p->pos);
		GteMVMVA_LightV0_Bk();
		GteSetRotScale(p->scale);
		*(int16_t *)(h + 4) = p->life;
		GteReadMAC123(w->v);
		Normalize(w->v, w->dir);
		const int32_t s = -(p->scale >> 3);
		w->v[0] = w->v[0] + (mul32(w->dir[0], s) >> 12);
		w->v[1] = w->v[1] + (mul32(w->dir[1], s) >> 12);
		w->v[2] = w->v[2] + (mul32(w->dir[2], s) >> 12);
		GteSetTransFromVec32(w->v);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// the draw part of an emitter: every SLOTS record of its kind drawn, then (not draw-only) freed at its sequence's end
	// or grown (scale >> 4), moved and slowed (>> 4) (342: + gravity on y); returns the number of its particles still alive
	static int32_t EmitterDraw(const Node24 *n, EmitWork *w, int32_t kind, uint32_t seq, int16_t gravity = 0)
	{
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_emitter(n, kind, seq);)
		uint8_t *h = AllocHeader(0xB4);
		*(int16_t *)(h + 0x24) = 0;
		int32_t alive = 0;
		memcpy(&w->cam, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = EmitSeq(seq);
		GteSetLightMatrix(&w->cam);
		GteSetBackColorFromTrans(&w->cam);
		for (uint32_t a = own::R(SLOTS); a < own::R(SLOTS_END); a += 0x20)
		{
			SlotRec *p = (SlotRec *)a;
			if (p->alive != kind) continue;
			EmitDraw(w, h, p);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->alive = 0;
				continue;
			}
			p->life++;
			p->scale = (int16_t)(p->scale + (int16_t)(p->scale >> 4));
			p->pos[0] = (int16_t)(p->pos[0] + p->w10);
			p->pos[1] = (int16_t)(p->pos[1] + p->w12);
			p->pos[2] = (int16_t)(p->pos[2] + p->w14);
			p->w10 = (int16_t)(p->w10 - (int16_t)(p->w10 >> 4));
			p->w12 = (int16_t)(p->w12 - (int16_t)(p->w12 >> 4));
			p->w14 = (int16_t)(p->w14 - (int16_t)(p->w14 >> 4));
			if (gravity) p->w12 = (int16_t)(p->w12 + gravity); // (342's hit emitters)
			alive++;
		}
		FieldFree(0xB4);
		(void)n;
		return alive;
	}

	// a record emitter (339 / 343): its own record holds the point and a base direction (+0x18 / +0x1A / +0x1C); up to tick
	// `last`, 3 particles of its kind (node +0x1A) per tick: scale 0x700..0xF7F, at the point, the direction (+- 0x300 /
	// -0x100..-0x3FF / +0x80..+0x17F random, normalized, through the caster's rotation) times 0x23..0xB8 (x + 0..0x27 -
	// 0x14); ends (its record freed) on the first tick with none left
	static uint32_t RecordEmitter(TaskNode *tn, int16_t last, uint32_t seq)
	{
		Node24 *n = (Node24 *)tn;
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= last)
		{
			const SlotRec *own = Slot(n->e);
			w->dir[0] = CrtRand() % 0x600 + own->w18 - 0x300;
			const int32_t ry = CrtRand() % 0x300;
			w->dir[1] = own->w1A - ry - 0x100;
			const int32_t rz = CrtRand() % 0x100;
			w->dir[2] = rz + own->w1C + 0x80;
			Normalize(w->dir, w->dir);
			TransformVectorBy3x3Matrix((const Mat4x3 *)(Caster() + 0x40), w->dir, w->dir);
			memcpy(w->pos, own->pos, 8);
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = n->w1A;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x880 + 0x700);
				memcpy(p->pos, w->pos, 8);
				const int32_t sp = CrtRand() % 0x96 + 0x23;
				const int32_t rx = CrtRand() % 0x28;
				p->w10 = (int16_t)(rx + (mul32(w->dir[0], sp) >> 12) - 0x14);
				p->w12 = (int16_t)(mul32(w->dir[1], sp) >> 12);
				p->w14 = (int16_t)(mul32(w->dir[2], sp) >> 12);
			}
		}
		const int32_t alive = EmitterDraw(n, w, n->w1A, seq);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		if (alive != 0) return 0;
		Slot(n->e)->alive = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Slot models (335 / 337 / 339 / 340 / 341 ...): a model task with its own SLOTS record (the point at +8, a direction at
	// +0x18 / +0x1A / +0x1C): the model turned from the module's reference direction onto it, at the point, scaled
	// uniformly by +0x1C (+= +0x1E) or (sep_y) x / z by +0x1C and y by +0x20 (+= +0x22), the rate - rate / 4 each tick,
	// fading from tick fade_from (step fade_mul), `ticks` ticks, then its record freed
	// ------------------------------------------------------------------
	struct SlotModelDef { uint32_t orig, model, ref; int32_t mode, fade_from, fade_mul, ticks; bool sep_y; };

	static void SlotModelDraw(const SlotModelDef &d, const Node24 *n, const SlotRec *r, int32_t sx, int32_t sy, int32_t fade)
	{
		int32_t v[3] = { r->w18, r->w1A, r->w1C };
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)d.ref, v, axis);
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only)
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[1] = r->pos[1];
		m.t[0] = r->pos[0];
		int32_t sv[3];
		sv[2] = sx;
		sv[1] = sy;
		sv[0] = sx;
		m.t[2] = r->pos[2];
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = OwnModel(d.model);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (n->c >= d.fade_from)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), d.mode, PacketCursor());
		FieldFree(0x58);
	}
	static int32_t SlotModelFade(const SlotModelDef &d, int32_t c) { return mul32(c - d.fade_from, d.fade_mul); }
	static int32_t SlotModelSX(const SlotModelDef &, const Node24 *n) { return n->w1C; }
	static int32_t SlotModelSY(const SlotModelDef &d, const Node24 *n) { return d.sep_y ? n->w20 : n->w1C; }

	static uint32_t SlotModelTask(const SlotModelDef &d, int kind, Node24 *n)
	{
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_slotmodel(kind, n);)
		(void)kind;
		SlotModelDraw(d, n, Slot(n->e), SlotModelSX(d, n), SlotModelSY(d, n), SlotModelFade(d, n->c));
		if (DrawOnly()) return 0;
		if (d.sep_y)
		{
			const int16_t rate = n->w22;
			n->w20 = (int16_t)(n->w20 + rate);
			n->w22 = (int16_t)(rate - (int16_t)(rate / 4));
		}
		else
		{
			const int16_t rate = n->w1E;
			n->w1C = (int16_t)(n->w1C + rate);
			n->w1E = (int16_t)(rate - (int16_t)(rate / 4));
		}
		n->c++;
		if (n->c < d.ticks) return 0;
		Slot(n->e)->alive = 0;
		return TASK_END;
	}

	// a record emitter with its own record: the hit point, its kind (node +0x1A), its base direction
	static void AddRecordEmitter(uint32_t orig, int16_t kind, int16_t dx, int16_t dy, int16_t dz)
	{
		Node24 *t = AddSlotTask(orig);
		t->w1A = kind;
		SlotRec *r = Slot(t->e);
		memcpy(r->pos, (const void *)own::R(HIT_POS), 8);
		r->w18 = dx;
		r->w1A = dy;
		r->w1C = dz;
	}

	// a slot model at the hit point turned onto the hit direction (the low words of its three ints)
	static void HitPointRecord(Node24 *t)
	{
		SlotRec *r = Slot(t->e);
		memcpy(r->pos, (const void *)own::R(HIT_POS), 8);
		r->w18 = var<int16_t>(own::R(HIT_DIR));
		r->w1A = var<int16_t>(own::R(HIT_DIR) + 4);
		r->w1C = var<int16_t>(own::R(HIT_DIR) + 8);
	}

	// ------------------------------------------------------------------
	// Dust (337 / 341 ...): kind-4 particles around a point on the ground (the node's +0x10; 341: the boss's default
	// effect position, set on the task's tick 0), `count` per tick up to tick `last`: scale0 + rand % scale_n, angle
	// rand % 0x1000, radius r0 + rand % r_n, y -(0..0x31), outward speed sp0 + rand % sp_n (x / z only); every tick the
	// kind-4 particles drawn (the shared sprite sequence 3, frame offset 8) through the camera translated to the node's
	// x / z, each moved scale / 8 toward the eye, moving / slowing (>> 3) on x / z; ends on the first tick with none left
	// ------------------------------------------------------------------
	inline int32_t ComputeCosine(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); } // computeCosine
	struct DustDef { uint32_t orig; bool delayed; int16_t last; int32_t count, scale_n, scale0, r_n, r0, sp_n, sp0; };
#pragma pack(push, 1)
	struct DustWork    // its Field_Alloc block (0x48 bytes)
	{
		int16_t ang[4];    // +0x00 the node's point (copied, unused) / the zero angles
		Mat4x3 m;          // +0x08 the camera through the node's x / z (light matrix / back colour)
		int32_t u[3];      // +0x28 sin / -, cos of the spawn angle / the draw's unit eye vector
		int32_t pad34;
		int32_t v[3];      // +0x38 the view-space point
		int32_t pad44;
	};
#pragma pack(pop)
	static_assert(sizeof(DustWork) == 0x48, "Renzokuken 341 dust block");

	// the draw part's matrix: the camera through the node's x / z (light matrix / back colour); the header's sequence
	static void DustBegin(DustWork *w, uint8_t *h, const Node24 *n)
	{
		*(uint32_t *)h = (uint32_t)EffectSpriteSharedSequence(3);
		*(int16_t *)(h + 0x24) = 8;
		w->ang[0] = 0;
		w->ang[1] = 0;
		w->ang[2] = 0;
		ComposeZYXRotationMatrix(w->ang, &w->m);
		w->m.t[0] = n->w10;
		w->m.t[1] = 0;
		w->m.t[2] = n->w14;
		ComposeAffineTransform(&Camera(), &w->m, &w->m);
		GteSetLightMatrix(&w->m);
		GteSetBackColorFromTrans(&w->m);
	}

	// one dust particle (its point relative to the node, x / z)
	static void DustDraw(DustWork *w, uint8_t *h, const SlotRec *p)
	{
		GteLoadV0(p->pos);
		GteMVMVA_LightV0_Bk();
		GteSetRotScale(p->scale);
		*(int16_t *)(h + 4) = p->life;
		GteReadMAC123(w->v);
		Normalize(w->v, w->u);
		const int32_t s = -(p->scale >> 3);
		w->v[0] = w->v[0] + (mul32(w->u[0], s) >> 12);
		w->v[1] = w->v[1] + (mul32(w->u[1], s) >> 12);
		w->v[2] = w->v[2] + (mul32(w->u[2], s) >> 12);
		GteSetTransFromVec32(w->v);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	static uint32_t DustRun(const DustDef &d, Node24 *n)
	{
		if (d.delayed && n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		DustWork *w = (DustWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= d.last)
		{
			if (d.delayed && n->c == 0) GetDefaultEffectPosition(Target(), &n->w10);
			memcpy(w->ang, &n->w10, 8);
			for (int32_t j = 0; j < d.count; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = 4;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % d.scale_n + d.scale0);
				const int32_t a = CrtRand() % 0x1000;
				w->u[0] = ComputeSin(a);
				w->u[2] = ComputeCosine(a);
				const int32_t r = CrtRand() % d.r_n + d.r0;
				p->pos[0] = (int16_t)(mul32(w->u[0], r) >> 12);
				p->pos[1] = (int16_t)-(CrtRand() % 0x32);
				p->pos[2] = (int16_t)(mul32(w->u[2], r) >> 12);
				const int32_t sp = CrtRand() % d.sp_n + d.sp0;
				p->w10 = (int16_t)(mul32(w->u[0], sp) >> 12);
				p->w14 = (int16_t)(mul32(w->u[2], sp) >> 12);
			}
		}
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_dust(n);)
		uint8_t *h = AllocHeader(0xB4);
		int32_t alive = 0;
		DustBegin(w, h, n);
		for (uint32_t a = own::R(SLOTS); a < own::R(SLOTS_END); a += 0x20)
		{
			SlotRec *p = (SlotRec *)a;
			if (p->alive != 4) continue;
			DustDraw(w, h, p);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->alive = 0;
				continue;
			}
			p->life++;
			p->pos[0] = (int16_t)(p->pos[0] + p->w10);
			p->pos[2] = (int16_t)(p->pos[2] + p->w14);
			p->w10 = (int16_t)(p->w10 - (int16_t)(p->w10 >> 3));
			p->w14 = (int16_t)(p->w14 - (int16_t)(p->w14 >> 3));
			alive++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Shake (337 / 341 ...): the camera shake word 0x1D97712 (the view build zeroes it every frame) = sin((tick << 10) / div
	// + 0x400) * amp >> 12, negated on even ticks (also on draw-only ticks), `ticks` ticks (341: after the node's delay)
	// ------------------------------------------------------------------
	struct ShakeDef { bool delayed; int32_t div, amp, ticks; };
	static uint32_t ShakeRun(const ShakeDef &d, Node24 *n)
	{
		if (d.delayed && n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		int32_t v = mul32(ComputeSin(shl32(n->c, 10) / d.div + 0x400), d.amp) >> 12;
		if (!(n->c & 1)) v = -v;
		var<int16_t>(0x1D97712) = (int16_t)v;
		if (DrawOnly()) return 0;
		n->c++;
		return n->c >= d.ticks ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bone emitters (334 / 337 / 340 ...): up to tick `last`, the boss's bone `bone` at fractions 0 and 0x1000 (the point
	// and its direction); 3 particles of kind 2 per tick at the second point, scale scale0 + rand % scale_n, the direction
	// times sp0 + rand % sp_n; the kind-2 particles drawn every tick (EmitterDraw); ends on the first tick with none left
	// ------------------------------------------------------------------
	// (336: two bones, bone2 != 0: the mean of both unit directions, normalized; the spawn point is bone2's at 0x1000)
	struct BoneEmitDef { int16_t last; int32_t bone, bone2, scale_n, scale0, sp_n, sp0; uint32_t seq; };
	// the unit direction of a bone (fraction 0 -> 0x1000); its second point stays in w->pos2
	static void BoneDir(EmitWork *w, int32_t bone, int32_t *dir)
	{
		GetEffectSpawnPosition(Target(), bone, 0, w->pos);
		GetEffectSpawnPosition(Target(), bone, 0x1000, w->pos2);
		dir[0] = (int32_t)w->pos2[0] - w->pos[0];
		dir[1] = (int32_t)w->pos2[1] - w->pos[1];
		dir[2] = (int32_t)w->pos2[2] - w->pos[2];
		Normalize(dir, dir);
	}
	static uint32_t BoneEmitter(const BoneEmitDef &d, Node24 *n)
	{
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= d.last)
		{
			if (d.bone2 == 0) BoneDir(w, d.bone, w->dir);
			else
			{
				BoneDir(w, d.bone, w->v);
				BoneDir(w, d.bone2, w->dir);
				w->dir[0] = (w->v[0] + w->dir[0]) >> 1;
				w->dir[1] = (w->v[1] + w->dir[1]) >> 1;
				w->dir[2] = (w->v[2] + w->dir[2]) >> 1;
				Normalize(w->dir, w->dir);
			}
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = 2;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % d.scale_n + d.scale0);
				memcpy(p->pos, w->pos2, 8);
				const int32_t sp = CrtRand() % d.sp_n + d.sp0;
				p->w10 = (int16_t)(mul32(w->dir[0], sp) >> 12);
				p->w12 = (int16_t)(mul32(w->dir[1], sp) >> 12);
				p->w14 = (int16_t)(mul32(w->dir[2], sp) >> 12);
			}
		}
		const int32_t alive = EmitterDraw(n, w, 2, d.seq);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Model tasks on the node's point (334 / 336 / 337 ...): turned by rot = NM_GROUND (none, at the node's x / z, y 0),
	// NM_CASTER (the caster's matrix +0x40, read every draw), NM_CASTER_OFS (334: the same with a zero offset through it
	// added to the point: matrixMultiplyVector of (0, 0, 0)), NM_YANGLE (334: the y angle +0x18); uniform scale +0x1C (+=
	// +0x1E, the rate - rate / rate_div; sep_y (340): x / z by +0x1C, y by +0x20 (+= +0x22)), fading from tick fade_from
	// (step fade_mul), `ticks` ticks (slot: then its own SLOTS record freed)
	// ------------------------------------------------------------------
	enum { NM_GROUND, NM_CASTER, NM_CASTER_OFS, NM_YANGLE };
	struct NodeModelDef { uint32_t orig, model; int rot; int32_t fade_from, fade_mul, rate_div, ticks; bool slot, sep_y; };

	static void NodeModelDraw(const NodeModelDef &d, const Node24 *n, int32_t scale, int32_t sy, int32_t fade)
	{
		Mat4x3 m; // (pad +0x12: the source's / never written: GTE R33 high half only)
		int32_t sv[3];
		if (d.rot == NM_CASTER || d.rot == NM_CASTER_OFS)
		{
			memcpy(&m, Caster() + 0x40, sizeof(Mat4x3));
			if (d.rot == NM_CASTER_OFS)
			{
				int16_t v[4]; // (4th word never written: matrixMultiplyVector reads / writes 3)
				v[2] = 0;
				v[1] = 0;
				v[0] = 0;
				MatVec(&m, v, v);
				m.t[0] = (int32_t)n->w10 + v[0];
				m.t[1] = (int32_t)n->w12 + v[1];
				m.t[2] = (int32_t)n->w14 + v[2];
			}
			else
			{
				m.t[1] = n->w12;
				m.t[0] = n->w10;
				m.t[2] = n->w14;
			}
		}
		else
		{
			int16_t ang[3] = { 0, (int16_t)(d.rot == NM_YANGLE ? n->w18 : 0), 0 };
			ComposeZYXRotationMatrix(ang, &m);
			m.t[0] = n->w10;
			m.t[1] = d.rot == NM_YANGLE ? n->w12 : 0;
			m.t[2] = n->w14;
		}
		sv[2] = scale;
		sv[1] = sy;
		sv[0] = scale;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = OwnModel(d.model);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (n->c >= d.fade_from)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static int32_t NodeModelFade(const NodeModelDef &d, int32_t c) { return mul32(c - d.fade_from, d.fade_mul); }

	static uint32_t NodeModelTask(const NodeModelDef &d, int kind, Node24 *n)
	{
		// 30 fps layer: see renzo_boss_held.inc
		FX_HELD(held_note_nodemodel(kind, n);)
		(void)kind;
		NodeModelDraw(d, n, n->w1C, d.sep_y ? n->w20 : n->w1C, NodeModelFade(d, n->c));
		if (DrawOnly()) return 0;
		if (d.sep_y)
		{
			const int16_t rate = n->w22;
			n->w20 = (int16_t)(n->w20 + rate);
			n->w22 = (int16_t)(rate - (int16_t)(rate / d.rate_div));
		}
		else
		{
			const int16_t rate = n->w1E;
			n->w1C = (int16_t)(n->w1C + rate);
			n->w1E = (int16_t)(rate - (int16_t)(rate / d.rate_div));
		}
		n->c++;
		if (n->c < d.ticks) return 0;
		if (d.slot) Slot(n->e)->alive = 0;
		return TASK_END;
	}

	// a port: noted for Fujin's programs (own::Fn), registered for the game
	static void Port(uint32_t orig, void *port, const char *name)
	{
		own::note(orig, port);
		register_port(orig, port, name, RENZO_ID);
	}

	// the common ports (the module registers its own tasks after these); the file loader and the setup for the programs
	static void RegisterCommon()
	{
		own::note(ORIG_FileLoader, (void *)FileLoader);
		own::note(ORIG_Setup, (void *)Setup);
		Port(ORIG_Master, (void *)MasterTask, RENZO_IDS " Master");
		Port(ORIG_Controller, (void *)ControllerTask, RENZO_IDS " Controller");
		Port(ORIG_Trail, (void *)TrailTask, RENZO_IDS " Trail");
		Port(ORIG_SparkSpawn, (void *)SparkSpawnTask, RENZO_IDS " SparkSpawn");
		Port(ORIG_Hit, (void *)HitTask, RENZO_IDS " Hit");
		Port(ORIG_Slash, (void *)SlashTask, RENZO_IDS " Slash");
		Port(ORIG_HitSpark, (void *)HitSparkTask, RENZO_IDS " HitSpark");
		Port(ORIG_Sparkle, (void *)SparkleTask, RENZO_IDS " Sparkle");
	}
