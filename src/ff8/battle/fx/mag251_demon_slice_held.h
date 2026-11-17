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

// 30 fps part of mag251_demon_slice.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace demon251
{
	static void held_note_master(const TaskNode *master);
	static void held_note_main(const MainNode *mn);
	static void held_note_draw(uint32_t fn, const void *node);
	static void held_note_layout(MainNode *mn, int site, uint8_t *layout, const PlayArg *arg, int32_t x, int32_t y, int32_t z);
	static void held_note_tile(int32_t t);
	static void held_note_target(const MainNode *mn, int32_t i, const int16_t *off);
	static void held_note_trail(const TrailNode *t);
	static void held_note_task_layout(const LayoutNode *t, const PlayArg *arg);
}
	static void register_mag251_held();
}
