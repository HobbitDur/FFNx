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

// 30 fps part of mag030_counter_laser_eye.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // GTE register windows of the held frames (magc::GTE_DATA / GTE_CTRL)

namespace ff8fx
{
namespace act
{
namespace lasereye
{
	static void held_note_system(uint32_t sys);
	static bool held_laser_draw(uint32_t node, uint32_t matrix);
	static void held_note_spark(uint32_t node);
}
}
	static void register_mag030_held();
}
