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

// 30 fps part of mag326_odin_reverse.cpp: memos of what the real tick drew and the held-frame
// drawing (included at the end of mag326_odin_reverse.cpp)

namespace ff8fx
{
namespace o326
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

	// held-frame memo of a model: the drawn skeleton (pose values + local matrices), the reader
	// command, the model matrix and the colour it was drawn with
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct ModelMemo
	{
		uint32_t tick;
		bool midpoint;   // the next draw shows the pose one frame ahead of this one
		bool hold_mat;   // the next tick cuts the model matrix
		int32_t next_dz; // the next tick moves t.z by this before drawing (creature 144..152)
		Mat4x3 mat;
		uint32_t color;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static ModelMemo g_creature = { 0xFFFFFFFF };
	static ModelMemo g_fade_a = { 0xFFFFFFFF };
	static ModelMemo g_fade_b = { 0xFFFFFFFF };

	static uint8_t *Skeleton(uint8_t *E)
	{
		uint8_t *com = *(uint8_t **)(E + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void MemoTake(ModelMemo &m, uint8_t *E)
	{
		uint8_t *sk = Skeleton(E);
		m.tick = 0xFFFFFFFF;
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.cmd, E + 0x6C, 8);
		m.mat = *(const Mat4x3 *)(E + 0x40);
		m.color = *(uint32_t *)(E + 0x28);
		m.midpoint = false;
		m.hold_mat = true;
		m.next_dz = 0;
		m.tick = g_real_tick;
	}

	static void held_note_creature(uint8_t *E) { MemoTake(g_creature, E); }

	struct SliceMemo { uint32_t tick; bool phase_b; int32_t cc; int16_t angle; uint32_t color; };
	static SliceMemo g_slice = { 0xFFFFFFFF };

	// reader steps between the draw at tick c and the one at c + 1 (see the CreatureTask table)
	static bool AdvanceAfterDraw(int32_t c) { return (c >= 0 && c < 12) || (c >= 65 && c < 120) || (c >= 132 && c < 174); }
	static bool AdvanceBeforeDraw(int32_t c) { return c >= 12 && c < 53; }
	static bool SetAnimBeforeDraw(int32_t c) { return c == 0 || c == 12 || c == 40 || c == 65 || c == 108 || c == 120 || c == 143; }

	static void held_note_slice_clear() { g_slice.tick = 0xFFFFFFFF; }

	// the sliced creature (174..253) as drawn: phase, tick, cut angle, colour
	static void held_note_slice(bool phase_b, int32_t cc, int16_t angle, const uint8_t *E)
	{
		g_slice.tick = g_real_tick;
		g_slice.phase_b = phase_b;
		g_slice.cc = cc;
		g_slice.angle = angle;
		g_slice.color = *(const uint32_t *)(E + 0x28);
	}

	// end of a creature tick that advanced: how the next draw relates to the one just memoed
	static void held_note_creature_next(const Node24 *n)
	{
		if (g_creature.tick == g_real_tick)
		{
			int32_t drawn = n->c - 1, nc = n->c;
			int steps = (AdvanceAfterDraw(drawn) ? 1 : 0) + (AdvanceBeforeDraw(nc) ? 1 : 0);
			g_creature.midpoint = steps == 1 && !SetAnimBeforeDraw(nc) && nc <= 174;
			// the model matrix the next draw uses: the current one moved by next_dz, except on its cuts
			g_creature.hold_mat = nc == 12 || nc == 143 || nc >= 174;
			g_creature.next_dz = nc >= 144 && nc <= 152 ? -0x1C2 : 0;
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

	struct GhostMemo { uint32_t tick; Node24 n; int32_t k; Mat4x3 mat; };
	static GhostMemo g_ghost = { 0xFFFFFFFF };

	static void held_note_ghost(const Node24 *n, int32_t k, const Mat4x3 &mat)
	{
		g_ghost.tick = g_real_tick;
		g_ghost.n = *n;
		g_ghost.k = k;
		g_ghost.mat = mat;
	}

	// fading models: FadeATask draws E2 (memo g_fade_a), FadeBTask E3 (memo g_fade_b)
	static ModelMemo &FadeMemo(const uint8_t *E) { return E == Model2() ? g_fade_a : g_fade_b; }

	static void held_note_fade(const Node24 *n, uint8_t *E)
	{
		MemoTake(FadeMemo(E), E);
		MemoNode(n);
	}

	static void held_note_fade_next(const uint8_t *E, int32_t c)
	{
		ModelMemo &memo = FadeMemo(E);
		memo.midpoint = c >= 1; // the next tick advances one frame (the pose right after SetAnim is held)
		memo.hold_mat = true;   // the matrix never moves
	}

	struct TintMemo { uint8_t rgb[4]; };
	static NodeMemo<TintMemo, 8> g_tint_memo;

	static void held_note_tint(const Node24 *n, const uint8_t *L)
	{
		if (TintMemo *m = g_tint_memo.put(n)) memcpy(m->rgb, L, 4);
		MemoNode(n);
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames, per-frame table
	// values and random choices keep their 15 Hz batches. The frame matrix is rebuilt from the
	// held-frame camera while the timeline (its owner) is alive, into a local (never into
	// 0x24F2268). Module globals written by the draws (blade end points, bone copy, model
	// matrices / colours / skeletons, the morph vertices) are either computed into private
	// copies or put back; model draws use a private vertex scratch.
	// ------------------------------------------------------------------
	static uint8_t g_scratch[0x20000];   // RenderGeometry / clipped renderer vertex scratch
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

	// the creature matrix the next real tick draws with (t.z moves before the draw at 144..152)
	static Mat4x3 CreatureMatNext()
	{
		Mat4x3 m = CreatureMat();
		if (g_creature.tick == g_real_tick) m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)g_creature.next_dz);
		return m;
	}

	// draws model E with the memo's skeleton (at the midpoint pose if asked), the given matrix and
	// colour and the private scratch; puts the skeleton, the reader command, matrix and colour back
	static void ModelHeld(const ModelMemo &m, uint8_t *E, const Mat4x3 *frame, const Mat4x3 &mat, uint32_t color, bool midpoint, int num, int den)
	{
		if (m.tick != g_real_tick) return;
		uint8_t *sk = Skeleton(E);
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		Mat4x3 mat_cur = *(Mat4x3 *)(E + 0x40);
		uint32_t color_cur = *(uint32_t *)(E + 0x28);
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		if (midpoint) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		*(Mat4x3 *)(E + 0x40) = mat;
		*(uint32_t *)(E + 0x28) = color;
		DrawModel(E, frame, (uint32_t)g_scratch);
		*(uint32_t *)(E + 0x28) = color_cur;
		*(Mat4x3 *)(E + 0x40) = mat_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	// ---- grids ----
	static void GridHeld(const Node24 *n, int kind, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		bool mv = NodeMoved(m, n) && !g_tick_paused;
		switch (kind)
		{
		case 0: // A
		case 1: // B
		{
			bool b = kind == 1;
			int32_t f = b ? GridBFade(c) : GridAFade(c);
			int32_t u = b ? c * 2 : c, v = c * 2;
			if (mv)
			{
				f = lerp_i(f, b ? GridBFade(c + 1) : GridAFade(c + 1), num, den);
				u = lerp_i(u, b ? (c + 1) * 2 : c + 1, num, den);
				v = lerp_i(v, (c + 1) * 2, num, den);
			}
			GridDraw(m, b ? 0x200 : 0x100, b ? 0x10 : 0x20, u, v, f);
			break;
		}
		case 2: // C: the grey level steps (15 Hz), the scroll moves
		{
			int32_t u = GridCU(c);
			if (mv) u = lerp_i(u, GridCU(c + 1), num, den);
			GridDraw(m, 0x100, 0x20, u, -u, GridCFade(c));
			break;
		}
		default: // D
		{
			int32_t u = c * 2;
			if (mv) u = lerp_i(u, (c + 1) * 2, num, den);
			GridDraw(m, 0x200, 0x10, u, -u, 0x40);
			break;
		}
		}
	}

	// ---- lit sprite pools: memo record integrated to the next draw, half way; frame kept ----
	enum PoolDraw { DRAW_DEBRIS, DRAW_LIT, DRAW_LIT_MAC };
	static void PoolHeld(int pool, const void *owner, uint8_t *h, int kind, int steps, int how, int num, int den)
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
				if (how == DRAW_DEBRIS) DebrisDrawOne(h, pos, size);
				else LitSprite(h, pos, size, mm.r.age, how == DRAW_LIT_MAC);
			}
		}
	}

	static void DebrisHeld(const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		DebrisSetupA(h, s);
		PoolHeld(0, owner, h, INT_FALL, 1, DRAW_DEBRIS, num, den);
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
		PoolHeld(pool, n, h, kind, 1, DRAW_LIT, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void MistHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE108E8;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		LitSetup(frame, s, m->p10, m->p12, m->p14); // the bone position of the last tick
		PoolHeld(0, n, h, INT_GROW5_DRAG8, 1, DRAW_LIT, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void SparkHeld(const Mat4x3 *frame, const Node24 *n, bool two, int drivers, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		if (two) SparkSetup(frame, h, s, 0xE10E04, 0x202020);
		else SparkSetup(frame, h, s, 0xE108E8, 0x404040);
		PoolHeld(2, n, h, two ? INT_GROW6_DRAG8 : INT_GROW5_DRAG8, two ? drivers : 1, DRAW_LIT, num, den);
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void BloodHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		BloodSetup(frame, h, s);
		PoolHeld(2, n, h, INT_BLOOD, 1, DRAW_LIT_MAC, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
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

	static void WallHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t s20 = m->s20, z = m->p14;
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			s20 = lerp_i(m->s20, n->s20, num, den);
			z = lerp_i(m->p14, n->p14, num, den);
		}
		WallDraw(m, s20, z);
	}

	static void ShockHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t sx = m->s1C;
		if (NodeMoved(m, n) && !g_tick_paused) sx = lerp_i(m->s1C, n->s1C, num, den);
		ShockDraw(m, sx);
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

	static void TintHeld(const Node24 *n, int num, int den)
	{
		const TintMemo *m = g_tint_memo.get(n);
		const Node24 *mn = g_node_memo.get(n);
		if (!m || !mn) return;
		uint8_t rgb[4];
		memcpy(rgb, m->rgb, 4);
		if (NodeMoved(mn, n) && !g_tick_paused)
		{
			uint8_t next[4];
			TintColor(mn->c + 1, next);
			for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)lerp_i(m->rgb[k], next[k], num, den);
		}
		ScreenTile(rgb[0], rgb[1], rgb[2], 0x1000);
	}

	static void BurstHeld(const Node24 *n, bool b, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t scroll = shl32(c, 4), fade = BurstFade(b, c);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			scroll = lerp_i(scroll, shl32(c + 1, 4), num, den);
			if (c >= (b ? 7 : 6)) fade = lerp_i(fade, BurstFade(b, c + 1), num, den);
		}
		BurstDraw(m, c, b, scroll, fade);
	}

	static void SlashHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t scale = m->s1C, f = SlashFade(c, m->s20, m->p16);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			scale = lerp_i(m->s1C, n->s1C, num, den);
			f = lerp_i(f, SlashFade(c + 1, m->s20, m->p16), num, den);
		}
		SlashDraw(m, scale, f);
	}

