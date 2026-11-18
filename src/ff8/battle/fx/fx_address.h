/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// Native battle effects: addresses of the other releases

#pragma once

#include <stdint.h>

namespace ff8fx
{
	// The effect code is written with the addresses of FF8 1.2 US (tables and constants keep them);
	// us() gives the running release's address of one
	bool address_init();
	uint32_t us(uint32_t address);
	bool effect_supported(int effect_id);
}
