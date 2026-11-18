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
namespace renzo341
{
	static const uint32_t PTS_A = 0x21E5BE0, PTS_B = 0x21E5C20, TARGET_SLOT = 0x21E5C60;
	static const uint32_t POOL_A = 0x21E5C68, POOL_A_END = 0x21E5E48;
	static const uint32_t POOL_B = 0x21E5E48, POOL_B_SPAWN_END = 0x21E60C8, POOL_B_END = 0x21E6208;
	static const uint32_t HIT_REC = 0x21E6208, MASTER_POOL = 0x21E6958, Q_MASTER = 0x21E6968;
	static const uint32_t SLOTS = 0x21E6978, SLOTS_END = 0x21E75F8;
	static const uint32_t POOL_C = 0x21E75F8, POOL_C_END = 0x21E7918;
	static const uint32_t SPL_A = 0x21E7918, SPL_B = 0x21E7A18;
	static const uint32_t HIT_POS = 0x21E7B18, HIT_DIR = 0x21E7B20, Q_MAIN = 0x21E7B30, MAIN_POOL = 0x21E7B40;
	static const uint32_t PREV_HIT = 0x21E8950, CONTEXT = 0x21E8968, CASTER_SLOT = 0x21E896C, TEX_FILE = 0x21E8970;
	static const uint32_t RING = 0x21E8978, RING_END = 0x21E8A04, LENGTH = 0x21E8A04, FINISHER = 0x21E8A08;
	static const uint32_t TEX_BASE = 0x21E8A0C, PACKET_CURSOR = 0x21E8A10, HIT_OFFS = 0x21E8A18;
	static const uint32_t PARITY = 0xC000;
	static const void *const SOUND = (const void *)0xE4EA18;
	static const uint32_t TEXTURE_NAME = 0xC96E70, CAMERA = 0xE4EBB4, ANIM_SETS = 0xE4F038, BIND_OUT = 0x24FE9A8;
	static const uint32_t MODEL_SPARKLE = 0xE4EA1C, SPARKLE_FADE = 0xE4EBA4, SPARKLE_DEPTH = 0xE4EB94;
	static const uint32_t MODEL_SLASH = 0xC907A4, MODEL_STREAK = 0xC9118C;
	static const uint32_t SEQ_FLASH = 0xC9021C, SEQ_DEBRIS = 0xC904EC, SEQ_SPARK = 0xC903C8, SPARK_FRAMES = 0xC96DC0;
	static const uint32_t SLASH_REF = 0xC96E20, HIT_LEAD = 0xC96E30;
	static const uint32_t RING_REF = 0xC96E60;
	static const uint32_t ORIG_FileLoader = 0x579A60, ORIG_Setup = 0x579A80;
	static const uint32_t ORIG_Controller = 0x579BA0, ORIG_HitSpark = 0x57A010, ORIG_Sparkle = 0x57A2A0,
		ORIG_Trail = 0x57A5D0, ORIG_SparkSpawn = 0x57AB50, ORIG_Hit = 0x57AC80, ORIG_Slash = 0x57B050,
		ORIG_Ring = 0x57B330, ORIG_Dust = 0x57B520, ORIG_Shake = 0x57B880, ORIG_Master = 0x57B970;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag341_renzokuken_jumbo_cactuar_held.h"
#endif

namespace ff8fx
{
namespace renzo341
{
#define RENZO_ID 341
#define RENZO_IDS "341"
#define RENZO_LENGTH_EXTRA
#define RENZO_LATE_EVENTS
	static void LateEvents(Node24 *n);
	static void Events(Node24 *n);
	static int32_t LengthExtra() { return Finisher() != 0 ? 0x2D : 0xA; }
#include "renzo_boss.inc"
#undef RENZO_LENGTH_EXTRA
#undef RENZO_LATE_EVENTS
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[1] = {
		{ ORIG_Ring, 0xC91DF4, RING_REF, 3, 8, 409, 0x12, false },
	};

	static void LateEvents(Node24 *n)
	{
		if (Finisher() == 0) return;
		if (n->c == 0x5A) *Caster() = (uint8_t)(*Caster() | 8);
		if (n->c == 0x7D) *(uint16_t *)Caster() = (uint16_t)(*(uint16_t *)Caster() & 0xFFF7);
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x50)
		{
			Node24 *t = AddSlotTask(ORIG_Ring);
			const int16_t s = (int16_t)((CrtRand() % 0x600 + 0xE00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x57)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_Dust);
			t->c = 0;
			t->e = 0xF;
			t = (Node24 *)AddTaskToQueue(QMain(), ORIG_Shake);
			t->c = 0;
			t->e = 0xF;
		}
	}

	static const DustDef DUST = { ORIG_Dust, true, 3, 0x10, 0xC00, 0x1800, 0x12C, 0x190, 0xA0, 0xFA };
	static uint32_t __cdecl DustTask(TaskNode *tn) { return DustRun(DUST, (Node24 *)tn); }
	static const ShakeDef SHAKE_DEF = { true, 10, 200, 0xA };
	static uint32_t __cdecl ShakeTask(TaskNode *tn) { return ShakeRun(SHAKE_DEF, (Node24 *)tn); }

	static uint32_t __cdecl RingTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
}

	void register_mag341_renzokuken_jumbo_cactuar()
	{
		renzo341::RegisterCommon();
		register_port(renzo341::ORIG_Ring, (void *)renzo341::RingTask, "341 Ring", 341);
		register_port(renzo341::ORIG_Dust, (void *)renzo341::DustTask, "341 Dust", 341);
		register_port(renzo341::ORIG_Shake, (void *)renzo341::ShakeTask, "341 Shake", 341);
		FX_HELD(register_mag341_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag341_renzokuken_jumbo_cactuar_held.inc"
#endif
