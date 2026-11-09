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

// 30 fps layer of Renzokuken vs NORG (effect 335), included at the end of mag335_renzokuken_norg.cpp: the common part's layer
// (renzo_boss_held.inc: trail, slashes, particles, the boss emitter's particles, the slot model Ring). The caster's jump in
// front of NORG and back (entity +0x1C / +0x20 on ticks 0xD / length - 5) is a discrete change: the engine draws the caster
// where the real tick put it.

namespace ff8fx
{
namespace renzo335
{
#define RENZO_HAS_SLOTMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_SLOTMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag335_held()
	{
		renzo335::MarkHeldCommon();
		mark_held(renzo335::ORIG_BossEmitter);
		mark_held(renzo335::ORIG_Ring);
		register_module_held(335, renzo335::HeldReady, renzo335::HeldFrame);
	}
}
