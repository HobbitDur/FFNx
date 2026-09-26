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

// 30 fps part of mag_cerberus.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_cerberus.cpp)
//
// Held-frame behaviour of the Cerberus handlers (the engine draws the held frames):
//   Draw 45 ScreenMelt: rt->boneSkipFlag is forced, so the frozen path runs: no integration, no
//     counter, the columns are drawn exactly where the real tick drew them. A first call on a
//     held frame (bone+0xBC still 0) would allocate + seed (rand) as vanilla; HeldFrame restores
//     the arena, the bones and the CRT seed, so nothing of it persists.
//   Prim 28 / 38 / 39: pure draw code.

namespace ff8fx
{
namespace gfc
{
namespace cerberus
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_cerberus_held()
	{
		using namespace gfc;
		mark_held(0xB0C820);
		register_module_held(203, cerberus::Ready, cerberus::Frame);
		register_module_camera(203, cerberus::Camera);
	}
}
