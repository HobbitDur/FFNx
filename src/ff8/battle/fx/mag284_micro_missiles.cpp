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

// Effect 284: Micro Missiles (enemy attack 201 of kernel.bin; GIM52A c0m001; MAG_284_*).
//
// The module 0x67B7A0..0x67C400 is the enemy twin of Quistis' Micro Missiles 299 (mag299_micro_missiles_quistis.cpp): the
// prim callback 0x67C010 is Shotgun 281's 0x67E2D0; it calls Shotgun's random bone point 0x67E550, the direction helper
// 0x6A6020 and MAG_210's 0x67BB70 across modules (ported here as RandomBonePoint / Direction / ArcOffset, as in 299); the
// master, missile, hit and spark tasks are 299's with other numbers (all six missiles at once, a half-circle path, no cast
// animation, the damage when every missile has ended).
//
// Structure (setup 0x67B7A0 -> _Init 0x67B7D0, file loader 0x67B7B0 = mag283-298.tim (name at 0x104E320, the file 299 loads)
// into 0x2507DB0; _Init keeps the context and the CASTER's entity (0x2507DB8), sets up the master queue 0x2507DF0 (pool
// 0x2507E00, 1 x 0x14), starts camera animation 0x1094DA4 and queues the TIM):
//   MasterTask (0x67B850) - alternates the packet arena (magic buffer + 0x47A8 / + 0x147A8); on counter 1 (once) sets up the
//     pools (missiles 6 x 0x818 at + 0x1518, hits 6 x 0x144 at + 0xD80, sparks 0x60 x 0x24 at + 0) and launches six missiles
//     k = 0..5 from the caster's position pushed -500 along its facing (0x67BB70 of entity +0x0E), at its height (entity +0x24)
//     - 1000, aimed at a random bone of action 0's first target (CRT rand: bone x the bone count >> 15, retried while the bone's
//     word +0 is negative); missiles 1 / 4 miss (the aim pushed by (rand & 0x7FF) - 0x400 in x / z, y = the target's entity
//     +0x24). Missile k's step is 0x1000 / (20 + 4k) (0xCC .. 0x66), its frame the basis of (launch - aim) x RotZ(k * 0x100 -
//     0x280), its length the distance. From then on the missile, hit and spark queues; the damage (action 0's first target
//     record) when the missile queue is first empty; sound 0x1094F98 at counter 2; ends when the three queues are empty.
//   MissileTask (0x67BBB0) - progress t (+0x2A) from step by step a tick; the path point at s = start + R x (0, y(s), z(s))
//     with d = length x s >> 12, y = -(sin(s / 2) x d) >> 12, z = -(cos(s / 2) x d) >> 12 (a half circle). Every tick a trail
//     of up to 12 sprites (sequence 0x108FC14, frames 0xB..0) at s = t - 0x4C8 + 0x66 i (0 <= s < 0x1000; the GTE path, camera
//     composed, identity rotation); while t <= 0x1000 the head layout 0x108FEE4 at the path point (float path 0x56C3D0), its
//     frame the basis of the step from the last point, composed with the camera; when t reaches 0x1000: the hit task (layout
//     0x1092478 and the target's chain transformation 4, miss 0x109374C, at the point, the step's basis flattened) and 16
//     sparks; ends at t >= 0x14C8 (the trail runs out).
//   HitTask (0x67C300) - its layout composed with the camera (depth -0x80); counters 0..0xD a flash (sequence 0x108FD58, frame
//     = the counter) at the point, identity rotation; ends when the layout has ended after counter 0xE.
//   SparkTask (0x67C240) - sequence 0x108FAD0 through the shadow-rotation camera (0x400, -0x100), frame = the counter; moves by
//     its velocity (random direction 0x6A6020 x (0x80 + rand & 0x3F) / 4096), the velocity drags by -1/16 of the first; 12 frames.
//   PrimCallback (0x67C010) - one layout object (Shotgun's).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the missile draws its trail, then (update) its head; the hit draws its layout then its flash; the spark draws then
// moves.
// ENTITY WRITES: the target (action 0's first) - chain transformation 4 at the impacts of the hitting missiles (engine), its
// action result (record 0) when every missile has ended (engine). No entity position word is written (entity +0x0E / +0x1C /
// +0x20 / +0x24 / bones only read).
// CAMERA: the setup's camera animation 0x1094DA4 (engine); no camera word is written by the tasks.
// Module globals: 0x2507DB0..0x2507E1C (texture file 0x2507DB0, context 0x2507DB4, caster 0x2507DB8, the queues 0x2507DC0
// sparks / 0x2507DD0 hits / 0x2507DE0 missiles / 0x2507DF0 master, master pool 0x2507E00, packet cursor 0x2507E14, magic
// buffer base 0x2507E18).
// STACK WORDS: the missile frame's pad word (+0x1E, read only into the GTE R33 control word's high half by the trail draws) is
// Matrix3x3_MultiplyPSX's (0x56C090) never-written temp pad (as in 299): it aligns its frame to 8 bytes, and under the
// master's frame the pad is the high half of a missile-pool node pointer the launch's earlier calls left there (A/B runs:
// 0x020E with the pool at 0x020E....): modelled as the missile node's high half in LaunchMissile (RESIDUE). The launch
// point's 4th word only fills the missile's +0x2A / +0x32 before they are set. The 4th words of the vectors loaded into GTE V0
// (trail, callback) reach the GTE's VZ0 high half (0x1CA8A16) only; 0x67BB70's out[1] is never read (set after). The missile
// task reuses its argument slot for the trail frame counter (the executor never reads that slot again).

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace micromissiles284
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507DB4); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507DB8); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507DC0; }         // pool: magic buffer, 0x60 x 0x24
	inline TaskQueue *QHits() { return (TaskQueue *)0x2507DD0; }           // pool: + 0xD80, 6 x 0x144
	inline TaskQueue *QMissiles() { return (TaskQueue *)0x2507DE0; }       // pool: + 0x1518, 6 x 0x818
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507E14); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507E18); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x67B850;
	static const uint32_t ORIG_MissileTask = 0x67BBB0;
	static const uint32_t ORIG_HitTask = 0x67C300;
	static const uint32_t ORIG_SparkTask = 0x67C240;
	static const uint32_t ORIG_PrimCallback = 0x67C010;   // not a task: the layouts' callback
	static const uint32_t POOL_Hits = 0xD80, POOL_Missiles = 0x1518;
	static const uint32_t MORPH = 0x45A8;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x47A8, ARENA_B = 0x147A8; // magic buffer: packet arenas
	static const uint32_t MODEL_Missile = 0x108FEE4;     // prim-model layouts
	static const uint32_t MODEL_Hit = 0x1092478;
	static const uint32_t MODEL_Miss = 0x109374C;
	static const uint32_t SEQ_Trail = 0x108FC14;         // sprite sequences
	static const uint32_t SEQ_Flash = 0x108FD58;
	static const uint32_t SEQ_Spark = 0x108FAD0;
	static const void *const SOUND_Launch = (const void *)0x1094F98;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline int32_t BuildOrthonormalBasis(int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); } // returns |v|
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }                // MAG_065_RotationZMatrix
	inline void Matrix3x3MultiplyPSX(const Mat4x3 *a, const Mat4x3 *b, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, const Mat4x3 *, Mat4x3 *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX: out = a x b (3x3)
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *v, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, v, out); } // apply3DTransform: rotation only (float)
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507E00)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F 1 missiles launched, 2 action result applied
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MissileNode // pool of 6 nodes of 0x818 bytes
	{
		TaskNode hdr;
		int16_t rot[9];    // +0x0C the missile frame R (3x3; 0x56C090 / 0x7012C0 write +0x0C..+0x1F)
		int16_t pad1E;
		int16_t dist;      // +0x20 launch -> aim distance
		int16_t type;      // +0x22 0 hit (target chain 4), 1 miss, -1 impact done
		int16_t start[3];  // +0x24 launch point
		int16_t t;         // +0x2A progress (step.., the head while <= 0x1000, ends at 0x14C8)
		int16_t cur[3];    // +0x2C the last head point
		int16_t step;      // +0x32 0x1000 / (20 + 4k)
		uint8_t layout[0x7E4]; // +0x34 head layout
	};
	struct HitNode // pool of 6 nodes of 0x144 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		Mat4x3 m;          // +0x10 the impact frame (translation = the impact point)
		uint8_t layout[0x114]; // +0x30 hit (0xA0) / miss (0x114) layout
	};
	struct SparkNode // pool of 0x60 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t pad1A;
		int16_t drag[3];   // +0x1C -velocity / 16, added to the velocity every tick
		int16_t pad22;
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MissileNode) == 0x818 && sizeof(HitNode) == 0x144 && sizeof(SparkNode) == 0x24, "Micro Missiles nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Micro Missiles prim block");

	inline Mat4x3 *Frame(MissileNode *m) { return (Mat4x3 *)m->rot; }
	inline const Mat4x3 *Frame(const MissileNode *m) { return (const Mat4x3 *)m->rot; }

	// ------------------------------------------------------------------
	// 0x67BB70 (MAG_210_sub_67BB70, called across modules): out[0] = sin(a) r, out[2] = -cos(a) r (>> 12; out[1] untouched)
	// ------------------------------------------------------------------
	static void ArcOffset(int32_t a, int32_t r, int16_t *out)
	{
		out[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
		out[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(a), r)) >> 12);
	}

	// 0x67E550 (Shotgun 281's MAG_281_Shotgun_RandomBonePoint, called across modules): bone idx of the entity in world space (out:
	// 3 words); 1 = the bone's word +0 is negative (no point, out untouched)
	static int RandomBonePoint(uint8_t *ent, int32_t idx, int16_t *out)
	{
		int32_t *blk = (int32_t *)FieldAlloc(0x14);
		const uint8_t *skel = **(uint8_t *const *const *)(ent + 0x64);
		if (idx >= 0xF0) idx = skel[idx - 0xEC];
		const uint8_t *bone = skel + 0x10 + idx * 0x30;
		if (*(const int16_t *)bone < 0)
		{
			FieldFree(0x14);
			return 1;
		}
		TransformVectorBy3x3Matrix((const Mat4x3 *)(ent + 0x40), (const int32_t *)(bone + 0x24), blk);
		out[0] = (int16_t)(*(const int16_t *)(ent + 0x54) + (int16_t)blk[0]);
		out[1] = (int16_t)(*(const int16_t *)(ent + 0x58) + (int16_t)blk[1]);
		out[2] = (int16_t)(*(const int16_t *)(ent + 0x5C) + (int16_t)blk[2]);
		FieldFree(0x14);
		return 0;
	}

	// 0x6A6020 (Effect_DirectionFromPitchYaw, called across modules): unit direction of the angles a (elevation) and b (heading)
	static void Direction(int32_t a, int32_t b, int16_t *out)
	{
		const int32_t c = ComputeCos(a);
		out[1] = (int16_t)(0 - ComputeSin(a));
		out[0] = (int16_t)(mul32(ComputeSin(b), c) >> 12);
		out[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(b), c)) >> 12);
	}

	// ------------------------------------------------------------------
	// The missile path (0x67BBB0): the half-circle offset at progress s in the missile frame
	// ------------------------------------------------------------------
	static void PathOffset(const MissileNode *m, int32_t s, int16_t v[4])
	{
		const int32_t d = mul32(m->dist, s) >> 12;
		const int32_t h = s >> 1;
		v[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeSin(h), d)) >> 12);
		v[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(h), d)) >> 12);
		v[0] = 0;
		v[3] = 0; // (never written: only loaded into the GTE V0 pad)
	}

	// the trail's world point at s: the GTE path (R through the GTE, + the launch point)
	static void TrailPoint(const MissileNode *m, int32_t s, int32_t p[3])
	{
		int16_t v[4];
		PathOffset(m, s, v);
		GteSetRotMatrixCtrl(Frame(m));
		GteLoadV0(v);
		GteMVMVA_RotV0();
		GteReadMAC123(p);
		p[0] = (int32_t)((uint32_t)p[0] + (uint32_t)(int32_t)m->start[0]);
		p[2] = (int32_t)((uint32_t)p[2] + (uint32_t)(int32_t)m->start[2]);
		p[1] = (int32_t)((uint32_t)p[1] + (uint32_t)(int32_t)m->start[1]);
	}

	// one trail sprite: the world point through the camera (identity rotation), frame k
	static void TrailDraw(uint8_t *h, const int32_t p[3], int16_t frame)
	{
		Mat4x3 view = {};
		view.m[0][0] = 0x1000;
		view.m[1][1] = 0x1000;
		view.m[2][2] = 0x1000;
		TransformVectorBy3x3Matrix(&Camera(), p, view.t);
		view.t[0] = (int32_t)((uint32_t)view.t[0] + (uint32_t)Camera().t[0]);
		view.t[2] = (int32_t)((uint32_t)view.t[2] + (uint32_t)Camera().t[2]);
		view.t[1] = (int32_t)((uint32_t)view.t[1] + (uint32_t)Camera().t[1]);
		GteSetRotMatrixCtrl(&view);
		GteSetTransVectorCtrl(&view);
		*(uint32_t *)h = SEQ_Trail;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	// the head's point at progress t (the float path 0x56C3D0, + the launch point)
	static void HeadPoint(const MissileNode *m, int32_t t, int32_t p[3])
	{
		int16_t v[4];
		PathOffset(m, t, v);
		int32_t o[3];
		Apply3DTransform(Frame(m), v, o);
		p[0] = (int32_t)((uint32_t)o[0] + (uint32_t)(int32_t)m->start[0]);
		p[1] = (int32_t)((uint32_t)o[1] + (uint32_t)(int32_t)m->start[1]);
		p[2] = (int32_t)((uint32_t)o[2] + (uint32_t)(int32_t)m->start[2]);
	}

	// the step from the last head point to the new one (the basis direction): x / z straight, y signed by the z step
	static void HeadStep(const int16_t cur[3], const int32_t p[3], int32_t v[3])
	{
		v[0] = (int32_t)((uint32_t)p[0] - (uint32_t)(int32_t)cur[0]);
		v[2] = (int32_t)((uint32_t)(int32_t)cur[2] - (uint32_t)p[2]);
		v[1] = v[2] > 0 ? (int32_t)((uint32_t)p[1] - (uint32_t)(int32_t)cur[1]) : (int32_t)((uint32_t)(int32_t)cur[1] - (uint32_t)p[1]);
	}

	// the head's parameter block: the step's basis at the point, composed with the camera
	static void HeadArg(const int32_t p[3], int32_t v[3], PrimArg *arg)
	{
		arg->m.t[0] = p[0];
		arg->m.t[1] = p[1];
		arg->m.t[2] = p[2];
		BuildOrthonormalBasis(v, &arg->m);
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		arg->depth = 0;
		arg->morph = TexBase() + MORPH;
	}

	// the hit's parameter block / flash frame
	static void HitArg(const HitNode *h, PrimArg *arg)
	{
		ComposeAffineTransform(&Camera(), &h->m, &arg->m);
		arg->depth = -0x80;
		arg->morph = TexBase() + MORPH;
	}

	// the flash: the block's frame with an identity rotation (written into the block)
	static void FlashDraw(PrimArg *arg, int16_t frame)
	{
		arg->m.m[0][0] = 0x1000; arg->m.m[0][1] = 0; arg->m.m[0][2] = 0;
		arg->m.m[1][0] = 0; arg->m.m[1][1] = 0x1000; arg->m.m[1][2] = 0;
		arg->m.m[2][0] = 0; arg->m.m[2][1] = 0; arg->m.m[2][2] = 0x1000;
		GteSetRotMatrix(&arg->m);
		GteSetTransVector(&arg->m);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Flash;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, 0x400, -0x100);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag284_micro_missiles_held.h"
#endif

namespace ff8fx
{
namespace micromissiles284
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x67C010, Shotgun 281's 0x67E2D0): one object of a layout. Vertex frame (or blend into the parameter
	// block's morph buffer), rotation from the record's angles, the record's offset through the parameter matrix (flag 0x200: as
	// is), the matrix composed with it (unless flag 0x200 or 0x8000), scale (scale3DMatrix), fade, drawn by 0x7043B0
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
		PacketCursor() = RenderPrim2(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Spark (0x67C240)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag284_micro_missiles_held.inc
		FX_HELD(held_note_spark(s);)
		SparkDraw(s);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		const int16_t vx = (int16_t)(s->drag[0] + s->vel[0]);
		const int16_t vy = (int16_t)(s->drag[1] + s->vel[1]);
		const int16_t vz = (int16_t)(s->drag[2] + s->vel[2]);
		s->counter++;
		s->vel[0] = vx;
		s->vel[1] = vy;
		s->vel[2] = vz;
		return s->counter < 0xC ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Hit (0x67C300)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		HitArg(h, &arg);
		// 30 fps layer: see mag284_micro_missiles_held.inc
		FX_HELD(held_note_hit(h);)
		const int left = prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0);
		if (h->counter < 0xE)
		{
			// 30 fps layer: see mag284_micro_missiles_held.inc
			FX_HELD(held_note_flash(h);)
			FlashDraw(&arg, h->counter);
			h->counter++;
			return 0;
		}
		if (left)
		{
			h->counter++;
			return 0;
		}
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Missile (0x67BBB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MissileTask(TaskNode *n)
	{
		MissileNode *m = (MissileNode *)n;
		// 30 fps layer: see mag284_micro_missiles_held.inc
		FX_HELD(held_note_missile(m);)
		uint8_t *h = AllocHeader(0xB4);
		int32_t s = m->t - 0x4C8;
		for (int16_t frame = 0xB; ; frame--)
		{
			if (s >= 0 && s < 0x1000)
			{
				int32_t p[3];
				TrailPoint(m, s, p);
				TrailDraw(h, p, frame);
			}
			s += 0x66;
			if (s >= 0x1000 || frame == 0) break;
		}
		FieldFree(0xB4);
		int32_t v[3] = {};
		const int32_t t = m->t;
		if (t <= 0x1000)
		{
			int32_t p[3];
			HeadPoint(m, t, p);
			HeadStep(m->cur, p, v);
			m->cur[0] = (int16_t)p[0];
			m->cur[2] = (int16_t)p[2];
			m->cur[1] = (int16_t)p[1];
			PrimArg arg;
			HeadArg(p, v, &arg);
			prim::play((prim::Layout *)m->layout, PrimCallback, (int)&arg, 0);
		}
		m->t = (int16_t)(m->t + m->step);
		if (m->t >= 0x1000 && m->type != -1)
		{
			HitNode *hit = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
			if (hit)
			{
				hit->counter = 0;
				v[1] = 0;
				BuildOrthonormalBasis(v, &hit->m);
				hit->m.t[0] = m->cur[0];
				hit->m.t[1] = m->cur[1];
				hit->m.t[2] = m->cur[2];
				if (m->type == 0)
				{
					DecodeModelPrimLayout(MODEL_Hit, hit->layout, 0xA0);
					QueueChainTransformation(Entity(Ctx()->actions[0].targets[0]), 4);
				}
				else DecodeModelPrimLayout(MODEL_Miss, hit->layout, 0x114);
			}
			for (int k = 0; k < 0x10; k++)
			{
				SparkNode *sp = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				if (!sp) continue;
				memcpy(sp->pos, m->cur, 4);
				memcpy(&sp->pos[2], &m->cur[2], 4);
				sp->counter = 0;
				const int32_t b = CrtRand();
				const int32_t a = CrtRand();
				Direction(a, b, sp->vel);
				GteSetIR0((CrtRand() & 0x3F) + 0x80);
				GteLoadIR123(sp->vel);
				GteGPF();
				GteStoreIR123(sp->vel);
				sp->drag[0] = (int16_t)((int32_t)(0 - (int32_t)sp->vel[0]) >> 4);
				sp->drag[1] = (int16_t)((int32_t)(0 - (int32_t)sp->vel[1]) >> 4);
				sp->drag[2] = (int16_t)((int32_t)(0 - (int32_t)sp->vel[2]) >> 4);
			}
			m->type = -1;
		}
		return m->t < 0x14C8 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x67B850)
	// ------------------------------------------------------------------
	// missile k: from the launch point pos, step 0x1000 / div, aimed at a random bone of the target
	static void LaunchMissile(int32_t k, int32_t div, uint8_t *target, const int16_t pos[4], bool align4)
	{
		MissileNode *m = (MissileNode *)AddTaskToQueue(QMissiles(), ORIG_MissileTask);
		memcpy(m->cur, pos, 4);
		memcpy(&m->cur[2], &pos[2], 4);
		memcpy(m->start, pos, 4);
		memcpy(&m->start[2], &pos[2], 4);
		const int16_t step = (int16_t)(0x1000 / div);
		m->step = step;
		m->t = step;
		int16_t aim[4];
		while (RandomBonePoint(target, mul32(CrtRand(), ***(const uint8_t *const *const *)(target + 0x64)) >> 15, aim)) {}
		if (k % 3 == 1)
		{
			aim[0] = (int16_t)(aim[0] + (CrtRand() & 0x7FF) - 0x400);
			aim[1] = *(const int16_t *)(target + 0x24);
			aim[2] = (int16_t)(aim[2] + (CrtRand() & 0x7FF) - 0x400);
			m->type = 1;
		}
		else m->type = 0;
		int32_t v[3] = { (int32_t)pos[0] - aim[0], (int32_t)pos[1] - aim[1], (int32_t)aim[2] - pos[2] };
		Mat4x3 basis;
		m->dist = (int16_t)BuildOrthonormalBasis(v, &basis);
		DecodeModelPrimLayout(MODEL_Missile, m->layout, 0x7E4);
		RotZ(shl32(k, 8) - 0x280, Frame(m));
		Matrix3x3MultiplyPSX(&basis, Frame(m), Frame(m));
		// RESIDUE (see STACK WORDS): 0x56C090 copies its 8-aligned temp's never-written pad word into +0x1E
		m->pad1E = align4 ? (int16_t)0x8000 : (int16_t)((uint32_t)m >> 16);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		// 30 fps layer: see mag284_micro_missiles_held.inc
		FX_HELD(held_note_master();)
		if (mn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			mn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			mn->arena = 1;
		}
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			InitTaskQueuePool(QMissiles(), (void *)(TexBase() + POOL_Missiles), 0x818, 6);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x144, 6);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x24, 0x60);
			uint8_t *target = Entity(Ctx()->actions[0].targets[0]);
			int16_t pos[4];
			ArcOffset(*(const int16_t *)(Caster() + 0xE), -500, pos);
			uint8_t *caster = Caster();
			pos[0] = (int16_t)(pos[0] + *(const int16_t *)(caster + 0x1C));
			pos[2] = (int16_t)(pos[2] + *(const int16_t *)(caster + 0x20));
			pos[1] = (int16_t)(*(const int16_t *)(caster + 0x24) - 1000);
			pos[3] = 0; // (never written: only fills +0x2A / +0x32 before they are set)
			// RESIDUE (see STACK WORDS): what the pad word holds depends on the master's stack alignment (its argument's address)
			const bool align4 = ((uint32_t)&n & 4) != 0;
			for (int32_t k = 0, div = 0x14; div < 0x2C; div += 4, k++) LaunchMissile(k, div, target, pos, align4);
		}
		int a0, a1, a2;
		if (mn->started)
		{
			a0 = ExecuteTaskQueue(QMissiles());
			if (a0 == 0 && mn->started == 1)
			{
				ApplyActionResultToTarget(Ctx()->actions[0].targets);
				mn->started = 2;
			}
			a1 = ExecuteTaskQueue(QHits());
			a2 = ExecuteTaskQueue(QSparks());
		}
		else a0 = a1 = a2 = (int)n;
		if (mn->counter == 2) BdPlaySE(SOUND_Launch, 0, 0x80);
		if (mn->started && !a0 && !a1 && !a2) return TASK_END;
		mn->counter++;
		return 0;
	}
}

	void register_mag284_micro_missiles()
	{
		register_port(micromissiles284::ORIG_MasterTask, (void *)micromissiles284::MasterTask, "284 MasterTask", 284);
		register_port(micromissiles284::ORIG_MissileTask, (void *)micromissiles284::MissileTask, "284 MissileTask", 284);
		register_port(micromissiles284::ORIG_HitTask, (void *)micromissiles284::HitTask, "284 HitTask", 284);
		register_port(micromissiles284::ORIG_SparkTask, (void *)micromissiles284::SparkTask, "284 SparkTask", 284);
		// 30 fps layer: see mag284_micro_missiles_held.inc
		FX_HELD(register_mag284_held();)
	}
	FX_LAB_MODULE(register_mag284_micro_missiles);
}

#ifdef FF8_FX_HELD
#include "mag284_micro_missiles_held.inc"
#endif
