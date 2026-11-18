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
namespace turret244
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag244_bgh_turret_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret244
{
	static void describe(Clone &c)
	{
		c.name = "244 BGH251F2 turret explosion";
		c.effect_id = 244;
		c.vm_table = 0x1861AA8;
		c.bone_table = 0x1861718;
		c.prim_table = 0x1861760;
		c.prim_size = 0x1861850;
		c.spr_table = 0x1861864;
		c.ptype_table = 0x1861890;
		c.pop_table = 0x18618A4;
		c.draw_table = 0x1861904;
		c.attr_16A = 0x1861A6E;
		c.attr_184 = 0x1861A88;
		c.attr_186 = 0x1861A8A;
		c.attr_194 = 0x1861A98;
		c.attr_19C = 0x1861AA0;
		c.desc = 0x186161C;
		c.ptr_block = 0x18616F0;
		c.queue = 0x27967C0;
		c.file00 = 0x27967BC;
		c.file01 = 0x27967B8;
		c.tick = 0x9E5E80;
		c.load_cb = 0x9EA510;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag244_bgh_turret_explode()
	{
		using namespace gfc;
		turret244::describe(turret244::g_c);
		init_clone(turret244::g_c, nullptr, 0);
		register_port(0x9E5E80, (void *)turret244::SequenceTask, "MAG244 BGH251F2 turret explosion SequenceTick", 244);
		FX_HELD(register_mag244_bgh_turret_explode_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag244_bgh_turret_explode_held.inc"
#endif
