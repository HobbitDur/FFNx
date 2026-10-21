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

// 30 fps layer of mag165_lion_heart.cpp (Lion Heart): declarations used by its FX_HELD seams.

#pragma once

#include "fx_held.h"

namespace ff8fx
{
	static void register_mag165_held();
	// held-frame camera of the shared camera-script task 0x63E9C0 (mag191_doomtrain_held.inc)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);

namespace lion165
{
	static void held_note_root();
	static void held_note_trail(const Node24 *n);
	static void held_note_node(uint32_t fn, const Node24 *n);
	static void held_note_pool(uint32_t fn, const Node24 *n, const uint8_t *h);
	static void held_note_sink(const Node24 *n, const uint8_t *h);
}
}
