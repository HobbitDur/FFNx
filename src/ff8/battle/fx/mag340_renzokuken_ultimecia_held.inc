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

// 30 fps layer of Renzokuken vs Ultimecia (final form) (effect 340), included at the end of mag340_renzokuken_ultimecia.cpp:
// the common part's layer (renzo_boss_held.inc: trail, slashes, particles, the emitters' particles (bone emitter, node
// emitter), the slot models RingA / RingB, the node models CasterModel / AngleModel).

namespace ff8fx
{
namespace renzo340
{
#define RENZO_HAS_SLOTMODELS
#define RENZO_HAS_NODEMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_NODEMODELS
#undef RENZO_HAS_SLOTMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag340_held()
	{
		renzo340::MarkHeldCommon();
		mark_held(renzo340::ORIG_RingA);
		mark_held(renzo340::ORIG_CasterModel);
		mark_held(renzo340::ORIG_RingB);
		mark_held(renzo340::ORIG_AngleModel);
		mark_held(renzo340::ORIG_BoneEmitter);
		mark_held(renzo340::ORIG_NodeEmitter);
		register_module_held(340, renzo340::HeldReady, renzo340::HeldFrame);
	}
}
