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

// 30 fps part of mag191_doomtrain.cpp: memos of what the real tick drew, the held-frame drawing
// and the held-frame camera of the shared camera-script task (included at the end of
// mag191_doomtrain.cpp)

namespace ff8fx
{
namespace d191
{
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static void held_note_master() { g_ported_tick = g_real_tick; }

	static int16_t L16(int16_t a, int16_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// ======================================================================
	// Held-frame camera of the camera-script task 0x63E9C0: CsRun (the port's own tick) run
	// once more on the real globals predicts its next camera
	// ======================================================================
	// the node that ran on the last real tick (for the held-frame camera)
	static CamNode *g_cs_node = nullptr;
	static uint32_t g_cs_tick = 0xFFFFFFFF;

	static void held_note_camscript(CamNode *n, uint32_t r)
	{
		if (r == 0)
		{
			g_cs_node = n;
			g_cs_tick = g_real_tick;
		}
		else if (g_cs_node == n) g_cs_tick = 0xFFFFFFFF;
	}

	// held-frame camera: the next tick of the script, run on the real globals and put back
	// (pure maths only: the parse / curve / easing helpers have no side effect)
	static bool CsHeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		CamNode *n = g_cs_node;
		if (!n || g_cs_tick != g_real_tick) return false;
		if (!(n->hdr.flags & 1) || (uint32_t)n->hdr.func != ORIG_CamScriptTask) return false;
		static uint8_t t0[sizeof(CamTrack)], t1[sizeof(CamTrack)], node[sizeof(CamNode)], cam[16], saved[16];
		memcpy(t0, TrackLook(), sizeof(t0));
		memcpy(t1, TrackEye(), sizeof(t1));
		memcpy(node, n, sizeof(node));
		memcpy(cam, (const void *)CAM_EYE, 16);
		memcpy(saved, (const void *)0x24FD250, 8);
		memcpy(saved + 8, (const void *)0x24FD358, 8);
		CamTrack *pc = TrackCur(), *po = TrackOther();
		int16_t h = CamH(), roll = CamRoll();
		bool cut = false;
		uint32_t r = CsRun(n, &cut);
		int16_t e1[3], a1[3];
		memcpy(e1, (const void *)CAM_EYE, 6);
		memcpy(a1, (const void *)CAM_AT, 6);
		memcpy(TrackLook(), t0, sizeof(t0));
		memcpy(TrackEye(), t1, sizeof(t1));
		memcpy(n, node, sizeof(node));
		memcpy((void *)CAM_EYE, cam, 16);
		memcpy((void *)0x24FD250, saved, 8);
		memcpy((void *)0x24FD358, saved + 8, 8);
		TrackCur() = pc;
		TrackOther() = po;
		CamH() = h;
		CamRoll() = roll;
		const int16_t *e0 = (const int16_t *)cam, *a0 = (const int16_t *)(cam + 8);
		bool move = r == 0 && !cut;
		for (int i = 0; i < 3 && move; i++)
		{
			int32_t de = (int32_t)e1[i] - e0[i], da = (int32_t)a1[i] - a0[i];
			if (de > 1500 || de < -1500 || da > 1500 || da < -1500) move = false; // a jump of its own: a cut
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(e0[i], e1[i], num, den) : e0[i];
			lookat[i] = move ? (int16_t)lerp_i(a0[i], a1[i], num, den) : a0[i];
		}
		return true;
	}

	// ======================================================================
	// Held-frame memos
	// ======================================================================
	// node a task drew with (whole node, before its update)
	static NodeMemo<Node24, 256> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	// the node moved on by one tick since it was drawn (not a different node reusing the slot)
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return n->c == (int16_t)(m->c + 1); }

