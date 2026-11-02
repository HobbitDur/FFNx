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

// Effect 294: Antidote (battle item Antidote: kernel.bin battle item +4 = 294; MAG_294_*). No other
// kernel.bin entry uses effect 294. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_294 0x6743F0 -> 0x674420, file loader 0x674400 = the texture file named at 0x10637A0;
// the setup keeps the cast context (0x2507A4C) and the caster's entity (0x2507A50, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507A78):
//   RootTask (0x674490) - alternates the packet arena (magic buffer + 0xA34 / + 0x10A34 by tick parity,
//     cursor 0x2507A8C); on counter 1 sets up the play pool (1 x 0x8B4 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its default effect position (0x571400) with y = half its height (+0x92) above its +0x24;
//     then the play queue (sound 0x106379C on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x6745C0) - draws THEN advances layout 0x1060C50 (0x8A0 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x674630) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x8B4, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     the offset + the anchor through the camera, composed with it unless flag 0x8000;
//     + the camera translation; scale (scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507A48..0x2507A94 (texture file 0x2507A48, context 0x2507A4C, caster entity 0x2507A50, play
// queue 0x2507A58, root queue 0x2507A68, root pool 0x2507A78 (1 x 0x14), packet cursor 0x2507A8C, magic buffer
// base 0x2507A90). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// The module is the status-cure item module (mag_status_item.h: its code, parts and parameters);
// this file says where this copy is and its build options (the record flags its callback tests).

#include "mag_status_item.h"

namespace ff8fx
{
namespace antidote294
{
	inline constexpr statusitem::Layout LAYOUT = {
		0x674490, 0x6745C0,                          // root, play
		0x1060C50, 0x8A0, 0x106379C,                 // layout (decoded size), sound
		0x2507A4C, 0x2507A58, 0x2507A8C, 0x2507A90,  // context, play queue, packet cursor, magic buffer base
		0xA34, 0x10A34,                              // packet arenas
		layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_NOT_COMPOSED,
	};
}

	void register_mag294_antidote()
	{
		statusitem::StatusItem<antidote294::LAYOUT>::register_module(294, "Antidote", "294");
	}
	FX_LAB_MODULE(register_mag294_antidote);
}
