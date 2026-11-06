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

// Effect 7: Mega Phoenix (battle item Mega Phoenix: kernel.bin battle item +4 = 7; MAG_007_*): a copy of the
// actor/heal effect library (see act_engine.h), built like Mega-Potion (mag053_megapotion.cpp: same master,
// emitter, stage fade, director set-up with ONE target per actor system and actor-system code with the
// module's own globals, its pool sizes / timings below) with its own prim-model actor draw 0x8CF4C0 (Raldo
// Throw's 0x8A46D0 render context with a texture V scroll on definition index 0, drawn through Raldo Throw's
// primitive-list dispatcher / list drawers: rh-identical copies, ported from mag019_raldo_throw.cpp). Its actor
// system (0x8CE970..0x8D41E0) is the library fx_actors.cpp (the act family's, on a Mem of module words, so
// Fujin's described effects run it too); this file keeps the master, the emitter and the stage fades, and the
// parameters (Fujin's effect lab): the emitter's ticks and sound, the actor system's start / length / mode /
// mask, the fades, and every field of the actor data's five definitions (fx_actors.h DefParams, written into
// the data at the master's first tick).
//
// Setup MAG_007_MEGA_PHOENIX 0x8CE3D0 (runs once, not ported; file loader MAG_007_MEGA_PHOENIX_FL 0x8CE3B0 =
// the effect's data file 0x163E29C -> [0x275FB80], TIM uploaded by the setup unless the cast context's flag +1
// bit 0, its packet arenas at file + 0 / + 0xA000 (second arena word + 0xA000 / + 0x14000), file arena cursor
// 0x2760B5C = file + 0x14000) creates the root queue 0x275FC70 with the master and three task pools: 0x27609C0
// (4 x 0x58: emitters), 0x2760720 (4 x 0x2A4: actor systems), 0x27609B0 (10 x 0x40: stage fade).
//   Master (0x8CE4F0 = Full-life's 0x883240) - camera copy 0x2793E58 (the module scratch stack 0x2760B50 grows
//     down from it), packet arena by tick parity (cursor 0x275FB84), bone follow, 11-state table: 0x8CE650
//     reserves the actor pools after the arena (0xA8C + 0xD200 bytes = 25 particles of 0x6C and 80 actors of
//     0x2A0, cleared by 0x8CE6A0) and starts the stage fade in (0x8CE6E0: level 0x1D98992 + k * 0x2C up to
//     0x400), 0x8CE7D0 one emitter per target of the action (action loop 0x8D4280: next target record +0x2B
//     once the emitter releases the master's +0x63 hold), 0x8D42D0 after 13 more ticks the stage fade out
//     (0x8D4300), then waits for the queues to empty.
//   Emitter (0x8CE800), one per target (each KO'd ally): bone follow + model bounds (CURE_Emitter helpers),
//     sound 0x163ACD0 at its first tick; state 0 spawns the actor system (0x8CE890 -> 0x8CE8C0 with the actor
//     data 0x163E198, length 0x2D ticks), state 1 releases the master at tick 7, state 2 applies the action result to
//     its target (the revive) from tick 10.
//   Actor system (0x8CE910 -> 0x8CE970 / 0x8D41E0): the engine's particle actors (emitters and particles,
//     sprite sequences 0x8D1780 / prim models 0x8CF4C0 drawn into the module arena) placed by the actor data's
//     key positions (caster / its one target), taken once at the director set-up 0x8CE9A0. The prim-model
//     draw advances the actor system's texture scroll counter (state +0x2A: +8, 7-bit wrap) at every draw of
//     an actor of definition index 0.
//   Stage fade (0x8CE6E0 in: 0x8CE740 clears the 4 colour/fade layers of 0x1D9898C, 0x8CE780 level +0x100 per
//     tick to 0x400; 0x8D4300 out: 0x8D4360 level 0x400, 0x8D4380 -0x100 per tick to 0): the level word +6 of
//     each layer (0x1D98992 + k * 0x2C).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x275FB80..0x2760B64 (file pointer 0x275FB80, packet cursor 0x275FB84, pools, queues, arenas,
// actor state 0x2760B60, scratch stack pointer 0x2760B50), the module scratch stack below the camera copy
// 0x2793E58, the fade layers 0x1D98992 + k * 0x2C (words) / 0x1D989B8 + k * 0x2C (bytes).

