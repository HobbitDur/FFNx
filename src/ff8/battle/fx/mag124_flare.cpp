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

// Effect 124: Flare (spell, MAG_124_*).
//
// Structure (setup MAG_124_FLARE 0x6BBDA0 -> _Init 0x6BBDD0, file loader 0x6BBDB0 = the texture
// file named at 0x12652A4; the setup starts camera animation 0x125D0C0 and queues the TIM):
//   RootTask (0x6BBE40) - alternates the packet arena (magic buffer + 0x30BC / + 0x1B0BC), clears
//     the screen flash level, and on counter 1 of a 10-tick cycle sets up the pools (first time)
//     and spawns the target task of the next action, unless a target task younger than 61 ticks
//     still runs on that target (the cycle then restarts); computes the effect camera, runs the
//     target and particle queues, sets the screen flash; ends when both queues are empty.
//   Target (0x6BC040) - node 0xB44, one per action (on its first target): sound at 0; screen
//     flash ramp (up over 0..7, 0x800, down from 77); fades at 39 and 41; the target's colour
//     blends to red over 4..35 and back over 72..80; plays two prim-model layouts (the burst
//     0x1252C0C at the target, the ground ring 0x125C5C4 at its feet) with the prim-player
//     callback PrimPart (0x6BC4A0); over 40..79 hides the target and draws it itself as a
//     rippling red silhouette (0x6BC8A0); damage at 80; at 0..7 spawns 6 particles per tick; ends
//     when both layouts are over (the target is shown again).
//   Particle (0x6BD020) - node 0x2C: the shared burst sprite (0x1252BE4) on a sphere around the
//     target (radius `size` along a direction turning by a random angular velocity): it shows at
//     its spawn radius, collapses into the centre at a random time, bursts out again from tick 41
//     and fades out from tick 56.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25213D0..0x2521448 (flash level, texture file, magic buffer base, context,
// root pool, packet cursor 0x25213F4, effect camera 0x25213F8, particle / target / root queues).
// The target pool (3 x 0xB44), the particle pool (64 x 0x2C), the morph vertex buffer (+0x2CCC)
// and the packet arenas are in the magic buffer. The silhouette draws into the frame arena
// (battle_texture_data_ptr 0x1D8E054) with the battle's bone workspace (0x1D98B3C) as its
// vertex buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace flare124
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &FlashLevel() { return var<int32_t>(0x25213D0); }       // screen flash, max of the targets
	inline uint32_t &TexBase() { return var<uint32_t>(0x25213D8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25213DC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25213F4); }
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x25213F8); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521418; }       // pool: + 0x21CC, 0x40 x 0x2C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521428; }         // pool: magic buffer, 3 x 0xB44
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // silhouette vertex buffer
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6BBE40;
	static const uint32_t ORIG_TargetTask = 0x6BC040;
	static const uint32_t ORIG_ParticleTask = 0x6BD020;
	static const uint32_t MODEL_Burst = 0x1252C0C;       // prim-model layout data (0xA70-byte layout)
	static const uint32_t MODEL_Ring = 0x125C5C4;        // prim-model layout data (0xA8-byte layout)
	static const uint32_t SEQ_Particle = 0x1252BE4;
	static const void *const SOUND_Flare = (const void *)0x125DE9C;
	static const void *const COLOUR_Red = (const void *)0x12652A0;   // 0x002020FF
	static const uint32_t RIPPLE_Distance = 0x125DEA0;   // u8 [128][128]: distance of (|dy|, |dx|)
	static const uint32_t RIPPLE_Weights = 0x1261EA0;    // u16 [20][128]: weight by distance, per step
	static const uint32_t SIN_Table = 0x12632A0;         // s16 [4096]

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	// sub_56CC00: out = colour blend of a (weight wa) and b (weight wb), weights 4.12
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	// sub_5088A0: an entity's ground shadow (returns the packet cursor)
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline void MatrixMultiplyVector(const void *m, const void *in, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C4F0)(m, in, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	// software GTE
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0xB44 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		int16_t spawn[4];  // +0x1C effect anchor 0xF0 (GetEffectSpawnPosition; not read)
		uint32_t colour;   // +0x24 the target's colour before the effect
		int32_t radius;    // +0x28 silhouette: largest ripple offset of the last draw (at most 0x80)
		uint8_t burst[0xA70];  // +0x2C prim-model layout (prim::Layout: data, frame, state)
		uint8_t ring[0xA8];    // +0xA9C
	};
	struct ParticleNode // pool of 0x40 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t size;      // +0x0C radius around the anchor
		int16_t grow;      // +0x0E spawn radius, then the burst step
		int16_t start;     // +0x10 tick the collapse starts
		int16_t pos[3];    // +0x12 anchor x, y, z
		int16_t shade;     // +0x18 sprite size (0x4B0)
		int16_t ang[3];    // +0x1A direction angles
		int16_t alpha;     // +0x20 brightness (sprite colour)
		int16_t vel[3];    // +0x22 angular velocity
		int16_t age;       // +0x28
		int16_t pad2A;
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 x, y, z (+ height word)
		int32_t scale[3];  // +0x08 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x14 scale present
		int32_t depth;     // +0x18 prim header +0x18 (/ 4 for objects 2 and 3)
		uint32_t morph;    // +0x1C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0xB44 && sizeof(ParticleNode) == 0x2C, "Flare nodes");
	static_assert(sizeof(PrimArg) == 0x20, "Flare prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6BC4A0): one object of the burst / ring model
	// ------------------------------------------------------------------
	static void __cdecl PrimPart(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		int16_t f0 = r->b0, f1 = r->b1;
		int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			int16_t t = r->b;
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset, scaled by the block's scale
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
		const uint32_t flags = r->flags;
		if (flags & 0x1000)
		{
			// the block position through the camera, the offset through the effect camera
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&EffectCamera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t mac[3];
			GteReadMAC123(mac);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)mac[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)mac[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)mac[2]);
			MatrixMultiply(&EffectCamera(), &m);
		}
		else if (flags & 0x200)
		{
			// the block position through the camera, the offset added unrotated, rotation kept
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
			// block position + offset through the camera, rotation composed with the camera
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
			if (flags & 0x100)
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
				int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
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
		int32_t depth = arg->depth;
		*(int32_t *)(h + 0x18) = depth;
		if (depth != 0 && ((int16_t)r->flags_lo == 2 || (int16_t)r->flags_lo == 3))
			*(int32_t *)(h + 0x18) = depth >> 2;
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Silhouette (0x6BC8A0): the target model redrawn flat in its colour, its vertices rippled on
	// screen around the anchor
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0x84): +0x00 camera x entity matrix, then S = +0x20:
	//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 triangle / quad counts (6 words),
	//   +0x14 clip rectangle (x0, y0, x1, y1), +0x1C colour, +0x20 object mask,
	//   +0x24..+0x43 four vertices (sx, sy, otz, clip flags), +0x44 NCLIP, +0x4C OTZ,
	//   +0x54 weight row, +0x58 radius, +0x5A / +0x5C anchor on screen, +0x5E largest offset,
	//   +0x60 ripple phase, +0x62 ripple amplitude.
	template<typename T> static inline T &F(uint8_t *s, uint32_t off) { return *(T *)(s + off); }

	// sub_6BCE40: one bone group of vertices, projected, rippled, clip-flagged
	static void RippleVertices(uint8_t **objp, uint8_t **outp, uint8_t *s)
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
				const uint32_t d = *(const uint8_t *)(RIPPLE_Distance + (int32_t)(int16_t)adx + shl32((int16_t)ady, 7));
				if (d != 0)
				{
					const int32_t radius = F<int16_t>(s, 0x58);
					if ((int32_t)d < radius)
					{
						const int32_t q = (int32_t)(d << 7) / radius;
						const uint32_t w = *(const uint16_t *)(F<uint32_t>(s, 0x54) + q * 2);
						const uint32_t ph = ((uint32_t)F<uint16_t>(s, 0x60) + (uint32_t)shl32(ey, 6)) & 0xFFF;
						const int32_t ox = mul32(ex, (int32_t)w) >> 12;
						int32_t wob = mul32(*(const int16_t *)(SIN_Table + ph * 2), F<int16_t>(s, 0x62)) >> 12;
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

	static inline void CopyVertex(uint8_t *s, uint32_t off, const uint8_t *verts, uint16_t index)
	{
		const uint8_t *v = verts + (uint32_t)(index & 0xFFF) * 8;
		F<uint32_t>(s, off) = *(const uint32_t *)v;
		F<uint32_t>(s, off + 4) = *(const uint32_t *)(v + 4);
	}

	// sub_6BCB50: the object's textured triangles and quads, flat in the colour
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
	static uint32_t RippleModel(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
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
					RippleVertices(&obj, &out, s);
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

	// sub_6BC8A0: shadow, then the body (+0x64) and the weapon (+0x78) of the target; returns the
	// largest ripple offset (the next draw's radius)
	static int32_t DrawSilhouette(uint8_t *entity, int16_t sx, int16_t sy, int16_t radius, int32_t step, int32_t amplitude, int32_t phase)
	{
		uint8_t *c = (uint8_t *)FieldAlloc(0x84);
		uint8_t *s = c + 0x20;
		F<int16_t>(c, 0x82) = (int16_t)(amplitude >> 2);
		F<int16_t>(c, 0x7A) = sx;
		F<int16_t>(c, 0x7C) = sy;
		F<int16_t>(c, 0x80) = (int16_t)phase;
		F<int16_t>(c, 0x78) = radius;
		F<uint32_t>(c, 0x74) = RIPPLE_Weights + (uint32_t)shl32(step, 8);
		F<int16_t>(c, 0x7E) = 0;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)c);
		FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
		ComputeBonesWorldMatrices(entity + 0x60, c);
		F<int16_t>(c, 0x34) = 0;
		F<uint32_t>(c, 0x24) = BoneWorkspace();
		F<int16_t>(c, 0x36) = 0;
		F<int16_t>(c, 0x38) = 0x140;
		F<int16_t>(c, 0x3A) = 0xD8;
		F<uint32_t>(c, 0x3C) = *(const uint32_t *)(entity + 0x28);
		F<uint32_t>(c, 0x40) = *(const uint32_t *)(entity + 0x7C);
		FrameCursor() = RippleModel(*(const uint8_t *const *)(entity + 0x64), s, RenderOT(0x44), 0, FrameCursor());
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *weapon = *(uint8_t *const *)(entity + 0x78);
		if (weapon)
		{
			F<uint32_t>(c, 0x40) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(weapon, c);
			FrameCursor() = RippleModel(*(const uint8_t *const *)(weapon + 4), s, RenderOT(0x44), 0, FrameCursor());
			BuildBoneMatricesFromPose(weapon);
		}
		FieldFree(0x84);
		return F<int16_t>(c, 0x7E);
	}

	// entity +0x00 bit 2 (hidden: the effect draws it) and bit 3 on every entity of its +0x8C ring
	static void HideTarget(uint8_t *entity)
	{
		entity[0] |= 4;
		uint8_t *e = entity;
		do
		{
			e[0] |= 8;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}

	static void ShowTarget(uint8_t *entity)
	{
		*(uint16_t *)entity &= 0xFFFB;
		uint8_t *e = entity;
		do
		{
			*(uint16_t *)e &= 0xFFF7;
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}

	// ------------------------------------------------------------------
	// Particle (0x6BD020)
	// ------------------------------------------------------------------
	// the tick's size / brightness steps before the draw: returns the sprite mode (4 = coloured by
	// alpha, 0 = plain), -1 = nothing drawn, -2 = finished
	static int ParticlePrepare(ParticleNode *p)
	{
		const int32_t age = p->age;
		const uint32_t k = (uint32_t)(age - p->start);
		if (k <= 8)
			p->size = (int16_t)((uint32_t)mul32(p->grow, (int32_t)(8 - k)) >> 3);
		if (age >= 0x29)
		{
			const int16_t g = p->grow;
			const int16_t q = (int16_t)(g >> 2);
			p->size = (int16_t)(p->size + q);
			if (g > 0x200) p->grow = (int16_t)(g - q);
			else p->grow = 0x200;
			if (age >= 0x38)
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

	static void ParticleDraw(const ParticleNode *p, int mode)
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
		*(uint32_t *)h = SEQ_Particle;
		*(uint16_t *)(h + 4) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void ParticleMove(ParticleNode *p)
	{
		p->ang[0] = (int16_t)(p->ang[0] + p->vel[0]);
		p->ang[1] = (int16_t)(p->ang[1] + p->vel[1]);
		p->ang[2] = (int16_t)(p->ang[2] + p->vel[2]);
		p->age++;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag124_flare_held.h"
#endif

namespace ff8fx
{
namespace flare124
{
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		const int mode = ParticlePrepare(p);
		if (mode == -2) return TASK_END;
		if (mode >= 0)
		{
			// 30 fps layer: see mag124_flare_held.inc
			FX_HELD(held_note_particle(p, mode);)
			ParticleDraw(p, mode);
		}
		ParticleMove(p);
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6BC040)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter == 0) BdPlaySE3D(SOUND_Flare, 1, t->pos);
		int16_t c = t->counter;
		if (c < 8)
		{
			const int32_t v = shl32(c, 8);
			if (v > FlashLevel()) FlashLevel() = v;
		}
		else if (c > 0x4C)
		{
			const int32_t v = shl32(0x50 - c, 9);
			if (v > FlashLevel()) FlashLevel() = v;
		}
		else if (FlashLevel() < 0x800) FlashLevel() = 0x800;
		c = t->counter;
		if (c == 0x27) ScreenFadeTask(0, 1, 0, 0x60);
		else if (c == 0x29)
		{
			FlashLevel() = 0x400;
			ScreenFadeTask(0, 1, 1, 0xC0);
		}
		*(uint16_t *)t->entity |= 0x800;
		// the target's colour: to red over 4..35, red, back over 72..80
		const int32_t cc = t->counter;
		if ((uint32_t)(cc - 4) < 0x20)
		{
			const int32_t w = (cc - 4) << 7;
			BlendColour(COLOUR_Red, &t->colour, w, 0x1000 - w, t->entity + 0x28);
		}
		else if ((uint32_t)(cc - 0x48) <= 8)
		{
			const int32_t w = (cc - 0x48) << 9;
			BlendColour(COLOUR_Red, &t->colour, 0x1000 - w, w, t->entity + 0x28);
		}
		else if (cc >= 4 && cc <= 0x48) *(uint32_t *)(t->entity + 0x28) = *(const uint32_t *)COLOUR_Red;
		else *(uint32_t *)(t->entity + 0x28) = t->colour;
		// burst at the anchor (scaled by the target's size), ring at its feet
		PrimArg arg;
		arg.morph = TexBase() + 0x2CCC;
		const int32_t size = *(const int16_t *)(t->entity + 0x26);
		arg.scaled = 1;
		const int32_t scale = (size >> 1) + 0x800;
		*(uint32_t *)&arg.pos[2] = *(const uint32_t *)&t->pos[2];
		arg.scale[2] = scale;
		arg.scale[1] = scale;
		arg.scale[0] = scale;
		*(uint32_t *)&arg.pos[0] = *(const uint32_t *)&t->pos[0];
		arg.depth = (int32_t)(0xFFFFF800u - (uint32_t)size) >> 3;
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_play(t, t->burst, &arg);)
		int alive = prim::play((prim::Layout *)t->burst, PrimPart, (int)&arg, 0);
		arg.pos[2] = t->pos[2];
		arg.pos[0] = t->pos[0];
		arg.pos[1] = *(const int16_t *)(t->entity + 0x24);
		arg.depth = 0;
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_play(t, t->ring, &arg);)
		alive |= prim::play((prim::Layout *)t->ring, PrimPart, (int)&arg, 0);
		// silhouette over 40..79 while the anchor is in front of the camera
		const int32_t e = (int32_t)t->counter - 0x28;
		if (e == 0x28) ShowTarget(t->entity);
		else if ((uint32_t)e < 0x28)
		{
			// 30 fps layer: see mag124_flare_held.inc
			FX_HELD(held_note_silhouette(t, e);)
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			GteLoadV0(t->pos);
			GteRTPS();
			int16_t sxy[2];
			uint32_t otz;
			GteReadSXY2(sxy);
			GteReadOTZ(&otz);
			sxy[0] = (int16_t)(sxy[0] >> 3);
			sxy[1] = (int16_t)(sxy[1] >> 3);
			if ((uint16_t)otz != 0)
			{
				// ripple step 0..19..0, amplitude sin(0..180 degrees) over the 40 ticks
				uint32_t step = (uint32_t)(e * 20) / 20u;
				if ((int32_t)step >= 0x14) step = 0x27 - step;
				const int32_t amplitude = ComputeSin((int32_t)((uint32_t)(e << 11) / 40u));
				t->radius = DrawSilhouette(t->entity, sxy[0], sxy[1], (int16_t)t->radius, (int32_t)step, amplitude, e << 9);
				HideTarget(t->entity);
			}
			else ShowTarget(t->entity);
		}
		if (t->counter == 0x50) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		const int32_t cnt = t->counter;
		if ((uint32_t)cnt < 8)
		{
			int left = 6;
			ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			while (p)
			{
				Memset32(&p->size, 0, 8);
				const int32_t r = CrtRand();
				p->start = (int16_t)((mul32(r, 20) >> 15) - cnt + 4);
				const int32_t s = arg.scale[0];
				const int16_t s0 = (int16_t)(s - (s >> 2));
				p->size = s0;
				p->grow = s0;
				*(uint32_t *)&p->pos[0] = *(const uint32_t *)&t->pos[0];
				*(uint32_t *)&p->pos[2] = *(const uint32_t *)&t->pos[2];
				p->shade = 0x4B0;
				int16_t ang[4] = {};
				ang[0] = (int16_t)CrtRand();
				ang[1] = (int16_t)CrtRand();
				ang[2] = (int16_t)CrtRand();
				Mat4x3 m = {};
				BuildRotationMatrixFromAngles(ang, &m);
				int16_t v[4] = { 0, 0, 0x32, 0 };
				MatrixMultiplyVector(&m, v, p->vel);
				p->ang[0] = (int16_t)CrtRand();
				p->ang[1] = (int16_t)CrtRand();
				p->ang[2] = (int16_t)CrtRand();
				if (--left == 0) break;
				p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			}
		}
		if (!alive)
		{
			*(uint16_t *)t->entity &= 0xF7FF;
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BBE40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x30BC;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1B0BC;
			r->arena = 1;
		}
		FlashLevel() = 0;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0xB44, 3);
				InitTaskQueuePool(QParticles(), (void *)(TexBase() + 0x21CC), 0x2C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				uint32_t colour = *(const uint32_t *)(entity + 0x28);
				// wait while a target task of 60 ticks or less runs on this target; an older one
				// hands over the colour the target had before it
				TargetNode *pool = (TargetNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity)
					{
						if (pool[k].counter <= 0x3C)
						{
							busy = true;
							break;
						}
						colour = pool[k].colour;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					Memset32(&t->counter, 0, 0x2CE);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					t->colour = colour;
					GetDefaultEffectPosition(entity, t->pos);
					GetEffectSpawnPosition(t->entity, 0xF0, 0, t->spawn);
					DecodeModelPrimLayout(MODEL_Burst, t->burst, 0xA70);
					DecodeModelPrimLayout(MODEL_Ring, t->ring, 0xA8);
					r->action++;
				}
			}
		}
		EffectCameraMatrix(&Camera(), &EffectCamera());
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QParticles());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag124_flare()
	{
		register_port(flare124::ORIG_RootTask, (void *)flare124::RootTask, "F124 RootTask", 124);
		register_port(flare124::ORIG_TargetTask, (void *)flare124::TargetTask, "F124 TargetTask", 124);
		register_port(flare124::ORIG_ParticleTask, (void *)flare124::ParticleTask, "F124 ParticleTask", 124);
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(register_mag124_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag124_flare_held.inc"
#endif
