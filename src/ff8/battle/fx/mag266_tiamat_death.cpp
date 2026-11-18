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
namespace tiamat266
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag266_tiamat_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace tiamat266
{
	static void describe(Clone &c)
	{
		c.name = "266 Tiamat death";
		c.effect_id = 266;
		c.vm_table = us(0x1855DCC);
		c.bone_table = us(0x1855A3C);
		c.prim_table = us(0x1855A84);
		c.prim_size = us(0x1855B74);
		c.spr_table = us(0x1855B88);
		c.ptype_table = us(0x1855BB4);
		c.pop_table = us(0x1855BC8);
		c.draw_table = us(0x1855C28);
		c.attr_16A = us(0x1855D92);
		c.attr_184 = us(0x1855DAC);
		c.attr_186 = us(0x1855DAE);
		c.attr_194 = us(0x1855DBC);
		c.attr_19C = us(0x1855DC4);
		c.desc = us(0x18558C8);
		c.ptr_block = us(0x1855A14);
		c.queue = us(0x2795CF8);
		c.file00 = us(0x2795D0C);
		c.file01 = us(0x2795D08);
		c.tick = us(0x91FC50);
		c.load_cb = us(0x928650);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t DIS_KEYS = 0x2795D20;
	static const uint16_t DIS_RET16 = 0x0F2E;
#include "gfc_disintegrate.inc"

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
		c.draw[22] = dh_22_EntityDisintegrate;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag266_tiamat_death()
	{
		using namespace gfc;
		tiamat266::describe(tiamat266::g_c);
		init_clone(tiamat266::g_c, nullptr, 0);
		tiamat266::apply_slots(tiamat266::g_c);
		register_port(0x91FC50, (void *)tiamat266::SequenceTask, "MAG266 Tiamat death SequenceTick", 266);
		FX_HELD(register_mag266_tiamat_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag266_tiamat_death_held.inc"
#endif
