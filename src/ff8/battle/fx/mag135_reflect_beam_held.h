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

// 30 fps part of mag135_reflect_beam.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace reflect135
{
	enum DrawKind { SPR_BURST = 1, SPR_GLINT = 2, SPR_SPARK = 3, PLAY_SWEEP = 4, DRAW_SWEEP = 5, PLAY_SHOT = 6, DRAW_SHOT = 7, PLAY_SHOTHIT = 8, PLAY_REFLECT = 9 };
	static void held_note_master();
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l);
	static void held_note_sweep(TaskNode *n);
	static void held_note_shot_beam(TaskNode *n);
	static void held_note_sprite(int kind, TaskNode *n);
}
	static void register_mag135_held();
}
