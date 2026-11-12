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

// Effect 177: 1000 Needles (enemy attack 25 of kernel.bin, used by the Cactuar c0m055; MAG_177_*).
// Needles fly from random effect bones of the caster to random effect bones of the target; each
// one bursts into a small spark when it arrives.
//
// Structure (setup MAG_177_1000_NEEDLES 0x5CF760, file loader 0x5CF740 = the texture file named at
// 0xD44EA4 (mag176.tim, file 0x2348558); the setup keeps the cast context 0x2348550, the first
// target's slot 0x2341948 and the caster's slot 0x2348554, sets up the root queue 0x2341960 (1 node)
// and the effect queue 0x2347730 (0x64 nodes of 0x24 bytes) with the director, clears the needle
// and spark tables, starts camera animation 0xD44C5C and queues the TIM):
//   RootTask (0x5D0090) - alternates the module packet arena (0x234855C / 0x235855C, 0x10000 each)
//     on the counter's parity (cursor 0x236855C), runs the effect queue, ends when it is empty.
//   Director (0x5CF820) - nothing in the draw-only mode; counter 0 = the target's effect position
//     (bone 0xF0) into its node +0x10, 1 = the needle task (with that position) and sound 0xD44E10,
//     0x32 = damage (action 0's targets); ends after counter 0x37.
//   NeedleTask (0x5CF8E0) - counters 0..0x27 (not in the draw-only mode): 25 needles a tick into
//     free entries of the needle table, each from a random effect bone of the caster to one of the
//     target (both positions moved by -100..99 per axis), 4..7 steps, rotated from (0, 0, -0x1000)
//     onto its direction (GetRotationBetweenVectors / BuildAxisAngleRotationMatrix) and scaled by
//     0x300..0x6FF. Then every needle is drawn (a tetrahedron of 4 flat triangles 0xD44E14 /
//     0xD44E34, camera o needle matrix, renderer 0x5CFE50) and moved by its step; a needle whose
//     steps run out frees its entry and starts a spark (6 ticks) at its position. Then every spark
//     is drawn (sprite sequence 0xD44DC0 frame 0, screen-space transform 0x5CFFE0 scale 0x100, y
//     offset from the table 0xD44E88 by the ticks left). Ends once no needle moved and no spark is
//     left.
// Needle table 0x2341970: 300 entries of 0x38 bytes. Spark table 0x2345B10: 300 entries of 0x18.
// Module globals: 0x2341948..0x2368580 (target slot, root pool / queue, tables, effect queue /
// pool, context 0x2348550, caster slot 0x2348554, texture file 0x2348558, the two packet arenas,
// packet cursor 0x236855C, the sprite transform matrix 0x2368560).

