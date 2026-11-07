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

// Effect 106: Reflect (spell, MAG_106_*). The same effect as the "ruby" part of Carbuncle's
// Ruby Light (mag278: RubyTask 0x6822F0 and its stones / rings / wall), in its own copy.
//
// Structure (setup MAG_106_REFLECT 0x6D2AE0 -> _Init 0x6D2AF0, file loader 0x6D2AC0 = the texture
// file named at 0x13009B4 into 0x2521F70). _Init keeps the cast context, sets up the root pool and
// the barrier / beam / hexagon / ring pools (module .data), measures the reflect wall model
// 0x12F9B80 (lowest vertex z -> 0x2523C7E, z span + 1/16 -> 0x2523C7C), starts camera animation
// 0x12FC200 and queues the TIM (unless cast flag bit0), seeds the shared LCG 0x2557098 with rand().
//   DirectorTask (0x6D2C00) - alternates the packet arena (0x2523C98 / 0x2533C98); on counter 0
//     of a 10-tick cycle starts a barrier on the first target of the next action, unless a
//     barrier of 35 ticks or less still runs on that target (the cycle then restarts), with 8
//     hexagons around it; runs the barrier, hexagon and ring queues and the beam list; ends when
//     all four are empty.
//   Barrier (0x6D3160) - node 0x5C, 41 ticks: the target's Y rotation scaled by its size composed
//     with the camera; ticks 0..19: the hexagons' centre moves along the wall model's vertex list
//     and 12 beams are "spawned" (see AppendByValue: never run); ticks 5..39: the reflect wall
//     (0x12F9B80, drawn by 0x6D3490 = textured quads, gouraud triangles, gouraud quads, all with
//     a V scroll) grows over 5..25 (vertices of z in [base, base + span * (t + 1) / 20]) and fades
//     to black over 26..39; rings at 5, 11, 17; sound at 0, damage at 35.
//   Hexagon (0x6D2F30) - node 0x1C, 17 ticks: prim model 0x12FBAA0 at `angle` around the centre,
//     Z-rotated by -angle, tilted about X, squashed in x and faded from tick 11.
//   Ring (0x6D3AF0) - node 0x1C, 21 ticks: prim model 0x12FBB78 at a height above the target,
//     scaled up (braking speed), fade sin(t * 1024 / 20).
//   Beam (0x6D3C50) - node 0x1C, sprite sequence 0x12F99D4 through 0x683490: vanilla dead code
//     (the beam list is never linked, see AppendByValue).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521BB8..0x2543CF0 (pools, texture file, context, wall base / span, root pool,
// beam header, both packet arenas, packet cursor 0x2543C98, queues). The shared effect LCG seed
// 0x2557098 (MAG_106_sub_7059E0) is outside.
// Helpers of other modules called here are local copies: MAG_103_sub_6DBCC0 (yaw x scale),
// MAG_011_sub_6CF070 (Z rotation), MAG_003_sub_6D5940 (X rotation), Thunder_InitBoltTask
// 0x6DA380 (dword fill), MAG_106_sub_705A00 (append by value), MAG_106_sub_6827B0 /
// MAG_106_sub_682D00 (Carbuncle's wall renderers, shared with this module's 0x6D34E0).
// Built from the parts library (fx_barrier.h): the barrier's frame (BarrierFrame), the hexagons'
// climbing centre (BarrierCenter), the scrolling wall (WallDraw, WallHeight, WallFade, its height
// range measured from the model as _Init does), the orbiting hexagons (OrbitDraw), the growing
// rings (GrowthDraw, GrowthStep), the dead beams (DeadBeams, DeadBeamsRun: their nodes and the list's
// sprite header are written as the original does, the beam task is not ported, it never runs).
// Parameters (ReflectParams, fx_params.h): the numbers above, defaults = the game's; the parts are
// tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include "fx_barrier.h"

