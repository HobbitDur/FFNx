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

// Effect 189: Wall (kernel.bin magic Wall +4 = 189: Selphie's Slot; MAG_189_*). Module 0x5BC940..0x5BE320.
//
// Structure (setup MAG_189_WALL 0x5BC960, file loader MAG_189_WALL_FL 0x5BC940 = the texture file named at 0xD1B4A8 into
// 0x2296A28; the setup keeps the cast context (0x2296A20), the first target's slot of action 0 (0x2293F4C) and the attacker
// (0x2296A24, never read again), sets up the root queue 0x2293F80 (pool 0x2293F60, 2 x 0x10) with RootTask and the effect queue
// 0x2295C00 (pool 0x2295C10, 100 x 0x24) with MainTask, frees the dust and debris pools, starts the camera animation 0xD19580
// and queues the TIM):
//   RootTask (0x5BE290) - alternates the packet arena (0x22A4A30 on odd counters, else 0x2296A30; cursor 0x22B2A30), runs the
//     effect queue; when it is empty queues RestoreTask in the root queue and ends.
//   RestoreTask (0x5BE2F0) - at counter 1 starts the engine texture restore task (on the even arena 0x2296A30), ends when its
//     done flag (+0xE) is set.
//   MainTask (0x5BCA20) - nothing in the draw-only mode. Counter 0: the wall point = the centre of the target mask's entities
//     (CalculateCenterPosition) + (0, 0, -1500) through the first target's world matrix (entity +0x40), then the ONE-SHOT
//     STAGE CAPTURE (0x5BCCC0); 1: 24 shard tasks on an ellipse around the point (random start angle, delays, sizes) and the
//     sound 0xD1B3A4; 0xE: the dust and debris particle tasks; 0x10: the wall task; 0x41: the action results of action 0;
//     ends after 0x46.
//   Capture (0x5BCCC0) - renders the battle stage's draw entries 1 and 0 (0x1D98990 + 0x2C k, when bits 0 and 1 of +1 are
//     set: their poses -> bone matrices through a fixed top-down matrix (rotation x 0x400, scale (235, 4096, 374), translation
//     (-1 - x / 18, z / 8 + 1, 0x7FFF)), RenderGeometry mode 0x10) through the other display buffer's draw environment (DR_ENV
//     0x2295BC0) into the capture area (0x180, 0x100, 256 x 256) with the screen offset at its centre and GTE H 0x7FFF (put
//     back), then the alternate-viewport packet 0xD1B490 (exe data), the draw area 0x2293F50 and draw offset 0x2294DA0 packets.
//   WallTask (0x5BCF40) - draws THEN updates: the wall model 0xD19B74 at its position (scale +0x1C); counters 0..0x13 through
//     the module's plane-clipped renderer (0x5BD1C0, the text of Sticky Web's 0x609FF0 relocated: the part of the model below
//     the model-space plane y = 2013 c / 20 - 2013, normal (0, -1, 0)) while it rises (y -= 100 a tick from 2013), from 0x14
//     whole through MAG_166_sub_64F0A0 (y 0, fade from 0x30); counters < 0x1A shake the camera (the battle camera shake word
//     0x1D97712 += +-40 sin(c * 0x800 / 26), the sign alternating); ends after 0x3B.
//   ShardTask (0x5BDB60) - a delay (+0xE) first (nothing drawn); then draws THEN updates: prim model 0xD19804 at its point
//     (yaw +0x18, scale x / z +0x1C, y +0x20 growing by +0x22 for counters < 6, fade from 0x10); ends after 0x17.
//   DustTask (0x5BDCC0) - draws THEN updates the dust pool 0x2294DB0 (150 x 0x18): flipbook 0xD19658 at each particle
//     (TransformCameraByShadowRotation, frame++, dies when the flipbook ends, x / z move by a decaying speed); counters 0..0x38
//     spawn 4 a tick on a circle of radius 1800 around the wall point (z flattened), flying outwards; ends from counter 4
//     once none is left.
//   DebrisTask (0x5BDF60) - draws THEN updates the debris pool 0x2293F90 (100 x 0x18): prim model 0xD19ABC at each piece
//     (angles x / z spinning, uniform scale shrinking from life 0xC, falling y with a decaying speed; dies at life 0x10);
//     counters 0..0x18 spawn 3 a tick; ends from counter 4 once none is left.
// Every task but RootTask / RestoreTask tests the draw-only flags (battle_to_update_flags 0x201); MainTask does nothing then.
// ENTITY / CAMERA WRITES: the camera shake word 0x1D97712 (WallTask, counters < 0x1A); the stage draw entries 1 / 0: their
// bone matrices rebuilt from their poses through the capture matrix (counter 0); the GTE screen offset / H (put back);
// engine: the camera animation 0xD19580 (setup), ApplyActionResultToTargets (counter 0x41). Reads: the targets' positions,
// the first target's world matrix (counter 0), the camera matrix 0x1D97778.
// Module globals: 0x2293F4C..0x22B2A34 (first target slot 0x2293F4C, draw-area packet 0x2293F50, root pool / queue, debris
// pool 0x2293F90, draw-offset packet 0x2294DA0, dust pool 0x2294DB0, DR_ENV packet 0x2295BC0, effect queue 0x2295C00 + pool,
// context 0x2296A20, attacker 0x2296A24, texture file 0x2296A28, draw offset words 0x2296A2C, the packet arenas 0x2296A30 /
// 0x22A4A30 and the cursor 0x22B2A30). Exe data written: the OT link of the alternate-viewport packet 0xD1B490.
// NEVER-WRITTEN STACK WORDS: none read (the angle blocks' 4th words and the vector 4th word are not read by
// ComposeZYXRotationMatrix / BuildRotationMatrixFromAngles / matrixMultiplyVector; every matrix is fully written).

