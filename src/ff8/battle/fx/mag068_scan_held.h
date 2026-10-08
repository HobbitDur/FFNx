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

// 30 fps part of mag068_scan.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace scan068
{
	// what a recorded draw is (the draw code a held frame replays)
	enum : uint32_t
	{
		REC_BRACKET = 1, REC_BRACKET_FOLLOW, REC_RING1, REC_RING2, REC_RING3, REC_RETICLE, REC_RETICLE_CLOSE,
		REC_PAIR, REC_MORPH, REC_STATBAR, REC_STATIC, REC_ALTSPINNER, REC_SPRITE
	};
	static void held_note_root(const RootNode *r);
	static void held_note(uint32_t kind, const void *node);
	static void held_note_camera(const CtlNode *n);
}
	static void register_mag068_held();
}
