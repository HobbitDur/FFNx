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

// 30 fps layer of Renzokuken vs Griever (effect 343), included at the end of mag343_renzokuken_griever.cpp: the common
// part's layer (renzo_boss_held.inc: trail, slashes, particles) and its own tasks:
//   Emitters - the common part's (renzo_boss_held.inc).
//   Final - scale half way, the fade half way from tick 6 on (mode switch before: held); the kept caster matrix and the
//     point are fixed.

namespace ff8fx
{
namespace renzo343
{
#include "renzo_boss_held.inc"

	enum { REC_FINAL = REC_MODULE };

	static void held_note_final(const Node24 *n) { NoteRec(REC_FINAL, n, true); }

	static void FinalHeld(const Rec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		int32_t scale = d.w1C;
		int32_t fade = FinalFade(d.c);
		if (num != 0 && !g_tick_paused && Alive(r.node, ORIG_Final))
		{
			scale = Lerp16(d.w1C, r.node->w1C, num, den);
			if (d.c >= 6) fade = lerp_i(fade, FinalFade(r.node->c), num, den);
		}
		Replay(r, true);
		FinalDraw(&d, scale, fade);
	}

	static void ModuleHeld(const Rec &r, int num, int den)
	{
		if (r.kind == REC_FINAL) FinalHeld(r, num, den);
	}
}

	static void register_mag343_held()
	{
		renzo343::MarkHeldCommon();
		mark_held(renzo343::ORIG_Emitter);
		mark_held(renzo343::ORIG_Final);
		register_module_held(343, renzo343::HeldReady, renzo343::HeldFrame);
	}
}
