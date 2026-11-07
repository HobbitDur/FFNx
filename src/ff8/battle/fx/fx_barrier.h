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

// Parts library, the barrier (Reflect 106, the "ruby" part of Carbuncle's Ruby Light in its own
// copy): a frame on the target (turned with it, scaled by its size), a wall model growing up
// through it with a scrolling texture (its textured quads, gouraud triangles and gouraud quads,
// each between two texture-window packets, the vertices drawn by their height), hexagon models
// orbiting a point that climbs the wall, rings growing above the target. Bit-exact with Reflect's
// code (mag106_reflect.cpp before it was built from here), each piece named with its original.

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
	namespace barrier
	{
		using parts::GteLoadV012;
		using parts::GteRTPT;
		using parts::GteAVSZ3;
		using parts::GteAVSZ4;
		using parts::GteReadOTZWord;
		inline void MatMul56C220(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }   // a = a x b (3x3 + pad)

		// MAG_103_sub_6DBCC0: yaw rotation scaled by s (3x3 only; the yaw is the low word of arg 1)
		inline void YawScaleMatrix(int16_t yaw, Mat4x3 *m, int16_t s)
		{
			int32_t sn = mul32(ComputeSin(yaw), s) >> 12;
			int32_t cs = mul32(parts::ComputeCos(yaw), s) >> 12;
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
		inline void ZRotation(int16_t angle, Mat4x3 *m)
		{
			const int32_t s = ComputeSin(angle);
			const int32_t c = parts::ComputeCos(angle);
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
		inline void XRotation(int16_t angle, Mat4x3 *m)
		{
			int32_t s = ComputeSin(angle);
			int32_t c = parts::ComputeCos(angle);
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

		// MAG_106_sub_6D30F0: (x, y) of src rotated by angle (the sin / cos table: 0x1000 x { s16 sin, s16 cos }), z copied
		inline void Rot2D(const int16_t *src, int16_t *dst, int32_t angle, uint32_t sincos)
		{
			const uint32_t a = (uint32_t)angle & 0xFFF;
			const int32_t c = *(const int16_t *)(sincos + a * 4 + 2);
			const int32_t ns = -(int32_t)*(const int16_t *)(sincos + a * 4);
			dst[0] = (int16_t)((int32_t)((uint32_t)mul32(src[0], c) - (uint32_t)mul32(src[1], ns)) >> 12);
			dst[1] = (int16_t)((int32_t)((uint32_t)mul32(src[1], c) + (uint32_t)mul32(src[0], ns)) >> 12);
			dst[2] = src[2];
		}

		// TexWindow: GP0(E2) texture window word of the 4 halves at tw (+0 / +2 bytes, +4 / +6 words)
		inline uint32_t TexWindow(const uint8_t *tw)
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
				GteReadSXY012To(pk + 8, pk + 0x10, pk + 0x18);
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
				GteReadOTZWord(h + 0x30);
				if (*(const int32_t *)(h + 8))
				{
					GteSetFarColor(h[4], h[5], h[6]);
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
				GteReadSXY012To(pk + 8, pk + 0x14, pk + 0x20);
				GteAVSZ3();
				if (OutX(pk + 8)) clip = 1;
				if (OutX(pk + 0x14)) clip |= 2;
				if (OutX(pk + 0x20)) clip |= 4;
				if (OutY(pk + 0xA)) clip |= 0x10;
				if (OutY(pk + 0x16)) clip |= 0x20;
				if (OutY(pk + 0x22)) clip |= 0x40;
				if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
				GteReadOTZWord(h + 0x30);
				if (*(const int32_t *)(h + 8))
				{
					GteSetFarColor(h[4], h[5], h[6]);
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
				GteReadSXY012To(pk + 8, pk + 0x14, pk + 0x20);
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
				GteReadOTZWord(h + 0x30);
				if (*(const int32_t *)(h + 8))
				{
					GteSetFarColor(h[4], h[5], h[6]);
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
	}

	// MAG_106_sub_7059E0: the effects' shared LCG ((125 s + 14) & 0x7FFF, its seed at 0x2557098) and
	// its seeding (0x7059D0)
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }
	inline void SeedSharedRand(int32_t seed) { fn<void (__cdecl *)(int32_t)>(0x7059D0)(seed); }

	// the wall model's lowest vertex height and its height span + 1/16 (Reflect's _Init: its +4 vertex
	// count, +8 vertices of 8 bytes, the height word at +4 of each)
	inline void WallMeasure(uint32_t wall, int16_t *base, int16_t *span)
	{
		const int16_t *v = (const int16_t *)(wall + 0x14);
		int32_t left = *(const int32_t *)(wall + 4) - 1;
		int16_t hi = *(const int16_t *)(wall + 0xC), lo = hi;
		do
		{
			if (lo <= *v)
			{
				if (hi < *v) hi = *v;
			}
			else lo = *v;
			v += 4;
		} while (--left);
		*base = lo;
		*span = (int16_t)((int16_t)((int16_t)(hi - lo) >> 4) + hi - lo);
	}

#pragma pack(push, 1)
	struct Barrier // a node's fields after its counter, action and entity (0x48 bytes)
	{
		Mat4x3 model;       // +0x00 Y rotation (the entity's facing) x its size scale; translation = its effect centre
		Mat4x3 view;        // +0x20 camera x model
		int16_t center[3];  // +0x40 the hexagons' centre (x = 0, y / z along the wall model's vertices)
		int16_t pad;
	};
	struct BarrierNode // a whole barrier node (0x5C): what its hexagons, rings and beams point at
	{
		TaskNode hdr;
		int16_t frame;      // +0x0C
		int16_t action;     // +0x0E action index
		uint8_t *entity;    // +0x10 first target of the action
		Barrier b;          // +0x14 its frame, view and the hexagons' centre
	};
	struct Orbit // a hexagon's fields after its counter (0x0E bytes)
	{
		int16_t tilt;       // X rotation
		int16_t angle;      // position angle around the centre (and -Z rotation)
		int16_t dangle;
		int16_t dtilt;
		int16_t pad;
		const BarrierNode *parent;
	};
	struct Growth // a ring's fields after its counter (0x0E bytes)
	{
		int16_t height;
		int16_t scale;
		int16_t speed;
		int16_t accel;
		int16_t pad;
		const BarrierNode *parent;
	};
	struct DeadBeam // a beam's node (0x1C), filled in and never run
	{
		TaskNode hdr;
		int16_t counter;    // +0x0C sequence frame
		int16_t angle;      // +0x0E
		int16_t scale;      // +0x10
		int16_t y;          // +0x12
		int16_t vy;         // +0x14
		int16_t pad16;
		const BarrierNode *parent; // +0x18
	};
#pragma pack(pop)
	static_assert(sizeof(Barrier) == 0x48 && sizeof(BarrierNode) == 0x5C && sizeof(Orbit) == 0x0E && sizeof(Growth) == 0x0E && sizeof(DeadBeam) == 0x1C, "barrier parts");

	// MAG_106_sub_6D2DE0: middle of the entity's effect points 0xF1 and 0xF0
	inline void EffectCentre(uint8_t *entity, int16_t *out)
	{
		int16_t a[4], b[4];
		GetEffectSpawnPosition(entity, 0xF1, 0, a);
		GetEffectSpawnPosition(entity, 0xF0, 0, b);
		out[0] = (int16_t)(((int32_t)b[0] + a[0]) >> 1);
		out[1] = (int16_t)(((int32_t)b[1] + a[1]) >> 1);
		out[2] = (int16_t)(((int32_t)b[2] + a[2]) >> 1);
	}

	// the barrier's frame this tick: turned with the entity, scaled by min(its size << shift, 0x1000)
	inline int32_t BarrierFrame(Barrier *b, uint8_t *entity, int32_t shift)
	{
		int32_t scale = shl32(*(const int16_t *)(entity + 0x26), shift);
		if (scale > 0x1000) scale = 0x1000;
		// the yaw goes in AX over the scale (the callee reads the low word)
		barrier::YawScaleMatrix((int16_t)*(const uint16_t *)(entity + 0xE), &b->model, (int16_t)scale);
		ComposeAffineTransform(&Camera(), &b->model, &b->view);
		return scale;
	}

	// the hexagons' centre at barrier frame f of `ticks`: the wall model's vertices f x steps / ticks
	// and the next, blended by the fraction (their y / z words)
	inline void BarrierCenter(uint32_t wall, int32_t f, int32_t steps, int32_t ticks, int16_t out[3])
	{
		const uint32_t t = ((uint32_t)f * (uint32_t)steps << 12) / (uint32_t)(ticks ? ticks : 1);
		const int32_t idx = (int32_t)t >> 12;
		const int32_t w = (int32_t)(t & 0xFFF);
		const uint32_t v = wall + 8 + 8 * idx;
		int16_t a = var<int16_t>(v + 2);
		out[1] = (int16_t)((mul32((int32_t)var<int16_t>(v + 0xA) - a, w) >> 12) + a);
		out[0] = 0;
		a = var<int16_t>(v + 4);
		out[2] = (int16_t)((mul32((int32_t)var<int16_t>(v + 0xC) - a, w) >> 12) + a);
	}

	// Reflect's beams (vanilla dead code: MAG_106_sub_705A00 appends to a queue passed by value and
	// never writes the real header back, so their nodes are filled in, never linked, never run, and
	// the pool fills up): one shared rand for their first angle, then one for each of `count` beams
	// while the pool has a free node
	inline void DeadBeams(const TaskQueue *q, uint32_t task, int32_t count, const BarrierNode *parent)
	{
		int32_t angle = SharedRand();
		for (int32_t i = count; i; i--)
		{
			uint8_t *p = (uint8_t *)q->pool, *free = nullptr;
			for (int32_t k = 0; k < q->capacity && !free; k++, p += q->node_size)
				if (!(*p & 1)) free = p;
			if (!free) continue;
			*free |= 1;
			DeadBeam *b = (DeadBeam *)free;
			b->hdr.func = (TaskFn)task;
			b->hdr.sequence = 0;
			b->hdr.next = nullptr;
			if (q->tail) q->tail->next = &b->hdr;
			b->vy = 0;
			b->y = 0;
			b->counter = 0;
			b->parent = parent;
			b->angle = (int16_t)angle;
			angle += 0x155;
			b->scale = (int16_t)((SharedRand() & 0x3FF) + 0x200);
		}
	}

	// MAG_106_sub_6D2E50 + MAG_106_sub_6D2EE0, the beam list's executor: a per-tick sprite header on
	// the scratch stack, published in `header`, then the list, which is empty (see DeadBeams), so its
	// tail stays 0 and nothing is left
	inline void DeadBeamsRun(uint8_t *&header, uint32_t seq, TaskQueue *q)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		header = h;
		*(uint32_t *)h = seq;
		const uint8_t *s = (const uint8_t *)seq;
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
		h[0x20] = s[4];
		h[0x21] = s[5];
		h[0x22] = s[6];
		h[0x23] = s[7];
		q->tail = nullptr;
		FieldFree(0xB4);
	}

	// the wall's top (its vertices drawn up to it) and fog at wall step e of a wall growing over
	// `grow` steps, fading over `fade` (Reflect: 20, 15)
	inline int32_t WallHeight(uint32_t wall, int32_t e, int32_t grow)
	{
		int16_t base, span;
		WallMeasure(wall, &base, &span);
		if ((uint32_t)e <= (uint32_t)grow) return (int16_t)((int16_t)((uint32_t)mul32(e + 1, span) / (uint32_t)(grow ? grow : 1)) + base);
		return (int16_t)(span + base);
	}
	inline int32_t WallFade(int32_t e, int32_t grow, int32_t fade)
	{
		if ((uint32_t)e <= (uint32_t)grow) return 0;
		return (int32_t)((uint32_t)shl32(e - grow, 12) / (uint32_t)(fade ? fade : 1));
	}

	// 0x6D32BE..0x6D339C: the wall through `view`, V scrolled by `scroll` (8 x the frame: steps of 8
	// texels, 0x80 wrap), its vertices drawn up to `height`, fogged by `fade`
	inline void WallDraw(uint32_t wall, int32_t frame, int32_t height, int32_t fade, const Mat4x3 *view, uint32_t &cursor)
	{
		int16_t base, span;
		WallMeasure(wall, &base, &span);
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)h = wall;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x0C) = 0;
		*(uint16_t *)(h + 0x0E) = (uint16_t)((uint8_t)(0u - (uint8_t)shl32(frame, 3)) & 0x7F);
		*(int16_t *)(h + 0x5C) = base;
		h[6] = 0;
		h[5] = 0;
		h[4] = 0;
		*(int32_t *)(h + 8) = fade;
		*(int16_t *)(h + 0x5E) = (int16_t)height;
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		cursor = barrier::RenderWall(h, RenderOT(), 2, cursor);
		FieldFree(0x60);
	}

	// 0x6D2F30..0x6D30A8: a hexagon model at `angle` around the centre, Z-rotated by -angle, squashed
	// in x (from `from`, over `ticks`: its fade too), tilted about X, at the rotated point
	inline void OrbitDraw(uint32_t model, const Orbit *o, int32_t c, int32_t from, int32_t ticks, uint32_t mode, uint32_t sincos, uint32_t &cursor)
	{
		int32_t fade, sx;
		if (c > from)
		{
			const int32_t q = shl32(c - from, 12) / (ticks ? ticks : 1);
			sx = (int16_t)(0x1000 - (int16_t)q);
			fade = (q >> 1) + 0x800;
		}
		else
		{
			sx = 0x1000;
			fade = 0x800;
		}
		const Mat4x3 *view = &o->parent->b.view;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t p[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		barrier::Rot2D(o->parent->b.center, p, o->angle, sincos);
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		Mat4x3 X = {};  // its translation = the rotated point in view space
		GteReadMAC123(X.t);
		*(uint32_t *)(h + 0x1C) = mode;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		Mat4x3 S = {};  // pad / translation: vanilla stack garbage (never read)
		S.m[0][0] = (int16_t)sx;
		S.m[1][1] = 0x1000;
		S.m[2][2] = 0x1000;
		Mat4x3 R = {};
		barrier::ZRotation((int16_t)-o->angle, &R);
		barrier::MatMul56C220(&R, &S);
		MatrixMultiply(view, &R);
		barrier::XRotation(o->tilt, &X);
		MatrixMultiply(&R, &X);
		GteSetRotMatrix(&X);
		GteSetTransVector(&X);
		*(uint32_t *)h = model;
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// 0x6D3AF0..0x6D3BF9: a ring model at its height above the barrier's origin, scaled, faded by
	// sin(c x 1024 / ticks)
	inline void GrowthDraw(uint32_t model, const Growth *g, int32_t c, int32_t ticks, uint32_t mode, uint32_t &cursor)
	{
		const int32_t fade = ComputeSin(shl32(c, 10) / (ticks ? ticks : 1));
		const Mat4x3 *view = &g->parent->b.view;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t V[4] = { 0, 0, g->height, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(V);
		GteMVMVA_RotV0Tr();
		Mat4x3 T = {};
		GteReadMAC123(T.t);
		GteSetTransVector(&T);
		Mat4x3 S = {};
		S.m[0][0] = g->scale;
		S.m[1][1] = g->scale;
		S.m[2][2] = g->scale;
		barrier::MatMul56C220(&S, view);
		GteSetRotMatrix(&S);
		*(uint32_t *)h = model;
		*(int32_t *)(h + 0xC) = fade;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = mode;
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// a ring's growth a tick: its speed braked by accel to at least `min`, its scale grown by it
	inline void GrowthStep(Growth *g, int32_t min)
	{
		g->speed = (int16_t)(g->speed + g->accel);
		if (g->speed < min) g->speed = (int16_t)min;
		g->scale = (int16_t)(g->scale + g->speed);
	}
}
}
