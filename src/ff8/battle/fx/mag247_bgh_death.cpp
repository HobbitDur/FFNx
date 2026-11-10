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

// Effect 247: BGH251F2 Death (enemy attack 144 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_247 0x9C9860..0x9D32E0,
// script data magic/mag246_b.00/.01 (loaded by MAG_247_UNKNOWN_FL 0x9C9860; setup MAG_247_UNKNOWN 0x9C9890 =
// effect table index 246).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185FBB8; its descriptor block 0x185F89C sits 0x31C below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9C98F0, BindContext 0x9C9940, the section pointers 0x9C99E0, InitSequenceState
//     0x9D28F0, SequenceTick 0x9C9F00 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9D32C0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 Attack 249's (mag249_bgh_attack.cpp)'s but for prim 7 / prim 17, live (generic)
//     here), except
//       VM 0x006 LoadBattleFile 0x9CEC60: same code, own load callback 0x9CED90 (c.load_cb).
//     No shared or other-clone handler is live: no apply_* call.
// The effect's queue (0x2796718) holds one task, the engine tick 0x9C9F00.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace bghdeath247
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag247_bgh_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace bghdeath247
{
	static void describe(Clone &c)
	{
		c.name = "247 BGH251F2 Death";
		c.effect_id = 247;
		c.vm_table = 0x185FD5C;      // MAG_247 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185F9CC;    // MAG_247 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185FA14;    // bone_table + 0x48
		c.prim_size = 0x185FB04;     // bone_table + 0x138
		c.spr_table = 0x185FB18;     // bone_table + 0x14C
		c.ptype_table = 0x185FB44;   // bone_table + 0x178
		c.pop_table = 0x185FB58;     // bone_table + 0x18C
		c.draw_table = 0x185FBB8;    // MAG_247 DrawHandlerTable
		c.attr_16A = 0x185FD22;
		c.attr_184 = 0x185FD3C;
		c.attr_186 = 0x185FD3E;
		c.attr_194 = 0x185FD4C;
		c.attr_19C = 0x185FD54;
		c.desc = 0x185F89C;          // draw_table - 0x31C (read by 0x9C99E0)
		c.ptr_block = 0x185F9A4;     // draw_table - 0x214 (read by 0x9C9940)
		c.queue = 0x2796718;
		c.file00 = 0x2796710;
		c.file01 = 0x2796714;
		c.tick = 0x9C9F00;
		c.load_cb = 0x9CED90;        // passed by VM 0x006 0x9CEC60
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

	void register_mag247_bgh_death()
	{
		using namespace gfc;
		bghdeath247::describe(bghdeath247::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<247>::Register("BGH251F2 Death", bghdeath247::g_c, 0x9C9860, 0x186027C, 0x1860288,
		                           (void *)bghdeath247::SequenceTask);
		init_clone(bghdeath247::g_c, nullptr, 0);
		register_port(0x9C9F00, (void *)bghdeath247::SequenceTask, "MAG247 BGH251F2 Death SequenceTick", 247);
		// 30 fps layer: see mag247_bgh_death_held.inc
		FX_HELD(register_mag247_bgh_death_held();)
	}
	FX_LAB_MODULE(register_mag247_bgh_death);
}

#ifdef FF8_FX_HELD
#include "mag247_bgh_death_held.inc"
#endif
