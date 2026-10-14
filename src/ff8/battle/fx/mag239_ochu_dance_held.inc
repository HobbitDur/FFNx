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

// 30 fps part of mag239_ochu_dance.cpp: Ochu Dance's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form. Camera: HeldCamera interpolates the
// battle camera words the VM writes (op 0x039).
//
// Draw 32 (EntityGlow) is Breath's port (mag235_breath.cpp) with its held handling
// (mag235_breath_held.inc: the battle entity record and its skeleton saved around a held draw).

namespace ff8fx
{
namespace gfc
{
namespace ochu_dance
{
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag239_ochu_dance_held()
	{
		using namespace gfc;
		mark_held(0xA160A0);
		register_module_held(239, ochu_dance::Ready, ochu_dance::Frame);
		register_module_camera(239, ochu_dance::Camera);
	}
}
