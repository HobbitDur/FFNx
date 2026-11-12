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
// Effect 337: Renzokuken vs the Sorceress (wormlike) (MAG_337_UNKNOWN in IDA). Squall's Renzokuken command takes the effect
// id from the TARGET's info +0x150 table (computeCommandAction 0x48DE0B): c0m117 (Sorceress, wormlike) has 337. The common
// part of the boss Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc) and its own events:
//   tick 0x46: a caster model (CasterModel 0x582270, model 0xCA79AC: the caster's matrix at the hit point, uniform scale
//     (rand % 0x600 + 0x1600) / 3, rate - rate / 5, 14 ticks, fading from tick 6 (step 512));
//   tick 0x47: a bone emitter (BoneEmitter 0x5823B0: the boss's bone 0xF1, until tick 0x16, scale 0x700..0xF7F, speed
//     0x19..0x7C, sequence 0xCA4B00);
//   tick 0x54: a slot model (Ring 0x5826C0, model 0xCA6884, reference direction 0xCAB8F0, render mode 3) at the hit point
//     turned onto the hit direction, uniform scale (rand % 0x400 + 0xC00) / 3, 18 ticks, fading from tick 6 (step 341);
//   tick 0x59: the dust (Dust 0x582850: 14 particles per tick on ticks 0..2 around the boss's bone 0xF1, scale
//     0x1200..0x19FF, radius 0x190..0x289, speed 0xBE..0x15D), a ground model (GroundModel 0x582B50, model 0xCA5E9C, at
//     that point's x / z, uniform scale (rand % 0x800 + 0x1800) / 3, rate - rate / 5, 16 ticks, fading from tick 6 (step
//     409)) and the shake (Shake 0x582C90: 8 ticks, amplitude 100, sin((tick << 10) / 8 + 0x400)).
// File loader 0x580970 (IO_GetFile_MAGIC of the name at 0xCAB900, kept at 0x21F14D8); setup MAG_337 0x580990 (not a task:
// context 0x21F14D0, caster 0x21F14D4, first target 0x21EE7C8, master 0x582DB0 in its pool 0x21EF4C0 (queue 0x21EF4D0),
// controller 0x580AB0 in the main pool 0x21F06A8 (100 x 0x24, queue 0x21F0698), length 0x21F156C = 4, finisher flag
// 0x21F1570, pools A / C / B and SLOTS freed, camera animation 0xE5D9A8, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task); no position write. CAMERA: the setup's camera animation and the shake word 0x1D97712 (Shake).
// Module globals 0x21EE748..0x21F15C0: gathered points 0x21EE748 / 0x21EE788, target 0x21EE7C8, pool A 0x21EE7D0, pool B
// 0x21EE9B0, hit records 0x21EED70, master pool / queue 0x21EF4C0 / 0x21EF4D0, SLOTS 0x21EF4E0 (100 x 0x20), pool C
// 0x21F0160, splines 0x21F0480 / 0x21F0580, hit point / direction 0x21F0680 / 0x21F0688, main queue / pool 0x21F0698 /
// 0x21F06A8, previous hit point / direction 0x21F14B8, context 0x21F14D0, caster 0x21F14D4, file 0x21F14D8, trail ring
// 0x21F14E0, length 0x21F156C, finisher 0x21F1570, texture base 0x21F1574, packet cursor 0x21F1578, hit offsets 0x21F1580
// (8 x 8 bytes, zero).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace renzo337
{
	// module globals
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
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE5D80C;
	static const uint32_t TEXTURE_NAME = 0xCAB900, CAMERA = 0xE5D9A8, ANIM_SETS = 0xE5DF6C, BIND_OUT = 0x24FEA48;
	static const uint32_t MODEL_SPARKLE = 0xE5D810, SPARKLE_FADE = 0xE5D998, SPARKLE_DEPTH = 0xE5D988;
	static const uint32_t MODEL_SLASH = 0xCA5234, MODEL_STREAK = 0xCA5C1C;
	static const uint32_t SEQ_FLASH = 0xCA4CAC, SEQ_DEBRIS = 0xCA4F7C, SEQ_SPARK = 0xCA4E58, SPARK_FRAMES = 0xCAB850;
	static const uint32_t SLASH_REF = 0xCAB8B0, HIT_LEAD = 0xCAB8C0;
	static const uint32_t RING_REF = 0xCAB8F0;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x580970, ORIG_Setup = 0x580990;
	static const uint32_t ORIG_Controller = 0x580AB0, ORIG_HitSpark = 0x580F50, ORIG_Sparkle = 0x5811E0,
		ORIG_Trail = 0x581510, ORIG_SparkSpawn = 0x581A90, ORIG_Hit = 0x581BC0, ORIG_Slash = 0x581F90,
		ORIG_CasterModel = 0x582270, ORIG_BoneEmitter = 0x5823B0, ORIG_Ring = 0x5826C0, ORIG_Dust = 0x582850,
		ORIG_GroundModel = 0x582B50, ORIG_Shake = 0x582C90, ORIG_Master = 0x582DB0;

#include "renzo_boss_defs.inc"
	// the game's data: the defaults of its parameters (renzo_boss_defs.inc's RenzoBossParams)
	static const RenzoBossParams GAME = { 0xE5D80C, { SEQ_FLASH, SEQ_DEBRIS, SEQ_SPARK, 0xCA4B00 }, { MODEL_SPARKLE, MODEL_SLASH, MODEL_STREAK, { 0xCA79AC, 0xCA6884, 0xCA5E9C, 0 } }, { TEXTURE_NAME, CAMERA, ANIM_SETS }, { HIT_LEAD, 0, SPARKLE_FADE, SPARKLE_DEPTH } };
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
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_CasterModel));
			memcpy(&t->w10, (const void *)own::R(HIT_POS), 8);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0x600 + 0x1600) / 3);
			t->w1E = s;
			t->w1C = s;
		}
		if (n->c == 0x47) ((Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_BoneEmitter)))->c = 0;
		if (n->c == 0x54)
		{
			Node24 *t = AddSlotTask(ORIG_Ring);
			const int16_t s = (int16_t)((CrtRand() % 0x400 + 0xC00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x59)
		{
			Node24 *d = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_Dust));
			d->c = 0;
			GetEffectSpawnPosition(Target(), 0xF1, 0, &d->w10);
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_GroundModel));
			memcpy(&t->w10, &d->w10, 8);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x1800) / 3);
			t->w1E = s;
			t->w1C = s;
			((Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_Shake)))->c = 0;
		}
	}

	static uint32_t __cdecl CasterModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl BoneEmitterTask(TaskNode *tn) { return BoneEmitter(BONE_EMIT, (Node24 *)tn); }
	static uint32_t __cdecl RingTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl DustTask(TaskNode *tn) { return DustRun(DUST, (Node24 *)tn); }
	static uint32_t __cdecl GroundModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[1], 1, (Node24 *)tn); }
	static uint32_t __cdecl ShakeTask(TaskNode *tn) { return ShakeRun(SHAKE_DEF, (Node24 *)tn); }
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "controller", "Controller", "Squall's Renzokuken on the Sorceress (wormlike): both animation sets and sequences, the gunblade's trail, the sparks, the hits' slashes with their flashes and debris, the caster model, the particles from her bone, the ring, the dust, the ground model and the camera shake, the screen flash and tint, the sound" },
		};
		static const Field FIELDS[] = {
			{ "controller", "sound", "Sound", "", (uint32_t)offsetof(RenzoBossParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "controller", "seqs.flash", "Flash", "The sprite sequence flash (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.flash), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.debris", "Debris", "The sprite sequence debris (a flipbook of 11 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.debris), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 10 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "seqs.emit", "Bone particles", "The sprite sequence of the emitters' particles (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RenzoBossParams, seqs.emit), fxp::Seq, 0, 0x7FFFFFFF },
			{ "controller", "models.sparkle", "Sparkle model", "The prim model sparkle (a mesh of 18 vertices, 8 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.sparkle), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.slash", "Slash model", "The prim model slash (a mesh of 96 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.slash), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.streak", "Streak model", "The prim model streak (a mesh of 21 vertices, 20 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RenzoBossParams, models.streak), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.caster", "Caster model", "The prim model caster (a mesh of 97 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 0), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.ring", "Ring model", "The prim model ring (a mesh of 112 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 4), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.ground", "Ground model", "The prim model ground (a mesh of 96 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 8), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(RenzoBossParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "controller", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(RenzoBossParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "controller", "res.anim_sets", "Animation sets", "The animation sets it binds to Squall and to the boss: body animations, sequence scripts, weapon animations ({count, offsets, end}, the game's count)", (uint32_t)offsetof(RenzoBossParams, res.anim_sets), fxp::AnimSet, 0, 0x7FFFFFFF },
			{ "controller", "tables.hit_lead", "Hit leads", "Each hit's lead (how far ahead of its slash) (48 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.hit_lead), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_fade", "Sparkle fades", "The finisher sparkles' fade by tick (8 ticks) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_fade), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_depth", "Sparkle depths", "The finisher sparkles' depth word by frame (8 frames) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_depth), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag337_renzokuken_sorceress()
	{
		fxp::register_params({ 337, "Renzokuken (vs Sorceress)", &renzo337::live, &renzo337::GAME, sizeof(renzo337::RenzoBossParams),
		                       renzo337::table::PARTS, (int)(sizeof(renzo337::table::PARTS) / sizeof(renzo337::table::PARTS[0])),
		                       renzo337::table::FIELDS, (int)(sizeof(renzo337::table::FIELDS) / sizeof(renzo337::table::FIELDS[0])),
		                       nullptr, 0 });
		renzo337::RegisterCommon();
		renzo337::Port(renzo337::ORIG_CasterModel, (void *)renzo337::CasterModelTask, "337 CasterModel");
		renzo337::Port(renzo337::ORIG_BoneEmitter, (void *)renzo337::BoneEmitterTask, "337 BoneEmitter");
		renzo337::Port(renzo337::ORIG_Ring, (void *)renzo337::RingTask, "337 Ring");
		renzo337::Port(renzo337::ORIG_Dust, (void *)renzo337::DustTask, "337 Dust");
		renzo337::Port(renzo337::ORIG_GroundModel, (void *)renzo337::GroundModelTask, "337 GroundModel");
		renzo337::Port(renzo337::ORIG_Shake, (void *)renzo337::ShakeTask, "337 Shake");
		// 30 fps layer: see mag337_renzokuken_sorceress_held.inc
		FX_HELD(register_mag337_held();)
	}
	FX_LAB_MODULE(register_mag337_renzokuken_sorceress);
}

#ifdef FF8_FX_HELD
#include "mag337_renzokuken_sorceress_held.inc"
#endif
