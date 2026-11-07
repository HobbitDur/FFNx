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

// Effect 108: Berserk (spell, MAG_108_*).
//
// Structure (setup MAG_108_BERSERK 0x6CFCD0 -> _Init 0x6CFD00, file loader 0x6CFCE0 = the texture
// file named at 0x12EC34C (mag107.tim); the setup starts camera animation 0x12EB9D4 and queues the
// TIM):
//   RootTask (0x6CFD70) - alternates the packet arena (magic buffer + 0x554C / + 0x1D54C); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the main task of the
//     next action on its first target, unless a main task of 43 ticks or less still runs on that
//     target (the cycle then restarts); runs the main, swirl and particle queues, ends when all
//     three are empty.
//   Main (0x6CFF60, "MainVisual_Tick") - node 0x8C4, one per action (spawn anchor = effect bone
//     0xF1 at the target's height, effect anchor = GetDefaultEffectPosition): over 0..3 a swirl
//     task per tick; a flash sprite sequence (0x12E6DDC) over 17..22; the glow prim model
//     0x12E7010 over 17..31 (uniform scale 0x400 + sin, fade rising); three embers per tick over
//     17..31 around the spawn anchor, a spark every other tick over 9..16 around the effect anchor
//     (random); over 10..17 eight light rays per tick (0x6D0B40) in the node's ray table
//     (32 x 0x30); a screen fade at 17; the burst prim-model layout 0x12E7BC0 with the callback
//     BurstPart (0x6D03D0) over 17..52; every tick the live rays are drawn and updated (0x6D0600);
//     sound at 0, the status at 43, ends from 53 once no ray is left.
//   Swirl (0x6D0C20) - node 0x420: a textured ribbon of up to 32 positions around the effect anchor
//     (0x6D0DC0 push, 0x6D0E50 draw: Gouraud-shaded textured quads between consecutive positions,
//     each between two texture-window packets); over its ticks 0..6 four positions per tick, the
//     position's brightness rises then falls; ends after tick 8 once fewer than two positions are
//     left.
//   Particle (0x6D1250) - node 0x1C: a sprite sequence in place, frame = its counter; the embers
//     (0x12E6E98) rise, decelerating (14 frames), the sparks (0x12E6C48) stay (15 frames).
// Direct calls into other spell modules (ported here): MAG_103_sub_6D67B0 (dword copy),
// MAG_011_sub_6CF070 (z rotation matrix), 0x6D9CF0 (Blizzaga's ribbon point).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521AC0..0x2521B30 (texture file, magic buffer base, context, root pool,
// packet cursor 0x2521AE4, particle / swirl / main / root queues; 0x2521B28 / 0x2521B2C are handed
// to the ray spawner, which never reads them). The main pool (3 x 0x8C4), the swirl pool
// (12 x 0x420, + 0x1A4C), the particle pool (64 x 0x1C, + 0x4BCC), the morph vertex buffer
// (+ 0x52CC) and the packet arenas are in the magic buffer.
// Built from the parts library (fx_layouts.h, fx_rays.h, fx_ribbon.h): the table callback
// (TablePrimCallback, the camera as its frame, the spawn point added: Aura's words-per-object
// draw modes), the swirl ribbon (RibbonStart / RibbonTick), the light rays (RaySpawn / RayDraw),
// the sprite motes (MoteDraw / MoteStep), the pulsing glow (PulseModelDraw), the flash sequence
// (DrawSequenceSprite). Parameters (BerserkParams, fx_params.h): the numbers above, defaults = the
// game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include "fx_rays.h"
#include "fx_ribbon.h"

