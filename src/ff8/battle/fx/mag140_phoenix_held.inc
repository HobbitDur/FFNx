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

// 30 fps part of mag140_phoenix.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag140_phoenix.cpp)

namespace ff8fx
{
namespace p140
{
	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	static uint32_t g_ported_tick = 0xFFFFFFFF; // real tick on which the ported master last ran

	static void held_note_master() { g_ported_tick = g_real_tick; }

	// what the creature drew on the real tick (held frames draw between it and the next tick)
	struct CreatureMemo
	{
		uint32_t tick;
		const void *node;
		int16_t c0;     // counter on entry (fire trail stage)
		int16_t c1;     // counter after the fire trail stage (every later stage)
		bool model;     // the model was drawn
		bool fade;      // ... with the fade-in draw (0x6A7A50)
		Mat4x3 root;    // root matrix it was drawn with
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	// creature: the counter the tick starts with (fire trail stage)
	static void held_note_creature(CreatureNode *cn)
	{
		CreatureMemo &memo = g_creature;
		memo.tick = g_real_tick;
		memo.node = cn;
		memo.c0 = cn->counter;
		memo.model = false;
	}

	// the counter after the fire trail stage (every later stage)
	static void held_note_trail_done(CreatureNode *cn) { g_creature.c1 = cn->counter; }

	static void held_note_model() { g_creature.model = true; }

	// the model's draw: fade-in draw or not, and the root it is drawn with
	static void held_note_model_root(CreatureNode *cn, bool fade)
	{
		g_creature.fade = fade;
		g_creature.root = Root(cn);
	}

	struct EmberMemo { int16_t pos[3], angle, age; };
	static NodeMemo<EmberMemo, 16> g_ember_memo;

	// ember as drawn, before the tick's update
	static void held_note_ember(EmberNode *p)
	{
		if (EmberMemo *m = g_ember_memo.put(p))
		{
			memcpy(m->pos, p->pos, sizeof(m->pos));
			m->angle = p->angle;
			m->age = p->age;
		}
	}

	struct SparkMemo { int16_t pos[4], age; };
	static NodeMemo<SparkMemo, 128> g_spark_memo;

	// spark as drawn, before the tick's update
	static void held_note_spark(SparkNode *p)
	{
		if (SparkMemo *m = g_spark_memo.put(p))
		{
			memcpy(m->pos, p->pos, 8);
			m->age = p->age;
		}
	}

	// FireTrail (vanilla file) at v4 + dv * num / den: the path positions and the fade in
	// between (the flipbook frames and the texture scroll keep their 15 Hz steps); the camera
	// cuts of v4 20 and 40 are real-tick writes only
	static void FireTrailHeld(uint32_t v4, int32_t dv, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		RotY(0x800, h + 0x78);
		*(int32_t *)(h + 0x8C) = 0;
		*(int32_t *)(h + 0x90) = 0;
		*(int32_t *)(h + 0x94) = Base();
		int32_t span = 96 * den;
		int32_t v = (int32_t)(86 * v4) * den + 86 * dv * num;
		for (int i = 0; i < 26; i++, v -= 43 * den)
		{
			if (v < 0 || v >= 0x20A0 * den) continue;
			int32_t idx = v / span;
			int32_t f = shl32(v - idx * span, 12) / span;
			const uint8_t *path = (const uint8_t *)(0x11D04BC + 8 * idx);
			int16_t P[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (see the v4 20/40 cut)
			VecLerp(path, path + 8, 0x1000 - f, f, P);
			uint8_t *hdr = (uint8_t *)FieldAlloc(0xB4);
			P[2] = (int16_t)(Base() - P[2]);
			TransformCameraByShadowRotation(P, 0x640, -0x100);
			Flipbook(hdr, (int16_t)(i >> 1));
			P[0] = (int16_t)-P[0];
			TransformCameraByShadowRotation(P, 0x640, -0x100);
			Flipbook(hdr, (int16_t)(i >> 1));
			FieldFree(0xB4);
		}
		Mat4x3 *m = (Mat4x3 *)(h + 0x78);
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(int32_t *)(h + 0x38) = (int32_t)((uint32_t)shl32((int32_t)v4 * den + dv * num, 12) / (uint32_t)(95 * den)) - 0x1000;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x1A) = 0x100;
		*(uint16_t *)(h + 0x18) = 0x100;
		*(uint16_t *)(h + 0x1E) = 0;
		*(uint16_t *)(h + 0x1C) = 0;
		*(uint16_t *)(h + 0x20) = 0x80;
		*(uint16_t *)(h + 0x22) = 0x100;
		*(uint16_t *)(h + 0x10) = (uint16_t)(v4 & 0x7F);
		*(uint32_t *)h = 0x11D04B4;
		*(uint32_t *)(h + 4) = 0x11D04BC;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		*(uint32_t *)h = 0x11D2594;
		*(uint16_t *)(h + 0x10) = (uint16_t)((v4 & 0x3F) << 1);
		*(uint32_t *)(h + 4) = 0x11D259C;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		*(uint32_t *)h = 0x11D3778;
		*(uint16_t *)(h + 0x10) = (uint16_t)((uint8_t)(0u - v4) & 0x7F);
		*(uint32_t *)(h + 4) = 0x11D3780;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}

