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

// 30 fps part of mag137_oil_blast.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace oil137
{
	enum DrawKind { SPR_SPRAY = 1, DRAW_CHUNK = 2, PLAY_PUDDLE1 = 3, PLAY_PUDDLE2 = 4, PLAY_SPOUT = 5, DRAW_MESH = 6, PLAY_BLAST = 7 };
	static void held_note_master();
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l);
	static void held_note_mesh(TaskNode *n);
	static void held_note_chunk(TaskNode *n);
	static void held_note_spray(TaskNode *n);
}
	static void register_mag137_held();
}
