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

// 30 fps part of mag290_trauma_shatter.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace shatter290
{
	enum DrawKind { PLAY_A = 1, PLAY_B = 2, PLAY_C = 3, UNDER = 4, BODY = 5, FRAGS = 6, MARKS = 7, SPRITE_A = 8, SPRITE_B = 9, LAYOUT = 10 };
	static void held_note_root();
	static void held_note_play(const MasterNode *m, int kind, const PrimArg *arg);
	static void held_note_under(const MasterNode *m, const int16_t *pos);
	static void held_note_body(const MasterNode *m);
	static void held_note_frags(const MasterNode *m);
	static void held_note_marks(const MasterNode *m);
	static void held_note_sprite(const SpriteNode *s, int kind);
	static void held_note_layout(LayoutNode *L, const PrimArg *arg);
}
	static void register_mag290_held();
}
