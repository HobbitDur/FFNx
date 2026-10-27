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

// Effect 110: Biggs and Wedge's first "death" (Dollet communication tower, scene encounter 29): a
// storm from above blows both away, then Elvoret enters. No kernel.bin entry uses effect 110: the
// death scripts of Biggs c0m074 and Wedge c0m073 launch it, `if (BattleVar98 == 0) { bvar(98, 1);
// target(ALL_ALLIES); attackAnimation(110); leave(0); leave(1); enter(2); }` (AI opcode 0x1E at
// 0x489A28: command 0xFC, the effect id byte through DRAW_TARGET_ID 0x1D27ADA -> 0x1D27ADC
// (Battle_applyDamage 0x4905E4) -> the action's effect id -> BattleActionSequence_Tick_MagicEffect
// 0x50B190). The caster is whichever of the two died first; the targets (action 0) are the monster
// side: Biggs (slot 3) and Wedge (slot 4).
//
// Structure (setup MAG_110_BIGGS__WEDGE_1ST_DEATH 0x6CE210 -> 0x6CE240, file loader 0x6CE220 = the
// texture file named at 0x12E3370 (mag010.tim, as Storm Breath); the setup queues the TIM; no camera
// animation of its own). The module is Storm Breath's (effect 11) code turned into a storm from a
// point above the targets: CountVertices 0x6CE5B0, VertexPosition 0x6CE7E0, DrawStream 0x6CF0C0,
// DebrisTask 0x6CF110, SparkTask 0x6CF400 and HitTask 0x6CE990 are byte copies of Storm Breath's
// 0x6EA8C0 / 0x6EB6F0 / 0x6EB6A0 / 0x6EB790 / 0x6EBA80 / 0x6EBCB0 (module data addresses aside),
// RingTask 0x6CF280 is Storm Breath's 0x6EB900 on this module's stream node layout, ZRotation
// 0x6CF070 its 0x6CF070 port (the function lies in this module's range), FindMaskedVertex 0x6CE880
// the helper Storm Breath calls here.
//   RootTask (0x6CE2A0) - node 0x14: alternates the packet arena (magic buffer + 0x1E9C /
//     + 0x11E9C); on counter 1 (the counter stops at 10) builds the stream vertex table (magic
//     buffer + 0x14FC = the 0x134 vertices of the stream model 0x12E0650, word +6 = z * 4096 / (z
//     range)), sets up the pools and starts the stream task: the targets of action 0 in record
//     order, sweep step (count << 12) / 60, the actor task of target 0; runs the stream, sprite,
//     ring, debris and actor queues; ends when all five are empty.
//   StreamTask (0x6CEA30) - node 0x64, counter 0..0x46: wind sound at 0; counters 0..0x45: the
//     stream vertices wobbled (z rotation by angle A, sine sway by angle B + the vertex's z phase)
//     into the model's own vertex table 0x12E0658 (exe data, rewritten every tick); from counter
//     0xF the sweep t += step (at t >= 1.0: the next target's position kept (+0x3C), its actor
//     task started, next target); the aim point: on the 2nd target (cur 1) half way between its
//     position now and the kept one, else from target cur to target next by t; the stream comes
//     from (aim x, -4000, -4000) (the "mouth" +0x34), stretched to the distance, grows in over 8
//     ticks, fades out after 0x3E, texture v scroll (counter & 7) * 16, drawn by DrawStream;
//     counters 5..0x40: for every stream vertex at y >= 0 a 1/2 chance of a spark (and until 0x36
//     of a bouncing debris model); every 4 ticks until 0x24 an expanding ring along the stream;
//     angles A -= 100, B += 100; ends after 0x46.
//   ActorTask (0x6CE610) - node 0x2C, one per target (target 0 at the start, delay 10; target 1 at
//     its sweep step, delay 2): the target's reaction (QueueChainTransformation 5) and battle text
//     (slot 3: text 10, slot 4: text 1) on its first tick; 10 ticks of two hit sparks on random
//     vertices of the target's model (velocity along the stream), the target pushed back (z + 8),
//     then blown away (velocity (+-100, -200, 256), slot 3 / 4: +x / -x) for the rest of its 50
//     ticks, turning (+0xC += 0xA0); entity flag 8 after 15 ticks; at the end the target is hidden
//     (entity +0x00 bit 2) and its flag 0x1000 cleared.
//   Debris (node 0x30), Ring (node 0x14), sprites Spark (13 frames) / Hit (8 frames) (nodes 0x20) -
//     draw THEN update.
// ENTITY WRITES: the targets' position +0x1C / +0x1E / +0x20 (every actor tick: it MOVES battle
// entities), rotation +0x0C, flags +0x00 (bit 3 after 15 ticks, bit 2 + 0x1000 cleared at the end)
// and +0x01 bit 0x10 every tick, chain transformation 5; battle text 0x487750 (slot, text).
// CAMERA: none written by the module.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25219C8..0x2521A64 (texture file, magic buffer base, context, root pool, packet
// cursor 0x25219EC, actor / debris / ring / sprite / stream / root queues, the sound position
// 0x2521A50 and the wobble translation 0x2521A58 (zero, never written)). Pools, the vertex table
// and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace bw110
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25219CC); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25219D0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25219EC); }
	inline TaskQueue *QActors() { return (TaskQueue *)0x25219F0; }          // pool: magic buffer, 2 x 0x2C
	inline TaskQueue *QDebris() { return (TaskQueue *)0x2521A00; }          // pool: + 0x58, 0x10 x 0x30
	inline TaskQueue *QRings() { return (TaskQueue *)0x2521A10; }           // pool: + 0x358, 0x10 x 0x14
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2521A20; }         // pool: + 0x498, 0x80 x 0x20
	inline TaskQueue *QStream() { return (TaskQueue *)0x2521A30; }          // pool: + 0x1498, 1 x 0x64
	inline const Mat4x3 *WobbleTrans() { return (const Mat4x3 *)0x2521A44; } // translation = words at 0x2521A58
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6CE2A0;
	static const uint32_t ORIG_StreamTask = 0x6CEA30;
	static const uint32_t ORIG_ActorTask = 0x6CE610;
	static const uint32_t ORIG_HitTask = 0x6CE990;
	static const uint32_t ORIG_DebrisTask = 0x6CF110;
	static const uint32_t ORIG_RingTask = 0x6CF280;
	static const uint32_t ORIG_SparkTask = 0x6CF400;
	static const uint32_t MODEL_Stream = 0x12E0650;     // stream model (vertex table at +8)
	static const uint32_t STREAM_VERTS = 0x12E0658;     // 0x134 vertices of 8 bytes
	static const int STREAM_NVERTS = 0x134;
	static const uint32_t MODEL_Debris = 0x12E2F40;     // prim model
	static const uint32_t MODEL_Ring = 0x12E2BD8;       // prim model
	static const uint32_t SEQ_Spark = 0x12E3130;        // sprite sequences
	static const uint32_t SEQ_Hit = 0x12E3290;
	static const void *const SOUND_Wind = (const void *)0x12E336C;
	static const int16_t *const SOUND_Pos = (const int16_t *)0x2521A50; // module global, never written (0, 0, 0)
	static const int16_t MOUTH_YZ = (int16_t)0xF060;    // the stream's source: (aim x, -4000, -4000)

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void BuildOrthonormalBasis(const int32_t *v, Mat4x3 *out) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a * b
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline void QueueChainTransformation(uint8_t *entity, int32_t chain) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, chain); }
	// Thunder_InitBoltTask 0x6DA380 (Effect_FillDwords, a copy of 0x701200): fill `count` dwords (the call this module makes)
	inline void FillDwords(void *dst, uint32_t value, uint32_t count) { fn<void (__cdecl *)(void *, uint32_t, uint32_t)>(0x6DA380)(dst, value, count); }
	inline void PrintMonsterText(int32_t slot, int32_t text) { fn<void (__cdecl *)(int32_t, int32_t)>(0x487750)(slot, text); }  // printMonsterRelatedText0
	// module 213 code: vertices of the objects of a model selected by a mask; the triangle / quad
	// lists of a model (header +0x20 vertices, +0x24 primitive cursor, advanced)
	inline int32_t CountMaskedVertices(uint32_t model, const uint32_t *mask) { return fn<int32_t (__cdecl *)(uint32_t, const uint32_t *)>(0x69EEE0)(model, mask); }
	inline uint32_t DrawTriangleList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x69FD50)(h, ot, mode, cursor); }
	inline uint32_t DrawQuadList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6A01B0)(h, ot, mode, cursor); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..10 (stops at 10), the stream at 1
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct StreamNode // pool of 1 node of 0x64 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..0x46
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ent[3];   // +0x10 target entities (record order)
		int16_t cur;       // +0x1C current / next target of the sweep
		int16_t next;      // +0x1E
		int16_t t;         // +0x20 sweep position between them (4.12)
		int16_t dt;        // +0x22 (count << 12) / 60
		int16_t a;         // +0x24 stream z rotation (-100 per tick)
		int16_t b;         // +0x26 stream sway phase (+100 per tick)
		int32_t dist;      // +0x28 source -> aim point distance
		int32_t dx;        // +0x2C aim - source: x (0), z
		int32_t dz;        // +0x30
		int16_t mouth[4];  // +0x34 the stream's source (aim x, -4000, -4000)
		int16_t kept[4];   // +0x3C the next target's position (x, y, z, +0x22) at its sweep step
		Mat4x3 basis;      // +0x44 stream orientation
	};
	struct ActorNode // pool of 2 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..0x32
		int16_t sparks;    // +0x0E hit-spark ticks left (10)
		StreamNode *sn;    // +0x10
		int16_t flag_delay; // +0x14 ticks before entity flag 8 (15)
		int16_t vc;        // +0x16 vertices of the target's model
		int16_t pad18[2];
		uint8_t *ent;      // +0x1C the target's entity
		int16_t idx;       // +0x20 target index
		int16_t delay;     // +0x22 ticks before the first (10 / 2)
		int16_t vel[3];    // +0x24 entity velocity (x, y, z)
		int16_t pad2A;
	};
	struct SpriteNode // sprite queue: pool of 0x80 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18
		int16_t pad1E;
	};
	struct RingNode // pool of 0x10 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pad0C;
		int16_t e;         // +0x0E growth 0..0x1000 (+0x80 per tick)
		StreamNode *sn;    // +0x10
	};
	struct DebrisNode // pool of 0x10 nodes of 0x30 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16 (written, never read)
		int16_t vel[3];    // +0x18
		int16_t pad1E;
		int16_t ang[3];    // +0x20
		int16_t pad26;
		int16_t spin[3];   // +0x28
		int16_t pad2E;
	};
	// grow / fade ramp and texture scroll of the stream at a counter
	struct Ramp
	{
		int kind;          // 0 = growing (z scale * s, fade 1 - s), 1 = fading out, 2 = steady
		int32_t s;         // kind 0: sin ramp (4.12)
		int32_t h8;        // draw header +8
		int32_t scroll;    // draw header +0xC: texture v scroll (0..0x70, the lists wrap v in 0x80)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(StreamNode) == 0x64 && sizeof(ActorNode) == 0x2C, "Biggs / Wedge death nodes");
	static_assert(sizeof(SpriteNode) == 0x20 && sizeof(RingNode) == 0x14 && sizeof(DebrisNode) == 0x30, "Biggs / Wedge death nodes");

	// x / 12 as the compiler does it (0x2AAAAAAB, sar 1, + sign)
	static int32_t Div12(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32);
		hi >>= 1;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// (count << 12) / 60 as the compiler does it (0x88888889, + x, sar 5, + sign)
	static int16_t SweepStep(int16_t count)
	{
		const int32_t x = shl32(count, 12);
		int32_t q = (int32_t)(((int64_t)x * (int32_t)0x88888889) >> 32) + x;
		q >>= 5;
		q += (int32_t)((uint32_t)q >> 31);
		return (int16_t)q;
	}

	// 0x6CF070: rotation about z (3x3 only)
	static void ZRotation(int16_t angle, Mat4x3 *m)
	{
		const int32_t s = ComputeSin(angle);
		const int32_t c = ComputeCos(angle);
		m->m[0][1] = (int16_t)-s;
		m->m[1][0] = (int16_t)s;
		m->m[0][0] = (int16_t)c;
		m->m[0][2] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[1][2] = 0;
		m->m[2][0] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = 0x1000;
	}

	// ------------------------------------------------------------------
	// Target model helpers
	// ------------------------------------------------------------------
	// 0x6CE5B0: vertices of an entity's visible objects (+ all of its weapon's)
	static int32_t CountVertices(uint8_t *entity)
	{
		uint8_t *a = (uint8_t *)FieldAlloc(0x38);
		uint32_t *mask = (uint32_t *)(a + 0x20);
		*mask = *(const uint32_t *)(entity + 0x7C);
		int32_t n = CountMaskedVertices(*(const uint32_t *)(entity + 0x64), mask);
		uint8_t *weapon = *(uint8_t **)(entity + 0x78);
		if (weapon)
		{
			*mask = 0xFFFFFFFF;
			n += CountMaskedVertices(*(const uint32_t *)(weapon + 4), mask);
		}
		FieldFree(0x38);
		return n;
	}

	// 0x6CE880 (Effect_FindMaskedModelVertex): counts state +4 down over the vertex groups of the
	// objects selected by the mask state +0; the group that reaches it: its vertex (group size - the
	// count left) through the group's bone matrix (skeleton + 0x20 + 0x30 * bone): state +0x10 the
	// vertex, +8 the transformed point (3 words); state +4 = the vertex's index. Returns 1 when found.
	static int32_t FindMaskedVertex(uint32_t model, uint8_t *state)
	{
		const uint8_t *bones = *(uint8_t *const *)model + 0x10;
		const uint8_t *objs = *(uint8_t *const *)(model + 4);
		const int32_t nobj = *(const int32_t *)objs;
		const int32_t *offs = (const int32_t *)(objs + 4);
		for (int32_t o = 0; o < nobj; o++)
		{
			const uint8_t *p = *(uint8_t *const *)(model + 4) + offs[o];
			if (!(*(const uint32_t *)state & (1u << (o & 31)))) continue;
			const int32_t ngroups = *(const int16_t *)p;
			p += 2;
			for (int32_t g = 0; g < ngroups; g++)
			{
				const int32_t bone = *(const int16_t *)p;
				p += 4;
				const uint8_t *bm = bones + bone * 0x30 + 0x10;
				const int32_t nv = *(const int16_t *)(p - 2);
				int32_t n = *(const int32_t *)(state + 4) - nv;
				*(int32_t *)(state + 4) = n;
				if (n <= 0)
				{
					n = -n;
					*(int32_t *)(state + 4) = n;
					const uint8_t *v = p + n * 6;
					GteSetRotMatrix((const Mat4x3 *)bm);
					GteSetTransVector((const Mat4x3 *)bm);
					*(int16_t *)(state + 0x10) = *(const int16_t *)v;
					*(int16_t *)(state + 0x12) = *(const int16_t *)(v + 2);
					*(int16_t *)(state + 0x14) = *(const int16_t *)(v + 4);
					GteLoadV0(state + 0x10);
					GteMVMVA_RotV0Tr();
					GteStoreIR123(state + 8);
					return 1;
				}
				p += nv * 6;
			}
		}
		return 0;
	}

	// 0x6CE7E0: position of vertex n of an entity's model (its weapon's after the body's); out gets
	// the 8 scratch bytes the search stores it in (3 words + a scratch word)
	static void VertexPosition(uint8_t *entity, int32_t n, void *out)
	{
		uint8_t *a = (uint8_t *)FieldAlloc(0x38);
		*(int32_t *)(a + 0x24) = n;
		*(uint32_t *)(a + 0x20) = *(const uint32_t *)(entity + 0x7C);
		ComputeBonesWorldMatrices(entity + 0x60, entity + 0x40);
		const int32_t found = FindMaskedVertex(*(const uint32_t *)(entity + 0x64), a + 0x20);
		BuildBoneMatricesFromPose(entity + 0x60);
		if (found == 0)
		{
			uint8_t *weapon = *(uint8_t **)(entity + 0x78);
			if (weapon)
			{
				*(uint32_t *)(a + 0x20) = 0xFFFFFFFF;
				ComputeBonesWorldMatrices(weapon, entity + 0x40);
				FindMaskedVertex(*(const uint32_t *)(weapon + 4), a + 0x20);
				BuildBoneMatricesFromPose(weapon);
			}
		}
		memcpy(out, a + 0x28, 8);
		FieldFree(0x38);
	}

	// ------------------------------------------------------------------
	// The stream (stream task)
	// ------------------------------------------------------------------
	// the stream vertices: z rotation by a, then swayed by b + the vertex's z phase
	static void StreamWobble(int16_t a, int16_t b, uint8_t *out)
	{
		Mat4x3 rz;
		ZRotation(a, &rz);
		GteSetRotMatrix(&rz);
		GteSetTransVector(WobbleTrans());
		const uint8_t *src = (const uint8_t *)(TexBase() + 0x14FC);
		for (int i = 0; i < STREAM_NVERTS; i++, src += 8, out += 8)
		{
			GteLoadV0(src);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(out);
			const int16_t w6 = *(const int16_t *)(src + 6);
			const int16_t ang = (int16_t)(b + w6);
			const int32_t r = ComputeSin((int32_t)w6 >> 1) >> 3;
			int16_t *o = (int16_t *)out;
			o[0] = (int16_t)(o[0] + (int16_t)(mul32(ComputeCos(ang), r) >> 12));
			o[1] = (int16_t)(o[1] - (int16_t)(mul32(ComputeSin(ang), r) >> 12));
		}
	}

	// the aim point between two positions (t 4.12)
	static void StreamAim(const int16_t *p0, const int16_t *p1, int16_t t, int16_t *aim)
	{
		for (int k = 0; k < 3; k++) aim[k] = (int16_t)((mul32(p1[k] - p0[k], t) >> 12) + p0[k]);
	}

	// the aim point on the 2nd target: half way between its position p and the kept one
	static void StreamAimKept(const int16_t *p, const int16_t *kept, int16_t *aim)
	{
		for (int k = 0; k < 3; k++) aim[k] = (int16_t)(((int32_t)p[k] + kept[k]) >> 1);
	}

	// grow / fade ramp and texture scroll at a counter
	static Ramp StreamRamp(uint32_t c)
	{
		Ramp r;
		r.scroll = (int32_t)((c & 7) << 4);
		r.s = 0x1000;
		if (c < 8)
		{
			r.kind = 0;
			r.s = ComputeSin((int32_t)((c & 0x3FFFFF) << 7));
			r.h8 = 0x1000 - r.s;
		}
		else if (c > 0x3E)
		{
			r.kind = 1;
			r.h8 = ComputeSin((int32_t)(((c << 7) - 0x1E81) & 0x1FFFFF80));
		}
		else
		{
			r.kind = 2;
			r.h8 = 0;
		}
		return r;
	}

	// 0x6CF0C0: the stream model's triangle and quad lists (vertices: the model's own table)
	static uint32_t DrawStream(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor, uint32_t verts)
	{
		const uint32_t model = *(const uint32_t *)h;
		*(uint32_t *)(h + 0x20) = verts;
		*(uint32_t *)(h + 0x24) = model + *(const uint32_t *)model + 0xC;
		cursor = DrawTriangleList(h, ot, mode, cursor);
		*(uint32_t *)(h + 0x24) += 0xC;
		return DrawQuadList(h, ot, mode, cursor);
	}

	// the stream from the source (aim x, -4000, -4000) to the aim point (header h allocated by the
	// caller, freed here): source, distance and aim vector into the node, ramp (computed here
	// unless given), orientation into node->basis, m1 = basis * z scale at the source
	static void StreamDraw(uint8_t *h, StreamNode *s, uint32_t c, const Ramp *given, const int16_t *aim, Mat4x3 *m1, uint32_t verts)
	{
		s->mouth[0] = aim[0];
		const int32_t dy = (int32_t)aim[1] - s->mouth[1];
		const int32_t dz = (int32_t)aim[2] - s->mouth[2];
		s->dx = 0;
		s->dz = dz;
		s->dist = Sqrt(mul32(dz, dz) + mul32(dy, dy) + 0);
		int16_t zs = (int16_t)s->dist;
		const Ramp r = given ? *given : StreamRamp(c);
		if (r.kind == 0)
		{
			zs = (int16_t)(mul32(zs, r.s) >> 12);
			*(int32_t *)(h + 8) = r.h8;
			*(int32_t *)(h + 4) = 0;
		}
		else if (r.kind == 1)
		{
			*(int32_t *)(h + 8) = r.h8;
			*(int32_t *)(h + 4) = 0;
		}
		else
		{
			*(int32_t *)(h + 8) = 0;
		}
		const int32_t v[3] = { 0, (int32_t)(0u - (uint32_t)dy), dz };
		Mat4x3 sc; // 3x3 only
		sc.m[0][0] = 0x1000; sc.m[0][1] = 0; sc.m[0][2] = 0;
		sc.m[1][0] = 0; sc.m[1][1] = 0x1000; sc.m[1][2] = 0;
		sc.m[2][0] = 0; sc.m[2][1] = 0; sc.m[2][2] = zs;
		BuildOrthonormalBasis(v, &s->basis);
		*m1 = s->basis;
		MatrixMultiply3(m1, &sc);
		m1->t[0] = s->mouth[0];
		m1->t[1] = s->mouth[1];
		m1->t[2] = s->mouth[2];
		Mat4x3 m3;
		ComposeAffineTransform(&Camera(), m1, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		*(int16_t *)(h + 0x16) = 0x100;
		*(int16_t *)(h + 0x14) = 0x100;
		*(int16_t *)(h + 0x1E) = 0x100;
		*(int16_t *)(h + 0x12) = 0;
		*(int16_t *)(h + 0x10) = 0;
		*(int16_t *)(h + 0x1A) = 0;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x1C) = 0x80;
		*(int16_t *)(h + 0xC) = (int16_t)r.scroll;
		*(uint32_t *)h = MODEL_Stream;
		PacketCursor() = DrawStream(h, RenderOT(0x44), 2, PacketCursor(), verts);
		FieldFree(0x5C);
	}

	// ------------------------------------------------------------------
	// Particles: draw functions and updates
	// ------------------------------------------------------------------
	// sprite sequence at a node's position (Spark / Hit)
	static void SpriteDraw(const SpriteNode *p, uint32_t seq)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->scale >> 2));
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = p->counter;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t SpriteMove(SpriteNode *p, int16_t frames)
	{
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->counter++;
		return p->counter < frames ? 0 : TASK_END;
	}

	// debris scale from counter 22 on
	static int32_t DebrisScale(int16_t counter) { return Div12(shl32(0x16 - counter, 12)) + 0x1000; }

	static void DebrisDraw(const DebrisNode *d, bool scaled, int32_t s)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 m;
		BuildRotationMatrixFromAngles(d->ang, &m);
		if (scaled)
		{
			const int32_t v[3] = { s, s, s };
			Scale3DMatrix(&m, v);
		}
		m.t[0] = d->pos[0];
		m.t[1] = d->pos[1];
		m.t[2] = d->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = MODEL_Debris;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// falls, bounces off y 0 (spin reversed), spins
	static uint32_t DebrisMove(DebrisNode *d)
	{
		d->vel[1] = (int16_t)(d->vel[1] + 0x14);
		d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
		d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
		d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
		if (d->pos[1] >= 0 && d->vel[1] > 0)
		{
			d->vel[1] = (int16_t)(-0x14 - d->vel[1]);
			d->spin[0] = (int16_t)-d->spin[0];
			d->spin[1] = (int16_t)-d->spin[1];
			d->spin[2] = (int16_t)-d->spin[2];
		}
		d->ang[0] = (int16_t)(d->ang[0] + d->spin[0]);
		d->ang[1] = (int16_t)(d->ang[1] + d->spin[1]);
		d->ang[2] = (int16_t)(d->ang[2] + d->spin[2]);
		d->counter++;
		return d->counter < 0x1E ? 0 : TASK_END;
	}

	// ring along the stream of node sn, radius sin(e/2)/8 around the stream axis at e
	static void RingDraw(const RingNode *g, const StreamNode *sn)
	{
		uint8_t *h = AllocHeader(0x58);
		const int16_t ang = (int16_t)(sn->b + g->e);
		const int32_t rad = (int16_t)(ComputeSin((int32_t)g->e >> 1) >> 3);
		int16_t v[4];
		v[0] = (int16_t)(mul32(ComputeCos(ang), rad) >> 12);
		v[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeSin(ang), rad)) >> 12);
		v[2] = (int16_t)(mul32(sn->dist, g->e) >> 12);
		v[3] = 0; // never written by the original (stack word)
		GteSetRotMatrix(&sn->basis);
		Mat4x3 tm; // translation only
		tm.t[0] = sn->mouth[0];
		tm.t[1] = sn->mouth[1];
		tm.t[2] = sn->mouth[2];
		GteSetTransVector(&tm);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		Mat4x3 m;
		GteReadMAC123(m.t);
		const int16_t s = (int16_t)(g->e + 0x400);
		m.m[0][0] = s; m.m[0][1] = 0; m.m[0][2] = 0;
		m.m[1][0] = 0; m.m[1][1] = s; m.m[1][2] = 0;
		m.m[2][0] = 0; m.m[2][1] = 0; m.m[2][2] = 0x1000;
		MatrixMultiply(&sn->basis, &m);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x30;
		*(uint32_t *)h = MODEL_Ring;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag110_biggs_wedge_death_held.h"
