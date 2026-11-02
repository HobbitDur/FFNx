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

// Effect 292: Soft (battle item Soft: kernel.bin battle item +4 = 292; MAG_292_*). No other
// kernel.bin entry uses effect 292. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_292 0x674D90 -> 0x674DC0, file loader 0x674DA0 = the texture file named at 0x10673F4;
// the setup keeps the cast context (0x2507AEC) and the caster's entity (0x2507AF0, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507B18):
//   RootTask (0x674E30) - alternates the packet arena (magic buffer + 0x6DC / + 0x106DC by tick parity,
//     cursor 0x2507B2C); on counter 1 sets up the play pool (1 x 0x3B4 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its default effect position (0x571400) with y = half its height (+0x92) above its +0x24;
//     then the play queue (sound 0x10673F0 on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x674F60) - draws THEN advances layout 0x1065140 (0x3A0 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x674FD0) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x3B4, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     flag 0x200: the anchor through the camera + the offset; else the offset + the anchor
//     through the camera, composed with it unless flag 0x8000;
//     + the camera translation; scale (scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507AE8..0x2507B34 (texture file 0x2507AE8, context 0x2507AEC, caster entity 0x2507AF0, play
// queue 0x2507AF8, root queue 0x2507B08, root pool 0x2507B18 (1 x 0x14), packet cursor 0x2507B2C, magic buffer
// base 0x2507B30). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// The module is the status-cure item module (mag_status_item.h: its code, parts and parameters);
// this file says where this copy is and its build options (the record flags its callback tests).

#include "mag_status_item.h"

namespace ff8fx
{
namespace soft292
{
	inline constexpr statusitem::Layout LAYOUT = {
		0x674E30, 0x674F60,                          // root, play
		0x1065140, 0x3A0, 0x10673F0,                 // layout (decoded size), sound
		0x2507AEC, 0x2507AF8, 0x2507B2C, 0x2507B30,  // context, play queue, packet cursor, magic buffer base
		0x6DC, 0x106DC,                              // packet arenas
		layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS | layouts::ANCHOR_NOT_COMPOSED,
	};
}

	void register_mag292_soft()
	{
		statusitem::StatusItem<soft292::LAYOUT>::register_module(292, "Soft", "292");
	}
	FX_LAB_MODULE(register_mag292_soft);
}
