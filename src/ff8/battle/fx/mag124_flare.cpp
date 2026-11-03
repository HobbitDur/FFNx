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

// Effect 124: Flare (spell, MAG_124_*).
//
// Structure (setup MAG_124_FLARE 0x6BBDA0 -> _Init 0x6BBDD0, file loader 0x6BBDB0 = the texture
// file named at 0x12652A4; the setup starts camera animation 0x125D0C0 and queues the TIM):
//   RootTask (0x6BBE40) - alternates the packet arena (magic buffer + 0x30BC / + 0x1B0BC), clears
//     the screen flash level, and on counter 1 of a 10-tick cycle sets up the pools (first time)
//     and spawns the target task of the next action, unless a target task younger than 61 ticks
//     still runs on that target (the cycle then restarts); computes the effect camera, runs the
//     target and particle queues, sets the screen flash; ends when both queues are empty.
//   Target (0x6BC040) - node 0xB44, one per action (on its first target): sound at 0; screen
//     flash ramp (up over 0..7, 0x800, down from 77); fades at 39 and 41; the target's colour
//     blends to red over 4..35 and back over 72..80; plays two prim-model layouts (the burst
//     0x1252C0C at the target, the ground ring 0x125C5C4 at its feet) with the prim-player
//     callback PrimPart (0x6BC4A0); over 40..79 hides the target and draws it itself as a
//     rippling red silhouette (0x6BC8A0); damage at 80; at 0..7 spawns 6 particles per tick; ends
//     when both layouts are over (the target is shown again).
//   Particle (0x6BD020) - node 0x2C: the shared burst sprite (0x1252BE4) on a sphere around the
//     target (radius `size` along a direction turning by a random angular velocity): it shows at
//     its spawn radius, collapses into the centre at a random time, bursts out again from tick 41
//     and fades out from tick 56.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25213D0..0x2521448 (flash level, texture file, magic buffer base, context,
// root pool, packet cursor 0x25213F4, effect camera 0x25213F8, particle / target / root queues).
// The target pool (3 x 0xB44), the particle pool (64 x 0x2C), the morph vertex buffer (+0x2CCC)
// and the packet arenas are in the magic buffer. The silhouette draws into the frame arena
// (battle_texture_data_ptr 0x1D8E054) with the battle's bone workspace (0x1D98B3C) as its
// vertex buffer.
// Built from the parts library (fx_layouts.h, fx_ripple.h): the anchor callback (AnchorPrimCallback,
// the record flags 0x40000, 0x1000 with the effect camera, 0x200 and 0x100 honoured, scaled by the
// target's size, objects 2 and 3 at a quarter depth), ScreenFlashRiseHoldFall, TintEntity, the
// ripple silhouette (RippleSilhouetteTick), the collapsing sparks (CollapseSpark*). Parameters
// (FlareParams, fx_params.h): the numbers above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include "fx_ripple.h"

