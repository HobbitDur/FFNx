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

// The magic-cast glow (engine code 0x574C80..0x575520, outside every effect module): the rings, the glow and the
// sparkles that rise around a character while it casts. AddTaskToQueueAnimSeq_99_B0_B1_B4_A5 (0x571840) spawns the
// task of its kind 0 (table 0xC816A4[0] = 0x574C80) into the hit-effect queue (TASK_QUEUE 0x209FAA8, 0x24-byte nodes,
// ticked once per battle frame by the battle loop at 0x500923) with the casting entity in node +0x20; the battle
// action sequence (AnimSeq opcodes 99 / B0 / B1 / B4 / A5) and a few spell modules call it. The kind's tasks are only
// reached from this spawner: no other effect runs them.
//
//   Spawner 0x574C80 - first tick: the entity's default effect position (0x571400), two 3D system sounds (5 and 6,
//     BdPlaySystemSE3D 0x5017A0), then 5 rings, 1 glow and 6 sparkles into the same queue (sizes from the entity's
//     size +0x26, positions from its height +0x24, angles / delays / speeds from rand()); then it counts 31 ticks.
//   Ring 0x5753A0 - prim model 1 (scale x / z fixed, y growing, turned by a random y angle), after a delay of 3 ticks
//     per ring; mode 0x230 for 5 ticks, then fading to black (mode 0x2F0, fade 409 per tick); 16 ticks.
//   Glow 0x575250 - prim model 7 (scale x / z growing by a step that shrinks by 1/8 per tick), fading to black from
//     its first tick (mode 0xF3, fade 292 per tick); 15 ticks.
//   Sparkle 0x574F50 - shared sprite sequence 5 on a point that turns round the entity (radius growing by 1/64 per
//     tick, y angle by its spin) and rises (speed growing by 1/32 per tick), after a random delay; ticks 1..12 spawn
//     3 trails each, from tick 12 the sprite frame cycles 0..9; 19 ticks.
//   Trail 0x5751A0 - shared sprite sequence 5 at its point (frame 9 - ticks left), rising (speed growing by 1/16
//     per tick), shrinking by 0x40 per tick; 4..6 ticks.
// Every drawing task draws THEN updates; with the effects-paused bit (battle_to_update_flags 0x1D96A9C bit 0) a task
// draws and does not update (the spawner does nothing). The draws go into the battle render list's packet cursor
// (0x1D8E054) and the render list's OT + 0x44.
//
// Quirks: the spawner keeps the entity pointer in its own argument slot ([esp+0x1C] = the executor's pushed node
// pointer, not re-read by the caller: not reproduced). The sparkle's local position vector has a 4th word the code
// never writes; it reaches the trail's +0x16 word, which is overwritten before the task returns (not observable).

#include "mag_common.h"

namespace ff8fx
{
namespace cast
{
	using namespace eng;
	using namespace magc;

	static const uint32_t ORIG_Spawner = 0x574C80;
	static const uint32_t ORIG_Sparkle = 0x574F50;
	static const uint32_t ORIG_Trail = 0x5751A0;
	static const uint32_t ORIG_Glow = 0x575250;
	static const uint32_t ORIG_Ring = 0x5753A0;
	static TaskQueue *const QUEUE = (TaskQueue *)0x209FAA8; // TASK_QUEUE (hit-effect queue)

#pragma pack(push, 1)
	struct SpawnerNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;     // +0x0E (0, set by 0x571840)
		int16_t pos[4];    // +0x10 the entity's default effect position (x, y, z, w)
		uint8_t pad18[8];
		uint8_t *ent;      // +0x20 the casting entity
	};
	struct RingNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E ticks before the first draw
		int16_t pos[4];    // +0x10 x, y (the entity's height), z
		int16_t angle;     // +0x18 y angle
		int16_t pad1A;
		int16_t scale_xz;  // +0x1C
		int16_t pad1E;
		int16_t scale_y;   // +0x20
		int16_t grow_y;    // +0x22 per tick, growing by 1/32 per tick
	};
	struct GlowNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[4];    // +0x10
		uint8_t pad18[4];
		int16_t scale;     // +0x1C x / z
		int16_t grow;      // +0x1E per tick, shrinking by 1/8 per tick
		uint8_t pad20[4];
	};
	struct SparkleNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E ticks before the first draw
		int16_t pos[3];    // +0x10 centre x, y, z
		int16_t speed_y;   // +0x16 per tick (negative = up), growing by 1/32 per tick
		int16_t angle;     // +0x18 y angle
		int16_t spin;      // +0x1A per tick
		int16_t size;      // +0x1C sprite scale (0x1000)
		int16_t pad1E;
		int16_t radius;    // +0x20 growing by 1/64 per tick
		int16_t frame;     // +0x22 sprite frame
	};
	struct TrailNode
	{
		TaskNode hdr;
		int16_t left;      // +0x0C ticks left
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t speed_y;   // +0x16 per tick, growing by 1/16 per tick
		uint8_t pad18[4];
		int16_t size;      // +0x1C sprite scale, -0x40 per tick
		int16_t pad1E;
		uint8_t pad20[4];
	};
