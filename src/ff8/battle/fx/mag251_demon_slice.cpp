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

// Effect 251: Demon Slice (enemy attack 314 of kernel.bin; Seifer c0m086; MAG_251_*). Bloodfest (210, Seifer c0m128)
// is its twin.
//
// Structure (setup MAG_251 0x6F44D0 -> _Init 0x6F44E0; the table's file loader is a null sub (0x5755C0): the init
// queues the module's own TIM (0x143CC90, in exe data), keeps the context, the CASTER's entity (0x2556394) and action
// 0's first TARGET's entity (0x2556398), starts the master and camera animation 0x142D5AC):
//   MasterTask (0x6F4580) - alternates the packet arena (magic buffer + 0xB300 / + 0x1B300); the screen flash level
//     (0x255643C) is 0 unless the main task sets it. On counter 1 (once): the stream state at + 0xA24C (0x534110), the
//     main pool (1 x 0x5EC at + 0xA294) and the flame pool (0x96 x 0x24 at + 0), the main task (cleared): the flame
//     spawner on the target (0x6F4840), the caster's and the target's positions, the target's height put to 0, the
//     target's effect position (0x571400), two points on the line from it to the caster (1.5 x and 1 x the two sizes
//     away), the target's step (-512 x the unit direction a tick); the height words of the wave layout's first two
//     models normalised (0x6F4FA0, exe data). Then (running): the view matrix orthonormalised into 0x2556458
//     (0x67AAE0), the main queue, the flames, the sparkles, the trail and the layout-task queues; the screen flash; the
//     master ends when the main queue is empty (flash off).
//   MainTask (0x6F5000) - counters 0..0xDC (the counter steps last). Each tick the caster's spawn point (0x502170, bone
//     0; from 0x71 its x / z stay those of 0x70). Screen flash: rising over 0..0xB, 0x8FC, falling over 0xD0..0xDB.
//     0x37: the sparkle / trail / layout-task pools (in the flame pool's memory) and the TRAIL task. 0x49..0x5C: layout
//     0x142FB40 at the spawn point (fading out from 0x54). 0x53..0xB1: the wave layout 0x1434430 at the spawn point
//     (its wave phase / texture scroll by the counter; it swells from 0xA3; 0x8D..0xA7 lifts the caster and the target
//     by a cosine curve), a layout task (0x1431F88) every 6 ticks up to 0xA5. 0x53..0x71: the caster slides to the
//     near point (sine ease). 0x72..0x8A: layout 0x1433318 at the spawn point of 0x72. 0x72..0xA7: every 5 ticks a
//     layout task (0x1431418) at the target and its hit chain (0x505C00, 5); up to 0x8C the caster swings between the
//     far and the near point. 0xA3..0xB0: layout 0x1432448 at the target (its effect point, y - 0x1000), the target
//     pushed back a step a tick, hidden at 0xB0. 0xA8..0xB5: the caster back to his start, at 0xB5 16 sparkles. 0x72 /
//     0xA3: white full-screen flashes (0x6F6020). 0xC0..0xDA: the target at his start, dropped by a cosine curve
//     (0xC0..0xC7), drawn by the module (Burning Rave's 0x6A2F60, turned 0x800 about x around his middle) and hidden.
//     0xC8..0xDB: layout 0x142D8A4 at the target's start, his hit chain at 0xC8. 0xDB: action 0's first target's result
//     (0x506690), the target shown. 0x21: the flame queue dropped; 0x23: stream load 0x2C9 into + 0xB300; 0x36: waits
//     while the stream is busy; the stream pump every tick. Sounds at 0 / 0x44 / 0xC7. 0xDC: the caster put back, end.
//   FlameSpawner (0x6F4890) - on the target: tick 1 four flames around his effect position; ends at 0x16.
//   Flame (0x6F4A30), Spark (0x6F4DC0), Ember (0x6F4EC0) - Fire's flame / spark / ember (the module's copy of the parts) on this module's
//     sequences, cursor and pause word.
//   TrailTask (0x6F5B20) - the blade trail of slot 4's bone 1 (0x6F5FC0, 0x1D97530 hard-wired): a 7-entry ring, splined
//     (32 points) and drawn as 30 gouraud quad pairs (Renzokuken's trail); 0x13 ticks, shorter from 0xD.
//   LayoutTask (0x6F6530) - its layout at its point through the view (depth -0x80); ends with the layout.
//   SparkleTask (0x6F65F0) - sequence 0x142D744 at its point, flying by its byte velocity; 13 frames.
//   PrimCallback (0x6F60D0) draws every layout object (0x701DD0): the orthonormal view 0x2556458 for flag 0x1000, no
//     matrix product for flag 0x8000, the per-object kind table (0x31 texture window scroll, 0x41 / 0x42 the wave
//     0x6F6470 into + 0xA880), a fade override.
// The tasks test the module's pause word 0x2556388 (never written: 0), not the battle's draw-only flags.
// DRAW ORDER: the main task draws its layouts first in each phase block, then moves; the other tasks draw then update.
// ENTITY WRITES: the caster +0x1C..+0x22 (slides 0x53..0xB5, height 0x8D..0xA7, put back at 0xDC); the target +0x1E (0 at
// the start, 0x8D..0xA7, 0xC0..0xC7), +0x1C / +0x20 (pushed 0xA3..0xB0, put back at 0xC0 / 0xC7), flags bit 4 (0xB0,
// 0xC0..0xDA set, 0xDB cleared), hit chains (0x72 + 5k, 0xC8). The camera is the engine's camera animation (no module
// write).
// Module globals: 0x2556388..0x2556478 (pause 0x2556388, packet cursor 0x255638C, context 0x2556390, caster 0x2556394,
// target 0x2556398, queues 0x25563A0 layout tasks / 0x25563B0 trail / 0x25563D0 sparkles / 0x25563E0 main / 0x25563F0
// master, master pool 0x2556400, flash level 0x255643C, flame queue 0x2556440, magic buffer 0x2556450, orthonormal
// view 0x2556458).
// NEVER-WRITTEN STACK WORDS: the main task's spawn point's 4th word (entry esp - 0x42; 0x502170 stores 3 words) goes
// into the node's +0x4E (counters < 0x71) and +0x3E (0x72), read by nothing; the callback's offset 4th word (GTE V0 pad),
// the play blocks' unset words (+0x26..+0x33 of layouts that have no kind 0x31 / 0x41 / 0x42 object) are never read.
// FUJIN: Fujin's described Demon Slice (effect_251.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn(), the setup (0x6F44D0 -> _Init 0x6F44E0) ported below (the file loader is the engine's null
// sub 0x5755C0); its parameters (DemonSliceParams: the sounds and the ticks, the screen flash, the flames, the layouts'
// windows, the wave, the caster's and the target's moves, the hits, the white flashes, the trail, the sparkles, the 7
// layouts, the 4 flipbooks, the TIM and the camera track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace demon251
{
	using namespace eng;
	using namespace magc;

	// ---- Fire's parts (0x629C10 flame, 0x62A0A0 ember, 0x629F90 spark): the module's own copy of the parts library
	// (fx_flames.h on the struct / True30FPS branches), as vanilla's mag002_fire.cpp has its own: a flame flipbook facing
	// the camera with a ground glow, embers rising faster and shrinking, sparks drifting, slowing and shrinking ----
#pragma pack(push, 1)
	// a flame (node +0x10, after its frame and delay words)
	struct Flame
	{
		int16_t pos[3];    // +0x00 x, y, z
		int16_t ground;    // +0x06 the target's height (the glow's plane)
		uint8_t pad08[4];
		int16_t scale;     // +0x0C
		uint8_t pad0E[6];
	};
	// an ember (node +0x10, after its frame and delay words)
	struct Ember
	{
		int16_t pos[3];    // +0x00
		int16_t vel_y;     // +0x06
		uint8_t pad08[4];
		int16_t scale;     // +0x0C
		uint8_t pad0E[6];
	};
	// a spark (node +0x10, after its life and delay words)
	struct FireSpark
	{
		int16_t pos[3];    // +0x00
		int16_t pad06;
		int16_t vel[3];    // +0x08
		int16_t pad0E;
		int16_t scale;     // +0x10
		int16_t pad12;
	};
#pragma pack(pop)
	static_assert(sizeof(Flame) == 0x14 && sizeof(Ember) == 0x14 && sizeof(FireSpark) == 0x14, "fire pieces");

	// a rolled value: base + sign x (rand % mod)
	struct Roll
	{
		int32_t base, mod, sign;
		int32_t of(int32_t random) const { return base + sign * (random % mod); }
	};

	// how the flames start around a target
	struct FlameRolls
	{
		int32_t delay_mod, delay_step;   // delay = rand % mod + step x its number
		int32_t spread_num, spread_max;  // spread = the target's size x num / 4096, at most max
		int32_t low;                     // a flame lower than the target's height - low...
		Roll lift;                       // ...is lifted by this
		Roll scale;
	};
	// a target's flame spread
	inline int32_t FlameSpread(int16_t size, const FlameRolls &r)
	{
		const int32_t s = (size * r.spread_num) >> 12;
		return s > r.spread_max ? r.spread_max : s;
	}
	// flame number k around `pos` (the target's default effect position, 4 words: its 4th the
	// ground word, then the target's height)
	inline void FlameStart(Flame *f, int16_t *delay, const int16_t *pos, int16_t height, int32_t spread, int32_t k, const FlameRolls &r)
	{
		const int32_t half = spread >> 1;
		*delay = (int16_t)(CrtRand() % r.delay_mod + r.delay_step * k);
		memcpy(f->pos, pos, 8);
		f->pos[0] += (int16_t)(CrtRand() % spread - half);
		f->pos[1] += (int16_t)(CrtRand() % spread - half);
		f->pos[2] += (int16_t)(CrtRand() % spread - half);
		f->ground = height;
		if (f->pos[1] > height - r.low) f->pos[1] += (int16_t)r.lift.of(CrtRand());
		f->scale = (int16_t)r.scale.of(CrtRand());
	}

	// how a flame looks: its flipbook, then its ground glow (a second sequence, flat on the
	// ground: turned by `tilt` about x, scaled by the flame's size; its grey = the flame's height
	// over `high` (y clamped there) x span / 4096 + base)
	struct FlameLook
	{
		uint32_t seq, glow_seq;
		int32_t shade_shift;             // the camera's offset: -(scale >> this)
		int32_t tilt, glow_mode, high, span, base;
	};
	inline void FlameDraw(const Flame *p, int16_t frame, const FlameLook &l, uint32_t &cursor)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> l.shade_shift));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = l.seq;
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		int16_t angles[4] = { (int16_t)l.tilt, 0, 0, 0 };
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[0] = p->pos[0];
		m.t[1] = p->ground;
		m.t[2] = p->pos[2];
		int32_t sv[4] = { p->scale, p->scale, 0x1000, 0 };
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		int32_t y = p->pos[1];
		if (y < l.high) y = l.high;
		const int32_t q = shl32(y, 12) / -l.high;          // (the original: / 2000 by a multiply)
		const uint8_t level = (uint8_t)((q * l.span) / 4096 + l.base);
		*(uint32_t *)h = l.glow_seq;
		*(uint16_t *)(h + 0x24) = (uint16_t)l.glow_mode;
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		h[0x1E] = level;
		h[0x1D] = level;
		h[0x1C] = level;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}

	// how the embers start from a flame: number k
	struct EmberRolls
	{
		int32_t delay_mod, delay_step;
		Roll x, y, z, rise, scale;
	};
	inline void EmberStart(Ember *e, int16_t *delay, const int16_t *flame_pos, int32_t k, const EmberRolls &r)
	{
		*delay = (int16_t)(CrtRand() % r.delay_mod + r.delay_step * k);
		memcpy(e->pos, flame_pos, 8);
		e->pos[0] += (int16_t)r.x.of(CrtRand());
		e->pos[1] += (int16_t)r.y.of(CrtRand());
		e->pos[2] += (int16_t)r.z.of(CrtRand());
		e->vel_y = (int16_t)r.rise.of(CrtRand());
		e->scale = (int16_t)r.scale.of(CrtRand());
	}
	// a sequence sprite at `pos`, the camera offset -(scale >> shift)
	inline void EmberDraw(const Ember *p, int16_t frame, uint32_t seq, int32_t shade_shift, uint32_t &cursor)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> shade_shift));
		uint8_t *h = AllocHeader(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}
	// rise: y += its speed, the speed grows by itself >> accel; it shrinks by itself >> shrink
	inline void EmberRise(Ember *p, int32_t accel, int32_t shrink)
	{
		const int16_t v = p->vel_y;
		p->pos[1] = (int16_t)(p->pos[1] + v);
		p->vel_y = (int16_t)(v + (int16_t)(v >> accel));
		const int16_t s = p->scale;
		p->scale = (int16_t)(s - (int16_t)(s >> shrink));
	}

	// how the sparks start from a flame: number k
	struct SparkRolls
	{
		Roll life;
		int32_t delay_mod, delay_step;
		Roll x, y, z, vx, vy, vz, scale;
	};
	inline void FireSparkStart(FireSpark *s, int16_t *life, int16_t *delay, const int16_t *flame_pos, int32_t k, const SparkRolls &r)
	{
		*life = (int16_t)r.life.of(CrtRand());
		*delay = (int16_t)(CrtRand() % r.delay_mod + r.delay_step * k);
		memcpy(s->pos, flame_pos, 8);
		s->pos[0] += (int16_t)r.x.of(CrtRand());
		s->pos[1] += (int16_t)r.y.of(CrtRand());
		s->pos[2] += (int16_t)r.z.of(CrtRand());
		s->vel[0] = (int16_t)r.vx.of(CrtRand());
		s->vel[1] = (int16_t)r.vy.of(CrtRand());
		s->vel[2] = (int16_t)r.vz.of(CrtRand());
		s->scale = (int16_t)r.scale.of(CrtRand());
	}
	// the shared effect sprite `sprite`, frame = last - life; header byte +0x20 and word +0x24
	struct SparkLook
	{
		int32_t sprite, last, flag20, mode, shade_shift;
	};
	inline void FireSparkDraw(const FireSpark *p, int16_t life, const SparkLook &l, uint32_t &cursor)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> l.shade_shift));
		uint8_t *h = AllocHeader(0xB4);
		*(void **)h = EffectSpriteSharedSequence(l.sprite);
		*(uint16_t *)(h + 4) = (uint16_t)(l.last - life);
		h[0x20] = (uint8_t)l.flag20;
		*(uint16_t *)(h + 0x24) = (uint16_t)l.mode;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}
	// drift: x and z slow by themselves >> drag, y (upwards) speeds up by itself >> drag; it
	// shrinks by `shrink`; its life goes down
	inline void FireSparkMove(FireSpark *p, int16_t *life, int32_t drag, int32_t shrink)
	{
		int16_t v = p->vel[0];
		p->scale = (int16_t)(p->scale - shrink);
		p->pos[0] = (int16_t)(p->pos[0] + v);
		p->vel[0] = (int16_t)(v - (int16_t)(v >> drag));
		v = p->vel[1];
		p->pos[1] = (int16_t)(p->pos[1] + v);
		p->vel[1] = (int16_t)(v + (int16_t)(v >> drag));
		v = p->vel[2];
		p->pos[2] = (int16_t)(p->pos[2] + v);
		p->vel[2] = (int16_t)(v - (int16_t)(v >> drag));
		(*life)--;
	}


	// --- module globals ---
	inline uint32_t &Pause() { return var<uint32_t>(own::R(0x2556388)); }           // never written by the game (0)
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x255638C)); }
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2556390)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2556394)); }
	inline uint8_t *&Target() { return var<uint8_t *>(own::R(0x2556398)); }
	inline TaskQueue *QLayouts() { return (TaskQueue *)own::R(0x25563A0); }          // + 0, 0x10 x 0x158 (from 0x37)
	inline TaskQueue *QTrail() { return (TaskQueue *)own::R(0x25563B0); }            // + 0x1700, 1 x 0x34C
	inline TaskQueue *QSparkles() { return (TaskQueue *)own::R(0x25563D0); }         // + 0x1580, 0x10 x 0x18
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x25563F0); }         // 1 x 0x10 (0x2556400)
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x25563E0); }             // + 0xA294, 1 x 0x5EC
	inline TaskQueue *QFlames() { return (TaskQueue *)own::R(0x2556440); }           // + 0, 0x96 x 0x24 (up to 0x21)
	inline int32_t &FlashLevel() { return var<int32_t>(own::R(0x255643C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2556450)); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 *OrthoView() { return (Mat4x3 *)own::R(0x2556458); }
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_MasterTask = 0x6F4580;
	static const uint32_t ORIG_FlameSpawner = 0x6F4890;
	static const uint32_t ORIG_FlameTask = 0x6F4A30;
	static const uint32_t ORIG_SparkTask = 0x6F4DC0;
	static const uint32_t ORIG_EmberTask = 0x6F4EC0;
	static const uint32_t ORIG_MainTask = 0x6F5000;
	static const uint32_t ORIG_TrailTask = 0x6F5B20;
	static const uint32_t ORIG_LayoutTask = 0x6F6530;
	static const uint32_t ORIG_SparkleTask = 0x6F65F0;
	static const uint32_t ORIG_PrimCallback = 0x6F60D0;
	static const uint32_t ORIG_DrawEntity212 = 0x6A2F60;  // Burning Rave's module-drawn entity (MAG_212_sub_6A2F60)

	static const uint32_t ORIG_Setup = 0x6F44D0;
	static const uint32_t TIM = 0x143CC90, CAMERA_ANIM = 0x142D5AC;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_FLAMES = 1, PART_OPENING = 2, PART_WAVE = 3, PART_RISERS = 4, PART_SLASH = 5, PART_HITS = 6, PART_PUSH = 7,
	             PART_FALL = 8, PART_TARGET = 9, PART_WHITES = 10, PART_TRAIL = 11, PART_SPARKLES = 12 };
	struct DemonSliceParams
	{
		struct { uint32_t start, swing, fall; } sounds;      // the three sounds
		// the main task's ticks: the three sounds, its last (the caster put back)
		struct { int16_t start, swing, fall, end; } timing;
		struct { int16_t level, rise, fall; } flash;        // the screen flash: its level, rising over, falling over the last
		// the flames on the target (Fire's parts): how many, their frames, embers and sparks from each, their size (+ a
		// random below size_rand), the main task's tick they are all dropped
		struct { int16_t count, frames, embers, sparks, size, size_rand, drop; } flames;
		// layout A at the caster's spawn point: from start for ticks, fading out from its tick fade_from over fade_ticks
		struct { int16_t start, ticks, fade_from, fade_ticks; } opening;
		// the wave layout at the spawn point: from start for ticks; its wave (phase step a tick, amplitude), texture scroll a
		// tick; from its tick swell_from it swells (swell x sin over swell_ticks a quarter turn)
		struct { int16_t start, ticks, phase, amp, scroll, swell_from, swell_ticks, swell; } wave;
		// the rising layout tasks: every so many of the wave's ticks below until, lift above the spawn point
		struct { int16_t every, until, lift; } risers;
		// from the main task's tick start the wave grows taller over ticks; until end it lifts the caster and the target
		struct { int16_t start, ticks, end; } lift;
		// the caster: his slide to the far point (from, ticks), his swings (until), his way back (from, ticks)
		struct { int16_t slide_from, slide_ticks, swing_until, back_from, back_ticks; } caster;
		struct { int16_t start, ticks, scroll; } slash;     // the slash layout at the spawn point of its first tick
		struct { int16_t start, ticks, every, chain; } hits; // the hit layout tasks at the target and his hit chain
		// the push layout at the target's effect point (its height), the target pushed a step a tick (hidden at its end)
		struct { int16_t start, ticks, height, step; } push;
		// the target at his start: dropped over drop ticks, drawn turned by the module until ticks, then his result
		struct { int16_t start, drop, ticks; } target;
		struct { int16_t start, ticks; } fall;               // the fall layout at the target's start (his hit chain at its first tick)
		// the two full-screen flashes: their ticks, in / hold / out ticks, colour
		struct { int16_t first, second, in, hold, out, r, g, b; } whites;
		// the blade trail (slot 4's bone 1): from start for ticks (shorter from shorten), its length, its colour (0xBBGGRR)
		struct { int16_t start, ticks, shorten, length; int32_t colour; } trail;
		struct { int16_t count, step, frames; } sparkles;    // the sparkles round the caster at his way back's end
		struct { uint32_t opening, wave, rise, slash, push, fall, hit; } layouts;   // the prim-model layouts played
		struct { uint32_t flame, glow, ember, sparkle; } seqs;   // the sprite sequences drawn (flipbooks)
		struct { uint32_t tim, camera; } res;                   // its resources (data): the TIM its setup uploads, the camera track
	};
	static const DemonSliceParams GAME = { { 0x142D738, 0x142D740, 0x142D73C }, { 0, 0x44, 0xC7, 0xDC }, { 2300, 12, 12 },
		{ 4, 0x10, 4, 10, 0xD00, 0x600, 0x21 }, { 0x49, 0x14, 10, 10 }, { 0x53, 0x5F, 0x40, 0x100, 0x20, 0x50, 15, 2 },
		{ 6, 0x53, 0x100 }, { 0x8D, 27, 0xA8 }, { 0x53, 0x1F, 0x8D, 0xA8, 0xE }, { 0x72, 0x19, 0x20 }, { 0x72, 0x36, 5, 5 },
		{ 0xA3, 0xE, -0x1000, 0x200 }, { 0xC0, 8, 0x1B }, { 0xC8, 0x14 }, { 0x72, 0xA3, 0, 1, 3, 0xFF, 0xFF, 0xFF },
		{ 0x37, 0x13, 0xD, 777, 0x185820 }, { 0x10, 0x100, 0xD },
		{ 0x142FB40, 0x1434430, 0x1431F88, 0x1433318, 0x1432448, 0x142D8A4, 0x1431418 },
		{ 0x143878C, 0x1438938, 0x1438AE4, 0x142D744 }, { TIM, CAMERA_ANIM } };
	static DemonSliceParams live = GAME;
	static const DemonSliceParams &PG() { return live; }

	static const uint32_t STREAM_STATE = 0xA24C;       // magic buffer offsets
	static const uint32_t POOL_Main = 0xA294;
	static const uint32_t POOL_Sparkles = 0x1580;
	static const uint32_t POOL_Trail = 0x1700;
	static const uint32_t MORPH = 0xA880;              // blended vertex frames / the wave's vertices
	static const uint32_t ARENA_A = 0xB300;            // packet arenas (the stream load lands in A)
	static const uint32_t ARENA_B = 0x1B300;
	static const uint32_t LAYOUT_Fade = 0x142FB40;     // prim-model layouts
	static const uint32_t LAYOUT_Wave = 0x1434430;
	static const uint32_t LAYOUT_Rise = 0x1431F88;
	static const uint32_t LAYOUT_Swing = 0x1433318;
	static const uint32_t LAYOUT_Push = 0x1432448;
	static const uint32_t LAYOUT_Fall = 0x142D8A4;
	static const uint32_t LAYOUT_Hit = 0x1431418;
	static const uint32_t KIND_Plain = 0x1438778;      // per-object kind tables (0x30 plain, 0x31 / 0x41 / 0x42)
	static const uint32_t KIND_Wave = 0x145D2B4;
	static const uint32_t KIND_Swing = 0x145D2B0;
	static const uint32_t SEQ_Flame = 0x143878C;       // sprite sequences
	static const uint32_t SEQ_FlameGlow = 0x1438938;
	static const uint32_t SEQ_Ember = 0x1438AE4;
	static const uint32_t SEQ_Sparkle = 0x142D744;
	static const int16_t *const WAVE_TABLE = (const int16_t *)0x1438C90;  // 0x1000 x (2 words)
	static const void *const SOUND_Start = (const void *)0x142D738;
	static const void *const SOUND_Swing = (const void *)0x142D740;
	static const void *const SOUND_Fall = (const void *)0x142D73C;
	static uint8_t *const TRAIL_ENTITY = (uint8_t *)0x1D97530;         // slot 4, hard-wired

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t ISqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }   // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a x b
	inline void MatrixMultiply3x3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline void DrawEntity212(uint8_t *e, const int16_t *angles, const int16_t *off) { fn<void (__cdecl *)(uint8_t *, const int16_t *, const int16_t *)>(ORIG_DrawEntity212)(e, angles, off); }
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }       // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60 (GTE_SetFarColor)
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(int32_t *p) { fn<void (__cdecl *)(int32_t *)>(0x45E3D0)(p); }   // GTE_ReadOTZ (a dword)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // GTE_LoadV0_u16
	inline void GteReadIR2(void *dst) { fn<void (__cdecl *)(void *)>(0x703F90)(dst); }        // GTE_ReadIR2Word
	// battle file streaming (0x534110 / 0x5341D0 / 0x534210 / 0x534270)
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x10 bytes (0x2556400)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t running;   // +0x0E set up (counter 1)
		uint8_t parity;    // +0x0F packet arena parity
	};
	struct SpawnerNode // flame pool (0x24)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t w0E;       // +0x0E (0)
		uint8_t pad10[0x10];
		int16_t slot;      // +0x20 the target's slot
		int16_t w22;       // +0x22 (0)
	};
	struct FlameNode { TaskNode hdr; int16_t frame, delay; Flame f; };      // 0x24
	struct SparkNode { TaskNode hdr; int16_t life, delay; FireSpark s; };   // 0x24
	struct EmberNode { TaskNode hdr; int16_t frame, delay; Ember e; };      // 0x24
	struct MainNode // pool of 1 node of 0x5EC bytes (magic buffer + 0xA294), cleared at the start
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C 0..0xDC
		int16_t pad0E;
		int16_t caster0[4];  // +0x10 the caster's +0x1C..+0x23 at the start (his slide's end, put back at the end)
		int16_t target0[4];  // +0x18 the target's +0x1C..+0x23 at the start
		int16_t target_fx[4];// +0x20 the target's effect position (start)
		int16_t near_pt[4];  // +0x28 1.5 x (caster + target size) from the target towards the caster
		int16_t far_pt[4];   // +0x30 (caster + target size) away; from 0xA3 the target's effect position then
		int16_t keep[4];     // +0x38 the spawn point of 0x72 (layout 0x1433318); from 0xA8 the caster's +0x1C..+0x23
		int16_t step[4];     // +0x40 the target's push a tick (x, -, z)
		uint32_t spawn[2];   // +0x48 the caster's spawn point (up to 0x70)
		uint8_t layout_a[0x1C8]; // +0x50 layouts 0x142FB40 / 0x1433318 / 0x1432448
		uint8_t layout_b[0x3D4]; // +0x218 layouts 0x1434430 / 0x142D8A4
	};
	struct RingEntry { uint32_t alive; int16_t base[4]; int16_t tip[4]; }; // 0x14
	struct TrailNode // pool of 1 node of 0x34C bytes (magic buffer + 0x1700)
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C 0..0x13
		int16_t pad0E;
		RingEntry ring[7];   // +0x10 slot counter % 7
		int16_t pts_a[8][4]; // +0x9C newest first: bases
		int16_t pts_b[8][4]; // +0xDC tips
		int16_t spl_a[32][4];// +0x11C splined bases
		int16_t spl_b[32][4];// +0x21C splined tips
		int16_t first_b[4];  // +0x31C the tip spline's first point
		int16_t first_a[4];  // +0x324 the base spline's first point
		int32_t dir_b[4];    // +0x32C unit (tip 0 - tip 1); +0x338 never written
		int32_t dir_ab[4];   // +0x33C unit (tip 0 - base 0); +0x348 never written
	};
	// the trail's Field_Alloc work block (0x40)
	struct TrailWork
	{
		uint8_t pad00[8];
		int32_t otz;         // +0x08
		uint32_t flag;       // +0x0C
		uint32_t col0;       // +0x10
		uint32_t col1;       // +0x14
		uint8_t pad18[4];
		uint32_t base_col;   // +0x1C
		int16_t base[4];     // +0x20 the bone's point (3 words written)
		int16_t tip[4];      // +0x28 777 along the blade (3 words written)
		int32_t dir[4];      // +0x30 the blade's unit direction (3 dwords)
	};
	struct LayoutNode // 0x158
	{
		TaskNode hdr;
		int16_t pos[4];      // +0x0C (its 4th word: the effect position's)
		uint8_t layout[0x144]; // +0x14
	};
	struct SparkleNode // 0x18
	{
		TaskNode hdr;
		int16_t pos[3];      // +0x0C
		int16_t frame;       // +0x12
		int8_t vel[3];       // +0x14
		uint8_t pad17;
	};
	// the prim-player callback's parameter block (part of the caller's stack frame)
	struct PlayArg
	{
		Mat4x3 m;            // +0x00 the layout's frame (the view composed)
		uint32_t kind;       // +0x20 per-object kind table
		int16_t depth;       // +0x24 header +0x18
		uint16_t scroll;     // +0x26 (its low byte: texture window offset; written as a word)
		int16_t fade;        // +0x28 != 0: header +0xC = fade, black, semi-transparent
		int16_t wave_phase;  // +0x2A the wave 0x6F6470's parameters
		int16_t wave_amp;    // +0x2C
		int16_t wave_xz;     // +0x2E
		int16_t wave_y;      // +0x30
		int16_t pad32;
		uint32_t morph;      // +0x34 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x10 && sizeof(SpawnerNode) == 0x24 && sizeof(FlameNode) == 0x24 && sizeof(SparkNode) == 0x24 && sizeof(EmberNode) == 0x24, "Demon Slice nodes");
	static_assert(sizeof(MainNode) == 0x5EC && sizeof(TrailNode) == 0x34C && sizeof(LayoutNode) == 0x158 && sizeof(SparkleNode) == 0x18, "Demon Slice nodes");
	static_assert(sizeof(RingEntry) == 0x14 && sizeof(TrailWork) == 0x40 && sizeof(PlayArg) == 0x38, "Demon Slice blocks");

	// Fire's parts (the copy above) with this module's numbers (the same as Fire's)
	static const FlameRolls FLAME_ROLLS = { 6, 2, 2800, 1000, 0x226, { -500, 300, -1 }, { 0xD00, 0x600, 1 } };
	static const FlameLook FLAME_LOOK = { SEQ_Flame, SEQ_FlameGlow, 4, 0x400, 0xC, -2000, 160, 0xA0 };
	static const EmberRolls EMBER_ROLLS = { 6, 1, { -150, 300, 1 }, { 0, 100, -1 }, { -150, 300, 1 }, { -20, 30, -1 }, { 0xA00, 0x500, 1 } };
	static const SparkRolls SPARK_ROLLS = { { 5, 4, 1 }, 4, 1, { -200, 400, 1 }, { 0, 200, -1 }, { -200, 400, 1 }, { -40, 80, 1 }, { -20, 50, -1 }, { -40, 80, 1 }, { 0x900, 0x600, 1 } };
	static const SparkLook SPARK_LOOK = { 5, 9, 4, 0x10, 3 };
}
}

