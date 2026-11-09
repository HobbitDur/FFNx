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

// 30 fps layer of Renzokuken vs Ultimecia + Griever (effect 342), included at the end of
// mag342_renzokuken_ultimecia_griever.cpp: the common part's layer (renzo_boss_held.inc: trail, slashes, particles, the hit
// emitters' particles) and its own tasks:
//   Facing models - scale half way, the fade half way from its first fading tick (mode switch before: held); the record
//     (point) as drawn; the caster's facing of the held frame (read by the draw, as on a tick).

namespace ff8fx
{
namespace renzo342
{
#include "renzo_boss_held.inc"

	enum { REC_FACING = REC_MODULE };

	static void held_note_facing(int kind, const Node24 *n)
	{
		if (Rec *r = NoteRec(REC_FACING, n, true))
		{
			r->sub = kind;
			memcpy(&r->slot, Slot(n->e), sizeof(SlotRec));
		}
	}

	static void FacingHeld(const Rec &r, int num, int den)
	{
		const FacingDef &d = FACINGS[r.sub];
		const Node24 &c = r.copy;
		int32_t scale = c.w1C;
		int32_t fade = FacingFade(d, &c, c.c);
		if (num != 0 && !g_tick_paused && Alive(r.node, d.orig))
		{
			scale = Lerp16(c.w1C, r.node->w1C, num, den);
			if (c.c >= FacingFrom(d, &c)) fade = lerp_i(fade, FacingFade(d, &c, r.node->c), num, den);
		}
		Replay(r, true);
		FacingDraw(d, &c, &r.slot, scale, fade);
	}

	static void ModuleHeld(const Rec &r, int num, int den)
	{
		if (r.kind == REC_FACING) FacingHeld(r, num, den);
	}
}

	static void register_mag342_held()
	{
		renzo342::MarkHeldCommon();
		mark_held(renzo342::ORIG_HitEmitter);
		mark_held(renzo342::ORIG_FacingA);
		mark_held(renzo342::ORIG_FacingB);
		register_module_held(342, renzo342::HeldReady, renzo342::HeldFrame);
	}
}
