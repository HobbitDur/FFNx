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
// Effect 336: Renzokuken vs Ultima Weapon (MAG_336_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from
// the TARGET's info +0x150 table (computeCommandAction 0x48DE0B): Ultima Weapon c0m090 has 336. The common part of the boss
// Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc; its hit offsets 0xCB2700 are a real table here) and its own
// events:
//   tick 0x3C: a caster model (CasterModelA 0x5848F0, model 0xCAE7BC: the caster's matrix at the hit point, uniform scale
//     (rand % 0x800 + 0x1600) / 3, rate - rate / 5, 14 ticks, fading from tick 6 (step 512));
//   tick 0x5B: a two-bone emitter (BoneEmitter 0x584A30: the mean direction of the boss's bones 0x11 and 0x25, until tick
//     0x18, scale 0x800..0xDFF, speed 0x28..0x95, sequence 0xCAB910);
//   tick 0x69: a second caster model (CasterModelB 0x584DE0, model 0xCB047C, scale (rand % 0x800 + 0x2000) / 3, rate -
//     rate / 4, 18 ticks, fading from tick 6 (step 341));
//   tick 0x78: the dust (Dust 0x584F20: 14 particles per tick on ticks 0..2 around the boss's bone 0xF1, scale
//     0x1800..0x25FF, radius 0x1F4..0x2BB, speed 0xDC..0x171), a ground model (GroundModel 0x585220, model 0xCACCAC, at
//     that point's x / z, uniform scale (rand % 0xA00 + 0x1E00) / 3, rate - rate / 5, 16 ticks, fading from tick 6 (step
//     409)) and the shake (Shake 0x585360: 8 ticks, amplitude 100, sin((tick << 10) / 8 + 0x400)).
// File loader 0x583020 (IO_GetFile_MAGIC of the name at 0xCB2760, kept at 0x21F4370); setup MAG_336 0x583040 (not a task:
// context 0x21F4368, caster 0x21F436C, first target 0x21F1660, master 0x585420 in its pool 0x21F2358 (queue 0x21F2368),
// controller 0x583160 in the main pool 0x21F3540 (100 x 0x24, queue 0x21F3530), length 0x21F4404 = 4, finisher flag
// 0x21F4408, pools A / C / B and SLOTS freed, camera animation 0xE63CD4, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task); no position write. CAMERA: the setup's camera animation and the shake word 0x1D97712 (Shake).
// Module globals 0x21F15E0..0x21F4414: gathered points 0x21F15E0 / 0x21F1620, target 0x21F1660, pool A 0x21F1668, pool B
// 0x21F1848, hit records 0x21F1C08, master pool / queue 0x21F2358 / 0x21F2368, SLOTS 0x21F2378 (100 x 0x20), pool C
// 0x21F2FF8, splines 0x21F3318 / 0x21F3418, hit point / direction 0x21F3518 / 0x21F3520, main queue / pool 0x21F3530 /
// 0x21F3540, previous hit point / direction 0x21F4350, context 0x21F4368, caster 0x21F436C, file 0x21F4370, trail ring
// 0x21F4378, length 0x21F4404, finisher 0x21F4408, texture base 0x21F440C, packet cursor 0x21F4410.

#include "mag_common.h"

