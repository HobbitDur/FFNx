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

// Effect 236: Gas (enemy attack 127 of kernel.bin, Death Claw c0m035), cinematic-engine module MAG_236 0xA34210..0xA3E340,
// script data magic/mag235_b.00/.01 (loaded by MAG_236_UNKNOWN_FL 0xA34210: IO_GetFile_MAGIC of the names
// 0x1866F48 / 0x1866F54; setup MAG_236_UNKNOWN 0xA34240 = effect table index 235).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1866884; its descriptor block 0x1866564 sits 0x320 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcX\tools\cmpall.py / best.py / callees.py):
//   - SetupSummon 0xA342A0, BindContext 0xA342F0, the section pointers 0xA34390, InitSequenceState
//     0xA3D950, SequenceTick 0xA348B0 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA3E320 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (as BGH251F2's
//     turret explosion 244, plus the generic prims 7 / 11), except
//       VM 0x006 LoadBattleFile 0xA39CC0: same code, own load callback 0xA39DF0 (c.load_cb),
//       bone 4 0xA34F60 OrbitBetweenBones = Disease Breath 231's 0xA671C0,
//       bone 8 0xA37770 VertexAtHeight, prim 48 0xA36FE0 TintedFT3Abr, draw 31 0xA37370 MeshHeightCut
//         (+ its block renderer 0xA37590): Death Claw's own handlers, the same code in Gas 236 and
//         Explosion 237: gfc_deathclaw.inc.
// The effect's queue (0x2796978) holds one task, the engine tick 0xA348B0.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace gas236
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag236_gas_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace gas236
{
	static void describe(Clone &c)
	{
		c.name = "236 Gas";
		c.effect_id = 236;
		c.vm_table = 0x1866A28;      // MAG_236 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1866698;    // MAG_236 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18666E0;    // bone_table + 0x48
		c.prim_size = 0x18667D0;     // bone_table + 0x138
		c.spr_table = 0x18667E4;     // bone_table + 0x14C
		c.ptype_table = 0x1866810;   // bone_table + 0x178
		c.pop_table = 0x1866824;     // bone_table + 0x18C
		c.draw_table = 0x1866884;    // MAG_236 DrawHandlerTable
		c.attr_16A = 0x18669EE;
		c.attr_184 = 0x1866A08;
		c.attr_186 = 0x1866A0A;
		c.attr_194 = 0x1866A18;
		c.attr_19C = 0x1866A20;
		c.desc = 0x1866564;          // draw_table - 0x320 (read by 0xA34390)
		c.ptr_block = 0x1866670;     // draw_table - 0x214 (read by 0xA342F0)
		c.queue = 0x2796978;
		c.file00 = 0x279698C;
		c.file01 = 0x2796988;
		c.tick = 0xA348B0;
		c.load_cb = 0xA39DF0;        // passed by VM 0x006 0xA39CC0
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}


#include "gfc_deathclaw.inc"

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag236_gas()
	{
		using namespace gfc;
		gas236::describe(gas236::g_c);
		init_clone(gas236::g_c, nullptr, 0);
		gas236::g_c.bone[4] = gas236::bh_04_OrbitBetweenBones;     // 0xA34F60
		gas236::g_c.bone[8] = gas236::bh_08_VertexAtHeight;        // 0xA37770
		gas236::g_c.prim[48] = gas236::pr_48_TintedFT3Abr;         // 0xA36FE0
		gas236::g_c.draw[31] = gas236::dh_31_MeshHeightCut;        // 0xA37370
		register_port(0xA348B0, (void *)gas236::SequenceTask, "MAG236 Gas SequenceTick", 236);
		// 30 fps layer: see mag236_gas_held.inc
		FX_HELD(register_mag236_gas_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag236_gas_held.inc"
#endif
