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

// 30 fps part of mag223_meteor.cpp: Meteor's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks,
// every draw handler re-run in its frozen (boneSkipFlag) form. Meteor's own handlers: draw 25
// (ribbon grid) is idempotent and redraws the in-between bones with the same code; draw 28
// (spread action results) is skipped on held frames (mag223_meteor.cpp); VM 0x035 / 0x0BB /
// 0x0BD run only on real ticks and in the prediction (VRAM queue calls and the texture copy
// skipped there). Camera: HeldCamera interpolates the battle camera words the VM writes (op
// 0x039); Meteor only starts a battle camera animation, so it returns false.

namespace ff8fx
{
namespace gfc
{
namespace meteor
{
	static bool Ready() { return HeldReady(g_meteor); }
	static void Frame(int num, int den) { HeldFrame(g_meteor, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_meteor, num, den, world, lookat); }
}
}

	static void register_mag223_meteor_held()
	{
		using namespace gfc;
		mark_held(0xA8FF00);
		register_module_held(223, meteor::Ready, meteor::Frame);
		register_module_camera(223, meteor::Camera);
	}
}
