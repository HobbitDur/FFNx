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

// Parts library (fx_parts.h), prim-model layouts: an effect part drawn as a layout of the effect
// library's prim player (prim::play, 0x701970), every object through the callback the modules each
// carry a copy of (Hypnotize 0x6B1520, Heartbreak 0x872700...), in a frame: on the caster's screen
// point in view space, at a point of the caster's bones, or at a position with the camera's
// rotation scaled; or at an anchor, each object's offset added to it (the status-cure items'
// callback, AnchorPrimCallback, with the record flags each build tests). Also the inputs these
// parts are placed with (a random target of an action, a random bone of it, a share of an entity's
// height) and the camera-facing sequence sprite. Everything takes plain values (the packet cursor
// to draw into, the caster, the morph buffer), never a module's memory. Bit-exact with Hypnotize's
// code (mag133_hypnotize.cpp) and Antidote's (mag294_antidote.cpp), each piece named with its
// original.

#pragma once

#include "fx_parts.h"

namespace ff8fx
{
namespace layouts
{
	using namespace eng;
	using namespace magc;

#pragma pack(push, 1)
	// the callback's parameter block (a stack block of the drawing task in the original)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
		uint32_t *cursor;  // the packet cursor the objects are drawn at (the module's global in the original)
	};
#pragma pack(pop)

	inline int16_t ProjectionH() { return var<int16_t>(0x1D8E038); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline uint32_t MagicBuffer() { return fn<uint32_t (__cdecl *)()>(0x571B50)(); }   // Magic_TextureOFF_ToEAX1

	// ------------------------------------------------------------------
	// The prim-player callback (Hypnotize 0x6B1520): one object of a layout. Vertex frame (or blend
	// into the parameter block's morph buffer), rotation from the record's angles, the record's
	// offset through the parameter matrix (flag 0x200: as is), the matrix composed with it, scale
	// unless 1.0, fade, drawn by 0x7043B0.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
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
		*(int32_t *)(h + 0x18) = arg->depth;
		*arg->cursor = RenderPrim2(h, RenderOT(), 2, *arg->cursor);
		FieldFree(0x58);
	}

	// the layout played one tick, every object drawn in `arg`'s frame: the frames left (0 = finished)
	inline int Play(void *layout, PrimArg *arg)
	{
		return prim::play((prim::Layout *)layout, PrimCallback, (int)arg, 0);
	}

	// ------------------------------------------------------------------
	// Additive quads (Blind 0x6BE4E0): a prim model drawn as its textured quads only (its flat list
	// 4, sub_6AE420 of the Oil Shot module, and its gouraud list 8, 0x6BE5C0), each quad after an
	// additive draw-mode primitive in the same bucket; the header as the engine renderer's (model,
	// vertices, fade colour +8, fade +0xC; +0x20 the list cursor, +0x2C the depth, +0x30 the flag)
	// ------------------------------------------------------------------
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012To(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                   // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	namespace quads
	{
		using parts::F; using parts::GteLoadV012; using parts::GteRTPT; using parts::GteAVSZ4; using parts::GteReadOTZWord;
		// screen clip of a projected vertex (x 0..0xA00, y 0..0x6C0: the PC's 4x subpixel screen)
		static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
		static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

		// the quads of the list at header +0x20 (flat: 0x18-byte records, 0x28-byte packets; gouraud:
		// 0x24-byte records, 0x34-byte packets), each projected, clipped, faded, bucketed by its depth >> shift
		// (depth: Zombie's, the OTZ + the header's +0x18 depth, clamped at 0)
		static uint32_t Quads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor, bool gouraud, bool depth = false)
		{
			const uint32_t size = gouraud ? 0x24 : 0x18;   // a record
			const uint32_t v1 = gouraud ? 0x14 : 0x10, v2 = gouraud ? 0x20 : 0x18, v3 = gouraud ? 0x2C : 0x20;   // its packet's vertices
			uint8_t *pk = (uint8_t *)cursor;
			const uint8_t *list = F<uint8_t *>(h, 0x20);
			const int32_t count = *(const int32_t *)list;
			const uint8_t *rec = list + 4;
			const uint8_t *verts = F<uint8_t *>(h, 4);
			F<const uint8_t *>(h, 0x20) = rec;
			if (count <= 0) return (uint32_t)pk;
			for (int32_t k = count; k != 0; k--, rec += size)
			{
				GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
				GteRTPT();
				F<uint32_t>(pk, 0) = gouraud ? 0xC000000 : 0x9000000;
				F<uint32_t>(pk, 4) = *(const uint32_t *)rec;
				F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
				F<uint32_t>(pk, v1 + 4) = *(const uint32_t *)(rec + 0x10);
				const uint32_t uv = *(const uint32_t *)(rec + 0x14);
				F<uint32_t>(pk, v2 + 4) = uv;
				F<uint32_t>(pk, v3 + 4) = uv >> 16;
				GteReadFLAG(h + 0x30);
				if (F<uint32_t>(h, 0x30) & 0x60000) continue;
				GteReadSXY012To(pk + 8, pk + v1, pk + v2);
				uint32_t clip = 0;
				GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
				GteRTPS();
				if (OutX(pk + 8)) clip = 1;
				if (OutX(pk + v1)) clip |= 2;
				if (OutX(pk + v2)) clip |= 4;
				if (OutY(pk + 8)) clip |= 0x10;
				if (OutY(pk + v1)) clip |= 0x20;
				if (OutY(pk + v2)) clip |= 0x40;
				GteReadSXY2(pk + v3);
				GteAVSZ4();
				if (OutX(pk + v3)) clip |= 8;
				if (OutY(pk + v3)) clip |= 0x80;
				if ((clip & 0xF) == 0xF) continue;
				if ((clip & 0xF0) == 0xF0) continue;
				GteReadOTZWord(h + 0x2C);
				if (F<uint32_t>(h, 0xC) != 0)
				{
					GteSetFarColor(F<uint8_t>(h, 8), F<uint8_t>(h, 9), F<uint8_t>(h, 0xA));
					if (gouraud)
					{
						GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
						GteSetIR0(F<int32_t>(h, 0xC));
						GteDPCT();
						GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
					}
					GteLoadRGBC(pk + 4);
					if (!gouraud) GteSetIR0(F<int32_t>(h, 0xC));   // (the gouraud quads' IR0 is still set)
					GteDPCS();
					GteStoreRGB2(pk + 4);
				}
				else if (gouraud)
				{
					F<uint32_t>(pk, 0x10) = *(const uint32_t *)(rec + 0x18);
					F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 0x1C);
					F<uint32_t>(pk, 0x28) = *(const uint32_t *)(rec + 0x20);
				}
				if (depth)
				{
					const int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
					F<int32_t>(h, 0x2C) = z;
					if (z < 0) F<int32_t>(h, 0x2C) = 0;
				}
				const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
				uint8_t *quad = pk;
				uint8_t *mode = pk + v3 + 8;
				pk = mode + 8;
				F<uint32_t>(mode, 0) = 0x1000000;
				F<uint32_t>(mode, 4) = 0xE1000220;
				InsertPrimAutoDepth(bucket, mode);
				InsertPrimAutoDepth(bucket, quad);
			}
			F<const uint8_t *>(h, 0x20) = rec;
			return (uint32_t)pk;
		}

