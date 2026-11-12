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

// Effect 302: Snipe Laser (enemy attack 155 of kernel.bin; GIM47N c0m008; MAG_302_*). No other kernel.bin entry uses
// effect 302. A Funguar laser (effect 277) variant: two beams from two points of the caster's skeleton, no muzzle layout
// drawn, the hits / glows / impact / sparks of beam 0 only, the sound from the master.
//
// Structure (setup MAG_302 0x66CA50 -> _Init 0x66CA80 (not ported), file loader 0x66CA60 = the texture file named at
// 0x103BBC8 (277's name) -> [0x2507664]; the setup queues the TIM, keeps the CASTER's entity (0x2507660), starts camera
// animation 0x103B914 and the master in its 1-node pool 0x2507670):
//   MasterTask (0x66CB00) - alternates the packet arena (magic buffer + 0x1710 / + 0x11710); on counter 1 sets up the pools
//     once (beams 2 x 0x664 at + 0x8C0, glows 0x10 x 0x14 at + 0x780, sparks 0x10 x 0x24 at + 0x540, hits 8 x 0xA8 at + 0)
//     and, if its action index <= action 0's last_action, starts TWO beams at the first target of that action (beam k:
//     index k, counter -2 k, the target's bone 0xF1 as its spawn point; the action index steps once per beam started).
//     Then the beam, glow, spark and hit queues; sound 0x103BBB0 at counter 0x14; ends when all four are empty.
//   BeamTask (0x66CD40) - waits while its counter is negative; counters 0..0x32. Frame: P = the point 0x103BBB8 + 8 k
//     (x, y, z, bone) through the caster's bone (0x66D880 = Brrawghh 289's CasterBonePoint 0x678390: the bone matrices
//     as they are), T = the target's default effect position, basis M of (T - P) in the floor plane (0x50CBA0), R = M
//     with rows 0 / 2 negated.
//       counters 5..0x2C: 0x66D810 puts P through the basis of the same direction and the camera (GTE work only: the
//         muzzle layout it is handed is never played); T lifted by 277's ease; the basis M2 of (T - P) with P as its
//         translation, composed with the camera: the ray layout 0x1039B1C (depth 0);
//       counters 15..0x2C: T at or below the floor moved onto it along P -> T; the beam = prim model 0x103462C in
//         M2 x diag(w, w, L) as 277's; spawns: T on the floor -> a hit layout 0x103AD7C (R, T); T above the floor but
//         not above the target's word +0x92, on even counters, when T is in the target's lowest eighth or for beam 0 ->
//         a glow sprite;
//       beam 0 only: counters 0x21..0x32 the impact layout 0x1035CF4 in R at (spawn x, target +0x92 / 2, spawn z)
//         composed with the camera (depth -0x80); counters 0x24 / 0x25: 8 sparks each (CRT rand, drag velocity / -12);
//         the damage of action 0's first target record at 0x2B.
//   HitTask (0x66D670), SparkTask (0x66D6D0, sequence 0x1034480), GlowTask (0x66D790, sequence 0x1034270): 277's code
//     (0x684C40 / 0x684CA0 / 0x684D60) on this module's globals (HitTask: its blend buffer at magic buffer + 0x1588).
//   PrimCallback (0x66D3C0) = 277's 0x684870 on this module's cursor: draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: nothing written (the caster's bone matrices and the target's entity are only read; the camera animation
// is the engine's).
// Module globals: 0x2507660..0x25076D8 (caster entity 0x2507660, texture file 0x2507664, magic buffer base 0x2507668,
// context 0x250766C, master pool 0x2507670, packet cursor 0x2507684, the queues 0x2507688 hits / 0x2507698 sparks /
// 0x25076A8 glows / 0x25076B8 beams / 0x25076C8 master).
// NEVER-WRITTEN STACK WORDS: as 277's (the 4th words of the vectors loaded into GTE V0 reach VZ0's high half only, the
// matrices' pad words are never read, the beam's prim header keeps the scratch's +0x04 / +0x18 (+0x08..+0x0C without a
// fade)); 0x66D810's MAC output is never read.
// Fujin's described Snipe Laser (effect_302.json) runs this code: its memory through own::R() (fx_port.h), its spawns
// through own::Fn(), the setup and the file loader ported below (277's); its parameters (SnipeLaserParams, Funguar
// laser's layout: the sound, the layouts, flipbooks, the beam mesh, the texture name and the camera track; and the two
// beams' points on the caster).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace snipe302
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2507660)); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2507668)); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x250766C)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2507684)); }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2507664)); }          // the effect's file (its TIM)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x25076C8); }           // 1 x 0x14
	inline TaskQueue *QHits() { return (TaskQueue *)own::R(0x2507688); }           // pool: magic buffer, 8 x 0xA8
	inline TaskQueue *QSparks() { return (TaskQueue *)own::R(0x2507698); }         // pool: + 0x540, 0x10 x 0x24
	inline TaskQueue *QGlows() { return (TaskQueue *)own::R(0x25076A8); }          // pool: + 0x780, 0x10 x 0x14
	inline TaskQueue *QBeam() { return (TaskQueue *)own::R(0x25076B8); }           // pool: + 0x8C0, 2 x 0x664

	static const uint32_t ORIG_MasterTask = 0x66CB00;
	static const uint32_t ORIG_BeamTask = 0x66CD40;
	static const uint32_t ORIG_HitTask = 0x66D670;
	static const uint32_t ORIG_SparkTask = 0x66D6D0;
	static const uint32_t ORIG_GlowTask = 0x66D790;
	static const uint32_t MORPH = 0x1588;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1710;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11710;
	static const uint32_t POOL_Beam = 0x8C0;
	static const uint32_t MODEL_Muzzle = 0x1036F60;      // prim-model layouts (the muzzle is decoded, never played)
	static const uint32_t MODEL_Ray = 0x1039B1C;
	static const uint32_t MODEL_Impact = 0x1035CF4;
	static const uint32_t MODEL_Hit = 0x103AD7C;
	static const uint32_t PRIM_Beam = 0x103462C;         // prim model (0x572200)
	static const uint32_t SEQ_Spark = 0x1034480;         // sprite sequences
	static const uint32_t SEQ_Glow = 0x1034270;
	static const void *const SOUND_Beam = (const void *)0x103BBB0;
	static const int16_t *const BEAM_POINTS = (const int16_t *)0x103BBB8; // beam k: x, y, z, caster bone
	static const uint32_t ORIG_FileLoader = 0x66CA60, ORIG_Setup = 0x66CA50;
	static const uint32_t TEXTURE_NAME = 0x103BBC8, CAMERA_ANIM = 0x103B914;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h): Funguar laser's (277) layout, and the beams' points
	// ------------------------------------------------------------------
	struct SnipeLaserParams
	{
		uint32_t sound;   // the master's counter 0x14
		struct { uint32_t hit, muzzle, ray, impact; } layouts; // the prim-model layouts played (their models' animation)
		struct { uint32_t spark, glow; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t beam; } model_data; // the prim models drawn (meshes)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track, TIMs, animation sets
		struct { uint32_t beam_points; } tables; // its small tables (data): the two beams' points on the caster
	};
	static const SnipeLaserParams GAME = { 0x103BBB0, { MODEL_Hit, MODEL_Muzzle, MODEL_Ray, MODEL_Impact }, { SEQ_Spark, SEQ_Glow }, { PRIM_Beam }, { TEXTURE_NAME, CAMERA_ANIM }, { (uint32_t)BEAM_POINTS } };
	static SnipeLaserParams live = GAME;
	static const SnipeLaserParams &P() { return live; }

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
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507670)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t index;     // +0x0E next action to start beams for (steps once per beam started)
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct BeamNode // pool of 2 nodes of 0x664 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C negative: ticks left to wait
		int16_t index;     // +0x0E beam 0 / 1 (its point 0x103BBB8 + 8 index)
		uint8_t *ent;      // +0x10 target entity (first target of the master's action)
		int16_t spawn[4];  // +0x14 target bone 0xF1 at the spawn
		uint8_t muzzle[0x3B0];   // +0x1C (decoded, never played)
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
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BeamNode) == 0x664 && sizeof(HitNode) == 0xA8, "Snipe Laser nodes");
	static_assert(sizeof(SparkNode) == 0x24 && sizeof(GlowNode) == 0x14, "Snipe Laser nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Snipe Laser prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x66D3C0 = Funguar laser 277's 0x684870, the same code): one object of a layout. Vertex frame (or blend into the
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

	// 0x66D880 (= Brrawghh 289's CasterBonePoint 0x678390): a point (x, y, z, bone) through the caster's bone composed
	// with its matrix (the bone matrices as they are)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x66D810: v through the basis M, plus P (v is rewritten), then through the camera rotation; the MAC result is read
	// into a local nobody reads (the muzzle layout it is handed is never played): GTE work only
	static void MuzzleFrame(const Mat4x3 *M, const int16_t *P, int16_t *v)
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
	}

	// the beam's frame (0x66CD5E..0x66CE4C): P = beam k's point through the caster's bone, T = the target's default
	// effect position, v = (T - P) in the floor plane, M its basis, R = M with rows 0 / 2 negated
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
		BonePoint((const int16_t *)fxp::data(P().tables.beam_points, (uint32_t)BEAM_POINTS, 16) + 4 * b->index, f->P);
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

	// counters 5..0x2C: the target lift, the beam's width angle and its fade (0x66CE88..0x66CF1A, = 277's 0x68433C..0x6843D4);
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

	// counters 15..0x2C: T at or below the floor (y >= 0) onto the floor along P -> T (0x66CFD9..0x66D031, = 277's)
	static void FloorHit(const int16_t *P, int16_t *T)
	{
		const int32_t dy = (int32_t)T[1] - P[1];
		const int32_t ax = mul32((int32_t)P[0] - T[0], P[1]);
		const int32_t az = mul32((int32_t)P[2] - T[2], P[1]);
		T[0] = (int16_t)(ax / dy + P[0]);
		T[2] = (int16_t)(az / dy + P[2]);
		T[1] = 0;
	}

	// the beam (0x66D03F..0x66D15F): the basis M2 of (T - P), its length; the spawn kind (0 none, 1 hit layout, 2 glow
	// sprite on even counters: T in the target's lowest eighth, or anywhere above the floor for beam 0) for the caller
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
		else if (((int32_t)T[1] >= (int32_t)top - ((int32_t)top >> 3) || b->index == 0) && !(counter & 1)) d->kind = 2;
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
		*(uint32_t *)h = fxp::model(P().model_data.beam, PRIM_Beam);
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

	// the impact frame (0x66D238..0x66D257, = 277's): R at (spawn x, target +0x92 / 2, spawn z)
	static void ImpactFrame(const BeamNode *b, Mat4x3 *R)
	{
		R->t[0] = b->spawn[0];
		R->t[1] = *(const int16_t *)(b->ent + 0x92) / 2;
		R->t[2] = b->spawn[2];
	}

	// the sprites (0x66D6D3..0x66D732 / 0x66D792..0x66D7F0, = 277's SparkTask / GlowTask)
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
#include "mag302_snipe_laser_held.h"
#endif

