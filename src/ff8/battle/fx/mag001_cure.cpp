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

// Effect 1: Cure (spell, MAG_001_CURE_*), the heal-ring module (mag_heal_ring_code.h: its code,
// structure and parts; mag_heal_ring.h: its parameters). This file says where Cure's copy is.
//
// Module code 0x8D69E0..0x8D8510: file loader MAG_001_CURE_FL 0x8D69E0 (the texture file named at
// 0x164252C, "mag000-049.tim"), setup MAG_001_CURE_Init 0x8D6A00, root 0x8D6B30, emitter 0x8D6CC0,
// ring 0x8D6D80, mote 0x8D7F60, sparkle 0x8D80B0; the effect library's glint task 0x8DCC20.
// Data: sound block 0x16417C4, camera animation 0x16417C8, flipbooks ring 0x1641F78, ring end
// 0x1641FCC, mote 0x164205C, sparkle 0x1642238, glow 0x1642504.
// Module globals 0x277AEC0..0x2792E78: texture file 0x277AEC0, packet cursor 0x277AEC4, arena limit
// 0x277AEEC, camera snapshot pointers 0x277AEF0 / 0x278C7DC, root pool 0x277AEF8 / queue 0x277AFC0,
// particle queue 0x277BF70 (pool 0x277BF80, 300 x 0x6C), arena pointers 0x2783E18 / 0x2783E1C /
// 0x278C798 / 0x278C79C / 0x278C7E4, init flags 0x2783E10 / 0x278C7E0, overlay vertex buffer
// 0x2783E28, emitter queue 0x278A228 (pool 0x278C628, 4 x 0x58), glint pool 0x278A238 (100 x 0x5C) /
// queue 0x278C788, overlay sprite block 0x278C7A0, overlay context 0x278C7E8; the effect library's
// glint cursor address 0x2792E74 and camera snapshot 0x2793E58.

#include "mag_heal_ring_code.h"

namespace ff8fx
{
namespace cure001
{
	inline constexpr healring::Layout LAYOUT = {
		0x8D69E0, 0x8D6A00, 0x8D6B30, 0x8D6CC0, 0x8D6D80, 0x8D7F60, 0x8D80B0,                 // functions
		0x164252C, 0x16417C8, 0x16417C4, 0x1641F78, 0x1641FCC, 0x1642504, 0x1642238, 0x164205C, // data
		0x277AEC0, 0x277AEC4, 0x277AEEC,                                                    // texture file, packet cursor / limit
		0x278C79C, 0x2783E1C, 0x278C798, 0x2783E18, 0x278C7E4,                              // arenas
		0x278C7E0, 0x2783E10, 0x278C7DC, 0x277AEF0,                                         // init flags, camera pointers
		0x277AFC0, 0x277AEF8, 0x278A228, 0x278C628, 0x277BF70, 0x277BF80, 0x278C788, 0x278A238, // queues, pools
		0x278C7E8, 0x277AEC8, 0x278C7A0, 0x2783E28,                                         // overlay
	};
}

	void register_mag001_cure()
	{
		healring::HealRing<cure001::LAYOUT>::register_module(1, "Cure", "001");
	}
	FX_LAB_MODULE(register_mag001_cure);
}