namespace ff8fx
{
namespace reflect106
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2523B78); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2543C98); }
	inline uint8_t *&BeamHeader() { return var<uint8_t *>(0x2523C94); }     // sprite header of the beam list (per tick)
	inline TaskQueue *QBarriers() { return (TaskQueue *)0x2543CB0; }        // pool 0x2521BB8, 3 x 0x5C
	inline TaskQueue *QBeams() { return (TaskQueue *)0x2543CC0; }           // pool 0x2521F78, 0x100 x 0x1C (never linked)
	inline TaskQueue *QHexagons() { return (TaskQueue *)0x2543CD0; }        // pool 0x2521CD0, 0x18 x 0x1C
	inline TaskQueue *QRings() { return (TaskQueue *)0x2543CE0; }           // pool 0x2523B80, 9 x 0x1C
	static const uint32_t POOL_Barriers = 0x2521BB8;
	static const uint32_t ARENA_0 = 0x2523C98;
	static const uint32_t ARENA_1 = 0x2533C98;

	static const uint32_t ORIG_DirectorTask = 0x6D2C00;
	static const uint32_t ORIG_HexagonTask = 0x6D2F30;
	static const uint32_t ORIG_BarrierTask = 0x6D3160;
	static const uint32_t ORIG_RingTask = 0x6D3AF0;
	static const uint32_t ORIG_BeamTask = 0x6D3C50;   // stored in the beams' nodes, never run
	static const uint32_t SEQ_Beam = 0x12F99D4;
	static const uint32_t MODEL_Wall = 0x12F9B80;     // +4 vertex count, +8 vertices (8 bytes: x, y, z, pad)
	static const uint32_t MODEL_Hexagon = 0x12FBAA0;
	static const uint32_t MODEL_Ring = 0x12FBB78;
	static const uint32_t TABLE_SinCos = 0x12FC9B4;   // 0x1000 x { s16 sin, s16 cos }
	static const void *const SOUND_Reflect = (const void *)0x12FC9B0;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct ReflectParams
	{
		struct Timing
		{
			int16_t every;          // the director starts the next action's barrier every this many ticks...
			int16_t busy_tick;      // ...unless a barrier on that target is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (barrier tick)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the barrier ends once its tick reaches this
			int16_t size_shift;     // its frame scaled by min(the target's size << this, 4096)
		} timing;
		struct Wall
		{
			int16_t from, ticks;    // drawn over from .. from + ticks - 1...
			int16_t grow;           // ...growing up over its first `grow` + 1 steps...
			int16_t fade;           // ...then fading out over this many
		} wall;
		struct Hexagons
		{
			int16_t count;          // spawned with the barrier, around the climbing centre
			int16_t climb_ticks;    // the centre climbs the wall's vertices over the barrier's first ticks...
			int16_t climb_steps;    // ...this many vertices
			int16_t tilt;           // their first X rotation
			int16_t angle_step;     // their first angles apart
			int16_t turn;           // their turn a tick (its side random)
			struct { int16_t base, mask; } tilt_speed;   // their tilt a tick: base - (rand & mask)
			int16_t end_tick;       // a hexagon ends once its tick reaches this
			int16_t fade_from, fade_ticks;   // squashed and faded from this tick, over this many
		} hexagons;
		struct Rings
		{
			int16_t every;          // one every few wall steps from the wall's first...
			int16_t last_tick;      // ...to this barrier tick
			int16_t scale, speed, accel, min_speed;   // its first size, growth, brake, least growth
			int16_t end_tick;       // a ring ends once its tick reaches this
			int16_t fade_ticks;     // faded in and out (a half sine) over this many
		} rings;
		struct Beams
		{
			int16_t count;          // the dead beams: this many a tick while the centre climbs (each draws a shared rand while their pool has room)
		} beams;
	};
	static const ReflectParams DEFAULTS = {
		{ 10, 0x23, (uint32_t)SOUND_Reflect, 0, 0x101, 0x23, 0x28, 2 },
		{ 5, 0x23, 0x14, 15 },
		{ 8, 0x14, 15, 0x400, 0x200, 0x10, { -0x25, 0x1F }, 0x10, 0xA, 6 },
		{ 6, 0x16, 0x800, 0x100, -8, 0x10, 0x14, 20 },
		{ 12 },
	};
	static ReflectParams g_params = DEFAULTS;
	static inline const ReflectParams &P() { return g_params; }
	enum PartId { PART_WALL = 1, PART_HEXAGONS = 2, PART_RINGS = 3 };

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct DirectorNode // root pool 0x2523C80, 1 x 0x14
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..every - 1, a barrier spawn at 0
		uint8_t action;   // +0x0E next action
		uint8_t pad0F;
		uint32_t arena;   // +0x10 packet arena parity
	};
	struct HexagonNode // pool 0x2521CD0, 0x18 x 0x1C
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		Orbit o;          // +0x0E
	};
	struct RingNode // pool 0x2523B80, 9 x 0x1C
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		Growth g;         // +0x0E
	};