namespace ff8fx
{
namespace snipe302
{
	// ------------------------------------------------------------------
	// Glow (0x66D790 = 277's 0x684D60)
	// ------------------------------------------------------------------
	// File loader (0x66CA60) and setup (0x66CA50 -> _Init 0x66CA80, Funguar laser's code): the game runs its own; Fujin's
	// programs these (its memory may be a copy's): the TIM, the magic buffer, the cast and its caster's entity, the
	// master (it sets up the rest), the camera
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = (uint32_t)layouts::MagicBuffer();
		Ctx() = ctx;
		Caster() = (uint8_t *)(0x1D972C0 + 0x9C * ctx->attacker);
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2507670), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 2);
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	static uint32_t __cdecl GlowTask(TaskNode *n)
	{
		GlowNode *g = (GlowNode *)n;
		// 30 fps layer: see mag302_snipe_laser_held.inc
		FX_HELD(held_note_sprite(SPR_GLOW, n, g->pos, nullptr, g->counter);)
		SpriteDraw(g->pos, g->counter, fxp::sequence(P().seqs.glow, SEQ_Glow), 0xAF0, -0x400);
		g->counter++;
		return g->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spark (0x66D6D0 = 277's 0x684CA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag302_snipe_laser_held.inc
		FX_HELD(held_note_sprite(SPR_SPARK, n, s->pos, s->vel, s->counter);)
		SpriteDraw(s->pos, s->counter, fxp::sequence(P().seqs.spark, SEQ_Spark), 0xC00, -0x300);
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
	// Hit (0x66D670 = 277's 0x684C40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &h->frame, &arg.m);
		arg.depth = 0;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag302_snipe_laser_held.inc
		FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)h->layout);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Beam (0x66CD40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		if (b->counter < 0)
		{
			b->counter++;
			return 0;
		}
		BeamFrame f;
		BuildFrame(b, &f);
		const int32_t c = b->counter;
		PrimArg arg;
		if ((uint32_t)(c - 5) < 0x28)
		{
			Mat4x3 M;
			BuildOrthonormalBasis(f.v, &M);
			int16_t v[4] = {};
			MuzzleFrame(&M, f.P, v);
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
			// 30 fps layer: see mag302_snipe_laser_held.inc
			FX_HELD(held_note_play(PLAY_RAY, n, (prim::Layout *)b->ray);)
			prim::play((prim::Layout *)b->ray, PrimCallback, (int)&ray, 0);
			if ((uint32_t)(c - 15) < 0x28)
			{
				if (T[1] >= 0) FloorHit(f.P, T);
				BeamShape(b, c, f.P, T, &d);
				if (d.kind == 2)
				{
					GlowNode *g = (GlowNode *)AddTaskToQueue(QGlows(), own::Fn(ORIG_GlowTask));
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
					HitNode *h = (HitNode *)AddTaskToQueue(QHits(), own::Fn(ORIG_HitTask));
					if (h)
					{
						memcpy(&h->frame.m, &f.R.m, sizeof(h->frame.m));
						h->frame.t[0] = T[0];
						h->frame.t[1] = T[1];
						h->frame.t[2] = T[2];
						DecodeModelPrimLayout(fxp::layout(P().layouts.hit, MODEL_Hit, 0x7C), h->layout, 0x7C);
					}
				}
				FX_HELD(held_note_beam(n);)
				BeamRender(&d.M2, d.len, BeamWidth(ez.width), ez.fade);
			}
		}
		if (b->index == 0)
		{
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
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
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
		}
		if (b->index == 0 && b->counter == 0x2B) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (b->counter >= 0x32) return TASK_END;
		b->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x66CB00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag302_snipe_laser_held.inc
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
				InitTaskQueuePool(QBeam(), (void *)(TexBase() + POOL_Beam), 0x664, 2);
				InitTaskQueuePool(QGlows(), (void *)(TexBase() + 0x780), 0x14, 0x10);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x540), 0x24, 0x10);
				InitTaskQueuePool(QHits(), (void *)TexBase(), 0xA8, 8);
			}
			if (m->index <= Ctx()->actions[0].last_action)
			{
				uint8_t *ent = Entity(Ctx()->actions[m->index].targets[0]);
				for (int k = 0; k < 2; k++)
				{
					BeamNode *b = (BeamNode *)AddTaskToQueue(QBeam(), own::Fn(ORIG_BeamTask));
					if (!b) continue;
					Memset32(&b->counter, 0, 0x196);
					b->index = (int16_t)k;
					b->counter = (int16_t)(-k * 2);
					b->ent = ent;
					GetEffectSpawnPosition(ent, 0xF1, 0, b->spawn);
					m->index++;
					DecodeModelPrimLayout(fxp::layout(P().layouts.muzzle, MODEL_Muzzle, 0x3B0), b->muzzle, 0x3B0);
					DecodeModelPrimLayout(fxp::layout(P().layouts.ray, MODEL_Ray, 0xE8), b->ray, 0xE8);
					DecodeModelPrimLayout(fxp::layout(P().layouts.impact, MODEL_Impact, 0x1B0), b->impact, 0x1B0);
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
		if (m->counter == 0x14) BdPlaySE((const void *)fxp::sound(P().sound, (uint32_t)SOUND_Beam), 0, 0x80);
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "beam", "Beam", "Two beams from two points of the caster's skeleton to each target: the aim layouts, the beams, the hit layouts and glows, beam 0's impact layout and sparks, the sound; the action result at beam 0's 0x2B" },
		};
		static const Field FIELDS[] = {
			{ "beam", "sound", "Sound", "The master's counter 0x14", (uint32_t)offsetof(SnipeLaserParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "beam", "layouts.hit", "Hit", "The prim-model layout hit (its models' animation)", (uint32_t)offsetof(SnipeLaserParams, layouts.hit), fxp::Layout, 0, 0x7FFFFFFF },
			{ "beam", "layouts.muzzle", "Muzzle", "The prim-model layout muzzle (its models' animation; decoded into each beam, never played: no change shows)", (uint32_t)offsetof(SnipeLaserParams, layouts.muzzle), fxp::Layout, 0, 0x7FFFFFFF },
			{ "beam", "layouts.ray", "Ray", "The prim-model layout ray (its models' animation)", (uint32_t)offsetof(SnipeLaserParams, layouts.ray), fxp::Layout, 0, 0x7FFFFFFF },
			{ "beam", "layouts.impact", "Impact", "The prim-model layout impact (its models' animation)", (uint32_t)offsetof(SnipeLaserParams, layouts.impact), fxp::Layout, 0, 0x7FFFFFFF },
			{ "beam", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(SnipeLaserParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "beam", "seqs.glow", "Glow", "The sprite sequence glow (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(SnipeLaserParams, seqs.glow), fxp::Seq, 0, 0x7FFFFFFF },
			{ "beam", "model_data.beam", "Beam model", "The prim model beam (a mesh of 220 vertices, 160 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(SnipeLaserParams, model_data.beam), fxp::Model, 0, 0x7FFFFFFF },
			{ "beam", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(SnipeLaserParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "beam", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(SnipeLaserParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "beam", "tables.beam_points", "Beam points", "Where each beam starts: a point (x, y, z) and the caster's bone it is on, beam 0 then beam 1 (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(SnipeLaserParams, tables.beam_points), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag302_snipe_laser()
	{
		fxp::register_params({ 302, "Snipe Laser", &snipe302::live, &snipe302::GAME, sizeof(snipe302::SnipeLaserParams),
		                       snipe302::table::PARTS, (int)(sizeof(snipe302::table::PARTS) / sizeof(snipe302::table::PARTS[0])),
		                       snipe302::table::FIELDS, (int)(sizeof(snipe302::table::FIELDS) / sizeof(snipe302::table::FIELDS[0])),
		                       nullptr, 0 });
		own::note(snipe302::ORIG_FileLoader, (void *)snipe302::FileLoader);
		own::note(snipe302::ORIG_Setup, (void *)snipe302::Setup);
		own::note(snipe302::ORIG_MasterTask, (void *)snipe302::MasterTask);
		register_port(snipe302::ORIG_MasterTask, (void *)snipe302::MasterTask, "302 MasterTask", 302);
		own::note(snipe302::ORIG_BeamTask, (void *)snipe302::BeamTask);
		register_port(snipe302::ORIG_BeamTask, (void *)snipe302::BeamTask, "302 BeamTask", 302);
		own::note(snipe302::ORIG_HitTask, (void *)snipe302::HitTask);
		register_port(snipe302::ORIG_HitTask, (void *)snipe302::HitTask, "302 HitTask", 302);
		own::note(snipe302::ORIG_SparkTask, (void *)snipe302::SparkTask);
		register_port(snipe302::ORIG_SparkTask, (void *)snipe302::SparkTask, "302 SparkTask", 302);
		own::note(snipe302::ORIG_GlowTask, (void *)snipe302::GlowTask);
		register_port(snipe302::ORIG_GlowTask, (void *)snipe302::GlowTask, "302 GlowTask", 302);
		// 30 fps layer: see mag302_snipe_laser_held.inc
		FX_HELD(register_mag302_held();)
	}
	FX_LAB_MODULE(register_mag302_snipe_laser);
}

#ifdef FF8_FX_HELD
#include "mag302_snipe_laser_held.inc"
#endif
