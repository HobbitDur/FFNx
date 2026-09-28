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

// Effect 169: Gastric Juice (enemy attack 112 of kernel.bin, used by the Grat; MAG_169_*).
//
// Structure (setup MAG_169_GASTRIC_JUICE_Init 0x5DEE60, file loader MAG_169_GASTRIC_JUICE_FL 0x5DEE40
// = the texture file named at 0xD6BC64 (mag168.tim, shared with Sleeping Gas); the setup keeps the
// cast context, the first target's and the attacker's slots, queues the root (one 0x10-byte node,
// pool 0x23B1378) and the director in the effect queue 0x23B1CF8 (0x64 nodes of 0x24 bytes, pool
// 0x23B1D08), frees the 100 droplet entries, plays camera animation 0xD6A3F8 and queues the TIM):
//   Root (0x5E16E0) - packet arena = magic buffer / + 0x8000 on its counter's parity, runs the
//     effect queue; ends when it is empty.
//   Director (0x5DEF20), nothing in the draw-only mode, 51 ticks: 1 sound, 6 five juice streams
//     (random start delay 0..3) and the splash, 0x1E the tinted target, 0x23 damage.
//   Stream (0x5DFD40), draws nothing: on its first tick the stream's Bezier curve (0x110 bytes per
//     stream at 0x23B2B20: the caster's effect anchor 0xF0, the target's position moved at random
//     on the ground, 4 control points, 28 samples), then its delay; at 1 the blob, 0x15 the
//     puddle, 0x16 the landing mesh and the drip emitter; 40 ticks.
//   Blob (0x5E00B0) - flipbook 0xD6A544 (frame = tick & 7) on the curve sample it has reached,
//     speed 4 slowing to 1 before sample 0xE; black on its first tick; ends past sample 26.
//   Landing mesh (0x5E01A0), 32 ticks: prim model 0xD6A8C4 whose 73 vertices blend towards those
//     of the second shape (0xD6B2EC) by sin(tick * 32) into the stream's vertex buffer (0x248 bytes
//     per stream at 0x23B3078), y-rotated at random; ticks 0..7 plain, 8..15 fading, 16..31 through
//     the module's prim renderer (0x5E0450) with the texture page moved 0x20 (fade in / out).
//   Puddle (0x5E1200), 16 ticks: prim model 0xD6B534 growing, fading from tick 8.
//   Drips (0x5E1330 / 0x5E0FE0 share the 100 entries of 0x18 bytes at 0x23B1398, owner bit mask):
//     drip emitter (0x5E0FE0, mask = 1 << stream) - ticks 0..11 throw droplets (count table
//     0xD6BC24) around the landing point, flipbook 0xD6A718 falling and decelerating (1/16);
//     splash (0x5E1330, mask 0x100) - ticks 1..2 twelve droplets thrown around stream 0's second
//     curve sample, flipbook 0xD6A620 through the light matrix of the curve's direction,
//     decelerating (1/4); both end when their droplets are gone.
//   Tinted target (0x5DF030 -> 0x5DF170 -> 0x5DF280 -> 0x5DF540), 23 ticks: the first target is
//     hidden (entity flags |= 4) and its model redrawn: flat textured faces plus a Gouraud overlay
//     of every face with a corner within 1200 of the target's position (colour 0x98C028 depth-cued
//     to black by the fade, x (1 - distance / 1200) per corner); fade in over 8 ticks, out from 13.
// Every task draws first; in the draw-only mode (battle_to_update_flags & 0x201) it returns after
// drawing (the director and the streams do nothing).
// Module globals: 0x23B1370..0x23B3C10 (target slot, queues and pools, droplets, context, streams,
// attacker slot, mesh vertex buffers, texture file, magic buffer base, packet cursor 0x23B3BE8,
// splash matrix 0x23B3BF0 whose translation the splash rewrites per droplet).

#include "mag_common.h"

