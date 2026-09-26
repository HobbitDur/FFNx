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

// 30 fps part of mag199_cactuar.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag199_cactuar.cpp)

namespace ff8fx
{
namespace c199
{
	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	static void held_note_master() { g_ported_tick = g_real_tick; }

	// held-frame memo of the creature (one instance): the pose it drew, its height
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		bool midpoint;         // pose advanced after the draw and the next tick shows the next frame
		int32_t y_drawn, y_next;
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

	// height the next tick will draw (the rise of ticks 43..63), from its counter and speed
	static int32_t NextCreatureY(int32_t y, int32_t c, int16_t v)
	{
		if (c >= 43 && c < 59)
		{
			if (c == 43) v = -600;
			return y + v;
		}
		if (c >= 59 && c < 64)
		{
			if (c == 59) v = -60;
			return y + v;
		}
		return y;
	}

	// memo of what was drawn (pose values + matrices of the drawn pose, reader command)
	static void held_note_creature()
	{
		uint8_t *E = Creature();
		CreatureMemo &m = g_creature;
		uint8_t *sk = Skeleton();
		m.tick = 0xFFFFFFFF;
		if (sk)
		{
			m.skel_size = 16 + 48 * (uint32_t)sk[0];
			memcpy(m.skel, sk, m.skel_size);
			memcpy(m.cmd, E + 0x6C, 8);
			m.y_drawn = CreatureY();
			m.y_next = m.y_drawn;
			m.midpoint = false;
			m.tick = g_real_tick;
		}
	}

	// after the tick's update: the height and pose the next tick draws
	static void held_note_creature_next(const Node24 *n, int advance)
	{
		CreatureMemo &m = g_creature;
		if (m.tick == g_real_tick)
		{
			int32_t c = n->c;
			m.y_next = NextCreatureY(m.y_drawn, c, n->v);
			// the next tick draws the frame just read unless it starts another animation
			m.midpoint = advance && c != 0 && c != 5 && c != 40 && c != 43 && c != 87;
		}
	}

	struct PrimMemo { int16_t c, v; };
	static NodeMemo<PrimMemo, 64> g_prim_memo;

	// prim tasks: counter and animated value (scale / angle) they were drawn with
	static void held_note_prim(const TaskNode *tn, int16_t c, int16_t v)
	{
		if (PrimMemo *m = g_prim_memo.put(tn)) { m->c = c; m->v = v; }
	}

	struct DustMemo { uint32_t tick; int16_t pos[4]; int16_t age; };
	static DustMemo g_dust_memo[100];

	// dust puff i as drawn
	static void held_note_dust(int i, const int16_t *pos, int16_t age)
	{
		DustMemo &m = g_dust_memo[i];
		m.tick = g_real_tick;
		memcpy(m.pos, pos, 8);
		m.age = age;
	}

	struct NeedleMemo { uint32_t tick; int16_t pos[3]; int16_t age; uint32_t color; };
	static NeedleMemo g_needle_memo[800];

	// needle i as drawn: position and age, then its colour
	static void held_note_needle(int i, const int16_t *pos, int16_t age)
	{
		NeedleMemo &m = g_needle_memo[i];
		m.tick = g_real_tick;
		memcpy(m.pos, pos, 6);
		m.age = age;
	}

	static void held_note_needle_color(int i, uint32_t color)
	{
		g_needle_memo[i].color = color;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames and the
	// random aim keep their 15 Hz batches. The frame matrix is rebuilt from the held-frame
	// camera while the timeline (its owner) is alive, into a local (never into 0x2259978).
	// ------------------------------------------------------------------
	static uint8_t g_skel_cur[SKEL_MAX];

	static void CreatureHeld(const Mat4x3 *frame, int32_t y, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		Mat4x3 mat_cur = CreatureMat();
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		if (m.midpoint) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		CreatureY() = y;
		CreatureDraw(frame);
		CreatureMat() = mat_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void RisePrimHeld(const Mat4x3 *frame, const Node24 *n, int32_t y, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		int32_t f0 = 682 * (m->c - 6), f = f0;
		if (m->c >= 6 && n->c != m->c) f = lerp_i(f0, 682 * (n->c - 6), num, den);
		RisePrimDraw(frame, n, y, m->c >= 6, f);
	}

	static void ShrinkPrimHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		int32_t f0 = shl32(4 - m->c, 10), f = f0;
		if (m->c < 4 && n->c != m->c && n->c < 4) f = lerp_i(f0, shl32(4 - n->c, 10), num, den);
		ShrinkPrimDraw(frame, n, lerp_i(m->v, n->s, num, den), m->c < 4, f);
	}

	static void SpinPrimHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		bool fade = m->c < 8 || m->c >= 14;
		int32_t f = SpinFade(m->c);
		if (n->c != m->c && ((m->c < 8 && n->c < 8) || m->c >= 14))
			f = lerp_i(f, SpinFade(n->c), num, den);
		SpinPrimDraw(frame, n, lerp_angle(m->v, n->a, num, den), fade, f);
	}

