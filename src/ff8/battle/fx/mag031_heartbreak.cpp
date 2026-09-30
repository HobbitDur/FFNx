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

// Effect 31: Heartbreak (enemy attack 26 of kernel.bin, used by Creeps c0m023; no other kernel entry
// uses effect 31; MAG_031_*): a copy of the actor/heal effect library (see act_engine.h) built like
// Psycho Blast (mag023_psycho_blast.cpp: the same six-queue master) with Curaga's emitter, two screen
// flashes (Confuse's overlay task), and the module's own prim-model tasks (a heart, a ring, an arrow
// flying to the target, a burst) and sparks (sprite sequences).
//
// Setup MAG_031_HEARTBREAK_Init 0x86C9E0 (runs once, not ported; file loader MAG_031_HEARTBREAK_FL
// 0x86C9C0 = the effect's data file -> 0x26CDD8C, TIM uploaded and camera animation 0x160D28C unless
// the cast flags +1 bit 0; its two packet arenas at file + 0 / + 0xD000 (and + 0xD000 / + 0x1A000),
// file arena cursor 0x26D33DC = file + 0x1A000) creates the root queue 0x26CDE80 with the master and
// six task pools: 0x26D2C30 (4 x 0x58: emitters), 0x26CDCC8 (3 x 0x3C: screen flashes), 0x26D2990
// (3 x 0x2A4: actor systems), 0x26D2188 (10 x 0x1B0: prim-model tasks), 0x26CDE90 (100 x 0x80:
// spark spawner, sparks, shake task) and 0x26D2C20 (10 x 0x40: the stage light ramps).
//   Master (0x86CB50 = Psycho Blast 0x893190) - camera copy 0x2793E58 (the module scratch stack
//     0x26D33D0 grows down from it), packet arena by tick parity (cursor 0x26CDD90), bone follow,
//     11-state table: 0x86CCE0 reserves the actor pools after the arena (0x438 + 0x8340 bytes = 10
//     particles of 0x6C and 50 actors of 0x2A0, cleared) and starts the light ramp up (0x86CD70:
//     the four stage light words 0x1D98992 + k * 0x2C from 0 to 0x400 by 0x100 per tick), 0x86CE60
//     one emitter per action (loop 0x873430 once the emitter releases the master's +0x63 hold), 10
//     ticks after the last one the light ramp down (0x8734A0), then waits for the queues to empty.
//   Emitter (0x86CE90), one per action: bone follow + model bounds, sound 0x15F8DF0 at its first
//     tick, the screen flash with the colour script 0x15F8DC8 at tick 0x11 and 0x15F8DD8 at tick
//     0x1A; state 0 (0x86D060) spawns the four prim-model tasks, the actor system (0x86D1D0 with the
//     actor data 0x160D188, start tick 0, mode 0x2D, flag 0) and the shake task (0x86D100, empty),
//     state 1 releases the master at tick 0x1E, state 2 applies the damage of the action's current
//     target (0x506690) from tick 0x1E.
//   Screen flash (0x86CF60 = Confuse 0x85F930): a full-screen flat quad (+ draw mode) coloured by
//     its script.
//   Actor system (0x86D1D0 -> 0x86D230 / 0x871EE0): the engine's particle actors (emitters and
//     particles, sprite sequences 0x86F480 / prim models 0x86DDA0 drawn into the module arena)
//     placed by the actor data's key positions.
//   Prim-model tasks (0x1B0 bytes, spawned by 0x871F10; the prim-model player 0x701970 at node +0x94
//     with the draw callback 0x872700, parameter block = camera * placement, vertex blend buffer
//     0x26D2DC0), all placed at the caster's effect position 0xF1:
//     heart (0x871F60, layout 0x15F98F8): from tick 0, (0x20, -0x20, 0x20) in front of the caster
//       (its bone-0xF1 matrix turned by 0x400); when its model ends, the spark spawner 0x872170;
//     ring (0x872920, layout 0x1602658): the same placement from tick 0x10;
//     arrow (0x872B10, layout 0x1608024): from tick 0xF flies from in front of the caster to the
//       target's effect position on an arc (tables 0x160D3B4 / 0x160D3C8), leaving two static sparks
//       (0x873010, flipbook 0x15F9250) and two rising sparks (0x8730C0, flipbook 0x15F944C) a tick;
//     burst (0x8731D0, layout 0x160AF24): from tick 0x10 at (0x20, -0x20, -0xE0) in front of the
//       caster (placement kept in the node +0x30).
//   Spark spawner (0x872170): two bursts (4 + 16 + 6, then 2 + 8 + 3 sparks 0x8723E0: flipbooks
//     0x15F95E0 / 0x15F96BC, random direction and speed turned by the heart's placement; motion
//     velocity * 13 / 16 with gravity, position += velocity / 16) until their last frame.
//   Sparks are drawn as sprite sequences (0x872440) into the module arena.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26CDCC8..0x26D33E4 (pools, queues, file pointer 0x26CDD8C, packet cursor
// 0x26CDD90, arenas, particle pool 0x26D2DAC / cursor 0x26D10A0, actor pool 0x26D33D4 / cursor
// 0x26D2DB8, actor state 0x26D33E0, scratch stack pointer 0x26D33D0, vertex blend buffer 0x26D2DC0),
// the module scratch stack below the camera copy 0x2793E58.
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (hb_XXXXXX), most of them
// the text of the matching Magma Breath / Psycho Blast / Confuse / Dribble / Counter Laser-Eye port
// with this module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace heartbreak
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_031 = { "heartbreak", 31, 0x86C9C0, 0x8735D0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D2DB8, 0x26D33D4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26CDDB0, 0x0, 0x26D10A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D2990, 0x0, 0x26D2C30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D2DAA, 0x26D2DAC, 0x0, 0x26D33C8, 0x0, 0x26D33D0, 0x0, 0x26D33E0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x86D260, 0x86DCB0, 0x86DD40, 0x86DDA0, 0x0, 0x86E020, 0x86E230, 0x86E4B0, 0x86E6F0, 0x86E990, 0x86EBC0, 0x86EE80, 0x86F0E0, 0x86F3C0, 0x86F500, 0x86F540, 0x8707D0, 0x870820, 0x870850, 0x8708D0, 0x870FC0, 0x871010, 0x8710B0, 0x871170, 0x8719C0, 0x871DC0, 0x871E50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x86CE90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x86D410, 0x86D4F0, 0x86D650, 0x86D720, 0x86D840, 0x86DA50, 0x86F480, 0x86F5D0, 0x86FC30, 0x86FC80, 0x86FD10, 0x86FDE0, 0x870570, 0x870680, 0x870700, 0x870730, 0x870F50, 0x870F80, 0x871060, 0x871180, 0x8712D0, 0x871960, 0x871990, 0x871B70, 0x871CB0, 0x0, 0x86DCB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26CDD90;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x26D33E0;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x26D33D0;      // module scratch stack pointer
	static const uint32_t BLEND_BUF = 0x26D2DC0;       // vertex blend buffer of the prim callback
	static const uint32_t Q_EMITTER = 0x26D2C30, Q_FLASH = 0x26CDCC8, Q_ACTORS = 0x26D2990;
	static const uint32_t Q_PRIM = 0x26D2188, Q_PART = 0x26CDE90, Q_STAGE = 0x26D2C20;
	static const uint32_t ORIG_Master = 0x86CB50;
	static const uint32_t ORIG_Emitter = 0x86CE90;
	static const uint32_t ORIG_Flash = 0x86CF60;
	static const uint32_t ORIG_ActorSystem = 0x86D1D0;
	static const uint32_t ORIG_Shake = 0x86D100;
	static const uint32_t ORIG_Heart = 0x871F60, ORIG_Ring = 0x872920, ORIG_Arrow = 0x872B10, ORIG_Burst = 0x8731D0;
	static const uint32_t ORIG_Spawner = 0x872170, ORIG_SparkA = 0x8723E0, ORIG_SparkB = 0x873010, ORIG_SparkC = 0x8730C0;
	static const uint32_t PRIM_CB = 0x872700;          // the prim-model player's draw callback
	static const uint32_t SOUND_Heartbreak = 0x15F8DF0;
	static const uint32_t ACTOR_DATA = 0x160D188;      // the actor system's data (exe data)
	static const uint32_t FLASH_SCRIPT_1 = 0x15F8DC8, FLASH_SCRIPT_2 = 0x15F8DD8; // screen flash colour scripts
	static const uint32_t FLIGHT_T = 0x160D3B4;        // the arrow's flight parameter per step (0..0x1000)
	static const uint32_t ARC_HEIGHT = 0x160D3C8;      // the arrow's arc height per step
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }
	// RESIDUE model (see the prim-model tasks): the 27 stack words [P - 0x94, P - 0x28) as the
	// original leaves them (P = the prim-model tasks' executor esp), updated where their writers run
	static uint32_t g_shadow[27];
	static uint32_t g_sprite_ebp;   // 0x86F3C0's ebp at its 0x86F480 call (the sprite's d8)
	static inline void shadow_copy(int d, uint32_t src, int n)
	{
		for (int k = 0; k < n; k++)
			g_shadow[d + k] = U32(src, 4 * k);
	}
	static inline void shadow5(int d, uint32_t a, uint32_t b, uint32_t c, uint32_t e, uint32_t f)
	{
		g_shadow[d] = a; g_shadow[d + 1] = b; g_shadow[d + 2] = c; g_shadow[d + 3] = e; g_shadow[d + 4] = f;
	}
	// a matrix's rotation and pad (0x14 bytes) from the words d..d+4
	static inline void residue_to(uint32_t m, int d)
	{
		for (int k = 0; k < 5; k++)
			U32(m, 4 * k) = g_shadow[d + k];
	}

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t sub_45DEA0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DEA0)(a1); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t sub_45E160(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a1, a2, a3); }
		static inline uint32_t sub_45E220(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E220)(a1); }
		static inline uint32_t GTE_MVMVA_ColorV0() { return fn<uint32_t (__cdecl *)()>(0x4607D0)(); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); }
		static inline uint32_t sub_56BDE0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(a1, a2); }
		static inline uint32_t TransformCameraByShadowRotation(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(a1, a2, a3); }
		static inline uint32_t InitEffectSequenceFromData(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(a1, a2, a3, a4); }
		static inline uint32_t sub_8DDB90(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DDB90)(a1, a2, a3); }
		static inline uint32_t sub_8DE8F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x8DE8F0)(a1, a2, a3, a4); }
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

	// the screen overlay's full-screen flat quad (320 x 216, code 0x2A, colour `colour`) and its draw
	// mode (0x45C690 / 0x45BFC0) into the module arena, OT bucket base + 0x1C
	static void FlashDraw(uint32_t colour)
	{
		const uint32_t bucket = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		uint32_t pkt = MEM<uint32_t>(PACKET_CURSOR);
		U32(pkt, 4) = colour;
		U32(pkt, 0) = 0x5000000;
		U8(pkt, 7) = 0x2A;
		U32(pkt, 8) = 0;
		U32(pkt, 0xC) = 0x140;
		U32(pkt, 0x10) = 0xD80000;
		U32(pkt, 0x14) = 0xD80140;
		x::SSIGPU_InsertPrimAltViewport(bucket, pkt);
		pkt += 0x18;
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAutoDepth(bucket, pkt);
		MEM<uint32_t>(PACKET_CURSOR) = pkt + 0xC;
	}


	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl hb_86CB50(uint32_t a1);
	uint32_t __cdecl hb_86CCE0(uint32_t a1);
	uint32_t __cdecl hb_86CD30(void);
	uint32_t __cdecl hb_86CE10(uint32_t a1);
	uint32_t __cdecl hb_86CE90(uint32_t a1);
	uint32_t __cdecl hb_86CF60(uint32_t a1);
	uint32_t __cdecl hb_86D060(uint32_t a1);
	uint32_t __cdecl hb_86D100(uint32_t a1);
	uint32_t __cdecl hb_86D230(uint32_t a1);
	uint32_t __cdecl hb_86D260(uint32_t a1);
	uint32_t __cdecl hb_86D410(uint32_t a1);
	uint32_t __cdecl hb_86D4F0(void);
	uint32_t __cdecl hb_86D650(void);
	uint32_t __cdecl hb_86D840(void);
	uint32_t __cdecl hb_86DCB0(uint32_t a1);
	uint32_t __cdecl hb_86DD40(void);
	uint32_t __cdecl hb_86DDA0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_86E020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86E230(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86E990(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86EBC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86EE80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86F0E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_86F3C0(void);
	uint32_t __cdecl hb_86F480(uint32_t a1);
	uint32_t __cdecl hb_86F500(void);
	uint32_t __cdecl hb_86F540(uint32_t a1);
	uint32_t __cdecl hb_870570(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_870680(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_870700(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_870730(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl hb_8707D0(uint32_t a1);
	uint32_t __cdecl hb_870850(uint32_t a1);
	uint32_t __cdecl hb_8708D0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl hb_8710B0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_8712D0(uint32_t a1);
	uint32_t __cdecl hb_871DC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_871E50(void);
	uint32_t __cdecl hb_871EE0(uint32_t a1);
	uint32_t __cdecl hb_871F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl hb_871FC0(uint32_t a1);
	uint32_t __cdecl hb_872020(uint32_t a1);
	uint32_t __cdecl hb_8721D0(uint32_t a1);
	uint32_t __cdecl hb_872310(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl hb_8723E0(uint32_t a1);
	uint32_t __cdecl hb_872440(uint32_t a1);
	uint32_t __cdecl hb_8724B0(uint32_t a1);
	uint32_t __cdecl hb_8725B0(uint32_t a1);
	uint32_t __cdecl hb_872700(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl hb_872920(uint32_t a1);
	uint32_t __cdecl hb_872980(uint32_t a1);
	uint32_t __cdecl hb_872990(uint32_t a1);
	uint32_t __cdecl hb_8729F0(uint32_t a1);
	uint32_t __cdecl hb_872B10(uint32_t a1);
	uint32_t __cdecl hb_872B70(uint32_t a1);
	uint32_t __cdecl hb_872B80(uint32_t a1);
	uint32_t __cdecl hb_872CB0(uint32_t a1);
	uint32_t __cdecl hb_872F60(uint32_t a1, uint32_t a2);
	uint32_t __cdecl hb_873010(uint32_t a1);
	uint32_t __cdecl hb_873070(uint32_t a1);
	uint32_t __cdecl hb_8730C0(uint32_t a1);
	uint32_t __cdecl hb_873120(uint32_t a1);
	uint32_t __cdecl hb_873150(uint32_t a1);
	uint32_t __cdecl hb_8731D0(uint32_t a1);
	uint32_t __cdecl hb_873230(uint32_t a1);
	uint32_t __cdecl hb_873240(uint32_t a1);
	uint32_t __cdecl hb_873350(uint32_t a1);
	uint32_t __cdecl hb_8733C0(uint32_t a1);
	uint32_t __cdecl hb_8733E0(uint32_t a1);
	uint32_t __cdecl hb_873430(uint32_t a1);
	uint32_t __cdecl hb_873470(uint32_t a1);
	uint32_t __cdecl hb_873500(uint32_t a1);
	uint32_t __cdecl hb_873520(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag031_heartbreak_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace heartbreak
{
	// ====================================================================================
	// the library functions of this module whose code matches a Magma Breath / Psycho Blast /
	// Confuse / Dribble / Counter Laser-Eye port up to the module's addresses and callees (the port's
	// text with the module's addresses; the comments describe the source original), then the
	// module's own code
	// ====================================================================================
	// 0x86CB50 (copy of pb_893190 0x893190: MAG_023_sub_893190; Raldo Throw rt_8A33F0 with this module's table and six queues):
	// MASTER task - camera copy, packet arena by tick parity, bone follow, 11-state table
	// {0x893300, 0x893310 (a_73A0D0), 0x893320 carve, 0x8933A0 emitters, 0x89A5C0 .. 0x89A660, ret},
	// the six queues (live task count -> node +0x5E; the actor counters 0x27044C0 / 0x270420C are
	// cleared before the queues run)
	uint32_t __cdecl hb_86CB50(uint32_t a1)
	{
		g_mod = &MOD_031;
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x26CDDB0) = 0x2793E58;
		states[0] = 0x86CCC0;
		states[1] = 0x86CCD0;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x86CCE0;
		states[3] = 0x86CE60;
		states[4] = 0x873430;
		states[5] = 0x873470;
		states[6] = 0x873570;
		states[7] = 0x873580;
		states[8] = 0x873590;
		states[9] = 0x8735A0;
		states[10] = 0x8735C0; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26D2DA4);
			const uint32_t v2 = MEM<uint32_t>(0x26D219C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26CDDAC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26D2DA0);
			const uint32_t v2 = MEM<uint32_t>(0x26D2198);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26CDDAC) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x26D33C8) = 0;
		MEM<uint16_t>(0x26D2DAA) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_FLASH));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PRIM));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PART));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86CE10 (copy of mb_8250F0 0x8250F0: copy of cl_8739F0 0x8739F0: copy of r_8517B0: 0x8517B0 (MAG_039_sub_8517B0)): stage wobble state 1 - amplitude +0x1C up by 0x100 per tick to
	// 0x400 (then finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl hb_86CE10(uint32_t a1)
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

	// 0x86CF60 (copy of mb_825220 0x825220: copy of c_85F930 0x85F930: MAG_036_CONFUSE_RenderFullscreenOverlay): SCREEN OVERLAY task - colour script at
	// +0x30 (dwords: colour, or 0xFE000000 | t: wait until tick t then take the next entry,
	// 0xFF......: finished), current colour +0x34, script index +0x38; draws the full-screen quad
	// (FlashDraw)
	uint32_t __cdecl hb_86CF60(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF)
			U8(node, 0x26) |= 1;
		else
		{
			if ((hi >> 8) == 0xFE)
			{
				if (S16(node, 0x24) >= (int16_t)(hi & 0xFF))
				{
					const uint16_t next = (uint16_t)(idx + 1);
					U16(node, 0x38) = next;
					U32(node, 0x34) = U32(script, (int32_t)(int16_t)next * 4);
				}
			}
			else
			{
				U32(node, 0x34) = entry;
				U16(node, 0x38) = (uint16_t)(idx + 1);
			}
			// 30 fps layer: see mag031_heartbreak_held.inc
			FX_HELD(held_note_flash(node);)
			FlashDraw(U32(node, 0x34));
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86D100 (copy of mb_82A540 0x82A540: copy of c_85FA30 0x85FA30: MAG_036_sub_85FA30): CAMERA SHAKE task - state {0x85FA80 shake step, 0x85FAB0 ret}
	uint32_t __cdecl hb_86D100(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = 0x86D150;
		states[1] = 0x86D170; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86D230 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl hb_86D230(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			hb_86D260(a1);
			hb_86DCB0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x86D260 (copy of mb_825440 0x825440: copy of db_846AB0 0x846AB0: copy of cg_87E230 0x87E230: MAG_028_CURAGA_MainVisual_SetupRenderWorkspace; = Confuse c_85FBD0, copy of a_73F990 0x73F990; engine; Siren sub_73F990, all 6 actor modules): actor director set-up for node a1:
	// *G_258FB78 = state block node+0x34; copies node+0x29A/0x29C(mode)/0x29E into it, points its 4
	// tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once by 0x740210,
	// flag data+0), fills the target slot list (+4.., count = root +0x5A) from the cast context's
	// action record, sets start / per-target / bounds positions (0x73FC20, 0x73FFB0; modes 1/3/4
	// extra set-ups) and stores the caster-to-first-target distance in state+0.
	uint32_t __cdecl hb_86D260(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(0x26D33E0) = st;
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
			st = MEM<uint32_t>(0x26D33E0);
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
		hb_86D4F0();
		st = MEM<uint32_t>(0x26D33E0);
		if (U16(st, 0x24) == 4)
			callp(0x86D410, node);
		hb_86D840();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(0x26D33E0);
		if (U16(st, 0x24) == 1)
		{
			callp(0x86D650, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(0x26D33E0);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(0x26D33E0);
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
		st = MEM<uint32_t>(0x26D33E0);
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x86D410 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl hb_86D410(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x26D33E0);
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

	// 0x86D4F0 (copy of mb_8256D0 0x8256D0: copy of db_846D40 0x846D40: copy of cg_87E4C0 0x87E4C0: sub_87E4C0; = Confuse c_85FE60): the actor system's reference points from the caster (slot state +0x1E):
	// +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same on the ground (y 0),
	// +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C (16.16 each);
	// mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl hb_86D4F0(void)
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

	// 0x86D650 (copy of mb_825830 0x825830: copy of db_846EA0 0x846EA0: copy of cg_87E620 0x87E620: sub_87E620; = Confuse c_85FFC0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl hb_86D650(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x26D33E0);
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

	// 0x86D840 (copy of mb_825A20 0x825A20: copy of db_847090 0x847090: copy of cg_87E810 0x87E810: sub_87E810; = Confuse c_8601B0): per target of the actor system (slots state +4.., count +0x1C) the
	// target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its ground point),
	// +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride 0x10;
	// then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl hb_86D840(void)
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

	// 0x86DCB0 (copy of c_860620 0x860620: sub_860620, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl hb_86DCB0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x26D33E0, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			hb_871E50();
			hb_86F500();
			// 30 fps layer: see mag031_heartbreak_held.inc
			FX_HELD(held_note_system(sys);)
			hb_86F3C0();
			hb_86DD40();
			sys = U32(0x26D33E0, 0);
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
		U16(0x26D33C8, 0) = (uint16_t)(U16(0x26D33C8, 0) + c14);
		U16(0x26D2DAA, 0) = (uint16_t)(U16(0x26D2DAA, 0) + c16);
		return done;
	}

	// 0x86DD40 (copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl hb_86DD40(void)
	{
		uint32_t act = U32(U32(0x26D33E0, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x26D33E0, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					hb_86DDA0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x86DDA0 (copy of mb_826030 0x826030: copy of db_847750 0x847750: copy of cg_87ED70 0x87ED70: sub_87ED70; = Confuse c_860710): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl hb_86DDA0(uint32_t a1, uint32_t a2)
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

	// 0x86E020 (copy of mb_8262B0 0x8262B0: copy of db_8479D0 0x8479D0: copy of cg_87EFF0 0x87EFF0: sub_87EFF0; = Confuse c_860990, copy of b_72BA00 0x; 097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl hb_86E020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86E230 (copy of mb_8264C0 0x8264C0: copy of db_847BE0 0x847BE0: copy of cg_87F200 0x87F200: sub_87F200; = Confuse c_860BA0, copy of b_72BC10 0x; 097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl hb_86E230(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86E990 (copy of mb_826C20 0x826C20: copy of db_848340 0x848340: copy of cg_87F960 0x87F960: sub_87F960; = Confuse c_861300, copy of b_72C370 0x72C370; Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl hb_86E990(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86EBC0 (copy of mb_826E50 0x826E50: copy of db_848570 0x848570: copy of cg_87FB90 0x87FB90: sub_87FB90; = Confuse c_861530, copy of b_72C5A0 0x72C5A0; Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl hb_86EBC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86EE80 (copy of mb_827110 0x827110: copy of db_848830 0x848830: copy of cg_87FE50 0x87FE50: sub_87FE50; = Confuse c_8617F0, copy of b_72C860 0x; Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl hb_86EE80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86F0E0 (copy of mb_827370 0x827370: copy of db_848A90 0x848A90: copy of cg_8800B0 0x8800B0: sub_8800B0; = Confuse c_861A50, copy of b_72CAC0 0x72CAC0; Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl hb_86F0E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x86F3C0 (copy of mb_827650 0x827650: copy of db_848D70 0x848D70: copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl hb_86F3C0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x26D33E0), 0x2C);
		if (act == 0)
			return 0; // void
		uint32_t ebp = Q_ACTORS; // (RESIDUE: ebp as the executor 0x508420 set it, the actor queue)
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x26D33E0);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						g_sprite_ebp = ebp;
						hb_86F480(act);   // (the original also pushes the bone entry, unused)
					}
					else
					{
						ebp = 0; // (xor ebp, ebp; for n <= 0 too)
					}
					if (n != 1 && n > 0)
					{
						int32_t i = 0;
						uint32_t sub = act + 0x19E;   // edi
						do
						{
							S32(act, 0xC0) = S16(sub, -2);
							S32(act, 0xC4) = S16(sub, 0);
							S32(act, 0xC8) = S16(sub, 2);
							g_sprite_ebp = (uint32_t)i;
							hb_86F480(act);
							i++;
							ebp = (uint32_t)i;
							sub += 8;
						} while (i < S8(act, 0x1D8));
					}
				}
			}
			act = U32(act, 4);
		} while (act != 0);
		return 0; // void
	}

	// 0x86F480 (copy of mb_827710 0x827710: copy of db_848E30 0x848E30: copy of cg_880450 0x880450: sub_880450; = Confuse c_861DF0): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl hb_86F480(uint32_t a1)
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
		// (RESIDUE: 0x8DD260 saves esi (the header), ebp, ebx (actor + 0xAC) below its return 0x86F4DD
		// and its first argument: d7..d11)
		shadow5(7, hdr, g_sprite_ebp, act + 0xAC, 0x86F4DD, hdr);
		return 0; // void
	}

	// 0x86F500 (copy of mb_827790 0x827790: copy of db_848EB0 0x848EB0: copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl hb_86F500(void)
	{
		uint32_t act = U32(U32(0x26D33E0, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				hb_8707D0(act);
			else if (type == 1)
				hb_86F540(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x86F540 (copy of pb_8971B0 0x8971B0: copy of rt_8A6A10 0x8A6A10: copy of dw_8B3110 0x8B3110: copy of dm_8C9810 0x8C9810: copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl hb_86F540(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x26D33E0);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (hb_870680(act, bone) == 0)
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
			st = MEM<uint32_t>(0x26D33E0);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x870570 (copy of mb_828BA0 0x828BA0: copy of db_849F20 0x849F20: copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl hb_870570(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x26D33E0, 0);                  // actor state block
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

	// 0x870680 (copy of pb_8982F0 0x8982F0: copy of rt_8A7B50 0x8A7B50: copy of dw_8B4250 0x8B4250: copy of dm_8CA950 0x8CA950: copy of mb_828CB0 0x828CB0: copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl hb_870680(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				hb_870700(a1, a2);
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
			hb_870700(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			hb_870700(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x870700 (copy of pb_898370 0x898370: copy of rt_8A7BD0 0x8A7BD0: copy of dw_8B42D0 0x8B42D0: copy of dm_8CA9D0 0x8CA9D0: copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl hb_870700(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return hb_870730(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x870730 (copy of pb_8983A0 0x8983A0: copy of rt_8A7C00 0x8A7C00: copy of dw_8B4300 0x8B4300: copy of dm_8CAA00 0x8CAA00: copy of d_8BBC10 0x8BBC10: sub_8BBC10): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 10 records
	uint32_t __cdecl hb_870730(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x26D2DAC);
		int32_t idx = MEM<int16_t>(0x26D10A0);
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
			if (idx >= 9)
				idx = 0;
			tries++;
		} while (tries < 0xA);
		idx++;
		if (idx < 9)
			MEM<uint16_t>(0x26D10A0) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x26D10A0) = 0;
		return slot;
	}

	// 0x8707D0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl hb_8707D0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x871180, a1);
		else if (mode == 4)
			callp(0x8712D0, a1);
		hb_870850(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x870850 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl hb_870850(uint32_t a1)
	{
		uint32_t sys = U32(0x26D33E0, 0);
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
				hb_8708D0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			hb_8708D0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8708D0 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl hb_8708D0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x26D33D0, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x26D33D0, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = hb_8710B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x26D33E0, 0);
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
		hb_871DC0(node, desc);
		U32(0x26D33D0, 0) = U32(0x26D33D0, 0) + 0x50;
		return 0; // void
	}

	// 0x8710B0 (copy of db_84AA60 0x84AA60: copy of c_863A20 0x863A20: sub_863A20): the engine's 0x7387B0 with the module's actor pool: allocates an actor
	// record of 0x2A0 bytes from the pool [0x26A9BB4] starting at the cursor 0x26A9BA8 (wraps at 49:
	// record 49 of the 50 is never used), cleared, owner a1, definition index a2
	uint32_t __cdecl hb_8710B0(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x26D33D4);
		int32_t i = MEM<int16_t>(0x26D2DB8);
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
			MEM<uint16_t>(0x26D2DB8) = 0;
		else
			MEM<uint16_t>(0x26D2DB8) = (uint16_t)i;
		return slot;
	}

	// 0x8712D0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl hb_8712D0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x26D33E0, 0);            // ecx
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
				uint32_t g = U32(0x26D33E0, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x26D33E0, 0);
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

	// 0x871DC0 (copy of pb_899A30 0x899A30: copy of rt_8A9290 0x8A9290: copy of dw_8B5990 0x8B5990: copy of dm_8CC090 0x8CC090: copy of mb_82A3F0 0x82A3F0: copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl hb_871DC0(uint32_t a1, uint32_t a2)
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
			hb_870730(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			hb_870730(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			hb_870730(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			hb_870730(obj, id, variant);
		}
		return 0; // void
	}

	// 0x871E50 (copy of pb_899AC0 0x899AC0: copy of rt_8A9320 0x8A9320: copy of dw_8B5A20 0x8B5A20: copy of dm_8CC120 0x8CC120: copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via hb_870730(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl hb_871E50(void)
	{
		uint32_t dir = MEM<uint32_t>(0x26D33E0);  // eax (re-read only after the calls)
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
					hb_870730(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x26D33E0);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				hb_870730(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x26D33E0);
			}
		}
		return 0; // void
	}

	// 0x871EE0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl hb_871EE0(uint32_t a1)
	{
		if (hb_86DCB0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x872440 (copy of db_84CB00 0x84CB00: copy of cl_878F80 0x878F80: sub_878F80; = Cure's MAG_001_CURE_DrawSprite 0x8D6F20): draws the node's flipbook
	// +0x4C frame +0x50 at +0x1C (camera-facing, angle +0x54) as a sprite sequence into the module
	// arena, unless hidden (+0x26 bit 2)
	uint32_t __cdecl hb_872440(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_spark(a1);)
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

	// 0x872920 (copy of pb_893950 0x893950: copy of s_73F250 0x73F250: module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl hb_872920(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x872980, 0x872990, 0x8729F0, 0x872B00 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x872B10 (copy of pb_893950 0x893950: copy of s_73F250 0x73F250: module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl hb_872B10(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x872B70, 0x872B80, 0x872CB0, 0x8731C0 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x872F60 (copy of cl_878E70 0x878E70: sub_878E70): moves node a1's position +0x1C..+0x20 by (0, 0, rand % a2) turned by a
	// random yaw and pitch (rand & 0xFFF each; a2 = 0 counts as 1)
	uint32_t __cdecl hb_872F60(uint32_t a1, uint32_t a2)
	{
		alignas(4) uint8_t frame[0x30] = {};
		const uint32_t f = P(frame);
		uint32_t range = a2;
		if ((uint16_t)range == 0)
			range = 1;
		const uint32_t yaw = x::CrtRand() & 0xFFF;
		const uint32_t pitch = x::CrtRand() & 0xFFF;
		x::MAG_022_sub_8DD770(f + 0x10);
		x::MAG_022_sub_8DD8A0(f + 0x10, (uint32_t)(int32_t)(int16_t)yaw);
		x::sub_8DD7E0(f + 0x10, (uint32_t)(int32_t)(int16_t)pitch);
		S16(f, 0) = 0;
		S16(f, 2) = 0;
		const int32_t r = (int32_t)x::CrtRand();
		S16(f, 4) = (int16_t)(r % (int32_t)(int16_t)range);
		x::matrixMultiplyVector(f + 0x10, f, f + 8);
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + U16(f, 8));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + U16(f, 0xA));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + U16(f, 0xC));
		return 0; // void
	}

	// 0x873010 (copy of cl_878F20 0x878F20: copy of t_767290: 0x767290 (module 090 sub_767290)): ring particle task (states 0x767410/0x767430) drawn by
	// t_7672F0; ends when finished and no child alive.
	uint32_t __cdecl hb_873010(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x873070;
		states[1] = 0x873090;
		states[2] = 0x8730B0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		hb_872440(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8730C0 (copy of cl_878F20 0x878F20: copy of t_767290: 0x767290 (module 090 sub_767290)): ring particle task (states 0x767410/0x767430) drawn by
	// t_7672F0; ends when finished and no child alive.
	uint32_t __cdecl hb_8730C0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x873120;
		states[1] = 0x873150;
		states[2] = 0x8731B0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		hb_872440(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8731D0 (copy of pb_893950 0x893950: copy of s_73F250 0x73F250: module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl hb_8731D0(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x873230, 0x873240, 0x873350, 0x8733B0 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x8733C0 (copy of c_864880 0x864880: MAG_036_sub_864880): emitter state 1 - at tick 30 releases the master (root +0x63 =
	// 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl hb_8733C0(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x1E)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x8733E0 (copy of c_8648A0 0x8648A0: MAG_036_sub_8648A0): emitter state 2 - from tick 30: damage of its target,
	// finished, next state
	uint32_t __cdecl hb_8733E0(uint32_t a1)
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

	// 0x873430 (copy of cg_882F50 0x882F50: MAG_028_sub_882F50; = Drain r_856830): master state - action loop: while the master is not held
	// (+0x63) and actions remain (+0x2A < +0x58), the next action (+0x2A / +0x2E up, state back
	// to the emitter spawn); after the last one waits 10 ticks (+0x60) and goes on
	uint32_t __cdecl hb_873430(uint32_t a1)
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

	// 0x873470 (copy of mb_82A670 0x82A670: copy of cl_879320 0x879320: copy of r_856870: 0x856870 (MAG_039_sub_856870)): master state - counts +0x60 down; at 0 starts the stage wobble
	// out (0x8568A0, 0x40 bytes, stage queue), next state
	uint32_t __cdecl hb_873470(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8734A0, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x873500 (copy of mb_82A700 0x82A700: copy of cl_8793B0 0x8793B0: copy of r_856900: 0x856900 (MAG_039_sub_856900)): stage wobble out state 0 - amplitude +0x1C = 0x400, next state
	uint32_t __cdecl hb_873500(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x873520 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl hb_873520(uint32_t a1)
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

	// 0x86CCE0 (MAG_031_sub_86CCE0; = Counter Laser-Eye 0x8738C0 with a 0x438 particle pool): master
	// state 2 - once (+0x28 clear) reserves the actor pools after the file arena (0x438 + 0x8340 bytes
	// = 10 particles of 0x6C and 50 actors of 0x2A0, cleared by 0x86CD30) and starts the stage light
	// ramp up (task 0x86CD70), next state
	uint32_t __cdecl hb_86CCE0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x26D33DC);
		MEM<uint32_t>(0x26D2DAC) = p;
		p += 0x438;
		MEM<uint32_t>(0x26D33D4) = p;
		p += 0x8340;
		MEM<uint32_t>(0x26D33DC) = p;
		hb_86CD30();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x86CD70, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x86CD30 (MAG_031_sub_86CD30; = Counter Laser-Eye 0x873910 with a 0x438 particle pool): clears
	// the particle pool 0x26D2DAC (0x438) and the actor pool 0x26D33D4 (0x8340), zeroes the particle
	// cursor 0x26D10A0, the actor cursor 0x26D2DB8 and the words 0x26D2DA8 / 0x26CDD94
	uint32_t __cdecl hb_86CD30(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26D2DAC), 0x438);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26D33D4), 0x8340);
		MEM<uint16_t>(0x26D10A0) = 0;
		MEM<uint16_t>(0x26D2DB8) = 0;
		MEM<uint16_t>(0x26D2DA8) = 0;
		MEM<uint16_t>(0x26CDD94) = 0;
		return 0; // void
	}

	// 0x86CE90 (MAG_031_sub_86CE90; Curaga's emitter 0x87DF70 with two screen flashes): EMITTER task
	// (one per action) - bone follow + model bounds (CURE_Emitter helpers), state table {0x86D060,
	// 0x8733C0, 0x8733E0, ret}, sound 0x15F8DF0 at its first tick, the screen flash (0x86CF60) with
	// the colour script 0x15F8DC8 at tick 0x11 and with 0x15F8DD8 at tick 0x1A
	uint32_t __cdecl hb_86CE90(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x86D060;
		states[1] = 0x8733C0;
		states[2] = 0x8733E0;
		states[3] = 0x873420; // nullsub (ret)
		// (RESIDUE: the words the two calls leave at the prim-model tasks' depth)
		const uint32_t ent = ENT(U8(node, 0x2D));
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		if ((U8(ent, 0) & 2) != 0)
		{
			static const uint32_t w[9] = { 0x43300000, 0x56C843, 0, 0, 0x50225A, 0x34, 0x1D97708, 0, 0 };
			for (int k = 0; k < 9; k++) g_shadow[k] = w[k];
			g_shadow[2] = g_shadow[8] = fa + 0x30;
			g_shadow[3] = g_shadow[7] = fa + 0x20;
		}
		const uint32_t fb = MEM<uint32_t>(0x1D999C4);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		const uint32_t skel = U32(U32(ent, 0x64), 0);
		const uint32_t bones = U8(skel, 0);
		if (bones > 1)
		{
			const uint32_t last = skel + 0x20 + 0x30 * (bones - 1); // the last bone matrix of its loop
			g_shadow[9] = 0x45DE3D;
			g_shadow[10] = U32(last, 0x10);
			g_shadow[11] = (uint32_t)(int32_t)S16(last, 0x10);
			g_shadow[22] = fb + 0x28;
			g_shadow[23] = fb + 8;
		}
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(SOUND_Heartbreak, 0, 0x80);
		if (U16(node, 0x24) == 0x11)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_FLASH, ORIG_Flash, 0x3C, node);
			U32(t, 0x30) = FLASH_SCRIPT_1;
		}
		if (U16(node, 0x24) == 0x1A)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_FLASH, ORIG_Flash, 0x3C, node);
			U32(t, 0x30) = FLASH_SCRIPT_2;
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86D060 (MAG_031_sub_86D060): emitter state 0 - the four prim-model tasks (0x871F10: heart
	// 0x871F60 / layout 0x15F98F8, ring 0x872920 / 0x1602658, arrow 0x872B10 / 0x1608024, burst
	// 0x8731D0 / 0x160AF24), the actor system (0x86D1D0 through the engine spawner 0x86D180: actor
	// data 0x160D188, start tick 0, mode 0x2D, flag 0) and the (empty) shake task 0x86D100, next state
	uint32_t __cdecl hb_86D060(uint32_t a1)
	{
		hb_871F10(a1, ORIG_Heart, 0x15F98F8, 0x100, 0, 0);
		hb_871F10(a1, ORIG_Ring, 0x1602658, 0x2C, 1, 0);
		hb_871F10(a1, ORIG_Arrow, 0x1608024, 0x7C, 2, 0);
		hb_871F10(a1, ORIG_Burst, 0x160AF24, 0x6C, 3, 0);
		a_73C100(a1, ORIG_ActorSystem, ACTOR_DATA, 0, 0x2D, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_Shake, 0x80, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// ====================================================================================
	// the prim-model tasks (0x1B0 bytes, queue 0x26D2188): the prim-model player (0x701970) at node
	// +0x94 with the draw callback 0x872700; the parameter block is the placement matrix (camera *
	// placement, +0x00..+0x1F) and the vertex blend buffer +0x48 = 0x26D2DC0
	// ====================================================================================

	// RESIDUE (UNINIT at the 0x8DE8F0 calls 0x872066, 0x872A30, 0x872BEC, 0x872210, 0x8725F0): the
	// heart / ring / arrow / spawner states build a stack matrix with 0x8DE8F0, which writes only
	// its translation (+0x14..+0x1F: the bone's camera-space position), then turn it by 0x400
	// (0x8DD7E0 multiplies it); its rotation and pad (+0x00..+0x13) are never written: the original
	// reads what earlier code left in those stack words. The game's executor 0x508420 calls every
	// task of a queue at the same esp X and the master runs its six queues 4 bytes apart (emitter X,
	// flash X - 4, actors X - 8, prim-model tasks X - 0xC, particles X - 0x10), so the words are
	// those the previous code left at that depth. g_shadow models the 27 words [P - 0x94, P - 0x28)
	// (P = the prim-model tasks' X): the heart reads d0..d4 (0x872020 block [esp+0x14]), the ring
	// d7..d11 (0x8729F0 block [esp+0x14]), the arrow d22..d26 (0x872B80 matrix [esp+0x18]), the
	// spawner d22..d26 (0x8721D0 / 0x8725B0 matrix [esp+0x14], particle queue P - 4). Writers, in the
	// original's order (each function below updates the words its frame leaves there):
	//   - emitter (0x86CE90): MAG_001_CURE_Emitter_UpdatePos 0x8DC610's second GetEffectSpawnPosition
	//     0x502170 (bone 0xF1 of the entity at node +0x2D, when its flags +0 bit 1 is set) leaves
	//     d0..d8 = 0x43300000 (MVMVA 0x460860's rounding double, high word), 0x56C843 (return of
	//     0x56C820's GTE_ReadFLAG call), FA + 0x30, FA + 0x20 (0x56C820's arguments), 0x50225A
	//     (return of its Field_Free call), 0x34, 0x1D97708, FA + 0x20, FA + 0x30 (FA = its
	//     Field_Alloc(0x34) block = the Field_Alloc top before the call); then
	//     MAG_001_CURE_Emitter_ComputeModelBounds 0x8DC870 (same entity, skeleton of more than one
	//     bone) leaves d9..d11 = 0x45DE3D (return in GTE_SetRotMatrix), the last bone matrix's
	//     +0x10 dword and its m22 sign-extended (GTE_WriteControlReg's argument slots), d22 / d23 =
	//     FB + 0x28 / FB + 8 (the GTE_ReadFLAG / GTE_ReadMAC123 arguments; FB = its Field_Alloc(0x2C)
	//     block);
	//   - flash (0x86CF60): its GPU calls stay above the words (nothing);
	//   - actor system, each actor sprite (0x86F480 -> 0x8DD260): d7..d11 = the header, the caller's
	//     ebp (0x86F3C0: the actor queue pointer, or the copy index / count of its multi-copy loop),
	//     actor + 0xAC, return 0x86F4DD, the header;
	//   - heart state 0 / 1, ring state 1 / 2, arrow state 1 / 2, burst states 1 / 2, spawner states:
	//     their own frames (see each).
	// Not modelled (they are overwritten before any read in a single action's timeline; with several
	// actions at once an earlier action's words may differ, by design): the actor model draws, the
	// sparks' sequence draws (0x571C80 internals), the spawner's spark spawns, the master's
	// 0x8DC740 and whatever code outside the effect left there before the first tick (0 here).
	// (g_shadow / the shadow helpers: with the module globals)

	// 0x871F10 (MAG_031_sub_871F10): spawns a prim-model task `fn` (a2) from the emitter a1: +0x74
	// layout data a3, +0x78 layout argument (short a4), +0x80 = a5, +0x82 = a6; returns the node
	uint32_t __cdecl hb_871F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x1B0, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// the caster's entity (slot = byte 0 of the cast context +0x0C)
	static inline uint32_t caster_entity(uint32_t node)
	{
		return ENT(U8(U32(node, 0xC), 0));
	}

	// the placement shared by the heart, the ring and the spawner: the caster's effect position 0xF1
	// (-> node +0x1C) moved by (0x20, -0x20, 0x20) through the caster's bone-0xF1 matrix turned by
	// 0x400 (8DE8F0 / 8DD7E0); `m` (0x20 bytes) = that matrix with the new position as translation;
	// its rotation starts as the stack words d.. (RESIDUE)
	static uint16_t g_front_vec[3]; // place_front's turned offset (its stack vector: the ring's d5 / d6)
	static void place_front(uint32_t node, uint32_t m, int d)
	{
		residue_to(m, d); // rotation + pad
		alignas(4) uint8_t v[8] = {};
		const uint32_t vec = P(v);
		const uint32_t ent = caster_entity(node);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		gx::sub_8DE8F0(ent, 0xF1, 0, m);
		x::sub_8DD7E0(m, 0x400);
		U16(vec, 0) = 0x20;
		U16(vec, 4) = 0x20;
		U16(vec, 2) = 0xFFE0;
		x::matrixMultiplyVector(m, vec, vec);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + U16(vec, 0));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(vec, 2));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + U16(vec, 4));
		g_front_vec[0] = U16(vec, 0);
		g_front_vec[1] = U16(vec, 2);
		g_front_vec[2] = U16(vec, 4);
	}

	// 0x871FC0 (MAG_031_sub_871FC0): heart state 0 - from frame 0 on (+0x24 >= 0): decodes the layout
	// (0x7016B0: data +0x74 -> node +0x94, argument +0x78), position = the caster's effect position
	// 0xF1, next state
	uint32_t __cdecl hb_871FC0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0)
			return 0; // void
		const uint32_t ent = caster_entity(node);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		// (RESIDUE: the arguments its calls leave in d22..d26)
		shadow5(22, ent, 0xF1, 0, node + 0x1C, U32(node, 0x74));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x872020 (MAG_031_sub_872020): heart state 1 - placement (place_front), parameter block =
	// camera 0x1D97778 * placement, plays the heart; when its model has ended: the spark spawner
	// (0x872170, particle queue) with the placement matrix at +0x58, finished, next state
	uint32_t __cdecl hb_872020(uint32_t a1)
	{
		const uint32_t node = a1;
		// stack frame: the parameter block (0x5C; the callback reads +0x00..+0x1F and +0x48 only)
		alignas(4) uint8_t fr[0x5C] = {};
		alignas(4) uint8_t pl[0x20] = {};
		const uint32_t L = P(fr), place = P(pl);
		place_front(node, L, 0);
		S32(L, 0x14) = S16(node, 0x1C);
		S32(L, 0x18) = S16(node, 0x1E);
		S32(L, 0x1C) = S16(node, 0x20);
		memcpy(pl, fr, 0x20); // rep movsd: the placement before the camera
		x::ComposeAffineTransform(0x1D97778, L, L);
		U32(L, 0x48) = BLEND_BUF;
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_play(node, place);)
		if (prim_play(node + 0x94, PRIM_CB, L, 0) == 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_Spawner, 0x80, node);
			U8(node, 0x26) |= 1;
			const uint8_t st = U8(node, 0x29);
			memcpy((void *)(t + 0x58), pl, 0x20);
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		// (RESIDUE: its frame's block (d0..d7, blend d18) and placement copy (d23..d26))
		shadow_copy(0, L, 8);
		g_shadow[18] = BLEND_BUF;
		shadow_copy(23, place, 4);
		return 0; // void
	}

	// ====================================================================================
	// the spark spawner (0x872170 = the engine's 0x73A380 on {0x8721D0, 0x8725B0, ret}) and its
	// sparks (0x8723E0)
	// ====================================================================================

	// 0x872310 (sub_872310): spawns a spark (0x8723E0, particle queue) of the spawner a1: position =
	// the spawner's +0x1C, flipbook a2, last frame a3, a random direction (rand & 0xFFF) in the xy
	// plane at speed a4 plus z = +-speed / 2 (rand & 1), turned by the spawner's placement +0x58
	uint32_t __cdecl hb_872310(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_SparkA, 0x80, a1);
		const uint32_t w20 = U32(a1, 0x20);
		U32(t, 0x1C) = U32(a1, 0x1C);
		U32(t, 0x20) = w20;
		U32(t, 0x4C) = a2;
		U16(t, 0x52) = (uint16_t)a3;
		const uint32_t r = x::CrtRand();
		const int32_t speed = (int16_t)a4;
		const int32_t ang = (int16_t)(uint16_t)(r & 0xFFF);
		const uint32_t vel = t + 0x78;
		int32_t p = mul32((int32_t)x::computeCosine((uint32_t)ang), speed);
		U16(vel, 0) = (uint16_t)(add32(p, (p >> 31) & 0xFFF) >> 12);
		p = mul32((int32_t)x::computeSin((uint32_t)ang), speed);
		U16(t, 0x7A) = (uint16_t)(add32(p, (p >> 31) & 0xFFF) >> 12);
		U16(t, 0x7C) = (uint16_t)(speed / 2);
		if ((x::CrtRand() & 1) != 0)
			U16(t, 0x7C) = (uint16_t)(0 - U16(t, 0x7C));
		x::matrixMultiplyVector(a1 + 0x58, vel, vel);
		return 0; // void
	}

	// the spawner's burst: n1 sparks (flipbook 0x15F95E0, 7 frames, speed rand % 0xC00 + 0x400), n2
	// (0x15F96BC, 8 frames, rand % 0x1800 + 0x800), n3 (0x15F95E0, 7 frames, rand % 0x1000 + 0x800)
	static void spawn_burst(uint32_t node, int n1, int n2, int n3)
	{
		for (int i = n1; i != 0; i--)
		{
			const int32_t r = (int32_t)x::CrtRand();
			hb_872310(node, 0x15F95E0, 7, (uint32_t)(r % 0xC00 + 0x400));
		}
		for (int i = n2; i != 0; i--)
		{
			const int32_t r = (int32_t)x::CrtRand();
			hb_872310(node, 0x15F96BC, 8, (uint32_t)(r % 0x1800 + 0x800));
		}
		for (int i = n3; i != 0; i--)
		{
			const int32_t r = (int32_t)x::CrtRand();
			hb_872310(node, 0x15F95E0, 7, (uint32_t)(r % 0x1000 + 0x800));
		}
	}

	// 0x8721D0 (sub_8721D0): spawner state 0 - position = place_front (its matrix unused), sparks
	// 4 + 16 + 6, next state
	uint32_t __cdecl hb_8721D0(uint32_t a1)
	{
		alignas(4) uint8_t m[0x20] = {};
		place_front(a1, P(m), 22);
		shadow_copy(22, P(m), 5); // (RESIDUE: its matrix's rotation and pad)
		spawn_burst(a1, 4, 16, 6);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8725B0 (sub_8725B0): spawner state 1 - position = place_front, sparks 2 + 8 + 3, finished,
	// next state
	uint32_t __cdecl hb_8725B0(uint32_t a1)
	{
		alignas(4) uint8_t m[0x20] = {};
		place_front(a1, P(m), 22);
		shadow_copy(22, P(m), 5); // (RESIDUE: its matrix's rotation and pad)
		spawn_burst(a1, 2, 8, 3);
		const uint8_t st = U8(a1, 0x29);
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8723E0 (sub_8723E0): SPARK task - state table {0x8724B0 motion, ret}, then its sprite draw
	// (0x872440)
	uint32_t __cdecl hb_8723E0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = 0x8724B0;
		states[1] = 0x8725A0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		hb_872440(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// v - v * 0x300 / 0x1000 (imul, cdq / and 0xFFF / add / sar 12: toward zero), 16-bit
	static inline uint16_t damp(uint16_t v)
	{
		const int32_t p = (int32_t)(int16_t)v * 0x300;
		return (uint16_t)(v - (uint16_t)(add32(p, (p >> 31) & 0xFFF) >> 12));
	}
	// x / 16 toward zero (cdq; and edx, 0xF; add; sar 4)
	static inline uint16_t div16(uint16_t v)
	{
		const int32_t s = (int16_t)v;
		return (uint16_t)(add32(s, (s >> 31) & 0xF) >> 4);
	}

	// 0x8724B0 (sub_8724B0): spark motion - velocity +0x78 (gravity +0x80 on y first) minus 3/16 of
	// itself, position += velocity / 16, next flipbook frame (0x872570); past its last frame:
	// finished (hidden), next state
	uint32_t __cdecl hb_8724B0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t vx = damp(U16(node, 0x78));
		U16(node, 0x78) = vx;
		const uint16_t vy = damp((uint16_t)(U16(node, 0x7A) + 0x80));
		U16(node, 0x7A) = vy;
		const uint16_t vz = damp(U16(node, 0x7C));
		U16(node, 0x7C) = vz;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + div16(vx));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + div16(vy));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + div16(vz));
		if (a_743C20(node) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// the ring (0x872920 = the engine's 0x73F250 on {0x872980, 0x872990, 0x8729F0, ret})
	// ====================================================================================

	// 0x872980 (sub_872980): ring state 0 - waits for frame 0x10, next state
	uint32_t __cdecl hb_872980(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x10)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1; // void (eax = node)
	}

	// 0x872990 (sub_872990): ring state 1 - decodes the layout, position = the caster's effect
	// position 0xF1, next state
	uint32_t __cdecl hb_872990(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = caster_entity(node);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		// (RESIDUE: the arguments its calls leave in d22..d26)
		shadow5(22, 0xF1, 0, node + 0x1C, U32(node, 0x74), node + 0x94);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8729F0 (sub_8729F0): ring state 2 - placement (place_front), block = camera * placement,
	// plays the ring; when its model has ended: finished, next state
	uint32_t __cdecl hb_8729F0(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		place_front(node, L, 7);
		S32(L, 0x14) = S16(node, 0x1C);
		S32(L, 0x18) = S16(node, 0x1E);
		S32(L, 0x1C) = S16(node, 0x20);
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_play(node, L);)
		x::ComposeAffineTransform(0x1D97778, L, L);
		U32(L, 0x48) = BLEND_BUF;
		if (prim_play(node + 0x94, PRIM_CB, L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		// (RESIDUE: its frame's vector (d5, d6 low half), block (d7..d14) and blend word (d25))
		g_shadow[5] = g_front_vec[0] | (uint32_t)g_front_vec[1] << 16;
		g_shadow[6] = (g_shadow[6] & 0xFFFF0000) | g_front_vec[2];
		shadow_copy(7, L, 8);
		g_shadow[25] = BLEND_BUF;
		return 0; // void
	}

	// ====================================================================================
	// the arrow (0x872B10 = the engine's 0x73F250 on {0x872B70, 0x872B80, 0x872CB0, ret}): flies on an
	// arc from in front of the caster to the target, leaving sparks
	// ====================================================================================

	// 0x872B70 (sub_872B70): arrow state 0 - waits for frame 0xF, next state
	uint32_t __cdecl hb_872B70(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xF)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1; // void (eax = node)
	}

	// 0x872B80 (sub_872B80): arrow state 1 - decodes the layout; start +0x19C = the caster's effect
	// position 0xF1 moved by (0x20, -0x20, 0x20) through its bone matrix turned by 0x400, end +0x1A4 =
	// the target's (node +0x2D) effect position 0xF1; position +0x1C = the start, next state
	uint32_t __cdecl hb_872B80(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t m[0x20] = {};
		alignas(4) uint8_t v[8] = {};
		const uint32_t M = P(m), vec = P(v);
		residue_to(M, 22); // (RESIDUE: the rotation and pad 0x8DE8F0 do not write)
		const uint32_t ent = caster_entity(node);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t tgt = ENT(U8(node, 0x2D));
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x19C);
		gx::sub_8DE8F0(ent, 0xF1, 0, M);
		x::sub_8DD7E0(M, 0x400);
		U16(vec, 0) = 0x20;
		U16(vec, 4) = 0x20;
		U16(vec, 2) = 0xFFE0;
		x::matrixMultiplyVector(M, vec, vec);
		U16(node, 0x19C) = (uint16_t)(U16(node, 0x19C) + U16(vec, 0));
		U16(node, 0x19E) = (uint16_t)(U16(node, 0x19E) + U16(vec, 2));
		U16(node, 0x1A0) = (uint16_t)(U16(node, 0x1A0) + U16(vec, 4));
		// (the start is also stored in the matrix's translation words, never read)
		S32(M, 0x14) = S16(node, 0x19C);
		S32(M, 0x18) = S16(node, 0x19E);
		S32(M, 0x1C) = S16(node, 0x1A0);
		x::GetEffectSpawnPosition(tgt, 0xF1, 0, node + 0x1A4);
		const uint32_t w20 = U32(node, 0x1A0);
		U32(node, 0x1C) = U32(node, 0x19C);
		U32(node, 0x20) = w20;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		shadow_copy(22, M, 5); // (RESIDUE: its matrix's rotation and pad, d22..d26)
		return 0; // void
	}

	// the arrow's position for the flight step +0x1AE = k: start + (end - start) * t / 0x1000 (t =
	// table 0x160D3B4[k]: imul, toward zero) minus the arc height 0x160D3C8[k] on y (16-bit words)
	static inline void arrow_pos(uint32_t node, int32_t k, int32_t t)
	{
		auto step = [&](int32_t o_end, int32_t o_start) -> uint16_t
		{
			const uint16_t s = U16(node, o_start);
			const int32_t p = mul32((int32_t)S16(node, o_end) - (int32_t)(int16_t)s, t);
			return (uint16_t)((add32(p, (p >> 31) & 0xFFF) >> 12) + s);
		};
		U16(node, 0x1C) = step(0x1A4, 0x19C);
		U16(node, 0x20) = step(0x1A8, 0x1A0);
		const uint16_t sy = U16(node, 0x19E);
		const int32_t p = mul32((int32_t)S16(node, 0x1A6) - (int32_t)(int16_t)sy, t);
		const uint16_t q = (uint16_t)(add32(p, (p >> 31) & 0xFFF) >> 12);
		U16(node, 0x1E) = (uint16_t)((uint16_t)(q - MEM<uint16_t>(ARC_HEIGHT + (uint32_t)k * 2)) + sy);
	}

	// the arrow's placement: its facing (identity turned by the caster's +0x0E, 8DD8A0) at its
	// position +0x1C
	static inline void arrow_matrix(uint32_t node, uint32_t ent, uint32_t m)
	{
		x::MAG_022_sub_8DD770(m);
		S32(m, 0x14) = S16(node, 0x1C);
		S32(m, 0x18) = S16(node, 0x1E);
		S32(m, 0x1C) = S16(node, 0x20);
		x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ent, 0xE));
	}

	// 0x872CB0 (sub_872CB0): arrow state 2 - keeps its previous position (+0x194), flight parameter
	// +0x1AC = the table 0x160D3B4 at step +0x1AE (0x1000 = arrived: finished, next state), position
	// on the arc (arrow_pos), step + 1; unless finished plays the arrow with the block camera *
	// placement (arrow_matrix); then two static sparks (0x873010, y + rand & 0xFF - 0x80) at the new
	// position and half way from the previous one, and two rising sparks (0x8730C0) there, pushed by
	// up to 0x80 in a random direction (0x872F60)
	uint32_t __cdecl hb_872CB0(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		const int32_t k = S16(node, 0x1AE);
		const uint32_t w20 = U32(node, 0x20);
		U32(node, 0x194) = U32(node, 0x1C);
		U32(node, 0x198) = w20;
		const uint16_t t = MEM<uint16_t>(FLIGHT_T + (uint32_t)k * 2);
		U16(node, 0x1AC) = t;
		if ((int16_t)t >= 0x1000)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		const uint32_t ent = caster_entity(node);
		arrow_pos(node, k, (int16_t)t);
		U16(node, 0x1AE) = (uint16_t)(U16(node, 0x1AE) + 1);
		arrow_matrix(node, ent, L);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U32(L, 0x48) = BLEND_BUF;
		if ((U8(node, 0x26) & 1) == 0)
		{
			// 30 fps layer: see mag031_heartbreak_held.inc
			FX_HELD(held_note_play(node, 0);)
			prim_play(node + 0x94, PRIM_CB, L, 0);
		}
		uint32_t p = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_SparkB, 0x80, node);
		uint32_t w = U32(node, 0x20);
		U32(p, 0x1C) = U32(node, 0x1C);
		U32(p, 0x20) = w;
		uint32_t r = x::CrtRand();
		U16(p, 0x1E) = (uint16_t)(U16(p, 0x1E) + (uint16_t)((r & 0xFF) - 0x80));
		p = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_SparkB, 0x80, node);
		U16(p, 0x1C) = (uint16_t)(((int32_t)S16(node, 0x194) + S16(node, 0x1C)) / 2);
		U16(p, 0x1E) = (uint16_t)(((int32_t)S16(node, 0x196) + S16(node, 0x1E)) / 2);
		U16(p, 0x20) = (uint16_t)(((int32_t)S16(node, 0x198) + S16(node, 0x20)) / 2);
		r = x::CrtRand();
		U16(p, 0x1E) = (uint16_t)(U16(p, 0x1E) + (uint16_t)((r & 0xFF) - 0x80));
		p = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_SparkC, 0x80, node);
		w = U32(node, 0x20);
		U32(p, 0x1C) = U32(node, 0x1C);
		U32(p, 0x20) = w;
		hb_872F60(p, 0x80);
		p = x::Effect_AddTaskAndInitFromCtx(Q_PART, ORIG_SparkC, 0x80, node);
		U16(p, 0x1C) = (uint16_t)(((int32_t)S16(node, 0x194) + S16(node, 0x1C)) / 2);
		U16(p, 0x1E) = (uint16_t)(((int32_t)S16(node, 0x196) + S16(node, 0x1E)) / 2);
		U16(p, 0x20) = (uint16_t)(((int32_t)S16(node, 0x198) + S16(node, 0x20)) / 2);
		hb_872F60(p, 0x80);
		shadow_copy(7, L, 8); // (RESIDUE: its frame's block (d7..d14) and blend word (d25))
		g_shadow[25] = BLEND_BUF;
		return 0; // void
	}

	// 0x873070 (sub_873070): static spark state 0 - flipbook 0x15F9250, last frame 0x12, next state
	uint32_t __cdecl hb_873070(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x15F9250;
		U16(a1, 0x52) = 0x12;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1; // void (eax = node)
	}

	// 0x873120 (sub_873120): rising spark state 0 - flipbook 0x15F944C, last frame 0xE, y velocity
	// +0x7A = -(rand & 0x1FF), next state
	uint32_t __cdecl hb_873120(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x15F944C;
		U16(a1, 0x52) = 0xE;
		const uint32_t r = x::CrtRand();
		U16(a1, 0x7A) = (uint16_t)(0 - (r & 0x1FF));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x873150 (sub_873150): rising spark state 1 - y velocity (+0x80, minus 3/16 of itself), y +=
	// velocity / 16, next flipbook frame (0x872570); past its last frame: finished, next state
	uint32_t __cdecl hb_873150(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t vy = damp((uint16_t)(U16(node, 0x7A) + 0x80));
		U16(node, 0x7A) = vy;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + div16(vy));
		if (a_743C20(node) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// the burst (0x8731D0 = the engine's 0x73F250 on {0x873230, 0x873240, 0x873350, ret})
	// ====================================================================================

	// 0x873230 (sub_873230): burst state 0 - waits for frame 0x10, next state
	uint32_t __cdecl hb_873230(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x10)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1; // void (eax = node)
	}

	// 0x873240 (sub_873240): burst state 1 - decodes the layout; placement +0x30 (0x20 bytes) = the
	// caster's bone-0xF1 matrix turned by 0x400 at its effect position 0xF1 moved by (0x20, -0x20,
	// -0xE0) through that matrix (-> position +0x1C); plays the burst with the block camera *
	// placement, next state
	uint32_t __cdecl hb_873240(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t fr[0x5C] = {};
		alignas(4) uint8_t v[8] = {};
		const uint32_t L = P(fr), vec = P(v);
		const uint32_t ent = caster_entity(node);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		const uint32_t M = node + 0x30;
		gx::sub_8DE8F0(ent, 0xF1, 0, M);
		x::sub_8DD7E0(M, 0x400);
		U16(vec, 0) = 0x20;
		U16(vec, 2) = 0xFFE0;
		U16(vec, 4) = 0xFF20;
		x::matrixMultiplyVector(M, vec, vec);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + U16(vec, 0));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(vec, 2));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + U16(vec, 4));
		S32(node, 0x44) = S16(node, 0x1C);
		S32(node, 0x48) = S16(node, 0x1E);
		S32(node, 0x4C) = S16(node, 0x20);
		x::ComposeAffineTransform(0x1D97778, M, L);
		U32(L, 0x48) = BLEND_BUF;
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_play(node, M);)
		prim_play(node + 0x94, PRIM_CB, L, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		// (RESIDUE: its frame's vector (d5, d6 low half), block (d7..d14) and blend word (d25))
		g_shadow[5] = U16(vec, 0) | (uint32_t)U16(vec, 2) << 16;
		g_shadow[6] = (g_shadow[6] & 0xFFFF0000) | U16(vec, 4);
		shadow_copy(7, L, 8);
		g_shadow[25] = BLEND_BUF;
		return 0; // void
	}

	// 0x873350 (sub_873350): burst state 2 - plays the burst with the block camera * placement +0x30;
	// when its model has ended: finished, next state
	uint32_t __cdecl hb_873350(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::ComposeAffineTransform(0x1D97778, node + 0x30, L);
		U32(L, 0x48) = BLEND_BUF;
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(held_note_play(node, node + 0x30);)
		if (prim_play(node + 0x94, PRIM_CB, L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		shadow_copy(7, L, 8); // (RESIDUE: its frame's block (d7..d14) and blend word (d25))
		g_shadow[25] = BLEND_BUF;
		return 0; // void
	}

	// 0x872700 (sub_872700): prim-model player draw callback (layout a1, record a2, block a3) - picks
	// the object's vertex frame (blended by MAG_017_sub_701390 into the block's buffer +0x48 between
	// two frames), builds its matrix: the record's rotation (0x701310); with record flag 0x200 the
	// record offset is used as is and the rotation is not composed with the block (screen axes),
	// otherwise the offset is turned by the block and the rotation composed with it; translation =
	// offset + the block's; scale unless 1.0; fade; draws it into the module arena with
	// Effect_RenderPrimModel
	uint32_t __cdecl hb_872700(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x38 bytes, offsets as in the original frame after its pushes): +0x00 SVECTOR
		// offset, +0x08 scale (3 x int32), +0x18 Mat4x3 object matrix (+0x2C translation)
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
		const uint32_t flags = U32(rec, 4);
		U16(L, 0) = U16(rec, 8);
		U16(L, 2) = U16(rec, 0xA);
		U16(L, 4) = U16(rec, 0xC);
		int32_t ox, oy, oz;
		if ((flags & 0x200) != 0)
		{
			ox = S16(L, 0);
			oy = S16(L, 2);
			oz = S16(L, 4);
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(L);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(blk, M);
			ox = S32(M, 0x14);
			oy = S32(M, 0x18);
			oz = S32(M, 0x1C);
		}
		S32(M, 0x14) = add32(ox, S32(blk, 0x14));
		S32(M, 0x18) = add32(oy, S32(blk, 0x18));
		S32(M, 0x1C) = add32(oz, S32(blk, 0x1C));
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
			U32(hdr, 0x1C) = 0x20F0;
			U32(hdr, 8) = U32(rec, 0x20);   // fade colour
		}
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x86CCC0, (void *)a_73A0D0, "031 MAG_031_sub_86CCC0" },
		{ 0x86CCD0, (void *)a_73A0D0, "031 MAG_031_sub_86CCD0" },
		{ 0x86CD70, (void *)a_73A380, "031 MAG_031_sub_86CD70" },
		{ 0x86CDD0, (void *)a_7335D0, "031 MAG_031_sub_86CDD0" },
		{ 0x86CE50, (void *)a_73A6D0, "031 nullsub_1669" },
		{ 0x86CE60, (void *)a_73A170, "031 MAG_031_sub_86CE60" },
		{ 0x86D150, (void *)a_7475A0, "031 sub_86D150" },
		{ 0x86D170, (void *)a_73A6D0, "031 nullsub_1672" },
		{ 0x86D180, (void *)a_73C100, "031 MAG_031_sub_86D180" },
		{ 0x86D1D0, (void *)a_73A380, "031 MAG_031_sub_86D1D0" },
		{ 0x86D720, (void *)a_73FE90, "031 sub_86D720" },
		{ 0x86DA50, (void *)a_740210, "031 sub_86DA50" },
		{ 0x86DEE0, (void *)a_7355E0, "031 sub_86DEE0" },
		{ 0x86E4B0, (void *)a_735BB0, "031 sub_86E4B0" },
		{ 0x86E6F0, (void *)a_735DF0, "031 sub_86E6F0" },
		{ 0x86F5D0, (void *)a_740910, "031 sub_86F5D0" },
		{ 0x86FC30, (void *)a_740F70, "031 sub_86FC30" },
		{ 0x86FC80, (void *)a_740FC0, "031 sub_86FC80" },
		{ 0x86FD10, (void *)a_741050, "031 sub_86FD10" },
		{ 0x86FDE0, (void *)a_741120, "031 sub_86FDE0" },
		{ 0x870820, (void *)a_741B60, "031 sub_870820" },
		{ 0x870F50, (void *)a_742290, "031 au_re__rand_54" },
		{ 0x870F80, (void *)a_7422C0, "031 sub_870F80" },
		{ 0x870FC0, (void *)a_742300, "031 sub_870FC0" },
		{ 0x871010, (void *)a_742350, "031 sub_871010" },
		{ 0x871060, (void *)a_7423A0, "031 au_re__rand_54_0" },
		{ 0x871170, (void *)a_7424B0, "031 sub_871170" },
		{ 0x871180, (void *)a_7424C0, "031 sub_871180" },
		{ 0x871960, (void *)a_742CA0, "031 sub_871960" },
		{ 0x871990, (void *)a_742CD0, "031 sub_871990" },
		{ 0x8719C0, (void *)a_742D00, "031 sub_8719C0" },
		{ 0x871B70, (void *)a_742EB0, "031 sub_871B70" },
		{ 0x871CB0, (void *)a_742FF0, "031 sub_871CB0" },
		{ 0x871F00, (void *)a_73A6D0, "031 nullsub_1673" },
		{ 0x871F60, (void *)a_73A380, "031 MAG_031_sub_871F60" },
		{ 0x872170, (void *)a_73A380, "031 sub_872170" },
		{ 0x872570, (void *)a_743C20, "031 sub_872570" },
		{ 0x8725A0, (void *)a_73A6D0, "031 nullsub_1674" },
		{ 0x8726F0, (void *)a_73A6D0, "031 nullsub_1675" },
		{ 0x872910, (void *)a_73A6D0, "031 nullsub_1676" },
		{ 0x872B00, (void *)a_73A6D0, "031 nullsub_1677" },
		{ 0x873090, (void *)a_743C00, "031 sub_873090" },
		{ 0x8730B0, (void *)a_73A6D0, "031 nullsub_1678" },
		{ 0x8731B0, (void *)a_73A6D0, "031 nullsub_1679" },
		{ 0x8731C0, (void *)a_73A6D0, "031 nullsub_1680" },
		{ 0x8733B0, (void *)a_73A6D0, "031 nullsub_1681" },
		{ 0x873420, (void *)a_73A6D0, "031 nullsub_1670" },
		{ 0x8734A0, (void *)a_73A380, "031 MAG_031_sub_8734A0" },
		{ 0x873560, (void *)a_73A6D0, "031 nullsub_1682" },
		{ 0x873570, (void *)a_747550, "031 MAG_031_sub_873570" },
		{ 0x873580, (void *)a_73A0D0, "031 MAG_031_sub_873580" },
		{ 0x873590, (void *)a_73A0D0, "031 MAG_031_sub_873590" },
		{ 0x8735A0, (void *)a_7475A0, "031 MAG_031_sub_8735A0" },
		{ 0x8735C0, (void *)a_73A6D0, "031 nullsub_1671" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x86CB50, (void *)hb_86CB50, "031 hb_86CB50" },
		{ 0x86CCE0, (void *)hb_86CCE0, "031 hb_86CCE0" },
		{ 0x86CD30, (void *)hb_86CD30, "031 hb_86CD30" },
		{ 0x86CE10, (void *)hb_86CE10, "031 hb_86CE10" },
		{ 0x86CE90, (void *)hb_86CE90, "031 hb_86CE90" },
		{ 0x86CF60, (void *)hb_86CF60, "031 hb_86CF60" },
		{ 0x86D060, (void *)hb_86D060, "031 hb_86D060" },
		{ 0x86D100, (void *)hb_86D100, "031 hb_86D100" },
		{ 0x86D230, (void *)hb_86D230, "031 hb_86D230" },
		{ 0x86D260, (void *)hb_86D260, "031 hb_86D260" },
		{ 0x86D410, (void *)hb_86D410, "031 hb_86D410" },
		{ 0x86D4F0, (void *)hb_86D4F0, "031 hb_86D4F0" },
		{ 0x86D650, (void *)hb_86D650, "031 hb_86D650" },
		{ 0x86D840, (void *)hb_86D840, "031 hb_86D840" },
		{ 0x86DCB0, (void *)hb_86DCB0, "031 hb_86DCB0" },
		{ 0x86DD40, (void *)hb_86DD40, "031 hb_86DD40" },
		{ 0x86DDA0, (void *)hb_86DDA0, "031 hb_86DDA0" },
		{ 0x86E020, (void *)hb_86E020, "031 hb_86E020" },
		{ 0x86E230, (void *)hb_86E230, "031 hb_86E230" },
		{ 0x86E990, (void *)hb_86E990, "031 hb_86E990" },
		{ 0x86EBC0, (void *)hb_86EBC0, "031 hb_86EBC0" },
		{ 0x86EE80, (void *)hb_86EE80, "031 hb_86EE80" },
		{ 0x86F0E0, (void *)hb_86F0E0, "031 hb_86F0E0" },
		{ 0x86F3C0, (void *)hb_86F3C0, "031 hb_86F3C0" },
		{ 0x86F480, (void *)hb_86F480, "031 hb_86F480" },
		{ 0x86F500, (void *)hb_86F500, "031 hb_86F500" },
		{ 0x86F540, (void *)hb_86F540, "031 hb_86F540" },
		{ 0x870570, (void *)hb_870570, "031 hb_870570" },
		{ 0x870680, (void *)hb_870680, "031 hb_870680" },
		{ 0x870700, (void *)hb_870700, "031 hb_870700" },
		{ 0x870730, (void *)hb_870730, "031 hb_870730" },
		{ 0x8707D0, (void *)hb_8707D0, "031 hb_8707D0" },
		{ 0x870850, (void *)hb_870850, "031 hb_870850" },
		{ 0x8708D0, (void *)hb_8708D0, "031 hb_8708D0" },
		{ 0x8710B0, (void *)hb_8710B0, "031 hb_8710B0" },
		{ 0x8712D0, (void *)hb_8712D0, "031 hb_8712D0" },
		{ 0x871DC0, (void *)hb_871DC0, "031 hb_871DC0" },
		{ 0x871E50, (void *)hb_871E50, "031 hb_871E50" },
		{ 0x871EE0, (void *)hb_871EE0, "031 hb_871EE0" },
		{ 0x871F10, (void *)hb_871F10, "031 hb_871F10" },
		{ 0x871FC0, (void *)hb_871FC0, "031 hb_871FC0" },
		{ 0x872020, (void *)hb_872020, "031 hb_872020" },
		{ 0x8721D0, (void *)hb_8721D0, "031 hb_8721D0" },
		{ 0x872310, (void *)hb_872310, "031 hb_872310" },
		{ 0x8723E0, (void *)hb_8723E0, "031 hb_8723E0" },
		{ 0x872440, (void *)hb_872440, "031 hb_872440" },
		{ 0x8724B0, (void *)hb_8724B0, "031 hb_8724B0" },
		{ 0x8725B0, (void *)hb_8725B0, "031 hb_8725B0" },
		{ 0x872700, (void *)hb_872700, "031 hb_872700" },
		{ 0x872920, (void *)hb_872920, "031 hb_872920" },
		{ 0x872980, (void *)hb_872980, "031 hb_872980" },
		{ 0x872990, (void *)hb_872990, "031 hb_872990" },
		{ 0x8729F0, (void *)hb_8729F0, "031 hb_8729F0" },
		{ 0x872B10, (void *)hb_872B10, "031 hb_872B10" },
		{ 0x872B70, (void *)hb_872B70, "031 hb_872B70" },
		{ 0x872B80, (void *)hb_872B80, "031 hb_872B80" },
		{ 0x872CB0, (void *)hb_872CB0, "031 hb_872CB0" },
		{ 0x872F60, (void *)hb_872F60, "031 hb_872F60" },
		{ 0x873010, (void *)hb_873010, "031 hb_873010" },
		{ 0x873070, (void *)hb_873070, "031 hb_873070" },
		{ 0x8730C0, (void *)hb_8730C0, "031 hb_8730C0" },
		{ 0x873120, (void *)hb_873120, "031 hb_873120" },
		{ 0x873150, (void *)hb_873150, "031 hb_873150" },
		{ 0x8731D0, (void *)hb_8731D0, "031 hb_8731D0" },
		{ 0x873230, (void *)hb_873230, "031 hb_873230" },
		{ 0x873240, (void *)hb_873240, "031 hb_873240" },
		{ 0x873350, (void *)hb_873350, "031 hb_873350" },
		{ 0x8733C0, (void *)hb_8733C0, "031 hb_8733C0" },
		{ 0x8733E0, (void *)hb_8733E0, "031 hb_8733E0" },
		{ 0x873430, (void *)hb_873430, "031 hb_873430" },
		{ 0x873470, (void *)hb_873470, "031 hb_873470" },
		{ 0x873500, (void *)hb_873500, "031 hb_873500" },
		{ 0x873520, (void *)hb_873520, "031 hb_873520" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag031_heartbreak()
	{
		act::register_module(31);
		for (const act::heartbreak::ModPort *p = act::heartbreak::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(31, p->addr, p->port, p->name);
		for (const act::heartbreak::ModPort *p = act::heartbreak::PORTS; p->addr; p++)
			act::register_module_port(31, p->addr, p->port, p->name);
		// 30 fps layer: see mag031_heartbreak_held.inc
		FX_HELD(register_mag031_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag031_heartbreak_held.inc"
#endif
