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

// Effect 2: Fire (spell, MAG_002_*).
//
// Structure (setup MAG_002_FIRE_Init 0x6298A0, file loader 0x629880 = mag001.tim):
//   RootTask (0x62A610) - alternates the packet arena, runs the effect queue, ends when it is empty.
//   Director, one per action: MultiTarget (0x629990, action with several targets) or
//     SingleTarget (0x62A380, waits until its target is free); at tick 1 spawns flames around
//     each target, 6 sound, 8 damage, 12 spawns the next action's director, ends at 22.
//   Flame (0x629C10) - 16-frame flipbook at a fixed position + a ground glow; at its first
//     drawn frame spawns 4 embers and 10 sparks.
//   Spark (0x629F90) - shared sprite 5, drifts and decelerates, shrinks.
//   Ember (0x62A0A0) - 16-frame flipbook rising faster and faster, shrinking.
// Module globals: 0x24CA818..0x24DFD70 (queues, node pool, packet arenas 0x24CBD6C / 0x24D5D6C,
// packet cursor 0x24DFD6C) and the per-slot "target free" flags 0xDEE360 (7 x u32, exe data).

#include "mag_common.h"

namespace ff8fx
{
namespace fire002
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline TaskQueuePair &Queues() { return var<TaskQueuePair>(0x24CA828); } // .first = root, .second = effect queue
	inline CastContext *&Ctx() { return var<CastContext *>(0x24CBD60); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x24DFD6C); }
	inline uint32_t *TargetFree() { return (uint32_t *)0xDEE360; }      // [slot]: 1 = no Fire running on it
	static const uint32_t ARENA_EVEN = 0x24CBD6C, ARENA_ODD = 0x24D5D6C;

	static const uint32_t ORIG_RootTask = 0x62A610;
	static const uint32_t ORIG_DirectorMulti = 0x629990;
	static const uint32_t ORIG_DirectorSingle = 0x62A380;
	static const uint32_t ORIG_FlameTask = 0x629C10;
	static const uint32_t ORIG_SparkTask = 0x629F90;
	static const uint32_t ORIG_EmberTask = 0x62A0A0;
	static const void *const SEQ_Flame = (const void *)0xDEDC2C;
	static const void *const SEQ_FlameGlow = (const void *)0xDEDDD8;
	static const void *const SEQ_Ember = (const void *)0xDEDF84;
	static const void *const SOUND_Fire = (const void *)0xDEE130;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// ------------------------------------------------------------------
	// Node layouts (pool of 0x96 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
	struct RootNode { TaskNode hdr; uint16_t counter; };
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t action;   // +0x0E action index
		uint8_t pad10[0x10];
		int16_t slot;     // +0x20 target slot (single-target director)
		int16_t sub;      // +0x22 ticks since spawn (next action at 12)
	};
	struct FlameNode
	{
		TaskNode hdr;
		int16_t frame;    // +0x0C flipbook frame
		int16_t delay;    // +0x0E ticks before the first draw
		int16_t pos[3];   // +0x10 x, y, z
		int16_t ground;   // +0x16 target height (glow plane)
		uint8_t pad18[4];
		int16_t scale;    // +0x1C
		uint8_t pad1E[6];
	};
	struct SparkNode
	{
		TaskNode hdr;
		int16_t life;     // +0x0C counts down, frame = 9 - life
		int16_t delay;    // +0x0E
		int16_t pos[3];   // +0x10
		int16_t pad16;
		int16_t vel[3];   // +0x18
		int16_t pad1E;
		int16_t scale;    // +0x20
		int16_t pad22;
	};
	struct EmberNode
	{
		TaskNode hdr;
		int16_t frame;    // +0x0C
		int16_t delay;    // +0x0E
		int16_t pos[3];   // +0x10
		int16_t vel_y;    // +0x16
		uint8_t pad18[4];
		int16_t scale;    // +0x1C
		uint8_t pad1E[6];
	};
	static_assert(sizeof(DirectorNode) == 0x24 && sizeof(FlameNode) == 0x24 && sizeof(SparkNode) == 0x24 && sizeof(EmberNode) == 0x24, "Fire nodes are 0x24 bytes");
}
}

#ifdef FF8_FX_HELD
#include "mag002_fire_held.h"
#endif

namespace ff8fx
{
namespace fire002
{
	// ------------------------------------------------------------------
	// Root task (0x62A610): node +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		if (!(r->counter & 1)) PacketCursor() = ARENA_EVEN;
		int alive = ExecuteTaskQueue(&Queues().second);
		r->counter++;
		return alive ? 0 : TASK_END;
	}

	// director of action a+1, spawned by the director of action a (MultiTarget when the action
	// has several targets)
	static void SpawnNextDirector(DirectorNode *d)
	{
		ActionData *acts = Ctx()->actions;
		int next = d->action + 1;
		if (next > acts[0].last_action) return;
		DirectorNode *t;
		if (acts[next].target_count > 1)
		{
			t = (DirectorNode *)AddTaskToQueue(&Queues().second, ORIG_DirectorMulti);
			t->counter = 0;
			t->action = (int16_t)next;
		}
		else
		{
			t = (DirectorNode *)AddTaskToQueue(&Queues().second, ORIG_DirectorSingle);
			t->action = (int16_t)next;
			t->counter = 0;
			t->slot = (int16_t)(uint16_t)Ctx()->actions[next].targets[0];
		}
		t->sub = 0;
	}

