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

// Parts library, the swirl ribbon (Berserk 0x6D0C20, 0x6D0DC0 push, 0x6D0E50 draw): a textured
// ribbon of up to 32 positions spiralling in towards a point, facing the camera: Gouraud-shaded
// textured quads between consecutive positions, each between two texture-window packets, the
// positions' brightness rising then falling, their angles waving. Bit-exact with Berserk's code
// (mag108_berserk.cpp before it was built from here), each piece named with its original.

#pragma once

#include "fx_barrier.h"
#include "fx_sway.h"

namespace ff8fx
{
namespace layouts
{
#pragma pack(push, 1)
	// a ribbon position (0x20 bytes)
	struct RibbonEntry
	{
		int16_t angle;     // +0x00 around the point, waving (+ sin(phase) >> 6 per tick)
		int16_t z;         // +0x02 depth (the ribbon's frame)
		int16_t radius;    // +0x04
		int16_t phase;     // +0x06 wave phase, + 0x200 per tick
		uint32_t sxy0;     // +0x08 projections of the two edge points
		int32_t otz0;      // +0x0C (compared as int16)
		uint32_t sxy1;     // +0x10
		int32_t otz1;      // +0x14
		int16_t bright;    // +0x18 0x10..0xFF
		int16_t bright_vel; // +0x1A
		uint16_t shade;    // +0x1C texture u offset
		uint16_t pad1E;
	};
	// a ribbon task's fields after its counter (0x412 bytes)
	struct Ribbon
	{
		int16_t pad0E;
		int16_t angle;     // +0x02 angle of the next position, + step per position
		int16_t pad12;     // (set to 0)
		int16_t pad14;
		int16_t amp;       // +0x08 radius wave amplitude
		int32_t count;     // +0x0A positions
		const TaskNode *owner; // +0x0E its spawner's node (the original keeps this, not the point inside it)
		RibbonEntry trail[32]; // +0x12
	};
	// the ribbon's stack frame (0x6D0C20 esp+0x10..): the basis's translation is the point
	struct RibbonFrame
	{
		int32_t v[3];      // +0x10 -x, -y, z of the point (camera space)
		uint32_t pad1C;
		Mat4x3 basis;      // +0x20 (translation +0x34 = the point in camera space)
		Mat4x3 roll;       // +0x40 (3x3 only)
	};
#pragma pack(pop)
	static_assert(sizeof(RibbonEntry) == 0x20 && sizeof(Ribbon) == 0x412 && sizeof(RibbonFrame) == 0x50, "ribbon");

	// the ribbon's motion: positions pushed over its first ticks, how many a tick, its angle step,
	// the brightness's change a tick
	struct RibbonMotion
	{
		int32_t push_ticks, per_tick, angle_step, fade;
	};

	namespace ribbon
	{
		inline void BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
		using sway::GteSetSXY012;
		using parts::GteNCLIP;

		// 0x6D9CF0 (Blizzaga's ribbon point): x, y on a circle of radius r / 4, z = z / 4 - z
		static void RibbonPoint(int16_t *out, int32_t angle, int32_t z, int32_t r)
		{
			out[0] = (int16_t)(mul32(parts::ComputeCos(angle), r) >> 14);
			out[1] = (int16_t)(mul32(ComputeSin(angle), r) >> 14);
			out[2] = (int16_t)((z >> 2) - z);
		}

		// 0x6D0DC0: a new position in front (the others move back while fewer than max; at max the
		// front one is overwritten)
		static int32_t RibbonPush(int32_t max, int32_t n, RibbonEntry *t, int32_t angle, int32_t z, int32_t radius, int32_t phase, int32_t bright,
			int32_t bright_vel, int32_t shade)
		{
			if (n < max)
			{
				uint32_t *d = (uint32_t *)t;
				for (int32_t k = n * 8; k > 0; k--) d[k + 7] = d[k - 1];
				n++;
			}
			t[0].angle = (int16_t)angle;
			t[0].z = (int16_t)z;
			t[0].radius = (int16_t)radius;
			t[0].phase = (int16_t)phase;
			t[0].bright = (int16_t)bright;
			t[0].bright_vel = (int16_t)bright_vel;
			t[0].shade = (uint16_t)shade;
			return n;
		}

