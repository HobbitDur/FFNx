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

// 30 fps part of mag_bahamut.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_bahamut.cpp)
//
// Held-frame behaviour of the Bahamut handlers (the engine draws the held frames; the handlers
// run with the in-between node state, rt->boneSkipFlag forced):
//   Draw 40 LitScrollGrid, Draw 41 SineStrip, Draw 46 FlatQuad: a pure function of the
//     (in-between) bone state; packets only, nothing advanced (Draw 40 undoes its bone+0x8E write
//     itself).
//   Draw 43 Beam: the rand weights / point generation run only on the block's first call, never
//     again (the jne at 0xB1B9F6 skips both). On a held frame the block exists (the real tick
//     drew this bone and bone+0xBC is in the snapshot), so the draw is the deterministic
//     projection of the stored points with the in-between bone state. Safety: a held frame that
//     finds no block (the real tick did not draw this bone) returns without drawing
//     (FX_HELD statement of dh_43_Beam), so no rand draw / arena allocation ever happens on a
//     held frame.

namespace ff8fx
{
namespace gfc
{
namespace bahamut
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_bahamut_held()
	{
		using namespace gfc;
		mark_held(0xB19010);
		register_module_held(202, bahamut::Ready, bahamut::Frame);
		register_module_camera(202, bahamut::Camera);
	}
}
