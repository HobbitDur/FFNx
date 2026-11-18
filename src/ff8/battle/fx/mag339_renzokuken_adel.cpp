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

// Effect 339: Renzokuken vs Adel

#include "mag_common.h"

namespace ff8fx
{
namespace renzo339
{
	static const uint32_t PTS_A = 0x21EB8B0, PTS_B = 0x21EB8F0, TARGET_SLOT = 0x21EB930;
	static const uint32_t POOL_A = 0x21EB938, POOL_A_END = 0x21EBB18;
	static const uint32_t POOL_B = 0x21EBB18, POOL_B_SPAWN_END = 0x21EBD98, POOL_B_END = 0x21EBED8;
	static const uint32_t HIT_REC = 0x21EBED8, MASTER_POOL = 0x21EC628, Q_MASTER = 0x21EC638;
	static const uint32_t SLOTS = 0x21EC648, SLOTS_END = 0x21ED2C8;
	static const uint32_t POOL_C = 0x21ED2C8, POOL_C_END = 0x21ED5E8;
	static const uint32_t SPL_A = 0x21ED5E8, SPL_B = 0x21ED6E8;
	static const uint32_t HIT_POS = 0x21ED7E8, HIT_DIR = 0x21ED7F0, Q_MAIN = 0x21ED800, MAIN_POOL = 0x21ED810;
	static const uint32_t PREV_HIT = 0x21EE620, CONTEXT = 0x21EE638, CASTER_SLOT = 0x21EE63C, TEX_FILE = 0x21EE640;
	static const uint32_t RING = 0x21EE648, RING_END = 0x21EE6D4, LENGTH = 0x21EE6D4, FINISHER = 0x21EE6D8;
	static const uint32_t TEX_BASE = 0x21EE6DC, PACKET_CURSOR = 0x21EE6E0, HIT_OFFS = 0x21EE6E8;
	static const uint32_t PARITY = 0xC000;
	static const void *SOUND() { return (const void *)us(0xE55D74); }
	static const uint32_t TEXTURE_NAME = 0xCA4AF0, CAMERA = 0xE55F10, ANIM_SETS = 0xE56798, BIND_OUT = 0x24FEA08;
	static const uint32_t MODEL_SPARKLE = 0xE55D78, SPARKLE_FADE = 0xE55F00, SPARKLE_DEPTH = 0xE55EF0;
	static const uint32_t MODEL_SLASH = 0xC9E424, MODEL_STREAK = 0xC9EE0C;
	static const uint32_t SEQ_FLASH = 0xC9DE9C, SEQ_DEBRIS = 0xC9E16C, SEQ_SPARK = 0xC9E048, SPARK_FRAMES = 0xCA4A40;
	static const uint32_t SLASH_REF = 0xCA4AA0, HIT_LEAD = 0xCA4AB0;
	static const uint32_t SEQ_EMIT = 0xC9DCF0, RING_REF = 0xCA4AE0;
	static const uint32_t ORIG_FileLoader = 0x57E5B0, ORIG_Setup = 0x57E5D0;
	static const uint32_t ORIG_Controller = 0x57E6F0, ORIG_HitSpark = 0x57EC70, ORIG_Sparkle = 0x57EF00,
		ORIG_Trail = 0x57F230, ORIG_SparkSpawn = 0x57F7B0, ORIG_Hit = 0x57F8E0, ORIG_Slash = 0x57FCB0,
		ORIG_Emitter = 0x57FF90, ORIG_RingA = 0x580310, ORIG_RingB = 0x5804A0, ORIG_Master = 0x580700;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag339_renzokuken_adel_held.h"
#endif

namespace ff8fx
{
namespace renzo339
{
#define RENZO_ID 339
#define RENZO_IDS "339"
#define RENZO_CUSTOM_START
	static void Events(Node24 *n);
	static void Start(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_CUSTOM_START
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[2] = {
		{ ORIG_RingA, 0xC9FA74, RING_REF, 3, 6, 409, 0x10, false },
		{ ORIG_RingB, 0xCA1C2C, RING_REF, 2, 6, 409, 0x10, true },
	};

	static void StartBoss(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		(void)caster; (void)out; (void)sets; (void)seq;
		QueueChainTransformation(boss, 1);
	}

	static void Bind3(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		next = BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		BindEntityModelFromComData(boss + 0x9C, next, sets + *(const uint32_t *)(sets + 0xC));
		QueueChainTransformation(caster, seq);
	}

	static void Start(Node24 *n)
	{
		if (n->c == 1) StartBoss(Caster(), Target(), us(BIND_OUT), us(ANIM_SETS), (Finisher() != 0) + 1);
		if (n->c == 2) Bind3(Caster(), Target(), us(BIND_OUT), us(ANIM_SETS), (Finisher() != 0) + 1);
		if (n->c == 3) ComputeLength();
		if (n->c == 2) StartTasks();
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x16) AddRecordEmitter(us(ORIG_Emitter), 2, 0, -0xA00, 0x500);
		if (n->c == 0x1D) AddRecordEmitter(us(ORIG_Emitter), 4, 0, -0x800, 0x600);
		if (n->c == 0x48)
		{
			AddRecordEmitter(us(ORIG_Emitter), 8, 0, -0xB00, 0x400);
			Node24 *t = AddSlotTask(us(ORIG_RingA));
			const int16_t s = (int16_t)((CrtRand() % 0x400 + 0xA00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
			t = AddSlotTask(us(ORIG_RingB));
			t->w1C = 0x1600;
			const int16_t sy = (int16_t)((CrtRand() % 0x400 + 0xA00) / 3);
			t->w22 = sy;
			t->w20 = sy;
			HitPointRecord(t);
		}
	}

	static uint32_t __cdecl EmitterTask(TaskNode *tn) { return RecordEmitter(tn, 0x10, us(SEQ_EMIT)); }
	static uint32_t __cdecl RingATask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl RingBTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[1], 1, (Node24 *)tn); }
}

	void register_mag339_renzokuken_adel()
	{
		renzo339::RegisterCommon();
		register_port(renzo339::ORIG_Emitter, (void *)renzo339::EmitterTask, "339 Emitter", 339);
		register_port(renzo339::ORIG_RingA, (void *)renzo339::RingATask, "339 RingA", 339);
		register_port(renzo339::ORIG_RingB, (void *)renzo339::RingBTask, "339 RingB", 339);
		FX_HELD(register_mag339_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag339_renzokuken_adel_held.inc"
#endif
