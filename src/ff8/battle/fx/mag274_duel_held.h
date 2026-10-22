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

// 30 fps part of mag274_duel.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace duel274
{
	static void held_note_root();
	static void held_note_layout(MoveNode *m, int site, uint32_t layout, const PlayArg *arg);
	static void held_note_ghosts(uint8_t *entity, const uint8_t *g);
	static void held_note_bars(uint8_t level, bool fading);
	static void held_note_wave(MoveNode *m, uint32_t v, const Mat4x3 *b);
	static void held_pillar_angle(int32_t angle);
	static void held_note_pillar(const Mat4x3 *w, const int16_t *pos, int16_t counter);
	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off, const int32_t *scale);
	static void held_note_spark(const SparkNode *s);
	static void held_note_flash(const FlashNode *f);
	static void held_note_rock(const RockNode *k);
}
	static void register_mag274_held();
}
