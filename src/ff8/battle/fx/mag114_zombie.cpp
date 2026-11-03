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

// Effect 114: Zombie (spell, MAG_114_*).
//
// Structure (setup MAG_114_ZOMBIE 0x6C99C0 -> _Init 0x6C99F0, file loader 0x6C99D0 = the texture
// file named at 0x12C0238; the setup starts camera animation 0x12BD8D8 and queues the TIM):
//   RootTask (0x6C9A60) - alternates the packet arena (magic buffer + 0x1600 / + 0x19600); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target, unless any target task of 40 ticks or less still
//     runs (the cycle then restarts); runs the target queue, ends when it is empty.
//   Target (0x6C9BC0) - node 0x710, one per action: follows the target's effect anchor; plays the
//     prim-model layout 0x12BA30C with the callback MistPart (0x6C9D60) every tick, scaled by the
//     target's size (sound at 0); from 30 marks the target's entities (flags bit 3 on its +0x8C
//     ring); over 30..59 (unless the cast context has flag 0x200) redraws the target model as a
//     glowing overlay (0x6CA560): lifted by sin(t) * size, its vertices swayed sideways by a
//     wave table, additive flat polygons in a colour going from black to the target's tint;
//     damage at 54; ends at 60 (marks cleared).
//   MistPart (0x6C9D60) - one object of the layout: record flag 0x20000 selects the rotation order,
//     the block scale (always on here) scales the offsets and the matrix; flag 0x200 = the offset
//     stays in screen axes, else anchor + offset through the camera; flag 0x100 = scale by a
//     diagonal matrix product, else scale3DMatrix. Objects 2 and 3 are drawn by QuadsAdditive
//     (0x6CA0E0: only the model's gouraud textured quads, 0x6CA1B0, each after an additive
//     draw-mode primitive, depth offset clamped at 0), the others by MAG_070_sub_7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521818..0x252186C (texture file, magic buffer base, context, root pool,
// packet cursor 0x252183C, target / root queues, the zero vector 0x2521860 and the black colour
// 0x2521868 the overlay starts from: never written). The target pool (3 x 0x710), the morph
// vertex buffer (+0x1530) and the packet arenas are in the magic buffer. The overlay uses the
// battle's bone workspace (0x1D98B3C) as its vertex buffer and rebuilds the target's bone
// matrices (camera space for the draw, then back from the pose).
// Built from the parts library (fx_layouts.h, fx_sway.h): the anchor callback (AnchorPrimCallback,
// the record flags 0x20000, 0x200 and 0x100 honoured, scaled by the target's size, objects 2 and 3
// as AdditiveQuads' gouraud-only variant), DepthFromSize, SizeScale, the sway overlay
// (DrawSwayOverlay, SwayOverlayParams), MarkRing. Parameters (ZombieParams, fx_params.h): the
// numbers above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include "fx_sway.h"

