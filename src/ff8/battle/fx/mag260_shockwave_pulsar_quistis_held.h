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

// 30 fps part of mag260_shockwave_pulsar_quistis.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace pulsar260
{
	// draw kinds of the real tick's log
	enum { K_CENTRE, K_WAVE, K_LIFT, K_RISE, K_RING, K_TILE, K_MODEL, K_SPARKS, K_SHARDS };
	static void held_note_master(const MasterNode *m);
	static void held_note_play(int kind, const void *node, const PrimArg *arg, int32_t counter);
	static void held_note_tile(int32_t w);
	static void held_note_model(const SubNode *rise, const Target *rec, uint8_t *e, const int16_t *vec);
	static void held_note_sparks();
	static void held_note_shards();
}
	static void register_mag260_held();
}
