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

// 30 fps part of mag106_reflect.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag106_reflect.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Reflect task draws THEN updates, and none has a draw-only mode (its logic always steps).
// A real tick records every draw in order with the GTE registers and the Field_Alloc scratch it
// started from (draw headers keep the scratch bytes where the code does not write). A held frame
// replays the log in the same order, each view matrix composed again with the current camera
// (barrier model matrix, recorded), half way to what the next tick draws:
//   Wall    - top of the z range (growth, ticks 5..25) and fog (26..39) half way between this
//             tick's and the next tick's closed-form values; V scroll half a step (4 texels, the
//             texture repeats every 0x80); its last draw (barrier frame 39) holds.
//   Hexagon - angle and tilt half way along their speeds, fade / x squash half way to the next
//             tick's, the centre half way to the next tick's point of the barrier's path (frames
//             0..19, then fixed); its last draw (tick 16) holds.
//   Ring    - scale half way along the next tick's (braked) speed, fade half way to the next
//             tick's; its last draw (tick 20) holds.
// The beams never run in vanilla (see AppendByValue): nothing to hold. Spawns, sound, damage and
// the LCG stay on real ticks.

namespace ff8fx
{
namespace reflect106
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_WALL = 1, REC_HEXAGON = 2, REC_RING = 3 };

	struct Rec
	{
		uint32_t kind;
		Mat4x3 model;        // barrier model matrix (view = camera x model)
		int32_t e;           // wall: step (barrier frame - 5)
		int16_t frame;       // wall: barrier frame; hexagon / ring: counter
		int16_t a, b;        // hexagon: angle, tilt; ring: height, scale
		int16_t a1, b1;      // hexagon: next angle, tilt; ring: -, next scale
		int16_t center[3];   // hexagon: centre drawn
		int16_t center1[3];  // hexagon: the next tick's centre
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[64];                  // up to 3 walls + 24 hexagons + 9 rings a tick
	static uint8_t g_held_packets[0x20000]; // module arena (0x10000 per tick)

	static void held_note_director()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind, const BarrierNode *parent)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.model = parent->model;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_wall(const BarrierNode *b, int32_t e)
	{
		Rec *r = NewRec(REC_WALL, b);
		if (!r) return;
		r->e = e;
		r->frame = b->frame;
	}

	static void held_note_hexagon(const HexagonNode *x)
	{
		Rec *r = NewRec(REC_HEXAGON, x->parent);
		if (!r) return;
		const BarrierNode *p = x->parent;
		r->frame = x->counter;
		r->a = x->angle;
		r->b = x->tilt;
		r->a1 = (int16_t)(x->angle + x->dangle);
		r->b1 = (int16_t)(x->tilt + x->dtilt);
		memcpy(r->center, p->center, sizeof(r->center));
		// the barrier ran before the hexagons this tick: its frame is already the next tick's
		if ((uint32_t)(int32_t)p->frame < 0x14) BarrierCenter(p->frame, r->center1);
		else memcpy(r->center1, p->center, sizeof(r->center1));
	}

	static void held_note_ring(const RingNode *g)
	{
		Rec *r = NewRec(REC_RING, g->parent);
		if (!r) return;
		r->frame = g->counter;
		r->a = g->height;
		r->b = g->scale;
		int16_t speed = (int16_t)(g->speed + g->accel);
		if (speed < 0x10) speed = 0x10;
		r->b1 = (int16_t)(g->scale + speed);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void WallHeld(const Rec &r, const Mat4x3 *view, int num, int den)
	{
		int32_t height = WallHeight(r.e), fade = WallFade(r.e), f8 = shl32(r.frame, 3);
		if ((uint32_t)(r.e + 1) < 0x23)
		{
			height = lerp_i(height, WallHeight(r.e + 1), num, den);
			fade = lerp_i(fade, WallFade(r.e + 1), num, den);
			f8 += 8 * num / den;
		}
		WallDraw(WallScroll(f8), height, fade, view);
		FieldFree(0x60);
	}

	static void HexagonHeld(const Rec &r, const Mat4x3 *view, int num, int den)
	{
		int32_t fade, sx;
		HexagonFade(r.frame, fade, sx);
		int16_t angle = r.a, tilt = r.b;
		int16_t center[3];
		memcpy(center, r.center, sizeof(center));
		if (r.frame < 0x10)
		{
			int32_t fade1, sx1;
			HexagonFade(r.frame + 1, fade1, sx1);
			fade = lerp_i(fade, fade1, num, den);
			sx = lerp_i(sx, sx1, num, den);
			angle = lerp_angle(r.a, r.a1, num, den);
			tilt = lerp_angle(r.b, r.b1, num, den);
			for (int k = 0; k < 3; k++) center[k] = (int16_t)lerp_i(r.center[k], r.center1[k], num, den);
		}
		HexagonDraw(center, angle, tilt, fade, (int16_t)sx, view);
	}

	static void RingHeld(const Rec &r, const Mat4x3 *view, int num, int den)
	{
		int32_t fade = RingFade(r.frame), scale = r.b;
		if (r.frame < 0x14)
		{
			fade = lerp_i(fade, RingFade(r.frame + 1), num, den);
			scale = lerp_i(r.b, r.b1, num, den);
		}
		RingDraw(r.a, (int16_t)scale, fade, view);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Mat4x3 view;
			ComposeAffineTransform(&Camera(), &r.model, &view);
			Replay(r);
			switch (r.kind)
			{
			case REC_WALL: WallHeld(r, &view, num, den); break;
			case REC_HEXAGON: HexagonHeld(r, &view, num, den); break;
			case REC_RING: RingHeld(r, &view, num, den); break;
			}
		}
	}
}

	static void register_mag106_held()
	{
		mark_held(reflect106::ORIG_BarrierTask);
		mark_held(reflect106::ORIG_HexagonTask);
		mark_held(reflect106::ORIG_RingTask);
		register_module_held(106, reflect106::HeldReady, reflect106::HeldFrame);
	}
}