namespace ff8fx
{
namespace flare124
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline int32_t &FlashLevel() { return var<int32_t>(0x25213D0); }       // screen flash, max of the targets
	inline uint32_t &TexBase() { return var<uint32_t>(0x25213D8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25213DC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25213F4); }
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x25213F8); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521418; }       // pool: + 0x21CC, 0x40 x 0x2C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521428; }         // pool: magic buffer, 3 x 0xB44

	static const uint32_t ORIG_RootTask = 0x6BBE40;
	static const uint32_t ORIG_TargetTask = 0x6BC040;
	static const uint32_t ORIG_ParticleTask = 0x6BD020;
	static const uint32_t MODEL_Burst = 0x1252C0C;       // prim-model layout data (0xA70-byte layout)
	static const uint32_t MODEL_Ring = 0x125C5C4;        // prim-model layout data (0xA8-byte layout)
	static const uint32_t SEQ_Particle = 0x1252BE4;
	static const void *const SOUND_Flare = (const void *)0x125DE9C;
	static const void *const COLOUR_Red = (const void *)0x12652A0;   // 0x002020FF
	static const RippleTables RIPPLE = RippleTablesAt(0x125DEA0);   // distance u8 [128][128], weights u16 [20][128], sine s16 [4096]
	static const uint32_t HONOURED = ANCHOR_ROT_YXZ | ANCHOR_MATRIX | ANCHOR_OFFSET_AS_IS | ANCHOR_DIAGONAL_SCALE;   // the record flags PrimPart tests

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct FlareParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks...
			int16_t busy_tick;      // ...unless a target task on that target is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
		} timing;
		struct Flash
		{
			int16_t rise, up;       // c x up before `rise`...
			int16_t level;          // ...then this level (a floor)...
			int16_t fall_after, end, down;   // ...then (end - c) x down after `fall_after`
			int16_t fade_tick;      // a screen fade (0, 1, 0, 0x60) at this tick
			int16_t set_tick, set_level;   // at this tick the level is set to `set_level`, and a second fade (0, 1, 1, 0xC0)
		} flash;
		struct Tint
		{
			int16_t from, ticks, shift;           // the target to red over from .. from + ticks - 1, weight (c - from) << shift
			int16_t back_from, back_ticks, back_shift;   // back over back_from .. back_from + back_ticks - 1
		} tint;
		struct Burst
		{
			struct { int16_t base, shift; } depth;  // depth = (base - the target's size word +0x26) >> shift (objects 2 and 3: >> 2 more)
			struct { int16_t base, shift; } scale;  // both layouts scaled by ((the target's size >> 1) + base) >> shift
		} burst;
		struct Silhouette
		{
			int16_t from, ticks;    // the target hidden and redrawn rippled over from .. from + ticks - 1
		} silhouette;
		struct Sparks
		{
			int16_t last_tick;      // spawned at target ticks 0 .. last...
			int16_t count;          // ...this many a tick
			int16_t window;         // each collapses at a random tick in a window this long
			int16_t speed;          // its direction turns this much a tick
			int16_t sprite;         // its sprite's size (4096 = its own)
			int16_t burst, fade;    // it bursts out from this tick, fades from this one
		} sparks;
	};
	static const FlareParams DEFAULTS = {
		{ 10, 0x3C, (uint32_t)SOUND_Flare, 0, 1, 0x50 },
		{ 8, 0x100, 0x800, 0x4C, 0x50, 0x200, 0x27, 0x29, 0x400 },
		{ 4, 0x20, 7, 0x48, 9, 9 },
		{ { -0x800, 3 }, { 0x800, 0 } },
		{ 0x28, 0x28 },
		{ 7, 6, 20, 0x32, 0x4B0, 0x29, 0x38 },
	};
	static FlareParams g_params = DEFAULTS;
	static inline const FlareParams &P() { return g_params; }
	enum PartId { PART_BURST = 1, PART_RING = 2, PART_SILHOUETTE = 3, PART_SPARKS = 4 };

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
	struct TargetNode // pool of 3 nodes of 0xB44 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		int16_t spawn[4];  // +0x1C effect anchor 0xF0 (GetEffectSpawnPosition; not read)
		uint32_t colour;   // +0x24 the target's colour before the effect
		int32_t radius;    // +0x28 silhouette: largest ripple offset of the last draw (at most 0x80)
		uint8_t burst[0xA70];  // +0x2C prim-model layout (prim::Layout: data, frame, state)
		uint8_t ring[0xA8];    // +0xA9C
	};
	struct ParticleNode // pool of 0x40 nodes of 0x2C bytes
	{
		TaskNode hdr;
		CollapseSpark spark; // +0x0C
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0xB44 && sizeof(ParticleNode) == 0x2C, "Flare nodes");

	// an object bit (-1: none)
	static inline uint32_t ObjectBit(int16_t object) { return (uint16_t)object < 32 ? 1u << object : 0; }
}
}

#ifdef FF8_FX_HELD
#include "mag124_flare_held.h"
#endif

