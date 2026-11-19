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
namespace turret245
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace turret245
{
	static void describe(Clone &c)
	{
		c.name = "245 BGH251F2 3rd turret explosion";
		c.effect_id = 245;
		c.vm_table = us(0x18610E4);
		c.bone_table = us(0x1860D54);
		c.prim_table = us(0x1860D9C);
		c.prim_size = us(0x1860E8C);
		c.spr_table = us(0x1860EA0);
		c.ptype_table = us(0x1860ECC);
		c.pop_table = us(0x1860EE0);
		c.draw_table = us(0x1860F40);
		c.attr_16A = us(0x18610AA);
		c.attr_184 = us(0x18610C4);
		c.attr_186 = us(0x18610C6);
		c.attr_194 = us(0x18610D4);
		c.attr_19C = us(0x18610DC);
		c.desc = us(0x1860C58);
		c.ptr_block = us(0x1860D2C);
		c.queue = us(0x2796788);
		c.file00 = us(0x2796784);
		c.file01 = us(0x2796780);
		c.tick = us(0x9DCC00);
		c.load_cb = us(0x9E1290);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag245_bgh_turret3_explode()
	{
		using namespace gfc;
		turret245::describe(turret245::g_c);
		init_clone(turret245::g_c, nullptr, 0);
		register_port(0x9DCC00, (void *)turret245::SequenceTask, "MAG245 BGH251F2 3rd turret explosion SequenceTick", 245);
	}
}

