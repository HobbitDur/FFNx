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

// Effect 220: Light Pillar (enemy attack 308 of kernel.bin, Omega Weapon c0m114 and Ultima Weapon
// c0m090), cinematic-engine module MAG_220 0xAB32F0..0xABD3F0, script data magic/mag219_b.00/.01
// (loaded by MAG_220_LIGHT_PILLAR_FL 0xAB32F0; setup MAG_220_LIGHT_PILLAR 0xAB3320 = effect table
// index 219).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186DFF4; its descriptor block 0x186DCEC sits 0x308 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcC\tools\cmpall.py / best.py):
//   - SetupSummon 0xAB3380, BindContext 0xAB33D0, the section pointers 0xAB3470, InitSequenceState
//     0xABCA00, SequenceTick 0xAB3990 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xABD3D0 are byte clones of Sand Shake 256's / Massive Anchor 263's; lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals Massive Anchor 263's but for 263's own VM 0x0AB / draw 70 and prim 8, stubs here), except
//       VM 0x006 LoadBattleFile 0xAB8D70: same code, own load callback 0xAB8EA0 (c.load_cb),
//       prim 6 0xAB57A0 TintedF3 = Meteor 223's 0xA93160 / Eden 0xAEB5B0 (mag_eden.cpp apply_eden_d:
//         only that slot taken, as Meteor does).
// The effect's queue (0x2796C18, pool 0x2796C30) holds one task, the engine tick 0xAB3990.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_eden_d(Clone &c);  // mag_eden.cpp

namespace pillar220
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag220_light_pillar_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace pillar220
{
	static void describe(Clone &c)
	{
		c.name = "220 Light Pillar";
		c.effect_id = 220;
		c.vm_table = 0x186E198;      // MAG_220 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186DE08;    // MAG_220 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186DE50;    // bone_table + 0x48
		c.prim_size = 0x186DF40;     // bone_table + 0x138
		c.spr_table = 0x186DF54;     // bone_table + 0x14C
		c.ptype_table = 0x186DF80;   // bone_table + 0x178
		c.pop_table = 0x186DF94;     // bone_table + 0x18C
		c.draw_table = 0x186DFF4;    // MAG_220 DrawHandlerTable
		c.attr_16A = 0x186E15E;
		c.attr_184 = 0x186E178;
		c.attr_186 = 0x186E17A;
		c.attr_194 = 0x186E188;
		c.attr_19C = 0x186E190;
		c.desc = 0x186DCEC;          // draw_table - 0x308 (read by 0xAB3470)
		c.ptr_block = 0x186DDE0;     // draw_table - 0x214 (read by 0xAB33D0)
		c.queue = 0x2796C18;
		c.file00 = 0x2796C2C;
		c.file01 = 0x2796C28;
		c.tick = 0xAB3990;
		c.load_cb = 0xAB8EA0;        // passed by VM 0x006 0xAB8D70
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// prim 6 (TintedF3) is the only Eden handler the module carries: take that one slot from
	// apply_eden_d and keep every other slot as init_clone built it (as Meteor 223)
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];        // 0xAB57A0 = Eden 0xAEB5B0 TintedF3
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

	void register_mag220_light_pillar()
	{
		using namespace gfc;
		pillar220::describe(pillar220::g_c);
		init_clone(pillar220::g_c, nullptr, 0);
		pillar220::apply_slots(pillar220::g_c);
		register_port(0xAB3990, (void *)pillar220::SequenceTask, "MAG220 Light Pillar SequenceTick", 220);
		// 30 fps layer: see mag220_light_pillar_held.inc
		FX_HELD(register_mag220_light_pillar_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag220_light_pillar_held.inc"
#endif
