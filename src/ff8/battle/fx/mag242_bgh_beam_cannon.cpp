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

// Effect 242: Beam Cannon (BGH251F2) (enemy attack 139 of kernel.bin, BGH251F2 c0m101), cinematic-engine module MAG_242 0x9F7CE0..0xA01DE0,
// script data magic/mag241_b.00/.01 (loaded by MAG_242_UNKNOWN_FL 0x9F7CE0; setup MAG_242_UNKNOWN 0x9F7D10 =
// effect table index 241).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1862CAC; its descriptor block 0x18629A4 sits 0x308 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x9F7D70, BindContext 0x9F7DC0, the section pointers 0x9F7E60, InitSequenceState
//     0xA013F0, SequenceTick 0x9F8380 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA01DC0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals Light Pillar 220's), except
//       VM 0x006 LoadBattleFile 0x9FD760: same code, own load callback 0x9FD890 (c.load_cb),
//       prim 6 0x9FA190 TintedF3 = Light Pillar 220's 0xAB57A0 / Meteor 223's 0xA93160 / Eden 0xAEB5B0
//         (mag_eden.cpp apply_eden_d: only that slot taken, as Light Pillar / Meteor do).
//     VM 0x020 0xA00040 SetFadeLayers is the generic one (0xB2E2F0, byte for byte); its IDB function is
//     split in two by a false start sub_A000A6 at its 7th instruction (the clone hash saw two pieces).
// The effect's queue (0x2796828) holds one task, the engine tick 0x9F8380.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_eden_d(Clone &c);  // mag_eden.cpp

namespace beam242
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag242_bgh_beam_cannon_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace beam242
{
	static void describe(Clone &c)
	{
		c.name = "242 Beam Cannon (BGH251F2)";
		c.effect_id = 242;
		c.vm_table = 0x1862E50;      // MAG_242 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1862AC0;    // MAG_242 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1862B08;    // bone_table + 0x48
		c.prim_size = 0x1862BF8;     // bone_table + 0x138
		c.spr_table = 0x1862C0C;     // bone_table + 0x14C
		c.ptype_table = 0x1862C38;   // bone_table + 0x178
		c.pop_table = 0x1862C4C;     // bone_table + 0x18C
		c.draw_table = 0x1862CAC;    // MAG_242 DrawHandlerTable
		c.attr_16A = 0x1862E16;
		c.attr_184 = 0x1862E30;
		c.attr_186 = 0x1862E32;
		c.attr_194 = 0x1862E40;
		c.attr_19C = 0x1862E48;
		c.desc = 0x18629A4;          // draw_table - 0x308 (read by 0x9F7E60)
		c.ptr_block = 0x1862A98;     // draw_table - 0x214 (read by 0x9F7DC0)
		c.queue = 0x2796828;
		c.file00 = 0x279683C;
		c.file01 = 0x2796838;
		c.tick = 0x9F8380;
		c.load_cb = 0x9FD890;        // passed by VM 0x006 0x9FD760
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// prim 6 (TintedF3) is the only Eden handler the module carries: take that one slot from
	// apply_eden_d and keep every other slot as init_clone built it (as Light Pillar 220 / Meteor 223)
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];               // 0x9FA190 = Eden 0xAEB5B0 TintedF3
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[6] = p6;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag242_bgh_beam_cannon()
	{
		using namespace gfc;
		beam242::describe(beam242::g_c);
		init_clone(beam242::g_c, nullptr, 0);
		beam242::apply_slots(beam242::g_c);
		register_port(0x9F8380, (void *)beam242::SequenceTask, "MAG242 Beam Cannon (BGH251F2) SequenceTick", 242);
		// 30 fps layer: see mag242_bgh_beam_cannon_held.inc
		FX_HELD(register_mag242_bgh_beam_cannon_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag242_bgh_beam_cannon_held.inc"
#endif