		// list at p: count dword + count records of `size` bytes -> the next list
		static inline const uint8_t *SkipList(const uint8_t *p, uint32_t size)
		{
			const int32_t n = *(const int32_t *)p;
			if (n == 0) return p + 4;
			return p + (uint32_t)mul32(n, (int32_t)size) + 4;
		}
	}

	// the model's lists 4 (flat textured quads) and 8 (gouraud textured quads), additive; gouraud
	// only: list 8 alone, its depth + the header's, clamped at 0 (Zombie 0x6CA0E0 / 0x6CA1B0)
	inline uint32_t AdditiveQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor, bool gouraud_only = false)
	{
		using quads::SkipList;
		const uint8_t *model = parts::F<uint8_t *>(h, 0);
		const uint8_t *p = model + *(const uint32_t *)model;
		if (gouraud_only)
		{
			static const uint32_t before[] = { 12, 12, 20, 24, 20, 24, 28 };
			parts::F<const uint8_t *>(h, 0x20) = p;
			for (uint32_t size : before) parts::F<const uint8_t *>(h, 0x20) = p = SkipList(p, size);
			return quads::Quads(h, ot, shift, cursor, true, true);
		}
		const uint32_t before_flat[] = { 12, 12, 20 }, before_gouraud[] = { 20, 24, 28 };
		parts::F<const uint8_t *>(h, 0x20) = p;
		for (uint32_t size : before_flat) parts::F<const uint8_t *>(h, 0x20) = p = SkipList(p, size);
		cursor = quads::Quads(h, ot, shift, cursor, false);
		p = parts::F<uint8_t *>(h, 0x20);
		for (uint32_t size : before_gouraud) parts::F<const uint8_t *>(h, 0x20) = p = SkipList(p, size);
		return quads::Quads(h, ot, shift, cursor, true);
	}

	// ------------------------------------------------------------------
	// The anchor callback (Antidote 0x674630, the status-cure items 292..296: one source built with
	// different options): one object of a layout played at an anchor. The record's offset + the
	// anchor through the camera, composed with it; the record flags a build tests are `honoured`
	// (ANCHOR_* below). Vertex frame, fade and draw as PrimCallback.
	// ------------------------------------------------------------------
	enum AnchorFlag : uint32_t
	{
		ANCHOR_DIAGONAL_SCALE = 0x100,   // scale as a diagonal matrix multiplied in (0x56C220), not scale3DMatrix
		ANCHOR_OFFSET_AS_IS = 0x200,     // the anchor through the camera + the offset as is, not composed
		ANCHOR_OFFSET_TURNED = 0x800,    // the offset turned by the object's own rotation first (Blind)
		ANCHOR_ROT_ZYX = 0x400,          // rotation from the angles in ZYX order (0x701430)
		ANCHOR_MATRIX = 0x1000,          // the anchor through the camera + the offset through the arg's matrix (composed with it)
		ANCHOR_NOT_COMPOSED = 0x8000,    // the rotation not composed with the camera
		ANCHOR_ROT_XZY = 0x20000,        // rotation from the angles in XZY order (0x701530, Zombie)
		ANCHOR_ROT_YXZ = 0x40000,        // rotation from the angles in YXZ order (0x7015B0)
	};
#pragma pack(push, 1)
	struct AnchorArg
	{
		int16_t anchor[4]; // +0x00 the anchor (8 bytes)
		int32_t depth;     // +0x08 the prim header's +0x18
		uint32_t morph;    // +0x0C blended vertex frames
		uint32_t honoured; // the record flags tested (the module's build options)
		uint32_t *cursor;  // the packet cursor the objects are drawn at (the module's global in the original)
		const Mat4x3 *matrix = nullptr; // ANCHOR_MATRIX's (Echo Screen's module camera matrix, 0x67AAE0 of the camera)
		int32_t nearer_object = -1;     // this object of the layout drawn `nearer_by` nearer (Silence's 4, 0x20)
		int32_t nearer_by = 0;
		int32_t scaled = 0;             // Aero: the record offsets and the object scaled by `scale` (4.12)
		int32_t scale[3] = {};
		bool short_scale = false;       // Aero: the record's scale applied row by row as 16-bit factors (its inlined copy)
		bool engine_draw = false;       // Blind: drawn by the engine's renderer 0x572200, the header's depth not written, not 0x7043B0
		int32_t ot_shift = 2;           // the renderers' depth shift (Blind: 3)
		uint32_t additive_objects = 0;  // these objects (a bit each) drawn as their textured quads only, additive (Blind: 0 and 3)
		bool quads_gouraud_only = false; // Zombie: those objects' gouraud quads only, their depth + the header's, clamped at 0
		uint32_t far_objects = 0;       // Flare: these objects (a bit each) at depth >> 2 (0 stays 0)
	};