#include "mag_common.h"

namespace ff8fx
{
namespace wall189
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &FirstTarget() { return var<int32_t>(0x2293F4C); }      // first target slot of action 0 (setup)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2293F80; }             // pool 0x2293F60, 2 x 0x10
	inline TaskQueue *QEffects() { return (TaskQueue *)0x2295C00; }          // pool 0x2295C10, 100 x 0x24
	inline CastContext *&Ctx() { return var<CastContext *>(0x2296A20); }
	inline uint32_t &Cursor() { return var<uint32_t>(0x22B2A30); }
	inline int16_t &CameraShakeY() { return var<int16_t>(0x1D97712); }      // battle camera shake word (y)
	inline uint8_t DisplayBuffer() { return var<uint8_t>(0x1D96A80); }

	static const uint32_t ARENA_ODD = 0x22A4A30, ARENA_EVEN = 0x2296A30;    // 0xE000 bytes each
	static const uint32_t PKT_DrawEnv = 0x2295BC0;     // DR_ENV (0x40)
	static const uint32_t PKT_DrawArea = 0x2293F50;    // draw area (0xC)
	static const uint32_t PKT_DrawOffset = 0x2294DA0;  // draw offset (0xC)
	static const uint32_t OFFSET_XY = 0x2296A2C;       // its position words
	static const uint32_t PKT_AltViewport = 0xD1B490;  // static (exe data)
	static const uint32_t DRAWENV = 0x1D969C8;         // 2 x 0x5C
	static const uint32_t STAGE_Entry1 = 0x1D989BD;    // draw entry 1 flags byte (0x1D98990 + 0x2C + 1); entry 0 at -0x2C

	static const uint32_t ORIG_RootTask = 0x5BE290;
	static const uint32_t ORIG_RestoreTask = 0x5BE2F0;
	static const uint32_t ORIG_MainTask = 0x5BCA20;
	static const uint32_t ORIG_WallTask = 0x5BCF40;
	static const uint32_t ORIG_ShardTask = 0x5BDB60;
	static const uint32_t ORIG_DustTask = 0x5BDCC0;
	static const uint32_t ORIG_DebrisTask = 0x5BDF60;
	static const uint32_t MODEL_Wall = 0xD19B74;
	static const uint32_t MODEL_Shard = 0xD19804;
	static const uint32_t MODEL_Debris = 0xD19ABC;
	static const uint32_t SEQ_Dust = 0xD19658;
	static const uint32_t CLIP_TABLE = 0xD1B3AC;       // 7 x 8 vertex-slot indices of the clipper
	static const void *const SOUND_Wall = (const void *)0xD1B3A4;
	static const int DUST_COUNT = 150, DEBRIS_COUNT = 100;
	static const int32_t WALL_TICKS = 0x3C, WALL_CLIPPED = 0x14, WALL_SHAKE = 0x1A, WALL_FADE = 0x30;
	static const int32_t SHARD_TICKS = 0x18, SHARD_GROW = 6, SHARD_FADE = 0x10;

