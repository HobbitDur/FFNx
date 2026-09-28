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

// 30 fps part of mag231_disease_breath.cpp: Disease Breath's held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form. Camera: HeldCamera interpolates the
// battle camera words the VM writes (op 0x039).
//
// Bone 4 (OrbitBetweenBones) only runs inside the engine's next-tick prediction (on the saved
// engine state, put back afterwards); prim 58 (semi-transparent FT4) only writes packets, so it is
// re-run as is by the held draw.
//
// Draw 32 (EntityGlow) runs as vanilla on a held frame: the glow colours follow the in-between
// node values (radius / intensity / phase = outAngle, base colour = the reference node's outPos),
// the model is drawn at the in-between camera node in the entity's current (real) pose; the
// per-vertex distances are fixed at the first draw. Its writes outside every saved set - the
// battle entity record (flag +0 on a first draw, bounds +0x34..+0x3F / +0x92 by 0x5086F0) and the
// entity skeleton (pose + world matrices) - are saved before the held draw and put back after it
// (the copy block, distance block and scratch are in the arena / bone array: HeldFrame).

namespace ff8fx
{
namespace gfc
{
namespace disease231
{
	static const uint32_t ENTITY_SIZE = 0x9C;   // battle entity record (0x1D972C0 + slot * 0x9C)

	struct EntitySave
	{
		uint8_t *ent;
		uint8_t head[ENTITY_SIZE];
		uint8_t *sk;
		uint32_t sk_n;
	};
	static EntitySave g_hs;
	static bool g_hs_on = false;
	static uint8_t g_held_skel[0x10 + 0x30 * 256];

	// the entity record + its skeleton (*(*(ent+0x64)), 0x10 + 0x30 * bone count bytes: the region
	// BuildBoneMatricesFromPose / ComputeBonesWorldMatrices write)
	static void held_entity_begin(uint8_t *ent)
	{
		g_hs_on = g_held.active;
		if (!g_hs_on) return;
		g_hs.ent = ent;
		memcpy(g_hs.head, ent, ENTITY_SIZE);
		uint8_t *p = PTR(ent, 0x64);
		g_hs.sk = p ? PTR(p, 0) : nullptr;
		g_hs.sk_n = g_hs.sk ? 0x10 + 0x30 * (uint32_t)U8(g_hs.sk, 0) : 0;
		if (g_hs.sk) memcpy(g_held_skel, g_hs.sk, g_hs.sk_n);
	}

	static void held_entity_end()
	{
		if (g_hs_on)
		{
			memcpy(g_hs.ent, g_hs.head, ENTITY_SIZE);
			if (g_hs.sk) memcpy(g_hs.sk, g_held_skel, g_hs.sk_n);
		}
		g_hs_on = false;
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag231_disease_breath_held()
	{
		using namespace gfc;
		mark_held(0xA66B10);
		register_module_held(231, disease231::Ready, disease231::Frame);
		register_module_camera(231, disease231::Camera);
	}
}
