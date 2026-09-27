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

// Effect 33: Shell (spell, MAG_033_*): a copy of the actor/heal effect library (see act_engine.h),
// built like Drain (mag039_drain.cpp) / Confuse (mag036_confuse.cpp): same master, emitter, actor
// system and stage-darkening code with the module's own globals and pools, plus a shield model and a
// per-cast scale of the actor particles.
//
// Setup MAG_033_SHELL 0x866010 (runs once, not ported; file loader MAG_033_SHELL_FL 0x865FF0 = the
// texture file mag032.tim -> 0x26C0380, uploaded by the setup, its packet arenas at file + 0 /
// + 0x6000 / + 0xC000, file arena cursor 0x26C30E4 = file + 0xC000) plays the camera animation
// 0x15F2E1C and creates the root queue 0x26C0470 with the master and four task pools: 0x26C2C68
// (4 x 0x58: emitters), 0x26C29C8 (3 x 0x2A4: actor systems), 0x26C21C0 (10 x 0x2EC: shield
// models), 0x26C2C58 (10 x 0x40: stage darkening).
//   Master (0x866150) - camera copy 0x2793E58 (the module scratch stack 0x26C30D8 grows down from
//     it), packet arena by tick parity (cursor 0x26C0384), bone follow, 11-state table: 0x8662C0
//     reserves the actor pools after the arena (0x654 + 0x8340 bytes = 15 particles of 0x6C and 50
//     actors of 0x2A0, cleared) and starts the stage darkening in (0x866350: the stage group words
//     0x1D98992 + k * 0x2C up by 0x100 per tick to 0x400), 0x866440 one emitter per action (engine
//     action loop 0x86B7E0 once the emitter releases the master's +0x63 hold), 0x86B820 after 10
//     more ticks the darkening out (0x86B850: 0x400 down to 0), then waits for the queues to empty.
//   Emitter (0x866470), one per action: bone follow + model bounds (CURE_Emitter helpers), sound
//     0x15EBD68 at its first tick; state 0 spawns the shield model (0x86B340 -> 0x86B390, model
//     0x15EC42C) and the actor system (0x866550 -> 0x8665A0 with the actor data 0x15F2D18, mode
//     0x2D), state 1 releases the master at tick 30, state 2 applies the damage (the status).
//   Shield model (0x86B390): decodes the prim-model layout and takes the target's bone 0xF1 as
//     position and 3 x its height as scale (0x86B3F0: also the actor system's particle scale vector
//     state +0x214), then plays the model every tick with the shared player 0x701970 and the draw
//     callback 0x86B560 (block: camera x yaw x scale x position matrix) until it ends (0x86B4B0).
//   Actor system (0x8665A0 -> 0x866600 / 0x86B310): the engine's particle actors (emitters and
//     particles, sprite sequences 0x868860 / prim models 0x867180 drawn into the module arena) placed
//     by the actor data's key positions (caster / targets). Unlike the engine's copies, the particle
//     spawner 0x869CC0, the anchor placement 0x86AF90 and the instance transform 0x8691C0 scale by
//     the state's scale vector +0x214 (0x1000 from 0x866630, the shield size once the shield starts).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26C0380..0x26C30EC (file pointer 0x26C0380, packet cursor 0x26C0384, pools,
// queues, arenas, the shield's blend buffer 0x26C2DF8, actor state 0x26C30E8, scratch stack pointer
// 0x26C30D8, camera copy pointer 0x26C03A4), the module scratch stack below the camera copy
// 0x2793E58, the stage group words 0x1D98992 + k * 0x2C (and the stage flags cleared by 0x8663B0).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace shell
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_033 = { "shell", 33, 0x865FF0, 0x86B980,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26C03A4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26C29C8, 0x0, 0x26C2C68, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26C2DE2, 0x0, 0x0, 0x26C30D0, 0x0, 0x0, 0x0, 0x26C30E8 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x866630, 0x867090, 0x867120, 0x867180, 0x0, 0x867400, 0x867610, 0x867890, 0x867AD0, 0x867D70, 0x867FA0, 0x868260, 0x8684C0, 0x8687A0, 0x8688E0, 0x868920, 0x869BC0, 0x869C10, 0x869C40, 0x869CC0, 0x0, 0x0, 0x0, 0x0, 0x86ADE0, 0x0, 0x86B280, 0x0, 0x0, 0x0, 0x0, 0x0, 0x866470, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x868860, 0x8689B0, 0x869010, 0x869060, 0x8690F0, 0x8691C0, 0x0, 0x869A70, 0x869AF0, 0x869B20, 0x86A370, 0x86A3A0, 0x86A480, 0x86A5A0, 0x86A6F0, 0x0, 0x86ADB0, 0x86AF90, 0x86B0E0, 0x0, 0x867090, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26C0384;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x26C30E8;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x26C30D8;      // module scratch stack pointer
	static const uint32_t CAMERA_PTR = 0x26C03A4;      // pointer to the camera copy 0x2793E58
	static const uint32_t BLEND_BUFFER = 0x26C2DF8;    // shield vertex frames blended by MAG_017_sub_701390
	static const uint32_t Q_EMITTER = 0x26C2C68, Q_ACTORS = 0x26C29C8, Q_SHIELD = 0x26C21C0, Q_STAGE = 0x26C2C58;
	static const uint32_t ORIG_Master = 0x866150;
	static const uint32_t ORIG_Emitter = 0x866470;
	static const uint32_t ORIG_StageDim = 0x866350;
	static const uint32_t ORIG_ActorSystem = 0x8665A0;
	static const uint32_t ORIG_Shield = 0x86B390;
	static const uint32_t ORIG_ShieldCallback = 0x86B560;
	static const void *const SOUND_Shell = (const void *)0x15EBD68;

	// engine functions not in act::x
	namespace cx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
	}

	// helpers of the library code (as in act_engine.cpp / mag097_boko.cpp / mag039_drain.cpp)
	static inline void e6_copy16(uint32_t dst, uint32_t src)
	{
		U32(dst, 0) = U32(src, 0);
		U32(dst, 4) = U32(src, 4);
		U32(dst, 8) = U32(src, 8);
		U32(dst, 12) = U32(src, 12);
	}
	static inline void e6_rotate_zxy(uint32_t m, uint32_t ang)
	{
		x::MAG_022_sub_8DD770(m);
		if (U16(ang, 4) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(ang, 4));
		if (U16(ang, 0) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(ang, 0));
		if (U16(ang, 2) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ang, 2));
	}
	static inline void e6_rotate_down_vector(uint32_t sp)
	{
		uint32_t v = sp + 0x20;
		U16(sp, 0x22) = 0xF000;
		U16(sp, 0x24) = 0;
		U16(v, 0) = 0;
		x::GTE_SetRotMatrix(sp);
		x::GTE_LoadV0(v);
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(v);
	}
	static inline void e6_rand_mod(uint32_t sp, int32_t off)
	{
		if (U16(sp, off) != 0)
		{
			int32_t r = (int32_t)x::CrtRand();
			U16(sp, off) = (uint16_t)(r % (int32_t)S16(sp, off));
		}
	}
	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	static inline bool b1_out(int16_t v, int16_t hi) { return v < 0 || v > hi; }
	static inline bool b2_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool b2_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}

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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl h_866150(uint32_t a1);
	uint32_t __cdecl h_8662C0(uint32_t a1);
	uint32_t __cdecl h_866310(void);
	uint32_t __cdecl h_8663F0(uint32_t a1);
	uint32_t __cdecl h_866470(uint32_t a1);
	uint32_t __cdecl h_866500(uint32_t a1);
	uint32_t __cdecl h_866600(uint32_t a1);
	uint32_t __cdecl h_866630(uint32_t a1);
	uint32_t __cdecl h_8667F0(uint32_t a1);
	uint32_t __cdecl h_8668D0(void);
	uint32_t __cdecl h_866A30(void);
	uint32_t __cdecl h_866C20(void);
	uint32_t __cdecl h_867090(uint32_t a1);
	uint32_t __cdecl h_867120(void);
	uint32_t __cdecl h_867180(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_867400(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_867610(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_867D70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_867FA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_868260(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_8684C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl h_8687A0(void);
	uint32_t __cdecl h_868860(uint32_t a1);
	uint32_t __cdecl h_8688E0(void);
	uint32_t __cdecl h_868920(uint32_t a1);
	uint32_t __cdecl h_8691C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_869960(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_869A70(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_869AF0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_869B20(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl h_869BC0(uint32_t a1);
	uint32_t __cdecl h_869C40(uint32_t a1);
	uint32_t __cdecl h_869CC0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl h_86A4D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_86A6F0(uint32_t a1);
	uint32_t __cdecl h_86ADE0(uint32_t a1);
	uint32_t __cdecl h_86AF90(uint32_t a1);
	uint32_t __cdecl h_86B1F0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl h_86B280(void);
	uint32_t __cdecl h_86B310(uint32_t a1);
	uint32_t __cdecl h_86B340(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl h_86B3F0(uint32_t a1);
	uint32_t __cdecl h_86B4B0(uint32_t a1);
	uint32_t __cdecl h_86B560(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl h_86B770(uint32_t a1);
	uint32_t __cdecl h_86B790(uint32_t a1);
	uint32_t __cdecl h_86B7E0(uint32_t a1);
	uint32_t __cdecl h_86B820(uint32_t a1);
	uint32_t __cdecl h_86B8B0(uint32_t a1);
	uint32_t __cdecl h_86B8D0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag033_shell_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace shell
{
	// ====================================================================================
	// the library functions of this module whose code matches Drain's / Confuse's / an engine port's
	// up to the module's addresses and callees (the port's text with the module's addresses)
	// ====================================================================================
	// 0x866150 (MAG_033_SHELL_Tick; drain copy r_851510): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the four queues (live task count -> node +0x5E; the actor counters
	// 0x26C30D0 / 0x26C2DE2 are cleared before the queues run)
	uint32_t __cdecl h_866150(uint32_t a1)
	{
		g_mod = &MOD_033;
		// 30 fps layer: see mag033_shell_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x26C03A4) = 0x2793E58;
		states[0] = 0x8662A0;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x8662B0;
		states[2] = 0x8662C0;
		states[3] = 0x866440;
		states[4] = 0x86B7E0;
		states[5] = 0x86B820;
		states[6] = 0x86B920;
		states[7] = 0x86B930;
		states[8] = 0x86B940;
		states[9] = 0x86B950;
		states[10] = 0x86B970; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26C2DDC);
			const uint32_t v2 = MEM<uint32_t>(0x26C21D4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26C03A0) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26C2DD8);
			const uint32_t v2 = MEM<uint32_t>(0x26C21D0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26C03A0) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x26C30D0) = 0;
		MEM<uint16_t>(0x26C2DE2) = 0;
		static const uint32_t queues[4] = { Q_EMITTER, Q_ACTORS, Q_SHIELD, Q_STAGE };
		for (int i = 0; i < 4; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8663F0 (MAG_033_sub_8663F0; drain copy r_8517B0): stage darkening in, state 1 - level +0x1C up by
	// 0x100 per tick to 0x400 (then finished, next state), written to the four stage group words
	// 0x1D98992 + k * 0x2C
	uint32_t __cdecl h_8663F0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x400)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x400;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x866600 (sub_866600, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl h_866600(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			h_866630(a1);
			h_867090(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8667F0 (sub_8667F0, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl h_8667F0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x26C30E8);
		const uint32_t v = arena + 0x1B4;
		U32(v, 0) = (uint32_t)shl32((int32_t)S16(a1, 0x290), 16);
		const int32_t y = S16(a1, 0x292);
		const int32_t z = S16(a1, 0x294);
		U32(arena, 0x1B8) = (uint32_t)shl32(y, 16);
		U32(arena, 0x1BC) = (uint32_t)shl32(z, 16);
		b1_copy16(arena + 0x204, v);
		b1_copy16(arena + 0x1F4, v);
		b1_copy16(arena + 0x1E4, v);
		b1_copy16(arena + 0x1D4, v);
		b1_copy16(arena + 0x1C4, v);
		return U32(v, 0xC);
	}

	// 0x8668D0 (sub_8668D0; drain copy r_851DA0): the actor system's reference points from the caster (slot state +0x1E):
	// +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same on the ground (y 0),
	// +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C (16.16 each);
	// mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl h_8668D0(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		const int32_t slot = S16(st, 0x1E);
		const uint32_t ent = 0x1D972C0 + (uint32_t)mul32(slot, 0x9C);
		// [esp+4] s16 x, y, z: never-written stack when mode 2 skips the bone reads (0 here)
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		const int32_t px = S16(pp, 0);
		U16(pp, 2) = U16(ent, 0x24);
		S32(st, 0x1C4) = shl32(px, 16);
		S32(st, 0x1C8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1CC) = shl32(S16(pp, 4), 16);
		U32(st, 0x1D4) = U32(st, 0x1C4);
		U32(st, 0x1D8) = 0;
		U32(st, 0x1DC) = U32(st, 0x1CC);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		S32(st, 0x1E4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1E8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1EC) = shl32(S16(pp, 4), 16);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		S32(st, 0x1F4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1F8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1FC) = shl32(S16(pp, 4), 16);
		const uint32_t sx = U32(st, 0x1C4);
		const int32_t h = shl32(S16(ent, 0x3C), 16);
		U32(st, 0x204) = sx;
		S32(st, 0x208) = h;
		U32(st, 0x20C) = U32(st, 0x1CC);
		return 0; // void
	}

	// 0x866A30 (sub_866A30, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl h_866A30(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x26C30E8);
		int32_t i = 0;
		if (S16(arena, 0x1C) <= 0)
			return arena; // void (eax = arena)
		const uint32_t src = arena + 0x194;
		uint32_t dst = arena + 0x94;
		do
		{
			i++;
			// quirk: for i >= 4 the destinations reach the source (+0x194): the copies are done
			// dword by dword in listing order, so the overlap behaves like the original
			b1_copy16(dst - 0x40, src);
			b1_copy16(dst, src);
			b1_copy16(dst + 0x40, src);
			b1_copy16(dst + 0x80, src);
			b1_copy16(dst + 0xC0, src);
			dst += 0x10;
		} while (i < (int32_t)S16(arena, 0x1C));
		return dst; // void (eax = last destination pointer)
	}

	// 0x866C20 (sub_866C20; drain copy r_8520F0): per target of the actor system (slots state +4.., count +0x1C) the
	// target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its ground point),
	// +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride 0x10;
	// then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl h_866C20(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		// [esp+0x18] s16 x, y, z: never-written stack when mode 2 skips the bone reads (0 here)
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (S16(st, 0x1C) > 0)
		{
			uint32_t o = 0;
			for (int32_t i = 0; ; )
			{
				const uint32_t slot = U32(st, 4 + 4 * i);
				const uint32_t ent = 0x1D972C0 + slot * 0x9C;
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				const int32_t px = S16(pp, 0);
				U16(pp, 2) = U16(ent, 0x24);
				S32(st, o + 0x54) = shl32(px, 16);
				S32(st, o + 0x58) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x5C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x94) = U32(st, o + 0x54);
				U32(st, o + 0x98) = 0;
				U32(st, o + 0x9C) = U32(st, o + 0x5C);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				S32(st, o + 0xD4) = shl32(S16(pp, 0), 16);
				S32(st, o + 0xD8) = shl32(S16(pp, 2), 16);
				S32(st, o + 0xDC) = shl32(S16(pp, 4), 16);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				S32(st, o + 0x114) = shl32(S16(pp, 0), 16);
				S32(st, o + 0x118) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x11C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x154) = U32(st, o + 0x54);
				S32(st, o + 0x158) = shl32(S16(ent, 0x3C), 16);
				U32(st, o + 0x15C) = U32(st, o + 0x5C);
				o += 0x10;
				i++;
				if (i >= S16(st, 0x1C)) break;
			}
		}
		// centre of the x / z range of the targets' +0x54 / +0x5C
		const int32_t n = S16(st, 0x1C);
		int32_t min_x = 0, max_x = 0, max_z = 0, min_z = 0;
		for (int32_t i = 0; i < n; i++)
		{
			const int32_t vx = S32(st, 0x54 + 0x10 * i);
			const int32_t vz = S32(st, 0x5C + 0x10 * i);
			if ((uint16_t)i == 0)
			{
				max_x = vx;
				max_z = vz;
				min_x = vx;
				min_z = vz;
				continue;
			}
			if (vx < min_x) min_x = vx;
			else if (vx > max_x) max_x = vx;
			if (vz < min_z) min_z = vz;
			else if (vz > max_z) max_z = vz;
		}
		S32(st, 0x194) = add32(min_x, max_x) / 2;
		U32(st, 0x198) = 0;
		S32(st, 0x19C) = add32(max_z, min_z) / 2;
		return 0; // void
	}

	// 0x867090 (sub_867090, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl h_867090(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x26C30E8, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			h_86B280();
			h_8688E0();
			// 30 fps layer: see mag033_shell_held.inc
			FX_HELD(held_note_system(sys);)
			h_8687A0();
			h_867120();
			sys = U32(0x26C30E8, 0);
			U16(sys, 0x18) = (uint16_t)(U16(sys, 0x18) + 1);
			if (S16(sys, 0x18) >= S16(sys, 0x1A) && U16(sys, 0x14) == 0 && U16(sys, 0x16) == 0)
				U16(sys, 0x20) = (uint16_t)(U16(sys, 0x20) + 1);
			break;
		case 2:
			done = 1;
			break;
		default:
			break;
		}
		uint16_t c14 = U16(sys, 0x14);
		uint16_t c16 = U16(sys, 0x16);
		U16(0x26C30D0, 0) = (uint16_t)(U16(0x26C30D0, 0) + c14);
		U16(0x26C2DE2, 0) = (uint16_t)(U16(0x26C2DE2, 0) + c16);
		return done;
	}

	// 0x867120 (sub_867120, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl h_867120(void)
	{
		uint32_t act = U32(U32(0x26C30E8, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x26C30E8, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					h_867180(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x867180 (sub_867180; drain copy r_852650): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl h_867180(uint32_t a1, uint32_t a2)
	{
		uint32_t ctx = MEM<uint32_t>(SCRATCH_SP) - 0x58;
		const bool def3a = U16(a2, 0x3A) != 0;
		const uint32_t model = U32(a1, 0x170);
		MEM<uint32_t>(SCRATCH_SP) = ctx;
		const uint32_t saved_ctx = ctx;
		U32(ctx, 0) = model;
		U32(ctx, 0x1C) = 0;
		if (!def3a)
			U32(ctx, 0x1C) = 0x30;
		const uint16_t col = U16(a1, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(a1, 0x16C);
			const uint32_t fl = U32(ctx, 0x1C);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x1C) = fl | 0xC0;
		}
		const int8_t copies = S8(a1, 0x1D8);
		if (copies == 1)
		{
			const uint32_t m = a1 + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
			const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			MEM<uint32_t>(PACKET_CURSOR) = a_7355E0(ctx, ot, 2, cursor);
		}
		else if (copies > 0)
		{
			const uint32_t m = a1 + 0xAC;
			uint32_t pos = a1 + 0x19E;
			int32_t i = 0;
			int32_t n;
			do
			{
				S32(a1, 0xC0) = S16(pos, -2);
				S32(a1, 0xC4) = S16(pos, 0);
				S32(a1, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
				const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
				const uint32_t r = a_7355E0(saved_ctx, ot, 2, cursor);
				n = S8(a1, 0x1D8);
				i++;
				pos += 8;
				MEM<uint32_t>(PACKET_CURSOR) = r;
			} while (i < n);
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x58;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x867400 (sub_867400, copy of b_72BA00 0x; 097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl h_867400(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // edi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x4000000;          // tag: 4 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					x::GTE_AVSZ3();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x14;
						pk += 0x14;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// 0x867610 (sub_867610, copy of b_72BC10 0x; 097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl h_867610(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // esi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x5000000;          // tag: 5 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					const uint32_t v3 = vbase + (uint32_t)U16(face, 0xA) * 4;
					x::GTE_LoadV0(v3);
					x::GTE_RTPS();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					x::GTE_ReadSXY2(pk + 8);
					x::GTE_AVSZ4();
					if (b1_out(S16(pk, 8), 0xA00)) clip |= 8;
					if (b1_out(S16(pk, 0xA), 0x6C0)) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x18;
						pk += 0x18;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// 0x867D70 (sub_867D70, copy of b_72C370 0x72C370; Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl h_867D70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x10]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a2 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;                // esi = pkt + 0x10 (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x6000000;       // tag: 6 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x1C;
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x867FA0 (sub_867FA0, copy of b_72C5A0 0x72C5A0; Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl h_867FA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x14]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x8000000;       // tag: 8 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x20))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, rec + 0x14);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 0x1C);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10), c3 = U32(rec, 0x14);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
							U32(pkt, 0x1C) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x868260 (sub_868260, copy of b_72C860 0x; Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl h_868260(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebp
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;          // tag: 9 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t uv2 = U32(rec, 8) >> 16;
			U32(pkt, 0x18) = uv1 + toff;
			U32(pkt, 0x24) = uv2 + toff;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x8684C0 (sub_8684C0, copy of b_72CAC0 0x72CAC0; Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl h_8684C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebx
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;          // tag: 12 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t w23 = (toff << 16) + toff;
			uv1 += toff;
			w23 += U32(rec, 0x14);
			U32(pkt, 0x24) = w23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = w23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x14]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x18, rec + 0x1C, rec + 0x20);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x8687A0 (sub_8687A0, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl h_8687A0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x26C30E8), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x26C30E8);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						h_868860(act);   // (the original also pushes the bone entry, unused)
					}
					else if (n > 0)
					{
						int32_t i = 0;
						uint32_t sub = act + 0x19E;   // edi
						do
						{
							S32(act, 0xC0) = S16(sub, -2);
							S32(act, 0xC4) = S16(sub, 0);
							S32(act, 0xC8) = S16(sub, 2);
							h_868860(act);
							i++;
							sub += 8;
						} while (i < S8(act, 0x1D8));
					}
				}
			}
			act = U32(act, 4);
		} while (act != 0);
		return 0; // void
	}

	// 0x868860 (sub_868860; drain copy r_853D30): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl h_868860(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t hdr = MEM<uint32_t>(SCRATCH_SP) - 0xB4;
		MEM<uint32_t>(SCRATCH_SP) = hdr;
		const uint32_t mtx = act + 0xAC;
		x::GTE_SetRotMatrix(mtx);
		x::GTE_SetTransVector(mtx);
		const uint32_t seq = U32(act, 0x170);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const int16_t variant = (int16_t)S8(act, 0x1D0);
		U32(hdr, 0) = seq;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		S16(hdr, 4) = variant;
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = x::InitEffectSequenceFromData_c19(hdr, ot, 2, cursor);
		MEM<uint32_t>(SCRATCH_SP) = MEM<uint32_t>(SCRATCH_SP) + 0xB4;
		return 0; // void
	}

	// 0x8688E0 (sub_8688E0, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl h_8688E0(void)
	{
		uint32_t act = U32(U32(0x26C30E8, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				h_869BC0(act);
			else if (type == 1)
				h_868920(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x868920 (sub_868920, copy of a_740880 0x711EF0; engine; Siren sub_740880): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl h_868920(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x26C30E8);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (h_869A70(act, bone) == 0)
			{
				a_740910(act, bone);
				h_8691C0(act, bone);
				U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
				return 0; // void
			}
			U16(act, 0x1CC) = (uint16_t)(U16(act, 0x1CC) + 1);
			U32(act, 0x170) = 0;
			return 0; // void
		}
		if (phase == 1)
		{
			a_742CD0(act);
			st = MEM<uint32_t>(0x26C30E8);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x869960 (sub_869960, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl h_869960(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x26C30E8, 0);                  // actor state block
		uint32_t dst_tab = U32(st, 0x22C);
		uint32_t src_tab = U32(st, 0x230);
		uint32_t dst = U32(dst_tab, i_dst * 4);                // ebx
		uint32_t i_src1 = U8(U32(data, 0x130), frame);
		uint32_t src1 = U32(src_tab, i_src1 * 4);              // ebp
		uint32_t i_src2 = U8(U32(data, 0x134), frame);
		uint32_t src2 = U32(src_tab, i_src2 * 4);              // [esp+0x18] (a2 slot)
		if (src1 == 0 || src2 == 0)
			return 0; // void
		uint32_t w_blk = x::Field_Alloc(8);                    // esi: {+0 4096 - w, +4 w}
		int32_t frame2 = S16(a1, 0x1CA);                       // (re-read through the a1 slot)
		uint32_t n = U32(dst, 4);
		int32_t w = S16(U32(data, 0x138), frame2 * 2);
		U32(w_blk, 4) = (uint32_t)w;
		U32(w_blk, 0) = (uint32_t)sub32(0x1000, w);
		if ((int32_t)n > 0)
		{
			uint32_t out = dst + 8;                            // ebx
			uint32_t in2 = src2 + 8;                           // edi
			uint32_t delta = src1 - src2;                      // ebp
			do
			{
				x::set_dword_1CA8A30(U32(w_blk, 0));
				cx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				cx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				cx::sub_45E0B0(in2);
				cx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x869A70 (sub_869A70, copy of a_7419C0 0x713030; engine; Siren sub_7419C0, in 095-100): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl h_869A70(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				h_869AF0(a1, a2);
				return 1;
			}
			// -> 0x741A21
		}
		else if (kind == 1)
		{
			// 0x7419F8
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) >= 0)
				return 0;
			h_869AF0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			h_869AF0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x869AF0 (sub_869AF0, copy of a_741A40 0x7130B0; engine; Siren sub_741A40, in 095-100): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl h_869AF0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return h_869B20(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x869BC0 (sub_869BC0, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl h_869BC0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			h_86ADE0(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x86A5A0, a1);
		else if (mode == 4)
			callp(0x86A6F0, a1);
		h_869C40(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x869C40 (sub_869C40, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl h_869C40(uint32_t a1)
	{
		uint32_t sys = U32(0x26C30E8, 0);
		int32_t k = S8(a1, 0x6A);
		uint32_t def = U32(U32(sys, 0x224), k * 4);
		int16_t frame = S16(a1, 0x60);
		if (frame > 0x27)
			return 0; // void
		uint32_t total = U8(U32(def, 0xC8), (int32_t)frame);
		if (total == 0)
			return 0; // void
		int32_t batch = S8(def, 0x36);
		int32_t full = (int32_t)total / batch;
		int32_t rest = (int32_t)total % batch;
		if (full > 0)
		{
			do
			{
				int32_t b = S8(def, 0x36);
				h_869CC0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			h_869CC0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x86A4D0 (sub_86A4D0): the engine's 0x7387B0 with the module's actor pool: allocates an actor
	// record of 0x2A0 bytes from the pool [0x26C30DC] starting at the cursor 0x26C2DF0 (wraps at 49:
	// record 49 of the 50 is never used), cleared, owner a1, definition index a2
	uint32_t __cdecl h_86A4D0(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x26C30DC);
		int32_t i = MEM<int16_t>(0x26C2DF0);
		uint32_t slot = 0;
		int32_t tries = 0;
		for (;;)
		{
			const uint32_t off = (uint32_t)mul32(i, 0x2A0);
			if (U8(off + pool, 0x1D7) == 0)
			{
				slot = (uint32_t)mul32(i, 0x2A0) + pool;
				x::MAG_007_sub_8DCC00(slot, 0x2A0);
				const uint8_t def_idx = (uint8_t)a2;
				U32(slot, 0x1BC) = a1;
				const uint8_t b6b = U8(a1, 0x6B);
				const uint32_t sys = MEM<uint32_t>(ACTOR_STATE);
				U8(slot, 0x1D7) = 1;
				U16(sys, 0x16) = (uint16_t)(U16(sys, 0x16) + 1);
				U8(slot, 0x1D6) = def_idx;
				U8(slot, 0x1D9) = b6b;
				a_742CA0(slot, 1);
				break;
			}
			i++;
			if (i >= 0x31)
				i = 0;
			tries++;
			if (tries >= 0x32)
				break;
		}
		i++;
		if (i >= 0x31)
			MEM<uint16_t>(0x26C2DF0) = 0;
		else
			MEM<uint16_t>(0x26C2DF0) = (uint16_t)i;
		return slot;
	}

	// 0x86A6F0 (sub_86A6F0, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl h_86A6F0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x26C30E8, 0);            // ecx
		int32_t aidx = S8(act, 0x6A);
		int32_t key = S16(act, 0x60);                          // ebx
		const uint32_t data = U32(U32(st, 0x224), aidx * 4);   // edi / [esp+0x10]
		uint8_t sel = U8(U32(data, 0x108), key);               // bp (movzx)
		uint8_t kind = U8(U32(data, 0x100), key);
		// UNINIT 0x72F1CE: the start point is a stack SVECTOR ([esp+0x14] x, [esp+0x16] y,
		// [esp+0x18] z) that only the switch cases fill; an unknown kind / index > 3 leaves it
		// uninitialised (0 here)
		int16_t ax = 0, ay = 0, az = 0;
		uint32_t base = 0;
		bool have = false;
		if (kind == 0)
		{
			if (sel <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5DC
				base = st + (uint32_t)((int32_t)S8(act, 0x6B) << 4) + offs0[sel];
				have = true;
			}
		}
		else if (kind == 1)
		{
			if (sel <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5CC
				base = st + offs1[sel];
				have = true;
			}
		}
		if (have)
		{
			int32_t vx = b2_fx(U32(base, 0));
			int32_t vy = b2_fx(U32(base, 4));
			ay = (int16_t)vy;
			int32_t vz = b2_fx(U32(base, 8));
			az = (int16_t)vz;
			ax = (int16_t)vx;
		}
		// first offset (SVECTOR [esp+0x1C]) rotated by the actor matrix -> IR1..3 ([esp+0x24], shorts)
		int16_t v[4];
		int16_t ir[8] = {};
		v[0] = (int16_t)U16(U32(data, 0x110), key * 2);
		v[1] = (int16_t)U16(U32(data, 0x114), key * 2);
		v[2] = (int16_t)U16(U32(data, 0x118), key * 2);
		v[3] = 0;                                              // (pad, not read by LoadV0)
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		az = (int16_t)(az + ir[2]);
		ay = (int16_t)(ay + ir[1]);
		const int32_t key2 = S16(act, 0x60);                   // ecx
		uint8_t kind2 = U8(U32(data, 0x104), key2);
		uint8_t sel2 = U8(U32(data, 0x10C), key2);
		ax = (int16_t)(ax + ir[0]);
		int16_t bx, by, bz;                                    // si, di, bx
		uint32_t base2 = 0;
		bool have2 = false;
		if (kind2 == 0)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5FC
				uint32_t g = U32(0x26C30E8, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x26C30E8, 0);
				base2 = g + offs1[sel2];
				have2 = true;
			}
		}
		if (have2)
		{
			int32_t vx = b2_fx(U32(base2, 0));
			int32_t vy = b2_fx(U32(base2, 4));
			int32_t vz = b2_fx(U32(base2, 8));
			bx = (int16_t)vx;
			by = (int16_t)vy;
			bz = (int16_t)vz;
		}
		else
		{
			// quirk 0x72F4EA: an unknown end kind / index > 3 reuses the rotated first offset (the IR
			// of the first MVMVA, still in [esp+0x24..0x29])
			bz = ir[2];
			by = ir[1];
			bx = ir[0];
		}
		// second offset rotated by the actor matrix
		v[0] = (int16_t)U16(U32(data, 0x11C), key2 * 2);
		v[1] = (int16_t)U16(U32(data, 0x120), key2 * 2);
		v[2] = (int16_t)U16(U32(data, 0x124), key2 * 2);
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		bx = (int16_t)(bx + ir[0]);
		by = (int16_t)(by + ir[1]);
		const int32_t key3 = S16(act, 0x60);
		const uint32_t ttab = U32(data, 0x128);
		bz = (int16_t)(bz + ir[2]);
		const int32_t t = S16(ttab, key3 * 2);
		int32_t a = ax;
		S32(act, 0x4C) = shl32(add32(mul32(sub32(bx, a), t), shl32(a, 12)), 4);
		a = ay;
		S32(act, 0x50) = shl32(add32(mul32(sub32(by, a), t), shl32(a, 12)), 4);
		a = az;
		S32(act, 0x54) = shl32(add32(mul32(sub32(bz, a), t), shl32(a, 12)), 4);
		return 0; // void
	}

	// 0x86ADE0 (sub_86ADE0, copy of a_742D00 0x714370; engine; Siren sub_742D00, in 095-100): particle init: copies descriptor placement
	// kind +0x11 to +0x68, builds the particle matrix +0x2C by descriptor +0x1C (identity / yaw
	// focus->anchor / yaw anchor->focus / owner matrix / yaw of battle entity dir+0x1E), places it
	// by kind (0x742EB0 / 0x742FF0 / 0x7424C0 / 0x742610) and sets its life +0x64 = desc +0x12
	uint32_t __cdecl h_86ADE0(uint32_t a1)
	{
		uint32_t dir = U32(0x26C30E8, 0);
		uint32_t table = U32(dir, 0x224);
		int32_t di = (int32_t)S8(a1, 0x6A);
		uint32_t owner = U32(a1, 0x5C);
		uint32_t desc = U32(table + di * 4, 0); // ebp, kept across the calls
		U8(a1, 0x68) = U8(desc, 0x11);
		uint32_t m = a1 + 0x2C;
		switch ((uint32_t)(int32_t)S8(desc, 0x1C))
		{
			case 0: // 0x742D38
				x::MAG_022_sub_8DD770(m);
				break;
			case 1: // 0x742D49: yaw of (focus dir+0x1C4/0x1CC - anchor point dir+0x54/0x5C + 16*anchor)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(0x26C30E8, 0);
				uint32_t e = (uint32_t)k + d;
				int32_t dx = sub32(S32(d, 0x1C4), S32(e, 0x54));
				int32_t dz = sub32(S32(d, 0x1CC), S32(e, 0x5C));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 2: // 0x742D8B: yaw of (anchor point - focus)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(0x26C30E8, 0);
				int32_t dx = sub32(S32((uint32_t)k + d, 0x54), S32(d, 0x1C4));
				int32_t dz = sub32(S32((uint32_t)k + d, 0x5C), S32(d, 0x1CC));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 3: // 0x742DCB: owner instance matrix
				memcpy((void *)m, (void *)(owner + 0x8C), 32);
				break;
			case 4: // 0x742DDD: yaw of battle entity dir+0x1E (entity +0xE)
			{
				x::MAG_022_sub_8DD770(m);
				uint32_t d = U32(0x26C30E8, 0);
				int32_t ent = (int32_t)S16(d, 0x1E);
				int32_t yaw = (int32_t)S16(0x1D972CE + (uint32_t)(ent * 39) * 4, 0);
				if (yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)yaw);
				break;
			}
			default:
				break;
		}
		// 0x742E11
		switch ((uint32_t)(int32_t)S8(a1, 0x68))
		{
			case 0: h_86AF90(a1); break;             // 0x742E21
			case 1: a_742FF0(a1); break;             // 0x742E38 (listing: not ported yet; part e4 ports it)
			case 2:
			case 3: a_7424C0(a1); break;             // 0x742E4F
			case 4: callp(0x86A6F0, a1); break;   // 0x742E66
			default: break;
		}
		U16(a1, 0x64) = (uint16_t)(int16_t)S8(desc, 0x12);
		return 0; // void
	}

	// 0x86B1F0 (sub_86B1F0, copy of a_743100 0x714770; engine; Siren sub_743100): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl h_86B1F0(uint32_t a1, uint32_t a2)
	{
		const uint32_t obj = a1;   // edi
		const uint32_t desc = a2;  // esi
		// quirk 0x743112..0x743179: args 2/3 are pushed as 32-bit registers of which only the low
		// 16 bits are set (movsx r16); the high halves are leftover register bits. a_741A70 only
		// reads their low byte, so the zero-extended 16-bit value is passed here.
		if (U8(desc, 0x28) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2C);
			h_869B20(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			h_869B20(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			h_869B20(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			h_869B20(obj, id, variant);
		}
		return 0; // void
	}

	// 0x86B280 (sub_86B280, copy of a_743190 0x714800; engine; Siren sub_743190): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via h_869B20(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl h_86B280(void)
	{
		uint32_t dir = MEM<uint32_t>(0x26C30E8);  // eax (re-read only after the calls)
		for (int32_t slot = 0; slot < 0x10; slot++)
		{
			uint32_t table = U32(dir, 0x224);
			uint32_t entry = U32(table, slot * 4);
			if (entry == 0)
				continue;
			uint32_t bit = (uint32_t)shl32(1, slot);
			uint32_t mask = U16(dir, 0x22);
			if (bit & mask)
				continue;
			uint8_t kind = U8(entry, 0x11);
			if (kind != 0 && kind != 1 && kind != 4)
				continue;
			int16_t group = (int16_t)S8(entry, 0x13);
			if (group != S16(dir, 0x18))
				continue;
			if (U8(entry, 0x37) == 1)
			{
				int32_t n = 0;
				if (S16(dir, 0x1C) <= 0)
					continue;
				do
				{
					h_869B20(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x26C30E8);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				h_869B20(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x26C30E8);
			}
		}
		return 0; // void
	}

	// 0x86B310 (sub_86B310, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl h_86B310(uint32_t a1)
	{
		if (h_867090(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x86B770 (MAG_033_sub_86B770; drain copy r_8567C0): emitter state 1 - at tick 30 releases the master (root +0x63 =
	// 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl h_86B770(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x1E)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x86B790 (MAG_033_sub_86B790; drain copy r_8567E0): emitter state 2 - from tick 30: damage of its target,
	// finished, next state
	uint32_t __cdecl h_86B790(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x1E)
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

	// 0x86B7E0 (MAG_033_sub_86B7E0; drain copy r_856830): master state - action loop: while the master is not held
	// (+0x63) and actions remain (+0x2A < +0x58), the next action (+0x2A / +0x2E up, state back
	// to the emitter spawn); after the last one waits 10 ticks (+0x60) and goes on
	uint32_t __cdecl h_86B7E0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return 0; // void
		const uint8_t idx = U8(node, 0x2A);
		if ((int16_t)(int8_t)idx < S16(node, 0x58))
		{
			const uint8_t loops = U8(node, 0x2E);
			U8(node, 0x2A) = (uint8_t)(idx + 1);
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x2E) = (uint8_t)(loops + 1);
			U8(node, 0x29) = (uint8_t)(st - 1);
			return 0; // void
		}
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x60) = 0xA;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x86B820 (MAG_033_sub_86B820; drain copy r_856870): master state - counts +0x60 down; at 0 starts the stage darkening
	// out (0x86B850, 0x40 bytes, stage queue), next state
	uint32_t __cdecl h_86B820(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x86B850, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x86B8B0 (MAG_033_sub_86B8B0; drain copy r_856900): stage darkening out state 0 - level +0x1C = 0x400, next state
	uint32_t __cdecl h_86B8B0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x86B8D0 (MAG_033_sub_86B8D0, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl h_86B8D0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// ====================================================================================
	// module code: master states, emitter, pools (functions unique to this copy)
	// ====================================================================================

	// 0x866310 (MAG_033_sub_866310): clears the two actor pools (0x654 bytes at [0x26C2DE4] = 15
	// particles of 0x6C, 0x8340 bytes at [0x26C30DC] = 50 actors of 0x2A0) and the pool cursors /
	// counters
	uint32_t __cdecl h_866310(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26C2DE4), 0x654);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26C30DC), 0x8340);
		MEM<uint16_t>(0x26C0480) = 0;
		MEM<uint16_t>(0x26C2DF0) = 0;
		MEM<uint16_t>(0x26C2DE0) = 0;
		MEM<uint16_t>(0x26C0388) = 0;
		return 0; // void
	}

	// 0x8662C0 (MAG_033_sub_8662C0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x26C30E4] (0x654 + 0x8340 bytes) and cleared, the stage
	// darkening task (0x866350, stage queue) starts, next state
	uint32_t __cdecl h_8662C0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x26C30E4);
		MEM<uint32_t>(0x26C2DE4) = p;
		p += 0x654;
		MEM<uint32_t>(0x26C30DC) = p;
		p += 0x8340;
		MEM<uint32_t>(0x26C30E4) = p;
		h_866310();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, ORIG_StageDim, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x866470 (MAG_033_sub_866470): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x866500 shield + actor system, 0x86B770 release the master, 0x86B790 damage,
	// ret}, sound at its first tick
	uint32_t __cdecl h_866470(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x866500;
		states[1] = 0x86B770;
		states[2] = 0x86B790;
		states[3] = 0x86B7D0; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Shell), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86B340 (MAG_033_sub_86B340): spawns a shield model task `fn` (0x2EC bytes, shield queue)
	// under `parent`: +0x74 model data, +0x78 animation word (sign-extended), +0x80 / +0x82 modes;
	// returns the node
	uint32_t __cdecl h_86B340(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SHIELD, a2, 0x2EC, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x866500 (MAG_033_sub_866500): emitter state 0 - the shield model (0x86B390, model 0x15EC42C,
	// animation 0x258) and the actor system (0x8665A0, actor data 0x15F2D18, start tick 0, mode
	// 0x2D) through the engine spawner 0x866550, next state
	uint32_t __cdecl h_866500(uint32_t a1)
	{
		h_86B340(a1, ORIG_Shield, 0x15EC42C, 0x258, 0, 0);
		a_73C100(a1, ORIG_ActorSystem, 0x15F2D18, 0, 0x2D, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x866630 (sub_866630, the engine's 0x73F990 plus the scale vector): actor director set-up for
	// node a1: *0x26C30E8 = state block node+0x34; copies node+0x29A/0x29C(mode)/0x29E into it,
	// points its 4 tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once by
	// 0x866E30, flag data+0), fills the target slot list (+4.., count = root +0x5A) from the cast
	// context's action record, sets the particle scale vector +0x214/+0x218/+0x21C to 0x1000 (the
	// shield model's state 0 overwrites it with its own scale), start / per-target / bounds positions
	// (0x8668D0, 0x866C20; modes 1/3/4 extra set-ups) and stores the caster-to-first-target distance
	// in state+0.
	uint32_t __cdecl h_866630(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(ACTOR_STATE) = st;
		U16(st, 0x22) = w29a;
		uint16_t caster = (uint16_t)U8(node, 0x2C);   // bp (movzx)
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		uint32_t relocated = U32(data, 0);
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(ACTOR_STATE);
			U32(data, 0) = 1;
		}
		uint16_t ntargets = U16(root, 0x5A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = ntargets;
		if ((int16_t)ntargets > 0)
		{
			int32_t i = 0;
			do
			{
				int32_t action = S8(node, 0x2A);
				uint32_t tab = U32(U32(node, 0x0C), 4);
				uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
				U32(st, 4 + 4 * i) = U8(rec, 0x18 * i);   // target slot
				i++;
			} while (i < S16(st, 0x1C));
		}
		U16(st, 0x1A) = U16(node, 0x29E);
		U32(st, 0x21C) = 0x1000;
		U32(st, 0x218) = 0x1000;
		U32(st, 0x214) = 0x1000;
		h_8668D0();
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 4)
			callp(0x8667F0, node);
		h_866C20();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 1)
		{
			callp(0x866A30, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		// distance caster entity -> first target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		int32_t s1 = S16(st, 0x1E);
		uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		uint32_t d10 = U32(e1, 0x1C);
		uint32_t d14 = U32(e1, 0x20);
		uint32_t s2 = U32(st, 4);
		uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		uint32_t d18 = U32(e2, 0x1C);
		int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(ACTOR_STATE);
		U32(st, 0) = dist;
		return 0; // void
	}

	// the actor instance transform helpers of the engine's 0x741120 (act_engine.cpp part e4) with
	// this module's camera copy pointer 0x26C03A4
	static inline int32_t h4_div65536(int32_t v) { return v / 65536; } // cdq / and edx,0xFFFF / add / sar 16
	// 16.16 position (3 dwords at src) -> integer dwords at +0xA0, rotate the instance matrix (+0x8C)
	// by the camera matrix column by column into +0xAC, then transform +0xA0 by the camera rotation
	// + translation into +0xC0 (MAC1..3)
	static void h4_world_to_view(uint32_t node, uint32_t src)
	{
		U32(node, 0xA0) = (uint32_t)h4_div65536(S32(src, 0));
		U32(node, 0xA4) = (uint32_t)h4_div65536(S32(src, 4));
		U32(node, 0xA8) = (uint32_t)h4_div65536(S32(src, 8));
		x::GTE_SetRotMatrix(MEM<uint32_t>(CAMERA_PTR));
		x::GTE_LoadIRFromMatrixColumn(node + 0x8C);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xAC);
		x::GTE_LoadIRFromMatrixColumn(node + 0x8E);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xAE);
		x::GTE_LoadIRFromMatrixColumn(node + 0x90);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xB0);
		x::GTE_SetTransVector(MEM<uint32_t>(CAMERA_PTR));
		x::GTE_LoadV0FromDwords(node + 0xA0);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(node + 0xC0);
	}
	// rotation build of cases 3 and 4 (identical code behind the two jump tables): base = parent
	// matrix copy (+0x1BC node +0x2C) or identity, or the camera matrix; then rotations +0xD0
	// (8DD960) / +0xCC (8DD7E0) / +0xCE (8DD8A0), skipped when 0
	static void h4_build_rot(uint32_t node, int32_t sub, uint32_t parent)
	{
		uint32_t m = node + 0x8C;
		switch ((uint32_t)sub)
		{
			case 0: // parent|identity, d0, cc, ce
				if (parent != 0)
					memcpy((void *)m, (void *)(parent + 0x2C), 32);
				else
					x::MAG_022_sub_8DD770(m);
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				break;
			case 1: // parent|identity, ce, cc, d0
				if (parent != 0)
					memcpy((void *)m, (void *)(parent + 0x2C), 32);
				else
					x::MAG_022_sub_8DD770(m);
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			case 2: // camera, d0, cc, ce
				x::UnpackRotationMatrix(MEM<uint32_t>(CAMERA_PTR), m);
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				break;
			case 3: // camera, ce, cc, d0
				x::UnpackRotationMatrix(MEM<uint32_t>(CAMERA_PTR), m);
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			default:
				break;
		}
	}

	// 0x8691C0 (sub_8691C0, the engine's 0x741120 + the actor scale): builds the transform of actor
	// a1 from its definition a2: keyframed modes 4/5 (+0x16) pick the frame's texture entry (+0x170,
	// 5 also morphs the vertices, 0x869960) and colour/scale (+0x16C..E, +0x1CE, +0xD4..D8, applied
	// by scale3DMatrix); rotation matrix +0x8C by +0x1B (camera+roll / fixed X 0x400 / camera yaw /
	// parent-or-identity or camera with ordered rotations); then projects the position(s) (+0xDC..
	// 16.16 stride 0x10, +0x1D8 = count) to view space: one point -> +0xC0..C8 (+0xC8 += depth bias
	// a2+0x38), several -> packed s16 x,y,z list at +0x19C (stride 8). Unlike the engine's copy both
	// exits then scale the view matrix +0xAC by the actor system's scale vector (state +0x214, the
	// shield size: 0x869905)
	uint32_t __cdecl h_8691C0(uint32_t a1, uint32_t a2)
	{
		uint32_t node = a1;
		uint32_t desc = a2;
		uint8_t mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			uint32_t tex = U8(U32(desc, 0x12C) + frame, 0);
			uint32_t dir = MEM<uint32_t>(ACTOR_STATE);
			U32(node, 0x170) = U32(U32(dir, 0x22C) + tex * 4, 0); // texture entry of this frame
			if (mode == 5)
				h_869960(node, desc);
		}

		// 0x86920F
		int32_t rot_mode = (int32_t)S8(desc, 0x1B);
		uint32_t m = node + 0x8C;
		switch ((uint32_t)rot_mode)
		{
			case 0: // camera rotation + roll (+0xD0)
				x::UnpackRotationMatrix(MEM<uint32_t>(CAMERA_PTR), m);
				if (U16(node, 0xD0) != 0)
					x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			case 1: // identity rotated by 0x400 (8DD7E0)
				x::MAG_022_sub_8DD770(m);
				x::sub_8DD7E0(m, 0x400);
				break;
			case 2: // identity + camera yaw
			{
				uint32_t yaw = x::sub_8DD7B0(MEM<uint32_t>(CAMERA_PTR));
				x::MAG_022_sub_8DD770(m);
				if ((uint16_t)yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)(int16_t)yaw);
				break;
			}
			case 3:
			case 4: // (same code, own jump table)
			{
				int32_t sub = (int32_t)S8(desc, 0x1D);
				uint32_t parent = U32(node, 0x1BC);
				h4_build_rot(node, sub, parent);
				break;
			}
			default:
				break;
		}

		// 0x86962B
		mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			U8(node, 0x16C) = U8(U32(desc, 0xF0) + frame, 0);
			U8(node, 0x16D) = U8(U32(desc, 0xF4) + frame, 0);
			U8(node, 0x16E) = U8(U32(desc, 0xF8) + frame, 0);
			U16(node, 0x1CE) = U16(U32(desc, 0xFC) + frame * 2, 0);
			U16(node, 0xD4) = U16(U32(desc, 0xE4) + frame * 2, 0); // scale x
			U16(node, 0xD6) = U16(U32(desc, 0xE8) + frame * 2, 0); // scale y
			uint16_t sz = U16(U32(desc, 0xEC) + frame * 2, 0);
			int32_t scale[4]; // [esp+0x10] (the 4th dword is never written by the original)
			scale[0] = (int32_t)S16(node, 0xD4);
			scale[1] = (int32_t)S16(node, 0xD6);
			U16(node, 0xD8) = sz;                                  // scale z
			scale[2] = (int32_t)(int16_t)sz;
			scale[3] = 0;
			x::scale3DMatrix(m, P(scale));
		}

		uint8_t count = U8(node, 0x1D8);
		if (count == 1)
		{
			h4_world_to_view(node, node + 0xDC);
			U32(node, 0xC8) = (uint32_t)add32(S32(node, 0xC8), (int32_t)S16(desc, 0x38));
		}
		else if ((int8_t)count > 0)
		{
			// point list
			int32_t i = 0;
			uint32_t src = node + 0xDC;
			uint32_t dst = node + 0x19C;
			do
			{
				h4_world_to_view(node, src);
				U16(dst, 0) = U16(node, 0xC0);
				U16(dst, 2) = U16(node, 0xC4);
				uint16_t z = (uint16_t)(U16(node, 0xC8) + U16(desc, 0x38));
				i++;
				src += 0x10;
				U16(dst, 4) = z;
				dst += 8;
			} while (i < (int32_t)S8(node, 0x1D8));
		}
		// 0x869905 (both exits)
		x::scale3DMatrix(node + 0xAC, MEM<uint32_t>(ACTOR_STATE) + 0x214);
		return 0; // void
	}

	// 0x869B20 (sub_869B20): the engine's 0x741A70 with the module's particle pool: allocates a free
	// 0x6C particle slot from the pool [0x26C2DE4] (round robin from the cursor 0x26C0480, which
	// wraps at 14: slot 14 of the 15 is never used; +0x69 in use), zeroes it, sets owner +0x5C = a1,
	// descriptor index +0x6A = a2, anchor +0x6B = a3, counts it in the director (+0x14) and appends
	// it to the particle list with state 0 (0x86AD80); returns the slot or 0
	uint32_t __cdecl h_869B20(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x26C2DE4);
		int32_t idx = (int32_t)MEM<int16_t>(0x26C0480);
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t dir = MEM<uint32_t>(ACTOR_STATE);
				U8(slot, 0x69) = 1;
				U16(dir, 0x14) = (uint16_t)(U16(dir, 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			idx++;
			if (idx >= 0xE)
				idx = 0;
			tries++;
		} while (tries < 0xF);
		idx++;
		if (idx < 0xE)
			MEM<uint16_t>(0x26C0480) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x26C0480) = 0;
		return slot;
	}

	// 0x869CC0 (sub_869CC0, the engine's 0x737FD0 + the actor scale): allocates a particle actor
	// record (0x86A4D0) for the emitter a1 and initialises its a3 particles from the descriptor a2:
	// start positions (+0xDC/+0x12C, spread along a randomly rotated down vector when desc+0x34 =
	// 1/2; unlike the engine's copy the random rotation is scaled by the actor system's scale vector
	// state +0x214 first: 0x869E75 / 0x86A016), per-particle rotation matrices (+0x0C+0x20i,
	// multiplied by the emitter matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..),
	// then 0x86B1F0; uses 0x50 bytes of the module scratch stack [0x26C30D8]
	uint32_t __cdecl h_869CC0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = MEM<uint32_t>(SCRATCH_SP) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		MEM<uint32_t>(SCRATCH_SP) = sp;
		// quirk (as the engine's 0x737FE1): `movsx ax` - the high half of the pushed dword is stale
		// eax; 0x86A4D0 only reads the low byte
		uint32_t node = h_86A4D0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = MEM<uint32_t>(ACTOR_STATE);
			int32_t k = S8(a1, 0x6A);
			uint32_t v = U32(U32(ctx, 0x228), shl32(k, 2));
			U32(node, 0x170) = v;
			U8(node, 0x1D1) = (uint8_t)a_7424B0(v);
			if (U8(desc, 0x16) == 2)
			{
				uint8_t b17 = U8(desc, 0x17);
				uint8_t b18 = U8(desc, 0x18);
				int32_t s1a = S8(desc, 0x1A);
				U8(node, 0x1D3) = b17;
				int32_t s19 = S8(desc, 0x19);
				U8(node, 0x1D4) = b18;
				U8(node, 0x1D5) = (uint8_t)a_742290((uint32_t)s19, (uint32_t)s1a);
			}
		}
		U16(node, 0x1DA) = U16(desc, 0x64);
		if ((int32_t)a3 > 0)
		{
			// (the original keeps these pointers in its own argument slots a1 / a2: compiler reuse)
			uint32_t src = a1 + 0x4C;          // emitter position (16 bytes)
			uint32_t emat = a1 + 0x2C;         // emitter matrix
			uint32_t out_speed = node + 0x184;
			uint32_t mat = node + 0x0C;
			uint32_t pos = node + 0x12C;
			for (int32_t n = (int32_t)a3; n != 0; n--)
			{
				e6_copy16(pos - 0x50, src);    // +0xDC+0x10i: origin
				if (U8(desc, 0x22) == 0)
					e6_copy16(pos, src);       // +0x12C+0x10i: current position
				uint8_t spread = U8(desc, 0x34);
				if (spread == 1)
				{
					// random direction over the whole sphere
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2C) = (uint16_t)(x::CrtRand() & 0xFFF);
					e6_rotate_zxy(sp, sp + 0x28);
					x::scale3DMatrix(sp, MEM<uint32_t>(ACTOR_STATE) + 0x214);   // 0x869E75
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (U8(desc, 0x33) != 0)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					e6_rotate_down_vector(sp);
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = mul32(S16(sp, 0x4A), S16(sp, 0x22));
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					int32_t old = S32(pos, 0);
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(old, ex);
					old = S32(pos, 4);
					ey = shl32(ey, 4);
					S32(sp, 0x3C) = ey;
					S32(pos, 4) = add32(old, ey);
					old = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(old, ez);
				}
				else if (spread == 2)
				{
					// random direction on a ring (angles rand, 0, 0x400)
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = 0;
					U16(sp, 0x2C) = 0x400;
					e6_rotate_zxy(sp, sp + 0x28);
					x::scale3DMatrix(sp, MEM<uint32_t>(ACTOR_STATE) + 0x214);   // 0x86A016
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (S8(desc, 0x33) != 1)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						e6_rand_mod(sp, 0x4A);
						int32_t e = S16(sp, 0x4E);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), shl32(e, 1));
					}
					if ((x::CrtRand() & 1) != 0)
						U16(sp, 0x4A) = (uint16_t)(0 - U16(sp, 0x4A));
					e6_rotate_down_vector(sp);
					// the y offset is the raw key << 16 (the rotated vector's y is not used)
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = shl32(S16(sp, 0x4A), 16);
					int32_t oldx = S32(pos, 0);
					S32(sp, 0x3C) = ey;
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(oldx, ex);
					int32_t ey2 = S32(sp, 0x3C);
					S32(pos, 4) = add32(S32(pos, 4), ey2);
					int32_t oldz = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(oldz, ez);
				}
				// particle rotation angles sp+0x30 (random range, or a copy of the spread angles; any
				// other mode keeps the stale scratch words)
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					// quirk (as the engine's 0x73846D): copies the dwords sp+0x28 / sp+0x2C, including
					// the word +0x2E this function never writes (stale module scratch, deterministic)
					uint32_t c0 = U32(sp, 0x28);
					uint32_t c1 = U32(sp, 0x2C);
					U32(sp, 0x30) = c0;
					U32(sp, 0x34) = c1;
				}
				e6_rotate_zxy(mat, sp + 0x30);
				// particle matrix = emitter matrix * particle matrix (column by column)
				x::GTE_SetRotMatrix(emat);
				x::GTE_LoadIRFromMatrixColumn(mat);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat);
				x::GTE_LoadIRFromMatrixColumn(mat + 2);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 2);
				x::GTE_LoadIRFromMatrixColumn(mat + 4);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 4);
				if (U8(desc, 0x27) == 0)
				{
					uint32_t r1 = a_7422C0(U32(desc, 0x4C), U32(desc, 0x50));
					U32(out_speed, -0x10) = r1;           // +0x174+4i
					uint32_t r2 = a_7422C0(U32(desc, 0x54), U32(desc, 0x58));
					U32(out_speed, 0) = r2;               // +0x184+4i
				}
				out_speed += 4;
				pos += 0x10;
				mat += 0x20;
			}
		}
		a_742350(node + 0x194, desc + 0x148, desc + 0x150);
		uint8_t m1e = U8(desc, 0x1E);
		if (m1e == 1 || m1e == 2)
		{
			int32_t s20 = S8(desc, 0x20);
			int32_t s1f = S8(desc, 0x1F);
			U16(node, 0x1C8) = (uint16_t)a_742290((uint32_t)s1f, (uint32_t)s20);
		}
		if (U8(desc, 0x27) == 0)
		{
			U32(node, 0x1DC) = a_7422C0(U32(desc, 0x5C), U32(desc, 0x60));
			if ((int32_t)a3 > 0)
			{
				// three key-colour sets per particle (+0x1E0 / +0x220 / +0x260, 16 bytes each)
				uint32_t q = node + 0x220;
				for (int32_t n = (int32_t)a3; n != 0; n--)
				{
					a_742300(q - 0x40, desc + 0x68, desc + 0x78);
					a_742300(q, desc + 0x88, desc + 0x98);
					a_742300(q + 0x40, desc + 0xA8, desc + 0xB8);
					q += 0x10;
				}
			}
		}
		h_86B1F0(node, desc);
		MEM<uint32_t>(SCRATCH_SP) = MEM<uint32_t>(SCRATCH_SP) + 0x50;
		return 0; // void
	}

	// 0x86AF90 (sub_86AF90, the engine's 0x742EB0 + the actor scale): places particle a1 at one of
	// the director's four per-anchor point sets (dir+0x94/0xD4/0x114/0x154 + 16*anchor, by
	// descriptor +0x25), then adds the descriptor offset rotated by a stack copy of the particle
	// matrix +0x2C scaled by the actor system's scale vector (state +0x214, 0x86B06B) (<<16)
	uint32_t __cdecl h_86AF90(uint32_t a1)
	{
		const uint32_t dir = MEM<uint32_t>(ACTOR_STATE);   // ebp
		uint32_t table = U32(dir, 0x224);
		uint32_t desc = U32(table + (int32_t)S8(a1, 0x6A) * 4, 0);
		uint32_t base = 0;
		bool has = true;
		switch ((uint32_t)(int32_t)S8(desc, 0x25))
		{
			case 0: base = 0x94; break;
			case 1: base = 0xD4; break;
			case 2: base = 0x114; break;
			case 3: base = 0x154; break;
			default: has = false; break;
		}
		if (has)
		{
			uint32_t src = (uint32_t)shl32((int32_t)S8(a1, 0x6B), 4) + dir + base;
			U32(a1, 0x4C) = U32(src, 0);
			U32(a1, 0x50) = U32(src, 4);
			uint32_t s8 = U32(src, 8);
			uint32_t sc = U32(src, 0xC);
			U32(a1, 0x54) = s8;
			U32(a1, 0x58) = sc;
		}
		// 0x86B016: [esp+0x10] SVECTOR +0 (pad +6 never written, not read), IR123 +8, matrix +0x10
		uint8_t local[0x30] = {};
		*(int16_t *)(local + 0) = (int16_t)h4_div65536(S32(desc, 0));
		*(int16_t *)(local + 2) = (int16_t)h4_div65536(S32(desc, 4));
		*(int16_t *)(local + 4) = (int16_t)h4_div65536(S32(desc, 8));
		memcpy(local + 0x10, (void *)(a1 + 0x2C), 32);
		x::scale3DMatrix(P(local + 0x10), dir + 0x214);
		x::GTE_SetRotMatrix(P(local + 0x10));
		x::GTE_LoadV0(P(local));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(local + 8));
		int32_t ir1 = *(int16_t *)(local + 8);
		int32_t ir2 = *(int16_t *)(local + 0xA);
		int32_t ir3 = *(int16_t *)(local + 0xC);
		U32(a1, 0x50) = (uint32_t)add32(S32(a1, 0x50), shl32(ir2, 16));
		U32(a1, 0x4C) = (uint32_t)add32(S32(a1, 0x4C), shl32(ir1, 16));
		U32(a1, 0x54) = (uint32_t)add32(S32(a1, 0x54), shl32(ir3, 16));
		return 0; // void
	}

	// ====================================================================================
	// shield model (task 0x86B390 = the engine's state-table task 0x73A380: states 0x86B3F0,
	// 0x86B4B0, ret)
	// ====================================================================================

	// 0x86B3F0 (sub_86B3F0): shield state 0 - decodes the model's prim layout (+0x74 data, +0x78
	// animation) into +0x94; unless +0x80, position = the target's effect bone 0xF1 with y = its
	// word +0x3C; scale = 3 * (entity +0x3C - +0x36) clamped to 0x400..0x4000 into +0x50/+0x54/+0x58
	// and into the actor system's scale vector (state +0x214..+0x21C; +0x220 = node +0x5C), next state
	uint32_t __cdecl h_86B3F0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		const uint32_t anim = U32(node, 0x78);
		const uint32_t ent = slot * 0x9C + 0x1D972C0;   // edi
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, anim);
		if (U16(node, 0x80) == 0)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
			U16(node, 0x1E) = U16(ent, 0x3C);
		}
		// movsx (u16 difference); *3 << 10; cdq; and edx, 0x3FF; add; sar 10; 16-bit clamp; movsx
		const int32_t h = (int16_t)(uint16_t)(U16(ent, 0x3C) - U16(ent, 0x36));
		const int32_t t = shl32(h * 3, 10);
		int16_t v = (int16_t)(add32(t, (t >> 31) & 0x3FF) >> 10);
		if (v > 0x4000)
			v = 0x4000;
		else if (v < 0x400)
			v = 0x400;
		const int32_t s = v;
		const uint32_t sc = MEM<uint32_t>(ACTOR_STATE) + 0x214;
		S32(node, 0x58) = s;
		S32(node, 0x54) = s;
		S32(sc, 0) = s;
		S32(node, 0x50) = s;
		S32(sc, 4) = s;
		const uint32_t w5c = U32(node, 0x5C);
		S32(sc, 8) = s;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		U32(sc, 0xC) = w5c;
		return 0; // void
	}

	// 0x86B4B0 (sub_86B4B0): shield state 1 - plays the model (shared player 0x701970, callback
	// 0x86B560) with a parameter block on the stack: +0 the model matrix = camera x (identity rotated
	// by the target's yaw entity +0xE, scaled by +0x50, translated to the position +0x1C..+0x20),
	// +0x48 the blend buffer 0x26C2DF8. When the model has ended: finished, next state.
	uint32_t __cdecl h_86B4B0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		const uint32_t ent = slot * 0x9C + 0x1D972C0;   // edi
		// stack frame 0x5C (the parameter block): the callback reads +0x00..+0x1F and +0x48; the
		// other words are never written by the original (0 here, never read)
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		S32(L, 0x14) = S16(node, 0x1C);
		S32(L, 0x18) = S16(node, 0x1E);
		S32(L, 0x1C) = S16(node, 0x20);
		x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)S16(ent, 0xE));
		x::scale3DMatrix(L, node + 0x50);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U32(L, 0x48) = BLEND_BUFFER;
		// 30 fps layer: see mag033_shell_held.inc
		FX_HELD(held_note_shield(node + 0x94, L);)
		if (prim_play(node + 0x94, ORIG_ShieldCallback, L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x86B560 (sub_86B560): prim-model player draw callback of the shield (layout a1, record a2,
	// block a3 of 0x86B4B0) - picks the object's vertex frame (blended by MAG_017_sub_701390 between
	// two frames into the block's buffer +0x48), builds its matrix (record rotation; record offset
	// through the block matrix; block matrix x object matrix; block translation added; record
	// scale), sets the fade and draws it into the module arena (Effect_RenderPrimModel)
	uint32_t __cdecl h_86B560(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x38 bytes, offsets from the frame base): +0x00 SVECTOR offset, +0x08 scale
		// (3 x int32), +0x18 Mat4x3 object matrix (+0x2C translation)
		alignas(4) uint8_t loc[0x38] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x18;
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
		U16(L, 0) = U16(rec, 8);
		U16(L, 2) = U16(rec, 0xA);
		U16(L, 4) = U16(rec, 0xC);
		// record offset through the block matrix, object rotation composed with it
		x::GTE_SetRotMatrix(blk);
		x::GTE_LoadV0(L);
		x::GTE_MVMVA_RotV0();
		x::GTE_ReadMAC123(M + 0x14);
		x::GTE_MatrixMultiply(blk, M);
		S32(M, 0x14) = add32(S32(M, 0x14), S32(blk, 0x14));
		S32(M, 0x18) = add32(S32(M, 0x18), S32(blk, 0x18));
		S32(M, 0x1C) = add32(S32(M, 0x1C), S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(L, 0x8) = S16(rec, 0x18);
			S32(L, 0xC) = S16(rec, 0x1A);
			S32(L, 0x10) = S16(rec, 0x1C);
			x::scale3DMatrix(M, L + 0x8);
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
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, cursor);
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8662A0, (void *)a_73A0D0, "033 MAG_033_sub_8662A0" },
		{ 0x8662B0, (void *)a_73A0D0, "033 MAG_033_sub_8662B0" },
		{ 0x866350, (void *)a_73A380, "033 MAG_033_sub_866350" },
		{ 0x8663B0, (void *)a_7335D0, "033 MAG_033_sub_8663B0" },
		{ 0x866430, (void *)a_73A6D0, "033 nullsub_1656" },
		{ 0x866440, (void *)a_73A170, "033 MAG_033_sub_866440" },
		{ 0x866550, (void *)a_73C100, "033 MAG_033_sub_866550" },
		{ 0x8665A0, (void *)a_73A380, "033 MAG_033_sub_8665A0" },
		{ 0x866B00, (void *)a_73FE90, "033 sub_866B00" },
		{ 0x866E30, (void *)a_740210, "033 sub_866E30" },
		{ 0x8672C0, (void *)a_7355E0, "033 sub_8672C0" },
		{ 0x867890, (void *)a_735BB0, "033 sub_867890" },
		{ 0x867AD0, (void *)a_735DF0, "033 sub_867AD0" },
		{ 0x8689B0, (void *)a_740910, "033 sub_8689B0" },
		{ 0x869010, (void *)a_740F70, "033 sub_869010" },
		{ 0x869060, (void *)a_740FC0, "033 sub_869060" },
		{ 0x8690F0, (void *)a_741050, "033 sub_8690F0" },
		{ 0x869C10, (void *)a_741B60, "033 sub_869C10" },
		{ 0x86A370, (void *)a_742290, "033 au_re__rand_53" },
		{ 0x86A3A0, (void *)a_7422C0, "033 sub_86A3A0" },
		{ 0x86A3E0, (void *)a_742300, "033 sub_86A3E0" },
		{ 0x86A430, (void *)a_742350, "033 sub_86A430" },
		{ 0x86A480, (void *)a_7423A0, "033 au_re__rand_53_0" },
		{ 0x86A590, (void *)a_7424B0, "033 sub_86A590" },
		{ 0x86A5A0, (void *)a_7424C0, "033 sub_86A5A0" },
		{ 0x86AD80, (void *)a_742CA0, "033 sub_86AD80" },
		{ 0x86ADB0, (void *)a_742CD0, "033 sub_86ADB0" },
		{ 0x86B0E0, (void *)a_742FF0, "033 sub_86B0E0" },
		{ 0x86B330, (void *)a_73A6D0, "033 nullsub_1657" },
		{ 0x86B390, (void *)a_73A380, "033 MAG_033_sub_86B390" },
		{ 0x86B760, (void *)a_73A6D0, "033 nullsub_1658" },
		{ 0x86B7D0, (void *)a_73A6D0, "033 nullsub_1659" },
		{ 0x86B850, (void *)a_73A380, "033 MAG_033_sub_86B850" },
		{ 0x86B910, (void *)a_73A6D0, "033 nullsub_1661" },
		{ 0x86B920, (void *)a_747550, "033 MAG_033_sub_86B920" },
		{ 0x86B930, (void *)a_73A0D0, "033 MAG_033_sub_86B930" },
		{ 0x86B940, (void *)a_73A0D0, "033 MAG_033_sub_86B940" },
		{ 0x86B950, (void *)a_7475A0, "033 MAG_033_sub_86B950" },
		{ 0x86B970, (void *)a_73A6D0, "033 nullsub_1660" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x866150, (void *)h_866150, "033 MAG_033_SHELL_Tick" },
		{ 0x8662C0, (void *)h_8662C0, "033 MAG_033_sub_8662C0" },
		{ 0x866310, (void *)h_866310, "033 MAG_033_sub_866310" },
		{ 0x8663F0, (void *)h_8663F0, "033 MAG_033_sub_8663F0" },
		{ 0x866470, (void *)h_866470, "033 MAG_033_sub_866470" },
		{ 0x866500, (void *)h_866500, "033 MAG_033_sub_866500" },
		{ 0x866600, (void *)h_866600, "033 sub_866600" },
		{ 0x866630, (void *)h_866630, "033 sub_866630" },
		{ 0x8667F0, (void *)h_8667F0, "033 sub_8667F0" },
		{ 0x8668D0, (void *)h_8668D0, "033 sub_8668D0" },
		{ 0x866A30, (void *)h_866A30, "033 sub_866A30" },
		{ 0x866C20, (void *)h_866C20, "033 sub_866C20" },
		{ 0x867090, (void *)h_867090, "033 sub_867090" },
		{ 0x867120, (void *)h_867120, "033 sub_867120" },
		{ 0x867180, (void *)h_867180, "033 sub_867180" },
		{ 0x867400, (void *)h_867400, "033 sub_867400" },
		{ 0x867610, (void *)h_867610, "033 sub_867610" },
		{ 0x867D70, (void *)h_867D70, "033 sub_867D70" },
		{ 0x867FA0, (void *)h_867FA0, "033 sub_867FA0" },
		{ 0x868260, (void *)h_868260, "033 sub_868260" },
		{ 0x8684C0, (void *)h_8684C0, "033 sub_8684C0" },
		{ 0x8687A0, (void *)h_8687A0, "033 sub_8687A0" },
		{ 0x868860, (void *)h_868860, "033 sub_868860" },
		{ 0x8688E0, (void *)h_8688E0, "033 sub_8688E0" },
		{ 0x868920, (void *)h_868920, "033 sub_868920" },
		{ 0x8691C0, (void *)h_8691C0, "033 sub_8691C0" },
		{ 0x869960, (void *)h_869960, "033 sub_869960" },
		{ 0x869A70, (void *)h_869A70, "033 sub_869A70" },
		{ 0x869AF0, (void *)h_869AF0, "033 sub_869AF0" },
		{ 0x869B20, (void *)h_869B20, "033 sub_869B20" },
		{ 0x869BC0, (void *)h_869BC0, "033 sub_869BC0" },
		{ 0x869C40, (void *)h_869C40, "033 sub_869C40" },
		{ 0x869CC0, (void *)h_869CC0, "033 sub_869CC0" },
		{ 0x86A4D0, (void *)h_86A4D0, "033 sub_86A4D0" },
		{ 0x86A6F0, (void *)h_86A6F0, "033 sub_86A6F0" },
		{ 0x86ADE0, (void *)h_86ADE0, "033 sub_86ADE0" },
		{ 0x86AF90, (void *)h_86AF90, "033 sub_86AF90" },
		{ 0x86B1F0, (void *)h_86B1F0, "033 sub_86B1F0" },
		{ 0x86B280, (void *)h_86B280, "033 sub_86B280" },
		{ 0x86B310, (void *)h_86B310, "033 sub_86B310" },
		{ 0x86B340, (void *)h_86B340, "033 MAG_033_sub_86B340" },
		{ 0x86B3F0, (void *)h_86B3F0, "033 sub_86B3F0" },
		{ 0x86B4B0, (void *)h_86B4B0, "033 sub_86B4B0" },
		{ 0x86B560, (void *)h_86B560, "033 sub_86B560" },
		{ 0x86B770, (void *)h_86B770, "033 MAG_033_sub_86B770" },
		{ 0x86B790, (void *)h_86B790, "033 MAG_033_sub_86B790" },
		{ 0x86B7E0, (void *)h_86B7E0, "033 MAG_033_sub_86B7E0" },
		{ 0x86B820, (void *)h_86B820, "033 MAG_033_sub_86B820" },
		{ 0x86B8B0, (void *)h_86B8B0, "033 MAG_033_sub_86B8B0" },
		{ 0x86B8D0, (void *)h_86B8D0, "033 MAG_033_sub_86B8D0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag033_shell()
	{
		act::register_module(33);
		for (const act::shell::ModPort *p = act::shell::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(33, p->addr, p->port, p->name);
		for (const act::shell::ModPort *p = act::shell::PORTS; p->addr; p++)
			act::register_module_port(33, p->addr, p->port, p->name);
		// 30 fps layer: see mag033_shell_held.inc
		FX_HELD(register_mag033_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag033_shell_held.inc"
#endif
