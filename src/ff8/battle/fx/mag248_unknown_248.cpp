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
namespace eff248
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag248_unknown_248_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace eff248
{
	static void describe(Clone &c)
	{
		c.name = "248 (no kernel.bin attack)";
		c.effect_id = 248;
		c.vm_table = 0x185F364;
		c.bone_table = 0x185EFD4;
		c.prim_table = 0x185F01C;
		c.prim_size = 0x185F10C;
		c.spr_table = 0x185F120;
		c.ptype_table = 0x185F14C;
		c.pop_table = 0x185F160;
		c.draw_table = 0x185F1C0;
		c.attr_16A = 0x185F32A;
		c.attr_184 = 0x185F344;
		c.attr_186 = 0x185F346;
		c.attr_194 = 0x185F354;
		c.attr_19C = 0x185F35C;
		c.desc = 0x185EEFC;
		c.ptr_block = 0x185EFAC;
		c.queue = 0x27966D8;
		c.file00 = 0x27966E8;
		c.file01 = 0x27966EC;
		c.tick = 0x9C0F40;
		c.load_cb = 0x9C5240;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static void __cdecl op_09A_SetTargetList()
	{
		const uint32_t n = (uint32_t)U16(RT(), 0x4A) >> 12;
		U8(SCENE(), 0x41) = (uint8_t)n;
		STREAM() += 2;
		for (uint32_t i = 0; i < n; i++)
		{
			U8(SCENE(), 0x48 + i) = U8(STREAM(), 0);
			STREAM() += 2;
		}
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_shared_misc(c);
		const VoidFn v042 = c.vm[0x042];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.vm[0x042] = v042;
		c.vm[0x09A] = op_09A_SetTargetList;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag248_unknown_248()
	{
		using namespace gfc;
		eff248::describe(eff248::g_c);
		init_clone(eff248::g_c, nullptr, 0);
		eff248::apply_slots(eff248::g_c);
		register_port(0x9C0F40, (void *)eff248::SequenceTask, "MAG248 effect 248 SequenceTick", 248);
		FX_HELD(register_mag248_unknown_248_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag248_unknown_248_held.inc"
#endif
