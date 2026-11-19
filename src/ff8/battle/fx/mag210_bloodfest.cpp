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

// Effect 210: Bloodfest (enemy attack 315 of kernel.bin; Seifer c0m128; MAG_210_*). Demon Slice (251) is its twin: the
// flames, sparks, embers, the blade trail and the layout task are 251's code on this module's globals.
//
// Structure (setup MAG_210_BLOODFEST 0x6F6800 -> _Init 0x6F6810; the table's file loader 0x5755D0; the init queues the
// module's own TIM 0x1470DC4 (exe data), keeps the context and the CASTER's entity, starts the master and camera
// animation 0x145D2BC):
//   MasterTask (0x6F6890) - alternates the packet arena (magic buffer + 0xB1B8 / + 0x1B1B8) and the stage-capture
//     cursor (+ 0x1B1B8 / + 0x2B1B8); the screen flash level is 0 unless the main task sets it. On counter 1 (once): the
//     stream state (+ 0xA708), the main pool (1 x 0x3C0 at + 0xA750) and the flame pool (0x96 x 0x24 at + 0), the main
//     task (cleared): action 0's targets (up to 3) as records (entity, position, the zero angles 0x2556520), the box of
//     their effect positions (its centre into the node and 0x2556538), a flame spawner on each, the caster's position
//     (0x2556530). Then (running): the view orthonormalised into 0x2556560, the main, flame, sparkle, rock, trail and
//     layout-task queues, the screen flash; ends when the main and the layout-task queues are empty (flash off).
//   MainTask (0x6F7310) - counters 0..0xF3 (then 0x133 while the preload waits; the counter steps last). Flash rising
//     0..0xB, 0x8FC, falling 0xE7..0xF2. 0x37..0x3D / 0x48..0x63: afterimages (212's / 274's code) of the entity after the
//     caster's slot, recorded up to 0x3B / 0x61; 0x37: the trail. 0x40..0x4E: layout 0x146D8A4 at the caster's effect
//     point. 0x41..0x48: prim model 0x145D6AC at the caster's bone 0x25, turning, fading in. 0x55: the stage captured into
//     VRAM over the burning layout's box; 0x55..0x72: the three-part draw at the centre (and a second from 0x60).
//     0x73..0xC2: the targets measured (0x73), lifted on a sine / cubic curve, spun, drawn by the module (0x6F9960, their
//     flags 0x0C set); the burning layout 0x1462010 (kind table 0x14913E4); 8 sparkles + 8 rocks a tick 0x73..0x76.
//     0xC3..0xEA: the targets dropped, the caster and the next entity hidden, layout 0x146A668, 2 rocks a tick. 0xEB:
//     targets / flags put back, the stage groups' bit 1 set (0x6F8BD0), the rocks dropped. 0xEC..0xF3: the targets raised
//     0x200 a tick; 0xF1 sound, 0xF3 a layout task (0x146ED2C) per target and action 0's result (0x506BA0). White tile
//     series 0xE3..0xF2 (0x700150). 0x21: the flames dropped; 0x23: stream load 0x2C0; 0x36: waits while the stream is
//     busy; 0xEB: loads 0x168..0x16A; the pump every tick. Sounds at 0 / 0x37 / 0x73. From 0xF3: ends once the preload
//     (0x534300) is done.
//   FlameSpawner (0x6F6C00), Flame (0x6F6DA0), Spark (0x6F7130), Ember (0x6F7230) - 251's (Fire's parts).
//   TrailTask (0x6F82D0) - 251's trail, 0x3B ticks, shorter from 0x35.
//   LayoutTask (0x6F9AA0) - its layout at its point (depth -0x80).
//   RockTask (0x6F9B60) - prim model 0x1461D98, turning (x / y bytes x 16), falling; SparkleTask (0x6F9C60) - sequence
//     0x145D4EC, rising; 16 frames.
//   PrimCallback (0x6F8850) draws every layout object (0x701DD0): kinds 0x31 / 0x32 / 0x33 texture windows, 0x39 depth.
// The tasks test the module's pause word 0x2556478 (never written: 0).
// ENTITY WRITES: the targets +0x1E (0x73..0xF3) and their matrix translation y +0x58 (0x73..0xEA), flags 0x0C (set 0x73,
// cleared 0xEB), +0x1C..+0x23 put back at 0xEB; the caster's and the next entity's flag 4 (0xC3..0xEA). The camera is the
// engine's camera animation.
// Module globals: 0x2556478..0x2556580 (pause 0x2556478, packet cursor 0x255647C, capture cursor 0x2556480, context
// 0x2556484, caster 0x2556488, queues 0x2556490 layout tasks / 0x25564A0 trail / 0x25564B0 rocks / 0x25564C0 sparkles /
// 0x25564D0 main / 0x25564E0 master, master pool 0x25564F0, zero angles 0x2556520, caster position 0x2556530, centre
// 0x2556538, flash level 0x2556540, flame queue 0x2556548, magic buffer 0x2556558, orthonormal view 0x2556560).
// FUJIN: Fujin's described Bloodfest (effect_210.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn(), the setup (0x6F6800 -> _Init 0x6F6810) ported below (the file loader is the engine's bare
// ret 0x5755D0); its parameters (BloodfestParams: the sounds and the ticks, the screen flash, the flames, the
// afterimages, the rising layout, the bone model, the vortex, the lift and its burst, the drop and its rocks, the raise,
// the white flash, the trail, the rocks, the sparkles, the 4 layouts, 3 meshes, 4 flipbooks, the TIM and the camera
// track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace blood210
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
	inline uint32_t &Pause() { return var<uint32_t>(own::R(0x2556478)); }           // never written by the game (0)
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x255647C)); }
	inline uint32_t &CaptureCursor() { return var<uint32_t>(own::R(0x2556480)); }
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2556484)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2556488)); }
	inline TaskQueue *QLayouts() { return (TaskQueue *)own::R(0x2556490); }          // + 0, 3 x 0x714
	inline TaskQueue *QTrail() { return (TaskQueue *)own::R(0x25564A0); }            // + 0x1BBC, 1 x 0x34C
	inline TaskQueue *QRocks() { return (TaskQueue *)own::R(0x25564B0); }            // + 0x183C, 0x20 x 0x1C
	inline TaskQueue *QSparkles() { return (TaskQueue *)own::R(0x25564C0); }         // + 0x153C, 0x20 x 0x18
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x25564E0); }         // 1 x 0x10 (0x25564F0)
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x25564D0); }             // + 0xA750, 1 x 0x3C0
	inline TaskQueue *QFlames() { return (TaskQueue *)own::R(0x2556548); }           // + 0, 0x96 x 0x24 (up to 0x21)
	inline uint32_t *ZeroAngles() { return (uint32_t *)own::R(0x2556520); }          // never written
	inline uint32_t *CasterPos() { return (uint32_t *)own::R(0x2556530); }
	inline int16_t *Centre() { return (int16_t *)own::R(0x2556538); }
	inline int32_t &FlashLevel() { return var<int32_t>(own::R(0x2556540)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2556558)); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 *OrthoView() { return (Mat4x3 *)own::R(0x2556560); }
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }

	static const uint32_t ORIG_MasterTask = 0x6F6890;
	static const uint32_t ORIG_FlameSpawner = 0x6F6C00;
	static const uint32_t ORIG_FlameTask = 0x6F6DA0;
	static const uint32_t ORIG_SparkTask = 0x6F7130;
	static const uint32_t ORIG_EmberTask = 0x6F7230;
	static const uint32_t ORIG_MainTask = 0x6F7310;
	static const uint32_t ORIG_TrailTask = 0x6F82D0;
	static const uint32_t ORIG_LayoutTask = 0x6F9AA0;
	static const uint32_t ORIG_RockTask = 0x6F9B60;
	static const uint32_t ORIG_SparkleTask = 0x6F9C60;

	static const uint32_t ORIG_Setup = 0x6F6800;
	static const uint32_t TIM = 0x1470DC4, CAMERA_ANIM = 0x145D2BC;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_FLAMES = 1, PART_GHOSTS = 2, PART_RISE = 3, PART_BONE = 4, PART_VORTEX = 5, PART_TARGETS = 6, PART_BURN = 7,
	             PART_DROP = 8, PART_END = 9, PART_WHITE = 10, PART_TRAIL = 11, PART_ROCKS = 12, PART_SPARKLES = 13 };
	struct BloodfestParams
	{
		struct { uint32_t start, ghost, lift, fall; } sounds;     // the four sounds
		// the main task's ticks: the sounds (the fall sound while the targets rise back), its last (it ends once the
		// battle files' preload is done)
		struct { int16_t start, ghost, lift, fall, end; } timing;
		struct { int16_t level, rise, fall; } flash;              // the screen flash: its level, rising over, falling over the last
		// the flames on each target (Fire's parts): how many, their frames, embers and sparks from each, their size (+ a
		// random below size_rand), the main task's tick they are all dropped
		struct { int16_t count, frames, embers, sparks, size, size_rand, drop; } flames;
		// the afterimages of the entity after the caster: two windows (start, ticks, recorded below rec), their brightness
		// and its step from the newer to the older
		struct { int16_t start_a, ticks_a, rec_a, start_b, ticks_b, rec_b, bright, fade; } ghosts;
		struct { int16_t start, ticks; } rise;                     // the rising layout at the caster's effect point
		// the bone model at the caster's bone: from start for ticks (fading in over them), its spin a tick
		struct { int16_t start, ticks, bone, spin; } bone;
		// the vortex at the targets' centre (the stage captured at its first tick): from start for ticks, a second one
		// from its tick second; growing in over grow ticks; fading out from fade_from over fade_ticks; their yaws, the
		// second's roll, the texture scroll a tick
		struct { int16_t start, ticks, second, grow, fade_from, fade_ticks, yaw_a, yaw_b, roll, scroll; } vortex;
		// the lift: the targets raised (a sine, then a cubic fall) from start for ticks, spinning; the burning layout's
		// scrolls, its second scroll from its tick scroll_from; the burst of sparkles and rocks on its first ticks (count
		// a tick each, on a ring of radius + a random below 0x800, rising at rise + a random below 0x80)
		struct { int16_t start, ticks, spin, scroll, scroll_from, burst_ticks, sparkles, rocks, radius, rise; } lift;
		// the drop: from start for ticks the targets fall from 0x2000 above; rocks a tick (their life, height, speed);
		// the drop layout's depth
		struct { int16_t start, ticks, rocks, rock_life, rock_height, rock_speed, depth; } drop;
		// then the targets put back (lowered), raised a step a tick for ticks; the end layout on each and the results
		struct { int16_t ticks, step, lower; } raise;
		// the white flash: its first tick, in / hold / out ticks, colour
		struct { int16_t start, in, hold, out, r, g, b; } white;
		// the blade trail (slot 4's bone 1): from start for ticks (shorter from shorten), its length, its colour (0xBBGGRR)
		struct { int16_t start, ticks, shorten, length; int32_t colour; } trail;
		struct { int16_t gravity; } rocks;                         // the rocks' fall speed's growth a tick
		struct { int16_t frames; } sparkles;                       // a sparkle's frames
		struct { uint32_t rise, burn, drop, end; } layouts;        // the prim-model layouts played
		struct { uint32_t bone, part, ring, rock; } models;        // the meshes drawn
		struct { uint32_t flame, glow, ember, sparkle; } seqs;     // the sprite sequences drawn (flipbooks)
		struct { uint32_t tim, camera; } res;                     // its resources (data): the TIM its setup uploads, the camera track
	};
	static const BloodfestParams GAME = { { 0x145D4DC, 0x145D4E0, 0x145D4E4, 0x145D4E8 }, { 0, 0x37, 0x73, 0xF1, 0xF3 },
		{ 2300, 12, 12 }, { 4, 0x10, 4, 10, 0xD00, 0x600, 0x21 }, { 0x37, 7, 5, 0x48, 0x1C, 0x1A, 0x800, 0x400 },
		{ 0x40, 0xF }, { 0x41, 8, 0x25, 0x100 }, { 0x55, 0x1E, 0xB, 8, 0x16, 8, 0x800, -0x400, 0x200, 0x20 },
		{ 0x73, 0x50, 0x100, 0x10, 0x1A, 4, 8, 8, 0x800, 0x80 }, { 0xC3, 0x28, 2, 0xA, 0x800, 0x100, 0x100 },
		{ 8, 0x200, 0x1000 }, { 0xE3, 6, 4, 6, 0xF0, 0xF0, 0xF0 }, { 0x37, 0x3B, 0x35, 777, 0x185820 }, { 0x14 }, { 0x10 },
		{ 0x146D8A4, 0x1462010, 0x146A668, 0x146ED2C }, { 0x145D6AC, 0x145E1C4, 0x14603F4, 0x1461D98 },
		{ 0x14708C0, 0x1470A6C, 0x1470C18, 0x145D4EC }, { TIM, CAMERA_ANIM } };
	static BloodfestParams live = GAME;
	static const BloodfestParams &PG() { return live; }

	static const uint32_t STREAM_STATE = 0xA708;       // magic buffer offsets
	static const uint32_t POOL_Main = 0xA750;
	static const uint32_t POOL_Sparkles = 0x153C;
	static const uint32_t POOL_Rocks = 0x183C;
	static const uint32_t POOL_Trail = 0x1BBC;
	static const uint32_t STREAM_B = 0x1F08;           // the end loads 0x168..0x16A
	static const uint32_t MORPH = 0xAB10;
	static const uint32_t ARENA_A = 0xB1B8;            // packet arenas (the stream load 0x2C0 lands in A)
	static const uint32_t ARENA_B = 0x1B1B8;
	static const uint32_t ARENA_C = 0x2B1B8;           // capture cursor on parity-0 ticks
	static const uint32_t LAYOUT_Rise = 0x146D8A4;     // prim-model layouts
	static const uint32_t LAYOUT_Burn = 0x1462010;
	static const uint32_t LAYOUT_Drop = 0x146A668;
	static const uint32_t LAYOUT_End = 0x146ED2C;
	static const uint32_t KIND_Plain = 0x14708AC;      // per-object kind tables
	static const uint32_t KIND_Burn = 0x14913E4;
	static const uint32_t MODEL_Bone = 0x145D6AC;      // prim models
	static const uint32_t MODEL_Rock = 0x1461D98;
	static const uint32_t MODEL_Part = 0x145E1C4;      // three-part draw: prim set, ring, quads (+ its vertices 0x145E1CC)
	static const uint32_t MODEL_Ring = 0x14603F4;
	static const uint32_t MODEL_PartVerts = 0x145E1CC;
	static const uint32_t SEQ_Flame = 0x14708C0;       // sprite sequences
	static const uint32_t SEQ_FlameGlow = 0x1470A6C;
	static const uint32_t SEQ_Ember = 0x1470C18;
	static const uint32_t SEQ_Sparkle = 0x145D4EC;
	static const void *const SOUND_Start = (const void *)0x145D4DC;
	static const void *const SOUND_Ghost = (const void *)0x145D4E0;
	static const void *const SOUND_Lift = (const void *)0x145D4E4;
	static const void *const SOUND_Fall = (const void *)0x145D4E8;
	static uint8_t *const TRAIL_ENTITY = (uint8_t *)0x1D97530;         // slot 4, hard-wired

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }   // Effect_RotationYMatrix
	inline void RotationX(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(a, out); }   // MAG_063_RotationXMatrix
	inline void RotationZ(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(a, out); }   // MAG_065_RotationZMatrix
	inline void ScaledRotY(int32_t a, int32_t s, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, int32_t, Mat4x3 *)>(0x6ED1E0)(a, s, out); } // GF_338Moomba_ScaledRotY
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a x b
	inline void MatrixMultiply3x3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }      // IR = R * IR
	inline void GteLoadIRFromColumn(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E180)(p); }
	inline void GteStoreIRToColumn(void *p) { fn<void (__cdecl *)(void *)>(0x45E470)(p); }
	inline void GteLoadV0FromDwords(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E060)(p); }
	inline void GteLoadIR123FromBytes(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(p); }
	inline void GteStoreIR123LowBytes(void *p) { fn<void (__cdecl *)(void *)>(0x45E410)(p); }
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline uint32_t SetDrawEnv(void *prim, uint32_t env) { return fn<uint32_t (__cdecl *)(void *, uint32_t)>(0x45C0F0)(prim, env); } // SSIGPU_SetDrawEnv
	inline void SetDrawAreaPrim(void *prim, const int16_t *rect) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C940)(prim, rect); }
	inline void SetDrawOffsetPrim(void *prim, const int16_t *xy) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C9B0)(prim, xy); }
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }  // Call_Bs_ParseCamera
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline uint32_t FadeTileInHoldOut(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x700150)(t, r, g, b, in, hold, out, cursor); } // Effect_FadeTileInHoldOut
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }       // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); }
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
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }
	inline int32_t StreamPreloadPending(int32_t n) { return fn<int32_t (__cdecl *)(int32_t)>(0x534300)(n); } // au_re_BattleFile_preLoad_1
	// other modules' code this module calls
	inline void GhostInit(uint8_t *g, int32_t n, int32_t bone) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t)>(0x6A1230)(g, n, bone); } // Burning Rave's afterimage init
	inline void GhostVertices(uint8_t **cur, uint8_t **out, void *ctx) { fn<void (__cdecl *)(uint8_t **, uint8_t **, void *)>(0x6A2A90)(cur, out, ctx); } // Burning Rave's
	inline uint32_t GhostPolys(void *ctx, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x68AAE0)(ctx, ot, shift, cursor); } // Duel's
	inline void ModelBox(uint32_t model, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x69A700)(model, out); }
	inline void RingPoint(int32_t angle, int32_t radius, int16_t *out) { fn<void (__cdecl *)(int32_t, int32_t, int16_t *)>(0x67BB70)(angle, radius, out); } // x = sin r, z = -cos r
	inline void StageGroupsBit1Off() { fn<void (__cdecl *)()>(0x6E2BB0)(); } // MAG_065_GreatAttractor_ModelsFlag2Off

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode { TaskNode hdr; int16_t counter; uint8_t running, parity; };                    // 0x10
	struct SpawnerNode { TaskNode hdr; int16_t counter, w0E; uint8_t pad10[0x10]; int16_t slot, w22; }; // 0x24
	struct FlameNode { TaskNode hdr; int16_t frame, delay; Flame f; };      // 0x24
	struct SparkNode { TaskNode hdr; int16_t life, delay; FireSpark s; };   // 0x24
	struct EmberNode { TaskNode hdr; int16_t frame, delay; Ember e; };      // 0x24
	struct TargetRec // 0x18
	{
		uint8_t *e;          // +0x00
		uint32_t pos01;      // +0x04 its +0x1C / +0x1E at the start
		uint32_t pos23;      // +0x08 +0x20 / +0x22
		int16_t ang[4];      // +0x0C its spin (from the zero angles 0x2556520)
		int16_t height[2];   // +0x14 its model's extent (0x6F9800 at 0x73): max, min
	};
	struct MainNode // pool of 1 node of 0x3C0 bytes (magic buffer + 0xA750), cleared at the start
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C
		int16_t ntargets;    // +0x0E
		TargetRec recs[3];   // +0x10
		int16_t centre[4];   // +0x58 the box centre x, y, z
		uint8_t ghosts[0x108]; // +0x60 the afterimage block (Burning Rave's, 1 bone, 0x40-byte slots)
		uint8_t layout[0x258]; // +0x168
	};
	struct RingEntry { uint32_t alive; int16_t base[4]; int16_t tip[4]; }; // 0x14
	struct TrailNode // 0x34C
	{
		TaskNode hdr;
		int16_t counter, pad0E;
		RingEntry ring[7];
		int16_t pts_a[8][4], pts_b[8][4];
		int16_t spl_a[32][4], spl_b[32][4];
		int16_t first_b[4], first_a[4];
		int32_t dir_b[4], dir_ab[4];
	};
	struct TrailWork { uint8_t pad00[8]; int32_t otz; uint32_t flag, col0, col1; uint8_t pad18[4]; uint32_t base_col; int16_t base[4], tip[4]; int32_t dir[4]; };
	struct LayoutNode { TaskNode hdr; int16_t pos[4]; uint8_t layout[0x700]; };   // 0x714
	struct RockNode // 0x1C
	{
		TaskNode hdr;
		int16_t pos[3];      // +0x0C
		int16_t vy;          // +0x12
		int16_t life;        // +0x14 (0 = until the pool is dropped)
		int8_t ax, ay;       // +0x16 angles (x 16)
		int8_t dax, day;     // +0x18
		int16_t scale;       // +0x1A
	};
	struct SparkleNode { TaskNode hdr; int16_t pos[3]; int16_t frame; int16_t vy; int16_t pad16; }; // 0x18
	struct PlayArg // the prim-player callback's block (0x30)
	{
		Mat4x3 m;            // +0x00
		uint32_t kind;       // +0x20
		int16_t depth;       // +0x24
		uint16_t scroll;     // +0x26 (low byte & 0x7F: kinds 0x31 / 0x32)
		uint16_t w28;        // +0x28 (low byte & 0x7F: kind 0x33)
		uint16_t pad2A;
		uint32_t morph;      // +0x2C
	};
	// the three-part draw's quad renderer header (0x98, 0x6F8F20)
	struct QuadHeader
	{
		uint32_t data;       // +0x00
		uint32_t verts;      // +0x04
		uint8_t pad08[0xA];
		uint16_t vscroll;    // +0x12
		uint16_t win_a[4];   // +0x14 texture window A (offset x / y, size x / y)
		uint16_t win_b[4];   // +0x1C texture window B
		uint32_t list;       // +0x24 the quad list cursor
		uint8_t pad28[8];
		int32_t otz;         // +0x30
		uint32_t flag;       // +0x34
		int32_t depth;       // +0x38
		uint32_t v[4][2];    // +0x3C the quad's vertices
		uint32_t col[4];     // +0x5C
		uint32_t uv01[2];    // +0x6C
		uint16_t uv2, uv3;   // +0x74
		uint8_t pad78[0x20];
	};
