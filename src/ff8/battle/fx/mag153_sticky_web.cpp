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

// Effect 153: Sticky Web (enemy attacks 100 / 169 of kernel.bin, Caterchipillar; MAG_153_*).
//
// Structure (setup MAG_153_STICKY_WEB 0x609640, file loader 0x609620 = the texture file named at
// 0xDBE364; the setup keeps the context, the caster slot and the first target's slot, starts camera
// animation 0xDBAF3C and queues the TIM). All tasks live in one queue 0x243AFC0 (pool 100 x 0x24):
//   RootTask (0x60AF50) - alternates the packet arena (0x2443E6C on odd counters, else 0x243BE6C),
//     runs the effect queue, ends when it is empty.
//   MasterTask (0x6096F0) - every tick (also in draw-only mode) the caster's effect bone 0xE
//     position (y - 100) into 0x243BDF8; counter 0: the target's default effect position into
//     0x243BDE0 (y = middle of entity +0x36 / +0x3C); 1: sound 0xDBE27C; 10: particles 1; 13: the
//     silk thread and the web; 48: particles 2; 50: the burst; 55: damage; ends after 60.
//   ThreadTask (0x609930) - draws THEN updates: the thread's anchor (target position + the caster's
//     facing vector scaled by sin(angle) * 600) into 0x243BE00 and a 5-entry history ring
//     0x243BE08 (caster position, anchor); a G2 line caster -> anchor; from 2 history points on,
//     two splines (0x571620 / 0x571690, 16 samples each) through the caster / anchor histories
//     drawn as 14 pairs of Gouraud quads (depth-cued colour along the strand). Update: angle and
//     anchor height move by +0x14 / +0x16 until counter 25; ends at 30.
//   WebTask (0x609DF0) - draws THEN updates: prim model 0xDBB33C at the target (random yaw, scale
//     0xC00); counters 0..29 through the module's own plane-clipped renderer (0x609FF0: the part
//     of the model below the thread's anchor height), 30..44 whole (fade from 37); ends at 45.
//   Particles1Task (0x60A990) / Particles2Task (0x60AC10) - draw THEN update the shared particle
//     array 0x243A560 (100 x 0x18, type bit 1 / 2): flipbook 0xDBB190 at each particle
//     (frame++, dies when the flipbook ends). Type 1 (counters 0..22, one per tick at the caster,
//     flying towards the target), type 2 (counters 0..8, eight per tick around the target,
//     rising with decaying speed). A task ends when it is past counter 3 and has no live particle.
//   BurstTask (0x60AE10) - draws THEN updates: prim model 0xDBDBF4 at the target, scale growing
//     by a decaying step, fade from counter 6; ends at 14.
// Every task but the master tests the draw-only flags (battle_to_update_flags 0x201) after drawing.
// Module globals: 0x243A4D8..0x244BE70 (spline points, slots, root pool/queue, particles, spline
// samples, effect queue + pool, positions, context, ring, the two packet arenas, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace web153
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x243A53C); }
	inline CastContext *&Ctx() { return var<CastContext *>(0x243BDE8); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x243BDEC); }
	inline int16_t *TargetPos() { return (int16_t *)0x243BDE0; }   // x, y, z (+ a word)
	inline int16_t *CasterPos() { return (int16_t *)0x243BDF8; }   // caster's bone 0xE, y - 100
	inline int16_t *Anchor() { return (int16_t *)0x243BE00; }      // thread anchor
	inline uint32_t &Cursor() { return var<uint32_t>(0x244BE6C); }
	inline TaskQueue *QEffects() { return (TaskQueue *)0x243AFC0; } // pool 0x243AFD0, 100 x 0x24
	static const uint32_t PTS_A = 0x243A4D8, PTS_B = 0x243A508;   // spline points (5 x 8 bytes)
	static const uint32_t SPL_A = 0x243AEC0, SPL_B = 0x243AF40;   // spline samples (16 x 8 bytes)
	static const uint32_t ARENA_A = 0x2443E6C, ARENA_B = 0x243BE6C; // 0x8000 bytes each

	static const uint32_t ORIG_RootTask = 0x60AF50;
	static const uint32_t ORIG_MasterTask = 0x6096F0;
	static const uint32_t ORIG_ThreadTask = 0x609930;
	static const uint32_t ORIG_WebTask = 0x609DF0;
	static const uint32_t ORIG_Particles1Task = 0x60A990;
	static const uint32_t ORIG_Particles2Task = 0x60AC10;
	static const uint32_t ORIG_BurstTask = 0x60AE10;
	static const uint32_t MODEL_Web = 0xDBB33C;
	static const uint32_t MODEL_Burst = 0xDBDBF4;
	static const uint32_t SEQ_Particle = 0xDBB190;
	static const uint32_t CLIP_TABLE = 0xDBE284;   // 7 x 8 vertex-slot indices of the clipper
	static const void *const SOUND_Web = (const void *)0xDBE27C;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline void SplineSetup(int32_t n, uint32_t points, void *work) { fn<void (__cdecl *)(int32_t, uint32_t, void *)>(0x571620)(n, points, work); }             // sub_571620
	inline void SplineEval(int32_t n, void *work, uint32_t out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, uint32_t, int32_t)>(0x571690)(n, work, out, t); } // sub_571690
	inline void GteSetFarColorB(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DDA0)(r, g, b); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }
	inline void GteWriteControlReg(uint32_t value, int32_t reg) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D7F0)(value, reg); }
	inline void GteLightV0() { fn<void (__cdecl *)()>(0x4607E0)(); }   // GTE_MVMVA_LightV0_Bk
	inline void GteLightV1() { fn<void (__cdecl *)()>(0x4607F0)(); }
	inline void GteLightV2() { fn<void (__cdecl *)()>(0x460800)(); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad;
	};
	struct Node // effect queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t pad0E;
		int16_t f10;     // +0x10 thread: angle / web, burst: x
		int16_t f12;     // +0x12 thread: anchor height / y
		int16_t f14;     // +0x14 thread: angle step / z
		int16_t f16;     // +0x16 thread: height step / (target position word 3)
		int16_t f18;     // +0x18 yaw
		int16_t pad1A;
		int16_t f1C;     // +0x1C scale
		int16_t f1E;     // +0x1E burst: scale step
		uint8_t pad20[4];
	};
	struct Particle // 0x243A560, 100 x 0x18
	{
		int32_t type;    // bit0 = particles 1, bit1 = particles 2, 0 = free
		int16_t frame;   // flipbook frame
		int16_t angle;   // TransformCameraByShadowRotation angle
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad;
	};
	struct RingEntry // 0x243BE08, 5 x 0x14: thread history
	{
		int32_t valid;
		uint32_t p0, p1; // caster position (x, y | z, w)
		uint32_t q0, q1; // anchor
	};
	struct Slot // clipper vertex slot: x, y, z, uv; (intersections:) crossing flag, plane distance
	{
		int16_t v[4];
		int32_t flag;
		int32_t d;
	};
	struct ClipHeader // 0x609FF0 draw header (0x1A8 bytes on the scratch stack)
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
		Slot *ptr[56];    // +0xC8 the slots of each crossing case (table 0xDBE284)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Sticky Web nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(RingEntry) == 0x14, "Sticky Web records");
	static_assert(sizeof(ClipHeader) == 0x1A8, "Sticky Web clip header");

	inline Particle *Parts() { return (Particle *)0x243A560; }
	inline RingEntry *Ring() { return (RingEntry *)0x243BE08; }
	inline uint32_t rd32(const void *p) { return *(const uint32_t *)p; }
	inline Node *AddTask(uint32_t fn) { return (Node *)AddTaskToQueue(QEffects(), fn); }
}
}

