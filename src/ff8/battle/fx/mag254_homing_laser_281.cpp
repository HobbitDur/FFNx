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

// Effect 254: Homing Laser (attack 281) (enemy attack 281 of kernel.bin, Right Probe c0m107; no other kernel.bin entry uses effect 254),
// cinematic-engine module MAG_254 0x98D180..0x997CE0, script data magic/mag253_b.00/.01 (loaded by
// MAG_254_UNKNOWN_FL 0x98D180; setup MAG_254_UNKNOWN 0x98D1B0 = effect table index 253).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185C0F4; its descriptor block 0x185BDF0 sits 0x304 below the draw
// table: the engine reads it through c.desc), a copy of Quistis' Homing Laser 229
// (mag229_homing_laser_quistis.cpp) without its VM 0x0AB. Instruction-level comparison against the
// ported clones (module-relative addresses normalised, table regions symbolic, jump tables and
// callee trees hashed; work\gfcC\tools\cmpall.py / best.py):
//   - SetupSummon 0x98D210, BindContext 0x98D260, the section pointers 0x98D300, InitSequenceState
//     0x9972F0, SequenceTick 0x98D820 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x997CC0 are byte clones of the engine's (Sand Shake 256 / Homing Laser 229); lit_dispatcher false;
//   - every table handler is a byte clone of Homing Laser 229's slot (the generic (Ifrit) handler, a
//     `ret` stub or one of the two below), VM 0x0AB being a stub here:
//       VM 0x006 LoadBattleFile 0x993580: same code, own load callback 0x9936B0 (c.load_cb),
//       draw 37 0x98E320 = 229's 0xA7BBD0 / Bahamut 0xB1A130 RibbonTrail (apply_shared_ribbon),
//       VM 0x0AD 0x996ED0 = 229's 0xA84780 / Mad Cow Special 258's 0x96E450 PosFromPosedEntityJoint
//         (static there: same code below), with its 30 fps prediction at the entity's next pose.
// The effect's queue (0x27965B8) holds one task, the engine tick 0x98D820.
// ENTITY / CAMERA writes: VM 0x0AD rebuilds the slot entity's bone matrices from its pose (0x508C90:
// the matrices the battle renderer builds anyway); the battle camera words through the engine's
// camera ops (VM 0x039, as every clone).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace laser254
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag254_homing_laser_281_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace laser254
{
	static void describe(Clone &c)
	{
		c.name = "254 Homing Laser (attack 281)";
		c.effect_id = 254;
		c.vm_table = 0x185C298;      // MAG_254 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185BF08;    // MAG_254 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185BF50;    // bone_table + 0x48
		c.prim_size = 0x185C040;     // bone_table + 0x138
		c.spr_table = 0x185C054;     // bone_table + 0x14C
		c.ptype_table = 0x185C080;   // bone_table + 0x178
		c.pop_table = 0x185C094;     // bone_table + 0x18C
		c.draw_table = 0x185C0F4;    // MAG_254 DrawHandlerTable
		c.attr_16A = 0x185C25E;
		c.attr_184 = 0x185C278;
		c.attr_186 = 0x185C27A;
		c.attr_194 = 0x185C288;
		c.attr_19C = 0x185C290;
		c.desc = 0x185BDF0;          // draw_table - 0x304 (read by 0x98D300)
		c.ptr_block = 0x185BEE0;     // draw_table - 0x214 (read by 0x98D260)
		c.queue = 0x27965B8;
		c.file00 = 0x27965C8;
		c.file01 = 0x27965CC;
		c.tick = 0x98D820;
		c.load_cb = 0x9936B0;        // passed by VM 0x006 0x993580
		c.lit_dispatcher = false;
		c.bone_count = 18;
		c.tag_parts = true;
	}

	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x996ED0 (VM 0x0AD PosFromPosedEntityJoint, = Homing Laser 229's 0xA84780): the slot
	// entity's bone matrices rebuilt from its pose (header entity+0x60, or *(entity+0x78) when op
	// bit 15 is set), joint w0 of that model (blob 0xB66B80, composed with the entity matrix +0x40)
	// applied to outPos -> accumPos = the camera-space point << 16. Then the bone handler. Length 4.
	// 30 fps: while the held layer predicts the next tick, the joint comes from the entity's NEXT
	// pose (see mag254_homing_laser_281_held.inc).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;                  // esi
		x::BuildBoneMatricesFromPose(hdr);
		// 30 fps layer: see mag254_homing_laser_281_held.inc
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

	static void apply_slots(Clone &c)
	{
		apply_shared_ribbon(c);                            // draw 37 0x98E320
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;      // 0x996ED0
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag254_homing_laser_281()
	{
		using namespace gfc;
		laser254::describe(laser254::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<254>::Register("Homing Laser (attack 281)", laser254::g_c, 0x98D180, 0x185C7B8, 0x185C7C4,
		                           (void *)laser254::SequenceTask);
		init_clone(laser254::g_c, nullptr, 0);
		laser254::apply_slots(laser254::g_c);
		register_port(0x98D820, (void *)laser254::SequenceTask, "MAG254 Homing Laser (attack 281) SequenceTick", 254);
		// 30 fps layer: see mag254_homing_laser_281_held.inc
		FX_HELD(register_mag254_homing_laser_281_held();)
	}
	FX_LAB_MODULE(register_mag254_homing_laser_281);
}

#ifdef FF8_FX_HELD
#include "mag254_homing_laser_281_held.inc"
#endif
