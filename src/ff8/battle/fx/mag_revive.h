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

// The revive module (effects 5, Phoenix Down, and 35, Life: one source built twice - Phoenix
// Down's code is Life's relocated, its data byte-identical; only the setup differs: Phoenix Down
// plays no camera), written once from the parts library (fx_revive.h, fx_parts.h). A `Layout` says
// where one copy is (its original task functions, its exe data, its memory).
//
//   Master - packet cursor = one of the two arenas by tick parity, follows the caster's bones,
//     phases: 0 on, 1 the emitter of the current action (holds itself), 2 the next action once the
//     emitter released it (back to 1) or on, 3 done when no task is alive; runs the four queues
//     (emitters, an unused one, sparkles, glints); ends when nothing is left.
//   Emitter, one per action (target 0 of the action): bone-follow anchor and model bounds; phases
//     animate its spawn parameters (radius, height spread, count, the sparkles' size ramp, the
//     ring's height): 0 sets them, spawning; 1 grows (until tick 10); 2 spreads (until 20); 3 fades
//     the size (until 30), then the result (the revive), releases the master, spawning off, done.
//     While spawning, every tick: rising sparkles (up to 30), revive sparkles (0, then even ticks
//     12..20), a ring sparkle (11..29), a falling sparkle (21..36), glints (4..15: 1 / 2 / 3 / 1).
//     Its sound at its first tick.
//   Sparkle - a flipbook (next frame a tick, done after the last) its size + a step a tick.
//   Ring sparkle - orbits its centre (phase + 0x80 a tick, radius 4096 / its divisor), rises 0x40 a
//     tick; its size + a step.
//   Falling sparkle - falls by its speed a tick, size 4096.
//   Glints - the effect library's (Effect_Glint_Tick 0x8DCC20), at the target's bounds centre.
// No task tests the draw-only flags (battle_to_update_flags 0x201). The setup and the file loader
// are not ported (they run from the exe). Parameters (ReviveParams, fx_params.h): one struct, parts
// and field table, each module with its own live values; defaults = the game's.

#pragma once

#include <string>

#include "mag_common.h"
#include "fx_params.h"
#include "fx_parts.h"
#include "fx_revive.h"

namespace ff8fx
{
namespace revive
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;
	using namespace parts;

	struct Layout
	{
		// its original task functions (the ports replace them)
		uint32_t master, emitter, fall, ring, sparkle;
		// its data: the sound block, the sparkles' sequences
		uint32_t sound, seq_revive, seq_rise_a, seq_rise_b, seq_ring, seq_fall;
		// its memory: the packet cursor, the arena words (odd ticks, even ticks), the queues
		uint32_t packet_cursor, arena_odd, arena_even, q_emitter, q_unused, q_sprite, q_glint;
	};

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct ReviveParams
	{
		struct Emitter
		{
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick, sound_pan;
			struct { int16_t start, end; } size;   // the sparkles' size ramp at the start (from start towards end)
			struct { int16_t ticks, size, radius; } grow;   // until this tick: the start size and the radius up by these a tick
			struct { int16_t ticks, radius, spread, ring, end; } spread;   // until this tick: the radius down, the spread up, the ring down; then the end size this
			struct { int16_t ticks, size; } fade;  // until this tick the start size down by this a tick; then the result
		} emitter;
		struct Rising
		{
			int16_t last, count;    // up to this emitter tick, this many a tick
			int16_t frames, shade;  // their flipbook's last frame, their camera offset
			int16_t lift, steps;    // below the disc by lift + rand % the spread; their size ramp in this many steps
		} rising;
		struct ReviveSparks
		{
			int16_t first;          // one at the anchor at this emitter tick...
			int16_t from, last, every;   // ...and one scattered every this many ticks over these
			int16_t frames, shade;
		} revive;
		struct Ring
		{
			int16_t from, last;     // one a tick over these emitter ticks
			int16_t frames;
			struct { int16_t sub, div, add; } divisor;   // its radius 4096 / ((emitter tick - sub) / div + add)
			int16_t spin, rise;     // its phase up by spin a tick, y down by rise
		} ring;
		struct Fall
		{
			int16_t from, last;     // one a tick over these emitter ticks
			int16_t frames;
			int16_t radius_div;     // on a circle of radius 4096 / this round the anchor
			struct { int16_t base, mask; } lift;    // above it by base + (rand & mask)
			struct { int16_t base, mask; } speed;   // falling base + (rand & mask) a tick
		} fall;
		struct Glints
		{
			int16_t from, last;     // over these emitter ticks...
			struct { int16_t from, count; } rate[4];   // ...the count of the last rate whose from tick <= the tick
			int16_t ticks;          // growing this many ticks, then shrinking
			struct { int16_t w, h; } grow, shrink;
			uint8_t centre[3], tip[3];   // their colours' steps a tick
			int16_t spin;
		} glints;
	};
	enum PartId { PART_SPARKLES = 1, PART_RING = 2, PART_FALL = 3, PART_GLINTS = 4 };