namespace ff8fx
{
namespace berserk108
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521AC4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521AC8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521AE4); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521AE8; }       // pool: + 0x4BCC, 0x40 x 0x1C
	inline TaskQueue *QSwirls() { return (TaskQueue *)0x2521AF8; }          // pool: + 0x1A4C, 12 x 0x420
	inline TaskQueue *QMains() { return (TaskQueue *)0x2521B08; }           // pool: magic buffer, 3 x 0x8C4

	static const uint32_t ORIG_RootTask = 0x6CFD70;
	static const uint32_t ORIG_MainTask = 0x6CFF60;
	static const uint32_t ORIG_SwirlTask = 0x6D0C20;
	static const uint32_t ORIG_ParticleTask = 0x6D1250;
	static const uint32_t MODEL_Burst = 0x12E7BC0;       // prim-model layout data (0x29C-byte layout)
	static const uint32_t MODEL_Glow = 0x12E7010;        // prim model
	static const uint32_t PART_Flags = 0x12EC330;        // per-object words of the burst layout
	static const uint32_t SEQ_Flash = 0x12E6DDC;
	static const uint32_t SEQ_Ember = 0x12E6E98;
	static const uint32_t SEQ_Spark = 0x12E6C48;
	static const uint32_t *const RAY_Colours = (const uint32_t *)0x12EC320;   // centre colour, its fade, middle colour, its fade
	static const uint8_t *const TEXWIN_Strip = (const uint8_t *)0x12EC310;    // RECT of the swirl's texture window
	static const uint8_t *const TEXWIN_Reset = (const uint8_t *)0x12EC318;    // RECT restored after each quad
	static const void *const SOUND_Berserk = (const void *)0x12EC30C;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct BerserkParams
	{
		struct Timing
		{
			int16_t every;          // the root starts the next action's main task every this many ticks...
			int16_t busy_tick;      // ...unless a main task on that target is at this tick or before (the cycle restarts)
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE3D reads its world sound id
			int16_t sound_tick;     // the sound plays at the target's effect position (main counter)...
			int16_t sound_volume;   // ...at this volume (BdPlaySE3D's 2nd argument)
			int16_t damage_tick;    // the action's target takes the result
			int16_t end_tick;       // the main task ends after this tick, once no ray is left
			int16_t fade_tick;      // the engine's screen fade (0, 1, 1, 0x80) starts
		} timing;
		struct Swirls
		{
			int16_t last_tick;      // one spawned at main ticks 0 .. this
			int16_t angle, angle_step;   // its first angle: angle + main tick x angle_step
			int16_t amp;            // its radius's wave amplitude
			int16_t push_ticks, per_tick, step;   // positions pushed over its ticks 0 .. push_ticks, this many a tick, this angle apart
			int16_t fade;           // its positions' brightness change a tick
			int16_t end_after;      // it ends after this tick once fewer than two positions are left
		} swirls;
		struct Flash
		{
			int16_t from, ticks;    // the flash sequence over these main ticks (frame = tick - from)
		} flash;
		struct Glow
		{
			int16_t from, ticks;    // the glow model over these main ticks
			int16_t scale, fade;    // its size: sin(e x 1024 / ticks) + scale; its fade: e x 2048 / ticks + fade
		} glow;
		struct Burst
		{
			int16_t from, ticks;    // the burst layout over these main ticks
		} burst;
		struct Embers
		{
			int16_t from, last;     // spawned over main ticks from .. last...
			int16_t count;          // ...this many a tick, a third of a turn apart (angle step)...
			int16_t angle_step;
			struct { int16_t base, mask; } dist;    // ...at base + (rand & mask) from the spawn point...
			int16_t rise_shift;     // ...lifted by rand >> this...
			struct { int16_t base, mask; } accel;   // ...accelerating by (int8)(base - (rand & mask)) a tick
			int16_t frames;         // an ember ends at this frame
		} embers;
		struct Sparks
		{
			int16_t from, last, every;    // one every few main ticks over from .. last
			struct { int16_t mask, offset; } spread;   // at the effect position + (rand & mask) - offset on each axis
			int16_t frames;         // a spark ends at this frame
		} sparks;
		struct Rays
		{
			int16_t from, ticks;    // spawned over these main ticks...
			int16_t count;          // ...this many a tick (the table holds 32)
			int16_t speed, length, length_vel, length_acc, width, width_vel, width_acc;   // their motion
		} rays;
	};
	static const BerserkParams DEFAULTS = {
		{ 10, 0x2B, (uint32_t)SOUND_Berserk, 0, 0x101, 0x2B, 0x35, 0x11 },
		{ 3, 0x200, 0x400, 0x800, 6, 4, 0x20, -0x28, 8 },
		{ 0x11, 6 },
		{ 0x11, 15, 0x400, 0x800 },
		{ 0x11, 0x24 },
		{ 0x11, 0x1F, 3, 0x555, { 0x96, 0xFF }, 5, { -2, 0xF }, 14 },
		{ 9, 0x10, 2, { 0x1FF, 0x100 }, 15 },
		{ 0xA, 8, 8, 0x64, 0xA0, 0xB40, -0x20, 0xA0, 0x40, -0x14 },
	};
	static BerserkParams g_params = DEFAULTS;
	static inline const BerserkParams &P() { return g_params; }
	enum PartId { PART_SWIRLS = 1, PART_FLASH = 2, PART_GLOW = 3, PART_BURST = 4, PART_EMBERS = 5, PART_SPARKS = 6, PART_RAYS = 7 };

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..every - 1, a main task spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 3 nodes of 0x8C4 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t pad10[4];
		uint8_t *entity;   // +0x14 target entity
		int16_t spawn[4];  // +0x18 GetEffectSpawnPosition(bone 0xF1) x, (entity height), z, +0x1E
		int16_t pos[4];    // +0x20 effect anchor x, y, z, height (GetDefaultEffectPosition)
		LightRay rays[32]; // +0x28
		uint8_t burst[0x29C]; // +0x628 prim-model layout (prim::Layout: data, frame, state)
	};
	struct SwirlNode // pool of 12 nodes of 0x420 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		Ribbon r;          // +0x0E (its owner: its main task, its point: that task's effect position)
	};
	struct ParticleNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		Mote m;            // +0x0C
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x8C4, "Berserk nodes");
	static_assert(sizeof(SwirlNode) == 0x420 && sizeof(ParticleNode) == 0x1C, "Berserk nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag108_berserk_held.h"
#endif

namespace ff8fx
{
namespace berserk108
{
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		const BerserkParams &pp = P();
		const bool ember = p->m.seq == SEQ_Ember;
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_particle(p);)
		FX_PART(ember ? PART_EMBERS : PART_SPARKS);
		MoteDraw(&p->m, PacketCursor());
		FX_PART(0);
		return MoteStep(&p->m, ember, ember ? pp.embers.frames : pp.sparks.frames) ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Swirl (0x6D0C20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SwirlTask(TaskNode *n)
	{
		SwirlNode *s = (SwirlNode *)n;
		const BerserkParams::Swirls &sp = P().swirls;
		const RibbonMotion mo = { sp.push_ticks, sp.per_tick, sp.step, sp.fade };
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_swirl(s);)
		FX_PART(PART_SWIRLS);
		RibbonTick(&s->r, offsetof(MainNode, pos), s->counter, mo, TEXWIN_Strip, TEXWIN_Reset, PacketCursor());
		FX_PART(0);
		s->counter++;
		if (s->counter > sp.end_after && s->r.count < 2) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x6CFF60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const BerserkParams &pp = P();
		const int32_t c0 = m->counter;
		if ((uint32_t)c0 <= (uint32_t)pp.swirls.last_tick)
		{
			SwirlNode *s = (SwirlNode *)AddTaskToQueue(QSwirls(), ORIG_SwirlTask);
			if (s)
			{
				Memset32(&s->counter, 0, 0x105);
				RibbonStart(&s->r, &m->hdr, c0 * pp.swirls.angle_step + pp.swirls.angle, pp.swirls.amp);
			}
		}
		int32_t e = m->counter - pp.flash.from;
		if ((uint32_t)e < (uint32_t)pp.flash.ticks)
		{
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_flash(m, e);)
			FX_PART(PART_FLASH);
			DrawSequenceSprite(m->pos, SEQ_Flash, (int16_t)e, 0x2000, (int32_t)0xFFFFF800, PacketCursor());
			FX_PART(0);
		}
		e = m->counter - pp.glow.from;
		if ((uint32_t)e < (uint32_t)pp.glow.ticks)
		{
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_glow(m, e);)
			FX_PART(PART_GLOW);
			PulseModelDraw(MODEL_Glow, m->pos, e, pp.glow.ticks, pp.glow.scale, pp.glow.fade, 0xF0, PacketCursor());
			FX_PART(0);
		}
		// embers a tick on a circle around the spawn point (random start angle, apart by the angle
		// step, random distance, height and deceleration)
		const BerserkParams::Embers &ep = pp.embers;
		e = m->counter - ep.from;
		if ((uint32_t)e <= (uint32_t)(ep.last - ep.from))
		{
			int32_t angle = CrtRand();
			for (int32_t k = ep.count; k > 0; k--)
			{
				const int32_t dist = (CrtRand() & ep.dist.mask) + ep.dist.base;
				ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
				if (!p) continue;
				Memset32(&p->m, 0, 4);
				p->m.seq = SEQ_Ember;
				p->m.scale = 0x800;
				p->m.pos[0] = (int16_t)((mul32(ComputeSin(angle), dist) >> 12) + m->spawn[0]);
				p->m.pos[2] = (int16_t)((mul32(parts::ComputeCos(angle), dist) >> 12) + m->spawn[2]);
				const int32_t r1 = CrtRand();
				p->m.pos[1] = (int16_t)(m->spawn[1] - (int16_t)(r1 >> ep.rise_shift));
				const int32_t r2 = CrtRand();
				angle += ep.angle_step;
				p->m.accel = (int8_t)(uint8_t)(ep.accel.base - (r2 & ep.accel.mask));
			}
		}
		// a spark every few ticks around the effect position
		const BerserkParams::Sparks &kp = pp.sparks;
		e = m->counter - kp.from;
		if ((uint32_t)e <= (uint32_t)(kp.last - kp.from) && e % kp.every == 0)
		{
			ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			if (p)
			{
				Memset32(&p->m, 0, 4);
				p->m.seq = SEQ_Spark;
				p->m.scale = 0x1000;
				const int32_t r1 = CrtRand();
				p->m.pos[0] = (int16_t)((r1 & kp.spread.mask) + m->pos[0] - kp.spread.offset);
				const int32_t r2 = CrtRand();
				p->m.pos[1] = (int16_t)((r2 & kp.spread.mask) + m->pos[1] - kp.spread.offset);
				const int32_t r3 = CrtRand();
				p->m.pos[2] = (int16_t)((r3 & kp.spread.mask) + m->pos[2] - kp.spread.offset);
			}
		}
		const BerserkParams::Rays &rp = pp.rays;
		if ((uint32_t)(m->counter - rp.from) < (uint32_t)rp.ticks)
		{
			const RayMotion mo = { rp.speed, rp.length, rp.length_vel, rp.length_acc, rp.width, rp.width_vel, rp.width_acc };
			RaySpawn(0x20, m->rays, rp.count, RAY_Colours, mo);
		}
		if (m->counter == pp.timing.fade_tick) ScreenFadeTask(0, 1, 1, 0x80);
		if ((uint32_t)(m->counter - pp.burst.from) < (uint32_t)pp.burst.ticks)
		{
			TableArg arg;
			arg.m = Camera();
			memcpy(arg.offset, m->spawn, sizeof(arg.offset));
			arg.flags = PART_Flags;
			arg.morph = TexBase() + 0x52CC;
			arg.cursor = &PacketCursor();
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_play(m, &arg);)
			FX_PART(PART_BURST);
			PlayWithTable(m->burst, &arg);
			FX_PART(0);
		}
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_rays(m);)
		FX_PART(PART_RAYS);
		const int32_t alive = RayDraw(0x20, m->rays, m->pos, PacketCursor());
		FX_PART(0);
		if (m->counter == pp.timing.sound_tick)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(m->entity, pos);
			BdPlaySE3D((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Berserk), pp.timing.sound_volume, pos);
		}
		if (m->counter == pp.timing.damage_tick) ApplyActionResultToTarget(Ctx()->actions[m->action].targets);
		if (m->counter >= pp.timing.end_tick && alive == 0) return TASK_END;
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CFD70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x554C;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1D54C;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMains(), (void *)TexBase(), 0x8C4, 3);
				InitTaskQueuePool(QSwirls(), (void *)(TexBase() + 0x1A4C), 0x420, 0xC);
				InitTaskQueuePool(QParticles(), (void *)(TexBase() + 0x4BCC), 0x1C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a main task at busy_tick or before runs on this target
				MainNode *pool = (MainNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= P().timing.busy_tick)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					MainNode *m = (MainNode *)AddTaskToQueue(QMains(), ORIG_MainTask);
					Memset32(&m->counter, 0, 0x22E);
					m->action = (int16_t)(uint16_t)r->action;
					m->entity = entity;
					GetEffectSpawnPosition(entity, 0xF1, 0, m->spawn);
					m->spawn[1] = *(const int16_t *)(entity + 0x24); // the entity's height
					GetDefaultEffectPosition(m->entity, m->pos);
					DecodeModelPrimLayout(MODEL_Burst, m->burst, 0x29C);
					r->action++;
				}
			}
		}
		int a, b, c;
		if (r->started)
		{
			a = ExecuteTaskQueue(QMains());
			b = ExecuteTaskQueue(QSwirls());
			c = ExecuteTaskQueue(QParticles());
		}
		else
		{
			a = (int)n;
			b = (int)n;
			c = (int)n;
		}
		if (r->started && a == 0 && b == 0 && c == 0) return TASK_END;
		r->counter++;
		if (r->counter >= P().timing.every) r->counter = 0;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of BerserkParams
	// ------------------------------------------------------------------
	namespace berserk108
	{
		using fxp::Field;
#define BERSERK_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(BerserkParams, member), fxp::type, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The root starts one main task per action, in turn (not while a young one runs on that target); each main task's clock: its sound, a screen fade, its result; it ends once its rays are gone (draws nothing)" },
			{ PART_SWIRLS, "swirls", "Swirls", "Textured ribbons spiralling in towards the target's effect position" },
			{ PART_FLASH, "flash", "Flash", "A sprite sequence at the target's effect position" },
			{ PART_GLOW, "glow", "Glow", "A model at the target's effect position, pulsing and fading" },
			{ PART_BURST, "burst", "Burst", "The burst layout at the target's feet (its objects' draw modes from a table)" },
			{ PART_EMBERS, "embers", "Embers", "Sprites rising from a circle around the target's feet" },
			{ PART_SPARKS, "sparks", "Sparks", "Sprites at random points around the target's effect position" },
			{ PART_RAYS, "rays", "Rays", "Light rays turning around the target's effect position, fading" },
		};
		static const fxp::Group GROUPS[] = {
			{ "embers.dist", "Distance", "Base + (rand & Mask) from the target's feet point" },
			{ "embers.accel", "Acceleration", "(int8)(Base - (rand & Mask)) a tick (negative: up)" },
			{ "sparks.spread", "Spread", "(rand & Mask) - Offset on each axis" },
		};
		static const Field FIELDS[] = {
			BERSERK_FIELD("timing", timing.every, "Every", "The root starts the next action's main task every this many ticks", I16, 1, 100),
			BERSERK_FIELD("timing", timing.busy_tick, "Busy tick", "Not while a main task on that target is at this tick or before (the cycle restarts; -1: never)", I16, -1, 200),
			BERSERK_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			BERSERK_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays at the target's effect position when the main tick == this", I16, 0, 200),
			BERSERK_FIELD("timing", timing.sound_volume, "Sound volume", "BdPlaySE3D's volume argument", I16, 0, 0x7FFF),
			BERSERK_FIELD("timing", timing.damage_tick, "Damage tick", "The action's target takes the result when the main tick == this", I16, 0, 200),
			BERSERK_FIELD("timing", timing.end_tick, "End tick", "The main task ends from this tick, once no ray is left", I16, 0, 200),
			BERSERK_FIELD("timing", timing.fade_tick, "Fade tick", "The engine's screen fade starts when the main tick == this", I16, -1, 200),
			BERSERK_FIELD("swirls", swirls.last_tick, "Last tick", "A swirl is spawned at main ticks 0 .. this", I16, -1, 200),
			BERSERK_FIELD("swirls", swirls.angle, "Angle", "Its first angle: this + the main tick x Angle step", I16, -0x1000, 0x1000),
			BERSERK_FIELD("swirls", swirls.angle_step, "Angle step", "", I16, -0x1000, 0x1000),
			BERSERK_FIELD("swirls", swirls.amp, "Wave", "Its radius's wave amplitude (4096 = its own)", I16, -0x4000, 0x4000),
			BERSERK_FIELD("swirls", swirls.push_ticks, "Push ticks", "Positions are pushed over its ticks 0 .. this...", I16, -1, 200),
			BERSERK_FIELD("swirls", swirls.per_tick, "Per tick", "...this many a tick (32 kept)...", I16, 0, 32),
			BERSERK_FIELD("swirls", swirls.step, "Step", "...this far apart (angle)", I16, -0x1000, 0x1000),
			BERSERK_FIELD("swirls", swirls.fade, "Fade", "Its positions' brightness change a tick (x 8)", I16, -0x400, 0x400),
			BERSERK_FIELD("swirls", swirls.end_after, "End after", "It ends after this tick once fewer than two positions are left", I16, 0, 200),
			BERSERK_FIELD("flash", flash.from, "From tick", "The flash sequence from this main tick...", I16, 0, 200),
			BERSERK_FIELD("flash", flash.ticks, "Ticks", "...for this many (frame = tick - from)", I16, 0, 200),
			BERSERK_FIELD("glow", glow.from, "From tick", "The glow from this main tick...", I16, 0, 200),
			BERSERK_FIELD("glow", glow.ticks, "Ticks", "...for this many", I16, 1, 200),
			BERSERK_FIELD("glow", glow.scale, "Size", "Its size: sin(e x 1024 / ticks) + this (4096 = its own)", I16, -0x4000, 0x4000),
			BERSERK_FIELD("glow", glow.fade, "Fade", "Its fade: e x 2048 / ticks + this", I16, -0x4000, 0x4000),
			BERSERK_FIELD("burst", burst.from, "From tick", "The burst layout from this main tick...", I16, 0, 200),
			BERSERK_FIELD("burst", burst.ticks, "Ticks", "...for this many", I16, 0, 200),
			BERSERK_FIELD("embers", embers.from, "From tick", "Embers spawned from this main tick...", I16, 0, 200),
			BERSERK_FIELD("embers", embers.last, "Last tick", "...to this one...", I16, -1, 200),
			BERSERK_FIELD("embers", embers.count, "Count", "...this many a tick", I16, 0, 64),
			BERSERK_FIELD("embers", embers.angle_step, "Angle step", "Their angles apart around the feet point", I16, -0x1000, 0x1000),
			BERSERK_FIELD("embers", embers.dist.base, "Base", "", I16, -0x4000, 0x4000),
			BERSERK_FIELD("embers", embers.dist.mask, "Mask", "", I16, 0, 0x7FFF),
			BERSERK_FIELD("embers", embers.rise_shift, "Lift shift", "Lifted by rand >> this", I16, 0, 15),
			BERSERK_FIELD("embers", embers.accel.base, "Base", "", I16, -128, 255),
			BERSERK_FIELD("embers", embers.accel.mask, "Mask", "", I16, 0, 0x7F),
			BERSERK_FIELD("embers", embers.frames, "Frames", "An ember ends at this frame", I16, 1, 127),
			BERSERK_FIELD("sparks", sparks.from, "From tick", "Sparks spawned from this main tick...", I16, 0, 200),
			BERSERK_FIELD("sparks", sparks.last, "Last tick", "...to this one...", I16, -1, 200),
			BERSERK_FIELD("sparks", sparks.every, "Every", "...one every this many", I16, 1, 200),
			BERSERK_FIELD("sparks", sparks.spread.mask, "Mask", "", I16, 0, 0x7FFF),
			BERSERK_FIELD("sparks", sparks.spread.offset, "Offset", "", I16, -0x4000, 0x4000),
			BERSERK_FIELD("sparks", sparks.frames, "Frames", "A spark ends at this frame", I16, 1, 127),
			BERSERK_FIELD("rays", rays.from, "From tick", "Rays spawned from this main tick...", I16, 0, 200),
			BERSERK_FIELD("rays", rays.ticks, "Ticks", "...for this many...", I16, 0, 200),
			BERSERK_FIELD("rays", rays.count, "Count", "...this many a tick (the table holds 32)", I16, 0, 32),
			BERSERK_FIELD("rays", rays.speed, "Turn", "Their turn a tick (its side random)", I16, -0x1000, 0x1000),
			BERSERK_FIELD("rays", rays.length, "Length", "Their first length (x 8)", I16, 0, 0x7FFF),
			BERSERK_FIELD("rays", rays.length_vel, "Length speed", "", I16, -0x4000, 0x4000),
			BERSERK_FIELD("rays", rays.length_acc, "Length brake", "", I16, -0x1000, 0x1000),
			BERSERK_FIELD("rays", rays.width, "Width", "Their first width (x 8)", I16, 0, 0x7FFF),
			BERSERK_FIELD("rays", rays.width_vel, "Width speed", "", I16, -0x4000, 0x4000),
			BERSERK_FIELD("rays", rays.width_acc, "Width brake", "", I16, -0x1000, 0x1000),
		};
