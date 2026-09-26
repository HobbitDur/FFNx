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

// 30 fps part of mag107_demi.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace demi107
{
	static void held_note_root();
	static void held_note_node(uint32_t fn, const void *node);
	static void held_note_burst(const TargetNode *t, uint32_t seq, int32_t e);
	static void held_note_silhouette(const TargetNode *t, int32_t step);
	static void held_note_play(const TargetNode *t, const PrimArg *arg);
}
	static void register_mag107_held();
}
