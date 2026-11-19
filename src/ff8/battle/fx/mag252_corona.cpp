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
namespace corona252
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace corona252
{
	static void describe(Clone &c)
	{
		c.name = "252 Corona (Mobile Type 8)";
		c.effect_id = 252;
		c.vm_table = us(0x185D600);
		c.bone_table = us(0x185D270);
		c.prim_table = us(0x185D2B8);
		c.prim_size = us(0x185D3A8);
		c.spr_table = us(0x185D3BC);
		c.ptype_table = us(0x185D3E8);
		c.pop_table = us(0x185D3FC);
		c.draw_table = us(0x185D45C);
		c.attr_16A = us(0x185D5C6);
		c.attr_184 = us(0x185D5E0);
		c.attr_186 = us(0x185D5E2);
		c.attr_194 = us(0x185D5F0);
		c.attr_19C = us(0x185D5F8);
		c.desc = us(0x185D1C0);
		c.ptr_block = us(0x185D248);
		c.queue = us(0x2796630);
		c.file00 = us(0x2796628);
		c.file01 = us(0x279662C);
		c.tick = us(0x9A2EE0);
		c.load_cb = us(0x9A7A70);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_shared_misc(c);
		const VoidFn p16 = c.prim[16];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.prim[16] = p16;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag252_corona()
	{
		using namespace gfc;
		corona252::describe(corona252::g_c);
		init_clone(corona252::g_c, nullptr, 0);
		corona252::apply_slots(corona252::g_c);
		register_port(0x9A2EE0, (void *)corona252::SequenceTask, "MAG252 Corona (Mobile Type 8) SequenceTick", 252);
	}
}

