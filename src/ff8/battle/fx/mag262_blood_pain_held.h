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

// 30 fps part of mag262_blood_pain.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace bloodpain262
{
	static void held_tick262();
	static void held_note_dh71(uint8_t *bone, uint8_t *ent, bool after);
	static void held_dh71();
	static bool held_pose_next(uint8_t *hdr);
}
}
	static void register_mag262_blood_pain_held();
}
