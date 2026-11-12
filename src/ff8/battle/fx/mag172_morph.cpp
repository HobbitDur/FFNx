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

// Effect 172: Morph (enemy attack 53 of kernel.bin; Jelleye c0m029; MAG_172_*).
//
// Structure (file loader 0x5D9CB0 = the texture file 0xD65634 -> 0x239D6F8; _Init 0x5D9CD0 keeps the context 0x239D4F0, the first
// target's slot 0x239A8E0 and the caster's slot 0x239D4F4, queues the root (one 0x10-byte node, pool 0x239A8E8, queue 0x239A8F8)
// and the director in the effect queue 0x239BEA8 (100 x 0x24, pool 0x239C6E0), frees the 100 sparkles (0x239A908, 0x18 bytes,
// word +0 = owner mask), plays the camera animation 0xD61E28 and queues the TIM; both run original):
//   Root (0x5DB210) - packet cursor 0x23AD6FC = 0x239D6FC / 0x23A56FC on its counter's parity, the effect queue; ends when it
//     is empty.
//   Director (0x5D9D80) - nothing on draw-only ticks. Tick 0: the caster's default effect position (its +0x10); tick 1: two
//     rings (0x5D9F20 / 0x5DA0F0, sizes 0xC00 / 0x1600, spin -10), the ring mesh's vertex radii (0x5D9EE0: 0x239BA90, from the
//     261 vertices of 0xD629A4), the sparkles (0x5DA2C0), the sound 0xD655DC at the caster; tick 8: the tinted caster; ends
//     after tick 0x3C.
//   Rings (0x5D9F20 / 0x5DA0F0) - prim model 0xD6299C with its vertices waved into 0x239BEB8 / 0x239B268 (0x5DA050 / 0x5DA220:
//     each vertex turned in xy by sin(radius * 3) * 0 = unchanged), through the shadow-rotation camera at the caster (R = size,
//     pull size >> 3) turned by z angle -2 / -3 x its angle; fade (0x20 - tick) << 7 to tick 8, 0xC00, (tick - 0x18) << 7 from
//     tick 0x30; angle + spin; 0x38 ticks.
//   Sparkles (0x5DA2C0) - sequence 0xD61F8C (frame base 2, scale x / y +-0x1000) of mask 1 at its point (R = size, pull -size
//     >> 4), still; ticks 0..0x2A: one new one at a random bone of the caster (rand % bone count, flag rand & 0xFFF) +-180
//     (size 0x140..0x7BF, flips from the point's low bits); ends from tick 4 once none is left.
//   Tinted caster (0x5DA520 -> 0x5DA660 -> 0x5DA770 -> 0x5DAA30 = Gastric Juice 169's 0x5DF170 / 0x5DF280 / its glow faces
//     with the record's code bits kept) - ticks 0..0x37: the caster hidden (entity flag 4) and its model redrawn with a Gouraud
//     overlay of every face with a corner within 1000 of its position (colour 0xD655E4[tick % 20]: a 20-step cycle, idiv remainder;
//     depth-cued to black by the fade (8 - tick) << 9 to tick 8, 0, (tick - 0x30) << 9 from tick 0x30); at tick 0x38 the flag 4 is
//     cleared.
// Every drawing task draws first and updates afterwards; on draw-only ticks it returns after drawing.
// ENTITY WRITES: the caster's flag word +0 bit 2 (hidden while it is redrawn; cleared at the end). CAMERA: the camera animation
// 0xD61E28 (engine). Module globals: 0x239A8E0..0x23AD700.

#include "mag_common.h"

namespace ff8fx
{
namespace morph172
{
	using namespace eng;
	using namespace magc;

	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }

