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

// 30 fps part of mag197_armor_shot.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace armor197
{
	static void held_note_root();
	static void held_note_node(uint32_t fn, const Node *n);
	static void held_note_head(int16_t size);
	static void held_note_part(const Particle *e);
	static void held_note_part_next(const Particle *e);
	static void held_note_alive(int32_t alive);
	static void held_note_ghosts(int32_t counter, int32_t slot, int32_t count, uint32_t colour);
}
	static void register_mag197_held();
}
