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

// Effect 123: Slow (spell, MAG_123_*).
//
// Structure (setup MAG_123_SLOW 0x6BD1C0 -> _Init 0x6BD1F0, file loader 0x6BD1D0 = the texture
// file named at 0x126E4B4; the setup starts camera animation 0x126D730 and queues the TIM):
//   RootTask (0x6BD260) - alternates the packet arena (magic buffer + 0x1AAC / + 0x19AAC); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target; every tick rebuilds the effect camera matrix
//     (MAG_069_sub_67AAE0 of the battle camera, 0x2521470) and runs the target queue; ends when
//     it is empty.
//   Target (0x6BD3E0) - node 0x764, one per action: two keyframed prim-model layouts (the clock
//     0x12652B0 at the target's anchor, the second 0x1268D3C lowered by the target's size) played
//     with the callback ClockPart (0x6BD580), both turned with the target and scaled by its size;
//     sound at 0; the target model blinks (entity flag bit 3) over 20..59; a screen fade at 48;
//     when both layouts have finished: the status, end.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521448..0x25214B0 (texture file, magic buffer base, context, root pool,
// packet cursor 0x252146C, effect camera matrix 0x2521470, target / root queues). The target pool
// (3 x 0x764), the morph vertex buffer (+0x162C) and the packet arenas are in the magic buffer.
// The module is the clock spell module (mag_clock_spell.h: its code, parts and parameters); this
// file says where this copy is and its build options (its sound tick, the "scaled" word it reads,
// the target's blink).

#include "mag_clock_spell.h"

namespace ff8fx
{
namespace slow123
{
	inline constexpr clockspell::Layout LAYOUT = {
		0x6BD260, 0x6BD3E0,                                         // root, target
		0x12652B0, 0x1D8, 0x1268D3C, 0x568, 0x126E4B0,     // clock, lower (decoded sizes), sound
		0x252144C, 0x2521450, 0x252146C, 0x2521490, 0x2521470,  // magic buffer base, context, cursor, targets, camera
		0x1AAC, 0x19AAC, 0x162C, 0x764,                          // arenas, morph buffer, target node
		0, true, 0x28,                                            // sound tick, scaled from the camera, blink ticks
	};
}

	void register_mag123_slow()
	{
		clockspell::ClockSpell<slow123::LAYOUT>::register_module(123, "Slow", "123");
	}
	FX_LAB_MODULE(register_mag123_slow);
}
