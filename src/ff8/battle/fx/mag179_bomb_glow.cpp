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

// Effect 179: Bomb glow (enemy attacks 82 and 83 of kernel.bin, both unnamed, used by the Bomb
// c0m043; MAG_179_*). The Bomb glows: its body is redrawn with a scrolling additive texture
// overlay in a cycling colour while fuse sparks fly off its bones.
//
// Structure (setup MAG_179_UNK7 0x5CDF40, file loader 0x5CDF20 = the texture file named at
// 0xD44990 (mag178.tim); the setup keeps the cast context 0x230AD00, the target slot 0x2309558 and
// the CASTER's slot 0x230AD04, sets up the root queue 0x2309570 (1 node) and the effect queue
// 0x2309EE0 (0x64 nodes of 0x24 bytes) with the director, clears the particle pool, starts camera
// animation 0xD445C8 and queues the TIM):
//   RootTask (0x5CED70) - alternates the module packet arena (0x230AD0C / 0x2312D0C, 0x8000 each)
//     on the counter's parity (cursor 0x231AD0C), runs the effect queue, ends when it is empty.
//   Director (0x5CDFF0) - one node, counter 0..0x28 (nothing in the draw-only mode): 1 = the
//     caster glow task (keeping the caster's entity flag bit 2 in its node +0x1A), the fuse sparks
//     task and sound 0xD44968 at the caster's default effect position, 0x23 = damage (action 0's
//     targets).
//   CasterGlow (0x5CE0F0) - counters 0..0x1D: the caster's battle model drawn by the module
//     (entity flags |= 4: the battle renderer skips it) with every visible polygon followed by an
//     additive copy textured through a texture window (x 0, y 0x80, 0x40 x 0x80) whose v
//     coordinates scroll with the counter (v = (vertex y + counter * 72) / 3, u = vertex x); the
//     overlay colour is the GTE depth cue (DPCS, far colour 0) of the colour table 0xD44970
//     (8 entries, cycled by the counter) by IR0 = (4 - counter) << 10 below 4 (fade in) and
//     (counter - 0x16) << 9 from 0x16 (fade out). After counter 0x1E the entity flag bit is put back.
//     Model draw = shadow + a copy of the battle model renderer (0x5CE3B0 / 0x5CE520) that adds the
//     overlay polygons; the second model (+0x78) through the engine renderer 0x5099D0 (mode 4).
//   FuseSparks (0x5CEB10) - particles of type 1, flipbook 0xD44700 randomly mirrored, 3 per tick
//     0..0x16 at random effect bones of the caster.
// Particle pool 0x2309580: 0x64 entries of 0x18 bytes (only the fuse sparks use it); the task
// draws then updates its entries (in the draw-only mode it only draws), spawns into free entries
// and ends at counter >= 4 once no entry is left.
// Module globals: 0x2309558..0x231AD10 (target slot, root pool / queue, particle pool, effect
// queue / pool, context 0x230AD00, caster slot 0x230AD04, texture file 0x230AD08, the two packet
// arenas, packet cursor 0x231AD0C).

#include "mag_common.h"