		// 0x6D0E50, first part: every position still at 0x10 or brighter steps its brightness
		// (speed + d, brightness + speed / 8, kept in 0x10..0xFF)
		static void RibbonFade(int32_t n, RibbonEntry *t, int32_t d)
		{
			for (int32_t k = 0; k < n; k++)
			{
				RibbonEntry &e = t[k];
				if (e.bright < 0x10) continue;
				e.bright_vel = (int16_t)(e.bright_vel + d);
				e.bright = (int16_t)(e.bright + (int16_t)(e.bright_vel >> 3));
				if (e.bright < 0x10) e.bright = 0x10;
				else if (e.bright > 0xFF) e.bright = 0xFF;
			}
		}

		// 0x6D0E50, rest: the last position is dropped when it and the one before are dark; every
		// position's two edge points are projected (angle -+ 0x80), then its angle waves; a textured
		// Gouraud quad between consecutive positions (brightness as colour, u = shade + position's
		// shade), between two texture-window packets (`strip`, `reset`: their RECTs), unless a corner
		// is behind the camera
		static uint32_t RibbonStrip(int32_t *pn, RibbonEntry *t, int32_t shade, const uint8_t *strip, const uint8_t *reset, uint32_t ot, int32_t shift,
		                            uint32_t cursor)
		{
			int32_t n = *pn;
			if (n < 2)
			{
				*pn = n;
				return cursor;
			}
			if (t[n - 1].bright <= 0x10 && t[n - 2].bright <= 0x10)
			{
				n--;
				if (n < 2)
				{
					*pn = n;
					return cursor;
				}
			}
			for (int32_t k = 0; k < n; k++)
			{
				RibbonEntry &e = t[k];
				// the 4th words are never written by the original (stack; they only reach the VZ0 high half)
				int16_t a[4], b[4];
				a[3] = 0;
				b[3] = 0;
				RibbonPoint(a, (int32_t)e.angle - 0x80, e.z, e.radius);
				GteLoadV0(a);
				GteRTPS();
				GteReadSXY2(&e.sxy0);
				GteReadOTZ(&e.otz0);
				RibbonPoint(b, (int32_t)e.angle + 0x80, e.z, e.radius);
				GteLoadV0(b);
				GteRTPS();
				GteReadSXY2(&e.sxy1);
				GteReadOTZ(&e.otz1);
				const int32_t s = ComputeSin(e.phase);
				e.phase = (int16_t)(e.phase + 0x200);
				e.angle = (int16_t)(e.angle + (int16_t)(s >> 6));
			}
			shift += 2;
			uint8_t *p = (uint8_t *)cursor;
			for (int32_t k = 0; k < n - 1; k++)
			{
				const RibbonEntry &a = t[k], &b = t[k + 1];
				if ((int16_t)a.otz0 <= 0 || (int16_t)a.otz1 <= 0 || (int16_t)b.otz0 <= 0 || (int16_t)b.otz1 <= 0) continue;
				uint8_t *q = p;
				p += 0x34;
				uint32_t u = (uint32_t)(shade + a.shade);
				uint32_t u2 = u + 0xF;
				if ((int32_t)(u2 | u) > 0xFF)
				{
					u -= 0x80;
					u2 -= 0x80;
				}
				*(uint32_t *)(q + 0x18) = u | 0xB9FF00;
				*(uint32_t *)(q + 0xC) = u | 0x3D94C000;
				*(uint32_t *)(q + 8) = a.sxy0;
				*(uint32_t *)(q + 0x14) = a.sxy1;
				int32_t mac1, mac2;
				GteSetSXY012(a.sxy0, a.sxy1, b.sxy0);
				GteNCLIP();
				parts::GteReadMAC0(&mac1);
				GteSetSXY012(a.sxy1, b.sxy1, b.sxy0);
				GteNCLIP();
				parts::GteReadMAC0(&mac2);
				const int32_t s1 = mac1 > 0, s2 = mac2 > 0;
				if (s1 == s2)
				{
					*(uint32_t *)(q + 0x20) = b.sxy0;
					*(uint32_t *)(q + 0x2C) = b.sxy1;
					*(uint32_t *)(q + 0x24) = u2 | 0xC000;
					u2 |= 0xFF00;
				}
				else
				{
					*(uint32_t *)(q + 0x20) = b.sxy1;
					*(uint32_t *)(q + 0x2C) = b.sxy0;
					*(uint32_t *)(q + 0x24) = u2 | 0xFF00;
					u2 |= 0xC000;
				}
				*(uint32_t *)(q + 0x30) = u2;
				uint32_t c = (uint32_t)(int32_t)a.bright;
				const uint32_t ca = (((c << 8) | c) << 8) | c;
				*(uint32_t *)(q + 0x10) = ca;
				*(uint32_t *)(q + 4) = ca;
				c = (uint32_t)(int32_t)b.bright;
				const uint32_t cb = (((c << 8) | c) << 8) | c;
				*(uint32_t *)q = 0xC000000;
				q[7] = 0x3E;
				*(uint32_t *)(q + 0x28) = cb;
				*(uint32_t *)(q + 0x1C) = cb;
				const int32_t z = ((int32_t)(int16_t)b.otz1 + (int16_t)b.otz0 + (int16_t)a.otz1 + (int16_t)a.otz0) >> shift;
				uint8_t *w = p;
				p += 0xC;
				*(uint32_t *)w = 0x2000000;
				const uint32_t bucket = ot + (uint32_t)z * 4;
				*(uint32_t *)(w + 4) = barrier::TexWindow(strip);
				*(uint32_t *)(w + 8) = 0;
				InsertPrimAutoDepth(bucket, w);
				InsertPrimAutoDepth(bucket, q);
				w = p;
				p += 0xC;
				*(uint32_t *)w = 0x2000000;
				*(uint32_t *)(w + 4) = barrier::TexWindow(reset);
				*(uint32_t *)(w + 8) = 0;
				InsertPrimAutoDepth(bucket, w);
			}
			*pn = n;
			return (uint32_t)p;
		}
	}

