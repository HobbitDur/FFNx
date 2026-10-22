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
// 30 fps part of mag091_angelo_rush.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // GTE register windows of the held frames (magc::GTE_DATA / GTE_CTRL)

namespace ff8fx
{
namespace act
{
namespace angelo91
{
	// the kinds of draw the real tick notes (in execution order)
	enum { HK_ANGELO = 1, HK_PLAY, HK_RING, HK_MODEL, HK_SYSTEM, HK_FLASH };
	// a draw of `node` (kind HK_*) is about to happen
	static void held_note_draw(int kind, uint32_t node);
	// the dash play of `node` is about to play (its block is built)
	static void held_note_play(uint32_t node);
	// an actor system (state block `sys`) is about to draw (after its update pass)
	static void held_note_system(uint32_t sys);
	// the master is about to run the queue of the screen flash (engine task 0x8DDEC0)
	static void held_note_flash(uint32_t queue);
	// the master ran its queues (the end of the module's draws on this tick)
	static void held_note_end();
}
}
	static void register_mag091_held();
}
