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

// Effect 59: Megalixir (item, MAG_059_*): Mega-Potion's module (mag053_megapotion.cpp) with other pool sizes and timings:
// a copy of the actor/heal effect library (see act_engine.h) built like Full-life (mag027_fulllife.cpp: same master,
// emitter, stage fade and actor-system code with the module's own globals) with ONE actor system per target (the
// director set-up 0x7F2DE0 takes the action's current target record instead of the whole target list) and the action loop
// of Mighty Guard (mag078). Its code differs from Mega-Potion's only in immediates: pool sizes, emitter timings, the
// fade-out wait, the emitter sound tick (and the setup's arena size).
//
// Setup MAG_059_MEGALIXIR_Init 0x7F2810 (runs once, not ported; file loader MAG_059_MEGALIXIR_FL 0x7F27F0 = the effect's
// data file mag057-058.tim (name 0x159ADEC) -> [0x2607060], TIM uploaded by the setup unless the cast context's flag +1 bit 0, its
// packet arenas at file + 0 / + 0x9000 (second arena word + 0x9000 / + 0x12000), file arena cursor 0x260803C = file +
// 0x12000) creates the root queue 0x2607150 with the master and three task pools: 0x2607EA0 (4 x 0x58: emitters), 0x2607C00
// (4 x 0x2A4: actor systems), 0x2607E90 (10 x 0x40: stage fade).
//   Master (0x7F2930 = Full-life's 0x883240) - camera copy 0x2793E58 (the module scratch stack 0x2608030 grows down from
//     it), packet arena by tick parity (cursor 0x2607064), bone follow, 11-state table: 0x7F2A90 reserves the actor pools
//     after the arena (0x12FC + 0xD200 bytes = 45 particles of 0x6C and 80 actors of 0x2A0, cleared by 0x7F2AE0) and starts
//     the stage fade in (0x7F2B20: level 0x1D98992 + k * 0x2C up to 0x400), 0x7F2C10 one emitter per target of the action
//     (action loop 0x7F7AE0: next target record +0x2B once the emitter releases the master's +0x63 hold), 0x7F7B30 after
//     12 more ticks the stage fade out (0x7F7B60), then waits for the queues to empty.
//   Emitter (0x7F2C40), one per target: bone follow + model bounds (CURE_Emitter helpers), sound 0x1597840 at its tick
//     8; state 0 spawns the actor system (0x7F2CD0 -> 0x7F2D00 with the actor data 0x159ACE8, mode 0x2D), state 1 releases
//     the master at tick 18, state 2 applies the action result to its target (the heal) from tick 18.
//   Actor system (0x7F2D50 -> 0x7F2DB0 / 0x7F7A40): the engine's particle actors (emitters and particles, sprite sequences
//     0x7F4FE0 / prim models 0x7F3900 drawn into the module arena) placed by the actor data's key positions (caster / its one
//     target), taken once at the director set-up 0x7F2DE0.
//   Stage fade (0x7F2B20 in: 0x7F2B80 clears the 4 colour/fade layers of 0x1D9898C, 0x7F2BC0 level +0x100 per tick to
//     0x400; 0x7F7B60 out: 0x7F7BC0 level 0x400, 0x7F7BE0 -0x100 per tick to 0): the level word +6 of each layer
//     (0x1D98992 + k * 0x2C).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2607060..0x2608044 (file pointer 0x2607060, packet cursor 0x2607064, pools, queues, arenas, actor state 0x2608040,
// scratch stack pointer 0x2608030), the module scratch stack below the camera copy 0x2793E58, the fade layers
// 0x1D98992 + k * 0x2C (words) / 0x1D989B8 + k * 0x2C (bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace megalixir
{
	// ---- generated (gen53.py): the module descriptor of this copy of the library ----
	static const Mod MOD_059 = { "megalixir", 59, 0x7F27F0, 0x7F7C90,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2607084, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2607C00, 0x0, 0x2607EA0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2608040 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F3B80, 0x7F3D90, 0x7F4010, 0x7F4250, 0x7F44F0, 0x7F4720, 0x7F49E0, 0x7F4C40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F2C40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F5790, 0x7F57E0, 0x0, 0x0, 0x7F60D0, 0x0, 0x0, 0x0, 0x7F6AB0, 0x7F6AE0, 0x7F6BC0, 0x7F6CE0, 0x7F6E30, 0x0, 0x7F74F0, 0x7F76D0, 0x7F7810, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2607064;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x2608040;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x2608030;      // module scratch stack pointer
	static const uint32_t Q_EMITTER = 0x2607EA0, Q_ACTORS = 0x2607C00, Q_STAGE = 0x2607E90;
	static const uint32_t ORIG_Master = 0x7F2930;
	static const uint32_t ORIG_Emitter = 0x7F2C40;
	static const uint32_t ORIG_StageIn = 0x7F2B20;
	static const uint32_t ORIG_StageOut = 0x7F7B60;
	static const uint32_t ORIG_ActorSystem = 0x7F2D50;
	static const void *const SOUND = (const void *)0x1597840; // the emitter's sound

	// engine functions not in act::x
	namespace mlx
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
	uint32_t __cdecl ml_7F2930(uint32_t a1);
	uint32_t __cdecl ml_7F2A90(uint32_t a1);
	uint32_t __cdecl ml_7F2AE0(void);
	uint32_t __cdecl ml_7F2BC0(uint32_t a1);
	uint32_t __cdecl ml_7F2C40(uint32_t a1);
	uint32_t __cdecl ml_7F2CD0(uint32_t a1);
	uint32_t __cdecl ml_7F2DB0(uint32_t a1);
	uint32_t __cdecl ml_7F2DE0(uint32_t a1);
	uint32_t __cdecl ml_7F2F70(uint32_t a1);
	uint32_t __cdecl ml_7F3050(void);
	uint32_t __cdecl ml_7F31B0(void);
	uint32_t __cdecl ml_7F33A0(void);
	uint32_t __cdecl ml_7F3810(uint32_t a1);
	uint32_t __cdecl ml_7F38A0(void);
	uint32_t __cdecl ml_7F3900(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F3B80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F3D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F44F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F4720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F49E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F4C40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ml_7F4F20(void);
	uint32_t __cdecl ml_7F4FE0(uint32_t a1);
	uint32_t __cdecl ml_7F5060(void);
	uint32_t __cdecl ml_7F50A0(uint32_t a1);
	uint32_t __cdecl ml_7F60D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F61E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F6260(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F6290(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ml_7F6330(uint32_t a1);
	uint32_t __cdecl ml_7F63B0(uint32_t a1);
	uint32_t __cdecl ml_7F6430(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ml_7F6C10(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F6E30(uint32_t a1);
	uint32_t __cdecl ml_7F7920(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ml_7F79B0(void);
	uint32_t __cdecl ml_7F7A40(uint32_t a1);
	uint32_t __cdecl ml_7F7A70(uint32_t a1);
	uint32_t __cdecl ml_7F7A90(uint32_t a1);
	uint32_t __cdecl ml_7F7AE0(uint32_t a1);
	uint32_t __cdecl ml_7F7B30(uint32_t a1);
	uint32_t __cdecl ml_7F7BC0(uint32_t a1);
	uint32_t __cdecl ml_7F7BE0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag059_megalixir_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace megalixir
{
	// ====================================================================================
	// the module's functions: the text of the matching Full-life port (Mighty Guard / Counter Laser-Eye for the
	// pool set-up / allocator / emitter states / action loop) with this module's addresses and callees (the
	// comments describe the source original); the director set-up is ported by hand
	// ====================================================================================


	// 0x7F2930 (MAG_059_sub_7F2930): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the three queues (live task count -> node +0x5E; the actor counters
	// 0x260802A / 0x260801A are cleared before the queues run)
	uint32_t __cdecl ml_7F2930(uint32_t a1)
	{
		g_mod = &MOD_059;
		// 30 fps layer: see mag059_megalixir_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x2607084) = 0x2793E58;
		states[0] = 0x7F2A70;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x7F2A80;
		states[2] = 0x7F2A90;
		states[3] = 0x7F2C10;
		states[4] = 0x7F7AE0;
		states[5] = 0x7F7B30;
		states[6] = 0x7F7C30;
		states[7] = 0x7F7C40;
		states[8] = 0x7F7C50;
		states[9] = 0x7F7C60;
		states[10] = 0x7F7C80; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2608014);
			const uint32_t v2 = MEM<uint32_t>(0x260716C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2607080) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2608010);
			const uint32_t v2 = MEM<uint32_t>(0x2607168);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2607080) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x260802A) = 0;
		MEM<uint16_t>(0x260801A) = 0;
		static const uint32_t queues[3] = { Q_EMITTER, Q_ACTORS, Q_STAGE };
		for (int i = 0; i < 3; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x7F2A90 (MAG_059_sub_7F2A90): master state 2 - once no child is alive: the two actor pools
	// are carved from the file arena cursor [0x260803C] (0x12FC bytes = 45 particles of 0x6C at
	// [0x260801C], 0xD200 bytes = 80 actors of 0x2A0 at [0x2608034]) and cleared (0x7F2AE0), the
	// stage fade task 0x7F2B20 (0x40 bytes, stage queue) starts, next state
	uint32_t __cdecl ml_7F2A90(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x260803C);
		MEM<uint32_t>(0x260801C) = p;
		p += 0x12FC;
		MEM<uint32_t>(0x2608034) = p;
		p += 0xD200;
		MEM<uint32_t>(0x260803C) = p;
		ml_7F2AE0();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, ORIG_StageIn, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x7F2AE0 (MAG_059_sub_7F2AE0): clears the two actor pools (0x12FC bytes at [0x260801C], 0xD200
	// at [0x2608034]) and the pool cursors / counters
	uint32_t __cdecl ml_7F2AE0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x260801C), 0x12FC);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2608034), 0xD200);
		MEM<uint16_t>(0x2607160) = 0;
		MEM<uint16_t>(0x2608028) = 0;
		MEM<uint16_t>(0x2608018) = 0;
		MEM<uint16_t>(0x2607068) = 0;
		return 0; // void
	}


	// ====================================================================================
	// the library functions of this module whose code matches Drain's / an engine port's up to
	// the module's addresses and callees (the port's text with the module's addresses)
	// ====================================================================================
	// 0x7F2BC0 (MAG_059_sub_7F2BC0), written to the four stage fade words 0x1D98992 + k * 0x2C
	uint32_t __cdecl ml_7F2BC0(uint32_t a1)
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


	// 0x7F2C40 (MAG_059_sub_7F2C40): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x7F2CD0 actor system, 0x7F7A70 release the master, 0x7F7A90 action result,
	// ret}, sound at its first tick
	uint32_t __cdecl ml_7F2C40(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7F2CD0;
		states[1] = 0x7F7A70;
		states[2] = 0x7F7A90;
		states[3] = 0x7F7AD0; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 8)
			x::BdPlaySE(P(SOUND), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x7F2CD0 (MAG_059_sub_7F2CD0): emitter state 0 - the actor system (0x7F2D50, actor data
	// 0x159ACE8, start tick 0, mode 0x2D) through the engine spawner 0x7F2D00, next state
	uint32_t __cdecl ml_7F2CD0(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, 0x159ACE8, 0, 0x2D, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x7F2DB0 (sub_7F2DB0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl ml_7F2DB0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			ml_7F2DE0(a1);
			ml_7F3810(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7F2DE0 (sub_7F2DE0): actor director set-up for node a1 (Full-life's 0x8836F0 with ONE target: the action's current
	// target record +0x2B instead of the action's whole target list): *ACTOR_STATE = state block node+0x34; copies
	// node+0x29A/0x29C(mode)/0x29E into it, points its 4 tables (+0x224..+0x230) into the loaded actor data
	// (node+0x30; relocated once by 0x7F35B0, flag data+0), target count 1 = the action's record +0x2B target slot,
	// sets start / per-target / bounds positions (0x7F3050, 0x7F33A0; modes 1/3/4 extra set-ups) and stores the
	// caster-to-target distance in state+0.
	uint32_t __cdecl ml_7F2DE0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		const uint16_t w29a = U16(node, 0x29A);
		const uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint16_t caster = (uint16_t)U8(node, 0x2C); // bx (movzx)
		U16(st, 0x22) = w29a;
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		const uint32_t relocated = U32(data, 0);
		MEM<uint32_t>(ACTOR_STATE) = st;
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(ACTOR_STATE);
			U32(data, 0) = 1;
		}
		const int32_t action = S8(node, 0x2A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = 1;
		const uint32_t tab = U32(U32(node, 0x0C), 4);
		const int32_t ti = S8(node, 0x2B);
		const uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
		U32(st, 4) = U8(rec, (uint32_t)mul32(ti, 3) * 8);   // target slot (byte, ebx cleared)
		U16(st, 0x1A) = U16(node, 0x29E);
		ml_7F3050();
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 4)
			callp(0x7F2F70, node);
		ml_7F33A0();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 1)
		{
			callp(0x7F31B0, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		// distance caster entity -> target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		const int32_t s1 = S16(st, 0x1E);
		const uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		const uint32_t d10 = U32(e1, 0x1C);
		const uint32_t d14 = U32(e1, 0x20);
		const uint32_t s2 = U32(st, 4);
		const uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		const uint32_t d18 = U32(e2, 0x1C);
		const int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		const int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		const int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		const int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		const uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(ACTOR_STATE);
		U32(st, 0) = dist;
		return 0; // void
	}


	// 0x7F2F70 (sub_7F2F70, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl ml_7F2F70(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x2608040);
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


	// 0x7F3050 (sub_7F3050): the actor system's reference points from the caster (slot state +0x1E):
	// +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same on the ground (y 0),
	// +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C (16.16 each);
	// mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl ml_7F3050(void)
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


	// 0x7F31B0 (sub_7F31B0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl ml_7F31B0(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x2608040);
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


	// 0x7F33A0 (sub_7F33A0): per target of the actor system (slots state +4.., count +0x1C) the
	// target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its ground point),
	// +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride 0x10;
	// then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl ml_7F33A0(void)
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


	// 0x7F3810 (sub_7F3810, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl ml_7F3810(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x2608040, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			ml_7F79B0();
			ml_7F5060();
			// 30 fps layer: see mag059_megalixir_held.inc
			FX_HELD(held_note_system(sys);)
			ml_7F4F20();
			ml_7F38A0();
			sys = U32(0x2608040, 0);
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
		U16(0x260802A, 0) = (uint16_t)(U16(0x260802A, 0) + c14);
		U16(0x260801A, 0) = (uint16_t)(U16(0x260801A, 0) + c16);
		return done;
	}


	// 0x7F38A0 (sub_7F38A0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl ml_7F38A0(void)
	{
		uint32_t act = U32(U32(0x2608040, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x2608040, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					ml_7F3900(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}


	// 0x7F3900 (sub_7F3900): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl ml_7F3900(uint32_t a1, uint32_t a2)
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


	// 0x7F3B80 (sub_7F3B80, copy of b_72BA00 0x; 097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl ml_7F3B80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F3D90 (sub_7F3D90, copy of b_72BC10 0x; 097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl ml_7F3D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F44F0 (sub_7F44F0, copy of b_72C370 0x72C370; Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl ml_7F44F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F4720 (sub_7F4720, copy of b_72C5A0 0x72C5A0; Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl ml_7F4720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F49E0 (sub_7F49E0, copy of b_72C860 0x; Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl ml_7F49E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F4C40 (sub_7F4C40, copy of b_72CAC0 0x72CAC0; Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl ml_7F4C40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x7F4F20 (sub_7F4F20, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl ml_7F4F20(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x2608040), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x2608040);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						ml_7F4FE0(act);   // (the original also pushes the bone entry, unused)
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
							ml_7F4FE0(act);
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


	// 0x7F4FE0 (sub_7F4FE0): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl ml_7F4FE0(uint32_t a1)
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


	// 0x7F5060 (sub_7F5060, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl ml_7F5060(void)
	{
		uint32_t act = U32(U32(0x2608040, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				ml_7F6330(act);
			else if (type == 1)
				ml_7F50A0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}


	// 0x7F50A0 (sub_7F50A0, copy of a_740880 0x740880, callee chain to the module's particle pool 0x7F6290): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl ml_7F50A0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x2608040);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (ml_7F61E0(act, bone) == 0)
			{
				a_740910(act, bone);
				a_741120(act, bone);
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
			st = MEM<uint32_t>(0x2608040);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}


	// 0x7F60D0 (sub_7F60D0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl ml_7F60D0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x2608040, 0);                  // actor state block
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
				mlx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				mlx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				mlx::sub_45E0B0(in2);
				mlx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}


	// 0x7F61E0 (sub_7F61E0, copy of a_7419C0 0x7419C0, callee chain to the module's particle pool 0x7F6290): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl ml_7F61E0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				ml_7F6260(a1, a2);
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
			ml_7F6260(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			ml_7F6260(a1, a2);
			return 1;
		}
		return 0;
	}


	// 0x7F6260 (sub_7F6260, copy of a_741A40 0x741A40, callee chain to the module's particle pool 0x7F6290): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl ml_7F6260(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return ml_7F6290(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}


	// 0x7F6290 (sub_7F6290): the engine's 0x741A70 with the module's particle pool: allocates a free
	// 0x6C particle slot from the pool [0x260801C] (round robin from the cursor 0x2607160, which
	// wraps at 44: slot 44 of the 45 is never used; +0x69 in use), zeroes it, sets owner +0x5C = a1,
	// descriptor index +0x6A = a2, anchor +0x6B = a3, counts it in the actor state (+0x14) and
	// appends it to the particle list with state 0 (0x7F74C0); returns the slot or 0
	uint32_t __cdecl ml_7F6290(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x260801C);
		int32_t idx = (int32_t)MEM<int16_t>(0x2607160);
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t st = MEM<uint32_t>(ACTOR_STATE);
				U8(slot, 0x69) = 1;
				U16(st, 0x14) = (uint16_t)(U16(st, 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			idx++;
			if (idx >= 0x2C)
				idx = 0;
			tries++;
		} while (tries < 0x2D);
		idx++;
		if (idx < 0x2C)
			MEM<uint16_t>(0x2607160) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x2607160) = 0;
		return slot;
	}


	// 0x7F6330 (sub_7F6330, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl ml_7F6330(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x7F6CE0, a1);
		else if (mode == 4)
			callp(0x7F6E30, a1);
		ml_7F63B0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}


	// 0x7F63B0 (sub_7F63B0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl ml_7F63B0(uint32_t a1)
	{
		uint32_t sys = U32(0x2608040, 0);
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
				ml_7F6430(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			ml_7F6430(a1, def, (uint32_t)rest);
		return 0; // void
	}


	// 0x7F6430 (sub_7F6430, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl ml_7F6430(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x2608030, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x2608030, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = ml_7F6C10(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x2608040, 0);
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
					// 0x738110: random direction over the whole sphere
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2C) = (uint16_t)(x::CrtRand() & 0xFFF);
					e6_rotate_zxy(sp, sp + 0x28);
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
					// 0x7382AD: random direction on a ring (angles rand, 0, 0x400)
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = 0;
					U16(sp, 0x2C) = 0x400;
					e6_rotate_zxy(sp, sp + 0x28);
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
				// 0x738461: particle rotation angles sp+0x30 (random range, or a copy of the
				// spread angles; any other mode keeps the stale scratch words)
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					// quirk 0x73846D: copies the dwords sp+0x28 / sp+0x2C, including the word
					// +0x2E this function never writes (stale module scratch, deterministic;
					// +0x28..+0x2C are also stale when desc+0x34 is neither 1 nor 2)
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
		ml_7F7920(node, desc);
		U32(0x2608030, 0) = U32(0x2608030, 0) + 0x50;
		return 0; // void
	}


	// 0x7F6C10 (sub_7F6C10): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2608034] starting at the cursor 0x2608028, cleared, owner a1, definition
	// index a2) with the module's pool of 0x50 records (cursor wraps at 0x4F)
	uint32_t __cdecl ml_7F6C10(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x2608034);
		int32_t i = MEM<int16_t>(0x2608028);
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
			if (i >= 0x4F)
				i = 0;
			tries++;
			if (tries >= 0x50)
				break;
		}
		i++;
		if (i >= 0x4F)
			MEM<uint16_t>(0x2608028) = 0;
		else
			MEM<uint16_t>(0x2608028) = (uint16_t)i;
		return slot;
	}


	// 0x7F6E30 (sub_7F6E30, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl ml_7F6E30(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x2608040, 0);            // ecx
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
				uint32_t g = U32(0x2608040, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x2608040, 0);
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


	// 0x7F7920 (sub_7F7920, copy of a_743100 0x743100, callee chain to the module's particle pool 0x7F6290): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl ml_7F7920(uint32_t a1, uint32_t a2)
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
			ml_7F6290(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			ml_7F6290(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			ml_7F6290(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			ml_7F6290(obj, id, variant);
		}
		return 0; // void
	}


	// 0x7F79B0 (sub_7F79B0, copy of a_743190 0x743190, callee chain to the module's particle pool 0x7F6290): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via ml_7F6290(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl ml_7F79B0(void)
	{
		uint32_t dir = MEM<uint32_t>(0x2608040);  // eax (re-read only after the calls)
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
					ml_7F6290(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x2608040);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				ml_7F6290(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x2608040);
			}
		}
		return 0; // void
	}


	// 0x7F7A40 (sub_7F7A40, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl ml_7F7A40(uint32_t a1)
	{
		if (ml_7F3810(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}


	// 0x7F7A70 (MAG_059_sub_7F7A70): emitter state 1 - at tick 18 releases the master (root +0x63
	// = 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl ml_7F7A70(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x12)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}


	// 0x7F7A90 (MAG_059_sub_7F7A90): emitter state 2 - from tick 18: the action result (the heal) of the
	// action's current target (+0x2A action, +0x2B target record of 24 bytes), finished, next state
	uint32_t __cdecl ml_7F7A90(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x12)
			return 0; // void
		const int32_t ai = S8(node, 0x2A);
		const int32_t ti = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t recs = U32(acts + (uint32_t)(ai * 5) * 4, 8);
		x::ApplyActionResultToTarget(recs + (uint32_t)(ti * 3) * 8);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}


	// 0x7F7AE0 (MAG_059_sub_7F7AE0): master state 4 - action loop: once the emitter released the
	// master (+0x63 = 0): the next target record (+0x2B) and action counter +0x2E, back to the
	// emitter state; after the last one: wait 12 ticks (+0x60), next state
	uint32_t __cdecl ml_7F7AE0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return node;
		const int8_t t = S8(node, 0x2B);
		if ((int32_t)t < (int32_t)S16(node, 0x5A) - 1)
		{
			U8(node, 0x2B) = (uint8_t)(t + 1);
			U8(node, 0x2E) = (uint8_t)(U8(node, 0x2E) + 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) - 1);
			return node;
		}
		U16(node, 0x60) = 0xC;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return node;
	}


	// 0x7F7B30 (MAG_059_sub_7F7B30): master state - counts +0x60 down; at 0 starts the stage fade
	// out (0x7F7B60, 0x40 bytes, stage queue), next state
	uint32_t __cdecl ml_7F7B30(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x7F7B60, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x7F7BC0 (MAG_059_sub_7F7BC0): stage fade out state 0 - level +0x1C = 0x400, next state
	uint32_t __cdecl ml_7F7BC0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}


	// 0x7F7BE0 (MAG_059_sub_7F7BE0, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl ml_7F7BE0(uint32_t a1)
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

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7F2A70, (void *)a_73A0D0, "059 MAG_059_sub_7F2A70" },
		{ 0x7F2A80, (void *)a_73A0D0, "059 MAG_059_sub_7F2A80" },
		{ 0x7F2B20, (void *)a_73A380, "059 MAG_059_sub_7F2B20" },
		{ 0x7F2B80, (void *)a_7335D0, "059 MAG_059_sub_7F2B80" },
		{ 0x7F2C00, (void *)a_73A6D0, "059 nullsub_1480" },
		{ 0x7F2C10, (void *)a_73A170, "059 MAG_059_sub_7F2C10" },
		{ 0x7F2D00, (void *)a_73C100, "059 MAG_059_sub_7F2D00" },
		{ 0x7F2D50, (void *)a_73A380, "059 MAG_059_sub_7F2D50" },
		{ 0x7F3280, (void *)a_73FE90, "059 sub_7F3280" },
		{ 0x7F35B0, (void *)a_740210, "059 sub_7F35B0" },
		{ 0x7F3A40, (void *)a_7355E0, "059 sub_7F3A40" },
		{ 0x7F4010, (void *)a_735BB0, "059 sub_7F4010" },
		{ 0x7F4250, (void *)a_735DF0, "059 sub_7F4250" },
		{ 0x7F5130, (void *)a_740910, "059 sub_7F5130" },
		{ 0x7F5790, (void *)a_740F70, "059 sub_7F5790" },
		{ 0x7F57E0, (void *)a_740FC0, "059 sub_7F57E0" },
		{ 0x7F5870, (void *)a_741050, "059 sub_7F5870" },
		{ 0x7F5940, (void *)a_741120, "059 sub_7F5940" },
		{ 0x7F6380, (void *)a_741B60, "059 sub_7F6380" },
		{ 0x7F6AB0, (void *)a_742290, "059 au_re__rand_34" },
		{ 0x7F6AE0, (void *)a_7422C0, "059 sub_7F6AE0" },
		{ 0x7F6B20, (void *)a_742300, "059 sub_7F6B20" },
		{ 0x7F6B70, (void *)a_742350, "059 sub_7F6B70" },
		{ 0x7F6BC0, (void *)a_7423A0, "059 au_re__rand_34_0" },
		{ 0x7F6CD0, (void *)a_7424B0, "059 sub_7F6CD0" },
		{ 0x7F6CE0, (void *)a_7424C0, "059 sub_7F6CE0" },
		{ 0x7F74C0, (void *)a_742CA0, "059 sub_7F74C0" },
		{ 0x7F74F0, (void *)a_742CD0, "059 sub_7F74F0" },
		{ 0x7F7520, (void *)a_742D00, "059 sub_7F7520" },
		{ 0x7F76D0, (void *)a_742EB0, "059 sub_7F76D0" },
		{ 0x7F7810, (void *)a_742FF0, "059 sub_7F7810" },
		{ 0x7F7A60, (void *)a_73A6D0, "059 nullsub_1481" },
		{ 0x7F7AD0, (void *)a_73A6D0, "059 nullsub_1482" },
		{ 0x7F7B60, (void *)a_73A380, "059 MAG_059_sub_7F7B60" },
		{ 0x7F7C20, (void *)a_73A6D0, "059 nullsub_1483" },
		{ 0x7F7C30, (void *)a_747550, "059 MAG_059_sub_7F7C30" },
		{ 0x7F7C40, (void *)a_73A0D0, "059 MAG_059_sub_7F7C40" },
		{ 0x7F7C50, (void *)a_73A0D0, "059 MAG_059_sub_7F7C50" },
		{ 0x7F7C60, (void *)a_7475A0, "059 MAG_059_sub_7F7C60" },
		{ 0x7F7C80, (void *)a_73A6D0, "059 nullsub_1484" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x7F2930, (void *)ml_7F2930, "059 MAG_059_sub_7F2930" },
		{ 0x7F2A90, (void *)ml_7F2A90, "059 MAG_059_sub_7F2A90" },
		{ 0x7F2AE0, (void *)ml_7F2AE0, "059 MAG_059_sub_7F2AE0" },
		{ 0x7F2BC0, (void *)ml_7F2BC0, "059 MAG_059_sub_7F2BC0" },
		{ 0x7F2C40, (void *)ml_7F2C40, "059 MAG_059_sub_7F2C40" },
		{ 0x7F2CD0, (void *)ml_7F2CD0, "059 MAG_059_sub_7F2CD0" },
		{ 0x7F2DB0, (void *)ml_7F2DB0, "059 sub_7F2DB0" },
		{ 0x7F2DE0, (void *)ml_7F2DE0, "059 sub_7F2DE0" },
		{ 0x7F2F70, (void *)ml_7F2F70, "059 sub_7F2F70" },
		{ 0x7F3050, (void *)ml_7F3050, "059 sub_7F3050" },
		{ 0x7F31B0, (void *)ml_7F31B0, "059 sub_7F31B0" },
		{ 0x7F33A0, (void *)ml_7F33A0, "059 sub_7F33A0" },
		{ 0x7F3810, (void *)ml_7F3810, "059 sub_7F3810" },
		{ 0x7F38A0, (void *)ml_7F38A0, "059 sub_7F38A0" },
		{ 0x7F3900, (void *)ml_7F3900, "059 sub_7F3900" },
		{ 0x7F3B80, (void *)ml_7F3B80, "059 sub_7F3B80" },
		{ 0x7F3D90, (void *)ml_7F3D90, "059 sub_7F3D90" },
		{ 0x7F44F0, (void *)ml_7F44F0, "059 sub_7F44F0" },
		{ 0x7F4720, (void *)ml_7F4720, "059 sub_7F4720" },
		{ 0x7F49E0, (void *)ml_7F49E0, "059 sub_7F49E0" },
		{ 0x7F4C40, (void *)ml_7F4C40, "059 sub_7F4C40" },
		{ 0x7F4F20, (void *)ml_7F4F20, "059 sub_7F4F20" },
		{ 0x7F4FE0, (void *)ml_7F4FE0, "059 sub_7F4FE0" },
		{ 0x7F5060, (void *)ml_7F5060, "059 sub_7F5060" },
		{ 0x7F50A0, (void *)ml_7F50A0, "059 sub_7F50A0" },
		{ 0x7F60D0, (void *)ml_7F60D0, "059 sub_7F60D0" },
		{ 0x7F61E0, (void *)ml_7F61E0, "059 sub_7F61E0" },
		{ 0x7F6260, (void *)ml_7F6260, "059 sub_7F6260" },
		{ 0x7F6290, (void *)ml_7F6290, "059 sub_7F6290" },
		{ 0x7F6330, (void *)ml_7F6330, "059 sub_7F6330" },
		{ 0x7F63B0, (void *)ml_7F63B0, "059 sub_7F63B0" },
		{ 0x7F6430, (void *)ml_7F6430, "059 sub_7F6430" },
		{ 0x7F6C10, (void *)ml_7F6C10, "059 sub_7F6C10" },
		{ 0x7F6E30, (void *)ml_7F6E30, "059 sub_7F6E30" },
		{ 0x7F7920, (void *)ml_7F7920, "059 sub_7F7920" },
		{ 0x7F79B0, (void *)ml_7F79B0, "059 sub_7F79B0" },
		{ 0x7F7A40, (void *)ml_7F7A40, "059 sub_7F7A40" },
		{ 0x7F7A70, (void *)ml_7F7A70, "059 MAG_059_sub_7F7A70" },
		{ 0x7F7A90, (void *)ml_7F7A90, "059 MAG_059_sub_7F7A90" },
		{ 0x7F7AE0, (void *)ml_7F7AE0, "059 MAG_059_sub_7F7AE0" },
		{ 0x7F7B30, (void *)ml_7F7B30, "059 MAG_059_sub_7F7B30" },
		{ 0x7F7BC0, (void *)ml_7F7BC0, "059 MAG_059_sub_7F7BC0" },
		{ 0x7F7BE0, (void *)ml_7F7BE0, "059 MAG_059_sub_7F7BE0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag059_megalixir()
	{
		act::register_module(59);
		for (const act::megalixir::ModPort *p = act::megalixir::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(59, p->addr, p->port, p->name);
		for (const act::megalixir::ModPort *p = act::megalixir::PORTS; p->addr; p++)
			act::register_module_port(59, p->addr, p->port, p->name);
		// 30 fps layer: see mag059_megalixir_held.inc
		FX_HELD(register_mag059_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag059_megalixir_held.inc"
#endif
