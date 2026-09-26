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

// 30 fps part of mag199_cactuar.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace c199
{
	static void held_note_master();
	static void held_note_creature();
	static void held_note_creature_next(const Node24 *n, int advance);
	static void held_note_prim(const TaskNode *tn, int16_t c, int16_t v);
	static void held_note_dust(int i, const int16_t *pos, int16_t age);
	static void held_note_needle(int i, const int16_t *pos, int16_t age);
	static void held_note_needle_color(int i, uint32_t color);
}
	static void register_mag199_held();
}
