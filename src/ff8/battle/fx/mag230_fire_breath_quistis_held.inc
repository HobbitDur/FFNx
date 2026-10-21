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

// 30 fps part of mag230_fire_breath_quistis.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039); its prediction of the
// next tick runs engine code that may use the Field_Alloc scratch stack: the scratch above the
// pointer is put back as the real tick left it (as Blasting Zone 164 / Lion Heart 165).
//
// The handlers taken from other clones carry their own held handling: prim 16 (shared TintedF4,
// drawn by the frozen draw path). VM 0x094 (a conditional jump) and VM 0x0AB (the caster's
// animation set + chain transformation: skipped while predicting, as every entity write of the
// engine) run only on real ticks and predictions.

namespace ff8fx
{
namespace gfc
{
namespace fire_breath_q
{
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

	static void register_mag230_fire_breath_quistis_held()
	{
		using namespace gfc;
		mark_held(0xA711E0);
		register_module_held(230, fire_breath_q::Ready, fire_breath_q::Frame);
		register_module_camera(230, fire_breath_q::Camera);
	}
}
