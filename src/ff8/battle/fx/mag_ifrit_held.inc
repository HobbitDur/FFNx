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

// 30 fps part of mag_ifrit.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_ifrit.cpp)
//
// Ifrit is the reference clone: no clone-specific held behaviour (the engine draws its held frames).

namespace ff8fx
{
namespace gfc
{
namespace ifrit
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_ifrit); }
	static void Frame(int num, int den) { HeldFrame(g_ifrit, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_ifrit, num, den, world, lookat); }
}
}

	static void register_gfc_ifrit_held()
	{
		using namespace gfc;
		mark_held(0xB25DF0);
		register_module_held(201, ifrit::Ready, ifrit::Frame);
		register_module_camera(201, ifrit::Camera);
	}
}
