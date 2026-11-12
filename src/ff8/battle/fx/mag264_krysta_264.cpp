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

// Effect 264: Krysta 264 (enemy attack 277 of kernel.bin (no name), Krysta c0m038), cinematic-engine module MAG_264 0x937E80..0x9457E0,
// script data magic/mag263_b.00/.01 (loaded by MAG_264_UNKNOWN_FL 0x937E80; setup MAG_264_UNKNOWN 0x937EB0 =
// effect table index 263). The effect makes the caster's model fall apart (draw 22
// EntityDisintegrate on the slot entity): the monster's death.
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x18570A0; its descriptor block 0x1856D48 sits 0x358 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcR\tools\cmpall.py / best.py; in this layout the only edx-indexed
// table is the bone table 0x1856EB4, draw table = bone table + 0x1EC, its dispatch [eax*4] / [ecx*4] at 0x940282 / 0x9402AF):
//   - SetupSummon 0x937F10, BindContext 0x937F60, the section pointers 0x938000, InitSequenceState
//     0x944DF0 and the initial frame limit 0x9457C0 are byte clones of Sand Shake 256's (0x979460 /
//     0x9794B0 / 0x979550 / 0x981C30 / 0x982600); SequenceTick 0x938520 is Ifrit's 0xB25DF0 (the lit
//     BuildMatricesAndDraw, as Elvoret's death 18): lit_dispatcher true;
//   - every table handler is a byte clone of Ultima Weapon Death 270's slot (the generic (Ifrit)
//     handler, a `ret` stub or one of those below) but VM 0x093 (generic here, a stub there), except
//       VM 0x006 LoadBattleFile 0x940F30: same code, own load callback 0x941060 (c.load_cb),
//       draw 16 0x938DD0 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0x93DD50 =
//         Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 22 0x939560 EntityDisintegrate = Elvoret's death 18's 0xB4BBE0 / Final Form Death 77's
//         0xB322E0 and its 22 callees, byte for byte but the four depth-key cells (0x2795D90..
//         0x2795D9C here) and the return address of its call 0x9397F9 to 0x508C90 (0x9397FE, low
//         half 0x97FE) that a never-written local reads: gfc_disintegrate.inc (DIS_KEYS / DIS_RET16),
//       VM 0x0AF 0x940730 SetInlineDescriptor (+ its helper) = Elvoret's death 18's 0xB52F20 / Final
//         Form Death's 0xB39820 (kept original there: never reached; Tiamat 266's script reaches it):
//         ported in gfc_disintegrate.inc.
// The effect's queue (0x2795D68) holds one task, the engine tick 0x938520.
// Battle entity writes: draw 22 sets the slot entity's flag +0 |= 4 on its first draw (the real
// model hidden: the copy falls apart) and rebuilds the entity skeleton (pose + world matrices) on
// every draw; the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace krysta264
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag264_krysta_264_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace krysta264
{
	static void describe(Clone &c)
	{
		c.name = "264 Krysta";
		c.effect_id = 264;
		c.vm_table = 0x1857244;      // MAG_264 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1856EB4;    // MAG_264 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1856EFC;    // bone_table + 0x48
		c.prim_size = 0x1856FEC;     // bone_table + 0x138
		c.spr_table = 0x1857000;     // bone_table + 0x14C
		c.ptype_table = 0x185702C;   // bone_table + 0x178
		c.pop_table = 0x1857040;     // bone_table + 0x18C
		c.draw_table = 0x18570A0;    // MAG_264 DrawHandlerTable
		c.attr_16A = 0x185720A;
		c.attr_184 = 0x1857224;
		c.attr_186 = 0x1857226;
		c.attr_194 = 0x1857234;
		c.attr_19C = 0x185723C;
		c.desc = 0x1856D48;          // draw_table - 0x358 (read by 0x938000)
		c.ptr_block = 0x1856E8C;     // draw_table - 0x214 (read by 0x937F60)
		c.queue = 0x2795D68;
		c.file00 = 0x2795D8C;
		c.file01 = 0x2795D88;
		c.tick = 0x938520;
		c.load_cb = 0x941060;        // passed by VM 0x006 0x940F30
		c.lit_dispatcher = true;
		c.bone_count = 18;
	}

	// draw 22 EntityDisintegrate (gfc_disintegrate.inc): this module's depth-key cells and the low
	// half of the return address 0x9397FE of draw 22's call to 0x508C90
	static const uint32_t DIS_KEYS = 0x2795D90;
	static const uint16_t DIS_RET16 = 0x97FE;
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
		const VoidFn d16 = c.draw[16];        // 0x938DD0 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0x93DD50 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.vm[0x0AF] = op_0AF_SetInlineDescriptor;       // 0x940730 (gfc_disintegrate.inc)
		c.draw[22] = dh_22_EntityDisintegrate;          // 0x939560
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag264_krysta_264()
	{
		using namespace gfc;
		krysta264::describe(krysta264::g_c);
		init_clone(krysta264::g_c, nullptr, 0);
		krysta264::apply_slots(krysta264::g_c);
		register_port(0x938520, (void *)krysta264::SequenceTask, "MAG264 Krysta 264 SequenceTick", 264);
		// 30 fps layer: see mag264_krysta_264_held.inc
		FX_HELD(register_mag264_krysta_264_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag264_krysta_264_held.inc"
#endif
