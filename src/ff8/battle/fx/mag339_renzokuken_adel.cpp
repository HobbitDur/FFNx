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
// Effect 339: Renzokuken vs Adel (MAG_339_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from the
// TARGET's info +0x150 table (computeCommandAction 0x48DE0B): Adel c0m112 has 339. The common part of the boss Renzokuken
// modules (renzo_boss_defs.inc / renzo_boss.inc) with its own start and events:
//   tick 1: the boss's sequence 1 started (StartBoss 0x580690); tick 2: Bind3 (0x5806A0): the container 0xE56798's three
//     com-data blocks bound to the caster (+4), the boss (+8) and the entity after the boss (+0xC: the next 0x9C entity,
//     Rinoa held by Adel), the caster's sequence 1 (2 with a finisher) started; the trail, the spark spawner and the hit
//     scheduler start on tick 2 too, with the setup's length 4; tick 3: the length from the stage durations;
//   ticks 0x16 / 0x1D / 0x48: record emitters (EmitterTask 0x57FF90, until tick 0x10 each) of kinds 2 / 4 / 8 at the hit
//     point, base directions (0, -0xA00, 0x500) / (0, -0x800, 0x600) / (0, -0xB00, 0x400);
//   tick 0x48: two slot models at the hit point turned onto the hit direction: RingA (0x580310, model 0xC9FA74, uniform
//     scale (rand % 0x400 + 0xA00) / 3, render mode 3) and RingB (0x5804A0, model 0xCA1C2C, x / z 0x1600, y (rand % 0x400 +
//     0xA00) / 3 growing), both 16 ticks, fading from tick 6 (step 409), reference direction 0xCA4AE0.
// File loader 0x57E5B0 (IO_GetFile_MAGIC of the name at 0xCA4AF0, kept at 0x21EE640); setup MAG_339 0x57E5D0 (not a task:
// context 0x21EE638, caster 0x21EE63C, first target 0x21EB930, master 0x580700 in its pool 0x21EC628 (queue 0x21EC638),
// controller 0x57E6F0 in the main pool 0x21ED810 (100 x 0x24, queue 0x21ED800), length 0x21EE6D4 = 4, finisher flag
// 0x21EE6D8, pools A / C / B and SLOTS freed, camera animation 0xE55F10, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, the binds (here three entities), chain starts (the
// caster's and the boss's), the hit-effect AnimSeq task); no position write. CAMERA: the setup's camera animation only.
// Module globals 0x21EB8B0..0x21EE728: gathered points 0x21EB8B0 / 0x21EB8F0, target 0x21EB930, pool A 0x21EB938, pool B
// 0x21EBB18, hit records 0x21EBED8, master pool / queue 0x21EC628 / 0x21EC638, SLOTS 0x21EC648 (100 x 0x20), pool C
// 0x21ED2C8, splines 0x21ED5E8 / 0x21ED6E8, hit point / direction 0x21ED7E8 / 0x21ED7F0, main queue / pool 0x21ED800 /
// 0x21ED810, previous hit point / direction 0x21EE620, context 0x21EE638, caster 0x21EE63C, file 0x21EE640, trail ring
// 0x21EE648, length 0x21EE6D4, finisher 0x21EE6D8, texture base 0x21EE6DC, packet cursor 0x21EE6E0, hit offsets
// 0x21EE6E8 (8 x 8 bytes, zero).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace renzo339
{
	// module globals
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
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE55D74;
	static const uint32_t TEXTURE_NAME = 0xCA4AF0, CAMERA = 0xE55F10, ANIM_SETS = 0xE56798, BIND_OUT = 0x24FEA08;
	static const uint32_t MODEL_SPARKLE = 0xE55D78, SPARKLE_FADE = 0xE55F00, SPARKLE_DEPTH = 0xE55EF0;
	static const uint32_t MODEL_SLASH = 0xC9E424, MODEL_STREAK = 0xC9EE0C;
	static const uint32_t SEQ_FLASH = 0xC9DE9C, SEQ_DEBRIS = 0xC9E16C, SEQ_SPARK = 0xC9E048, SPARK_FRAMES = 0xCA4A40;
	static const uint32_t SLASH_REF = 0xCA4AA0, HIT_LEAD = 0xCA4AB0;
	static const uint32_t SEQ_EMIT = 0xC9DCF0, RING_REF = 0xCA4AE0;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x57E5B0, ORIG_Setup = 0x57E5D0;
	static const uint32_t ORIG_Controller = 0x57E6F0, ORIG_HitSpark = 0x57EC70, ORIG_Sparkle = 0x57EF00,
		ORIG_Trail = 0x57F230, ORIG_SparkSpawn = 0x57F7B0, ORIG_Hit = 0x57F8E0, ORIG_Slash = 0x57FCB0,
		ORIG_Emitter = 0x57FF90, ORIG_RingA = 0x580310, ORIG_RingB = 0x5804A0, ORIG_Master = 0x580700;

#include "renzo_boss_defs.inc"
	// the game's data: the defaults of its parameters (renzo_boss_defs.inc's RenzoBossParams)
	static const RenzoBossParams GAME = { 0xE55D74, { SEQ_FLASH, SEQ_DEBRIS, SEQ_SPARK, SEQ_EMIT }, { MODEL_SPARKLE, MODEL_SLASH, MODEL_STREAK, { 0xC9FA74, 0xCA1C2C, 0, 0 } }, { TEXTURE_NAME, CAMERA, ANIM_SETS }, { HIT_LEAD, 0, SPARKLE_FADE, SPARKLE_DEPTH } };
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

	// the two slot models (RingA: uniform scale, render mode 3; RingB: x / z fixed, y growing)
	static const SlotModelDef SLOTMODELS[2] = {
		{ ORIG_RingA, 0xC9FA74, RING_REF, 3, 6, 409, 0x10, false },
		{ ORIG_RingB, 0xCA1C2C, RING_REF, 2, 6, 409, 0x10, true },
	};

	// StartBoss (0x580690): the boss's sequence 1
	static void StartBoss(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		(void)caster; (void)out; (void)sets; (void)seq;
		QueueChainTransformation(boss, 1);
	}

	// Bind3 (0x5806A0): the container's three com-data blocks bound to the caster, the boss and the entity after it; the
	// caster's sequence `seq` started
	static void Bind3(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		next = BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		BindEntityModelFromComData(boss + 0x9C, next, sets + *(const uint32_t *)(sets + 0xC));
		QueueChainTransformation(caster, seq);
	}

	static void Start(Node24 *n)
	{
		if (n->c == 1) StartBoss(Caster(), Target(), BIND_OUT, AnimSets(), (Finisher() != 0) + 1);
		if (n->c == 2) Bind3(Caster(), Target(), BIND_OUT, AnimSets(), (Finisher() != 0) + 1);
		if (n->c == 3) ComputeLength();
		if (n->c == 2) StartTasks();
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x16) AddRecordEmitter(ORIG_Emitter, 2, 0, -0xA00, 0x500);
		if (n->c == 0x1D) AddRecordEmitter(ORIG_Emitter, 4, 0, -0x800, 0x600);
		if (n->c == 0x48)
		{
			AddRecordEmitter(ORIG_Emitter, 8, 0, -0xB00, 0x400);
			Node24 *t = AddSlotTask(ORIG_RingA);
			const int16_t s = (int16_t)((CrtRand() % 0x400 + 0xA00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
			t = AddSlotTask(ORIG_RingB);
			t->w1C = 0x1600;
			const int16_t sy = (int16_t)((CrtRand() % 0x400 + 0xA00) / 3);
			t->w22 = sy;
			t->w20 = sy;
			HitPointRecord(t);
		}
	}

	// Emitter (0x57FF90): kind 2 / 4 / 8 particles in SLOTS until tick 0x10 (renzo_boss.inc)
	static uint32_t __cdecl EmitterTask(TaskNode *tn) { return RecordEmitter(tn, 0x10, SEQ_EMIT); }
	// RingA (0x580310) / RingB (0x5804A0)
	static uint32_t __cdecl RingATask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl RingBTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[1], 1, (Node24 *)tn); }
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "controller", "Controller", "Squall's Renzokuken on Adel: the animation sets bound to Squall, Adel and Rinoa, the sequences, the gunblade's trail, the sparks, the hits' slashes with their flashes and debris, three particle emitters, the two rings, the screen flash and tint, the sound" },
		};
		static const Field FIELDS[] = {
			{ "controller", "sound", "Sound", "", (uint32_t)offsetof(RenzoBossParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "controller", "seqs.flash", "Flash", "The sprite sequence flash (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.flash), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.debris", "Debris", "The sprite sequence debris (a flipbook of 11 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.debris), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 10 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.emit", "Emitter particles", "The sprite sequence of the emitters' particles (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.emit), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "models.sparkle", "Sparkle model", "The prim model sparkle (a mesh of 18 vertices, 8 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.sparkle), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.slash", "Slash model", "The prim model slash (a mesh of 96 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.slash), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.streak", "Streak model", "The prim model streak (a mesh of 21 vertices, 20 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.streak), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.ring_a", "Ring a model", "The prim model ring a (a mesh of 112 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 0), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.ring_b", "Ring b model", "The prim model ring b (a mesh of 73 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 4), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(RenzoBossParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "controller", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(RenzoBossParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "controller", "res.anim_sets", "Animation sets", "The animation sets it binds to Squall and to the boss and Rinoa: body animations, sequence scripts, weapon animations ({count, offsets, end}, the game's count)", (uint32_t)offsetof(RenzoBossParams, res.anim_sets), fxp::AnimSet, 0, 0x7FFFFFFF },
			{ "controller", "tables.hit_lead", "Hit leads", "Each hit's lead (how far ahead of its slash) (48 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.hit_lead), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_fade", "Sparkle fades", "The finisher sparkles' fade by tick (8 ticks) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_fade), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_depth", "Sparkle depths", "The finisher sparkles' depth word by frame (8 frames) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_depth), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag339_renzokuken_adel()
	{
		fxp::register_params({ 339, "Renzokuken (vs Adel)", &renzo339::live, &renzo339::GAME, sizeof(renzo339::RenzoBossParams),
		                       renzo339::table::PARTS, (int)(sizeof(renzo339::table::PARTS) / sizeof(renzo339::table::PARTS[0])),
		                       renzo339::table::FIELDS, (int)(sizeof(renzo339::table::FIELDS) / sizeof(renzo339::table::FIELDS[0])),
		                       nullptr, 0 });
		renzo339::RegisterCommon();
		renzo339::Port(renzo339::ORIG_Emitter, (void *)renzo339::EmitterTask, "339 Emitter");
		renzo339::Port(renzo339::ORIG_RingA, (void *)renzo339::RingATask, "339 RingA");
		renzo339::Port(renzo339::ORIG_RingB, (void *)renzo339::RingBTask, "339 RingB");
		// 30 fps layer: see mag339_renzokuken_adel_held.inc
		FX_HELD(register_mag339_held();)
	}
	FX_LAB_MODULE(register_mag339_renzokuken_adel);
}

#ifdef FF8_FX_HELD
#include "mag339_renzokuken_adel_held.inc"
#endif
