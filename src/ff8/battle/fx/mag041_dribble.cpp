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

// Effect 41: Dribble (enemy attack 56 of kernel.bin, used by Wendigo c0m032; MAG_041_*): a copy of
// the actor/heal effect library (see act_engine.h) built like Counter Laser-Eye
// (mag030_counter_laser_eye.cpp): the same master, emitter and actor-system code with the module's
// own globals and pools, plus the module's own dribble: the target turned into a ball, bounced in
// the caster's hand, thrown away, and walking back.
//
// Setup MAG_041_DRIBBLE_Init 0x8465C0 (runs once, not ported; file loader MAG_041_DRIBBLE_FL
// 0x8465A0 = the effect's data file 0x15CDDD8 -> 0x267B858, TIM uploaded by the setup, its two
// packet arenas at file + 0 / + 0x5000 (and + 0x5000 / + 0xA000), file arena cursor 0x2698AB0 =
// file + 0xA000) plays the camera animation 0x15CD9B4 and creates the root queue 0x268F288 with the
// master and three task pools: 0x26988E8 (4 x 0x58: emitters), 0x2690BD8 (3 x 0x2A4: actor
// systems) and 0x268F298 (0x14 x 0xDC: the dribble tasks).
//   Master (0x8466F0) - camera copy 0x2793E58 (the module scratch stack 0x2698AA4 grows down from
//     it), packet arena by tick parity (cursor 0x267B85C), bone follow, 11-state table: 0x846850
//     reserves the actor pools after the arena (0x870 + 0x8340 bytes = 20 particles of 0x6C and
//     50 actors of 0x2A0, cleared), 0x8468D0 one emitter per action (loop 0x84CF60 once the
//     emitter releases the master's +0x63 hold), then waits for the queues to empty.
//   Emitter (0x846900), one per action: bone follow + model bounds, sound 0x15CA5A8 at its tick
//     17; state 0 (0x846990) spawns the actor system (0x846A20 with the actor data 0x15CD8B0,
//     start tick 15, mode 0x2D, flag 0) and the dribble spawner (0x84B8C0), state 1 releases the
//     master at tick 116, state 2 applies the damage of the action's current target (0x506690)
//     from tick 116.
//   Actor system (0x846A20 -> 0x846A80 / 0x84B890): the engine's particle actors (emitters and
//     particles, sprite sequences 0x848E30 / prim models 0x847750 drawn into the module arena)
//     placed by the actor data's key positions; its step 0x847500 refreshes the key positions from
//     an entity's bones every tick.
//   Dribble spawner (0x84B8C0 = engine 0x73A380 on {0x84B920, 0x84CEC0, ret}): at its tick 8 the
//     dribble task.
//   Dribble (0x84B950): 11 states on the ball block 0x267B868 (0xB8 bytes) - set-up on the target
//     (its own draw hidden, one cast in 256 tilts the ball), grab (ticks 0..18), morph the target
//     into a ball (+0x200 a tick), bounces in the caster's hand (bone 0x18) until tick 51, the
//     throw (velocity from the target's facing, gravity 0x30, 16 ticks of spin), the landing
//     (dust, sound 0x15CA5AC), the target given back, turned round and walking back for 10 ticks;
//     while the ball is shown the target entity is placed at the ball and drawn by the module's
//     ball renderer (0x84BA60 / 0x84BB70: the model's vertices morphed between the model and a
//     sphere of the table radii 0x15CDC80, turned by the ball angles).
//   Dust (0x84C8E0 = engine 0x73A380 on {0x84C940, 0x84CCB0, ret}, two ticks): two tasks 0x84CA70
//     of 8 puffs each (random spread and speed), slowed by 3/16 a tick, flipbook 0x15CA5B0 of 16
//     frames drawn as sprite sequences (0x84CB00).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x267B858..0x2698AB8 (pools, queues, file pointer 0x267B858, packet cursor
// 0x267B85C, arenas, ball block 0x267B868, particle pool 0x2698A90 / cursor 0x26903D8, actor pool
// 0x2698AA8 / cursor 0x2698A9C, actor state 0x2698AB4, scratch stack pointer 0x2698AA4, vertex
// scratch 0x2690BE8, geometry descriptor 0x2698A60), the module scratch stack below the camera
// copy 0x2793E58.
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (db_XXXXXX), most of them
// the text of the matching Curaga / Full-life / Confuse / Counter Laser-Eye port with this
// module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace dribble
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_041 = { "dribble", 41, 0x8465A0, 0x84D010,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x268F1B8, 0x0, 0x26903D8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2690BD8, 0x0, 0x26988E8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2698A8E, 0x2698A90, 0x0, 0x2698A9E, 0x0, 0x2698AA4, 0x0, 0x2698AB4 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x846AB0, 0x847500, 0x8476F0, 0x847750, 0x0, 0x8479D0, 0x847BE0, 0x847E60, 0x8480A0, 0x848340, 0x848570, 0x848830, 0x848A90, 0x848D70, 0x848EB0, 0x848EF0, 0x84A180, 0x84A1D0, 0x84A200, 0x84A280, 0x84A970, 0x84A9C0, 0x84AA60, 0x84AB20, 0x84B370, 0x84B770, 0x84B800, 0x0, 0x0, 0x0, 0x0, 0x0, 0x846900, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x846C60, 0x846D40, 0x846EA0, 0x846F70, 0x847090, 0x8472A0, 0x848E30, 0x848F80, 0x8495E0, 0x849630, 0x8496C0, 0x849790, 0x849F20, 0x84A030, 0x84A0B0, 0x84A0E0, 0x84A900, 0x84A930, 0x84AA10, 0x84AB30, 0x84AC80, 0x84B310, 0x84B340, 0x84B520, 0x84B660, 0x0, 0x847500, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x267B85C;   // module packet cursor (the actors' arena)
	static const uint32_t ACTOR_STATE = 0x2698AB4;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x2698AA4;      // module scratch stack pointer
	static const uint32_t BLK = 0x267B868;             // the ball block (0xB8 bytes)
	static const uint32_t Q_EMITTER = 0x26988E8, Q_ACTORS = 0x2690BD8, Q_TASKS = 0x268F298;
	static const uint32_t ORIG_Master = 0x8466F0;
	static const uint32_t ORIG_Emitter = 0x846900;
	static const uint32_t ORIG_ActorSystem = 0x846A20;
	static const uint32_t ORIG_Spawner = 0x84B8C0;
	static const uint32_t ORIG_Dribble = 0x84B950;
	static const uint32_t ORIG_DustSpawner = 0x84C8E0;
	static const uint32_t ORIG_Dust = 0x84CA70;
	static const void *const SOUND_Grab = (const void *)0x15CA5A8;
	static const void *const SOUND_Land = (const void *)0x15CA5AC;
	static const uint32_t ACTOR_DATA = 0x15CD8B0;      // the actor system's data (exe data, relocated once by 0x8472A0)
	static const uint32_t RADIUS_TABLE = 0x15CDC80;    // ball y radius by dribble tick 0..56 (6-byte records)
	static const uint32_t DUST_FLIPBOOK = 0x15CA5B0;

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
	uint32_t __cdecl db_8466F0(uint32_t a1);
	uint32_t __cdecl db_846850(uint32_t a1);
	uint32_t __cdecl db_846890(void);
	uint32_t __cdecl db_846900(uint32_t a1);
	uint32_t __cdecl db_846990(uint32_t a1);
	uint32_t __cdecl db_846A80(uint32_t a1);
	uint32_t __cdecl db_846AB0(uint32_t a1);
	uint32_t __cdecl db_846C60(uint32_t a1);
	uint32_t __cdecl db_846D40(void);
	uint32_t __cdecl db_846EA0(void);
	uint32_t __cdecl db_847090(void);
	uint32_t __cdecl db_847500(uint32_t a1);
	uint32_t __cdecl db_8476F0(void);
	uint32_t __cdecl db_847750(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_8479D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_847BE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_848340(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_848570(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_848830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_848A90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_848D70(void);
	uint32_t __cdecl db_848E30(uint32_t a1);
	uint32_t __cdecl db_848EB0(void);
	uint32_t __cdecl db_848EF0(uint32_t a1);
	uint32_t __cdecl db_849F20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84A030(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84A0B0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84A0E0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl db_84A180(uint32_t a1);
	uint32_t __cdecl db_84A200(uint32_t a1);
	uint32_t __cdecl db_84A280(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl db_84AA60(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84AC80(uint32_t a1);
	uint32_t __cdecl db_84B770(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84B800(void);
	uint32_t __cdecl db_84B890(uint32_t a1);
	uint32_t __cdecl db_84B920(uint32_t a1);
	uint32_t __cdecl db_84B950(uint32_t a1);
	uint32_t __cdecl db_84BA60(uint32_t a1);
	uint32_t __cdecl db_84BB70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl db_84C220(uint32_t a1);
	uint32_t __cdecl db_84C2D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl db_84C350(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84C3A0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84C460(uint32_t a1, uint32_t a2);
	uint32_t __cdecl db_84C610(uint32_t a1);
	uint32_t __cdecl db_84C650(uint32_t a1);
	uint32_t __cdecl db_84C6B0(uint32_t a1);
	uint32_t __cdecl db_84C6F0(uint32_t a1);
	uint32_t __cdecl db_84C7A0(uint32_t a1);
	uint32_t __cdecl db_84C940(uint32_t a1);
	uint32_t __cdecl db_84CA70(uint32_t a1);
	uint32_t __cdecl db_84CB00(uint32_t a1);
	uint32_t __cdecl db_84CB70(uint32_t a1);
	uint32_t __cdecl db_84CB90(uint32_t a1);
	uint32_t __cdecl db_84CCE0(uint32_t a1);
	uint32_t __cdecl db_84CD20(uint32_t a1);
	uint32_t __cdecl db_84CDE0(uint32_t a1);
	uint32_t __cdecl db_84CE70(uint32_t a1);
	uint32_t __cdecl db_84CEF0(uint32_t a1);
	uint32_t __cdecl db_84CF10(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag041_dribble_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace dribble
{
	// ====================================================================================
	// the library functions of this module whose code matches a Full-life / Curaga / Confuse /
	// Counter Laser-Eye port up to the module's addresses and callees (the port's text with the
	// module's addresses; the comments describe the source original), then the module's own code
	// ====================================================================================
	// 0x8466F0 (copy of fl_883240 0x883240: MAG_027_sub_883240): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the three queues (live task count -> node +0x5E; the actor counters
	// 0x26D9032 / 0x26D9022 are cleared before the queues run)
	uint32_t __cdecl db_8466F0(uint32_t a1)
	{
		g_mod = &MOD_041;
		// 30 fps layer: see mag041_dribble_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x268F1B8) = 0x2793E58;
		states[0] = 0x846830;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x846840;
		states[2] = 0x846850;
		states[3] = 0x8468D0;
		states[4] = 0x84CF60;
		states[5] = 0x84CFA0;
		states[6] = 0x84CFB0;
		states[7] = 0x84CFC0;
		states[8] = 0x84CFD0;
		states[9] = 0x84CFE0;
		states[10] = 0x84D000; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2698A5C);
			const uint32_t v2 = MEM<uint32_t>(0x26903E4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x268F1B4) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2698A58);
			const uint32_t v2 = MEM<uint32_t>(0x26903E0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x268F1B4) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x2698A9E) = 0;
		MEM<uint16_t>(0x2698A8E) = 0;
		static const uint32_t queues[3] = { Q_EMITTER, Q_ACTORS, Q_TASKS };
		for (int i = 0; i < 3; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x846890 (copy of cl_873910 0x873910: MAG_030_sub_873910; = Wind Blast 0x879750 with this module's pool sizes): clears
	// the two actor pools (0x870 bytes at [0x26D6A14], 0x8340 bytes at [0x26D6A2C]) and the pool
	// cursors / counters
	uint32_t __cdecl db_846890(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2698A90), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2698AA8), 0x8340);
		MEM<uint16_t>(0x26903D8) = 0;
		MEM<uint16_t>(0x2698A9C) = 0;
		MEM<uint16_t>(0x2698A8C) = 0;
		MEM<uint16_t>(0x267B860) = 0;
		return 0; // void
	}

	// 0x846A80 (copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl db_846A80(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			db_846AB0(a1);
			db_847500(a1);
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
	uint32_t __cdecl db_846AB0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(0x2698AB4) = st;
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
			st = MEM<uint32_t>(0x2698AB4);
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
		db_846D40();
		st = MEM<uint32_t>(0x2698AB4);
		if (U16(st, 0x24) == 4)
			callp(0x846C60, node);
		db_847090();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(0x2698AB4);
		if (U16(st, 0x24) == 1)
		{
			callp(0x846EA0, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(0x2698AB4);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(0x2698AB4);
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
		st = MEM<uint32_t>(0x2698AB4);
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x846C60 (copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl db_846C60(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x2698AB4);
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
	uint32_t __cdecl db_846D40(void)
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
	uint32_t __cdecl db_846EA0(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x2698AB4);
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
	uint32_t __cdecl db_847090(void)
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

	// 0x8476F0 (copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl db_8476F0(void)
	{
		uint32_t act = U32(U32(0x2698AB4, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x2698AB4, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					db_847750(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x847750 (copy of cg_87ED70 0x87ED70: sub_87ED70; = Confuse c_860710): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl db_847750(uint32_t a1, uint32_t a2)
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

	// 0x8479D0 (copy of cg_87EFF0 0x87EFF0: sub_87EFF0; = Confuse c_860990, copy of b_72BA00 0x; 097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl db_8479D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x847BE0 (copy of cg_87F200 0x87F200: sub_87F200; = Confuse c_860BA0, copy of b_72BC10 0x; 097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl db_847BE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x848340 (copy of cg_87F960 0x87F960: sub_87F960; = Confuse c_861300, copy of b_72C370 0x72C370; Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl db_848340(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x848570 (copy of cg_87FB90 0x87FB90: sub_87FB90; = Confuse c_861530, copy of b_72C5A0 0x72C5A0; Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl db_848570(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x848830 (copy of cg_87FE50 0x87FE50: sub_87FE50; = Confuse c_8617F0, copy of b_72C860 0x; Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl db_848830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x848A90 (copy of cg_8800B0 0x8800B0: sub_8800B0; = Confuse c_861A50, copy of b_72CAC0 0x72CAC0; Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl db_848A90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x848D70 (copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl db_848D70(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x2698AB4), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x2698AB4);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						db_848E30(act);   // (the original also pushes the bone entry, unused)
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
							db_848E30(act);
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
	uint32_t __cdecl db_848E30(uint32_t a1)
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
	uint32_t __cdecl db_848EB0(void)
	{
		uint32_t act = U32(U32(0x2698AB4, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				db_84A180(act);
			else if (type == 1)
				db_848EF0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl db_848EF0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x2698AB4);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (db_84A030(act, bone) == 0)
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
			st = MEM<uint32_t>(0x2698AB4);
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
	uint32_t __cdecl db_849F20(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x2698AB4, 0);                  // actor state block
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
	uint32_t __cdecl db_84A030(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				db_84A0B0(a1, a2);
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
			db_84A0B0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			db_84A0B0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x84A0B0 (copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl db_84A0B0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return db_84A0E0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x84A0E0 (copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl db_84A0E0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x2698A90);
		int32_t idx = MEM<int16_t>(0x26903D8);
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
			MEM<uint16_t>(0x26903D8) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x26903D8) = 0;
		return slot;
	}

	// 0x84A180 (copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl db_84A180(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x84AB30, a1);
		else if (mode == 4)
			callp(0x84AC80, a1);
		db_84A200(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x84A200 (copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl db_84A200(uint32_t a1)
	{
		uint32_t sys = U32(0x2698AB4, 0);
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
				db_84A280(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			db_84A280(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x84A280 (copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl db_84A280(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x2698AA4, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x2698AA4, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = db_84AA60(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x2698AB4, 0);
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
		db_84B770(node, desc);
		U32(0x2698AA4, 0) = U32(0x2698AA4, 0) + 0x50;
		return 0; // void
	}

	// 0x84AA60 (copy of c_863A20 0x863A20: sub_863A20): the engine's 0x7387B0 with the module's actor pool: allocates an actor
	// record of 0x2A0 bytes from the pool [0x26A9BB4] starting at the cursor 0x26A9BA8 (wraps at 49:
	// record 49 of the 50 is never used), cleared, owner a1, definition index a2
	uint32_t __cdecl db_84AA60(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x2698AA8);
		int32_t i = MEM<int16_t>(0x2698A9C);
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
			MEM<uint16_t>(0x2698A9C) = 0;
		else
			MEM<uint16_t>(0x2698A9C) = (uint16_t)i;
		return slot;
	}

	// 0x84AC80 (copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl db_84AC80(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x2698AB4, 0);            // ecx
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
				uint32_t g = U32(0x2698AB4, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x2698AB4, 0);
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
	uint32_t __cdecl db_84B770(uint32_t a1, uint32_t a2)
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
			db_84A0E0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			db_84A0E0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			db_84A0E0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			db_84A0E0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x84B800 (copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via db_84A0E0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl db_84B800(void)
	{
		uint32_t dir = MEM<uint32_t>(0x2698AB4);  // eax (re-read only after the calls)
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
					db_84A0E0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x2698AB4);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				db_84A0E0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x2698AB4);
			}
		}
		return 0; // void
	}

	// 0x84B890 (copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl db_84B890(uint32_t a1)
	{
		if (db_847500(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x84CB00 (copy of cl_878F80 0x878F80: sub_878F80; = Cure's MAG_001_CURE_DrawSprite 0x8D6F20): draws the node's flipbook
	// +0x4C frame +0x50 at +0x1C (camera-facing, angle +0x54) as a sprite sequence into the module
	// arena, unless hidden (+0x26 bit 2)
	uint32_t __cdecl db_84CB00(uint32_t a1)
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

	// 0x846850 (MAG_041_sub_846850; = Counter Laser-Eye 0x8738C0 without the stage light ramp):
	// master state - once no child is alive: the two actor pools are carved from the file arena
	// cursor [0x2698AB0] (0x870 bytes = 20 particles of 0x6C, 0x8340 bytes = 50 actors of 0x2A0)
	// and cleared (0x846890), next state
	uint32_t __cdecl db_846850(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x2698AB0);
		MEM<uint32_t>(0x2698A90) = p;
		p += 0x870;
		MEM<uint32_t>(0x2698AA8) = p;
		p += 0x8340;
		MEM<uint32_t>(0x2698AB0) = p;
		db_846890();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x846900 (MAG_041_sub_846900): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x846990 actor system + dribble, 0x84CEF0 release the master, 0x84CF10 damage,
	// ret}, sound 0x15CA5A8 at its tick 17
	uint32_t __cdecl db_846900(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x846990;
		states[1] = 0x84CEF0;
		states[2] = 0x84CF10;
		states[3] = 0x84CF50; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0x11)
			x::BdPlaySE(P(SOUND_Grab), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x846990 (MAG_041_sub_846990): emitter state 0 - the actor system (0x846A20, actor data
	// 0x15CD8B0, start tick 15, mode 0x2D, flag 0) through the engine spawner 0x8469D0 and the
	// dribble spawner task 0x84B8C0 (0xDC B, queue 0x268F298), next state
	uint32_t __cdecl db_846990(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, ACTOR_DATA, 0xF, 0x2D, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Spawner, 0xDC, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84CEF0 (MAG_041_sub_84CEF0): emitter state 1 - at tick 116 releases the master (root +0x63
	// = 0), next state
	uint32_t __cdecl db_84CEF0(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x74)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x84CF10 (MAG_041_sub_84CF10): emitter state 2 - from tick 116: damage (action result) of
	// the action's current target, finished, next state
	uint32_t __cdecl db_84CF10(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x74)
			return 0; // void
		const uint32_t act = U32(U32(node, 0xC), 4) + (uint32_t)(S8(node, 0x2A) * 20);
		x::ApplyActionResultToTarget(U32(act, 8) + (uint32_t)(S8(node, 0x2B) * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x847500 (sub_847500; the engine's step of the actor system 0x7353B0 with the key positions
	// refreshed every tick): state block sys = node +0x34 (-> 0x2698AB4); unless its mode +0x24 is
	// 2, the point A (+0x114, 16.16) = the bone 0x18 effect position of the entity S16 sys +0x1E,
	// B (+0x154) = its bone 0x15 position, both lifted by 0x100 in z, the middle (+0xD4) = (A + B)
	// / 2, C (+0x204) = the bone 0x18 position lifted by 0x100 twice more (z; y = 0); then the
	// phase +0x20: 0 -> 1; 1 = one tick of the actors (0x84B800 spawns, 0x848EB0 update, 0x848D70
	// sprite draws, 0x8476F0 model draws), finished once the frame +0x18 reaches +0x1A with no
	// sprite / model left; 2 = done (returns 1). The live counts +0x14 / +0x16 are added to
	// 0x2698A9E / 0x2698A8E.
	// UNINIT 0x84751A..0x84764D: with mode 2 the three positions are never computed: the original
	// reads its stack words [esp+4..8] as left by earlier code (not reproducible; the port uses 0).
	// The effect's actor data never sets mode 2.
	uint32_t __cdecl db_847500(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		MEM<uint32_t>(ACTOR_STATE) = sys;
		int16_t pos[4] = { 0, 0, 0, 0 };
		const uint32_t pp = P(pos);
		if (U16(sys, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(0x1D972C0 + (uint32_t)((int32_t)S16(sys, 0x1E) * 0x9C), 0x18, 0, pp);
			sys = MEM<uint32_t>(ACTOR_STATE);
		}
		S16(pp, 4) = (int16_t)(U16(pp, 4) + 0x100);
		S32(sys, 0x114) = shl32(S16(pp, 0), 16);
		S32(sys, 0x118) = shl32(S16(pp, 2), 16);
		S32(sys, 0x11C) = shl32(S16(pp, 4), 16);
		if (U16(sys, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(0x1D972C0 + (uint32_t)((int32_t)S16(sys, 0x1E) * 0x9C), 0x15, 0, pp);
			sys = MEM<uint32_t>(ACTOR_STATE);
		}
		S16(pp, 4) = (int16_t)(U16(pp, 4) + 0x100);
		S32(sys, 0x154) = shl32(S16(pp, 0), 16);
		S32(sys, 0x158) = shl32(S16(pp, 2), 16);
		S32(sys, 0x15C) = shl32(S16(pp, 4), 16);
		S32(sys, 0xD4) = add32(S32(sys, 0x114), S32(sys, 0x154)) / 2;
		S32(sys, 0xD8) = add32(S32(sys, 0x118), S32(sys, 0x158)) / 2;
		S32(sys, 0xDC) = add32(S32(sys, 0x11C), S32(sys, 0x15C)) / 2;
		if (U16(sys, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(0x1D972C0 + (uint32_t)((int32_t)S16(sys, 0x1E) * 0x9C), 0x18, 0, pp);
			sys = MEM<uint32_t>(ACTOR_STATE);
		}
		S16(pp, 4) = (int16_t)(U16(pp, 4) + 0x100);
		S32(sys, 0x204) = shl32(S16(pp, 0), 16);
		S32(sys, 0x208) = 0;
		S32(sys, 0x20C) = shl32(S16(pp, 4), 16);
		const uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			db_84B800();
			db_848EB0();
			// 30 fps layer: see mag041_dribble_held.inc
			FX_HELD(held_note_system(sys);)
			db_848D70();
			db_8476F0();
			sys = MEM<uint32_t>(ACTOR_STATE);
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
		const uint16_t c14 = U16(sys, 0x14);
		const uint16_t c16 = U16(sys, 0x16);
		MEM<uint16_t>(0x2698A9E) = (uint16_t)(MEM<uint16_t>(0x2698A9E) + c14);
		MEM<uint16_t>(0x2698A8E) = (uint16_t)(MEM<uint16_t>(0x2698A8E) + c16);
		return done;
	}

	// ====================================================================================
	// the dribble (module code): the target turned into a ball, bounced by the caster, thrown
	// ====================================================================================

	// 0x84B920 (sub_84B920): spawner state 0 (task 0x84B8C0 = engine 0x73A380) - from its tick 8
	// spawns the dribble task 0x84B950 (0xDC B, queue 0x268F298), next state
	uint32_t __cdecl db_84B920(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 8)
		{
			x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Dribble, 0xDC, a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x84B950 (sub_84B950): DRIBBLE task - 11-state table (0x84C220 set-up, 0x84C610 / 0x84C650 /
	// 0x84C6B0 grab and morph, 0x84C6F0 bounces, 0x84C7A0 throw, 0x84CCE0 give the target back,
	// 0x84CD20 / 0x84CDE0 walk back, 0x84CE70 end, ret); while +0x26 bit 3 is set the target entity
	// is placed at the ball position (block 0x267B868: +0x7C position - +0x74 pivot, into the entity
	// position +0x1C and its matrix translation +0x54) and drawn by 0x84BA60
	uint32_t __cdecl db_84B950(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x84C220;
		states[1] = 0x84C610;
		states[2] = 0x84C650;
		states[3] = 0x84C6B0;
		states[4] = 0x84C6F0;
		states[5] = 0x84C7A0;
		states[6] = 0x84CCE0;
		states[7] = 0x84CD20;
		states[8] = 0x84CDE0;
		states[9] = 0x84CE70;
		states[10] = 0x84CEB0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 8) != 0)
		{
			const uint32_t ent = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
			const int16_t px = (int16_t)(MEM<uint16_t>(BLK + 0x7C) - MEM<uint16_t>(BLK + 0x74));
			S16(ent, 0x1C) = px;
			S32(ent, 0x54) = px;
			const int16_t py = (int16_t)(MEM<uint16_t>(BLK + 0x7E) - MEM<uint16_t>(BLK + 0x76));
			S16(ent, 0x1E) = py;
			S32(ent, 0x58) = py;
			const int16_t pz = (int16_t)(MEM<uint16_t>(BLK + 0x80) - MEM<uint16_t>(BLK + 0x78));
			S16(ent, 0x20) = pz;
			S32(ent, 0x5C) = pz;
			// 30 fps layer: see mag041_dribble_held.inc
			FX_HELD(held_note_ball(node);)
			db_84BA60(BLK);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x84BA60 (sub_84BA60; = ChocoBocle 0x716CD0 with the entity index at +0xB0): draws the target
	// entity (block a1 = 0x267B868): copies the entity's 0x20-byte world matrix (+0x40) to a1
	// +0x20, builds its bone matrices, composes camera x world into a1 +0x40 (rotation) / +0x54
	// (translation), draws the entity shadow (sub_5088A0 into OT +0x4040, frame arena cursor a1
	// +0x6C), then the body (entity +0x64) and the weapon (entity +0x78 -> +4) with the ball
	// renderer 0x84BB70, and rebuilds the bone matrices
	uint32_t __cdecl db_84BA60(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)((int32_t)S16(a1, 0xB0) * 0x9C);
		memcpy((void *)(a1 + 0x20), (const void *)(ent + 0x40), 0x20);
		const uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x40);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x42);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(a1 + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(a1 + 0x54);
		uint32_t cp = U32(a1, 0x6C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x4040;
		const uint32_t r = x::sub_5088A0(ent, ot, 0x10, U32(cp, 0));
		cp = U32(a1, 0x6C);
		const uint32_t ot44 = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(cp, 0) = r;
		db_84BB70(U32(ent, 0x64), ot44, 4, a1);
		const uint32_t wpn = U32(ent, 0x78);
		if (wpn != 0)
			db_84BB70(U32(wpn, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, a1);
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x84BB70 (sub_84BB70): ball renderer of a battle model (a1 = geometry {+0 bone matrices -
	// 0x10, +4 object table}, a2 = OT, a3 = depth shift, a4 = block 0x267B868 {+0 ball rotation,
	// +0x40 camera matrix, +0x64 -> {+4 vertex scratch}, +0x68 entity, +0x6C packet cursor pointer,
	// +0x74 pivot, +0x9C / +0x9E / +0xA0 ball angles, +0xA8 morph, +0xAA..+0xAE ball radii}): per
	// visible object (entity +0x7C mask) every vertex is put in world space by its bone (light
	// matrix + background vector), taken relative to the pivot, and morphed between itself (weight
	// 0x1000 - morph) and its direction scaled by the radii (weight morph) through the GTE
	// interpolation (0x45E0B0 / 0x45E9D0 / 0x45EBF0), turned by the ball rotation (colour matrix,
	// ColorV0) and put back at the pivot (20-byte scratch records {x, y, z, pad, sxy, sz, -});
	// projected (RTPS), then back-face-culled textured triangles (POLY_FT3, 7 words) and quads
	// (POLY_FT4, 9 words) tinted with the entity colour (+0x28), inserted with
	// SSIGPU_InsertPrimDepthKeys
	uint32_t __cdecl db_84BB70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t blk = a4;
		const uint32_t verts = U32(U32(blk, 0x64), 4);  // vertex scratch (ebx)
		const uint32_t ent = U32(blk, 0x68);
		uint32_t cursor = U32(U32(blk, 0x6C), 0);       // packet cursor (local +0x1C)
		const uint32_t bones = U32(a1, 0) + 0x10;       // local +0x30
		uint32_t tab = U32(a1, 4);                      // object table {count, offsets[count]}
		const int32_t count = S32(tab, 0);              // local +0x2C
		const uint32_t w = x::Field_Alloc(0xA8);        // scratch block (ebp)
		tab += 4;
		U16(w, 0x9E) = U16(blk, 0xA8);                  // morph weight of the ball
		U16(w, 0x9C) = (uint16_t)(0x1000 - U16(blk, 0xA8));
		U32(w, 0x5C) = U32(blk, 0x74);                  // pivot x, y
		U32(w, 0x60) = U32(blk, 0x78);                  // pivot z (+ pad)
		U16(w, 0xA0) = U16(blk, 0xAA);                  // radii
		U16(w, 0xA4) = U16(blk, 0xAE);
		U32(w, 0x98) = U32(ent, 0x7C);                  // visible objects
		const uint32_t rgb = U32(ent, 0x28) & 0xFFFFFF;
		memcpy((void *)w, (const void *)blk, 0x20);     // rep movsd: the ball rotation
		U16(w, 0xA2) = U16(blk, 0xAC);
		U32(w, 0x90) = rgb | 0x24000000;                // POLY_FT3 code + colour
		U32(w, 0x94) = rgb | 0x2C000000;                // POLY_FT4 code + colour
		if (U16(blk, 0x9C) != 0)
			x::sub_8DD7E0(w, (uint32_t)(int32_t)S16(blk, 0x9C));
		if (U16(blk, 0xA0) != 0)
			x::sub_8DD960(w, (uint32_t)(int32_t)S16(blk, 0xA0));
		if (U16(blk, 0x9E) != 0)
			x::MAG_022_sub_8DD8A0(w, (uint32_t)(int32_t)S16(blk, 0x9E));
		gx::sub_45DEA0(w);
		x::GTE_SetRotMatrix(blk + 0x40);
		x::GTE_SetTransVector(blk + 0x40);
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t obj = U32(a1, 4) + U32(tab, 0);
			tab += 4;
			if (((U32(w, 0x98) >> (i & 31)) & 1) == 0)
				continue;
			// pass 1: world vertex, relative to the pivot, morphed to the ball, turned, back
			const uint32_t groups = obj + 2;
			uint32_t p = groups;
			uint32_t wv = verts;
			for (int32_t g = (int32_t)S16(obj, 0); g > 0; g--)
			{
				const int32_t bone = (int32_t)S16(p, 0);
				p += 2;
				const uint32_t m = (uint32_t)(bone * 3 * 16) + bones + 0x10;
				gx::GTE_SetLightMatrix(m);
				gx::GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				const int32_t nv = (int32_t)S16(p, 0);
				p += 2;
				const uint32_t v = w + 0x4C;
				for (int32_t k = nv; k > 0; k--)
				{
					const uint16_t vx = U16(p, 0);
					const uint16_t vy = U16(p, 2);
					p += 4;
					U16(v, 0) = vx;
					U16(w, 0x4E) = vy;
					U16(w, 0x50) = U16(p, 0);
					p += 2;
					x::GTE_LoadV0(v);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(v);
					U16(v, 0) = (uint16_t)(U16(v, 0) - U16(w, 0x5C));
					U16(w, 0x4E) = (uint16_t)(U16(w, 0x4E) - U16(w, 0x5E));
					U16(w, 0x50) = (uint16_t)(U16(w, 0x50) - U16(w, 0x60));
					U32(w, 0x54) = U32(v, 0);
					U32(w, 0x58) = U32(v, 4);
					gx::sub_56BDE0(v, v);
					S16(v, 0) = (int16_t)(mul32(S16(w, 0xA0), S16(v, 0)) / 4096);
					S16(w, 0x4E) = (int16_t)(mul32(S16(w, 0xA2), S16(w, 0x4E)) / 4096);
					S16(w, 0x50) = (int16_t)(mul32(S16(w, 0xA4), S16(w, 0x50)) / 4096);
					x::set_dword_1CA8A30((uint32_t)(int32_t)S16(w, 0x9C));
					gx::sub_45E0B0(w + 0x54);
					gx::sub_45E9D0();
					x::set_dword_1CA8A30((uint32_t)(int32_t)S16(w, 0x9E));
					gx::sub_45E0B0(v);
					gx::sub_45EBF0();
					x::GTE_StoreIR123(v);
					x::GTE_LoadV0(v);
					gx::GTE_MVMVA_ColorV0();
					x::GTE_StoreIR123(v);
					U16(v, 0) = (uint16_t)(U16(v, 0) + U16(w, 0x5C));
					U16(w, 0x4E) = (uint16_t)(U16(w, 0x4E) + U16(w, 0x5E));
					U16(w, 0x50) = (uint16_t)(U16(w, 0x50) + U16(w, 0x60));
					U32(wv, 0) = U32(v, 0);
					U32(wv, 4) = U32(v, 4);
					wv += 0x14;
				}
			}
			// pass 2: project every vertex (sxy -> +8, sz -> +0xC)
			uint32_t q = groups;
			uint32_t vp = verts;
			for (int32_t g = (int32_t)S16(obj, 0); g > 0; g--)
			{
				const int32_t nv = (int32_t)S16(q, 2);
				q += 4;
				if (nv <= 0)
					continue;
				const uint32_t next = q + (uint32_t)(nv * 3 * 2);
				uint32_t zp = vp + 0xC;
				for (int32_t k = nv; k > 0; k--)
				{
					U32(w, 0x34) = U32(vp, 0);
					U32(w, 0x38) = U32(vp, 4);
					x::GTE_LoadV0(w + 0x34);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(zp - 4);
					gx::sub_45E220(zp);
					vp += 0x14;
					zp += 0x14;
				}
				q = next;
			}
			// faces (4-aligned): u16 tri count, u16 quad count, 8 bytes, then the records
			uint32_t f = (q + 3) & 0xFFFFFFFCu;
			const int32_t ntri = (int32_t)S16(f, 0);
			const int32_t nquad = (int32_t)S16(f, 2);
			f += 0xC;
			uint32_t pk = cursor;
			// triangles: 16-byte records {u16 i0,i1,i2, uv2, u32 uv0+clut, u32 uv1+tpage}
			for (int32_t k = ntri; k > 0; k--, f += 0x10)
			{
				const uint32_t i2 = U16(f, 4) & 0xFFF;
				const uint32_t i1 = U16(f, 2) & 0xFFF;
				const uint32_t i0 = U16(f, 0) & 0xFFF;
				U32(w, 0x88) = i2;
				U32(w, 0x84) = i1;
				U32(w, 0x80) = i0;
				const uint32_t s2 = U32(verts + i2 * 20, 8);
				const uint32_t s1 = U32(verts + i1 * 20, 8);
				const uint32_t s0 = U32(verts + i0 * 20, 8);
				U32(w, 0x24) = s0;
				U32(w, 0x28) = s1;
				U32(w, 0x2C) = s2;
				gx::sub_45E160(s0, s1, s2);
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(w + 0x7C);
				if (S32(w, 0x7C) <= 0)
					continue;
				U32(pk, 8) = U32(w, 0x24);
				U32(pk, 0x10) = U32(w, 0x28);
				U32(pk, 0x18) = U32(w, 0x2C);
				U32(pk, 0xC) = U32(f, 8);
				U32(pk, 0) = 0x7000000;
				U32(pk, 0x14) = U32(f, 0xC);
				U16(pk, 0x1C) = U16(f, 6);
				U32(pk, 4) = U32(w, 0x90);
				if (U8(f, 0xF) & 2)
					U8(pk, 7) |= 2;
				const int32_t z2 = S32(verts + U32(w, 0x88) * 20, 0xC);
				const int32_t z1 = S32(verts + U32(w, 0x84) * 20, 0xC);
				const int32_t z0 = S32(verts + U32(w, 0x80) * 20, 0xC);
				const int32_t sum = add32(add32(z1, z0), z2);
				// sum / 3 (magic 0x55555556: high dword + its sign bit)
				int32_t hq = (int32_t)(((int64_t)sum * 0x55555556LL) >> 32);
				hq = add32(hq, (int32_t)((uint32_t)hq >> 31));
				const int32_t key = hq >> (a3 & 31);
				U32(w, 0x78) = (uint32_t)key;
				x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, 0);
				pk += 0x20;
			}
			// quads: 20-byte records {u16 i0,i1,i2,i3, u32 uv0+clut, u32 uv1+tpage, u16 uv2, u16 uv3}
			if (nquad > 0)
			{
				f += 4;
				for (int32_t k = nquad; k > 0; k--, f += 0x14)
				{
					const uint32_t i2 = U16(f, 0) & 0xFFF;
					const uint32_t i1 = U16(f, -2) & 0xFFF;
					const uint32_t i0 = U16(f, -4) & 0xFFF;
					U32(w, 0x88) = i2;
					U32(w, 0x84) = i1;
					U32(w, 0x80) = i0;
					const uint32_t s2 = U32(verts + i2 * 20, 8);
					const uint32_t s1 = U32(verts + i1 * 20, 8);
					const uint32_t s0 = U32(verts + i0 * 20, 8);
					U32(w, 0x24) = s0;
					U32(w, 0x28) = s1;
					U32(w, 0x2C) = s2;
					gx::sub_45E160(s0, s1, s2);
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(w + 0x7C);
					if (S32(w, 0x7C) <= 0)
						continue;
					const uint32_t i3 = U16(f, 2) & 0xFFF;
					U32(pk, 8) = U32(w, 0x24);
					U32(w, 0x8C) = i3;
					U32(pk, 0x10) = U32(w, 0x28);
					U32(pk, 0) = 0x9000000;
					const uint32_t v3 = verts + i3 * 20;
					const uint32_t s3 = U32(v3, 8);
					U32(w, 0x30) = s3;
					U32(pk, 0x20) = s3;
					U32(pk, 0x18) = U32(w, 0x2C);
					U32(pk, 0xC) = U32(f, 4);
					U32(pk, 0x14) = U32(f, 8);
					U16(pk, 0x1C) = U16(f, 0xC);
					U16(pk, 0x24) = U16(f, 0xE);
					U32(pk, 4) = U32(w, 0x94);
					if (U8(f, 0xB) & 2)
						U8(pk, 7) |= 2;
					const int32_t z3 = S32(v3, 0xC);
					const int32_t z2 = S32(verts + U32(w, 0x88) * 20, 0xC);
					const int32_t z1 = S32(verts + U32(w, 0x84) * 20, 0xC);
					const int32_t z0 = S32(verts + U32(w, 0x80) * 20, 0xC);
					const int32_t sum = add32(add32(add32(z0, z3), z1), z2);
					const int32_t key = sum >> ((a3 + 2) & 31);
					U32(w, 0x78) = (uint32_t)key;
					x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, (uint32_t)z3);
					pk += 0x28;
				}
			}
			cursor = pk;
		}
		U32(U32(blk, 0x6C), 0) = cursor;
		x::Field_Free(0xA8);
		return 0; // void
	}

	// 0x84C220 (sub_84C220): dribble state 0 - the ball block 0x267B868 set up on the target entity
	// (0x84C2D0), the entity's own draw hidden (entity +0 bit 2), the custom draw enabled (+0x26
	// bit 3), the entity position +0x1C / +0x20 kept (0x267B8F4 / 0x267B8F8); one cast in 256
	// (rand low byte 0) tilts the ball (flag +0xB4 = 1, angle +0x9E = 0x800); pivot, radii and ball
	// position of the first tick, next state
	uint32_t __cdecl db_84C220(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		db_84C2D0(BLK, 0x267B920, 0x2698A60, 0x2690BE8, ent, slot);
		U8(ent, 0) |= 4;
		const uint32_t p0 = U32(ent, 0x1C);
		const uint32_t p1 = U32(ent, 0x20);
		U8(node, 0x26) |= 8;
		MEM<uint32_t>(0x267B8F4) = p0;
		MEM<uint32_t>(0x267B8F8) = p1;
		if ((uint8_t)x::CrtRand() == 0)
		{
			MEM<uint16_t>(BLK + 0xB4) = 1;
			MEM<uint16_t>(BLK + 0x9E) = 0x800;
		}
		db_84C3A0(node, BLK);
		db_84C350(node, BLK);
		db_84C460(node, BLK);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x84C2D0 (sub_84C2D0): clears the ball block a1 (0xB8 bytes) and sets it up: +0x60 = a2,
	// +0x64 = geometry descriptor a3 (+4 = vertex scratch a4, +0x14..+0x24 defaults), +0x68 =
	// entity a5, +0x6C / +0x70 = the frame arena cursor pointer 0x1D8E054, +0xB0 = slot a6
	uint32_t __cdecl db_84C2D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		x::MAG_007_sub_8DCC00(a1, 0xB8);
		U32(a1, 0x68) = a5;
		U32(a1, 0x60) = a2;
		U32(a1, 0x6C) = 0x1D8E054;
		U32(a1, 0x70) = 0x1D8E054;
		U16(a1, 0xB0) = (uint16_t)a6;
		U32(a3, 4) = a4;
		U32(a1, 0x64) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFF;
		U16(a3, 0x24) = 0;
		return a1;
	}

	// 0x84C350 (sub_84C350): ball radii of the node's tick (<= 56) from the table 0x15CDC80 (6-byte
	// records): +0xAC = y radius, +0xAA = +0xAE = 0x200 - y radius
	uint32_t __cdecl db_84C350(uint32_t a1, uint32_t a2)
	{
		const int16_t t = S16(a1, 0x24);
		if (t > 0x38)
			return 0; // void
		const uint16_t ry = MEM<uint16_t>(RADIUS_TABLE + (uint32_t)((int32_t)t * 6));
		const uint16_t rxz = (uint16_t)(0x200 - ry);
		U16(a2, 0xAC) = ry;
		U16(a2, 0xAE) = rxz;
		U16(a2, 0xAA) = rxz;
		return 0; // void
	}

	// 0x84C3A0 (sub_84C3A0): pivot of the target entity (slot a1 +0x2D): its effect position 0xF0
	// (0xF1 unless the block flag +0xB4 is 1) with the entity at the origin and an identity
	// rotation, minus the entity position, into the block a2 +0x74; the entity's position and matrix
	// are put back
	uint32_t __cdecl db_84C3A0(uint32_t a1, uint32_t a2)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		const uint32_t save1c = U32(ent, 0x1C);
		uint32_t mtx[8];
		memcpy(mtx, (const void *)(ent + 0x40), 0x20); // rep movsd
		const uint32_t save20 = U32(ent, 0x20);
		U16(ent, 0x1C) = 0;
		U16(ent, 0x1E) = 0;
		U16(ent, 0x20) = 0;
		x::MAG_022_sub_8DD770(ent + 0x40);
		const uint32_t pv = a2 + 0x74;
		x::GetEffectSpawnPosition(ent, U16(a2, 0xB4) == 1 ? 0xF0 : 0xF1, 0, pv);
		U16(pv, 0) = (uint16_t)(U16(pv, 0) - U16(ent, 0x1C));
		U16(a2, 0x76) = (uint16_t)(U16(a2, 0x76) - U16(ent, 0x1E));
		U16(a2, 0x78) = (uint16_t)(U16(a2, 0x78) - U16(ent, 0x20));
		memcpy((void *)(ent + 0x40), mtx, 0x20); // rep movsd
		U32(ent, 0x1C) = save1c;
		U32(ent, 0x20) = save20;
		return 0; // void
	}

	// 0x84C460 (sub_84C460): ball position (block a2): until the node's tick 30 the ball rotation
	// (+0..+0x1F) is built (0x8DDB90) from the caster's direction bone 0x18 -> bone 0x15
	// (normalised, up = z), later it is the identity; the ball position +0x7C = the caster's bone
	// 0x18 effect position + (0, y, 0) turned by that rotation and the ball angles (+0xA0 z, +0x9C
	// x, +0x9E y), y = pivot y + 0x400 blended to the y radius +0xAC by the morph +0xA8
	uint32_t __cdecl db_84C460(uint32_t a1, uint32_t a2)
	{
		const uint32_t caster = 0x1D972C0 + (uint32_t)U8(U32(a1, 0xC), 0) * 0x9C;
		const uint32_t blk = a2;
		// stack locals of the original: +0x10 offset vector, +0x18 direction, +0x20 / +0x28 bone
		// positions, +0x30 up vector, +0x38 rotation copy
		alignas(4) uint8_t loc[0x58];
		memset(loc, 0, sizeof(loc));
		const uint32_t L = P(loc);
		if (S16(a1, 0x24) <= 0x1E)
		{
			x::GetEffectSpawnPosition(caster, 0x18, 0, L + 0x28);
			x::GetEffectSpawnPosition(caster, 0x15, 0, L + 0x20);
			U16(L, 0x18) = (uint16_t)(U16(L, 0x20) - U16(L, 0x28));
			U16(L, 0x1A) = (uint16_t)(U16(L, 0x22) - U16(L, 0x2A));
			U16(L, 0x1C) = (uint16_t)(U16(L, 0x24) - U16(L, 0x2C));
			U16(L, 0x30) = 0;
			U16(L, 0x32) = 0;
			U16(L, 0x34) = 0x1000;
			gx::sub_56BDE0(L + 0x18, L + 0x18);
			gx::sub_8DDB90(blk, L + 0x18, L + 0x30);
		}
		else
			x::MAG_022_sub_8DD770(blk);
		memcpy((void *)(L + 0x38), (const void *)blk, 0x20); // rep movsd
		const uint32_t pos = blk + 0x7C;
		x::GetEffectSpawnPosition(caster, 0x18, 0, pos);
		const uint16_t base = (uint16_t)(U16(blk, 0x76) + 0x400);
		U16(L, 0x10) = 0;
		U16(L, 0x14) = 0;
		const int32_t d = mul32(sub32(S16(blk, 0xAC), (int16_t)base), S16(blk, 0xA8)) / 4096;
		U16(L, 0x12) = (uint16_t)(d + base);
		if (U16(blk, 0xA0) != 0)
			x::sub_8DD960(L + 0x38, (uint32_t)(int32_t)S16(blk, 0xA0));
		if (U16(blk, 0x9C) != 0)
			x::sub_8DD7E0(L + 0x38, (uint32_t)(int32_t)S16(blk, 0x9C));
		if (U16(blk, 0x9E) != 0)
			x::MAG_022_sub_8DD8A0(L + 0x38, (uint32_t)(int32_t)S16(blk, 0x9E));
		x::matrixMultiplyVector(L + 0x38, L + 0x10, L + 0x10);
		U16(pos, 0) = (uint16_t)(U16(pos, 0) + U16(L, 0x10));
		U16(blk, 0x7E) = (uint16_t)(U16(blk, 0x7E) + U16(L, 0x12));
		U16(blk, 0x80) = (uint16_t)(U16(blk, 0x80) + U16(L, 0x14));
		return 0; // void
	}

	// 0x84C610 (sub_84C610): dribble state 1 - pivot, radii, ball position; from tick 19 next state
	uint32_t __cdecl db_84C610(uint32_t a1)
	{
		db_84C3A0(a1, BLK);
		if (S16(a1, 0x24) >= 0x13)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		db_84C350(a1, BLK);
		db_84C460(a1, BLK);
		return 0; // void
	}

	// 0x84C650 (sub_84C650): dribble state 2 - the morph +0xA8 grows by 0x200 a tick; at 0x1000 next
	// state; pivot, radii, ball position
	uint32_t __cdecl db_84C650(uint32_t a1)
	{
		db_84C3A0(a1, BLK);
		const uint16_t m = (uint16_t)(MEM<uint16_t>(BLK + 0xA8) + 0x200);
		MEM<uint16_t>(BLK + 0xA8) = m;
		if ((int16_t)m >= 0x1000)
		{
			MEM<uint16_t>(BLK + 0xA8) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		db_84C350(a1, BLK);
		db_84C460(a1, BLK);
		return 0; // void
	}

	// 0x84C6B0 (sub_84C6B0): dribble state 3 - pivot, radii, ball position, next state
	uint32_t __cdecl db_84C6B0(uint32_t a1)
	{
		db_84C3A0(a1, BLK);
		db_84C350(a1, BLK);
		db_84C460(a1, BLK);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84C6F0 (sub_84C6F0): dribble state 4 - pivot; from tick 51 the throw: the target's chain
	// transformation 6, velocity (sin, -0x200, cos of the target facing +0xE) * 128 / 4096 into
	// 0x267B8FC / 0x267B8FE / 0x267B900, flight counter 0x267B91A = 0, next state; radii, position
	uint32_t __cdecl db_84C6F0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		db_84C3A0(a1, BLK);
		if (S16(a1, 0x24) >= 0x33)
		{
			gx::QueueChainTransformation(ent, 6);
			const int32_t f = S16(ent, 0xE);
			MEM<uint16_t>(BLK + 0x96) = 0xFE00;
			const int32_t s = (int32_t)x::computeSin((uint32_t)f);
			MEM<uint16_t>(BLK + 0x94) = (uint16_t)(shl32(s, 7) / 4096);
			const int32_t c = (int32_t)x::computeCosine((uint32_t)(int32_t)S16(ent, 0xE));
			MEM<uint16_t>(BLK + 0xB2) = 0;
			MEM<uint16_t>(BLK + 0x98) = (uint16_t)(shl32(c, 7) / 4096);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		db_84C350(a1, BLK);
		db_84C460(a1, BLK);
		return 0; // void
	}

	// 0x84C7A0 (sub_84C7A0): dribble state 5 - the flight: pivot; for 16 ticks the ball spins
	// (+0x9C = -0x100 a tick) and, when tilted, straightens (+0x9E = (16 - t) * 128); the morph
	// shrinks by 0x100 a tick (to 0); position += velocity, y velocity += 0x30 (gravity); once
	// falling at the pivot height: the target's chain transformation 6, the landing dust spawner
	// 0x84C8E0 (queue 0x268F298, at the target position, y 0), sound 0x15CA5AC, next state;
	// flight counter + 1, radii
	uint32_t __cdecl db_84C7A0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		db_84C3A0(a1, BLK);
		const uint16_t t = MEM<uint16_t>(BLK + 0xB2);
		if ((int16_t)t <= 0x10)
		{
			MEM<uint16_t>(BLK + 0x9C) = (uint16_t)((uint16_t)(t * 0xFF00u) & 0xFFF);
			if (MEM<uint16_t>(BLK + 0xB4) == 1)
				MEM<uint16_t>(BLK + 0x9E) = (uint16_t)((uint16_t)(0x10 - t) << 7);
		}
		const uint16_t m = (uint16_t)(MEM<uint16_t>(BLK + 0xA8) + 0xFF00);
		MEM<uint16_t>(BLK + 0xA8) = m;
		if ((int16_t)m <= 0)
			MEM<uint16_t>(BLK + 0xA8) = 0;
		const uint16_t vx = MEM<uint16_t>(BLK + 0x94);
		const uint16_t vz = MEM<uint16_t>(BLK + 0x98);
		MEM<uint16_t>(BLK + 0x7C) = (uint16_t)(MEM<uint16_t>(BLK + 0x7C) + vx);
		const uint16_t vy = (uint16_t)(MEM<uint16_t>(BLK + 0x96) + 0x30);
		MEM<uint16_t>(BLK + 0x80) = (uint16_t)(MEM<uint16_t>(BLK + 0x80) + vz);
		MEM<uint16_t>(BLK + 0x7E) = (uint16_t)(MEM<uint16_t>(BLK + 0x7E) + vy);
		MEM<uint16_t>(BLK + 0x96) = vy;
		if ((int16_t)vy > 0 && (int16_t)MEM<uint16_t>(BLK + 0x7E) >= (int16_t)MEM<uint16_t>(BLK + 0x76))
		{
			gx::QueueChainTransformation(ent, 6);
			const uint16_t ground = MEM<uint16_t>(BLK + 0x76);
			MEM<uint16_t>(BLK + 0x7E) = ground;
			const uint32_t n = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_DustSpawner, 0xDC, a1);
			U32(n, 0x1C) = U32(ent, 0x1C);
			U32(n, 0x20) = U32(ent, 0x20);
			U16(n, 0x1E) = 0;
			x::BdPlaySE(P(SOUND_Land), 1, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		MEM<uint16_t>(BLK + 0xB2) = (uint16_t)(MEM<uint16_t>(BLK + 0xB2) + 1);
		db_84C350(a1, BLK);
		return 0; // void
	}

	// 0x84CCE0 (sub_84CCE0): dribble state 6 - custom draw off, the target's own draw back (entity
	// +0 bit 2 cleared), its y = its ground y +0x24, next state
	uint32_t __cdecl db_84CCE0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		U8(a1, 0x26) &= 0xF7;
		U16(ent, 0) &= 0xFFFB;
		U16(ent, 0x1E) = U16(ent, 0x24);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84CD20 (sub_84CD20): dribble state 7 - keeps the target's flags (0x267B90C) and sets its bit
	// 12; unless its animation state (entity +0x74 -> +0) is 6: chain transformation 0x1A, turned
	// round (facing +0xE - 0x800), walk-back velocity = (kept position - position) / 10 over 10
	// ticks, next state
	uint32_t __cdecl db_84CD20(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		const uint16_t fl = U16(ent, 0);
		MEM<uint32_t>(0x267B90C) = fl;
		U16(ent, 0) = (uint16_t)(fl | 0x1000);
		if (U8(U32(ent, 0x74), 0) == 6)
			return 0; // void
		gx::QueueChainTransformation(ent, 0x1A);
		const uint16_t facing = (uint16_t)((U16(ent, 0xE) - 0x800) & 0xFFF);
		MEM<uint16_t>(BLK + 0xB2) = 0xA;
		U16(ent, 0xE) = facing;
		MEM<uint16_t>(BLK + 0x94) = (uint16_t)(sub32(MEM<int16_t>(0x267B8F4), S16(ent, 0x1C)) / 10);
		MEM<uint16_t>(BLK + 0x98) = (uint16_t)(sub32(MEM<int16_t>(0x267B8F8), S16(ent, 0x20)) / 10);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84CDE0 (sub_84CDE0): dribble state 8 - the target walks back by the velocity; after 10 ticks
	// its chain transformation (entity +0x74 -> +2), the kept position, turned round, next state
	uint32_t __cdecl db_84CDE0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		const uint16_t vx = MEM<uint16_t>(BLK + 0x94);
		const uint16_t vz = MEM<uint16_t>(BLK + 0x98);
		U16(ent, 0x1C) = (uint16_t)(U16(ent, 0x1C) + vx);
		U16(ent, 0x20) = (uint16_t)(U16(ent, 0x20) + vz);
		const uint16_t t = (uint16_t)(MEM<uint16_t>(BLK + 0xB2) - 1);
		MEM<uint16_t>(BLK + 0xB2) = t;
		if ((int16_t)t > 0)
			return 0; // void
		gx::QueueChainTransformation(ent, U8(U32(ent, 0x74), 2));
		U32(ent, 0x1C) = MEM<uint32_t>(0x267B8F4);
		U32(ent, 0x20) = MEM<uint32_t>(0x267B8F8);
		U16(ent, 0xE) = (uint16_t)((U16(ent, 0xE) - 0x800) & 0xFFF);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84CE70 (sub_84CE70): dribble state 9 - finished; the target's flag bit 12 as kept, next state
	uint32_t __cdecl db_84CE70(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		U8(a1, 0x26) |= 1;
		const uint16_t d = (uint16_t)((U16(ent, 0) ^ MEM<uint16_t>(0x267B90C)) & 0x1000);
		U16(ent, 0) ^= d;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84C940 (sub_84C940): dust spawner state 0 (task 0x84C8E0 = engine 0x73A380) - spawns a dust
	// task 0x84CA70 (0xDC B, queue 0x268F298) of 8 puffs at the spawner position +0x1C: puff i
	// at angle i * 0x200 (+ 0x100 on the spawner's second tick), the task position moved by
	// (rand & 0x1FF) along it, velocity (rand & 0x7FF) + 0x400 along it, y velocity -0x80 -
	// (rand & 0x3FF); from its tick 1 next state
	uint32_t __cdecl db_84C940(uint32_t a1)
	{
		const uint32_t n = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Dust, 0xDC, a1);
		U16(n, 0xD8) = 8;
		uint32_t vel = n + 0x98;
		for (int32_t i = 0; i < S16(n, 0xD8); i++, vel += 8)
		{
			U32(vel, -0x40) = U32(a1, 0x1C);
			U32(vel, -0x3C) = U32(a1, 0x20);
			int32_t ang = shl32(i, 9);
			if (U16(a1, 0x24) != 0)
				ang += 0x100;
			const int32_t r1 = (int32_t)x::CrtRand();
			ang = (int16_t)ang;
			const int32_t d = (int16_t)(r1 & 0x1FF);
			const int32_t s1 = (int32_t)x::computeSin((uint32_t)ang);
			U16(n, 0x1C) = (uint16_t)(U16(n, 0x1C) + (uint16_t)(mul32(s1, d) / 4096));
			const int32_t c1 = (int32_t)x::computeCosine((uint32_t)ang);
			U16(n, 0x20) = (uint16_t)(U16(n, 0x20) + (uint16_t)(mul32(c1, d) / 4096));
			const int32_t r2 = (int32_t)x::CrtRand();
			const int32_t sp = (int16_t)((r2 & 0x7FF) + 0x400);
			const int32_t s2 = (int32_t)x::computeSin((uint32_t)ang);
			U16(vel, 0) = (uint16_t)(mul32(s2, sp) / 4096);
			const int32_t c2 = (int32_t)x::computeCosine((uint32_t)ang);
			U16(vel, 4) = (uint16_t)(mul32(c2, sp) / 4096);
			const int32_t r3 = (int32_t)x::CrtRand();
			U16(vel, 2) = (uint16_t)(-0x80 - (r3 & 0x3FF));
		}
		if (S16(a1, 0x24) >= 1)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x84CA70 (sub_84CA70): DUST task - state {0x84CB70 set-up, 0x84CB90 motion, ret}, then every
	// puff (+0x58 + 8 i) drawn as the node's flipbook at its position (0x84CB00)
	uint32_t __cdecl db_84CA70(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x84CB70;
		states[1] = 0x84CB90;
		states[2] = 0x84CCA0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		for (int32_t i = 0; i < S16(node, 0xD8); i++)
		{
			U32(node, 0x1C) = U32(node, 0x58 + 8 * i);
			U32(node, 0x20) = U32(node, 0x5C + 8 * i);
			// 30 fps layer: see mag041_dribble_held.inc
			FX_HELD(held_note_dust(node, i);)
			db_84CB00(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x84CB70 (sub_84CB70): dust state 0 - flipbook 0x15CA5B0, last frame 15, next state
	uint32_t __cdecl db_84CB70(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = DUST_FLIPBOOK;
		U16(a1, 0x52) = 0xF;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x84CB90 (sub_84CB90): dust state 1 - every puff: velocity -= velocity * 768 / 4096, position
	// += velocity / 16; next flipbook frame (0x84CC70 = engine 0x743C20); after the last one
	// finished, next state
	uint32_t __cdecl db_84CB90(uint32_t a1)
	{
		for (int32_t i = 0; i < S16(a1, 0xD8); i++)
		{
			const uint32_t v = a1 + 0x98 + 8 * i;
			for (int k = 0; k < 3; k++)
			{
				const int16_t s = S16(v, 2 * k);
				S16(v, 2 * k) = (int16_t)(s - (int16_t)(shl32(s * 3, 8) / 4096));
			}
			for (int k = 0; k < 3; k++)
				U16(v, 2 * k - 0x40) = (uint16_t)(U16(v, 2 * k - 0x40) + (uint16_t)(S16(v, 2 * k) / 16));
		}
		if (a_743C20(a1) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}


	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x846830, (void *)a_73A0D0, "041 MAG_041_sub_846830" },
		{ 0x846840, (void *)a_73A0D0, "041 MAG_041_sub_846840" },
		{ 0x8468D0, (void *)a_73A170, "041 MAG_041_sub_8468D0" },
		{ 0x8469D0, (void *)a_73C100, "041 MAG_041_sub_8469D0" },
		{ 0x846A20, (void *)a_73A380, "041 MAG_041_sub_846A20" },
		{ 0x846F70, (void *)a_73FE90, "041 sub_846F70" },
		{ 0x8472A0, (void *)a_740210, "041 sub_8472A0" },
		{ 0x847890, (void *)a_7355E0, "041 sub_847890" },
		{ 0x847E60, (void *)a_735BB0, "041 sub_847E60" },
		{ 0x8480A0, (void *)a_735DF0, "041 sub_8480A0" },
		{ 0x848F80, (void *)a_740910, "041 sub_848F80" },
		{ 0x8495E0, (void *)a_740F70, "041 sub_8495E0" },
		{ 0x849630, (void *)a_740FC0, "041 sub_849630" },
		{ 0x8496C0, (void *)a_741050, "041 sub_8496C0" },
		{ 0x849790, (void *)a_741120, "041 sub_849790" },
		{ 0x84A1D0, (void *)a_741B60, "041 sub_84A1D0" },
		{ 0x84A900, (void *)a_742290, "041 au_re__rand_49" },
		{ 0x84A930, (void *)a_7422C0, "041 sub_84A930" },
		{ 0x84A970, (void *)a_742300, "041 sub_84A970" },
		{ 0x84A9C0, (void *)a_742350, "041 sub_84A9C0" },
		{ 0x84AA10, (void *)a_7423A0, "041 au_re__rand_49_0" },
		{ 0x84AB20, (void *)a_7424B0, "041 sub_84AB20" },
		{ 0x84AB30, (void *)a_7424C0, "041 sub_84AB30" },
		{ 0x84B310, (void *)a_742CA0, "041 sub_84B310" },
		{ 0x84B340, (void *)a_742CD0, "041 sub_84B340" },
		{ 0x84B370, (void *)a_742D00, "041 sub_84B370" },
		{ 0x84B520, (void *)a_742EB0, "041 sub_84B520" },
		{ 0x84B660, (void *)a_742FF0, "041 sub_84B660" },
		{ 0x84B8B0, (void *)a_73A6D0, "041 nullsub_1599" },
		{ 0x84B8C0, (void *)a_73A380, "041 MAG_041_sub_84B8C0" },
		{ 0x84C8E0, (void *)a_73A380, "041 sub_84C8E0" },
		{ 0x84CC70, (void *)a_743C20, "041 sub_84CC70" },
		{ 0x84CCA0, (void *)a_73A6D0, "041 nullsub_1600" },
		{ 0x84CCB0, (void *)a_7475A0, "041 sub_84CCB0" },
		{ 0x84CCD0, (void *)a_73A6D0, "041 nullsub_1601" },
		{ 0x84CEB0, (void *)a_73A6D0, "041 nullsub_1602" },
		{ 0x84CEC0, (void *)a_7475A0, "041 sub_84CEC0" },
		{ 0x84CEE0, (void *)a_73A6D0, "041 nullsub_1603" },
		{ 0x84CF50, (void *)a_73A6D0, "041 nullsub_1604" },
		{ 0x84CF60, (void *)a_747500, "041 MAG_041_sub_84CF60" },
		{ 0x84CFA0, (void *)a_73A0D0, "041 MAG_041_sub_84CFA0" },
		{ 0x84CFB0, (void *)a_747550, "041 MAG_041_sub_84CFB0" },
		{ 0x84CFC0, (void *)a_73A0D0, "041 MAG_041_sub_84CFC0" },
		{ 0x84CFD0, (void *)a_73A0D0, "041 MAG_041_sub_84CFD0" },
		{ 0x84CFE0, (void *)a_7475A0, "041 MAG_041_sub_84CFE0" },
		{ 0x84D000, (void *)a_73A6D0, "041 nullsub_1598" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x8466F0, (void *)db_8466F0, "041 db_8466F0" },
		{ 0x846850, (void *)db_846850, "041 db_846850" },
		{ 0x846890, (void *)db_846890, "041 db_846890" },
		{ 0x846900, (void *)db_846900, "041 db_846900" },
		{ 0x846990, (void *)db_846990, "041 db_846990" },
		{ 0x846A80, (void *)db_846A80, "041 db_846A80" },
		{ 0x846AB0, (void *)db_846AB0, "041 db_846AB0" },
		{ 0x846C60, (void *)db_846C60, "041 db_846C60" },
		{ 0x846D40, (void *)db_846D40, "041 db_846D40" },
		{ 0x846EA0, (void *)db_846EA0, "041 db_846EA0" },
		{ 0x847090, (void *)db_847090, "041 db_847090" },
		{ 0x847500, (void *)db_847500, "041 db_847500" },
		{ 0x8476F0, (void *)db_8476F0, "041 db_8476F0" },
		{ 0x847750, (void *)db_847750, "041 db_847750" },
		{ 0x8479D0, (void *)db_8479D0, "041 db_8479D0" },
		{ 0x847BE0, (void *)db_847BE0, "041 db_847BE0" },
		{ 0x848340, (void *)db_848340, "041 db_848340" },
		{ 0x848570, (void *)db_848570, "041 db_848570" },
		{ 0x848830, (void *)db_848830, "041 db_848830" },
		{ 0x848A90, (void *)db_848A90, "041 db_848A90" },
		{ 0x848D70, (void *)db_848D70, "041 db_848D70" },
		{ 0x848E30, (void *)db_848E30, "041 db_848E30" },
		{ 0x848EB0, (void *)db_848EB0, "041 db_848EB0" },
		{ 0x848EF0, (void *)db_848EF0, "041 db_848EF0" },
		{ 0x849F20, (void *)db_849F20, "041 db_849F20" },
		{ 0x84A030, (void *)db_84A030, "041 db_84A030" },
		{ 0x84A0B0, (void *)db_84A0B0, "041 db_84A0B0" },
		{ 0x84A0E0, (void *)db_84A0E0, "041 db_84A0E0" },
		{ 0x84A180, (void *)db_84A180, "041 db_84A180" },
		{ 0x84A200, (void *)db_84A200, "041 db_84A200" },
		{ 0x84A280, (void *)db_84A280, "041 db_84A280" },
		{ 0x84AA60, (void *)db_84AA60, "041 db_84AA60" },
		{ 0x84AC80, (void *)db_84AC80, "041 db_84AC80" },
		{ 0x84B770, (void *)db_84B770, "041 db_84B770" },
		{ 0x84B800, (void *)db_84B800, "041 db_84B800" },
		{ 0x84B890, (void *)db_84B890, "041 db_84B890" },
		{ 0x84B920, (void *)db_84B920, "041 db_84B920" },
		{ 0x84B950, (void *)db_84B950, "041 db_84B950" },
		{ 0x84BA60, (void *)db_84BA60, "041 db_84BA60" },
		{ 0x84BB70, (void *)db_84BB70, "041 db_84BB70" },
		{ 0x84C220, (void *)db_84C220, "041 db_84C220" },
		{ 0x84C2D0, (void *)db_84C2D0, "041 db_84C2D0" },
		{ 0x84C350, (void *)db_84C350, "041 db_84C350" },
		{ 0x84C3A0, (void *)db_84C3A0, "041 db_84C3A0" },
		{ 0x84C460, (void *)db_84C460, "041 db_84C460" },
		{ 0x84C610, (void *)db_84C610, "041 db_84C610" },
		{ 0x84C650, (void *)db_84C650, "041 db_84C650" },
		{ 0x84C6B0, (void *)db_84C6B0, "041 db_84C6B0" },
		{ 0x84C6F0, (void *)db_84C6F0, "041 db_84C6F0" },
		{ 0x84C7A0, (void *)db_84C7A0, "041 db_84C7A0" },
		{ 0x84C940, (void *)db_84C940, "041 db_84C940" },
		{ 0x84CA70, (void *)db_84CA70, "041 db_84CA70" },
		{ 0x84CB00, (void *)db_84CB00, "041 db_84CB00" },
		{ 0x84CB70, (void *)db_84CB70, "041 db_84CB70" },
		{ 0x84CB90, (void *)db_84CB90, "041 db_84CB90" },
		{ 0x84CCE0, (void *)db_84CCE0, "041 db_84CCE0" },
		{ 0x84CD20, (void *)db_84CD20, "041 db_84CD20" },
		{ 0x84CDE0, (void *)db_84CDE0, "041 db_84CDE0" },
		{ 0x84CE70, (void *)db_84CE70, "041 db_84CE70" },
		{ 0x84CEF0, (void *)db_84CEF0, "041 db_84CEF0" },
		{ 0x84CF10, (void *)db_84CF10, "041 db_84CF10" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag041_dribble()
	{
		act::register_module(41);
		for (const act::dribble::ModPort *p = act::dribble::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(41, p->addr, p->port, p->name);
		for (const act::dribble::ModPort *p = act::dribble::PORTS; p->addr; p++)
			act::register_module_port(41, p->addr, p->port, p->name);
		// 30 fps layer: see mag041_dribble_held.inc
		FX_HELD(register_mag041_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag041_dribble_held.inc"
#endif
