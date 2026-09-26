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

// 30 fps part of mag291_pandemona.cpp: memos of what the real tick drew, the held-frame draw
// passes (Pass.held / Pass.interp), the held-frame drawing and camera (included at the end of
// mag291_pandemona.cpp)

namespace ff8fx
{
namespace p291
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// ------------------------------------------------------------------
	// Held-frame memo of the real tick
	// ------------------------------------------------------------------
	struct CreatureMemo
	{
		uint32_t tick;
		uint8_t *nd;
		int32_t c;
		bool pose_ok;
		uint32_t pose_size;
		uint8_t pose[16 + 48 * 64];
	};
	static CreatureMemo g_cm;

	static uint8_t *CreatureSkeleton(uint8_t *nd, uint32_t *size)
	{
		uint8_t *com = At<uint8_t *>(nd, 0x1B8); // E + 0x60 BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0 || sk[0] > 64) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return sk;
	}

	struct TrailMemo { int16_t spin, scale; };
	static NodeMemo<TrailMemo, 16> g_trail_memo;

	// ------------------------------------------------------------------
	// Memo writes of the real tick (the FX_HELD statements of mag291_pandemona.cpp)
	// ------------------------------------------------------------------
	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
	}

	// timeline: the skeleton as the tick starts is the pose this tick draws (the draw comes
	// before this tick's anim advance)
	static void held_note_timeline(uint8_t *nd, int32_t c)
	{
		g_cm.tick = g_real_tick;
		g_cm.nd = nd;
		g_cm.c = c;
		{
			uint32_t size = 0;
			uint8_t *sk = CreatureSkeleton(nd, &size);
			g_cm.pose_ok = sk && size <= sizeof(g_cm.pose);
			if (g_cm.pose_ok) { memcpy(g_cm.pose, sk, size); g_cm.pose_size = size; }
		}
	}

	static void held_note_trail(uint8_t *nd)
	{
		if (TrailMemo *m = g_trail_memo.put(nd))
		{
			m->spin = At<int16_t>(nd, 0x16);
			m->scale = At<int16_t>(nd, 0x1A);
		}
	}

	// ------------------------------------------------------------------
	// Held draw passes (Pass.held: draw-only pass of a held frame, prim players replay and
	// nothing advances; Pass.interp: the next tick is c + 1, parameters lerp towards it)
	// ------------------------------------------------------------------
	static bool held_play(const Pass &p, prim::Layout *l, uint8_t *ctx)
	{
		if (!p.held) return false;
		prim::play_held(l, (prim::Callback)CB_Prim, (int)ctx, p.num, p.den);
		return true;
	}

	// a scalar draw parameter towards its value at tick c + 1 (next_in: c + 1 is in the block)
	static void held_lerp(const Pass &p, bool next_in, int32_t &x, int32_t x1)
	{
		if (p.interp && next_in && !Pause()) x = lerp_i(x, x1, p.num, p.den);
	}

	// numeric draw parameters of a block at a tick; a held frame lerps two of them field by field
	template<typename T> static void LerpPar(T &a, const T &b, int num, int den)
	{
		static_assert(sizeof(T) % 4 == 0, "int32 fields only");
		int32_t *x = (int32_t *)&a;
		const int32_t *y = (const int32_t *)&b;
		for (size_t i = 0; i < sizeof(T) / 4; i++) x[i] = lerp_i(x[i], y[i], num, den);
	}

	// held parameters: a = P(v) lerped to P(v + 1) when tick c + 1 is in the same block and has
	// the same shape, else kept
	template<typename T, typename G, typename S> static void HeldPar(const Pass &p, int32_t v, bool next_in, G gen, S same, T &a)
	{
		if (!p.interp || !next_in || Pause()) return;
		T b;
		gen(v + 1, b);
		if (same(a, b)) LerpPar(a, b, p.num, p.den);
	}

	static void held_par_b(const Pass &p, int32_t v, ParB &r)
	{
		HeldPar<ParB>(p, v, (uint32_t)(p.c + 1 - 24) < 0x10, GenB, [](const ParB &, const ParB &) { return true; }, r);
	}

	static void held_par_c(const Pass &p, int32_t v, ParC &r)
	{
		HeldPar<ParC>(p, v, (uint32_t)(p.c + 1 - 40) < 0x3C, GenC, [](const ParC &a, const ParC &b) { return a.third == b.third; }, r);
	}

	static void held_par_d(const Pass &p, int32_t v, ParD &r)
	{
		HeldPar<ParD>(p, v, (uint32_t)(p.c + 1 - 100) < 0x36, GenD,
			[](const ParD &a, const ParD &b) { return a.on == b.on && a.more == b.more && (a.v36 != 0) == (b.v36 != 0); }, r);
	}

	static void held_par_j(const Pass &p, int32_t v, ParJ &r)
	{
		HeldPar<ParJ>(p, v, (uint32_t)(p.c + 1 - 335) < 0x2A, GenJ, [](const ParJ &, const ParJ &) { return true; }, r);
	}

	static void held_par_k(const Pass &p, int32_t v, ParK &r)
	{
		if (!(p.interp && (uint32_t)(p.c + 1 - 377) < 0x28 && !Pause())) return;
		ParK r0 = r, r1;
		GenK(v + 1, r1);
		for (int k = 0; k < 4; k++)
			for (int j = 0; j < 10; j++) r.r[k][j] = lerp_i(r0.r[k][j], r1.r[k][j], p.num, p.den);
		for (int k = 0; k < 4; k++)
		{
			ParK::Tb &a = r.tb[k];
			const ParK::Tb &b = r1.tb[k];
			if (!a.vis || !b.vis || a.flags != b.flags) continue;
			a.fade = lerp_i(a.fade, b.fade, p.num, p.den);
			a.ang = lerp_i(a.ang, b.ang, p.num, p.den);
			a.scale = lerp_angle((int16_t)a.scale, (int16_t)b.scale, p.num, p.den);
		}
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). The logic stays vanilla; held frames only draw, on copies or on
	// state that is put back byte for byte afterwards (creature node, entity array, skeleton).
	// ------------------------------------------------------------------
	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static uint8_t *Creature()
	{
		TaskNode *t = QCreature().head;
		return (t && (uint32_t)t->func == ORIG_TimelineTask) ? (uint8_t *)t : nullptr;
	}

	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose after the tick (the one the next tick draws), from the pose drawn
		// this tick: write the midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *q = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*q = (int16_t)(f + ((int32_t)*q - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *q = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)q[a] - f[a] + 2048) & 4095) - 2048;
				q[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) q[a] = (int16_t)(f[a] + ((int32_t)q[a] - f[a]) * num / den);
		}
	}

	static const uint32_t ENT_BASE = 0x1D972C0, ENT_SIZE = 0x440; // BattleEntitySlotData[7]
	static uint8_t g_node_save[0x1030], g_ent_save[ENT_SIZE], g_skel_save[16 + 48 * 64];
	static uint8_t g_hfr[0xB0];

	// Next tick's particles and targets (c1 = the next tick), run on the real node / entities
	// (pure math: no generator, no spawn, no event) and put back by the caller. parts = also
	// the particle updates. hand = the hand position at the next tick's pose (null: unknown).
	// Returns false when the next tick changes the particles at random (tick 124).
	static bool PredictNext(uint8_t *nd, int32_t c1, const int16_t *hand, bool parts)
	{
		bool particles = parts;
		uint8_t *fr = g_hfr;
		if (Pause()) return particles;
		uint32_t v;
		if (parts && (c1 == 0x28 || c1 == 0x9A || c1 == 0xE0 || c1 == 0xFE || c1 == 0x14F || c1 == 0x179)) w::PartReset(nd);
		if (parts && (v = (uint32_t)(c1 - 10)) < 0x1E) w::PartFall(nd);
		if (parts && (v = (uint32_t)(c1 - 0x28)) < 0x3C)
		{
			int32_t e = (int32_t)((uint32_t)shl32(v, 12) / 60u);
			Mat4x3 m;
			w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, &m);
			m.t[0] = 0; m.t[1] = 0; m.t[2] = Center();
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			w::PartOrbit(nd, shl32(v, 7), e >> 1);
		}
		if ((v = (uint32_t)(c1 - 0x64)) < 0x36)
		{
			if (v <= 0x18)
			{
				At<int32_t>(nd, 0x1AC) = (int32_t)(shl32(v, 13) / 24) - 0x2000;
				if (parts)
				{
					Mat4x3 m;
					w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, &m);
					m.t[0] = 0; m.t[1] = 0; m.t[2] = Center();
					GteSetRotMatrix(&m);
					GteSetTransVector(&m);
					w::PartOrbit(nd, 0x3C, 0x800);
				}
				if (v == 0x18) particles = false;
			}
			else if (parts) w::PartBounce(nd);
		}
		if (parts && (v = (uint32_t)(c1 - 0xB8)) < 0x28) w::PartDrift(nd);
		if ((v = (uint32_t)(c1 - 0xE0)) < 0x1E)
		{
			if (parts) w::PartDrift(nd);
			MotionSink(nd, (int32_t)v);
		}
		if ((v = (uint32_t)(c1 - 0xFE)) < 0x1E)
		{
			if (parts) w::PartDrift(nd);
			if (hand)
			{
				F16(fr, 0x2C) = hand[0];
				F16(fr, 0x2E) = hand[1];
				F16(fr, 0x30) = hand[2];
				MotionLift(nd, fr, (int32_t)v);
			}
		}
		if (parts && (v = (uint32_t)(c1 - 0x11C)) < 0x24)
		{
			if (v == 0)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				for (int k = 0; k < 0x80; k++, q += 12)
					if (q[3]) { q[4] >>= 4; q[6] >>= 4; q[3] = 0x24; q[7] = 0x800; }
			}
			w::PartBounce(nd);
		}
		if ((v = (uint32_t)(c1 - 0x14F)) < 0x2A)
		{
			if (parts) w::PartFall(nd);
			if (hand)
			{
				F16(fr, 0x1C) = hand[0];
				F16(fr, 0x1E) = hand[1];
				F16(fr, 0x20) = hand[2];
				MotionSpin(nd, fr, (int32_t)v);
			}
		}
		if ((v = (uint32_t)(c1 - 0x179)) < 0x28)
		{
			if (v == 0) OrbitReset(nd);
			if (parts)
			{
				Mat4x3 m;
				w::MatY(shl32(v, 6), 0x5800, 0x2000, &m);
				m.t[0] = 0; m.t[1] = 0; m.t[2] = Ground();
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
				w::PartOrbit(nd, 0x28, 0x800);
			}
			MotionOrbit(nd, (int32_t)v);
		}
		if (parts && (v = (uint32_t)(c1 - 0x1A1)) < 0x17)
		{
			if (v == 0)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				for (int k = 0; k < 0x80; k++, q += 12)
					if (q[3]) { q[3] += 0x28; q[4] >>= 2; q[5] >>= 2; q[6] >>= 2; }
			}
			w::PartBounce(nd);
		}
		if ((v = (uint32_t)(c1 - 0x1B8)) < 0x28)
		{
			if (v == 0) DropReset(nd);
			MotionDrop(nd, (int32_t)v, false);
			if (parts) w::PartBounce(nd);
		}
		return particles;
	}

	// the hand position (bone vector 0x13B7BB0) at the next tick's pose: the skeleton's bone
	// matrices already hold it (the anim advanced after this tick's draw)
	static bool NextHand(uint8_t *nd, int16_t *hand)
	{
		uint32_t size = 0;
		if (!CreatureSkeleton(nd, &size)) return false;
		int16_t h[4];
		w::BonePos(E(nd), 0x13B7BB0, h);
		memcpy(hand, h, 6);
		return true;
	}

	inline bool Jump(int32_t a, int32_t b) { return a - b > 1500 || b - a > 1500; }

	static int16_t g_next_pool[0x80 * 12];
	static uint8_t g_next_recs[0x140];
	static int16_t g_next_pos[10][3];
	static int16_t g_held_pos[10][3]; // held target positions (also used by the target prims)
	static uint8_t *g_held_nd = nullptr;

	static int Targets() { int n = Count(); return n > 10 ? 10 : n; }

	// the timeline's draws of tick c (memo) at c + num / den, in the tick's order, on the real
	// node / entities / skeleton moved to the in-between state and put back byte for byte
	static void CreatureHeld(int num, int den)
	{
		uint8_t *nd = Creature();
		g_held_nd = nullptr;
		if (!nd || g_cm.tick != g_real_tick || g_cm.nd != nd) return;
		int32_t c = g_cm.c;
		int32_t c1 = At<int16_t>(nd, 0xC); // the next tick (== c when this tick waited on a load)
		bool interp = c1 == c + 1;
		int n = Targets();

		memcpy(g_node_save, nd, sizeof(g_node_save));
		memcpy(g_ent_save, (void *)ENT_BASE, ENT_SIZE);
		uint32_t size = 0;
		uint8_t *sk = CreatureSkeleton(nd, &size);
		if (sk) memcpy(g_skel_save, sk, size);

		if (interp)
		{
			int16_t hand[3];
			bool hand_ok = NextHand(nd, hand);
			bool next_particles = PredictNext(nd, c1, hand_ok ? hand : nullptr, true);
			memcpy(g_next_pool, nd + 0x42C, sizeof(g_next_pool));
			memcpy(g_next_recs, nd + 0x14, sizeof(g_next_recs));
			int32_t root1 = At<int32_t>(nd, 0x1AC);
			for (int i = 0; i < n; i++) memcpy(g_next_pos[i], Ent(nd, i) + 0x1C, 6);
			memcpy(nd, g_node_save, sizeof(g_node_save));
			memcpy((void *)ENT_BASE, g_ent_save, ENT_SIZE);

			// particles: the drawn state -> the next update
			if (next_particles)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				const int16_t *r = g_next_pool;
				for (int k = 0; k < 0x80; k++, q += 12, r += 12)
				{
					if (q[3] == 0 || r[3] == 0) continue;
					for (int a = 0; a < 3; a++) q[a] = (int16_t)lerp_i(q[a], r[a], num, den);
					q[7] = (int16_t)lerp_i(q[7], r[7], num, den);
					q[8] = lerp_angle(q[8], r[8], num, den);
					q[9] = lerp_angle(q[9], r[9], num, den);
				}
			}
			// targets: position (held on jumps), angles, draw scale
			for (int i = 0; i < n; i++)
			{
				uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
				const uint8_t *nr = g_next_recs + 0x20 * i;
				int16_t *pos = &At<int16_t>(ent, 0x1C);
				bool cut = false;
				for (int a = 0; a < 3; a++) cut |= Jump(pos[a], g_next_pos[i][a]);
				if (cut) continue;
				for (int a = 0; a < 3; a++) pos[a] = (int16_t)lerp_i(pos[a], g_next_pos[i][a], num, den);
				for (int a = 0; a < 3; a++)
					At<int16_t>(rec, 0x14 + 2 * a) = lerp_angle(At<int16_t>(rec, 0x14 + 2 * a), At<int16_t>(nr, 0x14 + 2 * a), num, den);
				At<int16_t>(rec, 0xA) = (int16_t)lerp_i(At<int16_t>(rec, 0xA), At<int16_t>(nr, 0xA), num, den);
			}
			// the creature's root height (rising 100..124)
			At<int32_t>(nd, 0x1AC) = lerp_i(At<int32_t>(nd, 0x1AC), root1, num, den);
		}
		for (int i = 0; i < n; i++) memcpy(g_held_pos[i], Ent(nd, i) + 0x1C, 6);
		g_held_nd = nd;

		// the pose drawn this tick (tick start) -> the pose after the advance
		if (sk && g_cm.pose_ok && size == g_cm.pose_size)
		{
			PoseBlend(sk, g_cm.pose, num, den);
			BuildBoneMatricesFromPose(nd + 0x1B4);
		}

		memcpy(g_hfr, g_fr, sizeof(g_hfr));
		Pass p = { nd, g_hfr, c, num, den, true, interp };
		uint32_t v;
		if ((v = (uint32_t)(c - 10)) < 0x1E)
		{
			if (v >= 8) DrawA(p, (int32_t)v);
			if (v > 0xE) DrawB(p, (int32_t)v - 0xE);
		}
		if ((v = (uint32_t)(c - 0x28)) < 0x3C) DrawC(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0x64)) < 0x36) DrawD(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0x90)) < 0x1E) DrawE(p);
		if ((v = (uint32_t)(c - 0xB8)) < 0x28)
			DrawBonePrim(p, 0x1C, (int32_t)(v * v), (int32_t)((v + 1) * (v + 1)), (uint32_t)(c + 1 - 0xB8) < 0x28, -128, 0x13EF438);
		if ((v = (uint32_t)(c - 0xE0)) < 0x1E) DrawG(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0xFE)) < 0x1E)
		{
			bool nx = interp && (uint32_t)(c + 1 - 0xFE) < 0x1E && !Pause();
			int32_t s = shl32(v, 5);
			DrawBonePrim(p, 0x2C, s, shl32(v + 1, 5), nx, 0x40, 0x13EF42C);
			DrawRing(nx ? lerp_i(s, shl32(v + 1, 5), num, den) : s, false, 0);
		}
		if ((v = (uint32_t)(c - 0x11C)) < 0x24)
		{
			bool nx = interp && (uint32_t)(c + 1 - 0x11C) < 0x24 && !Pause();
			int32_t s = shl32(v, 5), f = (int32_t)((uint32_t)shl32(v, 12) >> 4);
			DrawBonePrim(p, 0x1C, s, shl32(v + 1, 5), nx, 0x80, 0x13EF42C);
			if (nx)
			{
				s = lerp_i(s, shl32(v + 1, 5), num, den);
				f = lerp_i(f, (int32_t)((uint32_t)shl32(v + 1, 12) >> 4), num, den);
			}
			DrawRing(s, true, f);
		}
		if ((v = (uint32_t)(c - 0x14F)) < 0x2A)
		{
			w::BonePos(E(nd), 0x13B7BB0, g_hfr + 0x1C);
			DrawJ(p, (int32_t)v);
		}
		if ((v = (uint32_t)(c - 0x179)) < 0x28) DrawK(p, (int32_t)v);
		w::PartDraw(nd);
		if (c >= 0xE0) DrawTargets(p);
		if (CreatureDrawn(c))
			FrameCursor() = w::DrawCreature(E(nd), 0x2000, (uint32_t)(MB() + 0x31D4), FrameCursor(), var<uint32_t>(0x1D969A8));

		if (sk) memcpy(sk, g_skel_save, size);
		memcpy(nd, g_node_save, sizeof(g_node_save));
		memcpy((void *)ENT_BASE, g_ent_save, ENT_SIZE);
	}

	// held target position of a record (falling / landed target prims)
	static const int16_t *HeldTargetPos(uint8_t *rec)
	{
		if (g_held_nd)
		{
			int32_t i = (int32_t)(rec - (g_held_nd + 0x14));
			if (i >= 0 && (i & 0x1F) == 0 && (i >> 5) < 10 && (i >> 5) < Count()) return g_held_pos[i >> 5];
		}
		return &At<int16_t>(At<uint8_t *>(rec, 0), 0x1C);
	}

	static void TargetPrimHeld(uint8_t *nd, int num, int den)
	{
		uint8_t *rec = At<uint8_t *>(nd, 0xC);
		static uint8_t ctx[0x2C];
		TargetPrimCtx(ctx, rec, HeldTargetPos(rec));
		prim::play_held((prim::Layout *)(nd + 0x10), (prim::Callback)CB_Prim, (int)ctx, num, den);
	}

	// swirl: drawn at (angle - step, frame - 1) this tick; the angle turns on in between
	static void SwirlHeld(uint8_t *nd, int num, int den)
	{
		int16_t f = At<int16_t>(nd, 0xC), a = At<int16_t>(nd, 0x10), d = At<int16_t>(nd, 0x12);
		if (Pause()) { SwirlDraw(nd, a, f); return; }
		if (f <= 0) return;
		SwirlDraw(nd, (int16_t)(a - d + d * num / den), (int16_t)(f - 1));
	}

	static void RiseHeld(uint8_t *nd, int num, int den)
	{
		int16_t f = At<int16_t>(nd, 0x12);
		const int16_t *pos = &At<int16_t>(nd, 0xC);
		if (Pause()) { RiseDraw(nd, pos, f); return; }
		if (f <= 0) return;
		int16_t h[3];
		for (int k = 0; k < 3; k++)
		{
			int32_t vk = shl32((int8_t)nd[0x14 + k], 4);
			h[k] = (int16_t)(pos[k] - vk + vk * num / den);
		}
		RiseDraw(nd, h, (int16_t)(f - 1));
	}

	// trail: spin/scale between the drawn and the current values, brightness half way to the
	// next fade step (drawn on a copy, with the pause flag up so the copy is not faded again)
	static uint8_t g_trail_copy[0x1E * 0x24];
	static void TrailHeld(uint8_t *nd, int num, int den)
	{
		const TrailMemo *m = g_trail_memo.get(nd);
		if (!m) return;
		int32_t cnt = At<int32_t>(nd, 0x20);
		if (cnt < 0 || cnt > 0x1E) return;
		memcpy(g_trail_copy, nd + 0x24, cnt * 0x24);
		if (!Pause())
			for (int32_t k = 0; k < cnt; k++)
			{
				uint8_t *e = g_trail_copy + 0x24 * k;
				int32_t b0 = e[0x20];
				int8_t d = (int8_t)(e[0x21] - 4);
				int32_t b1 = b0 + (d >> 3);
				if (b1 < 0) b1 = 0;
				e[0x20] = (uint8_t)lerp_i(b0, (uint8_t)b1, num, den);
			}
		int16_t spin = Pause() ? At<int16_t>(nd, 0x16) : lerp_angle(m->spin, At<int16_t>(nd, 0x16), num, den);
		int16_t scale = Pause() ? At<int16_t>(nd, 0x1A) : (int16_t)lerp_i(m->scale, At<int16_t>(nd, 0x1A), num, den);
		uint8_t mat[0x20];
		TrailMatrix(mat, spin, scale);
		uint32_t pause = Pause();
		Pause() = 1;
		int32_t c2 = cnt;
		PacketCursor() = w::TrailDraw(&c2, g_trail_copy, -4, RenderOT(), 2, PacketCursor());
		Pause() = pause;
	}

	static uint8_t g_held_packets[0x100000];
	static uint8_t g_scratch_save[0x4000];

	// mirrors the master's queue order; packets go to a private buffer, the vertex scratch at
	// modelBuffer + 0x31D4 is put back
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		memcpy(g_scratch_save, MB() + 0x31D4, sizeof(g_scratch_save));
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x40000;
		CreatureHeld(num, den);
		for (TaskNode *t = QFlip().head; t; t = t->next)
		{
			if ((uint32_t)t->func == ORIG_SwirlFlipTask) SwirlHeld((uint8_t *)t, num, den);
			else if ((uint32_t)t->func == ORIG_RiseFlipTask) RiseHeld((uint8_t *)t, num, den);
		}
		for (TaskNode *t = QTrail().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TrailTask) TrailHeld((uint8_t *)t, num, den);
		for (TaskNode *t = QFall().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_FallPrimTask) TargetPrimHeld((uint8_t *)t, num, den);
		for (TaskNode *t = QLand().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_LandPrimTask) TargetPrimHeld((uint8_t *)t, num, den);
		g_held_nd = nullptr;
		memcpy(MB() + 0x31D4, g_scratch_save, sizeof(g_scratch_save));
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}
}

	// ------------------------------------------------------------------
	// Held-frame camera: the timeline's camera code for the next tick, run on the real globals
	// with the targets moved to their next positions (the look-at follows target 0 from 376
	// on), then everything is put back (camera, shake, roll, node, entities, GTE registers,
	// Field_Alloc pointer); cuts (absolute set-ups, jumps > 1500) hold.
	// ------------------------------------------------------------------
	bool mag291_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		using namespace p291;
		uint8_t *nd = Creature();
		if (!nd || !HeldReady() || g_cm.tick != g_real_tick || g_cm.nd != nd) return false;
		int32_t c1 = At<int16_t>(nd, 0xC);
		int16_t e0[3], a0[3], e1[3], a1[3];
		for (int i = 0; i < 3; i++) { e0[i] = CamW(i); a0[i] = CamL(i); }
		bool cut = true;
		if (c1 == g_cm.c + 1)
		{
			static uint8_t cam_save[0x20], node_save[0x1030], ent_save[ENT_SIZE], gte_d[0x70], gte_c[0xD0];
			int16_t shake = ShakeY(), roll = Roll(), w38 = W1D8E038();
			uint32_t alloc = var<uint32_t>(0x1D999C4);
			memcpy(cam_save, (void *)0xB8B7F0, sizeof(cam_save));
			memcpy(node_save, nd, sizeof(node_save));
			memcpy(ent_save, (void *)ENT_BASE, ENT_SIZE);
			memcpy(gte_d, (void *)0x1CA8A10, sizeof(gte_d));
			memcpy(gte_c, (void *)0x1CA9230, sizeof(gte_c));
			int16_t hand[3];
			bool hand_ok = NextHand(nd, hand);
			PredictNext(nd, c1, hand_ok ? hand : nullptr, false);
			cut = CamStep(nd, c1, false);
			for (int i = 0; i < 3; i++) { e1[i] = CamW(i); a1[i] = CamL(i); }
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			memcpy(nd, node_save, sizeof(node_save));
			memcpy((void *)ENT_BASE, ent_save, ENT_SIZE);
			memcpy((void *)0x1CA8A10, gte_d, sizeof(gte_d));
			memcpy((void *)0x1CA9230, gte_c, sizeof(gte_c));
			var<uint32_t>(0x1D999C4) = alloc;
			ShakeY() = shake;
			Roll() = roll;
			W1D8E038() = w38;
			for (int i = 0; i < 3; i++) cut |= Jump(e0[i], e1[i]) || Jump(a0[i], a1[i]);
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = cut ? e0[i] : (int16_t)lerp_i(e0[i], e1[i], num, den);
			lookat[i] = cut ? a0[i] : (int16_t)lerp_i(a0[i], a1[i], num, den);
		}
		return true;
	}

	static void register_mag291_held()
	{
		mark_held(p291::ORIG_TimelineTask);
		mark_held(p291::ORIG_LandPrimTask);
		mark_held(p291::ORIG_FallPrimTask);
		mark_held(p291::ORIG_SwirlFlipTask);
		mark_held(p291::ORIG_RiseFlipTask);
		mark_held(p291::ORIG_TrailTask);
		register_module_held(291, p291::HeldReady, p291::HeldFrame);
		register_module_camera(291, mag291_held_camera);
	}
}
