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
// Effect 343: Renzokuken vs Griever (MAG_343_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from the
// TARGET's info +0x150 table (computeCommandAction 0x48DE0B): Griever c0m122 has 343. The common part of the boss
// Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc: controller frame, both animation sets, trail, sparks, hits,
// slashes, particles, hit sparks; the hit spark tests AddTaskToQueue's answer here) and its own events:
//   tick 0x24: an emitter (EmitterTask 0x576EC0, its own SLOTS record: the hit point, base direction (0x600, -0xB00,
//     0x500)) that spawns kind-4 particles; tick 0x2C: a second one, kind 8, direction (-0x200, -0x800, 0x700);
//   tick 0x37: the closing model (FinalTask 0x577240, model 0xC852AC) at the hit point, scale (rand % 0xC00 + 0x1C00) / 3,
//     and the caster's matrix kept (0x21E1E28) for it.
//   EmitterTask (0x576EC0) - ticks 0..0xE: 3 particles per tick in SLOTS (alive = its kind, scale 0x700..0xF7F, at the
//     record's point, velocity along its direction (+- 0x300 / 0x300 / 0x100 random, through the caster's rotation) times
//     0x23..0xB8 (x + 0..0x27 - 0x14)); every tick it draws its kind's particles (sprite sequence 0xC82400, the view-space
//     point moved scale / 8 toward the eye), growing (scale >> 4), moving, slowing (>> 4), freed at the sequence's end; it
//     ends (and frees its own record) on the first tick with none left.
//   FinalTask (0x577240) - model 0xC852AC turned (0, 0, 0x400) then by the kept caster matrix, uniform scale +0x1C (+= rate,
//     rate -= rate / 4), 18 ticks, fading from tick 6 (step 341).
// File loader 0x575630 (IO_GetFile_MAGIC of the name at 0xC891F0, kept at 0x21E2CA0); setup MAG_343 0x575650 (not a task:
// context 0x21E2C98, caster 0x21E2C9C, first target 0x21DFF70, master 0x577440 in its pool 0x21E0C68 (queue 0x21E0C78),
// controller 0x575770 in the main pool 0x21E1E70 (100 x 0x24, queue 0x21E1E60), length 0x21E2D34 = 4, finisher flag
// 0x21E2D38, pools A / C / B and SLOTS freed, camera animation 0xE422E8, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq task);
// no position write. CAMERA: the setup's camera animation only.
// Module globals 0x21DFEF0..0x21E2D68: gathered points 0x21DFEF0 / 0x21DFF30, target 0x21DFF70, pool A 0x21DFF78, pool B
// 0x21E0158 (40 of 60 spawned), hit records 0x21E0518, master pool / queue 0x21E0C68 / 0x21E0C78, SLOTS 0x21E0C88 (100 x
// 0x20), pool C 0x21E1908, splines 0x21E1C28 / 0x21E1D28, kept caster matrix 0x21E1E28, hit point / direction 0x21E1E48 /
// 0x21E1E50, main queue / pool 0x21E1E60 / 0x21E1E70, previous hit point / direction 0x21E2C80, context 0x21E2C98, caster
// 0x21E2C9C, file 0x21E2CA0, trail ring 0x21E2CA8, length 0x21E2D34, finisher 0x21E2D38, texture base 0x21E2D3C, packet
// cursor 0x21E2D40, hit offsets 0x21E2D48 (8 x 8 bytes, zero).

#include "mag_common.h"

