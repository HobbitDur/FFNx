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
// Effect 335: Renzokuken vs NORG (MAG_335_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from the
// TARGET's info +0x150 table (computeCommandAction 0x48DE0B): NORG c0m077 has 335. The common part of the boss Renzokuken
// modules (renzo_boss_defs.inc / renzo_boss.inc; its hit offsets 0xCB9560 are a real table here) and its own events:
//   tick 0xD: the caster put in front of NORG: entity +0x1C = the boss's default effect position x, +0x20 = its z + 0xF0A;
//     tick length - 5 (after 0xD): back home (+0x1C / +0x20 = +0x14 / +0x18);
//   tick 0x28: the boss emitter (BossEmitter 0x586F90) at the hit point; tick 0x3B: a slot model (Ring 0x5872C0, model
//     0xCB44F4, reference direction 0xCB95D0, render mode 3) at the hit point turned onto the hit direction, uniform scale
//     (rand % 0x600 + 0x1000) / 3, 18 ticks, fading from tick 6 (step 341).
//   BossEmitter (0x586F90) - ticks 0..0x10: 3 particles of kind 2 per tick at the boss's effect bone (0xF0, fraction
//     0x1000) + (0, -600, -0x226): scale 0x800..0xDFF, direction (+- 0x300, -0x900..-0xBFF, 0x800..0xAFF) normalized,
//     speed 0x5A..0x185; the kind-2 particles drawn every tick (sequence 0xCB2770, EmitterDraw); ends on the first tick
//     with none left.
// The controller (0x5857D0) has no function start in the IDB / functions.json (the setup's MAG_335_UNKNOWN 0x5856B0 runs
// into it); the other functions are sub_* there (0x585C70 HitSpark .. 0x5874B0 BindBoth).
// File loader 0x585690 (IO_GetFile_MAGIC of the name at 0xCB95E0, kept at 0x21F71A8); setup 0x5856B0 (not a task: context
// 0x21F71A0, caster 0x21F71A4, first target 0x21F4498, master 0x587500 in its pool 0x21F5190 (queue 0x21F51A0), controller
// 0x5857D0 in the main pool 0x21F6378 (100 x 0x24, queue 0x21F6368), length 0x21F723C = 4, finisher flag 0x21F7240, pools
// A / C / B and SLOTS freed, camera animation 0xE6C628, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task) and the caster's POSITION words +0x1C / +0x20 (ticks 0xD and length - 5: a jump, not a movement). CAMERA: the
// setup's camera animation only.
// Module globals 0x21F4418..0x21F724C: gathered points 0x21F4418 / 0x21F4458, target 0x21F4498, pool A 0x21F44A0, pool B
// 0x21F4680, hit records 0x21F4A40, master pool / queue 0x21F5190 / 0x21F51A0, SLOTS 0x21F51B0 (100 x 0x20), pool C
// 0x21F5E30, splines 0x21F6150 / 0x21F6250, hit point / direction 0x21F6350 / 0x21F6358, main queue / pool 0x21F6368 /
// 0x21F6378, previous hit point / direction 0x21F7188, context 0x21F71A0, caster 0x21F71A4, file 0x21F71A8, trail ring
// 0x21F71B0, length 0x21F723C, finisher 0x21F7240, texture base 0x21F7244, packet cursor 0x21F7248.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace renzo335
{
	// module globals
	static const uint32_t PTS_A = 0x21F4418, PTS_B = 0x21F4458, TARGET_SLOT = 0x21F4498;
	static const uint32_t POOL_A = 0x21F44A0, POOL_A_END = 0x21F4680;
	static const uint32_t POOL_B = 0x21F4680, POOL_B_SPAWN_END = 0x21F4900, POOL_B_END = 0x21F4A40;
	static const uint32_t HIT_REC = 0x21F4A40, MASTER_POOL = 0x21F5190, Q_MASTER = 0x21F51A0;
	static const uint32_t SLOTS = 0x21F51B0, SLOTS_END = 0x21F5E30;
	static const uint32_t POOL_C = 0x21F5E30, POOL_C_END = 0x21F6150;
	static const uint32_t SPL_A = 0x21F6150, SPL_B = 0x21F6250;
	static const uint32_t HIT_POS = 0x21F6350, HIT_DIR = 0x21F6358, Q_MAIN = 0x21F6368, MAIN_POOL = 0x21F6378;
	static const uint32_t PREV_HIT = 0x21F7188, CONTEXT = 0x21F71A0, CASTER_SLOT = 0x21F71A4, TEX_FILE = 0x21F71A8;
	static const uint32_t RING = 0x21F71B0, RING_END = 0x21F723C, LENGTH = 0x21F723C, FINISHER = 0x21F7240;
	static const uint32_t TEX_BASE = 0x21F7244, PACKET_CURSOR = 0x21F7248;
	static const uint32_t PARITY = 0xC000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE6C48C;
	static const uint32_t TEXTURE_NAME = 0xCB95E0, CAMERA = 0xE6C628, ANIM_SETS = 0xE6CC38, BIND_OUT = 0x24FEAA8;
	static const uint32_t MODEL_SPARKLE = 0xE6C490, SPARKLE_FADE = 0xE6C618, SPARKLE_DEPTH = 0xE6C608;
	static const uint32_t MODEL_SLASH = 0xCB2EA4, MODEL_STREAK = 0xCB388C;
	static const uint32_t SEQ_FLASH = 0xCB291C, SEQ_DEBRIS = 0xCB2BEC, SEQ_SPARK = 0xCB2AC8, SPARK_FRAMES = 0xCB94C0;
	static const uint32_t SLASH_REF = 0xCB9520, HIT_LEAD = 0xCB9530, HIT_OFFS = 0xCB9560;
	static const uint32_t SEQ_EMIT = 0xCB2770, RING_REF = 0xCB95D0;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x585690, ORIG_Setup = 0x5856B0;
	static const uint32_t ORIG_Controller = 0x5857D0, ORIG_HitSpark = 0x585C70, ORIG_Sparkle = 0x585F00,
		ORIG_Trail = 0x586230, ORIG_SparkSpawn = 0x5867B0, ORIG_Hit = 0x5868E0, ORIG_Slash = 0x586CB0,
		ORIG_BossEmitter = 0x586F90, ORIG_Ring = 0x5872C0, ORIG_Master = 0x587500;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag335_renzokuken_norg_held.h"
#endif

namespace ff8fx
{
namespace renzo335
{
#define RENZO_ID 335
#define RENZO_IDS "335"
	static void Events(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[1] = {
		{ ORIG_Ring, 0xCB44F4, RING_REF, 3, 6, 341, 0x12, false },
	};

	static void Events(Node24 *n)
	{
		if (n->c == 0xD)
		{
			// the caster in front of NORG
			int16_t p[4];
			GetDefaultEffectPosition(Target(), p);
			uint8_t *e = Caster();
			*(int16_t *)(e + 0x1C) = p[0];
			*(int16_t *)(e + 0x20) = (int16_t)(p[2] + 0xF0A);
		}
		if (n->c > 0xD && (int32_t)n->c == Length() - 5)
		{
			// back home
			uint8_t *e = Caster();
			*(int16_t *)(e + 0x1C) = *(const int16_t *)(e + 0x14);
			*(int16_t *)(e + 0x20) = *(const int16_t *)(e + 0x18);
		}
		if (n->c == 0x28)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_BossEmitter);
			memcpy(&t->w10, (const void *)HIT_POS, 8);
			t->c = 0;
		}
		if (n->c == 0x3B)
		{
			Node24 *t = AddSlotTask(ORIG_Ring);
			const int16_t s = (int16_t)((CrtRand() % 0x600 + 0x1000) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
	}

	// ------------------------------------------------------------------
	// BossEmitter (0x586F90): kind-2 particles from the boss's effect bone
	// ------------------------------------------------------------------
	static uint32_t __cdecl BossEmitterTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= 0x10)
		{
			GetEffectSpawnPosition(Target(), 0xF0, 0x1000, w->pos2);
			w->pos2[1] = (int16_t)(w->pos2[1] - 600);
			w->pos2[2] = (int16_t)(w->pos2[2] - 0x226);
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = 2;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x600 + 0x800);
				memcpy(p->pos, w->pos2, 8);
				w->dir[0] = CrtRand() % 0x600 - 0x300;
				w->dir[1] = -0x900 - CrtRand() % 0x300;
				w->dir[2] = CrtRand() % 0x300 + 0x800;
				Normalize(w->dir, w->dir);
				const int32_t sp = CrtRand() % 0x12C + 0x5A;
				p->w10 = (int16_t)(mul32(w->dir[0], sp) >> 12);
				p->w12 = (int16_t)(mul32(w->dir[1], sp) >> 12);
				p->w14 = (int16_t)(mul32(w->dir[2], sp) >> 12);
			}
		}
		const int32_t alive = EmitterDraw(n, w, 2, SEQ_EMIT);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	// Ring (0x5872C0)
	static uint32_t __cdecl RingTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
}

	void register_mag335_renzokuken_norg()
	{
		renzo335::RegisterCommon();
		register_port(renzo335::ORIG_BossEmitter, (void *)renzo335::BossEmitterTask, "335 BossEmitter", 335);
		register_port(renzo335::ORIG_Ring, (void *)renzo335::RingTask, "335 Ring", 335);
		// 30 fps layer: see mag335_renzokuken_norg_held.inc
		FX_HELD(register_mag335_held();)
	}
	FX_LAB_MODULE(register_mag335_renzokuken_norg);
}

#ifdef FF8_FX_HELD
#include "mag335_renzokuken_norg_held.inc"
#endif
