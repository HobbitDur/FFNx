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

// 30 fps part of mag218_gilgamesh_kills_seifer.cpp (included at its end). Exact vanilla shapes: the logic stays at
// 15 Hz; a held frame redraws the real tick's draws (a record per drawing task, in the tick's order, with the state it
// drew and the pool records it drew) at num / den of the way to what the next tick draws:
//   nodes    every draw-then-update task: its node as drawn, half way to the node now (= what the next tick draws):
//            angles the short way, scales, heights, uv scrolls, fades (a fade starting on the next tick holds);
//            flare / ring / ground wave / flash / glow / slash parameters are their counter's functions, blended.
//   slash    the target's bone 0xF0 on its next pose (pose_midpoint at 1, its real bone matrices rebuilt after) and
//            the camera eye the next tick saves (0x24FD358 <- the eye after this tick).
//   glow     E2's bone points with E2's next model matrix (the GilTask's next step, predicted), E2's next scale.
//   sword    the sword's matrix before the frame (E1 o bone o turn), half way to the one the next tick builds, then
//            the held frame's frame matrix.
//   models   E1 / E2 head and skeleton as drawn, the midpoint pose (pose_midpoint), the model matrix half way to the
//            GilTask's next one (descent, scale), the fade colour half way; jumps (placements, anim switches) hold.
//   pools    dust / shard dust / chips: position (and the chips' tumble) half way, flipbook frame N (N + 1's at 1).
//   shards   shard position half way along its velocity.
// A part whose task ends with the tick (or whose pool record dies) holds below 1 and is not drawn at 1. A tick with
// the draw-only flags (0x201) moved nothing: its parts hold. Spawns, sounds, loads, texts, the screen grab and the
// camera shake stay on the real ticks; the shards' screen grab is drawn again as drawn (on packet copies); the shake's
// next offset goes to the gate (held_shake_next). Camera: the shared camera-script task's held camera (Doomtrain's
// port), the frame matrix 0x24F23F8 rebuilt from it while the timeline lives and put back after, like the screen
// sprite matrix 0x24F35F8. Model draws use a private vertex scratch; skeletons are put back.

namespace ff8fx
{
namespace g218
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false;
	static int16_t g_eye_next[3]; // the battle camera eye after the real tick (the camera script saves it next tick)

	static int16_t Lerp16(int32_t a, int32_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// ---- records of the real tick ----
	struct PoolRec { int idx; bool a; RecA ra; RecB rb; };
	struct Rec
	{
		uint32_t func;
		const Node24 *node;
		Node24 m;          // the node as drawn
		int16_t x1[3], x2[3];
		int32_t k;
		Mat4x3 mat;
		bool drawn;
		Shard sh;
		int pool0, npool;
	};
	static const int REC_MAX = 256, PREC_MAX = 1024; // pool B records can be drawn by two tasks (dust and shard dust both draw every mask-1 record)
	static Rec g_rec[REC_MAX];
	static int g_nrec = 0;
	static PoolRec g_prec[PREC_MAX];
	static int g_nprec = 0, g_prec_open = 0;
	struct Pending { int16_t x1[3], x2[3]; int32_t k; Mat4x3 mat; bool drawn; };
	static Pending g_pend;

	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct ModelMemo { bool valid, midpoint; uint8_t head[0x9C]; uint32_t skel_size; uint8_t skel[SKEL_MAX]; };
	static ModelMemo g_model[2];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
		g_nrec = 0;
		g_nprec = 0;
		g_prec_open = 0;
		g_model[0].valid = g_model[1].valid = false;
		memset(&g_pend, 0, sizeof(g_pend));
	}

	static void held_note_tick_end() { memcpy(g_eye_next, (const void *)0xB8B7F0, 6); }

	static void MemoExtra(const int16_t *v, int n) { memcpy(g_pend.x1, v, 2 * n); }
	static void MemoExtra2(const int16_t *v, int n, int32_t k) { memcpy(g_pend.x2, v, 2 * n); g_pend.k = k; }
	static void MemoMat(const Mat4x3 *m, bool drawn) { g_pend.mat = *m; g_pend.drawn = drawn; }
	static void PoolMemoB(int i, const RecB *p)
	{
		if (g_nprec >= PREC_MAX) return;
		PoolRec &r = g_prec[g_nprec++];
		r.idx = i;
		r.a = false;
		r.rb = *p;
	}
	static void PoolMemoA(int i, const RecA *p)
	{
		if (g_nprec >= PREC_MAX) return;
		PoolRec &r = g_prec[g_nprec++];
		r.idx = i;
		r.a = true;
		r.ra = *p;
	}

