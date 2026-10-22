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
// Effect 280: Full-cure (kernel.bin magic Full-cure +4 = 280: Selphie's Slot; MAG_280_*). Module 0x67E730..0x67EFD0.
//
// Structure (setup 0x67E730 -> 0x67E760, file loader 0x67E740 = the texture file named at 0x10AB128; the setup queues the
// TIM, takes the magic buffer (0x2508048), draws one rand() into 0x2508040 (never read again), keeps the cast context
// (0x2507FDC) and the caster's entity (0x2507FE0, never read again), starts the root task in its 1-node pool 0x2508028 and
// the camera animation 0x10AAFEC (always: the context flag bit 0 is not tested)):
//   RootTask (0x67E7F0) - alternates the packet arena (magic buffer + 0x1DF0 / + 0x11DF0 by tick parity, cursor
//     0x250803C); zeroes the flash level (0x2508044); on counter 1 (once) sets up the pools (control: magic buffer + 0x1BD4,
//     1 x 0x1C; target: + 0x140, 3 x 0x8DC; sparkle: + 0, 16 x 0x14) and starts the control task with the entities of
//     the targets of action 0; then runs the control, target and sparkle queues (the target queue's result is not used);
//     sets the screen flash to the level (Effect_SetScreenFlash, mode 0: every entity draw entry); ends when the control
//     and sparkle queues are both empty.
//   ControlTask (0x67E980) - 0x41 ticks: the flash level ramps up (c * 0x100 for c < 8), holds 0x800, ramps down
//     ((0x41 - c) * 0x100 for c > 0x39); every 20 ticks (c = 0, 20, 40, 60 while targets are left) a TargetTask on the
//     next target: anchor = the target's effect point 0xF1 (GetEffectSpawnPosition, taken once), layout 0x10A7A64
//     (0x8C4 bytes), sound 0x10AB124.
//   TargetTask (0x67EA80) - 0x1E ticks: the layout at the anchor (prim player, callback 0x67EB50, play block: anchor,
//     the counter word, depth -0x80, morph buffer magic buffer + 0x1BF0); from counter 0x13 on even counters a
//     SparkleTask at the target's effect point (counter % the model's first byte) taken once; at the end the action
//     result of this target (0x67EEE0).
//   SparkleTask (0x67EF50) - 8 ticks: sequence 0x10A7974 frame = its counter at its point through the camera
//     (TransformCameraByShadowRotation 0x800 / -0x200, header 0xB4 on the Field_Alloc scratch).
//   PrimCallback (0x67EB50) - one object of the layout (X-ATM092's 0x686120 shape with its flags 0x40000 / 0x1000 and
//     without the block scale): vertex frame (or blend into the morph buffer), rotation (flag 0x40000: 0x7015B0, else
//     0x701310); flag 0x1000: the anchor through the camera + the offset through the module matrix 0x2508050 (never written:
//     zero), the rotation through it too; flag 0x200: the anchor through the camera + the offset; else the offset + the
//     anchor through the camera, composed with it; + the camera translation; scale (flag 0x100: 3x3 product, else
//     scale3DMatrix); fade; 0x7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: the module writes no entity field directly and no camera word; it reads the targets' effect points
// (GetEffectSpawnPosition: their current pose) at a task's start and the camera matrix 0x1D97778; engine writes: the
// screen flash level into every entity draw entry (0x1D98990 + k * 0x2C: +2 level, +0x28 mode) every tick, the action
// results (ApplyActionResultToTarget), the camera animation 0x10AAFEC (Battle_PlayCameraAnimation, at the setup).
// Module globals: 0x2507FD8..0x2508070 (texture file 0x2507FD8, context 0x2507FDC, caster entity 0x2507FE0, sparkle queue
// 0x2507FE8, target queue 0x2507FF8, control queue 0x2508008, root queue 0x2508018, root pool 0x2508028 (1 x 0x14),
// packet cursor 0x250803C, rand word 0x2508040, flash level 0x2508044, magic buffer base 0x2508048, matrix 0x2508050).
// NEVER-WRITTEN STACK WORDS: none read (the play block is fully written; the callback's offset word +6 and matrix pad
// only reach the GTE VZ0 / R33 high halves).

#include "mag_common.h"

