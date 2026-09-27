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

// Effect 114: Zombie (spell, MAG_114_*).
//
// Structure (setup MAG_114_ZOMBIE 0x6C99C0 -> _Init 0x6C99F0, file loader 0x6C99D0 = the texture
// file named at 0x12C0238; the setup starts camera animation 0x12BD8D8 and queues the TIM):
//   RootTask (0x6C9A60) - alternates the packet arena (magic buffer + 0x1600 / + 0x19600); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target, unless any target task of 40 ticks or less still
//     runs (the cycle then restarts); runs the target queue, ends when it is empty.
//   Target (0x6C9BC0) - node 0x710, one per action: follows the target's effect anchor; plays the
//     prim-model layout 0x12BA30C with the callback MistPart (0x6C9D60) every tick, scaled by the
//     target's size (sound at 0); from 30 marks the target's entities (flags bit 3 on its +0x8C
//     ring); over 30..59 (unless the cast context has flag 0x200) redraws the target model as a
//     glowing overlay (0x6CA560): lifted by sin(t) * size, its vertices swayed sideways by a
//     wave table, additive flat polygons in a colour going from black to the target's tint;
//     damage at 54; ends at 60 (marks cleared).
//   MistPart (0x6C9D60) - one object of the layout: record flag 0x20000 selects the rotation order,
//     the block scale (always on here) scales the offsets and the matrix; flag 0x200 = the offset
//     stays in screen axes, else anchor + offset through the camera; flag 0x100 = scale by a
//     diagonal matrix product, else scale3DMatrix. Objects 2 and 3 are drawn by QuadsAdditive
//     (0x6CA0E0: only the model's gouraud textured quads, 0x6CA1B0, each after an additive
//     draw-mode primitive, depth offset clamped at 0), the others by MAG_070_sub_7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521818..0x252186C (texture file, magic buffer base, context, root pool,
// packet cursor 0x252183C, target / root queues, the zero vector 0x2521860 and the black colour
// 0x2521868 the overlay starts from: never written). The target pool (3 x 0x710), the morph
// vertex buffer (+0x1530) and the packet arenas are in the magic buffer. The overlay uses the
// battle's bone workspace (0x1D98B3C) as its vertex buffer and rebuilds the target's bone
// matrices (camera space for the draw, then back from the pose).

#include "mag_common.h"

