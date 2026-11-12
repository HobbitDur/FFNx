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

// 30 fps part of mag265_sphinxara_265.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace sphinxara265
{
	// draw 52 EntityMelt / draw 3 AnimatedModelWithShadow (gfc_melt.inc / gfc_melt_held.inc)
	static void dh_52_EntityMeltHeld();
	static void MeltNoteFlags();
	static void MeltPoseHeld(uint8_t *hdr);
	static void held_note_mid03();
	static bool held_shadow03(uint8_t *hdr, uint8_t *blk, int32_t otspec, int32_t y);
}
}
	static void register_mag265_sphinxara_265_held();
}
