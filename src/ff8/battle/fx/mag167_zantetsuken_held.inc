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

// 30 fps part of mag167_zantetsuken.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag167_zantetsuken.cpp)
//
// Held frames (30 fps). Everything the last real tick drew is drawn again, in the same order (a
// draw list kept on the real tick, so a task that ended on that tick still holds its last picture),
// half way between that state and the state the next tick will draw: sparks at their integrated
// next position and size, the screen prims at their next scale, the target slices at their next
// offset and fade. Flipbook frames (stepping at fraction 1 only), spawns and the blade trail (a
// spline through a 15 Hz history) keep their 15 Hz steps; the trail is redrawn from the held-frame
// camera. Entity skeletons and colours touched by a slice draw are put back, as are the Field_Alloc
// scratch words; the clipped renderer gets a private vertex scratch.

namespace ff8fx
{
namespace z167
{
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false; // the 0x201 flags were set on that tick (nothing advanced)
	static uint32_t g_arena = 0;       // the packet arena half the tick draws into

	// the draws of the real tick, in order: the node as drawn (a copy) and where it lives
	struct Entry { const Node24 *node; Node24 m; int trail; };
	static Entry g_list[64];
	static int g_nlist = 0;

	struct TrailMemo { bool drawn; uint8_t a[0x100], b[0x100]; };
	static TrailMemo g_trail_memo[4];
	static int g_ntrail = 0;

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
		g_arena = Cursor();
		g_nlist = 0;
		g_ntrail = 0;
	}

	static Entry *ListAdd(const Node24 *n)
	{
		if (g_ported_tick != g_real_tick || g_nlist >= 64) return nullptr;
		Entry *e = &g_list[g_nlist++];
		e->node = n;
		e->m = *n;
		e->trail = -1;
		return e;
	}

