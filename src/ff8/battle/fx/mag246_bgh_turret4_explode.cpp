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
namespace turret246
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag246_bgh_turret4_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret246
{
	static void describe(Clone &c)
	{
		c.name = "246 BGH251F2 4th turret explosion";
		c.effect_id = 246;
		c.vm_table = us(0x1860720);
		c.bone_table = us(0x1860390);
		c.prim_table = us(0x18603D8);
		c.prim_size = us(0x18604C8);
		c.spr_table = us(0x18604DC);
		c.ptype_table = us(0x1860508);
		c.pop_table = us(0x186051C);
		c.draw_table = us(0x186057C);
		c.attr_16A = us(0x18606E6);
		c.attr_184 = us(0x1860700);
		c.attr_186 = us(0x1860702);
		c.attr_194 = us(0x1860710);
		c.attr_19C = us(0x1860718);
		c.desc = us(0x1860294);
		c.ptr_block = us(0x1860368);
		c.queue = us(0x2796750);
		c.file00 = us(0x2796748);
		c.file01 = us(0x279674C);
		c.tick = us(0x9D3980);
		c.load_cb = us(0x9D8010);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag246_bgh_turret4_explode()
	{
		using namespace gfc;
		turret246::describe(turret246::g_c);
		init_clone(turret246::g_c, nullptr, 0);
		register_port(0x9D3980, (void *)turret246::SequenceTask, "MAG246 BGH251F2 4th turret explosion SequenceTick", 246);
		FX_HELD(register_mag246_bgh_turret4_explode_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag246_bgh_turret4_explode_held.inc"
#endif
