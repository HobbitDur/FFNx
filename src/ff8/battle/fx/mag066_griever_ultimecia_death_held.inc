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

// 30 fps part of mag066_griever_ultimecia_death.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag066_griever_ultimecia_death.cpp; shared pieces in
// mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records every draw in queue order: the task's
// node as drawn, the GTE registers and the Field_Alloc scratch under its headers it started from, and
// for the pool tasks a copy of their pool as drawn. A held frame replays the log in the same order
// with the module's own draw code, on the state half way (num / den) to the one the tick left (= what
// the next tick draws); nothing random is drawn, nothing is written back:
//   pools     - an entry drawn by the tick that the tick updated (same owner, age + 1): position, size,
//               spin, scroll, colour-scale cycle colour and depth-cue fade half way (the cue is on in
//               between when either side has it; at 0 / 1 exactly the drawn / next header); sprite
//               sequence frames hold (the next one at 1). Freed by the tick: holds below 1. Added after
//               the task's draw (the melt's dust): at 1 only. Spawned by the next tick: never (random).
//   nodes     - rings, wave, swirls, flare, billboards: scale / spin / fade half way; a task whose last
//               draw this was holds below 1.
//   melt      - the caster's faces at its in-between pose (pose_midpoint); a face that broke off on the
//               tick stays where it broke (its shard's start) and shows its shard (tint overlay) at 1; a
//               flying shard's corners and tint half way; a shard that ended holds below 1.
//   stage     - group 1 as drawn (its pose is the engine's).
//   fade      - the screen fade task's tile (0x571250) at the level half way; its last tile holds below 1.
// Draw-only ticks (battle_to_update_flags & 0x201) do not move: everything holds.
// The held frame draws into private packet buffers (module arena, frame arena) and puts back what the
// draws write outside them: GTE, Field_Alloc scratch, shadow-rotation scratch (HeldScope), the
// screen-anchor matrix 0x24C91B0, the vertex workspace (0x1D98B3C), the caster's and stage group 1's
// bone matrices, RenderGeometry's 0x1D99C18.

namespace ff8fx
{
namespace gu066
{
	using namespace magc;

	static const uint32_t SCRATCH = 0x200;      // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 160;
	static const uint32_t POOLBUF = 0x80000;    // pool copies of one tick
	static const uint32_t FACE_BYTES = 0x1184;  // face states 0x24C9190 (0x461 dwords)
	static const uint32_t SHARD_BYTES = 1000 * 0x44;

	struct Rec
	{
		uint32_t fn;                            // task function (kind)
		uint32_t node;
		uint8_t copy[0x24];                     // node as drawn
		uint32_t pool, pool_bytes, snap;        // pool tasks: the pool, its copy in g_poolbuf
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];               // Field_Alloc scratch under the draw's allocations
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static bool g_still = false;                // the tick was draw-only (nothing moved)
	static uint32_t g_packets = 0, g_frame = 0; // module / frame arena cursors at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_poolbuf[POOLBUF];
	static uint32_t g_poolused = 0;
	static uint8_t g_faces[FACE_BYTES];         // the melt's face states / shards as drawn
	static uint8_t g_shards[SHARD_BYTES];
	static uint8_t g_held_packets[0x10000];
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_ws_save[0x10000];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];
	static uint8_t g_anchor_save[0x20];

	static void *FieldScratch() { return var<void *>(0x1D999C4); }

