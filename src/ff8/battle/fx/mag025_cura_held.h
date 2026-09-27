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

// 30 fps part of mag025_cura.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // held-frame helpers (magc::HeldScope, GTE register windows)

namespace ff8fx
{
namespace act
{
namespace cura
{
	static void held_note_master();
	static void held_note_sprite(uint32_t node);
	static void held_note_ground(uint32_t node, uint32_t seq, uint32_t colour);
	static void held_note_spin(uint32_t node, uint32_t seq, uint32_t colour, uint32_t angle, uint32_t scale);
	static void held_note_play(uint32_t node, uint32_t frame);
	static void held_note_played(uint32_t node);
	static void held_note_overlay(uint32_t node);
	static void held_note_overlay_end();
}
}
	static void register_mag025_held();
}
