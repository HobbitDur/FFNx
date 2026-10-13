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

// Effect 113: Bad Breath (enemy attack 17, Geezard c0m004; MAG_113_*). No other kernel.bin entry
// uses effect 113 (Malboro's Bad Breath is effect 131, its own module).
//
// Structure (setup MAG_113_BAD_BREATH 0x6CAC20 -> _Init 0x6CAC50, file loader 0x6CAC30 = the
// texture file named at 0x12C360C; the setup starts camera animation 0x12C1590 and queues the TIM):
//   RootTask (0x6CACD0) - alternates the packet arena (magic buffer + 0x14C8 / + 0x114C8); on
//     counter 1 sets up the pools and starts the main task (it keeps the low byte of the target
//     mask); runs the main, particle and target queues, ends when all three are empty.
//   Main (0x6CAE10) - node 0x104, the breath from the caster's mouth (effect bone 9): at 0 the
//     party targets' average anchor (+ 0x7D0 in z) and the sound; over 0..44 one puff per tick
//     towards a swinging point (sin of a phase, x) / the average anchor (y, z), decelerating, plus
//     two spray puffs per tick on a cone around the caster's facing (0x6CBB40); at 10 / 16 / 26
//     the target task of party slot 0 / 1 / 2 if that mask bit is set; over 44..59 a puff falling
//     around the mouth; ends at 59.
//   Particle (0x6CB260) - node 0x2C: a sprite sequence (frame = its counter) at its position
//     (camera-facing, scale, roll), then velocity += acceleration, position += velocity.
//   Target (0x6CB340) - node 0xFC, one per target: over 0..10 a rising cloud puff around the
//     target's anchor per tick; over 10..37 the target redrawn swayed (0x6CB560, its own model
//     hidden: entity +0x00 bit 2) and the prim-model layout 0x12C0244 with the callback
//     PartCallback (0x6CB910) at its effect bone 0xF1 (at the target's height); the damage at 27;
//     ends after 38.
// Direct calls into other modules: sub_6BCB50 (Flare's flat polygons, ported here).
// The tasks write no entity position and no camera words (the camera animation is the engine's).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521870..0x25218E0 (caster entity, texture file, magic buffer base, context,
// root pool, packet cursor 0x2521894, target / particle / main / root queues, a zero vector at
// 0x25218D8 never written). The target pool (3 x 0xFC), the particle pool (0x50 x 0x2C, + 0x2F4),
// the main pool (+ 0x10B4), the morph vertex buffer (+ 0x11B8) and the packet arenas are in the
// magic buffer. The overlay draws into the frame arena (battle_texture_data_ptr 0x1D8E054) with
// the battle's bone workspace as its vertex buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace badbreath113
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2521870); }         // the caster's entity
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521878); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x252187C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521894); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521898; }         // pool: magic buffer, 3 x 0xFC
	inline TaskQueue *QParticles() { return (TaskQueue *)0x25218A8; }       // pool: + 0x2F4, 0x50 x 0x2C
	inline TaskQueue *QMain() { return (TaskQueue *)0x25218B8; }            // pool: + 0x10B4, 1 x 0x104
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // overlay vertex buffer
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6CACD0;
	static const uint32_t ORIG_MainTask = 0x6CAE10;
	static const uint32_t ORIG_ParticleTask = 0x6CB260;
	static const uint32_t ORIG_TargetTask = 0x6CB340;
	static const uint32_t MODEL_Cloud = 0x12C0244;       // prim-model layout data (0xEC-byte layout)
	static const uint32_t PART_Flags = 0x12C3604;        // per-object flags of the layout
	static const uint32_t SEQ_Stream = 0x12C118C;        // the breath puffs (20 frames)
	static const uint32_t SEQ_Spray = 0x12C1418;         // spray / falling puffs (14 frames)
	static const uint32_t SEQ_Rise = 0x12C1014;          // the target's cloud puffs (14 frames)
	static const uint32_t ZERO_Vector = 0x25218D8;       // module global, never written (0, 0, 0)
	static const uint32_t WAVE_Table = 0x12C1604;        // s16 [4096]
	static const void *const SOUND_BadBreath = (const void *)0x12C1600;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// sub_5088A0: the entity's ground shadow
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// software GTE
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
		int16_t counter;   // +0x0C the main task starts at 1
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x104 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..59
		uint8_t mask;      // +0x0E target mask (party slots 0..2), shifted at each target step
		uint8_t slot;      // +0x0F party slot of the next target step
		int16_t pos[3];    // +0x10 the targets' average anchor (+ 0x7D0 in z)
		int16_t phase;     // +0x16 swing phase (0xF000, + 0x174 per puff)
		uint8_t pad18[0xEC];
	};
	struct ParticleNode // pool of 0x50 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame
		int16_t end;       // +0x0E frames
		uint32_t seq;      // +0x10 sprite sequence data
		int16_t pos[3];    // +0x14
		int16_t scale;     // +0x1A
		int16_t vel[3];    // +0x1C
		int16_t rot;       // +0x22 roll (draw: -rot >> 2)
		int16_t acc[3];    // +0x24
		int16_t pad2A;
	};
	struct TargetNode // pool of 3 nodes of 0xFC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..38
		int16_t slot;      // +0x0E party slot of the target
		uint8_t cloud[0xEC]; // +0x10 prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 effect bone 0xF1 x, (entity height), z; +0x06 never written (not read)
		uint32_t flags;    // +0x08 per-object flags table
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x104 && sizeof(ParticleNode) == 0x2C && sizeof(TargetNode) == 0xFC, "Bad Breath nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Bad Breath prim block");

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// ------------------------------------------------------------------
	// Prim-player callback (0x6CB910, a copy of Berserk's 0x6D03D0): one object of the cloud layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		// the matrix's translation is the record offset + the block position (int32)
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		const uint32_t *flags = (const uint32_t *)arg->flags;
		if (flags[(int16_t)r->index] & 0x10000000)
		{
			// camera-aligned: only the position goes through the camera
			TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		}
		else ComposeAffineTransform(&Camera(), &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t mode = (flags[(int16_t)r->index] & 0xDFFF) | 0x2000;
		*(uint32_t *)(h + 0x1C) = mode;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = mode | 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// The overlay: the target model redrawn swayed, flat in its tint (as Zombie's 0x6CA560, without
	// the lift and the colour blend, with the entity's shadow and Flare's flat polygons).
	// Work block (Field_Alloc 0x8C): +0x00 camera x entity matrix, then S = +0x20:
	//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 / +0x0A triangle / quad counts,
	//   +0x14 clip rectangle (x0, y0, x1, y1 / 8), +0x1C colour, +0x20 object mask,
	//   +0x24..+0x43 four vertices (sx, sy, otz, clip flags), +0x44 NCLIP, +0x4C OTZ,
	//   +0x5C entity origin on screen, +0x64 wave phase, +0x66 sway amplitude; +0x68 origin OTZ.
	// ------------------------------------------------------------------
	// sub_6CB7D0 (a copy of Zombie's 0x6CAAE0, own wave table): one bone group of vertices,
	// projected, swayed, clip-flagged
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

	// sub_6BCB50 (in the Flare module, shared): the object's textured triangles and quads, flat in
	// the colour
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
						*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | F<uint32_t>(s, 0x1C);
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
						*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xA) & 0x200) | 0x2C00) << 16) | F<uint32_t>(s, 0x1C);
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

	// sub_6CB6B0 (a copy of Zombie's 0x6CA6E0, polygons through Flare's 0x6BCB50): every object of
	// a model the mask shows (bone groups of vertices, then polygons)
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

	// sub_6CB560: the overlay (shadow, body +0x64, then the second model +0x78 if any), sway
	// amplitude = sin >> 2, wave phase = step << 8
	static void DrawOverlay(uint8_t *entity, int32_t sinv, int32_t phase)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x8C);
		F<int16_t>(h, 0x86) = (int16_t)(sinv >> 2);
		F<int16_t>(h, 0x84) = (int16_t)phase;
		Mat4x3 *m = (Mat4x3 *)h;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		GteLoadV0((const void *)ZERO_Vector);
		GteRTPS();
		GteReadSXY2(h + 0x7C);
		GteReadOTZ(h + 0x88);
		if (F<int32_t>(h, 0x88) > 0)
		{
			FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
			ComputeBonesWorldMatrices(entity + 0x60, m);
			F<int16_t>(h, 0x38) = 0x140;
			F<uint32_t>(h, 0x24) = BoneWorkspace();
			F<int16_t>(h, 0x34) = 0;
			F<int16_t>(h, 0x36) = 0;
			F<int16_t>(h, 0x3A) = 0xD8;
			F<uint32_t>(h, 0x3C) = *(const uint32_t *)(entity + 0x28);
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(entity + 0x7C);
			FrameCursor() = SwayModel(*(const uint8_t *const *)(entity + 0x64), h + 0x20, RenderOT(0x44), 0, FrameCursor());
			BuildBoneMatricesFromPose(entity + 0x60);
			uint8_t *second = *(uint8_t *const *)(entity + 0x78);
			if (second)
			{
				F<uint32_t>(h, 0x40) = 0xFFFFFFFF;
				ComputeBonesWorldMatrices(second, m);
				FrameCursor() = SwayModel(*(const uint8_t *const *)(second + 4), h + 0x20, RenderOT(0x44), 0, FrameCursor());
				BuildBoneMatricesFromPose(second);
			}
		}
		FieldFree(0x8C);
	}

	// the overlay's sway amplitude at step e (0..27): sin(e * 2048 / 28)
	static int32_t OverlaySin(int32_t e)
	{
		const uint32_t x = (uint32_t)e << 11;
		const uint32_t hi = (uint32_t)(((uint64_t)x * 0x24924925u) >> 32);
		return ComputeSin((int32_t)((((x - hi) >> 1) + hi) >> 4));
	}

	// sub_6CBB40: unit direction (4.12) of the angles a[0] (pitch), a[1] (yaw), a[2]
	static void AngleDir(const int16_t *a, int16_t *out)
	{
		const int32_t c0 = ComputeCos(a[0]);
		const int32_t s0 = -ComputeSin(a[0]);
		const int32_t s2 = ComputeSin(a[2]);
		const int32_t t = (int32_t)(0u - (uint32_t)mul32(s2, s0)) >> 12;
		const int32_t c2 = ComputeCos(a[2]);
		out[1] = (int16_t)(mul32(c2, s0) >> 12);
		const int32_t s1 = -ComputeSin(a[1]);
		const int32_t c1 = -ComputeCos(a[1]);
		const int32_t x = (int32_t)((uint32_t)mul32(c1, t) + (uint32_t)mul32(s1, c0));
		const int32_t z = (int32_t)((uint32_t)mul32(c1, c0) - (uint32_t)mul32(s1, t));
		out[0] = (int16_t)(x >> 12);
		out[2] = (int16_t)(z >> 12);
	}

	// sub_6CB4F0 (as Junk's 0x6E87A0): the damage of every target record of action 0 that is this entity
	static void DamageEntity(uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)t[0] == slot) ApplyActionResultToTarget(t);
		}
	}

	// the particle's draw (0x6CB260 up to its counter step): the breath puff faces the camera
	// unrolled over its first 4 frames
	static void ParticleDraw(const ParticleNode *p)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (p->seq == SEQ_Stream && p->counter < 4) TransformCameraByShadowRotation(p->pos, p->scale, 0);
		else TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->rot >> 2));
		F<uint32_t>(h, 0) = p->seq;
		F<int16_t>(h, 4) = p->counter;
		F<int16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the particle's step after its draw: velocity += acceleration, position += velocity
	static void ParticleStep(ParticleNode *p)
	{
		p->vel[1] = (int16_t)(p->vel[1] + p->acc[1]);
		p->vel[0] = (int16_t)(p->vel[0] + p->acc[0]);
		p->vel[2] = (int16_t)(p->vel[2] + p->acc[2]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
	}

	static ParticleNode *NewParticle()
	{
		ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
		if (p) Memset32(&p->counter, 0, 8);
		return p;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag113_bad_breath_held.h"
#endif

namespace ff8fx
{
namespace badbreath113
{
	// ------------------------------------------------------------------
	// Particle (0x6CB260)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag113_bad_breath_held.inc
		FX_HELD(held_note_particle(p);)
		ParticleDraw(p);
		p->counter++;
		if (p->counter >= p->end) return TASK_END;
		ParticleStep(p);
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6CB340)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		uint8_t *entity = Entity(t->slot);
		if ((uint16_t)t->counter <= 0xA)
		{
			ParticleNode *p = NewParticle();
			if (p)
			{
				GetDefaultEffectPosition(entity, p->pos);
				p->vel[0] = (int16_t)((CrtRand() & 0x40) - 0x20);
				p->vel[2] = (int16_t)((CrtRand() & 0x40) - 0x20);
				const int32_t r = CrtRand();
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] << 2));
				p->vel[1] = (int16_t)(-0x20 - (r & 0x1F));
				const int32_t r2 = CrtRand();
				p->seq = SEQ_Rise;
				// y = rand * y, signed division by the magic 0x57619F1 (>> 41)
				const int32_t prod = mul32(r2, p->pos[1]);
				int32_t q = (int32_t)(((int64_t)prod * 0x57619F1) >> 32) >> 9;
				q += (int32_t)((uint32_t)q >> 31);
				p->end = 0xE;
				p->acc[2] = 0;
				p->pos[1] = (int16_t)q;
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] << 2));
				p->acc[1] = 0;
				p->acc[0] = 0;
				const int16_t s = (int16_t)((CrtRand() >> 4) + 0x800);
				p->scale = s;
				p->rot = s;
			}
		}
		const int32_t e = t->counter - 0xA;
		if ((uint32_t)e < 0x1C)
		{
			const int32_t sinv = OverlaySin(e);
			// 30 fps layer: see mag113_bad_breath_held.inc
			FX_HELD(held_note_overlay(entity, e);)
			DrawOverlay(entity, sinv, e << 8);
			*entity |= 4;
			PrimArg arg;
			GetEffectSpawnPosition(entity, 0xF1, 0, arg.pos);
			arg.pos[1] = *(const int16_t *)(entity + 0x24);
			arg.flags = PART_Flags;
			arg.morph = TexBase() + 0x11B8;
			// 30 fps layer: see mag113_bad_breath_held.inc
			FX_HELD(held_note_play(t, &arg);)
			prim::play((prim::Layout *)t->cloud, PartCallback, (int)&arg, 0);
		}
		else *(uint16_t *)entity &= 0xFFFB;
		if (t->counter == 0x1B) DamageEntity(entity);
		t->counter++;
		return t->counter <= 0x26 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x6CAE10)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		// the caster's mouth (effect bone 9); the block's 4th word is never written (the particle
		// nodes' +0x1A it would reach is set below)
		int16_t sp[3];
		GetEffectSpawnPosition(Caster(), 9, 0, sp);
		const int32_t c = m->counter;
		if ((uint32_t)c <= 0x2C)
		{
			if (c == 0)
			{
				uint32_t mask = *(const uint16_t *)&Ctx()->target_mask;
				int32_t count = 0;
				m->pos[2] = 0;
				m->pos[1] = 0;
				m->pos[0] = 0;
				for (int k = 0; k < 3; k++)
				{
					if (mask & 1)
					{
						int16_t a[4];
						GetDefaultEffectPosition(Entity(k), a);
						m->pos[0] = (int16_t)(m->pos[0] + a[0]);
						m->pos[1] = (int16_t)(m->pos[1] + a[1]);
						m->pos[2] = (int16_t)(m->pos[2] + a[2]);
						count++;
					}
					mask >>= 1;
				}
				m->phase = (int16_t)0xF000;
				m->pos[0] = (int16_t)(m->pos[0] / count);
				m->pos[1] = (int16_t)(m->pos[1] / count);
				m->pos[2] = (int16_t)(m->pos[2] / count + 0x7D0);
			}
			else if (c == 0xA || c == 0x10 || c == 0x1A)
			{
				if (m->mask & 1)
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					if (t)
					{
						Memset32(&t->counter, 0, 0x3C);
						DecodeModelPrimLayout(MODEL_Cloud, t->cloud, 0xEC);
						t->slot = (int16_t)(uint16_t)m->slot;
					}
				}
				m->mask = (uint8_t)(m->mask >> 1);
				m->slot++;
			}
			// the breath puff
			ParticleNode *p = NewParticle();
			if (p)
			{
				memcpy(p->pos, sp, sizeof(sp));
				p->vel[0] = (int16_t)(mul32(ComputeSin((int32_t)m->phase >> 2), 2000) >> 12);
				m->phase = (int16_t)(m->phase + 0x174);
				const int32_t r0 = CrtRand();
				p->vel[0] = (int16_t)((r0 & 0x3F) + ((int32_t)p->vel[0] - sp[0]) / 16 - 0x20);
				const int32_t r1 = CrtRand();
				p->vel[1] = (int16_t)((r1 & 0x3F) + ((int32_t)m->pos[1] - sp[1]) / 16 - 0x20);
				const int32_t r2 = CrtRand();
				const int32_t vz = (r2 & 0x3F) + ((int32_t)m->pos[2] - sp[2]) / 16 - 0x20;
				p->seq = SEQ_Stream;
				p->end = 0x14;
				p->rot = 0x800;
				p->vel[2] = (int16_t)vz;
				p->acc[0] = (int16_t)(-(int32_t)p->vel[0] >> 5);
				p->acc[1] = (int16_t)(-(int32_t)p->vel[1] >> 5);
				p->acc[2] = (int16_t)(-(int32_t)(int16_t)vz >> 5);
				p->scale = (int16_t)((CrtRand() >> 4) + 0xC00);
			}
			// two spray puffs on a cone around the caster's facing
			int16_t a[3];
			a[0] = (int16_t)((CrtRand() & 0x7F) + 0x258);
			a[1] = *(const int16_t *)(Caster() + 0xE);
			a[2] = (int16_t)CrtRand();
			for (int k = 0; k < 2; k++)
			{
				ParticleNode *q = NewParticle();
				if (!q) continue;
				memcpy(q->pos, sp, sizeof(sp));
				AngleDir(a, q->vel);
				q->vel[0] = (int16_t)(q->vel[0] >> 6);
				q->vel[2] = (int16_t)(q->vel[2] >> 6);
				q->vel[1] = (int16_t)(q->vel[1] >> 6);
				q->acc[0] = (int16_t)(-(int32_t)q->vel[0] >> 4);
				q->acc[2] = (int16_t)(-(int32_t)q->vel[2] >> 4);
				q->acc[1] = (int16_t)(-(int32_t)q->vel[1] >> 4);
				q->seq = SEQ_Spray;
				q->end = 0xE;
				const int32_t r = CrtRand();
				q->rot = 0;
				q->scale = (int16_t)((r >> 4) + 0x800);
				a[2] = (int16_t)(a[2] + 0x800);
			}
		}
		if ((uint32_t)(c - 0x2C) <= 0xF)
		{
			// a puff falling around the mouth
			ParticleNode *p = NewParticle();
			if (p)
			{
				p->vel[0] = (int16_t)((CrtRand() & 0x1F) - 0x10);
				p->vel[2] = (int16_t)((CrtRand() & 0x1F) - 0x10);
				p->vel[1] = (int16_t)(-0x10 - (CrtRand() & 0xF));
				p->pos[0] = (int16_t)((uint16_t)(p->vel[0] << 4) + (uint16_t)sp[0]);
				p->pos[2] = (int16_t)((uint16_t)(p->vel[2] << 4) + (uint16_t)sp[2]);
				p->pos[1] = sp[1];
				p->acc[2] = 0;
				p->acc[1] = 0;
				p->acc[0] = 0;
				p->seq = SEQ_Spray;
				p->end = 0xE;
				const int16_t s = (int16_t)((CrtRand() >> 4) + 0x800);
				p->scale = s;
				p->rot = s;
			}
		}
		if (m->counter == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			BdPlaySE3D(SOUND_BadBreath, 0x100, pos);
		}
		if (m->counter >= 0x3B) return TASK_END;
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CACD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag113_bad_breath_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x14C8;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x114C8;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			r->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + 0x10B4), 0x104, 1);
			InitTaskQueuePool(QParticles(), (void *)(TexBase() + 0x2F4), 0x2C, 0x50);
			InitTaskQueuePool(QTargets(), (void *)TexBase(), 0xFC, 3);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			if (m)
			{
				Memset32(&m->counter, 0, 0x3E);
				m->mask = *(const uint8_t *)&Ctx()->target_mask;
			}
		}
		int a, b, c;
		if (r->started)
		{
			a = ExecuteTaskQueue(QMain());
			b = ExecuteTaskQueue(QParticles());
			c = ExecuteTaskQueue(QTargets());
		}
		else a = b = c = (int)n;
		if (r->started && a == 0 && b == 0 && c == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag113_bad_breath()
	{
		register_port(badbreath113::ORIG_RootTask, (void *)badbreath113::RootTask, "113 RootTask", 113);
		register_port(badbreath113::ORIG_MainTask, (void *)badbreath113::MainTask, "113 MainTask", 113);
		register_port(badbreath113::ORIG_ParticleTask, (void *)badbreath113::ParticleTask, "113 ParticleTask", 113);
		register_port(badbreath113::ORIG_TargetTask, (void *)badbreath113::TargetTask, "113 TargetTask", 113);
		// 30 fps layer: see mag113_bad_breath_held.inc
		FX_HELD(register_mag113_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag113_bad_breath_held.inc"
#endif