	// a new ribbon of `owner`: its first angle, the wave amplitude of its radius
	inline void RibbonStart(Ribbon *r, const TaskNode *owner, int32_t angle, int32_t amp)
	{
		r->owner = owner;
		r->angle = (int16_t)angle;
		r->pad12 = 0;
		r->amp = (int16_t)amp;
	}

	// one tick (`counter`) of a ribbon: over its first push_ticks + 1 ticks per_tick new positions a
	// tick (position e: w = 1 - e / 24 of the way out, its radius waving with sin(w / 2)), then the
	// ribbon drawn in a basis facing the camera from its point (`point_at` bytes into its owner), its
	// texture shifted by counter << 5
	inline void RibbonTick(Ribbon *r, uint32_t point_at, int32_t counter, const RibbonMotion &mo, const uint8_t *strip, const uint8_t *reset, uint32_t &cursor)
	{
		RibbonFrame f = {};
		// the point in camera space (the basis's translation)
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0((const int16_t *)((const uint8_t *)r->owner + point_at));
		GteMVMVA_RotV0Tr();
		GteReadMAC123(f.basis.t);
		for (int32_t i = 0; i < mo.per_tick; i++)
		{
			if (counter > mo.push_ticks) continue;
			const int32_t e = i + counter * mo.per_tick;
			const int32_t x = shl32(e, 12);
			int32_t q = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 2;
			q += (int32_t)((uint32_t)q >> 31);
			const int32_t w = 0x1000 - q;
			const int32_t z = mul32(f.basis.t[2], w) >> 12;
			const int32_t radius = (mul32(ComputeSin(w >> 1), r->amp) >> 12) + w;
			const int32_t shade = (7 - (e & 7)) << 4;
			const int32_t count = ribbon::RibbonPush(0x20, r->count, r->trail, r->angle, z, radius, w, 0x10, 0x100, shade);
			r->angle = (int16_t)(r->angle + mo.angle_step);
			r->count = count;
		}
		// a basis facing the camera from the point (times a z rotation by 0), loaded
		f.v[0] = (int32_t)(0u - (uint32_t)f.basis.t[0]);
		f.v[1] = (int32_t)(0u - (uint32_t)f.basis.t[1]);
		f.v[2] = f.basis.t[2];
		ribbon::BuildOrthonormalBasis(f.v, &f.basis);
		barrier::ZRotation(0, &f.roll);
		barrier::MatMul56C220(&f.basis, &f.roll);
		GteSetRotMatrix(&f.basis);
		GteSetTransVector(&f.basis);
		ribbon::RibbonFade(r->count, r->trail, mo.fade);
		cursor = ribbon::RibbonStrip(&r->count, r->trail, shl32(counter, 5), strip, reset, RenderOT(), 2, cursor);
	}
}
}
