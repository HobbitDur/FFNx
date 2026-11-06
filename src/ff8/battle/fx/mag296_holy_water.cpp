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

// Effect 296: Holy Water (battle item Holy Water: kernel.bin battle item +4 = 296; MAG_296_*). No other
// kernel.bin entry uses effect 296. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_296 0x673970 -> 0x6739A0, file loader 0x673980 = the texture file named at 0x105DAB4;
// the setup keeps the cast context (0x250798C) and the caster's entity (0x2507990, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x25079B8):
//   RootTask (0x673A10) - alternates the packet arena (magic buffer + 0x65C / + 0x1065C by tick parity,
//     cursor 0x25079CC); on counter 1 sets up the play pool (1 x 0x29C bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its default effect position (0x571400) with y = (2h / 3 + h / 2) / 2 above its +0x24 (h = its +0x92);
//     then the play queue (sound 0x105DAB0 on counter 1, before it); ends when the play queue is empty.
//   PlayTask (0x673B60) - draws THEN advances layout 0x105BAA4 (0x288 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x673BD0) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x29C, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     flag 0x200: the anchor through the camera + the offset; else the offset + the anchor
//     through the camera, composed with it;
//     + the camera translation; scale (scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507988..0x25079D4 (texture file 0x2507988, context 0x250798C, caster entity 0x2507990, play
// queue 0x2507998, root queue 0x25079A8, root pool 0x25079B8 (1 x 0x14), packet cursor 0x25079CC, magic buffer
// base 0x25079D0). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// The module is the status-cure item module (mag_status_item.h: its code, parts and parameters);
// this file says where this copy is and its build options (the record flags its callback tests,
// its anchor, the sound's place, its camera matrix).

#include "mag_status_item.h"

namespace ff8fx
{
namespace holy296
{
	inline constexpr statusitem::Layout LAYOUT = {
		0x673A10, 0x673B60,                          // root, play
		0x105BAA4, 0x288, 0x105DAB0,                 // layout (decoded size), sound
		0x250798C, 0x2507998, 0x25079CC, 0x25079D0,  // context, play queue, packet cursor, magic buffer base
		0x65C, 0x1065C,                              // packet arenas
		layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS,
		layouts::ANCHOR_AT_HEIGHT, 0xF0, 2, 3, 1, 2,   // anchor: its effect position between 2 / 3 and 1 / 2 of its height; the sound before the play
		true, 0,                                    // sound first, camera matrix
	};
}

}

#ifdef FF8_FX_HELD
#include "mag296_holy_water_held.h"
#endif

namespace ff8fx
{
	void register_mag296_holy_water()
	{
		statusitem::StatusItem<holy296::LAYOUT>::register_module(296, "Holy Water", "296");
		// 30 fps layer: see mag296_holy_water_held.inc
		FX_HELD(register_mag296_held();)
	}
	FX_LAB_MODULE(register_mag296_holy_water);
}

#ifdef FF8_FX_HELD
#include "mag296_holy_water_held.inc"
#endif