	// engine functions not in fx_port.h / mag_common.h
	inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline void MatVec(const void *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const void *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }                    // computeCosine
	inline void NormalizeVectorFixed(const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void SetDrawEnvPrim(uint32_t prim, uint32_t env) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(prim, env); }                 // SSIGPU_SetDrawEnv
	inline void SetDrawAreaPrim(uint32_t prim, const int16_t *rect) { fn<void (__cdecl *)(uint32_t, const int16_t *)>(0x45C940)(prim, rect); } // sub_45C940
	inline void SetDrawOffsetPrim(uint32_t prim, uint32_t xy) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C9B0)(prim, xy); }                // sub_45C9B0
	inline void InsertPrimAltViewport(uint32_t bucket, uint32_t prim) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C8E0)(bucket, prim); }   // SSIGPU_InsertPrimAltViewport
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }                       // sub_56CD10
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }                             // Call_Bs_ParseCamera
	inline void SetProjectionH(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); }                                                     // Battle_SetProjectionH
	inline uint32_t RenderGeometry(uint32_t group, void *header, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(group, header, ot, mode, cursor); }
	// MAG_166_sub_64F0A0: prim model renderer of effect 166 (header 0x60 bytes: +0 model, +8 colour, +0xC fade, +0x1C mode,
	// +0x58 / +0x5C two draw-mode words)
	inline uint32_t ColumnRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64F0A0)(h, ot, mode, cursor); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
	inline void GteWriteControlReg(uint32_t value, int32_t reg) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D7F0)(value, reg); }
	inline void GteLightV0() { fn<void (__cdecl *)()>(0x4607E0)(); }   // GTE_MVMVA_LightV0_Bk
	inline void GteLightV1() { fn<void (__cdecl *)()>(0x4607F0)(); }
	inline void GteLightV2() { fn<void (__cdecl *)()>(0x460800)(); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root queue nodes (pool 0x2293F60, 2 x 0x10): RootTask, RestoreTask
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t done;     // +0x0E RestoreTask: the texture restore's done flag
	};
	struct Node // effect queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t delay;    // +0x0E shard: ticks before it shows
		int16_t x;        // +0x10 position (main: the wall point)
		int16_t y;        // +0x12
		int16_t z;        // +0x14
		int16_t w;        // +0x16 (CalculateCenterPosition leaves it: copied along with z)
		int16_t yaw;      // +0x18 shard
		int16_t pad1A;
		int16_t scale;    // +0x1C shard: x / z scale, wall: uniform scale
		int16_t pad1E;
		int16_t scale_y;  // +0x20 shard
		int16_t step;     // +0x22 shard: scale_y step, wall: rise step
	};
	struct Dust // 0x2294DB0, 150 x 0x18
	{
		int32_t flag;     // bit0 = live, 0 = free
		int16_t frame;    // +0x04 flipbook frame
		int16_t angle;    // +0x06 TransformCameraByShadowRotation angle
		int16_t pos[4];   // +0x08 x, y, z, w
		int16_t vel[3];   // +0x10 x, (y: -100, never used), z
		int16_t pad16;
	};
	struct Debris // 0x2293F90, 100 x 0x18
	{
		int32_t flag;     // bit0 = live, 0 = free
		int16_t life;     // +0x04
		int16_t scale;    // +0x06
		int16_t x, y, z;  // +0x08
		int16_t vy;       // +0x0E
		int16_t spin_x;   // +0x10
		int16_t ang_x;    // +0x12
		int16_t spin_z;   // +0x14
		int16_t ang_z;    // +0x16
	};
	struct Slot // clipper vertex slot: x, y, z, uv; (intersections:) crossing flag, plane distance
	{
		int16_t v[4];
		int32_t flag;
		int32_t d;
	};
	struct ClipHeader // 0x5BD1C0 draw header (0x1A8 bytes on the Field_Alloc scratch)
	{
		uint8_t pad00[0x1C];
		uint8_t *model;   // +0x1C
		uint8_t *verts;   // +0x20
		uint32_t flags;   // +0x24 bit 0x2000: +0x20 given
		uint8_t *list;    // +0x28 primitive list cursor
		uint32_t mask;    // +0x2C crossing edges (1, 2, 4)
		uint32_t code;    // +0x30 colour + raw flags of the primitive
		uint32_t uv0;     // +0x34 uv0 + clut
		uint32_t uv1;     // +0x38 uv1 + tpage
		uint32_t ot;      // +0x3C
		int32_t shift;    // +0x40 OTZ shift
		uint32_t cursor;  // +0x44 packet cursor
		uint8_t pad48[8];
		int32_t otz;      // +0x50
		int32_t flag;     // +0x54 GTE FLAG
		Slot slot[6];     // +0x58 v0, v1, v2, v0v1, v1v2, v2v0
		int16_t plane[4]; // +0xB8 normal (4.12) + distance
		uint8_t padC0[8];
		Slot *ptr[56];    // +0xC8 the slots of each crossing case (table 0xD1B3AC)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Wall nodes");
	static_assert(sizeof(Dust) == 0x18 && sizeof(Debris) == 0x18, "Wall particles");
	static_assert(sizeof(ClipHeader) == 0x1A8, "Wall clip header");

	inline Dust *Dusts() { return (Dust *)0x2294DB0; }
	inline Debris *Pieces() { return (Debris *)0x2293F90; }
	inline uint32_t rd32(const void *p) { return *(const uint32_t *)p; }
	inline Node *AddTask(uint32_t f) { return (Node *)AddTaskToQueue(QEffects(), f); }

	// the wall: clip-plane height (model space) and fade of counter c
	static int16_t WallPlane(int32_t c) { return (int16_t)(c * 2013 / 20 - 0x7DD); }
	static int32_t WallFade(int32_t c) { return c * 341 - 0x30 * 341; }
	static int32_t ShardFade(int32_t c) { return shl32(c - SHARD_FADE, 9); }
}
}

#ifdef FF8_FX_HELD
#include "mag189_wall_held.h"
#endif

namespace ff8fx
{
namespace wall189
{
	// ------------------------------------------------------------------
	// Plane-clipped prim model renderer (0x5BD1C0 and helpers: Sticky Web's 0x609FF0.. text relocated)
	// ------------------------------------------------------------------
	// 0x5BD180 (MAG_189_sub_5BD180): plane distance of point a: n[3] = -(a . n) >> 12
	static void PlaneDistance(const int16_t *a, int16_t *n)
	{
		uint32_t s = (uint32_t)((int32_t)a[2] * n[2]);
		s += (uint32_t)((int32_t)a[1] * n[1]);
		s += (uint32_t)((int32_t)a[0] * n[0]);
		n[3] = (int16_t)(-((int32_t)s >> 12));
	}

	// 0x5BD640 (sub_5BD640): crossing of edge a -> b with the plane (position and the uv bytes interpolated)
	static void Intersect(const int16_t *a, const int16_t *b, const int16_t *n, int16_t *out)
	{
		const int32_t dx = (int32_t)b[0] - a[0];
		const int32_t dy = (int32_t)b[1] - a[1];
		const int32_t dz = (int32_t)b[2] - a[2];
		int32_t den = (int32_t)((uint32_t)mul32(n[0], dx) + (uint32_t)mul32(n[1], dy) + (uint32_t)mul32(n[2], dz)) >> 12;
		if (den == 0) den = 1;
		const int32_t num = (int32_t)((uint32_t)shl32(n[3], 12) + (uint32_t)((int32_t)n[2] * a[2]) + (uint32_t)((int32_t)n[1] * a[1]) + (uint32_t)((int32_t)n[0] * a[0]));
		const int32_t t = -(num / den);
		out[0] = (int16_t)((mul32(t, dx) >> 12) + a[0]);
		out[1] = (int16_t)((mul32(t, dy) >> 12) + a[1]);
		out[2] = (int16_t)((mul32(t, dz) >> 12) + a[2]);
		const int32_t ua = (uint16_t)a[3], ub = (uint16_t)b[3];
		const int32_t lo = (mul32((ub & 0xFF) - (ua & 0xFF), t) >> 12) + (ua & 0xFF);
		const int32_t hi = (mul32((ub & 0xFF00) - (ua & 0xFF00), t) >> 12) + (ua & 0xFF00);
		out[3] = (int16_t)(uint16_t)((uint32_t)lo | ((uint32_t)hi & 0xFF00));
	}

