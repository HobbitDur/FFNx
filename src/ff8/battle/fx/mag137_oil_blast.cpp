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

// Effect 137: Oil Blast (enemy attack 181 of kernel.bin, Oilboyle c0m099; MAG_137_*). No other
// kernel.bin entry uses effect 137.
//
// Structure (setup MAG_137_OIL_BLAST 0x6AB850 -> _Init 0x6AB880, file loader 0x6AB860 = the
// texture file named at 0x11F2384; the setup keeps the CASTER's entity (0x2520CC0), starts camera
// animation 0x11F0108 and queues the TIM; setup / loader / _Init are the copies of effects 126..139):
//   MasterTask (0x6AB900) - alternates the packet arena (magic buffer + 0x1658 / + 0x11658); on
//     counter 1 sets up the pools (sprays 0x3C x 0x24 at + 0, chunks 0x20 x 0x2C at + 0x870, blast
//     1 x 0x5A0 at + 0xDF0) and starts the blast (the first target of action 0, its effect point
//     0xF1). Then the blast, chunk and spray queues (each executor call one queue argument deeper:
//     the queue words are popped after the third); ends when the BLAST queue is empty.
//   BlastTask (0x6ABA50) - counters 0..0x3C.
//     Counters 0x12..0x1A: two layouts (0x11EAE10, at +0x84 / +0xC4) at the caster's effect points
//       0x1E / 0x1C on the floor (y = 0), camera-aligned (mode 0: objects by 0x572200).
//     Counters 0x15..0x28: layout 0x11EB4D8 (+0x104) at the caster's effect point 0x13 on the floor,
//       turned by the caster's facing (RotY), mode 0.
//     Counters 0x15..0x1C (k = counter - 0x15, s = k << 9): on k = 0 the basis C of (target -
//       point 0x13) on the floor and its start point P = point 0x13 + C's z column / 16; the oil
//       mesh 0x11E92F8 (static vertices, model + 8): C (z the other way) scaled by sin(s / 4) + 0x400 (the diagonal
//       0x11F0360 / 0x11F0368 / 0x11F0370 of an exe matrix, rewritten every tick) at P blended to
//       the target point (on the floor) by s, composed with the camera, drawn by 0x6AC320 (U scrolled
//       by (k & 3) << 5); then two chunk tasks at that point (CRT rand: a direction through C,
//       length scaled by IR0 = rand & 0x7F + 0x80, three angles, three spins).
//     Counters 0x1C..0x35: on the first the basis D of (point 0x13 - target, z the other way) on the floor and layout
//       0x11ECDC0 (+0x2A4); D's translation = the target - D's z column / 4 (y = 0); on the first 6
//       counters 10 spray tasks there (CRT rand: a direction through C, length scaled by IR0 = rand &
//       0xFF + 0x100, scale, deceleration = -velocity / 16); the layout at D through the camera (mode
//       1, depth -0x100: objects 3 / 4 by 0x7043B0, the others by 0x572200).
//     Sound at counter 0xA, damage at counter 0x2D (ApplyActionResultToTarget of the action's
//     targets), ends at counter 0x3C.
//   ChunkTask (0x6AC1F0) - model 0x11EACC0 by 0x572200, rotation from its angles, shrinking over its
//     last 3 frames (scale (0x1E - frame) << 10 from frame 0x1B); gravity 0x10, bounces on y >= 0
//     (velocity y negated), angles += spins; 0x1E frames.
//   SprayTask (0x6AC140) - sprite sequence 0x11E91A0 at its position (scale +0x1A), 12 frames;
//     velocity += deceleration, position += velocity.
//   PrimCallback (0x6ACF40) draws every layout object: rotation from the record's angles (flag
//     0x40000: 0x7015B0), its offset through the block's matrix, composed with it unless flag 0x8000;
//     + the block's translation; scale3DMatrix; fade; block mode 1 objects 3 / 4: 0x7043B0 at the
//     block's depth, else Effect_RenderPrimModel 0x572200.
//   Mesh renderer (0x6AC320): list 1 flat textured quads (0x6AC380), list 2 Gouraud triangles
//     (0x6AC770), list 3 Gouraud quads (0x6AC910), a skipped word, list 4 Gouraud textured quads
//     (0x6ACB30); the textured ones with the U scroll between two texture windows.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the blast's counter after its draws, a layout in its
// play, a chunk / spray after its draw).
// ENTITY / CAMERA: no task writes an entity position (+0x1C..+0x20 only read, through 0x502170) or
// the camera words (the camera animation is the engine's).
// EXE DATA WRITES: the scale matrix diagonal 0x11F0360 / 0x11F0368 / 0x11F0370 (counters 0x15..0x1C).
// Module globals: 0x2520CC0..0x2520D28 (caster entity 0x2520CC0, texture file 0x2520CC4, magic
// buffer base 0x2520CC8, context 0x2520CCC, master pool 0x2520CD0, packet cursor 0x2520CE4, the
// queues 0x2520CE8 sprays / 0x2520CF8 chunks / 0x2520D08 blast / 0x2520D18 master).
// NEVER-WRITTEN STACK WORDS (none reaches the output): the play block's depth (entry esp - 0xC) on
// counters 0x12..0x1B (mode 0: the callback reads it for mode 1 only); the blend point's 4th word
// (entry esp - 0x3E: copied into a chunk's +0x12, then overwritten by 0); the block / chunk matrix
// pad words (GTE R33 pad 0x1CA928E only); the callback's offset 4th word (GTE VZ0 high half
// 0x1CA8A16 only).

