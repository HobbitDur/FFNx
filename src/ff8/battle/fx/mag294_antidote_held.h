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

// 30 fps part of mag294_antidote.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace antidote294
{
	static void held_note_root();
	static void held_note_play(prim::Layout *layout, const int16_t *anchor);
}
namespace statusitem
{
	template<>
	struct Held<antidote294::LAYOUT>
	{
		static void note_root() { antidote294::held_note_root(); }
		static void note_play(prim::Layout *layout, const int16_t *anchor) { antidote294::held_note_play(layout, anchor); }
	};
}
	static void register_mag294_held();
}