namespace ff8fx
{
namespace gastric169
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the renderers follow the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }

	// --- module globals ---
	inline int32_t &TargetSlot() { return var<int32_t>(0x23B1370); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x23B1CF8; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x23B2B18); }
	inline int32_t &AttackerSlot() { return var<int32_t>(0x23B3070); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x23B3BE4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23B3BE8); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	static const uint32_t DROPS = 0x23B1398, DROPS_END = 0x23B1CF8;   // 100 x 0x18 bytes
	static const uint32_t STREAMS = 0x23B2B20;                        // 5 x 0x110 bytes
	static const uint32_t MESH_VERTS = 0x23B3078;                     // 5 x 73 x 8 bytes
	static const uint32_t SPLASH_MATRIX = 0x23B3BF0;                  // Mat4x3
	static const uint32_t MDATA_LO = 0x23B1370, MDATA_HI = 0x23B3C10;

	static const uint32_t ORIG_Root = 0x5E16E0;
	static const uint32_t ORIG_Director = 0x5DEF20;
	static const uint32_t ORIG_Tint = 0x5DF030;
	static const uint32_t ORIG_Stream = 0x5DFD40;
	static const uint32_t ORIG_Blob = 0x5E00B0;
	static const uint32_t ORIG_Mesh = 0x5E01A0;
	static const uint32_t ORIG_Drips = 0x5E0FE0;
	static const uint32_t ORIG_Puddle = 0x5E1200;
	static const uint32_t ORIG_Splash = 0x5E1330;
	static const uint32_t SEQ_Blob = 0xD6A544, SEQ_Splash = 0xD6A620, SEQ_Drip = 0xD6A718;
	static const uint32_t MODEL_Mesh = 0xD6A8C4, MODEL_Puddle = 0xD6B534;
	static const uint32_t MESH_A = 0xD6A8CC, MESH_B = 0xD6B2EC;       // 73 vertices of 8 bytes each
	static const int32_t *const DRIP_COUNTS = (const int32_t *)0xD6BC24; // droplets per tick 0..11
	static const void *const SOUND_Juice = (const void *)0xD6BC1C;

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

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 0x64 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode { TaskNode hdr; uint16_t counter; uint16_t pad; };
	struct Node // every effect-queue task
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t idx;       // +0x0E stream index (director: next stream; drip emitter: owner mask)
		int16_t pos[4];    // +0x10 target / landing position
		int16_t a18;       // +0x18 stream: set-up done; blob: curve sample; mesh / puddle: y angle
		int16_t a1a;       // +0x1A stream: start delay; blob: speed; mesh: unused random
		int16_t a1c;       // +0x1C blob / mesh / puddle: scale
		int16_t a1e;       // +0x1E puddle: scale step
		uint32_t pad20;
	};
	struct Stream // 0x110 bytes per stream
	{
		int16_t src[4];      // +0x00 caster's effect anchor 0xF0
		int16_t dst[4];      // +0x08 target position, moved at random on the ground
		int16_t cp[4][4];    // +0x10 Bezier control points
		int16_t curve[28][4]; // +0x30 curve samples
	};
	struct Drop // droplet entry (0x18 bytes)
	{
		uint32_t flags;    // owner mask (1 << stream: drip emitter, 0x100: splash), 0 = free
		int16_t frame;     // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t pad0;
		int16_t vel[3];    // +0x10
		int16_t pad1;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "root node is 0x10 bytes");
	static_assert(sizeof(Node) == 0x24, "Gastric Juice nodes are 0x24 bytes");
	static_assert(sizeof(Stream) == 0x110 && sizeof(Drop) == 0x18, "stream / droplet entries");

	inline Stream *StreamOf(int32_t i) { return (Stream *)(STREAMS + (uint32_t)(i * 0x110)); }
	inline Drop *Drops() { return (Drop *)DROPS; }
}
}

#ifdef FF8_FX_HELD
#include "mag169_gastric_juice_held.h"
#endif

namespace ff8fx
{
namespace gastric169
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
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			U32(pk, 0xC) = U32(rec, 8);
			U32(pk, 0x14) = w & 0x1FFFFFF;
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
			U32(pk, 0x14) = w & 0x1FFFFFF;
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
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
		// 30 fps layer: see mag169_gastric_juice_held.inc
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

	// fade of the tinted target: in over ticks 0..8, none 8..13, out from 13
	static int32_t TintFade(int32_t c)
	{
		if (c < 8) return shl32(8 - c, 9);
		if (c >= 0xD) return shl32(c - 0xD, 9);
		return 0;
	}

	// the target hidden and redrawn tinted around its position
	static void TintDraw(const Node *n, int32_t fade)
	{
		uint8_t *entity = Entity(TargetSlot());
		entity[0] |= 4;
		const uint32_t h = P(FieldAlloc(0xDC));
		S32(h, 0xA8) = n->pos[0];
		S32(h, 0xAC) = 0;
		S32(h, 0xB0) = n->pos[2];
		S32(h, 0xB4) = 0x4B0;
		U8(h, 0xB8) = 0x98;
		U8(h, 0xB9) = 0xC0;
		U8(h, 0xBA) = 0x28;
		GteSetFarColorB(0, 0, 0);
		GteLoadRGBC(h + 0xB8);
		GteSetIR0(fade);
		GteDPCS();
		GteStoreRGB2(h + 0xB8);
		ModelRender(h, P(entity));
		FieldFree(0xDC);
	}

