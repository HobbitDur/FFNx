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
	static const uint32_t HELD_TASKS[] = { 0x746B60, 0x73C1A0, 0x73EE50, 0x73EF60, 0x73F110, 0x73F370, 0x73F500, 0x73F250, 0x73F6A0, 0x73F800, 0x73F900, 0x745180,
		0x7452F0, 0x745840, 0x7433C0, 0x744AA0, 0x744C30, 0x7466A0, 0x746FD0, 0x747220, 0x743DD0, 0x7441D0, 0x744570, 0x745AD0, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// node tasks (act::held_draw_node_tasks): every sprite / particle task that updates then draws
	// from its node (queue 0x257FA90, 0xE0 bytes; the spinning sprites in 0x2585EF0, 0x64 bytes)
	static uint32_t QueueFA90() { return 0x257FA90; }
	static uint32_t Queue5EF0() { return 0x2585EF0; }
	template<int N> static void States(uint32_t n, const uint32_t (&st)[N]) { callp(st[S8(n, 0x29)], n); }
	static void AtParticle(uint32_t n, int i) { U32(n, 0x1C) = U32(n, 0x6C + 8 * i); U32(n, 0x20) = U32(n, 0x70 + 8 * i); }
	// particle i: velocity (+0x8C+8i) -= velocity / 4, position (+0x6C+8i) += velocity / 16
	static void Damp4(uint32_t n)
	{
		for (int i = 0; i < 4; i++)
		{
			const uint32_t v = n + 0x8C + 8 * i, p = n + 0x6C + 8 * i;
			for (int k = 0; k < 3; k++) U16(v, 2 * k) = (uint16_t)(U16(v, 2 * k) - (uint16_t)((int32_t)S16(v, 2 * k) / 4));
			for (int k = 0; k < 3; k++) U16(p, 2 * k) = (uint16_t)(U16(p, 2 * k) + (uint16_t)((int32_t)S16(v, 2 * k) / 16));
		}
	}
	// the orbit sparkles' 4 sprite positions (+0x6C+8i): key lerp (index +0xCC+2i, t +0xD4+2i)
	// [+ node offset +0xAC+8i] through the orbit matrix, as s4_orbit_sprites_task / s_744570 place them
	static void OrbitPlace(uint32_t n, uint32_t keys, int32_t ty_bias, int32_t tz_bias, bool offset)
	{
		uint8_t m[0x20];
		s4_orbit_matrix(P(m), ty_bias, tz_bias);
		for (int i = 0; i < 4; i++)
		{
			const int32_t k8 = shl32(S16(n, 0xCC + 2 * i), 3), t = S16(n, 0xD4 + 2 * i);
			int16_t v[4];
			v[3] = 0x74; // (the high half of the state table entry the original's vector overlays)
			for (int c = 0; c < 3; c++)
				v[c] = (int16_t)(s4_key_lerp(keys, k8, 2 * c, t) + (offset ? U16(n, 0xAC + 8 * i + 2 * c) : 0));
			x::GTE_LoadV0(P(v));
			x::GTE_MVMVA_RotV0_Tr_2();
			x::GTE_StoreIR123(n + 0x6C + 8 * i);
		}
	}
	// the orbit globals of the next tick: Siren model offset 0x258FB80/82/84, scale vector
	// 0x258EC60..68 and the key tables 0x258BAA0 (25 keys), 0x258E8A0 / 0x258E9A0 (17 keys), all
	// written by the emitter prim-play callback (s_73EF60's draw, s_73E7F0 / s_73EB10): the held
	// frame's prim-play redraws left them at the held fraction, carried on to the whole tick from the
	// real tick's values; put back by OrbitNextEnd
	struct OrbitRange { uint32_t addr, bytes, s32; };
	static const OrbitRange ORBIT_GLOBALS[] = { { 0x258FB80, 6, 0 }, { 0x258EC60, 12, 1 }, { 0x258BAA0, 0xC8, 0 }, { 0x258E8A0, 0x88, 0 }, { 0x258E9A0, 0x88, 0 } };
	struct OrbitSave { uint8_t bytes[6 + 12 + 0xC8 + 0x88 + 0x88]; };
	static int g_orbit_num = 0, g_orbit_den = 1;
	static void OrbitNextBegin(OrbitSave &sv)
	{
		uint32_t o = 0;
		for (const OrbitRange &r : ORBIT_GLOBALS) { memcpy(sv.bytes + o, (const void *)r.addr, r.bytes); o += r.bytes; }
		if (g_orbit_num <= 0 || g_orbit_num >= g_orbit_den) return;
		for (const OrbitRange &r : ORBIT_GLOBALS)
			for (uint32_t k = 0; k < r.bytes; k += r.s32 ? 4 : 2)
			{
				const uint32_t a = r.addr + k;
				if (r.s32)
				{
					const int32_t v0 = *(const int32_t *)held_real_state(a);
					MEM<int32_t>(a) = v0 + (MEM<int32_t>(a) - v0) * g_orbit_den / g_orbit_num;
				}
				else
				{
					const int32_t v0 = *(const int16_t *)held_real_state(a);
					MEM<int16_t>(a) = (int16_t)(v0 + (MEM<int16_t>(a) - v0) * g_orbit_den / g_orbit_num);
				}
			}
	}
	static void OrbitNextEnd(const OrbitSave &sv)
	{
		uint32_t o = 0;
		for (const OrbitRange &r : ORBIT_GLOBALS) { memcpy((void *)r.addr, sv.bytes + o, r.bytes); o += r.bytes; }
	}
	static void Draw4Sprites(uint32_t n) { for (int i = 0; i < 4; i++) { AtParticle(n, i); s_7434C0(n); } }
	// 0x7452F0 orbiting sprite / 0x745840 spark: spin (+0x48), x (+0x1C) / scale (+0x30..)
	static void Step7452F0(uint32_t n)
	{
		static const uint32_t st[5] = { 0x745490, 0x7455B0, 0x7455F0, 0x745610, 0x745640 };
		States(n, st);
		const uint16_t spin = (uint16_t)(U16(n, 0x5E) + U16(n, 0x48));
		U16(n, 0x1C) = (uint16_t)(U16(n, 0x1C) + U16(n, 0x54));
		U16(n, 0x48) = (uint16_t)(spin & 0xFFF);
	}
	static void Draw7452F0(uint32_t n) { s_745380(n); }
	static void Step745840(uint32_t n)
	{
		static const uint32_t st[5] = { 0x7459D0, 0x745A20, 0x745A60, 0x745A80, 0x745AB0 };
		States(n, st);
		U16(n, 0x48) = (uint16_t)((U16(n, 0x5E) + U16(n, 0x48)) & 0xFFF);
		const uint32_t sc = U32(n, 0x38) + 0x100;
		U32(n, 0x38) = sc;
		U32(n, 0x34) = sc;
		U32(n, 0x30) = sc;
	}
	static void Draw745840(uint32_t n) { a_7458E0(n); }
	// 0x7433C0 bounding-box sparkle / 0x744AA0: 4 damped particles drawn with s_7434C0
	static void Step7433C0(uint32_t n) { static const uint32_t st[3] = { 0x743B90, 0x743C00, 0x743C50 }; States(n, st); Damp4(n); }
	static void Step744AA0(uint32_t n) { static const uint32_t st[3] = { 0x744BA0, 0x744BE0, 0x744C20 }; States(n, st); Damp4(n); }
	// 0x744C30 splash: gravity particles, only particle 0 drawn (s_744D30 0x3000)
	static void Step744C30(uint32_t n)
	{
		static const uint32_t st[4] = { 0x744E80, 0x744ED0, 0x744F30, 0x744F70 };
		States(n, st);
		for (int i = 0; i < 4; i++)
		{
			const uint32_t v = n + 0x8C + 8 * i, p = n + 0x6C + 8 * i;
			U16(v, 2) = (uint16_t)(U16(v, 2) + 0xC0);
			U16(v, 0) = (uint16_t)(U16(v, 0) - (uint16_t)((int32_t)S16(v, 0) / 16));
			U16(v, 4) = (uint16_t)(U16(v, 4) - (uint16_t)((int32_t)S16(v, 4) / 16));
			for (int k = 0; k < 3; k++) U16(p, 2 * k) = (uint16_t)(U16(p, 2 * k) + (uint16_t)((int32_t)S16(v, 2 * k) / 16));
		}
	}
	static void Draw744C30(uint32_t n) { AtParticle(n, 0); s_744D30(n, 0x3000); }
	// 0x7466A0 splash points (sprite per point; s_7434C0 from script phase 5, else s_744D30 0x400)
	static void Step7466A0(uint32_t n) { static const uint32_t st[3] = { 0x746760, 0x746780, 0x7467A0 }; States(n, st); }
	static void Draw7466A0(uint32_t n)
	{
		for (int i = 0; i < 4; i++)
		{
			AtParticle(n, i);
			U32(n, 0x4C) = i == 0 ? 0x15334A0u : i == 1 ? 0x1533328u : 0x15331D0u;
			if (a_73B7E0(5) != 0) s_7434C0(n);
			else s_744D30(n, 0x400);
		}
	}
	// 0x746FD0 droplets / 0x747220 bubbles: 4 damped particles drawn with s_744D30
	static void Step746FD0(uint32_t n) { static const uint32_t st[3] = { 0x747110, 0x747130, 0x747150 }; States(n, st); Damp4(n); }
	static void Draw746FD0(uint32_t n) { for (int i = 0; i < 4; i++) { AtParticle(n, i); U32(n, 0x4C) = i == 0 ? 0x15336C4u : 0x15334A0u; s_744D30(n, 0x400); } }
	static void Step747220(uint32_t n) { static const uint32_t st[3] = { 0x747320, 0x747340, 0x747360 }; States(n, st); Damp4(n); }
	static void Draw747220(uint32_t n) { for (int i = 0; i < 4; i++) { AtParticle(n, i); s_744D30(n, 0x800); } }
	// 0x743DD0 / 0x7441D0 orbit sparkles, 0x744570 key-path sparkle (the orbit matrix of the drawn tick)
	static void Step743DD0(uint32_t n) { static const uint32_t st[3] = { 0x743FD0, 0x744010, 0x744040 }; States(n, st); OrbitSave sv; OrbitNextBegin(sv); OrbitPlace(n, 0x258E9A0, 0x80, 0x2000, true); OrbitNextEnd(sv); }
	static void Step7441D0(uint32_t n) { static const uint32_t st[3] = { 0x7443C0, 0x744400, 0x744430 }; States(n, st); OrbitSave sv; OrbitNextBegin(sv); OrbitPlace(n, 0x258E8A0, 0, 0x2000, true); OrbitNextEnd(sv); }
	static void Step744570(uint32_t n) { static const uint32_t st[3] = { 0x744730, 0x744770, 0x7447A0 }; States(n, st); OrbitSave sv; OrbitNextBegin(sv); OrbitPlace(n, 0x258BAA0, 0, 0x1000, false); OrbitNextEnd(sv); }
	static const HeldNodeTask NODE_TASKS[] = {
		{ Queue5EF0, 0x7452F0, 0x64, Step7452F0, Draw7452F0, NT_POS | NT_ANG | NT_SCALE },
		{ Queue5EF0, 0x745840, 0x64, Step745840, Draw745840, NT_POS | NT_ANG | NT_SCALE },
		{ QueueFA90, 0x7433C0, 0xE0, Step7433C0, Draw4Sprites, NT_PART4 },
		{ QueueFA90, 0x744AA0, 0xE0, Step744AA0, Draw4Sprites, NT_PART4 },
		{ QueueFA90, 0x744C30, 0xE0, Step744C30, Draw744C30, NT_PART4 },
		{ QueueFA90, 0x7466A0, 0xE0, Step7466A0, Draw7466A0, NT_PART4 },
		{ QueueFA90, 0x746FD0, 0xE0, Step746FD0, Draw746FD0, NT_PART4 },
		{ QueueFA90, 0x747220, 0xE0, Step747220, Draw747220, NT_PART4 },
		{ QueueFA90, 0x743DD0, 0xE0, Step743DD0, Draw4Sprites, NT_PART4 },
		{ QueueFA90, 0x7441D0, 0xE0, Step7441D0, Draw4Sprites, NT_PART4 },
		{ QueueFA90, 0x744570, 0xE0, Step744570, Draw4Sprites, NT_PART4 },
		{ nullptr, 0 } };
	// the water surfaces 0x745AD0 (queue 0x257FA90, 0xE0 bytes): states 2 grow / 3 hold / 4 fade run
	// s_745F50, which steps the surface block (rows +1 up to 10, V scroll -0x10 texels, wave phases
	// +0x80) then builds the wave grid and draws it (and may spawn a splash); the fade state then
	// steps its colours. The held frame runs s_745F50 on copies of the node and the block set so that
	// its step lands half way: rows of the drawn tick (the next tick's at fraction 1), V scroll and
	// wave phases half way to the next tick's, colours half way from the ones the tick drew with (its
	// fade came after its draw: the colours the previous tick left, noted at the previous held frame)
	// to the next tick's; the node copy's frame counter parity keeps it from spawning. A surface that
	// did not draw on the tick (its step state noted then) or will not on the next (fraction 1) is not
	// drawn.
	struct WaterNote { uint32_t node, tick; int8_t state, prev_state; bool prev; uint8_t col[12], prev_col[12]; };
	static WaterNote g_water[8];
	static WaterNote *WaterNoteOf(uint32_t node)
	{
		WaterNote *free_slot = nullptr;
		for (WaterNote &w : g_water)
		{
			if (w.node == node && w.tick + 2 >= g_real_tick) return &w;
			if (!free_slot && (w.node == 0 || w.tick + 2 < g_real_tick)) free_slot = &w;
		}
		if (free_slot) { memset(free_slot, 0, sizeof(*free_slot)); free_slot->node = node; free_slot->tick = 0xFFFFFFF0u; }
		return free_slot;
	}
	static bool WaterDraws(int8_t st) { return st >= 2 && st <= 4; }
	static void WaterHeld(uint32_t node, int num, int den)
	{
		WaterNote *w = WaterNoteOf(node);
		const uint32_t surf = s6_surface(node);
		if (!w) return;
		if (w->tick != g_real_tick)
		{
			w->prev = w->tick + 1 == g_real_tick;
			w->prev_state = w->state;
			memcpy(w->prev_col, w->col, sizeof(w->col));
			w->tick = g_real_tick;
			w->state = S8(node, 0x29);
			memcpy(w->col, (const void *)(surf + 0x44), sizeof(w->col));
		}
		const bool drew = w->prev ? WaterDraws(w->prev_state) : (w->state == 3 || w->state == 4);
		const bool draws_next = WaterDraws(w->state);
		const bool at_next = num >= den;
		if (at_next ? !draws_next : !drew) return;
		const int f = draws_next ? num : 0, fd = den; // (a surface ending on the next tick holds)
		alignas(4) static uint8_t blk[0xD4], nd[0xE0];
		memcpy(blk, (const void *)surf, sizeof(blk));
		memcpy(nd, (const void *)node, sizeof(nd));
		const uint32_t b = P(blk);
		// the step of s_745F50 applied to the block copy: rows + 1 (clamped 10), V -0x1000, phases +0x80
		const int32_t rows = S16(surf, 0xCC);
		U16(b, 0xCC) = (uint16_t)(at_next ? rows : rows - 1);
		const uint32_t vs = U32(surf, 0xC0) & 0x7F00;
		U32(b, 0xC0) = (vs - (((uint32_t)(0x10 * f / fd)) << 8)) + 0x1000;
		for (int k = 0; k < 2; k++)
			U16(b, 0xCE + 2 * k) = (uint16_t)((U16(surf, 0xCE + 2 * k) + 0x80 * f / fd - 0x80) & 0xFFF);
		const uint8_t *c0 = w->prev ? w->prev_col : w->col;
		for (int k = 0; k < 12; k++)
			if ((k & 3) != 3) U8(b, 0x44 + k) = (uint8_t)lerp_i(c0[k], w->col[k], f, fd);
		// frame counter parity: no splash spawn from the held draw
		if (((U8(P(nd), 0xDE) + U8(P(nd), 0x24)) & 1) == 0) U8(P(nd), 0x24) = (uint8_t)(U8(P(nd), 0x24) + 1);
		s_745F50(P(nd), b);
	}
	static void WaterFrame(int num, int den)
	{
		for (uint32_t node = MEM<uint32_t>(0x257FA90); node; node = U32(node, 4))
			if (U32(node, 8) == 0x745AD0 && U16(node, 0x24) != 0)
				WaterHeld(node, num, den);
	}
	// module globals 0x257F8A0..0x258FCF4 (task pools, arenas, script/camera state, key tables)
	// the actor systems of the prim-model tasks 0x73F900 / 0x745180 (state 1 = the module's step
	// s_740470; its model actors are drawn by the original 0x740560, as s_740500 does)
	static void ModelDraw(uint32_t act, uint32_t def) { callo(0x740560, act, def); }
	static const HeldActorSys ACTOR_SYS[] = { { 0x73F900, ModelDraw }, { 0x745180, ModelDraw }, { 0 } };
	static const HeldDesc HELD = { &MOD_095, 0x258EC40, 0x746B60, 0x258BFB0, 0x257F8A0, 0x258FCF4, nullptr, nullptr, ACTOR_SYS, NODE_TASKS, WaterFrame, 0x258BE20 };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { g_orbit_num = num; g_orbit_den = den; held_frame(HELD, num, den); }
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
		mark_held(0x8DDC30); // the screen tints (shared engine task; redrawn by act::held_draw_tints)
		register_module_held(95, act::siren::HeldReady, act::siren::HeldFrame);
		register_module_camera(95, act::siren::HeldCamera);
	}
}
