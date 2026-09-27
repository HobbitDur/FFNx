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

// 30 fps part of mag035_life.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"
#include "mag_common.h" // magc::HeldScope, GTE register windows (mag_common_held.h)
#include "fx_glint_held.h" // glint::held_draw (shared glint task 0x8DCC20)

namespace ff8fx
{
namespace act
{
namespace life
{
	static void held_note_master(uint32_t node);
	static void held_note_sprite(uint32_t task, uint32_t node);
}
}
	static void register_mag035_held();
}