#pragma pack(pop)
	inline void RotationFromAnglesYXZ(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void RotationFromAnglesZYX(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701430)(angles, out); } // MAG_117_RotationFromAnglesZYX
	inline void RotationFromAnglesXZY(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701530)(angles, out); } // MAG_065_RotationFromAnglesXZY
	inline void MulMatrix3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }       // MatrixMultiply3x3: a = a * b

	static void __cdecl AnchorPrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const AnchorArg *arg = (const AnchorArg *)arg_;
		const uint32_t flags = r->flags & arg->honoured;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		if (flags & ANCHOR_ROT_YXZ) RotationFromAnglesYXZ(r->rot, &m);
		else if (flags & ANCHOR_ROT_ZYX) RotationFromAnglesZYX(r->rot, &m);
		else if (flags & ANCHOR_ROT_XZY) RotationFromAnglesXZY(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (turned by the object's rotation: Blind's), scaled by the arg's scale when
		// it has one (Aero's WindPart)
		int16_t pos[3] = { r->pos[0], r->pos[1], r->pos[2] };
		if (flags & ANCHOR_OFFSET_TURNED)
		{
			int16_t off[4] = { pos[0], pos[1], pos[2], 0 };   // in place, as the original
			MatrixMultiplyVector(&m, off, off);
			pos[0] = off[0];
			pos[1] = off[1];
			pos[2] = off[2];
		}
		if (arg->scaled)
		{
			pos[0] = (int16_t)(mul32(pos[0], arg->scale[0]) >> 12);
			pos[1] = (int16_t)(mul32(pos[1], arg->scale[1]) >> 12);
			pos[2] = (int16_t)(mul32(pos[2], arg->scale[2]) >> 12);
			Scale3DMatrix(&m, arg->scale);
		}
		if (flags & ANCHOR_MATRIX)   // (before 0x200: Flare's order)
		{
			// the anchor through the camera + the record's offset through the matrix (the offset's 4th
			// word never written: only loaded into the GTE V0 pad)
			const int16_t off[4] = { pos[0], pos[1], pos[2], 0 };
			int32_t v[3];
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(arg->matrix);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(v);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)v[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)v[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)v[2]);
			if (!(flags & ANCHOR_NOT_COMPOSED)) MatrixMultiply(arg->matrix, &m);
		}
		else if (flags & ANCHOR_OFFSET_AS_IS)
		{
			// the anchor through the camera (its 4th word goes to the GTE V0 pad only) + the record's offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)pos[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)pos[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)pos[2]);
		}
		else
		{
			// the record's offset + the anchor through the camera (4th word never written: only
			// loaded into the GTE V0 pad)
			const int16_t off[4] = { (int16_t)(pos[0] + arg->anchor[0]), (int16_t)(pos[1] + arg->anchor[1]), (int16_t)(pos[2] + arg->anchor[2]), 0 };
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(flags & ANCHOR_NOT_COMPOSED)) MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (flags & ANCHOR_DIAGONAL_SCALE)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
			}
			else if (arg->short_scale)
			{
				// each row of the rotation times its scale, read as 16 bits (a scale of 1.0 leaves the row)
				for (int k = 0; k < 3; k++)
				{
					const int16_t sk = (int16_t)r->scale[k];
					if (sk == 0x1000) continue;
					m.m[k][0] = (int16_t)(mul32(m.m[k][0], sk) >> 12);
					m.m[k][1] = (int16_t)(mul32(m.m[k][1], sk) >> 12);
					m.m[k][2] = (int16_t)(mul32(m.m[k][2], sk) >> 12);
				}
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
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
		if (!arg->engine_draw)
		{
			*(int32_t *)(h + 0x18) = arg->depth;
			if ((int32_t)(int16_t)r->flags_lo == arg->nearer_object) *(int32_t *)(h + 0x18) = (int32_t)((uint32_t)arg->depth - (uint32_t)arg->nearer_by);
			if (arg->depth != 0 && (uint32_t)object < 32 && (arg->far_objects >> object & 1)) *(int32_t *)(h + 0x18) = arg->depth >> 2;
		}
		if ((uint32_t)object < 32 && (arg->additive_objects >> object & 1)) *arg->cursor = AdditiveQuads(h, RenderOT(), arg->ot_shift, *arg->cursor, arg->quads_gouraud_only);
		else if (arg->engine_draw) *arg->cursor = RenderPrimModel(h, RenderOT(), arg->ot_shift, *arg->cursor);
		else *arg->cursor = RenderPrim2(h, RenderOT(), arg->ot_shift, *arg->cursor);
		FieldFree(0x58);
	}

	// the layout played one tick at `arg`'s anchor: the frames left (0 = finished)
	inline int PlayAtAnchor(void *layout, AnchorArg *arg)
	{
		return prim::play((prim::Layout *)layout, AnchorPrimCallback, (int)arg, 0);
	}

	// ------------------------------------------------------------------
	// The frame callback (Float 0x6C9760, Haste / Slow's ClockPart 0x6BBA40 / 0x6BD580): one object of
	// a layout in a frame of the target (turned with it, at a point of it) scaled by its size. The
	// record's offset scaled, through the frame, composed with it; the record flags a module's build
	// tests are `honoured` (FRAME_* below). Vertex frame and fade as PrimCallback.
	// ------------------------------------------------------------------
	enum FrameFlag : uint32_t
	{
		FRAME_DIAGONAL_SCALE = 0x100,   // the record's scale as a diagonal matrix multiplied in (0x56C220)
		FRAME_OFFSET_AS_IS = 0x200,     // the (scaled) offset as is, not turned, not composed
		FRAME_MATRIX = 0x1000,          // the offset through the arg's matrix (Haste's effect camera), composed with it
		FRAME_SUBTRACT = 0x2000,        // the draw mode | 0xC
		FRAME_NO_LIGHT = 0x4000,        // the draw mode 0x2000, not 0x2030
		FRAME_NOT_COMPOSED = 0x8000,    // the rotation not composed with the frame
	};
#pragma pack(push, 1)
	struct FrameArg
	{
		Mat4x3 m;          // +0x00 the frame: camera x (turned with the target, at its point)
		int32_t scale[3];  // +0x20 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x2C scale present
		int32_t depth;     // +0x30 the prim header's +0x18
		uint32_t morph;    // +0x34 blended vertex frames
		uint32_t honoured; // the record flags tested (the module's build options)
		uint32_t *cursor;  // the packet cursor the objects are drawn at (the module's global in the original)
		const Mat4x3 *matrix = nullptr; // FRAME_MATRIX's
		bool engine_draw = false;       // drawn by 0x572200, the header's depth not written (Haste, Slow), not 0x7043B0
	};