#ifdef FF8_FX_HELD
#include "mag251_demon_slice_held.h"
#endif

namespace ff8fx
{
namespace demon251
{
	using namespace eng;
	using namespace magc;

	inline int32_t SlotOf(uint8_t *e) { return (int32_t)((e - (uint8_t *)0x1D972C0) / 0x9C); }

	// ------------------------------------------------------------------
	// 0x6F4FA0: a model's vertex heights normalised into their 4th words: (y - max) * 4096 / (max - min)
	// ------------------------------------------------------------------
	static void NormaliseHeights(uint8_t *model)
	{
		const int32_t n = *(const int32_t *)(model + 4);
		int16_t *v = (int16_t *)(model + 0xC);
		int32_t lo = v[1], hi = v[1];
		// (the scan stops before the last vertex)
		for (int32_t i = 0; i < n - 1; i++)
		{
			const int32_t y = v[i * 4 + 1];
			if (lo > y) lo = y;
			else if (hi < y) hi = y;
		}
		const int32_t span = hi - lo;
		for (int32_t i = 0; i < n; i++) v[i * 4 + 3] = (int16_t)(shl32(v[i * 4 + 1] - hi, 12) / span);
	}

	// ------------------------------------------------------------------
	// 0x6F4840: the flame spawner on an entity
	// ------------------------------------------------------------------
	static void StartFlameSpawner(uint8_t *e)
	{
		SpawnerNode *s = (SpawnerNode *)AddTaskToQueue(QFlames(), own::Fn(ORIG_FlameSpawner));
		if (!s) return;
		s->counter = 0;
		s->w0E = 0;
		s->w22 = 0;
		s->slot = (int16_t)SlotOf(e);
	}

