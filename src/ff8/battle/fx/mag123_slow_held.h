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

// 30 fps part of mag123_slow.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace slow123
{
	static void held_note_root();
	static void held_note_play(uint8_t *layout, const layouts::FrameArg *local, const uint8_t *entity, const int32_t *point);
}
namespace clockspell
{
	template<>
	struct Held<slow123::LAYOUT>
	{
		static void note_root() { slow123::held_note_root(); }
		static void note_play(uint8_t *layout, const FrameArg *local, const uint8_t *entity, const int32_t *point) { slow123::held_note_play(layout, local, entity, point); }
	};
}
	static void register_mag123_held();
}
