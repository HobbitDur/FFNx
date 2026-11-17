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

// 30 fps part of mag167_zantetsuken.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace z167
{
	struct PoolMemo;
	struct TrailMemo;
	static void held_note_master();
	static void MemoNode(const Node24 *n);
	static PoolMemo *MemoRecord(const Node24 *n, int i, const Rec *r);
	static void MemoDied(PoolMemo *pm);
	static TrailMemo *held_trail_put(const Node24 *n);
	static void held_trail_drawn(TrailMemo *m);
}
	static void register_mag167_held();
}
