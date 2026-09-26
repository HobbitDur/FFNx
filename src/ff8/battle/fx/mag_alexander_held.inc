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

// 30 fps part of mag_alexander.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_alexander.cpp)
//
// Held-frame behaviour of the Alexander handlers (the engine draws the held frames):
//   Draw 35 RenderToTexture: see held_env_prims below.
//   Draw 36 ScreenLensDisc: pure function of the node pose (outPos, outAngle, colour) and the
//     param block: drawn at the in-between pose. Nothing to skip, nothing persistent.
//   Prim 29: pure draw code.

namespace ff8fx
{
namespace gfc
{
namespace part_alexander
{
	// 0xB02540 (Draw 35 RenderToTexture), held frames: the DR_ENV pair is built and linked into
	// the held frame's own OT (a new frame, so the "never twice into the same OT" hazard does not
	// arise). BUT the block lives in the arena, which HeldFrame restores after the held draw -
	// before the held OT is executed: the restore would put back the real tick's tag words (their
	// low 24 bits are the OT link, SSIGPU_InsertPrimAutoDepth keeps the chain in them), breaking
	// the held OT at slots 4 / 4364. So on held frames the two DR_ENV prims are built in the held
	// packet buffer (0x80 B taken from ctx+0x7C, same +0/+0x40 layout) instead of the persistent
	// block, and the block is neither allocated nor written (FX_HELD statements of the handler).
	// Everything else is identical (the strips are drawn at the in-between pose). Nothing else
	// here is persistent (ws scratch, arena-top scratch).
	static uint8_t *held_env_prims()
	{
		uint8_t *prim = PTR(CTX(), 0x7C);                                  // private packet memory
		PTR(CTX(), 0x7C) = prim + 0x80;
		return prim;
	}
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace alexander
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_alexander_held()
	{
		using namespace gfc;
		mark_held(0xB00310);
		register_module_held(204, alexander::Ready, alexander::Frame);
		register_module_camera(204, alexander::Camera);
	}
}
