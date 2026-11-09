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

// 30 fps layer of Renzokuken vs Ultima Weapon (effect 336), included at the end of mag336_renzokuken_ultima_weapon.cpp: the
// common part's layer (renzo_boss_held.inc: trail, slashes, particles, the two-bone emitter's particles, the dust, the node
// models CasterModelA / CasterModelB / GroundModel). The shake (camera shake word 0x1D97712) is the frame gate's generic shake
// handling; the shake task draws nothing.

namespace ff8fx
{
namespace renzo336
{
#define RENZO_HAS_NODEMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_NODEMODELS

	static void ModuleHeld(const Rec &, int, int) {}
}

	static void register_mag336_held()
	{
		renzo336::MarkHeldCommon();
		mark_held(renzo336::ORIG_CasterModelA);
		mark_held(renzo336::ORIG_BoneEmitter);
		mark_held(renzo336::ORIG_CasterModelB);
		mark_held(renzo336::ORIG_Dust);
		mark_held(renzo336::ORIG_GroundModel);
		register_module_held(336, renzo336::HeldReady, renzo336::HeldFrame);
	}
}
