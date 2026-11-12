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

// Effect 318: Strong counter-attack (Krysta c0m038; MAG_318_*). Counter-attack 319's big twin (the module right before it):
// the spike layouts, a burst of particles along the spike, flying sprites at the target, then the caster's arm bones grown
// back from nothing (the bone scale task 0x65F2B0, which 319 queues too).
//
// Structure (setup MAG_318 0x65E510 -> _Init 0x65E540 (not ported), file loader 0x65E520 = the texture file named at
// 0xFFE6A0 ("mag317.tim") -> [0x2505418]; the setup queues the TIM, keeps the CASTER's entity (0x2505424), starts camera
// animation 0xFF77AC and the root in its 1-node pool 0x2505468):
//   RootTask (0x65E5C0) - alternates the packet arena (magic buffer + 0x1DE0 / + 0x11DE0); on counter 1 sets up the pools
//     once (main 1 x 0x1304 at + 0, bone scales 5 x 0x1C at + 0x1304, sprites 0x20 x 0x30 at + 0x1390), starts the main task
//     at the first target of action 0 (its default effect position, the layouts 0xFFD9B8 / 0xFF8DB8 / 0xFFB4CC decoded, its
//     particle cursor) and the stream state at + 0x1990. Then the main, bone-scale and sprite queues; once set up its
//     counter is pinned at 2; it ends when the three queues are empty.
//   MainTask (0x65E790) - counters 0..0x32 (then waits for the stream, 0x5342C0):
//     counter 2: S = the caster's bone 0x1D (0x502170, flag 0x800), T = the target's position; the basis M of (T - S)
//       (0x50CBA0, y the other way), three offsets 0xFFE688 / 0xFFE690 / 0xFFE698 through M (0x56C4F0), M2 = the same
//       basis, M3 = the basis of the mirrored direction;
//     counters 2..0x17: layout A (+0xE4) in M2 at S - offset 0, composed with the camera (depth 0);
//     counters 4..0x13: the caster's object 3 shown (entity +0x7C bit 3 cleared at 4); the point L = T e + S (1 - e),
//       e = ((c - 4) / 16)^4 (0x56CB90); layout B (+0x1D0) in M at offset 0 + L (depth -0x80), the prim model 0xFF7CB0
//       in M at L;
//     counters 4..0x14: four steps i (e of (c - 4) / 16 - i / 64): the point L, two particles at offset 1 + L, and from
//       counter 8 on two more at offset 2 + L (0x65F710: a free cell of the 0x80-cell table at + 0xB00, 11 ticks, size
//       rand, velocity = the direction of (0x400, rand + 0x800 k) through M, / 256);
//     counters 0x13..0x32: layout C (+0x7B8) in M3 at T, composed with the camera (depth -0x80); on counters 0x13..0x16:
//       8 flying sprites at T (directions rand / + 0x200 k (0x65F0A0), speed rand, drag velocity / -12);
//     every tick: the particle table drawn and moved (0x65F340: frame of sequence 0xFF7B6C by the life left, the custom
//       quad renderer 0x65F4B0, position + velocity x 4), the stream pumped;
//     sound 0xFF77A8 at counter 8, the stream load of file 0x168 into magic buffer + 0x21DE0 at 0x29, the damage of action
//     0's first target record at 0x18; at 0x27 the caster's object 3 hidden (+0x7C bit 3) and five bone scales queued
//     (0x65F250): bones 0x16 / 0x1D / 0x21 / 0x22 / 0x23 after 0 / 8 / 16 / 16 / 16 ticks.
//   BoneScaleTask (0x65F2B0): a bone matrix of the caster's skeleton (skeleton record + 0x10, scale3DMatrix) scaled (x, x, y)
//     from 0 after its delay, x += 0x200 / y += 0x400 per tick up to 0x1000; ends (unscaled) once both reach 0x1000.
//   SpriteTask (0x65F100) - sprite sequence 0xFF7A00 through the shadow-rotation camera (0x400, -0x100), turned along its
//     screen direction (0x56D160 of its start velocity seen from there), flipbook frame = its counter; then velocity +=
//     drag, position += velocity; 12 ticks.
//   PrimCallback (0x65EE00) = Counter-attack 319's 0x65E190 (Funguar laser 277's without the 0x40000 rotation variant), on
//     this module's cursor.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update; the main draws its particle table after the
// tick's spawns).
// ENTITY / CAMERA: the CASTER's entity is written: +0x7C bit 3 (object 3 visibility) at main counters 4 / 0x27, and its
// skeleton's bone matrices (the bone scales); the camera animation is the engine's.
// Module globals: 0x2505418..0x2505490 (texture file 0x2505418, packet cursor 0x250541C, context 0x2505420, caster entity
// 0x2505424, the queues 0x2505428 sprites / 0x2505438 bone scales / 0x2505448 main / 0x2505458 root, root pool 0x2505468,
// the sprite task's zero vector 0x2505480 (read only), magic buffer base 0x2505488).
// NEVER-WRITTEN STACK WORDS: as 319's; the particle spawn's angle word [1] and the 4th words of the vectors loaded into GTE V0
// (sprite offsets, the start velocity's +0x2E copy of the pool word +0x1E) reach nothing but the GTE VZ0 high half; the main
// task's loop counter and the quad renderer's u width live in their caller's argument slots (written, never read back).

