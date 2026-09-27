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

// Effect 147: Regen (spell, MAG_147_*).
//
// Structure (setup MAG_147_REGEN 0x614290, file loader MAG_147_REGEN_FL 0x614270 = the texture
// file named at 0xDD05E8; the setup keeps the cast context and the first target, sets up the root
// queue 0x2476678 (one 0x10-byte node in 0x2476668) and the effect queue 0x2476688 (0x64 nodes of
// 0x24 bytes at 0x2476698), queues the root and the first action's director, starts camera
// animation 0xDCE760 and queues the TIM unless the cast context's flag bit 0 is set):
//   Root (0x614D10, au_re_BdlinkTask_28, same code as Meltdown's au_re_BdlinkTask_27 0x614230) -
//     alternates the packet arena (0x2479BB8 / 0x2485BB8), runs the effect queue, ends when it is
//     empty.
//   Director (0x614350), one per action; nothing in the draw-only mode (battle_to_update_flags
//     0x1D96A9C & 0x201): tick 0 takes the target's effect anchor, its height, a size scale and the
//     height of its bone 0xF0 (the last two never read again); tick 1 spawns the two envelopes and
//     plays the sound; tick 2 spawns 8 leaves, 1/8 turn apart from a random angle, flying outwards
//     (radius from the target's size) and up, and clears their trails; tick 0x1C applies the action
//     result and starts the next action's director; ends after tick 0x30.
//   Envelope (0x614720), two per action: prim model 0xDCFC64 at the target's feet (y = the target's
//     height word), turning around the vertical axis at a random speed, its xz scale shrinking and
//     its y scale growing faster and faster (y speed * 13/12 each tick), fading in over ticks 0..3
//     and out from 14; ends after 30 draws. Updates skipped in the draw-only mode.
//   Leaf (0x6148E0), 8 per action: waits its +0x0E ticks (a word the director never writes: what
//     the pool node held, e.g. an old envelope's kind 1), then for 40 ticks: until tick 14 draws the
//     leaf sprite sequence 0xDCEFD0 (frame 0, then 1.. from tick 8) and its ground shadow (frame 1,
//     same sequence, through a matrix at (x, 0, z) scaled 2 * size, grey 0x70 - 6 * tick); moves by
//     its velocity (x / z decelerate by 1/4, the rise speeds up by 1/8 each tick); over ticks 2..11
//     starts one trail particle per tick in its 16-slot trail (module globals 0x24774B8 + 0x140 *
//     (action * 8 + leaf)): each drawn with the same sequence (frame = its age while below 10, else
//     the frame the previous trail draw left in the header), rising and shrinking by 0x10 while the
//     leaf is younger than 14, then drifting by its accelerating velocity and shrinking by 1/16;
//     gone at age 26. Every draw comes before the update; the draw-only mode skips the updates.
// Module globals: 0x2476660..0x2491BBC (first target slot 0x2476660, root pool / queue, effect queue
// and pool, cast context 0x24774A8, caster 0x24774AC, texture file 0x24774B0, trails 0x24774B8,
// packet arenas 0x2479BB8 / 0x2485BB8, packet cursor 0x2491BB8).

#include "mag_common.h"

