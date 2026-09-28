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

// Effect 126: Electric Discharge (enemy attack 28 of kernel.bin, Cockatrice; MAG_126_*).
//
// Structure (setup MAG_126_ELECTRIC_DISCHARGE 0x6B9A10 -> _Init 0x6B9A40, file loader 0x6B9A20 =
// the texture file named at 0x10E298C (mag125.tim); the setup keeps the CASTER's entity and the
// tint words (+0x28) of the three party entities, starts camera animation 0x12421E8, queues the TIM):
//   RootTask (0x6B9AF0) - alternates the packet arena (magic buffer + 0xC454 / + 0x1C454); clears
//     the screen-flash level and the flag 0x800 of the three party entities; on counter 1 sets up
//     the pools (main 1 x 0x14, arcs 0x40 x 0x3C, crawlers 0x40 x 0x28, sparks 0x20 x 0x18, glow
//     layouts 8 x 0xD4, and two point pools of 0x200 x 0x28 in the magic buffer) and starts the
//     main task; runs the main and arc queues, the caster's bone world matrices (0x5095B0), the
//     crawler queue, the caster's pose matrices (0x508C90), the spark and layout queues; screen
//     flash; ends when all five queues are empty.
//   Main (0x6B9D60) - flash level ramp (0..0x800, counters 0..3 up, 49..52 down); counters 0..7:
//     three crawlers per tick from the crawler script table 0x124228C, counters 8..52: one random
//     crawler per tick; counters 8..19: an arc from the caster's centre in a random direction;
//     even counters 14..42: an arc from a random effect bone of the caster (0x1242284) to a random
//     bone of the next living party member; every 4th tick a glow layout (0x12417D8). Sound at 0,
//     damage (action 0) and end at 52.
//   Arc (0x6BA680) - a lightning arc: 8 steps per tick, each moves its source point by a random
//     walk, blends it toward the end point by the progress t, appends a point (0x6BA610) and may
//     fork a branch arc (0x6BA4F0); when t reaches 0x1000 a spark (0x6BAB30) and sometimes a
//     screen fade. Then the point fades, the projection and the textured ribbon (0x6BAC20 /
//     0x6BAD90), the points drift by their velocity and faded tail points are freed. UPDATE THEN DRAW.
//   Crawler (0x6BB150) - a ribbon crawling over the caster's model: script words (bone, vertex,
//     forks) append points on model vertices through the caster's bone matrices (0x6BB070);
//     offsets drift, fade; ribbon draw 0x6BB320 / 0x6BB520; faded tail points are freed. UPDATE
//     THEN DRAW.
//   Spark (0x6BAB30) - 8-frame sprite sequence (0x12415A8 / 0x12416FC) at a fixed place; the
//     spark of an arc that hit a party member tints that entity (0x56CC00) for 5 frames. DRAW THEN
//     UPDATE.
//   GlowLayout (0x6BA1F0) - prim-model layout 0x12417D8 played by the shared player at the caster
//     (200 below its effect anchor), callback PartCallback (0x6BA250). DRAW THEN UPDATE.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25212C0..0x2521368 (flash level, caster entity, texture file, magic buffer
// base, context, root pool, party tints, packet cursor 0x25212F8, queues 0x2521300..0x2521350, an
// never-written zero vector 0x2521360). Exe data written: the screen-fade chance 0x124227C.

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace disch126
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &FlashLevel() { return var<uint32_t>(0x25212C0); }       // screen flash level
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25212C4); }          // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x25212CC); }          // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25212D0); }
	inline uint32_t *SavedTints() { return (uint32_t *)0x25212EC; }          // party entities +0x28 (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25212F8); }
	inline TaskQueue *QLayouts() { return (TaskQueue *)0x2521300; }          // pool: magic buffer, 8 x 0xD4
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2521310; }           // + 0x6A0, 0x20 x 0x18
	inline TaskQueue *QCrawlers() { return (TaskQueue *)0x2521320; }         // + 0x9A0, 0x40 x 0x28
	inline TaskQueue *QArcs() { return (TaskQueue *)0x2521330; }             // + 0x13A0, 0x40 x 0x3C
	inline TaskQueue *QMain() { return (TaskQueue *)0x2521340; }             // + 0x22A0, 1 x 0x14
	static const uint32_t ZERO_VEC = 0x2521360;                             // 8 bytes, never written
	inline int32_t &FadeChance() { return var<int32_t>(0x124227C); }         // exe data: screen fade threshold
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	// magic buffer: arc points + 0x22B4 and crawler points + 0x72B4 (0x200 x 0x28 each), one freed
	// point kept per pool (+ 0xC2B4 / + 0xC2B8), blended vertex frames + 0xC2BC
	static const uint32_t ARC_POINTS = 0x22B4, CRAWL_POINTS = 0x72B4, ARC_FREE = 0xC2B4, CRAWL_FREE = 0xC2B8, MORPH = 0xC2BC;

	static const uint32_t ORIG_RootTask = 0x6B9AF0;
	static const uint32_t ORIG_MainTask = 0x6B9D60;
	static const uint32_t ORIG_LayoutTask = 0x6BA1F0;
	static const uint32_t ORIG_ArcTask = 0x6BA680;
	static const uint32_t ORIG_SparkTask = 0x6BAB30;
	static const uint32_t ORIG_CrawlerTask = 0x6BB150;
	static const uint32_t SEQ_Spark = 0x12415A8;      // sprite sequence of the arc end sparks
	static const uint32_t SEQ_SparkB = 0x12416FC;     // ... of the free arcs' ground sparks
	static const uint32_t MODEL_Glow = 0x12417D8;     // prim-model layout data (0xC8-byte layout)
	static const void *const SOUND_Discharge = (const void *)0x1242278;
	static const void *const TINT_Colour = (const void *)0x1242280;
	static const uint8_t *const BONES_Caster = (const uint8_t *)0x1242284;   // 5 effect bones
	static const int16_t *const CRAWL_Scripts = (const int16_t *)0x124228C;  // [0] = count, [1 + i] = script offset (words)
	static const uint8_t *const LEN_Table = (const uint8_t *)0x1242A0C;      // 128 x 128 |(dx, dy)|
	static const int16_t *const BLEND_Table = (const int16_t *)0x1246A0C;    // point velocity blend by t / 2

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }  // MAG_106_sub_7059E0: (125 s + 14) & 0x7FFF
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	// sub_56CC00: out colour = a * wa + b * wb (4.12 weights)
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0e;
		uint8_t started;   // +0x0F pools set up (counter 1)
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..52
		int16_t pad0e;
		int16_t party;     // +0x10 next party slot tried (0..2)
		int16_t pad12;
	};
	struct Point // both point pools (0x28 bytes); the ribbon code shares +0x10..+0x27
	{
		int16_t a[3];      // +0x00 arc: position; crawler: [0] model vertex, [1] width, [2] offset x
		int16_t b;         // +0x06 arc: width (-1 = free slot); crawler: offset y
		int16_t c[3];      // +0x08 arc: velocity; crawler: [0] offset z, [1..2] drift (3 x s8) + bone (s8)
		int16_t d;         // +0x0E arc: fade hold ticks
		int16_t sxy[2];    // +0x10 projected position, then the ribbon's first edge vertex
		int16_t otz;       // +0x14 (read as a dword by the GTE)
		int16_t pad16;
		int16_t edge[2];   // +0x18 the ribbon's second edge vertex
		int16_t pad1c[2];
		int16_t fade;      // +0x20
		int16_t step;      // +0x22
		Point *next;       // +0x24
	};
	struct ArcNode // pool of 0x40 nodes of 0x3C bytes
	{
		TaskNode hdr;
		int16_t pad0c;     // +0x0C cleared, unused
		int16_t count;     // +0x0E points
		Point *tail;       // +0x10 oldest point
		Point *head;       // +0x14 newest point
		int16_t pos[3];    // +0x18 random-walking source
		int16_t t;         // +0x1E progress (0x1000 = reached)
		int16_t end[3];    // +0x20
		int16_t speed;     // +0x26 progress per step
		int16_t vel[3];    // +0x28 source velocity
		int16_t forks;     // +0x2E fork chances left
		int16_t dir[3];    // +0x30 point velocity at t = 0
		int16_t slot;      // +0x36 party slot hit (-1 = none)
		int16_t width;     // +0x38
		int16_t style;     // +0x3A texture row (0 / 1)
	};
	struct SparkNode // pool of 0x20 nodes of 0x18 bytes
	{
		TaskNode hdr;
		int16_t frame;     // +0x0C 0..7
		uint8_t kind;      // +0x0E 0 = arc end, 1 = ground
		int8_t slot;       // +0x0F party slot tinted (-1 = none)
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
	};
	struct CrawlerNode // pool of 0x40 nodes of 0x28 bytes
	{
		TaskNode hdr;
		int16_t steps;     // +0x0C script steps per tick
		int16_t count;     // +0x0E points
		int16_t width;     // +0x10
		int16_t fade;      // +0x12 new points' fade
		int16_t step;      // +0x14 new points' fade step
		int16_t pad16;
		const int16_t *script; // +0x18 (null = finished)
		Point *tail;       // +0x1C
		Point *head;       // +0x20
		uint8_t *model;    // +0x24 caster entity +0x64
	};
	struct LayoutNode // pool of 8 nodes of 0xD4 bytes
	{
		TaskNode hdr;
		uint8_t layout[0xC8]; // +0x0C prim-model layout (prim::Layout)
	};
	// the prim-player callback's parameter block (a stack block of the layout task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z, height
		uint32_t morph;    // +0x08 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x14, "Electric Discharge nodes");
	static_assert(sizeof(Point) == 0x28 && sizeof(ArcNode) == 0x3C && sizeof(SparkNode) == 0x18, "Electric Discharge nodes");
	static_assert(sizeof(CrawlerNode) == 0x28 && sizeof(LayoutNode) == 0xD4 && sizeof(PrimArg) == 0xC, "Electric Discharge nodes");

	inline int8_t CrawlDrift(const Point *p, int i) { return ((const int8_t *)p)[0xC + i]; }
	inline int8_t CrawlBone(const Point *p) { return ((const int8_t *)p)[0xF]; }

	// ------------------------------------------------------------------
	// Ribbon (0x6BAD90 arcs / 0x6BB520 crawlers): the two edge vertices of point p from the
	// screen direction towards `next` (averaged with the previous one), half width = next's
	// width * 32 / (p's depth + bias); a point without next has no width
	// ------------------------------------------------------------------
	static void RibbonEdge(Point *p, const Point *next, const int16_t *prev, int16_t *out, int32_t width, int32_t bias)
	{
		if (p->otz < 0) return;
		int32_t ox, oy;
		if (!next)
		{
			ox = 0;
			oy = 0;
		}
		else
		{
			if (next->otz < 0) return;
			const int32_t dx = (int32_t)next->sxy[0] - p->sxy[0];
			int32_t ax = dx < 0 ? -dx : dx;
			if (ax >= 0x80) ax = 0x7F;
			const int32_t dy = (int32_t)p->sxy[1] - next->sxy[1];
			int32_t ay = dy < 0 ? -dy : dy;
			if (ay >= 0x80) ay = 0x7F;
			const int32_t len = LEN_Table[ay * 128 + ax];
			int32_t ux, uy;
			if (prev)
			{
				if (len == 0)
				{
					out[0] = prev[0];
					out[1] = prev[1];
					ux = prev[0];
					uy = prev[1];
				}
				else
				{
					const int32_t nx = shl32(dy, 12) / len;
					const int32_t ny = shl32(dx, 12) / len;
					int32_t sx = nx + prev[0];
					int32_t sy = ny + prev[1];
					out[0] = (int16_t)nx;
					out[1] = (int16_t)ny;
					if (sx == 0 && sy == 0)
					{
						sx = nx + nx;
						sy = ny + ny;
					}
					const int32_t k = 0x4000 - ((mul32(sx, sx) + mul32(sy, sy)) >> 13);
					ux = mul32(k, sx) >> 13;
					uy = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0)
				{
					p->otz = -1;
					return;
				}
				ux = shl32(dy, 12) / len;
				uy = shl32(dx, 12) / len;
				out[0] = (int16_t)ux;
				out[1] = (int16_t)uy;
			}
			const int32_t w = shl32(width, 5) / ((int32_t)p->otz + bias);
			ox = mul32(w, ux) >> 12;
			oy = mul32(w, uy) >> 12;
		}
		const int16_t x = p->sxy[0], y = p->sxy[1];
		p->sxy[0] = (int16_t)(x - ox);
		p->edge[0] = (int16_t)(x + ox);
		p->edge[1] = (int16_t)(y + oy);
		p->sxy[1] = (int16_t)(y - oy);
	}

	// one textured quad (0x3E) per pair of visible points, gouraud by the points' fades
	static void RibbonQuads(Point *tail, int32_t count, uint32_t uv0, uint32_t uv1, uint16_t uv2, uint16_t uv3)
	{
		Point *a = tail;
		for (int32_t i = count - 1; i != 0; i--)
		{
			Point *b = a->next;
			if (a->otz > 0 && b->otz > 0)
			{
				uint8_t *pk = (uint8_t *)PacketCursor();
				PacketCursor() += 0x34;
				*(uint32_t *)(pk + 8) = *(const uint32_t *)a->sxy;
				*(uint32_t *)(pk + 0x14) = *(const uint32_t *)a->edge;
				*(uint32_t *)(pk + 0xC) = uv0;
				*(uint32_t *)(pk + 0x18) = uv1;
				*(uint16_t *)(pk + 0x24) = uv2;
				*(uint16_t *)(pk + 0x30) = uv3;
				*(uint32_t *)(pk + 0x20) = *(const uint32_t *)b->sxy;
				*(uint32_t *)(pk + 0x2C) = *(const uint32_t *)b->edge;
				const int32_t ca = (int16_t)(a->fade >> 4);
				const uint32_t cola = (uint32_t)((shl32(shl32(ca, 8) | ca, 8)) | ca);
				*(uint32_t *)(pk + 0x10) = cola;
				*(uint32_t *)(pk + 4) = cola;
				*(uint32_t *)pk = 0xC000000;
				pk[7] = 0x3E;
				const int32_t cb = (int16_t)(b->fade >> 4);
				const uint32_t colb = (uint32_t)((shl32(shl32(cb, 8) | cb, 8)) | cb);
				*(uint32_t *)(pk + 0x28) = colb;
				*(uint32_t *)(pk + 0x1C) = colb;
				InsertPrimAutoDepth(OTBase() + (((int32_t)a->otz + b->otz) >> 5) * 4 + 0x44, pk);
			}
			a = b;
		}
	}

	// the edges of every point (first, middle, last) then the quads. The direction local starts
	// with what its stack slot held (dir0, see ArcRibbon / CrawlerTask): the first point leaves it
	// unwritten when it projects onto the second one (it is hidden), the second point then reads it.
	// Returns what the local holds at the end.
	static uint32_t Ribbon(Point *tail, int32_t count, int32_t width_word, int32_t bias, uint32_t uv0, uint32_t uv1, uint16_t uv2, uint16_t uv3, uint32_t dir0)
	{
		int16_t dir[2] = { (int16_t)dir0, (int16_t)(dir0 >> 16) };
		auto width = [&](const Point *n) { return (int32_t)((const int16_t *)n)[width_word]; };
		RibbonEdge(tail, tail->next, nullptr, dir, tail->next ? width(tail->next) : 0, bias);
		Point *p = tail->next;
		for (int32_t i = count - 2; i != 0; i--)
		{
			RibbonEdge(p, p->next, dir, dir, width(p->next), bias);
			p = p->next;
		}
		RibbonEdge(p, nullptr, dir, nullptr, 0, bias);
		RibbonQuads(tail, count, uv0, uv1, uv2, uv3);
		return (uint16_t)dir[0] | (uint32_t)(uint16_t)dir[1] << 16;
	}

	// Arc ribbon (0x6BAC20): texture row by style. UNINIT 0x6BAC32 / 0x6BAC53: its direction local
	// [esp] is the slot where the last projection's GTE_RTPS (0x45FAA0) kept its depth divisor
	// ([esp+0x10] of its frame: SZ3 when above H / 2, else H / 2)
	static void ArcRibbon(Point *tail, int32_t count, int32_t style)
	{
		const int32_t sz = (int32_t)var<uint32_t>(0x1CA8A5C);
		const int32_t h2 = (int32_t)((var<uint32_t>(0x1CA92E4) & 0xFFFF) >> 1);
		const uint32_t v = shl32(style + 0xC0, 8);
		Ribbon(tail, count, 3, 0x200, v | 0x3D140008, v | 0xB70038, (uint16_t)(v | 8), (uint16_t)(v | 0x38), (uint32_t)(sz > h2 ? sz : h2));
	}

	// projection of every point of a chain (arc task)
	static void ArcProject(Point *p)
	{
		do
		{
			GteLoadV0(p);
			GteRTPS();
			GteReadSXY2(p->sxy);
			GteReadOTZ(&p->otz);
			p = p->next;
		} while (p);
	}

	// crawler point on the caster's model (0x6BB320 loop): vertex through its bone's matrix, plus
	// the point's offset, projected with the camera
	static void CrawlerPointWorld(const uint8_t *model, const Point *p, int16_t w[4])
	{
		const Mat4x3 *m = (const Mat4x3 *)(*(uint8_t *const *)model + 0x20 + CrawlBone(p) * 0x30);
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(m);
		GteLoadV0u(*(uint8_t *const *)(model + 4) + p->a[0] * 2);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(w);
	}
	static void CrawlerPointProject(Point *p, int16_t w[4])
	{
		w[0] = (int16_t)(w[0] + p->a[2]);
		w[1] = (int16_t)(w[1] + p->b);
		w[2] = (int16_t)(w[2] + p->c[0]);
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(w);
		GteRTPS();
		GteReadSXY2(p->sxy);
		GteReadOTZ(&p->otz);
	}
	static uint32_t CrawlerRibbon(Point *tail, int32_t count, uint32_t dir0)
	{
		return Ribbon(tail, count, 1, 0x400, 0x3D14C008, 0xB7C038, 0xC008, 0xC038, dir0);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag126_electric_discharge_held.h"
#endif

namespace ff8fx
{
namespace disch126
{
	// ------------------------------------------------------------------
	// Crawler draw (0x6BB320)
	// ------------------------------------------------------------------
	static void CrawlerDraw(uint8_t *model, Point *tail, int32_t count, uint32_t dir0)
	{
		Point *p = tail;
		do
		{
			// the 4th word is never written (only loaded into the GTE V0 pad)
			int16_t w[4] = { 0, 0, 0, 0 };
			CrawlerPointWorld(model, p, w);
			// 30 fps layer: see mag126_electric_discharge_held.inc
			FX_HELD(held_note_crawler_point(w);)
			CrawlerPointProject(p, w);
			p = p->next;
		} while (p);
		CrawlerRibbon(tail, count, dir0);
	}

	// ------------------------------------------------------------------
	// Point pools (0x6BA610 arcs, 0x6BB070 crawlers)
	// ------------------------------------------------------------------
	static Point *AllocArcPoint(const int16_t *pos, int16_t width)
	{
		const uint32_t base = TexBase();
		Point *p = var<Point *>(base + ARC_FREE);
		if (p) var<Point *>(base + ARC_FREE) = nullptr;
		else
		{
			p = (Point *)(base + ARC_POINTS);
			int n = 0x200;
			while (p->b != -1)
			{
				p++;
				if (--n == 0) return nullptr;
			}
		}
		*(uint32_t *)p->a = *(const uint32_t *)pos;
		*(uint32_t *)&p->a[2] = *(const uint32_t *)&pos[2];
		p->b = width;
		p->d = 2;
		p->fade = 0x200;
		p->step = 0x300;
		p->next = nullptr;
		return p;
	}

	static Point *AllocCrawlerPoint(int32_t bone, int32_t vertex, int32_t width, int32_t drift, int32_t fade, int32_t step)
	{
		const uint32_t base = TexBase();
		Point *p = var<Point *>(base + CRAWL_FREE);
		if (p) var<Point *>(base + CRAWL_FREE) = nullptr;
		else
		{
			p = (Point *)(base + CRAWL_POINTS);
			int n = 0x200;
			while (p->a[0] != -1)
			{
				p++;
				if (--n == 0) return nullptr;
			}
		}
		p->c[0] = 0;
		p->b = 0;
		p->a[2] = 0;
		int8_t *d = (int8_t *)p + 0xC;
		if (drift)
		{
			for (int i = 0; i < 3; i++)
			{
				int32_t r = Rand();
				r += Rand();
				d[i] = (int8_t)((r >> 10) - 0x20);
			}
		}
		else
		{
			d[2] = 0;
			d[1] = 0;
			d[0] = 0;
		}
		d[3] = (int8_t)bone;
		p->a[1] = (int16_t)width;
		p->a[0] = (int16_t)vertex;
		p->step = (int16_t)step;
		p->fade = (int16_t)fade;
		p->next = nullptr;
		return p;
	}

	// ------------------------------------------------------------------
	// Spawns
	// ------------------------------------------------------------------
	// 0x6BA4F0: an arc from `from` (first point) toward `to`, source velocity `vel`, point
	// velocity dir (or random), progress t0 advancing by speed; returns the first point
	static Point *SpawnArc(const int16_t *vel, const int16_t *from, const int16_t *to, const int16_t *dir, int32_t slot, int32_t t0, int32_t speed, int32_t width, int32_t style, int32_t forks)
	{
		Point *pt = AllocArcPoint(from, (int16_t)width);
		if (!pt) return nullptr;
		ArcNode *a = (ArcNode *)AddTaskToQueue(QArcs(), ORIG_ArcTask);
		if (!a)
		{
			pt->b = -1;
			return nullptr;
		}
		Memset32(&a->pad0c, 0, 0xC);
		a->pad0c = 0;
		memcpy(a->pos, from, 8);
		memcpy(a->end, to, 8);
		memcpy(a->vel, vel, 8);
		a->t = (int16_t)t0;
		a->forks = (int16_t)forks;
		a->speed = (int16_t)speed;
		a->head = pt;
		a->tail = pt;
		a->count = 1;
		if (!dir)
		{
			a->dir[0] = (int16_t)((Rand() & 0xFF) - 0x80);
			a->dir[1] = (int16_t)((Rand() & 0xFF) - 0x80);
			a->dir[2] = (int16_t)((Rand() & 0xFF) - 0x80);
		}
		else memcpy(a->dir, dir, 8);
		a->slot = (int16_t)slot;
		a->width = (int16_t)width;
		a->style = (int16_t)style;
		return pt;
	}

	// 0x6BAFA0: a crawler running `script` (words after the bone word), `steps` script steps per tick
	static void SpawnCrawlerScript(uint8_t *model, const int16_t *script, int32_t steps, int32_t width)
	{
		const int32_t r0 = Rand() & 0x3FF;
		const int32_t r1 = Rand() & 0x1FF;
		const int32_t fade = r0 + r1 + 0x100;
		const int16_t step = (int16_t)-(int16_t)((int16_t)fade >> 3);
		const int16_t bone = script[0];
		script++;
		Point *pt = AllocCrawlerPoint(bone, script[0], width, 0, fade, step);
		if (!pt) return;
		CrawlerNode *c = (CrawlerNode *)AddTaskToQueue(QCrawlers(), ORIG_CrawlerTask);
		if (!c)
		{
			pt->a[0] = -1;
			var<Point *>(TexBase() + CRAWL_FREE) = pt;
			return;
		}
		Memset32(&c->steps, 0, 7);
		c->step = step;
		c->script = script + 1;
		c->head = pt;
		c->tail = pt;
		c->fade = (int16_t)fade;
		c->model = model;
		c->steps = (int16_t)steps;
		c->width = (int16_t)width;
		c->count = 1;
	}

	// 0x6BAF20: crawler script `index` of the table, random width scaled by scale (4.12)
	static void SpawnCrawler(uint8_t *model, const int16_t *table, int32_t index, int32_t steps, int32_t scale)
	{
		const int16_t *script = table + table[index + 1];
		static const int32_t widths[4] = { 0x96, 0xB4, 0xDC, 0x104 };
		int32_t w = widths[Rand() & 3];
		if (scale != 0x1000) w = mul32(w, scale) >> 12;
		SpawnCrawlerScript(model, script, steps, w);
	}

	// 0x6BA190: unit direction of the angles (pitch, yaw)
	static void AnglesToDir(const int16_t *ang, int16_t *out)
	{
		const int32_t c0 = ComputeCos(ang[0]);
		out[1] = (int16_t)-ComputeSin(ang[0]);
		out[0] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeSin(ang[1]), c0)) >> 12);
		out[2] = (int16_t)(mul32(ComputeCos(ang[1]), c0) >> 12);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6BA250): one object of the glow layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// anchor + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Glow layout (0x6BA1F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *g = (LayoutNode *)n;
		PrimArg arg;
		GetDefaultEffectPosition(Caster(), arg.pos);
		arg.pos[1] = (int16_t)(arg.pos[1] + 0xC8);
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(held_note_play((prim::Layout *)g->layout, &arg);)
		return prim::play((prim::Layout *)g->layout, PartCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spark (0x6BAB30)
	// ------------------------------------------------------------------
	static void SparkDraw(SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, s->scale, -(s->scale >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = s->kind ? SEQ_SparkB : SEQ_Spark;
		*(int16_t *)(h + 4) = s->frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		if ((uint8_t)s->slot != 0xFF && s->frame <= 4)
		{
			uint8_t *e = Entity(s->slot);
			e[1] |= 8;
			const int32_t sn = ComputeSin(shl32(s->frame, 8));
			BlendColour(&SavedTints()[s->slot], TINT_Colour, 0x1000 - sn, sn, e + 0x28);
		}
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(held_note_spark(s);)
		SparkDraw(s);
		s->frame++;
		return s->frame < 8 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Arc (0x6BA680)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ArcTask(TaskNode *n)
	{
		ArcNode *a = (ArcNode *)n;
		for (int k = 8; k != 0; k--)
		{
			if (a->t >= 0x1000) continue;
			a->pos[0] = (int16_t)(a->pos[0] + a->vel[0]);
			a->t = (int16_t)(a->speed + a->t);
			a->pos[1] = (int16_t)(a->pos[1] + a->vel[1]);
			a->pos[2] = (int16_t)(a->pos[2] + a->vel[2]);
			a->vel[0] = (int16_t)(a->vel[0] + ((Rand() & 0x7F) - 0x40));
			a->vel[1] = (int16_t)(a->vel[1] + ((Rand() & 0x7F) - 0x40));
			a->vel[2] = (int16_t)(a->vel[2] + ((Rand() & 0x7F) - 0x40));
			if (Rand() < 0x1000)
			{
				a->pos[0] = (int16_t)(a->pos[0] + ((Rand() & 0x1FF) - 0x100));
				a->pos[1] = (int16_t)(a->pos[1] + ((Rand() & 0x1FF) - 0x100));
				a->pos[2] = (int16_t)(a->pos[2] + ((Rand() & 0x1FF) - 0x100));
			}
			// the new point: source and end blended by t
			int16_t p[4] = { 0, 0, 0, 0 };
			GteSetIR0(0x1000 - a->t);
			GteLoadIR123(a->pos);
			GteGPF();
			GteSetIR0(a->t);
			GteLoadIR123(a->end);
			GteGPL();
			GteStoreIR123(p);
			if (a->t >= 0x1000)
			{
				if (a->slot >= 0)
				{
					if (Rand() < FadeChance())
					{
						ScreenFadeTask(0, 1, 0, 0x40);
						FadeChance() = 0x1000;
					}
					else FadeChance() += 0x1000;
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (s)
					{
						s->frame = 0;
						memcpy(s->pos, p, 8);
						s->kind = 0;
						s->scale = (int16_t)shl32(a->width, 4);
						s->slot = (int8_t)a->slot;
					}
				}
				else if (p[1] >= 0)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (s)
					{
						s->frame = 0;
						memcpy(s->pos, p, 8);
						s->kind = 1;
						s->scale = (int16_t)shl32(a->width, 5);
						s->slot = (int8_t)a->slot;
					}
				}
			}
			Point *pt = AllocArcPoint(p, a->width);
			if (!pt)
			{
				a->t = 0x1000;
				continue;
			}
			a->head->next = pt;
			a->count++;
			const int32_t blend = BLEND_Table[((uint16_t)a->t >> 1) & 0xFFF];
			a->head = pt;
			// the point's velocity: dir blended toward the zero vector (the hold word kept)
			const int16_t hold = pt->d;
			GteSetIR0(blend);
			GteLoadIR123(a->dir);
			GteGPF();
			GteSetIR0(0x1000 - blend);
			GteLoadIR123((const void *)ZERO_VEC);
			GteGPL();
			GteStoreIR123(pt->c);
			pt->d = hold;
			if (a->t >= 0xC00) continue;
			if (Rand() >= shl32(a->forks, 13)) continue;
			// a fork from the new point
			int16_t to[4] = { 0, 0, 0, 0 }, vel[4] = { 0, 0, 0, 0 };
			int32_t d;
			if (Rand() > 0x4000) d = -0x200 - (Rand() & 0x1FF);
			else d = (Rand() & 0x1FF) + 0x200;
			to[0] = (int16_t)(d + p[0]);
			if (Rand() > 0x4000) d = -0x200 - (Rand() & 0x1FF);
			else d = (Rand() & 0x1FF) + 0x200;
			to[1] = (int16_t)(d + p[1]);
			to[2] = (int16_t)((Rand() & 0x3FF) + p[2] + 0x400);
			vel[0] = (int16_t)((Rand() & 0x1FF) + a->vel[0] - 0x100);
			vel[1] = (int16_t)((Rand() & 0x1FF) + a->vel[1] - 0x100);
			vel[2] = (int16_t)((Rand() & 0x1FF) + a->vel[2] - 0x100);
			Point *f = SpawnArc(vel, p, to, a->dir, -1, a->t, 0x200, a->width >> 1, a->style, 0);
			if (f) memcpy(f->c, pt->c, 8);
			a->forks--;
		}
		// fades
		Point *q = a->tail;
		do
		{
			if (q->fade > 0)
			{
				q->fade = (int16_t)(q->fade + q->step);
				if (q->fade <= 0) q->fade = 0;
				else if (q->d != 0 && q->step <= 0) q->d--;
				else if (q->step > -0x180) q->step = (int16_t)(q->step - 0x140);
			}
			q = q->next;
		} while (q);
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(held_note_arc(a);)
		ArcProject(a->tail);
		if (a->count >= 2) ArcRibbon(a->tail, a->count, a->style);
		q = a->tail;
		do
		{
			q->a[0] = (int16_t)(q->a[0] + q->c[0]);
			q->a[1] = (int16_t)(q->a[1] + q->c[1]);
			q->a[2] = (int16_t)(q->a[2] + q->c[2]);
			q = q->next;
		} while (q);
		// free the faded tail
		q = a->tail;
		while (q->fade == 0 && q->next->fade == 0)
		{
			var<Point *>(TexBase() + ARC_FREE) = q;
			Point *next = q->next;
			q->b = -1;
			a->count--;
			a->tail = next;
			if (a->count < 2) return TASK_END;
			q = next;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Crawler (0x6BB150)
	// UNINIT 0x6BB3D5 / 0x6BB405: the draw's direction local (0x6BB320 [esp+0x18]) is the task's
	// stack slot [entry esp - 0x2C], never written by the draw itself. It holds the first argument
	// of the task's last 0x6BB070 call (edi: the bone word, high half = edi's), else the ebx (2) a
	// fork call (0x6BAFA0) saved there, else what the stack held when the task started (slot).
	// edi starts as the executor's (the queue node before this one, 0 for the first) and takes the
	// script pointer's high half at a fork.
	// ------------------------------------------------------------------
	static __declspec(noinline) uint32_t CrawlerTaskBody(CrawlerNode *c, uint32_t slot)
	{
		const uint32_t slot0 = slot;
		uint32_t edi = 0;
		for (TaskNode *q = QCrawlers()->head; q && q != &c->hdr; q = q->next) edi = (uint32_t)q;
		int32_t k = c->steps;
		do
		{
			if (!c->script) continue;
			int16_t bone = *c->script++;
			int32_t drift = 1;
			while (bone < 0)
			{
				// a fork: a new crawler runs the words that follow, this one skips them
				SpawnCrawlerScript(c->model, c->script, c->steps, c->width);
				slot = 2;
				c->script += -1 - bone;
				edi = (uint32_t)c->script;
				bone = *c->script++;
			}
			const int16_t vertex = *c->script++;
			const int16_t *peek = c->script;
			int16_t w = *peek;
			if (w < 0)
			{
				peek -= w;
				w = *peek;
			}
			if (w == 0x7FFF)
			{
				if (c->count <= 3) goto kill;
				c->script = nullptr;
				drift = 0;
			}
			{
				const int32_t width = c->width;
				Point *pt = AllocCrawlerPoint(bone, vertex, (mul32(Rand(), width) >> 15) + width, drift, c->fade, c->step);
				slot = (edi & 0xFFFF0000u) | (uint16_t)bone;
				if (!pt) goto kill;
				c->head->next = pt;
				c->count++;
				c->head = pt;
			}
		} while (--k != 0);
		{
			Point *q = c->tail;
			do
			{
				q->a[2] = (int16_t)(q->a[2] + CrawlDrift(q, 0));
				q->b = (int16_t)(q->b + CrawlDrift(q, 1));
				q->c[0] = (int16_t)(q->c[0] + CrawlDrift(q, 2));
				if (q->fade > 0)
				{
					q->fade = (int16_t)(q->fade + q->step);
					if (q->fade <= 0) q->fade = 0;
				}
				q = q->next;
			} while (q);
		}
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(held_note_crawler(c, slot, slot0);)
		if (c->count >= 2) CrawlerDraw(c->model, c->tail, c->count, slot);
		{
			Point *q = c->tail;
			while (q->fade <= 0x10 && q->next->fade <= 0x10)
			{
				var<Point *>(TexBase() + CRAWL_FREE) = q;
				q->a[0] = -1;
				q = q->next;
				c->count--;
				c->tail = q;
				if (c->count < 2) return TASK_END;
			}
		}
		return 0;
	kill:
		{
			Point *q = c->tail;
			var<Point *>(TexBase() + CRAWL_FREE) = q;
			do
			{
				q->a[0] = -1;
				q = q->next;
			} while (q);
		}
		return TASK_END;
	}
	static uint32_t __cdecl CrawlerTask(TaskNode *n)
	{
		// the original's stack slot, read before this function writes below its frame
		return CrawlerTaskBody((CrawlerNode *)n, *(const uint32_t *)((const uint8_t *)_AddressOfReturnAddress() - 0x2C));
	}

	// ------------------------------------------------------------------
	// Main (0x6B9D60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int16_t c = m->counter;
		if (c < 4 || c > 0x30)
		{
			const int32_t level = shl32(c < 4 ? c : 0x34 - c, 9);
			if (level > (int32_t)FlashLevel()) FlashLevel() = (uint32_t)level;
		}
		else if ((int32_t)FlashLevel() < 0x800) FlashLevel() = 0x800;
		if ((uint32_t)(int32_t)c < 8)
		{
			const int32_t s = c * 3;
			SpawnCrawler(*(uint8_t **)(Caster() + 0x64), CRAWL_Scripts, s % CRAWL_Scripts[0], 3, 0x1000);
			SpawnCrawler(*(uint8_t **)(Caster() + 0x64), CRAWL_Scripts, (s + 1) % CRAWL_Scripts[0], 3, 0x1000);
			SpawnCrawler(*(uint8_t **)(Caster() + 0x64), CRAWL_Scripts, (s + 2) % CRAWL_Scripts[0], 3, 0x1000);
		}
		if (m->counter == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			BdPlaySE3D(SOUND_Discharge, 0x100, pos);
		}
		const int32_t c8 = m->counter - 8;
		if ((uint32_t)c8 < 0x34)
		{
			const int32_t r = Rand();
			SpawnCrawler(*(uint8_t **)(Caster() + 0x64), CRAWL_Scripts, mul32(r, CRAWL_Scripts[0]) >> 15, 3, 0x1000);
		}
		if ((uint32_t)c8 < 0xC)
		{
			// a free arc from the caster's centre in a random direction
			int16_t from[4], dir[4] = { 0, 0, 0, 0 }, vel[4] = { 0, 0, 0, 0 };
			GetDefaultEffectPosition(Caster(), from);
			int16_t ang[2];
			ang[0] = (int16_t)((Rand() & 0x3FF) - 0x100);
			ang[1] = (int16_t)((uint32_t)shl32(c8, 10) / 12 + shl32(c8 & 3, 10));
			AnglesToDir(ang, dir);
			from[0] = (int16_t)(from[0] + (dir[0] >> 8));
			from[1] = (int16_t)(from[1] + (dir[1] >> 8));
			from[2] = (int16_t)(from[2] + (dir[2] >> 8));
			int16_t to[4] = { (int16_t)(dir[0] + from[0]), (int16_t)(dir[1] + from[1]), (int16_t)(dir[2] + from[2]), 0 };
			vel[0] = (int16_t)((Rand() & 0x7F) - 0x40);
			vel[1] = (int16_t)((Rand() & 0x7F) - 0x40);
			vel[2] = (int16_t)((Rand() & 0x7F) - 0x40);
			const int32_t style = Rand() & 1;
			Point *pt = SpawnArc(vel, from, to, nullptr, -1, 0, 0x100, 0xFA, style, 3);
			if (pt) memcpy(pt->c, (const void *)ZERO_VEC, 8);
		}
		const int32_t c14 = m->counter - 0xE;
		if ((uint32_t)c14 < 0x1E)
		{
			if (!(c14 & 1))
			{
				// an arc from a caster bone to a bone of the next living party member
				int16_t from[4] = { 0, 0, 0, 0 }, centre[4], to[4] = { 0, 0, 0, 0 };
				const int32_t r = Rand();
				GetEffectSpawnPosition(Caster(), BONES_Caster[(uint32_t)(r * 5) >> 15], 0, from);
				GetDefaultEffectPosition(Caster(), centre);
				uint8_t *e;
				do
				{
					const int16_t slot = m->party;
					e = Entity(slot);
					m->party = (int16_t)(slot + 1);
					if (m->party >= 3) m->party = 0;
				} while (!(e[0] & 2));
				const int32_t rb = Rand();
				GetEffectSpawnPosition(e, mul32(rb, **(uint8_t **)*(uint8_t **)(e + 0x64)) >> 15, 0, to);
				int16_t vel[4] = { 0, 0, 0, 0 };
				vel[0] = (int16_t)(from[0] - centre[0]);
				int16_t vy = (int16_t)(from[1] - centre[1]);
				vel[1] = vy > 0 ? (int16_t)(vy >> 3) : (int16_t)(vy >> 1);
				vel[2] = (int16_t)((int16_t)(from[2] - centre[2]) >> 2);
				const int32_t style = Rand() & 1;
				const int32_t slot = (int32_t)(e - Entity(0)) / 0x9C;
				Point *pt = SpawnArc(vel, from, to, nullptr, slot, 0, 0x100, 0xFA, style, 3);
				if (pt) memcpy(pt->c, (const void *)ZERO_VEC, 8);
			}
			if (!(c14 & 3))
			{
				LayoutNode *g = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_LayoutTask);
				DecodeModelPrimLayout(MODEL_Glow, g->layout, 0xC8);
			}
		}
		if (m->counter >= 0x34)
		{
			const ActionData *act = Ctx()->actions;
			ApplyActionResultToTargets(act->targets, act->target_count);
			return TASK_END;
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6B9AF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0xC454;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1C454;
			r->arena = 1;
		}
		FlashLevel() = 0;
		*(uint16_t *)Entity(0) &= 0xF7FF;
		*(uint16_t *)Entity(1) &= 0xF7FF;
		*(uint16_t *)Entity(2) &= 0xF7FF;
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + 0x22A0), 0x14, 1);
			InitTaskQueuePool(QArcs(), (void *)(TexBase() + 0x13A0), 0x3C, 0x40);
			InitTaskQueuePool(QCrawlers(), (void *)(TexBase() + 0x9A0), 0x28, 0x40);
			InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x6A0), 0x18, 0x20);
			InitTaskQueuePool(QLayouts(), (void *)TexBase(), 0xD4, 8);
			Memset32((void *)(TexBase() + CRAWL_POINTS), 0, 0x1400);
			Memset32((void *)(TexBase() + ARC_POINTS), 0, 0x1400);
			for (int i = 0; i < 0x200; i++) ((Point *)(TexBase() + CRAWL_POINTS))[i].a[0] = -1;
			for (int i = 0; i < 0x200; i++) ((Point *)(TexBase() + ARC_POINTS))[i].b = -1;
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 2);
			var<Point *>(TexBase() + ARC_FREE) = nullptr;
			var<Point *>(TexBase() + CRAWL_FREE) = nullptr;
		}
		int e1 = 0, e2 = 0, e3 = 0, e4 = 0, e5 = 0;
		if (r->started)
		{
			e1 = ExecuteTaskQueue(QMain());
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			e2 = ExecuteTaskQueue(QArcs());
			ComputeBonesWorldMatrices(Caster() + 0x60, Caster() + 0x40);
			e3 = ExecuteTaskQueue(QCrawlers());
			BuildBoneMatricesFromPose(Caster() + 0x60);
			e4 = ExecuteTaskQueue(QSparks());
			e5 = ExecuteTaskQueue(QLayouts());
		}
		SetScreenFlash(FlashLevel(), 0);
		if (r->started && e1 == 0 && e2 == 0 && e3 == 0 && e4 == 0 && e5 == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag126_electric_discharge()
	{
		register_port(disch126::ORIG_RootTask, (void *)disch126::RootTask, "D126 RootTask", 126);
		register_port(disch126::ORIG_MainTask, (void *)disch126::MainTask, "D126 MainTask", 126);
		register_port(disch126::ORIG_ArcTask, (void *)disch126::ArcTask, "D126 ArcTask", 126);
		register_port(disch126::ORIG_CrawlerTask, (void *)disch126::CrawlerTask, "D126 CrawlerTask", 126);
		register_port(disch126::ORIG_SparkTask, (void *)disch126::SparkTask, "D126 SparkTask", 126);
		register_port(disch126::ORIG_LayoutTask, (void *)disch126::LayoutTask, "D126 LayoutTask", 126);
		// 30 fps layer: see mag126_electric_discharge_held.inc
		FX_HELD(register_mag126_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag126_electric_discharge_held.inc"
#endif
