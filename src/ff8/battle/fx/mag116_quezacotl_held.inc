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

// 30 fps part of mag116_quezacotl.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag116_quezacotl.cpp)

namespace ff8fx
{
namespace q116
{
	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	static void held_note_master() { g_ported_tick = g_real_tick; }

	struct DebrisMemo { int16_t pos[3], rot[3]; };
	static NodeMemo<DebrisMemo, 256> g_debris_memo;

	// debris: the chunk as drawn, before the tick's update
	static void held_note_debris(DebrisNode *p)
	{
		if (DebrisMemo *m = g_debris_memo.put(p))
		{
			memcpy(m->pos, p->pos, sizeof(m->pos));
			memcpy(m->rot, p->rot, sizeof(m->rot));
		}
	}

	// jitter of the 14 inner points: the random values of the real tick are kept so a held
	// frame redraws the same arc shape without touching the module generator
	struct ArcMemo { int16_t progress; int16_t color[16]; int32_t rnd[14][3]; };
	static NodeMemo<ArcMemo, 256> g_arc_memo;
	static ArcMemo g_arc_local, *g_arc_cur = &g_arc_local;

	// arc: progress and colours it is drawn with (the memo the jitter values of this draw go to)
	static void held_note_arc(ArcNode *arc)
	{
		ArcMemo *m = g_arc_memo.put(arc);
		if (!m) m = &g_arc_local;
		m->progress = arc->progress;
		for (int k = 0; k < 16; k++) m->color[k] = arc->pts[k].color;
		g_arc_cur = m;
	}

