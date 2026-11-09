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

// 30 fps part of mag286_mini_pulse_cannon.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace minipulse286
{
	enum DrawKind { PLAY_MUZZLE = 1, PLAY_BEAM = 2, PLAY_IMPACT = 3, PLAY_GROUND = 4, SPR_SPARK = 5 };
	static void held_note_master();
	static void held_note_main(const MainNode *m, int kind, const PrimArg *arg);
	static void held_note_spark(const SparkNode *s);
}
	static void register_mag286_held();
}
