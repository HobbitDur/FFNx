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

// 30 fps part of mag090_tonberry.cpp: the module's held-frame descriptor, held frame / held-frame camera
// entry points and the held registration (included at the end of mag090_tonberry.cpp)
//
// Held frames (30 fps): the creature actor (position advanced by velocity/16 * num/den, midpoint
// pose) and the two prim-model tasks 0x763EB0 / 0x767B40 (all their packets come from the prim-model
// callback, redrawn by prim::play_held) are redrawn in between by act::held_frame; every other task
// stays on the generic packet path. Camera: act::held_camera.

namespace ff8fx
{
namespace act
{
namespace tonberry
{
	// tasks whose drawing the held frame redraws: the creature actor (0x7674E0) and the two
	// prim-model tasks (0x763EB0, 0x767B40 - every packet they emit comes from the prim-model
	// draw callback 0x764260, so prim::play_held redraws all of it)
	static const uint32_t HELD_TASKS[] = { 0x7674E0, 0x763EB0, 0x767B40, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// the creature walks: every tick 0x7674E0 adds velocity/16 (s16 +0x124/+0x126/+0x128) to its
	// position (+0x4C/+0x4E/+0x50) before drawing; the held frame adds the same step scaled by
	// num/den (the velocity of the next tick is taken to be the current one)
	static void Adjust(uint32_t node, int num, int den)
	{
		for (int i = 0; i < 3; i++)
		{
			const int32_t step = (int32_t)S16(node, 0x124 + 2 * i) / 16;
			S16(node, 0x4C + 2 * i) = (int16_t)(S16(node, 0x4C + 2 * i) + step * num / den);
		}
	}
	// module globals 0x259EEA8..0x25A4E00 (task pools, packet pools, arenas, script/camera state)
	static const HeldDesc HELD = { &MOD_090, 0x25A3F90, 0x7674E0, 0x25A2D00, 0x259EEA8, 0x25A4E00, nullptr, Adjust };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { held_frame(HELD, num, den); }
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_090;
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag090_held()
	{
		act::register_module_held_tasks(90, act::tonberry::HELD_TASKS);
		for (const act::tonberry::ModPort *p = act::tonberry::PORTS; p->addr; p++)
			if (act::tonberry::is_held(p->addr)) mark_held(p->addr);
		register_module_held(90, act::tonberry::HeldReady, act::tonberry::HeldFrame);
		register_module_camera(90, act::tonberry::HeldCamera);
	}
}
