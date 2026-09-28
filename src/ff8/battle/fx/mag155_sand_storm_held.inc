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

// 30 fps part of mag155_sand_storm.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag155_sand_storm.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick logs, in queue order: the wave's angle (the
// funnel vertices it wrote), each funnel draw (node, position, GTE state, Field_Alloc scratch) and
// each dust pool's draw loop (its header scratch, GTE state and every sprite as drawn). A held
// frame replays the log in the same order at tick + num/den:
//   wave   - the funnel vertices recomputed (vanilla wave code) at the in-between angle, into a
//            private buffer;
//   funnel - the two draws at the in-between spin, height scale and position (the next position is
//            the funnel's own position code run for the next counter on the updated node: the path
//            lerp / sway before tick 30, the spline table after), fade lerped while the header flags
//            stay the same (fade in 0..5, out 52..), the flag switch (tick 5 -> 6) at fraction 1;
//   dust   - every sprite drawn on the real tick, moved half way to its updated state (A: x / z
//            slide; B: the orbiting position half way along its N -> N+1 chord, sub-pixel from the
//            arc, which can swing outside the screen-space span when the orbit turns along the view
//            direction); the flipbook frame is lerped too (it changes at fraction 1 only).
// A task or sprite that did not advance (draw-only tick, ended task, finished sprite, or a slot
// reused by a new sprite) holds its drawn state; sprites spawned on the tick first appear on the
// next real tick. At fraction 0 a held frame inserts exactly the primitives of the real tick.

namespace ff8fx
{
namespace sandstorm155
{
	static const uint32_t ARENA = 0x8000;   // module packet arena per tick
	static const uint32_t SCRATCH = 0x60;   // Field_Alloc scratch under the funnel's prim header

	struct HeldItem
	{
		int kind;                   // 0 wave, 1 funnel, 2 dust A, 3 dust B
		const void *node;           // live node (wave / funnel)
		int16_t angle, radius, counter; // wave
		FunnelNode funnel;          // funnel node as drawn
		uint32_t x, z;              // funnel position words as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[0xB4];      // funnel: scratch under the header; dust: the header at the first sprite
		int ndust;
		uint8_t idx[100];           // dust: pool index and the sprite as drawn
		Dust dust[100];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nitems = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static HeldItem g_items[8];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_verts[0x130 * 8]; // funnel vertices at the in-between wave angle

