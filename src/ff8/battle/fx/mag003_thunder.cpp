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

// Effect 3: Thunder (spell, MAG_003_*).
//
// Structure (setup MAG_003_THUNDER 0x700C10 -> _Init 0x700C20, file loader 0x575610 = nothing;
// the textures are uploaded by the setup from exe data 0x14FA790):
//   RootTask (0x700CA0) - alternates the packet arena (magic buffer + 0 / + 0x8000); every 5
//     ticks (counter 1) spawns the bolt of the next action on the action's first target (it
//     waits while a bolt of 15 frames or less is still running on that target), runs the bolt
//     queue, ends when the queue is empty after the first spawn.
//   Bolt (0x700DE0) - frame 0: full-screen flash quad; frames 1..24: the lightning strike, up to
//     8 sprite-sequence layers keyed on the frame (one of them on a tilted plane at the
//     target's feet); sound at frame 1, damage at frame 5, ends after frame 24.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2557000..0x2557098 (bolt pool 3 x 0x1C, texture base, context, root pool
// 1 x 0x14, packet cursor 0x2557074, queues 0x2557078).
// Built from the parts library (fx_bolt.h: the screen quad, the layers at the target's point, the
// ground layer). Parameters (ThunderParams, fx_params.h): the numbers above, defaults = the game's;
// the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_bolt.h"

namespace ff8fx
{
namespace thunder003
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline TaskQueuePair &Queues() { return var<TaskQueuePair>(0x2557078); } // .first = root, .second = bolts
	inline uint32_t &TexBase() { return var<uint32_t>(0x2557054); }          // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2557058); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2557074); }

