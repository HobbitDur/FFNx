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

// 30 fps part of mag089_wishing_star.cpp: the draw log of the real tick, the held-frame drawing and the
// held-frame camera (included at the end of mag089_wishing_star.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// A real tick records every draw group in order (with the GTE registers and the Field_Alloc scratch it
// started from); a held frame replays the log in the same order with the module's own draw code, on the
// state half way to the next tick's ("exact vanilla shapes"). Every draw of the module happens before the
// tick's updates (the director draws, then moves its particles / shards / stars; the tasks draw, then
// move), so the next tick's state is the live state the real tick left:
//   layouts   - prim::play_held at their placement (the director's four layouts at the centre / Angelo's
//               bone 0x13 (the anchor half way to the one the bones left by this tick give), the dives'
//               layouts at the centre + their z axis / 4); the last draw of a phase holds;
//   grow      - the prim model's scale half way (its sink follows); landing - its Z turn, the first
//               model's fade and the third's half way, the uv step and the blend switch on the real tick,
//               the anchor (Angelo's bone 0x13) half way;
//   stars     - the dots' twinkle levels half way from the drawn ones to the ones the tick's twinkle left
//               (a level 0 draws the plain colour: a step to or from 0 holds); the 8 groups' angles half way
//               (a safeguard: the setup zeroes their steps, they never turn);
//   shards    - (the captured screen in pieces, screen space) every shard the tick kept moved / turned
//               half way (centroid and vertices); a shard the update kills holds its last draw;
//   Angelo    - the bone matrices of the pose half way: the renderer (0x6A79B0) draws with the bone
//               matrices built before it (after the previous draw, or by a flash's bone point) and then
//               rebuilds them from the pose; the held frame builds the matrices from the pose half way
//               between the drawn source pose and the next draw's (each angle the short way round, as
//               pose_midpoint); an animation change (SetAnim on 7 / 0x1E / 0x20, a restart) or a frame turning
//               the bone scales on / off between them holds the drawn pose; hidden on the next tick (0xB1): holds;
//   tiles     - the fade level half way along its series; a stream wait (0x65 / 0xAD) draws grow / landing again;
//   particles - drawn THEN updated: particle models (position, scale, depth cue half way), sparkles
//               (position half way, flipbook frame by age: holds); one freed / reborn by the tick holds
//               its last draw; a system the next tick no longer draws holds;
//   flashes   - flipbook frame by tick: holds; debris - position and turn (12 bits, the short way) half
//               way; trails - position half way (the last dive's turn on its tick 3: holds).
// The screen capture (0x24) and the screen feedback request run on real ticks only (the captured texture
// stays); the sparks, particle bursts, damage, loads and sounds are real-tick only. Blended vertex frames:
// a private morph buffer. Everything a held draw writes in the module's state (tables, the pose block,
// 0x1D9770C) is put back. Rinoa's battle entity (moved by the director: +0x1C..+0x20, +0x58 / +0x5C) is
// drawn by the engine (its entity position is not interpolated here).
// Camera: HeldCamera runs the director's camera code for the next counter on saved words (Rinoa's entity
// position as the next tick sets it first) and returns the in-between look-at / eye, holding on cuts (the
// direct set at 0xA1, the put-back at 0xE3, keyed segments starting away) and while a stream wait holds
// the tick (0x24 / 0x65 / 0xAD with the loader busy).

namespace ff8fx
{
namespace ws089
{
	using namespace magc;

	static const uint32_t SCRATCH = 0x800;
	static const uint32_t ARENA = 0x10000;        // module packet arena per tick
	static const uint32_t FRAME = 0x40000;        // frame arena (sky, stars, shards, tiles, dive trails / layouts)
	static const int MAX_BONES = 64;
	static const uint32_t SK_MAX = 16 + 48 * MAX_BONES;

