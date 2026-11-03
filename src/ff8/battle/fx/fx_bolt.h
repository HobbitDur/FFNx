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

// Parts library, a lightning strike (Thunder 0x700DE0, 0x7011A0): a full-screen flash quad, sprite
// sequence layers at a point of the target (its 4th word the camera offset), a layer on a plane
// tilted at the target's feet. Bit-exact with Thunder's code (mag003_thunder.cpp before it was
// built from here).

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }

	// MAG_003_EmitScreenFlashQuad (0x7011A0): a semi-transparent flat quad over the whole screen
	// (320 x 216) in the colour word at `colour`, in the render list's bucket + `bucket`
	inline uint32_t ScreenQuad(const void *colour, uint32_t bucket, uint32_t cursor)
	{
		uint8_t *p = (uint8_t *)cursor;
		*(uint16_t *)(p + 8) = 0;
		*(uint16_t *)(p + 0x10) = 0;
		*(uint16_t *)(p + 0xE) = 0;
		*(uint16_t *)(p + 0xA) = 0;
		*(uint16_t *)(p + 0x14) = 0x140;
		*(uint16_t *)(p + 0x16) = 0xD8;
		*(uint16_t *)(p + 0x12) = 0xD8;
		*(uint16_t *)(p + 0xC) = 0x140;
		*(uint32_t *)p = 0x5000000;
		*(uint32_t *)(p + 4) = *(const uint32_t *)colour;
		p[7] = 0x2A;
		InsertPrimAltViewport(var<uint32_t>(0x1D8E04C) + bucket, p);
		return cursor + 0x18;
	}

	// a sequence frame at `point` lowered by `lift` (y + lift), the camera scaled by rot_a, offset by
	// the point's 4th word; `local` (4 words) holds the lowered copy (the originals' stack slot)
	inline void PointLayer(const int16_t *point, int32_t lift, int32_t rot_a, uint32_t seq, int16_t frame, int16_t *local, uint32_t &cursor)
	{
		const int16_t *at = point;
		if (lift)
		{
			memcpy(local, point, 8);
			local[1] = (int16_t)(local[1] + lift);
			at = local;
		}
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(at, rot_a, point[3]);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}

	// MAG_003_sub_6D5940: a rotation about the X axis (translation and pad untouched)
	inline void RotX(int16_t angle, Mat4x3 *m)
	{
		const int32_t s = ComputeSin(angle);
		const int32_t c = parts::ComputeCos(angle);
		m->m[1][2] = (int16_t)-s;
		m->m[2][1] = (int16_t)s;
		m->m[0][0] = 0x1000;
		m->m[0][1] = 0;
		m->m[0][2] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[2][0] = 0;
		m->m[2][2] = (int16_t)c;
	}

	// a sequence frame on a plane tilted `tilt` about x at (x, the entity's feet, z) of `point` in
	// camera space, pushed `push` away; `local` (4 words) the stack slot its vector is built in (its
	// 4th word: what a lowered layer left there this tick, else 0 - only the GTE's VZ0 high half)
	inline void GroundLayer(const int16_t *point, const uint8_t *entity, int32_t tilt, int32_t push, uint32_t seq, int16_t frame, int16_t *local,
	                        uint32_t &cursor)
	{
		Mat4x3 m = {};
		RotX((int16_t)tilt, &m);
		local[0] = point[0];
		local[1] = *(const int16_t *)(entity + 0x24);
		local[2] = point[2];
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(local);
		GteMVMVA_RotV0();
		GteReadMAC123(m.t);
		m.t[0] += Camera().t[0];
		m.t[1] += Camera().t[1];
		m.t[2] = m.t[2] + Camera().t[2] + push;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}
}
}