	static const uint32_t ORIG_RootTask = 0x700CA0;
	static const uint32_t ORIG_BoltTask = 0x700DE0;
	static const uint32_t BOLT_POOL = 0x2557000;
	static const int BOLT_POOL_COUNT = 3;
	static const void *const SOUND_Thunder = (const void *)0x15244D0;
	static const void *const FLASH_Colour = (const void *)0x15244D4; // flash quad colour word
	static const uint32_t SEQ_Strike = 0x1522FB0;
	static const uint32_t SEQ_Upper = 0x1523204;
	static const uint32_t SEQ_Core = 0x1523458;
	static const uint32_t SEQ_Arc = 0x1523578;
	static const uint32_t SEQ_Glow = 0x1523684;
	static const uint32_t SEQ_Flash = 0x15237A4;
	static const uint32_t SEQ_Ground = 0x1523DE0;
	static const uint32_t SEQ_Smoke = 0x1523928;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct ThunderParams
	{
		struct Window { int16_t from, frames; };   // a layer drawn over bolt ticks from .. from + frames - 1, its frame = tick - from
		struct Timing
		{
			int16_t every;          // the root starts the next action's bolt every this many ticks...
			int16_t busy_tick;      // ...not while a bolt at this tick or before runs on that target (-1: never)
			int16_t bone;           // the target's effect point the bolt is at
			struct { int16_t base, shift; } offset;   // the layers' camera offset: (base - the target's size) >> shift
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the bolt's point...
			int16_t sound_volume;   // ...at this volume
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the bolt ends
		} timing;
		struct Flash
		{
			Window window;          // a flat semi-transparent quad over the whole screen
		} flash;
		struct Strike
		{
			int16_t rot;            // the layers' camera scale (4096: the sequences' own size)
			Window strike, upper;   // the strike, its upper copy...
			int16_t lift;           // ...that one lowered by this (y + lift)
			Window core, arc, glow, spark;
		} strike;
		struct Ground
		{
			Window window;          // a layer on a plane at the target's feet...
			int16_t tilt;           // ...turned about x...
			int16_t push;           // ...pushed this far from the camera
		} ground;
		struct Smoke
		{
			Window window;
		} smoke;
	};
	static const ThunderParams DEFAULTS = {
		{ 5, 0xF, 0xF1, { -0x1000, 2 }, (uint32_t)SOUND_Thunder, 1, 0x101, 5, 0x19 },
		{ { 0, 1 } },
		{ 0x1000, { 1, 7 }, { 3, 7 }, 0xC8, { 2, 9 }, { 4, 9 }, { 7, 9 }, { 2, 9 } },
		{ { 2, 13 }, 0x5A, 0x1F4 },
		{ { 6, 19 } },
	};
	static ThunderParams g_params = DEFAULTS;
	static inline const ThunderParams &P() { return g_params; }
	enum PartId { PART_FLASH = 1, PART_STRIKE = 2, PART_GROUND = 3, PART_SMOKE = 4 };

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..4, a bolt spawn at 1
		uint8_t action;   // +0x0E next action
		uint8_t started;  // +0x0F set at the first spawn tick
		uint32_t arena;   // +0x10 packet arena parity
	};
	struct BoltNode // pool of 3 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C frame
		int16_t action;   // +0x0E action index
		uint8_t *entity;  // +0x10 target entity
		int16_t pos[3];   // +0x14 effect anchor (x, y, z)
		int16_t size;     // +0x1A (-0x1000 - target size) / 4: the layers' camera offset (pos's 4th word)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(BoltNode) == 0x1C, "Thunder nodes");

	// Thunder_InitBoltTask (0x6DA380): rep stosd of `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (uint32_t k = 0; k < count; k++) d[k] = value;
	}

	// the frame of a window at `tick` (-1: not drawn)
	static inline int32_t FrameOf(const ThunderParams::Window &w, int32_t tick)
	{
		const int32_t e = tick - w.from;
		return (uint32_t)e < (uint32_t)w.frames ? e : -1;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag003_thunder_held.h"
#endif

namespace ff8fx
{
namespace thunder003
{
	// ------------------------------------------------------------------
	// Root task (0x700CA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		const ThunderParams::Timing &tp = P().timing;
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase();
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x8000;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			r->started = 1;
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a young bolt runs on this target
				BoltNode *pool = (BoltNode *)BOLT_POOL;
				bool busy = false;
				for (int k = 0; k < BOLT_POOL_COUNT && tp.busy_tick >= 0; k++)
					if ((pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= tp.busy_tick)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					BoltNode *b = (BoltNode *)AddTaskToQueue(&Queues().second, ORIG_BoltTask);
					FillDwords(&b->counter, 0, 4);
					b->action = (int16_t)(uint16_t)r->action;
					b->entity = entity;
					GetEffectSpawnPosition(entity, tp.bone, 0, b->pos);
					b->size = (int16_t)((tp.offset.base - (int32_t)*(int16_t *)(b->entity + 0x26)) >> tp.offset.shift);
					r->action++;
				}
			}
		}
		int alive = ExecuteTaskQueue(&Queues().second);
		if (r->started && !alive) return TASK_END;
		r->counter++;
		if (r->counter >= tp.every) r->counter = 0;
		return 0;
	}

	// ------------------------------------------------------------------
	// Bolt (0x700DE0)
	// ------------------------------------------------------------------
	static void BoltDraw(const BoltNode *p)
	{
		const ThunderParams &pp = P();
		const int32_t c = p->counter;
		int32_t f;
		FX_PART(PART_FLASH);
		if (FrameOf(pp.flash.window, c) >= 0) PacketCursor() = ScreenQuad(FLASH_Colour, 0x20, PacketCursor());
		// the stack slot of the lowered copy of the anchor is reused by the ground layer's vector
		int16_t local[4] = { 0, 0, 0, 0 };
		const ThunderParams::Strike &s = pp.strike;
		FX_PART(PART_STRIKE);
		if ((f = FrameOf(s.strike, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Strike, (int16_t)f, local, PacketCursor());
		if ((f = FrameOf(s.upper, c)) >= 0) PointLayer(p->pos, s.lift, s.rot, SEQ_Upper, (int16_t)f, local, PacketCursor());
		if ((f = FrameOf(s.core, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Core, (int16_t)f, local, PacketCursor());
		if ((f = FrameOf(s.arc, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Arc, (int16_t)f, local, PacketCursor());
		if ((f = FrameOf(s.glow, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Glow, (int16_t)f, local, PacketCursor());
		if ((f = FrameOf(s.spark, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Flash, (int16_t)f, local, PacketCursor());
		FX_PART(PART_GROUND);
		if ((f = FrameOf(pp.ground.window, c)) >= 0)
			GroundLayer(p->pos, p->entity, pp.ground.tilt, pp.ground.push, SEQ_Ground, (int16_t)f, local, PacketCursor());
		FX_PART(PART_SMOKE);
		if ((f = FrameOf(pp.smoke.window, c)) >= 0) PointLayer(p->pos, 0, s.rot, SEQ_Smoke, (int16_t)f, local, PacketCursor());
		FX_PART(0);
	}

	static uint32_t __cdecl BoltTask(TaskNode *n)
	{
		BoltNode *p = (BoltNode *)n;
		const ThunderParams::Timing &tp = P().timing;
		// (the sound plays before the layers)
		if (p->counter == tp.sound_tick) BdPlaySE3D((const void *)fxp::sound(tp.sound, (uint32_t)SOUND_Thunder), tp.sound_volume, p->pos);
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(held_note_draw(ORIG_BoltTask, p);)
		BoltDraw(p);
		if (p->counter == tp.damage_tick) ApplyActionResultToTarget(Ctx()->actions[p->action].targets);
		p->counter++;
		return p->counter < tp.end_tick ? 0 : TASK_END;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of ThunderParams
	// ------------------------------------------------------------------
	namespace thunder003
	{
		using fxp::Field;
#define THUNDER_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(ThunderParams, member), fxp::type, lo, hi }
#define THUNDER_WINDOW(part, member) \
		THUNDER_FIELD(part, member.from, "From tick", "", I16, 0, 200), THUNDER_FIELD(part, member.frames, "Frames", "", I16, 0, 200)
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one bolt per action, in turn (not while a young one runs on that target); each bolt's clock: its sound, its result, its end" },
			{ PART_FLASH, "flash", "Screen flash", "A flat semi-transparent quad over the whole screen" },
			{ PART_STRIKE, "strike", "Strike", "Sprite sequence layers at the target's effect point: the strike, its lowered copy, the core, the arc, the glow, the spark" },
			{ PART_GROUND, "ground", "Ground", "A sprite sequence on a plane at the target's feet" },
			{ PART_SMOKE, "smoke", "Smoke", "A sprite sequence at the target's effect point, after the strike" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing.offset", "Camera offset", "The layers' camera offset: (Base - the target's size) >> Shift" },
			{ "flash.window", "Drawn", "Over bolt ticks From tick .. From tick + Frames - 1" },
			{ "strike.strike", "Strike", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "strike.upper", "Lowered strike", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "strike.core", "Core", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "strike.arc", "Arc", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "strike.glow", "Glow", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "strike.spark", "Spark", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "ground.window", "Drawn", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
			{ "smoke.window", "Drawn", "Over bolt ticks From tick .. From tick + Frames - 1, its frame = the tick - From tick" },
		};
		static const Field FIELDS[] = {
			THUNDER_FIELD("timing", timing.every, "Every", "The root starts the next action's bolt every this many ticks", I16, 1, 100),
			THUNDER_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a bolt at this tick or before runs on that target (the cycle restarts; -1: never)", I16, -1, 200),
			THUNDER_FIELD("timing", timing.bone, "Bone", "The target's effect point the bolt is at (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			THUNDER_FIELD("timing", timing.offset.base, "Base", "", I16, -0x4000, 0x4000),
			THUNDER_FIELD("timing", timing.offset.shift, "Shift", "", I16, 0, 15),
			THUNDER_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			THUNDER_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the bolt's point when the bolt tick == this", I16, 0, 200),
			THUNDER_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			THUNDER_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the bolt tick == this", I16, 0, 200),
			THUNDER_FIELD("timing", timing.end_tick, "End tick", "The bolt ends when its tick reaches this", I16, 1, 200),
			THUNDER_WINDOW("flash", flash.window),
			THUNDER_FIELD("strike", strike.rot, "Size", "The layers' camera scale (4096: the sequences' own size)", I16, 0, 0x7FFF),
			THUNDER_WINDOW("strike", strike.strike),
			THUNDER_WINDOW("strike", strike.upper),
			THUNDER_FIELD("strike", strike.lift, "Lowered by", "The lowered strike is this much lower (y + this)", I16, -0x4000, 0x4000),
			THUNDER_WINDOW("strike", strike.core),
			THUNDER_WINDOW("strike", strike.arc),
			THUNDER_WINDOW("strike", strike.glow),
			THUNDER_WINDOW("strike", strike.spark),
			THUNDER_WINDOW("ground", ground.window),
			THUNDER_FIELD("ground", ground.tilt, "Tilt", "The plane turned about x by this (1024: flat)", I16, -0x1000, 0x1000),
			THUNDER_FIELD("ground", ground.push, "Push", "The plane pushed this far from the camera", I16, -0x4000, 0x4000),
			THUNDER_WINDOW("smoke", smoke.window),
		};
#undef THUNDER_WINDOW
#undef THUNDER_FIELD
	}

	void register_mag003_thunder()
	{
		fxp::register_params({ 3, "Thunder", &thunder003::g_params, &thunder003::DEFAULTS, sizeof(thunder003::ThunderParams),
		                       thunder003::PARTS, (int)(sizeof(thunder003::PARTS) / sizeof(thunder003::PARTS[0])),
		                       thunder003::FIELDS, (int)(sizeof(thunder003::FIELDS) / sizeof(thunder003::FIELDS[0])),
		                       thunder003::GROUPS, (int)(sizeof(thunder003::GROUPS) / sizeof(thunder003::GROUPS[0])) });
		register_port(thunder003::ORIG_RootTask, (void *)thunder003::RootTask, "T003 RootTask", 3);
		register_port(thunder003::ORIG_BoltTask, (void *)thunder003::BoltTask, "T003 BoltTask", 3);
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(register_mag003_held();)
	}
	FX_LAB_MODULE(register_mag003_thunder);
}

#ifdef FF8_FX_HELD
#include "mag003_thunder_held.inc"
#endif
