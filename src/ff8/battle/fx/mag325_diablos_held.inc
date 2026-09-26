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

// 30 fps part of mag325_diablos.cpp: memos of what the real tick drew, the held-frame drawing
// and the held-frame camera (included at the end of mag325_diablos.cpp)

namespace ff8fx
{
namespace d325
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;     // real tick on which the ported master last ran

	// ------------------------------------------------------------------
	// Held-frame memo of what the real timeline tick drew
	// ------------------------------------------------------------------
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct PoseMemo { bool ok; uint32_t size; uint8_t pose[SKEL_MAX]; };
	struct TimelineMemo
	{
		uint32_t tick;
		const void *node;
		int16_t c;                  // counter the tick ran with
		bool play_a; PrimCtx ctx_a;
		bool grow; uint32_t grow_e;
		bool morph; uint32_t morph_e;
		int nflip; struct { int32_t x, y, z; uint32_t model; } flip[2];
		bool play_c; PrimCtx ctx_c; bool restart_c; uint8_t layout_c[0x1E8];
		bool play_b; PrimCtx ctx_b; bool b406;
		bool sink; int32_t sink_k;
		bool well; uint32_t well_e;
		bool tile; int32_t tile_e;
		int e1_draw;                // 0 none, 1 stylised (-0x2000, mode 2), 2 stylised (-0x22BC, mode 0), 3 standard
		bool e1_blend;              // E1 was drawn on the previous real tick too (the drawn pose is the memo)
		Mat4x3 e1_root; uint32_t e1_colour;
		bool e2_draw, e2_blend;
		PoseMemo p1, p2;
		int pmode;                  // particle system drawn: 2 bolts, 3 puffs
		uint8_t pool[MB_CHAINS_END - MB_PARTICLES];
	};
	static TimelineMemo g_tm = { 0xFFFFFFFF };
	static uint32_t g_e1_drawn_tick = 0xFFFFFFF0, g_e2_drawn_tick = 0xFFFFFFF0;

	static uint8_t *Skeleton(uint8_t *e, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(e + 0x64); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	static void PoseSave(uint8_t *e, PoseMemo &m)
	{
		uint32_t size = 0;
		uint8_t *sk = Skeleton(e, &size);
		m.ok = sk != nullptr;
		if (m.ok) { memcpy(m.pose, sk, size); m.size = size; }
	}

	// ------------------------------------------------------------------
	// Memo writes of the real tick (the FX_HELD statements of mag325_diablos.cpp)
	// ------------------------------------------------------------------
	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
	}

	// timeline: start of the tick
	static void held_note_timeline(TimelineNode *t)
	{
		TimelineMemo &M = g_tm;
		M.tick = g_real_tick;
		M.node = t;
		M.c = t->counter;
		M.play_a = M.grow = M.morph = M.play_c = M.restart_c = M.play_b = M.sink = M.well = M.tile = false;
		M.nflip = 0;
		M.e1_draw = 0;
		M.e2_draw = false;
		M.pmode = 0;
		// the pose each model draws this tick is the one the tick starts with (its draw builds the
		// world matrices from the bone matrices of the previous draw, before this tick's anim read)
		PoseSave(E1(t), M.p1);
		PoseSave(E2(t), M.p2);
	}

	static void held_note_play_a(const PrimCtx &ctx)
	{
		g_tm.play_a = true;
		g_tm.ctx_a = ctx;
	}

	static void held_note_grow(uint32_t e)
	{
		g_tm.grow = true;
		g_tm.grow_e = e;
	}

	static void held_note_morph(uint32_t e)
	{
		g_tm.morph = true;
		g_tm.morph_e = e;
	}

	static void held_note_flip(int32_t x, int32_t y, int32_t z, uint32_t model)
	{
		TimelineMemo &M = g_tm;
		M.flip[M.nflip++] = { x, y, z, model };
	}

	static void held_note_play_c(const PrimCtx &ctx)
	{
		g_tm.play_c = true;
		g_tm.ctx_c = ctx;
	}

	// player C finished and is restarted: the held frame needs the player as it was drawn
	static void held_note_restart_c(TimelineNode *t)
	{
		g_tm.restart_c = true;
		memcpy(g_tm.layout_c, t->prim_c, sizeof(g_tm.layout_c));
	}

