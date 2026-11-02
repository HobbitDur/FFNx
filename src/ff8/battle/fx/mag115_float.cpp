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

// Effect 115: Float (spell, MAG_115_*).
//
// Structure (setup MAG_115_FLOAT 0x6C9380 -> _Init 0x6C93B0, file loader 0x6C9390 = the texture
// file named at 0x12BA300; the setup starts camera animation 0x12B9928 and queues the TIM):
//   RootTask (0x6C9420) - alternates the packet arena (magic buffer + 0x1158 / + 0x19158); on
//     counter 1 of a 10-tick cycle sets up the target pool (first time) and starts the target task
//     of the next action on its first target; runs the target queue, ends when it is empty.
//   Target (0x6C9580) - node 0x568, one per action: the lift glyph prim-model layout 0x12B5780 at
//     the target's feet (bone 0xF1 spawn position, height of the target), from tick 12 a second
//     layout 0x12B7768 at the bone 0xF0 spawn position whose depth offset follows the depth of a
//     fixed point of the model (0x12BA2F8); both turned with the target and scaled by its size,
//     drawn by the callback FloatPart (0x6C9760); sound at 0, the status at 64; ends at 70.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25217D0..0x2521818 (texture file, magic buffer base, context, root pool,
// packet cursor 0x25217F4, target / root queues). The target pool (3 x 0x568), the morph vertex
// buffer (+0x1038) and the packet arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h): the frame callback (FramePrimCallback, record flag
// 0x8000 honoured), TargetFrame, SizeScale, PointDepth. Parameters (FloatParams, fx_params.h): the
// numbers above, defaults = the game's; the layouts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace float115
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	inline uint32_t &TexBase() { return var<uint32_t>(0x25217D4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25217D8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25217F4); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25217F8; }         // pool: magic buffer, 3 x 0x568

	static const uint32_t ORIG_RootTask = 0x6C9420;
	static const uint32_t ORIG_TargetTask = 0x6C9580;
	static const uint32_t MODEL_Glyph = 0x12B5780;      // prim-model layout data (0x2E8-byte layout)
	static const uint32_t MODEL_Upper = 0x12B7768;      // prim-model layout data (0x26C-byte layout)
	static const int16_t *const DEPTH_Point = (const int16_t *)0x12BA2F8; // SVECTOR
	static const void *const SOUND_Float = (const void *)0x12BA2F4;
	static const uint32_t HONOURED = FRAME_NOT_COMPOSED;  // the record flags FloatPart tests

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct FloatParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this
		} timing;
		struct Glyph
		{
			int16_t bone;           // its x / z: the target's effect point; its y: the target's feet (+0x24)
		} glyph;
		struct Upper
		{
			int16_t bone;           // the target's effect point it is at
			int16_t from_tick;      // drawn from this target tick
		} upper;
		struct Scale
		{
			int16_t base, shift;    // both layouts scaled by ((the target's size >> 1) + base) >> shift
		} scale;
	};
	static const FloatParams DEFAULTS = {
		{ 10, (uint32_t)SOUND_Float, 0, 1, 0x40, 0x46 },
		{ 0xF1 },
		{ 0xF0, 0xC },
		{ 0x800, 2 },
	};
	static FloatParams g_params = DEFAULTS;
	static inline const FloatParams &P() { return g_params; }
	enum PartId { PART_GLYPH = 1, PART_UPPER = 2 };

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x568 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		uint8_t glyph[0x2E8]; // +0x14 prim-model layout (prim::Layout: data, frame, state)
		uint8_t upper[0x26C]; // +0x2FC prim-model layout
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x568, "Float nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag115_float_held.h"
#endif

