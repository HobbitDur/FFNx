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

// 30 fps part of mag232_breath_of_death.cpp: Breath of Death's held frames are the cinematic
// engine's (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real
// ticks, every draw handler re-run in its frozen (boneSkipFlag) form. Camera: HeldCamera
// interpolates the battle camera words the VM writes (op 0x039).
//
// Draw 44 (AnimatedEntityCopy) is an embedded battle model block like draw 3 (bone+0xBC, header
// +4 -> block+0x30 -> the private skeleton copy in the arena): registered with
// held_model_draws, so the held frame saves its skeleton and 0xB26AD0's frozen path draws the
// exact in-between pose (pose_midpoint) when the real tick advanced the animation. The render
// colour, the render descriptor and the model copy block live in the arena (saved by HeldFrame);
// the two engine cells RenderGeometry writes (0x1D99C18 / 0x1D99C1C) are put back after the draw.
// Bone 10 (ArcBetweenBones) only runs in the VM prediction (pure node math on saved state).
// Draws 2 / 44 (scaled parent transform 0xA5D580): the scale is the exact in-between of the two
// real ticks' outAngle << 4 (held_bone_ends), not the integer-mixed outAngle.

namespace ff8fx
{
namespace gfc
{
namespace death232
{
	// RenderGeometry (0x5099D0 -> ParsePolygons 0x50FDF0) stores its arguments in the engine cells
	// 0x1D99C18 / 0x1D99C1C: outside every saved set, so a held draw puts them back
	static uint8_t g_rg_cells[8];
	static bool g_rg_on = false;
	static void held_render_begin()
	{
		g_rg_on = g_held.active;
		if (g_rg_on) memcpy(g_rg_cells, (const void *)0x1D99C18, 8);
	}
	static void held_render_end()
	{
		if (g_rg_on) memcpy((void *)0x1D99C18, g_rg_cells, 8);
		g_rg_on = false;
	}

	// 0xA5D580 on a held frame: the scale words ws+0xD4.. = the exact in-between of the two real
	// ticks' outAngle << 4 (ApplyInBetween mixes outAngle as an integer, so a scale that grows by 1
	// per tick kept the real tick's value and the scaled mesh left the [N, N+1] range)
	static void held_scale_fraction(const uint8_t *bone, uint8_t *scale3)
	{
		int16_t a0[3], p0[3], a1[3], p1[3];
		if (!held_bone_ends(bone, a0, p0, a1, p1)) return;
		for (int k = 0; k < 3; k++)
		{
			int32_t a = shl32(a0[k], 4), b = shl32(a1[k], 4);
			S32(scale3, 4 * k) = (int32_t)(a + (int64_t)(b - a) * g_held.num / g_held.den);
		}
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag232_breath_of_death_held()
	{
		using namespace gfc;
		held_model_draws(death232::g_c, 44);
		mark_held(0xA5C950);
		register_module_held(232, death232::Ready, death232::Frame);
		register_module_camera(232, death232::Camera);
	}
}
