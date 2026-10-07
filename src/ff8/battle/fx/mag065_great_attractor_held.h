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

// 30 fps part of mag065_great_attractor.cpp: declarations used by its FX_HELD(...) seams
// (the held-frame code is in mag065_great_attractor_held.inc)

#pragma once

#include "fx_held.h"

namespace ff8fx
{
namespace ga065
{
	static void held_note_root();
	static void held_note_backdrop(int kind, int32_t step, int32_t size);
	static void held_note_mirrored(MasterNode *m, bool roty);
	static void held_note_caster_layout(MasterNode *m, int32_t step, bool scroll, int32_t len);
	static void held_note_beams(int32_t step, int32_t a, int32_t b);
	static void held_note_rays(int32_t step, int32_t t, int32_t u);
	static void held_note_rings(int32_t step, const int16_t *centre, int32_t size);
	static void held_note_tile(int32_t t, int32_t in, int32_t hold, int32_t out, int32_t len);
	static void held_note_overlay(MasterNode *m, int32_t step);
	static void held_note_particles(int which);
	static void held_note_layout(LayoutNode *L);
}
	static void register_mag065_held();
}
