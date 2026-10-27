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

// 30 fps part of mag110_biggs_wedge_death.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"
#include <math.h>

namespace ff8fx
{
namespace bw110
{
	enum { K_STREAM = 1, K_SPARK, K_HIT, K_DEBRIS, K_RING };
	static void held_note_root();
	static void held_note(int kind, const void *node, int size);
	static void held_note_stream(const StreamNode *s);
	static void held_note_aim(const StreamNode *s, const int16_t *aim);
}
	static void register_mag110_held();
}
