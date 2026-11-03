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

// Parts library, the ripple silhouette (Flare 0x6BC8A0): an entity's model redrawn flat in its
// colour, its vertices rippled on screen around a point (a weight row by distance, a sine wobble),
// after its ground shadow, into the battle's frame arena; the entity itself hidden meanwhile. Bit-
// exact with Flare's code (mag124_flare.cpp before it was built from here), each piece named with
// its original.
//
// Work block (Field_Alloc 0x84): +0x00 camera x entity matrix, then S = +0x20:
//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 triangle / quad counts (6 words),
//   +0x14 clip rectangle (x0, y0, x1, y1), +0x1C colour, +0x20 object mask,
//   +0x24..+0x43 four vertices (sx, sy, otz, clip flags), +0x44 NCLIP, +0x4C OTZ,
//   +0x54 weight row, +0x58 radius, +0x5A / +0x5C the point on screen, +0x5E largest offset,
//   +0x60 ripple phase, +0x62 ripple amplitude.

#pragma once

#include "fx_sway.h"

namespace ff8fx
{
namespace layouts
{
	// the ripple's tables (Flare's, one after the other in its data: the distance lookup reads one
	// row past its own table, at |dy| = 128, into the weights)
	struct RippleTables
	{
		const uint8_t *distance;   // u8 [128][128]: the distance of (|dy|, |dx|)
		const uint16_t *weights;   // u16 [20][128]: the offset's weight by distance (4.12), per step
		const int16_t *sine;       // s16 [4096]
	};
	static const uint32_t RIPPLE_TABLES_SIZE = 0x4000 + 0x1400 + 0x2000;
	// the three tables, one after the other from `base`
	inline RippleTables RippleTablesAt(uint32_t base)
	{
		return { (const uint8_t *)base, (const uint16_t *)(base + 0x4000), (const int16_t *)(base + 0x5400) };
	}

	namespace ripple
	{
		using parts::F;
		using sway::GteLoadV0u;
		using sway::GteSetSXY012;
		using sway::GteSetSZ123;
		using sway::GteSetSZ0123;
		using sway::CopyVertex;
		using parts::GteNCLIP;
		using parts::GteReadMAC0;
		using parts::GteAVSZ3;
		using parts::GteAVSZ4;
		using parts::GteReadOTZWord;
		inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
		// sub_5088A0: an entity's ground shadow (returns the packet cursor)
		inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }

		// sub_6BCE40: one bone group of vertices, projected, rippled, clip-flagged
		static void RippleVertices(uint8_t **objp, uint8_t **outp, uint8_t *s, const RippleTables &tables)
		{
			uint8_t *p = *objp;
			uint8_t *out = *outp;
			int32_t n = *(const int16_t *)p;
			p += 2;
			if (n == 0)
			{
				*objp = p;
				*outp = out;
				return;
			}
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0u(p);
				GteRTPS();
				p += 6;
				GteReadSXY2(s + 0x24);
				F<int16_t>(s, 0x24) = (int16_t)(F<int16_t>(s, 0x24) >> 3);
				F<int16_t>(s, 0x26) = (int16_t)(F<int16_t>(s, 0x26) >> 3);
				GteReadOTZ(s + 0x28);
				if (F<uint16_t>(s, 0x28) == 0)
					*(uint16_t *)(out + 6) = 0x10;
				else
				{
					const int16_t cx = F<int16_t>(s, 0x5A);
					const int16_t cy = F<int16_t>(s, 0x5C);
					const int16_t dxs = (int16_t)(F<int16_t>(s, 0x24) - cx);
					const int16_t dys = (int16_t)(F<int16_t>(s, 0x26) - cy);
					const int32_t ex = dxs;
					int32_t adx = ex;
					if (dxs < 0) adx = -adx;
					if ((int16_t)adx >= 0x80) adx = 0x80;
					const int32_t ey = dys;
					int32_t ady = ey;
					if (dys < 0) ady = -ady;
					if ((int16_t)ady >= 0x80) ady = 0x80;
					if (F<int16_t>(s, 0x5E) < (int16_t)adx) F<int16_t>(s, 0x5E) = (int16_t)adx;
					if (F<int16_t>(s, 0x5E) < (int16_t)ady) F<int16_t>(s, 0x5E) = (int16_t)ady;
					const uint32_t d = tables.distance[(int32_t)(int16_t)adx + shl32((int16_t)ady, 7)];
					if (d != 0)
					{
						const int32_t radius = F<int16_t>(s, 0x58);
						if ((int32_t)d < radius)
						{
							const int32_t q = (int32_t)(d << 7) / radius;
							const uint32_t w = *(const uint16_t *)(F<uint32_t>(s, 0x54) + q * 2);
							const uint32_t ph = ((uint32_t)F<uint16_t>(s, 0x60) + (uint32_t)shl32(ey, 6)) & 0xFFF;
							const int32_t ox = mul32(ex, (int32_t)w) >> 12;
							int32_t wob = mul32(tables.sine[ph], F<int16_t>(s, 0x62)) >> 12;
							wob = mul32(wob, F<int16_t>(s, 0x58)) >> 14;
							const int32_t oy = mul32(ey, (int32_t)w) >> 12;
							F<int16_t>(s, 0x24) = (int16_t)(ox + cx + wob);
							F<int16_t>(s, 0x26) = (int16_t)(oy + cy);
						}
					}
					const int16_t vx = F<int16_t>(s, 0x24);
					if (vx < F<int16_t>(s, 0x14)) s[0x2A] |= 1;
					else if (vx >= F<int16_t>(s, 0x18)) s[0x2A] |= 2;
					const int16_t vy = F<int16_t>(s, 0x26);
					if (vy < F<int16_t>(s, 0x16)) s[0x2A] |= 4;
					else if (vy >= F<int16_t>(s, 0x1A)) s[0x2A] |= 8;
					F<int16_t>(s, 0x24) = (int16_t)(vx << 3);
					F<int16_t>(s, 0x26) = (int16_t)(vy << 3);
					*(uint32_t *)out = F<uint32_t>(s, 0x24);
					*(uint32_t *)(out + 4) = F<uint32_t>(s, 0x28);
				}
				out += 8;
			} while (--left != 0);
			*objp = p;
			*outp = out;
		}

