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

// Effect 246: BGH251F2 4th turret explosion (enemy attacks 143 / 150 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_246 0x9D32E0..0x9DC560,
// script data magic/mag245_b.00/.01 (loaded by MAG_246_UNKNOWN_FL 0x9D32E0; setup MAG_246_UNKNOWN 0x9D3310 =
// effect table index 245).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186057C; its descriptor block 0x1860294 sits 0x2E8 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9D3370, BindContext 0x9D33C0, the section pointers 0x9D3460, InitSequenceState
//     0x9DBB70, SequenceTick 0x9D3980 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9DC540 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's (mag244_bgh_turret_explode.cpp)'s), except
//       VM 0x006 LoadBattleFile 0x9D7EE0: same code, own load callback 0x9D8010 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x2796750) holds one task, the engine tick 0x9D3980.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace turret246
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag246_bgh_turret4_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret246
{
	static void describe(Clone &c)
	{
		c.name = "246 BGH251F2 4th turret explosion";
		c.effect_id = 246;
		c.vm_table = 0x1860720;      // MAG_246 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1860390;    // MAG_246 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18603D8;    // bone_table + 0x48
		c.prim_size = 0x18604C8;     // bone_table + 0x138
		c.spr_table = 0x18604DC;     // bone_table + 0x14C
		c.ptype_table = 0x1860508;   // bone_table + 0x178
		c.pop_table = 0x186051C;     // bone_table + 0x18C
		c.draw_table = 0x186057C;    // MAG_246 DrawHandlerTable
		c.attr_16A = 0x18606E6;
		c.attr_184 = 0x1860700;
		c.attr_186 = 0x1860702;
		c.attr_194 = 0x1860710;
		c.attr_19C = 0x1860718;
		c.desc = 0x1860294;          // draw_table - 0x2E8 (read by 0x9D3460)
		c.ptr_block = 0x1860368;     // draw_table - 0x214 (read by 0x9D33C0)
		c.queue = 0x2796750;
		c.file00 = 0x2796748;
		c.file01 = 0x279674C;
		c.tick = 0x9D3980;
		c.load_cb = 0x9D8010;        // passed by VM 0x006 0x9D7EE0
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag246_bgh_turret4_explode()
	{
		using namespace gfc;
		turret246::describe(turret246::g_c);
		init_clone(turret246::g_c, nullptr, 0);
		register_port(0x9D3980, (void *)turret246::SequenceTask, "MAG246 BGH251F2 4th turret explosion SequenceTick", 246);
		// 30 fps layer: see mag246_bgh_turret4_explode_held.inc
		FX_HELD(register_mag246_bgh_turret4_explode_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag246_bgh_turret4_explode_held.inc"
#endif
