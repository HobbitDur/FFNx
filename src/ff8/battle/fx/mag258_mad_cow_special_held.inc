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

// 30 fps part of mag258_mad_cow_special.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//
// The handlers taken from other clones carry their own held handling: draw 26 (shared, capture
// of the real tick), draw 43 Beam (mag_bahamut_held.inc) and draw 53 PixelParticleEmitter
// (mag_eden_held.inc). VM 0x0AD runs only on real ticks and predictions (its entity skeleton
// rebuild is skipped while predicting by the x::BuildBoneMatricesFromPose wrapper).

namespace ff8fx
{
namespace gfc
{
namespace mad_cow_special
{
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag258_mad_cow_special_held()
	{
		using namespace gfc;
		mark_held(0x965070);
		register_module_held(258, mad_cow_special::Ready, mad_cow_special::Frame);
		register_module_camera(258, mad_cow_special::Camera);
	}
}
