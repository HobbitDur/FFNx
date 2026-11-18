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
namespace bgh249
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag249_bgh_attack_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace bgh249
{
	static void describe(Clone &c)
	{
		c.name = "249 Attack (BGH251F2)";
		c.effect_id = 249;
		c.vm_table = us(0x185E9C4);
		c.bone_table = us(0x185E634);
		c.prim_table = us(0x185E67C);
		c.prim_size = us(0x185E76C);
		c.spr_table = us(0x185E780);
		c.ptype_table = us(0x185E7AC);
		c.pop_table = us(0x185E7C0);
		c.draw_table = us(0x185E820);
		c.attr_16A = us(0x185E98A);
		c.attr_184 = us(0x185E9A4);
		c.attr_186 = us(0x185E9A6);
		c.attr_194 = us(0x185E9B4);
		c.attr_19C = us(0x185E9BC);
		c.desc = us(0x185E51C);
		c.ptr_block = us(0x185E60C);
		c.queue = us(0x27966A0);
		c.file00 = us(0x27966B0);
		c.file01 = us(0x27966B4);
		c.tick = us(0x9B79F0);
		c.load_cb = us(0x9BC350);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag249_bgh_attack()
	{
		using namespace gfc;
		bgh249::describe(bgh249::g_c);
		init_clone(bgh249::g_c, nullptr, 0);
		register_port(0x9B79F0, (void *)bgh249::SequenceTask, "MAG249 Attack (BGH251F2) SequenceTick", 249);
		FX_HELD(register_mag249_bgh_attack_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag249_bgh_attack_held.inc"
#endif
