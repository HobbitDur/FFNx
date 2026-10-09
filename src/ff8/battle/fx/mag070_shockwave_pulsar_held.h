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

// 30 fps part of mag070_shockwave_pulsar.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace sp070
{
	// draw kinds of the real tick's log
	enum { K_CENTRE, K_WAVE, K_LIFT, K_RISE, K_RING, K_TILE, K_MODEL, K_RMODEL, K_SPARKS, K_SHARDS };
	static void held_note_root();
	static void held_note_play(int kind, const void *node, const PrimArg *arg, int32_t counter);
	static void held_note_tile(int32_t level, int32_t counter);
	static void held_note_model(int kind, const Target *rec, const int16_t *vec, int32_t scale, int32_t counter, int32_t len);
	static void held_note_sparks(const MasterNode *m);
	static void held_note_shards(const MasterNode *m);
}
	static void register_mag070_held();
}