	namespace table
	{
		using fxp::Field;
#define REVIVE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(ReviveParams, member), fxp::type, lo, hi }
#define REVIVE_I16(part, member, label, doc, lo, hi) REVIVE_FIELD(part, member, label, doc, I16, lo, hi)
		static const fxp::Part PARTS[] = {
			{ 0, "emitter", "Emitter", "One per action, on its target: its spawn parameters animated by phases, its sound, the result (draws nothing)" },
			{ PART_SPARKLES, "sparkles", "Sparkles", "Flipbooks rising from a disc round the target, and revive sparkles scattered above it" },
			{ PART_RING, "ring", "Ring", "Flipbooks orbiting up a ring round the target" },
			{ PART_FALL, "fall", "Falling sparkles", "Flipbooks falling round the target" },
			{ PART_GLINTS, "glints", "Glints", "The shared effect library's glints at the target's centre, growing then shrinking, spinning" },
		};
		static const fxp::Group GROUPS[] = {
			{ "emitter.size", "Sparkle size", "The sparkles' size ramp at the start: from Start towards End" },
			{ "emitter.grow", "Grow", "Until emitter tick Ticks: the start size up by Size a tick, the radius by Radius" },
			{ "emitter.spread", "Spread", "Until emitter tick Ticks: the radius down by Radius, the height spread up by Spread, the ring's height down by Ring; then the end size End" },
			{ "emitter.fade", "Fade", "Until emitter tick Ticks: the start size down by Size a tick; then the result" },
			{ "ring.divisor", "Radius", "Its orbit's radius 4096 / ((emitter tick - Sub) / Div + Add)" },
			{ "fall.lift", "Above", "Above the anchor by Base + (rand & Mask)" },
			{ "fall.speed", "Speed", "Falling Base + (rand & Mask) a tick" },
			{ "glints.rate", "How many", "Glints per tick = the count of the last rate whose from tick <= the emitter tick" },
			{ "glints.grow", "Growth", "width, height += this every tick while growing" },
			{ "glints.shrink", "Shrinking", "width, height -= this every tick while shrinking" },
			{ "glints.centre", "Centre colour step", "centre colour += this every tick while growing, -= while shrinking" },
			{ "glints.tip", "Tip colour step", "tip colour += this every tick while growing, -= while shrinking" },
		};
		static const Field FIELDS[] = {
			REVIVE_FIELD("emitter", emitter.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			REVIVE_I16("emitter", emitter.sound_tick, "Sound tick", "The sound plays when the emitter tick == this", 0, 200),
			REVIVE_I16("emitter", emitter.sound_pan, "Sound pan", "Where it plays between the speakers: 128 the middle (BdPlaySE's pan)", 0, 255),
			REVIVE_I16("emitter", emitter.size.start, "Start", "", 0, 0x7FFF),
			REVIVE_I16("emitter", emitter.size.end, "End", "", 0, 0x7FFF),
			REVIVE_I16("emitter", emitter.grow.ticks, "Ticks", "", 0, 200),
			REVIVE_I16("emitter", emitter.grow.size, "Size", "", -0x4000, 0x4000),
			REVIVE_I16("emitter", emitter.grow.radius, "Radius", "", -0x4000, 0x4000),
			REVIVE_I16("emitter", emitter.spread.ticks, "Ticks", "", 0, 200),
			REVIVE_I16("emitter", emitter.spread.radius, "Radius", "", -0x4000, 0x4000),
			REVIVE_I16("emitter", emitter.spread.spread, "Spread", "", -0x4000, 0x4000),
			REVIVE_I16("emitter", emitter.spread.ring, "Ring", "", -0x4000, 0x4000),
			REVIVE_I16("emitter", emitter.spread.end, "End", "", 0, 0x7FFF),
			REVIVE_I16("emitter", emitter.fade.ticks, "Ticks", "", 0, 200),
			REVIVE_I16("emitter", emitter.fade.size, "Size", "", -0x4000, 0x4000),
			REVIVE_I16("sparkles", rising.last, "Rising until", "Rising sparkles up to this emitter tick...", 0, 200),
			REVIVE_I16("sparkles", rising.count, "Rising a tick", "...this many a tick", 0, 50),
			REVIVE_I16("sparkles", rising.frames, "Rising last frame", "Their flipbook's last frame", 0, 100),
			REVIVE_I16("sparkles", rising.shade, "Rising shade", "Their camera offset", -0x4000, 0x4000),
			REVIVE_I16("sparkles", rising.lift, "Rising below", "Below the disc by this + rand % the height spread", -0x4000, 0x4000),
			REVIVE_I16("sparkles", rising.steps, "Rising steps", "Their size ramp in this many steps", 1, 100),
			REVIVE_I16("sparkles", revive.first, "Revive first", "A revive sparkle at the anchor at this emitter tick", -1, 200),
			REVIVE_I16("sparkles", revive.from, "Revive from", "Then one scattered from this emitter tick...", 0, 200),
			REVIVE_I16("sparkles", revive.last, "Revive last", "...to this one...", 0, 200),
			REVIVE_I16("sparkles", revive.every, "Revive every", "...every this many ticks", 1, 100),
			REVIVE_I16("sparkles", revive.frames, "Revive last frame", "Their flipbook's last frame", 0, 100),
			REVIVE_I16("sparkles", revive.shade, "Revive shade", "Their camera offset", -0x4000, 0x4000),
			REVIVE_I16("ring", ring.from, "From tick", "A ring sparkle a tick from this emitter tick...", 0, 200),
			REVIVE_I16("ring", ring.last, "Last tick", "...to this one", 0, 200),
			REVIVE_I16("ring", ring.frames, "Last frame", "Its flipbook's last frame", 0, 100),
			REVIVE_I16("ring", ring.divisor.sub, "Sub", "", -200, 200),
			REVIVE_I16("ring", ring.divisor.div, "Div", "", 1, 100),
			REVIVE_I16("ring", ring.divisor.add, "Add", "", -100, 100),
			REVIVE_I16("ring", ring.spin, "Spin", "Its phase up by this a tick (4096: a full turn)", -0x1000, 0x1000),
			REVIVE_I16("ring", ring.rise, "Rise", "Its height up by this a tick", -0x4000, 0x4000),
			REVIVE_I16("fall", fall.from, "From tick", "A falling sparkle a tick from this emitter tick...", 0, 200),
			REVIVE_I16("fall", fall.last, "Last tick", "...to this one", 0, 200),
			REVIVE_I16("fall", fall.frames, "Last frame", "Its flipbook's last frame", 0, 100),
			REVIVE_I16("fall", fall.radius_div, "Circle", "On a circle of radius 4096 / this round the anchor", 1, 0x1000),
			REVIVE_I16("fall", fall.lift.base, "Base", "", -0x4000, 0x4000),
			REVIVE_I16("fall", fall.lift.mask, "Mask", "", 0, 0x7FFF),
			REVIVE_I16("fall", fall.speed.base, "Base", "", -0x4000, 0x4000),
			REVIVE_I16("fall", fall.speed.mask, "Mask", "", 0, 0x7FFF),
			REVIVE_I16("glints", glints.from, "From tick", "Glints from this emitter tick...", 0, 200),
			REVIVE_I16("glints", glints.last, "Last tick", "...to this one", 0, 200),
			REVIVE_I16("glints", glints.rate[0].from, "From tick", "", 0, 200),
			REVIVE_I16("glints", glints.rate[0].count, "Count", "", 0, 50),
			REVIVE_I16("glints", glints.rate[1].from, "From tick", "", 0, 200),
			REVIVE_I16("glints", glints.rate[1].count, "Count", "", 0, 50),
			REVIVE_I16("glints", glints.rate[2].from, "From tick", "", 0, 200),
			REVIVE_I16("glints", glints.rate[2].count, "Count", "", 0, 50),
			REVIVE_I16("glints", glints.rate[3].from, "From tick", "", 0, 200),
			REVIVE_I16("glints", glints.rate[3].count, "Count", "", 0, 50),
			REVIVE_I16("glints", glints.ticks, "Growing ticks", "A glint grows this many ticks, then shrinks", 0, 200),
			REVIVE_I16("glints", glints.grow.w, "Width", "", -0x4000, 0x4000),
			REVIVE_I16("glints", glints.grow.h, "Height", "", -0x4000, 0x4000),
			REVIVE_I16("glints", glints.shrink.w, "Width", "", -0x4000, 0x4000),
			REVIVE_I16("glints", glints.shrink.h, "Height", "", -0x4000, 0x4000),
			REVIVE_FIELD("glints", glints.centre[0], "Red", "", U8, 0, 255),
			REVIVE_FIELD("glints", glints.centre[1], "Green", "", U8, 0, 255),
			REVIVE_FIELD("glints", glints.centre[2], "Blue", "", U8, 0, 255),
			REVIVE_FIELD("glints", glints.tip[0], "Red", "", U8, 0, 255),
			REVIVE_FIELD("glints", glints.tip[1], "Green", "", U8, 0, 255),
			REVIVE_FIELD("glints", glints.tip[2], "Blue", "", U8, 0, 255),
			REVIVE_I16("glints", glints.spin, "Spin", "It turns this much a tick, either way at random", -0x1000, 0x1000),
		};
#undef REVIVE_I16
#undef REVIVE_FIELD
	}

