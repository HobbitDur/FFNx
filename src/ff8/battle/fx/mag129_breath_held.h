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

// 30 fps part of mag129_breath.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace breath129
{
	enum PlayKind { PLAY_MOUTH = 1, PLAY_MOUTH3 = 2, PLAY_TARGET = 3 };
	static void held_note_master();
	static void held_note_play(prim::Layout *l, int kind, const TargetNode *t);
	static void held_note_restart(prim::Layout *l);
	static void held_note_mesh(const MainNode *m, int32_t c1e);
	static void held_note_sprite(const SpriteNode *p);
	static void held_note_branch(const BranchNode *b, uint32_t slot);
	static void held_note_arc(const ArcNode *arc);
	static void held_note_arc_jitter(int i, const int32_t *rv);
}
	static void register_mag129_held();
}
