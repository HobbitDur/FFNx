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

// Effect 222: Water (cinematic-engine spell, module MAG_222 0xA9B1D0..0xAA5530, script data
// magic/mag221_b.00/.01, loaded by MAG_222_WATER_FL 0xA9B1D0).
//
// Water is not a GF but runs the GF cinematic engine (gfc_engine.cpp): the module is one more
// compile of the same engine library, linked in a different order. Instruction-level comparison
// against the seven GF clones (module-relative addresses normalised, callee trees hashed):
//   - SetupSummon 0xA9B200, SequenceTick 0xA9B870, BindContext 0xA9B2B0, ReleaseContext 0xA9B340,
//     AnimChannelsNeg 0xAA4F60, AnimIntegrator 0xA9BB90, AnimChannelsPos 0xAA5380, the debug pad
//     control 0xA9BA90 and the section pointer set-up 0xA9B350 are byte clones of Ifrit's;
//   - BuildMatricesAndDraw 0xAA0030 is the unlit variant (= Brothers 0xAF9ED0), lit_dispatcher false;
//   - every table handler is the generic (Ifrit) one except
//       VM 0x006 LoadBattleFile 0xAA0DA0: same code, own load callback 0xAA0ED0 (c.load_cb),
//       VM 0x022 0xAA0840: the ApplyActionResultList variant (= Brothers 0xAFA6E0 / Eden 0xAEF450),
//       VM 0x0CF 0xAA2C20: TargetListFromChain (= Brothers 0xAFCBE0 / Eden 0xAF1830),
//       draw 38 0xA9C340: AttachBattleEntity (= Brothers 0xAF59D0 dh_38 + its shadow 0xAF5D00).
//   The last three have ports already (apply_shared_misc in gfc_engine.cpp, apply_brothers in
//   mag_brothers.cpp); this file only describes the clone and installs them.
// The effect's queue (0x2796BA8) holds one task, the engine tick 0xA9B870.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag222_water_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_brothers(Clone &c);  // mag_brothers.cpp

namespace water
{
	static Clone g_water = {};

	static void describe(Clone &c)
	{
		c.name = "222 Water";
		c.effect_id = 222;
		c.vm_table = 0x186CCFC;     // GF_222Water_VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186C96C;   // GF_222Water_BoneHandlerTable
		c.prim_table = 0x186C9B4;   // bone_table + 0x48
		c.prim_size = 0x186CAA4;    // bone_table + 0x138
		c.spr_table = 0x186CAB8;    // bone_table + 0x14C
		c.ptype_table = 0x186CAE4;  // bone_table + 0x178
		c.pop_table = 0x186CAF8;    // bone_table + 0x18C
		c.draw_table = 0x186CB58;   // GF_222Water_DrawHandlerTable
		c.attr_16A = 0x186CCC2;
		c.attr_184 = 0x186CCDC;
		c.attr_186 = 0x186CCDE;
		c.attr_194 = 0x186CCEC;
		c.attr_19C = 0x186CCF4;
		c.desc = 0x186C84C;         // draw_table - 0x30C
		c.ptr_block = 0x186C944;    // draw_table - 0x214
		c.queue = 0x2796BA8;
		c.file00 = 0x2796BDC;
		c.file01 = 0x2796BD8;
		c.tick = 0xA9B870;
		c.load_cb = 0xAA0ED0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// draw 38 (AttachBattleEntity) is the only Brothers handler Water carries: take that one slot
	// from apply_brothers and keep every other slot as init_clone built it
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_draw38(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_brothers(c);
		VoidFn d38 = c.draw[38];
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[38] = d38;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_water);
	}
}
}

	void register_mag222_water()
	{
		using namespace gfc;
		water::describe(water::g_water);
		init_clone(water::g_water, nullptr, 0);
		apply_shared_misc(water::g_water);      // VM 0x0CF (and VM 0x022, see the engine)
		water::apply_draw38(water::g_water);
		register_port(0xA9B870, (void *)water::SequenceTask, "MAG222 Water SequenceTick", 222);
		// 30 fps layer: see mag222_water_held.inc
		FX_HELD(register_mag222_water_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag222_water_held.inc"
#endif