	template<const Layout &L>
	struct Revive
	{
		// the game's values (with this copy's own sound) and the live ones the code reads
		inline static const ReviveParams GAME = {
			{ L.sound, 0, 0x80, { 0x100, 0x1000 }, { 10, 0x180, 0x66 }, { 20, 0x4C, 0x40, 0x26, 0x10 }, { 30, 0xFF } },
			{ 30, 1, 0xF, -0x200, 0x100, 16 },
			{ 0, 12, 20, 2, 2, -0x400 },
			{ 11, 29, 0xF, { 10, 3, 8 }, 0x80, 0x40 },
			{ 21, 36, 9, 8, { 0x80, 0x1FF }, { 0x10, 0xF } },
			{ 4, 15, { { 4, 1 }, { 6, 2 }, { 11, 3 }, { 15, 1 } }, 8, { 0x100, 0x10 }, { 0x40, 0x10 }, { 0x10, 0x10, 0x10 }, { 0x10, 5, 8 }, 0x40 },
		};
		inline static ReviveParams live = GAME;
		static const ReviveParams &P() { return live; }
		static uint32_t &PacketCursor() { return var<uint32_t>(L.packet_cursor); }

		template<typename T> static T &N(uint8_t *n, uint32_t off) { return *(T *)(n + off); }
		static uint32_t End(uint8_t *n, uint8_t status)
		{
			if ((status & 1) != 0 && n[0x28] == 0)
			{
				ReleaseLinkedTask(n);
				return TASK_END;
			}
			return 0;
		}

