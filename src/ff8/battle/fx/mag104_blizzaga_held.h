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

// 30 fps part of mag104_blizzaga.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace blizzaga104
{
	static void held_note_root();
	static void held_note_draw(uint32_t fn, const void *node);
	static void held_note_frozen(const FreezeNode *f, int16_t level);
	static void held_note_block(const FreezeNode *f, int kind, const Mat4x3 *m);
	static void held_note_part(uint32_t fn, const FreezeNode *f);
	static void held_note_trail(uint32_t fn, const void *node);
	static bool held_drawing();
}
	static void register_mag104_held();
}
