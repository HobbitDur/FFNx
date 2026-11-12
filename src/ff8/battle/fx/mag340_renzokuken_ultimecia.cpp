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
// Effect 340: Renzokuken vs Ultimecia (final form) (MAG_340_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id
// from the TARGET's info +0x150 table (computeCommandAction 0x48DE0B): c0m126 (Ultimecia, final form) has 340. The common
// part of the boss Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc; its hit offsets 0xC9DC70 are a real table here)
// with its own start and events:
//   tick 1: the boss's sequence 1 started (StartBoss 0x57E2F0); tick 2: Bind2 (0x57E300): the container 0xE510FC's two
//     com-data blocks bound to the caster and the boss, only the caster's sequence 1 (2 with a finisher) started; the
//     trail, the spark spawner and the hit scheduler start on tick 2 too, with the setup's length 4; tick 3: the length;
//   tick 0x1C: a slot model (RingA 0x57D6B0, model 0xC99D2C, reference 0xC9DC30) at the hit point onto the hit direction,
//     scale (rand % 0x900 + 0x1400) / 3; tick 0x23: a second RingA at the hit point + (600, 100, -400) through the caster's
//     matrix (matrixMultiplyVector in the node +0x10);
//   tick 0x3B: a caster model (CasterModel 0x57D840, model 0xC9B9EC) at the boss's bone 0x1D, scale (rand % 0x800 + 0x2000)
//     / 3, and a bone emitter (BoneEmitter 0x57DC60: bone 0x1D, until tick 0xE, sequence 0xC96E80);
//   tick 0x43: a slot model (RingB 0x57D980, model 0xC98C04, reference 0xC9DCD0) at the hit point onto the hit direction,
//     scale (rand % 0x500 + 0xD00) / 3, and the node emitter (NodeEmitter 0x57DF60) at the hit point - 300 in y;
//   tick 0x44: a turned model (AngleModel 0x57DB10, model 0xC9CF24) at the hit point's x / z, y 0x226, y angle
//     rand % 0x1000, x / z scale 0x3200, y scale (rand % 0xC00 + 0x1400) / 3 growing (- rate / 4), 18 ticks, fading from
//     tick 8 (step 409).
//   Slot models: render mode 3, 16 ticks, fading from tick 6 (step 409); CasterModel: rate - rate / 4, 16 ticks, fading
//   from tick 6 (step 409).
//   NodeEmitter (0x57DF60) - ticks 0..0x10: 3 particles of kind 4 per tick at its point, direction (-0x100..0xFF,
//     -0x900..-0xDFF, 0x400..0x9FF) normalized through the caster's rotation, speed 0x28..0xB3 (x + 0..0x27 - 0x14), scale
//     0x700..0xF7F; the kind-4 particles drawn every tick (EmitterDraw, sequence 0xC96E80); ends on the first tick with
//     none left.
// File loader 0x57BBE0 (IO_GetFile_MAGIC of the name at 0xC9DCE0, kept at 0x21EB808); setup MAG_340 0x57BC00 (not a task:
// context 0x21EB800, caster 0x21EB804, first target 0x21E8AF8, master 0x57E340 in its pool 0x21E97F0 (queue 0x21E9800),
// controller 0x57BD20 in the main pool 0x21EA9D8 (100 x 0x24, queue 0x21EA9C8), length 0x21EB89C = 4, finisher flag
// 0x21EB8A0, pools A / C / B and SLOTS freed, camera animation 0xE50D14, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, the binds, chain starts (the caster's and the
// boss's), the hit-effect AnimSeq task); no position write. CAMERA: the setup's camera animation only.
// Module globals 0x21E8A78..0x21EB8AC: gathered points 0x21E8A78 / 0x21E8AB8, target 0x21E8AF8, pool A 0x21E8B00, pool B
// 0x21E8CE0, hit records 0x21E90A0, master pool / queue 0x21E97F0 / 0x21E9800, SLOTS 0x21E9810 (100 x 0x20), pool C
// 0x21EA490, splines 0x21EA7B0 / 0x21EA8B0, hit point / direction 0x21EA9B0 / 0x21EA9B8, main queue / pool 0x21EA9C8 /
// 0x21EA9D8, previous hit point / direction 0x21EB7E8, context 0x21EB800, caster 0x21EB804, file 0x21EB808, trail ring
// 0x21EB810, length 0x21EB89C, finisher 0x21EB8A0, texture base 0x21EB8A4, packet cursor 0x21EB8A8.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace renzo340
{
	// module globals
	static const uint32_t PTS_A = 0x21E8A78, PTS_B = 0x21E8AB8, TARGET_SLOT = 0x21E8AF8;
	static const uint32_t POOL_A = 0x21E8B00, POOL_A_END = 0x21E8CE0;
	static const uint32_t POOL_B = 0x21E8CE0, POOL_B_SPAWN_END = 0x21E8F60, POOL_B_END = 0x21E90A0;
	static const uint32_t HIT_REC = 0x21E90A0, MASTER_POOL = 0x21E97F0, Q_MASTER = 0x21E9800;
	static const uint32_t SLOTS = 0x21E9810, SLOTS_END = 0x21EA490;
	static const uint32_t POOL_C = 0x21EA490, POOL_C_END = 0x21EA7B0;
	static const uint32_t SPL_A = 0x21EA7B0, SPL_B = 0x21EA8B0;
	static const uint32_t HIT_POS = 0x21EA9B0, HIT_DIR = 0x21EA9B8, Q_MAIN = 0x21EA9C8, MAIN_POOL = 0x21EA9D8;
	static const uint32_t PREV_HIT = 0x21EB7E8, CONTEXT = 0x21EB800, CASTER_SLOT = 0x21EB804, TEX_FILE = 0x21EB808;
	static const uint32_t RING = 0x21EB810, RING_END = 0x21EB89C, LENGTH = 0x21EB89C, FINISHER = 0x21EB8A0;
	static const uint32_t TEX_BASE = 0x21EB8A4, PACKET_CURSOR = 0x21EB8A8;
	static const uint32_t PARITY = 0xC000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE50B78;
	static const uint32_t TEXTURE_NAME = 0xC9DCE0, CAMERA = 0xE50D14, ANIM_SETS = 0xE510FC, BIND_OUT = 0x24FE9D8;
	static const uint32_t MODEL_SPARKLE = 0xE50B7C, SPARKLE_FADE = 0xE50D04, SPARKLE_DEPTH = 0xE50CF4;
	static const uint32_t MODEL_SLASH = 0xC975B4, MODEL_STREAK = 0xC97F9C;
	static const uint32_t SEQ_FLASH = 0xC9702C, SEQ_DEBRIS = 0xC972FC, SEQ_SPARK = 0xC971D8, SPARK_FRAMES = 0xC9DBD0;
	static const uint32_t SLASH_REF = 0xC9DC30, HIT_LEAD = 0xC9DC40, HIT_OFFS = 0xC9DC70;
	static const uint32_t SEQ_EMIT = 0xC96E80;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x57BBE0, ORIG_Setup = 0x57BC00;
	static const uint32_t ORIG_Controller = 0x57BD20, ORIG_HitSpark = 0x57C390, ORIG_Sparkle = 0x57C620,
		ORIG_Trail = 0x57C950, ORIG_SparkSpawn = 0x57CED0, ORIG_Hit = 0x57D000, ORIG_Slash = 0x57D3D0,
		ORIG_RingA = 0x57D6B0, ORIG_CasterModel = 0x57D840, ORIG_RingB = 0x57D980, ORIG_AngleModel = 0x57DB10,
		ORIG_BoneEmitter = 0x57DC60, ORIG_NodeEmitter = 0x57DF60, ORIG_Master = 0x57E340;

#include "renzo_boss_defs.inc"
	// the game's data: the defaults of its parameters (renzo_boss_defs.inc's RenzoBossParams)
	static const RenzoBossParams GAME = { 0xE50B78, { SEQ_FLASH, SEQ_DEBRIS, SEQ_SPARK, SEQ_EMIT }, { MODEL_SPARKLE, MODEL_SLASH, MODEL_STREAK, { 0xC99D2C, 0xC98C04, 0xC9B9EC, 0xC9CF24 } }, { TEXTURE_NAME, CAMERA, ANIM_SETS }, { HIT_LEAD, HIT_OFFS, SPARKLE_FADE, SPARKLE_DEPTH } };
}
}