#pragma pack(pop)
	static_assert(sizeof(SpawnerNode) == 0x24 && sizeof(RingNode) == 0x24 && sizeof(GlowNode) == 0x24, "cast nodes");
	static_assert(sizeof(SparkleNode) == 0x24 && sizeof(TrailNode) == 0x24, "cast nodes");

	// battle_to_update_flags bit 0: effects paused (draw, do not update)
	inline bool Paused() { return (UpdateFlags() & 1) != 0; }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x1D8E054); }
	inline void BdPlaySystemSE3D(int32_t id, int32_t a, const int16_t *pos) { fn<void (__cdecl *)(int32_t, int32_t, const int16_t *)>(0x5017A0)(id, a, pos); }
	inline uint32_t EffectPrimModel(int id) { return fn<uint32_t (__cdecl *)(int)>(0x571240)(id); } // sub_571240: table 0xC81678
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector

	// (9011 * entity size) >> 12, at most 0x7FFF
	static int32_t SizeScale(const uint8_t *ent)
	{
		int32_t s = (int32_t)*(const int16_t *)(ent + 0x26) * 9011 >> 12;
		return s > 0x7FFF ? 0x7FFF : s;
	}

	// ------------------------------------------------------------------ draws (the part before the paused test)
	// ring / glow: prim model with a y angle and a scale at the node's position, through the camera
	static void ModelMatrix(int16_t angle_y, const int16_t *pos, int32_t sx, int32_t sy)
	{
		int16_t ang[4] = {};
		Mat4x3 m;
		ang[0] = 0;
		ang[1] = angle_y;
		ang[2] = 0;
		BuildRotationMatrixFromAngles(ang, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		int32_t sv[3] = { sx, sy, sx };
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	static void RingHeader(uint8_t *h, const RingNode *r)
	{
		*(uint32_t *)h = EffectPrimModel(1);
		*(uint32_t *)(h + 0x14) = 0x3867;
		*(uint32_t *)(h + 0x1C) = 0x230;
		if (r->counter >= 5)
		{
			const int32_t c = (int32_t)r->counter - 5;
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0x2F0;
			*(int32_t *)(h + 0xC) = c * 409;
		}
	}

	static void GlowHeader(uint8_t *h, const GlowNode *g)
	{
		*(uint32_t *)h = EffectPrimModel(7);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (g->counter >= 0)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = (int32_t)g->counter * 292;
		}
	}

	static void RenderModel(uint8_t *h)
	{
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// sparkle: its point, R_y(angle) * (0, 0, -radius) + centre (v[3] is never written by the original)
	static void SparklePoint(const SparkleNode *s, int16_t v[4])
	{
		int16_t ang[4] = {};
		Mat4x3 m;
		v[2] = (int16_t)-s->radius;
		v[0] = 0;
		v[1] = 0;
		v[3] = 0;
		ang[0] = 0;
		ang[1] = s->angle;
		ang[2] = 0;
		BuildRotationMatrixFromAngles(ang, &m);
		MatrixMulVector(&m, v, v);
		v[2] = (int16_t)(v[2] + s->pos[2]);
		v[0] = (int16_t)(v[0] + s->pos[0]);
		v[1] = (int16_t)(v[1] + s->pos[1]);
	}

	// shared sprite sequence 5, frame `frame`, scaled by `size` at `pos`
	static void SpriteDraw(const int16_t *pos, int16_t size, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, size, -((int32_t)size >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(void **)h = EffectSpriteSharedSequence(5);
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------ updates (after the paused test; pure on the node)
	static uint32_t RingStep(RingNode *r)
	{
		const int16_t g = r->grow_y;
		r->scale_y = (int16_t)(r->scale_y + g);
		r->grow_y = (int16_t)(g + (int16_t)(g >> 5));
		r->counter++;
		return r->counter > 0xF ? TASK_END : 0;
	}

	static uint32_t GlowStep(GlowNode *g)
	{
		const int16_t d = g->grow;
		g->scale = (int16_t)(g->scale + d);
		g->grow = (int16_t)(d - (int16_t)((int32_t)d / 8));
		g->counter++;
		return g->counter > 0xE ? TASK_END : 0;
	}

	static uint32_t SparkleStep(SparkleNode *s)
	{
		const int16_t v = s->speed_y;
		s->pos[1] = (int16_t)(s->pos[1] + v);
		s->speed_y = (int16_t)(v + (int16_t)(v >> 5));
		s->angle = (int16_t)(s->angle + s->spin);
		s->radius = (int16_t)(s->radius + (int16_t)(s->radius >> 6));
		const int16_t c = s->counter;
		if (c >= 0xC) s->frame++;
		if (s->frame > 9) s->frame = 0;
		s->counter = (int16_t)(c + 1);
		return s->counter > 0x12 ? TASK_END : 0;
	}

	static uint32_t TrailStep(TrailNode *t)
	{
		const int16_t v = t->speed_y;
		t->size = (int16_t)(t->size - 0x40);
		t->pos[1] = (int16_t)(t->pos[1] + v);
		t->speed_y = (int16_t)(v + (int16_t)(v >> 4));
		t->left--;
		return t->left < 0 ? TASK_END : 0;
	}
}
}

#ifdef FF8_FX_HELD
#include "fx_cast_held.h"
#endif

namespace ff8fx
{
namespace cast
{
	// ------------------------------------------------------------------
	// Spawner (0x574C80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpawnerTask(TaskNode *n)
	{
		if (Paused()) return 0;
		SpawnerNode *sp = (SpawnerNode *)n;
		uint8_t *ent = sp->ent;
		if (sp->counter == 0)
		{
			GetDefaultEffectPosition(ent, sp->pos);
			BdPlaySystemSE3D(5, 0, sp->pos);
			BdPlaySystemSE3D(6, 0, sp->pos);
			if (sp->counter != 0) goto done;
			{
				// 5 rings: scale x / z from the entity's size, growing per ring; y grows from 0
				const int32_t base = SizeScale(ent);
				const int32_t step = base / 5;
				int32_t div = 8;
				int32_t i = 0;
				do
				{
					RingNode *r = (RingNode *)AddTaskToQueue(QUEUE, ORIG_Ring);
					r->delay = (int16_t)(i * 3);
					*(uint32_t *)&r->pos[0] = *(const uint32_t *)&sp->pos[0];
					*(uint32_t *)&r->pos[2] = *(const uint32_t *)&sp->pos[2];
					r->counter = 0;
					r->pos[1] = *(const int16_t *)(ent + 0x24);
					r->angle = (int16_t)(CrtRand() % 2048);
					r->scale_xz = (int16_t)(i * step + base);
					const int32_t q = base / div;
					r->scale_y = 0;
					div += 2;
					r->grow_y = (int16_t)(q - i * 8);
					i++;
				} while (div < 0x12);
			}
			if (sp->counter != 0) goto done;
			{
				// the glow: scale = 2 x the size scale, its first step a quarter of it
				const int32_t s = SizeScale(ent);
				GlowNode *g = (GlowNode *)AddTaskToQueue(QUEUE, ORIG_Glow);
				*(uint32_t *)&g->pos[0] = *(const uint32_t *)&sp->pos[0];
				*(uint32_t *)&g->pos[2] = *(const uint32_t *)&sp->pos[2];
				g->pos[1] = *(const int16_t *)(ent + 0x24);
				const int16_t s2 = (int16_t)(s + s);
				g->counter = 0;
				g->scale = s2;
				g->grow = (int16_t)((int32_t)s2 / 4);
			}
			if (sp->counter != 0) goto done;
			{
				// 6 sparkles: radius ((10012 * size) >> 12, at most 0x7FFF) * 900 >> 12
				int32_t r0 = (int32_t)*(const int16_t *)(ent + 0x26) * 10012 >> 12;
				if (r0 > 0x7FFF) r0 = 0x7FFF;
				const int32_t radius = r0 * 900 >> 12;
				for (int32_t i = 0; i < 6; i++)
				{
					SparkleNode *s = (SparkleNode *)AddTaskToQueue(QUEUE, ORIG_Sparkle);
					s->counter = 0;
					s->delay = (int16_t)(CrtRand() % 6 + i);
					*(uint32_t *)&s->pos[0] = *(const uint32_t *)&sp->pos[0];
					*(uint32_t *)&s->pos[2] = *(const uint32_t *)&sp->pos[2];
					s->pos[1] = *(const int16_t *)(ent + 0x24);
					s->pos[1] = (int16_t)(s->pos[1] + (-400 - CrtRand() % 150));
					s->speed_y = (int16_t)(-(CrtRand() % 40) - (i * 4 + 40));
					s->angle = (int16_t)((CrtRand() % 400 + 400) * i);
					const int32_t sp_r = CrtRand() % 200;
					s->size = 0x1000;
					s->radius = (int16_t)radius;
					s->frame = 0;
					s->spin = (int16_t)(sp_r + 0x50);
				}
			}
		}
	done:
		sp->counter++;
		return sp->counter > 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparkle (0x574F50)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		if (s->delay > 0)
		{
			if (Paused()) return 0;
			s->delay--;
			return 0;
		}
		// 30 fps layer: see fx_cast_held.inc
		FX_HELD(held_note_draw(ORIG_Sparkle, s);)
		int16_t v[4];
		SparklePoint(s, v);
		SpriteDraw(v, s->size, s->frame);
		FX_HELD(held_note_drawn(s);)
		if (Paused()) return 0;
		const int16_t c = s->counter;
		if (c >= 1 && c <= 0xC)
		{
			// 3 trails from the point
			for (int k = 3; k != 0; k--)
			{
				TrailNode *t = (TrailNode *)AddTaskToQueue(QUEUE, ORIG_Trail);
				if (!t) continue;
				t->left = (int16_t)(CrtRand() % 3 + 4);
				*(uint32_t *)&t->pos[0] = *(const uint32_t *)&v[0];
				*(uint32_t *)&t->pos[2] = *(const uint32_t *)&v[2];
				t->pos[0] = (int16_t)(t->pos[0] + (0x1E - CrtRand() % 0x3C));
				t->pos[1] = (int16_t)(t->pos[1] + (0x1E - CrtRand() % 0x3C));
				const int32_t dz = CrtRand() % 0x3C;
				t->speed_y = (int16_t)(s->speed_y >> 2);
				t->pos[2] = (int16_t)(t->pos[2] + (0x1E - dz));
				t->size = (int16_t)(CrtRand() % 0x320 + 0x840);
			}
		}
		return SparkleStep(s);
	}

	// ------------------------------------------------------------------
	// Trail (0x5751A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TrailTask(TaskNode *n)
	{
		TrailNode *t = (TrailNode *)n;
		// 30 fps layer: see fx_cast_held.inc
		FX_HELD(held_note_draw(ORIG_Trail, t);)
		SpriteDraw(t->pos, t->size, (int16_t)(9 - t->left));
		FX_HELD(held_note_drawn(t);)
		if (Paused()) return 0;
		return TrailStep(t);
	}

	// ------------------------------------------------------------------
	// Glow (0x575250)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GlowTask(TaskNode *n)
	{
		GlowNode *g = (GlowNode *)n;
		// 30 fps layer: see fx_cast_held.inc
		FX_HELD(held_note_draw(ORIG_Glow, g);)
		ModelMatrix(0, g->pos, g->scale, 0x1000);
		uint8_t *h = AllocHeader(0x58);
		GlowHeader(h, g);
		RenderModel(h);
		FX_HELD(held_note_drawn(g);)
		if (Paused()) return 0;
		return GlowStep(g);
	}

	// ------------------------------------------------------------------
	// Ring (0x5753A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *r = (RingNode *)n;
		if (r->delay > 0)
		{
			if (Paused()) return 0;
			r->delay--;
			return 0;
		}
		// 30 fps layer: see fx_cast_held.inc
		FX_HELD(held_note_draw(ORIG_Ring, r);)
		ModelMatrix(r->angle, r->pos, r->scale_xz, r->scale_y);
		uint8_t *h = AllocHeader(0x58);
		RingHeader(h, r);
		RenderModel(h);
		FX_HELD(held_note_drawn(r);)
		if (Paused()) return 0;
		return RingStep(r);
	}
}

	void register_fx_cast()
	{
		register_port(cast::ORIG_Spawner, (void *)cast::SpawnerTask, "cast 574C80 CastGlow_Spawner", 0);
		register_port(cast::ORIG_Sparkle, (void *)cast::SparkleTask, "cast 574F50 CastGlow_Sparkle", 0);
		register_port(cast::ORIG_Trail, (void *)cast::TrailTask, "cast 5751A0 CastGlow_SparkleTrail", 0);
		register_port(cast::ORIG_Glow, (void *)cast::GlowTask, "cast 575250 CastGlow_Glow", 0);
		register_port(cast::ORIG_Ring, (void *)cast::RingTask, "cast 5753A0 CastGlow_Ring", 0);
		// 30 fps layer: see fx_cast_held.inc
		FX_HELD(register_fx_cast_held();)
	}
}

#ifdef FF8_FX_HELD
#include "fx_cast_held.inc"
#endif
