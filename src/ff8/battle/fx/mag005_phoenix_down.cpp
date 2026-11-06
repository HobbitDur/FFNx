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

// Effect 5: Phoenix Down (battle item Phoenix Down: kernel.bin battle item +4 = 5; MAG_005_*, IDA names
// MagicEffect05_*). The module is a relocated copy of Life's (effect 35, mag035_life.cpp): the same code
// instruction for instruction (Life's + 0x6FA80), byte-identical data (sound, sprite sequences, spawn
// tables: Life's + 0x57F20; the same texture file "mag004-034.tim"), its own globals (Life's + 0xB5120).
// Only the setup differs: it plays no camera animation. Like Life it is a copy of the actor/heal effect
// library (see act_engine.h).
//
// Setup MAG_005_PHOENIXDOWN 0x8D4450 (runs once, not ported; file loader 0x8D4430 = the sprite file
// 0x163E91C, its pointer in 0x27618A0) creates the root queue 0x2761988 with the master (node 0x64
// bytes: +0x58 = actions - 1, +0x5A = target count, +0x2D = first target), uploads the file's TIMs
// (unless cast flag bit0), takes two packet arenas in the magic texture buffer (0x276C294 = buffer,
// 0x276C290 = buffer + 0x2000) and four task pools:
//   0x2769CF0 emitters (4 x 0x64), 0x276C298 (300 x 0x30, never used), 0x2769CD8 sprite particles
//   (300 x 0x70), 0x276C280 glints (100 x 0x5C, the engine task Effect_Glint_Tick 0x8DCC20).
//   Master (0x8D4530) - packet cursor 0x27618A4 = one of the two arenas by tick parity, follows
//     the caster's bones, runs its 5-state table (0x8D4610 spawns one emitter per action and holds
//     itself on +0x63, 0x8D5000 goes on to the next action once the emitter releases it, 0x8D5030
//     finishes when no task is left), runs the four queues (live task count -> +0x5E), ends when
//     nothing is left.
//   Emitter (0x8D4640), one per action (target 0 of the action: the KO'd ally): bone-follow anchor +
//     model bounds (CURE_Emitter helpers), 5-state table animating its spawn parameters (+0x58 radius,
//     +0x5A height spread, +0x5C count, +0x5E / +0x60 sprite scale ramp, +0x62 ring height); while +0x26
//     bit1 is set (states 0..3) it spawns every tick: 0x8D4850 rising sparkles, 0x8D4700 revive
//     sparkles, 0x8D4980 ring sparkles, 0x8D4A10 falling sparkles, 0x8D4AD0 glints. State 3 (tick
//     > 30) applies the action's result to its target (ApplyActionResultToTarget: the revive) and
//     releases the master. Sound 0x163E2A8 at its first tick.
//   Sprite particles (0x70-byte nodes), all update THEN draw one sprite sequence (flipbook frame
//     +0x50 of the sequence +0x4C, InitEffectSequenceFromData) at +0x1C placed by
//     TransformCameraByShadowRotation (scale, camera offset +0x54):
//       0x8D4E60 static sparkle (scale +0x68 += +0x6A), 0x8D4CF0 ring sparkle (orbits the centre
//       +0x60/+0x64 by 0x80 per tick at radius sin/cos / +0x6C, rises 0x40 per tick, scale ramp),
//       0x8D4BA0 falling sparkle (y += +0x5A per tick, fixed scale 0x1000).
//   Glints (Effect_Glint_Tick 0x8DCC20, engine range, shared with the other heal spells: ported in
//     the shared glint file, not here): a spinning gouraud star at the target's bounds centre; its
//     packet cursor is the global pointer 0x2792E74, which Phoenix Down points at 0x27618A4 before each spawn.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x27618A0..0x276FAE8 (file pointer, packet cursor 0x27618A4, pools, queues, arena
// pointers 0x276C290 / 0x276C294); engine global 0x2792E74 (glint packet cursor pointer).
// Written from the revive template (mag_revive.h: the parts library's revive sparkles, fx_revive.h,
// and the effect library's nodes and glints, fx_parts.h): this copy's Layout. Parameters
// (ReviveParams, fx_params.h): the numbers above, defaults = the game's; the parts are tagged
// (FX_PART).

#include "mag_revive.h"

namespace ff8fx
{
namespace phoenixdown005
{
	inline constexpr revive::Layout LAYOUT = {
		0x8D4530, 0x8D4640, 0x8D4BA0, 0x8D4CF0, 0x8D4E60,              // master, emitter, falling, ring, sparkle
		0x163E2A8, 0x163E7B0, 0x163E2AC, 0x163E458, 0x163E604, 0x163E80C,   // sound; revive, rising a / b, ring, falling sequences
		0x27618A4, 0x276C294, 0x276C290,                             // packet cursor, arena words (odd, even ticks)
		0x2769CF0, 0x276C298, 0x2769CD8, 0x276C280,                  // emitter, unused, sparkle, glint queues
	};
}

}

#ifdef FF8_FX_HELD
#include "mag005_phoenix_down_held.h"
#endif

namespace ff8fx
{
	void register_mag005_phoenix_down()
	{
		revive::Revive<phoenixdown005::LAYOUT>::register_module(5, "Phoenix Down", "005");
		// 30 fps layer: see mag005_phoenix_down_held.inc
		FX_HELD(register_mag005_held();)
	}
	FX_LAB_MODULE(register_mag005_phoenix_down);
}

#ifdef FF8_FX_HELD
#include "mag005_phoenix_down_held.inc"
#endif
