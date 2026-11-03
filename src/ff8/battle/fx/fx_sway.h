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

// Parts library, the sway overlay (Zombie 0x6CA560): an entity's model redrawn lifted, its
// vertices swayed sideways by a wave table, as flat additive polygons in one colour going from
// black to the entity's tint. Bit-exact with Zombie's code (mag114_zombie.cpp before it was built
// from here), each piece named with its original.
//
// Work block (Field_Alloc 0x8C): +0x00 camera x entity matrix, then S = +0x20:
//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 / +0x0A triangle / quad counts,
//   +0x14 clip rectangle (x0, y0, x1, y1 / 8), +0x1C colour, +0x20 object mask,
//   +0x24..+0x43 four vertices (sx, sy, otz, clip flags), +0x44 NCLIP, +0x4C OTZ,
//   +0x5C entity origin on screen, +0x64 wave phase, +0x66 sway amplitude; +0x68 origin OTZ.

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
	namespace sway
	{
		using parts::F;
		using parts::GteNCLIP;
		using parts::GteReadMAC0;
		using parts::GteAVSZ3;
		using parts::GteAVSZ4;
		using parts::GteReadOTZWord;
		inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
		inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
		inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
		inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
		// sub_56CC00: out = colour blend of a (weight wa) and b (weight wb), weights 4.12
		inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
		inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // the overlay's vertex buffer
		// Zombie's module words never written (0x2521860: the zero vector, 0x2521868: black)
		static const int16_t ZERO_VECTOR[4] = {};
		static const uint8_t BLACK[4] = {};

		// sub_6CAAE0: one bone group of vertices, projected, swayed by the wave table, clip-flagged
		static void SwayVertices(uint8_t **objp, uint8_t **outp, uint8_t *s, const int16_t *wave)
		{
			uint8_t *p = *objp;
			uint8_t *out = *outp;
			const int32_t n = *(const int16_t *)p;
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
				GteReadSXY2(s + 0x24);
				p += 6;
				GteReadOTZ(s + 0x28);
				if (F<uint16_t>(s, 0x28) == 0)
					*(uint16_t *)(out + 6) = 0x10;
				else
				{
					GteStoreIR123(s + 0x2C);
					const uint32_t idx = ((uint32_t)F<uint16_t>(s, 0x64) + (uint32_t)F<uint16_t>(s, 0x2E) * 4) & 0xFFF;
					const int16_t sx = F<int16_t>(s, 0x24);
					int32_t d = mul32(wave[idx], F<int16_t>(s, 0x66)) >> 12;
					d = mul32(d, (int32_t)sx - (int32_t)F<int16_t>(s, 0x5C)) >> 11;
					const int16_t nx = (int16_t)(sx + d);
					F<int16_t>(s, 0x24) = nx;
					if ((int32_t)nx < shl32(F<int16_t>(s, 0x14), 3)) s[0x2A] |= 1;
					else if ((int32_t)nx >= shl32(F<int16_t>(s, 0x18), 3)) s[0x2A] |= 2;
					const int32_t sy = F<int16_t>(s, 0x26);
					if (sy < shl32(F<int16_t>(s, 0x16), 3)) s[0x2A] |= 4;
					else if (sy >= shl32(F<int16_t>(s, 0x1A), 3)) s[0x2A] |= 8;
					*(uint32_t *)out = F<uint32_t>(s, 0x24);
					*(uint32_t *)(out + 4) = F<uint32_t>(s, 0x28);
				}
				out += 8;
			} while (--left != 0);
			*objp = p;
			*outp = out;
		}

		static inline void CopyVertex(uint8_t *s, uint32_t off, const uint8_t *verts, uint16_t index)
		{
			const uint8_t *v = verts + (uint32_t)(index & 0xFFF) * 8;
			F<uint32_t>(s, off) = *(const uint32_t *)v;
			F<uint32_t>(s, off + 4) = *(const uint32_t *)(v + 4);
		}

		// sub_6CA800: the object's textured triangles and quads, flat additive in the colour
		static uint32_t FlatPolygons(uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
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
					const uint8_t f0 = s[0x2A], f2 = s[0x3A];
					if ((uint8_t)(f0 | s[0x32] | f2) < 0x10 && !(f2 & (uint8_t)(f0 & s[0x32])))
					{
						GteSetSXY012(F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x34));
						GteNCLIP();
						GteReadMAC0(s + 0x44);
						if (F<int32_t>(s, 0x44) >= 0)
						{
							GteSetSZ123(F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x38));
							GteAVSZ3();
							F<uint32_t>(pk, 0) = 0x7000000;
							F<uint32_t>(pk, 4) = F<uint32_t>(s, 0x1C) | 0x26000000;
							F<uint32_t>(pk, 8) = F<uint32_t>(s, 0x24);
							F<uint32_t>(pk, 0x10) = F<uint32_t>(s, 0x2C);
							F<uint32_t>(pk, 0x18) = F<uint32_t>(s, 0x34);
							F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
							F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
							F<uint16_t>(pk, 0x16) &= 0xFDFF;
							F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 6);
							GteReadOTZWord(s + 0x4C);
							InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(s, 0x4C) >> shift) * 4, pk);
							pk += 0x20;
						}
					}
					p += 0x10;
					k = F<uint16_t>(s, 8);
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
					const uint8_t f3 = s[0x42], f2 = s[0x3A];
					if ((uint8_t)(f3 | s[0x2A] | s[0x32] | f2) < 0x10 && !(f2 & (uint8_t)(f3 & s[0x2A] & s[0x32])))
					{
						GteSetSXY012(F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x34));
						GteNCLIP();
						GteReadMAC0(s + 0x44);
						if (F<int32_t>(s, 0x44) >= 0)
						{
							GteSetSZ0123(F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x38), F<uint16_t>(s, 0x40));
							GteAVSZ4();
							F<uint32_t>(pk, 0) = 0x9000000;
							F<uint32_t>(pk, 4) = F<uint32_t>(s, 0x1C) | 0x2E000000;
							F<uint32_t>(pk, 8) = F<uint32_t>(s, 0x24);
							F<uint32_t>(pk, 0x10) = F<uint32_t>(s, 0x2C);
							F<uint32_t>(pk, 0x18) = F<uint32_t>(s, 0x34);
							F<uint32_t>(pk, 0x20) = F<uint32_t>(s, 0x3C);
							F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 4);
							F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 8);
							F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 0xC);
							F<uint16_t>(pk, 0x16) &= 0xFDFF;
							F<uint16_t>(pk, 0x24) = *(const uint16_t *)(p + 0xE);
							GteReadOTZWord(s + 0x4C);
							InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(s, 0x4C) >> shift) * 4, pk);
							pk += 0x28;
						}
					}
					p += 0x14;
					k = F<uint16_t>(s, 0xA);
					F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
				} while (k != 0);
			}
			return (uint32_t)pk;
		}

		// MAG_114_sub_6CA6E0: every object of a model the mask shows (bone groups of vertices, then
		// polygons)
		static uint32_t SwayModel(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor, const int16_t *wave)
		{
			const uint8_t *table = *(const uint8_t *const *)(model + 4);
			const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
			const int32_t count = *(const int32_t *)table;
			const uint8_t *offs = table + 4;
			if (count <= 0) return cursor;
			for (int32_t k = 0; k < count; k++)
			{
				uint8_t *obj = (uint8_t *)(*(const uint8_t *const *)(model + 4) + *(const uint32_t *)offs);
				offs += 4;
				if (!(F<uint32_t>(s, 0x20) & (1u << (k & 31)))) continue;
				uint8_t *out = F<uint8_t *>(s, 4);
				const int32_t groups = *(const int16_t *)obj;
				obj += 2;
				for (int32_t g = groups; g > 0; g--)
				{
					const int32_t bone = *(const int16_t *)obj;
					obj += 2;
					const Mat4x3 *mtx = (const Mat4x3 *)(bones + (uint32_t)mul32(bone, 0x30) + 0x10);
					GteSetRotMatrixCtrl(mtx);
					GteSetTransVectorCtrl(mtx);
					SwayVertices(&obj, &out, s, wave);
				}
				obj = (uint8_t *)(((uint32_t)obj + 3) & ~3u);
				F<uint16_t>(s, 8) = *(const uint16_t *)obj;
				obj += 2;
				F<uint16_t>(s, 0xA) = *(const uint16_t *)obj;
				obj += 0xA;
				F<uint8_t *>(s, 0) = obj;
				cursor = FlatPolygons(s, ot, shift, cursor);
			}
			return cursor;
		}
	}

	// the overlay's inputs (the arguments of 0x6CA560 after the entity)
	struct SwayArg
	{
		int16_t lift[3];   // offset added to the entity matrix translation (0, -sin * reach, 0)
		int32_t weight;    // sin: the entity tint's weight in the colour (4.12)
		int32_t amp;       // sway amplitude (* 4)
		int32_t phase;     // wave table phase
	};

	// the overlay at step e of `ticks` for an entity of reach `reach` (Zombie: 30 ticks, the size
	// scale): t = e * 4096 / ticks, sin(t / 4) lifts it (>> lift_shift) and weighs its tint; the sway
	// grows by amp_step a t from amp_base; the wave's phase moves phase_step a step
	inline void SwayOverlayParams(int32_t e, int32_t ticks, int32_t reach, int32_t lift_shift, int32_t amp_base, int32_t amp_step,
	                              int32_t phase_step, SwayArg *a)
	{
		const uint32_t t = ((uint32_t)e << 12) / (uint32_t)(ticks ? ticks : 1);
		const int32_t sinv = ComputeSin((int32_t)t >> 2);
		a->lift[0] = 0;
		a->lift[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(sinv, reach)) >> lift_shift);
		a->lift[2] = 0;
		a->weight = sinv;
		a->amp = (int32_t)(t * (uint32_t)amp_step + (uint32_t)amp_base);
		a->phase = e * phase_step;
	}

	// MAG_114_sub_6CA560: the overlay of `entity` (its body +0x64, then its second model +0x78 if
	// any), swayed by the s16 [4096] `wave` table, drawn at `cursor`
	inline void DrawSwayOverlay(uint8_t *entity, const SwayArg *a, const int16_t *wave, uint32_t &cursor)
	{
		using parts::F;
		uint8_t *h = (uint8_t *)FieldAlloc(0x8C);
		F<int16_t>(h, 0x86) = (int16_t)(a->amp >> 2);
		F<int16_t>(h, 0x84) = (int16_t)a->phase;
		// MAG_103_sub_6D67B0 (in the Blizzaga module, shared): copy of the entity's matrix (8 dwords)
		for (int i = 0; i < 8; i++) F<uint32_t>(h, i * 4) = *(const uint32_t *)(entity + 0x40 + i * 4);
		Mat4x3 *m = (Mat4x3 *)h;
		m->t[0] = (int32_t)((uint32_t)m->t[0] + (uint32_t)(int32_t)a->lift[0]);
		m->t[1] = (int32_t)((uint32_t)m->t[1] + (uint32_t)(int32_t)a->lift[1]);
		m->t[2] = (int32_t)((uint32_t)m->t[2] + (uint32_t)(int32_t)a->lift[2]);
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		GteLoadV0(sway::ZERO_VECTOR);
		GteRTPS();
		GteReadSXY2(h + 0x7C);
		GteReadOTZ(h + 0x88);
		if (F<int32_t>(h, 0x88) > 0)
		{
			ComputeBonesWorldMatrices(entity + 0x60, m);
			F<int16_t>(h, 0x34) = 0;
			F<int16_t>(h, 0x36) = 0;
			F<uint32_t>(h, 0x24) = sway::BoneWorkspace();
			F<int16_t>(h, 0x38) = 0x140;
			F<int16_t>(h, 0x3A) = 0xD8;
			sway::BlendColour(sway::BLACK, entity + 0x28, a->weight, 0x1000 - a->weight, h + 0x3C);
			h[0x3F] = 0;
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(entity + 0x7C);
			cursor = sway::SwayModel(*(const uint8_t *const *)(entity + 0x64), h + 0x20, RenderOT(), 0, cursor, wave);
			BuildBoneMatricesFromPose(entity + 0x60);
			uint8_t *second = *(uint8_t *const *)(entity + 0x78);
			if (second)
			{
				F<uint32_t>(h, 0x40) = 0xFFFFFFFF;
				ComputeBonesWorldMatrices(second, m);
				cursor = sway::SwayModel(*(const uint8_t *const *)(second + 4), h + 0x20, RenderOT(), 0, cursor, wave);
				BuildBoneMatricesFromPose(second);
			}
		}
		FieldFree(0x8C);
	}

	// entity +0x00 `bits` on (off) every entity of its +0x8C ring (Zombie: bit 3)
	inline void MarkRing(uint8_t *entity, uint16_t bits, bool on)
	{
		uint8_t *e = entity;
		do
		{
			if (on) *(uint16_t *)e |= bits;
			else *(uint16_t *)e &= (uint16_t)~bits;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}
}
}
