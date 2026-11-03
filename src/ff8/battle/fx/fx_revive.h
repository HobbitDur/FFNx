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

// Parts library, the revive sparkles (Life 35 and Phoenix Down 5, one module: Phoenix Down's
// 0x8D4700..0x8D4E60): an emitter's spawn parameters (+0x58..), the sparkles it spawns (0x70-byte
// effect-library nodes) - rising from a disc, scattered above, orbiting up a ring, falling - their
// ring orbit and their draw. Bit-exact with Phoenix Down's code (mag005_phoenix_down.cpp before it
// was built from here).

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
	// a revive sparkle node's fields (0x70 bytes, after the effect-library header)
	enum ReviveSparkleField : uint32_t
	{
		RS_ANGLE = 0x42,   // ring / fall: phase angle
		RS_SEQ = 0x4C, RS_FRAME = 0x50, RS_LAST = 0x52, RS_SHADE = 0x54,
		RS_SPEED = 0x5A,   // fall: y speed
		RS_CENTRE = 0x60,  // ring: centre x, y, z (+0x60, +0x62, +0x64)
		RS_SCALE = 0x68, RS_STEP = 0x6A,   // its size and its step a tick
		RS_DIVISOR = 0x6C, // ring: radius = 4096 / this
	};
	// an emitter's spawn parameters (+0x58..)
	enum ReviveEmitterField : uint32_t
	{
		RE_RADIUS = 0x58, RE_SPREAD = 0x5A, RE_COUNT = 0x5C, RE_SCALE_START = 0x5E, RE_SCALE_END = 0x60, RE_RING = 0x62,
	};
	template<typename T> static inline T &RF(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	template<typename T> static inline T RF(const uint8_t *p, uint32_t off) { return *(const T *)(p + off); }

	// 0x8D4800: a sparkle under `parent` at its target's anchor x / z: sequence, last frame, camera
	// offset, size 4096
	inline uint8_t *ReviveSparkleAt(TaskQueue *q, uint32_t task, void *parent, uint32_t seq, int32_t last, int32_t shade)
	{
		uint8_t *s = (uint8_t *)parts::AddEffectTask(q, task, 0x70, parent);
		parts::CopyAnchorXZFromSource(s, (int16_t *)(s + 0x1C));
		RF<uint16_t>(s, RS_SHADE) = (uint16_t)shade;
		RF<uint32_t>(s, RS_SEQ) = seq;
		RF<uint16_t>(s, RS_LAST) = (uint16_t)last;
		RF<uint16_t>(s, RS_SCALE) = 0x1000;
		return s;
	}

	// 0x8D4850's loop body: a rising sparkle at a random point of the emitter's disc (its radius,
	// or half of it), below its anchor by lift + rand % its spread, its size from the emitter's start
	// size towards its end size in `steps` steps
	struct ReviveRising
	{
		uint32_t seq_a, seq_b;
		int32_t last, shade, lift, steps;
	};
	inline uint8_t *ReviveRisingSpawn(TaskQueue *q, uint32_t task, uint8_t *emitter, const ReviveRising &r)
	{
		int16_t count = RF<int16_t>(emitter, RE_SPREAD);
		if (count == 0) count = 1;
		const int32_t d = count;
		int16_t radius;
		if ((CrtRand() & 1) != 0) radius = (int16_t)(RF<int16_t>(emitter, RE_RADIUS) / 2);
		else radius = RF<int16_t>(emitter, RE_RADIUS);
		if (radius == 0) radius = 1;
		const uint32_t seq = (CrtRand() & 1) != 0 ? r.seq_a : r.seq_b;
		uint8_t *s = ReviveSparkleAt(q, task, emitter, seq, r.last, r.shade);
		const int32_t angle = (int32_t)(CrtRand() & 0xFFF);
		const int32_t dist = (int16_t)(CrtRand() % (int32_t)radius);
		RF<uint16_t>(s, RS_SCALE) = RF<uint16_t>(emitter, RE_SCALE_START);
		RF<uint16_t>(s, RS_STEP) = (uint16_t)((RF<int16_t>(emitter, RE_SCALE_END) - RF<int16_t>(emitter, RE_SCALE_START)) / r.steps);
		RF<uint16_t>(s, 0x1C) = (uint16_t)(RF<uint16_t>(s, 0x1C) + (uint16_t)(mul32(ComputeSin(angle), dist) / 4096));
		RF<uint16_t>(s, 0x20) = (uint16_t)(RF<uint16_t>(s, 0x20) + (uint16_t)(mul32(parts::ComputeCos(angle), dist) / 4096));
		const int32_t rem = CrtRand() % d;
		RF<uint16_t>(s, 0x1E) = (uint16_t)(RF<uint16_t>(s, 0x1E) + (uint16_t)(-r.lift - rem));
		return s;
	}

	// 0x8D4700's second half: a revive sparkle off the anchor by x / z within the emitter's radius / 2
	// (either side), above it by up to its spread
	inline uint8_t *ReviveScatterSpawn(TaskQueue *q, uint32_t task, uint8_t *emitter, uint32_t seq, int32_t last, int32_t shade)
	{
		uint8_t *s = ReviveSparkleAt(q, task, emitter, seq, last, shade);
		int16_t dx = (int16_t)((int32_t)(CrtRand() & 0x7FFF) % (RF<int16_t>(emitter, RE_RADIUS) / 2 + 1));
		const int16_t dy = (int16_t)((int32_t)(CrtRand() & 0x7FFF) % (RF<int16_t>(emitter, RE_SPREAD) + 1));
		int16_t dz = (int16_t)((int32_t)(CrtRand() & 0x7FFF) % (RF<int16_t>(emitter, RE_RADIUS) / 2 + 1));
		if ((CrtRand() & 1) != 0) dx = (int16_t)-dx;
		if ((CrtRand() & 1) != 0) dz = (int16_t)-dz;
		RF<uint16_t>(s, 0x1C) = (uint16_t)(RF<uint16_t>(s, 0x1C) + (uint16_t)dx);
		RF<uint16_t>(s, 0x1E) = (uint16_t)(RF<uint16_t>(s, 0x1E) - (uint16_t)dy);
		RF<uint16_t>(s, 0x20) = (uint16_t)(RF<uint16_t>(s, 0x20) + (uint16_t)dz);
		return s;
	}

	// 0x8D4980: a ring sparkle at the anchor raised by the emitter's ring height (its orbit's centre),
	// a random phase, its radius divisor (emitter tick - sub) / div + add
	inline uint8_t *ReviveRingSpawn(TaskQueue *q, uint32_t task, uint8_t *emitter, uint32_t seq, int32_t last, int32_t sub, int32_t div, int32_t add)
	{
		uint8_t *s = (uint8_t *)parts::AddEffectTask(q, task, 0x70, emitter);
		parts::CopyAnchorXZFromSource(s, (int16_t *)(s + 0x1C));
		RF<uint16_t>(s, 0x1E) = (uint16_t)(RF<uint16_t>(s, 0x1E) + RF<uint16_t>(emitter, RE_RING));
		RF<uint32_t>(s, RS_CENTRE) = RF<uint32_t>(s, 0x1C);
		RF<uint32_t>(s, RS_CENTRE + 4) = RF<uint32_t>(s, 0x20);
		RF<uint32_t>(s, RS_SEQ) = seq;
		RF<uint16_t>(s, RS_LAST) = (uint16_t)last;
		RF<uint16_t>(s, RS_SCALE) = 0x1000;
		RF<uint16_t>(s, RS_ANGLE) = (uint16_t)(CrtRand() & 0xFFF);
		RF<uint16_t>(s, RS_DIVISOR) = (uint16_t)((RF<int16_t>(emitter, 0x24) - sub) / div + add);
		return s;
	}

	// 0x8D4A10: a falling sparkle on a circle of radius 4096 / `radius_div` round the anchor, above it
	// by lift + (rand & lift_mask), falling speed + (rand & speed_mask)
	inline uint8_t *ReviveFallSpawn(TaskQueue *q, uint32_t task, uint8_t *emitter, uint32_t seq, int32_t last, int32_t radius_div, int32_t lift,
	                                int32_t lift_mask, int32_t speed, int32_t speed_mask)
	{
		uint8_t *s = (uint8_t *)parts::AddEffectTask(q, task, 0x70, emitter);
		RF<uint32_t>(s, RS_SEQ) = seq;
		RF<uint16_t>(s, RS_LAST) = (uint16_t)last;
		RF<uint16_t>(s, RS_SCALE) = 0x1000;
		RF<uint16_t>(s, RS_ANGLE) = (uint16_t)(CrtRand() & 0xFFF);
		parts::CopyAnchorXZFromSource(s, (int16_t *)(s + 0x1C));
		RF<uint16_t>(s, 0x1C) = (uint16_t)(RF<uint16_t>(s, 0x1C) + (uint16_t)(ComputeSin(RF<int16_t>(s, RS_ANGLE)) / radius_div));
		RF<uint16_t>(s, 0x20) = (uint16_t)(RF<uint16_t>(s, 0x20) + (uint16_t)(parts::ComputeCos(RF<int16_t>(s, RS_ANGLE)) / radius_div));
		RF<uint16_t>(s, 0x1E) = (uint16_t)(RF<uint16_t>(s, 0x1E) + (uint16_t)(-lift - (int32_t)(CrtRand() & lift_mask)));
		RF<uint16_t>(s, RS_SPEED) = (uint16_t)((CrtRand() & speed_mask) + speed);
		return s;
	}

	// 0x8D4DE0's orbit: the phase + spin, the position on the circle round the centre (radius
	// 4096 / divisor), y - rise
	inline void ReviveRingStep(uint8_t *node, int32_t spin, int32_t rise)
	{
		const uint16_t angle = (uint16_t)((RF<uint16_t>(node, RS_ANGLE) + spin) & 0xFFF);
		RF<uint16_t>(node, 0x1C) = RF<uint16_t>(node, RS_CENTRE);
		RF<uint16_t>(node, RS_ANGLE) = angle;
		RF<uint16_t>(node, 0x20) = RF<uint16_t>(node, RS_CENTRE + 4);
		const int32_t sx = ComputeSin((int16_t)angle) / (int32_t)RF<int16_t>(node, RS_DIVISOR);
		RF<uint16_t>(node, 0x1C) = (uint16_t)(RF<uint16_t>(node, 0x1C) + (uint16_t)sx);
		const int32_t cz = parts::ComputeCos(RF<int16_t>(node, RS_ANGLE)) / (int32_t)RF<int16_t>(node, RS_DIVISOR);
		RF<uint16_t>(node, 0x1E) = (uint16_t)(RF<uint16_t>(node, 0x1E) - rise);
		RF<uint16_t>(node, 0x20) = (uint16_t)(RF<uint16_t>(node, 0x20) + (uint16_t)cz);
	}

	// 0x8D4D60: a sparkle's flipbook frame at its position, the camera scaled by `scale` (a 16-bit
	// size: its high word cleared), offset by its camera offset (nothing when hidden)
	inline void ReviveSparkleDraw(const uint8_t *node, uint16_t scale, uint32_t &cursor)
	{
		if ((RF<uint8_t>(node, 0x26) & 4) != 0) return;
		TransformCameraByShadowRotation(node + 0x1C, scale, RF<int16_t>(node, RS_SHADE));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = RF<uint32_t>(node, RS_SEQ);
		*(uint16_t *)(h + 4) = RF<uint16_t>(node, RS_FRAME);
		*(uint16_t *)(h + 0x24) = 0;
		cursor = InitEffectSequenceFromData(h, RenderOT(), 2, cursor);
		FieldFree(0xB4);
	}
}
}