namespace ff8fx
{
namespace zombie114
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x252181C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521820); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252183C); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521840; }         // pool: magic buffer, 3 x 0x710

	static const uint32_t ORIG_RootTask = 0x6C9A60;
	static const uint32_t ORIG_TargetTask = 0x6C9BC0;
	static const uint32_t MODEL_Mist = 0x12BA30C;        // prim-model layout data (0x6F4-byte layout)
	static const int16_t *const WAVE_Table = (const int16_t *)0x12BE238;   // s16 [4096]
	static const void *const SOUND_Zombie = (const void *)0x12BE234;
	static const uint32_t HONOURED = ANCHOR_ROT_XZY | ANCHOR_OFFSET_AS_IS | ANCHOR_DIAGONAL_SCALE;   // the record flags MistPart tests

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct ZombieParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks...
			int16_t busy_tick;      // ...unless any target task is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this
			int16_t mark_from;      // the target's entities marked (flags bit 3, its +0x8C ring) from this tick to the end
		} timing;
		struct Mist
		{
			struct { int16_t base, shift; } depth;  // depth = (base - the target's size word +0x26) >> shift
			struct { int16_t base, shift; } scale;  // scaled by ((the target's size >> 1) + base) >> shift
			struct { int16_t a, b; } additive;      // these two objects drawn as their gouraud quads only, additive (-1: none)
		} mist;
		struct Overlay
		{
			int16_t from, ticks;    // the target redrawn swayed over from .. from + ticks - 1 (not when the cast's flags have 2)
			int16_t lift_shift;     // lifted by sin x the mist's size >> this
			struct { int16_t base, step; } amp;   // the sway's amplitude: base + step x the step's 4096ths
			int16_t phase_step;     // the wave's phase a step
		} overlay;
	};
	static const ZombieParams DEFAULTS = {
		{ 10, 0x28, (uint32_t)SOUND_Zombie, 0, 0x101, 0x36, 0x3C, 0x1E },
		{ { -0x800, 3 }, { 0x800, 0 }, { 2, 3 } },
		{ 0x1E, 0x1E, 13, { 0x200, 2 }, 8 },
	};
	static ZombieParams g_params = DEFAULTS;
	static inline const ZombieParams &P() { return g_params; }
	enum PartId { PART_MIST = 1, PART_OVERLAY = 2 };

	// an object (-1: none) as an additive_objects bit
	static inline uint32_t ObjectBit(int16_t object) { return (uint16_t)object < 32 ? 1u << object : 0; }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x710 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		uint8_t mist[0x6F4]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x710, "Zombie nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag114_zombie_held.h"
#endif

