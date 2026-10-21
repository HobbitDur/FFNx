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
// Effect 273: Mad Rush (command ability Mad Rush: kernel.bin command ability data entry 3 +0 = 273; MAG_273_*). No other
// kernel.bin entry uses effect 273.
//
// Structure (setup MAG_273 0x68D650 -> 0x68D680, not ported; file loader 0x68D660 = the texture file named at 0x1111220 ->
// [0x25084EC]; the setup keeps the cast context (0x25084F4) and the caster's entity (0x25084E8, never read again), queues the
// TIM, starts the root task in its 1-node pool 0x25084F8 and plays the camera animation 0x11110D0 (Battle_PlayCameraAnimation
// 0x5099A0: the engine's camera script, no module task writes the camera)):
//   RootTask (0x68D700) - alternates the packet arena (magic buffer + 0x1250 / + 0x11250 by tick parity, cursor 0x250850C);
//     on counter 1 sets up the pools once (play 3 x 0x310 at the start of the magic buffer, dust 0x40 x 0x18 at + 0x930)
//     and starts one play task per target of action 0 (target k waits 2 k ticks: counter -2 k): the target's bone 0xF1
//     (GetEffectSpawnPosition 0x502170, flag 0) with y = the target's +0x24, the layout 0x110E778 (0x2F4 bytes), sound
//     0x11110CC each. Then the play queue and the dust queue; ends when both are empty.
//   PlayTask (0x68D8B0) - waits while its counter is negative; then plays the layout (prim player 0x701970, callback
//     0x68DA10) with a stack block {the anchor, the morph buffer magic buffer + 0xF30}; counters 0x14..0x1D: 1 +
//     (sin((c - 0x14) * 0x800 / 10) >> 11) dust tasks (CRT rand 0x55CBD2, 5 draws each: scale 0x400 / 0x800, x / z within
//     +-0x100 of the anchor, y up to 0x3FF above it, rise speed -16..-79); when the layout has finished: the action result
//     of its target record (ApplyActionResultToTarget 0x506690) and the task ends.
//   PrimCallback (0x68DA10, Bad Breath's 0x6B3EE0: the same code) - one object of the layout: vertex frame / blend
//     (0x701390), rotation (0x701310), the record's offset + the block's anchor composed with the camera, record scale
//     (scale3DMatrix), fade, the engine's prim-model draw 0x572200 (header +0x18 not written: the scratch word).
//   DustTask (0x68DBE0) - sprite sequence 0x110E5DC through the shadow-rotation camera (scale, -scale / 4), frame = counter;
//     13 frames; rises by its speed after each draw but the last.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA WRITES: none (the targets' entities are only read: bone 0xF1 through GetEffectSpawnPosition, +0x24; the
// camera matrix 0x1D97778 is only read); the damage is the engine's ApplyActionResultToTarget.
// Module globals: 0x25084E8..0x2508540 (caster entity 0x25084E8, texture file 0x25084EC, magic buffer base 0x25084F0, context
// 0x25084F4, root pool 0x25084F8 (1 x 0x14), packet cursor 0x250850C, dust queue 0x2508510, play queue 0x2508520, root queue
// 0x2508530). The pools, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN WORDS: the callback's prim-model header +0x18 (the Field_Alloc scratch word under it, read by 0x572200 as the
// depth offset: the port reads the same scratch word); its matrix pad +0x12 (GTE R33 high half only). The anchor's 4th word is
// never read (the callback uses x, y, z only).

#include "mag_common.h"

