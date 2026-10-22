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

// 30 fps part of mag085_the_end.cpp (included at the end of mag085_the_end.cpp).
//
// INTERIM (fx189): no held-frame drawing yet. No task of the module is marked held and no module held part is registered:
// the frame gate replays the real tick's packets on held frames (everything holds at 15 Hz). The exact held part (the
// actor template for the actor systems, the prim plays, the meteors / petals / ground models, the glow and letterbox) is
// the remaining work of effect 85.

namespace ff8fx
{
namespace act
{
namespace theend
{
	static void held_note_system(uint32_t sys) { (void)sys; }
}
}

	static void register_mag085_held()
	{
	}
}
