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

// 30 fps part of mag318_strong_counter_attack.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace strong318
{
	enum DrawKind { PLAY_A = 1, PLAY_B = 2, DRAW_SPIKE = 3, PLAY_C = 4, PARTS = 5, SPR = 6 };
	static void held_note_root();
	static void held_note_main(int kind, TaskNode *n);
	static void held_note_particles(TaskNode *n);
	static void held_note_sprite(TaskNode *n);
}
	static void register_mag318_held();
}