namespace ff8fx
{
namespace madrush273
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x25084F4); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x25084F0); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x250850C); }
	inline TaskQueue *QDust() { return (TaskQueue *)0x2508510; }              // pool: magic buffer + 0x930, 0x40 x 0x18
	inline TaskQueue *QPlay() { return (TaskQueue *)0x2508520; }              // pool: magic buffer, 3 x 0x310
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_RootTask = 0x68D700;
	static const uint32_t ORIG_PlayTask = 0x68D8B0;
	static const uint32_t ORIG_DustTask = 0x68DBE0;
	static const uint32_t MODEL_LAYOUT = 0x110E778;   // prim-model layout data
	static const uint32_t LAYOUT_BYTES = 0x2F4;
	static const uint32_t PLAY_BYTES = 0x310;         // play node
	static const uint32_t DUST_POOL = 0x930;          // magic buffer: dust pool
	static const uint32_t MORPH = 0xF30;              // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1250;           // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11250;
	static const uint32_t SEQ_DUST = 0x110E5DC;       // sprite sequence
	static const int DUST_FRAMES = 0xD;
	static const void *const SOUND = (const void *)0x11110CC;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x25084F8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;     // +0x0E
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct PlayNode // pool of 3 nodes of 0x310 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C negative: ticks left to wait
		int16_t index;     // +0x0E the target record of action 0
		uint8_t *entity;   // +0x10 the target's battle entity
		int16_t anchor[4]; // +0x14 x, y (the target's +0x24), z: its bone 0xF1; the 4th word of 0x502170
		uint8_t layout[LAYOUT_BYTES]; // +0x1C
	};
	struct DustNode // pool of 0x40 nodes of 0x18 bytes (magic buffer + 0x930)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame
		int16_t scale;     // +0x0E 0x400 / 0x800
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16 added to y after each draw but the last
	};
	// the prim-player callback's parameter block (a stack block of the play task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the anchor (4th word: GTE VZ0 high half only)
		uint32_t morph;    // +0x08 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == PLAY_BYTES && sizeof(DustNode) == 0x18, "Mad Rush nodes");
	static_assert(sizeof(PrimArg) == 0xC, "Mad Rush prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x68DA10, the code of Bad Breath's 0x6B3EE0): one object of the layout. Vertex frame (or blend
	// into the block's morph buffer), rotation from the record's angles, the record's offset + the block's anchor composed
	// with the camera, scaled (scale3DMatrix), drawn by 0x572200.
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
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2000;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20C0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		// +0x18 (depth offset) is not written: the scratch word under the header
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Dust (0x68DBE0): sequence 0x110E5DC through the shadow-rotation camera, 13 frames, rising
	// ------------------------------------------------------------------
	static void DustDraw(const DustNode *d)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(d->pos, d->scale, -((int32_t)d->scale >> 2));
		*(uint32_t *)h = SEQ_DUST;
		*(int16_t *)(h + 4) = d->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag273_mad_rush_held.h"
#endif

namespace ff8fx
{
namespace madrush273
{
	static uint32_t __cdecl DustTask(TaskNode *n)
	{
		DustNode *d = (DustNode *)n;
		// 30 fps layer: see mag273_mad_rush_held.inc
		FX_HELD(held_note_dust(d);)
		DustDraw(d);
		d->counter++;
		if (d->counter >= DUST_FRAMES) return TASK_END;
		d->pos[1] = (int16_t)(d->pos[1] + d->vy);
		return 0;
	}

	// ------------------------------------------------------------------
	// Play (0x68D8B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		PlayNode *p = (PlayNode *)n;
		if (p->counter < 0)
		{
			p->counter++;
			return 0;
		}
		PrimArg arg;
		memcpy(arg.pos, p->anchor, sizeof(arg.pos));
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag273_mad_rush_held.inc
		FX_HELD(held_note_play(p);)
		const int left = prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0);
		const int32_t c = p->counter - 0x14;
		if ((uint32_t)c < 10u)
		{
			// 1 + (sin(c * 0x800 / 10) >> 11) dust tasks
			const int32_t s = ComputeSin((int32_t)((uint32_t)shl32(c, 11) / 10u)) >> 11;
			for (int32_t k = s + 1; k; k--)
			{
				DustNode *d = (DustNode *)AddTaskToQueue(QDust(), ORIG_DustTask);
				if (!d) continue;
				d->counter = 0;
				d->scale = (int16_t)((CrtRand() & 0x400) + 0x400);
				d->pos[0] = (int16_t)((CrtRand() & 0x1FF) + p->anchor[0] - 0x100);
				d->pos[2] = (int16_t)((CrtRand() & 0x1FF) + p->anchor[2] - 0x100);
				d->pos[1] = (int16_t)(p->anchor[1] - (CrtRand() & 0x3FF));
				d->vy = (int16_t)(-0x10 - (CrtRand() & 0x3F));
			}
		}
		if (left == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets + TARGET_STRIDE * p->index);
			return TASK_END;
		}
		p->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x68D700)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag273_mad_rush_held.inc
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
				InitTaskQueuePool(QPlay(), (void *)TexBase(), PLAY_BYTES, 3);
				InitTaskQueuePool(QDust(), (void *)(TexBase() + DUST_POOL), 0x18, 0x40);
			}
			for (int32_t k = 0; k < (int32_t)Ctx()->actions[0].target_count; k++)
			{
				PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), ORIG_PlayTask);
				if (!p) continue;
				Memset32(&p->counter, 0, 0xC1);
				p->index = (int16_t)k;
				p->counter = (int16_t)(-k * 2);
				p->entity = Entity(Ctx()->actions[0].targets[TARGET_STRIDE * k]);
				GetEffectSpawnPosition(p->entity, 0xF1, 0, p->anchor);
				p->anchor[1] = *(const int16_t *)(p->entity + 0x24);
				DecodeModelPrimLayout(MODEL_LAYOUT, p->layout, LAYOUT_BYTES);
				BdPlaySE(SOUND, 0, 0x80);
			}
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QPlay());
			b = ExecuteTaskQueue(QDust());
		}
		else a = b = (int)n;
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag273_mad_rush()
	{
		register_port(madrush273::ORIG_RootTask, (void *)madrush273::RootTask, "273 RootTask", 273);
		register_port(madrush273::ORIG_PlayTask, (void *)madrush273::PlayTask, "273 PlayTask", 273);
		register_port(madrush273::ORIG_DustTask, (void *)madrush273::DustTask, "273 DustTask", 273);
		// 30 fps layer: see mag273_mad_rush_held.inc
		FX_HELD(register_mag273_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag273_mad_rush_held.inc"
#endif
