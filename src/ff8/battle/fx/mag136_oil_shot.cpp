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

// Effect 136: Oil Shot (enemy attack 180 of kernel.bin, Oilboyle c0m099; MAG_136_*). No other
// kernel.bin entry uses effect 136.
//
// Structure (setup MAG_136_OIL_SHOT 0x6AD1A0 -> _Init 0x6AD1D0, file loader 0x6AD1B0 = the
// texture file named at 0x11FA1FC; the setup keeps the CASTER's entity (0x2520D28), starts camera
// animation 0x11F7EDC and queues the TIM; setup / loader / _Init are the copies of effects 126..135):
//   MasterTask (0x6AD250) - alternates the packet arena (magic buffer + 0x416C / + 0x1C16C); on
//     counter 1 sets up the pools (falling drops 0x10 x 0x2BC at + 0, splats 8 x 0x14 at + 0x2BC0,
//     stream drops 0x10 x 0xC4 at + 0x2C60, sprays 8 x 0x20 at + 0x38A0, shot 1 x 0x46C at + 0x39A0),
//     starts the shot (the first target of action 0, its default effect position, layouts
//     0x11F40F4 / 0x11F5A90 / 0x11F4E1C) and normalises the oil mesh 0x11F2684 (0x6AD430: w of every
//     vertex = (z - min) * 4096 / (max - min) over the vertices 2..count, one past the end; this
//     static exe table is rewritten on EVERY cast, with the same values). Then the shot, spray,
//     stream-drop, splat and falling-drop queues; ends when the SHOT queue is empty (the other
//     queues are abandoned).
//   ShotTask (0x6AD480) - counters 0..0x52. Screen flash 0xF0F0F0 from counter 0x1E (rise over 4
//     ticks to 0x708, hold, fall from counter 0x46). Counters 0x1E..0x25: M (the caster's mouth:
//     its bone 27 through 0x11F81F0, 0x6AEE20), a spray sprite task (CRT rand: angle, spin,
//     scale, direction RotX(-0x200) x RotY(caster facing) applied to a random unit vector, its
//     length scaled by IR0 = rand & 0x7F + 0x80), layout +0x2C at M (depth -0x80). Counter 0x1E:
//     the shot's start = M. Counters 0x1F..0x2A (k = counter - 0x1E, s = k * 4096 / 12): the oil
//     mesh: its vertices through RotZ(k << 8) (no translation) into magic buffer + 0x3E0C, each y
//     lowered by table[w / 2] x sin(s / 2) >> 17 (0x6ADFF0); P = the start blended to the target by
//     s, y - sin(s / 2) / 8; the basis of (P - prev, with y and z the other way and z - 100), prev
//     = the last P (node +0x24); a stream-drop task (CRT rand: a point on a circle of radius 0x40 in
//     that basis, velocity on a circle of radius 0x10 + 0x32 forward); the basis scaled in z by
//     sin(s / 2) - sin(0x400 - s / 4) / 2 + 0x800 (word 0x11F81E0 of the exe matrix 0x11F81D0,
//     rewritten every tick) at P, composed with the camera; the mesh drawn by 0x6AE9B0 (U scrolled
//     by (k & 7) << 4). Counters 0x26..0x3F: layout +0xF4 at the target (depth -0x80, objects drawn
//     by the module renderer 0x6AE3B0); on even counters up to 0x34 a falling drop (CRT rand:
//     direction, speed, rise), on counters 0x2A..0x39 a splat sprite (random offset). Counters
//     0x2C..0x51: layout +0x2E8 at the target - 0xFA in y (depth 0, module renderer). Sound at
//     counter 0x1E, damage at counter 0x3E (ApplyActionResultToTarget of the action's targets).
//   SprayTask (0x6AE300) - sprite sequence 0x11F2390 (rotation +0x0C, scale +0x16) through the
//     shadow-rotation camera, 9 frames; position += velocity, rotation += spin.
//   StreamTask (0x6AE210) - layout +0x1C at its position (depth 0, objects 0 by the module
//     renderer); position += velocity; ends with the layout.
//   SplatTask (0x6AE190) - sprite sequence 0x11F24D8 at its position, 16 frames.
//   FallTask (0x6AE050) - layout 0x11F7D84 falling (gravity 0x10) until y >= 0, then the splash
//     layout 0x11F74FC at y = 0 (objects by the module renderer); ends with the splash layout.
//   PrimCallback (0x6ADC50, Quezacotl's 0x6C53A0 with the block's renderer choice) draws every
//     layout object: flag 0x1000 relative to the never-written module matrix 0x2520DB0 (zero),
//     0x200 absolute, else relative to the block's matrix; block mode 1 (or 2 for object 0): the
//     module renderer 0x6AE3B0 (flat textured quads 0x6AE420 with a depth-cue fade, Gouraud quads
//     0x6AE700 with the fade added to the OTZ), else 0x7043B0 at the block's depth.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the shot's counter after its draws, a layout in its
// play, a sprite after its draw).
// ENTITY / CAMERA: no task writes an entity position (+0x1C..+0x20 only read) or the camera words
// (the camera animation is the engine's); the shot's mouth point rebuilds the caster's bone
// matrices from its pose (0x508C90). The screen flash level is the engine's (0x5713E0).
// EXE DATA WRITES: the oil mesh 0x11F2684 (vertex w words, every cast) and the word 0x11F81E0
// (counters 0x1F..0x2A).
// Module globals: 0x2520D28..0x2520DD0 (caster entity 0x2520D28, texture file 0x2520D2C, magic
// buffer base 0x2520D30, context 0x2520D34, master pool 0x2520D38, packet cursor 0x2520D4C, the
// queues 0x2520D50 falling drops / 0x2520D60 splats / 0x2520D70 stream drops / 0x2520D80 sprays /
// 0x2520D90 shot / 0x2520DA0 master, the matrix 0x2520DB0 (never written)).
// NEVER-WRITTEN STACK WORDS: ShotTask's P word +6 (entry esp - 0x3E; 0x56CB90 writes 3 words) is
// copied into the node's previous-point word +0x2A on counters 0x1F..0x2A (0x6AD75B); it is never
// read afterwards (see ShotResidue). The 4th words of the vectors loaded into GTE V0 (the target
// point - 0xFA, the callback's record offset, the mesh output) reach the GTE's VZ0 high half
// (0x1CA8A16) only; the block matrices' pad words only reach the GTE R33 pad (0x1CA928E).

