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

// Effect 121: Blind (spell, MAG_121_*).
//
// Structure (setup MAG_121_BLIND 0x6BDE80 -> _Init 0x6BDEB0, file loader 0x6BDE90 = the texture
// file named at 0x1277634; the setup starts camera animation 0x1276E00 and queues the TIM):
//   RootTask (0x6BDF20) - alternates the packet arena (magic buffer + 0x1B34 / + 0x19B34), clears
//     the module's screen flash level; on counter 1 of a 10-tick cycle sets up the target pool
//     (first time) and starts the target task of the next action on its first target (no wait);
//     runs the target queue, then sets the screen flash to the level the target tasks raised;
//     ends when the queue is empty.
//   Target (0x6BE080) - node 0x83C, one per action: raises the flash level (up over 22..25 by
//     625 per tick, down over 26..33 from 0x138 * 8); follows the target's effect anchor 0xF0 for
//     20 ticks; plays the prim-model layout 0x12726A0 with the callback BlindPart (0x6BE1C0) every
//     tick (sound at 4, screen fades at 13, 18, 21, damage at 32); ends when the layout is over.
//   BlindPart (0x6BE1C0) - one object of the layout: record flag 0x800 = the offset is rotated by
//     the object's own rotation first; flag 0x200 = the offset stays in screen axes (the anchor
//     through the camera, the offset added unrotated, the rotation not composed with the camera),
//     else anchor + offset through the camera; flag 0x100 = scale by a diagonal matrix product,
//     else scale3DMatrix. Objects 0 and 3 are drawn by QuadsAdditive (0x6BE4E0: only the model's
//     textured quads, flat 0x6AE420 and gouraud 0x6BE5C0, each after an additive draw-mode
//     primitive), the others by Effect_RenderPrimModel (0x572200); both with OT shift 3.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25214F8..0x2521540 (screen flash level 0x25214F8, texture file, magic buffer
// base, context, root pool, packet cursor 0x252151C, target / root queues). The target pool
// (3 x 0x83C), the morph vertex buffer (+0x18B4) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace blind121
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &FlashLevel() { return var<int32_t>(0x25214F8); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521500); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521504); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252151C); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521520; }         // pool: magic buffer, 3 x 0x83C
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6BDF20;
	static const uint32_t ORIG_TargetTask = 0x6BE080;
	static const uint32_t MODEL_Blind = 0x12726A0;       // prim-model layout data (0x820-byte layout)
	static const void *const SOUND_Blind = (const void *)0x1277630;

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                   // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x83C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		int16_t pos[4];    // +0x10 effect anchor 0xF0 x, y, z (4th word stays 0)
		uint8_t *entity;   // +0x18 target entity
		uint8_t layout[0x820]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z, 0
		uint32_t morph;    // +0x08 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x83C, "Blind nodes");
	static_assert(sizeof(PrimArg) == 0xC, "Blind prim block");

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// screen clip of a projected vertex (x 0..0xA00, y 0..0x6C0: the PC's 4x subpixel screen)
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	// ------------------------------------------------------------------
	// sub_6AE420 (in the Oil Shot module, effect 136, shared): the prim model's flat textured
	// quads (list at header +0x20, 0x18-byte records), each after an additive draw-mode
	// primitive (same bucket)
	// ------------------------------------------------------------------
	static uint32_t FlatQuadsAdditive(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *list = F<uint8_t *>(h, 0x20);
		const int32_t count = *(const int32_t *)list;
		const uint8_t *rec = list + 4;
		const uint8_t *verts = F<uint8_t *>(h, 4);
		F<const uint8_t *>(h, 0x20) = rec;
		if (count <= 0) return (uint32_t)pk;
		for (int32_t k = count; k != 0; k--, rec += 0x18)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x9000000;
			F<uint32_t>(pk, 4) = *(const uint32_t *)rec;
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			F<uint32_t>(pk, 0x1C) = uv;
			F<uint32_t>(pk, 0x24) = uv >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			uint32_t clip = 0;
			GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x10)) clip |= 0x20;
			if (OutY(pk + 0x18)) clip |= 0x40;
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x20)) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint32_t>(h, 0xC) != 0)
			{
				GteSetFarColor(F<uint8_t>(h, 8), F<uint8_t>(h, 9), F<uint8_t>(h, 0xA));
				GteLoadRGBC(pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
			uint8_t *quad = pk;
			uint8_t *mode = pk + 0x28;
			pk = mode + 8;
			F<uint32_t>(mode, 0) = 0x1000000;
			F<uint32_t>(mode, 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, quad);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// sub_6BE5C0: the prim model's gouraud textured quads (list at header +0x20, 0x24-byte
	// records), each after an additive draw-mode primitive (same bucket)
	// ------------------------------------------------------------------
	static uint32_t GouraudQuadsAdditive(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *list = F<uint8_t *>(h, 0x20);
		const int32_t count = *(const int32_t *)list;
		const uint8_t *rec = list + 4;
		const uint8_t *verts = F<uint8_t *>(h, 4);
		F<const uint8_t *>(h, 0x20) = rec;
		if (count <= 0)
		{
			F<const uint8_t *>(h, 0x20) = rec;
			return (uint32_t)pk;
		}
		for (int32_t k = count; k != 0; k--, rec += 0x24)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0xC000000;
			F<uint32_t>(pk, 4) = *(const uint32_t *)rec;
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			F<uint32_t>(pk, 0x24) = uv;
			F<uint32_t>(pk, 0x30) = uv >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			uint32_t clip = 0;
			GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x14)) clip |= 0x20;
			if (OutY(pk + 0x20)) clip |= 0x40;
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			if (OutX(pk + 0x2C)) clip |= 8;
			if (OutY(pk + 0x2C)) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint32_t>(h, 0xC) != 0)
			{
				GteSetFarColor(F<uint8_t>(h, 8), F<uint8_t>(h, 9), F<uint8_t>(h, 0xA));
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				F<uint32_t>(pk, 0x10) = *(const uint32_t *)(rec + 0x18);
				F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 0x1C);
				F<uint32_t>(pk, 0x28) = *(const uint32_t *)(rec + 0x20);
			}
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
			uint8_t *quad = pk;
			uint8_t *mode = pk + 0x34;
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

	// ------------------------------------------------------------------
	// sub_6BE4E0: a prim model drawn as its textured quads only (lists 4 and 8 of the model's
	// primitive lists), additive
	// ------------------------------------------------------------------
	static uint32_t QuadsAdditive(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint8_t *model = F<uint8_t *>(h, 0);
		const uint8_t *p = model + *(const uint32_t *)model;
		F<const uint8_t *>(h, 0x20) = p;
		p = SkipList(p, 12);
		F<const uint8_t *>(h, 0x20) = p;
		p = SkipList(p, 12);
		F<const uint8_t *>(h, 0x20) = p;
		p = SkipList(p, 20);
		F<const uint8_t *>(h, 0x20) = p;
		cursor = FlatQuadsAdditive(h, ot, shift, cursor);
		p = F<uint8_t *>(h, 0x20);
		p = SkipList(p, 20);
		F<const uint8_t *>(h, 0x20) = p;
		p = SkipList(p, 24);
		F<const uint8_t *>(h, 0x20) = p;
		p = SkipList(p, 28);
		F<const uint8_t *>(h, 0x20) = p;
		return GouraudQuadsAdditive(h, ot, shift, cursor);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6BE1C0): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl BlindPart(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		if (r->flags & 0x800) MatrixMultiplyVector(&m, off, off);
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// anchor + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				// diagonal matrix product (sub_56C220)
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
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
		// (header +0x18, the depth offset, is not set: it keeps the scratch word)
		if (r->flags_lo == 0 || r->flags_lo == 3) PacketCursor() = QuadsAdditive(h, RenderOT(0x44), 3, PacketCursor());
		else PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 3, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag121_blind_held.h"
#endif

namespace ff8fx
{
namespace blind121
{
	// ------------------------------------------------------------------
	// Target (0x6BE080)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		// the screen flash: up over 22..25, down over 26..33 (the highest level of all targets)
		const int32_t e = t->counter - 0x16;
		if ((uint32_t)e < 4)
		{
			const int32_t v = e * 625;
			if (v > FlashLevel()) FlashLevel() = v;
		}
		else if ((uint32_t)e <= 0xC)
		{
			const int32_t v = (0xC - e) * 0x138;
			if (v > FlashLevel()) FlashLevel() = v;
		}
		if (t->counter < 0x14) GetEffectSpawnPosition(t->entity, 0xF0, 0, t->pos);
		PrimArg arg;
		memcpy(arg.pos, t->pos, 8);
		arg.morph = TexBase() + 0x18B4;
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(held_note_play(t, &arg);)
		const int alive = prim::play((prim::Layout *)t->layout, BlindPart, (int)&arg, 0);
		if (t->counter == 4)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D(SOUND_Blind, 0x101, pos);
		}
		if (t->counter == 0xD || t->counter == 0x12 || t->counter == 0x15) ScreenFadeTask(0, 1, 0, 0x60);
		if (t->counter == 0x20) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (alive == 0) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BDF20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1B34;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19B34;
			r->arena = 1;
		}
		FlashLevel() = 0;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x83C, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				if (t)
				{
					Memset32(&t->counter, 0, 0x20C);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					DecodeModelPrimLayout(MODEL_Blind, t->layout, 0x820);
					r->action++;
				}
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag121_blind()
	{
		register_port(blind121::ORIG_RootTask, (void *)blind121::RootTask, "B121 RootTask", 121);
		register_port(blind121::ORIG_TargetTask, (void *)blind121::TargetTask, "B121 TargetTask", 121);
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(register_mag121_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag121_blind_held.inc"
#endif
