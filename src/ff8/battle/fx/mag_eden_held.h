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

// 30 fps part of mag_eden.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace part_eden_a
{
	static bool held_fade_colour(uint32_t col, int32_t t, int32_t fv, bool frac, uint32_t *out);
	static void held_entity60_save(uint8_t *E);
	static void held_entity60_restore();
}
namespace part_eden_c
{
	static void dh_53_PixelParticleEmitterHeld();
	static uint8_t *held_local_env();
	static uint8_t *held_packet_block();
	static void held_scratch55_save();
	static uint16_t held_phase55(uint8_t *pool, uint8_t *desc, int k);
	static bool held_skip56();
	static void held_scratch56_save(uint8_t *desc);
	static void held_scratch_restore();
}
namespace part_eden_d
{
	static void dh_72_Held();
}
}
	static void register_gfc_eden_held();
}
