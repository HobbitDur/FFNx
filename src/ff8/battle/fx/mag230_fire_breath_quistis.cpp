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

// Effect 230: Fire Breath, Quistis' Blue Magic (kernel.bin section 19, effect id at entry +4; MAG_230_*), cinematic-engine
// module MAG_230 0xA70B40..0xA7A9F0, script data magic/mag229_b.00/.01 (loaded by MAG_230_UNKNOWN_FL 0xA70B40; setup
// 0xA70B70 = effect table index 229).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard table layout (draw table
// 0x186A364; its descriptor block 0x186A02C sits 0x338 below the draw table: the engine reads it through c.desc).
// Instruction-level comparison against the ported clones (module-relative addresses normalised, table regions symbolic,
// jump tables and callee trees hashed: work\fx071\w71\gfc\cmp230.txt):
//   - SetupSummon / InitSequenceState / SequenceTick 0xA711E0 (the unlit BuildMatricesAndDraw) are byte clones of the
//     generic engine; lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0xA762F0: same code, own load callback 0xA76420 (c.load_cb),
//       prim 16 0xA735D0 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4 (apply_shared_misc's port: only that slot is taken),
//       VM 0x094 0xA77C10 (new, below): a conditional jump on a bone's ground distance to the current bone,
//       VM 0x0AB 0xA71C40 (new, below; Ifrit's VM 0x0AB is a `ret` stub): binds the module's animation set (com data
//         0x187EAB8) to the slot entity (the caster) and queues its chain transformation (the cast animation).
// The effect's queue (0x2796AC8) holds one task, the engine tick 0xA711E0.
// ENTITY / CAMERA writes: VM 0x0AB (the slot entity's model / animation set: BS_BindEntityModelFromComData 0x5022C0 into a
// 0x20-byte arena block; its chain transformation: QueueChainTransformation 0x505C00); the battle camera words through the
// engine's camera ops (VM 0x039, as every clone).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace fire_breath_q
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag230_fire_breath_quistis_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace fire_breath_q
{
	static void describe(Clone &c)
	{
		c.name = "230 Fire Breath (Quistis)";
		c.effect_id = 230;
		c.vm_table = 0x186A508;     // MAG_230 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186A178;   // MAG_230 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186A1C0;   // bone_table + 0x48
		c.prim_size = 0x186A2B0;    // bone_table + 0x138
		c.spr_table = 0x186A2C4;    // bone_table + 0x14C
		c.ptype_table = 0x186A2F0;  // bone_table + 0x178
		c.pop_table = 0x186A304;    // bone_table + 0x18C
		c.draw_table = 0x186A364;   // MAG_230 DrawHandlerTable
		c.attr_16A = 0x186A4CE;
		c.attr_184 = 0x186A4E8;
		c.attr_186 = 0x186A4EA;
		c.attr_194 = 0x186A4F8;
		c.attr_19C = 0x186A500;
		c.desc = 0x186A02C;         // draw_table - 0x338 (read by 0xA70CC0)
		c.ptr_block = 0x186A150;    // draw_table - 0x214 (read by 0xA70C20)
		c.queue = 0x2796AC8;
		c.file00 = 0x2796ADC;
		c.file01 = 0x2796AD8;
		c.tick = 0xA711E0;
		c.load_cb = 0xA76420;
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

	static const uint32_t COM_ANIM = 0x187EAB8;   // the module's animation set (com data) bound by VM 0x0AB

	// ------------------------------------------------------------------------------------
	// 0xA71C40 (VM 0x0AB BindEntityAnimSet): the slot entity (SceneHeader +0x60 + 4 * cur
	// bone +0x1B) gets the module's animation set (com data 0x187EAB8) through a 0x20-byte
	// arena block, then its chain transformation w1. Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AB_BindEntityAnimSet()
	{
		uint8_t *ent = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));   // esi
		uint8_t *blk = blob::ArenaAlloc(0x20);
		BindEntityModelFromComData(ent, blk, COM_ANIM);
		QueueChainTransformation(ent, (int32_t)S16(STREAM(), 2));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xA77C10 (VM 0x094 JumpIfBoneNear): bone w1 (GfCinematic_GetRotationVector) within the
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
	// module carries), then this module's own handlers
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn p16 = c.prim[16];           // 0xA735D0 = Cerberus 0xB105B0 / Eden 0xAEC030
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[16] = p16;
		c.vm[0x094] = op_094_JumpIfBoneNear;     // 0xA77C10
		c.vm[0x0AB] = op_0AB_BindEntityAnimSet;  // 0xA71C40
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag230_fire_breath_quistis()
	{
		using namespace gfc;
		fire_breath_q::describe(fire_breath_q::g_c);
		init_clone(fire_breath_q::g_c, nullptr, 0);
		fire_breath_q::apply_slots(fire_breath_q::g_c);
		register_port(0xA711E0, (void *)fire_breath_q::SequenceTask, "MAG230 Fire Breath (Quistis) SequenceTick", 230);
		// 30 fps layer: see mag230_fire_breath_quistis_held.inc
		FX_HELD(register_mag230_fire_breath_quistis_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag230_fire_breath_quistis_held.inc"
#endif
