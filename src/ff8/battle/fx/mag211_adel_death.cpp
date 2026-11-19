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
namespace adel211
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace adel211
{
	static void describe(Clone &c)
	{
		c.name = "211 Adel Death";
		c.effect_id = 211;
		c.vm_table = us(0x186F6B4);
		c.bone_table = us(0x186F324);
		c.prim_table = us(0x186F36C);
		c.prim_size = us(0x186F45C);
		c.spr_table = us(0x186F470);
		c.ptype_table = us(0x186F49C);
		c.pop_table = us(0x186F4B0);
		c.draw_table = us(0x186F510);
		c.attr_16A = us(0x186F67A);
		c.attr_184 = us(0x186F694);
		c.attr_186 = us(0x186F696);
		c.attr_194 = us(0x186F6A4);
		c.attr_19C = us(0x186F6AC);
		c.desc = us(0x186F1B0);
		c.ptr_block = us(0x186F2FC);
		c.queue = us(0x2796C88);
		c.file00 = us(0x2796CBC);
		c.file01 = us(0x2796CB8);
		c.tick = us(0xACA790);
		c.load_cb = us(0xAD3300);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t DIS_KEYS = 0x2796CA8;
	static const uint16_t DIS_RET16 = 0xBA6E;
#include "gfc_disintegrate.inc"

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
		c.vm[0x0AF] = op_0AF_SetInlineDescriptor;
		c.vm[0x035] = op_035_VramReadback;
		c.draw[22] = dh_22_EntityDisintegrate;
		c.draw[47] = dh_47_PaletteHsvShift;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag211_adel_death()
	{
		using namespace gfc;
		adel211::describe(adel211::g_c);
		init_clone(adel211::g_c, nullptr, 0);
		adel211::apply_slots(adel211::g_c);
		register_port(0xACA790, (void *)adel211::SequenceTask, "MAG211 Adel Death SequenceTick", 211);
	}
}

