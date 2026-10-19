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

// Effect 293: Eye Drops (battle item Eye Drops: kernel.bin battle item +4 = 293; MAG_293_*). No other
// kernel.bin entry uses effect 293. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_293 0x674870 -> 0x6748A0, file loader 0x674880 = the texture file named at 0x1065134;
// the setup keeps the cast context (0x2507A9C) and the caster's entity (0x2507AA0, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507AC8):
//   RootTask (0x674910) - alternates the packet arena (magic buffer + 0x4EC / + 0x104EC by tick parity,
//     cursor 0x2507ADC); on counter 1 sets up the play pool (1 x 0x4E4 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its effect anchor 0xF0 (GetEffectSpawnPosition 0x502170);
//     then the play queue (sound 0x1065130 on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x674A30) - draws THEN advances layout 0x10637AC (0x4D0 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x674AA0) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x4E4, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     flag 0x200: the anchor through the camera + the offset; else the offset + the anchor
//     through the camera, composed with it;
//     + the camera translation; scale (flag 0x100: 3x3 product, else scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507A98..0x2507AE4 (texture file 0x2507A98, context 0x2507A9C, caster entity 0x2507AA0, play
// queue 0x2507AA8, root queue 0x2507AB8, root pool 0x2507AC8 (1 x 0x14), packet cursor 0x2507ADC, magic buffer
// base 0x2507AE0). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).

#include "mag_common.h"

namespace ff8fx
{
namespace eyedrops293
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507A9C); }
	inline TaskQueue *QPlay() { return (TaskQueue *)0x2507AA8; }              // pool: magic buffer, 1 x 0x4E4
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507ADC); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507AE0); }          // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_RootTask = 0x674910;
	static const uint32_t ORIG_PlayTask = 0x674A30;
	static const uint32_t MODEL = 0x10637AC;           // prim-model layout data
	static const int32_t LAYOUT_BYTES = 0x4D0;
	static const uint32_t NODE_BYTES = 0x4E4;       // play node; the morph buffer follows it
	static const void *const SOUND = (const void *)0x1065130;
	static const uint32_t ARENA_A = 0x4EC;         // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x104EC;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_RotationFromAnglesYXZ
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_RenderPrimModel: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }       // MatrixMultiply3x3: a = a * b
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone`
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507AC8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F play pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct PlayNode // pool of 1 node of 0x4E4 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t anchor[4]; // +0x0C anchor (counter 1; the 4th word is the pool's bytes)
		uint8_t layout[LAYOUT_BYTES]; // +0x14 prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the play task)
	struct PrimArg
	{
		int16_t anchor[4]; // +0x00 the node's anchor (8 bytes)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == NODE_BYTES, "Eye Drops nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Eye Drops prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x674AA0): one object of the layout
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
		if (r->flags & 0x200)
		{
			// the anchor through the camera (its 4th word goes to the GTE V0 pad only) + the record's offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)r->pos[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)r->pos[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)r->pos[2]);
		}
		else
		{
			// the record's offset + the anchor through the camera (4th word never written: only
			// loaded into the GTE V0 pad)
			const int16_t off[4] = { (int16_t)(r->pos[0] + arg->anchor[0]), (int16_t)(r->pos[1] + arg->anchor[1]), (int16_t)(r->pos[2] + arg->anchor[2]), 0 };
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
}
}

#ifdef FF8_FX_HELD
#include "mag293_eye_drops_held.h"
#endif

namespace ff8fx
{
namespace eyedrops293
{
	// ------------------------------------------------------------------
	// Play (0x674A30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		PlayNode *p = (PlayNode *)n;
		PrimArg arg;
		memcpy(arg.anchor, p->anchor, sizeof(arg.anchor));
		arg.depth = -0x80;
		arg.morph = TexBase() + NODE_BYTES;
		// 30 fps layer: see mag293_eye_drops_held.inc
		FX_HELD(held_note_play((prim::Layout *)p->layout, p->anchor);)
		if (prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0) == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x674910)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag293_eye_drops_held.inc
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
			InitTaskQueuePool(QPlay(), (void *)TexBase(), NODE_BYTES, 1);
			PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), ORIG_PlayTask);
			uint8_t *e = Entity(Ctx()->actions[0].targets[0]);
			GetEffectSpawnPosition(e, 0xF0, 0, p->anchor);
			DecodeModelPrimLayout(MODEL, p->layout, LAYOUT_BYTES);
		}
		int a;
		if (r->started)
		{
			a = ExecuteTaskQueue(QPlay());
			if (r->counter == 1) BdPlaySE(SOUND, 0, 0x80);
		}
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag293_eye_drops()
	{
		register_port(eyedrops293::ORIG_RootTask, (void *)eyedrops293::RootTask, "293 RootTask", 293);
		register_port(eyedrops293::ORIG_PlayTask, (void *)eyedrops293::PlayTask, "293 PlayTask", 293);
		// 30 fps layer: see mag293_eye_drops_held.inc
		FX_HELD(register_mag293_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag293_eye_drops_held.inc"
#endif