namespace ff8fx
{
namespace bombglow179
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the model renderer follows the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x230AD00); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x230AD04); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x231AD0C); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x2309EE0; }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }       // frame packet arena cursor
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }     // transformed model vertices
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }
	static const uint32_t ARENA0 = 0x230AD0C, ARENA1 = 0x2312D0C, ARENA_SIZE = 0x8000;
	static const uint32_t POOL = 0x2309580, POOL_END = 0x2309EE0;             // 0x64 particles of 0x18 bytes

	static const uint32_t ORIG_RootTask = 0x5CED70;
	static const uint32_t ORIG_Director = 0x5CDFF0;
	static const uint32_t ORIG_CasterGlow = 0x5CE0F0;
	static const uint32_t ORIG_FuseSparks = 0x5CEB10;
	static const uint32_t SEQ_Spark = 0xD44700;
	static const uint32_t GLOW_COLOURS = 0xD44970;                             // 8 GTE RGBC words
	static const void *const SOUND_Glow = (const void *)0xD44968;

	// ---- engine functions (original addresses)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }      // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }     // set_param_with_dword_1CA8A68
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(bucket, pkt); }
	// GTE helpers of the Tornado module
	inline void GteLoadV0Words(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }    // MAG_146_sub_64DEE0: V0 = 3 words
	inline void GteStoreIRVertex(uint32_t out) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(out); } // MAG_146_sub_64DF10: IR1..3 -> 4 words (4th 0)
	// battle model helpers
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline uint32_t RenderGeometry(uint32_t model, uint32_t h, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x5099D0)(model, h, ot, mode, cursor); }
	// GetEffectSpawnPosition (0x502170): position of an entity's effect bone
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }

	// ------------------------------------------------------------------
	// Node / particle layouts
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
		int16_t counter;     // +0x0C
		uint8_t pad0E[0xC];
		uint16_t hidden;     // +0x1A glow: the caster's entity flag bit 2 before the glow
		uint8_t pad1C[8];
	};
	struct Particle // 0x18 bytes
	{
		uint32_t type;       // +0x00 1 = fuse spark (0 = free)
		int16_t frame;       // +0x04 flipbook frame
		int16_t scale;       // +0x06
		int16_t pos[4];      // +0x08 x, y, z (+ the 4th word of GetEffectSpawnPosition)
		int16_t mirror[2];   // +0x10 x / y mirror (0x1000 / -0x1000)
		uint32_t pad14;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "Bomb glow nodes");
	static_assert(sizeof(Particle) == 0x18, "Bomb glow particle");

	inline Particle *Pool() { return (Particle *)POOL; }
	static const int POOL_COUNT = 0x64;
	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }

	// a free pool entry (the scan stops at the pool's end), or -1
	static int FreeParticle()
	{
		int i = 0;
		for (uint32_t p = POOL; ; p += 0x18, i++)
		{
			if (U32(p, 0) == 0) break;
			if (p + 0x18 >= POOL_END) return -1;
		}
		if (i >= 0x64) return -1;
		return i;
	}

	// ------------------------------------------------------------------
	// Caster model renderer (copies of the battle model renderer)
	// ------------------------------------------------------------------
	// draw block h (0xB0 bytes of Field_Alloc scratch): +0 primitives, +4 vertices, +8 / +0A
	// triangle / quad counts, +1C colour / mode bits, +20 object mask, +30 v scroll, +34 texture
	// window (x, y bytes, w, h words), +3C MAC0, +44 OTZ, +48 FLAG, +4C..+6B the polygon's vertices,
	// +6C overlay colour, +70..+7F the v of each vertex, +88 / +8C texture window commands, +90
	// matrix

	// the v of a vertex: its y plus the scroll, / 3
	static int32_t ScrollV(int16_t y, int32_t scroll) { return (y + scroll) / 3; }
	// the overlay uv word of a vertex when the polygon spans two v pages: v re-based on h +3A
	static uint16_t RebaseUV(uint32_t h, int32_t o, int32_t q, int32_t base)
	{
		return (uint16_t)(((uint32_t)(q - base) << 8) ^ AT<uint8_t>(h, o));
	}

	// MAG_179_sub_5CE520: the polygons of one object; every visible textured polygon is followed
	// by its overlay copy one bucket nearer (POLY_FT3 / FT4 0x26 / 0x2E, clut 0x3F54, tpage 0xBA,
	// texture window h +88, reset h +8C)
	static uint32_t RenderObjectPrims(uint32_t h, uint32_t ot, uint32_t shift, uint32_t pkt)
	{
		const int32_t base = S16(h, 0x3A);
		const uint32_t verts = U32(h, 4);
		uint32_t prim = U32(h, 0);
		for (int32_t i = 0; i < (int32_t)U16(h, 8); i++, prim += 0x10)
		{
			GteLoadV012(verts + (U16(prim, 0) & 0xFFF) * 8, verts + (U16(prim, 2) & 0xFFF) * 8, verts + (U16(prim, 4) & 0xFFF) * 8);
			GteRTPT();
			U32(pkt, 0x14) = U32(prim, 0xC);
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = U32(h, 0x1C) | 0x24000000;
			U32(pkt, 0xC) = U32(prim, 8);
			GteReadFLAG(h + 0x48);
			if (U32(h, 0x48) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x3C);
			if (S32(h, 0x3C) <= 0) continue;
			const uint32_t xy0 = pkt + 8, xy1 = pkt + 0x10, xy2 = pkt + 0x18;
			GteReadSXY012(xy0, xy1, xy2);
			GteAVSZ3();
			S32(pkt, 0x1C) = S32(prim, 4) >> 16;
			GteReadOTZ32(h + 0x44);
			int32_t z = S32(h, 0x44) >> shift;
			InsertPrim(ot + (uint32_t)z * 4, pkt);
			const uint32_t v0 = verts + (U16(prim, 0) & 0xFFF) * 8, v1 = verts + (U16(prim, 2) & 0xFFF) * 8, v2 = verts + (U16(prim, 4) & 0xFFF) * 8;
			U32(h, 0x4C) = U32(v0, 0);
			U32(h, 0x50) = U32(v0, 4);
			U32(h, 0x54) = U32(v1, 0);
			U32(h, 0x58) = U32(v1, 4);
			U32(h, 0x5C) = U32(v2, 0);
			U32(h, 0x60) = U32(v2, 4);
			const int32_t scroll = S32(h, 0x30);
			const int32_t q0 = ScrollV(S16(h, 0x4E), scroll);
			S32(h, 0x70) = q0;
			const int32_t q1 = ScrollV(S16(h, 0x56), scroll);
			S32(h, 0x74) = q1;
			const int32_t q2 = ScrollV(S16(h, 0x5E), scroll);
			S32(h, 0x78) = q2;
			const uint16_t p0 = (uint16_t)q0 & 0xFF00, p1 = (uint16_t)q1 & 0xFF00, p2 = (uint16_t)q2 & 0xFF00;
			U16(h, 0x50) = p0;
			U16(h, 0x58) = p1;
			U16(h, 0x60) = p2;
			if (p0 != p1 || p0 != p2)
			{
				U16(h, 0x52) = RebaseUV(h, 0x52, q0, base);
				U16(h, 0x5A) = RebaseUV(h, 0x5A, q1, base);
				U16(h, 0x62) = RebaseUV(h, 0x62, q2, base);
			}
			if (z > 0) z--;
			pkt += 0x20;
			U32(pkt, 8) = U32(h, 0x6C) | 0x26000000;
			U16(pkt, 0x10) = U16(h, 0x52);
			U16(pkt, 0x18) = U16(h, 0x5A);
			U16(pkt, 0x20) = U16(h, 0x62);
			U32(pkt, 0) = 0x9000000;
			U32(pkt, 0xC) = U32(xy0, 0);
			U32(pkt, 0x14) = U32(xy1, 0);
			U16(pkt, 0x12) = 0x3F54;
			U32(pkt, 0x1C) = U32(xy2, 0);
			U32(pkt, 4) = U32(h, 0x88);
			U16(pkt, 0x1A) = 0xBA;
			U32(pkt, 0x24) = U32(h, 0x8C);
			InsertPrim(ot + (uint32_t)z * 4, pkt);
			pkt += 0x28;
		}
		if (U16(h, 0xA) != 0)
		{
			for (int32_t i = 0; i < (int32_t)U16(h, 0xA); i++, prim += 0x14)
			{
				GteLoadV012(verts + (U16(prim, 0) & 0xFFF) * 8, verts + (U16(prim, 2) & 0xFFF) * 8, verts + (U16(prim, 4) & 0xFFF) * 8);
				GteRTPT();
				U32(pkt, 0xC) = U32(prim, 8);
				U32(pkt, 0) = 0x9000000;
				U32(pkt, 4) = U32(h, 0x1C) | 0x2C000000;
				U32(pkt, 0x14) = U32(prim, 0xC);
				GteReadFLAG(h + 0x48);
				if (U32(h, 0x48) & 0x60000) continue;
				GteNCLIP();
				GteReadMAC0(h + 0x3C);
				if (S32(h, 0x3C) <= 0) continue;
				const uint32_t xy0 = pkt + 8, xy1 = pkt + 0x10, xy2 = pkt + 0x18, xy3 = pkt + 0x20;
				GteReadSXY012(xy0, xy1, xy2);
				GteLoadV0((const void *)(verts + (U16(prim, 6) & 0xFFF) * 8));
				GteRTPS();
				U32(pkt, 0x1C) = U32(prim, 0x10);
				S32(pkt, 0x24) = S32(prim, 0x10) >> 16;
				GteReadSXY2((void *)xy3);
				GteAVSZ4();
				GteReadOTZ32(h + 0x44);
				int32_t z = S32(h, 0x44) >> shift;
				InsertPrim(ot + (uint32_t)z * 4, pkt);
				const uint32_t v0 = verts + (U16(prim, 0) & 0xFFF) * 8, v1 = verts + (U16(prim, 2) & 0xFFF) * 8;
				const uint32_t v2 = verts + (U16(prim, 4) & 0xFFF) * 8, v3 = verts + (U16(prim, 6) & 0xFFF) * 8;
				U32(h, 0x4C) = U32(v0, 0);
				U32(h, 0x50) = U32(v0, 4);
				U32(h, 0x54) = U32(v1, 0);
				U32(h, 0x58) = U32(v1, 4);
				U32(h, 0x5C) = U32(v2, 0);
				U32(h, 0x60) = U32(v2, 4);
				U32(h, 0x64) = U32(v3, 0);
				U32(h, 0x68) = U32(v3, 4);
				const int32_t scroll = S32(h, 0x30);
				const int32_t q0 = ScrollV(S16(h, 0x4E), scroll);
				S32(h, 0x70) = q0;
				const int32_t q1 = ScrollV(S16(h, 0x56), scroll);
				S32(h, 0x74) = q1;
				const int32_t q2 = ScrollV(S16(h, 0x5E), scroll);
				S32(h, 0x78) = q2;
				const int32_t q3 = ScrollV(S16(h, 0x66), scroll);
				S32(h, 0x7C) = q3;
				const uint16_t p0 = (uint16_t)q0 & 0xFF00, p1 = (uint16_t)q1 & 0xFF00, p2 = (uint16_t)q2 & 0xFF00, p3 = (uint16_t)q3 & 0xFF00;
				U16(h, 0x60) = p2;
				U16(h, 0x50) = p0;
				U16(h, 0x58) = p1;
				U16(h, 0x68) = p3;
				if (p0 != p1 || p0 != p2 || p0 != p3)
				{
					U16(h, 0x52) = RebaseUV(h, 0x52, q0, base);
					U16(h, 0x5A) = RebaseUV(h, 0x5A, q1, base);
					U16(h, 0x62) = RebaseUV(h, 0x62, q2, base);
					U16(h, 0x6A) = RebaseUV(h, 0x6A, q3, base);
				}
				if (z > 0) z--;
				pkt += 0x28;
				U32(pkt, 8) = U32(h, 0x6C) | 0x2E000000;
				U16(pkt, 0x10) = U16(h, 0x52);
				U16(pkt, 0x18) = U16(h, 0x5A);
				U16(pkt, 0x20) = U16(h, 0x62);
				U16(pkt, 0x28) = U16(h, 0x6A);
				U32(pkt, 0) = 0xB000000;
				U32(pkt, 0xC) = U32(xy0, 0);
				U32(pkt, 0x14) = U32(xy1, 0);
				U16(pkt, 0x12) = 0x3F54;
				U32(pkt, 0x1C) = U32(xy2, 0);
				U16(pkt, 0x1A) = 0xBA;
				U32(pkt, 0x24) = U32(xy3, 0);
				U32(pkt, 4) = U32(h, 0x88);
				U32(pkt, 0x2C) = U32(h, 0x8C);
				InsertPrim(ot + (uint32_t)z * 4, pkt);
				pkt += 0x30;
			}
		}
		return pkt;
	}

	// MAG_179_sub_5CE3B0: one battle model (the objects of the visibility mask h +0x20): the
	// vertices of every bone group through the bone's matrix into h +4 (4th word = u: the x low
	// byte, v: the scrolled y), then the polygons through the camera
	static uint32_t RenderModel(uint32_t model, uint32_t h, uint32_t ot, uint32_t shift, uint32_t pkt)
	{
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(h, 0x20) & (1u << (i & 31)))) continue; // shl by cl (count mod 32)
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(h, 4);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const uint32_t bone = bones + (uint32_t)((int32_t)S16(obj, 0) * 0x30) + 0x10;
				obj += 2;
				GteSetRotMatrixCtrl((const Mat4x3 *)bone);
				GteSetTransVectorCtrl((const Mat4x3 *)bone);
				int32_t n = S16(obj, 0);
				obj += 2;
				for (; n > 0; n--)
				{
					GteLoadV0Words(obj);
					obj += 6;
					GteMVMVA_RotV0Tr2();
					GteStoreIRVertex(out);
					const int32_t q = ScrollV(S16(out, 2), S32(h, 0x30));
					U16(out, 6) = (uint16_t)(AT<uint8_t>(out, 0) | ((uint32_t)(uint8_t)q << 8));
					out += 8;
				}
			}
			const uint32_t a = (obj + 3) & 0xFFFFFFFC;
			U16(h, 8) = U16(a, 0);
			U16(h, 0xA) = U16(a, 2);
			U16(h, 0xC) = U16(a, 4);
			U16(h, 0xE) = U16(a, 6);
			U32(h, 0) = a + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			pkt = RenderObjectPrims(h, ot, shift, pkt);
		}
		return pkt;
	}

	// MAG_179_sub_5CE230: the caster's shadow and battle model(s) with the overlay (texture window
	// commands from h +34..+3B)
	static void DrawCasterModel(uint32_t h, uint8_t *entity)
	{
		const uint32_t e = (uint32_t)entity;
		const uint32_t ox = AT<uint8_t>(h, 0x34), oy = AT<uint8_t>(h, 0x36);
		const uint32_t mw = U16(h, 0x38), mh = U16(h, 0x3A);
		uint32_t win = ((oy & 0xF8) | 0xFFFE2000) << 5;
		win = (win | (ox & 0xF8)) << 5;
		win = (win | (~(mh - 1) & 0xF8)) << 2;
		win |= (uint32_t)((int32_t)~(mw - 1) >> 3) & 0x1F;
		U32(h, 0x88) = win;
		U32(h, 0x8C) = 0xE2000000;
		const uint32_t anim = e + 0x60;
		if (!(entity[0] & 0x20)) FrameCursor() = DrawShadow(entity, RenderOT(0x4064), 0x10, FrameCursor());
		const uint32_t world = e + 0x40;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)world, (Mat4x3 *)(h + 0x90));
		ComputeBonesWorldMatrices((void *)anim, (void *)world);
		U32(h, 0x1C) = U32(e, 0x28);
		U32(h, 0x20) = U32(e, 0x7C);
		AT<uint8_t>(h, 0x2A) = entity[7];
		AT<uint8_t>(h, 0x29) = entity[7];
		AT<uint8_t>(h, 0x28) = entity[7];
		U32(h, 4) = BoneWorkspace();
		U32(h, 0x10) = var<uint32_t>(0x1D969A8);
		U16(h, 0x14) = 0;
		U16(h, 0x16) = 0;
		U16(h, 0x18) = 0x140;
		U16(h, 0x1A) = 0xD8;
		FrameCursor() = RenderModel(U32(anim, 4), h, RenderOT(0x44), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t second = U32(e, 0x78);
		if (second && !(entity[1] & 2))
		{
			U32(h, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)second, (void *)(h + 0x90));
			FrameCursor() = RenderGeometry(U32(second, 4), h, RenderOT(0x44), 4, FrameCursor());
			BuildBoneMatricesFromPose((void *)second);
		}
	}

	// the glow's depth-cue IR0 at a counter (fade in below 4, fade out from 0x16)
	static int32_t GlowLevel(int16_t counter)
	{
		if (counter < 4) return shl32(4 - counter, 10);
		if (counter >= 0x16) return shl32(counter - 0x16, 9);
		return 0;
	}

	// the overlay colour into h +6C: colour table entry counter % 8 depth-cued (far colour 0) by IR0
	static void GlowColour(uint32_t h, int16_t counter, int32_t level)
	{
		GteSetFarColor(0, 0, 0);
		GteLoadRGBC(GLOW_COLOURS + (uint32_t)((int32_t)counter % 8) * 4);
		GteSetIR0(level);
		GteDPCS();
		GteStoreRGB2(h + 0x6C);
	}

	// the rest of the glow's block: texture window (0, 0x80, 0x40 x 0x80), v scroll counter * 72
	static void GlowBlock(uint32_t h, int32_t scroll)
	{
		U16(h, 0x34) = 0;
		U16(h, 0x36) = 0x80;
		U16(h, 0x3A) = 0x80;
		U16(h, 0x38) = 0x40;
		U32(h, 0x2C) = 0;
		S32(h, 0x30) = scroll;
	}
	static int32_t GlowScroll(int16_t counter) { return (int32_t)counter * 72; }

	// ------------------------------------------------------------------
	// Particle draw (one entry of the pool, with the task's draw header)
	// ------------------------------------------------------------------
	static void DrawFuseParticle(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		*(int32_t *)(h + 0xC) = p->mirror[0];
		*(int32_t *)(h + 0x10) = p->mirror[1];
		TransformCameraByShadowRotation(p->pos, (uint16_t)p->scale, -((int32_t)p->scale >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
	}
	static uint8_t *SparkHeader()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)(h + 0) = SEQ_Spark;
		*(int16_t *)(h + 0x24) = 2;
		*(int32_t *)(h + 0x14) = 0x1000;
		return h;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag179_bomb_glow_held.h"
#endif

namespace ff8fx
{
namespace bombglow179
{
	// ------------------------------------------------------------------
	// Caster glow (0x5CE0F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CasterGlowTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		if (t->counter < 0x1E)
		{
			const int32_t level = GlowLevel(t->counter);
			// 30 fps layer: see mag179_bomb_glow_held.inc
			FX_HELD(held_note_glow(t);)
			const uint32_t h = (uint32_t)FieldAlloc(0xB0);
			GlowColour(h, t->counter, level);
			GlowBlock(h, GlowScroll(t->counter));
			uint8_t *entity = Caster();
			entity[0] |= 4;
			DrawCasterModel(h, entity);
			FieldFree(0xB0);
		}
		if (DrawOnly()) return 0;
		t->counter++;
		if (t->counter <= 0x1E) return 0;
		uint16_t &flags = *(uint16_t *)Caster();
		flags = (uint16_t)((flags & 0xFFFB) | t->hidden);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Fuse sparks (0x5CEB10): particles of type 1
	// ------------------------------------------------------------------
	static uint32_t __cdecl FuseSparksTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag179_bomb_glow_held.inc
		FX_HELD(held_note_pool(t);)
		uint8_t *h = SparkHeader();
		int alive = 0;
		for (Particle *p = Pool(); (uint32_t)p < POOL_END; p++)
		{
			if (!(p->type & 1)) continue;
			DrawFuseParticle(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0) p->type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 0x16)
		{
			// number of effect bones of the caster's model
			const int32_t bones = **(const uint8_t *const *)*(const uint32_t *)(Caster() + 0x64);
			for (int k = 0; k < 3; k++)
			{
				const int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Pool()[i];
				p->type = 1;
				p->frame = 0;
				const int32_t r = CrtRand() % 0x6A0;
				p->scale = (int16_t)(r + (int32_t)(uint16_t)(t->counter + 0x40) * 8);
				const int32_t flag = CrtRand() % 0x1000;
				const int32_t bone = CrtRand() % bones;
				GetEffectSpawnPosition(Caster(), bone, flag, p->pos);
				p->pos[0] = (int16_t)(p->pos[0] + (CrtRand() % 0x1B8 - 0xDC));
				p->pos[1] = (int16_t)(p->pos[1] + (CrtRand() % 0x1B8 - 0xDC));
				p->pos[2] = (int16_t)(p->pos[2] + (CrtRand() % 0x1B8 - 0xDC));
				p->mirror[0] = (p->pos[0] & 1) ? 0x1000 : -0x1000;
				p->mirror[1] = (p->pos[1] & 1) ? 0x1000 : -0x1000;
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5CDFF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)n;
		if (t->counter == 1)
		{
			EffectNode *g = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_CasterGlow);
			g->counter = 0;
			g->hidden = (uint16_t)(Caster()[0] & 4);
		}
		if (t->counter == 1)
		{
			EffectNode *f = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_FuseSparks);
			f->counter = 0;
		}
		if (t->counter == 0x23) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == 1)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			BdPlaySE3D(SOUND_Glow, 0, pos);
		}
		t->counter++;
		return t->counter > 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x5CED70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag179_bomb_glow_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = ARENA1;
		else PacketCursor() = ARENA0;
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag179_bomb_glow()
	{
		register_port(bombglow179::ORIG_RootTask, (void *)bombglow179::RootTask, "B179 RootTask", 179);
		register_port(bombglow179::ORIG_Director, (void *)bombglow179::DirectorTask, "B179 Director", 179);
		register_port(bombglow179::ORIG_CasterGlow, (void *)bombglow179::CasterGlowTask, "B179 CasterGlow", 179);
		register_port(bombglow179::ORIG_FuseSparks, (void *)bombglow179::FuseSparksTask, "B179 FuseSparks", 179);
		// 30 fps layer: see mag179_bomb_glow_held.inc
		FX_HELD(register_mag179_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag179_bomb_glow_held.inc"
#endif
