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

// Effect 201: Ifrit - Hell Fire

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag_ifrit_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace ifrit
{
	static Clone g_ifrit = {};

	static void describe(Clone &c)
	{
		c.name = "201 Ifrit";
		c.effect_id = 201;
		c.vm_table = us(0x1874F10);
		c.bone_table = us(0x1874B80);
		c.prim_table = us(0x1874BC8);
		c.prim_size = us(0x1874CB8);
		c.spr_table = us(0x1874CCC);
		c.ptype_table = us(0x1874CF8);
		c.pop_table = us(0x1874D0C);
		c.draw_table = us(0x1874D6C);
		c.attr_16A = us(0x1874ED6);
		c.attr_184 = us(0x1874EF0);
		c.attr_186 = us(0x1874EF2);
		c.attr_194 = us(0x1874F00);
		c.attr_19C = us(0x1874F08);
		c.desc = us(0x1874894);
		c.ptr_block = us(0x1874B58);
		c.queue = us(0x2796E18);
		c.file00 = us(0x2796E4C);
		c.file01 = us(0x2796E48);
		c.tick = us(0xB25DF0);
		c.load_cb = us(0xB2BB40);
		c.lit_dispatcher = true;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_ifrit);
	}
}
}

	void register_gfc_ifrit()
	{
		using namespace gfc;
		ifrit::describe(ifrit::g_ifrit);
		init_clone(ifrit::g_ifrit, nullptr, 0);
		register_port(0xB25DF0, (void *)ifrit::SequenceTask, "GFC201 Ifrit SequenceTick", 201);
		FX_HELD(register_gfc_ifrit_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag_ifrit_held.inc"
#endif