namespace ff8fx
{
namespace renzo343
{
	// module globals
	static const uint32_t PTS_A = 0x21DFEF0, PTS_B = 0x21DFF30, TARGET_SLOT = 0x21DFF70;
	static const uint32_t POOL_A = 0x21DFF78, POOL_A_END = 0x21E0158;
	static const uint32_t POOL_B = 0x21E0158, POOL_B_SPAWN_END = 0x21E03D8, POOL_B_END = 0x21E0518;
	static const uint32_t HIT_REC = 0x21E0518, MASTER_POOL = 0x21E0C68, Q_MASTER = 0x21E0C78;
	static const uint32_t SLOTS = 0x21E0C88, SLOTS_END = 0x21E1908;
	static const uint32_t POOL_C = 0x21E1908, POOL_C_END = 0x21E1C28;
	static const uint32_t SPL_A = 0x21E1C28, SPL_B = 0x21E1D28, SAVED_MAT = 0x21E1E28;
	static const uint32_t HIT_POS = 0x21E1E48, HIT_DIR = 0x21E1E50, Q_MAIN = 0x21E1E60, MAIN_POOL = 0x21E1E70;
	static const uint32_t PREV_HIT = 0x21E2C80, CONTEXT = 0x21E2C98, CASTER_SLOT = 0x21E2C9C, TEX_FILE = 0x21E2CA0;
	static const uint32_t RING = 0x21E2CA8, RING_END = 0x21E2D34, LENGTH = 0x21E2D34, FINISHER = 0x21E2D38;
	static const uint32_t TEX_BASE = 0x21E2D3C, PACKET_CURSOR = 0x21E2D40, HIT_OFFS = 0x21E2D48;
	static const uint32_t PARITY = 0xC000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE4214C;
	static const uint32_t TEXTURE_NAME = 0xC891F0, CAMERA = 0xE422E8, ANIM_SETS = 0xE426C0, BIND_OUT = 0x24FE948;
	static const uint32_t MODEL_SPARKLE = 0xE42150, SPARKLE_FADE = 0xE422D8, SPARKLE_DEPTH = 0xE422C8;
	static const uint32_t MODEL_SLASH = 0xC82B34, MODEL_STREAK = 0xC8351C;
	static const uint32_t SEQ_FLASH = 0xC825AC, SEQ_DEBRIS = 0xC8287C, SEQ_SPARK = 0xC82758, SPARK_FRAMES = 0xC89150;
	static const uint32_t SLASH_REF = 0xC891B0, HIT_LEAD = 0xC891C0;
	static const uint32_t SEQ_EMIT = 0xC82400, MODEL_FINAL = 0xC852AC;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x575630, ORIG_Setup = 0x575650;
	static const uint32_t ORIG_Controller = 0x575770, ORIG_HitSpark = 0x575BA0, ORIG_Sparkle = 0x575E30,
		ORIG_Trail = 0x576160, ORIG_SparkSpawn = 0x5766E0, ORIG_Hit = 0x576810, ORIG_Slash = 0x576BE0,
		ORIG_Emitter = 0x576EC0, ORIG_Final = 0x577240, ORIG_Master = 0x577440;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag343_renzokuken_griever_held.h"
#endif

namespace ff8fx
{
namespace renzo343
{
#define RENZO_ID 343
#define RENZO_IDS "343"
#define RENZO_HITSPARK_NULLCHECK
	static void Events(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_HITSPARK_NULLCHECK
#undef RENZO_IDS
#undef RENZO_ID

	static void Events(Node24 *n)
	{
		if (n->c == 0x24) AddRecordEmitter(ORIG_Emitter, 4, 0x600, -0xB00, 0x500);
		if (n->c == 0x2C) AddRecordEmitter(ORIG_Emitter, 8, -0x200, -0x800, 0x700);
		if (n->c == 0x37)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_Final);
			memcpy(&t->w10, (const void *)HIT_POS, 8);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0xC00 + 0x1C00) / 3);
			t->w1E = s;
			t->w1C = s;
			memcpy((void *)SAVED_MAT, Caster() + 0x40, 0x20);
		}
	}

	// Emitter (0x576EC0): kind 4 / 8 particles in SLOTS until tick 0xE (renzo_boss.inc)
	static uint32_t __cdecl EmitterTask(TaskNode *tn) { return RecordEmitter(tn, 0xE, SEQ_EMIT); }

	// ------------------------------------------------------------------
	// Final (0x577240): the closing model through the kept caster matrix
	// ------------------------------------------------------------------
	static void FinalDraw(const Node24 *n, int32_t scale, int32_t fade)
	{
		int16_t ang[3] = { 0, 0, 0x400 };
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only)
		ComposeZYXRotationMatrix(ang, &m);
		MatrixMultiply((const Mat4x3 *)SAVED_MAT, &m);
		m.t[0] = n->w10;
		m.t[1] = n->w12;
		int32_t sv[3];
		sv[2] = scale;
		sv[1] = scale;
		sv[0] = scale;
		m.t[2] = n->w14;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_FINAL;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (n->c >= 6)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static int32_t FinalFade(int32_t c) { return mul32(c - 6, 341); }

	static uint32_t __cdecl FinalTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag343_renzokuken_griever_held.inc
		FX_HELD(held_note_final(n);)
		FinalDraw(n, n->w1C, FinalFade(n->c));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - (int16_t)(rate / 4));
		n->c++;
		return n->c >= 0x12 ? TASK_END : 0;
	}
}

	void register_mag343_renzokuken_griever()
	{
		renzo343::RegisterCommon();
		register_port(renzo343::ORIG_Emitter, (void *)renzo343::EmitterTask, "343 Emitter", 343);
		register_port(renzo343::ORIG_Final, (void *)renzo343::FinalTask, "343 Final", 343);
		// 30 fps layer: see mag343_renzokuken_griever_held.inc
		FX_HELD(register_mag343_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag343_renzokuken_griever_held.inc"
#endif
