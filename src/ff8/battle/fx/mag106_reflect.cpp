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

// Effect 106: Reflect (spell, MAG_106_*). The same effect as the "ruby" part of Carbuncle's
// Ruby Light (mag278: RubyTask 0x6822F0 and its stones / rings / wall), in its own copy.
//
// Structure (setup MAG_106_REFLECT 0x6D2AE0 -> _Init 0x6D2AF0, file loader 0x6D2AC0 = the texture
// file named at 0x13009B4 into 0x2521F70). _Init keeps the cast context, sets up the root pool and
// the barrier / beam / hexagon / ring pools (module .data), measures the reflect wall model
// 0x12F9B80 (lowest vertex z -> 0x2523C7E, z span + 1/16 -> 0x2523C7C), starts camera animation
// 0x12FC200 and queues the TIM (unless cast flag bit0), seeds the shared LCG 0x2557098 with rand().
//   DirectorTask (0x6D2C00) - alternates the packet arena (0x2523C98 / 0x2533C98); on counter 0
//     of a 10-tick cycle starts a barrier on the first target of the next action, unless a
//     barrier of 35 ticks or less still runs on that target (the cycle then restarts), with 8
//     hexagons around it; runs the barrier, hexagon and ring queues and the beam list; ends when
//     all four are empty.
//   Barrier (0x6D3160) - node 0x5C, 41 ticks: the target's Y rotation scaled by its size composed
//     with the camera; ticks 0..19: the hexagons' centre moves along the wall model's vertex list
//     and 12 beams are "spawned" (see AppendByValue: never run); ticks 5..39: the reflect wall
//     (0x12F9B80, drawn by 0x6D3490 = textured quads, gouraud triangles, gouraud quads, all with
//     a V scroll) grows over 5..25 (vertices of z in [base, base + span * (t + 1) / 20]) and fades
//     to black over 26..39; rings at 5, 11, 17; sound at 0, damage at 35.
//   Hexagon (0x6D2F30) - node 0x1C, 17 ticks: prim model 0x12FBAA0 at `angle` around the centre,
//     Z-rotated by -angle, tilted about X, squashed in x and faded from tick 11.
//   Ring (0x6D3AF0) - node 0x1C, 21 ticks: prim model 0x12FBB78 at a height above the target,
//     scaled up (braking speed), fade sin(t * 1024 / 20).
//   Beam (0x6D3C50) - node 0x1C, sprite sequence 0x12F99D4 through 0x683490: vanilla dead code
//     (the beam list is never linked, see AppendByValue).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521BB8..0x2543CF0 (pools, texture file, context, wall base / span, root pool,
// beam header, both packet arenas, packet cursor 0x2543C98, queues). The shared effect LCG seed
// 0x2557098 (MAG_106_sub_7059E0) is outside.
// Helpers of other modules called here are local copies: MAG_103_sub_6DBCC0 (yaw x scale),
// MAG_011_sub_6CF070 (Z rotation), MAG_003_sub_6D5940 (X rotation), Thunder_InitBoltTask
// 0x6DA380 (dword fill), MAG_106_sub_705A00 (append by value), MAG_106_sub_6827B0 /
// MAG_106_sub_682D00 (Carbuncle's wall renderers, shared with this module's 0x6D34E0).

#include "mag_common.h"