		// Master: the arena, the bones, its phases, the four queues (live tasks -> +0x5E)
		static uint32_t __cdecl Master(TaskNode *t)
		{
			uint8_t *n = (uint8_t *)t;
			PacketCursor() = var<uint32_t>((n[0x5C] & 1) != 0 ? L.arena_odd : L.arena_even);
			UpdateTargetPosFromBones(n);
			switch ((int8_t)n[0x29])
			{
			case 0: n[0x29]++; break;
			case 1: // the emitter of the current action, the master held
				n[0x63] = 1;
				AddEffectTask((TaskQueue *)L.q_emitter, L.emitter, 0x64, n);
				n[0x29]++;
				break;
			case 2: // once released: the next action (back to 1) or on
				if (n[0x63] == 0)
				{
					const int8_t action = (int8_t)n[0x2A];
					if ((int16_t)action < N<int16_t>(n, 0x58))
					{
						n[0x2A] = (uint8_t)(action + 1);
						n[0x29]--;
					}
					else n[0x29]++;
				}
				break;
			case 3: // done once no task is alive
				if (N<uint16_t>(n, 0x5E) == 0)
				{
					n[0x26] |= 1;
					n[0x29]++;
				}
				break;
			default: break;
			}
			N<uint16_t>(n, 0x5E) = (uint16_t)ExecuteTaskQueue((TaskQueue *)L.q_emitter);
			N<uint16_t>(n, 0x5E) = (uint16_t)(N<uint16_t>(n, 0x5E) + (uint16_t)ExecuteTaskQueue((TaskQueue *)L.q_unused));
			N<uint16_t>(n, 0x5E) = (uint16_t)(N<uint16_t>(n, 0x5E) + (uint16_t)ExecuteTaskQueue((TaskQueue *)L.q_sprite));
			FX_PART(PART_GLINTS);
			N<uint16_t>(n, 0x5E) = (uint16_t)(N<uint16_t>(n, 0x5E) + (uint16_t)ExecuteTaskQueue((TaskQueue *)L.q_glint));
			FX_PART(0);
			const uint8_t status = n[0x26];
			N<uint16_t>(n, 0x5C) = (uint16_t)(N<uint16_t>(n, 0x5C) + 1);
			N<uint16_t>(n, 0x24) = (uint16_t)(N<uint16_t>(n, 0x24) + 1);
			return End(n, status);
		}