	// pool records as drawn on the real tick (two owners per record: a few tasks draw the same
	// records of pool A, e.g. the wheel trail draws the puffs' records too)
	struct RecMemo { uint32_t tick; const void *owner; Rec r; };
	template<int N> struct PoolMemo
	{
		RecMemo m[N][2];
		void put(int i, const void *owner, const Rec *r)
		{
			RecMemo *x = &m[i][0];
			if (x->tick == g_real_tick && x->owner != owner) x = &m[i][1];
			x->tick = g_real_tick;
			x->owner = owner;
			x->r = *r;
		}
		const RecMemo *get(int i, int k, const void *owner) const
		{
			const RecMemo &x = m[i][k];
			return x.tick == g_real_tick && x.owner == owner ? &x : nullptr;
		}
	};
	static PoolMemo<170> g_memo_a, g_memo_b;
	static PoolMemo<150> g_memo_c;
	static PoolMemo<120> g_memo_d;
	static PoolMemo<130> g_memo_e;
	static PoolMemo<590> g_memo_f;
	// the record moved on to its next frame on this tick (not freed, not respawned)
	static bool Advanced(const Rec &m, const Rec *cur) { return cur->mask == m.mask && cur->age == (int16_t)(m.age + 1); }

	static void held_note_rec_a(int i, const void *owner, const Rec *r) { g_memo_a.put(i, owner, r); }
	static void held_note_rec_b(int i, const void *owner, const Rec *r) { g_memo_b.put(i, owner, r); }
	static void held_note_rec_c(int i, const void *owner, const Rec *r) { g_memo_c.put(i, owner, r); }
	static void held_note_rec_d(int i, const void *owner, const Rec *r) { g_memo_d.put(i, owner, r); }
	static void held_note_rec_e(int i, const void *owner, const Rec *r) { g_memo_e.put(i, owner, r); }
	static void held_note_rec_f(int i, const void *owner, const Rec *r) { g_memo_f.put(i, owner, r); }

