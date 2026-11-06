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

// Effect 111: Aura (spell, MAG_111_*).
//
// Structure (setup MAG_111_AURA 0x6CD920 -> _Init 0x6CD950, file loader 0x6CD930 = the texture
// file named at 0x12E0644; the setup starts camera animation 0x12DFD9C and queues the TIM):
//   RootTask (0x6CD9C0) - alternates the packet arena (magic buffer + 0xE28 / + 0x18E28); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the target task of the
//     next action on its first target, unless a target task of 28 ticks or less still runs on that
//     target (the cycle then restarts); runs the target and flame queues, ends when both are empty.
//   Target (0x6CDB60) - node 0x360, one per action: the aura prim-model layout 0x12DB9E0 played at
//     the target's feet (scaled by the square root of its size) with the callback AuraPart
//     (0x6CDE10); flames: 4 per tick over 15..18 around the target; a screen fade at 28 and a
//     glow sprite sequence (0x12DB75C) over 28..33; sound at 0, the status at 29; ends at 34.
//   Flame (0x6CE020) - node 0x20: the flame prim model 0x12DB818 turning around the target's
//     vertical axis (angle += speed each tick) at a random radius, fading in over 0..7 and out
//     from 13, ends after 20 draws.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521970..0x25219C8 (texture file, magic buffer base, context, root pool,
// packet cursor 0x2521994, flame / target / root queues). The target pool (3 x 0x360), the flame
// pool (32 x 0x20), the morph vertex buffer (+0xE20) and the packet arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h): the table callback (TablePrimCallback, the aura's
// per-object words), SqrtSizeFrame, FeetPoint, FadeInOut, DrawSpinningModel (the flames),
// DrawSequenceSprite (the glow). Parameters (AuraParams, fx_params.h): the numbers above, defaults =
// the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace aura111
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521974); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521978); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521994); }
	inline TaskQueue *QFlames() { return (TaskQueue *)0x2521998; }          // pool: + 0xA20, 0x20 x 0x20
	inline TaskQueue *QTargets() { return (TaskQueue *)0x25219A8; }         // pool: magic buffer, 3 x 0x360

	static const uint32_t ORIG_RootTask = 0x6CD9C0;
	static const uint32_t ORIG_TargetTask = 0x6CDB60;
	static const uint32_t ORIG_FlameTask = 0x6CE020;
	static const uint32_t MODEL_Aura = 0x12DB9E0;        // prim-model layout data (0x34C-byte layout)
	static const uint32_t MODEL_Flame = 0x12DB818;       // prim model
	static const uint32_t SEQ_Glow = 0x12DB75C;
	static const uint32_t PART_Flags = 0x12E061C;        // per-object words of the aura layout
	static const void *const SOUND_Aura = (const void *)0x12E0618;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct AuraParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's target task every this many ticks...
			int16_t busy_tick;      // ...unless a target task on that target is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (target counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the target task ends once its counter reaches this
		} timing;
		struct Aura
		{
			int16_t bone;           // at this effect point's x / z, the target's feet
			int16_t size_shift;     // scaled by sqrt(the target's size << this)
		} aura;
		struct Flames
		{
			fxp::Window window;     // target ticks they are spawned at...
			int16_t count;          // ...this many a tick, a quarter turn apart
			int16_t bone;           // around this effect point...
			int16_t lift;           // ...at the target's feet + this (y goes down)
			int16_t angle_step;     // between the flames of one tick
			struct { int16_t base, mask; } radius;   // radius = base + (rand & mask)
			struct { int16_t base, mask; } speed;    // turn a tick = base + (rand & mask), either way (rand & 1)
			struct { int16_t base, mask, in, from, out; } fade;  // strength = base + (rand & mask); in over `in` ticks, out over `out` after `from`
			int16_t nearer;         // subtracted from its depth
			int16_t mode;           // its draw mode
			int16_t frames;         // it ends once its counter reaches this
		} flames;
		struct Glow
		{
			int16_t from;           // drawn from this target tick (the screen fade then)...
			int16_t frames;         // ...this many ticks, frame = tick - from
			int16_t lift;           // at the flames' point, the target's feet + this
			int16_t rot_a, rot_b;   // the sprite's size and its angle (the shadow-rotation camera's)
		} glow;
	};
	static const AuraParams DEFAULTS = {
		{ 10, 0x1C, (uint32_t)SOUND_Aura, 0, 0x101, 0x1D, 0x22 },
		{ 0xF1, 12 },
		{ { 0xF, 0x12 }, 4, 0xF1, -0x258, 0x400, { 0x320, 0x3FF }, { 0x10, 0xF }, { 0x800, 0x7FF, 8, 0xC, 8 }, 0x100, 0xF3, 0x14 },
		{ 0x1C, 6, -0x258, 0x1000, -0x100 },
	};
	static AuraParams g_params = DEFAULTS;
	static inline const AuraParams &P() { return g_params; }
	enum PartId { PART_AURA = 1, PART_FLAMES = 2, PART_GLOW = 3 };

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x360 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		uint8_t aura[0x34C]; // +0x14 prim-model layout (prim::Layout: data, frame, state)
	};
	struct FlameNode // pool of 0x20 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10 x, y (the feet + lift), z of the target's effect point
		int16_t radius;    // +0x16 (GetEffectSpawnPosition's 4th word, replaced)
		int16_t angle;     // +0x18 around the view's axis
		int16_t speed;     // +0x1A angle step
		int16_t fade;      // +0x1C strength (0x800..0xFFF)
		int16_t pad1E;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x360 && sizeof(FlameNode) == 0x20, "Aura nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag111_aura_held.h"
