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

// Effect 183: Defend (command ability Defend: kernel.bin command ability data entry 11 +0 = 183;
// MAG_183_*). No other kernel.bin entry uses effect 183. Two shields appear in front of the
// caster: a large one shrinking onto it, then a smaller one flickering through a colour table.
//
// Structure (setup MAG_183_DEFEND 0x5C9A50, file loader 0x5C9A30 = the texture file named at
// 0xD39364 (mag182.tim, kept in 0x22F4218); the setup keeps the cast context 0x22F4210, the
// first target's slot 0x22F33C8 and the caster's slot 0x22F4214, sets up the root queue 0x22F33E0
// (1 node) and the effect queue 0x22F33F0 (0x64 nodes of 0x24 bytes) with the director and
// queues the TIM; no camera animation):
//   RootTask (0x5C9F80) - alternates the module packet arena (0x22FC21C on odd counters,
//     0x22F421C on even ones, 0x8000 each; cursor 0x230421C), runs the effect queue, ends when it
//     is empty.
//   Director (0x5C9AE0) - one node, counter 0..0x23 (nothing in the draw-only mode): 0 = the
//     caster's effect point (GetEffectSpawnPosition, bone selector 0xF1) into its node +0x10;
//     1 = ShieldA (scale 0x2000, step 0x200) + sound 0xD39304 at the caster's default effect
//     position; 6 = ShieldB (scale 0x700); 0x1E = the action result (action 0's targets).
//     Both shields face the caster's direction (entity +0x0E).
//   ShieldA (0x5C9C40) - counters 0..0xF: model 0xD348C4 600 units in front of the point,
//     drawn through MAG_066's scroll renderer 0x650720 (header 0x90 bytes: v scroll -10 *
//     counter, colour 0x808080, fade (4 - counter) << 10 below 4), scale - 0x200 per tick.
//   ShieldB (0x5C9DD0) - counters 0..0x13: model 0xD34CE4 200 units in front, v scroll
//     -6 * counter, colour = table 0xD3933C[counter % 10], fade in (4 - counter) << 10 below 4
//     and out (counter - 0x10) << 10 from 0x10.
// The shields draw THEN update (in the draw-only mode they only draw). The module writes no
// entity and no camera word.
// Module globals: 0x22F33C8..0x2304220 (target slot, root pool / queue, effect queue / pool,
// context 0x22F4210, caster slot 0x22F4214, texture file 0x22F4218, the two packet arenas,
// packet cursor 0x230421C).

#include "mag_common.h"

