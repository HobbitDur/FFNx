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

// Effect 125: Haste (spell, MAG_125_*).
//
// Structure (setup MAG_125_HASTE 0x6BB6B0 -> _Init 0x6BB6E0, file loader 0x6BB6C0 = the texture
// file named at 0x1252BD8; the setup starts camera animation 0x1251DE8 and queues the TIM):
//   RootTask (0x6BB750) - alternates the packet arena (magic buffer + 0x1A70 / + 0x19A70); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target; every tick rebuilds the effect camera matrix
//     (MAG_069_sub_67AAE0 of the battle camera, 0x2521390) and runs the target queue; ends when
//     it is empty.
//   Target (0x6BB8D0) - node 0x7D0, one per action: two keyframed prim-model layouts (0x1248A0C at
//     the target's anchor, 0x124BD98 lowered by the target's size) played with the callback
//     ClockPart (0x6BBA40), both turned with the target and scaled by its size; sound at 6; a
//     screen fade at 48; when both layouts have finished: the status, end.
// Same code template as Slow (123) with the block's "scaled" word written (1) and no blink.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521368..0x25213D0 (texture file, magic buffer base, context, root pool,
// packet cursor 0x252138C, effect camera matrix 0x2521390, target / root queues). The target pool
// (3 x 0x7D0), the morph vertex buffer (+0x1770) and the packet arenas are in the magic buffer.
// The module is the clock spell module (mag_clock_spell.h: its code, parts and parameters); this
// file says where this copy is and its build options (its sound tick, the "scaled" word it reads,
// the target's blink).

#include "mag_clock_spell.h"

namespace ff8fx
{
namespace haste125
{
	inline constexpr clockspell::Layout LAYOUT = {
		0x6BB750, 0x6BB8D0,                                         // root, target
		0x1248A0C, 0x254, 0x124BD98, 0x558, 0x1252BD4,     // clock, lower (decoded sizes), sound
		0x252136C, 0x2521370, 0x252138C, 0x25213B0, 0x2521390,  // magic buffer base, context, cursor, targets, camera
		0x1A70, 0x19A70, 0x1770, 0x7D0,                          // arenas, morph buffer, target node
		6, false, 0,                                            // sound tick, scaled from the camera, blink ticks
	};
}

}

#ifdef FF8_FX_HELD
#include "mag125_haste_held.h"
#endif

namespace ff8fx
{
	void register_mag125_haste()
	{
		clockspell::ClockSpell<haste125::LAYOUT>::register_module(125, "Haste", "125");
		// 30 fps layer: see mag125_haste_held.inc
		FX_HELD(register_mag125_held();)
	}
	FX_LAB_MODULE(register_mag125_haste);
}

#ifdef FF8_FX_HELD
#include "mag125_haste_held.inc"
#endif
