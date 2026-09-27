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

// 30 fps part of mag222_water.cpp: Water's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks,
// every draw handler re-run in its frozen (boneSkipFlag) form; draw 38 (AttachBattleEntity,
// mag_brothers.cpp) saves / restores the battle entity it writes. Camera: HeldCamera
// interpolates the battle camera words the VM writes (op 0x039).

namespace ff8fx
{
namespace gfc
{
namespace water
{
	static bool Ready() { return HeldReady(g_water); }
	static void Frame(int num, int den) { HeldFrame(g_water, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_water, num, den, world, lookat); }
}
}

	static void register_mag222_water_held()
	{
		using namespace gfc;
		mark_held(0xA9B870);
		register_module_held(222, water::Ready, water::Frame);
		register_module_camera(222, water::Camera);
	}
}
