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

// Effect 238: Breath (Ruby Dragon) (enemy attack 114 of kernel.bin, Ruby Dragon c0m052), cinematic-engine module MAG_238 0xA1FD90..0xA29C00,
// script data magic/mag237_b.00/.01 (loaded by MAG_238_UNKNOWN_FL 0xA1FD90; setup MAG_238_UNKNOWN 0xA1FDC0 =
// effect table index 237).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186548C; its descriptor block 0x1865154 sits 0x338 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0xA1FE20, BindContext 0xA1FE70, the section pointers 0xA1FF10, InitSequenceState
//     0xA29210, SequenceTick 0xA20430 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA29BE0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of Fire Breath (Quistis) 230's slot (the generic (Ifrit)
//     handler, a `ret` stub or one of those below; VM 0x0AB is a stub here), except
//       VM 0x006 LoadBattleFile 0xA25500: same code, own load callback 0xA25630 (c.load_cb),
//       prim 16 0xA227E0 = Fire Breath 230's 0xA735D0 / Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
//         (apply_shared_misc's port: only that slot is taken, as Fire Breath does),
//       VM 0x094 0xA26E20 JumpIfBoneNear = Fire Breath 230's 0xA77C10 (static there: same code below).
// The effect's queue (0x2796910) holds one task, the engine tick 0xA20430.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace breath238
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag238_breath_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace breath238
{
	static void describe(Clone &c)
	{
		c.name = "238 Breath (Ruby Dragon)";
		c.effect_id = 238;
		c.vm_table = 0x1865630;      // MAG_238 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18652A0;    // MAG_238 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18652E8;    // bone_table + 0x48
		c.prim_size = 0x18653D8;     // bone_table + 0x138
		c.spr_table = 0x18653EC;     // bone_table + 0x14C
		c.ptype_table = 0x1865418;   // bone_table + 0x178
		c.pop_table = 0x186542C;     // bone_table + 0x18C
		c.draw_table = 0x186548C;    // MAG_238 DrawHandlerTable
		c.attr_16A = 0x18655F6;
		c.attr_184 = 0x1865610;
		c.attr_186 = 0x1865612;
		c.attr_194 = 0x1865620;
		c.attr_19C = 0x1865628;
		c.desc = 0x1865154;          // draw_table - 0x338 (read by 0xA1FF10)
		c.ptr_block = 0x1865278;     // draw_table - 0x214 (read by 0xA1FE70)
		c.queue = 0x2796910;
		c.file00 = 0x279690C;
		c.file01 = 0x2796908;
		c.tick = 0xA20430;
		c.load_cb = 0xA25630;        // passed by VM 0x006 0xA25500
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// 0xA26E20 (VM 0x094 JumpIfBoneNear, = Fire Breath (Quistis) 230's 0xA77C10):
	// bone w1 (GfCinematic_GetRotationVector) within the
	// box |x - cur x| <= w2, |z - cur z| <= w3 (outPos +0x94 / +0x98, s16): jump by w4 (from
	// the opcode), else the next instruction (length 0xA).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_094_JumpIfBoneNear()
	{
		const uint8_t *b = blob::GetBone((int32_t)S16(STREAM(), 2));
		const int32_t bx = S16(b, 0x94);                                    // esi
		const int32_t dx = S16(STREAM(), 4), dz = S16(STREAM(), 6);          // ecx, edi
		const int32_t cx = S16(CUR(), 0x94);
		if (bx >= cx - dx && bx <= cx + dx)
		{
			const int32_t bz = S16(b, 0x98), cz = S16(CUR(), 0x98);
			if (bz >= cz - dz && bz <= cz + dz)
			{
				STREAM() += (int32_t)S16(STREAM(), 8);
				return;
			}
		}
		STREAM() += 0xA;
	}

	// ------------------------------------------------------------------------------------
	// handler set: the shared prim 16 (Cerberus / Eden TintedF4: the only shared slot this
	// module carries), then the copied VM 0x094
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn p16 = c.prim[16];             // 0xA227E0 = Cerberus 0xB105B0 / Eden 0xAEC030
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[16] = p16;
		c.vm[0x094] = op_094_JumpIfBoneNear;     // 0xA26E20
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag238_breath()
	{
		using namespace gfc;
		breath238::describe(breath238::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<238>::Register("Breath (Ruby Dragon)", breath238::g_c, 0xA1FD90, 0x1865B50, 0x1865B5C,
		                           (void *)breath238::SequenceTask);
		init_clone(breath238::g_c, nullptr, 0);
		breath238::apply_slots(breath238::g_c);
		register_port(0xA20430, (void *)breath238::SequenceTask, "MAG238 Breath (Ruby Dragon) SequenceTick", 238);
		// 30 fps layer: see mag238_breath_held.inc
		FX_HELD(register_mag238_breath_held();)
	}
	FX_LAB_MODULE(register_mag238_breath);
}

#ifdef FF8_FX_HELD
#include "mag238_breath_held.inc"
#endif
