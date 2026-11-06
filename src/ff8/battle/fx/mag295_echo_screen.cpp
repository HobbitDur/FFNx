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

// Effect 295: Echo Screen (battle item Echo Screen: kernel.bin battle item +4 = 295; MAG_295_*). No other
// kernel.bin entry uses effect 295. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_295 0x673E70 -> 0x673EA0, file loader 0x673E80 = the texture file named at 0x1060C44;
// the setup keeps the cast context (0x25079DC) and the caster's entity (0x25079E0, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507A08):
//   RootTask (0x673F10) - alternates the packet arena (magic buffer + 0x9E8 / + 0x109E8 by tick parity,
//     cursor 0x2507A1C); on counter 1 sets up the play pool (1 x 0x9E0 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its default effect position (0x571400);
//     then the module camera matrix 0x2507A28 (0x67AAE0 of the camera) and the play queue (sound 0x1060C40 on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x674030) - draws THEN advances layout 0x105DAC0 (0x9CC bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x6740A0) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x9E0, right after the play node), rotation from the record's angles (flag 0x400: 0x701430, else 0x701310);
//     flag 0x1000: the anchor through the camera + the offset through the module camera matrix,
//     composed with it unless flag 0x8000; else the offset + the anchor through the camera, composed with it unless flag 0x8000;
//     + the camera translation; scale (flag 0x100: 3x3 product, else scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x25079D8..0x2507A48 (texture file 0x25079D8, context 0x25079DC, caster entity 0x25079E0, play
// queue 0x25079E8, root queue 0x25079F8, root pool 0x2507A08 (1 x 0x14), packet cursor 0x2507A1C, magic buffer
// base 0x2507A20, module camera matrix 0x2507A28). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// The module is the status-cure item module (mag_status_item.h: its code, parts and parameters);
// this file says where this copy is and its build options (the record flags its callback tests,
// its anchor, the sound's place, its camera matrix).

#include "mag_status_item.h"

namespace ff8fx
{
namespace echo295
{
	inline constexpr statusitem::Layout LAYOUT = {
		0x673F10, 0x674030,                          // root, play
		0x105DAC0, 0x9CC, 0x1060C40,                 // layout (decoded size), sound
		0x25079DC, 0x25079E8, 0x2507A1C, 0x2507A20,  // context, play queue, packet cursor, magic buffer base
		0x9E8, 0x109E8,                              // packet arenas
		layouts::ANCHOR_ROT_ZYX | layouts::ANCHOR_MATRIX | layouts::ANCHOR_NOT_COMPOSED | layouts::ANCHOR_DIAGONAL_SCALE,
		layouts::ANCHOR_AT_POSITION, 0xF0, 1, 2, 1, 2,   // anchor: its effect position; its camera matrix at 0x2507A28
		false, 0x2507A28,                                    // sound first, camera matrix
	};
}

}

#ifdef FF8_FX_HELD
#include "mag295_echo_screen_held.h"
#endif

namespace ff8fx
{
	void register_mag295_echo_screen()
	{
		statusitem::StatusItem<echo295::LAYOUT>::register_module(295, "Echo Screen", "295");
		// 30 fps layer: see mag295_echo_screen_held.inc
		FX_HELD(register_mag295_held();)
	}
	FX_LAB_MODULE(register_mag295_echo_screen);
}

#ifdef FF8_FX_HELD
#include "mag295_echo_screen_held.inc"
#endif
