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

// 30 fps part of mag084_angel_wing.cpp (declarations for its FX_HELD statements)
#pragma once
#include "act_engine_held.h"
#include "mag_common.h" // GTE register windows of the held frames (magc::GTE_DATA / GTE_CTRL)

namespace ff8fx
{
namespace act
{
namespace angelwing
{
	// the kinds of draw the real tick notes (in execution order): the prim plays (0x78EDB0), the model container (0x796390),
	// glow A (0x797080), glow B (0x797330), the feather actor systems
	enum { HK_PLAY = 1, HK_MODEL, HK_GLOW, HK_MOON, HK_SYSTEM };
	// a draw of `node` (kind HK_*) is about to happen
	static void held_note_draw(int kind, uint32_t node);
	// an actor system (state block `sys`) is about to draw (after its update pass)
	static void held_note_system(uint32_t sys);
	// the master ran its queues (the end of the module's draws on this tick)
	static void held_note_end();
}
}
	static void register_mag084_held();
}
