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

// 30 fps part of mag_brothers.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_brothers.cpp)
//
// Prediction: VM 0x09D rewrites file vertex data in place (guard()ed), VM 0x0D5 writes the
// Brothers flag byte 0x2796D40 outside the engine block (guard()ed), VM 0x0DA writes 16 bytes at
// the arena top before the allocation (guard()ed; ArenaAlloc journals the rest) and its render
// 0xB65F30 is skipped (xl_OffscreenModelRender_B65F30).

namespace ff8fx
{
namespace gfc
{
namespace part_brothers
{
	// ------------------------------------------------------------------------------------
	// 0xAF5810 (Draw 3 AnimatedModelWithShadow), held frames: the embedded model is posed as in
	// h_B26AD0 (pose_midpoint when the real tick advanced it). The shadow: when the model was
	// advanced (same test as h_B26AD0's in-between path) the bone matrices are built from the
	// in-between pose (pose_midpoint) + world matrices from ws+0x80 and 0xB67040 is called with
	// root = 0 (it then neither rebuilds from the real pose nor rebuilds after), so the shadow
	// follows the in-between model; otherwise the vanilla call. The skeleton writes are undone by
	// HeldFrame (draw 3 model blocks are saved).
	// ------------------------------------------------------------------------------------
	static bool g_mid03 = false;

	// h_B26AD0's in-between test (rt+0x45 is forced, its frozen path runs), before it runs
	static void held_note_mid03()
	{
		g_mid03 = false;
		if (g_held.active)
		{
			uint8_t *b = PTR(CUR(), 0xBC);
			uint16_t fl = U16(b, 2);
			g_mid03 = g_held.real_skip == 0 && !(fl & 1) && U16(b, 0) != 0 && !(fl & 2);
		}
	}

	// the shadow of the in-between model; false = the vanilla call
	static bool held_shadow03(uint8_t *hdr, uint8_t *blk, int32_t otspec, int32_t y)
	{
		if (!g_mid03) return false;
		pose_midpoint(hdr, blk + 0x20, g_held.num, g_held.den);
		x::ComputeBonesWorldMatrices(hdr, WS() + 0x80);
		xl_ModelShadow_B67040(hdr, nullptr, otspec, y, 0x80);
		return true;
	}

	// ------------------------------------------------------------------------------------
	// held frames: battle entity bytes + skeleton written by draw handlers 38 / 48 (outside
	// every saved set) are saved before the held draw of the handler and put back after it.
	// Draw 38 AttachBattleEntity: runs as vanilla with the in-between node pose (shadow at the
	// in-between transform), the entity bytes 0..0x23 (flags +0/+1, rotation, position) and the
	// entity skeleton are SAVED before and RESTORED after, so the entity keeps exactly the real
	// tick's state (bone+0xBC/+0xC4 are in the bone array, restored by HeldFrame).
	// Draw 48 DrawBattleEntityCopy: runs as vanilla with the in-between node pose (the copy is
	// drawn at the in-between transform, in the entity's current real pose); entity byte 0 and
	// the entity skeleton are saved before and restored after (arena + bone array: HeldFrame).
	// ------------------------------------------------------------------------------------
	struct EntityHeldSave
	{
		uint8_t *ent;
		uint32_t head_n;
		uint8_t head[0x24];
		uint8_t *sk;
		uint32_t sk_n;
	};
	static uint8_t g_held_skel[0x10 + 0x30 * 256];

	// head_n bytes of the entity + its skeleton (*(*(ent+0x64)), 0x10 + 0x30 * bone count bytes: the
	// region BuildBoneMatricesFromPose / ComputeBonesWorldMatrices write, as HeldFrame's model saves)
	static void held_save_entity(EntityHeldSave &s, uint8_t *ent, uint32_t head_n)
	{
		s.ent = ent;
		s.head_n = head_n;
		memcpy(s.head, ent, head_n);
		uint8_t *p = PTR(ent, 0x64);
		s.sk = p ? PTR(p, 0) : nullptr;
		s.sk_n = s.sk ? 0x10 + 0x30 * (uint32_t)U8(s.sk, 0) : 0;
		if (s.sk) memcpy(g_held_skel, s.sk, s.sk_n);
	}

	static void held_restore_entity(const EntityHeldSave &s)
	{
		memcpy(s.ent, s.head, s.head_n);
		if (s.sk) memcpy(s.sk, g_held_skel, s.sk_n);
	}

	// the save of the handler being drawn (38 / 48 do not nest)
	static EntityHeldSave g_hs;
	static bool g_hs_on = false;

	static void held_entity_begin(uint8_t *ent, uint32_t head_n)
	{
		g_hs_on = g_held.active;
		if (g_hs_on) held_save_entity(g_hs, ent, head_n);
	}

	static void held_entity_end()
	{
		if (g_hs_on) held_restore_entity(g_hs);
		g_hs_on = false;
	}
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace brothers
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_brothers_held()
	{
		using namespace gfc;
		mark_held(0xAF4B90);
		register_module_held(205, brothers::Ready, brothers::Frame);
		register_module_camera(205, brothers::Camera);
	}
}
