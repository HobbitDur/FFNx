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

// Effect 168: Sleeping Gas (enemy attack 113 of kernel.bin, used by the Grat; MAG_168_*).
//
// Structure (setup MAG_168_SLEEPING_GAS_Init 0x5E1750, file loader MAG_168_SLEEPING_GAS_FL 0x5E1730 =
// the texture file named at 0xD6F798; the setup keeps the cast context, the first target's slot
// (0x23B3C10, never read) and the caster's slot, queues the root (one 0x10-byte node, pool 0x23B5C58,
// queue 0x23B5C68) and the director in the effect queue 0x23B6A88 (0x64 nodes of 0x24 bytes, pool
// 0x23B6A98), frees both sparkle pools, plays camera animation 0xD6BC70 and uploads the texture):
//   Root (0x5E29A0) - alternates the packet arena (magic buffer / + 0x14000) on its counter's parity,
//     runs the effect queue; ends when the queue is empty.
//   Director (0x5E1820), nothing in the draw-only mode, 71 ticks: at 0 the caster's effect anchor
//     (bone 0xF0, 0xA0 higher) and the cloud centre 13/16 of the way to the targets' centre, and the
//     vertex pair table (0x5E1CA0); ticks 6..0xB two gas clouds per tick around the centre (random
//     offset, size, wave parameters); 0x17 one target sparkle task per target; 4 the caster sparkle
//     task; 0x3C damage; screen flash ramps up over ticks 4..0xC and down from 0x3E; sound at 1.
//   Gas cloud (0x5E1D20), 60 ticks, updates its shape THEN draws: the wave (0x5E2220: 61 offsets
//     x = sin(a) * amplitude a, y = sin(b) * amplitude b along the cloud, angle a advancing 100 per
//     tick, its step decaying 1/32 along the cloud, amplitudes growing along it) is applied to the
//     quad model 0xD6EDE4 (0x5E22E0: the model's 122 vertices in pairs sorted by depth, offset pair by
//     pair; the pairs past the grown length collapse onto the last one; the length grows by 2 per
//     tick up to 61 except in the draw-only mode); drawn by the module's quad renderer (0x5E1F00 /
//     0x5E1F70) through a matrix turning the model from the caster towards the cloud position and
//     scaled (width, 1.5 x width, distance / 1600); fades in over 24 ticks and out from 0x34.
//   Target sparkles (0x5E2430), one task per target (bit mask): the flipbook 0xD6BDE8 of every
//     particle of pool A (100 x 0x18 bytes at 0x23B6128) with its bit, drifting away from the caster,
//     rising and decelerating (1/32), fading in while the task is young; one new particle per tick
//     2..0x22 around the target; ends when none is left (after tick 4).
//   Caster sparkles (0x5E2720): the same flipbook for pool B (50 x 0x18 bytes at 0x23B5C78), rising
//     one unit per tick towards the cloud centre; one new particle per tick 0..0x32 at the caster's
//     anchor; ends when none is left (after tick 4).
// Every sprite / cloud task draws first (the gas cloud after its shape update); in the draw-only mode
// (battle_to_update_flags & 0x201) it returns after drawing (the gas shape still moves: its wave
// update does not test the mode, only the length growth does).
// Module globals: 0x23B3C10..0x23BD350 (first target slot, cloud records 0x23B3C18 (12 x 0x1C), wave
// offsets 0x23B3DD8 (12 x 61 x 8), root / effect queues and pools, sparkle pools, cloud centre
// 0x23B78A8, context 0x23B78B0, caster slot 0x23B78B4, texture file 0x23B9268, caster anchor
// 0x23B9270, cloud vertex buffers 0x23B9278 (12 x 122 x 8), vertex pair table 0x23BCF78, magic buffer
// 0x23BD348, packet cursor 0x23BD34C).

#include "mag_common.h"

