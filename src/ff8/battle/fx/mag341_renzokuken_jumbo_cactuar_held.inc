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

// 30 fps layer of Renzokuken vs Jumbo Cactuar (effect 341), included at the end of mag341_renzokuken_jumbo_cactuar.cpp: the
// common part's layer (renzo_boss_held.inc: trail, slashes, particles, the slot model Ring, the dust).
// The shake (camera shake word 0x1D97712, rewritten every tick and zeroed by the view build) is the frame gate's generic
// shake handling; the shake task draws nothing.

namespace ff8fx
{
namespace renzo341
{
#define RENZO_HAS_SLOTMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_SLOTMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag341_held()
	{
		renzo341::MarkHeldCommon();
		mark_held(renzo341::ORIG_Ring);
		mark_held(renzo341::ORIG_Dust);
		register_module_held(341, renzo341::HeldReady, renzo341::HeldFrame);
	}
}
