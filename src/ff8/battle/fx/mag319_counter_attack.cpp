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

// Effect 319: Counter-attack (Krysta c0m038; MAG_319_*). The weak twin of Strong counter-attack (effect 318, the module
// right after it): the same spike layouts drawn once, then the caster's arm bones grown back from nothing by 318's bone
// scale task (0x65F2B0) queued in this module's own queue.
//
// Structure (setup MAG_319 0x65DB30 -> _Init 0x65DB60 (not ported), file loader 0x65DB40 = the texture file named at
// 0xFF7798 ("mag318.tim") -> [0x25053A0]; the setup queues the TIM, keeps the CASTER's entity (0x25053AC), starts camera
// animation 0xFF75D0 and the root in its 1-node pool 0x25053F0):
//   RootTask (0x65DBE0) - alternates the packet arena (magic buffer + 0xD1C / + 0x10D1C); on counter 1 sets up the pools
//     once (main 1 x 0x898 at + 0, bone scales 5 x 0x1C at + 0x898, sprites 0x10 x 0x18 at + 0x924), starts the main task
//     at the first target of action 0 (its default effect position, the two layouts 0xFF24F8 / 0xFF4350 decoded) and the
//     stream state at + 0xAA4. Then the main, bone-scale and sprite queues, the stream pump (counter >= 1); once set up
//     its counter is pinned (0x40 + 1); it ends when the three queues are empty and the stream is done (0x5342C0).
//   MainTask (0x65DDA0) - counters 0..0x26:
//     counter 2: S = the caster's bone 0xD (0x502170, flag 0x800), T = the target's position; the basis M of
//       (T - S) (0x50CBA0, y the other way), three offsets 0xFF7780 / 0xFF7788 / 0xFF7790 through M (0x56C4F0: only the
//       first is used), the basis M2 of the mirrored direction;
//     counters 2..8: the caster's object 1 shown (entity +0x7C bit 1 cleared at 2); the point L = T (c - 2) / 6 + S
//       (6 - c + 2) / 6 (0x56CB90); layout A (+0xC4) in M at offset 0 + L, composed with the camera (depth -0x80);
//       then the prim model 0xFF13F0 in M at L;
//     counters 7..0x26: layout B (+0x42C) in M2 at T - offset 0, composed with the camera (depth -0x80); on counters
//       15 / 17 / 19 / 21: 4 sprites each around T (CRT rand +-0x200 per axis);
//     sound 0xFF75CC at counter 1, the damage of action 0's first target record at 0xC; at 0x1B the caster's object 1
//     hidden (+0x7C bit 1) and five bone scales queued (0x65E4B0): bones 0xA / 0xD / 0x17 / 0x18 / 0x19 after 0 / 8 /
//     16 / 16 / 16 ticks; at 0x27 the stream load of file 0x168 into magic buffer + 0x20D1C, and the task ends.
//   BoneScaleTask (0x65F2B0, Strong counter-attack 318's task: its port is mag318_strong_counter_attack.cpp's): a bone
//     matrix of the caster's skeleton (skeleton record + 0x10, scale3DMatrix) scaled (x, x, y) from 0 after its delay,
//     x += 0x200 / y += 0x400 per tick up to 0x1000; ends (unscaled) once both reach 0x1000.
//   SpriteTask (0x65E430) - sprite sequence 0xFF12E0 through the shadow-rotation camera (0x1000, -0x400), flipbook frame
//     = its counter; 10 ticks.
//   PrimCallback (0x65E190) = Funguar laser 277's 0x684870 without the 0x40000 rotation variant, on this module's cursor.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: the CASTER's entity is written: +0x7C bit 1 (object 1 visibility) at main counters 2 / 0x1B, and its
// skeleton's bone matrices (the engine rebuilds them from the pose before each render; the bone scales scale them in place
// in between); the camera animation is the engine's.
// Module globals: 0x25053A0..0x2505418 (texture file 0x25053A0, packet cursor 0x25053A4, context 0x25053A8, caster entity
// 0x25053AC, the queues 0x25053B0 sprites / 0x25053C0 bone scales / 0x25053D0 main / 0x25053E0 root, root pool 0x25053F0,
// magic buffer base 0x2505410).
// NEVER-WRITTEN STACK WORDS: as 277's (the 4th word of the callback's record offset reaches the GTE VZ0 high half only,
// the matrices' pad words are never read, the model's prim header keeps the scratch's words but +0x00 / +0x1C).
// FUJIN: Fujin's described Counter-attack (effect_319.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn() (the bone scales: 318's task, its port in a program), the file loader (0x65DB40) and the setup
// (0x65DB30 -> _Init 0x65DB60) ported below; its parameters (CounterParams: the sound, the main's ticks, the spike and
// burst layouts' ticks and depths, the spike's bone, the sprites, the five bone scales; the two layouts, the spike model,
// the sprites' flipbook, the offsets, the texture name and the camera track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace counter319
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x25053A4)); }
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x25053A8)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x25053AC)); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2505410)); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSprites() { return (TaskQueue *)own::R(0x25053B0); }        // pool: + 0x924, 0x10 x 0x18
	inline TaskQueue *QBones() { return (TaskQueue *)own::R(0x25053C0); }          // pool: + 0x898, 5 x 0x1C
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x25053D0); }           // pool: magic buffer, 1 x 0x898

	static const uint32_t ORIG_RootTask = 0x65DBE0;
	static const uint32_t ORIG_MainTask = 0x65DDA0;
	static const uint32_t ORIG_SpriteTask = 0x65E430;
	static const uint32_t ORIG_BoneScaleTask = 0x65F2B0;  // Strong counter-attack 318's
	static const uint32_t POOL_Bones = 0x898, POOL_Sprites = 0x924;
	static const uint32_t STREAM_STATE = 0xAA4;          // magic buffer: stream state
	static const uint32_t MORPH = 0xAEC;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xD1C;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10D1C;
	static const uint32_t STREAM_DEST = 0x20D1C;         // magic buffer: file 0x168 (loaded at the end)
	static const uint32_t MODEL_A = 0xFF24F8;            // prim-model layouts
	static const uint32_t MODEL_B = 0xFF4350;
	static const uint32_t PRIM_Spike = 0xFF13F0;         // prim model (0x572200)
	static const uint32_t SEQ_Sprite = 0xFF12E0;         // sprite sequence
	static const void *const SOUND = (const void *)0xFF75CC;
	static const int16_t *const OFFSETS = (const int16_t *)0xFF7780; // three vectors of 4 words (through M)

	static const uint32_t ORIG_FileLoader = 0x65DB40, ORIG_Setup = 0x65DB30;
	static const uint32_t TEXTURE_NAME = 0xFF7798, CAMERA_ANIM = 0xFF75D0;
	static const uint32_t OFFSETS_TABLE = 0xFF7780, OFFSETS_SIZE = 0x18;   // the three offsets (4 words each)
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x25053A0)); }     // the effect's file (its TIM)
	inline uint32_t MagicTextureOff() { return fn<uint32_t (__cdecl *)()>(0x571B50)(); }   // Magic_TextureOFF_ToEAX1

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SPIKE = 1, PART_BURST = 2, PART_SPRITES = 3 };
	struct CounterParams
	{
		struct { uint32_t start; } sounds;            // the sound
		struct { int16_t sound, damage, bones, end; } timing; // the main's ticks: the sound, the damage, the bone scales, its end
		// the spike (layout A and the spike model): from `start` for ticks + 1 ticks, travelling from the caster's bone to
		// the target (the weight of the target: tick / ticks), its layout's depth
		struct { int16_t start, ticks, bone, depth; } spike;
		struct { int16_t start, ticks, depth; } burst;  // layout B at the target: its ticks, its depth
		// the sprites around the target: on the ticks first..last every `every` (inside the burst's), count, a random
		// below 2 x spread - spread on each axis, their ticks (frames), size
		struct { int16_t first, last, every, count, spread, life, size; } sprites;
		// the caster's five bones grown back from nothing (after their delays, x / y by grow_x, z by grow_y a tick)
		struct { int16_t bone[5], delay[5], grow_x, grow_y; } bones;
		struct { uint32_t spike, burst; } layouts;      // layouts A and B
		struct { uint32_t spike; } models;              // the spike model
		struct { uint32_t sprite; } seqs;               // the sprites' flipbook
		struct { uint32_t offsets; } tables;            // three offsets through the spike's frame (the first one used)
		struct { uint32_t texture_name, camera; } res;  // its resources (data): texture file name, camera track
	};
	static const CounterParams GAME = { { 0xFF75CC }, { 1, 0xC, 0x1B, 0x27 }, { 2, 6, 0xD, -0x80 }, { 7, 0x20, -0x80 },
		{ 15, 21, 2, 4, 0x200, 0xA, 0x1000 }, { { 0xA, 0xD, 0x17, 0x18, 0x19 }, { 0, 8, 0x10, 0x10, 0x10 }, 0x200, 0x400 },
		{ MODEL_A, MODEL_B }, { PRIM_Spike }, { SEQ_Sprite }, { OFFSETS_TABLE }, { TEXTURE_NAME, CAMERA_ANIM } };
	static CounterParams live = GAME;
	static const CounterParams &PG() { return live; }
	// a bone of the caster's skeleton: the parameter, below its bone count (else its last bone)
	static int32_t CasterBone(int32_t bone)
	{
		const uint8_t count = (**(uint8_t ***)(Caster() + 0x64))[0];
		return bone < 0 ? 0 : bone < count ? bone : count - 1;
	}

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector (rotation only)
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12 (GTE GPF / GPL)
	// battle file streaming (0x5341D0 load a file, 0x534210 pump, 0x5342C0 done)
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamDone(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x25053F0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C pinned at 0x41 once set up
		uint8_t started;   // +0x0E pools set up
		uint8_t pad0F;
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x898 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[0x4A];
		uint8_t *ent;      // +0x58 target entity (first target of action 0)
		int16_t spawn[4];  // +0x5C S: the caster's bone 0xD (counter 2)
		int16_t pos[4];    // +0x64 T: the target's default effect position
		int16_t off[3][4]; // +0x6C the offsets through M (only [0] used)
		Mat4x3 M;          // +0x84 basis of T - S; translation rewritten per draw
		Mat4x3 M2;         // +0xA4 basis of the mirrored direction; translation T - off[0]
		uint8_t layout_a[0x368]; // +0xC4
		uint8_t layout_b[0x46C]; // +0x42C
	};
	struct BoneNode // pool of 5 nodes of 0x1C bytes
	{
		TaskNode hdr;
		uint8_t *bone;     // +0x0C skeleton record (the matrix at + 0x10)
		int16_t delay;     // +0x10 ticks before the growth
		int16_t sy;        // +0x12 scale z (y step +0x14)
		int16_t dy;        // +0x14
		int16_t sx;        // +0x16 scale x / y (x step +0x18)
		int16_t dx;        // +0x18
		int16_t pad1A;
	};
	struct SpriteNode // pool of 0x10 nodes of 0x18 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t pad16;
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x898 && sizeof(BoneNode) == 0x1C, "Counter-attack nodes");
	static_assert(sizeof(SpriteNode) == 0x18 && sizeof(PrimArg) == 0x28, "Counter-attack nodes");
	static_assert(offsetof(MainNode, ent) == 0x58 && offsetof(MainNode, M) == 0x84 && offsetof(MainNode, layout_a) == 0xC4, "Counter-attack main node");

	// ------------------------------------------------------------------
	// Prim-player callback (0x65E190): one object of a layout. Vertex frame (or blend into the
	// parameter block's morph buffer), rotation from the record's angles, the record's offset
	// through the parameter matrix (flag 0x200: as is), the matrix composed with it (unless flag
	// 0x200 or 0x8000), scale (flag 0x100: 3x3 product, else scale3DMatrix), fade, drawn by 0x7043B0.
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

	// counters 2..8 (0x65DE69..0x65DE9B): the point L between T and S, weight (c - 2) / 6 of T (the spike's ticks)
	static void SpikePoint(const MainNode *m, int32_t c2, int16_t *L)
	{
		const uint32_t t = ((uint32_t)c2 << 12) / (uint32_t)PG().spike.ticks;
		VecLerp(m->pos, m->spawn, (int32_t)t, (int32_t)(0x1000 - t), L);
	}

	// the spike model (0x65DF19..0x65DF95): M at L composed with the camera, model 0xFF13F0 (no fade)
	static void SpikeDraw(Mat4x3 *M, const int16_t *L, PrimArg *arg)
	{
		M->t[0] = L[0];
		M->t[1] = L[1];
		M->t[2] = L[2];
		ComposeAffineTransform(&Camera(), M, &arg->m);
		GteSetRotMatrix(&arg->m);
		GteSetTransVector(&arg->m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = fxp::model(PG().models.spike, PRIM_Spike);
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the sprites (0x65E430..0x65E490)
	static void SpriteDraw(const int16_t *pos, int16_t frame)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, PG().sprites.size, -0x400);
		*(uint32_t *)h = fxp::sequence(PG().seqs.sprite, SEQ_Sprite);
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag319_counter_attack_held.h"
#endif

namespace ff8fx
{
namespace counter319
{
	// ------------------------------------------------------------------
	// Sprite (0x65E430)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag319_counter_attack_held.inc
		FX_HELD(held_note_sprite(n, s->pos, s->counter);)
		FX_PART(PART_SPRITES);
		SpriteDraw(s->pos, s->counter);
		s->counter++;
		return s->counter < PG().sprites.life ? 0 : TASK_END;
	}

	// 0x65E4B0: a bone scale task on the caster's skeleton bone `bone` (no pool check: 5 nodes for the 5 calls)
	static void StartBoneScale(int32_t bone, int16_t delay, int16_t dx, int16_t dy)
	{
		BoneNode *b = (BoneNode *)AddTaskToQueue(QBones(), own::Fn(ORIG_BoneScaleTask));
		b->bone = **(uint8_t ***)(Caster() + 0x64) + CasterBone(bone) * 0x30 + 0x10;
		b->delay = delay;
		b->dx = dx;
		b->dy = dy;
		b->sy = 0;
		b->sx = 0;
	}

	// ------------------------------------------------------------------
	// Main (0x65DDA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const CounterParams &g = PG();
		PrimArg arg;
		if (m->counter == 2)
		{
			GetEffectSpawnPosition(Caster(), CasterBone(g.spike.bone), 0x800, m->spawn);
			const int16_t *const OFFSETS = (const int16_t *)fxp::data(g.tables.offsets, OFFSETS_TABLE, OFFSETS_SIZE);
			int32_t v[3] = { (int32_t)m->pos[0] - m->spawn[0], (int32_t)m->spawn[1] - m->pos[1], (int32_t)m->spawn[2] - m->pos[2] };
			BuildOrthonormalBasis(v, &m->M);
			MatrixMulVector(&m->M, OFFSETS, m->off[0]);
			MatrixMulVector(&m->M, OFFSETS + 4, m->off[1]);
			MatrixMulVector(&m->M, OFFSETS + 8, m->off[2]);
			v[0] = 0 - v[0];
			v[2] = 0 - v[2];
			BuildOrthonormalBasis(v, &m->M2);
		}
		const int32_t c2 = m->counter - g.spike.start;
		if ((uint32_t)c2 <= (uint32_t)g.spike.ticks)
		{
			if (c2 == 0) *(uint32_t *)(Caster() + 0x7C) &= ~2u;
			int16_t L[4];
			SpikePoint(m, c2, L);
			m->M.t[0] = (int32_t)m->off[0][0] + L[0];
			m->M.t[1] = (int32_t)m->off[0][1] + L[1];
			m->M.t[2] = (int32_t)m->off[0][2] + L[2];
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = g.spike.depth;
			// 30 fps layer: see mag319_counter_attack_held.inc
			FX_HELD(held_note_main(PLAY_A, n);)
			FX_PART(PART_SPIKE);
			prim::play((prim::Layout *)m->layout_a, PrimCallback, (int)&arg, 0);
			FX_HELD(held_note_main(DRAW_SPIKE, n);)
			SpikeDraw(&m->M, L, &arg);
		}
		const int32_t c7 = m->counter - g.burst.start;
		if ((uint32_t)c7 < (uint32_t)g.burst.ticks)
		{
			m->M2.t[0] = (int32_t)m->pos[0] - m->off[0][0];
			m->M2.t[1] = (int32_t)m->pos[1] - m->off[0][1];
			m->M2.t[2] = (int32_t)m->pos[2] - m->off[0][2];
			ComposeAffineTransform(&Camera(), &m->M2, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = g.burst.depth;
			FX_HELD(held_note_main(PLAY_B, n);)
			FX_PART(PART_BURST);
			prim::play((prim::Layout *)m->layout_b, PrimCallback, (int)&arg, 0);
			const int32_t cs = m->counter - g.sprites.first;
			if (cs >= 0 && m->counter <= g.sprites.last && cs % g.sprites.every == 0)
			{
				for (int k = g.sprites.count; k > 0; k--)
				{
					SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), own::Fn(ORIG_SpriteTask));
					if (!s) continue;
					s->counter = 0;
					const int32_t sp = g.sprites.spread;
					s->pos[0] = (int16_t)(CrtRand() % (2 * sp) + m->pos[0] - sp);
					s->pos[1] = (int16_t)(CrtRand() % (2 * sp) + m->pos[1] - sp);
					s->pos[2] = (int16_t)(CrtRand() % (2 * sp) + m->pos[2] - sp);
				}
			}
		}
		if (m->counter == g.timing.sound) BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND), 0, 0x80);
		if (m->counter == g.timing.damage) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (m->counter == g.timing.bones)
		{
			*(uint32_t *)(Caster() + 0x7C) |= 2u;
			for (int k = 0; k < 5; k++) StartBoneScale(g.bones.bone[k], g.bones.delay[k], g.bones.grow_x, g.bones.grow_y);
		}
		m->counter++;
		if (m->counter >= g.timing.end)
		{
			StreamLoad(0x168, TexBase() + STREAM_DEST, 0);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x65DBE0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag319_counter_attack_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMain(), (void *)TexBase(), 0x898, 1);
				InitTaskQueuePool(QBones(), (void *)(TexBase() + POOL_Bones), 0x1C, 5);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + POOL_Sprites), 0x18, 0x10);
			}
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x223);
			m->ent = Entity(Ctx()->actions[0].targets[0]);
			GetDefaultEffectPosition(m->ent, m->pos);
			DecodeModelPrimLayout(fxp::layout(PG().layouts.spike, MODEL_A, 0x368), m->layout_a, 0x368);
			DecodeModelPrimLayout(fxp::layout(PG().layouts.burst, MODEL_B, 0x46C), m->layout_b, 0x46C);
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
		}
		int a0, a1, a2;
		if (r->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QBones());
			a2 = ExecuteTaskQueue(QSprites());
		}
		else a0 = a1 = a2 = (int)n;
		if (r->counter >= 1) StreamPump();
		if (r->started)
		{
			if (a0 == 0 && a1 == 0 && a2 == 0 && StreamDone(a2) != 0) return TASK_END;
			r->counter = 0x40;
		}
		r->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x65DB40) and setup (0x65DB30 -> _Init 0x65DB60): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the context, the caster, the root, the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = MagicTextureOff();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		TaskQueue *root = (TaskQueue *)own::R(0x25053E0);
		InitTaskQueuePool(root, (void *)own::R(0x25053F0), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(root, own::Fn(ORIG_RootTask)) + 0xC, 0, 2);
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		return root;
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "main", "Main", "The main's ticks, the sound, the damage; the texture and the camera" },
			{ PART_SPIKE, "spike", "Spike", "Layout A and the spike model travelling from the caster's bone to the target (the caster's arm object shown)" },
			{ PART_BURST, "burst", "Burst", "Layout B at the target" },
			{ PART_SPRITES, "sprites", "Sprites", "Sprites around the target on a few ticks (a flipbook)" },
			{ 0, "bones", "Arm regrowth", "The caster's arm object hidden and five of its bones grown back from nothing" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The main's ticks" },
		};