namespace ff8fx
{
namespace reflect106
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2523B78); }
	inline int16_t &WallSpan() { return var<int16_t>(0x2523C7C); }          // z span of the wall model + 1/16
	inline int16_t &WallBase() { return var<int16_t>(0x2523C7E); }          // lowest vertex z of the wall model
	inline uint8_t *&BeamHeader() { return var<uint8_t *>(0x2523C94); }     // sprite header of the beam list (per tick)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2543C98); }
	inline TaskQueue *QBarriers() { return (TaskQueue *)0x2543CB0; }        // pool 0x2521BB8, 3 x 0x5C
	inline TaskQueue *QBeams() { return (TaskQueue *)0x2543CC0; }           // pool 0x2521F78, 0x100 x 0x1C (own executor)
	inline TaskQueue *QHexagons() { return (TaskQueue *)0x2543CD0; }        // pool 0x2521CD0, 0x18 x 0x1C
	inline TaskQueue *QRings() { return (TaskQueue *)0x2543CE0; }           // pool 0x2523B80, 9 x 0x1C
	static const uint32_t POOL_Barriers = 0x2521BB8;
	static const uint32_t ARENA_0 = 0x2523C98;
	static const uint32_t ARENA_1 = 0x2533C98;

	static const uint32_t ORIG_DirectorTask = 0x6D2C00;
	static const uint32_t ORIG_HexagonTask = 0x6D2F30;
	static const uint32_t ORIG_BarrierTask = 0x6D3160;
	static const uint32_t ORIG_RingTask = 0x6D3AF0;
	static const uint32_t ORIG_BeamTask = 0x6D3C50;
	static const uint32_t MODEL_Wall = 0x12F9B80;     // +4 vertex count, +8 vertices (8 bytes: x, y, z, pad)
	static const uint32_t MODEL_Hexagon = 0x12FBAA0;
	static const uint32_t MODEL_Ring = 0x12FBB78;
	static const uint32_t SEQ_Beam = 0x12F99D4;
	static const uint32_t TABLE_SinCos = 0x12FC9B4;   // 0x1000 x { s16 sin, s16 cos }
	static const void *const SOUND_Reflect = (const void *)0x12FC9B0;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }  // MAG_106_sub_7059E0: (125 s + 14) & 0x7FFF
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatMul56C220(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }           // a = a x b (3x3 + pad)
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }        // GTE_MatrixMultiply: b = a x b (3x3 + pad)
	// MAG_106_sub_683490 (Carbuncle's beam sprites, reached only by the dead beam task)
	inline uint32_t DrawBeamSprites(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x683490)(h, ot, mode, cursor); }
	// software GTE
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }  // GTE_ReadOTZ
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }  // someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }            // set_unk_1CA8A28
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                     // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct DirectorNode // root pool 0x2523C80, 1 x 0x14
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..9, a barrier spawn at 0
		uint8_t action;   // +0x0E next action
		uint8_t pad0F;
		uint32_t arena;   // +0x10 packet arena parity
	};
	struct BarrierNode // pool 0x2521BB8, 3 x 0x5C
	{
		TaskNode hdr;
		int16_t frame;      // +0x0C
		int16_t action;     // +0x0E action index
		uint8_t *entity;    // +0x10 first target of the action
		Mat4x3 model;       // +0x14 Y rotation (entity +0x0E) x min(size << 2, 0x1000); translation = the
		                    //   middle of effect points 0xF0 / 0xF1 (set at the spawn)
		Mat4x3 view;        // +0x34 camera x model
		int16_t center[3];  // +0x54 centre of the hexagons (x = 0, y / z along the wall model's vertices)
		int16_t pad5A;
	};
	struct HexagonNode // pool 0x2521CD0, 0x18 x 0x1C
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t tilt;     // +0x0E X rotation
		int16_t angle;    // +0x10 position angle around the centre (and -Z rotation)
		int16_t dangle;   // +0x12
		int16_t dtilt;    // +0x14
		int16_t pad16;
		BarrierNode *parent; // +0x18
	};
	struct RingNode // pool 0x2523B80, 9 x 0x1C
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t height;   // +0x0E
		int16_t scale;    // +0x10
		int16_t speed;    // +0x12
		int16_t accel;    // +0x14
		int16_t pad16;
		BarrierNode *parent; // +0x18
	};
	struct BeamNode // pool 0x2521F78, 0x100 x 0x1C (never run in vanilla)
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C sequence frame
		int16_t angle;    // +0x0E
		int16_t scale;    // +0x10
		int16_t y;        // +0x12
		int16_t vy;       // +0x14
		int16_t pad16;
		BarrierNode *parent; // +0x18
	};
