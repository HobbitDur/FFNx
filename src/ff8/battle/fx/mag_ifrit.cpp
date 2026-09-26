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

// Effect 201: Ifrit - Hell Fire (cinematic-engine GF, module MAG_201 0xB25750..0xB302C0,
// script data magic/mag200_b.00/.01, streamed parts battle/mag200_b.02..12).
//
// The engine itself is gfc_engine.cpp (generic port of the seven cloned copies). Ifrit is the
// reference clone: every generic port was written from Ifrit's copy, so Ifrit has no
// exceptions. The effect's queue (0x2796E18, created by MAG_201_IFRIT_SUMMON_HELL_FIRE 0xB25780)
// holds one task, GF_Ifrit_seqBDlink 0xB25DF0 = the engine tick.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag_ifrit_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace ifrit
{
	static Clone g_ifrit = {};

	static void describe(Clone &c)
	{
		c.name = "201 Ifrit";
		c.effect_id = 201;
		c.vm_table = 0x1874F10;     // GF_201Ifrit_VmOpcodeTable
		c.bone_table = 0x1874B80;   // GF_201Ifrit_BoneHandlerTable
		c.prim_table = 0x1874BC8;   // bone_table + 0x48
		c.prim_size = 0x1874CB8;    // bone_table + 0x138
		c.spr_table = 0x1874CCC;    // bone_table + 0x14C
		c.ptype_table = 0x1874CF8;  // bone_table + 0x178
		c.pop_table = 0x1874D0C;    // bone_table + 0x18C
		c.draw_table = 0x1874D6C;   // GF_201Ifrit_DrawHandlerTable
		c.attr_16A = 0x1874ED6;
		c.attr_184 = 0x1874EF0;
		c.attr_186 = 0x1874EF2;
		c.attr_194 = 0x1874F00;
		c.attr_19C = 0x1874F08;
		c.desc = 0x1874894;
		c.ptr_block = 0x1874B58;
		c.queue = 0x2796E18;
		c.file00 = 0x2796E4C;
		c.file01 = 0x2796E48;
		c.tick = 0xB25DF0;
		c.load_cb = 0xB2BB40;
		c.lit_dispatcher = true;
		c.bone_count = 18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_ifrit);
	}
}
}

	void register_gfc_ifrit()
	{
		using namespace gfc;
		ifrit::describe(ifrit::g_ifrit);
		init_clone(ifrit::g_ifrit, nullptr, 0);
		register_port(0xB25DF0, (void *)ifrit::SequenceTask, "GFC201 Ifrit SequenceTick", 201);
		// 30 fps layer: see mag_ifrit_held.inc
		FX_HELD(register_gfc_ifrit_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag_ifrit_held.inc"
#endif