#define CPF(member) (uint32_t)offsetof(CounterParams, member)
		static const Field FIELDS[] = {
			{ "main", "sounds.start", "Sound", "At the sound's tick", CPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "timing.sound", "Sound", "The main's tick of the sound", CPF(timing.sound), fxp::I16, 0, 400 },
			{ "main", "timing.damage", "Damage", "Its tick the target's damage is shown", CPF(timing.damage), fxp::I16, 0, 400 },
			{ "main", "timing.bones", "Bones", "Its tick the arm is hidden and its bones grow back", CPF(timing.bones), fxp::I16, 0, 400 },
			{ "main", "timing.end", "End", "Its last tick (then the battle file it streams)", CPF(timing.end), fxp::I16, 1, 400 },
			{ "main", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", CPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "main", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", CPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "spike", "spike.start", "Start", "The main's tick it starts (its points are taken on tick 2)", CPF(spike.start), fxp::I16, 2, 400 },
			{ "spike", "spike.ticks", "Ticks", "Its travel's ticks (drawn one tick more: from the caster's bone to the target)", CPF(spike.ticks), fxp::I16, 1, 400 },
			{ "spike", "spike.bone", "Bone", "The caster's bone it starts from", CPF(spike.bone), fxp::I16, 0, 63 },
			{ "spike", "spike.depth", "Depth", "Its layout's depth offset", CPF(spike.depth), fxp::I16, -0x1000, 0x1000 },
			{ "spike", "tables.offsets", "Offsets", "Three offsets x / y / z (4 words each) turned along its direction: the first one moves the spike and the burst", CPF(tables.offsets), fxp::Data, 0, 0x7FFFFFFF },
			{ "spike", "layouts.spike", "Layout", "Layout A", CPF(layouts.spike), fxp::Layout, 0, 0x7FFFFFFF },
			{ "spike", "models.spike", "Model", "The spike model", CPF(models.spike), fxp::Model, 0, 0x7FFFFFFF },
			{ "burst", "burst.start", "Start", "The main's tick it starts", CPF(burst.start), fxp::I16, 2, 400 },
			{ "burst", "burst.ticks", "Ticks", "Its ticks", CPF(burst.ticks), fxp::I16, 0, 400 },
			{ "burst", "burst.depth", "Depth", "Its layout's depth offset", CPF(burst.depth), fxp::I16, -0x1000, 0x1000 },
			{ "burst", "layouts.burst", "Layout", "Layout B", CPF(layouts.burst), fxp::Layout, 0, 0x7FFFFFFF },
			{ "sprites", "sprites.first", "First tick", "The main's first tick they appear on (during the burst)", CPF(sprites.first), fxp::I16, 0, 400 },
			{ "sprites", "sprites.last", "Last tick", "Its last tick they appear on", CPF(sprites.last), fxp::I16, 0, 400 },
			{ "sprites", "sprites.every", "Every", "Every that many ticks", CPF(sprites.every), fxp::I16, 1, 400 },
			{ "sprites", "sprites.count", "Count", "How many each time (16 at most alive)", CPF(sprites.count), fxp::I16, 0, 16 },
			{ "sprites", "sprites.spread", "Scatter", "Around the target: a random below 2 x this - this on each axis", CPF(sprites.spread), fxp::I16, 1, 0x2000 },
			{ "sprites", "sprites.life", "Life", "Their ticks (their flipbook's frames: 10 at most)", CPF(sprites.life), fxp::I16, 1, 10 },
			{ "sprites", "sprites.size", "Size", "Their size", CPF(sprites.size), fxp::I16, 0, 0x4000, fxp::Scale },
			{ "sprites", "seqs.sprite", "Flipbook", "The sprites' flipbook", CPF(seqs.sprite), fxp::Seq, 0, 0x7FFFFFFF },
			{ "bones", "bones.bone[0]", "Bone 1", "The first bone grown back", CPF(bones.bone[0]), fxp::I16, 0, 63 },
			{ "bones", "bones.bone[1]", "Bone 2", "The second", CPF(bones.bone[1]), fxp::I16, 0, 63 },
			{ "bones", "bones.bone[2]", "Bone 3", "The third", CPF(bones.bone[2]), fxp::I16, 0, 63 },
			{ "bones", "bones.bone[3]", "Bone 4", "The fourth", CPF(bones.bone[3]), fxp::I16, 0, 63 },
			{ "bones", "bones.bone[4]", "Bone 5", "The fifth", CPF(bones.bone[4]), fxp::I16, 0, 63 },
			{ "bones", "bones.delay[0]", "Delay 1", "The first bone's ticks before it grows", CPF(bones.delay[0]), fxp::I16, 0, 400 },
			{ "bones", "bones.delay[1]", "Delay 2", "The second's", CPF(bones.delay[1]), fxp::I16, 0, 400 },
			{ "bones", "bones.delay[2]", "Delay 3", "The third's", CPF(bones.delay[2]), fxp::I16, 0, 400 },
			{ "bones", "bones.delay[3]", "Delay 4", "The fourth's", CPF(bones.delay[3]), fxp::I16, 0, 400 },
			{ "bones", "bones.delay[4]", "Delay 5", "The fifth's", CPF(bones.delay[4]), fxp::I16, 0, 400 },
			{ "bones", "bones.grow_x", "Growth x / y", "Their scale x / y grows by this a tick (up to 0x1000)", CPF(bones.grow_x), fxp::I16, 1, 0x1000, fxp::Scale },
			{ "bones", "bones.grow_y", "Growth z", "Their scale z grows by this a tick (up to 0x1000)", CPF(bones.grow_y), fxp::I16, 1, 0x1000, fxp::Scale },
		};
