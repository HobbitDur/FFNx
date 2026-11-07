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

// Effect 118: Aero (spell, MAG_118_*).
//
// Structure (setup MAG_118_AERO 0x6C0C80 -> _Init 0x6C0CB0, file loader 0x6C0C90 = the texture
// file named at 0x128E2C4; the setup starts camera animation 0x128D5B0 and queues the TIM):
//   RootTask (0x6C0D20) - alternates the packet arena (magic buffer + 0x2584 / + 0x1A584); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the target task of the
//     next action on its first target (no wait); computes the effect camera, runs the target and
//     leaf queues, ends when both are empty.
//   Target (0x6C0EC0) - node 0x68C, one per action: sound at 8; from tick 11 plays two prim-model
//     layouts with the callback WindPart (0x6C10A0): the whirlwind 0x1287DBC at the target's feet
//     and 0x128C110 at its effect anchor, both scaled by its size; over 0..19 spawns a leaf per
//     tick; when both layouts are over: damage, ends.
//   Leaf (0x6C14B0) - node 0x3C: the leaf prim model 0x1287C44 spinning on three axes: while its
//     target task is younger than 40 ticks it grows in and circles the target, climbing (spiral
//     radius shrinking with height), then it falls, bouncing on the ground and drifting outwards,
//     and shrinks away after 17 more ticks.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25215D8..0x2521650 (texture file, magic buffer base, context, root pool,
// packet cursor 0x25215FC, effect camera 0x2521600, leaf / target / root queues). The target
// pool (3 x 0x68C), the leaf pool (64 x 0x3C), the morph vertex buffer (+0x22A4) and the packet
// arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h): its wind through the anchor callback, scaled by the
// target's size (record flags 0x1000, 0x8000, 0x100; WindPart's row scale is scale3DMatrix), the
// whirl leaves (WhirlLeafStart / WhirlLeafMove / DrawTumblingModel). Parameters (AeroParams,
// fx_params.h): the numbers above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace aero118
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25215DC); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25215E0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25215FC); }
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x2521600); }
	inline TaskQueue *QLeaves() { return (TaskQueue *)0x2521620; }          // pool: + 0x13A4, 0x40 x 0x3C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521630; }         // pool: magic buffer, 3 x 0x68C

	static const uint32_t ORIG_RootTask = 0x6C0D20;
	static const uint32_t ORIG_TargetTask = 0x6C0EC0;
	static const uint32_t ORIG_LeafTask = 0x6C14B0;
	static const uint32_t MODEL_Whirl = 0x1287DBC;       // prim-model layout data (0x4EC-byte layout)
	static const uint32_t MODEL_Gust = 0x128C110;        // prim-model layout data (0x184-byte layout)
	static const uint32_t MODEL_Leaf = 0x1287C44;        // prim model
	static const void *const SOUND_Aero = (const void *)0x128D5AC;
	// the record flags WindPart tests (0x1000: the offset through the effect camera)
	static const uint32_t HONOURED = ANCHOR_MATRIX | ANCHOR_NOT_COMPOSED | ANCHOR_DIAGONAL_SCALE;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct AeroParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
		} timing;
		struct Wind
		{
			int16_t from_tick;      // both layouts drawn from this target tick (the result when both have finished)
			struct { int16_t base, shift; } depth;   // depth = (base - the target's size) >> shift
			struct { int16_t base, shift; } scale;   // scaled by ((the target's size >> 1) + base) >> shift
		} wind;
		struct Leaves
		{
			fxp::Window window;     // one spawned every target tick of this window
			int16_t scale;          // its first size (4.12)
			int16_t speed;          // its first turn a tick
			int16_t vy;             // its first vertical speed (y goes down)
			int16_t climb;          // it climbs while its target task's tick is below this, then falls
			int16_t speed_up;       // faster from this target tick
		} leaves;
	};
	static const AeroParams DEFAULTS = {
		{ 10, (uint32_t)SOUND_Aero, 8, 1 },
		{ 0xB, { -0x800, 5 }, { 0x800, 0 } },
		{ { 0, 0x13 }, 0x200, 0x40, -4, 0x28, 0xA },
	};
	static AeroParams g_params = DEFAULTS;
	static inline const AeroParams &P() { return g_params; }
	enum PartId { PART_WIND = 1, PART_LEAVES = 2 };

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
	struct TargetNode // pool of 3 nodes of 0x68C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index (-1 once the damage is done)
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		uint8_t whirl[0x4EC]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
		uint8_t gust[0x184];  // +0x508
	};
	struct LeafNode // pool of 0x40 nodes of 0x3C bytes
	{
		TaskNode hdr;
		WhirlLeaf leaf;    // +0x0C
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x68C && sizeof(LeafNode) == 0x3C, "Aero nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag118_aero_held.h"
#endif

namespace ff8fx
{
namespace aero118
{
	// ------------------------------------------------------------------
	// Leaf (0x6C14B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LeafTask(TaskNode *n)
	{
		LeafNode *f = (LeafNode *)n;
		const AeroParams &pp = P();
		if (!WhirlLeafMove(&f->leaf, pp.leaves.climb, pp.leaves.speed_up)) return TASK_END;
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(held_note_leaf(f);)
		FX_PART(PART_LEAVES);
		DrawTumblingModel(MODEL_Leaf, f->leaf.ang, f->leaf.scale, f->leaf.pos, 0, PacketCursor());
		FX_PART(0);
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6C0EC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const AeroParams &pp = P();
		int alive = 1;
		if (t->counter == pp.timing.sound_tick) BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Aero), pp.timing.sound_volume, t->pos);
		const int32_t reach = SizeScale(t->entity, pp.wind.scale.base, pp.wind.scale.shift);
		if (t->counter >= pp.wind.from_tick)
		{
			// the whirlwind at the target's feet, the gust at its effect position, scaled by its size
			AnchorArg arg;
			arg.anchor[0] = t->pos[0];
			arg.anchor[2] = t->pos[2];
			arg.anchor[1] = *(const int16_t *)(t->entity + 0x24);
			arg.anchor[3] = 0; // never written by the original, never read
			arg.scale[2] = reach;
			arg.scale[1] = reach;
			arg.scale[0] = reach;
			arg.scaled = 1;
			arg.depth = DepthFromSize(t->entity, pp.wind.depth.base, pp.wind.depth.shift);
			arg.morph = TexBase() + 0x22A4;
			arg.honoured = HONOURED;
			arg.cursor = &PacketCursor();
			arg.matrix = &EffectCamera();
			arg.short_scale = true;
			// 30 fps layer: see mag118_aero_held.inc
			FX_HELD(held_note_play(t->whirl, &arg);)
			FX_PART(PART_WIND);
			alive = PlayAtAnchor(t->whirl, &arg);
			GetDefaultEffectPosition(t->entity, arg.anchor);
			// 30 fps layer: see mag118_aero_held.inc
			FX_HELD(held_note_play(t->gust, &arg);)
			alive |= PlayAtAnchor(t->gust, &arg);
			FX_PART(0);
		}
		if (pp.leaves.window.has(t->counter))
		{
			LeafNode *f = (LeafNode *)AddTaskToQueue(QLeaves(), ORIG_LeafTask);
			if (f)
			{
				Memset32(&f->leaf, 0, 0xC);
				WhirlLeafStart(&f->leaf, t->action, t->pos, reach, &t->hdr, pp.leaves.scale, pp.leaves.speed, pp.leaves.vy);
			}
		}
		t->counter++;
		if (alive == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
			t->action = -1;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6C0D20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x2584;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1A584;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x68C, 3);
				InitTaskQueuePool(QLeaves(), (void *)(TexBase() + 0x13A4), 0x3C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x1A0);
				t->action = (int16_t)(uint16_t)r->action;
				t->entity = Entity(Ctx()->actions[t->action].targets[0]);
				GetDefaultEffectPosition(t->entity, t->pos);
				DecodeModelPrimLayout(MODEL_Whirl, t->whirl, 0x4EC);
				DecodeModelPrimLayout(MODEL_Gust, t->gust, 0x184);
				r->action++;
			}
		}
		EffectCameraMatrix(&Camera(), &EffectCamera());
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QLeaves());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of AeroParams
	// ------------------------------------------------------------------
	namespace aero118
	{
		using fxp::Field;
#define AERO_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(AeroParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn; each target task's clock: its sound; the result when both layouts have finished (draws nothing)" },
			{ PART_WIND, "wind", "Wind", "The whirlwind at the target's feet and the gust at its effect position, scaled by its size" },
			{ PART_LEAVES, "leaves", "Leaves", "A leaf a tick circling the target and climbing, then falling and bouncing away" },
		};
		static const fxp::Group GROUPS[] = {
			{ "wind.depth", "Depth", "(Base - the target's size) >> Shift" },
			{ "wind.scale", "Size", "Both layouts scaled by ((the target's size >> 1) + Base) >> Shift (4096 = their own size); the leaves' reach too" },
			{ "leaves.window", "Spawning", "One leaf spawned at every target tick From tick .. To tick" },
		};
		static const Field FIELDS[] = {
			AERO_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			AERO_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			AERO_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			AERO_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 255),
			AERO_FIELD("wind", wind.from_tick, "From tick", "Both layouts drawn from this target tick", I16, 0, 200),
			AERO_FIELD("wind", wind.depth.base, "Base", "", I16, -0x4000, 0x4000),
			AERO_FIELD("wind", wind.depth.shift, "Shift", "", I16, 0, 15),
			AERO_FIELD("wind", wind.scale.base, "Base", "", I16, -0x4000, 0x4000),
			AERO_FIELD("wind", wind.scale.shift, "Shift", "", I16, 0, 15),
			AERO_FIELD("leaves", leaves.window.first, "From tick", "", I16, 0, 200),
			AERO_FIELD("leaves", leaves.window.last, "To tick", "", I16, 0, 200),
			AERO_FIELD("leaves", leaves.scale, "Size", "A leaf's first size (4096 = its own); it grows to 4096", I16, 0, 0x1000),
			AERO_FIELD("leaves", leaves.speed, "Turn", "Its first turn a tick", I16, -0x1000, 0x1000),
			AERO_FIELD("leaves", leaves.vy, "Climb", "Its first vertical speed (y goes down)", I16, -0x400, 0x400),
			AERO_FIELD("leaves", leaves.climb, "Climb until", "It climbs while its target task's tick is below this, then falls", I16, 0, 200),
			AERO_FIELD("leaves", leaves.speed_up, "Faster from", "It climbs and turns faster from this target tick", I16, 0, 200),
		};
#undef AERO_FIELD
	}

	void register_mag118_aero()
	{
		fxp::register_params({ 118, "Aero", &aero118::g_params, &aero118::DEFAULTS, sizeof(aero118::AeroParams),
		                       aero118::PARTS, (int)(sizeof(aero118::PARTS) / sizeof(aero118::PARTS[0])),
		                       aero118::FIELDS, (int)(sizeof(aero118::FIELDS) / sizeof(aero118::FIELDS[0])),
		                       aero118::GROUPS, (int)(sizeof(aero118::GROUPS) / sizeof(aero118::GROUPS[0])) });
		register_port(aero118::ORIG_RootTask, (void *)aero118::RootTask, "A118 RootTask", 118);
		register_port(aero118::ORIG_TargetTask, (void *)aero118::TargetTask, "A118 TargetTask", 118);
		register_port(aero118::ORIG_LeafTask, (void *)aero118::LeafTask, "A118 LeafTask", 118);
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(register_mag118_held();)
	}
	FX_LAB_MODULE(register_mag118_aero);
}

#ifdef FF8_FX_HELD
#include "mag118_aero_held.inc"
#endif
