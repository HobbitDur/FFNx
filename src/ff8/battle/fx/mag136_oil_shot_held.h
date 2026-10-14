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

// 30 fps part of mag136_oil_shot.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace oil136
{
	enum DrawKind { SPR_SPRAY = 1, SPR_SPLAT = 2, PLAY_MOUTH = 3, DRAW_MESH = 4, PLAY_HIT = 5, PLAY_POOL = 6, PLAY_STREAM = 7, PLAY_FALL = 8 };
	static void held_note_master();
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const int16_t *pos);
	static void held_note_landing(TaskNode *n);
	static void held_note_mesh(TaskNode *n);
	static void held_note_sprite(int kind, TaskNode *n);
}
	static void register_mag136_held();
}
