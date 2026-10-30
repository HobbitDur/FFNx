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

// The pieces effect modules are built from: the effect library's node header and calls, the inputs
// a value is measured against, the behaviours a part follows and the ways a part is drawn. Each
// takes plain values (positions, flipbook state, parameters, the packet cursor to draw into), never
// a module's nodes or memory, so one module's code can serve every copy of it (Cure and Potion:
// mag_heal_ring_code.h) and a part can be reused by another effect. Every piece is bit-exact with
// the original code it comes from (named at each), in the same order of random draws and calls.

// The projector overlay (Cure's and Potion's, MAG_001_CURE_DrawTargetModelWithOverlay 0x8D7110 and
// its copies): the target redrawn lit by an image projected from a point, a slide projector. The
// target's shadow, then every object of its body (+0x64) and weapon (+0x78) models: vertices
// through the bones into the overlay vertex buffer, each front-facing polygon drawn twice - flat
// in the entity's colour with its own texture (frame arena, battle_texture_data_ptr 0x1D8E054),
// and, when it faces the projector and falls inside the projected window, gouraud with the
// effect's texture page mapped by a projection from the projector's position (module arena).
// Everything it reads is in its context (OverlayCtx), set up by SetupTargetOverlay.

#pragma once

#include "fx_parts.h"

namespace ff8fx
{
namespace overlay
{
	using namespace parts;

#pragma pack(push, 1)
	// the overlay context (Cure 0x278C7E8, set up by 0x8D8280; the render also reads the word at +0xF4)
	struct OverlayCtx
	{
		uint32_t a00;           // +0x00 (Cure 0x277AEC8: not read by the draw)
		uint8_t *sprite;        // +0x04 the model draw descriptor (Cure 0x278C7A0): +0x04 = the vertex buffer
		uint8_t *entity;        // +0x08 target entity
		uint32_t *frame_cursor; // +0x0C &battle_texture_data_ptr (0x1D8E054)
		uint32_t *packets;      // +0x10 &the module's packet cursor
		Mat4x3 world;           // +0x14 entity matrix
		Mat4x3 view;            // +0x34 camera * world
		Mat4x3 world2;          // +0x54 entity matrix moved by `offset`
		Mat4x3 light;           // +0x74 projector matrix (the entity seen from light_pos)
		uint8_t pad94[0x10];
		int16_t unused_vecA4[3];    // +0xA4 vestigial: cleared, read by no copy of this overlay code
		int16_t padAA;
		int16_t unused_vecAC[3];    // +0xAC (same)
		int16_t padB2;
		int16_t offset[3];      // +0xB4 (y = the ring's height)
		int16_t padBA;
		int16_t unused_vecBC[3];    // +0xBC (same)
		int16_t padC2;
		uint8_t rgb[3];         // +0xC4 projected colour
		uint8_t padC7;
		int32_t light_dist;     // +0xC8
		int16_t wave_phase;     // +0xCC sway: the swaying copies (Bio's) push each vertex by
		int16_t wave_amplitude; // +0xCE amplitude x sin/cos(phase + 12 y) / 0x40000; Cure: 0
		int16_t padD0;
		int16_t mode;           // +0xD2 0: offset in model space, 1: in world space
		uint8_t padD4[4];
		int16_t anim;           // +0xD8
		int16_t slot;           // +0xDA 0xFF: the overlay advances the animation itself
		int16_t light_pos[3];   // +0xDC projector position (the ring)
		int16_t padE2;
		int16_t proj_w;         // +0xE4 projected window size
		int16_t proj_h;         // +0xE6
		int16_t u_off;          // +0xE8 texture offset
		int16_t v_off;          // +0xEA
		uint16_t tpage;         // +0xEC
		uint16_t clut;          // +0xEE
		int16_t padF0;
		int16_t unused_F2;      // +0xF2 cleared, never read
		int16_t cull;           // +0xF4 (never written by Cure / Potion) 1: projector back faces skipped
		int16_t padF6;
	};
#pragma pack(pop)
	static_assert(sizeof(OverlayCtx) == 0xF8, "overlay context");

