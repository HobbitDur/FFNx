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

// 30 fps part of mag278_carbuncle.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace c278
{
	static void held_note_master();
	static void held_note_sparkle(SparkleNode *p);
	static void held_note_shard(ShardNode *p);
	static void held_note_orbit(OrbitNode *o);
	static void held_note_ring(RingNode *r);
	static void held_note_beam(BeamNode *b, const int16_t *out);
	static void held_note_ruby(RubyNode *r);
	static void held_note_creature(CreatureNode *cn);
	static void held_note_model(CreatureNode *cn);
	static void held_note_dome();
	static void held_note_prim_stage(int stage);
}
	static void register_mag278_held();
}
