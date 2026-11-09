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

// Effect 331: effect 331 (attack 313) (enemy attack 313 of kernel.bin (no name), c0m118), cinematic-engine module MAG_331 0x8DFF70..0x8E9C00,
// script data magic/mag330_b.00/.01 (loaded by MAG_331_UNKNOWN_FL 0x8DFF70; setup MAG_331_UNKNOWN 0x8DFFA0 =
// effect table index 330).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x18528F4; its descriptor block 0x185260C sits 0x2E8 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x8E0000, BindContext 0x8E0050, the section pointers 0x8E00F0, InitSequenceState
//     0x8E9210, SequenceTick 0x8E0610 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x8E9BE0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals BGH251F2 turret explosion 244's but for prim 9, live (generic) here, and the three
//     slots below), except
//       VM 0x006 LoadBattleFile 0x8E54A0: same code, own load callback 0x8E55D0 (c.load_cb),
//       VM 0x0AD 0x8E8DF0 PosFromPosedEntityJoint = Homing Laser 229's 0xA84780 / Ice Strike 228's
//         0xA8EA50 / Homing Laser 282 255's 0x98C370 (static there: same code below), with its 30 fps
//         prediction at the entity's next pose,
//       draw 38 0x8E0FD0 AttachBattleEntity = Water 222's 0xA9C340 / Brothers 0xAF59D0 (+ its shadow
//         helper; apply_brothers: only that slot taken, as Water does),
//       VM 0x0B5 0x8E4B30 JumpIfSlot (12 instructions, own, below; no other ported module carries it).
// The effect's queue (0x2795BE0) holds one task, the engine tick 0x8E0610.
// ENTITY / CAMERA writes: VM 0x0AD rebuilds the slot entity's bone matrices from its pose (0x508C90:
// the matrices the battle renderer builds anyway); draw 38 writes the attached battle entity
// (mag_brothers.cpp); the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_brothers(Clone &c);  // mag_brothers.cpp

namespace eff331
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag331_unknown_331_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace eff331
{
	static void describe(Clone &c)
	{
		c.name = "331 (attack 313)";
		c.effect_id = 331;
		c.vm_table = 0x1852A98;      // MAG_331 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1852708;    // MAG_331 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1852750;    // bone_table + 0x48
		c.prim_size = 0x1852840;     // bone_table + 0x138
		c.spr_table = 0x1852854;     // bone_table + 0x14C
		c.ptype_table = 0x1852880;   // bone_table + 0x178
		c.pop_table = 0x1852894;     // bone_table + 0x18C
		c.draw_table = 0x18528F4;    // MAG_331 DrawHandlerTable
		c.attr_16A = 0x1852A5E;
		c.attr_184 = 0x1852A78;
		c.attr_186 = 0x1852A7A;
		c.attr_194 = 0x1852A88;
		c.attr_19C = 0x1852A90;
		c.desc = 0x185260C;          // draw_table - 0x2E8 (read by 0x8E00F0)
		c.ptr_block = 0x18526E0;     // draw_table - 0x214 (read by 0x8E0050)
		c.queue = 0x2795BE0;
		c.file00 = 0x2795BF4;
		c.file01 = 0x2795BF0;
		c.tick = 0x8E0610;
		c.load_cb = 0x8E55D0;        // passed by VM 0x006 0x8E54A0
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x8E8DF0 (VM 0x0AD PosFromPosedEntityJoint, = Homing Laser 229's 0xA84780): the slot
	// entity's bone matrices rebuilt from its pose (header entity+0x60, or *(entity+0x78) when op
	// bit 15 is set), joint w0 of that model (blob 0xB66B80, composed with the entity matrix +0x40)
	// applied to outPos -> accumPos = the camera-space point << 16. Then the bone handler. Length 4.
	// 30 fps: while the held layer predicts the next tick, the joint comes from the entity's NEXT
	// pose (see mag331_unknown_331_held.inc).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;                  // esi
		x::BuildBoneMatricesFromPose(hdr);
		// 30 fps layer: see mag331_unknown_331_held.inc
		FX_HELD(if (held_predicting()) held_next_pose(hdr, weapon ? hdr + 0xC : ent + 0x6C);)
		uint8_t *m = xl_JointMatrix_B66B80(hdr, S16(STREAM(), 2), 0x1000, ent + 0x40);
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		uint8_t *w = WS();
		x::TransformToCamera(CUR() + 0x94, w + 0xF0, w + 0xFC);
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0x8E4B30 (VM 0x0B5 JumpIfSlot): w1 == the current bone's slot byte (+0x1B, zero-extended to
	// 16 bits): STREAM += w2 (s16); else STREAM += 6.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0B5_JumpIfSlot()
	{
		const uint16_t slot = (uint16_t)U8(CUR(), 0x1B);                     // cx
		uint8_t *s = STREAM();                                                // eax
		if (U16(s, 2) == slot) STREAM() = s + (int32_t)S16(s, 4);
		else STREAM() = s + 6;
	}

	// ------------------------------------------------------------------------------------
	// handler set: draw 38 (AttachBattleEntity) is the only Brothers handler the module carries:
	// take that one slot from apply_brothers (as Water 222), then the module's VM 0x0AD / 0x0B5
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_brothers(c);
		const VoidFn d38 = c.draw[38];             // 0x8E0FD0 = Brothers 0xAF59D0 AttachBattleEntity
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[38] = d38;
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;   // 0x8E8DF0
		c.vm[0x0B5] = op_0B5_JumpIfSlot;                // 0x8E4B30
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag331_unknown_331()
	{
		using namespace gfc;
		eff331::describe(eff331::g_c);
		init_clone(eff331::g_c, nullptr, 0);
		eff331::apply_slots(eff331::g_c);
		register_port(0x8E0610, (void *)eff331::SequenceTask, "MAG331 effect 331 (attack 313) SequenceTick", 331);
		// 30 fps layer: see mag331_unknown_331_held.inc
		FX_HELD(register_mag331_unknown_331_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag331_unknown_331_held.inc"
#endif