	// --- module globals ---
	inline TaskQueue *QRoot() { return (TaskQueue *)0x239A8F8; }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x239BEA8; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x239D4F0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x239D4F4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23AD6FC); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	inline uint8_t *TintEntity() { return Entity(CasterSlot()); }
	static const uint32_t SPARKS = 0x239A908, SPARKS_END = 0x239B268;   // 100 x 0x18 bytes
	static const uint32_t RING_VERTS_A = 0x239BEB8, RING_VERTS_B = 0x239B268; // 261 x 8 bytes
	static const uint32_t RADII = 0x239BA90, RADII_END = 0x239BEA4;        // 261 vertex radii
	static const uint32_t MESH = 0xD629A4;                                // the ring mesh's 261 vertices (8 bytes)
	static const uint32_t ARENA_A = 0x239D6FC, ARENA_B = 0x23A56FC;

	static const uint32_t ORIG_Root = 0x5DB210, ORIG_Director = 0x5D9D80, ORIG_Ring = 0x5D9F20, ORIG_Ring2 = 0x5DA0F0,
		ORIG_Sparks = 0x5DA2C0, ORIG_Tint = 0x5DA520;
	static const uint32_t MODEL_Ring = 0xD6299C, SEQ_Spark = 0xD61F8C, SOUND_Morph = 0xD655DC;
	static const uint32_t *const TINT_COLOURS = (const uint32_t *)0xD655E4;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	inline void GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t mode, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, mode, out); }
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline uint32_t RenderGeometry(uint32_t model, uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x5099D0)(model, hdr, ot, mode, cursor); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	// sub_571620 / sub_571690: Bezier curve of n control points (int16 x 4): set-up into a work
	// block, then the point at t (4.12)
	inline void BezierInit(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }
	inline void BezierPoint(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); }

	inline void GteWriteData(uint32_t value, uint32_t reg) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45D7C0)(value, reg); }
	inline void GteReadData(void *dst, uint32_t reg) { fn<void (__cdecl *)(void *, uint32_t)>(0x45D7A0)(dst, reg); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }     // someCameraWork_45DD60
	inline void GteSetFarColorB(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DDA0)(r, g, b); }    // pre_someCameraWork_45DD60
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }       // set_param_with_dword_1CA8A68
	inline void GteLoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteStoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void GteSetIR0FromWord(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x64DF80)(p); }  // sub_64DF80: IR0 = *(int16 *)p
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // MAC = L * V0 + BK
	inline void GteSetBKFromMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = matrix translation
	inline void GteSetRotDiagonal16(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DDD0)(s); }     // MAG_148_sub_64DDD0: R11 / R22 / R33 = s
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { InsertPrimAutoDepth(bucket, (void *)pkt); }
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline const Mat4x3 *ShadowCamera(const void *pos, int32_t r, int32_t pull) { return fn<const Mat4x3 *(__cdecl *)(const void *, int32_t, int32_t)>(0x571BC0)(pos, r, pull); }
}
}

#ifdef FF8_FX_HELD
#include "mag172_morph_held.h"
#endif

namespace ff8fx
{
namespace morph172
{
	// ==================================================================
	// Tinted target model
	// Header (0xDC bytes on the scratch): +0x00 primitive list, +0x04 vertex buffer, +0x08..+0x0E
	// primitive counts, +0x10 / +0x14..+0x1A as RenderGeometry's, +0x1C code bits (entity +0x28),
	// +0x20 visible objects, +0x28..+0x2A depth cue, +0x48 NCLIP, +0x50 OTZ, +0x58 / +0x60 / +0x68 /
	// +0x70 corner copies (+6 of each: its overlay fade), +0xA8 centre (3 dwords), +0xB4 radius,
	// +0xB8 colour, +0xBC camera x entity matrix
	// ==================================================================