// Fujin's described 1000 Needles (effect_177.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn(), the setup and the file loader ported below; its parameters (NeedlesParams, one layout
// for 1000 and 10,000 Needles: the sound, the director's ticks, the needles' spawns, steps, spread and scale, the
// sparks' life and size; its data: the needle's vertices and triangles, the spark flipbook and its lift table, the
// texture name, the camera track). FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace needles177
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2348550)); }
	inline uint32_t &TargetSlot() { return var<uint32_t>(own::R(0x2341948)); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(own::R(0x2348554)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x236855C)); }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2348558)); }          // the effect's file (its TIM)
	inline TaskQueue *RootQueue() { return (TaskQueue *)own::R(0x2341960); }           // 1 x 0x10
	inline TaskQueue *EffectQueue() { return (TaskQueue *)own::R(0x2347730); }
	static const uint32_t ARENA0 = 0x234855C, ARENA1 = 0x235855C, ARENA_SIZE = 0x10000;
	static const uint32_t SPRITE_MATRIX = 0x2368560;                          // Mat4x3 of the sprite transform
	static const uint32_t NEEDLES = 0x2341970, NEEDLES_END = 0x2345B10;       // 300 x 0x38
	static const uint32_t SPARKS = 0x2345B10, SPARKS_END = 0x2347730;         // 300 x 0x18
	static const int NEEDLE_COUNT = 300, SPARK_COUNT = 300;

	static const uint32_t ORIG_RootTask = 0x5D0090;
	static const uint32_t ORIG_Director = 0x5CF820;
	static const uint32_t ORIG_NeedleTask = 0x5CF8E0;
	static const uint32_t NEEDLE_VERTS = 0xD44E14;    // 4 vertices (8 bytes each)
	static const uint32_t NEEDLE_POLYS = 0xD44E34;    // 4 triangles of 0x14 bytes
	static const uint32_t SEQ_Spark = 0xD44DC0;       // sprite sequence (0x28 bytes, copied to the scratch)
	static const uint32_t SPARK_LIFT = 0xD44E88;      // int32 y offsets by the ticks left
	static const void *const SOUND_Needles = (const void *)0xD44E10;
	static const uint32_t ORIG_FileLoader = 0x5CF740, ORIG_Setup = 0x5CF760;
	static const uint32_t TEXTURE_NAME = 0xD44EA4, CAMERA = 0xD44C5C;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h); one layout for 1000 Needles (177) and 10,000 Needles (178)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_NEEDLES = 1, PART_SPARKS = 2 };
	struct NeedlesParams
	{
		uint32_t sound;                                    // at the director's spawn tick
		struct { int16_t spawn, damage, end; } director;  // its ticks
		struct { int16_t spawn_last, per_tick, steps_min, steps_rand, spread, scale_min, scale_rand; } needles;
		struct { int16_t life, size; } sparks;
		struct { uint32_t spark; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t needle_vertices, needle_triangles, spark_lift; } tables; // its small tables (data)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track
	};
	static const NeedlesParams GAME = {
		0xD44E10,
		{ 1, 0x32, 0x37 },
		{ 0x27, 0x19, 4, 4, 200, 0x300, 0x400 },
		{ 6, 0x100 },
		{ SEQ_Spark },
		{ NEEDLE_VERTS, NEEDLE_POLYS, SPARK_LIFT },
		{ TEXTURE_NAME, CAMERA } };
	static NeedlesParams live = GAME;
	static const NeedlesParams &P() { return live; }

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
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "1000 Needles nodes");
	static_assert(sizeof(Needle) == 0x38 && sizeof(Spark) == 0x18 && sizeof(PolyRec) == 0x14, "1000 Needles tables");

	inline Needle *Needles() { return (Needle *)own::R(NEEDLES); }
	inline Spark *Sparks() { return (Spark *)own::R(SPARKS); }
	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }
	inline uint8_t *Target() { return Entity((int)TargetSlot()); }
	inline int32_t EffectBones(uint8_t *entity) { return **(const uint8_t *const *)*(const uint32_t *)(entity + 0x64); }

	// ------------------------------------------------------------------
	// Needle renderer (0x5CFE50): the 4 triangles of the draw block, culled when a vertex
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

	// sprite transform (0x5CFFE0): a uniform scale on the screen position of pos (x / 8 - 0xA0,
	// y / 8 - 0x6C, projection distance + z), the rotation part loaded into the GTE
	static Mat4x3 *SpriteTransform(const int16_t *pos, int16_t scale, int32_t z)
	{
		Mat4x3 *m = (Mat4x3 *)own::R(SPRITE_MATRIX);
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
		Mat4x3 *m = SpriteTransform(s->pos, P().sparks.size, 0);
		m->t[1] += lift;
		GteSetTransVectorCtrl(m);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	inline int32_t SparkLift(int16_t left) { return var<int32_t>(fxp::data(P().tables.spark_lift, SPARK_LIFT, 0x1C, 4) + (uint32_t)(int32_t)left * 4); }

	// draw blocks (Field_Alloc): the needles' (0xDC) and the sparks' sequence copy (0x28) + header (0xB4)
	static uint8_t *NeedleBlock()
	{
		uint8_t *b = (uint8_t *)FieldAlloc(0xDC);
		memcpy(b + 0x6C, (const void *)fxp::data(P().tables.needle_vertices, NEEDLE_VERTS, 0x20), 0x20);
		memcpy(b + 0x8C, (const void *)fxp::data(P().tables.needle_triangles, NEEDLE_POLYS, 0x50, 4), 0x50);
		memcpy(b + 0x2C, &Camera(), 0x20);
		return b;
	}
	static uint8_t *SparkHeader()
	{
		// (an edited flipbook is drawn where it is: the game's copy holds 0x28 bytes, its one frame)
		uint8_t *seq = (uint8_t *)FieldAlloc(0x28);
		const uint32_t spark = fxp::sequence(P().seqs.spark, SEQ_Spark);
		if (spark == SEQ_Spark) memcpy(seq, (const void *)SEQ_Spark, 0x28);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint8_t **)h = spark == SEQ_Spark ? seq : (uint8_t *)spark;
		*(int16_t *)(h + 0x24) = 0;
		return h;
	}

	static int FreeNeedle()
	{
		int i = 0;
		for (uint32_t a = own::R(NEEDLES); a < own::R(NEEDLES_END); a += sizeof(Needle), i++)
			if (*(uint32_t *)a == 0) return i < NEEDLE_COUNT ? i : -1;
		return -1;
	}
	static int FreeSpark()
	{
		int i = 0;
		for (uint32_t a = own::R(SPARKS); a < own::R(SPARKS_END); a += sizeof(Spark), i++)
			if (*(uint32_t *)a == 0) return i < SPARK_COUNT ? i : -1;
		return -1;
	}

	// 25 needles (0x5CF8E0, ticks 0..0x27); s = the task's Field_Alloc block (0x50)
	static void SpawnNeedles(uint8_t *s)
	{
		int16_t *from = (int16_t *)s, *to = (int16_t *)(s + 8);
		int32_t *scale = (int32_t *)(s + 0x10), *ref = (int32_t *)(s + 0x20), *dir = (int32_t *)(s + 0x30), *axis = (int32_t *)(s + 0x40);
		const int32_t caster_bones = EffectBones(Caster());
		const int32_t target_bones = EffectBones(Target());
		for (int k = 0; k < P().needles.per_tick; k++)
		{
			const int i = FreeNeedle();
			if (i < 0) return;
			Needle *n = &Needles()[i];
			n->active = 1;
			const int32_t steps = CrtRand() % P().needles.steps_rand + P().needles.steps_min;
			const int32_t spread = P().needles.spread;
			n->steps = (int16_t)steps;
			int32_t r = CrtRand();
			GetEffectSpawnPosition(Caster(), r % caster_bones, r % 0x1000, from);
			from[0] = (int16_t)(from[0] + (CrtRand() % spread - spread / 2));
			from[1] = (int16_t)(from[1] + (CrtRand() % spread - spread / 2));
			from[2] = (int16_t)(from[2] + (CrtRand() % spread - spread / 2));
			r = CrtRand();
			GetEffectSpawnPosition(Target(), r % target_bones, r % 0x1000, to);
			to[0] = (int16_t)(to[0] + (CrtRand() % spread - spread / 2));
			to[1] = (int16_t)(to[1] + (CrtRand() % spread - spread / 2));
			to[2] = (int16_t)(to[2] + (CrtRand() % spread - spread / 2));
			dir[0] = to[0] - from[0];
			dir[1] = to[1] - from[1];
			dir[2] = to[2] - from[2];
			n->step[0] = (int16_t)(dir[0] / steps);
			n->step[1] = (int16_t)(dir[1] / steps);
			n->step[2] = (int16_t)(dir[2] / steps);
			NormalizeVector(dir, dir);
			const int32_t angle = RotationBetweenVectors(ref, dir, axis);
			BuildAxisAngleRotationMatrix(angle, &n->m, axis);
			const int32_t sc = CrtRand() % P().needles.scale_rand + P().needles.scale_min;
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
#include "mag177_1000_needles_held.h"
#endif

namespace ff8fx
{
namespace needles177
{
	// ------------------------------------------------------------------
	// Needles and sparks (0x5CF8E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl NeedleTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = 0;
		*(int32_t *)(s + 0x28) = -0x1000;
		if (!DrawOnly() && t->counter >= 0 && t->counter <= P().needles.spawn_last) SpawnNeedles(s);
		FieldFree(0x50);
		// 30 fps layer: see mag177_1000_needles_held.inc
		FX_HELD(held_note_needles(t);)
		uint8_t *b = NeedleBlock();
		int moved = 0;
		FX_PART(PART_NEEDLES);
		for (Needle *n = Needles(); (uint32_t)n < own::R(NEEDLES_END); n++)
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
			sp->left = P().sparks.life;
			sp->pos[0] = (int16_t)n->m.t[0];
			sp->pos[1] = (int16_t)n->m.t[1];
			sp->pos[2] = (int16_t)n->m.t[2];
		}
		FX_PART(0);
		FieldFree(0xDC);
		// 30 fps layer: see mag177_1000_needles_held.inc
		FX_HELD(held_note_sparks();)
		uint8_t *h = SparkHeader();
		int alive = 0;
		FX_PART(PART_SPARKS);
		for (Spark *sp = Sparks(); (uint32_t)sp < own::R(SPARKS_END); sp++)
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
		FX_PART(0);
		FieldFree(0xB4);
		FieldFree(0x28);
		if (DrawOnly()) return 0;
		t->counter++;
		return (alive | moved) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Director (0x5CF820)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)nd;
		if (t->counter == 0) GetEffectSpawnPosition(Target(), 0xF0, 0x1000, t->pos);
		if (t->counter == P().director.spawn)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_NeedleTask));
			*(uint32_t *)&n->pos[0] = *(const uint32_t *)&t->pos[0];
			n->counter = 0;
			*(uint32_t *)&n->pos[2] = *(const uint32_t *)&t->pos[2];
		}
		if (t->counter == P().director.damage) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == P().director.spawn) BdPlaySE((const void *)fxp::sound(P().sound, (uint32_t)SOUND_Needles), 0, 0x80);
		t->counter++;
		return t->counter > P().director.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x5CF740) and setup (0x5CF760): the game runs its own; Fujin's programs these (its memory
	// may be a copy's): the texture, the cast, its first target's and its caster's slots, the root and the director,
	// the needle and spark tables freed, the camera and the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		Ctx() = ctx;
		TargetSlot() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(RootQueue(), (void *)own::R(0x2341950), 0x10, 1);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_RootTask)) + 0xC) = 0;
		InitTaskQueuePool(EffectQueue(), (void *)own::R(0x2347740), 0x24, 0x64);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Director)) + 0xC) = 0;
		for (uint32_t a = own::R(SPARKS); a < own::R(SPARKS_END); a += sizeof(Spark)) *(uint32_t *)a = 0;
		for (uint32_t a = own::R(NEEDLES); a < own::R(NEEDLES_END); a += sizeof(Needle)) *(uint32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA));
		QueueTimUpload(TexFile());
		return RootQueue();
	}

	// ------------------------------------------------------------------
	// Root task (0x5D0090)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		RootNode *r = (RootNode *)nd;
		// 30 fps layer: see mag177_1000_needles_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = own::R(ARENA1);
		if (!(r->counter & 1)) PacketCursor() = own::R(ARENA0);
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The timeline: the needles start at a tick with the sound, the damage (every target of the action) at a tick, the end" },
			{ PART_NEEDLES, "needles", "Needles", "Needles fly from random effect points of the caster to random ones of the target (both moved at random), a few steps each, a flat-shaded tetrahedron turned along its way and scaled at random" },
			{ PART_SPARKS, "sparks", "Sparks", "A spark sprite where each needle arrives, lifted by its table as its ticks run out" },
		};
		static const Field FIELDS[] = {
			{ "director", "sound", "Sound", "At the director's spawn tick", (uint32_t)offsetof(NeedlesParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.spawn", "Needles tick", "The needles start at this tick of the director (with the sound)", (uint32_t)offsetof(NeedlesParams, director.spawn), fxp::I16, 0, 300 },
			{ "director", "director.damage", "Damage tick", "The damage (every target of the action) at this tick", (uint32_t)offsetof(NeedlesParams, director.damage), fxp::I16, 0, 300 },
			{ "director", "director.end", "End tick", "The director's last tick (the effect ends once every needle and spark has)", (uint32_t)offsetof(NeedlesParams, director.end), fxp::I16, 0, 300 },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(NeedlesParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(NeedlesParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "needles", "needles.spawn_last", "Last spawn tick", "Needles spawn from the needle task's tick 0 to this one", (uint32_t)offsetof(NeedlesParams, needles.spawn_last), fxp::I16, -1, 300 },
			{ "needles", "needles.per_tick", "A tick", "How many spawn a tick (300 needles at most at once)", (uint32_t)offsetof(NeedlesParams, needles.per_tick), fxp::I16, 0, 300 },
			{ "needles", "needles.steps_min", "Steps", "A needle's flight in ticks: this + a random below the next", (uint32_t)offsetof(NeedlesParams, needles.steps_min), fxp::I16, 1, 100 },
			{ "needles", "needles.steps_rand", "Steps random", "(at least 1)", (uint32_t)offsetof(NeedlesParams, needles.steps_rand), fxp::I16, 1, 100 },
			{ "needles", "needles.spread", "Spread", "Its start and its end each moved by a random below this on each axis, centred", (uint32_t)offsetof(NeedlesParams, needles.spread), fxp::I16, 1, 0x7FFF },
			{ "needles", "needles.scale_min", "Scale", "Its size: this + a random below the next (4096 = the model's own)", (uint32_t)offsetof(NeedlesParams, needles.scale_min), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "needles", "needles.scale_rand", "Scale random", "(at least 1)", (uint32_t)offsetof(NeedlesParams, needles.scale_rand), fxp::I16, 1, 0x7FFF },
			{ "needles", "tables.needle_vertices", "Needle vertices", "The needle's four vertices (x, y, z; it points along -z) (32 bytes: an edited one keeps its length)", (uint32_t)offsetof(NeedlesParams, tables.needle_vertices), fxp::Data, 0, 0x7FFFFFFF },
			{ "needles", "tables.needle_triangles", "Needle triangles", "The needle's four gouraud triangles: the first corner's colour and GPU code, three vertex numbers (x 2), the other two corners' colours (80 bytes: an edited one keeps its length)", (uint32_t)offsetof(NeedlesParams, tables.needle_triangles), fxp::Data, 0, 0x7FFFFFFF },
			{ "sparks", "sparks.life", "Life", "A spark's ticks (its lift table holds 7 rows, read by the ticks left: 6 at most)", (uint32_t)offsetof(NeedlesParams, sparks.life), fxp::I16, 1, 6 },
			{ "sparks", "sparks.size", "Size", "The spark sprite's scale (256 = the game's)", (uint32_t)offsetof(NeedlesParams, sparks.size), fxp::I16, 0, 0x7FFF },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 1 frame: an edited one keeps as many)", (uint32_t)offsetof(NeedlesParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sparks", "tables.spark_lift", "Spark lift", "The spark's screen y offset by its ticks left (rows 0..6; negative = up) (28 bytes: an edited one keeps its length)", (uint32_t)offsetof(NeedlesParams, tables.spark_lift), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag177_1000_needles()
	{
		fxp::register_params({ 177, "1000 Needles", &needles177::live, &needles177::GAME, sizeof(needles177::NeedlesParams),
		                       needles177::table::PARTS, (int)(sizeof(needles177::table::PARTS) / sizeof(needles177::table::PARTS[0])),
		                       needles177::table::FIELDS, (int)(sizeof(needles177::table::FIELDS) / sizeof(needles177::table::FIELDS[0])),
		                       nullptr, 0 });
		own::note(needles177::ORIG_FileLoader, (void *)needles177::FileLoader);
		own::note(needles177::ORIG_Setup, (void *)needles177::Setup);
		own::note(needles177::ORIG_RootTask, (void *)needles177::RootTask);
		register_port(needles177::ORIG_RootTask, (void *)needles177::RootTask, "N177 RootTask", 177);
		own::note(needles177::ORIG_Director, (void *)needles177::DirectorTask);
		register_port(needles177::ORIG_Director, (void *)needles177::DirectorTask, "N177 Director", 177);
		own::note(needles177::ORIG_NeedleTask, (void *)needles177::NeedleTask);
		register_port(needles177::ORIG_NeedleTask, (void *)needles177::NeedleTask, "N177 NeedleTask", 177);
		// 30 fps layer: see mag177_1000_needles_held.inc
		FX_HELD(register_mag177_held();)
	}
	FX_LAB_MODULE(register_mag177_1000_needles);
}

#ifdef FF8_FX_HELD
#include "mag177_1000_needles_held.inc"
#endif
