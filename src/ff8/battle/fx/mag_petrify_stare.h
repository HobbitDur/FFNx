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

// The Petrify Stare module (effects 120, the Glacial Eye's, and 127, the Cockatrice's: one source
// built twice), written once from the parts library (fx_layouts.h). A `Layout` says where one copy
// is (its original functions, its exe data, its memory) and which rotation builder its callback's
// record flag selects (0x400: 0x701430; 0x40000: 0x7015B0).
//
//   RootTask - alternates the packet arena (magic buffer + arena_a / arena_b); clears the screen
//     flash level; on counter 1 of an `every`-tick cycle sets up the target pool (first time: ONE
//     node) and starts the target task of the next action on its first target (while one runs,
//     later actions fail to spawn, and the root ends once the queue is empty: only the first
//     action plays); runs the target queue; sets the screen flash (level 0 every tick); ends when
//     the queue is empty.
//   TargetTask - the "stare" layout at the CASTER's effect point 0xF0 (followed while the counter is
//     below 10, then left), moved 1/16 of the unit circle ahead of the caster's facing, its depth
//     from the caster's size; the engine's screen fade at 4; from 14 (for 100 ticks) the "stone"
//     layout at the target's default effect position (taken at 14), its depth from the target's
//     size; both through the anchor callback (AnchorPrimCallback: the record flag 0x200 and the
//     copy's rotation flag); the sound at 0 at the caster's default position; the result at 42;
//     ends at 49.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// The setup (it keeps the caster's entity) and the file loader are not ported (they run from the exe).
// Parameters (PetrifyStareParams, fx_params.h): one struct, parts and field table, each module with
// its own live values; defaults = the game's.
// The True30FPS branch's per-module 30 fps memos (mag120_petrify_stare_held.inc...) have to follow
// this template when that branch takes this one.

#pragma once

