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

// Parts library, fire (Fire 0x629C10 flame, 0x62A0A0 ember, 0x629F90 spark; Firaga's area flame
// 0x61C6B0 draws as the flame): a flame flipbook facing the camera with a ground glow under it,
// brighter the higher the flame; embers rising faster and faster, shrinking; sparks drifting and
// slowing, shrinking. Each piece waits its delay first (a tick of its own, not counted while the
// battle only draws). Bit-exact with Fire's code (mag002_fire.cpp before it was built from here).

#pragma once

#include "fx_layouts.h"

namespace ff8fx
{
namespace layouts
{
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
}
}
