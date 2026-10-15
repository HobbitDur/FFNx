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

// Effect 226: White Wind, the enemy version (enemy attack 276 of kernel.bin, Adamantoise c0m046;
// MAG_226_*). No other kernel.bin entry uses effect 226. Quistis' blue magic White Wind is effect
// 297 (its own module MAG_297_* at 0x6730D0..): its root task 0x6730D0 and prim callback 0x673510 are
// byte-identical to this module's, its scene task 0x6731D0 is a variant, and this module calls its
// camera billboard helper 0x673810 directly (ported here as Billboard).
//
// Structure (setup MAG_226 0x694520 -> _Init 0x694550, file loader 0x694530 = the texture file
// named at 0x105BA98; the setup keeps the CASTER's entity (0x2508778), starts camera animation
// 0x113555C and queues the TIM):
//   RootTask (0x6945D0) - alternates the packet arena (magic buffer + 0xECC / + 0x10ECC); on counter 1
//     sets up the pools (targets 3 x 0x290 at + 0, scene 1 x 0x33C at + 0x7B0) and starts the scene
//     task; then the module camera matrix 0x25087D0 (MAG_069_sub_67AAE0 of the camera), the scene
//     queue and the target queue; ends when both queues are empty.
//   SceneTask (0x6946D0) - counters 0..0x1C, draws THEN updates:
//     counters 0..0x15: layout 0x1134B2C (+0x18) at the caster's default effect position (taken at
//       counter 0, y from the caster's +0x92 / +0x24), camera billboard, scaled by the caster's size;
//     counters 7..0x1C: layout 0x11340C4 (+0x1A8) at (0, the caster's y, the mean of the targets'
//       +0x20 words) (taken at counter 7), matrix diag(0x1000, 0x800, 0x800) composed with the camera;
//     counter 1: sound 0x11355CC; counters 0x13 / 0x17 / 0x1B: the target task of target 0 / 1 / 2
//       of action 0 (if it exists).
//   TargetTask (0x694D40) - layout 0x113202C (+0x18) at the target's default effect position (y from
//     its +0x92 / +0x24), camera billboard, scaled by the target's size; when the layout ends (the
//     play of its last frame): ApplyResult (0x694DF0, the target's records of action 0) and the end.
//   PrimCallback (0x694A40) - one object of a layout: vertex frame (or blend into the block's morph
//     buffer, magic buffer + 0xAEC), rotation from the record's angles (flag 0x40000: 0x7015B0);
//     flag 0x1000: the offset through the module camera matrix, composed with it unless flag 0x8000;
//     0x200: the offset as the translation; else the offset through the block's matrix, composed
//     with it unless flag 0x8000; + the block's translation; scale (flag 0x100: 3x3 product, else
//     scale3DMatrix); fade; 0x7043B0 at the block's depth.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: positions, sizes,
// the camera matrix 0x1D97778).
// Module globals: 0x2508770..0x25087F0 (texture file 0x2508770, context 0x2508774, caster entity
// 0x2508778, scene queue 0x2508780, root queue 0x2508790, target queue 0x25087A0, root pool
// 0x25087B0 (1 x 0x14), packet cursor 0x25087C4, magic buffer base 0x25087C8, module camera matrix
// 0x25087D0). The scene / target pools, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS (none reaches the output): the block matrix pad (+0x12: GTE R33 pad only)
// and the callback's offset 4th word (GTE VZ0 high half only).

#include "mag_common.h"

