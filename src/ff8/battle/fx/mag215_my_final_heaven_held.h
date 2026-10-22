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

// 30 fps part of mag215_my_final_heaven.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace heaven215
{
	static void held_note_root(RootNode *r);
	static void held_note_director(DirectorNode *d);
	static void held_note_play(DirectorNode *d, int site, uint8_t *layout, const ArgA *a, const ArgB *b);
	static void held_note_debris(const Debris *k);
	static void held_note_world(uint8_t *e);
	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off);
	static void held_note_ring(RingNode *k);
}
	static void register_mag215_held();
}
