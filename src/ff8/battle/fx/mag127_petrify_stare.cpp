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

// Effect 127: Petrify Stare (enemy attack 29 of kernel.bin, Cockatrice; MAG_127_*). Same code as
// effect 120 (the Glacial Eye's Petrify Stare) with its own layouts, sizes and rotation builder.
//
// Structure (setup MAG_127_PETRIFY_STARE 0x6B93A0 -> _Init 0x6B93D0, file loader 0x6B93B0 = the
// texture file named at 0x124159C (mag126.tim); the setup keeps the CASTER's entity, starts camera
// animation 0x12414AC and queues the TIM):
//   RootTask (0x6B9450) - alternates the packet arena (magic buffer + 0x8A4 / + 0x188A4); clears the
//     screen-flash level; on counter 1 of a 10-tick cycle sets up the target pool (first time: ONE
//     node) and starts the target task of the next action on its first target (the pool holds one
//     node: while a target task runs, later actions fail to spawn, and the root ends as soon as the
//     target queue is empty, so only the first action plays); runs the target queue; screen flash
//     (level 0 every tick); ends when the target queue is empty.
//   Target (0x6B95C0) - node 0x4BC, draws THEN updates:
//     every tick: the "stare" layout 0x12377BC at the caster's effect anchor 0xF0 (followed until
//       counter 10, then frozen) moved 1/16 of the unit circle towards the caster's facing
//       (-sin / -cos of entity +0x0E), depth offset from the CASTER's size;
//     counter 4: engine screen fade task (0x5712C0);
//     counters 14..: the "stone" layout 0x123BC84 at the target's default effect position (taken
//       at counter 14), depth offset from the target's size;
//     counter 0: sound 0x1241598 at the caster's default position; counter 42: damage; ends at 49.
//   PartCallback (0x6B9770) - one object of either layout: record flag 0x40000 selects the rotation
//     builder 0x7015B0, else 0x701310; flag 0x200 = the offset stays in screen axes (the anchor
//     through the camera, the offset added unrotated, the rotation not composed with the camera),
//     else anchor + offset through the camera; scale by scale3DMatrix.
//     Blended vertex frames go to magic buffer + 0x4BC (0x3E8 bytes before arena A).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521270..0x25212C0 (flash level, caster entity, texture file, magic buffer
// base, context, root pool, packet cursor 0x252129C, target / root queues). The target pool
// (1 x 0x4BC), the blend buffer (+0x4BC) and the packet arenas are in the magic buffer.
// Written from the Petrify Stare template (mag_petrify_stare.h: the parts library's anchor callback,
// DepthFromSize, AheadOfFacing): this copy's Layout. Parameters (PetrifyStareParams, fx_params.h):
// the numbers above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_petrify_stare.h"

namespace ff8fx
{
namespace petrify127
{
	inline constexpr petrifystare::Layout LAYOUT = {
		0x6B9450, 0x6B95C0,                                         // root, target
		0x12377BC, 0x1C8, 0x123BC84, 0x2D0, 0x1241598,            // stare, stone (decoded sizes), sound
		0x2521270, 0x2521274, 0x252127C, 0x2521280, 0x252129C, 0x25212A0,   // flash, caster, magic buffer base, context, cursor, targets
		0x8A4, 0x188A4, 0x4BC, 0x4BC,                              // arenas, morph buffer, target node
		layouts::ANCHOR_OFFSET_AS_IS | layouts::ANCHOR_ROT_YXZ,     // record flags: 0x200, 0x40000 (0x7015B0)
	};
}

}

#ifdef FF8_FX_HELD
#include "mag127_petrify_stare_held.h"
#endif

namespace ff8fx
{
	void register_mag127_petrify_stare()
	{
		petrifystare::PetrifyStare<petrify127::LAYOUT>::register_module(127, "Petrify Stare (Cockatrice)", "P127");
		// 30 fps layer: see mag127_petrify_stare_held.inc
		FX_HELD(register_mag127_held();)
	}
	FX_LAB_MODULE(register_mag127_petrify_stare);
}

#ifdef FF8_FX_HELD
#include "mag127_petrify_stare_held.inc"
#endif
