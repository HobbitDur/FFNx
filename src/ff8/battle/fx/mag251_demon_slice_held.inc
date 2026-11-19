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

// 30 fps part of mag251_demon_slice.cpp (Demon Slice, effect 251). Exact vanilla shapes: the logic stays at 15 Hz, a
// held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the main task draws each phase's layout before its moves). A real tick records its
// draws in order (the master's seam starts the log) with the GTE state and the Field_Alloc scratch each one started
// from; a held frame replays them in the same order into private packets, with the live camera (the frame's view in
// 0x1D97778; the orthonormal view 0x2556458 rebuilt from it):
//   Layouts  - (main task) prim::play_held with the play block at its in-between state: 0x142FB40 its fade along its
//              ramp (the switch from no fade at 0x54: tick N's records held), the wave layout its wave phase / texture scroll / swell /
//              height scale num/den of the way to the next tick's, both at the caster's spawn point num/den of the way
//              to the next tick's (the caster's bone 0 at his NEXT pose, pose_midpoint at 1, through his world matrix as
//              the frame holds it: the next tick's; up to 0x70, then fixed as the tick keeps it); 0x1433318 its scroll;
//              0x1432448 / 0x142D8A4 fixed. Blended / waved vertices go to a private buffer.
//   Layout tasks - prim::play_held at their point; their fade word (the play block's never-written +0x28, see the .cpp)
//              num/den of the way to the value the next tick leaves there (PredictTaskFade; a switch to / from 0 holds).
//   Tiles    - the white flash level num/den along its series.
//   Target   - (0xC0..0xDA, drawn by the module) at his pose num/den of the way (pose_midpoint), his middle measured at
//              that pose, his drop height (+0x1E, the matrix translation y the draw uses) num/den of the way to the next
//              tick's, his world matrix x / z from the drawn one to the one the frame holds; bones / matrix put back.
//   Trail    - the next tick's ring predicted exactly (slot 4's bone 1 at its NEXT pose through its world matrix as the
//              frame holds it), splined on private buffers; each spline point num/den of the way. Holds when the trail
//              ends or the next tick draws no trail.
//   Flames / sparks / embers / sparkles - (Fire's parts) the node num/den of the way to its update (sparks / sparkles
//              along their velocity, embers rising, sizes shrinking); flipbook frames switch on the real frame (at 1 the
//              next tick's frame); a flame queue the main task drops (counter 0x21) or a part ending on the tick holds
//              up to 1 and is not drawn at 1.
// A part the next tick does not draw holds up to 1 and is not drawn at 1. Spawns, the stream load, sounds, the hit
// chains, the result and the screen flash are real-tick only.
// Entities: the caster (slides, swings, lift, return, put back) and the target (height, push) are drawn by the engine:
// HeldEntities lists both to the frame gate (continuous, the discrete changes marked as jumps: the swing's snap back to
// the far point every 5 ticks from 0x77, the put-back at 0xDC; the target's height zeroed at the start, its first lift
// at 0x8D, its reset at 0xC0 / 0xC7).
// Nothing here writes game state: the module globals, the packet cursor and the frame-arena cursor (private buffers
// holding the real tick's packets under the replay), the Field_Alloc scratch, the shadow-rotation scratch, the screen
// vertex buffer (0x1D98B3C), the sprite words, the word 0x1D9770C, the GTE registers, the entities' bone / world
// matrices are put back.

namespace ff8fx
{
namespace demon251
{
	enum RecKind : uint8_t { R_SPRITE, R_LAYOUT, R_TILE, R_TARGET, R_TRAIL, R_TASKLAYOUT };
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;             // module packet arena per tick
	static const uint32_t FARENA = 0x40000;            // frame arena (the target's model)
	static const uint32_t BSS_LO = 0x2556388, BSS_HI = 0x2556478;
	static const int MAX_RECS = 0x100;
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }   // the frame arena cursor (the target's draw)

