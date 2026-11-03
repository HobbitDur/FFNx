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
// Built from the parts library (fx_flames.h: the flames and their ground glow, the embers, the
// sparks). Parameters (FireParams, fx_params.h): the numbers above, defaults = the game's; the
// parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_flames.h"

namespace ff8fx
{
namespace fire002
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

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
	static const uint32_t SEQ_Flame = 0xDEDC2C;
	static const uint32_t SEQ_FlameGlow = 0xDEDDD8;
	static const uint32_t SEQ_Ember = 0xDEDF84;
	static const void *const SOUND_Fire = (const void *)0xDEE130;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct FireParams
	{
		struct Timing
		{
			int16_t chain_tick;     // a director starts the next action's at this tick of its own (counted from its start, waiting or not)
			int16_t flames_tick;    // the flames start (director tick)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE / BdPlaySE3D read its world sound id
			int16_t sound_tick;     // the sound plays (a lone target: at its effect position)...
			int16_t sound_volume;   // ...at this volume...
			int16_t sound_pan;      // ...and, several targets, this pan
			int16_t damage_tick;    // the action's targets take the result
			int16_t end_tick;       // the director ends (and its lone target is free again)
		} timing;
		struct Flames
		{
			int16_t count;          // flames around a lone target (several targets: one each)...
			struct { int16_t mod, step; } delay;   // ...flame k waits rand % mod + step x k ticks
			struct { int16_t num, max; } spread;   // around the target's effect position: its size x num / 4096, at most max
			int16_t low;            // a flame lower than the target's height - this...
			struct { int16_t base, mod; } lift;    // ...goes up: y += base - rand % mod
			struct { int16_t base, mod; } scale;   // its size: base + rand % mod
			int16_t frames;         // the flipbook's frames (it ends after them)
			int16_t shade_shift;    // the camera offset: -(size >> this)
			struct { int16_t tilt, mode, high, span, base; } glow;   // the ground glow: turned about x, its draw mode, its grey from the flame's height
		} flames;
		struct Embers
		{
			int16_t count;          // embers from each flame at its first frame...
			struct { int16_t mod, step; } delay;   // ...ember k waits rand % mod + step x k ticks
			int16_t spread;         // around the flame: x and z + rand % spread - spread / 2...
			int16_t drop;           // ...y - rand % drop
			struct { int16_t base, mod; } rise;    // its speed up: base - rand % mod
			struct { int16_t base, mod; } scale;   // its size: base + rand % mod
			int16_t frames;         // the flipbook's frames (it ends after them)
			int16_t shade_shift;    // the camera offset: -(size >> this)
			int16_t accel;          // its speed grows by itself >> this a tick
			int16_t shrink;         // its size shrinks by itself >> this a tick
		} embers;
		struct Sparks
		{
			int16_t count;          // sparks from each flame at its first frame...
			struct { int16_t base, mod; } life;    // ...each living base + rand % mod (+ 1) ticks...
			struct { int16_t mod, step; } delay;   // ...after rand % mod + step x k ticks
			int16_t spread;         // around the flame: x and z + rand % spread - spread / 2...
			int16_t drop;           // ...y - rand % drop
			int16_t speed;          // x and z speed: rand % speed - speed / 2
			struct { int16_t base, mod; } rise;    // its speed up: base - rand % mod
			struct { int16_t base, mod; } scale;   // its size: base + rand % mod
			int16_t sprite;         // the shared effect sprite...
			int16_t last;           // ...its frame last - life
			int16_t flag20, mode;   // the sprite header's byte +0x20 and draw mode
			int16_t shade_shift;    // the camera offset: -(size >> this)
			int16_t drag;           // x and z slow by themselves >> this, y speeds up so
			int16_t shrink;         // its size shrinks by this a tick
		} sparks;
	};
	static const FireParams DEFAULTS = {
		{ 12, 1, (uint32_t)SOUND_Fire, 6, 0x100, 0x80, 8, 0x16 },
		{ 4, { 6, 2 }, { 2800, 1000 }, 0x226, { -500, 300 }, { 0xD00, 0x600 }, 0x10, 4, { 0x400, 0xC, -2000, 160, 0xA0 } },
		{ 4, { 6, 1 }, 300, 100, { -20, 30 }, { 0xA00, 0x500 }, 0x10, 3, 5, 6 },
		{ 10, { 5, 4 }, { 4, 1 }, 400, 200, 80, { -20, 50 }, { 0x900, 0x600 }, 5, 9, 4, 0x10, 3, 4, 0x40 },
	};
	static FireParams g_params = DEFAULTS;
	static inline const FireParams &P() { return g_params; }
	enum PartId { PART_FLAMES = 1, PART_EMBERS = 2, PART_SPARKS = 3 };

