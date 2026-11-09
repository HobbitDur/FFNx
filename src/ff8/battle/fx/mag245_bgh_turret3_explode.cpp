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

// Effect 245: BGH251F2 3rd turret explosion (enemy attacks 142 / 149 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_245 0x9DC560..0x9E57E0,
// script data magic/mag244_b.00/.01 (loaded by MAG_245_UNKNOWN_FL 0x9DC560; setup MAG_245_UNKNOWN 0x9DC590 =
// effect table index 244).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1860F40; its descriptor block 0x1860C58 sits 0x2E8 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9DC5F0, BindContext 0x9DC640, the section pointers 0x9DC6E0, InitSequenceState
//     0x9E4DF0, SequenceTick 0x9DCC00 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9E57C0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's (mag244_bgh_turret_explode.cpp)'s), except
//       VM 0x006 LoadBattleFile 0x9E1160: same code, own load callback 0x9E1290 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x2796788) holds one task, the engine tick 0x9DCC00.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace turret245
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag245_bgh_turret3_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret245
{
	static void describe(Clone &c)
	{
		c.name = "245 BGH251F2 3rd turret explosion";
		c.effect_id = 245;
		c.vm_table = 0x18610E4;      // MAG_245 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1860D54;    // MAG_245 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1860D9C;    // bone_table + 0x48
		c.prim_size = 0x1860E8C;     // bone_table + 0x138
		c.spr_table = 0x1860EA0;     // bone_table + 0x14C
		c.ptype_table = 0x1860ECC;   // bone_table + 0x178
		c.pop_table = 0x1860EE0;     // bone_table + 0x18C
		c.draw_table = 0x1860F40;    // MAG_245 DrawHandlerTable
		c.attr_16A = 0x18610AA;
		c.attr_184 = 0x18610C4;
		c.attr_186 = 0x18610C6;
		c.attr_194 = 0x18610D4;
		c.attr_19C = 0x18610DC;
		c.desc = 0x1860C58;          // draw_table - 0x2E8 (read by 0x9DC6E0)
		c.ptr_block = 0x1860D2C;     // draw_table - 0x214 (read by 0x9DC640)
		c.queue = 0x2796788;
		c.file00 = 0x2796784;
		c.file01 = 0x2796780;
		c.tick = 0x9DCC00;
		c.load_cb = 0x9E1290;        // passed by VM 0x006 0x9E1160
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag245_bgh_turret3_explode()
	{
		using namespace gfc;
		turret245::describe(turret245::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<245>::Register("3rd turret exploding (BGH251F2)", turret245::g_c, 0x9DC560, 0x1861604, 0x1861610,
		                           (void *)turret245::SequenceTask);
		init_clone(turret245::g_c, nullptr, 0);
		register_port(0x9DCC00, (void *)turret245::SequenceTask, "MAG245 BGH251F2 3rd turret explosion SequenceTick", 245);
		// 30 fps layer: see mag245_bgh_turret3_explode_held.inc
		FX_HELD(register_mag245_bgh_turret3_explode_held();)
	}
	FX_LAB_MODULE(register_mag245_bgh_turret3_explode);
}

#ifdef FF8_FX_HELD
#include "mag245_bgh_turret3_explode_held.inc"
#endif