#pragma pack(pop)

	static void __cdecl FramePrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const FrameArg *arg = (const FrameArg *)arg_;
		const uint32_t flags = r->flags & arg->honoured;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		RotationFromAngles(r->rot, &m);
		// the record's offset (scaled by the block's scale when "scaled"; 4th word only loaded into the GTE V0 pad)
		int16_t off[4] = {};
		const int32_t scaled = arg->scaled;
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		if (flags & FRAME_MATRIX)
		{
			GteSetRotMatrixCtrl(arg->matrix);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(flags & FRAME_NOT_COMPOSED)) MatrixMultiply(arg->matrix, &m);
		}
		else if (flags & FRAME_OFFSET_AS_IS)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(flags & FRAME_NOT_COMPOSED)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (flags & FRAME_DIAGONAL_SCALE)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
			}
			else
			{
				int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = (flags & FRAME_NO_LIGHT) ? 0x2000 : 0x2030;
		if (flags & FRAME_SUBTRACT) *(uint32_t *)(h + 0x1C) |= 0xC;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		if (arg->engine_draw) *arg->cursor = RenderPrimModel(h, RenderOT(), 2, *arg->cursor);
		else
		{
			*(int32_t *)(h + 0x18) = arg->depth;
			*arg->cursor = RenderPrim2(h, RenderOT(), 2, *arg->cursor);
		}
		FieldFree(0x58);
	}

	// the layout played one tick in `arg`'s frame: the frames left (0 = finished)
	inline int PlayInFrame(void *layout, FrameArg *arg)
	{
		return prim::play((prim::Layout *)layout, FramePrimCallback, (int)arg, 0);
	}

	// ------------------------------------------------------------------
	// The table callback (Aura 0x6CDE10): one object of a layout in a frame, its draw mode from a
	// table of one word per object (exe data). Flag 0x10000000 of its word: the offset through the
	// frame's 3x3 + its translation, the rotation kept; else the object composed with the frame. Its
	// draw mode: the word with bit 0x2000 set (| 0xC0 when it fades); the engine's renderer 0x572200.
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct TableArg
	{
		Mat4x3 m;          // +0x00 the frame
		uint32_t flags;    // +0x20 the per-object words (an address)
		uint32_t morph;    // +0x24 blended vertex frames
		uint32_t *cursor;  // the packet cursor the objects are drawn at (the module's global in the original)
		int16_t offset[3] = {};   // added to each record's offset first (Berserk's: its spawn point, the frame the camera)
	};
