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

// 30 fps part of mag069_griever.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace g069
{
	static void held_note_master();
	static void held_note_timeline(TimelineNode *node, int32_t c);
	static void held_note_prim(const PrimCtx &ctx);
	static void held_note_attacker();
	static void held_note_models(int32_t thr);
	static void held_note_sparks();
	static void held_note_shards();
	static void held_note_fade(int32_t t);
	static void held_note_advanced();
}
	static void register_mag069_held();
}
