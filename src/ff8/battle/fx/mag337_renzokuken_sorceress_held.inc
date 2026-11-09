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

// 30 fps layer of Renzokuken vs the Sorceress (wormlike) (effect 337), included at the end of mag337_renzokuken_sorceress.cpp:
// the common part's layer (renzo_boss_held.inc: trail, slashes, particles, the bone emitter's particles, the slot model Ring,
// the dust, the node models CasterModel / GroundModel). The shake (camera shake word 0x1D97712) is the frame gate's generic
// shake handling; the shake task draws nothing.

namespace ff8fx
{
namespace renzo337
{
#define RENZO_HAS_SLOTMODELS
#define RENZO_HAS_NODEMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_NODEMODELS
#undef RENZO_HAS_SLOTMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag337_held()
	{
		renzo337::MarkHeldCommon();
		mark_held(renzo337::ORIG_CasterModel);
		mark_held(renzo337::ORIG_BoneEmitter);
		mark_held(renzo337::ORIG_Ring);
		mark_held(renzo337::ORIG_Dust);
		mark_held(renzo337::ORIG_GroundModel);
		register_module_held(337, renzo337::HeldReady, renzo337::HeldFrame);
	}
}
