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

// 30 fps part of mag126_electric_discharge.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace disch126
{
	static void held_note_root();
	static void held_note_arc(const ArcNode *a);
	static void held_note_crawler(const CrawlerNode *c, uint32_t dir0, uint32_t slot0);
	static void held_note_crawler_point(const int16_t *w);
	static void held_note_spark(const SparkNode *s);
	static void held_note_play(prim::Layout *layout, const PrimArg *arg);
}
	static void register_mag126_held();
}