	static void held_save_gte(HeldItem &it)
	{
		memcpy(it.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(it.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}

	static void held_load_gte(const HeldItem &it)
	{
		memcpy((void *)GTE_DATA, it.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, it.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static HeldItem *held_new_item(int kind)
	{
		if (g_log_tick != g_real_tick || g_nitems >= (int)(sizeof(g_items) / sizeof(g_items[0]))) return nullptr;
		if (kind != 0 && g_packets == 0) g_packets = PacketCursor();
		HeldItem &it = g_items[g_nitems++];
		it.kind = kind;
		it.ndust = 0;
		return &it;
	}

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nitems = 0;
		g_packets = 0;
	}

	static void held_note_wave(const WaveNode *w)
	{
		HeldItem *it = held_new_item(0);
		if (!it) return;
		it->node = w;
		it->angle = w->angle;
		it->radius = w->radius;
		it->counter = w->counter;
	}

	static void held_note_funnel(const FunnelNode *f)
	{
		HeldItem *it = held_new_item(1);
		if (!it) return;
		it->node = f;
		it->funnel = *f;
		it->x = PosX();
		it->z = PosZ();
		held_save_gte(*it);
		memcpy(it->scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_dust(int pool, const uint8_t *h, const Dust *d)
	{
		HeldItem *it = g_nitems > 0 && g_log_tick == g_real_tick ? &g_items[g_nitems - 1] : nullptr;
		if (!it || it->kind != 2 + pool)
		{
			it = held_new_item(2 + pool);
			if (!it) return;
			held_save_gte(*it);
			memcpy(it->scratch, h, 0xB4);
		}
		if (it->ndust >= 100) return;
		it->idx[it->ndust] = (uint8_t)(d - (const Dust *)(pool ? POOL_B : POOL_A));
		it->dust[it->ndust++] = *d;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFunnel(const HeldItem &it, uint32_t verts, int num, int den)
	{
		const FunnelNode &a = it.funnel;
		const FunnelNode *live = (const FunnelNode *)it.node;
		FunnelNode b = a;
		uint32_t bx = it.x, bz = it.z;
		if (live->counter == (int16_t)(a.counter + 1) && live->counter < 0x3C)
		{
			// the next real tick: the updated node at the next position
			b = *live;
			bx = PosX();
			bz = PosZ();
			FunnelPosition(&b, b.counter, &bx, &bz);
		}
		int32_t fa, fb;
		uint32_t la, lb;
		FunnelFade(a.counter, &fa, &la);
		FunnelFade(b.counter, &fb, &lb);
		if (la == lb) fa = lerp_i(fa, fb, num, den);
		else if (num >= den)
		{
			// the fade mode switches (end of the fade-in): at the next tick only
			fa = fb;
			la = lb;
		}
		const int16_t angle = lerp_angle(a.angle, b.angle, num, den);
		const int16_t height = lerp_angle(a.height, b.height, num, den);
		const int16_t x = lerp_angle((int16_t)it.x, (int16_t)bx, num, den);
		const int16_t z = lerp_angle((int16_t)it.z, (int16_t)bz, num, den);
		held_load_gte(it);
		memcpy(var<uint8_t *>(0x1D999C4), it.scratch, SCRATCH);
		FunnelDraw(angle, height, x, z, verts, fa, la);
	}

	static void HeldDust(const HeldItem &it, int num, int den)
	{
		const bool b = it.kind == 3;
		const Dust *pool = (const Dust *)(b ? POOL_B : POOL_A);
		held_load_gte(it);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(h, it.scratch, 0xB4);
		for (int i = 0; i < it.ndust; i++)
		{
			const Dust &a = it.dust[i];
			const Dust &n = pool[it.idx[i]];
			const bool next = (n.used & 1) && n.frame == (int16_t)(a.frame + 1);
			const int16_t frame = next ? lerp_angle(a.frame, n.frame, num, den) : a.frame;
			if (b)
			{
				// the orbit position half way along its chord (N -> N+1)
				int16_t pa[4], pn[4], p[4];
				DustPosB(&a, pa);
				if (next) DustPosB(&n, pn);
				else memcpy(pn, pa, 8);
				for (int k = 0; k < 4; k++) p[k] = lerp_angle(pa[k], pn[k], num, den);
				DustDraw(h, (int16_t)(frame >> 1), p, a.scale);
			}
			else
			{
				int16_t p[4] = { a.pos[0], a.pos[1], a.pos[2], a.pos[3] };
				if (next)
				{
					p[0] = lerp_angle(a.pos[0], n.pos[0], num, den);
					p[2] = lerp_angle(a.pos[2], n.pos[2], num, den);
				}
				DustDraw(h, frame, p, a.scale);
			}
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		uint32_t verts = VERTS;
		for (int i = 0; i < g_nitems; i++)
		{
			const HeldItem &it = g_items[i];
			if (it.kind == 0)
			{
				const WaveNode *w = (const WaveNode *)it.node;
				int16_t angle = it.angle;
				if (w->counter == (int16_t)(it.counter + 1) && w->counter < 0x3C) angle = lerp_angle(it.angle, w->angle, num, den);
				WaveVertices(angle, it.radius, (uint32_t)g_held_verts - VERTS);
				verts = (uint32_t)g_held_verts;
			}
			else if (it.kind == 1) HeldFunnel(it, verts, num, den);
			else HeldDust(it, num, den);
		}
	}
}

	static void register_mag155_held()
	{
		mark_held(sandstorm155::ORIG_Funnel);
		mark_held(sandstorm155::ORIG_DustA);
		mark_held(sandstorm155::ORIG_DustB);
		register_module_held(155, sandstorm155::HeldReady, sandstorm155::HeldFrame);
	}
}
