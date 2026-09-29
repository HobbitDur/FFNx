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
	static const uint32_t HELD_097[] = { 0x7306A0, 0x72A660, 0x731100, 0x731840, 0x731950, 0x730C10, 0 };
	static const uint32_t HELD_098[] = { 0x728210, 0x722460, 0x728C60, 0x728DA0, 0x7294C0, 0x7295D0, 0x722920, 0x728780, 0 };
	static const uint32_t HELD_099[] = { 0x71E8D0, 0x7189B0, 0x71F950, 0x71FDC0, 0x71FEC0, 0x71FFB0, 0x7201F0, 0x720320, 0x720410, 0x720610, 0x718E70, 0x71EE40, 0x7211D0, 0x721390, 0x720B90, 0x720EA0, 0x7208D0, 0x720500, 0 };
	static const uint32_t HELD_100[] = { 0x715540, 0x7175F0, 0x70F5F0, 0x7163D0, 0x716840, 0x716950, 0x70FAF0, 0x715AB0, 0 };
	// ChocoBocle's second creature kind (0x7175F0 = the Boko chocobo actor of 097/098)
	static const HeldCreature MORE_100[] = { { 0x25656B8, 0x7175F0, 0x25642E8 }, { 0, 0, 0 } };
	// the actor systems of the prim-model tasks whose state 1 is the engine step a_7395E0 (098 0x722920,
	// 099 0x718E70, 100 0x70FAF0; 097's own step 0x72B690 is not the engine's)
	static const HeldActorSys ACTOR_098[] = { { 0x722920 }, { 0 } };
	static const HeldActorSys ACTOR_099[] = { { 0x718E70 }, { 0 } };
	static const HeldActorSys ACTOR_100[] = { { 0x70FAF0 }, { 0 } };
	// node tasks (act::held_draw_node_tasks)
	// the shadow (b_730C10: 097 0x730C10 / 098 0x728780 / 100 0x715AB0 in its queue, 099 0x71EE40 in
	// 0x2565C20): its sequence frame
	// steps (the shape grows)
	static uint32_t ShadowQueue() { return BG(BG_2575458); }
	static void ShadowStep(uint32_t n)
	{
		const uint32_t states[6] = { BF(BF_730D00), BF(BF_730D50), BF(BF_730D70), BF(BF_730DD0), BF(BF_730E00), BF(BF_730E20) };
		callp(states[S8(n, 0x29)], n);
	}
	static void ShadowDraw(uint32_t n) { b_730C90(n, 0x800); }
	// 099: the two prim models 0x7211D0 / 0x721390 (a_7458E0, still: they move with the camera)
	static uint32_t Queue9338() { return 0x2569338; }
	static void Step7211D0(uint32_t n) { const uint32_t st[3] = { 0x721320, 0x721360, 0x721380 }; callp(st[S8(n, 0x29)], n); }
	static void Step721390(uint32_t n) { const uint32_t st[3] = { 0x7213F0, 0x721430, 0x721450 }; callp(st[S8(n, 0x29)], n); }
	static void DrawModel(uint32_t n) { a_7458E0(n); }
	// 099: the 4-particle sequence bursts 0x720B90 (b9_720C90) / 0x720EA0 (b_730C90 0x2000) and the
	// sequence 0x7208D0 (b_730C90 0x1600); particles: velocity damped, position moved, drawn there
	static uint32_t Queue5C20() { return 0x2565C20; }
	static void StepParticles(uint32_t n) { for (int i = 0; i < 4; i++) b92_particle_step(n, i); }
	static void AtParticle(uint32_t n, int i) { U32(n, 0x1C) = U32(n, 0x6C + 8 * i); U32(n, 0x20) = U32(n, 0x70 + 8 * i); }
	static void Step720B90(uint32_t n) { const uint32_t st[3] = { 0x720D00, 0x720D20, 0x720D60 }; callp(st[S8(n, 0x29)], n); StepParticles(n); }
	static void Draw720B90(uint32_t n) { for (int i = 0; i < 4; i++) { AtParticle(n, i); b9_720C90(n); } }
	static void Step720EA0(uint32_t n) { const uint32_t st[3] = { 0x720FA0, 0x720FC0, 0x721000 }; callp(st[S8(n, 0x29)], n); StepParticles(n); }
	static void Draw720EA0(uint32_t n) { for (int i = 0; i < 4; i++) { AtParticle(n, i); b_730C90(n, 0x2000); } }
	static void Step7208D0(uint32_t n) { const uint32_t st[3] = { 0x720940, 0x720960, 0x720990 }; callp(st[S8(n, 0x29)], n); }
	static void Draw7208D0(uint32_t n) { b_730C90(n, 0x1600); }
	static void Step720500(uint32_t n) { const uint32_t st[4] = { 0x720570, 0x7205A0, 0x7205D0, 0x720780 }; callp(st[S8(n, 0x29)], n); }
	static void Draw720500(uint32_t n) { b_730C90(n, 0x1800); }
	static const HeldNodeTask NODE_097[] = { { ShadowQueue, 0x730C10, 0x70, ShadowStep, ShadowDraw, NT_POS | NT_ANG54 }, { nullptr, 0 } };
	static const HeldNodeTask NODE_098[] = { { ShadowQueue, 0x728780, 0x70, ShadowStep, ShadowDraw, NT_POS | NT_ANG54 }, { nullptr, 0 } };
	static const HeldNodeTask NODE_100[] = { { ShadowQueue, 0x715AB0, 0x70, ShadowStep, ShadowDraw, NT_POS | NT_ANG54 }, { nullptr, 0 } };
	static const HeldNodeTask NODE_099[] = {
		{ Queue5C20, 0x71EE40, 0xB0, ShadowStep, ShadowDraw, NT_POS | NT_ANG54 },
		{ Queue9338, 0x7211D0, 0x58, Step7211D0, DrawModel, NT_POS | NT_ANG | NT_SCALE },
		{ Queue9338, 0x721390, 0x58, Step721390, DrawModel, NT_POS | NT_ANG | NT_SCALE },
		{ Queue5C20, 0x720B90, 0xB0, Step720B90, Draw720B90, NT_PART4 | NT_ANG54 },
		{ Queue5C20, 0x720EA0, 0xB0, Step720EA0, Draw720EA0, NT_PART4 | NT_ANG54 },
		{ Queue5C20, 0x7208D0, 0xB0, Step7208D0, Draw7208D0, NT_POS | NT_ANG54 },
		{ Queue5C20, 0x720500, 0xB0, Step720500, Draw720500, NT_POS | NT_ANG54 },
		{ nullptr, 0 } };
	// module globals: each module's block up to the next module's
	static const HeldDesc HELD_D097 = { &MOD_097, 0x257B158, 0x7306A0, 0x2579EC8, 0x2575268, 0x257B740, nullptr, nullptr, nullptr, NODE_097 };
	static const HeldDesc HELD_D098 = { &MOD_098, 0x2574ED8, 0x728210, 0x2573C48, 0x256E358, 0x2575268, nullptr, nullptr, ACTOR_098, NODE_098 };
	static const HeldDesc HELD_D099 = { &MOD_099, 0x256D9D0, 0x71E8D0, 0x256C600, 0x2565A30, 0x256E358, nullptr, nullptr, ACTOR_099, NODE_099, nullptr, 0x256C470 };
	static const HeldDesc HELD_D100 = { &MOD_100, 0x25656B8, 0x715540, 0x25642E8, 0x255EAC8, 0x2565A30, MORE_100, nullptr, ACTOR_100, NODE_100 };
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
		mark_held(0x8DDC30); // 099's screen tint (shared engine task; redrawn by act::held_draw_tints)
		reg_held(100, PORTS_100, HELD_100, HeldFrame<&HELD_D100>, HeldCamera<100>);
	}
}
