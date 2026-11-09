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

// 30 fps layer of mag343_renzokuken_griever.cpp (Renzokuken vs Griever): declarations used by its FX_HELD seams.

#pragma once

#include "fx_held.h"

namespace ff8fx
{
	static void register_mag343_held();

namespace renzo343
{
	static void held_note_master();
	static void held_note_trail(const Node24 *n);
	static void held_note_slash(const Node24 *n, const HitRec *r);
	static void held_note_particles();
	static void held_note_emitter(const Node24 *n, int32_t kind, uint32_t seq);
	static void held_note_slotmodel(int kind, const Node24 *n);
	static void held_note_dust(const Node24 *n);
	static void held_note_nodemodel(int kind, const Node24 *n);
	static void held_note_final(const Node24 *n);
}
}
