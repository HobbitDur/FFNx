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

// Effect 285: Thunder Summon (enemy attack 242 of kernel.bin; Catoblepas c0m133; MAG_285_*).
//
// Structure (setup 0x67A860 -> _Init 0x67A890, file loader 0x67A870 = mag284.tim (name at 0x108FAC0) into 0x2507D08; _Init
// queues the TIM, keeps the context and the CASTER's entity (0x2507D10), sets up the master queue 0x2507D38 (pool 0x2507D48,
// 1 x 0x14); no camera animation: the main task drives the camera words):
//   MasterTask (0x67A900) - alternates the packet arena (magic buffer + 0x1E80 / + 0x11E80), clears the screen-flash level
//     (0x2507D64); on counter 1 (once): keeps the entity words +0 of the 7 slots (0x67ABD0 -> 0x2507D90), sets up the pools
//     (main 1 x 0x15B8 at + 0x500, sparks 0x40 x 0x14 at + 0), starts the main task (layouts 0x1084BBC / 0x1089B00), the mean
//     z (entity +0x20) of action 0's targets into 0x2507D5C, and the stream state (+ 0x1E38, 0x534110). Then: the effect camera
//     (0x67AAE0 into 0x2507D70), the main and spark queues (the spark queue's result is ignored), the screen flash at the level;
//     once the main queue is empty: stream pump, flash 0, it ends when 0x5342C0 (0) returns non-zero (the stream is done).
//   MainTask (0x67AC00) - counters 0..23: 2 sparks at the caster's bone 10 points 0x108FAB8 / 0x108FAB0 (0x67B560, its world
//     matrix x the bone matrix), counters 16..18 each with 4 more around it (CRT rand: +-0x100 in x, y, z); counters 0..31 the
//     camera moved from (look-at caster + (-0x2C0, -0x6EF, 0x8ED), eye caster + (-0x4F6, -0x11, 0x13A4)) towards ((0x6E,
//     -0xB52, 0x953), (0x27C, 0x952, 0x1A5D)) by counter x 0x80 (0x56CB90); counter 0: 0x50A730, word 0x1D8E038 = 0x120, the
//     party's entity words +0 get bit 2, 0x1D977A2 = 0; counters 16..47 the eye orbits the look-at (0x67B5C0: RotY(c - 16),
//     scale 1) and both sink (look-at y -= (c - 16) x 16, eye y -= (c - 16) x 4); counter 48: the slot flags (0x67B6C0 (0)), a
//     fixed camera; counters 48..86 layout A at (0, -10000, 0) (camera composed, depth 0), counter 82 a white full-screen tile
//     (0x7040B0); counters 86..133 layout B at (0, 0, the mean z) (depth -0x100, texture scroll ((86 - c) & 3) x 0x20), counter
//     86: 0x67B6C0 (1), the camera at (0, -0x36D, mean z) / (-0x5DC, -0x5DC, mean z + 0x1518), a white tile; then the eye
//     orbits by sin((c - 86) x 0x1000 / 48 / 2) >> 7 at scale 0xFC8; fades 0x5712C0 at counters 87 / 94 / 96. Flash level:
//     counter x 300 until 8, (0x87 - counter) x 300 after 0x7F, else 0x960. Sounds 0x108FAA8 (counter 0) / 0x108FAAC (48);
//     action results of every target at counter 110; after counter 134: the camera words back from 0xB8B800, word 0x1D8E038
//     from 0x1D977A0, stream load of file 0x168 to magic buffer + 0x21E80.
//   SparkTask (0x67B720) - sequence 0x1084AF8 through the shadow-rotation camera (0x800, -0x200), frame = its counter; 7 frames.
//   PrimCallback (0x67B190) - one layout object drawn by 0x701DD0 (header 0x6C): the block's point through the camera, the
//     record's offset added (flag 0x200), through the effect camera (flag 0x1000, composed with it) or with the point (composed
//     with the camera); flag 0x100: the scale as a 3x3 product; texture scroll of object 0 from the block (+0x0A).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY WRITES: the entity words +0 of the slots (bit 2: party at counter 0, all slots at 48 / 86 from the kept words), the
//   bytes 0x1D98991..0x1D98A15 (stride 0x2C) bit 1; the targets' action results (engine). No entity position word is written.
// CAMERA: the eye 0xB8B7F0 / look-at 0xB8B7F8 words every tick of counters 0..47 and 86..133, set at 48 / 86, restored at the
//   end (the in-game held camera is the frame gate's lerp of the real words).
// Module globals: 0x2507D08..0x2507DAC (texture file 0x2507D08, context 0x2507D0C, caster 0x2507D10, the queues 0x2507D18
// sparks / 0x2507D28 main / 0x2507D38 master, master pool 0x2507D48, mean z 0x2507D5C, packet cursor 0x2507D60, flash level
// 0x2507D64, magic buffer base 0x2507D68, effect camera 0x2507D70, kept entity words 0x2507D90).
// STACK WORDS: the 4th word of the layout blocks (their point, loaded into GTE V0) and of the records' offsets reach the GTE's
// VZ0 high half (0x1CA8A16) only; 0x67B5C0's matrices / vector pads are never read.
// Fujin's described Thunder Summon (effect_285.json) runs this code: its memory through own::R() (fx_port.h), its spawns
// through own::Fn(), the setup and the file loader ported below; its parameters (ThunderSummonParams: the sounds, the
// screen flash, the bolt and ground layouts' ticks, places, depths and scroll, the white tiles, the sparks, the camera's
// moves (its track, orbit, cut and sway: code, no camera track), the screen fades, the damage and the end; its data: the
// two layouts, the spark flipbook, the spark points and the texture name). FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace thunder285
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2507D0C)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2507D10)); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)own::R(0x2507D18); }         // pool: magic buffer, 0x40 x 0x14
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2507D28); }           // pool: + 0x500, 1 x 0x15B8
	inline int16_t &MeanZ() { return var<int16_t>(own::R(0x2507D5C)); }            // mean z of action 0's targets
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2507D60)); }
	inline uint32_t &FlashLevel() { return var<uint32_t>(own::R(0x2507D64)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2507D68)); }        // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &EffCam() { return var<Mat4x3>(own::R(0x2507D70)); }             // the battle camera orthonormalised (0x67AAE0)
	inline uint32_t *KeptWords() { return (uint32_t *)own::R(0x2507D90); }         // 7 entity words +0 (0x67ABD0)
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2507D08)); }        // the effect's file (its TIM)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2507D38); }         // pool 0x2507D48, 1 x 0x14
	inline int16_t *Eye() { return (int16_t *)0xB8B7F0; }                  // battle camera words
	inline int16_t *LookAt() { return (int16_t *)0xB8B7F8; }
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x67A900;
	static const uint32_t ORIG_MainTask = 0x67AC00;
	static const uint32_t ORIG_SparkTask = 0x67B720;
	static const uint32_t ORIG_PrimCallback = 0x67B190;   // not a task: the layouts' callback
	static const uint32_t POOL_Main = 0x500;
	static const uint32_t STREAM = 0x1E38;               // magic buffer: stream state
	static const uint32_t MORPH = 0x1AB8;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1E80, ARENA_B = 0x11E80; // magic buffer: packet arenas
	static const uint32_t LOAD_Next = 0x21E80;           // magic buffer: the stream load at the end
	static const uint32_t MODEL_Bolt = 0x1084BBC;        // prim-model layouts
	static const uint32_t MODEL_Ground = 0x1089B00;
	static const uint32_t SEQ_Spark = 0x1084AF8;         // sprite sequence
	static const uint32_t POINT_A = 0x108FAB8, POINT_B = 0x108FAB0; // the spark points (offset, bone 10)
	static const void *const SOUND_Start = (const void *)0x108FAA8;
	static const void *const SOUND_Bolt = (const void *)0x108FAAC;
	static const uint32_t ORIG_FileLoader = 0x67A870, ORIG_Setup = 0x67A860;
	static const uint32_t TEXTURE_NAME = 0x108FAC0;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_BOLT = 1, PART_GROUND = 2, PART_SPARKS = 3, PART_TILES = 4 };
	struct Vec { int16_t x, y, z; };
	struct Fade { int16_t tick, a, b, c, d; };           // a screen fade (ScreenFadeTask 0x5712C0's four values) at a tick
	struct ThunderSummonParams
	{
		struct { uint32_t start, bolt; } sounds;
		struct { int16_t start_sound, bolt_sound, damage, end; } director;   // the main task's ticks
		struct { int16_t in, out, out_to, step, level; } flash;              // the screen flash's level
		struct { int16_t from, ticks, depth; Vec pos; } bolt;
		struct { int16_t from, ticks, depth, scroll; } ground;
		struct { int16_t first, r, g, b, level; } tile;
		struct { int16_t until, burst_from, burst_until, burst, life, shadow, shadow_y; fxp::Range spread; } sparks;
		struct { int16_t track_ticks; Vec look_from, look_to, eye_from, eye_to; } camera;   // its first move (from the caster)
		struct { int16_t from, ticks, spin, look_sink, eye_sink; } orbit;
		struct { int16_t tick; Vec look, eye; } cut;      // the fixed camera on the bolt
		struct { int16_t from, ticks, shift, scale, eye_back; Vec look, eye; } sway;   // the camera on the ground
		struct { Fade a, b, c; } fades;
		struct { uint32_t spark; } seqs;                  // the sprite sequences drawn (flipbooks)
		struct { uint32_t bolt, ground; } layouts;        // the prim-model layouts played
		struct { uint32_t points; } tables;               // its small tables (data): the spark points
		struct { uint32_t texture_name; } res;            // its resources (data): texture file name
	};
	static const ThunderSummonParams GAME = {
		{ 0x108FAA8, 0x108FAAC },
		{ 0, 0x30, 0x6E, 0x87 },
		{ 8, 0x7F, 0x87, 300, 0x960 },
		{ 0x30, 0x27, 0, { 0, -0x2710, 0 } },
		{ 0x56, 0x30, -0x100, 0x20 },
		{ 0x52, 0x80, 0x80, 0x80, 0x1000 },
		{ 0x17, 0x10, 0x12, 4, 7, 0x800, -0x200, { -0x100, 0xFF } },
		{ 0x20, { -0x2C0, -0x6EF, 0x8ED }, { 0x6E, -0xB52, 0x953 }, { -0x4F6, -0x11, 0x13A4 }, { 0x27C, 0x952, 0x1A5D } },
		{ 0x10, 0x20, 1, 0x10, 4 },
		{ 0x30, { 0, -0x2710, 0 }, { 0x138, -0x2509, -0x193 } },
		{ 0x56, 0x30, 7, 0xFC8, 0x1518, { 0, -0x36D, 0 }, { -0x5DC, -0x5DC, 0 } },
		{ { 0x57, 0, 1, 1, 0x80 }, { 0x5E, 0, 1, 0, 0x80 }, { 0x60, 0, 1, 0, 0x60 } },
		{ SEQ_Spark },
		{ MODEL_Bolt, MODEL_Ground },
		{ POINT_B },
		{ TEXTURE_NAME } };
	static ThunderSummonParams live = GAME;
	static const ThunderSummonParams &P() { return live; }
	// a random in a range (the game's `rand() & mask` + min for its power-of-two spans); an empty range: its min
	inline int32_t Roll(const fxp::Range &r, int32_t random)
	{
		const int32_t span = (int32_t)r.max - r.min + 1;
		return span > 0 ? r.min + random % span : r.min;
	}
	inline void FadeAt(const Fade &f) { ScreenFadeTask(f.a, f.b, f.c, f.d); }
	inline uint32_t Points() { return fxp::data(P().tables.points, POINT_B, 0x10); }   // POINT_B then POINT_A
	inline void SetVec(int16_t *v, const Vec &w) { v[0] = w.x; v[1] = w.y; v[2] = w.z; }

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void MatrixMultiply3x3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }      // a = a x b (3x3)
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }                // Effect_RotationYMatrix
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }                // MAG_063_RotationXMatrix
	inline void VecLerp(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }   // BS_Camera_ArmReturnAfterEffect
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamDone(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507D48)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x15B8 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t bolt[0x944];   // +0x10 layout A
		uint8_t ground[0xC64]; // +0x954 layout B
	};
	struct SparkNode // pool of 0x40 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the layout's point (4th word never written: the GTE V0 pad)
		int16_t depth;     // +0x08 the header's +0x18
		int16_t scroll;    // +0x0A texture scroll of object 0 (0: none)
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x15B8 && sizeof(SparkNode) == 0x14, "Thunder Summon nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Thunder Summon prim block");

	// 0x67AAE0 (MAG_069_OrthonormalizeCameraMatrix, in this module's range; other modules call it too): column 1 of in normalised
	// (v), w = (v.y, -v.x, 0) normalised, their cross product normalised; out = columns (w, v, w x v), translation copied (the pad
	// word +0x12 is not written)
	static void OrthonormalizeCamera(const Mat4x3 *in, Mat4x3 *out)
	{
		const int16_t *r = (const int16_t *)in;
		int16_t *o = (int16_t *)out;
		int32_t v[3] = { r[1], r[4], r[7] };
		NormalizeVector(v, v);
		o[1] = (int16_t)v[0];
		o[4] = (int16_t)v[1];
		int32_t w[3] = { v[1], (int32_t)(0u - (uint32_t)v[0]), 0 };
		o[7] = (int16_t)v[2];
		NormalizeVector(w, w);
		o[0] = (int16_t)w[0];
		o[3] = (int16_t)w[1];
		o[6] = (int16_t)w[2];
		int32_t c[3];
		CrossProduct(w, v, c);
		NormalizeVector(c, c);
		o[2] = (int16_t)c[0];
		o[5] = (int16_t)c[1];
		o[8] = (int16_t)c[2];
		out->t[0] = in->t[0];
		out->t[1] = in->t[1];
		out->t[2] = in->t[2];
	}

	// 0x67B560: the offset v (word +3 = bone) through the caster's world matrix x its bone matrix -> out (3 words, IR)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x67B5C0: the eye orbits the look-at: RotY(b) (x RotX(a) when a != 0), the eye -> look-at offset scaled by s (y twice)
	static void CamOrbit(int32_t a, int32_t b, int32_t s)
	{
		Mat4x3 m;
		RotY(b, &m);
		if (a)
		{
			Mat4x3 x;
			RotX(a, &x);
			MatrixMultiply3x3(&m, &x);
		}
		int16_t *eye = Eye(), *la = LookAt();
		eye[1] = (int16_t)(la[1] + (int16_t)(mul32((int32_t)eye[1] - la[1], s) >> 12));
		int16_t v[4];
		v[1] = (int16_t)(mul32((int32_t)eye[1] - la[1], s) >> 12);
		v[0] = (int16_t)(mul32((int32_t)eye[0] - la[0], s) >> 12);
		v[2] = (int16_t)(mul32((int32_t)eye[2] - la[2], s) >> 12);
		v[3] = 0;
		MatrixMulVector(&m, v, v);
		eye[0] = (int16_t)(la[0] + v[0]);
		eye[1] = (int16_t)(la[1] + v[1]);
		eye[2] = (int16_t)(la[2] + v[2]);
	}

	// 0x67B6C0: the bytes 0x1D98991..0x1D98A15 (stride 0x2C) bit 1 = on; the 7 entity words +0 bit 2: the kept bit (on) / set
	static void SlotFlags(int32_t on)
	{
		for (uint8_t *p = (uint8_t *)0x1D98A15; ; p -= 0x2C)
		{
			*p = (uint8_t)((*p & 0xFD) | ((on & 1) << 1));
			if (p == (uint8_t *)0x1D98991) break;
		}
		uint16_t *w = (uint16_t *)0x1D97668;
		for (int i = 6; i >= 0; i--, w = (uint16_t *)((uint8_t *)w - 0x9C))
		{
			if (on) *w = (uint16_t)(((KeptWords()[i] ^ *w) & 4) ^ *w);
			else *(uint8_t *)w |= 4;
		}
	}

	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, P().sparks.shadow, P().sparks.shadow_y);
		uint8_t *h = AllocHeader(0xB4);
		const uint32_t seq = fxp::sequence(P().seqs.spark, SEQ_Spark);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = fxp::last_frame(seq, s->counter);
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the layouts' blocks at main counter c
	static void BoltArg(PrimArg *arg)
	{
		arg->pos[0] = P().bolt.pos.x;
		arg->pos[1] = P().bolt.pos.y;
		arg->pos[2] = P().bolt.pos.z;
		arg->depth = P().bolt.depth;
		arg->scroll = 0;
		arg->morph = TexBase() + MORPH;
	}

	static void GroundArg(int32_t c, PrimArg *arg)
	{
		arg->pos[0] = 0;
		arg->pos[1] = 0;
		arg->pos[2] = MeanZ();
		arg->depth = P().ground.depth;
		arg->scroll = (int16_t)mul32((uint8_t)(0 - (uint8_t)(c - P().ground.from)) & 3, P().ground.scroll);
		arg->morph = TexBase() + MORPH;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag285_thunder_summon_held.h"
#endif

namespace ff8fx
{
namespace thunder285
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x67B190): one object of a layout. Vertex frame (or blend into the block's morph buffer), rotation
	// from the record's angles; flag 0x1000: the block's point through the camera + the record's offset through the effect camera,
	// composed with the effect camera (unless flag 0x8000); flag 0x200: the point through the camera + the offset; else point +
	// offset through the camera, composed with it (unless flag 0x8000); + the camera's translation, scale (flag 0x100: a 3x3
	// product, else scale3DMatrix), fade, the texture scroll of object 0, drawn by 0x701DD0 (header 0x6C)
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x6C);
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
		if (r->flags & 0x1000)
		{
			int32_t a[3], w[3];
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(a);
			GteSetRotMatrixCtrl(&EffCam());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(w);
			m.t[0] = (int32_t)((uint32_t)a[0] + (uint32_t)w[0]);
			m.t[1] = (int32_t)((uint32_t)a[1] + (uint32_t)w[1]);
			m.t[2] = (int32_t)((uint32_t)a[2] + (uint32_t)w[2]);
			if (!(r->flags & 0x8000)) MatrixMultiply(&EffCam(), &m);
		}
		else if (r->flags & 0x200)
		{
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[10] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2], 0 };
				MatrixMultiply3x3(&m, s);
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
		if (arg->scroll != 0 && (int16_t)r->flags_lo == 0)
		{
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x2E) = 0x80;
			*(uint16_t *)(h + 0x2C) = 0x80;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(int16_t *)(h + 0x22) = arg->scroll;
		}
		else *(int16_t *)(h + 0x22) = 0;
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x20) = 0;
		PacketCursor() = RenderPrimSet(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Spark (0x67B720)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag285_thunder_summon_held.inc
		FX_HELD(held_note_spark(s);)
		FX_PART(PART_SPARKS);
		SparkDraw(s);
		FX_PART(0);
		s->counter++;
		return s->counter < P().sparks.life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x67AC00)
	// ------------------------------------------------------------------
	static void SpawnSparks(int32_t c)
	{
		const auto &S = P().sparks;
		const uint32_t points = Points();
		for (uint32_t point = points + 8; ; point -= 8)
		{
			SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
			if (s)
			{
				BonePoint((const int16_t *)point, s->pos);
				s->counter = 0;
				if ((uint32_t)c >= (uint32_t)S.burst_from && (uint32_t)c <= (uint32_t)S.burst_until)
				{
					for (int k = S.burst; k > 0; k--)
					{
						SparkNode *e = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
						if (!e) break;
						e->pos[0] = (int16_t)(Roll(S.spread, CrtRand()) + s->pos[0]);
						e->pos[1] = (int16_t)(Roll(S.spread, CrtRand()) + s->pos[1]);
						const int32_t z = Roll(S.spread, CrtRand()) + s->pos[2];
						e->counter = 0;
						e->pos[2] = (int16_t)z;
					}
				}
			}
			if (point == points) break;
		}
	}

	// counters 0..31: the camera moved from its start towards its end by counter x 0x80
	static void CameraTrack(int32_t c)
	{
		const uint8_t *e = Caster();
		const int16_t x = *(const int16_t *)(e + 0x1C), y = *(const int16_t *)(e + 0x1E), z = *(const int16_t *)(e + 0x20);
		const auto &K = P().camera;
		const int32_t t = (int32_t)((uint32_t)shl32(c, 12) / (uint32_t)K.track_ticks);
		int16_t a[4] = { (int16_t)(x + K.look_from.x), (int16_t)(y + K.look_from.y), (int16_t)(z + K.look_from.z), 0 };
		int16_t b[4] = { (int16_t)(x + K.look_to.x), (int16_t)(y + K.look_to.y), (int16_t)(z + K.look_to.z), 0 };
		VecLerp(a, b, 0x1000 - t, t, LookAt());
		int16_t p[4] = { (int16_t)(x + K.eye_from.x), (int16_t)(y + K.eye_from.y), (int16_t)(z + K.eye_from.z), 0 };
		int16_t q[4] = { (int16_t)(x + K.eye_to.x), (int16_t)(y + K.eye_to.y), (int16_t)(z + K.eye_to.z), 0 };
		VecLerp(p, q, 0x1000 - t, t, Eye());
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c = m->counter;
		const auto &F = P().flash;
		if (c < F.in) FlashLevel() = (uint32_t)mul32(c, F.step);
		else if (c > F.out) FlashLevel() = (uint32_t)mul32(F.out_to - c, F.step);
		else FlashLevel() = (uint32_t)(int32_t)F.level;
		if ((uint32_t)(c - P().bolt.from) < (uint32_t)P().bolt.ticks)
		{
			PrimArg arg;
			BoltArg(&arg);
			// 30 fps layer: see mag285_thunder_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_BOLT, &arg);)
			FX_PART(PART_BOLT);
			prim::play((prim::Layout *)m->bolt, PrimCallback, (int)&arg, 0);
			FX_PART(0);
		}
		const auto &T = P().tile;
		if (c == T.first)
		{
			// 30 fps layer: see mag285_thunder_summon_held.inc
			FX_HELD(held_note_tile();)
			FX_PART(PART_TILES);
			PacketCursor() = Tile(T.r, T.g, T.b, T.level, PacketCursor());
			FX_PART(0);
		}
		if ((uint32_t)(c - P().ground.from) < (uint32_t)P().ground.ticks)
		{
			PrimArg arg;
			GroundArg(c, &arg);
			// 30 fps layer: see mag285_thunder_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_GROUND, &arg);)
			FX_PART(PART_GROUND);
			prim::play((prim::Layout *)m->ground, PrimCallback, (int)&arg, 0);
			FX_PART(0);
		}
		if (m->counter == P().director.start_sound) BdPlaySE((const void *)fxp::sound(P().sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		if (m->counter == P().director.bolt_sound) BdPlaySE((const void *)fxp::sound(P().sounds.bolt, (uint32_t)SOUND_Bolt), 0, 0x80);
		const int32_t c1 = m->counter;
		if ((uint32_t)c1 <= (uint32_t)P().sparks.until) SpawnSparks(c1);
		if (c1 == 0)
		{
			CameraArmReturn();
			var<uint16_t>(0x1D8E038) = 0x120;
			var<uint16_t>(0x1D972C0) |= 4;
			var<uint16_t>(0x1D9735C) |= 4;
			var<uint16_t>(0x1D973F8) |= 4;
			var<uint16_t>(0x1D977A2) = 0;
		}
		if (P().camera.track_ticks > 0 && (uint32_t)c1 < (uint32_t)P().camera.track_ticks) CameraTrack(c1);
		const auto &O = P().orbit;
		const int32_t c2 = m->counter - O.from;
		if ((uint32_t)c2 < (uint32_t)O.ticks)
		{
			CamOrbit(0, mul32(c2, O.spin), 0x1000);
			LookAt()[1] = (int16_t)(LookAt()[1] - (int16_t)mul32(c2, O.look_sink));
			Eye()[1] = (int16_t)(Eye()[1] - (int16_t)mul32(c2, O.eye_sink));
		}
		if (m->counter == P().cut.tick)
		{
			SlotFlags(0);
			SetVec(LookAt(), P().cut.look);
			SetVec(Eye(), P().cut.eye);
		}
		if (m->counter == P().fades.a.tick) FadeAt(P().fades.a);
		if (m->counter == P().fades.b.tick) FadeAt(P().fades.b);
		if (m->counter == P().fades.c.tick) FadeAt(P().fades.c);
		const auto &W = P().sway;
		const int32_t c3 = m->counter - W.from;
		if (W.ticks > 0 && (uint32_t)c3 < (uint32_t)W.ticks)
		{
			const int32_t a = (int32_t)((uint32_t)shl32(c3, 12) / (uint32_t)W.ticks);
			if (c3 == 0)
			{
				SlotFlags(1);
				const uint32_t mz = var<uint32_t>(own::R(0x2507D5C));
				LookAt()[2] = (int16_t)mz;
				Eye()[2] = (int16_t)(mz + W.eye_back);
				// 30 fps layer: see mag285_thunder_summon_held.inc
				FX_HELD(held_note_tile();)
				FX_PART(PART_TILES);
				PacketCursor() = Tile(T.r, T.g, T.b, T.level, PacketCursor());
				FX_PART(0);
				LookAt()[0] = W.look.x;
				LookAt()[1] = W.look.y;
				Eye()[0] = W.eye.x;
				Eye()[1] = W.eye.y;
			}
			CamOrbit(0, ComputeSin(a >> 1) >> (W.shift & 31), W.scale);
		}
		if (m->counter == P().director.damage) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		m->counter++;
		if (m->counter >= P().director.end)
		{
			var<uint32_t>(0xB8B7FC) = var<uint32_t>(0xB8B80C);
			var<uint16_t>(0x1D8E038) = var<uint16_t>(0x1D977A0);
			var<uint32_t>(0xB8B7F8) = var<uint32_t>(0xB8B808);
			var<uint32_t>(0xB8B7F0) = var<uint32_t>(0xB8B800);
			var<uint32_t>(0xB8B7F4) = var<uint32_t>(0xB8B804);
			StreamLoad(0x168, TexBase() + LOAD_Next, 0);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x67A870) and setup (0x67A860 -> _Init 0x67A890; no camera track: the main task drives the camera
	// words): the game runs its own; Fujin's programs these (its memory may be a copy's): the TIM, the magic buffer, the
	// cast and its caster's entity, the master (it sets up the rest)
	// ------------------------------------------------------------------
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
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2507D48), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 2);
		return QMaster();
	}

	// ------------------------------------------------------------------
	// Master (0x67A900)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		// 30 fps layer: see mag285_thunder_summon_held.inc
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
		FlashLevel() = 0;
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			for (int i = 0; i < 7; i++) KeptWords()[i] = *(const uint16_t *)Entity(i);
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x15B8, 1);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x14, 0x40);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x56B);
			const int32_t nt = Ctx()->actions[0].target_count;
			int16_t sum = 0;
			MeanZ() = 0;
			for (int i = 0; i < nt; i++)
			{
				sum = (int16_t)(sum + *(const int16_t *)(Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]) + 0x20));
				MeanZ() = sum;
			}
			MeanZ() = (int16_t)((int32_t)sum / nt);
			DecodeModelPrimLayout(fxp::layout(P().layouts.bolt, MODEL_Bolt, 0x944), m->bolt, 0x944);
			DecodeModelPrimLayout(fxp::layout(P().layouts.ground, MODEL_Ground, 0xC64), m->ground, 0xC64);
			StreamStateInit((void *)(TexBase() + STREAM));
		}
		int a0;
		if (mn->started)
		{
			OrthonormalizeCamera(&Camera(), &EffCam());
			a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QSparks());
		}
		else a0 = (int)n;
		SetScreenFlash(FlashLevel(), 0);
		if (mn->started && !a0)
		{
			StreamPump();
			SetScreenFlash(0, 0);
			return StreamDone(0) ? TASK_END : 0;
		}
		mn->counter++;
		return 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The main task's timeline: the sounds, the screen flash's level, the damage (every target of action 0), the end (the camera words put back, a stream load)" },
			{ PART_BOLT, "bolt", "Bolt", "The bolt layout high above the stage, seen by the cut camera" },
			{ PART_GROUND, "ground", "Ground", "The ground layout at the targets' mean depth, its texture scrolling" },
			{ PART_TILES, "tiles", "White tiles", "A full-screen tile during the bolt, another when the camera cuts to the ground" },
			{ PART_SPARKS, "sparks", "Sparks", "Sparks at two points of the caster's skeleton on the first ticks, with bursts round them for a few" },
			{ 0, "camera", "Camera", "The camera's moves (code, no camera track): from the caster's start to its end, an orbit sinking, the cut to the bolt, the cut to the ground and its sway; the screen fades" },
		};
		static const fxp::Group GROUPS[] = {
			{ "bolt.pos", "Place", "Where the bolt plays" },
			{ "sparks.spread", "Burst spread", "A burst spark's offset, at random" },
			{ "camera.look_from", "Look-at from", "The camera's look-at at the start (from the caster)" },
			{ "camera.look_to", "Look-at to", "At the end of its move" },
			{ "camera.eye_from", "Eye from", "The camera's eye at the start (from the caster)" },
			{ "camera.eye_to", "Eye to", "At the end of its move" },
			{ "cut.look", "Cut look-at", "The bolt's fixed view: its look-at" },
			{ "cut.eye", "Cut eye", "Its eye" },
			{ "fades.a", "Fade 1", "A screen fade" },
			{ "fades.b", "Fade 2", "A screen fade" },
			{ "fades.c", "Fade 3", "A screen fade" },
		};
		static const Field FIELDS[] = {
			{ "director", "sounds.start", "Start sound", "At the main task's start sound tick", (uint32_t)offsetof(ThunderSummonParams, sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.bolt", "Bolt sound", "At its bolt sound tick", (uint32_t)offsetof(ThunderSummonParams, sounds.bolt), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.start_sound", "Start sound tick", "The start sound at this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, director.start_sound), fxp::I16, -1, 300 },
			{ "director", "director.bolt_sound", "Bolt sound tick", "The bolt sound at this tick", (uint32_t)offsetof(ThunderSummonParams, director.bolt_sound), fxp::I16, -1, 300 },
			{ "director", "director.damage", "Damage tick", "The damage (every target of action 0) at this tick", (uint32_t)offsetof(ThunderSummonParams, director.damage), fxp::I16, -1, 300 },
			{ "director", "director.end", "End tick", "The main task's last tick + 1: the camera words are put back, the next stream load starts (the effect ends once it is done)", (uint32_t)offsetof(ThunderSummonParams, director.end), fxp::I16, 1, 300 },
			{ "director", "flash.in", "Flash in", "The screen flash brightens on the first ticks, up to this one (its level: the tick x Flash step)", (uint32_t)offsetof(ThunderSummonParams, flash.in), fxp::I16, 0, 300 },
			{ "director", "flash.out", "Flash out", "After this tick it fades: (Flash out to - the tick) x Flash step", (uint32_t)offsetof(ThunderSummonParams, flash.out), fxp::I16, 0, 300 },
			{ "director", "flash.out_to", "Flash out to", "(its level is 0 at this tick)", (uint32_t)offsetof(ThunderSummonParams, flash.out_to), fxp::I16, 0, 300 },
			{ "director", "flash.step", "Flash step", "(its level a tick)", (uint32_t)offsetof(ThunderSummonParams, flash.step), fxp::I16, -0x8000, 0x7FFF },
			{ "director", "flash.level", "Flash level", "In between, this level (4096 = white)", (uint32_t)offsetof(ThunderSummonParams, flash.level), fxp::I16, -0x8000, 0x7FFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(ThunderSummonParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "bolt", "layouts.bolt", "Bolt", "The prim-model layout bolt high above the stage, seen through the cut camera (its models' animation)", (uint32_t)offsetof(ThunderSummonParams, layouts.bolt), fxp::Layout, 0, 0x7FFFFFFF },
			{ "bolt", "bolt.from", "From", "From this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, bolt.from), fxp::I16, 0, 300 },
			{ "bolt", "bolt.ticks", "Ticks", "For this many ticks", (uint32_t)offsetof(ThunderSummonParams, bolt.ticks), fxp::I16, 0, 300 },
			{ "bolt", "bolt.depth", "Depth", "Its depth offset (the prim header's +0x18)", (uint32_t)offsetof(ThunderSummonParams, bolt.depth), fxp::I16, -0x8000, 0x7FFF },
			{ "bolt", "bolt.pos.x", "Place x", "Where it plays (the stage's coordinates)", (uint32_t)offsetof(ThunderSummonParams, bolt.pos.x), fxp::I16, -0x8000, 0x7FFF },
			{ "bolt", "bolt.pos.y", "Place y", "(y)", (uint32_t)offsetof(ThunderSummonParams, bolt.pos.y), fxp::I16, -0x8000, 0x7FFF },
			{ "bolt", "bolt.pos.z", "Place z", "(z)", (uint32_t)offsetof(ThunderSummonParams, bolt.pos.z), fxp::I16, -0x8000, 0x7FFF },
			{ "ground", "layouts.ground", "Ground", "The prim-model layout ground at the targets' mean depth, its object 0's texture scrolling (its models' animation)", (uint32_t)offsetof(ThunderSummonParams, layouts.ground), fxp::Layout, 0, 0x7FFFFFFF },
			{ "ground", "ground.from", "From", "From this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, ground.from), fxp::I16, 0, 300 },
			{ "ground", "ground.ticks", "Ticks", "For this many ticks", (uint32_t)offsetof(ThunderSummonParams, ground.ticks), fxp::I16, 0, 300 },
			{ "ground", "ground.depth", "Depth", "Its depth offset", (uint32_t)offsetof(ThunderSummonParams, ground.depth), fxp::I16, -0x8000, 0x7FFF },
			{ "ground", "ground.scroll", "Texture scroll", "Its object 0 scrolls its texture by this x (-tick & 3)", (uint32_t)offsetof(ThunderSummonParams, ground.scroll), fxp::I16, 0, 255 },
			{ "tiles", "tile.first", "First tick", "A full-screen tile at this tick of the main task (the second one comes with the sway camera)", (uint32_t)offsetof(ThunderSummonParams, tile.first), fxp::I16, -1, 300 },
			{ "tiles", "tile.r", "Red", "The tiles' colour", (uint32_t)offsetof(ThunderSummonParams, tile.r), fxp::I16, 0, 255 },
			{ "tiles", "tile.g", "Green", "(green)", (uint32_t)offsetof(ThunderSummonParams, tile.g), fxp::I16, 0, 255 },
			{ "tiles", "tile.b", "Blue", "(blue)", (uint32_t)offsetof(ThunderSummonParams, tile.b), fxp::I16, 0, 255 },
			{ "tiles", "tile.level", "Level", "Their level (4096 = full)", (uint32_t)offsetof(ThunderSummonParams, tile.level), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 7 frames: an edited one keeps as many; a spark holds its last past it)", (uint32_t)offsetof(ThunderSummonParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sparks", "tables.points", "Spark points", "The two points on the caster's bones the sparks start at: an offset (x, y, z) and a bone each, the second one first (16 bytes; an edited one keeps its length)", (uint32_t)offsetof(ThunderSummonParams, tables.points), fxp::Data, 0, 0x7FFFFFFF },
			{ "sparks", "sparks.until", "Until", "A spark at each point every tick of the main task up to this one", (uint32_t)offsetof(ThunderSummonParams, sparks.until), fxp::I16, -1, 300 },
			{ "sparks", "sparks.burst_from", "Burst from", "From this tick each spark comes with a burst of more round it", (uint32_t)offsetof(ThunderSummonParams, sparks.burst_from), fxp::I16, 0, 300 },
			{ "sparks", "sparks.burst_until", "Burst until", "Until this tick", (uint32_t)offsetof(ThunderSummonParams, sparks.burst_until), fxp::I16, -1, 300 },
			{ "sparks", "sparks.burst", "Burst", "How many in a burst (64 sparks at most at once)", (uint32_t)offsetof(ThunderSummonParams, sparks.burst), fxp::I16, 0, 64 },
			{ "sparks", "sparks.spread.min", "Burst spread", "A burst spark's offset on each axis, at random: from", (uint32_t)offsetof(ThunderSummonParams, sparks.spread.min), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "sparks.spread.max", "Burst spread to", "(to)", (uint32_t)offsetof(ThunderSummonParams, sparks.spread.max), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "sparks.life", "Life", "A spark's ticks (a frame a tick)", (uint32_t)offsetof(ThunderSummonParams, sparks.life), fxp::I16, 1, 100 },
			{ "sparks", "sparks.shadow", "Distance", "Its sprite's camera (the shadow-rotation camera): its distance", (uint32_t)offsetof(ThunderSummonParams, sparks.shadow), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "sparks.shadow_y", "Height", "Its height", (uint32_t)offsetof(ThunderSummonParams, sparks.shadow_y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.track_ticks", "Move ticks", "On its first ticks the camera moves from its start to its end (from the caster's position; the battle camera returns afterwards)", (uint32_t)offsetof(ThunderSummonParams, camera.track_ticks), fxp::I16, 0, 300 },
			{ "camera", "camera.look_from.x", "Look-at from x", "Its look-at at the start, from the caster", (uint32_t)offsetof(ThunderSummonParams, camera.look_from.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.look_from.y", "Look-at from y", "(y)", (uint32_t)offsetof(ThunderSummonParams, camera.look_from.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.look_from.z", "Look-at from z", "(z)", (uint32_t)offsetof(ThunderSummonParams, camera.look_from.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.look_to.x", "Look-at to x", "At the end", (uint32_t)offsetof(ThunderSummonParams, camera.look_to.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.look_to.y", "Look-at to y", "(y)", (uint32_t)offsetof(ThunderSummonParams, camera.look_to.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.look_to.z", "Look-at to z", "(z)", (uint32_t)offsetof(ThunderSummonParams, camera.look_to.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_from.x", "Eye from x", "Its eye at the start, from the caster", (uint32_t)offsetof(ThunderSummonParams, camera.eye_from.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_from.y", "Eye from y", "(y)", (uint32_t)offsetof(ThunderSummonParams, camera.eye_from.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_from.z", "Eye from z", "(z)", (uint32_t)offsetof(ThunderSummonParams, camera.eye_from.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_to.x", "Eye to x", "At the end", (uint32_t)offsetof(ThunderSummonParams, camera.eye_to.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_to.y", "Eye to y", "(y)", (uint32_t)offsetof(ThunderSummonParams, camera.eye_to.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "camera.eye_to.z", "Eye to z", "(z)", (uint32_t)offsetof(ThunderSummonParams, camera.eye_to.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "orbit.from", "Orbit from", "From this tick the eye orbits the look-at and both sink", (uint32_t)offsetof(ThunderSummonParams, orbit.from), fxp::I16, 0, 300 },
			{ "camera", "orbit.ticks", "Orbit ticks", "For this many ticks", (uint32_t)offsetof(ThunderSummonParams, orbit.ticks), fxp::I16, 0, 300 },
			{ "camera", "orbit.spin", "Orbit spin", "Its turn: this x its tick (4096 = a full turn)", (uint32_t)offsetof(ThunderSummonParams, orbit.spin), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "camera", "orbit.look_sink", "Look-at sink", "The look-at's y taken: this x its tick", (uint32_t)offsetof(ThunderSummonParams, orbit.look_sink), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "orbit.eye_sink", "Eye sink", "The eye's y taken: this x its tick", (uint32_t)offsetof(ThunderSummonParams, orbit.eye_sink), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.tick", "Cut tick", "At this tick the camera cuts to the bolt's fixed view (the fighters hidden)", (uint32_t)offsetof(ThunderSummonParams, cut.tick), fxp::I16, -1, 300 },
			{ "camera", "cut.look.x", "Cut look-at x", "Its look-at (the stage's coordinates)", (uint32_t)offsetof(ThunderSummonParams, cut.look.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.look.y", "Cut look-at y", "(y)", (uint32_t)offsetof(ThunderSummonParams, cut.look.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.look.z", "Cut look-at z", "(z)", (uint32_t)offsetof(ThunderSummonParams, cut.look.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.eye.x", "Cut eye x", "Its eye", (uint32_t)offsetof(ThunderSummonParams, cut.eye.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.eye.y", "Cut eye y", "(y)", (uint32_t)offsetof(ThunderSummonParams, cut.eye.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "cut.eye.z", "Cut eye z", "(z)", (uint32_t)offsetof(ThunderSummonParams, cut.eye.z), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "sway.from", "Sway from", "From this tick the camera cuts to the ground (the fighters shown, a white tile) and sways", (uint32_t)offsetof(ThunderSummonParams, sway.from), fxp::I16, 0, 300 },
			{ "camera", "sway.ticks", "Sway ticks", "For this many ticks (half a sine over them)", (uint32_t)offsetof(ThunderSummonParams, sway.ticks), fxp::I16, 0, 300 },
			{ "camera", "sway.shift", "Sway shift", "The sway's turn: the sine >> this", (uint32_t)offsetof(ThunderSummonParams, sway.shift), fxp::I16, 0, 31 },
			{ "camera", "sway.scale", "Sway scale", "The eye's distance x this a tick (4096 = kept)", (uint32_t)offsetof(ThunderSummonParams, sway.scale), fxp::I16, -0x8000, 0x7FFF, fxp::Scale },
			{ "camera", "sway.eye_back", "Eye back", "Its eye's z: the targets' mean depth + this", (uint32_t)offsetof(ThunderSummonParams, sway.eye_back), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "sway.look.x", "Sway look-at x", "Its look-at's x (its z: the targets' mean depth)", (uint32_t)offsetof(ThunderSummonParams, sway.look.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "sway.look.y", "Sway look-at y", "(y)", (uint32_t)offsetof(ThunderSummonParams, sway.look.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "sway.eye.x", "Sway eye x", "Its eye's x", (uint32_t)offsetof(ThunderSummonParams, sway.eye.x), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "sway.eye.y", "Sway eye y", "(y)", (uint32_t)offsetof(ThunderSummonParams, sway.eye.y), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.a.tick", "Fade 1 tick", "A screen fade at this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, fades.a.tick), fxp::I16, -1, 300 },
			{ "camera", "fades.a.a", "Fade 1: first", "ScreenFadeTask's first value", (uint32_t)offsetof(ThunderSummonParams, fades.a.a), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.a.b", "Fade 1: second", "Its second value", (uint32_t)offsetof(ThunderSummonParams, fades.a.b), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.a.c", "Fade 1: third", "Its third value (the game's mode)", (uint32_t)offsetof(ThunderSummonParams, fades.a.c), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.a.d", "Fade 1: level", "Its fourth value (the game's level, 128 = full)", (uint32_t)offsetof(ThunderSummonParams, fades.a.d), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.b.tick", "Fade 2 tick", "A screen fade at this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, fades.b.tick), fxp::I16, -1, 300 },
			{ "camera", "fades.b.a", "Fade 2: first", "ScreenFadeTask's first value", (uint32_t)offsetof(ThunderSummonParams, fades.b.a), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.b.b", "Fade 2: second", "Its second value", (uint32_t)offsetof(ThunderSummonParams, fades.b.b), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.b.c", "Fade 2: third", "Its third value (the game's mode)", (uint32_t)offsetof(ThunderSummonParams, fades.b.c), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.b.d", "Fade 2: level", "Its fourth value (the game's level, 128 = full)", (uint32_t)offsetof(ThunderSummonParams, fades.b.d), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.c.tick", "Fade 3 tick", "A screen fade at this tick of the main task", (uint32_t)offsetof(ThunderSummonParams, fades.c.tick), fxp::I16, -1, 300 },
			{ "camera", "fades.c.a", "Fade 3: first", "ScreenFadeTask's first value", (uint32_t)offsetof(ThunderSummonParams, fades.c.a), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.c.b", "Fade 3: second", "Its second value", (uint32_t)offsetof(ThunderSummonParams, fades.c.b), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.c.c", "Fade 3: third", "Its third value (the game's mode)", (uint32_t)offsetof(ThunderSummonParams, fades.c.c), fxp::I16, -0x8000, 0x7FFF },
			{ "camera", "fades.c.d", "Fade 3: level", "Its fourth value (the game's level, 128 = full)", (uint32_t)offsetof(ThunderSummonParams, fades.c.d), fxp::I16, -0x8000, 0x7FFF },
		};
	}
}

	void register_mag285_thunder_summon()
	{
		fxp::register_params({ 285, "Thunder Summon", &thunder285::live, &thunder285::GAME, sizeof(thunder285::ThunderSummonParams),
		                       thunder285::table::PARTS, (int)(sizeof(thunder285::table::PARTS) / sizeof(thunder285::table::PARTS[0])),
		                       thunder285::table::FIELDS, (int)(sizeof(thunder285::table::FIELDS) / sizeof(thunder285::table::FIELDS[0])),
		                       thunder285::table::GROUPS, (int)(sizeof(thunder285::table::GROUPS) / sizeof(thunder285::table::GROUPS[0])) });
		own::note(thunder285::ORIG_FileLoader, (void *)thunder285::FileLoader);
		own::note(thunder285::ORIG_Setup, (void *)thunder285::Setup);
		own::note(thunder285::ORIG_MasterTask, (void *)thunder285::MasterTask);
		register_port(thunder285::ORIG_MasterTask, (void *)thunder285::MasterTask, "285 MasterTask", 285);
		own::note(thunder285::ORIG_MainTask, (void *)thunder285::MainTask);
		register_port(thunder285::ORIG_MainTask, (void *)thunder285::MainTask, "285 MainTask", 285);
		own::note(thunder285::ORIG_SparkTask, (void *)thunder285::SparkTask);
		register_port(thunder285::ORIG_SparkTask, (void *)thunder285::SparkTask, "285 SparkTask", 285);
		// 30 fps layer: see mag285_thunder_summon_held.inc
		FX_HELD(register_mag285_held();)
	}
	FX_LAB_MODULE(register_mag285_thunder_summon);
}

#ifdef FF8_FX_HELD
#include "mag285_thunder_summon_held.inc"
#endif