	struct Rec
	{
		RecKind kind;
		uint32_t fn;                 // sprite: the task
		const void *node;            // sprite / trail / layout task: the node
		uint8_t *layout;             // layout: the layout drawn
		int site;                    // layout: 0..4 (0x142FB40, wave, 0x1433318, 0x1432448, 0x142D8A4)
		int32_t i;                   // layout / tile / target: the phase index; trail: the counter
		int32_t c;                   // the main counter of the tick
		int16_t xyz[3];              // layout: its point
		int16_t off[3];              // target: the offset drawn
		int16_t y;                   // target: his height as drawn (+0x1E, his matrix translation y)
		Mat4x3 world;                // target: his world matrix as drawn
		PlayArg arg;                 // layout / layout task: the play block
		uint8_t snode[0x24];         // sprite: the node as drawn
		uint8_t hdr[0xB4];           // sprite: the scratch under its draw header
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_paused = false;
	static int32_t g_main_c = -1;          // the main task's counter on this tick (-1: it did not run)
	static const MainNode *g_main = nullptr;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static Rec g_recs[MAX_RECS];
	static int16_t g_trail_sa[32][4], g_trail_sb[32][4];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_morph[0x4000];   // blended / waved vertices
	// predictions of the held frame
	static int16_t g_sp_next[4];
	static bool g_sp_ok = false;
	static int16_t g_na[32][4], g_nb[32][4];
	static uint8_t g_bones_save[16 + 48 * 256];

	static const TaskNode *g_master = nullptr;
	static void held_note_master(const TaskNode *master)
	{
		g_master = master;
		g_ported_tick = g_real_tick;
		g_paused = Pause() != 0;
		g_main_c = -1;
		g_main = nullptr;
		g_nrec = 0;
		g_packets = 0;
		g_frame_start = 0;
	}

	static void held_note_main(const MainNode *mn)
	{
		if (g_ported_tick != g_real_tick) return;
		g_main_c = mn->counter;
		g_main = mn;
	}

