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

// The clock spell module (effects 123 Slow and 125 Haste: one source built twice), written once
// from the parts library (fx_layouts.h). A `Layout` says where one copy is (its original functions,
// its exe data, its memory) and which options it was built with (its sound tick, the "scaled" word
// it reads, the target's blink).
//
//   RootTask - alternates the packet arena (magic buffer + arena_a / arena_b); on counter 1 of an
//     `every`-tick cycle sets up the target pool (first time) and starts the target task of the next
//     action on its first target (its default effect position and its effect point 0xF0, taken
//     once); every tick rebuilds the effect camera matrix (0x67AAE0 of the battle camera) and runs
//     the target queue; ends when it is empty.
//   TargetTask - two layouts in frames of the target, turned with it and scaled by its size, both
//     through the frame callback (FramePrimCallback, every record flag it knows, the engine's
//     renderer 0x572200): the clock at its effect position on its feet, the lower layout under its
//     effect point (lowered by its size: s / 16 + s / 4); the sound at its tick; Slow: the target
//     blinks (entity flag bit 3); a screen fade at its tick; when both layouts have finished: the
//     status (the action's result), end.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// The setup and the file loader are not ported (they run from the exe).
// Parameters (ClockSpellParams, fx_params.h): one struct, parts and field table, each module with
// its own live values; defaults = the game's.
// The True30FPS branch's per-module 30 fps memos (mag123_slow_held.inc...) have to follow this
// template when that branch takes this one.

#pragma once

#include <string>

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace clockspell
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	struct Layout
	{
		// its original functions (the ports replace them)
		uint32_t root, target;
		// its data: the clock and the lower layouts (and their decoded sizes), the sound block
		uint32_t clock, clock_bytes, lower, lower_bytes, sound;
		// its memory: the magic buffer base, the cast context, the packet cursor, the target queue,
		// the effect camera matrix
		uint32_t tex_base, ctx, packet_cursor, target_queue, camera_matrix;
		// magic buffer offsets: the packet arenas, the blended vertex frames; the target node's size
		uint32_t arena_a, arena_b, morph, node_bytes;
		// its build: the sound's tick; the "scaled" word the callback reads, never written by Slow
		// (VANILLA UNINITIALISED READ 0x6BD3E0..0x6BD4E9: the slot holds what 0x67AAE0 left there,
		// the effect camera matrix's m[1][0]: 0 unless the camera rolls); the blink's ticks (0: none)
		int16_t sound_tick;
		bool scaled_from_camera;
		int16_t blink_ticks;
	};

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct ClockSpellParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t fade_tick;      // the engine's screen fade (0x5712C0) starts
		} timing;
		struct Scale
		{
			int16_t base, shift;    // both layouts scaled by ((the target's size >> 1) + base) >> shift
		} scale;
		struct Blink
		{
			int16_t from;           // the target blinks (entity flag bit 3) from this tick...
			int16_t ticks;          // ...for this many ticks (0: never), on the ticks e % (ticks - e) == 0
		} blink;
	};
	enum PartId { PART_CLOCK = 1, PART_LOWER = 2 };

	namespace table
	{
		using fxp::Field;
#define CLOCK_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(ClockSpellParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn; each target task's clock: its sound, the screen fade, the target's blink; both layouts' size (draws nothing); it ends when both layouts have finished" },
			{ PART_CLOCK, "clock", "Clock", "The clock at the target's effect position, on its feet, turned with it" },
			{ PART_LOWER, "lower", "Lower layout", "The second layout under the target's effect point, turned with it" },
		};
		static const fxp::Group GROUPS[] = {
			{ "scale", "Size", "Both layouts scaled by ((the target's size >> 1) + Base) >> Shift (4096 = their own size)" },
			{ "blink", "Blink", "The target blinks over From .. From + Ticks - 1, on the ticks e % (Ticks - e) == 0 (e = tick - From; Ticks 0: never)" },
		};
		static const Field FIELDS[] = {
			CLOCK_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			CLOCK_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			CLOCK_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			CLOCK_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 255),
			CLOCK_FIELD("timing", timing.fade_tick, "Fade tick", "The engine's screen fade starts when the target tick == this", I16, -1, 200),
			CLOCK_FIELD("timing", scale.base, "Base", "", I16, -0x4000, 0x4000),
			CLOCK_FIELD("timing", scale.shift, "Shift", "", I16, 0, 15),
			CLOCK_FIELD("timing", blink.from, "From", "", I16, 0, 200),
			CLOCK_FIELD("timing", blink.ticks, "Ticks", "", I16, 0, 200),
		};
#undef CLOCK_FIELD
	}

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes: the header, then the clock and the lower layouts
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor (GetDefaultEffectPosition)
		int16_t spawn[4];  // +0x1C effect spawn position of bone 0xF0
		uint8_t clock[1];  // +0x24 prim-model layout (Layout::clock_bytes), the lower one after it
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && offsetof(TargetNode, clock) == 0x24, "clock spell nodes");

	static const uint32_t HONOURED = FRAME_DIAGONAL_SCALE | FRAME_OFFSET_AS_IS | FRAME_MATRIX | FRAME_SUBTRACT | FRAME_NO_LIGHT | FRAME_NOT_COMPOSED;

#ifdef FF8_FX_HELD
	// the 30 fps layer of a copy of the module (FX_HELD lines below): a copy with a held part specializes
	// this (mag123_slow_held.h, mag125_haste_held.h); any other copy notes nothing
	template<const Layout &L>
	struct Held
	{
		static void note_root() {}
		static void note_play(uint8_t *, const FrameArg *, const uint8_t *, const int32_t *) {}
	};
