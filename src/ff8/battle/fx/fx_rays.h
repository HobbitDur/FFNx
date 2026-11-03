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

// Parts library, light rays (Berserk 0x6D0B40 spawn, 0x6D0600 draw + update, 0x6D0870 prims,
// 0x6D0AE0 direction): a table of rays from a point outwards, each a Gouraud triangle + quad fan
// (semi-transparent) turning around the point, its colours fading, its length and width moving.
// Bit-exact with Berserk's code (mag108_berserk.cpp before it was built from here), each piece
// named with its original.

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
#pragma pack(push, 1)
	// a light ray (0x30 bytes)
	struct LightRay
	{
		int16_t pad00;     // +0x00 (never written)
		int16_t tilt;      // +0x02 random, fixed
		int16_t spin;      // +0x04 (rand & 0x7FF) - 0x400, += speed every tick
		int16_t alive;     // +0x06
		uint8_t col0[4];   // +0x08 centre colour (3 bytes fade)
		uint8_t dec0[4];   // +0x0C its fade per tick
		uint8_t col1[4];   // +0x10 middle colour
		uint8_t dec1[4];   // +0x14
		uint8_t col2[4];   // +0x18 tip colour (never set by the spawner: 0 from the node clear)
		uint8_t dec2[4];   // +0x1C
		int16_t speed;     // +0x20 spin step
		int16_t length;    // +0x22 (draw: >> 3)
		int16_t length_vel; // +0x24
		int16_t length_acc; // +0x26
		int16_t width;     // +0x28 (draw: >> 3)
		int16_t width_vel; // +0x2A
		int16_t width_acc; // +0x2C
		int16_t pad2E;
	};