	static void held_note_play_b(const PrimCtx &ctx, bool b406)
	{
		g_tm.play_b = true;
		g_tm.b406 = b406;
		g_tm.ctx_b = ctx;
	}

	static void held_note_sink(int32_t k)
	{
		g_tm.sink = true;
		g_tm.sink_k = k;
	}

	static void held_note_well(uint32_t e)
	{
		g_tm.well = true;
		g_tm.well_e = e;
	}

	static void held_note_tile(int32_t e)
	{
		g_tm.tile = true;
		g_tm.tile_e = e;
	}

	// Diablos: draw kind (0 = not drawn) and what it is drawn with
	static void held_note_e1(uint8_t *e1, int kind)
	{
		TimelineMemo &M = g_tm;
		M.e1_draw = kind;
		if (kind)
		{
			M.e1_blend = g_e1_drawn_tick + 1 == g_real_tick;
			g_e1_drawn_tick = g_real_tick;
			M.e1_root = Root(e1);
			M.e1_colour = *(uint32_t *)(e1 + 0x28);
		}
	}

	static void held_note_e2()
	{
		TimelineMemo &M = g_tm;
		M.e2_draw = true;
		M.e2_blend = g_e2_drawn_tick + 1 == g_real_tick;
		g_e2_drawn_tick = g_real_tick;
	}

	// particle pool as drawn: 2 = bolts (after their advance), 3 = puffs (before their moves)
	static void held_note_particles(int mode)
	{
		TimelineMemo &M = g_tm;
		M.pmode = mode;
		if (mode == 2) memcpy(M.pool, ModelBuffer() + MB_PARTICLES, sizeof(M.pool));
		else memcpy(M.pool, ModelBuffer() + MB_PARTICLES, MB_PUFFS_END - MB_PARTICLES);
	}

	struct RisingMemo { int16_t pos[3]; };
	static NodeMemo<RisingMemo, 64> g_rising_memo;

	static void held_note_rising(RisingNode *p)
	{
		if (RisingMemo *m = g_rising_memo.put(p)) memcpy(m->pos, p->pos, sizeof(m->pos));
	}