namespace ff8fx
{
namespace fullcure280
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507FDC); }
	inline TaskQueue *QSparkle() { return (TaskQueue *)0x2507FE8; }      // pool: magic buffer + 0, 16 x 0x14
	inline TaskQueue *QTarget() { return (TaskQueue *)0x2507FF8; }       // pool: magic buffer + 0x140, 3 x 0x8DC
	inline TaskQueue *QControl() { return (TaskQueue *)0x2508008; }      // pool: magic buffer + 0x1BD4, 1 x 0x1C
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x250803C); }
	inline int32_t &FlashLevel() { return var<int32_t>(0x2508044); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2508048); }      // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &ModuleMatrix() { return var<Mat4x3>(0x2508050); }     // never written (zero)

	static const uint32_t ORIG_RootTask = 0x67E7F0;
	static const uint32_t ORIG_ControlTask = 0x67E980;
	static const uint32_t ORIG_TargetTask = 0x67EA80;
	static const uint32_t ORIG_SparkleTask = 0x67EF50;
	static const uint32_t LAYOUT = 0x10A7A64;     // prim-model layout data (0x8C4 bytes decoded)
	static const uint32_t SEQUENCE = 0x10A7974;   // sparkle sequence
	static const void *const SOUND = (const void *)0x10AB124;
	static const uint32_t ARENA_A = 0x1DF0;        // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11DF0;
	static const uint32_t MORPH = 0x1BF0;          // magic buffer: blended vertex frames
	static const int32_t CONTROL_TICKS = 0x41, TARGET_TICKS = 0x1E, SPARKLE_FIRST = 0x13, SPARKLE_FRAMES = 8;
	static const int32_t EFFECT_POINT = 0xF1;      // GetEffectSpawnPosition: the target's effect point

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_RenderPrimModel: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // MatrixMultiply3x3
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2508028)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F the pools are set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct ControlNode // pool of 1 node of 0x1C bytes (magic buffer + 0x1BD4)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ent[3];   // +0x10 their entities (the original indexes past the node with more than 3 targets: Ent())
		uint8_t *&Ent(uint32_t i) { return ((uint8_t **)((uint8_t *)this + 0x10))[i]; }
	};
	struct TargetNode // pool of 3 nodes of 0x8DC bytes (magic buffer + 0x140)
	{
		TaskNode hdr;
		uint8_t *entity;   // +0x0C
		int16_t anchor[3]; // +0x10 effect point 0xF1 at the start
		int16_t counter;   // +0x16
		uint8_t layout[0x8C4]; // +0x18
	};
	struct SparkleNode // pool of 16 nodes of 0x14 bytes (magic buffer + 0)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[3];    // +0x00 anchor
		int16_t counter;   // +0x06 the node's counter word (GTE VZ0 high half only)
		int32_t depth;     // +0x08 -0x80
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(ControlNode) == 0x1C && sizeof(TargetNode) == 0x8DC && sizeof(SparkleNode) == 0x14, "Full-cure nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Full-cure block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x67EB50): one object of the layout
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
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const uint32_t flags = r->flags;
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 }; // (4th word never written in the original: GTE VZ0 high half only)
		if (flags & 0x1000)
		{
			// the anchor through the camera + the offset through the module matrix; the rotation through it too
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&ModuleMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t v[3];
			GteReadMAC123(v);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)v[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)v[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)v[2]);
			MatrixMultiply(&ModuleMatrix(), &m);
		}
		else if (flags & 0x200)
		{
			// the anchor through the camera + the offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// the offset + the anchor through the camera, composed with it
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
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
			if (flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
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

	// the flash level of control counter c
	static int32_t ControlFlash(int16_t c)
	{
		if (c < 8) return (int32_t)c * 0x800 >> 3;
		if (c > 0x39) return (0x41 - (int32_t)c) * 0x800 >> 3;
		return 0x800;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag280_full_cure_held.h"
#endif

namespace ff8fx
{
namespace fullcure280
{
	// ------------------------------------------------------------------
	// Action result of a target (0x67EEE0): every target record of action 0 whose slot is this entity's
	// ------------------------------------------------------------------
	static void ApplyResult(uint8_t *e)
	{
		const int32_t slot = (int32_t)((uint32_t)e - 0x1D972C0) / 0x9C;
		for (int32_t i = 0; i < (int32_t)Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)*t == slot) ApplyActionResultToTarget(t);
		}
	}

	// ------------------------------------------------------------------
	// Sparkle (0x67EF50): sequence frame = counter at the point through the camera, 8 frames
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		// 30 fps layer: see mag280_full_cure_held.inc
		FX_HELD(held_note_sparkle(s);)
		TransformCameraByShadowRotation(s->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQUENCE;
		*(int16_t *)(h + 4) = s->frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		s->frame++;
		return s->frame < SPARKLE_FRAMES ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Target (0x67EA80): the layout at the anchor, sparkles, the action result at the end
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		// 30 fps layer: see mag280_full_cure_held.inc
		FX_HELD(held_note_target(t);)
		PrimArg arg;
		memcpy(arg.pos, t->anchor, 6);
		arg.counter = t->counter;
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, 0);
		if (t->counter >= SPARKLE_FIRST && !(t->counter & 1))
		{
			SparkleNode *s = (SparkleNode *)AddTaskToQueue(QSparkle(), ORIG_SparkleTask);
			if (s)
			{
				uint8_t *e = t->entity;
				// the bone count: first byte of the skeleton (entity +0x64 -> model record -> skeleton)
				const int32_t bones = **(const uint8_t *const *)*(const uint32_t *)(e + 0x64);
				GetEffectSpawnPosition(e, (int32_t)t->counter % bones, 0, s->pos);
				s->frame = 0;
			}
		}
		t->counter++;
		if (t->counter >= TARGET_TICKS)
		{
			ApplyResult(t->entity);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Control (0x67E980): the flash ramp, a target task every 20 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl ControlTask(TaskNode *n)
	{
		ControlNode *c = (ControlNode *)n;
		FlashLevel() = ControlFlash(c->counter);
		const uint32_t k = (uint32_t)(int32_t)c->counter;
		if (k < 0x3F && k % 20 == 0 && k / 20 < (uint32_t)(int32_t)c->count)
		{
			TargetNode *t = (TargetNode *)AddTaskToQueue(QTarget(), ORIG_TargetTask);
			if (t)
			{
				uint8_t *e = c->Ent(k / 20);
				t->entity = e;
				GetEffectSpawnPosition(e, EFFECT_POINT, 0, t->anchor);
				t->counter = 0;
				DecodeModelPrimLayout(LAYOUT, t->layout, 0x8C4);
				BdPlaySE(SOUND, 0, 0x80);
			}
		}
		c->counter++;
		if (c->counter >= CONTROL_TICKS)
		{
			FlashLevel() = 0;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x67E7F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag280_full_cure_held.inc
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
		FlashLevel() = 0;
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QControl(), (void *)(TexBase() + 0x1BD4), 0x1C, 1);
			InitTaskQueuePool(QTarget(), (void *)(TexBase() + 0x140), 0x8DC, 3);
			InitTaskQueuePool(QSparkle(), (void *)TexBase(), 0x14, 0x10);
			ControlNode *c = (ControlNode *)AddTaskToQueue(QControl(), ORIG_ControlTask);
			Memset32((uint8_t *)c + 0xC, 0, 4);
			c->count = (int16_t)Ctx()->actions[0].target_count;
			for (int32_t i = 0; i < c->count; i++) c->Ent((uint32_t)i) = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QControl());
			ExecuteTaskQueue(QTarget());
			b = ExecuteTaskQueue(QSparkle());
		}
		else a = b = (int)n;
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag280_full_cure()
	{
		register_port(fullcure280::ORIG_RootTask, (void *)fullcure280::RootTask, "280 RootTask", 280);
		register_port(fullcure280::ORIG_ControlTask, (void *)fullcure280::ControlTask, "280 ControlTask", 280);
		register_port(fullcure280::ORIG_TargetTask, (void *)fullcure280::TargetTask, "280 TargetTask", 280);
		register_port(fullcure280::ORIG_SparkleTask, (void *)fullcure280::SparkleTask, "280 SparkleTask", 280);
		// 30 fps layer: see mag280_full_cure_held.inc
		FX_HELD(register_mag280_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag280_full_cure_held.inc"
#endif