	// held-frame memo of the creature: the skeleton as drawn (pose values + local matrices) and
	// the reader command after that frame, the model matrix drawn with
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		Mat4x3 model;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	static void CreatureMemoTake()
	{
		CreatureMemo &m = g_creature;
		uint8_t *sk = Skeleton();
		m.tick = 0xFFFFFFFF;
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.cmd, Creature() + 0x6C, 8);
		m.model = CreatureMat();
		m.tick = g_real_tick;
	}

	// the headlight flare as FlareTask draws it, at a given x/y scale
	static void FlareDraw(const Mat4x3 *frame, const Node24 *n, int32_t v)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->f12;
		m.t[0] = n->f10;
		m.t[2] = n->f14;
		int32_t sc[3] = { v, v, n->f1C };
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(0xE3A744, 2, 0x33);
	}

	// the glow fade drawn on the real tick (the flicker keeps its 15 Hz steps)
	struct GlowMemo { int32_t fade; };
	static NodeMemo<GlowMemo, 64> g_glow_memo;
	static void held_note_glow(const Node24 *n, int32_t fade) { if (GlowMemo *m = g_glow_memo.put(n)) m->fade = fade; }

	// ======================================================================
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick draws; spawns, flipbook frames, colour-table steps
	// and the glow flicker keep their 15 Hz batches. The effect frame is rebuilt from the
	// held-frame camera while the timeline (its owner) is alive, into a local (never into
	// 0x24FBF90); the rig frames and the chimney bone matrix likewise (locals). Packets go to a
	// private buffer; the morph / model scratch mb+0x37000 is saved and put back.
	// ======================================================================
	static uint8_t g_held_packets[0x80000];
	static const uint32_t MORPH_SAVE = 0x10000;
	static uint8_t g_morph_save[MORPH_SAVE];
	static uint8_t g_skel_cur[SKEL_MAX], g_skel_next[SKEL_MAX];
	static Mat4x3 g_held_bone;
	static Mat4x3 g_held_w[2];

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	// what the creature's next tick does with the pose: 1 = reads one frame and draws, the
	// distance changing by dz; 0 = no step (static pose, a cut, or no draw)
	static int CreatureStep(int32_t c, int32_t *dz)
	{
		*dz = 0;
		if (c >= 36 && c <= 43) { *dz = -0x190; return 1; }
		if (c >= 44 && c <= 68) { *dz = -0x258; return 1; }
		if (c >= 70 && c <= 168) return 1;
		if (c >= 170 && c <= 188) { *dz = -0x3E8; return 1; }
		if (c >= 190 && c <= 210) { *dz = -0x320; return 1; }
		if (c >= 212 && c <= 231) return 1;
		if (c >= 232 && c <= 241) { *dz = -0x708; return 1; }
		if (c >= 243 && c <= 281) { *dz = -0x12C0; return 1; }
		return 0;
	}

	// pose values half way (root offset, bone angles the short way round, bone scales)
	static void PoseBlend(uint8_t *sk, const uint8_t *next, int num, int den)
	{
		int16_t *r = (int16_t *)(sk + 8);
		const int16_t *rn = (const int16_t *)(next + 8);
		for (int k = 0; k < 3; k++) r[k] = L16(r[k], rn[k], num, den);
		for (uint32_t i = 0; i < sk[0]; i++)
		{
			int16_t *a = (int16_t *)(sk + 0x10 + 48 * i + 4);
			const int16_t *b = (const int16_t *)(next + 0x10 + 48 * i + 4);
			for (int k = 0; k < 3; k++) a[k] = lerp_angle(a[k], b[k], num, den);
			for (int k = 3; k < 6; k++) a[k] = L16(a[k], b[k], num, den);
		}
	}

	// the train: the drawn pose, advanced half a frame through the private reader (run on a
	// copy of the command; a finished animation or a phase change holds the pose), its distance
	// half way; the held chimney matrix from the same pose
	static void CreatureHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint32_t size = m.skel_size;
		Mat4x3 model_cur = CreatureMat();
		uint8_t cmd_cur[8];
		memcpy(cmd_cur, E + 0x6C, 8);
		memcpy(g_skel_cur, sk, size);
		memcpy(sk, m.skel, size);
		Mat4x3 model = m.model;
		int32_t dz;
		if (!P1() && CreatureStep(n->c, &dz))
		{
			uint8_t cmd[8];
			memcpy(cmd, m.cmd, 8);
			if (ReaderRead(E + 0x60, cmd) != 1)
			{
				memcpy(g_skel_next, sk, size);
				memcpy(sk, m.skel, size);
				PoseBlend(sk, g_skel_next, num, den);
				ReaderBuildBones(E + 0x60);
				model.t[2] = lerp_i(m.model.t[2], m.model.t[2] + dz, num, den);
			}
			else memcpy(sk, m.skel, size);
		}
		CreatureMat() = model;
		DrawModel(E, frame);
		ComposeAffineTransform(&model, (const Mat4x3 *)(sk + 0x50), &g_held_bone);
		CreatureMat() = model_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, size);
	}

	// lit records of an owner as drawn, moved half way when they moved on
	template<int N> static void HeldLit(const PoolMemo<N> &pm, uint32_t cell, const void *owner, uint8_t *h, bool ir, bool lerp, int num, int den)
	{
		for (int i = 0; i < N; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = pm.get(i, k, owner);
				if (!m) continue;
				const Rec *cur = R(cell, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				int16_t size = m->r.size;
				if (lerp && Advanced(m->r, cur))
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				LitSprite(h, pos, size, m->r.age, ir);
			}
	}

	template<int N> static void HeldOffset(const PoolMemo<N> &pm, uint32_t cell, const void *owner, uint8_t *h, int32_t *mac, int32_t *nv, int shift, bool age2, bool lerp, int num, int den)
	{
		for (int i = 0; i < N; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = pm.get(i, k, owner);
				if (!m) continue;
				const Rec *cur = R(cell, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				int16_t size = m->r.size;
				if (lerp && Advanced(m->r, cur))
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				OffsetSprite(h, mac, nv, pos, size, age2 ? (int16_t)(m->r.age << 1) : m->r.age, shift);
			}
	}

	static void PuffHeld(const Mat4x3 *frame, const Node24 *n, uint32_t seq, bool ir, int16_t w24, bool has_tint, uint32_t tint, const Mat4x3 *pre, int32_t tx, int32_t ty, int32_t tz, bool lerp, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = (uint16_t)w24;
		if (has_tint) *(uint32_t *)(h + 0x1C) = tint;
		LitFrame(s, pre, frame, tx, ty, tz);
		HeldLit(g_memo_a, POOL_A, n, h, ir, lerp, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void SteamHeld(const Mat4x3 *light, const Node24 *n, uint32_t cell, bool pool_c, bool tint, int16_t w24, int shift, bool lerp, uint32_t seq, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(uint32_t *)h = seq;
		if (tint) *(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = (uint16_t)w24;
		memcpy(s + 0x10, light, sizeof(Mat4x3));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
		if (pool_c) HeldOffset(g_memo_c, cell, n, h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), shift, false, lerp, num, den);
		else HeldOffset(g_memo_d, cell, n, h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), shift, true, lerp, num, den);
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void WheelTrailHeld(const Mat4x3 *frame, const Node24 *n)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		memcpy(s + 0x10, frame, sizeof(Mat4x3));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
		for (int i = 0; i < 170; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *a = g_memo_a.get(i, k, n);
				if (!a) continue;
				const RecMemo *b = g_memo_b.get(i, 0, n);
				if (!b) b = g_memo_b.get(i, 1, n);
				if (!b) continue;
				WheelTrailPair(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), &a->r, &b->r, a->r.age, b->r.age);
			}
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void RainHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x50) + 0x10;
		uint8_t *T = (uint8_t *)FieldAlloc(0x44);
		RainTemplate(T);
		memcpy(s, &Camera(), sizeof(Mat4x3));
		GteSetLightMatrix(s);
		GteSetBackColorFromTrans(s);
		*(uint32_t *)T = OT();
		*(uint32_t *)(T + 4) = (var<uint8_t>(0x24FBE80) & 1) ? 0xE : 4;
		uint32_t cursor = Cursor();
		for (int i = 0; i < 590; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = g_memo_f.get(i, k, n);
				if (!m) continue;
				const Rec *cur = R(POOL_F, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				uint32_t color = RAIN_COLOR[m->r.age & 0x3F];
				if (Advanced(m->r, cur) && cur->age < 0x40)
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					color = LerpColor(color, RAIN_COLOR[cur->age], num, den);
				}
				GteLoadV0(pos);
				GteMVMVA_LightV0Bk();
				RainQuad(T, cursor, color, m->r.size);
			}
		Cursor() = cursor;
		FieldFree(0x44);
		FieldFree(0x50);
	}

	static void RigHeld(const Node24 *n, uint32_t fnaddr, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		const Mat4x3 *W = &g_held_w[m->e ? 1 : 0];
		bool mv = NodeMoved(m, n);
		switch (fnaddr)
		{
		case ORIG_RigPrimATask: RigPrimADraw(W, m); break;
		case ORIG_RigPrimBTask: RigPrimBDraw(W, m); break;
		case ORIG_RigRockTask: RigRockDraw(W, m, mv ? lerp_angle(m->f18, n->f18, num, den) : m->f18); break;
		case ORIG_RigFlashTask:
		{
			int32_t f = RigFlashFade(m->c);
			if (mv && m->c >= 2) f = lerp_i(f, RigFlashFade(n->c), num, den);
			RigFlashDraw(W, m, m->c, true, f);
			break;
		}
		case ORIG_RigSpriteTask: RigSpriteDraw(W, m, m->c); break;
		}
	}

	static void SkyHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t ax = mv ? lerp_angle(m->f10, n->f10, num, den) : m->f10;
		int16_t az = mv ? lerp_angle(m->f14, n->f14, num, den) : m->f14;
		int16_t ph = mv ? lerp_angle(m->f20, n->f20, num, den) : m->f20;
		int32_t f = SkyFade(m->c);
		if (mv) f = lerp_i(f, SkyFade(n->c), num, den);
		SkyDraw(m->f18, ax, az, m->f1A, m->f1C, m->f1E, ph, f);
	}

	static void PuffWallHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint32_t grey = WallGrey(m->c);
		if (m->c >= 0x29 && NodeMoved(m, n))
		{
			uint32_t v = (uint32_t)lerp_i(shl32(0x31 - m->c, 4), shl32(0x31 - n->c, 4), num, den);
			grey = (((v << 8) | v) << 8) | v;
		}
		PuffWallSetup(h, s, frame, m->c, true, grey);
		HeldLit(g_memo_a, POOL_A, n, h, false, false, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void DebrisHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int32_t c = m->c, d = m->f16 - 8;
		int16_t sxz = mv ? L16(m->f1C, n->f1C, num, den) : m->f1C;
		int16_t sy = mv ? L16(m->f20, n->f20, num, den) : m->f20;
		int32_t t = shl32(c, 12) / 12, f = shl32(c - d, 9);
		if (mv)
		{
			t = lerp_i(t, shl32(n->c, 12) / 12, num, den);
			if (c >= d) f = lerp_i(f, shl32(n->c - d, 9), num, den);
		}
		DebrisDraw(frame, &g_held_bone, m, sxz, sy, c >= d, f, t);
	}

	static void ExplosionHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x98);
		*(uint32_t *)h = 0xE3460C;
		*(uint16_t *)(h + 0x24) = 0x208;
		if (m->c < 5)
		{
			int16_t sz = m->f1C;
			TransformCameraByShadowRotation(&m->f10, sz, -((int32_t)sz >> 3));
			*(int16_t *)(h + 4) = m->c;
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		*(uint32_t *)h = 0xE34548;
		ExplosionFrame(s, m, frame);
		*(int32_t *)(s + 0x70) = 0x1000;
		*(int32_t *)(s + 0x68) = 0x1000;
		for (int i = 0; i < 130; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *r = g_memo_e.get(i, k, n);
				if (!r) continue;
				const Rec *cur = R(POOL_E, i);
				int16_t pos[4];
				memcpy(pos, r->r.pos, 8);
				if (Advanced(r->r, cur))
					for (int j = 0; j < 3; j++) pos[j] = L16(r->r.pos[j], cur->pos[j], num, den);
				ShardDraw(h, s, pos, r->r.size, r->r.vel, r->r.age);
			}
		FieldFree(0x98);
		FieldFree(0xB4);
	}

	static void LandDustHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(s + 8, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = 0xE34404;
		*(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = 0xC;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		HeldOffset(g_memo_a, POOL_A, n, h, (int32_t *)(s + 0x38), (int32_t *)(s + 0x28), 3, false, true, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		uint8_t *morph = (uint8_t *)(MB() + 0x37000);
		memcpy(g_morph_save, morph, MORPH_SAVE);
		bool timeline = false;
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &Root(), &frame);
		else frame = Frame();
		g_held_w[0] = *RigW(0);
		g_held_w[1] = *RigW(1);
		g_held_bone = BoneWorld();
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			uint32_t f = (uint32_t)t->func;
			switch (f)
			{
			case ORIG_RigTask:
				ComposeAffineTransform(&frame, RigM(n->e), &g_held_w[n->e ? 1 : 0]);
				break;
			case ORIG_RigPrimATask: case ORIG_RigPrimBTask: case ORIG_RigRockTask: case ORIG_RigFlashTask: case ORIG_RigSpriteTask:
				RigHeld(n, f, num, den);
				break;
			case ORIG_SkyTask: SkyHeld(n, num, den); break;
			case ORIG_SteamPuffTask: PuffHeld(&frame, n, 0xE33E40, true, 8, false, 0, nullptr, 0, 0, 0, false, num, den); break;
			case ORIG_SteamPairTask: PuffHeld(&frame, n, 0xE33E40, false, 8, false, 0, nullptr, 0, 0, 0, false, num, den); break;
			case ORIG_RainTask: RainHeld(n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, n, num, den); break;
			case ORIG_WheelTrailTask: WheelTrailHeld(&frame, n); break;
			case ORIG_PuffLineTask: PuffHeld(&frame, n, 0xE33E40, false, 8, false, 0, nullptr, 0, 0, 0, true, num, den); break;
			case ORIG_PuffWallTask: PuffWallHeld(&frame, n, num, den); break;
			case ORIG_FlareTask:
			{
				const Node24 *m = g_node_memo.get(n);
				if (!m) break;
				int32_t v = FlareScale(m->c, m->f1C);
				if (NodeMoved(m, n)) v = lerp_i(v, FlareScale(n->c, n->f1C), num, den);
				FlareDraw(&frame, m, v);
				break;
			}
			case ORIG_GlowTask:
				if (const GlowMemo *g = g_glow_memo.get(n)) GlowDraw(&frame, &g_held_bone, n, g->fade);
				break;
			case ORIG_ChimneyTask: PuffHeld(&frame, n, 0xE342A4, false, 8, false, 0, &g_held_bone, -0xB4, -0x456, -0x2454, true, num, den); break;
			case ORIG_SteamTrailTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 4, true, 0xE342A4, num, den); break;
			case ORIG_SteamStillTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 2, false, 0xE342A4, num, den); break;
			case ORIG_SteamBurstTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 4, true, 0xE342A4, num, den); break;
			case ORIG_WheelSteamTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 3, true, 0xE342A4, num, den); break;
			case ORIG_WheelStillTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 2, false, 0xE342A4, num, den); break;
			case ORIG_WheelBurstTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 2, true, 0xE342A4, num, den); break;
			case ORIG_DebrisTask: if (n->e <= 0) DebrisHeld(&frame, n, num, den); break;
			case ORIG_ExplosionTask: if (n->f16 <= 0) ExplosionHeld(&frame, n, num, den); break;
			case ORIG_LandDustTask: LandDustHeld(n, num, den); break;
			default: break;
			}
		}
		memcpy(morph, g_morph_save, MORPH_SAVE);
		Cursor() = cursor;
	}
}

	// held-frame camera of the shared camera-script task 0x63E9C0 (effects 185, 191, 199 and
	// any other ported module that queues it)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		return d191::CsHeldCamera(num, den, world, lookat);
	}

	static void register_mag191_held()
	{
		using namespace d191;
		mark_held(ORIG_RigPrimATask);
		mark_held(ORIG_RigPrimBTask);
		mark_held(ORIG_RigRockTask);
		mark_held(ORIG_RigFlashTask);
		mark_held(ORIG_RigSpriteTask);
		mark_held(ORIG_SkyTask);
		mark_held(ORIG_SteamPuffTask);
		mark_held(ORIG_SteamPairTask);
		mark_held(ORIG_RainTask);
		mark_held(ORIG_CreatureTask);
		mark_held(ORIG_WheelTrailTask);
		mark_held(ORIG_PuffLineTask);
		mark_held(ORIG_PuffWallTask);
		mark_held(ORIG_FlareTask);
		mark_held(ORIG_GlowTask);
		mark_held(ORIG_ChimneyTask);
		mark_held(ORIG_SteamTrailTask);
		mark_held(ORIG_SteamStillTask);
		mark_held(ORIG_SteamBurstTask);
		mark_held(ORIG_WheelSteamTask);
		mark_held(ORIG_WheelStillTask);
		mark_held(ORIG_WheelBurstTask);
		mark_held(ORIG_DebrisTask);
		mark_held(ORIG_ExplosionTask);
		mark_held(ORIG_LandDustTask);
		register_module_held(191, HeldReady, HeldFrame);
		register_module_camera(191, camscript_held_camera);
	}
}
