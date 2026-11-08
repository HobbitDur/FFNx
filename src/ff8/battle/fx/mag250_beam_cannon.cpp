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

// Effect 250: Beam Cannon (enemy attack 118 of kernel.bin, BGH251F2 c0m102 (damaged form)), cinematic-engine module MAG_250 0x9ABFC0..0x9B7350,
// script data magic/mag249_b.00/.01 (loaded by MAG_250_UNKNOWN_FL 0x9ABFC0; setup MAG_250_UNKNOWN 0x9ABFF0 =
// effect table index 249).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185DE40; its descriptor block 0x185DB38 sits 0x308 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcB\tools\cmpall.py / best.py):
//   - SetupSummon 0x9AC050, BindContext 0x9AC0A0, the section pointers 0x9AC140, InitSequenceState
//     0x9B6960, SequenceTick 0x9AC660 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9B7330 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals Light Pillar 220's but for draw 37, live here), except
//       VM 0x006 LoadBattleFile 0x9B2CD0: same code, own load callback 0x9B2E00 (c.load_cb),
//       prim 6 0x9AF700 TintedF3 = Light Pillar 220's 0xAB57A0 / Meteor 223's 0xA93160 / Eden 0xAEB5B0
//         (mag_eden.cpp apply_eden_d: only that slot taken, as Light Pillar / Meteor do),
//       draw 37 0x9AD270 = Twin Homing Laser 253's 0x998E80 / Homing Laser 229's 0xA7BBD0 / Bahamut
//         0xB1A130 RibbonTrail (apply_shared_ribbon).
// The effect's queue (0x2796668) holds one task, the engine tick 0x9AC660.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_eden_d(Clone &c);  // mag_eden.cpp

namespace beam250
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag250_beam_cannon_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace beam250
{
	static void describe(Clone &c)
	{
		c.name = "250 Beam Cannon";
		c.effect_id = 250;
		c.vm_table = 0x185DFE4;      // MAG_250 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185DC54;    // MAG_250 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185DC9C;    // bone_table + 0x48
		c.prim_size = 0x185DD8C;     // bone_table + 0x138
		c.spr_table = 0x185DDA0;     // bone_table + 0x14C
		c.ptype_table = 0x185DDCC;   // bone_table + 0x178
		c.pop_table = 0x185DDE0;     // bone_table + 0x18C
		c.draw_table = 0x185DE40;    // MAG_250 DrawHandlerTable
		c.attr_16A = 0x185DFAA;
		c.attr_184 = 0x185DFC4;
		c.attr_186 = 0x185DFC6;
		c.attr_194 = 0x185DFD4;
		c.attr_19C = 0x185DFDC;
		c.desc = 0x185DB38;          // draw_table - 0x308 (read by 0x9AC140)
		c.ptr_block = 0x185DC2C;     // draw_table - 0x214 (read by 0x9AC0A0)
		c.queue = 0x2796668;
		c.file00 = 0x2796678;
		c.file01 = 0x2796660;
		c.tick = 0x9AC660;
		c.load_cb = 0x9B2E00;        // passed by VM 0x006 0x9B2CD0
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// prim 6 (TintedF3) is the only Eden handler the module carries: take that one slot from
	// apply_eden_d and keep every other slot as init_clone built it (as Light Pillar 220 / Meteor 223)
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];        // 0x9AF700 = Eden 0xAEB5B0 TintedF3
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[6] = p6;
		apply_shared_ribbon(c);             // draw 37 0x9AD270
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag250_beam_cannon()
	{
		using namespace gfc;
		beam250::describe(beam250::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<250>::Register("Beam Cannon", beam250::g_c, 0x9ABFC0, 0x185E504, 0x185E510,
		                           (void *)beam250::SequenceTask);
		init_clone(beam250::g_c, nullptr, 0);
		beam250::apply_slots(beam250::g_c);
		register_port(0x9AC660, (void *)beam250::SequenceTask, "MAG250 Beam Cannon SequenceTick", 250);
		// 30 fps layer: see mag250_beam_cannon_held.inc
		FX_HELD(register_mag250_beam_cannon_held();)
	}
	FX_LAB_MODULE(register_mag250_beam_cannon);
}

#ifdef FF8_FX_HELD
#include "mag250_beam_cannon_held.inc"
#endif
