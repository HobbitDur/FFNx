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

// Effect 222: Water

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag222_water_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_brothers(Clone &c);

namespace water
{
	static Clone g_water = {};

	static void describe(Clone &c)
	{
		c.name = "222 Water";
		c.effect_id = 222;
		c.vm_table = 0x186CCFC;
		c.bone_table = 0x186C96C;
		c.prim_table = 0x186C9B4;
		c.prim_size = 0x186CAA4;
		c.spr_table = 0x186CAB8;
		c.ptype_table = 0x186CAE4;
		c.pop_table = 0x186CAF8;
		c.draw_table = 0x186CB58;
		c.attr_16A = 0x186CCC2;
		c.attr_184 = 0x186CCDC;
		c.attr_186 = 0x186CCDE;
		c.attr_194 = 0x186CCEC;
		c.attr_19C = 0x186CCF4;
		c.desc = 0x186C84C;
		c.ptr_block = 0x186C944;
		c.queue = 0x2796BA8;
		c.file00 = 0x2796BDC;
		c.file01 = 0x2796BD8;
		c.tick = 0xA9B870;
		c.load_cb = 0xAA0ED0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_draw38(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_brothers(c);
		VoidFn d38 = c.draw[38];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.draw[38] = d38;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_water);
	}
}
}

	void register_mag222_water()
	{
		using namespace gfc;
		water::describe(water::g_water);
		init_clone(water::g_water, nullptr, 0);
		apply_shared_misc(water::g_water);
		water::apply_draw38(water::g_water);
		register_port(0xA9B870, (void *)water::SequenceTask, "MAG222 Water SequenceTick", 222);
		FX_HELD(register_mag222_water_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag222_water_held.inc"
#endif
