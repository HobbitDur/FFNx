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

// 30 fps part of mag319_counter_attack.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace counter319
{
	enum DrawKind { PLAY_A = 1, DRAW_SPIKE = 2, PLAY_B = 3, SPR = 4 };
	static void held_note_root();
	static void held_note_main(int kind, TaskNode *n);
	static void held_note_sprite(TaskNode *n, const int16_t *pos, int16_t counter);
}
	static void register_mag319_held();
}
