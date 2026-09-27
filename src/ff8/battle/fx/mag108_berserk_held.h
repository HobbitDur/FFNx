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

// 30 fps part of mag108_berserk.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace berserk108
{
	static void held_note_root();
	static void held_note_flash(const MainNode *m, int32_t e);
	static void held_note_glow(const MainNode *m, int32_t e);
	static void held_note_play(const MainNode *m, const PrimArg *arg);
	static void held_note_rays(const MainNode *m);
	static void held_note_swirl(const SwirlNode *s);
	static void held_note_particle(const ParticleNode *p);
}
	static void register_mag108_held();
}