namespace ff8fx
{
namespace renzo336
{
	// module globals
	static const uint32_t PTS_A = 0x21F15E0, PTS_B = 0x21F1620, TARGET_SLOT = 0x21F1660;
	static const uint32_t POOL_A = 0x21F1668, POOL_A_END = 0x21F1848;
	static const uint32_t POOL_B = 0x21F1848, POOL_B_SPAWN_END = 0x21F1AC8, POOL_B_END = 0x21F1C08;
	static const uint32_t HIT_REC = 0x21F1C08, MASTER_POOL = 0x21F2358, Q_MASTER = 0x21F2368;
	static const uint32_t SLOTS = 0x21F2378, SLOTS_END = 0x21F2FF8;
	static const uint32_t POOL_C = 0x21F2FF8, POOL_C_END = 0x21F3318;
	static const uint32_t SPL_A = 0x21F3318, SPL_B = 0x21F3418;
	static const uint32_t HIT_POS = 0x21F3518, HIT_DIR = 0x21F3520, Q_MAIN = 0x21F3530, MAIN_POOL = 0x21F3540;
	static const uint32_t PREV_HIT = 0x21F4350, CONTEXT = 0x21F4368, CASTER_SLOT = 0x21F436C, TEX_FILE = 0x21F4370;
	static const uint32_t RING = 0x21F4378, RING_END = 0x21F4404, LENGTH = 0x21F4404, FINISHER = 0x21F4408;
	static const uint32_t TEX_BASE = 0x21F440C, PACKET_CURSOR = 0x21F4410;
	static const uint32_t PARITY = 0xC000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE63B38;
	static const uint32_t TEXTURE_NAME = 0xCB2760, CAMERA = 0xE63CD4, ANIM_SETS = 0xE640EC, BIND_OUT = 0x24FEA78;
	static const uint32_t MODEL_SPARKLE = 0xE63B3C, SPARKLE_FADE = 0xE63CC4, SPARKLE_DEPTH = 0xE63CB4;
	static const uint32_t MODEL_SLASH = 0xCAC044, MODEL_STREAK = 0xCACA2C;
	static const uint32_t SEQ_FLASH = 0xCABABC, SEQ_DEBRIS = 0xCABD8C, SEQ_SPARK = 0xCABC68, SPARK_FRAMES = 0xCB2660;
	static const uint32_t SLASH_REF = 0xCB26C0, HIT_LEAD = 0xCB26D0, HIT_OFFS = 0xCB2700;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x583020, ORIG_Setup = 0x583040;
	static const uint32_t ORIG_Controller = 0x583160, ORIG_HitSpark = 0x5835D0, ORIG_Sparkle = 0x583860,
		ORIG_Trail = 0x583B90, ORIG_SparkSpawn = 0x584110, ORIG_Hit = 0x584240, ORIG_Slash = 0x584610,
		ORIG_CasterModelA = 0x5848F0, ORIG_BoneEmitter = 0x584A30, ORIG_CasterModelB = 0x584DE0, ORIG_Dust = 0x584F20,
		ORIG_GroundModel = 0x585220, ORIG_Shake = 0x585360, ORIG_Master = 0x585420;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag336_renzokuken_ultima_weapon_held.h"
#endif

namespace ff8fx
{
namespace renzo336
{
#define RENZO_ID 336
#define RENZO_IDS "336"
	static void Events(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_IDS
#undef RENZO_ID

	static const NodeModelDef NODEMODELS[3] = {
		{ ORIG_CasterModelA, 0xCAE7BC, NM_CASTER, 6, 512, 5, 0xE, false },
		{ ORIG_CasterModelB, 0xCB047C, NM_CASTER, 6, 341, 4, 0x12, false },
		{ ORIG_GroundModel, 0xCACCAC, NM_GROUND, 6, 409, 5, 0x10, false },
	};
	static const BoneEmitDef BONE_EMIT = { 0x18, 0x11, 0x25, 0x600, 0x800, 0x6E, 0x28, 0xCAB910 };
	static const DustDef DUST = { ORIG_Dust, false, 2, 0xE, 0xE00, 0x1800, 0xC8, 0x1F4, 0x96, 0xDC };
	static const ShakeDef SHAKE_DEF = { false, 8, 100, 8 };

	// a caster model at the hit point, scale (rand % 0x800 + s0) / 3
	static void AddCasterModel(uint32_t orig, int32_t s0)
	{
		Node24 *t = (Node24 *)AddTaskToQueue(QMain(), orig);
		memcpy(&t->w10, (const void *)HIT_POS, 8);
		t->c = 0;
		const int16_t s = (int16_t)((CrtRand() % 0x800 + s0) / 3);
		t->w1E = s;
		t->w1C = s;
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x3C) AddCasterModel(ORIG_CasterModelA, 0x1600);
		if (n->c == 0x5B) ((Node24 *)AddTaskToQueue(QMain(), ORIG_BoneEmitter))->c = 0;
		if (n->c == 0x69) AddCasterModel(ORIG_CasterModelB, 0x2000);
		if (n->c == 0x78)
		{
			Node24 *d = (Node24 *)AddTaskToQueue(QMain(), ORIG_Dust);
			d->c = 0;
			GetEffectSpawnPosition(Target(), 0xF1, 0, &d->w10);
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_GroundModel);
			memcpy(&t->w10, &d->w10, 8);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0xA00 + 0x1E00) / 3);
			t->w1E = s;
			t->w1C = s;
			((Node24 *)AddTaskToQueue(QMain(), ORIG_Shake))->c = 0;
		}
	}

	static uint32_t __cdecl CasterModelATask(TaskNode *tn) { return NodeModelTask(NODEMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl BoneEmitterTask(TaskNode *tn) { return BoneEmitter(BONE_EMIT, (Node24 *)tn); }
	static uint32_t __cdecl CasterModelBTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[1], 1, (Node24 *)tn); }
	static uint32_t __cdecl DustTask(TaskNode *tn) { return DustRun(DUST, (Node24 *)tn); }
	static uint32_t __cdecl GroundModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[2], 2, (Node24 *)tn); }
	static uint32_t __cdecl ShakeTask(TaskNode *tn) { return ShakeRun(SHAKE_DEF, (Node24 *)tn); }
}

	void register_mag336_renzokuken_ultima_weapon()
	{
		renzo336::RegisterCommon();
		register_port(renzo336::ORIG_CasterModelA, (void *)renzo336::CasterModelATask, "336 CasterModelA", 336);
		register_port(renzo336::ORIG_BoneEmitter, (void *)renzo336::BoneEmitterTask, "336 BoneEmitter", 336);
		register_port(renzo336::ORIG_CasterModelB, (void *)renzo336::CasterModelBTask, "336 CasterModelB", 336);
		register_port(renzo336::ORIG_Dust, (void *)renzo336::DustTask, "336 Dust", 336);
		register_port(renzo336::ORIG_GroundModel, (void *)renzo336::GroundModelTask, "336 GroundModel", 336);
		register_port(renzo336::ORIG_Shake, (void *)renzo336::ShakeTask, "336 Shake", 336);
		// 30 fps layer: see mag336_renzokuken_ultima_weapon_held.inc
		FX_HELD(register_mag336_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag336_renzokuken_ultima_weapon_held.inc"
#endif
