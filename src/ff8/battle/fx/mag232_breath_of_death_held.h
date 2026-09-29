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

// 30 fps part of mag232_breath_of_death.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
	static void register_mag232_breath_of_death_held();
namespace gfc
{
namespace death232
{
	// draw 44: the engine scratch cells RenderGeometry writes (0x1D99C18..0x1D99C1F, set by
	// ParsePolygons 0x50FDF0) are put back after a held draw
	static void held_render_begin();
	static void held_render_end();
	// 0xA5D580 (scaled parent transform of draws 2 / 44): the scale words ws+0xD4.. on a held frame
	static void held_scale_fraction(const uint8_t *bone, uint8_t *scale3);
}
}
}