	// ---- the creature ----
	static void SliceHeld(const Mat4x3 *frame, int num, int den)
	{
		const SliceMemo &s = g_slice;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton(E);
		uint32_t size = sk ? 16 + 48 * (uint32_t)sk[0] : 0;
		if (!sk || size > SKEL_MAX) return;
		bool mv = !g_tick_paused;
		uint32_t color_cur = *(uint32_t *)(E + 0x28);
		memcpy(g_skel_cur, sk, size);
		if (!s.phase_b)
		{
			// halves drifting apart: 8c' - 8 -> 8c' (the switch to phase B at 200 is a cut)
			int32_t d = s.cc * 8 - 8;
			if (mv && s.cc + 1 < 0x1A) d = lerp_i(d, s.cc * 8, num, den);
			*(uint32_t *)(E + 0x28) = s.color;
			SliceADraw(E, d, frame, (uint32_t)g_scratch);
		}
		else
		{
			int16_t a = s.angle;
			uint32_t col = s.color;
			int32_t nc = s.cc + 1;
			if (mv && nc < 0x36)
			{
				if (nc >= 0x1E) a = lerp_angle(a, (int16_t)(a + 0x12), num, den);
				if (nc >= 0x2E)
				{
					uint32_t next = 0;
					DepthCue(SliceBFadeW(nc), 0x80, 0x80, 0x80, &next);
					col = LerpColor(col, next, num, den);
				}
			}
			SliceBDraw(E, s.cc, a, frame, (uint32_t)g_scratch, false, col);
		}
		*(uint32_t *)(E + 0x28) = color_cur;
		memcpy(sk, g_skel_cur, size);
	}

