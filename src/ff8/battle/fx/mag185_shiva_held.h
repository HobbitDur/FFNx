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

// 30 fps part of mag185_shiva.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace s185
{
	static void held_note_master();
	static void held_note_rec_a0(int i, const void *owner, const Rec *r);
	static void held_note_rec_a4(int i, const void *owner, const Rec *r);
	static void held_note_node(const Node24 *n);
	static void held_note_ring_timer(int32_t arg);
	static void held_note_shatter_recs();
	static void held_note_mount_recs();
	static void held_note_block0_recs();
	static void held_note_block1_recs();
	static void held_note_creature(bool shell);
	static void held_note_shell_next(int32_t c, const Node24 *n);
	static void held_note_creature_advanced();
	static void held_note_creature_next(const Node24 *n);
}
	static void register_mag185_held();
}