	// 0x5BD460 (MAG_189_sub_5BD460): textured triangle (POLY_FT3)
	static void EmitTri(ClipHeader *h, const Slot *a, const Slot *b, const Slot *c)
	{
		uint32_t *pk = (uint32_t *)h->cursor;
		GteLoadV012(a, b, c);
		GteRTPT();
		pk[1] = h->code | 0x24000000;
		pk[3] = h->uv0;
		pk[5] = h->uv1;
		pk[0] = 0x07000000;
		GteReadSXY012(&pk[2], &pk[4], &pk[6]);
		GteAVSZ3();
		*(int16_t *)&pk[5] = b->v[3];
		*(int16_t *)&pk[3] = a->v[3];
		*(int16_t *)&pk[7] = c->v[3];
		GteReadOTZWord(&h->otz);
		InsertPrimAutoDepth(h->ot + (uint32_t)(h->otz >> (h->shift & 31)) * 4, pk);
		h->cursor = (uint32_t)pk + 0x20;
	}

	// 0x5BD760 (sub_5BD760): textured quad (POLY_FT4)
	static void EmitQuad(ClipHeader *h, const Slot *a, const Slot *b, const Slot *c, const Slot *d)
	{
		uint32_t *pk = (uint32_t *)h->cursor;
		GteLoadV012(a, b, c);
		GteRTPT();
		pk[1] = h->code | 0x2C000000;
		pk[3] = h->uv0;
		pk[5] = h->uv1;
		pk[0] = 0x09000000;
		GteReadSXY012(&pk[2], &pk[4], &pk[6]);
		GteLoadV0(d);
		GteRTPS();
		*(int16_t *)&pk[5] = b->v[3];
		*(int16_t *)&pk[3] = a->v[3];
		*(int16_t *)&pk[7] = c->v[3];
		*(int16_t *)&pk[9] = d->v[3];
		GteReadSXY2(&pk[8]);
		GteAVSZ4();
		GteReadOTZWord(&h->otz);
		InsertPrimAutoDepth(h->ot + (uint32_t)(h->otz >> (h->shift & 31)) * 4, pk);
		h->cursor = (uint32_t)pk + 0x28;
	}

	// 0x5BD500 (MAG_189_sub_5BD500): a triangle crossing the plane: the crossings, then the kept part
	static void ClipTri(ClipHeader *h)
	{
		if (h->slot[3].flag) Intersect(h->slot[0].v, h->slot[1].v, h->plane, h->slot[3].v);
		if (h->slot[4].flag) Intersect(h->slot[1].v, h->slot[2].v, h->plane, h->slot[4].v);
		if (h->slot[5].flag) Intersect(h->slot[2].v, h->slot[0].v, h->plane, h->slot[5].v);
		const uint32_t m = h->mask;
		Slot **p = &h->ptr[m * 8];
		switch (m) // (the original: jump table 0x5BD628, cases 1..6)
		{
		case 1: case 2: case 4:
			if (p[3]->d > 0) EmitTri(h, p[0], p[1], p[2]);
			else EmitTri(h, p[4], p[5], p[6]);
			break;
		case 3: case 5: case 6:
			if (p[3]->d > 0) EmitTri(h, p[0], p[1], p[2]);
			else EmitQuad(h, p[4], p[5], p[6], p[7]);
			break;
		default:
			break;
		}
	}

	// the plane distances of the three loaded vertices (GTE light matrix row 1 = plane normal, BK = distance), the crossing
	// edges, then clipped / whole / culled
	static void ClassifyAndDraw(ClipHeader *h)
	{
		GteLightV0();
		h->slot[3].flag = 0;
		GteReadDataReg(9, &h->slot[3].d);
		GteLightV1();
		h->slot[4].flag = 0;
		GteReadDataReg(9, &h->slot[4].d);
		GteLightV2();
		h->slot[5].flag = 0;
		GteReadDataReg(9, &h->slot[5].d);
		const int32_t d0 = h->slot[3].d, d1 = h->slot[4].d, d2 = h->slot[5].d;
		if (d0 != 0 && (d1 ^ d0) < 0) h->slot[3].flag = 1;
		if (d1 != 0 && (d1 ^ d2) < 0) h->slot[4].flag = 2;
		if (d2 != 0 && (d2 ^ d0) < 0) h->slot[5].flag = 4;
		h->mask = h->slot[5].flag | h->slot[3].flag | h->slot[4].flag;
		if (h->mask) ClipTri(h);
		else if ((d2 | d1 | d0) > 0) EmitTri(h, &h->slot[0], &h->slot[1], &h->slot[2]);
	}

