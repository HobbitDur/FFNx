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

// Effect 11: Storm Breath (enemy attack, e.g. Elvoret; MAG_011_*).
//
// Structure (setup MAG_011_STORM_BREATH 0x6EA500 -> _Init 0x6EA530, file loader 0x6EA510 = the
// texture file named at 0x12E3370 (mag010.tim); the setup keeps the CASTER's entity, starts
// camera animation 0x136B974 and queues the TIM):
//   RootTask (0x6EA5B0) - alternates the packet arena (magic buffer + 0x26B8 / + 0x126B8); on
//     counter 1 (the counter stops at 10: one spawn only) sets up the pools and the breath-stream
//     vertex table (magic buffer + 0x1B18 = the 0x134 vertices of the stream model 0x1365720, word
//     +6 = z * 4096 / (z range)) and starts the target task: action 0's targets sorted by entity
//     +0x1C (descending), the breath sweeps over them in that order. Runs the target, sprite,
//     ring and debris queues; ends when all four are empty.
//   Target (0x6EA920) - node 0x504: every tick the caster's mouth (anchor 0x1D, flag 0x400);
//     sound at 8;
//     counters 25..74: the breath stream: the stream vertices wobbled (z rotation by angle A,
//       then a sine sway by angle B + the vertex's z phase) into the model's own vertex table
//       0x1365728 (exe data, rewritten every tick), aimed from the mouth at a point moving from the
//       current target's anchor 0xF1 to the next one's (t += (targets << 12) / 40 per tick from
//       counter 30; at t >= 1.0 damage to the current target, next target), stretched to the
//       distance (basis 0x50CBA0 from the aim vector, z scale = distance, grows in over 8 ticks,
//       fades out over the last 7), drawn by 0x6EB6A0 (the triangle / quad lists of module 213's
//       0x69FD50 / 0x6A01B0);
//     counters 0..23: prim-model layout 0x13693D0 at the mouth turned by the caster's facing, and
//       two smoke puffs per tick around the mouth; from counter 29: layout 0x13686C4 at the mouth
//       position frozen at 29, oriented along the stream;
//     counters 35..64: two hit sparks per tick on random vertices of the current target's model;
//     counters 30..69: for every stream vertex above y 0, 1/2 chance of a spark (and until 59 1/2
//       chance of a bouncing debris model);
//     counters 20..64: two mist sprites per tick blown out of the mouth; every 4 ticks from 25 to
//       41 an expanding ring along the stream; angles A -= 100, B += 100; ends at counter 75.
//   PartCallback (0x6EB4D0) - one object of either layout: ZYX rotation, record offset, composed
//     with the parameter block's matrix, scaled, vertex frames blended into magic buffer + 0x24B8
//     (64-vertex objects: exactly up to arena A), draw flags from the block's table.
//   Debris (0x6EB790, node 0x30), Ring (0x6EB900, node 0x14), sprites Spark (0x6EBA80), Puff
//   (0x6EBB20: GTE blend from its spawn point to the mouth over 12 frames), Mist (0x6EBC10),
//   Hit (0x6EBCB0) (nodes 0x20) - draw THEN update.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2556140..0x25561C8 (caster entity, texture file, magic buffer base, context,
// root pool, packet cursor 0x2556164, debris / ring / sprite / target / root queues; the stream's
// wobble translation reads the three words at 0x25561B8). Pools, the vertex table, the morph
// buffer and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace storm011
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2556140); }          // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2556148); }         // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x255614C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2556164); }
	inline TaskQueue *QDebris() { return (TaskQueue *)0x2556168; }          // pool: + 0x1D4, 0x10 x 0x30
	inline TaskQueue *QRings() { return (TaskQueue *)0x2556178; }           // pool: + 0x4D4, 0x10 x 0x14
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2556188; }         // pool: + 0x614, 0x80 x 0x20
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2556198; }         // pool: + 0x1614, 1 x 0x504
	inline const Mat4x3 *WobbleTrans() { return (const Mat4x3 *)0x25561A4; } // translation = words at 0x25561B8
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6EA5B0;
	static const uint32_t ORIG_TargetTask = 0x6EA920;
	static const uint32_t ORIG_DebrisTask = 0x6EB790;
	static const uint32_t ORIG_RingTask = 0x6EB900;
	static const uint32_t ORIG_SparkTask = 0x6EBA80;
	static const uint32_t ORIG_PuffTask = 0x6EBB20;
	static const uint32_t ORIG_MistTask = 0x6EBC10;
	static const uint32_t ORIG_HitTask = 0x6EBCB0;
	static const uint32_t MODEL_Stream = 0x1365720;     // stream model (vertex table at +8)
	static const uint32_t STREAM_VERTS = 0x1365728;     // 0x134 vertices of 8 bytes
	static const int STREAM_NVERTS = 0x134;
	static const uint32_t MODEL_Mouth = 0x13693D0;      // prim-model layout data (0x4A0-byte layout)
	static const uint32_t MODEL_Cone = 0x13686C4;       // prim-model layout data (0x140-byte layout)
	static const uint32_t FLAGS_Mouth = 0x136BAFC;      // draw flags per object
	static const uint32_t FLAGS_Cone = 0x136BB24;
	static const uint32_t MODEL_Debris = 0x1368010;     // prim model
	static const uint32_t MODEL_Ring = 0x1367CA8;       // prim model
	static const uint32_t SEQ_Spark = 0x1368200;        // sprite sequences
	static const uint32_t SEQ_Puff = 0x1368580;
	static const uint32_t SEQ_Mist = 0x136843C;
	static const uint32_t SEQ_Hit = 0x1368360;
	static const void *const SOUND_Breath = (const void *)0x136B970;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void BuildOrthonormalBasis(const int32_t *v, Mat4x3 *out) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a * b
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	// MAG_011_sub_6D5580: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x6D5580)(model, f0, f1, t, out); }
	// MAG_011_sub_69EEE0 (module 213 code): vertices of the objects of a model selected by a mask
	inline int32_t CountMaskedVertices(uint32_t model, const uint32_t *mask) { return fn<int32_t (__cdecl *)(uint32_t, const uint32_t *)>(0x69EEE0)(model, mask); }
	// MAG_011_sub_6CE880: finds vertex +4 (counted down over the masked objects' vertex groups);
	// found: its bone-space position through the bone matrix at +8 (3 words), returns 1
	inline int32_t FindMaskedVertex(uint32_t model, void *state) { return fn<int32_t (__cdecl *)(uint32_t, void *)>(0x6CE880)(model, state); }
	// MAG_011_sub_69FD50 / MAG_011_sub_6A01B0 (module 213 code): the triangle / quad lists of a
	// model (header +0x20 vertices, +0x24 primitive cursor, advanced)
	inline uint32_t DrawTriangleList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x69FD50)(h, ot, mode, cursor); }
	inline uint32_t DrawQuadList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6A01B0)(h, ot, mode, cursor); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..10 (stops at 10), the target spawn at 1
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 1 node of 0x504 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ent[3];   // +0x10 target entities, sorted by entity +0x1C (descending)
		uint8_t order[4];  // +0x1C their target record indices
		int16_t cur;       // +0x20 current / next target of the sweep
		int16_t next;      // +0x22
		int16_t t;         // +0x24 sweep position between them (4.12)
		int16_t dt;        // +0x26 (count << 12) / 40
		int16_t a;         // +0x28 stream z rotation (-100 per tick)
		int16_t b;         // +0x2A stream sway phase (+100 per tick)
		int16_t vc;        // +0x2C vertices of the current target's model
		int16_t pad2E;
		int32_t dist;      // +0x30 mouth -> aim point distance
		int16_t mouth[4];  // +0x34 caster's mouth (anchor 0x1D, every tick)
		int16_t frozen[4]; // +0x3C the mouth at counter 29
		Mat4x3 basis;      // +0x44 stream orientation
		uint8_t layout[0x4A0]; // +0x64 prim-model layout (mouth, then cone)
	};
	struct SpriteNode // sprite queue: pool of 0x80 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18 (not the puff)
		int16_t pad1E;
	};
	struct RingNode // pool of 0x10 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pad0C;
		int16_t e;         // +0x0E growth 0..0x1000 (+0x80 per tick)
		TargetNode *tn;    // +0x10
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
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		const Mat4x3 *m;   // +0x00 camera * layout frame
		uint32_t flags;    // +0x04 draw flags per object (table)
		uint32_t morph;    // +0x08 blended vertex frames
	};
	// grow / fade ramp and texture scroll of the stream
	struct Ramp
	{
		int kind;          // 0 = growing (z scale * s, fade 1 - s), 1 = fading out, 2 = steady
		int32_t s;         // kind 0: sin ramp (4.12)
		int32_t h8;        // draw header +8
		int32_t scroll;    // draw header +0xC: texture v scroll (0..0x7F, the lists wrap v in 0x80)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x504, "Storm Breath nodes");
	static_assert(sizeof(SpriteNode) == 0x20 && sizeof(RingNode) == 0x14 && sizeof(DebrisNode) == 0x30, "Storm Breath nodes");
	static_assert(sizeof(PrimArg) == 0xC, "Storm Breath prim block");

	// x / 12 as the compiler does it (0x2AAAAAAB, sar 1, + sign)
	static int32_t Div12(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32);
		hi >>= 1;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// MAG_011_sub_6CF070: rotation about z (3x3 only)
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

	// MAG_011_sub_6D9510: rotation about y (3x3 only)
	static void YawMatrix(int16_t yaw, Mat4x3 *m)
	{
		const int32_t sn = ComputeSin(yaw);
		const int32_t cs = ComputeCos(yaw);
		m->m[0][2] = (int16_t)sn;
		m->m[2][0] = (int16_t)-sn;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = 0x1000;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	// ------------------------------------------------------------------
	// Target model helpers
	// ------------------------------------------------------------------
	// 0x6EA8C0: vertices of an entity's visible objects (+ all of its weapon's)
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

	// 0x6EB6F0: position of vertex n of an entity's model (its weapon's after the body's); out gets
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
	// Prim-player callback (0x6EB4D0): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m;
		ComposeZYXRotationMatrix(r->rot, &m);
		m.t[0] = r->pos[0];
		m.t[1] = r->pos[1];
		m.t[2] = r->pos[2];
		ComposeAffineTransform(arg->m, &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t f = (((const uint32_t *)arg->flags)[(int16_t)r->index] & 0xDFFF) | 0x2000;
		*(uint32_t *)(h + 0x1C) = f;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = f | 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Breath stream pieces (target task)
	// ------------------------------------------------------------------
	// the stream vertices: z rotation by a, then swayed by b + the vertex's z phase
	static void StreamWobble(int16_t a, int16_t b, uint8_t *out)
	{
		Mat4x3 rz;
		ZRotation(a, &rz);
		GteSetRotMatrix(&rz);
		GteSetTransVector(WobbleTrans());
		const uint8_t *src = (const uint8_t *)(TexBase() + 0x1B18);
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

	// the aim point between two target anchors (t 4.12)
	static void StreamAim(const int16_t *p0, const int16_t *p1, int16_t t, int16_t *aim)
	{
		for (int k = 0; k < 3; k++) aim[k] = (int16_t)((mul32(p1[k] - p0[k], t) >> 12) + p0[k]);
	}

	// grow / fade ramp and texture scroll at a stream phase (counter - 25)
	static Ramp StreamRamp(uint32_t phase)
	{
		Ramp r;
		r.scroll = (int32_t)((phase & 7) << 4);
		r.s = 0x1000;
		if (phase < 8)
		{
			r.kind = 0;
			r.s = ComputeSin((int32_t)((phase & 0x3FFFFF) << 7));
			r.h8 = 0x1000 - r.s;
		}
		else if (phase > 0x2A)
		{
			r.kind = 1;
			r.h8 = ComputeSin((int32_t)(((phase << 7) - 0x1481) & 0x1FFFFF80));
		}
		else
		{
			r.kind = 2;
			r.h8 = 0;
		}
		return r;
	}

	// 0x6EB6A0: the stream model's triangle and quad lists
	static uint32_t DrawStream(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor, uint32_t verts)
	{
		const uint32_t model = *(const uint32_t *)h;
		*(uint32_t *)(h + 0x20) = verts;
		*(uint32_t *)(h + 0x24) = model + *(const uint32_t *)model + 0xC;
		cursor = DrawTriangleList(h, ot, mode, cursor);
		*(uint32_t *)(h + 0x24) += 0xC;
		return DrawQuadList(h, ot, mode, cursor);
	}

	// the stream from the mouth to the aim point (header h allocated by the caller, freed here):
	// distance, ramp (computed here unless given), orientation into *basis, m1 = basis * z scale
	// at the mouth; d = aim - mouth
	static void StreamDraw(uint8_t *h, uint32_t phase, const Ramp *given, const int16_t *aim, const int16_t *mouth,
		int32_t *dist, Mat4x3 *basis, Mat4x3 *m1, int32_t d[3], uint32_t verts)
	{
		d[0] = (int32_t)aim[0] - mouth[0];
		d[2] = (int32_t)aim[2] - mouth[2];
		d[1] = (int32_t)aim[1] - mouth[1];
		*dist = Sqrt(mul32(d[2], d[2]) + mul32(d[1], d[1]) + mul32(d[0], d[0]));
		int16_t zs = (int16_t)*dist;
		const Ramp r = given ? *given : StreamRamp(phase);
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
		const int32_t v[3] = { (int32_t)(0u - (uint32_t)d[0]), (int32_t)(0u - (uint32_t)d[1]), d[2] };
		Mat4x3 sc; // 3x3 only
		sc.m[0][0] = 0x1000; sc.m[0][1] = 0; sc.m[0][2] = 0;
		sc.m[1][0] = 0; sc.m[1][1] = 0x1000; sc.m[1][2] = 0;
		sc.m[2][0] = 0; sc.m[2][1] = 0; sc.m[2][2] = zs;
		BuildOrthonormalBasis(v, basis);
		*m1 = *basis;
		MatrixMultiply3(m1, &sc);
		m1->t[0] = mouth[0];
		m1->t[1] = mouth[1];
		m1->t[2] = mouth[2];
		Mat4x3 m3;
		ComposeAffineTransform(&Camera(), m1, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		*(int16_t *)(h + 0x12) = 0;
		*(int16_t *)(h + 0x10) = 0;
		*(int16_t *)(h + 0x1A) = 0;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x16) = 0x100;
		*(int16_t *)(h + 0x14) = 0x100;
		*(int16_t *)(h + 0x1C) = 0x80;
		*(int16_t *)(h + 0x1E) = 0x100;
		*(int16_t *)(h + 0xC) = (int16_t)r.scroll;
		*(uint32_t *)h = MODEL_Stream;
		PacketCursor() = DrawStream(h, RenderOT(0x44), 2, PacketCursor(), verts);
		FieldFree(0x5C);
	}

	// the sweep's next state (after a hit: damage, next target)
	static void SweepStep(TargetNode *t)
	{
		t->t = (int16_t)(t->t + t->dt);
		if (t->t < 0x1000) return;
		ApplyActionResultToTarget(Ctx()->actions[0].targets + t->order[t->cur] * TARGET_STRIDE);
		if (t->cur != t->next) t->vc = (int16_t)CountVertices(t->ent[t->next]);
		t->t = (int16_t)(t->t + 0xF000);
		int16_t nx = t->next;
		t->cur = nx;
		nx = (int16_t)(nx + 1);
		t->next = nx;
		if (nx >= t->count) t->next = (int16_t)(t->count - 1);
	}

	// ------------------------------------------------------------------
	// Particles: draw functions and updates
	// ------------------------------------------------------------------
	// sprite sequence at a node's position (Spark / Mist / Hit)
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

	// puff: its spawn point blended toward the mouth (target node +0x34) by w (4.12)
	static void PuffDraw(const SpriteNode *p, int32_t w)
	{
		uint8_t *h = AllocHeader(0xB4);
		GteSetIR0(w);
		GteLoadIR123((const void *)(TexBase() + 0x1648));
		GteGPF();
		GteSetIR0(0x1000 - w);
		GteLoadIR123(p->pos);
		GteGPL();
		int16_t pos[4];
		pos[3] = 0; // never written by the original (stack word)
		GteStoreIR123(pos);
		TransformCameraByShadowRotation(pos, p->scale, -((int32_t)p->scale >> 2));
		*(uint32_t *)h = SEQ_Puff;
		*(int16_t *)(h + 4) = p->counter;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static int32_t PuffWeight(int16_t counter) { return Div12(shl32(counter, 12)); }

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

	// ring along the stream of target node tn, radius sin(e/2)/8 around the stream axis at e
	static void RingDraw(const RingNode *g, const TargetNode *tn)
	{
		uint8_t *h = AllocHeader(0x58);
		const int16_t ang = (int16_t)(tn->b + g->e);
		const int32_t rad = (int16_t)(ComputeSin((int32_t)g->e >> 1) >> 3);
		int16_t v[4];
		v[0] = (int16_t)(mul32(ComputeCos(ang), rad) >> 12);
		v[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeSin(ang), rad)) >> 12);
		v[2] = (int16_t)(mul32(tn->dist, g->e) >> 12);
		v[3] = 0; // never written by the original (stack word)
		GteSetRotMatrix(&tn->basis);
		Mat4x3 tm; // translation only
		tm.t[0] = tn->mouth[0];
		tm.t[1] = tn->mouth[1];
		tm.t[2] = tn->mouth[2];
		GteSetTransVector(&tm);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		Mat4x3 m;
		GteReadMAC123(m.t);
		const int16_t s = (int16_t)(g->e + 0x400);
		m.m[0][0] = s; m.m[0][1] = 0; m.m[0][2] = 0;
		m.m[1][0] = 0; m.m[1][1] = s; m.m[1][2] = 0;
		m.m[2][0] = 0; m.m[2][1] = 0; m.m[2][2] = 0x1000;
		MatrixMultiply(&tn->basis, &m);
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
#include "mag011_storm_breath_held.h"
#endif

namespace ff8fx
{
namespace storm011
{
	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_SPARK, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Spark);
		return SpriteMove(p, 0xD);
	}

	static uint32_t __cdecl MistTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_MIST, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Mist);
		return SpriteMove(p, 0xC);
	}

	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_HIT, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Hit);
		return SpriteMove(p, 8);
	}

	static uint32_t __cdecl PuffTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_PUFF, p, sizeof(SpriteNode));)
		PuffDraw(p, PuffWeight(p->counter));
		p->counter++;
		return p->counter < 0xC ? 0 : TASK_END;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_DEBRIS, d, sizeof(DebrisNode));)
		const bool scaled = d->counter >= 0x16;
		DebrisDraw(d, scaled, scaled ? DebrisScale(d->counter) : 0);
		return DebrisMove(d);
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note(K_RING, g, sizeof(RingNode));)
		RingDraw(g, g->tn);
		g->e = (int16_t)(g->e + 0x80);
		return g->e < 0x1000 ? 0 : TASK_END;
	}

	// two mist sprites blown out of the mouth (depth vz), 180 degrees apart
	static void SpawnMist(TargetNode *t, int16_t vz)
	{
		Mat4x3 m;
		YawMatrix(*(const int16_t *)(Caster() + 0xE), &m);
		m.t[0] = t->mouth[0];
		m.t[1] = t->mouth[1];
		m.t[2] = t->mouth[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		int32_t angle = CrtRand();
		for (int k = 0; k < 2; k++)
		{
			SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_MistTask);
			if (!p) continue;
			Memset32(&p->counter, 0, 5);
			int16_t v[4];
			v[0] = (int16_t)(ComputeCos(angle) >> 2);
			v[1] = (int16_t)(ComputeSin(angle) >> 2);
			v[2] = vz;
			v[3] = 0; // never written by the original (stack word)
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(p->vel);
			v[0] = (int16_t)(v[0] >> 3);
			v[1] = (int16_t)(v[1] >> 3);
			v[2] = 0;
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(p->pos);
			p->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
			p->vel[0] = (int16_t)(((int32_t)p->pos[0] - p->vel[0]) >> 4);
			p->vel[1] = (int16_t)(((int32_t)p->pos[1] - p->vel[1]) >> 4);
			p->vel[2] = (int16_t)(((int32_t)p->pos[2] - p->vel[2]) >> 4);
			angle += 0x800;
		}
	}

	// ------------------------------------------------------------------
	// Target (0x6EA920)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		GetEffectSpawnPosition(Caster(), 0x1D, 0x400, t->mouth);
		if (t->counter == 8) BdPlaySE3D(SOUND_Breath, 0, t->mouth);
		const uint32_t phase = (uint32_t)((int32_t)t->counter - 0x19);
		Mat4x3 m1;    // stream frame (basis * z scale at the mouth)
		int32_t d[3]; // aim - mouth
		if (phase < 0x32)
		{
			// 30 fps layer: see mag011_storm_breath_held.inc
			FX_HELD(held_note_stream(t);)
			uint8_t *h = AllocHeader(0x5C);
			StreamWobble(t->a, t->b, (uint8_t *)STREAM_VERTS);
			int16_t p0[4], p1[4];
			GetEffectSpawnPosition(t->ent[t->cur], 0xF1, 0, p0);
			GetEffectSpawnPosition(t->ent[t->next], 0xF1, 0, p1);
			// 30 fps layer: see mag011_storm_breath_held.inc
			FX_HELD(held_note_aim(p0, p1);)
			int16_t aim[3];
			StreamAim(p0, p1, t->t, aim);
			if (phase >= 5) SweepStep(t);
			StreamDraw(h, phase, nullptr, aim, t->mouth, &t->dist, &t->basis, &m1, d, STREAM_VERTS);
		}
		if ((uint32_t)(int32_t)t->counter < 0x18)
		{
			// the mouth layout, turned by the caster's facing
			if (t->counter == 0) DecodeModelPrimLayout(MODEL_Mouth, t->layout, 0x4A0);
			const int16_t angles[3] = { (int16_t)0xFF80, (int16_t)(*(const int16_t *)(Caster() + 0xE) + 0x800), 0 };
			Mat4x3 m2, m3;
			BuildRotationMatrixFromAngles(angles, &m2);
			m2.t[0] = t->mouth[0];
			m2.t[1] = t->mouth[1];
			m2.t[2] = t->mouth[2];
			ComposeAffineTransform(&Camera(), &m2, &m3);
			PrimArg arg;
			arg.m = &m3;
			arg.flags = FLAGS_Mouth;
			arg.morph = TexBase() + 0x24B8;
			// 30 fps layer: see mag011_storm_breath_held.inc
			FX_HELD(held_note_play(K_MOUTH, t, &arg);)
			prim::play((prim::Layout *)t->layout, PartCallback, (int)&arg, 0);
			// two smoke puffs around the mouth, 180 degrees apart
			CrtRand();
			int32_t angle = CrtRand();
			GteSetRotMatrix(&m2);
			GteSetTransVector(&m2);
			for (int k = 0; k < 2; k++)
			{
				SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_PuffTask);
				if (!p) continue;
				Memset32(&p->counter, 0, 5);
				int16_t v[4];
				v[0] = (int16_t)(ComputeCos(angle) >> 2);
				v[1] = (int16_t)(ComputeSin(angle) >> 2);
				v[2] = (int16_t)((CrtRand() & 0x3FF) + 0x400);
				v[3] = 0x0136; // the original's stack word (high half of FLAGS_Mouth)
				GteLoadV0(v);
				GteMVMVA_RotV0Tr();
				GteStoreIR123(p->pos);
				p->scale = (int16_t)((CrtRand() & 0x3FF) + 0x800);
				angle += 0x800;
			}
		}
		if (t->counter >= 0x1D)
		{
			// the cone layout at the mouth position of counter 29, along the stream
			Mat4x3 m2 = t->basis;
			if (t->counter == 0x1D)
			{
				memcpy(t->frozen, t->mouth, 8);
				DecodeModelPrimLayout(MODEL_Cone, t->layout, 0x140);
			}
			m2.t[0] = t->frozen[0];
			m2.t[1] = t->frozen[1];
			m2.t[2] = t->frozen[2];
			ComposeAffineTransform(&Camera(), &m2, &m2);
			PrimArg arg;
			arg.m = &m2;
			arg.flags = FLAGS_Cone;
			arg.morph = TexBase() + 0x24B8;
			// 30 fps layer: see mag011_storm_breath_held.inc
			FX_HELD(held_note_play(K_CONE, t, &arg);)
			prim::play((prim::Layout *)t->layout, PartCallback, (int)&arg, 0);
		}
		if ((uint32_t)((int32_t)t->counter - 0x23) < 0x1E)
		{
			// hit sparks on two vertices of the current target's model
			int32_t v = CrtRand();
			v = mul32(v, t->vc) >> 16;
			for (int k = 0; k < 2; k++)
			{
				SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_HitTask);
				if (!p) continue;
				Memset32(&p->counter, 0, 5);
				VertexPosition(t->ent[t->cur], v, p->pos);
				p->scale = (int16_t)((CrtRand() & 0x7FF) + 0xC00);
				p->vel[1] = 0;
				p->vel[0] = (int16_t)(shl32(d[0], 7) / t->dist);
				p->vel[2] = (int16_t)(shl32(d[2], 7) / t->dist);
				v += (int32_t)t->vc >> 1;
			}
		}
		const uint32_t sweep = (uint32_t)((int32_t)t->counter - 0x1E);
		if (sweep < 0x28)
		{
			// sparks (and debris) off the stream vertices above the ground
			GteSetRotMatrix(&m1);
			GteSetTransVector(&m1);
			const uint8_t *vtx = (const uint8_t *)STREAM_VERTS;
			for (int i = 0; i < STREAM_NVERTS; i++, vtx += 8)
			{
				int16_t w[3];
				GteLoadV0(vtx);
				GteMVMVA_RotV0Tr();
				GteStoreIR123(w);
				if (w[1] < 0) continue;
				if (CrtRand() < 0x800)
				{
					SpriteNode *p = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SparkTask);
					if (p)
					{
						Memset32(&p->counter, 0, 5);
						p->pos[0] = w[0];
						p->pos[1] = w[1];
						p->pos[2] = w[2];
						p->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
						p->vel[1] = -0x32;
						p->vel[0] = (int16_t)(shl32(d[0], 7) / t->dist);
						p->vel[2] = (int16_t)(shl32(d[2], 7) / t->dist);
					}
				}
				if (sweep >= 0x1E) continue;
				if (CrtRand() >= 0x800) continue;
				DebrisNode *e = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
				if (!e) continue;
				Memset32(&e->counter, 0, 9);
				e->pos[0] = w[0];
				e->pos[1] = w[1];
				e->pos[2] = w[2];
				e->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
				e->vel[0] = (int16_t)(shl32(d[0], 6) / t->dist);
				e->vel[1] = (int16_t)(-0x80 - (CrtRand() & 0x7F));
				e->vel[2] = (int16_t)(shl32(d[2], 6) / t->dist);
				e->spin[0] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
				e->spin[1] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
				e->spin[2] = (int16_t)((CrtRand() & 0x3FF) - 0x200);
			}
		}
		if ((uint32_t)((int32_t)t->counter - 0x14) < 5) SpawnMist(t, 0x100);
		if ((uint32_t)((int32_t)t->counter - 0x19) < 0x28) SpawnMist(t, 0x400);
		const uint32_t ring = (uint32_t)((int32_t)t->counter - 0x19);
		if (ring < 0x11 && (ring & 3) == 0)
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
			if (g)
			{
				Memset32(&g->pad0C, 0, 2);
				g->tn = t;
			}
		}
		const int16_t c = t->counter;
		t->a = (int16_t)(t->a - 100);
		t->b = (int16_t)(t->b + 100);
		if (c >= 0x4B) return TASK_END;
		t->counter = (int16_t)(c + 1);
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6EA5B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x26B8;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x126B8;
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
				uint8_t *dst = (uint8_t *)(TexBase() + 0x1B18);
				for (int i = 0; i < STREAM_NVERTS; i++, dst += 8)
				{
					memcpy(dst, (const void *)(STREAM_VERTS + i * 8), 8);
					*(int16_t *)(dst + 6) = (int16_t)(shl32(*(const int16_t *)(dst + 4), 12) / range);
				}
				InitTaskQueuePool(QTargets(), (void *)(TexBase() + 0x1614), 0x504, 1);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x614), 0x20, 0x80);
				InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x4D4), 0x14, 0x10);
				InitTaskQueuePool(QDebris(), (void *)(TexBase() + 0x1D4), 0x30, 0x10);
			}
			TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
			if (t)
			{
				Memset32(&t->counter, 0, 0x13E);
				t->count = (int16_t)Ctx()->actions[0].target_count;
				for (int i = 0; i < t->count; i++)
				{
					t->order[i] = (uint8_t)i;
					t->ent[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
				}
				// sorted by entity +0x1C, largest first
				for (int i = 0; i < t->count - 1; i++)
				{
					for (int j = i + 1; j < t->count; j++)
					{
						uint8_t *ei = t->ent[i], *ej = t->ent[j];
						if (*(const int16_t *)(ei + 0x1C) >= *(const int16_t *)(ej + 0x1C)) continue;
						t->ent[i] = ej;
						t->ent[j] = ei;
						const uint8_t o = t->order[i];
						t->order[i] = t->order[j];
						t->order[j] = o;
					}
				}
				t->next = t->count > 1 ? 1 : 0;
				// (count << 12) / 40
				const int32_t x = shl32(t->count, 12);
				int32_t q = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 4;
				q += (int32_t)((uint32_t)q >> 31);
				t->dt = (int16_t)q;
				t->vc = (int16_t)CountVertices(t->ent[0]);
			}
		}
		int a, b, c, d;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QSprites());
			c = ExecuteTaskQueue(QRings());
			d = ExecuteTaskQueue(QDebris());
		}
		else
		{
			a = b = c = d = (int)n;
		}
		if (r->started && a == 0 && b == 0 && c == 0 && d == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 10;
		return 0;
	}
}

	void register_mag011_storm_breath()
	{
		register_port(storm011::ORIG_RootTask, (void *)storm011::RootTask, "S011 RootTask", 11);
		register_port(storm011::ORIG_TargetTask, (void *)storm011::TargetTask, "S011 TargetTask", 11);
		register_port(storm011::ORIG_DebrisTask, (void *)storm011::DebrisTask, "S011 DebrisTask", 11);
		register_port(storm011::ORIG_RingTask, (void *)storm011::RingTask, "S011 RingTask", 11);
		register_port(storm011::ORIG_SparkTask, (void *)storm011::SparkTask, "S011 SparkTask", 11);
		register_port(storm011::ORIG_PuffTask, (void *)storm011::PuffTask, "S011 PuffTask", 11);
		register_port(storm011::ORIG_MistTask, (void *)storm011::MistTask, "S011 MistTask", 11);
		register_port(storm011::ORIG_HitTask, (void *)storm011::HitTask, "S011 HitTask", 11);
		// 30 fps layer: see mag011_storm_breath_held.inc
		FX_HELD(register_mag011_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag011_storm_breath_held.inc"
#endif
