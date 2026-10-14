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

// Effect 277: Funguar's laser (enemy attack 115 of kernel.bin, unnamed; Funguar c0m058; MAG_277_*).
// No other kernel.bin entry uses effect 277. A Beam Laser (effect 134) variant: no charge layouts,
// the beam starts at a model vertex, a different ease, sparks with drag.
//
// Structure (setup MAG_277 0x683F10 -> _Init 0x683F40, file loader 0x683F20 = the texture file
// named at 0x103BBC8; the setup keeps the CASTER's entity, starts camera animation 0x10D2CCC and
// queues the TIM):
//   MasterTask (0x683FC0) - alternates the packet arena (magic buffer + 0x10AC / + 0x110AC); on
//     counter 1 sets up the pools once (beam 1 x 0x664 at + 0x8C0, glows 0x10 x 0x14 at + 0x780,
//     sparks 0x10 x 0x24 at + 0x540, hits 8 x 0xA8 at + 0) and, while its action index <= action
//     0's last_action, starts a beam at the first target of that action (a beam already running on
//     the same entity: counter back to 0, retried next tick; the pool full: no beam). Then the beam,
//     glow, spark and hit queues; ends when all four are empty.
//   BeamTask (0x6841F0) - counters 0..0x32. Frame: P = vertex 0x7B of the caster model's object 0
//     in world space (0x684B20: the caster's world bone matrices 0x5095B0, then its bone matrices
//     rebuilt from its pose 0x508C90), T = the target's default effect position, basis M of
//     (T - P) in the floor plane (0x50CBA0), R = M with rows 0 / 2 negated. Sound at counter 0.
//       counters 5..0x2C: the muzzle layout 0x10CE318 at P (0x684DE0: camera rotation, depth 0);
//         T lifted by an ease on the target's size word +0x26 (e = counter - 15); the basis M2 of
//         (T - P) with P as its translation, composed with the camera: layout 0x10D0ED4 (depth 0);
//       counters 15..0x2C: T at or below the floor (y >= 0) is moved onto the floor along P -> T;
//         the beam = prim model 0x10CB9E4 (0x572200) in M2 x diag(w, w, L), w = sin(e / 2) / 8,
//         L = |T - P| (twice while T.y < the target's word +0x92); fade (e - 20) * 0x200 from e = 20.
//         Spawns: T on the floor -> a hit layout (R, T); T in the target's lowest eighth (between
//         +0x92 - (+0x92 >> 3) and the floor) on even counters -> a glow sprite;
//       counters 0x21..0x32: the impact layout 0x10CD0AC in R at (spawn x, target +0x92 / 2,
//         spawn z) (spawn = the target's bone 0xF1 at the spawn), composed with the camera (depth
//         -0x80); counters 0x24 / 0x25: 8 sparks each (CRT rand velocities, drag velocity / -12).
//     Damage of action 0's first target record at 0x2B.
//   HitTask (0x684C40) - its layout 0x10D2134 in its frame composed with the camera; ends with it.
//   SparkTask (0x684CA0) - sprite sequence 0x10CB838 through the shadow-rotation camera (0xC00,
//     -0x300), flipbook frame = its counter, then moves by its velocity, the velocity plus its drag
//     while the counter is below 0xC; 16 ticks.
//   GlowTask (0x684D60) - sprite sequence 0x10CB628 (0xAF0, -0x400), frame = counter; 16 ticks.
//   PrimCallback (0x684870, Beam Laser's 0x6B0AB0 with the 0x40000 rotation variant and the 0x8000
//     no-composition flag of Oil Blast's 0x6ACF40, on this module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: the beam rebuilds the caster entity's bone matrices (0x5095B0 world, 0x508C90 from
// the pose) every tick; no task writes an entity position (+0x1C..+0x20 only read) or the camera
// words (the camera animation is the engine's).
// Module globals: 0x2508200..0x2508278 (caster entity 0x2508200, texture file 0x2508204, magic
// buffer base 0x2508208, context 0x250820C, master pool 0x2508210, packet cursor 0x2508224, the
// queues 0x2508228 hits / 0x2508238 sparks / 0x2508248 glows / 0x2508258 beam / 0x2508268 master).
// NEVER-WRITTEN STACK WORDS: the 4th words of the vectors loaded into GTE V0 (0x684DE0's offset, the
// callback's record offset) reach the GTE's VZ0 high half (0x1CA8A16) only; P's 4th word (StoreIR123
// writes 3) is never read; the matrices' pad words are never read; the beam's prim header keeps the
// scratch's +0x04 / +0x18 (and +0x08..+0x0C when it does not fade), as in the game.

