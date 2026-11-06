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

// Effect 121: Blind (spell, MAG_121_*).
//
// Structure (setup MAG_121_BLIND 0x6BDE80 -> _Init 0x6BDEB0, file loader 0x6BDE90 = the texture
// file named at 0x1277634; the setup starts camera animation 0x1276E00 and queues the TIM):
//   RootTask (0x6BDF20) - alternates the packet arena (magic buffer + 0x1B34 / + 0x19B34), clears
//     the module's screen flash level; on counter 1 of a 10-tick cycle sets up the target pool
//     (first time) and starts the target task of the next action on its first target (no wait);
//     runs the target queue, then sets the screen flash to the level the target tasks raised;
//     ends when the queue is empty.
//   Target (0x6BE080) - node 0x83C, one per action: raises the flash level (up over 22..25 by
//     625 per tick, down over 26..33 from 0x138 * 8); follows the target's effect anchor 0xF0 for
//     20 ticks; plays the prim-model layout 0x12726A0 with the callback BlindPart (0x6BE1C0) every
//     tick (sound at 4, screen fades at 13, 18, 21, damage at 32); ends when the layout is over.
//   BlindPart (0x6BE1C0) - one object of the layout: record flag 0x800 = the offset is rotated by
//     the object's own rotation first; flag 0x200 = the offset stays in screen axes (the anchor
//     through the camera, the offset added unrotated, the rotation not composed with the camera),
//     else anchor + offset through the camera; flag 0x100 = scale by a diagonal matrix product,
//     else scale3DMatrix. Objects 0 and 3 are drawn by QuadsAdditive (0x6BE4E0: only the model's
//     textured quads, flat 0x6AE420 and gouraud 0x6BE5C0, each after an additive draw-mode
//     primitive), the others by Effect_RenderPrimModel (0x572200); both with OT shift 3.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25214F8..0x2521540 (screen flash level 0x25214F8, texture file, magic buffer
// base, context, root pool, packet cursor 0x252151C, target / root queues). The target pool
// (3 x 0x83C), the morph vertex buffer (+0x18B4) and the packet arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h): the anchor callback (AnchorPrimCallback, the record
// flags 0x800, 0x200 and 0x100 honoured, the engine renderer with OT shift 3, objects 0 and 3 as
// AdditiveQuads). Parameters (BlindParams, fx_params.h): the numbers above, defaults = the game's;
// the layout is tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace blind121
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline int32_t &FlashLevel() { return var<int32_t>(0x25214F8); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521500); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521504); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252151C); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521520; }         // pool: magic buffer, 3 x 0x83C

	static const uint32_t ORIG_RootTask = 0x6BDF20;
	static const uint32_t ORIG_TargetTask = 0x6BE080;
	static const uint32_t MODEL_Blind = 0x12726A0;       // prim-model layout data (0x820-byte layout)
	static const void *const SOUND_Blind = (const void *)0x1277630;
	static const uint32_t HONOURED = ANCHOR_OFFSET_TURNED | ANCHOR_OFFSET_AS_IS | ANCHOR_DIAGONAL_SCALE;   // the record flags BlindPart tests

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct BlindParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
		} timing;
		struct Layout
		{
			int16_t bone;           // the target's effect point the layout is at...
			int16_t follow;         // ...while the target counter is below this (then it stays)
			int16_t ot_shift;       // the renderers' depth shift
			struct { int16_t a, b; } additive;   // these two objects of the layout drawn as their textured quads only, additive (-1: none)
		} layout;
		struct Flash
		{
			int16_t from, peak, to; // the screen flash: up over from..peak - 1, down over peak..to (the highest of all targets)
			int16_t up, down;       // its level a tick up from `from`, down to `to`
			struct { int16_t first, second, third; } fade_at;   // the screen fades started at these target ticks...
			struct { int16_t a, b, c, d; } fade;                // ...ScreenFadeTask's four values
		} flash;
	};
	static const BlindParams DEFAULTS = {
		{ 10, (uint32_t)SOUND_Blind, 4, 0x101, 0x20 },
		{ 0xF0, 0x14, 3, { 0, 3 } },
		{ 0x16, 0x1A, 0x22, 625, 0x138, { 0xD, 0x12, 0x15 }, { 0, 1, 0, 0x60 } },
	};
	static BlindParams g_params = DEFAULTS;
	static inline const BlindParams &P() { return g_params; }
	enum PartId { PART_LAYOUT = 1, PART_FLASH = 2 };

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
	struct TargetNode // pool of 3 nodes of 0x83C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		int16_t pos[4];    // +0x10 effect anchor 0xF0 x, y, z (4th word stays 0)
		uint8_t *entity;   // +0x18 target entity
		uint8_t layout[0x820]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x83C, "Blind nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag121_blind_held.h"
#endif

