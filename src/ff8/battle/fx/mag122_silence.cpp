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

// Effect 122: Silence (spell, MAG_122_*).
//
// Structure (setup MAG_122_SILENCE 0x6BD8E0 -> _Init 0x6BD910, file loader 0x6BD8F0 = the texture
// file named at 0x1272694; the setup starts camera animation 0x1271C68 and queues the TIM):
//   RootTask (0x6BD980) - alternates the packet arena (magic buffer + 0x1124 / + 0x11124); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target (no wait); runs the target queue, ends when it is
//     empty.
//   Target (0x6BDAC0) - node 0x4AC, one per action: marks the target entity (flags word bit
//     0x2000) while it runs; plays the prim-model layout 0x126E4C0 with the callback MutePart
//     (0x6BDB90) at the target's effect anchor 0xF0 every tick (sound at 0), depth offset from the
//     target's size; damage at 50; ends at 60 (mark cleared).
//   MutePart (0x6BDB90) - one object of the layout: record flag 0x200 = the offset stays in
//     screen axes (the anchor through the camera, the offset added unrotated, the rotation not
//     composed with the camera), else anchor + offset through the camera; flag 0x100 = scale by a
//     diagonal matrix product, else scale3DMatrix; object 4 is drawn 0x20 nearer.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25214B0..0x25214F8 (texture file, magic buffer base, context, root pool,
// packet cursor 0x25214D4, target / root queues). The target pool (3 x 0x4AC), the morph vertex
// buffer (+0xE04) and the packet arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h): the anchor callback (AnchorPrimCallback, the record
// flags 0x200 and 0x100 honoured, object 4 nearer), DepthFromSize. Parameters (SilenceParams,
// fx_params.h): the numbers above, defaults = the game's; the layout is tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace silence122
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25214B4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25214B8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25214D4); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25214D8; }         // pool: magic buffer, 3 x 0x4AC

	static const uint32_t ORIG_RootTask = 0x6BD980;
	static const uint32_t ORIG_TargetTask = 0x6BDAC0;
	static const uint32_t MODEL_Mute = 0x126E4C0;        // prim-model layout data (0x498-byte layout)
	static const void *const SOUND_Silence = (const void *)0x1272690;
	static const uint32_t HONOURED = ANCHOR_OFFSET_AS_IS | ANCHOR_DIAGONAL_SCALE;   // the record flags MutePart tests
	static const uint16_t MARK = 0x2000;                 // the target entity's flags word bit set while it plays

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct SilenceParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the anchor (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this
		} timing;
		struct Layout
		{
			int16_t bone;           // the target's effect point the layout is at, every tick
			struct { int16_t base, shift; } depth;  // depth = (base - the target's size word +0x26) >> shift
			struct { int16_t object, by; } nearer;  // this object of the layout drawn `by` nearer (-1: none)
		} layout;
	};
	static const SilenceParams DEFAULTS = {
		{ 10, (uint32_t)SOUND_Silence, 0, 1, 0x32, 0x3C },
		{ 0xF0, { -0x800, 5 }, { 4, 0x20 } },
	};
	static SilenceParams g_params = DEFAULTS;
	static inline const SilenceParams &P() { return g_params; }
	enum PartId { PART_LAYOUT = 1 };

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
	struct TargetNode // pool of 3 nodes of 0x4AC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		uint8_t mute[0x498]; // +0x14 prim-model layout (prim::Layout: data, frame, state)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x4AC, "Silence nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag122_silence_held.h"
#endif

