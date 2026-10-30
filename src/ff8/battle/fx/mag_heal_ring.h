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
// struct per part, the values edited together in a sub-structure of their own (a group: the
// ring's radius, its steps...), the game's values (the same in both but the sprite sequences:
// each module has its own copies), the parts (FX_PART tags), the field and group tables; each
// module keeps its own live copy, so each effect can be edited on its own.

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
			uint32_t sound;          // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick;      // the sound plays at this tick
			int16_t heal_tick;       // the HP are given from this tick
		} emitter;
		struct Ring
		{
			uint32_t seq;            // sprite sequence of the climbing light (FF8_EN.exe data)
			struct Steps
			{
				int16_t per_tick;    // each tick: drawn at the last one, a sparkle born at each
				int16_t angle;       // around the target (4096 = a full turn)
			} steps;
			fxp::Scaled radius;      // of the target's bounds radius
			struct Climb
			{
				int16_t above;       // climbs the target's height plus this...
				int16_t ticks;       // ...in this many ticks (upwards)
			} climb;
			struct Ending
			{
				uint32_t seq;        // sprite sequence of its ending flipbook...
				int16_t tick;        // ...from this tick...
				int16_t frames;      // ...up to this frame
			} ending;
		} ring;
		struct Glow
		{
			uint32_t seq;            // sprite sequence (its frame 0, lying on the ground)
			int16_t ticks;           // drawn under the ring for this many ticks
			struct Brightness
			{
				uint8_t start;       // additive grey...
				uint8_t fade;        // ...lost per tick
			} brightness;
		} glow;
		struct Overlay
		{
			struct Image
			{
				int16_t tex_x, tex_y;    // its VRAM place...
				int16_t clut_x, clut_y;  // ...its palette
				int16_t width, height;   // projected window
			} image;
			uint8_t rgb[3];          // projected light colour
			uint8_t pad;
			int32_t light_dist;      // projector distance
		} overlay;
		struct Sparkle
		{
			uint32_t seq;            // sprite sequence
			fxp::Window window;      // ring ticks it spawns them (one per step)
			fxp::Range3 speed;       // per tick (y down)
			int16_t frames;          // last flipbook frame: lifetime
		} sparkle;
		struct Mote
		{
			uint32_t seq;            // sprite sequence
			fxp::Window window;      // ring ticks it spawns them
			fxp::Schedule count;     // how many per tick
			fxp::Scaled radius;      // circle: of the target's bounds radius
			fxp::Range angle;        // place on the circle
			fxp::RandomFraction drop;    // below the ring: this share of its height over the target
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
	// the game's values but the sprite sequences, each module's own (game_values)
	inline const HealRingParams DEFAULTS = {
		{ 0, 0, 0xF },
		{ 0, { 4, 0x80 }, { 2, 1, 0x400 }, { 0x100, 20 }, { 0, 0x14, 4 } },
		{ 0, 0x14, { 0x3C, 3 } },
		{ { 0x200, 0x180, 0x140, 0xF1, 0x80, 0x80 }, { 0x80, 0x80, 0x80 }, 0, 0x2000 },
		{ 0, { 1, 0x10 }, { { -0x10, 0xF }, { 0x10, 0x1F }, { -0x10, 0xF } }, 8 },
		{ 0, { 2, 0x7FFE }, { { 2, 7, 9, 11 }, { 1, 2, 3, 2 } }, { 3, 2, 0x400 }, { 0, 0xFFF }, { 0, 0x1F, 48 }, { 8, 0x17 }, 0x10 },
		{ { 4, 10 }, { { 4, 7, 9, 11 }, { 2, 3, 4, 2 } }, { 8, { 0x100, 0x10 }, { 0x40, 0x10 }, { 0x10, 0x10, 0x10 }, { 0, 0, 0x10 }, 0x40 } },
	};

	// a module's sprite sequences (Cure's and Potion's copies lie apart in the exe)
	struct Sequences { uint32_t ring, ring_end, glow, sparkle, mote, sound; };
	inline HealRingParams game_values(const Sequences &s)
	{
		HealRingParams p = DEFAULTS;
		p.ring.seq = s.ring;
		p.ring.ending.seq = s.ring_end;
		p.glow.seq = s.glow;
		p.sparkle.seq = s.sparkle;
		p.mote.seq = s.mote;
		p.emitter.sound = s.sound;
		return p;
	}

#define HEAL_RING_FIELD(part, member, label, doc, type, lo, hi) \
	{ part, #member, label, doc, (uint32_t)offsetof(HealRingParams, member), fxp::type, lo, hi }
#define HEAL_RING_UNIT(part, member, label, doc, type, lo, hi, unit) \
	{ part, #member, label, doc, (uint32_t)offsetof(HealRingParams, member), fxp::type, lo, hi, fxp::unit }
	inline const fxp::Part PARTS[] = {
		{ 0, "emitter", "Emitter", "The cast on one target: when its sound plays and its HP are given (draws nothing)" },
		{ PART_RING, "ring", "Ring", "The light climbing in a helix around the target, in steps; it spawns the sparkles, motes and glints" },
		{ PART_GLOW, "glow", "Ground glow", "An additive glow on the ground under the ring, fading out" },
		{ PART_OVERLAY, "overlay", "Overlay", "The target redrawn lit by an image projected from the ring" },
		{ PART_SPARKLES, "sparkles", "Sparkles", "Flipbooks born at each step of the ring, drifting out and down" },
		{ PART_MOTES, "motes", "Motes", "Flipbooks rising on a circle around the target" },
		{ PART_GLINTS, "glints", "Glints", "The shared effect library's glints: streaks at the target's centre (they never move), growing then shrinking, spinning" },
	};
	inline const fxp::Group GROUPS[] = {
		{ "ring.steps", "Steps", "step n = ring tick x Per tick + k (k = 0 .. Per tick - 1); its angle = n x Angle. Each ring tick the ring goes through its Per tick steps and is drawn at the last one; a sparkle is born at every step. It turns Per tick x Angle per tick" },
		{ "ring.radius", "Radius", "radius = target's radius x Times / Divided by (whole numbers), then at most At most. At step n the ring is at centre + (sin(angle) x radius, height, cos(angle) x radius)" },
		{ "ring.climb", "Climb speed", "rise per step = ((target's height + Target height +) / Every) / Per tick (whole numbers, rounded toward 0). The ring starts at the target's feet and rises at this speed until it ends (its ending flipbook done, its sparkles and motes gone): after Every ticks it is Target height + over the target's top, then it goes on" },
		{ "ring.ending", "Ending flipbook", "From ring tick From tick the ring plays this sequence, one frame per tick, up to Last frame, then ends once its sparkles and motes are gone: it lives about From tick + Last frame + 1 ticks" },
		{ "glow.brightness", "Brightness", "grey = (Start - ring tick x Lost per tick), kept to 0-255 by wrapping (below 0 it starts again from 255); additive: it adds grey to what is behind, 0 adds nothing" },
		{ "overlay.image", "Projected image", "The target is redrawn with this image projected from the ring (a slide projector): a target point seen from the ring at screen (x, y) takes texel image corner + (x - 160, y - 120) + window / 2, inside the Width x Height window, with this palette" },
		{ "overlay.rgb", "Light colour", "redrawn colour = texel x this / 128 (128 = the image as it is)" },
		{ "sparkle.window", "Born", "A sparkle is born at each ring step while From tick <= ring tick <= To tick: Per tick sparkles per ring tick" },
		{ "sparkle.speed", "Speed", "Rolled once at birth, x then z then y, each min + random % (max - min + 1); then position += speed every tick (y goes down: a positive y falls)" },
		{ "mote.window", "Born", "Motes are born while From tick <= ring tick <= To tick" },
		{ "mote.count", "How many", "Motes per ring tick = the count of the last rate whose from tick <= ring tick" },
		{ "mote.radius", "Circle", "circle radius = target's radius x Times / Divided by (whole numbers), then at most At most; centred on the target" },
		{ "mote.angle", "Place on the circle", "angle = min + random % (max - min + 1) (4096 = a full turn), rolled at birth" },
		{ "mote.drop", "Drop", "Born below the ring by (the ring's height over the target's feet) x (min + random % (max - min + 1)) / Divided by, rolled at birth" },
		{ "mote.rise", "Rise speed", "Rises min + random % (max - min + 1) every tick, rolled once (its first tick)" },
		{ "glint.window", "Born", "Glints are born while From tick <= ring tick <= To tick, at the target's centre (every tick they go back there: the middle of its line anchor, at the height of its bounds centre)" },
		{ "glint.count", "How many", "Glints per ring tick = the count of the last rate whose from tick <= ring tick" },
		{ "glint.style.grow", "Growth", "width, height += this every tick while growing" },
		{ "glint.style.shrink", "Shrinking", "width, height -= this every tick while shrinking" },
		{ "glint.style.centre_step", "Centre colour step", "centre colour += this every tick while growing, -= while shrinking (bytes wrap past 255)" },
		{ "glint.style.tip_step", "Tip colour step", "tip colour += this every tick while growing, -= while shrinking (bytes wrap past 255)" },
	};
	inline const fxp::Field FIELDS[] = {
		HEAL_RING_FIELD("emitter", emitter.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("emitter", emitter.sound_tick, "Sound tick", "The sound plays when the cast's tick == this", I16, 0, 200),
		HEAL_RING_FIELD("emitter", emitter.heal_tick, "Heal tick", "The HP are given once, at the first cast tick >= this", I16, 0, 200),
		HEAL_RING_FIELD("ring", ring.seq, "Sequence", "Sprite sequence of the climbing light (its frame 0, drawn at the ring's last step of each tick)", Seq, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("ring", ring.steps.per_tick, "Per tick", "Steps the ring takes each ring tick", I16, 1, 16),
		HEAL_RING_UNIT("ring", ring.steps.angle, "Angle", "angle of step n = n x this (4096 = a full turn)", I16, -2048, 2048, Angle),
		HEAL_RING_FIELD("ring", ring.radius.num, "Times", "radius = target's radius x this...", I16, 0, 32),
		HEAL_RING_FIELD("ring", ring.radius.den, "Divided by", "... / this (whole numbers)...", I16, 1, 32),
		HEAL_RING_FIELD("ring", ring.radius.max, "At most", "...then never more than this (game units)", I16, 0, 0x2000),
		HEAL_RING_FIELD("ring", ring.climb.above, "Target height +", "The ring rises (target's height + this) every 'Every' ticks", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("ring", ring.climb.ticks, "Every (ticks)", "A speed, not a duration: the ring keeps rising until it ends", I16, 1, 200),
		HEAL_RING_FIELD("ring", ring.ending.seq, "Sequence", "Sprite sequence of the ending flipbook", Seq, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("ring", ring.ending.tick, "From tick", "The ring switches to it at this ring tick", I16, 1, 200),
		HEAL_RING_FIELD("ring", ring.ending.frames, "Last frame", "One frame per tick up to this one, then the ring ends", I16, 0, 64),
		HEAL_RING_FIELD("glow", glow.seq, "Sequence", "Its frame 0, lying flat on the ground (y = 0) under the ring", Seq, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("glow", glow.ticks, "Duration", "Drawn while ring tick < this", I16, 0, 200),
		HEAL_RING_FIELD("glow", glow.brightness.start, "Start", "grey at ring tick 0", U8, 0, 255),
		HEAL_RING_FIELD("glow", glow.brightness.fade, "Lost per tick", "grey taken away every ring tick", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.image.tex_x, "VRAM x", "The image's corner in video memory", I16, 0, 1023),
		HEAL_RING_FIELD("overlay", overlay.image.tex_y, "VRAM y", "", I16, 0, 511),
		HEAL_RING_FIELD("overlay", overlay.image.clut_x, "Palette x", "Its palette in video memory", I16, 0, 1023),
		HEAL_RING_FIELD("overlay", overlay.image.clut_y, "Palette y", "", I16, 0, 511),
		HEAL_RING_FIELD("overlay", overlay.image.width, "Width", "The projected window's size, centred on the ring's view of the target", I16, 1, 255),
		HEAL_RING_FIELD("overlay", overlay.image.height, "Height", "", I16, 1, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[0], "Red", "", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[1], "Green", "", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.rgb[2], "Blue", "", U8, 0, 255),
		HEAL_RING_FIELD("overlay", overlay.light_dist, "Projector distance", "The target is seen from the ring at this depth (farther: it covers less of the image)", I32, 0, 0x10000),
		HEAL_RING_FIELD("sparkles", sparkle.seq, "Sequence", "Sprite sequence", Seq, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("sparkles", sparkle.window.first, "From tick", "", I16, 0, 200),
		HEAL_RING_FIELD("sparkles", sparkle.window.last, "To tick", "", I16, 0, 200),
		HEAL_RING_FIELD("sparkles", sparkle.speed.x.min, "x min", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.speed.x.max, "x max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.speed.y.min, "y min", "y goes down", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.speed.y.max, "y max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.speed.z.min, "z min", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.speed.z.max, "z max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("sparkles", sparkle.frames, "Lifetime (frames)", "One frame per tick from frame 0; after this frame it disappears: it lives this + 1 ticks (at most its sequence's last frame)", I16, 0, 64),
		HEAL_RING_FIELD("motes", mote.seq, "Sequence", "Sprite sequence", Seq, 0, 0x7FFFFFFF),
		HEAL_RING_FIELD("motes", mote.window.first, "From tick", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.window.last, "To tick", "32766: as long as the ring lives", I16, 0, 0x7FFE),
		HEAL_RING_FIELD("motes", mote.count.from[0], "1: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[0], "1: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[1], "2: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[1], "2: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[2], "3: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[2], "3: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.count.from[3], "4: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("motes", mote.count.count[3], "4: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("motes", mote.radius.num, "Times", "The target's radius times this...", I16, 0, 32),
		HEAL_RING_FIELD("motes", mote.radius.den, "Divided by", "...divided by this...", I16, 1, 32),
		HEAL_RING_FIELD("motes", mote.radius.max, "At most", "...never more than this (game units)", I16, 0, 0x2000),
		HEAL_RING_UNIT("motes", mote.angle.min, "Min", "", I16, 0, 4095, Angle),
		HEAL_RING_UNIT("motes", mote.angle.max, "Max", "", I16, 0, 4095, Angle),
		HEAL_RING_FIELD("motes", mote.drop.min, "Min", "", I16, 0, 255),
		HEAL_RING_FIELD("motes", mote.drop.max, "Max", "", I16, 0, 255),
		HEAL_RING_FIELD("motes", mote.drop.den, "Divided by", "", I16, 1, 1000),
		HEAL_RING_FIELD("motes", mote.rise.min, "Min", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("motes", mote.rise.max, "Max", "", I16, -0x400, 0x400),
		HEAL_RING_FIELD("motes", mote.frames, "Lifetime (frames)", "One frame per tick from frame 0; after this frame it disappears: it lives this + 1 ticks (at most its sequence's last frame)", I16, 0, 64),
		HEAL_RING_FIELD("glints", glint.window.first, "From tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.window.last, "To tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.from[0], "1: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[0], "1: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[1], "2: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[1], "2: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[2], "3: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[2], "3: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.count.from[3], "4: from tick", "", I16, 0, 200),
		HEAL_RING_FIELD("glints", glint.count.count[3], "4: per tick", "", I16, 0, 16),
		HEAL_RING_FIELD("glints", glint.style.ticks, "Ticks per phase", "Grows for this many ticks, then shrinks for as many: it lives 2 x this ticks", I16, 1, 100),
		HEAL_RING_FIELD("glints", glint.style.grow.w, "Width", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.grow.h, "Height", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.shrink.w, "Width", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.shrink.h, "Height", "", I16, -0x1000, 0x1000),
		HEAL_RING_FIELD("glints", glint.style.centre_step[0], "Red", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.centre_step[1], "Green", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.centre_step[2], "Blue", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[0], "Red", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[1], "Green", "", U8, 0, 255),
		HEAL_RING_FIELD("glints", glint.style.tip_step[2], "Blue", "", U8, 0, 255),
		HEAL_RING_UNIT("glints", glint.style.spin, "Spin per tick", "Turn about the vertical axis every tick, + or - this, one random roll per glint (4096 = a full turn)", I16, -2048, 2048, Angle),
	};
#undef HEAL_RING_FIELD
#undef HEAL_RING_UNIT

	// a module's live parameters and game values under its effect id (for the tools: fx_params.h)
	inline void register_params(int effect_id, const char *name, HealRingParams *live, const HealRingParams *game)
	{
		fxp::register_params({ effect_id, name, live, game, sizeof(HealRingParams),
		                       PARTS, (int)(sizeof(PARTS) / sizeof(PARTS[0])),
		                       FIELDS, (int)(sizeof(FIELDS) / sizeof(FIELDS[0])),
		                       GROUPS, (int)(sizeof(GROUPS) / sizeof(GROUPS[0])) });
	}
}
}