	static int32_t HazeAmp(uint32_t e) { return ComputeSin((int32_t)(((e - 40) << 11) / 20)) / 440; }

	// root translation y of the model at counter c (0x6A6C08..)
	static int32_t RootRise(int32_t c)
	{
		uint32_t e = (uint32_t)(c - 101);
		return e < 0x14 ? (int32_t)((0x14 - e) << 5) : 0;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Every stage is a function of the creature counter: the held frame
	// draws the stage the real tick drew (counter memo) at the in-between of its parameters and
	// the parameters of the next tick (the counter the creature holds now); what jumps
	// (cuts, stage changes, flipbook frames, texture scroll of the flame meshes) keeps the
	// 15 Hz steps. Nothing here changes game state: no RNG, no spawn, no camera write.
	// ------------------------------------------------------------------
	static void HeldTile(bool (*spec)(int32_t, uint8_t *), int32_t c, int32_t cn, int num, int den)
	{
		uint8_t a[3], b[3];
		if (!spec(c, a)) return; // not drawn on the real tick
		b[0] = a[0]; b[1] = a[1]; b[2] = a[2];
		if (cn != c && !spec(cn, b)) b[0] = b[1] = b[2] = 0; // gone next tick: fades out
		uint8_t rgb[3];
		for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)lerp_i(a[k], b[k], num, den);
		PacketCursor() = TileEmit(rgb, PacketCursor());
	}

	// next-tick value if the next tick stays in the same stage (d = counter step 1..2), else hold
	static int32_t Toward(int32_t now, int32_t next, int32_t c, int32_t cnext, int32_t lo, int32_t hi, int num, int den)
	{
		if (cnext <= c || cnext > c + 2 || cnext < lo || cnext >= hi) return now;
		return lerp_i(now, next, num, den);
	}

	static void CreatureHeld(CreatureNode *cn, int num, int den)
	{
		const CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick || m.node != cn) return;
		int32_t c0 = m.c0, c1 = m.c1, cx = cn->counter;

		// fire trail: path position advances by the counter step (2 per tick below 60)
		uint32_t v4 = (uint32_t)(c0 - 1);
		if (v4 < 100)
		{
			int32_t dv = (cx - 1) - (int32_t)v4;
			if (dv < 1 || dv > 2) dv = 0;
			FireTrailHeld(v4, dv, num, den);
		}

		HeldTile(TileSpec93, c1, cx, num, den);

		// model: midpoint pose, root half way to the next draw's root
		if (m.model)
		{
			uint8_t *e = E(cn);
			Mat4x3 save = Root(cn), next = Root(cn), r = m.root;
			next.t[1] = RootRise(cx);
			if ((uint32_t)(cx - 101) < 0x88 && cx > c1)
			{
				for (int i = 0; i < 3; i++)
					for (int j = 0; j < 3; j++) r.m[i][j] = (int16_t)lerp_i(m.root.m[i][j], next.m[i][j], num, den);
				for (int i = 0; i < 3; i++) r.t[i] = lerp_i(m.root.t[i], next.t[i], num, den);
			}
			Root(cn) = r;
			pose_midpoint(e + 0x60, e + 0x6C, num, den);
			if (m.fade) DrawModelFade(e, 0, ModelBuffer() + 0x878);
			else FrameCursor() = DrawModel(e, ModelBuffer() + 0x878, FrameCursor(), var<uint32_t>(0x1D969A8));
			Root(cn) = save;
		}

