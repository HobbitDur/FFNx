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

// 30 fps part of mag263_massive_anchor.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//
// VM 0x0AB runs only on real ticks and in the prediction (the animation set / chain binding is
// skipped while predicting by its wrappers). Draw 70 (the anchor's attach offset in the weapon
// block, read by the battle when it draws the caster) writes nothing on a held frame: the battle
// draws the caster and his weapon outside the effect (the frame gate interpolates them), and a
// held write would reach the next real tick's battle code before the real draw 70 rewrites it.
// VM 0x05B (generic AttachToModelJoint) on a battle entity: see op_05B_AttachToModelJointHeld.

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	// VM 0x05B AttachToModelJoint (generic) on a battle entity (op bit 10) while predicting: the
	// engine skips the entity's pose rebuild (its skeleton is outside the snapshot), so the
	// prediction composed the joint of the skeleton as the battle left it (the real tick's pose):
	// the bones on the caster's joints held tick N on the held frames. The entity's NEXT pose
	// (pose_midpoint at 1: the frame the next tick's own pose rebuild 0x508C90 reads) is put in
	// first, the skeleton journaled (guard) and put back when the prediction ends; then the generic
	// handler runs.
	static VoidFn g_op05B = nullptr;
	static void __cdecl op_05B_AttachToModelJointHeld()
	{
		if (held_predicting() && (U8(RT(), 0x4B) & 4))
		{
			uint8_t *hdr = SlotEntity() + 0x60;
			uint8_t *sk = PTR(PTR(hdr, 4), 0);
			guard(sk, 0x10 + 0x30 * (int)U8(sk, 0));
			pose_midpoint(hdr, hdr + 0xC, 1, 1);
		}
		g_op05B();
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag263_massive_anchor_held()
	{
		using namespace gfc;
		anchor263::g_op05B = anchor263::g_c.vm[0x05B];
		anchor263::g_c.vm[0x05B] = anchor263::op_05B_AttachToModelJointHeld;
		mark_held(0x945E80);
		register_module_held(263, anchor263::Ready, anchor263::Frame);
		register_module_camera(263, anchor263::Camera);
	}
}