#include "mag_common.h"
#include <stddef.h>

namespace ff8fx
{
namespace strong318
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x250541C); }
	inline CastContext *&Ctx() { return var<CastContext *>(0x2505420); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2505424); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2505488); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2505428; }        // pool: + 0x1390, 0x20 x 0x30
	inline TaskQueue *QBones() { return (TaskQueue *)0x2505438; }          // pool: + 0x1304, 5 x 0x1C
	inline TaskQueue *QMain() { return (TaskQueue *)0x2505448; }           // pool: magic buffer, 1 x 0x1304
	static const int16_t *const ZERO_VEC = (const int16_t *)0x2505480;    // a module word quad the sprite task only reads

	static const uint32_t ORIG_RootTask = 0x65E5C0;
	static const uint32_t ORIG_MainTask = 0x65E790;
	static const uint32_t ORIG_SpriteTask = 0x65F100;
	static const uint32_t ORIG_BoneScaleTask = 0x65F2B0;
	static const uint32_t POOL_Bones = 0x1304, POOL_Sprites = 0x1390;
	static const uint32_t STREAM_STATE = 0x1990;         // magic buffer: stream state
	static const uint32_t MORPH = 0x19D8;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1DE0;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11DE0;
	static const uint32_t STREAM_DEST = 0x21DE0;         // magic buffer: file 0x168 (loaded at counter 0x29)
	static const uint32_t MODEL_A = 0xFFD9B8;            // prim-model layouts
	static const uint32_t MODEL_B = 0xFF8DB8;
	static const uint32_t MODEL_C = 0xFFB4CC;
	static const uint32_t PRIM_Spike = 0xFF7CB0;         // prim model (0x572200)
	static const uint32_t SEQ_Sprite = 0xFF7A00;         // sprite sequence (flying sprites)
	static const uint32_t SEQ_Particle = 0xFF7B6C;       // particle quads: +8 frame count, +8 + 2 i frame offsets
	static const void *const SOUND = (const void *)0xFF77A8;
	static const int16_t *const OFFSETS = (const int16_t *)0xFFE688; // three vectors of 4 words (through M)
	static const int PARTICLES = 0x80;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector (rotation only)
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12 (GTE GPF / GPL)
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t CartesianToGameAngle(int32_t x, int32_t y) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(x, y); }
	// GTE (software PSX geometry engine)
	inline void GteLoadIRFromColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteStoreIRToColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	// battle file streaming (0x5341D0 load a file, 0x534210 pump, 0x5342C0 done)
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamDone(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2505468)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C pinned at 2 once set up
		uint8_t started;   // +0x0E pools set up
		uint8_t pad0F;
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct Particle // 0x80 cells of 0x10 bytes in the main node
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 ticks left (0 = free); the frame of the quad sequence
		int16_t size;      // +0x08 scale (rand / 16 + 0x800)
		int16_t pad0A;
		int8_t vel[3];     // +0x0C x 4 per tick
		uint8_t pad0F;
	};
	struct MainNode // pool of 1 node of 0x1304 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[0x4A];
		uint8_t *ent;      // +0x58 target entity (first target of action 0)
		int16_t S[4];      // +0x5C the caster's bone 0x1D (counter 2)
		int16_t T[4];      // +0x64 the target's default effect position
		int16_t off[3][4]; // +0x6C the offsets through M
		Mat4x3 M;          // +0x84 basis of T - S; translation rewritten per draw
		Mat4x3 M2;         // +0xA4 the same basis; translation S - offset 0
		Mat4x3 M3;         // +0xC4 basis of the mirrored direction; translation T
		uint8_t layout_a[0xEC];  // +0xE4
		uint8_t layout_b[0x5E8]; // +0x1D0
		uint8_t layout_c[0x348]; // +0x7B8
		Particle parts[PARTICLES]; // +0xB00
		Particle *cursor;  // +0x1300 the next cell to try
	};
	struct BoneNode // pool of 5 nodes of 0x1C bytes
	{
		TaskNode hdr;
		uint8_t *bone;     // +0x0C skeleton record (the matrix at + 0x10)
		int16_t delay;     // +0x10 ticks before the growth
		int16_t sy;        // +0x12 scale z (y step +0x14)
		int16_t dy;        // +0x14
		int16_t sx;        // +0x16 scale x / y (x step +0x18)
		int16_t dx;        // +0x18
		int16_t pad1A;
	};
	struct SpriteNode // pool of 0x20 nodes of 0x30 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[4];    // +0x10 (the 4th word: T's)
		int16_t vel[3];    // +0x18
		int16_t pad1E;
		int16_t drag[3];   // +0x20 start velocity / -12
		int16_t pad26;
		int16_t vel0[4];   // +0x28 start velocity (direction on screen; the 4th word: the pool's +0x1E)
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x1304 && sizeof(BoneNode) == 0x1C && sizeof(Particle) == 0x10, "Strong counter-attack nodes");
	static_assert(sizeof(SpriteNode) == 0x30 && sizeof(PrimArg) == 0x28, "Strong counter-attack nodes");
	static_assert(offsetof(MainNode, ent) == 0x58 && offsetof(MainNode, M3) == 0xC4 && offsetof(MainNode, layout_c) == 0x7B8 && offsetof(MainNode, parts) == 0xB00, "Strong counter-attack main node");

	// ------------------------------------------------------------------
	// Prim-player callback (0x65EE00): one object of a layout. Vertex frame (or blend into the
	// parameter block's morph buffer), rotation from the record's angles, the record's offset
	// through the parameter matrix (flag 0x200: as is), the matrix composed with it (unless flag
	// 0x200 or 0x8000), scale (flag 0x100: 3x3 product, else scale3DMatrix), fade, drawn by 0x7043B0.
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
		if (r->flags & 0x200)
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
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MulMatrix3(&m, s);
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
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// e = (x / 4096)^4 (0x65E90C / 0x65EA6C), x = (c - 4) x 256 (- 0x40 per particle step)
	static int32_t Ease4(int32_t x)
	{
		int32_t e = mul32(x, x) >> 12;
		return mul32(e, e) >> 12;
	}

	// the spike point (0x65E918..0x65E92E): weight e of T, 0x1000 - e of S
	static void SpikePoint(const MainNode *m, int32_t e, int16_t *L)
	{
		VecLerp(m->T, m->S, e, 0x1000 - e, L);
	}

	// the spike model (0x65E9B0..0x65EA2C): M at L composed with the camera, model 0xFF7CB0 (no fade)
	static void SpikeDraw(Mat4x3 *M, const int16_t *L, PrimArg *arg)
	{
		M->t[0] = L[0];
		M->t[1] = L[1];
		M->t[2] = L[2];
		ComposeAffineTransform(&Camera(), M, &arg->m);
		GteSetRotMatrix(&arg->m);
		GteSetTransVector(&arg->m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = PRIM_Spike;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x65F7E0: the direction of (a[0], -, a[2]): (sin a2 x -sin a0, -(cos a2 x -sin a0), cos a0)
	static void AngleToDir(const int16_t *a, int16_t *out)
	{
		out[2] = (int16_t)ComputeCos(a[0]);
		const int32_t s = 0 - ComputeSin(a[0]);
		out[0] = (int16_t)(mul32(ComputeSin(a[2]), s) >> 12);
		out[1] = (int16_t)((0 - mul32(ComputeCos(a[2]), s)) >> 12);
	}

	// 0x65F0A0 (AngleToUnitVector): (sin a1 x cos a0, -sin a0, -(cos a1 x cos a0))
	static void AngleToUnitVector(const int16_t *a, int16_t *out)
	{
		const int32_t c = ComputeCos(a[0]);
		out[1] = (int16_t)(0 - ComputeSin(a[0]));
		out[0] = (int16_t)(mul32(ComputeSin(a[1]), c) >> 12);
		out[2] = (int16_t)((0 - mul32(ComputeCos(a[1]), c)) >> 12);
	}

	// 0x65F710: a particle at pos (2 x u32: pos[3] lands in the life word, rewritten) in the next free cell from the cursor
	// (the cursor's own cell first, else the first free one; none: nothing), 11 ticks, size rand / 16 + 0x800, velocity =
	// the direction of ang through the GTE rotation (M) / 256
	static void SpawnParticle(MainNode *m, const int16_t *pos, const int16_t *ang)
	{
		Particle *p = m->cursor;
		if (p->life != 0)
		{
			p = &m->parts[0];
			if (p->life != 0)
			{
				int k = PARTICLES;
				for (;;)
				{
					p++;
					if (--k == 0) return;
					if (p->life == 0) break;
				}
			}
		}
		if ((uint8_t *)p >= (uint8_t *)m + 0x12F0) m->cursor = &m->parts[0];
		else m->cursor = p + 1;
		*(uint32_t *)&p->pos[0] = *(const uint32_t *)&pos[0];
		*(uint32_t *)&p->pos[2] = *(const uint32_t *)&pos[2];
		p->life = 0xB;
		p->size = (int16_t)((CrtRand() >> 4) + 0x800);
		int16_t d[4];
		AngleToDir(ang, d);
		GteLoadV0(d);
		GteMVMVA_RotV0();
		GteStoreIR123(d);
		p->vel[0] = (int8_t)(d[0] >> 8);
		p->vel[1] = (int8_t)(d[1] >> 8);
		p->vel[2] = (int8_t)(d[2] >> 8);
	}

	// 0x65F4B0 (no IDB start): the particle quads of the frame h+0x2C (h+0x30 quads of 0x14 bytes: u width, grey, v height,
	// code, u0, v1, clut / uv word +4, x / y offset +8 / +0xA, tpage +0xE, scale x / y +0x10 / +0x12) as POLY_FT4 packets at
	// cursor, in the frame of h (scale +0x84, camera-space position +0x98), inserted at ot + 4 mode unless the GTE clips them
	static uint32_t RenderQuads(uint8_t *h, uint32_t ot, int mode, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *q = *(const uint8_t *const *)(h + 0x2C);
		int32_t count = *(const int32_t *)(h + 0x30);
		if (count <= 0) return cursor;
		for (; count != 0; count--, q += 0x14)
		{
			*(int16_t *)(h + 0x72) = 0;
			*(int16_t *)(h + 0x70) = 0;
			*(int16_t *)(h + 0x6E) = 0;
			*(int16_t *)(h + 0x6A) = 0;
			*(int16_t *)(h + 0x68) = 0;
			*(int16_t *)(h + 0x66) = 0;
			*(int16_t *)(h + 0x74) = 0x1000;
			*(int16_t *)(h + 0x64) = *(const int16_t *)(q + 0x10);
			const int32_t tpage = *(const int16_t *)(q + 0xE);
			*(int16_t *)(h + 0x6C) = *(const int16_t *)(q + 0x12);
			const int32_t uw = q[0], vh = q[2];
			const int32_t w = uw << 3, hh = vh << 3;
			*(int16_t *)(h + 0x54) = (int16_t)-w;
			*(int16_t *)(h + 0x44) = (int16_t)-w;
			*(int16_t *)(h + 0x5C) = (int16_t)w;
			*(int16_t *)(h + 0x4C) = (int16_t)w;
			*(int16_t *)(h + 0x4E) = (int16_t)-hh;
			*(int16_t *)(h + 0x46) = (int16_t)-hh;
			*(int16_t *)(h + 0x5E) = (int16_t)hh;
			*(int16_t *)(h + 0x56) = (int16_t)hh;
			*(int32_t *)(h + 0x78) = (*(const int16_t *)(q + 8) << 4) + w;
			*(int32_t *)(h + 0x7C) = (*(const int16_t *)(q + 0xA) << 4) + hh;
			*(int32_t *)(h + 0x80) = 0;
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x84));
			GteLoadIRFromColumn(h + 0x64);
			GteMVMVA_RotIR();
			*(uint32_t *)pk = 0x9000000;
			pk[4] = q[1];
			pk[5] = q[1];
			pk[6] = q[1];
			pk[7] = q[3];
			GteStoreIRToColumn(h + 0x64);
			GteLoadIRFromColumn(h + 0x66);
			GteMVMVA_RotIR();
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(q + 4);
			GteStoreIRToColumn(h + 0x66);
			GteLoadIRFromColumn(h + 0x68);
			GteMVMVA_RotIR();
			pk[0x1C] = q[4];
			GteStoreIRToColumn(h + 0x68);
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x84));
			GteLoadV0Dwords(h + 0x78);
			GteMVMVA_RotV0Tr();
			pk[0x15] = q[5];
			GteReadMAC123((int32_t *)(h + 0x78));
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x64));
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x64));
			GteLoadV012(h + 0x44, h + 0x4C, h + 0x54);
			GteRTPT();
			int32_t u = q[4] + uw;
			if (u >= 0x100) u = 0xFF;
			pk[0x24] = (uint8_t)u;
			pk[0x14] = (uint8_t)u;
			GteReadFLAG(h + 0x40);
			if (*(const uint32_t *)(h + 0x40) & 0x60000) continue;
			*(uint16_t *)(pk + 0x16) = (uint16_t)(tpage & 0x1FF);
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(h + 0x5C);
			GteRTPS();
			int32_t v = q[5] + vh;
			if (v >= 0x100) v = 0xFF;
			pk[0x25] = (uint8_t)v;
			pk[0x1D] = (uint8_t)v;
			GteReadSXY2(pk + 0x20);
			InsertPrimAutoDepth(ot + mode * 4, pk);
			pk += 0x28;
		}
		return (uint32_t)pk;
	}

	// 0x65F3D0 (= Red Giant 316's 0x6627B0): one particle: its camera-space position, its scale, the frame of the quad
	// sequence its life selects, drawn; then its life steps down and it moves by its velocity x 4
	static void ParticleDraw(uint8_t *h, Particle *p)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(p->pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 0x94) = p->size;
		*(int16_t *)(h + 0x8C) = p->size;
		*(int16_t *)(h + 0x84) = p->size;
		const uint32_t idx = (*(const uint32_t *)(SEQ_Particle + 8) & 0xFFFF) - (int32_t)p->life;
		const uint32_t f = *(const uint16_t *)(SEQ_Particle + 8 + idx * 2) + SEQ_Particle;
		*(uint32_t *)(h + 0x30) = *(const uint32_t *)f;
		*(uint32_t *)(h + 0x2C) = f + 4;
		PacketCursor() = RenderQuads(h, RenderOT(), 2, PacketCursor());
		p->life--;
		p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] << 2));
		p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->vel[1] << 2));
		p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] << 2));
	}

	// 0x65F340: every live cell of the table drawn (one header for all)
	static void ParticleWalk(Particle *cells)
	{
		uint8_t *h = AllocHeader(0xB4);
		*(int16_t *)(h + 0x92) = 0;
		*(int16_t *)(h + 0x90) = 0;
		*(int16_t *)(h + 0x8E) = 0;
		*(int16_t *)(h + 0x8A) = 0;
		*(int16_t *)(h + 0x88) = 0;
		*(int16_t *)(h + 0x86) = 0;
		*(int16_t *)(h + 0x60) = 0;
		*(int16_t *)(h + 0x58) = 0;
		*(int16_t *)(h + 0x50) = 0;
		*(int16_t *)(h + 0x48) = 0;
		for (int i = 0; i < PARTICLES; i++)
			if (cells[i].life != 0) ParticleDraw(h, &cells[i]);
		FieldFree(0xB4);
	}

	// the flying sprite's draw (0x65F100..0x65F1F3): turned along the screen direction of its start velocity
	static void SpriteDraw(const int16_t *pos, const int16_t *vel0, int16_t frame)
	{
		uint8_t *h = AllocHeader(0xB4);
		int16_t d[4];
		d[0] = (int16_t)(pos[0] - vel0[0]);
		d[1] = (int16_t)(pos[1] - vel0[1]);
		d[2] = (int16_t)(pos[2] - vel0[2]);
		d[3] = 0; // (never written: only the GTE VZ0 high half would see it; TransformCameraByShadowRotation reads 3 words)
		TransformCameraByShadowRotation(d, 0x400, -0x100);
		GteLoadV0(ZERO_VEC);
		GteMVMVA_RotV0Tr();
		int16_t a[4], b[4];
		GteStoreIR123(a);
		TransformCameraByShadowRotation(pos, 0x400, -0x100);
		GteLoadV0(vel0);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(b);
		*(int32_t *)(h + 8) = 0 - CartesianToGameAngle((int32_t)a[1] - b[1], (int32_t)b[0] - a[0]);
		*(uint32_t *)h = SEQ_Sprite;
		*(int16_t *)(h + 0x24) = 1;
		*(int16_t *)(h + 4) = frame;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag318_strong_counter_attack_held.h"
#endif

namespace ff8fx
{
namespace strong318
{
	// ------------------------------------------------------------------
	// Sprite (0x65F100)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag318_strong_counter_attack_held.inc
		FX_HELD(held_note_sprite(n);)
		SpriteDraw(s->pos, s->vel0, s->counter);
		s->counter++;
		if (s->counter >= 0xC) return TASK_END;
		s->vel[1] = (int16_t)(s->vel[1] + s->drag[1]);
		s->vel[0] = (int16_t)(s->vel[0] + s->drag[0]);
		s->vel[2] = (int16_t)(s->vel[2] + s->drag[2]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Bone scale (0x65F2B0): delay, then the scales grow to 0x1000
	// ------------------------------------------------------------------
	static uint32_t __cdecl BoneScaleTask(TaskNode *n)
	{
		BoneNode *b = (BoneNode *)n;
		b->delay--;
		if (b->delay <= 0)
		{
			b->sy = (int16_t)(b->sy + b->dy);
			b->delay = 0;
			if (b->sy > 0x1000) b->sy = 0x1000;
			b->sx = (int16_t)(b->sx + b->dx);
			if (b->sx > 0x1000) b->sx = 0x1000;
			if (b->sx >= 0x1000 && b->sy >= 0x1000) return TASK_END;
		}
		const int32_t v[3] = { b->sx, b->sx, b->sy };
		Scale3DMatrix((Mat4x3 *)(b->bone + 0x10), v);
		return 0;
	}

	// 0x65F250: a bone scale task on the caster's skeleton bone `bone` (no pool check: 5 nodes for the 5 calls)
	static void StartBoneScale(int32_t bone, int16_t delay, int16_t dx, int16_t dy)
	{
		BoneNode *b = (BoneNode *)AddTaskToQueue(QBones(), ORIG_BoneScaleTask);
		b->bone = **(uint8_t ***)(Caster() + 0x64) + bone * 0x30 + 0x10;
		b->delay = delay;
		b->dx = dx;
		b->dy = dy;
		b->sy = 0;
		b->sx = 0;
	}

	// ------------------------------------------------------------------
	// Main (0x65E790)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		PrimArg arg;
		if (m->counter == 2)
		{
			GetEffectSpawnPosition(Caster(), 0x1D, 0x800, m->S);
			int32_t v[3] = { (int32_t)m->T[0] - m->S[0], (int32_t)m->S[1] - m->T[1], (int32_t)m->S[2] - m->T[2] };
			BuildOrthonormalBasis(v, &m->M);
			MatrixMulVector(&m->M, OFFSETS, m->off[0]);
			MatrixMulVector(&m->M, OFFSETS + 4, m->off[1]);
			MatrixMulVector(&m->M, OFFSETS + 8, m->off[2]);
			BuildOrthonormalBasis(v, &m->M2);
			v[0] = 0 - v[0];
			v[2] = 0 - v[2];
			BuildOrthonormalBasis(v, &m->M3);
		}
		if ((uint32_t)(m->counter - 2) < 0x16)
		{
			m->M2.t[0] = (int32_t)m->S[0] - m->off[0][0];
			m->M2.t[1] = (int32_t)m->S[1] - m->off[0][1];
			m->M2.t[2] = (int32_t)m->S[2] - m->off[0][2];
			ComposeAffineTransform(&Camera(), &m->M2, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = 0;
			// 30 fps layer: see mag318_strong_counter_attack_held.inc
			FX_HELD(held_note_main(PLAY_A, n);)
			prim::play((prim::Layout *)m->layout_a, PrimCallback, (int)&arg, 0);
		}
		const int32_t e4 = m->counter - 4;
		if ((uint32_t)e4 < 0x10)
		{
			if (e4 == 0) *(uint32_t *)(Caster() + 0x7C) &= ~8u;
			int16_t L[4];
			SpikePoint(m, Ease4((int32_t)((uint32_t)(e4 << 12) >> 4)), L);
			m->M.t[0] = (int32_t)m->off[0][0] + L[0];
			m->M.t[1] = (int32_t)m->off[0][1] + L[1];
			m->M.t[2] = (int32_t)m->off[0][2] + L[2];
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x80;
			FX_HELD(held_note_main(PLAY_B, n);)
			prim::play((prim::Layout *)m->layout_b, PrimCallback, (int)&arg, 0);
			FX_HELD(held_note_main(DRAW_SPIKE, n);)
			SpikeDraw(&m->M, L, &arg);
		}
		if ((uint32_t)e4 < 0x11)
		{
			GteSetRotMatrix(&m->M);
			int32_t x = (e4 << 12) / 16;
			for (int i = 0; i < 4; i++)
			{
				int16_t L[4];
				SpikePoint(m, Ease4(x), L);
				x -= 0x40;
				int16_t ang[3], pos[4];
				ang[0] = 0x400;
				ang[2] = (int16_t)CrtRand();
				pos[0] = (int16_t)(m->off[1][0] + L[0]);
				pos[1] = (int16_t)(m->off[1][1] + L[1]);
				pos[2] = (int16_t)(m->off[1][2] + L[2]);
				pos[3] = 0; // (never written: copied into the particle's life word, rewritten there)
				for (int j = 2; j != 0; j--)
				{
					SpawnParticle(m, pos, ang);
					ang[2] = (int16_t)(ang[2] + 0x800);
				}
				if ((uint32_t)e4 >= 4)
				{
					ang[0] = 0x400;
					ang[2] = (int16_t)CrtRand();
					pos[0] = (int16_t)(m->off[2][0] + L[0]);
					pos[1] = (int16_t)(m->off[2][1] + L[1]);
					pos[2] = (int16_t)(m->off[2][2] + L[2]);
					for (int j = 2; j != 0; j--)
					{
						SpawnParticle(m, pos, ang);
						ang[2] = (int16_t)(ang[2] + 0x800);
					}
				}
			}
		}
		const int32_t c13 = m->counter - 0x13;
		if ((uint32_t)c13 < 0x20)
		{
			m->M3.t[0] = m->T[0];
			m->M3.t[1] = m->T[1];
			m->M3.t[2] = m->T[2];
			ComposeAffineTransform(&Camera(), &m->M3, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x80;
			FX_HELD(held_note_main(PLAY_C, n);)
			prim::play((prim::Layout *)m->layout_c, PrimCallback, (int)&arg, 0);
			if ((uint32_t)c13 < 4)
			{
				int16_t ang[2];
				ang[1] = (int16_t)CrtRand();
				for (int k = 8; k != 0; k--)
				{
					SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
					if (s)
					{
						s->counter = 0;
						*(uint32_t *)&s->pos[0] = *(const uint32_t *)&m->T[0];
						*(uint32_t *)&s->pos[2] = *(const uint32_t *)&m->T[2];
						ang[0] = (int16_t)((mul32(CrtRand(), 1500) >> 15) - 0x2EE);
						AngleToUnitVector(ang, s->vel);
						GteSetIR0((CrtRand() & 0xFF) + 0x80);
						GteLoadIR123(s->vel);
						GteGPF();
						GteStoreIR123(s->vel);
						s->drag[0] = (int16_t)(s->vel[0] / -12);
						s->drag[1] = (int16_t)(s->vel[1] / -12);
						s->drag[2] = (int16_t)(s->vel[2] / -12);
						*(uint32_t *)&s->vel0[0] = *(const uint32_t *)&s->vel[0];
						*(uint32_t *)&s->vel0[2] = *(const uint32_t *)&s->vel[2];
					}
					ang[1] = (int16_t)(ang[1] + 0x200);
				}
			}
		}
		FX_HELD(held_note_particles(n);)
		ParticleWalk(m->parts);
		if (m->counter == 8) BdPlaySE(SOUND, 0, 0x80);
		if (m->counter == 0x29) StreamLoad(0x168, TexBase() + STREAM_DEST, 0);
		StreamPump();
		if (m->counter == 0x18) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (m->counter == 0x27)
		{
			*(uint32_t *)(Caster() + 0x7C) |= 8u;
			StartBoneScale(0x16, 0, 0x200, 0x400);
			StartBoneScale(0x1D, 8, 0x200, 0x400);
			StartBoneScale(0x21, 0x10, 0x200, 0x400);
			StartBoneScale(0x22, 0x10, 0x200, 0x400);
			StartBoneScale(0x23, 0x10, 0x200, 0x400);
		}
		m->counter++;
		if (m->counter >= 0x33)
		{
			if (StreamDone(0) != 0) return TASK_END;
			m->counter = 0x33;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x65E5C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag318_strong_counter_attack_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMain(), (void *)TexBase(), 0x1304, 1);
				InitTaskQueuePool(QBones(), (void *)(TexBase() + POOL_Bones), 0x1C, 5);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + POOL_Sprites), 0x30, 0x20);
			}
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x4BE);
			m->ent = Entity(Ctx()->actions[0].targets[0]);
			GetDefaultEffectPosition(m->ent, m->T);
			m->cursor = m->parts;
			DecodeModelPrimLayout(MODEL_A, m->layout_a, 0xEC);
			DecodeModelPrimLayout(MODEL_B, m->layout_b, 0x5E8);
			DecodeModelPrimLayout(MODEL_C, m->layout_c, 0x348);
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
		}
		int a0, a1, a2;
		if (r->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QBones());
			a2 = ExecuteTaskQueue(QSprites());
		}
		else a0 = a1 = a2 = (int)n;
		if (r->started)
		{
			if (a0 == 0 && a1 == 0 && a2 == 0) return TASK_END;
			r->counter = 2;
			return 0;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag318_strong_counter_attack()
	{
		register_port(strong318::ORIG_RootTask, (void *)strong318::RootTask, "318 RootTask", 318);
		register_port(strong318::ORIG_MainTask, (void *)strong318::MainTask, "318 MainTask", 318);
		register_port(strong318::ORIG_SpriteTask, (void *)strong318::SpriteTask, "318 SpriteTask", 318);
		register_port(strong318::ORIG_BoneScaleTask, (void *)strong318::BoneScaleTask, "318 BoneScaleTask", 318);
		// 30 fps layer: see mag318_strong_counter_attack_held.inc
		FX_HELD(register_mag318_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag318_strong_counter_attack_held.inc"
#endif
