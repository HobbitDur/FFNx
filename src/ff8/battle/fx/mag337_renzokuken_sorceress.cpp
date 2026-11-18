/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#include "mag_common.h"

namespace ff8fx
{
namespace renzo337
{
	static const uint32_t PTS_A = 0x21EE748, PTS_B = 0x21EE788, TARGET_SLOT = 0x21EE7C8;
	static const uint32_t POOL_A = 0x21EE7D0, POOL_A_END = 0x21EE9B0;
	static const uint32_t POOL_B = 0x21EE9B0, POOL_B_SPAWN_END = 0x21EEC30, POOL_B_END = 0x21EED70;
	static const uint32_t HIT_REC = 0x21EED70, MASTER_POOL = 0x21EF4C0, Q_MASTER = 0x21EF4D0;
	static const uint32_t SLOTS = 0x21EF4E0, SLOTS_END = 0x21F0160;
	static const uint32_t POOL_C = 0x21F0160, POOL_C_END = 0x21F0480;
	static const uint32_t SPL_A = 0x21F0480, SPL_B = 0x21F0580;
	static const uint32_t HIT_POS = 0x21F0680, HIT_DIR = 0x21F0688, Q_MAIN = 0x21F0698, MAIN_POOL = 0x21F06A8;
	static const uint32_t PREV_HIT = 0x21F14B8, CONTEXT = 0x21F14D0, CASTER_SLOT = 0x21F14D4, TEX_FILE = 0x21F14D8;
	static const uint32_t RING = 0x21F14E0, RING_END = 0x21F156C, LENGTH = 0x21F156C, FINISHER = 0x21F1570;
	static const uint32_t TEX_BASE = 0x21F1574, PACKET_CURSOR = 0x21F1578, HIT_OFFS = 0x21F1580;
	static const uint32_t PARITY = 0xC000;
	static const void *SOUND() { return (const void *)us(0xE5D80C); }
	static const uint32_t TEXTURE_NAME = 0xCAB900, CAMERA = 0xE5D9A8, ANIM_SETS = 0xE5DF6C, BIND_OUT = 0x24FEA48;
	static const uint32_t MODEL_SPARKLE = 0xE5D810, SPARKLE_FADE = 0xE5D998, SPARKLE_DEPTH = 0xE5D988;
	static const uint32_t MODEL_SLASH = 0xCA5234, MODEL_STREAK = 0xCA5C1C;
	static const uint32_t SEQ_FLASH = 0xCA4CAC, SEQ_DEBRIS = 0xCA4F7C, SEQ_SPARK = 0xCA4E58, SPARK_FRAMES = 0xCAB850;
	static const uint32_t SLASH_REF = 0xCAB8B0, HIT_LEAD = 0xCAB8C0;
	static const uint32_t RING_REF = 0xCAB8F0;
	static const uint32_t ORIG_FileLoader = 0x580970, ORIG_Setup = 0x580990;
	static const uint32_t ORIG_Controller = 0x580AB0, ORIG_HitSpark = 0x580F50, ORIG_Sparkle = 0x5811E0,
		ORIG_Trail = 0x581510, ORIG_SparkSpawn = 0x581A90, ORIG_Hit = 0x581BC0, ORIG_Slash = 0x581F90,
		ORIG_CasterModel = 0x582270, ORIG_BoneEmitter = 0x5823B0, ORIG_Ring = 0x5826C0, ORIG_Dust = 0x582850,
		ORIG_GroundModel = 0x582B50, ORIG_Shake = 0x582C90, ORIG_Master = 0x582DB0;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag337_renzokuken_sorceress_held.h"
#endif

namespace ff8fx
{
namespace renzo337
{
#define RENZO_ID 337
#define RENZO_IDS "337"
	static void Events(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[1] = {
		{ ORIG_Ring, 0xCA6884, RING_REF, 3, 6, 341, 0x12, false },
	};
	static const NodeModelDef NODEMODELS[2] = {
		{ ORIG_CasterModel, 0xCA79AC, NM_CASTER, 6, 512, 5, 0xE, false },
		{ ORIG_GroundModel, 0xCA5E9C, NM_GROUND, 6, 409, 5, 0x10, false },
	};
	static const BoneEmitDef BONE_EMIT = { 0x16, 0xF1, 0, 0x880, 0x700, 0x64, 0x19, 0xCA4B00 };
	static const DustDef DUST = { ORIG_Dust, false, 2, 0xE, 0x800, 0x1200, 0xFA, 0x190, 0xA0, 0xBE };
	static const ShakeDef SHAKE_DEF = { false, 8, 100, 8 };

	static void Events(Node24 *n)
	{
		if (n->c == 0x46)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_CasterModel));
			ff8fx::copy<(8)>(&t->w10, (const void *)us(HIT_POS));
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0x600 + 0x1600) / 3);
			t->w1E = s;
			t->w1C = s;
		}
		if (n->c == 0x47) ((Node24 *)AddTaskToQueue(QMain(), us(ORIG_BoneEmitter)))->c = 0;
		if (n->c == 0x54)
		{
			Node24 *t = AddSlotTask(us(ORIG_Ring));
			const int16_t s = (int16_t)((CrtRand() % 0x400 + 0xC00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x59)
		{
			Node24 *d = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_Dust));
			d->c = 0;
			GetEffectSpawnPosition(Target(), 0xF1, 0, &d->w10);
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_GroundModel));
			ff8fx::copy<(8)>(&t->w10, &d->w10);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x1800) / 3);
			t->w1E = s;
			t->w1C = s;
			((Node24 *)AddTaskToQueue(QMain(), us(ORIG_Shake)))->c = 0;
		}
	}

	static uint32_t __cdecl CasterModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl BoneEmitterTask(TaskNode *tn) { return BoneEmitter(BONE_EMIT, (Node24 *)tn); }
	static uint32_t __cdecl RingTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl DustTask(TaskNode *tn) { return DustRun(DUST, (Node24 *)tn); }
	static uint32_t __cdecl GroundModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[1], 1, (Node24 *)tn); }
	static uint32_t __cdecl ShakeTask(TaskNode *tn) { return ShakeRun(SHAKE_DEF, (Node24 *)tn); }
}

	void register_mag337_renzokuken_sorceress()
	{
		renzo337::RegisterCommon();
		register_port(renzo337::ORIG_CasterModel, (void *)renzo337::CasterModelTask, "337 CasterModel", 337);
		register_port(renzo337::ORIG_BoneEmitter, (void *)renzo337::BoneEmitterTask, "337 BoneEmitter", 337);
		register_port(renzo337::ORIG_Ring, (void *)renzo337::RingTask, "337 Ring", 337);
		register_port(renzo337::ORIG_Dust, (void *)renzo337::DustTask, "337 Dust", 337);
		register_port(renzo337::ORIG_GroundModel, (void *)renzo337::GroundModelTask, "337 GroundModel", 337);
		register_port(renzo337::ORIG_Shake, (void *)renzo337::ShakeTask, "337 Shake", 337);
		FX_HELD(register_mag337_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag337_renzokuken_sorceress_held.inc"
#endif
