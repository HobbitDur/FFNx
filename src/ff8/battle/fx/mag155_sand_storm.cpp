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

// Effect 155: Sand Storm (enemy attack 39 of kernel.bin, used by Fastitocalon; MAG_155_*).
//
// Structure (setup MAG_155_SAND_STORM 0x606CF0, file loader 0x606CD0 = the texture file named at
// 0xDBA900 (mag154.tim); code 0x606CD0..0x607BF0). The setup keeps the cast context, the caster's
// slot and the first target's slot, builds the root queue 0x23F0818 (pool 0x23F0808, 1 x 0x10) with
// the root task and the effect queue 0x23F1AE8 (pool 0x23F1AF8, 100 x 0x24) with the director,
// clears the two sprite pools, starts camera animation 0xDB7DC4 and queues the TIM.
//   Root (0x607BB0) - alternates the packet arena (0x23F4230 / 0x23FC230, cursor 0x2404230), runs
//     the effect queue, ends when it is empty.
//   Director (0x606DB0) - tick 0: the caster's default effect position (0x23F2958, its y and height
//     words swapped), the targets' centre (0x23F2938), the funnel vertex order (0x607090); tick 1: the
//     wave and the funnel tasks, the two random side points of the path (0x23F07F0 / 0x23F07F8) and
//     the random middle point 0x23F2920, the wind sound; tick 3: dust pool A; tick 4: dust pool B;
//     tick 50: damage; ends after 60.
//   VertexOrder (0x607090) - the 19 x 16 vertices of the funnel model 0xDB8150 (vertex data 0xDB8158)
//     as (source, destination 0x23F2A70, y) records 0x23F33F0 sorted by y, highest first: 19 rings.
//   Wave (0x607130) - every tick rewrites the destination vertices: ring k (of 16 vertices) is the
//     source ring moved by radius (15 k) along angle (a + 300 k); a turns by -70..-139 per tick.
//   Funnel (0x607210) - the funnel prim model drawn twice (scale 2 / 1.6 horizontally, height
//     scale +0x12, spin +0x18, the second one half faded); its position moves from the caster to the
//     targets' centre with a sway (ticks 0..29), then along a spline (0x571620 / 0x571690, 6 points
//     built at tick 29: current position, the far side point's midpoint, the near side point, the
//     random middle point, the far side point, the current position) sampled in 34 steps
//     (0x23F2960); fades in (0..5) and out (52..); ends at 60.
//   Dust A (0x6076E0) - pool 0x23F1188 (100 x 0x18) of sprites 0xDB7E94 thrown outwards from the
//     funnel (3 per tick, ticks 0..52 of the task), sliding and slowing down.
//   Dust B (0x607920) - pool 0x23F0828 (100 x 0x18) of sprites 0xDB8040 rising around the funnel
//     (5 per tick, ticks 0..40), orbiting on a growing radius.
// Every task but the root tests battle_to_update_flags (0x1D96A9C) & 0x201: the wave still rewrites
// the vertices, the funnel and the dust pools draw, nothing moves, the director does nothing.
// Module globals: 0x23F07F0..0x2404234 (path points, slots, queues, pools, spline table, funnel
// vertices and records, packet arenas and cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace sandstorm155
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// module globals
	// ------------------------------------------------------------------
	inline int16_t *SideA() { return (int16_t *)0x23F07F0; }           // random side point (x, y, z, w)
	inline int16_t *SideB() { return (int16_t *)0x23F07F8; }           // random side point
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x23F0800); } // first target's slot (setup, unused)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x23F0818; }
	inline TaskQueue *QEffect() { return (TaskQueue *)0x23F1AE8; }
	inline int16_t *Spline() { return (int16_t *)0x23F2908; }          // 6 spline points of 8 bytes
	inline int16_t *Centre() { return (int16_t *)0x23F2938; }          // targets' centre
	inline CastContext *&Ctx() { return var<CastContext *>(0x23F2940); }
	inline uint32_t &PosX() { return var<uint32_t>(0x23F2948); }       // funnel position x (+ word 0x23F294A)
	inline uint32_t &PosZ() { return var<uint32_t>(0x23F294C); }       // funnel position z (+ word 0x23F294E)
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x23F2950); }
	inline uint32_t &TexFile() { return var<uint32_t>(0x23F2954); }
	inline int16_t *CasterPos() { return (int16_t *)0x23F2958; }       // caster's default position (y / h swapped)
	inline int16_t *PathTable() { return (int16_t *)0x23F2960; }       // 34 spline samples of 8 bytes
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2404230); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t POOL_A = 0x23F1188, POOL_B = 0x23F0828; // 100 x 0x18 each
	static const uint32_t VERTS = 0x23F2A70;                       // funnel vertices (19 x 16 x 8 bytes)
	static const uint32_t ORDER = 0x23F33F0;                       // 0x130 records of 12 bytes
	static const uint32_t ARENA_A = 0x23F4230, ARENA_B = 0x23FC230;
	static const uint32_t MODEL_Funnel = 0xDB8150;                 // prim model (vertices at +8)
	static const uint32_t SEQ_DustA = 0xDB7E94, SEQ_DustB = 0xDB8040;
	static const void *const SOUND_Wind = (const void *)0xDBA8F8;

	static const uint32_t ORIG_Root = 0x607BB0;
	static const uint32_t ORIG_Director = 0x606DB0;
	static const uint32_t ORIG_Wave = 0x607130;
	static const uint32_t ORIG_Funnel = 0x607210;
	static const uint32_t ORIG_DustA = 0x6076E0;
	static const uint32_t ORIG_DustB = 0x607920;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	// CalculateCenterPosition (0x5020A0): centre of the entities of a slot mask (low 16 bits)
	inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	// matrixMultiplyVector (0x56C4F0): out = m (3x3) * in (3 x int16), in and out may alias
	inline void MatrixMultiplyVector(const void *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const void *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	// sub_571620 / sub_571690: spline through n points (8-byte SVECTORs) into a 0x190-byte work
	// block, then its point at t (4.12) into out
	inline void SplineSetup(int32_t n, const int16_t *points, void *work) { fn<void (__cdecl *)(int32_t, const int16_t *, void *)>(0x571620)(n, points, work); }
	inline void SplineEval(int32_t n, void *work, int16_t *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, int16_t *, int32_t)>(0x571690)(n, work, out, t); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C packet arena parity
		int16_t pad;
	};
	struct DirectorNode // effect pool, 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad[0x16];
	};
	struct WaveNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E[5];
		int16_t angle;     // +0x18
		int16_t spin;      // +0x1A
		int16_t radius;    // +0x1C (0)
		int16_t pad1E[3];
	};
	struct FunnelNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E[2];
		int16_t height;    // +0x12 height scale
		int16_t pad14;
		int16_t grow;      // +0x16 height scale speed (ticks 0..9)
		int16_t angle;     // +0x18 spin
		int16_t spin;      // +0x1A
		int16_t sway;      // +0x1C sway amplitude
		int16_t sway_grow; // +0x1E
		int16_t phase;     // +0x20 sway phase
		int16_t phase_v;   // +0x22
	};
	struct DustNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad[0x16];
	};
	// one sprite of a dust pool (0x18 bytes)
	struct Dust
	{
		uint32_t used;     // +0x00 bit0
		int16_t frame;     // +0x04
		int16_t scale;     // +0x06
		int16_t pos[4];    // +0x08 x, y, z, w
		int16_t v[4];      // +0x10 A: vx, -, vz, - / B: radius, radius speed, angle, spin
	};
	// funnel vertex record (12 bytes)
	struct Order
	{
		uint32_t src, dst;
		int32_t y;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x24 && sizeof(WaveNode) == 0x24, "Sand Storm nodes");
	static_assert(sizeof(FunnelNode) == 0x24 && sizeof(DustNode) == 0x24, "Sand Storm nodes");
	static_assert(sizeof(Dust) == 0x18 && sizeof(Order) == 12, "Sand Storm records");

	// ------------------------------------------------------------------
	// shared pieces of the draws
	// ------------------------------------------------------------------
	// the wave: ring k of the funnel moved by (radius + 15 k) along (angle + 300 k); dst + rebase
	static void WaveVertices(int32_t angle, int32_t radius, uint32_t rebase)
	{
		Order *o = (Order *)ORDER;
		for (int ring = 0x13; ring != 0; ring--)
		{
			const int32_t c = ComputeCos(angle) * radius >> 12;
			const int32_t s = ComputeSin(angle) * radius >> 12;
			for (int i = 0x10; i != 0; i--, o++)
			{
				const int16_t *src = (const int16_t *)o->src;
				int16_t *dst = (int16_t *)(o->dst + rebase);
				dst[0] = (int16_t)(src[0] + c);
				dst[1] = src[1];
				dst[2] = (int16_t)(src[2] + s);
			}
			angle += 0x12C;
			radius += 0xF;
		}
	}

	// funnel position of a tick (x / z dwords: the path sample also writes the y / w words)
	static void FunnelPosition(const FunnelNode *f, int16_t counter, uint32_t *x, uint32_t *z)
	{
		if (counter < 0x1E)
		{
			// caster -> targets' centre (t = counter / 29), plus the sway through the caster's matrix
			const int32_t t = shl32((int32_t)counter, 12) / 29;
			const int16_t *cp = CasterPos();
			*(int16_t *)x = (int16_t)((((int32_t)Centre()[0] - cp[0]) * t >> 12) + cp[0]);
			*(int16_t *)z = (int16_t)((((int32_t)Centre()[2] - cp[2]) * t >> 12) + cp[2]);
			int16_t v[3];
			v[0] = (int16_t)(ComputeSin(f->phase) * f->sway >> 12);
			v[1] = 0;
			v[2] = 0;
			MatrixMultiplyVector(Entity(CasterSlot()) + 0x40, v, v);
			*(int16_t *)x = (int16_t)(*(int16_t *)x + v[0]);
			*(int16_t *)z = (int16_t)(*(int16_t *)z + v[2]);
		}
		else
		{
			int32_t e = counter - 0x1E;
			if (e >= 0x22) e -= 0x22;
			*x = *(const uint32_t *)(PathTable() + e * 4);
			*z = *(const uint32_t *)(PathTable() + e * 4 + 2);
		}
	}

	// funnel fade of a tick (prim header +0x0C, +0x1C)
	static void FunnelFade(int16_t counter, int32_t *fade, uint32_t *flags)
	{
		*fade = 0;
		*flags = 0x2033;
		if (counter < 6)
		{
			*fade = 0x1000 - (int32_t)counter * 682;
			*flags = 0x20F3;
		}
		else if (counter >= 0x34)
		{
			*fade = shl32((int32_t)counter - 0x34, 9);
			*flags = 0x20F3;
		}
	}

	// the two funnel draws (outer 2.0 / inner 1.6 horizontal scale, the inner one half faded)
	static void FunnelDraw(int16_t angle, int16_t height, int16_t x, int16_t z, uint32_t verts, int32_t fade, uint32_t flags)
	{
		int16_t ang[3] = { 0, angle, 0 };
		Mat4x3 rot, m;
		ComposeZYXRotationMatrix(ang, &rot);
		rot.t[0] = x;
		rot.t[1] = 0;
		rot.t[2] = z;
		m = rot;
		int32_t sc[3] = { 0x2000, height, 0x2000 };
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(int32_t *)(h + 0xC) = 0;
		*(int32_t *)(h + 8) = 0;
		*(uint32_t *)h = MODEL_Funnel;
		*(uint32_t *)(h + 4) = verts;
		*(uint32_t *)(h + 0x1C) = flags;
		*(int32_t *)(h + 0xC) = fade;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		m = rot;
		sc[0] = 0x19C8;
		sc[2] = 0x19C8;
		sc[1] = height;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		const int32_t f = *(const int32_t *)(h + 0xC);
		*(int32_t *)(h + 0xC) = (0x1000 - f) / 2 + f;
		*(uint32_t *)(h + 0x1C) |= 0xC0;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// one dust sprite at p (header shared by the task's whole pool)
	static void DustDraw(uint8_t *h, int16_t frame, const int16_t *p, int16_t scale)
	{
		*(int16_t *)(h + 4) = frame;
		TransformCameraByShadowRotation(p, scale, -((int32_t)scale >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
	}

	static void DustDrawA(uint8_t *h, const Dust *d)
	{
		DustDraw(h, d->frame, d->pos, d->scale);
	}

	// pool B: the sprite orbits its base position (radius v[0], angle v[2])
	static void DustPosB(const Dust *d, int16_t p[4])
	{
		memcpy(p, d->pos, 8);
		p[0] = (int16_t)(p[0] + (ComputeCos(d->v[2]) * d->v[0] >> 12));
		p[2] = (int16_t)(p[2] + (ComputeSin(d->v[2]) * d->v[0] >> 12));
	}

	static void DustDrawB(uint8_t *h, const Dust *d)
	{
		int16_t p[4];
		DustPosB(d, p);
		DustDraw(h, (int16_t)(d->frame >> 1), p, d->scale);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag155_sand_storm_held.h"
#endif

namespace ff8fx
{
namespace sandstorm155
{
	// ------------------------------------------------------------------
	// Vertex order (0x607090)
	// ------------------------------------------------------------------
	static void VertexOrder()
	{
		Order *o = (Order *)ORDER;
		uint32_t src = MODEL_Funnel + 8, dst = VERTS;
		for (int ring = 0x13; ring != 0; ring--)
			for (int i = 0x10; i != 0; i--, o++, src += 8, dst += 8)
			{
				o->src = src;
				o->dst = dst;
				o->y = *(const int16_t *)(src + 2);
			}
		o = (Order *)ORDER;
		for (int i = 0; i + 1 < 0x130; i++)
			for (int j = i + 1; j < 0x130; j++)
				if (o[i].y < o[j].y)
				{
					const Order t = o[i];
					o[i] = o[j];
					o[j] = t;
				}
	}

	// ------------------------------------------------------------------
	// Wave (0x607130)
	// ------------------------------------------------------------------
	static uint32_t __cdecl WaveTask(TaskNode *n)
	{
		WaveNode *w = (WaveNode *)n;
		// 30 fps layer: see mag155_sand_storm_held.inc
		FX_HELD(held_note_wave(w);)
		WaveVertices(w->angle, w->radius, 0);
		if (DrawOnly()) return 0;
		w->angle = (int16_t)(w->angle + w->spin);
		w->counter++;
		return w->counter >= 0x3C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Funnel (0x607210)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FunnelTask(TaskNode *n)
	{
		FunnelNode *f = (FunnelNode *)n;
		FunnelPosition(f, f->counter, &PosX(), &PosZ());
		int32_t fade;
		uint32_t flags;
		FunnelFade(f->counter, &fade, &flags);
		// 30 fps layer: see mag155_sand_storm_held.inc
		FX_HELD(held_note_funnel(f);)
		FunnelDraw(f->angle, f->height, (int16_t)PosX(), (int16_t)PosZ(), VERTS, fade, flags);
		if (DrawOnly()) return 0;
		const int16_t c = f->counter;
		if (c < 0xA)
		{
			const int16_t g = f->grow;
			f->height = (int16_t)(f->height + g);
			f->grow = (int16_t)(g - (int16_t)(g >> 4));
		}
		f->angle = (int16_t)(f->angle + f->spin);
		f->sway = (int16_t)(f->sway + f->sway_grow);
		const int16_t pv = f->phase_v;
		f->phase = (int16_t)(f->phase + pv);
		f->phase_v = (int16_t)(pv - (int16_t)(pv >> 5));
		if (c == 0x1D)
		{
			// the spline: current position, far side midpoint, near side point, middle point, far
			// side point, current position
			int16_t *sp = Spline();
			const int16_t cx = (int16_t)PosX(), cz = (int16_t)PosZ();
			sp[0] = cx;
			sp[1] = 0;
			sp[2] = cz;
			*(uint32_t *)(sp + 20) = *(const uint32_t *)(sp + 0);
			*(uint32_t *)(sp + 22) = *(const uint32_t *)(sp + 2);
			const int32_t bx = SideB()[0] - cx, bz = SideB()[2] - cz;
			const int32_t ax = SideA()[0] - cx, az = SideA()[2] - cz;
			const int32_t db = bx * bx + bz * bz;
			const int32_t da = ax * ax + az * az;
			int16_t farx, farz;
			if (da > db)
			{
				sp[10] = SideB()[2];
				sp[8] = SideB()[0];
				sp[9] = 0;
				sp[16] = SideA()[0];
				sp[17] = 0;
				farx = SideA()[0];
				farz = SideA()[2];
			}
			else
			{
				sp[8] = SideA()[0];
				sp[9] = 0;
				sp[10] = SideA()[2];
				sp[16] = SideB()[0];
				sp[17] = 0;
				farx = SideB()[0];
				farz = SideB()[2];
			}
			sp[18] = farz;
			sp[4] = (int16_t)(((int32_t)farx + cx) >> 1);
			sp[5] = 0;
			sp[6] = (int16_t)(((int32_t)farz + cz) >> 1);
			void *work = FieldAlloc(0x190);
			SplineSetup(6, sp, work);
			int32_t t = 0;
			for (int16_t *out = PathTable(); out < (int16_t *)0x23F2A70; out += 4, t += 0x1000)
				SplineEval(6, work, out, t / 33);
			FieldFree(0x190);
		}
		f->counter++;
		return f->counter >= 0x3C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dust A (0x6076E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DustATask(TaskNode *n)
	{
		DustNode *t = (DustNode *)n;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_DustA;
		*(int16_t *)(h + 0x24) = 0;
		int alive = 0;
		for (Dust *d = (Dust *)POOL_A; d < (Dust *)POOL_A + 100; d++)
		{
			if (!(d->used & 1)) continue;
			// 30 fps layer: see mag155_sand_storm_held.inc
			FX_HELD(held_note_dust(0, h, d);)
			DustDrawA(h, d);
			if (DrawOnly()) continue;
			d->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				d->used = 0;
				continue;
			}
			d->pos[0] = (int16_t)(d->pos[0] + d->v[0]);
			d->pos[2] = (int16_t)(d->pos[2] + d->v[2]);
			d->v[0] = (int16_t)(d->v[0] - (int16_t)(d->v[0] >> 3));
			d->v[2] = (int16_t)(d->v[2] - (int16_t)(d->v[2] >> 3));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 0x34)
		{
			for (int k = 0; k < 3; k++)
			{
				Dust *d = (Dust *)POOL_A;
				while (d->used != 0)
					if (++d >= (Dust *)POOL_A + 100) goto spawned;
				d->used = 1;
				d->frame = 0;
				d->scale = (int16_t)(CrtRand() % 0xA00 + 0x700);
				const int32_t a = CrtRand() % 4096;
				const int32_t c = ComputeCos(a);
				const int32_t s = ComputeSin(a);
				const int32_t r = CrtRand() % 200;
				*(uint32_t *)&d->pos[0] = PosX();
				*(uint32_t *)&d->pos[2] = PosZ();
				d->pos[0] = (int16_t)(d->pos[0] + (c * (r + 0x96) >> 12));
				d->pos[2] = (int16_t)(d->pos[2] + (s * (r + 0x96) >> 12));
				const int32_t sp = CrtRand() % 150 + 0x3C;
				d->v[0] = (int16_t)(c * sp >> 12);
				d->v[2] = (int16_t)(s * sp >> 12);
			}
		}
	spawned:
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Dust B (0x607920)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DustBTask(TaskNode *n)
	{
		DustNode *t = (DustNode *)n;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_DustB;
		*(int16_t *)(h + 0x24) = 0;
		int alive = 0;
		for (Dust *d = (Dust *)POOL_B; d < (Dust *)POOL_B + 100; d++)
		{
			if (!(d->used & 1)) continue;
			// 30 fps layer: see mag155_sand_storm_held.inc
			FX_HELD(held_note_dust(1, h, d);)
			DustDrawB(h, d);
			if (DrawOnly()) continue;
			d->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				d->used = 0;
				continue;
			}
			d->pos[1] = (int16_t)(d->pos[1] + d->pos[3]);
			d->pos[3] = (int16_t)(d->pos[3] + (int16_t)(d->pos[3] >> 4));
			d->v[0] = (int16_t)(d->v[0] + d->v[1]);
			d->v[1] = (int16_t)(d->v[1] - (int16_t)(d->v[1] >> 3));
			d->v[2] = (int16_t)(d->v[2] + d->v[3]);
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 0x28)
		{
			for (int k = 0; k < 5; k++)
			{
				Dust *d = (Dust *)POOL_B;
				while (d->used != 0)
					if (++d >= (Dust *)POOL_B + 100) goto spawned;
				d->used = 1;
				d->frame = 0;
				d->scale = (int16_t)(CrtRand() % 0x600 + 0x400);
				*(uint32_t *)&d->pos[0] = PosX();
				*(uint32_t *)&d->pos[2] = PosZ();
				d->pos[1] = (int16_t)(-50 - CrtRand() % 400);
				d->pos[3] = (int16_t)(-50 - CrtRand() % 120);
				const int16_t r = (int16_t)(CrtRand() % 240 + 0x78);
				d->v[1] = r;
				d->v[0] = r;
				d->v[2] = (int16_t)(CrtRand() % 4096);
				d->v[3] = (int16_t)(-20 - CrtRand() % 110);
			}
		}
	spawned:
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x606DB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		DirectorNode *r = (DirectorNode *)n;
		if (r->counter == 0)
		{
			int16_t *cp = CasterPos();
			GetDefaultEffectPosition(Entity(CasterSlot()), cp);
			const int16_t y = cp[1];
			cp[1] = cp[3];
			cp[3] = y;
			CalculateCenterPosition(Ctx()->target_mask, Centre());
			VertexOrder();
		}
		if (r->counter == 1)
		{
			WaveNode *w = (WaveNode *)AddTaskToQueue(QEffect(), ORIG_Wave);
			w->counter = 0;
			w->angle = (int16_t)(CrtRand() % 4096);
			w->spin = (int16_t)(-70 - CrtRand() % 70);
			w->radius = 0;
			FunnelNode *f = (FunnelNode *)AddTaskToQueue(QEffect(), ORIG_Funnel);
			f->counter = 0;
			f->grow = 0x400;
			f->height = 0x400;
			f->angle = (int16_t)(CrtRand() % 4096);
			f->spin = (int16_t)(CrtRand() % 70 * 2 + 0x8C);
			f->sway = (int16_t)(CrtRand() % 100 - 50);
			const int32_t g = CrtRand() % 40 + 0x14;
			f->sway_grow = (int16_t)g;
			if (f->sway < 0) f->sway_grow = (int16_t)-g;
			f->phase = 0;
			const int32_t pv = CrtRand() % 150;
			memcpy(SideB(), Centre(), 8);
			memcpy(SideA(), Centre(), 8);
			f->phase_v = (int16_t)(pv + 0xBE);
			SideA()[0] = (int16_t)(SideA()[0] + (-4000 - CrtRand() % 2600));
			SideA()[2] = (int16_t)(SideA()[2] + (CrtRand() % 1300 - 650));
			SideB()[0] = (int16_t)(SideB()[0] + (CrtRand() % 2600 + 4000));
			SideB()[2] = (int16_t)(SideB()[2] + (CrtRand() % 1300 - 650));
			int16_t *sp = Spline();
			const int32_t mx = CrtRand() % 2600;
			sp[13] = 0;
			sp[12] = (int16_t)(mx + Centre()[0] - 1300);
			const int32_t mz = CrtRand() % 1300;
			sp[14] = (int16_t)(mz + Centre()[2] + 600);
		}
		if (r->counter == 3) ((DustNode *)AddTaskToQueue(QEffect(), ORIG_DustA))->counter = 0;
		if (r->counter == 4) ((DustNode *)AddTaskToQueue(QEffect(), ORIG_DustB))->counter = 0;
		if (r->counter == 0x32) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (r->counter == 1) BdPlaySE(SOUND_Wind, 0, 0x80);
		r->counter++;
		return r->counter > 0x3C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x607BB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag155_sand_storm_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (r->counter & 1) ? ARENA_B : ARENA_A;
		const int a = ExecuteTaskQueue(QEffect());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag155_sand_storm()
	{
		register_port(sandstorm155::ORIG_Root, (void *)sandstorm155::RootTask, "155 RootTask", 155);
		register_port(sandstorm155::ORIG_Director, (void *)sandstorm155::DirectorTask, "155 DirectorTask", 155);
		register_port(sandstorm155::ORIG_Wave, (void *)sandstorm155::WaveTask, "155 WaveTask", 155);
		register_port(sandstorm155::ORIG_Funnel, (void *)sandstorm155::FunnelTask, "155 FunnelTask", 155);
		register_port(sandstorm155::ORIG_DustA, (void *)sandstorm155::DustATask, "155 DustATask", 155);
		register_port(sandstorm155::ORIG_DustB, (void *)sandstorm155::DustBTask, "155 DustBTask", 155);
		// 30 fps layer: see mag155_sand_storm_held.inc
		FX_HELD(register_mag155_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag155_sand_storm_held.inc"
#endif
