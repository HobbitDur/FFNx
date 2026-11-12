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

// Effect 322: Zan (enemy attack 214 of kernel.bin; Fujin c0m119 / c0m136; MAG_322_*). No other kernel.bin entry
// uses effect 322. A sibling of Boomerang Sword (304) and Metsu (323): one throw at action 0's first target.
//
// Structure (setup MAG_322 0x65AB70 -> _Init 0x65ABA0, file loader 0x65AB80 = the TIM named at 0xFE7288; the setup
// keeps the CASTER's entity, starts camera animation 0xFE7004 and queues the TIM):
//   MasterTask (0x65AC20) - alternates the packet arena (magic buffer + 0xCEC / + 0xECEC); on counter 1 sets up the
//     pools once (main 1 x 0x7C at + 0, trails 8 x 0x30 at + 0x7C, hit 1 x 0x228 at + 0x1FC, streaks 0x20 x 0x2C at
//     + 0x424, dust 0x20 x 0x18 at + 0x9A4), the main task (action 0's first target and its effect position, 0x571400)
//     and the stream state at + 0xCA4 (0x534110). Then the main, trail, hit, streak and dust queues (results ignored);
//     counter 0x3B: stream load 0x168 into + 0x1CCEC (0x5341D0); every tick the stream pump (0x534210); the counter
//     stops at 0x3C, where the master ends once the preload (0x5342C0) reports done: parts still alive are dropped.
//   MainTask (0x65AE00) - counter 13: the blade's point = the caster's bone 0x18 (0x502170, flag 0x800). Counter 14:
//     the path (the bone twice, the target's frame points (0, 0, -1000) + 300 high and the target, (0, 0, 1000)
//     - 600 high, the bone twice; the frame = the floor-plane basis from the bone towards the target, at the target).
//     Counters 14..56: the blade moves along the path (304's segment blend; segment 0 over counters 14..33 with the
//     parameter squared, segments 1..2 over 34..37, segment 3 over 38..56), spins (+0x400 a tick), its frame = the
//     spin in the floor-plane basis of its motion, at its point, drawn as prim model 0xFE44C8; a trail task gets that
//     frame. The CASTER's visible-object bit 1 (+0x7C) is cleared every tick and set back at the end. While the blade
//     is above y -0x200 two dust tasks a tick (on the floor under it and half way to its last point). Counter 15:
//     sound 0xFE7000. Counter 36: the hit layout 0xFE5CC0 at the target's effect position, 32 streaks around the
//     motion (CRT rand: speed 0x40 + rand & 0x3F in the plane, 0x40 + rand & 0x3F out of it, through the motion
//     basis; drag -v / 12) and action 0's first target's result (0x506690). Counter 30: the last two path points =
//     the caster's bone again. Ends at 57.
//   TrailTask (0x65BB20) - prim model 0xFE5A50 at its frame, faded by sin(counter * 0x80) and its fade weight, blue;
//     8 ticks (304's trail, flags 0xF0).
//   HitTask (0x65B7E0) - its layout at its position through the camera (depth -0x80); ends with the layout.
//   StreakTask (0x65B680) - sprite sequence 0xFE42A8 through the shadow-rotation camera (0x400, -0x100), turned to the
//     screen direction between its point and its point minus its first velocity, flipbook frame = its counter; 8 ticks,
//     then moves by its velocity and adds its drag.
//   DustTask (0x65B600) - sprite sequence 0xFE4384 at its floor point through the shadow-rotation camera (its height
//     word, -(height >> 2)), frame = its counter; 12 ticks.
//   PrimCallback (0x65B830, Boomerang Sword's 0x66A440 on this module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the main task updates then draws; trails, hit, streaks and dust draw then update.
// ENTITY / CAMERA: the main task clears the CASTER entity's visible-object bit 1 (+0x7C &= ~2: her weapon) every tick
// from counter 14 on and sets it back when it ends; no task writes an entity position or the camera words.
// Module globals: 0x25052B0..0x2505344 (texture file 0x25052B0, context 0x25052B4, caster entity 0x25052B8, queues
// 0x25052C0 dust / 0x25052D0 streaks / 0x25052E0 hit / 0x25052F0 trails / 0x2505300 main / 0x2505310 master, master
// pool 0x2505320, packet cursor 0x2505334, zero vector 0x2505338 (bss, read only), magic buffer base 0x2505340).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls the main task first in every tick, at entry esp E):
//   - the blade's moved point word +6 (E - 0xA2; 0x56CB90 stores 3 words) is copied with the point into the node's
//     +0x6A (and from there into +0x72), never read; the bone point word +6 (E - 0xB2, 0x502170 stores 3 words, or the
//     segment blend's copy of a path point) goes with it into the path points' +6 words (+0x22, +0x2A on counter 14,
//     +0x4A, +0x52 on counter 30), never read. Nothing the tick runs before the main task reaches them: tick-start
//     residue (0 under the game executor of the verification harness, which clears the stack at the tick start).
//     MODEL: 0 (the locals start at 0).
//   - the frame vector's +6 word, the matrices' pad words (GTE R33 pad only) reach no output; a streak's never-written
//     +0x1A goes with its first velocity into +0x2A (the GTE V0 pad only); the prim headers keep the Field_Alloc
//     scratch's unwritten fields, as in the game.

