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
namespace sorceress208
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag208_final_sorceress_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sorceress208
{
	static void describe(Clone &c)
	{
		c.name = "208 Final Sorceress Death";
		c.effect_id = 208;
		c.vm_table = us(0x1870108);
		c.bone_table = us(0x186FD78);
		c.prim_table = us(0x186FDC0);
		c.prim_size = us(0x186FEB0);
		c.spr_table = us(0x186FEC4);
		c.ptype_table = us(0x186FEF0);
		c.pop_table = us(0x186FF04);
		c.draw_table = us(0x186FF64);
		c.attr_16A = us(0x18700CE);
		c.attr_184 = us(0x18700E8);
		c.attr_186 = us(0x18700EA);
		c.attr_194 = us(0x18700F8);
		c.attr_19C = us(0x1870100);
		c.desc = us(0x186FBEC);
		c.ptr_block = us(0x186FD50);
		c.queue = us(0x2796CC0);
		c.file00 = us(0x2796CF0);
		c.file01 = us(0x2796CF4);
		c.tick = us(0xAD7F70);
		c.load_cb = us(0xADE7D0);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

#include "gfc_melt.inc"

#include "gfc_palshift.inc"

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
		c.vm[0x035] = op_035_VramReadback;
		c.draw[47] = dh_47_PaletteHsvShift;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		FX_HELD(PalShiftRestore();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag208_final_sorceress_death()
	{
		using namespace gfc;
		sorceress208::describe(sorceress208::g_c);
		init_clone(sorceress208::g_c, nullptr, 0);
		sorceress208::apply_slots(sorceress208::g_c);
		register_port(0xAD7F70, (void *)sorceress208::SequenceTask, "MAG208 Final Sorceress Death SequenceTick", 208);
		FX_HELD(register_mag208_final_sorceress_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag208_final_sorceress_death_held.inc"
#endif
