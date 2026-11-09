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

// Effect 178: 10,000 Needles (enemy attack 230 of kernel.bin, used by the Jumbo Cactuar c0m084; MAG_178_*).
// The same module as 1000 Needles (effect 177, mag177_1000_needles.cpp: same code at other addresses):
// the needles start at the director's tick 0x24, the damage comes at 0x55, the director ends after
// 0x5A and the needles are scaled by 0x480..0xA7F.
//
// Structure (setup MAG_178_10000_NEEDLES 0x5CEDD0, file loader 0x5CEDB0 = the texture file named at
// 0xD44C50 (mag176.tim, file 0x2321920); the setup keeps the cast context 0x2321918, the first
// target's slot 0x231AD10 and the caster's slot 0x232191C, sets up the root queue 0x231AD28 (1 node)
// and the effect queue 0x2320AF8 (0x64 nodes of 0x24 bytes) with the director, clears the needle
// and spark tables, starts camera animation 0xD4499C and queues the TIM):
//   RootTask (0x5CF700) - alternates the module packet arena (0x2321924 / 0x2331924, 0x10000 each)
//     on the counter's parity (cursor 0x2341924), runs the effect queue, ends when it is empty.
//   Director (0x5CEE90) - nothing in the draw-only mode; counter 0 = the target's effect position
//     (bone 0xF0) into its node +0x10, 0x24 = the needle task (with that position) and sound
//     0xD44BBC, 0x55 = damage (action 0's targets); ends after counter 0x5A.
//   NeedleTask (0x5CEF50) - as 177's 0x5CF8E0 (needle renderer 0x5CF4C0, sprite transform 0x5CF650,
//     needle model 0xD44BC0 / 0xD44BE0, spark sequence 0xD44B94, spark y offsets 0xD44C34).
// Needle table 0x231AD38: 300 entries of 0x38 bytes. Spark table 0x231EED8: 300 entries of 0x18.
// Module globals: 0x231AD10..0x2341948 (target slot, root pool / queue, tables, effect queue /
// pool, context 0x2321918, caster slot 0x232191C, texture file 0x2321920, the two packet arenas,
// packet cursor 0x2341924, the sprite transform matrix 0x2341928).

#include "mag_common.h"

