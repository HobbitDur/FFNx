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
	void apply_eden_d(Clone &c);

namespace beam242
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag242_bgh_beam_cannon_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace beam242
{
	static void describe(Clone &c)
	{
		c.name = "242 Beam Cannon (BGH251F2)";
		c.effect_id = 242;
		c.vm_table = 0x1862E50;
		c.bone_table = 0x1862AC0;
		c.prim_table = 0x1862B08;
		c.prim_size = 0x1862BF8;
		c.spr_table = 0x1862C0C;
		c.ptype_table = 0x1862C38;
		c.pop_table = 0x1862C4C;
		c.draw_table = 0x1862CAC;
		c.attr_16A = 0x1862E16;
		c.attr_184 = 0x1862E30;
		c.attr_186 = 0x1862E32;
		c.attr_194 = 0x1862E40;
		c.attr_19C = 0x1862E48;
		c.desc = 0x18629A4;
		c.ptr_block = 0x1862A98;
		c.queue = 0x2796828;
		c.file00 = 0x279683C;
		c.file01 = 0x2796838;
		c.tick = 0x9F8380;
		c.load_cb = 0x9FD890;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.prim[6] = p6;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag242_bgh_beam_cannon()
	{
		using namespace gfc;
		beam242::describe(beam242::g_c);
		init_clone(beam242::g_c, nullptr, 0);
		beam242::apply_slots(beam242::g_c);
		register_port(0x9F8380, (void *)beam242::SequenceTask, "MAG242 Beam Cannon (BGH251F2) SequenceTick", 242);
		FX_HELD(register_mag242_bgh_beam_cannon_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag242_bgh_beam_cannon_held.inc"
#endif
