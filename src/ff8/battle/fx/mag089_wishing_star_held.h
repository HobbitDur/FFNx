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

// 30 fps part of mag089_wishing_star.cpp: declarations used by its FX_HELD(...) seams
// (the held-frame code is in mag089_wishing_star_held.inc)

#pragma once

#include "fx_held.h"

namespace ff8fx
{
namespace ws089
{
	static void held_note_root();
	static void held_note_director(DirectorNode *d);
	static void held_note_play(uint8_t *layout, int32_t x, int32_t y, int32_t z, int16_t depth, bool frame_arena);
	static void held_note_grow(int32_t g);
	static void held_note_landing(uint32_t lt, const int16_t *p, int32_t g);
	static void held_note_stars();
	static void held_note_shards();
	static void held_note_angelo(DirectorNode *d);
	static void held_note_tile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out);
	static void held_note_particles(int which);
	static void held_note_flash(SmallNode *t);
	static void held_note_debris(SmallNode *t);
	static void held_note_trail(DiveNode *d);
	static void held_note_dive_play(DiveNode *d, const Mat4x3 *m);
	static void held_note_restart();
}
	static void register_mag089_held();
}
