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

// Effect 252: Corona (Mobile Type 8) (enemy attack 284 of kernel.bin, Mobile Type 8 c0m105), cinematic-engine module MAG_252 0x9A2840..0x9ABFC0,
// script data magic/mag251_b.00/.01 (loaded by MAG_252_UNKNOWN_FL 0x9A2840; setup MAG_252_UNKNOWN 0x9A2870 =
// effect table index 251).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185D45C; its descriptor block 0x185D1C0 sits 0x29C below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9A28D0, BindContext 0x9A2920, the section pointers 0x9A29C0, InitSequenceState
//     0x9AB5D0, SequenceTick 0x9A2EE0 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9ABFA0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals Homing Laser 282 255's but for VM 0x0AD, prim 18 and draw 37, stubs here, and prim 16,
//     live here), except
//       VM 0x006 LoadBattleFile 0x9A7940: same code, own load callback 0x9A7A70 (c.load_cb),
//       prim 16 0x9A4EF0 = Fire Breath 230's 0xA735D0 / Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
//         (apply_shared_misc's port: only that slot is taken, as Fire Breath does).
// The effect's queue (0x2796630) holds one task, the engine tick 0x9A2EE0.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace corona252
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag252_corona_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace corona252
{
	static void describe(Clone &c)
	{
		c.name = "252 Corona (Mobile Type 8)";
		c.effect_id = 252;
		c.vm_table = 0x185D600;      // MAG_252 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185D270;    // MAG_252 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185D2B8;    // bone_table + 0x48
		c.prim_size = 0x185D3A8;     // bone_table + 0x138
		c.spr_table = 0x185D3BC;     // bone_table + 0x14C
		c.ptype_table = 0x185D3E8;   // bone_table + 0x178
		c.pop_table = 0x185D3FC;     // bone_table + 0x18C
		c.draw_table = 0x185D45C;    // MAG_252 DrawHandlerTable
		c.attr_16A = 0x185D5C6;
		c.attr_184 = 0x185D5E0;
		c.attr_186 = 0x185D5E2;
		c.attr_194 = 0x185D5F0;
		c.attr_19C = 0x185D5F8;
		c.desc = 0x185D1C0;          // draw_table - 0x29C (read by 0x9A29C0)
		c.ptr_block = 0x185D248;     // draw_table - 0x214 (read by 0x9A2920)
		c.queue = 0x2796630;
		c.file00 = 0x2796628;
		c.file01 = 0x279662C;
		c.tick = 0x9A2EE0;
		c.load_cb = 0x9A7A70;        // passed by VM 0x006 0x9A7940
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// ------------------------------------------------------------------------------------
	// handler set: the shared prim 16 (Cerberus / Eden TintedF4: the only shared slot this
	// module carries)
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn p16 = c.prim[16];             // 0x9A4EF0 = Cerberus 0xB105B0 / Eden 0xAEC030
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[16] = p16;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag252_corona()
	{
		using namespace gfc;
		corona252::describe(corona252::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<252>::Register("Corona (Mobile Type 8)", corona252::g_c, 0x9A2840, 0x185DB20, 0x185DB2C,
		                           (void *)corona252::SequenceTask);
		init_clone(corona252::g_c, nullptr, 0);
		corona252::apply_slots(corona252::g_c);
		register_port(0x9A2EE0, (void *)corona252::SequenceTask, "MAG252 Corona (Mobile Type 8) SequenceTick", 252);
		// 30 fps layer: see mag252_corona_held.inc
		FX_HELD(register_mag252_corona_held();)
	}
	FX_LAB_MODULE(register_mag252_corona);
}

#ifdef FF8_FX_HELD
#include "mag252_corona_held.inc"
#endif
