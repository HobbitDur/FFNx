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

// 30 fps part of mag095_siren.cpp: the module's held-frame descriptor, held frame / held-frame camera
// entry points and the held registration (included at the end of mag095_siren.cpp)
//
// Held frames (30 fps): the creature actor and the prim-model emitters (s_73C1A0, s_73EE50,
// s_73EF60, 0x73F110/0x73F370/0x73F500, s_73F250, s_73F6A0) are redrawn in between by
// act::held_frame; every other task stays on the generic packet path. Camera: act::held_camera.

namespace ff8fx
{
namespace act
{
namespace siren
{
	// tasks whose drawing the held frame redraws: the creature actor and the prim-model emitters
	static const uint32_t HELD_TASKS[] = { 0x746B60, 0x73C1A0, 0x73EE50, 0x73EF60, 0x73F110, 0x73F370, 0x73F500, 0x73F250, 0x73F6A0, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// module globals 0x257F8A0..0x258FCF4 (task pools, arenas, script/camera state, key tables)
	static const HeldDesc HELD = { &MOD_095, 0x258EC40, 0x746B60, 0x258BFB0, 0x257F8A0, 0x258FCF4 };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { held_frame(HELD, num, den); }
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_095;
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag095_held()
	{
		act::register_module_held_tasks(95, act::siren::HELD_TASKS);
		for (const act::siren::ModPort *p = act::siren::PORTS; p->addr; p++)
			if (act::siren::is_held(p->addr)) mark_held(p->addr);
		register_module_held(95, act::siren::HeldReady, act::siren::HeldFrame);
		register_module_camera(95, act::siren::HeldCamera);
	}
}