#pragma pack(pop)
	static_assert(sizeof(DirectorNode) == 0x14 && sizeof(BarrierNode) == 0x5C, "Reflect nodes");
	static_assert(sizeof(HexagonNode) == 0x1C && sizeof(RingNode) == 0x1C && sizeof(BeamNode) == 0x1C, "Reflect nodes");

	// ------------------------------------------------------------------
	// Local copies of helpers of other modules
	// ------------------------------------------------------------------
	// Thunder_InitBoltTask (0x6DA380): fill `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (; count; count--) *d++ = value;
	}

	// MAG_103_sub_6DBCC0: yaw rotation scaled by s (3x3 only; the yaw is the low word of arg 1)
	static void YawScaleMatrix(int16_t yaw, Mat4x3 *m, int16_t s)
	{
		int32_t sn = mul32(ComputeSin(yaw), s) >> 12;
		int32_t cs = mul32(ComputeCos(yaw), s) >> 12;
		m->m[0][2] = (int16_t)sn;
		m->m[2][0] = (int16_t)-sn;
		m->m[1][1] = s;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	// MAG_011_sub_6CF070: rotation about z (3x3 only: pad and translation not written)
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

	// MAG_003_sub_6D5940: rotation about the X axis (translation and pad untouched)
	static void XRotation(int16_t angle, Mat4x3 *m)
	{
		int32_t s = ComputeSin(angle);
		int32_t c = ComputeCos(angle);
		m->m[1][2] = (int16_t)-s;
		m->m[2][1] = (int16_t)s;
		m->m[0][0] = 0x1000;
		m->m[0][1] = 0;
		m->m[0][2] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[2][0] = 0;
		m->m[2][2] = (int16_t)c;
	}

	// MAG_106_sub_705A00: "append" a node to a queue passed BY VALUE. It links the new node
	// only behind the copy's tail and never writes the real header back.
	// VANILLA DEAD CODE: the beam list 0x2543CC0 is only ever filled through this function,
	// its head stays 0 (only _Init's InitTaskQueuePool and the executor 0x6D2EE0 write it) and
	// its tail is the executor's last survivor (0 on an empty list), so every beam node is
	// allocated but never linked: BeamTask never runs, and the 256-node pool fills up (after
	// which no more are made, and no LCG number is drawn for them). Reproduced as is.
	static TaskNode *AppendByValue(TaskQueue q, uint32_t task_fn)
	{
		uint8_t *p = (uint8_t *)q.pool;
		for (int32_t i = 0; i < (int32_t)q.capacity; i++, p += q.node_size)
		{
			if (*p & 1) continue;
			TaskNode *t = (TaskNode *)p;
			*p |= 1;
			t->func = (TaskFn)task_fn;
			t->sequence = 0;
			t->next = nullptr;
			if (q.tail) q.tail->next = t;
			return t;
		}
		return nullptr;
	}

	// ------------------------------------------------------------------
	// Module helpers
	// ------------------------------------------------------------------
	// MAG_106_sub_6D30F0: (x, y) of src rotated by angle (module sin / cos table), z copied
	static void Rot2D(const int16_t *src, int16_t *dst, int32_t angle)
	{
		const uint32_t a = (uint32_t)angle & 0xFFF;
		const int32_t c = *(const int16_t *)(TABLE_SinCos + a * 4 + 2);
		const int32_t ns = -(int32_t)*(const int16_t *)(TABLE_SinCos + a * 4);
		dst[0] = (int16_t)((int32_t)((uint32_t)mul32(src[0], c) - (uint32_t)mul32(src[1], ns)) >> 12);
		dst[1] = (int16_t)((int32_t)((uint32_t)mul32(src[1], c) + (uint32_t)mul32(src[0], ns)) >> 12);
		dst[2] = src[2];
	}

	// MAG_106_sub_6D2DE0: middle of the entity's effect points 0xF1 and 0xF0
	static void EffectCentre(uint8_t *entity, int16_t *out)
	{
		int16_t a[4], b[4];
		GetEffectSpawnPosition(entity, 0xF1, 0, a);
		GetEffectSpawnPosition(entity, 0xF0, 0, b);
		out[0] = (int16_t)(((int32_t)b[0] + a[0]) >> 1);
		out[1] = (int16_t)(((int32_t)b[1] + a[1]) >> 1);
		out[2] = (int16_t)(((int32_t)b[2] + a[2]) >> 1);
	}

	// hexagon centre at barrier frame f (0..19): the wall model's vertices f * 3 / 4 and the next,
	// blended by the fraction (y +2, z +4 of the 8-byte vertices)
	static void BarrierCenter(int32_t f, int16_t out[3])
	{
		const uint32_t t = ((uint32_t)f * 15u << 12) / 20u;
		const int32_t idx = (int32_t)t >> 12;
		const int32_t w = (int32_t)(t & 0xFFF);
		const uint32_t v = MODEL_Wall + 8 + 8 * idx;
		int16_t a = var<int16_t>(v + 2);
		out[1] = (int16_t)((mul32((int32_t)var<int16_t>(v + 0xA) - a, w) >> 12) + a);
		out[0] = 0;
		a = var<int16_t>(v + 4);
		out[2] = (int16_t)((mul32((int32_t)var<int16_t>(v + 0xC) - a, w) >> 12) + a);
	}

	// ------------------------------------------------------------------
	// The reflect wall renderer (MAG_106_sub_6D3490) and its three primitive passes. Header h
	// (0x60 bytes): +0 model, +4..+6 far colour, +8 fog (IR0, 0 = none), +0x0C / +0x0E U / V
	// scroll, +0x10 / +0x18 texture windows around each primitive, +0x1C / +0x1E U / V wrap,
	// +0x20 vertices, +0x24 primitive list cursor, +0x30 OTZ, +0x34 GTE FLAG, +0x38..+0x57 the
	// primitive's vertices, +0x5C / +0x5E z range of the vertices drawn.
	// ------------------------------------------------------------------
	// GP0(E2) texture window word of the 4 halves at tw (+0 / +2 bytes, +4 / +6 words)
	static uint32_t TexWindow(const uint8_t *tw)
	{
		uint32_t e = (uint32_t)(tw[2] & 0xF8) | 0xFFFE2000u;
		e <<= 5;
		e |= (uint32_t)(tw[0] & 0xF8);
		e <<= 5;
		e |= ~((uint32_t)*(const uint16_t *)(tw + 6) - 1) & 0xF8;
		e <<= 2;
		e |= (uint32_t)((int32_t)~((uint32_t)*(const uint16_t *)(tw + 4) - 1) >> 3) & 0x1F;
		return e;
	}

	// screen coordinate out of range: x 0..0xA00, y 0..0x6C0
	static inline bool OutX(const uint8_t *xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0x6C0; }

	// the texture window prims around a primitive: window h + 0x10 inserted first, then the
	// primitive, then the window h + 0x18
	static uint8_t *Windows(const uint8_t *h, uint32_t bucket, uint8_t *pk, uint8_t *p2)
	{
		*(uint32_t *)p2 = 0x2000000;
		*(uint32_t *)(p2 + 4) = TexWindow(h + 0x10);
		*(uint32_t *)(p2 + 8) = 0;
		InsertPrimAutoDepth(bucket, p2);
		InsertPrimAutoDepth(bucket, pk);
		uint8_t *p3 = p2 + 0xC;
		*(uint32_t *)p3 = 0x2000000;
		*(uint32_t *)(p3 + 4) = TexWindow(h + 0x18);
		*(uint32_t *)(p3 + 8) = 0;
		InsertPrimAutoDepth(bucket, p3);
		return p3 + 0xC;
	}

	// vertex `index` (8 bytes at vtx + index * 4) into the header slot; false when its z word is
	// outside [h + 0x5C, h + 0x5E] (the primitive is skipped)
	static bool FetchVertex(uint8_t *h, int slot, const uint8_t *vtx, uint16_t index)
	{
		*(uint32_t *)(h + slot) = *(const uint32_t *)(vtx + index * 4);
		*(uint32_t *)(h + slot + 4) = *(const uint32_t *)(vtx + index * 4 + 4);
		const int16_t z = *(const int16_t *)(h + slot + 4);
		return z >= *(const int16_t *)(h + 0x5C) && z <= *(const int16_t *)(h + 0x5E);
	}

	// adds the scroll word to a byte of each vertex; when any sum exceeds 0xFF all of them wrap
	// by the wrap byte (b[] = the byte offsets, the last one is written last)
	static void Scroll(uint8_t *pk, const int *b, int n, uint32_t scroll, uint8_t wrap)
	{
		uint32_t v[4], any = 0;
		for (int i = 0; i < n; i++)
		{
			v[i] = pk[b[i]] + scroll;
			any |= v[i];
		}
		if ((int32_t)any > 0xFF)
			for (int i = 0; i < n; i++) v[i] = (uint8_t)(v[i] - wrap);
		for (int i = 0; i < n; i++) pk[b[i]] = (uint8_t)v[i];
	}

	// MAG_106_sub_6827B0 (Carbuncle's module, local copy): textured quads (records of 0x18 bytes:
	// code / colour, 4 vertex indices, UVs), fog toward the far colour
	static uint32_t MeshFT4(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pl = *(uint8_t **)(h + 0x24);
		const int32_t count = *(const int32_t *)pl;
		pl += 4;
		const uint8_t *vtx = *(uint8_t **)(h + 0x20);
		*(uint8_t **)(h + 0x24) = pl;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return (uint32_t)pk;
		static const int U[4] = { 0xC, 0x14, 0x1C, 0x24 }, V[4] = { 0xD, 0x15, 0x1D, 0x25 };
		for (uint32_t left = (uint32_t)count; left; left--, pl += 0x18)
		{
			if (!FetchVertex(h, 0x38, vtx, *(const uint16_t *)(pl + 4))) continue;
			if (!FetchVertex(h, 0x40, vtx, *(const uint16_t *)(pl + 6))) continue;
			if (!FetchVertex(h, 0x48, vtx, *(const uint16_t *)(pl + 8))) continue;
			if (!FetchVertex(h, 0x50, vtx, *(const uint16_t *)(pl + 0xA))) continue;
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)pk = 0x9000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)pl;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(pl + 0xC);
			*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(pl + 0x10);
			*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(pl + 0x14);
			*(uint32_t *)(pk + 0x24) = *(const uint32_t *)(pl + 0x14) >> 16;
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			uint32_t clip = 0;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(h + 0x50);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 0xA)) clip |= 0x10;
			if (OutY(pk + 0x12)) clip |= 0x20;
			if (OutY(pk + 0x1A)) clip |= 0x40;
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x22)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteStoreOTZ(h + 0x30);
			if (*(const int32_t *)(h + 8))
			{
				GteSetFarColour(h[4], h[5], h[6]);
				GteLoadRGBC(pk + 4);
				GteSetIR0(*(const int32_t *)(h + 8));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			Scroll(pk, U, 4, *(const uint16_t *)(h + 0xC), h[0x1C]);
			Scroll(pk, V, 4, *(const uint16_t *)(h + 0xE), h[0x1E]);
			const uint32_t bucket = ot + (uint32_t)(*(const int32_t *)(h + 0x30) >> mode) * 4;
			pk = Windows(h, bucket, pk, pk + 0x28);
		}
		*(uint8_t **)(h + 0x24) = pl;
		return (uint32_t)pk;
	}

	// MAG_106_sub_682D00 (Carbuncle's module, local copy): gouraud textured triangles (records of
	// 0x1C bytes: code / colour 0, 3 vertex indices, UV 2, UV 0 / CLUT, UV 1 / page, colours 1, 2);
	// fog: colours 1, 2 and 0 through DPCT
	static uint32_t MeshGT3(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pl = *(uint8_t **)(h + 0x24);
		const int32_t count = *(const int32_t *)pl;
		pl += 4;
		const uint8_t *vtx = *(uint8_t **)(h + 0x20);
		*(uint8_t **)(h + 0x24) = pl;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return (uint32_t)pk;
		static const int U[3] = { 0xC, 0x18, 0x24 }, V[3] = { 0xD, 0x19, 0x25 };
		for (uint32_t left = (uint32_t)count; left; left--, pl += 0x1C)
		{
			if (!FetchVertex(h, 0x38, vtx, *(const uint16_t *)(pl + 4))) continue;
			if (!FetchVertex(h, 0x40, vtx, *(const uint16_t *)(pl + 6))) continue;
			if (!FetchVertex(h, 0x48, vtx, *(const uint16_t *)(pl + 8))) continue;
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)pk = 0x9000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)pl;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(pl + 0xC);
			*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(pl + 0x10);
			*(uint32_t *)(pk + 0x24) = *(const uint32_t *)(pl + 8) >> 16;
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			uint32_t clip = 0;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 0xA)) clip |= 0x10;
			if (OutY(pk + 0x16)) clip |= 0x20;
			if (OutY(pk + 0x22)) clip |= 0x40;
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteStoreOTZ(h + 0x30);
			if (*(const int32_t *)(h + 8))
			{
				GteSetFarColour(h[4], h[5], h[6]);
				GteLoadRGB012(pl + 0x14, pl + 0x18, pk + 4);
				GteSetIR0(*(const int32_t *)(h + 8));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(pl + 0x14);
				*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(pl + 0x18);
			}
			Scroll(pk, U, 3, *(const uint16_t *)(h + 0xC), h[0x1C]);
			Scroll(pk, V, 3, *(const uint16_t *)(h + 0xE), h[0x1E]);
			const uint32_t bucket = ot + (uint32_t)(*(const int32_t *)(h + 0x30) >> mode) * 4;
			pk = Windows(h, bucket, pk, pk + 0x28);
		}
		*(uint8_t **)(h + 0x24) = pl;
		return (uint32_t)pk;
	}

	// MAG_106_sub_6D34E0: gouraud textured quads (records of 0x24 bytes, colours at +0x18..+0x20),
	// fog toward the far colour
	static uint32_t MeshGT4(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pl = *(uint8_t **)(h + 0x24);
		const int32_t count = *(const int32_t *)pl;
		pl += 4;
		const uint8_t *vtx = *(uint8_t **)(h + 0x20);
		*(uint8_t **)(h + 0x24) = pl;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return (uint32_t)pk;
		static const int U[4] = { 0xC, 0x18, 0x24, 0x30 }, V[4] = { 0xD, 0x19, 0x25, 0x31 };
		for (uint32_t left = (uint32_t)count; left; left--, pl += 0x24)
		{
			if (!FetchVertex(h, 0x38, vtx, *(const uint16_t *)(pl + 4))) continue;
			if (!FetchVertex(h, 0x40, vtx, *(const uint16_t *)(pl + 6))) continue;
			if (!FetchVertex(h, 0x48, vtx, *(const uint16_t *)(pl + 8))) continue;
			if (!FetchVertex(h, 0x50, vtx, *(const uint16_t *)(pl + 0xA))) continue;
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)pk = 0xC000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)pl;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(pl + 0xC);
			*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(pl + 0x10);
			*(uint32_t *)(pk + 0x24) = *(const uint32_t *)(pl + 0x14);
			*(uint32_t *)(pk + 0x30) = *(const uint32_t *)(pl + 0x14) >> 16;
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			uint32_t clip = 0;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(h + 0x50);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 0xA)) clip |= 0x10;
			if (OutY(pk + 0x16)) clip |= 0x20;
			if (OutY(pk + 0x22)) clip |= 0x40;
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			if (OutX(pk + 0x2C)) clip |= 8;
			if (OutY(pk + 0x2E)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteStoreOTZ(h + 0x30);
			if (*(const int32_t *)(h + 8))
			{
				GteSetFarColour(h[4], h[5], h[6]);
				GteLoadRGB012(pl + 0x18, pl + 0x1C, pl + 0x20);
				GteSetIR0(*(const int32_t *)(h + 8));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(pl + 0x18);
				*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(pl + 0x1C);
				*(uint32_t *)(pk + 0x28) = *(const uint32_t *)(pl + 0x20);
			}
			Scroll(pk, U, 4, *(const uint16_t *)(h + 0xC), h[0x1C]);
			Scroll(pk, V, 4, *(const uint16_t *)(h + 0xE), h[0x1E]);
			const uint32_t bucket = ot + (uint32_t)(*(const int32_t *)(h + 0x30) >> mode) * 4;
			pk = Windows(h, bucket, pk, pk + 0x34);
		}
		*(uint8_t **)(h + 0x24) = pl;
		return (uint32_t)pk;
	}

	// MAG_106_sub_6D3490: the wall model's textured quads, gouraud triangles, gouraud quads
	static uint32_t RenderWall(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *model = *(uint8_t **)h;
		*(uint8_t **)(h + 0x20) = model + 8;
		*(uint8_t **)(h + 0x24) = model + *(const int32_t *)model + 0xC;
		cursor = MeshFT4(h, ot, mode, cursor);
		*(uint8_t **)(h + 0x24) += 8;
		cursor = MeshGT3(h, ot, mode, cursor);
		return MeshGT4(h, ot, mode, cursor);
	}

	// ------------------------------------------------------------------
	// Draw helpers
	// ------------------------------------------------------------------
	// wall parameters at wall step e = barrier frame - 5 (0..34): top of the z range (+0x5E)
	// and fog (+0x08)
	static int32_t WallHeight(int32_t e)
	{
		if ((uint32_t)e <= 0x14) return (int16_t)((int16_t)((uint32_t)mul32(e + 1, WallSpan()) / 20u) + WallBase());
		return (int16_t)(WallSpan() + WallBase());
	}
	static int32_t WallFade(int32_t e)
	{
		if ((uint32_t)e <= 0x14) return 0;
		return (int32_t)((uint32_t)shl32(e + 0xFFFEC, 12) / 15u);
	}
	// V scroll of the wall at 8 x barrier frame (steps of 8 texels, 0x80 wrap)
	static uint16_t WallScroll(int32_t f8) { return (uint16_t)((uint8_t)(0u - (uint8_t)f8) & 0x7F); }

	// 0x6D32BE..0x6D339C: the reflect wall through the barrier's view (header freed by the caller:
	// the ring spawn reads its +0x5E)
	static uint8_t *WallDraw(uint16_t scroll, int32_t height, int32_t fade, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)h = MODEL_Wall;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x0C) = 0;
		*(uint16_t *)(h + 0x0E) = scroll;
		*(int16_t *)(h + 0x5C) = WallBase();
		h[6] = 0;
		h[5] = 0;
		h[4] = 0;
		*(int32_t *)(h + 8) = fade;
		*(int16_t *)(h + 0x5E) = (int16_t)height;
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		PacketCursor() = RenderWall(h, RenderOT(), 2, PacketCursor());
		return h;
	}

	// fade parameters of a hexagon at its tick c (0x6D2FA2..0x6D2FF4): fade and x squash from 11
	static void HexagonFade(int32_t c, int32_t &fade, int32_t &sx)
	{
		if (c > 0xA)
		{
			const int32_t q = shl32(c - 0xA, 12) / 6;
			sx = (int16_t)(0x1000 - (int16_t)q);
			fade = (q >> 1) + 0x800;
		}
		else
		{
			sx = 0x1000;
			fade = 0x800;
		}
	}

	// 0x6D2F30..0x6D30A8: hexagon at `angle` around the centre, Z-rotated by -angle, squashed in
	// x, tilted about X, at the rotated point
	static void HexagonDraw(const int16_t *center, int16_t angle, int16_t tilt, int32_t fade, int16_t sx, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t p[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		Rot2D(center, p, angle);
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		Mat4x3 X = {};  // its translation = the rotated point in view space
		GteReadMAC123(X.t);
		*(uint32_t *)(h + 0x1C) = 0xF3;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		Mat4x3 S = {};  // pad / translation: vanilla stack garbage (never read)
		S.m[0][0] = sx;
		S.m[1][1] = 0x1000;
		S.m[2][2] = 0x1000;
		Mat4x3 R = {};
		ZRotation((int16_t)-angle, &R);
		MatMul56C220(&R, &S);
		MatrixMultiply(view, &R);
		XRotation(tilt, &X);
		MatrixMultiply(&R, &X);
		GteSetRotMatrix(&X);
		GteSetTransVector(&X);
		*(uint32_t *)h = MODEL_Hexagon;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static int32_t RingFade(int32_t c) { return ComputeSin(shl32(c, 10) / 20); }

	// 0x6D3AF0..0x6D3BF9: ring at `height` above the barrier's origin, scaled
	static void RingDraw(int16_t height, int16_t scale, int32_t fade, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t V[4] = { 0, 0, height, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(V);
		GteMVMVA_RotV0Tr();
		Mat4x3 T = {};
		GteReadMAC123(T.t);
		GteSetTransVector(&T);
		Mat4x3 S = {};
		S.m[0][0] = scale;
		S.m[1][1] = scale;
		S.m[2][2] = scale;
		MatMul56C220(&S, view);
		GteSetRotMatrix(&S);
		*(uint32_t *)h = MODEL_Ring;
		*(int32_t *)(h + 0xC) = fade;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag106_reflect_held.h"
#endif

namespace ff8fx
{
namespace reflect106
{
	// ------------------------------------------------------------------
	// Hexagon (0x6D2F30): 17 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl HexagonTask(TaskNode *n)
	{
		HexagonNode *x = (HexagonNode *)n;
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_hexagon(x);)
		int32_t fade, sx;
		HexagonFade(x->counter, fade, sx);
		HexagonDraw(x->parent->center, x->angle, x->tilt, fade, (int16_t)sx, &x->parent->view);
		if (x->counter >= 0x10) return TASK_END;
		x->angle = (int16_t)(x->angle + x->dangle);
		x->tilt = (int16_t)(x->tilt + x->dtilt);
		x->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x6D3AF0): 21 ticks, the growth speed brakes to 0x10
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_ring(g);)
		RingDraw(g->height, g->scale, RingFade(g->counter), &g->parent->view);
		if (g->counter >= 0x14) return TASK_END;
		g->speed = (int16_t)(g->speed + g->accel);
		if (g->speed < 0x10) g->speed = 0x10;
		g->scale = (int16_t)(g->scale + g->speed);
		g->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Beam (0x6D3C50): 16-frame sprite sequence 0x12F99D4 rising from the hexagons' centre once
	// the barrier passes frame 20. VANILLA DEAD CODE (see AppendByValue): never runs.
	// ------------------------------------------------------------------
	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		uint8_t *h = BeamHeader();
		BarrierNode *parent = b->parent;
		int16_t out[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage
		if (parent->frame >= 0x14)
		{
			if (b->vy == 0) b->vy = (int16_t)((SharedRand() & 0x3F) + 0x80);
			else
			{
				b->vy = (int16_t)(b->vy - 0x10);
				if (b->vy < 1) b->vy = 1;
			}
			int16_t P[4] = { parent->center[0], parent->center[1], parent->center[2], parent->pad5A };
			b->y = (int16_t)(b->y + b->vy);
			P[2] = b->y;
			Rot2D(P, out, b->angle);
		}
		else
		{
			Rot2D(parent->center, out, b->angle);
			b->y = parent->center[2];
		}
		GteSetRotMatrixCtrl(&parent->view);
		GteSetTransVectorCtrl(&parent->view);
		GteLoadV0(out);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		*(int16_t *)(h + 0x94) = b->scale;
		*(int16_t *)(h + 0x8C) = b->scale;
		*(int16_t *)(h + 0x84) = b->scale;
		uint8_t *seq = *(uint8_t **)h;
		uint8_t *fr = seq + *(const uint16_t *)(seq + 2 * b->counter + 0xA);
		*(uint8_t **)(h + 0x2C) = fr;
		*(uint32_t *)(h + 0x30) = *(const uint32_t *)fr;
		*(uint8_t **)(h + 0x2C) = fr + 4;
		PacketCursor() = DrawBeamSprites(h, RenderOT(), 2, PacketCursor());
		if (b->counter >= 0xF) return TASK_END;
		b->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Barrier (0x6D3160), one per action, 41 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl BarrierTask(TaskNode *n)
	{
		BarrierNode *b = (BarrierNode *)n;
		int32_t scale = (int32_t)*(const int16_t *)(b->entity + 0x26) << 2;
		if (scale > 0x1000) scale = 0x1000;
		// the yaw goes in AX over the scale (the callee reads the low word)
		YawScaleMatrix((int16_t)*(const uint16_t *)(b->entity + 0xE), &b->model, (int16_t)scale);
		ComposeAffineTransform(&Camera(), &b->model, &b->view);
		if ((uint32_t)(int32_t)b->frame < 0x14)
		{
			BarrierCenter(b->frame, b->center);
			int32_t angle = SharedRand();
			for (int i = 0xC; i; i--)
			{
				BeamNode *bm = (BeamNode *)AppendByValue(*QBeams(), ORIG_BeamTask);
				if (!bm) continue;
				bm->vy = 0;
				bm->y = 0;
				bm->counter = 0;
				bm->parent = b;
				bm->angle = (int16_t)angle;
				angle += 0x155;
				bm->scale = (int16_t)((SharedRand() & 0x3FF) + 0x200);
			}
		}
		const int32_t e = b->frame - 5;
		if ((uint32_t)e < 0x23)
		{
			// 30 fps layer: see mag106_reflect_held.inc
			FX_HELD(held_note_wall(b, e);)
			uint8_t *h = WallDraw(WallScroll(shl32(b->frame, 3)), WallHeight(e), WallFade(e), &b->view);
			if ((uint32_t)e < 0x12 && (uint32_t)e % 6u == 0)
			{
				RingNode *g = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
				if (g)
				{
					FillDwords(&g->counter, 0, 4);
					g->parent = b;
					g->height = (int16_t)(mul32(*(const int16_t *)(h + 0x5E), scale) >> 12);
					g->scale = 0x800;
					g->speed = 0x100;
					g->accel = -8;
				}
			}
			FieldFree(0x60);
		}
		if (b->frame == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(b->entity, pos);
			BdPlaySE3D(SOUND_Reflect, 0x101, pos);
		}
		if (b->frame == 0x23) ApplyActionResultToTarget(Ctx()->actions[b->action].targets);
		if (b->frame >= 0x28) return TASK_END;
		b->frame++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Beam list executor (MAG_106_sub_6D2E50 + MAG_106_sub_6D2EE0): a per-tick sprite header on
	// the scratch stack (published in 0x2523C94), then every node of the list; finished nodes are
	// freed and unlinked, the tail becomes the last survivor. Returns the nodes left.
	// ------------------------------------------------------------------
	static uint32_t RunBeams()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		BeamHeader() = h;
		*(uint32_t *)h = SEQ_Beam;
		const uint8_t *seq = *(const uint8_t **)h;
		*(uint16_t *)(h + 0x92) = 0;
		*(uint16_t *)(h + 0x90) = 0;
		*(uint16_t *)(h + 0x8E) = 0;
		*(uint16_t *)(h + 0x8A) = 0;
		*(uint16_t *)(h + 0x88) = 0;
		*(uint16_t *)(h + 0x86) = 0;
		*(uint16_t *)(h + 0x60) = 0;
		*(uint16_t *)(h + 0x58) = 0;
		*(uint16_t *)(h + 0x50) = 0;
		*(uint16_t *)(h + 0x48) = 0;
		h[0x20] = seq[4];
		h[0x21] = seq[5];
		h[0x22] = seq[6];
		h[0x23] = seq[7];
		TaskQueue *q = QBeams();
		TaskNode *prev = nullptr;
		uint32_t alive = 0;
		for (TaskNode *t = q->head; t; t = t->next)
		{
			void *port = lookup((uint32_t)t->func);
			const uint32_t r = (port ? (TaskFn)port : t->func)(t);
			if (r & 2)
			{
				t->flags = 0;
				if (prev) prev->next = t->next;
				else q->head = t->next;
			}
			else
			{
				prev = t;
				alive++;
			}
		}
		q->tail = prev;
		FieldFree(0xB4);
		return alive;
	}

	// ------------------------------------------------------------------
	// Director (0x6D2C00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_director();)
		if (d->arena)
		{
			PacketCursor() = ARENA_0;
			d->arena = 0;
		}
		else
		{
			PacketCursor() = ARENA_1;
			d->arena = 1;
		}
		if (d->counter == 0)
		{
			ActionData *acts = Ctx()->actions;
			if (d->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[d->action].targets[0]);
				// wait while a barrier of 35 ticks or less runs on this target
				BarrierNode *pool = (BarrierNode *)POOL_Barriers;
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].frame <= 0x23)
					{
						busy = true;
						break;
					}
				if (busy) d->counter = 0;
				else
				{
					// (no null test in the original: the pool holds 3 barriers)
					BarrierNode *b = (BarrierNode *)AddTaskToQueue(QBarriers(), ORIG_BarrierTask);
					FillDwords(&b->frame, 0, 0x14);
					b->action = (int16_t)(uint16_t)d->action;
					b->entity = entity;
					int16_t c[4];
					EffectCentre(entity, c);
					b->model.t[0] = c[0];
					b->model.t[1] = c[1];
					b->model.t[2] = c[2];
					d->action++;
					int32_t angle = SharedRand();
					for (int i = 8; i; i--)
					{
						HexagonNode *x = (HexagonNode *)AddTaskToQueue(QHexagons(), ORIG_HexagonTask);
						if (!x) continue;
						FillDwords(&x->counter, 0, 4);
						x->parent = b;
						x->tilt = 0x400;
						x->angle = (int16_t)angle;
						x->dangle = 0x10;
						if (SharedRand() & 1) x->dangle = (int16_t)-x->dangle;
						x->dtilt = (int16_t)(-0x25 - (SharedRand() & 0x1F));
						angle += 0x200;
					}
				}
			}
		}
		const int barriers = ExecuteTaskQueue(QBarriers());
		const int hexagons = ExecuteTaskQueue(QHexagons());
		const int rings = ExecuteTaskQueue(QRings());
		const uint32_t beams = RunBeams();
		if (barriers == 0 && beams == 0 && hexagons == 0 && rings == 0) return TASK_END;
		d->counter++;
		if (d->counter >= 10) d->counter = 0;
		return 0;
	}
}

	void register_mag106_reflect()
	{
		register_port(reflect106::ORIG_DirectorTask, (void *)reflect106::DirectorTask, "R106 DirectorTask", 106);
		register_port(reflect106::ORIG_BarrierTask, (void *)reflect106::BarrierTask, "R106 BarrierTask", 106);
		register_port(reflect106::ORIG_HexagonTask, (void *)reflect106::HexagonTask, "R106 HexagonTask", 106);
		register_port(reflect106::ORIG_RingTask, (void *)reflect106::RingTask, "R106 RingTask", 106);
		register_port(reflect106::ORIG_BeamTask, (void *)reflect106::BeamTask, "R106 BeamTask", 106);
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(register_mag106_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag106_reflect_held.inc"
#endif
