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

// 30 fps part of mag187_odin.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag187_odin.cpp)

namespace ff8fx
{
namespace o187
{
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false; // the 0x201 flags were set on that tick (nothing advanced)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
	}

	// ---- held-frame memos ----
	static NodeMemo<Node24, 128> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return n->c == (int16_t)(m->c + 1); }

	// sprite records as drawn (per pool, record, draw ordinal: pool C mask 2 is drawn twice a tick)
	struct PoolMemo { uint32_t tick; const void *owner; Rec r; bool died; };
	static PoolMemo g_pool_memo[3][150][2];
	static PoolMemo *MemoRecord(int pool, int i, const void *owner, const Rec *r)
	{
		PoolMemo *m = g_pool_memo[pool][i];
		int k = 0;
		if (m[0].tick == g_real_tick)
		{
			if (m[1].tick == g_real_tick) return nullptr;
			k = 1;
		}
		m[k].tick = g_real_tick;
		m[k].owner = owner;
		m[k].r = *r;
		m[k].died = false;
		return &m[k];
	}

	static void MemoDied(PoolMemo *pm) { if (pm) pm->died = true; }

	// held-frame memo of the creature: the drawn skeleton (pose values + local matrices), the
	// reader command, the model matrix and the colour it was drawn with
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		bool midpoint;   // the next draw shows the pose one frame ahead of this one
		bool hold_mat;   // the next tick cuts the model matrix
		Mat4x3 mat;
		uint32_t color;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	static uint8_t *Skeleton()
	{
		uint8_t *com = *(uint8_t **)(Creature() + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

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
		m.mat = CreatureMat();
		m.color = *(uint32_t *)(Creature() + 0x28);
		m.midpoint = false;
		m.hold_mat = true;
		m.tick = g_real_tick;
	}

	// reader steps between the draw at tick c and the one at c + 1 (see the CreatureTask table)
	static bool AdvanceAfterDraw(int32_t c) { return (c >= 0 && c < 12) || (c >= 65 && c < 131) || (c >= 132 && c < 143) || (c >= 144 && c < 155) || (c >= 168 && c < 198); }
	static bool AdvanceBeforeDraw(int32_t c) { return c >= 12 && c < 53; }
	static bool SetAnimBeforeDraw(int32_t c) { return c == 0 || c == 12 || c == 40 || c == 65 || c == 108 || c == 120 || c == 132 || c == 144 || c == 157; }

	// end of a creature tick that advanced: how the next draw relates to the one just memoed
	static void held_note_creature_next(const Node24 *n)
	{
		if (g_creature.tick == g_real_tick)
		{
			int32_t drawn = n->c - 1, nc = n->c;
			int steps = (AdvanceAfterDraw(drawn) ? 1 : 0) + (AdvanceBeforeDraw(nc) ? 1 : 0);
			g_creature.midpoint = steps == 1 && !SetAnimBeforeDraw(nc) && nc < 0xC6;
			// the model matrix the next draw uses: the current one, except on its cuts
			g_creature.hold_mat = nc == 12 || nc == 120 || nc == 157 || nc >= 0xC6;
		}
	}

	struct FlashMemo { int32_t level, otz; };
	static NodeMemo<FlashMemo, 8> g_flash_memo;

	static void held_note_flash(const Node24 *n, int32_t level, int32_t otz)
	{
		if (FlashMemo *m = g_flash_memo.put(n)) { m->level = level; m->otz = otz; }
	}

	// mist: held frames draw around the translation of this tick
	static void held_note_mist(const Node24 *n, const uint8_t *s)
	{
		if (Node24 *m = g_node_memo.put(n))
		{
			*m = *n;
			m->p10 = (int16_t)*(const int32_t *)(s + 0x1C);
			m->p12 = (int16_t)*(const int32_t *)(s + 0x20);
			m->p14 = (int16_t)*(const int32_t *)(s + 0x24);
		}
	}

	struct BladeMemo { uint32_t tick; int16_t c; Mat4x3 e, bone; };
	static BladeMemo g_blade = { 0xFFFFFFFF };

	static void held_note_blade(const Node24 *n)
	{
		g_blade.tick = g_real_tick;
		g_blade.c = n->c;
		g_blade.e = CreatureMat();
		g_blade.bone = BoneCopy();
	}

	struct TrailMemo { bool drawn; uint8_t a[0x100], b[0x100]; };
	static NodeMemo<TrailMemo, 8> g_trail_memo;

	static TrailMemo *held_trail_put(const Node24 *n)
	{
		TrailMemo *m = g_trail_memo.put(n);
		if (m) m->drawn = false;
		return m;
	}

	static void held_trail_drawn(TrailMemo *m)
	{
		if (m)
		{
			m->drawn = true;
			memcpy(m->a, (const void *)SPLINE_A, 0x100);
			memcpy(m->b, (const void *)SPLINE_B, 0x100);
		}
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames, texture scrolls
	// in whole steps and random choices keep their 15 Hz batches. The frame matrix is rebuilt
	// from the held-frame camera while the timeline (its owner) is alive, into a local (never
	// into 0x24FE8A0). Module globals written by the draws (blade end points, spline rails, the
	// morph vertices, the creature state, the target skeletons) are either computed into
	// private copies or put back.
	// ------------------------------------------------------------------
	static uint8_t g_scratch[0x20000];   // RenderGeometry vertex scratch
	static uint8_t g_morph_verts[0x400]; // 126 x 8
	static uint8_t g_skel_cur[SKEL_MAX];

	static int16_t Lerp16(int32_t a, int32_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	static void LerpMatrix(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		*out = a;
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++) out->m[i][j] = Lerp16(a.m[i][j], b.m[i][j], num, den);
			out->t[i] = lerp_i(a.t[i], b.t[i], num, den);
		}
	}

	// ---- grids ----
	static void GridHeld(const Node24 *n, bool b, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		bool mv = NodeMoved(m, n) && !g_tick_paused;
		int32_t f = b ? GridBFade(c) : GridAFade(c);
		int32_t u = b ? c * 2 : c, v = c * 2;
		if (mv)
		{
			f = lerp_i(f, b ? GridBFade(c + 1) : GridAFade(c + 1), num, den);
			u = lerp_i(u, b ? (c + 1) * 2 : c + 1, num, den);
			v = lerp_i(v, (c + 1) * 2, num, den);
		}
		GridDraw(m, b ? 0x200 : 0x100, b ? 0x10 : 0x20, u, v, f);
	}

	// ---- lit sprite pools: memo record integrated to the next draw, half way; frame kept ----
	static void PoolHeld(int pool, const void *owner, uint8_t *h, int kind, int steps, bool debris, int num, int den)
	{
		for (int i = 0; i < 150; i++)
		{
			for (int k = 0; k < 2; k++)
			{
				const PoolMemo &mm = g_pool_memo[pool][i][k];
				if (mm.tick != g_real_tick || mm.owner != owner) continue;
				Rec t = mm.r;
				if (!mm.died && !g_tick_paused)
					for (int s = 0; s < steps; s++) Integrate(kind, &t);
				int16_t pos[4] = { Lerp16(mm.r.x, t.x, num, den), Lerp16(mm.r.y, t.y, num, den), Lerp16(mm.r.z, t.z, num, den), mm.r.w0E };
				int16_t size = Lerp16(mm.r.size, t.size, num, den);
				if (debris) DebrisDrawOne(h, pos, size);
				else LitSprite(h, pos, size, mm.r.age);
			}
		}
	}

	static void DebrisHeld(const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		DebrisSetupA(h, s);
		PoolHeld(0, owner, h, INT_FALL, 1, true, num, den);
		DebrisSetupB(h, s);
		for (int i = 0; i < 150; i++)
		{
			const PoolMemo &mm = g_pool_memo[1][i][0];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			SplashDrawOne(h, s, &mm.r); // standing splashes: the frame drawn
		}
		FieldFree(0x68);
		FieldFree(0xB4);
	}

	static void LitPoolHeld(const Mat4x3 *frame, const Node24 *n, int pool, uint32_t seq, int32_t ty, int kind, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 0x24) = 0;
		LitSetup(frame, s, 0, ty, 0x28);
		PoolHeld(pool, n, h, kind, 1, false, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void MistHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE3CC38;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		LitSetup(frame, s, m->p10, m->p12, m->p14); // the bone position of the last tick
		PoolHeld(0, n, h, INT_GROW5_DRAG8, 1, false, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void SparkHeld(const Mat4x3 *frame, const Node24 *n, bool two, int drivers, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		if (two) SparkSetup(frame, h, s, 0xE3D154, 0x202020);
		else SparkSetup(frame, h, s, 0xE3CC38, 0x404040);
		PoolHeld(2, n, h, two ? INT_GROW6_DRAG8 : INT_GROW5_DRAG8, two ? drivers : 1, false, num, den);
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	// ---- node-driven models ----
	static void GlowHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const Node24 *m = g_node_memo.get(n);
		if (m) GlowDraw(frame, m, m->c); // the scroll steps whole units: the one drawn
	}

	static void StreakHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t x = m->p10;
		if (NodeMoved(m, n) && !g_tick_paused) x = lerp_i(m->p10, n->p10, num, den);
		StreakDraw(m, x);
	}

	static void SliceHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t off = m->s1C;
		if (NodeMoved(m, n) && !g_tick_paused && n->c < 80) off = lerp_i(m->s1C, n->s1C, num, den);
		uint8_t *ent = Entity((uint32_t)(int32_t)m->e);
		// the draw rebuilds the target's local bone matrices: put its skeleton back afterwards
		uint8_t *com = *(uint8_t **)(ent + 0x64);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		uint32_t size = sk ? 16 + 48 * (uint32_t)sk[0] : 0;
		if (!sk || size > SKEL_MAX) return;
		memcpy(g_skel_cur, sk, size);
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
		SliceDraw(h, m, ent, off, m->c, false);
		FieldFree(0x1D0);
		memcpy(sk, g_skel_cur, size);
	}

	static void CreatureHeld(const Mat4x3 *frame, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		Mat4x3 mat_cur = CreatureMat();
		uint32_t color_cur = *(uint32_t *)(E + 0x28);
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		bool mv = !g_tick_paused;
		if (m.midpoint && mv) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		CreatureMat() = m.mat;
		if (mv && !m.hold_mat)
			for (int k = 0; k < 3; k++) CreatureMat().t[k] = lerp_i(m.mat.t[k], mat_cur.t[k], num, den);
		*(uint32_t *)(E + 0x28) = mv ? LerpColor(m.color, color_cur, num, den) : m.color;
		DrawModel(E, frame, (uint32_t)g_scratch);
		*(uint32_t *)(E + 0x28) = color_cur;
		CreatureMat() = mat_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void RingHeld(const Mat4x3 *frame, const Node24 *n, bool b, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n) && !g_tick_paused;
		int32_t c = m->c;
		int32_t sx = m->s1C, sy = m->s20;
		int32_t start = b ? 4 : 6;
		int32_t f = b ? RingBFade(c) : RingAFade(c);
		if (mv)
		{
			sx = lerp_i(m->s1C, n->s1C, num, den);
			sy = lerp_i(m->s20, n->s20, num, den);
			if (c >= start) f = lerp_i(f, b ? RingBFade(c + 1) : RingAFade(c + 1), num, den);
		}
		RingDraw(frame, m, sx, sy, c >= start, f);
	}

	static void FlashHeld(const Node24 *n, int num, int den)
	{
		const FlashMemo *m = g_flash_memo.get(n);
		if (!m || m->level < 0) return;
		int32_t level = m->level;
		if (!g_tick_paused)
		{
			int32_t next = FlashLevel(n->c, n->e, n->s1C);
			if (next >= 0) level = lerp_i(m->level, next, num, den);
		}
		ScreenTile(level, level, level, m->otz);
	}

	static void MorphHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t arg = MorphArg(c), f = mul32(c - 6, 682);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			arg = lerp_i(arg, MorphArg(c + 1), num, den);
			if (c >= 6) f = lerp_i(f, mul32(c - 5, 682), num, den);
		}
		MorphDraw(m, arg, c >= 6, f, (uint32_t)g_morph_verts);
	}

	static void BladeHeld(const Mat4x3 *frame, int num, int den)
	{
		if (g_blade.tick != g_real_tick) return;
		Mat4x3 e = g_blade.e, bone = g_blade.bone;
		if (!g_tick_paused)
		{
			LerpMatrix(&e, g_blade.e, CreatureMat(), num, den);
			LerpMatrix(&bone, g_blade.bone, BoneCopy(), num, den);
		}
		int16_t p0[4], p1[4];
		memcpy(p0, BladeP0(), 8);
		memcpy(p1, BladeP1(), 8);
		BladeDraw(frame, &e, &bone, p0, p1, g_blade.c > 0);
	}

	static void TrailHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const TrailMemo *m = g_trail_memo.get(n);
		if (!m || !m->drawn) return; // the trail grows in 15 Hz steps: the one drawn, from the held camera
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		TrailDraw(frame, (uint32_t)m->a, (uint32_t)m->b, s);
		FieldFree(0x40);
	}

	static void SlashHeld(const Node24 *n, bool model, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t in_len = model ? 8 : 6;
		int32_t c = m->c;
		int32_t scale = m->s1C, f = SlashFade(c, m->s20, m->p16, in_len);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			scale = lerp_i(m->s1C, n->s1C, num, den);
			f = lerp_i(f, SlashFade(c + 1, m->s20, m->p16, in_len), num, den);
		}
		SlashDraw(m, scale, f, model);
	}

	static uint8_t g_held_packets[0x80000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order; packets go to a private buffer (the module draws through the
	// engine cursor battle_texture_data_ptr_1D8E054, redirected and put back)
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		bool timeline = false;
		int drivers = 0;
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
			if ((uint32_t)t->func == ORIG_Spark2Task) drivers++;
		}
		if (drivers < 1) drivers = 1;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &frame);
		else frame = Frame();
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_GridATask: GridHeld(n, false, num, den); break;
			case ORIG_GridBTask: GridHeld(n, true, num, den); break;
			case ORIG_DebrisTask: DebrisHeld(n, num, den); break;
			case ORIG_GlowTask: GlowHeld(&frame, n); break;
			case ORIG_StreakTask: StreakHeld(n, num, den); break;
			case ORIG_SliceTask: SliceHeld(n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, num, den); break;
			case ORIG_SmokeTask: LitPoolHeld(&frame, n, 0, 0xE3D300, 0, INT_DRAG4, num, den); break;
			case ORIG_RingATask: RingHeld(&frame, n, false, num, den); break;
			case ORIG_RingBTask: RingHeld(&frame, n, true, num, den); break;
			case ORIG_DustTask: LitPoolHeld(&frame, n, 1, 0xE3CC38, -40, INT_XZ_DRAG8, num, den); break;
			case ORIG_MistTask: MistHeld(&frame, n, num, den); break;
			case ORIG_FlashTask: FlashHeld(n, num, den); break;
			case ORIG_MorphTask: MorphHeld(n, num, den); break;
			case ORIG_SparkTask: SparkHeld(&frame, n, false, 1, num, den); break;
			case ORIG_Spark2Task: SparkHeld(&frame, n, true, drivers, num, den); break;
			case ORIG_BladeTask: BladeHeld(&frame, num, den); break;
			case ORIG_TrailTask: TrailHeld(&frame, n); break;
			case ORIG_SlashATask: SlashHeld(n, true, num, den); break;
			case ORIG_SlashBTask: SlashHeld(n, true, num, den); break;
			case ORIG_SlashCTask: SlashHeld(n, false, num, den); break;
			case ORIG_SlashDTask: SlashHeld(n, false, num, den); break;
			default: break;
			}
		}
		Cursor() = cursor;
	}
}

	static void register_mag187_held()
	{
		mark_held(o187::ORIG_GridATask);
		mark_held(o187::ORIG_GridBTask);
		mark_held(o187::ORIG_DebrisTask);
		mark_held(o187::ORIG_GlowTask);
		mark_held(o187::ORIG_StreakTask);
		mark_held(o187::ORIG_SliceTask);
		mark_held(o187::ORIG_CreatureTask);
		mark_held(o187::ORIG_SmokeTask);
		mark_held(o187::ORIG_RingATask);
		mark_held(o187::ORIG_RingBTask);
		mark_held(o187::ORIG_DustTask);
		mark_held(o187::ORIG_MistTask);
		mark_held(o187::ORIG_FlashTask);
		mark_held(o187::ORIG_MorphTask);
		mark_held(o187::ORIG_SparkTask);
		mark_held(o187::ORIG_Spark2Task);
		mark_held(o187::ORIG_BladeTask);
		mark_held(o187::ORIG_TrailTask);
		mark_held(o187::ORIG_SlashATask);
		mark_held(o187::ORIG_SlashBTask);
		mark_held(o187::ORIG_SlashCTask);
		mark_held(o187::ORIG_SlashDTask);
		register_module_held(187, o187::HeldReady, o187::HeldFrame);
	}
}
