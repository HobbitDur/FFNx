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

// Effect 248: effect 248 (no enemy attack of kernel.bin uses effect 248), cinematic-engine module MAG_248 0x9C08A0..0x9C9860,
// script data magic/mag247_b.00/.01 (loaded by MAG_248_UNKNOWN_FL 0x9C08A0; setup MAG_248_UNKNOWN 0x9C08D0 =
// effect table index 247).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185F1C0; its descriptor block 0x185EEFC sits 0x2C4 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcB\tools\cmpall.py / best.py):
//   - SetupSummon 0x9C0930, BindContext 0x9C0980, the section pointers 0x9C0A20, InitSequenceState
//     0x9C8E70, SequenceTick 0x9C0F40 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9C9840 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's but for VM 0x042 / 0x09A, live here, and prim 8, a stub
//     here), except
//       VM 0x006 LoadBattleFile 0x9C5110: same code, own load callback 0x9C5240 (c.load_cb),
//       VM 0x042 0x9C8CC0 = Bahamut 0xB24BB0 / Breath of Death 232's 0xA658D0 / Elvoret's death 18's
//         0xB57280 EntityChainAnim (apply_shared_misc: only that slot taken, as Elvoret's death does),
//       VM 0x09A 0x9C5700 SetTargetList (own, below; no other module carries it),
//     and VM 0x066 0x9C7F80 NodeCamTranslate, the generic one (its IDB function is split in two by a
//     false start sub_9C8000 at its last three instructions).
// The effect's queue (0x27966D8) holds one task, the engine tick 0x9C0F40.
// Battle entity writes: VM 0x042 queues a chain transformation of the slot entity (0x505C00, as
// Bahamut / Elvoret's death); the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace eff248
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag248_unknown_248_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace eff248
{
	static void describe(Clone &c)
	{
		c.name = "248 (no kernel.bin attack)";
		c.effect_id = 248;
		c.vm_table = 0x185F364;      // MAG_248 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185EFD4;    // MAG_248 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185F01C;    // bone_table + 0x48
		c.prim_size = 0x185F10C;     // bone_table + 0x138
		c.spr_table = 0x185F120;     // bone_table + 0x14C
		c.ptype_table = 0x185F14C;   // bone_table + 0x178
		c.pop_table = 0x185F160;     // bone_table + 0x18C
		c.draw_table = 0x185F1C0;    // MAG_248 DrawHandlerTable
		c.attr_16A = 0x185F32A;
		c.attr_184 = 0x185F344;
		c.attr_186 = 0x185F346;
		c.attr_194 = 0x185F354;
		c.attr_19C = 0x185F35C;
		c.desc = 0x185EEFC;          // draw_table - 0x2C4 (read by 0x9C0A20)
		c.ptr_block = 0x185EFAC;     // draw_table - 0x214 (read by 0x9C0980)
		c.queue = 0x27966D8;
		c.file00 = 0x27966E8;
		c.file01 = 0x27966EC;
		c.tick = 0x9C0F40;
		c.load_cb = 0x9C5240;        // passed by VM 0x006 0x9C5110
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// 0x9C5700 (VM 0x09A SetTargetList): n = op >> 12 target slots: scene+0x41 = n, STREAM += 2,
	// then for each: scene+0x48+i = the low byte of the next stream word, STREAM += 2 (the scene
	// block's target list, as setup fills it from the action; read by VM 0x05D SpawnPerTarget).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_09A_SetTargetList()
	{
		const uint32_t n = (uint32_t)U16(RT(), 0x4A) >> 12;
		U8(SCENE(), 0x41) = (uint8_t)n;
		STREAM() += 2;
		for (uint32_t i = 0; i < n; i++)
		{
			U8(SCENE(), 0x48 + i) = U8(STREAM(), 0);
			STREAM() += 2;
		}
	}

	// VM 0x042 is the only shared handler the module carries: take that one slot from
	// apply_shared_misc and keep every other slot as init_clone built it (as Elvoret's death 18)
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn v042 = c.vm[0x042];    // 0x9C8CC0 = Bahamut 0xB24BB0 EntityChainAnim
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.vm[0x042] = v042;
		c.vm[0x09A] = op_09A_SetTargetList; // 0x9C5700
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag248_unknown_248()
	{
		using namespace gfc;
		eff248::describe(eff248::g_c);
		init_clone(eff248::g_c, nullptr, 0);
		eff248::apply_slots(eff248::g_c);
		register_port(0x9C0F40, (void *)eff248::SequenceTask, "MAG248 effect 248 SequenceTick", 248);
		// 30 fps layer: see mag248_unknown_248_held.inc
		FX_HELD(register_mag248_unknown_248_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag248_unknown_248_held.inc"
#endif