#pragma pack(pop)
	static_assert(sizeof(DirectorNode) == 0x14, "Reflect nodes");
	static_assert(sizeof(HexagonNode) == 0x1C && sizeof(RingNode) == 0x1C, "Reflect nodes");

	// Thunder_InitBoltTask (0x6DA380): fill `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (; count; count--) *d++ = value;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag106_reflect_held.h"
#endif

namespace ff8fx
{
namespace reflect106
{
	// ------------------------------------------------------------------
	// Hexagon (0x6D2F30): 17 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl HexagonTask(TaskNode *n)
	{
		HexagonNode *x = (HexagonNode *)n;
		const ReflectParams::Hexagons &hp = P().hexagons;
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_hexagon(x);)
		FX_PART(PART_HEXAGONS);
		OrbitDraw(MODEL_Hexagon, &x->o, x->counter, hp.fade_from, hp.fade_ticks, 0xF3, TABLE_SinCos, PacketCursor());
		FX_PART(0);
		if (x->counter >= hp.end_tick) return TASK_END;
		x->o.angle = (int16_t)(x->o.angle + x->o.dangle);
		x->o.tilt = (int16_t)(x->o.tilt + x->o.dtilt);
		x->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x6D3AF0): 21 ticks, the growth speed brakes to 0x10
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		const ReflectParams::Rings &rp = P().rings;
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_ring(g);)
		FX_PART(PART_RINGS);
		GrowthDraw(MODEL_Ring, &g->g, g->counter, rp.fade_ticks, 0xF3, PacketCursor());
		FX_PART(0);
		if (g->counter >= rp.end_tick) return TASK_END;
		GrowthStep(&g->g, rp.min_speed);
		g->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Barrier (0x6D3160), one per action, 41 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl BarrierTask(TaskNode *n)
	{
		BarrierNode *b = (BarrierNode *)n;
		const ReflectParams &pp = P();
		const int32_t scale = BarrierFrame(&b->b, b->entity, pp.timing.size_shift);
		if ((uint32_t)(int32_t)b->frame < (uint32_t)pp.hexagons.climb_ticks)
		{
			BarrierCenter(MODEL_Wall, b->frame, pp.hexagons.climb_steps, pp.hexagons.climb_ticks, b->b.center);
			// the beams: vanilla dead code (their nodes never linked: see fx_barrier.h DeadBeams)
			DeadBeams(QBeams(), ORIG_BeamTask, pp.beams.count, b);
		}
		const int32_t e = b->frame - pp.wall.from;
		if ((uint32_t)e < (uint32_t)pp.wall.ticks)
		{
			// 30 fps layer: see mag106_reflect_held.inc
			FX_HELD(held_note_wall(b, e);)
			const int32_t height = WallHeight(MODEL_Wall, e, pp.wall.grow);
			FX_PART(PART_WALL);
			WallDraw(MODEL_Wall, b->frame, height, WallFade(e, pp.wall.grow, pp.wall.fade), &b->b.view, PacketCursor());
			FX_PART(0);
			if (b->frame <= pp.rings.last_tick && (uint32_t)e % (uint32_t)pp.rings.every == 0)
			{
				RingNode *g = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
				if (g)
				{
					FillDwords(&g->counter, 0, 4);
					g->g.parent = b;
					g->g.height = (int16_t)(mul32((int16_t)height, scale) >> 12);
					g->g.scale = pp.rings.scale;
					g->g.speed = pp.rings.speed;
					g->g.accel = pp.rings.accel;
				}
			}
		}
		if (b->frame == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(b->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Reflect), pp.timing.sound_volume, pos);
		}
		if (b->frame == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[b->action].targets);
		if (b->frame >= pp.timing.end_tick) return TASK_END;
		b->frame++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x6D2C00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		const ReflectParams &pp = P();
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(held_note_director();)
		if (d->arena)
		{
			PacketCursor() = ARENA_0;
			d->arena = 0;
		}
		else
		{
			PacketCursor() = ARENA_1;
			d->arena = 1;
		}
		if (d->counter == 0)
		{
			ActionData *acts = Ctx()->actions;
			if (d->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[d->action].targets[0]);
				// wait while a barrier at busy_tick or before runs on this target
				BarrierNode *pool = (BarrierNode *)POOL_Barriers;
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].frame <= pp.timing.busy_tick)
					{
						busy = true;
						break;
					}
				if (busy) d->counter = 0;
				else
				{
					// (no null test in the original: the pool holds 3 barriers)
					BarrierNode *b = (BarrierNode *)AddTaskToQueue(QBarriers(), ORIG_BarrierTask);
					FillDwords(&b->frame, 0, 0x14);
					b->action = (int16_t)(uint16_t)d->action;
					b->entity = entity;
					int16_t c[4];
					EffectCentre(entity, c);
					b->b.model.t[0] = c[0];
					b->b.model.t[1] = c[1];
					b->b.model.t[2] = c[2];
					d->action++;
					const ReflectParams::Hexagons &hp = pp.hexagons;
					int32_t angle = SharedRand();
					for (int i = hp.count; i > 0; i--)
					{
						HexagonNode *x = (HexagonNode *)AddTaskToQueue(QHexagons(), ORIG_HexagonTask);
						if (!x) continue;
						FillDwords(&x->counter, 0, 4);
						x->o.parent = b;
						x->o.tilt = hp.tilt;
						x->o.angle = (int16_t)angle;
						x->o.dangle = hp.turn;
						if (SharedRand() & 1) x->o.dangle = (int16_t)-x->o.dangle;
						x->o.dtilt = (int16_t)(hp.tilt_speed.base - (SharedRand() & hp.tilt_speed.mask));
						angle += hp.angle_step;
					}
				}
			}
		}
		const int barriers = ExecuteTaskQueue(QBarriers());
		const int hexagons = ExecuteTaskQueue(QHexagons());
		const int rings = ExecuteTaskQueue(QRings());
		DeadBeamsRun(BeamHeader(), SEQ_Beam, QBeams());
		if (barriers == 0 && hexagons == 0 && rings == 0) return TASK_END;
		d->counter++;
		if (d->counter >= pp.timing.every) d->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of ReflectParams
	// ------------------------------------------------------------------
	namespace reflect106
	{
		using fxp::Field;
#define REFLECT_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(ReflectParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The director starts one barrier per action, in turn (not while a young one runs on that target), with its hexagons; each barrier's clock: its frame on the target, its sound, its result, its end; the dead beams (draws nothing)" },
			{ PART_WALL, "wall", "Wall", "The wall model growing up through the barrier's frame, its texture scrolling, then fading out" },
			{ PART_HEXAGONS, "hexagons", "Hexagons", "Hexagon models orbiting a point that climbs the wall, tilting, then squashed and faded" },
			{ PART_RINGS, "rings", "Rings", "Ring models growing above the target, one every few wall steps" },
		};
		static const fxp::Group GROUPS[] = {
			{ "hexagons.tilt_speed", "Tilt speed", "Their tilt a tick: Base - (rand & Mask)" },
		};
		static const Field FIELDS[] = {
			REFLECT_FIELD("timing", timing.every, "Every", "The director starts the next action's barrier every this many ticks", I16, 1, 100),
			REFLECT_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a barrier on that target is at this tick or before (the cycle restarts; -1: never)", I16, -1, 200),
			REFLECT_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			REFLECT_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the barrier tick == this", I16, 0, 200),
			REFLECT_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			REFLECT_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the barrier tick == this", I16, 0, 200),
			REFLECT_FIELD("timing", timing.end_tick, "End tick", "The barrier ends when its tick reaches this", I16, 0, 200),
			REFLECT_FIELD("timing", timing.size_shift, "Size shift", "Its frame scaled by min(the target's size << this, 4096)", I16, 0, 12),
			REFLECT_FIELD("timing", beams.count, "Dead beams", "Vanilla dead code: beams allocated a tick while the centre climbs, each drawing a shared random number while their pool has room (never drawn)", I16, 0, 64),
			REFLECT_FIELD("wall", wall.from, "From tick", "The wall is drawn from this barrier tick...", I16, 0, 200),
			REFLECT_FIELD("wall", wall.ticks, "Ticks", "...for this many ticks", I16, 0, 200),
			REFLECT_FIELD("wall", wall.grow, "Grow steps", "It grows up over its first steps (this many + 1)...", I16, 1, 200),
			REFLECT_FIELD("wall", wall.fade, "Fade steps", "...then fades out over this many", I16, 1, 200),
			REFLECT_FIELD("hexagons", hexagons.count, "Count", "Hexagons spawned with each barrier", I16, 0, 24),
			REFLECT_FIELD("hexagons", hexagons.climb_ticks, "Climb ticks", "Their centre climbs the wall's vertices over the barrier's first ticks...", I16, 1, 200),
			REFLECT_FIELD("hexagons", hexagons.climb_steps, "Climb steps", "...this many vertices", I16, 0, 64),
			REFLECT_FIELD("hexagons", hexagons.tilt, "Tilt", "Their first X rotation (4096 = a turn)", I16, -0x1000, 0x1000),
			REFLECT_FIELD("hexagons", hexagons.angle_step, "Angle step", "Their first angles apart around the centre", I16, -0x1000, 0x1000),
			REFLECT_FIELD("hexagons", hexagons.turn, "Turn", "Their turn a tick (its side random)", I16, -0x1000, 0x1000),
			REFLECT_FIELD("hexagons", hexagons.tilt_speed.base, "Base", "", I16, -0x1000, 0x1000),
			REFLECT_FIELD("hexagons", hexagons.tilt_speed.mask, "Mask", "", I16, 0, 0xFFF),
			REFLECT_FIELD("hexagons", hexagons.end_tick, "End tick", "A hexagon ends when its tick reaches this", I16, 0, 200),
			REFLECT_FIELD("hexagons", hexagons.fade_from, "Fade from", "Squashed and faded after this tick...", I16, 0, 200),
			REFLECT_FIELD("hexagons", hexagons.fade_ticks, "Fade ticks", "...over this many", I16, 1, 200),
			REFLECT_FIELD("rings", rings.every, "Every", "A ring every this many wall steps from the wall's first tick...", I16, 1, 200),
			REFLECT_FIELD("rings", rings.last_tick, "Last tick", "...to this barrier tick", I16, -1, 200),
			REFLECT_FIELD("rings", rings.scale, "Size", "Its first size (4096 = its own)", I16, 0, 0x7FFF),
			REFLECT_FIELD("rings", rings.speed, "Growth", "Its first growth a tick", I16, -0x4000, 0x4000),
			REFLECT_FIELD("rings", rings.accel, "Brake", "Added to its growth a tick...", I16, -0x1000, 0x1000),
			REFLECT_FIELD("rings", rings.min_speed, "Least growth", "...down to this", I16, -0x1000, 0x1000),
			REFLECT_FIELD("rings", rings.end_tick, "End tick", "A ring ends when its tick reaches this", I16, 0, 200),
			REFLECT_FIELD("rings", rings.fade_ticks, "Fade ticks", "Faded in and out (a half sine) over this many", I16, 1, 200),
		};
