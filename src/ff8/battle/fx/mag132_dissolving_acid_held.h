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

// 30 fps part of mag132_dissolving_acid.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace acid132
{
	static void held_note_master();
	static void held_note_drop_list();
	static void held_note_drop(const DropNode *d);
	static void held_note_layout(const LayoutNode *s, const PrimArg *arg, bool splash);
	static void held_note_sequence(const void *node, bool stream);
	static void held_note_stream(const StreamNode *s, bool grew);
}
	static void register_mag132_held();
}
