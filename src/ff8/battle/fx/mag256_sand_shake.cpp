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

// Effect 256: Sand Shake (enemy attack 174 of kernel.bin, Abyss Worm c0m044, Deling-region desert;
// no other kernel.bin entry uses effect 256), cinematic-engine module MAG_256 0x9793D0..0x982620,
// script data magic/mag255_b.00/.01 (loaded by MAG_256_UNKNOWN_FL 0x9793D0; setup MAG_256_UNKNOWN
// 0x979400 = effect table index 255).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185AD34; its descriptor block 0x185AA9C sits 0x298 below the draw
// table, 0x1C closer than in the other clones, which only moves the block: the engine reads it
// through c.desc). Instruction-level comparison against the ported clones (module-relative
// addresses normalised, table regions symbolic, jump tables and callee trees hashed):
//   - SetupSummon 0x979460, InitSequenceState 0x981C30, SequenceTick 0x979A70 (the unlit
//     BuildMatricesAndDraw) and 0x982600 are byte clones of the generic engine (Ifrit / Mad Cow
//     Special 0x964A60 / 0x96E870 / 0x965070 / 0x96F240); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (Ifrit's VM
//     0x03F / 0x041 / 0x089 / 0x093, prim 7 / 8 / 9 / 12 / 17 / 18 / 19 and draw 3 / 21 are stubs
//     here), except
//       VM 0x006 LoadBattleFile 0x97DF70: same code, own load callback 0x97E0A0 (c.load_cb),
//       draw 26 0x97A580 = Cerberus 0xB0E930 / Eden 0xAEA0C0 and VM 0x096 0x980C10 = Cerberus
//         0xB16F60 / Mad Cow Special 0x96D770 (apply_shared_misc: the only live slots it takes).
//     VM 0x022 0x97DA90 / VM 0x090 0x980B00 are Ifrit's forms: generic.
// The effect's queue (0x2796548) holds one task, the engine tick 0x979A70.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace sand_shake
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag256_sand_shake_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace sand_shake
{
	static void describe(Clone &c)
	{
		c.name = "256 Sand Shake";
		c.effect_id = 256;
		c.vm_table = 0x185AED8;     // MAG_256 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185AB48;   // MAG_256 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185AB90;   // bone_table + 0x48
		c.prim_size = 0x185AC80;    // bone_table + 0x138
		c.spr_table = 0x185AC94;    // bone_table + 0x14C
		c.ptype_table = 0x185ACC0;  // bone_table + 0x178
		c.pop_table = 0x185ACD4;    // bone_table + 0x18C
		c.draw_table = 0x185AD34;   // MAG_256 DrawHandlerTable
		c.attr_16A = 0x185AE9E;
		c.attr_184 = 0x185AEB8;
		c.attr_186 = 0x185AEBA;
		c.attr_194 = 0x185AEC8;
		c.attr_19C = 0x185AED0;
		c.desc = 0x185AA9C;         // draw_table - 0x298 (read by 0x979550)
		c.ptr_block = 0x185AB20;    // draw_table - 0x214 (read by 0x9794B0)
		c.queue = 0x2796548;
		c.file00 = 0x2796558;
		c.file01 = 0x2796540;
		c.tick = 0x979A70;
		c.load_cb = 0x97E0A0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag256_sand_shake()
	{
		using namespace gfc;
		sand_shake::describe(sand_shake::g_c);
		init_clone(sand_shake::g_c, nullptr, 0);
		apply_shared_misc(sand_shake::g_c);   // draw 26 0x97A580, VM 0x096 0x980C10 (the live slots)
		register_port(0x979A70, (void *)sand_shake::SequenceTask, "MAG256 Sand Shake SequenceTick", 256);
		// 30 fps layer: see mag256_sand_shake_held.inc
		FX_HELD(register_mag256_sand_shake_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag256_sand_shake_held.inc"
#endif
