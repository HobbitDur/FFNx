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

// The actor system of the actor/heal effect library (act_engine.h): the engine's particle actors
// (emitters and particles, sprite sequences and prim models) that a module's actor data drives
// (4 tables of 16 entries: the actor definitions, their models, sequences and vertex blocks), run
// by one task of 0x2A4 bytes (its data +0x30, its state block +0x34, start tick +0x298, mask
// +0x29A, mode +0x29C, length +0x29E). Every module of the family has a private copy of this code;
// this one is Mega Phoenix's (0x8CE970..0x8D41E0, mag007_mega_phoenix.cpp), its module memory
// taken from a Mem (the current one: Use), so the same code runs for the module or for an effect
// of its own (Fujin's described effects) with memory of its own:
//   - Mem: the module words the code reads (the current actor state block, the module scratch
//     stack pointer, the packet cursor, the two actor pools and their cursors, the two counters
//     the master clears every tick);
//   - MOD_007: the module descriptor of the engine ports it calls (act_engine.h G()/F()): an own
//     copy changes its globals (G_257F9B0 scratch top, G_258BA90 actor system queue, G_258BE30
//     emitter queue, G_258FB78 actor state), its functions stay Mega Phoenix's addresses, the keys
//     of the ports (register_dispatch).

#pragma once

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace actors
{
	struct Mem
	{
		uint32_t actor_state;    // the current actor system's state block (word)
		uint32_t scratch_sp;     // the module scratch stack pointer (word; the master sets it to the camera snapshot)
		uint32_t packet_cursor;  // the module packet cursor (word: the draws' arena)
		uint32_t particle_pool;  // the particle pool (word: 25 records of 0x6C) and its next slot (u16)
		uint32_t particle_next;
		uint32_t actor_pool;     // the actor pool (word: 80 records of 0x2A0) and its next slot (u16)
		uint32_t actor_next;
		uint32_t count_a;        // the counters every step adds its pending counts to (u16; the master clears them)
		uint32_t count_b;
		int32_t part_actors = 0; // Fujin's effect lab: the part of the system's queue and, from part_first, each definition's
		int32_t part_first = 0;  // draws (part_first + its index; 0: not told apart)
	};
	// Mega Phoenix's (effect 7)
	inline constexpr Mem MEGA_PHOENIX = { 0x2760B60, 0x2760B50, 0x275FB84, 0x2760B3C, 0x275FC80, 0x2760B54, 0x2760B48, 0x2760B4A,
	                                      0x2760B3A, 1, 2 };

	// an actor definition's fields an editor changes (its 16.16 values: their integer word), written into the
	// actor data before a cast (PatchDefinition): what its emitter is (where, when, how many), what its particles
	// are (kind, orientation, life), how they move, what they spawn
	struct DefParams
	{
		uint8_t placement;
		uint8_t point;
		uint8_t facing;
		int16_t offset_x;
		int16_t offset_y;
		int16_t offset_z;
		uint8_t between;
		uint8_t tick;
		uint8_t per_target;
		uint8_t frames;
		uint8_t batch;
		uint8_t spread;
		uint8_t exact;
		uint8_t kind;
		uint8_t loop_first;
		uint8_t loop_last;
		uint8_t loops_min;
		uint8_t loops_max;
		uint8_t orientation;
		uint8_t rotation;
		uint8_t turn_offsets;
		int16_t depth;
		int16_t one_sided;
		uint8_t end;
		uint8_t timer_min;
		uint8_t timer_max;
		uint8_t angle_from;
		int16_t dir_x;
		int16_t dir_y;
		int16_t dir_z;
		int16_t dir_jitter_x;
		int16_t dir_jitter_y;
		int16_t dir_jitter_z;
		int16_t angle_x;
		int16_t angle_y;
		int16_t angle_z;
		int16_t angle_jitter_x;
		int16_t angle_jitter_y;
		int16_t angle_jitter_z;
		uint8_t motion;
		uint8_t relative;
		int16_t speed_min;
		int16_t speed_max;
		int16_t speed_step_min;
		int16_t speed_step_max;
		int16_t gravity_min;
		int16_t gravity_max;
		int16_t damping;
		int16_t velocity_min_x;
		int16_t velocity_min_y;
		int16_t velocity_min_z;
		int16_t velocity_max_x;
		int16_t velocity_max_y;
		int16_t velocity_max_z;
		int16_t accel_min_x;
		int16_t accel_min_y;
		int16_t accel_min_z;
		int16_t accel_max_x;
		int16_t accel_max_y;
		int16_t accel_max_z;
		int16_t accel_step_min_x;
		int16_t accel_step_min_y;
		int16_t accel_step_min_z;
		int16_t accel_step_max_x;
		int16_t accel_step_max_y;
		int16_t accel_step_max_z;
		uint8_t follow;
		uint8_t follow_def;
		uint8_t sub_0_on;
		uint8_t sub_0_def;
		uint8_t sub_1_on;
		uint8_t sub_1_def;
		uint8_t sub_2_on;
		uint8_t sub_2_def;
		uint8_t sub_3_on;
		uint8_t sub_3_def;
	};
	struct DefField { uint16_t param, data; uint8_t type; };   // in DefParams, in the definition; 0 u8, 1 i16
	extern const DefField DEF_FIELDS[];
	extern const int DEF_FIELD_COUNT;
	void PatchDefinition(uint32_t def, const DefParams &p);
	extern const Mod MOD_007;

	extern const Mem *g_mem;
	inline const Mem &M() { return *g_mem; }
	// the module memory and descriptor the code runs with (until the next call)
	inline void Use(const Mem *mem, const Mod *mod)
	{
		g_mem = mem;
		g_mod = mod;
	}

	// the actor system task's two states (0x8CE970, 0x8D41E0) and what they run
	uint32_t __cdecl mf_8CE970(uint32_t a1);   // from its start tick: the set-up and a first step, next state
	uint32_t __cdecl mf_8D41E0(uint32_t a1);   // a step; once done, finished, next state
	uint32_t __cdecl mf_8CE9A0(uint32_t a1);   // the set-up (state block, tables, reference points of the caster and target)
	uint32_t __cdecl mf_8CF3D0(uint32_t a1);   // a step (update, draws); 1 once done

	// the library's ports at Mega Phoenix's addresses: in the dispatch table and the task registry
	void register_ports(int effect_id);
	// in the dispatch table only (an effect of its own: no original to stand in for)
	void register_dispatch();
}
}
}