	// flame spread around a target: its size * 2800 / 4096, at most 1000
	static int32_t SpreadOf(int slot)
	{
		int32_t s = (EntitySize(slot) * 2800) >> 12;
		return s > 1000 ? 1000 : s;
	}

	// ------------------------------------------------------------------
	// Director, action with several targets (0x629990)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorMulti(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		if (DrawOnly()) return 0;
		if (d->sub == 12) SpawnNextDirector(d);
		d->sub++;
		if (d->counter == 1)
		{
			ActionData *ad = &Ctx()->actions[d->action];
			if (ad->target_count > 0)
			{
				int off = 0;
				for (int k = 0;;)
				{
					int slot = ad->targets[off];
					int32_t spread = SpreadOf(slot);
					int16_t pos[4];
					GetDefaultEffectPosition(Entity(slot), pos);
					int32_t half = spread >> 1;
					FlameNode *f = (FlameNode *)AddTaskToQueue(&Queues().second, ORIG_FlameTask);
					f->frame = 0;
					f->delay = (int16_t)(CrtRand() % 6);
					memcpy(f->pos, pos, 8);
					f->pos[0] += (int16_t)(CrtRand() % spread - half);
					f->pos[1] += (int16_t)(CrtRand() % spread - half);
					int32_t rz = CrtRand() % spread;
					int16_t h = EntityHeight(slot);
					f->ground = h;
					f->pos[2] += (int16_t)(rz - half);
					if (f->pos[1] > h - 0x226)
						f->pos[1] += (int16_t)(-500 - CrtRand() % 300);
					f->scale = (int16_t)(CrtRand() % 0x600 + 0xD00);
					off += TARGET_STRIDE;
					k++;
					ad = &Ctx()->actions[d->action];
					if (k >= ad->target_count) break;
				}
			}
		}
		if (d->counter == 8)
		{
			ActionData *ad = &Ctx()->actions[d->action];
			ApplyActionResultToTargets(ad->targets, ad->target_count);
		}
		if (d->counter == 6) BdPlaySE(SOUND_Fire, 0x100, 0x80);
		d->counter++;
		return d->counter >= 0x16 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director, single target (0x62A380): waits until no other Fire runs on its target
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorSingle(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		if (DrawOnly()) return 0;
		if (d->sub == 12) SpawnNextDirector(d);
		int slot = d->slot;
		d->sub++;
		if (TargetFree()[slot] == 0 && d->counter == 0) return 0;
		if (d->counter == 0) TargetFree()[slot] = 0;
		if (d->counter == 1)
		{
			int32_t spread = SpreadOf(slot);
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(slot), pos);
			int32_t half = spread >> 1;
			for (int k = 0; k < 4; k++)
			{
				FlameNode *f = (FlameNode *)AddTaskToQueue(&Queues().second, ORIG_FlameTask);
				f->frame = 0;
				f->delay = (int16_t)(CrtRand() % 6 + 2 * k);
				memcpy(f->pos, pos, 8);
				f->pos[0] += (int16_t)(CrtRand() % spread - half);
				f->pos[1] += (int16_t)(CrtRand() % spread - half);
				f->pos[2] += (int16_t)(CrtRand() % spread - half);
				int16_t h = EntityHeight(d->slot);
				f->ground = h;
				if (f->pos[1] > h - 0x226)
					f->pos[1] += (int16_t)(-500 - CrtRand() % 300);
				f->scale = (int16_t)(CrtRand() % 0x600 + 0xD00);
			}
		}
		if (d->counter == 8) ApplyActionResultToTarget(Ctx()->actions[d->action].targets);
		if (d->counter == 6)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(d->slot), pos);
			BdPlaySE3D(SOUND_Fire, 0x100, pos);
		}
		d->counter++;
		if (d->counter < 0x16) return 0;
		TargetFree()[d->slot] = 1;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Flame (0x629C10): flipbook frame `frame` at pos (camera-facing, size = scale), then a ground
	// glow flat on the target's height plane, brighter the higher the flame (y clamped at -2000)
	// ------------------------------------------------------------------
	static void FlameDraw(const FlameNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
		uint8_t *h = AllocHeader(0xB4);
		*(const void **)h = SEQ_Flame;
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		int16_t angles[4] = { 0x400, 0, 0, 0 };
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[0] = p->pos[0];
		m.t[1] = p->ground;
		m.t[2] = p->pos[2];
		int32_t sv[4] = { p->scale, p->scale, 0x1000, 0 };
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		int32_t y = p->pos[1];
		if (y < -2000) y = -2000;
		int32_t q = (int32_t)(((int64_t)shl32(y, 12) * 0x10624DD3) >> 32) >> 7;
		q += (int32_t)((uint32_t)q >> 31);                  // (y << 12) / 2000
		uint8_t level = (uint8_t)((q * 160) / 4096 + 0xA0);
		*(const void **)h = SEQ_FlameGlow;
		*(uint16_t *)(h + 0x24) = 0xC;
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		h[0x1E] = level;
		h[0x1D] = level;
		h[0x1C] = level;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		FlameNode *p = (FlameNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_FlameTask, p);)
		FlameDraw(p);
		if (DrawOnly()) return 0;
		if (p->frame == 0)
		{
			for (int k = 0; k < 4; k++)
			{
				EmberNode *e = (EmberNode *)AddTaskToQueue(&Queues().second, ORIG_EmberTask);
				e->frame = 0;
				e->delay = (int16_t)(CrtRand() % 6 + k);
				memcpy(e->pos, p->pos, 8);
				e->pos[0] += (int16_t)(CrtRand() % 300 - 150);
				e->pos[1] -= (int16_t)(CrtRand() % 100);
				e->pos[2] += (int16_t)(CrtRand() % 300 - 150);
				e->vel_y = (int16_t)(-20 - CrtRand() % 30);
				e->scale = (int16_t)(CrtRand() % 0x500 + 0xA00);
			}
			for (int k = 0; k < 10; k++)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(&Queues().second, ORIG_SparkTask);
				s->life = (int16_t)(CrtRand() % 4 + 5);
				s->delay = (int16_t)(CrtRand() % 4 + k);
				memcpy(s->pos, p->pos, 8);
				s->pos[0] += (int16_t)(CrtRand() % 400 - 200);
				s->pos[1] -= (int16_t)(CrtRand() % 200);
				s->pos[2] += (int16_t)(CrtRand() % 400 - 200);
				s->vel[0] = (int16_t)(CrtRand() % 80 - 40);
				s->vel[1] = (int16_t)(-20 - CrtRand() % 50);
				s->vel[2] = (int16_t)(CrtRand() % 80 - 40);
				s->scale = (int16_t)(CrtRand() % 0x600 + 0x900);
			}
		}
		p->frame++;
		return p->frame >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark (0x629F90): shared sprite 5, frame 9 - life
	// ------------------------------------------------------------------
	static void SparkDraw(const SparkNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(void **)h = EffectSpriteSharedSequence(5);
		*(uint16_t *)(h + 4) = (uint16_t)(9 - p->life);
		h[0x20] = 4;
		*(uint16_t *)(h + 0x24) = 0x10;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// drift: x and z decelerate by 1/16, y (upwards) accelerates by 1/16; shrink by 0x40
	static void SparkUpdate(SparkNode *p)
	{
		int16_t v = p->vel[0];
		p->scale = (int16_t)(p->scale - 0x40);
		p->pos[0] = (int16_t)(p->pos[0] + v);
		p->vel[0] = (int16_t)(v - (int16_t)(v >> 4));
		v = p->vel[1];
		p->pos[1] = (int16_t)(p->pos[1] + v);
		p->vel[1] = (int16_t)(v + (int16_t)(v >> 4));
		v = p->vel[2];
		p->pos[2] = (int16_t)(p->pos[2] + v);
		p->vel[2] = (int16_t)(v - (int16_t)(v >> 4));
		p->life--;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_SparkTask, p);)
		SparkDraw(p);
		if (DrawOnly()) return 0;
		SparkUpdate(p);
		return p->life < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ember (0x62A0A0): flipbook 0xDEDF84, frame `frame`
	// ------------------------------------------------------------------
	static void EmberDraw(const EmberNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		*(const void **)h = SEQ_Ember;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// rise: y += vel_y, vel_y grows by 1/32; shrink by 1/64
	static void EmberUpdate(EmberNode *p)
	{
		int16_t v = p->vel_y;
		p->pos[1] = (int16_t)(p->pos[1] + v);
		p->vel_y = (int16_t)(v + (int16_t)(v >> 5));
		int16_t s = p->scale;
		p->scale = (int16_t)(s - (int16_t)(s >> 6));
		p->frame++;
	}

	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		EmberNode *p = (EmberNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_EmberTask, p);)
		EmberDraw(p);
		if (DrawOnly()) return 0;
		EmberUpdate(p);
		return p->frame >= 0x10 ? TASK_END : 0;
	}
}

	void register_mag002_fire()
	{
		register_port(fire002::ORIG_RootTask, (void *)fire002::RootTask, "F002 RootTask", 2);
		register_port(fire002::ORIG_DirectorMulti, (void *)fire002::DirectorMulti, "F002 DirectorMulti", 2);
		register_port(fire002::ORIG_DirectorSingle, (void *)fire002::DirectorSingle, "F002 DirectorSingle", 2);
		register_port(fire002::ORIG_FlameTask, (void *)fire002::FlameTask, "F002 FlameTask", 2);
		register_port(fire002::ORIG_SparkTask, (void *)fire002::SparkTask, "F002 SparkTask", 2);
		register_port(fire002::ORIG_EmberTask, (void *)fire002::EmberTask, "F002 EmberTask", 2);
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(register_mag002_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag002_fire_held.inc"
#endif
