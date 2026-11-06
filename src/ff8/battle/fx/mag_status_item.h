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

// The status-cure item module (effects 292 Soft, 293 Eye Drops, 294 Antidote, 295 Echo Screen,
// 296 Holy Water: one source built five times with different options), written once from the
// parts library (fx_layouts.h). A `Layout` says where one copy is (its original functions, its exe
// data, its memory) and which options it was built with (the record flags its callback tests, its
// anchor, the sound's place, a camera matrix of its own).
//
//   RootTask - alternates the packet arena (magic buffer + arena_a / arena_b by tick parity); on
//     counter 1 sets up the play pool (1 node at the start of the magic buffer) and starts the play
//     task with the anchor of target 0 of action 0, taken once (AnchorPoint: its default effect
//     position, that point between two shares of its height, or a bone's effect point); then, every
//     tick, its camera matrix (Echo Screen: 0x67AAE0 of the camera), the play queue and the sound on
//     its counter (after the play; Holy Water: before); ends when the play queue is empty.
//   PlayTask - draws THEN advances the layout (at node +0x14) at the anchor (AnchorPrimCallback,
//     the morph buffer right after the play node); when the layout ends: the action result of
//     target 0 of action 0 (ApplyActionResultToTarget: the status cure) and the end.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// The setup and the file loader are not ported (they run from the exe).
// Parameters (StatusItemParams, fx_params.h): one struct, parts and field table, each module with
// its own live values; defaults = the game's.
// The True30FPS branch's per-module 30 fps memos (mag292_soft_held.inc...) have to follow this
// template when that branch takes this one.

#pragma once