#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace oil137
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520CC0); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520CC8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520CCC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520CE4); }
	inline TaskQueue *QSprays() { return (TaskQueue *)0x2520CE8; }         // pool: magic buffer, 0x3C x 0x24
	inline TaskQueue *QChunks() { return (TaskQueue *)0x2520CF8; }         // pool: + 0x870, 0x20 x 0x2C
	inline TaskQueue *QBlast() { return (TaskQueue *)0x2520D08; }          // pool: + 0xDF0, 1 x 0x5A0

	static const uint32_t ORIG_MasterTask = 0x6AB900;
	static const uint32_t ORIG_BlastTask = 0x6ABA50;
	static const uint32_t ORIG_SprayTask = 0x6AC140;
	static const uint32_t ORIG_ChunkTask = 0x6AC1F0;
	static const uint32_t VERTS = 0x1390;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1658;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11658;
	static const uint32_t MESH = 0x11E92F8;              // oil mesh (vertices at + 8)
	static const uint32_t MODEL_Puddle = 0x11EAE10;      // prim-model layouts
	static const uint32_t MODEL_Spout = 0x11EB4D8;
	static const uint32_t MODEL_Blast = 0x11ECDC0;
	static const uint32_t MODEL_Chunk = 0x11EACC0;       // chunk model (Effect_RenderPrimModel)
	static const uint32_t SEQ_Spray = 0x11E91A0;         // sprite sequence
	static const void *const SCALE_MAT = (const void *)0x11F0360;   // exe matrix, its diagonal rewritten
	static int16_t *const SCALE_DIAG = (int16_t *)0x11F0360;        // words 0 / 4 / 8
	static const void *const SOUND_Blast = (const void *)0x11F035C;

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // MAG_063_sub_701270
	inline void Matrix3x3MultiplyPSX(const Mat4x3 *a, const void *b, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, const void *, Mat4x3 *)>(0x56C090)(a, b, out); }
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void ScaleMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); } // scale3DMatrix
	inline void DirectionFromAngles(int32_t pitch, int32_t yaw, int16_t *out) { fn<void (__cdecl *)(int32_t, int32_t, int16_t *)>(0x6A6020)(pitch, yaw, out); } // MAG_063_sub_6A6020

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520CD0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t started;   // +0x0E pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct BlastNode // pool of 1 node of 0x5A0 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *target;   // +0x10 first target of action 0
		int16_t start[4];  // +0x14 the mesh's start point (k = 0; y and +0x1A: 0 from the clear)
		int16_t aim[4];    // +0x1C the target's effect point 0xF1
		uint8_t pad24[8];
		int16_t puddle1[4]; // +0x2C the caster's point 0x1E (y = 0)
		int16_t puddle2[4]; // +0x34 the caster's point 0x1C (y = 0)
		int16_t spout[4];  // +0x3C the caster's point 0x13 (y = 0)
		Mat4x3 basisC;     // +0x44 towards the target (translation unused)
		Mat4x3 basisD;     // +0x64 from the target, its translation (+0x78) rewritten every tick
		uint8_t lay1[0x40];   // +0x84 layout 0x11EAE10
		uint8_t lay2[0x40];   // +0xC4 layout 0x11EAE10
		uint8_t layS[0x1A0];  // +0x104 layout 0x11EB4D8
		uint8_t layB[0x2FC];  // +0x2A4 layout 0x11ECDC0
	};
	struct ChunkNode // pool of 0x20 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
		int16_t vel[3];    // +0x14
		int16_t pad1A;     // never written (pool residue)
		int16_t angle[3];  // +0x1C
		int16_t pad22;
		int16_t spin[3];   // +0x24
		int16_t pad2A;
	};
	struct SprayNode // pool of 0x3C nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12 (also the 4th word of pos)
		int16_t vel[3];    // +0x14
		int16_t scale;     // +0x1A
		int16_t accel[3];  // +0x1C
		int16_t pad22;
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 rotation (pad word +0x12 never written) and anchor
		int32_t depth;     // +0x20 prim header +0x18 (renderer 0x7043B0; never written in mode 0)
		int32_t mode;      // +0x24 1: 0x7043B0 for objects 3 / 4
		uint32_t morph;    // +0x28 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BlastNode) == 0x5A0, "Oil Blast nodes");
	static_assert(sizeof(ChunkNode) == 0x2C && sizeof(SprayNode) == 0x24, "Oil Blast nodes");
	static_assert(sizeof(PrimArg) == 0x2C, "Oil Blast prim block");

	static uint8_t *&Cursor(uint8_t *h, int off) { return *(uint8_t **)(h + off); }
	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	// ------------------------------------------------------------------
	// Mesh renderer (0x6AC320). Header (0x54, the blast's): +0x00 mesh, +0x04 U scroll, +0x08 /
	// +0x10 texture windows, +0x14 U wrap byte, +0x18 vertices, +0x1C list cursor, +0x28 OTZ,
	// +0x2C FLAG.
	// ------------------------------------------------------------------
	static void ScrollU(uint8_t *p, const int *off, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < 4; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < 4; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	// the main packet between the texture windows +0x08 / +0x10 (no draw mode)
	static uint8_t *Windowed(const uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 8);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0xC;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x10);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x18;
	}

	// a quad's screen clip bits (x 1 / 2 / 4 / 8, y 0x10 / 0x20 / 0x40 / 0x80): drawn unless all four off
	// one side
	static bool QuadClipped(const uint8_t *x0, const uint8_t *x1, const uint8_t *x2, uint32_t clip3, const uint8_t *x3)
	{
		uint32_t clip = clip3;
		if (OffX(*(const int16_t *)x3)) clip |= 8;
		if (OffY(*(const int16_t *)(x3 + 2))) clip |= 0x80;
		(void)x0; (void)x1; (void)x2;
		return (clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0;
	}
	static uint32_t TriClip(const uint8_t *a, const uint8_t *b, const uint8_t *c)
	{
		uint32_t clip = 0;
		if (OffX(*(const int16_t *)a)) clip = 1;
		if (OffX(*(const int16_t *)b)) clip |= 2;
		if (OffX(*(const int16_t *)c)) clip |= 4;
		if (OffY(*(const int16_t *)(a + 2))) clip |= 0x10;
		if (OffY(*(const int16_t *)(b + 2))) clip |= 0x20;
		if (OffY(*(const int16_t *)(c + 2))) clip |= 0x40;
		return clip;
	}

	// MAG_137_sub_6AC380: list 1, flat textured quads (0x18-byte records, 0x28-byte packets)
	static uint32_t FlatTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x1C);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 0x18);
		Cursor(h, 0x1C) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x1C) = uv;
			*(uint32_t *)(p + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x2C);
			if (*(const uint32_t *)(h + 0x2C) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x10, p + 0x18);
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (QuadClipped(p + 8, p + 0x10, p + 0x18, clip, p + 0x20)) continue;
			GteReadOTZReg(h + 0x28);
			static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 };
			ScrollU(p, us, *(const uint16_t *)(h + 4), h[0x14]);
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x28) >> shift);
			p = Windowed(h, otp, p, p + 0x28);
		}
		Cursor(h, 0x1C) = rec;
		return (uint32_t)p;
	}

	// MAG_137_sub_6AC770: list 2, Gouraud triangles (0x14-byte records, 0x1C-byte packets)
	static uint32_t GouraudTris(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x1C);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 0x18);
		Cursor(h, 0x1C) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x14)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x06000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			GteReadFLAG(h + 0x2C);
			if (*(const uint32_t *)(h + 0x2C) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteAVSZ3();
			const uint32_t clip = TriClip(p + 8, p + 0x10, p + 0x18);
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteReadOTZReg(h + 0x28);
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			InsertPrimAutoDepth(ot + 4 * (*(const int32_t *)(h + 0x28) >> shift), p);
			p += 0x1C;
		}
		Cursor(h, 0x1C) = rec;
		return (uint32_t)p;
	}

	// MAG_137_sub_6AC910: list 3, Gouraud quads (0x18-byte records, 0x24-byte packets)
	static uint32_t GouraudQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x1C);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 0x18);
		Cursor(h, 0x1C) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x08000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			GteReadFLAG(h + 0x2C);
			if (*(const uint32_t *)(h + 0x2C) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x10, p + 0x18);
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (QuadClipped(p + 8, p + 0x10, p + 0x18, clip, p + 0x20)) continue;
			GteReadOTZReg(h + 0x28);
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x14);
			InsertPrimAutoDepth(ot + 4 * (*(const int32_t *)(h + 0x28) >> shift), p);
			p += 0x24;
		}
		Cursor(h, 0x1C) = rec;
		return (uint32_t)p;
	}

	// MAG_137_sub_6ACB30: list 4, Gouraud textured quads (0x24-byte records, 0x34-byte packets)
	static uint32_t GouraudTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x1C);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 0x18);
		Cursor(h, 0x1C) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x24)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x0C000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x24) = uv;
			*(uint32_t *)(p + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x2C);
			if (*(const uint32_t *)(h + 0x2C) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x14, p + 0x20);
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (QuadClipped(p + 8, p + 0x14, p + 0x20, clip, p + 0x2C)) continue;
			GteReadOTZReg(h + 0x28);
			*(uint32_t *)(p + 0x10) = *(const uint32_t *)(rec + 0x18);
			*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x1C);
			*(uint32_t *)(p + 0x28) = *(const uint32_t *)(rec + 0x20);
			static const int us[4] = { 0xC, 0x18, 0x24, 0x30 };
			ScrollU(p, us, *(const uint16_t *)(h + 4), h[0x14]);
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x28) >> shift);
			p = Windowed(h, otp, p, p + 0x34);
		}
		Cursor(h, 0x1C) = rec;
		return (uint32_t)p;
	}

	// MAG_137_sub_6AC320
	static uint32_t MeshDraw(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x18) = m + 8;
		Cursor(h, 0x1C) = m + *(const uint32_t *)m + 0xC;
		uint32_t r = FlatTexQuads(h, ot, shift, cursor);
		r = GouraudTris(h, ot, shift, r);
		r = GouraudQuads(h, ot, shift, r);
		Cursor(h, 0x1C) += 4;
		return GouraudTexQuads(h, ot, shift, r);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6ACF40): one object of a layout. Vertex frame (or blend into the
	// block's morph buffer), rotation from the record's angles (flag 0x40000: 0x7015B0); the offset
	// through the block's matrix, composed with it unless flag 0x8000; + the block's translation;
	// scale3DMatrix, fade; block mode 1 objects 3 / 4: 0x7043B0 at the block's depth, else 0x572200.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		GteSetRotMatrixCtrl(&arg->m);
		GteLoadV0(off);
		GteMVMVA_RotV0();
		GteReadMAC123(m.t);
		if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			ScaleMatrix(&m, v);
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
		if (arg->mode == 1 && (r->flags_lo == 3 || r->flags_lo == 4))
		{
			*(int32_t *)(h + 0x18) = arg->depth;
			PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		}
		else PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	static void Play(void *layout, const PrimArg *arg)
	{
		prim::play((prim::Layout *)layout, PrimCallback, (int)arg, 0);
	}

	// the block at a point through the camera: the camera rotation, the point through it + the
	// camera translation
	static void BlockAt(PrimArg *arg, const int16_t *p4)
	{
		memcpy(arg->m.m, Camera().m, sizeof(arg->m.m));
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(p4);
		GteMVMVA_RotV0();
		GteReadMAC123(arg->m.t);
		arg->m.t[0] = (int32_t)((uint32_t)arg->m.t[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)arg->m.t[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)arg->m.t[2] + (uint32_t)Camera().t[2]);
	}

	// the spout layout's block: RotY(caster facing) at the point, composed with the camera
	static void SpoutBlock(PrimArg *arg, const int16_t *p)
	{
		RotY(*(const int16_t *)(Caster() + 0xE), &arg->m);
		arg->m.t[0] = p[0];
		arg->m.t[1] = p[1];
		arg->m.t[2] = p[2];
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
	}

	// the oil mesh at progress s (k << 9): C scaled by sin(s / 4) + 0x400 (the exe matrix diagonal), at
	// the start point blended to the target point (on the floor) by s
	static void MeshAt(const BlastNode *w, int32_t s, Mat4x3 *M, int16_t *Q)
	{
		const int16_t sc = (int16_t)(ComputeSin(s >> 2) + 0x400);
		SCALE_DIAG[8] = sc;
		SCALE_DIAG[4] = sc;
		SCALE_DIAG[0] = sc;
		Matrix3x3MultiplyPSX(&w->basisC, SCALE_MAT, M);
		Q[0] = w->aim[0];
		Q[2] = w->aim[2];
		Q[1] = 0;
		BlendVectors(w->start, Q, 0x1000 - s, s, Q);
		M->t[0] = Q[0];
		M->t[1] = Q[1];
		M->t[2] = Q[2];
	}

	// the mesh composed with the camera, drawn by 0x6AC320 (U scroll u)
	static void MeshRender(Mat4x3 *M, uint16_t u)
	{
		ComposeAffineTransform(&Camera(), M, M);
		GteSetRotMatrix(M);
		GteSetTransVector(M);
		uint8_t *h = (uint8_t *)FieldAlloc(0x54);
		*(uint32_t *)h = MESH;
		*(uint16_t *)(h + 0xA) = 0;
		*(uint16_t *)(h + 8) = 0;
		*(uint16_t *)(h + 0xE) = 0x100;
		*(uint16_t *)(h + 0xC) = 0x100;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x14) = 0x80;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 4) = u;
		PacketCursor() = MeshDraw(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x54);
	}

	// a direction through C (CRT rand: yaw first, then pitch), its length scaled by IR0 = ir0
	static void RandomVelocity(const BlastNode *w, int16_t *vel, int32_t mask, int32_t base)
	{
		const int32_t yaw = (CrtRand() * 2000 >> 15) - 1000;
		const int32_t pitch = CrtRand() * 800 >> 15;
		DirectionFromAngles(pitch, yaw, vel);
		MatrixMultiplyVector(&w->basisC, vel, vel);
		GteSetIR0((CrtRand() & mask) + base);
		GteLoadIR123(vel);
		GteGPF();
		GteStoreIR123(vel);
	}

	// the floor basis of (a - b) with z the other way (y = 0)
	static void FloorBasis(const int16_t *a, const int16_t *b, Mat4x3 *M)
	{
		const int32_t v[3] = { (int32_t)a[0] - b[0], 0, (int32_t)b[2] - a[2] };
		BuildOrthonormalBasis(v, M);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag137_oil_blast_held.h"
#endif

namespace ff8fx
{
namespace oil137
{
	// ------------------------------------------------------------------
	// Sprays (0x6AC140)
	// ------------------------------------------------------------------
	static void SprayDraw(const int16_t *pos, int16_t scale, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, scale, -(scale >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Spray;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SprayTask(TaskNode *n)
	{
		SprayNode *s = (SprayNode *)n;
		// 30 fps layer: see mag137_oil_blast_held.inc
		FX_HELD(held_note_spray(n);)
		SprayDraw(s->pos, s->scale, s->frame);
		s->vel[0] = (int16_t)(s->vel[0] + s->accel[0]);
		s->vel[1] = (int16_t)(s->vel[1] + s->accel[1]);
		s->vel[2] = (int16_t)(s->vel[2] + s->accel[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->frame++;
		return s->frame < 0xC ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Chunks (0x6AC1F0)
	// ------------------------------------------------------------------
	// the chunk model at pos, rotation from angles, scaled by scale (when scaled)
	static void ChunkDraw(const int16_t *angle, bool scaled, int32_t scale, const int16_t *pos)
	{
		Mat4x3 M;
		BuildRotationMatrixFromAngles(angle, &M);
		if (scaled)
		{
			const int32_t v[3] = { scale, scale, scale };
			ScaleMatrix(&M, v);
		}
		M.t[0] = pos[0];
		M.t[1] = pos[1];
		M.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &M, &M);
		GteSetRotMatrix(&M);
		GteSetTransVector(&M);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Chunk;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl ChunkTask(TaskNode *n)
	{
		ChunkNode *c = (ChunkNode *)n;
		// 30 fps layer: see mag137_oil_blast_held.inc
		FX_HELD(held_note_chunk(n);)
		ChunkDraw(c->angle, c->frame > 0x1A, shl32(0x1E - c->frame, 0xA), c->pos);
		c->vel[1] = (int16_t)(c->vel[1] + 0x10);
		const int16_t vy = c->vel[1];
		c->pos[1] = (int16_t)(c->pos[1] + vy);
		c->pos[0] = (int16_t)(c->pos[0] + c->vel[0]);
		if (c->pos[1] >= 0) c->vel[1] = (int16_t)-vy;
		c->pos[2] = (int16_t)(c->pos[2] + c->vel[2]);
		c->angle[0] = (int16_t)(c->angle[0] + c->spin[0]);
		c->angle[1] = (int16_t)(c->angle[1] + c->spin[1]);
		c->angle[2] = (int16_t)(c->angle[2] + c->spin[2]);
		c->frame++;
		return c->frame < 0x1E ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Blast (0x6ABA50)
	// ------------------------------------------------------------------
	static uint32_t __cdecl BlastTask(TaskNode *n)
	{
		BlastNode *w = (BlastNode *)n;
		PrimArg arg;      // entry esp - 0x2C (depth: never written on counters 0x12..0x1B)
		arg.depth = 0;
		const int32_t ka = w->counter - 0x12;
		if ((uint32_t)ka < 9)
		{
			if (ka == 0)
			{
				GetEffectSpawnPosition(Caster(), 0x1E, 0, w->puddle1);
				w->puddle1[1] = 0;
				GetEffectSpawnPosition(Caster(), 0x1C, 0, w->puddle2);
				w->puddle2[1] = 0;
				DecodeModelPrimLayout(MODEL_Puddle, w->lay1, 0x40);
				DecodeModelPrimLayout(MODEL_Puddle, w->lay2, 0x40);
			}
			BlockAt(&arg, w->puddle1);
			arg.mode = 0;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag137_oil_blast_held.inc
			FX_HELD(held_note_play(PLAY_PUDDLE1, n, (prim::Layout *)w->lay1);)
			Play(w->lay1, &arg);
			BlockAt(&arg, w->puddle2);
			// 30 fps layer: see mag137_oil_blast_held.inc
			FX_HELD(held_note_play(PLAY_PUDDLE2, n, (prim::Layout *)w->lay2);)
			Play(w->lay2, &arg);
		}
		const int32_t kb = w->counter - 0x15;
		if ((uint32_t)kb < 0x14)
		{
			if (kb == 0)
			{
				GetEffectSpawnPosition(Caster(), 0x13, 0, w->spout);
				w->spout[1] = 0;
				DecodeModelPrimLayout(MODEL_Spout, w->layS, 0x1A0);
			}
			SpoutBlock(&arg, w->spout);
			arg.mode = 0;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag137_oil_blast_held.inc
			FX_HELD(held_note_play(PLAY_SPOUT, n, (prim::Layout *)w->layS);)
			Play(w->layS, &arg);
		}
		const int32_t k = w->counter - 0x15;
		if ((uint32_t)k < 8)
		{
			if (k == 0)
			{
				FloorBasis(w->aim, w->spout, &w->basisC);
				w->start[0] = (int16_t)((w->basisC.m[0][2] >> 4) + w->spout[0]);
				w->start[1] = 0;
				w->start[2] = (int16_t)((w->basisC.m[2][2] >> 4) + w->spout[2]);
			}
			Mat4x3 M;
			int16_t Q[4];  // entry esp - 0x44 (the 4th word never written)
			Q[3] = 0;
			MeshAt(w, shl32(k & 0xFFFFF, 9), &M, Q);
			// 30 fps layer: see mag137_oil_blast_held.inc
			FX_HELD(held_note_mesh(n);)
			MeshRender(&M, (uint16_t)((k & 3) << 5));
			for (int i = 2; i; i--)
			{
				ChunkNode *c = (ChunkNode *)AddTaskToQueue(QChunks(), ORIG_ChunkTask);
				if (!c) continue;
				memcpy(c->pos, Q, 8);
				c->frame = 0;
				RandomVelocity(w, c->vel, 0x7F, 0x80);
				c->angle[0] = (int16_t)CrtRand();
				c->angle[1] = (int16_t)CrtRand();
				c->angle[2] = (int16_t)CrtRand();
				c->spin[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				c->spin[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				c->spin[2] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			}
		}
		const int32_t kd = w->counter - 0x1C;
		if ((uint32_t)kd < 0x1A)
		{
			if (kd == 0)
			{
				FloorBasis(w->spout, w->aim, &w->basisD);
				DecodeModelPrimLayout(MODEL_Blast, w->layB, 0x2FC);
			}
			w->basisD.t[1] = 0;
			w->basisD.t[0] = w->aim[0] - (w->basisD.m[0][2] >> 2);
			w->basisD.t[2] = w->aim[2] - (w->basisD.m[2][2] >> 2);
			if ((uint32_t)kd < 6)
			{
				for (int i = 0xA; i; i--)
				{
					SprayNode *s = (SprayNode *)AddTaskToQueue(QSprays(), ORIG_SprayTask);
					s->pos[0] = (int16_t)w->basisD.t[0];
					s->pos[2] = (int16_t)w->basisD.t[2];
					s->pos[1] = 0;
					s->frame = 0;
					RandomVelocity(w, s->vel, 0xFF, 0x100);
					s->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
					s->accel[2] = (int16_t)(-(int32_t)s->vel[2] >> 4);
					s->accel[0] = (int16_t)(-(int32_t)s->vel[0] >> 4);
					s->accel[1] = (int16_t)(-(int32_t)s->vel[1] >> 4);
				}
			}
			ComposeAffineTransform(&Camera(), &w->basisD, &arg.m);
			arg.morph = TexBase() + VERTS;
			arg.depth = -0x100;
			arg.mode = 1;
			// 30 fps layer: see mag137_oil_blast_held.inc
			FX_HELD(held_note_play(PLAY_BLAST, n, (prim::Layout *)w->layB);)
			Play(w->layB, &arg);
		}
		if (w->counter == 0xA) BdPlaySE(SOUND_Blast, 0, 0x80);
		if (w->counter == 0x2D) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (w->counter >= 0x3C) return TASK_END;
		w->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6AB900)
	// ------------------------------------------------------------------
	static void StartBlast()
	{
		uint8_t *target = Entity(Ctx()->actions->targets[0]);
		InitTaskQueuePool(QBlast(), (void *)(TexBase() + 0xDF0), 0x5A0, 1);
		InitTaskQueuePool(QChunks(), (void *)(TexBase() + 0x870), 0x2C, 0x20);
		InitTaskQueuePool(QSprays(), (void *)TexBase(), 0x24, 0x3C);
		BlastNode *w = (BlastNode *)AddTaskToQueue(QBlast(), ORIG_BlastTask);
		Memset32(&w->counter, 0, 0x165);
		w->target = target;
		GetEffectSpawnPosition(target, 0xF1, 0, w->aim);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag137_oil_blast_held.inc
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
			m->started = 1;
			StartBlast();
		}
		int r;
		if (m->started)
		{
			r = ExecuteTaskQueue(QBlast());
			ExecuteTaskQueue(QChunks());
			ExecuteTaskQueue(QSprays());
		}
		else r = (int)n;
		if (m->started && r == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag137_oil_blast()
	{
		register_port(oil137::ORIG_MasterTask, (void *)oil137::MasterTask, "137 MasterTask", 137);
		register_port(oil137::ORIG_BlastTask, (void *)oil137::BlastTask, "137 BlastTask", 137);
		register_port(oil137::ORIG_ChunkTask, (void *)oil137::ChunkTask, "137 ChunkTask", 137);
		register_port(oil137::ORIG_SprayTask, (void *)oil137::SprayTask, "137 SprayTask", 137);
		// 30 fps layer: see mag137_oil_blast_held.inc
		FX_HELD(register_mag137_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag137_oil_blast_held.inc"
#endif