namespace ff8fx
{
namespace silence122
{
	// ------------------------------------------------------------------
	// Target (0x6BDAC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const SilenceParams &pp = P();
		if (t->counter >= 0)
		{
			FX_PART(PART_LAYOUT);
			AnchorArg arg;
			GetEffectSpawnPosition(t->entity, pp.layout.bone, 0, arg.anchor);
			// never written by the original (stack word), only loaded into the GTE V0 pad
			arg.anchor[3] = 0;
			if (t->counter == pp.timing.sound_tick) BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Silence), pp.timing.sound_volume, arg.anchor);
			arg.depth = DepthFromSize(t->entity, pp.layout.depth.base, pp.layout.depth.shift);
			arg.morph = TexBase() + 0xE04;
			arg.honoured = HONOURED;
			arg.cursor = &PacketCursor();
			arg.nearer_object = pp.layout.nearer.object;
			arg.nearer_by = pp.layout.nearer.by;
			// 30 fps layer: see mag122_silence_held.inc
			FX_HELD(held_note_play(t, &arg);)
			PlayAtAnchor(t->mute, &arg);
			FX_PART(0);
		}
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= pp.timing.end_tick)
		{
			*(uint16_t *)t->entity &= (uint16_t)~MARK;
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BD980)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag122_silence_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1124;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x11124;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x4AC, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x128);
				t->action = (int16_t)(uint16_t)r->action;
				uint8_t *entity = Entity(Ctx()->actions[t->action].targets[0]);
				t->entity = entity;
				*(uint8_t *)(entity + 1) |= MARK >> 8;
				DecodeModelPrimLayout(MODEL_Mute, t->mute, 0x498);
				r->action++;
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
	// Parameter table (fx_params.h): what an editor shows of SilenceParams
	// ------------------------------------------------------------------
	namespace silence122
	{
		using fxp::Field;
#define SILENCE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(SilenceParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn; each target task's clock: its sound, its result, its end (draws nothing)" },
			{ PART_LAYOUT, "layout", "Layout", "The layout played on the action's target, at one of its effect points every tick" },
		};
		static const fxp::Group GROUPS[] = {
			{ "layout.depth", "Depth", "Its drawing depth: (Base - the target's size word) >> Shift" },
			{ "layout.nearer", "Nearer object", "This object of the layout is drawn By nearer (-1: none)" },
		};
		static const Field FIELDS[] = {
			SILENCE_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			SILENCE_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			SILENCE_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the anchor when the target tick == this", I16, 0, 200),
			SILENCE_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 255),
			SILENCE_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			SILENCE_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this", I16, 0, 200),
			SILENCE_FIELD("layout", layout.bone, "Bone", "The target's effect point the layout is at (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			SILENCE_FIELD("layout", layout.depth.base, "Base", "", I16, -0x4000, 0x4000),
			SILENCE_FIELD("layout", layout.depth.shift, "Shift", "", I16, 0, 15),
			SILENCE_FIELD("layout", layout.nearer.object, "Object", "", I16, -1, 63),
			SILENCE_FIELD("layout", layout.nearer.by, "By", "", I16, -0x1000, 0x1000),
		};
#undef SILENCE_FIELD
	}

	void register_mag122_silence()
	{
		fxp::register_params({ 122, "Silence", &silence122::g_params, &silence122::DEFAULTS, sizeof(silence122::SilenceParams),
		                       silence122::PARTS, (int)(sizeof(silence122::PARTS) / sizeof(silence122::PARTS[0])),
		                       silence122::FIELDS, (int)(sizeof(silence122::FIELDS) / sizeof(silence122::FIELDS[0])),
		                       silence122::GROUPS, (int)(sizeof(silence122::GROUPS) / sizeof(silence122::GROUPS[0])) });
		register_port(silence122::ORIG_RootTask, (void *)silence122::RootTask, "S122 RootTask", 122);
		register_port(silence122::ORIG_TargetTask, (void *)silence122::TargetTask, "S122 TargetTask", 122);
		// 30 fps layer: see mag122_silence_held.inc
		FX_HELD(register_mag122_held();)
	}
	FX_LAB_MODULE(register_mag122_silence);
}

#ifdef FF8_FX_HELD
#include "mag122_silence_held.inc"
#endif
