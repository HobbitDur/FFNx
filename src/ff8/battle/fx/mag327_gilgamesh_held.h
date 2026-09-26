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

// 30 fps part of mag327_gilgamesh.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace g327
{
	// model memo draw kinds (ModelMemoTake)
	enum { DK_NORMAL4, DK_NORMAL3, DK_CLIP, DK_MATERIAL };

	static void held_note_master();
	static void MemoNode(const Node24 *n);
	static void MemoA(int i, const void *owner);
	static void MemoB(int i, const void *owner);
	static void held_note_trail(int32_t cnt);
	static void held_note_trail_splines();
	static void held_note_tip(const Node24 *n, const uint8_t *s);
	static void ModelMemoTake(uint8_t *E, const void *owner, int kind, int32_t threshold);
	static void held_note_advanced(uint8_t *E);
}
	static void register_mag327_held();
}
