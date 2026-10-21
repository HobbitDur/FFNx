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

// 30 fps part of mag229_homing_laser_quistis.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039); its prediction of the
// next tick runs engine code that may use the Field_Alloc scratch stack: the scratch above the
// pointer is put back as the real tick left it (as Blasting Zone 164 / Lion Heart 165).
//
// The handlers taken from other clones carry their own held handling: draw 37 RibbonTrail
// (gfc_engine_held.inc). VM 0x0AB (the caster's animation set + chain transformation) is skipped
// while predicting, as every entity write of the engine. VM 0x0AD places a bone at a joint of the
// slot entity (the caster, animating): the prediction of the next tick takes the joint from the
// entity's NEXT pose (held_next_pose: pose_midpoint at 1, the animation reader one frame ahead,
// as the next real tick rebuilds the bone matrices after the entity's animation step; Mad Cow
// Special 258's port keeps the real tick's matrices there, which holds the joint half a tick
// behind an animating entity). The skeleton block is journaled by guard() and put back when the
// prediction ends.

namespace ff8fx
{
namespace gfc
{
namespace homing_laser_q
{
	// while predicting: the bone matrices of the model whose BattleAnimHeader is hdr (animation
	// command cmd) at its next pose; the skeleton block (root, poses, matrices: 16 + 48 bytes per
	// bone) journaled first
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
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		static uint8_t save[0x1000];
		uint8_t *scratch = MEM<uint8_t *>(0x1D999C4);
		if (scratch) memcpy(save, scratch, sizeof(save));
		const bool r = HeldCamera(g_c, num, den, world, lookat);
		if (scratch) memcpy(scratch, save, sizeof(save));
		return r;
	}
}
}

	static void register_mag229_homing_laser_quistis_held()
	{
		using namespace gfc;
		mark_held(0xA7B090);
		register_module_held(229, homing_laser_q::Ready, homing_laser_q::Frame);
		register_module_camera(229, homing_laser_q::Camera);
	}
}
