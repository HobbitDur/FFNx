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

// 30 fps part of mag187_odin.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace o187
{
	struct PoolMemo;
	struct TrailMemo;
	static void held_note_master();
	static void MemoNode(const Node24 *n);
	static PoolMemo *MemoRecord(int pool, int i, const void *owner, const Rec *r);
	static void MemoDied(PoolMemo *pm);
	static void CreatureMemoTake();
	static void held_note_creature_next(const Node24 *n);
	static void held_note_flash(const Node24 *n, int32_t level, int32_t otz);
	static void held_note_mist(const Node24 *n, const uint8_t *s);
	static void held_note_blade(const Node24 *n);
	static TrailMemo *held_trail_put(const Node24 *n);
	static void held_trail_drawn(TrailMemo *m);
}
	static void register_mag187_held();
}
