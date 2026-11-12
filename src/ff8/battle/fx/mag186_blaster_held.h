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

// 30 fps part of mag186_blaster.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace blaster186
{
	enum DrawKind { REC_BEAM = 1, REC_RING, REC_BOLT, REC_SPARKS, REC_GLOW, REC_WAVE, REC_SPLASHES, REC_FADE };
	static void held_note_root();
	static void held_note_node(int kind, uint32_t n);
	static void held_note_parts(int kind, uint32_t n);
	static void held_note_particle(uint32_t p);
	static void register_mag186_held();
}
}