	// The layout tasks' play block leaves its +0x28 word (the fade override) unwritten: it reads the stack word the game's
	// executor left at the layout task's entry esp - 0x10. The master runs its five queues through 0x508420 with the
	// argument pushes piling up, so that word is the main task's entry esp - 0x20 (its play block's composed y
	// translation, +0x40 of its frame, after any layout it played this tick), else what the master's view
	// orthonormalisation 0x67AAE0 saved there (its caller's esi: the master node), and the sparkles' entry esp - 0x18
	// (their sequence draw's OT pointer argument) when a sparkle ran this tick. The flame / trail queues never run
	// together with a layout task. Low 16 bits of the latest of these:
	static int16_t g_task_fade = 0;

	// ------------------------------------------------------------------
	// MasterTask (0x6F4580): node +0x0C counter, +0x0E set up, +0x0F arena parity
	// ------------------------------------------------------------------
	static void MasterSetup()
	{
		StreamStateInit((void *)(TexBase() + STREAM_STATE));
		InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x5EC, 1);
		InitTaskQueuePool(QFlames(), (void *)TexBase(), 0x24, 0x96);
		MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
		Memset32(&m->counter, 0, 0x178);
		StartFlameSpawner(Target());
		memcpy(m->caster0, Caster() + 0x1C, 8);
		memcpy(m->target0, Target() + 0x1C, 8);
		*(int16_t *)(Target() + 0x1E) = 0;
		GetDefaultEffectPosition(Target(), m->target_fx);
		const int32_t dx = *(const int16_t *)(Caster() + 0x1C) - m->target_fx[0];
		const int32_t dz = *(const int16_t *)(Caster() + 0x20) - m->target_fx[2];
		const int32_t d = ISqrt(dz * dz + dx * dx);
		int32_t s = *(const int16_t *)(Caster() + 0x26) + *(const int16_t *)(Target() + 0x26);
		s += s / 2;
		const int16_t x0 = m->target_fx[0];
		m->near_pt[0] = (int16_t)(s * dx / d + x0);
		m->near_pt[1] = 0;
		m->near_pt[2] = (int16_t)(s * dz / d + m->target_fx[2]);
		s = *(const int16_t *)(Caster() + 0x26) + *(const int16_t *)(Target() + 0x26);
		m->far_pt[1] = 0;
		m->far_pt[0] = (int16_t)(s * dx / d + x0);
		m->far_pt[2] = (int16_t)(s * dz / d + m->target_fx[2]);
		m->step[0] = (int16_t)(mul32(-dx, PG().push.step) / d);
		m->step[2] = (int16_t)(mul32(-dz, PG().push.step) / d);
		uint8_t *wave = (uint8_t *)fxp::layout(PG().layouts.wave, LAYOUT_Wave, 0x24C);
		NormaliseHeights(wave + *(const int32_t *)(wave + 8));
		NormaliseHeights(wave + *(const int32_t *)(wave + 0xC));
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *ms = (MasterNode *)n;
		if (ms->parity)
		{
			PacketCursor() = TexBase() + ARENA_A;
			ms->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			ms->parity = 1;
		}
		FlashLevel() = 0;
		if (ms->counter == 1 && Pause() == 0 && !ms->running)
		{
			ms->running = 1;
			MasterSetup();
		}
		int alive = 1;
		if (ms->running)
		{
			// 30 fps layer: see mag251_demon_slice_held.inc
			FX_HELD(held_note_master(n);)
			g_task_fade = (int16_t)(uint32_t)n;
			EffectCameraMatrix(&Camera(), OrthoView());
			alive = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QFlames());
			ExecuteTaskQueue(QSparkles());
			ExecuteTaskQueue(QTrail());
			ExecuteTaskQueue(QLayouts());
		}
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (Pause() != 0) return 0;
		if (ms->running && alive == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		ms->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// FlameSpawner (0x6F4890): tick 1, four flames around the target's effect position
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlameSpawner(TaskNode *n)
	{
		if (Pause() != 0) return 0;
		SpawnerNode *s = (SpawnerNode *)n;
		if (s->counter == 1)
		{
			FlameRolls rolls = FLAME_ROLLS;
			rolls.scale.base = PG().flames.size;
			rolls.scale.mod = PG().flames.size_rand;
			const int32_t spread = FlameSpread(EntitySize(s->slot), rolls);
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(s->slot), pos);
			for (int32_t k = 0; k < PG().flames.count; k++)
			{
				FlameNode *f = (FlameNode *)AddTaskToQueue(QFlames(), own::Fn(ORIG_FlameTask));
				if (!f) continue;
				f->frame = 0;
				FlameStart(&f->f, &f->delay, pos, EntityHeight(s->slot), spread, k, rolls);
			}
		}
		if (s->counter == 6)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(s->slot), pos);
		}
		s->counter++;
		return s->counter >= 0x16 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Flame (0x6F4A30), Spark (0x6F4DC0), Ember (0x6F4EC0): Fire's parts (the copy above)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		FlameNode *p = (FlameNode *)n;
		if (p->delay > 0)
		{
			if (Pause() != 0) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_draw(ORIG_FlameTask, p);)
		FX_PART(PART_FLAMES);
		FlameLook look = FLAME_LOOK;
		look.seq = fxp::sequence(PG().seqs.flame, SEQ_Flame);
		look.glow_seq = fxp::sequence(PG().seqs.glow, SEQ_FlameGlow);
		FlameDraw(&p->f, p->frame, look, PacketCursor());
		if (Pause() != 0) return 0;
		if (p->frame == 0)
		{
			for (int32_t k = 0; k < PG().flames.embers; k++)
			{
				EmberNode *m = (EmberNode *)AddTaskToQueue(QFlames(), own::Fn(ORIG_EmberTask));
				if (!m) continue;
				m->frame = 0;
				EmberStart(&m->e, &m->delay, p->f.pos, k, EMBER_ROLLS);
			}
			for (int32_t k = 0; k < PG().flames.sparks; k++)
			{
				SparkNode *m = (SparkNode *)AddTaskToQueue(QFlames(), own::Fn(ORIG_SparkTask));
				if (!m) continue;
				FireSparkStart(&m->s, &m->life, &m->delay, p->f.pos, k, SPARK_ROLLS);
			}
		}
		p->frame++;
		return p->frame >= PG().flames.frames ? TASK_END : 0;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		if (p->delay > 0)
		{
			if (Pause() != 0) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_draw(ORIG_SparkTask, p);)
		FX_PART(PART_FLAMES);
		FireSparkDraw(&p->s, p->life, SPARK_LOOK, PacketCursor());
		if (Pause() != 0) return 0;
		FireSparkMove(&p->s, &p->life, 4, 0x40);
		return p->life < 0 ? TASK_END : 0;
	}

	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		EmberNode *p = (EmberNode *)n;
		if (p->delay > 0)
		{
			if (Pause() != 0) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_draw(ORIG_EmberTask, p);)
		FX_PART(PART_FLAMES);
		EmberDraw(&p->e, p->frame, fxp::sequence(PG().seqs.ember, SEQ_Ember), 3, PacketCursor());
		if (Pause() != 0) return 0;
		EmberRise(&p->e, 5, 6);
		p->frame++;
		return p->frame >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6F60D0): one object of a layout (0x701DD0)
	// ------------------------------------------------------------------
	// 0x6F6470: the wave: frame 0's vertices into the morph buffer (magic buffer + 0xA880: the play block's), x / z pushed
	// along the table's direction at (height word + phase) by the table's amplitude at the height word x amp, scaled by
	// xz; y scaled by ys
	static void Wave(uint8_t *h, uint32_t morph, int32_t phase, int32_t amp, int32_t xz, int32_t ys)
	{
		const uint8_t *model = *(uint8_t **)h;
		int32_t n = *(const int32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 0xC);
		int16_t *out = (int16_t *)morph;
		do
		{
			const int32_t w = v[3];
			const int32_t a = mul32(WAVE_TABLE[w & 0xFFE], amp) >> 12;
			const int16_t *dir = WAVE_TABLE + ((w + phase) & 0xFFF) * 2;
			const int32_t dz = mul32(dir[1], a);
			const int32_t dx = (int16_t)(mul32(dir[0], a) >> 12);
			out[0] = (int16_t)(mul32(dx + v[0], xz) >> 12);
			out[2] = (int16_t)(mul32(v[2] + (int16_t)(dz >> 12), xz) >> 12);
			out[1] = (int16_t)(mul32(v[1], ys) >> 12);
			v += 4;
			out += 4;
		} while (--n != 0);
		*(uint32_t *)(h + 4) = morph;
	}

	// the record's offset is loaded into the GTE V0 with its 4th word never written (the stack's residue: the port loads 0)
	static int16_t g_off_pad = 0;
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PlayArg *arg = (const PlayArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m;
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		if (r->flags & 0x1000)
		{
			// the offset through the orthonormal view 0x2556458, the rotation composed with it (flag 0x8000: not)
			GteSetRotMatrixCtrl(OrthoView());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t t[3];
			GteReadMAC123(t);
			m.t[0] = t[0];
			m.t[1] = t[1];
			m.t[2] = t[2];
			if (!(r->flags & 0x8000)) MatrixMultiply(OrthoView(), &m);
		}
		else if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				// 3x3 scale matrix (diagonal x, y, z)
				Mat4x3 s;
				s.m[0][0] = r->scale[0];
				s.m[1][1] = r->scale[1];
				s.m[2][2] = r->scale[2];
				s.m[1][2] = 0;
				s.m[0][2] = 0;
				s.m[2][1] = 0;
				s.m[0][1] = 0;
				s.m[2][0] = 0;
				s.m[1][0] = 0;
				MatrixMultiply3x3(&m, &s);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x26) = 0;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint16_t *)(h + 0x2A) = 0x100;
		*(uint16_t *)(h + 0x28) = 0x100;
		const uint8_t kind = ((const uint8_t *)arg->kind)[object];
		if (kind == 0x31)
		{
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0;
			*(uint16_t *)(h + 0x32) = 0x100;
			*(uint16_t *)(h + 0x30) = 0x40;
			*(uint16_t *)(h + 0x20) = (uint16_t)(arg->scroll & 0x3F);
		}
		else if (kind == 0x41 || kind == 0x42)
		{
			if (kind == 0x41)
			{
				*(uint16_t *)(h + 0x2C) = 0;
				*(uint16_t *)(h + 0x2E) = 0;
				*(uint16_t *)(h + 0x32) = 0x80;
				*(uint16_t *)(h + 0x30) = 0x100;
				*(uint16_t *)(h + 0x22) = (uint16_t)(arg->scroll & 0x7F);
			}
			Wave(h, arg->morph, arg->wave_phase, arg->wave_amp, arg->wave_xz, arg->wave_y);
		}
		if (arg->fade != 0)
		{
			*(int32_t *)(h + 0xC) = arg->fade;
			*(uint32_t *)(h + 8) = 0;
			*(uint8_t *)(h + 0x1C) |= 0xC0;
		}
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// MainTask (0x6F5000)
	// ------------------------------------------------------------------
	// one layout through the prim player (0x701970) with the callback 0x6F60D0: its frame (a yaw-0 matrix at x, y, z)
	// under the view
	static void Play(MainNode *mn, int site, uint8_t *layout, PlayArg *arg, int32_t x, int32_t y, int32_t z)
	{
		RotationY(0, &arg->m);
		arg->m.t[0] = x;
		arg->m.t[1] = y;
		arg->m.t[2] = z;
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		g_task_fade = (int16_t)arg->m.t[1];
		arg->morph = TexBase() + MORPH;
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_layout(mn, site, layout, arg, x, y, z);)
		prim::play((prim::Layout *)layout, PrimCallback, (int)arg, (int)Pause());
	}

	// 0x6F6020: full-screen tile (r, g, b) at step t of a fade in over `in` ticks, held `hold`, out over `out`
	static int32_t FadeLevel(int32_t t, int32_t in, int32_t hold, int32_t out)
	{
		const int32_t din = in != 0 ? in : 1;
		const int32_t dout = out != 0 ? out : 1;
		if (t < in) return shl32(t + 1, 12) / din;
		if (t < in + hold) return 0x1000;
		return (shl32(in - t + hold, 12) - 0x1000) / dout + 0x1000;
	}
	static void FadeTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out)
	{
		const int32_t level = FadeLevel(t, in, hold, out);
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_tile(t);)
		PacketCursor() = Tile(r, g, b, level, PacketCursor());
	}

	// 0x6F6690: a model's vertical extent in its bone space (every shown object's vertices through their bones, IR2
	// min / max) -> out[0] = max, out[1] = min. The entity's matrix translation y is 0 for the bone build, then its
	// height (+0x1E); its pose matrices rebuilt after.
	static void MeasureHeight(uint8_t *e, int16_t *out)
	{
		*(int32_t *)(e + 0x58) = 0;
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		*(int32_t *)(e + 0x58) = *(const int16_t *)(e + 0x1E);
		uint8_t *h = (uint8_t *)FieldAlloc(0x18);
		*(uint32_t *)(h + 4) = 0xFFFF0000;
		*(uint32_t *)h = 0x10000;
		*(uint32_t *)(h + 0x14) = *(const uint32_t *)(e + 0x7C);
		const uint8_t *model = *(uint8_t *const *)(e + 0x64);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		*(uint32_t *)(h + 8) = 0;
		*(uint16_t *)(h + 0xC) = 0x1000;
		const uint8_t *objs = *(const uint8_t *const *)(model + 4);
		const int32_t count = *(const int32_t *)objs;
		const uint32_t *offs = (const uint32_t *)(objs + 4);
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + offs[i];
			if (!(*(const uint32_t *)(h + 0x14) & (1u << (i & 31)))) continue;
			int32_t ng = *(const int16_t *)p;
			p += 2;
			if (ng <= 0) continue;
			do
			{
				const int32_t bone = *(const int16_t *)p;
				p += 2;
				const Mat4x3 *bm = (const Mat4x3 *)(bones + (uint32_t)bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(bm);
				GteSetTransVectorCtrl(bm);
				int32_t nv = *(const int16_t *)p;
				p += 2;
				for (; nv != 0; nv--)
				{
					GteLoadV0u(p);
					GteRTPS();
					p += 6;
					GteReadIR2(h + 0x10);
					const int32_t v = *(const int16_t *)(h + 0x10);
					if (*(const int32_t *)h > v) *(int32_t *)h = v;
					if (*(const int32_t *)(h + 4) < v) *(int32_t *)(h + 4) = v;
				}
			} while (--ng != 0);
		}
		out[0] = *(const int16_t *)(h + 4);
		out[1] = *(const int16_t *)h;
		FieldFree(0x18);
		BuildBoneMatricesFromPose(e + 0x60);
	}

	// the flash level at counter c (rising 0..0xB, 0x8FC, falling 0xD0..0xDB)
	static int32_t FlashOf(int32_t c)
	{
		const DemonSliceParams &g = PG();
		if (c < g.flash.rise) return c * g.flash.level / g.flash.rise;
		if (c >= g.timing.end - g.flash.fall) return (g.timing.end - c) * g.flash.level / g.flash.fall;
		return g.flash.level;
	}

	// the target's drop (0xC0..0xC7): 0x1000 - cos((0x1000 - cos(((i << 12) >> 3) + 0x100) >> 2) >> 2), as a height
	static int16_t DropHeight(int32_t i)
	{
		const int32_t a = ComputeCos((int32_t)((uint32_t)shl32(i, 12) / (uint32_t)PG().target.drop + 0x100) >> 2);
		const int32_t b = ComputeCos((0x1000 - a) >> 2);
		return (int16_t)(shl32(0x1000 - b, 12) / 4096 - 0x1000);
	}

	// 0x6F5AF0: the trail task (its ring cleared)
	static void StartTrail()
	{
		TrailNode *t = (TrailNode *)AddTaskToQueue(QTrail(), own::Fn(ORIG_TrailTask));
		if (!t) return;
		t->counter = 0;
		for (int32_t k = 0; k < 7; k++) t->ring[k].alive = 0;
	}

	// the residue model of the spawn point's never-written 4th word (see the header comment)
	static int16_t g_spawn_pad = 0;

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *mn = (MainNode *)n;
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_main(mn);)
		int16_t sp[4];
		sp[3] = g_spawn_pad;
		if (mn->counter < 0x71)
		{
			GetEffectSpawnPosition(Caster(), 0, 0, sp);
			mn->spawn[0] = *(const uint32_t *)&sp[0];
			mn->spawn[1] = *(const uint32_t *)&sp[2];
		}
		else
		{
			GetEffectSpawnPosition(Caster(), 0, 0, sp);
			sp[0] = (int16_t)mn->spawn[0];
			sp[2] = (int16_t)mn->spawn[1];
		}
		const int32_t c0 = mn->counter;
		FlashLevel() = FlashOf(c0);
		if (c0 == 0x37)
		{
			InitTaskQueuePool(QTrail(), (void *)(TexBase() + POOL_Trail), 0x34C, 1);
			InitTaskQueuePool(QLayouts(), (void *)TexBase(), 0x158, 0x10);
			InitTaskQueuePool(QSparkles(), (void *)(TexBase() + POOL_Sparkles), 0x18, 0x10);
		}
		if (c0 == PG().trail.start && Pause() == 0) StartTrail();
		// the play block (one stack block for every layout of the tick: each layout sets only its own words)
		PlayArg arg = {};
		// 0x49..0x5C: layout 0x142FB40 at the spawn point, fading from 0x54
		const DemonSliceParams &g = PG();
		uint32_t i = (uint32_t)(mn->counter - g.opening.start);
		if (i < (uint32_t)g.opening.ticks)
		{
			if (i == 0) DecodeModelPrimLayout(fxp::layout(g.layouts.opening, LAYOUT_Fade, 0x1C8), mn->layout_a, 0x1C8);
			arg.depth = 0;
			arg.kind = KIND_Plain;
			arg.fade = i > (uint32_t)g.opening.fade_from ? (int16_t)((uint32_t)shl32(i - g.opening.fade_from, 12) / (uint32_t)g.opening.fade_ticks) : 0;
			FX_PART(PART_OPENING);
			Play(mn, 0, mn->layout_a, &arg, sp[0], 0, sp[2]);
		}
		// 0x53..0xB1: the wave layout at the spawn point; a rising layout task every 6 ticks up to 0xA5; the lift
		i = (uint32_t)(mn->counter - g.wave.start);
		if (i < (uint32_t)g.wave.ticks)
		{
			if (i == 0) DecodeModelPrimLayout(fxp::layout(g.layouts.wave, LAYOUT_Wave, 0x24C), mn->layout_b, 0x24C);
			if (i < (uint32_t)g.risers.until && i % (uint32_t)g.risers.every == 0 && Pause() == 0)
			{
				LayoutNode *t = (LayoutNode *)AddTaskToQueue(QLayouts(), own::Fn(ORIG_LayoutTask));
				if (t)
				{
					DecodeModelPrimLayout(fxp::layout(g.layouts.rise, LAYOUT_Rise, 0xC0), t->layout, 0xC0);
					t->pos[0] = sp[0];
					t->pos[1] = (int16_t)(sp[1] - g.risers.lift);
					t->pos[2] = sp[2];
				}
			}
			arg.depth = 0;
			arg.kind = KIND_Wave;
			arg.wave_y = 0x1000;
			arg.wave_xz = 0x1000;
			arg.fade = 0;
			if (i >= (uint32_t)g.wave.swell_from)
				arg.wave_xz = (int16_t)(ComputeSin((int32_t)((uint32_t)shl32(i - g.wave.swell_from, 10) / (uint32_t)g.wave.swell_ticks)) * g.wave.swell + 0x1000);
			arg.wave_amp = g.wave.amp;
			arg.wave_phase = (int16_t)(i * (uint32_t)g.wave.phase);
			arg.scroll = (uint16_t)(i * (uint32_t)g.wave.scroll);
			if (mn->counter >= g.lift.start)
			{
				int32_t u = shl32(mn->counter - g.lift.start, 12) / g.lift.ticks;
				if (u > 0x1000) u = 0x1000;
				arg.wave_y = (int16_t)(u + 0x1000);
				if (mn->counter < g.lift.end)
				{
					const int32_t a = ComputeCos(u >> 2);
					const int32_t b = ComputeCos((0x1000 - a) >> 2);
					const int16_t lift = (int16_t)(0x1000 - b);
					*(int16_t *)(Caster() + 0x1E) = (int16_t)(mn->caster0[1] - lift);
					*(int16_t *)(Target() + 0x1E) = (int16_t)(mn->target0[1] - lift);
				}
			}
			FX_PART(PART_WAVE);
			Play(mn, 1, mn->layout_b, &arg, sp[0], 0, sp[2]);
		}
		// 0x53..0x71: the caster slides from his start to the far point (sine ease)
		i = (uint32_t)(mn->counter - g.caster.slide_from);
		if (i < (uint32_t)g.caster.slide_ticks)
		{
			const int32_t s = ComputeSin((int32_t)((uint32_t)shl32(i, 12) / (uint32_t)g.caster.slide_ticks + 0x42) / 4);
			VecLerp(mn->caster0, mn->far_pt, 0x1000 - s, s, Caster() + 0x1C);
		}
		// 0x72..0x8A: layout 0x1433318 at the spawn point of 0x72
		i = (uint32_t)(mn->counter - g.slash.start);
		if (i < (uint32_t)g.slash.ticks)
		{
			if (i == 0)
			{
				DecodeModelPrimLayout(fxp::layout(g.layouts.slash, LAYOUT_Swing, 0x1A0), mn->layout_a, 0x1A0);
				memcpy(mn->keep, sp, 8);
			}
			arg.depth = 0;
			arg.kind = KIND_Swing;
			arg.scroll = (uint16_t)(i * (uint32_t)g.slash.scroll);
			arg.fade = 0;
			FX_PART(PART_SLASH);
			Play(mn, 2, mn->layout_a, &arg, mn->keep[0], 0, mn->keep[2]);
		}
		// 0xA3..0xB0: layout 0x1432448 at the target's effect point (y - 0x1000); the target pushed back, hidden at 0xB0
		i = (uint32_t)(mn->counter - g.push.start);
		if (i < (uint32_t)g.push.ticks)
		{
			if (i == 0)
			{
				DecodeModelPrimLayout(fxp::layout(g.layouts.push, LAYOUT_Push, 0x144), mn->layout_a, 0x144);
				GetDefaultEffectPosition(Target(), mn->far_pt);
			}
			arg.depth = 0;
			arg.kind = KIND_Plain;
			arg.fade = 0;
			FX_PART(PART_PUSH);
			Play(mn, 3, mn->layout_a, &arg, mn->far_pt[0], g.push.height, mn->far_pt[2]);
			if (Pause() == 0)
			{
				*(int16_t *)(Target() + 0x1C) += mn->step[0];
				*(int16_t *)(Target() + 0x20) += mn->step[2];
				if (i == (uint32_t)g.push.ticks - 1) *Target() |= 4;
			}
		}
		// 0xC8..0xDB: layout 0x142D8A4 at the target's start; his hit chain at 0xC8
		i = (uint32_t)(mn->counter - g.fall.start);
		if (i < (uint32_t)g.fall.ticks)
		{
			if (i == 0)
			{
				DecodeModelPrimLayout(fxp::layout(g.layouts.fall, LAYOUT_Fall, 0x208), mn->layout_b, 0x208);
				if (Pause() == 0) QueueChainTransformation(Target(), g.hits.chain);
			}
			arg.depth = 0;
			arg.kind = KIND_Plain;
			arg.fade = 0;
			FX_PART(PART_FALL);
			Play(mn, 4, mn->layout_b, &arg, mn->target0[0], 0, mn->target0[2]);
		}
		// white flashes at 0x72 and 0xA3
		const uint32_t white = (uint32_t)(g.whites.in + g.whites.hold + g.whites.out);
		i = (uint32_t)(mn->counter - g.whites.first);
		if (i < white)
		{
			FX_PART(PART_WHITES);
			FadeTile((int32_t)i, g.whites.r, g.whites.g, g.whites.b, g.whites.in, g.whites.hold, g.whites.out);
		}
		i = (uint32_t)(mn->counter - g.whites.second);
		if (i < white)
		{
			FX_PART(PART_WHITES);
			FadeTile((int32_t)i, g.whites.r, g.whites.g, g.whites.b, g.whites.in, g.whites.hold, g.whites.out);
		}
		// 0xC0..0xDA: the target at his start, dropped (0xC0..0xC7), drawn by the module and hidden; 0xDB: his result
		i = (uint32_t)(mn->counter - g.target.start);
		bool draw_target = false;
		if (i < (uint32_t)g.target.drop)
		{
			if (i == 0)
			{
				*(int16_t *)(Target() + 0x1C) = mn->target0[0];
				*(int16_t *)(Target() + 0x20) = mn->target0[2];
			}
			if (Pause() == 0)
			{
				*(int16_t *)(Target() + 0x1E) = DropHeight((int32_t)i);
				if (i == (uint32_t)g.target.drop - 1)
				{
					memcpy(Target() + 0x1C, &mn->target0[0], 4);
					memcpy(Target() + 0x20, &mn->target0[2], 4);
				}
			}
			draw_target = true;
		}
		else if (i < (uint32_t)g.target.ticks) draw_target = true;
		else if (i == (uint32_t)g.target.ticks)
		{
			if (Pause() == 0) ApplyActionResultToTarget(Ctx()->actions[0].targets);
			*(uint16_t *)Target() &= 0xFFFB;
		}
		if (draw_target)
		{
			int16_t box[2];
			MeasureHeight(Target(), box);
			int16_t off[4];
			int16_t angles[4];
			off[1] = (int16_t)((box[1] + box[0]) >> 1);
			off[2] = 0;
			off[0] = 0;
			off[3] = 0;
			angles[0] = 0x800;
			angles[2] = 0;
			angles[1] = 0;
			angles[3] = 0;
			// 30 fps layer: see mag251_demon_slice_held.inc
			FX_HELD(held_note_target(mn, (int32_t)i, off);)
			FX_PART(PART_TARGET);
			DrawEntity212(Target(), angles, off);
			*Target() |= 4;
		}
		// counters: the flames dropped, the stream load, its wait, the pump
		if (mn->counter == g.flames.drop)
		{
			QFlames()->tail = nullptr;
			QFlames()->head = nullptr;
		}
		else if (mn->counter == 0x23) StreamQueueLoad(0x2C9, TexBase() + ARENA_A, 0);
		else if (mn->counter == 0x36 && StreamBusy() != 0) return 0;
		StreamPump();
		if (mn->counter == g.timing.start) BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		else if (mn->counter == g.timing.swing) BdPlaySE((const void *)fxp::sound(g.sounds.swing, (uint32_t)SOUND_Swing), 0, 0x80);
		else if (mn->counter == g.timing.fall) BdPlaySE((const void *)fxp::sound(g.sounds.fall, (uint32_t)SOUND_Fall), 0, 0x80);
		// 0x72..0xA7: every 5 ticks a hit layout task at the target and his hit chain; up to 0x8C the caster swings
		i = (uint32_t)(mn->counter - g.hits.start);
		if (i < (uint32_t)g.hits.ticks)
		{
			const uint32_t k = i % (uint32_t)g.hits.every;
			if (k == 0)
			{
				LayoutNode *t = (LayoutNode *)AddTaskToQueue(QLayouts(), own::Fn(ORIG_LayoutTask));
				if (t)
				{
					DecodeModelPrimLayout(fxp::layout(g.layouts.hit, LAYOUT_Hit, 0x144), t->layout, 0x144);
					GetDefaultEffectPosition(Target(), t->pos);
					QueueChainTransformation(Target(), g.hits.chain);
				}
			}
			if (mn->counter < g.caster.swing_until)
			{
				const int32_t s = ComputeSin(shl32((int32_t)k, 12) / 9 / 2);
				VecLerp(mn->far_pt, mn->near_pt, 0x1000 - s, s, Caster() + 0x1C);
			}
		}
		// 0xA8..0xB5: the caster back to his start; 0xB5: 16 sparkles around him
		i = (uint32_t)(mn->counter - g.caster.back_from);
		if (i < (uint32_t)g.caster.back_ticks)
		{
			const int32_t t = (int32_t)((uint32_t)shl32((int32_t)i + 1, 12) / (uint32_t)g.caster.back_ticks);
			if (i == 0) memcpy(mn->keep, Caster() + 0x1C, 8);
			VecLerp(mn->keep, mn->caster0, 0x1000 - t, t, Caster() + 0x1C);
			if (i == (uint32_t)g.caster.back_ticks - 1)
			{
				int32_t count = g.sparkles.count;
				int32_t angle = 0;
				SparkleNode *s = count > 0 ? (SparkleNode *)AddTaskToQueue(QSparkles(), own::Fn(ORIG_SparkleTask)) : nullptr;
				while (s)
				{
					const int32_t r = CrtRand() + 0x4000;
					const int32_t sx = mul32(ComputeSin(angle), r) >> 15;
					const int32_t cz = mul32(ComputeCos(angle), r) >> 15;
					s->pos[0] = (int16_t)(sx / 8 + mn->caster0[0]);
					s->pos[2] = (int16_t)(cz / 8 + mn->caster0[2]);
					s->vel[0] = (int8_t)(sx >> 4);
					s->vel[2] = (int8_t)(cz >> 4);
					angle += g.sparkles.step;
					s->pos[1] = 0;
					s->vel[1] = (int8_t)(0xF8 - (CrtRand() & 7));
					s->frame = 0;
					if (--count == 0) break;
					s = (SparkleNode *)AddTaskToQueue(QSparkles(), own::Fn(ORIG_SparkleTask));
				}
			}
		}
		if (mn->counter >= g.timing.end)
		{
			memcpy(Caster() + 0x1C, &mn->caster0[0], 4);
			memcpy(Caster() + 0x20, &mn->caster0[2], 4);
			return TASK_END;
		}
		mn->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// TrailTask (0x6F5B20): the blade trail of slot 4's bone 1
	// ------------------------------------------------------------------
	// 0x6F87D0 (Bloodfest's range): bone `bone` of a skeleton (0xF0+: the remap table) -> its unit z axis (the bone's
	// rotation x (0, 0, -0x1000)) and its point
	static void BoneFrame(uint8_t *anim, int32_t bone, int32_t *dir, int16_t *point)
	{
		const uint8_t *skel = **(uint8_t ***)(anim + 4);
		if (bone >= 0xF0) bone = skel[bone - 0xEC];
		const uint8_t *b = skel + 0x10 + bone * 0x30;
		dir[1] = 0;
		dir[0] = 0;
		dir[2] = -0x1000;
		TransformVectorBy3x3Matrix((const Mat4x3 *)(b + 0x10), dir, dir);
		NormalizeVector(dir, dir);
		point[0] = *(const int16_t *)(b + 0x24);
		point[1] = *(const int16_t *)(b + 0x28);
		point[2] = *(const int16_t *)(b + 0x2C);
	}
	// 0x6F5FC0: an entity's bone direction / point through its world matrix (+0x40), the point plus its translation
	static void WeaponBone(uint8_t *e, int32_t bone, int32_t *dir, int16_t *point)
	{
		BoneFrame(e + 0x60, bone, dir, point);
		TransformVectorBy3x3Matrix((const Mat4x3 *)(e + 0x40), dir, dir);
		NormalizeVector(dir, dir);
		MatVec((const Mat4x3 *)(e + 0x40), point, point);
		point[0] = (int16_t)(point[0] + *(const int16_t *)(e + 0x54));
		point[1] = (int16_t)(point[1] + *(const int16_t *)(e + 0x58));
		point[2] = (int16_t)(point[2] + *(const int16_t *)(e + 0x5C));
	}

	static void TrailSplines(TrailNode *t, int32_t cnt)
	{
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, t->pts_a, wk);
		for (int32_t k = 0, u = 0; k < 32; k++, u += 0x1000) SplineEval(cnt, wk, t->spl_a[k], u / 31);
		SplineSetup(cnt, t->pts_b, wk);
		for (int32_t k = 0, u = 0; k < 32; k++, u += 0x1000) SplineEval(cnt, wk, t->spl_b[k], u / 31);
		FieldFree(0x190);
	}

	// the first points and two unit directions (read by nothing)
	static void TrailEnds(TrailNode *t)
	{
		memcpy(t->first_b, t->spl_b[0], 8);
		t->dir_b[0] = (int32_t)t->spl_b[0][0] - t->spl_b[1][0];
		t->dir_b[1] = (int32_t)t->spl_b[0][1] - t->spl_b[1][1];
		t->dir_b[2] = (int32_t)t->spl_b[0][2] - t->spl_b[1][2];
		NormalizeVector(t->dir_b, t->dir_b);
		memcpy(t->first_a, t->spl_a[0], 8);
		t->dir_ab[0] = (int32_t)t->first_b[0] - t->first_a[0];
		t->dir_ab[1] = (int32_t)t->first_b[1] - t->first_a[1];
		t->dir_ab[2] = (int32_t)t->first_b[2] - t->first_a[2];
		NormalizeVector(t->dir_ab, t->dir_ab);
	}

	// 30 quad pairs between the two splines: gouraud from 0x3A185820 fading toward the far colour (black) along the
	// trail, and a copy whose leading edge is 0x3A000000 (the cursor moves past the second packet even when none is drawn)
	static void TrailDraw(const int16_t (*sa)[4], const int16_t (*sb)[4], TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk + 9;
		w->col1 = 0x3A000000 | ((uint32_t)PG().trail.colour & 0xFFFFFF);
		w->base_col = w->col1;
		for (int32_t k = 0, tacc = 0; tacc < 0x1E000; k++, tacc += 0x1000)
		{
			w->col0 = w->col1;
			pk[0] = 0x8000000;
			GteLoadV012(sa[k], sa[k + 1], sb[k]);
			GteRTPT();
			GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb[k + 1]);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(&w->otz);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(&w->base_col);
			GteDPCS();
			GteStoreRGB2(&w->col1);
			pk[5] = pk[1] = w->col0;
			pk[7] = pk[3] = w->col1;
			InsertPrimAutoDepth(OTBase() + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk);
			pk2[0] = 0x8000000;
			pk2[3] = 0x3A000000;
			pk2[1] = 0x3A000000;
			pk2[5] = w->col0;
			pk2[7] = w->col1;
			pk2[2] = pk[2];
			pk2[4] = pk[4];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			InsertPrimAutoDepth(OTBase() + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl TrailTask(TaskNode *n)
	{
		TrailNode *t = (TrailNode *)n;
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		WeaponBone(TRAIL_ENTITY, 1, w->dir, w->base);
		w->tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], PG().trail.length) >> 12) + w->base[0]);
		w->tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], PG().trail.length) >> 12) + w->base[1]);
		w->tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], PG().trail.length) >> 12) + w->base[2]);
		int32_t idx = t->counter % 7;
		RingEntry *e = &t->ring[idx];
		e->alive = 1;
		memcpy(e->base, w->base, 8);
		memcpy(e->tip, w->tip, 8);
		const int32_t want = t->counter >= PG().trail.shorten ? PG().trail.ticks + 1 - t->counter : 7;
		int32_t cnt = 0;
		while (cnt < want)
		{
			const RingEntry *r = &t->ring[idx];
			if (!r->alive) break;
			idx--;
			memcpy(t->pts_a[cnt], r->base, 8);
			memcpy(t->pts_b[cnt], r->tip, 8);
			if (idx < 0) idx = 6;
			cnt++;
		}
		if (cnt > 1)
		{
			TrailSplines(t, cnt);
			TrailEnds(t);
			// 30 fps layer: see mag251_demon_slice_held.inc
			FX_HELD(held_note_trail(t);)
			FX_PART(PART_TRAIL);
			TrailDraw(t->spl_a, t->spl_b, w);
		}
		FieldFree(0x40);
		if (Pause() != 0) return 0;
		t->counter++;
		return t->counter >= PG().trail.ticks ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// LayoutTask (0x6F6530): its layout at its point through the view (depth -0x80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *t = (LayoutNode *)n;
		PlayArg arg = {};
		arg.m.m[2][2] = 0x1000;
		arg.m.m[1][1] = 0x1000;
		arg.m.m[0][0] = 0x1000;
		arg.m.t[1] = t->pos[1];
		arg.m.t[2] = t->pos[2];
		arg.m.t[0] = t->pos[0];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.depth = -0x80;
		arg.kind = KIND_Plain;
		arg.fade = g_task_fade;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_task_layout(t, &arg);)
		FX_PART(*(const uint32_t *)t->layout == fxp::layout(PG().layouts.rise, LAYOUT_Rise, 0xC0) ? PART_RISERS : PART_HITS);
		const int left = prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, (int)Pause());
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// SparkleTask (0x6F65F0): sequence 0x142D744 at its point, frame = its counter; 13 frames
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(held_note_draw(ORIG_SparkleTask, s);)
		FX_PART(PART_SPARKLES);
		TransformCameraByShadowRotation(s->pos, 0x1000, -0x400);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = fxp::sequence(PG().seqs.sparkle, SEQ_Sparkle);
		*(uint16_t *)(h + 4) = (uint16_t)s->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		g_task_fade = (int16_t)RenderOT();
		FieldFree(0xB4);
		if (Pause() != 0) return 0;
		s->frame++;
		if (s->frame >= PG().sparkles.frames) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Setup (0x6F44D0 -> _Init 0x6F44E0): the game runs its own; Fujin's programs this (its memory may be a copy's): the
	// TIM (exe data), the magic buffer, the context, the caster's and action 0's first target's entities, the master, the
	// camera. (The file loader is the engine's null sub.)
	// ------------------------------------------------------------------
	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(fxp::tim(PG().res.tim, TIM));
		TexBase() = (uint32_t)layouts::MagicBuffer();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		Target() = Entity(ctx->actions->targets[0]);
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2556400), 0x10, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 1);
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "master", "Master", "The main task's ticks, its three sounds, the screen flash rising and falling; the TIM and the camera" },
			{ 0, "caster", "Caster", "The caster sliding toward the target, swinging between two points by him, then back to his start (the effect moves him, it draws nothing)" },
			{ PART_FLAMES, "flames", "Flames", "Flames around the target (Fire's parts), each with its ground glow, embers rising and sparks drifting" },
			{ PART_OPENING, "opening", "Opening", "Layout A at the caster's spawn point, fading out" },
			{ PART_WAVE, "wave", "Wave", "The wave layout at the caster's spawn point, its vertices pushed by a travelling wave, its texture scrolling; it swells, then grows taller as it lifts the caster and the target" },
			{ PART_RISERS, "risers", "Risers", "Layout tasks rising above the caster's spawn point while the wave plays" },
			{ PART_SLASH, "slash", "Slash", "The slash layout at the caster's spawn point of its first tick, its texture scrolling" },
			{ PART_HITS, "hits", "Hits", "A hit layout task at the target every few ticks, with his hit chain" },
			{ PART_PUSH, "push", "Push", "The push layout at the target's effect point; the target pushed back a step a tick, hidden at its end" },
			{ PART_TARGET, "target", "Target", "The target back at his start, dropped from above, drawn by the module turned over about his middle; then his result" },
			{ PART_FALL, "fall", "Fall", "The fall layout at the target's start, his hit chain at its first tick" },
			{ PART_WHITES, "whites", "White flashes", "Two full-screen flashes (fading in, held, fading out)" },
			{ PART_TRAIL, "trail", "Blade trail", "The blade trail of slot 4's bone 1: a ring of its last 7 points splined, drawn as gouraud quads fading along it" },
			{ PART_SPARKLES, "sparkles", "Sparkles", "Sparkles flying out round the caster when he is back" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The main task's ticks" },
			{ "flash", "Screen flash", "The screen flash's level and slopes" },
		};
