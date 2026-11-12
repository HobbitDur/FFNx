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

// Effect 265: Sphinxara 265 (enemy attack 259 of kernel.bin (no name), Sphinxara c0m012), cinematic-engine module MAG_265 0x92CBA0..0x937E80,
// script data magic/mag264_b.00 / mag264_b.01 (loaded by MAG_265_UNKNOWN_FL 0x92CBA0: IO_GetFile_MAGIC of the names
// 0x1856D30 / 0x1856D3C; setup MAG_265_UNKNOWN 0x92CBD0 = effect table index 264). The effect
// melts the caster's model into the ground (draw 52 EntityMelt on the slot entity).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185666C; its descriptor block 0x1856304 sits 0x368 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcR\tools\cmpall.py / best.py / fdiff.py): the handler set is Ultima Weapon Death 270's
// (mag270_ultima_weapon_death.cpp) without its draw 22 / VM 0x0AF (stubs here), plus draw 3 and draw 52:
//   - SetupSummon 0x92CC30, BindContext 0x92CC80, the section pointers 0x92CD20, InitSequenceState
//     0x937490, SequenceTick 0x92D240 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x937E60 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is the generic (Ifrit) handler, a `ret` stub or one of those below:
//       VM 0x006 LoadBattleFile 0x933800: same code, own load callback 0x933930 (c.load_cb),
//       draw 16 0x92DAF0 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0x9307C0 =
//         Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 3 0x92E170 AnimatedModelWithShadow = Brothers 205's 0xAF5810: gfc_melt.inc,
//       draw 52 0x92E430 EntityMelt and its callees (no other ported module has it; effects 208 and
//         265 carry the same code): gfc_melt.inc.
// The effect's queue (0x2795D30) holds one task, the engine tick 0x92D240.
// Battle entity writes: draw 52 sets the slot entity's flag +0 |= 4 on its first draw (the real
// model hidden: the copy melts) and rebuilds the entity skeleton (pose + world matrices) on every
// draw; the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace sphinxara265
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag265_sphinxara_265_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sphinxara265
{
	static void describe(Clone &c)
	{
		c.name = "265 Sphinxara";
		c.effect_id = 265;
		c.vm_table = 0x1856810;      // MAG_265 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1856480;    // MAG_265 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18564C8;    // bone_table + 0x48
		c.prim_size = 0x18565B8;     // bone_table + 0x138
		c.spr_table = 0x18565CC;     // bone_table + 0x14C
		c.ptype_table = 0x18565F8;   // bone_table + 0x178
		c.pop_table = 0x185660C;     // bone_table + 0x18C
		c.draw_table = 0x185666C;    // MAG_265 DrawHandlerTable
		c.attr_16A = 0x18567D6;
		c.attr_184 = 0x18567F0;
		c.attr_186 = 0x18567F2;
		c.attr_194 = 0x1856800;
		c.attr_19C = 0x1856808;
		c.desc = 0x1856304;          // draw_table - 0x368 (read by 0x92CD20)
		c.ptr_block = 0x1856458;     // draw_table - 0x214 (read by 0x92CC80)
		c.queue = 0x2795D30;
		c.file00 = 0x2795D44;
		c.file01 = 0x2795D40;
		c.tick = 0x92D240;
		c.load_cb = 0x933930;        // passed by VM 0x006 0x933800
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// draw 52 EntityMelt 0x92E430, draw 3 AnimatedModelWithShadow 0x92E170
#include "gfc_melt.inc"

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (apply_shared_misc: only draw 16 /
	// prim 16 are taken) and draws 3 / 52
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0x92DAF0 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0x9307C0 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.draw[3] = dh_03_AnimatedModelWithShadow;      // 0x92E170 (gfc_melt.inc)
		c.draw[52] = dh_52_EntityMelt;                  // 0x92E430 (gfc_melt.inc)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag265_sphinxara_265()
	{
		using namespace gfc;
		sphinxara265::describe(sphinxara265::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<265>::Register("Sphinxara (attack 259)", sphinxara265::g_c, 0x92CBA0, 0x1856D30, 0x1856D3C,
		                           (void *)sphinxara265::SequenceTask);
		init_clone(sphinxara265::g_c, nullptr, 0);
		sphinxara265::apply_slots(sphinxara265::g_c);
		register_port(0x92D240, (void *)sphinxara265::SequenceTask, "MAG265 Sphinxara 265 SequenceTick", 265);
		// 30 fps layer: see mag265_sphinxara_265_held.inc
		FX_HELD(register_mag265_sphinxara_265_held();)
	}
	FX_LAB_MODULE(register_mag265_sphinxara_265);
}

#ifdef FF8_FX_HELD
#include "mag265_sphinxara_265_held.inc"
#endif