	// ------------------------------------------------------------------
	// Tinted target (0x5DF030)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TintTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (n->counter < 0x16)
		{
			// 30 fps layer: see mag169_gastric_juice_held.inc
			FX_HELD(held_note_draw(ORIG_Tint, n);)
			TintDraw(n, TintFade(n->counter));
		}
		if (DrawOnly()) return 0;
		n->counter++;
		if (n->counter <= 0x16) return 0;
		*(uint16_t *)Entity(TargetSlot()) &= 0xFFFB;
		return TASK_END;
	}

	// ==================================================================
	// Module prim renderer (0x5E0450; header 0x5C bytes on the scratch: +0x00 model, +0x04 vertices,
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

	// 0x5E0540: flat textured triangles (0x14-byte records -> 0x24-byte packets)
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

	// 0x5E07A0: flat textured quads (0x18-byte records -> 0x2C-byte packets)
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

	// 0x5E0A70: Gouraud textured triangles (0x1C-byte records -> 0x2C-byte packets)
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

	// 0x5E0CD0: Gouraud textured quads (0x24-byte records -> 0x38-byte packets)
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

	// 0x5E0450: the model's sections (two skipped words, FT3, FT4, two skipped words, GT3, GT4; an
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

	// ------------------------------------------------------------------
	// Landing mesh (0x5E01A0)
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
		m.t[2] = n->pos[2];
		const int32_t sc[3] = { n->a1c, n->a1c, n->a1c };
		Scale3DMatrix(&m, sc);
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
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(held_note_draw(ORIG_Mesh, n);)
		MeshDraw(n, MESH_VERTS + (uint32_t)(n->idx * 0x248), MeshBlend(n->counter), MeshFade(n->counter));
		if (DrawOnly()) return 0;
		n->counter++;
		return n->counter >= 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Puddle (0x5E1200)
	// ------------------------------------------------------------------
	static int32_t PuddleFade(int32_t c) { return c < 8 ? 0x800 : shl32(c, 8); }

	static void PuddleDraw(const Node *n, int32_t fade)
	{
		int16_t ang[3] = { 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->pos[0];
		m.t[1] = n->pos[1];
		m.t[2] = n->pos[2];
		const int32_t sc[3] = { n->a1c, n->a1c, n->a1c };
		Scale3DMatrix(&m, sc);
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
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(held_note_draw(ORIG_Puddle, n);)
		PuddleDraw(n, PuddleFade(n->counter));
		if (DrawOnly()) return 0;
		n->a1c = (int16_t)(n->a1c + n->a1e);
		n->counter++;
		return n->counter >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Blob (0x5E00B0)
	// ------------------------------------------------------------------
	static void BlobDraw(const Node *n, const int16_t *pos, uint32_t seq)
	{
		TransformCameraByShadowRotation(pos, n->a1c, -(n->a1c >> 3));
		const uint32_t h = P(FieldAlloc(0xB4));
		const int16_t c = n->counter;
		U32(h, 0) = seq;
		U16(h, 4) = (uint16_t)(c & 7);
		U16(h, 0x24) = 0;
		if (c < 1)
		{
			const uint32_t v = (uint32_t)shl32(c, 7);
			U16(h, 0x24) = 4;
			U32(h, 0x1C) = (((v << 8) | v) << 8) | v;
		}
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl BlobTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(held_note_draw(ORIG_Blob, n);)
		BlobDraw(n, StreamOf(n->idx)->curve[0] + n->a18 * 4, SEQ_Blob);
		if (DrawOnly()) return 0;
		const int16_t v = n->a1a;
		n->a18 = (int16_t)(n->a18 + v);
		if (v > 1 && n->a18 < 0xE) n->a1a = (int16_t)(v - 1);
		n->counter++;
		return n->a18 >= 0x1B ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Drip emitter (0x5E0FE0)
	// ------------------------------------------------------------------
	static void DripDraw(uint32_t h, const Drop *e)
	{
		U16(h, 4) = (uint16_t)e->frame;
		TransformCameraByShadowRotation(e->pos, e->scale, -(e->scale >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t __cdecl DripTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag169_gastric_juice_held.inc
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
		if (c >= 0 && c <= 0xB && DRIP_COUNTS[c] > 0)
		{
			for (int32_t k = 0; k < DRIP_COUNTS[n->counter]; k++)
			{
				Drop *e = Drops();
				while (P(e) < DROPS_END && e->flags) e++;
				if (P(e) >= DROPS_END) break;
				e->flags = (uint32_t)(int32_t)n->idx;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x900 + 0x600);
				memcpy(e->pos, n->pos, 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(CrtRand() % 700 - 350));
				e->pos[1] = (int16_t)(e->pos[1] + (int16_t)(-100 - CrtRand() % 300));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(CrtRand() % 700 - 350));
				e->vel[1] = (int16_t)(-40 - CrtRand() % 70);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Splash (0x5E1330)
	// ------------------------------------------------------------------
	// the light matrix: from the camera x (rotation taking the down axis onto the direction of
	// curve samples 0 -> 2, at sample 1); the splash matrix's rotation goes to the GTE
	static void SplashSetup(const Stream *s)
	{
		int32_t down[3] = { 0, -0x1000, 0 };
		int32_t dir[3];
		dir[0] = s->curve[2][0] - s->curve[0][0];
		dir[1] = s->curve[2][1] - s->curve[0][1];
		dir[2] = s->curve[2][2] - s->curve[0][2];
		NormalizeVector(dir, dir);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(down, dir, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = s->curve[1][0];
		m.t[1] = s->curve[1][1];
		m.t[2] = s->curve[1][2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrixCtrl((const Mat4x3 *)SPLASH_MATRIX);
		GteSetLightMatrix(&m);
		GteSetBKFromMatrix(&m);
	}

	// one droplet: its camera-space position (light matrix + BK) pulled towards the camera by
	// scale / 16 into the splash matrix's translation, the scale on the rotation diagonal
	static void SplashDraw(uint32_t h, const Drop *e)
	{
		Mat4x3 &g = *(Mat4x3 *)SPLASH_MATRIX;
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotDiagonal16(e->scale);
		U16(h, 4) = (uint16_t)e->frame;
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

	static uint32_t __cdecl SplashTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(held_note_draw(ORIG_Splash, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int32_t alive = 0;
		U32(h, 0) = SEQ_Splash;
		U16(h, 0x24) = 0;
		SplashSetup(StreamOf(n->idx));
		for (Drop *e = Drops(); P(e) < DROPS_END; e++)
		{
			if (!(e->flags & 0x100)) continue;
			SplashDraw(h, e);
			if (DrawOnly()) continue;
			e->frame++;
			if (S16(h, 0x28) < 0)
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
		if (n->counter >= 1 && n->counter <= 2)
		{
			for (int32_t k = 0; k < 0xC; k++)
			{
				Drop *e = Drops();
				while (P(e) < DROPS_END && e->flags) e++;
				if (P(e) >= DROPS_END) break;
				e->flags = 0x100;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x180 + 0x200);
				e->pos[0] = (int16_t)(CrtRand() % 300 - 150);
				e->pos[1] = 0;
				e->pos[2] = (int16_t)(CrtRand() % 300 - 150);
				const int32_t a = CrtRand() % 4096;
				const int32_t r = CrtRand() % 100 + 0x50;
				e->vel[0] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				e->vel[1] = (int16_t)(-100 - CrtRand() % 120);
				e->vel[2] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Stream (0x5DFD40): the curve, then the blob, puddle, mesh and drip emitter
	// ------------------------------------------------------------------
	static void StreamSetup(Stream *s)
	{
		GetEffectSpawnPosition(Entity(AttackerSlot()), 0xF0, 0, s->src);
		GetDefaultEffectPosition(Entity(TargetSlot()), s->dst);
		s->dst[0] = (int16_t)(s->dst[0] + (int16_t)(CrtRand() % 0x578 - 0x2BC));
		s->dst[1] = 0;
		s->dst[2] = (int16_t)(s->dst[2] + (int16_t)(CrtRand() % 0x578 - 0x2BC));
		memcpy(s->cp[0], s->src, 8);
		memcpy(s->cp[3], s->dst, 8);
		const int16_t x0 = s->src[0];
		const int32_t dx = (s->dst[0] - x0) / 3;
		const int32_t dz = (s->dst[2] - s->src[2]) / 3;
		s->cp[1][0] = (int16_t)(x0 + dx);
		s->cp[1][1] = (int16_t)(s->src[1] - (int16_t)(CrtRand() % 0x1F4) - 0x898);
		s->cp[1][2] = (int16_t)(s->src[2] + dz);
		s->cp[2][0] = (int16_t)(dx * 2 + s->src[0]);
		s->cp[2][1] = (int16_t)(s->cp[1][1] - (int16_t)(CrtRand() % 0x258) - 0xC8);
		s->cp[2][2] = (int16_t)(dz * 2 + s->src[2]);
		void *work = FieldAlloc(0x190);
		BezierInit(4, s->cp, work);
		for (int32_t t = 0, k = 0; t < 0x1C000; t += 0x1000, k++)
			BezierPoint(4, work, s->curve[k], t / 27);
		FieldFree(0x190);
	}

	static uint32_t __cdecl StreamTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		Stream *s = StreamOf(n->idx);
		if (DrawOnly()) return 0;
		if (n->a18 == 0)
		{
			StreamSetup(s);
			n->a18 = 1;
		}
		if (n->a1a > 0)
		{
			n->a1a--;
			return 0;
		}
		if (n->counter == 1)
		{
			Node *b = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Blob);
			b->counter = 0;
			b->idx = n->idx;
			b->a18 = 0;
			b->a1a = 4;
			b->a1c = (int16_t)(CrtRand() % 0x180 + 0x140);
		}
		if (n->counter == 0x16)
		{
			Node *m = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Mesh);
			memcpy(m->pos, s->dst, 8);
			m->counter = 0;
			m->idx = n->idx;
			m->a18 = (int16_t)(CrtRand() % 4096);
			m->a1a = (int16_t)(CrtRand() % 6 + 0x10);
			m->a1c = (int16_t)(CrtRand() % 0x600 + 0x400);
			if (n->counter == 0x16)
			{
				Node *d = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Drips);
				d->counter = 0;
				d->idx = (int16_t)(1u << ((uint8_t)n->idx & 31));
				memcpy(d->pos, s->dst, 8);
			}
		}
		if (n->counter == 0x15)
		{
			Node *p = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Puddle);
			memcpy(p->pos, s->dst, 8);
			p->counter = 0;
			p->a18 = (int16_t)(CrtRand() % 4096);
			const int16_t g = (int16_t)((CrtRand() % 2048 + 0xD00) / 10);
			p->a1e = g;
			p->a1c = g;
		}
		n->counter++;
		return n->counter >= 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5DEF20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (DrawOnly()) return 0;
		if (n->counter == 6)
		{
			for (int k = 0; k < 5; k++)
			{
				Node *s = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Stream);
				const int16_t i = n->idx;
				s->counter = 0;
				s->idx = i;
				s->a18 = 0;
				const int32_t d = CrtRand() % 4;
				n->idx++;
				s->a1a = (int16_t)d;
			}
			Node *sp = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Splash);
			sp->counter = 0;
			sp->idx = 0;
		}
		if (n->counter == 0x1E)
		{
			Node *t = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Tint);
			t->counter = 0;
			GetDefaultEffectPosition(Entity(TargetSlot()), t->pos);
		}
		if (n->counter == 0x23) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (n->counter == 1) BdPlaySE(SOUND_Juice, 0, 0x80);
		n->counter++;
		return n->counter > 0x32 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x5E16E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0x8000;
		else PacketCursor() = TexBase();
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag169_gastric_juice()
	{
		register_port(gastric169::ORIG_Root, (void *)gastric169::RootTask, "G169 RootTask", 169);
		register_port(gastric169::ORIG_Director, (void *)gastric169::DirectorTask, "G169 DirectorTask", 169);
		register_port(gastric169::ORIG_Tint, (void *)gastric169::TintTask, "G169 TintTask", 169);
		register_port(gastric169::ORIG_Stream, (void *)gastric169::StreamTask, "G169 StreamTask", 169);
		register_port(gastric169::ORIG_Blob, (void *)gastric169::BlobTask, "G169 BlobTask", 169);
		register_port(gastric169::ORIG_Mesh, (void *)gastric169::MeshTask, "G169 MeshTask", 169);
		register_port(gastric169::ORIG_Drips, (void *)gastric169::DripTask, "G169 DripTask", 169);
		register_port(gastric169::ORIG_Puddle, (void *)gastric169::PuddleTask, "G169 PuddleTask", 169);
		register_port(gastric169::ORIG_Splash, (void *)gastric169::SplashTask, "G169 SplashTask", 169);
		// 30 fps layer: see mag169_gastric_juice_held.inc
		FX_HELD(register_mag169_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag169_gastric_juice_held.inc"
#endif