#include <string>

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace petrifystare
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	struct Layout
	{
		// its original functions (the ports replace them)
		uint32_t root, target;
		// its data: the stare and the stone layouts (and their decoded sizes), the sound block
		uint32_t stare, stare_bytes, stone, stone_bytes, sound;
		// its memory: the flash level, the caster's entity, the magic buffer base, the cast context,
		// the packet cursor, the target queue
		uint32_t flash_level, caster, tex_base, ctx, packet_cursor, target_queue;
		// magic buffer offsets: the packet arenas, the blended vertex frames; the target node's size
		uint32_t arena_a, arena_b, morph, node_bytes;
		// its build: the record flags the callback tests (0x200 and its rotation flag)
		uint32_t honoured;
	};

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct PetrifyStareParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the caster's default position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this
			int16_t fade_tick;      // the engine's screen fade (0x5712C0) starts...
			struct { int16_t a, b, c, d; } fade;   // ...with these four values
		} timing;
		struct Stare
		{
			int16_t bone;           // the caster's effect point the stare is at...
			int16_t follow;         // ...while the target counter is below this (then it stays)
			int16_t ahead;          // moved ahead of the caster's facing by the unit circle >> this
			struct { int16_t base, shift; } depth;  // depth = (base - the caster's size word +0x26) >> shift
		} stare;
		struct Stone
		{
			int16_t from, ticks;    // drawn over from .. from + ticks - 1, at the target's default effect position taken at `from`
			struct { int16_t base, shift; } depth;  // depth = (base - the target's size word +0x26) >> shift
		} stone;
	};
	enum PartId { PART_STARE = 1, PART_STONE = 2 };

	namespace table
	{
		using fxp::Field;
#define STARE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(PetrifyStareParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts the target task of the first action (its pool holds one); its clock: the sound, the screen fade, the result, the end (draws nothing)" },
			{ PART_STARE, "stare", "Stare", "The layout at the caster's effect point, ahead of its facing" },
			{ PART_STONE, "stone", "Stone", "The layout at the target, from a later tick" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing.fade", "Fade", "The engine's screen fade's four values" },
			{ "stare.depth", "Depth", "Its drawing depth: (Base - the caster's size word) >> Shift" },
			{ "stone.depth", "Depth", "Its drawing depth: (Base - the target's size word) >> Shift" },
		};
		static const Field FIELDS[] = {
			STARE_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			STARE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			STARE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the caster's default position when the target tick == this", I16, 0, 200),
			STARE_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			STARE_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			STARE_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this", I16, 0, 200),
			STARE_FIELD("timing", timing.fade_tick, "Fade tick", "The engine's screen fade starts when the target tick == this", I16, -1, 200),
			STARE_FIELD("timing", timing.fade.a, "A", "", I16, -0x8000, 0x7FFF),
			STARE_FIELD("timing", timing.fade.b, "B", "", I16, -0x8000, 0x7FFF),
			STARE_FIELD("timing", timing.fade.c, "C", "", I16, -0x8000, 0x7FFF),
			STARE_FIELD("timing", timing.fade.d, "D", "", I16, -0x8000, 0x7FFF),
			STARE_FIELD("stare", stare.bone, "Bone", "The caster's effect point the stare is at (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			STARE_FIELD("stare", stare.follow, "Follow until", "The stare follows that point while the target tick is below this, then stays", I16, 0, 200),
			STARE_FIELD("stare", stare.ahead, "Ahead", "Moved ahead of the caster's facing by the unit circle (4096) >> this", I16, 0, 15),
			STARE_FIELD("stare", stare.depth.base, "Base", "", I16, -0x4000, 0x4000),
			STARE_FIELD("stare", stare.depth.shift, "Shift", "", I16, 0, 15),
			STARE_FIELD("stone", stone.from, "From tick", "The stone is drawn from this target tick, at the target's default effect position taken then...", I16, 0, 200),
			STARE_FIELD("stone", stone.ticks, "Ticks", "...for this many ticks", I16, 0, 200),
			STARE_FIELD("stone", stone.depth.base, "Base", "", I16, -0x4000, 0x4000),
			STARE_FIELD("stone", stone.depth.shift, "Shift", "", I16, 0, 15),
		};
#undef STARE_FIELD
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
	struct TargetNode // pool of 1 node: the header, then the stare and the stone layouts
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		int16_t anchor[4]; // +0x10 the caster's effect point (x, y, z; followed until `follow`)
		int16_t stone[4];  // +0x18 the target's default effect position (taken at the stone's first tick)
		uint8_t *entity;   // +0x20 target entity
		uint8_t stare[1];  // +0x24 prim-model layout (Layout::stare_bytes), the stone one after it
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && offsetof(TargetNode, stare) == 0x24, "Petrify Stare nodes");

#ifdef FF8_FX_HELD
	// the 30 fps layer of a copy of the module (FX_HELD lines below): a copy with a held part specializes
	// this (mag120_petrify_stare_held.h, mag127_petrify_stare_held.h); any other copy notes nothing
	template<const Layout &L>
	struct Held
	{
		static void note_root() {}
		static void note_play(prim::Layout *, const AnchorArg *) {}
	};
#endif

	template<const Layout &L>
	struct PetrifyStare
	{
		// the game's values (with this copy's own sound) and the live ones the code reads
		inline static const PetrifyStareParams GAME = { { 10, L.sound, 0, 0x100, 0x2A, 0x31, 4, { 0, 1, 0, 0x80 } },
		                                                { 0xF0, 10, 4, { -0x800, 5 } }, { 0xE, 0x64, { -0x800, 5 } } };
		inline static PetrifyStareParams live = GAME;
		static const PetrifyStareParams &P() { return live; }

		static uint32_t &FlashLevel() { return var<uint32_t>(L.flash_level); }
		static uint8_t *&Caster() { return var<uint8_t *>(L.caster); }
		static CastContext *&Ctx() { return var<CastContext *>(L.ctx); }
		static TaskQueue *QTargets() { return (TaskQueue *)L.target_queue; }
		static uint32_t &PacketCursor() { return var<uint32_t>(L.packet_cursor); }
		static uint32_t &TexBase() { return var<uint32_t>(L.tex_base); }

		static uint32_t __cdecl TargetTask(TaskNode *n)
		{
			TargetNode *t = (TargetNode *)n;
			const PetrifyStareParams &pp = P();
			uint8_t *stone = t->stare + L.stare_bytes;
			if (t->counter < pp.stare.follow) GetEffectSpawnPosition(Caster(), pp.stare.bone, 0, t->anchor);
			AnchorArg arg;
			AheadOfFacing(Caster(), t->anchor, pp.stare.ahead, arg.anchor);
			// never written by the original (stack word), only loaded into the GTE V0 pad
			arg.anchor[3] = 0;
			arg.depth = DepthFromSize(Caster(), pp.stare.depth.base, pp.stare.depth.shift);
			arg.morph = TexBase() + L.morph;
			arg.honoured = L.honoured;
			arg.cursor = &PacketCursor();
			FX_HELD(Held<L>::note_play((prim::Layout *)t->stare, &arg);)
			FX_PART(PART_STARE);
			PlayAtAnchor(t->stare, &arg);
			FX_PART(0);
			if (t->counter == pp.timing.fade_tick) ScreenFadeTask(pp.timing.fade.a, pp.timing.fade.b, pp.timing.fade.c, pp.timing.fade.d);
			if ((uint32_t)((int32_t)t->counter - pp.stone.from) < (uint32_t)pp.stone.ticks)
			{
				if (t->counter == pp.stone.from) GetDefaultEffectPosition(t->entity, t->stone);
				memcpy(arg.anchor, t->stone, 8);
				arg.depth = DepthFromSize(t->entity, pp.stone.depth.base, pp.stone.depth.shift);
				FX_HELD(Held<L>::note_play((prim::Layout *)stone, &arg);)
				FX_PART(PART_STONE);
				PlayAtAnchor(stone, &arg);
				FX_PART(0);
			}
			if (t->counter == pp.timing.sound_tick)
			{
				int16_t pos[4];
				GetDefaultEffectPosition(Caster(), pos);
				BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, L.sound), pp.timing.sound_volume, pos);
			}
			if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
			if (t->counter >= pp.timing.end_tick) return TASK_END;
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
			FlashLevel() = 0;
			if (r->counter == 1)
			{
				if (!r->started)
				{
					r->started = 1;
					InitTaskQueuePool(QTargets(), (void *)TexBase(), L.node_bytes, 1);
				}
				if (r->action <= Ctx()->actions[0].last_action)
				{
					uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), L.target);
					if (t)
					{
						Memset32(&t->counter, 0, (L.node_bytes - 0xC) / 4);
						t->action = (int16_t)(uint16_t)r->action;
						t->entity = entity;
						DecodeModelPrimLayout(L.stare, t->stare, L.stare_bytes);
						DecodeModelPrimLayout(L.stone, t->stare + L.stare_bytes, L.stone_bytes);
						r->action++;
					}
				}
			}
			const int a = r->started ? ExecuteTaskQueue(QTargets()) : (int)n;
			SetScreenFlash(FlashLevel(), 0);
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
			fxp::register_params({ effect_id, name, &live, &GAME, sizeof(PetrifyStareParams),
			                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
			                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
			                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
			register_port(L.root, (void *)RootTask, names[0].c_str(), effect_id);
			register_port(L.target, (void *)TargetTask, names[1].c_str(), effect_id);
		}
	};
}
}
