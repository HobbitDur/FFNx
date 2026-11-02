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

// Effect 109: Dispel (spell, MAG_109_*).
//
// Structure (setup MAG_109_DISPEL 0x6CF4A0 -> _Init 0x6CF4D0, file loader 0x6CF4B0 = the texture
// file named at 0x12E6C3C; the setup starts camera animation 0x12E6410 and queues the TIM):
//   RootTask (0x6CF540) - alternates the packet arena (magic buffer + 0x10E0 / + 0x110E0); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the target task of the
//     next action on its first target, unless a target task of 67 ticks or less still runs on that
//     target (the cycle then restarts); runs the target and particle queues, ends when both are
//     empty.
//   Target (0x6CF700) - node 0x348, one per action: the screen flash (up over 0..7, 0x8FC, down
//     from 60); particles: one every other tick over 26..41 around the target (random), one every
//     other tick over 23..28 on a small circle; a screen fade at 27; the ring prim-model layout
//     0x12E3600 played with the callback RingPart (0x6CF9A0) from tick 0 (sound at 0); damage at
//     67; ends at 72 (flash off).
//   Particle (0x6CFC50) - node 0x1C: a sprite sequence (0x12E3440 or 0x12E337C) drawn in place,
//     frame = its counter, until its end frame.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521A68..0x2521AC0 (texture file, magic buffer base, context, root pool,
// packet cursor 0x2521A8C, particle / target / root queues). The target pool (3 x 0x348), the
// particle pool (64 x 0x1C), the morph vertex buffer (+0x10D8) and the packet arenas are in the
// magic buffer.
// Built from the parts library (fx_layouts.h): the ring through the anchor callback (record flags
// 0x40000 and 0x8000; its never-written "scaled" word is 0), ScreenFlashCurve, RandomAround,
// RisingCirclePoint, DrawSequenceSprite. Parameters (DispelParams, fx_params.h): the numbers above,
// defaults = the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace dispel109
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521A6C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521A70); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521A8C); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521A90; }       // pool: magic buffer, 0x40 x 0x1C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521AA0; }         // pool: + 0x700, 3 x 0x348

	static const uint32_t ORIG_RootTask = 0x6CF540;
	static const uint32_t ORIG_TargetTask = 0x6CF700;
	static const uint32_t ORIG_ParticleTask = 0x6CFC50;
	static const uint32_t MODEL_Ring = 0x12E3600;        // prim-model layout data (0x328-byte layout)
	static const uint32_t SEQ_Sparkle = 0x12E3440;
	static const uint32_t SEQ_Circle = 0x12E337C;
	static const void *const SOUND_Dispel = (const void *)0x12E6C38;
	// the record flags RingPart tests; its "scaled" word is never written (0: the ring is not scaled),
	// so it is the anchor callback
	static const uint32_t HONOURED = ANCHOR_ROT_YXZ | ANCHOR_NOT_COMPOSED;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct DispelParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks...
			int16_t busy_tick;      // ...unless a target task on that target is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t fade_tick;      // the engine's screen fade (0x5712C0) starts
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this (the flash off)
		} timing;
		struct Flash
		{
			int16_t rise, level, fall;   // the screen flash: up over `rise` ticks to `level`, down from `fall` to the end
		} flash;
		struct Ring
		{
			int16_t depth;          // the prim header's depth
			int16_t size_base, size_times;  // the anchor's size word: (the target's size + base) x times (the sparkles' spread)
		} ring;
		struct Particles
		{
			fxp::Window window;     // spawned at target ticks first, first + every... up to last
			int16_t every;
			int16_t lift;           // added to y (y goes down)
			int16_t scale;          // the sprite's size (its rotation a)
			int16_t frames;         // sequence frames: it lives this many ticks
			int16_t turns;          // the circle: angle = (e << 12) / turns, e = tick - first
			uint32_t seq;           // its sprite sequence (FF8_EN.exe data)
		};
		Particles sparkles, circle;
	};
	static const DispelParams DEFAULTS = {
		{ 10, 0x43, (uint32_t)SOUND_Dispel, 0, 0x101, 0x1B, 0x43, 0x48 },
		{ 8, 0x8FC, 0x3C },
		{ -0x100, 0x100, 3 },
		{ { 0x1A, 0x29 }, 2, 0x100, 0x400, 0x10, 0, SEQ_Sparkle },
		{ { 0x17, 0x1C }, 2, 0x100, 0x800, 7, 6, SEQ_Circle },
	};
	static DispelParams g_params = DEFAULTS;
	static inline const DispelParams &P() { return g_params; }
	enum PartId { PART_RING = 1, PART_SPARKLES = 2, PART_CIRCLE = 3 };

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x348 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t pad10[4];
		uint8_t *entity;   // +0x14 target entity
		int16_t pos[4];    // +0x18 effect anchor x, y, z (GetDefaultEffectPosition), then the size word
		uint8_t ring[0x328]; // +0x20 prim-model layout (prim::Layout: data, frame, state)
	};
	struct ParticleNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame
		int16_t end;       // +0x0E frames
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		uint32_t seq;      // +0x18 sprite sequence data
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x348 && sizeof(ParticleNode) == 0x1C, "Dispel nodes");

	// a particle of `part` at tick c, if its window spawns one: its node, placed by `place`
	template<typename Place>
	static void SpawnParticle(const DispelParams::Particles &part, int32_t c, uint32_t seq, Place place)
	{
		const int32_t e = c - part.window.first;
		if (!(part.window.has(c) && part.every > 0 && e % part.every == 0)) return;
		ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
		Memset32(&p->counter, 0, 4);
		place(p, e);
		p->scale = part.scale;
		p->seq = fxp::sequence(part.seq, seq);
		p->end = part.frames;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag109_dispel_held.h"
#endif

namespace ff8fx
{
namespace dispel109
{
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(held_note_particle(p);)
		DrawSequenceSprite(p->pos, p->seq, p->counter, p->scale, (int32_t)0xFFFFF800, PacketCursor());
		p->counter++;
		return p->counter < p->end ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Target (0x6CF700)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const DispelParams &pp = P();
		SetScreenFlash((uint32_t)ScreenFlashCurve(t->counter, pp.flash.rise, pp.flash.level, pp.flash.fall, pp.timing.end_tick), 0);
		// a random sparkle around the target, one on a rising circle
		SpawnParticle(pp.sparkles, t->counter, SEQ_Sparkle, [&](ParticleNode *p, int32_t) { RandomAround(t->pos, t->pos[3], pp.sparkles.lift, p->pos); });
		SpawnParticle(pp.circle, t->counter, SEQ_Circle, [&](ParticleNode *p, int32_t e) { RisingCirclePoint(t->pos, e, pp.circle.turns, pp.circle.lift, p->pos); });
		if (t->counter == pp.timing.fade_tick) ScreenFadeTask(0, 1, 1, 0x60);
		if (t->counter >= 0)
		{
			FX_PART(PART_RING);
			AnchorArg arg;
			memcpy(arg.anchor, t->pos, sizeof(arg.anchor));
			arg.depth = pp.ring.depth;
			arg.morph = TexBase() + 0x10D8;
			arg.honoured = HONOURED;
			arg.cursor = &PacketCursor();
			// 30 fps layer: see mag109_dispel_held.inc
			FX_HELD(held_note_play(t, &arg);)
			PlayAtAnchor(t->ring, &arg);
			FX_PART(0);
			if (t->counter == pp.timing.sound_tick)
				BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Dispel), pp.timing.sound_volume, t->pos);
		}
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= pp.timing.end_tick)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CF540)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		const DispelParams &pp = P();
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x10E0;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x110E0;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)(TexBase() + 0x700), 0x348, 3);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x1C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a target task at busy_tick or before runs on this target
				TargetNode *pool = (TargetNode *)(TexBase() + 0x700);
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= pp.timing.busy_tick)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					Memset32(&t->counter, 0, 0xCF);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					GetDefaultEffectPosition(entity, t->pos);
					t->pos[3] = (int16_t)((*(const int16_t *)(t->entity + 0x26) + pp.ring.size_base) * pp.ring.size_times);
					DecodeModelPrimLayout(MODEL_Ring, t->ring, 0x328);
					r->action++;
				}
			}
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QParticles());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= pp.timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of DispelParams
	// ------------------------------------------------------------------
	namespace dispel109
	{
		using fxp::Field;
#define DISPEL_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(DispelParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn (not while one is young on that target); each target task's clock: the screen flash, its sound, the fade, its result, its end (draws nothing)" },
			{ PART_RING, "ring", "Ring", "The ring layout at the target's effect position" },
			{ PART_SPARKLES, "sparkles", "Sparkles", "A flipbook at random points around the target, spread by the anchor's size word" },
			{ PART_CIRCLE, "circle", "Circle", "A flipbook on a rising circle around the target" },
		};
		static const fxp::Group GROUPS[] = {
			{ "flash", "Screen flash", "Up over Rise ticks to Level, Level, then down from Fall to the end tick" },
			{ "ring.size", "Size word", "The anchor's size word: (the target's size + Base) x Times (the sparkles' spread)" },
			{ "sparkles.window", "Spawning", "One spawned at target ticks From tick, From tick + Every, ... up to To tick" },
			{ "circle.window", "Spawning", "One spawned at target ticks From tick, From tick + Every, ... up to To tick" },
		};
		static const Field FIELDS[] = {
			DISPEL_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			DISPEL_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a target task on that target is at this tick or before (the cycle restarts)", I16, -1, 200),
			DISPEL_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			DISPEL_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			DISPEL_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			DISPEL_FIELD("timing", timing.fade_tick, "Fade tick", "The engine's screen fade starts when the target tick == this", I16, -1, 200),
			DISPEL_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			DISPEL_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this (the flash off)", I16, 0, 200),
			DISPEL_FIELD("timing", flash.rise, "Rise", "", I16, 1, 200),
			DISPEL_FIELD("timing", flash.level, "Level", "", I16, 0, 0x7FFF),
			DISPEL_FIELD("timing", flash.fall, "Fall", "", I16, 0, 199),
			DISPEL_FIELD("ring", ring.depth, "Depth", "Added to its drawing depth (sort key)", I16, -0x1000, 0x1000),
			DISPEL_FIELD("ring", ring.size_base, "Base", "", I16, -0x1000, 0x1000),
			DISPEL_FIELD("ring", ring.size_times, "Times", "", I16, 0, 16),
			DISPEL_FIELD("sparkles", sparkles.window.first, "From tick", "", I16, 0, 200),
			DISPEL_FIELD("sparkles", sparkles.window.last, "To tick", "", I16, 0, 200),
			DISPEL_FIELD("sparkles", sparkles.every, "Every", "", I16, 1, 50),
			DISPEL_FIELD("sparkles", sparkles.lift, "Lift", "Added to y (y goes down)", I16, -0x2000, 0x2000),
			DISPEL_FIELD("sparkles", sparkles.scale, "Size", "The sprite's size", I16, 0, 0x7FFF),
			DISPEL_FIELD("sparkles", sparkles.frames, "Frames", "Flipbook frames, one per tick: it lives this many ticks", I16, 1, 64),
			DISPEL_FIELD("sparkles", sparkles.seq, "Sequence", "The sprite sequence", Seq, 0, 0x7FFFFFFF),
			DISPEL_FIELD("circle", circle.window.first, "From tick", "", I16, 0, 200),
			DISPEL_FIELD("circle", circle.window.last, "To tick", "", I16, 0, 200),
			DISPEL_FIELD("circle", circle.every, "Every", "", I16, 1, 50),
			DISPEL_FIELD("circle", circle.lift, "Lift", "Added to y (y goes down)", I16, -0x2000, 0x2000),
			DISPEL_FIELD("circle", circle.scale, "Size", "The sprite's size", I16, 0, 0x7FFF),
			DISPEL_FIELD("circle", circle.frames, "Frames", "Flipbook frames, one per tick: it lives this many ticks", I16, 1, 64),
			DISPEL_FIELD("circle", circle.turns, "Points a turn", "angle = (tick - From tick) x 4096 / this", I16, 1, 64),
			DISPEL_FIELD("circle", circle.seq, "Sequence", "The sprite sequence", Seq, 0, 0x7FFFFFFF),
		};
#undef DISPEL_FIELD
	}

	void register_mag109_dispel()
	{
		fxp::register_params({ 109, "Dispel", &dispel109::g_params, &dispel109::DEFAULTS, sizeof(dispel109::DispelParams),
		                       dispel109::PARTS, (int)(sizeof(dispel109::PARTS) / sizeof(dispel109::PARTS[0])),
		                       dispel109::FIELDS, (int)(sizeof(dispel109::FIELDS) / sizeof(dispel109::FIELDS[0])),
		                       dispel109::GROUPS, (int)(sizeof(dispel109::GROUPS) / sizeof(dispel109::GROUPS[0])) });
		register_port(dispel109::ORIG_RootTask, (void *)dispel109::RootTask, "D109 RootTask", 109);
		register_port(dispel109::ORIG_TargetTask, (void *)dispel109::TargetTask, "D109 TargetTask", 109);
		register_port(dispel109::ORIG_ParticleTask, (void *)dispel109::ParticleTask, "D109 ParticleTask", 109);
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(register_mag109_held();)
	}
	FX_LAB_MODULE(register_mag109_dispel);
}

#ifdef FF8_FX_HELD
#include "mag109_dispel_held.inc"
#endif
