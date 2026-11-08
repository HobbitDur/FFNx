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

// 30 fps part of mag266_tiamat_death.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039). Draw 22
// EntityDisintegrate has its own held draw (gfc_disintegrate_held.inc: the model at the entity's
// in-between pose, the pieces / dots at their exact in-between positions, launches on real ticks
// only); draw 16 / prim 16 are the shared engine's; every other live slot is the generic engine's.

namespace ff8fx
{
namespace gfc
{
namespace tiamat266
{
#include "gfc_disintegrate_held.inc"

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag266_tiamat_death_held()
	{
		using namespace gfc;
		mark_held(0x91FC50);
		// the entity-joint VM ops (0x05B / 0x060 / 0x06E / 0x075) read a joint of an animating battle
		// entity: at its next pose while predicting (gfc_engine_held.inc held_entity_next_pose)
		held_entity_joints(tiamat266::g_c);
		register_module_held(266, tiamat266::Ready, tiamat266::Frame);
		register_module_camera(266, tiamat266::Camera);
	}
}
