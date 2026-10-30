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

// Effect 50: Potion (battle items Potion and Potion+: kernel.bin battle item +4 = 50; MAG_050_*).
// The module is a relocated copy of Cure's (effect 1, mag001_cure.cpp): the same code instruction for
// instruction, byte-identical data tables (sound, camera animation, flipbooks; the same texture file
// "mag000-049.tim"), its own globals (Cure's - 0x146408); it shares the effect library 0x8DC530..
// (glint cursor address 0x2792E74, camera snapshot 0x2793E58) with Cure. Its code is the heal-ring
// module's (mag_heal_ring_code.h); this file says where Potion's copy is.
//
// Module code 0x81F9F0..0x821520: file loader MAG_050_POTION_FL 0x81F9F0 (the texture file named at
// 0x15ADAE0), setup MAG_050_POTION_Init 0x81FA10, root 0x81FB40, emitter 0x81FCD0, ring 0x81FD90,
// mote 0x820F70, sparkle 0x8210C0. Data: sound block 0x15ACD78, camera animation 0x15ACD7C,
// flipbooks ring 0x15AD52C, ring end 0x15AD580, mote 0x15AD610, sparkle 0x15AD7EC, glow 0x15ADAB8.
// Module globals 0x2634AB8..0x26464D8: texture file 0x2634AB8, packet cursor 0x2634ABC, arena limit
// 0x2634AE4, camera snapshot pointers 0x2634AE8 / 0x26463D4, root pool 0x2634AF0 / queue 0x2634BB8,
// particle queue 0x2635B68 (pool 0x2635B78, 300 x 0x6C), arena pointers 0x263DA10 / 0x263DA14 /
// 0x2646390 / 0x2646394 / 0x26463DC, init flags 0x263DA08 / 0x26463D8, overlay vertex buffer
// 0x263DA20, emitter queue 0x2643E20 (pool 0x2646220, 4 x 0x58), glint pool 0x2643E30 (100 x 0x5C) /
// queue 0x2646380, overlay sprite block 0x2646398, overlay context 0x26463E0.

#include "mag_heal_ring_code.h"

namespace ff8fx
{
namespace potion050
{
	inline constexpr healring::Layout LAYOUT = {
		0x81F9F0, 0x81FA10, 0x81FB40, 0x81FCD0, 0x81FD90, 0x820F70, 0x8210C0,                 // functions
		0x15ADAE0, 0x15ACD7C, 0x15ACD78, 0x15AD52C, 0x15AD580, 0x15ADAB8, 0x15AD7EC, 0x15AD610, // data
		0x2634AB8, 0x2634ABC, 0x2634AE4,                                                    // texture file, packet cursor / limit
		0x2646394, 0x263DA14, 0x2646390, 0x263DA10, 0x26463DC,                              // arenas
		0x26463D8, 0x263DA08, 0x26463D4, 0x2634AE8,                                         // init flags, camera pointers
		0x2634BB8, 0x2634AF0, 0x2643E20, 0x2646220, 0x2635B68, 0x2635B78, 0x2646380, 0x2643E30, // queues, pools
		0x26463E0, 0x2634AC0, 0x2646398, 0x263DA20,                                         // overlay
	};
}

	void register_mag050_potion()
	{
		healring::HealRing<potion050::LAYOUT>::register_module(50, "Potion", "050");
	}
	FX_LAB_MODULE(register_mag050_potion);
}