#include "mag_common.h"

namespace ff8fx
{
namespace laser277
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508200); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2508208); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x250820C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508224); }
	inline TaskQueue *QHits() { return (TaskQueue *)0x2508228; }           // pool: magic buffer, 8 x 0xA8
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2508238; }         // pool: + 0x540, 0x10 x 0x24
	inline TaskQueue *QGlows() { return (TaskQueue *)0x2508248; }          // pool: + 0x780, 0x10 x 0x14
	inline TaskQueue *QBeam() { return (TaskQueue *)0x2508258; }           // pool: + 0x8C0, 1 x 0x664

	static const uint32_t ORIG_MasterTask = 0x683FC0;
	static const uint32_t ORIG_BeamTask = 0x6841F0;
	static const uint32_t ORIG_HitTask = 0x684C40;
	static const uint32_t ORIG_SparkTask = 0x684CA0;
	static const uint32_t ORIG_GlowTask = 0x684D60;
	static const uint32_t MORPH = 0xF24;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x10AC;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x110AC;
	static const uint32_t POOL_Beam = 0x8C0;
	static const uint32_t MODEL_Muzzle = 0x10CE318;      // prim-model layouts
	static const uint32_t MODEL_Ray = 0x10D0ED4;
	static const uint32_t MODEL_Impact = 0x10CD0AC;
	static const uint32_t MODEL_Hit = 0x10D2134;
	static const uint32_t PRIM_Beam = 0x10CB9E4;         // prim model (0x572200)
	static const uint32_t SEQ_Spark = 0x10CB838;         // sprite sequences
	static const uint32_t SEQ_Glow = 0x10CB628;
	static const void *const SOUND_Beam = (const void *)0x10D2E9C;
	static const int16_t P_OBJECT = 0, P_VERTEX = 0x7B;  // the beam's start: caster model object 0, vertex 0x7B

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }       // MAG_069_sub_703FB0: V0 = 3 x u16
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2508210)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t index;     // +0x0E next action to start a beam for
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct BeamNode // pool of 1 node of 0x664 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t index;     // +0x0E its action
		uint8_t *ent;      // +0x10 target entity (first target of its action)
		int16_t spawn[4];  // +0x14 target bone 0xF1 at the spawn
		uint8_t muzzle[0x3B0];   // +0x1C
		uint8_t ray[0xE8];       // +0x3CC
		uint8_t impact[0x1B0];   // +0x4B4
	};
	struct HitNode // pool of 8 nodes of 0xA8 bytes
	{
		TaskNode hdr;
		Mat4x3 frame;      // +0x0C R, translation = T on the floor (pad +0x1E never written)
		uint8_t layout[0x7C]; // +0x2C
	};
	struct SparkNode // pool of 0x10 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t pad1A;
		int16_t drag[3];   // +0x1C velocity / -12, added to the velocity while the counter < 0xC
		int16_t pad22;
	};
	struct GlowNode // pool of 0x10 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BeamNode) == 0x664 && sizeof(HitNode) == 0xA8, "Funguar laser nodes");
	static_assert(sizeof(SparkNode) == 0x24 && sizeof(GlowNode) == 0x14, "Funguar laser nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Funguar laser prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x684870): one object of a layout. Vertex frame (or blend into the
	// parameter block's morph buffer), rotation from the record's angles (flag 0x40000: 0x7015B0),
	// the record's offset through the parameter matrix (flag 0x200: as is), the matrix composed
	// with it (unless flag 0x200 or 0x8000), scale (flag 0x100: 3x3 product, else scale3DMatrix),
	// fade, drawn by 0x7043B0.
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
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

	// 0x684BA0: the vertex (Field_Alloc block +0x0E) of a model object (+0x0C): walks the object's
	// bone groups (bone, vertex count, vertices of 6 bytes); +0x04 = the vertex, +0x08 = its bone's
	// matrix (block indices past the last group: +0x04 just after it)
	static void SeekVertex(uint8_t *blk, uint8_t *anim)
	{
		uint8_t *model = *(uint8_t **)(anim + 4);
		uint8_t *bones = *(uint8_t **)model + 0x10;
		uint8_t *table = *(uint8_t **)(model + 4);
		uint8_t *p = table + *(const int32_t *)(table + *(int16_t *)(blk + 0xC) * 4 + 4);
		int32_t groups = *(const int16_t *)p;
		p += 2;
		*(uint8_t **)(blk + 4) = p;
		while (groups-- != 0)
		{
			*(uint8_t **)(blk + 8) = bones + *(const int16_t *)p * 0x30 + 0x10;
			p += 2;
			const int16_t count = *(const int16_t *)p;
			p += 2;
			*(uint8_t **)(blk + 4) = p;
			const int16_t idx = *(int16_t *)(blk + 0xE);
			if (idx < count)
			{
				*(uint8_t **)(blk + 4) = p + idx * 6;
				return;
			}
			*(int16_t *)(blk + 0xE) = (int16_t)(idx - count);
			p += count * 6;
			*(uint8_t **)(blk + 4) = p;
		}
	}

	// 0x684B20: a vertex of the entity's model in world space (out: 3 words): the world bone
	// matrices (0x5095B0), the vertex through its bone, then the bone matrices rebuilt from the pose
	static void VertexPoint(uint8_t *ent, int16_t object, int16_t vertex, int16_t *out)
	{
		uint8_t *blk = (uint8_t *)FieldAlloc(0x10);
		*(int16_t *)(blk + 0xC) = object;
		*(int16_t *)(blk + 0xE) = vertex;
		uint8_t *anim = ent + 0x60;
		ComputeBonesWorldMatrices(anim, ent + 0x40);
		SeekVertex(blk, anim);
		GteSetRotMatrix(*(const Mat4x3 **)(blk + 8));
		GteSetTransVector(*(const Mat4x3 **)(blk + 8));
		GteLoadV0u(*(const void **)(blk + 4));
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
		BuildBoneMatricesFromPose(anim);
		FieldFree(0x10);
	}

	// 0x684DE0 (frame part): the offset v through the basis M, plus P (v is rewritten), then through the
	// camera: camera rotation, translation camera x v + camera t; depth 0
	static void PointFrame(const Mat4x3 *M, const int16_t *P, int16_t *v, PrimArg *arg)
	{
		GteSetRotMatrixCtrl(M);
		GteLoadV0(v);
		GteMVMVA_RotV0();
		GteStoreIR123(v);
		v[0] = (int16_t)(v[0] + P[0]);
		v[1] = (int16_t)(v[1] + P[1]);
		v[2] = (int16_t)(v[2] + P[2]);
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(v);
		GteMVMVA_RotV0();
		int32_t mac[3];
		GteReadMAC123(mac);
		memcpy(&arg->m.m, &Camera().m, sizeof(arg->m.m));
		arg->m.t[0] = (int32_t)((uint32_t)mac[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)mac[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)mac[2] + (uint32_t)Camera().t[2]);
		arg->depth = 0;
		arg->morph = TexBase() + MORPH;
	}

	// the beam's frame (0x6841F0..0x6842D8): P = the caster model's vertex, T = the target's default
	// effect position, v = (T - P) in the floor plane, M its basis
	struct BeamFrame
	{
		int16_t P[4];
		int16_t T[4];
		int32_t v[3];
		Mat4x3 M;
		Mat4x3 R;          // M, rows 0 / 2 negated (translation set where used)
	};
	static void BuildFrame(const BeamNode *b, BeamFrame *f)
	{
		VertexPoint(Caster(), P_OBJECT, P_VERTEX, f->P);
		GetDefaultEffectPosition(b->ent, f->T);
		f->v[0] = (int32_t)f->T[0] - f->P[0];
		f->v[1] = 0;
		f->v[2] = (int32_t)f->P[2] - f->T[2];
		BuildOrthonormalBasis(f->v, &f->M);
		for (int k = 0; k < 3; k++)
		{
			f->R.m[0][k] = (int16_t)(0 - f->M.m[0][k]);
			f->R.m[1][k] = f->M.m[1][k];
			f->R.m[2][k] = (int16_t)(0 - f->M.m[2][k]);
		}
	}

	// counters 5..0x2C: the target lift, the beam's width angle and its fade (0x68433C..0x6843D4);
	// e = counter - 15 (unsigned compares)
	struct Ease
	{
		int16_t lift;      // added to T.y
		int32_t width;     // ebp
		int32_t fade;
	};
	static Ease BeamEase(int32_t counter, int16_t size)
	{
		Ease r;
		const uint32_t e = (uint32_t)(counter - 15);
		if (e < 0x14)
		{
			r.width = 0x1000 - ComputeCos((int32_t)((e << 10) / 20u));
			r.fade = 0;
			r.lift = (int16_t)((int32_t)size * 2 - (mul32(size, r.width) >> 10));
		}
		else if (e < 0x28)
		{
			r.fade = (int32_t)((e - 0x14) << 9);
			r.lift = (int16_t)((int32_t)size * 2 - (shl32(mul32((int32_t)e + 0x2C, size), 2) >> 6));
			r.width = 0x380 - (int32_t)(e << 5);
		}
		else
		{
			r.fade = 0;
			r.width = 0;
			r.lift = (int16_t)(size * 2);
		}
		return r;
	}

	// counters 15..0x2C: T at or below the floor (y >= 0) onto the floor along P -> T (0x68449D..0x6844E6)
	static void FloorHit(const int16_t *P, int16_t *T)
	{
		const int32_t dy = (int32_t)T[1] - P[1];
		const int32_t ax = mul32((int32_t)P[0] - T[0], P[1]);
		const int32_t az = mul32((int32_t)P[2] - T[2], P[1]);
		T[0] = (int16_t)(ax / dy + P[0]);
		T[2] = (int16_t)(az / dy + P[2]);
		T[1] = 0;
	}

	// the beam (0x6844F9..0x684616): the basis M2 of (T - P), its length; the spawn kind (0 none,
	// 1 hit layout, 2 glow sprite on even counters) for the caller
	struct BeamDraw
	{
		Mat4x3 M2;
		int32_t len;       // |T - P| (x 2 while T.y < the target's word +0x92)
		int kind;
	};
	static void BeamShape(const BeamNode *b, int32_t counter, const int16_t *P, const int16_t *T, BeamDraw *d)
	{
		const int32_t u[3] = { (int32_t)T[0] - P[0], (int32_t)P[1] - T[1], (int32_t)P[2] - T[2] };
		int32_t len = BuildOrthonormalBasis(u, &d->M2);
		d->kind = 0;
		const int16_t top = *(const int16_t *)(b->ent + 0x92);
		if (T[1] < top) len <<= 1;
		else if (T[1] >= 0) d->kind = 1;
		else if ((int32_t)T[1] >= (int32_t)top - ((int32_t)top >> 3) && !(counter & 1)) d->kind = 2;
		d->len = len;
	}
	static int16_t BeamWidth(int32_t angle) { return (int16_t)(ComputeSin(angle >> 1) >> 3); }
	static void BeamRender(Mat4x3 *M2, int32_t len, int16_t w, int32_t fade)
	{
		Mat4x3 s;
		Memset32(&s, 0, 8);
		s.m[0][0] = w;
		s.m[1][1] = w;
		s.m[2][2] = (int16_t)len;
		MulMatrix3(M2, &s);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = PRIM_Beam;
		if (fade == 0) *(uint32_t *)(h + 0x1C) = 0x30;
		else
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF0;
		}
		ComposeAffineTransform(&Camera(), M2, M2);
		GteSetRotMatrix(M2);
		GteSetTransVector(M2);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the impact frame (0x6846E2..0x684714): R at (spawn x, target +0x92 / 2, spawn z)
	static void ImpactFrame(const BeamNode *b, Mat4x3 *R)
	{
		R->t[0] = b->spawn[0];
		R->t[1] = *(const int16_t *)(b->ent + 0x92) / 2;
		R->t[2] = b->spawn[2];
	}

	// the sprites (0x684CA3..0x684D02 / 0x684D62..0x684DC0)
	static void SpriteDraw(const int16_t *pos, int16_t frame, uint32_t seq, int32_t a, int32_t b)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, a, b);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag277_funguar_laser_held.h"
