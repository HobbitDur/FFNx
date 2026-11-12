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

// Effect 323: Metsu (enemy attack 215 of kernel.bin; Fujin c0m119 / c0m136; MAG_323_*). No other kernel.bin entry
// uses effect 323. Boomerang Sword's module (304) on its own path, timing, models and layout callback.
//
// Structure (setup MAG_323 0x659A80 -> _Init 0x659AB0, file loader 0x659A90 = the TIM named at 0xFE429C; the setup
// keeps the CASTER's entity, starts camera animation 0xFE0FCC and queues the TIM):
//   MasterTask (0x659B30) - alternates the packet arena (magic buffer + 0x1824 / + 0x11824); on counter 1 sets up the
//     pools once (main 1 x 0xBC at + 0, trails 0x20 x 0x30 at + 0xBC, hits 3 x 0x288 at + 0x6BC, sparks 0x40 x 0x24
//     at + 0xE54) and the main task: action 0's targets sorted by x (largest first), their effect positions
//     (0x571400), the smallest / largest x -/+ 1000, the mean y and the mean z - 1000. Then the main, trail, hit and
//     spark queues; ends when the main, trail and hit queues are empty (sparks still alive are dropped).
//   MainTask (0x659DD0) - counter 11: the blade's point = the caster's bone 0x18 (0x502170, flag 0x800), tilt 0x400.
//     Counter 12: the path (the bone 500 nearer, the bone, (max x, y - 1000, z), every target, (min x, y, z), half way
//     between the bone and (min x, z) 1500 high, the bone twice); sound 0xFE12B0. Counters 12..63: the blade moves
//     along the path (304's segment blend; segment 0 over counters 12..26, segment 1 over 27..34, the target segments
//     over 35..44, the last two over 45..63), spins (+0x400 a tick) and tilts (+0x4D a tick), its frame = the rotations
//     in the floor-plane basis of its motion, at its point, drawn as prim model 0xFE13EC; a trail task gets that frame.
//     The CASTER's visible-object bit 1 (+0x7C) is cleared every tick and set back at the end. Counters 35..44: when
//     the blade passes the next target (x below the target's), that target's damage (0x65A5F0), a hit layout
//     0xFE2BE4 at its effect position and 21 sparks around the motion (CRT rand: speed 0x40 + rand & 0x3F in the
//     plane, 0x40 + rand & 0x3F out of it, through the motion basis; drag -v / 12). Counter 29: the last two path
//     points = the caster's bone again (the last one 1000 nearer). Ends at 64.
//   TrailTask (0x65AA80) - prim model 0xFE2974 at its frame, faded by sin(counter * 0x80) and its fade weight, blue;
//     8 ticks (304's trail, flags 0xF0).
//   HitTask (0x65A720) - its layout at its position through the camera (depth -0x80); ends with the layout.
//   SparkTask (0x65A660, = 304's 0x66A330) - sprite sequence 0xFE12B4 through the shadow-rotation camera (0x800,
//     -0x200), flipbook frame = its counter; 10 ticks, then moves by its velocity and adds its drag.
//   PrimCallback (0x65A770) draws every layout object: Boomerang Sword's 0x66A440 on this module's cursor, plus record
//     flag 0x10000 = the rotation from the angles by 0x7014B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the main task updates then draws; trails, hits and sparks draw then update.
// ENTITY / CAMERA: the main task clears the CASTER entity's visible-object bit 1 (+0x7C &= ~2) every tick from counter
// 12 on and sets it back when it ends; no task writes an entity position or the camera words.
// Module globals: 0x2505230..0x25052AC (texture file 0x2505230, context 0x2505234, caster entity 0x2505238, queues
// 0x2505240 sparks / 0x2505250 hits / 0x2505260 trails / 0x2505270 main / 0x2505280 master, master pool 0x2505290,
// packet cursor 0x25052A4, magic buffer base 0x25052A8).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls the main task first in every tick, at entry esp E):
//   - the blade's moved point word +6 (E - 0xCA; 0x56CB90 stores 3 words) is copied with the point into the node's
//     +0xAA (and from there into +0xB2), never read; the bone point word +6 of counter 29 (E - 0xD2, 0x502170 stores
//     3 words) goes into the last two path points' +6 words, never read. Tick-start residue (0 under the game executor
//     of the verification harness, which clears the stack at the tick start). MODEL: 0 (the locals start at 0).
//   - the segment points' 4th words, the matrices' pad words (GTE R33 pad only) reach no output; the prim headers keep
//     the Field_Alloc scratch's unwritten fields, as in the game.