	static void DustHeld(const Mat4x3 *frame, int num, int den)
	{
		uint8_t *pool = DustPool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(scratch + 8, frame, sizeof(Mat4x3));
		*(uint32_t *)h = 0xCF0740;
		*(uint16_t *)(h + 0x24) = 0;
		GteSetLightMatrix(scratch + 8);
		GteSetBackColorFromTrans(scratch + 8);
		for (int i = 0; i < 100; i++)
		{
			const DustMemo &m = g_dust_memo[i];
			if (m.tick != g_real_tick) continue;
			const DustPuff *p = (const DustPuff *)(pool + 0x1C * i);
			int16_t pos[4];
			memcpy(pos, m.pos, 8);
			if ((p->alive & 1) && p->age != m.age) // moved on the last tick: half way to where it is now
				for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m.pos[k], p->pos[k], num, den);
			DustDrawOne(h, pos, p->size, m.age);
		}
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	static void NeedleHeld(int num, int den)
	{
		uint8_t *pool = NeedlePool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x90);
		uint8_t *T = (uint8_t *)FieldAlloc(0x68);
		NeedleTemplate(T);
		memcpy(scratch + 0x10, &Camera(), sizeof(Mat4x3));
		uint32_t cursor = Cursor();
		for (int i = 0; i < 800; i++)
		{
			const NeedleMemo &m = g_needle_memo[i];
			if (m.tick != g_real_tick) continue;
			const Needle *p = (const Needle *)(pool + 0x3C * i);
			int32_t pos[3] = { m.pos[0], m.pos[1], m.pos[2] };
			uint32_t color = m.color;
			if ((p->alive & 1) && p->age != m.age)
			{
				// the colour the next tick draws: fade-in step, or the next fade-out step
				int16_t a = p->age;
				uint32_t next = a < 4 ? NEEDLE_FADE_IN[a] : *(uint32_t *)(T + 0x54);
				if ((int16_t)(a + 1) >= p->steps) next = NEEDLE_FADE_OUT[p->fade];
				color = LerpColor(m.color, next, num, den);
				for (int k = 0; k < 3; k++) pos[k] = lerp_i(m.pos[k], p->pos[k], num, den);
			}
			NeedleProject(scratch, p->rot, pos, T);
			*(uint32_t *)(T + 0x10) = color;
			NeedleEmit(T, cursor);
		}
		Cursor() = cursor;
		FieldFree(0x68);
		FieldFree(0x90);
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
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &frame);
		else frame = Frame();
		int32_t y = CreatureY();
		if (g_creature.tick == g_real_tick) y = lerp_i(g_creature.y_drawn, g_creature.y_next, num, den);
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_CreatureTask: CreatureHeld(&frame, y, num, den); break;
			case ORIG_RisePrimTask: RisePrimHeld(&frame, n, y, num, den); break;
			case ORIG_ShrinkPrimTask: ShrinkPrimHeld(&frame, n, num, den); break;
			case ORIG_SpinPrimTask: SpinPrimHeld(&frame, n, num, den); break;
			case ORIG_DustTask: DustHeld(&frame, num, den); break;
			case ORIG_NeedleTask: NeedleHeld(num, den); break;
			}
		}
		Cursor() = cursor;
	}
}

	static void register_mag199_held()
	{
		mark_held(c199::ORIG_CreatureTask);
		mark_held(c199::ORIG_RisePrimTask);
		mark_held(c199::ORIG_DustTask);
		mark_held(c199::ORIG_ShrinkPrimTask);
		mark_held(c199::ORIG_SpinPrimTask);
		mark_held(c199::ORIG_NeedleTask);
		register_module_held(199, c199::HeldReady, c199::HeldFrame);
	}
}
