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

// Effect 294: Antidote (battle item Antidote: kernel.bin battle item +4 = 294; MAG_294_*). No other
// kernel.bin entry uses effect 294. One of the five adjacent status-cure item modules (292 Soft,
// 293 Eye Drops, 294 Antidote, 295 Echo Screen, 296 Holy Water: one source, built five times with
// different options), each a single prim-model layout played on the target.
//
// Structure (setup MAG_294 0x6743F0 -> 0x674420, file loader 0x674400 = the texture file named at 0x10637A0;
// the setup keeps the cast context (0x2507A4C) and the caster's entity (0x2507A50, never read again), queues
// the TIM and starts the root task in its 1-node pool 0x2507A78):
//   RootTask (0x674490) - alternates the packet arena (magic buffer + 0xA34 / + 0x10A34 by tick parity,
//     cursor 0x2507A8C); on counter 1 sets up the play pool (1 x 0x8B4 bytes at the start of the magic
//     buffer) and starts the play task with the anchor of target 0 of action 0, taken once:
//     its default effect position (0x571400) with y = half its height (+0x92) above its +0x24;
//     then the play queue (sound 0x106379C on counter 1, after it); ends when the play queue is empty.
//   PlayTask (0x6745C0) - draws THEN advances layout 0x1060C50 (0x8A0 bytes at node +0x14) at the anchor;
//     when the layout ends (the play of its last frame): the action result of target 0 of action 0
//     (ApplyActionResultToTarget: the status cure) and the end.
//   PrimCallback (0x674630) - one object of the layout: vertex frame (or blend into the morph buffer, magic
//     buffer + 0x8B4, right after the play node), rotation from the record's angles (flag 0x40000: 0x7015B0, else 0x701310);
//     the offset + the anchor through the camera, composed with it unless flag 0x8000;
//     + the camera translation; scale (scale3DMatrix); fade; 0x7043B0 at depth -0x80.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity or the camera words (only read: the target's position
// and height, the camera matrix 0x1D97778).
// Module globals: 0x2507A48..0x2507A94 (texture file 0x2507A48, context 0x2507A4C, caster entity 0x2507A50, play
// queue 0x2507A58, root queue 0x2507A68, root pool 0x2507A78 (1 x 0x14), packet cursor 0x2507A8C, magic buffer
// base 0x2507A90). The play pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS: the callback's matrix pad (+0x12: loaded into the GTE control register
// R33 word only) and its offset's 4th word (loaded into the GTE VZ0 word only); see the report of the
// port (they never reach a packet).
// Built from the parts library (fx_layouts.h): the anchor callback (AnchorPrimCallback, the record
// flags 0x40000 and 0x8000 honoured) and the anchor at a share of the target's height (HeightPoint).
// Parameters (AntidoteParams, fx_params.h): the numbers above, defaults = the game's; the layout is
// tagged (FX_PART) for the effect lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace antidote294
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507A4C); }
	inline TaskQueue *QPlay() { return (TaskQueue *)0x2507A58; }              // pool: magic buffer, 1 x 0x8B4
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507A8C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507A90); }          // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_RootTask = 0x674490;
	static const uint32_t ORIG_PlayTask = 0x6745C0;
	static const uint32_t MODEL = 0x1060C50;           // prim-model layout data
	static const int32_t LAYOUT_BYTES = 0x8A0;
	static const uint32_t NODE_BYTES = 0x8B4;       // play node; the morph buffer follows it
	static const void *const SOUND = (const void *)0x106379C;
	static const uint32_t ARENA_A = 0xA34;         // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10A34;
	static const uint32_t HONOURED = ANCHOR_ROT_YXZ | ANCHOR_NOT_COMPOSED;   // the record flags its callback tests

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct AntidoteParams
	{
		struct Timing
		{
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick;     // the sound plays (root counter, after the play)...
			int16_t sound_pan;      // ...panned so (BdPlaySE's 3rd argument: 0x80 the middle)
		} timing;
		struct Layout
		{
			int16_t depth;          // the prim header's depth
			int16_t height_times;   // the anchor's y: the target's +0x24 + its height x times / share
			int16_t height_share;
		} layout;
	};
	static const AntidoteParams DEFAULTS = {
		{ (uint32_t)SOUND, 1, 0x80 },
		{ -0x80, 1, 2 },
	};
	static AntidoteParams g_params = DEFAULTS;
	static inline const AntidoteParams &P() { return g_params; }
	enum PartId { PART_LAYOUT = 1 };

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507A78)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F play pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct PlayNode // pool of 1 node of 0x8B4 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t anchor[4]; // +0x0C anchor (counter 1; the 4th word is the pool's bytes)
		uint8_t layout[LAYOUT_BYTES]; // +0x14 prim-model layout
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == NODE_BYTES, "Antidote nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag294_antidote_held.h"
#endif

