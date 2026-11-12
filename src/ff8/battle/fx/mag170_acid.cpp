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

// Effect 170: Acid (enemy attack 35 of kernel.bin; Tri-Face c0m036; MAG_170_*). Gastric Juice 169's design
// (mag169_gastric_juice.cpp) with one stream and the target's model melting.
//
// Structure (file loader 0x5DC910 = the texture file 0xD6A3EC -> 0x23B1338; _Init 0x5DC930 keeps the texture base 0x23B1348,
// the context 0x23B0FD0, the first target's slot 0x23AF820 and the caster's slot 0x23B10E8, queues the root (one 0x10-byte
// node, pool 0x23AF828, queue 0x23AF838) and the director in the effect queue 0x23B01A8 (100 x 0x24, pool 0x23B01B8), frees
// the 100 droplets (0x23AF848, 0x18 bytes, word +0 = owner mask), plays the camera animation 0xD68D0C and queues the TIM;
// both run original):
//   Root (0x5DEDF0) - packet arena = texture base / + 0x8000 on its counter's parity, the effect queue; ends when it is empty.
//   Director (0x5DC9F0) - nothing on draw-only ticks; tick 1: the stream, the caster's effect points 0x28 (0x23B1340) and
//     0x26 (0x23B0FC8), the splash (mask 2), the sound 0xD6A3A4; tick 0x2E: the action result; ends after tick 0x37.
//   Stream (0x5DCAF0) - nothing on draw-only ticks. Tick 0: the curve (stream 0x23B0FD8: the caster's effect point 0x29
//     (flag 0x600), the target's default position +-350 on the ground, control points a third / two thirds of the way up by
//     50..149 each, 28 samples); tick 1: the blob; tick 0xC: 4 puddles around the landing point (+-800, delay rand % 6 + k)
//     and the melting target; tick 0xD: the landing mesh and the drip emitter (mask 1 << stream); ends at tick 0x28.
//   Blob (0x5DCF20) - sequence 0xD68DF8 (frame tick % 6) at 4 curve samples back from the one it has reached (radius - 0x70
//     each, a sample past the curve skipped, before its start the rest), speed 5 slowing to 2 before sample 0xE; ends past
//     sample 0x1F.
//   Landing mesh (0x5DD030) - prim model 0xD6904C, its 73 vertices blended towards the second shape (0xD69A74) by
//     sin(tick * 32) into 0x23B10F0, y angle at random, scale + step (step halved) on ticks 0..7; ticks 0..7 plain, 8..15
//     fading, 16..31 through the module's prim renderer (0x5DD300 = Gastric Juice's 0x5E0450) with the texture page moved
//     0x20 (fade in / out); 32 ticks.
//   Drip emitter (0x5DDE90) - droplets of its mask: sequence 0xD68EA0 (frame / 2, pull -size >> 4), falling (1/16
//     deceleration); ticks 0..12: the count of 0xD6A3AC[tick] new ones around the landing point (size 0x700..0x10FF, +-700 /
//     -100..-399, falling 20..69); ends from tick 4 once none is left.
//   Puddle (0x5DE0B0) - waits its delay, then prim model 0xD69CBC at its point (y angle, scale + step), faded 0x800 /
//     tick << 8 from tick 8; 16 ticks.
//   Melting target (0x5DE200 -> 0x5DE3E0 -> 0x5DE500 -> 0x5DE7C0) - ticks 0..0x21: the first target hidden (entity flag 4) and
//     its model (and weapon) redrawn with every vertex beyond the plane through the landing point facing the caster pulled
//     down by an amount growing to tick 0x1C then shrinking (tick * 81 / 0x305B - tick * 364), within 10000 of the landing
//     point, flattened at the ground and spread outward from the target's position; flat textured faces only; at tick 0x23
//     the target's flag 4 is restored as it was.
//   Splash (0x5DEA10) - droplets of mask 2: sequence 0xD68DF8 (frame base 4, frame / 2, grey 0x80 then (11 - frame) * 16 from
//     frame 3) through the light matrix of the direction 0x26 -> 0x28 at point 0x28, decelerating (1/4), 11 frames; tick 0:
//     16 new ones; ends from tick 4 once none is left.
// Every drawing task draws first and updates afterwards; on draw-only ticks it returns after drawing.
// ENTITY WRITES: the first target's flag word +0 bit 2 (hidden while it is redrawn; restored at the end). CAMERA: the camera
// animation 0xD68D0C (engine). Module globals: 0x23AF820..0x23B1370.
// NEVER-WRITTEN STACK WORDS: none read.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace acid170
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
	inline int32_t &TargetSlot() { return var<int32_t>(0x23AF820); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x23AF838; }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x23B01A8; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x23B0FD0); }
	inline int32_t &AttackerSlot() { return var<int32_t>(0x23B10E8); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x23B1348); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23B134C); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	static const uint32_t DROPS = 0x23AF848, DROPS_END = 0x23B01A8;   // 100 x 0x18 bytes
	static const uint32_t STREAM = 0x23B0FD8;                         // 1 x 0x110 bytes
	static const uint32_t MESH_VERTS = 0x23B10F0;                     // 73 x 8 bytes
	static const uint32_t SPLASH_FROM = 0x23B0FC8, SPLASH_AT = 0x23B1340; // the caster's effect points 0x26 / 0x28
	static const uint32_t SPLASH_MATRIX = 0x23B1350;                  // Mat4x3 (translation 0x23B1364)

	static const uint32_t ORIG_Root = 0x5DEDF0, ORIG_Director = 0x5DC9F0, ORIG_Stream = 0x5DCAF0, ORIG_Blob = 0x5DCF20,
		ORIG_Mesh = 0x5DD030, ORIG_Drips = 0x5DDE90, ORIG_Puddle = 0x5DE0B0, ORIG_Melt = 0x5DE200, ORIG_Splash = 0x5DEA10;
	static const uint32_t SEQ_Blob = 0xD68DF8, SEQ_Drip = 0xD68EA0;
	static const uint32_t MODEL_Mesh = 0xD6904C, MODEL_Puddle = 0xD69CBC;
	static const uint32_t MESH_A = 0xD69054, MESH_B = 0xD69A74;       // 73 vertices of 8 bytes each
	static const int32_t *const DRIP_COUNTS = (const int32_t *)0xD6A3AC; // droplets per tick 0..12
	static const uint32_t SOUND_Acid = 0xD6A3A4;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	inline void GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t mode, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, mode, out); }
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void BezierInit(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }
	inline void BezierPoint(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); }
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
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }
	inline void GteLoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteStoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }
	inline void GteWriteCtrl(uint32_t value, uint32_t reg) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45D7F0)(value, reg); }     // GTE_WriteControlReg
	inline void GteLoadV0SVec3(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }                       // GTE_LoadV0FromSVec3
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void GteStoreIR123SVec(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(v); }                    // GTE_StoreIR123ToSVec
	inline void GteMVMVA_LightIR() { fn<void (__cdecl *)()>(0x460810)(); }                                        // MAC = L * IR + BK
	inline void GteReadMAC1Shl12(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x64DF60)(dst); }
	inline void VectorNormalS16(uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(in, out); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // MAC = L * V0 + BK
	inline void GteSetBKFromMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = matrix translation
	inline void GteSetRotDiagonal16(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DDD0)(s); }     // R11 / R22 / R33 = s
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { InsertPrimAutoDepth(bucket, (void *)pkt); }

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 100 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct Node // every effect-queue task
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t idx;       // +0x0E stream index (director: next stream; drips / splash: owner mask; puddle: delay)
		int16_t pos[4];    // +0x10 landing / target position
		int16_t a18;       // +0x18 blob: curve sample; mesh / puddle: y angle
		int16_t a1a;       // +0x1A blob: speed; mesh: unused random; melt: the target's flag 4 as it was
		int16_t a1c;       // +0x1C blob / mesh / puddle: scale
		int16_t a1e;       // +0x1E mesh / puddle: scale step
		uint32_t pad20;
	};
	struct Stream // 0x110 bytes
	{
		int16_t src[4];      // +0x00 caster's effect point 0x29
		int16_t dst[4];      // +0x08 landing point
		int16_t cp[4][4];    // +0x10 Bezier control points
		int16_t curve[28][4]; // +0x30 curve samples
	};
	struct Drop // droplet entry (0x18 bytes)
	{
		uint32_t flags;    // owner mask (1 << stream: drip emitter, 2: splash), 0 = free
		int16_t frame;     // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t pad0;
		int16_t vel[3];    // +0x10
		int16_t pad1;
	};
