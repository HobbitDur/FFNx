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
// Effect 342: Renzokuken vs Ultimecia + Griever (MAG_342_UNKNOWN in IDA). Squall's Renzokuken command takes the effect id
// from the TARGET's info +0x150 table (computeCommandAction 0x48DE0B): c0m124 (Ultimecia + Griever) has 342. The common part
// of the boss Renzokuken modules (renzo_boss_defs.inc / renzo_boss.inc) and its own events:
//   hits 1..6: a hit emitter each (HitEmitter 0x5790D0, started by the hit scheduler with the hit's index): ticks 0..0xE,
//     2 particles per tick of kind = the hit index at the hit record's point (+- 0x46 random), along its direction
//     (speed 0x19..0xC2, + 0..0x4F - 0x28 / 0..0x3B - 0x1E / 0..0x4F - 0x28), scale 0x700..0x107F; drawn every tick
//     (sequence 0xC89200, EmitterDraw with gravity 5 on y); ends on the first tick with none left;
//   tick 0x16: a facing model (FacingA 0x579440, model 0xC8AF84: turned (-0x400, the caster's facing +0xE, 0) at its
//     record's point = the hit point, uniform scale (rand % 0x400 + 0x900) / 3, render mode 3, 16 ticks, fading from tick
//     6 (step 409));
//   ticks 0x69 / 0x6A: facing models (FacingB 0x5795C0: (+0x400, the caster's facing, 0), render mode 2, lasting the node
//     +0x1A ticks (0x14 / 0x16), fading from 12 ticks before its end (step 341)) at the hit point - 100 / - 800 along the
//     caster's forward direction ((0, 0, -0x1000) 0xC8FFB0 through its rotation), uniform scale (rand % 0x300 + 0xA00) / 3
//     / (rand % 0x580 + 0xD00) / 3.
//   Facing models: rate - rate / 4; their SLOTS record freed at the end. The length is the stages + 35 ticks.
// File loader 0x5776B0 (IO_GetFile_MAGIC of the name at 0xC90060, kept at 0x21E5B38); setup MAG_342 0x5776D0 (not a task:
// context 0x21E5B30, caster 0x21E5B34, first target 0x21E2E28, master 0x5797F0 in its pool 0x21E3B20 (queue 0x21E3B30),
// controller 0x5777F0 in the main pool 0x21E4D08 (100 x 0x24, queue 0x21E4CF8), length 0x21E5BCC = 4, finisher flag
// 0x21E5BD0, pools A / C / B and SLOTS freed, camera animation 0xE47144, TIM upload).
// ENTITY WRITES: as every Renzokuken (caster +0 bit 0x800, +0x28 tint, both binds, chain starts, the hit-effect AnimSeq
// task); no position write. CAMERA: the setup's camera animation only.
// Module globals 0x21E2DA8..0x21E5C20: gathered points 0x21E2DA8 / 0x21E2DE8, target 0x21E2E28, pool A 0x21E2E30, pool B
// 0x21E3010, hit records 0x21E33D0, master pool / queue 0x21E3B20 / 0x21E3B30, SLOTS 0x21E3B40 (100 x 0x20), pool C
// 0x21E47C0, splines 0x21E4AE0 / 0x21E4BE0, hit point / direction 0x21E4CE0 / 0x21E4CE8, main queue / pool 0x21E4CF8 /
// 0x21E4D08, previous hit point / direction 0x21E5B18, context 0x21E5B30, caster 0x21E5B34, file 0x21E5B38, trail ring
// 0x21E5B40, length 0x21E5BCC, finisher 0x21E5BD0, texture base 0x21E5BD4, packet cursor 0x21E5BD8; hit offsets 0xC8FFF0
// (exe data, zero).

#include "mag_common.h"

