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

// Effect 269: Gargantua 269 (enemy attack 266 of kernel.bin (no name), Gargantua c0m094), cinematic-engine module MAG_269 0x8F71F0..0x9049D0,
// script data magic/mag268_b.00/.01 (loaded by MAG_269_UNKNOWN_FL 0x8F71F0: IO_GetFile_MAGIC of the names
// 0x1854428 / 0x1854434; setup MAG_269_UNKNOWN 0x8F7220 = effect table index 268). The effect
// makes the caster's model fall apart (draw 22 EntityDisintegrate on the slot entity) while draw 47
// shifts a palette read back from VRAM through HSV and tints battle entities 3 / 4 with it.
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1853D64; its descriptor block 0x1853A0C sits 0x358 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcX\tools\cmpall.py / best.py): Ultima Weapon Death 270 (gfc_disintegrate.inc)
// plus VM 0x035 and draw 47:
//   - SetupSummon 0x8F7280, BindContext 0x8F72D0, the section pointers 0x8F7370, InitSequenceState
//     0x903FE0, SequenceTick 0x8F7890 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9049B0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of Ultima Weapon Death 270's slot (the generic (Ifrit)
//     handler, a `ret` stub or one of those below), except
//       VM 0x006 LoadBattleFile 0x9002D0: same code, own load callback 0x900400 (c.load_cb),
//       draw 16 0x8F8140 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0x8FD230 =
//         Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 22 0x8F88D0 EntityDisintegrate = Ultima Weapon Death 270's / Elvoret's death 18's tree,
//         byte for byte but the four depth-key cells (0x2795C78..0x2795C84 here) and the return
//         address of its call 0x8F8B69 to 0x508C90 (0x8F8B6E, low half 0x8B6E) that a never-written
//         local reads: gfc_disintegrate.inc (DIS_KEYS / DIS_RET16),
//       VM 0x0AF 0x8FFAD0 SetInlineDescriptor (+ its helper): gfc_disintegrate.inc,
//       VM 0x035 0x902F80 VramReadback = Meteor 223's 0xA996A0: gfc_palshift.inc,
//       draw 47 0x8FB8B0 PaletteHsvShift (no other ported module has it; Adel's death 211 and effect
//         269 carry the same code): gfc_palshift.inc.
// The effect's queue (0x2795C50) holds one task, the engine tick 0x8F7890.
// Battle entity writes: draw 22 sets the slot entity's flag +0 |= 4 on its first draw (the real
// model hidden: the copy falls apart) and rebuilds the entity skeleton (pose + world matrices) on
// every draw; draw 47 writes the colour word +0x28 of entities 3 / 4 and 0xB8BA00..0xB8BA07; the
// battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace gargantua269
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag269_gargantua_269_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace gargantua269
{
	static void describe(Clone &c)
	{
		c.name = "269 Gargantua";
		c.effect_id = 269;
		c.vm_table = 0x1853F08;      // MAG_269 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1853B78;    // MAG_269 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1853BC0;    // bone_table + 0x48
		c.prim_size = 0x1853CB0;     // bone_table + 0x138
		c.spr_table = 0x1853CC4;     // bone_table + 0x14C
		c.ptype_table = 0x1853CF0;   // bone_table + 0x178
		c.pop_table = 0x1853D04;     // bone_table + 0x18C
		c.draw_table = 0x1853D64;    // MAG_269 DrawHandlerTable
		c.attr_16A = 0x1853ECE;
		c.attr_184 = 0x1853EE8;
		c.attr_186 = 0x1853EEA;
		c.attr_194 = 0x1853EF8;
		c.attr_19C = 0x1853F00;
		c.desc = 0x1853A0C;          // draw_table - 0x358 (read by 0x8F7370)
		c.ptr_block = 0x1853B50;     // draw_table - 0x214 (read by 0x8F72D0)
		c.queue = 0x2795C50;
		c.file00 = 0x2795C74;
		c.file01 = 0x2795C70;
		c.tick = 0x8F7890;
		c.load_cb = 0x900400;        // passed by VM 0x006 0x9002D0
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// draw 22 EntityDisintegrate (gfc_disintegrate.inc): this module's depth-key cells and the low
	// half of the return address 0x8F8B6E of draw 22's call to 0x508C90
	static const uint32_t DIS_KEYS = 0x2795C78;
	static const uint16_t DIS_RET16 = 0x8B6E;
#include "gfc_disintegrate.inc"

	// VM 0x035 VramReadback 0x902F80, draw 47 PaletteHsvShift 0x8FB8B0
#include "gfc_palshift.inc"

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (apply_shared_misc: only draw 16 /
	// prim 16 are taken), then VM 0x0AF / 0x035 and draws 22 / 47
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0x8F8140 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0x8FD230 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.vm[0x0AF] = op_0AF_SetInlineDescriptor;       // 0x8FFAD0 (gfc_disintegrate.inc)
		c.vm[0x035] = op_035_VramReadback;              // 0x902F80 (gfc_palshift.inc)
		c.draw[22] = dh_22_EntityDisintegrate;          // 0x8F88D0
		c.draw[47] = dh_47_PaletteHsvShift;             // 0x8FB8B0 (gfc_palshift.inc)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		// 30 fps layer: the colour words of the real tick back where a held frame drew (gfc_palshift_held.inc)
		FX_HELD(PalShiftRestore();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag269_gargantua_269()
	{
		using namespace gfc;
		gargantua269::describe(gargantua269::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<269>::Register("Gargantua (attack 266)", gargantua269::g_c, 0x8F71F0, 0x1854428, 0x1854434,
		                           (void *)gargantua269::SequenceTask);
		init_clone(gargantua269::g_c, nullptr, 0);
		gargantua269::apply_slots(gargantua269::g_c);
		register_port(0x8F7890, (void *)gargantua269::SequenceTask, "MAG269 Gargantua 269 SequenceTick", 269);
		// 30 fps layer: see mag269_gargantua_269_held.inc
		FX_HELD(register_mag269_gargantua_269_held();)
	}
	FX_LAB_MODULE(register_mag269_gargantua_269);
}

#ifdef FF8_FX_HELD
#include "mag269_gargantua_269_held.inc"
#endif
