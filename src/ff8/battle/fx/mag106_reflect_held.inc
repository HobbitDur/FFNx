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

	static Rec *NewRec(uint32_t kind, const Barrier *parent)
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
		Rec *r = NewRec(REC_WALL, &b->b);
		if (!r) return;
		r->e = e;
		r->frame = b->frame;
	}

	static void held_note_hexagon(const HexagonNode *x)
	{
		Rec *r = NewRec(REC_HEXAGON, x->o.parent);
		if (!r) return;
		const Barrier *p = x->o.parent;
		// (the barrier's frame is its node's, right before its Barrier b)
		const int16_t frame = ((const BarrierNode *)((const uint8_t *)p - offsetof(BarrierNode, b)))->frame;
		r->frame = x->counter;
		r->a = x->o.angle;
		r->b = x->o.tilt;
		r->a1 = (int16_t)(x->o.angle + x->o.dangle);
		r->b1 = (int16_t)(x->o.tilt + x->o.dtilt);
		memcpy(r->center, p->center, sizeof(r->center));
		// the barrier ran before the hexagons this tick: its frame is already the next tick's
		if ((uint32_t)(int32_t)frame < 0x14) BarrierCenter(MODEL_Wall, frame, P().hexagons.climb_steps, P().hexagons.climb_ticks, r->center1);
		else memcpy(r->center1, p->center, sizeof(r->center1));
	}

	static void held_note_ring(const RingNode *g)
	{
		Rec *r = NewRec(REC_RING, g->g.parent);
		if (!r) return;
		r->frame = g->counter;
		r->a = g->g.height;
		r->b = g->g.scale;
		int16_t speed = (int16_t)(g->g.speed + g->g.accel);
		if (speed < 0x10) speed = 0x10;
		r->b1 = (int16_t)(g->g.scale + speed);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the draws at an in-between state: fx_barrier.h's WallDraw / OrbitDraw / GrowthDraw take a whole frame or
	// counter; these take the V scroll (half steps) and the fade / squash directly, same writes in the same order
	static uint16_t WallScroll(int32_t f8) { return (uint16_t)((uint8_t)(0u - (uint8_t)f8) & 0x7F); }
	static void HeldWallDraw(uint16_t scroll, int32_t height, int32_t fade, const Mat4x3 *view)
	{
		int16_t base, span;
		WallMeasure(MODEL_Wall, &base, &span);
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)h = MODEL_Wall;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x0C) = 0;
		*(uint16_t *)(h + 0x0E) = scroll;
		*(int16_t *)(h + 0x5C) = base;
		h[6] = 0;
		h[5] = 0;
		h[4] = 0;
		*(int32_t *)(h + 8) = fade;
		*(int16_t *)(h + 0x5E) = (int16_t)height;
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		PacketCursor() = barrier::RenderWall(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x60);
	}
	static void HexagonFade(int32_t c, int32_t &fade, int32_t &sx)
	{
		const ReflectParams::Hexagons &hp = P().hexagons;
		if (c > hp.fade_from)
		{
			const int32_t q = shl32(c - hp.fade_from, 12) / hp.fade_ticks;
			sx = (int16_t)(0x1000 - (int16_t)q);
			fade = (q >> 1) + 0x800;
		}
		else
		{
			sx = 0x1000;
			fade = 0x800;
		}
	}
	static void HeldHexagonDraw(const int16_t *center, int16_t angle, int16_t tilt, int32_t fade, int16_t sx, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t p[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		barrier::Rot2D(center, p, angle, TABLE_SinCos);
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		Mat4x3 X = {};  // its translation = the rotated point in view space
		GteReadMAC123(X.t);
		*(uint32_t *)(h + 0x1C) = 0xF3;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		Mat4x3 S = {};  // pad / translation: vanilla stack garbage (never read)
		S.m[0][0] = sx;
		S.m[1][1] = 0x1000;
		S.m[2][2] = 0x1000;
		Mat4x3 R = {};
		barrier::ZRotation((int16_t)-angle, &R);
		barrier::MatMul56C220(&R, &S);
		MatrixMultiply(view, &R);
		barrier::XRotation(tilt, &X);
		MatrixMultiply(&R, &X);
		GteSetRotMatrix(&X);
		GteSetTransVector(&X);
		*(uint32_t *)h = MODEL_Hexagon;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static int32_t RingFade(int32_t c) { return ComputeSin(shl32(c, 10) / P().rings.fade_ticks); }
	static void HeldRingDraw(int16_t height, int16_t scale, int32_t fade, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t V[4] = { 0, 0, height, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(V);
		GteMVMVA_RotV0Tr();
		Mat4x3 T = {};
		GteReadMAC123(T.t);
		GteSetTransVector(&T);
		Mat4x3 S = {};
		S.m[0][0] = scale;
		S.m[1][1] = scale;
		S.m[2][2] = scale;
		barrier::MatMul56C220(&S, view);
		GteSetRotMatrix(&S);
		*(uint32_t *)h = MODEL_Ring;
		*(int32_t *)(h + 0xC) = fade;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void WallHeld(const Rec &r, const Mat4x3 *view, int num, int den)
	{
		const ReflectParams::Wall &wp = P().wall;
		int32_t height = WallHeight(MODEL_Wall, r.e, wp.grow), fade = WallFade(r.e, wp.grow, wp.fade), f8 = shl32(r.frame, 3);
		if ((uint32_t)(r.e + 1) < 0x23)
		{
			height = lerp_i(height, WallHeight(MODEL_Wall, r.e + 1, wp.grow), num, den);
			fade = lerp_i(fade, WallFade(r.e + 1, wp.grow, wp.fade), num, den);
			f8 += 8 * num / den;
		}
		HeldWallDraw(WallScroll(f8), height, fade, view);
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
		HeldHexagonDraw(center, angle, tilt, fade, (int16_t)sx, view);
	}

	static void RingHeld(const Rec &r, const Mat4x3 *view, int num, int den)
	{
		int32_t fade = RingFade(r.frame), scale = r.b;
		if (r.frame < 0x14)
		{
			fade = lerp_i(fade, RingFade(r.frame + 1), num, den);
			scale = lerp_i(r.b, r.b1, num, den);
		}
		HeldRingDraw(r.a, (int16_t)scale, fade, view);
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
