/****************************************************************************/
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

// Effect 239: Ochu Dance (enemy attack 192 of kernel.bin, used by Ochu c0m045; cinematic-engine
// attack, module MAG_239 0xA15A00..0xA1FD90, script data magic/mag238_b.00/.01, loaded by
// MAG_239_UNKNOWN_FL 0xA15A00).
//
// One more compile of the GF cinematic engine (gfc_engine.cpp), with Breath's table layout
// (mag235_breath.cpp: desc = draw_table - 0x318). Instruction-level comparison (module-relative
// addresses normalised, callee trees hashed) against Ifrit and Breath:
//   - SequenceTick 0xA160A0 (the unlit variant = Breath 0xA3E9E0 = Brothers 0xAF4B90),
//     BindContext 0xA15AE0, the section pointer set-up 0xA15B80 are byte clones of Breath's;
//   - every table handler is the generic (Ifrit) one or a stub (VM 0x093, prim 7 / 9 / 12 / 17 /
//     18 and draw 3 are stubs here; VM 0x006 LoadBattleFile 0xA1B710 has its own load callback
//     0xA1B840, c.load_cb) except
//       draw 32 0xA16E80 EntityGlow: a byte clone of Breath's 0xA3F700 (redraws a live battle
//         entity, the caster, with a per-vertex colour glow around a node): Breath's port is
//         taken (apply_breath_glow, mag235_breath.cpp).
// The effect's queue (0x27968D8) holds one task, the engine tick 0xA160A0.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag239_ochu_dance_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_breath_glow(Clone &c);  // mag235_breath.cpp

namespace ochu_dance
{
	static Clone g_c = {};

	static void describe(Clone &c)
	{
		c.name = "239 Ochu Dance";
		c.effect_id = 239;
		c.vm_table = 0x1864C1C;     // VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186488C;   // BoneHandlerTable
		c.prim_table = 0x18648D4;   // bone_table + 0x48
		c.prim_size = 0x18649C4;    // bone_table + 0x138
		c.spr_table = 0x18649D8;    // bone_table + 0x14C
		c.ptype_table = 0x1864A04;  // bone_table + 0x178
		c.pop_table = 0x1864A18;    // bone_table + 0x18C
		c.draw_table = 0x1864A78;   // DrawHandlerTable
		c.attr_16A = 0x1864BE2;
		c.attr_184 = 0x1864BFC;
		c.attr_186 = 0x1864BFE;
		c.attr_194 = 0x1864C0C;
		c.attr_19C = 0x1864C14;
		c.desc = 0x1864760;         // draw_table - 0x318
		c.ptr_block = 0x1864864;    // draw_table - 0x214
		c.queue = 0x27968D8;
		c.file00 = 0x27968D4;
		c.file01 = 0x27968D0;
		c.tick = 0xA160A0;
		c.load_cb = 0xA1B840;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag239_ochu_dance()
	{
		using namespace gfc;
		ochu_dance::describe(ochu_dance::g_c);
		init_clone(ochu_dance::g_c, nullptr, 0);
		apply_breath_glow(ochu_dance::g_c);                                   // draw 32 0xA16E80
		register_port(0xA160A0, (void *)ochu_dance::SequenceTask, "MAG239 Ochu Dance SequenceTick", 239);
		// 30 fps layer: see mag239_ochu_dance_held.inc
		FX_HELD(register_mag239_ochu_dance_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag239_ochu_dance_held.inc"
#endif
