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

// 30 fps part of mag235_breath.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace breath
{
	static void held_entity_begin(uint8_t *ent);
	static void held_entity_end();
}
}
	static void register_mag235_breath_held();
}