#include <string>

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace statusitem
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	struct Layout
	{
		// its original functions (the ports replace them)
		uint32_t root, play;
		// its data: the prim-model layout (and its decoded size), the sound block
		uint32_t model, layout_bytes, sound;
		// its memory: the cast context, the play queue, the packet cursor, the magic buffer base
		uint32_t ctx, play_queue, packet_cursor, tex_base;
		// the packet arenas (magic buffer offsets)
		uint32_t arena_a, arena_b;
		// its build: the record flags its callback tests (layouts::ANCHOR_*)
		uint32_t honoured;
		// its anchor (layouts::AnchorKind, the bone, the two height shares: the game's parameters)
		int16_t anchor, bone, height_a_times, height_a_share, height_b_times, height_b_share;
		// the sound played before the play queue (Holy Water), not after
		bool sound_first;
		// its camera matrix (Echo Screen's: 0x67AAE0 of the camera every tick; 0: none)
		uint32_t camera_matrix;
	};

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct StatusItemParams
	{
		struct Timing
		{
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick;     // the sound plays (root counter, after the play)...
			int16_t sound_pan;      // ...panned so (BdPlaySE's 3rd argument: 0x80 the middle)
		} timing;
		struct Play
		{
			int16_t depth;          // the prim header's depth
			int16_t anchor;         // where on the target (layouts::AnchorKind)
			int16_t bone;           // ANCHOR_AT_BONE: its effect point
			struct Share { int16_t times, share; };
			Share height_a, height_b; // ANCHOR_AT_HEIGHT: y = the target's +0x24 + (h x a + h x b) / 2
		} layout;
	};
	enum PartId { PART_LAYOUT = 1 };

	namespace table
	{
		using fxp::Field;
#define STATUS_ITEM_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(StatusItemParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root task's clock: when the sound plays (draws nothing)" },
			{ PART_LAYOUT, "layout", "Layout", "The layout played on the target; target 0 takes the result when it ends" },
		};
		static const fxp::Group GROUPS[] = {
			{ "layout.height_a", "Anchor height A", "Anchor 1: y = the target's +0x24 word + (its height x A + its height x B) / 2, each Times / Share (y goes down)" },
			{ "layout.height_b", "Anchor height B", "" },
		};
		static const Field FIELDS[] = {
			STATUS_ITEM_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			STATUS_ITEM_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays when the root tick == this (the layout starts at root tick 1)", I16, 0, 200),
			STATUS_ITEM_FIELD("timing", timing.sound_pan, "Sound pan", "Where it plays between the speakers: 128 the middle (BdPlaySE's pan)", I16, 0, 255),
			STATUS_ITEM_FIELD("layout", layout.depth, "Depth", "Added to its drawing depth (sort key)", I16, -0x1000, 0x1000),
			STATUS_ITEM_FIELD("layout", layout.anchor, "Anchor", "Where on target 0: 0 its effect position, 1 that point between two shares of its height (A, B), 2 the effect point of Bone", I16, 0, 2),
			STATUS_ITEM_FIELD("layout", layout.bone, "Bone", "Anchor 2: the target's effect point (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			STATUS_ITEM_FIELD("layout", layout.height_a.times, "Times", "", I16, -16, 16),
			STATUS_ITEM_FIELD("layout", layout.height_a.share, "Share", "", I16, 1, 16),
			STATUS_ITEM_FIELD("layout", layout.height_b.times, "Times", "", I16, -16, 16),
			STATUS_ITEM_FIELD("layout", layout.height_b.share, "Share", "", I16, 1, 16),
		};
#undef STATUS_ITEM_FIELD
	}

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F play pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct PlayNode // pool of 1 node (magic buffer): the header, the anchor, then the layout
	{
		TaskNode hdr;
		int16_t anchor[4]; // +0x0C anchor (counter 1; the 4th word is the pool's bytes)
		uint8_t layout[1]; // +0x14 prim-model layout (Layout::layout_bytes)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && offsetof(PlayNode, layout) == 0x14, "status item nodes");

#ifdef FF8_FX_HELD
	// the 30 fps layer of a copy of the module (FX_HELD lines below): a copy with a held part specializes
	// this (mag292_soft_held.h...); any other copy notes nothing
	template<const Layout &L>
	struct Held
	{
		static void note_root() {}
		static void note_play(prim::Layout *, const int16_t *) {}
	};
#endif

	template<const Layout &L>
	struct StatusItem
	{
		// the game's values (with this copy's own sound) and the live ones the code reads
		inline static const StatusItemParams GAME = { { L.sound, 1, 0x80 },
			{ -0x80, L.anchor, L.bone, { L.height_a_times, L.height_a_share }, { L.height_b_times, L.height_b_share } } };
		inline static StatusItemParams live = GAME;
		static const StatusItemParams &P() { return live; }

		static CastContext *&Ctx() { return var<CastContext *>(L.ctx); }
		static TaskQueue *QPlay() { return (TaskQueue *)L.play_queue; }
		static uint32_t &PacketCursor() { return var<uint32_t>(L.packet_cursor); }
		static uint32_t &TexBase() { return var<uint32_t>(L.tex_base); }
		static uint32_t NodeBytes() { return 0x14 + L.layout_bytes; }   // the morph buffer follows the play node

		// ------------------------------------------------------------------
		// Play
		// ------------------------------------------------------------------
		static uint32_t __cdecl PlayTask(TaskNode *n)
		{
			PlayNode *p = (PlayNode *)n;
			FX_PART(PART_LAYOUT);
			AnchorArg arg;
			memcpy(arg.anchor, p->anchor, sizeof(arg.anchor));
			arg.depth = P().layout.depth;
			arg.morph = TexBase() + NodeBytes();
			arg.honoured = L.honoured;
			arg.cursor = &PacketCursor();
			arg.matrix = L.camera_matrix ? (const Mat4x3 *)L.camera_matrix : &Camera();
			FX_HELD(Held<L>::note_play((prim::Layout *)p->layout, p->anchor);)
			if (PlayAtAnchor(p->layout, &arg) == 0)
			{
				ApplyActionResultToTarget(Ctx()->actions[0].targets);
				return TASK_END;
			}
			return 0;
		}

		static void Sound(int16_t counter)
		{
			const StatusItemParams &pp = P();
			if (counter == pp.timing.sound_tick) BdPlaySE((const void *)fxp::sound(pp.timing.sound, L.sound), 0, pp.timing.sound_pan);
		}

		// ------------------------------------------------------------------
		// Root
		// ------------------------------------------------------------------
		static uint32_t __cdecl RootTask(TaskNode *n)
		{
			RootNode *r = (RootNode *)n;
			const StatusItemParams &pp = P();
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
			if (r->counter == 1 && !r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QPlay(), (void *)TexBase(), NodeBytes(), 1);
				PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), L.play);
				// the anchor of target 0 of action 0
				AnchorPoint(Entity(Ctx()->actions[0].targets[0]), pp.layout.anchor, pp.layout.bone, pp.layout.height_a.times,
				            pp.layout.height_a.share, pp.layout.height_b.times, pp.layout.height_b.share, p->anchor);
				DecodeModelPrimLayout(L.model, p->layout, L.layout_bytes);
			}
			int a;
			if (r->started)
			{
				if (L.camera_matrix) EffectCameraMatrix(&Camera(), (Mat4x3 *)L.camera_matrix);
				if (L.sound_first) Sound(r->counter);
				a = ExecuteTaskQueue(QPlay());
				FX_PART(0);
				if (!L.sound_first) Sound(r->counter);
			}
			else a = (int)n;
			if (r->started && a == 0) return TASK_END;
			r->counter++;
			return 0;
		}

		// its parameters and its ports under `effect_id`, the ports named "<tag> RootTask"...
		static void register_module(int effect_id, const char *name, const char *tag)
		{
			static std::string names[2];
			names[0] = std::string(tag) + " RootTask";
			names[1] = std::string(tag) + " PlayTask";
			fxp::register_params({ effect_id, name, &live, &GAME, sizeof(StatusItemParams),
			                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
			                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
			                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
			register_port(L.root, (void *)RootTask, names[0].c_str(), effect_id);
			register_port(L.play, (void *)PlayTask, names[1].c_str(), effect_id);
		}
	};
}
}
