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

// Effect 213: Storm Breath (enemy attack 233 of kernel.bin, Elnoyle c0m131; MAG_213_*). Module 213 is the same code as
// Storm Breath 11 (mag011_storm_breath.cpp; module 11 even calls module 213's 0x69EEE0 / 0x69FD50 / 0x6A01B0): this file
// is mag011_storm_breath.cpp with every module address moved to module 213's (instruction-aligned map of the two modules:
// globals -0x45FD0, data and code by block), plus module 213's one extra statement: its target task plays the sound
// 0x1181028 at its counter 1 (0x69FADE). The description below is 11's with 213's addresses.
//
// Structure (setup MAG_213_STORM_BREATH 0x69EAC0 -> 0x69EAF0, file loader 0x69EAD0 = the
// texture file named at 0x1181060 (mag212.tim); the setup keeps the CASTER's entity, starts
// camera animation 0x117AC50 and queues the TIM):
//   RootTask (0x69EB70) - alternates the packet arena (magic buffer + 0x26B8 / + 0x126B8); on
//     counter 1 (the counter stops at 10: one spawn only) sets up the pools and the breath-stream
//     vertex table (magic buffer + 0x1B18 = the 0x134 vertices of the stream model 0x117ADD8, word
//     +6 = z * 4096 / (z range)) and starts the target task: action 0's targets sorted by entity
//     +0x1C (descending), the breath sweeps over them in that order. Runs the target, sprite,
//     ring and debris queues; ends when all four are empty.
//   Target (0x69EF60) - node 0x504: every tick the caster's mouth (anchor 0x1D, flag 0x400);
//     sound at 8;
//     counters 25..74: the breath stream: the stream vertices wobbled (z rotation by angle A,
//       then a sine sway by angle B + the vertex's z phase) into the model's own vertex table
//       0x117ADE0 (exe data, rewritten every tick), aimed from the mouth at a point moving from the
//       current target's anchor 0xF1 to the next one's (t += (targets << 12) / 40 per tick from
//       counter 30; at t >= 1.0 damage to the current target, next target), stretched to the
//       distance (basis 0x50CBA0 from the aim vector, z scale = distance, grows in over 8 ticks,
//       fades out over the last 7), drawn by 0x69FD00 (the triangle / quad lists of module 213's
//       0x69FD50 / 0x6A01B0);
//     counters 0..23: prim-model layout 0x117EA88 at the mouth turned by the caster's facing, and
//       two smoke puffs per tick around the mouth; from counter 29: layout 0x117DD7C at the mouth
//       position frozen at 29, oriented along the stream;
//     counters 35..64: two hit sparks per tick on random vertices of the current target's model;
//     counters 30..69: for every stream vertex above y 0, 1/2 chance of a spark (and until 59 1/2
//       chance of a bouncing debris model);
//     counters 20..64: two mist sprites per tick blown out of the mouth; every 4 ticks from 25 to
//       41 an expanding ring along the stream; angles A -= 100, B += 100; ends at counter 75.
//   PartCallback (0x69FB30) - one object of either layout: ZYX rotation, record offset, composed
//     with the parameter block's matrix, scaled, vertex frames blended into magic buffer + 0x24B8
//     (64-vertex objects: exactly up to arena A), draw flags from the block's table.
//   Debris (0x6A0760, node 0x30), Ring (0x6A08D0, node 0x14), sprites Spark (0x6A0A50), Puff
//   (0x6A0AF0: GTE blend from its spawn point to the mouth over 12 frames), Mist (0x6A0BE0),
//   Hit (0x6A0C80) (nodes 0x20) - draw THEN update.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2510170..0x25101F8 (caster entity, texture file, magic buffer base, context,
// root pool, packet cursor 0x2510194, debris / ring / sprite / target / root queues; the stream's
// wobble translation reads the three words at 0x25101E8). Pools, the vertex table, the morph
// buffer and the packet arenas are in the magic buffer.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace storm213
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2510170); }          // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2510178); }         // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x251017C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2510194); }
	inline TaskQueue *QDebris() { return (TaskQueue *)0x2510198; }          // pool: + 0x1D4, 0x10 x 0x30
	inline TaskQueue *QRings() { return (TaskQueue *)0x25101A8; }           // pool: + 0x4D4, 0x10 x 0x14
	inline TaskQueue *QSprites() { return (TaskQueue *)0x25101B8; }         // pool: + 0x614, 0x80 x 0x20
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25101C8; }         // pool: + 0x1614, 1 x 0x504
	inline const Mat4x3 *WobbleTrans() { return (const Mat4x3 *)0x25101D4; } // translation = words at 0x25101E8
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x69EB70;
	static const uint32_t ORIG_TargetTask = 0x69EF60;
	static const uint32_t ORIG_DebrisTask = 0x6A0760;
	static const uint32_t ORIG_RingTask = 0x6A08D0;
	static const uint32_t ORIG_SparkTask = 0x6A0A50;
	static const uint32_t ORIG_PuffTask = 0x6A0AF0;
	static const uint32_t ORIG_MistTask = 0x6A0BE0;
	static const uint32_t ORIG_HitTask = 0x6A0C80;
	static const uint32_t MODEL_Stream = 0x117ADD8;     // stream model (vertex table at +8)
	static const uint32_t STREAM_VERTS = 0x117ADE0;     // 0x134 vertices of 8 bytes
	static const int STREAM_NVERTS = 0x134;
	static const uint32_t MODEL_Mouth = 0x117EA88;      // prim-model layout data (0x4A0-byte layout)
	static const uint32_t MODEL_Cone = 0x117DD7C;       // prim-model layout data (0x140-byte layout)
	static const uint32_t FLAGS_Mouth = 0x118102C;      // draw flags per object
	static const uint32_t FLAGS_Cone = 0x1181054;
	static const uint32_t MODEL_Debris = 0x117D6C8;     // prim model
	static const uint32_t MODEL_Ring = 0x117D360;       // prim model
	static const uint32_t SEQ_Spark = 0x117D8B8;        // sprite sequences
	static const uint32_t SEQ_Puff = 0x117DC38;
	static const uint32_t SEQ_Mist = 0x117DAF4;
	static const uint32_t SEQ_Hit = 0x117DA18;
	static const void *const SOUND_Breath = (const void *)0x1181028;
	static const void *const SOUND_Start = (const void *)0x1181028; // module 213 only: the target task's counter 1

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
	// ---- module 213's own model helpers (Storm Breath 11 and Biggs / Wedge death 110 call these originals; module 213
	// has them in its range: ported here)
	// 0x69EEE0 (Effect_CountMaskedModelVertices): vertices of the objects of a model (+4: object table) selected by a mask
	static int32_t CountMaskedVertices(uint32_t model, const uint32_t *mask)
	{
		const uint32_t base = *(const uint32_t *)(model + 4);
		const int32_t count = *(const int32_t *)base;
		const uint32_t m = *mask;
		int32_t total = 0;
		uint32_t p = base + 4;
		for (int32_t i = 0; i < count; i++, p += 4)
		{
			uint32_t d = base + *(const uint32_t *)p;
			if (!(m & (1u << (i & 31)))) continue;
			int32_t groups = *(const int16_t *)d;
			d += 2;
			if (groups <= 0) continue;
			do
			{
				const int32_t n = *(const int16_t *)(d + 2);
				d += 2;
				total += n;
				d += (uint32_t)(n * 6 + 2);
			} while (--groups);
		}
		return total;
	}
	// MAG_011_sub_6CE880: finds vertex +4 (counted down over the masked objects' vertex groups);
	// found: its bone-space position through the bone matrix at +8 (3 words), returns 1
	inline int32_t FindMaskedVertex(uint32_t model, void *state) { return fn<int32_t (__cdecl *)(uint32_t, void *)>(0x6CE880)(model, state); }

	inline void GteLoadV012w(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPTw() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAGw(uint32_t d) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(d); }
	inline void GteReadSXY012w(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteLoadV0w(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteRTPSw() { fn<void (__cdecl *)()>(0x45FAA0)(); }
	inline void GteReadSXY2w(uint32_t d) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(d); }
	inline void GteAVSZ4w() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZw(uint32_t d) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(d); }
	inline void GteSetFarColorw(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBCw(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }
	inline void GteDPCSw() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2w(uint32_t d) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(d); }
	inline void GteLoadRGB012w(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteDPCTw() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB012w(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }

	static inline bool OutX16(uint32_t a) { const int16_t v = *(const int16_t *)a; return v < 0 || v > 0xA00; }
	static inline bool OutY16(uint32_t a) { const int16_t v = *(const int16_t *)a; return v < 0 || v > 0x6C0; }
	// the GP0 0xE2 texture window word of the header's window at +o (x / y bytes +0 / +2, w / h words +4 / +6)
	static inline uint32_t TexWindow(uint32_t h, int32_t o)
	{
		uint32_t w = (uint32_t)(*(const uint8_t *)(h + o + 2) & 0xF8) | 0xFFFE2000u;
		w = (w << 5) | (uint32_t)(*(const uint8_t *)(h + o) & 0xF8);
		w = (w << 5) | (~((uint32_t)*(const uint16_t *)(h + o + 6) - 1) & 0xF8);
		w = (w << 2) | ((uint32_t)((int32_t)~((uint32_t)*(const uint16_t *)(h + o + 4) - 1) >> 3) & 0x1F);
		return w;
	}
	// 0x69FD50 (Effect_DrawModelTriangleList, GT = false: 0x18-byte records -> POLY_FT4) / 0x6A01B0 (Effect_DrawModelQuadList,
	// GT = true: 0x24-byte records -> POLY_GT4): header +4 far colour, +8 fade (IR0), +0xC u offset (wrapped by +0x1C), +0x10 /
	// +0x18 texture windows, +0x20 vertices (4 bytes apart), +0x24 the list (advanced), +0x30 OTZ, +0x34 FLAG; each quad
	// between a texture window packet (+0x10) and the reset one (+0x18), 0xC bytes each
	template<bool GT>
	static uint32_t QuadList(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t SZ = GT ? 0x34 : 0x28, XY1 = GT ? 0x14 : 0x10, XY2 = GT ? 0x20 : 0x18, XY3 = GT ? 0x2C : 0x20;
		const uint32_t UV1 = GT ? 0x18 : 0x14, UV2 = GT ? 0x24 : 0x1C, UV3 = GT ? 0x30 : 0x24;
		uint32_t rec = *(const uint32_t *)(h + 0x24);
		int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint32_t V = *(const uint32_t *)(h + 0x20);
		*(uint32_t *)(h + 0x24) = rec;
		for (; count > 0; count--, rec += GT ? 0x24 : 0x18)
		{
			GteLoadV012w(V + 4 * (uint32_t)*(const uint16_t *)(rec + 4), V + 4 * (uint32_t)*(const uint16_t *)(rec + 6), V + 4 * (uint32_t)*(const uint16_t *)(rec + 8));
			GteRTPTw();
			*(uint32_t *)pk = GT ? 0xC000000u : 0x9000000u;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(pk + UV1) = *(const uint32_t *)(rec + 0x10);
			const uint32_t w14 = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(pk + UV2) = w14;
			*(uint32_t *)(pk + UV3) = w14 >> 16;
			GteReadFLAGw(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012w(pk + 8, pk + XY1, pk + XY2);
			GteLoadV0w(V + 4 * (uint32_t)*(const uint16_t *)(rec + 0xA));
			GteRTPSw();
			uint32_t clip = 0;
			if (OutX16(pk + 8)) clip = 1;
			if (OutX16(pk + XY1)) clip |= 2;
			if (OutX16(pk + XY2)) clip |= 4;
			if (OutY16(pk + 0xA)) clip |= 0x10;
			if (OutY16(pk + XY1 + 2)) clip |= 0x20;
			if (OutY16(pk + XY2 + 2)) clip |= 0x40;
			GteReadSXY2w(pk + XY3);
			GteAVSZ4w();
			if (OutX16(pk + XY3)) clip |= 8;
			if (OutY16(pk + XY3 + 2)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZw(h + 0x30);
			if (*(const int32_t *)(h + 8) != 0)
			{
				GteSetFarColorw(*(const uint8_t *)(h + 4), *(const uint8_t *)(h + 5), *(const uint8_t *)(h + 6));
				if (GT)
				{
					GteLoadRGB012w(rec + 0x18, rec + 0x1C, rec + 0x20);
					GteSetIR0(*(const int32_t *)(h + 8));
					GteDPCTw();
					GteStoreRGB012w(pk + 0x10, pk + 0x1C, pk + 0x28);
					GteLoadRGBCw(pk + 4);
					GteDPCSw();
					GteStoreRGB2w(pk + 4);
				}
				else
				{
					GteLoadRGBCw(pk + 4);
					GteSetIR0(*(const int32_t *)(h + 8));
					GteDPCSw();
					GteStoreRGB2w(pk + 4);
				}
			}
			else if (GT)
			{
				*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(rec + 0x18);
				*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(rec + 0x1C);
				*(uint32_t *)(pk + 0x28) = *(const uint32_t *)(rec + 0x20);
			}
			const uint32_t du = *(const uint16_t *)(h + 0xC);
			const uint32_t u0 = *(const uint8_t *)(pk + 0xC) + du, u1 = *(const uint8_t *)(pk + UV1) + du;
			const uint32_t u2 = *(const uint8_t *)(pk + UV2) + du, u3 = *(const uint8_t *)(pk + UV3) + du;
			if ((int32_t)(u3 | u2 | u1 | u0) > 0xFF)
			{
				const uint8_t m = *(const uint8_t *)(h + 0x1C);
				*(uint8_t *)(pk + 0xC) = (uint8_t)(u0 - m);
				*(uint8_t *)(pk + UV1) = (uint8_t)(u1 - m);
				*(uint8_t *)(pk + UV2) = (uint8_t)(u2 - m);
				*(uint8_t *)(pk + UV3) = (uint8_t)(u3 - m);
			}
			else
			{
				*(uint8_t *)(pk + 0xC) = (uint8_t)u0;
				*(uint8_t *)(pk + UV1) = (uint8_t)u1;
				*(uint8_t *)(pk + UV2) = (uint8_t)u2;
				*(uint8_t *)(pk + UV3) = (uint8_t)u3;
			}
			const uint32_t bucket = ot + (uint32_t)(*(const int32_t *)(h + 0x30) >> shift) * 4;
			const uint32_t a = pk + SZ, b = a + 0xC;
			*(uint32_t *)a = 0x2000000;
			*(uint32_t *)(a + 4) = TexWindow(h, 0x10);
			*(uint32_t *)(a + 8) = 0;
			InsertPrimAutoDepth(bucket, (void *)a);
			InsertPrimAutoDepth(bucket, (void *)pk);
			*(uint32_t *)b = 0x2000000;
			*(uint32_t *)(b + 4) = TexWindow(h, 0x18);
			*(uint32_t *)(b + 8) = 0;
			InsertPrimAutoDepth(bucket, (void *)b);
			pk = b + 0xC;
		}
		*(uint32_t *)(h + 0x24) = rec;
		return pk;
	}
	inline uint32_t DrawTriangleList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return QuadList<false>((uint32_t)h, ot, mode, cursor); }
	inline uint32_t DrawQuadList(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return QuadList<true>((uint32_t)h, ot, mode, cursor); }

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
	// 0x69EE80: vertices of an entity's visible objects (+ all of its weapon's)
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

	// 0x6A06C0: position of vertex n of an entity's model (its weapon's after the body's); out gets
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
	// Prim-player callback (0x69FB30): one object of a layout
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

	// 0x69FD00: the stream model's triangle and quad lists
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
#include "mag213_storm_breath_held.h"
#endif

namespace ff8fx
{
namespace storm213
{
	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(held_note(K_SPARK, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Spark);
		return SpriteMove(p, 0xD);
	}

	static uint32_t __cdecl MistTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(held_note(K_MIST, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Mist);
		return SpriteMove(p, 0xC);
	}

	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(held_note(K_HIT, p, sizeof(SpriteNode));)
		SpriteDraw(p, SEQ_Hit);
		return SpriteMove(p, 8);
	}

	static uint32_t __cdecl PuffTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(held_note(K_PUFF, p, sizeof(SpriteNode));)
		PuffDraw(p, PuffWeight(p->counter));
		p->counter++;
		return p->counter < 0xC ? 0 : TASK_END;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(held_note(K_DEBRIS, d, sizeof(DebrisNode));)
		const bool scaled = d->counter >= 0x16;
		DebrisDraw(d, scaled, scaled ? DebrisScale(d->counter) : 0);
		return DebrisMove(d);
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
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
	// Target (0x69EF60)
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
			// 30 fps layer: see mag213_storm_breath_held.inc
			FX_HELD(held_note_stream(t);)
			uint8_t *h = AllocHeader(0x5C);
			StreamWobble(t->a, t->b, (uint8_t *)STREAM_VERTS);
			int16_t p0[4], p1[4];
			GetEffectSpawnPosition(t->ent[t->cur], 0xF1, 0, p0);
			GetEffectSpawnPosition(t->ent[t->next], 0xF1, 0, p1);
			// 30 fps layer: see mag213_storm_breath_held.inc
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
			// 30 fps layer: see mag213_storm_breath_held.inc
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
			// 30 fps layer: see mag213_storm_breath_held.inc
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
		if (t->counter == 1) BdPlaySE(SOUND_Start, 0, 0x80); // module 213 only (0x69FADE)
		const int16_t c = t->counter;
		t->a = (int16_t)(t->a - 100);
		t->b = (int16_t)(t->b + 100);
		if (c >= 0x4B) return TASK_END;
		t->counter = (int16_t)(c + 1);
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x69EB70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag213_storm_breath_held.inc
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

	void register_mag213_storm_breath()
	{
		register_port(storm213::ORIG_RootTask, (void *)storm213::RootTask, "213 RootTask", 213);
		register_port(storm213::ORIG_TargetTask, (void *)storm213::TargetTask, "213 TargetTask", 213);
		register_port(storm213::ORIG_DebrisTask, (void *)storm213::DebrisTask, "213 DebrisTask", 213);
		register_port(storm213::ORIG_RingTask, (void *)storm213::RingTask, "213 RingTask", 213);
		register_port(storm213::ORIG_SparkTask, (void *)storm213::SparkTask, "213 SparkTask", 213);
		register_port(storm213::ORIG_PuffTask, (void *)storm213::PuffTask, "213 PuffTask", 213);
		register_port(storm213::ORIG_MistTask, (void *)storm213::MistTask, "213 MistTask", 213);
		register_port(storm213::ORIG_HitTask, (void *)storm213::HitTask, "213 HitTask", 213);
		// 30 fps layer: see mag213_storm_breath_held.inc
		FX_HELD(register_mag213_held();)
	}
	FX_LAB_MODULE(register_mag213_storm_breath);
}

#ifdef FF8_FX_HELD
#include "mag213_storm_breath_held.inc"
#endif
