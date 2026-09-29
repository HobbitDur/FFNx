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

// 30 fps part of mag012_blade_shot.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace blade012
{
	// what a draw-log record redraws
	enum HeldKind { K_BLADE, K_HIT, K_FLASH, K_RING, K_WING_A, K_WING_B, K_CATCH, K_PART, K_STREAK, K_TRAIL };
	static void held_note_root(RootNode *n);
	static void held_note(int kind, const void *node, int size);
	static void held_note_h(int kind, const RootNode *n, const uint8_t *h, int hsize, int32_t k);
	static void held_note_k(int kind, const RootNode *n, int32_t k);
}
	static void register_mag012_held();
}