#endif

	template<const Layout &L>
	struct ClockSpell
	{
		// the game's values (with this copy's own sound, sound tick and blink) and the live ones the code reads
		inline static const ClockSpellParams GAME = { { 10, L.sound, L.sound_tick, 1, 0x30 }, { 0x800, 0 }, { 0x14, L.blink_ticks } };
		inline static ClockSpellParams live = GAME;
		static const ClockSpellParams &P() { return live; }

		static CastContext *&Ctx() { return var<CastContext *>(L.ctx); }
		static TaskQueue *QTargets() { return (TaskQueue *)L.target_queue; }
		static uint32_t &PacketCursor() { return var<uint32_t>(L.packet_cursor); }
		static uint32_t &TexBase() { return var<uint32_t>(L.tex_base); }
		static Mat4x3 *EffCam() { return (Mat4x3 *)L.camera_matrix; }

		static uint32_t __cdecl TargetTask(TaskNode *n)
		{
			TargetNode *t = (TargetNode *)n;
			const ClockSpellParams &pp = P();
			uint8_t *lower = t->clock + L.clock_bytes;
			if (t->counter == pp.timing.sound_tick) BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, L.sound), pp.timing.sound_volume, t->pos);
			FrameArg arg;
			arg.morph = TexBase() + L.morph;
			const int32_t s = SizeScale(t->entity, pp.scale.base, pp.scale.shift);
			arg.scale[2] = s;
			arg.scale[1] = s;
			arg.scale[0] = s;
			arg.scaled = L.scaled_from_camera ? EffCam()->m[1][0] : 1;
			arg.honoured = HONOURED;
			arg.cursor = &PacketCursor();
			arg.matrix = EffCam();
			arg.engine_draw = true;
			int32_t point[3] = { t->pos[0], *(const int16_t *)(t->entity + 0x24), t->pos[2] };
			FX_HELD(Held<L>::note_play(t->clock, &arg, t->entity, point);)
			TargetFrame(t->entity, point, &arg.m);
			FX_PART(PART_CLOCK);
			int32_t left = PlayInFrame(t->clock, &arg);
			point[1] = (int32_t)t->spawn[1] - (s >> 4) - (s >> 2);
			FX_HELD(Held<L>::note_play(lower, &arg, t->entity, point);)
			TargetFrame(t->entity, point, &arg.m);
			FX_PART(PART_LOWER);
			left |= PlayInFrame(lower, &arg);
			FX_PART(0);
			// the target blinks over from .. from + ticks - 1 (entity flag bit 3 on the ticks e % (ticks - e) == 0)
			const int32_t e = t->counter - pp.blink.from;
			if ((uint32_t)e < (uint32_t)pp.blink.ticks)
			{
				if (e != 0 && (uint32_t)e % (uint32_t)(pp.blink.ticks - e) == 0) *t->entity |= 8;
				else *(uint16_t *)t->entity &= 0xFFF7;
			}
			if (t->counter == pp.timing.fade_tick) ScreenFadeTask(0, 1, 1, 0x60);
			if (left == 0)
			{
				if (L.blink_ticks) *(uint16_t *)t->entity &= 0xFFF7;
				ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
				return TASK_END;
			}
			t->counter++;
			return 0;
		}

		static uint32_t __cdecl RootTask(TaskNode *n)
		{
			RootNode *r = (RootNode *)n;
			FX_HELD(Held<L>::note_root();)
			if (r->arena)
			{
				PacketCursor() = TexBase() + L.arena_a;
				r->arena = 0;
			}
			else
			{
				PacketCursor() = TexBase() + L.arena_b;
				r->arena = 1;
			}
			if (r->counter == 1)
			{
				if (!r->started)
				{
					r->started = 1;
					InitTaskQueuePool(QTargets(), (void *)TexBase(), L.node_bytes, 3);
				}
				if (r->action <= Ctx()->actions[0].last_action)
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), L.target);
					Memset32(&t->counter, 0, (L.node_bytes - 0xC) / 4);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = Entity(Ctx()->actions[t->action].targets[0]);
					GetDefaultEffectPosition(t->entity, t->pos);
					GetEffectSpawnPosition(t->entity, 0xF0, 0, t->spawn);
					DecodeModelPrimLayout(L.clock, t->clock, L.clock_bytes);
					DecodeModelPrimLayout(L.lower, t->clock + L.clock_bytes, L.lower_bytes);
					r->action++;
				}
			}
			EffectCameraMatrix(&Camera(), EffCam());
			const int a = r->started ? ExecuteTaskQueue(QTargets()) : (int)n;
			if (r->started && a == 0) return TASK_END;
			r->counter++;
			if (r->counter >= P().timing.every) r->counter = 0;
			return 0;
		}

		// its parameters and its ports under `effect_id`, the ports named "<tag> RootTask"...
		static void register_module(int effect_id, const char *name, const char *tag)
		{
			static std::string names[2];
			names[0] = std::string(tag) + " RootTask";
			names[1] = std::string(tag) + " TargetTask";
			fxp::register_params({ effect_id, name, &live, &GAME, sizeof(ClockSpellParams),
			                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
			                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
			                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
			register_port(L.root, (void *)RootTask, names[0].c_str(), effect_id);
			register_port(L.target, (void *)TargetTask, names[1].c_str(), effect_id);
		}
	};
}
}
