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

// 30 fps part of mag253_twin_homing_laser.cpp (the 30 fps part of Homing Laser 229's copy): the held frames
// are the cinematic engine's (gfc_engine_held.inc): the node poses / matrix nodes interpolated
// between two real ticks, every draw handler re-run in its frozen (boneSkipFlag) form on the saved
// engine state. Camera: HeldCamera interpolates the battle camera words the VM writes (op 0x039);
// its prediction of the next tick runs engine code that may use the Field_Alloc scratch stack: the
// scratch above the pointer is put back as the real tick left it (as 229).
//
// Draw 37 RibbonTrail carries its own held handling (gfc_engine_held.inc). VM 0x0AD places a bone
// at a joint of the slot entity (the caster, animating): the prediction of the next tick takes the
// joint from the entity's NEXT pose (held_next_pose: pose_midpoint at 1, the animation reader one
// frame ahead, as the next real tick rebuilds the bone matrices after the entity's animation step).
// The skeleton block is journaled by guard() and put back when the prediction ends.

namespace ff8fx
{
namespace gfc
{
namespace twinlaser253
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

	static void register_mag253_twin_homing_laser_held()
	{
		using namespace gfc;
		mark_held(0x998380);
		// the entity-joint VM ops (0x05B / 0x060 / 0x06E / 0x075) read a joint of an animating battle
		// entity: at its next pose while predicting (gfc_engine_held.inc held_entity_next_pose)
		held_entity_joints(twinlaser253::g_c);
		register_module_held(253, twinlaser253::Ready, twinlaser253::Frame);
		register_module_camera(253, twinlaser253::Camera);
	}
}
