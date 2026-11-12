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

// 30 fps part of mag320_dark_flare.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace flare320
{
	enum DrawKind { PLAY_1 = 1, PLAY_1L = 2, PLAY_2 = 3, PLAY_3 = 4, SPARK = 5 };
	static void held_note_root();
	static void held_note_main(int kind, TaskNode *n);
	static void held_note_spark(TaskNode *n);
}
	static void register_mag320_held();
}
