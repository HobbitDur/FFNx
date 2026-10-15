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

// 30 fps part of mag176_sand_storm_adamantoise.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag176_sand_storm_adamantoise.cpp; shared pieces in
// mag_common_held.h / fx_held.h).
//
// The funnels update their rings, then draw; the sand and landing-dust pools draw THEN update. A
// real tick logs, in queue order: each funnel draw (node, position, rings as drawn, wander offset,
// GTE state, Field_Alloc scratch under its draw block) and each pool's draw loop (its header at the
// first sprite, GTE state and every sprite as drawn). A held frame replays the log in the same
// order at tick + num/den:
//   funnel - the next tick's rings are predicted from the deterministic part of 0x5D0F30 (each ring
//            takes the previous ring's tilt, ring 0 the wander offset; parameters at counter + 1),
//            the random tilt / wander jitter (-64..63 / -256..255 per tick) is left out; ring
//            positions and radii half way, the position half way to the next one (the funnel's
//            own position code on the updated node at counter + 1), spin angle and fade half way.
//            Private copy of the funnel's packets; the funnel matrix is a local copy (the exe's
//            0xD45158 is not written).
//   sand / landing dust - every sprite drawn on the real tick, moved half way to its updated
//            state (x / y / z slide); the flipbook frame is lerped too (it changes at fraction 1
//            only).
// A funnel or sprite that did not advance (draw-only tick, the funnel's last tick, a finished
// sprite, or a slot reused by a new sprite) holds its drawn state; sprites spawned on the tick first
// appear on the next real tick. Not drawn by the module (hold, 15 Hz): the lifted targets' entity
// rotation / position / flags (the battle draws them). At fraction 0 a held frame inserts exactly
// the primitives of the real tick.

namespace ff8fx
{
namespace sandstorm176
{
	static const uint32_t ARENA = ARENA_HALF; // module packet arena per tick
	static const uint32_t SCRATCH = 0xA0;     // Field_Alloc scratch under the funnel's draw block
	static const uint32_t RINGS_BYTES = 17 * RING_SIZE;
	static const uint32_t PACKETS_BYTES = 0x5000;
	static const int MAX_SPRITES = 150;

	struct HeldItem
	{
		int kind;                   // 0 funnel, 1 sprite pool loop
		uint32_t node;              // live node
		uint8_t copy[0x24];         // node as drawn
		bool moving;                // the tick was not draw-only
		int16_t x, z;               // funnel position as drawn
		int16_t head[3];            // funnel: wander offset after the update (ring 0's next tilt)
		uint8_t rings[RINGS_BYTES]; // funnel: rings as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[0xB4];      // funnel: scratch under its draw block; pool: the header at the first sprite
		int n;
		uint8_t idx[MAX_SPRITES];   // pool index and the sprite as drawn
		uint8_t spr[MAX_SPRITES][0x18];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nitems = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first sprite draw
	static HeldItem g_items[12];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_rings[RINGS_BYTES];
	static uint8_t g_next_rings[RINGS_BYTES];
	static uint8_t g_held_packets[PACKETS_BYTES];

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

	static HeldItem *held_new_item(int kind, uint32_t node)
	{
		if (g_log_tick != g_real_tick || g_nitems >= (int)(sizeof(g_items) / sizeof(g_items[0]))) return nullptr;
		HeldItem &it = g_items[g_nitems++];
		it.kind = kind;
		it.node = node;
		memcpy(it.copy, (const void *)node, sizeof(it.copy));
		it.moving = !DrawOnly();
		it.n = 0;
		held_save_gte(it);
		return &it;
	}

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nitems = 0;
		g_packets = 0;
	}