	static int16_t Lerp16(int32_t a, int32_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	static void MemoNode(const Node24 *n) { ListAdd(n); }

	// spark records as drawn (before their update); the spark task's draw goes into the list once
	struct PoolMemo { uint32_t tick; Rec r; bool died; };
	static PoolMemo g_pool_memo[100];
	static uint32_t g_spark_listed = 0xFFFFFFFF;
	static PoolMemo *MemoRecord(const Node24 *n, int i, const Rec *r)
	{
		if (g_spark_listed != g_real_tick)
		{
			g_spark_listed = g_real_tick;
			ListAdd(n);
		}
		PoolMemo *m = &g_pool_memo[i];
		m->tick = g_real_tick;
		m->r = *r;
		m->died = false;
		return m;
	}

	static void MemoDied(PoolMemo *pm) { if (pm) pm->died = true; }

	static TrailMemo *held_trail_put(const Node24 *n)
	{
		Entry *e = ListAdd(n);
		if (!e || g_ntrail >= 4) return nullptr;
		e->trail = g_ntrail;
		TrailMemo *m = &g_trail_memo[g_ntrail++];
		m->drawn = false;
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

	// the node as it is now (after the real tick), or nullptr when its task ended
	static const Node24 *Current(const Entry &e)
	{
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if (t == &e.node->hdr) return (t->flags & 1) && t->func == e.m.hdr.func ? e.node : nullptr;
		return nullptr;
	}

	static bool Moved(const Entry &e, const Node24 *cur) { return cur && !g_tick_paused && cur->c == (int16_t)(e.m.c + 1); }

	// ---- held draws ----
	static uint8_t g_vertex_scratch[0x20000];
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	static uint8_t g_ent_head[0x30];
	static uint8_t g_skel_save[2][SKEL_MAX];

	// sparks: each record drawn on the last tick at the midpoint of its next update; the flipbook
	// frame of the next tick only at fraction 1 (a spark whose flipbook ended holds its last picture
	// until then)
	static void SparkHeld(int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		SparkSetup(h, s);
		for (int i = 0; i < 100; i++)
		{
			const PoolMemo &mm = g_pool_memo[i];
			if (mm.tick != g_real_tick) continue;
			if (mm.died && num >= den) continue; // gone on the next tick (its flipbook ended): not in its picture
			Rec t = mm.r;
			const bool step = !mm.died && !g_tick_paused;
			if (step) Integrate(&t);
			int16_t pos[4] = { Lerp16(mm.r.x, t.x, num, den), Lerp16(mm.r.y, t.y, num, den), Lerp16(mm.r.z, t.z, num, den), mm.r.w0E };
			int16_t frame = (int16_t)(mm.r.age + (step && num >= den ? 1 : 0));
			SparkDrawOne(h, pos, Lerp16(mm.r.size, t.size, num, den), frame);
		}
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void TrailHeld(const Entry &e)
	{
		if (e.trail < 0 || !g_trail_memo[e.trail].drawn) return; // the trail grows in 15 Hz steps: the one drawn, from the held camera
		const TrailMemo &m = g_trail_memo[e.trail];
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		TrailDraw((uint32_t)m.a, (uint32_t)m.b, s);
		FieldFree(0x40);
	}

	static void PrimHeld(const Entry &e, bool b, int num, int den)
	{
		const Node24 *cur = Current(e);
		const Node24 *m = &e.m;
		int32_t sx = Moved(e, cur) ? lerp_i(m->s1C, cur->s1C, num, den) : m->s1C;
		if (b) PrimDraw(m, 0xD7042C, 0, sx, sx);
		else PrimDraw(m, 0xD6FA7C, 0x8AC, sx, m->s20);
	}

	static uint8_t *AnimSkeleton(uint8_t *anim_header, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(anim_header + 4);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	static void SliceHeld(const Entry &e, int num, int den)
	{
		const Node24 *cur = Current(e);
		const Node24 *m = &e.m;
		bool mv = Moved(e, cur) && m->c + 1 < 0x28; // the next tick draws again
		int32_t off = mv ? lerp_i(m->s1C, cur->s1C, num, den) : m->s1C;
		int32_t w = -1;
		if (m->c >= 0x20)
		{
			w = SliceFadeW(m->c);
			if (mv) w = lerp_i(w, SliceFadeW(m->c + 1), num, den);
		}
		uint8_t *ent = Entity((uint32_t)(int32_t)m->e);
		// the draw recolours the target and rebuilds its (and its attached model's) local bone
		// matrices: put them back
		uint32_t size = 0, size_sub = 0;
		uint8_t *sk = AnimSkeleton(ent + 0x60, &size);
		if (!sk) return;
		uint8_t *sub = *(uint8_t **)(ent + 0x78);
		const bool has_sub = sub && !(ent[1] & 2);
		uint8_t *sk_sub = has_sub ? AnimSkeleton(sub, &size_sub) : nullptr;
		if (has_sub && !sk_sub) return;
		memcpy(g_ent_head, ent, sizeof(g_ent_head));
		memcpy(g_skel_save[0], sk, size);
		if (sk_sub) memcpy(g_skel_save[1], sk_sub, size_sub);
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
		SliceDraw(h, m, ent, off, w);
		FieldFree(0x1D0);
		if (sk_sub) memcpy(sk_sub, g_skel_save[1], size_sub);
		memcpy(sk, g_skel_save[0], size);
		memcpy(ent, g_ent_head, sizeof(g_ent_head));
	}

	static uint8_t g_held_packets[0x8000];
	static uint8_t g_held_packets_engine[0x40000];
	static uint8_t g_falloc_save[0x1000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the real tick's draws in order; packets go to private buffers (the module cursor 0x23BF030,
	// its buffer starting as a copy of the tick's arena half so that never-written pad words match,
	// and the engine cursor battle_texture_data_ptr_1D8E054 of the slices), redirected and put back
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor(), ecursor = EngineCursor();
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		memcpy(g_falloc_save, (const void *)fa, sizeof(g_falloc_save));
		if (g_arena) memcpy(g_held_packets, (const void *)g_arena, sizeof(g_held_packets));
		Cursor() = (uint32_t)g_held_packets;
		EngineCursor() = (uint32_t)g_held_packets_engine;
		g_vertex_scratch_override = (uint32_t)g_vertex_scratch;
		for (int i = 0; i < g_nlist; i++)
		{
			const Entry &e = g_list[i];
			switch ((uint32_t)e.m.hdr.func)
			{
			case ORIG_SparkTask: SparkHeld(num, den); break;
			case ORIG_TrailTask: TrailHeld(e); break;
			case ORIG_PrimATask: PrimHeld(e, false, num, den); break;
			case ORIG_PrimBTask: PrimHeld(e, true, num, den); break;
			case ORIG_SliceTask: SliceHeld(e, num, den); break;
			default: break;
			}
		}
		g_vertex_scratch_override = 0;
		EngineCursor() = ecursor;
		Cursor() = cursor;
		memcpy((void *)fa, g_falloc_save, sizeof(g_falloc_save));
		var<uint32_t>(0x1D999C4) = fa;
	}
}

	static void register_mag167_held()
	{
		mark_held(z167::ORIG_SparkTask);
		mark_held(z167::ORIG_TrailTask);
		mark_held(z167::ORIG_PrimATask);
		mark_held(z167::ORIG_PrimBTask);
		mark_held(z167::ORIG_SliceTask);
		register_module_held(167, z167::HeldReady, z167::HeldFrame);
	}
}
