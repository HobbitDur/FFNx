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

// 30 fps part of mag112_meteor_barret.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace meteor112
{
	static void held_note_root();
	static void held_note_layout(const DirectorNode *d, int site, const PlayArg *arg);
	static void held_note_tile(int32_t r, int32_t g, int32_t b, int32_t n, int32_t d);
	static void held_note_column(const DirectorNode *d, int32_t k, const int16_t *spawn);
	static void held_note_flat();
	static void held_note_smoke(const SmokeNode *s);
	static void held_note_spark(const SparkNode *s);
	static void held_note_debris(const DebrisNode *d);
}
	static void register_mag112_held();
}