#ifdef FF8_FX_HELD
#include "mag340_renzokuken_ultimecia_held.h"
#endif

namespace ff8fx
{
namespace renzo340
{
#define RENZO_ID 340
#define RENZO_IDS "340"
#define RENZO_CUSTOM_START
	static void Events(Node24 *n);
	static void Start(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_CUSTOM_START
#undef RENZO_IDS
#undef RENZO_ID

	static const SlotModelDef SLOTMODELS[2] = {
		{ ORIG_RingA, 0xC99D2C, 0xC9DC30, 3, 6, 409, 0x10, false },
		{ ORIG_RingB, 0xC98C04, 0xC9DCD0, 3, 6, 409, 0x10, false },
	};
	static const NodeModelDef NODEMODELS[2] = {
		{ ORIG_CasterModel, 0xC9B9EC, NM_CASTER, 6, 409, 4, 0x10, false, false },
		{ ORIG_AngleModel, 0xC9CF24, NM_YANGLE, 8, 409, 4, 0x12, false, true },
	};
	static const BoneEmitDef BONE_EMIT = { 0xE, 0x1D, 0, 0x880, 0x700, 0x64, 0x19, SEQ_EMIT };

	// StartBoss (0x57E2F0): the boss's sequence 1
	static void StartBoss(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		(void)caster; (void)out; (void)sets; (void)seq;
		QueueChainTransformation(boss, 1);
	}

	// Bind2 (0x57E300): the container's two com-data blocks bound to the caster and to the boss; only the caster's
	// sequence `seq` started
	static void Bind2(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		const uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		QueueChainTransformation(caster, seq);
	}

	static void Start(Node24 *n)
	{
		if (n->c == 1) StartBoss(Caster(), Target(), BIND_OUT, AnimSets(), (Finisher() != 0) + 1);
		if (n->c == 2) Bind2(Caster(), Target(), BIND_OUT, AnimSets(), (Finisher() != 0) + 1);
		if (n->c == 3) ComputeLength();
		if (n->c == 2) StartTasks();
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x1C)
		{
			Node24 *t = AddSlotTask(ORIG_RingA);
			const int16_t s = (int16_t)((CrtRand() % 0x900 + 0x1400) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x23)
		{
			// at the hit point + (600, 100, -400) through the caster's matrix
			Node24 *t = AddSlotTask(ORIG_RingA);
			t->w10 = 0x258;
			t->w12 = 0x64;
			t->w14 = -0x190;
			MatVec((const Mat4x3 *)(Caster() + 0x40), &t->w10, &t->w10);
			const int16_t s = (int16_t)((CrtRand() % 0x900 + 0x1400) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
			SlotRec *r = Slot(t->e);
			r->pos[0] = (int16_t)(r->pos[0] + t->w10);
			r->pos[1] = (int16_t)(r->pos[1] + t->w12);
			r->pos[2] = (int16_t)(r->pos[2] + t->w14);
		}
		if (n->c == 0x3B)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_CasterModel));
			t->c = 0;
			GetEffectSpawnPosition(Target(), 0x1D, 0, &t->w10);
			const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x2000) / 3);
			t->w1E = s;
			t->w1C = s;
		}
		if (n->c == 0x43)
		{
			Node24 *t = AddSlotTask(ORIG_RingB);
			const int16_t s = (int16_t)((CrtRand() % 0x500 + 0xD00) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x44)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_AngleModel));
			t->c = 0;
			t->w10 = var<int16_t>(own::R(HIT_POS));
			t->w12 = 0x226;
			t->w14 = var<int16_t>(own::R(HIT_POS) + 4);
			t->w18 = (int16_t)(CrtRand() % 0x1000);
			t->w1C = 0x3200;
			const int16_t sy = (int16_t)((CrtRand() % 0xC00 + 0x1400) / 3);
			t->w22 = sy;
			t->w20 = sy;
		}
		if (n->c == 0x3B) ((Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_BoneEmitter)))->c = 0;
		if (n->c == 0x43)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_NodeEmitter));
			memcpy(&t->w10, (const void *)own::R(HIT_POS), 8);
			t->w12 = (int16_t)(t->w12 - 0x12C);
			t->c = 0;
		}
	}

	// ------------------------------------------------------------------
	// NodeEmitter (0x57DF60): kind-4 particles at the node's point
	// ------------------------------------------------------------------
	static uint32_t __cdecl NodeEmitterTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= 0x10)
		{
			w->dir[0] = CrtRand() % 0x200 - 0x100;
			w->dir[1] = -0x900 - CrtRand() % 0x500;
			w->dir[2] = CrtRand() % 0x600 + 0x400;
			Normalize(w->dir, w->dir);
			TransformVectorBy3x3Matrix((const Mat4x3 *)(Caster() + 0x40), w->dir, w->dir);
			memcpy(w->pos, &n->w10, 8);
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = 4;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x880 + 0x700);
				memcpy(p->pos, w->pos, 8);
				const int32_t sp = CrtRand() % 0x8C + 0x28;
				const int32_t rx = CrtRand() % 0x28;
				p->w10 = (int16_t)(rx + (mul32(w->dir[0], sp) >> 12) - 0x14);
				p->w12 = (int16_t)(mul32(w->dir[1], sp) >> 12);
				p->w14 = (int16_t)(mul32(w->dir[2], sp) >> 12);
			}
		}
		const int32_t alive = EmitterDraw(n, w, 4, SEQ_EMIT);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl RingATask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl RingBTask(TaskNode *tn) { return SlotModelTask(SLOTMODELS[1], 1, (Node24 *)tn); }
	static uint32_t __cdecl CasterModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl AngleModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[1], 1, (Node24 *)tn); }
	static uint32_t __cdecl BoneEmitterTask(TaskNode *tn) { return BoneEmitter(BONE_EMIT, (Node24 *)tn); }
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "controller", "Controller", "Squall's Renzokuken on Ultimecia (final form): both animation sets and sequences, the gunblade's trail, the sparks, the hits' slashes with their flashes and debris, the rings, the caster model at her bone and its particles, the particles at the hit point, the turned model, the screen flash and tint, the sound" },
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
			{ "controller", "models.ring_a", "Ring a model", "The prim model ring a (a mesh of 97 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 0), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.ring_b", "Ring b model", "The prim model ring b (a mesh of 112 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 4), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.caster", "Caster model", "The prim model caster (a mesh of 130 vertices, 128 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 8), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.turned", "Turned model", "The prim model turned (a mesh of 112 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 12), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(RenzoBossParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "controller", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(RenzoBossParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "controller", "res.anim_sets", "Animation sets", "The animation sets it binds to Squall and to the boss: body animations, sequence scripts, weapon animations ({count, offsets, end}, the game's count)", (uint32_t)offsetof(RenzoBossParams, res.anim_sets), fxp::AnimSet, 0, 0x7FFFFFFF },
			{ "controller", "tables.hit_lead", "Hit leads", "Each hit's lead (how far ahead of its slash) (48 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.hit_lead), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.hit_offs", "Hit offsets", "Where each hit lands from the caster (x, y, z) (96 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.hit_offs), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_fade", "Sparkle fades", "The finisher sparkles' fade by tick (8 ticks) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_fade), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_depth", "Sparkle depths", "The finisher sparkles' depth word by frame (8 frames) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_depth), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag340_renzokuken_ultimecia()
	{
		fxp::register_params({ 340, "Renzokuken (vs Ultimecia)", &renzo340::live, &renzo340::GAME, sizeof(renzo340::RenzoBossParams),
		                       renzo340::table::PARTS, (int)(sizeof(renzo340::table::PARTS) / sizeof(renzo340::table::PARTS[0])),
		                       renzo340::table::FIELDS, (int)(sizeof(renzo340::table::FIELDS) / sizeof(renzo340::table::FIELDS[0])),
		                       nullptr, 0 });
		renzo340::RegisterCommon();
		renzo340::Port(renzo340::ORIG_RingA, (void *)renzo340::RingATask, "340 RingA");
		renzo340::Port(renzo340::ORIG_CasterModel, (void *)renzo340::CasterModelTask, "340 CasterModel");
		renzo340::Port(renzo340::ORIG_RingB, (void *)renzo340::RingBTask, "340 RingB");
		renzo340::Port(renzo340::ORIG_AngleModel, (void *)renzo340::AngleModelTask, "340 AngleModel");
		renzo340::Port(renzo340::ORIG_BoneEmitter, (void *)renzo340::BoneEmitterTask, "340 BoneEmitter");
		renzo340::Port(renzo340::ORIG_NodeEmitter, (void *)renzo340::NodeEmitterTask, "340 NodeEmitter");
		// 30 fps layer: see mag340_renzokuken_ultimecia_held.inc
		FX_HELD(register_mag340_held();)
	}
	FX_LAB_MODULE(register_mag340_renzokuken_ultimecia);
}

#ifdef FF8_FX_HELD
#include "mag340_renzokuken_ultimecia_held.inc"
#endif