#pragma pack(pop)
	static_assert(sizeof(MainNode) == 0x3C0 && sizeof(TargetRec) == 0x18 && sizeof(TrailNode) == 0x34C && sizeof(LayoutNode) == 0x714, "Bloodfest nodes");
	static_assert(sizeof(RockNode) == 0x1C && sizeof(SparkleNode) == 0x18 && sizeof(PlayArg) == 0x30 && sizeof(QuadHeader) == 0x98 && sizeof(TrailWork) == 0x40, "Bloodfest blocks");

	static const FlameRolls FLAME_ROLLS = { 6, 2, 2800, 1000, 0x226, { -500, 300, -1 }, { 0xD00, 0x600, 1 } };
	static const FlameLook FLAME_LOOK = { SEQ_Flame, SEQ_FlameGlow, 4, 0x400, 0xC, -2000, 160, 0xA0 };
	static const EmberRolls EMBER_ROLLS = { 6, 1, { -150, 300, 1 }, { 0, 100, -1 }, { -150, 300, 1 }, { -20, 30, -1 }, { 0xA00, 0x500, 1 } };
	static const SparkRolls SPARK_ROLLS = { { 5, 4, 1 }, 4, 1, { -200, 400, 1 }, { 0, 200, -1 }, { -200, 400, 1 }, { -40, 80, 1 }, { -20, 50, -1 }, { -40, 80, 1 }, { 0x900, 0x600, 1 } };
	static const SparkLook SPARK_LOOK = { 5, 9, 4, 0x10, 3 };
}
}