	// the corner at hdr + off made relative to the centre; below the radius: fade word at off + 6
	static bool CornerNear(uint32_t hdr, int32_t off)
	{
		S16(hdr, off + 2) = (int16_t)(S16(hdr, off + 2) - U16(hdr, 0xAC));
		S16(hdr, off) = (int16_t)(S16(hdr, off) - U16(hdr, 0xA8));
		S16(hdr, off + 4) = (int16_t)(S16(hdr, off + 4) - U16(hdr, 0xB0));
		int32_t y = S16(hdr, off + 2), x = S16(hdr, off), z = S16(hdr, off + 4);
		int32_t d2 = mul32(x, x);
		d2 = (int32_t)((uint32_t)d2 + (uint32_t)mul32(y, y));
		d2 = (int32_t)((uint32_t)d2 + (uint32_t)mul32(z, z));
		const int32_t d = Sqrt(d2);
		const int32_t r = S32(hdr, 0xB4);
		if (d >= r) return false;
		S16(hdr, off + 6) = (int16_t)(0x1000 - shl32(d, 12) / r);
		return true;
	}

	// 0x5DF540: the object's flat textured triangles (0x10-byte records) and quads (0x14-byte
	// records); a visible face with a corner within the radius gets a Gouraud overlay (G3 / G4) one
	// bucket closer: corner colour = the far colour depth-cued by 1 - distance / radius (corners out
	// of range keep the vertex buffer's pad word as their fade)
	static uint32_t GlowPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pk = cursor;
		const uint32_t verts = U32(hdr, 4);
		uint32_t rec = U32(hdr, 0);
		uint32_t bucket = 0;
		for (uint32_t i = 0; i < U16(hdr, 8); i++, rec += 0x10)
		{
			U32(hdr, 0x58) = U32(verts + (U16(rec, 0) & 0xFFF) * 8, 0);
			U32(hdr, 0x5C) = U32(verts + (U16(rec, 0) & 0xFFF) * 8, 4);
			U32(hdr, 0x60) = U32(verts + (U16(rec, 2) & 0xFFF) * 8, 0);
			U32(hdr, 0x64) = U32(verts + (U16(rec, 2) & 0xFFF) * 8, 4);
			U32(hdr, 0x68) = U32(verts + (U16(rec, 4) & 0xFFF) * 8, 0);
			U32(hdr, 0x6C) = U32(verts + (U16(rec, 4) & 0xFFF) * 8, 4);
			GteLoadV012(hdr + 0x58, hdr + 0x60, hdr + 0x68);
			GteRTPT();
			U32(pk, 0) = 0x7000000;
			const uint32_t w = U32(rec, 0xC);
			U32(pk, 4) = U32(hdr, 0x1C) | 0x24000000;
			U32(pk, 0xC) = U32(rec, 8);
			U32(pk, 0x14) = w;
			GteNCLIP();
			GteReadMAC0(hdr + 0x48);
			if (S32(hdr, 0x48) <= 0) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			S32(pk, 0x1C) = S32(rec, 4) >> 16;
			GteReadOTZ32(hdr + 0x50);
			bucket = ot + (uint32_t)shl32(S32(hdr, 0x50) >> (shift & 31), 2);
			InsertPrim(bucket, pk);
			pk += 0x20;
			uint32_t mask = 0;
			if (CornerNear(hdr, 0x58)) mask = 1;
			if (CornerNear(hdr, 0x60)) mask |= 2;
			if (CornerNear(hdr, 0x68)) mask |= 4;
			if (!mask) continue;
			U32(pk, 4) = 0x32000000;
			U32(pk, 0) = 0x6000000;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadRGBC(pk + 4);
			GteSetIR0FromWord(hdr + 0x5E);
			GteDPCS();
			GteStoreRGB2(pk + 4);
			GteSetIR0FromWord(hdr + 0x66);
			GteDPCS();
			GteStoreRGB2(pk + 0xC);
			GteSetIR0FromWord(hdr + 0x6E);
			GteDPCS();
			GteStoreRGB2(pk + 0x14);
			InsertPrim(bucket - 4, pk);
			pk += 0x1C;
		}
		const uint32_t quads = rec + 4;
		for (uint32_t i = 0; i < U16(hdr, 0xA); i++)
		{
			const uint32_t q = quads + i * 0x14;
			U32(hdr, 0x58) = U32(verts + (U16(q, -4) & 0xFFF) * 8, 0);
			U32(hdr, 0x5C) = U32(verts + (U16(q, -4) & 0xFFF) * 8, 4);
			U32(hdr, 0x60) = U32(verts + (U16(q, -2) & 0xFFF) * 8, 0);
			U32(hdr, 0x64) = U32(verts + (U16(q, -2) & 0xFFF) * 8, 4);
			U32(hdr, 0x68) = U32(verts + (U16(q, 0) & 0xFFF) * 8, 0);
			U32(hdr, 0x6C) = U32(verts + (U16(q, 0) & 0xFFF) * 8, 4);
			GteLoadV012(hdr + 0x58, hdr + 0x60, hdr + 0x68);
			GteRTPT();
			U32(pk, 0) = 0x9000000;
			U32(hdr, 0x70) = U32(verts + (U16(q, 2) & 0xFFF) * 8, 0);
			U32(hdr, 0x74) = U32(verts + (U16(q, 2) & 0xFFF) * 8, 4);
			const uint32_t w = U32(q, 8);
			U32(pk, 0xC) = U32(q, 4);
			U32(pk, 0x14) = w;
			U32(pk, 4) = U32(hdr, 0x1C) | 0x2C000000;
			GteNCLIP();
			GteReadMAC0(hdr + 0x48);
			if (S32(hdr, 0x48) <= 0) continue;
			const uint32_t xy0 = pk + 8;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(hdr + 0x70);
			GteRTPS();
			U32(pk, 0x1C) = U32(q, 0xC);
			S32(pk, 0x24) = S32(q, 0xC) >> 16;
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			GteReadOTZ32(hdr + 0x50);
			bucket = ot + (uint32_t)shl32(S32(hdr, 0x50) >> (shift & 31), 2);
			InsertPrim(bucket, pk);
			uint32_t mask = 0;
			if (CornerNear(hdr, 0x58)) mask = 1;
			if (CornerNear(hdr, 0x60)) mask |= 2;
			if (CornerNear(hdr, 0x68)) mask |= 4;
			if (CornerNear(hdr, 0x70)) mask |= 8;
			if (!mask)
			{
				pk += 0x28;
				continue;
			}
			pk += 0x28;
			U32(pk, 8) = U32(xy0, 0);
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = 0x3A000000;
			GteReadSXY012(pk + 0x10, pk + 0x18, pk + 0x20);
			GteLoadRGBC(pk + 4);
			GteSetIR0FromWord(hdr + 0x5E);
			GteDPCS();
			GteStoreRGB2(pk + 4);
			GteSetIR0FromWord(hdr + 0x66);
			GteDPCS();
			GteStoreRGB2(pk + 0xC);
			GteSetIR0FromWord(hdr + 0x6E);
			GteDPCS();
			GteStoreRGB2(pk + 0x14);
			GteSetIR0FromWord(hdr + 0x76);
			GteDPCS();
			GteStoreRGB2(pk + 0x1C);
			InsertPrim(bucket - 4, pk);
			pk += 0x24;
		}
		return pk;
	}

