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

// 30 fps part of mag209_sorceress_spawn.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag209_sorceress_spawn.cpp)
//
// Held frames (30 fps). The real tick's draws are drawn again in the same order (a draw list kept on the real tick,
// so a task that ended on that tick still holds its last picture), half way to the next tick's state: the prims at
// their next angle / scale (fades and texture scrolls of the tick kept, the next tick's state at fraction 1), the
// screen's colour half way to the next tick's, the grid at the ripple half way to the next step, the stage copy into
// VRAM as on the tick. Each draw sees the scratch words and GTE data registers its real draw saw (never-written header
// words, leftover SXY words), the packet buffer the tick's never-written packet words. Engine-side state of the screen task (background / ambient colours, the screen flash, the
// VRAM clear) keeps the tick's values (no held write). Stage-group skeletons rebuilt by the VRAM copy are put back; the
// renderer gets a private vertex scratch, the grid private copies of the points and of the cells' quads.

namespace ff8fx
{
namespace s209
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false;
	static uint32_t g_tick_cursor = 0; // the packet cursor at the tick's start

	// (scr: the Field_Alloc scratch words the task's draw saw, its header's never-written words included)
	// (gte / gtc: the GTE data and control registers at the start of the task: the renderers read leftover words)
	struct Entry { const Node24 *node; Node24 m; uint8_t parity; uint8_t scr[0x100]; uint8_t gte[0x70]; uint8_t gtc[0xF0]; };
	static uint8_t g_gte_start[0x70], g_gtc_start[0xF0];
	static void held_note_gte()
	{
		memcpy(g_gte_start, (const void *)0x1CA8A10, sizeof(g_gte_start));
		memcpy(g_gtc_start, (const void *)0x1CA9210, sizeof(g_gtc_start));
	}
	static Entry g_list[128];
	static int g_nlist = 0;

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
		g_tick_cursor = Cursor();
		g_nlist = 0;
	}

	static Entry *ListAdd(const Node24 *n)
	{
		if (g_ported_tick != g_real_tick || g_nlist >= 128) return nullptr;
		Entry *e = &g_list[g_nlist++];
		e->node = n;
		e->m = *n;
		e->parity = var<uint8_t>(0x1D96A80);
		memcpy(e->scr, (const void *)var<uint32_t>(0x1D999C4), sizeof(e->scr));
		memcpy(e->gte, g_gte_start, sizeof(e->gte));
		memcpy(e->gtc, g_gtc_start, sizeof(e->gtc));
		return e;
	}

	static void MemoNode(const Node24 *n) { ListAdd(n); }
	static void held_note_screen(const Node24 *n) { ListAdd(n); }

	static const Node24 *Current(const Entry &e)
	{
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if (t == &e.node->hdr) return (t->flags & 1) && t->func == e.m.hdr.func ? e.node : nullptr;
		return nullptr;
	}

	static bool Moved(const Entry &e, const Node24 *cur) { return cur && !g_tick_paused && cur->c == (int16_t)(e.m.c + 1); }
	static int16_t Lerp16(int32_t a, int32_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// the scratch words of the task's real draw, back before its held draw
	static void Scratch(const Entry &e)
	{
		memcpy((void *)var<uint32_t>(0x1D999C4), e.scr, sizeof(e.scr));
		memcpy((void *)0x1CA8A10, e.gte, sizeof(e.gte)); // (the gate puts the GTE back after the held frame)
		memcpy((void *)0x1CA9210, e.gtc, sizeof(e.gtc));
	}

	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8) r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	// ---- prims: half way to the next tick's angle / scales (the task's own deterministic update); fade and
	// texture scroll steps of the tick kept, the next tick's state as a whole at fraction 1 ----
	static void PrimHeld(const Entry &e, int num, int den)
	{
		const Node24 *cur = Current(e);
		const bool mv = Moved(e, cur);
		const Node24 *m = (mv && num >= den) ? cur : &e.m;
		const Node24 *o = &e.m;
		const int16_t a = mv ? Lerp16(o->a18, cur->a18, num, den) : o->a18;
		const int32_t s1 = mv ? lerp_i(o->s1C, cur->s1C, num, den) : o->s1C;
		const int32_t s2 = mv ? lerp_i(o->s20, cur->s20, num, den) : o->s20;
		Scratch(e);
		switch ((uint32_t)o->hdr.func)
		{
		case ORIG_PrimATask: PrimADraw(m, s1); break;
		case ORIG_PrimBTask: PrimBDraw(m, a, s1, s2); break;
		case ORIG_PrimCTask: PrimCDraw(m, m->c); break; // (scale constant, scroll in whole steps)
		case ORIG_PrimDTask: PrimDDraw(m, a, s1); break;
		case ORIG_PrimETask: PrimEDraw(m, a, s1); break;
		default: break;
		}
	}

	// ---- screen task ----
	static uint8_t g_grid[0x1C * 0x29 * 0xC];
	static uint8_t g_cells[0x1B * 0x28 * 0x60];
	static int16_t g_znext[0x1C * 0x29];
	static uint8_t g_vertex_scratch[0x20000];
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	static uint8_t g_skel_save[4][SKEL_MAX];

	// the ripple depths of step i into z (MAG_209_sub_6380F0 on a private output)
	static void RippleInto(int32_t i, int32_t n, int16_t *z)
	{
		int16_t *s = (int16_t *)FieldAlloc(0xC);
		GetEffectSpawnPosition(Caster(), 0xF1, 0, s);
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(s);
		GteRTPS();
		GteReadSXY2(s + 4);
		s[4] = (int16_t)((int16_t)(s[4] >> 3) - 0xA4);
		s[5] = (int16_t)((int16_t)(s[5] >> 3) - 0x70);
		const int32_t phase = 385 * i;
		const int32_t amp = (500 * ComputeSin(shl32(i, 11) / n)) >> 12;
		const int32_t range = 0x258 - 9 * i;
		const uint8_t *g = (const uint8_t *)GRID;
		for (int32_t k = 0; k < 0x1C * 0x29; k++, g += 0xC)
		{
			z[k] = *(const int16_t *)(g + 8);
			if (*(const uint8_t *)(g + 0xA) & 3) continue;
			const int32_t dx = *(const int16_t *)(g + 4) - s[4];
			const int32_t dy = *(const int16_t *)(g + 6) - s[5];
			const int32_t d = Sqrt(dy * dy + dx * dx);
			const int32_t c = ComputeCos(96 * d - phase) - 0x1000;
			const int32_t f = (range - d) * amp / range;
			z[k] = (int16_t)((c * f) >> 13);
		}
		FieldFree(0xC);
	}

	// the grid of the tick (step i), its depths half way to step i + 1 when the next tick ripples it too
	static void GridHeld(int32_t i, bool next, int parity, uint32_t colour, int num, int den)
	{
		memcpy(g_grid, (const void *)GRID, sizeof(g_grid));
		if (next)
		{
			RippleInto(i + 1, 0x3C, g_znext);
			for (int32_t k = 0; k < 0x1C * 0x29; k++)
			{
				int16_t *z = (int16_t *)(g_grid + 0xC * k + 8);
				*z = Lerp16(*z, g_znext[k], num, den);
			}
		}
		Mat4x3 m;
		ResetRotation(&m);
		m.t[0] = 4;
		m.t[1] = 4;
		m.t[2] = 0x800;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		SetProjection(0x800);
		for (uint8_t *g = g_grid; g < g_grid + sizeof(g_grid); g += 0xC)
		{
			GteLoadV0(g + 4);
			GteRTPS();
			GteReadSXY2(g);
			*(int16_t *)g = (int16_t)(*(int16_t *)g >> 3);
			*(int16_t *)(g + 2) = (int16_t)(*(int16_t *)(g + 2) >> 3);
		}
		SetProjection(var<int16_t>(0x1D8E038));
		memcpy(g_cells, Cells(), sizeof(g_cells));
		const uint32_t col = colour | 0x2C000000;
		uint8_t *cell = g_cells;
		for (int32_t k = 0; k < 0x1B * 0x28; k++, cell += 0x60)
		{
			uint32_t *q = (uint32_t *)(cell + 0x18 + 0x28 * parity);
			const uint32_t *pt = (const uint32_t *)cell;
			q[-1] = col;
			for (int v = 0; v < 4; v++) q[2 * v] = *(const uint32_t *)(g_grid + (pt[v] - GRID));
			InsertPrimAltViewport(OTBase() + 0x4068, q - 2);
		}
	}

	// MAG_209_sub_637D30 without the VRAM clear (the draw environment clears the area too), on a private vertex
	// scratch, the stage-group skeletons put back
	static void StageToVramHeld()
	{
		uint8_t *sk[4];
		uint32_t size[4];
		for (int i = 0; i < 4; i++)
		{
			uint8_t *p = (uint8_t *)(0x1D989A4 + 0x2C * i);
			uint8_t *com = *(uint8_t **)(p + 4);
			sk[i] = com ? *(uint8_t **)com : nullptr;
			size[i] = sk[i] ? 16 + 48 * (uint32_t)sk[i][0] : 0;
			if (size[i] > SKEL_MAX) sk[i] = nullptr;
			if (sk[i]) memcpy(g_skel_save[i], sk[i], size[i]);
		}
		int32_t lo = 0x1121;
		uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
		*(int16_t *)(h + 0x14) = 0;
		*(int16_t *)(h + 0x16) = 0;
		*(int16_t *)(h + 0x1A) = 0xD8;
		*(uint32_t *)(h + 0x10) = var<uint32_t>(0x1D969A8);
		*(int16_t *)(h + 0x18) = 0x140;
		*(uint32_t *)(h + 4) = (uint32_t)g_vertex_scratch;
		*(uint32_t *)(h + 0x20) = 0xFFFFFFFF;
		for (int i = 0; i < 4; i++)
		{
			if (!(var<uint8_t>(0x1D98991 + 0x2C * i) & 1)) continue;
			int32_t o = var<int32_t>(0xE2B4F8 + 8 * i);
			if (o < lo) lo = o;
		}
		DrawEnvAt(0, 0, lo);
		for (int i = 0; i < 4; i++)
		{
			uint8_t *p = (uint8_t *)(0x1D989A4 + 0x2C * i);
			const int32_t *tb = (const int32_t *)(0xE2B4F8 + 8 * i);
			BuildBoneMatricesFromPose(p);
			ComputeBonesWorldMatrices(p, &Camera());
			if (!(p[-0x13] & 1)) continue;
			*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(p + 0x14);
			*(uint16_t *)(h + 0x24) = *(const uint16_t *)(p - 0x12);
			Cursor() = RenderGeometry(p - 0x10, h, OTBase() + 4 * (uint32_t)tb[0], tb[1], Cursor());
		}
		DrawEnvAt(0x240, 0x100, 0x1121);
		FieldFree(0x2C);
		for (int i = 0; i < 4; i++)
			if (sk[i]) memcpy(sk[i], g_skel_save[i], size[i]);
	}

	static int32_t FadeW(int32_t c) { return shl32(c - 0x52, 12) / 16; }

	// the grid colour of the fade to the new stage (0x52 .. 0x61): the tick's colour through GTE DPCS
	static uint32_t FadeColour(int32_t c)
	{
		uint32_t g = ScreenColour(c) | 0x2000000, rgbc = g;
		GteSetFarColor(0, 0, 0);
		GteLoadRGBC(&rgbc);
		GteSetIR0(FadeW(c));
		GteDPCS();
		GteStoreRGB2(&g);
		return g;
	}

	// the tick's screen draw with its colour half way to the next tick's, the grid depths half way to the next
	// ripple step, when the next tick draws the same way (else the tick's)
	static void ScreenHeld(const Entry &e, int num, int den)
	{
		const Node24 *cur = Current(e);
		const int32_t c = e.m.c;
		auto branch = [](int32_t k) { return k < 0x24 ? 0 : k < 0x26 ? 1 : k < 0x52 ? 2 : k < 0x62 ? 3 : 4; };
		const bool mv = Moved(e, cur) && c + 1 < 0x64 && branch(c) == branch(c + 1);
		Scratch(e);
		if (c < 0x26)
		{
			uint32_t col = ScreenColour(c);
			if (mv) col = LerpColour(col, ScreenColour(c + 1), num, den);
			Tiles(col | 0x64000000);
			if (c < 0x24) StageToVramHeld();
		}
		else if (c < 0x52)
		{
			uint32_t col = ScreenColour(c);
			if (mv) col = LerpColour(col, ScreenColour(c + 1), num, den);
			GridHeld(c - 0x26, mv, e.parity, col, num, den);
		}
		else if (c < 0x62)
		{
			uint32_t col = FadeColour(c);
			if (mv) col = LerpColour(col, FadeColour(c + 1), num, den);
			GridHeld(c - 0x26, mv, e.parity, col, num, den);
			StageToVramHeld();
		}
		else StageToVramHeld();
	}

	static uint8_t g_held_packets[0x40000];
	static uint8_t g_falloc_save[0x1000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the real tick's draws in order; packets go to a private buffer (the engine cursor battle_texture_data_ptr_1D8E054,
	// redirected and put back), the Field_Alloc scratch words put back
	static void HeldFrame(int num, int den)
	{
		const uint32_t cursor = Cursor();
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		memcpy(g_falloc_save, (const void *)fa, sizeof(g_falloc_save));
		uint8_t c18[8];
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18)); // (RenderGeometry's last-draw words)
		if (g_tick_cursor) memcpy(g_held_packets, (const void *)g_tick_cursor, sizeof(g_held_packets)); // (never-written packet words: the tick's)
		Cursor() = (uint32_t)g_held_packets;
		for (int i = 0; i < g_nlist; i++)
		{
			const Entry &e = g_list[i];
			if ((uint32_t)e.m.hdr.func == ORIG_ScreenTask) ScreenHeld(e, num, den);
			else PrimHeld(e, num, den);
		}
		Cursor() = cursor;
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)fa, g_falloc_save, sizeof(g_falloc_save));
		var<uint32_t>(0x1D999C4) = fa;
	}
}

	static void register_mag209_held()
	{
		mark_held(s209::ORIG_ScreenTask);
		mark_held(s209::ORIG_PrimATask);
		mark_held(s209::ORIG_PrimBTask);
		mark_held(s209::ORIG_PrimCTask);
		mark_held(s209::ORIG_PrimDTask);
		mark_held(s209::ORIG_PrimETask);
		register_module_held(209, s209::HeldReady, s209::HeldFrame);
	}
}