		// the emitter's phases (its spawn parameters)
		static void EmitterPhase(uint8_t *n)
		{
			const ReviveParams::Emitter &e = P().emitter;
			const int16_t tick = N<int16_t>(n, 0x24);
			switch ((int8_t)n[0x29])
			{
			case 0: // spawning; radius, spread, ring height 0, count, the size ramp
				n[0x26] |= 2;
				N<uint16_t>(n, RE_RADIUS) = 0;
				N<uint16_t>(n, RE_SPREAD) = 0;
				N<uint16_t>(n, RE_RING) = 0;
				N<uint16_t>(n, RE_COUNT) = (uint16_t)P().rising.count;
				N<uint16_t>(n, RE_SCALE_START) = (uint16_t)e.size.start;
				N<uint16_t>(n, RE_SCALE_END) = (uint16_t)e.size.end;
				n[0x29]++;
				break;
			case 1:
				N<uint16_t>(n, RE_SCALE_START) = (uint16_t)(N<uint16_t>(n, RE_SCALE_START) + e.grow.size);
				N<uint16_t>(n, RE_RADIUS) = (uint16_t)(N<uint16_t>(n, RE_RADIUS) + e.grow.radius);
				if (tick > e.grow.ticks) n[0x29]++;
				break;
			case 2:
				N<uint16_t>(n, RE_RADIUS) = (uint16_t)(N<uint16_t>(n, RE_RADIUS) - e.spread.radius);
				N<uint16_t>(n, RE_SPREAD) = (uint16_t)(N<uint16_t>(n, RE_SPREAD) + e.spread.spread);
				N<uint16_t>(n, RE_RING) = (uint16_t)(N<uint16_t>(n, RE_RING) - e.spread.ring);
				if (tick > e.spread.ticks)
				{
					N<uint16_t>(n, RE_COUNT) = (uint16_t)P().rising.count;
					N<uint16_t>(n, RE_SCALE_END) = (uint16_t)e.spread.end;
					n[0x29]++;
				}
				break;
			case 3:
				N<uint16_t>(n, RE_SCALE_START) = (uint16_t)(N<uint16_t>(n, RE_SCALE_START) - e.fade.size);
				if (tick <= e.fade.ticks) break;
				N<uint8_t>(N<uint8_t *>(n, 0x10), 0x63) = 0;   // the master goes on
				ApplyActionResultToTarget(N<CastContext *>(n, 0xC)->actions[(int8_t)n[0x2A]].targets);
				N<uint16_t>(n, 0x26) = (uint16_t)((N<uint16_t>(n, 0x26) & 0xFFFD) | 1);
				n[0x29]++;
				break;
			default: break;
			}
		}

