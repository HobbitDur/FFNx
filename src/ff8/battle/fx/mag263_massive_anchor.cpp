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

// Effect 263: Massive Anchor (Ward's limit break while he is a party member, the dream sequences of
// disc 1; kernel.bin section 18 "temporary character limit breaks" entry 4, the only kernel entry
// using effect 263), cinematic-engine module MAG_263 0x9457E0..0x94F9C0, script data
// magic/mag262_b.00/.01 (loaded by MAG_263_UNKNOWN_FL 0x9457E0; setup MAG_263_UNKNOWN 0x945810 =
// effect table index 262).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp), a sibling of
// Laguna's Desperado (mag261_desperado.cpp) and Kiros' Blood Pain (mag262_blood_pain.cpp).
// Instruction-level comparison against the ported clones (module-relative addresses normalised,
// table regions symbolic, callee trees hashed):
//   - SetupSummon 0x945870, BindContext 0x9458C0, ReleaseContext 0x945950, the section pointer
//     set-up 0x945960, InitSequenceState 0x94EFD0, AnimChannels / AnimIntegrator and SequenceTick
//     0x945E80 (the unlit BuildMatricesAndDraw, = Water 0xA9B870) are byte clones of the engine's;
//     0x94F9A0 sets the initial frame limit (= Blood Pain's 0x959880); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (VM 0x03F /
//     0x041 / 0x089 / 0x093, prim 12, draw 3 are stubs here), except
//       VM 0x006 LoadBattleFile 0x94B340: same code, own load callback 0x94B470 (c.load_cb),
//     and the module's own handlers (Ifrit has `ret` stubs in these slots), ported here:
//       VM 0x0AB 0x9468E0 BindLimitAnimSet: as Desperado's 0x95ABB0 with the module's animation
//         set (com data 0x1877DAC). Length 4;
//       draw 70 0x9469E0 PlaceWeaponAtBone: the slot entity's weapon block (*(entity+0x78), Ward's
//         anchor) gets the bone's outPos as its attach offset (+0x1C / +0x1E / +0x20, from which
//         the battle builds the weapon's attach matrix +0x24 every frame): the anchor follows the
//         bone. Draws nothing.
// The effect's queue (0x2795DA8, pool 0x2795DB8) holds one task, the engine tick 0x945E80.
//
// Writes outside the engine state: battle entity (the caster): its animation set / chain (VM
// 0x0AB), its POSITION (+0x1C/+0x1E/+0x20) and flag +1 bit 0x10 through the generic VM 0x048
// OverrideEntityTransform (Ward leaps: the script moves him from tick ~54 on) and its WEAPON's
// attach offset (draw 70: weapon block +0x1C..+0x21 - the anchor follows a bone); the camera shake
// offsets 0x1D97710..0x1D97715 (generic VM 0x047); battle results (VM 0x022 generic). Camera: VM 0x039
// (engine).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag263_massive_anchor_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	static void describe(Clone &c)
	{
		c.name = "263 Massive Anchor";
		c.effect_id = 263;
		c.vm_table = 0x1857C70;     // MAG_263 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18578E0;   // MAG_263 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1857928;   // bone_table + 0x48
		c.prim_size = 0x1857A18;    // bone_table + 0x138
		c.spr_table = 0x1857A2C;    // bone_table + 0x14C
		c.ptype_table = 0x1857A58;  // bone_table + 0x178
		c.pop_table = 0x1857A6C;    // bone_table + 0x18C
		c.draw_table = 0x1857ACC;   // MAG_263 DrawHandlerTable
		c.attr_16A = 0x1857C36;
		c.attr_184 = 0x1857C50;
		c.attr_186 = 0x1857C52;
		c.attr_194 = 0x1857C60;
		c.attr_19C = 0x1857C68;
		c.desc = 0x185777C;         // draw_table - 0x350
		c.ptr_block = 0x18578B8;    // draw_table - 0x214 (read by 0x9458C0)
		c.queue = 0x2795DA8;
		c.file00 = 0x2795DA4;
		c.file01 = 0x2795DA0;
		c.tick = 0x945E80;
		c.load_cb = 0x94B470;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// module data / engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	static const uint32_t COMDATA = 0x1877DAC;   // the module's animation set (VM 0x0AB)

	template<typename F> static inline F fx(uint32_t a) { return (F)a; }
	// battle entity side effects (30 fps layer: see gfc_engine_held.h)
	static inline void xl_BindEntityModelFromComData(void *ent, void *out, uint32_t data) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, void *, uint32_t)>(0x5022C0)(ent, out, data); }
	static inline void xl_QueueChainTransformation(void *ent, int32_t id) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, int32_t)>(0x505C00)(ent, id); }

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x9468E0 (VM 0x0AB BindLimitAnimSet): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AB_BindLimitAnimSet()
	{
		uint8_t *ent = SlotEntity();                                          // esi
		uint8_t *blk = blob::ArenaAlloc(0x20);
		xl_BindEntityModelFromComData(ent, blk, COMDATA);
		xl_QueueChainTransformation(ent, S16(STREAM(), 2));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0x9469E0 (draw 70 PlaceWeaponAtBone): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_70_PlaceWeaponAtBone()
	{
		// 30 fps layer: see mag263_massive_anchor_held.inc
		FX_HELD(if (held_active()) return;)
		uint8_t *w = PTR(SlotEntity(), 0x78);                                 // eax
		S16(w, 0x1C) = S16(CUR(), 0x94);
		S16(w, 0x1E) = S16(CUR(), 0x96);
		S16(w, 0x20) = S16(CUR(), 0x98);
	}

	// ------------------------------------------------------------------------------------
	// the module's own handlers (no shared / other-clone handler is live in this module)
	// ------------------------------------------------------------------------------------
	static void apply_slots(Clone &c)
	{
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;          // 0x9468E0
		c.draw[70] = dh_70_PlaceWeaponAtBone;           // 0x9469E0
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag263_massive_anchor()
	{
		using namespace gfc;
		anchor263::describe(anchor263::g_c);
		init_clone(anchor263::g_c, nullptr, 0);
		anchor263::apply_slots(anchor263::g_c);
		register_port(0x945E80, (void *)anchor263::SequenceTask, "MAG263 Massive Anchor SequenceTick", 263);
		// 30 fps layer: see mag263_massive_anchor_held.inc
		FX_HELD(register_mag263_massive_anchor_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag263_massive_anchor_held.inc"
#endif
