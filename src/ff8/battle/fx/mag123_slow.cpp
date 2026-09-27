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

// Effect 123: Slow (spell, MAG_123_*).
//
// Structure (setup MAG_123_SLOW 0x6BD1C0 -> _Init 0x6BD1F0, file loader 0x6BD1D0 = the texture
// file named at 0x126E4B4; the setup starts camera animation 0x126D730 and queues the TIM):
//   RootTask (0x6BD260) - alternates the packet arena (magic buffer + 0x1AAC / + 0x19AAC); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target; every tick rebuilds the effect camera matrix
//     (MAG_069_sub_67AAE0 of the battle camera, 0x2521470) and runs the target queue; ends when
//     it is empty.
//   Target (0x6BD3E0) - node 0x764, one per action: two keyframed prim-model layouts (the clock
//     0x12652B0 at the target's anchor, the second 0x1268D3C lowered by the target's size) played
//     with the callback ClockPart (0x6BD580), both turned with the target and scaled by its size;
//     sound at 0; the target model blinks (entity flag bit 3) over 20..59; a screen fade at 48;
//     when both layouts have finished: the status, end.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521448..0x25214B0 (texture file, magic buffer base, context, root pool,
// packet cursor 0x252146C, effect camera matrix 0x2521470, target / root queues). The target pool
// (3 x 0x764), the morph vertex buffer (+0x162C) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace slow123
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x252144C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521450); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252146C); }
	inline Mat4x3 *EffCam() { return (Mat4x3 *)0x2521470; }                // effect camera matrix
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521490; }         // pool: magic buffer, 3 x 0x764
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6BD260;
	static const uint32_t ORIG_TargetTask = 0x6BD3E0;
	static const uint32_t MODEL_Clock = 0x12652B0;      // prim-model layout data (0x1D8-byte layout)
	static const uint32_t MODEL_Lower = 0x1268D3C;      // prim-model layout data (0x568-byte layout)
	static const void *const SOUND_Slow = (const void *)0x126E4B0;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_063_sub_701270: rotation about the vertical axis by -angle (3x3 and pad; translation not written)
	inline void RotationY(int16_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)((int32_t)(uint16_t)angle, out); }
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); } // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x764 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor (GetDefaultEffectPosition)
		int16_t spawn[4];  // +0x1C effect spawn position of bone 0xF0
		uint8_t clock[0x1D8]; // +0x24 prim-model layout (prim::Layout: data, frame, state)
		uint8_t lower[0x568]; // +0x1FC prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 camera x (turn about the vertical axis + position)
		int32_t scale[3];  // +0x20 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x2C scale present (never written by the original, see TargetTask)
		uint32_t morph;    // +0x30 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x764, "Slow nodes");
	static_assert(sizeof(PrimArg) == 0x34, "Slow prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6BD580): one object of a layout (keyframe model element)
	// ------------------------------------------------------------------
	static void __cdecl ClockPart(prim::Layout *l, prim::Record *r, int arg_)
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
		RotationFromAngles(r->rot, &m);
		// the record's offset (scaled by the block's scale when "scaled")
		int16_t off[4] = {};
		const int32_t scaled = arg->scaled;
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		if (r->flags & 0x1000)
		{
			// through the effect camera matrix (camera-facing), the block's position added below
			GteSetRotMatrixCtrl(EffCam());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(EffCam(), &m);
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
				// diagonal scale matrix (its pad and translation are not written: sub_56C220 reads the 3x3)
				Mat4x3 s = {};
				s.m[0][0] = r->scale[0];
				s.m[1][1] = r->scale[1];
				s.m[2][2] = r->scale[2];
				MatrixMultiply3x3(&m, &s);
			}
			else
			{
				int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		if (r->flags & 0x2000) *(uint32_t *)(h + 0x1C) |= 0xC;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag123_slow_held.h"
#endif

namespace ff8fx
{
namespace slow123
{
	// ------------------------------------------------------------------
	// Target (0x6BD3E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter == 0) BdPlaySE3D(SOUND_Slow, 1, t->pos);
		PrimArg arg;
		arg.morph = TexBase() + 0x162C;
		const int32_t s = (*(const int16_t *)(t->entity + 0x26) >> 1) + 0x800;
		arg.scale[2] = s;
		arg.scale[1] = s;
		arg.scale[0] = s;
		// VANILLA UNINITIALISED READ (0x6BD3E0..0x6BD4E9): the block's "scaled" word [esp+0x2C] (the
		// task's entry esp - 8) is never written, the callback tests it (0x6BD66B). With the game's
		// executor the slot holds what the root's MAG_069_sub_67AAE0 left in its frame ([esp+0x14]
		// there): the int32 it stores as the effect camera matrix's m[1][0] (0 unless the camera
		// rolls; then the layouts are scaled like Haste's)
		arg.scaled = EffCam()->m[1][0];
		RotationY((int16_t)-*(const int16_t *)(t->entity + 0xE), &arg.m);
		arg.m.t[0] = t->pos[0];
		arg.m.t[2] = t->pos[2];
		arg.m.t[1] = *(const int16_t *)(t->entity + 0x24);
		// 30 fps layer: see mag123_slow_held.inc
		FX_HELD(held_note_play(t->clock, &arg);)
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		int32_t left = prim::play((prim::Layout *)t->clock, ClockPart, (int)&arg, 0);
		RotationY((int16_t)-*(const int16_t *)(t->entity + 0xE), &arg.m);
		arg.m.t[0] = t->pos[0];
		arg.m.t[2] = t->pos[2];
		arg.m.t[1] = (int32_t)t->spawn[1] - (s >> 4) - (s >> 2);
		// 30 fps layer: see mag123_slow_held.inc
		FX_HELD(held_note_play(t->lower, &arg);)
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		left |= prim::play((prim::Layout *)t->lower, ClockPart, (int)&arg, 0);
		// the target blinks over 20..59 (entity flag bit 3 on the ticks e % (40 - e) == 0)
		const int32_t e = t->counter - 0x14;
		if ((uint32_t)e < 0x28)
		{
			if (e != 0 && (uint32_t)e % (uint32_t)(0x28 - e) == 0) *t->entity |= 8;
			else *(uint16_t *)t->entity &= 0xFFF7;
		}
		if (t->counter == 0x30) ScreenFadeTask(0, 1, 1, 0x60);
		if (left == 0)
		{
			*(uint16_t *)t->entity &= 0xFFF7;
			ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BD260)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag123_slow_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1AAC;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19AAC;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x764, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x1D6);
				t->action = (int16_t)(uint16_t)r->action;
				t->entity = Entity(Ctx()->actions[t->action].targets[0]);
				GetDefaultEffectPosition(t->entity, t->pos);
				GetEffectSpawnPosition(t->entity, 0xF0, 0, t->spawn);
				DecodeModelPrimLayout(MODEL_Clock, t->clock, 0x1D8);
				DecodeModelPrimLayout(MODEL_Lower, t->lower, 0x568);
				r->action++;
			}
		}
		EffectCameraMatrix(&Camera(), EffCam());
		const int a = r->started ? ExecuteTaskQueue(QTargets()) : (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag123_slow()
	{
		register_port(slow123::ORIG_RootTask, (void *)slow123::RootTask, "S123 RootTask", 123);
		register_port(slow123::ORIG_TargetTask, (void *)slow123::TargetTask, "S123 TargetTask", 123);
		// 30 fps layer: see mag123_slow_held.inc
		FX_HELD(register_mag123_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag123_slow_held.inc"
#endif
