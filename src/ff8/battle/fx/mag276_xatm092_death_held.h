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

// 30 fps part of mag276_xatm092_death.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace xatm276
{
	static void held_note_root();
	static void held_note_arc(uint32_t n, uint32_t points, uint32_t residue);
	static void held_note_prim(uint32_t n, const PrimArg *a);
	static void held_note_sprite(uint8_t kind, uint32_t n);
	static void held_note_director(uint32_t n);
}
	static void register_mag276_held();
}
