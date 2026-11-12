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
// Effect 334: Renzokuken vs Bahamut (MAG_334_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id from the
// TARGET's info +0x150 table (computeCommandAction 0x48DE0B): Bahamut c0m067 has 334. The common part of the boss Renzokuken
// modules (renzo_boss_defs.inc / renzo_boss.inc) and its own events:
//   tick 0x39: the ring pool (RingPool 0x588FC0);
//   tick 0x65: a turned model with its own SLOTS record (AngleModel 0x5892B0, model 0xCBB374 at the hit point, y angle
//     rand % 0x1000, uniform scale (rand % 0x300 + 0xA00) / 3, rate - rate / 4, 14 ticks, fading from tick 6 (step 512))
//     and a bone emitter (BoneEmitter 0x589400: the boss's effect bone 0xF0, until tick 0x20, scale 0x800..0xDFF, speed
//     0x1E..0x77, sequence 0xCB95F0);
//   tick 0x8E: a caster model (CasterModel 0x589710, model 0xCBE15C: the caster's matrix at the hit point (+ a zero offset
//     through it), uniform scale (rand % 0x800 + 0x2400) / 3, rate - rate / 4, 16 ticks, fading from tick 4 (step 341)).
//   RingPool (0x588FC0) - ticks 0..8: two rings per tick in SLOTS (alive 1): at the hit point and half way between the
//     previous tick's hit point and this one (the second one in the record after the first, taken or not), y + 600,
//     scale 0x800 (+0x12) growing by +0x16 (0x800, - rate / 4), facing the hit direction / the previous one flattened
//     (x, 0, z; VectorNormalS16); every tick it draws every SLOTS record whose alive word has bit 0 (also the slot tasks'
//     -1 records): model 0xCBD48C turned from 0xCC03E0 onto (x, 0, z) of its +0x18 / +0x1C, at its point, scaled by
//     (that vector's x, its scale, its z), fading from life 4 (step 512), freed at life 12; ends on the first tick
//     with none left.
// File loader 0x587770 (IO_GetFile_MAGIC of the name at 0xCC0400, kept at 0x21F9FE0); setup MAG_334 0x587790 (not a task:
// context 0x21F9FD8, caster 0x21F9FDC, first target 0x21F72D0, master 0x589940 in its pool 0x21F7FC8 (queue 0x21F7FD8),
// controller 0x5878B0 in the main pool 0x21F91B0 (100 x 0x24, queue 0x21F91A0), length 0x21FA074 = 4, finisher flag
// 0x21FA078, pools A / C / B and SLOTS freed, camera animation 0xE70EF4, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task); no position write. CAMERA: the setup's camera animation only.
// Module globals 0x21F7250..0x21FA0C8: gathered points 0x21F7250 / 0x21F7290, target 0x21F72D0, pool A 0x21F72D8, pool B
// 0x21F74B8, hit records 0x21F7878, master pool / queue 0x21F7FC8 / 0x21F7FD8, SLOTS 0x21F7FE8 (100 x 0x20), pool C
// 0x21F8C68, splines 0x21F8F88 / 0x21F9088, hit point / direction 0x21F9188 / 0x21F9190, main queue / pool 0x21F91A0 /
// 0x21F91B0, previous hit point / direction 0x21F9FC0, context 0x21F9FD8, caster 0x21F9FDC, file 0x21F9FE0, trail ring
// 0x21F9FE8, length 0x21FA074, finisher 0x21FA078, texture base 0x21FA07C, packet cursor 0x21FA080, hit offsets 0x21FA088
// (8 x 8 bytes, zero).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace renzo334
{
	// module globals
	static const uint32_t PTS_A = 0x21F7250, PTS_B = 0x21F7290, TARGET_SLOT = 0x21F72D0;
	static const uint32_t POOL_A = 0x21F72D8, POOL_A_END = 0x21F74B8;
	static const uint32_t POOL_B = 0x21F74B8, POOL_B_SPAWN_END = 0x21F7738, POOL_B_END = 0x21F7878;
	static const uint32_t HIT_REC = 0x21F7878, MASTER_POOL = 0x21F7FC8, Q_MASTER = 0x21F7FD8;
	static const uint32_t SLOTS = 0x21F7FE8, SLOTS_END = 0x21F8C68;
	static const uint32_t POOL_C = 0x21F8C68, POOL_C_END = 0x21F8F88;
	static const uint32_t SPL_A = 0x21F8F88, SPL_B = 0x21F9088;
	static const uint32_t HIT_POS = 0x21F9188, HIT_DIR = 0x21F9190, Q_MAIN = 0x21F91A0, MAIN_POOL = 0x21F91B0;
	static const uint32_t PREV_HIT = 0x21F9FC0, CONTEXT = 0x21F9FD8, CASTER_SLOT = 0x21F9FDC, TEX_FILE = 0x21F9FE0;
	static const uint32_t RING = 0x21F9FE8, RING_END = 0x21FA074, LENGTH = 0x21FA074, FINISHER = 0x21FA078;
	static const uint32_t TEX_BASE = 0x21FA07C, PACKET_CURSOR = 0x21FA080, HIT_OFFS = 0x21FA088;
	static const uint32_t PARITY = 0x8000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE70D58;
	static const uint32_t TEXTURE_NAME = 0xCC0400, CAMERA = 0xE70EF4, ANIM_SETS = 0xE71854, BIND_OUT = 0x24FEAD8;
	static const uint32_t MODEL_SPARKLE = 0xE70D5C, SPARKLE_FADE = 0xE70EE4, SPARKLE_DEPTH = 0xE70ED4;
	static const uint32_t MODEL_SLASH = 0xCB9D24, MODEL_STREAK = 0xCBA70C;
	static const uint32_t SEQ_FLASH = 0xCB979C, SEQ_DEBRIS = 0xCB9A6C, SEQ_SPARK = 0xCB9948, SPARK_FRAMES = 0xCC0340;
	static const uint32_t SLASH_REF = 0xCC03A0, HIT_LEAD = 0xCC03B0;
	static const uint32_t MODEL_POOL_RING = 0xCBD48C, POOL_RING_REF = 0xCC03E0;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x587770, ORIG_Setup = 0x587790;
	static const uint32_t ORIG_Controller = 0x5878B0, ORIG_HitSpark = 0x587CA0, ORIG_Sparkle = 0x587F30,
		ORIG_Trail = 0x588260, ORIG_SparkSpawn = 0x5887E0, ORIG_Hit = 0x588910, ORIG_Slash = 0x588CE0,
		ORIG_RingPool = 0x588FC0, ORIG_AngleModel = 0x5892B0, ORIG_BoneEmitter = 0x589400, ORIG_CasterModel = 0x589710,
		ORIG_Master = 0x589940;

#include "renzo_boss_defs.inc"
	// the game's data: the defaults of its parameters (renzo_boss_defs.inc's RenzoBossParams)
	static const RenzoBossParams GAME = { 0xE70D58, { SEQ_FLASH, SEQ_DEBRIS, SEQ_SPARK, 0xCB95F0 }, { MODEL_SPARKLE, MODEL_SLASH, MODEL_STREAK, { MODEL_POOL_RING, 0xCBB374, 0xCBE15C, 0 } }, { TEXTURE_NAME, CAMERA, ANIM_SETS }, { HIT_LEAD, 0, SPARKLE_FADE, SPARKLE_DEPTH } };
}
}

