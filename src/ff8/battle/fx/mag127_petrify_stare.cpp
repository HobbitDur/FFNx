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

// Effect 127: Petrify Stare (enemy attack 29 of kernel.bin, Cockatrice; MAG_127_*). Same code as
// effect 120 (the Glacial Eye's Petrify Stare) with its own layouts, sizes and rotation builder.
//
// Structure (setup MAG_127_PETRIFY_STARE 0x6B93A0 -> _Init 0x6B93D0, file loader 0x6B93B0 = the
// texture file named at 0x124159C (mag126.tim); the setup keeps the CASTER's entity, starts camera
// animation 0x12414AC and queues the TIM):
//   RootTask (0x6B9450) - alternates the packet arena (magic buffer + 0x8A4 / + 0x188A4); clears the
//     screen-flash level; on counter 1 of a 10-tick cycle sets up the target pool (first time: ONE
//     node) and starts the target task of the next action on its first target (the pool holds one
//     node: while a target task runs, later actions fail to spawn, and the root ends as soon as the
//     target queue is empty, so only the first action plays); runs the target queue; screen flash
//     (level 0 every tick); ends when the target queue is empty.
//   Target (0x6B95C0) - node 0x4BC, draws THEN updates:
//     every tick: the "stare" layout 0x12377BC at the caster's effect anchor 0xF0 (followed until
//       counter 10, then frozen) moved 1/16 of the unit circle towards the caster's facing
//       (-sin / -cos of entity +0x0E), depth offset from the CASTER's size;
//     counter 4: engine screen fade task (0x5712C0);
//     counters 14..: the "stone" layout 0x123BC84 at the target's default effect position (taken
//       at counter 14), depth offset from the target's size;
//     counter 0: sound 0x1241598 at the caster's default position; counter 42: damage; ends at 49.
//   PartCallback (0x6B9770) - one object of either layout: record flag 0x40000 selects the rotation
//     builder 0x7015B0, else 0x701310; flag 0x200 = the offset stays in screen axes (the anchor
//     through the camera, the offset added unrotated, the rotation not composed with the camera),
//     else anchor + offset through the camera; scale by scale3DMatrix.
//     Blended vertex frames go to magic buffer + 0x4BC (0x3E8 bytes before arena A).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521270..0x25212C0 (flash level, caster entity, texture file, magic buffer
// base, context, root pool, packet cursor 0x252129C, target / root queues). The target pool
// (1 x 0x4BC), the blend buffer (+0x4BC) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace petrify127
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &FlashLevel() { return var<uint32_t>(0x2521270); }       // screen flash level (always 0)
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2521274); }          // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x252127C); }          // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521280); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252129C); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25212A0; }          // pool: magic buffer, 1 x 0x4BC
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6B9450;
	static const uint32_t ORIG_TargetTask = 0x6B95C0;
	static const uint32_t MODEL_Stare = 0x12377BC;       // prim-model layout data (0x1C8-byte layout)
	static const uint32_t MODEL_Stone = 0x123BC84;       // prim-model layout data (0x2D0-byte layout)
	static const void *const SOUND_Stare = (const void *)0x1241598;

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
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
	struct TargetNode // pool of 1 node of 0x4BC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		int16_t anchor[4]; // +0x10 caster's effect anchor 0xF0 (x, y, z; followed until counter 10)
		int16_t stone[4];  // +0x18 target's default effect position (taken at counter 14)
		uint8_t *entity;   // +0x20 target entity
		uint8_t stare[0x1C8]; // +0x24 prim-model layout (prim::Layout: data, frame, state)
		uint8_t rock[0x2D0];  // +0x1EC prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+ a word never written)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x4BC, "Petrify Stare nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Petrify Stare prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6B9770): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
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
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag127_petrify_stare_held.h"
#endif

namespace ff8fx
{
namespace petrify127
{
	// depth offset of a layout from an entity's size (prim header +0x18)
	static int32_t SizeDepth(const uint8_t *entity)
	{
		return (int32_t)(0xFFFFF800u - (uint32_t)(int32_t)*(const int16_t *)(entity + 0x26)) >> 5;
	}

	// ------------------------------------------------------------------
	// Target (0x6B95C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter < 10) GetEffectSpawnPosition(Caster(), 0xF0, 0, t->anchor);
		PrimArg arg;
		arg.pos[0] = (int16_t)((-ComputeSin(*(const int16_t *)(Caster() + 0xE)) >> 4) + t->anchor[0]);
		arg.pos[2] = (int16_t)((-ComputeCos(*(const int16_t *)(Caster() + 0xE)) >> 4) + t->anchor[2]);
		arg.pos[1] = t->anchor[1];
		// never written by the original (stack word), only loaded into the GTE V0 pad
		arg.pos[3] = 0;
		arg.depth = SizeDepth(Caster());
		arg.morph = TexBase() + 0x4BC;
		// 30 fps layer: see mag127_petrify_stare_held.inc
		FX_HELD(held_note_play((prim::Layout *)t->stare, &arg);)
		prim::play((prim::Layout *)t->stare, PartCallback, (int)&arg, 0);
		if (t->counter == 4) ScreenFadeTask(0, 1, 0, 0x80);
		if ((uint32_t)((int32_t)t->counter - 0xE) < 0x64)
		{
			if (t->counter == 0xE) GetDefaultEffectPosition(t->entity, t->stone);
			memcpy(arg.pos, t->stone, 8);
			arg.depth = SizeDepth(t->entity);
			arg.morph = TexBase() + 0x4BC;
			// 30 fps layer: see mag127_petrify_stare_held.inc
			FX_HELD(held_note_play((prim::Layout *)t->rock, &arg);)
			prim::play((prim::Layout *)t->rock, PartCallback, (int)&arg, 0);
		}
		if (t->counter == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			BdPlaySE3D(SOUND_Stare, 0x100, pos);
		}
		if (t->counter == 0x2A) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter > 0x30) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6B9450)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag127_petrify_stare_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x8A4;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x188A4;
			r->arena = 1;
		}
		FlashLevel() = 0;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x4BC, 1);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				if (t)
				{
					Memset32(&t->counter, 0, 0x12C);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					DecodeModelPrimLayout(MODEL_Stare, t->stare, 0x1C8);
					DecodeModelPrimLayout(MODEL_Stone, t->rock, 0x2D0);
					r->action++;
				}
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		SetScreenFlash(FlashLevel(), 0);
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag127_petrify_stare()
	{
		register_port(petrify127::ORIG_RootTask, (void *)petrify127::RootTask, "P127 RootTask", 127);
		register_port(petrify127::ORIG_TargetTask, (void *)petrify127::TargetTask, "P127 TargetTask", 127);
		// 30 fps layer: see mag127_petrify_stare_held.inc
		FX_HELD(register_mag127_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag127_petrify_stare_held.inc"
#endif
