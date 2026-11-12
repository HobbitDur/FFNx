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

// Effect 208: Final Sorceress Death (enemy attack 328 of kernel.bin, Final "Sorceress" Death of "Sorceress" (Wormlike) c0m117), cinematic-engine module MAG_208 0xAD78D0..0xAE2DA0,
// script data magic/mag207_b.00 / mag207_b.01 (loaded by MAG_208_FINAL_SORCERESS_DEATH_FL 0xAD78D0: IO_GetFile_MAGIC of the names
// 0x1870628 / 0x1870634; setup MAG_208_FINAL_SORCERESS_DEATH 0xAD7900 = effect table index 207). The effect
// melts the caster's model into the ground (draw 52 EntityMelt on the slot entity) while draw 47
// shifts a palette read back from VRAM through HSV and tints battle entities 3 / 4 with it.
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186FF64; its descriptor block 0x186FBEC sits 0x378 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcR\tools\cmpall.py / best.py / fdiff.py): the handler set is effect 269 (Gargantua)'s
// (mag269_gargantua_269.cpp) without its draw 22 / VM 0x0AF (stubs here), plus draw 3 and draw 52:
//   - SetupSummon 0xAD7960, BindContext 0xAD79B0, the section pointers 0xAD7A50, InitSequenceState
//     0xAE23B0, SequenceTick 0xAD7F70 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xAE2D80 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is the generic (Ifrit) handler, a `ret` stub or one of those below:
//       VM 0x006 LoadBattleFile 0xADE6A0: same code, own load callback 0xADE7D0 (c.load_cb),
//       draw 16 0xAD8820 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0xADB660 =
//         Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 3 0xAD8EA0 AnimatedModelWithShadow = Brothers 205's 0xAF5810: gfc_melt.inc,
//       draw 52 0xAD9160 EntityMelt and its callees (no other ported module has it; effects 208 and
//         265 carry the same code): gfc_melt.inc.
//       VM 0x035 0xAE1350 VramReadback = Meteor 223's 0xA996A0: gfc_palshift.inc,
//       draw 47 0xAD9BF0 PaletteHsvShift (= Adel's death 211 0xACE7B0 / effect 269 0x8FB8B0): gfc_palshift.inc.
// The effect's queue (0x2796CC0) holds one task, the engine tick 0xAD7F70.
// Battle entity writes: draw 52 sets the slot entity's flag +0 |= 4 on its first draw (the real
// model hidden: the copy melts) and rebuilds the entity skeleton (pose + world matrices) on every
// draw; draw 47 writes the colour word +0x28 of entities 3 / 4 and 0xB8BA00..0xB8BA07; the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace sorceress208
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag208_final_sorceress_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sorceress208
{
	static void describe(Clone &c)
	{
		c.name = "208 Final Sorceress Death";
		c.effect_id = 208;
		c.vm_table = 0x1870108;      // MAG_208 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186FD78;    // MAG_208 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186FDC0;    // bone_table + 0x48
		c.prim_size = 0x186FEB0;     // bone_table + 0x138
		c.spr_table = 0x186FEC4;     // bone_table + 0x14C
		c.ptype_table = 0x186FEF0;   // bone_table + 0x178
		c.pop_table = 0x186FF04;     // bone_table + 0x18C
		c.draw_table = 0x186FF64;    // MAG_208 DrawHandlerTable
		c.attr_16A = 0x18700CE;
		c.attr_184 = 0x18700E8;
		c.attr_186 = 0x18700EA;
		c.attr_194 = 0x18700F8;
		c.attr_19C = 0x1870100;
		c.desc = 0x186FBEC;          // draw_table - 0x378 (read by 0xAD7A50)
		c.ptr_block = 0x186FD50;     // draw_table - 0x214 (read by 0xAD79B0)
		c.queue = 0x2796CC0;
		c.file00 = 0x2796CF0;
		c.file01 = 0x2796CF4;
		c.tick = 0xAD7F70;
		c.load_cb = 0xADE7D0;        // passed by VM 0x006 0xADE6A0
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// draw 52 EntityMelt 0xAD9160, draw 3 AnimatedModelWithShadow 0xAD8EA0
#include "gfc_melt.inc"

	// VM 0x035 VramReadback 0xAE1350, draw 47 PaletteHsvShift 0xAD9BF0
#include "gfc_palshift.inc"

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (apply_shared_misc: only draw 16 /
	// prim 16 are taken), VM 0x035 and draws 3 / 47 / 52
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0xAD8820 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0xADB660 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.draw[3] = dh_03_AnimatedModelWithShadow;      // 0xAD8EA0 (gfc_melt.inc)
		c.draw[52] = dh_52_EntityMelt;                  // 0xAD9160 (gfc_melt.inc)
		c.vm[0x035] = op_035_VramReadback;              // 0xAE1350 (gfc_palshift.inc)
		c.draw[47] = dh_47_PaletteHsvShift;             // 0xAD9BF0 (gfc_palshift.inc)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		// 30 fps layer: the colour words of the real tick back where a held frame drew (gfc_palshift_held.inc)
		FX_HELD(PalShiftRestore();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag208_final_sorceress_death()
	{
		using namespace gfc;
		sorceress208::describe(sorceress208::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<208>::Register("Final Sorceress Death", sorceress208::g_c, 0xAD78D0, 0x1870628, 0x1870634,
		                           (void *)sorceress208::SequenceTask);
		init_clone(sorceress208::g_c, nullptr, 0);
		sorceress208::apply_slots(sorceress208::g_c);
		register_port(0xAD7F70, (void *)sorceress208::SequenceTask, "MAG208 Final Sorceress Death SequenceTick", 208);
		// 30 fps layer: see mag208_final_sorceress_death_held.inc
		FX_HELD(register_mag208_final_sorceress_death_held();)
	}
	FX_LAB_MODULE(register_mag208_final_sorceress_death);
}

#ifdef FF8_FX_HELD
#include "mag208_final_sorceress_death_held.inc"
#endif
