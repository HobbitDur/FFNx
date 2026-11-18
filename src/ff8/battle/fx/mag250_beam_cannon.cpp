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
	void apply_eden_d(Clone &c);

namespace beam250
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag250_beam_cannon_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace beam250
{
	static void describe(Clone &c)
	{
		c.name = "250 Beam Cannon";
		c.effect_id = 250;
		c.vm_table = 0x185DFE4;
		c.bone_table = 0x185DC54;
		c.prim_table = 0x185DC9C;
		c.prim_size = 0x185DD8C;
		c.spr_table = 0x185DDA0;
		c.ptype_table = 0x185DDCC;
		c.pop_table = 0x185DDE0;
		c.draw_table = 0x185DE40;
		c.attr_16A = 0x185DFAA;
		c.attr_184 = 0x185DFC4;
		c.attr_186 = 0x185DFC6;
		c.attr_194 = 0x185DFD4;
		c.attr_19C = 0x185DFDC;
		c.desc = 0x185DB38;
		c.ptr_block = 0x185DC2C;
		c.queue = 0x2796668;
		c.file00 = 0x2796678;
		c.file01 = 0x2796660;
		c.tick = 0x9AC660;
		c.load_cb = 0x9B2E00;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.prim[6] = p6;
		apply_shared_ribbon(c);
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag250_beam_cannon()
	{
		using namespace gfc;
		beam250::describe(beam250::g_c);
		init_clone(beam250::g_c, nullptr, 0);
		beam250::apply_slots(beam250::g_c);
		register_port(0x9AC660, (void *)beam250::SequenceTask, "MAG250 Beam Cannon SequenceTick", 250);
		FX_HELD(register_mag250_beam_cannon_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag250_beam_cannon_held.inc"
#endif
