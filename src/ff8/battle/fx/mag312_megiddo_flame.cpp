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

// Effect 312: Megiddo Flame (enemy attack 285 of kernel.bin, Mobile Type 8 c0m105) (MAG_312_*): file loader 0x59CB70 (texture mag311.tim), setup 0x59CB90.
// The module is one compile of the Megiddo Flame code shared with effect 166 (fx_megiddo.h: structure,
// tasks and globals, written with 166's addresses); this file gives its descriptor and registers its tasks.

#include "fx_megiddo.h"

namespace ff8fx
{
namespace megiddo
{
	struct D312
	{
		static constexpr uint32_t CODE = 0x48300, GLOB = 0x182398, DATA = 0x989F0; // 166 address - this module's
		static constexpr uint32_t CAMERA = 0xCD8D90;                     // camera track (setup)
		static constexpr int32_t BONE = 0xA;                            // the caster's bone of the effect point
		static constexpr int32_t RMODE = 2;                              // OT shift of the glow / spin / ring prim models
		static constexpr int EFFECT = 312;
	};
}
}

#ifdef FF8_FX_HELD
#include "mag312_megiddo_flame_held.h"
#endif

namespace ff8fx
{
	void register_mag312_megiddo_flame()
	{
		static const char *const names[19] = { "312 Root", "312 Restore", "312 Director", "312 Glow", "312 Spin", "312 Ring", "312 BoltDirector",
			"312 Arc", "312 Sheet", "312 Ribbon", "312 Column", "312 ColumnFlash", "312 Pillar", "312 Flame", "312 Sparks", "312 Debris",
			"312 KnockDown", "312 Throw", "312 Ring2" };
		megiddo::Megiddo<megiddo::D312>::Register(names);
		// 30 fps layer: see mag312_megiddo_flame_held.inc
		FX_HELD(register_mag312_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag312_megiddo_flame_held.inc"
#endif