	// MAG_001_CURE_SetOverlayTexturePage (0x8D7050): projector position, the texture page / CLUT of
	// the projected image (x, y / clut x, y as GetTPage / GetClut), its window size and texel offset
	inline void SetOverlayTexturePage(OverlayCtx *c, const int16_t *pos, int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t w, int32_t h)
	{
		c->light_pos[0] = pos[0];
		c->light_pos[1] = pos[1];
		c->light_pos[2] = pos[2];
		const int32_t yy = (int16_t)y;
		uint32_t t = ((uint32_t)x & 0x3C0) | 0x2800;
		t = (uint32_t)(uint16_t)((int16_t)t >> 6);
		t |= (uint32_t)(yy >> 4) & 0x10;
		t |= ((uint32_t)yy & 0x200) << 2;
		c->tpage = (uint16_t)t;
		c->clut = (uint16_t)((uint32_t)((cx >> 4) & 0x3F) | shl32(cy, 6));
		c->proj_w = (int16_t)w;
		c->u_off = (int16_t)(((uint32_t)x & 0x3F) << 1);
		c->v_off = (int16_t)(y & 0xFF);
		c->proj_h = (int16_t)h;
	}

	// MAG_001_CURE_Overlay_UpdateMatrices (0x8D7200): the entity's matrix, and a copy moved by `offset`
	inline void Overlay_UpdateMatrices(OverlayCtx *c, const uint8_t *entity)
	{
		memcpy(&c->world, entity + 0x40, 0x20);
		memcpy(&c->world2, entity + 0x40, 0x20);
		if (c->mode == 0)
		{
			int16_t out[4];
			MatrixMulVector(&c->world2, c->offset, out);
			c->world2.t[0] = (int32_t)((uint32_t)c->world2.t[0] + (uint32_t)(int32_t)out[0]);
			c->world2.t[1] = (int32_t)((uint32_t)c->world2.t[1] + (uint32_t)(int32_t)out[1]);
			c->world2.t[2] = (int32_t)((uint32_t)c->world2.t[2] + (uint32_t)(int32_t)out[2]);
		}
		else if (c->mode == 1)
		{
			c->world2.t[0] = (int32_t)((uint32_t)c->world2.t[0] + (uint32_t)(int32_t)c->offset[0]);
			c->world2.t[1] = (int32_t)((uint32_t)c->world2.t[1] + (uint32_t)(int32_t)c->offset[1]);
			c->world2.t[2] = (int32_t)((uint32_t)c->world2.t[2] + (uint32_t)(int32_t)c->offset[2]);
		}
	}

