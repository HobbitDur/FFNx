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

// Effect 46: Sigh (enemy attack 85 of kernel.bin, used by Vysage c0m054; MAG_046_*; no other
// kernel.bin entry uses effect 46): a copy of the actor/heal effect library (see act_engine.h)
// built like Dribble (mag041_dribble.cpp: the same master, emitter and actor-system code with the
// module's own globals and pools; the master and its pool set-up as Magma Breath's) plus the
// module's own sigh: two prim models turned three ways in front of the caster with scrolling
// textures, and a breath of rising smoke puffs.
//
// Setup MAG_046_SIGH 0x831B50 (runs once, not ported; file loader MAG_046_SIGH_FL 0x831B30 = the
// effect's data file (mag045.tim) -> 0x264EC60, TIM uploaded by the setup, packet arenas by tick
// parity (0x2652780 / 0x2652784 -> cursor 0x264EC64, 0x2651B78 / 0x2651B7C -> 0x264EC80), file
// arena cursor 0x26527AC) plays the camera animation 0x15C349C and creates the root queue
// 0x264ED50 with the master and five task pools: 0x2652610 (4 x 0x58 emitters), 0x2652370 (3 x
// 0x2A4 actor systems), 0x264ED60 (4 x 0x8C sigh tasks), 0x264EFA8 (0x50 x 0x8C breath) and
// 0x2652600 (10 x 0x40 stage tasks).
//   Master (0x831CB0) - camera copy 0x2793E58 (the module scratch stack 0x26527A0 grows down from
//     it), packet arena by tick parity, bone follow, 11-state table: 0x831E30 reserves the actor
//     pools after the file arena (0x870 + 0x10680 bytes: 20 particles of 0x6C and 100 actors of
//     0x2A0, cleared by 0x831E80) and starts the stage task 0x831EC0 (engine 0x73A380: stage
//     wobble 0x831F60), 0x831FB0 one emitter per action, 0x8363B0 next action once the emitter
//     released the master, 0x8363F0 waits 5 ticks and starts the closing stage task 0x836420
//     (0x836480 / 0x8364A0), then the engine's end states.
//   Emitter (0x831FE0), one per action: bone follow + model bounds, sound 0x15B82A0 at its first
//     tick; state 0 (0x832070) spawns the actor system (0x832100 = engine 0x73A380 with the actor
//     data 0x15C3398, mode 0x2D) and the sigh task 0x8359C0, state 1 releases the master at tick
//     0x28, state 2 applies the damage of the action's targets (0x506690) from tick 0x28.
//   Actor system (0x832100 -> 0x832160 / 0x835990): the engine's particle actors (sprite sequences
//     0x832F30 drawn into the module arena; the prim-model actor draw 0x832CD0 calls the module's
//     0x832E10, which draws nothing).
//   Sigh (0x8359C0, node 0x8C): set-up 0x835CF0 in front of the caster (two texture scroll
//     sources copied into VRAM), from tick 4 shown with the breath spawner 0x835E90, after tick
//     0x16 the fade 0x8362C0 (table 0x15C3550) to the end. Each shown tick: two VRAM texture
//     scrolls (0x835C70), the colour frame +0x82 (0..0x10) of the tables 0x15BCD10 / 0x15C0D14
//     written into the two prim models 0x15C0D58 / 0x15C1858 (0x835C20), and both models drawn
//     at the three angles +0x7C / +0x7E / +0x80 (0x835B30, Effect_RenderPrimModel).
//   Breath (spawner 0x835E90 = Siren's 0x73F250 task on {0x835EF0, 0x835F00, 0x835F20, ret}): from
//     its tick 8 bursts (count table 0x15C353C) of puffs 0x836090 (0x836160: one of three
//     flipbooks at random; 0x8361C0: rise, x / z slowed by 1/16, flipbook to its end 0x836250),
//     drawn as sprite sequences 0x8360F0.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x264EC60..0x26527B4 (file pointer 0x264EC60, packet cursor 0x264EC64, pools,
// queues, arenas, particle pool 0x265278C / cursor 0x264EFA0, actor pool 0x26527A4 / cursor
// 0x2652798, actor state 0x26527B0, scratch stack pointer 0x26527A0), the module scratch stack
// below the camera copy 0x2793E58.
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (sg_XXXXXX), the library ones
// the text of the matching Dribble / Magma Breath / Melting Bubble / Curaga / Siren / Heartbreak
// port with this module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace sigh
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_046 = { "sigh", 46, 0x831B30, 0x836570,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2652798, 0x26527A4, 0x0, 0x0, 0x264EC60, 0x0, 0x0, 0x0, 0x264EC84, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2652370, 0x0, 0x2652610, 0x0, 0x0, 0x0, 0x0, 0x0, 0x265278A, 0x0, 0x0, 0x265279A, 0x0, 0x26527A0, 0x0, 0x26527B0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x832190, 0x832BE0, 0x832C70, 0x832CD0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x832E70, 0x832FB0, 0x832FF0, 0x834280, 0x8342D0, 0x834300, 0x834380, 0x834A70, 0x834AC0, 0x834B60, 0x834C20, 0x835470, 0x835870, 0x835900, 0x0, 0x0, 0x0, 0x0, 0x0, 0x831FE0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x832340, 0x832420, 0x832580, 0x832650, 0x832770, 0x832980, 0x832F30, 0x833080, 0x8336E0, 0x833730, 0x8337C0, 0x833890, 0x834020, 0x834130, 0x8341B0, 0x8341E0, 0x834A00, 0x834A30, 0x834B10, 0x834C30, 0x834D80, 0x835410, 0x835440, 0x835620, 0x835760, 0x0, 0x832BE0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x264EC64;   // module packet cursor (tick parity)
	static const uint32_t ACTOR_STATE = 0x26527B0;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x26527A0;      // module scratch stack pointer
	static const uint32_t Q_EMITTER = 0x2652610, Q_ACTORS = 0x2652370, Q_SIGH = 0x264ED60, Q_PART = 0x264EFA8, Q_STAGE = 0x2652600;
	static const uint32_t ORIG_Master = 0x831CB0;
	static const uint32_t ORIG_Emitter = 0x831FE0;
	static const uint32_t ORIG_ActorSystem = 0x832100; // actor system task (engine task a_73A380)
	static const uint32_t ORIG_Sigh = 0x8359C0;
	static const uint32_t ORIG_Spawner = 0x835E90;     // breath spawner
	static const uint32_t ORIG_Breath = 0x836090;      // breath puff
	static const uint32_t ACTOR_DATA = 0x15C3398;      // the actor system's data
	static const uint32_t MODEL_A = 0x15C0D58, MODEL_B = 0x15C1858;          // the sigh's two prim models
	static const uint32_t COLOR_TABLE_A = 0x15BCD10, COLOR_TABLE_B = 0x15C0D14; // their colour frames (17 each)
	static const uint32_t BURST_TABLE = 0x15C353C;     // breath puffs per burst (s16, 0-terminated)
	static const uint32_t FADE_TABLE = 0x15C3550;      // sigh fade by fade tick (s16)
	static const uint32_t FLIPBOOK_0 = 0x15B82A4, FLIPBOOK_1 = 0x15B8438, FLIPBOOK_2 = 0x15B8BA4; // breath puffs
	static const void *const SOUND_Sigh = (const void *)0x15B82A0;
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t TransformCameraByShadowRotation(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(a1, a2, a3); }
		static inline uint32_t InitEffectSequenceFromData(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(a1, a2, a3, a4); }
	}

	// helpers of the library code (as in mag041_dribble.cpp)
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
	uint32_t __cdecl sg_831CB0(uint32_t a1);
	uint32_t __cdecl sg_831E30(uint32_t a1);
	uint32_t __cdecl sg_831E80(void);
	uint32_t __cdecl sg_831F60(uint32_t a1);
	uint32_t __cdecl sg_831FE0(uint32_t a1);
	uint32_t __cdecl sg_832070(uint32_t a1);
	uint32_t __cdecl sg_832160(uint32_t a1);
	uint32_t __cdecl sg_832190(uint32_t a1);
	uint32_t __cdecl sg_832340(uint32_t a1);
	uint32_t __cdecl sg_832420(void);
	uint32_t __cdecl sg_832580(void);
	uint32_t __cdecl sg_832770(void);
	uint32_t __cdecl sg_832BE0(uint32_t a1);
	uint32_t __cdecl sg_832C70(void);
	uint32_t __cdecl sg_832CD0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_832E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl sg_832E70(void);
	uint32_t __cdecl sg_832F30(uint32_t a1);
	uint32_t __cdecl sg_832FB0(void);
	uint32_t __cdecl sg_832FF0(uint32_t a1);
	uint32_t __cdecl sg_834020(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_834130(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_8341B0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_8341E0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl sg_834280(uint32_t a1);
	uint32_t __cdecl sg_834300(uint32_t a1);
	uint32_t __cdecl sg_834380(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl sg_834D80(uint32_t a1);
	uint32_t __cdecl sg_835870(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_835900(void);
	uint32_t __cdecl sg_835990(uint32_t a1);
	uint32_t __cdecl sg_8359C0(uint32_t a1);
	uint32_t __cdecl sg_835B30(uint32_t a1);
	uint32_t __cdecl sg_835C20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl sg_835C70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7);
	uint32_t __cdecl sg_835CF0(uint32_t a1);
	uint32_t __cdecl sg_835E30(uint32_t a1);
	uint32_t __cdecl sg_835E90(uint32_t a1);
	uint32_t __cdecl sg_835F00(uint32_t a1);
	uint32_t __cdecl sg_835F20(uint32_t a1);
	uint32_t __cdecl sg_836090(uint32_t a1);
	uint32_t __cdecl sg_8360F0(uint32_t a1);
	uint32_t __cdecl sg_836160(uint32_t a1);
	uint32_t __cdecl sg_8361C0(uint32_t a1);
	uint32_t __cdecl sg_8362A0(uint32_t a1);
	uint32_t __cdecl sg_8362C0(uint32_t a1);
	uint32_t __cdecl sg_836310(uint32_t a1);
	uint32_t __cdecl sg_836330(uint32_t a1);
	uint32_t __cdecl sg_8363B0(uint32_t a1);
	uint32_t __cdecl sg_8363F0(uint32_t a1);
	uint32_t __cdecl sg_836480(uint32_t a1);
	uint32_t __cdecl sg_8364A0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag046_sigh_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace sigh
{

	// ====================================================================================
	// library functions of this copy (adapted from the matching ports)
	// ====================================================================================

	// 0x824E40 (copy of cl_873740 0x873740: MAG_030_sub_873740): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the five queues (live task count -> node +0x5E; the actor counters
	// 0x26D6A22 / 0x26D6A12 are cleared before the queues run)
	uint32_t __cdecl sg_831CB0(uint32_t a1)
	{
		g_mod = &MOD_046;
		// 30 fps layer: see mag046_sigh_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x264EC84) = 0x2793E58;
		states[0] = 0x831E10;
		states[1] = 0x831E20;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x831E30;
		states[3] = 0x831FB0;
		states[4] = 0x8363B0;
		states[5] = 0x8363F0;
		states[6] = 0x8364F0;
		states[7] = 0x836500;
		states[8] = 0x836530;
		states[9] = 0x836540;
		states[10] = 0x836560; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2652784);
			const uint32_t v2 = MEM<uint32_t>(0x2651B7C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x264EC80) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2652780);
			const uint32_t v2 = MEM<uint32_t>(0x2651B78);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x264EC80) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x265279A) = 0;
		MEM<uint16_t>(0x265278A) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SIGH));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PART));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x824FC0 (MAG_048_sub_824FC0; = Counter Laser-Eye 0x8738C0 with 100 actors): master state -
	// once (+0x28 clear) reserves the actor pools after the file arena (0x870 + 0x10680 bytes = 20
	// particles of 0x6C and 100 actors of 0x2A0, cleared by 0x825010) and starts the stage light
	// ramp up (task 0x825050), next state
	uint32_t __cdecl sg_831E30(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x26527AC);
		MEM<uint32_t>(0x265278C) = p;
		p += 0x870;
		MEM<uint32_t>(0x26527A4) = p;
		p += 0x10680;
		MEM<uint32_t>(0x26527AC) = p;
		sg_831E80();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x831EC0, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x825010 (MAG_048_sub_825010; = Counter Laser-Eye 0x873910 with 100 actors): clears the
	// particle pool 0x26495C4 (0x870) and the actor pool 0x26495DC (0x10680), zeroes the particle
	// cursor 0x26489A8, the actor cursor 0x26495D0 and the words 0x26495C0 / 0x26481BC
	uint32_t __cdecl sg_831E80(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x265278C), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26527A4), 0x10680);
		MEM<uint16_t>(0x264EFA0) = 0;
		MEM<uint16_t>(0x2652798) = 0;
		MEM<uint16_t>(0x2652788) = 0;
		MEM<uint16_t>(0x264EC68) = 0;
		return 0; // void
	}

	// 0x87DEF0 (MAG_028_sub_87DEF0; = Drain r_8517B0): stage wobble state 1 - amplitude +0x1C up by 0x100 per tick to
	// 0x400 (then finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl sg_831F60(uint32_t a1)
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

	// 0x839C60 (MAG_043_sub_839C60; = Shell 0x866470): EMITTER task (one per action) - bone-follow
	// anchor and model bounds, state {0x839CF0 actor system, 0x83EB80 release the master, 0x83EBA0
	// damage, ret}, sound at its first tick
	uint32_t __cdecl sg_831FE0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x832070;
		states[1] = 0x836310;
		states[2] = 0x836330;
		states[3] = 0x8363A0; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Sigh), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x846A80 (copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl sg_832160(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			sg_832190(a1);
			sg_832BE0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x846AB0 (copy of cg_87E230 0x87E230: MAG_028_CURAGA_MainVisual_SetupRenderWorkspace; = Confuse c_85FBD0, copy of a_73F990 0x73F990; engine; Siren sub_73F990, all 6 actor modules): actor director set-up for node a1:
	// *G_258FB78 = state block node+0x34; copies node+0x29A/0x29C(mode)/0x29E into it, points its 4
	// tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once by 0x740210,
	// flag data+0), fills the target slot list (+4.., count = root +0x5A) from the cast context's
	// action record, sets start / per-target / bounds positions (0x73FC20, 0x73FFB0; modes 1/3/4
	// extra set-ups) and stores the caster-to-first-target distance in state+0.
	uint32_t __cdecl sg_832190(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(0x26527B0) = st;
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
			st = MEM<uint32_t>(0x26527B0);
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
		sg_832420();
		st = MEM<uint32_t>(0x26527B0);
		if (U16(st, 0x24) == 4)
			callp(0x832340, node);
		sg_832770();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(0x26527B0);
		if (U16(st, 0x24) == 1)
		{
			callp(0x832580, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(0x26527B0);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(0x26527B0);
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
		st = MEM<uint32_t>(0x26527B0);
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x846C60 (copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl sg_832340(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x26527B0);
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

	// 0x846D40 (copy of cg_87E4C0 0x87E4C0: sub_87E4C0; = Confuse c_85FE60): the actor system's reference points from the caster (slot state +0x1E):
	// +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same on the ground (y 0),
	// +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C (16.16 each);
	// mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl sg_832420(void)
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

	// 0x846EA0 (copy of cg_87E620 0x87E620: sub_87E620; = Confuse c_85FFC0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl sg_832580(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x26527B0);
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

	// 0x847090 (copy of cg_87E810 0x87E810: sub_87E810; = Confuse c_8601B0): per target of the actor system (slots state +4.., count +0x1C) the
	// target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its ground point),
	// +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride 0x10;
	// then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl sg_832770(void)
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

	// 0x87EC80 (MAG_028_CURAGA_MainVisual_Advance; = Confuse c_860620, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl sg_832BE0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x26527B0, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			sg_835900();
			sg_832FB0();
			// 30 fps layer: see mag046_sigh_held.inc
			FX_HELD(held_note_system(sys);)
			sg_832E70();
			sg_832C70();
			sys = U32(0x26527B0, 0);
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
		U16(0x265279A, 0) = (uint16_t)(U16(0x265279A, 0) + c14);
		U16(0x265278A, 0) = (uint16_t)(U16(0x265278A, 0) + c16);
		return done;
	}

	// 0x8476F0 (copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl sg_832C70(void)
	{
		uint32_t act = U32(U32(0x26527B0, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x26527B0, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					sg_832CD0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x847750 (copy of cg_87ED70 0x87ED70: sub_87ED70; = Confuse c_860710): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl sg_832CD0(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(PACKET_CURSOR) = sg_832E10(ctx, ot, 2, cursor);
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
				const uint32_t r = sg_832E10(saved_ctx, ot, 2, cursor);
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


	// 0x848D70 (copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl sg_832E70(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x26527B0), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x26527B0);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						sg_832F30(act);   // (the original also pushes the bone entry, unused)
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
							sg_832F30(act);
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

	// 0x848E30 (copy of cg_880450 0x880450: sub_880450; = Confuse c_861DF0): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl sg_832F30(uint32_t a1)
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

	// 0x848EB0 (copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl sg_832FB0(void)
	{
		uint32_t act = U32(U32(0x26527B0, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				sg_834280(act);
			else if (type == 1)
				sg_832FF0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl sg_832FF0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x26527B0);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (sg_834130(act, bone) == 0)
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
			st = MEM<uint32_t>(0x26527B0);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x849F20 (copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl sg_834020(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x26527B0, 0);                  // actor state block
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
				gx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				gx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				gx::sub_45E0B0(in2);
				gx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x84A030 (copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl sg_834130(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				sg_8341B0(a1, a2);
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
			sg_8341B0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			sg_8341B0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x84A0B0 (copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl sg_8341B0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return sg_8341E0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x84A0E0 (copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl sg_8341E0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x265278C);
		int32_t idx = MEM<int16_t>(0x264EFA0);
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
			if (idx >= 0x13)
				idx = 0;
			tries++;
		} while (tries < 0x14);
		idx++;
		if (idx < 0x13)
			MEM<uint16_t>(0x264EFA0) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x264EFA0) = 0;
		return slot;
	}

	// 0x84A180 (copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl sg_834280(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x834C30, a1);
		else if (mode == 4)
			callp(0x834D80, a1);
		sg_834300(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x84A200 (copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl sg_834300(uint32_t a1)
	{
		uint32_t sys = U32(0x26527B0, 0);
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
				sg_834380(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			sg_834380(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x84A280 (copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl sg_834380(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x26527A0, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x26527A0, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = a_7387B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x26527B0, 0);
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
		sg_835870(node, desc);
		U32(0x26527A0, 0) = U32(0x26527A0, 0) + 0x50;
		return 0; // void
	}

	// 0x84AC80 (copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl sg_834D80(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x26527B0, 0);            // ecx
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
				uint32_t g = U32(0x26527B0, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x26527B0, 0);
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

	// 0x84B770 (copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl sg_835870(uint32_t a1, uint32_t a2)
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
			sg_8341E0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			sg_8341E0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			sg_8341E0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			sg_8341E0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x84B800 (copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via sg_8341E0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl sg_835900(void)
	{
		uint32_t dir = MEM<uint32_t>(0x26527B0);  // eax (re-read only after the calls)
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
					sg_8341E0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x26527B0);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				sg_8341E0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x26527B0);
			}
		}
		return 0; // void
	}

	// 0x84B890 (copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl sg_835990(uint32_t a1)
	{
		if (sg_832BE0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}


	// 0x73F250 (module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl sg_835E90(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x835EF0, 0x835F00, 0x835F20, 0x836290 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}


	// 0x873010 (copy of cl_878F20 0x878F20: copy of t_767290: 0x767290 (module 090 sub_767290)): ring particle task (states 0x767410/0x767430) drawn by
	// t_7672F0; ends when finished and no child alive.
	uint32_t __cdecl sg_836090(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x836160;
		states[1] = 0x8361C0;
		states[2] = 0x836280;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag046_sigh_held.inc
		FX_HELD(held_note_puff(node);)
		sg_8360F0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x84CB00 (copy of cl_878F80 0x878F80: sub_878F80; = Cure's MAG_001_CURE_DrawSprite 0x8D6F20): draws the node's flipbook
	// +0x4C frame +0x50 at +0x1C (camera-facing, angle +0x54) as a sprite sequence into the module
	// arena, unless hidden (+0x26 bit 2)
	uint32_t __cdecl sg_8360F0(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		const uint32_t hdr = x::Field_Alloc(0xB4);
		gx::TransformCameraByShadowRotation(a1 + 0x1C, 0x1000, (uint32_t)(int32_t)S16(a1, 0x54));
		U32(hdr, 0) = U32(a1, 0x4C);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		U16(hdr, 4) = U16(a1, 0x50);
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = gx::InitEffectSequenceFromData(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, cursor);
		x::Field_Free(0xB4);
		return 0; // void
	}


	// 0x882F90 (MAG_028_sub_882F90; = Drain r_856870): master state - counts +0x60 down; at 0 starts the stage wobble
	// out (0x882FC0, 0x40 bytes, stage queue), next state
	uint32_t __cdecl sg_8363F0(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x836420, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x883020 (MAG_028_sub_883020; = Drain r_856900): stage wobble out state 0 - amplitude +0x1C = 0x400, next state
	uint32_t __cdecl sg_836480(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x883040 (MAG_028_sub_883040; = Drain r_856920, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl sg_8364A0(uint32_t a1)
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
	// the module's own code
	// ====================================================================================

	// 0x832070 (MAG_046_sub_832070): emitter state 0 - the actor system (0x832100 = engine task
	// 0x73A380, actor data 0x15C3398, start tick 0, mode 0x2D, flag 0) through the engine spawner
	// 0x8320B0 and the sigh task 0x8359C0 (0x8C bytes, queue 0x264ED60), next state
	uint32_t __cdecl sg_832070(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, ACTOR_DATA, 0, 0x2D, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_SIGH, ORIG_Sigh, 0x8C, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x832E10 (sub_832E10): the module's prim-model actor "renderer" (the library's draw of a
	// prim-model actor calls it with (header, OT, 2, cursor)): only prepares the header (+4 packet
	// list = model + 8 unless flag 0x2000, +0x20 = model + its first word, +0x18 = 0 unless flag
	// 0x1000), sets the GTE far colour from the header's rgb bytes +8..+0xA and steps +0x20 by
	// 0x20; draws nothing and returns the cursor unchanged
	uint32_t __cdecl sg_832E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a2;
		(void)a3;
		const uint32_t flags = U32(a1, 0x1C);
		if ((flags & 0x2000) == 0)
			U32(a1, 4) = U32(a1, 0) + 8;
		const uint32_t model = U32(a1, 0);
		U32(a1, 0x20) = U32(model, 0) + model;
		if ((flags & 0x1000) == 0)
			U32(a1, 0x18) = 0;
		x::someCameraWork_45DD60(U8(a1, 8), U8(a1, 9), U8(a1, 0xA));
		U32(a1, 0x20) = U32(a1, 0x20) + 0x20;
		return a4;
	}

	// 0x8359C0 (MAG_046_sub_8359C0): SIGH task (node 0x8C, one per emitter) - state table
	// {0x835CF0 set-up, 0x835E30 breath spawner, 0x8362A0 wait, 0x8362C0 fade, ret 0x836300};
	// while shown (+0x26 bit 2 clear): the two texture scrolls (VRAM blits 0x835C70: rows 0x2C0
	// by +0x84 += 8, rows 0x300 by +0x86 += 6), the frame +0x82 (0..0x10) of the two vertex
	// colour tables 0x15BCD10 / 0x15C0D14 copied into the two prim models +0x74 / +0x78
	// (0x835C20), and the two models drawn three times (angles +0x7C / +0x7E / +0x80 about y,
	// 0x835B30); frame counter++, ends when finished and no children
	uint32_t __cdecl sg_8359C0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x835CF0;
		states[1] = 0x835E30;
		states[2] = 0x8362A0;
		states[3] = 0x8362C0;
		states[4] = 0x836300; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			const uint32_t s1 = (uint8_t)(U8(node, 0x84) + 8);
			U16(node, 0x84) = (uint16_t)s1;
			sg_835C70(0x2C0, 0x100, 0x40, 0x100, 0x200, 0x100, s1);
			const uint32_t s2 = (uint8_t)(U8(node, 0x86) + 6);
			U16(node, 0x86) = (uint16_t)s2;
			sg_835C70(0x300, 0x100, 0x20, 0x100, 0x2A0, 0x100, s2);
			sg_835C20(U32(node, 0x74), MEM<uint32_t>(COLOR_TABLE_A + (uint32_t)(int32_t)S16(node, 0x82) * 4));
			sg_835C20(U32(node, 0x78), MEM<uint32_t>(COLOR_TABLE_B + (uint32_t)(int32_t)S16(node, 0x82) * 4));
			// 30 fps layer: see mag046_sigh_held.inc
			FX_HELD(held_note_sigh(node);)
			uint32_t ang = node + 0x7C;
			for (int k = 3; k != 0; k--, ang += 2)
			{
				U16(node, 0x46) = U16(ang, 0);
				U32(node, 0x4C) = U32(node, 0x74);
				sg_835B30(node);
				U32(node, 0x4C) = U32(node, 0x78);
				sg_835B30(node);
			}
			U16(node, 0x82) = (uint16_t)(U16(node, 0x82) + 1);
			if (S16(node, 0x82) > 0x10)
				U16(node, 0x82) = 0x10;
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x835B30 (sub_835B30): draws the prim model +0x4C (colour +0x40, fade +0x50, flags 0xF0)
	// at +0x1C turned by the angles +0x44 (x) / +0x46 (y) / +0x48 (z), scaled by +0x30, through
	// the camera, into the module arena; nothing while hidden (+0x26 bit 2)
	uint32_t __cdecl sg_835B30(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		uint32_t mbuf[8];
		const uint32_t m = P(mbuf);
		x::MAG_022_sub_8DD770(m);
		x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(a1, 0x46));
		x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(a1, 0x44));
		x::sub_8DD960(m, (uint32_t)(int32_t)S16(a1, 0x48));
		S32(m, 0x14) = S16(a1, 0x1C);
		S32(m, 0x18) = S16(a1, 0x1E);
		S32(m, 0x1C) = S16(a1, 0x20);
		x::scale3DMatrix(m, a1 + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		x::GTE_SetRotMatrix_W(m);
		x::GTE_SetTransVector_W(m);
		const uint32_t hdr = x::Field_Alloc(0x58);
		U32(hdr, 0) = U32(a1, 0x4C);
		U32(hdr, 8) = U32(a1, 0x40);
		U32(hdr, 0xC) = (uint32_t)(int32_t)S16(a1, 0x50);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(hdr, 0x1C) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, cursor);
		x::Field_Free(0x58);
		return 0; // void
	}

	// 0x835C20 (sub_835C20): copies the colour frame a2 (16 bytes per primitive) into the
	// primitive list of the prim model a1 (list index model[0] / 4: count at +0x1C + 4 * index,
	// 0x24-byte records from the next word, words +0 / +0x18 / +0x1C / +0x20)
	uint32_t __cdecl sg_835C20(uint32_t a1, uint32_t a2)
	{
		const int32_t idx = S32(a1, 0) / 4;
		int32_t n = S32(a1, 0x1C + idx * 4);
		uint32_t rec = a1 + 0x1C + (uint32_t)idx * 4 + 4;
		if (n > 0)
		{
			uint32_t src = a2;
			do
			{
				U32(rec, 0) = U32(src, 0);
				rec += 0x24;
				U32(rec, -0xC) = U32(src, 4);
				U32(rec, -8) = U32(src, 8);
				U32(rec, -4) = U32(src, 0xC);
				src += 0x10;
			} while (--n != 0);
		}
		return 0; // void
	}

	// 0x835C70 (sub_835C70): texture scroll - VRAM blit of the rectangle {a1, a2, a3, a4 - a7} to
	// (a5, a6 + a7), and when a7 != 0 the wrapped rows {a1, a2 + a4 - a7, a3, a7} to (a5, a6)
	uint32_t __cdecl sg_835C70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7)
	{
		uint16_t rect[4];
		rect[0] = (uint16_t)a1;
		rect[2] = (uint16_t)a3;
		rect[3] = (uint16_t)(a4 - a7);
		rect[1] = (uint16_t)a2;
		x::QueueBlitCommand(P(rect), a5, a7 + a6);
		if ((uint16_t)a7 != 0)
		{
			rect[1] = (uint16_t)(a2 - a7 + a4);
			rect[3] = (uint16_t)a7;
			x::QueueBlitCommand(P(rect), a5, a6);
		}
		return 0; // void
	}

	// 0x835CF0 (sub_835CF0): sigh state 0 - at the entity +0x2C (the caster): position (x / z of
	// the entity, y 0) moved 0x300 forward along its facing (matrix +0x54 = rotation y by the
	// entity +0x0E), scale 0x1000, the three draw angles facing -/+ 0x155, the two prim models
	// 0x15C0D58 / 0x15C1858, the two texture-scroll sources copied once into VRAM (0x200,0x100
	// 64 x 256 -> 0x2C0,0x100; 0x2A0,0x100 64 x 256 -> 0x300,0x100), hidden (+0x26 bit 2), next
	// state
	uint32_t __cdecl sg_835CF0(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2C));
		const uint32_t m = a1 + 0x54;
		U32(a1, 0x1C) = U32(ent, 0x1C);
		U32(a1, 0x20) = U32(ent, 0x20);
		x::MAG_022_sub_8DD770(m);
		if (U16(ent, 0xE) != 0)
			x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ent, 0xE));
		uint16_t vbuf[4];
		const uint32_t v = P(vbuf);
		U16(v, 0) = 0;
		U16(v, 2) = 0;
		U16(v, 4) = 0xFD00;
		x::matrixMultiplyVector(m, v, v);
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + U16(v, 4));
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + U16(v, 0));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + U16(v, 2));
		S32(a1, 0x30) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x38) = 0x1000;
		const uint16_t face = U16(ent, 0xE);
		U16(a1, 0x80) = face;
		U32(a1, 0x74) = MODEL_A;
		U32(a1, 0x78) = MODEL_B;
		U16(a1, 0x7C) = (uint16_t)(face - 0x155);
		U16(a1, 0x7E) = (uint16_t)(face + 0x155);
		uint16_t rect[4];
		rect[0] = 0x200;
		rect[1] = 0x100;
		rect[2] = 0x40;
		rect[3] = 0x100;
		x::QueueBlitCommand(P(rect), 0x2C0, 0x100);
		rect[0] = 0x2A0;
		rect[1] = 0x100;
		rect[2] = 0x40;
		rect[3] = 0x100;
		x::QueueBlitCommand(P(rect), 0x300, 0x100);
		U8(a1, 0x26) |= 4;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x835E30 (sub_835E30): sigh state 1 - from tick 4: shown, the breath spawner task 0x835E90
	// (queue 0x264EFA8) at the sigh's x / z (y 0) with its facing matrix (+0x54 -> +0x58), next
	// state
	uint32_t __cdecl sg_835E30(uint32_t a1)
	{
		if (S16(a1, 0x24) < 4)
			return 0; // void
		U8(a1, 0x26) &= 0xFB;
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_Spawner, 0x8C, a1);
		const uint16_t px = U16(a1, 0x1C);
		const uint16_t pz = U16(a1, 0x20);
		U16(t, 0x1C) = px;
		U16(t, 0x1E) = 0;
		U16(t, 0x20) = pz;
		memcpy((void *)(t + 0x58), (const void *)(a1 + 0x54), 0x20);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x835F00 (sub_835F00): breath spawner state 1 - at tick 8 the burst counter +0x88 = 0, next
	// state
	uint32_t __cdecl sg_835F00(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 8)
		{
			U16(a1, 0x88) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x835F20 (sub_835F20): breath spawner state 2 - burst +0x88 of the count table 0x15C353C
	// (0-terminated): that many breath particles 0x836090 (queue 0x264EFA8), each at the spawner
	// position plus the vector (0, 0, -0xC00) turned by its facing matrix and a random yaw of
	// -0x200..0x1FF; speed (+0x78, +0x7A, +0x7C) = that vector x random 0x400..0xBFF / 0x1000
	// with a random upward y 0..-0x3FF, rise acceleration +0x82 = -16..-31; after the last burst
	// finished, next state
	uint32_t __cdecl sg_835F20(uint32_t a1)
	{
		const uint32_t node = a1;
		uint16_t vbuf[4];
		const uint32_t v = P(vbuf);
		U16(v, 0) = 0;
		U16(v, 2) = 0;
		U16(v, 4) = 0xF400;
		if (MEM<int16_t>(BURST_TABLE + (uint32_t)(int32_t)S16(node, 0x88) * 2) > 0)
		{
			int32_t i = 0;
			do
			{
				uint32_t mbuf[8];
				const uint32_t m = P(mbuf);
				memcpy((void *)m, (const void *)(node + 0x58), 0x20);
				int32_t r = (int32_t)x::CrtRand();
				r = r % 0x400;
				r = (r - 0x200) & 0xFFF;
				x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)(int16_t)r);
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_Breath, 0x8C, node);
				const uint32_t pz = U32(node, 0x20);
				U32(t, 0x1C) = U32(node, 0x1C);
				U32(t, 0x20) = pz;
				uint16_t obuf[4];
				const uint32_t o = P(obuf);
				x::matrixMultiplyVector(m, v, o);
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + U16(o, 0));
				U16(t, 0x1E) = (uint16_t)(U16(t, 0x1E) + U16(o, 2));
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + U16(o, 4));
				r = (int32_t)x::CrtRand();
				const int32_t sp = (int16_t)((r & 0x7FF) + 0x400);
				U16(t, 0x78) = (uint16_t)((S16(o, 0) * sp) / 0x1000);
				U16(t, 0x7C) = (uint16_t)((S16(o, 4) * sp) / 0x1000);
				r = (int32_t)x::CrtRand();
				U16(t, 0x7A) = (uint16_t)(-(r & 0x3FF));
				r = (int32_t)x::CrtRand();
				U16(t, 0x82) = (uint16_t)(-16 - (r & 0xF));
				i++;
			} while (i < MEM<int16_t>(BURST_TABLE + (uint32_t)(int32_t)S16(node, 0x88) * 2));
		}
		U16(node, 0x88) = (uint16_t)(U16(node, 0x88) + 1);
		if (MEM<uint16_t>(BURST_TABLE + (uint32_t)(int32_t)S16(node, 0x88) * 2) == 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x836160 (sub_836160): breath particle state 0 - one of the three flipbooks 0x15B82A4 /
	// 0x15B8438 / 0x15B8BA4 at random (rand() % 3), sprite angle +0x52 = 0xE, next state
	uint32_t __cdecl sg_836160(uint32_t a1)
	{
		const int32_t r = (int32_t)x::CrtRand() % 3;
		if (r == 0)
			U32(a1, 0x4C) = FLIPBOOK_0;
		else if (r == 1)
			U32(a1, 0x4C) = FLIPBOOK_1;
		else if (r == 2)
			U32(a1, 0x4C) = FLIPBOOK_2;
		U16(a1, 0x52) = 0xE;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8361C0 (sub_8361C0): breath particle state 1 - y speed += the rise acceleration +0x82,
	// x / z speeds lose 1/16, position += speed / 16; when its flipbook ends (0x836250), finished,
	// next state
	uint32_t __cdecl sg_8361C0(uint32_t a1)
	{
		U16(a1, 0x7A) = (uint16_t)(U16(a1, 0x7A) + U16(a1, 0x82));
		int16_t vx = S16(a1, 0x78);
		int16_t vz = S16(a1, 0x7C);
		const int16_t vy = S16(a1, 0x7A);
		vx = (int16_t)(vx - vx / 16);
		U16(a1, 0x78) = (uint16_t)vx;
		vz = (int16_t)(vz - vz / 16);
		U16(a1, 0x7C) = (uint16_t)vz;
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + (uint16_t)(vx / 16));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + (uint16_t)(vy / 16));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + (uint16_t)(vz / 16));
		if (a_743C20(a1) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8362A0 (sub_8362A0): sigh state 2 - after tick 0x16, fade counter +0x88 = 0, next state
	uint32_t __cdecl sg_8362A0(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x16)
		{
			U16(a1, 0x88) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8362C0 (sub_8362C0): sigh state 3 - fade +0x50 from the table 0x15C3550 by the counter
	// +0x88; at 0x1000 finished, next state
	uint32_t __cdecl sg_8362C0(uint32_t a1)
	{
		U16(a1, 0x88) = (uint16_t)(U16(a1, 0x88) + 1);
		const uint16_t f = MEM<uint16_t>(FADE_TABLE + (uint32_t)(int32_t)S16(a1, 0x88) * 2);
		U16(a1, 0x50) = f;
		if ((int16_t)f >= 0x1000)
		{
			U8(a1, 0x26) |= 1;
			U16(a1, 0x50) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x836310 (MAG_046_sub_836310): emitter state 1 - at tick 0x28 releases the master (root
	// +0x63 = 0), next state
	uint32_t __cdecl sg_836310(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x28)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x836330 (MAG_046_sub_836330): emitter state 2 - from tick 0x28 the damage of every target
	// of the action +0x2A (0x506690), finished, next state
	uint32_t __cdecl sg_836330(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x28)
			return 0; // void
		uint32_t act = U32(U32(a1, 0xC), 4) + (uint32_t)(int32_t)S8(a1, 0x2A) * 20;
		if (U8(act, 0x10) != 0)
		{
			int32_t i = 0;
			uint32_t off = 0;
			do
			{
				x::ApplyActionResultToTarget(U32(act, 8) + off);
				i++;
				off += 0x18;
				act = U32(U32(a1, 0xC), 4) + (uint32_t)(int32_t)S8(a1, 0x2A) * 20;
			} while (i < (int32_t)U8(act, 0x10));
		}
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8363B0 (MAG_046_sub_8363B0): master state 4 - once the emitter released it (+0x63 = 0):
	// next action (+0x2A, +0x2E) back to the emitter state while actions remain (+0x58), else
	// the wait +0x60 = 5, next state
	uint32_t __cdecl sg_8363B0(uint32_t a1)
	{
		if (U8(a1, 0x63) != 0)
			return 0; // void
		const uint8_t k = U8(a1, 0x2A);
		if ((int16_t)(int8_t)k < S16(a1, 0x58))
		{
			U8(a1, 0x2A) = (uint8_t)(k + 1);
			U8(a1, 0x2E) = (uint8_t)(U8(a1, 0x2E) + 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) - 1);
			return 0; // void
		}
		U16(a1, 0x60) = 5;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x831E10, (void *)a_73A0D0, "046 MAG_046_sub_831E10" },
		{ 0x831E20, (void *)a_73A0D0, "046 MAG_046_sub_831E20" },
		{ 0x831EC0, (void *)a_73A380, "046 MAG_046_sub_831EC0" },
		{ 0x831F20, (void *)a_7335D0, "046 MAG_046_sub_831F20" },
		{ 0x831FA0, (void *)a_73A6D0, "046 nullsub_1568" },
		{ 0x831FB0, (void *)a_73A170, "046 MAG_046_sub_831FB0" },
		{ 0x8320B0, (void *)a_73C100, "046 MAG_046_sub_8320B0" },
		{ 0x832100, (void *)a_73A380, "046 MAG_046_sub_832100" },
		{ 0x832650, (void *)a_73FE90, "046 sub_832650" },
		{ 0x832980, (void *)a_740210, "046 sub_832980" },
		{ 0x833080, (void *)a_740910, "046 sub_833080" },
		{ 0x8336E0, (void *)a_740F70, "046 sub_8336E0" },
		{ 0x833730, (void *)a_740FC0, "046 sub_833730" },
		{ 0x8337C0, (void *)a_741050, "046 sub_8337C0" },
		{ 0x833890, (void *)a_741120, "046 sub_833890" },
		{ 0x8342D0, (void *)a_741B60, "046 sub_8342D0" },
		{ 0x834A00, (void *)a_742290, "046 au_re__rand_45" },
		{ 0x834A30, (void *)a_7422C0, "046 sub_834A30" },
		{ 0x834A70, (void *)a_742300, "046 sub_834A70" },
		{ 0x834AC0, (void *)a_742350, "046 sub_834AC0" },
		{ 0x834B10, (void *)a_7423A0, "046 au_re__rand_45_0" },
		{ 0x834B60, (void *)a_7387B0, "046 sub_834B60" },
		{ 0x834C20, (void *)a_7424B0, "046 sub_834C20" },
		{ 0x834C30, (void *)a_7424C0, "046 sub_834C30" },
		{ 0x835410, (void *)a_742CA0, "046 sub_835410" },
		{ 0x835440, (void *)a_742CD0, "046 sub_835440" },
		{ 0x835470, (void *)a_742D00, "046 sub_835470" },
		{ 0x835620, (void *)a_742EB0, "046 sub_835620" },
		{ 0x835760, (void *)a_742FF0, "046 sub_835760" },
		{ 0x8359B0, (void *)a_73A6D0, "046 nullsub_1569" },
		{ 0x835EF0, (void *)a_73A0D0, "046 sub_835EF0" },
		{ 0x836250, (void *)a_743C20, "046 sub_836250" },
		{ 0x836280, (void *)a_73A6D0, "046 nullsub_1570" },
		{ 0x836290, (void *)a_73A6D0, "046 nullsub_1571" },
		{ 0x836300, (void *)a_73A6D0, "046 nullsub_1572" },
		{ 0x8363A0, (void *)a_73A6D0, "046 nullsub_1573" },
		{ 0x836420, (void *)a_73A380, "046 MAG_046_sub_836420" },
		{ 0x8364E0, (void *)a_73A6D0, "046 nullsub_1575" },
		{ 0x8364F0, (void *)a_747550, "046 MAG_046_sub_8364F0" },
		{ 0x836500, (void *)a_747560, "046 MAG_046_sub_836500" },
		{ 0x836530, (void *)a_747590, "046 MAG_046_sub_836530" },
		{ 0x836540, (void *)a_7475A0, "046 MAG_046_sub_836540" },
		{ 0x836560, (void *)a_73A6D0, "046 nullsub_1574" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x831CB0, (void *)sg_831CB0, "046 sg_831CB0" },
		{ 0x831E30, (void *)sg_831E30, "046 sg_831E30" },
		{ 0x831E80, (void *)sg_831E80, "046 sg_831E80" },
		{ 0x831F60, (void *)sg_831F60, "046 sg_831F60" },
		{ 0x831FE0, (void *)sg_831FE0, "046 sg_831FE0" },
		{ 0x832070, (void *)sg_832070, "046 sg_832070" },
		{ 0x832160, (void *)sg_832160, "046 sg_832160" },
		{ 0x832190, (void *)sg_832190, "046 sg_832190" },
		{ 0x832340, (void *)sg_832340, "046 sg_832340" },
		{ 0x832420, (void *)sg_832420, "046 sg_832420" },
		{ 0x832580, (void *)sg_832580, "046 sg_832580" },
		{ 0x832770, (void *)sg_832770, "046 sg_832770" },
		{ 0x832BE0, (void *)sg_832BE0, "046 sg_832BE0" },
		{ 0x832C70, (void *)sg_832C70, "046 sg_832C70" },
		{ 0x832CD0, (void *)sg_832CD0, "046 sg_832CD0" },
		{ 0x832E10, (void *)sg_832E10, "046 sg_832E10" },
		{ 0x832E70, (void *)sg_832E70, "046 sg_832E70" },
		{ 0x832F30, (void *)sg_832F30, "046 sg_832F30" },
		{ 0x832FB0, (void *)sg_832FB0, "046 sg_832FB0" },
		{ 0x832FF0, (void *)sg_832FF0, "046 sg_832FF0" },
		{ 0x834020, (void *)sg_834020, "046 sg_834020" },
		{ 0x834130, (void *)sg_834130, "046 sg_834130" },
		{ 0x8341B0, (void *)sg_8341B0, "046 sg_8341B0" },
		{ 0x8341E0, (void *)sg_8341E0, "046 sg_8341E0" },
		{ 0x834280, (void *)sg_834280, "046 sg_834280" },
		{ 0x834300, (void *)sg_834300, "046 sg_834300" },
		{ 0x834380, (void *)sg_834380, "046 sg_834380" },
		{ 0x834D80, (void *)sg_834D80, "046 sg_834D80" },
		{ 0x835870, (void *)sg_835870, "046 sg_835870" },
		{ 0x835900, (void *)sg_835900, "046 sg_835900" },
		{ 0x835990, (void *)sg_835990, "046 sg_835990" },
		{ 0x8359C0, (void *)sg_8359C0, "046 sg_8359C0" },
		{ 0x835B30, (void *)sg_835B30, "046 sg_835B30" },
		{ 0x835C20, (void *)sg_835C20, "046 sg_835C20" },
		{ 0x835C70, (void *)sg_835C70, "046 sg_835C70" },
		{ 0x835CF0, (void *)sg_835CF0, "046 sg_835CF0" },
		{ 0x835E30, (void *)sg_835E30, "046 sg_835E30" },
		{ 0x835E90, (void *)sg_835E90, "046 sg_835E90" },
		{ 0x835F00, (void *)sg_835F00, "046 sg_835F00" },
		{ 0x835F20, (void *)sg_835F20, "046 sg_835F20" },
		{ 0x836090, (void *)sg_836090, "046 sg_836090" },
		{ 0x8360F0, (void *)sg_8360F0, "046 sg_8360F0" },
		{ 0x836160, (void *)sg_836160, "046 sg_836160" },
		{ 0x8361C0, (void *)sg_8361C0, "046 sg_8361C0" },
		{ 0x8362A0, (void *)sg_8362A0, "046 sg_8362A0" },
		{ 0x8362C0, (void *)sg_8362C0, "046 sg_8362C0" },
		{ 0x836310, (void *)sg_836310, "046 sg_836310" },
		{ 0x836330, (void *)sg_836330, "046 sg_836330" },
		{ 0x8363B0, (void *)sg_8363B0, "046 sg_8363B0" },
		{ 0x8363F0, (void *)sg_8363F0, "046 sg_8363F0" },
		{ 0x836480, (void *)sg_836480, "046 sg_836480" },
		{ 0x8364A0, (void *)sg_8364A0, "046 sg_8364A0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag046_sigh()
	{
		act::register_module(46);
		for (const act::sigh::ModPort *p = act::sigh::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(46, p->addr, p->port, p->name);
		for (const act::sigh::ModPort *p = act::sigh::PORTS; p->addr; p++)
			act::register_module_port(46, p->addr, p->port, p->name);
		// 30 fps layer: see mag046_sigh_held.inc
		FX_HELD(register_mag046_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag046_sigh_held.inc"
#endif
