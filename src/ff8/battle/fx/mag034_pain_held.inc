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

// 30 fps part of mag034_pain.cpp: the module's held frame and the held registration (included at the
// end of mag034_pain.cpp)
//
// Held frames (30 fps): everything Pain draws comes from its two prim-model players (0x8659D0,
// 0x865DB0: every packet is emitted by the draw callback 0x865AE0), so the held frame is the prim-model
// replay of act::held_frame (prim::play_held: the exact in-between records of this real tick's plays).
// The callback draws into the module's own arena (cursor 0x26BA9E4): the held frame points that
// cursor at a private buffer; act::held_frame saves / restores the module globals around it.

namespace ff8fx
{
namespace act
{
namespace pain
{
	static const uint32_t HELD_TASKS[] = { ORIG_GlowPlayer, ORIG_ModelPlayer, 0 };
	// no creature actor: an empty queue head
	static const uint32_t g_no_creature = 0;
	// module globals 0x26BA9E0..0x26C0380 (file pointer, packet cursor, pools, queues, blend buffer)
	static const HeldDesc HELD = { &MOD_034, P(&g_no_creature), 0, 0, 0x26BA9E0, 0x26C0380 };
	static uint8_t g_held_packets[0x40000];
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den)
	{
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		held_frame(HELD, num, den);
		MEM<uint32_t>(PACKET_CURSOR) = cursor;
	}
}
}
	static void register_mag034_held()
	{
		for (const uint32_t *h = act::pain::HELD_TASKS; *h; h++) mark_held(*h);
		register_module_held(34, act::pain::HeldReady, act::pain::HeldFrame);
	}
}