#endif

namespace ff8fx
{
namespace laser277
{
	// ------------------------------------------------------------------
	// Glow (0x684D60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GlowTask(TaskNode *n)
	{
		GlowNode *g = (GlowNode *)n;
		// 30 fps layer: see mag277_funguar_laser_held.inc
		FX_HELD(held_note_sprite(SPR_GLOW, n, g->pos, nullptr, g->counter);)
		SpriteDraw(g->pos, g->counter, SEQ_Glow, 0xAF0, -0x400);
		g->counter++;
		return g->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spark (0x684CA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag277_funguar_laser_held.inc
		FX_HELD(held_note_sprite(SPR_SPARK, n, s->pos, s->vel, s->counter);)
		SpriteDraw(s->pos, s->counter, SEQ_Spark, 0xC00, -0x300);
		s->counter++;
		if (s->counter >= 0x10) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		if (s->counter < 0xC)
		{
			s->vel[0] = (int16_t)(s->drag[0] + s->vel[0]);
			s->vel[1] = (int16_t)(s->drag[1] + s->vel[1]);
			s->vel[2] = (int16_t)(s->drag[2] + s->vel[2]);
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Hit (0x684C40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &h->frame, &arg.m);
		arg.depth = 0;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag277_funguar_laser_held.inc
		FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)h->layout);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Beam (0x6841F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		BeamFrame f;
		BuildFrame(b, &f);
		if (b->counter == 0) BdPlaySE(SOUND_Beam, 0, 0x80);
		const int32_t c = b->counter;
		PrimArg arg;
		if ((uint32_t)(c - 5) < 0x28)
		{
			Mat4x3 M;
			BuildOrthonormalBasis(f.v, &M);
			int16_t v[4] = {};
			PointFrame(&M, f.P, v, &arg);
			// 30 fps layer: see mag277_funguar_laser_held.inc
			FX_HELD(held_note_play(PLAY_MUZZLE, n, (prim::Layout *)b->muzzle);)
			prim::play((prim::Layout *)b->muzzle, PrimCallback, (int)&arg, 0);
			const Ease ez = BeamEase(c, *(const int16_t *)(b->ent + 0x26));
			int16_t T[4] = { f.T[0], (int16_t)(f.T[1] + ez.lift), f.T[2], f.T[3] };
			BeamDraw d;
			{
				const int32_t u[3] = { (int32_t)T[0] - f.P[0], (int32_t)f.P[1] - T[1], (int32_t)f.P[2] - T[2] };
				BuildOrthonormalBasis(u, &d.M2);
			}
			d.M2.t[0] = v[0];
			d.M2.t[1] = v[1];
			d.M2.t[2] = v[2];
			PrimArg ray;
			ComposeAffineTransform(&Camera(), &d.M2, &ray.m);
			ray.depth = 0;
			ray.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_RAY, n, (prim::Layout *)b->ray);)
			prim::play((prim::Layout *)b->ray, PrimCallback, (int)&ray, 0);
			if ((uint32_t)(c - 15) < 0x28)
			{
				if (T[1] >= 0) FloorHit(f.P, T);
				BeamShape(b, c, f.P, T, &d);
				if (d.kind == 2)
				{
					GlowNode *g = (GlowNode *)AddTaskToQueue(QGlows(), ORIG_GlowTask);
					if (g)
					{
						g->pos[0] = T[0];
						g->pos[1] = T[1];
						g->pos[2] = T[2];
						g->counter = 0;
					}
				}
				else if (d.kind == 1)
				{
					HitNode *h = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
					if (h)
					{
						memcpy(&h->frame.m, &f.R.m, sizeof(h->frame.m));
						h->frame.t[0] = T[0];
						h->frame.t[1] = T[1];
						h->frame.t[2] = T[2];
						DecodeModelPrimLayout(MODEL_Hit, h->layout, 0x7C);
					}
				}
				FX_HELD(held_note_beam(n);)
				BeamRender(&d.M2, d.len, BeamWidth(ez.width), ez.fade);
			}
		}
		if ((uint32_t)(c - 0x21) < 0x18)
		{
			ImpactFrame(b, &f.R);
			ComposeAffineTransform(&Camera(), &f.R, &arg.m);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_IMPACT, n, (prim::Layout *)b->impact);)
			prim::play((prim::Layout *)b->impact, PrimCallback, (int)&arg, 0);
		}
		if ((uint32_t)(c - 0x24) < 2)
		{
			for (int k = 0; k < 8; k++)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				if (!s) continue;
				memcpy(s->pos, b->spawn, 4);
				*(uint32_t *)&s->pos[2] = *(const uint32_t *)&b->spawn[2];
				s->counter = 0;
				s->vel[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				s->vel[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				s->vel[2] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				s->drag[0] = (int16_t)(s->vel[0] / -12);
				s->drag[1] = (int16_t)(s->vel[1] / -12);
				s->drag[2] = (int16_t)(s->vel[2] / -12);
			}
		}
		if (b->counter == 0x2B) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (b->counter >= 0x32) return TASK_END;
		b->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x683FC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag277_funguar_laser_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		if (m->counter == 1)
		{
			if (!m->started)
			{
				m->started = 1;
				InitTaskQueuePool(QBeam(), (void *)(TexBase() + POOL_Beam), 0x664, 1);
				InitTaskQueuePool(QGlows(), (void *)(TexBase() + 0x780), 0x14, 0x10);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x540), 0x24, 0x10);
				InitTaskQueuePool(QHits(), (void *)TexBase(), 0xA8, 8);
			}
			if (m->index <= Ctx()->actions[0].last_action)
			{
				uint8_t *ent = Entity(Ctx()->actions[m->index].targets[0]);
				const BeamNode *live = (const BeamNode *)(TexBase() + POOL_Beam);
				if ((live->hdr.flags & 1) && live->ent == ent) m->counter = 0;
				else
				{
					BeamNode *b = (BeamNode *)AddTaskToQueue(QBeam(), ORIG_BeamTask);
					if (b)
					{
						Memset32(&b->counter, 0, 0x196);
						b->index = m->index;
						b->ent = ent;
						GetEffectSpawnPosition(ent, 0xF1, 0, b->spawn);
						m->index++;
						DecodeModelPrimLayout(MODEL_Muzzle, b->muzzle, 0x3B0);
						DecodeModelPrimLayout(MODEL_Ray, b->ray, 0xE8);
						DecodeModelPrimLayout(MODEL_Impact, b->impact, 0x1B0);
					}
				}
			}
		}
		int a0, a1, a2, a3;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QBeam());
			a1 = ExecuteTaskQueue(QGlows());
			a2 = ExecuteTaskQueue(QSparks());
			a3 = ExecuteTaskQueue(QHits());
		}
		else a0 = a1 = a2 = a3 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag277_funguar_laser()
	{
		register_port(laser277::ORIG_MasterTask, (void *)laser277::MasterTask, "277 MasterTask", 277);
		register_port(laser277::ORIG_BeamTask, (void *)laser277::BeamTask, "277 BeamTask", 277);
		register_port(laser277::ORIG_HitTask, (void *)laser277::HitTask, "277 HitTask", 277);
		register_port(laser277::ORIG_SparkTask, (void *)laser277::SparkTask, "277 SparkTask", 277);
		register_port(laser277::ORIG_GlowTask, (void *)laser277::GlowTask, "277 GlowTask", 277);
		// 30 fps layer: see mag277_funguar_laser_held.inc
		FX_HELD(register_mag277_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag277_funguar_laser_held.inc"
#endif