#pragma pack(pop)

	static void __cdecl TablePrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const TableArg *arg = (const TableArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->offset[0];
		m.t[1] = (int32_t)r->pos[1] + arg->offset[1];
		m.t[2] = (int32_t)r->pos[2] + arg->offset[2];
		const uint32_t word = *(const uint32_t *)(arg->flags + 4 * (int16_t)r->index);
		if (word & 0x10000000)
		{
			TransformVectorBy3x3Matrix(&arg->m, m.t, m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		}
		else ComposeAffineTransform(&arg->m, &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t mode = (*(const uint32_t *)(arg->flags + 4 * (int16_t)r->index) & 0xDFFF) | 0x2000;
		*(uint32_t *)(h + 0x1C) = mode;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = mode | 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*arg->cursor = RenderPrimModel(h, RenderOT(), 2, *arg->cursor);
		FieldFree(0x58);
	}

	// the layout played one tick through the table callback: the frames left (0 = finished)
	inline int PlayWithTable(void *layout, TableArg *arg)
	{
		return prim::play((prim::Layout *)layout, TablePrimCallback, (int)arg, 0);
	}

	// ------------------------------------------------------------------
	// Frames
	// ------------------------------------------------------------------
	// Hypnotize 0x6B18A0 (= Dissolving Acid's 0x6B2B70): a point (x, y, z, bone) through the
	// caster's bone composed with its matrix
	inline void BonePoint(uint8_t *caster, const int16_t *entry, int16_t *out)
	{
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), (const Mat4x3 *)(**(uint8_t ***)(caster + 0x64) + 0x20 + (int32_t)entry[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(entry);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// view space on a point's screen position (Hypnotize 0x6B1243..0x6B130B): the point projected,
	// the basis of its screen offset from the centre (PSX pixels: 160, 120) times 2^shift at
	// basis_h x H, translation at depth_h x H (H = the projection distance). The basis direction and
	// the translation share the frame's words (the original's stack layout). The PC's projection is
	// in eighths of a pixel: `pixels` takes them back to pixels first, as the PSX had them.
	inline void ViewFrame(const int16_t *pos, int32_t basis_h, int32_t depth_h, int32_t shift, bool pixels, Mat4x3 *m)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		if (pixels)
		{
			sxy[0] = (int16_t)(sxy[0] >> 3);
			sxy[1] = (int16_t)(sxy[1] >> 3);
		}
		int32_t *v = m->t;
		v[0] = shl32(0xA0 - (int32_t)sxy[0], shift);
		v[1] = shl32(0x78 - (int32_t)sxy[1], shift);
		const int32_t h = ProjectionH();
		v[2] = h * basis_h;
		BuildOrthonormalBasis(v, m);
		v[1] = (int32_t)(0u - (uint32_t)v[1]);
		v[0] = (int32_t)(0u - (uint32_t)v[0]);
		v[2] = h * depth_h;
	}

	// the camera rows scaled by s / 4096 through the GTE (IR0 interpolation, Hypnotize 0x6B190D..0x6B1958)
	inline void ScaledCamera(int32_t s, Mat4x3 *m)
	{
		GteSetIR0(s);
		GteLoadIR123(&Camera().m[0][0]);
		GteGPF();
		GteStoreIR123(&m->m[0][0]);
		GteLoadIR123(&Camera().m[1][0]);
		GteGPF();
		GteStoreIR123(&m->m[1][0]);
		GteLoadIR123(&Camera().m[2][0]);
		GteGPF();
		GteStoreIR123(&m->m[2][0]);
	}

	// the camera's rotation as is
	inline void CameraRotation(Mat4x3 *m) { memcpy(&m->m, &Camera().m, sizeof(m->m)); }

	// a position through the camera: rotation already in m, translation = camera x pos + camera t
	inline void CameraAnchor(const int16_t *pos, Mat4x3 *m)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0();
		GteReadMAC123(m->t);
		m->t[0] = (int32_t)((uint32_t)m->t[0] + (uint32_t)Camera().t[0]);
		m->t[1] = (int32_t)((uint32_t)m->t[1] + (uint32_t)Camera().t[1]);
		m->t[2] = (int32_t)((uint32_t)m->t[2] + (uint32_t)Camera().t[2]);
	}

	// ------------------------------------------------------------------
	// Inputs: where a part is placed
	// ------------------------------------------------------------------
	// one of `count` entities at random (CRT rand; Hypnotize's main task)
	inline uint8_t *RandomEntity(uint8_t *const *ents, int32_t count)
	{
		const int32_t i = mul32(CrtRand(), count) >> 15;
		return ents[i];
	}

	// a random bone of `ent` (CRT rand: the spawn flag, then the bone), its effect point
	inline void RandomBonePoint(uint8_t *ent, int16_t *out)
	{
		const int32_t flag = CrtRand() >> 3;
		const int32_t bone = mul32(CrtRand(), (int32_t)***(uint8_t ***)(ent + 0x64)) >> 15;
		GetEffectSpawnPosition(ent, bone, flag, out);
	}

	// `ent`'s default effect position (0x571400) between two shares of its height: y = its +0x24 word
	// + (h x times_a / share_a + h x times_b / share_b) / 2, h = its height (+0x92); Antidote's
	// anchor: 1 / 2 and 1 / 2, the middle; Holy Water's: 2 / 3 and 1 / 2
	inline void HeightPoint(uint8_t *ent, int32_t times_a, int32_t share_a, int32_t times_b, int32_t share_b, int16_t *out)
	{
		GetDefaultEffectPosition(ent, out);
		const int32_t h = *(const int16_t *)(ent + 0x92);
		const int32_t a = h * times_a / (share_a ? share_a : 1), b = h * times_b / (share_b ? share_b : 1);
		out[1] = (int16_t)((a + b) / 2 + *(const int16_t *)(ent + 0x24));
	}

	inline void RotationY(int16_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)((int32_t)(uint16_t)angle, out); }   // MAG_063_sub_701270

	// a frame of an entity (Float, Haste, Slow): turned about the vertical axis with it (-facing,
	// +0x0E), at `point` (x, y, z), composed with the camera
	inline void TargetFrame(const uint8_t *ent, const int32_t *point, Mat4x3 *m)
	{
		RotationY((int16_t)-*(const int16_t *)(ent + 0xE), m);
		m->t[0] = point[0];
		m->t[1] = point[1];
		m->t[2] = point[2];
		ComposeAffineTransform(&Camera(), m, m);
	}

	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }   // Math_Sqrt

	// a frame at an entity's feet scaled by the square root of its size (Aura 0x6CDB60): a diagonal
	// of sqrt(size << shift) (no turn), at its effect point `bone`'s x / z and its +0x24, composed
	// with the camera (the frame's pad word is never written: 0)
	inline void SqrtSizeFrame(uint8_t *ent, int32_t shift, int32_t bone, Mat4x3 *m FX_HELD(, Mat4x3 *held_local = nullptr))
	{
		const int16_t s = (int16_t)Sqrt(shl32(*(const int16_t *)(ent + 0x26), shift));
		int16_t pos[4];
		m->m[0][0] = s;
		m->m[1][1] = s;
		m->m[2][2] = s;
		m->m[2][1] = 0;
		m->m[2][0] = 0;
		m->m[1][2] = 0;
		m->m[1][0] = 0;
		m->m[0][2] = 0;
		m->m[0][1] = 0;
		m->pad = 0;
		GetEffectSpawnPosition(ent, bone, 0, pos);
		m->t[0] = pos[0];
		m->t[2] = pos[2];
		m->t[1] = *(const int16_t *)(ent + 0x24);
		// 30 fps layer: the frame before the camera (mag111_aura_held.inc)
		FX_HELD(if (held_local) *held_local = *m;)
		ComposeAffineTransform(&Camera(), m, m);
	}

	// an entity's effect point `bone` at its feet raised by `lift` (y: its +0x24 + lift; y goes down)
	inline void FeetPoint(uint8_t *ent, int32_t bone, int32_t lift, int16_t *out)
	{
		GetEffectSpawnPosition(ent, bone, 0, out);
		out[1] = (int16_t)(*(const int16_t *)(ent + 0x24) + lift);
	}

	// a fade (0x1000 = gone) of strength `strength` at tick c: in over `in` ticks, held, out over
	// `out` ticks after `from` (Aura's flames: 8, 0xC, 8)
	inline int32_t FadeInOut(int32_t c, int32_t strength, int32_t in, int32_t from, int32_t out)
	{
		if (c < in)
		{
			const int32_t e = 0x1000 - (shl32(c, 12) / in);
			return 0x1000 - (mul32(strength, 0x1000 - e) >> 12);
		}
		if (c > from)
		{
			const int32_t e = shl32(c - from, 12) / out;
			return 0x1000 - (mul32(strength, 0x1000 - e) >> 12);
		}
		return 0x1000 - strength;
	}

	// a prim model (no layout: its first vertex frame, its draw mode `mode`, black fade colour, `fade`)
	// turned about the view's z axis by `angle` and scaled by `radius` in x / y, at `pos` through the
	// camera, `nearer` subtracted from its depth; the engine's renderer (Aura's flames 0x6CE020)
	inline void DrawSpinningModel(uint32_t model, const int16_t *pos, int16_t angle, int32_t radius, int32_t fade, uint32_t mode,
	                              int32_t nearer, uint32_t &cursor)
	{
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = model;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = mode;
		Mat4x3 m = {};
		const int32_t sn = mul32(ComputeSin(angle), radius) >> 12;
		const int32_t cs = mul32(parts::ComputeCos(angle), radius) >> 12;
		m.m[0][1] = (int16_t)-sn;
		m.m[1][0] = (int16_t)sn;
		m.m[0][0] = (int16_t)cs;
		m.m[0][2] = 0;
		m.m[1][1] = (int16_t)cs;
		m.m[1][2] = 0;
		m.m[2][0] = 0;
		m.m[2][1] = 0;
		m.m[2][2] = 0x1000;
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0();
		GteReadMAC123(m.t);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2] - (uint32_t)nearer);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Whirl leaves (Aero 0x6C14B0): a model circling its spawner's anchor, climbing (the spiral's
	// radius shrinking with the height) while its spawner is young, then falling, bouncing on the
	// ground, drifting outwards and shrinking away; tumbling on three axes all along.
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct WhirlLeaf // a node's fields after the header (0x30 bytes)
	{
		int16_t action;    // +0x00 the spawner's action (not read)
		int16_t reach;     // +0x02 spiral reach (the target's size / 2 + 0x800)
		int16_t scale;     // +0x04 size 4.12, grows to 0x1000
		int16_t angle;     // +0x06 around the anchor
		int16_t speed;     // +0x08 angle step
		int16_t cx;        // +0x0A the anchor's x
		int16_t cz;        // +0x0C the anchor's z
		int16_t pos[3];    // +0x0E x, y, z
		int16_t pad14;
		int16_t cosv;      // +0x16 cos(angle) of the last circling tick
		int16_t vy;        // +0x18 vertical speed
		int16_t sinv;      // +0x1A sin(angle) of the last circling tick
		int16_t pad1C;
		int16_t ang[3];    // +0x1E tumbling angles
		int16_t fall;      // +0x24 ticks since the fall started
		int8_t spin[3];    // +0x26 tumbling speeds (x 8)
		uint8_t pad29[3];
		const int16_t *owner_counter; // +0x2C its spawner's counter (Aero: its target task's)
	};
