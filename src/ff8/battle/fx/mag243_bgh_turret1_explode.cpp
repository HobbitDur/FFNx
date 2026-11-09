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

// Effect 243: BGH251F2 1st turret explosion (enemy attacks 140 / 147 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_243 0x9EEA60..0x9F7CE0,
// script data magic/mag242_b.00/.01 (loaded by MAG_243_UNKNOWN_FL 0x9EEA60; setup MAG_243_UNKNOWN 0x9EEA90 =
// effect table index 242).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x18622C8; its descriptor block 0x1861FE0 sits 0x2E8 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9EEAF0, BindContext 0x9EEB40, the section pointers 0x9EEBE0, InitSequenceState
//     0x9F72F0, SequenceTick 0x9EF100 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9F7CC0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's (mag244_bgh_turret_explode.cpp)'s), except
//       VM 0x006 LoadBattleFile 0x9F3660: same code, own load callback 0x9F3790 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x27967F0) holds one task, the engine tick 0x9EF100.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace turret243
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag243_bgh_turret1_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret243
{
	static void describe(Clone &c)
	{
		c.name = "243 BGH251F2 1st turret explosion";
		c.effect_id = 243;
		c.vm_table = 0x186246C;      // MAG_243 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18620DC;    // MAG_243 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1862124;    // bone_table + 0x48
		c.prim_size = 0x1862214;     // bone_table + 0x138
		c.spr_table = 0x1862228;     // bone_table + 0x14C
		c.ptype_table = 0x1862254;   // bone_table + 0x178
		c.pop_table = 0x1862268;     // bone_table + 0x18C
		c.draw_table = 0x18622C8;    // MAG_243 DrawHandlerTable
		c.attr_16A = 0x1862432;
		c.attr_184 = 0x186244C;
		c.attr_186 = 0x186244E;
		c.attr_194 = 0x186245C;
		c.attr_19C = 0x1862464;
		c.desc = 0x1861FE0;          // draw_table - 0x2E8 (read by 0x9EEBE0)
		c.ptr_block = 0x18620B4;     // draw_table - 0x214 (read by 0x9EEB40)
		c.queue = 0x27967F0;
		c.file00 = 0x2796804;
		c.file01 = 0x2796800;
		c.tick = 0x9EF100;
		c.load_cb = 0x9F3790;        // passed by VM 0x006 0x9F3660
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag243_bgh_turret1_explode()
	{
		using namespace gfc;
		turret243::describe(turret243::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<243>::Register("1st turret exploding (BGH251F2)", turret243::g_c, 0x9EEA60, 0x186298C, 0x1862998,
		                           (void *)turret243::SequenceTask);
		init_clone(turret243::g_c, nullptr, 0);
		register_port(0x9EF100, (void *)turret243::SequenceTask, "MAG243 BGH251F2 1st turret explosion SequenceTick", 243);
		// 30 fps layer: see mag243_bgh_turret1_explode_held.inc
		FX_HELD(register_mag243_bgh_turret1_explode_held();)
	}
	FX_LAB_MODULE(register_mag243_bgh_turret1_explode);
}

#ifdef FF8_FX_HELD
#include "mag243_bgh_turret1_explode_held.inc"
#endif
