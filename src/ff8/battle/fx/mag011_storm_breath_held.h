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

// 30 fps part of mag011_storm_breath.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"
#include <math.h>

namespace ff8fx
{
namespace storm011
{
	// what a draw-log record redraws
	enum HeldKind { K_STREAM, K_MOUTH, K_CONE, K_SPARK, K_MIST, K_HIT, K_PUFF, K_DEBRIS, K_RING };
	static void held_note_root();
	static void held_note(int kind, const void *node, int size);
	static void held_note_stream(const TargetNode *t);
	static void held_note_aim(const int16_t *p0, const int16_t *p1);
	static void held_note_play(int kind, const TargetNode *t, const PrimArg *arg);
}
	static void register_mag011_held();
}