#include "act_engine.h"
#include "fx_actors.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace act
{
namespace megaphoenix
{
	using namespace actors;

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x275FB84;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x2760B60;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x2760B50;      // module scratch stack pointer
	static const uint32_t Q_EMITTER = 0x27609C0, Q_ACTORS = 0x2760720, Q_STAGE = 0x27609B0;
	static const uint32_t ORIG_Master = 0x8CE4F0;
	static const uint32_t ORIG_Emitter = 0x8CE800;
	static const uint32_t ORIG_StageIn = 0x8CE6E0;
	static const uint32_t ORIG_StageOut = 0x8D4300;
	static const uint32_t ORIG_ActorSystem = 0x8CE910;
	static const uint32_t SOUND = 0x163ACD0;      // the emitter's sound
	static const uint32_t ACTOR_DATA = 0x163E198; // the actor data (header of its 4 tables)

	// ------------------------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------------------------
	enum Part { PART_ACTORS = 1, PART_FIRST = 2 };   // PART_FIRST + k: what definition k draws
	static const int8_t DEF_INDEX[5] = { 0, 1, 2, 3, 5 };   // the actor data's definitions
	struct MegaPhoenixParams
	{
		struct { uint32_t sound; int16_t release, result; } emitter;   // its sound; the master released at this tick; the revive from this one
		struct { int16_t start, length, mode, mask; } actors;         // the actor system: start tick, ticks it runs, mode, definitions skipped
		int16_t wait;                                                 // the master: ticks after the last target's release before the fade out
		struct { int16_t step, level; } fade_in, fade_out;           // the stage fades: level by step a tick up to level (in), from level down to 0 (out)
		DefParams defs[5];                                            // the actor data's definitions (DEF_INDEX), written into it at the master's first tick
	};
	static const MegaPhoenixParams GAME = { { SOUND, 7, 10 }, { 0, 0x2D, 0, 0 }, 13, { 0x100, 0x400 }, { 0x100, 0x400 },
		{ { 0, 0, 0, 0, 0, 0, 0, 1, 0, 27, 1, 0, 0, 5, 0, 0, 1, 1, 3, 0, 0, -512, 0, 1, 26, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4084, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		  { 0, 0, 0, 0, -200, 0, 0, 0, 0, 11, 1, 0, 0, 4, 0, 0, 1, 1, 0, 0, 0, -512, 0, 1, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4084, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		  { 4, 0, 0, 0, 0, 0, 0, 6, 0, 23, 1, 1, 0, 4, 0, 0, 1, 1, 0, 0, 0, -512, 0, 1, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4084, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		  { 4, 1, 0, 0, 0, 0, 0, 4, 0, 22, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, -512, 0, 1, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		  { 4, 0, 0, 0, -200, 0, 0, 0, 0, 34, 4, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, -512, 0, 0, 0, 0, 0, 0, 0, 0, 2048, 4084, 2048, 0, 0, 0, 0, 0, 0, 0, 0, 70, 100, 0, 0, -5, -7, 6553, -3, 0, -3, 3, 0, 3, -3, 0, -3, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };
	static MegaPhoenixParams live = GAME;
	static const MegaPhoenixParams &PM() { return live; }

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
	uint32_t __cdecl mf_8CE4F0(uint32_t a1);
	uint32_t __cdecl mf_8CE650(uint32_t a1);
	uint32_t __cdecl mf_8CE6A0(void);
	uint32_t __cdecl mf_8CE780(uint32_t a1);
	uint32_t __cdecl mf_8CE800(uint32_t a1);
	uint32_t __cdecl mf_8CE890(uint32_t a1);
	uint32_t __cdecl mf_8D4210(uint32_t a1);
	uint32_t __cdecl mf_8D4230(uint32_t a1);
	uint32_t __cdecl mf_8D4280(uint32_t a1);
	uint32_t __cdecl mf_8D42D0(uint32_t a1);
	uint32_t __cdecl mf_8D4360(uint32_t a1);
	uint32_t __cdecl mf_8D4380(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag007_mega_phoenix_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace megaphoenix
{
	using namespace actors;
	// ====================================================================================
	// the module's functions: the text of the matching Full-life port (Mighty Guard / Counter Laser-Eye for the
	// pool set-up / allocator / emitter states / action loop; Psycho Blast for 0x8D33B0, Raldo Throw for the
	// primitive-list dispatcher and list drawers) with this module's addresses and callees and Mega-Potion's
	// variant constants (the comments describe the source original); the director set-up and the prim-model
	// actor draw 0x8CF4C0 are ported by hand
	// ====================================================================================


	// 0x8CE4F0 (MAG_007_sub_8CE4F0): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the three queues (live task count -> node +0x5E; the actor counters
	// 0x2760B4A / 0x2760B3A are cleared before the queues run)
	uint32_t __cdecl mf_8CE4F0(uint32_t a1)
	{
		Use(&MEGA_PHOENIX, &MOD_007);
		if (U16(a1, 0x24) == 0)   // (the edited definitions: written before the actor systems read them)
			for (int s = 0; s < 5; s++) PatchDefinition(MEM<uint32_t>(ACTOR_DATA + 4 + 4 * DEF_INDEX[s]), PM().defs[s]);
		// 30 fps layer: see mag007_mega_phoenix_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x275FBA4) = 0x2793E58;
		states[0] = 0x8CE630;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x8CE640;
		states[2] = 0x8CE650;
		states[3] = 0x8CE7D0;
		states[4] = 0x8D4280;
		states[5] = 0x8D42D0;
		states[6] = 0x8D43D0;
		states[7] = 0x8D43E0;
		states[8] = 0x8D43F0;
		states[9] = 0x8D4400;
		states[10] = 0x8D4420; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2760B34);
			const uint32_t v2 = MEM<uint32_t>(0x275FC8C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x275FBA0) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2760B30);
			const uint32_t v2 = MEM<uint32_t>(0x275FC88);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x275FBA0) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x2760B4A) = 0;
		MEM<uint16_t>(0x2760B3A) = 0;
		static const uint32_t queues[3] = { Q_EMITTER, Q_ACTORS, Q_STAGE };
		for (int i = 0; i < 3; i++)
		{
			if (i == 1) FX_PART(PART_ACTORS);   // (the actor system's queue)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
			if (i == 1) FX_PART(0);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x8CE650 (MAG_007_sub_8CE650): master state 2 - once no child is alive: the two actor pools
	// are carved from the file arena cursor [0x2760B5C] (0xA8C bytes = 25 particles of 0x6C at
	// [0x2760B3C], 0xD200 bytes = 80 actors of 0x2A0 at [0x2760B54]) and cleared (0x8CE6A0), the
	// stage fade task 0x8CE6E0 (0x40 bytes, stage queue) starts, next state
	uint32_t __cdecl mf_8CE650(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x2760B5C);
		MEM<uint32_t>(0x2760B3C) = p;
		p += 0xA8C;
		MEM<uint32_t>(0x2760B54) = p;
		p += 0xD200;
		MEM<uint32_t>(0x2760B5C) = p;
		mf_8CE6A0();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, ORIG_StageIn, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8CE6A0 (MAG_007_sub_8CE6A0): clears the two actor pools (0xA8C bytes at [0x2760B3C], 0xD200
	// at [0x2760B54]) and the pool cursors / counters
	uint32_t __cdecl mf_8CE6A0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2760B3C), 0xA8C);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2760B54), 0xD200);
		MEM<uint16_t>(0x275FC80) = 0;
		MEM<uint16_t>(0x2760B48) = 0;
		MEM<uint16_t>(0x2760B38) = 0;
		MEM<uint16_t>(0x275FB88) = 0;
		return 0; // void
	}


	// ====================================================================================
	// the library functions of this module whose code matches Drain's / an engine port's up to
	// the module's addresses and callees (the port's text with the module's addresses)
	// ====================================================================================
	// 0x8CE780 (MAG_007_sub_8CE780), written to the four stage fade words 0x1D98992 + k * 0x2C
	uint32_t __cdecl mf_8CE780(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + PM().fade_in.step);
		if (S16(a1, 0x1C) >= PM().fade_in.level)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = (uint16_t)PM().fade_in.level;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}


	// 0x8CE800 (MAG_007_sub_8CE800): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x8CE890 actor system, 0x8D4210 release the master, 0x8D4230 action result,
	// ret}, sound at its first tick
	uint32_t __cdecl mf_8CE800(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x8CE890;
		states[1] = 0x8D4210;
		states[2] = 0x8D4230;
		states[3] = 0x8D4270; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(fxp::sound(PM().emitter.sound, SOUND), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x8CE890 (MAG_007_sub_8CE890): emitter state 0 - the actor system (0x8CE910, actor data
	// 0x163E198, start tick 0, length 0x2D ticks) through the engine spawner 0x8CE8C0, next state
	uint32_t __cdecl mf_8CE890(uint32_t a1)
	{
		const uint32_t n = a_73C100(a1, ORIG_ActorSystem, ACTOR_DATA, (uint16_t)PM().actors.start, (uint16_t)PM().actors.length,
		                            (uint16_t)PM().actors.mode);
		U16(n, 0x29A) = (uint16_t)PM().actors.mask;   // (the game's: 0, as the node is made)
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8D4210 (MAG_007_sub_8D4210): emitter state 1 - at tick 7 releases the master (root +0x63
	// = 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl mf_8D4210(uint32_t a1)
	{
		if (S16(a1, 0x24) == PM().emitter.release)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}


	// 0x8D4230 (MAG_007_sub_8D4230): emitter state 2 - from tick 10: the action result (the heal) of the
	// action's current target (+0x2A action, +0x2B target record of 24 bytes), finished, next state
	uint32_t __cdecl mf_8D4230(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < PM().emitter.result)
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


	// 0x8D4280 (MAG_007_sub_8D4280): master state 4 - action loop: once the emitter released the
	// master (+0x63 = 0): the next target record (+0x2B) and action counter +0x2E, back to the
	// emitter state; after the last one: wait 13 ticks (+0x60), next state
	uint32_t __cdecl mf_8D4280(uint32_t a1)
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
		U16(node, 0x60) = (uint16_t)PM().wait;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return node;
	}


	// 0x8D42D0 (MAG_007_sub_8D42D0): master state - counts +0x60 down; at 0 starts the stage fade
	// out (0x8D4300, 0x40 bytes, stage queue), next state
	uint32_t __cdecl mf_8D42D0(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8D4300, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8D4360 (MAG_007_sub_8D4360): stage fade out state 0 - level +0x1C = 0x400, next state
	uint32_t __cdecl mf_8D4360(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = (uint16_t)PM().fade_out.level;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}


	// 0x8D4380 (MAG_007_sub_8D4380, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl mf_8D4380(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) - PM().fade_out.step);
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

	namespace table
	{
		using fxp::Field;
#define MP_FIELD(part, member, label, doc, type, lo, hi) { part, #member, label, doc, (uint32_t)offsetof(MegaPhoenixParams, member), fxp::type, lo, hi }
#define MP_I16(part, member, label, doc, lo, hi) MP_FIELD(part, member, label, doc, I16, lo, hi)
		static const fxp::Part PARTS[] = {
			{ 0, "emitter", "Emitter", "One per target of the action, in turn: the actor system, the master's release, the revive, its sound (draws nothing)" },
			{ PART_ACTORS, "actors", "Actors", "The actor system: the engine's particle actors its actor data defines, placed by the caster and the target" },
			{ 0, "fades", "Stage fades", "The stage's fade layers: up while the effect plays, down after (draws nothing itself)" },
			{ PART_FIRST + 0, "actor0", "Actor 0", "Definition 0 of the actor data: the morphing ring model (its emitter and its particles)" },
			{ PART_FIRST + 1, "actor1", "Actor 1", "Definition 1 of the actor data: the rising ring models (its emitter and its particles)" },
			{ PART_FIRST + 2, "actor2", "Actor 2", "Definition 2 of the actor data: the rings climbing the path (its emitter and its particles)" },
			{ PART_FIRST + 3, "actor3", "Actor 3", "Definition 3 of the actor data: the sparkles up to the target (its emitter and its particles)" },
			{ PART_FIRST + 5, "actor5", "Actor 5", "Definition 5 of the actor data: the sparks bursting up (its emitter and its particles)" },
		};
		static const fxp::Group GROUPS[] = {
			{ "fade_in", "Fade in", "Once the master's children are gone: the level up by Step a tick, to Level" },
			{ "fade_out", "Fade out", "After the last target: from Level down by Step a tick, to 0" },
			{ "actor0.offset", "Offset", "From its point, world units, turned by its matrix" },
			{ "actor0.loop", "Loop", "Kind 2: frames First to Last looped..." },
			{ "actor0.loops", "Loops", "...a random number of times in [Min, Max)" },
			{ "actor0.timer", "Timer", "Its life in frames: at random in [Min, Max)" },
			{ "actor0.dir", "Direction", "Each particle's speed direction (angles X, Y, Z)" },
			{ "actor0.dir_jitter", "Direction jitter", "Each angle up or down by half of this at random" },
			{ "actor0.angle", "Angles", "The record's angles X, Y, Z; its per-frame angle tables added" },
			{ "actor0.angle_jitter", "Angles jitter", "Each up or down by half of this at random" },
			{ "actor0.speed", "Speed", "Along its direction, world units a frame: at random in [Min, Max)" },
			{ "actor0.speed_step", "Speed change", "Added to its speed every frame: at random in [Min, Max)" },
			{ "actor0.gravity", "Gravity", "Added to its velocity's height every frame: at random in [Min, Max)" },
			{ "actor0.velocity", "Velocity", "World units a frame on each axis: at random from Min to Max" },
			{ "actor0.accel", "Acceleration", "Added to its velocity every frame: at random from Min to Max" },
			{ "actor0.accel_step", "Acceleration change", "Added to its acceleration every frame: at random from Min to Max" },
			{ "actor0.sub", "Sub-emitters", "When a record starts: an emitter of each definition whose On is 1" },
			{ "actor1.offset", "Offset", "From its point, world units, turned by its matrix" },
			{ "actor1.loop", "Loop", "Kind 2: frames First to Last looped..." },
			{ "actor1.loops", "Loops", "...a random number of times in [Min, Max)" },
			{ "actor1.timer", "Timer", "Its life in frames: at random in [Min, Max)" },
			{ "actor1.dir", "Direction", "Each particle's speed direction (angles X, Y, Z)" },
			{ "actor1.dir_jitter", "Direction jitter", "Each angle up or down by half of this at random" },
			{ "actor1.angle", "Angles", "The record's angles X, Y, Z; its per-frame angle tables added" },
			{ "actor1.angle_jitter", "Angles jitter", "Each up or down by half of this at random" },
			{ "actor1.speed", "Speed", "Along its direction, world units a frame: at random in [Min, Max)" },
			{ "actor1.speed_step", "Speed change", "Added to its speed every frame: at random in [Min, Max)" },
			{ "actor1.gravity", "Gravity", "Added to its velocity's height every frame: at random in [Min, Max)" },
			{ "actor1.velocity", "Velocity", "World units a frame on each axis: at random from Min to Max" },
			{ "actor1.accel", "Acceleration", "Added to its velocity every frame: at random from Min to Max" },
			{ "actor1.accel_step", "Acceleration change", "Added to its acceleration every frame: at random from Min to Max" },
			{ "actor1.sub", "Sub-emitters", "When a record starts: an emitter of each definition whose On is 1" },
			{ "actor2.offset", "Offset", "From its point, world units, turned by its matrix" },
			{ "actor2.loop", "Loop", "Kind 2: frames First to Last looped..." },
			{ "actor2.loops", "Loops", "...a random number of times in [Min, Max)" },
			{ "actor2.timer", "Timer", "Its life in frames: at random in [Min, Max)" },
			{ "actor2.dir", "Direction", "Each particle's speed direction (angles X, Y, Z)" },
			{ "actor2.dir_jitter", "Direction jitter", "Each angle up or down by half of this at random" },
			{ "actor2.angle", "Angles", "The record's angles X, Y, Z; its per-frame angle tables added" },
			{ "actor2.angle_jitter", "Angles jitter", "Each up or down by half of this at random" },
			{ "actor2.speed", "Speed", "Along its direction, world units a frame: at random in [Min, Max)" },
			{ "actor2.speed_step", "Speed change", "Added to its speed every frame: at random in [Min, Max)" },
			{ "actor2.gravity", "Gravity", "Added to its velocity's height every frame: at random in [Min, Max)" },
			{ "actor2.velocity", "Velocity", "World units a frame on each axis: at random from Min to Max" },
			{ "actor2.accel", "Acceleration", "Added to its velocity every frame: at random from Min to Max" },
			{ "actor2.accel_step", "Acceleration change", "Added to its acceleration every frame: at random from Min to Max" },
			{ "actor2.sub", "Sub-emitters", "When a record starts: an emitter of each definition whose On is 1" },
			{ "actor3.offset", "Offset", "From its point, world units, turned by its matrix" },
			{ "actor3.loop", "Loop", "Kind 2: frames First to Last looped..." },
			{ "actor3.loops", "Loops", "...a random number of times in [Min, Max)" },
			{ "actor3.timer", "Timer", "Its life in frames: at random in [Min, Max)" },
			{ "actor3.dir", "Direction", "Each particle's speed direction (angles X, Y, Z)" },
			{ "actor3.dir_jitter", "Direction jitter", "Each angle up or down by half of this at random" },
			{ "actor3.angle", "Angles", "The record's angles X, Y, Z; its per-frame angle tables added" },
			{ "actor3.angle_jitter", "Angles jitter", "Each up or down by half of this at random" },
			{ "actor3.speed", "Speed", "Along its direction, world units a frame: at random in [Min, Max)" },
			{ "actor3.speed_step", "Speed change", "Added to its speed every frame: at random in [Min, Max)" },
			{ "actor3.gravity", "Gravity", "Added to its velocity's height every frame: at random in [Min, Max)" },
			{ "actor3.velocity", "Velocity", "World units a frame on each axis: at random from Min to Max" },
			{ "actor3.accel", "Acceleration", "Added to its velocity every frame: at random from Min to Max" },
			{ "actor3.accel_step", "Acceleration change", "Added to its acceleration every frame: at random from Min to Max" },
			{ "actor3.sub", "Sub-emitters", "When a record starts: an emitter of each definition whose On is 1" },
			{ "actor5.offset", "Offset", "From its point, world units, turned by its matrix" },
			{ "actor5.loop", "Loop", "Kind 2: frames First to Last looped..." },
			{ "actor5.loops", "Loops", "...a random number of times in [Min, Max)" },
			{ "actor5.timer", "Timer", "Its life in frames: at random in [Min, Max)" },
			{ "actor5.dir", "Direction", "Each particle's speed direction (angles X, Y, Z)" },
			{ "actor5.dir_jitter", "Direction jitter", "Each angle up or down by half of this at random" },
			{ "actor5.angle", "Angles", "The record's angles X, Y, Z; its per-frame angle tables added" },
			{ "actor5.angle_jitter", "Angles jitter", "Each up or down by half of this at random" },
			{ "actor5.speed", "Speed", "Along its direction, world units a frame: at random in [Min, Max)" },
			{ "actor5.speed_step", "Speed change", "Added to its speed every frame: at random in [Min, Max)" },
			{ "actor5.gravity", "Gravity", "Added to its velocity's height every frame: at random in [Min, Max)" },
			{ "actor5.velocity", "Velocity", "World units a frame on each axis: at random from Min to Max" },
			{ "actor5.accel", "Acceleration", "Added to its velocity every frame: at random from Min to Max" },
			{ "actor5.accel_step", "Acceleration change", "Added to its acceleration every frame: at random from Min to Max" },
			{ "actor5.sub", "Sub-emitters", "When a record starts: an emitter of each definition whose On is 1" },
		};
		static const Field FIELDS[] = {
			MP_FIELD("emitter", emitter.sound, "Sound", "The sound block played at its first tick (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			MP_I16("emitter", emitter.release, "Next target at tick", "The master starts the next target's emitter when this one's tick == this", 0, 200),
			MP_I16("emitter", emitter.result, "Revive from tick", "Its target revived (the action's result) from this tick", 0, 200),
			MP_I16("actors", actors.start, "Start tick", "The actor system starts when its own tick reaches this", 0, 200),
			MP_I16("actors", actors.length, "Ticks", "It spawns its actors for this many ticks, then ends once they are gone", 0, 1000),
			MP_I16("actors", actors.mode, "Mode", "Where it takes its reference points: 0 the caster's bones, 2 none, 1 / 3 / 4 their extra set-ups", 0, 4),
			MP_I16("actors", actors.mask, "Skipped definitions", "Bit k set: definition k spawns nothing", 0, 0xFFFF),
			MP_I16("fades", wait, "Fade out after", "Ticks after the last target's emitter released the master", 0, 200),
			MP_I16("fades", fade_in.step, "Step", "", 0, 0x400),
			MP_I16("fades", fade_in.level, "Level", "", 0, 0x1000),
			MP_I16("fades", fade_out.level, "Level", "", 0, 0x1000),
			MP_I16("fades", fade_out.step, "Step", "", 0, 0x400),
			{ "actor0", "actor0.placement", "Placement", "Where its emitter sits: 0 a point of its target, 1 a point of the caster, 2 its owner's position, 3 following its owner, 4 along a path (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, placement)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor0", "actor0.point", "Point", "Placements 0 / 1: 0 the ground under it, 1 its bone 0xF1, 2 its bone 0xF0, 3 its height point", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, point)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor0", "actor0.facing", "Facing", "Its emitter's matrix: 0 none, 1 the caster-to-target yaw, 2 the reverse, 3 its owner's, 4 the caster's yaw", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, facing)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor0", "actor0.offset.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, offset_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.offset.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, offset_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.offset.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, offset_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.between", "Between positions", "1: at the midpoint of its owner's position and the previous one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, between)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.tick", "Spawn tick", "Its emitter starts when the actor system's tick == this", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, tick)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.per_target", "Per target", "1: an emitter for each target, else one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, per_target)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.frames", "Emitter frames", "Its emitter lives this many frames (its per-frame tables: how many it emits, where)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, frames)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.batch", "Batch", "Particles a record holds (1..4): what a frame emits, in records of this many", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, batch)), fxp::U8, 1, 4, fxp::Plain },
			{ "actor0", "actor0.spread", "Spread", "Where they start round the emitter: 0 on it, 1 a random sphere, 2 a ring (its per-frame radii)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, spread)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor0", "actor0.exact", "Exact spread", "A sphere: 1 at its radii exactly, 0 at random within; a ring: 1 only the height at random", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, exact)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.kind", "Kind", "0 a flipbook played once, 1 looping, 2 looping some times, 3 nothing, 4 a prim model (its scale and fade tables), 5 a morphing prim model", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, kind)), fxp::U8, 0, 5, fxp::Plain },
			{ "actor0", "actor0.loop.first", "First", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, loop_first)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor0", "actor0.loop.last", "Last", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, loop_last)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor0", "actor0.loops.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, loops_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.loops.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, loops_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.orientation", "Orientation", "0 facing the camera with its roll, 1 flat on the ground, 2 the camera's yaw only, 3 / 4 its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, orientation)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor0", "actor0.rotation", "Rotation", "Its angles from: 0 / 1 its owner's matrix (or none), Z X Y / Y X Z; 2 / 3 the camera's, Z X Y / Y X Z", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, rotation)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor0", "actor0.turn_offsets", "Turned offsets", "1: its offsets turned by its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, turn_offsets)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.depth", "Depth bias", "Added to its depth once projected (negative: drawn nearer)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, depth)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.one_sided", "One-sided", "Prim models: 0 drawn both sides, 1 its back faces culled", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, one_sided)), fxp::I16, 0, 1, fxp::Plain },
			{ "actor0", "actor0.end", "End", "A record ends: 0 when its flipbook is done, 1 when its timer runs out, 2 either", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, end)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor0", "actor0.timer.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, timer_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.timer.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, timer_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor0", "actor0.angle_from", "Directions", "Each particle's direction: 0 Direction with its jitter, 1 the spread's", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_from)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.dir.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.dir.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.dir.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.dir_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.dir_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.dir_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.angle_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor0", "actor0.motion", "Motion", "0 physics (speed, velocity, acceleration, gravity), 1 its keyed offsets (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, motion)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.relative", "Follows its emitter", "1: placed from its emitter every frame, else left behind", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, relative)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.speed.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, speed_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.speed.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, speed_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.speed_step.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, speed_step_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.speed_step.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, speed_step_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.gravity.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, gravity_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.gravity.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, gravity_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.damping", "Damping", "Its speed and velocity lose this / 65536 every frame", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, damping)), fxp::I16, 0, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.velocity.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, velocity_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.accel_step.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor0", "actor0.follow", "Follow-up", "1: when a record ends, an emitter of definition Follow-up definition where it was", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, follow)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.follow_def", "Follow-up definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, follow_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor0", "actor0.sub[0].on", "1: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_0_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.sub[0].def", "1: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_0_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor0", "actor0.sub[1].on", "2: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_1_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.sub[1].def", "2: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_1_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor0", "actor0.sub[2].on", "3: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_2_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.sub[2].def", "3: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_2_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor0", "actor0.sub[3].on", "4: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_3_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor0", "actor0.sub[3].def", "4: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 0 * sizeof(DefParams) + offsetof(DefParams, sub_3_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor1", "actor1.placement", "Placement", "Where its emitter sits: 0 a point of its target, 1 a point of the caster, 2 its owner's position, 3 following its owner, 4 along a path (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, placement)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor1", "actor1.point", "Point", "Placements 0 / 1: 0 the ground under it, 1 its bone 0xF1, 2 its bone 0xF0, 3 its height point", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, point)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor1", "actor1.facing", "Facing", "Its emitter's matrix: 0 none, 1 the caster-to-target yaw, 2 the reverse, 3 its owner's, 4 the caster's yaw", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, facing)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor1", "actor1.offset.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, offset_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.offset.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, offset_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.offset.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, offset_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.between", "Between positions", "1: at the midpoint of its owner's position and the previous one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, between)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.tick", "Spawn tick", "Its emitter starts when the actor system's tick == this", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, tick)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.per_target", "Per target", "1: an emitter for each target, else one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, per_target)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.frames", "Emitter frames", "Its emitter lives this many frames (its per-frame tables: how many it emits, where)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, frames)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.batch", "Batch", "Particles a record holds (1..4): what a frame emits, in records of this many", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, batch)), fxp::U8, 1, 4, fxp::Plain },
			{ "actor1", "actor1.spread", "Spread", "Where they start round the emitter: 0 on it, 1 a random sphere, 2 a ring (its per-frame radii)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, spread)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor1", "actor1.exact", "Exact spread", "A sphere: 1 at its radii exactly, 0 at random within; a ring: 1 only the height at random", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, exact)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.kind", "Kind", "0 a flipbook played once, 1 looping, 2 looping some times, 3 nothing, 4 a prim model (its scale and fade tables), 5 a morphing prim model", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, kind)), fxp::U8, 0, 5, fxp::Plain },
			{ "actor1", "actor1.loop.first", "First", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, loop_first)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor1", "actor1.loop.last", "Last", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, loop_last)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor1", "actor1.loops.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, loops_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.loops.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, loops_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.orientation", "Orientation", "0 facing the camera with its roll, 1 flat on the ground, 2 the camera's yaw only, 3 / 4 its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, orientation)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor1", "actor1.rotation", "Rotation", "Its angles from: 0 / 1 its owner's matrix (or none), Z X Y / Y X Z; 2 / 3 the camera's, Z X Y / Y X Z", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, rotation)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor1", "actor1.turn_offsets", "Turned offsets", "1: its offsets turned by its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, turn_offsets)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.depth", "Depth bias", "Added to its depth once projected (negative: drawn nearer)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, depth)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.one_sided", "One-sided", "Prim models: 0 drawn both sides, 1 its back faces culled", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, one_sided)), fxp::I16, 0, 1, fxp::Plain },
			{ "actor1", "actor1.end", "End", "A record ends: 0 when its flipbook is done, 1 when its timer runs out, 2 either", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, end)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor1", "actor1.timer.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, timer_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.timer.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, timer_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor1", "actor1.angle_from", "Directions", "Each particle's direction: 0 Direction with its jitter, 1 the spread's", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_from)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.dir.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.dir.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.dir.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.dir_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.dir_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.dir_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.angle_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor1", "actor1.motion", "Motion", "0 physics (speed, velocity, acceleration, gravity), 1 its keyed offsets (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, motion)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.relative", "Follows its emitter", "1: placed from its emitter every frame, else left behind", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, relative)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.speed.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, speed_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.speed.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, speed_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.speed_step.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, speed_step_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.speed_step.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, speed_step_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.gravity.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, gravity_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.gravity.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, gravity_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.damping", "Damping", "Its speed and velocity lose this / 65536 every frame", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, damping)), fxp::I16, 0, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.velocity.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, velocity_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.accel_step.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor1", "actor1.follow", "Follow-up", "1: when a record ends, an emitter of definition Follow-up definition where it was", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, follow)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.follow_def", "Follow-up definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, follow_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor1", "actor1.sub[0].on", "1: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_0_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.sub[0].def", "1: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_0_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor1", "actor1.sub[1].on", "2: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_1_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.sub[1].def", "2: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_1_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor1", "actor1.sub[2].on", "3: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_2_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.sub[2].def", "3: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_2_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor1", "actor1.sub[3].on", "4: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_3_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor1", "actor1.sub[3].def", "4: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 1 * sizeof(DefParams) + offsetof(DefParams, sub_3_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor2", "actor2.placement", "Placement", "Where its emitter sits: 0 a point of its target, 1 a point of the caster, 2 its owner's position, 3 following its owner, 4 along a path (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, placement)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor2", "actor2.point", "Point", "Placements 0 / 1: 0 the ground under it, 1 its bone 0xF1, 2 its bone 0xF0, 3 its height point", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, point)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor2", "actor2.facing", "Facing", "Its emitter's matrix: 0 none, 1 the caster-to-target yaw, 2 the reverse, 3 its owner's, 4 the caster's yaw", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, facing)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor2", "actor2.offset.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, offset_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.offset.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, offset_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.offset.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, offset_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.between", "Between positions", "1: at the midpoint of its owner's position and the previous one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, between)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.tick", "Spawn tick", "Its emitter starts when the actor system's tick == this", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, tick)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.per_target", "Per target", "1: an emitter for each target, else one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, per_target)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.frames", "Emitter frames", "Its emitter lives this many frames (its per-frame tables: how many it emits, where)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, frames)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.batch", "Batch", "Particles a record holds (1..4): what a frame emits, in records of this many", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, batch)), fxp::U8, 1, 4, fxp::Plain },
			{ "actor2", "actor2.spread", "Spread", "Where they start round the emitter: 0 on it, 1 a random sphere, 2 a ring (its per-frame radii)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, spread)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor2", "actor2.exact", "Exact spread", "A sphere: 1 at its radii exactly, 0 at random within; a ring: 1 only the height at random", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, exact)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.kind", "Kind", "0 a flipbook played once, 1 looping, 2 looping some times, 3 nothing, 4 a prim model (its scale and fade tables), 5 a morphing prim model", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, kind)), fxp::U8, 0, 5, fxp::Plain },
			{ "actor2", "actor2.loop.first", "First", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, loop_first)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor2", "actor2.loop.last", "Last", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, loop_last)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor2", "actor2.loops.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, loops_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.loops.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, loops_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.orientation", "Orientation", "0 facing the camera with its roll, 1 flat on the ground, 2 the camera's yaw only, 3 / 4 its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, orientation)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor2", "actor2.rotation", "Rotation", "Its angles from: 0 / 1 its owner's matrix (or none), Z X Y / Y X Z; 2 / 3 the camera's, Z X Y / Y X Z", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, rotation)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor2", "actor2.turn_offsets", "Turned offsets", "1: its offsets turned by its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, turn_offsets)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.depth", "Depth bias", "Added to its depth once projected (negative: drawn nearer)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, depth)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.one_sided", "One-sided", "Prim models: 0 drawn both sides, 1 its back faces culled", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, one_sided)), fxp::I16, 0, 1, fxp::Plain },
			{ "actor2", "actor2.end", "End", "A record ends: 0 when its flipbook is done, 1 when its timer runs out, 2 either", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, end)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor2", "actor2.timer.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, timer_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.timer.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, timer_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor2", "actor2.angle_from", "Directions", "Each particle's direction: 0 Direction with its jitter, 1 the spread's", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_from)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.dir.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.dir.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.dir.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.dir_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.dir_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.dir_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.angle_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor2", "actor2.motion", "Motion", "0 physics (speed, velocity, acceleration, gravity), 1 its keyed offsets (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, motion)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.relative", "Follows its emitter", "1: placed from its emitter every frame, else left behind", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, relative)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.speed.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, speed_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.speed.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, speed_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.speed_step.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, speed_step_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.speed_step.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, speed_step_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.gravity.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, gravity_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.gravity.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, gravity_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.damping", "Damping", "Its speed and velocity lose this / 65536 every frame", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, damping)), fxp::I16, 0, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.velocity.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, velocity_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.accel_step.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor2", "actor2.follow", "Follow-up", "1: when a record ends, an emitter of definition Follow-up definition where it was", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, follow)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.follow_def", "Follow-up definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, follow_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor2", "actor2.sub[0].on", "1: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_0_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.sub[0].def", "1: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_0_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor2", "actor2.sub[1].on", "2: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_1_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.sub[1].def", "2: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_1_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor2", "actor2.sub[2].on", "3: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_2_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.sub[2].def", "3: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_2_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor2", "actor2.sub[3].on", "4: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_3_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor2", "actor2.sub[3].def", "4: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 2 * sizeof(DefParams) + offsetof(DefParams, sub_3_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor3", "actor3.placement", "Placement", "Where its emitter sits: 0 a point of its target, 1 a point of the caster, 2 its owner's position, 3 following its owner, 4 along a path (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, placement)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor3", "actor3.point", "Point", "Placements 0 / 1: 0 the ground under it, 1 its bone 0xF1, 2 its bone 0xF0, 3 its height point", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, point)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor3", "actor3.facing", "Facing", "Its emitter's matrix: 0 none, 1 the caster-to-target yaw, 2 the reverse, 3 its owner's, 4 the caster's yaw", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, facing)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor3", "actor3.offset.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, offset_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.offset.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, offset_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.offset.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, offset_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.between", "Between positions", "1: at the midpoint of its owner's position and the previous one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, between)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.tick", "Spawn tick", "Its emitter starts when the actor system's tick == this", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, tick)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.per_target", "Per target", "1: an emitter for each target, else one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, per_target)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.frames", "Emitter frames", "Its emitter lives this many frames (its per-frame tables: how many it emits, where)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, frames)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.batch", "Batch", "Particles a record holds (1..4): what a frame emits, in records of this many", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, batch)), fxp::U8, 1, 4, fxp::Plain },
			{ "actor3", "actor3.spread", "Spread", "Where they start round the emitter: 0 on it, 1 a random sphere, 2 a ring (its per-frame radii)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, spread)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor3", "actor3.exact", "Exact spread", "A sphere: 1 at its radii exactly, 0 at random within; a ring: 1 only the height at random", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, exact)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.kind", "Kind", "0 a flipbook played once, 1 looping, 2 looping some times, 3 nothing, 4 a prim model (its scale and fade tables), 5 a morphing prim model", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, kind)), fxp::U8, 0, 5, fxp::Plain },
			{ "actor3", "actor3.loop.first", "First", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, loop_first)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor3", "actor3.loop.last", "Last", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, loop_last)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor3", "actor3.loops.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, loops_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.loops.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, loops_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.orientation", "Orientation", "0 facing the camera with its roll, 1 flat on the ground, 2 the camera's yaw only, 3 / 4 its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, orientation)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor3", "actor3.rotation", "Rotation", "Its angles from: 0 / 1 its owner's matrix (or none), Z X Y / Y X Z; 2 / 3 the camera's, Z X Y / Y X Z", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, rotation)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor3", "actor3.turn_offsets", "Turned offsets", "1: its offsets turned by its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, turn_offsets)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.depth", "Depth bias", "Added to its depth once projected (negative: drawn nearer)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, depth)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.one_sided", "One-sided", "Prim models: 0 drawn both sides, 1 its back faces culled", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, one_sided)), fxp::I16, 0, 1, fxp::Plain },
			{ "actor3", "actor3.end", "End", "A record ends: 0 when its flipbook is done, 1 when its timer runs out, 2 either", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, end)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor3", "actor3.timer.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, timer_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.timer.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, timer_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor3", "actor3.angle_from", "Directions", "Each particle's direction: 0 Direction with its jitter, 1 the spread's", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_from)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.dir.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.dir.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.dir.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.dir_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.dir_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.dir_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.angle_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor3", "actor3.motion", "Motion", "0 physics (speed, velocity, acceleration, gravity), 1 its keyed offsets (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, motion)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.relative", "Follows its emitter", "1: placed from its emitter every frame, else left behind", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, relative)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.speed.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, speed_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.speed.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, speed_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.speed_step.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, speed_step_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.speed_step.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, speed_step_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.gravity.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, gravity_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.gravity.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, gravity_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.damping", "Damping", "Its speed and velocity lose this / 65536 every frame", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, damping)), fxp::I16, 0, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.velocity.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, velocity_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.accel_step.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor3", "actor3.follow", "Follow-up", "1: when a record ends, an emitter of definition Follow-up definition where it was", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, follow)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.follow_def", "Follow-up definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, follow_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor3", "actor3.sub[0].on", "1: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_0_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.sub[0].def", "1: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_0_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor3", "actor3.sub[1].on", "2: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_1_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.sub[1].def", "2: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_1_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor3", "actor3.sub[2].on", "3: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_2_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.sub[2].def", "3: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_2_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor3", "actor3.sub[3].on", "4: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_3_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor3", "actor3.sub[3].def", "4: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 3 * sizeof(DefParams) + offsetof(DefParams, sub_3_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor5", "actor5.placement", "Placement", "Where its emitter sits: 0 a point of its target, 1 a point of the caster, 2 its owner's position, 3 following its owner, 4 along a path (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, placement)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor5", "actor5.point", "Point", "Placements 0 / 1: 0 the ground under it, 1 its bone 0xF1, 2 its bone 0xF0, 3 its height point", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, point)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor5", "actor5.facing", "Facing", "Its emitter's matrix: 0 none, 1 the caster-to-target yaw, 2 the reverse, 3 its owner's, 4 the caster's yaw", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, facing)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor5", "actor5.offset.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, offset_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.offset.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, offset_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.offset.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, offset_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.between", "Between positions", "1: at the midpoint of its owner's position and the previous one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, between)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.tick", "Spawn tick", "Its emitter starts when the actor system's tick == this", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, tick)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.per_target", "Per target", "1: an emitter for each target, else one", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, per_target)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.frames", "Emitter frames", "Its emitter lives this many frames (its per-frame tables: how many it emits, where)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, frames)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.batch", "Batch", "Particles a record holds (1..4): what a frame emits, in records of this many", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, batch)), fxp::U8, 1, 4, fxp::Plain },
			{ "actor5", "actor5.spread", "Spread", "Where they start round the emitter: 0 on it, 1 a random sphere, 2 a ring (its per-frame radii)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, spread)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor5", "actor5.exact", "Exact spread", "A sphere: 1 at its radii exactly, 0 at random within; a ring: 1 only the height at random", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, exact)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.kind", "Kind", "0 a flipbook played once, 1 looping, 2 looping some times, 3 nothing, 4 a prim model (its scale and fade tables), 5 a morphing prim model", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, kind)), fxp::U8, 0, 5, fxp::Plain },
			{ "actor5", "actor5.loop.first", "First", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, loop_first)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor5", "actor5.loop.last", "Last", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, loop_last)), fxp::U8, 0, 255, fxp::Plain },
			{ "actor5", "actor5.loops.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, loops_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.loops.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, loops_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.orientation", "Orientation", "0 facing the camera with its roll, 1 flat on the ground, 2 the camera's yaw only, 3 / 4 its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, orientation)), fxp::U8, 0, 4, fxp::Plain },
			{ "actor5", "actor5.rotation", "Rotation", "Its angles from: 0 / 1 its owner's matrix (or none), Z X Y / Y X Z; 2 / 3 the camera's, Z X Y / Y X Z", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, rotation)), fxp::U8, 0, 3, fxp::Plain },
			{ "actor5", "actor5.turn_offsets", "Turned offsets", "1: its offsets turned by its angles", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, turn_offsets)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.depth", "Depth bias", "Added to its depth once projected (negative: drawn nearer)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, depth)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.one_sided", "One-sided", "Prim models: 0 drawn both sides, 1 its back faces culled", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, one_sided)), fxp::I16, 0, 1, fxp::Plain },
			{ "actor5", "actor5.end", "End", "A record ends: 0 when its flipbook is done, 1 when its timer runs out, 2 either", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, end)), fxp::U8, 0, 2, fxp::Plain },
			{ "actor5", "actor5.timer.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, timer_min)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.timer.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, timer_max)), fxp::U8, 0, 127, fxp::Plain },
			{ "actor5", "actor5.angle_from", "Directions", "Each particle's direction: 0 Direction with its jitter, 1 the spread's", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_from)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.dir.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.dir.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.dir.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.dir_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.dir_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.dir_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, dir_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle_jitter.x", "X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_x)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle_jitter.y", "Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_y)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.angle_jitter.z", "Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, angle_jitter_z)), fxp::I16, 0, 4095, fxp::Angle },
			{ "actor5", "actor5.motion", "Motion", "0 physics (speed, velocity, acceleration, gravity), 1 its keyed offsets (its per-frame tables)", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, motion)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.relative", "Follows its emitter", "1: placed from its emitter every frame, else left behind", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, relative)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.speed.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, speed_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.speed.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, speed_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.speed_step.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, speed_step_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.speed_step.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, speed_step_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.gravity.min", "Min", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, gravity_min)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.gravity.max", "Max", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, gravity_max)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.damping", "Damping", "Its speed and velocity lose this / 65536 every frame", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, damping)), fxp::I16, 0, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.velocity.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, velocity_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.min.x", "Min X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.min.y", "Min Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.min.z", "Min Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_min_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.max.x", "Max X", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_x)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.max.y", "Max Y", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_y)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.accel_step.max.z", "Max Z", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, accel_step_max_z)), fxp::I16, -32768, 32767, fxp::Plain },
			{ "actor5", "actor5.follow", "Follow-up", "1: when a record ends, an emitter of definition Follow-up definition where it was", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, follow)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.follow_def", "Follow-up definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, follow_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor5", "actor5.sub[0].on", "1: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_0_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.sub[0].def", "1: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_0_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor5", "actor5.sub[1].on", "2: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_1_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.sub[1].def", "2: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_1_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor5", "actor5.sub[2].on", "3: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_2_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.sub[2].def", "3: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_2_def)), fxp::U8, 0, 15, fxp::Plain },
			{ "actor5", "actor5.sub[3].on", "4: On", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_3_on)), fxp::U8, 0, 1, fxp::Plain },
			{ "actor5", "actor5.sub[3].def", "4: Definition", "", (uint32_t)(offsetof(MegaPhoenixParams, defs) + 4 * sizeof(DefParams) + offsetof(DefParams, sub_3_def)), fxp::U8, 0, 15, fxp::Plain },
		};
