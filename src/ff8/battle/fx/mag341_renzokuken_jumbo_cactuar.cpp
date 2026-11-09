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
// Effect 341: Renzokuken vs Jumbo Cactuar (MAG_341_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from the
// TARGET's info +0x150 table (computeCommandAction 0x48DE0B): Jumbo Cactuar c0m084 has 341. The common part of the boss
// Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc) and its own events:
//   tick 0x50: a slot model (Ring 0x57B330, model 0xC91DF4, reference direction 0xC96E60, render mode 3) at the hit point
//     turned onto the hit direction, uniform scale (rand % 0x600 + 0xE00) / 3, 18 ticks, fading from tick 8 (step 409);
//   tick 0x57: the dust (DustTask 0x57B520) and the shake (ShakeTask 0x57B880), both after a delay of 15 ticks;
//   the length is the stages + 10 ticks (+ 45 with a finisher); with a finisher the caster's entity flag 8 is set on tick
//   0x5A and cleared on tick 0x7D.
//   DustTask (0x57B520) - on its tick 0 the boss's default effect position into the node (+0x10); ticks 0..3: 16 dust
//     particles of kind 4 per tick in SLOTS around it (angle rand % 0x1000, radius 0x190..0x2BB, y -(0..0x31), scale
//     0x1800..0x23FF, outward speed 0xFA..0x199, x / z only); every tick the kind-4 particles drawn (the shared sprite
//     sequence 3, frame offset 8) through the camera translated to the node's x / z, each moved scale / 8 toward the eye,
//     moving / slowing (>> 3) on x / z; ends on the first tick with none left.
//   ShakeTask (0x57B880) - 10 ticks: the camera shake word 0x1D97712 = sin((tick << 10) / 10 + 0x400) * 200 >> 12,
//     negated on even ticks (also on draw-only ticks).
// File loader 0x579A60 (IO_GetFile_MAGIC of the name at 0xC96E70, kept at 0x21E8970); setup MAG_341 0x579A80 (not a task:
// context 0x21E8968, caster 0x21E896C, first target 0x21E5C60, master 0x57B970 in its pool 0x21E6958 (queue 0x21E6968),
// controller 0x579BA0 in the main pool 0x21E7B40 (100 x 0x24, queue 0x21E7B30), length 0x21E8A04 = 4, finisher flag
// 0x21E8A08, pools A / C / B and SLOTS freed, camera animation 0xE4EBB4, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task); no position write. CAMERA: the setup's camera animation and the shake word 0x1D97712 (ShakeTask).
// Module globals 0x21E5BE0..0x21E8A58: gathered points 0x21E5BE0 / 0x21E5C20, target 0x21E5C60, pool A 0x21E5C68, pool B
// 0x21E5E48, hit records 0x21E6208, master pool / queue 0x21E6958 / 0x21E6968, SLOTS 0x21E6978 (100 x 0x20), pool C
// 0x21E75F8, splines 0x21E7918 / 0x21E7A18, hit point / direction 0x21E7B18 / 0x21E7B20, main queue / pool 0x21E7B30 /
// 0x21E7B40, previous hit point / direction 0x21E8950, context 0x21E8968, caster 0x21E896C, file 0x21E8970, trail ring
// 0x21E8978, length 0x21E8A04, finisher 0x21E8A08, texture base 0x21E8A0C, packet cursor 0x21E8A10, hit offsets 0x21E8A18
// (8 x 8 bytes, zero).

#include "mag_common.h"

namespace ff8fx
{
namespace renzo341
{
	// module globals
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
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE4EA18;
	static const uint32_t TEXTURE_NAME = 0xC96E70, CAMERA = 0xE4EBB4, ANIM_SETS = 0xE4F038, BIND_OUT = 0x24FE9A8;
	static const uint32_t MODEL_SPARKLE = 0xE4EA1C, SPARKLE_FADE = 0xE4EBA4, SPARKLE_DEPTH = 0xE4EB94;
	static const uint32_t MODEL_SLASH = 0xC907A4, MODEL_STREAK = 0xC9118C;
	static const uint32_t SEQ_FLASH = 0xC9021C, SEQ_DEBRIS = 0xC904EC, SEQ_SPARK = 0xC903C8, SPARK_FRAMES = 0xC96DC0;
	static const uint32_t SLASH_REF = 0xC96E20, HIT_LEAD = 0xC96E30;
	static const uint32_t RING_REF = 0xC96E60;
	// tasks
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
	// the length: the stages + 10 ticks (45 with a finisher)
	static int32_t LengthExtra() { return Finisher() != 0 ? 0x2D : 0xA; }
#include "renzo_boss.inc"
#undef RENZO_LENGTH_EXTRA
#undef RENZO_LATE_EVENTS
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[1] = {
		{ ORIG_Ring, 0xC91DF4, RING_REF, 3, 8, 409, 0x12, false },
	};

	// with a finisher: the caster's entity flag 8 set on tick 0x5A, cleared on tick 0x7D
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

	// Dust (0x57B520) / Shake (0x57B880): renzo_boss.inc
	static const DustDef DUST = { ORIG_Dust, true, 3, 0x10, 0xC00, 0x1800, 0x12C, 0x190, 0xA0, 0xFA };
	static uint32_t __cdecl DustTask(TaskNode *tn) { return DustRun(DUST, (Node24 *)tn); }
	static const ShakeDef SHAKE_DEF = { true, 10, 200, 0xA };
	static uint32_t __cdecl ShakeTask(TaskNode *tn) { return ShakeRun(SHAKE_DEF, (Node24 *)tn); }

	// Ring (0x57B330)
	static uint32_t __cdecl RingTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
}

	void register_mag341_renzokuken_jumbo_cactuar()
	{
		renzo341::RegisterCommon();
		register_port(renzo341::ORIG_Ring, (void *)renzo341::RingTask, "341 Ring", 341);
		register_port(renzo341::ORIG_Dust, (void *)renzo341::DustTask, "341 Dust", 341);
		register_port(renzo341::ORIG_Shake, (void *)renzo341::ShakeTask, "341 Shake", 341);
		// 30 fps layer: see mag341_renzokuken_jumbo_cactuar_held.inc
		FX_HELD(register_mag341_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag341_renzokuken_jumbo_cactuar_held.inc"
#endif
