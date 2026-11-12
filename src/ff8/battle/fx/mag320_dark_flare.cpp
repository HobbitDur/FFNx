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

// Effect 320: Dark Flare (Tiamat c0m132; MAG_320_*).
//
// Structure (setup MAG_320 0x65BC10 -> _Init 0x65BC40 (not ported), file loader 0x65BC20 = the texture file named at
// 0xFF12B0 ("mag319.tim") -> [0x2505348]; the setup queues the TIM, keeps the context (0x2505350) and the CASTER's entity
// (0x2505354), starts camera animation 0xFE729C and the root in its 1-node pool 0x2505388):
//   RootTask (0x65BCC0) - alternates the packet arena (magic buffer + 0x1314 / + 0x11314); on counter 1 sets up the
//     pools once (main 1 x 0x754 at + 0, sparks 0x40 x 0x20 at + 0x754) and starts the main task: T = the mean default
//     effect position of action 0's targets (x / z, y 0), the three layouts 0xFE761C / 0xFE9294 / 0xFE9BCC decoded. Then
//     the main and spark queues; it ends when both are empty.
//   MainTask (0x65BE90) - counters 0..0x61:
//     counters 0..0x23: S = the caster's point 0xFF12A0 (bone 0x1B, CasterBonePoint 0x65DA30 = Brrawghh 289's: the bone
//       matrices as they are); the basis M of (T - S) (0x50CBA0, y / z the other way) at S, composed with the camera:
//       layout 1 (depth -0x10); counters 0xF..0x23: two sparks a tick at the caster's points 0xFF12A8 (x +-(0x64 + 0x1F
//       + rand 0x1F), written to that .data word), their velocity towards a second point (z + 0x10 + rand 0xF);
//     counters 0x24..0x2E: M moved to L = T w + S (1 - w), w = (c - 0x24) / 10: layout 1 again; a grey tile at 0x24;
//       four sparks a tick along T -> S (w, then w - 0.1 ..., while w >= 0) thrown out in M's plane (sine / cosine of a
//       CRT rand angle, GTE GPF by 0x10 + rand 0xF);
//     counters 0x24..0x33: M at S: layout 2 (depth -0x10);
//     counters 0x2D..0x61: a rotation-free frame at T: layout 3 (depth -0x200, texture v scroll (c - 0x2D) * 16); tiles
//       at 0x2E (grey, between draw-mode packets 0x2A0 / 0x2C0), 0x2F / 0x30 / 0x31 (0xF0 / 0xA0 / 0x50 grey);
//     sound 0xFE7298 at counter 0; the damage of action 0's targets at 0x53.
//   SparkTask (0x65DA90) - sprite sequence 0xFE7470 through the shadow-rotation camera (scale, -0x100), flipbook frame =
//     its counter (from 0 or c - 0x14), 0x10 frames; moves by its velocity, which falls by 2 a tick.
//   PrimCallback (0x65C430) - one layout object (header 0x6C): 063's callback with two kind tables in the parameter
//     block: texture window kinds '1' / '2' and the renderer, '0' = the engine's 0x701DD0, else 0x65C750.
//   RenderPrimSetAlt (0x65C750) = Diablos 325's 0x657030 = Maelstrom 207's 0x6A45E0: the prim set lists 0..7 of the
//     model; lists 0 / 4 / 5 / 7 through this module's renderers 0x65C880 (F3) / 0x65CB40 (G3) / 0x65CE80 (G4) /
//     0x65D2A0 (GT4) (ported below: 325's and 207's copies call them too, in the exe), lists 1 / 2 / 3 / 6 through
//     325's 0x657160 and 207's 0x6A4710 / 0x6A4BA0 / 0x6A5120 (called in the exe). Each renderer brackets its
//     polygons with draw-mode packets 0xE1000220 / 0xE1000240 (the GT4 one: 0x220 only, plus two texture windows
//     when the header scrolls); the G3 / G4 ones step their packet cursor by twice the bracket (as the original).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: nothing written (the caster's bone matrices are only read; the camera animation is the engine's).
// The .data word 0xFF12A8 (x of the spark point) is written by the main task.
// Module globals: 0x2505348..0x25053A0 (texture file 0x2505348, packet cursor 0x250534C, context 0x2505350, caster
// entity 0x2505354, the queues 0x2505358 sparks / 0x2505368 main / 0x2505378 root, root pool 0x2505388, magic buffer
// base 0x250539C).
// NEVER-WRITTEN STACK WORDS: the 4th word of the vectors loaded into GTE V0 (the record offset of the callback, the
// spark direction) reaches VZ0's high half only; the parameter block's scroll word is written for layout 3 only (the
// other layouts' kind tables have no '1' / '2'); the matrices' pad words are never read; the prim header keeps the
// scratch's words but the ones the callback sets.

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace flare320
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x250534C); }
	inline CastContext *&Ctx() { return var<CastContext *>(0x2505350); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2505354); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x250539C); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2505358; }         // pool: + 0x754, 0x40 x 0x20
	inline TaskQueue *QMain() { return (TaskQueue *)0x2505368; }           // pool: magic buffer, 1 x 0x754
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }          // g_Battle_FrameRenderListBase

	static const uint32_t ORIG_RootTask = 0x65BCC0;
	static const uint32_t ORIG_MainTask = 0x65BE90;
	static const uint32_t ORIG_SparkTask = 0x65DA90;
	static const uint32_t POOL_Sparks = 0x754;
	static const uint32_t MORPH = 0xF54;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1314;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11314;
	static const uint32_t MODEL_1 = 0xFE761C;            // prim-model layouts
	static const uint32_t MODEL_2 = 0xFE9294;
	static const uint32_t MODEL_3 = 0xFE9BCC;
	static const uint32_t KINDS_1 = 0xFE0FAC, DRAWS_1 = 0xFF12D4; // per object: texture window kind / renderer
	static const uint32_t KINDS_2 = 0xFE0FB4, DRAWS_2 = 0xFE0FC8;
	static const uint32_t KINDS_3 = 0xFF12C8, DRAWS_3 = 0xFF12BC;
	static const uint32_t SEQ_Spark = 0xFE7470;          // sprite sequence
	static const void *const SOUND = (const void *)0xFE7298;
	static const int16_t *const POINT_S = (const int16_t *)0xFF12A0; // caster point (x, y, z, bone)
	static int16_t *const POINT_SPARK = (int16_t *)0xFF12A8;         // spark point (x written by the main task)

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }  // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatMul3x3(Mat4x3 *a, const int16_t *b) { fn<void (__cdecl *)(Mat4x3 *, const int16_t *)>(0x56C220)(a, b); }    // MatrixMultiply3x3: a = a * b
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	// MAG_063_RenderPrimSet (0x701DD0): the engine's prim set draw (header 0x6C bytes)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, shift, cursor); }
	// the prim set renderers of other modules that 0x65C750 calls (pure draws of the header's next list)
	inline uint32_t RenderList(uint32_t f, void *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(f)(h, ot, shift, cursor); }
	// software GTE
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E260)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
	inline void GteLoadRGBC(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E110)(src); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2505388)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t started;   // +0x0E pools set up
		uint8_t pad0F;
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x754 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		Mat4x3 M;          // +0x10 basis of T - S (counters 0..0x23), translation rewritten per draw
		int16_t S[4];      // +0x30 the caster's point (counters 0..0x23)
		int16_t T[4];      // +0x38 the targets' mean position (y 0)
		uint8_t layout_1[0x1F0]; // +0x40
		uint8_t layout_2[0x110]; // +0x230
		uint8_t layout_3[0x414]; // +0x340
	};
	struct SparkNode // pool of 0x40 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16 sprite scale (shadow-rotation camera)
		int16_t vel[3];    // +0x18
		int16_t pad1E;
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		uint32_t kinds;    // +0x20 per object: texture window kind ('1' / '2')
		uint32_t draws;    // +0x24 per object: '0' = 0x701DD0, else 0x65C750
		int16_t depth;     // +0x28 the prim header's +0x18
		int16_t scroll;    // +0x2A texture v scroll (low 7 bits; written for layout 3 only)
		uint32_t morph;    // +0x2C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x754 && sizeof(SparkNode) == 0x20, "Dark Flare nodes");
	static_assert(sizeof(PrimArg) == 0x30, "Dark Flare prim block");
	static_assert(offsetof(MainNode, S) == 0x30 && offsetof(MainNode, layout_1) == 0x40 && offsetof(MainNode, layout_3) == 0x340, "Dark Flare main node");

	// ------------------------------------------------------------------
	// Prim set renderers of this module (folded copies: 0x657030 / 0x6A45E0 call them too)
	// ------------------------------------------------------------------
	// header (0x6C, Field_Alloc scratch): +0x00 model, +0x04 vertices (4 bytes per index step), +0x08 far colour (r, g,
	// b), +0x0C fade (IR0), +0x18 depth, +0x1C flags, +0x20 / +0x22 texture u / v scroll, +0x24..+0x32 two texture
	// windows, +0x34 the list cursor; +0x38 / +0x40 / +0x44 the GTE's MAC0 / OTZ / FLAG of the last polygon
	static inline uint32_t &H32(uint8_t *h, int o) { return *(uint32_t *)(h + o); }
	static inline int32_t &HI32(uint8_t *h, int o) { return *(int32_t *)(h + o); }
	static inline uint16_t &H16(uint8_t *h, int o) { return *(uint16_t *)(h + o); }
	static inline uint32_t &P32(uint8_t *p, int o) { return *(uint32_t *)(p + o); }
	static inline int16_t PX(const uint8_t *p, int o) { return *(const int16_t *)(p + o); }
	static inline bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static inline bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }
	static inline const uint8_t *Vtx(const uint8_t *v, const uint8_t *f, int o) { return v + (uint32_t)*(const uint16_t *)(f + o) * 4; }

	// the polygon's OT bucket (OTZ + header depth, at least 0)
	static uint32_t OTBucket(uint8_t *h, uint32_t ot, int32_t shift)
	{
		HI32(h, 0x40) = (int32_t)((uint32_t)HI32(h, 0x40) + (uint32_t)HI32(h, 0x18));
		if (HI32(h, 0x40) < 0) HI32(h, 0x40) = 0;
		return ot + (uint32_t)(HI32(h, 0x40) >> shift) * 4;
	}

	// 0x65C880: list 0, flat triangles (faces 0xC bytes: colour + code, three vertex indices)
	static uint32_t RenderF3(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *lp = (uint8_t *)H32(h, 0x34);
		int32_t n = *(const int32_t *)lp;
		uint8_t *f = lp + 4;
		const uint8_t *v = (const uint8_t *)H32(h, 4);
		H32(h, 0x34) = (uint32_t)f;
		uint8_t *P = (uint8_t *)cursor;
		if (n <= 0) return cursor;
		do
		{
			GteLoadV012(Vtx(v, f, 4), Vtx(v, f, 6), Vtx(v, f, 8));
			GteRTPT();
			P32(P, 0) = 0x4000000;
			P32(P, 4) = *(const uint32_t *)f;
			if (h[0x1C] & 1) P32(P, 4) = *(const uint32_t *)f | 0x2000000;
			if (h[0x1C] & 4) P32(P, 4) &= 0xFDFFFFFF;
			GteReadFLAG(h + 0x44);
			if (H32(h, 0x44) & 0x60000) goto next;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (HI32(h, 0x38) < 0 && !(h[0x1C] & 0x10)) goto next;
			{
				GteReadSXY012Split(P + 8, P + 0xC, P + 0x10);
				GteAVSZ3();
				uint32_t clip = 0;
				if (OffX(PX(P, 8))) clip = 1;
				if (OffX(PX(P, 0xC))) clip |= 2;
				if (OffX(PX(P, 0x10))) clip |= 4;
				if (OffY(PX(P, 0xA))) clip |= 0x10;
				if (OffY(PX(P, 0xE))) clip |= 0x20;
				if (OffY(PX(P, 0x12))) clip |= 0x40;
				if ((clip & 7) == 7 || (clip & 0x70) == 0x70) goto next;
				GteReadOTZ32(h + 0x40);
				if (h[0x1C] & 0x40)
				{
					GteLoadRGBC(P + 4);
					GteSetIR0(HI32(h, 0xC));
					GteDPCS();
					GteStoreRGB2(P + 4);
				}
				const uint32_t bucket = OTBucket(h, ot, shift);
				P32(P, 0x14) = 0x1000000;
				P32(P, 0x18) = 0xE1000220;
				InsertPrimAutoDepth(bucket, P + 0x14);
				InsertPrimAutoDepth(bucket, P);
				P32(P, 0x1C) = 0x1000000;
				P32(P, 0x20) = 0xE1000240;
				InsertPrimAutoDepth(bucket, P + 0x1C);
				P += 0x24;
			}
		next:
			f += 0xC;
		} while (--n);
		H32(h, 0x34) = (uint32_t)f;
		return (uint32_t)P;
	}

	// 0x65CB40: list 4, gouraud triangles (faces 0x14 bytes: colour 0 + code, three indices, colours 1 / 2); the
	// packet cursor steps 0x48 per polygon drawn (the packets use 0x2C)
	static uint32_t RenderG3(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *lp = (uint8_t *)H32(h, 0x34);
		int32_t n = *(const int32_t *)lp;
		uint8_t *f = lp + 4;
		const uint8_t *v = (const uint8_t *)H32(h, 4);
		H32(h, 0x34) = (uint32_t)f;
		uint8_t *P = (uint8_t *)cursor;
		if (n <= 0) return cursor;
		do
		{
			GteLoadV012(Vtx(v, f, 4), Vtx(v, f, 6), Vtx(v, f, 8));
			GteRTPT();
			P32(P, 0) = 0x6000000;
			P32(P, 4) = *(const uint32_t *)f;
			if (h[0x1C] & 2) P32(P, 4) = *(const uint32_t *)f | 0x2000000;
			if (h[0x1C] & 8) P32(P, 4) &= 0xFDFFFFFF;
			GteReadFLAG(h + 0x44);
			if (H32(h, 0x44) & 0x60000) goto next;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (HI32(h, 0x38) < 0 && !(h[0x1C] & 0x20)) goto next;
			{
				GteReadSXY012Split(P + 8, P + 0x10, P + 0x18);
				GteAVSZ3();
				uint32_t clip = 0;
				if (OffX(PX(P, 8))) clip = 1;
				if (OffX(PX(P, 0x10))) clip |= 2;
				if (OffX(PX(P, 0x18))) clip |= 4;
				if (OffY(PX(P, 0xA))) clip |= 0x10;
				if (OffY(PX(P, 0x12))) clip |= 0x20;
				if (OffY(PX(P, 0x1A))) clip |= 0x40;
				if ((clip & 7) == 7 || (clip & 0x70) == 0x70) goto next;
				GteReadOTZ32(h + 0x40);
				if (h[0x1C] & 0x80)
				{
					GteLoadRGB012(f + 0xC, f + 0x10, P + 4);
					GteSetIR0(HI32(h, 0xC));
					GteDPCT();
					GteStoreRGB012(P + 0xC, P + 0x14, P + 4);
				}
				else
				{
					P32(P, 0xC) = *(const uint32_t *)(f + 0xC);
					P32(P, 0x14) = *(const uint32_t *)(f + 0x10);
				}
				const uint32_t bucket = OTBucket(h, ot, shift);
				P32(P, 0x1C) = 0x1000000;
				P32(P, 0x20) = 0xE1000220;
				InsertPrimAutoDepth(bucket, P + 0x1C);
				InsertPrimAutoDepth(bucket, P);
				P32(P, 0x24) = 0x1000000;
				P32(P, 0x28) = 0xE1000240;
				InsertPrimAutoDepth(bucket, P + 0x24);
				P += 0x48;
			}
		next:
			f += 0x14;
		} while (--n);
		H32(h, 0x34) = (uint32_t)f;
		return (uint32_t)P;
	}

	// 0x65CE80: list 5, gouraud quads (faces 0x18 bytes: colour 0 + code, four indices, colours 1..3); the packet
	// cursor steps 0x58 per polygon drawn (the packets use 0x34)
	static uint32_t RenderG4(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *lp = (uint8_t *)H32(h, 0x34);
		int32_t n = *(const int32_t *)lp;
		uint8_t *f = lp + 4;
		const uint8_t *v = (const uint8_t *)H32(h, 4);
		H32(h, 0x34) = (uint32_t)f;
		uint8_t *P = (uint8_t *)cursor;
		if (n <= 0) return cursor;
		do
		{
			GteLoadV012(Vtx(v, f, 4), Vtx(v, f, 6), Vtx(v, f, 8));
			GteRTPT();
			P32(P, 0) = 0x8000000;
			P32(P, 4) = *(const uint32_t *)f;
			if (h[0x1C] & 2) P32(P, 4) = *(const uint32_t *)f | 0x2000000;
			if (h[0x1C] & 8) P32(P, 4) &= 0xFDFFFFFF;
			GteReadFLAG(h + 0x44);
			if (H32(h, 0x44) & 0x60000) goto next;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (HI32(h, 0x38) < 0 && !(h[0x1C] & 0x20)) goto next;
			{
				GteReadSXY012Split(P + 8, P + 0x10, P + 0x18);
				GteLoadV0(Vtx(v, f, 0xA));
				GteRTPS();
				uint32_t clip = 0;
				if (OffX(PX(P, 8))) clip = 1;
				if (OffX(PX(P, 0x10))) clip |= 2;
				if (OffX(PX(P, 0x18))) clip |= 4;
				if (OffY(PX(P, 0xA))) clip |= 0x10;
				if (OffY(PX(P, 0x12))) clip |= 0x20;
				if (OffY(PX(P, 0x1A))) clip |= 0x40;
				GteReadSXY2(P + 0x20);
				GteAVSZ4();
				if (OffX(PX(P, 0x20))) clip |= 8;
				if (OffY(PX(P, 0x22))) clip |= 0x80;
				if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) goto next;
				GteReadOTZ32(h + 0x40);
				if (h[0x1C] & 0x80)
				{
					GteLoadRGB012(f + 0xC, f + 0x10, f + 0x14);
					GteSetIR0(HI32(h, 0xC));
					GteDPCT();
					GteStoreRGB012(P + 0xC, P + 0x14, P + 0x1C);
					GteLoadRGBC(P + 4);
					GteDPCS();
					GteStoreRGB2(P + 4);
				}
				else
				{
					P32(P, 0xC) = *(const uint32_t *)(f + 0xC);
					P32(P, 0x14) = *(const uint32_t *)(f + 0x10);
					P32(P, 0x1C) = *(const uint32_t *)(f + 0x14);
				}
				const uint32_t bucket = OTBucket(h, ot, shift);
				P32(P, 0x24) = 0x1000000;
				P32(P, 0x28) = 0xE1000220;
				InsertPrimAutoDepth(bucket, P + 0x24);
				InsertPrimAutoDepth(bucket, P);
				P32(P, 0x2C) = 0x1000000;
				P32(P, 0x30) = 0xE1000240;
				InsertPrimAutoDepth(bucket, P + 0x2C);
				P += 0x58;
			}
		next:
			f += 0x18;
		} while (--n);
		H32(h, 0x34) = (uint32_t)f;
		return (uint32_t)P;
	}

	// a texture window command from the header's window block at o (+0 / +2 offset bytes, +4 / +6 mask words)
	static uint32_t TexWindow(const uint8_t *h, int o)
	{
		uint32_t e = ((uint32_t)(h[o + 2] & 0xF8) | 0xFFFE2000u) << 5;
		e = (e | (uint32_t)(h[o] & 0xF8)) << 5;
		e |= (0u - (uint32_t)*(const uint16_t *)(h + o + 6)) & 0xF8;
		e <<= 2;
		return e | ((uint32_t)((int32_t)(0u - (uint32_t)*(const uint16_t *)(h + o + 4)) >> 3) & 0x1F);
	}

	// one texture coordinate scrolled: the four bytes at P + o + 0xC * i moved by d, less the wrap byte when any passes 0xFF
	static void ScrollUV(uint8_t *P, int o, uint32_t d, uint8_t wrap)
	{
		const uint32_t s0 = P[o] + d, s1 = P[o + 0xC] + d, s2 = P[o + 0x18] + d, s3 = P[o + 0x24] + d;
		if ((int32_t)(s3 | s2 | s1 | s0) > 0xFF)
		{
			P[o] = (uint8_t)((uint8_t)s0 - wrap);
			P[o + 0xC] = (uint8_t)((uint8_t)s1 - wrap);
			P[o + 0x18] = (uint8_t)((uint8_t)s2 - wrap);
			P[o + 0x24] = (uint8_t)((uint8_t)s3 - wrap);
		}
		else
		{
			P[o + 0x18] = (uint8_t)s2;
			P[o] = (uint8_t)s0;
			P[o + 0xC] = (uint8_t)s1;
			P[o + 0x24] = (uint8_t)s3;
		}
	}

	// 0x65D2A0: list 7, textured gouraud quads (faces 0x24 bytes: colour 0 + code, four indices, uv 0 + clut, uv 1 +
	// page, uv 2 / 3, colours 1..3); with a texture scroll (header +0x20 / +0x22) the uvs move (wrapping by the window
	// bytes +0x30 / +0x32) between two texture windows
	static uint32_t RenderGT4(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *lp = (uint8_t *)H32(h, 0x34);
		int32_t n = *(const int32_t *)lp;
		uint8_t *f = lp + 4;
		const uint8_t *v = (const uint8_t *)H32(h, 4);
		H32(h, 0x34) = (uint32_t)f;
		uint8_t *P = (uint8_t *)cursor;
		if (n <= 0) return cursor;
		do
		{
			GteLoadV012(Vtx(v, f, 4), Vtx(v, f, 6), Vtx(v, f, 8));
			GteRTPT();
			P32(P, 0) = 0xC000000;
			P32(P, 4) = *(const uint32_t *)f;
			if (h[0x1C] & 2) P32(P, 4) |= 0x2000000;
			if (h[0x1C] & 8) P32(P, 4) &= 0xFDFFFFFF;
			P32(P, 0xC) = *(const uint32_t *)(f + 0xC);
			P32(P, 0x18) = *(const uint32_t *)(f + 0x10);
			P32(P, 0x24) = *(const uint32_t *)(f + 0x14);
			P32(P, 0x30) = *(const uint32_t *)(f + 0x14) >> 16;
			GteReadFLAG(h + 0x44);
			if (H32(h, 0x44) & 0x60000) goto next;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (HI32(h, 0x38) < 0 && !(h[0x1C] & 0x20)) goto next;
			{
				GteReadSXY012Split(P + 8, P + 0x14, P + 0x20);
				GteLoadV0(Vtx(v, f, 0xA));
				GteRTPS();
				uint32_t clip = 0;
				if (OffX(PX(P, 8))) clip = 1;
				if (OffX(PX(P, 0x14))) clip |= 2;
				if (OffX(PX(P, 0x20))) clip |= 4;
				if (OffY(PX(P, 0xA))) clip |= 0x10;
				if (OffY(PX(P, 0x16))) clip |= 0x20;
				if (OffY(PX(P, 0x22))) clip |= 0x40;
				GteReadSXY2(P + 0x2C);
				GteAVSZ4();
				if (OffX(PX(P, 0x2C))) clip |= 8;
				if (OffY(PX(P, 0x2E))) clip |= 0x80;
				if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) goto next;
				GteReadOTZ32(h + 0x40);
				if (h[0x1C] & 0x80)
				{
					GteLoadRGB012(f + 0x18, f + 0x1C, f + 0x20);
					GteSetIR0(HI32(h, 0xC));
					GteDPCT();
					GteStoreRGB012(P + 0x10, P + 0x1C, P + 0x28);
					GteLoadRGBC(P + 4);
					GteDPCS();
					GteStoreRGB2(P + 4);
				}
				else
				{
					P32(P, 0x10) = *(const uint32_t *)(f + 0x18);
					P32(P, 0x1C) = *(const uint32_t *)(f + 0x1C);
					P32(P, 0x28) = *(const uint32_t *)(f + 0x20);
				}
				const uint32_t bucket = OTBucket(h, ot, shift);
				P32(P, 0x34) = 0x1000000;
				P32(P, 0x38) = 0xE1000220;
				InsertPrimAutoDepth(bucket, P + 0x34);
				if ((H16(h, 0x20) | H16(h, 0x22)) == 0)
				{
					InsertPrimAutoDepth(bucket, P);
					P += 0x3C;
				}
				else
				{
					if (H16(h, 0x20) != 0) ScrollUV(P, 0xC, H16(h, 0x20), h[0x30]);
					if (H16(h, 0x22) != 0) ScrollUV(P, 0xD, H16(h, 0x22), h[0x32]);
					P32(P, 0x3C) = 0x2000000;
					P32(P, 0x40) = TexWindow(h, 0x24);
					P32(P, 0x44) = 0;
					InsertPrimAutoDepth(bucket, P + 0x3C);
					InsertPrimAutoDepth(bucket, P);
					P32(P, 0x48) = 0x2000000;
					P32(P, 0x4C) = TexWindow(h, 0x2C);
					P32(P, 0x50) = 0;
					InsertPrimAutoDepth(bucket, P + 0x48);
					P += 0x54;
				}
			}
		next:
			f += 0x24;
		} while (--n);
		H32(h, 0x34) = (uint32_t)f;
		return (uint32_t)P;
	}

	// 0x65C750 (= 325's 0x657030 / 207's 0x6A45E0): the model's eight prim lists
	static uint32_t RenderPrimSetAlt(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		if (!(H32(h, 0x1C) & 0x2000)) H32(h, 4) = H32(h, 0) + 8;
		const uint32_t model = H32(h, 0);
		H32(h, 0x34) = *(const uint32_t *)model + model;
		GteSetFarColor(h[8], h[9], h[0xA]);
		uint32_t c = cursor;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderF3(h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderList(0x657160, h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderList(0x6A4710, h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderList(0x6A4BA0, h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderG3(h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderG4(h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderList(0x6A5120, h, ot, shift, c); else H32(h, 0x34) += 4;
		if (*(const uint32_t *)H32(h, 0x34) != 0) c = RenderGT4(h, ot, shift, c); else H32(h, 0x34) += 4;
		return c;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x65C430): one object of a layout. Vertex frame (or blend into the
	// parameter block's morph buffer), rotation from the record's angles (flag 0x40000: YXZ order),
	// the record's offset through the parameter matrix composed with it (flag 0x200: the offset as
	// is), scale (flag 0x100: 3x3 product, else scale3DMatrix), fade, the texture window of the
	// object's kind, drawn by 0x701DD0 or this module's 0x65C750.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
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
		Mat4x3 m = {};
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				const int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MatMul3x3(&m, s);
			}
			else
			{
				const int32_t s[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, s);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x26) = 0;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint16_t *)(h + 0x2A) = 0x100;
		*(uint16_t *)(h + 0x28) = 0x100;
		const uint8_t kind = *(const uint8_t *)(arg->kinds + object);
		if (kind == '2')
		{
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0x80;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x22) = (uint16_t)(*(const uint8_t *)&arg->scroll & 0x7F);
		}
		else if (kind == '1')
		{
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x2E) = 0;
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x22) = (uint16_t)(*(const uint8_t *)&arg->scroll & 0x7F);
		}
		if (*(const uint8_t *)(arg->draws + object) == '0') PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		else PacketCursor() = RenderPrimSetAlt(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// 0x65DA30 (= Brrawghh 289's CasterBonePoint 0x678390): a point (x, y, z, bone) through the caster's bone composed
	// with its matrix (the bone matrices as they are)
	static void CasterBonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x65D900: a full-screen grey tile (semi-transparent F4) on the frame OT + 0x20
	static void Tile(uint32_t rgb)
	{
		uint8_t *p = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		*(uint16_t *)(p + 8) = 0;
		*(uint16_t *)(p + 0x10) = 0;
		*(uint16_t *)(p + 0xE) = 0;
		*(uint16_t *)(p + 0xA) = 0;
		*(uint16_t *)(p + 0x14) = 0x140;
		*(uint16_t *)(p + 0x16) = 0xD8;
		*(uint16_t *)(p + 0x12) = 0xD8;
		*(uint16_t *)(p + 0xC) = 0x140;
		*(uint32_t *)(p + 4) = rgb;
		*(uint32_t *)p = 0x5000000;
		p[7] = 0x2A;
		InsertPrimAltViewport(OTBase() + 0x20, p);
	}

	// a draw-mode packet on the frame OT + 0x20
	static void DrawMode(uint32_t mode)
	{
		uint8_t *p = (uint8_t *)PacketCursor();
		PacketCursor() += 8;
		*(uint32_t *)p = 0x1000000;
		*(uint32_t *)(p + 4) = mode;
		InsertPrimAutoDepth(OTBase() + 0x20, p);
	}

	// 0x65D970: the tile between the draw modes 0x2A0 and 0x2C0
	static void TileModes(uint32_t rgb)
	{
		DrawMode(0xE10002A0);
		Tile(rgb);
		DrawMode(0xE10002C0);
	}

	// the sprite of a spark (0x65DA90..0x65DAEC)
	static void SparkDraw(const int16_t *pos, int16_t scale, int16_t frame)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, scale, -0x100);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the main's frame of counters 0..0x23: S, the basis M of T - S at S
	static void MainFrame(MainNode *m, const int16_t *S, Mat4x3 *M)
	{
		const int32_t v[3] = { (int32_t)m->T[0] - S[0], (int32_t)S[1] - m->T[1], (int32_t)S[2] - m->T[2] };
		BuildOrthonormalBasis(v, M);
		M->t[0] = S[0];
		M->t[1] = S[1];
		M->t[2] = S[2];
	}

	// the weight of T for counters 0x24..0x2E: (c - 0x24) / 10
	static inline int32_t LerpWeight(int32_t c) { return (int32_t)(((uint32_t)(c - 0x24) << 12) / 10u); }

	static void SetArg(PrimArg *arg, uint32_t kinds, uint32_t draws, int16_t depth)
	{
		arg->morph = TexBase() + MORPH;
		arg->kinds = kinds;
		arg->draws = draws;
		arg->depth = depth;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag320_dark_flare_held.h"
#endif

namespace ff8fx
{
namespace flare320
{
	// ------------------------------------------------------------------
	// Spark (0x65DA90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag320_dark_flare_held.inc
		FX_HELD(held_note_spark(n);)
		SparkDraw(s->pos, s->scale, s->counter);
		s->counter++;
		if (s->counter >= 0x10) return TASK_END;
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->vel[1] = (int16_t)(s->vel[1] - 2);
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x65BE90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		PrimArg arg;
		arg.scroll = 0; // (never-written stack word: only read for layout 3's kinds, which write it)
		const int32_t c = m->counter;
		if ((uint32_t)c < 0x24)
		{
			CasterBonePoint(POINT_S, m->S);
			MainFrame(m, m->S, &m->M);
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			SetArg(&arg, KINDS_1, DRAWS_1, -0x10);
			// 30 fps layer: see mag320_dark_flare_held.inc
			FX_HELD(held_note_main(PLAY_1, n);)
			prim::play((prim::Layout *)m->layout_1, PrimCallback, (int)&arg, 0);
			if ((uint32_t)c >= 0xF)
			{
				int16_t p[4];
				p[1] = POINT_SPARK[1];
				p[3] = POINT_SPARK[3];
				for (int k = 0; k < 2; k++)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (!s) break;
					p[0] = (int16_t)((CrtRand() & 0x1F) + 0x1F);
					if (k == 0)
					{
						p[0] = (int16_t)(p[0] + 0x64);
						POINT_SPARK[0] = 0x64;
					}
					else
					{
						p[0] = (int16_t)(-0x64 - p[0]);
						POINT_SPARK[0] = -0x64;
					}
					CasterBonePoint(POINT_SPARK, s->pos);
					s->counter = (int16_t)((uint32_t)c > 0x14 ? c - 0x14 : 0);
					s->scale = (int16_t)((CrtRand() & 0x3FF) + 0x400);
					p[2] = (int16_t)((CrtRand() & 0xF) + *(const int32_t *)(POINT_SPARK + 2) + 0x10);
					int16_t q[4];
					CasterBonePoint(p, q);
					s->vel[0] = (int16_t)(q[0] - s->pos[0]);
					s->vel[1] = (int16_t)(q[1] - s->pos[1]);
					s->vel[2] = (int16_t)(q[2] - s->pos[2]);
				}
			}
		}
		if ((uint32_t)(c - 0x24) <= 0xA)
		{
			const int32_t w = LerpWeight(c);
			if (c == 0x24) Tile(0x808080);
			int16_t L[4];
			VecLerp(m->T, m->S, w, 0x1000 - w, L);
			m->M.t[0] = L[0];
			m->M.t[1] = L[1];
			m->M.t[2] = L[2];
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			SetArg(&arg, KINDS_1, DRAWS_1, -0x10);
			FX_HELD(held_note_main(PLAY_1L, n);)
			prim::play((prim::Layout *)m->layout_1, PrimCallback, (int)&arg, 0);
		}
		if ((uint32_t)(c - 0x24) < 0x10)
		{
			m->M.t[0] = m->S[0];
			m->M.t[1] = m->S[1];
			m->M.t[2] = m->S[2];
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			SetArg(&arg, KINDS_2, DRAWS_2, -0x10);
			FX_HELD(held_note_main(PLAY_2, n);)
			prim::play((prim::Layout *)m->layout_2, PrimCallback, (int)&arg, 0);
		}
		const int32_t e = c - 0x2D;
		if ((uint32_t)e < 0x35)
		{
			if (e == 1) TileModes(0x808080);
			else if (e == 2) Tile(0xF0F0F0);
			else if (e == 3) Tile(0xA0A0A0);
			else if (e == 4) Tile(0x505050);
			RotY(0, &arg.m);
			arg.m.t[0] = m->T[0];
			arg.m.t[1] = m->T[1];
			arg.m.t[2] = m->T[2];
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			SetArg(&arg, KINDS_3, DRAWS_3, -0x200);
			arg.scroll = (int16_t)(e << 4);
			FX_HELD(held_note_main(PLAY_3, n);)
			prim::play((prim::Layout *)m->layout_3, PrimCallback, (int)&arg, 0);
		}
		if (c == 0) BdPlaySE(SOUND, 0, 0x80);
		if ((uint32_t)(c - 0x24) <= 0xA)
		{
			int32_t w = LerpWeight(c);
			GteSetRotMatrix(&m->M);
			int32_t ws = 0x1000 - w;
			for (int k = 0; k < 4; k++)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				if (!s) break;
				s->counter = 0;
				VecLerp(m->T, m->S, w, ws, s->pos);
				s->scale = (int16_t)((CrtRand() & 0x7FF) + 0x1000);
				const int32_t a = CrtRand();
				int16_t d[4];
				d[0] = (int16_t)ComputeSin(a);
				d[1] = (int16_t)ComputeCos(a);
				d[2] = 0;
				d[3] = 0; // (never written: the GTE V0 pad)
				GteLoadV0(d);
				GteMVMVA_RotV0();
				GteStoreIR123(d);
				s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(d[0] >> 6));
				s->pos[1] = (int16_t)(s->pos[1] + (int16_t)(d[1] >> 6));
				s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(d[2] >> 6));
				GteSetIR0((CrtRand() & 0xF) + 0x10);
				GteLoadIR123(d);
				GteGPF();
				GteStoreIR123(s->vel);
				w -= 0x66;
				ws += 0x66;
				if (w < 0) break;
			}
		}
		if (c == 0x53)
		{
			ActionData *a = Ctx()->actions;
			ApplyActionResultToTargets(a->targets, a->target_count);
		}
		m->counter++;
		return m->counter < 0x62 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Root (0x65BCC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		// 30 fps layer: see mag320_dark_flare_held.inc
		FX_HELD(held_note_root();)
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMain(), (void *)TexBase(), 0x754, 1);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + POOL_Sparks), 0x20, 0x40);
			}
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x1D2);
			const int32_t count = Ctx()->actions->target_count;
			m->T[2] = 0;
			m->T[0] = 0;
			for (int k = 0; k < count; k++)
			{
				int16_t p[4];
				GetDefaultEffectPosition(Entity(Ctx()->actions->targets[k * TARGET_STRIDE]), p);
				m->T[0] = (int16_t)(m->T[0] + p[0]);
				m->T[2] = (int16_t)(m->T[2] + p[2]);
			}
			m->T[1] = 0;
			m->T[0] = (int16_t)((int32_t)m->T[0] / count);
			m->T[2] = (int16_t)((int32_t)m->T[2] / count);
			DecodeModelPrimLayout(MODEL_1, m->layout_1, 0x1F0);
			DecodeModelPrimLayout(MODEL_2, m->layout_2, 0x110);
			DecodeModelPrimLayout(MODEL_3, m->layout_3, 0x414);
		}
		if (r->started)
		{
			const int a0 = ExecuteTaskQueue(QMain());
			const int a1 = ExecuteTaskQueue(QSparks());
			if (a0 == 0 && a1 == 0) return TASK_END;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag320_dark_flare()
	{
		register_port(flare320::ORIG_RootTask, (void *)flare320::RootTask, "320 RootTask", 320);
		register_port(flare320::ORIG_MainTask, (void *)flare320::MainTask, "320 MainTask", 320);
		register_port(flare320::ORIG_SparkTask, (void *)flare320::SparkTask, "320 SparkTask", 320);
		// 30 fps layer: see mag320_dark_flare_held.inc
		FX_HELD(register_mag320_held();)
	}
	FX_LAB_MODULE(register_mag320_dark_flare);
}

#ifdef FF8_FX_HELD
#include "mag320_dark_flare_held.inc"
#endif
