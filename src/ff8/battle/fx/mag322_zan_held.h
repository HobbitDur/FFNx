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

// 30 fps part of mag322_zan.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace zan322
{
	static void held_note_master();
	static void held_note_end();
	static void held_note_blade(TaskNode *n);
	static void held_note_trail(TaskNode *n);
	static void held_note_hit(TaskNode *n);
	static void held_note_streak(TaskNode *n);
	static void held_note_dust(TaskNode *n);
}
	static void register_mag322_held();
}