#define MPF(member) (uint32_t)offsetof(DemonSliceParams, member)
		static const Field FIELDS[] = {
			{ "master", "sounds.start", "Start sound", "At the start sound's tick", MPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.swing", "Swing sound", "At the swing sound's tick", MPF(sounds.swing), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.fall", "Fall sound", "At the fall sound's tick", MPF(sounds.fall), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "timing.start", "Start sound", "The main task's tick of the start sound", MPF(timing.start), fxp::I16, 0, 400 },
			{ "master", "timing.swing", "Swing sound", "Its tick of the swing sound", MPF(timing.swing), fxp::I16, 0, 400 },
			{ "master", "timing.fall", "Fall sound", "Its tick of the fall sound", MPF(timing.fall), fxp::I16, 0, 400 },
			{ "master", "timing.end", "End", "Its last tick: the caster put back at his start (the screen flash falls to 0 there)", MPF(timing.end), fxp::I16, 1, 400 },
			{ "master", "flash.level", "Flash level", "The screen flash's level", MPF(flash.level), fxp::I16, 0, 0x1000 },
			{ "master", "flash.rise", "Flash rise", "It rises over the first ticks (at least 1)", MPF(flash.rise), fxp::I16, 1, 400 },
			{ "master", "flash.fall", "Flash fall", "It falls over the last ticks before the end (at least 1)", MPF(flash.fall), fxp::I16, 1, 400 },
			{ "master", "res.tim", "TIM", "The TIM its setup uploads (exe data: the effect has no file)", MPF(res.tim), fxp::Tim, 0, 0x7FFFFFFF },
			{ "master", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", MPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "caster", "caster.slide_from", "Slide from", "The main task's tick the caster starts sliding to the far point (an eased sine)", MPF(caster.slide_from), fxp::I16, 0, 400 },
			{ "caster", "caster.slide_ticks", "Slide ticks", "Its ticks (at least 1)", MPF(caster.slide_ticks), fxp::I16, 1, 400 },
			{ "caster", "caster.swing_until", "Swings until", "While the hits run, below this tick the caster swings between the far and the near point", MPF(caster.swing_until), fxp::I16, 0, 400 },
			{ "caster", "caster.back_from", "Back from", "The tick he starts back to his start (the sparkles at its end; 0x37 at least: their pool is set up then)", MPF(caster.back_from), fxp::I16, 0x37, 400 },
			{ "caster", "caster.back_ticks", "Back ticks", "Its ticks (at least 1)", MPF(caster.back_ticks), fxp::I16, 1, 400 },
			{ "flames", "flames.count", "Count", "Flames around the target", MPF(flames.count), fxp::I16, 0, 8 },
			{ "flames", "flames.frames", "Frames", "A flame's frames (its flipbook's: 16 at most)", MPF(flames.frames), fxp::I16, 1, 16 },
			{ "flames", "flames.embers", "Embers", "Embers rising from each flame", MPF(flames.embers), fxp::I16, 0, 16 },
			{ "flames", "flames.sparks", "Sparks", "Sparks drifting from each flame", MPF(flames.sparks), fxp::I16, 0, 20 },
			{ "flames", "flames.size", "Size", "A flame's size: this + a random below the next", MPF(flames.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "flames", "flames.size_rand", "Size random", "(at least 1)", MPF(flames.size_rand), fxp::I16, 1, 0x7FFF },
			{ "flames", "flames.drop", "Dropped", "The main task's tick every flame, ember and spark is dropped", MPF(flames.drop), fxp::I16, 0, 400 },
			{ "flames", "seqs.flame", "Flame", "The flames' sprite sequence (a flipbook)", MPF(seqs.flame), fxp::Seq, 0, 0x7FFFFFFF },
			{ "flames", "seqs.glow", "Ground glow", "Their ground glow's sprite sequence", MPF(seqs.glow), fxp::Seq, 0, 0x7FFFFFFF },
			{ "flames", "seqs.ember", "Ember", "The embers' sprite sequence", MPF(seqs.ember), fxp::Seq, 0, 0x7FFFFFFF },
			{ "opening", "opening.start", "Start", "The main task's tick it starts", MPF(opening.start), fxp::I16, 0, 400 },
			{ "opening", "opening.ticks", "Ticks", "Its ticks", MPF(opening.ticks), fxp::I16, 0, 400 },
			{ "opening", "opening.fade_from", "Fade from", "After this tick of it, it fades out", MPF(opening.fade_from), fxp::I16, 0, 400 },
			{ "opening", "opening.fade_ticks", "Fade ticks", "Over so many ticks (at least 1)", MPF(opening.fade_ticks), fxp::I16, 1, 400 },
			{ "opening", "layouts.opening", "Layout", "The prim-model layout played", MPF(layouts.opening), fxp::Layout, 0, 0x7FFFFFFF },
			{ "wave", "wave.start", "Start", "The main task's tick it starts (0x37 at least: the risers' pool is set up then)", MPF(wave.start), fxp::I16, 0x37, 400 },
			{ "wave", "wave.ticks", "Ticks", "Its ticks", MPF(wave.ticks), fxp::I16, 0, 400 },
			{ "wave", "wave.phase", "Wave step", "The wave's phase step a tick (0x1000 a turn)", MPF(wave.phase), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "wave", "wave.amp", "Wave amplitude", "How far the wave pushes the vertices", MPF(wave.amp), fxp::I16, -0x1000, 0x1000 },
			{ "wave", "wave.scroll", "Scroll", "Its texture scroll a tick", MPF(wave.scroll), fxp::I16, -0x100, 0x100 },
			{ "wave", "wave.swell_from", "Swell from", "From this tick of it, it swells sideways", MPF(wave.swell_from), fxp::I16, 0, 400 },
			{ "wave", "wave.swell_ticks", "Swell ticks", "A quarter turn of the swell's sine over so many ticks (at least 1)", MPF(wave.swell_ticks), fxp::I16, 1, 400 },
			{ "wave", "wave.swell", "Swell", "Its width grows by this x the sine (x 1 = its own width)", MPF(wave.swell), fxp::I16, -8, 8 },
			{ "wave", "lift.start", "Lift from", "From this main task's tick it grows taller (to twice its height)", MPF(lift.start), fxp::I16, 0, 400 },
			{ "wave", "lift.ticks", "Lift ticks", "Over so many ticks (at least 1)", MPF(lift.ticks), fxp::I16, 1, 400 },
			{ "wave", "lift.end", "Lift until", "Below this tick it lifts the caster and the target (a cosine ease)", MPF(lift.end), fxp::I16, 0, 400 },
			{ "wave", "layouts.wave", "Layout", "The wave layout played (its first two models' heights are normalised for the wave)", MPF(layouts.wave), fxp::Layout, 0, 0x7FFFFFFF },
			{ "risers", "risers.every", "Every", "A riser every so many of the wave's ticks (at least 1)", MPF(risers.every), fxp::I16, 1, 400 },
			{ "risers", "risers.until", "Until", "Below this tick of the wave", MPF(risers.until), fxp::I16, 0, 400 },
			{ "risers", "risers.lift", "Lift", "Above the caster's spawn point", MPF(risers.lift), fxp::I16, -0x2000, 0x2000 },
			{ "risers", "layouts.rise", "Layout", "The risers' prim-model layout", MPF(layouts.rise), fxp::Layout, 0, 0x7FFFFFFF },
			{ "slash", "slash.start", "Start", "The main task's tick it starts", MPF(slash.start), fxp::I16, 0, 400 },
			{ "slash", "slash.ticks", "Ticks", "Its ticks", MPF(slash.ticks), fxp::I16, 0, 400 },
			{ "slash", "slash.scroll", "Scroll", "Its texture scroll a tick", MPF(slash.scroll), fxp::I16, -0x100, 0x100 },
			{ "slash", "layouts.slash", "Layout", "The slash's prim-model layout", MPF(layouts.slash), fxp::Layout, 0, 0x7FFFFFFF },
			{ "hits", "hits.start", "Start", "The main task's tick the hits start (0x37 at least: their pool is set up then)", MPF(hits.start), fxp::I16, 0x37, 400 },
			{ "hits", "hits.ticks", "Ticks", "For so many ticks", MPF(hits.ticks), fxp::I16, 0, 400 },
			{ "hits", "hits.every", "Every", "A hit every so many ticks (at least 1); the caster's swing is that long too", MPF(hits.every), fxp::I16, 1, 400 },
			{ "hits", "hits.chain", "Hit chain", "The target's hit chain (QueueChainTransformation) with each hit and the fall", MPF(hits.chain), fxp::I16, 0, 0x40 },
			{ "hits", "layouts.hit", "Layout", "The hits' prim-model layout", MPF(layouts.hit), fxp::Layout, 0, 0x7FFFFFFF },
			{ "push", "push.start", "Start", "The main task's tick it starts", MPF(push.start), fxp::I16, 0, 400 },
			{ "push", "push.ticks", "Ticks", "Its ticks (the target hidden at its last)", MPF(push.ticks), fxp::I16, 1, 400 },
			{ "push", "push.height", "Height", "The layout's height", MPF(push.height), fxp::I16, -0x4000, 0x4000 },
			{ "push", "push.step", "Step", "The target pushed back so much a tick", MPF(push.step), fxp::I16, -0x1000, 0x1000 },
			{ "push", "layouts.push", "Layout", "The push's prim-model layout", MPF(layouts.push), fxp::Layout, 0, 0x7FFFFFFF },
			{ "target", "target.start", "Start", "The main task's tick he is back at his start", MPF(target.start), fxp::I16, 0, 400 },
			{ "target", "target.drop", "Drop", "He drops from above over so many ticks (at least 1)", MPF(target.drop), fxp::I16, 1, 400 },
			{ "target", "target.ticks", "Ticks", "He is drawn by the module until this tick of it, then his result and he is shown", MPF(target.ticks), fxp::I16, 0, 400 },
			{ "fall", "fall.start", "Start", "The main task's tick it starts", MPF(fall.start), fxp::I16, 0, 400 },
			{ "fall", "fall.ticks", "Ticks", "Its ticks", MPF(fall.ticks), fxp::I16, 0, 400 },
			{ "fall", "layouts.fall", "Layout", "The fall's prim-model layout", MPF(layouts.fall), fxp::Layout, 0, 0x7FFFFFFF },
			{ "whites", "whites.first", "First", "The main task's tick of the first", MPF(whites.first), fxp::I16, 0, 400 },
			{ "whites", "whites.second", "Second", "The tick of the second", MPF(whites.second), fxp::I16, 0, 400 },
			{ "whites", "whites.in", "In", "Ticks fading in", MPF(whites.in), fxp::I16, 0, 100 },
			{ "whites", "whites.hold", "Hold", "Ticks held", MPF(whites.hold), fxp::I16, 0, 100 },
			{ "whites", "whites.out", "Out", "Ticks fading out", MPF(whites.out), fxp::I16, 0, 100 },
			{ "whites", "whites.r", "Red", "Their colour", MPF(whites.r), fxp::I16, 0, 255 },
			{ "whites", "whites.g", "Green", "Their colour", MPF(whites.g), fxp::I16, 0, 255 },
			{ "whites", "whites.b", "Blue", "Their colour", MPF(whites.b), fxp::I16, 0, 255 },
			{ "trail", "trail.start", "Start", "The main task's tick it starts (0x37 at least: its pool is set up then)", MPF(trail.start), fxp::I16, 0x37, 400 },
			{ "trail", "trail.ticks", "Ticks", "Its ticks", MPF(trail.ticks), fxp::I16, 1, 400 },
			{ "trail", "trail.shorten", "Shorter from", "From this tick of it, it keeps ticks + 1 - its tick points (7 before)", MPF(trail.shorten), fxp::I16, 0, 400 },
			{ "trail", "trail.length", "Length", "Its tip's distance from the bone along the blade", MPF(trail.length), fxp::I16, -0x2000, 0x2000 },
			{ "trail", "trail.colour", "Colour", "Its colour at the blade (0xBBGGRR), fading to black along it", MPF(trail.colour), fxp::I32, 0, 0xFFFFFF },
			{ "sparkles", "sparkles.count", "Count", "How many", MPF(sparkles.count), fxp::I16, 0, 16 },
			{ "sparkles", "sparkles.step", "Angle step", "The angle between two of them", MPF(sparkles.step), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "sparkles", "sparkles.frames", "Frames", "A sparkle's frames (its flipbook's: 13 at most)", MPF(sparkles.frames), fxp::I16, 1, 13 },
			{ "sparkles", "seqs.sparkle", "Sparkle", "The sparkles' sprite sequence", MPF(seqs.sparkle), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef MPF
	}
}

	void register_mag251_demon_slice()
	{
		fxp::register_params({ 251, "Demon Slice", &demon251::live, &demon251::GAME, sizeof(demon251::DemonSliceParams),
		                       demon251::table::PARTS, (int)(sizeof(demon251::table::PARTS) / sizeof(demon251::table::PARTS[0])),
		                       demon251::table::FIELDS, (int)(sizeof(demon251::table::FIELDS) / sizeof(demon251::table::FIELDS[0])),
		                       demon251::table::GROUPS, (int)(sizeof(demon251::table::GROUPS) / sizeof(demon251::table::GROUPS[0])) });
		own::note(demon251::ORIG_Setup, (void *)demon251::Setup);
		own::note(demon251::ORIG_MasterTask, (void *)demon251::MasterTask);
		register_port(demon251::ORIG_MasterTask, (void *)demon251::MasterTask, "251 MasterTask", 251);
		own::note(demon251::ORIG_FlameSpawner, (void *)demon251::FlameSpawner);
		register_port(demon251::ORIG_FlameSpawner, (void *)demon251::FlameSpawner, "251 FlameSpawner", 251);
		own::note(demon251::ORIG_FlameTask, (void *)demon251::FlameTask);
		register_port(demon251::ORIG_FlameTask, (void *)demon251::FlameTask, "251 FlameTask", 251);
		own::note(demon251::ORIG_SparkTask, (void *)demon251::SparkTask);
		register_port(demon251::ORIG_SparkTask, (void *)demon251::SparkTask, "251 SparkTask", 251);
		own::note(demon251::ORIG_EmberTask, (void *)demon251::EmberTask);
		register_port(demon251::ORIG_EmberTask, (void *)demon251::EmberTask, "251 EmberTask", 251);
		own::note(demon251::ORIG_MainTask, (void *)demon251::MainTask);
		register_port(demon251::ORIG_MainTask, (void *)demon251::MainTask, "251 MainTask", 251);
		own::note(demon251::ORIG_TrailTask, (void *)demon251::TrailTask);
		register_port(demon251::ORIG_TrailTask, (void *)demon251::TrailTask, "251 TrailTask", 251);
		own::note(demon251::ORIG_LayoutTask, (void *)demon251::LayoutTask);
		register_port(demon251::ORIG_LayoutTask, (void *)demon251::LayoutTask, "251 LayoutTask", 251);
		own::note(demon251::ORIG_SparkleTask, (void *)demon251::SparkleTask);
		register_port(demon251::ORIG_SparkleTask, (void *)demon251::SparkleTask, "251 SparkleTask", 251);
		// 30 fps layer: see mag251_demon_slice_held.inc
		FX_HELD(register_mag251_held();)
	}
	FX_LAB_MODULE(register_mag251_demon_slice);
}

#ifdef FF8_FX_HELD
#include "mag251_demon_slice_held.inc"
#endif
