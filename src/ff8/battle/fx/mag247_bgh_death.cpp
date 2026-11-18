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
namespace bghdeath247
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag247_bgh_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace bghdeath247
{
	static void describe(Clone &c)
	{
		c.name = "247 BGH251F2 Death";
		c.effect_id = 247;
		c.vm_table = 0x185FD5C;
		c.bone_table = 0x185F9CC;
		c.prim_table = 0x185FA14;
		c.prim_size = 0x185FB04;
		c.spr_table = 0x185FB18;
		c.ptype_table = 0x185FB44;
		c.pop_table = 0x185FB58;
		c.draw_table = 0x185FBB8;
		c.attr_16A = 0x185FD22;
		c.attr_184 = 0x185FD3C;
		c.attr_186 = 0x185FD3E;
		c.attr_194 = 0x185FD4C;
		c.attr_19C = 0x185FD54;
		c.desc = 0x185F89C;
		c.ptr_block = 0x185F9A4;
		c.queue = 0x2796718;
		c.file00 = 0x2796710;
		c.file01 = 0x2796714;
		c.tick = 0x9C9F00;
		c.load_cb = 0x9CED90;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag247_bgh_death()
	{
		using namespace gfc;
		bghdeath247::describe(bghdeath247::g_c);
		init_clone(bghdeath247::g_c, nullptr, 0);
		register_port(0x9C9F00, (void *)bghdeath247::SequenceTask, "MAG247 BGH251F2 Death SequenceTick", 247);
		FX_HELD(register_mag247_bgh_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag247_bgh_death_held.inc"
#endif