	// the three random values of inner point i (1..14) of the arc being drawn
	static void held_note_arc_jitter(int i, const int32_t *rv)
	{
		memcpy(g_arc_cur->rnd[i - 1], rv, sizeof(g_arc_cur->rnd[i - 1]));
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn half way
	// between the state drawn on the last real tick and the next one; what the vanilla code
	// creates at random steps (bolt growth, forks, flashes) keeps the 15 Hz batches.
	// ------------------------------------------------------------------
	static void DebrisHeld(DebrisNode *p, int num, int den)
	{
		const DebrisMemo *m = g_debris_memo.get(p);
		if (!m) return;
		int16_t pos[3], rot[3];
		for (int k = 0; k < 3; k++)
		{
			pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
			rot[k] = lerp_angle(m->rot[k], p->rot[k], num, den);
		}
		DebrisDraw(pos, rot);
	}

	// Bolt vertices: drawn at P, then drifted by vel at the end of the tick; the next draw
	// applies one fade step first. Drawn on copies (the real vertices keep their state).
	static BoltVert g_held_verts[256];

	static void BoltHeld(BoltNode *b, int num, int den)
	{
		if (b->count < 2) return;
		int k = 0;
		for (BoltVert *v = b->head; v && k < 256; v = v->next, k++)
		{
			BoltVert &h = g_held_verts[k];
			h = *v;
			h.next = nullptr;
			if (k) g_held_verts[k - 1].next = &h;
			for (int c = 0; c < 3; c++)
				h.pos[c] = (int16_t)(v->pos[c] - v->vel[c] + v->vel[c] * num / den);
			int32_t i0 = v->intensity, i1 = i0;
			if (i0 > 0)
			{
				i1 = (int16_t)(i0 + v->decay);
				if (i1 <= 0) i1 = 0;
			}
			h.intensity = (int16_t)lerp_i(i0, i1, num, den);
			GteLoadV0(h.pos);
			GteRTPS();
			GteReadSXY2(&h.sx);
			GteReadOTZ(&h.otz);
		}
		int count = b->count < k ? b->count : k;
		static int16_t held_dir[2];
		held_dir[0] = g_dir_bolt[0];
		held_dir[1] = g_dir_bolt[1];
		if (count >= 2) DrawRibbon<BoltVert, 0x200>(g_held_verts, count, held_dir);
	}

	// Branch: the path grows in 15 Hz batches (vanilla shapes); held frames draw the current
	// chain with each segment's brightness half way to the next fade step.
	static BoltSeg g_held_segs[512];

	static void BranchHeld(BranchNode *b, int num, int den)
	{
		if (b->count < 2) return;
		int k = 0;
		for (BoltSeg *s = b->head; s && k < 512; s = s->next, k++)
		{
			BoltSeg &h = g_held_segs[k];
			h = *s;
			h.next = nullptr;
			if (k) g_held_segs[k - 1].next = &h;
			int32_t i0 = s->intensity, i1 = i0;
			if (i0 > 0)
			{
				i1 = (int16_t)(i0 + s->decay);
				if (i1 <= 0) i1 = 0;
			}
			h.intensity = (int16_t)lerp_i(i0, i1, num, den);
			GteLoadV0(BOLT_VERTICES + 8 * (int32_t)h.vertex);
			GteRTPS();
			GteReadSXY2(&h.sx);
			GteReadOTZ(&h.otz);
		}
		static int16_t held_dir[2];
		held_dir[0] = g_dir_branch[0];
		held_dir[1] = g_dir_branch[1];
		int count = b->count < k ? b->count : k;
		if (count >= 2) DrawRibbon<BoltSeg, 0x400>(g_held_segs, count, held_dir);
	}

	// impact flash: a flipbook, redrawn on its current frame with the held-frame camera
	static void FlashHeld(FlashNode *f, int, int)
	{
		if (f->age <= 0) return;
		TransformCameraByShadowRotation(f->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(void **)h = SEQ_ImpactFlash;
		*(int16_t *)(h + 4) = (int16_t)(f->age - 1);
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void ArcHeld(ArcNode *arc, int num, int den)
	{
		const ArcMemo *m = g_arc_memo.get(arc);
		if (!m) return;
		int32_t length;
		ArcSeg *pts = ArcCurve(arc, (int16_t)lerp_i(m->progress, arc->progress, num, den), &length);
		for (int k = 0; k < 16; k++) pts[k].color = (int16_t)lerp_i(m->color[k], arc->pts[k].color, num, den);
		// the jitter of the real tick, replayed (the module generator is not touched)
		const int16_t *profile = (const int16_t *)(ModelBuffer() + 0x3C25C);
		for (int i = 1; i < 15; i++)
		{
			int32_t amp = mul32(profile[i], length) >> 12;
			const int32_t *rv = m->rnd[i - 1];
			pts[i].sx += (int16_t)(mul32(rv[0], amp) >> 18);
			pts[i].sy += (int16_t)(mul32(rv[1], amp) >> 18);
			pts[i].otz += (int16_t)(mul32(rv[2], amp) >> 18);
		}
		int16_t held_dir[2] = { g_dir_arc[0], g_dir_arc[1] };
		ArcDraw(pts, held_dir);
		FieldFree(0x100);
	}

	static uint8_t g_held_packets[0x60000];

	// ---- held-frame memo of what the real tick drew ----
	struct CreaturePlay { prim::Layout *l; uint32_t cb; bool compose; uint8_t arg[0x2C]; };
	struct CreatureMemo
	{
		uint32_t tick;
		bool model;  int32_t v3, v4, acc; // materialisation draw (0x6C6770)
		bool orbit;                        // plain model draw (0x6A79B0)
		bool pose_ok; uint32_t pose_size;  // skeleton at the start of the tick: the pose drawn this tick
		uint8_t pose[16 + 48 * 64];
		int nplays; CreaturePlay plays[4];
	};
	static CreatureMemo g_cm;

	static uint8_t *CreatureSkeleton(uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(ModelBuffer() + 0x88 + 4); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0 || sk[0] > 64) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return sk;
	}

	// creature: the skeleton as the tick starts is the pose this tick draws
	static void held_creature_begin()
	{
		g_cm.tick = g_real_tick;
		g_cm.model = g_cm.orbit = false;
		g_cm.nplays = 0;
		{
			uint32_t size = 0;
			uint8_t *sk = CreatureSkeleton(&size);
			g_cm.pose_ok = sk && size <= sizeof(g_cm.pose);
			if (g_cm.pose_ok) { memcpy(g_cm.pose, sk, size); g_cm.pose_size = size; }
		}
	}

	// materialisation draw (0x6C6770) of this tick: its opacity step and accumulator
	static void held_note_materialise(int32_t v3, int32_t v4, int32_t acc)
	{
		g_cm.model = true; g_cm.v3 = v3; g_cm.v4 = v4; g_cm.acc = acc;
	}

	static void held_note_orbit() { g_cm.orbit = true; }

	// the matrix of the next play before it is composed with the camera: the held frame
	// composes it with the held-frame camera
	static uint8_t g_uncomposed[0x2C];
	static bool g_uncomposed_next = false;

	static void held_note_uncomposed(const uint8_t *mat)
	{
		memcpy(g_uncomposed, mat, sizeof(g_uncomposed));
		g_uncomposed_next = true;
	}

	// every play of the tick (up to 4): layout, callback and its argument (0x14 bytes for
	// CB_PrimDraw, a 0x2C-byte matrix for CB_PrimMatrix; the uncomposed one when noted)
	static void held_note_play(prim::Layout *l, uint32_t cb, void *arg)
	{
		bool compose = g_uncomposed_next;
		g_uncomposed_next = false;
		uint32_t arg_size = cb == CB_PrimMatrix ? 0x2C : 0x14;
		if (g_cm.nplays < 4)
		{
			CreaturePlay &p = g_cm.plays[g_cm.nplays++];
			p.l = l;
			p.cb = cb;
			p.compose = compose;
			memcpy(p.arg, arg, arg_size);
		}
		if (compose && g_cm.nplays) memcpy(g_cm.plays[g_cm.nplays - 1].arg, g_uncomposed, sizeof(g_uncomposed));
	}

	// ---- held frame of the creature: what the real tick drew, in between ----
	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose after the tick (the one the next tick draws), from the pose drawn
		// this tick: write the midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *p = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*p = (int16_t)(f + ((int32_t)*p - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *p = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048;
				p[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	static void CreatureHeld(int num, int den)
	{
		if (g_cm.tick != g_real_tick || !QueueCreature().second.head) return;
		uint8_t *nd = ModelBuffer();
		uint8_t *model = nd + 0x28;
		Mat4x3 bolt_frame = BoltFrame(), effect_camera = EffectCamera();
		EffectCameraMatrix(&Camera(), &EffectCamera());
		uint32_t size = 0;
		uint8_t *sk = (g_cm.model || g_cm.orbit) ? CreatureSkeleton(&size) : nullptr;
		static uint8_t sk_save[16 + 48 * 64];
		if (sk)
		{
			memcpy(sk_save, sk, size);
			// the pose drawn this tick is the one the tick started with (the draw uses the
			// matrices built before the anim read); no memo (tick 86 binds the model): as is
			if (g_cm.pose_ok && size == g_cm.pose_size)
			{
				PoseBlend(sk, g_cm.pose, num, den);
				cx::BuildBoneMatrices(nd + 0x88);
			}
			if (g_cm.model)
			{
				// next tick draws with the accumulator + v3 and the next opacity step
				int32_t v3n = g_cm.v3 + 1;
				bool next = !Pause() && v3n < 0xAB;
				int32_t v4n = v3n < 0x32 ? (int32_t)(-3500 * v3n) / 50 : -4096;
				int32_t v4 = next ? lerp_i(g_cm.v4, v4n, num, den) : g_cm.v4;
				int32_t acc = next ? lerp_i(g_cm.acc, g_cm.acc + g_cm.v3, num, den) : g_cm.acc;
				static const int16_t a3[4] = { 0x1C0, 0x180, 0x80, 0x80 };
				static const uint8_t a7[4] = { 0x80, 0x80, 0x80, 0 };
				cx::DrawMaterialise(model, v4, a3, 0x3F54, acc >> 6, acc >> 5, a7);
			}
			if (g_cm.orbit)
				var<uint32_t>(0x1D8E054) = cx::DrawModel(model, nd + 0x8D0, var<uint32_t>(0x1D8E054), var<uint32_t>(0x1D969A8));
			memcpy(sk, sk_save, size);
		}
		for (int i = 0; i < g_cm.nplays; i++)
		{
			CreaturePlay &p = g_cm.plays[i];
			static uint8_t arg[0x2C];
			memcpy(arg, p.arg, sizeof(arg));
			if (p.compose) ComposeAffineTransform(&Camera(), (Mat4x3 *)arg, (Mat4x3 *)arg);
			prim::play_held(p.l, (prim::Callback)p.cb, (int)arg, num, den);
		}
		BoltFrame() = bolt_frame;
		EffectCamera() = effect_camera;
	}

	// ---- held-frame camera: the creature's camera code for the next tick, run on the real
	// globals and put back (all it calls is pure math), then the in-between camera ----
	static bool CreatureCameraNext(int16_t w[3], int16_t l[3])
	{
		uint8_t *nd = ModelBuffer();
		auto W16 = [nd](int o) -> int16_t & { return *(int16_t *)(nd + o); };
		auto D32 = [nd](int o) -> int32_t & { return *(int32_t *)(nd + o); };
		int16_t c = W16(0x0C);
		bool cut = false;
		static uint8_t cam_save[0x10], node_save[0x10], fr_save[sizeof(g_fr)];
		memcpy(cam_save, (void *)CAM_EYE, 0x10);
		memcpy(node_save, nd + 0x18, 0x10);
		memcpy(fr_save, g_fr, sizeof(g_fr));
		bool paused = Pause() != 0;
		uint32_t v5 = (uint32_t)((int32_t)c - 257);
		if (v5 < 0x14 && !paused)
		{
			F16(0x18) = G16(CAM_EYE);
			F16(0x1A) = G16(CAM_EYE + 2);
			F16(0x1C) = (int16_t)(G32(CAM_EYE + 4) - G32(BASE_Y));
			int32_t s = ComputeSin((int32_t)((v5 << 12) / 40));
			cx::RotMatrixY(s >> 7, FP(0x44));
			cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_EYE);
			G16(CAM_EYE + 4) += G16(BASE_Y);
		}
		uint32_t v7 = (uint32_t)((int32_t)c - 86);
		if (v7 < 0x4C && v7 != 0 && !paused)
		{
			int32_t v10 = (int32_t)((v7 << 12) / 76);
			W16(0x1A) -= 17;
			W16(0x22) -= 5;
			W16(0x24) -= G16(BASE_Y3);
			int32_t s = ComputeSin(v10 >> 1);
			cx::RotMatrixY(s / 90, FP(0x44));
			cx::MatVec(FP(0x44), nd + 0x20, nd + 0x20);
			W16(0x24) += G16(BASE_Y3);
			if (v7 == 31) cut = true;
			else if (v7 > 31 && v7 <= 40)
			{
				F16(0x18) = G16(CAM_EYE);
				F16(0x1A) = G16(CAM_EYE + 2);
				F16(0x1C) = (int16_t)(G32(CAM_EYE + 4) - G32(BASE_Y3));
				cx::RotMatrixY(0x40, FP(0x44));
				cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_EYE);
				G16(CAM_EYE + 4) += (int16_t)G32(BASE_Y3);
				F16(0x18) = G16(CAM_AT);
				F16(0x1A) = G16(CAM_AT + 2);
				F16(0x1C) = (int16_t)(G32(CAM_AT + 4) - G32(BASE_Y3));
				cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_AT);
				G16(CAM_AT + 4) += G16(BASE_Y3);
			}
			else if (v10 < 0x800)
			{
				G32(CAM_AT) = D32(0x18);
				G32(CAM_AT + 4) = D32(0x1C);
				G32(CAM_EYE) = D32(0x20);
				G32(CAM_EYE + 4) = D32(0x24);
			}
			else
			{
				int32_t v12 = 2 * v10 - 0x1000;
				F16(0x2C) = (int16_t)(G32(BASE_Y3) - 0xA14);
				F16(0x20) = (int16_t)0xFD69; F16(0x22) = (int16_t)0xF544; F16(0x24) = (int16_t)(G32(BASE_Y3) + 0x6F7);
				F16(0x28) = 0x547; F16(0x2A) = (int16_t)0xF9F4;
				cx::Blend(nd + 0x18, FP(0x20), 0x1000 - v12, v12, (void *)CAM_AT);
				cx::Blend(nd + 0x20, FP(0x28), 0x1000 - v12, v12, (void *)CAM_EYE);
			}
		}
		if ((uint32_t)((int32_t)c - 207) < 0x14)
		{
			// the look-at follows a vertex of the posed creature: kept as drawn (it would need
			// the next pose); the eye cut at 207 is a cut
			if (c == 207) cut = true;
		}
		{
			uint32_t t = (uint32_t)((int32_t)c - 257);
			if (t < 0x2E && t >= 0x14 && !paused)
			{
				int16_t d = t < 0x26 ? -64 : 0x180;
				G16(CAM_EYE + 2) += d;
				G16(CAM_AT + 2) += d;
			}
		}
		{
			uint32_t t = (uint32_t)((int32_t)c - 319);
			if (t < 0x24 && !paused)
			{
				int16_t d = (int16_t)(0x80 - (t << 7) / 36);
				G16(CAM_EYE) += d;
				G16(CAM_EYE + 2) -= d;
				G16(CAM_EYE + 4) -= d;
			}
		}
		switch ((uint16_t)c)
		{
		case 0: case 0x4C: case 0xF6: case 0xE2: case 0x100: case 0x114: case 0x12E: case 0x13E: cut = true; break;
		default: break;
		}
		const int16_t *e = (const int16_t *)CAM_EYE, *a = (const int16_t *)CAM_AT;
		const int16_t *e0 = (const int16_t *)cam_save, *a0 = (const int16_t *)(cam_save + 8);
		for (int i = 0; i < 3; i++)
		{
			// a jump the timeline makes on its own (e.g. back to the saved camera at 127) is a cut too
			if (abs((int32_t)e[i] - e0[i]) > 1500 || abs((int32_t)a[i] - a0[i]) > 1500) cut = true;
			w[i] = e[i];
			l[i] = a[i];
		}
		memcpy((void *)CAM_EYE, cam_save, 0x10);
		memcpy(nd + 0x18, node_save, 0x10);
		memcpy(g_fr, fr_save, sizeof(g_fr));
		return !cut;
	}

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cm.tick != g_real_tick || g_ported_tick != g_real_tick || !QueueCreature().second.head) return false;
		int16_t w[3], l[3];
		const int16_t *e0 = (const int16_t *)CAM_EYE, *a0 = (const int16_t *)CAM_AT;
		bool move = CreatureCameraNext(w, l);
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(e0[i], w[i], num, den) : e0[i];
			lookat[i] = move ? (int16_t)lerp_i(a0[i], l[i], num, den) : a0[i];
		}
		return true;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master's queue order and GTE setup; packets go to a private buffer so the
	// real tick's arenas are untouched
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = PacketCursor(), frame_cursor = var<uint32_t>(0x1D8E054);
		PacketCursor() = (uint32_t)g_held_packets;
		var<uint32_t>(0x1D8E054) = (uint32_t)g_held_packets + 0x40000;
		CreatureHeld(num, den);
		if (QueueCreature().first.head)
		{
			// arcs read the bones in the composed (world) form the master gives them during the
			// tick: compose, draw, then put the skeleton bytes back exactly
			uint8_t *com = *(uint8_t **)(ModelBuffer() + 0x88 + 4);   // BattleAnimHeader.comFileData
			uint8_t *skel = com ? *(uint8_t **)com : nullptr;           // skeleton section
			static uint8_t skel_save[16 + 48 * 256];
			uint32_t skel_size = skel ? 16 + 48 * (uint32_t)skel[0] : 0;
			if (skel && skel_size <= sizeof(skel_save))
			{
				memcpy(skel_save, skel, skel_size);
				ComputeBonesWorldMatrices(ModelBuffer() + 0x88, ModelBuffer() + 0x68);
				for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
					if ((uint32_t)t->func == ORIG_ArcTask) ArcHeld((ArcNode *)t, num, den);
				memcpy(skel, skel_save, skel_size);
			}
		}
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		for (TaskNode *t = QueueBolts().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_BoltTask) BoltHeld((BoltNode *)t, num, den);
		GteSetRotMatrix(&BoltFrame());
		GteSetTransVector(&BoltFrame());
		for (TaskNode *t = QueueBolts().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_BranchTask) BranchHeld((BranchNode *)t, num, den);
		for (TaskNode *t = QueueDebris().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_FlashTask) FlashHeld((FlashNode *)t, num, den);
		for (TaskNode *t = QueueDebris().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_DebrisTask) DebrisHeld((DebrisNode *)t, num, den);
		PacketCursor() = cursor;
		var<uint32_t>(0x1D8E054) = frame_cursor;
	}
}

	static void register_mag116_held()
	{
		mark_held(q116::ORIG_CreatureTask);
		mark_held(q116::ORIG_DebrisTask);
		mark_held(q116::ORIG_BranchTask);
		mark_held(q116::ORIG_BoltTask);
		mark_held(q116::ORIG_FlashTask);
		mark_held(q116::ORIG_ArcTask);
		register_module_held(116, q116::HeldReady, q116::HeldFrame);
		register_module_camera(116, q116::HeldCamera);
	}
}