#pragma pack(pop)
	static_assert(sizeof(Node) == 0x24 && sizeof(Stream) == 0x110 && sizeof(Drop) == 0x18, "Acid nodes / entries");

	inline Stream *StreamOf(int32_t i) { return (Stream *)(STREAM + (uint32_t)(i * 0x110)); }
	inline Drop *Drops() { return (Drop *)DROPS; }

	// the first free droplet (mask 0) among the first 100, or null
	static Drop *FreeDrop()
	{
		int32_t i = 0;
		Drop *e = Drops();
		for (; e->flags; e++, i++)
			if (P(e + 1) >= DROPS_END) return nullptr;
		return i < 0x64 ? e : nullptr;
	}


	// ==================================================================
	// Module prim renderer (0x5DD300 = Gastric Juice 169's 0x5E0450; header 0x5C bytes on the scratch: +0x00 model, +0x04 vertices,
	// +0x08..+0x0A far colour, +0x0C fade, +0x10 / +0x14 texture page / clut words, +0x18 texture
	// offset, +0x1C flags, +0x20 section cursor, +0x24 NCLIP, +0x2C OTZ, +0x30 GTE FLAG, +0x58 draw
	// mode word)
	// ==================================================================

	// clip bits of one corner: x outside 0..0xA00 -> xbit, y outside 0..0x6C0 -> ybit
	static uint32_t ClipX(uint32_t xy, uint32_t bit) { const int16_t x = S16(xy, 0); return (x < 0 || x > 0xA00) ? bit : 0; }
	static uint32_t ClipY(uint32_t xy, uint32_t bit) { const int16_t y = S16(xy, 2); return (y < 0 || y > 0x6C0) ? bit : 0; }

	// the texture page / clut words of a packet (flags 0x100 / 0x400, 0x200 / 0x800)
	static void PageWords(uint32_t h, uint32_t pk, int32_t page_off)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & 0x400) U16(pk, page_off) = (uint16_t)(U16(pk, page_off) + U16(h, 0x10));
		else if (f & 0x100) U16(pk, page_off) = U16(h, 0x10);
		if (f & 0x800) U16(pk, 0xE) = (uint16_t)(U16(pk, 0xE) + U16(h, 0x14));
		else if (f & 0x200) U16(pk, 0xE) = U16(h, 0x14);
	}

	// the face's semi-transparency bit (flags bit 0 / 1 sets it, bit 2 / 3 clears it)
	static void SemiTrans(uint32_t h, uint32_t pk, uint32_t set, uint32_t clear)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & set) U32(pk, 4) = U32(pk, 4) | 0x2000000;
		if (f & clear) U32(pk, 4) &= 0xFDFFFFFF;
	}

	// 0x5DD3F0 (= 169's 0x5E0540): flat textured triangles (0x14-byte records -> 0x24-byte packets)
	static uint32_t PrimFT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x14)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x1C) = (U32(rec, 8) >> 16) + d;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x16);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			U32(pk, 0x20) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x24;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5DD650 (= 169's 0x5E07A0): flat textured quads (0x18-byte records -> 0x2C-byte packets)
	static uint32_t PrimFT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x18)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x1C) = uv3;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = uv3 >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x16);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			m |= ClipX(pk + 0x20, 8) | ClipY(pk + 0x20, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5DD920 (= 169's 0x5E0A70): Gouraud textured triangles (0x1C-byte records -> 0x2C-byte packets)
	static uint32_t PrimGT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x1C)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = (U32(rec, 8) >> 16) + d;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x1A);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x14, rec + 0x18, pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x14);
				U32(pk, 0x1C) = U32(rec, 0x18);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5DDB80 (= 169's 0x5E0CD0): Gouraud textured quads (0x24-byte records -> 0x38-byte packets)
	static uint32_t PrimGT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x24)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xD000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x24) = uv3;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x30) = uv3 >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x1A);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			GteReadSXY2u(pk + 0x2C);
			GteAVSZ4();
			m |= ClipX(pk + 0x2C, 8) | ClipY(pk + 0x2C, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(S32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x18);
				U32(pk, 0x1C) = U32(rec, 0x1C);
				U32(pk, 0x28) = U32(rec, 0x20);
			}
			U32(pk, 0x34) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x38;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5DD300 = Gastric Juice 169's 0x5E0450: the model's sections (two skipped words, FT3, FT4, two skipped words, GT3, GT4; an
	// empty section is one zero word); vertices = model + 8 unless flag 0x2000, texture offset 0
	// unless flag 0x1000
	static uint32_t PrimRender(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t f = U32(h, 0x1C);
		if (!(f & 0x2000)) U32(h, 4) = U32(h, 0) + 8;
		U32(h, 0x20) = U32(U32(h, 0), 0) + U32(h, 0);
		if (!(f & 0x1000)) U32(h, 0x18) = 0;
		U32(h, 0x58) = 0xE1000220;
		GteSetFarColor(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		return cursor;
	}

}
}

