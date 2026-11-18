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
namespace gatling241
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag241_bgh_gatling_gun_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace gatling241
{
	static void describe(Clone &c)
	{
		c.name = "241 Gatling Gun (BGH251F2)";
		c.effect_id = 241;
		c.vm_table = 0x1863830;
		c.bone_table = 0x18634A0;
		c.prim_table = 0x18634E8;
		c.prim_size = 0x18635D8;
		c.spr_table = 0x18635EC;
		c.ptype_table = 0x1863618;
		c.pop_table = 0x186362C;
		c.draw_table = 0x186368C;
		c.attr_16A = 0x18637F6;
		c.attr_184 = 0x1863810;
		c.attr_186 = 0x1863812;
		c.attr_194 = 0x1863820;
		c.attr_19C = 0x1863828;
		c.desc = 0x1863388;
		c.ptr_block = 0x1863478;
		c.queue = 0x2796860;
		c.file00 = 0x2796874;
		c.file01 = 0x2796870;
		c.tick = 0xA02480;
		c.load_cb = 0xA06DE0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag241_bgh_gatling_gun()
	{
		using namespace gfc;
		gatling241::describe(gatling241::g_c);
		init_clone(gatling241::g_c, nullptr, 0);
		register_port(0xA02480, (void *)gatling241::SequenceTask, "MAG241 Gatling Gun (BGH251F2) SequenceTick", 241);
		FX_HELD(register_mag241_bgh_gatling_gun_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag241_bgh_gatling_gun_held.inc"
#endif
