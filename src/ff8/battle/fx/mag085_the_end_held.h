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

// 30 fps part of mag085_the_end.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // GTE register windows of the held frames (magc::GTE_DATA / GTE_CTRL)

namespace ff8fx
{
namespace act
{
namespace theend
{
	// the kinds of draw the real tick notes (in execution order)
	enum { HK_PLAY = 1, HK_SHADOW, HK_MODEL, HK_GROUND, HK_GLOW, HK_BARS, HK_SYSTEM, HK_TINT };
	// a draw of `node` (kind HK_*) is about to happen
	static void held_note_draw(int kind, uint32_t node);
	// an actor system (state block `sys`) is about to draw (after its update pass)
	static void held_note_system(uint32_t sys);
	// the master is about to run the screen-tint queue `queue` (its tints that draw on this tick)
	static void held_note_tints(uint32_t queue);
	// the master ran its queues (the end of the module's draws on this tick)
	static void held_note_end();
}
}
	static void register_mag085_held();
}
