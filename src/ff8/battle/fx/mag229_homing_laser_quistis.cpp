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

// Effect 229: Homing Laser, Quistis' Blue Magic (kernel.bin section 19, effect id at entry +4; MAG_229_*), cinematic-engine
// module MAG_229 0xA7A9F0..0xA85590, script data magic/mag228_b.00/.01 (loaded by MAG_229_UNKNOWN_FL 0xA7A9F0; setup
// 0xA7AA20 = effect table index 228).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard table layout (draw table
// 0x186AD5C; its descriptor block 0x186AA40 sits 0x31C below the draw table: the engine reads it through c.desc).
// Instruction-level comparison against the ported clones (module-relative addresses normalised, table regions symbolic,
// jump tables and callee trees hashed: work\fx071\w71\gfc\cmp229.txt):
//   - SetupSummon / InitSequenceState / SequenceTick 0xA7B090 (the unlit BuildMatricesAndDraw) are byte clones of the
//     generic engine; lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0xA80E30: same code, own load callback 0xA80F60 (c.load_cb),
//       draw 37 0xA7BBD0 = Bahamut 0xB1A130 / Cerberus 0xB0D690 / Eden 0xAE8E20 RibbonTrail (apply_shared_ribbon),
//       VM 0x0AB 0xA7B9E0 (new, below; = Fire Breath 230's 0xA71C40 with the com data 0x188062C): binds the module's
//         animation set to the slot entity (the caster) and queues its chain transformation (the cast animation),
//       VM 0x0AD 0xA84780 = Mad Cow Special 258's 0x96E450 / Final Form Death 0xB3DA20 PosFromPosedEntityJoint (static
//         there: same code below), with its 30 fps prediction at the entity's next pose (below).
// The effect's queue (0x2796B00) holds one task, the engine tick 0xA7B090.
// ENTITY / CAMERA writes: VM 0x0AB (the slot entity's model / animation set: BS_BindEntityModelFromComData 0x5022C0 into a
// 0x20-byte arena block; its chain transformation: QueueChainTransformation 0x505C00); VM 0x0AD rebuilds the slot entity's
// bone matrices from its pose (0x508C90: the matrices the battle renderer builds anyway); the battle camera words through
// the engine's camera ops (VM 0x039, as every clone).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace homing_laser_q
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag229_homing_laser_quistis_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace homing_laser_q
{
	static void describe(Clone &c)
	{
		c.name = "229 Homing Laser (Quistis)";
		c.effect_id = 229;
		c.vm_table = 0x186AF00;     // MAG_229 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186AB70;   // MAG_229 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186ABB8;   // bone_table + 0x48
		c.prim_size = 0x186ACA8;    // bone_table + 0x138
		c.spr_table = 0x186ACBC;    // bone_table + 0x14C
		c.ptype_table = 0x186ACE8;  // bone_table + 0x178
		c.pop_table = 0x186ACFC;    // bone_table + 0x18C
		c.draw_table = 0x186AD5C;   // MAG_229 DrawHandlerTable
		c.attr_16A = 0x186AEC6;
		c.attr_184 = 0x186AEE0;
		c.attr_186 = 0x186AEE2;
		c.attr_194 = 0x186AEF0;
		c.attr_19C = 0x186AEF8;
		c.desc = 0x186AA40;         // draw_table - 0x31C
		c.ptr_block = 0x186AB48;    // draw_table - 0x214
		c.queue = 0x2796B00;
		c.file00 = 0x2796B14;
		c.file01 = 0x2796B10;
		c.tick = 0xA7B090;
		c.load_cb = 0xA80F60;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine calls not wrapped by gfc_engine.h (battle entity writes: skipped while the held
	// layer predicts the next tick, as the engine's other entity-writing wrappers)
	// ------------------------------------------------------------------------------------
	static inline void BindEntityModelFromComData(uint8_t *entity, uint8_t *out, uint32_t data)
	{
		FX_HELD(if (held_predicting()) return;)
		x::f<void (__cdecl *)(uint8_t *, uint8_t *, uint32_t)>(0x5022C0)(entity, out, data); // BS_BindEntityModelFromComData
	}
	static inline void QueueChainTransformation(uint8_t *entity, int32_t id)
	{
		FX_HELD(if (held_predicting()) return;)
		x::f<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id);
	}
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}

	static const uint32_t COM_ANIM = 0x188062C;   // the module's animation set (com data) bound by VM 0x0AB

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0xA7B9E0 (VM 0x0AB BindEntityAnimSet, = Fire Breath 230's 0xA71C40): the slot entity gets
	// the module's animation set (com data 0x188062C) through a 0x20-byte arena block, then its
	// chain transformation w1. Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AB_BindEntityAnimSet()
	{
		uint8_t *ent = SlotEntity();                                         // esi
		uint8_t *blk = blob::ArenaAlloc(0x20);
		BindEntityModelFromComData(ent, blk, COM_ANIM);
		QueueChainTransformation(ent, (int32_t)S16(STREAM(), 2));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xA84780 (VM 0x0AD PosFromPosedEntityJoint, = Mad Cow Special 258's 0x96E450 = Final Form
	// Death 0xB3DA20): the slot entity's bone matrices rebuilt from its pose (header entity+0x60,
	// or *(entity+0x78) when op bit 15 is set), joint w0 of that model (blob 0xB66B80, composed
	// with the entity matrix +0x40) applied to outPos -> accumPos = the camera-space point << 16.
	// Then the bone handler. Length 4.
	// 30 fps: while the held layer predicts the next tick, the joint comes from the entity's NEXT
	// pose (pose_midpoint at 1: the bone matrices of the animation reader's next frame, as the
	// next real tick rebuilds them after the entity's animation step); the skeleton block (pose
	// + matrices) is journaled (guard) and put back when the prediction ends.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;                  // esi
		x::BuildBoneMatricesFromPose(hdr);
		// 30 fps layer: see mag229_homing_laser_quistis_held.inc
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
		apply_shared_ribbon(c);                            // draw 37 0xA7BBD0
		c.vm[0x0AB] = op_0AB_BindEntityAnimSet;            // 0xA7B9E0
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;      // 0xA84780
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag229_homing_laser_quistis()
	{
		using namespace gfc;
		homing_laser_q::describe(homing_laser_q::g_c);
		init_clone(homing_laser_q::g_c, nullptr, 0);
		homing_laser_q::apply_slots(homing_laser_q::g_c);
		register_port(0xA7B090, (void *)homing_laser_q::SequenceTask, "MAG229 Homing Laser (Quistis) SequenceTick", 229);
		// 30 fps layer: see mag229_homing_laser_quistis_held.inc
		FX_HELD(register_mag229_homing_laser_quistis_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag229_homing_laser_quistis_held.inc"
#endif