namespace ff8fx
{
namespace whitewind226
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2508774); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508778); }          // caster entity (setup)
	inline TaskQueue *QScene() { return (TaskQueue *)0x2508780; }            // pool: magic buffer + 0x7B0, 1 x 0x33C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25087A0; }          // pool: magic buffer, 3 x 0x290
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25087C4); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x25087C8); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &ModMatrix() { return var<Mat4x3>(0x25087D0); }            // camera matrix of the effect (every tick)

	static const uint32_t ORIG_RootTask = 0x6945D0;
	static const uint32_t ORIG_SceneTask = 0x6946D0;
	static const uint32_t ORIG_TargetTask = 0x694D40;
	static const uint32_t MODEL_Caster = 0x1134B2C;      // prim-model layout data (0x188-byte layout)
	static const uint32_t MODEL_Sweep = 0x11340C4;       // prim-model layout data (0x194-byte layout)
	static const uint32_t MODEL_Target = 0x113202C;      // prim-model layout data (0x278-byte layout)
	static const void *const SOUND_Wind = (const void *)0x11355CC;
	static const uint32_t MORPH = 0xAEC;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xECC;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10ECC;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }       // sub_56C220: a = a * b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x25087B0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct SceneNode // pool of 1 node of 0x33C bytes (magic buffer + 0x7B0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t caster[4]; // +0x10 caster layout anchor (default effect position, y replaced; counter 0)
		uint8_t lay_caster[0x188]; // +0x18 prim-model layout
		int16_t sweep[3];  // +0x1A0 sweep layout anchor (counter 7)
		int16_t pad1A6;
		uint8_t lay_sweep[0x194];  // +0x1A8 prim-model layout
	};
	struct TargetNode // pool of 3 nodes of 0x290 bytes (magic buffer)
	{
		TaskNode hdr;
		uint8_t *entity;   // +0x0C target entity
		int16_t pos[4];    // +0x10 anchor (default effect position, y replaced)
		uint8_t layout[0x278]; // +0x18 prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the scene / target task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 block matrix (pad +0x12 never written)
		int32_t depth;     // +0x20 prim header +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(SceneNode) == 0x33C && sizeof(TargetNode) == 0x290, "White Wind nodes");
	static_assert(sizeof(PrimArg) == 0x28, "White Wind prim block");

	enum : int { PLAY_CASTER = 0, PLAY_SWEEP = 1, PLAY_TARGET = 2 };

	// ------------------------------------------------------------------
	// Prim-player callback (0x694A40): one object of a layout
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
		if (r->flags & 0x1000)
		{
			// the offset through the module camera matrix
			GteSetRotMatrixCtrl(&ModMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&ModMatrix(), &m);
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
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// MAG_226_sub_673810 (in effect 297's module): m = the camera's 3x3 (pad untouched), t = camera * t
	static void Billboard(Mat4x3 *m)
	{
		memcpy(m->m, Camera().m, 18);
		TransformVectorBy3x3Matrix(&Camera(), m->t, m->t);
		m->t[0] = (int32_t)((uint32_t)m->t[0] + (uint32_t)Camera().t[0]);
		m->t[1] = (int32_t)((uint32_t)m->t[1] + (uint32_t)Camera().t[1]);
		m->t[2] = (int32_t)((uint32_t)m->t[2] + (uint32_t)Camera().t[2]);
	}

	// anchor height of an entity: (2h / 3 + h / 2) / 2 above its +0x24 (h = its +0x92)
	static int16_t AnchorY(const uint8_t *e)
	{
		const int32_t h = *(const int16_t *)(e + 0x92);
		const int32_t v = ((2 * h) / 3 + h / 2) / 2;
		return (int16_t)(v + *(const int16_t *)(e + 0x24));
	}

	// billboard scale of an entity's size: size / 2 * 4 + 0x1000
	static int32_t SizeScale(const uint8_t *e)
	{
		return (int32_t)*(const int16_t *)(e + 0x26) / 2 * 4 + 0x1000;
	}

	// 0x694DF0: the action results of every target record of action 0 for this entity
	static void ApplyResult(const uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (int i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *rec = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if (*rec == slot) ApplyActionResultToTarget(rec);
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag226_white_wind_held.h"
#endif

namespace ff8fx
{
namespace whitewind226
{
	// ------------------------------------------------------------------
	// Target (0x694D40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		PrimArg arg;
		arg.m.t[0] = t->pos[0];
		arg.m.t[1] = t->pos[1];
		arg.m.t[2] = t->pos[2];
		Billboard(&arg.m);
		const int32_t s = SizeScale(t->entity);
		const int32_t v[3] = { s, s, s };
		Scale3DMatrix(&arg.m, v);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag226_white_wind_held.inc
		FX_HELD(held_note_play((prim::Layout *)t->layout, PLAY_TARGET, t->pos, s);)
		if (prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, 0) == 0)
		{
			ApplyResult(t->entity);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Scene (0x6946D0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SceneTask(TaskNode *n)
	{
		SceneNode *s = (SceneNode *)n;
		PrimArg arg;
		if ((uint32_t)(int32_t)s->counter < 0x16)
		{
			if (s->counter == 0)
			{
				GetDefaultEffectPosition(Caster(), s->caster);
				s->caster[1] = AnchorY(Caster());
				DecodeModelPrimLayout(MODEL_Caster, s->lay_caster, 0x188);
			}
			arg.m.t[0] = s->caster[0];
			arg.m.t[1] = s->caster[1];
			arg.m.t[2] = s->caster[2];
			Billboard(&arg.m);
			const int32_t sc = SizeScale(Caster());
			const int32_t v[3] = { sc, sc, sc };
			Scale3DMatrix(&arg.m, v);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag226_white_wind_held.inc
			FX_HELD(held_note_play((prim::Layout *)s->lay_caster, PLAY_CASTER, s->caster, sc);)
			prim::play((prim::Layout *)s->lay_caster, PrimCallback, (int)&arg, 0);
		}
		if ((uint32_t)((int32_t)s->counter - 7) < 0x16)
		{
			if (s->counter == 7)
			{
				ActionData *a = Ctx()->actions;
				s->sweep[2] = 0;
				for (int i = 0; i < a->target_count; i++)
				{
					s->sweep[2] = (int16_t)(s->sweep[2] + *(const int16_t *)(Entity(a->targets[i * TARGET_STRIDE]) + 0x20));
					a = Ctx()->actions;
				}
				s->sweep[0] = 0;
				s->sweep[2] = (int16_t)((int32_t)s->sweep[2] / (int32_t)a->target_count);
				s->sweep[1] = AnchorY(Caster());
				DecodeModelPrimLayout(MODEL_Sweep, s->lay_sweep, 0x194);
			}
			arg.m.t[0] = s->sweep[0];
			arg.m.t[1] = s->sweep[1];
			arg.m.t[2] = s->sweep[2];
			arg.m.m[0][0] = 0x1000;
			arg.m.m[2][2] = 0x800;
			arg.m.m[1][1] = 0x800;
			arg.m.m[2][1] = 0;
			arg.m.m[2][0] = 0;
			arg.m.m[1][2] = 0;
			arg.m.m[1][0] = 0;
			arg.m.m[0][2] = 0;
			arg.m.m[0][1] = 0;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag226_white_wind_held.inc
			FX_HELD(held_note_play((prim::Layout *)s->lay_sweep, PLAY_SWEEP, s->sweep, 0);)
			prim::play((prim::Layout *)s->lay_sweep, PrimCallback, (int)&arg, 0);
		}
		if (s->counter == 1) BdPlaySE(SOUND_Wind, 0, 0x80);
		const int32_t k = (int32_t)s->counter - 0x13;
		if ((uint32_t)k < 0xC && (k & 3) == 0)
		{
			const uint32_t i = (uint32_t)k >> 2;
			if (i < Ctx()->actions[0].target_count)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				if (t)
				{
					uint8_t *e = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
					t->entity = e;
					GetDefaultEffectPosition(e, t->pos);
					t->pos[1] = AnchorY(e);
					DecodeModelPrimLayout(MODEL_Target, t->layout, 0x278);
				}
			}
		}
		s->counter++;
		return s->counter < 0x1D ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Root task (0x6945D0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag226_white_wind_held.inc
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
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QScene(), (void *)(TexBase() + 0x7B0), 0x33C, 1);
			InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x290, 3);
			((SceneNode *)AddTaskToQueue(QScene(), ORIG_SceneTask))->counter = 0;
		}
		int a, b;
		if (r->started)
		{
			EffectCameraMatrix(&Camera(), &ModMatrix());
			a = ExecuteTaskQueue(QScene());
			b = ExecuteTaskQueue(QTargets());
		}
		else a = b = (int)n;
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag226_white_wind()
	{
		register_port(whitewind226::ORIG_RootTask, (void *)whitewind226::RootTask, "226 RootTask", 226);
		register_port(whitewind226::ORIG_SceneTask, (void *)whitewind226::SceneTask, "226 SceneTask", 226);
		register_port(whitewind226::ORIG_TargetTask, (void *)whitewind226::TargetTask, "226 TargetTask", 226);
		// 30 fps layer: see mag226_white_wind_held.inc
		FX_HELD(register_mag226_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag226_white_wind_held.inc"
#endif
