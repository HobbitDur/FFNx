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

// Effect 34: Pain (spell, MAG_034_*): a copy of the actor/heal effect library (see act_engine.h).
//
// Setup MAG_034_PAIN_Init 0x865600 (runs once, not ported; file loader 0x8655E0 = the prim-model
// file, its two packet arenas at file + 0xB800 / + 0x17000) creates the root queue 0x26BAAD0 with
// the master and two task pools: 0x26C0118 (4 nodes of 0x58 bytes: emitters) and 0x26C0100 (10 nodes
// of 0x89C bytes: prim-model players).
//   Master (0x865720) - copies the camera matrix to 0x2793E58, picks the packet arena by tick parity,
//     follows the target's bones, runs its 11-state table (one emitter per action: 0x865870 spawns
//     it, engine loop 0x865F40 repeats for the next action once the emitter releases the master's
//     +0x63 hold), runs the two queues, ends when nothing is left.
//   Emitter (0x8658A0), one per action: bone-follow anchor + model bounds (CURE_Emitter helpers),
//     sound at its first tick; state 0 spawns the two prim-model players (0x865930), state 1 lets
//     the master go on at tick 20, state 2 applies the damage (tick >= 20) and finishes.
//   Prim-model players (0x8659D0 model 0x15E7040, 0x865DB0 model 0x15EA2E8; the same code twice,
//     engine task body a_73A380): decode the model layout and take the target's
//     bone 0xF1 as position (0x865A30), then play the model with the shared player 0x701970 and the
//     draw callback 0x865AE0 until it ends (0x865A80). The first one draws with the engine's
//     second prim renderer 0x8DE9D0 (header flag 0x400), the second with Effect_RenderPrimModel.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26BA9E0..0x26C0380 (file pointer, packet cursor 0x26BA9E4, pools, queues,
// the player's vertex blend buffer 0x26C0298), camera copy 0x2793E58 (0x20 bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace pain
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_034 = { "pain", 34, 0x8655E0, 0x865FF0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26C0118, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8658A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26BA9E4;   // module packet cursor (the draw callback's arena)
	static const uint32_t Q_EMITTER = 0x26C0118;       // emitter queue (0x58-byte nodes)
	static const uint32_t Q_PLAYER = 0x26C0100;        // prim-model player queue (0x89C-byte nodes)
	static const uint32_t BLEND_BUFFER = 0x26C0298;    // vertex frames blended by MAG_017_sub_701390
	static uint32_t g_block40 = 0;                     // stack residue of the players' block +0x40 (PlayerPlay)

	static const uint32_t ORIG_Master = 0x865720;
	static const uint32_t ORIG_Emitter = 0x8658A0;
	static const uint32_t ORIG_GlowPlayer = 0x8659D0;
	static const uint32_t ORIG_ModelPlayer = 0x865DB0;
	static const uint32_t ORIG_DrawCallback = 0x865AE0;
	static const void *const SOUND_Pain = (const void *)0x15E703C;

	// engine prim renderer variant (glow pass): same header as Effect_RenderPrimModel
	inline uint32_t RenderPrimModelGlow(uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x8DE9D0)(hdr, ot, mode, cursor); }

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
}
}
}

