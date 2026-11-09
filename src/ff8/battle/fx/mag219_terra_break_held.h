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

// 30 fps part of mag219_terra_break.cpp (declarations for its FX_HELD statements)
#pragma once
#include "gfc_engine_held.h"

namespace ff8fx
{
namespace gfc
{
namespace terra219
{
	// VM 0x0AD while predicting: the slot entity's next pose
	static void held_next_pose(uint8_t *hdr, uint8_t *cmd);
	// draw 49 StageShatter on held frames
	static uint8_t *TbHeldDraw(uint8_t *e, uint8_t *pool);
	static int8_t TbHeldLife(const uint8_t *s);
	static void TbNoteBegin(const uint8_t *pool);
	static void TbNoteStep(const uint8_t *s);
	static void TbHeldShard(const uint8_t *s, int16_t ang[3], int16_t pos[3]);
}
}
	static void register_mag219_terra_break_held();
}
