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

// 30 fps part of mag010_ray_bomb.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"
#include <initializer_list>

namespace ff8fx
{
namespace raybomb010
{
	static void held_note_root();
	static void held_note_draw(uint32_t fn, const void *node);
	static void held_note_after(const void *node);
	static void held_note_particles();
	static void held_fix_header(uint8_t *h);
	static void held_fix_stretch(int32_t *stretch, int32_t full);
}
	static void register_mag010_held();
}