namespace ff8fx
{
namespace needles178
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2321918); }
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x231AD10); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x232191C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2341924); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x2320AF8; }
	static const uint32_t ARENA0 = 0x2321924, ARENA1 = 0x2331924, ARENA_SIZE = 0x10000;
	static const uint32_t SPRITE_MATRIX = 0x2341928;                          // Mat4x3 of the sprite transform
	static const uint32_t NEEDLES = 0x231AD38, NEEDLES_END = 0x231EED8;       // 300 x 0x38
	static const uint32_t SPARKS = 0x231EED8, SPARKS_END = 0x2320AF8;         // 300 x 0x18
	static const int NEEDLE_COUNT = 300, SPARK_COUNT = 300;

	static const uint32_t ORIG_RootTask = 0x5CF700;
	static const uint32_t ORIG_Director = 0x5CEE90;
	static const uint32_t ORIG_NeedleTask = 0x5CEF50;
	static const uint32_t NEEDLE_VERTS = 0xD44BC0;    // 4 vertices (8 bytes each)
	static const uint32_t NEEDLE_POLYS = 0xD44BE0;    // 4 triangles of 0x14 bytes
	static const uint32_t SEQ_Spark = 0xD44B94;       // sprite sequence (0x28 bytes, copied to the scratch)
	static const uint32_t SPARK_LIFT = 0xD44C34;      // int32 y offsets by the ticks left
	static const void *const SOUND_Needles = (const void *)0xD44BBC;
	// timing and needle scale of this module (10,000 Needles)
	static const int16_t TICK_SPAWN = 0x24, TICK_DAMAGE = 0x55, TICK_LAST = 0x5A;
	static const int32_t SCALE_RANGE = 0x600, SCALE_BASE = 0x480;

	// ---- engine functions (original addresses)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
	inline void GteLoadIRFromMatrixColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteStoreIRToMatrixColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
	inline void GteLoadV0FromDwords(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x45E060)(v); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }

	// ------------------------------------------------------------------
	// Node / table layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad;
	};
	struct EffectNode // pool of 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[4];    // +0x10 director: the target's effect position (copied to the needle task, unused)
		uint8_t pad18[0xC];
	};
	struct Needle // 0x38 bytes
	{
		uint32_t active;   // +0x00 bit 0
		int16_t steps;     // +0x04 steps left
		uint8_t pad06[0xA];
		int16_t step[3];   // +0x10 position change per tick
		int16_t pad16;
		Mat4x3 m;          // +0x18 rotation * scale, translation = position
	};
	struct Spark // 0x18 bytes
	{
		uint32_t active;   // +0x00 bit 0
		int16_t left;      // +0x04 ticks left (6 .. 1)
		int16_t pad06;
		int16_t pos[3];    // +0x08
		uint8_t pad0E[0xA];
	};
	// draw block of the needles (Field_Alloc 0xDC): +0x00 MAC0, +0x08 OTZ, +0x0C FLAG, +0x2C the camera,
	// +0x4C camera o needle, +0x6C the vertices, +0x8C the triangles
	struct PolyRec // 0x14 bytes
	{
		uint32_t code;     // colour + GPU code
		uint16_t v[3];     // vertex byte offsets / 4
		uint16_t pad;
		uint32_t uv0, uv1; // packet words 3 and 5
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "10,000 Needles nodes");
	static_assert(sizeof(Needle) == 0x38 && sizeof(Spark) == 0x18 && sizeof(PolyRec) == 0x14, "10,000 Needles tables");

	inline Needle *Needles() { return (Needle *)NEEDLES; }
	inline Spark *Sparks() { return (Spark *)SPARKS; }
	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }
	inline uint8_t *Target() { return Entity((int)TargetSlot()); }
	inline int32_t EffectBones(uint8_t *entity) { return **(const uint8_t *const *)*(const uint32_t *)(entity + 0x64); }

	// ------------------------------------------------------------------
	// Needle renderer (0x5CF4C0): the 4 triangles of the draw block, culled when a vertex
	// overflows, back-facing, or every vertex is off screen on one axis
	// ------------------------------------------------------------------
	static uint32_t DrawNeedlePolys(uint8_t *b, uint32_t ot, int shift, uint32_t cursor)
	{
		const PolyRec *rec = (const PolyRec *)(b + 0x8C);
		uint8_t *verts = b + 0x6C;
		for (int k = 0; k < 4; k++, rec++)
		{
			uint32_t *p = (uint32_t *)cursor;
			GteLoadV012(verts + rec->v[0] * 4, verts + rec->v[1] * 4, verts + rec->v[2] * 4);
			GteRTPT();
			p[0] = 0x6000000;
			p[1] = rec->code;
			GteReadFLAG(b + 0xC);
			if (*(int32_t *)(b + 0xC) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(b);
			if (*(int32_t *)b <= 0) continue;
			int16_t *xy0 = (int16_t *)(cursor + 8), *xy1 = (int16_t *)(cursor + 0x10), *xy2 = (int16_t *)(cursor + 0x18);
			GteReadSXY012(xy0, xy1, xy2);
			GteAVSZ3();
			uint32_t off = 0;
			if (xy0[0] < 0 || xy0[0] > 0xA00) off = 1;
			if (xy1[0] < 0 || xy1[0] > 0xA00) off |= 2;
			if (xy2[0] < 0 || xy2[0] > 0xA00) off |= 4;
			if (xy0[1] < 0 || xy0[1] > 0x6C0) off |= 0x10;
			if (xy1[1] < 0 || xy1[1] > 0x6C0) off |= 0x20;
			if (xy2[1] < 0 || xy2[1] > 0x6C0) off |= 0x40;
			if ((off & 7) == 7 || (off & 0x70) == 0x70) continue;
			GteReadOTZ32(b + 8);
			p[3] = rec->uv0;
			p[5] = rec->uv1;
			InsertPrimAutoDepth(ot + (uint32_t)(*(int32_t *)(b + 8) >> shift) * 4, p);
			cursor += 0x1C;
		}
		return cursor;
	}

	// one needle: camera o needle matrix into the draw block, then its triangles
	static void DrawNeedle(uint8_t *b, const Needle *n)
	{
		Mat4x3 *cam = (Mat4x3 *)(b + 0x2C);
		Mat4x3 *m = (Mat4x3 *)(b + 0x4C);
		GteSetRotMatrixCtrl(cam);
		GteLoadIRFromMatrixColumn(&n->m.m[0][0]);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(&m->m[0][0]);
		GteLoadIRFromMatrixColumn(&n->m.m[0][1]);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(&m->m[0][1]);
		GteLoadIRFromMatrixColumn(&n->m.m[0][2]);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(&m->m[0][2]);
		GteSetTransVectorCtrl(cam);
		GteLoadV0FromDwords(n->m.t);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(m->t);
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(m);
		PacketCursor() = DrawNeedlePolys(b, RenderOT(), 2, PacketCursor());
	}

	// sprite transform (0x5CF650): a uniform scale on the screen position of pos (x / 8 - 0xA0,
	// y / 8 - 0x6C, projection distance + z), the rotation part loaded into the GTE
	static Mat4x3 *SpriteTransform(const int16_t *pos, int16_t scale, int32_t z)
	{
		Mat4x3 *m = (Mat4x3 *)SPRITE_MATRIX;
		m->m[2][2] = scale;
		m->m[1][1] = scale;
		m->m[0][0] = scale;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		sxy[0] = (int16_t)(sxy[0] / 8);
		sxy[1] = (int16_t)(sxy[1] / 8);
		m->t[0] = sxy[0] - 0xA0;
		m->t[1] = sxy[1] - 0x6C;
		m->t[2] = var<int16_t>(0x1D8E038) + z;
		GteSetRotMatrix(m);
		return m;
	}

	// one spark with the sprite header h
	static void DrawSpark(uint8_t *h, const Spark *s, int32_t lift)
	{
		*(int16_t *)(h + 4) = 0;
		Mat4x3 *m = SpriteTransform(s->pos, 0x100, 0);
		m->t[1] += lift;
		GteSetTransVectorCtrl(m);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	inline int32_t SparkLift(int16_t left) { return var<int32_t>(SPARK_LIFT + (uint32_t)(int32_t)left * 4); }

	// draw blocks (Field_Alloc): the needles' (0xDC) and the sparks' sequence copy (0x28) + header (0xB4)
	static uint8_t *NeedleBlock()
	{
		uint8_t *b = (uint8_t *)FieldAlloc(0xDC);
		memcpy(b + 0x6C, (const void *)NEEDLE_VERTS, 0x20);
		memcpy(b + 0x8C, (const void *)NEEDLE_POLYS, 0x50);
		memcpy(b + 0x2C, &Camera(), 0x20);
		return b;
	}
	static uint8_t *SparkHeader()
	{
		uint8_t *seq = (uint8_t *)FieldAlloc(0x28);
		memcpy(seq, (const void *)SEQ_Spark, 0x28);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint8_t **)h = seq;
		*(int16_t *)(h + 0x24) = 0;
		return h;
	}

	static int FreeNeedle()
	{
		int i = 0;
		for (uint32_t a = NEEDLES; a < NEEDLES_END; a += sizeof(Needle), i++)
			if (*(uint32_t *)a == 0) return i < NEEDLE_COUNT ? i : -1;
		return -1;
	}
	static int FreeSpark()
	{
		int i = 0;
		for (uint32_t a = SPARKS; a < SPARKS_END; a += sizeof(Spark), i++)
			if (*(uint32_t *)a == 0) return i < SPARK_COUNT ? i : -1;
		return -1;
	}

	// 25 needles (0x5CEF50, ticks 0..0x27); s = the task's Field_Alloc block (0x50)
	static void SpawnNeedles(uint8_t *s)
	{
		int16_t *from = (int16_t *)s, *to = (int16_t *)(s + 8);
		int32_t *scale = (int32_t *)(s + 0x10), *ref = (int32_t *)(s + 0x20), *dir = (int32_t *)(s + 0x30), *axis = (int32_t *)(s + 0x40);
		const int32_t caster_bones = EffectBones(Caster());
		const int32_t target_bones = EffectBones(Target());
		for (int k = 0; k < 0x19; k++)
		{
			const int i = FreeNeedle();
			if (i < 0) return;
			Needle *n = &Needles()[i];
			n->active = 1;
			const int32_t steps = CrtRand() % 4 + 4;
			n->steps = (int16_t)steps;
			int32_t r = CrtRand();
			GetEffectSpawnPosition(Caster(), r % caster_bones, r % 0x1000, from);
			from[0] = (int16_t)(from[0] + (CrtRand() % 200 - 100));
			from[1] = (int16_t)(from[1] + (CrtRand() % 200 - 100));
			from[2] = (int16_t)(from[2] + (CrtRand() % 200 - 100));
			r = CrtRand();
			GetEffectSpawnPosition(Target(), r % target_bones, r % 0x1000, to);
			to[0] = (int16_t)(to[0] + (CrtRand() % 200 - 100));
			to[1] = (int16_t)(to[1] + (CrtRand() % 200 - 100));
			to[2] = (int16_t)(to[2] + (CrtRand() % 200 - 100));
			dir[0] = to[0] - from[0];
			dir[1] = to[1] - from[1];
			dir[2] = to[2] - from[2];
			n->step[0] = (int16_t)(dir[0] / steps);
			n->step[1] = (int16_t)(dir[1] / steps);
			n->step[2] = (int16_t)(dir[2] / steps);
			NormalizeVector(dir, dir);
			const int32_t angle = RotationBetweenVectors(ref, dir, axis);
			BuildAxisAngleRotationMatrix(angle, &n->m, axis);
			const int32_t sc = CrtRand() % SCALE_RANGE + SCALE_BASE;
			scale[2] = sc;
			scale[1] = sc;
			scale[0] = sc;
			Scale3DMatrix(&n->m, scale);
			n->m.t[0] = from[0];
			n->m.t[1] = from[1];
			n->m.t[2] = from[2];
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag178_10000_needles_held.h"
#endif

namespace ff8fx
{
namespace needles178
{
	// ------------------------------------------------------------------
	// Needles and sparks (0x5CEF50)
	// ------------------------------------------------------------------
	static uint32_t __cdecl NeedleTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = 0;
		*(int32_t *)(s + 0x28) = -0x1000;
		if (!DrawOnly() && t->counter >= 0 && t->counter <= 0x27) SpawnNeedles(s);
		FieldFree(0x50);
		// 30 fps layer: see mag178_10000_needles_held.inc
		FX_HELD(held_note_needles(t);)
		uint8_t *b = NeedleBlock();
		int moved = 0;
		for (Needle *n = Needles(); (uint32_t)n < NEEDLES_END; n++)
		{
			if (!(n->active & 1)) continue;
			DrawNeedle(b, n);
			if (DrawOnly()) continue;
			n->steps--;
			if (n->steps >= 0)
			{
				n->m.t[0] += n->step[0];
				n->m.t[1] += n->step[1];
				n->m.t[2] += n->step[2];
				moved++;
				continue;
			}
			n->active = 0;
			const int i = FreeSpark();
			if (i < 0) continue;
			Spark *sp = &Sparks()[i];
			sp->active = 1;
			sp->left = 6;
			sp->pos[0] = (int16_t)n->m.t[0];
			sp->pos[1] = (int16_t)n->m.t[1];
			sp->pos[2] = (int16_t)n->m.t[2];
		}
		FieldFree(0xDC);
		// 30 fps layer: see mag178_10000_needles_held.inc
		FX_HELD(held_note_sparks();)
		uint8_t *h = SparkHeader();
		int alive = 0;
		for (Spark *sp = Sparks(); (uint32_t)sp < SPARKS_END; sp++)
		{
			if (!(sp->active & 1)) continue;
			const int16_t left = sp->left;
			DrawSpark(h, sp, SparkLift(left));
			if (DrawOnly()) continue;
			const int32_t next = (int32_t)left - 1;
			if (next > 0)
			{
				sp->left = (int16_t)next;
				alive++;
			}
			else sp->active = 0;
		}
		FieldFree(0xB4);
		FieldFree(0x28);
		if (DrawOnly()) return 0;
		t->counter++;
		return (alive | moved) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Director (0x5CEE90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)nd;
		if (t->counter == 0) GetEffectSpawnPosition(Target(), 0xF0, 0x1000, t->pos);
		if (t->counter == TICK_SPAWN)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_NeedleTask);
			*(uint32_t *)&n->pos[0] = *(const uint32_t *)&t->pos[0];
			n->counter = 0;
			*(uint32_t *)&n->pos[2] = *(const uint32_t *)&t->pos[2];
		}
		if (t->counter == TICK_DAMAGE) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == TICK_SPAWN) BdPlaySE(SOUND_Needles, 0, 0x80);
		t->counter++;
		return t->counter > TICK_LAST ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x5CF700)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		RootNode *r = (RootNode *)nd;
		// 30 fps layer: see mag178_10000_needles_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA1;
		if (!(r->counter & 1)) PacketCursor() = ARENA0;
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag178_10000_needles()
	{
		register_port(needles178::ORIG_RootTask, (void *)needles178::RootTask, "N178 RootTask", 178);
		register_port(needles178::ORIG_Director, (void *)needles178::DirectorTask, "N178 Director", 178);
		register_port(needles178::ORIG_NeedleTask, (void *)needles178::NeedleTask, "N178 NeedleTask", 178);
		// 30 fps layer: see mag178_10000_needles_held.inc
		FX_HELD(register_mag178_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag178_10000_needles_held.inc"
#endif