#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace oil136
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520D28); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520D30); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520D34); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520D4C); }
	inline Mat4x3 &ZeroMatrix() { return var<Mat4x3>(0x2520DB0); }        // never written
	inline TaskQueue *QFalls() { return (TaskQueue *)0x2520D50; }          // pool: magic buffer, 0x10 x 0x2BC
	inline TaskQueue *QSplats() { return (TaskQueue *)0x2520D60; }         // pool: + 0x2BC0, 8 x 0x14
	inline TaskQueue *QStreams() { return (TaskQueue *)0x2520D70; }        // pool: + 0x2C60, 0x10 x 0xC4
	inline TaskQueue *QSprays() { return (TaskQueue *)0x2520D80; }         // pool: + 0x38A0, 8 x 0x20
	inline TaskQueue *QShot() { return (TaskQueue *)0x2520D90; }           // pool: + 0x39A0, 1 x 0x46C

	static const uint32_t ORIG_MasterTask = 0x6AD250;
	static const uint32_t ORIG_ShotTask = 0x6AD480;
	static const uint32_t ORIG_FallTask = 0x6AE050;
	static const uint32_t ORIG_SplatTask = 0x6AE190;
	static const uint32_t ORIG_StreamTask = 0x6AE210;
	static const uint32_t ORIG_SprayTask = 0x6AE300;
	static const uint32_t VERTS = 0x3E0C;                // magic buffer: mesh vertices / blended vertex frames
	static const uint32_t ARENA_A = 0x416C;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x1C16C;
	static uint8_t *const MESH = (uint8_t *)0x11F2684;   // oil mesh: size, count, vertices (w rewritten), lists
	static const uint32_t MODEL_Mouth = 0x11F40F4;       // prim-model layouts
	static const uint32_t MODEL_Hit = 0x11F5A90;
	static const uint32_t MODEL_Pool = 0x11F4E1C;
	static const uint32_t MODEL_Stream = 0x11F342C;
	static const uint32_t MODEL_Fall = 0x11F7D84;
	static const uint32_t MODEL_Splash = 0x11F74FC;
	static const uint32_t SEQ_Spray = 0x11F2390;         // sprite sequences
	static const uint32_t SEQ_Splat = 0x11F24D8;
	static const int16_t *const MOUTH = (const int16_t *)0x11F81F0;  // caster bone point (x, y, z, bone 27)
	static const void *const SCALE_Z = (const void *)0x11F81D0;       // exe matrix, its m[2][2] (0x11F81E0) rewritten
	static int16_t &ScaleZ() { return *(int16_t *)0x11F81E0; }
	static const int32_t *const WOBBLE = (const int32_t *)0x11F81F8;  // 0x801 x s32, by vertex w / 2
	static const void *const SOUND_Shot = (const void *)0x11F81CC;

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }   // MAG_063_sub_701220
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // MAG_063_sub_701270
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }   // MAG_065_sub_7012C0
	inline void Matrix3x3MultiplyPSX(const Mat4x3 *a, const Mat4x3 *b, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, const Mat4x3 *, Mat4x3 *)>(0x56C090)(a, b, out); }
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void GteZeroTrans() { fn<void (__cdecl *)()>(0x704070)(); }      // MAG_129_sub_704070: TRX = TRY = TRZ = 0
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }     // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                    // sub_45F270
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }             // set_param_with_dword_1CA8A68
	inline void ScaleMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); } // scale3DMatrix

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520D38)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t started;   // +0x0E pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct ShotNode // pool of 1 node of 0x46C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *target;   // +0x10 first target of action 0
		int16_t start[4];  // +0x14 the mouth at counter 0x1E (+0x1A: 0 from the clear)
		int16_t pos[4];    // +0x1C the target's default effect position
		int16_t prev[4];   // +0x24 the stream's last point (+0x2A: a stack word, see ShotResidue)
		uint8_t mouth[0xC8];  // +0x2C layout 0x11F40F4
		uint8_t hit[0x1F4];   // +0xF4 layout 0x11F5A90
		uint8_t pool[0x184];  // +0x2E8 layout 0x11F4E1C
	};
	struct FallNode // pool of 0x10 nodes of 0x2BC bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t landed;    // +0x12 (also the 4th word of pos)
		int16_t vel[3];    // +0x14
		int16_t pad1A;     // never written (pool residue)
		uint8_t layout[0x2A0]; // +0x1C falling 0x50 / splash 0xA8 bytes used
	};
	struct SplatNode // pool of 8 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12 (also the 4th word of pos)
	};
	struct StreamNode // pool of 0x10 nodes of 0xC4 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t zero;      // +0x12
		int16_t vel[3];    // +0x14
		int16_t pad1A;     // never written (pool residue)
		uint8_t layout[0xA8]; // +0x1C
	};
	struct SprayNode // pool of 8 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t angle;     // +0x0C sprite rotation
		int16_t spin;      // +0x0E
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18
		int16_t frame;     // +0x1E
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 rotation (pad word +0x12 never written) and anchor
		int32_t depth;     // +0x20 prim header +0x18 (renderer 0x7043B0)
		int32_t mode;      // +0x24 1: module renderer, 2: module renderer for object 0, else 0x7043B0
		uint32_t morph;    // +0x28 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(ShotNode) == 0x46C && sizeof(FallNode) == 0x2BC, "Oil Shot nodes");
	static_assert(sizeof(SplatNode) == 0x14 && sizeof(StreamNode) == 0xC4 && sizeof(SprayNode) == 0x20, "Oil Shot nodes");
	static_assert(sizeof(PrimArg) == 0x2C, "Oil Shot prim block");

	static uint8_t *&Cursor(uint8_t *h, int off) { return *(uint8_t **)(h + off); }
	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	// ------------------------------------------------------------------
	// Module renderer (0x6AE3B0 -> 0x6AE420 / 0x6AE700): the model's list 1 (flat textured quads,
	// 0x18-byte records) and list 3 (Gouraud quads, 0x18-byte records); list 2 is skipped. Header
	// (0x58, the callback's): +0x00 model, +0x04 vertices, +0x08 fade colour, +0x0C fade, +0x20 list
	// cursor, +0x2C OTZ, +0x30 FLAG.
	// ------------------------------------------------------------------
	// MAG_136_sub_6AE420: textured quads (0x28-byte packets) behind a draw mode 0xE1000220; a fade
	// (+0x0C) depth-cues the colour towards +0x08 (GTE DPCS)
	static uint32_t FlatQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x20);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x20) = rec;
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
			GteReadFLAG(h + 0x30);
			if (*(const uint32_t *)(h + 0x30) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(const int16_t *)(p + 8))) clip = 1;
			if (OffX(*(const int16_t *)(p + 0x10))) clip |= 2;
			if (OffX(*(const int16_t *)(p + 0x18))) clip |= 4;
			if (OffY(*(const int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(const int16_t *)(p + 0x12))) clip |= 0x20;
			if (OffY(*(const int16_t *)(p + 0x1A))) clip |= 0x40;
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (OffX(*(const int16_t *)(p + 0x20))) clip |= 8;
			if (OffY(*(const int16_t *)(p + 0x22))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x2C);
			if (*(const int32_t *)(h + 0xC) != 0)
			{
				GteSetFarColor(h[8], h[9], h[0xA]);
				GteLoadRGBC(p + 4);
				GteSetIR0(*(const int32_t *)(h + 0xC));
				GteDPCS();
				GteStoreRGB2(p + 4);
			}
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x2C) >> shift);
			uint8_t *md = p + 0x28;
			*(uint32_t *)md = 0x01000000;
			*(uint32_t *)(md + 4) = 0xE1000220;
			InsertPrimAutoDepth(otp, md);
			InsertPrimAutoDepth(otp, p);
			p += 0x30;
		}
		Cursor(h, 0x20) = rec;
		return (uint32_t)p;
	}

	// MAG_136_sub_6AE700: Gouraud quads (0x24-byte packets) between draw modes 0xE1000220 /
	// 0xE1000240; the fade (+0x0C) is added to the OTZ (a negative depth is not drawn)
	static uint32_t GouraudQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x20);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x20) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x08000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			GteReadFLAG(h + 0x30);
			if (*(const uint32_t *)(h + 0x30) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(const int16_t *)(p + 8))) clip = 1;
			if (OffX(*(const int16_t *)(p + 0x10))) clip |= 2;
			if (OffX(*(const int16_t *)(p + 0x18))) clip |= 4;
			if (OffY(*(const int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(const int16_t *)(p + 0x12))) clip |= 0x20;
			if (OffY(*(const int16_t *)(p + 0x1A))) clip |= 0x40;
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (OffX(*(const int16_t *)(p + 0x20))) clip |= 8;
			if (OffY(*(const int16_t *)(p + 0x22))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x2C);
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x14);
			const int32_t z = (int32_t)((uint32_t)*(const int32_t *)(h + 0x2C) + (uint32_t)*(const int32_t *)(h + 0xC));
			*(int32_t *)(h + 0x2C) = z;
			if (z < 0) continue;
			const uint32_t otp = ot + 4 * (z >> shift);
			uint8_t *md = p + 0x24;
			*(uint32_t *)md = 0x01000000;
			*(uint32_t *)(md + 4) = 0xE1000220;
			InsertPrimAutoDepth(otp, md);
			InsertPrimAutoDepth(otp, p);
			uint8_t *md2 = p + 0x2C;
			*(uint32_t *)md2 = 0x01000000;
			*(uint32_t *)(md2 + 4) = 0xE1000240;
			InsertPrimAutoDepth(otp, md2);
			p += 0x34;
		}
		Cursor(h, 0x20) = rec;
		return (uint32_t)p;
	}

	// MAG_136_sub_6AE3B0: list 1 (count at model + size + 0xC), list 2 skipped, list 3
	static uint32_t ObjectDraw(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x20) = m + *(const uint32_t *)m + 0xC;
		uint32_t r = cursor;
		if (*(const uint32_t *)Cursor(h, 0x20) != 0) r = FlatQuads(h, ot, shift, cursor);
		else Cursor(h, 0x20) += 4;
		Cursor(h, 0x20) += 4;
		if (*(const uint32_t *)Cursor(h, 0x20) != 0) return GouraudQuads(h, ot, shift, r);
		Cursor(h, 0x20) += 4;
		return r;
	}

	// ------------------------------------------------------------------
	// Oil mesh renderer (0x6AE9B0 -> 0x6AE9E0): Gouraud textured quads (0x24-byte records at model +
	// size + 0x1C, 0x34-byte packets), U scrolled by +0x10 (wrapping by the byte +0x20), each
	// between the texture windows +0x14 / +0x1C with a draw mode 0xE1000220. Header (0x5C, the
	// shot's): +0x00 mesh, +0x04 vertices, +0x24 list cursor, +0x30 OTZ, +0x34 FLAG.
	// ------------------------------------------------------------------
	static void ScrollU(uint8_t *p, const int *off, int n, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < n; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < n; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	static uint8_t *Windowed(const uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 0x14);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		uint8_t *md = p + 0xC;
		*(uint32_t *)md = 0x01000000;
		*(uint32_t *)(md + 4) = 0xE1000220;
		InsertPrimAutoDepth(otp, md);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0x14;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x1C);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x20;
	}

	static uint32_t MeshQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x24);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x24) = rec;
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
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(const int16_t *)(p + 8))) clip = 1;
			if (OffX(*(const int16_t *)(p + 0x14))) clip |= 2;
			if (OffX(*(const int16_t *)(p + 0x20))) clip |= 4;
			if (OffY(*(const int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(const int16_t *)(p + 0x16))) clip |= 0x20;
			if (OffY(*(const int16_t *)(p + 0x22))) clip |= 0x40;
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (OffX(*(const int16_t *)(p + 0x2C))) clip |= 8;
			if (OffY(*(const int16_t *)(p + 0x2E))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x30);
			*(uint32_t *)(p + 0x10) = *(const uint32_t *)(rec + 0x18);
			*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x1C);
			*(uint32_t *)(p + 0x28) = *(const uint32_t *)(rec + 0x20);
			static const int us[4] = { 0xC, 0x18, 0x24, 0x30 };
			ScrollU(p, us, 4, *(const uint16_t *)(h + 0x10), h[0x20]);
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x30) >> shift);
			p = Windowed(h, otp, p, p + 0x34);
		}
		Cursor(h, 0x24) = rec;
		return (uint32_t)p;
	}

	// MAG_136_sub_6AE9B0
	static uint32_t MeshDraw(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x24) = m + *(const uint32_t *)m + 0x1C;
		return MeshQuads(h, ot, shift, cursor);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6ADC50, Quezacotl's 0x6C53A0 with the block's renderer choice): one
	// object of a layout. Vertex frame (or blend into the block's morph buffer), rotation from the
	// record's angles (flag 0x40000: 0x7015B0); flag 0x1000: the offset through the module matrix
	// 0x2520DB0, composed with it; 0x200: the offset as the translation; else the offset through the
	// block's matrix, composed with it; + the block's translation; scale (flag 0x100: 3x3 product,
	// else scale3DMatrix), fade; drawn by the module renderer (block mode 1, or mode 2 for object 0)
	// or 0x7043B0 at the block's depth.
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
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x1000)
		{
			GteSetRotMatrixCtrl(&ZeroMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&ZeroMatrix(), &m);
		}
		else if (r->flags & 0x200)
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
			MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
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
				ScaleMatrix(&m, v);
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
		if (arg->mode == 1 || (arg->mode == 2 && r->flags_lo == 0))
			PacketCursor() = ObjectDraw(h, RenderOT(), 2, PacketCursor());
		else
		{
			*(int32_t *)(h + 0x18) = arg->depth;
			PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// MAG_136_sub_6AD430: w of every vertex = (z - min) * 4096 / (max - min), the z range over the
	// vertices 2..count (one past the end)
	static void NormalizeMesh(uint8_t *model)
	{
		const int32_t count = *(const int32_t *)(model + 4);
		int32_t mx = *(const int16_t *)(model + 0x1C), mn = mx;
		const uint8_t *z = model + 0x1C;
		for (int32_t i = count - 1; i; i--, z += 8)
		{
			const int32_t v = *(const int16_t *)z;
			if (mn > v) mn = v;
			else if (mx < v) mx = v;
		}
		const int32_t range = mx - mn;
		uint8_t *p = model + 0xE;
		for (int32_t i = count; i; i--, p += 8)
			*(int16_t *)p = (int16_t)(shl32(*(const int16_t *)(p - 2) - mn, 12) / range);
	}

	// MAG_136_sub_6AEE20: a caster bone point (x, y, z, bone) through its pose (IR, 16 bits)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		const uint32_t bones = **(const uint32_t *const *)(c + 0x64) + 0x10;
		BuildBoneMatricesFromPose(c + 0x60);
		Mat4x3 m;
		const int32_t bone = v[3];
		ComposeAffineTransform((const Mat4x3 *)(Caster() + 0x40), (const Mat4x3 *)(bones + (uint32_t)bone * 0x30 + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// MAG_136_sub_6ADF60: RotX(b) x RotY(a) (3x3)
	static void RotXY(int32_t a, int32_t b, Mat4x3 *out)
	{
		Mat4x3 x, y;
		RotX(b, &x);
		RotY(a, &y);
		Matrix3x3MultiplyPSX(&x, &y, out);
	}

	// MAG_136_sub_6ADFA0: unit vector of (a, b): (-sin a sin b, sin a cos b, cos a) (z negated by the caller)
	static void AngleVector(int32_t a, int32_t b, int16_t *out)
	{
		out[2] = (int16_t)ComputeCos(a);
		const int32_t s = -ComputeSin(a);
		out[0] = (int16_t)(mul32(ComputeSin(b), s) >> 12);
		out[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(b), s)) >> 12);
	}

	// MAG_136_sub_6ADFF0: the mesh vertices through the GTE (rotation + translation) into the magic
	// buffer, each y lowered by WOBBLE[w / 2] x amp >> 17
	static void WobbleMesh(const uint8_t *model, int32_t amp, int16_t *out)
	{
		int32_t count = *(const int32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 8);
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(out);
			const int32_t w = v[3] >> 1;
			out[1] = (int16_t)(out[1] - (int16_t)(mul32(WOBBLE[w], amp) >> 0x11));
			out += 4;
			v += 4;
		} while (--count);
	}

	// ShotTask's P word +6 (entry esp - 0x3E, never written: 0x56CB90 stores 3 words), copied into the
	// node's previous-point word +0x2A on counters 0x1F..0x2A. ShotTask is the first module task of every
	// tick and nothing of the tick reaches that word before it: it holds what the battle loop left there
	// (0 under the game's task executor when the stack starts clear). Never read afterwards.
	static int16_t ShotResidue()
	{
		return 0;
	}

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
}
}