#ifdef FF8_FX_HELD
#include "mag153_sticky_web_held.h"
#endif

namespace ff8fx
{
namespace web153
{
	// ------------------------------------------------------------------
	// Plane-clipped prim model renderer (0x609FF0 and helpers)
	// ------------------------------------------------------------------
	// MAG_153_sub_609FB0: plane distance of point a: n[3] = -(a . n) >> 12
	static void PlaneDistance(const int16_t *a, int16_t *n)
	{
		uint32_t s = (uint32_t)((int32_t)a[2] * n[2]);
		s += (uint32_t)((int32_t)a[1] * n[1]);
		s += (uint32_t)((int32_t)a[0] * n[0]);
		n[3] = (int16_t)(-((int32_t)s >> 12));
	}

	// sub_60A470: crossing of edge a -> b with the plane (position and the uv bytes interpolated)
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

	// MAG_153_sub_60A290: textured triangle (POLY_FT3)
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

	// sub_60A590: textured quad (POLY_FT4)
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

	// MAG_153_sub_60A330: a triangle crossing the plane: the crossings, then the kept part
	static void ClipTri(ClipHeader *h)
	{
		if (h->slot[3].flag) Intersect(h->slot[0].v, h->slot[1].v, h->plane, h->slot[3].v);
		if (h->slot[4].flag) Intersect(h->slot[1].v, h->slot[2].v, h->plane, h->slot[4].v);
		if (h->slot[5].flag) Intersect(h->slot[2].v, h->slot[0].v, h->plane, h->slot[5].v);
		const uint32_t m = h->mask;
		Slot **p = &h->ptr[m * 8];
		switch (m)
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

	// the plane distances of the three loaded vertices (GTE light matrix row 1 = plane normal,
	// BK = distance), the crossing edges, then clipped / whole / culled
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

	// MAG_153_sub_60A0C0: triangle list (0x14-byte records)
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

	// MAG_153_sub_60A660: quad list (0x18-byte records), drawn as two triangles
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

	// MAG_153_sub_609FF0: the model's triangles and quads on the kept side of the plane +0xB8
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
	// Draw helpers shared by the tasks
	// ------------------------------------------------------------------
	// MAG_153_sub_609D60: semi-transparent G2 line a -> b
	static void DrawLine(const int16_t *a, const int16_t *b, uint32_t colour)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		uint32_t *pk = (uint32_t *)Cursor();
		GteLoadV012(a, b, b);
		GteRTPT();
		pk[1] = colour;
		pk[3] = colour;
		pk[0] = 0x04000000;
		((uint8_t *)pk)[7] = 0x52;
		GteReadSXY012(&pk[2], &pk[4], &pk[4]);
		GteAVSZ3();
		int32_t otz;
		GteReadOTZWord(&otz);
		InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + (uint32_t)(otz >> 2) * 4 + 0x44, pk);
		Cursor() = (uint32_t)pk + 0x14;
	}

