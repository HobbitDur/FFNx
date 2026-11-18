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

// Effect 167: Zantetsuken of the monster Odin (enemy attack 274, c0m070; MAG_167_*). A small
// relative of Odin's GF module 187 (mag187_odin.cpp): the same sword sparks, blade trail and target
// slices, driven by the caster's own model instead of a summoned creature.
//
// Structure (file loader 0x5E29F0: "mag166.tim" (name 0xD716CC) into 0x23BEF58; setup 0x5E2A10, not
// ported: it runs once):
//   setup - packet arena = Magic_TextureOFF_ToEAX1 (0x23BF02C); cast context 0x23BEF50, caster slot
//     0x23BEF54, first target slot 0x23BD400; root queue 0x23BD5A8 (pool 0x23BD598, 1 x 0x10) with
//     the master, sub-queue 0x23BE130 (pool 0x23BE140, 100 x 0x24) with the timeline; spark pool
//     0x23BD5B8 cleared; camera animation 0xD6F7A4, the TIM queued.
//   Master (0x5E4E20, au_re_BdlinkTask_20) - packet cursor 0x23BF030 = arena (+0x8000 on odd
//     counters), runs the sub-queue, ++counter, ends when it is empty (no pause test).
//   Timeline (0x5E2AD0), 146 ticks: 27 sword sparks, 28 sound, 44 blade trail, 64 / 68 two screen
//     prims, 64 stage shown + entities flagged (+0 |= 4), 72 screen fade, 74 entities flagged (|= 8)
//     and the caster moved (+0x1E += 700, +0x20 += 0x1130; old words kept in 0x23BE118), 77 stage
//     hidden + flags cleared, 97 one target slice per target, 143 the action results, end: caster
//     sub-object +0x2C |= 0x40.
//   Sparks (0x5E2F30), trail (0x5E33E0), prims (0x5E38D0 / 0x5E3A00), slices (0x5E3C20): below.
// Every task except the master tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when
// set).
// The module-private renderer (clipped model 0x5E4030 with 0x5E4320 / 0x5E48A0..0x5E4D40, a pure
// function of a scratch header, = Odin 187's 0x648ED0) is called through its original address.

#include "fx_port.h"
#include <stddef.h>

namespace ff8fx
{
namespace z167
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b.rot = a.rot * b.rot
		inline void NormalizeVector16(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // VectorNormalS16 (float)
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
		inline void TransformCoordinateToCameraSpace(const int16_t *in, void *out, void *flag) { fn<void (__cdecl *)(const int16_t *, void *, void *)>(0x56C820)(in, out, flag); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline void SplineSetup(int32_t n, uint32_t points, void *work) { fn<void (__cdecl *)(int32_t, uint32_t, void *)>(0x571620)(n, points, work); }             // Effect_SplineSetup
		inline void SplineEval(int32_t n, void *work, uint32_t out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, uint32_t, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
		inline void SetProjection(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); }                                             // Battle_SetProjectionH
		inline uint32_t DrawShadow(void *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, shift, cursor); } // BattleEntity_DrawShadow
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void ScreenFade(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // Effect_SpawnScreenFadeTask
		inline void ApplyActionResultToTargets(uint32_t list, uint32_t count) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x506BA0)(list, count); }
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // GTE_SetBackColorFromTrans: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                 // GTE_SetRotScale: R = s * I
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                                 // GTE_TransFromMAC: TR = MAC123
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }                   // GTE_ReadOTZ
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }  // GTE_SetFarColor
		inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60
		// module-private renderer called through its original address
		inline uint32_t SliceRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5E4030)(geom, h, ot, mode, cursor); } // MAG_167_sub_5E4030
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline TaskQueue &SubQueue() { return var<TaskQueue>(0x23BE130); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x23BEF50); }
	inline uint32_t CasterSlot() { return var<uint32_t>(0x23BEF54); }
	inline uint32_t &Cursor() { return var<uint32_t>(0x23BF030); }        // the module's packet cursor (arena 0x23BF02C)
	inline uint32_t &EngineCursor() { return var<uint32_t>(0x1D8E054); }  // battle_texture_data_ptr_1D8E054 (the target slices)
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint8_t *Actions() { return *(uint8_t **)(CastCtx() + 4); }    // +0x08 -> 0x18-byte target records (byte 0 = slot), +0x10 u8 count
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	static const uint32_t HIST_A = 0x23BD350;   // spline input points (8 bytes each)
	static const uint32_t HIST_B = 0x23BD3A8;   // spline input directions / second rail
	static const uint32_t SPARK_HIST = 0x23BD408; // 3 x 0x14 sword history of the sparks
	static const uint32_t POOL = 0x23BD5B8;     // 100 x 0x18 spark records
	static const uint32_t SPLINE_A = 0x23BDF18; // spline outputs (8 bytes each)
	static const uint32_t SPLINE_B = 0x23BE018;
	static const uint32_t BLADE_P0 = 0x23BE120;  // blade end points (world), written by the trail
	static const uint32_t BLADE_P1 = 0x23BE128;
	static const uint32_t TRAIL_RECS = 0x23BEF60; // 10 x 0x14 blade trail history

	static const uint32_t ORIG_Master = 0x5E4E20;
	static const uint32_t ORIG_TimelineTask = 0x5E2AD0;
	static const uint32_t ORIG_SparkTask = 0x5E2F30;
	static const uint32_t ORIG_TrailTask = 0x5E33E0;
	static const uint32_t ORIG_PrimATask = 0x5E38D0;
	static const uint32_t ORIG_PrimBTask = 0x5E3A00;
	static const uint32_t ORIG_SliceTask = 0x5E3C20;

	// every sub-queue node is 0x24 bytes (pool 0x23BE140); field use differs per task
