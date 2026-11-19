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
namespace ultima270
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace ultima270
{
	static void describe(Clone &c)
	{
		c.name = "270 Ultima Weapon Death";
		c.effect_id = 270;
		c.vm_table = us(0x18534D4);
		c.bone_table = us(0x1853144);
		c.prim_table = us(0x185318C);
		c.prim_size = us(0x185327C);
		c.spr_table = us(0x1853290);
		c.ptype_table = us(0x18532BC);
		c.pop_table = us(0x18532D0);
		c.draw_table = us(0x1853330);
		c.attr_16A = us(0x185349A);
		c.attr_184 = us(0x18534B4);
		c.attr_186 = us(0x18534B6);
		c.attr_194 = us(0x18534C4);
		c.attr_19C = us(0x18534CC);
		c.desc = us(0x1852FD0);
		c.ptr_block = us(0x185311C);
		c.queue = us(0x2795C18);
		c.file00 = us(0x2795C3C);
		c.file01 = us(0x2795C38);
		c.tick = us(0x8EA2A0);
		c.load_cb = us(0x8F2CA0);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t DIS_KEYS = 0x2795C40;
	static const uint16_t DIS_RET16 = 0xB57E;
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

	void register_mag270_ultima_weapon_death()
	{
		using namespace gfc;
		ultima270::describe(ultima270::g_c);
		init_clone(ultima270::g_c, nullptr, 0);
		ultima270::apply_slots(ultima270::g_c);
		register_port(0x8EA2A0, (void *)ultima270::SequenceTask, "MAG270 Ultima Weapon Death SequenceTick", 270);
	}
}