#undef BERSERK_FIELD
	}

	void register_mag108_berserk()
	{
		fxp::register_params({ 108, "Berserk", &berserk108::g_params, &berserk108::DEFAULTS, sizeof(berserk108::BerserkParams),
		                       berserk108::PARTS, (int)(sizeof(berserk108::PARTS) / sizeof(berserk108::PARTS[0])),
		                       berserk108::FIELDS, (int)(sizeof(berserk108::FIELDS) / sizeof(berserk108::FIELDS[0])),
		                       berserk108::GROUPS, (int)(sizeof(berserk108::GROUPS) / sizeof(berserk108::GROUPS[0])) });
		register_port(berserk108::ORIG_RootTask, (void *)berserk108::RootTask, "B108 RootTask", 108);
		register_port(berserk108::ORIG_MainTask, (void *)berserk108::MainTask, "B108 MainTask", 108);
		register_port(berserk108::ORIG_SwirlTask, (void *)berserk108::SwirlTask, "B108 SwirlTask", 108);
		register_port(berserk108::ORIG_ParticleTask, (void *)berserk108::ParticleTask, "B108 ParticleTask", 108);
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(register_mag108_held();)
	}
	FX_LAB_MODULE(register_mag108_berserk);
}

#ifdef FF8_FX_HELD
#include "mag108_berserk_held.inc"
#endif
