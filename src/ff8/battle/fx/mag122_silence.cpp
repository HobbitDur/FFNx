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

// Effect 122: Silence (spell, MAG_122_*).
//
// Structure (setup MAG_122_SILENCE 0x6BD8E0 -> _Init 0x6BD910, file loader 0x6BD8F0 = the texture
// file named at 0x1272694; the setup starts camera animation 0x1271C68 and queues the TIM):
//   RootTask (0x6BD980) - alternates the packet arena (magic buffer + 0x1124 / + 0x11124); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target (no wait); runs the target queue, ends when it is
//     empty.
//   Target (0x6BDAC0) - node 0x4AC, one per action: marks the target entity (flags word bit
//     0x2000) while it runs; plays the prim-model layout 0x126E4C0 with the callback MutePart
//     (0x6BDB90) at the target's effect anchor 0xF0 every tick (sound at 0), depth offset from the
//     target's size; damage at 50; ends at 60 (mark cleared).
//   MutePart (0x6BDB90) - one object of the layout: record flag 0x200 = the offset stays in
//     screen axes (the anchor through the camera, the offset added unrotated, the rotation not
//     composed with the camera), else anchor + offset through the camera; flag 0x100 = scale by a
//     diagonal matrix product, else scale3DMatrix; object 4 is drawn 0x20 nearer.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25214B0..0x25214F8 (texture file, magic buffer base, context, root pool,
// packet cursor 0x25214D4, target / root queues). The target pool (3 x 0x4AC), the morph vertex
// buffer (+0xE04) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace silence122
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25214B4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25214B8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25214D4); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25214D8; }         // pool: magic buffer, 3 x 0x4AC
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6BD980;
	static const uint32_t ORIG_TargetTask = 0x6BDAC0;
	static const uint32_t MODEL_Mute = 0x126E4C0;        // prim-model layout data (0x498-byte layout)
	static const void *const SOUND_Silence = (const void *)0x1272690;

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
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
	struct TargetNode // pool of 3 nodes of 0x4AC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		uint8_t mute[0x498]; // +0x14 prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+ a word never written)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x4AC, "Silence nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Silence prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6BDB90): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl MutePart(prim::Layout *l, prim::Record *r, int arg_)
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
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
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
			// anchor + offset through the camera, rotation composed with the camera
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
			if (r->flags & 0x100)
			{
				// diagonal matrix product (sub_56C220)
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
		int32_t depth = arg->depth;
		*(int32_t *)(h + 0x18) = depth;
		if (r->flags_lo == 4)
		{
			depth = (int32_t)((uint32_t)depth - 0x20);
			*(int32_t *)(h + 0x18) = depth;
		}
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag122_silence_held.h"
#endif

namespace ff8fx
{
namespace silence122
{
	// ------------------------------------------------------------------
	// Target (0x6BDAC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter >= 0)
		{
			PrimArg arg;
			GetEffectSpawnPosition(t->entity, 0xF0, 0, arg.pos);
			// never written by the original (stack word), only loaded into the GTE V0 pad
			arg.pos[3] = 0;
			if (t->counter == 0) BdPlaySE3D(SOUND_Silence, 1, arg.pos);
			arg.depth = (int32_t)(0xFFFFF800u - (uint32_t)(int32_t)*(const int16_t *)(t->entity + 0x26)) >> 5;
			arg.morph = TexBase() + 0xE04;
			// 30 fps layer: see mag122_silence_held.inc
			FX_HELD(held_note_play(t, &arg);)
			prim::play((prim::Layout *)t->mute, MutePart, (int)&arg, 0);
		}
		if (t->counter == 0x32) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= 0x3C)
		{
			*(uint16_t *)t->entity &= 0xDFFF;
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BD980)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag122_silence_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1124;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x11124;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x4AC, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x128);
				t->action = (int16_t)(uint16_t)r->action;
				uint8_t *entity = Entity(Ctx()->actions[t->action].targets[0]);
				t->entity = entity;
				*(uint8_t *)(entity + 1) |= 0x20;
				DecodeModelPrimLayout(MODEL_Mute, t->mute, 0x498);
				r->action++;
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag122_silence()
	{
		register_port(silence122::ORIG_RootTask, (void *)silence122::RootTask, "S122 RootTask", 122);
		register_port(silence122::ORIG_TargetTask, (void *)silence122::TargetTask, "S122 TargetTask", 122);
		// 30 fps layer: see mag122_silence_held.inc
		FX_HELD(register_mag122_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag122_silence_held.inc"
#endif