#ifdef FF8_FX_HELD
#include "mag136_oil_shot_held.h"
#endif

namespace ff8fx
{
namespace oil136
{
	// ------------------------------------------------------------------
	// Sprites
	// ------------------------------------------------------------------
	// 0x6AE300: sprite sequence 0x11F2390 (rotation, scale) through the shadow-rotation camera, 9 frames
	static void SprayDraw(const int16_t *pos, int16_t scale, int16_t frame, int16_t angle)
	{
		TransformCameraByShadowRotation(pos, scale, -(scale >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spray;
		*(int16_t *)(h + 4) = frame;
		*(int32_t *)(h + 8) = angle;
		*(int16_t *)(h + 0x24) = 1;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SprayTask(TaskNode *n)
	{
		SprayNode *s = (SprayNode *)n;
		// 30 fps layer: see mag136_oil_shot_held.inc
		FX_HELD(held_note_sprite(SPR_SPRAY, n);)
		SprayDraw(s->pos, s->scale, s->frame, s->angle);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->angle = (int16_t)(s->angle + s->spin);
		s->frame++;
		return s->frame < 9 ? 0 : TASK_END;
	}

	// 0x6AE190: sprite sequence 0x11F24D8 through the shadow-rotation camera, 16 frames
	static void SplatDraw(const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x1000, -0x400);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Splat;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SplatTask(TaskNode *n)
	{
		SplatNode *s = (SplatNode *)n;
		// 30 fps layer: see mag136_oil_shot_held.inc
		FX_HELD(held_note_sprite(SPR_SPLAT, n);)
		SplatDraw(s->pos, s->frame);
		s->frame++;
		return s->frame < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Drops
	// ------------------------------------------------------------------
	// 0x6AE210: layout +0x1C at the drop (depth 0, object 0 by the module renderer), then
	// position += velocity; ends with the layout
	static uint32_t __cdecl StreamTask(TaskNode *n)
	{
		StreamNode *d = (StreamNode *)n;
		PrimArg arg;
		BlockAt(&arg, d->pos);
		arg.depth = 0;
		arg.mode = 2;
		arg.morph = TexBase() + VERTS;
		// 30 fps layer: see mag136_oil_shot_held.inc
		FX_HELD(held_note_play(PLAY_STREAM, n, (prim::Layout *)d->layout, d->pos);)
		if (prim::play((prim::Layout *)d->layout, PrimCallback, (int)&arg, 0) == 0) return TASK_END;
		d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
		d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
		d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
		return 0;
	}

	// 0x6AE050: layout +0x1C falling (gravity 0x10, object 0 by the module renderer) until y >= 0,
	// then the splash layout at y = 0 (every object by the module renderer); ends with the splash
	static uint32_t __cdecl FallTask(TaskNode *n)
	{
		FallNode *d = (FallNode *)n;
		PrimArg arg;
		BlockAt(&arg, d->pos);
		arg.depth = 0;
		arg.mode = d->landed != 0 ? 1 : 2;
		arg.morph = TexBase() + VERTS;
		// 30 fps layer: see mag136_oil_shot_held.inc
		FX_HELD(held_note_play(PLAY_FALL, n, (prim::Layout *)d->layout, d->pos);)
		if (prim::play((prim::Layout *)d->layout, PrimCallback, (int)&arg, 0) == 0 && d->landed == 1) return TASK_END;
		if (d->landed == 0)
		{
			d->vel[1] = (int16_t)(d->vel[1] + 0x10);
			d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
			d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
			d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
			if (d->pos[1] >= 0)
			{
				d->pos[1] = 0;
				d->landed = 1;
				// 30 fps layer: see mag136_oil_shot_held.inc
				FX_HELD(held_note_landing(n);)
				DecodeModelPrimLayout(MODEL_Splash, d->layout, 0xA8);
			}
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Shot (0x6AD480)
	// ------------------------------------------------------------------
	// the flash level at counter c (0x1E..0x51)
	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0x22) return (int32_t)mul32(c - 0x1E, 1800) / 4;
		if (c < 0x46) return 0x708;
		return (int32_t)mul32(0x52 - c, 1800) / 12;
	}

	// the stream's progress at k = counter - 0x1E (1..12): s = k * 4096 / 12
	static int32_t StreamS(int32_t k) { return (int32_t)(((uint64_t)(uint32_t)shl32(k, 12) * 0xAAAAAAABu) >> 35); }

	// the stream point at progress s: the start blended to the target, y - sin(s / 2) / 8
	static void StreamPoint(const ShotNode *w, int32_t s, int32_t sine, int16_t *P)
	{
		BlendVectors(w->start, w->pos, 0x1000 - s, s, P);
		P[1] = (int16_t)(P[1] - (int16_t)(sine >> 3));
	}

	// the stream basis at P from prev
	static int32_t StreamBasis(const int16_t *P, const int16_t *prev, Mat4x3 *M)
	{
		const int32_t v[3] = { (int32_t)P[0] - prev[0], (int32_t)prev[1] - P[1], (int32_t)prev[2] - P[2] - 100 };
		return BuildOrthonormalBasis(v, M);
	}

	// the mesh z scale at progress s
	static int16_t MeshScaleZ(int32_t s, int32_t sine)
	{
		const int32_t q = ComputeSin(0x400 - s / 4);
		return (int16_t)(sine - q / 2 + 0x800);
	}

	// the mesh: the basis scaled in z at P, composed with the camera, drawn by 0x6AE9B0 (U scroll u)
	static void MeshRender(Mat4x3 *M, uint16_t u, uint32_t verts)
	{
		ComposeAffineTransform(&Camera(), M, M);
		GteSetTransVector(M);
		GteSetRotMatrix(M);
		GteSetTransVector(M);
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		*(uint8_t **)h = MESH;
		*(uint32_t *)(h + 4) = verts;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x1A) = 0x100;
		*(uint16_t *)(h + 0x18) = 0x100;
		*(uint16_t *)(h + 0x1E) = 0;
		*(uint16_t *)(h + 0x1C) = 0;
		*(uint16_t *)(h + 0x20) = 0x80;
		*(uint16_t *)(h + 0x22) = 0x100;
		*(uint16_t *)(h + 0x10) = u;
		PacketCursor() = MeshDraw(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	static uint32_t __cdecl ShotTask(TaskNode *n)
	{
		ShotNode *w = (ShotNode *)n;
		const int16_t c0 = w->counter;
		if (c0 >= 0x1E) SetScreenFlash((uint32_t)FlashLevel(c0), 0xF0F0F0);
		int16_t P[4];      // entry esp - 0x44: the mouth (counters 0x1E..0x25), then the stream point
		P[3] = ShotResidue();
		Mat4x3 M;          // entry esp - 0x2C
		if ((uint32_t)(w->counter - 0x1E) < 8)
		{
			BonePoint(MOUTH, P);
			RotXY(*(const int16_t *)(Caster() + 0xE), -0x200, &M);
			SprayNode *s = (SprayNode *)AddTaskToQueue(QSprays(), ORIG_SprayTask);
			if (s)
			{
				s->angle = (int16_t)CrtRand();
				s->spin = (int16_t)((CrtRand() & 0x1FF) - 0x100);
				memcpy(s->pos, P, 8);
				s->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
				const int32_t b = CrtRand();
				const int32_t a = (CrtRand() & 0x3FF) - 0x200;
				AngleVector(a, b, s->vel);
				s->vel[2] = (int16_t)-s->vel[2];
				MatrixMultiplyVector(&M, s->vel, s->vel);
				GteSetIR0((CrtRand() & 0x7F) + 0x80);
				GteLoadIR123(s->vel);
				GteGPF();
				GteStoreIR123(s->vel);
				s->frame = 0;
			}
			M.t[0] = P[0];
			M.t[1] = P[1];
			M.t[2] = P[2];
			ComposeAffineTransform(&Camera(), &M, &M);
			PrimArg arg;
			arg.m = M;
			arg.depth = -0x80;
			arg.mode = 0;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag136_oil_shot_held.inc
			FX_HELD(held_note_play(PLAY_MOUTH, n, (prim::Layout *)w->mouth, P);)
			Play(w->mouth, &arg);
		}
		const int32_t k = w->counter - 0x1E;
		if ((uint32_t)k <= 0xC)
		{
			const int32_t s = StreamS(k);
			const int32_t sine = ComputeSin(s >> 1);
			if (k == 0)
			{
				BonePoint(MOUTH, w->start);
				memcpy(w->prev, w->start, 8);
			}
			else
			{
				RotZ((k & 0xFF) << 8, &M);
				GteSetRotMatrix(&M);
				GteZeroTrans();
				WobbleMesh(MESH, sine, (int16_t *)(TexBase() + VERTS));
				StreamPoint(w, s, sine, P);
				StreamBasis(P, w->prev, &M);
				StreamNode *d = (StreamNode *)AddTaskToQueue(QStreams(), ORIG_StreamTask);
				memcpy(w->prev, P, 8);
				if (d)
				{
					const int32_t r = CrtRand();
					d->pos[0] = (int16_t)(ComputeCos(r) >> 6);
					d->pos[1] = (int16_t)(ComputeSin(r) >> 6);
					d->pos[2] = 0xFF;
					MatrixMultiplyVector(&M, d->pos, d->pos);
					d->zero = 0;
					d->pos[0] = (int16_t)(d->pos[0] + P[0]);
					d->pos[1] = (int16_t)(d->pos[1] + P[1]);
					d->pos[2] = (int16_t)(d->pos[2] + P[2]);
					d->vel[0] = (int16_t)(ComputeCos(r) >> 8);
					d->vel[1] = (int16_t)(ComputeSin(r) >> 8);
					d->vel[2] = 0x32;
					MatrixMultiplyVector(&M, d->vel, d->vel);
					DecodeModelPrimLayout(MODEL_Stream, d->layout, 0xA8);
				}
				ScaleZ() = MeshScaleZ(s, sine);
				MulMatrix3(&M, SCALE_Z);
				M.t[0] = P[0];
				M.t[1] = P[1];
				M.t[2] = P[2];
				// 30 fps layer: see mag136_oil_shot_held.inc
				FX_HELD(held_note_mesh(n);)
				MeshRender(&M, (uint16_t)((k & 7) << 4), TexBase() + VERTS);
			}
		}
		const int32_t h = w->counter - 0x26;
		if ((uint32_t)h < 0x1A)
		{
			PrimArg arg;
			BlockAt(&arg, w->pos);
			arg.depth = -0x80;
			arg.mode = 1;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag136_oil_shot_held.inc
			FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)w->hit, w->pos);)
			Play(w->hit, &arg);
			if ((uint32_t)h < 0x10 && !(h & 1))
			{
				FallNode *f = (FallNode *)AddTaskToQueue(QFalls(), ORIG_FallTask);
				if (f)
				{
					memcpy(f->pos, w->pos, 8);
					f->landed = 0;
					const int32_t a = CrtRand();
					const int32_t sp = (CrtRand() & 0x1F) + 0x20;
					f->vel[0] = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
					f->vel[2] = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
					f->vel[1] = (int16_t)(-0x40 - (CrtRand() & 0x7F));
					DecodeModelPrimLayout(MODEL_Fall, f->layout, 0x50);
				}
			}
			if ((uint32_t)h >= 4 && (uint32_t)h < 0x14)
			{
				SplatNode *s = (SplatNode *)AddTaskToQueue(QSplats(), ORIG_SplatTask);
				if (s)
				{
					memcpy(s->pos, w->pos, 8);
					s->pos[0] = (int16_t)(s->pos[0] + ((CrtRand() & 0x1FF) - 0x100));
					s->pos[1] = (int16_t)(s->pos[1] + ((CrtRand() & 0x3FF) - 0x200));
					const int32_t r = CrtRand();
					s->frame = 0;
					s->pos[2] = (int16_t)(s->pos[2] + ((r & 0x1FF) - 0x100));
				}
			}
		}
		if ((uint32_t)(w->counter - 0x2C) < 0x26)
		{
			// the target point - 0xFA in y (4th word never written: only loaded into the GTE V0 pad)
			const int16_t p[4] = { w->pos[0], (int16_t)(w->pos[1] - 0xFA), w->pos[2], 0 };
			PrimArg arg;
			BlockAt(&arg, p);
			arg.depth = 0;
			arg.mode = 1;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag136_oil_shot_held.inc
			FX_HELD(held_note_play(PLAY_POOL, n, (prim::Layout *)w->pool, w->pos);)
			Play(w->pool, &arg);
		}
		if (w->counter == 0x1E) BdPlaySE(SOUND_Shot, 0, 0x80);
		if (w->counter == 0x3E) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (w->counter >= 0x52)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		w->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6AD250)
	// ------------------------------------------------------------------
	static void StartShot()
	{
		uint8_t *target = Entity(Ctx()->actions->targets[0]);
		InitTaskQueuePool(QShot(), (void *)(TexBase() + 0x39A0), 0x46C, 1);
		InitTaskQueuePool(QSprays(), (void *)(TexBase() + 0x38A0), 0x20, 8);
		InitTaskQueuePool(QStreams(), (void *)(TexBase() + 0x2C60), 0xC4, 0x10);
		InitTaskQueuePool(QSplats(), (void *)(TexBase() + 0x2BC0), 0x14, 8);
		InitTaskQueuePool(QFalls(), (void *)TexBase(), 0x2BC, 0x10);
		ShotNode *w = (ShotNode *)AddTaskToQueue(QShot(), ORIG_ShotTask);
		Memset32(&w->counter, 0, 0x118);
		w->target = target;
		GetDefaultEffectPosition(target, w->pos);
		DecodeModelPrimLayout(MODEL_Mouth, w->mouth, 0xC8);
		DecodeModelPrimLayout(MODEL_Hit, w->hit, 0x1F4);
		DecodeModelPrimLayout(MODEL_Pool, w->pool, 0x184);
		NormalizeMesh(MESH);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag136_oil_shot_held.inc
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
			StartShot();
		}
		int r;
		if (m->started)
		{
			r = ExecuteTaskQueue(QShot());
			ExecuteTaskQueue(QSprays());
			ExecuteTaskQueue(QStreams());
			ExecuteTaskQueue(QSplats());
			ExecuteTaskQueue(QFalls());
		}
		else r = (int)n;
		if (m->started && r == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag136_oil_shot()
	{
		register_port(oil136::ORIG_MasterTask, (void *)oil136::MasterTask, "136 MasterTask", 136);
		register_port(oil136::ORIG_ShotTask, (void *)oil136::ShotTask, "136 ShotTask", 136);
		register_port(oil136::ORIG_SprayTask, (void *)oil136::SprayTask, "136 SprayTask", 136);
		register_port(oil136::ORIG_StreamTask, (void *)oil136::StreamTask, "136 StreamTask", 136);
		register_port(oil136::ORIG_SplatTask, (void *)oil136::SplatTask, "136 SplatTask", 136);
		register_port(oil136::ORIG_FallTask, (void *)oil136::FallTask, "136 FallTask", 136);
		// 30 fps layer: see mag136_oil_shot_held.inc
		FX_HELD(register_mag136_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag136_oil_shot_held.inc"
#endif