#undef REFLECT_FIELD
	}

	void register_mag106_reflect()
	{
		fxp::register_params({ 106, "Reflect", &reflect106::g_params, &reflect106::DEFAULTS, sizeof(reflect106::ReflectParams),
		                       reflect106::PARTS, (int)(sizeof(reflect106::PARTS) / sizeof(reflect106::PARTS[0])),
		                       reflect106::FIELDS, (int)(sizeof(reflect106::FIELDS) / sizeof(reflect106::FIELDS[0])),
		                       reflect106::GROUPS, (int)(sizeof(reflect106::GROUPS) / sizeof(reflect106::GROUPS[0])) });
		register_port(reflect106::ORIG_DirectorTask, (void *)reflect106::DirectorTask, "R106 DirectorTask", 106);
		register_port(reflect106::ORIG_BarrierTask, (void *)reflect106::BarrierTask, "R106 BarrierTask", 106);
		register_port(reflect106::ORIG_HexagonTask, (void *)reflect106::HexagonTask, "R106 HexagonTask", 106);
		register_port(reflect106::ORIG_RingTask, (void *)reflect106::RingTask, "R106 RingTask", 106);
		// 30 fps layer: see mag106_reflect_held.inc
		FX_HELD(register_mag106_held();)
	}
	FX_LAB_MODULE(register_mag106_reflect);
}

#ifdef FF8_FX_HELD
#include "mag106_reflect_held.inc"
#endif