#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E target slot (slices)
		int16_t p10, p12, p14, p16; // +0x10 position
		int16_t a18;                // +0x18 slice angle
		int16_t w1A;
		int16_t s1C, s1E;           // +0x1C scale / slice offset, its speed
		int16_t s20, s22;           // +0x20 second scale
	};
	// spark record of the pool 0x23BD5B8
	struct Rec
	{
		uint32_t alive;       // +0x00
		int16_t age;          // +0x04 flipbook frame
		int16_t size;         // +0x06
		int16_t x, y, z;      // +0x08
		int16_t w0E;          // +0x0E
		int16_t vx, vy, vz;   // +0x10
		int16_t w16;          // +0x16
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");
	static_assert(sizeof(Rec) == 0x18, "spark record is 0x18 bytes");
}
}

#ifdef FF8_FX_HELD
#include "mag167_zantetsuken_held.h"
#endif

namespace ff8fx
{
namespace z167
{
	inline Rec *RecOf(int i) { return (Rec *)(POOL + 0x18 * i); }

	// ------------------------------------------------------------------
	// Master (0x5E4E20, au_re_BdlinkTask_20): node from pool 0x23BD598, +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl Master(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint32_t arena = var<uint32_t>(0x23BF02C);
		Cursor() = (*(const uint8_t *)&n->c & 1) ? arena + 0x8000 : arena;
		// 30 fps layer: see mag167_zantetsuken_held.inc
		FX_HELD(held_note_master();)
		int left = ExecuteTaskQueue(&SubQueue());
		n->c++;
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Timeline (0x5E2AD0), 146 ticks
	// ------------------------------------------------------------------
	// MAG_167_sub_5E2E20 / 5E2E40: the battle stage groups stru_1D9898C[0..3] +5 bit 1 (hide / show the stage)
	static void HideStage() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2; }
	static void ShowStage() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD; }

	// MAG_167_sub_5E2E60: caster +0 &= ~4, and every party entity with bit 1
	static void ClearFlag4()
	{
		var<uint16_t>((uint32_t)Caster()) &= 0xFFFB;
		for (uint32_t a = 0x1D972C0; a < 0x1D97494; a += 0x9C)
		{
			uint16_t f = var<uint16_t>(a);
			if (f & 2) var<uint16_t>(a) = (uint16_t)(f & 0xFFFB);
		}
	}

	// MAG_167_sub_5E2EB0 / 5E2EF0: caster +0 |= bit, and every party entity with bit 1
	static void SetFlag(uint8_t bit)
	{
		Caster()[0] |= bit;
		for (uint32_t a = 0x1D972C0; a < 0x1D97494; a += 0x9C)
		{
			uint16_t f = var<uint16_t>(a);
			if (f & 2) var<uint16_t>(a) = (uint16_t)(f | bit);
		}
	}

	// MAG_167_sub_5E3B20 (= GF_327_BonePosition 0x58E2E0): the bone's position (camera space of its
	// matrix: a point at bone length x frac along the bone); hidden model (+0 bit 1 clear): the entity
	// position, returns -1
	static int32_t BonePosition(uint8_t *ent, int32_t bone, int32_t frac, int16_t *out)
	{
		int16_t v[3] = { 0, 0, 0 };
		if (!(ent[0] & 2))
		{
			out[0] = *(int16_t *)(ent + 0x1C);
			out[1] = *(int16_t *)(ent + 0x1E);
			out[2] = *(int16_t *)(ent + 0x20);
			return -1;
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x34);
		uint8_t *sk = **(uint8_t ***)(ent + 0x64);
		uint8_t *bones = sk + 0x10;
		int32_t f = frac;
		if (bone >= 0xF0)
		{
			uint32_t b = sk[bone - 0xEC];
			if (b & 0x80)
			{
				f = 0x1000;
				bone = (int32_t)(b & 0x7F);
			}
			else bone = (int32_t)b;
		}
		uint8_t *rec = bones + 0x30 * bone;
		if (*(int16_t *)rec >= 0) v[2] = (int16_t)(mul32(*(int16_t *)(rec + 2), f) >> 12);
		else v[2] = 0;
		GteSetRotMatrix((const Mat4x3 *)(rec + 0x10));
		GteSetTransVector((const Mat4x3 *)(rec + 0x10));
		TransformCoordinateToCameraSpace(v, h + 0x20, h + 0x30);
		out[0] = *(int16_t *)(h + 0x20);
		out[1] = *(int16_t *)(h + 0x24);
		out[2] = *(int16_t *)(h + 0x28);
		FieldFree(0x34);
		return 0;
	}

	static Node24 *Spawn(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		return n;
	}

	// one target slice per target: position = the target's bone 0xF1 (jittered by its size), angle
	// random +-0x280, offset 0, speed (0x30..0x5F) x min(size, 0xA00) >> 12
	static void SpawnSlices()
	{
		for (int32_t i = 0; i < (int32_t)Actions()[0x10]; i++)
		{
			uint32_t slot = (*(uint8_t **)(Actions() + 8))[0x18 * i];
			Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SliceTask);
			n->c = 0;
			n->e = (int16_t)slot;
			uint8_t *ent = Entity(slot);
			BonePosition(ent, 0xF1, 0, &n->p10);
			int32_t size = *(int16_t *)(ent + 0x26);
			int32_t cap = size;
			int32_t r = CrtRand() % 256;
			int32_t half = mul32(r + 0x80, size) >> 12;
			int32_t range = half * 2;
			int32_t d = CrtRand() % range;
			n->p10 = (int16_t)(n->p10 + (int16_t)(d - half));
			d = CrtRand() % range;
			n->p12 = (int16_t)(n->p12 + (int16_t)(d - half));
			d = CrtRand() % range;
			n->p14 = (int16_t)(n->p14 + (int16_t)(d - half));
			d = CrtRand() % 0x500;
			n->s1C = 0;
			n->a18 = (int16_t)(d - 0x280);
			if (cap > 0xA00) cap = 0xA00;
			d = CrtRand() % 0x30 + 0x30;
			n->s1E = (int16_t)(mul32(d, cap) >> 12);
		}
	}

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		var<uint32_t>(0x23BF028) = 1;
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0x1B)
		{
			Spawn(ORIG_SparkTask);
			for (uint32_t a = SPARK_HIST; a < 0x23BD444; a += 0x14) var<uint32_t>(a) = 0;
		}
		if (n->c == 0x2C)
		{
			Spawn(ORIG_TrailTask);
			for (uint32_t a = TRAIL_RECS; a < 0x23BF028; a += 0x14) var<uint32_t>(a) = 0;
		}
		if (n->c == 0x40)
		{
			Node24 *p = Spawn(ORIG_PrimATask);
			p->p10 = (int16_t)0xFF56;
			p->p12 = (int16_t)0xFFCE;
			p->s1E = 0x700;
			p->s1C = 0x700;
			p->s20 = 0x800;
		}
		if (n->c == 0x44)
		{
			Node24 *p = Spawn(ORIG_PrimBTask);
			p->p10 = 0;
			p->p12 = 0;
			p->s1E = 0x500;
			p->s1C = 0x500;
		}
		if (n->c == 0x61 && Actions()[0x10] != 0) SpawnSlices();
		if (n->c == 0x40)
		{
			ShowStage();
			SetFlag(4);
		}
		if (n->c == 0x4D)
		{
			HideStage();
			ClearFlag4();
		}
		if (n->c == 0x48) ScreenFade(4, 3, 5, 0xFF);
		if (n->c == 0x4A)
		{
			SetFlag(8);
			uint8_t *e = Caster();
			var<uint32_t>(0x23BE118) = *(uint32_t *)(e + 0x1C);
			uint32_t w = *(uint32_t *)(e + 0x20);
			*(int16_t *)(e + 0x1E) = (int16_t)(*(int16_t *)(e + 0x1E) + 0x2BC);
			*(int16_t *)(e + 0x20) = (int16_t)(*(int16_t *)(e + 0x20) + 0x1130);
			var<uint32_t>(0x23BE11C) = w;
		}
		if (n->c == 0x8F) ApplyActionResultToTargets(*(uint32_t *)(Actions() + 8), Actions()[0x10]);
		if (n->c == 0x1C) BdPlaySE(0xD71564, 0, 0x80);
		n->c++;
		if (n->c <= 0x91) return 0;
		*(uint16_t *)(*(uint8_t **)(Caster() + 0x74) + 0x2C) |= 0x40;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Sword sparks (0x5E2F30, timeline tick 27): every tick the caster's bone 0x28 (base and tip
	// direction) goes into the 3-entry ring 0x23BD408; while the counter <= 11 and at least 2
	// entries exist, a spline through them (Effect_SplineSetup / Effect_SplineEval) seeds 5 sparks
	// along 5 of 6 points. Every spark record (mask 1) is drawn first: flipbook 0xD6F8D0 colour
	// 0x483830, lit by the camera matrix, sequence scale = its size (growing 1/32), moving by its
	// velocity (drag 1/8); dies when its flipbook ends. Ends from tick 4 on the first tick with no
	// live spark.
	// ------------------------------------------------------------------
	static int32_t BoneHistory(int32_t c, uint8_t *s)
	{
		GetEffectSpawnPosition(Caster(), 0x28, 0, (int16_t *)s);
		GetEffectSpawnPosition(Caster(), 0x28, 0x1000, (int16_t *)(s + 8));
		int16_t *a = (int16_t *)s, *b = (int16_t *)(s + 8);
		int16_t d0 = (int16_t)(b[0] - a[0]), d1 = (int16_t)(b[1] - a[1]), d2 = (int16_t)(b[2] - a[2]);
		a[0] = d0;
		a[1] = d1;
		a[2] = d2;
		NormalizeVector16(a, a);
		int32_t idx = c % 3;
		uint32_t *t = (uint32_t *)(SPARK_HIST + 0x14 * idx);
		t[0] = 1;
		t[1] = *(uint32_t *)(s + 8);
		t[2] = *(uint32_t *)(s + 0xC);
		t[3] = *(uint32_t *)(s + 0);
		t[4] = *(uint32_t *)(s + 4);
		int32_t n = 0;
		for (int32_t j = idx; n < 3; n++)
		{
			const uint32_t *e = (const uint32_t *)(SPARK_HIST + 0x14 * j);
			if (!e[0]) break;
			var<uint32_t>(HIST_A + 8 * n) = e[1];
			var<uint32_t>(HIST_A + 8 * n + 4) = e[2];
			var<uint32_t>(HIST_B + 8 * n) = e[3];
			var<uint32_t>(HIST_B + 8 * n + 4) = e[4];
			if (--j < 0) j = 2;
		}
		return n;
	}

	// the sprite header and the light (camera matrix as light matrix, its translation as back colour)
	static void SparkSetup(uint8_t *h, uint8_t *s)
	{
		ff8fx::copy<(0x20)>(s + 0x10, &Camera());
		*(uint32_t *)h = 0xD6F8D0;
		*(uint32_t *)(h + 0x1C) = 0x483830;
		*(int16_t *)(h + 0x24) = 0xC;
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
	}

	static void SparkDrawOne(uint8_t *h, const int16_t *pos, int16_t size, int16_t frame)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void Integrate(Rec *r)
	{
		r->size = (int16_t)(r->size + (int16_t)(r->size >> 5));
		r->x = (int16_t)(r->x + r->vx);
		r->y = (int16_t)(r->y + r->vy);
		r->z = (int16_t)(r->z + r->vz);
		r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
		r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
		r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
	}

	static void SplineRails(int32_t n, int32_t points, int32_t div)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(n, HIST_A, w);
		for (int32_t i = 0, t = 0; i < points; i++, t += 0x1000) SplineEval(n, w, SPLINE_A + 8 * i, t / div);
		SplineSetup(n, HIST_B, w);
		for (int32_t i = 0, t = 0; i < points; i++, t += 0x1000) SplineEval(n, w, SPLINE_B + 8 * i, t / div);
		FieldFree(0x190);
	}

	static uint32_t __cdecl SparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		int32_t hist = BoneHistory(n->c, s);
		SparkSetup(h, s);
		int32_t count = 0;
		for (int i = 0; i < 100; i++)
		{
			Rec *r = RecOf(i);
			if (!(*(const uint8_t *)&r->alive & 1)) continue;
			SparkDrawOne(h, &r->x, r->size, r->age);
			FX_HELD(PoolMemo *pm = MemoRecord(n, i, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
				continue;
			}
			Integrate(r);
			count++;
		}
		FieldFree(0x50);
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0xB && hist > 1)
		{
			SplineRails(hist, 6, 5);
			for (int j = 0; j < 5; j++)
			{
				int i = 0;
				while (i < 100 && RecOf(i)->alive != 0) i++;
				if (i >= 100) break;
				Rec *r = RecOf(i);
				r->alive = 1;
				r->age = 0;
				int32_t sz = CrtRand() % 0x380;
				uint32_t pa = var<uint32_t>(SPLINE_A + 8 * j), pb = var<uint32_t>(SPLINE_A + 8 * j + 4);
				r->size = (int16_t)(sz + 0x280);
				*(uint32_t *)&r->x = pa;
				*(uint32_t *)&r->z = pb;
				r->x = (int16_t)(r->x + (int16_t)(CrtRand() % 10 - 5));
				r->y = (int16_t)(r->y + (int16_t)(CrtRand() % 10 - 5));
				r->z = (int16_t)(r->z + (int16_t)(CrtRand() % 10 - 5));
				int32_t sp = CrtRand() % 0xF + 0x19;
				const int16_t *d = (const int16_t *)(SPLINE_B + 8 * j);
				int32_t k = CrtRand() % 10;
				r->vx = (int16_t)(k + (mul32(d[0], sp) >> 12) - 5);
				k = CrtRand() % 10;
				r->vy = (int16_t)(k + (mul32(d[1], sp) >> 12) - 5);
				k = CrtRand() % 10;
				r->vz = (int16_t)(k + (mul32(d[2], sp) >> 12) - 5);
			}
		}
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Blade trail (0x5E33E0, timeline tick 44), 20 ticks: every tick the blade end points (world)
	// are computed from the caster's matrix o its bone matrix (skeleton + 0xB30) o R(0x400, 0, 0x800)
	// x 0x1A00: base (0, 1000, -1000) -> 0x23BE120, tip (0, 0x1838, 0x258) -> 0x23BE128; they go into
	// the 10-entry ring 0x23BEF60; a 32-point spline through the history (base rail and tip rail) is
	// drawn as 30 Gouraud quad pairs (semi-transparent, colour 0x903030 fading to the far colour 0
	// along the trail through GTE DPCS), through the camera matrix.
	// ------------------------------------------------------------------
	// (the scale vector lives in the task's scratch block s + 0x30)
	static void BladePoints(uint8_t *s, int16_t *p0, int16_t *p1)
	{
		int16_t v[4] = { 0x400, 0, 0x800, 0 };
		Mat4x3 m1, m2;
		ComposeZYXRotationMatrix(v, &m1);
		uint8_t *e = Caster();
		int32_t *sc = (int32_t *)(s + 0x30);
		m1.t[0] = 0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		sc[2] = 0x1A00;
		sc[1] = 0x1A00;
		sc[0] = 0x1A00;
		Scale3DMatrix(&m1, sc);
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), (const Mat4x3 *)(**(uint8_t ***)(e + 0x64) + 0xB30), &m2);
		GteMatrixMultiply(&m2, &m1);
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
		TransformVectorBy3x3Matrix(&m2, m1.t, m1.t);
		int32_t t0 = (int32_t)((uint32_t)m1.t[0] + (uint32_t)m2.t[0]);
		int32_t t1 = (int32_t)((uint32_t)m1.t[1] + (uint32_t)m2.t[1]);
		int32_t t2 = (int32_t)((uint32_t)m1.t[2] + (uint32_t)m2.t[2]);
		m1.t[1] = t1;
		m1.t[2] = t2;
		m1.t[0] = t0;
		v[0] = 0;
		v[1] = 1000;
		v[2] = -1000;
		MatrixMultiplyVector(&m1, v, p0);
		p0[0] = (int16_t)(p0[0] + (int16_t)t0);
		p0[1] = (int16_t)(p0[1] + (int16_t)t1);
		p0[2] = (int16_t)(p0[2] + (int16_t)t2);
		v[0] = 0;
		v[1] = 0x1838;
		v[2] = 0x258;
		MatrixMultiplyVector(&m1, v, p1);
		p1[0] = (int16_t)(p1[0] + (int16_t)t0);
		p1[2] = (int16_t)(p1[2] + (int16_t)t2);
		p1[1] = (int16_t)(p1[1] + (int16_t)t1);
	}

	static void TrailDraw(uint32_t rail_a, uint32_t rail_b, uint8_t *s)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t pk = Cursor();
		*(uint32_t *)(s + 0x14) = 0x3A903030;
		*(uint32_t *)(s + 0x1C) = 0x3A903030;
		uint32_t pk2 = pk + 0x24;
		int32_t acc = 0;
		for (uint32_t j = 0; j < 0xF0; j += 8, acc += 0x1000)
		{
			*(uint32_t *)(s + 0x10) = *(uint32_t *)(s + 0x14);
			uint32_t *a = (uint32_t *)pk, *b = (uint32_t *)pk2;
			a[0] = 0x8000000;
			GteLoadV012((const void *)(rail_a + j), (const void *)(rail_a + j + 8), (const void *)(rail_b + j));
			GteRTPT();
			GteReadFLAG(s + 0xC);
			if (*(uint32_t *)(s + 0xC) & 0x60000) continue;
			GteReadSXY012Split(a + 2, a + 4, a + 6);
			GteLoadV0((const void *)(rail_b + j + 8));
			GteRTPS();
			GteReadSXY2(a + 8);
			GteAVSZ4();
			GteReadOTZ32(s + 8);
			GteSetIR0(acc / 30);
			GteLoadRGBC(s + 0x1C);
			GteDPCS();
			GteStoreRGB2(s + 0x14);
			uint32_t c0 = *(uint32_t *)(s + 0x10), c1 = *(uint32_t *)(s + 0x14);
			a[5] = c0;
			a[1] = c0;
			a[7] = c1;
			a[3] = c1;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, a);
			b[5] = *(uint32_t *)(s + 0x10);
			b[3] = 0x3A000000;
			b[1] = 0x3A000000;
			b[2] = a[2];
			b[7] = *(uint32_t *)(s + 0x14);
			b[4] = a[4];
			b[6] = a[6];
			b[8] = a[8];
			b[0] = 0x8000000;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, b);
			pk += 0x48;
			pk2 += 0x48;
		}
		Cursor() = pk2; // (vanilla: the cursor ends 0x24 bytes past the last packet)
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		BladePoints(s, (int16_t *)BLADE_P0, (int16_t *)BLADE_P1);
		int32_t idx = (int32_t)n->c % 10;
		uint32_t *rec = (uint32_t *)(TRAIL_RECS + 0x14 * idx);
		rec[0] = 1;
		rec[1] = var<uint32_t>(BLADE_P0);
		rec[2] = var<uint32_t>(BLADE_P0 + 4);
		rec[3] = var<uint32_t>(BLADE_P1);
		rec[4] = var<uint32_t>(BLADE_P1 + 4);
		int32_t cnt = 0;
		for (int32_t j = idx; cnt < 10; cnt++)
		{
			const uint32_t *e = (const uint32_t *)(TRAIL_RECS + 0x14 * j);
			if (!e[0]) break;
			var<uint32_t>(HIST_A + 8 * cnt) = e[1];
			var<uint32_t>(HIST_A + 8 * cnt + 4) = e[2];
			const uint32_t *src = cnt == 0 ? (const uint32_t *)BLADE_P1 : e + 3;
			var<uint32_t>(HIST_B + 8 * cnt) = src[0];
			var<uint32_t>(HIST_B + 8 * cnt + 4) = src[1];
			if (--j < 0) j = 9;
		}
		FX_HELD(TrailMemo *m = held_trail_put(n);)
		if (cnt > 1)
		{
			SplineRails(cnt, 32, 31);
			TrailDraw(SPLINE_A, SPLINE_B, s);
			FX_HELD(held_trail_drawn(m);)
		}
		FieldFree(0x40);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x14 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen prims (prim models in screen space, GTE H forced to 0x120 around the draw):
	//   0x5E38D0 (timeline tick 64): model 0xD6FA7C, rotation (0, 0, 0x8AC), t = (+0x10, +0x12,
	//            0x120), scale (+0x1C, +0x20, +0x20); 13 ticks;
	//   0x5E3A00 (timeline tick 68): model 0xD7042C, no rotation, t = (+0x10, +0x12, 0x120), scale
	//            +0x1C; 9 ticks.
	// +0x1C grows by +0x1E, which loses a third every tick.
	// ------------------------------------------------------------------
	static void PrimDraw(const Node24 *n, uint32_t model, int16_t rz, int32_t sx, int32_t syz)
	{
		int16_t v[4] = { 0, 0, rz, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(v, &m);
		m.t[2] = 0x120;
		m.t[1] = n->p12;
		m.t[0] = n->p10;
		int32_t sc[3] = { sx, syz, syz };
		Scale3DMatrix(&m, sc);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		SetProjection(0x120);
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		SetProjection(var<int16_t>(0x1D8E038));
		FieldFree(0x58);
	}

	static uint32_t PrimTask(Node24 *n, int16_t end)
	{
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 3));
		n->c++;
		return n->c >= end ? TASK_END : 0;
	}

	static uint32_t __cdecl PrimATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PrimDraw(n, 0xD6FA7C, 0x8AC, n->s1C, n->s20);
		FX_HELD(MemoNode(n);)
		return PrimTask(n, 0xD);
	}

	static uint32_t __cdecl PrimBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PrimDraw(n, 0xD7042C, 0, n->s1C, n->s1C);
		FX_HELD(MemoNode(n);)
		return PrimTask(n, 9);
	}

	// ------------------------------------------------------------------
	// Target slice (0x5E3C20, timeline tick 97, one per target), 41 ticks; drawn on ticks 0-39: the
	// target entity is hidden from the normal pass (+0 |= 4) and drawn here in two halves by the
	// clipped renderer MAG_167_sub_5E4030: the plane through the node position (normal = the slice
	// angle), one half at the entity matrix, the other shifted along the plane by +0x1C (speed +0x1E:
	// x3 for 4 ticks, then decaying by 1/10, >= 2); from tick 32 the target colour fades to the
	// background colour (GTE DPCS). Its attached model (+0x78, when shown) is cut the same way.
	// ------------------------------------------------------------------
	// MAG_167_sub_5E3EE0: plane distance, n[3] = -(p . n) >> 12
	static void PlaneDistance(const int16_t *p, int16_t *nrm)
	{
		int32_t d = mul32(p[2], nrm[2]) + mul32(p[1], nrm[1]);
		d = d + mul32(p[0], nrm[0]);
		nrm[3] = (int16_t)-(d >> 12);
	}

	// vertex scratch of the clipped renderer: the engine's (0x1D98B3C) unless another one is set here
	static uint32_t g_vertex_scratch_override = 0;

	// MAG_167_sub_5E3F20: shadow, both root matrices, the clipped model, the bone matrices; then the
	// same for the attached model
	static void TargetDraw(uint8_t *h, uint8_t *ent, const Mat4x3 *moved)
	{
		if (!(ent[0] & 0x20))
			EngineCursor() = DrawShadow(ent, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, EngineCursor());
		Mat4x3 *r0 = (Mat4x3 *)(h + 0x190), *r1 = (Mat4x3 *)(h + 0x1B0);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), r0);
		ComposeAffineTransform(&Camera(), moved, r1);
		*(uint32_t *)(h + 4) = g_vertex_scratch_override ? g_vertex_scratch_override : var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x18) = *(uint32_t *)(ent + 0x28);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(ent + 0x7C);
		EngineCursor() = SliceRender(*(void **)(ent + 0x64), h, OT(), 2, EngineCursor());
		BuildBoneMatricesFromPose(ent + 0x60);
		uint8_t *sub = *(uint8_t **)(ent + 0x78);
		if (sub && !(ent[1] & 2))
		{
			*(uint32_t *)(h + 0x10) = 0xFFFFFFFF;
			ComposeAffineTransform(r0, (const Mat4x3 *)(sub + 0x24), r0);
			ComposeAffineTransform(r1, (const Mat4x3 *)(*(uint8_t **)(ent + 0x78) + 0x24), r1);
			EngineCursor() = SliceRender(*(void **)(sub + 4), h, OT(), 2, EngineCursor());
			BuildBoneMatricesFromPose(sub);
		}
	}

	static int32_t SliceFadeW(int32_t c) { return 0x1000 - ComputeSin(shl32(c - 0x20, 10) / 8); }

	// w < 0: the target colour as it is (no fade)
	static void SliceDraw(uint8_t *h, const Node24 *n, uint8_t *ent, int32_t offset, int32_t w)
	{
		int16_t v[4] = { 0, 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(v, &m);
		*(uint32_t *)(h + 0x40) = *(const uint32_t *)&n->p10;
		*(uint32_t *)(h + 0x44) = *(const uint32_t *)&n->p14;
		int16_t *nrm = (int16_t *)(h + 0xA0);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance((const int16_t *)(h + 0x40), nrm);
		v[0] = 0x1000;
		v[1] = 0;
		v[2] = 0;
		GteMatrixMultiply((const Mat4x3 *)(ent + 0x40), &m);
		MatrixMultiplyVector(&m, v, v);
		int32_t d = offset;
		if (v[1] < 0) d = -d;
		Mat4x3 moved = *(const Mat4x3 *)(ent + 0x40);
		moved.t[0] = (int32_t)((uint32_t)moved.t[0] + (uint32_t)(mul32(v[0], d) >> 12));
		moved.t[1] = (int32_t)((uint32_t)moved.t[1] + (uint32_t)(mul32(v[1], d) >> 12));
		moved.t[2] = (int32_t)((uint32_t)moved.t[2] + (uint32_t)(mul32(v[2], d) >> 12));
		if (w >= 0) // the target colour fade (from tick 32)
		{
			uint32_t k = (uint32_t)(shl32(w, 7) >> 12);
			uint32_t col = ((((k | 0x3200u) << 8) | k) << 8) | k;
			*(uint32_t *)(ent + 0x2C) = col;
			GteSetFarColor(var<uint8_t>(0xB8B7D8), var<uint8_t>(0xB8B7D9), var<uint8_t>(0xB8B7DA));
			// the original zeroes the dword above its only argument ([esp + 0x4C], outside its
			// frame) and passes its address: a zero RGBC
			uint32_t zero = 0;
			GteLoadRGBC(&zero);
			GteSetIR0(w);
			GteDPCS();
			GteStoreRGB2(ent + 0x28);
			ent[0x2B] = 2;
			ent[7] = 0;
		}
		TargetDraw(h, ent, &moved);
	}

	static uint32_t __cdecl SliceTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c < 0x28)
		{
			uint8_t *ent = Entity((uint32_t)(int32_t)n->e);
			ent[0] |= 4;
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
			SliceDraw(h, n, ent, n->s1C, n->c >= 0x20 ? SliceFadeW(n->c) : -1);
			FieldFree(0x1D0);
			FX_HELD(MemoNode(n);)
		}
		if (Paused()) return 0;
		int16_t cc = n->c;
		if (cc < 4) n->s1C = (int16_t)(n->s1C + (int16_t)(n->s1E * 3));
		else
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C + v);
			int16_t nv = (int16_t)(v - (int16_t)((int32_t)v / 10));
			n->s1E = nv < 2 ? (int16_t)2 : nv;
		}
		n->c = (int16_t)(cc + 1);
		return n->c > 0x28 ? TASK_END : 0;
	}
}

	void register_mag167_zantetsuken()
	{
		register_port(z167::ORIG_Master, (void *)z167::Master, "167 Master", 167);
		register_port(z167::ORIG_TimelineTask, (void *)z167::TimelineTask, "167 TimelineTask", 167);
		register_port(z167::ORIG_SparkTask, (void *)z167::SparkTask, "167 SparkTask", 167);
		register_port(z167::ORIG_TrailTask, (void *)z167::TrailTask, "167 TrailTask", 167);
		register_port(z167::ORIG_PrimATask, (void *)z167::PrimATask, "167 PrimATask", 167);
		register_port(z167::ORIG_PrimBTask, (void *)z167::PrimBTask, "167 PrimBTask", 167);
		register_port(z167::ORIG_SliceTask, (void *)z167::SliceTask, "167 SliceTask", 167);
		// 30 fps layer: see mag167_zantetsuken_held.inc
		FX_HELD(register_mag167_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag167_zantetsuken_held.inc"
#endif