#ifdef FF8_FX_HELD
#include "mag334_renzokuken_bahamut_held.h"
#endif

namespace ff8fx
{
namespace renzo334
{
#define RENZO_ID 334
#define RENZO_IDS "334"
	static void Events(Node24 *n);
#include "renzo_boss.inc"
#undef RENZO_IDS
#undef RENZO_ID

	static const NodeModelDef NODEMODELS[2] = {
		{ ORIG_AngleModel, 0xCBB374, NM_YANGLE, 6, 512, 4, 0xE, true },
		{ ORIG_CasterModel, 0xCBE15C, NM_CASTER_OFS, 4, 341, 4, 0x10, false },
	};
	static const BoneEmitDef BONE_EMIT = { 0x20, 0xF0, 0, 0x600, 0x800, 0x5A, 0x1E, 0xCB95F0 };

	static void Events(Node24 *n)
	{
		if (n->c == 0x39) ((Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_RingPool)))->c = 0;
		if (n->c == 0x65)
		{
			Node24 *t = AddSlotTask(ORIG_AngleModel);
			memcpy(&t->w10, (const void *)own::R(HIT_POS), 8);
			t->w18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t s = (int16_t)((CrtRand() % 0x300 + 0xA00) / 3);
			t->w1E = s;
			t->w1C = s;
			((Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_BoneEmitter)))->c = 0;
		}
		if (n->c == 0x8E)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), own::Fn(ORIG_CasterModel));
			memcpy(&t->w10, (const void *)own::R(HIT_POS), 8);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x2400) / 3);
			t->w1E = s;
			t->w1C = s;
		}
	}

	// ------------------------------------------------------------------
	// RingPool (0x588FC0): rings in SLOTS at the hit point and half way from the previous one
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct PoolWork    // its Field_Alloc block (0x68 bytes)
	{
		uint8_t pad00[8];
		Mat4x3 cam;        // +0x08 the camera matrix
		Mat4x3 m;          // +0x28 the ring's matrix
		int32_t v[4];      // +0x48 the turn's target vector / the scale vector (x, scale, z)
		int32_t axis[4];   // +0x58
	};