#include "mag_common.h"

namespace ff8fx
{
namespace zan322
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25052B8); }        // caster entity (setup)
	inline uint32_t &TexFile() { return var<uint32_t>(0x25052B0); }         // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25052B4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2505334); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2505340); }         // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QDust() { return (TaskQueue *)0x25052C0; }            // pool: + 0x9A4, 0x20 x 0x18
	inline TaskQueue *QStreaks() { return (TaskQueue *)0x25052D0; }         // pool: + 0x424, 0x20 x 0x2C
	inline TaskQueue *QHit() { return (TaskQueue *)0x25052E0; }             // pool: + 0x1FC, 1 x 0x228
	inline TaskQueue *QTrails() { return (TaskQueue *)0x25052F0; }          // pool: + 0x7C, 8 x 0x30
	inline TaskQueue *QMain() { return (TaskQueue *)0x2505300; }            // pool: + 0, 1 x 0x7C
	static const int16_t *const ZERO_VECTOR = (const int16_t *)0x2505338; // bss, never written

	static const uint32_t ORIG_MasterTask = 0x65AC20;
	static const uint32_t ORIG_MainTask = 0x65AE00;
	static const uint32_t ORIG_DustTask = 0x65B600;
	static const uint32_t ORIG_StreakTask = 0x65B680;
	static const uint32_t ORIG_HitTask = 0x65B7E0;
	static const uint32_t ORIG_TrailTask = 0x65BB20;
	static const uint32_t POOL_Trails = 0x7C;
	static const uint32_t POOL_Hit = 0x1FC;
	static const uint32_t POOL_Streaks = 0x424;
	static const uint32_t POOL_Dust = 0x9A4;
	static const uint32_t STREAM_STATE = 0xCA4;          // magic buffer: stream state
	static const uint32_t ARENA_A = 0xCEC;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0xECEC;
	static const uint32_t STREAM_DEST = 0x1CCEC;         // magic buffer: the stream load (inside arena B)
	static const uint32_t MORPH = 0x1CCF0;               // magic buffer: blended vertex frames (inside arena B)
	static const uint32_t MODEL_Blade = 0xFE44C8;        // prim models
	static const uint32_t MODEL_Trail = 0xFE5A50;
	static const uint32_t MODEL_Hit = 0xFE5CC0;          // prim-model layout
	static const uint32_t SEQ_Streak = 0xFE42A8;         // sprite sequences
	static const uint32_t SEQ_Dust = 0xFE4384;
	static const void *const SOUND_Throw = (const void *)0xFE7000;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t ISqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t CartesianToGameAngle(int32_t a, int32_t b) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(a, b); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_sub_7015B0
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }   // MAG_063_sub_701220
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a x b
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12, 3 words
	inline void GteLoadIRDelta(const int16_t *a, const int16_t *b) { fn<void (__cdecl *)(const int16_t *, const int16_t *)>(0x704020)(a, b); } // MAG_140_sub_704020: IR1..3 = b - a
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	// battle file streaming (0x5341D0 / 0x534210 / 0x5342C0)
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPreload(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2505320)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x7C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t spin;      // +0x0E
		uint8_t *target;   // +0x10 action 0's first target
		int16_t tpos[4];   // +0x14 its effect position
		int16_t path[7][4];// +0x1C path points (+6 words: stack residue, see the module comment)
		uint8_t pad54[0x10];
		int16_t cur[4];    // +0x64 the blade's point (+0x6A: stack residue)
		int16_t prev[4];   // +0x6C its last point of a different x / z
		uint8_t pad74[8];
	};
	struct TrailNode // pool of 8 nodes of 0x30 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t fade;      // +0x0E weight of the fade (0x1000 = full)
		Mat4x3 m;          // +0x10 the blade's frame (pad +0x22 not copied: pool residue)
	};
	struct HitNode // pool of 1 node of 0x228 bytes
	{
		TaskNode hdr;
		uint8_t *ent;      // +0x0C
		int16_t pos[4];    // +0x10 its effect position (word +0x16 never written: pool residue)
		uint8_t layout[0x210]; // +0x18
	};
	struct StreakNode // pool of 0x20 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t pad1A;     // never written (pool residue)
		int16_t drag[3];   // +0x1C -first velocity / 12, added to the velocity every tick
		int16_t pad22;
		int16_t v0[4];     // +0x24 the first velocity (+ pad1A)
	};
	struct DustNode // pool of 0x20 nodes of 0x18 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C (y 0)
		int16_t counter;   // +0x12 flipbook frame
		int16_t height;    // +0x14 the blade's y * 8 + 0x1800
		int16_t pad16;
	};
	// the prim-player callback's parameter block (a stack block of the hit task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the node's position (word +6: the node's +0x16)
		int32_t depth;     // +0x08 the prim header's +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x7C && sizeof(TrailNode) == 0x30, "Zan nodes");
	static_assert(sizeof(HitNode) == 0x228 && sizeof(StreakNode) == 0x2C && sizeof(DustNode) == 0x18, "Zan nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Zan prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x65B830 = Boomerang Sword's 0x66A440): one object of a layout. Vertex frame (or blend into
	// the parameter block's morph buffer), rotation from the record's angles (flag 0x40000: 0x7015B0); flag 0x200: the
	// block's position through the camera rotation + the record's offset as the translation (rotation as is); else the
	// record's offset + the block's position through the camera rotation, the camera composed with the rotation; + the
	// camera translation; scale (flag 0x100: 3x3 product, else scale3DMatrix), fade, drawn by 0x7043B0 at the block's
	// depth.
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
		if (r->flags & 0x200)
		{
			// the block's position (its 4th word goes to the GTE V0 pad only)
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)r->pos[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)r->pos[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)r->pos[2]);
		}
		else
		{
			// the record's offset + the block's position (4th word never written: only loaded into the GTE V0 pad)
			const int16_t off[4] = { (int16_t)(r->pos[0] + arg->pos[0]), (int16_t)(r->pos[1] + arg->pos[1]), (int16_t)(r->pos[2] + arg->pos[2]), 0 };
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
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
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

	// the blade's point on segment `seg` of the path at t (0..0x1000): from point seg + 1 (t = 0) to point seg + 2
	// (t = 0x1000); each end's tangent extension (the neighbour segment's direction at this segment's length) blended
	// toward it, both blends mixed again (out: 3 words) (Boomerang Sword's segment blend)
	static void PathPoint(const MainNode *m, int32_t seg, int32_t t, int16_t *out)
	{
		const int16_t *P = m->path[seg], *A = m->path[seg + 1], *B = m->path[seg + 2], *C = m->path[seg + 3];
		int32_t sq;
		GteLoadIRDelta(A, B);
		GteSQR();
		GteSumMAC123(&sq);
		const int32_t len = ISqrt(sq);
		int16_t a[4] = {}; // (word +6 never written: not read)
		a[0] = (int16_t)(A[0] - P[0]);
		a[1] = (int16_t)(A[1] - P[1]);
		a[2] = (int16_t)(A[2] - P[2]);
		GteLoadIR123(a);
		GteSQR();
		GteSumMAC123(&sq);
		int32_t l0 = ISqrt(sq);
		if (l0 > 0)
		{
			a[0] = (int16_t)(mul32(a[0], len) / l0 + A[0]);
			a[1] = (int16_t)(mul32(a[1], len) / l0 + A[1]);
			a[2] = (int16_t)(mul32(a[2], len) / l0 + A[2]);
			BlendVectors(A, a, 0x1000 - t, t, a);
		}
		else memcpy(a, A, 8);
		int16_t b[4] = {}; // (word +6 never written: not read)
		b[0] = (int16_t)(B[0] - C[0]);
		b[1] = (int16_t)(B[1] - C[1]);
		b[2] = (int16_t)(B[2] - C[2]);
		GteLoadIR123(b);
		GteSQR();
		GteSumMAC123(&sq);
		l0 = ISqrt(sq);
		if (l0 > 0)
		{
			b[0] = (int16_t)(mul32(b[0], len) / l0 + B[0]);
			b[1] = (int16_t)(mul32(b[1], len) / l0 + B[1]);
			b[2] = (int16_t)(mul32(b[2], len) / l0 + B[2]);
			BlendVectors(B, b, t, 0x1000 - t, b);
		}
		else memcpy(b, B, 8);
		BlendVectors(a, b, 0x1000 - t, t, out);
	}

	// the blade's frame: the spin (X) in the floor-plane basis of its motion `dir`, at its point; drawn through the
	// camera (0x65B207..0x65B2D6). `frame` is what the trail gets.
	static void BladeDraw(const int16_t *cur, int32_t *dir, int16_t spin, Mat4x3 *frame)
	{
		Mat4x3 basis;
		RotX(spin, frame);
		BuildOrthonormalBasis(dir, &basis);
		MatrixMultiply(&basis, frame);
		frame->t[0] = cur[0];
		frame->t[1] = cur[1];
		frame->t[2] = cur[2];
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 cam;
		ComposeAffineTransform(&Camera(), frame, &cam);
		GteSetRotMatrix(&cam);
		GteSetTransVector(&cam);
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = MODEL_Blade;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the trail's draw at its counter (0x65BB20..0x65BBEF)
	static void TrailDraw(const TrailNode *t, int32_t angle)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 cam;
		ComposeAffineTransform(&Camera(), &t->m, &cam);
		GteSetRotMatrix(&cam);
		GteSetTransVector(&cam);
		int32_t f = ComputeSin(angle);
		*(int32_t *)(h + 0xC) = f;
		if (t->fade < 0x1000)
		{
			f = 0x1000 - ((0x1000 - f) * t->fade >> 12);
			*(int32_t *)(h + 0xC) = f;
		}
		h[9] = 0;
		h[8] = 0;
		int32_t blue = (0x1000 - f) >> 4;
		if (blue > 0xFF) blue = 0xFF;
		h[0xA] = (uint8_t)blue;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		*(uint32_t *)h = MODEL_Trail;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the streak's draw at `pos` (0x65B680..0x65B776): turned to the screen direction from the point pos - v0 to pos
	static void StreakDraw(const int16_t *pos, const int16_t *v0, int16_t frame)
	{
		uint8_t *h = AllocHeader(0xB4);
		int16_t d[4] = { (int16_t)(pos[0] - v0[0]), (int16_t)(pos[1] - v0[1]), (int16_t)(pos[2] - v0[2]), 0 }; // (+6: GTE V0 pad only)
		int16_t p1[4] = {}, p2[4] = {};
		TransformCameraByShadowRotation(d, 0x400, -0x100);
		GteLoadV0(ZERO_VECTOR);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(p1);
		TransformCameraByShadowRotation(pos, 0x400, -0x100);
		GteLoadV0(v0);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(p2);
		*(int32_t *)(h + 8) = -CartesianToGameAngle((int32_t)p1[1] - p2[1], (int32_t)p2[0] - p1[0]);
		*(uint32_t *)h = SEQ_Streak;
		*(int16_t *)(h + 0x24) = 1;
		*(int16_t *)(h + 4) = frame;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the dust sprite (0x65B600..0x65B662)
	static void DustDraw(const int16_t *pos, int16_t height, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, height, -(height >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Dust;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag322_zan_held.h"
#endif

namespace ff8fx
{
namespace zan322
{
	// ------------------------------------------------------------------
	// Dust (0x65B600)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DustTask(TaskNode *n)
	{
		DustNode *d = (DustNode *)n;
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(held_note_dust(n);)
		DustDraw(d->pos, d->height, d->counter);
		d->counter++;
		return d->counter < 0xC ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Streak (0x65B680)
	// ------------------------------------------------------------------
	static uint32_t __cdecl StreakTask(TaskNode *n)
	{
		StreakNode *s = (StreakNode *)n;
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(held_note_streak(n);)
		StreakDraw(s->pos, s->v0, s->counter);
		s->counter++;
		if (s->counter >= 8) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->vel[0] = (int16_t)(s->drag[0] + s->vel[0]);
		s->vel[1] = (int16_t)(s->drag[1] + s->vel[1]);
		s->vel[2] = (int16_t)(s->drag[2] + s->vel[2]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Hit (0x65B7E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		memcpy(arg.pos, h->pos, 8);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(held_note_hit(n);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Trail (0x65BB20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TrailTask(TaskNode *n)
	{
		TrailNode *t = (TrailNode *)n;
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(held_note_trail(n);)
		TrailDraw(t, (int32_t)t->counter * 0x400 / 8);
		t->counter++;
		return t->counter < 8 ? 0 : TASK_END;
	}

	// the path on counter 14 (0x65AE56..0x65AF5B): from the caster's bone towards the target and back
	static void BuildPath(MainNode *m)
	{
		int16_t bone[4] = {}; // (word +6 never written: stack residue, modelled 0)
		GetEffectSpawnPosition(Caster(), 0x18, 0x800, bone);
		int32_t v[3] = { (int32_t)bone[0] - m->tpos[0], 0, (int32_t)m->tpos[2] - bone[2] };
		Mat4x3 basis;
		BuildOrthonormalBasis(v, &basis);
		basis.t[0] = m->tpos[0];
		basis.t[1] = m->tpos[1];
		basis.t[2] = m->tpos[2];
		memcpy(m->path[0], bone, 8);
		memcpy(m->path[1], bone, 8);
		GteSetRotMatrix(&basis);
		GteSetTransVector(&basis);
		int16_t off[4] = { 0, 0, -1000, 0 }; // (word +6 never written: GTE V0 pad only)
		GteLoadV0(off);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(m->path[2]);
		m->path[2][1] = (int16_t)(m->path[2][1] + 300);
		memcpy(m->path[3], m->tpos, 8);
		off[2] = 1000;
		GteLoadV0(off);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(m->path[4]);
		m->path[4][1] = (int16_t)(m->path[4][1] - 600);
		memcpy(m->path[5], bone, 8);
		memcpy(m->path[6], bone, 8);
	}

	// ------------------------------------------------------------------
	// Main (0x65AE00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		if (m->counter == 0xD)
		{
			GetEffectSpawnPosition(Caster(), 0x18, 0x800, m->cur);
			memcpy(m->prev, m->cur, 8);
		}
		const int32_t k = m->counter - 0xE;
		int32_t seg = 0, t = 0;
		if ((uint32_t)k < 0x14)
		{
			if (k == 0) BuildPath(m);
			const int32_t u = (int32_t)((uint32_t)(k << 12) / 20u) + 0x66;
			t = u * u >> 12;
			seg = 0;
		}
		else if ((uint32_t)(m->counter - 0x22) < 4)
		{
			const int32_t w = (int32_t)((uint32_t)(m->counter - 0x22) << 11) + 0x200;
			seg = (w >> 12) + 1;
			t = w & 0xFFF;
		}
		else if ((uint32_t)(m->counter - 0x26) < 0x14)
		{
			t = (int32_t)((uint32_t)((m->counter - 0x26) << 12) / 20u) + 0x66;
			seg = 3;
		}
		if ((uint32_t)k < 0x3A)
		{
			// 30 fps layer: see mag322_zan_held.inc
			FX_HELD(held_note_blade(n);)
			int16_t cur[4] = {}; // (word +6 never written: stack residue, modelled 0)
			PathPoint(m, seg, t, cur);
			if (m->cur[0] != cur[0] || m->cur[2] != cur[2]) memcpy(m->prev, m->cur, 8);
			m->spin = (int16_t)(m->spin + 0x400);
			memcpy(m->cur, cur, 8);
			*(uint32_t *)(Caster() + 0x7C) &= 0xFFFFFFFDu;
			int32_t dir[3] = { (int32_t)m->prev[0] - cur[0], 0, (int32_t)cur[2] - m->prev[2] };
			Mat4x3 frame;
			BladeDraw(cur, dir, m->spin, &frame);
			TrailNode *tr = (TrailNode *)AddTaskToQueue(QTrails(), ORIG_TrailTask);
			if (tr)
			{
				tr->counter = 0;
				memcpy(&tr->m, &frame, 0x12); // (the pad word +0x22 is not copied)
				memcpy(tr->m.t, frame.t, sizeof(frame.t));
				if ((uint32_t)k < 0x10) tr->fade = (int16_t)(k << 8);
				else if ((uint32_t)k > 0x24) tr->fade = (int16_t)((0x2C - k) << 9);
				else tr->fade = 0x1000;
			}
			if (cur[1] > -0x200)
			{
				const int16_t height = (int16_t)(cur[1] * 8 + 0x1800);
				DustNode *d = (DustNode *)AddTaskToQueue(QDust(), ORIG_DustTask);
				if (d)
				{
					memcpy(d->pos, cur, 8);
					d->pos[1] = 0;
					d->counter = 0;
					d->height = height;
				}
				d = (DustNode *)AddTaskToQueue(QDust(), ORIG_DustTask);
				if (d)
				{
					d->pos[0] = (int16_t)(((int32_t)m->prev[0] + cur[0]) >> 1);
					d->pos[1] = 0;
					d->pos[2] = (int16_t)(((int32_t)m->prev[2] + cur[2]) >> 1);
					d->counter = 0;
					d->height = height;
				}
			}
			if (m->counter == 0x24)
			{
				uint8_t *ent = m->target;
				HitNode *h = (HitNode *)AddTaskToQueue(QHit(), ORIG_HitTask);
				if (h)
				{
					h->ent = ent;
					GetDefaultEffectPosition(ent, h->pos);
					DecodeModelPrimLayout(MODEL_Hit, h->layout, 0x210);
				}
				Mat4x3 basis;
				BuildOrthonormalBasis(dir, &basis);
				int32_t angle = 0;
				cur[3] = 0;
				for (int i = 0; i < 0x20; i++)
				{
					StreakNode *s = (StreakNode *)AddTaskToQueue(QStreaks(), ORIG_StreakTask);
					if (!s) continue;
					memcpy(s->pos, cur, 8); // (+0x12: the counter, 0)
					const int32_t speed = (CrtRand() & 0x3F) + 0x40;
					s->vel[0] = (int16_t)(mul32(ComputeSin(angle), speed) / 0x1000);
					s->vel[1] = (int16_t)(mul32(ComputeCos(angle), speed) / 0x1000);
					s->vel[2] = (int16_t)((CrtRand() & 0x3F) + 0x40);
					MatrixMulVector(&basis, s->vel, s->vel);
					memcpy(s->v0, s->vel, 8); // (with the never-written +0x1A)
					s->drag[0] = (int16_t)((int32_t)s->vel[0] / -12);
					s->drag[1] = (int16_t)((int32_t)s->vel[1] / -12);
					s->drag[2] = (int16_t)((int32_t)s->vel[2] / -12);
					angle += 0x80;
				}
				ApplyActionResultToTarget(Ctx()->actions[0].targets);
			}
		}
		if (m->counter == 0xF) BdPlaySE(SOUND_Throw, 0, 0x80);
		if (m->counter == 0x1E)
		{
			int16_t bone[4] = {}; // (word +6 never written: stack residue, modelled 0)
			GetEffectSpawnPosition(Caster(), 0x18, 0x800, bone);
			memcpy(m->path[5], bone, 8);
			memcpy(m->path[6], bone, 8);
		}
		m->counter++;
		if (m->counter < 0x39) return 0;
		*(uint32_t *)(Caster() + 0x7C) |= 2;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x65AC20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *ms = (MasterNode *)n;
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(held_note_master();)
		if (ms->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			ms->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			ms->arena = 1;
		}
		if (ms->counter == 1 && !ms->started)
		{
			ms->started = 1;
			InitTaskQueuePool(QMain(), (void *)TexBase(), 0x7C, 1);
			InitTaskQueuePool(QTrails(), (void *)(TexBase() + POOL_Trails), 0x30, 8);
			InitTaskQueuePool(QHit(), (void *)(TexBase() + POOL_Hit), 0x228, 1);
			InitTaskQueuePool(QStreaks(), (void *)(TexBase() + POOL_Streaks), 0x2C, 0x20);
			InitTaskQueuePool(QDust(), (void *)(TexBase() + POOL_Dust), 0x18, 0x20);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			if (m)
			{
				Memset32(&m->counter, 0, 0x1C);
				m->target = Entity(Ctx()->actions[0].targets[0]);
				GetDefaultEffectPosition(m->target, m->tpos);
			}
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
		}
		if (ms->started)
		{
			ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QTrails());
			ExecuteTaskQueue(QHit());
			ExecuteTaskQueue(QStreaks());
			ExecuteTaskQueue(QDust());
		}
		if (ms->counter == 0x3B) StreamQueueLoad(0x168, TexBase() + STREAM_DEST, 0);
		if (ms->started) StreamPump();
		if (ms->counter > 0x3B)
		{
			if (StreamPreload(0) == 0) return 0;
			// 30 fps layer: see mag322_zan_held.inc
			FX_HELD(held_note_end();)
			return TASK_END;
		}
		ms->counter++;
		return 0;
	}
}

	void register_mag322_zan()
	{
		register_port(zan322::ORIG_MasterTask, (void *)zan322::MasterTask, "322 MasterTask", 322);
		register_port(zan322::ORIG_MainTask, (void *)zan322::MainTask, "322 MainTask", 322);
		register_port(zan322::ORIG_TrailTask, (void *)zan322::TrailTask, "322 TrailTask", 322);
		register_port(zan322::ORIG_HitTask, (void *)zan322::HitTask, "322 HitTask", 322);
		register_port(zan322::ORIG_StreakTask, (void *)zan322::StreakTask, "322 StreakTask", 322);
		register_port(zan322::ORIG_DustTask, (void *)zan322::DustTask, "322 DustTask", 322);
		// 30 fps layer: see mag322_zan_held.inc
		FX_HELD(register_mag322_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag322_zan_held.inc"
#endif
