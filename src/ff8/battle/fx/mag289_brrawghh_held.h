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

// 30 fps part of mag289_brrawghh.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace brr289
{
	enum DrawKind { SPRITE = 1, TARGET = 2, SPLASH = 3, TUBE = 4 };
	static void held_note_master();
	static void held_note_tube(TaskNode *n, int k, const int16_t *B, const int16_t *P);
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l);
	static void held_note_sprite(TaskNode *n);
}
	static void register_mag289_held();
}
