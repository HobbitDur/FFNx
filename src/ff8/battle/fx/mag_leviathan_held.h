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

// 30 fps part of mag_leviathan.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace part_lev_b
{
	static void HeldBegin(uint8_t *blk);
	static void HeldEnd();
	static void held_note_vertex(uint8_t *v);
	static bool held_mid24_pose(uint8_t *blk);
	static void held_pose20(uint8_t *hdr, uint8_t *cmd);
	static void held_begin24(uint8_t *cur);
	static void held_end24();
}
}
	static void register_gfc_leviathan_held();
}