namespace ff8fx
{
namespace zombie114
{
	// ------------------------------------------------------------------
	// Target (0x6C9BC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const ZombieParams &pp = P();
		if (t->counter >= pp.timing.mark_from) MarkRing(t->entity, 8, true);
		GetDefaultEffectPosition(t->entity, t->pos);
		AnchorArg arg;
		memcpy(arg.anchor, t->pos, 8);
		arg.scaled = 1;
		const int32_t reach = SizeScale(t->entity, pp.mist.scale.base, pp.mist.scale.shift);
		arg.scale[0] = reach;
		arg.scale[2] = reach;
		arg.scale[1] = reach;
		arg.depth = DepthFromSize(t->entity, pp.mist.depth.base, pp.mist.depth.shift);
		arg.morph = TexBase() + 0x1530;
		arg.honoured = HONOURED;
		arg.cursor = &PacketCursor();
		arg.additive_objects = ObjectBit(pp.mist.additive.a) | ObjectBit(pp.mist.additive.b);
		arg.quads_gouraud_only = true;
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(held_note_play(t, &arg);)
		FX_PART(PART_MIST);
		PlayAtAnchor(t->mist, &arg);
		FX_PART(0);
		const int32_t e = t->counter - pp.overlay.from;
		if ((uint32_t)e < (uint32_t)pp.overlay.ticks && !(Ctx()->flags & 2))
		{
			SwayArg o;
			SwayOverlayParams(e, pp.overlay.ticks, arg.scale[0], pp.overlay.lift_shift, pp.overlay.amp.base, pp.overlay.amp.step,
			                  pp.overlay.phase_step, &o);
			// 30 fps layer: see mag114_zombie_held.inc
			FX_HELD(held_note_overlay(t, &o, e, arg.scale[0]);)
			FX_PART(PART_OVERLAY);
			DrawSwayOverlay(t->entity, &o, WAVE_Table, PacketCursor());
			FX_PART(0);
		}
		if (t->counter == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Zombie), pp.timing.sound_volume, pos);
		}
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= pp.timing.end_tick)
		{
			MarkRing(t->entity, 8, false);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6C9A60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1600;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19600;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x710, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
				// wait while any target task at busy_tick or before runs (on any target)
				TargetNode *pool = (TargetNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].counter <= P().timing.busy_tick)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					if (t)
					{
						Memset32(&t->counter, 0, 0x1C1);
						t->action = (int16_t)(uint16_t)r->action;
						t->entity = entity;
						DecodeModelPrimLayout(MODEL_Mist, t->mist, 0x6F4);
						r->action++;
					}
				}
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of ZombieParams
	// ------------------------------------------------------------------
	namespace zombie114
	{
		using fxp::Field;
#define ZOMBIE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(ZombieParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn (not while any young one runs); each target task's clock: its sound, the target's mark, its result, its end (draws nothing)" },
			{ PART_MIST, "mist", "Mist", "The layout at the target's effect position, scaled by its size, two objects as additive quads" },
			{ PART_OVERLAY, "overlay", "Overlay", "The target redrawn lifted and swayed, flat additive in a colour going from black to its tint" },
		};
		static const fxp::Group GROUPS[] = {
			{ "mist.depth", "Depth", "Its drawing depth: (Base - the target's size word) >> Shift" },
			{ "mist.scale", "Size", "Scaled by ((the target's size >> 1) + Base) >> Shift (4096 = its own size); the overlay's lift too" },
			{ "mist.additive", "Additive objects", "These two objects of the layout are drawn as their gouraud quads only, additive (-1: none)" },
			{ "overlay.amp", "Sway", "Amplitude Base + Step x the step's 4096ths" },
		};
		static const Field FIELDS[] = {
			ZOMBIE_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			ZOMBIE_FIELD("timing", timing.busy_tick, "Busy tick", "Not while any target task is at this tick or before (the cycle restarts; -1: never)", I16, -1, 200),
			ZOMBIE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			ZOMBIE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			ZOMBIE_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			ZOMBIE_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			ZOMBIE_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this", I16, 0, 200),
			ZOMBIE_FIELD("timing", timing.mark_from, "Mark from", "The target's entities are marked (flags bit 3) from this tick to the end", I16, 0, 200),
			ZOMBIE_FIELD("mist", mist.depth.base, "Base", "", I16, -0x4000, 0x4000),
			ZOMBIE_FIELD("mist", mist.depth.shift, "Shift", "", I16, 0, 15),
			ZOMBIE_FIELD("mist", mist.scale.base, "Base", "", I16, -0x4000, 0x4000),
			ZOMBIE_FIELD("mist", mist.scale.shift, "Shift", "", I16, 0, 15),
			ZOMBIE_FIELD("mist", mist.additive.a, "Object", "", I16, -1, 31),
			ZOMBIE_FIELD("mist", mist.additive.b, "Object", "", I16, -1, 31),
			ZOMBIE_FIELD("overlay", overlay.from, "From tick", "The target is redrawn swayed from this tick...", I16, 0, 200),
			ZOMBIE_FIELD("overlay", overlay.ticks, "Ticks", "...for this many ticks (its lift and tint: a quarter sine over them)", I16, 1, 200),
			ZOMBIE_FIELD("overlay", overlay.lift_shift, "Lift shift", "Lifted by sin x the mist's size >> this", I16, 0, 31),
			ZOMBIE_FIELD("overlay", overlay.amp.base, "Base", "", I16, -0x4000, 0x4000),
			ZOMBIE_FIELD("overlay", overlay.amp.step, "Step", "", I16, -16, 16),
			ZOMBIE_FIELD("overlay", overlay.phase_step, "Phase step", "The sway's wave moves this much a tick", I16, -0x800, 0x800),
		};
#undef ZOMBIE_FIELD
	}

	void register_mag114_zombie()
	{
		fxp::register_params({ 114, "Zombie", &zombie114::g_params, &zombie114::DEFAULTS, sizeof(zombie114::ZombieParams),
		                       zombie114::PARTS, (int)(sizeof(zombie114::PARTS) / sizeof(zombie114::PARTS[0])),
		                       zombie114::FIELDS, (int)(sizeof(zombie114::FIELDS) / sizeof(zombie114::FIELDS[0])),
		                       zombie114::GROUPS, (int)(sizeof(zombie114::GROUPS) / sizeof(zombie114::GROUPS[0])) });
		register_port(zombie114::ORIG_RootTask, (void *)zombie114::RootTask, "Z114 RootTask", 114);
		register_port(zombie114::ORIG_TargetTask, (void *)zombie114::TargetTask, "Z114 TargetTask", 114);
		// 30 fps layer: see mag114_zombie_held.inc
		FX_HELD(register_mag114_held();)
	}
	FX_LAB_MODULE(register_mag114_zombie);
}

#ifdef FF8_FX_HELD
#include "mag114_zombie_held.inc"
#endif