namespace ff8fx
{
namespace defend183
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x22F4210); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x22F4214); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x230421C); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x22F33F0; }
	static const uint32_t ARENA0 = 0x22F421C, ARENA1 = 0x22FC21C, ARENA_SIZE = 0x8000;

	static const uint32_t ORIG_RootTask = 0x5C9F80;
	static const uint32_t ORIG_Director = 0x5C9AE0;
	static const uint32_t ORIG_ShieldA = 0x5C9C40;
	static const uint32_t ORIG_ShieldB = 0x5C9DD0;
	static const uint32_t MODEL_ShieldA = 0xD348C4, MODEL_ShieldB = 0xD34CE4;
	static const uint32_t SHIELD_COLOURS = 0xD3933C;                          // 10 colour words (ShieldB)
	static const void *const SOUND_Defend = (const void *)0xD39304;

	// ---- engine functions (original addresses)
	// GetEffectSpawnPosition (0x502170): position of an entity's effect bone
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	// matrixMultiplyVector (0x56C4F0): SVECTOR out = rotation of m * SVECTOR in (in may be out)
	inline void MatrixMultiplySVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	// MAG_066_RenderScrollPrimModel (0x650720): prim model with a v texture scroll (header 0x90 bytes)
	inline uint32_t ScrollRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x650720)(h, ot, mode, cursor); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C
		int16_t pad;
	};
	struct EffectNode // pool of 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C
		int16_t pad0E;
		int16_t pos[4];      // +0x10 the caster's effect point (director: as GetEffectSpawnPosition wrote it)
		int16_t angle;       // +0x18 shields: the caster's facing (entity +0x0E)
		int16_t pad1A;
		int16_t scale;       // +0x1C shields
		int16_t step;        // +0x1E ShieldA: scale decrease per tick
		uint8_t pad20[4];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "Defend nodes");

	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }

	// ------------------------------------------------------------------
	// Shield draw (both shields): the model `front` units ahead of the node's point along the
	// facing, scaled, composed with the camera, drawn through the scroll renderer
	// ------------------------------------------------------------------
	struct ShieldDraw
	{
		uint32_t model;
		int16_t front;       // offset along the facing (rotated z)
		int32_t scale;
		int32_t scroll;      // header +0x14
		bool fade_on;        // header +0x20 = 0xF3 (else 0x33)
		int32_t fade;        // header +0x0C (only when fade_on)
		uint32_t colour;     // header +0x24
		bool b;              // ShieldB's header (+0x18 = 0x40), else ShieldA's (+0x1C = 0x40)
	};

	static void DrawShield(const EffectNode *t, const ShieldDraw &d)
	{
		int16_t ang[4]; // (4th word never written)
		ang[0] = 0;
		ang[1] = t->angle;
		ang[2] = 0;
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only)
		ComposeZYXRotationMatrix(ang, &m);
		int16_t v[4]; // (4th word never written; the rotation reads / writes 3)
		v[0] = 0;
		v[1] = 0;
		v[2] = d.front;
		MatrixMultiplySVector(&m, v, v);
		m.t[0] = t->pos[0] + v[0];
		m.t[1] = t->pos[1];
		m.t[2] = t->pos[2] + v[2];
		int32_t sv[3];
		sv[0] = d.scale;
		sv[1] = d.scale;
		sv[2] = d.scale;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x90);
		if (d.b)
		{
			*(int16_t *)(h + 0x18) = 0x40;
			*(int16_t *)(h + 0x1C) = 0x40;
			*(uint32_t *)(h + 8) = 0;
			*(int16_t *)(h + 0x1A) = 0;
			*(uint32_t *)(h + 0x10) = 0;
			*(uint32_t *)h = d.model;
			*(uint32_t *)(h + 0x20) = 0x33;
			*(int16_t *)(h + 0x1E) = 0x80;
			*(int32_t *)(h + 0x14) = d.scroll;
			if (d.fade_on)
			{
				*(int32_t *)(h + 0xC) = d.fade;
				*(uint32_t *)(h + 0x20) = 0xF3;
			}
		}
		else
		{
			*(uint32_t *)(h + 8) = 0;
			*(int16_t *)(h + 0x18) = 0;
			*(int16_t *)(h + 0x1A) = 0;
			*(uint32_t *)(h + 0x10) = 0;
			*(uint32_t *)h = d.model;
			*(uint32_t *)(h + 0x20) = 0x33;
			*(int16_t *)(h + 0x1C) = 0x40;
			*(int16_t *)(h + 0x1E) = 0x80;
			*(int32_t *)(h + 0x14) = d.scroll;
			if (d.fade_on)
			{
				*(uint32_t *)(h + 0x20) = 0xF3;
				*(int32_t *)(h + 0xC) = d.fade;
			}
		}
		*(uint32_t *)(h + 0x24) = d.colour;
		PacketCursor() = ScrollRender(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x90);
	}

	// ShieldA's draw at a counter: fade (4 - counter) << 10 below 4, v scroll -10 * counter
	static ShieldDraw ShieldAState(int16_t counter, int16_t scale)
	{
		ShieldDraw d;
		d.model = MODEL_ShieldA;
		d.front = -600;
		d.scale = scale;
		d.scroll = mul32(counter, -10);
		d.fade_on = counter < 4;
		d.fade = d.fade_on ? shl32(4 - counter, 10) : 0;
		d.colour = 0x808080;
		d.b = false;
		return d;
	}

	// ShieldB's draw at a counter: fade in below 4 / out from 0x10 ((4 - c) / (c - 0x10) << 10),
	// v scroll -6 * counter, colour table entry counter % 10
	static ShieldDraw ShieldBState(int16_t counter, int16_t scale)
	{
		ShieldDraw d;
		d.model = MODEL_ShieldB;
		d.front = 200;
		d.scale = scale;
		d.scroll = mul32(counter, -6);
		d.fade_on = counter < 4 || counter >= 0x10;
		d.fade = counter < 4 ? shl32(4 - counter, 10) : counter >= 0x10 ? shl32(counter - 0x10, 10) : 0;
		d.colour = var<uint32_t>(SHIELD_COLOURS + (uint32_t)((int32_t)counter % 10) * 4);
		d.b = true;
		return d;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag183_defend_held.h"
#endif

namespace ff8fx
{
namespace defend183
{
	// ------------------------------------------------------------------
	// ShieldA (0x5C9C40): counters 0..0xF, shrinking
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShieldATask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag183_defend_held.inc
		FX_HELD(held_note_shield(ORIG_ShieldA, t);)
		DrawShield(t, ShieldAState(t->counter, t->scale));
		if (DrawOnly()) return 0;
		t->scale = (int16_t)(t->scale - t->step);
		t->counter++;
		return t->counter >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// ShieldB (0x5C9DD0): counters 0..0x13, colour cycling
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShieldBTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag183_defend_held.inc
		FX_HELD(held_note_shield(ORIG_ShieldB, t);)
		DrawShield(t, ShieldBState(t->counter, t->scale));
		if (DrawOnly()) return 0;
		t->counter++;
		return t->counter >= 0x14 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5C9AE0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)n;
		if (t->counter == 0) GetEffectSpawnPosition(Caster(), 0xF1, 0, t->pos);
		if (t->counter == 1)
		{
			EffectNode *s = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_ShieldA);
			*(uint32_t *)&s->pos[0] = *(const uint32_t *)&t->pos[0];
			*(uint32_t *)&s->pos[2] = *(const uint32_t *)&t->pos[2];
			s->counter = 0;
			s->scale = 0x2000;
			s->step = 0x200;
			s->angle = *(const int16_t *)(Caster() + 0xE);
		}
		if (t->counter == 6)
		{
			EffectNode *s = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_ShieldB);
			*(uint32_t *)&s->pos[0] = *(const uint32_t *)&t->pos[0];
			*(uint32_t *)&s->pos[2] = *(const uint32_t *)&t->pos[2];
			s->counter = 0;
			s->scale = 0x700;
			s->angle = *(const int16_t *)(Caster() + 0xE);
		}
		if (t->counter == 0x1E) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == 1)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			BdPlaySE3D(SOUND_Defend, 0, pos);
		}
		t->counter++;
		return t->counter > 0x23 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x5C9F80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		PacketCursor() = ARENA1;
		if (!(r->counter & 1)) PacketCursor() = ARENA0;
		// 30 fps layer: see mag183_defend_held.inc
		FX_HELD(held_note_root();)
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag183_defend()
	{
		register_port(defend183::ORIG_RootTask, (void *)defend183::RootTask, "D183 RootTask", 183);
		register_port(defend183::ORIG_Director, (void *)defend183::DirectorTask, "D183 Director", 183);
		register_port(defend183::ORIG_ShieldA, (void *)defend183::ShieldATask, "D183 ShieldA", 183);
		register_port(defend183::ORIG_ShieldB, (void *)defend183::ShieldBTask, "D183 ShieldB", 183);
		// 30 fps layer: see mag183_defend_held.inc
		FX_HELD(register_mag183_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag183_defend_held.inc"
#endif
