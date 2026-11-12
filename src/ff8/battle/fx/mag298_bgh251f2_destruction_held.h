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

// 30 fps part of mag298_bgh251f2_destruction.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace destruct298
{
	enum DrawKind { BODY = 1, FRAGS = 2, MARKS = 3, SPRITE = 4, LAYOUT = 5 };
	static void held_note_root();
	static void held_note_body(const MasterNode *m);
	static void held_note_frags(const MasterNode *m);
	static void held_note_marks(const MasterNode *m);
	static void held_note_sprite(const SpriteNode *s);
	static void held_note_layout(LayoutNode *L, const PrimArg *arg);
}
	static void register_mag298_held();
}
