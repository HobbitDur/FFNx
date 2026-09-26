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

// 30 fps part of mag143_firaga.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag143_firaga.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   AreaFlame  - flipbook at a fixed position, ground glow from its fixed height: holds.
//   Burst      - size half way; fade (+0x0C) half way inside the fade-out ramp.
//   FlameSwirl - spin and size half way; fade half way inside the fade-out ramp.
//   Fireball   - size half way along its growth.
//   Spark      - orbit angle half way; fade half way inside the fade-in / fade-out ramps.
// The directors draw nothing. In the draw-only mode everything holds.

namespace ff8fx
{
namespace firaga143
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

	// a prim-model part half way to its next state (see mag142_fira_held.inc)
	static void ModelHeld(const DrawRec &r, int num, int den, bool moving, void (*matrix)(const ModelNode *),
		void (*header)(uint8_t *, int16_t), void (*update)(ModelNode *))
	{
		ModelNode a, b;
		memcpy(&a, r.node, sizeof(a));
		b = a;
		if (moving) update(&b);
		ModelNode m = a;
		m.yaw = lerp16(a.yaw, b.yaw, num, den);
		m.scale = lerp16(a.scale, b.scale, num, den);
		matrix(&m);
		uint8_t *h = AllocHeader(0x58);
		header(h, a.counter);
		if (moving)
		{
			uint8_t next[0x58];
			memcpy(next, r.hdr, sizeof(next));
			header(next, b.counter);
			LerpPrimFade(h, next, num, den);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void SparkHeld(const DrawRec &r, int num, int den, bool moving)
	{
		SparkNode a, b;
		memcpy(&a, r.node, sizeof(a));
		b = a;
		if (moving) SparkUpdate(&b);
		SparkNode m = a;
		m.angle = lerp16(a.angle, b.angle, num, den);
		SparkMatrix(&m);
		uint8_t *h = AllocHeader(0x58);
		SparkHeader(h, a.counter);
		if (moving)
		{
			uint8_t next[0x58];
			memcpy(next, r.hdr, sizeof(next));
			SparkHeader(next, b.counter);
			LerpPrimFade(h, next, num, den);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
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
			case ORIG_AreaFlameTask:
			{
				FlameNode a;
				memcpy(&a, r.node, sizeof(a));
				AreaFlameDraw(&a);
				break;
			}
			case ORIG_BurstTask: ModelHeld(r, num, den, moving, BurstMatrix, BurstHeader, BurstUpdate); break;
			case ORIG_FlameSwirlTask: ModelHeld(r, num, den, moving, FlameSwirlMatrix, FlameSwirlHeader, FlameSwirlUpdate); break;
			case ORIG_FireballTask:
			{
				ModelNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				if (moving) FireballUpdate(&b);
				ModelNode m = a;
				m.scale = lerp16(a.scale, b.scale, num, den);
				FireballDraw(&m);
				break;
			}
			case ORIG_SparkTask: SparkHeld(r, num, den, moving); break;
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag143_held()
	{
		mark_held(firaga143::ORIG_AreaFlameTask);
		mark_held(firaga143::ORIG_BurstTask);
		mark_held(firaga143::ORIG_FlameSwirlTask);
		mark_held(firaga143::ORIG_FireballTask);
		mark_held(firaga143::ORIG_SparkTask);
		register_module_held(143, firaga143::HeldReady, firaga143::HeldFrame);
	}
}
