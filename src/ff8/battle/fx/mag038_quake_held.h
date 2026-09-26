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

// 30 fps part of mag038_quake.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace quake038
{
	static void held_note_root();
	static void held_note_ground();
	static void held_note_edge(int32_t id, int32_t k, int32_t a, int32_t b, int32_t n, int16_t st, int16_t h);
	static void held_note_wall(int32_t m, uint8_t step);
	static void held_note_debris(uint32_t n);
	static void held_note_sprite(uint32_t n);
}
	static void register_mag038_held();
}
