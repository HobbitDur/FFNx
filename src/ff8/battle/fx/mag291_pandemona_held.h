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

// 30 fps part of mag291_pandemona.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace p291
{
	struct Pass;
	struct ParB;
	struct ParC;
	struct ParD;
	struct ParJ;
	struct ParK;
	static void held_note_master();
	static void held_note_timeline(uint8_t *nd, int32_t c);
	static void held_note_trail(uint8_t *nd);
	static bool held_play(const Pass &p, prim::Layout *l, uint8_t *ctx);
	static void held_lerp(const Pass &p, bool next_in, int32_t &x, int32_t x1);
	static void held_par_b(const Pass &p, int32_t v, ParB &r);
	static void held_par_c(const Pass &p, int32_t v, ParC &r);
	static void held_par_d(const Pass &p, int32_t v, ParD &r);
	static void held_par_j(const Pass &p, int32_t v, ParJ &r);
	static void held_par_k(const Pass &p, int32_t v, ParK &r);
}
	static void register_mag291_held();
}
