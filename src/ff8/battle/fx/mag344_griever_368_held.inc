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

// 30 fps part of mag344_griever_368.cpp (included at the end of it).
//
// The module draws nothing (its packet arenas stay empty): a held frame has nothing of its own to draw. The camera
// animation it starts is the engine's camera script and the chain transformation the engine's model animation; the
// held part only says the ports ran on the last real tick.

namespace ff8fx
{
namespace griever344
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	static void held_note_root() { g_ported_tick = g_real_tick; }

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void HeldFrame(int, int) {}
}

	static void register_mag344_held()
	{
		mark_held(griever344::ORIG_RootTask);
		mark_held(griever344::ORIG_CueTask);
		register_module_held(344, griever344::HeldReady, griever344::HeldFrame);
	}
}
