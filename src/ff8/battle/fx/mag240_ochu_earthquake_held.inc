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

// 30 fps part of mag240_ochu_earthquake.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//
// Draw 33 (ShatterSpin) and draw 34 (MeshOnNode) need nothing of their own: both read only the
// bone's outAngle / outPos (interpolated by the engine), its parent / reference node matrices
// (interpolated nodes) and data fixed after their first draw (the pieces' centroids and random
// spins, the mesh pointers). A held draw of a piece = the vanilla draw at the in-between push-out,
// spin and scale; the arena block and the CRT seed a first draw on a held frame would touch are
// restored by HeldFrame. The OT insertion is skipped while the engine predicts (never happens:
// the prediction runs no draw).

namespace ff8fx
{
namespace gfc
{
namespace ochu_quake
{
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag240_ochu_earthquake_held()
	{
		using namespace gfc;
		mark_held(0xA0B9D0);
		// the entity-joint VM ops (0x05B / 0x060 / 0x06E / 0x075) read a joint of an animating battle
		// entity: at its next pose while predicting (gfc_engine_held.inc held_entity_next_pose)
		held_entity_joints(ochu_quake::g_c);
		register_module_held(240, ochu_quake::Ready, ochu_quake::Frame);
		register_module_camera(240, ochu_quake::Camera);
	}
}
