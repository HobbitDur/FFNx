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

// Effect 299: Micro Missiles (Quistis' Blue Magic, kernel.bin section 19 "Micro Missiles", magic +4 = 299;
// MAG_299_MICRO_MISSILES_QUISTIS*), cast by a party member (Quistis) on an enemy.
//
// The module 0x66FCC0..0x670970 is a hand-written module: setup / loader / _Init are the copies of effects 126..131 / 300, the
// prim callback 0x670570 is Shotgun 281's 0x67E2D0 (+ the record flag 0x2000 draw mode 0x203C, the fade bits OR'd in); it calls
// Shotgun's random bone point 0x67E550, the direction helper 0x6A6020 and MAG_210's 0x67BB70 across modules (ported here as
// RandomBonePoint / Direction / ArcOffset); the master, the missile, hit and spark tasks are new.
//
// Structure (setup 0x66FCC0 -> _Init 0x66FCF0, file loader 0x66FCD0 = the texture named at 0x104E320 into 0x2507810; _Init keeps
// the context and the CASTER's entity (0x2507818), sets up the master queue 0x2507850 (pool 0x2507860, 1 x 0x14), starts camera
// animation 0x104DF68 and queues the TIM):
//   MasterTask (0x66FD70) - alternates the packet arena (magic buffer + 0x4780 / + 0x14780); on counter 1 sets up the pools
//     (sparks 0x60 x 0x24 at + 0, hits 6 x 0x144 at + 0xD80, missiles 6 x 0x80C at + 0x1518), binds the caster's animation set
//     (com data 0x1047120, out block magic buffer + 0x4560) and starts her chain transformation 2. Counters 10, 14, .., 30: missile
//     k = 0..5 from the caster's default effect position pushed 300 along her facing (0x67BB70: sin / -cos of -(entity +0x0E)), at
//     her height (entity +0x24) - 0x26C, aimed at a random bone of action 0's first target (CRT rand: bone x the bone count >> 15,
//     retried while the bone's word +0 is negative); missiles 1 / 4 miss (the aim pushed by 2 x (rand & 0x7FF) - 0x400 in x / z, y =
//     the target's entity +0x24), missile 5 applies the damage, the others make the target play chain transformation 5. The
//     missile's frame = the basis of (caster - aim) x RotZ(k * 0x100 - 0x280) (a fan), its length = the distance. From counter 10
//     the missile, hit and spark queues (sound at 10); ends when every missile has ended and the hit and spark queues are empty.
//   MissileTask (0x6700C0) - progress t (+0x2A) from 0xCC by 0xCC a tick; the path point at s = start + R x (0, y(s), z(s)) with
//     d = length x s >> 12, z = d x s >> 12, y = (0x1000 - cos(s / 4)) x (-(sin(s / 2) x d) >> 12) >> 12 (an arc). Every tick a trail
//     of up to 12 sprites (sequence 0x1048DD8, frames 0xB..0) at s = t - 0x990 + 0xCC i (0 <= s < 0x1000; the GTE path, camera
//     composed, identity rotation); while t <= 0x1000 the head layout 0x10490A8 at the path point (float path 0x56C3D0), its frame
//     the basis of the step from the last point, composed with the camera; when t reaches 0x1000: the hit task (layout 0x104B63C,
//     miss 0x104C910 at the point, the step's basis flattened) and 16 sparks; ends at t >= 0x1990 (the trail runs out).
//   HitTask (0x670870) - its layout composed with the camera (depth -0x80); counters 0..0xD a flash (sequence 0x1048F1C, frame =
//     the counter) at the point scaled x 1.5; ends when the layout has ended after counter 0xE.
//   SparkTask (0x6707B0) - sequence 0x1048C94 through the shadow-rotation camera (0x400, -0x100), frame = the counter; moves by
//     its velocity (random direction 0x6A6020 x (0x80 + rand & 0x3F) / 4096), the velocity drags by -1/16 of the first; 12 frames.
//   PrimCallback (0x670570) - one layout object (Shotgun's).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the missile draws its trail, then (update) its head; the hit draws its layout then its flash; the spark draws then
// moves.
// ENTITY WRITES: caster - animation set + chain transformation 2 (master, counter 1, engine); the target (action 0's first) - chain
// transformation 5 at the impacts of missiles 0 / 2 / 3, its action result (record 0) at missile 5's impact (engine). No entity
// position word is written (entity +0x0E / +0x24 / bones only read).
// CAMERA: the setup's camera animation 0x104DF68 (engine); no camera word is written by the tasks.
// Module globals: 0x2507810..0x250787C (texture file 0x2507810, context 0x2507814, caster 0x2507818, the queues 0x2507820 sparks /
// 0x2507830 hits / 0x2507840 missiles / 0x2507850 master, master pool 0x2507860, packet cursor 0x2507874, magic buffer base
// 0x2507878).
// STACK WORDS: the missile frame's pad word (+0x1E, read only into the GTE R33 control word's high half by the trail draws) is
// Matrix3x3_MultiplyPSX's (0x56C090) never-written temp pad: it aligns its frame to 8 bytes, and under the master's frame the pad is
// the high half of a missile-pool node pointer the spawn's earlier calls left there (A/B and game-executor runs: 0x020E with the
// pool at 0x020E....): modelled as the missile node's high half (RESIDUE in SpawnMissile). The 4th words of the vectors loaded into
// GTE V0 (trail, callback) reach the GTE's VZ0 high half (0x1CA8A16) only; 0x67BB70's out[1] is never read. The missile task reuses
// its argument slot for the trail frame counter and 0x1000 - cos(t / 4) (the executor never reads that slot again).

