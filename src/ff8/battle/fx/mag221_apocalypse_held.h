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

// 30 fps part of mag221_apocalypse.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace apocalypse221
{
	// draw 50 ObjectShatter: the piece at the in-between of its step
	static uint8_t *PieceHeld(uint8_t *p, uint8_t *buf);
	static int32_t PieceHeldAge(const uint8_t *p);
	// draw 69 EntityTint: the entity words of the real tick / of a held frame
	static void TintNote();
	static void TintRestore();
}
}
	static void register_mag221_apocalypse_held();
}
