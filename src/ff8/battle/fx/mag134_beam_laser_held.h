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

// 30 fps part of mag134_beam_laser.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace beam134
{
	enum DrawKind { SPR_GLOW = 1, SPR_SPARK = 2, PLAY_CHARGE0 = 3, PLAY_CHARGE1 = 4, PLAY_MUZZLE = 5, PLAY_RAY = 6, PLAY_IMPACT = 7, PLAY_HIT = 8, DRAW_BEAM = 9 };
	static void held_note_master();
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const void *);
	static void held_note_beam(TaskNode *n);
	static void held_note_sprite(int kind, TaskNode *n, const int16_t *pos, int16_t counter);
}
	static void register_mag134_held();
}