	static Rec *NewRec(RecKind kind)
	{
		g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.fn = 0;
		r.node = nullptr;
		r.layout = nullptr;
		r.c = g_main_c;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_draw(uint32_t fn, const void *node)
	{
		Rec *r = NewRec(R_SPRITE);
		if (!r) return;
		r->fn = fn;
		r->node = node;
		memcpy(r->snode, node, fn == ORIG_SparkleTask ? sizeof(SparkleNode) : 0x24);
		g_hdr_slot = r->hdr;  // the draw's header scratch (AllocHeader)
	}

	static int32_t SiteIndex(int site, int32_t c)
	{
		static const int32_t start[5] = { 0x49, 0x53, 0x72, 0xA3, 0xC8 };
		return c - start[site];
	}

	static void held_note_layout(MainNode *mn, int site, uint8_t *layout, const PlayArg *arg, int32_t x, int32_t y, int32_t z)
	{
		Rec *r = NewRec(R_LAYOUT);
		if (!r) return;
		r->site = site;
		r->layout = layout;
		r->c = mn->counter;
		r->i = SiteIndex(site, mn->counter);
		r->arg = *arg;
		r->xyz[0] = (int16_t)x;
		r->xyz[1] = (int16_t)y;
		r->xyz[2] = (int16_t)z;
	}

	static void held_note_tile(int32_t t)
	{
		Rec *r = NewRec(R_TILE);
		if (r) r->i = t;
	}

	static void held_note_target(const MainNode *mn, int32_t i, const int16_t *off)
	{
		Rec *r = NewRec(R_TARGET);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->c = mn->counter;
		r->i = i;
		memcpy(r->off, off, 6);
		r->y = *(const int16_t *)(Target() + 0x1E);
		r->world = *(const Mat4x3 *)(Target() + 0x40);
	}

	static void held_note_trail(const TrailNode *t)
	{
		Rec *r = NewRec(R_TRAIL);
		if (!r) return;
		r->node = t;
		r->i = t->counter;
		memcpy(g_trail_sa, t->spl_a, sizeof(g_trail_sa));
		memcpy(g_trail_sb, t->spl_b, sizeof(g_trail_sb));
	}

	static void held_note_task_layout(const LayoutNode *t, const PlayArg *arg)
	{
		Rec *r = NewRec(R_TASKLAYOUT);
		if (!r) return;
		r->node = t;
		r->layout = (uint8_t *)t->layout;
		r->arg = *arg;
		memcpy(r->xyz, t->pos, 6);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the next tick runs the main task again (and so the master's queues)
	static bool MainNext() { return g_main_c >= 0 && g_main_c < 0xDC; }

	// a battle model's skeleton block (bone matrices) and its size
	static uint8_t *Skeleton(uint8_t *hdr, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(hdr + 4);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= sizeof(g_bones_save) ? sk : nullptr;
	}

	// the caster's spawn point (0x502170, bone 0) at his next pose through his world matrix as the frame holds it
	static void PredictSpawn()
	{
		uint8_t *e = Caster();
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(e + 0x60, &nb);
		if (sk) memcpy(g_bones_save, sk, nb);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		if (sk) pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
		GetEffectSpawnPosition(e, 0, 0, g_sp_next);
		var<uint16_t>(0x1D9770C) = w70c;
		if (sk) memcpy(sk, g_bones_save, nb);
		g_sp_ok = true;
	}

	// ---- layouts of the main task ----
	static int16_t FadeOf(int32_t i) { return i > 10 ? (int16_t)((uint32_t)shl32(i - 10, 12) / 10) : 0; }
	static int16_t SwellOf(int32_t i) { return i >= 0x50 ? (int16_t)(ComputeSin((int32_t)((uint32_t)shl32(i - 0x50, 10) / 15)) * 2 + 0x1000) : (int16_t)0x1000; }
	static int16_t HeightScaleOf(int32_t c)
	{
		if (c < 0x8D) return 0x1000;
		int32_t u = shl32(c - 0x8D, 12) / 27;
		if (u > 0x1000) u = 0x1000;
		return (int16_t)(u + 0x1000);
	}

	// a layout at k/den: below 1 the player's in-between records (play_held: an object appearing or disappearing on the next
	// tick does so on the real frame); at 1 (the harness's look at the next tick; the frame gate draws num < den only) the
	// next tick's draw exactly: the player run on the layout (its state is inline in the Layout block), the block put back
	// (play notes it drew the next frame: the last held frame of the tick)
	static uint8_t g_layout_save[0x800];
	static void PlayAt(uint8_t *layout, uint32_t size, prim::Callback cb, int arg, int k, int den)
	{
		if (k == den && size <= sizeof(g_layout_save))
		{
			memcpy(g_layout_save, layout, size);
			prim::play((prim::Layout *)layout, cb, arg, 0);
			memcpy(layout, g_layout_save, size);
		}
		else prim::play_held((prim::Layout *)layout, cb, arg, k, den);
	}

	static void HeldLayout(const Rec &r, int num, int den, bool at_next)
	{
		static const int32_t length[5] = { 0x14, 0x5F, 0x19, 0xE, 0x14 };
		const int32_t ni = r.i + 1;
		const bool next = !g_paused && MainNext() && ni < length[r.site];
		if (at_next && !next) return;
		const int k = next ? num : 0;
		int kp = k; // the records' fraction
		PlayArg a = r.arg;
		int32_t x = r.xyz[0], z = r.xyz[2];
		if (k)
		{
			if (r.site == 0 || r.site == 1)
			{
				// the spawn point: the next tick's up to 0x70 (from 0x71 the tick keeps the one of 0x70)
				if (r.c + 1 < 0x71 && g_sp_ok)
				{
					x = lerp16(r.xyz[0], g_sp_next[0], k, den);
					z = lerp16(r.xyz[2], g_sp_next[2], k, den);
				}
			}
			if (r.site == 0)
			{
				const int16_t f0 = FadeOf(r.i), f1 = FadeOf(ni);
				// (no fade -> fade (0x53 -> 0x54): the fade replaces the records' own colour blend (rgb by a) with black by the
				// fade, two blends no single in-between draw spans: below 1 the held frame keeps tick N's records and no fade,
				// the switch happens at 1)
				if (f0 != 0 || k == den) a.fade = lerp16(f0, f1, k, den);
				else if (f1 != 0) kp = 0;
			}
			else if (r.site == 1)
			{
				a.wave_phase = lerp16((int16_t)(r.i << 6), (int16_t)(ni << 6), k, den);
				a.scroll = (uint16_t)lerp16((int16_t)(r.i << 5), (int16_t)(ni << 5), k, den);
				a.wave_xz = lerp16(SwellOf(r.i), SwellOf(ni), k, den);
				a.wave_y = lerp16(HeightScaleOf(r.c), HeightScaleOf(r.c + 1), k, den);
			}
			else if (r.site == 2) a.scroll = (uint16_t)lerp16((int16_t)(r.i << 5), (int16_t)(ni << 5), k, den);
		}
		RotationY(0, &a.m);
		a.m.t[0] = x;
		a.m.t[1] = r.xyz[1];
		a.m.t[2] = z;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		static const uint32_t size[5] = { 0x1C8, 0x24C, 0x1A0, 0x144, 0x208 }; // (their decode sizes)
		PlayAt(r.layout, size[r.site], PrimCallback, (int)&a, kp, den);
	}

	// ---- layout tasks ----
	static bool NodeAlive(const void *node, uint32_t fn)
	{
		const TaskNode *t = (const TaskNode *)node;
		return (t->flags & 1) != 0 && (uint32_t)t->func == fn;
	}

	// the layout tasks' fade word on the next tick (mag251_demon_slice.cpp g_task_fade): the composed y translation of the
	// last layout the main task plays then (its place as the next tick sets it, the frame's camera), else the master node's
	// low half; a sparkle drawing on the next tick leaves its OT pointer there (held as on this tick)
	static bool g_fade_ok = false;
	static int16_t g_fade_next = 0;
	static void PredictTaskFade()
	{
		g_fade_ok = false;
		if (g_paused || !MainNext() || !g_main) return;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == R_SPRITE && g_recs[i].fn == ORIG_SparkleTask && ((const SparkleNode *)g_recs[i].snode)->frame + 1 < PG().sparkles.frames) return;
		const int32_t cn = g_main_c + 1;
		int32_t x, y = 0, z;
		if ((uint32_t)(cn - 0xC8) < 0x14) { x = g_main->target0[0]; z = g_main->target0[2]; }
		else if ((uint32_t)(cn - 0xA3) < 0xE)
		{
			y = -0x1000;
			if (cn == 0xA3)
			{
				int16_t p[4];
				const uint16_t w70c = var<uint16_t>(0x1D9770C);
				GetDefaultEffectPosition(Target(), p);
				var<uint16_t>(0x1D9770C) = w70c;
				x = p[0];
				z = p[2];
			}
			else { x = g_main->far_pt[0]; z = g_main->far_pt[2]; }
		}
		else if ((uint32_t)(cn - 0x72) < 0x19)
		{
			if (cn == 0x72) { x = (int16_t)g_main->spawn[0]; z = (int16_t)g_main->spawn[1]; }
			else { x = g_main->keep[0]; z = g_main->keep[2]; }
		}
		else if ((uint32_t)(cn - 0x53) < 0x5F || (uint32_t)(cn - 0x49) < 0x14)
		{
			if (cn < 0x71)
			{
				if (!g_sp_ok) return;
				x = g_sp_next[0];
				z = g_sp_next[2];
			}
			else { x = (int16_t)g_main->spawn[0]; z = (int16_t)g_main->spawn[1]; }
		}
		else
		{
			if (!g_master) return;
			g_fade_next = (int16_t)(uint32_t)g_master;
			g_fade_ok = true;
			return;
		}
		Mat4x3 m;
		RotationY(0, &m);
		m.t[0] = x;
		m.t[1] = y;
		m.t[2] = z;
		ComposeAffineTransform(&Camera(), &m, &m);
		g_fade_next = (int16_t)m.t[1];
		g_fade_ok = true;
	}

	static void HeldTaskLayout(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = !g_paused && MainNext() && NodeAlive(r.node, ORIG_LayoutTask);
		if (at_next && !next) return;
		const int k = next ? num : 0;
		PlayArg a = r.arg;
		// the fade word num/den of the way to the next tick's (a switch to or from no fade on the real frame)
		if (k && g_fade_ok && (a.fade != 0) == (g_fade_next != 0)) a.fade = lerp16(a.fade, g_fade_next, k, den);
		else if (at_next && g_fade_ok) a.fade = g_fade_next;
		memset(&a.m, 0, sizeof(a.m));
		a.m.m[0][0] = 0x1000;
		a.m.m[1][1] = 0x1000;
		a.m.m[2][2] = 0x1000;
		a.m.t[0] = r.xyz[0];
		a.m.t[1] = r.xyz[1];
		a.m.t[2] = r.xyz[2];
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		PlayAt(r.layout, sizeof(((LayoutNode *)nullptr)->layout), PrimCallback, (int)&a, k, den);
	}

	// ---- white flash tiles ----
	static void HeldTile(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = !g_paused && MainNext() && r.i + 1 < 4;
		if (at_next && !next) return;
		const int k = next ? num : 0;
		const int32_t level = lerp_i(FadeLevel(r.i, 0, 1, 3), FadeLevel(r.i + 1, 0, 1, 3), k, den);
		PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
	}

	// ---- the module-drawn target ----
	static void HeldTarget(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t ni = r.i + 1;
		const bool next = !g_paused && MainNext() && ni < 0x1B;
		if (at_next && !next) return;
		const int k = next ? num : 0;
		uint8_t *e = Target();
		// his height as the next draw makes it: the drop (0xC0..0xC6), his start height (0xC7), then unchanged
		int16_t y1 = *(const int16_t *)(e + 0x1E);
		if (ni < 7) y1 = DropHeight(ni);
		else if (ni == 7 && g_main) y1 = g_main->target0[1];
		const Mat4x3 keep = *(const Mat4x3 *)(e + 0x40);
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(e + 0x60, &nb);
		if (sk) memcpy(g_bones_save, sk, nb);
		Mat4x3 *w = (Mat4x3 *)(e + 0x40);
		*w = r.world;
		w->t[0] = lerp_i(r.world.t[0], keep.t[0], k, den);
		w->t[2] = lerp_i(r.world.t[2], keep.t[2], k, den);
		const int32_t y = lerp_i(r.y, y1, k, den);
		const int16_t live_y = *(const int16_t *)(e + 0x1E);
		*(int16_t *)(e + 0x1E) = (int16_t)y;
		if (k && sk) pose_midpoint(e + 0x60, e + 0x6C, k, den);
		int16_t box[2];
		MeasureHeight(e, box);
		*(int16_t *)(e + 0x1E) = live_y;
		const int16_t off[4] = { 0, (int16_t)((box[1] + box[0]) >> 1), 0, 0 };
		const int16_t angles[4] = { 0x800, 0, 0, 0 };
		if (k && sk) pose_midpoint(e + 0x60, e + 0x6C, k, den);
		DrawEntity212(e, angles, k ? off : r.off);
		if (sk) memcpy(sk, g_bones_save, nb);
		*w = keep;
	}

	// ---- the trail ----
	// the next tick's splines (false: the next tick draws no trail)
	// cn: the counter the next tick draws with (the recorded one + 1; the live node already holds it after its update)
	static bool TrailNext(const TrailNode *t, int32_t cn)
	{
		if (cn >= 0x13) return false;
		uint8_t *e = TRAIL_ENTITY;
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(e + 0x60, &nb);
		if (sk) memcpy(g_bones_save, sk, nb);
		if (sk) pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		WeaponBone(e, 1, w->dir, w->base);
		RingEntry ring[7];
		memcpy(ring, t->ring, sizeof(ring));
		int32_t idx = cn % 7;
		ring[idx].alive = 1;
		memcpy(ring[idx].base, w->base, 8);
		ring[idx].tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 777) >> 12) + w->base[0]);
		ring[idx].tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 777) >> 12) + w->base[1]);
		ring[idx].tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 777) >> 12) + w->base[2]);
		ring[idx].tip[3] = w->tip[3];
		FieldFree(0x40);
		if (sk) memcpy(sk, g_bones_save, nb);
		const int32_t want = cn >= 0xD ? 0x14 - cn : 7;
		static int16_t pa[8][4], pb[8][4];
		int32_t cnt = 0;
		while (cnt < want)
		{
			const RingEntry *q = &ring[idx];
			if (!q->alive) break;
			idx--;
			memcpy(pa[cnt], q->base, 8);
			memcpy(pb[cnt], q->tip, 8);
			if (idx < 0) idx = 6;
			cnt++;
		}
		if (cnt <= 1) return false;
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, wk);
		for (int32_t j = 0, u = 0; j < 32; j++, u += 0x1000) SplineEval(cnt, wk, g_na[j], u / 31);
		SplineSetup(cnt, pb, wk);
		for (int32_t j = 0, u = 0; j < 32; j++, u += 0x1000) SplineEval(cnt, wk, g_nb[j], u / 31);
		FieldFree(0x190);
		return true;
	}

	static void HeldTrail(const Rec &r, int num, int den, bool at_next)
	{
		const TrailNode *t = (const TrailNode *)r.node;
		const bool next = !g_paused && MainNext() && NodeAlive(t, ORIG_TrailTask);
		if (at_next && !next) return;
		const int16_t (*sa)[4] = g_trail_sa;
		const int16_t (*sb)[4] = g_trail_sb;
		static int16_t ma[32][4], mb[32][4];
		if (next && num != 0)
		{
			if (TrailNext(t, r.i + 1))
			{
				for (int j = 0; j < 32; j++)
					for (int q = 0; q < 4; q++)
					{
						ma[j][q] = lerp16(g_trail_sa[j][q], g_na[j][q], num, den);
						mb[j][q] = lerp16(g_trail_sb[j][q], g_nb[j][q], num, den);
					}
				sa = ma;
				sb = mb;
			}
			else if (at_next) return;
			// (the predictions used the scratch: the draw starts from the real one's)
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		}
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailDraw(sa, sb, w);
		FieldFree(0x40);
	}

	// ---- sprites (Fire's parts, sparkles) ----
	static void HeldSprite(const Rec &r, int num, int den, bool at_next)
	{
		const bool flames = MainNext() && g_main_c + 1 != 0x21;  // the main task drops the flame queue at 0x21
		g_hdr_slot = (uint8_t *)r.hdr;
		switch (r.fn)
		{
		case ORIG_FlameTask:
		{
			FlameNode a;
			memcpy(&a, r.snode, sizeof(a));
			const bool next = flames && a.frame + 1 < PG().flames.frames;
			if (at_next && !next) break;
			if (at_next && !g_paused) a.frame++;
			FlameDraw(&a.f, a.frame, FLAME_LOOK, PacketCursor());
			return;
		}
		case ORIG_SparkTask:
		{
			SparkNode a, b;
			memcpy(&a, r.snode, sizeof(a));
			b = a;
			if (!g_paused) FireSparkMove(&b.s, &b.life, 4, 0x40);
			const bool next = flames && b.life >= 0;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			SparkNode m = a;
			for (int q = 0; q < 3; q++) m.s.pos[q] = lerp16(a.s.pos[q], b.s.pos[q], k, den);
			m.s.scale = lerp16(a.s.scale, b.s.scale, k, den);
			if (at_next) m.life = b.life;
			FireSparkDraw(&m.s, m.life, SPARK_LOOK, PacketCursor());
			return;
		}
		case ORIG_EmberTask:
		{
			EmberNode a, b;
			memcpy(&a, r.snode, sizeof(a));
			b = a;
			if (!g_paused)
			{
				EmberRise(&b.e, 5, 6);
				b.frame++;
			}
			const bool next = flames && b.frame < PG().flames.frames;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			EmberNode m = a;
			m.e.pos[1] = lerp16(a.e.pos[1], b.e.pos[1], k, den);
			m.e.scale = lerp16(a.e.scale, b.e.scale, k, den);
			if (at_next) m.frame = b.frame;
			EmberDraw(&m.e, m.frame, fxp::sequence(PG().seqs.ember, SEQ_Ember), 3, PacketCursor());
			return;
		}
		case ORIG_SparkleTask:
		{
			SparkleNode a;
			memcpy(&a, r.snode, sizeof(a));
			const bool next = !g_paused && MainNext() && a.frame + 1 < PG().sparkles.frames;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			SparkleNode m = a;
			for (int q = 0; q < 3; q++) m.pos[q] = lerp16(a.pos[q], (int16_t)(a.pos[q] + a.vel[q]), k, den);
			if (at_next) m.frame = (int16_t)(a.frame + 1);
			TransformCameraByShadowRotation(m.pos, 0x1000, -0x400);
			uint8_t *h = AllocHeader(0xB4);
			*(uint32_t *)h = fxp::sequence(PG().seqs.sparkle, SEQ_Sparkle);
			*(uint16_t *)(h + 4) = (uint16_t)m.frame;
			*(uint16_t *)(h + 0x24) = 0;
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			FieldFree(0xB4);
			return;
		}
		default:
			break;
		}
		g_hdr_slot = nullptr;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8], vbuf[0x8000];
		const uint32_t vb = var<uint32_t>(0x1D98B3C);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		const uint32_t frame_saved = FramePackets();
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		if (vb) memcpy(vbuf, (const void *)vb, sizeof(vbuf));
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			if (g_frame_start && frame_saved > g_frame_start)
			{
				const uint32_t used = frame_saved - g_frame_start;
				memcpy(g_held_frame, (const void *)g_frame_start, used < FARENA ? used : FARENA);
			}
			if (g_frame_start) FramePackets() = (uint32_t)g_held_frame;
			// the orthonormal view of the frame's camera (the master's, every tick)
			EffectCameraMatrix(&Camera(), OrthoView());
			// the caster's next spawn point (the main task's layouts at it up to 0x70)
			g_sp_ok = false;
			if (num != 0 && !g_paused && MainNext() && g_main_c + 1 < 0x71)
				for (int i = 0; i < g_nrec; i++)
					if (g_recs[i].kind == R_LAYOUT && g_recs[i].site <= 1) { PredictSpawn(); break; }
			g_fade_ok = false;
			for (int i = 0; i < g_nrec; i++)
				if (g_recs[i].kind == R_TASKLAYOUT) { PredictTaskFade(); break; }
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
				switch (r.kind)
				{
				case R_SPRITE: HeldSprite(r, num, den, at_next); break;
				case R_LAYOUT: HeldLayout(r, num, den, at_next); break;
				case R_TILE: HeldTile(r, num, den, at_next); break;
				case R_TARGET: HeldTarget(r, num, den, at_next); break;
				case R_TRAIL: HeldTrail(r, num, den, at_next); break;
				case R_TASKLAYOUT: HeldTaskLayout(r, num, den, at_next); break;
				}
			}
			g_hdr_slot = nullptr;
		}
		FramePackets() = frame_saved;
		var<uint16_t>(0x1D9770C) = w70c;
		if (vb) memcpy((void *)vb, vbuf, sizeof(vbuf));
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}

	// ---- entities moved by the main task (drawn by the engine) ----
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (!HeldReady() || g_main_c < 0 || max < 2) return 0;
		const int32_t c = g_main_c;
		HeldEntity &cs = out[0];
		held_entity_init(cs, Caster());
		// the swing snaps back to the far point every 5 ticks; the put-back at the end
		if (((uint32_t)(c - 0x77) < 0x16 && (c - 0x72) % 5 == 0) || c >= 0xDC)
			for (int q = 0; q < 3; q++) held_entity_jump(cs, q);
		HeldEntity &tg = out[1];
		held_entity_init(tg, Target());
		if (c == 0 || c == 0x8D) held_entity_jump(tg, 1);    // height zeroed (start), first lift
		if (c == 0xC0) { held_entity_jump(tg, 0); held_entity_jump(tg, 2); }
		if (c == 0xC7) for (int q = 0; q < 3; q++) held_entity_jump(tg, q);
		return Caster() == Target() ? 1 : 2;
	}
}

	static void register_mag251_held()
	{
		mark_held(demon251::ORIG_FlameTask);
		mark_held(demon251::ORIG_SparkTask);
		mark_held(demon251::ORIG_EmberTask);
		mark_held(demon251::ORIG_MainTask);
		mark_held(demon251::ORIG_TrailTask);
		mark_held(demon251::ORIG_LayoutTask);
		mark_held(demon251::ORIG_SparkleTask);
		register_module_held(251, demon251::HeldReady, demon251::HeldFrame);
		register_module_entity(251, demon251::HeldEntities);
	}
}
