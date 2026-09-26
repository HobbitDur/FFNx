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

// 30 fps part of mag096_minimog.cpp: the module's held-frame descriptor, held frame / held-frame camera
// entry points and the held registration (included at the end of mag096_minimog.cpp)
//
// Held frames (30 fps): the creature actor and the prim-model sparkles (m_733FC0) are redrawn in
// between by act::held_frame; every other task stays on the generic packet path. Camera:
// act::held_camera.

namespace ff8fx
{
namespace act
{
namespace minimog
{
	// tasks whose drawing the held frame redraws: the creature actor and the prim-model emitters
	static const uint32_t HELD_TASKS[] = { 0x739610, 0x733FC0, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// module globals 0x257B740..0x257F8A0 (task pools, arenas, script/camera state, key tables)
	static const HeldDesc HELD = { &MOD_096, 0x257F678, 0x739610, 0x257E3E8, 0x257B740, 0x257F8A0 };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { held_frame(HELD, num, den); }
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_096;
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag096_held()
	{
		act::register_module_held_tasks(96, act::minimog::HELD_TASKS);
		for (const act::minimog::ModPort *p = act::minimog::PORTS; p->addr; p++)
			if (act::minimog::is_held(p->addr)) mark_held(p->addr);
		register_module_held(96, act::minimog::HeldReady, act::minimog::HeldFrame);
		register_module_camera(96, act::minimog::HeldCamera);
	}
}
