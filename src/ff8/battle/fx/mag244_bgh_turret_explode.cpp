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

// Effect 244: BGH251F2's second turret exploding (enemy attacks 141 / 148 of kernel.bin, BGH251F2
// c0m101), cinematic-engine module MAG_244 0x9E57E0..0x9EEA60, script data magic/mag243_b.00/.01
// (loaded by MAG_244_UNKNOWN_FL 0x9E57E0; setup MAG_244_UNKNOWN 0x9E5810 = effect table index 243).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1861904; its descriptor block 0x186161C sits 0x2E8 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcC\tools\cmpall.py):
//   - SetupSummon 0x9E5870, BindContext 0x9E58C0, the section pointers 0x9E5960, InitSequenceState
//     0x9EE070, SequenceTick 0x9E5E80 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9EEA40 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0x9EA3E0: same code, own load callback 0x9EA510 (c.load_cb).
//     Unlike Sand Shake, draw 26 / VM 0x096 are stubs here and prim 8 is live (generic): no shared
//     handler is live, so no apply_* call.
// The effect's queue (0x27967C0, pool 0x27967D0) holds one task, the engine tick 0x9E5E80.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace turret244
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag244_bgh_turret_explode_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace turret244
{
	static void describe(Clone &c)
	{
		c.name = "244 BGH251F2 turret explosion";
		c.effect_id = 244;
		c.vm_table = 0x1861AA8;      // MAG_244 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1861718;    // MAG_244 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1861760;    // bone_table + 0x48
		c.prim_size = 0x1861850;     // bone_table + 0x138
		c.spr_table = 0x1861864;     // bone_table + 0x14C
		c.ptype_table = 0x1861890;   // bone_table + 0x178
		c.pop_table = 0x18618A4;     // bone_table + 0x18C
		c.draw_table = 0x1861904;    // MAG_244 DrawHandlerTable
		c.attr_16A = 0x1861A6E;
		c.attr_184 = 0x1861A88;
		c.attr_186 = 0x1861A8A;
		c.attr_194 = 0x1861A98;
		c.attr_19C = 0x1861AA0;
		c.desc = 0x186161C;          // draw_table - 0x2E8 (read by 0x9E5960)
		c.ptr_block = 0x18616F0;     // draw_table - 0x214 (read by 0x9E58C0)
		c.queue = 0x27967C0;
		c.file00 = 0x27967BC;
		c.file01 = 0x27967B8;
		c.tick = 0x9E5E80;
		c.load_cb = 0x9EA510;        // passed by VM 0x006 0x9EA3E0
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

	void register_mag244_bgh_turret_explode()
	{
		using namespace gfc;
		turret244::describe(turret244::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<244>::Register("BGH251F2 turret explosion", turret244::g_c, 0x9E57E0, 0x1861FC8, 0x1861FD4,
		                           (void *)turret244::SequenceTask);
		init_clone(turret244::g_c, nullptr, 0);
		register_port(0x9E5E80, (void *)turret244::SequenceTask, "MAG244 BGH251F2 turret explosion SequenceTick", 244);
		// 30 fps layer: see mag244_bgh_turret_explode_held.inc
		FX_HELD(register_mag244_bgh_turret_explode_held();)
	}
	FX_LAB_MODULE(register_mag244_bgh_turret_explode);
}

#ifdef FF8_FX_HELD
#include "mag244_bgh_turret_explode_held.inc"
#endif