namespace ff8fx
{
namespace flare124
{
	// ------------------------------------------------------------------
	// Particle (0x6BD020)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		const FlareParams &pp = P();
		const int mode = CollapseSparkPrepare(&p->spark, pp.sparks.burst, pp.sparks.fade);
		if (mode == -2) return TASK_END;
		if (mode >= 0)
		{
			// 30 fps layer: see mag124_flare_held.inc
			FX_HELD(held_note_particle(p, mode);)
			FX_PART(PART_SPARKS);
			CollapseSparkDraw(&p->spark, mode, SEQ_Particle, PacketCursor());
			FX_PART(0);
		}
		CollapseSparkMove(&p->spark);
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6BC040)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const FlareParams &pp = P();
		if (t->counter == pp.timing.sound_tick) BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Flare), pp.timing.sound_volume, t->pos);
		const FlareParams::Flash &f = pp.flash;
		const int32_t v = ScreenFlashRiseHoldFall(t->counter, f.rise, f.up, f.level, f.fall_after, f.end, f.down);
		if (v > FlashLevel()) FlashLevel() = v;
		if (t->counter == f.fade_tick) ScreenFadeTask(0, 1, 0, 0x60);
		else if (t->counter == f.set_tick)
		{
			FlashLevel() = f.set_level;
			ScreenFadeTask(0, 1, 1, 0xC0);
		}
		*(uint16_t *)t->entity |= 0x800;
		// the target's colour: to red, red, back
		const FlareParams::Tint &tn = pp.tint;
		TintEntity(t->entity, COLOUR_Red, &t->colour, t->counter, tn.from, tn.ticks, tn.shift, tn.back_from, tn.back_ticks, tn.back_shift);
		// burst at the anchor (scaled by the target's size), ring at its feet
		AnchorArg arg;
		arg.morph = TexBase() + 0x2CCC;
		const int32_t scale = SizeScale(t->entity, pp.burst.scale.base, pp.burst.scale.shift);
		memcpy(arg.anchor, t->pos, 8);
		arg.scaled = 1;
		arg.scale[2] = scale;
		arg.scale[1] = scale;
		arg.scale[0] = scale;
		arg.depth = DepthFromSize(t->entity, pp.burst.depth.base, pp.burst.depth.shift);
		arg.honoured = HONOURED;
		arg.cursor = &PacketCursor();
		arg.matrix = &EffectCamera();
		arg.far_objects = ObjectBit(2) | ObjectBit(3);
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_play(t, t->burst, &arg);)
		FX_PART(PART_BURST);
		int alive = PlayAtAnchor(t->burst, &arg);
		arg.anchor[2] = t->pos[2];
		arg.anchor[0] = t->pos[0];
		arg.anchor[1] = *(const int16_t *)(t->entity + 0x24);
		arg.depth = 0;
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_play(t, t->ring, &arg);)
		FX_PART(PART_RING);
		alive |= PlayAtAnchor(t->ring, &arg);
		FX_PART(PART_SILHOUETTE);
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(if ((uint32_t)((int32_t)t->counter - pp.silhouette.from) < (uint32_t)pp.silhouette.ticks) held_note_silhouette(t, (int32_t)t->counter - pp.silhouette.from);)
		RippleSilhouetteTick(t->entity, t->pos, (int32_t)t->counter - pp.silhouette.from, pp.silhouette.ticks, &t->radius, RIPPLE);
		FX_PART(0);
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		const int32_t cnt = t->counter;
		if ((uint32_t)cnt <= (uint32_t)pp.sparks.last_tick)
		{
			int left = pp.sparks.count;
			ParticleNode *p = left > 0 ? (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask) : nullptr;
			while (p)
			{
				CollapseSparkStart(&p->spark, t->pos, cnt, arg.scale[0], pp.sparks.window, pp.sparks.speed, pp.sparks.sprite);
				if (--left == 0) break;
				p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			}
		}
		if (!alive)
		{
			*(uint16_t *)t->entity &= 0xF7FF;
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BBE40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x30BC;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1B0BC;
			r->arena = 1;
		}
		FlashLevel() = 0;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0xB44, 3);
				InitTaskQueuePool(QParticles(), (void *)(TexBase() + 0x21CC), 0x2C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				uint32_t colour = *(const uint32_t *)(entity + 0x28);
				// wait while a target task at busy_tick or before runs on this target; an older one
				// hands over the colour the target had before it
				TargetNode *pool = (TargetNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity)
					{
						if (pool[k].counter <= P().timing.busy_tick)
						{
							busy = true;
							break;
						}
						colour = pool[k].colour;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					Memset32(&t->counter, 0, 0x2CE);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					t->colour = colour;
					GetDefaultEffectPosition(entity, t->pos);
					GetEffectSpawnPosition(t->entity, 0xF0, 0, t->spawn);
					DecodeModelPrimLayout(MODEL_Burst, t->burst, 0xA70);
					DecodeModelPrimLayout(MODEL_Ring, t->ring, 0xA8);
					r->action++;
				}
			}
		}
		EffectCameraMatrix(&Camera(), &EffectCamera());
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
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of FlareParams
	// ------------------------------------------------------------------
	namespace flare124
	{
		using fxp::Field;
#define FLARE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(FlareParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn (not while a young one runs on that target); each target task's clock: its sound, the screen flash and fades, the target's red tint, its result; it ends when both layouts have finished" },
			{ PART_BURST, "burst", "Burst", "The burst layout at the target's effect position, scaled by its size" },
			{ PART_RING, "ring", "Ring", "The ring layout at the target's feet" },
			{ PART_SILHOUETTE, "silhouette", "Silhouette", "The target hidden and redrawn rippled around its effect position" },
			{ PART_SPARKS, "sparks", "Sparks", "Sprites on a sphere around the target that collapse into it, then burst out" },
		};
		static const fxp::Group GROUPS[] = {
			{ "burst.depth", "Depth", "Its drawing depth: (Base - the target's size word) >> Shift (objects 2 and 3: >> 2 more)" },
			{ "burst.scale", "Size", "Both layouts scaled by ((the target's size >> 1) + Base) >> Shift (4096 = their own size); the sparks' radius too" },
		};
		static const Field FIELDS[] = {
			FLARE_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			FLARE_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a target task on that target is at this tick or before (the cycle restarts; -1: never)", I16, -1, 200),
			FLARE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			FLARE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			FLARE_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			FLARE_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			FLARE_FIELD("timing", flash.rise, "Flash rise", "The screen flash rises (Up a tick) before this tick...", I16, 0, 200),
			FLARE_FIELD("timing", flash.up, "Flash up", "", I16, 0, 0x7FFF),
			FLARE_FIELD("timing", flash.level, "Flash level", "...stays at least at this level...", I16, 0, 0x7FFF),
			FLARE_FIELD("timing", flash.fall_after, "Flash fall after", "...and falls (Down a tick) to 0 at End after this tick", I16, 0, 200),
			FLARE_FIELD("timing", flash.end, "Flash end", "", I16, 0, 200),
			FLARE_FIELD("timing", flash.down, "Flash down", "", I16, 0, 0x7FFF),
			FLARE_FIELD("timing", flash.fade_tick, "Fade tick", "A screen fade starts at this tick", I16, -1, 200),
			FLARE_FIELD("timing", flash.set_tick, "Flash set tick", "At this tick the flash level is set (and a second fade starts)...", I16, -1, 200),
			FLARE_FIELD("timing", flash.set_level, "Flash set level", "...to this", I16, 0, 0x7FFF),
			FLARE_FIELD("timing", tint.from, "Red from", "The target turns red from this tick...", I16, 0, 200),
			FLARE_FIELD("timing", tint.ticks, "Red ticks", "...over this many ticks...", I16, 0, 200),
			FLARE_FIELD("timing", tint.shift, "Red shift", "...its weight (tick - from) << this (4096 = red)", I16, 0, 12),
			FLARE_FIELD("timing", tint.back_from, "Back from", "It turns back from this tick...", I16, 0, 200),
			FLARE_FIELD("timing", tint.back_ticks, "Back ticks", "...over this many ticks...", I16, 0, 200),
			FLARE_FIELD("timing", tint.back_shift, "Back shift", "...its weight (tick - from) << this", I16, 0, 12),
			FLARE_FIELD("burst", burst.depth.base, "Base", "", I16, -0x4000, 0x4000),
			FLARE_FIELD("burst", burst.depth.shift, "Shift", "", I16, 0, 15),
			FLARE_FIELD("burst", burst.scale.base, "Base", "", I16, -0x4000, 0x4000),
			FLARE_FIELD("burst", burst.scale.shift, "Shift", "", I16, 0, 15),
			FLARE_FIELD("silhouette", silhouette.from, "From tick", "The target is redrawn rippled from this tick...", I16, 0, 200),
			FLARE_FIELD("silhouette", silhouette.ticks, "Ticks", "...for this many ticks (the ripple grows and shrinks back)", I16, 2, 200),
			FLARE_FIELD("sparks", sparks.last_tick, "Last tick", "Sparks are spawned at target ticks 0 .. this...", I16, -1, 200),
			FLARE_FIELD("sparks", sparks.count, "Count", "...this many a tick", I16, 0, 64),
			FLARE_FIELD("sparks", sparks.window, "Collapse window", "Each collapses at a random tick in a window this long", I16, 0, 200),
			FLARE_FIELD("sparks", sparks.speed, "Turn", "Its direction turns this much a tick", I16, 0, 0x1000),
			FLARE_FIELD("sparks", sparks.sprite, "Sprite size", "Its sprite's size (4096 = its own)", I16, 0, 0x4000),
			FLARE_FIELD("sparks", sparks.burst, "Burst tick", "It bursts out from this tick of its own...", I16, 0, 200),
			FLARE_FIELD("sparks", sparks.fade, "Fade tick", "...and fades out from this one", I16, 0, 200),
		};
#undef FLARE_FIELD
	}

	void register_mag124_flare()
	{
		fxp::register_params({ 124, "Flare", &flare124::g_params, &flare124::DEFAULTS, sizeof(flare124::FlareParams),
		                       flare124::PARTS, (int)(sizeof(flare124::PARTS) / sizeof(flare124::PARTS[0])),
		                       flare124::FIELDS, (int)(sizeof(flare124::FIELDS) / sizeof(flare124::FIELDS[0])),
		                       flare124::GROUPS, (int)(sizeof(flare124::GROUPS) / sizeof(flare124::GROUPS[0])) });
		register_port(flare124::ORIG_RootTask, (void *)flare124::RootTask, "F124 RootTask", 124);
		register_port(flare124::ORIG_TargetTask, (void *)flare124::TargetTask, "F124 TargetTask", 124);
		register_port(flare124::ORIG_ParticleTask, (void *)flare124::ParticleTask, "F124 ParticleTask", 124);
		// 30 fps layer: see mag124_flare_held.inc
		FX_HELD(register_mag124_held();)
	}
	FX_LAB_MODULE(register_mag124_flare);
}

#ifdef FF8_FX_HELD
#include "mag124_flare_held.inc"
#endif