#pragma pack(pop)
	static_assert(sizeof(PoolWork) == 0x68, "Renzokuken 334 ring pool block");

	// a pool ring: alive 1, life 0, at p (y + 600), scale 0x800 growing by 0x800, facing (dx, 0, dz) (VectorNormalS16)
	static void PoolRing(SlotRec *r, const int16_t *p, int16_t dx, int16_t dz)
	{
		r->alive = 1;
		r->life = 0;
		r->pos[0] = p[0];
		r->pos[1] = p[1];
		r->pos[2] = p[2];
		r->pos[3] = p[3];
		r->pos[1] = (int16_t)(r->pos[1] + 600);
		r->w16 = 0x800;
		r->w12 = 0x800;
		r->w18 = dx;
		r->w1A = 0;
		r->w1C = dz;
		NormalizeS16(&r->w18, &r->w18);
	}

	// the draw of one pool ring (the header holds the model) at its scale / life
	static void PoolRingDraw(PoolWork *w, uint8_t *h, const SlotRec *r, int32_t scale, int32_t fade)
	{
		w->v[1] = 0;
		w->v[0] = r->w18;
		w->v[2] = r->w1C;
		const int32_t angle = RotationBetweenVectors((const int32_t *)POOL_RING_REF, w->v, w->axis);
		BuildAxisAngleRotationMatrix(angle, &w->m, w->axis);
		w->m.t[0] = r->pos[0];
		w->m.t[1] = r->pos[1];
		w->v[1] = scale;
		w->m.t[2] = r->pos[2];
		Scale3DMatrix(&w->m, w->v);
		ComposeAffineTransform(&w->cam, &w->m, &w->m);
		GteSetRotMatrix(&w->m);
		GteSetTransVector(&w->m);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (r->life >= 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}
	static int32_t PoolRingFade(int32_t life) { return shl32(life - 4, 9); }

	static uint32_t __cdecl RingPoolTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PoolWork *w = (PoolWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 8)
		{
			const int32_t i = FreeRecord();
			if (i >= 0)
			{
				PoolRing(Slot(i), (const int16_t *)own::R(HIT_POS), var<int16_t>(own::R(HIT_DIR)), var<int16_t>(own::R(HIT_DIR) + 8));
				// the second one in the next record (taken or not), half way from the previous hit point
				int16_t mid[4];
				mid[0] = (int16_t)(((int32_t)var<int16_t>(own::R(PREV_HIT)) + var<int16_t>(own::R(HIT_POS))) >> 1);
				mid[1] = (int16_t)(((int32_t)var<int16_t>(own::R(PREV_HIT) + 2) + var<int16_t>(own::R(HIT_POS) + 2)) >> 1);
				mid[2] = (int16_t)(((int32_t)var<int16_t>(own::R(PREV_HIT) + 4) + var<int16_t>(own::R(HIT_POS) + 4)) >> 1);
				SlotRec *r = Slot(i + 1);
				mid[3] = r->pos[3];
				PoolRing(r, mid, var<int16_t>(own::R(PREV_HIT) + 8), var<int16_t>(own::R(PREV_HIT) + 0x10));
			}
		}
		// 30 fps layer: see mag334_renzokuken_bahamut_held.inc
		FX_HELD(held_note_ringpool(n);)
		uint8_t *h = AllocHeader(0x58);
		memcpy(&w->cam, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = OwnModel(MODEL_POOL_RING);
		*(uint32_t *)(h + 8) = 0;
		int32_t alive = 0;
		w->v[0] = 0;
		w->v[2] = 0x300;
		for (uint32_t a = own::R(SLOTS); a < own::R(SLOTS_END); a += 0x20)
		{
			SlotRec *r = (SlotRec *)a;
			if (!(r->alive & 1)) continue;
			PoolRingDraw(w, h, r, r->w12, PoolRingFade(r->life));
			if (DrawOnly()) continue;
			if (r->life >= 0xC)
			{
				r->alive = 0;
				continue;
			}
			r->life++;
			r->w12 = (int16_t)(r->w12 + r->w16);
			r->w16 = (int16_t)(r->w16 - (int16_t)(r->w16 / 4));
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl AngleModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[0], 0, (Node24 *)tn); }
	static uint32_t __cdecl BoneEmitterTask(TaskNode *tn) { return BoneEmitter(BONE_EMIT, (Node24 *)tn); }
	static uint32_t __cdecl CasterModelTask(TaskNode *tn) { return NodeModelTask(NODEMODELS[1], 1, (Node24 *)tn); }
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "controller", "Controller", "Squall's Renzokuken on Bahamut: both animation sets and sequences, the gunblade's trail, the sparks, the hits' slashes with their flashes and debris, the rings along the trail, the turned model, the particles from Bahamut's bone, the caster model, the screen flash and tint, the sound" },
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
			{ "controller", "models.ring", "Ring model", "The prim model ring (a mesh of 6 vertices, 2 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 0), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.turned", "Turned model", "The prim model turned (a mesh of 112 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 4), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "models.caster", "Caster model", "The prim model caster (a mesh of 130 vertices, 128 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)(offsetof(RenzoBossParams, models.own) + 8), fxp::Model, 0, 0x7FFFFFFF },
			{ "controller", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(RenzoBossParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "controller", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(RenzoBossParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "controller", "res.anim_sets", "Animation sets", "The animation sets it binds to Squall and to the boss: body animations, sequence scripts, weapon animations ({count, offsets, end}, the game's count)", (uint32_t)offsetof(RenzoBossParams, res.anim_sets), fxp::AnimSet, 0, 0x7FFFFFFF },
			{ "controller", "tables.hit_lead", "Hit leads", "Each hit's lead (how far ahead of its slash) (48 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.hit_lead), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_fade", "Sparkle fades", "The finisher sparkles' fade by tick (8 ticks) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_fade), fxp::Data, 0, 0x7FFFFFFF },
			{ "controller", "tables.sparkle_depth", "Sparkle depths", "The finisher sparkles' depth word by frame (8 frames) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(RenzoBossParams, tables.sparkle_depth), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag334_renzokuken_bahamut()
	{
		fxp::register_params({ 334, "Renzokuken (vs Bahamut)", &renzo334::live, &renzo334::GAME, sizeof(renzo334::RenzoBossParams),
		                       renzo334::table::PARTS, (int)(sizeof(renzo334::table::PARTS) / sizeof(renzo334::table::PARTS[0])),
		                       renzo334::table::FIELDS, (int)(sizeof(renzo334::table::FIELDS) / sizeof(renzo334::table::FIELDS[0])),
		                       nullptr, 0 });
		renzo334::RegisterCommon();
		renzo334::Port(renzo334::ORIG_RingPool, (void *)renzo334::RingPoolTask, "334 RingPool");
		renzo334::Port(renzo334::ORIG_AngleModel, (void *)renzo334::AngleModelTask, "334 AngleModel");
		renzo334::Port(renzo334::ORIG_BoneEmitter, (void *)renzo334::BoneEmitterTask, "334 BoneEmitter");
		renzo334::Port(renzo334::ORIG_CasterModel, (void *)renzo334::CasterModelTask, "334 CasterModel");
		// 30 fps layer: see mag334_renzokuken_bahamut_held.inc
		FX_HELD(register_mag334_held();)
	}
	FX_LAB_MODULE(register_mag334_renzokuken_bahamut);
}

#ifdef FF8_FX_HELD
#include "mag334_renzokuken_bahamut_held.inc"
#endif
