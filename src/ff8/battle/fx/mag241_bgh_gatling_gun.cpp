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

// Effect 241: Gatling Gun (BGH251F2) (enemy attack 138 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_241 0xA01DE0..0xA0B330,
// script data magic/mag240_b.00/.01 (loaded by MAG_241_UNKNOWN_FL 0xA01DE0; setup MAG_241_UNKNOWN 0xA01E10 =
// effect table index 240).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186368C; its descriptor block 0x1863388 sits 0x304 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0xA01E70, BindContext 0xA01EC0, the section pointers 0xA01F60, InitSequenceState
//     0xA0A940, SequenceTick 0xA02480 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA0B310 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 Attack 249's (mag249_bgh_attack.cpp)'s), except
//       VM 0x006 LoadBattleFile 0xA06CB0: same code, own load callback 0xA06DE0 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x2796860) holds one task, the engine tick 0xA02480.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace gatling241
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag241_bgh_gatling_gun_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace gatling241
{
	static void describe(Clone &c)
	{
		c.name = "241 Gatling Gun (BGH251F2)";
		c.effect_id = 241;
		c.vm_table = 0x1863830;      // MAG_241 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18634A0;    // MAG_241 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18634E8;    // bone_table + 0x48
		c.prim_size = 0x18635D8;     // bone_table + 0x138
		c.spr_table = 0x18635EC;     // bone_table + 0x14C
		c.ptype_table = 0x1863618;   // bone_table + 0x178
		c.pop_table = 0x186362C;     // bone_table + 0x18C
		c.draw_table = 0x186368C;    // MAG_241 DrawHandlerTable
		c.attr_16A = 0x18637F6;
		c.attr_184 = 0x1863810;
		c.attr_186 = 0x1863812;
		c.attr_194 = 0x1863820;
		c.attr_19C = 0x1863828;
		c.desc = 0x1863388;          // draw_table - 0x304 (read by 0xA01F60)
		c.ptr_block = 0x1863478;     // draw_table - 0x214 (read by 0xA01EC0)
		c.queue = 0x2796860;
		c.file00 = 0x2796874;
		c.file01 = 0x2796870;
		c.tick = 0xA02480;
		c.load_cb = 0xA06DE0;        // passed by VM 0x006 0xA06CB0
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag241_bgh_gatling_gun()
	{
		using namespace gfc;
		gatling241::describe(gatling241::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<241>::Register("Gatling Gun (BGH251F2)", gatling241::g_c, 0xA01DE0, 0x1863D50, 0x1863D5C,
		                           (void *)gatling241::SequenceTask);
		init_clone(gatling241::g_c, nullptr, 0);
		register_port(0xA02480, (void *)gatling241::SequenceTask, "MAG241 Gatling Gun (BGH251F2) SequenceTick", 241);
		// 30 fps layer: see mag241_bgh_gatling_gun_held.inc
		FX_HELD(register_mag241_bgh_gatling_gun_held();)
	}
	FX_LAB_MODULE(register_mag241_bgh_gatling_gun);
}

#ifdef FF8_FX_HELD
#include "mag241_bgh_gatling_gun_held.inc"
#endif
