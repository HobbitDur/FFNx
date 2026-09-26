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

// 30 fps part of mag003_thunder.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag003_thunder.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Bolt - the flash quad (frame 0) and the strike layers are flipbooks keyed on the bolt's
//          frame at a fixed anchor, the ground layer a fixed tilted plane: everything holds,
//          the held frame shows exactly the real tick's picture.
// The root draws nothing. No Thunder task has a draw-only mode.

namespace ff8fx
{
namespace thunder003
{
	// real tick on which the ported root task last ran
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<16> g_draws;
	static uint8_t g_held_packets[0x8000];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
	}

	static void held_note_draw(uint32_t fn, const void *node) { g_draws.add(fn, node, PacketCursor()); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	static void HeldFrame(int, int)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			if (r.fn == ORIG_BoltTask)
			{
				BoltNode a;
				memcpy(&a, r.node, sizeof(a));
				BoltDraw(&a);
			}
			g_hdr_slot = nullptr;
		}
	}
}

	static void register_mag003_held()
	{
		mark_held(thunder003::ORIG_BoltTask);
		register_module_held(3, thunder003::HeldReady, thunder003::HeldFrame);
	}
}