#pragma pack(pop)
	static_assert(sizeof(LightRay) == 0x30, "light ray");

	// the rays' motion: their spin step, length and width, each a speed and an acceleration
	struct RayMotion
	{
		int32_t speed, length, length_vel, length_acc, width, width_vel, width_acc;
	};

	namespace rays
	{
		// 0x6D0AE0: unit direction of a ray (tilt from the -z axis, spin around it)
		static void RayDirection(const LightRay *e, int16_t *out)
		{
			const int32_t s = ComputeSin(e->tilt);
			const int32_t c = parts::ComputeCos(e->tilt);
			out[2] = (int16_t)-c;
			out[1] = (int16_t)(mul32(ComputeSin(e->spin), s) >> 12);
			out[0] = (int16_t)((int32_t)(0u - (uint32_t)mul32(parts::ComputeCos(e->spin), s)) >> 12);
		}

		// 0x6D0870: the ray from `base` to `length` along its direction, `width` wide at the tip
		// (perpendicular on screen, scaled by the tip's depth): a Gouraud triangle (centre colour to
		// half way) and a Gouraud quad (half way to the tip), both semi-transparent
		static uint32_t RayPrims(const LightRay *e, int32_t width, int32_t base, int32_t length, const uint8_t *c0, const uint8_t *c1, const uint8_t *c2,
			uint32_t ot, int32_t shift, uint32_t cursor)
		{
			int16_t d[3];
			RayDirection(e, d);
			// the 4th word is never written by the original (stack; it only reaches the VZ0 high half)
			int16_t v[4];
			v[3] = 0;
			v[0] = (int16_t)(mul32(d[0], base) >> 12);
			v[1] = (int16_t)(mul32(d[1], base) >> 12);
			v[2] = (int16_t)(mul32(d[2], base) >> 12);
			GteLoadV0(v);
			GteRTPS();
			uint8_t *p = (uint8_t *)cursor;
			GteReadSXY2(p + 8);
			int32_t otz1;
			GteReadOTZ(&otz1);
			if (otz1 <= 0) return cursor;
			v[0] = (int16_t)(mul32(d[0], length) >> 12);
			v[1] = (int16_t)(mul32(d[1], length) >> 12);
			v[2] = (int16_t)(mul32(d[2], length) >> 12);
			GteLoadV0(v);
			GteRTPS();
			GteReadSXY2(p + 0x10);
			int32_t otz2;
			GteReadOTZ(&otz2);
			if (otz2 <= 0) return cursor;
			const int32_t sx1 = *(const int16_t *)(p + 8);
			const int32_t sy2 = *(const int16_t *)(p + 0x12);
			const int32_t dx = *(const int16_t *)(p + 0x10) - sx1;
			const int32_t dy = *(const int16_t *)(p + 0xA) - sy2;
			const int32_t len = parts::Sqrt(mul32(dy, dy) + mul32(dx, dx));
			if (len == 0) return cursor;
			uint8_t *q = p + 0x1C;
			int32_t px = shl32(dy, 12) / len;
			px = mul32(px, width) / otz2;
			int32_t py = shl32(dx, 12) / len;
			px >>= 4;
			py = mul32(py, width) / otz2;
			// tip +- the perpendicular
			const int16_t tx = *(const int16_t *)(p + 0x10);
			*(int16_t *)(q + 0x18) = (int16_t)(tx + (int16_t)px);
			*(int16_t *)(q + 0x20) = (int16_t)(tx - (int16_t)px);
			py >>= 4;
			const int16_t ty = *(const int16_t *)(p + 0x12);
			*(int16_t *)(q + 0x1A) = (int16_t)(ty + (int16_t)py);
			*(int16_t *)(q + 0x22) = (int16_t)(ty - (int16_t)py);
			// half way between the centre and the two tip corners
			int16_t h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x18) + *(const int16_t *)(p + 8)) >> 1);
			*(int16_t *)(q + 8) = h;
			*(int16_t *)(p + 0x10) = h;
			h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x20) + *(const int16_t *)(p + 8)) >> 1);
			*(int16_t *)(q + 0x10) = h;
			*(int16_t *)(p + 0x18) = h;
			h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x1A) + *(const int16_t *)(p + 0xA)) >> 1);
			*(int16_t *)(q + 0xA) = h;
			*(int16_t *)(p + 0x12) = h;
			h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x22) + *(const int16_t *)(p + 0xA)) >> 1);
			*(int16_t *)(q + 0x12) = h;
			*(int16_t *)(p + 0x1A) = h;
			*(uint32_t *)(p + 4) = *(const uint32_t *)c0;
			const uint32_t k1 = *(const uint32_t *)c1;
			*(uint32_t *)(q + 0xC) = k1;
			*(uint32_t *)(q + 4) = k1;
			*(uint32_t *)(p + 0x14) = k1;
			*(uint32_t *)(p + 0xC) = k1;
			const uint32_t k2 = *(const uint32_t *)c2;
			*(uint32_t *)(q + 0x1C) = k2;
			*(uint32_t *)(q + 0x14) = k2;
			*(uint32_t *)p = 0x6000000;
			p[7] = 0x32;
			*(uint32_t *)q = 0x8000000;
			q[7] = 0x3A;
			const int32_t z = ((otz1 + otz2) / 2) >> shift;
			const uint32_t bucket = ot + (uint32_t)z * 4;
			InsertPrimAutoDepth(bucket, p);
			InsertPrimAutoDepth(bucket, q);
			return (uint32_t)(q + 0x24);
		}

		// one byte of a ray colour down by its fade, not below 0
		static inline uint8_t FadeByte(uint8_t c, uint8_t d) { const int32_t v = (int32_t)c - d; return (uint8_t)(v < 0 ? 0 : v); }

		// the update part of 0x6D0600: spin, colour fades, length / width motion; dead when every colour
		// is 0 or the length or the width is no longer positive
		static void RayUpdate(LightRay *e)
		{
			e->spin = (int16_t)(e->spin + e->speed);
			e->col0[0] = FadeByte(e->col0[0], e->dec0[0]);
			e->col0[1] = FadeByte(e->col0[1], e->dec0[1]);
			e->col0[2] = FadeByte(e->col0[2], e->dec0[2]);
			e->col1[0] = FadeByte(e->col1[0], e->dec1[0]);
			e->col1[1] = FadeByte(e->col1[1], e->dec1[1]);
			e->col1[2] = FadeByte(e->col1[2], e->dec1[2]);
			e->col2[0] = FadeByte(e->col2[0], e->dec2[0]);
			e->col2[1] = FadeByte(e->col2[1], e->dec2[1]);
			e->col2[2] = FadeByte(e->col2[2], e->dec2[2]);
			if (!e->col0[0] && !e->col0[1] && !e->col0[2] && !e->col1[0] && !e->col1[1] && !e->col1[2] && !e->col2[0] && !e->col2[1] && !e->col2[2])
			{
				e->alive = 0;
				return;
			}
			e->length_vel = (int16_t)(e->length_vel + e->length_acc);
			e->length = (int16_t)(e->length + e->length_vel);
			if (e->length <= 0)
			{
				e->alive = 0;
				return;
			}
			e->width_vel = (int16_t)(e->width_vel + e->width_acc);
			e->width = (int16_t)(e->width + e->width_vel);
			if (e->width <= 0) e->alive = 0;
		}
	}

	// 0x6D0B40: n new rays in free slots of the table (the scan restarts from the first slot for each;
	// stops when the table is full); `colours`: centre colour, its fade, middle colour, its fade (CRT
	// rand: each one's tilt, spin and side). The original's two more arguments are never read.
	inline void RaySpawn(int32_t max, LightRay *table, int32_t n, const uint32_t *colours, const RayMotion &mo)
	{
		if (n <= 0) return;
		int32_t made = 0;
		for (;;)
		{
			LightRay *e = table;
			int32_t left = max;
			for (;;)
			{
				if (left-- == 0) return;
				if (e->alive == 0) break;
				e++;
			}
			e->alive = 1;
			e->tilt = (int16_t)CrtRand();
			e->spin = (int16_t)((CrtRand() & 0x7FF) - 0x400);
			if (CrtRand() & 1) e->speed = (int16_t)mo.speed;
			else e->speed = (int16_t)-mo.speed;
			made++;
			*(uint32_t *)e->col0 = colours[0];
			*(uint32_t *)e->dec0 = colours[1];
			*(uint32_t *)e->col1 = colours[2];
			e->length = (int16_t)mo.length;
			e->width = (int16_t)mo.width;
			*(uint32_t *)e->dec1 = colours[3];
			e->length_vel = (int16_t)mo.length_vel;
			e->length_acc = (int16_t)mo.length_acc;
			e->width_vel = (int16_t)mo.width_vel;
			e->width_acc = (int16_t)mo.width_acc;
			if (made >= n) return;
		}
	}

	// 0x6D0600: every live ray drawn around pos (the frame set up at the first one: the camera's
	// rotation, pos in camera space), then updated: the rays drawn
	inline int32_t RayDraw(int32_t max, LightRay *table, const int16_t *pos, uint32_t &cursor)
	{
		int32_t drawn = 0;
		if (max == 0) return 0;
		Mat4x3 m;
		LightRay *e = table;
		for (int32_t k = max; k; k--, e++)
		{
			if (e->alive == 0) continue;
			if (drawn++ == 0)
			{
				// MAG_103_sub_6D67B0 (dword copy): the camera's rotation (5 dwords)
				for (int i = 0; i < 5; i++) ((uint32_t *)&m)[i] = ((const uint32_t *)&Camera())[i];
				GteSetRotMatrixCtrl(&m);
				GteLoadV0(pos);
				GteMVMVA_RotV0();
				GteReadMAC123(m.t);
				m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
				m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
				m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
			}
			cursor = rays::RayPrims(e, e->width >> 3, 0, e->length >> 3, e->col0, e->col1, e->col2, RenderOT(), 2, cursor);
			rays::RayUpdate(e);
		}
		return drawn;
	}
}
}
