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

// 30 fps part of mag116_quezacotl.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace q116
{
	struct DebrisNode;
	struct ArcNode;
	static void held_note_master();
	static void held_note_debris(DebrisNode *p);
	static void held_note_arc(ArcNode *arc);
	static void held_note_arc_jitter(int i, const int32_t *rv);
	static void held_creature_begin();
	static void held_note_materialise(int32_t v3, int32_t v4, int32_t acc);
	static void held_note_orbit();
	static void held_note_uncomposed(const uint8_t *m);
	static void held_note_play(prim::Layout *l, uint32_t cb, void *arg);
}
	static void register_mag116_held();
}