#pragma pack(pop)
	static_assert(sizeof(WhirlLeaf) == 0x30, "whirl leaf");

	// a new leaf around `anchor` (CRT rand in the original's order: its height, its angle, its three
	// tumbling angles and speeds)
	inline void WhirlLeafStart(WhirlLeaf *f, int16_t action, const int16_t *anchor, int32_t reach, const int16_t *owner_counter,
	                           int32_t scale, int32_t speed, int32_t vy)
	{
		f->action = action;
		f->cx = anchor[0];
		f->cz = anchor[2];
		const int32_t y = CrtRand() & 0xFF;
		f->owner_counter = owner_counter;
		f->pos[1] = (int16_t)y;
		f->reach = (int16_t)reach;
		f->scale = (int16_t)scale;
		f->angle = (int16_t)CrtRand();
		f->speed = (int16_t)speed;
		f->ang[0] = (int16_t)CrtRand();
		f->ang[1] = (int16_t)CrtRand();
		f->ang[2] = (int16_t)CrtRand();
		f->spin[0] = (int8_t)(CrtRand() + 0x80);
		f->spin[1] = (int8_t)(CrtRand() + 0x80);
		const int32_t s2 = CrtRand();
		f->vy = (int16_t)vy;
		f->spin[2] = (int8_t)(s2 + 0x80);
	}

	// one tick of a leaf: false once it has shrunk away. It climbs while its spawner's counter is
	// below `climb` (faster from `speed_up`), then falls.
	inline bool WhirlLeafMove(WhirlLeaf *f, int32_t climb, int32_t speed_up)
	{
		const int16_t oc = *f->owner_counter;
		if (oc < climb)
		{
			if (oc >= speed_up)
			{
				f->vy = (int16_t)(f->vy - 4);
				f->speed = (int16_t)(f->speed + 0x10);
			}
			f->scale = (int16_t)(f->scale + 0x100);
			if (f->scale > 0x1000) f->scale = 0x1000;
			f->pos[1] = (int16_t)(f->pos[1] + f->vy);
			const int32_t y = f->pos[1] >> 6;
			const int32_t radius = shl32(f->reach, 5) / (mul32(y, y) + 0x200);
			f->angle = (int16_t)(f->angle + f->speed);
			const int32_t c = parts::ComputeCos(f->angle);
			f->cosv = (int16_t)c;
			f->pos[0] = (int16_t)((mul32((int16_t)c, radius) >> 9) + f->cx);
			const int32_t s = ComputeSin(f->angle);
			f->sinv = (int16_t)s;
			f->pos[2] = (int16_t)((mul32((int16_t)s, radius) >> 9) + f->cz);
		}
		else
		{
			const int16_t n = f->fall;
			f->fall = (int16_t)(n + 1);
			if (n > 0x10)
			{
				f->scale = (int16_t)(f->scale - 0x200);
				if (f->scale <= 0) return false;
			}
			f->vy = (int16_t)(f->vy + 0x10);
			f->pos[1] = (int16_t)(f->pos[1] + f->vy);
			if (f->pos[1] >= 0) f->vy = (int16_t)-f->vy;
			f->pos[0] = (int16_t)(f->pos[0] + (int16_t)(f->cosv >> 6));
			f->pos[2] = (int16_t)(f->pos[2] + (int16_t)(f->sinv >> 6));
		}
		f->ang[0] = (int16_t)(f->ang[0] + (int16_t)(f->spin[0] << 3));
		f->ang[1] = (int16_t)(f->ang[1] + (int16_t)(f->spin[1] << 3));
		f->ang[2] = (int16_t)(f->ang[2] + (int16_t)(f->spin[2] << 3));
		return true;
	}

	// ------------------------------------------------------------------
	// Collapsing sparks (Flare 0x6BD020): a sprite on a sphere around a point, its direction turning;
	// it shows at its spawn radius, collapses into the centre at a random time, bursts out again from
	// `burst` and fades out from `fade`.
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct CollapseSpark // a node's fields after the header (0x20 bytes)
	{
		int16_t size;      // +0x00 radius around the point
		int16_t grow;      // +0x02 spawn radius, then the burst step
		int16_t start;     // +0x04 tick the collapse starts
		int16_t pos[3];    // +0x06 the point x, y, z
		int16_t shade;     // +0x0C sprite size (0x4B0)
		int16_t ang[3];    // +0x0E direction angles
		int16_t alpha;     // +0x14 brightness (sprite colour)
		int16_t vel[3];    // +0x16 angular velocity
		int16_t age;       // +0x1C
		int16_t mode;      // +0x1E (a pad in the original) this tick's sprite mode: 4 coloured by alpha, 0 plain, -1 none, -2 gone
	};
