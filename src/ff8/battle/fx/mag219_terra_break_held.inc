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

// 30 fps part of mag219_terra_break.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
// VM 0x0AD places a bone at a joint of the slot entity: the prediction of the next tick takes the
// joint from the entity's NEXT pose (held_next_pose, as Ice Strike 228). VM 0x0B3 / 0x12E run on
// real ticks and in the prediction (0x12E's action result skipped there by its wrapper).
// Draw 67 (FoggedGridSheet) draws the in-between bone (grid offset, height, parent matrix).
// Draw 49 (StageShatter) has its own held path (below).

namespace ff8fx
{
namespace gfc
{
namespace terra219
{
	// while predicting: the bone matrices of the model whose BattleAnimHeader is hdr (animation
	// command cmd) at its next pose; the skeleton block (root, poses, matrices: 16 + 48 bytes per
	// bone) journaled first (Ice Strike 228's)
	static void held_next_pose(uint8_t *hdr, uint8_t *cmd)
	{
		uint8_t *com = hdr ? PTR(hdr, 4) : nullptr;
		uint8_t *sk = com ? PTR(com, 0) : nullptr;
		if (!sk || U8(sk, 0) == 0) return;
		guard(sk, 16 + 48 * (int)U8(sk, 0));
		pose_midpoint(hdr, cmd, 1, 1);
	}

	// ------------------------------------------------------------------------------------
	// Draw 49 StageShatter on held frames (dh_49_StageShatter -> TbHeldDraw): no launch (it draws random
	// numbers: the polygons a real tick launches leave the model on that tick); the model drawn as the
	// draw finds it (the engine rebuilds its bone matrices from the pose, composed with the in-between
	// parent matrix), its skeleton put back after; every shard at the exact in-between of its position /
	// angles N and the next step (Y + speed << 4, angles + rates), in the colour the real tick drew it
	// (the next tick's at the full step); held at N when the real tick was frozen (the step is gated by
	// the freeze), the engine is frozen, or the shard ends on the next tick (life < 0 after its step).
	// ------------------------------------------------------------------------------------
	static inline bool TbEngineFrozen() { return U8(CTX(), 0x35) != 0; }
	static uint8_t g_tb_skel[0x10 + 0x30 * 256];

	// the shard moves on the next tick (and the held frame shows part of that step)
	static inline bool TbShardSteps(const uint8_t *s)
	{
		return g_held.real_skip == 0 && !TbEngineFrozen() && g_held.num > 0 && S8(s, 7) >= 0;
	}
	// the shards the last real draw stepped (drawn and aged on that tick): a dead shard keeps life -1
	// without a step, so the pool alone does not tell a shard drawn with life 0 on the real tick from one
	// dead before it (real ticks only, never while predicting: draws do not run there)
	static struct { const uint8_t *pool; uint8_t stepped[0x400]; } g_tb_memo = {};
	static void TbNoteBegin(const uint8_t *pool)
	{
		g_tb_memo.pool = pool;
		memset(g_tb_memo.stepped, 0, sizeof(g_tb_memo.stepped));
	}
	static void TbNoteStep(const uint8_t *s)
	{
		if (g_held.active || g_tb_memo.pool == nullptr) return;
		const uint32_t i = (uint32_t)(s - g_tb_memo.pool) / 0x2C;
		if (i < 0x400) g_tb_memo.stepped[i] = 1;
	}
	static bool TbStepped(const uint8_t *s)
	{
		if (g_tb_memo.pool == nullptr || s < g_tb_memo.pool) return false;
		const uint32_t i = (uint32_t)(s - g_tb_memo.pool) / 0x2C;
		return i < 0x400 && g_tb_memo.stepped[i] != 0;
	}
	// the life index the held frame draws: the real tick's (before its step) for a shard the real tick
	// stepped, the next tick's at the full step; a shard the real tick did not step (frozen, dead) keeps its
	static int8_t TbHeldLife(const uint8_t *s)
	{
		if (!TbStepped(s)) return S8(s, 7);
		if (g_held.num >= g_held.den && !TbEngineFrozen()) return S8(s, 7);
		return (int8_t)(S8(s, 7) + 1);
	}
	static void TbHeldShard(const uint8_t *s, int16_t ang[3], int16_t pos[3])
	{
		if (!TbShardSteps(s)) return;
		for (int k = 0; k < 3; k++)
			ang[k] = (int16_t)(ang[k] + (int32_t)(S8(s, 0x13 + 8 * k) << 4) * g_held.num / g_held.den);
		pos[1] = (int16_t)(pos[1] + (int32_t)(int16_t)(S8(s, 9) << 4) * g_held.num / g_held.den);
	}
	static uint8_t *TbHeldDraw(uint8_t *e, uint8_t *pool)
	{
		uint8_t *m = (U8(e, 1) & 1) ? PTR(e, 0x18) : nullptr;
		uint8_t *sk = m ? PTR(m, 0) : nullptr;
		const uint32_t n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		if (sk) memcpy(g_tb_skel, sk, n);
		uint8_t *r = Draw_AC2EA0(e, pool);
		if (sk) memcpy(sk, g_tb_skel, n);
		return r;
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag219_terra_break_held()
	{
		using namespace gfc;
		mark_held(0xABDA90);
		// the entity-joint VM ops (0x05B / 0x060 / 0x06E / 0x075) read a joint of an animating battle
		// entity: at its next pose while predicting (gfc_engine_held.inc held_entity_next_pose)
		held_entity_joints(terra219::g_c);
		register_module_held(219, terra219::Ready, terra219::Frame);
		register_module_camera(219, terra219::Camera);
	}
}
