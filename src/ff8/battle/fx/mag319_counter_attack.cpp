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

#include "mag_common.h"
#include <stddef.h>

namespace ff8fx
{
namespace counter319
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25053A4); }
	inline CastContext *&Ctx() { return var<CastContext *>(0x25053A8); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25053AC); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2505410); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSprites() { return (TaskQueue *)0x25053B0; }        // pool: + 0x924, 0x10 x 0x18
	inline TaskQueue *QBones() { return (TaskQueue *)0x25053C0; }          // pool: + 0x898, 5 x 0x1C
	inline TaskQueue *QMain() { return (TaskQueue *)0x25053D0; }           // pool: magic buffer, 1 x 0x898

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

	// counters 2..8 (0x65DE69..0x65DE9B): the point L between T and S, weight (c - 2) / 6 of T
	static void SpikePoint(const MainNode *m, int32_t c2, int16_t *L)
	{
		const uint32_t t = ((uint32_t)c2 << 12) / 6u;
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
		*(uint32_t *)h = PRIM_Spike;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the sprites (0x65E430..0x65E490)
	static void SpriteDraw(const int16_t *pos, int16_t frame)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, 0x1000, -0x400);
		*(uint32_t *)h = SEQ_Sprite;
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
		SpriteDraw(s->pos, s->counter);
		s->counter++;
		return s->counter < 0xA ? 0 : TASK_END;
	}

	// 0x65E4B0: a bone scale task on the caster's skeleton bone `bone` (no pool check: 5 nodes for the 5 calls)
	static void StartBoneScale(int32_t bone, int16_t delay, int16_t dx, int16_t dy)
	{
		BoneNode *b = (BoneNode *)AddTaskToQueue(QBones(), ORIG_BoneScaleTask);
		b->bone = **(uint8_t ***)(Caster() + 0x64) + bone * 0x30 + 0x10;
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
		PrimArg arg;
		if (m->counter == 2)
		{
			GetEffectSpawnPosition(Caster(), 0xD, 0x800, m->spawn);
			int32_t v[3] = { (int32_t)m->pos[0] - m->spawn[0], (int32_t)m->spawn[1] - m->pos[1], (int32_t)m->spawn[2] - m->pos[2] };
			BuildOrthonormalBasis(v, &m->M);
			MatrixMulVector(&m->M, OFFSETS, m->off[0]);
			MatrixMulVector(&m->M, OFFSETS + 4, m->off[1]);
			MatrixMulVector(&m->M, OFFSETS + 8, m->off[2]);
			v[0] = 0 - v[0];
			v[2] = 0 - v[2];
			BuildOrthonormalBasis(v, &m->M2);
		}
		const int32_t c2 = m->counter - 2;
		if ((uint32_t)c2 <= 6)
		{
			if (c2 == 0) *(uint32_t *)(Caster() + 0x7C) &= ~2u;
			int16_t L[4];
			SpikePoint(m, c2, L);
			m->M.t[0] = (int32_t)m->off[0][0] + L[0];
			m->M.t[1] = (int32_t)m->off[0][1] + L[1];
			m->M.t[2] = (int32_t)m->off[0][2] + L[2];
			ComposeAffineTransform(&Camera(), &m->M, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x80;
			// 30 fps layer: see mag319_counter_attack_held.inc
			FX_HELD(held_note_main(PLAY_A, n);)
			prim::play((prim::Layout *)m->layout_a, PrimCallback, (int)&arg, 0);
			FX_HELD(held_note_main(DRAW_SPIKE, n);)
			SpikeDraw(&m->M, L, &arg);
		}
		const int32_t c7 = m->counter - 7;
		if ((uint32_t)c7 < 0x20)
		{
			m->M2.t[0] = (int32_t)m->pos[0] - m->off[0][0];
			m->M2.t[1] = (int32_t)m->pos[1] - m->off[0][1];
			m->M2.t[2] = (int32_t)m->pos[2] - m->off[0][2];
			ComposeAffineTransform(&Camera(), &m->M2, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x80;
			FX_HELD(held_note_main(PLAY_B, n);)
			prim::play((prim::Layout *)m->layout_b, PrimCallback, (int)&arg, 0);
			if ((uint32_t)c7 >= 8 && (uint32_t)c7 < 0xF && !(c7 & 1))
			{
				for (int k = 4; k != 0; k--)
				{
					SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
					if (!s) continue;
					s->counter = 0;
					s->pos[0] = (int16_t)((CrtRand() & 0x3FF) + m->pos[0] - 0x200);
					s->pos[1] = (int16_t)((CrtRand() & 0x3FF) + m->pos[1] - 0x200);
					s->pos[2] = (int16_t)((CrtRand() & 0x3FF) + m->pos[2] - 0x200);
				}
			}
		}
		if (m->counter == 1) BdPlaySE(SOUND, 0, 0x80);
		if (m->counter == 0xC) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (m->counter == 0x1B)
		{
			*(uint32_t *)(Caster() + 0x7C) |= 2u;
			StartBoneScale(0xA, 0, 0x200, 0x400);
			StartBoneScale(0xD, 8, 0x200, 0x400);
			StartBoneScale(0x17, 0x10, 0x200, 0x400);
			StartBoneScale(0x18, 0x10, 0x200, 0x400);
			StartBoneScale(0x19, 0x10, 0x200, 0x400);
		}
		m->counter++;
		if (m->counter >= 0x27)
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
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x223);
			m->ent = Entity(Ctx()->actions[0].targets[0]);
			GetDefaultEffectPosition(m->ent, m->pos);
			DecodeModelPrimLayout(MODEL_A, m->layout_a, 0x368);
			DecodeModelPrimLayout(MODEL_B, m->layout_b, 0x46C);
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
}

	void register_mag319_counter_attack()
	{
		register_port(counter319::ORIG_RootTask, (void *)counter319::RootTask, "319 RootTask", 319);
		register_port(counter319::ORIG_MainTask, (void *)counter319::MainTask, "319 MainTask", 319);
		register_port(counter319::ORIG_SpriteTask, (void *)counter319::SpriteTask, "319 SpriteTask", 319);
		// 30 fps layer: see mag319_counter_attack_held.inc
		FX_HELD(register_mag319_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag319_counter_attack_held.inc"
#endif