#ifdef FF8_FX_HELD
#include "mag170_acid_held.h"
#endif

namespace ff8fx
{
namespace acid170
{
	// ==================================================================
	// Melting target
	// Header (0xAC bytes on the scratch): +0x00 primitive list, +0x04 vertex buffer, +0x08..+0x0E primitive counts, +0x10 /
	// +0x14..+0x1A as RenderGeometry's, +0x1C code bits (entity +0x28), +0x20 visible objects, +0x28..+0x2A depth cue, +0x2C
	// the vertex's plane distance << 12, +0x30 the plane offset << 12, +0x34 -normal y, +0x38 the pull, +0x3C the melt amount,
	// +0x40 the squared radius, +0x44 NCLIP, +0x4C OTZ, +0x50 GTE FLAG, +0x54 a vector, +0x74 the plane normal (3 words) and
	// -offset (+0x7A), +0x7C the landing point, +0x84 the target's position, +0x8A spread (body 1, weapon 0), +0x8C camera x entity
	// ==================================================================

	// 0x5DE7C0: the object's flat textured triangles (0x10-byte records) and quads (0x14-byte records)
	static uint32_t FlatPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pk = cursor;
		const uint32_t verts = U32(hdr, 4);
		uint32_t rec = U32(hdr, 0);
		for (uint32_t i = 0; i < U16(hdr, 8); i++, rec += 0x10)
		{
			GteLoadV012(verts + (U16(rec, 0) & 0xFFF) * 8, verts + (U16(rec, 2) & 0xFFF) * 8, verts + (U16(rec, 4) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t w = U32(rec, 0xC);
			U32(pk, 0x14) = w & 0x1FFFFFF;
			U32(pk, 0) = 0x7000000;
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			U32(pk, 0xC) = U32(rec, 8);
			GteReadFLAG(hdr + 0x50);
			if (U32(hdr, 0x50) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(hdr + 0x44);
			if (S32(hdr, 0x44) <= 0) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			S32(pk, 0x1C) = S32(rec, 4) >> 16;
			GteReadOTZ32(hdr + 0x4C);
			InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), pk);
			pk += 0x20;
		}
		const uint32_t quads = rec + 2;
		for (uint32_t i = 0; i < U16(hdr, 0xA); i++)
		{
			const uint32_t q = quads + i * 0x14;
			GteLoadV012(verts + (U16(q, -2) & 0xFFF) * 8, verts + (U16(q, 0) & 0xFFF) * 8, verts + (U16(q, 2) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t w = U32(q, 0xA);
			U32(pk, 0x14) = w & 0x1FFFFFF;
			U32(pk, 0) = 0x9000000;
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
			U32(pk, 0xC) = U32(q, 6);
			GteReadFLAG(hdr + 0x50);
			if (U32(hdr, 0x50) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(hdr + 0x44);
			if (S32(hdr, 0x44) <= 0) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(verts + (U16(q, 4) & 0xFFF) * 8);
			GteRTPS();
			U32(pk, 0x1C) = U32(q, 0xE);
			S32(pk, 0x24) = S32(q, 0xE) >> 16;
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			GteReadOTZ32(hdr + 0x4C);
			InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), pk);
			pk += 0x28;
		}
		return pk;
	}

	// one vertex out of the object's bone (camera-less world space) at out: beyond the plane (L x V + BK > 0) and within the
	// radius of the landing point, pulled down by the melt; above the ground it is flattened at y 0 and (body) spread outward
	// from the target's position
	static void MeltVertex(uint32_t hdr, uint32_t out)
	{
		GteStoreIR123SVec(out);
		GteMVMVA_LightIR();
		GteReadMAC1Shl12(hdr + 0x2C);
		const int32_t dist = S32(hdr, 0x2C);
		S32(hdr, 0x38) = (int32_t)S16(hdr, 0x7A) + (dist >> 12);
		if (S32(hdr, 0x38) <= 0) return;
		const int16_t ox = S16(out, 0), oz = S16(out, 4);
		const int16_t dy = (int16_t)(S16(out, 2) - S16(hdr, 0x7E));
		const int16_t dx = (int16_t)(ox - S16(hdr, 0x7C));
		const int16_t dz = (int16_t)(oz - S16(hdr, 0x80));
		S16(hdr, 0x56) = dy;
		S16(hdr, 0x54) = dx;
		S16(hdr, 0x58) = dz;
		int32_t sq = mul32(dx, dx) + mul32(dy, dy) + mul32(dz, dz);
		const int32_t r2 = S32(hdr, 0x40);
		if (sq >= r2) return;
		if (sq < 0x9C400) sq = 0x9C400;
		const int32_t f = shl32(sq, 12) / r2;
		const int32_t q = (S32(hdr, 0x30) - dist) / S32(hdr, 0x34);
		int32_t m = mul32(mul32(f, S32(hdr, 0x3C)) >> 9, q) >> 12;
		if (m < 0) m = -m;
		S32(hdr, 0x38) = m;
		S16(out, 2) = (int16_t)(S16(out, 2) + (int16_t)((m >> 1) + (int16_t)m));
		if (S16(out, 2) <= 0) return;
		S16(out, 2) = 0;
		if (!S16(hdr, 0x8A)) return;
		S16(hdr, 0x56) = 0;
		S16(hdr, 0x54) = (int16_t)(ox - S16(hdr, 0x84));
		S16(hdr, 0x58) = (int16_t)(oz - S16(hdr, 0x88));
		VectorNormalS16(hdr + 0x54, hdr + 0x54);
		S16(out, 0) = (int16_t)(S16(out, 0) + (int16_t)(mul32(S16(hdr, 0x54), S32(hdr, 0x38)) >> 12));
		S16(out, 4) = (int16_t)(S16(out, 4) + (int16_t)(mul32(S16(hdr, 0x58), S32(hdr, 0x38)) >> 12));
	}

	// 0x5DE500: the plane into the light matrix's first row and BK red; every visible object: its vertices through their bone
	// matrices (MeltVertex) into the vertex buffer, then its faces through the camera (FlatPrims)
	static uint32_t MeltObjects(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		GteWriteCtrl(U32(hdr, 0x74), 8);
		GteWriteCtrl(U32(hdr, 0x78), 9);
		GteWriteCtrl((uint32_t)(S32(hdr, 0x78) >> 16), 0xD);
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(hdr, 4);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)(bone * 0x30) + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--, out += 8)
				{
					GteLoadV0SVec3(obj);
					obj += 6;
					GteMVMVA_RotV0Tr2();
					MeltVertex(hdr, out);
				}
			}
			obj = (obj + 3) & ~3u;
			U16(hdr, 8) = U16(obj, 0);
			U16(hdr, 0xA) = U16(obj, 2);
			U16(hdr, 0xC) = U16(obj, 4);
			U16(hdr, 0xE) = U16(obj, 6);
			U32(hdr, 0) = obj + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			cursor = FlatPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x5DE3E0: the entity's shadow, its model, its weapon (+0x78), both through MeltObjects; the bone matrices are rebuilt from
	// the pose afterwards
	static void ModelRender(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_model_pose(entity);)
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0x8C));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 4) = var<uint32_t>(0x1D98B3C);
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		U16(hdr, 0x8A) = 1;
		FrameCursor() = MeltObjects(U32(anim, 4), hdr, RenderOT44(), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t weapon = U32(entity, 0x78);
		if (weapon && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)weapon, (void *)(entity + 0x40));
			U16(hdr, 0x8A) = 0;
			FrameCursor() = MeltObjects(U32(weapon, 4), hdr, RenderOT44(), 2, FrameCursor());
			BuildBoneMatricesFromPose((void *)weapon);
		}
	}

	// the melt amount of a tick: up to tick 0x1C, then down
	static int32_t MeltAmount(int32_t c) { return c < 0x1C ? mul32(c, 81) : 0x305B - mul32(c, 364); }

	// the plane through the landing point whose normal points from it to the caster, the target hidden and redrawn melting
	static void MeltDraw(const Node *n, int32_t amount)
	{
		const Stream *s = StreamOf(n->idx);
		int32_t d[3] = { (int32_t)s->src[0] - s->dst[0], (int32_t)s->src[1] - s->dst[1], (int32_t)s->src[2] - s->dst[2] };
		NormalizeVector(d, d);
		const uint32_t h = P(FieldAlloc(0xAC));
		U32(h, 0x84) = *(const uint32_t *)&n->pos[0];
		U32(h, 0x88) = *(const uint32_t *)&n->pos[2];
		U32(h, 0x7C) = *(const uint32_t *)&s->dst[0];
		U32(h, 0x80) = *(const uint32_t *)&s->dst[2];
		S32(h, 0x40) = 0x5F5E10;
		S16(h, 0x74) = (int16_t)d[0];
		S16(h, 0x76) = (int16_t)d[1];
		S16(h, 0x78) = (int16_t)d[2];
		const int32_t x = s->dst[0], y = s->dst[1], z = s->dst[2];
		S16(h, 0x7A) = (int16_t)-((mul32(z, d[2]) + mul32(y, d[1]) + mul32(x, d[0])) >> 12);
		S32(h, 0x34) = (int32_t)((uint32_t)-S16(h, 0x76) << 12) >> 12;
		S32(h, 0x30) = mul32(S16(h, 0x78), z) + mul32(S16(h, 0x74), x) + mul32(S16(h, 0x76), y);
		S32(h, 0x3C) = amount;
		uint8_t *entity = Entity(TargetSlot());
		entity[0] |= 4;
		ModelRender(h, P(entity));
		FieldFree(0xAC);
	}

	static uint32_t __cdecl MeltTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (n->counter < 0x22)
		{
			// 30 fps layer: see mag170_acid_held.inc
			FX_HELD(held_note_draw(ORIG_Melt, n);)
			MeltDraw(n, MeltAmount(n->counter));
		}
		if (DrawOnly()) return 0;
		n->counter++;
		if (n->counter <= 0x22) return 0;
		uint16_t &w = *(uint16_t *)Entity(TargetSlot());
		w = (uint16_t)((w & 0xFFFB) | (uint16_t)n->a1a);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Landing mesh (0x5DD030)
	// ------------------------------------------------------------------
	// blend weight of the second shape: sin(tick * 1024 / 32)
	static int32_t MeshBlend(int32_t c) { return ComputeSin(shl32(c, 10) / 32); }

	// fade of the mesh by tick (0 where the draw leaves the header's fade word as it is)
	static int32_t MeshFade(int32_t c)
	{
		if (c < 8) return 0;
		if (c < 0x10) return shl32(c - 8, 9);
		if (c < 0x18) return shl32(8 - (c - 0x10), 9);
		return shl32(c - 0x18, 9);
	}

	// the mesh at the landing point (vertices blended by s into verts); nothing from tick 32
	static void MeshDraw(const Node *n, uint32_t verts, int32_t s, int32_t fade)
	{
		int16_t ang[3] = { 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->pos[0];
		m.t[1] = n->pos[1];
		const int32_t sc[3] = { n->a1c, n->a1c, n->a1c };
		Scale3DMatrix(&m, sc);
		m.t[2] = n->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0x5C));
		U32(h, 0) = MODEL_Mesh;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x2033;
		for (int32_t k = 0; k < 0x49; k++)
		{
			const uint32_t a = MESH_A + (uint32_t)k * 8, b = MESH_B + (uint32_t)k * 8, o = verts + (uint32_t)k * 8;
			for (int32_t w = 0; w < 3; w++)
				S16(o, w * 2) = (int16_t)((mul32(S16(b, w * 2) - S16(a, w * 2), s) >> 12) + S16(a, w * 2));
		}
		U32(h, 4) = verts;
		const int32_t c = n->counter;
		if (c < 8)
			PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		else if (c < 0x10)
		{
			S32(h, 0xC) = fade;
			U32(h, 0x1C) |= 0xC0;
			PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		}
		else if (c < 0x20)
		{
			U32(h, 0x1C) |= 0x4C0;
			S32(h, 0x10) = 0x20;
			S32(h, 0xC) = fade;
			PacketCursor() = PrimRender(h, RenderOT44(), 2, PacketCursor());
		}
		FieldFree(0x5C);
	}

	static uint32_t __cdecl MeshTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_draw(ORIG_Mesh, n);)
		MeshDraw(n, MESH_VERTS + (uint32_t)(n->idx * 0x248), MeshBlend(n->counter), MeshFade(n->counter));
		if (DrawOnly()) return 0;
		if (n->counter < 8)
		{
			const int16_t d = n->a1e;
			n->a1c = (int16_t)(n->a1c + d);
			n->a1e = (int16_t)(d - d / 2);
		}
		n->counter++;
		return n->counter >= 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Puddle (0x5DE0B0)
	// ------------------------------------------------------------------
	static int32_t PuddleFade(int32_t c) { return c < 8 ? 0x800 : shl32(c, 8); }

	static void PuddleDraw(const Node *n, int32_t fade)
	{
		int16_t ang[3] = { 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->pos[0];
		m.t[1] = n->pos[1];
		const int32_t sc[3] = { n->a1c, n->a1c, n->a1c };
		Scale3DMatrix(&m, sc);
		m.t[2] = n->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Puddle;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0xF3;
		S32(h, 0xC) = fade;
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl PuddleTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (n->idx > 0)
		{
			if (DrawOnly()) return 0;
			n->idx--;
			return 0;
		}
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_draw(ORIG_Puddle, n);)
		PuddleDraw(n, PuddleFade(n->counter));
		if (DrawOnly()) return 0;
		n->a1c = (int16_t)(n->a1c + n->a1e);
		n->counter++;
		return n->counter >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Blob (0x5DCF20): 4 sprites at curve samples a18, a18 - 1, ... (radius - 0x70 each)
	// ------------------------------------------------------------------
	// pos(i): the point of sprite i (null: skipped); a sample before the curve's start ends the row
	static void BlobDraw(const Node *n, const int16_t *const pos[4])
	{
		const uint32_t h = P(FieldAlloc(0xB4));
		int32_t r = n->a1c;
		U16(h, 0x24) = 0;
		U32(h, 0) = SEQ_Blob;
		U16(h, 4) = (uint16_t)(n->counter % 6);
		int32_t k = n->a18;
		for (int i = 0; i < 4; i++, k--, r -= 0x70)
		{
			if (k < 0x1C)
			{
				if (k < 0) break;
				TransformCameraByShadowRotation(pos[i], r, -(r >> 3));
				PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
			}
		}
		FieldFree(0xB4);
	}

	static uint32_t __cdecl BlobTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_draw(ORIG_Blob, n);)
		const int16_t *pos[4];
		for (int i = 0; i < 4; i++) pos[i] = StreamOf(n->idx)->curve[0] + (n->a18 - i) * 4;
		BlobDraw(n, pos);
		if (DrawOnly()) return 0;
		const int16_t v = n->a1a;
		n->a18 = (int16_t)(n->a18 + v);
		if (v > 2 && n->a18 < 0xE) n->a1a = (int16_t)(v - 1);
		n->counter++;
		return n->a18 >= 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Drip emitter (0x5DDE90)
	// ------------------------------------------------------------------
	static void DripDraw(uint32_t h, const Drop *e)
	{
		TransformCameraByShadowRotation(e->pos, (int32_t)(uint16_t)e->scale, -(e->scale >> 4));
		U16(h, 4) = (uint16_t)(e->frame >> 1);
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t __cdecl DripTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_draw(ORIG_Drips, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int32_t alive = 0;
		U32(h, 0) = SEQ_Drip;
		U16(h, 0x24) = 0;
		for (Drop *e = Drops(); P(e) < DROPS_END; e++)
		{
			if (!(e->flags & (uint32_t)(int32_t)n->idx)) continue;
			DripDraw(h, e);
			if (DrawOnly()) continue;
			e->frame++;
			if (S16(h, 0x28) < 0)
			{
				e->flags = 0;
				continue;
			}
			e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
			e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 4));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = n->counter;
		if (c >= 0 && c <= 0xC && DRIP_COUNTS[c] > 0)
		{
			for (int32_t k = 0; k < DRIP_COUNTS[n->counter]; k++)
			{
				Drop *e = FreeDrop();
				if (!e) break;
				e->flags = (uint32_t)(int32_t)n->idx;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0xA00 + 0x700);
				memcpy(e->pos, n->pos, 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(CrtRand() % 0x578 - 0x2BC));
				e->pos[1] = (int16_t)(e->pos[1] + (int16_t)(-100 - CrtRand() % 300));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(CrtRand() % 0x578 - 0x2BC));
				e->vel[1] = (int16_t)(-20 - CrtRand() % 50);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Splash (0x5DEA10)
	// ------------------------------------------------------------------
	// the light matrix: the camera x (rotation taking the down axis onto the direction 0x26 -> 0x28, at 0x28); the splash
	// matrix's rotation goes to the GTE
	static void SplashSetup()
	{
		int32_t down[3] = { 0, -0x1000, 0 };
		int32_t dir[3];
		dir[0] = (int32_t)S16(SPLASH_AT, 0) - S16(SPLASH_FROM, 0);
		dir[1] = (int32_t)S16(SPLASH_AT, 2) - S16(SPLASH_FROM, 2);
		dir[2] = (int32_t)S16(SPLASH_AT, 4) - S16(SPLASH_FROM, 4);
		NormalizeVector(dir, dir);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(down, dir, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = S16(SPLASH_AT, 0);
		m.t[1] = S16(SPLASH_AT, 2);
		m.t[2] = S16(SPLASH_AT, 4);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrixCtrl((const Mat4x3 *)SPLASH_MATRIX);
		GteSetLightMatrix(&m);
		GteSetBKFromMatrix(&m);
	}

	// one droplet: grey 0x80 then (11 - frame) * 16 from frame 3, its camera-space position (light matrix + BK) pulled toward
	// the camera by scale / 16 into the splash matrix's translation, the scale on the rotation diagonal, frame / 2
	static void SplashDraw(uint32_t h, const Drop *e, int32_t grey)
	{
		Mat4x3 &g = *(Mat4x3 *)SPLASH_MATRIX;
		U32(h, 0x1C) = (uint32_t)((((grey << 8) | grey) << 8) | grey);
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotDiagonal16(e->scale);
		U16(h, 4) = (uint16_t)(e->frame >> 1);
		GteReadMAC123(g.t);
		int32_t nrm[3];
		NormalizeVector(g.t, nrm);
		const int32_t f = -(e->scale >> 4);
		g.t[0] = (int32_t)((uint32_t)g.t[0] + (uint32_t)(mul32(f, nrm[0]) >> 12));
		g.t[1] = (int32_t)((uint32_t)g.t[1] + (uint32_t)(mul32(f, nrm[1]) >> 12));
		g.t[2] = (int32_t)((uint32_t)g.t[2] + (uint32_t)(mul32(f, nrm[2]) >> 12));
		GteSetTransVectorCtrl(&g);
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static int32_t SplashGrey(int32_t frame) { return frame >= 3 ? shl32(0xB - frame, 4) : 0x80; }

	static uint32_t __cdecl SplashTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_draw(ORIG_Splash, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int32_t alive = 0;
		U32(h, 0) = SEQ_Blob;
		U16(h, 0x24) = 4;
		SplashSetup();
		for (Drop *e = Drops(); P(e) < DROPS_END; e++)
		{
			if (!(e->flags & (uint32_t)(int32_t)n->idx)) continue;
			SplashDraw(h, e, SplashGrey(e->frame));
			if (DrawOnly()) continue;
			e->frame++;
			if (e->frame >= 0xB)
			{
				e->flags = 0;
				continue;
			}
			for (int k = 0; k < 3; k++) e->pos[k] = (int16_t)(e->pos[k] + e->vel[k]);
			for (int k = 0; k < 3; k++) e->vel[k] = (int16_t)(e->vel[k] - (int16_t)(e->vel[k] >> 2));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 0x10; k++)
			{
				Drop *e = FreeDrop();
				if (!e) break;
				e->flags = (uint32_t)(int32_t)n->idx;
				e->frame = 0;
				e->scale = (int16_t)((CrtRand() & 0x3F) + 0x60);
				e->pos[0] = (int16_t)(CrtRand() % 200 - 100);
				e->pos[1] = 0;
				e->pos[2] = (int16_t)(CrtRand() % 200 - 100);
				const int32_t a = CrtRand() & 0xFFF;
				const int32_t r = CrtRand() % 0x46 + 0x32;
				e->vel[0] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				e->vel[1] = (int16_t)(-0x3C - CrtRand() % 0x50);
				e->vel[2] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Stream (0x5DCAF0): the curve, then the blob, puddles, melting target, mesh and drip emitter
	// ------------------------------------------------------------------
	static void StreamSetup(Stream *s)
	{
		GetEffectSpawnPosition(Entity(AttackerSlot()), 0x29, 0x600, s->src);
		GetDefaultEffectPosition(Entity(TargetSlot()), s->dst);
		s->dst[0] = (int16_t)(s->dst[0] + (int16_t)(CrtRand() % 0x2BC - 0x15E));
		s->dst[1] = 0;
		s->dst[2] = (int16_t)(s->dst[2] + (int16_t)(CrtRand() % 0x2BC - 0x15E));
		memcpy(s->cp[0], s->src, 8);
		memcpy(s->cp[3], s->dst, 8);
		const int16_t x0 = s->src[0];
		const int32_t dx = (s->dst[0] - x0) / 3;
		const int32_t dz = (s->dst[2] - s->src[2]) / 3;
		s->cp[1][0] = (int16_t)(x0 + dx);
		s->cp[1][1] = (int16_t)(s->src[1] - (int16_t)(CrtRand() % 100) - 0x32);
		s->cp[1][2] = (int16_t)(s->src[2] + dz);
		s->cp[2][0] = (int16_t)(dx * 2 + s->src[0]);
		s->cp[2][1] = (int16_t)(s->cp[1][1] - (int16_t)(CrtRand() % 100) - 0x32);
		s->cp[2][2] = (int16_t)(dz * 2 + s->src[2]);
		void *work = FieldAlloc(0x190);
		BezierInit(4, s->cp, work);
		for (int32_t t = 0, k = 0; t < 0x1C000; t += 0x1000, k++)
			BezierPoint(4, work, s->curve[k], t / 27);
		FieldFree(0x190);
	}

	static Node *NewNode(uint32_t f)
	{
		Node *t = (Node *)AddTaskToQueue(EffectQueue(), f);
		t->counter = 0;
		return t;
	}

	static uint32_t __cdecl StreamTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node *n = (Node *)tn;
		Stream *s = StreamOf(n->idx);
		if (n->counter == 0) StreamSetup(s);
		if (n->counter == 1)
		{
			Node *b = NewNode(ORIG_Blob);
			b->idx = n->idx;
			b->a18 = 0;
			b->a1a = 5;
			b->a1c = (int16_t)(CrtRand() % 0x50 + 0x300);
		}
		if (n->counter == 0xD)
		{
			Node *m = NewNode(ORIG_Mesh);
			memcpy(m->pos, s->dst, 8);
			m->idx = n->idx;
			m->a18 = (int16_t)(CrtRand() & 0xFFF);
			m->a1a = (int16_t)(CrtRand() % 6 + 0x10);
			m->a1e = m->a1c = (int16_t)(((CrtRand() & 0x3FF) + 0xB00) / 3);
			Node *d = NewNode(ORIG_Drips);
			d->idx = (int16_t)(1 << (U8(P(n), 0xE) & 31));
			memcpy(d->pos, s->dst, 8);
		}
		if (n->counter == 0xC)
		{
			for (int32_t k = 0; k < 4; k++)
			{
				Node *p = NewNode(ORIG_Puddle);
				p->idx = (int16_t)(CrtRand() % 6 + k);
				p->pos[0] = (int16_t)(CrtRand() % 0x640 + s->dst[0] - 0x320);
				p->pos[1] = s->dst[1];
				p->pos[2] = (int16_t)(CrtRand() % 0x640 + s->dst[2] - 0x320);
				p->a18 = (int16_t)(CrtRand() & 0xFFF);
				p->a1e = p->a1c = (int16_t)((CrtRand() % 0x600 + 0xA00) / 10);
			}
			Node *m = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Melt);
			m->idx = n->idx;
			m->counter = 0;
			GetDefaultEffectPosition(Entity(TargetSlot()), m->pos);
			const int16_t y = m->pos[1];
			m->pos[1] = m->pos[3];
			m->pos[3] = y;
			m->a1a = (int16_t)(Entity(TargetSlot())[0] & 4);
		}
		n->counter++;
		return n->counter >= 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5DC9F0) and root (0x5DEDF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node *n = (Node *)tn;
		if (n->counter == 1)
		{
			Node *s = NewNode(ORIG_Stream);
			s->idx = n->idx;
			n->idx++;
			GetEffectSpawnPosition(Entity(AttackerSlot()), 0x28, 0, (int16_t *)SPLASH_AT);
			GetEffectSpawnPosition(Entity(AttackerSlot()), 0x26, 0, (int16_t *)SPLASH_FROM);
			Node *p = NewNode(ORIG_Splash);
			p->idx = (int16_t)(1 << (U8(P(n), 0xE) & 31));
		}
		if (n->counter == 0x2E) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (n->counter == 1) BdPlaySE((const void *)SOUND_Acid, 0, 0x80);
		n->counter++;
		return n->counter > 0x37 ? TASK_END : 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? TexBase() + 0x8000 : TexBase();
		const int running = ExecuteTaskQueue(EffectQueue());
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	void register_mag170_acid_ports()
	{
		register_port(ORIG_Root, (void *)RootTask, "170 RootTask", 170);
		register_port(ORIG_Director, (void *)DirectorTask, "170 DirectorTask", 170);
		register_port(ORIG_Stream, (void *)StreamTask, "170 StreamTask", 170);
		register_port(ORIG_Blob, (void *)BlobTask, "170 BlobTask", 170);
		register_port(ORIG_Mesh, (void *)MeshTask, "170 MeshTask", 170);
		register_port(ORIG_Drips, (void *)DripTask, "170 DripTask", 170);
		register_port(ORIG_Puddle, (void *)PuddleTask, "170 PuddleTask", 170);
		register_port(ORIG_Melt, (void *)MeltTask, "170 MeltTask", 170);
		register_port(ORIG_Splash, (void *)SplashTask, "170 SplashTask", 170);
		// 30 fps layer: see mag170_acid_held.inc
		FX_HELD(register_mag170_held();)
	}
}

	void register_mag170_acid() { acid170::register_mag170_acid_ports(); }
	FX_LAB_MODULE(register_mag170_acid);
}

#ifdef FF8_FX_HELD
#include "mag170_acid_held.inc"
#endif
