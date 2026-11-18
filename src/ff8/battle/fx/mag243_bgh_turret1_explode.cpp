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
namespace turret243
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag243_bgh_turret1_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret243
{
	static void describe(Clone &c)
	{
		c.name = "243 BGH251F2 1st turret explosion";
		c.effect_id = 243;
		c.vm_table = 0x186246C;
		c.bone_table = 0x18620DC;
		c.prim_table = 0x1862124;
		c.prim_size = 0x1862214;
		c.spr_table = 0x1862228;
		c.ptype_table = 0x1862254;
		c.pop_table = 0x1862268;
		c.draw_table = 0x18622C8;
		c.attr_16A = 0x1862432;
		c.attr_184 = 0x186244C;
		c.attr_186 = 0x186244E;
		c.attr_194 = 0x186245C;
		c.attr_19C = 0x1862464;
		c.desc = 0x1861FE0;
		c.ptr_block = 0x18620B4;
		c.queue = 0x27967F0;
		c.file00 = 0x2796804;
		c.file01 = 0x2796800;
		c.tick = 0x9EF100;
		c.load_cb = 0x9F3790;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag243_bgh_turret1_explode()
	{
		using namespace gfc;
		turret243::describe(turret243::g_c);
		init_clone(turret243::g_c, nullptr, 0);
		register_port(0x9EF100, (void *)turret243::SequenceTask, "MAG243 BGH251F2 1st turret explosion SequenceTick", 243);
		FX_HELD(register_mag243_bgh_turret1_explode_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag243_bgh_turret1_explode_held.inc"
#endif