	enum RecKind : uint8_t { R_PLAY, R_GROW, R_LANDING, R_STARS, R_SHARDS, R_ANGELO, R_TILE, R_PMODELS, R_SPRITES, R_FLASH, R_DEBRIS, R_TRAIL, R_DIVEPLAY };
	struct Rec
	{
		RecKind kind;
		bool fa;                    // play: into the frame arena
		int32_t a, b, c, d, e, f, g; // per kind
		int16_t p[4];               // play / landing: anchor
		uint8_t *ptr;               // layout / node
		uint8_t node[0x30];         // flash / debris node; trail: the dive node +0..+0x2F
		Mat4x3 m;                   // dive play: its block matrix before the camera
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	// the pose values (root position + every bone's angles / scales) of a pose block (as 87's held part)
	struct Pose
	{
		int nb = 0;
		bool scaled = false;
		uint32_t gen = 0;           // animation generation (SetAnim / restart)
		int16_t root[3] = {};
		int16_t v[MAX_BONES][6] = {};
		void get(const uint8_t *sk)
		{
			nb = sk[0];
			scaled = (sk[1] & 1) != 0;
			memcpy(root, sk + 8, 6);
			for (int b = 0; b < nb && b < MAX_BONES; b++) memcpy(v[b], sk + 16 + 48 * b + 4, scaled ? 12 : 6);
		}
		void set(uint8_t *sk) const
		{
			memcpy(sk + 8, root, 6);
			for (int b = 0; b < nb && b < MAX_BONES; b++) memcpy(sk + 16 + 48 * b + 4, v[b], scaled ? 12 : 6);
		}
	};
	static int pm_round(int v, int num, int den) { const int t = v * num; return (t >= 0) ? (t + den / 2) / den : -((-t + den / 2) / den); }
	// the pose at num / den from a to b (as pose_midpoint: a bone rotation (x, y, z) is also written (2048 - x, y + 2048,
	// z + 2048); each bone blends toward the spelling of b nearer a, each angle the shortest way round)
	static void pose_lerp(const Pose &a, const Pose &b, int num, int den, Pose &out)
	{
		out = a;
		for (int k = 0; k < 3; k++) out.root[k] = (int16_t)(a.root[k] + pm_round(b.root[k] - a.root[k], num, den));
		for (int i = 0; i < a.nb && i < MAX_BONES; i++)
		{
			int dd[3], ee[3], sd = 0, se = 0;
			for (int k = 0; k < 3; k++) { dd[k] = ((b.v[i][k] - a.v[i][k] + 2048) & 4095) - 2048; sd += dd[k] < 0 ? -dd[k] : dd[k]; }
			ee[0] = ((2048 - b.v[i][0] - a.v[i][0] + 2048) & 4095) - 2048;
			ee[1] = ((b.v[i][1] - a.v[i][1]) & 4095) - 2048;
			ee[2] = ((b.v[i][2] - a.v[i][2]) & 4095) - 2048;
			for (int k = 0; k < 3; k++) se += ee[k] < 0 ? -ee[k] : ee[k];
			const int *d = se < sd ? ee : dd;
			for (int k = 0; k < 3; k++) out.v[i][k] = (int16_t)(a.v[i][k] + pm_round(d[k], num, den));
			if (a.scaled) for (int k = 3; k < 6; k++) out.v[i][k] = (int16_t)(a.v[i][k] + pm_round(b.v[i][k] - a.v[i][k], num, den));
		}
	}

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static Rec g_recs[96];
	static uint32_t g_packets = 0, g_frame = 0;   // module / frame packet cursors at the tick's first draw
	static DirectorNode *g_dir = nullptr;          // the director that ran this tick
	static int32_t g_dir_c = -1;                   // its counter at the start of the tick
	// tables as drawn
	static uint8_t g_levels[0x400];                // the dots' twinkle levels
	static int16_t g_star_ang[8][2], g_save_ang[8][2]; // the star groups' angles as drawn (0x14AF49C + 0x10 x group)
	static uint8_t g_shards[0x78 * 0x34];
	static uint8_t g_pmodels[0x80 * 0x30];
	static uint8_t g_sprites[0x200 * 0x18];
	// Angelo: the drawn pose (every pose change rebuilds his bone matrices at once: a draw renders the pose it finds)
	static Pose g_drawn, g_tmp_next, g_tmp_half;
	static uint8_t g_sk_drawn[SK_MAX];             // the pose block as drawn (its bone matrices: what the draw used)
	static uint32_t g_sk_drawn_n = 0;
	static int32_t g_t_drawn[3];                   // his matrix translation as drawn
	static uint32_t g_gen = 0;                     // animation restarts (AnimStep)
	// held-frame buffers / saves
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_morph[0x10000];          // the layouts' blended vertex frames
	static uint8_t g_save_levels[0x400], g_save_shards[0x78 * 0x34], g_save_pmodels[0x80 * 0x30], g_save_sprites[0x200 * 0x18];
	static uint8_t g_sk_save[SK_MAX], g_sk_next[SK_MAX];

	static uint8_t *Skel(uint8_t *c)
	{
		uint8_t *com = *(uint8_t **)(c + 0x64);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0 || sk[0] > MAX_BONES) return nullptr;
		return sk;
	}
	inline uint32_t SkSize(const uint8_t *sk) { return 16 + 48 * (uint32_t)sk[0]; }
	inline uint8_t *StarLevel(int i) { return (uint8_t *)(TexBase() + 0x7C04 + i * 0x10 + 0xB); }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame = 0;
		g_dir = nullptr;
		g_dir_c = -1;
	}

