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

// 30 fps part of mag269_gargantua_269.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace gargantua269
{
	// draw 22 EntityDisintegrate (gfc_disintegrate.inc / gfc_disintegrate_held.inc)
	static void dh_22_EntityDisintegrateHeld();
	static void DrawPiecesHeld();
	static void DotsHeld();
	static void NoteLightN();
	// draw 47 PaletteHsvShift (gfc_palshift.inc / gfc_palshift_held.inc)
	static void PalShiftNote();
	static void PalShiftRestore();
}
}
	static void register_mag269_gargantua_269_held();
}