namespace ff8fx
{
namespace antidote294
{
	// ------------------------------------------------------------------
	// Play (0x6745C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		PlayNode *p = (PlayNode *)n;
		FX_PART(PART_LAYOUT);
		AnchorArg arg;
		memcpy(arg.anchor, p->anchor, sizeof(arg.anchor));
		arg.depth = P().layout.depth;
		arg.morph = TexBase() + NODE_BYTES;
		arg.honoured = HONOURED;
		arg.cursor = &PacketCursor();
		// 30 fps layer: see mag294_antidote_held.inc
		FX_HELD(held_note_play((prim::Layout *)p->layout, p->anchor);)
		if (PlayAtAnchor(p->layout, &arg) == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x674490)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		const AntidoteParams &pp = P();
		// 30 fps layer: see mag294_antidote_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QPlay(), (void *)TexBase(), NODE_BYTES, 1);
			PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), ORIG_PlayTask);
			// the anchor of target 0 of action 0: its effect position at half its height
			HeightPoint(Entity(Ctx()->actions[0].targets[0]), pp.layout.height_times, pp.layout.height_share, p->anchor);
			DecodeModelPrimLayout(MODEL, p->layout, LAYOUT_BYTES);
		}
		int a;
		if (r->started)
		{
			a = ExecuteTaskQueue(QPlay());
			FX_PART(0);
			if (r->counter == pp.timing.sound_tick) BdPlaySE((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND), 0, pp.timing.sound_pan);
		}
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of AntidoteParams
	// ------------------------------------------------------------------
	namespace antidote294
	{
		using fxp::Field;
#define ANTIDOTE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(AntidoteParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root task's clock: when the sound plays (draws nothing)" },
			{ PART_LAYOUT, "layout", "Layout", "The layout played on the target; target 0 takes the result when it ends" },
		};
		static const fxp::Group GROUPS[] = {
			{ "layout.height", "Anchor height", "The anchor: the target's effect position, y = its +0x24 word + its height x Times / Share (y goes down)" },
		};
		static const Field FIELDS[] = {
			ANTIDOTE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			ANTIDOTE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays when the root tick == this (the layout starts at root tick 1)", I16, 0, 200),
			ANTIDOTE_FIELD("timing", timing.sound_pan, "Sound pan", "Where it plays between the speakers: 128 the middle (BdPlaySE's pan)", I16, 0, 255),
			ANTIDOTE_FIELD("layout", layout.depth, "Depth", "Added to its drawing depth (sort key)", I16, -0x1000, 0x1000),
			ANTIDOTE_FIELD("layout", layout.height_times, "Times", "", I16, -16, 16),
			ANTIDOTE_FIELD("layout", layout.height_share, "Share", "", I16, 1, 16),
		};
#undef ANTIDOTE_FIELD
	}

	void register_mag294_antidote()
	{
		fxp::register_params({ 294, "Antidote", &antidote294::g_params, &antidote294::DEFAULTS, sizeof(antidote294::AntidoteParams),
		                       antidote294::PARTS, (int)(sizeof(antidote294::PARTS) / sizeof(antidote294::PARTS[0])),
		                       antidote294::FIELDS, (int)(sizeof(antidote294::FIELDS) / sizeof(antidote294::FIELDS[0])),
		                       antidote294::GROUPS, (int)(sizeof(antidote294::GROUPS) / sizeof(antidote294::GROUPS[0])) });
		register_port(antidote294::ORIG_RootTask, (void *)antidote294::RootTask, "294 RootTask", 294);
		register_port(antidote294::ORIG_PlayTask, (void *)antidote294::PlayTask, "294 PlayTask", 294);
		// 30 fps layer: see mag294_antidote_held.inc
		FX_HELD(register_mag294_held();)
	}
	FX_LAB_MODULE(register_mag294_antidote);
}

#ifdef FF8_FX_HELD
#include "mag294_antidote_held.inc"
#endif
