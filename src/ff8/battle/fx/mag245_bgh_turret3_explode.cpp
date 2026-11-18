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

#ifdef FF8_FX_HELD
#include "mag245_bgh_turret3_explode_held.h"
#endif

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
		c.vm_table = 0x18610E4;
		c.bone_table = 0x1860D54;
		c.prim_table = 0x1860D9C;
		c.prim_size = 0x1860E8C;
		c.spr_table = 0x1860EA0;
		c.ptype_table = 0x1860ECC;
		c.pop_table = 0x1860EE0;
		c.draw_table = 0x1860F40;
		c.attr_16A = 0x18610AA;
		c.attr_184 = 0x18610C4;
		c.attr_186 = 0x18610C6;
		c.attr_194 = 0x18610D4;
		c.attr_19C = 0x18610DC;
		c.desc = 0x1860C58;
		c.ptr_block = 0x1860D2C;
		c.queue = 0x2796788;
		c.file00 = 0x2796784;
		c.file01 = 0x2796780;
		c.tick = 0x9DCC00;
		c.load_cb = 0x9E1290;
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
		FX_HELD(register_mag245_bgh_turret3_explode_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag245_bgh_turret3_explode_held.inc"
#endif
