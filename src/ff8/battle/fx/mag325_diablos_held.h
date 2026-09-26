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

// 30 fps part of mag325_diablos.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace d325
{
	static void held_note_master();
	static void held_note_timeline(TimelineNode *t);
	static void held_note_play_a(const PrimCtx &ctx);
	static void held_note_grow(uint32_t e);
	static void held_note_morph(uint32_t e);
	static void held_note_flip(int32_t x, int32_t y, int32_t z, uint32_t model);
	static void held_note_play_c(const PrimCtx &ctx);
	static void held_note_restart_c(TimelineNode *t);
	static void held_note_play_b(const PrimCtx &ctx, bool b406);
	static void held_note_sink(int32_t k);
	static void held_note_well(uint32_t e);
	static void held_note_tile(int32_t e);
	static void held_note_e1(uint8_t *e1, int kind);
	static void held_note_e2();
	static void held_note_particles(int mode);
	static void held_note_rising(RisingNode *p);
}
	static void register_mag325_held();
}
