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

// Effect 120: Petrify Stare (enemy attack 19 of kernel.bin, e.g. Glacial Eye; MAG_120_*).
//
// Structure (setup MAG_120_PETRIFY_STARE 0x6BE960 -> _Init 0x6BE990, file loader 0x6BE970 = the
// texture file named at 0x127DB50 (mag119.tim, shared with Stop); the setup keeps the CASTER's
// entity, starts camera animation 0x127DA24 and queues the TIM):
//   RootTask (0x6BEA10) - alternates the packet arena (magic buffer + 0x660 / + 0x18660); clears the
//     screen-flash level; on counter 1 of a 10-tick cycle sets up the target pool (first time: ONE
//     node) and starts the target task of the next action on its first target (the pool holds one
//     node: while a target task runs, later actions fail to spawn, and the root ends as soon as the
//     target queue is empty, so only the first action plays); runs the target queue; screen flash
//     (level 0 every tick); ends when the target queue is empty.
//   Target (0x6BEB80) - node 0x658, draws THEN updates:
//     every tick: the "stare" layout 0x1277640 at the caster's effect anchor 0xF0 (followed until
//       counter 10, then frozen) moved 1/16 of the unit circle towards the caster's facing
//       (-sin / -cos of entity +0x0E), depth offset from the CASTER's size;
//     counter 4: engine screen fade task (0x5712C0);
//     counters 14..: the "stone" layout 0x1279C68 at the target's default effect position (taken
//       at counter 14), depth offset from the target's size;
//     counter 0: sound 0x127DB4C at the caster's default position; counter 42: damage; ends at 49.
//   PartCallback (0x6BED30) - one object of either layout: record flag 0x400 selects the rotation
//     builder 0x701430, else 0x701310; flag 0x200 = the offset stays in screen axes (the anchor
//     through the camera, the offset added unrotated, the rotation not composed with the camera),
//     else anchor + offset through the camera; scale by scale3DMatrix. Both layouts only use
//     flag 0x200 records without vertex-frame blending (the other paths are ported as coded).
//     Blended vertex frames would go to magic buffer + 0x658, 8 bytes before arena A.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521540..0x25215A0 (flash level, caster entity, texture file, magic buffer
// base, context, root pool, packet cursor 0x252156C, target / root queues). The target pool
// (1 x 0x658), the blend buffer (+0x658) and the packet arenas are in the magic buffer.
// Written from the Petrify Stare template (mag_petrify_stare.h: the parts library's anchor callback,
// DepthFromSize, AheadOfFacing): this copy's Layout. Parameters (PetrifyStareParams, fx_params.h):
// the numbers above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_petrify_stare.h"

namespace ff8fx
{
namespace petrify120
{
	inline constexpr petrifystare::Layout LAYOUT = {
		0x6BEA10, 0x6BEB80,                                         // root, target
		0x1277640, 0x140, 0x1279C68, 0x4F4, 0x127DB4C,            // stare, stone (decoded sizes), sound
		0x2521540, 0x2521544, 0x252154C, 0x2521550, 0x252156C, 0x2521570,   // flash, caster, magic buffer base, context, cursor, targets
		0x660, 0x18660, 0x658, 0x658,                              // arenas, morph buffer, target node
		layouts::ANCHOR_OFFSET_AS_IS | layouts::ANCHOR_ROT_ZYX,     // record flags: 0x200, 0x400 (0x701430)
	};
}

	void register_mag120_petrify_stare()
	{
		petrifystare::PetrifyStare<petrify120::LAYOUT>::register_module(120, "Petrify Stare", "P120");
	}
	FX_LAB_MODULE(register_mag120_petrify_stare);
}