	static void held_note_root(uint32_t)
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_poolused = 0;
		g_packets = 0;
		g_frame = 0;
		g_still = DrawOnly();
	}

	static Rec *held_add(uint32_t fn, uint32_t n, uint32_t base)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = PacketCursor();
			g_frame = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = n;
		memcpy(r.copy, (const void *)n, sizeof(r.copy));
		r.pool = r.pool_bytes = r.snap = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)base, SCRATCH);
		return &r;
	}

	static uint32_t PoolBytes(uint32_t fn)
	{
		switch (fn)
		{
		case ORIG_DustTask: case ORIG_SmokeTask: case ORIG_ShardSpin: return POOL2_END - POOL2;
		case ORIG_DustFall: return 800 * 0x18;
		case ORIG_DustFall2: return 400 * 0x18;
		default: return POOL1_END - POOL1;
		}
	}

	static void held_note_pool(uint32_t fn, uint32_t n, uint32_t base, uint32_t pool)
	{
		Rec *r = held_add(fn, n, base);
		if (!r) return;
		const uint32_t bytes = PoolBytes(fn);
		if (g_poolused + bytes > POOLBUF)
		{
			g_nrec--;
			return;
		}
		r->pool = pool;
		r->pool_bytes = bytes;
		r->snap = g_poolused;
		memcpy(g_poolbuf + g_poolused, (const void *)pool, bytes);
		g_poolused += bytes;
	}

	static void held_note_task(uint32_t fn, uint32_t n) { held_add(fn, n, (uint32_t)FieldScratch()); }

	static void held_note_melt(uint32_t n, uint32_t h)
	{
		if (!held_add(ORIG_MeltTask, n, h)) return;
		memcpy(g_faces, (const void *)FaceStates(), FACE_BYTES);
		memcpy(g_shards, (const void *)ShardTable(), SHARD_BYTES);
	}

	static void held_note_stage(uint32_t n, uint32_t h) { held_add(ORIG_StageTask, n, h); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// in-between helpers
	// ------------------------------------------------------------------
	// colour bytes 0..2 half way, the code byte of a
	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t c = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			c |= (uint32_t)(lerp_i((int32_t)((a >> k) & 0xFF), (int32_t)((b >> k) & 0xFF), num, den) & 0xFF) << k;
		return c;
	}

	// a draw header's depth cue: flags 0xF3 + level, or 0x33 (no cue: the level field is not written)
	struct Cue { uint32_t flags; int32_t fade; };
	static Cue LerpCue(Cue a, Cue b, int num, int den)
	{
		if (num <= 0) return a;
		if (num >= den) return b;
		const bool ca = a.flags == 0xF3, cb = b.flags == 0xF3;
		if (!ca && !cb) return a;
		return { 0xF3u, lerp_i(ca ? a.fade : 0, cb ? b.fade : 0, num, den) };
	}
	static void ApplyCue(uint32_t h, Cue c)
	{
		At<uint32_t>(h, 0x1C) = c.flags;
		if (c.flags == 0xF3) At<int32_t>(h, 0xC) = c.fade;
	}

	// the live node is the drawn one, one update later (or the same on a draw-only tick)
	static bool NodeNext(const Rec &r)
	{
		const TaskNode *t = (const TaskNode *)r.node;
		if (!(t->flags & 1) || (uint32_t)t->func != r.fn) return false;
		const int16_t c = At<int16_t>((uint32_t)r.copy, 0xC);
		return At<int16_t>(r.node, 0xC) == (int16_t)(g_still ? c : c + 1);
	}

	// ------------------------------------------------------------------
	// pools
	// ------------------------------------------------------------------
	static bool Owns(const Rec &r, const uint8_t *e)
	{
		switch (r.fn)
		{
		case ORIG_SparkTask: return *(const int32_t *)e == 1;
		case ORIG_PuffTask: case ORIG_DustTask: case ORIG_SmokeTask: return *(const int32_t *)e == At<int16_t>((uint32_t)r.copy, 0xE);
		case ORIG_GlowTask: return (e[0] & 2) != 0;
		default: return (e[0] & 1) != 0;
		}
	}

	static Cue SparkCue(int16_t c)
	{
		if (c < 6) return { 0xF3u, 0x1000 - c * 682 };
		if (c >= 0x12) return { 0xF3u, (c - 0x12) * 682 };
		return { 0x33u, 0 };
	}
	static Cue RainCue(int16_t c)
	{
		if (c < 6) return { 0xF3u, 0x1000 - c * 682 };
		if (c >= 0xA) return { 0xF3u, (c - 0xA) * 682 };
		return { 0x33u, 0 };
	}
	static Cue GlowCue(int16_t c)
	{
		if (c < 0x10) return { 0xF3u, (0x10 - c) << 8 };
		return { 0x33u, 0 };
	}

	// one entry's draw: a = as drawn (or the next tick's for an entry shown at 1 only), b = the next
	// tick's (same entry) or a; frame fields from `f` (a below 1, b at 1)
	static void PoolDraw(const Rec &r, uint32_t w, uint32_t h, const uint8_t *a, const uint8_t *b, int num, int den)
	{
		const uint8_t *f = num >= den ? b : a;
		alignas(4) uint8_t d[0x18];
		memcpy(d, a, sizeof(d));
		const uint32_t e = (uint32_t)d;
		const int16_t ca = *(const int16_t *)(a + 4), cb = *(const int16_t *)(b + 4);
		switch (r.fn)
		{
		case ORIG_SparkTask:
		{
			// SparkDraw with the colour-table colour and the cue half way
			At<int32_t>(w, 0x54) = At<int16_t>(e, 0x12);
			At<int32_t>(w, 0x50) = At<int16_t>(e, 0x10);
			At<int32_t>(w, 0x58) = At<int16_t>(e, 0x14);
			const int32_t angle = RotationBetweenVectors((const int32_t *)(w + 0x70), (const int32_t *)(w + 0x50), (int32_t *)(w + 0x60));
			BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(w + 0x30), (const int32_t *)(w + 0x60));
			At<int32_t>(w, 0x44) = At<int16_t>(e, 8);
			At<int32_t>(w, 0x48) = At<int16_t>(e, 0xA);
			At<int32_t>(w, 0x4C) = At<int16_t>(e, 0xC);
			const int32_t sx = At<int16_t>(e, 0xE);
			At<int32_t>(w, 0x58) = sx;
			At<int32_t>(w, 0x50) = sx;
			At<int32_t>(w, 0x54) = At<int16_t>(e, 0x16);
			Scale3DMatrix((Mat4x3 *)(w + 0x30), (const int32_t *)(w + 0x50));
			ComposeAffineTransform((const Mat4x3 *)(w + 0x10), (const Mat4x3 *)(w + 0x30), (Mat4x3 *)(w + 0x30));
			GteSetRotMatrix((const Mat4x3 *)(w + 0x30));
			GteSetTransVector((const Mat4x3 *)(w + 0x30));
			ApplyCue(h, LerpCue(SparkCue(ca), SparkCue(cb), num, den));
			At<uint32_t>(h, 0x20) = LerpColour(At<uint32_t>(SPARK_COLOURS, (ca % 0x12) * 4), At<uint32_t>(SPARK_COLOURS, (cb % 0x12) * 4), num, den);
			PacketCursor() = RenderModel((RenderHeader *)h, OT44(), 2, PacketCursor());
			break;
		}
		case ORIG_SparkRain:
		{
			At<int16_t>(e, 0xC) = lerp16(*(const int16_t *)(a + 0xC), *(const int16_t *)(b + 0xC), num, den);
			At<int16_t>(w, 4) = At<int16_t>(e, 0xC);
			ComposeZYXRotationMatrix((const int16_t *)w, (Mat4x3 *)(w + 0x28));
			At<int32_t>(w, 0x48) = At<int16_t>(e, 6);
			Scale3DMatrix((Mat4x3 *)(w + 0x28), (const int32_t *)(w + 0x48));
			GteSetRotMatrix((const Mat4x3 *)(w + 0x28));
			ApplyCue(h, LerpCue(RainCue(ca), RainCue(cb), num, den));
			At<uint32_t>(h, 0x20) = LerpColour(At<uint32_t>(RAIN_COLOURS, *(const int16_t *)(a + 8) * 4), At<uint32_t>(RAIN_COLOURS, *(const int16_t *)(b + 8) * 4), num, den);
			PacketCursor() = RenderModel((RenderHeader *)h, OT44(), 2, PacketCursor());
			break;
		}
		case ORIG_GlowTask:
		{
			At<int16_t>(e, 0x12) = lerp16(*(const int16_t *)(a + 0x12), *(const int16_t *)(b + 0x12), num, den);
			At<int16_t>(w, 2) = At<int16_t>(e, 0xE);
			ComposeZYXRotationMatrix((const int16_t *)w, (Mat4x3 *)(w + 0x28));
			At<int32_t>(w, 0x3C) = At<int16_t>(e, 8);
			At<int32_t>(w, 0x40) = At<int16_t>(e, 0xA);
			At<int32_t>(w, 0x44) = At<int16_t>(e, 0xC);
			const int32_t s = At<int16_t>(e, 6);
			At<int32_t>(w, 0x50) = s;
			At<int32_t>(w, 0x48) = s;
			Scale3DMatrix((Mat4x3 *)(w + 0x28), (const int32_t *)(w + 0x48));
			ComposeAffineTransform((const Mat4x3 *)(w + 8), (const Mat4x3 *)(w + 0x28), (Mat4x3 *)(w + 0x28));
			GteSetRotMatrix((const Mat4x3 *)(w + 0x28));
			GteSetTransVector((const Mat4x3 *)(w + 0x28));
			const Cue q = LerpCue(GlowCue(ca), GlowCue(cb), num, den);
			At<uint32_t>(h, 0x20) = q.flags;
			if (q.flags == 0xF3) At<int32_t>(h, 0xC) = q.fade;
			At<int32_t>(h, 0x14) = At<int16_t>(e, 0x12);
			At<uint32_t>(h, 0x24) = LerpColour(At<uint32_t>(GLOW_COLOURS, *(const int16_t *)(a + 0x10) * 4), At<uint32_t>(GLOW_COLOURS, *(const int16_t *)(b + 0x10) * 4), num, den);
			FrameArena() = GlowRender((void *)h, OT44(), 2, FrameArena());
			break;
		}
		default: // sprites: position and size half way, frame = age
			for (int k = 6; k <= 0xC; k += 2)
				At<int16_t>(e, k) = lerp16(*(const int16_t *)(a + k), *(const int16_t *)(b + k), num, den);
			{
				const int16_t frame = *(const int16_t *)(f + 4);
				switch (r.fn)
				{
				case ORIG_DustFall2: SpriteAtMAC((uint8_t *)h, e, (int16_t)(frame >> 2)); break;
				case ORIG_DustFall: SpriteAtMAC((uint8_t *)h, e, frame); break;
				case ORIG_ShardSpin: SpritePushed(w, (uint8_t *)h, e, -(At<int16_t>(e, 6) >> 3), frame); break;
				default: SpritePushed(w, (uint8_t *)h, e, -(int32_t)Push(), frame); break;
				}
			}
			break;
		}
	}

	static void HeldPool(const Rec &r, int num, int den)
	{
		// the task's allocations and set-up, as its draw does them
		uint32_t w, h;
		uint32_t wsize = 0x48, hsize = 0xB4;
		switch (r.fn)
		{
		case ORIG_SparkTask:
			wsize = 0x80; hsize = 0x6C;
			w = (uint32_t)FieldAlloc(wsize);
			h = (uint32_t)FieldAlloc(hsize);
			memcpy((void *)(w + 0x10), (const void *)&Camera(), 0x20);
			At<uint32_t>(h, 0) = 0xDE90CC;
			At<uint32_t>(h, 8) = 0;
			At<int32_t>(w, 0x70) = 0;
			At<int32_t>(w, 0x74) = -0x1000;
			At<int32_t>(w, 0x78) = 0;
			break;
		case ORIG_SparkRain:
			wsize = 0x68; hsize = 0x6C;
			w = (uint32_t)FieldAlloc(wsize);
			h = (uint32_t)FieldAlloc(hsize);
			At<uint32_t>(h, 0) = 0xDE90CC;
			At<uint32_t>(h, 8) = 0;
			TransformCameraByShadowRotation((const void *)0x24C9170, 0x1000, -1000);
			At<int16_t>(w, 2) = 0;
			At<int16_t>(w, 0) = 0;
			At<int32_t>(w, 0x50) = 0x2800;
			At<int32_t>(w, 0x4C) = 0x2800;
			break;
		case ORIG_GlowTask:
			wsize = 0x68; hsize = 0x90;
			w = (uint32_t)FieldAlloc(wsize);
			h = (uint32_t)FieldAlloc(hsize);
			At<uint32_t>(h, 8) = 0;
			At<int16_t>(h, 0x18) = 0;
			At<uint32_t>(h, 0x10) = 0;
			memcpy((void *)(w + 8), (const void *)&Camera(), 0x20);
			At<uint32_t>(h, 0) = 0xDE97A4;
			At<int16_t>(h, 0x1A) = 0x80;
			At<int16_t>(h, 0x1C) = 0x40;
			At<int16_t>(h, 0x1E) = 0x80;
			At<int32_t>(w, 0x4C) = 0x3000;
			At<int16_t>(w, 4) = 0;
			At<int16_t>(w, 0) = 0;
			break;
		default:
		{
			w = (uint32_t)FieldAlloc(wsize);
			h = (uint32_t)FieldAlloc(hsize);
			const uint32_t model = r.fn == ORIG_PuffTask ? 0xDE43E8 : r.fn == ORIG_DustTask ? 0xDE4910 : r.fn == ORIG_SmokeTask ? 0xDE423C : r.fn == ORIG_ShardSpin ? 0xDE45D0 : 0xDE477C;
			SpriteBegin(w, (uint8_t *)h, model);
			break;
		}
		}
		const uint8_t *A = g_poolbuf + r.snap;
		const uint8_t *B = (const uint8_t *)r.pool;
		const bool next = NodeNext(r); // the task draws on the next tick
		for (uint32_t o = 0; o < r.pool_bytes; o += 0x18)
		{
			const uint8_t *a = A + o, *b = B + o;
			const bool ia = Owns(r, a), ib = next && Owns(r, b);
			const bool same = ia && ib && *(const int16_t *)(b + 4) == (int16_t)(*(const int16_t *)(a + 4) + (g_still ? 0 : 1));
			if (same) PoolDraw(r, w, h, a, b, num, den);
			else
			{
				if (ia && num < den) PoolDraw(r, w, h, a, a, 0, den);   // freed by the tick: holds
				if (ib && num >= den) PoolDraw(r, w, h, b, b, den, den); // added after the draw: from the next tick on
			}
		}
		FieldFree(hsize);
		FieldFree(wsize);
	}

	// ------------------------------------------------------------------
	// the screen fade task: its tile (0x571250) at the level half way
	// ------------------------------------------------------------------
	static int32_t FadeLevel(uint32_t n, int32_t t)
	{
		const int32_t in = At<int16_t>(n, 0x10), level = At<int16_t>(n, 0x1C), hold = At<int16_t>(n, 0x12), out = At<int16_t>(n, 0x14);
		if (t < in) return (level / in) * t;
		if (t - in < hold) return level;
		const int32_t u = t - in - hold;
		return u < out ? level - (level / out) * u : -1;
	}

	static void HeldFade(const Rec &r, int num, int den)
	{
		if (g_still) return; // draw-only: the task draws nothing
		const uint32_t a = (uint32_t)r.copy;
		const int16_t c = At<int16_t>(a, 0xC);
		const int32_t va = FadeLevel(a, c);
		if (va < 0) return; // ended: nothing drawn
		const int32_t vb = FadeLevel(a, c + 1);
		int32_t v = va;
		if (vb < 0)
		{
			if (num >= den) return; // its last tile: holds
		}
		else v = lerp_i(va, vb, num, den);
		ScreenFadeColour(v, v, v);
	}

	// ------------------------------------------------------------------
	// node tasks (prim models through 0x572200)
	// ------------------------------------------------------------------
	static Cue NodeCue(uint32_t fn, uint32_t n)
	{
		const int16_t c = At<int16_t>(n, 0xC);
		switch (fn)
		{
		case ORIG_RingTask: if (c >= 2) return { 0xF3u, (c - 2) << 9 }; break;
		case ORIG_FlareTask:
			if (c < 6) return { 0xF3u, 0x1000 - c * 682 };
			if (c >= 0x22) return { 0xF3u, (c - 0x22) << 8 };
			break;
		case ORIG_Billboard: if (c >= 0x16) return { 0xF3u, (c - 0x16) << 10 }; break;
		case ORIG_SwirlTask:
		{
			const int32_t out = At<int16_t>(n, 0x22);
			const int16_t in = At<int16_t>(n, 0x16);
			const int32_t start_out = At<int16_t>(n, 0xE) - out;
			if (c < in) return { 0xF3u, 0x1000 - (0x1000 / in) * c };
			if (c >= start_out) return { 0xF3u, (c - start_out) * (0x1000 / out) };
			break;
		}
		}
		return { 0x33u, 0 };
	}

	// the model at the node's place (as RingDraw / WaveDraw / SwirlDraw / FlareDraw / BillboardDraw /
	// Billboard2Draw), the header with the given cue
	static void NodeModel(uint32_t fn, uint32_t n, Cue q)
	{
		Mat4x3 m;
		uint32_t model = 0;
		int32_t shift = 2;
		if (fn == ORIG_Billboard || fn == ORIG_Billboard2)
		{
			int16_t angles[3] = { 0, 0, 0 };
			ComposeZYXRotationMatrix(angles, &m);
			const int32_t s = At<int16_t>(n, 0x1C);
			const int32_t scale[3] = { s, fn == ORIG_Billboard ? At<int16_t>(n, 0x20) : s, 0 };
			if (fn == ORIG_Billboard2)
			{
				m.t[0] = 0;
				m.t[1] = 0;
				m.t[2] = var<int16_t>(0x1D8E038);
			}
			Scale3DMatrix(&m, scale);
			ScreenAnchor((const int16_t *)(n + 0x10), 0x1000, 0);
			GteSetRotMatrix(&m);
			model = 0xDE87FC;
			shift = 0xE;
		}
		else
		{
			int16_t angles[3] = { 0, At<int16_t>(n, 0x18), 0 };
			ComposeZYXRotationMatrix(angles, &m);
			m.t[0] = At<int16_t>(n, 0x10);
			m.t[1] = At<int16_t>(n, 0x12);
			m.t[2] = At<int16_t>(n, 0x14);
			const int32_t s = At<int16_t>(n, 0x1C);
			const int32_t scale[3] = { s, fn == ORIG_WaveTask ? At<int16_t>(n, 0x20) : s, s };
			Scale3DMatrix(&m, scale);
			if (fn == ORIG_RingTask)
			{
				TransformCameraByShadowRotation((const void *)(n + 0x10), At<int16_t>(n, 0x1C), -(int32_t)Push());
				GteSetRotMatrix(&m);
				model = 0xDE4A54;
			}
			else
			{
				ComposeAffineTransform(&Camera(), &m, &m);
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
				model = fn == ORIG_WaveTask ? 0xDE562C : fn == ORIG_SwirlTask ? 0xDE7F54 : 0xDE98E4;
				if (fn == ORIG_FlareTask) shift = 3;
			}
		}
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		At<uint32_t>(h, 0) = model;
		At<uint32_t>(h, 8) = 0;
		ApplyCue(h, q);
		PacketCursor() = RenderPrimModel((void *)h, OT44(), shift, PacketCursor());
		FieldFree(0x58);
	}

	static void HeldNode(const Rec &r, int num, int den)
	{
		const uint32_t a = (uint32_t)r.copy;
		if (!NodeNext(r))
		{
			if (num < den) NodeModel(r.fn, a, NodeCue(r.fn, a)); // its last draw: holds
			return;
		}
		alignas(4) uint8_t d[0x24];
		memcpy(d, r.copy, sizeof(d));
		const uint32_t n = (uint32_t)d;
		At<int16_t>(n, 0x18) = lerp16(At<int16_t>(a, 0x18), At<int16_t>(r.node, 0x18), num, den);
		At<int16_t>(n, 0x1C) = lerp16(At<int16_t>(a, 0x1C), At<int16_t>(r.node, 0x1C), num, den);
		NodeModel(r.fn, n, LerpCue(NodeCue(r.fn, a), NodeCue(r.fn, r.node), num, den));
	}

	// ------------------------------------------------------------------
	// the melt
	// ------------------------------------------------------------------
	// vertex workspace bytes a model's largest object writes (object table at model +4)
	static uint32_t WorkspaceBytes(uint32_t model)
	{
		if (!model || !At<uint32_t>(model, 4)) return 0;
		const uint32_t ob = At<uint32_t>(model, 4);
		const int32_t count = At<int32_t>(ob, 0);
		uint32_t most = 0;
		for (int32_t o = 0; o < count; o++)
		{
			uint32_t d = ob + At<uint32_t>(ob, 4 + 4 * (uint32_t)o);
			int32_t groups = At<int16_t>(d, 0);
			d += 2;
			uint32_t verts = 0;
			for (; groups > 0; groups--)
			{
				const int32_t nv = At<int16_t>(d, 2);
				d += 4;
				if (nv > 0)
				{
					verts += (uint32_t)nv;
					d += 6 * (uint32_t)nv;
				}
			}
			if (verts > most) most = verts;
		}
		return most * 8;
	}

	// skeleton of an anim header (BattleAnimHeader: +4 -> pointer -> bone count, matrices)
	static uint8_t *Skeleton(uint32_t anim, uint32_t *bytes)
	{
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_bones_save[0])) n = sizeof(g_bones_save[0]);
		*bytes = n;
		return sk;
	}

	static uint32_t ShardTint(int16_t age)
	{
		if (age >= 0x30) return 0xFFFFFF;
		uint32_t c = 0;
		GteLoadRGBC((uint32_t)&c);
		GteSetIR0((age << 12) / 48);
		GteDPCS();
		GteStoreRGB2((uint32_t)&c);
		return c;
	}

	// MeltFaces on a held frame: A = the faces / shards as drawn (g_faces / g_shards), B = as the tick
	// left them; the intact faces' corners are the in-between pose's (vertex workspace)
	static uint32_t HeldFaces(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor, int num, int den)
	{
		uint32_t pkt = cursor;
		const uint32_t vb = At<uint32_t>(h, 4);
		uint32_t st = At<uint32_t>(h, 0x2C);
		uint32_t rec = At<uint32_t>(h, 0);
		for (int kind = 0; kind < 2; kind++)
		{
			const int nv = kind == 0 ? 3 : 4;
			const uint32_t count = At<uint16_t>(h, kind == 0 ? 8 : 0xA);
			for (uint32_t i = 0; i < count; i++, rec += (kind == 0 ? 0x10 : 0x14), st += 2)
			{
				const uint32_t off = st - FaceStates();
				if (off + 2 > FACE_BYTES) continue;
				const uint16_t sa = *(const uint16_t *)(g_faces + off), sb = At<uint16_t>(st, 0);
				if (sa & 0x8000) continue;
				bool overlay = false;
				uint32_t tint = 0;
				if (sa & 0x4000)
				{
					for (int k = 0; k < nv; k++)
					{
						const uint32_t idx = At<uint16_t>(rec, k * 2) & 0xFFF;
						At<uint32_t>(h, 0x58 + k * 8) = At<uint32_t>(vb, idx * 8);
						At<uint32_t>(h, 0x5C + k * 8) = At<uint32_t>(vb, idx * 8 + 4);
					}
					if (!(sb & 0xC000) && sb < 1000)
					{
						// broke off on the tick: where it broke (its shard's start); the shard at 1
						const uint32_t eb = ShardTable() + sb * 0x44;
						for (int k = 0; k < nv * 2; k++)
							At<uint32_t>(h, 0x58 + k * 4) = At<uint32_t>(eb, 4 + k * 4);
						if (num >= den)
						{
							overlay = true;
							tint = ShardTint(At<int16_t>(eb, 0));
						}
					}
				}
				else if (sa < 1000)
				{
					const uint8_t *ea = g_shards + sa * 0x44;
					const uint32_t eb = ShardTable() + sa * 0x44;
					const int16_t age_a = *(const int16_t *)ea;
					overlay = true;
					if (sb == sa)
					{
						for (int k = 0; k < nv; k++)
						{
							At<uint32_t>(h, 0x5C + k * 8) = *(const uint32_t *)(ea + 8 + k * 8);
							for (int j = 0; j < 3; j++)
								At<int16_t>(h, 0x58 + k * 8 + j * 2) = lerp16(*(const int16_t *)(ea + 4 + k * 8 + j * 2), At<int16_t>(eb, 4 + k * 8 + j * 2), num, den);
						}
						const uint32_t ta = ShardTint(age_a);
						const uint32_t tb = ShardTint(At<int16_t>(eb, 0));
						tint = num <= 0 ? ta : num >= den ? tb : LerpColour(ta, tb, num, den);
					}
					else
					{
						if (num >= den) continue; // ended on the tick: holds
						for (int k = 0; k < nv * 2; k++)
							At<uint32_t>(h, 0x58 + k * 4) = *(const uint32_t *)(ea + 4 + k * 4);
						tint = ShardTint(age_a);
					}
				}
				else continue;
				At<uint32_t>(h, 0x38) = tint;
				pkt = FaceDraw(h, rec, nv, overlay, ot, shift, pkt);
			}
		}
		At<uint32_t>(h, 0x2C) = st;
		return pkt;
	}

	// MeltModel on a held frame (the vertices at the in-between pose's bone matrices)
	static uint32_t HeldMeltModel(uint32_t model, uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor, int num, int den)
	{
		const uint32_t bones = At<uint32_t>(model, 0) + 0x10;
		uint32_t offs = At<uint32_t>(model, 4);
		const int32_t count = At<int32_t>(offs, 0);
		offs += 4;
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t obj0 = At<uint32_t>(model, 4) + At<uint32_t>(offs, 0);
			offs += 4;
			if (!(At<uint32_t>(h, 0x20) & (1u << (i & 31)))) continue;
			uint32_t obj = obj0;
			int32_t groups = At<int16_t>(obj, 0);
			uint32_t out = At<uint32_t>(h, 4);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = At<int16_t>(obj, 0);
				obj += 2;
				const Mat4x3 *m = (const Mat4x3 *)(bones + bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				int32_t nv = At<int16_t>(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					GteLoadV0Words(obj);
					obj += 6;
					GteMVMVA_RotV0Tr2();
					GteStoreIRVertex(out);
					out += 8;
				}
			}
			const uint32_t a = (obj + 3) & ~3u;
			At<uint16_t>(h, 8) = At<uint16_t>(a, 0);
			At<uint16_t>(h, 0xA) = At<uint16_t>(a, 2);
			At<uint16_t>(h, 0xC) = At<uint16_t>(a, 4);
			At<uint16_t>(h, 0xE) = At<uint16_t>(a, 6);
			At<uint32_t>(h, 0) = a + 0xC;
			GteSetFarColour(0xFF, 0xFF, 0xFF);
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			cursor = HeldFaces(h, ot, shift, cursor, num, den);
		}
		return cursor;
	}

	static void HeldMelt(const Rec &r, int num, int den)
	{
		const int16_t c = At<int16_t>((uint32_t)r.copy, 0xC);
		if (c == 0) return; // the start ticks only: nothing drawn
		const bool next = (int16_t)(g_still ? c : c + 1) < 0xA0;
		if (!next)
		{
			if (num >= den) return; // its last draw: holds
			num = 0;
		}
		uint8_t *entity = Caster();
		const uint32_t model = *(const uint32_t *)(entity + 0x64);
		uint32_t sk_bytes = 0;
		uint8_t *sk = Skeleton((uint32_t)(entity + 0x60), &sk_bytes);
		if (sk) memcpy(g_bones_save[0], sk, sk_bytes);
		const uint32_t h = (uint32_t)FieldAlloc(0xB0);
		At<uint32_t>(h, 0x2C) = FaceStates();
		At<int32_t>(h, 0x44) = c;
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		ComputeBonesWorldMatrices(entity + 0x60, entity + 0x40);
		At<uint32_t>(h, 4) = var<uint32_t>(0x1D98B3C);
		At<int16_t>(h, 0x14) = 0;
		At<int16_t>(h, 0x16) = 0;
		At<uint32_t>(h, 0x1C) = *(const uint32_t *)(entity + 0x28);
		At<uint32_t>(h, 0x20) = *(const uint32_t *)(entity + 0x7C);
		At<uint32_t>(h, 0x10) = var<uint32_t>(0x1D969A8);
		const uint8_t b = entity[7];
		At<uint8_t>(h, 0x2A) = b;
		At<uint8_t>(h, 0x29) = b;
		At<uint8_t>(h, 0x28) = b;
		At<int16_t>(h, 0x18) = 0x140;
		At<int16_t>(h, 0x1A) = 0xD8;
		FrameArena() = HeldMeltModel(model, h, OT44(), 2, FrameArena(), num, den);
		BuildBoneMatricesFromPose(entity + 0x60);
		if (sk) memcpy(sk, g_bones_save[0], sk_bytes);
		FieldFree(0xB0);
	}

	// ------------------------------------------------------------------
	// stage group 1, as drawn
	// ------------------------------------------------------------------
	static void HeldStage(const Rec &r, int num, int den)
	{
		const int16_t c = At<int16_t>((uint32_t)r.copy, 0xC);
		if ((int16_t)(g_still ? c : c + 1) >= 0x78 && num >= den) return; // its last draw
		uint32_t sk_bytes = 0;
		uint8_t *sk = Skeleton(0x1D989D0, &sk_bytes);
		if (sk) memcpy(g_bones_save[1], sk, sk_bytes);
		const uint32_t h = (uint32_t)FieldAlloc(0x2C);
		At<int16_t>(h, 0x14) = 0;
		At<int16_t>(h, 0x16) = 0;
		At<int16_t>(h, 0x18) = 0x140;
		At<int16_t>(h, 0x1A) = 0xD8;
		At<uint32_t>(h, 4) = var<uint32_t>(0x1D98B3C);
		At<uint32_t>(h, 0x10) = var<uint32_t>(0x1D969A8);
		At<int32_t>(h, 0x20) = -1;
		BuildBoneMatricesFromPose((void *)0x1D989D0);
		ComputeBonesWorldMatrices((void *)0x1D989D0, (void *)&Camera());
		At<uint32_t>(h, 0x1C) = var<uint32_t>(0x1D989E4);
		At<int16_t>(h, 0x24) = var<int16_t>(0x1D989BE);
		FrameArena() = RenderGeometry(0x1D989C0, (void *)h, OT44(), 4, FrameArena());
		FieldFree(0x2C);
		if (sk) memcpy(sk, g_bones_save[1], sk_bytes);
	}

	// ------------------------------------------------------------------
	static void HeldFrame(int num, int den)
	{
		if (g_nrec == 0) return;
		// a held frame larger than the private buffers is not drawn (never seen)
		if (g_packets && PacketCursor() - g_packets > sizeof(g_held_packets)) return;
		if (g_frame && FrameArena() - g_frame > sizeof(g_held_frame)) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const uint32_t arena = FrameArena();
		// the private frame arena starts as the real one from where the tick's draws started, so the
		// bytes a packet does not write (pad halves) are the real ones
		const uint32_t base = g_frame ? g_frame : arena;
		const uint32_t limit = var<uint32_t>(0x1D969A8);
		uint32_t fill = limit > base ? limit - base : 0;
		if (fill > sizeof(g_held_frame)) fill = sizeof(g_held_frame);
		memcpy(g_held_frame, (const void *)base, fill);
		FrameArena() = (uint32_t)g_held_frame;
		// what the draws write outside the buffers
		memcpy(g_anchor_save, (const void *)BILLBOARD, sizeof(g_anchor_save));
		const uint32_t last_packet = var<uint32_t>(0x1D99C18);
		const uint32_t ws = var<uint32_t>(0x1D98B3C);
		uint32_t ws_bytes = 0;
		if (ws)
		{
			const uint32_t m1 = WorkspaceBytes(*(const uint32_t *)(Caster() + 0x64));
			const uint32_t m2 = WorkspaceBytes(0x1D989C0);
			ws_bytes = m1 > m2 ? m1 : m2;
			if (ws_bytes > sizeof(g_ws_save)) ws_bytes = sizeof(g_ws_save);
			memcpy(g_ws_save, (const void *)ws, ws_bytes);
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(FieldScratch(), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_MeltTask: HeldMelt(r, num, den); break;
			case ORIG_StageTask: HeldStage(r, num, den); break;
			case ORIG_FadeTask: HeldFade(r, num, den); break;
			case ORIG_RingTask: case ORIG_WaveTask: case ORIG_SwirlTask: case ORIG_FlareTask:
			case ORIG_Billboard: case ORIG_Billboard2: HeldNode(r, num, den); break;
			default: HeldPool(r, num, den); break;
			}
		}
		if (ws_bytes) memcpy((void *)ws, g_ws_save, ws_bytes);
		var<uint32_t>(0x1D99C18) = last_packet;
		memcpy((void *)BILLBOARD, g_anchor_save, sizeof(g_anchor_save));
		FrameArena() = arena;
	}
}

	static void register_mag066_held()
	{
		using namespace gu066;
		const uint32_t held[] = { ORIG_FadeTask, ORIG_SparkTask, ORIG_RingTask, ORIG_PuffTask, ORIG_DustTask, ORIG_SmokeTask, ORIG_MeltTask,
			ORIG_GlowTask, ORIG_WaveTask, ORIG_SwirlTask, ORIG_FlareTask, ORIG_ShardSpin, ORIG_SparkRain, ORIG_Billboard,
			ORIG_Billboard2, ORIG_DustFall2, ORIG_DustFall, ORIG_StageTask };
		for (uint32_t f : held) mark_held(f);
		register_module_held(66, HeldReady, HeldFrame);
	}
}
