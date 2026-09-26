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

// 30 fps part of mag105_thundaga.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace thundaga105
{
	struct BoltNode;
	struct BoltArg;
	static void held_note_root();
	static void held_note_debris();
	static void held_note_bolt(const BoltNode *p, const BoltArg *arg);
	static void held_note_capture(const int16_t *anchor, const int16_t *box);
	static void held_note_draw(uint32_t fn, const void *node, uint32_t size);
	static uint16_t held_part_frame(uint16_t frame);
}
	static void register_mag105_held();
}