	static FlameRolls FlameRollsOf(const FireParams::Flames &f)
	{
		return { f.delay.mod, f.delay.step, f.spread.num, f.spread.max, f.low, { f.lift.base, f.lift.mod, -1 }, { f.scale.base, f.scale.mod, 1 } };
	}
	static EmberRolls EmberRollsOf(const FireParams::Embers &e)
	{
		return { e.delay.mod, e.delay.step, { -(e.spread >> 1), e.spread, 1 }, { 0, e.drop, -1 }, { -(e.spread >> 1), e.spread, 1 },
		         { e.rise.base, e.rise.mod, -1 }, { e.scale.base, e.scale.mod, 1 } };
	}
	static SparkRolls SparkRollsOf(const FireParams::Sparks &s)
	{
		return { { s.life.base, s.life.mod, 1 }, s.delay.mod, s.delay.step, { -(s.spread >> 1), s.spread, 1 }, { 0, s.drop, -1 },
		         { -(s.spread >> 1), s.spread, 1 }, { -(s.speed >> 1), s.speed, 1 }, { s.rise.base, s.rise.mod, -1 },
		         { -(s.speed >> 1), s.speed, 1 }, { s.scale.base, s.scale.mod, 1 } };
	}

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
		Flame f;          // +0x10 position, ground, size
	};
	struct SparkNode
	{
		TaskNode hdr;
		int16_t life;     // +0x0C counts down, frame = 9 - life
		int16_t delay;    // +0x0E
		FireSpark s;      // +0x10 position, speed, size
	};
	struct EmberNode
	{
		TaskNode hdr;
		int16_t frame;    // +0x0C
		int16_t delay;    // +0x0E
		Ember e;          // +0x10 position, rise speed, size
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

	// `count` flames around target `slot`
	static void StartFlames(int slot, int count)
	{
		const FlameRolls r = FlameRollsOf(P().flames);
		const int32_t spread = FlameSpread(EntitySize(slot), r);
		int16_t pos[4];
		GetDefaultEffectPosition(Entity(slot), pos);
		for (int k = 0; k < count; k++)
		{
			FlameNode *f = (FlameNode *)AddTaskToQueue(&Queues().second, ORIG_FlameTask);
			f->frame = 0;
			FlameStart(&f->f, &f->delay, pos, EntityHeight(slot), spread, k, r);
		}
	}

	// ------------------------------------------------------------------
	// Director, action with several targets (0x629990)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorMulti(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		const FireParams::Timing &tp = P().timing;
		if (DrawOnly()) return 0;
		if (d->sub == tp.chain_tick) SpawnNextDirector(d);
		d->sub++;
		if (d->counter == tp.flames_tick)   // one flame around each target
			for (int k = 0; k < Ctx()->actions[d->action].target_count; k++) StartFlames(Ctx()->actions[d->action].targets[k * TARGET_STRIDE], 1);
		if (d->counter == tp.damage_tick)
		{
			ActionData *ad = &Ctx()->actions[d->action];
			ApplyActionResultToTargets(ad->targets, ad->target_count);
		}
		if (d->counter == tp.sound_tick) BdPlaySE((const void *)fxp::sound(tp.sound, (uint32_t)SOUND_Fire), tp.sound_volume, tp.sound_pan);
		d->counter++;
		return d->counter >= tp.end_tick ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director, single target (0x62A380): waits until no other Fire runs on its target
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorSingle(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		const FireParams::Timing &tp = P().timing;
		if (DrawOnly()) return 0;
		if (d->sub == tp.chain_tick) SpawnNextDirector(d);
		int slot = d->slot;
		d->sub++;
		if (TargetFree()[slot] == 0 && d->counter == 0) return 0;
		if (d->counter == 0) TargetFree()[slot] = 0;
		if (d->counter == tp.flames_tick) StartFlames(slot, P().flames.count);
		if (d->counter == tp.damage_tick) ApplyActionResultToTarget(Ctx()->actions[d->action].targets);
		if (d->counter == tp.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(d->slot), pos);
			BdPlaySE3D((const void *)fxp::sound(tp.sound, (uint32_t)SOUND_Fire), tp.sound_volume, pos);
		}
		d->counter++;
		if (d->counter < tp.end_tick) return 0;
		TargetFree()[d->slot] = 1;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Flame (0x629C10): flipbook frame `frame` at pos (camera-facing, size = scale), then a ground
	// glow flat on the target's height plane, brighter the higher the flame (y clamped at -2000)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		FlameNode *p = (FlameNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		const FireParams &pp = P();
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_FlameTask, p);)
		FX_PART(PART_FLAMES);
		const FireParams::Flames &fp = pp.flames;
		const FlameLook look = { SEQ_Flame, SEQ_FlameGlow, fp.shade_shift, fp.glow.tilt, fp.glow.mode, fp.glow.high, fp.glow.span, fp.glow.base };
		FlameDraw(&p->f, p->frame, look, PacketCursor());
		FX_PART(0);
		if (DrawOnly()) return 0;
		if (p->frame == 0)
		{
			const EmberRolls er = EmberRollsOf(pp.embers);
			for (int k = 0; k < pp.embers.count; k++)
			{
				EmberNode *m = (EmberNode *)AddTaskToQueue(&Queues().second, ORIG_EmberTask);
				m->frame = 0;
				EmberStart(&m->e, &m->delay, p->f.pos, k, er);
			}
			const SparkRolls sr = SparkRollsOf(pp.sparks);
			for (int k = 0; k < pp.sparks.count; k++)
			{
				SparkNode *m = (SparkNode *)AddTaskToQueue(&Queues().second, ORIG_SparkTask);
				FireSparkStart(&m->s, &m->life, &m->delay, p->f.pos, k, sr);
			}
		}
		p->frame++;
		return p->frame >= fp.frames ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark (0x629F90): shared sprite 5, frame 9 - life
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		const FireParams::Sparks &s = P().sparks;
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_SparkTask, p);)
		FX_PART(PART_SPARKS);
		const SparkLook look = { s.sprite, s.last, s.flag20, s.mode, s.shade_shift };
		FireSparkDraw(&p->s, p->life, look, PacketCursor());
		FX_PART(0);
		if (DrawOnly()) return 0;
		FireSparkMove(&p->s, &p->life, s.drag, s.shrink);
		return p->life < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ember (0x62A0A0): flipbook 0xDEDF84, frame `frame`
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		EmberNode *p = (EmberNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		const FireParams::Embers &e = P().embers;
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(held_note_draw(ORIG_EmberTask, p);)
		FX_PART(PART_EMBERS);
		EmberDraw(&p->e, p->frame, SEQ_Ember, e.shade_shift, PacketCursor());
		FX_PART(0);
		if (DrawOnly()) return 0;
		EmberRise(&p->e, e.accel, e.shrink);
		p->frame++;
		return p->frame >= e.frames ? TASK_END : 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of FireParams
	// ------------------------------------------------------------------
	namespace fire002
	{
		using fxp::Field;
#define FIRE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(FireParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "One director per action, each starting the next one's; its clock: the flames' start, its sound, its result, its end (draws nothing)" },
			{ PART_FLAMES, "flames", "Flames", "Flipbooks around the action's targets, each over a ground glow; at its first frame a flame starts its embers and sparks" },
			{ PART_EMBERS, "embers", "Embers", "Flipbooks rising from each flame, faster and faster, shrinking" },
			{ PART_SPARKS, "sparks", "Sparks", "Shared effect sprites drifting from each flame, slowing, shrinking" },
		};
		static const fxp::Group GROUPS[] = {
			{ "flames.delay", "Delay", "Flame k (0, 1...) waits rand % Mod + Step x k ticks before its first frame" },
			{ "flames.spread", "Spread", "Around the target's effect position, each axis + rand % s - s / 2: s = the target's size x Times / 4096, at most At most" },
			{ "flames.lift", "Lift", "A flame lower than the target's height - Low goes up: y += Base - rand % Mod" },
			{ "flames.scale", "Size", "Base + rand % Mod (4096: the sequence's own size)" },
			{ "flames.glow", "Ground glow", "Under each flame, on the target's height plane: turned by Tilt about x (1024: flat), draw mode Mode; its grey = ((the flame's y, at least High) x 4096 / -High) x Span / 4096 + Base" },
			{ "embers.delay", "Delay", "Ember k (0, 1...) waits rand % Mod + Step x k ticks before its first frame" },
			{ "embers.rise", "Rise speed", "Its speed up: Base - rand % Mod (y goes down: up)" },
			{ "embers.scale", "Size", "Base + rand % Mod (4096: the sequence's own size)" },
			{ "sparks.life", "Life", "It lives Base + rand % Mod + 1 ticks: its frame Last frame - life (life counting down to 0)" },
			{ "sparks.delay", "Delay", "Spark k (0, 1...) waits rand % Mod + Step x k ticks before its first frame" },
			{ "sparks.rise", "Rise speed", "Its speed up: Base - rand % Mod (y goes down: up)" },
			{ "sparks.scale", "Size", "Base + rand % Mod (4096: the sprite's own size)" },
		};
		static const Field FIELDS[] = {
			FIRE_FIELD("timing", timing.chain_tick, "Next action", "A director starts the next action's director at this tick of its own (counted from its start, waiting or not)", I16, 0, 200),
			FIRE_FIELD("timing", timing.flames_tick, "Flames tick", "The flames start at this director tick", I16, 0, 200),
			FIRE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			FIRE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at this director tick (a lone target: at its effect position)", I16, 0, 200),
			FIRE_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE's and BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			FIRE_FIELD("timing", timing.sound_pan, "Sound pan", "Several targets: where it plays between the speakers (128: the middle)", I16, 0, 255),
			FIRE_FIELD("timing", timing.damage_tick, "Damage tick", "The action's targets take the result at this director tick", I16, 0, 200),
			FIRE_FIELD("timing", timing.end_tick, "End tick", "The director ends at this tick (its lone target is free for the next one)", I16, 1, 200),
			FIRE_FIELD("flames", flames.count, "Count", "Flames around a lone target (several targets: one around each)", I16, 0, 50),
			FIRE_FIELD("flames", flames.delay.mod, "Mod", "", I16, 1, 200),
			FIRE_FIELD("flames", flames.delay.step, "Step", "", I16, 0, 50),
			FIRE_FIELD("flames", flames.spread.num, "Times", "", I16, 1, 0x7FFF),
			FIRE_FIELD("flames", flames.spread.max, "At most", "", I16, 1, 0x7FFF),
			FIRE_FIELD("flames", flames.low, "Low", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("flames", flames.lift.base, "Base", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("flames", flames.lift.mod, "Mod", "", I16, 1, 0x4000),
			FIRE_FIELD("flames", flames.scale.base, "Base", "", I16, 0, 0x7FFF),
			FIRE_FIELD("flames", flames.scale.mod, "Mod", "", I16, 1, 0x7FFF),
			FIRE_FIELD("flames", flames.frames, "Frames", "The flipbook's frames: the flame ends after them", I16, 1, 200),
			FIRE_FIELD("flames", flames.shade_shift, "Shade shift", "The camera offset it is drawn with: -(its size >> this)", I16, 0, 15),
			FIRE_FIELD("flames", flames.glow.tilt, "Tilt", "", I16, -0x1000, 0x1000),
			FIRE_FIELD("flames", flames.glow.mode, "Mode", "", I16, 0, 0x7FFF),
			FIRE_FIELD("flames", flames.glow.high, "High", "", I16, -0x7FFF, -1),
			FIRE_FIELD("flames", flames.glow.span, "Span", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("flames", flames.glow.base, "Base", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("embers", embers.count, "Count", "Embers each flame starts at its first frame", I16, 0, 50),
			FIRE_FIELD("embers", embers.delay.mod, "Mod", "", I16, 1, 200),
			FIRE_FIELD("embers", embers.delay.step, "Step", "", I16, 0, 50),
			FIRE_FIELD("embers", embers.spread, "Spread", "Around the flame: x and z + rand % this - this / 2", I16, 1, 0x4000),
			FIRE_FIELD("embers", embers.drop, "Above", "Above the flame: y - rand % this", I16, 1, 0x4000),
			FIRE_FIELD("embers", embers.rise.base, "Base", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("embers", embers.rise.mod, "Mod", "", I16, 1, 0x4000),
			FIRE_FIELD("embers", embers.scale.base, "Base", "", I16, 0, 0x7FFF),
			FIRE_FIELD("embers", embers.scale.mod, "Mod", "", I16, 1, 0x7FFF),
			FIRE_FIELD("embers", embers.frames, "Frames", "The flipbook's frames: the ember ends after them", I16, 1, 200),
			FIRE_FIELD("embers", embers.shade_shift, "Shade shift", "The camera offset it is drawn with: -(its size >> this)", I16, 0, 15),
			FIRE_FIELD("embers", embers.accel, "Speed-up shift", "Its speed grows by itself >> this a tick", I16, 0, 15),
			FIRE_FIELD("embers", embers.shrink, "Shrink shift", "Its size shrinks by itself >> this a tick", I16, 0, 15),
			FIRE_FIELD("sparks", sparks.count, "Count", "Sparks each flame starts at its first frame", I16, 0, 50),
			FIRE_FIELD("sparks", sparks.life.base, "Base", "", I16, 0, 200),
			FIRE_FIELD("sparks", sparks.life.mod, "Mod", "", I16, 1, 200),
			FIRE_FIELD("sparks", sparks.delay.mod, "Mod", "", I16, 1, 200),
			FIRE_FIELD("sparks", sparks.delay.step, "Step", "", I16, 0, 50),
			FIRE_FIELD("sparks", sparks.spread, "Spread", "Around the flame: x and z + rand % this - this / 2", I16, 1, 0x4000),
			FIRE_FIELD("sparks", sparks.drop, "Above", "Above the flame: y - rand % this", I16, 1, 0x4000),
			FIRE_FIELD("sparks", sparks.speed, "Speed", "x and z speed: rand % this - this / 2", I16, 1, 0x4000),
			FIRE_FIELD("sparks", sparks.rise.base, "Base", "", I16, -0x4000, 0x4000),
			FIRE_FIELD("sparks", sparks.rise.mod, "Mod", "", I16, 1, 0x4000),
			FIRE_FIELD("sparks", sparks.scale.base, "Base", "", I16, 0, 0x7FFF),
			FIRE_FIELD("sparks", sparks.scale.mod, "Mod", "", I16, 1, 0x7FFF),
			FIRE_FIELD("sparks", sparks.sprite, "Sprite", "The shared effect sprite drawn", I16, 0, 31),
			FIRE_FIELD("sparks", sparks.last, "Last frame", "Its frame: this - its life", I16, 0, 200),
			FIRE_FIELD("sparks", sparks.flag20, "Header +0x20", "The sprite header's byte +0x20", I16, 0, 255),
			FIRE_FIELD("sparks", sparks.mode, "Draw mode", "The sprite header's draw mode (+0x24)", I16, 0, 0x7FFF),
			FIRE_FIELD("sparks", sparks.shade_shift, "Shade shift", "The camera offset it is drawn with: -(its size >> this)", I16, 0, 15),
			FIRE_FIELD("sparks", sparks.drag, "Drag shift", "x and z speeds slow by themselves >> this a tick, y's grows so", I16, 0, 15),
			FIRE_FIELD("sparks", sparks.shrink, "Shrink", "Its size shrinks by this a tick", I16, -0x4000, 0x4000),
		};
#undef FIRE_FIELD
	}

	void register_mag002_fire()
	{
		fxp::register_params({ 2, "Fire", &fire002::g_params, &fire002::DEFAULTS, sizeof(fire002::FireParams),
		                       fire002::PARTS, (int)(sizeof(fire002::PARTS) / sizeof(fire002::PARTS[0])),
		                       fire002::FIELDS, (int)(sizeof(fire002::FIELDS) / sizeof(fire002::FIELDS[0])),
		                       fire002::GROUPS, (int)(sizeof(fire002::GROUPS) / sizeof(fire002::GROUPS[0])) });
		register_port(fire002::ORIG_RootTask, (void *)fire002::RootTask, "F002 RootTask", 2);
		register_port(fire002::ORIG_DirectorMulti, (void *)fire002::DirectorMulti, "F002 DirectorMulti", 2);
		register_port(fire002::ORIG_DirectorSingle, (void *)fire002::DirectorSingle, "F002 DirectorSingle", 2);
		register_port(fire002::ORIG_FlameTask, (void *)fire002::FlameTask, "F002 FlameTask", 2);
		register_port(fire002::ORIG_SparkTask, (void *)fire002::SparkTask, "F002 SparkTask", 2);
		register_port(fire002::ORIG_EmberTask, (void *)fire002::EmberTask, "F002 EmberTask", 2);
		// 30 fps layer: see mag002_fire_held.inc
		FX_HELD(register_mag002_held();)
	}
	FX_LAB_MODULE(register_mag002_fire);
}

#ifdef FF8_FX_HELD
#include "mag002_fire_held.inc"
#endif
