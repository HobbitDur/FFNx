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

// 30 fps part of mag218_gilgamesh_kills_seifer.cpp (declarations for its FX_HELD statements)
#pragma once
#include "fx_held.h"

namespace ff8fx
{
	// held-frame camera of the shared camera-script task 0x63E9C0 (mag191_doomtrain_held.inc)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);
namespace g218
{
	static void held_note_master();
	static void held_note_tick_end();
	static void MemoNode(const Node24 *n);
	static void MemoExtra(const int16_t *v, int n);
	static void MemoExtra2(const int16_t *v, int n, int32_t k);
	static void MemoMat(const Mat4x3 *m, bool drawn);
	static void PoolMemoA(int i, const RecA *p);
	static void PoolMemoB(int i, const RecB *p);
	static void ModelMemoTake(const Node24 *n);
	static void held_note_advanced(uint8_t *E);
}
	static void register_mag218_held();
}
