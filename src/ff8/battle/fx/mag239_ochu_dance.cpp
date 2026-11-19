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

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_breath_glow(Clone &c);

namespace ochu_dance
{
	static Clone g_c = {};

	static void describe(Clone &c)
	{
		c.name = "239 Ochu Dance";
		c.effect_id = 239;
		c.vm_table = us(0x1864C1C);
		c.bone_table = us(0x186488C);
		c.prim_table = us(0x18648D4);
		c.prim_size = us(0x18649C4);
		c.spr_table = us(0x18649D8);
		c.ptype_table = us(0x1864A04);
		c.pop_table = us(0x1864A18);
		c.draw_table = us(0x1864A78);
		c.attr_16A = us(0x1864BE2);
		c.attr_184 = us(0x1864BFC);
		c.attr_186 = us(0x1864BFE);
		c.attr_194 = us(0x1864C0C);
		c.attr_19C = us(0x1864C14);
		c.desc = us(0x1864760);
		c.ptr_block = us(0x1864864);
		c.queue = us(0x27968D8);
		c.file00 = us(0x27968D4);
		c.file01 = us(0x27968D0);
		c.tick = us(0xA160A0);
		c.load_cb = us(0xA1B840);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag239_ochu_dance()
	{
		using namespace gfc;
		ochu_dance::describe(ochu_dance::g_c);
		init_clone(ochu_dance::g_c, nullptr, 0);
		apply_breath_glow(ochu_dance::g_c);
		register_port(0xA160A0, (void *)ochu_dance::SequenceTask, "MAG239 Ochu Dance SequenceTick", 239);
	}
}

