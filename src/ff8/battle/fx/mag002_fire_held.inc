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

// 30 fps part of mag002_fire.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag002_fire.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Flame  - flipbook at a fixed position, ground glow from its fixed height: holds.
//   Spark  - position half way along its velocity, size half way to the next shrink step;
//            the flipbook frame (9 - life) holds.
//   Ember  - height half way along its rising velocity, size half way to the next shrink step;
//            the flipbook frame holds.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.

namespace ff8fx
{
namespace fire002
{
	// real tick on which the ported root task last ran
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
			case ORIG_FlameTask:
			{
				FlameNode a;
				memcpy(&a, r.node, sizeof(a));
				const FireParams::Flames &fp = P().flames;
				const FlameLook look = { SEQ_Flame, SEQ_FlameGlow, fp.shade_shift, fp.glow.tilt, fp.glow.mode, fp.glow.high, fp.glow.span, fp.glow.base };
				FlameDraw(&a.f, a.frame, look, PacketCursor());
				break;
			}
			case ORIG_SparkTask:
			{
				SparkNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				const FireParams::Sparks &s = P().sparks;
				if (moving) FireSparkMove(&b.s, &b.life, s.drag, s.shrink);
				SparkNode m = a;
				for (int k = 0; k < 3; k++) m.s.pos[k] = lerp16(a.s.pos[k], b.s.pos[k], num, den);
				m.s.scale = lerp16(a.s.scale, b.s.scale, num, den);
				const SparkLook look = { s.sprite, s.last, s.flag20, s.mode, s.shade_shift };
				FireSparkDraw(&m.s, m.life, look, PacketCursor());
				break;
			}
			case ORIG_EmberTask:
			{
				EmberNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				const FireParams::Embers &e = P().embers;
				if (moving) { EmberRise(&b.e, e.accel, e.shrink); b.frame++; }
				EmberNode m = a;
				m.e.pos[1] = lerp16(a.e.pos[1], b.e.pos[1], num, den);
				m.e.scale = lerp16(a.e.scale, b.e.scale, num, den);
				EmberDraw(&m.e, m.frame, SEQ_Ember, e.shade_shift, PacketCursor());
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag002_held()
	{
		mark_held(fire002::ORIG_FlameTask);
		mark_held(fire002::ORIG_SparkTask);
		mark_held(fire002::ORIG_EmberTask);
		register_module_held(2, fire002::HeldReady, fire002::HeldFrame);
	}
}