#endif

namespace ff8fx
{
namespace aura111
{
	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		FlameNode *f = (FlameNode *)n;
		const AuraParams::Flames &pf = P().flames;
		// 30 fps layer: see mag111_aura_held.inc
		FX_HELD(held_note_flame(f);)
		FX_PART(PART_FLAMES);
		DrawSpinningModel(MODEL_Flame, f->pos, f->angle, f->radius, FadeInOut(f->counter, f->fade, pf.fade.in, pf.fade.from, pf.fade.out),
		                  (uint32_t)(uint16_t)pf.mode, pf.nearer, PacketCursor());
		FX_PART(0);
		if (f->counter >= pf.frames) return TASK_END;
		f->angle = (int16_t)(f->angle + f->speed);
		f->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6CDB60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		const AuraParams &pp = P();
		if (t->counter >= 0)
		{
			FX_PART(PART_AURA);
			TableArg arg;
			// 30 fps layer: see mag111_aura_held.inc
			FX_HELD(Mat4x3 held_local;)
			SqrtSizeFrame(t->entity, pp.aura.size_shift, pp.aura.bone, &arg.m FX_HELD(, &held_local));
			FX_HELD(held_note_play(t, &held_local);)
			arg.flags = PART_Flags;
			arg.morph = TexBase() + 0xE20;
			arg.cursor = &PacketCursor();
			PlayWithTable(t->aura, &arg);
			FX_PART(0);
		}
		if (pp.flames.window.has(t->counter))
		{
			// one turn and speed for the tick's flames, a quarter turn apart
			int32_t angle = CrtRand();
			int32_t speed = (CrtRand() & pp.flames.speed.mask) + pp.flames.speed.base;
			if (CrtRand() & 1) speed = -speed;
			for (int k = 0; k < pp.flames.count; k++)
			{
				FlameNode *f = (FlameNode *)AddTaskToQueue(QFlames(), ORIG_FlameTask);
				if (f)
				{
					Memset32(&f->counter, 0, 5);
					FeetPoint(t->entity, pp.flames.bone, pp.flames.lift, f->pos);
					const int32_t r = CrtRand() & pp.flames.radius.mask;
					f->angle = (int16_t)angle;
					f->speed = (int16_t)speed;
					f->radius = (int16_t)(r + pp.flames.radius.base);
					f->fade = (int16_t)((CrtRand() & pp.flames.fade.mask) + pp.flames.fade.base);
				}
				angle += pp.flames.angle_step;
			}
		}
		const int32_t e = t->counter - pp.glow.from;
		if ((uint32_t)e < (uint32_t)pp.glow.frames)
		{
			if (e == 0) ScreenFadeTask(0, 1, 0, 0x80);
			int16_t pos[4];
			FeetPoint(t->entity, pp.flames.bone, pp.glow.lift, pos);
			// 30 fps layer: see mag111_aura_held.inc
			FX_HELD(held_note_glow(pos, e);)
			FX_PART(PART_GLOW);
			DrawSequenceSprite(pos, SEQ_Glow, (int16_t)e, pp.glow.rot_a, pp.glow.rot_b, PacketCursor());
			FX_PART(0);
		}
		if (t->counter == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(t->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Aura), pp.timing.sound_volume, pos);
		}
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= pp.timing.end_tick) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CD9C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		const AuraParams &pp = P();
		// 30 fps layer: see mag111_aura_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0xE28;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x18E28;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x360, 3);
				InitTaskQueuePool(QFlames(), (void *)(TexBase() + 0xA20), 0x20, 0x20);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a target task at busy_tick or before runs on this target
				TargetNode *pool = (TargetNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= pp.timing.busy_tick)
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
						Memset32(&t->counter, 0, 0xD5);
						t->action = (int16_t)(uint16_t)r->action;
						t->entity = entity;
						DecodeModelPrimLayout(MODEL_Aura, t->aura, 0x34C);
						r->action++;
					}
				}
			}
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QFlames());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= pp.timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of AuraParams
	// ------------------------------------------------------------------
	namespace aura111
	{
		using fxp::Field;
#define AURA_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(AuraParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one target task per action, in turn (not while one is young on that target); each target task's clock: its sound, its result, its end (draws nothing)" },
			{ PART_AURA, "aura", "Aura", "The aura layout at the target's feet, scaled by the square root of its size, each object's draw mode from a table" },
			{ PART_FLAMES, "flames", "Flames", "Flame models spinning around the target, a few a tick, fading in and out" },
			{ PART_GLOW, "glow", "Glow", "A flipbook at the target's feet, with a screen fade" },
		};
		static const fxp::Group GROUPS[] = {
			{ "flames.window", "Spawning", "Count flames spawned at every target tick From tick .. To tick, Angle step apart" },
			{ "flames.radius", "Radius", "Base + (random & Mask)" },
			{ "flames.speed", "Turn a tick", "Base + (random & Mask), either way at random" },
			{ "flames.fade", "Strength", "Base + (random & Mask) (4096: full), in over In ticks, out over Out ticks after From" },
		};
		static const Field FIELDS[] = {
			AURA_FIELD("timing", timing.every, "Every", "The root starts the next action's target task every this many ticks", I16, 1, 100),
			AURA_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a target task on that target is at this tick or before (the cycle restarts)", I16, -1, 200),
			AURA_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			AURA_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the target tick == this", I16, 0, 200),
			AURA_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			AURA_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the target tick == this", I16, 0, 200),
			AURA_FIELD("timing", timing.end_tick, "End tick", "The target task ends when its tick reaches this", I16, 0, 200),
			AURA_FIELD("aura", aura.bone, "Bone", "The target's effect point giving its x and z (0xF0 and up: the engine's named points)", I16, 0, 0xFF),
			AURA_FIELD("aura", aura.size_shift, "Size shift", "Scaled by sqrt(the target's size << this)", I16, 0, 16),
			AURA_FIELD("flames", flames.window.first, "From tick", "", I16, 0, 200),
			AURA_FIELD("flames", flames.window.last, "To tick", "", I16, 0, 200),
			AURA_FIELD("flames", flames.count, "Count", "", I16, 0, 16),
			AURA_FIELD("flames", flames.angle_step, "Angle step", "", I16, -0x1000, 0x1000),
			AURA_FIELD("flames", flames.bone, "Bone", "The target's effect point they turn around (at its feet)", I16, 0, 0xFF),
			AURA_FIELD("flames", flames.lift, "Lift", "Added to the target's feet (y goes down)", I16, -0x2000, 0x2000),
			AURA_FIELD("flames", flames.radius.base, "Base", "", I16, 0, 0x7FFF),
			AURA_FIELD("flames", flames.radius.mask, "Mask", "", I16, 0, 0x7FFF),
			AURA_FIELD("flames", flames.speed.base, "Base", "", I16, -0x1000, 0x1000),
			AURA_FIELD("flames", flames.speed.mask, "Mask", "", I16, 0, 0x7FFF),
			AURA_FIELD("flames", flames.fade.base, "Base", "", I16, 0, 0x1000),
			AURA_FIELD("flames", flames.fade.mask, "Mask", "", I16, 0, 0xFFF),
			AURA_FIELD("flames", flames.fade.in, "In", "", I16, 1, 64),
			AURA_FIELD("flames", flames.fade.from, "From", "", I16, 0, 200),
			AURA_FIELD("flames", flames.fade.out, "Out", "", I16, 1, 64),
			AURA_FIELD("flames", flames.nearer, "Nearer", "Subtracted from their depth", I16, -0x1000, 0x1000),
			AURA_FIELD("flames", flames.mode, "Draw mode", "The prim model's draw mode word", I16, 0, 0x7FFF),
			AURA_FIELD("flames", flames.frames, "Frames", "A flame ends once its tick reaches this", I16, 0, 200),
			AURA_FIELD("glow", glow.from, "From tick", "Drawn from this target tick (the screen fade then)", I16, 0, 200),
			AURA_FIELD("glow", glow.frames, "Frames", "Drawn this many ticks, frame = tick - From tick", I16, 0, 64),
			AURA_FIELD("glow", glow.lift, "Lift", "Added to the target's feet (y goes down)", I16, -0x2000, 0x2000),
			AURA_FIELD("glow", glow.rot_a, "Size", "The sprite's size", I16, 0, 0x7FFF),
			AURA_FIELD("glow", glow.rot_b, "Angle", "The sprite's angle", I16, -0x8000, 0x7FFF),
		};
#undef AURA_FIELD
	}

	void register_mag111_aura()
	{
		fxp::register_params({ 111, "Aura", &aura111::g_params, &aura111::DEFAULTS, sizeof(aura111::AuraParams),
		                       aura111::PARTS, (int)(sizeof(aura111::PARTS) / sizeof(aura111::PARTS[0])),
		                       aura111::FIELDS, (int)(sizeof(aura111::FIELDS) / sizeof(aura111::FIELDS[0])),
		                       aura111::GROUPS, (int)(sizeof(aura111::GROUPS) / sizeof(aura111::GROUPS[0])) });
		register_port(aura111::ORIG_RootTask, (void *)aura111::RootTask, "A111 RootTask", 111);
		register_port(aura111::ORIG_TargetTask, (void *)aura111::TargetTask, "A111 TargetTask", 111);
		register_port(aura111::ORIG_FlameTask, (void *)aura111::FlameTask, "A111 FlameTask", 111);
		// 30 fps layer: see mag111_aura_held.inc
		FX_HELD(register_mag111_held();)
	}
	FX_LAB_MODULE(register_mag111_aura);
}

#ifdef FF8_FX_HELD
#include "mag111_aura_held.inc"
#endif