#pragma pack(pop)
	static_assert(sizeof(CollapseSpark) == 0x20, "collapse spark");

	// a new spark around `point` (its 4 words: the 4th lands on `shade`, then set) of a spawner at
	// tick `counter`, its radius from `scale` (s - s / 4): CRT rand in the original's order (its
	// start in `window` ticks, its velocity's three angles, its direction's three)
	inline void CollapseSparkStart(CollapseSpark *f, const int16_t *point, int32_t counter, int32_t scale, int32_t window, int32_t speed, int32_t sprite)
	{
		memset(f, 0, sizeof(*f));
		const int32_t r = CrtRand();
		f->start = (int16_t)((mul32(r, window) >> 15) - counter + 4);
		const int16_t s0 = (int16_t)(scale - (scale >> 2));
		f->size = s0;
		f->grow = s0;
		f->pos[0] = point[0];
		f->pos[1] = point[1];
		f->pos[2] = point[2];
		f->shade = (int16_t)sprite;
		int16_t ang[4] = {};
		ang[0] = (int16_t)CrtRand();
		ang[1] = (int16_t)CrtRand();
		ang[2] = (int16_t)CrtRand();
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(ang, &m);
		int16_t v[4] = { 0, 0, (int16_t)speed, 0 };
		MatrixMultiplyVector(&m, v, f->vel);
		f->ang[0] = (int16_t)CrtRand();
		f->ang[1] = (int16_t)CrtRand();
		f->ang[2] = (int16_t)CrtRand();
	}

	// the tick's size / brightness steps before the draw: the sprite mode (4 = coloured by alpha,
	// 0 = plain), -1 = nothing drawn, -2 = finished
	inline int CollapseSparkPrepare(CollapseSpark *p, int32_t burst, int32_t fade)
	{
		const int32_t age = p->age;
		const uint32_t k = (uint32_t)(age - p->start);
		if (k <= 8)
			p->size = (int16_t)((uint32_t)mul32(p->grow, (int32_t)(8 - k)) >> 3);
		if (age >= burst)
		{
			const int16_t g = p->grow;
			const int16_t q = (int16_t)(g >> 2);
			p->size = (int16_t)(p->size + q);
			if (g > 0x200) p->grow = (int16_t)(g - q);
			else p->grow = 0x200;
			if (age >= fade)
			{
				p->alpha = (int16_t)(p->alpha - 0x20);
				if (p->alpha <= 0) return -2;
				return 4;
			}
		}
		if (p->size <= 0) return -1;
		if (p->alpha < 0x80)
		{
			p->alpha = (int16_t)(p->alpha + 8);
			return 4;
		}
		return 0;
	}

	// the sprite sequence `seq` at the spark's place on its sphere
	inline void CollapseSparkDraw(const CollapseSpark *p, int mode, uint32_t seq, uint32_t &cursor)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (mode == 4)
		{
			const uint8_t a = (uint8_t)p->alpha;
			*(uint16_t *)(h + 0x24) = 4;
			h[0x1E] = a;
			h[0x1D] = a;
			h[0x1C] = a;
		}
		else *(uint16_t *)(h + 0x24) = 0;
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(p->ang, &m);
		int16_t v[4] = { 0, 0, p->size, 0 };
		int16_t at[4] = {};
		MatrixMultiplyVector(&m, v, at);
		at[1] = (int16_t)(at[1] + p->pos[1]);
		at[0] = (int16_t)(at[0] + p->pos[0]);
		at[2] = (int16_t)(at[2] + p->pos[2]);
		TransformCameraByShadowRotation(at, p->shade, -(p->shade >> 2));
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 4) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}

	inline void CollapseSparkMove(CollapseSpark *p)
	{
		p->ang[0] = (int16_t)(p->ang[0] + p->vel[0]);
		p->ang[1] = (int16_t)(p->ang[1] + p->vel[1]);
		p->ang[2] = (int16_t)(p->ang[2] + p->vel[2]);
		p->age++;
	}

	// ------------------------------------------------------------------
	// Sprite motes (Berserk 0x6D1250): a sprite sequence in place, its frame its counter; rising ones
	// (Berserk's embers) move up, decelerating
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct Mote // a node's fields after the header (0x10 bytes)
	{
		uint8_t counter;   // +0x00 sequence frame
		int8_t accel;      // +0x01 rising: vertical acceleration
		int16_t scale;     // +0x02
		int16_t pos[3];    // +0x04
		int16_t vel;       // +0x0A rising: vertical speed
		uint32_t seq;      // +0x0C sprite sequence data
	};
