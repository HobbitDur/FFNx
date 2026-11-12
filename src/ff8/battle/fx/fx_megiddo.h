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

// Megiddo Flame: the two compiles of one effect module, effect 166 (MAG_166, enemy attack 310, Omega Weapon c0m114)
// and effect 312 (MAG_312, enemy attack 285, Mobile Type 8 c0m105), shared by mag166_megiddo_flame.cpp and
// mag312_megiddo_flame.cpp. The two modules are the same instructions: 312's code is 166's moved by -0x48300, its
// globals by -0x182398, its data by -0x989F0 (its camera track 0xCD8D90 by -0x98948); 312 takes the caster's bone 0xA
// (166: 0x25) and draws its glow / spin / ring prim models with OT shift 2 (166: 3). The code below is written with
// 166's addresses; a module descriptor (struct with CODE / GLOB / DATA deltas, CAMERA, BONE, RMODE, EFFECT) moves them.
//
// Structure (file loader 0x5E4E70 = the texture file named at 0xD780B8 (mag311.tim) into 0x23C3258; setup 0x5E4E90: the
// texture base 0x23C3270 (Magic_TextureOFF_ToEAX1), the cast context 0x23C3250, the first target's slot 0x23BF038, the
// caster slot 0x23C3254, the root queue 0x23BF070 (pool 0x23BF050, 2 x 0x10) with the root, the main queue 0x23C0540
// (pool 0x23C0550, 100 x 0x24) with the director, pools A / B cleared, camera track 0xD716D8, TIM upload):
//   Root (0x5E93A0) - packet cursor 0x23C3274 = texture base (+0xC000 on odd ticks), the main queue; when it is empty
//     queues the texture restore (0x5E9410) and ends.
//   Director (0x5E4F60), 0x63 ticks. EVERY tick, draw-only included: the effect point 0x23C3260 = the caster's bone
//     (0x25 / 0xA) matrix (0x5E56C0) through the caster's matrix applied to 0xD78048, plus its translation; the frame
//     0x23C04E0 = the rotation from 0xD78050 to the direction point -> target centre 0x23C3248, translated to the point.
//     Then (not in the draw-only mode), by counter:
//       0     the targets' centre 0x23C3248 (0x5020A0), 0x23C3268 = 0;  1 glow (0x5E5980), sound 0xD7803C
//       6..0x1E (counter % 6 == 2) a ring (0x5E5C30);  6 screen fade (2, 1, 0xA, 0xFF);  8 two spins (0x5E5AB0) and the
//       six bolt directors (0x5E5D50, e = 0..5);  0x2B the stage capture (0x5E5700: the stage groups drawn from above
//       into VRAM (0x180, 0x100) 256 x 256);  0x2C column flash (0x5E8450), column (0x5E8330, the shared renderer
//       0x64F0A0 with the captured stage as texture), a knock-down task (0x5E8DA0) per present party member;
//       0x30 sound 0xD78040;  0x33 screen fade (0, 2, 4, 0xFF);  0x36 the pillar (0x5E8590), 16 flames (0x5E86D0,
//       delayed 0..18 ticks), a throw task (0x5E8EF0) per present party member;  0x37 sparks (0x5E8870, pool A) and
//       debris (0x5E8A80, pool B). Screen flash (0x5713E0): counter << 8 below 8; red (DPCS of 0 toward (0x30, 0x10,
//       0x10) by (counter - 0x1B) / 8) from 0x1B to 0x22; white fading out from 0x5A.
//   Glow (0x5E5980, model 0xD7688C, 0x14 ticks), spins (0x5E5AB0, model 0xD767BC, 0x1A ticks), rings (0x5E5C30, model
//     0xD7712C, 12 ticks): prim models (0x572200) in the frame, scaled by the node, fading.
//   Bolt director (0x5E5D50, 16 ticks, draws nothing): on its tick 0 the record 0x23C1428 + e * 0x334: two random
//     ends around the centre, 3 random inner control points, 48 spline points (+0x38) of the 5 (+0x10); then an arc
//     (0x5E6030), a sheet (0x5E7AE0) and a ribbon (0x5E7ED0) along it.
//   Arc (0x5E6030): model 0xD719E4 through the module's renderer 0x5E6240 from the effect point to spline point +0x18
//     (+2 a tick up to 0x30). Sheet (0x5E7AE0): the last 3 (effect point, spline point) pairs kept in the record, two
//     splines of 16 points through them, 14 POLY_G4 pairs between them (colour 0xD78060[counter % 12], depth-cued).
//   Ribbon (0x5E7ED0): spline points 0..+0x18, each widened by +-0x5A across its direction (table 0x23C2DC8), POLY_G4
//     pairs, fading from 0x18; 0x24 ticks.
//   Column flash (0x5E8450, model 0xD756A4), column (0x5E8330, model 0xD7398C through 0x64F0A0), pillar (0x5E8590,
//     model 0xD722E4), flames (0x5E86D0, models 0xD7217C / 0xD72014 through MAG_151's renderer 0x64E080, v scroll),
//     ring 2 (0x5E9250, model 0xD7712C, at a thrown target's landing point).
//   Sparks (0x5E8870): 2 a tick into pool A (0x23BF080, 100 x 0x18) up to counter 0x10, sprite sequence 0xD718AC at
//     their position (light matrix = camera); ends when none is left. Debris (0x5E8A80): 3 a tick into pool B
//     (0x23BF9E0, 100 x 0x1C) for counters 0..5, prim models 0xD78090[kind] falling and turning, 16 ticks each.
//   Knock-down (0x5E8DA0, 9 ticks) / throw (0x5E8EF0, 0x1C ticks): a party member's flags +0, rotation +0x0C / +0x10
//     and position +0x1C / +0x20 saved (0x23C1360 + slot * 0x1C), moved (thrown up and turned, landing ring 2 and
//     sequence 6 at 0x14), put back; the throw applies the action result of its target record at the end (waiting
//     while the entity's sequence 6 runs).
// Every task but the root / restore tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// Module globals (166): 0x23BF038..0x23C3278.
// Fujin's described Megiddo Flames (effect_166.json, effect_312.json) run this code: its memory through own::R() (fx_port.h,
// G()), its spawns through own::Fn(), the setup and the file loader ported below; their parameters (MegiddoParams, one
// layout for both, each module's game values from its own addresses: the sounds, the director's ticks, the effect
// point's bone, the glow, spins, rings, bolts and flames' sizes, counts and spreads; its data: 12 meshes, the spark
// flipbook, the sheet colours, the effect point's offset, the texture name, the camera track). FX_PART tags its parts.

#pragma once

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace megiddo
{
	using namespace eng;
	using namespace magc;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }                                              // Math_SqrtInt
	inline void MatVec(const Mat4x3 *m, const void *in, void *out) { fn<void (__cdecl *)(const Mat4x3 *, const void *, void *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }         // b.rot = a.rot * b.rot
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); }
	inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, seq); }
	inline void SetDrawEnvPrim(uint32_t prim, uint32_t env) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(prim, env); }           // SSIGPU_SetDrawEnv
	inline void InsertPrimAltViewport(uint32_t bucket, uint32_t packet) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C8E0)(bucket, packet); }
	inline void SetDrawAreaPrim(uint32_t prim, const void *rect) { fn<void (__cdecl *)(uint32_t, const void *)>(0x45C940)(prim, rect); } // sub_45C940 (prim, RECT16)
	inline void SetDrawOffsetPrim(uint32_t prim, const void *xy) { fn<void (__cdecl *)(uint32_t, const void *)>(0x45C9B0)(prim, xy); }   // sub_45C9B0 (prim, int16 xy[2])
	inline void GteReadScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }               // sub_56CD10: OFX / OFY >> 16
	inline void GteSetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }                      // Call_Bs_ParseCamera
	inline void SetProjectionH(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); }                                                // Battle_SetProjectionH
	inline uint32_t RenderGeometry(uint32_t geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); }   // pre_someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }             // MAC = LLM * V0 + BK
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = m.t
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }   // R = s * I
	inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                   // TR = MAC1..3
	inline uint32_t Mag151Render(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64E080)(h, ot, mode, cursor); } // MAG_151_sub_64E080
	// MAG_166_sub_64F0A0: the prim-model renderer at 0x64F0A0 (also called by Wall 189 and Fake President's death 315)
	inline uint32_t ColumnRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64F0A0)(h, ot, mode, cursor); }
	inline uint32_t OT0() { return var<uint32_t>(0x1D8E04C); }