	static void LoadVertex(ClipHeader *h, int slot, uint16_t index)
	{
		const uint32_t *v = (const uint32_t *)(h->verts + (uint32_t)index * 4);
		uint32_t *s = (uint32_t *)h->slot[slot].v;
		s[0] = v[0];
		s[1] = v[1];
	}

	// 0x5BD290 (MAG_189_sub_5BD290): triangle list (0x14-byte records)
	static void TriList(ClipHeader *h)
	{
		const int32_t count0 = *(const int32_t *)h->list;
		uint8_t *rec = h->list + 4;
		h->list = rec;
		if (count0 <= 0) { h->list = rec; return; }
		int32_t count = count0;
		do
		{
			LoadVertex(h, 0, *(const uint16_t *)(rec + 4));
			LoadVertex(h, 1, *(const uint16_t *)(rec + 6));
			LoadVertex(h, 2, *(const uint16_t *)(rec + 8));
			GteLoadV012(&h->slot[0], &h->slot[1], &h->slot[2]);
			GteRTPT();
			h->code = rd32(rec) & 0x2FFFFFF;
			h->uv0 = rd32(rec + 0xC);
			h->uv1 = rd32(rec + 0x10);
			h->slot[0].v[3] = (int16_t)h->uv0;
			h->slot[1].v[3] = (int16_t)h->uv1;
			h->slot[2].v[3] = (int16_t)(rd32(rec + 8) >> 16);
			GteReadFLAG(&h->flag);
			if (!(h->flag & 0x60000)) ClassifyAndDraw(h);
			rec += 0x14;
		} while (--count);
		h->list = rec;
	}

	// 0x5BD830 (MAG_189_sub_5BD830): quad list (0x18-byte records), drawn as two triangles
	static void QuadList(ClipHeader *h)
	{
		const int32_t count0 = *(const int32_t *)h->list;
		uint8_t *rec = h->list + 4;
		h->list = rec;
		if (count0 <= 0) { h->list = rec; return; }
		int32_t count = count0;
		do
		{
			LoadVertex(h, 0, *(const uint16_t *)(rec + 4));
			LoadVertex(h, 1, *(const uint16_t *)(rec + 6));
			LoadVertex(h, 2, *(const uint16_t *)(rec + 8));
			GteLoadV012(&h->slot[0], &h->slot[1], &h->slot[2]);
			GteRTPT();
			h->code = rd32(rec) & 0x2FFFFFF;
			const uint32_t uv0 = rd32(rec + 0xC);
			h->slot[0].v[3] = (int16_t)uv0;
			const uint32_t uv23 = rd32(rec + 0x14);
			h->slot[2].v[3] = (int16_t)uv23;
			h->uv0 = uv0;
			const uint32_t uv1 = rd32(rec + 0x10);
			h->uv1 = uv1;
			h->slot[1].v[3] = (int16_t)uv1;
			GteReadFLAG(&h->flag);
			if (!(h->flag & 0x60000))
			{
				ClassifyAndDraw(h);
				// second triangle: v3, v1, v2 (its FLAG is read but not tested)
				LoadVertex(h, 0, *(const uint16_t *)(rec + 0xA));
				LoadVertex(h, 1, *(const uint16_t *)(rec + 6));
				LoadVertex(h, 2, *(const uint16_t *)(rec + 8));
				GteLoadV012(&h->slot[0], &h->slot[1], &h->slot[2]);
				GteRTPT();
				h->slot[2].v[3] = (int16_t)uv23;
				h->slot[1].v[3] = (int16_t)h->uv1;
				h->slot[0].v[3] = (int16_t)(uv23 >> 16);
				GteReadFLAG(&h->flag);
				ClassifyAndDraw(h);
			}
			rec += 0x18;
		} while (--count);
		h->list = rec;
	}