namespace ff8fx
{
namespace renzo342
{
	// module globals
	static const uint32_t PTS_A = 0x21E2DA8, PTS_B = 0x21E2DE8, TARGET_SLOT = 0x21E2E28;
	static const uint32_t POOL_A = 0x21E2E30, POOL_A_END = 0x21E3010;
	static const uint32_t POOL_B = 0x21E3010, POOL_B_SPAWN_END = 0x21E3290, POOL_B_END = 0x21E33D0;
	static const uint32_t HIT_REC = 0x21E33D0, MASTER_POOL = 0x21E3B20, Q_MASTER = 0x21E3B30;
	static const uint32_t SLOTS = 0x21E3B40, SLOTS_END = 0x21E47C0;
	static const uint32_t POOL_C = 0x21E47C0, POOL_C_END = 0x21E4AE0;
	static const uint32_t SPL_A = 0x21E4AE0, SPL_B = 0x21E4BE0;
	static const uint32_t HIT_POS = 0x21E4CE0, HIT_DIR = 0x21E4CE8, Q_MAIN = 0x21E4CF8, MAIN_POOL = 0x21E4D08;
	static const uint32_t PREV_HIT = 0x21E5B18, CONTEXT = 0x21E5B30, CASTER_SLOT = 0x21E5B34, TEX_FILE = 0x21E5B38;
	static const uint32_t RING = 0x21E5B40, RING_END = 0x21E5BCC, LENGTH = 0x21E5BCC, FINISHER = 0x21E5BD0;
	static const uint32_t TEX_BASE = 0x21E5BD4, PACKET_CURSOR = 0x21E5BD8;
	static const uint32_t PARITY = 0xC000;
	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xE46FA8;
	static const uint32_t TEXTURE_NAME = 0xC90060, CAMERA = 0xE47144, ANIM_SETS = 0xE476F4, BIND_OUT = 0x24FE978;
	static const uint32_t MODEL_SPARKLE = 0xE46FAC, SPARKLE_FADE = 0xE47134, SPARKLE_DEPTH = 0xE47124;
	static const uint32_t MODEL_SLASH = 0xC89934, MODEL_STREAK = 0xC8A31C;
	static const uint32_t SEQ_FLASH = 0xC893AC, SEQ_DEBRIS = 0xC8967C, SEQ_SPARK = 0xC89558, SPARK_FRAMES = 0xC8FF50;
	static const uint32_t SLASH_REF = 0xC8FFB0, HIT_LEAD = 0xC8FFC0, HIT_OFFS = 0xC8FFF0;
	static const uint32_t SEQ_HIT = 0xC89200, MODEL_FACING = 0xC8AF84;
	// tasks
	static const uint32_t ORIG_FileLoader = 0x5776B0, ORIG_Setup = 0x5776D0;
	static const uint32_t ORIG_Controller = 0x5777F0, ORIG_HitSpark = 0x577D90, ORIG_Sparkle = 0x578020,
		ORIG_Trail = 0x578350, ORIG_SparkSpawn = 0x5788D0, ORIG_Hit = 0x578A00, ORIG_Slash = 0x578DF0,
		ORIG_HitEmitter = 0x5790D0, ORIG_FacingA = 0x579440, ORIG_FacingB = 0x5795C0, ORIG_Master = 0x5797F0;

#include "renzo_boss_defs.inc"
}
}

#ifdef FF8_FX_HELD
#include "mag342_renzokuken_ultimecia_griever_held.h"
#endif

namespace ff8fx
{
namespace renzo342
{
#define RENZO_ID 342
#define RENZO_IDS "342"
#define RENZO_HIT_EXTRA
#define RENZO_LENGTH_EXTRA
	// the length: the stages + 35 ticks
	static int32_t LengthExtra() { return 0x23; }
	static void Events(Node24 *n);
	static void HitExtra(int32_t b);
#include "renzo_boss.inc"
#undef RENZO_HIT_EXTRA
#undef RENZO_LENGTH_EXTRA
#undef RENZO_IDS
#undef RENZO_ID

	// hits 1..6: a hit emitter with the hit's index (the hit scheduler, after the hit's debris)
	static void HitExtra(int32_t b)
	{
		if (b <= 0 || b >= 7) return;
		Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_HitEmitter);
		t->c = 0;
		t->e = (int16_t)b;
	}

	// a facing model at the hit point - k / 0x1000 along the caster's forward direction (its record's point)
	static void FacingAhead(Node24 *t, int32_t k)
	{
		int32_t v[3];
		TransformVectorBy3x3Matrix((const Mat4x3 *)(Caster() + 0x40), (const int32_t *)SLASH_REF, v);
		SlotRec *r = Slot(t->e);
		r->pos[0] = (int16_t)((mul32(v[0], -k) >> 12) + var<int32_t>(HIT_POS));
		r->pos[1] = (int16_t)((mul32(v[1], -k) >> 12) + var<int16_t>(HIT_POS + 2));
		r->pos[2] = (int16_t)((mul32(v[2], -k) >> 12) + var<int32_t>(HIT_POS + 4));
		r->w18 = var<int16_t>(HIT_DIR);
		r->w1A = var<int16_t>(HIT_DIR + 4);
		r->w1C = var<int16_t>(HIT_DIR + 8);
	}

	static void Events(Node24 *n)
	{
		if (n->c == 0x16)
		{
			Node24 *t = AddSlotTask(ORIG_FacingA);
			const int16_t s = (int16_t)((CrtRand() % 0x400 + 0x900) / 3);
			t->w1E = s;
			t->w1C = s;
			HitPointRecord(t);
		}
		if (n->c == 0x69)
		{
			Node24 *t = AddSlotTask(ORIG_FacingB);
			t->w1A = 0x14;
			const int16_t s = (int16_t)((CrtRand() % 0x300 + 0xA00) / 3);
			t->w1E = s;
			t->w1C = s;
			FacingAhead(t, 100);
		}
		if (n->c == 0x6A)
		{
			Node24 *t = AddSlotTask(ORIG_FacingB);
			t->w1A = 0x16;
			const int16_t s = (int16_t)((CrtRand() % 0x580 + 0xD00) / 3);
			t->w1E = s;
			t->w1C = s;
			FacingAhead(t, 800);
		}
	}