#include "mag_common.h"

namespace ff8fx
{
namespace metsu323
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(0x2505230); }         // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2505234); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2505238); }        // caster entity (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25052A4); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x25052A8); }         // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2505240; }          // pool: + 0xE54, 0x40 x 0x24
	inline TaskQueue *QHits() { return (TaskQueue *)0x2505250; }            // pool: + 0x6BC, 3 x 0x288
	inline TaskQueue *QTrails() { return (TaskQueue *)0x2505260; }          // pool: + 0xBC, 0x20 x 0x30
	inline TaskQueue *QMain() { return (TaskQueue *)0x2505270; }            // pool: + 0, 1 x 0xBC

	static const uint32_t ORIG_MasterTask = 0x659B30;
	static const uint32_t ORIG_MainTask = 0x659DD0;
	static const uint32_t ORIG_SparkTask = 0x65A660;
	static const uint32_t ORIG_HitTask = 0x65A720;
	static const uint32_t ORIG_TrailTask = 0x65AA80;
	static const uint32_t POOL_Trails = 0xBC;
	static const uint32_t POOL_Hits = 0x6BC;
	static const uint32_t POOL_Sparks = 0xE54;
	static const uint32_t MORPH = 0x1754;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1824;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11824;
	static const uint32_t MODEL_Blade = 0xFE13EC;        // prim models
	static const uint32_t MODEL_Trail = 0xFE2974;
	static const uint32_t MODEL_Hit = 0xFE2BE4;          // prim-model layout
	static const uint32_t SEQ_Spark = 0xFE12B4;          // sprite sequence
	static const void *const SOUND_Throw = (const void *)0xFE12B0;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t ISqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_sub_7015B0
	inline void RotationFromAnglesC(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7014B0)(angles, out); } // sub_7014B0: Y, Z, then X
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }   // MAG_063_sub_701220
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }   // MAG_065_sub_7012C0
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

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2505290)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0xBC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets
		int16_t next;      // +0x10 next target to pass
		int16_t minx;      // +0x12 smallest target x - 1000
		int16_t maxx;      // +0x14 largest target x + 1000
		int16_t meany;     // +0x16
		int16_t meanz;     // +0x18 mean z - 1000
		int16_t spin;      // +0x1A
		int16_t tilt;      // +0x1C
		int16_t pad1E;
		uint8_t *ents[3];  // +0x20 the targets' entities, largest x first
		int16_t tpos[3][4];// +0x2C their effect positions
		int16_t path[10][4];// +0x44 path points (count + 7)
		uint8_t pad94[8];
		int16_t spawn[4];  // +0x9C the caster's bone 0x18 at counter 12
		int16_t cur[4];    // +0xA4 the blade's point (+0xAA: stack residue, see the module comment)
		int16_t prev[4];   // +0xAC its last different point
		uint8_t padB4[8];
	};
	struct TrailNode // pool of 0x20 nodes of 0x30 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t fade;      // +0x0E weight of the fade (0x1000 = full)
		Mat4x3 m;          // +0x10 the blade's frame (pad +0x22 not copied: pool residue)
	};
	struct HitNode // pool of 3 nodes of 0x288 bytes
	{
		TaskNode hdr;
		uint8_t *ent;      // +0x0C
		int16_t pos[4];    // +0x10 its effect position (word +0x16 never written: pool residue)
		uint8_t layout[0x270]; // +0x18
	};
	struct SparkNode // pool of 0x40 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t pad1A;
		int16_t drag[3];   // +0x1C -velocity / 12, added to the velocity every tick
		int16_t pad22;
	};
	// the prim-player callback's parameter block (a stack block of the hit task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the node's position (word +6: the node's +0x16)
		int32_t depth;     // +0x08 the prim header's +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0xBC && sizeof(TrailNode) == 0x30, "Metsu nodes");
	static_assert(sizeof(HitNode) == 0x288 && sizeof(SparkNode) == 0x24, "Metsu nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Metsu prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x65A770): Boomerang Sword's 0x66A440 plus flag 0x10000. One object of a layout. Vertex
	// frame (or blend into the parameter block's morph buffer), rotation from the record's angles (flag 0x10000:
	// 0x7014B0, else flag 0x40000: 0x7015B0); flag 0x200: the block's position through the camera rotation + the
	// record's offset as the translation (rotation as is); else the record's offset + the block's position through the
	// camera rotation, the camera composed with the rotation; + the camera translation; scale (flag 0x100: 3x3
	// product, else scale3DMatrix), fade, drawn by 0x7043B0 at the block's depth.
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
		if (r->flags & 0x10000) RotationFromAnglesC(r->rot, &m);
		else if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
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

	// the blade's frame: spin (X) and tilt (Z) in the floor-plane basis of its motion `dir`, at its point; drawn
	// through the camera (0x65A229..0x65A32B). `frame` is what the trail gets.
	static void BladeDraw(const int16_t *cur, int32_t *dir, int16_t spin, int16_t tilt, Mat4x3 *frame)
	{
		Mat4x3 rz, basis;
		RotX(spin, frame);
		RotZ(tilt, &rz);
		BuildOrthonormalBasis(dir, &basis);
		MatrixMultiply(&rz, frame);
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

	// the trail's draw at its counter (0x65AA80..0x65AB4F)
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

	// the sprite (0x65A660..0x65A6BF)
	static void SpriteDraw(int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x800, -0x200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag323_metsu_held.h"
#endif

namespace ff8fx
{
namespace metsu323
{
	// ------------------------------------------------------------------
	// Spark (0x65A660)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag323_metsu_held.inc
		FX_HELD(held_note_spark(n);)
		SpriteDraw(s->pos, s->counter);
		s->counter++;
		if (s->counter >= 0xA) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->vel[0] = (int16_t)(s->drag[0] + s->vel[0]);
		s->vel[1] = (int16_t)(s->drag[1] + s->vel[1]);
		s->vel[2] = (int16_t)(s->drag[2] + s->vel[2]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Hit (0x65A720)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		memcpy(arg.pos, h->pos, 8);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag323_metsu_held.inc
		FX_HELD(held_note_hit(n);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Trail (0x65AA80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TrailTask(TaskNode *n)
	{
		TrailNode *t = (TrailNode *)n;
		// 30 fps layer: see mag323_metsu_held.inc
		FX_HELD(held_note_trail(n);)
		TrailDraw(t, (int32_t)t->counter * 0x400 / 8);
		t->counter++;
		return t->counter < 8 ? 0 : TASK_END;
	}

	// 0x65A5F0 (= Boomerang Sword's 0x66A2C0): applies action 0's results to the target records of this entity
	static void ApplyResultsOf(const uint8_t *ent)
	{
		const int32_t slot = (int32_t)((uint32_t)ent - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *rec = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)rec[0] == slot) ApplyActionResultToTarget(rec);
		}
	}

	// the path on counter 12 (0x659E33..0x659F17)
	static void BuildPath(MainNode *m)
	{
		GetEffectSpawnPosition(Caster(), 0x18, 0x800, m->spawn);
		int16_t (*p)[4] = m->path;
		memcpy(*p, m->spawn, 8);
		(*p)[2] = (int16_t)((*p)[2] - 500);
		p++;
		memcpy(*p++, m->spawn, 8);
		(*p)[0] = m->maxx;
		(*p)[1] = (int16_t)(m->meany - 1000);
		(*p)[2] = m->meanz;
		p++;
		for (int32_t i = 0; i < m->count; i++) memcpy(*p++, m->tpos[i], 8);
		(*p)[0] = m->minx;
		(*p)[1] = m->meany;
		(*p)[2] = m->meanz;
		p++;
		(*p)[0] = (int16_t)(((int32_t)m->spawn[0] + m->minx) / 2);
		(*p)[1] = (int16_t)(m->meany - 1500);
		(*p)[2] = (int16_t)(((int32_t)m->spawn[2] + m->meanz) / 2);
		p++;
		memcpy(*p++, m->spawn, 8);
		memcpy(*p, m->spawn, 8);
	}

	// ------------------------------------------------------------------
	// Main (0x659DD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		if (m->counter == 0xB)
		{
			GetEffectSpawnPosition(Caster(), 0x18, 0x800, m->cur);
			memcpy(m->prev, m->cur, 8);
			m->tilt = 0x400;
		}
		const int32_t k = m->counter - 0xC;
		int32_t seg = 0, t = 0;
		if ((uint32_t)k < 0xF)
		{
			if (k == 0) BuildPath(m);
			t = (int32_t)((uint32_t)(k << 12) / 15u) + 0x88;
			seg = 0;
		}
		else
		{
			// (counters below 12 keep a never-written seg / t: not drawn)
			if ((uint32_t)(m->counter - 0x1B) < 0x1B)
			{
				t = (int32_t)(((uint32_t)(m->counter - 0x1B) & 0xFFFFF) << 9) + 0x100;
				seg = 1;
			}
			if ((uint32_t)(m->counter - 0x23) < 0xA)
			{
				const int32_t w = (int32_t)(((uint32_t)mul32(m->count, m->counter - 0x23) << 12) / 10u) + 0xCC;
				seg = (w >> 12) + 2;
				t = w & 0xFFF;
			}
			else if ((uint32_t)(m->counter - 0x2D) < 0x14)
			{
				const int32_t w = (int32_t)((uint32_t)((m->counter - 0x2D) << 13) / 20u) + 0xCC;
				seg = m->count + (w >> 12) + 2;
				t = w & 0xFFF;
			}
		}
		if ((uint32_t)k < 0x41)
		{
			m->tilt = (int16_t)(m->tilt + 0x4D);
			// 30 fps layer: see mag323_metsu_held.inc
			FX_HELD(held_note_blade(n);)
			int16_t cur[4] = {}; // (word +6 never written: stack residue, modelled 0)
			PathPoint(m, seg, t, cur);
			if (m->cur[0] != cur[0] || m->cur[1] != cur[1] || m->cur[2] != cur[2]) memcpy(m->prev, m->cur, 8);
			m->spin = (int16_t)(m->spin + 0x400);
			memcpy(m->cur, cur, 8);
			*(uint32_t *)(Caster() + 0x7C) &= 0xFFFFFFFDu;
			int32_t dir[3] = { (int32_t)m->prev[0] - cur[0], 0, (int32_t)cur[2] - m->prev[2] };
			Mat4x3 frame;
			BladeDraw(cur, dir, m->spin, m->tilt, &frame);
			TrailNode *tr = (TrailNode *)AddTaskToQueue(QTrails(), ORIG_TrailTask);
			if (tr)
			{
				tr->counter = 0;
				memcpy(&tr->m, &frame, 0x12); // (the pad word +0x22 is not copied)
				memcpy(tr->m.t, frame.t, sizeof(frame.t));
				if ((uint32_t)k < 0x10) tr->fade = (int16_t)(k << 8);
				else if ((uint32_t)k > 0x31) tr->fade = (int16_t)((0x41 - k) << 8);
				else tr->fade = 0x1000;
			}
			if (m->counter >= 0x23 && m->counter < 0x2D && m->next < m->count)
			{
				uint8_t *ent = m->ents[m->next];
				if (cur[0] < *(const int16_t *)(ent + 0x1C))
				{
					HitNode *h = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
					ApplyResultsOf(ent);
					if (h)
					{
						h->ent = ent;
						GetDefaultEffectPosition(ent, h->pos);
						DecodeModelPrimLayout(MODEL_Hit, h->layout, 0x270);
					}
					int32_t angle = 0;
					Mat4x3 basis;
					BuildOrthonormalBasis(dir, &basis);
					cur[3] = 0;
					for (int i = 0; i < 0x15; i++)
					{
						SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
						if (!s) continue;
						memcpy(s->pos, cur, 8); // (+0x12: the counter, 0)
						const int32_t speed = (CrtRand() & 0x3F) + 0x40;
						s->vel[0] = (int16_t)(mul32(ComputeSin(angle), speed) / 0x1000);
						s->vel[1] = (int16_t)(mul32(ComputeCos(angle), speed) / 0x1000);
						s->vel[2] = (int16_t)((CrtRand() & 0x3F) + 0x40);
						MatrixMulVector(&basis, s->vel, s->vel);
						s->drag[0] = (int16_t)((int32_t)s->vel[0] / -12);
						s->drag[1] = (int16_t)((int32_t)s->vel[1] / -12);
						s->drag[2] = (int16_t)((int32_t)s->vel[2] / -12);
						angle += 0xC3;
					}
					m->next++;
				}
			}
		}
		if (m->counter == 0xC) BdPlaySE(SOUND_Throw, 0, 0x80);
		if (m->counter == 0x1D)
		{
			int16_t bone[4] = {}; // (word +6 never written: stack residue, modelled 0)
			GetEffectSpawnPosition(Caster(), 0x18, 0x800, bone);
			memcpy(m->path[m->count + 5], bone, 8);
			memcpy(m->path[m->count + 6], bone, 8);
			m->path[m->count + 6][2] = (int16_t)(m->path[m->count + 6][2] - 1000);
		}
		m->counter++;
		if (m->counter < 0x40) return 0;
		*(uint32_t *)(Caster() + 0x7C) |= 2;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x659B30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *ms = (MasterNode *)n;
		// 30 fps layer: see mag323_metsu_held.inc
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
			InitTaskQueuePool(QMain(), (void *)TexBase(), 0xBC, 1);
			InitTaskQueuePool(QTrails(), (void *)(TexBase() + POOL_Trails), 0x30, 0x20);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x288, 3);
			InitTaskQueuePool(QSparks(), (void *)(TexBase() + POOL_Sparks), 0x24, 0x40);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			if (m)
			{
				Memset32(&m->counter, 0, 0x2C);
				const ActionData &a = Ctx()->actions[0];
				m->count = a.target_count;
				for (int32_t i = 0; i < m->count; i++) m->ents[i] = Entity(a.targets[i * TARGET_STRIDE]);
				// largest x first
				for (int32_t i = 0; i + 1 < m->count; i++)
					for (int32_t j = i + 1; j < m->count; j++)
						if (*(const int16_t *)(m->ents[i] + 0x1C) < *(const int16_t *)(m->ents[j] + 0x1C))
						{
							uint8_t *e = m->ents[i];
							m->ents[i] = m->ents[j];
							m->ents[j] = e;
						}
				m->minx = -1000;
				m->maxx = 1000;
				m->meanz = 0;
				m->meany = 0;
				for (int32_t i = 0; i < m->count; i++)
				{
					GetDefaultEffectPosition(m->ents[i], m->tpos[i]);
					m->meany = (int16_t)(m->meany + m->tpos[i][1]);
					m->meanz = (int16_t)(m->meanz + m->tpos[i][2]);
					if (m->minx > m->tpos[i][0]) m->minx = m->tpos[i][0];
					if (m->maxx < m->tpos[i][0]) m->maxx = m->tpos[i][0];
				}
				const int32_t y = (int32_t)m->meany / m->count;
				m->minx = (int16_t)(m->minx - 1000);
				m->maxx = (int16_t)(m->maxx + 1000);
				m->meany = (int16_t)y;
				m->meanz = (int16_t)((int32_t)m->meanz / m->count - 1000);
			}
		}
		int a0, a1, a2;
		if (ms->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QTrails());
			a2 = ExecuteTaskQueue(QHits());
			ExecuteTaskQueue(QSparks());
		}
		else a0 = a1 = a2 = (int)n;
		if (ms->started && a0 == 0 && a1 == 0 && a2 == 0)
		{
			// 30 fps layer: see mag323_metsu_held.inc
			FX_HELD(held_note_end();)
			return TASK_END;
		}
		ms->counter++;
		return 0;
	}
}

	void register_mag323_metsu()
	{
		register_port(metsu323::ORIG_MasterTask, (void *)metsu323::MasterTask, "323 MasterTask", 323);
		register_port(metsu323::ORIG_MainTask, (void *)metsu323::MainTask, "323 MainTask", 323);
		register_port(metsu323::ORIG_TrailTask, (void *)metsu323::TrailTask, "323 TrailTask", 323);
		register_port(metsu323::ORIG_HitTask, (void *)metsu323::HitTask, "323 HitTask", 323);
		register_port(metsu323::ORIG_SparkTask, (void *)metsu323::SparkTask, "323 SparkTask", 323);
		// 30 fps layer: see mag323_metsu_held.inc
		FX_HELD(register_mag323_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag323_metsu_held.inc"
#endif
