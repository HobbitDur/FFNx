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
namespace tripoint268
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag268_tri_point_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace tripoint268
{
	static void describe(Clone &c)
	{
		c.name = "268 Tri-Point death";
		c.effect_id = 268;
		c.vm_table = 0x185494C;
		c.bone_table = 0x18545BC;
		c.prim_table = 0x1854604;
		c.prim_size = 0x18546F4;
		c.spr_table = 0x1854708;
		c.ptype_table = 0x1854734;
		c.pop_table = 0x1854748;
		c.draw_table = 0x18547A8;
		c.attr_16A = 0x1854912;
		c.attr_184 = 0x185492C;
		c.attr_186 = 0x185492E;
		c.attr_194 = 0x185493C;
		c.attr_19C = 0x1854944;
		c.desc = 0x1854440;
		c.ptr_block = 0x1854594;
		c.queue = 0x2795C88;
		c.file00 = 0x2795CAC;
		c.file01 = 0x2795CA8;
		c.tick = 0x905070;
		c.load_cb = 0x90DA70;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t DIS_KEYS = 0x2795CB0;
	static const uint16_t DIS_RET16 = 0x634E;
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

	void register_mag268_tri_point_death()
	{
		using namespace gfc;
		tripoint268::describe(tripoint268::g_c);
		init_clone(tripoint268::g_c, nullptr, 0);
		tripoint268::apply_slots(tripoint268::g_c);
		register_port(0x905070, (void *)tripoint268::SequenceTask, "MAG268 Tri-Point death SequenceTick", 268);
		FX_HELD(register_mag268_tri_point_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag268_tri_point_death_held.inc"
#endif
