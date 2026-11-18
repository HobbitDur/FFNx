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
namespace sand_shake
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag256_sand_shake_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sand_shake
{
	static void describe(Clone &c)
	{
		c.name = "256 Sand Shake";
		c.effect_id = 256;
		c.vm_table = 0x185AED8;
		c.bone_table = 0x185AB48;
		c.prim_table = 0x185AB90;
		c.prim_size = 0x185AC80;
		c.spr_table = 0x185AC94;
		c.ptype_table = 0x185ACC0;
		c.pop_table = 0x185ACD4;
		c.draw_table = 0x185AD34;
		c.attr_16A = 0x185AE9E;
		c.attr_184 = 0x185AEB8;
		c.attr_186 = 0x185AEBA;
		c.attr_194 = 0x185AEC8;
		c.attr_19C = 0x185AED0;
		c.desc = 0x185AA9C;
		c.ptr_block = 0x185AB20;
		c.queue = 0x2796548;
		c.file00 = 0x2796558;
		c.file01 = 0x2796540;
		c.tick = 0x979A70;
		c.load_cb = 0x97E0A0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag256_sand_shake()
	{
		using namespace gfc;
		sand_shake::describe(sand_shake::g_c);
		init_clone(sand_shake::g_c, nullptr, 0);
		apply_shared_misc(sand_shake::g_c);
		register_port(0x979A70, (void *)sand_shake::SequenceTask, "MAG256 Sand Shake SequenceTick", 256);
		FX_HELD(register_mag256_sand_shake_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag256_sand_shake_held.inc"
#endif