		static void EmitterSpawns(uint8_t *n)
		{
			const ReviveParams &pp = P();
			const int16_t t = N<int16_t>(n, 0x24);
			TaskQueue *const q = (TaskQueue *)L.q_sprite;
			const ReviveParams::Rising &r = pp.rising;
			if (t <= r.last && N<int16_t>(n, RE_COUNT) > 0)
			{
				const ReviveRising rising = { L.seq_rise_a, L.seq_rise_b, r.frames, r.shade, r.lift, r.steps };
				for (int32_t i = 0; i < N<int16_t>(n, RE_COUNT); i++) ReviveRisingSpawn(q, L.sparkle, n, rising);
			}
			const ReviveParams::ReviveSparks &v = pp.revive;
			if (t == v.first) ReviveSparkleAt(q, L.sparkle, n, L.seq_revive, v.frames, v.shade);
			if (t >= v.from && t <= v.last && (t - v.from) % v.every == 0) ReviveScatterSpawn(q, L.sparkle, n, L.seq_revive, v.frames, v.shade);
			const ReviveParams::Ring &g = pp.ring;
			if (t >= g.from && t <= g.last) ReviveRingSpawn(q, L.ring, n, L.seq_ring, g.frames, g.divisor.sub, g.divisor.div, g.divisor.add);
			const ReviveParams::Fall &f = pp.fall;
			if (t >= f.from && t <= f.last) ReviveFallSpawn(q, L.fall, n, L.seq_fall, f.frames, f.radius_div, f.lift.base, f.lift.mask, f.speed.base, f.speed.mask);
			const ReviveParams::Glints &gl = pp.glints;
			if (t >= gl.from && t <= gl.last)
			{
				fxp::Schedule schedule;
				for (int k = 0; k < 4; k++)
				{
					schedule.from[k] = gl.rate[k].from;
					schedule.count[k] = gl.rate[k].count;
				}
				fxp::GlintStyle style;
				style.ticks = gl.ticks;
				style.grow = { gl.grow.w, gl.grow.h };
				style.shrink = { gl.shrink.w, gl.shrink.h };
				for (int k = 0; k < 3; k++)
				{
					style.centre_step[k] = gl.centre[k];
					style.tip_step[k] = gl.tip[k];
				}
				style.spin = gl.spin;
				for (int32_t m = schedule.at(t); m > 0; m--) parts::SpawnGlint((TaskQueue *)L.q_glint, n, L.packet_cursor, style);
			}
		}

		// Emitter, one per action
		static uint32_t __cdecl Emitter(TaskNode *t)
		{
			uint8_t *n = (uint8_t *)t;
			const ReviveParams::Emitter &e = P().emitter;
			Emitter_UpdatePos(n);
			Emitter_ComputeModelBounds(n);
			EmitterPhase(n);
			if ((n[0x26] & 2) != 0) EmitterSpawns(n);
			if (N<int16_t>(n, 0x24) == e.sound_tick) BdPlaySE((const void *)fxp::sound(e.sound, L.sound), 0, e.sound_pan);
			const uint8_t status = n[0x26];
			N<uint16_t>(n, 0x24) = (uint16_t)(N<uint16_t>(n, 0x24) + 1);
			return End(n, status);
		}

