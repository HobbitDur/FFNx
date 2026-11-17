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

// 30 fps part of mag210_bloodfest.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace blood210
{
	static void held_note_master();
	static void held_note_main(const MainNode *mn);
	static void held_note_draw(uint32_t fn, const void *node);
	static void held_note_layout(MainNode *mn, int site, uint8_t *layout, const PlayArg *arg, int32_t x, int32_t z);
	static void held_note_ghosts(uint8_t *entity, const uint8_t *g);
	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off);
	static void held_note_capture(const int16_t *a, const int16_t *b);
	static void held_note_three(const int16_t *c, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6);
	static void held_note_bone(int32_t i);
	static void held_note_bone_point(const int16_t *sp);
	static void held_note_tile(int32_t t);
	static void held_note_trail(const TrailNode *t);
	static void held_note_task_layout(const LayoutNode *t, const PlayArg *arg);
}
	static void register_mag210_held();
}