	// the thread strands: 14 segments between the samples A[k], A[k+1], B[k], B[k+1], each a
	// Gouraud quad (colour depth-cued along the strand) and a quad black at the A side
	static void DrawStrips(uint8_t *h, const uint8_t *a, const uint8_t *b)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColorB(0, 0, 0);
		uint32_t *q = (uint32_t *)Cursor();
		*(uint32_t *)(h + 0x14) = 0x3A203040;
		*(uint32_t *)(h + 0x1C) = 0x3A203040;
		uint32_t *p2 = q + 9;
		q += 1;
		int32_t acc = 0;
		for (int32_t off = 0; off < 0x70; off += 8, acc += 0x1000)
		{
			*(uint32_t *)(h + 0x10) = *(uint32_t *)(h + 0x14);
			q[-1] = 0x08000000;
			GteLoadV012(a + off, a + off + 8, b + off);
			GteRTPT();
			GteReadFLAG(h + 0xC);
			if (*(uint32_t *)(h + 0xC) & 0x60000) continue;
			GteReadSXY012(&q[1], &q[3], &q[5]);
			GteLoadV0(b + off + 8);
			GteRTPS();
			GteReadSXY2(&q[7]);
			GteAVSZ4();
			GteReadOTZWord(h + 8);
			GteSetIR0(acc / 14);
			GteLoadRGBC(h + 0x1C);
			GteDPCS();
			GteStoreRGB2(h + 0x14);
			const uint32_t c0 = *(uint32_t *)(h + 0x10), c1 = *(uint32_t *)(h + 0x14);
			q[4] = c0;
			q[0] = c0;
			q[6] = c1;
			q[2] = c1;
			const uint32_t bucket = var<uint32_t>(0x1D8E04C) + (uint32_t)(*(int32_t *)(h + 8) >> 2) * 4 + 0x44;
			InsertPrimAutoDepth(bucket, q - 1);
			p2[5] = c0;
			p2[3] = 0x3A000000;
			p2[1] = 0x3A000000;
			p2[7] = c1;
			p2[4] = q[3];
			p2[2] = q[1];
			p2[6] = q[5];
			p2[8] = q[7];
			p2[0] = 0x08000000;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + (uint32_t)(*(int32_t *)(h + 8) >> 2) * 4 + 0x44, p2);
			q += 0x12;
			p2 += 0x12;
		}
		Cursor() = (uint32_t)p2;
	}

	// the matrix of the web / burst models: yaw, uniform scale, at +0x10, through the camera
	static void ModelMatrix(const Node *n)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->f10;
		m.t[1] = n->f12;
		const int32_t s = n->f1C;
		const int32_t v[3] = { s, s, s };
		m.t[2] = n->f14;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// the web model: clipped at the anchor height (counter < 30), then whole (fade from 37)
	static void WebDraw(const Node *n, int32_t c, int16_t anchor_y, int32_t fade)
	{
		ModelMatrix(n);
		if (c < 0x1E)
		{
			ClipHeader *h = (ClipHeader *)FieldAlloc(0x1A8);
			const int32_t y = n->f12;
			h->flags = 0;
			const int32_t s = n->f1C;
			h->slot[0].v[0] = 0;
			h->model = (uint8_t *)MODEL_Web;
			const int32_t d = (int32_t)anchor_y - y;
			const int32_t inv = 0x1000000 / s;
			h->plane[1] = 0x1000;
			h->slot[0].v[2] = 0;
			h->plane[0] = 0;
			h->plane[2] = 0;
			h->slot[0].v[1] = (int16_t)(mul32(d, inv) >> 12);
			PlaneDistance(h->slot[0].v, h->plane);
			Cursor() = ClipRender(h, RenderOT(), 2, Cursor());
			FieldFree(0x1A8);
		}
		else
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x58);
			*(uint32_t *)h = MODEL_Web;
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x1C) = 0;
			if (c >= 0x25)
			{
				*(uint32_t *)(h + 0x1C) = 0xF3;
				*(int32_t *)(h + 0xC) = fade;
			}
			Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
			FieldFree(0x58);
		}
	}
	static int32_t WebFade(int32_t c) { return shl32(c - 0x25, 9); }

	// the burst model (fade from counter 6)
	static void BurstDraw(const Node *n, int32_t c, int32_t fade)
	{
		ModelMatrix(n);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Burst;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c >= 6)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		FieldFree(0x58);
	}
	static int32_t BurstFade(int32_t c) { return shl32(c - 6, 9); }

	// one particle: flipbook frame at pos (header allocated by the task, 0xB4 bytes)
	static void ParticleDraw(uint8_t *h, int16_t frame, const int16_t *pos, int16_t angle)
	{
		*(int16_t *)(h + 4) = frame;
		TransformCameraByShadowRotation(pos, angle, -((int32_t)angle >> 4));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// Particles type 1 (0x60A990)
	static uint32_t __cdecl Particles1Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_begin(ORIG_Particles1Task, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 0x24) = 8;
		for (Particle *e = Parts(); e < Parts() + 100; e++)
		{
			if (!(*(const uint8_t *)&e->type & 1)) continue;
			// 30 fps layer: see mag153_sticky_web_held.inc
			FX_HELD(held_note_part(e);)
			ParticleDraw(h, e->frame, e->pos, e->angle);
			if (!(UpdateFlags() & 0x201))
			{
				e->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) e->type = 0;
				else
				{
					e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
					e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
					e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
					alive++;
				}
			}
			// 30 fps layer: see mag153_sticky_web_held.inc
			FX_HELD(held_note_part_next(e);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter >= 0 && n->counter <= 0x16)
		{
			int32_t v[3];
			v[0] = (int32_t)TargetPos()[0] - CasterPos()[0];
			v[1] = 0;
			v[2] = (int32_t)TargetPos()[2] - CasterPos()[2];
			NormalizeVector(v, v);
			for (int k = 0; k < 1; k++)
			{
				int idx = 0;
				while (idx < 100 && Parts()[idx].type != 0) idx++;
				if (idx >= 100) break;
				Particle *e = &Parts()[idx];
				e->type = 1;
				e->frame = 0;
				e->angle = (int16_t)(CrtRand() % 0x500 + 0xB00);
				*(uint32_t *)&e->pos[0] = rd32(&CasterPos()[0]);
				*(uint32_t *)&e->pos[2] = rd32(&CasterPos()[2]);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 50 - 25));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 50 - 25));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 50 - 25));
				const int32_t r = CrtRand() % 5 + 5;
				e->vel[0] = (int16_t)(mul32(r, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(r, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(r, v[2]) >> 12);
				e->vel[1] = (int16_t)(e->vel[1] + (CrtRand() % 20 - 10));
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// Particles type 2 (0x60AC10)
	static uint32_t __cdecl Particles2Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_begin(ORIG_Particles2Task, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 0x24) = 8;
		for (Particle *e = Parts(); e < Parts() + 100; e++)
		{
			if (!(*(const uint8_t *)&e->type & 2)) continue;
			// 30 fps layer: see mag153_sticky_web_held.inc
			FX_HELD(held_note_part(e);)
			ParticleDraw(h, e->frame, e->pos, e->angle);
			if (!(UpdateFlags() & 0x201))
			{
				e->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) e->type = 0;
				else
				{
					e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
					e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 4));
					alive++;
				}
			}
			// 30 fps layer: see mag153_sticky_web_held.inc
			FX_HELD(held_note_part_next(e);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter >= 0 && n->counter <= 8)
		{
			for (int k = 0; k < 8; k++)
			{
				int idx = 0;
				while (idx < 100 && Parts()[idx].type != 0) idx++;
				if (idx >= 100) break;
				Particle *e = &Parts()[idx];
				e->type = 2;
				e->frame = 0;
				e->angle = (int16_t)(CrtRand() % 0x900 + 0xD00);
				*(uint32_t *)&e->pos[0] = rd32(&TargetPos()[0]);
				*(uint32_t *)&e->pos[2] = rd32(&TargetPos()[2]);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 1000 - 500));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 1000 - 500));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 1000 - 500));
				e->vel[1] = (int16_t)(-10 - CrtRand() % 60);
			}
		}
		n->counter++;
		if (n->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// Burst (0x60AE10)
	static uint32_t __cdecl BurstTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_begin(ORIG_BurstTask, n);)
		BurstDraw(n, n->counter, BurstFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int32_t)step / 7);
		n->counter++;
		return n->counter >= 0xE ? TASK_END : 0;
	}

	// Web (0x609DF0)
	static uint32_t __cdecl WebTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_begin(ORIG_WebTask, n);)
		WebDraw(n, n->counter, Anchor()[1], WebFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 0x2D ? TASK_END : 0;
	}

	// Silk thread (0x609930)
	static uint32_t __cdecl ThreadTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_begin(ORIG_ThreadTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x40);
		int16_t *v = (int16_t *)(h + 0x20);
		v[1] = 0;
		v[2] = 0;
		v[0] = (int16_t)0xF000;
		MatrixMulVector((const Mat4x3 *)(Entity(CasterSlot()) + 0x40), v, v);
		const int32_t k = mul32(ComputeSin(n->f10), 600) >> 12;
		Anchor()[0] = (int16_t)((mul32(v[0], k) >> 12) + TargetPos()[0]);
		Anchor()[1] = n->f12;
		Anchor()[2] = (int16_t)((mul32(v[2], k) >> 12) + TargetPos()[2]);
		int32_t slot = n->counter % 5;
		RingEntry &r = Ring()[slot];
		r.valid = 1;
		r.p0 = rd32(&CasterPos()[0]);
		r.p1 = rd32(&CasterPos()[2]);
		r.q0 = rd32(&Anchor()[0]);
		r.q1 = rd32(&Anchor()[2]);
		DrawLine(CasterPos(), Anchor(), 0x808080);
		const int32_t cnt = n->counter >= 0x19 ? 0x1E - n->counter : 5;
		int32_t i = 0;
		while (i < cnt)
		{
			const RingEntry &e = Ring()[slot];
			if (!e.valid) break;
			((uint32_t *)PTS_A)[i * 2] = e.p0;
			((uint32_t *)PTS_A)[i * 2 + 1] = e.p1;
			((uint32_t *)PTS_B)[i * 2] = e.q0;
			((uint32_t *)PTS_B)[i * 2 + 1] = e.q1;
			slot--;
			if (slot < 0) slot = 4;
			i++;
		}
		if (i > 1)
		{
			void *work = FieldAlloc(0x190);
			SplineSetup(i, PTS_A, work);
			for (int32_t j = 0, t = 0; j < 16; j++, t += 0x1000) SplineEval(i, work, SPL_A + j * 8, t / 15);
			SplineSetup(i, PTS_B, work);
			for (int32_t j = 0, t = 0; j < 16; j++, t += 0x1000) SplineEval(i, work, SPL_B + j * 8, t / 15);
			FieldFree(0x190);
			DrawStrips(h, (const uint8_t *)SPL_A, (const uint8_t *)SPL_B);
		}
		FieldFree(0x40);
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_note_thread(i);)
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter < 0x19)
		{
			n->f10 = (int16_t)(n->f10 + n->f14);
			n->f12 = (int16_t)(n->f12 + n->f16);
		}
		n->counter++;
		return n->counter >= 0x1E ? TASK_END : 0;
	}

	// Master (0x6096F0)
	static uint32_t __cdecl MasterTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		GetEffectSpawnPosition(Entity(CasterSlot()), 0xE, 0x1000, CasterPos());
		CasterPos()[1] = (int16_t)(CasterPos()[1] - 100);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			GetDefaultEffectPosition(Entity(TargetSlot()), TargetPos());
			const uint8_t *e = Entity(TargetSlot());
			TargetPos()[1] = (int16_t)(((int32_t)*(const int16_t *)(e + 0x3C) + *(const int16_t *)(e + 0x36)) >> 1);
		}
		if (n->counter == 0xD)
		{
			Node *t = AddTask(ORIG_ThreadTask);
			t->counter = 0;
			t->f10 = 0;
			t->f14 = 0x380;
			t->f12 = (int16_t)(TargetPos()[1] + 0x2EE);
			t->f16 = (int16_t)0xFFC4;
			for (int k = 0; k < 5; k++) Ring()[k].valid = 0;
			if (n->counter == 0xD)
			{
				Node *w = AddTask(ORIG_WebTask);
				*(uint32_t *)&w->f10 = rd32(&TargetPos()[0]);
				w->counter = 0;
				*(uint32_t *)&w->f14 = rd32(&TargetPos()[2]);
				w->f18 = (int16_t)(CrtRand() % 0x1000);
				w->f1C = 0xC00;
			}
		}
		if (n->counter == 0xA) AddTask(ORIG_Particles1Task)->counter = 0;
		if (n->counter == 0x30) AddTask(ORIG_Particles2Task)->counter = 0;
		if (n->counter == 0x32)
		{
			Node *b = AddTask(ORIG_BurstTask);
			*(uint32_t *)&b->f10 = rd32(&TargetPos()[0]);
			b->counter = 0;
			*(uint32_t *)&b->f14 = rd32(&TargetPos()[2]);
			b->f18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t r = CrtRand();
			const int16_t s = (int16_t)((r % 0x500 + 0x980) / 4);
			b->f1E = s;
			b->f1C = s;
		}
		if (n->counter == 0x37) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (n->counter == 1) BdPlaySE(SOUND_Web, 0, 0x80);
		n->counter++;
		return n->counter > 0x3C ? TASK_END : 0;
	}

	// Root (0x60AF50, au_re_BdlinkTask_25)
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(held_note_root();)
		Cursor() = ARENA_A;
		if (!(*(const uint8_t *)&r->counter & 1)) Cursor() = ARENA_B;
		const int a = ExecuteTaskQueue(QEffects());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag153_sticky_web()
	{
		register_port(web153::ORIG_RootTask, (void *)web153::RootTask, "W153 RootTask", 153);
		register_port(web153::ORIG_MasterTask, (void *)web153::MasterTask, "W153 MasterTask", 153);
		register_port(web153::ORIG_ThreadTask, (void *)web153::ThreadTask, "W153 ThreadTask", 153);
		register_port(web153::ORIG_WebTask, (void *)web153::WebTask, "W153 WebTask", 153);
		register_port(web153::ORIG_Particles1Task, (void *)web153::Particles1Task, "W153 Particles1Task", 153);
		register_port(web153::ORIG_Particles2Task, (void *)web153::Particles2Task, "W153 Particles2Task", 153);
		register_port(web153::ORIG_BurstTask, (void *)web153::BurstTask, "W153 BurstTask", 153);
		// 30 fps layer: see mag153_sticky_web_held.inc
		FX_HELD(register_mag153_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag153_sticky_web_held.inc"
#endif
