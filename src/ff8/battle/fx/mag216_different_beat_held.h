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


// 30 fps part of mag216_different_beat.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace beat216
{
	static void held_note_root(RootNode *r);
	static void held_note_director(DirectorNode *d);
	static void held_note_play(int site, uint8_t *layout, const PlayArg *arg, const Mat4x3 *frame);
	static void held_note_twist(const int16_t *p, int32_t scale, int32_t angle, bool last);
	static void held_note_tile(int32_t f);
	static void held_note_ghosts(uint8_t *entity, uint8_t *g);
	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off, const int32_t *scale);
	static void held_note_capture(const int16_t *pos, const int16_t *box);
	static void held_note_ring(RingNode *k);
	static void held_note_rock(const RockNode *k);
	static void held_note_sprite(const SpriteNode *s, uint32_t seq);
}
	static void register_mag216_held();
}
