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

// 30 fps part of mag032_protect.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // GTE register windows of the held frames (magc::GTE_DATA / GTE_CTRL)

namespace ff8fx
{
namespace act
{
namespace protect
{
	// real tick: a screen flash draws (its colour is at node +0x34)
	static void held_note_flash(uint32_t node);
	// real tick: a particle draws (held frame: its replay takes its header record)
	static void held_note_particle(uint32_t node);
	// real tick: a shield player is about to play its model (parameter block built)
	static void held_note_shield(uint32_t node);
	// the shield draw callback runs (real tick: a new header record; held frame: the next one)
	static void held_note_callback();
	// a draw header was allocated on the scratch stack: real tick = record its scratch bytes,
	// held frame = put the recorded bytes back under it
	static void held_header(uint32_t hdr, int size);
}
}
	static void register_mag032_held();
}
