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

// 30 fps part of mag279_mega_spark.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace megaspark279
{
	enum DrawKind { REC_BOLT = 1, REC_RING = 2, REC_ORB = 3, REC_ARC = 4, REC_PRIM = 5, REC_SPARK = 6 };
	static void held_note_master();
	static void held_note_bolt(const BoltNode *b);
	static void held_note_main(const MainNode *m, int kind, int ring, const PrimArg *arg);
	static void held_note_arc(const ArcNode *a, uint32_t points, uint32_t residue);
	static void held_note_prim(const PrimNode *p, const PrimArg *arg);
	static void held_note_spark(const SparkNode *s);
	static void register_mag279_held();
}
}