#undef CPF
	}
}

	void register_mag319_counter_attack()
	{
		fxp::register_params({ 319, "Counter-attack", &counter319::live, &counter319::GAME, sizeof(counter319::CounterParams),
		                       counter319::table::PARTS, (int)(sizeof(counter319::table::PARTS) / sizeof(counter319::table::PARTS[0])),
		                       counter319::table::FIELDS, (int)(sizeof(counter319::table::FIELDS) / sizeof(counter319::table::FIELDS[0])),
		                       counter319::table::GROUPS, (int)(sizeof(counter319::table::GROUPS) / sizeof(counter319::table::GROUPS[0])) });
		own::note(counter319::ORIG_FileLoader, (void *)counter319::FileLoader);
		own::note(counter319::ORIG_Setup, (void *)counter319::Setup);
		own::note(counter319::ORIG_RootTask, (void *)counter319::RootTask);
		register_port(counter319::ORIG_RootTask, (void *)counter319::RootTask, "319 RootTask", 319);
		own::note(counter319::ORIG_MainTask, (void *)counter319::MainTask);
		register_port(counter319::ORIG_MainTask, (void *)counter319::MainTask, "319 MainTask", 319);
		own::note(counter319::ORIG_SpriteTask, (void *)counter319::SpriteTask);
		register_port(counter319::ORIG_SpriteTask, (void *)counter319::SpriteTask, "319 SpriteTask", 319);
		// 30 fps layer: see mag319_counter_attack_held.inc
		FX_HELD(register_mag319_held();)
	}
	FX_LAB_MODULE(register_mag319_counter_attack);
}

#ifdef FF8_FX_HELD
#include "mag319_counter_attack_held.inc"
#endif
