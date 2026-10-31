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
// rotation scaled. Also the inputs these parts are placed with (a random target of an action, a
// random bone of it) and the camera-facing sequence sprite. Everything takes plain values (the
// packet cursor to draw into, the caster, the morph buffer), never a module's memory. Bit-exact
// with Hypnotize's code (mag133_hypnotize.cpp), each piece named with its original.

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
