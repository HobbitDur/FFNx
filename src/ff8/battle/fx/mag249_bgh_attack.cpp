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

// Effect 249: Attack (BGH251F2) (enemy attack 117 of kernel.bin, BGH251F2 c0m102 (damaged form)), cinematic-engine module MAG_249 0x9B7350..0x9C08A0,
// script data magic/mag248_b.00/.01 (loaded by MAG_249_UNKNOWN_FL 0x9B7350; setup MAG_249_UNKNOWN 0x9B7380 =
// effect table index 248).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185E820; its descriptor block 0x185E51C sits 0x304 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcB\tools\cmpall.py / best.py):
//   - SetupSummon 0x9B73E0, BindContext 0x9B7430, the section pointers 0x9B74D0, InitSequenceState
//     0x9BFEB0, SequenceTick 0x9B79F0 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9C0880 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's but for prim 18, live (generic) here), except
//       VM 0x006 LoadBattleFile 0x9BC220: same code, own load callback 0x9BC350 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x27966A0) holds one task, the engine tick 0x9B79F0.

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
		c.vm_table = 0x185E9C4;      // MAG_249 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185E634;    // MAG_249 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185E67C;    // bone_table + 0x48
		c.prim_size = 0x185E76C;     // bone_table + 0x138
		c.spr_table = 0x185E780;     // bone_table + 0x14C
		c.ptype_table = 0x185E7AC;   // bone_table + 0x178
		c.pop_table = 0x185E7C0;     // bone_table + 0x18C
		c.draw_table = 0x185E820;    // MAG_249 DrawHandlerTable
		c.attr_16A = 0x185E98A;
		c.attr_184 = 0x185E9A4;
		c.attr_186 = 0x185E9A6;
		c.attr_194 = 0x185E9B4;
		c.attr_19C = 0x185E9BC;
		c.desc = 0x185E51C;          // draw_table - 0x304 (read by 0x9B74D0)
		c.ptr_block = 0x185E60C;     // draw_table - 0x214 (read by 0x9B7430)
		c.queue = 0x27966A0;
		c.file00 = 0x27966B0;
		c.file01 = 0x27966B4;
		c.tick = 0x9B79F0;
		c.load_cb = 0x9BC350;        // passed by VM 0x006 0x9BC220
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
		// 30 fps layer: see mag249_bgh_attack_held.inc
		FX_HELD(register_mag249_bgh_attack_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag249_bgh_attack_held.inc"
#endif
