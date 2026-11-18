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
namespace gas236
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag236_gas_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace gas236
{
	static void describe(Clone &c)
	{
		c.name = "236 Gas";
		c.effect_id = 236;
		c.vm_table = us(0x1866A28);
		c.bone_table = us(0x1866698);
		c.prim_table = us(0x18666E0);
		c.prim_size = us(0x18667D0);
		c.spr_table = us(0x18667E4);
		c.ptype_table = us(0x1866810);
		c.pop_table = us(0x1866824);
		c.draw_table = us(0x1866884);
		c.attr_16A = us(0x18669EE);
		c.attr_184 = us(0x1866A08);
		c.attr_186 = us(0x1866A0A);
		c.attr_194 = us(0x1866A18);
		c.attr_19C = us(0x1866A20);
		c.desc = us(0x1866564);
		c.ptr_block = us(0x1866670);
		c.queue = us(0x2796978);
		c.file00 = us(0x279698C);
		c.file01 = us(0x2796988);
		c.tick = us(0xA348B0);
		c.load_cb = us(0xA39DF0);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

#include "gfc_deathclaw.inc"

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag236_gas()
	{
		using namespace gfc;
		gas236::describe(gas236::g_c);
		init_clone(gas236::g_c, nullptr, 0);
		gas236::g_c.bone[4] = gas236::bh_04_OrbitBetweenBones;
		gas236::g_c.bone[8] = gas236::bh_08_VertexAtHeight;
		gas236::g_c.prim[48] = gas236::pr_48_TintedFT3Abr;
		gas236::g_c.draw[31] = gas236::dh_31_MeshHeightCut;
		register_port(0xA348B0, (void *)gas236::SequenceTask, "MAG236 Gas SequenceTick", 236);
		FX_HELD(register_mag236_gas_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag236_gas_held.inc"
#endif
