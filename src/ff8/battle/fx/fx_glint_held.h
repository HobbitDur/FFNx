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

// 30 fps part of fx_glint.cpp (the shared effect-library glint task Effect_Glint_Tick 0x8DCC20).
// Included by fx_glint.cpp (its FX_HELD statements) and by the held part of every module whose
// effect spawns glints: their held frame calls glint::held_draw at the place of the glint queue in
// the module's queue order (inside the module's HeldScope, so the glints' packets go to the
// module's private packet buffer).
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace glint
{
	// seam of the glint task (real tick): the glint `node` is about to draw into the packet cursor
	// at `cursor` (the address the spawner stored in 0x2792E74)
	void held_note_draw(const void *node, uint32_t *cursor);

	// held frame: draws again, in the real tick's order, every glint of the effect whose root node
	// is `root` (effect-library node +0x10; nullptr = every glint) that drew on this real tick, at
	// its state num/den of the way to its next tick (size, colours and spin half way; a glint whose
	// next tick hides it holds). Each draw starts from the GTE state its real draw started from and
	// writes into the cursor it used then (redirect that cursor first, e.g. HeldScope). Nothing is
	// drawn when no glint drew on this real tick. No state is changed (the cursor advances).
	void held_draw(const void *root, int num, int den);
}
	void register_fx_glint_held();
}
