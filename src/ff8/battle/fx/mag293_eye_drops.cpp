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

// Effect 293: Eye Drops (battle item Eye Drops: kernel.bin battle item +4 = 293; MAG_293_*). No other
// kernel.bin entry uses effect 293. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_293 0x674870 -> 0x6748A0, file loader 0x674880 = the texture file named at 0x1065134;
// the setup keeps the cast context (0x2507A9C) and the caster's entity (0x2507AA0, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507AC8):
//   RootTask (0x674910) - alternates the packet arena (magic buffer + 0x4EC / + 0x104EC by tick parity,
//     cursor 0x2507ADC); on counter 1 sets up the play pool (1 x 0x4E4 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its effect anchor 0xF0 (GetEffectSpawnPosition 0x502170);
//     then the play queue (sound 0x1065130 on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x674A30) - draws THEN advances layout 0x10637AC (0x4D0 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x674AA0) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x4E4, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     flag 0x200: the anchor through the camera + the offset; else the offset + the anchor
//     through the camera, composed with it;
//     + the camera translation; scale (flag 0x100: 3x3 product, else scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507A98..0x2507AE4 (texture file 0x2507A98, context 0x2507A9C, caster entity 0x2507AA0, play
// queue 0x2507AA8, root queue 0x2507AB8, root pool 0x2507AC8 (1 x 0x14), packet cursor 0x2507ADC, magic buffer
// base 0x2507AE0). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// The module is the status-cure item module (mag_status_item.h: its code, parts and parameters);
// this file says where this copy is and its build options (the record flags its callback tests,
// its anchor, the sound's place, its camera matrix).

#include "mag_status_item.h"

namespace ff8fx
{
namespace eyedrops293
{
	inline constexpr statusitem::Layout LAYOUT = {
		0x674910, 0x674A30,                          // root, play
		0x10637AC, 0x4D0, 0x1065130,                 // layout (decoded size), sound
		0x2507A9C, 0x2507AA8, 0x2507ADC, 0x2507AE0,  // context, play queue, packet cursor, magic buffer base
		0x4EC, 0x104EC,                              // packet arenas
		layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS | layouts::ANCHOR_DIAGONAL_SCALE,
		layouts::ANCHOR_AT_BONE, 0xF0, 1, 2, 1, 2,   // anchor: its effect point 0xF0
		false, 0,                                    // sound first, camera matrix
	};
}

}

#ifdef FF8_FX_HELD
#include "mag293_eye_drops_held.h"
#endif

namespace ff8fx
{
	void register_mag293_eye_drops()
	{
		statusitem::StatusItem<eyedrops293::LAYOUT>::register_module(293, "Eye Drops", "293");
		// 30 fps layer: see mag293_eye_drops_held.inc
		FX_HELD(register_mag293_held();)
	}
	FX_LAB_MODULE(register_mag293_eye_drops);
}

#ifdef FF8_FX_HELD
#include "mag293_eye_drops_held.inc"
#endif