#include "mag_common.h"

namespace ff8fx
{
namespace micromissiles299
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507814); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507818); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507820; }         // pool: magic buffer, 0x60 x 0x24
	inline TaskQueue *QHits() { return (TaskQueue *)0x2507830; }           // pool: + 0xD80, 6 x 0x144
	inline TaskQueue *QMissiles() { return (TaskQueue *)0x2507840; }       // pool: + 0x1518, 6 x 0x80C
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507874); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507878); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x66FD70;
	static const uint32_t ORIG_MissileTask = 0x6700C0;
	static const uint32_t ORIG_HitTask = 0x670870;
	static const uint32_t ORIG_SparkTask = 0x6707B0;
	static const uint32_t ORIG_PrimCallback = 0x670570;   // not a task: the layouts' callback
	static const uint32_t POOL_Hits = 0xD80, POOL_Missiles = 0x1518;
	static const uint32_t BIND_Block = 0x4560;           // magic buffer: BS_BindEntityModelFromComData's out block
	static const uint32_t MORPH = 0x4580;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x4780, ARENA_B = 0x14780; // magic buffer: packet arenas
	static const uint32_t COM_Anim = 0x1047120;          // the caster's animation set (com data)
	static const uint32_t CAMERA_Anim = 0x104DF68;       // (the setup's)
	static const uint32_t MODEL_Missile = 0x10490A8;     // prim-model layouts
	static const uint32_t MODEL_Hit = 0x104B63C;
	static const uint32_t MODEL_Miss = 0x104C910;
	static const uint32_t SEQ_Trail = 0x1048DD8;         // sprite sequences
	static const uint32_t SEQ_Flash = 0x1048F1C;
	static const uint32_t SEQ_Spark = 0x1048C94;
	static const void *const SOUND_Launch = (const void *)0x104E2FC;

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
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507860)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t done;      // +0x0E every missile has ended
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MissileNode // pool of 6 nodes of 0x80C bytes
	{
		TaskNode hdr;
		int16_t rot[9];    // +0x0C the missile frame R (3x3; 0x56C090 / 0x7012C0 write +0x0C..+0x1F)
		int16_t pad1E;
		int16_t dist;      // +0x20 launch -> aim distance
		int16_t type;      // +0x22 0 hit (target chain 5), 1 hit + damage, 2 miss, -1 impact done
		int16_t start[3];  // +0x24 launch point
		int16_t t;         // +0x2A progress (0xCC.., the head while <= 0x1000, ends at 0x1990)
		int16_t cur[3];    // +0x2C the last head point
		int16_t step;      // +0x32 0xCC
		uint8_t layout[0x7D8]; // +0x34 head layout
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
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MissileNode) == 0x80C && sizeof(HitNode) == 0x144 && sizeof(SparkNode) == 0x24, "Micro Missiles nodes");
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
	// The missile path (0x6700C0): the arc offset at progress s in the missile frame
	// ------------------------------------------------------------------
	static void PathOffset(const MissileNode *m, int32_t s, int16_t v[4])
	{
		const int32_t d = mul32(m->dist, s) >> 12;
		const int32_t k = 0x1000 - ComputeCos(s >> 2);
		const int32_t a = (int32_t)(0u - (uint32_t)mul32(ComputeSin(s >> 1), d)) >> 12;
		v[1] = (int16_t)(mul32(k, a) >> 12);
		v[2] = (int16_t)(mul32(d, s) >> 12);
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
		p[1] = (int32_t)((uint32_t)p[1] + (uint32_t)(int32_t)m->start[1]);
		p[0] = (int32_t)((uint32_t)p[0] + (uint32_t)(int32_t)m->start[0]);
		p[2] = (int32_t)((uint32_t)p[2] + (uint32_t)(int32_t)m->start[2]);
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
		view.t[1] = (int32_t)((uint32_t)view.t[1] + (uint32_t)Camera().t[1]);
		view.t[2] = (int32_t)((uint32_t)view.t[2] + (uint32_t)Camera().t[2]);
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
		BuildOrthonormalBasis(v, &arg->m);
		arg->m.t[0] = p[0];
		arg->m.t[1] = p[1];
		arg->m.t[2] = p[2];
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

	static void FlashDraw(const PrimArg *arg, int16_t frame)
	{
		Mat4x3 m = arg->m;
		m.m[0][0] = 0x1800; m.m[0][1] = 0; m.m[0][2] = 0;
		m.m[1][0] = 0; m.m[1][1] = 0x1800; m.m[1][2] = 0;
		m.m[2][0] = 0; m.m[2][1] = 0; m.m[2][2] = 0x1800;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
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
#include "mag299_micro_missiles_quistis_held.h"
#endif

namespace ff8fx
{
namespace micromissiles299
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x670570, Shotgun 281's 0x67E2D0): one object of a layout. Vertex frame (or blend into the parameter
	// block's morph buffer), rotation from the record's angles, the record's offset through the parameter matrix (flag 0x200: as
	// is), the matrix composed with it (unless flag 0x200 or 0x8000), scale (scale3DMatrix), fade, drawn by 0x7043B0 (flag 0x2000:
	// draw mode 0x203C)
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
		if (r->flags & 0x2000) *(uint32_t *)(h + 0x1C) = 0x203C;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrim2(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Spark (0x6707B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
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
	// Hit (0x670870)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		HitArg(h, &arg);
		// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
		FX_HELD(held_note_hit(h);)
		const int left = prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0);
		if (h->counter < 0xE)
		{
			// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
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
	// Missile (0x6700C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MissileTask(TaskNode *n)
	{
		MissileNode *m = (MissileNode *)n;
		// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
		FX_HELD(held_note_missile(m);)
		uint8_t *h = AllocHeader(0xB4);
		int32_t s = m->t - 0x990;
		for (int16_t frame = 0xB; ; frame--)
		{
			if (s >= 0 && s < 0x1000)
			{
				int32_t p[3];
				TrailPoint(m, s, p);
				TrailDraw(h, p, frame);
			}
			s += 0xCC;
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
				if (m->type <= 1)
				{
					DecodeModelPrimLayout(MODEL_Hit, hit->layout, 0xA0);
					if (m->type == 0) QueueChainTransformation(Entity(Ctx()->actions[0].targets[0]), 5);
					else ApplyActionResultToTarget(Ctx()->actions[0].targets);
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
		return m->t < 0x1990 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x66FD70)
	// ------------------------------------------------------------------
	// missile k: from the caster pushed 300 along her facing, aimed at a random bone of the first target
	static void SpawnMissile(int32_t k)
	{
		uint8_t *target = Entity(Ctx()->actions[0].targets[0]);
		uint8_t *caster = Caster();
		int16_t pos[4];
		GetDefaultEffectPosition(caster, pos);
		int16_t off[4];
		ArcOffset(-(int32_t)*(const int16_t *)(caster + 0xE), 0x12C, off);
		pos[0] = (int16_t)(pos[0] + off[0]);
		pos[2] = (int16_t)(pos[2] + off[2]);
		pos[1] = (int16_t)(*(const int16_t *)(caster + 0x24) - 0x26C);
		MissileNode *m = (MissileNode *)AddTaskToQueue(QMissiles(), ORIG_MissileTask);
		memcpy(m->cur, pos, 4);
		memcpy(&m->cur[2], &pos[2], 4);
		memcpy(m->start, pos, 4);
		memcpy(&m->start[2], &pos[2], 4);
		m->step = 0xCC;
		m->t = 0xCC;
		int16_t aim[4];
		while (RandomBonePoint(target, mul32(CrtRand(), ***(const uint8_t *const *const *)(target + 0x64)) >> 15, aim)) {}
		if (k % 3 == 1)
		{
			aim[0] = (int16_t)(aim[0] + (CrtRand() & 0x7FF) - 0x400);
			aim[2] = (int16_t)(aim[2] + (CrtRand() & 0x7FF) - 0x400);
			aim[1] = *(const int16_t *)(target + 0x24);
			m->type = 2;
		}
		else if (k == 5) m->type = 1;
		else m->type = 0;
		int32_t v[3] = { (int32_t)pos[0] - aim[0], (int32_t)aim[1] - pos[1], (int32_t)aim[2] - pos[2] };
		Mat4x3 basis;
		m->dist = (int16_t)BuildOrthonormalBasis(v, &basis);
		DecodeModelPrimLayout(MODEL_Missile, m->layout, 0x7D8);
		RotZ(shl32(k, 8) - 0x280, Frame(m));
		Matrix3x3MultiplyPSX(&basis, Frame(m), Frame(m));
		// RESIDUE (see STACK WORDS): 0x56C090 copies its 8-aligned temp's never-written pad word into +0x1E; with the master's frame
		// the temp's pad is the high half of a missile-pool node pointer the earlier calls of this spawn left there
		m->pad1E = (int16_t)((uint32_t)m >> 16);
	}

	// the master task (0x66FD70; IDA has no function there: MAG_299_sub_66FCF0 ends at its ret 0x66FD69)
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
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
		if (mn->counter == 1)
		{
			InitTaskQueuePool(QMissiles(), (void *)(TexBase() + POOL_Missiles), 0x80C, 6);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x144, 6);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x24, 0x60);
			BindEntityModelFromComData(Caster(), TexBase() + BIND_Block, COM_Anim);
			QueueChainTransformation(Caster(), 2);
		}
		const int32_t c = mn->counter - 0xA;
		if ((uint32_t)c < 0x18 && !(c & 3)) SpawnMissile(c >> 2);
		int a0, a1, a2;
		if (mn->counter >= 0xA)
		{
			a0 = ExecuteTaskQueue(QMissiles());
			if (a0 == 0 && mn->done == 0) mn->done = 1;
			a1 = ExecuteTaskQueue(QHits());
			a2 = ExecuteTaskQueue(QSparks());
			if (mn->counter == 0xA) BdPlaySE(SOUND_Launch, 0, 0x80);
		}
		else a0 = a1 = a2 = (int)n;
		if (mn->done && !a0 && !a1 && !a2) return TASK_END;
		mn->counter++;
		return 0;
	}
}

	void register_mag299_micro_missiles_quistis()
	{
		register_port(micromissiles299::ORIG_MasterTask, (void *)micromissiles299::MasterTask, "299 MasterTask", 299);
		register_port(micromissiles299::ORIG_MissileTask, (void *)micromissiles299::MissileTask, "299 MissileTask", 299);
		register_port(micromissiles299::ORIG_HitTask, (void *)micromissiles299::HitTask, "299 HitTask", 299);
		register_port(micromissiles299::ORIG_SparkTask, (void *)micromissiles299::SparkTask, "299 SparkTask", 299);
		// 30 fps layer: see mag299_micro_missiles_quistis_held.inc
		FX_HELD(register_mag299_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag299_micro_missiles_quistis_held.inc"
#endif
