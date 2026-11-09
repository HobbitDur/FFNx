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

// Effect 266: Tiamat death (enemy attack 306 of kernel.bin (no name), Tiamat c0m132), cinematic-engine module MAG_266 0x91F5B0..0x92CBA0,
// script data magic/mag265_b.00/.01 (loaded by MAG_266_UNKNOWN_FL 0x91F5B0; setup MAG_266_UNKNOWN 0x91F5E0 =
// effect table index 265). The effect makes the caster's model fall apart (draw 22
// EntityDisintegrate on the slot entity): the monster's death.
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1855C28; its descriptor block 0x18558C8 sits 0x360 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcB\tools\cmpall.py / best.py):
//   - SetupSummon 0x91F640, BindContext 0x91F690, the section pointers 0x91F730, InitSequenceState
//     0x92C1B0, SequenceTick 0x91FC50 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x92CB80 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false (Elvoret's death 18 has Ifrit's lit one);
//   - every table handler is a byte clone of Elvoret's death 18's slot (the generic (Ifrit) handler,
//     a `ret` stub or one of those below; VM 0x042 / 0x093 and prim 12 are stubs here, prim 9 is
//     live (generic) here), except
//       VM 0x006 LoadBattleFile 0x928520: same code, own load callback 0x928650 (c.load_cb),
//       draw 16 0x920500 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0x925480 =
//         Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 22 0x920C90 EntityDisintegrate = Elvoret's death 18's 0xB4BBE0 / Final Form Death 77's
//         0xB322E0 and its 22 callees, byte for byte but the four depth-key cells (0x2795D20..
//         0x2795D2C here) and the return address of its call 0x920F29 to 0x508C90 (0x920F2E, low
//         half 0x0F2E) that a never-written local reads: gfc_disintegrate.inc (DIS_KEYS / DIS_RET16),
//       VM 0x0AF 0x927D20 SetInlineDescriptor (+ its helper) = Elvoret's death 18's 0xB52F20 / Final
//         Form Death's 0xB39820 (kept original there: never reached; Tiamat 266's script reaches it):
//         ported in gfc_disintegrate.inc.
// The effect's queue (0x2795CF8) holds one task, the engine tick 0x91FC50.
// Battle entity writes: draw 22 sets the slot entity's flag +0 |= 4 on its first draw (the real
// model hidden: the copy falls apart) and rebuilds the entity skeleton (pose + world matrices) on
// every draw; the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace tiamat266
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag266_tiamat_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace tiamat266
{
	static void describe(Clone &c)
	{
		c.name = "266 Tiamat death";
		c.effect_id = 266;
		c.vm_table = 0x1855DCC;      // MAG_266 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1855A3C;    // MAG_266 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1855A84;    // bone_table + 0x48
		c.prim_size = 0x1855B74;     // bone_table + 0x138
		c.spr_table = 0x1855B88;     // bone_table + 0x14C
		c.ptype_table = 0x1855BB4;   // bone_table + 0x178
		c.pop_table = 0x1855BC8;     // bone_table + 0x18C
		c.draw_table = 0x1855C28;    // MAG_266 DrawHandlerTable
		c.attr_16A = 0x1855D92;
		c.attr_184 = 0x1855DAC;
		c.attr_186 = 0x1855DAE;
		c.attr_194 = 0x1855DBC;
		c.attr_19C = 0x1855DC4;
		c.desc = 0x18558C8;          // draw_table - 0x360 (read by 0x91F730)
		c.ptr_block = 0x1855A14;     // draw_table - 0x214 (read by 0x91F690)
		c.queue = 0x2795CF8;
		c.file00 = 0x2795D0C;
		c.file01 = 0x2795D08;
		c.tick = 0x91FC50;
		c.load_cb = 0x928650;        // passed by VM 0x006 0x928520
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// draw 22 EntityDisintegrate (gfc_disintegrate.inc): this module's depth-key cells and the low
	// half of the return address 0x920F2E of draw 22's call to 0x508C90
	static const uint32_t DIS_KEYS = 0x2795D20;
	static const uint16_t DIS_RET16 = 0x0F2E;
#include "gfc_disintegrate.inc"

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (apply_shared_misc: only draw 16 /
	// prim 16 are taken), then VM 0x0AF and draw 22
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0x920500 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0x925480 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.vm[0x0AF] = op_0AF_SetInlineDescriptor;       // 0x927D20 (gfc_disintegrate.inc)
		c.draw[22] = dh_22_EntityDisintegrate;          // 0x920C90
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag266_tiamat_death()
	{
		using namespace gfc;
		tiamat266::describe(tiamat266::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<266>::Register("Tiamat death", tiamat266::g_c, 0x91F5B0, 0x18562EC, 0x18562F8,
		                           (void *)tiamat266::SequenceTask);
		init_clone(tiamat266::g_c, nullptr, 0);
		tiamat266::apply_slots(tiamat266::g_c);
		register_port(0x91FC50, (void *)tiamat266::SequenceTask, "MAG266 Tiamat death SequenceTick", 266);
		// 30 fps layer: see mag266_tiamat_death_held.inc
		FX_HELD(register_mag266_tiamat_death_held();)
	}
	FX_LAB_MODULE(register_mag266_tiamat_death);
}

#ifdef FF8_FX_HELD
#include "mag266_tiamat_death_held.inc"
#endif
