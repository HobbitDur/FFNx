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

// Parts library, the effect library's runes (Double 0x8D5060.., Triple its bigger sibling): prim
// models at a node's place, rotation, scale, colour and fade; flipbook frames turned by two angles;
// a model's vertices blended between two vertex sets; places on a circle around a centre; the
// column's flash and the glow's pulse; a raw textured quad at a projected point. Bit-exact with
// Double's code (mag004_double.cpp before it was built from here).

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
	inline void RotateY(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD8A0)(m, a); }
	inline void InsertPrimRaw(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline void CopyAnchorTopFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC720)(n, out); } // the target's top anchor (emitter +0x40)

	// sub_8D5440: prim model `model` at `pos`, rotated by `angle` (BuildRotationMatrixFromAngles),
	// scaled by `scale` (3 x 4.12), in `rgb` (a colour word) and `fade` (nothing when hidden)
	inline void EffectModelDraw(const int16_t *pos, bool hidden, const int16_t *angle, const int32_t *scale, uint32_t rgb, int32_t fade,
	                            uint32_t model, uint32_t &cursor)
	{
		if (hidden) return;
		Mat4x3 m;
		BuildRotationMatrixFromAngles(angle, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		Scale3DMatrix(&m, scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = rgb;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// sub_8D5950: flipbook `seq` frame `frame` at `pos`, turned by its Y then X angle, scaled, in
	// `rgb` (draw mode 4; nothing when hidden)
	inline void EffectSpriteTurned(const int16_t *pos, bool hidden, int16_t ax, int16_t ay, const int32_t *scale, uint32_t seq, int16_t frame,
	                               uint32_t rgb, uint32_t &cursor)
	{
		if (hidden) return;
		Mat4x3 m;
		parts::IdentityMatrix(&m);
		RotateY(&m, ay);
		parts::RotateX(&m, ax);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		Scale3DMatrix(&m, scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint32_t *)(h + 0x1C) = rgb;
		*(int16_t *)(h + 0x24) = 4;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}

	// sub_8D57C0: vertex blend out = a * (1 - t) + b * t (4.12) of the vertex blocks (+4 = count,
	// 8 bytes per vertex from +8), through the GTE (GPF then GPL, 3 words stored per vertex)
	inline void MorphVertices(const uint8_t *a, const uint8_t *b, uint8_t *out, int16_t t)
	{
		const uint8_t *pa = a + 8;
		const uint8_t *pb = b + 8;
		uint8_t *po = out + 8;
		int32_t count = *(const int32_t *)(a + 4);
		int32_t *w = (int32_t *)FieldAlloc(8);
		w[1] = t;
		w[0] = 0x1000 - t;
		for (; count > 0; count--)
		{
			GteSetIR0(w[0]);
			GteLoadIR123(pa);
			GteGPF();
			GteSetIR0(w[1]);
			GteLoadIR123(pb);
			GteGPL();
			GteStoreIR123(po);
			pa += 8;
			pb += 8;
			po += 8;
		}
		FieldFree(8);
	}

	// `pos` = `base` (4 words) + radius x (sin, cos)(angle) / divisor in x / z (Double's runes,
	// columns, glows and motes)
	inline void CirclePlace(int16_t *pos, const int16_t *base, int16_t angle, int32_t radius, int32_t divisor)
	{
		memcpy(pos, base, 8);
		pos[0] = (int16_t)(pos[0] + mul32(ComputeSin(angle), radius) / divisor);
		pos[2] = (int16_t)(pos[2] + mul32(parts::ComputeCos(angle), radius) / divisor);
	}

	// pos += `v` turned by `angle` about Y (Double's big rune's sprites)
	inline void TurnedOffset(int16_t *pos, int16_t angle, int16_t *v)
	{
		Mat4x3 m;
		parts::IdentityMatrix(&m);
		RotateY(&m, angle);
		MatrixMultiplyVector(&m, v, v);
		pos[0] = (int16_t)(pos[0] + v[0]);
		pos[1] = (int16_t)(pos[1] + v[1]);
		pos[2] = (int16_t)(pos[2] + v[2]);
	}

	// a flash over a model (Double's column): state 1 / 3 turn it on at fade `first` / `second` in
	// grey `grey`, 2 / 4 fade it by `step_first` / `step_second` a tick, then it is off (fade 0,
	// black)
	struct FadePulse
	{
		int32_t first, second, step_first, step_second, grey;
	};
	inline void FadePulseStep(int16_t *state, int16_t *on, int16_t *fade, uint8_t *rgb, const FadePulse &p)
	{
		const int16_t s = *state;
		switch (s)
		{
		case 1:
		case 3:
			*on = 1;
			*fade = (int16_t)(s == 1 ? p.first : p.second);
			rgb[0] = rgb[1] = rgb[2] = (uint8_t)p.grey;
			*state = (int16_t)(s + 1);
			return;
		case 2:
			*fade = (int16_t)(*fade - p.step_first);
			if (*fade > 0) return;
			break;
		case 4:
			*fade = (int16_t)(*fade - p.step_second);
			if (*fade > 0) return;
			break;
		default: return;
		}
		*fade = 0;
		rgb[0] = rgb[1] = rgb[2] = 0;
		*on = 0;
		*state = 0;
	}

	// a grey pulse (Double's glow): state 1 / 3 set `peak`, 2 / 4 lower it by `step` a tick back to
	// `floor` (then off)
	inline void GreyPulseStep(int16_t *state, uint8_t *rgb, int32_t peak, int32_t step, int32_t floor)
	{
		switch (*state)
		{
		case 1:
		case 3:
			rgb[0] = rgb[1] = rgb[2] = (uint8_t)peak;
			*state = (int16_t)(*state + 1);
			break;
		case 2:
		case 4:
			rgb[1] = (uint8_t)(rgb[1] - step);
			rgb[0] = (uint8_t)(rgb[0] - step);
			rgb[2] = (uint8_t)(rgb[2] - step);
			if (rgb[0] <= floor) *state = 0;
			break;
		default: break;
		}
	}

	// a raw textured quad (POLY_FT4, semi-transparent) of the texture page (tx, ty), CLUT (cx, cy),
	// texels (u, v) w x h, its bottom edge centred on `pos` projected (screen / 8), straight into the
	// render list (Double's flash)
	struct TexQuad
	{
		int32_t tx, ty, cx, cy, u, v, w, h;
	};
	inline uint32_t ScreenTexQuad(const int16_t *pos, const TexQuad &q, uint32_t cursor)
	{
		uint8_t *p = (uint8_t *)cursor;
		const int32_t ty = (int16_t)q.ty;
		uint32_t tp = ((uint32_t)(uint16_t)q.tx & 0x3C0) | 0x2800;
		tp = (uint32_t)(uint16_t)((int16_t)tp >> 6);
		tp |= (uint32_t)(ty >> 4) & 0x10;
		tp |= ((uint32_t)ty & 0x200) << 2;
		*(uint16_t *)(p + 0x16) = (uint16_t)tp;
		const uint8_t v = (uint8_t)q.v;
		p[0xD] = v;
		*(uint16_t *)(p + 0xE) = (uint16_t)((((uint32_t)(uint16_t)q.cx >> 4) & 0x3F) | (uint16_t)((uint16_t)q.cy << 6));
		const uint8_t u = (uint8_t)q.u;
		p[0xC] = u;
		p[0x1C] = u;
		const uint8_t u1 = (uint8_t)(u + (uint8_t)q.w);
		p[0x15] = v;
		const uint8_t v1 = (uint8_t)(v + (uint8_t)q.h);
		*(uint32_t *)p = 0x9000000;
		p[7] = 0x2F;
		p[0x14] = u1;
		p[0x1D] = v1;
		p[0x24] = u1;
		p[0x25] = v1;
		GteSetTransVectorCtrl(&Camera());
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		int16_t sxy[4];
		GteReadSXY2(sxy);
		sxy[0] = (int16_t)(sxy[0] >> 3);
		sxy[1] = (int16_t)(sxy[1] >> 3);
		const int32_t half = (int16_t)q.w / 2;
		const int16_t hh = (int16_t)q.h;
		*(int16_t *)(p + 8) = (int16_t)(sxy[0] - half);
		*(int16_t *)(p + 0xA) = (int16_t)(sxy[1] - hh);
		*(int16_t *)(p + 0x10) = (int16_t)(sxy[0] + half);
		*(int16_t *)(p + 0x12) = (int16_t)(sxy[1] - hh);
		*(int16_t *)(p + 0x18) = (int16_t)(sxy[0] - half);
		*(int16_t *)(p + 0x1A) = sxy[1];
		*(int16_t *)(p + 0x20) = (int16_t)(sxy[0] + half);
		*(int16_t *)(p + 0x22) = sxy[1];
		InsertPrimRaw(var<uint32_t>(0x1D8E04C), p);
		return (uint32_t)(p + 0x28);
	}
}
}
