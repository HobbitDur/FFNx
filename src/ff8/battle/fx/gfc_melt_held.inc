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

// 30 fps part of gfc_melt.inc (draw 52 EntityMelt, draw 3 AnimatedModelWithShadow): included INSIDE
// the clone's namespace by the module's _held.inc.
//
// Draw 52 on a held frame runs its vanilla code (MeltFrame) on the engine's in-between state: the
// draw bone's outPos (fade / wave heights, the y offset) and the wave bone's outAngle / outPos
// (amplitude, its increments, phase, phase step) are interpolated by HeldFrame, so the wave is the
// exact in-between one; the model copy is posed at the entity's in-between pose (pose_midpoint of
// the step the battle loop makes between the two ticks; the pose of the real tick when the engine
// ran frozen, ctx+0x35), the shadow too (bone matrices of the in-between pose + world matrices from
// the entity matrix, then 0xB67040 with root 0). The per-polygon life bytes: before the full step
// the ones the real tick's draw started from (noted by MeltNoteFlags), so every polygon shows the
// colour of the real tick (a polygon that starts fading on the held frame's geometry takes
// table[life], the next tick's colour) and polygons that run out on the real tick stay drawn until
// it; at the full step the current ones (what the next tick's draw starts from). Every write lands
// in the saved engine state (arena, workspace, packets) or in the entity record / skeleton, which
// the held draw saves before and puts back after.
//
// Draw 3 is Brothers 205's held draw 3 (mag_brothers_held.inc): the embedded model posed as in
// h_B26AD0 (pose_midpoint when the real tick advanced it), the shadow following the in-between
// model.

	// ------------------------------------------------------------------------------------
	// draw 3 (Brothers' held_note_mid03 / held_shadow03)
	// ------------------------------------------------------------------------------------
	static bool g_mid03 = false;

	// the block's frame counter before h_B26AD0 runs (its frozen path turns 0 into 1)
	static void held_note_mid03()
	{
		g_mid03 = g_held.active && U16(PTR(CUR(), 0xBC), 0) != 0;
	}

	// the shadow of the in-between model; false = the vanilla call
	static bool held_shadow03(uint8_t *hdr, uint8_t *blk, int32_t otspec, int32_t y)
	{
		// held_model_pose: the same test as h_B26AD0's in-between path + the in-between pose
		if (!g_mid03 || !held_model_pose(blk, hdr, blk + 0x20)) return false;
		x::ComputeBonesWorldMatrices(hdr, WS() + 0x80);
		xl_ModelShadow_B67040(hdr, nullptr, otspec, y, 0x80);
		return true;
	}

	// ------------------------------------------------------------------------------------
	// draw 52
	// ------------------------------------------------------------------------------------
	static const uint32_t MELT_ENTITY_SIZE = 0x9C;   // battle entity record (0x1D972C0 + slot * 0x9C)
	static uint8_t g_melt_ent_save[MELT_ENTITY_SIZE];
	static uint8_t g_melt_skel_save[0x10 + 0x30 * 256];

	static inline bool MeltEngineFrozen() { return U8(CTX(), 0x35) != 0; }

	// the life bytes each draw-52 bone's real draw started from (a few bones at most)
	struct MeltLifeN { const uint8_t *fl; uint32_t n; uint8_t b[0x1000]; };
	static MeltLifeN g_melt_n[4];
	static int g_melt_next = 0;

	static MeltLifeN *MeltFind(const uint8_t *fl)
	{
		for (int k = 0; k < 4; k++)
			if (g_melt_n[k].fl == fl) return &g_melt_n[k];
		return nullptr;
	}

	// real draw, before the polygons: the life bytes of bone+0xC4 ((count + 4) & 0xFFC bytes)
	static void MeltNoteFlags()
	{
		if (held_predicting()) return;
		const uint8_t *fl = PTR(CUR(), 0xC4);
		const uint32_t n = ((uint32_t)U16(PTR(CUR(), 0xC0), 0) + 4) & 0xFFC;
		MeltLifeN *e = MeltFind(fl);
		if (!e)
		{
			e = &g_melt_n[g_melt_next];
			g_melt_next = (g_melt_next + 1) & 3;
		}
		e->fl = fl;
		e->n = n;
		memcpy(e->b, fl, n);
	}

	// held draw (MeltVertices_92E830): the model copy's bone matrices at the entity's in-between pose
	static void MeltPoseHeld(uint8_t *hdr)
	{
		uint8_t *ent = PTR(hdr, -4);   // copy+0xC
		if (MeltEngineFrozen()) x::BuildBoneMatricesFromPose(hdr);   // the pose of the real tick
		else pose_midpoint(hdr, ent + 0x6C, g_held.num, g_held.den);
	}

	// 0x92E430 on a held frame: the vanilla draw on the in-between state, life bytes of the real
	// tick before the full step, the entity record + skeleton put back after
	static void dh_52_EntityMeltHeld()
	{
		if (U32(CUR(), 0xBC) == 0) return;   // first draw: on a real tick only
		uint8_t *ent = PTR(PTR(CUR(), 0xBC), 0xC);
		memcpy(g_melt_ent_save, ent, MELT_ENTITY_SIZE);
		uint8_t *m = PTR(ent, 0x64);
		uint8_t *sk = m ? PTR(m, 0) : nullptr;
		const uint32_t sk_n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		if (sk) memcpy(g_melt_skel_save, sk, sk_n);
		// the life bytes (arena: saved and put back by HeldFrame)
		const MeltLifeN *e = MeltFind(PTR(CUR(), 0xC4));
		if (e && g_held.num < g_held.den) memcpy(PTR(CUR(), 0xC4), e->b, e->n);
		uint8_t *blk = MeltFrame_92E430();
		uint8_t *hdr = blk + 0x10;
		uint8_t *root = MeltSlotEntity() + 0x40;
		const int32_t level = S16(CUR(), 0x8C);
		if (MeltEngineFrozen()) xl_ModelShadow_B67040(hdr, root, -1, 0, level);
		else
		{
			pose_midpoint(hdr, ent + 0x6C, g_held.num, g_held.den);
			x::ComputeBonesWorldMatrices(hdr, root);
			xl_ModelShadow_B67040(hdr, nullptr, -1, 0, level);
		}
		if (sk) memcpy(sk, g_melt_skel_save, sk_n);
		memcpy(ent, g_melt_ent_save, MELT_ENTITY_SIZE);
	}
