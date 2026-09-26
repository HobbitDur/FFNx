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

// 30 fps part of mag338_moomba.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace m338
{
	static void held_note_master();
	static void held_note_prim(PrimNode *p);
	static void held_note_debris(DebrisNode *d);
	static void held_note_debris_ended();
	static void held_note_creature(CreatureNode *cn);
	static void held_note_ring(const Mat4x3 &root, int32_t angle, int32_t fade);
	static void held_note_model(CreatureNode *cn);
	static void held_note_built(CreatureNode *cn);
	static void held_note_sparks();
}
	static void register_mag338_held();
}