#ifdef FF8_FX_HELD
#include "mag210_bloodfest_held.h"
#endif

namespace ff8fx
{
namespace blood210
{
	using namespace eng;
	using namespace magc;

	inline int32_t SlotOf(uint8_t *e) { return (int32_t)((e - (uint8_t *)0x1D972C0) / 0x9C); }

	// 0x6F6BB0: the flame spawner on an entity (251's 0x6F4840)
	static void StartFlameSpawner(uint8_t *e)
	{
		SpawnerNode *s = (SpawnerNode *)AddTaskToQueue(QFlames(), own::Fn(ORIG_FlameSpawner));
		if (!s) return;
		s->counter = 0;
		s->w0E = 0;
		s->w22 = 0;
		s->slot = (int16_t)SlotOf(e);
	}

	// ------------------------------------------------------------------
	// MasterTask (0x6F6890)
	// ------------------------------------------------------------------
	static void MasterSetup()
	{
		StreamStateInit((void *)(TexBase() + STREAM_STATE));
		InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x3C0, 1);
		InitTaskQueuePool(QFlames(), (void *)TexBase(), 0x24, 0x96);
		MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
		Memset32(&m->counter, 0, 0xED);
		int16_t minx = 0x7FFF, miny = 0x7FFF, minz = 0x7FFF, maxx = -0x7FFF, maxy = -0x7FFF, maxz = -0x7FFF;
		const ActionData *ad = Ctx()->actions;
		m->ntargets = (int16_t)ad->target_count;
		for (int32_t k = 0; k < m->ntargets; k++)
		{
			TargetRec *r = &m->recs[k];
			uint8_t *e = Entity(Ctx()->actions->targets[k * TARGET_STRIDE]);
			r->e = e;
			r->pos01 = *(const uint32_t *)(e + 0x1C);
			r->pos23 = *(const uint32_t *)(e + 0x20);
			memcpy(r->ang, ZeroAngles(), 8);
			int16_t p[4];
			GetDefaultEffectPosition(e, p);
			if (p[0] < minx) minx = p[0];
			if (p[0] > maxx) maxx = p[0];
			if (p[1] < miny) miny = p[1];
			if (p[1] > maxy) maxy = p[1];
			if (p[2] < minz) minz = p[2];
			if (p[2] > maxz) maxz = p[2];
			StartFlameSpawner(e);
		}
		Centre()[0] = m->centre[0] = (int16_t)((maxx + minx) / 2);
		Centre()[1] = m->centre[1] = (int16_t)((maxy + miny) / 2);
		Centre()[2] = m->centre[2] = (int16_t)((maxz + minz) / 2);
		CasterPos()[0] = *(const uint32_t *)(Caster() + 0x1C);
		CasterPos()[1] = *(const uint32_t *)(Caster() + 0x20);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *ms = (MasterNode *)n;
		if (ms->parity)
		{
			PacketCursor() = TexBase() + ARENA_A;
			CaptureCursor() = TexBase() + ARENA_B;
			ms->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			CaptureCursor() = TexBase() + ARENA_C;
			ms->parity = 1;
		}
		FlashLevel() = 0;
		if (ms->counter == 1 && Pause() == 0 && !ms->running)
		{
			ms->running = 1;
			MasterSetup();
		}
		int main_alive = 1, tasks_alive = 1;
		if (ms->running)
		{
			// 30 fps layer: see mag210_bloodfest_held.inc
			FX_HELD(held_note_master();)
			EffectCameraMatrix(&Camera(), OrthoView());
			main_alive = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QFlames());
			ExecuteTaskQueue(QSparkles());
			ExecuteTaskQueue(QRocks());
			ExecuteTaskQueue(QTrail());
			tasks_alive = ExecuteTaskQueue(QLayouts());
		}
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (Pause() != 0) return 0;
		if (ms->running && main_alive == 0 && tasks_alive == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		ms->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// FlameSpawner (0x6F6C00), Flame (0x6F6DA0), Spark (0x6F7130), Ember (0x6F7230): 251's (Fire's parts)
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

	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		FlameNode *p = (FlameNode *)n;
		if (p->delay > 0)
		{
			if (Pause() != 0) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag210_bloodfest_held.inc
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
		// 30 fps layer: see mag210_bloodfest_held.inc
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
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_draw(ORIG_EmberTask, p);)
		FX_PART(PART_FLAMES);
		EmberDraw(&p->e, p->frame, fxp::sequence(PG().seqs.ember, SEQ_Ember), 3, PacketCursor());
		if (Pause() != 0) return 0;
		EmberRise(&p->e, 5, 6);
		p->frame++;
		return p->frame >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6F8850): one object of a layout (0x701DD0)
	// ------------------------------------------------------------------
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
		RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		if (r->flags & 0x1000)
		{
			GteSetRotMatrixCtrl(OrthoView());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t t[3];
			GteReadMAC123(t);
			m.t[0] = t[0];
			m.t[1] = t[1];
			m.t[2] = t[2];
			MatrixMultiply(OrthoView(), &m);
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				Mat4x3 s;
				s.m[1][1] = r->scale[1];
				s.m[0][0] = r->scale[0];
				s.m[1][2] = 0;
				s.m[0][2] = 0;
				s.m[2][1] = 0;
				s.m[0][1] = 0;
				s.m[2][0] = 0;
				s.m[1][0] = 0;
				s.m[2][2] = r->scale[2];
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
		*(uint32_t *)(h + 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		if (r->flags & 0x2000) *(uint8_t *)(h + 0x1C) |= 0xC;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint8_t *)(h + 0x1C) |= 0xC0;
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
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x22) = (uint16_t)(arg->scroll & 0x7F);
		}
		else if (kind == 0x32)
		{
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0x80;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x22) = (uint16_t)(arg->scroll & 0x7F);
		}
		else if (kind == 0x33)
		{
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0;
			*(uint16_t *)(h + 0x32) = 0x100;
			*(uint16_t *)(h + 0x30) = 0x80;
			*(uint16_t *)(h + 0x20) = (uint16_t)(arg->w28 & 0x7F);
		}
		else if (kind == 0x39) *(int32_t *)(h + 0x18) = 0x80;
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// one layout of the main task through the prim player (0x701970), the callback 0x6F8850: a yaw-0 frame at x, 0, z
	static void Play(MainNode *mn, int site, uint8_t *layout, PlayArg *arg, int32_t x, int32_t z)
	{
		RotationY(0, &arg->m);
		arg->m.t[0] = x;
		arg->m.t[1] = 0;
		arg->m.t[2] = z;
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		arg->morph = TexBase() + MORPH;
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_layout(mn, site, layout, arg, x, z);)
		prim::play((prim::Layout *)layout, PrimCallback, (int)arg, (int)Pause());
	}

	// ------------------------------------------------------------------
	// Afterimages (Burning Rave's / Duel's code with 0x40-byte slots: 1 bone)
	// ------------------------------------------------------------------
	// 0x6F8200: the recorded bones' matrices of the entity's model into the next ghost slot (274's 0x68ADF0)
	static void GhostRecord(uint8_t *entity, uint8_t *g)
	{
		g[0] = 1;
		const uint8_t s = g[2];
		g[3] = s;
		uint8_t *dst = g + 0x88 + (uint32_t)s * 0x40;
		g[2] = (uint8_t)(s + 1);
		if ((uint8_t)(s + 1) >= 2) g[2] = 0;
		g[1] = (uint8_t)(g[1] + 1);
		if (g[1] > 2) g[1] = 2;
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		for (uint32_t b = 0; b < skel[0]; b++)
		{
			const int32_t k = (int8_t)g[5 + b];
			if (k >= 0) memcpy(dst + k * 0x20, skel + 0x20 + b * 0x30, 0x20);
		}
	}

	// the afterimage draw context (the Field_Alloc block of 0x74 bytes from +0x20; Burning Rave's GhostCtx)
#pragma pack(push, 1)
	struct GhostCtx
	{
		uint32_t polys;      // +0x00
		uint32_t verts;      // +0x04
		int16_t ntri, nquad; // +0x08
		int16_t clip[4];     // +0x0C
		uint32_t colour;     // +0x14
		uint32_t visible;    // +0x18
		uint32_t v[8];       // +0x1C
		int32_t mac0;        // +0x3C
		uint32_t pad40;
		int32_t otz;         // +0x44
		uint8_t pad48[4];
		uint8_t *ghosts;     // +0x4C
		uint32_t slot;       // +0x50
	};
