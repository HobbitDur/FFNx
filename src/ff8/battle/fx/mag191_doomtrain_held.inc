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
	// GTE projection distance H (control register 26) as the tick's tasks start: the sky resets
	// it to the camera's (0x1D8E038) after its draw, so the tasks queued before the sky draw
	// with the H the previous tick left (on the sky's first tick it changes for them); held
	// frames start from the tick's start H blended toward the one the next tick starts with
	static int32_t g_h_start;
	static int32_t &GteH() { return var<int32_t>(0x1CA927C + 4 * 26); }
	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_h_start = GteH();
	}

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
	struct GlowMemo { int32_t fade; int16_t c; };
	static NodeMemo<GlowMemo, 64> g_glow_memo;
	static void held_note_glow(const Node24 *n, int32_t fade) { if (GlowMemo *m = g_glow_memo.put(n)) { m->fade = fade; m->c = n->c; } }

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

	// bones that turn by a few units only (the train's bounce: +-1..5 per tick, which the int16
	// pose values cannot split): their local matrices (built from the two ticks' poses) blended
	// element-wise instead of rebuilt from the rounded half pose; bigger turns (wheels) keep the
	// matrix of the blended angles
	static void BoneBlend(uint8_t *sk, const uint8_t *s0, const uint8_t *s1, int num, int den)
	{
		for (uint32_t i = 0; i < sk[0]; i++)
		{
			uint32_t o = 0x10 + 48 * i;
			const int16_t *a0 = (const int16_t *)(s0 + o + 4), *a1 = (const int16_t *)(s1 + o + 4);
			bool small = true;
			for (int k = 0; k < 3 && small; k++)
			{
				int d = ((a1[k] - a0[k] + 2048) & 4095) - 2048;
				if (d > 64 || d < -64) small = false;
			}
			if (!small) continue;
			Mat4x3 *m = (Mat4x3 *)(sk + o + 0x10);
			const Mat4x3 *m0 = (const Mat4x3 *)(s0 + o + 0x10), *m1 = (const Mat4x3 *)(s1 + o + 0x10);
			for (int r = 0; r < 3; r++)
			{
				for (int c = 0; c < 3; c++) m->m[r][c] = L16(m0->m[r][c], m1->m[r][c], num, den);
				m->t[r] = lerp_i(m0->t[r], m1->t[r], num, den);
			}
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
		bool blended = false;
		if (!P1() && CreatureStep(n->c, &dz))
		{
			uint8_t cmd[8], hdr[0xC];
			memcpy(cmd, m.cmd, 8);
			memcpy(hdr, E + 0x60, sizeof(hdr));
			// AdvanceLoop on copies: the next frame; a looping animation that finished restarts
			if (ReaderRead(E + 0x60, cmd) == 1 && !(E[0] & 1)) ReaderSetAnim(E + 0x60, cmd, (int32_t)cmd[0]);
			memcpy(E + 0x60, hdr, sizeof(hdr));
			{
				ReaderBuildBones(E + 0x60);
				memcpy(g_skel_next, sk, size);
				memcpy(sk, m.skel, size);
				PoseBlend(sk, g_skel_next, num, den);
				ReaderBuildBones(E + 0x60);
				BoneBlend(sk, m.skel, g_skel_next, num, den);
				blended = true;
				model.t[2] = lerp_i(m.model.t[2], m.model.t[2] + dz, num, den);
			}
		}
		CreatureMat() = model;
		DrawModel(E, frame);
		// DrawModel rebuilt the local matrices from the (rounded) pose values: the chimney bone
		// is taken from the blended one
		if (blended) BoneBlend(sk, m.skel, g_skel_next, num, den);
		ComposeAffineTransform(&model, (const Mat4x3 *)(sk + 0x50), &g_held_bone);
		CreatureMat() = model_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, size);
	}

	// GPU polygon packets (GP0 0x20..0x3F) written by one draw at a (up to ae) and the same
	// draw with the next tick's flipbook frame at b (up to be): each polygon of a is paired with
	// the nearest polygon of b with the same command and length (nearest pairs first: a
	// flipbook frame may list its quads in another order, or keep them in place and cycle their
	// texture windows); with the same texture words its vertex colours and positions are moved
	// to the in-between, else (a discrete frame step) and without a partner it keeps a's picture
	struct PolyRef { uint32_t *w; uint32_t len, nv; uint8_t code; bool gouraud, tex; };
	static int PolyList(uint8_t *p, const uint8_t *e, PolyRef *out, int max)
	{
		int n = 0;
		while (p + 8 <= e && n < max)
		{
			uint32_t len = p[3];
			if (!len) break;
			uint8_t code = p[7];
			if ((code & 0xE0) == 0x20)
			{
				PolyRef r = { (uint32_t *)(p + 4), len, (code & 8) ? 4u : 3u, code, (code & 0x10) != 0, (code & 4) != 0 };
				if (len == 1 + r.nv * (r.tex ? 2 : 1) + (r.gouraud ? r.nv - 1 : 0)) out[n++] = r;
			}
			p += 4 + 4 * len;
		}
		return n;
	}
	// word index of vertex v's position / colour (-1: flat, no own colour)
	static uint32_t PosIdx(const PolyRef &r, uint32_t v) { return 1 + v * (r.tex ? 2 : 1) + (r.gouraud ? v : 0); }
	static int ColIdx(const PolyRef &r, uint32_t v) { return v == 0 ? 0 : (r.gouraud ? (int)PosIdx(r, v) - 1 : -1); }
	static bool SameKind(const PolyRef &a, const PolyRef &b) { return a.code == b.code && a.len == b.len; }
	static bool SameTexture(const PolyRef &a, const PolyRef &b)
	{
		if (a.tex)
			for (uint32_t v = 0; v < a.nv; v++)
			{
				// uv (+ clut on vertex 0, texpage on vertex 1; the high half of the others is
				// not read by the GPU)
				uint32_t i = PosIdx(a, v) + 1, mask = v < 2 ? 0xFFFFFFFFu : 0xFFFFu;
				if ((a.w[i] ^ b.w[i]) & mask) return false;
			}
		return true;
	}
	static void LerpPolyPackets(uint8_t *a, uint8_t *ae, const uint8_t *b, const uint8_t *be, int num, int den)
	{
		static PolyRef pa[32], pb[32];
		int na = PolyList(a, ae, pa, 32), nb = PolyList((uint8_t *)b, be, pb, 32);
		static uint64_t cand[32 * 32];
		int nc = 0;
		for (int i = 0; i < na; i++)
			for (int j = 0; j < nb; j++)
			{
				if (!SameKind(pa[i], pb[j])) continue;
				uint32_t d = 0;
				for (uint32_t v = 0; v < pa[i].nv; v++)
				{
					const int16_t *x = (const int16_t *)&pa[i].w[PosIdx(pa[i], v)], *y = (const int16_t *)&pb[j].w[PosIdx(pb[j], v)];
					int dx = x[0] - y[0], dy = x[1] - y[1];
					d += (uint32_t)(dx < 0 ? -dx : dx) + (uint32_t)(dy < 0 ? -dy : dy);
				}
				cand[nc++] = ((uint64_t)d << 16) | (uint64_t)(i << 8) | (uint64_t)j;
			}
		for (int k = 1; k < nc; k++) // insertion sort (a few polygons per sprite)
		{
			uint64_t c = cand[k];
			int q = k - 1;
			for (; q >= 0 && cand[q] > c; q--) cand[q + 1] = cand[q];
			cand[q + 1] = c;
		}
		uint64_t useda = 0, usedb = 0;
		for (int k = 0; k < nc; k++)
		{
			int i = (int)((cand[k] >> 8) & 0xFF), j = (int)(cand[k] & 0xFF);
			if ((useda >> i & 1) || (usedb >> j & 1)) continue;
			useda |= 1ull << i;
			usedb |= 1ull << j;
			const PolyRef &x = pa[i], &y = pb[j];
			if (!SameTexture(x, y)) continue;
			for (uint32_t v = 0; v < x.nv; v++)
			{
				int ci = ColIdx(x, v);
				if (ci >= 0)
				{
					uint8_t *ca = (uint8_t *)&x.w[ci];
					const uint8_t *cb = (const uint8_t *)&y.w[ci];
					for (int c = 0; c < 3; c++) ca[c] = (uint8_t)lerp_i(ca[c], cb[c], num, den);
				}
				int16_t *p0 = (int16_t *)&x.w[PosIdx(x, v)];
				const int16_t *p1 = (const int16_t *)&y.w[PosIdx(y, v)];
				p0[0] = (int16_t)lerp_i(p0[0], p1[0], num, den);
				p0[1] = (int16_t)lerp_i(p0[1], p1[1], num, den);
			}
		}
	}

	// OT() (g_Battle_FrameRenderListBase + 0x44) pointed at a private bucket while alive
	static uint32_t g_ot_next[64];
	struct OTSwap
	{
		uint32_t saved;
		OTSwap() : saved(var<uint32_t>(0x1D8E04C)) { var<uint32_t>(0x1D8E04C) = (uint32_t)&g_ot_next[32] - 0x44; }
		~OTSwap() { var<uint32_t>(0x1D8E04C) = saved; }
	};
	static uint8_t g_seq_next[0x1000];

	// a flipbook sprite drawn with the frame of the tick (draw(frame) into the frame's packets),
	// then, when the record moves on to next_frame (>= 0), drawn again with that frame into a
	// private bucket / buffer (header, OT node pool and cursor put back) and its packets blended
	template<class D> static void SeqLerp(uint8_t *h, int16_t frame, int32_t next_frame, int num, int den, D draw)
	{
		uint32_t c0 = Cursor();
		draw(frame);
		uint32_t c1 = Cursor();
		if (!num || next_frame < 0) return;
		static uint8_t hs[0xB4];
		if (h) memcpy(hs, h, sizeof(hs));
		uint32_t exec = var<uint32_t>(0x1CA8828); // ssigpu_execution_cur: the node this draw adds is dropped
		uint32_t n1;
		{
			memset(g_ot_next, 0, sizeof(g_ot_next));
			OTSwap ot;
			Cursor() = (uint32_t)g_seq_next;
			draw((int16_t)next_frame); // hf191 private draw
			n1 = Cursor();
			Cursor() = c1;
		}
		var<uint32_t>(0x1CA8828) = exec;
		if (h) memcpy(h, hs, sizeof(hs));
		if (n1 - (uint32_t)g_seq_next <= sizeof(g_seq_next))
			LerpPolyPackets((uint8_t *)c0, (uint8_t *)c1, g_seq_next, (const uint8_t *)n1, num, den);
	}

	// lit records of an owner as drawn, moved half way when they moved on (flipbook frame blended)
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
				bool adv = Advanced(m->r, cur);
				if (lerp && adv)
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				SeqLerp(h, m->r.age, adv ? cur->age : -1, num, den, [&](int16_t f) { LitSprite(h, pos, size, f, ir); });
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
				bool adv = Advanced(m->r, cur);
				if (lerp && adv)
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				int16_t f0 = age2 ? (int16_t)(m->r.age << 1) : m->r.age;
				int32_t f1 = adv ? (age2 ? (int16_t)(cur->age << 1) : cur->age) : -1;
				SeqLerp(h, f0, f1, num, den, [&](int16_t f) { OffsetSprite(h, mac, nv, pos, size, f, shift); });
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

	static void WheelTrailHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
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
				const Rec *ca = R(POOL_A, i), *cb = R(POOL_B, i);
				int32_t next = Advanced(a->r, ca) && Advanced(b->r, cb) ? ca->age : -1;
				int16_t db = (int16_t)(b->r.age - a->r.age);
				SeqLerp(h, a->r.age, next, num, den, [&](int16_t f) { WheelTrailPair(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), &a->r, &b->r, f, (int16_t)(f + db)); });
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
			// fade 0x800 on tick 0, none on tick 1 (= fade 0), then -682 per tick: blended as
			// one fade value (the fade form with 0 draws as the plain form)
			if (!mv) { RigFlashDraw(W, m, m->c, true, RigFlashFade(m->c)); break; }
			auto fd = [](int32_t c) { return c == 0 ? 0x800 : (c == 1 ? 0 : RigFlashFade(c)); };
			RigFlashDraw(W, m, 2, true, lerp_i(fd(m->c), fd(n->c), num, den));
			break;
		}
		case ORIG_RigSpriteTask: SeqLerp(nullptr, m->c, mv ? n->c : -1, num, den, [&](int16_t f) { RigSpriteDraw(W, m, f); }); break;
		}
	}

	// the sky's colour as SkyDraw computes it: table colour (+0x1E) depth-cued by the phase (+0x20)
	static uint32_t SkyColor(int16_t idx, int16_t phase)
	{
		uint32_t c = ((const uint32_t *)0xE3C8DC)[idx & 0x1F], out;
		GteSetFarColor(0, 0, 0);
		GteLoadRGBC(&c);
		GteSetIR0((ComputeSin(phase) + 0x1000) / 2);
		GteDPCS();
		GteStoreRGB2(&out);
		return out;
	}

	// SkyDraw of the in-between state: the rotation matrix, the morph weight and the colour are
	// blended between the two ticks' own values (the sphere wraps the eye, so its huge vertices
	// turn the 1-LSB rounding of a matrix built from rounded half angles into pixels of jitter;
	// the colour table index +0x1E steps every tick: its colours are blended, not its index)
	static void SkyHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		if (!mv) { SkyDraw(m->f18, m->f10, m->f14, m->f1A, m->f1C, m->f1E, m->f20, SkyFade(m->c)); return; }
		int16_t a0[3] = { m->f18, m->f10, m->f14 }, a1[3] = { n->f18, n->f10, n->f14 };
		Mat4x3 r0, r1, mm;
		RotMatrixXYZ(a0, &r0);
		RotMatrixXYZ(a1, &r1);
		mm = r0;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) mm.m[i][j] = L16(r0.m[i][j], r1.m[i][j], num, den);
		int32_t v[3] = { m->f1C, m->f1C, m->f1C };
		mm.t[0] = 0;
		mm.t[1] = 0;
		mm.t[2] = 0x120;
		Scale3DMatrix(&mm, v);
		uint32_t c0 = SkyColor(m->f1E, m->f20), c1 = SkyColor(n->f1E, n->f20);
		GteSetRotMatrix(&mm);
		GteSetTransVector(&mm);
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		*(uint32_t *)h = 0xE3496C;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = (m->f1A ? 3 : 0xC) | 0x20C0;
		*(uint32_t *)(h + 0x20) = LerpColor(c0, c1, num, den);
		*(int32_t *)(h + 0xC) = lerp_i(SkyFade(m->c), SkyFade(n->c), num, den);
		int32_t t = lerp_i((ComputeSin((int32_t)m->f20 >> 2) + 0x1000) / 2, (ComputeSin((int32_t)n->f20 >> 2) + 0x1000) / 2, num, den);
		int32_t count = var<int32_t>(0xE34970);
		int16_t *out = (int16_t *)(MB() + 0x37000);
		const int16_t *a = (const int16_t *)0xE34974, *b = (const int16_t *)0xE3702C;
		for (int32_t i = 0; i < count; i++, a += 4, b += 4, out += 4)
		{
			out[0] = (int16_t)((mul32(b[0] - a[0], t) >> 12) + a[0]);
			out[1] = (int16_t)((mul32(b[1] - a[1], t) >> 12) + a[1]);
			out[2] = (int16_t)((mul32(b[2] - a[2], t) >> 12) + a[2]);
		}
		*(uint32_t *)(h + 4) = MB() + 0x37000;
		ProjectionH(0x120);
		Cursor() = SkyRender(h, var<uint32_t>(0x1D8E04C) + 0x4078, 6, Cursor());
		ProjectionH(CamH());
		FieldFree(0x6C);
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
			SeqLerp(h, m->c, NodeMoved(m, n) && n->c < 5 ? n->c : -1, num, den, [&](int16_t f)
			{
				TransformCameraByShadowRotation(&m->f10, sz, -((int32_t)sz >> 3));
				*(int16_t *)(h + 4) = f;
				Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
			});
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
				int16_t vel[3] = { r->r.vel[0], r->r.vel[1], r->r.vel[2] };
				bool adv = Advanced(r->r, cur);
				if (adv)
					for (int j = 0; j < 3; j++)
					{
						pos[j] = L16(r->r.pos[j], cur->pos[j], num, den);
						vel[j] = L16(r->r.vel[j], cur->vel[j], num, den); // orientation (the halving rounds per axis)
					}
				SeqLerp(h, r->r.age, adv ? cur->age : -1, num, den, [&](int16_t f) { ShardDraw(h, s, pos, r->r.size, vel, f); });
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
		int32_t h_now = GteH();
		if (h_now != g_h_start) ProjectionH(lerp_i(g_h_start, h_now, num, den));
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
			case ORIG_WheelTrailTask: WheelTrailHeld(&frame, n, num, den); break;
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
				if (const GlowMemo *g = g_glow_memo.get(n))
				{
					// the flicker's phase / speed are already stepped: GlowFade() is the fade the
					// next tick draws
					int32_t fd = g->fade;
					if (n->c == (int16_t)(g->c + 1) && n->c < n->e)
					{
						int32_t f1 = GlowFade();
						fd = lerp_i(fd, f1 < 0 ? 0 : f1, num, den);
					}
					GlowDraw(&frame, &g_held_bone, n, fd);
				}
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
		if (GteH() != h_now) ProjectionH(h_now);
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
