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
namespace sphinxara265
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag265_sphinxara_265_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sphinxara265
{
	static void describe(Clone &c)
	{
		c.name = "265 Sphinxara";
		c.effect_id = 265;
		c.vm_table = us(0x1856810);
		c.bone_table = us(0x1856480);
		c.prim_table = us(0x18564C8);
		c.prim_size = us(0x18565B8);
		c.spr_table = us(0x18565CC);
		c.ptype_table = us(0x18565F8);
		c.pop_table = us(0x185660C);
		c.draw_table = us(0x185666C);
		c.attr_16A = us(0x18567D6);
		c.attr_184 = us(0x18567F0);
		c.attr_186 = us(0x18567F2);
		c.attr_194 = us(0x1856800);
		c.attr_19C = us(0x1856808);
		c.desc = us(0x1856304);
		c.ptr_block = us(0x1856458);
		c.queue = us(0x2795D30);
		c.file00 = us(0x2795D44);
		c.file01 = us(0x2795D40);
		c.tick = us(0x92D240);
		c.load_cb = us(0x933930);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

#include "gfc_melt.inc"

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];
		const VoidFn p16 = c.prim[16];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.draw[3] = dh_03_AnimatedModelWithShadow;
		c.draw[52] = dh_52_EntityMelt;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag265_sphinxara_265()
	{
		using namespace gfc;
		sphinxara265::describe(sphinxara265::g_c);
		init_clone(sphinxara265::g_c, nullptr, 0);
		sphinxara265::apply_slots(sphinxara265::g_c);
		register_port(0x92D240, (void *)sphinxara265::SequenceTask, "MAG265 Sphinxara 265 SequenceTick", 265);
		FX_HELD(register_mag265_sphinxara_265_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag265_sphinxara_265_held.inc"
#endif
