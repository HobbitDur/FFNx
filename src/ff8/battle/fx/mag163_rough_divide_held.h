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

// 30 fps layer of mag163_rough_divide.cpp (Rough Divide): declarations used by its FX_HELD seams.

#pragma once

#include "fx_held.h"

namespace ff8fx
{
	static void register_mag163_held();

namespace rough163
{
	static void held_note_master();
	static void held_note_trail(const Node24 *n);
	static void held_note_node(uint32_t fn, const Node24 *n);
	static void held_note_glow(const Node24 *n, const int16_t *anchor);
	static void held_note_pool(uint32_t fn, const Node24 *n);
	static void held_note_header2(const uint8_t *h, int size);
	static void held_note_streak(const Node24 *n, const StreakWork *w);
}
}
