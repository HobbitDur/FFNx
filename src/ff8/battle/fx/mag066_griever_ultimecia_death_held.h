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

// 30 fps part of mag066_griever_ultimecia_death.cpp: declarations used by its FX_HELD(...) seams
// (the held-frame code is in mag066_griever_ultimecia_death_held.inc)

#pragma once

#include "fx_held.h"

namespace ff8fx
{
namespace gu066
{
	static void held_note_root(uint32_t n);
	static void held_note_pool(uint32_t fn, uint32_t n, uint32_t base, uint32_t pool);
	static void held_note_task(uint32_t fn, uint32_t n);
	static void held_note_melt(uint32_t n, uint32_t h);
	static void held_note_stage(uint32_t n, uint32_t h);
}
	static void register_mag066_held();
}
