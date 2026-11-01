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
// (mag_eden_held.inc). VM 0x0AD places a bone at joint 38 of entity 3 / 4 (the Brothers, animating):
// the prediction of the next tick takes the joint from the entity's NEXT pose (held_next_pose:
// pose_midpoint at 1, the animation reader one frame ahead, as the next real tick rebuilds the bone
// matrices after the entity's animation step; the x::BuildBoneMatricesFromPose wrapper skips the
// rebuild while predicting, which held the joint half a tick behind). The skeleton block is
// journaled by guard() and put back when the prediction ends.

namespace ff8fx
{
namespace gfc
{
namespace mad_cow_special
{
	// while predicting: the bone matrices of the model whose BattleAnimHeader is hdr (animation
	// command cmd) at its next pose; the skeleton block (root, poses, matrices: 16 + 48 bytes per
	// bone) journaled first (as Homing Laser 229's)
	static void held_next_pose(uint8_t *hdr, uint8_t *cmd)
	{
		uint8_t *com = hdr ? PTR(hdr, 4) : nullptr;
		uint8_t *sk = com ? PTR(com, 0) : nullptr;
		if (!sk || U8(sk, 0) == 0) return;
		guard(sk, 16 + 48 * (int)U8(sk, 0));
		pose_midpoint(hdr, cmd, 1, 1);
	}

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