namespace ff8fx
{
namespace zombie114
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x252181C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521820); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252183C); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521840; }         // pool: magic buffer, 3 x 0x710
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // overlay vertex buffer
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6C9A60;
	static const uint32_t ORIG_TargetTask = 0x6C9BC0;
	static const uint32_t MODEL_Mist = 0x12BA30C;        // prim-model layout data (0x6F4-byte layout)
	static const uint32_t ZERO_Vector = 0x2521860;       // module global, never written (0, 0, 0)
	static const uint32_t OVERLAY_Colour = 0x2521868;    // module global, never written (black)
	static const uint32_t WAVE_Table = 0x12BE238;        // s16 [4096]
	static const void *const SOUND_Zombie = (const void *)0x12BE234;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesC(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701530)(angles, out); }  // MAG_065_sub_701530
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	// sub_56CC00: out = colour blend of a (weight wa) and b (weight wb), weights 4.12
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	// software GTE
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
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
	struct TargetNode // pool of 3 nodes of 0x710 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		uint8_t mist[0x6F4]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z, height
		int32_t scale[3];  // +0x08 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x14 scale present (always 1)
		int32_t depth;     // +0x18 prim header +0x18
		uint32_t morph;    // +0x1C blended vertex frames
	};
	// the overlay's parameters (the arguments of 0x6CA560 after the entity)
	struct OverlayArg
	{
		int16_t lift[3];   // offset added to the entity matrix translation (0, -sin * size, 0)
		int32_t weight;    // sin: the target tint's weight in the colour (4.12)
		int32_t amp;       // sway amplitude (* 4)
		int32_t phase;     // wave table phase
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x710, "Zombie nodes");
	static_assert(sizeof(PrimArg) == 0x20, "Zombie prim block");

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// screen clip of a projected vertex (x 0..0xA00, y 0..0x6C0: the PC's 4x subpixel screen)
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	// ------------------------------------------------------------------
	// sub_6CA1B0: the prim model's gouraud textured quads (list at header +0x20, 0x24-byte
	// records), each after an additive draw-mode primitive (same bucket); depth = OTZ + header
	// +0x18, clamped at 0
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
			int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
			F<int32_t>(h, 0x2C) = z;
			if (z < 0) F<int32_t>(h, 0x2C) = 0;
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
	// MAG_114_sub_6CA0E0: a prim model drawn as its gouraud textured quads only (the 8th of the
	// model's primitive lists), additive
	// ------------------------------------------------------------------
	static uint32_t QuadsAdditive(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint8_t *model = F<uint8_t *>(h, 0);
		const uint8_t *p = model + *(const uint32_t *)model;
		F<const uint8_t *>(h, 0x20) = p;
		static const uint8_t sizes[7] = { 12, 12, 20, 24, 20, 24, 28 };
		for (int i = 0; i < 7; i++)
		{
			p = SkipList(p, sizes[i]);
			F<const uint8_t *>(h, 0x20) = p;
		}
		return GouraudQuadsAdditive(h, ot, shift, cursor);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6C9D60): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl MistPart(prim::Layout *l, prim::Record *r, int arg_)
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
		if (r->flags & 0x20000) RotationFromAnglesC(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (scaled by the block's scale; 4th word never written: only loaded
		// into the GTE V0 pad)
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
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
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
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
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
		*(int32_t *)(h + 0x18) = arg->depth;
		if (r->flags_lo == 2 || r->flags_lo == 3) PacketCursor() = QuadsAdditive(h, RenderOT(0x44), 2, PacketCursor());
		else PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// The overlay: the target model redrawn swayed, flat additive in one colour.
	// Work block (Field_Alloc 0x8C): +0x00 camera x entity matrix, then S = +0x20:
	//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 / +0x0A triangle / quad counts,
	//   +0x14 clip rectangle (x0, y0, x1, y1 / 8), +0x1C colour, +0x20 object mask,
	//   +0x24..+0x43 four vertices (sx, sy, otz, clip flags), +0x44 NCLIP, +0x4C OTZ,
	//   +0x5C entity origin on screen, +0x64 wave phase, +0x66 sway amplitude; +0x68 origin OTZ.
	// ------------------------------------------------------------------
	// sub_6CAAE0: one bone group of vertices, projected, swayed, clip-flagged
	static void SwayVertices(uint8_t **objp, uint8_t **outp, uint8_t *s)
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
				int32_t d = mul32(*(const int16_t *)(WAVE_Table + idx * 2), F<int16_t>(s, 0x66)) >> 12;
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
	static uint32_t SwayModel(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
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
				SwayVertices(&obj, &out, s);
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

	// MAG_114_sub_6CA560: the overlay (body +0x64, then the second model +0x78 if any)
	static void DrawOverlay(uint8_t *entity, const OverlayArg *a)
	{
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
		GteLoadV0((const void *)ZERO_Vector);
		GteRTPS();
		GteReadSXY2(h + 0x7C);
		GteReadOTZ(h + 0x88);
		if (F<int32_t>(h, 0x88) > 0)
		{
			ComputeBonesWorldMatrices(entity + 0x60, m);
			F<int16_t>(h, 0x34) = 0;
			F<int16_t>(h, 0x36) = 0;
			F<uint32_t>(h, 0x24) = BoneWorkspace();
			F<int16_t>(h, 0x38) = 0x140;
			F<int16_t>(h, 0x3A) = 0xD8;
			BlendColour((const void *)OVERLAY_Colour, entity + 0x28, a->weight, 0x1000 - a->weight, h + 0x3C);
			h[0x3F] = 0;
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(entity + 0x7C);
			PacketCursor() = SwayModel(*(const uint8_t *const *)(entity + 0x64), h + 0x20, RenderOT(0x44), 0, PacketCursor());
			BuildBoneMatricesFromPose(entity + 0x60);
			uint8_t *second = *(uint8_t *const *)(entity + 0x78);
			if (second)
			{
				F<uint32_t>(h, 0x40) = 0xFFFFFFFF;
				ComputeBonesWorldMatrices(second, m);
				PacketCursor() = SwayModel(*(const uint8_t *const *)(second + 4), h + 0x20, RenderOT(0x44), 0, PacketCursor());
				BuildBoneMatricesFromPose(second);
			}
		}
		FieldFree(0x8C);
	}

	// the overlay's parameters at step e (0..29) for a target of the given size word (>> 1 + 0x800)
	static void OverlayParams(int32_t e, int32_t reach, OverlayArg *a)
	{
		const uint32_t t = (uint32_t)(((uint64_t)((uint32_t)e << 12) * 0x88888889u) >> 32) >> 4; // e * 4096 / 30
		const int32_t sinv = ComputeSin((int32_t)t >> 2);
		a->lift[0] = 0;
		a->lift[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(sinv, reach)) >> 13);
		a->lift[2] = 0;
		a->weight = sinv;
		a->amp = (int32_t)(t * 2 + 0x200);
		a->phase = e * 8;
	}

	// entity +0x00 bit 3 on every entity of its +0x8C ring
	static void MarkTarget(uint8_t *entity)
	{
		uint8_t *e = entity;
		do
		{
			*e |= 8;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}
	static void UnmarkTarget(uint8_t *entity)
	{
		uint8_t *e = entity;
		do
		{
			*(uint16_t *)e &= 0xFFF7;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag114_zombie_held.h"
#endif

namespace ff8fx
{
namespace zombie114
{
	// ------------------------------------------------------------------
	// Target (0x6C9BC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter >= 0x1E) MarkTarget(t->entity);
		GetDefaultEffectPosition(t->entity, t->pos);
		PrimArg arg;
		memcpy(arg.pos, t->pos, 8);
		arg.scaled = 1;
		const int32_t reach = (*(const int16_t *)(t->entity + 0x26) >> 1) + 0x800;
		arg.scale[0] = reach;
		arg.scale[2] = reach;
		arg.scale[1] = reach;
		arg.depth = (int32_t)(0xFFFFF800u - (uint32_t)(int32_t)*(const int16_t *)(t->entity + 0x26)) >> 3;
		arg.morph = TexBase() + 0x1530;
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(held_note_play(t, &arg);)
		prim::play((prim::Layout *)t->mist, MistPart, (int)&arg, 0);
		const int32_t e = t->counter - 0x1E;
		if ((uint32_t)e < 0x1E && !(Ctx()->flags & 2))
		{
			OverlayArg o;
			OverlayParams(e, arg.scale[0], &o);
			// 30 fps layer: see mag114_zombie_held.inc
			FX_HELD(held_note_overlay(t, &o, e, arg.scale[0]);)
			DrawOverlay(t->entity, &o);
		}
		if (t->counter == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D(SOUND_Zombie, 0x101, pos);
		}
		if (t->counter == 0x36) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= 0x3C)
		{
			UnmarkTarget(t->entity);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6C9A60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1600;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19600;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x710, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
				// wait while any target task of 40 ticks or less runs
				TargetNode *pool = (TargetNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].counter <= 0x28)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					if (t)
					{
						Memset32(&t->counter, 0, 0x1C1);
						t->action = (int16_t)(uint16_t)r->action;
						t->entity = entity;
						DecodeModelPrimLayout(MODEL_Mist, t->mist, 0x6F4);
						r->action++;
					}
				}
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag114_zombie()
	{
		register_port(zombie114::ORIG_RootTask, (void *)zombie114::RootTask, "Z114 RootTask", 114);
		register_port(zombie114::ORIG_TargetTask, (void *)zombie114::TargetTask, "Z114 TargetTask", 114);
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(register_mag114_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag114_zombie_held.inc"
#endif