		// prim stages: the shared player draws the in-between records; texture scroll in between
		uint32_t e1 = (uint32_t)(c1 - 101);
		if (e1 < 0x32)
		{
			PrimCtx ctx;
			CtxStage101(ctx, (int16_t)Toward((int32_t)(e1 << 4), (cx - 101) << 4, c1, cx, 101, 151, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
		}
		e1 = (uint32_t)(c1 - 151);
		if (e1 < 0x2C)
		{
			PrimCtx ctx;
			CtxIdentity(ctx, -0x400, (int16_t)Toward((int32_t)(e1 << 3), (cx - 151) << 3, c1, cx, 151, 195, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
			ctx.shade = 0x20;
			ctx.m.t[0] = 0x100;
			ctx.m.t[1] = 0x200;
			prim::play_held((prim::Layout *)cn->prim_b, PrimObject, (int)&ctx, num, den);
		}
		e1 = (uint32_t)(c1 - 195);
		if (e1 < 0x3C)
		{
			HeldTile(TileSpec195a, c1, cx, num, den);
			HeldTile(TileSpec195b, c1, cx, num, den);
			PrimCtx ctx;
			int32_t sc = (int32_t)e1 * 2 - (int32_t)(e1 << 4), scn = (cx - 195) * -14;
			CtxIdentity(ctx, -0x400, (int16_t)Toward(sc, scn, c1, cx, 195, 255, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
			if (e1 >= 0x28)
			{
				uint32_t en = (uint32_t)(cx - 195);
				int32_t amp = HazeAmp(e1), phase = (int32_t)(e1 << 7);
				if (en > e1 && en < 0x3C)
				{
					amp = lerp_i(amp, HazeAmp(en), num, den);
					phase = lerp_i(phase, (int32_t)(en << 7), num, den);
				}
				Haze(amp, phase, 0x11);
			}
		}
	}

	static void EmberHeld(EmberNode *p, int num, int den)
	{
		const EmberMemo *m = g_ember_memo.get(p);
		if (!m) return;
		int16_t pos[3];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		EmberDraw(pos, lerp_angle(m->angle, p->angle, num, den), lerp_i((int32_t)m->age << 8, (int32_t)p->age << 8, num, den));
	}

	static void SparkHeld(SparkNode *p, int num, int den)
	{
		const SparkMemo *m = g_spark_memo.get(p);
		if (!m) return;
		int16_t pos[4];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		pos[3] = m->age;
		SparkDraw(pos, m->age); // flipbook frame keeps its 15 Hz step
	}

	static uint8_t g_held_packets[0x60000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static MasterNode *Master()
	{
		for (TaskNode *t = QueueCreature().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_SequenceTask) return (MasterNode *)t;
		return nullptr;
	}

	// mirrors the master's queue order; packets go to a private buffer (the module cursor and
	// the frame arena the model draw uses are redirected and put back); the effect-camera
	// matrix is rebuilt from the held-frame camera and put back
	static void HeldFrame(int num, int den)
	{
		MasterNode *master = Master();
		if (!master || !master->spawned) return;
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		Mat4x3 effect_camera = EffectCamera();
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x40000;
		EffectCameraMatrix(&Camera(), &EffectCamera());
		for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask) CreatureHeld((CreatureNode *)t, num, den);
		for (TaskNode *t = QueueParticles().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_SparkTask) SparkHeld((SparkNode *)t, num, den);
		for (TaskNode *t = QueueParticles().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_EmberTask) EmberHeld((EmberNode *)t, num, den);
		EffectCamera() = effect_camera;
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}

	// ------------------------------------------------------------------
	// Held-frame camera (for the integrator's camera API; never writes the camera).
	// Camera = Battle_Camera_world (0xB8B7F0 x,y,z) / LookAt (0xB8B7F8 x,y,z). The next tick's
	// camera is predicted from the current one by the creature's camera code for the counter
	// it holds now; cuts (absolute writes) hold, everything else is lerped.
	// ------------------------------------------------------------------
	struct Cam { int16_t w[3], l[3]; };

	// the creature tick's camera writes for counter c (no pause, streams ready), on a copy
	static bool PredictCamera(int16_t c, Cam &k)
	{
		bool cut = false;
		int32_t base = Base();
		uint32_t v4 = (uint32_t)(c - 1);
		if (v4 < 100)
		{
			if (v4 == 20 || v4 == 40) // flipbook 0's position (mirrored at 40)
			{
				int32_t v = (int32_t)(86 * v4), idx = v / 96, f = shl32(v % 96, 12) / 96;
				const uint8_t *path = (const uint8_t *)(0x11D04BC + 8 * idx);
				int16_t P[4] = { 0, 0, 0, 0 };
				VecLerp(path, path + 8, 0x1000 - f, f, P);
				k.l[0] = v4 == 40 ? (int16_t)-P[0] : P[0];
				k.l[1] = P[1];
				k.l[2] = (int16_t)(base - P[2]);
				k.w[2] = (int16_t)(base + 0x7D0);
				cut = true;
			}
			if (v4 < 60)
			{
				if (v4 >= 20) { k.w[2] += 200; k.l[2] += 100; }
				c++;
			}
			else if (v4 == 60)
			{
				c = 0x51;
				k.l[0] = 0; k.l[1] = 0; k.l[2] = (int16_t)base;
				k.w[0] = 0; k.w[1] = (int16_t)0xF830; k.w[2] = (int16_t)(base + 0x7D0);
				cut = true;
			}
			else if (v4 >= 80)
			{
				uint32_t t = ((v4 - 80) << 12) / 20;
				int16_t a[4] = { 0, 0, (int16_t)base, 0 }, b[4] = { 0, 0x125, (int16_t)(base + 0xD1), 0 };
				int16_t o[4];
				VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, o);
				memcpy(k.l, o, 6);
				a[1] = (int16_t)0xF830; a[2] = (int16_t)(base + 0x7D0);
				b[1] = (int16_t)0xF448; b[2] = (int16_t)(base + 0x1194);
				VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, o);
				memcpy(k.w, o, 6);
			}
		}
		uint32_t e = (uint32_t)(c - 151);
		if (e < 0x2C && e > 0x1E) { k.w[0] += 0x30; k.w[1] += 0x30; k.w[2] -= 0x60; }
		if (c == 0 || c == 0x64 || c == 0x96 || c == 0xC2) cut = true;
		else if (c >= 0x65 && c < 0x96)
		{
			Mat4x3 rot;
			RotY(8, &rot);
			int16_t d[4] = { (int16_t)(k.w[0] - k.l[0]), (int16_t)(k.w[1] - k.l[1]), (int16_t)(k.w[2] - k.l[2]), 0 };
			if (c >= 0x79)
			{
				int32_t len = ISqrt((int32_t)((uint32_t)mul32(d[2], d[2]) + (uint32_t)mul32(d[0], d[0]) + (uint32_t)mul32(d[1], d[1])));
				int32_t kk = 2 * (int32_t)c - 0xF2;
				for (int i = 0; i < 3; i++) d[i] = (int16_t)(d[i] - (int16_t)(mul32(d[i], kk) / len));
				k.l[2] += 10;
			}
			MatMulVec(&rot, d, d);
			for (int i = 0; i < 3; i++) k.w[i] = (int16_t)(d[i] + k.l[i]);
		}
		else if (c > 0xC2)
		{
			int32_t f = (int32_t)c - 0xC2, a = shl32(f, 11) / 60;
			k.l[0] = 0;
			k.w[0] = 0;
			k.l[1] = (int16_t)(-0x44C - (ComputeSin(a + 0x80) >> 2));
			k.l[2] = (int16_t)(base - 0x1399);
			int32_t co = ComputeCos(a >> 1);
			int32_t q = (int32_t)(((int64_t)co * (int32_t)0x53896E7B) >> 32) - co;
			q >>= 7;
			q += (int32_t)((uint32_t)q >> 31);
			k.w[1] += (int16_t)q;
			k.w[2] = (int16_t)(TargetsAvg() - 4 * f - 0x1398);
		}
		return cut;
	}

	// held-frame camera of effect 140 at tick + num / den: false = the module does not drive
	// the camera now (creature not running / not ported this tick)
	static bool mag140_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick || !HeldReady()) return false;
		CreatureNode *cn = nullptr;
		for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask && t == m.node) cn = (CreatureNode *)t;
		if (!cn) return false;
		Cam now, next;
		for (int i = 0; i < 3; i++) { now.w[i] = CamWorld(i); now.l[i] = CamLookAt(i); }
		next = now;
		bool cut = PredictCamera(cn->counter, next);
		for (int i = 0; i < 3; i++)
		{
			world[i] = cut ? now.w[i] : (int16_t)lerp_i(now.w[i], next.w[i], num, den);
			lookat[i] = cut ? now.l[i] : (int16_t)lerp_i(now.l[i], next.l[i], num, den);
		}
		return true;
	}
}

	static void register_mag140_held()
	{
		mark_held(p140::ORIG_CreatureTask);
		mark_held(p140::ORIG_EmberTask);
		mark_held(p140::ORIG_SparkTask);
		register_module_held(140, p140::HeldReady, p140::HeldFrame);
		register_module_camera(140, p140::mag140_held_camera);
	}
}