namespace ff8fx
{
namespace regen147
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x24774A8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2491BB8); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x2476688; }   // pool 0x2476698, 0x64 x 0x24
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_Root = 0x614D10;
	static const uint32_t ORIG_Director = 0x614350;
	static const uint32_t ORIG_Envelope = 0x614720;
	static const uint32_t ORIG_Leaf = 0x6148E0;
	static const uint32_t MODEL_Envelope = 0xDCFC64;    // prim model
	static const uint32_t SEQ_Leaf = 0xDCEFD0;          // sprite sequence (leaf, shadow, trail)
	static const void *const SOUND_Regen = (const void *)0xDD05CC;
	static const uint32_t TRAILS = 0x24774B8;           // 0x140 bytes (16 x 0x14) per action * 8 + leaf
	static const uint32_t ARENA_EVEN = 0x2479BB8, ARENA_ODD = 0x2485BB8;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool 0x2476668, 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C arena parity
		uint16_t pad0E;
	};
	struct DirectorNode // effect pool, 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E
		int16_t pos[4];    // +0x10 effect anchor x, y, z; +0x16 = the target's height word (+0x24)
		int16_t scale;     // +0x18 min((size * 5600 + 5000) >> 12, 2200) * 4096 / 1000 (never read)
		int16_t rise;      // +0x1A (bone 0xF0 height - 900) / 24 (never read)
		uint8_t pad1C[4];
		int16_t slot;      // +0x20 target slot
		uint16_t pad22;
	};
	struct EnvelopeNode // effect pool, 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t kind;      // +0x0E 0 / 1 (fade range, growth speeds)
		int16_t pos[4];    // +0x10 x, height word, z, (+0x16 the director's +0x16)
		int16_t angle;     // +0x18 around the vertical axis
		int16_t spin;      // +0x1A angle step (30..89)
		int16_t sxz;       // +0x1C x / z scale (4.12)
		int16_t dsxz;      // +0x1E x / z scale step (-0x100 / -0xDB)
		int16_t sy;        // +0x20 y scale
		int16_t dsy;       // +0x22 y scale step (0x66 / 0x57), * 13 / 12 each tick
	};
	struct LeafNode // effect pool, 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E ticks to wait (not written by the director)
		int16_t pos[3];    // +0x10 x, y, z
		int16_t action;    // +0x16
		int16_t vel[3];    // +0x18
		int16_t leaf;      // +0x1E 0..7
		int16_t size;      // +0x20 sprite scale
		uint16_t pad22;
	};
	struct TrailPart // 0x14 bytes, 16 per leaf in the module globals
	{
		int16_t counter;   // +0x00 age, < 0 = free
		int16_t size;      // +0x02
		int16_t pos[3];    // +0x04
		int16_t rise;      // +0x0A y step while the leaf is younger than 14
		int16_t vel[3];    // +0x0C drift afterwards (+1/64 each tick)
		int16_t unused;    // +0x12 random 12..23, never read
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x24 && sizeof(EnvelopeNode) == 0x24 && sizeof(LeafNode) == 0x24, "Regen nodes");
	static_assert(sizeof(TrailPart) == 0x14, "Regen trail particle");

	static inline TrailPart *TrailOf(int32_t action, int32_t leaf)
	{
		return (TrailPart *)(TRAILS + (uint32_t)mul32(leaf + action * 8, 0x140));
	}

	// x / 12 (0x2AAAAAAB, sar 1), x / 24 (0x2AAAAAAB, sar 2), x / 1000 (0x10624DD3, sar 6)
	static inline int32_t DivMagic(int32_t x, int32_t magic, int sh)
	{
		int32_t hi = (int32_t)(((int64_t)x * magic) >> 32) >> sh;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// ------------------------------------------------------------------
	// Envelope (0x614720)
	// ------------------------------------------------------------------
	// fade (prim header +0x0C, toward black): kind 0 0x1000 - 0x100 * tick over 0..3, then 0, then
	// 0x100 * (tick - 14) from 14; kind 1 the same from 0x800 in steps of 0x80
	static int32_t EnvelopeFade(int16_t counter, int16_t kind)
	{
		const int32_t base = kind ? 0x800 : 0;
		const int32_t step = (kind ? 0x800 : 0x1000) / 16;
		if (counter < 4) return 0x1000 - mul32(step, counter);
		if (counter >= 0xE) return mul32(step, counter - 0xE) + base;
		return base;
	}

	static void EnvelopeDraw(const EnvelopeNode *e, int32_t fade)
	{
		int16_t angles[4] = { 0, e->angle, 0, 0 };
		Mat4x3 m;
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[0] = e->pos[0];
		m.t[1] = e->pos[1];
		m.t[2] = e->pos[2];
		int32_t scale[3] = { e->sxz, e->sy, e->sxz };
		Scale3DMatrix(&m, scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)h = MODEL_Envelope;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void EnvelopeUpdate(EnvelopeNode *e)
	{
		e->sxz = (int16_t)(e->sxz + e->dsxz);
		e->angle = (int16_t)(e->angle + e->spin);
		const int32_t d = e->dsy;
		e->sy = (int16_t)(e->sy + d);
		e->dsy = (int16_t)(DivMagic(d, 0x2AAAAAAB, 1) + d);
		e->counter++;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag147_regen_held.h"
#endif

namespace ff8fx
{
namespace regen147
{
	static uint32_t __cdecl EnvelopeTask(TaskNode *n)
	{
		EnvelopeNode *e = (EnvelopeNode *)n;
		// 30 fps layer: see mag147_regen_held.inc
		FX_HELD(held_note_envelope(e);)
		EnvelopeDraw(e, EnvelopeFade(e->counter, e->kind));
		if (DrawOnly()) return 0;
		EnvelopeUpdate(e);
		return e->counter >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Leaf (0x6148E0)
	// ------------------------------------------------------------------
	// grey of the shadow: 0x70 - 6 * tick (bytes)
	static int32_t LeafShade(int16_t counter) { return (uint8_t)(0x70 - (uint8_t)((uint8_t)counter * 6)); }

	// the leaf and its shadow (until tick 14); returns the sprite header, freed but used again by
	// the trail draws (fields they do not set keep what the leaf draws left)
	static uint8_t *LeafDraw(const LeafNode *p, int32_t shade)
	{
		TransformCameraByShadowRotation(p->pos, p->size, -(p->size >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Leaf;
		if (p->counter < 0xE)
		{
			*(int16_t *)(h + 4) = p->counter >= 8 ? (int16_t)(p->counter - 8) : 0;
			*(uint16_t *)(h + 0x24) = 0;
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
			// the shadow: frame 1 through a matrix on the ground (y = 0) scaled by 2 * size
			int16_t angles[4] = { 0x400, 0, 0, 0 };
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			m.t[0] = p->pos[0];
			const int32_t s = p->size * 2;
			int32_t scale[3] = { s, s, s };
			m.t[1] = 0;
			m.t[2] = p->pos[2];
			Scale3DMatrix(&m, scale);
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			*(int16_t *)(h + 4) = 1;
			h[0x1E] = (uint8_t)shade;
			h[0x1D] = (uint8_t)shade;
			h[0x1C] = (uint8_t)shade;
			*(uint16_t *)(h + 0x24) = 4;
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		}
		FieldFree(0xB4);
		*(uint16_t *)(h + 0x24) = 0;
		*(int16_t *)(h + 4) = 8;
		return h;
	}

	// one trail particle (header of LeafDraw: frame = its age while below 10, else the last frame set)
	static void TrailDraw(uint8_t *h, const TrailPart *t)
	{
		if (t->counter < 0xA) *(int16_t *)(h + 4) = t->counter;
		TransformCameraByShadowRotation(t->pos, t->size, -(t->size >> 3));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	static void TrailUpdate(TrailPart *t, int16_t leaf_counter)
	{
		t->counter++;
		if (t->counter >= 0x1A)
		{
			t->counter = -1;
			return;
		}
		if (leaf_counter < 0xE)
		{
			t->size = (int16_t)(t->size - 0x10);
			t->pos[1] = (int16_t)(t->pos[1] + t->rise);
			return;
		}
		t->size = (int16_t)(t->size - (int16_t)(t->size >> 4));
		t->pos[0] = (int16_t)(t->pos[0] + t->vel[0]);
		t->pos[1] = (int16_t)(t->pos[1] + t->vel[1]);
		t->pos[2] = (int16_t)(t->pos[2] + t->vel[2]);
		t->vel[0] = (int16_t)(t->vel[0] + (int16_t)(t->vel[0] >> 6));
		t->vel[1] = (int16_t)(t->vel[1] + (int16_t)(t->vel[1] >> 6));
		t->vel[2] = (int16_t)(t->vel[2] + (int16_t)(t->vel[2] >> 6));
	}

	static void LeafMove(LeafNode *p)
	{
		const int16_t vx = p->vel[0], vy = p->vel[1], vz = p->vel[2];
		p->pos[0] = (int16_t)(p->pos[0] + vx);
		p->pos[1] = (int16_t)(p->pos[1] + vy);
		p->pos[2] = (int16_t)(p->pos[2] + vz);
		p->vel[0] = (int16_t)(vx - (int16_t)(vx >> 2));
		p->vel[1] = (int16_t)(vy + (int16_t)(vy >> 3));
		p->vel[2] = (int16_t)(vz - (int16_t)(vz >> 2));
	}

	static uint32_t __cdecl LeafTask(TaskNode *n)
	{
		LeafNode *p = (LeafNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag147_regen_held.inc
		FX_HELD(held_note_leaf(p);)
		uint8_t *h = LeafDraw(p, LeafShade(p->counter));
		TrailPart *trail = TrailOf(p->action, p->leaf);
		for (int k = 0; k < 16; k++)
		{
			TrailPart *t = &trail[k];
			if (t->counter < 0) continue;
			TrailDraw(h, t);
			if (DrawOnly()) continue;
			TrailUpdate(t, p->counter);
		}
		if (DrawOnly()) return 0;
		// one new trail particle per tick over 2..11 in the first free slot
		if (p->counter >= 2 && p->counter <= 0xB)
		{
			int k = 0;
			while (k < 16 && trail[k].counter >= 0) k++;
			if (k < 16)
			{
				TrailPart *t = &trail[k];
				t->counter = 1;
				const int32_t r0 = CrtRand();
				t->size = (int16_t)(r0 % 0x600 + 0xD00);
				*(uint32_t *)&t->pos[0] = *(const uint32_t *)&p->pos[0];
				*(uint32_t *)&t->pos[2] = *(const uint32_t *)&p->pos[2]; // z, and the leaf's action word as rise
				const int32_t r1 = CrtRand();
				t->pos[0] = (int16_t)(t->pos[0] + (r1 % 200 - 100));
				const int32_t r2 = CrtRand();
				t->pos[1] = (int16_t)(t->pos[1] + (r2 % 100 - 50));
				const int32_t r3 = CrtRand();
				t->pos[2] = (int16_t)(t->pos[2] + (r3 % 200 - 100));
				const int32_t r4 = CrtRand();
				t->rise = (int16_t)(-10 - r4 % 30);
				const int32_t r5 = CrtRand();
				t->vel[0] = (int16_t)(r5 % 40 - 20);
				const int32_t r6 = CrtRand();
				t->vel[1] = (int16_t)(r6 % 40 - 20);
				const int32_t r7 = CrtRand();
				t->vel[2] = (int16_t)(r7 % 40 - 20);
				const int32_t r8 = CrtRand();
				t->unused = (int16_t)(r8 % 12 + 12);
			}
		}
		LeafMove(p);
		p->counter++;
		return p->counter >= 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x614350)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		DirectorNode *d = (DirectorNode *)n;
		int16_t local[4]; // [esp+8] of the original: both uses write it before reading
		if (d->counter == 0)
		{
			GetDefaultEffectPosition(Entity(d->slot), d->pos);
			d->pos[3] = EntityHeight(d->slot);
			int32_t s = (mul32(EntitySize(d->slot), 5600) + 0x1388) >> 12;
			if (s > 0x898) s = 0x898;
			d->scale = (int16_t)DivMagic(shl32(s, 12), 0x10624DD3, 6);
			GetEffectSpawnPosition(Entity(d->slot), 0xF0, 0x1000, local);
			d->rise = (int16_t)DivMagic(local[1] - 0x384, 0x2AAAAAAB, 2);
		}
		if (d->counter == 1)
		{
			for (int kind = 0; kind < 2; kind++)
			{
				EnvelopeNode *e = (EnvelopeNode *)AddTaskToQueue(EffectQueue(), ORIG_Envelope);
				*(uint32_t *)&e->pos[0] = *(const uint32_t *)&d->pos[0];
				*(uint32_t *)&e->pos[2] = *(const uint32_t *)&d->pos[2];
				e->counter = 0;
				e->kind = (int16_t)kind;
				e->pos[1] = d->pos[3];
				const int32_t r0 = CrtRand();
				e->angle = (int16_t)(r0 % 4096);
				const int32_t r1 = CrtRand();
				e->sxz = 0x1800;
				e->dsxz = kind ? (int16_t)-0xDB : (int16_t)-0x100;
				e->sy = 0;
				e->dsy = kind ? 0x57 : 0x66;
				e->spin = (int16_t)(r1 % 60 + 0x1E);
			}
		}
		if (d->counter == 2)
		{
			int32_t radius = (mul32(EntitySize(d->slot), 4000) >> 12) + 0xC8;
			if (radius > 0x3E8) radius = 0x3E8;
			const int32_t r0 = CrtRand();
			int32_t angle = r0 % 4096;
			const int32_t half = radius / 2;
			for (int leaf = 0; leaf < 8; leaf++)
			{
				LeafNode *p = (LeafNode *)AddTaskToQueue(EffectQueue(), ORIG_Leaf);
				angle += 0x200;
				p->counter = 0;
				p->pos[0] = d->pos[0];
				p->pos[1] = d->pos[3];
				p->pos[2] = d->pos[2];
				p->vel[0] = (int16_t)(mul32(ComputeCos(angle), half) >> 12);
				const int32_t r1 = CrtRand();
				p->vel[1] = (int16_t)(-0x1E - r1 % 0x46);
				p->vel[2] = (int16_t)(mul32(ComputeSin(angle), half) >> 12);
				const int32_t r2 = CrtRand();
				p->leaf = (int16_t)leaf;
				p->size = (int16_t)(r2 % 1024 + 0x1000);
				p->action = d->action;
				TrailPart *trail = TrailOf(d->action, leaf);
				for (int k = 0; k < 16; k++) trail[k].counter = -1;
			}
		}
		if (d->counter == 0x1C)
		{
			ApplyActionResultToTarget(Ctx()->actions[d->action].targets);
			const int32_t next = d->action + 1;
			if (next <= Ctx()->actions[0].last_action)
			{
				DirectorNode *nd = (DirectorNode *)AddTaskToQueue(EffectQueue(), ORIG_Director);
				nd->counter = 0;
				nd->action = (int16_t)next;
				nd->slot = Ctx()->actions[next].targets[0];
			}
		}
		if (d->counter == 1)
		{
			GetDefaultEffectPosition(Entity(d->slot), local);
			BdPlaySE3D(SOUND_Regen, 0, local);
		}
		d->counter++;
		return d->counter > 0x30 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x614D10)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag147_regen_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		if (!(*(const uint8_t *)&r->counter & 1)) PacketCursor() = ARENA_EVEN;
		const int alive = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return alive ? 0 : TASK_END;
	}
}

	void register_mag147_regen()
	{
		register_port(regen147::ORIG_Root, (void *)regen147::RootTask, "R147 Root", 147);
		register_port(regen147::ORIG_Director, (void *)regen147::DirectorTask, "R147 Director", 147);
		register_port(regen147::ORIG_Envelope, (void *)regen147::EnvelopeTask, "R147 Envelope", 147);
		register_port(regen147::ORIG_Leaf, (void *)regen147::LeafTask, "R147 Leaf", 147);
		// 30 fps layer: see mag147_regen_held.inc
		FX_HELD(register_mag147_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag147_regen_held.inc"
#endif
