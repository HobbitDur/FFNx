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

// 30 fps part of mag001_cure.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"
#include "fx_glint_held.h"

namespace ff8fx
{
namespace cure001
{
	static void held_note_root(const RootNode *r);
	static void held_note_ring(const Particle *r);
	static void held_note_particle(const Particle *p);
}
	static void register_mag001_held();
}
