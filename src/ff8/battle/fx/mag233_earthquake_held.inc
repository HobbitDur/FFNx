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

// 30 fps part of mag233_earthquake.cpp: Earthquake's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks,
// every draw handler re-run in its frozen (boneSkipFlag) form. Draw 18 WaterColumn
// (mag_leviathan.cpp) runs unchanged: every input is a node pose value, no counter / timer / rand,
// so it draws the exact in-between ripple mesh. Camera: HeldCamera interpolates the battle
// camera words the VM writes (op 0x039).
// Prediction (the next tick's VM on saved state, for HeldCamera): VM 0x06F OffscreenStageGround
// writes its block header at the arena top (outside [ctx+0x70, ctx+0x74) until the allocation)
// -> guard() before its first write; 0xB65B00 (GPU work + stage model writes) is skipped.
// VM 0x070 RideWaterColumn only writes the current node (+0x60) and calls its bone handler:
// engine state, saved by the prediction.

namespace ff8fx
{
namespace gfc
{
namespace earthquake
{
	static bool Ready() { return HeldReady(g_eq); }
	static void Frame(int num, int den) { HeldFrame(g_eq, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_eq, num, den, world, lookat); }
}
}

	static void register_mag233_earthquake_held()
	{
		using namespace gfc;
		mark_held(0xA525B0);
		register_module_held(233, earthquake::Ready, earthquake::Frame);
		register_module_camera(233, earthquake::Camera);
	}
}