#endif

namespace ff8fx
{
namespace bw110
{
	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(held_note(K_SPARK, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Spark);
		return SpriteMove(p, 0xD);
	}

	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(held_note(K_HIT, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Hit);
		return SpriteMove(p, 8);
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(held_note(K_DEBRIS, d, sizeof(DebrisNode));)
		const bool scaled = d->counter >= 0x16;
		DebrisDraw(d, scaled, scaled ? DebrisScale(d->counter) : 0);
		return DebrisMove(d);
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(held_note(K_RING, g, sizeof(RingNode));)
		RingDraw(g, g->sn);
		g->e = (int16_t)(g->e + 0x80);
		return g->e < 0x1000 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Actor (0x6CE610) and its start (0x6CE530)
	// ------------------------------------------------------------------
	static void StartActor(StreamNode *s, int32_t idx)
	{
		ActorNode *a = (ActorNode *)AddTaskToQueue(QActors(), ORIG_ActorTask);
		if (!a) return;
		FillDwords(&a->counter, 0, 8);
		a->sn = s;
		uint8_t *e = s->ent[idx];
		a->ent = e;
		a->vc = (int16_t)CountVertices(e);
		a->idx = (int16_t)idx;
		a->vel[2] = 8;
		if (idx == 0)
		{
			a->delay = 0xA;
			a->flag_delay = 0xF;
		}
		else if (idx == 1)
		{
			a->delay = 2;
			a->flag_delay = 0xF;
		}
	}

	static uint32_t __cdecl ActorTask(TaskNode *n)
	{
		ActorNode *a = (ActorNode *)n;
		if (a->delay != 0)
		{
			a->delay--;
			return 0;
		}
		if (a->counter == 0)
		{
			if (a->idx == 0 || a->idx == 1)
			{
				QueueChainTransformation(a->ent, 5);
				a->sparks = 0xA;
			}
			const int32_t off = (int32_t)((uint32_t)a->ent - 0x1D972C0u);
			int32_t slot = (int32_t)(((int64_t)off * (int32_t)0xD20D20D3) >> 32) + off;
			slot >>= 7;
			slot += (int32_t)((uint32_t)slot >> 31);
			if (slot == 3) PrintMonsterText(3, 0xA);
			else if (slot == 4) PrintMonsterText(4, 1);
		}
		if (a->flag_delay == 0) a->ent[0] |= 8;
		else a->flag_delay--;
		if (a->sparks == 0)
		{
			*(int16_t *)(a->ent + 0xC) = (int16_t)(*(const int16_t *)(a->ent + 0xC) + 0xA0);
		}
		else
		{
			// two hit sparks on vertices of the target's model, along the stream
			int32_t v = mul32(CrtRand(), a->vc) >> 16;
			for (int k = 0; k < 2; k++)
			{
				SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_HitTask);
				if (!p) continue;
				FillDwords(&p->counter, 0, 5);
				VertexPosition(a->ent, v, p->pos);
				p->scale = (int16_t)((CrtRand() & 0x7FF) + 0xC00);
				p->vel[0] = (int16_t)(shl32(a->sn->dx, 7) / a->sn->dist);
				p->vel[1] = 0;
				p->vel[2] = (int16_t)(shl32(a->sn->dz, 7) / a->sn->dist);
				v += (int32_t)a->vc >> 1;
			}
			a->sparks--;
			if (a->sparks == 0)
			{
				// blown away
				a->vel[2] = 0x100;
				a->vel[1] = (int16_t)0xFF38;
				if (a->idx == 0) a->vel[0] = 0x64;
				else if (a->idx == 1) a->vel[0] = (int16_t)0xFF9C;
			}
		}
		*(int16_t *)(a->ent + 0x1C) = (int16_t)(*(const int16_t *)(a->ent + 0x1C) + a->vel[0]);
		*(int16_t *)(a->ent + 0x1E) = (int16_t)(*(const int16_t *)(a->ent + 0x1E) + a->vel[1]);
		*(int16_t *)(a->ent + 0x20) = (int16_t)(*(const int16_t *)(a->ent + 0x20) + a->vel[2]);
		a->ent[1] |= 0x10;
		a->counter++;
		if (a->counter < 0x32) return 0;
		a->ent[0] |= 4;
		*(uint16_t *)a->ent &= 0xEFFF;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Stream (0x6CEA30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl StreamTask(TaskNode *n)
	{
		StreamNode *s = (StreamNode *)n;
		s->mouth[1] = MOUTH_YZ;
		s->mouth[2] = MOUTH_YZ;
		const uint32_t c = (uint32_t)(int32_t)s->counter;
		Mat4x3 m1;    // stream frame (basis * z scale at the source)
		if (c < 0x46)
		{
			// 30 fps layer: see mag110_biggs_wedge_death_held.inc
			FX_HELD(held_note_stream(s);)
			uint8_t *h = AllocHeader(0x5C);
			if (c == 0) BdPlaySE3D(SOUND_Wind, 0, SOUND_Pos);
			StreamWobble(s->a, s->b, (uint8_t *)STREAM_VERTS);
			if (c >= 0xF)
			{
				s->t = (int16_t)(s->t + s->dt);
				if (s->t >= 0x1000)
				{
					s->t = (int16_t)(s->t + 0xF000);
					if (s->cur != s->next)
					{
						memcpy(s->kept, s->ent[s->next] + 0x1C, 8);
						StartActor(s, s->next);
					}
					const int16_t nx = s->next;
					s->cur = nx;
					s->next = (int16_t)(nx + 1);
					if (s->next >= s->count) s->next = (int16_t)(s->count - 1);
				}
			}
			int16_t aim[3];
			if (s->cur == 1) StreamAimKept((const int16_t *)(s->ent[s->next] + 0x1C), s->kept, aim);
			else StreamAim((const int16_t *)(s->ent[s->cur] + 0x1C), (const int16_t *)(s->ent[s->next] + 0x1C), s->t, aim);
			// 30 fps layer: see mag110_biggs_wedge_death_held.inc
			FX_HELD(held_note_aim(s, aim);)
			StreamDraw(h, s, c, nullptr, aim, &m1, STREAM_VERTS);
		}
		if (c - 5 < 0x3C)
		{
			// sparks (and debris) off the stream vertices above the ground
			const uint32_t sweep = c - 5;
			GteSetRotMatrix(&m1);
			GteSetTransVector(&m1);
			const uint8_t *vtx = (const uint8_t *)STREAM_VERTS;
			for (int i = 0; i < STREAM_NVERTS; i++, vtx += 8)
			{
				int16_t w[4];
				GteLoadV0(vtx);
				GteMVMVA_RotV0Tr();
				GteStoreIR123(w);
				if (w[1] < 0) continue;
				if (CrtRand() < 0x800)
				{
					SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SparkTask);
					if (p)
					{
						FillDwords(&p->counter, 0, 5);
						p->pos[0] = w[0];
						p->pos[1] = w[1];
						p->pos[2] = w[2];
						p->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
						p->vel[1] = -0x32;
						p->vel[0] = (int16_t)(shl32(0, 7) / s->dist);
						p->vel[2] = (int16_t)(shl32(s->dz, 7) / s->dist);
					}
				}
				if (sweep >= 0x32) continue;
				if (CrtRand() >= 0x800) continue;
				DebrisNode *e = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
				if (!e) continue;
				FillDwords(&e->counter, 0, 9);
				e->pos[0] = w[0];
				e->pos[1] = w[1];
				e->pos[2] = w[2];
				e->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
				e->vel[0] = (int16_t)(shl32(0, 6) / s->dist);
				e->vel[1] = (int16_t)(-0x80 - (CrtRand() & 0x7F));
				e->vel[2] = (int16_t)(shl32(s->dz, 6) / s->dist);
				e->spin[0] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
				e->spin[1] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
				e->spin[2] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
			}
		}
		if (c < 0x25 && (c & 3) == 0)
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
			if (g)
			{
				FillDwords(&g->pad0C, 0, 2);
				g->sn = s;
			}
		}
		const int16_t cc = s->counter;
		s->a = (int16_t)(s->a - 100);
		s->b = (int16_t)(s->b + 100);
		if (cc >= 0x46) return TASK_END;
		s->counter = (int16_t)(cc + 1);
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CE2A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1E9C;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x11E9C;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				// stream vertex table: word +6 = z * 4096 / (z range)
				int32_t lo = *(const int16_t *)(STREAM_VERTS + 4), hi = lo;
				for (int i = 1; i < STREAM_NVERTS; i++)
				{
					const int32_t z = *(const int16_t *)(STREAM_VERTS + i * 8 + 4);
					if (lo > z) lo = z;
					else if (hi < z) hi = z;
				}
				const int32_t range = hi - lo;
				uint8_t *dst = (uint8_t *)(TexBase() + 0x14FC);
				for (int i = 0; i < STREAM_NVERTS; i++, dst += 8)
				{
					memcpy(dst, (const void *)(STREAM_VERTS + i * 8), 8);
					*(int16_t *)(dst + 6) = (int16_t)(shl32(*(const int16_t *)(dst + 4), 12) / range);
				}
				InitTaskQueuePool(QStream(), (void *)(TexBase() + 0x1498), 0x64, 1);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x498), 0x20, 0x80);
				InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x358), 0x14, 0x10);
				InitTaskQueuePool(QDebris(), (void *)(TexBase() + 0x58), 0x30, 0x10);
				InitTaskQueuePool(QActors(), (void *)TexBase(), 0x2C, 2);
			}
			StreamNode *s = (StreamNode *)AddTaskToQueue(QStream(), ORIG_StreamTask);
			if (s)
			{
				FillDwords(&s->counter, 0, 0x16);
				s->next = 1;
				s->count = (int16_t)Ctx()->actions[0].target_count;
				for (int i = 0; i < s->count; i++) s->ent[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
				s->dt = SweepStep(s->count);
				StartActor(s, 0);
			}
		}
		int a, b, c, d, e;
		if (r->started)
		{
			a = ExecuteTaskQueue(QStream());
			b = ExecuteTaskQueue(QSprites());
			c = ExecuteTaskQueue(QRings());
			d = ExecuteTaskQueue(QDebris());
			e = ExecuteTaskQueue(QActors());
		}
		else
		{
			a = b = c = d = e = (int)n;
		}
		if (r->started && a == 0 && b == 0 && c == 0 && d == 0 && e == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 10;
		return 0;
	}
}

	void register_mag110_biggs_wedge_death()
	{
		register_port(bw110::ORIG_RootTask, (void *)bw110::RootTask, "110 RootTask", 110);
		register_port(bw110::ORIG_StreamTask, (void *)bw110::StreamTask, "110 StreamTask", 110);
		register_port(bw110::ORIG_ActorTask, (void *)bw110::ActorTask, "110 ActorTask", 110);
		register_port(bw110::ORIG_HitTask, (void *)bw110::HitTask, "110 HitTask", 110);
		register_port(bw110::ORIG_DebrisTask, (void *)bw110::DebrisTask, "110 DebrisTask", 110);
		register_port(bw110::ORIG_RingTask, (void *)bw110::RingTask, "110 RingTask", 110);
		register_port(bw110::ORIG_SparkTask, (void *)bw110::SparkTask, "110 SparkTask", 110);
		// 30 fps layer: see mag110_biggs_wedge_death_held.inc
		FX_HELD(register_mag110_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag110_biggs_wedge_death_held.inc"
#endif