	// MAG_001_CURE_Overlay_BuildProjectorMatrix (0x8D72C0): `light` = the entity's rotation seen from
	// light_pos (looking at the moved entity origin, at light_dist). Work block (Field_Alloc 0x70):
	// CureProjectorScratch in IDA.
	inline void Overlay_BuildProjectorMatrix(OverlayCtx *c)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x70);
		const int16_t x = (int16_t)c->world2.t[0];
		const int16_t y = (int16_t)c->world2.t[1];
		const int16_t z = (int16_t)c->world2.t[2];
		F<int16_t>(s, 0x68) = x;
		const int32_t dx = (int32_t)x - c->light_pos[0];
		F<int16_t>(s, 0x6A) = y;
		const int32_t dy = (int32_t)y - c->light_pos[1];
		F<int16_t>(s, 0x6C) = z;
		const int32_t dz = (int32_t)z - c->light_pos[2];
		F<int32_t>(s, 4) = dy;
		F<int32_t>(s, 8) = dz;
		F<int32_t>(s, 0) = dx;
		const int32_t d2 = (int32_t)((uint32_t)mul32(dz, dz) + (uint32_t)mul32(dy, dy) + (uint32_t)mul32(dx, dx));
		F<int32_t>(s, 0x60) = d2;
		F<int32_t>(s, 0x64) = Sqrt(d2);
		F<int16_t>(s, 0x12) = 0x1000;
		F<int16_t>(s, 0x10) = 0;
		F<int16_t>(s, 0x14) = 0;
		NormalizeVec3((const int32_t *)s, (int16_t *)(s + 0x18));
		BuildMatrixFromDirAndUp(s + 0x20, s + 0x18, s + 0x10);
		UnpackRotationMatrix(s + 0x20, s + 0x40);
		F<int32_t>(s, 0x5C) = c->light_dist;
		memcpy(s + 0x20, &c->world, 0x20);
		F<int32_t>(s, 0x54) = 0;
		F<int32_t>(s, 0x58) = 0;
		F<int32_t>(s, 0x38) = (int32_t)((uint32_t)c->world.t[1] - (uint32_t)c->world2.t[1]);
		F<int32_t>(s, 0x34) = (int32_t)((uint32_t)c->world.t[0] - (uint32_t)c->world2.t[0]);
		F<int32_t>(s, 0x3C) = (int32_t)((uint32_t)c->world.t[2] - (uint32_t)c->world2.t[2]);
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x40));
		uint8_t *light = (uint8_t *)&c->light;
		for (int k = 0; k < 3; k++)
		{
			GteLoadIRFromMatrixColumn(s + 0x20 + 2 * k);
			GteMVMVA_RotIR();
			GteStoreIRToMatrixColumn(light + 2 * k);
		}
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x40));
		GteLoadV0Dwords(s + 0x34);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(c->light.t);
		FieldFree(0x70);
	}

	// the projected vertices inside the window (x0..x1, y0..y1) move by the texel offset; false = out
	inline bool ProjectedUV(uint8_t *s, int count)
	{
		for (int v = 0; v < count; v++)
		{
			const int16_t x = F<int16_t>(s, 0x40 + 4 * v);
			if (x < F<int16_t>(s, 0xC0) || x > F<int16_t>(s, 0xC2)) return false;
			const int16_t y = F<int16_t>(s, 0x42 + 4 * v);
			if (y < F<int16_t>(s, 0xC4) || y > F<int16_t>(s, 0xC6)) return false;
			F<int16_t>(s, 0x40 + 4 * v) = (int16_t)(F<int16_t>(s, 0xC8) + x);
			F<int16_t>(s, 0x42 + 4 * v) = (int16_t)(F<int16_t>(s, 0x42 + 4 * v) + F<int16_t>(s, 0xCA));
		}
		return true;
	}

	// the three / four vertices again through the projector matrix: skipped (+0xCC = 1) when the
	// back-face test is on and they face away from it
	inline void ProjectorPass(uint8_t *s, const uint8_t *verts)
	{
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x20));
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x20));
		GteLoadV012(verts + F<uint32_t>(s, 0x94) * 16 + 4, verts + F<uint32_t>(s, 0x98) * 16 + 4, verts + F<uint32_t>(s, 0x9C) * 16 + 4);
		GteRTPT();
		F<int16_t>(s, 0xCC) = 0;
		GteNCLIP();
		GteReadMAC0(s + 0x8C);
		if (F<int16_t>(s, 0xCE) == 1 && F<int32_t>(s, 0x8C) <= 0) F<int16_t>(s, 0xCC) = 1;
	}

	// MAG_001_CURE_Overlay_RenderModel (0x8D7430): every object of `model` the entity's mask shows.
	// Packets: model pass GpuPolyFT3 / GpuPolyFT4, overlay pass GpuPolyGT3 / GpuPolyGT4; geometry records
	// BattleModelTriangle / BattleModelQuad; overlay vertex buffer entries CureOverlayVertex (IDA types).
	// Work block (Field_Alloc 0xD0, CureOverlayRenderScratch in IDA): +0x00 view matrix, +0x20 projector matrix, +0x40 projected
	// xy (4), +0x70 vertex, +0x8C NCLIP, +0x90 OTZ, +0x94..+0xA0 vertex indices, +0xA8 / +0xAC
	// projected colour codes (GT3 / GT4), +0xB0 / +0xB4 entity colour codes (FT3 / FT4), +0xB8
	// object mask, +0xBC tpage, +0xBE clut, +0xC0..+0xC6 window, +0xC8 / +0xCA texel offset,
	// +0xCC skip, +0xCE back-face test.
	// skip_back_faces: the projector cull switch on whatever the context says (vanilla: never)
	inline void Overlay_RenderModel(const uint8_t *model, uint32_t ot, int32_t mode, OverlayCtx *c, bool skip_back_faces)
	{
		(void)mode;
		uint32_t fcur = *c->frame_cursor;
		uint32_t pcur = *c->packets;
		uint8_t *verts = *(uint8_t **)(c->sprite + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const uint8_t *table = *(const uint8_t *const *)(model + 4);
		const int32_t count = *(const int32_t *)table;
		const uint8_t *entity = c->entity;
		uint8_t *s = (uint8_t *)FieldAlloc(0xD0);
		// the projector cull switch: vanilla never writes c->cull (0, both sides lit)
		F<int16_t>(s, 0xCE) = skip_back_faces ? 1 : c->cull;
		F<uint32_t>(s, 0xB8) = *(const uint32_t *)(entity + 0x7C);
		F<uint16_t>(s, 0xBC) = c->tpage;
		F<uint16_t>(s, 0xBE) = c->clut;
		const int32_t hw = (int32_t)c->proj_w / 2;
		const int32_t hh = (int32_t)c->proj_h / 2;
		F<int16_t>(s, 0xC2) = (int16_t)(hw + 0x9F);
		F<int16_t>(s, 0xC0) = (int16_t)(0xA0 - hw);
		F<int16_t>(s, 0xC6) = (int16_t)(hh + 0x77);
		F<int16_t>(s, 0xC4) = (int16_t)(0x78 - hh);
		F<int16_t>(s, 0xCA) = (int16_t)(c->v_off + (int16_t)hh - 0x78);
		F<int16_t>(s, 0xC8) = (int16_t)(c->u_off + (int16_t)hw - 0xA0);
		const uint32_t rgb = *(const uint32_t *)c->rgb & 0xFFFFFF;
		F<uint32_t>(s, 0xAC) = rgb | 0x3E000000;
		F<uint32_t>(s, 0xA8) = rgb | 0x36000000;
		const uint32_t col = *(const uint32_t *)(entity + 0x28) & 0xFFFFFF;
		F<uint32_t>(s, 0xB0) = col | 0x24000000;
		F<uint32_t>(s, 0xB4) = col | 0x2C000000;
		memcpy(s, &c->view, 0x20);
		memcpy(s + 0x20, &c->light, 0x20);
		F<int16_t>(s, 0xA4) = c->wave_amplitude;
		F<int16_t>(s, 0xA6) = c->wave_phase;
		GteSetRotMatrixCtrl((const Mat4x3 *)s);
		GteSetTransVectorCtrl((const Mat4x3 *)s);
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t k = 0; k < count; k++)
		{
			uint8_t *vcur = verts;
			const uint8_t *p = table + offs[k];
			if (!((F<uint32_t>(s, 0xB8) >> (k & 31)) & 1)) continue;
			// vertices, grouped by bone: L = bone matrix, BK = its translation
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t bone = *(const int16_t *)p;
				p += 2;
				const uint8_t *bm = bones + (uint32_t)mul32(bone, 0x30) + 0x10;
				GteSetLightMatrix(bm);
				GteSetBackground(F<int32_t>(bm, 0x14), F<int32_t>(bm, 0x18), F<int32_t>(bm, 0x1C));
				const int32_t nv = *(const int16_t *)p;
				p += 2;
				if (nv <= 0) continue;
				uint8_t *out = vcur + 4;
				vcur += (uint32_t)nv * 16;
				for (int32_t v = nv; v > 0; v--)
				{
					F<int16_t>(s, 0x70) = *(const int16_t *)p;
					F<int16_t>(s, 0x72) = *(const int16_t *)(p + 2);
					F<int16_t>(s, 0x74) = *(const int16_t *)(p + 4);
					p += 6;
					GteLoadV0(s + 0x70);
					GteMVMVA_LightV0Bk();
					GteStoreIR123(out);
					out += 0x10;
				}
			}
			// polygons: 12-byte header (triangle count, quad count), 16-byte triangles, 20-byte quads
			p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
			const int32_t tris = *(const int16_t *)p;
			const int32_t quads = *(const int16_t *)(p + 2);
			const uint8_t *poly = p + 0xC;
			uint8_t *gp = (uint8_t *)pcur;
			uint8_t *fp = (uint8_t *)fcur;
			for (int32_t t = tris; t > 0; t--, poly += 0x10)
			{
				const uint32_t i2 = *(const uint16_t *)(poly + 4) & 0xFFF;
				const uint32_t i1 = *(const uint16_t *)(poly + 2) & 0xFFF;
				const uint32_t i0 = *(const uint16_t *)poly & 0xFFF;
				F<uint32_t>(s, 0x9C) = i2;
				F<uint32_t>(s, 0x98) = i1;
				F<uint32_t>(s, 0x94) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				const uint32_t z1 = GteSZ(1), z2 = GteSZ(2), z3 = GteSZ(3);
				GteNCLIP();
				GteReadMAC0(s + 0x8C);
				if (F<int32_t>(s, 0x8C) <= 0) continue;
				GteAVSZ3();
				GteReadOTZWord(s + 0x90);
				F<uint32_t>(s, 0x90) >>= 2;
				GteStoreSXY012_GT3(gp);
				GteStoreSXY012_FT3(fp);
				ProjectorPass(s, verts);
				if (F<int16_t>(s, 0xCC) == 0)
				{
					GteReadSXY012(s + 0x40);
					if (!ProjectedUV(s, 3)) F<int16_t>(s, 0xCC) = 1;
					if (F<int16_t>(s, 0xCC) == 0)
					{
						// the triangle in the projected light: gouraud, Cure texture page
						gp[0xC] = s[0x40];
						gp[0xD] = s[0x42];
						gp[0x18] = s[0x44];
						gp[0x19] = s[0x46];
						gp[0x24] = s[0x48];
						gp[0x25] = s[0x4A];
						F<uint16_t>(gp, 0x1A) = F<uint16_t>(s, 0xBC);
						F<uint16_t>(gp, 0xE) = F<uint16_t>(s, 0xBE);
						F<uint32_t>(gp, 0x1C) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 0x10) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 4) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 0) = 0x9000000;
						InsertPrimAutoDepth(ot + F<uint32_t>(s, 0x90) * 4, gp);
						gp += 0x28;
					}
				}
				// the triangle itself: flat in the entity's colour, its own texture
				F<uint32_t>(fp, 0xC) = *(const uint32_t *)(poly + 8);
				F<uint32_t>(fp, 0x14) = *(const uint32_t *)(poly + 0xC);
				F<uint16_t>(fp, 0x1C) = *(const uint16_t *)(poly + 6);
				F<uint32_t>(fp, 4) = F<uint32_t>(s, 0xB0);
				F<uint32_t>(fp, 0) = 0x7000000;
				if (poly[0xF] & 2) fp[7] |= 2;
				InsertPrimDepthKeys(ot + F<uint32_t>(s, 0x90) * 4, fp, z1, z2, z3, 0);
				fp += 0x20;
				GteSetRotMatrixCtrl((const Mat4x3 *)s);
				GteSetTransVectorCtrl((const Mat4x3 *)s);
			}
			for (int32_t q = quads; q > 0; q--, poly += 0x14)
			{
				const uint32_t i2 = *(const uint16_t *)(poly + 4) & 0xFFF;
				const uint32_t i1 = *(const uint16_t *)(poly + 2) & 0xFFF;
				const uint32_t i0 = *(const uint16_t *)poly & 0xFFF;
				F<uint32_t>(s, 0x9C) = i2;
				F<uint32_t>(s, 0x98) = i1;
				F<uint32_t>(s, 0x94) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				const uint32_t z1 = GteSZ(1), z2 = GteSZ(2), z3 = GteSZ(3);
				GteNCLIP();
				GteReadMAC0(s + 0x8C);
				if (F<int32_t>(s, 0x8C) <= 0) continue;
				GteStoreSXY012_GT4(gp);
				GteStoreSXY012_FT4(fp);
				const uint32_t i3 = *(const uint16_t *)(poly + 6) & 0xFFF;
				F<uint32_t>(s, 0xA0) = i3;
				GteLoadV0(verts + i3 * 16 + 4);
				GteRTPS();
				const uint32_t z4 = GteSZ(3);
				GteReadSXY2(gp + 0x2C);
				GteReadSXY2(fp + 0x20);
				GteAVSZ4();
				GteReadOTZWord(s + 0x90);
				F<uint32_t>(s, 0x90) >>= 2;
				ProjectorPass(s, verts);
				if (F<int16_t>(s, 0xCC) == 0)
				{
					GteReadSXY012(s + 0x40);
					GteLoadV0(verts + F<uint32_t>(s, 0xA0) * 16 + 4);
					GteRTPS();
					GteReadSXY2(s + 0x4C);
					if (!ProjectedUV(s, 4)) F<int16_t>(s, 0xCC) = 1;
					if (F<int16_t>(s, 0xCC) == 0)
					{
						gp[0xD] = s[0x42];
						gp[0xC] = s[0x40];
						gp[0x18] = s[0x44];
						gp[0x24] = s[0x48];
						gp[0x19] = s[0x46];
						gp[0x25] = s[0x4A];
						gp[0x31] = s[0x4E];
						gp[0x30] = s[0x4C];
						F<uint16_t>(gp, 0x1A) = F<uint16_t>(s, 0xBC);
						F<uint32_t>(gp, 0x28) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 0x1C) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 0x10) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 4) = F<uint32_t>(s, 0xAC);
						F<uint16_t>(gp, 0xE) = F<uint16_t>(s, 0xBE);
						F<uint32_t>(gp, 0) = 0xC000000;
						InsertPrimAutoDepth(ot + F<uint32_t>(s, 0x90) * 4, gp);
						gp += 0x34;
					}
				}
				F<uint32_t>(fp, 0xC) = *(const uint32_t *)(poly + 8);
				F<uint32_t>(fp, 0x14) = *(const uint32_t *)(poly + 0xC);
				F<uint16_t>(fp, 0x1C) = *(const uint16_t *)(poly + 0x10);
				F<uint16_t>(fp, 0x24) = *(const uint16_t *)(poly + 0x12);
				F<uint32_t>(fp, 4) = F<uint32_t>(s, 0xB4);
				F<uint32_t>(fp, 0) = 0x9000000;
				if (poly[0xF] & 2) fp[7] |= 2;
				InsertPrimDepthKeys(ot + F<uint32_t>(s, 0x90) * 4, fp, z1, z2, z3, z4);
				fp += 0x28;
				GteSetRotMatrixCtrl((const Mat4x3 *)s);
				GteSetTransVectorCtrl((const Mat4x3 *)s);
			}
			pcur = (uint32_t)gp;
			fcur = (uint32_t)fp;
		}
		*c->frame_cursor = fcur;
		*c->packets = pcur;
		FieldFree(0xD0);
	}

	// MAG_001_CURE_DrawTargetModelWithOverlay (0x8D7110): shadow, body (+0x64) and weapon (+0x78)
	// of the target with the projected light
	inline void DrawTargetModelWithOverlay(OverlayCtx *c, bool skip_back_faces)
	{
		uint8_t *entity = c->entity;
		Overlay_UpdateMatrices(c, entity);
		if (c->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
		else
		{
			const int32_t more = BattleReadAnimation(entity + 0x60, entity + 0x6C);
			// the animation number goes in the low word of the returned register
			if (more) PreBattleReadAnimation(entity + 0x60, entity + 0x6C, ((uint32_t)more & 0xFFFF0000u) | (uint16_t)c->anim);
		}
		ComposeAffineTransform(&Camera(), &c->world, &c->view);
		if (!(entity[0] & 0x20)) *c->frame_cursor = BattleEntity_DrawShadow(entity, RenderOTAt(0x4040), 0x10, *c->frame_cursor);
		Overlay_BuildProjectorMatrix(c);
		Overlay_RenderModel(*(const uint8_t *const *)(entity + 0x64), RenderOTAt(0x44), 4, c, skip_back_faces);
		const uint8_t *weapon = *(const uint8_t *const *)(entity + 0x78);
		if (weapon) Overlay_RenderModel(*(const uint8_t *const *)(weapon + 4), RenderOTAt(0x44), 4, c, skip_back_faces);
		if (c->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
	}

	// MAG_001_CURE_SetupTargetOverlay (0x8D8280): `packets` the module's packet cursor, `rgb` the
	// projected colour (128 = the image as it is), `light_dist` the projector distance
	inline void SetupTargetOverlay(OverlayCtx *c, uint32_t a00, uint8_t *sprite, uint32_t verts, uint8_t *entity, int32_t slot,
	                               uint32_t *packets, const uint8_t *rgb, int32_t light_dist)
	{
		c->entity = entity;
		c->a00 = a00;
		c->slot = (int16_t)slot;
		c->sprite = sprite;
		c->frame_cursor = (uint32_t *)0x1D8E054;
		c->packets = packets;
		c->offset[0] = 0;
		c->offset[1] = 0;
		c->offset[2] = 0;
		c->rgb[0] = rgb[0];
		c->rgb[1] = rgb[1];
		c->rgb[2] = rgb[2];
		c->light_dist = light_dist;
		c->mode = 1;
		c->wave_amplitude = 0;
		c->unused_F2 = 0;
		c->unused_vecA4[0] = 0;
		c->unused_vecA4[1] = 0;
		c->unused_vecA4[2] = 0;
		c->unused_vecAC[0] = 0;
		c->unused_vecAC[1] = 0;
		c->unused_vecAC[2] = 0;
		c->unused_vecBC[0] = 0;
		c->unused_vecBC[1] = 0;
		c->unused_vecBC[2] = 0;
		sprite[0x1C] = 0x80;
		sprite[0x1D] = 0x80;
		sprite[0x1E] = 0x80;
		F<uint32_t>(sprite, 4) = verts;
		// the effect library's model draw descriptor (Effect_FlatPolygons 0x6BCB50): its clip
		// rectangle (left, top, right, bottom: unused by Cure's own renderer), every object visible,
		// the vertex scratch cleared
		F<uint16_t>(sprite, 0x14) = 0;           // clip_left
		F<uint16_t>(sprite, 0x16) = 0;           // clip_top
		F<uint16_t>(sprite, 0x18) = 0x140;       // clip_right
		F<uint16_t>(sprite, 0x1A) = 0;           // clip_bottom
		F<uint32_t>(sprite, 0x20) = 0xFFFFFFFF;  // visible_parts_mask
		F<uint16_t>(sprite, 0x24) = 0;           // vertex_sxy_scratch
	}
}
}
