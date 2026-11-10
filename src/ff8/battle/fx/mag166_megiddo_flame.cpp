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

// Effect 166: Megiddo Flame (enemy attack 310 of kernel.bin, Omega Weapon c0m114) (MAG_166_*): file loader 0x5E4E70 (texture mag311.tim), setup 0x5E4E90.
// The module is one compile of the Megiddo Flame code shared with effect 312 (fx_megiddo.h: structure,
// tasks and globals, written with 166's addresses); this file gives its descriptor and registers its tasks.

#include "fx_megiddo.h"
#include "fx_params.h"

namespace ff8fx
{
namespace megiddo
{
	struct D166
	{
		static constexpr uint32_t CODE = 0, GLOB = 0, DATA = 0; // 166 address - this module's
		static constexpr uint32_t CAMERA = 0xD716D8;                     // camera track (setup)
		static constexpr int32_t BONE = 0x25;                            // the caster's bone of the effect point
		static constexpr int32_t RMODE = 3;                              // OT shift of the glow / spin / ring prim models
		static constexpr int EFFECT = 166;
	};
}
}

#ifdef FF8_FX_HELD
#include "mag166_megiddo_flame_held.h"
#endif

namespace ff8fx
{
	void register_mag166_megiddo_flame()
	{
		static const char *const names[19] = { "166 Root", "166 Restore", "166 Director", "166 Glow", "166 Spin", "166 Ring", "166 BoltDirector",
			"166 Arc", "166 Sheet", "166 Ribbon", "166 Column", "166 ColumnFlash", "166 Pillar", "166 Flame", "166 Sparks", "166 Debris",
			"166 KnockDown", "166 Throw", "166 Ring2" };
		megiddo::Megiddo<megiddo::D166>::Register(names);
		// 30 fps layer: see mag166_megiddo_flame_held.inc
		FX_HELD(register_mag166_held();)
	}
	FX_LAB_MODULE(register_mag166_megiddo_flame);
}

#ifdef FF8_FX_HELD
#include "mag166_megiddo_flame_held.inc"
#endif
