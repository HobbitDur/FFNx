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

// 30 fps part of mag234_fart.cpp: Fart's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form. Camera: HeldCamera interpolates the
// battle camera words the VM writes (op 0x039).
//
// Draw 30 (WobbleEntity) runs as vanilla on a held frame: the wobble table follows the in-between
// reference node (amplitude / phase / step = outPos), the model is drawn at the in-between camera
// node. Embedded-model path (param flag bit 0): the model block is registered as a model draw
// (held_model_draws), so its skeleton is posed at the exact in-between pose and put back. Live
// entity path: vanilla composes the entity skeleton (and its weapon's) IN PLACE with the camera
// node, so after a real tick the skeleton holds world matrices; the real tick records the
// skeletons as they were before the composition (the entity's pose of that tick, built by the
// battle), a held draw starts from that record (the pose holds, as the battle drives it) and
// everything it writes outside the saved sets - the skeletons, the battle entity record (flag +0
// on a first draw, fields written by the entity render 0x5088A0), the packet cursor global of the
// object renderer 0x5106E0 (dword_1D99C18) - is put back after it.

namespace ff8fx
{
namespace gfc
{
namespace fart
{
	static const uint32_t ENTITY_BASE = 0x1D972C0, ENTITY_SIZE = 0x9C, ENTITY_COUNT = 7;
	static const uint32_t SKEL_MAX = 0x10 + 0x30 * 256;

	// skeleton of a BattleAnimHeader (hdr+4 -> model, model+0 -> skeleton: u8 bone count, bone
	// matrices from +0x20, 0x30 each): the region 0x5095B0 composes
	static uint8_t *skeleton_of(uint8_t *hdr, uint32_t *n)
	{
		uint8_t *m = hdr ? PTR(hdr, 4) : nullptr;
		uint8_t *sk = m ? PTR(m, 0) : nullptr;
		*n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		return sk;
	}

	// the skeletons before the composition on the real tick (index 0 = entity, 1 = weapon)
	struct LocalMemo { uint32_t tick; uint8_t *sk; uint32_t n; uint8_t m[SKEL_MAX]; };
	static LocalMemo g_local[2] = { { 0xFFFFFFFF }, { 0xFFFFFFFF } };
	// held draw: what the draw overwrites, put back at its end
	struct Save { uint8_t *p; uint32_t n; uint8_t m[SKEL_MAX]; };
	static Save g_save_sk[2];
	static uint8_t *g_save_ent = nullptr;
	static uint8_t g_save_rec[ENTITY_SIZE];
	static bool g_save_on = false;
	static uint32_t g_save_cursor = 0;   // dword_1D99C18: packet cursor of 0x5106E0 (left at its end)

	static void held_entity_begin(uint8_t *ent)
	{
		g_save_ent = nullptr;
		g_save_sk[0].n = g_save_sk[1].n = 0;
		g_save_on = g_held.active;
		if (!g_held.active) return;
		g_save_cursor = MEM<uint32_t>(0x1D99C18);
		const uint32_t a = (uint32_t)ent;
		if (a < ENTITY_BASE || a >= ENTITY_BASE + ENTITY_SIZE * ENTITY_COUNT) return;
		g_save_ent = ent;
		memcpy(g_save_rec, ent, ENTITY_SIZE);
	}

	static void held_local_skeletons(uint8_t *hdr, uint8_t *weapon)
	{
		uint8_t *h[2] = { hdr, weapon };
		for (int i = 0; i < 2; i++)
		{
			uint32_t n;
			uint8_t *sk = skeleton_of(h[i], &n);
			if (!sk || n > SKEL_MAX) continue;
			if (!g_held.active)
			{
				g_local[i].tick = g_real_tick;
				g_local[i].sk = sk;
				g_local[i].n = n;
				memcpy(g_local[i].m, sk, n);
				continue;
			}
			Save &s = g_save_sk[i];
			s.p = sk;
			s.n = n;
			memcpy(s.m, sk, n);
			if (g_local[i].tick == g_real_tick && g_local[i].sk == sk && g_local[i].n == n) memcpy(sk, g_local[i].m, n);
		}
	}

	static void held_entity_end()
	{
		for (int i = 1; i >= 0; i--)
		{
			if (g_save_sk[i].n) memcpy(g_save_sk[i].p, g_save_sk[i].m, g_save_sk[i].n);
			g_save_sk[i].n = 0;
		}
		if (g_save_ent) memcpy(g_save_ent, g_save_rec, ENTITY_SIZE);
		g_save_ent = nullptr;
		if (g_save_on) MEM<uint32_t>(0x1D99C18) = g_save_cursor;
		g_save_on = false;
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag234_fart_held()
	{
		using namespace gfc;
		held_model_draws(fart::g_c, 30);
		mark_held(0xA48F60);
		register_module_held(234, fart::Ready, fart::Frame);
		register_module_camera(234, fart::Camera);
	}
}