		// sub_6BCB50: the object's textured triangles and quads, flat in the colour (each polygon's own
		// semi-transparency bit kept)
		static uint32_t RipplePolygons(uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
		{
			uint8_t *pk = (uint8_t *)cursor;
			const uint8_t *verts = F<uint8_t *>(s, 4);
			const uint8_t *p = F<uint8_t *>(s, 0);
			uint16_t k = F<uint16_t>(s, 8);
			F<uint16_t>(s, 8) = (uint16_t)(k - 1);
			if (k != 0)
			{
				do
				{
					CopyVertex(s, 0x24, verts, *(const uint16_t *)p);
					CopyVertex(s, 0x2C, verts, *(const uint16_t *)(p + 2));
					CopyVertex(s, 0x34, verts, *(const uint16_t *)(p + 4));
					const uint8_t a = s[0x2A], b = s[0x32], c = s[0x3A];
					if ((uint8_t)(a | b | c) < 0x10 && !(c & (a & b)))
					{
						GteSetSXY012(F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x34));
						GteNCLIP();
						GteReadMAC0(s + 0x44);
						if (F<int32_t>(s, 0x44) >= 0)
						{
							GteSetSZ123(F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x38));
							GteAVSZ3();
							*(uint32_t *)pk = 0x7000000;
							*(uint32_t *)(pk + 4) = (((uint32_t)*(const uint16_t *)(p + 0xE) & 0x200 | 0x2400) << 16) | F<uint32_t>(s, 0x1C);
							*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x24);
							*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x2C);
							*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x34);
							*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
							*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
							*(uint16_t *)(pk + 0x16) &= 0xFDFF;
							*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
							GteReadOTZWord(s + 0x4C);
							InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(s, 0x4C) >> shift) * 4, pk);
							pk += 0x20;
						}
					}
					k = F<uint16_t>(s, 8);
					p += 0x10;
					F<uint16_t>(s, 8) = (uint16_t)(k - 1);
				} while (k != 0);
			}
			k = F<uint16_t>(s, 0xA);
			F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
			if (k != 0)
			{
				p += 4;
				do
				{
					CopyVertex(s, 0x24, verts, *(const uint16_t *)(p - 4));
					CopyVertex(s, 0x2C, verts, *(const uint16_t *)(p - 2));
					CopyVertex(s, 0x34, verts, *(const uint16_t *)p);
					CopyVertex(s, 0x3C, verts, *(const uint16_t *)(p + 2));
					const uint8_t d = s[0x42], a = s[0x2A], b = s[0x32], c = s[0x3A];
					if ((uint8_t)(d | a | b | c) < 0x10 && !(c & (d & a & b)))
					{
						GteSetSXY012(F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x34));
						GteNCLIP();
						GteReadMAC0(s + 0x44);
						if (F<int32_t>(s, 0x44) >= 0)
						{
							GteSetSZ0123(F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x38), F<uint16_t>(s, 0x40));
							GteAVSZ4();
							*(uint32_t *)pk = 0x9000000;
							*(uint32_t *)(pk + 4) = (((uint32_t)*(const uint16_t *)(p + 0xA) & 0x200 | 0x2C00) << 16) | F<uint32_t>(s, 0x1C);
							*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x24);
							*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x2C);
							*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x34);
							*(uint32_t *)(pk + 0x20) = F<uint32_t>(s, 0x3C);
							*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 4);
							*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 8);
							*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0xC);
							*(uint16_t *)(pk + 0x16) &= 0xFDFF;
							*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0xE);
							GteReadOTZWord(s + 0x4C);
							InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(s, 0x4C) >> shift) * 4, pk);
							pk += 0x28;
						}
					}
					k = F<uint16_t>(s, 0xA);
					p += 0x14;
					F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
				} while (k != 0);
			}
			return (uint32_t)pk;
		}

		// sub_6BC9F0: every object of a model the mask shows (bone groups of vertices, then polygons)
		static uint32_t RippleModel(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor, const RippleTables &tables)
		{
			const uint8_t *table = *(uint8_t *const *)(model + 4);
			const uint8_t *bones = *(uint8_t *const *)model + 0x10;
			int32_t count = *(const int32_t *)table;
			const uint32_t *offs = (const uint32_t *)(table + 4);
			for (int32_t i = 0; i < count; i++)
			{
				uint8_t *obj = (uint8_t *)(*(uint8_t *const *)(model + 4) + offs[i]);
				if (!(F<uint32_t>(s, 0x20) & (1u << (i & 31)))) continue;
				uint8_t *out = F<uint8_t *>(s, 4);
				int32_t groups = *(const int16_t *)obj;
				obj += 2;
				if (groups > 0)
				{
					do
					{
						int32_t bone = *(const int16_t *)obj;
						obj += 2;
						const Mat4x3 *bm = (const Mat4x3 *)(bones + bone * 0x30 + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						RippleVertices(&obj, &out, s, tables);
					} while (--groups != 0);
				}
				obj = (uint8_t *)(((uintptr_t)obj + 3) & ~(uintptr_t)3);
				for (int w = 0; w < 6; w++)
				{
					F<uint16_t>(s, 8 + 2 * w) = *(const uint16_t *)obj;
					obj += 2;
				}
				F<uint8_t *>(s, 0) = obj;
				cursor = RipplePolygons(s, ot, shift, cursor);
			}
			return cursor;
		}

		// sub_6BC8A0: the shadow, then the body (+0x64) and the weapon (+0x78) of the entity, into the
		// frame arena; returns the largest ripple offset (the next draw's radius)
		static int32_t DrawSilhouette(uint8_t *entity, int16_t sx, int16_t sy, int16_t radius, int32_t step, int32_t amplitude, int32_t phase,
		                              const RippleTables &tables)
		{
			uint8_t *c = (uint8_t *)FieldAlloc(0x84);
			uint8_t *s = c + 0x20;
			F<int16_t>(c, 0x82) = (int16_t)(amplitude >> 2);
			F<int16_t>(c, 0x7A) = sx;
			F<int16_t>(c, 0x7C) = sy;
			F<int16_t>(c, 0x80) = (int16_t)phase;
			F<int16_t>(c, 0x78) = radius;
			F<uint32_t>(c, 0x74) = (uint32_t)tables.weights + (uint32_t)shl32(step, 8);
			F<int16_t>(c, 0x7E) = 0;
			ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)c);
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4040, 0x10, FrameCursor());
			ComputeBonesWorldMatrices(entity + 0x60, c);
			F<int16_t>(c, 0x34) = 0;
			F<uint32_t>(c, 0x24) = sway::BoneWorkspace();
			F<int16_t>(c, 0x36) = 0;
			F<int16_t>(c, 0x38) = 0x140;
			F<int16_t>(c, 0x3A) = 0xD8;
			F<uint32_t>(c, 0x3C) = *(const uint32_t *)(entity + 0x28);
			F<uint32_t>(c, 0x40) = *(const uint32_t *)(entity + 0x7C);
			FrameCursor() = RippleModel(*(const uint8_t *const *)(entity + 0x64), s, RenderOT(), 0, FrameCursor(), tables);
			BuildBoneMatricesFromPose(entity + 0x60);
			uint8_t *weapon = *(uint8_t *const *)(entity + 0x78);
			if (weapon)
			{
				F<uint32_t>(c, 0x40) = 0xFFFFFFFF;
				ComputeBonesWorldMatrices(weapon, c);
				FrameCursor() = RippleModel(*(const uint8_t *const *)(weapon + 4), s, RenderOT(), 0, FrameCursor(), tables);
				BuildBoneMatricesFromPose(weapon);
			}
			FieldFree(0x84);
			return F<int16_t>(c, 0x7E);
		}
	}

	// entity +0x00 bit 2 (hidden: an effect draws it) and bit 3 on every entity of its +0x8C ring
	inline void HideEntity(uint8_t *entity)
	{
		entity[0] |= 4;
		uint8_t *e = entity;
		do
		{
			e[0] |= 8;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}
	inline void ShowEntity(uint8_t *entity)
	{
		*(uint16_t *)entity &= 0xFFFB;
		MarkRing(entity, 8, false);
	}

	// one tick e of the silhouette over `ticks` (Flare: 40): shown again at e == ticks; before, while
	// `point` is in front of the camera, the entity hidden and redrawn rippled around it (the ripple's
	// weight row going 0..19..0, its amplitude a half sine, its phase e << 9); *radius: the largest
	// offset of the last draw (the next one's radius)
	inline void RippleSilhouetteTick(uint8_t *entity, const int16_t *point, int32_t e, int32_t ticks, int32_t *radius, const RippleTables &tables)
	{
		if (e == ticks) ShowEntity(entity);
		else if ((uint32_t)e < (uint32_t)ticks)
		{
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			GteLoadV0(point);
			GteRTPS();
			int16_t sxy[2];
			uint32_t otz;
			GteReadSXY2(sxy);
			GteReadOTZ(&otz);
			sxy[0] = (int16_t)(sxy[0] >> 3);
			sxy[1] = (int16_t)(sxy[1] >> 3);
			if ((uint16_t)otz != 0)
			{
				const int32_t half = ticks / 2 ? ticks / 2 : 1;
				uint32_t step = (uint32_t)(e * 20) / (uint32_t)half;
				if ((int32_t)step >= 0x14) step = 0x27 - step;
				const int32_t amplitude = ComputeSin((int32_t)((uint32_t)(e << 11) / (uint32_t)ticks));
				*radius = ripple::DrawSilhouette(entity, sxy[0], sxy[1], (int16_t)*radius, (int32_t)step, amplitude, e << 9, tables);
				HideEntity(entity);
			}
			else ShowEntity(entity);
		}
	}
}
}
