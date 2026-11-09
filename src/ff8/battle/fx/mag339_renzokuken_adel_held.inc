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

// 30 fps layer of Renzokuken vs Adel (effect 339), included at the end of mag339_renzokuken_adel.cpp: the common part's
// layer (renzo_boss_held.inc: trail, slashes, particles, the emitters, the two slot models RingA / RingB).

namespace ff8fx
{
namespace renzo339
{
#define RENZO_HAS_SLOTMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_SLOTMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag339_held()
	{
		renzo339::MarkHeldCommon();
		mark_held(renzo339::ORIG_Emitter);
		mark_held(renzo339::ORIG_RingA);
		mark_held(renzo339::ORIG_RingB);
		register_module_held(339, renzo339::HeldReady, renzo339::HeldFrame);
	}
}