	static Rec *held_add(RecKind kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = Cursor();
			g_frame = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.fa = false;
		r.a = r.b = r.c = r.d = r.e = r.f = r.g = 0;
		r.ptr = nullptr;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_director(DirectorNode *d)
	{
		g_dir = d;
		g_dir_c = d->counter;
	}

	static void held_note_play(uint8_t *layout, int32_t x, int32_t y, int32_t z, int16_t depth, bool frame_arena)
	{
		Rec *r = held_add(R_PLAY);
		if (!r) return;
		r->ptr = layout;
		r->a = x;
		r->b = y;
		r->c = z;
		r->d = depth;
		r->e = g_dir_c;
		r->fa = frame_arena;
	}

	static void held_note_grow(int32_t g)
	{
		Rec *r = held_add(R_GROW);
		if (!r) return;
		r->a = g;
		r->e = g_dir_c;
	}

	static void held_note_landing(uint32_t lt, const int16_t *p, int32_t g)
	{
		Rec *r = held_add(R_LANDING);
		if (!r) return;
		r->a = (int32_t)lt;
		r->b = g;
		r->e = g_dir_c;
		memcpy(r->p, p, 6);
	}

	static void held_note_stars()
	{
		Rec *r = held_add(R_STARS);
		if (!r) return;
		r->e = g_dir_c;
		for (int i = 0; i < 0x400; i++) g_levels[i] = *StarLevel(i);
		for (int k = 0; k < 8; k++) memcpy(g_star_ang[k], (const void *)(STAR_GROUPS + 0xC + 0x10 * k), 4);
	}

	static void held_note_shards()
	{
		Rec *r = held_add(R_SHARDS);
		if (!r) return;
		r->e = g_dir_c;
		memcpy(g_shards, Shards(), sizeof(g_shards));
	}

	static void held_note_angelo(DirectorNode *d)
	{
		uint8_t *c = d->angelo;
		uint8_t *sk = Skel(c);
		Rec *r = held_add(R_ANGELO);
		if (!r) return;
		r->e = g_dir_c;
		r->ptr = c;
		memcpy(g_t_drawn, c + 0x54, sizeof(g_t_drawn));
		g_sk_drawn_n = sk ? SkSize(sk) : 0;
		if (!sk) return;
		memcpy(g_sk_drawn, sk, g_sk_drawn_n);
		g_drawn.get(sk);
		g_drawn.gen = g_gen;
	}

	static void held_note_restart() { g_gen++; }

	static void held_note_tile(int32_t t, int32_t r_, int32_t g_, int32_t b_, int32_t in, int32_t hold, int32_t out)
	{
		Rec *r = held_add(R_TILE);
		if (!r) return;
		r->a = t;
		r->b = in;
		r->c = hold;
		r->d = out;
		r->e = g_dir_c;
		r->f = (int32_t)((uint32_t)(uint8_t)r_ | ((uint32_t)(uint8_t)g_ << 8) | ((uint32_t)(uint8_t)b_ << 16));
		r->g = g_dir_c - t;        // the series' base counter (0x24 / 0xC5)
	}

	static void held_note_particles(int which)
	{
		Rec *r = held_add(which == 0 ? R_PMODELS : R_SPRITES);
		if (!r) return;
		r->e = g_dir_c;
		if (which == 0) memcpy(g_pmodels, PModels(), sizeof(g_pmodels));
		else memcpy(g_sprites, Sprites(), sizeof(g_sprites));
	}

	static void held_note_flash(SmallNode *t)
	{
		Rec *r = held_add(R_FLASH);
		if (!r) return;
		r->ptr = (uint8_t *)t;
		memcpy(r->node, t, sizeof(r->node));
	}

	static void held_note_debris(SmallNode *t)
	{
		Rec *r = held_add(R_DEBRIS);
		if (!r) return;
		r->ptr = (uint8_t *)t;
		memcpy(r->node, t, sizeof(r->node));
	}

	static void held_note_trail(DiveNode *d)
	{
		Rec *r = held_add(R_TRAIL);
		if (!r) return;
		r->ptr = (uint8_t *)d;
		memcpy(r->node, d, sizeof(r->node));
		r->fa = (uint32_t)d->hdr.func == ORIG_DiveTask;
	}

	static void held_note_dive_play(DiveNode *d, const Mat4x3 *m)
	{
		Rec *r = held_add(R_DIVEPLAY);
		if (!r) return;
		r->ptr = (uint8_t *)d;
		r->m = *m;
	}

	// ------------------------------------------------------------------
	// held-frame pieces
	// ------------------------------------------------------------------
	// the director's counter for the next tick (it ran this tick: incremented, or not during a stream wait)
	static int32_t NextCounter()
	{
		const DirectorNode *d = (const DirectorNode *)QA()->head;
		return d ? d->counter : -1;
	}

	// a prim-model layout still plays on the next tick (its frame, counted by this tick's play, below its frame count)
	static bool LayoutPlaysOn(const uint8_t *layout)
	{
		const prim::Layout *l = (const prim::Layout *)layout;
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// the counter range a director layout plays in (from the counter it played on)
	static bool SamePhase(int32_t c, int32_t cn, const uint8_t *layout)
	{
		const DirectorNode *d = g_dir;
		if (!d || cn < 0) return false;
		if (layout == d->layout2) return (uint32_t)(cn - 0x25) < 0xBD;
		if ((uint32_t)(c - 0x4B) < 0x14) return (uint32_t)(cn - 0x4B) < 0x14;
		if ((uint32_t)(c - 0x20) < 4) return (uint32_t)(cn - 0x20) < 4;
		if ((uint32_t)(c - 0xB4) < 0x2F) return (uint32_t)(cn - 0xB4) < 0x2F;
		return false;
	}

	// Angelo's matrix translation as the next tick's prelude sets it (the rise / fall heights and the turn of Rinoa's entity, which his
	// container follows: director +0x8C / +0x90 = container +0x58 / +0x5C)
	static void NextAngeloT(const DirectorNode *d, int32_t c, int32_t *t)
	{
		const uint8_t *e = Caster();
		int16_t z = *(const int16_t *)(e + 0x20), y = *(const int16_t *)(e + 0x1E);
		bool zs = false, ys = false;
		if ((uint32_t)c < 0x16)
		{
			const uint32_t q = mulhi_u((uint32_t)c << 12, 0xBA2E8BA3) >> 4;
			z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 5000), 12) + 0x1770);
			zs = true;
		}
		if ((uint32_t)(c - 0x1E) <= 3) { y = (int16_t)mul32(c - 0x1E + 1, -275); ys = true; }
		if ((uint32_t)(c - 0x16) < 0x1C)
		{
			const uint32_t q = ((uint32_t)(c - 0x16) << 12) / 28;
			z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 12000), 12) + 0x1770);
			zs = true;
		}
		const uint32_t lt = (uint32_t)(c - 0xA2);
		if (lt < 0xF)
		{
			const int32_t g = (int32_t)(mulhi_u(lt << 12, 0x88888889) >> 3);
			if (lt == 0) { y = d->centre[1]; ys = true; }
			z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2(g * 13500, 12) + 0x3A98);
			zs = true;
		}
		const uint8_t *a = d->angelo;
		t[0] = *(const int32_t *)(a + 0x54);
		t[1] = ys ? y : *(const int32_t *)(a + 0x58);
		t[2] = zs ? z : *(const int32_t *)(a + 0x5C);
	}

	// Angelo's bone 0x13 on the next tick (the bones as this tick left them, his position as the next tick's prelude sets it;
	// 0x1D9770C put back)
	static void NextAnchor(int16_t *out)
	{
		uint8_t *c = g_dir->angelo;
		int32_t t_live[3], t1[3];
		memcpy(t_live, c + 0x54, sizeof(t_live));
		NextAngeloT(g_dir, NextCounter(), t1);
		memcpy(c + 0x54, t1, sizeof(t1));
		const uint16_t w = var<uint16_t>(0x1D9770C);
		GetEffectSpawnPosition(c, 0x13, 0, out);
		var<uint16_t>(0x1D9770C) = w;
		memcpy(c + 0x54, t_live, sizeof(t_live));
	}

	static void PlayHeld(const Rec &r, int num, int den)
	{
		const int32_t cn = NextCounter();
		const bool next = SamePhase(r.e, cn, r.ptr) && LayoutPlaysOn(r.ptr);
		if (!next)
		{
			if (num >= den) return;
			num = 0; // the last draw of the phase (or of the layout) holds
		}
		int32_t x = r.a, y = r.b, z = r.c;
		if (next && g_dir && r.ptr == g_dir->layout && (uint32_t)(r.e - 0x20) < 4)
		{
			// the anchor at Angelo's bone 0x13 moves with his bones
			int16_t p1[4];
			NextAnchor(p1);
			x = lerp_i(x, p1[0], num, den);
			y = lerp_i(y, p1[1], num, den);
			z = lerp_i(z, p1[2], num, den);
		}
		PrimArg a;
		RotY(0, &a.m);
		a.m.t[0] = x;
		a.m.t[1] = y;
		a.m.t[2] = z;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		a.depth = (int16_t)r.d;
		uint32_t &cursor = r.fa ? FrameArena() : Cursor();
		a.cursor = cursor;
		prim::play_held((prim::Layout *)r.ptr, PartCallback, (int)&a, num, den);
		cursor = a.cursor;
	}

	static void DivePlayHeld(const Rec &r, int num, int den)
	{
		const DiveNode *dn = (const DiveNode *)r.ptr;
		if (!(dn->hdr.flags & 1) || !LayoutPlaysOn(dn->layout))
		{
			if (num >= den) return;
			num = 0;
		}
		PrimArg a;
		a.m = r.m;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		a.depth = 0;
		a.cursor = FrameArena();
		prim::play_held((prim::Layout *)dn->layout, PartCallback, (int)&a, num, den);
		FrameArena() = a.cursor;
	}

	static void LandingHeld(const Rec &r, int num, int den)
	{
		const uint32_t lt = (uint32_t)r.a, ln = (uint32_t)(NextCounter() - 0xA2);
		const bool next = ln < 0xF; // (a stream wait at 0xAD draws the same step again)
		if (!next && num >= den) return;
		uint32_t l = lt;
		int32_t angle = (int32_t)((lt & 0xFF) << 8), fade = shl32((int32_t)(lt - 0xB) + 1, 10), g = r.b;
		int16_t p[3] = { r.p[0], r.p[1], r.p[2] };
		if (next)
		{
			// the uv step and the blend switch on the real tick
			if (num >= den) l = ln;
			angle = lerp_i(angle, (int32_t)((ln & 0xFF) << 8), num, den);
			const int32_t fade1 = shl32((int32_t)(ln - 0xB) + 1, 10);
			if (num >= den) fade = fade1;
			else if (lt - 0xB < 4 && ln - 0xB < 4) fade = lerp_i(fade, fade1, num, den);
			g = lerp_i(g, (int32_t)(mulhi_u(ln << 12, 0x88888889) >> 3), num, den);
			int16_t p1[4];
			NextAnchor(p1);
			for (int k = 0; k < 3; k++) p[k] = lerp16(p[k], p1[k], num, den);
		}
		LandingDraw(l, p, angle, fade, g);
	}

	static void GrowHeld(const Rec &r, int num, int den)
	{
		const uint32_t sn = (uint32_t)(NextCounter() - 0x57);
		const bool next = sn < 0xF; // (a stream wait at 0x65 draws the same step again)
		if (!next && num >= den) return;
		int32_t g = r.a;
		if (next) g = lerp_i(g, (int32_t)(mulhi_u(sn << 12, 0x88888889) >> 3), num, den);
		GrowDraw(g_dir, g);
	}

	static void TileHeld(const Rec &r, int num, int den)
	{
		int32_t level = FadeLevel(r.a, r.b, r.c, r.d);
		const int32_t tn = NextCounter() - r.g;
		const int32_t last = r.g == 0x24 ? 8 : 0x3E;
		const bool next = tn >= 0 && tn < last;
		if (!next && num >= den) return;
		if (next && tn != r.a) level = lerp_i(level, FadeLevel(tn, r.b, r.c, r.d), num, den);
		FrameArena() = Tile(r.f & 0xFF, (r.f >> 8) & 0xFF, (r.f >> 16) & 0xFF, level, FrameArena());
	}

	static void StarsHeld(const Rec &r, int num, int den)
	{
		const bool next = (uint32_t)(NextCounter() - 0x25) < 0xBD;
		if (!next && num >= den) return;
		for (int i = 0; i < 0x400; i++)
		{
			uint8_t *l = StarLevel(i);
			g_save_levels[i] = *l;
			// level 0 draws the plain colour (no blend): a step to or from it is discrete (switches on the real tick)
			if (!next) *l = g_levels[i];
			else if (g_levels[i] == 0 || *l == 0) *l = num < den ? g_levels[i] : *l;
			else *l = (uint8_t)lerp_i(g_levels[i], *l, num, den);
		}
		for (int k = 0; k < 8; k++)
		{
			int16_t *a = (int16_t *)(STAR_GROUPS + 0xC + 0x10 * k);
			memcpy(g_save_ang[k], a, 4);
			for (int q = 0; q < 2; q++) a[q] = next ? lerp16(g_star_ang[k][q], a[q], num, den) : g_star_ang[k][q];
		}
		FrameArena() = Stars(0, Centre()[0], 0, Centre()[2], 0x10000, FrameArena());
		for (int i = 0; i < 0x400; i++) *StarLevel(i) = g_save_levels[i];
		for (int k = 0; k < 8; k++) memcpy((void *)(STAR_GROUPS + 0xC + 0x10 * k), g_save_ang[k], 4);
	}

	static void ShardsHeld(const Rec &r, int num, int den)
	{
		const bool next = (uint32_t)(NextCounter() - 0x24) < 0x27;
		if (!next && num >= den) return;
		if (next && num >= den)
		{
			ShardsDraw(); // (the table as the tick left it)
			return;
		}
		uint8_t *live = (uint8_t *)Shards();
		memcpy(g_save_shards, live, sizeof(g_save_shards));
		memcpy(live, g_shards, sizeof(g_shards));
		if (next)
			for (int i = 0; i < 0x78; i++)
			{
				uint8_t *s = live + i * 0x34;
				const uint8_t *n = g_save_shards + i * 0x34;
				if (F<int16_t>(s, 6) == 0 || *(const int16_t *)(n + 6) == 0) continue; // (gone: its last draw holds)
				for (uint32_t o = 0; o <= 4; o += 2) F<int16_t>(s, o) = lerp16(F<int16_t>(s, o), *(const int16_t *)(n + o), num, den);
				for (uint32_t o = 0x14; o <= 0x24; o += 2) F<int16_t>(s, o) = lerp16(F<int16_t>(s, o), *(const int16_t *)(n + o), num, den);
			}
		ShardsDraw();
		memcpy(live, g_save_shards, sizeof(g_save_shards));
	}

	static void AngeloHeld(const Rec &r, int num, int den)
	{
		uint8_t *c = r.ptr;
		uint8_t *sk = Skel(c);
		if (!sk || !g_sk_drawn_n || SkSize(sk) != g_sk_drawn_n) return;
		const uint32_t skn = SkSize(sk);
		// drawn on the next tick?
		const int32_t cn = NextCounter();
		const bool draws = cn >= 0 && cn < 0x10F && !(c[0] & 4) && cn != 0xB1; // (0xB1 hides him before his draw)
		if (!draws && num >= den) return;
		memcpy(g_sk_save, sk, skn);
		// the next draw's pose: the live one (this tick's AnimStep read it), or the first pose of the animation the next tick's
		// SetAnim starts (run here on saved state); an animation change or restart is a switch (holds below 1)
		const bool setanim = cn == 7 || cn == 0x1E || cn == 0x20;
		if (setanim)
		{
			static uint8_t cont[0x9C];
			memcpy(cont, c, sizeof(cont));
			SetAnim(c, cn == 7 ? 1 : cn == 0x1E ? 2 : 3);
			g_tmp_next.get(sk);
			g_tmp_next.gen = g_gen + 1;
			memcpy(g_sk_next, sk, skn);
			memcpy(c, cont, sizeof(cont));
			memcpy(sk, g_sk_save, skn);
		}
		else
		{
			g_tmp_next.get(sk);
			g_tmp_next.gen = g_gen;
			memcpy(g_sk_next, sk, skn);
		}
		const Pose *src = nullptr;
		if (draws && num >= den) memcpy(sk, g_sk_next, skn); // (the pose block the next tick renders)
		else if (draws && g_tmp_next.gen == g_drawn.gen && g_tmp_next.nb == g_drawn.nb && g_tmp_next.scaled == g_drawn.scaled && num > 0) // (a frame turning the bone scales on / off: a switch)
		{
			pose_lerp(g_drawn, g_tmp_next, num, den, g_tmp_half);
			src = &g_tmp_half;
		}
		if (src)
		{
			src->set(sk);
			BuildBoneMatricesFromPose(c + 0x60);
			memcpy(sk + 8, g_sk_save + 8, 6); // (the renderer rebuilds the matrices from the pose after its render: the real pose)
			for (int b = 0; b < (int)sk[0]; b++) memcpy(sk + 16 + 48 * b + 4, g_sk_save + 16 + 48 * b + 4, 12);
		}
		else if (!(draws && num >= den)) memcpy(sk, g_sk_drawn, skn); // (the drawn bone matrices: holds)
		// his position half way to the one the next tick's prelude sets
		int32_t t_live[3], t1[3];
		memcpy(t_live, c + 0x54, sizeof(t_live));
		int32_t *t = (int32_t *)(c + 0x54);
		if (draws)
		{
			NextAngeloT(g_dir, cn, t1);
			for (int k = 0; k < 3; k++) t[k] = lerp_i(g_t_drawn[k], t1[k], num, den);
		}
		else memcpy(t, g_t_drawn, sizeof(g_t_drawn));
		Cursor() = DrawCreatureModel(c, (uint32_t)g_held_morph, Cursor(), CursorEnd());
		memcpy(t, t_live, sizeof(t_live));
		memcpy(sk, g_sk_save, skn);
	}

	// particle models: the table as drawn, each one the update kept moved / scaled / faded half way (at 1: the table the tick left)
	static void PModelsHeld(int num, int den)
	{
		const int32_t cn0 = NextCounter();
		const bool next = cn0 >= 0 && cn0 != 0xA2; // (no next tick after the director)
		if (!next && num >= den) return;
		if (num >= den)
		{
			PModelsDraw();
			return;
		}
		uint8_t *live = (uint8_t *)PModels();
		memcpy(g_save_pmodels, live, sizeof(g_save_pmodels));
		memcpy(live, g_pmodels, sizeof(g_pmodels));
		if (next)
			for (int i = 0; i < 0x80; i++)
			{
				PModel *p = (PModel *)(live + i * 0x30);
				const PModel *n = (const PModel *)(g_save_pmodels + i * 0x30);
				if (p->life == 0 || n->life != (uint8_t)(p->life - 1) || n->kind != p->kind) continue; // (freed / reborn: holds)
				p->fade = lerp16(p->fade, n->fade, num, den);
				for (int k = 0; k < 3; k++) p->scale[k] = lerp16(p->scale[k], n->scale[k], num, den);
				for (int k = 0; k < 3; k++) p->pos[k] = lerp_i(p->pos[k], n->pos[k], num, den);
			}
		PModelsDraw();
		memcpy(live, g_save_pmodels, sizeof(g_save_pmodels));
	}

	// sparkles: the table as drawn (flipbook frame by age: holds), each one the update kept moved half way (at 1: the table the
	// tick left)
	static void SpritesHeld(int num, int den)
	{
		const int32_t cn = NextCounter();
		const bool next = cn >= 0 && cn != 0xA2 && cn != 0xB1 && cn != 0xEB;
		if (!next && num >= den) return;
		if (num >= den)
		{
			SpritesDraw();
			return;
		}
		uint8_t *live = (uint8_t *)Sprites();
		memcpy(g_save_sprites, live, sizeof(g_save_sprites));
		memcpy(live, g_sprites, sizeof(g_sprites));
		if (next)
			for (int i = 0; i < 0x200; i++)
			{
				Sprite *p = (Sprite *)(live + i * 0x18);
				const Sprite *n = (const Sprite *)(g_save_sprites + i * 0x18);
				if (p->life == 0 || n->life != (int16_t)(p->life - 1) || n->seq != p->seq || n->life == 0) continue;
				for (int k = 0; k < 3; k++) p->pos[k] = lerp16(p->pos[k], n->pos[k], num, den);
			}
		SpritesDraw();
		memcpy(live, g_save_sprites, sizeof(g_save_sprites));
	}

	// flash: its flipbook frame holds (at 1: the next frame, nothing after its last)
	static void FlashHeld(const Rec &r, int num, int den)
	{
		const SmallNode *a = (const SmallNode *)r.node, *n = (const SmallNode *)r.ptr;
		if (num >= den)
		{
			// (the director resets queue B on 0x16)
			if ((n->hdr.flags & 1) && n->hdr.func == a->hdr.func && n->frame == (int8_t)(a->frame + 1) && NextCounter() != 0x16) FlashDraw(n);
			return;
		}
		FlashDraw(a);
	}

	static void DebrisHeld(const Rec &r, int num, int den)
	{
		const SmallNode *a = (const SmallNode *)r.node, *n = (const SmallNode *)r.ptr;
		const bool next = (n->hdr.flags & 1) && n->hdr.func == a->hdr.func && n->frame == (int8_t)(a->frame - 1) && NextCounter() != 0x16; // (queue B reset on 0x16)
		if (!next && num >= den) return;
		int16_t pos[3] = { a->pos[0], a->pos[1], a->pos[2] };
		int16_t ax = (int16_t)((int16_t)a->ang[0] << 4), ay = (int16_t)((int16_t)a->ang[1] << 4);
		// (the task ended this tick: its last draw holds)
		if (next)
		{
			for (int k = 0; k < 3; k++) pos[k] = lerp16(pos[k], n->pos[k], num, den);
			const int16_t bx = (int16_t)((int16_t)n->ang[0] << 4), by = (int16_t)((int16_t)n->ang[1] << 4);
			ax = (int16_t)(ax + (int16_t)((((int32_t)bx - ax + 0x800) & 0xFFF) - 0x800) * num / den);
			ay = (int16_t)(ay + (int16_t)((((int32_t)by - ay + 0x800) & 0xFFF) - 0x800) * num / den);
			if (num >= den) { ax = bx; ay = by; }
		}
		DebrisDraw(pos, ax, ay);
	}

	static void TrailHeld(const Rec &r, int num, int den)
	{
		const DiveNode *a = (const DiveNode *)r.node, *n = (const DiveNode *)r.ptr;
		const bool draws = (n->hdr.flags & 1) && n->hdr.func == a->hdr.func && n->counter == a->counter + 1 && n->counter < 0xC;
		if (!draws && num >= den) return;
		Mat4x3 m = a->m;
		if (draws && num >= den) m = n->m;
		// the next tick draws it at the position this tick moved it to (the last dive's turn on its tick 3: holds)
		else if (draws && !memcmp(n->m.m, a->m.m, sizeof(a->m.m)))
			for (int k = 0; k < 3; k++) m.t[k] = lerp_i(a->m.t[k], n->m.t[k], num, den);
		TrailDraw(&m, r.fa ? &FrameArena() : &Cursor());
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(Cursor(), g_held_arena, ARENA, g_packets);
		// written by the draws (geometry / shadow words, the shadow-rotation scratch, the camera block's work words)
		static uint8_t shadow[0x80], cam[0xB0];
		const uint32_t geom[2] = { var<uint32_t>(0x1D99C18), var<uint32_t>(0x1D99C1C) };
		const uint32_t shadow_words[2] = { var<uint32_t>(0xB8B9E4), var<uint32_t>(0xB8B9EC) };
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		const uint32_t frame = FrameArena();
		if (g_frame && frame > g_frame)
		{
			const uint32_t used = frame - g_frame;
			memcpy(g_held_frame, (const void *)g_frame, used < FRAME ? used : FRAME);
		}
		FrameArena() = (uint32_t)g_held_frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case R_PLAY: PlayHeld(r, num, den); break;
			case R_GROW: GrowHeld(r, num, den); break;
			case R_LANDING: LandingHeld(r, num, den); break;
			case R_STARS: StarsHeld(r, num, den); break;
			case R_SHARDS: ShardsHeld(r, num, den); break;
			case R_ANGELO: AngeloHeld(r, num, den); break;
			case R_TILE: TileHeld(r, num, den); break;
			case R_PMODELS: PModelsHeld(num, den); break;
			case R_SPRITES: SpritesHeld(num, den); break;
			case R_FLASH: FlashHeld(r, num, den); break;
			case R_DEBRIS: DebrisHeld(r, num, den); break;
			case R_TRAIL: TrailHeld(r, num, den); break;
			case R_DIVEPLAY: DivePlayHeld(r, num, den); break;
			}
		}
		FrameArena() = frame;
		var<uint32_t>(0x1D99C18) = geom[0];
		var<uint32_t>(0x1D99C1C) = geom[1];
		var<uint32_t>(0xB8B9E4) = shadow_words[0];
		var<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)0x1D97700, cam, sizeof(cam));
	}

	// ------------------------------------------------------------------
	// held-frame camera
	// ------------------------------------------------------------------
	// counters on which the director writes the camera words
	static bool CameraWritten(int32_t c)
	{
		return (uint32_t)c < 0x23 || (uint32_t)(c - 0x31) < 0x19 || (uint32_t)(c - 0x5F) < 0x42 || c == 0xA1 || (uint32_t)(c - 0xB0) < 0x32 || c == 0xE3;
	}

	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

	// Rinoa's entity position as the director sets it on counter c (before its camera)
	static void RinoaNext(const DirectorNode *d, int32_t c)
	{
		uint8_t *e = Caster();
		if ((uint32_t)c < 0x16)
		{
			const uint32_t q = mulhi_u((uint32_t)c << 12, 0xBA2E8BA3) >> 4;
			*(int16_t *)(e + 0x20) = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 5000), 12) + 0x1770);
		}
		if ((uint32_t)(c - 0x1E) <= 3) *(int16_t *)(e + 0x1E) = (int16_t)mul32(c - 0x1E + 1, -275);
		if ((uint32_t)(c - 0x16) < 0x1C)
		{
			const uint32_t q = ((uint32_t)(c - 0x16) << 12) / 28;
			*(int16_t *)(e + 0x20) = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 12000), 12) + 0x1770);
		}
	}

	// ------------------------------------------------------------------
	// held-frame entity (fx_held.h HeldEntity, drawn by the frame gate): Rinoa
	// ------------------------------------------------------------------
	// The director writes her +0x1E (y) / +0x20 (z) as functions of its counter (the rise c < 0x32, the turn 0x1E..0x21,
	// the landing 0xA2..0xB0), each with the render translation +0x58 / +0x5C (the "direct" components 10 / 11: the real
	// frame draws them; kept in the director +0x8C / +0x90). Discrete changes of the tick just run (its director counter
	// c0): c0 == 0 (DirectorInit puts her at the centre, +0x0E = 0), 0x16 (the rise restarts its curve: z), 0xA2 (the
	// landing's placement: y, z), 0xE3 (put back: x, y, z), the counter reaching 0x10F (angles put back). Direct
	// translation: lerp(this tick's, the next tick's) when both write it (a restart / placement of the next tick holds),
	// held when only this tick writes it.
	static bool RinoaZ(const DirectorNode *d, int32_t c, int16_t *z)
	{
		bool w = false;
		if ((uint32_t)c < 0x16)
		{
			const uint32_t q = mulhi_u((uint32_t)c << 12, 0xBA2E8BA3) >> 4;
			*z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 5000), 12) + 0x1770);
			w = true;
		}
		if ((uint32_t)(c - 0x16) < 0x1C)
		{
			const uint32_t q = ((uint32_t)(c - 0x16) << 12) / 28;
			*z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2((int32_t)(q * 12000), 12) + 0x1770);
			w = true;
		}
		const uint32_t lt = (uint32_t)(c - 0xA2);
		if (lt < 0xF)
		{
			const int32_t g = (int32_t)(mulhi_u(lt << 12, 0x88888889) >> 3);
			*z = (int16_t)(d->centre[2] - (int16_t)sdiv_pow2(g * 13500, 12) + 0x3A98);
			w = true;
		}
		return w;
	}
	static bool RinoaY(const DirectorNode *d, int32_t c, int16_t *y)
	{
		bool w = false;
		if ((uint32_t)(c - 0x1E) <= 3) { *y = (int16_t)mul32(c - 0x1E + 1, -275); w = true; }
		if (c == 0xA2) { *y = d->centre[1]; w = true; }
		return w;
	}
	// a direct translation component: tick N wrote from (w0), tick N + 1 writes v (w1; cut = a discrete start)
	static void RinoaDirect(HeldEntity &h, int c, bool w0, int32_t from, bool w1, int32_t v, bool cut)
	{
		if (w0 && w1 && !cut) held_entity_direct(h, c, from, v);
		else if (w0) held_entity_direct_hold(h, c, from);
	}

	static int HeldEntities(HeldEntity *out, int max)
	{
		if (max < 1 || g_ported_tick != g_real_tick || !Caster()) return 0;
		HeldEntity &h = out[0];
		held_entity_init(h, Caster());
		const DirectorNode *d = g_dir;                     // the director that ran this tick (it may have ended)
		if (!d || g_dir_c < 0) return 1;
		const int32_t c0 = g_dir_c;                        // its counter on this tick
		if (c0 == 0) { held_entity_jump(h, 0); held_entity_jump(h, 1); held_entity_jump(h, 2); held_entity_jump(h, 4); }
		if (c0 == 0x16) held_entity_jump(h, 2);
		if (c0 == 0xA2) { held_entity_jump(h, 1); held_entity_jump(h, 2); }
		if (c0 == 0xE3) { held_entity_jump(h, 0); held_entity_jump(h, 1); held_entity_jump(h, 2); }
		if (c0 + 1 >= 0x10F) { held_entity_jump(h, 3); held_entity_jump(h, 4); held_entity_jump(h, 5); }
		const DirectorNode *dn = (const DirectorNode *)QA()->head;
		const int32_t c = dn == d && (d->hdr.flags & 1) ? d->counter : -1;   // the next tick's counter (-1: it ended)
		int16_t y0 = 0, z0 = 0, y1 = 0, z1 = 0;
		const bool wy0 = RinoaY(d, c0, &y0), wz0 = RinoaZ(d, c0, &z0);
		const bool wy1 = c >= 0 && RinoaY(d, c, &y1), wz1 = c >= 0 && RinoaZ(d, c, &z1);
		RinoaDirect(h, 10, wy0, *(const int32_t *)((const uint8_t *)d + 0x8C), wy1, y1, c == 0xA2);
		RinoaDirect(h, 11, wz0, *(const int32_t *)((const uint8_t *)d + 0x90), wz1, z1, c == 0x16 || c == 0xA2);
		return 1;
	}

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_ported_tick != g_real_tick) return false;
		const DirectorNode *d = (const DirectorNode *)QA()->head;
		if (!d) return false;
		const int32_t c = d->counter;
		if (c == 0 || !CameraWritten(c)) return false;
		if (c == 0x24 || c == 0x65 || c == 0xAD)
		{
			// a stream wait (0x534270 not idle) holds the tick: nothing moves
			const uint8_t *st = var<const uint8_t *>(0x1DCD6EC);
			if (!(st[3] == st[4] && st[0] == 0 && st[2] != 0 && st[1] == 0)) return false;
		}
		int16_t *eye = (int16_t *)0xB8B7F0, *at = (int16_t *)0xB8B7F8;
		int16_t e0[3], a0[3];
		for (int k = 0; k < 3; k++) { e0[k] = eye[k]; a0[k] = at[k]; }
		bool cut = c == 0xA1 || c == 0xE3;
		int16_t e1[3], a1[3];
		if (!cut)
		{
			static uint8_t cam_save[0x10], ent_save[8];
			static uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
			memcpy(cam_save, (const void *)0xB8B7F0, sizeof(cam_save));
			memcpy(ent_save, Caster() + 0x1C, sizeof(ent_save));
			const int16_t zoom = var<int16_t>(0x1D977A2);
			const int16_t w038 = var<int16_t>(0x1D8E038);
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			RinoaNext(d, c);
			DirectorCamera(c);
			for (int k = 0; k < 3; k++) { e1[k] = eye[k]; a1[k] = at[k]; }
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			var<int16_t>(0x1D977A2) = zoom;
			var<int16_t>(0x1D8E038) = w038;
			memcpy(Caster() + 0x1C, ent_save, sizeof(ent_save));
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			// a keyed segment that starts away from the current camera is a cut too
			for (int k = 0; k < 3; k++)
				if (AbsI(e1[k] - e0[k]) > 1500 || AbsI(a1[k] - a0[k]) > 1500) cut = true;
		}
		for (int k = 0; k < 3; k++)
		{
			world[k] = cut ? e0[k] : lerp16(e0[k], e1[k], num, den);
			lookat[k] = cut ? a0[k] : lerp16(a0[k], a1[k], num, den);
		}
		return true;
	}
}

	static void register_mag089_held()
	{
		mark_held(ws089::ORIG_Director);
		mark_held(ws089::ORIG_FlashTask);
		mark_held(ws089::ORIG_DebrisTask);
		mark_held(ws089::ORIG_DiveTask);
		mark_held(ws089::ORIG_LastDiveTask);
		register_module_held(89, ws089::HeldReady, ws089::HeldFrame);
		register_module_camera(89, ws089::HeldCamera);
		register_module_entity(89, ws089::HeldEntities);
	}
}