#pragma pack(pop)
	static_assert(sizeof(GhostCtx) == 0x54, "afterimage context");

	// 0x6F80B0: the model's visible objects with the ghost's bone matrices (slot + (k + 1) x 0x20), into the packet cursor
	static void GhostModel(uint8_t *model, GhostCtx *c, uint32_t ot, int32_t shift)
	{
		const uint8_t *geom = *(uint8_t **)(model + 4);
		const int32_t nobj = *(const int32_t *)geom;
		const uint32_t *offs = (const uint32_t *)(geom + 4);
		for (int32_t i = 0; i < nobj; i++)
		{
			uint8_t *obj = (uint8_t *)(offs[i] + *(uint32_t *)(model + 4));
			if (!(c->visible & (1u << i))) continue;
			uint8_t *out = (uint8_t *)c->verts;
			uint8_t *cur = obj + 2;
			int32_t groups = *(const int16_t *)obj;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)cur;
				cur += 2;
				const int32_t k = (int8_t)c->ghosts[bone + 5];
				if (k >= 0)
				{
					const Mat4x3 *m = (const Mat4x3 *)(c->slot + (uint32_t)(k + 1) * 0x20);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					GhostVertices(&cur, &out, c);
				}
				else
				{
					const int32_t nv = *(const int16_t *)cur;
					cur += nv * 6 + 2;
					for (int32_t v = nv; v != 0; v--)
					{
						*(uint16_t *)(out + 6) = 0x10;
						out += 8;
					}
				}
			}
			cur = (uint8_t *)(((uint32_t)cur + 3) & ~3u);
			c->ntri = *(const int16_t *)cur;
			cur += 2;
			c->nquad = *(const int16_t *)cur;
			cur += 0xA;
			c->polys = (uint32_t)cur;
			PacketCursor() = GhostPolys(c, ot, shift, PacketCursor());
		}
	}

	// 0x6F7EE0: the recorded ghosts (up to 2, the newer one first, brighter) of the entity's model; the count drops by one
	// on an unpaused tick without a recording
	static void GhostDraw(uint8_t *entity, uint8_t *g)
	{
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_ghosts(entity, g);)
		FX_PART(PART_GHOSTS);
		uint8_t *h = (uint8_t *)FieldAlloc(0x74);
		GhostCtx *c = (GhostCtx *)(h + 0x20);
		c->verts = var<uint32_t>(0x1D98B3C);
		c->clip[0] = 0;
		c->clip[1] = 0;
		c->clip[2] = 0x140;
		c->clip[3] = 0xD8;
		c->visible = *(const uint32_t *)(entity + 0x7C);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)h);
		c->ghosts = g;
		int32_t w = PG().ghosts.bright;
		for (int32_t i = 0; i < g[1]; i++)
		{
			GteSetIR0((w >> 1) + w);
			GteLoadIR123FromBytes(entity + 0x28);
			GteGPF();
			GteStoreIR123LowBytes(&c->colour);
			*((uint8_t *)c + 0x17) = 0;
			c->slot = (uint32_t)g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x40;
			for (int32_t b = 0; b < g[4]; b++)
			{
				const uint32_t r = c->slot + (uint32_t)b * 0x20;
				GteSetRotMatrixCtrl((const Mat4x3 *)h);
				GteLoadIRFromColumn((const void *)(r + 0));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x20));
				GteLoadIRFromColumn((const void *)(r + 2));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x22));
				GteLoadIRFromColumn((const void *)(r + 4));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x24));
				GteSetTransVectorCtrl((const Mat4x3 *)h);
				GteLoadV0FromDwords((const void *)(r + 0x14));
				GteMVMVA_RotV0Tr();
				GteReadMAC123((int32_t *)(r + 0x34));
			}
			GhostModel(*(uint8_t **)(entity + 0x64), c, OTBase() + 0x44, 0);
			w -= PG().ghosts.fade;
		}
		FieldFree(0x74);
		if (Pause() != 0) return;
		if (g[0] == 0 && g[1] != 0) g[1]--;
		g[0] = 0;
	}

	// ------------------------------------------------------------------
	// 0x6F9800: a model's vertical extent in its bone space -> out[0] = max, out[1] = min (251's 0x6F6690)
	// ------------------------------------------------------------------
	static void MeasureHeight(uint8_t *e, int16_t *out)
	{
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
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

	// ------------------------------------------------------------------
	// 0x6F9960: an entity drawn by the module (Burning Rave's 0x6A2F60 without the shadow): its bones' world matrices from
	// its world matrix x (rotation `angles`, translation `off` minus the rotated `off`) under the view, RenderGeometry into
	// the frame arena, its pose matrices rebuilt; the same for its weapon (+0x78)
	// ------------------------------------------------------------------
	static void DrawEntity(uint8_t *e, const int16_t *angles, const int16_t *off)
	{
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_entity(e, angles, off);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		ComposeZYXRotationMatrix(angles, m);
		m->t[0] = off[0];
		m->t[1] = off[1];
		m->t[2] = off[2];
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), m, m);
		ComposeAffineTransform(&Camera(), m, m);
		Apply3DTransform(m, off, (int32_t *)(h + 0x20));
		m->t[1] = (int32_t)((uint32_t)m->t[1] - *(const uint32_t *)(h + 0x24));
		m->t[0] = (int32_t)((uint32_t)m->t[0] - *(const uint32_t *)(h + 0x20));
		m->t[2] = (int32_t)((uint32_t)m->t[2] - *(const uint32_t *)(h + 0x28));
		ComputeBonesWorldMatrices(e + 0x60, m);
		*(uint32_t *)(h + 0x34) = var<uint32_t>(0x1D98B3C);
		*(uint16_t *)(h + 0x44) = 0;
		*(uint16_t *)(h + 0x46) = 0;
		*(uint16_t *)(h + 0x48) = 0x140;
		*(uint16_t *)(h + 0x4A) = 0xD8;
		*(uint32_t *)(h + 0x4C) = *(const uint32_t *)(e + 0x28);
		*(uint32_t *)(h + 0x58) = 0;
		*(uint32_t *)(h + 0x50) = *(const uint32_t *)(e + 0x7C);
		*(uint32_t *)(h + 0x40) = var<uint32_t>(0x1D969A8);
		FramePackets() = RenderGeometry(*(const uint32_t *)(e + 0x64), h + 0x30, OTBase() + 0x44, 4, FramePackets());
		BuildBoneMatricesFromPose(e + 0x60);
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (w)
		{
			*(uint32_t *)(h + 0x50) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(w, m);
			FramePackets() = RenderGeometry(*(const uint32_t *)(w + 4), h + 0x30, OTBase() + 0x44, 4, FramePackets());
			BuildBoneMatricesFromPose(w);
		}
		FieldFree(0x5C);
	}

	// ------------------------------------------------------------------
	// 0x6F9440: the stage captured into VRAM (0x2C0, 0x100, 0xC0 x 0x100) over the box `b` (Burning Rave's 0x6A12B0 with
	// a 0xC0-wide rectangle, a yaw-0 view and the flash at 0x8FC): the stage geometry 0x1D989C0 seen from above, scaled into
	// the rectangle; then the drawing area / offset packets of the rectangle
	// ------------------------------------------------------------------
	static void CaptureStage(const int16_t *a, const int16_t *b)
	{
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_capture(a, b);)
		FX_PART(PART_VORTEX);
		const int32_t b0 = -(int32_t)b[0];
		const int32_t b3 = b[3];
		const int32_t nb2 = -(int32_t)b[2];
		const int32_t b5 = b[5];
		const int32_t w = b3 + b0;
		const int32_t dz = b5 + nb2;
		SetScreenFlash(0x8FC, 0);
		int16_t rect[4] = { 0x2C0, 0x100, 0xC0, 0x100 };
		// (the round-up tests the quotient's remainder, not the extent's)
		int32_t sx = w / 0xC0;
		int32_t sz = dz / 0x100;
		if (sx % 0xC0 != 0) sx++;
		if (sz % 0x100 != 0) sz++;
		Mat4x3 m, m2;
		m.t[1] = 0;
		m.t[0] = -((int32_t)a[0] / sx);
		m.t[2] = (int32_t)a[2] / sz;
		RotationY(0, &m);
		TransformVectorBy3x3Matrix(&m, m.t, m.t);
		RotationX(-0x400, &m2);
		MatrixMultiply3x3(&m2, &m);
		int32_t s3[3];
		s3[1] = 0;
		m2.t[0] = (b0 - b3) / 2 / sx + m.t[0];
		m2.t[2] = (int32_t)var<int16_t>(0x1D8E038);
		m2.t[1] = m.t[2] - ((nb2 - b5) / 2 / sz);
		s3[0] = 0x1000000 / (shl32(w, 12) / rect[2]);
		s3[2] = 0x1000000 / (shl32(dz, 12) / rect[3]);
		Scale3DMatrix(&m2, s3);
		uint8_t *env = (uint8_t *)PacketCursor();
		PacketCursor() += 0x40;
		SetDrawEnv(env, 0x1D969C8 + (uint32_t)(var<uint8_t>(0x1D96A80) ^ 1) * 0x5C);
		InsertPrimAutoDepth(OTBase() + 0x44, env);
		int32_t ox, oy;
		GetScreenOffset(&ox, &oy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		GteSetRotMatrix(&m2);
		GteSetTransVector(&m2);
		if (var<uint8_t>(0x1D989BD) & 1)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
			*(uint16_t *)(h + 0x14) = 0;
			*(uint16_t *)(h + 0x16) = 0;
			*(uint16_t *)(h + 0x18) = 0x140;
			*(uint16_t *)(h + 0x1A) = 0xD8;
			*(uint16_t *)(h + 0x24) = var<uint16_t>(0x1D989BE);
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(0x1D989E4);
			*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
			*(uint32_t *)(h + 0x20) = 0xFFFFFFFF;
			*(uint32_t *)(h + 0x28) = 0;
			*(uint32_t *)(h + 0x10) = CaptureCursor();
			BuildBoneMatricesFromPose((void *)0x1D989D0);
			ComputeBonesWorldMatrices((void *)0x1D989D0, &m2);
			PacketCursor() = RenderGeometry(0x1D989C0, h, OTBase() + 0x44, 0x10, PacketCursor());
			FieldFree(0x2C);
		}
		uint8_t *q = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		*(uint16_t *)(q + 8) = 0;
		*(uint16_t *)(q + 0xA) = 0;
		*(uint16_t *)(q + 0xC) = 0xC0;
		*(uint16_t *)(q + 0xE) = 0;
		*(uint16_t *)(q + 0x10) = 0;
		*(uint16_t *)(q + 0x12) = 0x100;
		*(uint16_t *)(q + 0x14) = 0xC0;
		*(uint16_t *)(q + 0x16) = 0x100;
		q[4] = 0;
		q[5] = 0;
		q[6] = 0;
		*(uint32_t *)q = 0x05000000;
		q[7] = 0x28;
		InsertPrimAltViewport(OTBase() + 0x44, q);
		SetScreenOffset(ox, oy);
		uint8_t *area = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawAreaPrim(area, rect);
		InsertPrimAutoDepth(OTBase() + 0x44, area);
		int16_t xy[2] = { rect[0], rect[1] };
		uint8_t *offp = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawOffsetPrim(offp, xy);
		InsertPrimAutoDepth(OTBase() + 0x44, offp);
	}

	// ------------------------------------------------------------------
	// 0x6F8EF0 / 0x6F8F20: textured gouraud quads of a model (its list after the vertex block): vertex colours faded by
	// depth (+0x38 + z, x 16 against 4096), v coordinates scrolled (+0x12, wrapped by +0x20's low byte), screen-clipped
	// (all 4 vertices off one side: dropped), each quad between texture windows A and B
	// ------------------------------------------------------------------
	static uint32_t TexWindow(const uint16_t *w)
	{
		uint32_t e = ((uint32_t)(uint8_t)w[1] & 0xF8) | 0xFFFE2000u;
		e = (e << 5) | ((uint32_t)(uint8_t)w[0] & 0xF8);
		e = (e << 5) | (~((uint32_t)w[3] - 1) & 0xF8);
		e = (e << 2) | (((int32_t)~((uint32_t)w[2] - 1) >> 3) & 0x1F);
		return e;
	}
	static void QuadColour(QuadHeader *h, int k, int32_t z, uint32_t col, uint32_t none)
	{
		const int32_t d = h->depth + z;
		if (d <= 0)
		{
			h->col[k] = none;
			return;
		}
		h->col[k] = col;
		const int32_t f = shl32(d, 4);
		if (f < 0x1000)
		{
			GteSetIR0(f);
			GteLoadIR123FromBytes(&h->col[k]);
			GteGPF();
			GteStoreIR123LowBytes(&h->col[k]);
		}
	}
	static uint32_t RenderQuads(QuadHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t *list = (const uint32_t *)h->list;
		int32_t n = (int32_t)list[0];
		const uint8_t *rec = (const uint8_t *)(list + 1);
		h->list = (uint32_t)rec;
		const uint8_t *verts = (const uint8_t *)h->verts;
		uint8_t *pk = (uint8_t *)cursor;
		for (; n > 0; n--, rec += 0x24)
		{
			for (int k = 0; k < 4; k++)
			{
				const uint8_t *v = verts + (uint32_t)*(const uint16_t *)(rec + 4 + 2 * k) * 4;
				h->v[k][0] = *(const uint32_t *)v;
				h->v[k][1] = *(const uint32_t *)(v + 4);
				const uint32_t col = k == 0 ? *(const uint32_t *)rec : *(const uint32_t *)(rec + 0x14 + 4 * k);
				QuadColour(h, k, *(const int16_t *)&h->v[k][1], col, k == 0 ? 0x3E000000u : 0u);
			}
			h->uv01[0] = *(const uint32_t *)(rec + 0xC);
			h->uv01[1] = *(const uint32_t *)(rec + 0x10);
			h->uv2 = *(const uint16_t *)(rec + 0x14);
			const uint32_t vs = h->vscroll;
			uint32_t v2 = (uint32_t)((const uint8_t *)&h->uv2)[1] + vs;
			uint32_t v0 = (uint32_t)((const uint8_t *)h->uv01)[1] + vs;
			h->uv3 = *(const uint16_t *)(rec + 0x16);
			uint32_t v3 = (uint32_t)((const uint8_t *)&h->uv3)[1] + vs;
			uint32_t v1 = (uint32_t)((const uint8_t *)&h->uv01[1])[1] + vs;
			if ((int32_t)(v3 | v2 | v1 | v0) > 0xFF)
			{
				const uint8_t wrap = (uint8_t)h->win_b[2];
				v0 = (uint8_t)(v0 - wrap);
				v1 = (uint8_t)(v1 - wrap);
				v2 = (uint8_t)(v2 - wrap);
				v3 = (uint8_t)(v3 - wrap);
			}
			((uint8_t *)h->uv01)[1] = (uint8_t)v0;
			((uint8_t *)&h->uv01[1])[1] = (uint8_t)v1;
			((uint8_t *)&h->uv2)[1] = (uint8_t)v2;
			((uint8_t *)&h->uv3)[1] = (uint8_t)v3;
			GteLoadV012(h->v[0], h->v[1], h->v[2]);
			GteRTPT();
			*(uint32_t *)pk = 0x0C000000;
			*(uint32_t *)(pk + 0xC) = h->uv01[0];
			*(uint32_t *)(pk + 0x18) = h->uv01[1];
			const uint32_t uv23 = (uint32_t)h->uv2 | ((uint32_t)h->uv3 << 16);
			*(uint32_t *)(pk + 0x24) = uv23;
			*(uint32_t *)(pk + 0x30) = uv23 >> 16;
			GteReadFLAG(&h->flag);
			if (h->flag & 0x60000) continue;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(h->v[3]);
			GteRTPS();
			uint32_t clip = 0;
			int16_t c = *(const int16_t *)(pk + 8);
			if (c < 0 || c > 0xA00) clip = 1;
			c = *(const int16_t *)(pk + 0x14);
			if (c < 0 || c > 0xA00) clip |= 2;
			c = *(const int16_t *)(pk + 0x20);
			if (c < 0 || c > 0xA00) clip |= 4;
			c = *(const int16_t *)(pk + 0xA);
			if (c < 0 || c > 0x6C0) clip |= 0x10;
			c = *(const int16_t *)(pk + 0x16);
			if (c < 0 || c > 0x6C0) clip |= 0x20;
			c = *(const int16_t *)(pk + 0x22);
			if (c < 0 || c > 0x6C0) clip |= 0x40;
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			c = *(const int16_t *)(pk + 0x2C);
			if (c < 0 || c > 0xA00) clip |= 8;
			c = *(const int16_t *)(pk + 0x2E);
			if (c < 0 || c > 0x6C0) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZ32(&h->otz);
			*(uint32_t *)(pk + 4) = h->col[0];
			*(uint32_t *)(pk + 0x10) = h->col[1];
			*(uint32_t *)(pk + 0x1C) = h->col[2];
			*(uint32_t *)(pk + 0x28) = h->col[3];
			const uint32_t bucket = ot + 4 * (uint32_t)(h->otz >> shift);
			uint8_t *poly = pk;
			uint8_t *wa = pk + 0x34;
			uint8_t *wb = pk + 0x40;
			pk += 0x4C;
			*(uint32_t *)wa = 0x02000000;
			*(uint32_t *)(wa + 4) = TexWindow(h->win_a);
			*(uint32_t *)(wa + 8) = 0;
			InsertPrimAutoDepth(bucket, wa);
			InsertPrimAutoDepth(bucket, poly);
			*(uint32_t *)wb = 0x02000000;
			*(uint32_t *)(wb + 4) = TexWindow(h->win_b);
			*(uint32_t *)(wb + 8) = 0;
			InsertPrimAutoDepth(bucket, wb);
		}
		h->list = (uint32_t)rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x6F8BF0: the three-part draw at the centre: with a fade (> 0) a prim set 0x145E1C4 (yaw a2, scaled 0x2000) only;
	// else a ring 0x14603F4 (below 0x1000: its yaw a2, pushed (0x800 - a4) x 2 out, rolled a3) and the quads of 0x145E1C4
	// (depth a4 - 0x800, v scroll a5)
	// ------------------------------------------------------------------
	static void ThreePart(const int16_t *c, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6)
	{
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_three(c, a2, a3, a4, a5, a6);)
		FX_PART(PART_VORTEX);
		const uint32_t part = fxp::model(PG().models.part, MODEL_Part);
		Mat4x3 m;
		if (a6 > 0)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
			ScaledRotY(a2, 0x2000, &m);
			m.t[1] = 0;
			m.t[0] = c[0];
			m.t[2] = c[2];
			ComposeAffineTransform(&Camera(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x20) = 0;
			*(uint16_t *)(h + 0x22) = (uint16_t)(a5 & 0x7F);
			*(uint32_t *)h = part;
			*(int32_t *)(h + 0xC) = a6;
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x18) = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
			FieldFree(0x6C);
			return;
		}
		if (a4 < 0x1000)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
			Mat4x3 *rm = (Mat4x3 *)(h + 0x6C);
			*(uint32_t *)(h + 0x1C) = 0xF0;
			RotationY(a2, rm);
			*(uint32_t *)(h + 8) = 0;
			int16_t *pv = (int16_t *)(h + 0xAC);
			pv[1] = 0;
			pv[0] = 0;
			pv[2] = (int16_t)((0x800 - a4) * 2);
			Apply3DTransform(rm, pv, rm->t);
			rm->t[0] = (int32_t)((uint32_t)rm->t[0] + (uint32_t)c[0]);
			rm->t[2] = (int32_t)((uint32_t)rm->t[2] + (uint32_t)c[2]);
			if (a3 != 0)
			{
				RotationZ(a3, (Mat4x3 *)(h + 0x8C));
				MatrixMultiply3x3(rm, h + 0x8C);
			}
			ComposeAffineTransform(&Camera(), rm, rm);
			GteSetRotMatrix(rm);
			GteSetTransVector(rm);
			*(uint32_t *)(h + 0x18) = 0;
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0;
			*(uint16_t *)(h + 0x32) = 0x100;
			*(uint16_t *)(h + 0x30) = 0x40;
			*(uint16_t *)(h + 0x20) = (uint16_t)(a5 & 0x3F);
			*(uint16_t *)(h + 0x22) = 0;
			*(uint32_t *)h = fxp::model(PG().models.ring, MODEL_Ring);
			*(int32_t *)(h + 0xC) = a4;
			PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
			FieldFree(0xB4);
		}
		QuadHeader *q = (QuadHeader *)FieldAlloc(0x98);
		ScaledRotY(a2, 0x2000, &m);
		m.t[1] = 0;
		m.t[0] = c[0];
		m.t[2] = c[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		q->depth = a4 - 0x800;
		q->win_a[1] = 0;
		q->win_a[0] = 0;
		q->win_a[3] = 0x100;
		q->win_a[2] = 0x100;
		q->win_b[0] = 0;
		q->win_b[1] = 0x80;
		q->win_b[2] = 0x100;
		q->win_b[3] = 0x80;
		q->vscroll = (uint16_t)(a5 & 0x7F);
		q->data = part;
		q->verts = part + (MODEL_PartVerts - MODEL_Part);
		q->list = part + *(const uint32_t *)part + 0x1C;
		PacketCursor() = RenderQuads(q, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}

	// ------------------------------------------------------------------
	// MainTask (0x6F7310)
	// ------------------------------------------------------------------
	// 8 sparkles and 8 rocks on a ring around the centre (0x73..0x76)
	static void SpawnBurst(const MainNode *mn)
	{
		const BloodfestParams &g = PG();
		int32_t count = g.lift.sparkles;
		SparkleNode *s = count > 0 ? (SparkleNode *)AddTaskToQueue(QSparkles(), own::Fn(ORIG_SparkleTask)) : nullptr;
		while (s)
		{
			s->frame = 0;
			const int32_t r = (CrtRand() & 0x7FF) + g.lift.radius;
			RingPoint(CrtRand(), r, s->pos);
			s->pos[0] = (int16_t)(s->pos[0] + mn->centre[0]);
			s->pos[2] = (int16_t)(s->pos[2] + mn->centre[2]);
			s->pos[1] = mn->centre[1];
			s->vy = (int16_t)(-g.lift.rise - (CrtRand() & 0x7F));
			if (--count == 0) break;
			s = (SparkleNode *)AddTaskToQueue(QSparkles(), own::Fn(ORIG_SparkleTask));
		}
		count = g.lift.rocks;
		RockNode *k = count > 0 ? (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask)) : nullptr;
		while (k)
		{
			k->life = 0;
			const int32_t r = (CrtRand() & 0x7FF) + g.lift.radius;
			RingPoint(CrtRand(), r, k->pos);
			k->pos[0] = (int16_t)(k->pos[0] + mn->centre[0]);
			k->pos[2] = (int16_t)(k->pos[2] + mn->centre[2]);
			k->pos[1] = mn->centre[1];
			k->vy = (int16_t)(-g.lift.rise - (CrtRand() & 0x7F));
			k->scale = (int16_t)((shl32(CrtRand(), 11) >> 15) + 0x800);
			k->ax = (int8_t)CrtRand();
			k->ay = (int8_t)CrtRand();
			k->dax = (int8_t)CrtRand();
			k->day = (int8_t)CrtRand();
			if (--count == 0) break;
			k = (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask));
		}
	}

	// the flash level at counter c (rising 0..0xB, 0x8FC, falling 0xE7..0xF2)
	static int32_t FlashOf(int32_t c)
	{
		const BloodfestParams &g = PG();
		if (c < g.flash.rise) return c * g.flash.level / g.flash.rise;
		if (c >= g.timing.end - g.flash.fall) return (g.timing.end - c) * g.flash.level / g.flash.fall;
		return g.flash.level;
	}

	// a target's height while lifted (0x73..0xC2): its start height minus a sine / cubic ramp of u
	static int16_t LiftHeight(int32_t u, int16_t y0)
	{
		if (u < 0x800) return (int16_t)(y0 - (int16_t)(ComputeSin(u >> 1) << 1));
		int32_t v = 0x1000 - ComputeSin(u >> 1);
		v = mul32(v, v) >> 12;
		v = mul32(v, v) >> 12;
		return (int16_t)((int16_t)(v + v - 0x1000) + y0);
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *mn = (MainNode *)n;
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_main(mn);)
		const BloodfestParams &g = PG();
		FlashLevel() = FlashOf(mn->counter);
		// 0x37..0x3D: afterimages of the next entity (the block and the trail at 0x37), recorded up to 0x3B
		if (mn->counter == 0x37) InitTaskQueuePool(QTrail(), (void *)(TexBase() + POOL_Trail), 0x34C, 1);
		if (mn->counter == g.trail.start && Pause() == 0)
		{
			TrailNode *t = (TrailNode *)AddTaskToQueue(QTrail(), own::Fn(ORIG_TrailTask));
			if (t)
			{
				t->counter = 0;
				for (int32_t k = 0; k < 7; k++) t->ring[k].alive = 0;
			}
		}
		uint32_t i = (uint32_t)(mn->counter - g.ghosts.start_a);
		if (i < (uint32_t)g.ghosts.ticks_a)
		{
			if (i == 0) GhostInit(mn->ghosts, 1, 1);
			GhostDraw(Caster() + 0x9C, mn->ghosts);
			if (i < (uint32_t)g.ghosts.rec_a && Pause() == 0) GhostRecord(Caster() + 0x9C, mn->ghosts);
		}
		PlayArg arg = {};
		// 0x40..0x4E: layout 0x146D8A4 at the caster's effect point
		i = (uint32_t)(mn->counter - g.rise.start);
		if (i < (uint32_t)g.rise.ticks)
		{
			if (i == 0) DecodeModelPrimLayout(fxp::layout(g.layouts.rise, LAYOUT_Rise, 0xE0), mn->layout, 0xE0);
			int16_t p[4];
			GetDefaultEffectPosition(Caster(), p);
			arg.depth = 0;
			arg.kind = KIND_Plain;
			FX_PART(PART_RISE);
			Play(mn, 0, mn->layout, &arg, p[0], p[2]);
		}
		// 0x41..0x48: prim model 0x145D6AC at the caster's bone 0x25 (its y axis scaled by minus the bone's height), turning
		i = (uint32_t)(mn->counter - g.bone.start);
		if (i < (uint32_t)g.bone.ticks)
		{
			// 30 fps layer: see mag210_bloodfest_held.inc
			FX_HELD(held_note_bone((int32_t)i);)
			FX_PART(PART_BONE);
			uint8_t *h = (uint8_t *)FieldAlloc(0x80);
			int16_t *sp = (int16_t *)(h + 0x78);
			GetEffectSpawnPosition(Caster(), g.bone.bone, 0, sp);
			// 30 fps layer: see mag210_bloodfest_held.inc
			FX_HELD(held_note_bone_point(sp);)
			Mat4x3 *m = (Mat4x3 *)(h + 0x58);
			RotationY((int32_t)(i * (uint32_t)g.bone.spin), m);
			m->m[1][1] = (int16_t)-sp[1];
			m->t[0] = sp[0];
			m->t[1] = 0;
			m->t[2] = sp[2];
			ComposeAffineTransform(&Camera(), m, m);
			GteSetRotMatrix(m);
			GteSetTransVector(m);
			*(uint32_t *)h = fxp::model(g.models.bone, MODEL_Bone);
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(uint32_t *)(h + 0xC) = (i << 12) / (uint32_t)g.bone.ticks;
			*(uint32_t *)(h + 8) = 0;
			PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
			FieldFree(0x80);
		}
		// 0x48..0x63: afterimages again, recorded up to 0x61
		i = (uint32_t)(mn->counter - g.ghosts.start_b);
		if (i < (uint32_t)g.ghosts.ticks_b)
		{
			if (i == 0) GhostInit(mn->ghosts, 1, 1);
			GhostDraw(Caster() + 0x9C, mn->ghosts);
			if (i < (uint32_t)g.ghosts.rec_b && Pause() == 0) GhostRecord(Caster() + 0x9C, mn->ghosts);
		}
		// 0x55..0x72: the stage capture (0x55) and the three-part draws at the centre
		i = (uint32_t)(mn->counter - g.vortex.start);
		if (i < (uint32_t)g.vortex.ticks)
		{
			int32_t fade = 0;
			if (i == 0)
			{
				const uint32_t burn = fxp::layout(g.layouts.burn, LAYOUT_Burn, 0x244);
				const uint32_t model = burn + *(const uint32_t *)(burn + 0x18);
				int16_t box[6];
				ModelBox(model, box);
				ModelBox(model, box);
				CaptureStage(mn->centre, box);
			}
			else if (i >= (uint32_t)g.vortex.fade_from) fade = shl32((int32_t)i - g.vortex.fade_from, 12) / g.vortex.fade_ticks;
			ThreePart(mn->centre, g.vortex.yaw_a, 0, (int32_t)((i << 12) / (uint32_t)g.vortex.grow), (int32_t)(i * (uint32_t)g.vortex.scroll), fade);
			if (i >= (uint32_t)g.vortex.second)
				ThreePart(mn->centre, g.vortex.yaw_b, g.vortex.roll, (int32_t)(((i - g.vortex.second) << 12) / (uint32_t)g.vortex.grow), (int32_t)(i * (uint32_t)g.vortex.scroll), fade);
		}
		// 0x73..0xC2: the targets lifted, spun and drawn by the module; the burning layout; 8 + 8 sparkles / rocks 0x73..0x76
		int16_t off[4];
		i = (uint32_t)(mn->counter - g.lift.start);
		if (i < (uint32_t)g.lift.ticks)
		{
			const int32_t u = (int32_t)((i << 12) / (uint32_t)g.lift.ticks) + 0x19;
			if (i == 0)
			{
				for (int32_t k = 0; k < mn->ntargets; k++)
				{
					MeasureHeight(mn->recs[k].e, mn->recs[k].height);
					*mn->recs[k].e |= 0xC;
				}
				DecodeModelPrimLayout(fxp::layout(g.layouts.burn, LAYOUT_Burn, 0x244), mn->layout, 0x244);
				InitTaskQueuePool(QSparkles(), (void *)(TexBase() + POOL_Sparkles), 0x18, 0x20);
				InitTaskQueuePool(QRocks(), (void *)(TexBase() + POOL_Rocks), 0x1C, 0x20);
				InitTaskQueuePool(QLayouts(), (void *)TexBase(), 0x714, 3);
				SpawnBurst(mn);
			}
			else if (i < (uint32_t)g.lift.burst_ticks) SpawnBurst(mn);
			off[2] = 0;
			off[0] = 0;
			for (int32_t k = 0; k < mn->ntargets; k++)
			{
				TargetRec *r = &mn->recs[k];
				uint8_t *e = r->e;
				if (Pause() == 0) *(int16_t *)(e + 0x1E) = LiftHeight(u, (int16_t)(r->pos01 >> 16));
				*(int32_t *)(e + 0x58) = *(const int16_t *)(e + 0x1E);
				r->ang[0] = (int16_t)(r->ang[0] + ((k & 1) ? g.lift.spin : -g.lift.spin));
				off[1] = (int16_t)((r->height[1] + r->height[0]) >> 1);
				FX_PART(PART_TARGETS);
				DrawEntity(e, r->ang, off);
			}
			const uint32_t d = i - g.lift.scroll_from;
			arg.depth = -0x80;
			arg.kind = KIND_Burn;
			arg.scroll = (uint16_t)(i * (uint32_t)g.lift.scroll);
			arg.w28 = d < 0x64 ? (uint16_t)((d * d) >> 4) : 0;
			FX_PART(PART_BURN);
			Play(mn, 1, mn->layout, &arg, mn->centre[0], mn->centre[2]);
		}
		// 0xC3..0xEA: the targets dropped and drawn, the caster and the next entity hidden; layout 0x146A668; 2 rocks a tick
		i = (uint32_t)(mn->counter - g.drop.start);
		if (i < (uint32_t)g.drop.ticks)
		{
			const int32_t w = (int32_t)((i << 12) / (uint32_t)g.drop.ticks) + 0x33;
			if (i == 0)
			{
				InitTaskQueuePool(QRocks(), (void *)(TexBase() + POOL_Rocks), 0x1C, 0x20);
				StageGroupsBit1Off();
				DecodeModelPrimLayout(fxp::layout(g.layouts.drop, LAYOUT_Drop, 0x258), mn->layout, 0x258);
				*Caster() |= 4;
				*(Caster() + 0x9C) |= 4;
			}
			off[2] = 0;
			off[0] = 0;
			for (int32_t k = 0; k < mn->ntargets; k++)
			{
				TargetRec *r = &mn->recs[k];
				uint8_t *e = r->e;
				const int16_t y = (int16_t)((int16_t)(r->pos01 >> 16) + (int16_t)(w * 4 - 0x2000));
				*(int16_t *)(e + 0x1E) = y;
				*(int32_t *)(e + 0x58) = y;
				r->ang[0] = (int16_t)(r->ang[0] + ((k & 1) ? g.lift.spin : -g.lift.spin));
				off[1] = (int16_t)((r->height[0] + r->height[1]) >> 1);
				FX_PART(PART_TARGETS);
				DrawEntity(e, r->ang, off);
			}
			if (Pause() == 0)
			{
				int32_t count = g.drop.rocks;
				RockNode *k = count > 0 ? (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask)) : nullptr;
				while (k)
				{
					k->life = g.drop.rock_life;
					const int32_t r = (CrtRand() & 0x7FF) + 0x800;
					RingPoint(CrtRand(), r, k->pos);
					k->pos[0] = (int16_t)(k->pos[0] + mn->centre[0]);
					k->pos[2] = (int16_t)(k->pos[2] + mn->centre[2]);
					k->pos[1] = (int16_t)(mn->centre[1] - g.drop.rock_height);
					k->vy = (int16_t)((CrtRand() & 0x7F) + g.drop.rock_speed);
					k->scale = (int16_t)((shl32(CrtRand(), 11) >> 15) + 0x800);
					k->ax = (int8_t)CrtRand();
					k->ay = (int8_t)CrtRand();
					k->dax = (int8_t)CrtRand();
					k->day = (int8_t)CrtRand();
					if (--count == 0) break;
					k = (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask));
				}
			}
			arg.depth = g.drop.depth;
			arg.kind = KIND_Plain;
			FX_PART(PART_DROP);
			Play(mn, 2, mn->layout, &arg, mn->centre[0], mn->centre[2]);
		}
		else if (i == (uint32_t)g.drop.ticks)
		{
			// 0xEB: the targets put back (flags 0x0C off, position, 0x1000 lower), the caster / next entity shown
			for (int32_t k = 0; k < mn->ntargets; k++)
			{
				TargetRec *r = &mn->recs[k];
				*(uint16_t *)r->e &= 0xFFF3;
				*(uint32_t *)(r->e + 0x1C) = r->pos01;
				*(uint32_t *)(r->e + 0x20) = r->pos23;
				*(int16_t *)(r->e + 0x1E) = (int16_t)(*(int16_t *)(r->e + 0x1E) - g.raise.lower);
			}
			*(uint16_t *)Caster() &= 0xFFFB;
			*(uint16_t *)(Caster() + 0x9C) &= 0xFFFB;
			// 0x6F8BD0: the stage groups' bit 1 back on
			for (uint8_t *gp = (uint8_t *)0x1D98991; gp < (uint8_t *)0x1D98A41; gp += 0x2C) *gp |= 2;
			QRocks()->tail = nullptr;
			QRocks()->head = nullptr;
		}
		else if (i <= (uint32_t)(g.drop.ticks + g.raise.ticks) && Pause() == 0)
		{
			// 0xEC..0xF3: the targets raised; 0xF3: a layout task on each and the result; 0xF1: the sound
			const uint32_t last = (uint32_t)(g.drop.ticks + g.raise.ticks);
			for (int32_t k = 0; k < mn->ntargets; k++)
			{
				uint8_t *e = mn->recs[k].e;
				*(int16_t *)(e + 0x1E) = (int16_t)(*(int16_t *)(e + 0x1E) + g.raise.step);
				if (i == last)
				{
					LayoutNode *t = (LayoutNode *)AddTaskToQueue(QLayouts(), own::Fn(ORIG_LayoutTask));
					if (!t) continue;
					GetDefaultEffectPosition(e, t->pos);
					t->pos[1] = 0;
					DecodeModelPrimLayout(fxp::layout(g.layouts.end, LAYOUT_End, 0x1C0), t->layout, 0x1C0);
				}
			}
			if (mn->counter == g.timing.fall) BdPlaySE((const void *)fxp::sound(g.sounds.fall, (uint32_t)SOUND_Fall), 0, 0x80);
			else if (i == last) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		}
		// white tile series 0xE3..0xF2
		i = (uint32_t)(mn->counter - g.white.start);
		if (i < (uint32_t)(g.white.in + g.white.hold + g.white.out))
		{
			// 30 fps layer: see mag210_bloodfest_held.inc
			FX_HELD(held_note_tile((int32_t)i);)
			FX_PART(PART_WHITE);
			PacketCursor() = FadeTileInHoldOut((int32_t)i, g.white.r, g.white.g, g.white.b, g.white.in, g.white.hold, g.white.out, PacketCursor());
		}
		// counters: the flames dropped, the stream loads, the wait, the pump
		if (mn->counter == g.flames.drop)
		{
			QFlames()->tail = nullptr;
			QFlames()->head = nullptr;
		}
		else if (mn->counter == 0x23) StreamQueueLoad(0x2C0, TexBase() + ARENA_A, 0);
		else if (mn->counter == 0x36 && StreamBusy() != 0) return 0;
		if (mn->counter == 0xEB)
		{
			StreamQueueLoad(0x168, TexBase() + STREAM_B, 1);
			StreamQueueLoad(0x169, TexBase() + STREAM_B, 1);
			StreamQueueLoad(0x16A, TexBase() + STREAM_B, 0);
		}
		StreamPump();
		if (mn->counter == g.timing.start) BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		else if (mn->counter == g.timing.ghost) BdPlaySE((const void *)fxp::sound(g.sounds.ghost, (uint32_t)SOUND_Ghost), 0, 0x80);
		else if (mn->counter == g.timing.lift) BdPlaySE((const void *)fxp::sound(g.sounds.lift, (uint32_t)SOUND_Lift), 0, 0x80);
		if (mn->counter >= g.timing.end)
		{
			if (StreamPreloadPending(2) != 0) return TASK_END;
			mn->counter = 0x133;
			return 0;
		}
		mn->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// TrailTask (0x6F82D0): 251's blade trail of slot 4's bone 1 (0x6F8770 / 0x6F87D0), 0x3B ticks, shorter from 0x35
	// ------------------------------------------------------------------
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
			uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
			SplineSetup(cnt, t->pts_a, wk);
			for (int32_t k = 0, u = 0; k < 32; k++, u += 0x1000) SplineEval(cnt, wk, t->spl_a[k], u / 31);
			SplineSetup(cnt, t->pts_b, wk);
			for (int32_t k = 0, u = 0; k < 32; k++, u += 0x1000) SplineEval(cnt, wk, t->spl_b[k], u / 31);
			FieldFree(0x190);
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
			// 30 fps layer: see mag210_bloodfest_held.inc
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
	// LayoutTask (0x6F9AA0): its layout at its point through the view (depth -0x80)
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
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_task_layout(t, &arg);)
		FX_PART(PART_END);
		const int left = prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, (int)Pause());
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// RockTask (0x6F9B60): prim model 0x1461D98 turned by its x / y bytes (x 16), scaled; falls and spins
	// ------------------------------------------------------------------
	static void RockDrawAngles(const RockNode *k, int16_t ax, int16_t ay)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		ScaledRotY(ay, k->scale, (Mat4x3 *)(h + 0x78));
		RotationX(ax, (Mat4x3 *)(h + 0x58));
		MatrixMultiply3x3((Mat4x3 *)(h + 0x58), h + 0x78);
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		m->t[0] = k->pos[0];
		m->t[1] = k->pos[1];
		m->t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = fxp::model(PG().models.rock, MODEL_Rock);
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}
	static void RockDraw(const RockNode *k) { RockDrawAngles(k, (int16_t)((int16_t)k->ax << 4), (int16_t)((int16_t)k->ay << 4)); }
	static uint32_t __cdecl RockTask(TaskNode *n)
	{
		RockNode *k = (RockNode *)n;
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_draw(ORIG_RockTask, k);)
		FX_PART(PART_ROCKS);
		RockDraw(k);
		if (Pause() != 0) return 0;
		k->life--;
		if (k->life == 0) return TASK_END;
		k->ax = (int8_t)(k->ax + k->dax);
		k->ay = (int8_t)(k->ay + k->day);
		k->pos[1] = (int16_t)(k->pos[1] + k->vy);
		k->vy = (int16_t)(k->vy + PG().rocks.gravity);
		return 0;
	}

	// ------------------------------------------------------------------
	// SparkleTask (0x6F9C60): sequence 0x145D4EC at its point, rising by its speed; 16 frames
	// ------------------------------------------------------------------
	static void SparkleDraw(const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x1000, -0x400);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = fxp::sequence(PG().seqs.sparkle, SEQ_Sparkle);
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(held_note_draw(ORIG_SparkleTask, s);)
		FX_PART(PART_SPARKLES);
		SparkleDraw(s->pos, s->frame);
		if (Pause() != 0) return 0;
		s->pos[1] = (int16_t)(s->pos[1] + s->vy);
		s->frame++;
		return s->frame < PG().sparkles.frames ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Setup (0x6F6800 -> _Init 0x6F6810): the game runs its own; Fujin's programs this (its memory may be a copy's): the
	// TIM (exe data), the magic buffer, the context, the caster's entity, the master, the camera. (The file loader is the
	// engine's bare ret.)
	// ------------------------------------------------------------------
	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(fxp::tim(PG().res.tim, TIM));
		TexBase() = (uint32_t)layouts::MagicBuffer();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		InitTaskQueuePool(QMaster(), (void *)own::R(0x25564F0), 0x10, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 1);
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "master", "Master", "The main task's ticks, its four sounds, the screen flash rising and falling; the TIM and the camera" },
			{ PART_FLAMES, "flames", "Flames", "Flames around each target (Fire's parts), each with its ground glow, embers rising and sparks drifting" },
			{ PART_GHOSTS, "ghosts", "Afterimages", "Afterimages of the entity after the caster (the last two recorded poses, the newer brighter), in two windows" },
			{ PART_RISE, "rise", "Rising layout", "The rising layout at the caster's effect point" },
			{ PART_BONE, "bone", "Bone model", "A mesh at the caster's bone, stretched to the ground, turning, fading in" },
			{ PART_VORTEX, "vortex", "Vortex", "The stage captured into VRAM, then a vortex at the targets' centre (a ring pushed out and quads of the capture's texture growing in, a second one turned and rolled), fading out" },
			{ PART_TARGETS, "targets", "Targets", "The targets lifted spinning (a sine, then a cubic fall), then dropped from above, drawn by the module" },
			{ PART_BURN, "burn", "Burning layout", "The burning layout at the targets' centre while they are lifted (its textures scrolling); a burst of sparkles and rocks on its first ticks" },
			{ PART_DROP, "drop", "Drop", "The drop layout at the targets' centre while they fall (the caster and the next entity hidden), rocks falling a few a tick; then the targets put back and raised" },
			{ PART_END, "end", "End layouts", "A layout at each target's feet as the results come" },
			{ PART_WHITE, "white", "White flash", "A full-screen flash (fading in, held, fading out)" },
			{ PART_TRAIL, "trail", "Blade trail", "The blade trail of slot 4's bone 1: a ring of its last 7 points splined, drawn as gouraud quads fading along it" },
			{ PART_ROCKS, "rocks", "Rocks", "Rocks (a mesh) turning, rising then falling" },
			{ PART_SPARKLES, "sparkles", "Sparkles", "Sparkles rising from a ring round the targets' centre" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The main task's ticks" },
			{ "flash", "Screen flash", "The screen flash's level and slopes" },
		};