	static void MemoNode(const Node24 *n)
	{
		if (g_nrec >= REC_MAX) return;
		Rec &r = g_rec[g_nrec++];
		r.func = (uint32_t)n->hdr.func;
		r.node = n;
		r.m = *n;
		memcpy(r.x1, g_pend.x1, sizeof(r.x1));
		memcpy(r.x2, g_pend.x2, sizeof(r.x2));
		r.k = g_pend.k;
		r.mat = g_pend.mat;
		r.drawn = g_pend.drawn;
		if (r.func == ORIG_ShardTask) r.sh = *SH(n->e);
		r.pool0 = g_prec_open;
		r.npool = g_nprec - g_prec_open;
		g_prec_open = g_nprec;
		memset(&g_pend, 0, sizeof(g_pend));
	}

	static uint8_t *Skeleton(uint8_t *E)
	{
		uint8_t *com = *(uint8_t **)(E + 0x64);
		return com ? *(uint8_t **)com : nullptr;
	}

	static void TakeModel(int k, uint8_t *E)
	{
		ModelMemo &m = g_model[k];
		m.valid = false;
		m.midpoint = false;
		uint8_t *sk = Skeleton(E);
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.head, E, sizeof(m.head));
		m.valid = true;
	}

	static void ModelMemoTake(const Node24 *n)
	{
		TakeModel(0, E1);
		TakeModel(1, E2);
		MemoNode(n);
	}

	// AdvanceModel read the next pose after the draw: the next tick draws the pose just read
	static void held_note_advanced(uint8_t *E)
	{
		ModelMemo &m = g_model[E == E1 ? 0 : 1];
		if (m.valid) m.midpoint = true;
	}

	// ---- the next tick ----
	static const Node24 *FindTask(TaskQueue &q, uint32_t f)
	{
		for (TaskNode *t = q.head; t; t = t->next)
			if ((uint32_t)t->func == f && (t->flags & 1)) return (const Node24 *)t;
		return nullptr;
	}

	// the node is still linked in its queue with the same task (a task that returned TASK_END was unlinked)
	static bool Alive(const Rec &r)
	{
		TaskQueue &q = r.func == ORIG_ModelDrawTask ? Q2() : Q1();
		for (TaskNode *t = q.head; t; t = t->next)
			if (t == &r.node->hdr) return (uint32_t)t->func == r.func;
		return false;
	}
	static bool Moved(const Rec &r) { return !g_tick_paused && Alive(r) && r.node->c == (int16_t)(r.m.c + 1); }

	// E2's model matrix and the fade colour the next tick's GilTask leaves (jump: a placement, anim: an anim switch)
	struct GilPred { Mat4x3 M; uint32_t colour; bool jump, anim; int32_t anim_id; };
	static GilPred g_pred;

	static void SetDiag(Mat4x3 &M, int16_t s) { M.m[0][0] = s; M.m[1][1] = s; M.m[2][2] = s; }

	static void GilNext(GilPred &g)
	{
		g.M = *(const Mat4x3 *)(E2 + 0x40);
		g.colour = var<uint32_t>(0x24F3580);
		g.jump = false;
		g.anim = false;
		g.anim_id = -1;
		const Node24 *t = FindTask(Q1(), ORIG_GilTask);
		if (!t || g_tick_paused) return;
		const int32_t c = t->c;
		if (c == 0)
		{
			g.jump = true;
			g.anim = true;
			return;
		}
		if (c < 0x14)
		{
			g.M.t[1] += t->s20;
			g.M.t[2] += t->s22;
			SetDiag(g.M, t->s1C);
		}
		else if (c < 0x28)
		{
			if (c == 0x14)
			{
				g.M.t[1] = -5000;
				g.M.t[2] = 0;
				g.jump = true;
			}
			SetDiag(g.M, 0x1000);
		}
		else if (c == 0x28)
		{
			g.M.t[1] = 0;
			g.jump = true;
		}
		static const int32_t SWITCH[5] = { 0x28, 0x42, 0x136, 0x173, 0x191 };
		for (int i = 0; i < 5; i++)
			if (c == SWITCH[i])
			{
				g.anim = true;
				g.anim_id = i + 1;
			}
		const int32_t d = c - 0x191;
		if (d >= 0x6D && d < 0x7D)
		{
			uint8_t gte[0x70 + 0xF0];
			memcpy(gte, (const void *)0x1CA8A10, 0x70);
			memcpy(gte + 0x70, (const void *)0x1CA9210, 0xF0);
			GteSetFarColor(0, 0, 0);
			uint32_t rgbc = 0x2606060;
			GteLoadRGBC(&rgbc);
			GteSetIR0(shl32(d - 0x6D, 12) / 16);
			GteDPCS();
			GteStoreRGB2(&g.colour);
			memcpy((void *)0x1CA8A10, gte, 0x70);
			memcpy((void *)0x1CA9210, gte + 0x70, 0xF0);
		}
	}

	// the GilTask's GTE work of the real tick (the fade colour from its counter 0x1FE: far colour 0 for what follows)
	static void GilHeld(const Rec &r)
	{
		const int32_t d = r.m.c - 0x191;
		if (d < 0x6D || d >= 0x7D) return;
		GteSetFarColor(0, 0, 0);
		uint32_t rgbc = 0x2606060, out;
		GteLoadRGBC(&rgbc);
		GteSetIR0(shl32(d - 0x6D, 12) / 16);
		GteDPCS();
		GteStoreRGB2(&out);
	}

	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t o = 0;
		for (int k = 0; k < 32; k += 8) o |= ((uint32_t)lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return o;
	}

	static void LerpMat(Mat4x3 &o, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) o.m[i][j] = Lerp16(a.m[i][j], b.m[i][j], num, den);
		for (int i = 0; i < 3; i++) o.t[i] = lerp_i(a.t[i], b.t[i], num, den);
	}

	// ---- held draws ----
	// common rule: moved = blend; not moved = hold; ended = hold below 1, nothing at 1
	static bool Skip(const Rec &r, int num, int den) { return num == den && !Alive(r) && !g_tick_paused; }

	static void FadeTileHeld(const Rec &r, int num, int den)
	{
		int32_t v = FadeTileLevel(r.m.c);
		if (Moved(r)) v = lerp_i(v, FadeTileLevel(r.m.c + 1), num, den);
		Tile(v, v, v, 0);
	}

	static void SpinnerHeld(const Rec &r, int num, int den)
	{
		const bool mv = Moved(r);
		SpinnerDraw(&r.m, mv ? lerp_angle(r.m.a18, r.node->a18, num, den) : r.m.a18, r.m.s1C);
	}

	static void FlareHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		if (!Moved(r))
		{
			FlareDraw(&r.m, c, FlareA(c), FlareB(c), FlareDZ(c));
			return;
		}
		FlareDraw(&r.m, num == den ? c + 1 : c, lerp_i(FlareA(c), FlareA(c + 1), num, den), lerp_i(FlareB(c), FlareB(c + 1), num, den),
			lerp_i(FlareDZ(c), FlareDZ(c + 1), num, den));
	}

	static void RingHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		if (!Moved(r))
		{
			RingDraw(&r.m, r.m.a18, RingScale(c));
			return;
		}
		RingDraw(&r.m, lerp_angle(r.m.a18, r.node->a18, num, den), lerp_i(RingScale(c), RingScale(c + 1), num, den));
	}

	static void GroundWaveHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		const bool mv = Moved(r);
		const int32_t c1 = mv ? c + 1 : c;
		GroundWaveDraw(&r.m, lerp_i(mul32(c, 24), mul32(c1, 24), num, den), lerp_i(GroundWaveIR0(c), GroundWaveIR0(c1), num, den));
	}

	static void SlashHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		int16_t p[3] = { r.m.p10, r.m.p12, r.m.p14 }, eye[3] = { r.x1[0], r.x1[1], r.x1[2] };
		int32_t uv = -mul32(c, 24);
		if (Moved(r))
		{
			// the target's bone 0xF0 on its next pose; its real bone matrices back after
			uint8_t *ent = Entity(TargetSlot());
			const int16_t w = var<int16_t>(0x1D9770C);
			int16_t q[4] = {};
			pose_midpoint(ent + 0x60, ent + 0x6C, 1, 1);
			GetEffectSpawnPosition(ent, 0xF0, 0x1000, q);
			BuildBoneMatricesFromPose(ent + 0x60);
			var<int16_t>(0x1D9770C) = w;
			for (int i = 0; i < 3; i++)
			{
				p[i] = Lerp16(p[i], q[i], num, den);
				eye[i] = Lerp16(eye[i], g_eye_next[i], num, den);
			}
			uv = lerp_i(uv, -mul32(c + 1, 24), num, den);
		}
		SlashDraw(p, eye, uv);
	}

	// the flags the sword task reads on the next tick: the timeline (before it in Q1) sets bit 2 for its counters
	// 0x172..0x175 and clears it elsewhere in 0x16A..0x17E; the GilTask (before it too) sets bit 1 at its counter 0x1FB
	static uint32_t NextSwordFlags()
	{
		uint32_t f = Flags();
		if (const Node24 *t = FindTask(Q1(), ORIG_TimelineTask))
		{
			const int32_t d = t->c - 0x16A;
			if (d >= 0 && d < 0x15)
			{
				if (d >= 8 && d <= 0xB) f |= 4;
				else f &= ~4u;
			}
		}
		if (const Node24 *g = FindTask(Q1(), ORIG_GilTask))
			if (g->c == 0x1FB) f |= 2;
		return f;
	}

	// the sword's matrix the next tick builds: E1's matrix and bones as the model draw left them
	static void SwordHeld(const Rec &r, int num, int den)
	{
		Mat4x3 m = r.mat;
		bool drawn = r.drawn;
		if (Moved(r) && !g_pred.anim) // an anim switch on the next tick (SetAnim before the sword task) rebuilds E1's bones: hold
		{
			Mat4x3 next;
			SwordLocal(&next);
			const uint32_t nf = NextSwordFlags();
			const bool next_drawn = !(nf & 4) && !(nf & 2);
			if (num == den)
			{
				m = next;
				drawn = next_drawn;
			}
			else if (drawn && next_drawn) LerpMat(m, r.mat, next, num, den);
		}
		ComposeAffineTransform(&Frame(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		if (drawn) PrimModel(0xE254D8, 0x33, false, 0, OT(), 3);
	}

	static void GlowHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		int16_t A[3] = { r.x1[0], r.x1[1], r.x1[2] }, C[3] = { r.x2[0], r.x2[1], r.x2[2] };
		int32_t k = r.k, uv = shl32(-c, 5);
		if (Moved(r))
		{
			// E2's bone points with the GilTask's next model matrix (its bones as the model draw left them)
			Mat4x3 *M2 = (Mat4x3 *)(E2 + 0x40);
			const Mat4x3 keep = *M2;
			const int16_t w = var<int16_t>(0x1D9770C);
			*M2 = g_pred.M;
			int16_t A1[4], C1[4];
			GlowPoints(A1, C1);
			*M2 = keep;
			var<int16_t>(0x1D9770C) = w;
			for (int i = 0; i < 3; i++)
			{
				A[i] = Lerp16(A[i], A1[i], num, den);
				C[i] = Lerp16(C[i], C1[i], num, den);
			}
			k = lerp_i(k, g_pred.M.m[0][0], num, den);
			uv = lerp_i(uv, shl32(-(c + 1), 5), num, den);
		}
		GlowDraw(&r.m, A, C, k, uv);
	}

	static void ShockRingHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		const bool mv = Moved(r);
		const int16_t s = mv ? Lerp16(r.m.s1C, r.node->s1C, num, den) : r.m.s1C;
		int32_t level = shl32(c - 6, 9);
		if (mv && c >= 6) level = lerp_i(level, shl32(c + 1 - 6, 9), num, den);
		RingPrim(&r.m, 0xE1EF08, 0, s, s, c >= 6, level);
	}

	static void RiseRingHeld(const Rec &r, uint32_t model, int32_t step, int num, int den)
	{
		const int32_t c = r.m.c;
		const bool mv = Moved(r);
		int32_t level = mul32(c - 10, step);
		if (mv && c >= 10) level = lerp_i(level, mul32(c + 1 - 10, step), num, den);
		RingPrim(&r.m, model, mv ? lerp_angle(r.m.a18, r.node->a18, num, den) : r.m.a18, mv ? Lerp16(r.m.s1C, r.node->s1C, num, den) : r.m.s1C,
			mv ? Lerp16(r.m.s20, r.node->s20, num, den) : r.m.s20, c >= 10, level);
	}

	static void ShardHeld(const Rec &r, int num, int den)
	{
		Shard t = r.sh;
		if (Moved(r))
		{
			const Shard *cur = SH(r.m.e);
			t.x = Lerp16(r.sh.x, cur->x, num, den);
			t.y = Lerp16(r.sh.y, cur->y, num, den);
			t.z = Lerp16(r.sh.z, cur->z, num, den);
		}
		ShardDraw(&r.m, &t);
	}

	// the screen grab of the shards' counter 0 (a draw of its own: the stage group into VRAM through the env / area / offset
	// packets): drawn again as drawn, on copies of the packets in the held frame's packet buffer (the module's and the exe's
	// packets keep their OT links of the real tick); not at 1 when the task moved on (its next tick does not grab)
	static void GrabHeld(const Rec &r, int num, int den)
	{
		if (r.m.c != 0 || (num == den && Moved(r))) return;
		GrabPackets pk;
		uint8_t *p = (uint8_t *)Cursor();
		pk.env = (uint32_t)p; p += 0x40;
		pk.stp = (uint32_t)p; p += 8;
		pk.area = (uint32_t)p; p += 0xC;
		pk.ofs = (uint32_t)p; p += 8;
		pk.xy = (uint32_t)p; p += 4;
		const uint32_t n = 4 + 4 * (uint32_t)(*(const uint8_t *)(0xE25AD8 + 3));
		memcpy(p, (const void *)0xE25AD8, n);
		pk.exe = (uint32_t)p; p += (n + 3) & ~3u;
		Cursor() = (uint32_t)p;
		ScreenGrab(&r.m.p10, pk);
	}

	static void FlashHeld(const Rec &r, int num, int den)
	{
		const int32_t c = r.m.c;
		const bool mv = Moved(r);
		int32_t uv = mul32(-3 - c, 28), level = shl32(c - 6, 10);
		if (mv)
		{
			uv = lerp_i(uv, mul32(-4 - c, 28), num, den);
			if (c >= 6) level = lerp_i(level, shl32(c + 1 - 6, 10), num, den);
		}
		FlashDraw(uv, c >= 6, level);
	}

	// pool records: the record as drawn, half way to the record now when it moved on (same mask, next age); a record
	// that died holds below 1, none at 1; at 1 a moved record is the record now (its next flipbook frame)
	static bool PoolStep(const PoolRec &p, int num, int den, RecA *a, RecB *b)
	{
		uint8_t *pool = p.a ? PoolA() : PoolB();
		if (p.a)
		{
			const RecA *cur = (const RecA *)(pool + 0x20 * p.idx);
			*a = p.ra;
			if (g_tick_paused) return true;
			if (cur->mask != p.ra.mask || cur->age != (int16_t)(p.ra.age + 1)) return num != den;
			if (num == den) { *a = *cur; return true; }
			a->x = Lerp16(p.ra.x, cur->x, num, den);
			a->y = Lerp16(p.ra.y, cur->y, num, den);
			a->z = Lerp16(p.ra.z, cur->z, num, den);
			a->ax = lerp_angle(p.ra.ax, cur->ax, num, den);
			a->az = lerp_angle(p.ra.az, cur->az, num, den);
			return true;
		}
		const RecB *cur = (const RecB *)(pool + 0x18 * p.idx);
		*b = p.rb;
		if (g_tick_paused) return true;
		if (cur->mask != p.rb.mask || cur->age != (int16_t)(p.rb.age + 1)) return num != den;
		if (num == den) { *b = *cur; return true; }
		b->x = Lerp16(p.rb.x, cur->x, num, den);
		b->y = Lerp16(p.rb.y, cur->y, num, den);
		b->z = Lerp16(p.rb.z, cur->z, num, den);
		return true;
	}

	static void PoolHeld(const Rec &r, int num, int den)
	{
		if (r.func == ORIG_ShardChipTask)
		{
			uint8_t *s = (uint8_t *)FieldAlloc(0x68);
			uint8_t *h = (uint8_t *)FieldAlloc(0x58);
			ChipSetup(h, s);
			for (int i = 0; i < r.npool; i++)
			{
				RecA a;
				if (PoolStep(g_prec[r.pool0 + i], num, den, &a, nullptr)) ChipOne(h, s, &a);
			}
			FieldFree(0x58);
			FieldFree(0x68);
			return;
		}
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		const bool dust = r.func == ORIG_DustTask;
		if (dust) DustSetup(h, s);
		else ShardDustSetup(h, s);
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		for (int i = 0; i < r.npool; i++)
		{
			RecB b;
			if (!PoolStep(g_prec[r.pool0 + i], num, den, nullptr, &b)) continue;
			if (dust) DustOne(h, s, &b);
			else ShardDustOne(h, &b);
		}
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	// ---- models: the drawn head and skeleton put back, the midpoint pose, the matrix / colour half way ----
	static uint8_t g_head_cur[0x9C];
	static uint8_t g_skel_cur[SKEL_MAX];
	static uint8_t g_skel_next[SKEL_MAX];
	static uint8_t g_vertex_scratch[0x40000];

	static void ModelHeld(int k, uint8_t *E, bool mv, int num, int den)
	{
		ModelMemo &m = g_model[k];
		if (!m.valid) return;
		uint8_t *sk = Skeleton(E);
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		memcpy(g_head_cur, E, sizeof(g_head_cur));
		memcpy(g_skel_cur, sk, m.skel_size);
		// the next tick's GilTask switches the animation before the draw: the pose it draws is the new animation's
		const bool sw = mv && g_pred.anim && g_pred.anim_id >= 0;
		if (sw)
		{
			SetAnim(E, g_pred.anim_id);
			memcpy(g_skel_next, sk, m.skel_size);
		}
		memcpy(E, m.head, sizeof(m.head));
		memcpy(sk, m.skel, m.skel_size);
		if (mv)
		{
			Mat4x3 *M = (Mat4x3 *)(E + 0x40);
			const Mat4x3 drawn = *M;
			if (!g_pred.jump) LerpMat(*M, drawn, g_pred.M, num, den);
			else if (num == den) *M = g_pred.M;
			*(uint32_t *)(E + 0x28) = LerpColour(*(uint32_t *)(E + 0x28), g_pred.colour, num, den); // 0x24F3580 is E2's colour (+0x28), E1 copies it
			// the pose the next tick draws is the skeleton now (AdvanceModel read it; a completed animation restarted)
			if (sw) pose_midpoint_to(E + 0x60, g_skel_next, num, den);
			else if (m.midpoint) pose_midpoint_to(E + 0x60, g_skel_cur, num, den);
		}
		DrawModel(E, &Frame());
		memcpy(E, g_head_cur, sizeof(g_head_cur));
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void ModelsHeld(const Rec &r, int num, int den)
	{
		const bool mv = Moved(r);
		ModelHeld(0, E1, mv, num, den);
		ModelHeld(1, E2, mv, num, den);
	}

	static void HeldRec(const Rec &r, int num, int den)
	{
		if (r.func == ORIG_GilTask)
		{
			GilHeld(r);
			return;
		}
		if (Skip(r, num, den)) return;
		switch (r.func)
		{
		case ORIG_FadeTileTask: FadeTileHeld(r, num, den); break;
		case ORIG_TileTask: Tile(0x70, 0x80, 0xF0, 0x1000); break;
		case ORIG_SpinnerTask: SpinnerHeld(r, num, den); break;
		case ORIG_FlareTask: FlareHeld(r, num, den); break;
		case ORIG_RingTask: RingHeld(r, num, den); break;
		case ORIG_GroundWaveTask: GroundWaveHeld(r, num, den); break;
		case ORIG_SlashTask: SlashHeld(r, num, den); break;
		case ORIG_SwordTask: SwordHeld(r, num, den); break;
		case ORIG_GlowTask: GlowHeld(r, num, den); break;
		case ORIG_DustTask:
		case ORIG_ShardDustTask:
		case ORIG_ShardChipTask: PoolHeld(r, num, den); break;
		case ORIG_ShockRingTask: ShockRingHeld(r, num, den); break;
		case ORIG_RisingRingTask: RiseRingHeld(r, 0xE1C918, 341, num, den); break;
		case ORIG_WideRingTask: RiseRingHeld(r, 0xE1E260, 682, num, den); break;
		case ORIG_ShardTask: ShardHeld(r, num, den); break;
		case ORIG_FlashTask: FlashHeld(r, num, den); break;
		case ORIG_ShardsTask: GrabHeld(r, num, den); break;
		case ORIG_ModelDrawTask: ModelsHeld(r, num, den); break;
		default: break; // shake (gate)
		}
	}

	static uint8_t g_held_packets[0x100000];
	static uint32_t g_held_used = sizeof(g_held_packets); // bytes the last held frame wrote (cleared before the next: the model
	                                                      // renderers leave packet pad words as the buffer held them)
	static uint8_t g_scratch_save[0x1000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the camera shake the next tick leaves (the shake task writes the y offset every tick it lives)
	static void HeldShake()
	{
		const Node24 *t = FindTask(Q1(), ORIG_ShakeTask);
		if (!t || g_tick_paused) return;
		const int16_t next[3] = { 0, ShakeValue(t->c, t->e), 0 };
		held_shake_next(next, 2);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const uint32_t cursor = Cursor();
		memset(g_held_packets, 0, g_held_used);
		Cursor() = (uint32_t)g_held_packets;
		// the Field_Alloc scratch above the pointer as the real tick left it (the next real tick's headers read fields they do
		// not set), and the polygon parser's packet / record cursors 0x1D99C18 (RenderGeometry)
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		if (scratch) memcpy(g_scratch_save, scratch, sizeof(g_scratch_save));
		uint8_t objcur[8];
		memcpy(objcur, (const void *)0x1D99C18, 8);
		g_vertex_scratch_override = (uint32_t)g_vertex_scratch;
		const Mat4x3 frame = Frame();
		uint8_t screen_mat[0x20];
		memcpy(screen_mat, (const void *)0x24F35F8, sizeof(screen_mat));
		const int16_t bonew = var<int16_t>(0x1D9770C);
		if (FindTask(Q1(), ORIG_TimelineTask)) ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		GilNext(g_pred);
		for (int i = 0; i < g_nrec; i++) HeldRec(g_rec[i], num, den);
		HeldShake();
		g_held_used = Cursor() - (uint32_t)g_held_packets;
		if (g_held_used > sizeof(g_held_packets)) g_held_used = sizeof(g_held_packets);
		memcpy((void *)0x1D99C18, objcur, 8);
		if (scratch) memcpy(scratch, g_scratch_save, sizeof(g_scratch_save));
		var<int16_t>(0x1D9770C) = bonew;
		memcpy((void *)0x24F35F8, screen_mat, sizeof(screen_mat));
		Frame() = frame;
		g_vertex_scratch_override = 0;
		Cursor() = cursor;
	}

	// the shared camera-script task's held camera; the Field_Alloc scratch above the pointer put back (as 200)
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		static uint8_t save[0x1000];
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		if (scratch) memcpy(save, scratch, sizeof(save));
		const bool r = camscript_held_camera(num, den, world, lookat);
		if (scratch) memcpy(scratch, save, sizeof(save));
		return r;
	}
}

	static void register_mag218_held()
	{
		using namespace g218;
		mark_held(ORIG_FadeTileTask);
		mark_held(ORIG_TileTask);
		mark_held(ORIG_SpinnerTask);
		mark_held(ORIG_FlareTask);
		mark_held(ORIG_RingTask);
		mark_held(ORIG_GroundWaveTask);
		mark_held(ORIG_SlashTask);
		mark_held(ORIG_SwordTask);
		mark_held(ORIG_GlowTask);
		mark_held(ORIG_DustTask);
		mark_held(ORIG_ShardDustTask);
		mark_held(ORIG_ShardChipTask);
		mark_held(ORIG_ShockRingTask);
		mark_held(ORIG_RisingRingTask);
		mark_held(ORIG_WideRingTask);
		mark_held(ORIG_ShardTask);
		mark_held(ORIG_FlashTask);
		mark_held(ORIG_ModelDrawTask);
		mark_held(ORIG_ShardsTask);
		register_module_held(218, HeldReady, HeldFrame);
		register_module_camera(218, HeldCamera);
	}
}