namespace ff8fx
{
namespace blind121
{
	// ------------------------------------------------------------------
	// Target (0x6BE080)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const BlindParams &pp = P();
		// the screen flash: up over from..peak - 1, down over peak..to (the highest level of all targets;
		// the flash part draws nothing of its own)
		const int32_t v = ScreenFlashRiseFall(t->counter, pp.flash.from, pp.flash.peak, pp.flash.to, pp.flash.up, pp.flash.down);
		if (v >= 0 && v > FlashLevel()) FlashLevel() = v;
		FX_PART(PART_LAYOUT);
		if (t->counter < pp.layout.follow) GetEffectSpawnPosition(t->entity, pp.layout.bone, 0, t->pos);
		AnchorArg arg;
		memcpy(arg.anchor, t->pos, 8);
		arg.morph = TexBase() + 0x18B4;
		arg.honoured = HONOURED;
		arg.cursor = &PacketCursor();
		arg.engine_draw = true;
		arg.ot_shift = pp.layout.ot_shift;
		arg.additive_objects = ObjectBit(pp.layout.additive.a) | ObjectBit(pp.layout.additive.b);
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(held_note_play(t, &arg);)
		const int alive = PlayAtAnchor(t->layout, &arg);
		FX_PART(0);
		if (t->counter == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Blind), pp.timing.sound_volume, pos);
		}
		const BlindParams::Flash &f = pp.flash;
		if (t->counter == f.fade_at.first || t->counter == f.fade_at.second || t->counter == f.fade_at.third)
			ScreenFadeTask(f.fade.a, f.fade.b, f.fade.c, f.fade.d);
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (alive == 0) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6BDF20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1B34;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19B34;
			r->arena = 1;
		}
		FlashLevel() = 0;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x83C, 3);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				uint8_t *entity = Entity(Ctx()->actions[r->action].targets[0]);
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				if (t)
				{
					Memset32(&t->counter, 0, 0x20C);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					DecodeModelPrimLayout(MODEL_Blind, t->layout, 0x820);
					r->action++;
				}
			}
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QTargets());
		else a = (int)n;
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of BlindParams
	// ------------------------------------------------------------------
	namespace blind121
	{
		using fxp::Field;
#define BLIND_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(BlindParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn; each target task's clock: its sound, its result (draws nothing)" },
			{ PART_LAYOUT, "layout", "Layout", "The layout played on the action's target, at one of its effect points; the task ends with it" },
			{ PART_FLASH, "flash", "Flash", "The screen flash all the target tasks raise, and the screen fades they start" },
		};
		static const fxp::Group GROUPS[] = {
			{ "layout.additive", "Additive objects", "These two objects of the layout are drawn as their textured quads only, additive (-1: none)" },
			{ "flash.fade_at", "Fade ticks", "A screen fade starts at each of these target ticks" },
			{ "flash.fade", "Fade", "ScreenFadeTask's four values" },
		};
		static const Field FIELDS[] = {
			BLIND_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			BLIND_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			BLIND_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			BLIND_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			BLIND_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			BLIND_FIELD("layout", layout.bone, "Bone", "The target's effect point the layout is at (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			BLIND_FIELD("layout", layout.follow, "Follow until", "The layout follows that point while the target tick is below this, then stays", I16, 0, 200),
			BLIND_FIELD("layout", layout.ot_shift, "Depth shift", "The renderers' depth >> this picks the drawing bucket", I16, 0, 15),
			BLIND_FIELD("layout", layout.additive.a, "Object", "", I16, -1, 31),
			BLIND_FIELD("layout", layout.additive.b, "Object", "", I16, -1, 31),
			BLIND_FIELD("flash", flash.from, "From", "The flash rises from this target tick (Up a tick)", I16, 0, 200),
			BLIND_FIELD("flash", flash.peak, "Peak", "...to this one, then falls (Down a tick)", I16, 0, 200),
			BLIND_FIELD("flash", flash.to, "To", "...to 0 at this one", I16, 0, 200),
			BLIND_FIELD("flash", flash.up, "Up", "Its level a tick while it rises", I16, 0, 0x7FFF),
			BLIND_FIELD("flash", flash.down, "Down", "Its level a tick while it falls", I16, 0, 0x7FFF),
			BLIND_FIELD("flash", flash.fade_at.first, "First", "", I16, 0, 200),
			BLIND_FIELD("flash", flash.fade_at.second, "Second", "", I16, 0, 200),
			BLIND_FIELD("flash", flash.fade_at.third, "Third", "", I16, 0, 200),
			BLIND_FIELD("flash", flash.fade.a, "A", "", I16, -0x8000, 0x7FFF),
			BLIND_FIELD("flash", flash.fade.b, "B", "", I16, -0x8000, 0x7FFF),
			BLIND_FIELD("flash", flash.fade.c, "C", "", I16, -0x8000, 0x7FFF),
			BLIND_FIELD("flash", flash.fade.d, "D", "", I16, -0x8000, 0x7FFF),
		};
#undef BLIND_FIELD
	}

	void register_mag121_blind()
	{
		fxp::register_params({ 121, "Blind", &blind121::g_params, &blind121::DEFAULTS, sizeof(blind121::BlindParams),
		                       blind121::PARTS, (int)(sizeof(blind121::PARTS) / sizeof(blind121::PARTS[0])),
		                       blind121::FIELDS, (int)(sizeof(blind121::FIELDS) / sizeof(blind121::FIELDS[0])),
		                       blind121::GROUPS, (int)(sizeof(blind121::GROUPS) / sizeof(blind121::GROUPS[0])) });
		register_port(blind121::ORIG_RootTask, (void *)blind121::RootTask, "B121 RootTask", 121);
		register_port(blind121::ORIG_TargetTask, (void *)blind121::TargetTask, "B121 TargetTask", 121);
		// 30 fps layer: see mag121_blind_held.inc
		FX_HELD(register_mag121_held();)
	}
	FX_LAB_MODULE(register_mag121_blind);
}

#ifdef FF8_FX_HELD
#include "mag121_blind_held.inc"
#endif
