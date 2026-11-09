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

// 30 fps layer of fx_sprite.cpp (the generic effect sprites of the hit-effect queue): declarations used by its FX_HELD
// seams. The drawing is in fx_sprite_held.inc.

#pragma once

#include "fx_held.h"

namespace ff8fx
{
	static void register_fx_sprite_held();

namespace sprite
{
	// a sprite task `node` (original function orig) is about to draw on this real tick of the hit-effect queue
	static void held_note_draw(uint32_t orig, const void *node);
	// ... and has drawn (before its paused test and its update)
	static void held_note_drawn(const void *node);
}
}