#undef MP_I16
#undef MP_FIELD
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8CE630, (void *)a_73A0D0, "007 MAG_007_sub_8CE630" },
		{ 0x8CE640, (void *)a_73A0D0, "007 MAG_007_sub_8CE640" },
		{ 0x8CE6E0, (void *)a_73A380, "007 MAG_007_sub_8CE6E0" },
		{ 0x8CE740, (void *)a_7335D0, "007 MAG_007_sub_8CE740" },
		{ 0x8CE7C0, (void *)a_73A6D0, "007 nullsub_1849" },
		{ 0x8CE7D0, (void *)a_73A170, "007 MAG_007_sub_8CE7D0" },
		{ 0x8CE8C0, (void *)a_73C100, "007 MAG_007_sub_8CE8C0" },
		{ 0x8CE910, (void *)a_73A380, "007 MAG_007_sub_8CE910" },
		{ 0x8D4200, (void *)a_73A6D0, "007 nullsub_1850" },
		{ 0x8D4270, (void *)a_73A6D0, "007 nullsub_1851" },
		{ 0x8D4300, (void *)a_73A380, "007 MAG_007_sub_8D4300" },
		{ 0x8D43C0, (void *)a_73A6D0, "007 nullsub_1852" },
		{ 0x8D43D0, (void *)a_747550, "007 MAG_007_sub_8D43D0" },
		{ 0x8D43E0, (void *)a_73A0D0, "007 MAG_007_sub_8D43E0" },
		{ 0x8D43F0, (void *)a_73A0D0, "007 MAG_007_sub_8D43F0" },
		{ 0x8D4400, (void *)a_7475A0, "007 MAG_007_sub_8D4400" },
		{ 0x8D4420, (void *)a_73A6D0, "007 nullsub_1853" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x8CE4F0, (void *)mf_8CE4F0, "007 MAG_007_sub_8CE4F0" },
		{ 0x8CE650, (void *)mf_8CE650, "007 MAG_007_sub_8CE650" },
		{ 0x8CE6A0, (void *)mf_8CE6A0, "007 MAG_007_sub_8CE6A0" },
		{ 0x8CE780, (void *)mf_8CE780, "007 MAG_007_sub_8CE780" },
		{ 0x8CE800, (void *)mf_8CE800, "007 MAG_007_sub_8CE800" },
		{ 0x8CE890, (void *)mf_8CE890, "007 MAG_007_sub_8CE890" },
		{ 0x8D4210, (void *)mf_8D4210, "007 MAG_007_sub_8D4210" },
		{ 0x8D4230, (void *)mf_8D4230, "007 MAG_007_sub_8D4230" },
		{ 0x8D4280, (void *)mf_8D4280, "007 MAG_007_sub_8D4280" },
		{ 0x8D42D0, (void *)mf_8D42D0, "007 MAG_007_sub_8D42D0" },
		{ 0x8D4360, (void *)mf_8D4360, "007 MAG_007_sub_8D4360" },
		{ 0x8D4380, (void *)mf_8D4380, "007 MAG_007_sub_8D4380" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag007_mega_phoenix()
	{
		fxp::register_params({ 7, "Mega Phoenix", &act::megaphoenix::live, &act::megaphoenix::GAME, sizeof(act::megaphoenix::MegaPhoenixParams),
		                       act::megaphoenix::table::PARTS, (int)(sizeof(act::megaphoenix::table::PARTS) / sizeof(act::megaphoenix::table::PARTS[0])),
		                       act::megaphoenix::table::FIELDS, (int)(sizeof(act::megaphoenix::table::FIELDS) / sizeof(act::megaphoenix::table::FIELDS[0])),
		                       act::megaphoenix::table::GROUPS, (int)(sizeof(act::megaphoenix::table::GROUPS) / sizeof(act::megaphoenix::table::GROUPS[0])) });
		act::register_mod(&act::actors::MOD_007);
		act::register_module(7);
		act::actors::register_ports(7);
		for (const act::megaphoenix::ModPort *p = act::megaphoenix::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(7, p->addr, p->port, p->name);
		for (const act::megaphoenix::ModPort *p = act::megaphoenix::PORTS; p->addr; p++)
			act::register_module_port(7, p->addr, p->port, p->name);
		// 30 fps layer: see mag007_mega_phoenix_held.inc
		FX_HELD(register_mag007_held();)
	}
	FX_LAB_MODULE(register_mag007_mega_phoenix);
}

#ifdef FF8_FX_HELD
#include "mag007_mega_phoenix_held.inc"
#endif
