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

// 30 fps part of mag324_tonberry_king_death.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace tonk324
{
	enum DrawKind { SPARKS_A = 1, SPARKS_B = 2, GHOST = 3, SPIN = 4 };
	static void held_note_root();
	static void held_note_sparks(TaskNode *n, int which);
	static void held_note_ghost(TaskNode *n);
	static void held_note_spin(TaskNode *n);
}
	static void register_mag324_held();
}