namespace ff8fx
{
namespace gas168
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
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x23B6A88; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x23B78B0); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x23B78B4); }
	inline int16_t *Centre() { return (int16_t *)0x23B78A8; }        // cloud centre x, y, z (+ pad)
	inline int16_t *CasterAnchor() { return (int16_t *)0x23B9270; }  // caster's bone 0xF0, 0xA0 higher
	inline uint32_t &TexBase() { return var<uint32_t>(0x23BD348); }  // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23BD34C); }
	static const uint32_t CLOUDS = 0x23B3C18;     // 12 x 0x1C cloud records
	static const uint32_t OFFSETS = 0x23B3DD8;    // [cloud][61] x (x, y) int32 wave offsets
	static const uint32_t VERTS = 0x23B9278;      // [cloud][122] x 8 bytes: cloud vertex buffers
	static const uint32_t PAIRS = 0x23BCF78;      // 122 x (model vertex, cloud vertex slot), sorted
	static const uint32_t PAIRS_END = 0x23BD348;
	static const uint32_t POOL_A = 0x23B6128, POOL_A_END = 0x23B6A88; // 100 target sparkles
	static const uint32_t POOL_B = 0x23B5C78, POOL_B_END = 0x23B6128; // 50 caster sparkles
	static const int32_t WAVE = 0x3D;             // 61 offsets per cloud, 2 vertices each
	static const uint32_t CLOUD_VERTS = 0x3D0;    // 122 vertices x 8 bytes

	static const uint32_t ORIG_Root = 0x5E29A0;
	static const uint32_t ORIG_Director = 0x5E1820;
	static const uint32_t ORIG_Gas = 0x5E1D20;
	static const uint32_t ORIG_SparkTarget = 0x5E2430;
	static const uint32_t ORIG_SparkCaster = 0x5E2720;
	static const uint32_t MODEL_Gas = 0xD6EDE4;         // quad model (+8 = its 122 vertices)
	static const uint32_t MODEL_GasVerts = 0xD6EDEC;
	static const uint32_t SEQ_Sparkle = 0xD6BDE8;       // flipbook of both sparkle pools
	static const int32_t *const AXIS_Model = (const int32_t *)0xD6F788; // model direction (unit vector)
	static const void *const SOUND_Gas = (const void *)0xD6F77C;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// ---- engine functions (original addresses)
	inline void GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t mode, void *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, void *)>(0x502170)(entity, bone, mode, out); }
	inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	// software GTE (register files 0x1CA8A10 data / 0x1CA927C control)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }     // GTE_ReadOTZ
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }          // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                 // sub_45F270
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }         // set_param_with_dword_1CA8A68

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 0x64 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode { TaskNode hdr; uint16_t counter; uint16_t pad; };
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t clouds;    // +0x0E clouds spawned (the next cloud's record)
		uint8_t pad10[0x14];
	};
	struct GasNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t cloud;     // +0x0E cloud record / wave / vertex buffer index
		int16_t pos[3];    // +0x10 cloud position (the model is turned towards it from the caster)
		int16_t pad16;     // +0x16 (copied from the centre's pad word)
		uint8_t pad18[4];
		int16_t width;     // +0x1C model scale x (y: 1.5 x)
		uint8_t pad1E[6];
	};
	struct SparkNode // target sparkles (mask = the target's bit) and caster sparkles (mask unused)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t mask;      // +0x0E
		int16_t pos[4];    // +0x10 target: its bone 0xF1 anchor, y = entity +0x3C; caster: bone 0xF0, 0xA0 higher
		uint8_t pad18[0xC];
	};
	// cloud record (12 at 0x23B3C18)
	struct Cloud
	{
		int32_t count;     // +0x00 offsets applied (grows by `step` up to 61)
		int32_t step;      // +0x04
		int16_t angle_a;   // +0x08 wave a angle (100 more per tick)
		int16_t speed_a;   // +0x0A its step along the cloud (decays 1/32 per offset, for good)
		int16_t angle_a0;  // +0x0C angle a at the start of the update
		int16_t amp_a;     // +0x0E wave a amplitude at the first offset
		int16_t amp_a_step;// +0x10 its growth per offset
		int16_t angle_b;   // +0x12 wave b angle (0x800, constant)
		int16_t amp_b_step;// +0x14 wave b amplitude growth per offset
		int16_t angle_b0;  // +0x16
		int16_t amp_b;     // +0x18 wave b amplitude at the first offset
		int16_t pad1A;
	};
	// sparkle particle (pools A and B)
	struct Spark
	{
		int32_t owner;     // +0x00 A: target bit of its task (0 = free); B: bit 0 = used
		int16_t frame;     // +0x04 flipbook time
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t fade;      // +0x0E A: fading in (B: z copy's pad word)
		int16_t vel[3];    // +0x10 A: x, y (decays 1/32), z; B: x, (unused), z (y falls by 1 per tick)
		int16_t fade_len;  // +0x16 A: fade-in ticks (task counter < fade_len)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "root node is 0x10 bytes");
	static_assert(sizeof(DirectorNode) == 0x24 && sizeof(GasNode) == 0x24 && sizeof(SparkNode) == 0x24, "Sleeping Gas nodes are 0x24 bytes");
	static_assert(sizeof(Cloud) == 0x1C && sizeof(Spark) == 0x18, "cloud record / sparkle entry");

	inline Cloud *CloudRec(int32_t i) { return (Cloud *)(CLOUDS + (uint32_t)(i * 0x1C)); }
	inline int32_t *CloudOffsets(int32_t i) { return (int32_t *)(OFFSETS + (uint32_t)(i * WAVE * 8)); }
	inline uint32_t CloudVerts(int32_t i) { return VERTS + (uint32_t)(i * WAVE * 2 * 8); }

	// x / 61 as compiled (0x4325C53F, sar 4)
	static int32_t Div61(int32_t x)
	{
		int32_t q = (int32_t)(((int64_t)x * 0x4325C53F) >> 32) >> 4;
		return q + (int32_t)((uint32_t)q >> 31);
	}
	// x / 1600 as compiled (0x51EB851F, sar 9)
	static int32_t Div1600(int32_t x)
	{
		int32_t q = (int32_t)(((int64_t)x * 0x51EB851F) >> 32) >> 9;
		return q + (int32_t)((uint32_t)q >> 31);
	}

	// ==================================================================
	// Quad renderer (module copy of the engine's textured-quad list renderer)
	// Header (0x58 bytes on the scratch): +0x00 model, +0x04 vertices, +0x08..+0x0A far colour,
	// +0x0C depth cue (IR0), +0x10 / +0x14 texture page / CLUT override, +0x18 texture offset,
	// +0x1C flags (1 / 4 semi-transparency on / off, 0x10 both faces, 0x40 depth cue, 0x100..0x800
	// overrides, 0x1000 keep the texture offset, 0x2000 keep the vertices), +0x20 list cursor,
	// +0x24 NCLIP, +0x2C OTZ, +0x30 GTE flags
	// ==================================================================

	// 0x5E1F70: the textured quads (0x18-byte records -> POLY_FT4 packets of 0x28 bytes): RTPT + RTPS,
	// GTE-flag / back-face (a zero area is always rejected) / screen-clip rejection, optional depth
	// cue, inserted at ot[otz >> shift]; returns the packet cursor
	static uint32_t QuadList(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pk = cursor;
		const uint32_t list = U32(hdr, 0x20);
		const int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vtx = U32(hdr, 4);
		U32(hdr, 0x20) = rec;
		for (int32_t n = count; n > 0; n--, rec += 0x18)
		{
			GteLoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			GteRTPT();
			const uint32_t fl = U32(hdr, 0x1C);
			const uint32_t w0 = U32(rec, 0);
			U32(pk, 0) = 0x9000000;
			U32(pk, 4) = w0;
			if (fl & 1) U32(pk, 4) = w0 | 0x2000000;
			if (fl & 4) U32(pk, 4) = U32(pk, 4) & 0xFDFFFFFF;
			const uint32_t toff = U32(hdr, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + toff;
			const uint32_t w1c = (toff << 16) + toff + U32(rec, 0x14);
			U32(pk, 0x1C) = w1c;
			U32(pk, 0x14) = U32(rec, 0x10) + toff;
			U32(pk, 0x24) = w1c >> 16;
			GteReadFLAG(hdr + 0x30);
			if (U32(hdr, 0x30) & 0x60000) continue;
			GteNCLIP();
			const uint32_t f2 = U32(hdr, 0x1C);
			if (f2 & 0x400) U16(pk, 0x16) = (uint16_t)(U16(pk, 0x16) + U16(hdr, 0x10));
			else if (f2 & 0x100) U16(pk, 0x16) = U16(hdr, 0x10);
			if (f2 & 0x800) U16(pk, 0xE) = (uint16_t)(U16(pk, 0xE) + U16(hdr, 0x14));
			else if (f2 & 0x200) U16(pk, 0xE) = U16(hdr, 0x14);
			GteReadMAC0(hdr + 0x24);
			const int32_t area = S32(hdr, 0x24);
			if (area == 0) continue;
			if (area < 0 && !(U8(hdr, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(vtx + (uint32_t)U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t clip = 0;
			if (S16(pk, 8) < 0 || S16(pk, 8) > 0xA00) clip = 1;
			if (S16(pk, 0x10) < 0 || S16(pk, 0x10) > 0xA00) clip |= 2;
			if (S16(pk, 0x18) < 0 || S16(pk, 0x18) > 0xA00) clip |= 4;
			if (S16(pk, 0xA) < 0 || S16(pk, 0xA) > 0x6C0) clip |= 0x10;
			if (S16(pk, 0x12) < 0 || S16(pk, 0x12) > 0x6C0) clip |= 0x20;
			if (S16(pk, 0x1A) < 0 || S16(pk, 0x1A) > 0x6C0) clip |= 0x40;
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			if (S16(pk, 0x20) < 0 || S16(pk, 0x20) > 0xA00) clip |= 8;
			if (S16(pk, 0x22) < 0 || S16(pk, 0x22) > 0x6C0) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZ32(hdr + 0x2C);
			if (U8(hdr, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0((int32_t)U32(hdr, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const int32_t z = S32(hdr, 0x2C) >> (shift & 31);
			InsertPrimAutoDepth(ot + (uint32_t)z * 4, (void *)pk);
			pk += 0x28;
		}
		U32(hdr, 0x20) = rec;
		return pk;
	}

	// 0x5E1F00: the model's header set-up (vertices = model + 8 unless flag 0x2000, texture offset 0
	// unless flag 0x1000, far colour) and its quad list (model + *model + 0xC)
	static uint32_t QuadModel(uint32_t hdr, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		const uint32_t fl = U32(hdr, 0x1C);
		if (!(fl & 0x2000)) U32(hdr, 4) = U32(hdr, 0) + 8;
		const uint32_t model = U32(hdr, 0);
		U32(hdr, 0x20) = U32(model, 0) + model;
		if (!(fl & 0x1000)) U32(hdr, 0x18) = 0;
		GteSetFarColor(U8(hdr, 8), U8(hdr, 9), U8(hdr, 0xA));
		U32(hdr, 0x20) = U32(hdr, 0x20) + 0xC;
		return QuadList(hdr, ot, mode, cursor);
	}

	// ==================================================================
	// Gas cloud shape
	// ==================================================================

	// 0x5E1CA0: the vertex pair table: model vertex k / cloud vertex slot k (122 pairs, two per wave
	// offset), sorted by the model vertex's z (descending; the slots follow their vertex)
	static void InitPairs()
	{
		uint32_t a = MODEL_GasVerts, b = VERTS, e = PAIRS;
		for (int32_t i = WAVE; i != 0; i--)
			for (int32_t k = 2; k != 0; k--)
			{
				U32(e, 0) = a;
				U32(e, 4) = b;
				a += 8;
				b += 8;
				e += 8;
			}
		for (uint32_t i = PAIRS, n = 0; i < PAIRS_END - 8; i += 8, n++)
		{
			if (n + 1 >= 0x7A) continue;
			for (uint32_t j = i + 8; j < PAIRS_END; j += 8)
			{
				const uint32_t vi = U32(i, 0), vj = U32(j, 0);
				if (S16(vi, 4) >= S16(vj, 4)) continue;
				U32(i, 0) = vj;
				U32(j, 0) = vi;
				const uint32_t si = U32(j, 4);
				U32(j, 4) = U32(i, 4);
				U32(i, 4) = si;
			}
		}
	}

	// 0x5E2220: the wave offsets of a cloud (61 x (x, y)); angle a advances 100 per tick, its step
	// decays along the cloud for good; angle b stays; the amplitudes grow along the cloud
	static void WaveStep(Cloud *c, int32_t *out)
	{
		c->angle_a0 = c->angle_a;
		c->angle_b0 = c->angle_b;
		int32_t amp_a = c->amp_a, amp_b = c->amp_b;
		for (int32_t i = WAVE; i != 0; i--, out += 2)
		{
			out[0] = mul32(ComputeSin(c->angle_a), amp_a) >> 12;
			c->angle_a = (int16_t)(c->angle_a + c->speed_a);
			c->speed_a = (int16_t)(c->speed_a - (int16_t)(c->speed_a >> 5));
			amp_a += c->amp_a_step;
			out[1] = mul32(ComputeSin(c->angle_b), amp_b) >> 12;
			amp_b += c->amp_b_step;
		}
		c->angle_a = (int16_t)(c->angle_a0 + 0x64);
		c->angle_b = c->angle_b0;
	}

	// 0x5E22E0: the cloud vertices: pair by pair (depth order) model vertex + wave offset (x, x / 2,
	// y as z), the pairs past `count` a copy of the last vertex written; then (not in the draw-only
	// mode) the length grows. verts = the cloud's vertex buffer (the pair table's slots are those of
	// cloud 0: slot + verts - VERTS). node: read (its first 8 bytes) only when count <= 0.
	static void WaveBuild(Cloud *c, const int32_t *offs, uint32_t verts, uint32_t node)
	{
		const int32_t count = c->count;
		const uint32_t delta = verts - VERTS;
		uint32_t e = PAIRS;
		uint32_t last = node;
		if (count > 0)
		{
			for (int32_t k = 0; k < count; k++)
			{
				const int16_t ox = (int16_t)offs[k * 2];
				const int16_t oz = (int16_t)offs[k * 2 + 1];
				const int16_t oy = (int16_t)(ox >> 1);
				for (int32_t j = 2; j != 0; j--)
				{
					last = U32(e, 4) + delta;
					const uint32_t m = U32(e, 0);
					e += 8;
					S16(last, 0) = (int16_t)(S16(m, 0) + ox);
					S16(last, 2) = (int16_t)(S16(m, 2) + oy);
					S16(last, 4) = (int16_t)(S16(m, 4) + oz);
				}
			}
		}
		const uint32_t w0 = U32(last, 0), w1 = U32(last, 4);
		if (count < WAVE)
		{
			for (int32_t k = WAVE - count; k != 0; k--)
				for (int32_t j = 2; j != 0; j--)
				{
					const uint32_t v = U32(e, 4);
					e += 8;
					U32(v + delta, 0) = w0;
					U32(v + delta, 4) = w1;
				}
		}
		if (DrawOnly()) return;
		c->count += c->step;
		if (c->count > WAVE) c->count = WAVE;
	}

	// the cloud's matrix: the model axis turned onto the caster -> cloud direction, at the caster's
	// anchor, scaled (width, 1.5 x width, distance / 1600), through the camera; into the GTE
	static void GasMatrix(const GasNode *g)
	{
		int32_t v[3];
		v[0] = g->pos[0] - CasterAnchor()[0];
		v[1] = g->pos[1] - CasterAnchor()[1];
		v[2] = g->pos[2] - CasterAnchor()[2];
		const int32_t dist = Sqrt(Normalize(v, v));
		int32_t axis[3];
		Mat4x3 m = {};
		const int32_t angle = RotationBetweenVectors(AXIS_Model, v, axis);
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[1] = CasterAnchor()[1];
		m.t[0] = CasterAnchor()[0];
		int32_t s[3];
		s[0] = g->width;
		s[1] = g->width + (g->width >> 1);
		s[2] = Div1600(shl32(dist, 12));
		m.t[2] = CasterAnchor()[2];
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// the cloud's draw header: fade in over 24 ticks, out from 0x34 (IR0 towards black); in between
	// no depth cue (+0x0C keeps the scratch)
	static void GasHeader(uint8_t *h, const GasNode *g, uint32_t verts)
	{
		*(uint32_t *)(h + 0) = MODEL_Gas;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x2033;
		*(uint32_t *)(h + 4) = verts;
		const int16_t c = g->counter;
		if (c < 0x18)
		{
			*(int32_t *)(h + 0xC) = 0x1000 - c * 170;
			*(uint32_t *)(h + 0x1C) = 0x20F3;
		}
		else if (c >= 0x34)
		{
			*(int32_t *)(h + 0xC) = shl32(c - 0x34, 9);
			*(uint32_t *)(h + 0x1C) = 0x20F3;
		}
	}

	// flipbook draw of a target sparkle (header +0x04 time, +0x1C / +0x24 bit 2 = fade colour)
	static void SparkTargetFade(uint8_t *h, Spark *e, int16_t counter)
	{
		*(int16_t *)(h + 4) = e->frame;
		if (e->fade != 0)
		{
			if (counter < e->fade_len)
			{
				const int32_t v = 0x80 / e->fade_len * counter;
				*(uint32_t *)(h + 0x1C) = (uint32_t)(shl32(shl32(v, 8) | v, 8) | v);
				h[0x24] |= 4;
				return;
			}
			e->fade = 0;
		}
		*(uint16_t *)(h + 0x24) = 0;
	}

	static void SparkDraw(uint8_t *h, const Spark *e, int32_t shift)
	{
		TransformCameraByShadowRotation(e->pos, e->scale, -(e->scale >> shift));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}
}
}

#ifdef FF8_FX_HELD
#include "mag168_sleeping_gas_held.h"
#endif

namespace ff8fx
{
namespace gas168
{
	// ------------------------------------------------------------------
	// Gas cloud (0x5E1D20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GasTask(TaskNode *n)
	{
		GasNode *g = (GasNode *)n;
		const int32_t i = g->cloud;
		WaveStep(CloudRec(i), CloudOffsets(i));
		WaveBuild(CloudRec(g->cloud), CloudOffsets(g->cloud), CloudVerts(g->cloud), P(g));
		// 30 fps layer: see mag168_sleeping_gas_held.inc
		FX_HELD(held_note_gas(g);)
		GasMatrix(g);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		GasHeader(h, g, CloudVerts(g->cloud));
		PacketCursor() = QuadModel(P(h), RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		g->counter++;
		return g->counter >= 0x3C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Target sparkles (0x5E2430)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTargetTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag168_sleeping_gas_held.inc
		FX_HELD(held_note_spark(ORIG_SparkTarget, s);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t active = 0;
		*(uint32_t *)h = SEQ_Sparkle;
		for (uint32_t p = POOL_A; p < POOL_A_END; p += 0x18)
		{
			Spark *e = (Spark *)p;
			if (!(e->owner & (int32_t)s->mask)) continue;
			SparkTargetFade(h, e, s->counter);
			SparkDraw(h, e, 4);
			if (DrawOnly()) continue;
			e->frame++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				e->owner = 0;
				continue;
			}
			e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
			e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
			e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
			e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 5));
			active++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (s->counter >= 2 && s->counter <= 0x22)
		{
			int32_t v[3];
			v[0] = s->pos[0] - CasterAnchor()[0];
			v[1] = s->pos[1] - CasterAnchor()[1];
			v[2] = s->pos[2] - CasterAnchor()[2];
			Normalize(v, v);
			for (int32_t k = 0; k < 1; k++)
			{
				int32_t idx = 0;
				uint32_t p = POOL_A;
				while (p < POOL_A_END && ((Spark *)p)->owner != 0)
				{
					p += 0x18;
					idx++;
				}
				if (p >= POOL_A_END || idx >= 0x64) break;
				Spark *e = (Spark *)p;
				e->owner = s->mask;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0xA00 + 0x600);
				e->pos[0] = (int16_t)(CrtRand() % 1000 + s->pos[0] - 0x1F4);
				e->pos[1] = (int16_t)(s->pos[1] - CrtRand() % 300);
				e->pos[2] = (int16_t)(CrtRand() % 1000 + s->pos[2] - 0x1F4);
				e->vel[1] = (int16_t)(-0x32 - CrtRand() % 0x46);
				if (s->counter < 0x10)
				{
					e->fade = 1;
					e->fade_len = (int16_t)(0x10 - s->counter);
				}
				else e->fade = 0;
				const int32_t speed = CrtRand() % 20 + 20;
				e->vel[0] = (int16_t)(mul32(speed, v[0]) >> 12);
				e->vel[2] = (int16_t)(mul32(speed, v[2]) >> 12);
			}
		}
		s->counter++;
		if (s->counter >= 4 && active == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Caster sparkles (0x5E2720)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkCasterTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag168_sleeping_gas_held.inc
		FX_HELD(held_note_spark(ORIG_SparkCaster, s);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t active = 0;
		*(uint32_t *)h = SEQ_Sparkle;
		*(uint16_t *)(h + 0x24) = 8;
		for (uint32_t p = POOL_B; p < POOL_B_END; p += 0x18)
		{
			Spark *e = (Spark *)p;
			if (!(e->owner & 1)) continue;
			*(int16_t *)(h + 4) = e->frame;
			SparkDraw(h, e, 3);
			if (DrawOnly()) continue;
			e->frame++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				e->owner = 0;
				continue;
			}
			e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
			e->pos[1]--;
			e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
			active++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (s->counter >= 0 && s->counter <= 0x32)
		{
			GetEffectSpawnPosition(Entity((int)CasterSlot()), 0xF0, 0x1000, s->pos);
			s->pos[1] = (int16_t)(s->pos[1] - 0xA0);
			int32_t v[3];
			v[0] = Centre()[0] - s->pos[0];
			v[1] = Centre()[1] - s->pos[1];
			v[2] = Centre()[2] - s->pos[2];
			Normalize(v, v);
			for (int32_t k = 0; k < 1; k++)
			{
				int32_t idx = 0;
				uint32_t p = POOL_B;
				while (p < POOL_B_END && ((Spark *)p)->owner != 0)
				{
					p += 0x18;
					idx++;
				}
				if (p >= POOL_B_END || idx >= 0x32) break;
				Spark *e = (Spark *)p;
				e->owner = 1;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x800 + 0x600);
				U32(p, 8) = U32(P(s), 0x10);
				U32(p, 0xC) = U32(P(s), 0x14);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 100 - 0x32));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 100 - 0x32));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 100 - 0x32));
				const int32_t speed = CrtRand() % 25 + 15;
				e->vel[0] = (int16_t)(mul32(speed, v[0]) >> 12);
				e->vel[2] = (int16_t)(mul32(speed, v[2]) >> 12);
			}
		}
		s->counter++;
		if (s->counter >= 4 && active == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5E1820)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		DirectorNode *d = (DirectorNode *)n;
		if (d->counter == 0)
		{
			GetEffectSpawnPosition(Entity((int)CasterSlot()), 0xF0, 0x1000, CasterAnchor());
			CasterAnchor()[1] = (int16_t)(CasterAnchor()[1] - 0xA0);
			CalculateCenterPosition(Ctx()->target_mask, Centre());
			int32_t v[3];
			v[0] = Centre()[0] - CasterAnchor()[0];
			v[1] = Centre()[1] - CasterAnchor()[1];
			v[2] = Centre()[2] - CasterAnchor()[2];
			const int32_t len = shl32(mul32(Sqrt(Normalize(v, v)), 13), 8) >> 12;
			Centre()[1] = (int16_t)((mul32(len, v[1]) >> 12) + CasterAnchor()[1]);
			Centre()[0] = (int16_t)((mul32(len, v[0]) >> 12) + CasterAnchor()[0]);
			Centre()[2] = (int16_t)((mul32(len, v[2]) >> 12) + CasterAnchor()[2]);
			InitPairs();
		}
		if (d->counter >= 6 && d->counter <= 0xB)
		{
			for (int32_t k = 2; k != 0; k--)
			{
				GasNode *g = (GasNode *)AddTaskToQueue(EffectQueue(), ORIG_Gas);
				const int16_t cloud = d->clouds;
				const uint32_t z = U32(0x23B78AC, 0);
				U32(P(g), 0x10) = U32(0x23B78A8, 0);
				g->counter = 0;
				g->cloud = cloud;
				U32(P(g), 0x14) = z;
				g->pos[0] = (int16_t)(g->pos[0] + (int16_t)(CrtRand() % 5000 - 0x9C4));
				g->pos[1] = (int16_t)(-0xC8 - CrtRand() % 0x6A4);
				g->pos[2] = (int16_t)(g->pos[2] + (int16_t)(CrtRand() % 0x9C4 - 0x4E2));
				const int32_t w = CrtRand() % 0xD00;
				Cloud *c = CloudRec(g->cloud);
				c->step = 2;
				c->count = 2;
				g->width = (int16_t)(w + 0x500);
				c->angle_a = (int16_t)(CrtRand() % 0x1000);
				c->speed_a = (int16_t)(CrtRand() % 0xA0 + 0x96);
				c->amp_a = 0;
				c->amp_a_step = (int16_t)(CrtRand() % 0x14 + 5);
				c->angle_b = 0x800;
				c->amp_b_step = (int16_t)Div61(CrtRand() % 0x400 + 0x640);
				c->amp_b = (int16_t)(CrtRand() % 0x258 + 0x384);
				d->clouds++;
			}
		}
		if (d->counter == 0x17)
		{
			for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
			{
				const uint8_t slot = Ctx()->actions[0].targets[i * TARGET_STRIDE];
				SparkNode *s = (SparkNode *)AddTaskToQueue(EffectQueue(), ORIG_SparkTarget);
				s->counter = 0;
				s->mask = (int16_t)(1 << (i & 31));
				GetEffectSpawnPosition(Entity(slot), 0xF1, 0, s->pos);
				s->pos[1] = *(int16_t *)(Entity(slot) + 0x3C);
			}
		}
		if (d->counter == 4) ((SparkNode *)AddTaskToQueue(EffectQueue(), ORIG_SparkCaster))->counter = 0;
		if (d->counter == 0x3C) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (d->counter >= 4 && d->counter <= 0xC) SetScreenFlash((uint32_t)((d->counter - 4) * 0xC0), 0x302060);
		else if (d->counter >= 0x3E) SetScreenFlash((uint32_t)((0x46 - d->counter) * 0xC0), 0x302060);
		if (d->counter == 1) BdPlaySE(SOUND_Gas, 0, 0x80);
		d->counter++;
		if (d->counter <= 0x46) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Root (0x5E29A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag168_sleeping_gas_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0x14000;
		else PacketCursor() = TexBase();
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag168_sleeping_gas()
	{
		register_port(gas168::ORIG_Root, (void *)gas168::RootTask, "G168 RootTask", 168);
		register_port(gas168::ORIG_Director, (void *)gas168::DirectorTask, "G168 DirectorTask", 168);
		register_port(gas168::ORIG_Gas, (void *)gas168::GasTask, "G168 GasTask", 168);
		register_port(gas168::ORIG_SparkTarget, (void *)gas168::SparkTargetTask, "G168 SparkTargetTask", 168);
		register_port(gas168::ORIG_SparkCaster, (void *)gas168::SparkCasterTask, "G168 SparkCasterTask", 168);
		// 30 fps layer: see mag168_sleeping_gas_held.inc
		FX_HELD(register_mag168_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag168_sleeping_gas_held.inc"
#endif