#define MPF(member) (uint32_t)offsetof(BloodfestParams, member)
		static const Field FIELDS[] = {
			{ "master", "sounds.start", "Start sound", "At the start sound's tick", MPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.ghost", "Afterimage sound", "At the afterimage sound's tick", MPF(sounds.ghost), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.lift", "Lift sound", "At the lift sound's tick", MPF(sounds.lift), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.fall", "Fall sound", "At the fall sound's tick", MPF(sounds.fall), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "timing.start", "Start sound", "The main task's tick of the start sound", MPF(timing.start), fxp::I16, 0, 400 },
			{ "master", "timing.ghost", "Afterimage sound", "Its tick of the afterimage sound", MPF(timing.ghost), fxp::I16, 0, 400 },
			{ "master", "timing.lift", "Lift sound", "Its tick of the lift sound", MPF(timing.lift), fxp::I16, 0, 400 },
			{ "master", "timing.fall", "Fall sound", "Its tick of the fall sound (while the targets rise back)", MPF(timing.fall), fxp::I16, 0, 400 },
			{ "master", "timing.end", "End", "Its last tick: it ends once the battle files' preload is done (the screen flash falls to 0 there)", MPF(timing.end), fxp::I16, 1, 400 },
			{ "master", "flash.level", "Flash level", "The screen flash's level", MPF(flash.level), fxp::I16, 0, 0x1000 },
			{ "master", "flash.rise", "Flash rise", "It rises over the first ticks (at least 1)", MPF(flash.rise), fxp::I16, 1, 400 },
			{ "master", "flash.fall", "Flash fall", "It falls over the last ticks before the end (at least 1)", MPF(flash.fall), fxp::I16, 1, 400 },
			{ "master", "res.tim", "TIM", "The TIM its setup uploads (exe data: the effect has no file)", MPF(res.tim), fxp::Tim, 0, 0x7FFFFFFF },
			{ "master", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", MPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "flames", "flames.count", "Count", "Flames around each target", MPF(flames.count), fxp::I16, 0, 8 },
			{ "flames", "flames.frames", "Frames", "A flame's frames (its flipbook's: 16 at most)", MPF(flames.frames), fxp::I16, 1, 16 },
			{ "flames", "flames.embers", "Embers", "Embers rising from each flame", MPF(flames.embers), fxp::I16, 0, 16 },
			{ "flames", "flames.sparks", "Sparks", "Sparks drifting from each flame", MPF(flames.sparks), fxp::I16, 0, 20 },
			{ "flames", "flames.size", "Size", "A flame's size: this + a random below the next", MPF(flames.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "flames", "flames.size_rand", "Size random", "(at least 1)", MPF(flames.size_rand), fxp::I16, 1, 0x7FFF },
			{ "flames", "flames.drop", "Dropped", "The main task's tick every flame, ember and spark is dropped", MPF(flames.drop), fxp::I16, 0, 400 },
			{ "flames", "seqs.flame", "Flame", "The flames' sprite sequence (a flipbook)", MPF(seqs.flame), fxp::Seq, 0, 0x7FFFFFFF },
			{ "flames", "seqs.glow", "Ground glow", "Their ground glow's sprite sequence", MPF(seqs.glow), fxp::Seq, 0, 0x7FFFFFFF },
			{ "flames", "seqs.ember", "Ember", "The embers' sprite sequence", MPF(seqs.ember), fxp::Seq, 0, 0x7FFFFFFF },
			{ "ghosts", "ghosts.start_a", "First from", "The main task's tick of the first window (its first tick resets the record)", MPF(ghosts.start_a), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.ticks_a", "First ticks", "Its ticks", MPF(ghosts.ticks_a), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.rec_a", "First records", "A pose recorded on its ticks below this (after, they fade away)", MPF(ghosts.rec_a), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.start_b", "Second from", "The main task's tick of the second window", MPF(ghosts.start_b), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.ticks_b", "Second ticks", "Its ticks", MPF(ghosts.ticks_b), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.rec_b", "Second records", "A pose recorded on its ticks below this", MPF(ghosts.rec_b), fxp::I16, 0, 400 },
			{ "ghosts", "ghosts.bright", "Brightness", "The newer afterimage's colour: the entity's x 1.5 x this / 0x1000", MPF(ghosts.bright), fxp::I16, 0, 0x1000 },
			{ "ghosts", "ghosts.fade", "Older", "The older one's brightness is less by this", MPF(ghosts.fade), fxp::I16, -0x1000, 0x1000 },
			{ "rise", "rise.start", "Start", "The main task's tick it starts", MPF(rise.start), fxp::I16, 0, 400 },
			{ "rise", "rise.ticks", "Ticks", "Its ticks", MPF(rise.ticks), fxp::I16, 0, 400 },
			{ "rise", "layouts.rise", "Layout", "The prim-model layout played", MPF(layouts.rise), fxp::Layout, 0, 0x7FFFFFFF },
			{ "bone", "bone.start", "Start", "The main task's tick it starts", MPF(bone.start), fxp::I16, 0, 400 },
			{ "bone", "bone.ticks", "Ticks", "Its ticks, fading in over them (at least 1)", MPF(bone.ticks), fxp::I16, 1, 400 },
			{ "bone", "bone.bone", "Bone", "The caster's bone it stands at (its spawn point)", MPF(bone.bone), fxp::I16, 0, 0xFF },
			{ "bone", "bone.spin", "Spin", "Its turn a tick", MPF(bone.spin), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "bone", "models.bone", "Mesh", "The mesh drawn", MPF(models.bone), fxp::Model, 0, 0x7FFFFFFF },
			{ "vortex", "vortex.start", "Start", "The main task's tick it starts (the stage captured then)", MPF(vortex.start), fxp::I16, 0, 400 },
			{ "vortex", "vortex.ticks", "Ticks", "Its ticks", MPF(vortex.ticks), fxp::I16, 0, 400 },
			{ "vortex", "vortex.second", "Second from", "Its tick the second one starts", MPF(vortex.second), fxp::I16, 0, 400 },
			{ "vortex", "vortex.grow", "Grow", "Each grows in over so many ticks (the ring pushed out, the quads' depth; at least 1)", MPF(vortex.grow), fxp::I16, 1, 400 },
			{ "vortex", "vortex.fade_from", "Fade from", "From this tick of it both fade out (the mesh drawn alone, faded)", MPF(vortex.fade_from), fxp::I16, 0, 400 },
			{ "vortex", "vortex.fade_ticks", "Fade ticks", "Over so many ticks (at least 1)", MPF(vortex.fade_ticks), fxp::I16, 1, 400 },
			{ "vortex", "vortex.yaw_a", "First yaw", "The first one's turn", MPF(vortex.yaw_a), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "vortex", "vortex.yaw_b", "Second yaw", "The second one's turn", MPF(vortex.yaw_b), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "vortex", "vortex.roll", "Second roll", "The second ring's roll", MPF(vortex.roll), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "vortex", "vortex.scroll", "Scroll", "Their texture scroll a tick", MPF(vortex.scroll), fxp::I16, -0x100, 0x100 },
			{ "vortex", "models.part", "Mesh", "The vortex mesh (drawn as quads of the capture's texture, or faded)", MPF(models.part), fxp::Model, 0, 0x7FFFFFFF },
			{ "vortex", "models.ring", "Ring", "The ring mesh", MPF(models.ring), fxp::Model, 0, 0x7FFFFFFF },
			{ "targets", "lift.start", "Lift from", "The main task's tick the targets are lifted (the burning layout, the burst; the sparkles', rocks' and end layouts' pools set up)", MPF(lift.start), fxp::I16, 0, 400 },
			{ "targets", "lift.ticks", "Lift ticks", "Its ticks", MPF(lift.ticks), fxp::I16, 1, 400 },
			{ "targets", "lift.spin", "Spin", "The targets' turn a tick (every other one the other way)", MPF(lift.spin), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "targets", "raise.ticks", "Raise ticks", "After the drop, the targets put back and raised for so many ticks; then the end layouts and the results", MPF(raise.ticks), fxp::I16, 0, 100 },
			{ "targets", "raise.step", "Raise step", "Raised so much a tick", MPF(raise.step), fxp::I16, -0x1000, 0x1000 },
			{ "targets", "raise.lower", "Lowered", "Put back so much lower first", MPF(raise.lower), fxp::I16, -0x4000, 0x4000 },
			{ "burn", "lift.scroll", "Scroll", "The burning layout's texture scroll a tick", MPF(lift.scroll), fxp::I16, -0x100, 0x100 },
			{ "burn", "lift.scroll_from", "Second scroll from", "Its second scroll (a square curve) from this tick of it", MPF(lift.scroll_from), fxp::I16, 0, 400 },
			{ "burn", "lift.burst_ticks", "Burst ticks", "Its first ticks with a burst", MPF(lift.burst_ticks), fxp::I16, 0, 100 },
			{ "burn", "lift.sparkles", "Burst sparkles", "Sparkles a tick of the burst", MPF(lift.sparkles), fxp::I16, 0, 32 },
			{ "burn", "lift.rocks", "Burst rocks", "Rocks a tick of the burst", MPF(lift.rocks), fxp::I16, 0, 32 },
			{ "burn", "lift.radius", "Burst radius", "On a ring of this + a random below 0x800 round the centre", MPF(lift.radius), fxp::I16, 0, 0x7000 },
			{ "burn", "lift.rise", "Burst rise", "Rising at this + a random below 0x80 a tick", MPF(lift.rise), fxp::I16, -0x1000, 0x1000 },
			{ "burn", "layouts.burn", "Layout", "The burning layout (its first model's box is the stage capture's)", MPF(layouts.burn), fxp::Layout, 0, 0x7FFFFFFF },
			{ "drop", "drop.start", "Start", "The main task's tick the targets drop", MPF(drop.start), fxp::I16, 0, 400 },
			{ "drop", "drop.ticks", "Ticks", "Its ticks (at least 1)", MPF(drop.ticks), fxp::I16, 1, 400 },
			{ "drop", "drop.rocks", "Rocks", "Rocks a tick", MPF(drop.rocks), fxp::I16, 0, 32 },
			{ "drop", "drop.rock_life", "Rock life", "A rock's ticks", MPF(drop.rock_life), fxp::I16, 1, 400 },
			{ "drop", "drop.rock_height", "Rock height", "Rocks start so high above the centre", MPF(drop.rock_height), fxp::I16, -0x4000, 0x4000 },
			{ "drop", "drop.rock_speed", "Rock speed", "Falling at this + a random below 0x80 a tick", MPF(drop.rock_speed), fxp::I16, -0x1000, 0x1000 },
			{ "drop", "drop.depth", "Depth", "The drop layout's depth offset", MPF(drop.depth), fxp::I16, -0x1000, 0x1000 },
			{ "drop", "layouts.drop", "Layout", "The drop layout", MPF(layouts.drop), fxp::Layout, 0, 0x7FFFFFFF },
			{ "end", "layouts.end", "Layout", "The end layout", MPF(layouts.end), fxp::Layout, 0, 0x7FFFFFFF },
			{ "white", "white.start", "Start", "The main task's tick it starts", MPF(white.start), fxp::I16, 0, 400 },
			{ "white", "white.in", "In", "Ticks fading in", MPF(white.in), fxp::I16, 0, 100 },
			{ "white", "white.hold", "Hold", "Ticks held", MPF(white.hold), fxp::I16, 0, 100 },
			{ "white", "white.out", "Out", "Ticks fading out", MPF(white.out), fxp::I16, 0, 100 },
			{ "white", "white.r", "Red", "Its colour", MPF(white.r), fxp::I16, 0, 255 },
			{ "white", "white.g", "Green", "Its colour", MPF(white.g), fxp::I16, 0, 255 },
			{ "white", "white.b", "Blue", "Its colour", MPF(white.b), fxp::I16, 0, 255 },
			{ "trail", "trail.start", "Start", "The main task's tick it starts (0x37 at least: its pool is set up then)", MPF(trail.start), fxp::I16, 0x37, 400 },
			{ "trail", "trail.ticks", "Ticks", "Its ticks", MPF(trail.ticks), fxp::I16, 1, 400 },
			{ "trail", "trail.shorten", "Shorter from", "From this tick of it, it keeps ticks + 1 - its tick points (7 before)", MPF(trail.shorten), fxp::I16, 0, 400 },
			{ "trail", "trail.length", "Length", "Its tip's distance from the bone along the blade", MPF(trail.length), fxp::I16, -0x2000, 0x2000 },
			{ "trail", "trail.colour", "Colour", "Its colour at the blade (0xBBGGRR), fading to black along it", MPF(trail.colour), fxp::I32, 0, 0xFFFFFF },
			{ "rocks", "rocks.gravity", "Gravity", "A rock's fall speed grows by this a tick", MPF(rocks.gravity), fxp::I16, -0x400, 0x400 },
			{ "rocks", "models.rock", "Mesh", "The rock mesh", MPF(models.rock), fxp::Model, 0, 0x7FFFFFFF },
			{ "sparkles", "sparkles.frames", "Frames", "A sparkle's frames (its flipbook's: 16 at most)", MPF(sparkles.frames), fxp::I16, 1, 16 },
			{ "sparkles", "seqs.sparkle", "Sparkle", "The sparkles' sprite sequence", MPF(seqs.sparkle), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef MPF
	}
}

	void register_mag210_bloodfest()
	{
		fxp::register_params({ 210, "Bloodfest", &blood210::live, &blood210::GAME, sizeof(blood210::BloodfestParams),
		                       blood210::table::PARTS, (int)(sizeof(blood210::table::PARTS) / sizeof(blood210::table::PARTS[0])),
		                       blood210::table::FIELDS, (int)(sizeof(blood210::table::FIELDS) / sizeof(blood210::table::FIELDS[0])),
		                       blood210::table::GROUPS, (int)(sizeof(blood210::table::GROUPS) / sizeof(blood210::table::GROUPS[0])) });
		own::note(blood210::ORIG_Setup, (void *)blood210::Setup);
		own::note(blood210::ORIG_MasterTask, (void *)blood210::MasterTask);
		register_port(blood210::ORIG_MasterTask, (void *)blood210::MasterTask, "210 MasterTask", 210);
		own::note(blood210::ORIG_FlameSpawner, (void *)blood210::FlameSpawner);
		register_port(blood210::ORIG_FlameSpawner, (void *)blood210::FlameSpawner, "210 FlameSpawner", 210);
		own::note(blood210::ORIG_FlameTask, (void *)blood210::FlameTask);
		register_port(blood210::ORIG_FlameTask, (void *)blood210::FlameTask, "210 FlameTask", 210);
		own::note(blood210::ORIG_SparkTask, (void *)blood210::SparkTask);
		register_port(blood210::ORIG_SparkTask, (void *)blood210::SparkTask, "210 SparkTask", 210);
		own::note(blood210::ORIG_EmberTask, (void *)blood210::EmberTask);
		register_port(blood210::ORIG_EmberTask, (void *)blood210::EmberTask, "210 EmberTask", 210);
		own::note(blood210::ORIG_MainTask, (void *)blood210::MainTask);
		register_port(blood210::ORIG_MainTask, (void *)blood210::MainTask, "210 MainTask", 210);
		own::note(blood210::ORIG_TrailTask, (void *)blood210::TrailTask);
		register_port(blood210::ORIG_TrailTask, (void *)blood210::TrailTask, "210 TrailTask", 210);
		own::note(blood210::ORIG_LayoutTask, (void *)blood210::LayoutTask);
		register_port(blood210::ORIG_LayoutTask, (void *)blood210::LayoutTask, "210 LayoutTask", 210);
		own::note(blood210::ORIG_RockTask, (void *)blood210::RockTask);
		register_port(blood210::ORIG_RockTask, (void *)blood210::RockTask, "210 RockTask", 210);
		own::note(blood210::ORIG_SparkleTask, (void *)blood210::SparkleTask);
		register_port(blood210::ORIG_SparkleTask, (void *)blood210::SparkleTask, "210 SparkleTask", 210);
		// 30 fps layer: see mag210_bloodfest_held.inc
		FX_HELD(register_mag210_held();)
	}
	FX_LAB_MODULE(register_mag210_bloodfest);
}

#ifdef FF8_FX_HELD
#include "mag210_bloodfest_held.inc"
#endif
