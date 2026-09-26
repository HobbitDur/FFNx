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

// 30 fps part of mag149_ultima.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace ultima149
{
	struct PartNode;
	static void held_note_root();
	static void held_note_draw(uint32_t fn, const PartNode *node);
	static int32_t held_ramp(int32_t v, int32_t frame, int32_t slope, int32_t offset);
	static int32_t held_frame_arg(int32_t t, int32_t frame, int shift, int32_t duration);
}
	static void register_mag149_held();
}