#ifdef FF8_FX_HELD
#include "mag034_pain_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace pain
{
	// 0x865720 (MAG_034_PAIN_Tick): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the two queues (live task count -> node +0x5E)
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_034;
		// 30 fps layer: see mag034_pain_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x26C0374) = 0x2793E58;
		MEM<uint32_t>(0x26BAA00) = 0x2793E58;
		states[0] = 0x865840;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x865850;
		states[2] = 0x865860;
		states[3] = 0x865870;
		states[4] = 0x865F40;
		states[5] = 0x865F80;
		states[6] = 0x865F90;
		states[7] = 0x865FA0;
		states[8] = 0x865FB0;
		states[9] = 0x865FC0;
		states[10] = 0x865FE0; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26C028C);
			const uint32_t v2 = MEM<uint32_t>(0x26C0114);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26BA9FC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26C0288);
			const uint32_t v2 = MEM<uint32_t>(0x26C0110);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26BA9FC) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		// the emitter pass leaves a non-1 word at the players' block +0x40 stack slot (see PlayerPlay)
		g_block40 = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PLAYER));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x865860 (MAG_034_sub_865860): master state - wait until no child is alive, next state
	static uint32_t __cdecl WaitChildren(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8658A0 (MAG_034_sub_8658A0): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x865930 spawn players, 0x865ED0 release the master, 0x865EF0 damage, ret},
	// sound at its first tick
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x865930;
		states[1] = 0x865ED0;
		states[2] = 0x865EF0;
		states[3] = 0x865F30; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Pain), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x865980 (MAG_034_sub_865980): spawns a prim-model player task `fn` (0x89C bytes, player queue)
	// under `parent`: +0x74 model data, +0x78 animation word (sign-extended), +0x80 / +0x82 modes
	static uint32_t SpawnPlayer(uint32_t parent, uint32_t fn_addr, uint32_t model, uint32_t anim, uint32_t w80, uint32_t w82)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, fn_addr, 0x89C, parent);
		U16(t, 0x80) = (uint16_t)w80;
		U32(t, 0x74) = model;
		S32(t, 0x78) = (int16_t)anim;
		U16(t, 0x82) = (uint16_t)w82;
		return t;
	}

	// 0x865930 (MAG_034_PAIN_SpawnVisualModels): emitter state 0 - the glow model player (0x15E7040)
	// and the model player (0x15EA2E8), next state
	static uint32_t __cdecl SpawnModels(uint32_t a1)
	{
		SpawnPlayer(a1, ORIG_GlowPlayer, 0x15E7040, 0x808, 0, 1);
		SpawnPlayer(a1, ORIG_ModelPlayer, 0x15EA2E8, 0x280, 1, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x865A30 / 0x865E10: player state 0 - decode the model's prim layout into +0x94, position =
	// the target's effect bone 0xF1, next state
	static uint32_t __cdecl PlayerInit(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		const uint32_t anim = U32(node, 0x78);
		const uint32_t entity = slot * 0x9C + 0x1D972C0;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, anim);
		x::GetEffectSpawnPosition(entity, 0xF1, 0, node + 0x1C);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x865A80 / 0x865E60: player state 1 - plays the model (shared player 0x701970, callback
	// 0x865AE0) with a parameter block on the stack: +0x20 position words (node +0x1C..+0x23),
	// +0x40 glow flag (written only when node +0x82 == 1, else the stack's residue: see the note
	// on the frame below), +0x48 blend buffer. When the model has ended: finished, next state.
	//
	// Block +0x40 of the model player (+0x82 == 0) is never written: 0x865E60 reads what the
	// last function run at that stack depth left in [esp+0x44]. Both state functions (0x865A80 /
	// 0x865E60: sub esp 0x5C, push esi, block = esp+4) and both task bodies (0x8659D0 / 0x865DB0:
	// sub esp 0xC, push esi, push node, call state) have identical frames, the executor 0x508420
	// calls every task of the queue at the same esp without touching the stack in between, and
	// AddTaskToQueue 0x508360 appends: the glow player (spawned first by 0x865930) plays right
	// before its model player and leaves 1 there (the player 0x701970 it calls lives below that
	// frame). A glow player is released on the tick of its last play (which still writes the 1),
	// and it ends before its model player, so every model play reads 1: the model is drawn by the
	// glow renderer too (original run with the game's executor: every model player packet is the
	// glow renderer's). Before the player queue, the emitter pass leaves a non-1 word there
	// (MAG_001_CURE_Emitter_ComputeModelBounds 0x8DC870 pushes the executor's edi into that slot).
	static uint32_t __cdecl PlayerPlay(uint32_t a1)
	{
		const uint32_t node = a1;
		// stack frame 0x5C: the callback reads +0x20..+0x27, +0x40 and +0x48 (not +0x00..+0x1F);
		// +0x40 of the second player (+0x82 == 0) is the stack residue (g_block40, see the note above)
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		U32(L, 0x48) = BLEND_BUFFER;
		U32(L, 0x20) = U32(node, 0x1C);
		U32(L, 0x24) = U32(node, 0x20);
		U32(L, 0x40) = U16(node, 0x82) == 1 ? 1 : g_block40;
		g_block40 = U32(L, 0x40);
		if (prim_play(node + 0x94, ORIG_DrawCallback, L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x865AE0 (sub_865AE0): prim-model player draw callback (layout a1, record a2, block a3 of
	// PlayerPlay) - picks the object's vertex frame (blended by MAG_017_sub_701390 between two
	// frames), builds its matrix (record rotation, position through the camera, scale), sets the
	// fade and draws it into the module arena: glow renderer 0x8DE9D0 when block +0x40 == 1, else
	// Effect_RenderPrimModel
	static uint32_t __cdecl DrawCallback(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x38 bytes, offsets as in the original frame after its three pushes): +0x10
		// SVECTOR position, +0x18 scale (3 x int32), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void (full fade to black: nothing drawn)

		const uint32_t hdr = x::Field_Alloc(0x58);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);   // second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // first vertex frame
		U32(hdr, 0) = base;
		// vertex frame f: base + 0xC + f * vertex_count * 8
		auto frame_ptr = [base](int16_t f) -> uint32_t {
			if (f == 0)
				return base + 0xC;
			const int32_t n = mul32(S32(base, 4), (int32_t)f);
			return base + (uint32_t)shl32(n, 3) + 0xC;
		};
		if (f0 == f1)
			U32(hdr, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);   // blend factor 0..0x1000
			if (t == 0)
				U32(hdr, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(hdr, 4) = frame_ptr(f1);
			else
			{
				x::MAG_017_sub_701390(base, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, U32(blk, 0x48));
				U32(hdr, 4) = U32(blk, 0x48);
			}
		}

		x::MAG_017_sub_701310(rec + 0x10, M);
		const uint32_t flags = U32(rec, 4);
		uint16_t vx = U16(rec, 8);
		uint16_t vy = U16(rec, 0xA);
		uint16_t vz = U16(rec, 0xC);
		U16(L, 0x10) = vx;
		U16(L, 0x12) = vy;
		U16(L, 0x14) = vz;
		if ((flags & 0x200) != 0)
		{
			// offset rotated by the camera, object rotation kept (not composed with the camera)
			x::GTE_SetRotMatrix(0x1D97778);
			x::GTE_LoadV0(blk + 0x20);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			S32(M, 0x14) = add32(S32(M, 0x14), S16(L, 0x10));
			S32(M, 0x18) = add32(S32(M, 0x18), S16(L, 0x12));
			S32(M, 0x1C) = add32(S32(M, 0x1C), S16(L, 0x14));
		}
		else
		{
			// block position + record offset, through the camera; object rotation composed
			vx = (uint16_t)(vx + U16(blk, 0x20));
			vy = (uint16_t)(vy + U16(blk, 0x22));
			vz = (uint16_t)(vz + U16(blk, 0x24));
			U16(L, 0x10) = vx;
			U16(L, 0x12) = vy;
			U16(L, 0x14) = vz;
			x::GTE_SetRotMatrix(0x1D97778);
			x::GTE_LoadV0(L + 0x10);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(0x1D97778, M);
		}
		S32(M, 0x14) = add32(S32(M, 0x14), MEM<int32_t>(0x1D9778C));
		S32(M, 0x18) = add32(S32(M, 0x18), MEM<int32_t>(0x1D97790));
		S32(M, 0x1C) = add32(S32(M, 0x1C), MEM<int32_t>(0x1D97794));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(L, 0x18) = S16(rec, 0x18);
			S32(L, 0x1C) = S16(rec, 0x1A);
			S32(L, 0x20) = S16(rec, 0x1C);
			x::scale3DMatrix(M, L + 0x18);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const int32_t fade = S16(rec, 0x24);
		U32(hdr, 0x1C) = 0x2030;
		U32(hdr, 0xC) = (uint32_t)fade;
		if (fade != 0)
		{
			U32(hdr, 8) = U32(rec, 0x20);   // fade colour
			U32(hdr, 0x1C) = 0x20F0;
		}
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		if (U32(blk, 0x40) == 1)
		{
			U32(hdr, 0x1C) |= 0x400;
			U32(hdr, 0x10) = 0;
			MEM<uint32_t>(PACKET_CURSOR) = RenderPrimModelGlow(hdr, ot, 2, MEM<uint32_t>(PACKET_CURSOR));
		}
		else
			MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	// 0x865ED0 (MAG_034_sub_865ED0): emitter state 1 - at tick 20 releases the master (root +0x63 =
	// 0: its action loop may spawn the next emitter), next state
	static uint32_t __cdecl ReleaseMaster(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x14)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x865EF0 (MAG_034_sub_865EF0): emitter state 2 - from tick 20: damage of its target, finished,
	// next state
	static uint32_t __cdecl Damage(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x14)
			return 0; // void
		const int32_t action = S8(node, 0x2A);
		const int32_t target = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t targets = U32(acts + (uint32_t)(action * 20), 8);
		x::ApplyActionResultToTarget(targets + (uint32_t)(target * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x865840, (void *)a_73A0D0, "034 MAG_034_sub_865840" },
		{ 0x865850, (void *)a_73A0D0, "034 MAG_034_sub_865850" },
		{ 0x865870, (void *)a_73A170, "034 MAG_034_sub_865870" },
		{ 0x8659D0, (void *)a_73A380, "034 MAG_034_sub_8659D0" },
		{ 0x865DA0, (void *)a_73A6D0, "034 nullsub_1653" },
		{ 0x865DB0, (void *)a_73A380, "034 MAG_034_sub_865DB0" },
		{ 0x865EC0, (void *)a_73A6D0, "034 nullsub_1654" },
		{ 0x865F30, (void *)a_73A6D0, "034 nullsub_1655" },
		{ 0x865F40, (void *)a_747500, "034 MAG_034_sub_865F40" },
		{ 0x865F80, (void *)a_73A0D0, "034 MAG_034_sub_865F80" },
		{ 0x865F90, (void *)a_747550, "034 MAG_034_sub_865F90" },
		{ 0x865FA0, (void *)a_73A0D0, "034 MAG_034_sub_865FA0" },
		{ 0x865FB0, (void *)a_73A0D0, "034 MAG_034_sub_865FB0" },
		{ 0x865FC0, (void *)a_7475A0, "034 MAG_034_sub_865FC0" },
		{ 0x865FE0, (void *)a_73A6D0, "034 nullsub_1652" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "034 Pain Master" },
		{ 0x865860, (void *)WaitChildren, "034 WaitChildren" },
		{ ORIG_Emitter, (void *)Emitter, "034 Emitter" },
		{ 0x865930, (void *)SpawnModels, "034 SpawnModels" },
		{ 0x865A30, (void *)PlayerInit, "034 GlowPlayer Init" },
		{ 0x865E10, (void *)PlayerInit, "034 ModelPlayer Init" },
		{ 0x865A80, (void *)PlayerPlay, "034 GlowPlayer Play" },
		{ 0x865E60, (void *)PlayerPlay, "034 ModelPlayer Play" },
		{ ORIG_DrawCallback, (void *)DrawCallback, "034 DrawCallback" },
		{ 0x865ED0, (void *)ReleaseMaster, "034 ReleaseMaster" },
		{ 0x865EF0, (void *)Damage, "034 Damage" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag034_pain()
	{
		act::register_module(34);
		for (const act::pain::ModPort *p = act::pain::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(34, p->addr, p->port, p->name);
		for (const act::pain::ModPort *p = act::pain::PORTS; p->addr; p++)
			act::register_module_port(34, p->addr, p->port, p->name);
		// 30 fps layer: see mag034_pain_held.inc
		FX_HELD(register_mag034_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag034_pain_held.inc"
#endif