	// ------------------------------------------------------------------
	// HitEmitter (0x5790D0): particles of kind = the hit index from its hit record
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitEmitterTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		EmitWork *w = (EmitWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= 0xE)
		{
			const HitRec *hr = (const HitRec *)(HIT_REC + 0x9C * n->e);
			memcpy(w->dir, hr->dir, 16);
			memcpy(w->pos, hr->pos, 8);
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FreeRecord();
				if (i < 0) break;
				SlotRec *p = Slot(i);
				p->alive = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x980 + 0x700);
				memcpy(p->pos, w->pos, 8);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x8C - 0x46));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x8C - 0x46));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x8C - 0x46));
				const int32_t sp = CrtRand() % 0xAA + 0x19;
				const int32_t rx = CrtRand() % 0x50;
				p->w10 = (int16_t)(rx + (mul32(w->dir[0], sp) >> 12) - 0x28);
				const int32_t ry = CrtRand() % 0x3C;
				p->w12 = (int16_t)(ry + (mul32(w->dir[1], sp) >> 12) - 0x1E);
				const int32_t rz = CrtRand() % 0x50;
				p->w14 = (int16_t)(rz + (mul32(w->dir[2], sp) >> 12) - 0x28);
			}
		}
		const int32_t alive = EmitterDraw(n, w, n->e, SEQ_HIT, 5);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Facing models (0x579440 / 0x5795C0): turned (x angle, the caster's facing, 0) at their record's point
	// ------------------------------------------------------------------
	struct FacingDef { uint32_t orig; int16_t ax; int32_t mode, fade_mul; bool node_ticks; };
	static const FacingDef FACINGS[2] = {
		{ ORIG_FacingA, -0x400, 3, 409, false }, // 16 ticks, fading from tick 6
		{ ORIG_FacingB, 0x400, 2, 341, true },   // node +0x1A ticks, fading from 12 before the end
	};
	static int32_t FacingFrom(const FacingDef &d, const Node24 *n) { return d.node_ticks ? n->w1A - 0xC : 6; }
	static int32_t FacingTicks(const FacingDef &d, const Node24 *n) { return d.node_ticks ? n->w1A : 0x10; }
	static int32_t FacingFade(const FacingDef &d, const Node24 *n, int32_t c) { return mul32(c - FacingFrom(d, n), d.fade_mul); }

	static void FacingDraw(const FacingDef &d, const Node24 *n, const SlotRec *r, int32_t scale, int32_t fade)
	{
		int16_t ang[3];
		ang[0] = d.ax;
		ang[2] = 0;
		ang[1] = *(const int16_t *)(Caster() + 0xE);
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only)
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = r->pos[1];
		m.t[0] = r->pos[0];
		int32_t sv[3];
		sv[2] = scale;
		sv[1] = scale;
		sv[0] = scale;
		m.t[2] = r->pos[2];
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_FACING;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (n->c >= FacingFrom(d, n))
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), d.mode, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t FacingTask(int kind, Node24 *n)
	{
		const FacingDef &d = FACINGS[kind];
		// 30 fps layer: see mag342_renzokuken_ultimecia_griever_held.inc
		FX_HELD(held_note_facing(kind, n);)
		FacingDraw(d, n, Slot(n->e), n->w1C, FacingFade(d, n, n->c));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - (int16_t)(rate / 4));
		n->c++;
		if (n->c < FacingTicks(d, n)) return 0;
		Slot(n->e)->alive = 0;
		return TASK_END;
	}
	static uint32_t __cdecl FacingATask(TaskNode *tn) { return FacingTask(0, (Node24 *)tn); }
	static uint32_t __cdecl FacingBTask(TaskNode *tn) { return FacingTask(1, (Node24 *)tn); }
}

	void register_mag342_renzokuken_ultimecia_griever()
	{
		renzo342::RegisterCommon();
		register_port(renzo342::ORIG_HitEmitter, (void *)renzo342::HitEmitterTask, "342 HitEmitter", 342);
		register_port(renzo342::ORIG_FacingA, (void *)renzo342::FacingATask, "342 FacingA", 342);
		register_port(renzo342::ORIG_FacingB, (void *)renzo342::FacingBTask, "342 FacingB", 342);
		// 30 fps layer: see mag342_renzokuken_ultimecia_griever_held.inc
		FX_HELD(register_mag342_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag342_renzokuken_ultimecia_griever_held.inc"
#endif
