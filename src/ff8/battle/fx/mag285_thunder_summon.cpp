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

#include "mag_common.h"

namespace ff8fx
{
namespace thunder285
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507D0C); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507D10); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507D18; }         // pool: magic buffer, 0x40 x 0x14
	inline TaskQueue *QMain() { return (TaskQueue *)0x2507D28; }           // pool: + 0x500, 1 x 0x15B8
	inline int16_t &MeanZ() { return var<int16_t>(0x2507D5C); }            // mean z of action 0's targets
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507D60); }
	inline uint32_t &FlashLevel() { return var<uint32_t>(0x2507D64); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507D68); }        // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &EffCam() { return var<Mat4x3>(0x2507D70); }             // the battle camera orthonormalised (0x67AAE0)
	inline uint32_t *KeptWords() { return (uint32_t *)0x2507D90; }         // 7 entity words +0 (0x67ABD0)
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
		TransformCameraByShadowRotation(s->pos, 0x800, -0x200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the layouts' blocks at main counter c
	static void BoltArg(PrimArg *arg)
	{
		arg->pos[0] = 0;
		arg->pos[1] = -0x2710;
		arg->pos[2] = 0;
		arg->depth = 0;
		arg->scroll = 0;
		arg->morph = TexBase() + MORPH;
	}

	static void GroundArg(int32_t c, PrimArg *arg)
	{
		arg->pos[0] = 0;
		arg->pos[1] = 0;
		arg->pos[2] = MeanZ();
		arg->depth = -0x100;
		arg->scroll = (int16_t)(((uint8_t)(0 - (uint8_t)(c - 0x56)) & 3) << 5);
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
		SparkDraw(s);
		s->counter++;
		return s->counter < 7 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x67AC00)
	// ------------------------------------------------------------------
	static void SpawnSparks(int32_t c)
	{
		for (uint32_t point = POINT_A; ; point -= 8)
		{
			SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
			if (s)
			{
				BonePoint((const int16_t *)point, s->pos);
				s->counter = 0;
				if ((uint32_t)c >= 0x10 && (uint32_t)c <= 0x12)
				{
					for (int k = 4; k; k--)
					{
						SparkNode *e = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
						if (!e) break;
						e->pos[0] = (int16_t)((CrtRand() & 0x1FF) + s->pos[0] - 0x100);
						e->pos[1] = (int16_t)((CrtRand() & 0x1FF) + s->pos[1] - 0x100);
						const int32_t z = (CrtRand() & 0x1FF) + s->pos[2];
						e->counter = 0;
						e->pos[2] = (int16_t)(z - 0x100);
					}
				}
			}
			if (point == POINT_B) break;
		}
	}

	// counters 0..31: the camera moved from its start towards its end by counter x 0x80
	static void CameraTrack(int32_t c)
	{
		const uint8_t *e = Caster();
		const int16_t x = *(const int16_t *)(e + 0x1C), y = *(const int16_t *)(e + 0x1E), z = *(const int16_t *)(e + 0x20);
		const int32_t t = (int32_t)((uint32_t)shl32(c, 12) >> 5);
		int16_t a[4] = { (int16_t)(x - 0x2C0), (int16_t)(y - 0x6EF), (int16_t)(z + 0x8ED), 0 };
		int16_t b[4] = { (int16_t)(x + 0x6E), (int16_t)(y - 0xB52), (int16_t)(z + 0x953), 0 };
		VecLerp(a, b, 0x1000 - t, t, LookAt());
		int16_t p[4] = { (int16_t)(x - 0x4F6), (int16_t)(y - 0x11), (int16_t)(z + 0x13A4), 0 };
		int16_t q[4] = { (int16_t)(x + 0x27C), (int16_t)(y + 0x952), (int16_t)(z + 0x1A5D), 0 };
		VecLerp(p, q, 0x1000 - t, t, Eye());
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c = m->counter;
		if (c < 8) FlashLevel() = (uint32_t)((int32_t)((uint32_t)mul32(c, 0x4B) << 5) >> 3);
		else if (c > 0x7F) FlashLevel() = (uint32_t)((int32_t)((uint32_t)mul32(0x87 - c, 0x4B) << 5) >> 3);
		else FlashLevel() = 0x960;
		if ((uint32_t)(c - 0x30) < 0x27)
		{
			PrimArg arg;
			BoltArg(&arg);
			// 30 fps layer: see mag285_thunder_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_BOLT, &arg);)
			prim::play((prim::Layout *)m->bolt, PrimCallback, (int)&arg, 0);
			if (c - 0x30 == 0x22)
			{
				// 30 fps layer: see mag285_thunder_summon_held.inc
				FX_HELD(held_note_tile();)
				PacketCursor() = Tile(0x80, 0x80, 0x80, 0x1000, PacketCursor());
			}
		}
		if ((uint32_t)(c - 0x56) < 0x30)
		{
			PrimArg arg;
			GroundArg(c, &arg);
			// 30 fps layer: see mag285_thunder_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_GROUND, &arg);)
			prim::play((prim::Layout *)m->ground, PrimCallback, (int)&arg, 0);
		}
		if (m->counter == 0) BdPlaySE(SOUND_Start, 0, 0x80);
		else if (m->counter == 0x30) BdPlaySE(SOUND_Bolt, 0, 0x80);
		const int32_t c1 = m->counter;
		if ((uint32_t)c1 < 0x20)
		{
			if ((uint32_t)c1 < 0x18) SpawnSparks(c1);
			if (c1 == 0)
			{
				CameraArmReturn();
				var<uint16_t>(0x1D8E038) = 0x120;
				var<uint16_t>(0x1D972C0) |= 4;
				var<uint16_t>(0x1D9735C) |= 4;
				var<uint16_t>(0x1D973F8) |= 4;
				var<uint16_t>(0x1D977A2) = 0;
			}
			CameraTrack(c1);
		}
		const int32_t c2 = m->counter - 0x10;
		if ((uint32_t)c2 < 0x20)
		{
			const int32_t k = shl32(c2 & 0xFFFFF, 7);
			CamOrbit(0, k >> 7, 0x1000);
			LookAt()[1] = (int16_t)(LookAt()[1] - (int16_t)(k >> 3));
			Eye()[1] = (int16_t)(Eye()[1] - (int16_t)(k >> 5));
		}
		if (m->counter == 0x30)
		{
			SlotFlags(0);
			LookAt()[0] = 0;
			LookAt()[1] = (int16_t)0xD8F0;
			LookAt()[2] = 0;
			Eye()[0] = 0x138;
			Eye()[1] = (int16_t)0xDAF7;
			Eye()[2] = (int16_t)0xFE6D;
		}
		const int32_t c3 = m->counter - 0x56;
		if ((uint32_t)c3 < 0x30)
		{
			const int32_t a = (int32_t)(uint32_t)(((uint64_t)(uint32_t)shl32(c3, 12) * 0xAAAAAAABull) >> 37);
			if (c3 == 0)
			{
				SlotFlags(1);
				const uint32_t mz = var<uint32_t>(0x2507D5C);
				LookAt()[2] = (int16_t)mz;
				Eye()[2] = (int16_t)(mz + 0x1518);
				// 30 fps layer: see mag285_thunder_summon_held.inc
				FX_HELD(held_note_tile();)
				PacketCursor() = Tile(0x80, 0x80, 0x80, 0x1000, PacketCursor());
				LookAt()[0] = 0;
				LookAt()[1] = (int16_t)0xFC93;
				Eye()[0] = (int16_t)0xFA24;
				Eye()[1] = (int16_t)0xFA24;
			}
			else if (c3 == 1) ScreenFadeTask(0, 1, 1, 0x80);
			else if (c3 == 8) ScreenFadeTask(0, 1, 0, 0x80);
			else if (c3 == 0xA) ScreenFadeTask(0, 1, 0, 0x60);
			CamOrbit(0, ComputeSin(a >> 1) >> 7, 0xFC8);
		}
		if (m->counter == 0x6E) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		m->counter++;
		if (m->counter >= 0x87)
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
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
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
			DecodeModelPrimLayout(MODEL_Bolt, m->bolt, 0x944);
			DecodeModelPrimLayout(MODEL_Ground, m->ground, 0xC64);
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
}

	void register_mag285_thunder_summon()
	{
		register_port(thunder285::ORIG_MasterTask, (void *)thunder285::MasterTask, "285 MasterTask", 285);
		register_port(thunder285::ORIG_MainTask, (void *)thunder285::MainTask, "285 MainTask", 285);
		register_port(thunder285::ORIG_SparkTask, (void *)thunder285::SparkTask, "285 SparkTask", 285);
		// 30 fps layer: see mag285_thunder_summon_held.inc
		FX_HELD(register_mag285_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag285_thunder_summon_held.inc"
#endif