namespace ff8fx
{
namespace float115
{
	// ------------------------------------------------------------------
	// Target (0x6C9580)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const FloatParams &pp = P();
		if (t->counter == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Float), pp.timing.sound_volume, pos);
		}
		FrameArg arg;
		const int32_t s = SizeScale(t->entity, pp.scale.base, pp.scale.shift);
		arg.scaled = 1;
		arg.scale[0] = s;
		arg.scale[2] = s;
		arg.scale[1] = s;
		arg.honoured = HONOURED;
		arg.cursor = &PacketCursor();
		int16_t sp[4];
		GetEffectSpawnPosition(t->entity, pp.glyph.bone, 0, sp);
		int32_t point[3] = { sp[0], *(const int16_t *)(t->entity + 0x24), sp[2] };
		// 30 fps layer: see mag115_float_held.inc
		FX_HELD(held_note_play(t, t->glyph, &arg);)
		TargetFrame(t->entity, point, &arg.m);
		arg.morph = TexBase() + 0x1038;
		arg.depth = 0;
		FX_PART(PART_GLYPH);
		PlayInFrame(t->glyph, &arg);
		if (t->counter >= pp.upper.from_tick)
		{
			GetEffectSpawnPosition(t->entity, pp.upper.bone, 0, sp);
			point[0] = sp[0];
			point[1] = sp[1];
			point[2] = sp[2];
			// 30 fps layer: see mag115_float_held.inc
			FX_HELD(held_note_play(t, t->upper, &arg);)
			TargetFrame(t->entity, point, &arg.m);
			arg.depth = PointDepth(&arg.m, t->entity, DEPTH_Point);
			GteSetTransVector(&arg.m);
			FX_PART(PART_UPPER);
			PlayInFrame(t->upper, &arg);
		}
		FX_PART(0);
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= pp.timing.end_tick) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6C9420)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag115_float_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1158;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x19158;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x568, 3);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				if (t)
				{
					Memset32(&t->counter, 0, 0x157);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					DecodeModelPrimLayout(MODEL_Glyph, t->glyph, 0x2E8);
					DecodeModelPrimLayout(MODEL_Upper, t->upper, 0x26C);
					r->action++;
				}
			}
		}
		const int a = r->started ? ExecuteTaskQueue(QTargets()) : (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of FloatParams
	// ------------------------------------------------------------------
	namespace float115
	{
		using fxp::Field;
#define FLOAT_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(FloatParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn; each target task's clock: its sound, its result, its end; both layouts' size (draws nothing)" },
			{ PART_GLYPH, "glyph", "Glyph", "The lift glyph at the target's feet, turned with it" },
			{ PART_UPPER, "upper", "Upper layout", "The second layout at one of the target's effect points, turned with it, its depth from a point of it" },
		};
		static const fxp::Group GROUPS[] = {
			{ "scale", "Size", "Both layouts scaled by ((the target's size >> 1) + Base) >> Shift (4096 = their own size)" },
		};
		static const Field FIELDS[] = {
			FLOAT_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			FLOAT_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			FLOAT_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			FLOAT_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 255),
			FLOAT_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			FLOAT_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this", I16, 0, 200),
			FLOAT_FIELD("timing", scale.base, "Base", "", I16, -0x4000, 0x4000),
			FLOAT_FIELD("timing", scale.shift, "Shift", "", I16, 0, 15),
			FLOAT_FIELD("glyph", glyph.bone, "Bone", "The target's effect point giving its x and z (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			FLOAT_FIELD("upper", upper.bone, "Bone", "The target's effect point it is at", I16, 0, 0xFF),
			FLOAT_FIELD("upper", upper.from_tick, "From tick", "Drawn from this target tick", I16, 0, 200),
		};
#undef FLOAT_FIELD
	}

	void register_mag115_float()
	{
		fxp::register_params({ 115, "Float", &float115::g_params, &float115::DEFAULTS, sizeof(float115::FloatParams),
		                       float115::PARTS, (int)(sizeof(float115::PARTS) / sizeof(float115::PARTS[0])),
		                       float115::FIELDS, (int)(sizeof(float115::FIELDS) / sizeof(float115::FIELDS[0])),
		                       float115::GROUPS, (int)(sizeof(float115::GROUPS) / sizeof(float115::GROUPS[0])) });
		register_port(float115::ORIG_RootTask, (void *)float115::RootTask, "F115 RootTask", 115);
		register_port(float115::ORIG_TargetTask, (void *)float115::TargetTask, "F115 TargetTask", 115);
		// 30 fps layer: see mag115_float_held.inc
		FX_HELD(register_mag115_held();)
	}
	FX_LAB_MODULE(register_mag115_float);
}

#ifdef FF8_FX_HELD
#include "mag115_float_held.inc"
#endif