	static void PlayHeld(void *layout, PrimCtx &c, int num, int den)
	{
		c.cursor = FrameCursor();
		prim::play_held((prim::Layout *)layout, (prim::Callback)CB_PrimObject, (int)&c, num, den);
		FrameCursor() = c.cursor;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn between the
	// state drawn on the last real tick and the state of the next one; what jumps (cuts, stage
	// changes, flipbook frames, particle growth / spawns) keeps its 15 Hz steps. Nothing here
	// changes game state: no RNG, no spawn, no camera write; the model-buffer scratch and the
	// particle pools the held draw rewrites are put back.
	// ------------------------------------------------------------------
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

	static uint8_t g_skel_save[SKEL_MAX];

	// a model drawn between its memo pose and its current pose; draw() is the tick's draw call
	template<typename Draw>
	static void ModelHeld(uint8_t *e, const PoseMemo &pm, bool blend, int num, int den, Draw draw)
	{
		uint32_t size = 0;
		uint8_t *sk = Skeleton(e, &size);
		if (!sk) return;
		memcpy(g_skel_save, sk, size);
		if (blend && pm.ok && pm.size == size)
		{
			PoseBlend(sk, pm.pose, num, den);
			BuildBoneMatricesFromPose(e + 0x60);
		}
		draw();
		memcpy(sk, g_skel_save, size);
	}

	// next-tick value if the timeline advances by one tick inside [lo, hi), else hold
	static int32_t Toward(int32_t now, int32_t next, int32_t c, int32_t cn, int32_t lo, int32_t hi, int num, int den)
	{
		if (cn != c + 1 || cn < lo || cn >= hi) return now;
		return lerp_i(now, next, num, den);
	}

	static uint8_t g_layout_save[0x1E8];

	static void BoltsHeld(int num, int den)
	{
		// the chains as drawn, drifted towards the next tick: vertices by their velocity,
		// brightness by one decay step, chain fade by one step (growth keeps the 15 Hz batches)
		uint8_t *base = ModelBuffer() + MB_PARTICLES;
		memcpy(base, g_tm.pool, sizeof(g_tm.pool));
		if (!Pause())
		{
			uint8_t *ch = ModelBuffer() + MB_CHAINS;
			for (int k = 0; k < 64; k++, ch += 0x30)
			{
				uint8_t *v = *(uint8_t **)ch;
				if (!v) continue;
				int16_t &fade = *(int16_t *)(ch + 0x2E);
				int32_t fn = fade - 0x100;
				if (fn < 0) fn = 0;
				fade = (int16_t)lerp_i(fade, fn, num, den);
				for (int guard = 0; v && guard < 0x200; guard++)
				{
					if (v < base || v >= ModelBuffer() + MB_CHAINS) break;
					int16_t *pos = (int16_t *)v, *vel = (int16_t *)(v + 8);
					for (int a = 0; a < 3; a++) pos[a] = (int16_t)(pos[a] + vel[a] * num / den);
					int16_t &in = *(int16_t *)(v + 0x10);
					int32_t i1 = (int16_t)(in + *(int16_t *)(v + 0x12));
					if (i1 < 0) i1 = 0;
					in = (int16_t)lerp_i(in, i1, num, den);
					v = *(uint8_t **)(v + 0x14);
				}
			}
		}
		x::BoltDraw();
	}

	static void PuffsHeld(int32_t c, int num, int den)
	{
		// puffs as drawn, moved towards their state after this tick's moves; the pool resets
		// (0, 30, 90, 202, 469) and stage switches keep the drawn state
		static uint8_t cur[MB_PUFFS_END - MB_PARTICLES];
		uint8_t *pool = ModelBuffer() + MB_PARTICLES;
		memcpy(cur, pool, sizeof(cur));
		bool move = Mode() == 3 && c != 0 && c != 0x1E && c != 0x5A && c != 0xCA && c != 0x1D5;
		memcpy(pool, g_tm.pool, sizeof(cur));
		if (move)
			for (int i = 0; i < 0xA0; i++)
			{
				uint8_t *p = pool + 0x10 * i;
				const uint8_t *q = cur + 0x10 * i;
				if (!p[0xF] || *(const uint32_t *)q != *(const uint32_t *)p) continue;
				int16_t *pp = (int16_t *)(p + 4);
				const int16_t *qp = (const int16_t *)(q + 4);
				bool jump = false;
				for (int a = 0; a < 3; a++)
				{
					int32_t d = (int32_t)qp[a] - pp[a];
					if (d > 0x1000 || d < -0x1000) jump = true;
				}
				if (jump) continue;
				for (int a = 0; a < 3; a++) pp[a] = (int16_t)lerp_i(pp[a], qp[a], num, den);
				if (q[0xB] <= p[0xB]) p[0xB] = (uint8_t)lerp_i(p[0xB], q[0xB], num, den);
			}
		x::PuffDraw();
	}

	static void TimelineHeld(TimelineNode *t, int num, int den)
	{
		const TimelineMemo &M = g_tm;
		if (M.tick != g_real_tick || M.node != t) return;
		int32_t c = M.c, cn = t->counter;
		uint8_t *e1 = E1(t), *e2 = E2(t);

		if (M.play_a)
		{
			PrimCtx ctx = M.ctx_a;
			if (cn == c + 1 && (uint32_t)(cn - 1) < 0x211 && (uint32_t)(cn - 1) >= 8 && (uint32_t)(cn - 1) < 0x209)
				ctx.fade = (int16_t)lerp_i(M.ctx_a.fade, FadeA((uint32_t)(cn - 1)), num, den);
			PlayHeld(t->prim_a, ctx, num, den);
		}
		if (M.grow)
		{
			int32_t d = GrowDiag(M.grow_e);
			d = Toward(d, GrowDiag(M.grow_e + 1), c, cn, 0x1F, 0x5B, num, den);
			DrawGrowing(d, M.grow_e < 0x1E ? -0x4000 : -0x2000);
		}
		if (M.morph)
		{
			int32_t tt = (int32_t)((M.morph_e << 12) / 12u);
			tt = Toward(tt, (int32_t)(((M.morph_e + 1) << 12) / 12u), c, cn, 0x5B, 0x67, num, den);
			x::MorphPair(0, -0x2000, (int16_t)FarZ(), (uint32_t)(ModelBuffer() + 0x4C244), t->flip, 2, tt, t->morph_b, 2, tt);
		}
		for (int i = 0; i < M.nflip; i++) x::DrawPrimAt(M.flip[i].x, M.flip[i].y, M.flip[i].z, M.flip[i].model);
		if (M.play_c)
		{
			PrimCtx ctx = M.ctx_c;
			if (M.restart_c)
			{
				memcpy(g_layout_save, t->prim_c, sizeof(g_layout_save));
				memcpy(t->prim_c, M.layout_c, sizeof(g_layout_save));
				PlayHeld(t->prim_c, ctx, num, den);
				memcpy(t->prim_c, g_layout_save, sizeof(g_layout_save));
			}
			else PlayHeld(t->prim_c, ctx, num, den);
		}
		if (M.play_b && !M.b406)
		{
			PrimCtx ctx = M.ctx_b;
			PlayHeld(t->prim_b, ctx, num, den);
		}
		if (M.sink)
			DrawSinking(M.sink_k, (cn == c + 1 && M.sink_k < 7) ? 0x200 * num / den : 0);
		if (M.well)
		{
			uint32_t e = M.well_e;
			bool next = cn == c + 1 && e + 1 < 0x26;
			int32_t depth = WellDepth(e);
			if (next) depth = lerp_i(depth, WellDepth(e + 1), num, den);
			int32_t scroll = (int32_t)(e << 4) + (next ? 16 * num / den : 0);
			for (int k = 0; k < TargetCount() && k < 10; k++)
			{
				Target &tg = t->targets[k];
				int32_t save58 = *(int32_t *)(tg.entity + 0x58);
				int32_t scale[3];
				SquashTarget(tg, depth, scale);
				x::DrawEntityScaled(tg.entity, scale);
				*(int32_t *)(tg.entity + 0x58) = save58;
			}
			x::DrawWell(CenterX(), depth - 0x1000, CenterZ(), scroll);
		}
		if (M.tile)
		{
			int32_t lv = FlashTileLevel(M.tile_e);
			if (cn == c + 1 && M.tile_e + 1 < 0xC) lv = lerp_i(lv, FlashTileLevel(M.tile_e + 1), num, den);
			PacketCursor() = x::Tile(0xFF, 0xFF, 0xFF, lv, PacketCursor());
		}
		if (M.play_b && M.b406)
		{
			PrimCtx ctx = M.ctx_b;
			if (cn == c + 1 && (uint32_t)(cn - 0x196) < 0x32) ctx.scroll = (int16_t)(ctx.scroll + 4 * num / den);
			PlayHeld(t->prim_b, ctx, num, den);
		}

		if (M.e1_draw)
		{
			Mat4x3 root = Root(e1);
			uint32_t colour = *(uint32_t *)(e1 + 0x28);
			// root y: this tick's rise (92..166) happened after the draw; the next tick sinks by 10
			// before its draw (268..297); the jump at 203 holds
			Mat4x3 r = M.e1_root;
			if (cn == c + 1 && cn != 0xCB && !Pause())
			{
				int32_t next = root.t[1] + ((uint32_t)(cn - 0x10C) < 0x1E ? -10 : 0);
				r.t[1] = lerp_i(M.e1_root.t[1], next, num, den);
			}
			Root(e1) = r;
			uint8_t *col = e1 + 0x28;
			const uint8_t *c0 = (const uint8_t *)&M.e1_colour, *c1 = (const uint8_t *)&colour;
			for (int k = 0; k < 3; k++) col[k] = (uint8_t)lerp_i(c0[k], c1[k], num, den);
			int kind = M.e1_draw;
			ModelHeld(e1, M.p1, M.e1_blend, num, den, [&]() {
				if (kind == 1) x::DrawStylised(e1, -0x2000, 0x80, 2, ModelBuffer() + MB_MORPH);
				else if (kind == 2) x::DrawStylised(e1, -0x22BC, 0x80, 0, ModelBuffer() + MB_MORPH);
				else FrameCursor() = x::DrawModel(e1, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			});
			Root(e1) = root;
			*(uint32_t *)(e1 + 0x28) = colour;
		}
		if (M.e2_draw)
			ModelHeld(e2, M.p2, M.e2_blend, num, den, [&]() {
				FrameCursor() = x::DrawModel(e2, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			});

		if (M.pmode == 2) BoltsHeld(num, den);
		else if (M.pmode == 3) PuffsHeld(c, num, den);
	}

	static void RisingHeld(RisingNode *p, int num, int den)
	{
		const RisingMemo *m = g_rising_memo.get(p);
		if (!m) return;
		int16_t pos[3];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		PrimCtx c;
		CtxRising(c, pos, p->shade);
		PlayHeld(p->layout, c, num, den);
	}

	static void BurstHeld(BurstNode *p, int num, int den)
	{
		PrimCtx c;
		CtxBurst(c, p);
		PlayHeld(p->layout, c, num, den);
	}

	static uint8_t g_held_packets[0x80000]; // module arena 0x20000, then the frame arena (model draws)
	static uint8_t g_scratch_save[MB_STREAM_STATE - MB_MORPH];
	static uint8_t g_pool_save[MB_CHAINS_END - MB_PARTICLES];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static TimelineNode *Timeline()
	{
		for (TaskNode *t = QueueTimeline().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) return (TimelineNode *)t;
		return nullptr;
	}

	// mirrors the master's queue order; packets go to a private buffer (the module cursor and the
	// frame arena are redirected and put back); the morph scratch and the particle pools the held
	// draw rewrites are saved and put back, and so is the effect-camera matrix
	static void HeldFrame(int num, int den)
	{
		uint8_t *MB = ModelBuffer();
		if (!MB) return;
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		Mat4x3 effect_camera = EffectCamera();
		memcpy(g_scratch_save, MB + MB_MORPH, sizeof(g_scratch_save));
		memcpy(g_pool_save, MB + MB_PARTICLES, sizeof(g_pool_save));
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x20000;

		if (TimelineNode *t = Timeline()) TimelineHeld(t, num, den);
		if (QueueParticles().head)
		{
			EffectCameraMatrix(&Camera(), &EffectCamera());
			for (TaskNode *t = QueueParticles().head; t; t = t->next)
			{
				if ((uint32_t)t->func == ORIG_RisingTask) RisingHeld((RisingNode *)t, num, den);
				else if ((uint32_t)t->func == ORIG_BurstTask) BurstHeld((BurstNode *)t, num, den);
			}
		}

		memcpy(MB + MB_MORPH, g_scratch_save, sizeof(g_scratch_save));
		memcpy(MB + MB_PARTICLES, g_pool_save, sizeof(g_pool_save));
		EffectCamera() = effect_camera;
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}
}

	// held-frame camera of effect 325 at tick + num / den: the timeline's camera code for the
	// counter it holds now (the next tick) is run on the real camera words and put back (it only
	// writes those words and the GTE data registers), then the in-between camera; cuts and the
	// first tick of a camera move that starts elsewhere hold. false = no timeline this tick.
	bool mag325_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		using namespace d325;
		if (!HeldReady() || g_tm.tick != g_real_tick) return false;
		TimelineNode *t = Timeline();
		if (!t || t != g_tm.node) return false;
		int16_t now_w[3], now_l[3];
		for (int i = 0; i < 3; i++) { now_w[i] = CamWorld(i); now_l[i] = CamLookAt(i); }
		uint8_t save[16];
		memcpy(save, (void *)0xB8B7F0, sizeof(save));
		int r = CameraTick(t->counter, false);
		int16_t next_w[3], next_l[3];
		for (int i = 0; i < 3; i++) { next_w[i] = CamWorld(i); next_l[i] = CamLookAt(i); }
		memcpy((void *)0xB8B7F0, save, sizeof(save));
		bool move = (r & 1) && !(r & 2);
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(now_w[i], next_w[i], num, den) : now_w[i];
			lookat[i] = move ? (int16_t)lerp_i(now_l[i], next_l[i], num, den) : now_l[i];
		}
		return true;
	}

	static void register_mag325_held()
	{
		mark_held(d325::ORIG_TimelineTask);
		mark_held(d325::ORIG_RisingTask);
		mark_held(d325::ORIG_BurstTask);
		register_module_held(325, d325::HeldReady, d325::HeldFrame);
		register_module_camera(325, mag325_held_camera);
	}
}