	// 0x5BD1C0 (MAG_189_sub_5BD1C0): the model's triangles and quads on the kept side of the plane +0xB8
	static uint32_t ClipRender(ClipHeader *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		if (!(h->flags & 0x2000)) h->verts = h->model + 8;
		h->list = h->model + rd32(h->model);
		const uint32_t w = rd32(&h->plane[2]);
		GteWriteControlReg(rd32(&h->plane[0]), 8);
		GteWriteControlReg(w, 9);
		GteWriteControlReg((uint32_t)((int32_t)w >> 16), 0xD);
		for (int k = 0; k < 56; k++) h->ptr[k] = (Slot *)((uint8_t *)h->slot + (var<uint32_t>(CLIP_TABLE + 4 * k) << 4));
		h->cursor = cursor;
		h->ot = ot;
		h->shift = mode;
		h->list += 8;
		if (*(const int32_t *)h->list != 0) TriList(h);
		else h->list += 4;
		if (*(const int32_t *)h->list != 0)
		{
			QuadList(h);
			return h->cursor;
		}
		h->list += 4;
		return h->cursor;
	}

	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	// the wall model at the node's position (the draw of 0x5BCF40) (uniform scale +0x1C) through the camera: clipped at the model-space plane
	// height plane_y (counter < 0x14), then whole (fade from 0x30)
	static void WallDraw(const Node *n, int32_t c, int16_t plane_y, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = 0; // (4th word never written: not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->y;
		m.t[0] = n->x;
		const int32_t s = n->scale;
		const int32_t v[3] = { s, s, s };
		m.t[2] = n->z;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		if (c < WALL_CLIPPED)
		{
			ClipHeader *h = (ClipHeader *)FieldAlloc(0x1A8);
			h->model = (uint8_t *)MODEL_Wall;
			h->flags = 0;
			h->slot[0].v[0] = 0;
			h->slot[0].v[2] = 0;
			h->plane[0] = 0;
			h->plane[1] = (int16_t)0xF000;
			h->plane[2] = 0;
			h->slot[0].v[1] = plane_y;
			PlaneDistance(h->slot[0].v, h->plane);
			Cursor() = ClipRender(h, RenderOT(), 2, Cursor());
			FieldFree(0x1A8);
		}
		else
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x60);
			*(uint32_t *)h = MODEL_Wall;
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x1C) = 0;
			*(uint32_t *)(h + 0x58) = 0xE1000020;
			*(uint32_t *)(h + 0x5C) = 0xE1000220;
			if (c >= WALL_FADE)
			{
				*(uint32_t *)(h + 0x1C) = 0xF3;
				*(int32_t *)(h + 0xC) = fade;
			}
			Cursor() = ColumnRender(h, RenderOT(), 2, Cursor());
			FieldFree(0x60);
		}
	}

	// a shard: the draw of 0x5BDB60, prim model 0xD19804 at its point, yaw, scale (x / z +0x1C, y +0x20), fade from counter 0x10
	static void ShardDraw(const Node *n, int32_t c, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->yaw;
		ang[2] = 0; // (4th word never written: not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->x;
		m.t[1] = n->y;
		const int32_t v[3] = { n->scale, n->scale_y, n->scale };
		m.t[2] = n->z;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Shard;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c >= SHARD_FADE)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		FieldFree(0x58);
	}

	// a dust particle (the draw of 0x5BDCC0): flipbook frame at pos (header allocated by the task, 0xB4 bytes)
	static void DustDraw(uint8_t *h, int16_t frame, const int16_t *pos, int16_t angle)
	{
		*(int16_t *)(h + 4) = frame;
		TransformCameraByShadowRotation(pos, angle, -((int32_t)angle >> 4));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}

	// a debris piece (the draw of 0x5BDF60): prim model 0xD19ABC (header h, 0x58 bytes) with the work block w (0x88 bytes: angles +0, matrix +8,
	// the camera copy +0x28, the scale vector +0x48)
	static void DebrisDraw(uint8_t *h, uint8_t *w, const Debris *p)
	{
		*(int16_t *)w = p->ang_x;
		*(int16_t *)(w + 4) = p->ang_z;
		Mat4x3 *m = (Mat4x3 *)(w + 8);
		ComposeZYXRotationMatrix((const int16_t *)w, m);
		m->t[1] = p->y;
		m->t[2] = p->z;
		m->t[0] = p->x;
		int32_t *v = (int32_t *)(w + 0x48);
		v[2] = p->scale;
		v[1] = p->scale;
		v[0] = p->scale;
		Scale3DMatrix(m, v);
		ComposeAffineTransform((const Mat4x3 *)(w + 0x28), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
	}

	// 0x5BCCC0: the stage capture (see the module comment)
	static void Capture(const int16_t *point)
	{
		int16_t rect[4] = { 0x180, 0x100, 0x100, 0x100 };
		const int32_t tx = -1 - (int32_t)point[0] / 18;
		const int32_t ty = (int32_t)point[2] / 4 / 2 + 1;
		int16_t ang[4];
		ang[0] = 0x400;
		ang[1] = 0;
		ang[2] = 0; // (4th word never written: not read)
		Mat4x3 m;
		m.t[0] = tx;
		m.t[1] = ty;
		m.t[2] = 0x7FFF;
		BuildRotationMatrixFromAngles(ang, &m);
		int32_t v[3];
		v[1] = 0x1000;
		v[0] = 0x1000000 / (0x1162000 / (int32_t)rect[2]);
		v[2] = 0x1000000 / (0xAF0000 / (int32_t)rect[3]);
		Scale3DMatrix(&m, v);
		SetDrawEnvPrim(PKT_DrawEnv, DRAWENV + 0x5C * ((uint32_t)DisplayBuffer() ^ 1));
		InsertPrimAutoDepth(RenderOT(), (void *)PKT_DrawEnv);
		int32_t ofx, ofy;
		GetScreenOffset(&ofx, &ofy);
		SetScreenOffset((int32_t)rect[2] / 2, (int32_t)rect[3] / 2);
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
		for (uint32_t e = STAGE_Entry1; e >= STAGE_Entry1 - 0x2C; e -= 0x2C)
		{
			const uint8_t f = var<uint8_t>(e);
			if (!(f & 1) || !(f & 2)) continue;
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(e + 0x27);
			*(uint16_t *)(h + 0x24) = var<uint16_t>(e + 1);
			BuildBoneMatricesFromPose((void *)(e + 0x13));
			ComputeBonesWorldMatrices((void *)(e + 0x13), &m);
			Cursor() = RenderGeometry(e + 3, h, RenderOT(), 0x10, Cursor());
		}
		SetProjectionH(var<int16_t>(0x1D8E038));
		FieldFree(0x2C);
		SetScreenOffset(ofx, ofy);
		InsertPrimAltViewport(RenderOT(), PKT_AltViewport);
		SetDrawAreaPrim(PKT_DrawArea, rect);
		InsertPrimAutoDepth(RenderOT(), (void *)PKT_DrawArea);
		var<int16_t>(OFFSET_XY) = rect[0];
		var<int16_t>(OFFSET_XY + 2) = rect[1];
		SetDrawOffsetPrim(PKT_DrawOffset, OFFSET_XY);
		InsertPrimAutoDepth(RenderOT(), (void *)PKT_DrawOffset);
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// Debris (0x5BDF60)
	static uint32_t __cdecl DebrisTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(held_begin(ORIG_DebrisTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *w = (uint8_t *)FieldAlloc(0x88);
		memcpy(w + 0x28, &Camera(), 0x20);
		*(uint32_t *)h = MODEL_Debris;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		int32_t alive = 0;
		*(int16_t *)(w + 2) = 0;
		for (Debris *p = Pieces(); p < Pieces() + DEBRIS_COUNT; p++)
		{
			if (!(*(const uint8_t *)&p->flag & 1)) continue;
			// 30 fps layer: see mag189_wall_held.inc
			FX_HELD(held_note_part(p);)
			DebrisDraw(h, w, p);
			if (!(UpdateFlags() & 0x201))
			{
				p->life++;
				if (p->life >= 0x10) p->flag = 0;
				else
				{
					if (p->life >= 0xC) p->scale = (int16_t)(p->scale - (int16_t)(p->scale >> 3));
					p->y = (int16_t)(p->y + p->vy);
					p->vy = (int16_t)(p->vy - (int16_t)(p->vy >> 3));
					p->ang_x = (int16_t)(p->ang_x + p->spin_x);
					p->ang_z = (int16_t)(p->ang_z + p->spin_z);
					alive++;
				}
			}
			// 30 fps layer: see mag189_wall_held.inc
			FX_HELD(held_note_part_next(p);)
		}
		FieldFree(0x88);
		FieldFree(0x58);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter >= 0 && n->counter <= 0x18)
		{
			for (int k = 0; k < 3; k++)
			{
				int idx = 0;
				while (idx < DEBRIS_COUNT && Pieces()[idx].flag != 0) idx++;
				if (idx >= DEBRIS_COUNT) break;
				Debris *p = &Pieces()[idx];
				p->flag = 1;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x2C0 + 0x80);
				*(uint32_t *)&p->x = rd32(&n->x);
				*(uint32_t *)&p->z = rd32(&n->z);
				const int32_t a = shl32(CrtRand() % 24, 12) / 24;
				p->x = (int16_t)(p->x + (int16_t)(mul32(ComputeCos(a), 1800) >> 12));
				p->y = (int16_t)-(CrtRand() % 200);
				const int32_t s = mul32(ComputeSin(a), 1800) >> 12;
				p->z = (int16_t)(p->z + (int16_t)(mul32(s, 1280) >> 12));
				p->vy = (int16_t)(-25 - CrtRand() % 200);
				int16_t sx = (int16_t)(CrtRand() % 120 + 60);
				if (sx & 1) sx = (int16_t)-sx;
				p->spin_x = sx;
				int16_t sz = (int16_t)(CrtRand() % 60 + 30);
				if (sz & 1) sz = (int16_t)-sz;
				p->spin_z = sz;
				p->ang_x = (int16_t)(CrtRand() % 0x1000);
				p->ang_z = (int16_t)(p->ang_x >> 1);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// Dust (0x5BDCC0)
	static uint32_t __cdecl DustTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(held_begin(ORIG_DustTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Dust;
		*(int16_t *)(h + 0x24) = 8;
		for (Dust *p = Dusts(); p < Dusts() + DUST_COUNT; p++)
		{
			if (!(*(const uint8_t *)&p->flag & 1)) continue;
			// 30 fps layer: see mag189_wall_held.inc
			FX_HELD(held_note_part(p);)
			DustDraw(h, p->frame, p->pos, p->angle);
			if (!(UpdateFlags() & 0x201))
			{
				p->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) p->flag = 0;
				else
				{
					p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
					p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
					p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 4));
					p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 4));
					alive++;
				}
			}
			// 30 fps layer: see mag189_wall_held.inc
			FX_HELD(held_note_part_next(p);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter >= 0 && n->counter <= 0x38)
		{
			for (int k = 0; k < 4; k++)
			{
				int idx = 0;
				while (idx < DUST_COUNT && Dusts()[idx].flag != 0) idx++;
				if (idx >= DUST_COUNT) break;
				Dust *p = &Dusts()[idx];
				p->flag = 1;
				p->frame = 0;
				p->angle = (int16_t)(CrtRand() % 0xA00 + 0xA00);
				*(uint32_t *)&p->pos[0] = rd32(&n->x);
				*(uint32_t *)&p->pos[2] = rd32(&n->z);
				const int32_t a = shl32(CrtRand() % 24, 12) / 24;
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(mul32(ComputeCos(a), 1800) >> 12));
				const int32_t s = mul32(ComputeSin(a), 1800) >> 12;
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(mul32(s, 1280) >> 12));
				int32_t v[3];
				v[1] = 0;
				v[0] = (int32_t)p->pos[0] - n->x;
				v[2] = (int32_t)p->pos[2] - n->z;
				NormalizeVectorFixed(v, v);
				const int32_t r = CrtRand() % 25 + 20;
				p->vel[0] = (int16_t)(mul32(v[0], r) >> 12);
				p->vel[1] = -100;
				p->vel[2] = (int16_t)(mul32(v[2], r) >> 12);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// Shard (0x5BDB60)
	static uint32_t __cdecl ShardTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->delay > 0)
		{
			if (UpdateFlags() & 0x201) return 0;
			n->delay--;
			return 0;
		}
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(held_begin(ORIG_ShardTask, n);)
		ShardDraw(n, n->counter, ShardFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter < SHARD_GROW) n->scale_y = (int16_t)(n->scale_y + n->step);
		n->counter++;
		return n->counter >= SHARD_TICKS ? TASK_END : 0;
	}

	// Wall (0x5BCF40)
	static uint32_t __cdecl WallTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(held_begin(ORIG_WallTask, n);)
		WallDraw(n, n->counter, WallPlane(n->counter), WallFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t c = n->counter;
		if (c < WALL_CLIPPED) n->y = (int16_t)(n->y - n->step);
		else n->y = 0;
		if (c < WALL_SHAKE)
		{
			int32_t v = mul32(ComputeSin(shl32(c, 11) / 26), 40) >> 12;
			if (!(c & 1)) v = -v;
			CameraShakeY() = (int16_t)(CameraShakeY() + (int16_t)v);
		}
		n->counter = (int16_t)(c + 1);
		return n->counter >= WALL_TICKS ? TASK_END : 0;
	}

	// Main (0x5BCA20)
	static uint32_t __cdecl MainTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			CalculateCenterPosition(Ctx()->target_mask, &n->x);
			int16_t v[4];
			v[0] = 0;
			v[1] = 0;
			v[2] = (int16_t)0xFA24; // -1500 (4th word never written: not read)
			MatVec(Entity(FirstTarget()) + 0x40, v, v);
			n->x = (int16_t)(n->x + v[0]);
			n->y = (int16_t)(n->y + v[1]);
			n->z = (int16_t)(n->z + v[2]);
			Capture(&n->x);
		}
		if (n->counter == 1)
		{
			const int32_t base = CrtRand() % 0x1000;
			int32_t i = 0, acc = 0;
			for (int32_t k = 0x20; k > 8; k--)
			{
				Node *s = AddTask(ORIG_ShardTask);
				s->counter = 0;
				const int32_t r = CrtRand();
				*(uint32_t *)&s->x = rd32(&n->x);
				s->delay = (int16_t)(r % k + i);
				*(uint32_t *)&s->z = rd32(&n->z);
				const int32_t a = (acc / 24 + base) & 0xFFF;
				s->x = (int16_t)(s->x + (int16_t)(mul32(ComputeCos(a), 2300) >> 12));
				const int32_t sn = mul32(ComputeSin(a), 2300) >> 12;
				s->yaw = (int16_t)(0xC00 - a);
				s->z = (int16_t)(s->z + (int16_t)(mul32(sn, 1280) >> 12));
				s->scale = (int16_t)(CrtRand() % 0x1000 + 0x800);
				s->scale_y = 0;
				s->step = 0x5AA;
				i++;
				acc += 0x1000;
			}
		}
		if (n->counter == 0xE)
		{
			Node *d = AddTask(ORIG_DustTask);
			*(uint32_t *)&d->x = rd32(&n->x);
			d->counter = 0;
			*(uint32_t *)&d->z = rd32(&n->z);
			if (n->counter == 0xE)
			{
				Node *b = AddTask(ORIG_DebrisTask);
				*(uint32_t *)&b->x = rd32(&n->x);
				b->counter = 0;
				*(uint32_t *)&b->z = rd32(&n->z);
			}
		}
		if (n->counter == 0x10)
		{
			Node *w = AddTask(ORIG_WallTask);
			w->counter = 0;
			w->x = n->x;
			w->y = 0x7DD;
			w->z = n->z;
			w->scale = 0x1000;
			w->step = 0x64;
		}
		if (n->counter == 0x41) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (n->counter == 1) BdPlaySE(SOUND_Wall, 0, 0x80);
		n->counter++;
		return n->counter > 0x46 ? TASK_END : 0;
	}

	// RestoreTask (0x5BE2F0)
	static uint32_t __cdecl RestoreTask(TaskNode *n_)
	{
		RootNode *t = (RootNode *)n_;
		if (t->counter == 1) TextureRestoreTask((void *)ARENA_EVEN, &t->done);
		const int16_t done = t->done;
		t->counter++;
		return done ? TASK_END : 0;
	}

	// Root (0x5BE290)
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(held_note_root();)
		Cursor() = ARENA_ODD;
		if (!(*(const uint8_t *)&r->counter & 1)) Cursor() = ARENA_EVEN;
		const int a = ExecuteTaskQueue(QEffects());
		if (a == 0)
		{
			RootNode *t = (RootNode *)AddTaskToQueue(QRoot(), ORIG_RestoreTask);
			t->counter = 0;
			t->done = 0;
		}
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag189_wall()
	{
		register_port(wall189::ORIG_RootTask, (void *)wall189::RootTask, "189 RootTask", 189);
		register_port(wall189::ORIG_RestoreTask, (void *)wall189::RestoreTask, "189 RestoreTask", 189);
		register_port(wall189::ORIG_MainTask, (void *)wall189::MainTask, "189 MainTask", 189);
		register_port(wall189::ORIG_WallTask, (void *)wall189::WallTask, "189 WallTask", 189);
		register_port(wall189::ORIG_ShardTask, (void *)wall189::ShardTask, "189 ShardTask", 189);
		register_port(wall189::ORIG_DustTask, (void *)wall189::DustTask, "189 DustTask", 189);
		register_port(wall189::ORIG_DebrisTask, (void *)wall189::DebrisTask, "189 DebrisTask", 189);
		// 30 fps layer: see mag189_wall_held.inc
		FX_HELD(register_mag189_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag189_wall_held.inc"
#endif
