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
namespace renzo343
{
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
	static const void *const SOUND = (const void *)0xE4214C;
	static const uint32_t TEXTURE_NAME = 0xC891F0, CAMERA = 0xE422E8, ANIM_SETS = 0xE426C0, BIND_OUT = 0x24FE948;
	static const uint32_t MODEL_SPARKLE = 0xE42150, SPARKLE_FADE = 0xE422D8, SPARKLE_DEPTH = 0xE422C8;
	static const uint32_t MODEL_SLASH = 0xC82B34, MODEL_STREAK = 0xC8351C;
	static const uint32_t SEQ_FLASH = 0xC825AC, SEQ_DEBRIS = 0xC8287C, SEQ_SPARK = 0xC82758, SPARK_FRAMES = 0xC89150;
	static const uint32_t SLASH_REF = 0xC891B0, HIT_LEAD = 0xC891C0;
	static const uint32_t SEQ_EMIT = 0xC82400, MODEL_FINAL = 0xC852AC;
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
			ff8fx::copy<(8)>(&t->w10, (const void *)HIT_POS);
			t->c = 0;
			const int16_t s = (int16_t)((CrtRand() % 0xC00 + 0x1C00) / 3);
			t->w1E = s;
			t->w1C = s;
			ff8fx::copy<(0x20)>((void *)SAVED_MAT, Caster() + 0x40);
		}
	}

	static uint32_t __cdecl EmitterTask(TaskNode *tn) { return RecordEmitter(tn, 0xE, SEQ_EMIT); }

	static void FinalDraw(const Node24 *n, int32_t scale, int32_t fade)
	{
		int16_t ang[3] = { 0, 0, 0x400 };
		Mat4x3 m;
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
		FX_HELD(register_mag343_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag343_renzokuken_griever_held.inc"
#endif
