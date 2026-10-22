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

// 30 fps part of mag212_burning_rave.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace rave212
{
	static void held_note_root(RootNode *r);
	static void held_note_director(DirectorNode *d);
	static void held_note_layout(DirectorNode *d, int site, uint8_t *layout, const PlayArg *arg);
	static void held_note_ghosts(uint8_t *entity, const uint8_t *g);
	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off);
	static void held_note_capture(const int16_t *a, const int16_t *b);
	static void held_note_glow(int32_t b);
	static void held_note_rock(const RockNode *k, uint32_t model);
	static void held_note_sprite(const SpriteNode *s, uint32_t seq, int32_t depth);
}
	static void register_mag212_held();
}