	static void held_note_funnel(uint32_t f, int16_t x, int16_t z)
	{
		HeldItem *it = held_new_item(0, f);
		if (!it) return;
		it->x = x;
		it->z = z;
		memcpy(it->rings, (const void *)RingSel(), RINGS_BYTES);
		for (int m = 0; m < 3; m++) it->head[m] = S16(HeadSel(), 2 * m);
		memcpy(it->scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_sand(uint32_t task, uint32_t h, uint32_t e)
	{
		HeldItem *it = g_nitems > 0 && g_log_tick == g_real_tick ? &g_items[g_nitems - 1] : nullptr;
		if (!it || it->kind != 1 || it->node != task)
		{
			if (g_log_tick == g_real_tick && g_packets == 0) g_packets = PacketCursor();
			it = held_new_item(1, task);
			if (!it) return;
			memcpy(it->scratch, (const void *)h, 0xB4);
		}
		if (it->n >= MAX_SPRITES) return;
		it->idx[it->n] = (uint8_t)((e - POOL) / 0x18);
		memcpy(it->spr[it->n++], (const void *)e, 0x18);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the funnel's next rings: 0x5D0F30 without its random jitter
	static void FunnelPredict(uint32_t rings, const int16_t head[3], const FunnelParams &P)
	{
		uint8_t h[0x28];
		for (int32_t k = 16; k >= 0; k--)
		{
			const uint32_t ring = rings + (uint32_t)k * RING_SIZE;
			for (int m = 0; m < 3; m++)
				S16(ring, 8 + 2 * m) = k > 0 ? S16(ring, 8 + 2 * m - (int32_t)RING_SIZE) : head[m];
			RingPlace(ring, k, P, (uint32_t)h);
		}
	}

	static void HeldFunnel(const HeldItem &it, int num, int den)
	{
		const uint32_t f = (uint32_t)it.copy;
		const int16_t c = S16(f, 0xC);
		memcpy(g_held_rings, it.rings, RINGS_BYTES);
		int32_t angle = c * 100, alpha = FunnelAlpha(c);
		int16_t x = it.x, z = it.z;
		if (it.moving && c + 1 < 0x58)
		{
			// the next real tick: rings without the jitter, the position of the updated node
			uint8_t next[0x24];
			memcpy(next, it.copy, sizeof(next));
			S16((uint32_t)next, 0xC) = (int16_t)(c + 1);
			memcpy(g_next_rings, it.rings, RINGS_BYTES);
			FunnelPredict((uint32_t)g_next_rings, it.head, FunnelPhase(c + 1, it.node));
			int16_t nx, nz;
			FunnelPosition((uint32_t)next, &nx, &nz);
			for (int k = 0; k < 17; k++)
			{
				const uint32_t a = (uint32_t)g_held_rings + k * RING_SIZE, b = (uint32_t)g_next_rings + k * RING_SIZE;
				for (int m = 0; m < 3; m++) S16(a, 2 * m) = lerp16(S16(a, 2 * m), S16(b, 2 * m), num, den);
				S32(a, 0x10) = lerp_i(S32(a, 0x10), S32(b, 0x10), num, den);
			}
			angle = lerp_i(c * 100, (c + 1) * 100, num, den);
			alpha = lerp_i(FunnelAlpha(c), FunnelAlpha(c + 1), num, den);
			x = lerp16(x, nx, num, den);
			z = lerp16(z, nz, num, den);
		}
		if (S16(f, 0xE) != 0) angle = -angle;
		// the funnel matrix at the in-between position (a local copy), then the real draw's GTE
		// state with that rotation / translation
		Mat4x3 base = *(const Mat4x3 *)FUNNEL_Matrix, m;
		base.t[0] = x;
		base.t[1] = 0;
		base.t[2] = z;
		Compose(CAMERA, (uint32_t)&base, (uint32_t)&m);
		held_load_gte(it);
		SetRotW((uint32_t)&m);
		SetTransW((uint32_t)&m);
		memcpy(var<uint8_t *>(0x1D999C4), it.scratch, SCRATCH);
		memcpy(g_held_packets, (const void *)(S16(f, 0xE) != 0 ? PACKETS_B : PACKETS_A), PACKETS_BYTES);
		FunnelDraw((uint32_t)g_held_rings, (uint32_t)g_held_packets, angle, alpha);
	}

	static void HeldSand(const HeldItem &it, int num, int den)
	{
		held_load_gte(it);
		const uint32_t h = Alloc(0xB4);
		memcpy((void *)h, it.scratch, 0xB4);
		for (int i = 0; i < it.n; i++)
		{
			uint8_t s[0x18];
			memcpy(s, it.spr[i], sizeof(s));
			const uint32_t a = (uint32_t)s, live = POOL + (uint32_t)it.idx[i] * 0x18;
			if (it.moving && U32(live, 0) == U32(a, 0) && S16(live, 4) == (int16_t)(S16(a, 4) + 1))
			{
				S16(a, 4) = lerp16(S16(a, 4), S16(live, 4), num, den);
				for (int k = 0; k < 3; k++) S16(a, 8 + 2 * k) = lerp16(S16(a, 8 + 2 * k), S16(live, 8 + 2 * k), num, den);
			}
			SandDraw(h, a);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nitems; i++)
		{
			const HeldItem &it = g_items[i];
			if (it.kind == 0) HeldFunnel(it, num, den);
			else HeldSand(it, num, den);
		}
	}
}

	static void register_mag176_held()
	{
		mark_held(sandstorm176::ORIG_Funnel);
		mark_held(sandstorm176::ORIG_Sand);
		mark_held(sandstorm176::ORIG_Landing);
		register_module_held(176, sandstorm176::HeldReady, sandstorm176::HeldFrame);
	}
}