		// the sparkles' phase 1: next flipbook frame, done after the last (phase 0 only goes on)
		static void Flipbook(uint8_t *n)
		{
			if (FlipbookExpired(N<int16_t>(n, RS_FRAME), N<int16_t>(n, RS_LAST), N<uint16_t>(n, 0x26)))
			{
				n[0x26] |= 1;
				n[0x29]++;
			}
		}

		// Falling sparkle: falls by its speed a tick, its size 4096
		static uint32_t __cdecl Fall(TaskNode *t)
		{
			uint8_t *n = (uint8_t *)t;
			switch ((int8_t)n[0x29])
			{
			case 0: n[0x29]++; break;
			case 1:
				N<uint16_t>(n, 0x1E) = (uint16_t)(N<uint16_t>(n, 0x1E) + N<uint16_t>(n, RS_SPEED));
				Flipbook(n);
				break;
			default: break;
			}
			FX_PART(PART_FALL);
			ReviveSparkleDraw(n, 0x1000, PacketCursor());
			FX_PART(0);
			const uint8_t status = n[0x26];
			N<uint16_t>(n, 0x24) = (uint16_t)(N<uint16_t>(n, 0x24) + 1);
			return End(n, status);
		}

		// Ring sparkle: orbits up its ring, its size + its step
		static uint32_t __cdecl Ring(TaskNode *t)
		{
			uint8_t *n = (uint8_t *)t;
			switch ((int8_t)n[0x29])
			{
			case 0: n[0x29]++; break;
			case 1:
				ReviveRingStep(n, P().ring.spin, P().ring.rise);
				Flipbook(n);
				break;
			default: break;
			}
			N<uint16_t>(n, RS_SCALE) = (uint16_t)(N<uint16_t>(n, RS_SCALE) + N<uint16_t>(n, RS_STEP));
			FX_PART(PART_RING);
			ReviveSparkleDraw(n, N<uint16_t>(n, RS_SCALE), PacketCursor());
			FX_PART(0);
			const uint8_t status = n[0x26];
			N<uint16_t>(n, 0x24) = (uint16_t)(N<uint16_t>(n, 0x24) + 1);
			return End(n, status);
		}

		// Sparkle (rising and revive): its flipbook, its size + its step
		static uint32_t __cdecl Sparkle(TaskNode *t)
		{
			uint8_t *n = (uint8_t *)t;
			switch ((int8_t)n[0x29])
			{
			case 0: n[0x29]++; break;
			case 1: Flipbook(n); break;
			default: break;
			}
			N<uint16_t>(n, RS_SCALE) = (uint16_t)(N<uint16_t>(n, RS_SCALE) + N<uint16_t>(n, RS_STEP));
			FX_PART(PART_SPARKLES);
			ReviveSparkleDraw(n, N<uint16_t>(n, RS_SCALE), PacketCursor());
			FX_PART(0);
			const uint8_t status = n[0x26];
			N<uint16_t>(n, 0x24) = (uint16_t)(N<uint16_t>(n, 0x24) + 1);
			return End(n, status);
		}

		// its parameters and its ports under `effect_id`, the ports named "<tag> Master"...
		static void register_module(int effect_id, const char *name, const char *tag)
		{
			static std::string names[5];
			const char *const what[5] = { " Master", " Emitter", " Fall", " Ring", " Sparkle" };
			for (int k = 0; k < 5; k++) names[k] = std::string(tag) + what[k];
			fxp::register_params({ effect_id, name, &live, &GAME, sizeof(ReviveParams),
			                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
			                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
			                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
			register_port(L.master, (void *)Master, names[0].c_str(), effect_id);
			register_port(L.emitter, (void *)Emitter, names[1].c_str(), effect_id);
			register_port(L.fall, (void *)Fall, names[2].c_str(), effect_id);
			register_port(L.ring, (void *)Ring, names[3].c_str(), effect_id);
			register_port(L.sparkle, (void *)Sparkle, names[4].c_str(), effect_id);
		}
	};
}
}
