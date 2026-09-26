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

// 30 fps part of mag_leviathan.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_leviathan.cpp)
//
// Held-frame behaviour of the Leviathan handlers (the engine draws the held frames):
//   Draw 17 RippleTextureUpload: returns at once (no VRAM upload, no rect-ring / arena / bone
//     change); the texture the real tick uploaded stays in VRAM.
//   Draw 18 WaterColumn: runs unchanged. Every input is a bone pose value (the dispatcher already
//     set the in-between values) - no counter, timer or rand -> the exact in-between mesh. Its
//     writes go to the packet cursor (private held buffer), ws scratch, the arena (state block +
//     scratch past the arena top) and, on a first draw only, bone+0xBC/+0x92 + an arena
//     allocation: all inside the HeldFrame save set (the scratch past the top is unallocated
//     arena memory that vanilla also overwrites every draw).
//   Prim 2 NormalLitG3: packets only (drawn from the mesh the held draw transformed).
//   Draw 20 / Draw 24: see below.
// Prediction: VM 0x04F OffscreenPass: the block header lies at the arena top (outside
// [ctx+0x70, ctx+0x74) until the allocation) -> guard() before its first write; 0xB65810 (GPU +
// battle model writes) is skipped (xl_OffscreenRender_B65810).

#include <vector>

namespace ff8fx
{
namespace gfc
{
namespace part_lev_b
{
	// ------------------------------------------------------------------------------------
	// held-frame protection of the file data written by these handlers
	// ------------------------------------------------------------------------------------
	static uint8_t g_skel_copy[0x10 + 0x30 * 256];
	static uint8_t *g_skel_ptr = nullptr;
	static uint32_t g_skel_size = 0;

	struct VtxSave { int16_t *p; int16_t v[3]; };
	static std::vector<VtxSave> g_vtx_journal;
	static bool g_vtx_journal_on = false;

	// held draw of the given model block starts: save its skeleton, journal morph writes
	static void HeldBegin(uint8_t *blk)
	{
		g_skel_ptr = PTR(blk, 0x30);
		g_skel_size = 0x10 + (uint32_t)U8(g_skel_ptr, 0) * 0x30;
		memcpy(g_skel_copy, g_skel_ptr, g_skel_size);
		g_vtx_journal.clear();
		g_vtx_journal_on = true;
	}

	// held draw ends: undo the morph writes (newest first), put the skeleton back
	static void HeldEnd()
	{
		g_vtx_journal_on = false;
		for (size_t i = g_vtx_journal.size(); i-- > 0;)
		{
			const VtxSave &s = g_vtx_journal[i];
			s.p[0] = s.v[0];
			s.p[1] = s.v[1];
			s.p[2] = s.v[2];
		}
		g_vtx_journal.clear();
		if (g_skel_ptr) memcpy(g_skel_ptr, g_skel_copy, g_skel_size);
		g_skel_ptr = nullptr;
	}

	// 0xB5A8E0 MorphApply: held frames journal every overwritten vertex (restored by HeldEnd)
	static void held_note_vertex(uint8_t *v)
	{
		if (g_vtx_journal_on)
		{
			VtxSave s;
			s.p = (int16_t *)v;
			s.v[0] = S16(v, 0);
			s.v[1] = S16(v, 2);
			s.v[2] = S16(v, 4);
			g_vtx_journal.push_back(s);
		}
	}

	// Held frames of draw 24: when h_B26AD0 poses the model in-between on this held frame
	// (g_mid24), the renderer's own rebuilds (BuildPose) use the same in-between pose (vanilla
	// rebuilds from the stored pose, which would discard h_B26AD0's). Returns true then.
	static bool g_mid24 = false;

	static bool held_mid24_pose(uint8_t *blk)
	{
		if (!(g_held.active && g_mid24)) return false;
		pose_midpoint(blk + 0x10, blk + 0x20, g_held.num, g_held.den);
		return true;
	}

	// Draw 20 MorphAnimModel, held: never ReadAnimation / PreReadAnimation / counter++; when the
	// next real tick reads a frame (flag bit0 clear, counter != 0) the bone matrices come from
	// the exact in-between pose (pose_midpoint); the morph uses the weight bone's in-between
	// outPos. No block creation on held frames (nothing drawn if bone+0xBC == 0). Skeleton and
	// morphed vertices are put back after the draw (HeldBegin / HeldEnd).
	static void held_pose20(uint8_t *hdr, uint8_t *cmd)
	{
		pose_midpoint(hdr, cmd, g_held.num, g_held.den);
	}

	// Draw 24 AnimModelAltTex, held: h_B26AD0 is held-aware (in-between pose); the renderer's two
	// pose rebuilds per object use the same in-between pose (BuildPose); no extra counter++; no
	// block creation (nothing drawn if bone+0xBC == 0); morphed vertices and skeleton put back
	// after the draw.
	static void held_begin24(uint8_t *cur)
	{
		if (!g_held.active) return;
		uint8_t *hb = PTR(cur, 0xBC);
		HeldBegin(hb);
		// the condition under which h_B26AD0 draws the in-between pose on this held frame
		uint16_t fl = U16(hb, 2);
		g_mid24 = g_held.real_skip == 0 && !(fl & 1) && U16(hb, 0) != 0 && !(fl & 2);
	}

	static void held_end24()
	{
		g_mid24 = false;
		HeldEnd();
	}
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace leviathan
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_leviathan_held()
	{
		using namespace gfc;
		held_model_draws(leviathan::g_c, 20, 24);   // MorphAnimModel (same model block as draw 3), AnimModelAltTex
		mark_held(0xB586F0);
		register_module_held(6, leviathan::Ready, leviathan::Frame);
		register_module_camera(6, leviathan::Camera);
	}
}