#pragma pack(push, 1)
	struct RootNode // root pool, 2 x 0x10
	{
		TaskNode hdr;
		int16_t c;         // +0x0C
		int16_t e;         // +0x0E restore: done
	};
	struct Node24 // main pool, 100 x 0x24
	{
		TaskNode hdr;
		int16_t c;         // +0x0C counter
		int16_t e;         // +0x0E bolt record / party slot / flame delay
		int16_t w10, w12, w14, w16; // +0x10 position
		int16_t w18;       // +0x18 angle / spline index / throw turn
		int16_t w1A;       // +0x1A angle rate / index step / target record (-1 none) / flame model
		int16_t w1C;       // +0x1C scale
		int16_t w1E;       // +0x1E scale rate
		int16_t w20;       // +0x20 y scale / height
		int16_t w22;       // +0x22 y scale rate / v scroll rate
	};
	// the module's prim-model renderer header (0x6C bytes on the scratch stack; gu066::RenderHeader)
	struct RenderHeader
	{
		uint32_t model;    // +0x00
		uint32_t verts;    // +0x04 vertex base (model + 8 unless flag 0x2000)
		uint8_t far_rgb[4];// +0x08 GTE far colour
		int32_t fade;      // +0x0C depth-cue level (IR0) with flag 0x40 / 0x80
		uint16_t u_off;    // +0x10 flags 0x400 (add) / 0x100 (set)
		uint16_t pad12;
		uint16_t v_off;    // +0x14 flags 0x800 (add) / 0x200 (set)
		uint16_t pad16;
		int32_t depth;     // +0x18 added to the uv words (0 unless flag 0x1000)
		uint32_t flags;    // +0x1C
		uint32_t colour;   // +0x20 code / colour of the flat lists, colour scale of the gouraud lists
		uint32_t list;     // +0x24 current primitive list
		int32_t mac0;      // +0x28
		uint32_t pad2C;
		int32_t otz;       // +0x30
		uint32_t gte_flag; // +0x34
		uint8_t pad38[0x20];
		uint32_t scale_rgb;// +0x58 copy of +0x20 (gouraud lists)
		uint32_t c[4];     // +0x5C vertex colours (gouraud lists)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24 && sizeof(RenderHeader) == 0x6C, "Megiddo Flame layouts");

	template<typename T> static inline T &At(uint32_t base, int32_t off) { return *(T *)(base + off); }

	// ------------------------------------------------------------------
	// Prim-model renderer 0x5E6240 (312: 0x59DF40) = Blasting Zone's 0x5F2240 = Griever death's 0x623240
	// (blast164::RenderModel, gu066::RenderModel): eight primitive lists, byte copies
	// ------------------------------------------------------------------
	namespace rm
	{
	static inline bool OutX(uint32_t xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(uint32_t xy) { int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	static inline void UvOffsets(const RenderHeader *h, uint32_t pkt, uint32_t u_word, uint32_t v_word)
	{
		const uint32_t fl = h->flags;
		if (fl & 0x400) At<uint16_t>(pkt, u_word) = (uint16_t)(At<uint16_t>(pkt, u_word) + h->u_off);
		else if (fl & 0x100) At<uint16_t>(pkt, u_word) = h->u_off;
		if (fl & 0x800) At<uint16_t>(pkt, v_word) = (uint16_t)(At<uint16_t>(pkt, v_word) + h->v_off);
		else if (fl & 0x200) At<uint16_t>(pkt, v_word) = h->v_off;
	}

	static inline void ScaleColours(RenderHeader *h, int n)
	{
		h->scale_rgb = h->colour;
		uint8_t *s = (uint8_t *)&h->scale_rgb;
		for (int k = 0; k < n; k++)
		{
			uint8_t *c = (uint8_t *)&h->c[k];
			c[0] = (uint8_t)((uint32_t)c[0] * s[0] >> 7);
			c[1] = (uint8_t)((uint32_t)c[1] * s[1] >> 7);
			c[2] = (uint8_t)((uint32_t)c[2] * s[2] >> 7);
		}
	}

	static inline uint32_t Bucket(uint32_t ot, const RenderHeader *h, int32_t shift) { return ot + (uint32_t)(h->otz >> shift) * 4; }

	// the flat / flat-textured lists' common steps: depth cue of the flat colour word
	static inline void FlatDepthCue(RenderHeader *h, uint32_t pkt)
	{
		if (h->flags & 0x40)
		{
			GteLoadRGBC((const void *)(pkt + 4));
			GteSetIR0(h->fade);
			GteDPCS();
			GteStoreRGB2((void *)(pkt + 4));
		}
	}

	// Flat triangles (0x5E6380): 0xC-byte records (vertex indices +4/+6/+8) -> POLY_F3 (0x14)
	static uint32_t ListF3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t code = h->colour | 0x20000000;
			const uint32_t fl = h->flags;
			At<uint32_t>(pkt, 0) = 0x4000000;
			At<uint32_t>(pkt, 4) = code;
			if (fl & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (fl & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0xC), (void *)(pkt + 0x10));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0xC)) clip |= 2;
					if (OutX(pkt + 0x10)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0xC)) clip |= 0x20;
					if (OutY(pkt + 0x10)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						FlatDepthCue(h, pkt);
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x14;
					}
				}
			}
			rec += 0xC;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Flat quads (0x5E6590): 0xC-byte records (vertex indices +4/+6/+8/+0xA) -> POLY_F4 (0x18)
	static uint32_t ListF4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t code = h->colour | 0x28000000;
			const uint32_t fl = h->flags;
			At<uint32_t>(pkt, 0) = 0x5000000;
			At<uint32_t>(pkt, 4) = code;
			if (fl & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (fl & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0xC), (void *)(pkt + 0x10));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0xC)) clip |= 2;
					if (OutX(pkt + 0x10)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0xC)) clip |= 0x20;
					if (OutY(pkt + 0x10)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x14));
					GteAVSZ4();
					if (OutX(pkt + 0x14)) clip |= 8;
					if (OutY(pkt + 0x14)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						FlatDepthCue(h, pkt);
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x18;
					}
				}
			}
			rec += 0xC;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Flat textured triangles (0x5E6820): 0x14-byte records -> POLY_FT3 (0x20)
	static uint32_t ListFT3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			uint32_t code = h->colour | 0x24000000;
			At<uint32_t>(pkt, 0) = 0x7000000;
			At<uint32_t>(pkt, 4) = code;
			if (h->flags & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (h->flags & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			At<uint32_t>(pkt, 0x14) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x1C) = (At<uint32_t>(rec, 8) >> 16) + d;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x16, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						FlatDepthCue(h, pkt);
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x20;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Flat textured quads (0x5E6A80): 0x18-byte records -> POLY_FT4 (0x28)
	static uint32_t ListFT4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			uint32_t code = h->colour | 0x2C000000;
			At<uint32_t>(pkt, 0) = 0x9000000;
			At<uint32_t>(pkt, 4) = code;
			if (h->flags & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (h->flags & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			const uint32_t uv23 = (d << 16) + d + At<uint32_t>(rec, 0x14);
			At<uint32_t>(pkt, 0x1C) = uv23;
			At<uint32_t>(pkt, 0x14) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = uv23 >> 16;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x16, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x20));
					GteAVSZ4();
					if (OutX(pkt + 0x20)) clip |= 8;
					if (OutY(pkt + 0x20)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						FlatDepthCue(h, pkt);
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Gouraud triangles (0x5E6D30): 0x14-byte records -> POLY_G3 (0x1C)
	static uint32_t ListG3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t fl = h->flags;
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0x6000000;
			h->c[0] = code;
			if (fl & 2) h->c[0] = code | 0x2000000;
			if (fl & 8) h->c[0] &= 0xFDFFFFFF;
			h->c[1] = At<uint32_t>(rec, 0xC);
			h->c[2] = At<uint32_t>(rec, 0x10);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 3);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[0]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0xC), (void *)(pkt + 0x14), (void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0xC) = h->c[1];
							At<uint32_t>(pkt, 0x14) = h->c[2];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Gouraud quads (0x5E7030): 0x18-byte records -> POLY_G4 (0x24)
	static uint32_t ListG4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t fl = h->flags;
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0x8000000;
			h->c[0] = code;
			if (fl & 2) h->c[0] = code | 0x2000000;
			if (fl & 8) h->c[0] &= 0xFDFFFFFF;
			h->c[1] = At<uint32_t>(rec, 0xC);
			h->c[2] = At<uint32_t>(rec, 0x10);
			h->c[3] = At<uint32_t>(rec, 0x14);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x20));
					GteAVSZ4();
					if (OutX(pkt + 0x20)) clip |= 8;
					if (OutY(pkt + 0x20)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 4);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[3]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0xC), (void *)(pkt + 0x14), (void *)(pkt + 0x1C));
							GteLoadRGBC(&h->c[0]);
							GteDPCS();
							GteStoreRGB2((void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0xC) = h->c[1];
							At<uint32_t>(pkt, 0x14) = h->c[2];
							At<uint32_t>(pkt, 0x1C) = h->c[3];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Gouraud textured triangles (0x5E73D0) -> POLY_GT3 (0x28)
	static uint32_t ListGT3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0x9000000;
			h->c[0] = code;
			if (h->flags & 2) h->c[0] = code | 0x2000000;
			if (h->flags & 8) h->c[0] &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			At<uint32_t>(pkt, 0x18) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = (At<uint32_t>(rec, 8) >> 16) + d;
			h->c[1] = At<uint32_t>(rec, 0x14);
			h->c[2] = At<uint32_t>(rec, 0x18);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x1A, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x14), (void *)(pkt + 0x20));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x14)) clip |= 2;
					if (OutX(pkt + 0x20)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x14)) clip |= 0x20;
					if (OutY(pkt + 0x20)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 3);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[0]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0x10), (void *)(pkt + 0x1C), (void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0x10) = h->c[1];
							At<uint32_t>(pkt, 0x1C) = h->c[2];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Gouraud textured quads (0x5E76F0) -> POLY_GT4 (0x34)
	static uint32_t ListGT4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012((const void *)(vb + (uint32_t)At<uint16_t>(rec, 4) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 6) * 4), (const void *)(vb + (uint32_t)At<uint16_t>(rec, 8) * 4));
			GteRTPT();
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0xC000000;
			h->c[0] = code;
			if (h->flags & 2) h->c[0] = code | 0x2000000;
			if (h->flags & 8) h->c[0] &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			const uint32_t uv23 = (d << 16) + d + At<uint32_t>(rec, 0x14);
			At<uint32_t>(pkt, 0x18) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = uv23;
			At<uint32_t>(pkt, 0x30) = uv23 >> 16;
			h->c[1] = At<uint32_t>(rec, 0x18);
			h->c[2] = At<uint32_t>(rec, 0x1C);
			h->c[3] = At<uint32_t>(rec, 0x20);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x1A, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x14), (void *)(pkt + 0x20));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x14)) clip |= 2;
					if (OutX(pkt + 0x20)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x14)) clip |= 0x20;
					if (OutY(pkt + 0x20)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x2C));
					GteAVSZ4();
					if (OutX(pkt + 0x2C)) clip |= 8;
					if (OutY(pkt + 0x2C)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 4);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[3]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0x10), (void *)(pkt + 0x1C), (void *)(pkt + 0x28));
							GteLoadRGBC(&h->c[0]);
							GteDPCS();
							GteStoreRGB2((void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0x10) = h->c[1];
							At<uint32_t>(pkt, 0x1C) = h->c[2];
							At<uint32_t>(pkt, 0x28) = h->c[3];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	// Prim-model renderer (0x5E6240): header h, OT, OT shift, packet cursor; the eight lists in order (an empty list is
	// one zero word)
	static uint32_t RenderModel(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t fl = h->flags;
		if (!(fl & 0x2000)) h->verts = h->model + 8;
		h->list = At<uint32_t>(h->model, 0) + h->model;
		if (!(fl & 0x1000)) h->depth = 0;
		GteSetFarColor(h->far_rgb[0], h->far_rgb[1], h->far_rgb[2]);
		typedef uint32_t (*List)(RenderHeader *, uint32_t, int32_t, uint32_t);
		static const List lists[8] = { ListF3, ListF4, ListFT3, ListFT4, ListG3, ListG4, ListGT3, ListGT4 };
		for (int i = 0; i < 8; i++)
		{
			if (At<uint32_t>(h->list, 0) != 0) cursor = lists[i](h, ot, shift, cursor);
			else h->list += 4;
		}
		return cursor;
	}
	}

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h): one layout for 166 and 312, the game values each module's
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_GLOW = 1, PART_SPINS = 2, PART_RINGS = 3, PART_BOLTS = 4, PART_COLUMN = 5, PART_PILLAR = 6, PART_FLAMES = 7,
	             PART_SPARKS = 8, PART_DEBRIS = 9, PART_LANDING = 10 };
	struct MegiddoParams
	{
		struct { uint32_t start, blast; } sounds;     // the director's glow tick, its blast tick
		// its ticks: the glow, the spins and bolts, the rings (every 6th from..to), the stage capture (the tick before the
		// column), the column, the blast sound, the pillar, the sparks and debris, the end
		struct { int16_t glow, bolts, rings_from, rings_to, column, blast, pillar, sparks, end; } director;
		struct { int16_t bone; } point;               // the caster's bone of the effect point
		struct { int16_t size, shrink; } glow;
		struct { int16_t size, spin_min, spin_rand; } spins;
		struct { int16_t size; } rings;
		struct { int16_t count, spread; } bolts;
		struct { int16_t count, spread_x, spread_z; } flames;
		struct { uint32_t glow, spin, ring, arc, column, column_flash, pillar, flame_a, flame_b, debris0, debris1, debris2; } models; // the prim models drawn (meshes)
		struct { uint32_t spark; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t sheet_colours, point_offset; } tables; // its small tables (data)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track
	};
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The timeline: the effect point at the caster's bone and the frame toward the targets' centre (every tick), when each part starts, the stage captured from above for the column, the screen flashes and fades, the sounds" },
			{ PART_GLOW, "glow", "Glow", "A glow model at the effect point, shrinking, fading at its end" },
			{ PART_SPINS, "spins", "Spins", "Two models at the effect point spinning (slowing down), fading in and out" },
			{ PART_RINGS, "rings", "Rings", "Ring models at the effect point every 6th tick, growing and fading" },
			{ PART_BOLTS, "bolts", "Bolts", "Bolts round the targets' centre (random splines): each an arc model from the effect point along it, a gouraud sheet between them and a widened ribbon on it" },
			{ PART_COLUMN, "column", "Column", "The column at the centre (textured with the stage drawn from above) and its flash, stretched up" },
			{ PART_PILLAR, "pillar", "Pillar", "The pillar model at the centre, growing up, fading" },
			{ PART_FLAMES, "flames", "Flames", "Flame models round the centre (two kinds, scrolled), each after a delay, spinning, fading" },
			{ PART_SPARKS, "sparks", "Sparks", "Spark sprites thrown round the centre (two a tick)" },
			{ PART_DEBRIS, "debris", "Debris", "Debris models thrown round the centre (three a tick), falling and turning" },
			{ PART_LANDING, "landing", "Knock-down and landing", "Each present party member knocked down, then thrown up, turned and landing (a ring model where it lands); the action result at the end" },
		};
		static const fxp::Group GROUPS[] = {
			{ "sounds", "Sounds", "The start, the blast" },
		};
		static const Field FIELDS[] = {
			{ "director", "sounds.start", "Start", "At the director's glow tick", (uint32_t)offsetof(MegiddoParams, sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.blast", "Blast", "At the director's blast tick", (uint32_t)offsetof(MegiddoParams, sounds.blast), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.glow", "Glow tick", "The glow (and the start sound) at this tick of the director", (uint32_t)offsetof(MegiddoParams, director.glow), fxp::I16, 0, 300 },
			{ "director", "director.bolts", "Bolts tick", "The two spins and the bolts start at this tick", (uint32_t)offsetof(MegiddoParams, director.bolts), fxp::I16, 0, 300 },
			{ "director", "director.rings_from", "Rings from", "Rings start on the ticks from this one to the next (each tick whose remainder by 6 is 2)", (uint32_t)offsetof(MegiddoParams, director.rings_from), fxp::I16, 0, 300 },
			{ "director", "director.rings_to", "Rings until", "(the last tick a ring may start)", (uint32_t)offsetof(MegiddoParams, director.rings_to), fxp::I16, -1, 300 },
			{ "director", "director.column", "Column tick", "The column, its flash and the knock-downs start at this tick (the stage is captured the tick before)", (uint32_t)offsetof(MegiddoParams, director.column), fxp::I16, 1, 300 },
			{ "director", "director.blast", "Blast tick", "The blast sound at this tick", (uint32_t)offsetof(MegiddoParams, director.blast), fxp::I16, 0, 300 },
			{ "director", "director.pillar", "Pillar tick", "The pillar, the flames and the throws start at this tick", (uint32_t)offsetof(MegiddoParams, director.pillar), fxp::I16, 0, 300 },
			{ "director", "director.sparks", "Sparks tick", "The sparks and the debris start at this tick", (uint32_t)offsetof(MegiddoParams, director.sparks), fxp::I16, 0, 300 },
			{ "director", "director.end", "End tick", "The director's last tick (the white flash fades out from 0x5A to 0x62; the effect ends once every part has)", (uint32_t)offsetof(MegiddoParams, director.end), fxp::I16, 0, 300 },
			{ "director", "point.bone", "Effect point bone", "The caster's bone the effect point is on (0xF0 and up: the model's named points; below: a bone)", (uint32_t)offsetof(MegiddoParams, point.bone), fxp::I16, 0, 0xFF },
			{ "director", "tables.point_offset", "Effect point offset", "The effect point's offset in that bone's frame (x, y, z, pad) (8 bytes: an edited one keeps its length)", (uint32_t)offsetof(MegiddoParams, tables.point_offset), fxp::Data, 0, 0x7FFFFFFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(MegiddoParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(MegiddoParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "glow", "glow.size", "Size", "Its scale at the start (4096 = the model's own)", (uint32_t)offsetof(MegiddoParams, glow.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "glow", "glow.shrink", "Shrink", "Taken from its scale on its first tick, 5/6 of that the next, and so on for 8 ticks", (uint32_t)offsetof(MegiddoParams, glow.shrink), fxp::I16, -0x7FFF, 0x7FFF },
			{ "glow", "models.glow", "Glow model", "The prim model glow (a mesh of 73 vertices: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(MegiddoParams, models.glow), fxp::Model, 0, 0x7FFFFFFF },
			{ "spins", "spins.size", "Size", "Their scale (4096 = the model's own)", (uint32_t)offsetof(MegiddoParams, spins.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "spins", "spins.spin_min", "Spin", "The first one's turn a tick: this + a random below the next (4096 = a full turn; 7/8 of it the next tick...; the second turns the other way by its first angle)", (uint32_t)offsetof(MegiddoParams, spins.spin_min), fxp::I16, -0x7FFF, 0x7FFF, fxp::Angle },
			{ "spins", "spins.spin_rand", "Spin random", "(at least 1)", (uint32_t)offsetof(MegiddoParams, spins.spin_rand), fxp::I16, 1, 0x7FFF },
			{ "spins", "models.spin", "Spin model", "The prim model spin (a mesh of 9 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.spin), fxp::Model, 0, 0x7FFFFFFF },
			{ "rings", "rings.size", "Size", "Its scale at the start and its first growth (5/6 of it the next tick, and so on)", (uint32_t)offsetof(MegiddoParams, rings.size), fxp::I16, 0, 0x7FFF },
			{ "rings", "models.ring", "Ring model", "The prim model ring (a mesh of 72 vertices: an edited one keeps its vertices; the landing rings draw it too)", (uint32_t)offsetof(MegiddoParams, models.ring), fxp::Model, 0, 0x7FFFFFFF },
			{ "bolts", "bolts.count", "Count", "How many bolts (6 records: 6 at most)", (uint32_t)offsetof(MegiddoParams, bolts.count), fxp::I16, 0, 6 },
			{ "bolts", "bolts.spread", "Spread", "Each bolt's two ends: the centre moved by a random below this on x and z, centred", (uint32_t)offsetof(MegiddoParams, bolts.spread), fxp::I16, 1, 0x7FFF },
			{ "bolts", "models.arc", "Arc model", "The prim model arc (a mesh of 65 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.arc), fxp::Model, 0, 0x7FFFFFFF },
			{ "bolts", "tables.sheet_colours", "Sheet colours", "The sheet's colour words by its tick (12, in turn: red, green, blue, the GPU code byte) (48 bytes: an edited one keeps its length)", (uint32_t)offsetof(MegiddoParams, tables.sheet_colours), fxp::Data, 0, 0x7FFFFFFF },
			{ "column", "models.column", "Column model", "The prim model column (a mesh of 176 vertices: an edited one keeps its vertices; its own renderer textures it with the captured stage)", (uint32_t)offsetof(MegiddoParams, models.column), fxp::Model, 0, 0x7FFFFFFF },
			{ "column", "models.column_flash", "Column flash model", "The prim model column flash (a mesh of 110 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.column_flash), fxp::Model, 0, 0x7FFFFFFF },
			{ "pillar", "models.pillar", "Pillar model", "The prim model pillar (a mesh of 192 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.pillar), fxp::Model, 0, 0x7FFFFFFF },
			{ "flames", "flames.count", "Count", "How many flames (each delayed by its number + a random below 4; the task pool holds 100 tasks: 64 at most)", (uint32_t)offsetof(MegiddoParams, flames.count), fxp::I16, 0, 64 },
			{ "flames", "flames.spread_x", "Spread x", "A flame's place: the centre moved by a random below this on x, centred", (uint32_t)offsetof(MegiddoParams, flames.spread_x), fxp::I16, 1, 0x7FFF },
			{ "flames", "flames.spread_z", "Spread z", "... and below this on z", (uint32_t)offsetof(MegiddoParams, flames.spread_z), fxp::I16, 1, 0x7FFF },
			{ "flames", "models.flame_a", "Flame model a", "The prim model flame a (a mesh of 16 vertices: an edited one keeps its vertices; drawn by the shared scrolling renderer)", (uint32_t)offsetof(MegiddoParams, models.flame_a), fxp::Model, 0, 0x7FFFFFFF },
			{ "flames", "models.flame_b", "Flame model b", "The prim model flame b (a mesh of 16 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.flame_b), fxp::Model, 0, 0x7FFFFFFF },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 10 frames: an edited one keeps as many)", (uint32_t)offsetof(MegiddoParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "debris", "models.debris0", "Debris model 0", "The prim model debris 0 (a mesh of 12 vertices: an edited one keeps its vertices; a debris piece is one of the three at random)", (uint32_t)offsetof(MegiddoParams, models.debris0), fxp::Model, 0, 0x7FFFFFFF },
			{ "debris", "models.debris1", "Debris model 1", "The prim model debris 1 (a mesh of 12 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.debris1), fxp::Model, 0, 0x7FFFFFFF },
			{ "debris", "models.debris2", "Debris model 2", "The prim model debris 2 (a mesh of 12 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegiddoParams, models.debris2), fxp::Model, 0, 0x7FFFFFFF },
		};
	}

#ifdef FF8_FX_HELD
	template<class D> struct MegiddoHeld; // 30 fps layer: see fx_megiddo_held.inc
#endif

	// The module, for a descriptor D (166 addresses moved by D::CODE / D::GLOB / D::DATA)
	template<class D>
	struct Megiddo
	{
		static constexpr uint32_t C(uint32_t a) { return a - D::CODE; }   // code
		static uint32_t G(uint32_t a) { return own::R(a - D::GLOB); }     // module globals (where the running copy has them)
		static constexpr uint32_t X(uint32_t a) { return a - D::DATA; }   // data

		// --- module globals ---
		static int32_t &TargetSlot() { return var<int32_t>(G(0x23BF038)); }
		static CastContext *&Context() { return var<CastContext *>(G(0x23C3250)); }
		static int32_t &CasterSlot() { return var<int32_t>(G(0x23C3254)); }
		static uint32_t &TexFile() { return var<uint32_t>(G(0x23C3258)); }
		static uint32_t &TexBase() { return var<uint32_t>(G(0x23C3270)); }
		static uint32_t &PacketCursor() { return var<uint32_t>(G(0x23C3274)); }
		static uint32_t &FlashColour() { return var<uint32_t>(G(0x23C3268)); }
		static TaskQueue *QRoot() { return (TaskQueue *)G(0x23BF070); }
		static TaskQueue *QMain() { return (TaskQueue *)G(0x23C0540); }
		static int16_t *Centre() { return (int16_t *)G(0x23C3248); }      // the targets' centre (x, y, z, pad)
		static int16_t *Point() { return (int16_t *)G(0x23C3260); }       // the effect point at the caster's bone
		static Mat4x3 *Frame() { return (Mat4x3 *)G(0x23C04E0); }         // the effect frame (rotation + the point)
		static uint32_t Bolt(int32_t e) { return G(0x23C1428) + 0x334 * e; } // bolt records (6 x 0x334)
		static uint32_t Save(int32_t slot) { return G(0x23C1360) + 0x1C * slot; } // knock-down / throw: the entity words kept
		static const uint32_t POOL_A = 0x23BF080, POOL_A_END = 0x23BF9E0;  // sparks, 100 x 0x18 (166 addresses)
		static const uint32_t POOL_B = 0x23BF9E0, POOL_B_END = 0x23C04D0;  // debris, 100 x 0x1C
		static const uint32_t RIBBON_TABLE = 0x23C2DC8;                    // ribbon points (24 bytes each)

		// --- tasks (166 addresses) ---
		static const uint32_t ORIG_Root = 0x5E93A0, ORIG_Restore = 0x5E9410, ORIG_Director = 0x5E4F60, ORIG_Glow = 0x5E5980,
			ORIG_Spin = 0x5E5AB0, ORIG_Ring = 0x5E5C30, ORIG_BoltDir = 0x5E5D50, ORIG_Arc = 0x5E6030, ORIG_Sheet = 0x5E7AE0,
			ORIG_Ribbon = 0x5E7ED0, ORIG_Column = 0x5E8330, ORIG_ColumnFlash = 0x5E8450, ORIG_Pillar = 0x5E8590,
			ORIG_Flame = 0x5E86D0, ORIG_Sparks = 0x5E8870, ORIG_Debris = 0x5E8A80, ORIG_Knock = 0x5E8DA0, ORIG_Throw = 0x5E8EF0,
			ORIG_Ring2 = 0x5E9250;

		// --- data (166 addresses) ---
		static const uint32_t BONE_OFFSET = 0xD78048, REF_DIR = 0xD78050, SOUND_START = 0xD7803C, SOUND_BLAST = 0xD78040,
			SHEET_COLOURS = 0xD78060, DEBRIS_MODELS = 0xD78090, ALT_VIEWPORT = 0xD780A0, SEQ_SPARK = 0xD718AC,
			MODEL_GLOW = 0xD7688C, MODEL_SPIN = 0xD767BC, MODEL_RING = 0xD7712C, MODEL_ARC = 0xD719E4, MODEL_COLUMN = 0xD7398C,
			MODEL_COLUMN_FLASH = 0xD756A4, MODEL_PILLAR = 0xD722E4, MODEL_FLAME_A = 0xD7217C, MODEL_FLAME_B = 0xD72014;
		static const uint32_t MODEL_DEBRIS0 = 0xD77814, MODEL_DEBRIS1 = 0xD77ACC, MODEL_DEBRIS2 = 0xD77D84;   // DEBRIS_MODELS' three
		static const uint32_t TEXTURE_NAME = 0xD780B8;
		static const uint32_t ORIG_FileLoader = 0x5E4E70, ORIG_Setup = 0x5E4E90;

		// --- parameters (MegiddoParams): this module's game values ---
		static MegiddoParams Game()
		{
			return { { X(SOUND_START), X(SOUND_BLAST) },
			         { 1, 8, 6, 0x1E, 0x2C, 0x30, 0x36, 0x37, 0x62 },
			         { (int16_t)D::BONE },
			         { 0x6800, 0x14CC },
			         { 0x2000, 0x14, 0x28 },
			         { 0x340 },
			         { 6, 0x1CE8 },
			         { 0x10, 0x15E0, 0xAF0 },
			         { X(MODEL_GLOW), X(MODEL_SPIN), X(MODEL_RING), X(MODEL_ARC), X(MODEL_COLUMN), X(MODEL_COLUMN_FLASH), X(MODEL_PILLAR),
			           X(MODEL_FLAME_A), X(MODEL_FLAME_B), X(MODEL_DEBRIS0), X(MODEL_DEBRIS1), X(MODEL_DEBRIS2) },
			         { X(SEQ_SPARK) },
			         { X(SHEET_COLOURS), X(BONE_OFFSET) },
			         { X(TEXTURE_NAME), D::CAMERA } };
		}
		static inline const MegiddoParams GAME = Game();
		static inline MegiddoParams live = Game();
		static const MegiddoParams &P() { return live; }

		static Node24 *AddMain(uint32_t f) { return (Node24 *)AddTaskToQueue(QMain(), own::Fn(C(f))); }
		static inline uint32_t EndAt(int16_t c, int16_t limit) { return c >= limit ? TASK_END : 0; }

		// a prim-model header's fade by counter: wf = the fade word +0x0C is written (else the scratch keeps its bytes)
		struct PF { bool wf; int32_t fade; uint32_t flags; };
		static void ApplyPF(uint8_t *h, const PF &f, int flags_off)
		{
			if (f.wf) *(int32_t *)(h + 0xC) = f.fade;
			*(uint32_t *)(h + flags_off) = f.flags;
		}
		static PF GlowPF(int32_t c) { if (c == 0) return { true, 0x800, 0xF3 }; if (c >= 0xC) return { true, shl32(c - 0xC, 9), 0xF3 }; return { false, 0, 0x33 }; }
		static PF SpinPF(int32_t c) { if (c < 4) return { true, shl32(8 - c, 9) | 0xC0, 0x33 }; if (c >= 0x12) return { true, shl32(c - 0x12, 9), 0xF3 }; return { false, 0, 0x33 }; }
		static PF RingPF(int32_t c) { if (c >= 1) return { true, mul32(c - 1, 0x174), 0xF3 }; return { false, 0, 0x33 }; }
		static PF ArcPF(int32_t c) { if (c == 0) return { true, 0x800, 0xF3 }; if (c >= 0x10) return { true, shl32(c - 0x10, 9), 0xF3 }; return { false, 0, 0x33 }; }
		static PF ColumnFlashPF(int32_t c) { return { true, c < 8 ? shl32(0x10 - c, 8) : 0x800, 0xC3 }; }
		static PF PillarPF(int32_t c) { return { true, c >= 0 ? mul32(c, 0xCC) : 0, 0xF3 }; }
		static PF Ring2PF(int32_t c) { if (c >= 2) return { true, mul32(c - 2, 0x199), 0xF3 }; return { false, 0, 0x33 }; }
		static PF FlamePF(int32_t c) { if (c < 4) return { true, shl32(4 - c, 10), 0xF3 }; if (c >= 10) return { true, shl32(c - 10, 9), 0xF3 }; return { false, 0, 0x33 }; }

		// 0x5E56C0: a bone's matrix of an animation header (bones >= 0xF0 through the skeleton's remap bytes)
		static void BoneMatrix(uint32_t anim_header, int32_t bone, Mat4x3 *out)
		{
			const uint32_t sk = *(const uint32_t *)*(const uint32_t *)(anim_header + 4);
			uint32_t b = (uint32_t)bone;
			if (bone >= 0xF0)
			{
				b = *(const uint8_t *)(sk + bone - 0xEC);
				if (b & 0x80) b &= 0x7F;
			}
			memcpy(out, (const void *)(sk + b * 0x30 + 0x20), 0x20);
		}

		// the director's first steps: the effect point at the caster's bone and the frame toward the centre
		static void EffectPoint(int16_t *point)
		{
			const uint32_t ent = 0x1D972C0 + 0x9C * (uint32_t)CasterSlot();
			Mat4x3 m;
			BoneMatrix(ent + 0x60, P().point.bone, &m);
			ComposeAffineTransform((const Mat4x3 *)(ent + 0x40), &m, &m);
			MatVec(&m, (const void *)fxp::data(P().tables.point_offset, X(BONE_OFFSET), 8), point);
			point[0] = (int16_t)(point[0] + (int16_t)m.t[0]);
			point[1] = (int16_t)(point[1] + (int16_t)m.t[1]);
			point[2] = (int16_t)(point[2] + (int16_t)m.t[2]);
		}
		static void FrameAt(const int16_t *point, Mat4x3 *frame)
		{
			int32_t v[3], axis[4];
			v[0] = Centre()[0] - point[0];
			v[1] = Centre()[1] - point[1];
			v[2] = Centre()[2] - point[2];
			Normalize(v, v);
			const int32_t angle = RotationBetweenVectors((const int32_t *)X(REF_DIR), v, axis);
			BuildAxisAngleRotationMatrix(angle, frame, axis);
			frame->t[1] = point[1];
			frame->t[0] = point[0];
			frame->t[2] = point[2];
		}

		// ------------------------------------------------------------------
		// Director (0x5E4F60)
		// ------------------------------------------------------------------
		// first party slot -> index of its record among action 0's targets, -1 when it is not targeted
		static int32_t TargetIndex(int32_t slot)
		{
			const ActionData *a = Context()->actions;
			const int32_t n = a->target_count;
			for (int32_t i = 0; i < n; i++)
				if (slot == (int32_t)a->targets[i * TARGET_STRIDE]) return i;
			return -1;
		}

		static uint32_t __cdecl DirectorTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			EffectPoint(Point());
			FrameAt(Point(), Frame());
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_director(n);)
			if (DrawOnly()) return 0;
			if (n->c == 0)
			{
				CalculateCenterPosition(Context()->target_mask, Centre());
				FlashColour() = 0;
			}
			if (n->c == P().director.column - 1) { FX_PART(PART_COLUMN); StageCapture(Centre()); FX_PART(0); }
			if (n->c == P().director.glow)
			{
				Node24 *t = AddMain(ORIG_Glow);
				t->c = 0;
				t->w1C = P().glow.size;
				t->w1E = P().glow.shrink;
			}
			if (n->c == P().director.bolts)
			{
				Node24 *a = AddMain(ORIG_Spin);
				a->c = 0;
				a->w18 = (int16_t)(CrtRand() % 0x1000);
				const int32_t r = CrtRand() % P().spins.spin_rand;
				a->w1C = P().spins.size;
				a->w1A = (int16_t)(r + P().spins.spin_min);
				Node24 *b = AddMain(ORIG_Spin);
				b->c = 0;
				const int16_t ang = (int16_t)(CrtRand() % 0x1000);
				b->w18 = ang;
				b->w1A = (int16_t)-a->w18; // (the first spin's angle, as the original)
				b->w1C = P().spins.size;
			}
			if (n->c >= P().director.rings_from && n->c <= P().director.rings_to && n->c % 6 == 2)
			{
				Node24 *t = AddMain(ORIG_Ring);
				t->c = 0;
				t->w1E = P().rings.size;
				t->w1C = P().rings.size;
			}
			if (n->c == P().director.bolts)
				for (int i = 0; i < P().bolts.count && i < 6; i++)
				{
					Node24 *t = AddMain(ORIG_BoltDir);
					t->e = (int16_t)i;
					t->c = 0;
				}
			if (n->c == P().director.column)
			{
				Node24 *t = AddMain(ORIG_ColumnFlash);
				*(uint32_t *)&t->w10 = *(const uint32_t *)Centre();
				t->c = 0;
				*(uint32_t *)&t->w14 = *(const uint32_t *)(Centre() + 2);
				t->w1C = 0;
				t->w1E = 0x200;
				if (n->c == P().director.column)
				{
					t = AddMain(ORIG_Column);
					*(uint32_t *)&t->w10 = *(const uint32_t *)Centre();
					t->c = 0;
					*(uint32_t *)&t->w14 = *(const uint32_t *)(Centre() + 2);
					t->w1C = 0;
					t->w1E = 0x200;
				}
			}
			if (n->c == 6) ScreenFadeTask(2, 1, 0xA, 0xFF);
			if (n->c == 0x33) ScreenFadeTask(0, 2, 4, 0xFF);
			if (n->c == P().director.pillar)
			{
				Node24 *t = AddMain(ORIG_Pillar);
				*(uint32_t *)&t->w10 = *(const uint32_t *)Centre();
				*(uint32_t *)&t->w14 = *(const uint32_t *)(Centre() + 2);
				t->c = 0;
				t->w1C = 0x1000;
				t->w22 = 0x555;
				t->w20 = 0x555;
				if (n->c == P().director.pillar)
					for (int i = 0; i < P().flames.count; i++)
					{
						Node24 *f = AddMain(ORIG_Flame);
						f->c = 0;
						f->e = (int16_t)(CrtRand() % 4 + i);
						*(uint32_t *)&f->w10 = *(const uint32_t *)Centre();
						*(uint32_t *)&f->w14 = *(const uint32_t *)(Centre() + 2);
						f->w10 = (int16_t)(f->w10 + (int16_t)(CrtRand() % P().flames.spread_x - P().flames.spread_x / 2));
						f->w14 = (int16_t)(f->w14 + (int16_t)(CrtRand() % P().flames.spread_z - P().flames.spread_z / 2));
						const int32_t r = CrtRand() % 0x1000;
						f->w18 = (int16_t)r;
						f->w1A = (int16_t)(r & 1);
						f->w20 = 0x4000;
						f->w1C = (int16_t)(CrtRand() % 0x1800 + 0x1C00);
						f->w22 = (int16_t)(CrtRand() % 10 + 6);
					}
			}
			if (n->c == P().director.sparks)
			{
				Node24 *t = AddMain(ORIG_Sparks);
				*(uint32_t *)&t->w10 = *(const uint32_t *)Centre();
				*(uint32_t *)&t->w14 = *(const uint32_t *)(Centre() + 2);
				t->c = 0;
				if (n->c == P().director.sparks)
				{
					t = AddMain(ORIG_Debris);
					*(uint32_t *)&t->w10 = *(const uint32_t *)Centre();
					t->c = 0;
					*(uint32_t *)&t->w14 = *(const uint32_t *)(Centre() + 2);
				}
			}
			if (n->c == P().director.column)
				for (int32_t slot = 0; slot < 3; slot++)
				{
					if (!(*Entity(slot) & 2)) continue;
					Node24 *t = AddMain(ORIG_Knock);
					t->e = (int16_t)slot;
					const int32_t idx = TargetIndex(slot);
					t->c = 0;
					t->w20 = 0x1F4;
					t->w1A = (int16_t)idx;
					if (idx < 0) t->w20 = 0xFA;
				}
			if (n->c == P().director.pillar)
				for (int32_t slot = 0; slot < 3; slot++)
				{
					if (!(*Entity(slot) & 2)) continue;
					Node24 *t = AddMain(ORIG_Throw);
					t->c = 0;
					t->e = (int16_t)slot;
					t->w18 = (int16_t)((CrtRand() % 3 + 1) << 12);
					const int16_t h = (int16_t)(-0xBB8 - CrtRand() % 0x5DC);
					t->w20 = h;
					const int32_t idx = TargetIndex(slot);
					t->w1A = (int16_t)idx;
					if (idx < 0) t->w20 = (int16_t)(h >> 1);
				}
			const int16_t c = n->c;
			if (c < 8) SetScreenFlash((uint32_t)shl32(c, 8), 0);
			else if (c >= 0x1B && c < 0x23)
			{
				const int32_t ir0 = shl32(c - 0x1B, 12) / 8;
				GteSetFarColor2(0x30, 0x10, 0x10);
				FlashColour() = 0;
				GteLoadRGBC(&FlashColour());
				GteSetIR0(ir0);
				GteDPCS();
				GteStoreRGB2(&FlashColour());
				SetScreenFlash((uint32_t)shl32(n->c - 0xB, 7), FlashColour());
			}
			else if (c >= 0x5A) SetScreenFlash((uint32_t)shl32((0x62 - c) * 3, 7), 0x101030);
			if (n->c == P().director.glow) BdPlaySE((const void *)fxp::sound(P().sounds.start, X(SOUND_START)), 0, 0x80);
			if (n->c == P().director.blast) BdPlaySE((const void *)fxp::sound(P().sounds.blast, X(SOUND_BLAST)), 0, 0x80);
			n->c = (int16_t)(n->c + 1);
			return n->c > P().director.end ? TASK_END : 0;
		}

		// 0x5E5700: the shown stage groups (0x1D989BD / 0x1D98991, flags 1 and 2) drawn from above (rotation (0x400, 0, 0),
		// scaled (0x1000000 / (0x2378000 / 0x100), 0x1000, 0x1000000 / (0x17A4000 / 0x100)), at (-x / 36, z / 24, 0x7FFF)
		// of the centre) into the draw area (0x180, 0x100) 256 x 256 (screen offset 0x80, 0x80; projection 0x7FFF), the
		// draw environment back (0xD780A0), the area / offset primitives 0x23BF040 / 0x23C04D0
		static void StageCapture(const int16_t *centre)
		{
			int16_t rect[4] = { 0x180, 0x100, 0x100, 0x100 };
			int32_t ofx, ofy;
			int16_t angles[4];
			Mat4x3 m;
			int32_t q = (int32_t)(((int64_t)centre[0] * (int32_t)0xC71C71C7) >> 32) >> 3;
			m.t[0] = q + (int32_t)((uint32_t)q >> 31);
			q = (int32_t)(((int64_t)centre[2] * 0x2AAAAAAB) >> 32) >> 2;
			m.t[1] = q + (int32_t)((uint32_t)q >> 31);
			angles[0] = 0x400;
			angles[1] = 0;
			angles[2] = 0;
			m.t[2] = 0x7FFF;
			BuildRotationMatrixFromAngles(angles, &m);
			int32_t s[3];
			s[1] = 0x1000;
			s[0] = 0x1000000 / (0x2378000 / rect[2]);
			s[2] = 0x1000000 / (0x17A4000 / rect[3]);
			Scale3DMatrix(&m, s);
			const uint32_t env = 0x1D969C8 + 0x5C * (uint32_t)(var<uint8_t>(0x1D96A80) ^ 1);
			SetDrawEnvPrim(G(0x23C0500), env);
			InsertPrimAutoDepth(OT0() + 0x44, (void *)G(0x23C0500));
			GteReadScreenOffset(&ofx, &ofy);
			GteSetScreenOffset(rect[2] / 2, rect[3] / 2);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
			*(int16_t *)(h + 0x14) = 0;
			*(int16_t *)(h + 0x16) = 0;
			*(int16_t *)(h + 0x18) = 0x140;
			*(int16_t *)(h + 0x1A) = 0xD8;
			*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
			*(int32_t *)(h + 0x10) = -1;
			*(int32_t *)(h + 0x20) = -1;
			SetProjectionH(0x7FFF);
			for (uint32_t g = 0x1D989BD; g >= 0x1D98991; g -= 0x2C)
			{
				const uint8_t fl = *(const uint8_t *)g;
				if (!(fl & 1) || !(fl & 2)) continue;
				*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(g + 0x27);
				*(uint16_t *)(h + 0x24) = *(const uint16_t *)(g + 1);
				BuildBoneMatricesFromPose((void *)(g + 0x13));
				ComputeBonesWorldMatrices((void *)(g + 0x13), &m);
				PacketCursor() = RenderGeometry(g + 3, h, OT0() + 0x44, 0x10, PacketCursor());
			}
			SetProjectionH(var<int16_t>(0x1D8E038));
			FieldFree(0x2C);
			GteSetScreenOffset(ofx, ofy);
			InsertPrimAltViewport(OT0() + 0x44, X(ALT_VIEWPORT));
			SetDrawAreaPrim(G(0x23BF040), rect);
			InsertPrimAutoDepth(OT0() + 0x44, (void *)G(0x23BF040));
			var<int16_t>(G(0x23C326C)) = rect[0];
			var<int16_t>(G(0x23C326E)) = rect[1];
			SetDrawOffsetPrim(G(0x23C04D0), (const void *)G(0x23C326C));
			InsertPrimAutoDepth(OT0() + 0x44, (void *)G(0x23C04D0));
		}

		// ------------------------------------------------------------------
		// Single prim models in the frame: glow (0x5E5980), spin (0x5E5AB0), ring (0x5E5C30)
		// ------------------------------------------------------------------
		static uint8_t *PrimHeader(uint32_t model, int size)
		{
			uint8_t *h = AllocHeader(size);
			*(uint32_t *)h = model;
			*(uint32_t *)(h + 8) = 0;
			return h;
		}
		static void DrawPrim(uint8_t *h, int32_t mode, int size)
		{
			PacketCursor() = RenderPrimModel(h, OT0() + 0x44, mode, PacketCursor());
			FieldFree(size);
		}

		static void GlowDraw(const Node24 *n, const Mat4x3 *frame, const PF &f)
		{
			Mat4x3 m = *frame;
			int32_t s[3] = { n->w1C, n->w1C, n->w1C };
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			uint8_t *h = PrimHeader(fxp::model(P().models.glow, X(MODEL_GLOW)), 0x58);
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, D::RMODE, 0x58);
		}
		static uint32_t __cdecl GlowTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Glow, n);)
			FX_PART(PART_GLOW);
			GlowDraw(n, Frame(), GlowPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t c = n->c;
			if (c < 8)
			{
				const int16_t r = n->w1E;
				n->w1C = (int16_t)(n->w1C - r);
				n->w1E = (int16_t)(r - r / 6);
			}
			n->c = (int16_t)(c + 1);
			return EndAt(n->c, 0x14);
		}

		static void SpinDraw(const Node24 *n, const Mat4x3 *frame, const PF &f)
		{
			int16_t ang[4] = { 0, 0, n->w18, 0 };
			Mat4x3 m;
			ComposeZYXRotationMatrix(ang, &m);
			GteMatrixMultiply(frame, &m);
			m.t[2] = frame->t[2];
			m.t[1] = frame->t[1];
			m.t[0] = frame->t[0];
			int32_t s[3] = { n->w1C, n->w1C, n->w1C };
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			uint8_t *h = PrimHeader(fxp::model(P().models.spin, X(MODEL_SPIN)), 0x58);
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, D::RMODE, 0x58);
		}
		static uint32_t __cdecl SpinTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Spin, n);)
			FX_PART(PART_SPINS);
			SpinDraw(n, Frame(), SpinPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t r = n->w1A;
			n->w18 = (int16_t)(n->w18 + r);
			n->w1A = (int16_t)(r - r / 8);
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0x1A);
		}

		static void RingDraw(const Node24 *n, const Mat4x3 *frame, const PF &f)
		{
			Mat4x3 m = *frame;
			int32_t s[3] = { n->w1C, n->w1C, n->w1C };
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			uint8_t *h = PrimHeader(fxp::model(P().models.ring, X(MODEL_RING)), 0x58);
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, D::RMODE, 0x58);
		}
		static uint32_t __cdecl RingTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Ring, n);)
			FX_PART(PART_RINGS);
			RingDraw(n, Frame(), RingPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t r = n->w1E;
			n->w1C = (int16_t)(n->w1C + r);
			n->w1E = (int16_t)(r - r / 6);
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0xC);
		}

		// ------------------------------------------------------------------
		// Bolt director (0x5E5D50): the bolt's record and its three drawing tasks
		// ------------------------------------------------------------------
		static uint32_t __cdecl BoltDirTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			const uint32_t rec = Bolt(n->e);
			if (DrawOnly()) return 0;
			if (n->c == 0)
			{
				const uint32_t c01 = *(const uint32_t *)Centre(), c23 = *(const uint32_t *)(Centre() + 2);
				At<uint32_t>(rec, 8) = c01;
				At<uint32_t>(rec, 0xC) = c23;
				At<uint32_t>(rec, 0) = c01;
				At<uint32_t>(rec, 4) = c23;
				At<int16_t>(rec, 0) = (int16_t)(At<int16_t>(rec, 0) + (int16_t)(CrtRand() % P().bolts.spread - P().bolts.spread / 2));
				At<int16_t>(rec, 4) = (int16_t)(At<int16_t>(rec, 4) + (int16_t)(CrtRand() % P().bolts.spread - P().bolts.spread / 2));
				At<int16_t>(rec, 8) = (int16_t)(At<int16_t>(rec, 8) + (int16_t)(CrtRand() % P().bolts.spread - P().bolts.spread / 2));
				At<int16_t>(rec, 0xC) = (int16_t)(At<int16_t>(rec, 0xC) + (int16_t)(CrtRand() % P().bolts.spread - P().bolts.spread / 2));
				const int32_t dx = At<int16_t>(rec, 8) - At<int16_t>(rec, 0);
				const int32_t dz = At<int16_t>(rec, 0xC) - At<int16_t>(rec, 4);
				const int32_t len = Sqrt(mul32(dz, dz) + mul32(dx, dx));
				At<uint32_t>(rec, 0x10) = At<uint32_t>(rec, 0);
				At<uint32_t>(rec, 0x14) = At<uint32_t>(rec, 4);
				At<uint32_t>(rec, 0x30) = At<uint32_t>(rec, 8);
				At<uint32_t>(rec, 0x34) = At<uint32_t>(rec, 0xC);
				At<int16_t>(rec, 0x2A) = 0;
				At<int16_t>(rec, 0x22) = 0;
				At<int16_t>(rec, 0x1A) = 0;
				At<int16_t>(rec, 0x20) = (int16_t)((At<int16_t>(rec, 0) + At<int16_t>(rec, 8)) >> 1);
				At<int16_t>(rec, 0x24) = (int16_t)((At<int16_t>(rec, 4) + At<int16_t>(rec, 0xC)) >> 1);
				At<int16_t>(rec, 0x18) = (int16_t)((At<int16_t>(rec, 0) + At<int16_t>(rec, 0x20)) >> 1);
				At<int16_t>(rec, 0x1C) = (int16_t)((At<int16_t>(rec, 4) + At<int16_t>(rec, 0x24)) >> 1);
				At<int16_t>(rec, 0x28) = (int16_t)((At<int16_t>(rec, 0x20) + At<int16_t>(rec, 8)) >> 1);
				At<int16_t>(rec, 0x2C) = (int16_t)((At<int16_t>(rec, 0xC) + At<int16_t>(rec, 0x24)) >> 1);
				const int32_t two = len + len;
				static const int offs[6] = { 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C };
				for (int k = 0; k < 6; k++)
					At<int16_t>(rec, offs[k]) = (int16_t)(At<int16_t>(rec, offs[k]) + (int16_t)(CrtRand() % two - len));
				void *work = FieldAlloc(0x190);
				SplineSetup(5, (const void *)(rec + 0x10), work);
				for (int32_t i = 0; i < 0x30; i++)
					SplineEval(5, work, (void *)(rec + 0x38 + 8 * i), shl32(i, 12) / 0x2F);
				FieldFree(0x190);
				Node24 *t = AddMain(ORIG_Arc);
				t->c = 0;
				t->e = n->e;
				t->w18 = 0;
				t->w1A = 2;
				t->w1C = (int16_t)(CrtRand() % 0x200 + 0x200);
				t = AddMain(ORIG_Sheet);
				t->c = 0;
				t->e = n->e;
				t->w18 = 0;
				t->w1A = 2;
				for (int k = 0; k < 3; k++) At<uint32_t>(rec, 0x1B8 + 0x14 * k) = 0;
				t = AddMain(ORIG_Ribbon);
				t->w1A = 2;
				t->e = n->e;
				t->c = 0;
				t->w18 = 0;
			}
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0x10);
		}

		// Arc (0x5E6030): model 0xD719E4 from the effect point to spline point `target`, through the module's renderer
		static void ArcDraw(const Node24 *n, const int16_t *point, const int16_t *target, const PF &f)
		{
			int32_t v[3], axis[4];
			v[0] = target[0] - point[0];
			v[1] = target[1] - point[1];
			v[2] = target[2] - point[2];
			const int32_t len = Sqrt(Normalize(v, v));
			const int32_t angle = RotationBetweenVectors((const int32_t *)X(REF_DIR), v, axis);
			Mat4x3 r, m;
			BuildAxisAngleRotationMatrix(angle, &r, axis);
			int16_t ang[4] = { 0, 0, 0, 0 };
			ComposeZYXRotationMatrix(ang, &m);
			m.t[1] = point[1];
			m.t[0] = point[0];
			int32_t s[3];
			s[1] = n->w1C;
			s[0] = n->w1C;
			const int32_t q = (int32_t)(((int64_t)shl32(len, 12) * 0x61FF6D01) >> 32) >> 11;
			m.t[2] = point[2];
			s[2] = q + (int32_t)((uint32_t)q >> 31);
			Scale3DMatrix(&m, s);
			GteMatrixMultiply(&r, &m);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			RenderHeader *h = (RenderHeader *)AllocHeader(0x6C);
			h->model = fxp::model(P().models.arc, X(MODEL_ARC));
			*(uint32_t *)h->far_rgb = 0;
			h->colour = 0x30307F;
			ApplyPF((uint8_t *)h, f, 0x1C);
			PacketCursor() = rm::RenderModel(h, OT0() + 0x44, 2, PacketCursor());
			FieldFree(0x6C);
		}
		static uint32_t __cdecl ArcTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Arc, n);)
			FX_PART(PART_BOLTS);
			ArcDraw(n, Point(), (const int16_t *)(Bolt(n->e) + 0x38 + 8 * n->w18), ArcPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t s = n->w1A;
			n->w18 = (int16_t)(n->w18 + s);
			if (s > 2) n->w1A = (int16_t)(s - 1);
			n->c = (int16_t)(n->c + 1);
			return n->w18 >= 0x30 ? TASK_END : 0;
		}

		// Sheet (0x5E7AE0): history entry (counter % 3) = (effect point, spline point +0x18) [done every tick], then the
		// valid entries newest first (at most 3, fewer from counter 0x15) into +0x1F4 (points) / +0x214 (spline points);
		// with 2 or more: 16-point splines through each (+0x234 / +0x2B4) and 14 POLY_G4 pairs between them
		static void SheetHistory(const Node24 *n, uint32_t rec, const int16_t *point)
		{
			const int32_t j = n->c % 3;
			At<uint32_t>(rec, 0x1B8 + 0x14 * j) = 1;
			At<uint32_t>(rec, 0x1BC + 0x14 * j) = *(const uint32_t *)point;
			At<uint32_t>(rec, 0x1C0 + 0x14 * j) = *(const uint32_t *)(point + 2);
			const uint32_t sp = rec + 0x38 + 8 * n->w18;
			At<uint32_t>(rec, 0x1C4 + 0x14 * j) = At<uint32_t>(sp, 0);
			At<uint32_t>(rec, 0x1C8 + 0x14 * j) = At<uint32_t>(sp, 4);
		}
		static int32_t SheetGather(const Node24 *n, uint32_t rec)
		{
			int32_t j = n->c % 3;
			const int32_t lim = n->c >= 0x15 ? 0x18 - n->c : 3;
			int32_t k = 0;
			for (; k < lim; k++)
			{
				if (At<uint32_t>(rec, 0x1B8 + 0x14 * j) == 0) break;
				At<uint32_t>(rec, 0x1F4 + 8 * k) = At<uint32_t>(rec, 0x1BC + 0x14 * j);
				At<uint32_t>(rec, 0x1F8 + 8 * k) = At<uint32_t>(rec, 0x1C0 + 0x14 * j);
				At<uint32_t>(rec, 0x214 + 8 * k) = At<uint32_t>(rec, 0x1C4 + 0x14 * j);
				At<uint32_t>(rec, 0x218 + 8 * k) = At<uint32_t>(rec, 0x1C8 + 0x14 * j);
				if (--j < 0) j = 2;
			}
			return k;
		}
		static void SheetCurves(uint32_t rec, int32_t k)
		{
			void *work = FieldAlloc(0x190);
			SplineSetup(k, (const void *)(rec + 0x1F4), work);
			for (int32_t i = 0; i < 0x10; i++) SplineEval(k, work, (void *)(rec + 0x234 + 8 * i), shl32(i, 12) / 15);
			SplineSetup(k, (const void *)(rec + 0x214), work);
			for (int32_t i = 0; i < 0x10; i++) SplineEval(k, work, (void *)(rec + 0x2B4 + 8 * i), shl32(i, 12) / 15);
			FieldFree(0x190);
		}
		static uint32_t SheetColour(int32_t c) { return *(const uint32_t *)(fxp::data(P().tables.sheet_colours, X(SHEET_COLOURS), 48, 4) + 4 * (c % 12)); }
		static void SheetDraw(uint32_t colour, uint32_t rec, uint8_t *blk)
		{
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			GteSetFarColor2(0, 0, 0);
			uint32_t a = PacketCursor() + 4, b = PacketCursor() + 0x24; // edi = packet A + 4, esi = packet B
			const uint32_t col = colour | 0x3A000000u;
			At<uint32_t>((uint32_t)blk, 0x14) = col;
			At<uint32_t>((uint32_t)blk, 0x1C) = col;
			uint32_t v = rec + 0x23C;
			for (int32_t t = 0; t < 0xE000; t += 0x1000, v += 8)
			{
				At<uint32_t>(a, -4) = 0x8000000;
				At<uint32_t>((uint32_t)blk, 0x10) = At<uint32_t>((uint32_t)blk, 0x14);
				GteLoadV012((const void *)(v - 8), (const void *)v, (const void *)(v + 0x78));
				GteRTPT();
				GteReadFLAG(blk + 0xC);
				if (At<uint32_t>((uint32_t)blk, 0xC) & 0x60000) continue;
				GteReadSXY012Split((void *)(a + 4), (void *)(a + 0xC), (void *)(a + 0x14));
				GteLoadV0((const void *)(v + 0x80));
				GteRTPS();
				GteReadSXY2((void *)(a + 0x1C));
				GteAVSZ4();
				GteStoreOTZ(blk + 8);
				GteSetIR0(t / 14);
				GteLoadRGBC(blk + 0x1C);
				GteDPCS();
				GteStoreRGB2(blk + 0x14);
				const uint32_t c0 = At<uint32_t>((uint32_t)blk, 0x10);
				At<uint32_t>(a, 0x10) = c0;
				At<uint32_t>(a, 0) = c0;
				const uint32_t c1 = At<uint32_t>((uint32_t)blk, 0x14);
				At<uint32_t>(a, 0x18) = c1;
				At<uint32_t>(a, 8) = c1;
				InsertPrimAutoDepth(OT0() + (uint32_t)(At<int32_t>((uint32_t)blk, 8) >> 2) * 4 + 0x44, (void *)(a - 4));
				At<uint32_t>(b, 0xC) = 0x3A000000;
				At<uint32_t>(b, 4) = 0x3A000000;
				At<uint32_t>(b, 0x1C) = At<uint32_t>((uint32_t)blk, 0x14);
				At<uint32_t>(b, 0x14) = At<uint32_t>((uint32_t)blk, 0x10);
				At<uint32_t>(b, 0x10) = At<uint32_t>(a, 0xC);
				At<uint32_t>(b, 8) = At<uint32_t>(a, 4);
				At<uint32_t>(b, 0x18) = At<uint32_t>(a, 0x14);
				At<uint32_t>(b, 0x20) = At<uint32_t>(a, 0x1C);
				At<uint32_t>(b, 0) = 0x8000000;
				InsertPrimAutoDepth(OT0() + (uint32_t)(At<int32_t>((uint32_t)blk, 8) >> 2) * 4 + 0x44, (void *)b);
				a += 0x48;
				b += 0x48;
			}
			PacketCursor() = b;
		}
		static uint32_t __cdecl SheetTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			const uint32_t rec = Bolt(n->e);
			uint8_t *blk = (uint8_t *)FieldAlloc(0x40);
			SheetHistory(n, rec, Point());
			const int32_t k = SheetGather(n, rec);
			if (k > 1)
			{
				SheetCurves(rec, k);
				// 30 fps layer: see fx_megiddo_held.inc
				FX_HELD(MegiddoHeld<D>::note_sheet(n, rec, k);)
				FX_PART(PART_BOLTS);
				SheetDraw(SheetColour(n->c), rec, blk);
				FX_PART(0);
			}
			FieldFree(0x40);
			if (DrawOnly()) return 0;
			const int16_t s = n->w1A;
			n->w18 = (int16_t)(n->w18 + s);
			if (s > 2) n->w1A = (int16_t)(s - 1);
			n->c = (int16_t)(n->c + 1);
			return n->w18 >= 0x30 ? TASK_END : 0;
		}

		// Ribbon (0x5E7ED0): points 0..cnt-1 of the spline widened by the offset (0x5A, 0, 0) turned from 0xD78050 to the
		// horizontal direction from the previous point (the first: to the next), table 0x23C2DC8 (24 bytes: point,
		// point + offset, point - offset); cnt - 1 POLY_G4 pairs
		static void RibbonTable(uint32_t rec, uint8_t *blk, int32_t cnt)
		{
			const uint32_t w = (uint32_t)blk;
			memcpy(blk + 0x40, (const void *)X(REF_DIR), 0x10);
			At<int16_t>(w, 0x30) = 0x5A;
			At<int16_t>(w, 0x32) = 0;
			At<int16_t>(w, 0x34) = 0;
			for (int32_t j = 0; j < cnt; j++)
			{
				if (j != 0)
				{
					At<uint32_t>(w, 0x20) = At<uint32_t>(rec, 0x38 + 8 * j);
					At<uint32_t>(w, 0x28) = At<uint32_t>(rec, 0x30 + 8 * j);
					At<uint32_t>(w, 0x24) = At<uint32_t>(rec, 0x3C + 8 * j);
					At<uint32_t>(w, 0x2C) = At<uint32_t>(rec, 0x34 + 8 * j);
					At<int32_t>(w, 0x50) = At<int16_t>(w, 0x20) - At<int16_t>(w, 0x28);
					At<int32_t>(w, 0x58) = At<int16_t>(w, 0x24) - At<int16_t>(w, 0x2C);
				}
				else
				{
					At<uint32_t>(w, 0x20) = At<uint32_t>(rec, 0x38);
					At<uint32_t>(w, 0x28) = At<uint32_t>(rec, 0x40);
					At<uint32_t>(w, 0x24) = At<uint32_t>(rec, 0x3C);
					At<uint32_t>(w, 0x2C) = At<uint32_t>(rec, 0x44);
					At<int32_t>(w, 0x50) = At<int16_t>(w, 0x28) - At<int16_t>(w, 0x20);
					At<int32_t>(w, 0x58) = At<int16_t>(w, 0x2C) - At<int16_t>(w, 0x24);
				}
				At<int32_t>(w, 0x54) = 0;
				Normalize((const int32_t *)(w + 0x50), (int32_t *)(w + 0x50));
				const int32_t angle = RotationBetweenVectors((const int32_t *)(w + 0x40), (const int32_t *)(w + 0x50), (int32_t *)(w + 0x60));
				BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(w + 0x70), (const int32_t *)(w + 0x60));
				MatVec((const Mat4x3 *)(w + 0x70), (const void *)(w + 0x30), (void *)(w + 0x38));
				const uint32_t t = G(RIBBON_TABLE) + 0x18 * j;
				At<uint32_t>(t, 0) = At<uint32_t>(w, 0x20);
				At<uint32_t>(t, 4) = At<uint32_t>(w, 0x24);
				At<int16_t>(t, 8) = (int16_t)(At<int16_t>(w, 0x38) + At<int16_t>(w, 0x20));
				At<int16_t>(t, 0x10) = (int16_t)(At<int16_t>(w, 0x20) - At<int16_t>(w, 0x38));
				At<int16_t>(t, 0xC) = (int16_t)(At<int16_t>(w, 0x3C) + At<int16_t>(w, 0x24));
				At<int16_t>(t, 0x14) = (int16_t)(At<int16_t>(w, 0x24) - At<int16_t>(w, 0x3C));
				At<int16_t>(t, 0x12) = 0;
				At<int16_t>(t, 0xA) = 0;
			}
		}
		static void RibbonDraw(bool fade, int32_t ir0, uint8_t *blk, int32_t cnt)
		{
			const uint32_t w = (uint32_t)blk;
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			GteSetFarColor2(0, 0, 0);
			uint32_t pa = PacketCursor();       // packet A
			uint32_t pb = PacketCursor() + 0x24; // packet B
			At<uint32_t>(w, 0x10) = 0x3A202080;
			At<uint32_t>(w, 0x14) = 0x3A040430;
			At<uint32_t>(w, 0x18) = 0x3A04040E;
			At<uint32_t>(w, 0x1C) = 0x3A000000;
			if (fade)
			{
				GteLoadRGB012(blk + 0x10, blk + 0x14, blk + 0x18);
				GteSetIR0(ir0);
				GteDPCT();
				GteStoreRGB012(blk + 0x10, blk + 0x14, blk + 0x18);
				At<uint32_t>(w, 0x10) |= 0x3A000000;
				At<uint32_t>(w, 0x14) |= 0x3A000000;
			}
			const int32_t segs = cnt - 1;
			const uint32_t T = G(RIBBON_TABLE);
			for (int32_t k = 0; k < segs; k++)
			{
				const uint32_t tk = T + 0x18 * k;
				const uint32_t a10 = pa + 0x10;
				At<uint32_t>(pb, 0) = 0x8000000;
				At<uint32_t>(pa, 0) = 0x8000000;
				GteLoadV012((const void *)(tk + 0x18), (const void *)(tk + 8), (const void *)(tk + 0x10));
				GteRTPT();
				GteReadFLAG(blk + 0xC);
				if (At<uint32_t>(w, 0xC) & 0x60000) continue;
				GteReadSXY012Split((void *)a10, (void *)(a10 + 8), (void *)(pb + 0x20));
				At<uint32_t>(pb, 8) = At<uint32_t>(a10, 0);
				GteAVSZ3();
				GteStoreOTZ(blk + 8);
				GteLoadV012((const void *)tk, (const void *)(tk + 0x20), (const void *)(tk + 0x28));
				GteRTPT();
				GteReadSXY012Split((void *)(a10 + 0x10), (void *)(a10 - 8), (void *)(pb + 0x10));
				At<uint32_t>(pb, 0x18) = At<uint32_t>(a10, 0x10);
				if (k == 0)
				{
					At<uint32_t>(pb, 0xC) = At<uint32_t>(w, 0x14); At<uint32_t>(a10, -0xC) = At<uint32_t>(w, 0x14);
					At<uint32_t>(pb, 0x1C) = At<uint32_t>(w, 0x1C); At<uint32_t>(a10, 4) = At<uint32_t>(w, 0x1C);
					At<uint32_t>(pb, 4) = At<uint32_t>(w, 0x10); At<uint32_t>(a10, -4) = At<uint32_t>(w, 0x10);
					At<uint32_t>(pb, 0x14) = At<uint32_t>(w, 0x18); At<uint32_t>(a10, 0xC) = At<uint32_t>(w, 0x18);
				}
				else if (k == segs - 1)
				{
					At<uint32_t>(pb, 0xC) = At<uint32_t>(w, 0x1C); At<uint32_t>(a10, -0xC) = At<uint32_t>(w, 0x1C);
					At<uint32_t>(pb, 0x1C) = At<uint32_t>(w, 0x14); At<uint32_t>(a10, 4) = At<uint32_t>(w, 0x14);
					At<uint32_t>(pb, 4) = At<uint32_t>(w, 0x18); At<uint32_t>(a10, -4) = At<uint32_t>(w, 0x18);
					At<uint32_t>(pb, 0x14) = At<uint32_t>(w, 0x10); At<uint32_t>(a10, 0xC) = At<uint32_t>(w, 0x10);
				}
				else
				{
					const uint32_t c14 = At<uint32_t>(w, 0x14), c10 = At<uint32_t>(w, 0x10);
					At<uint32_t>(a10, 4) = c14;
					At<uint32_t>(a10, -0xC) = c14;
					At<uint32_t>(a10, 0xC) = c10;
					At<uint32_t>(a10, -4) = c10;
					At<uint32_t>(pb, 0x14) = c10;
					At<uint32_t>(pb, 4) = c10;
					At<uint32_t>(pb, 0x1C) = c14;
					At<uint32_t>(pb, 0xC) = c14;
				}
				InsertPrimAutoDepth(OT0() + (uint32_t)(At<int32_t>(w, 8) >> 2) * 4 + 0x44, (void *)pa);
				InsertPrimAutoDepth(OT0() + (uint32_t)(At<int32_t>(w, 8) >> 2) * 4 + 0x44, (void *)pb);
				pa += 0x48;
				pb += 0x48;
			}
			PacketCursor() = pb;
		}
		static int32_t RibbonFade(int32_t c) { return mul32(c - 0x18, 0x155); }
		static uint32_t __cdecl RibbonTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			const uint32_t rec = Bolt(n->e);
			uint8_t *blk = (uint8_t *)FieldAlloc(0x90);
			const int32_t cnt = n->w18 + 1;
			if (cnt >= 2)
			{
				// 30 fps layer: see fx_megiddo_held.inc
				FX_HELD(MegiddoHeld<D>::note_node(ORIG_Ribbon, n);)
				RibbonTable(rec, blk, cnt);
				FX_PART(PART_BOLTS);
				RibbonDraw(n->c >= 0x18, RibbonFade(n->c), blk, cnt);
				FX_PART(0);
			}
			FieldFree(0x90);
			if (DrawOnly()) return 0;
			const int16_t s = n->w1A;
			n->w18 = (int16_t)(n->w18 + s);
			if (n->w18 >= 0x30) n->w18 = 0x2F;
			if (s > 2) n->w1A = (int16_t)(s - 1);
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0x24);
		}

		// ------------------------------------------------------------------
		// Models at the centre: column (0x5E8330), column flash (0x5E8450), pillar (0x5E8590), flames (0x5E86D0),
		// ring 2 (0x5E9250)
		// ------------------------------------------------------------------
		static void CentreMatrix(const Node24 *n, int16_t ax, int16_t ay, int32_t sx, int32_t sy, int32_t sz)
		{
			int16_t ang[4] = { ax, ay, 0, 0 };
			Mat4x3 m;
			ComposeZYXRotationMatrix(ang, &m);
			m.t[1] = n->w12;
			m.t[0] = n->w10;
			int32_t s[3];
			s[2] = sz;
			s[0] = sx;
			m.t[2] = n->w14;
			s[1] = sy;
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
		}
		static inline uint32_t StepScale(Node24 *n, int16_t limit)
		{
			const int16_t c = n->c;
			if (c >= 0 && c < 8) n->w1C = (int16_t)(n->w1C + n->w1E);
			n->c = (int16_t)(c + 1);
			return EndAt(n->c, limit);
		}

		static void ColumnDraw(const Node24 *n, int16_t sy)
		{
			CentreMatrix(n, 0, 0, 0x1000, sy, 0x1000);
			uint8_t *h = AllocHeader(0x60);
			*(uint32_t *)h = fxp::model(P().models.column, X(MODEL_COLUMN));
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x1C) = 0;
			*(uint32_t *)(h + 0x58) = 0xE1000020;
			*(uint32_t *)(h + 0x5C) = 0xE1000220;
			PacketCursor() = ColumnRender(h, OT0() + 0x44, 2, PacketCursor());
			FieldFree(0x60);
		}
		static uint32_t __cdecl ColumnTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Column, n);)
			FX_PART(PART_COLUMN);
			ColumnDraw(n, n->w1C);
			FX_PART(0);
			if (DrawOnly()) return 0;
			return StepScale(n, 8);
		}

		static void ColumnFlashDraw(const Node24 *n, int16_t sy, const PF &f)
		{
			CentreMatrix(n, 0, 0, 0x1000, sy, 0x1000);
			uint8_t *h = AllocHeader(0x58);
			*(uint32_t *)h = fxp::model(P().models.column_flash, X(MODEL_COLUMN_FLASH));
			*(uint32_t *)(h + 8) = 0;
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, 2, 0x58);
		}
		static uint32_t __cdecl ColumnFlashTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_ColumnFlash, n);)
			FX_PART(PART_COLUMN);
			ColumnFlashDraw(n, n->w1C, ColumnFlashPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			return StepScale(n, 8);
		}

		static void PillarDraw(const Node24 *n, int16_t sx, int16_t sy, const PF &f)
		{
			CentreMatrix(n, 0, 0, sx, sy, sx);
			uint8_t *h = PrimHeader(fxp::model(P().models.pillar, X(MODEL_PILLAR)), 0x58);
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, 2, 0x58);
		}
		static uint32_t __cdecl PillarTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Pillar, n);)
			FX_PART(PART_PILLAR);
			PillarDraw(n, n->w1C, n->w20, PillarPF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t r = n->w22;
			n->w20 = (int16_t)(n->w20 + r);
			n->w22 = (int16_t)(r - r / 5);
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0x14);
		}

		static void FlameDraw(const Node24 *n, int32_t scroll, const PF &f)
		{
			CentreMatrix(n, 0, n->w18, n->w1C, n->w20, n->w1C);
			uint8_t *h = AllocHeader(0x7C);
			*(int32_t *)(h + 0x14) = scroll;
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x20) = 0x33;
			*(int16_t *)(h + 0x1A) = 0;
			*(int16_t *)(h + 0x1C) = 0x40;
			*(int16_t *)(h + 0x1E) = 0x80;
			*(uint32_t *)(h + 0x10) = 0;
			if (n->w1A != 0) { *(uint32_t *)h = fxp::model(P().models.flame_a, X(MODEL_FLAME_A)); *(int16_t *)(h + 0x18) = 0x40; }
			else { *(uint32_t *)h = fxp::model(P().models.flame_b, X(MODEL_FLAME_B)); *(int16_t *)(h + 0x18) = 0; }
			ApplyPF(h, f, 0x20);
			PacketCursor() = Mag151Render(h, OT0() + 0x44, 2, PacketCursor());
			FieldFree(0x7C);
		}
		static uint32_t __cdecl FlameTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			if (n->e > 0)
			{
				if (DrawOnly()) return 0;
				n->e = (int16_t)(n->e - 1);
				return 0;
			}
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Flame, n);)
			FX_PART(PART_FLAMES);
			FlameDraw(n, mul32(n->w22, n->c), FlamePF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0x12);
		}

		static void Ring2Draw(const Node24 *n, int16_t s, const PF &f)
		{
			CentreMatrix(n, 0x400, 0, s, s, s);
			uint8_t *h = PrimHeader(fxp::model(P().models.ring, X(MODEL_RING)), 0x58);
			ApplyPF(h, f, 0x1C);
			DrawPrim(h, 2, 0x58);
		}
		static uint32_t __cdecl Ring2Task(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_node(ORIG_Ring2, n);)
			FX_PART(PART_LANDING);
			Ring2Draw(n, n->w1C, Ring2PF(n->c));
			FX_PART(0);
			if (DrawOnly()) return 0;
			const int16_t r = n->w1E;
			n->w1C = (int16_t)(n->w1C + r);
			n->w1E = (int16_t)(r - r / 5);
			n->c = (int16_t)(n->c + 1);
			return EndAt(n->c, 0xC);
		}

		// ------------------------------------------------------------------
		// Sparks (0x5E8870, pool A) and debris (0x5E8A80, pool B)
		// ------------------------------------------------------------------
		static int32_t FreeEntry(uint32_t pool, uint32_t end, uint32_t stride)
		{
			int32_t i = 0;
			for (uint32_t a = pool; *(const uint32_t *)a != 0; i++)
			{
				a += stride;
				if (a >= end) return -1;
			}
			return i >= 0x64 ? -1 : i;
		}

		static void SparksSpawn(const Node24 *n)
		{
			for (int k = 0; k < 2; k++)
			{
				const int32_t i = FreeEntry(G(POOL_A), G(POOL_A_END), 0x18);
				if (i < 0) return;
				const uint32_t e = G(POOL_A) + 0x18 * i;
				At<uint32_t>(e, 0) = 1;
				At<int16_t>(e, 4) = 0;
				const int32_t r = CrtRand() % 0xC00;
				At<int16_t>(e, 6) = (int16_t)(r + 0xA00);
				At<uint32_t>(e, 8) = *(const uint32_t *)&n->w10;
				At<uint32_t>(e, 0xC) = *(const uint32_t *)&n->w14;
				At<int16_t>(e, 8) = (int16_t)(At<int16_t>(e, 8) + (int16_t)(CrtRand() % 0x12C0 - 0x960));
				At<int16_t>(e, 0xA) = (int16_t)(At<int16_t>(e, 0xA) + (int16_t)(-0x190 - CrtRand() % 0x898));
				At<int16_t>(e, 0xC) = (int16_t)(At<int16_t>(e, 0xC) + (int16_t)(CrtRand() % 0x960 - 0x4B0));
			}
		}
		// the pool's sprites (sequence 0xD718AC) at their positions; `update`: the real tick's frame steps / frees
		static int32_t SparksDraw(uint8_t *blk, uint32_t pool, uint32_t pool_end, bool update)
		{
			uint8_t *h = AllocHeader(0xB4);
			*(int16_t *)(h + 0x24) = 0;
			int32_t live = 0;
			memcpy(blk + 8, &Camera(), 0x20);
			*(uint32_t *)h = fxp::sequence(P().seqs.spark, X(SEQ_SPARK));
			GteSetLightMatrix(blk + 8);
			GteSetBackColorFromTrans(blk + 8);
			for (uint32_t e = pool; e < pool_end; e += 0x18)
			{
				if (!(*(const uint8_t *)e & 1)) continue;
				GteLoadV0((const void *)(e + 8));
				GteMVMVA_LightV0Bk();
				GteSetRotScale(At<int16_t>(e, 6));
				*(int16_t *)(h + 4) = At<int16_t>(e, 4);
				GteTransFromMAC();
				PacketCursor() = InitEffectSequenceFromData(h, OT0() + 0x44, 2, PacketCursor());
				if (!update) continue;
				At<int16_t>(e, 4) = (int16_t)(At<int16_t>(e, 4) + 1);
				if (*(const int16_t *)(h + 0x28) < 0) At<uint32_t>(e, 0) = 0;
				else live++;
			}
			FieldFree(0xB4);
			return live;
		}
		static uint32_t __cdecl SparksTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			uint8_t *blk = (uint8_t *)FieldAlloc(0x48);
			if (!DrawOnly() && n->c <= 0x10) SparksSpawn(n);
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_pool(ORIG_Sparks, n);)
			FX_PART(PART_SPARKS);
			const int32_t live = SparksDraw(blk, G(POOL_A), G(POOL_A_END), !DrawOnly());
			FX_PART(0);
			FieldFree(0x48);
			if (DrawOnly()) return 0;
			n->c = (int16_t)(n->c + 1);
			return live != 0 ? 0 : TASK_END;
		}

		// one debris entry: model 0xD78090[kind] turned (x +0x16, z +0x1A), scaled +6, at +0xC
		static void DebrisEntry(uint8_t *hdr, uint8_t *blk, uint32_t e, int16_t ax, int16_t az, int16_t s, int16_t x, int16_t y, int16_t z)
		{
			const uint32_t w = (uint32_t)blk;
			At<int16_t>(w, 0) = ax;
			const int32_t kind = At<int32_t>(e, 8);
			*(uint32_t *)hdr = fxp::model((&P().models.debris0)[kind], *(const uint32_t *)(X(DEBRIS_MODELS) + 4 * kind));
			At<int16_t>(w, 4) = az;
			ComposeZYXRotationMatrix((const int16_t *)w, (Mat4x3 *)(w + 8));
			At<int32_t>(w, 0x1C) = x;
			At<int32_t>(w, 0x24) = z;
			At<int32_t>(w, 0x20) = y;
			At<int32_t>(w, 0x50) = s;
			At<int32_t>(w, 0x4C) = s;
			At<int32_t>(w, 0x48) = s;
			Scale3DMatrix((Mat4x3 *)(w + 8), (const int32_t *)(w + 0x48));
			ComposeAffineTransform((const Mat4x3 *)(w + 0x28), (const Mat4x3 *)(w + 8), (Mat4x3 *)(w + 8));
			GteSetRotMatrix((const Mat4x3 *)(w + 8));
			GteSetTransVector((const Mat4x3 *)(w + 8));
			FX_PART(PART_DEBRIS);
			PacketCursor() = RenderPrimModel(hdr, OT0() + 0x44, 2, PacketCursor());
			FX_PART(0);
		}
		static void DebrisSpawn(const Node24 *n)
		{
			for (int k = 0; k < 3; k++)
			{
				const int32_t i = FreeEntry(G(POOL_B), G(POOL_B_END), 0x1C);
				if (i < 0) return;
				const uint32_t e = G(POOL_B) + 0x1C * i;
				At<uint32_t>(e, 0) = 2;
				At<int16_t>(e, 4) = 0;
				At<int16_t>(e, 6) = (int16_t)(CrtRand() % 0x3C0 + 0x280);
				const int32_t kind = CrtRand() % 3;
				At<int32_t>(e, 8) = kind;
				At<uint32_t>(e, 0xC) = *(const uint32_t *)&n->w10;
				At<uint32_t>(e, 0x10) = *(const uint32_t *)&n->w14;
				At<int16_t>(e, 0xC) = (int16_t)(At<int16_t>(e, 0xC) + (int16_t)(CrtRand() % 0x12C0 - 0x960));
				At<int16_t>(e, 0xE) = (int16_t)-(CrtRand() % 0x12C);
				At<int16_t>(e, 0x10) = (int16_t)(At<int16_t>(e, 0x10) + (int16_t)(CrtRand() % 0x960 - 0x4B0));
				At<int16_t>(e, 0x12) = (int16_t)(-0xB9 - CrtRand() % 0x186);
				int32_t v = CrtRand() % 0x78 + 0x3C;
				At<int16_t>(e, 0x14) = (int16_t)v;
				if (v & 1) At<int16_t>(e, 0x14) = (int16_t)-v;
				v = CrtRand() % 0x3C + 0x1E;
				At<int16_t>(e, 0x18) = (int16_t)v;
				if (v & 1) At<int16_t>(e, 0x18) = (int16_t)-v;
				const int16_t a = (int16_t)(CrtRand() % 0x1000);
				At<int16_t>(e, 0x16) = a;
				At<int16_t>(e, 0x1A) = (int16_t)(a >> 1);
			}
		}
		static uint32_t __cdecl DebrisTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_pool(ORIG_Debris, n);)
			uint8_t *hdr = AllocHeader(0x58);
			uint8_t *blk = (uint8_t *)FieldAlloc(0x88);
			memcpy(blk + 0x28, &Camera(), 0x20);
			*(uint32_t *)(hdr + 8) = 0;
			*(uint32_t *)(hdr + 0x1C) = 0;
			int32_t live = 0;
			*(int16_t *)(blk + 2) = 0;
			for (uint32_t e = G(POOL_B); e < G(POOL_B_END); e += 0x1C)
			{
				if (!(*(const uint8_t *)e & 2)) continue;
				DebrisEntry(hdr, blk, e, At<int16_t>(e, 0x16), At<int16_t>(e, 0x1A), At<int16_t>(e, 6), At<int16_t>(e, 0xC), At<int16_t>(e, 0xE), At<int16_t>(e, 0x10));
				if (DrawOnly()) continue;
				const int16_t life = (int16_t)(At<int16_t>(e, 4) + 1);
				At<int16_t>(e, 4) = life;
				if (life >= 0x10) { At<uint32_t>(e, 0) = 0; continue; }
				if (life >= 0xC)
				{
					const int16_t s = At<int16_t>(e, 6);
					At<int16_t>(e, 6) = (int16_t)(s - (int16_t)(s >> 3));
				}
				At<int16_t>(e, 0xE) = (int16_t)(At<int16_t>(e, 0xE) + At<int16_t>(e, 0x12));
				const int16_t vy = At<int16_t>(e, 0x12);
				At<int16_t>(e, 0x12) = (int16_t)(vy - (int16_t)(vy >> 3));
				At<int16_t>(e, 0x16) = (int16_t)(At<int16_t>(e, 0x16) + At<int16_t>(e, 0x14));
				At<int16_t>(e, 0x1A) = (int16_t)(At<int16_t>(e, 0x1A) + At<int16_t>(e, 0x18));
				live++;
			}
			FieldFree(0x88);
			FieldFree(0x58);
			if (DrawOnly()) return 0;
			if (n->c >= 0 && n->c <= 5) DebrisSpawn(n);
			n->c = (int16_t)(n->c + 1);
			if (n->c >= 4 && live == 0) return TASK_END;
			return 0;
		}

		// ------------------------------------------------------------------
		// Knock-down (0x5E8DA0) and throw (0x5E8EF0): a party member moved
		// ------------------------------------------------------------------
		static void SaveEntity(uint8_t *ent, uint32_t sv)
		{
			At<uint16_t>(sv, 0) = (uint16_t)(*(const uint16_t *)ent & 0x1000);
			At<uint32_t>(sv, 0xC) = *(const uint32_t *)(ent + 0x1C);
			At<uint32_t>(sv, 0x10) = *(const uint32_t *)(ent + 0x20);
			At<uint32_t>(sv, 4) = *(const uint32_t *)(ent + 0xC);
			At<uint32_t>(sv, 8) = *(const uint32_t *)(ent + 0x10);
		}
		static void RestoreEntity(uint8_t *ent, uint32_t sv)
		{
			*(uint16_t *)ent = (uint16_t)(At<uint16_t>(sv, 0) | (*(const uint16_t *)ent & 0xEFFF));
			*(uint32_t *)(ent + 0x1C) = At<uint32_t>(sv, 0xC);
			*(uint32_t *)(ent + 0x20) = At<uint32_t>(sv, 0x10);
			*(uint32_t *)(ent + 0xC) = At<uint32_t>(sv, 4);
			*(uint32_t *)(ent + 0x10) = At<uint32_t>(sv, 8);
		}

		static uint32_t __cdecl KnockTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			const int16_t c = n->c;
			const int32_t slot = n->e;
			uint8_t *ent = Entity(slot);
			const uint32_t sv = Save(slot);
			if (c == 0) SaveEntity(ent, sv);
			if (c < 8)
			{
				const int32_t v = mul32(n->w20, c) / 8;
				ent[1] |= 0x10;
				*(int16_t *)(ent + 0x1E) = (int16_t)(At<int16_t>(sv, 0xE) - (int16_t)v);
			}
			if (DrawOnly()) return 0;
			n->c = (int16_t)(c + 1);
			if (n->c < 9) return 0;
			RestoreEntity(ent, sv);
			return TASK_END;
		}

		static uint32_t __cdecl ThrowTask(TaskNode *tn)
		{
			Node24 *n = (Node24 *)tn;
			const int32_t slot = n->e;
			uint8_t *ent = Entity(slot);
			const uint32_t sv = Save(slot);
			if (n->c == 0)
			{
				GetDefaultEffectPosition(ent, &n->w10);
				SaveEntity(ent, sv);
			}
			const int16_t c = n->c;
			if (c <= 0x14)
			{
				const int32_t a = shl32(c, 11) / 20;
				ent[1] |= 0x10;
				const int32_t lift = mul32(ComputeSin(a), n->w20);
				*(int16_t *)(ent + 0xC) = (int16_t)(mul32(n->w18, n->c) / 20);
				*(int16_t *)(ent + 0xE) = (int16_t)(shl32(n->c, 13) / 20);
				int16_t d[4];
				d[0] = (int16_t)(n->w10 - At<int16_t>(sv, 0xC));
				d[1] = (int16_t)(n->w12 - At<int16_t>(sv, 0xE));
				d[2] = (int16_t)(n->w14 - At<int16_t>(sv, 0x10));
				Mat4x3 m;
				ComposeZYXRotationMatrix((const int16_t *)(ent + 0xC), &m);
				MatVec(&m, d, d);
				const int16_t x = (int16_t)(n->w10 - d[0]);
				int16_t y = (int16_t)(n->w12 - d[1]);
				const int16_t z = (int16_t)(n->w14 - d[2]);
				y = (int16_t)(y + (int16_t)(lift >> 12));
				*(int16_t *)(ent + 0x1C) = x;
				*(int16_t *)(ent + 0x1E) = y;
				*(int16_t *)(ent + 0x20) = z;
			}
			else if (c <= 0x1C)
			{
				const int32_t a = shl32(c - 0x14, 11) / 8;
				ent[1] |= 0x10;
				*(int16_t *)(ent + 0x1E) = (int16_t)(mul32(ComputeSin(a), n->w20 >> 3) >> 12);
			}
			if (DrawOnly()) return 0;
			if (n->c == 0x14)
			{
				Node24 *t = AddMain(ORIG_Ring2);
				t->c = 0;
				t->w10 = *(const int16_t *)(ent + 0x1C);
				t->w12 = 0;
				t->w14 = *(const int16_t *)(ent + 0x20);
				const int32_t r = (CrtRand() % 0x400 + 0xC00) / 4;
				t->w1E = (int16_t)r;
				t->w1C = (int16_t)r;
				if (n->w1A >= 0) QueueChainTransformation(ent, 6);
			}
			n->c = (int16_t)(n->c + 1);
			if (n->c < 0x1C) return 0;
			const int16_t idx = n->w1A;
			if (idx >= 0 && **(const uint8_t **)(ent + 0x74) == 6) return 0;
			RestoreEntity(ent, sv);
			if (idx >= 0) ApplyActionResultToTarget(Context()->actions->targets + idx * TARGET_STRIDE);
			return TASK_END;
		}

		// ------------------------------------------------------------------
		// File loader (0x5E4E70) and setup (0x5E4E90) (166 addresses): the game runs its own; Fujin's programs these (its
		// memory may be a copy's): the texture, the magic buffer, the cast, its first target's and its caster's slots, the
		// root and the director, pools A / B freed, the camera and the TIM
		// ------------------------------------------------------------------
		static uint32_t __cdecl FileLoader()
		{
			TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, X(TEXTURE_NAME)));
			return TexFile();
		}

		static TaskQueue *__cdecl Setup(CastContext *ctx)
		{
			TexBase() = fn<uint32_t (__cdecl *)()>(0x571B50)();   // Magic_TextureOFF_ToEAX1
			Context() = ctx;
			TargetSlot() = ctx->actions[0].targets[0];
			CasterSlot() = ctx->attacker;
			InitTaskQueuePool(QRoot(), (void *)G(0x23BF050), 0x10, 2);
			*(uint16_t *)((uint8_t *)AddTaskToQueue(QRoot(), own::Fn(C(ORIG_Root))) + 0xC) = 0;
			InitTaskQueuePool(QMain(), (void *)G(0x23C0550), 0x24, 100);
			*(uint16_t *)((uint8_t *)AddTaskToQueue(QMain(), own::Fn(C(ORIG_Director))) + 0xC) = 0;
			for (uint32_t a = G(POOL_A); a < G(POOL_A_END); a += 0x18) *(uint32_t *)a = 0;
			for (uint32_t a = G(POOL_B); a < G(POOL_B_END); a += 0x1C) *(uint32_t *)a = 0;
			PlayCameraAnimation((const void *)fxp::camera(P().res.camera, D::CAMERA));
			QueueTimUpload(TexFile());
			return QRoot();
		}

		// ------------------------------------------------------------------
		// Root (0x5E93A0) and texture restore (0x5E9410)
		// ------------------------------------------------------------------
		static uint32_t __cdecl RootTask(TaskNode *tn)
		{
			RootNode *n = (RootNode *)tn;
			if (n->c & 1) PacketCursor() = TexBase() + 0xC000;
			else PacketCursor() = TexBase();
			// 30 fps layer: see fx_megiddo_held.inc
			FX_HELD(MegiddoHeld<D>::note_root();)
			const int running = ExecuteTaskQueue(QMain());
			if (!running)
			{
				RootNode *t = (RootNode *)AddTaskToQueue(QRoot(), own::Fn(C(ORIG_Restore)));
				t->c = 0;
				t->e = 0;
			}
			n->c = (int16_t)(n->c + 1);
			return running ? 0 : TASK_END;
		}
		static uint32_t __cdecl RestoreTask(TaskNode *tn)
		{
			RootNode *n = (RootNode *)tn;
			if (n->c == 1) TextureRestoreTask((void *)TexBase(), &n->e);
			const int16_t done = n->e;
			n->c = (int16_t)(n->c + 1);
			return done ? TASK_END : 0;
		}

		static void Register(const char *const names[19])
		{
			fxp::register_params({ D::EFFECT, "Megiddo Flame", &live, &GAME, sizeof(MegiddoParams),
			                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
			                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
			                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
			own::note(C(ORIG_FileLoader), (void *)FileLoader);
			own::note(C(ORIG_Setup), (void *)Setup);
			own::note(C(ORIG_Root), (void *)RootTask);
			register_port(C(ORIG_Root), (void *)RootTask, names[0], D::EFFECT);
			own::note(C(ORIG_Restore), (void *)RestoreTask);
			register_port(C(ORIG_Restore), (void *)RestoreTask, names[1], D::EFFECT);
			own::note(C(ORIG_Director), (void *)DirectorTask);
			register_port(C(ORIG_Director), (void *)DirectorTask, names[2], D::EFFECT);
			own::note(C(ORIG_Glow), (void *)GlowTask);
			register_port(C(ORIG_Glow), (void *)GlowTask, names[3], D::EFFECT);
			own::note(C(ORIG_Spin), (void *)SpinTask);
			register_port(C(ORIG_Spin), (void *)SpinTask, names[4], D::EFFECT);
			own::note(C(ORIG_Ring), (void *)RingTask);
			register_port(C(ORIG_Ring), (void *)RingTask, names[5], D::EFFECT);
			own::note(C(ORIG_BoltDir), (void *)BoltDirTask);
			register_port(C(ORIG_BoltDir), (void *)BoltDirTask, names[6], D::EFFECT);
			own::note(C(ORIG_Arc), (void *)ArcTask);
			register_port(C(ORIG_Arc), (void *)ArcTask, names[7], D::EFFECT);
			own::note(C(ORIG_Sheet), (void *)SheetTask);
			register_port(C(ORIG_Sheet), (void *)SheetTask, names[8], D::EFFECT);
			own::note(C(ORIG_Ribbon), (void *)RibbonTask);
			register_port(C(ORIG_Ribbon), (void *)RibbonTask, names[9], D::EFFECT);
			own::note(C(ORIG_Column), (void *)ColumnTask);
			register_port(C(ORIG_Column), (void *)ColumnTask, names[10], D::EFFECT);
			own::note(C(ORIG_ColumnFlash), (void *)ColumnFlashTask);
			register_port(C(ORIG_ColumnFlash), (void *)ColumnFlashTask, names[11], D::EFFECT);
			own::note(C(ORIG_Pillar), (void *)PillarTask);
			register_port(C(ORIG_Pillar), (void *)PillarTask, names[12], D::EFFECT);
			own::note(C(ORIG_Flame), (void *)FlameTask);
			register_port(C(ORIG_Flame), (void *)FlameTask, names[13], D::EFFECT);
			own::note(C(ORIG_Sparks), (void *)SparksTask);
			register_port(C(ORIG_Sparks), (void *)SparksTask, names[14], D::EFFECT);
			own::note(C(ORIG_Debris), (void *)DebrisTask);
			register_port(C(ORIG_Debris), (void *)DebrisTask, names[15], D::EFFECT);
			own::note(C(ORIG_Knock), (void *)KnockTask);
			register_port(C(ORIG_Knock), (void *)KnockTask, names[16], D::EFFECT);
			own::note(C(ORIG_Throw), (void *)ThrowTask);
			register_port(C(ORIG_Throw), (void *)ThrowTask, names[17], D::EFFECT);
			own::note(C(ORIG_Ring2), (void *)Ring2Task);
			register_port(C(ORIG_Ring2), (void *)Ring2Task, names[18], D::EFFECT);
		}
	};
}
}
