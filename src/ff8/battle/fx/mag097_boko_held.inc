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

// 30 fps part of mag097_boko.cpp: the module's held-frame descriptor, held frame / held-frame camera
// entry points and the held registration (included at the end of mag097_boko.cpp)
//
// Held frames (30 fps): the creature actors (midpoint pose) and every prim-model task (all their
// packets come from the prim-model draw callbacks, redrawn by prim::play_held) are redrawn in
// between by act::held_frame; every other task stays on the generic packet path. Camera:
// act::held_camera.

namespace ff8fx
{
namespace act
{
namespace boko
{
	// tasks whose drawing the held frame redraws: the creature actor(s) and the prim-model tasks
	// (every packet of these comes from their prim-model plays / draw callbacks)
	static const uint32_t HELD_097[] = { 0x7306A0, 0x72A660, 0x731100, 0x731840, 0x731950, 0 };
	static const uint32_t HELD_098[] = { 0x728210, 0x722460, 0x728C60, 0x728DA0, 0x7294C0, 0x7295D0, 0 };
	static const uint32_t HELD_099[] = { 0x71E8D0, 0x7189B0, 0x71F950, 0x71FDC0, 0x71FEC0, 0x71FFB0, 0x7201F0, 0x720320, 0x720410, 0x720610, 0 };
	static const uint32_t HELD_100[] = { 0x715540, 0x7175F0, 0x70F5F0, 0x7163D0, 0x716840, 0x716950, 0 };
	// ChocoBocle's second creature kind (0x7175F0 = the Boko chocobo actor of 097/098)
	static const HeldCreature MORE_100[] = { { 0x25656B8, 0x7175F0, 0x25642E8 }, { 0, 0, 0 } };
	// module globals: each module's block up to the next module's
	static const HeldDesc HELD_D097 = { &MOD_097, 0x257B158, 0x7306A0, 0x2579EC8, 0x2575268, 0x257B740 };
	static const HeldDesc HELD_D098 = { &MOD_098, 0x2574ED8, 0x728210, 0x2573C48, 0x256E358, 0x2575268 };
	static const HeldDesc HELD_D099 = { &MOD_099, 0x256D9D0, 0x71E8D0, 0x256C600, 0x2565A30, 0x256E358 };
	static const HeldDesc HELD_D100 = { &MOD_100, 0x25656B8, 0x715540, 0x25642E8, 0x255EAC8, 0x2565A30, MORE_100 };
	static bool is_in(const uint32_t *l, uint32_t a) { for (; *l; l++) if (*l == a) return true; return false; }
	static bool HeldReady() { return held_ready(); }
	template<const HeldDesc *D> static void HeldFrame(int num, int den) { held_frame(*D, num, den); }
	template<int E> static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = find_mod(E);
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
	// held tasks of a module (engine ports and Boko ports), its held frame and held-frame camera
	static void reg_held(int eff, const ModPort *ports, const uint32_t *held, void (*frame)(int, int), HeldCameraFn cam)
	{
		register_module_held_tasks(eff, held);
		for (const ModPort *p = ports; p->addr; p++)
			if (is_in(held, p->addr)) mark_held(p->addr);
		register_module_held(eff, HeldReady, frame);
		register_module_camera(eff, cam);
	}
}
}
	static void register_mag097_held()
	{
		using namespace act::boko;
		reg_held(97, PORTS_097, HELD_097, HeldFrame<&HELD_D097>, HeldCamera<97>);
		reg_held(98, PORTS_098, HELD_098, HeldFrame<&HELD_D098>, HeldCamera<98>);
		reg_held(99, PORTS_099, HELD_099, HeldFrame<&HELD_D099>, HeldCamera<99>);
		reg_held(100, PORTS_100, HELD_100, HeldFrame<&HELD_D100>, HeldCamera<100>);
	}
}
