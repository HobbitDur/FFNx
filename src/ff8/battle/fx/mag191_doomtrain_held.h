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

// 30 fps part of mag191_doomtrain.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace d191
{
	static void held_note_master();
	static void held_note_camscript(CamNode *n, uint32_t r);
	static void MemoNode(const Node24 *n);
	static void held_note_rec_a(int i, const void *owner, const Rec *r);
	static void held_note_rec_b(int i, const void *owner, const Rec *r);
	static void held_note_rec_c(int i, const void *owner, const Rec *r);
	static void held_note_rec_d(int i, const void *owner, const Rec *r);
	static void held_note_rec_e(int i, const void *owner, const Rec *r);
	static void held_note_rec_f(int i, const void *owner, const Rec *r);
	static void CreatureMemoTake();
	static void held_note_glow(const Node24 *n, int32_t fade);
}
	// held-frame camera of the shared camera-script task 0x63E9C0 (registered by fx_port.cpp for
	// every ported effect that queues it)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);
	static void register_mag191_held();
}