	static void CreatureHeld(const Mat4x3 *frame, int num, int den)
	{
		if (g_slice.tick == g_real_tick)
		{
			SliceHeld(frame, num, den);
			return;
		}
		const ModelMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		bool mv = !g_tick_paused;
		Mat4x3 mat = m.mat;
		if (mv && !m.hold_mat)
		{
			Mat4x3 next = CreatureMatNext();
			for (int k = 0; k < 3; k++) mat.t[k] = lerp_i(m.mat.t[k], next.t[k], num, den);
		}
		// the colour the next draw uses is already in place (the fade-in task runs after the creature)
		uint32_t color = mv ? LerpColor(m.color, *(uint32_t *)(E + 0x28), num, den) : m.color;
		ModelHeld(m, E, frame, mat, color, m.midpoint && mv, num, den);
	}

	static void BladeHeld(const Mat4x3 *frame, int num, int den)
	{
		if (g_blade.tick != g_real_tick) return;
		Mat4x3 e = g_blade.e, bone = g_blade.bone;
		if (!g_tick_paused)
		{
			LerpMatrix(&e, g_blade.e, CreatureMatNext(), num, den);
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

	// ---- model 2: ghost trail and its sword ----
	static bool g_m2_valid = false;
	static Mat4x3 g_m2_held;

	// the ghost's in-between matrix (turned half way, moved half the velocity); the echoes stay
	// where they were recorded
	static void GhostPrepare(int num, int den)
	{
		g_m2_valid = false;
		if (g_ghost.tick != g_real_tick) return;
		const Node24 &n = g_ghost.n;
		Mat4x3 m = g_ghost.mat;
		if (!g_tick_paused)
		{
			GhostPose(&m, lerp_angle(n.a18, (int16_t)(n.a18 + n.s1A), num, den));
			m.t[0] = (int32_t)((uint32_t)g_ghost.mat.t[0] + (uint32_t)lerp_i(0, n.p10, num, den));
			m.t[1] = (int32_t)((uint32_t)g_ghost.mat.t[1] + (uint32_t)lerp_i(0, n.p12, num, den));
			m.t[2] = (int32_t)((uint32_t)g_ghost.mat.t[2] + (uint32_t)lerp_i(0, n.p14, num, den));
		}
		g_m2_held = m;
		g_m2_valid = true;
	}

	static void GhostHeld(const Mat4x3 *frame)
	{
		if (!g_m2_valid) return;
		uint8_t *E = Model2();
		uint8_t *sk = Skeleton(E);
		uint32_t size = sk ? 16 + 48 * (uint32_t)sk[0] : 0;
		if (!sk || size > SKEL_MAX) return;
		Mat4x3 mat_cur = Model2Mat();
		uint32_t color_cur = var<uint32_t>(0x24F0F10);
		memcpy(g_skel_cur, sk, size);
		Model2Mat() = g_m2_held;
		DrawModel(E, frame, (uint32_t)g_scratch);
		GhostEchoes(E, g_ghost.k, frame, (uint32_t)g_scratch);
		var<uint32_t>(0x24F0F10) = color_cur;
		Model2Mat() = mat_cur;
		memcpy(sk, g_skel_cur, size);
	}

	static void SwordHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		Mat4x3 mat_cur = Model2Mat(), bone_cur = BoneCopy();
		if (g_m2_valid) Model2Mat() = g_m2_held;
		SwordDraw(frame, &Model2Mat(), m->c > 0);
		BoneCopy() = bone_cur;
		Model2Mat() = mat_cur;
	}

	// ---- the fading models ----
	static void FadeHeld(const Mat4x3 *frame, const Node24 *n, uint8_t *E, const ModelMemo &memo, int num, int den)
	{
		const Node24 *mn = g_node_memo.get(n);
		if (!mn || memo.tick != g_real_tick) return;
		bool mv = NodeMoved(mn, n) && !g_tick_paused;
		uint32_t color = memo.color;
		if (mv && mn->c + 1 >= 0x13)
		{
			uint32_t next = 0;
			DepthCue(FadeW(mn->c + 1), 0x80, 0x80, 0x80, &next);
			color = LerpColor(memo.color, next, num, den);
		}
		ModelHeld(memo, E, frame, memo.mat, color, memo.midpoint && mv, num, den);
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
		GhostPrepare(num, den);
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_GridATask: GridHeld(n, 0, num, den); break;
			case ORIG_GridBTask: GridHeld(n, 1, num, den); break;
			case ORIG_GridCTask: GridHeld(n, 2, num, den); break;
			case ORIG_GridDTask: GridHeld(n, 3, num, den); break;
			case ORIG_DebrisTask: DebrisHeld(n, num, den); break;
			case ORIG_GlowTask: GlowHeld(&frame, n); break;
			case ORIG_StreakTask: StreakHeld(n, num, den); break;
			case ORIG_WallTask: WallHeld(n, num, den); break;
			case ORIG_ShockTask: ShockHeld(n, num, den); break;
			case ORIG_GhostTask: GhostHeld(&frame); break;
			case ORIG_SwordTask: SwordHeld(&frame, n); break;
			case ORIG_FadeATask: FadeHeld(&frame, n, Model2(), g_fade_a, num, den); break;
			case ORIG_FadeBTask: FadeHeld(&frame, n, Model3(), g_fade_b, num, den); break;
			case ORIG_TintTask: TintHeld(n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, num, den); break;
			case ORIG_SmokeTask: LitPoolHeld(&frame, n, 0, 0xE10FB0, 0, INT_DRAG4, num, den); break;
			case ORIG_RingATask: RingHeld(&frame, n, false, num, den); break;
			case ORIG_RingBTask: RingHeld(&frame, n, true, num, den); break;
			case ORIG_DustTask: LitPoolHeld(&frame, n, 1, 0xE108E8, -40, INT_XZ_DRAG8, num, den); break;
			case ORIG_MistTask: MistHeld(&frame, n, num, den); break;
			case ORIG_FlashTask: FlashHeld(n, num, den); break;
			case ORIG_MorphTask: MorphHeld(n, num, den); break;
			case ORIG_SparkTask: SparkHeld(&frame, n, false, 1, num, den); break;
			case ORIG_Spark2Task: SparkHeld(&frame, n, true, drivers, num, den); break;
			case ORIG_BladeTask: BladeHeld(&frame, num, den); break;
			case ORIG_TrailTask: TrailHeld(&frame, n); break;
			case ORIG_BurstATask: BurstHeld(n, false, num, den); break;
			case ORIG_BurstBTask: BurstHeld(n, true, num, den); break;
			case ORIG_BloodTask: BloodHeld(&frame, n, num, den); break;
			case ORIG_SlashATask:
			case ORIG_SlashBTask:
			case ORIG_SlashCTask:
			case ORIG_SlashDTask: SlashHeld(n, num, den); break;
			default: break;
			}
		}
		Cursor() = cursor;
	}
}

	static void register_mag326_held()
	{
		mark_held(o326::ORIG_GridATask);
		mark_held(o326::ORIG_GridBTask);
		mark_held(o326::ORIG_DebrisTask);
		mark_held(o326::ORIG_GlowTask);
		mark_held(o326::ORIG_StreakTask);
		mark_held(o326::ORIG_GridCTask);
		mark_held(o326::ORIG_GridDTask);
		mark_held(o326::ORIG_WallTask);
		mark_held(o326::ORIG_ShockTask);
		mark_held(o326::ORIG_GhostTask);
		mark_held(o326::ORIG_SwordTask);
		mark_held(o326::ORIG_FadeATask);
		mark_held(o326::ORIG_FadeBTask);
		mark_held(o326::ORIG_TintTask);
		mark_held(o326::ORIG_CreatureTask);
		mark_held(o326::ORIG_SmokeTask);
		mark_held(o326::ORIG_RingATask);
		mark_held(o326::ORIG_RingBTask);
		mark_held(o326::ORIG_DustTask);
		mark_held(o326::ORIG_MistTask);
		mark_held(o326::ORIG_FlashTask);
		mark_held(o326::ORIG_MorphTask);
		mark_held(o326::ORIG_SparkTask);
		mark_held(o326::ORIG_Spark2Task);
		mark_held(o326::ORIG_BladeTask);
		mark_held(o326::ORIG_TrailTask);
		mark_held(o326::ORIG_BurstATask);
		mark_held(o326::ORIG_BurstBTask);
		mark_held(o326::ORIG_BloodTask);
		mark_held(o326::ORIG_SlashATask);
		mark_held(o326::ORIG_SlashBTask);
		mark_held(o326::ORIG_SlashCTask);
		mark_held(o326::ORIG_SlashDTask);
		register_module_held(326, o326::HeldReady, o326::HeldFrame);
	}
}