#pragma pack(pop)
	static_assert(sizeof(Mote) == 0x10, "mote");

	inline void MoteDraw(const Mote *p, uint32_t &cursor)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		// the original pushes the scale as eax with only its low word loaded: the high word is the
		// header address's (0x571BC0 reads the low word only)
		TransformCameraByShadowRotation(p->pos, (int32_t)(((uint32_t)h & 0xFFFF0000u) | (uint16_t)p->scale), -((int32_t)p->scale >> 2));
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = (int16_t)(int8_t)p->counter;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}

	// a tick of a mote (rising: its speed + its acceleration, its height + its speed): done once its
	// frame reaches `frames`
	inline bool MoteStep(Mote *p, bool rising, int32_t frames)
	{
		if (rising)
		{
			p->vel = (int16_t)(p->vel + p->accel);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel);
		}
		p->counter++;
		return (int8_t)p->counter >= frames;
	}

	// a prim model (no layout) at `pos` (through the camera), unrotated, scaled uniformly by `scale`,
	// faded by `fade`, its draw mode `mode` (Berserk's glow: sin(e x 1024 / ticks) + base, fade
	// e x 2048 / ticks + base)
	inline void PulseModelDraw(uint32_t model, const int16_t *pos, int32_t e, int32_t ticks, int32_t scale_base, int32_t fade_base, uint32_t mode, uint32_t &cursor)
	{
		const uint32_t t = (uint32_t)(ticks ? ticks : 1);
		const int32_t scale = ComputeSin((int32_t)((uint32_t)shl32(e, 10) / t)) + scale_base;
		const int32_t fade = (int32_t)((uint32_t)shl32(e, 11) / t + (uint32_t)fade_base);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		// the matrix pad is never written by the original (stack)
		Mat4x3 m = {};
		m.m[0][0] = (int16_t)scale;
		m.m[1][1] = (int16_t)scale;
		m.m[2][2] = (int16_t)scale;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(m.t);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = model;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = mode;
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// a prim model (no layout) turned by three angles (0x56CD50), scaled by `scale`, at `pos` composed
	// with the camera, its draw mode `mode` (its fade words left as the scratch held them); the
	// engine's renderer (Aero's leaves)
	inline void DrawTumblingModel(uint32_t model, const int16_t *ang, int32_t scale, const int16_t *pos, uint32_t mode, uint32_t &cursor)
	{
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = model;
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(ang, &m);
		const int32_t v[3] = { scale, scale, scale };
		Scale3DMatrix(&m, v);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = mode;
		cursor = RenderPrimModel(h, RenderOT(), 2, cursor);
		FieldFree(0x58);
	}

	// a scale (4.12) from an entity's size word (+0x26): ((size >> 1) + base) >> shift (Float: 0x800, 2)
	inline int32_t SizeScale(const uint8_t *ent, int32_t base, int32_t shift)
	{
		return ((*(const int16_t *)(ent + 0x26) >> 1) + base) >> shift;
	}

	// the depth of a point of a frame, by an entity's size (Float's upper layout 0x6C96C2..0x6C96F3):
	// (size + 0x800) x (the point through the frame).z >> 15
	inline int32_t PointDepth(const Mat4x3 *m, const uint8_t *ent, const int16_t *point)
	{
		int16_t out[4];
		MatrixMultiplyVector(m, point, out);
		return mul32(*(const int16_t *)(ent + 0x26) + 0x800, out[2]) >> 15;
	}

	// the screen flash level at tick c (Dispel 0x6CF700): c x level / rise over the first `rise`
	// ticks, then level, then (end - c) x level / (end - fall) from `fall` (signed divisions)
	inline int32_t ScreenFlashCurve(int32_t c, int32_t rise, int32_t level, int32_t fall, int32_t end)
	{
		if (c < rise) return c * level / rise;
		if (c >= fall) return (end - c) * level / (end - fall);
		return level;
	}

	// a screen flash level of tick c (Flare's): c x up before `rise`, (end - c) x down after
	// `fall_after`, else `level` (a floor the target tasks raise it to)
	inline int32_t ScreenFlashRiseHoldFall(int32_t c, int32_t rise, int32_t up, int32_t level, int32_t fall_after, int32_t end, int32_t down)
	{
		if (c < rise) return c * up;
		if (c > fall_after) return (end - c) * down;
		return level;
	}

	// sub_56CC00: out = colour blend of a (weight wa) and b (weight wb), weights 4.12
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }

	// an entity's colour (+0x28) at tick c (Flare's red): from `saved` to `tint` over from ..
	// from + ticks - 1 (weight (c - from) << shift), `tint` until back_from, back to `saved` over
	// back_from .. back_from + back_ticks - 1 (weight (c - back_from) << back_shift), else `saved`
	inline void TintEntity(uint8_t *entity, const void *tint, const uint32_t *saved, int32_t c, int32_t from, int32_t ticks, int32_t shift,
	                       int32_t back_from, int32_t back_ticks, int32_t back_shift)
	{
		if ((uint32_t)(c - from) < (uint32_t)ticks)
		{
			const int32_t w = (c - from) << shift;
			BlendColour(tint, saved, w, 0x1000 - w, entity + 0x28);
		}
		else if ((uint32_t)(c - back_from) < (uint32_t)back_ticks)
		{
			const int32_t w = (c - back_from) << back_shift;
			BlendColour(tint, saved, 0x1000 - w, w, entity + 0x28);
		}
		else if (c >= from && c <= back_from) *(uint32_t *)(entity + 0x28) = *(const uint32_t *)tint;
		else *(uint32_t *)(entity + 0x28) = *saved;
	}

	// a screen flash level of tick c rising and falling (Blind's, a level each target task raises):
	// (c - from) x up over from..peak - 1, (to - c) x down over peak..to, -1 outside
	inline int32_t ScreenFlashRiseFall(int32_t c, int32_t from, int32_t peak, int32_t to, int32_t up, int32_t down)
	{
		const int32_t e = c - from;
		if ((uint32_t)e < (uint32_t)(peak - from)) return e * up;
		if ((uint32_t)e <= (uint32_t)(to - from)) return (to - c) * down;
		return -1;
	}

	// `point` moved ahead of an entity's facing (+0x0E) by the unit circle >> shift (Petrify Stare's
	// stare: x - sin >> 4, z - cos >> 4), its height kept
	inline void AheadOfFacing(const uint8_t *ent, const int16_t *point, int32_t shift, int16_t *out)
	{
		const int32_t facing = *(const int16_t *)(ent + 0xE);
		out[0] = (int16_t)((-ComputeSin(facing) >> shift) + point[0]);
		out[2] = (int16_t)((-parts::ComputeCos(facing) >> shift) + point[2]);
		out[1] = point[1];
	}

	// a point around `centre` (Dispel's sparkles): x and z each centre + ((r + r' - 0x8000) x spread
	// >> 18), y = centre.y - (r >> 6) + lift (CRT rand, in the original's order: x's two, y's, z's two)
	inline void RandomAround(const int16_t *centre, int32_t spread, int32_t lift, int16_t *out)
	{
		const int32_t r1 = CrtRand();
		const int32_t r2 = CrtRand();
		out[0] = (int16_t)((mul32(r1 + r2 - 0x8000, spread) >> 18) + centre[0]);
		const int32_t r3 = CrtRand();
		out[1] = (int16_t)(centre[1] - (int16_t)(r3 >> 6) + lift);
		const int32_t r4 = CrtRand();
		const int32_t r5 = CrtRand();
		out[2] = (int16_t)((mul32(r4 + r5 - 0x8000, spread) >> 18) + centre[2]);
	}

	// point e of a circle around `centre` (Dispel's rising circle): angle = (e << 12) / turns, x / z =
	// centre + sin / cos >> 4, y = centre.y - (angle >> 3) + lift
	inline void RisingCirclePoint(const int16_t *centre, int32_t e, int32_t turns, int32_t lift, int16_t *out)
	{
		const int32_t angle = (int32_t)((uint32_t)e << 12) / turns;
		out[0] = (int16_t)((int16_t)(ComputeSin(angle) >> 4) + centre[0]);
		out[2] = (int16_t)((int16_t)(parts::ComputeCos(angle) >> 4) + centre[2]);
		out[1] = (int16_t)(centre[1] - (int16_t)(angle >> 3) + lift);
	}

	// a drawing depth from an entity's size word (+0x26): (base - size) >> shift (Silence: -0x800, 5)
	inline int32_t DepthFromSize(const uint8_t *ent, int32_t base, int32_t shift)
	{
		return (int32_t)((uint32_t)base - (uint32_t)(int32_t)*(const int16_t *)(ent + 0x26)) >> shift;
	}

	// where a part is anchored on an entity (the status-cure items' options)
	enum AnchorKind { ANCHOR_AT_POSITION = 0, ANCHOR_AT_HEIGHT = 1, ANCHOR_AT_BONE = 2 };
	// its default effect position (0x571400), that point between two shares of its height
	// (HeightPoint), or its effect point `bone` (0x502170, flag 0)
	inline void AnchorPoint(uint8_t *ent, int32_t kind, int32_t bone, int32_t times_a, int32_t share_a, int32_t times_b, int32_t share_b, int16_t *out)
	{
		if (kind == ANCHOR_AT_HEIGHT) HeightPoint(ent, times_a, share_a, times_b, share_b, out);
		else if (kind == ANCHOR_AT_BONE) GetEffectSpawnPosition(ent, bone, 0, out);
		else GetDefaultEffectPosition(ent, out);
	}

	// ------------------------------------------------------------------
	// Renderers
	// ------------------------------------------------------------------
	// a sprite sequence frame at a position through the shadow-rotation camera (Hypnotize 0x6B1742..0x6B17A0)
	inline void DrawSequenceSprite(const int16_t *pos, uint32_t seq, int16_t frame, int32_t rot_a, int32_t rot_b, uint32_t &cursor)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, rot_a, rot_b);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}
}
}