	// 0x5DF280: the far colour = the header's colour; every visible object: its vertices through
	// their bone matrices (world space) into the vertex buffer (+4), then its faces through the
	// camera (GlowPrims)
	static uint32_t ModelObjects(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		GteSetFarColor(U8(hdr, 0xB8), U8(hdr, 0xB9), U8(hdr, 0xBA));
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(hdr, 4);
			obj += 2;
			if (groups > 0)
			{
				do
				{
					const int32_t bone = S16(obj, 0);
					obj += 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)(bone * 0x30) + 0x10);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					int32_t nv = S16(obj, 0);
					obj += 2;
					for (; nv != 0; nv--)
					{
						const uint32_t xy = (uint32_t)U16(obj, 0) | ((uint32_t)U16(obj, 2) << 16);
						const uint32_t z = U16(obj, 4);
						GteWriteData(xy, 0);
						GteWriteData(z, 1);
						obj += 6;
						out += 8;
						GteMVMVA_RotV0Tr2();
						int32_t a, b, c;
						GteReadData(&a, 9);
						GteReadData(&b, 10);
						GteReadData(&c, 11);
						U32(out, -8) = (uint32_t)shl32(b, 16) | ((uint32_t)a & 0xFFFF);
						U16(out, -4) = (uint16_t)c;
					}
				} while (--groups != 0);
			}
			obj = (obj + 3) & ~3u;
			U16(hdr, 8) = U16(obj, 0);
			U16(hdr, 0xA) = U16(obj, 2);
			U16(hdr, 0xC) = U16(obj, 4);
			U16(hdr, 0xE) = U16(obj, 6);
			U32(hdr, 0) = obj + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			cursor = GlowPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x5DF170: the entity's shadow, its model (ModelObjects), its weapon (+0x78) through
	// RenderGeometry; the bone matrices are rebuilt from the pose afterwards
	static void ModelRender(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		// 30 fps layer: see mag172_morph_held.inc
		FX_HELD(held_model_pose(entity);)
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0xBC));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 4) = var<uint32_t>(0x1D98B3C);
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		FrameCursor() = ModelObjects(U32(anim, 4), hdr, RenderOT44(), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t weapon = U32(entity, 0x78);
		if (weapon && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)weapon, (void *)(hdr + 0xBC));
			FrameCursor() = RenderGeometry(U32(weapon, 4), hdr, RenderOT44(), 4, FrameCursor());
			BuildBoneMatricesFromPose((void *)weapon);
		}
	}

	// the caster hidden and redrawn tinted around its position (colour of the tick's cycle, depth-cued to black by the fade)
	static uint32_t TintColour(int32_t c) { return TINT_COLOURS[c % 0x14]; }
	static void TintDraw(uint32_t n, int32_t fade, uint32_t colour)
	{
		uint8_t *entity = TintEntity();
		entity[0] |= 4;
		const uint32_t h = P(FieldAlloc(0xDC));
		S32(h, 0xAC) = S16(n, 0x12);
		S32(h, 0xA8) = S16(n, 0x10);
		S32(h, 0xB0) = S16(n, 0x14);
		S32(h, 0xB4) = 0x3E8;
		U32(h, 0xB8) = colour;
		GteSetFarColorB(0, 0, 0);
		GteLoadRGBC(h + 0xB8);
		GteSetIR0(fade);
		GteDPCS();
		GteStoreRGB2(h + 0xB8);
		ModelRender(h, P(entity));
		FieldFree(0xDC);
	}

	// fade of the tinted caster: in over ticks 0..8, none, out from 0x30
	static int32_t TintFade(int32_t c) { return c < 8 ? shl32(8 - c, 9) : (c >= 0x30 ? shl32(c - 0x30, 9) : 0); }

	static uint32_t __cdecl TintTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		if (S16(n, 0xC) < 0x38)
		{
			// 30 fps layer: see mag172_morph_held.inc
			FX_HELD(held_note_node(REC_TINT, n);)
			TintDraw(n, TintFade(S16(n, 0xC)), TintColour(S16(n, 0xC)));
		}
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 0x38) return 0;
		*(uint16_t *)TintEntity() &= 0xFFFB;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Rings (0x5D9F20 / 0x5DA0F0)
	// ------------------------------------------------------------------
	// 0x5D9EE0: every mesh vertex's radius in xy
	static void MeshRadii()
	{
		uint32_t v = MESH;
		for (uint32_t o = RADII; o < RADII_END; o += 4, v += 8)
		{
			const int32_t x = S16(v, 0), y = S16(v, 2);
			S32(o, 0) = Sqrt(mul32(x, x) + mul32(y, y));
		}
	}

	// 0x5DA050 / 0x5DA220: every mesh vertex turned in xy by sin(radius * 3) * amp into out
	static void WaveVerts(uint32_t out, int32_t amp)
	{
		uint32_t v = MESH + 2, r = RADII, o = out + 2;
		for (; r < RADII_END; r += 4, v += 8, o += 8)
		{
			const int32_t a = mul32(ComputeSin(S32(r, 0) * 3), amp) >> 12;
			const int32_t c = ComputeCos(a), s = ComputeSin(a);
			const int32_t x = S16(v, -2), y = S16(v, 0);
			S16(o, -2) = (int16_t)((mul32(x, c) >> 12) + (mul32(y, s) >> 12));
			S16(o, 0) = (int16_t)((-mul32(x, s) >> 12) + (mul32(y, c) >> 12));
			S16(o, 2) = S16(v, 2);
		}
	}

	static int32_t RingFade(int32_t c) { return c < 8 ? shl32(0x20 - c, 7) : (c >= 0x30 ? shl32(c - 0x18, 7) : 0xC00); }

	// the ring through the shadow-rotation camera at its point, turned by z angle mul x its angle; its vertices waved into verts
	static void RingDraw(uint32_t n, int32_t mul, uint32_t verts, int32_t fade)
	{
		const int16_t r = S16(n, 0x1C);
		const Mat4x3 *sm = ShadowCamera((const void *)(n + 0x10), (int32_t)(uint16_t)r, r >> 3);
		const int16_t ang[3] = { 0, 0, (int16_t)(S16(n, 0x18) * mul) };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply(sm, &m);
		GteSetRotMatrix(&m);
		WaveVerts(verts, 0);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Ring;
		U32(h, 4) = verts;
		S32(h, 0xC) = 0xC00;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x20F3;
		S32(h, 0xC) = fade;
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t RingTaskImpl(uint32_t n, bool second)
	{
		// 30 fps layer: see mag172_morph_held.inc
		FX_HELD(held_note_node(second ? REC_RING2 : REC_RING, n);)
		RingDraw(n, second ? -3 : -2, second ? RING_VERTS_B : RING_VERTS_A, RingFade(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x38 ? TASK_END : 0;
	}

	static uint32_t __cdecl RingTask(TaskNode *tn) { return RingTaskImpl(P(tn), false); }
	static uint32_t __cdecl Ring2Task(TaskNode *tn) { return RingTaskImpl(P(tn), true); }

	// ------------------------------------------------------------------
	// Sparkles (0x5DA2C0)
	// ------------------------------------------------------------------
	// a sparkle (0x18 bytes): +0 owner mask, +4 frame, +6 size, +8 point (4 words), +0x10 / +0x12 scale x / y
	static void SparkDraw(uint32_t h, uint32_t p)
	{
		const int16_t size = S16(p, 6);
		U16(h, 4) = U16(p, 4);
		S32(h, 0xC) = S16(p, 0x10);
		S32(h, 0x10) = S16(p, 0x12);
		TransformCameraByShadowRotation((const void *)(p + 8), (int32_t)(uint16_t)size, -(size >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t __cdecl SparksTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag172_morph_held.inc
		FX_HELD(held_note_parts(REC_SPARKS, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int alive = 0;
		U32(h, 0) = SEQ_Spark;
		U16(h, 0x24) = 2;
		S32(h, 0x14) = 0x1000;
		for (uint32_t p = SPARKS; p < SPARKS_END; p += 0x18)
		{
			if (!(U8(p, 0) & 1)) continue;
			// 30 fps layer: see mag172_morph_held.inc
			FX_HELD(held_note_particle(p);)
			SparkDraw(h, p);
			if (DrawOnly()) continue;
			S16(p, 4) = (int16_t)(S16(p, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(p, 0) = 0;
				continue;
			}
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 0x2A)
		{
			const int32_t bones = *(const uint8_t *)U32(U32(P(TintEntity()), 0x64), 0);
			int32_t i = 0;
			uint32_t p = SPARKS;
			for (; U32(p, 0) != 0; p += 0x18, i++)
				if (p + 0x18 >= SPARKS_END) { p = 0; break; }
			if (p && i < 0x64)
			{
				U32(p, 0) = 1;
				S16(p, 4) = 0;
				S16(p, 6) = (int16_t)(CrtRand() % 0x680 + 0x140);
				const int32_t flag = CrtRand() & 0xFFF;
				const int32_t bone = CrtRand() % bones;
				GetEffectSpawnPosition(TintEntity(), bone, flag, (int16_t *)(p + 8));
				S16(p, 8) = (int16_t)(S16(p, 8) + CrtRand() % 0x168 - 0xB4);
				S16(p, 0xA) = (int16_t)(S16(p, 0xA) + CrtRand() % 0x168 - 0xB4);
				S16(p, 0xC) = (int16_t)(S16(p, 0xC) + CrtRand() % 0x168 - 0xB4);
				S16(p, 0x10) = (int16_t)((U8(p, 8) & 1) ? 0x1000 : -0x1000);
				S16(p, 0x12) = (int16_t)((U8(p, 0xA) & 1) ? 0x1000 : -0x1000);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5D9D80) and root (0x5DB210)
	// ------------------------------------------------------------------
	static uint32_t NewTask(uint32_t f)
	{
		const uint32_t t = P(AddTaskToQueue(EffectQueue(), f));
		S16(t, 0xC) = 0;
		return t;
	}

	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = P(tn);
		if (S16(n, 0xC) == 0) GetDefaultEffectPosition(TintEntity(), (int16_t *)(n + 0x10));
		if (S16(n, 0xC) == 1)
		{
			uint32_t t = NewTask(ORIG_Ring);
			U32(t, 0x10) = U32(n, 0x10);
			U32(t, 0x14) = U32(n, 0x14);
			S16(t, 0x18) = 0;
			S16(t, 0x1A) = -10;
			S16(t, 0x1C) = 0xC00;
			t = NewTask(ORIG_Ring2);
			U32(t, 0x10) = U32(n, 0x10);
			U32(t, 0x14) = U32(n, 0x14);
			S16(t, 0x18) = 0;
			S16(t, 0x1A) = -10;
			S16(t, 0x1C) = 0x1600;
			MeshRadii();
			NewTask(ORIG_Sparks);
		}
		if (S16(n, 0xC) == 8)
		{
			const uint32_t t = NewTask(ORIG_Tint);
			U32(t, 0x10) = U32(n, 0x10);
			U32(t, 0x14) = U32(n, 0x14);
		}
		if (S16(n, 0xC) == 1)
		{
			int16_t at[4];
			GetDefaultEffectPosition(TintEntity(), at);
			BdPlaySE3D((const void *)SOUND_Morph, 0, at);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x3C ? TASK_END : 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag172_morph_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? ARENA_B : ARENA_A;
		const int running = ExecuteTaskQueue(EffectQueue());
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	void register_mag172_morph_ports()
	{
		register_port(ORIG_Root, (void *)RootTask, "172 RootTask", 172);
		register_port(ORIG_Director, (void *)DirectorTask, "172 DirectorTask", 172);
		register_port(ORIG_Ring, (void *)RingTask, "172 RingTask", 172);
		register_port(ORIG_Ring2, (void *)Ring2Task, "172 Ring2Task", 172);
		register_port(ORIG_Sparks, (void *)SparksTask, "172 SparksTask", 172);
		register_port(ORIG_Tint, (void *)TintTask, "172 TintTask", 172);
		// 30 fps layer: see mag172_morph_held.inc
		FX_HELD(register_mag172_held();)
	}
}

	void register_mag172_morph() { morph172::register_mag172_morph_ports(); }
}

#ifdef FF8_FX_HELD
#include "mag172_morph_held.inc"
#endif
