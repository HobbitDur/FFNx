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

// 30 fps part of mag142_fira.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag142_fira.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Spark        - position half way along its velocity, size half way to the next shrink
//                  step; flipbook frame (9 - life / 2) holds.
//   Ember        - height half way along its rising velocity, size half way; frame holds.
//   FlameColumn  - spin and width half way; fade (+0x0C) half way inside a fade ramp.
//   Burst        - spin and height half way; fade half way inside the fade-out ramp.
//   Trail        - size half way; fade half way inside the fade-out ramp; table value holds.
//   ShockRing    - fixed place and size; colour half way inside its fade-in / fade-out ramps.
// The projectile and the directors draw nothing. In the draw-only mode everything holds.

namespace ff8fx
{
namespace fira142
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<256> g_draws;
	static uint8_t g_held_packets[0x20000];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
	}

	static void held_note_draw(uint32_t fn, const void *node) { g_draws.add(fn, node, PacketCursor()); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	// a prim-model part half way to its next state: moved fields interpolated, header rebuilt on
	// the real draw's scratch bytes, fade interpolated when the next header keeps the same flags
	static void ModelHeld(const DrawRec &r, int num, int den, bool moving, void (*matrix)(const ModelNode *),
		void (*header)(uint8_t *, int16_t), void (*update)(ModelNode *))
	{
		ModelNode a, b;
		memcpy(&a, r.node, sizeof(a));
		b = a;
		if (moving && update) update(&b);
		ModelNode m = a;
		m.yaw = lerp16(a.yaw, b.yaw, num, den);
		m.scale = lerp16(a.scale, b.scale, num, den);
		m.scale_y = lerp16(a.scale_y, b.scale_y, num, den);
		matrix(&m);
		uint8_t *h = AllocHeader(0x58);
		header(h, a.counter);
		if (moving && update)
		{
			uint8_t next[0x58];
			memcpy(next, r.hdr, sizeof(next));
			header(next, b.counter);
			LerpPrimFade(h, next, num, den);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// shock ring: sprite sequence on a plane; colour (+0x1C..+0x1E with +0x24 flag 4) half way
	// when the next tick's header uses the colour too
	static void ShockRingHeld(const DrawRec &r, int num, int den, bool moving)
	{
		ModelNode a;
		memcpy(&a, r.node, sizeof(a));
		ShockRingMatrix(&a);
		uint8_t *h = AllocHeader(0xB4);
		ShockRingHeader(h, a.counter);
		if (moving)
		{
			uint8_t next[0xB4];
			memcpy(next, r.hdr, sizeof(next));
			ShockRingHeader(next, (int16_t)(a.counter + 1));
			if (*(uint16_t *)(h + 0x24) == 4 && *(uint16_t *)(next + 0x24) == 4)
				for (int k = 0x1C; k <= 0x1E; k++) h[k] = (uint8_t)lerp_i(h[k], next[k], num, den);
		}
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		bool moving = !DrawOnly();
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			switch (r.fn)
			{
			case ORIG_SparkTask:
			{
				SparkNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				if (moving) SparkUpdate(&b);
				SparkNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				m.scale = lerp16(a.scale, b.scale, num, den);
				SparkDraw(&m);
				break;
			}
			case ORIG_EmberTask:
			{
				EmberNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				if (moving) EmberUpdate(&b);
				EmberNode m = a;
				m.pos[1] = lerp16(a.pos[1], b.pos[1], num, den);
				m.scale = lerp16(a.scale, b.scale, num, den);
				EmberDraw(&m);
				break;
			}
			case ORIG_ShockRingTask: ShockRingHeld(r, num, den, moving); break;
			case ORIG_FlameColumnTask: ModelHeld(r, num, den, moving, FlameColumnMatrix, FlameColumnHeader, FlameColumnUpdate); break;
			case ORIG_BurstTask: ModelHeld(r, num, den, moving, BurstMatrix, BurstHeader, BurstUpdate); break;
			case ORIG_TrailTask: ModelHeld(r, num, den, moving, TrailMatrix, TrailHeader, TrailUpdate); break;
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag142_held()
	{
		mark_held(fira142::ORIG_SparkTask);
		mark_held(fira142::ORIG_EmberTask);
		mark_held(fira142::ORIG_ShockRingTask);
		mark_held(fira142::ORIG_FlameColumnTask);
		mark_held(fira142::ORIG_BurstTask);
		mark_held(fira142::ORIG_TrailTask);
		register_module_held(142, fira142::HeldReady, fira142::HeldFrame);
	}
}
