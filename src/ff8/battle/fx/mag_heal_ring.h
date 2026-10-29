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

// The healing ring's parameters (fx_params.h), shared by the modules that run its code: Cure
// (effect 1, mag001_cure.cpp) and its relocated copy Potion (effect 50, mag050_potion.cpp). One
// struct per part, the game's values (the same in both), the parts (FX_PART tags) and the field
// table; each module keeps its own live copy, so each effect can be edited on its own.

#pragma once

#include <stddef.h>

#include "fx_params.h"

namespace ff8fx
{
namespace healring
{
	enum PartId { PART_RING = 1, PART_GLOW = 2, PART_OVERLAY = 3, PART_SPARKLES = 4, PART_MOTES = 5, PART_GLINTS = 6 };
	struct HealRingParams
	{
		struct Emitter
		{
			int16_t sound_tick;      // the sound plays at this tick
			int16_t heal_tick;       // the HP are given from this tick
		} emitter;
		struct Ring
		{
			int16_t substeps;        // places per tick (each spawns a sparkle while it spawns)
			int16_t angle_step;      // turn per place (4096 = a full turn)
			fxp::Fraction radius;    // of the target's bounds radius
			int16_t radius_max;
			int16_t climb_extra;     // climbs the target's height plus this...
			int16_t climb_ticks;     // ...in this many ticks (upwards)
			int16_t end_tick;        // its ending flipbook starts at this tick
			int16_t end_frames;      // last frame of the ending flipbook
		} ring;
		struct Glow
		{
			int16_t ticks;           // drawn under the ring for this many ticks
			uint8_t grey;            // starting brightness (additive)
			uint8_t fade;            // brightness lost per tick
		} glow;
		struct Overlay
		{
			int16_t tex_x, tex_y;    // projected image: its VRAM place...
			int16_t clut_x, clut_y;  // ...its palette
			int16_t width, height;   // projected window
			uint8_t rgb[3];          // projected light colour
			uint8_t pad;
			int32_t light_dist;      // projector distance
		} overlay;
		struct Sparkle
		{
			fxp::Window window;      // ring ticks it spawns them (one per place)
			fxp::Range vx, vy, vz;   // speed per tick (y down)
			int16_t frames;          // last flipbook frame: lifetime
		} sparkle;
		struct Mote
		{
			fxp::Window window;      // ring ticks it spawns them
			fxp::Schedule count;     // how many per tick
			fxp::Fraction radius;    // circle: of the target's bounds radius
			int16_t radius_max;
			fxp::Range angle;        // place on the circle
			fxp::Range drop;         // below the ring: drop / drop_divisor of its height over the target
			int16_t drop_divisor;
			fxp::Range rise;         // rising speed per tick
			int16_t frames;          // last flipbook frame: lifetime
		} mote;
		struct Glint
		{
			fxp::Window window;      // ring ticks it spawns them
			fxp::Schedule count;     // how many per tick
			fxp::GlintStyle style;
		} glint;
	};
	inline const HealRingParams DEFAULTS = {
		{ 0, 0xF },
		{ 4, 0x80, { 2, 1 }, 0x400, 0x100, 20, 0x14, 4 },
		{ 0x14, 0x3C, 3 },
		{ 0x200, 0x180, 0x140, 0xF1, 0x80, 0x80, { 0x80, 0x80, 0x80 }, 0, 0x2000 },
		{ { 1, 0x10 }, { -0x10, 0xF }, { 0x10, 0x1F }, { -0x10, 0xF }, 8 },
		{ { 2, 0x7FFE }, { { 2, 7, 9, 11 }, { 1, 2, 3, 2 } }, { 3, 2 }, 0x400, { 0, 0xFFF }, { 0, 0x1F }, 48, { 8, 0x17 }, 0x10 },
		{ { 4, 10 }, { { 4, 7, 9, 11 }, { 2, 3, 4, 2 } }, { 8, 0x100, 0x10, 0x40, 0x10, { 0x10, 0x10, 0x10 }, { 0, 0, 0x10 }, 0x40 } },
	};

#define HEAL_RING_FIELD(part, member, label, doc, type, lo, hi) \
	{ part, #member, label, doc, (uint32_t)offsetof(HealRingParams, member), fxp::type, lo, hi }
	inline const fxp::Part PARTS[] = {
		{ 0, "emitter", "Emitter", "The cast on one target: when its sound plays and its HP are given (draws nothing)" },
		{ PART_RING, "ring", "Ring", "The light climbing in a helix around the target; it spawns the sparkles, motes and glints" },
		{ PART_GLOW, "glow", "Ground glow", "An additive glow on the ground under the ring, fading out" },
		{ PART_OVERLAY, "overlay", "Overlay", "The target redrawn lit by an image projected from the ring" },
		{ PART_SPARKLES, "sparkles", "Sparkles", "Flipbooks flying out and up from each place of the ring" },
		{ PART_MOTES, "motes", "Motes", "Flipbooks rising on a circle around the target" },
		{ PART_GLINTS, "glints", "Glints", "The shared effect library's glints: streaks growing then shrinking, spinning" },
	};
	inline const fxp::Field FIELDS[] = {
		HEAL_RING_FIELD("emitter", emitter.sound_tick, "Sound tick", "The sound plays at this tick", I16, 0, 200),
		HEAL_RING_FIELD("emitter", emitter.heal_tick, "Heal tick", "The HP are given from this tick", I16, 0, 200),
		HEAL_RING_FIELD("ring", ring.substeps, "Places per tick", "Places the ring goes through each tick (each spawns a sparkle)", I16, 1, 16),
		HEAL_RING_FIELD("ring", ring.angle_step, "Turn per place", "Angle between two places (4096 = a full turn)", I16, -2048, 2048),
		HEAL_RING_FIELD("ring", ring.radius.num, "Radius (times)", "Radius = target radius x this / divisor", I16, 0, 32),
		HEAL_RING_FIELD("ring", ring.radius.den, "Radius (divisor)", "", I16, 1, 32),
		HEAL_RING_FIELD("ring", ring.radius_max, "Radius max", "", I16, 0, 0x2000),
		HEAL_RING_FIELD("ring", ring.climb_extra, "Climb above", "The ring climbs the target's height plus this", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("ring", ring.climb_ticks, "Climb ticks", "...in this many ticks", I16, 1, 200),
		HEAL_RING_FIELD("ring", ring.end_tick, "End tick", "Its ending flipbook starts at this tick", I16, 1, 200),
		HEAL_RING_FIELD("ring", ring.end_frames, "End frames", "Last frame of the ending flipbook", I16, 0, 64),
		HEAL_RING_FIELD("glow", glow.ticks, "Ticks", "Drawn for this many ticks", I16, 0, 200),
		HEAL_RING_FIELD("glow", glow.grey, "Brightness", "Starting brightness (additive grey)", U8, 0, 255),
		HEAL_RING_FIELD("glow", glow.fade, "Fade per tick", "Brightness lost per tick", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.tex_x, "Image VRAM x", "Projected image: VRAM x", I16, 0, 1023),
		HEAL_RING_FIELD("overlay", overlay.tex_y, "Image VRAM y", "", I16, 0, 511),
		HEAL_RING_FIELD("overlay", overlay.clut_x, "Palette x", "", I16, 0, 1023),
		HEAL_RING_FIELD("overlay", overlay.clut_y, "Palette y", "", I16, 0, 511),
		HEAL_RING_FIELD("overlay", overlay.width, "Window width", "Projected window", I16, 1, 255),
		HEAL_RING_FIELD("overlay", overlay.height, "Window height", "", I16, 1, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[0], "Red", "Projected light colour", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[1], "Green", "", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[2], "Blue", "", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.light_dist, "Projector distance", "", I32, 0, 0x10000),
		HEAL_RING_FIELD("sparkles", sparkle.window.first, "From tick", "Ring ticks they are spawned (one per place)", I16, 0, 200),
		HEAL_RING_FIELD("sparkles", sparkle.window.last, "To tick", "", I16, 0, 200),
		HEAL_RING_FIELD("sparkles", sparkle.vx.min, "Speed x min", "Speed per tick, at random in [min, max]", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.vx.max, "Speed x max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.vy.min, "Speed y min", "y goes down", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.vy.max, "Speed y max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.vz.min, "Speed z min", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.vz.max, "Speed z max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.frames, "Frames", "Last flipbook frame: lifetime", I16, 0, 64),
		HEAL_RING_FIELD("motes", mote.window.first, "From tick", "Ring ticks they are spawned", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.window.last, "To tick", "", I16, 0, 0x7FFE),
		HEAL_RING_FIELD("motes", mote.count.from[0], "Step 1 from", "From this tick...", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[0], "Step 1 count", "...this many per tick", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[1], "Step 2 from", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[1], "Step 2 count", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[2], "Step 3 from", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[2], "Step 3 count", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[3], "Step 4 from", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[3], "Step 4 count", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.radius.num, "Circle (times)", "Circle radius = target radius x this / divisor", I16, 0, 32),
		HEAL_RING_FIELD("motes", mote.radius.den, "Circle (divisor)", "", I16, 1, 32),
		HEAL_RING_FIELD("motes", mote.radius_max, "Circle max", "", I16, 0, 0x2000),
		HEAL_RING_FIELD("motes", mote.angle.min, "Angle min", "Place on the circle (4096 = a full turn)", I16, 0, 4095),
		HEAL_RING_FIELD("motes", mote.angle.max, "Angle max", "", I16, 0, 4095),
		HEAL_RING_FIELD("motes", mote.drop.min, "Drop min", "Below the ring: drop / divisor of its height over the target", I16, 0, 255),
		HEAL_RING_FIELD("motes", mote.drop.max, "Drop max", "", I16, 0, 255),
		HEAL_RING_FIELD("motes", mote.drop_divisor, "Drop divisor", "", I16, 1, 1000),
		HEAL_RING_FIELD("motes", mote.rise.min, "Rise min", "Rising speed per tick", I16, -0x400, 0x400),
		HEAL_RING_FIELD("motes", mote.rise.max, "Rise max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("motes", mote.frames, "Frames", "Last flipbook frame: lifetime", I16, 0, 64),
		HEAL_RING_FIELD("glints", glint.window.first, "From tick", "Ring ticks they are spawned", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.window.last, "To tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.from[0], "Step 1 from", "From this tick...", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[0], "Step 1 count", "...this many per tick", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[1], "Step 2 from", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[1], "Step 2 count", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[2], "Step 3 from", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[2], "Step 3 count", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[3], "Step 4 from", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[3], "Step 4 count", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.style.ticks, "Ticks per phase", "Grows for this many ticks, then shrinks for as many", I16, 1, 100),
		HEAL_RING_FIELD("glints", glint.style.grow_w, "Grow width", "Size added per tick while growing", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.grow_h, "Grow height", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.shrink_w, "Shrink width", "Size taken per tick while shrinking", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.shrink_h, "Shrink height", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.centre_step[0], "Centre red step", "Centre colour added per tick (taken while shrinking)", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.centre_step[1], "Centre green step", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.centre_step[2], "Centre blue step", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[0], "Tip red step", "Tip colour added per tick", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[1], "Tip green step", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[2], "Tip blue step", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.spin, "Spin", "Y angle per tick, either way at random", I16, -2048, 2048),
	};
#undef HEAL_RING_FIELD

	// a module's live parameters under its effect id (for the tools: fx_params.h)
	inline void register_params(int effect_id, const char *name, HealRingParams *live)
	{
		fxp::register_params({ effect_id, name, live, &DEFAULTS, sizeof(HealRingParams),
		                       PARTS, (int)(sizeof(PARTS) / sizeof(PARTS[0])),
		                       FIELDS, (int)(sizeof(FIELDS) / sizeof(FIELDS[0])) });
	}
}
}
