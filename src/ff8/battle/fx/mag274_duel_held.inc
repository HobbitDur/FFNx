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

// 30 fps part of mag274_duel.cpp (Zell's Duel): the draw log of the real tick and the held-frame drawing (included at the
// end of mag274_duel.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// A real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the
// Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the same order, with the vanilla draw
// code, each at its in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every
// draw composes with it, as the real draws do with theirs):
//   Layouts   - the prim-model layouts through prim::play_held (the exact in-between records of what the player drew). The
//               frame each layout is drawn in (built by the move without the view): Punch Rush's two at Zell's fist and
//               Meteor Strike's wave at his hip follow his bones: their next tick's frame is rebuilt at his NEXT pose
//               (pose_midpoint at 1: the look-ahead of the animation reader; then his real pose is rebuilt), the held frame
//               num / den of the way (translation, and Punch Rush's bone-turned rotation element by element); the other
//               frames are fixed points. Heel Drop / Dolphin Blow's orthonormal view copy (0x2508440) is rebuilt from the
//               live view. Dolphin Blow's spout scrolls its texture 8 a tick: num / den of the way, the real next scroll at 1.
//               Blended vertex frames go to a private buffer.
//   Ghosts    - (draw then record) the afterimages of Zell's model: each ghost's recorded bone matrices num / den of the way
//               to the same ghost's matrices on the next tick (the block after the tick: its newest record is this tick's
//               pose), on a private copy of the block (the draw writes its view-composed matrices into it); a ghost the next
//               tick drops holds; at 1 the next tick's ghosts.
//   Bars      - the Duel pause's letterbox: the level num / den of the way to the next tick's (rising to 0x40 while no
//               move is posted; falling 0x10 a tick once one is; gone at 1 when the fall ends).
//   Wave      - Mach Kick's bent wave: its vertices num / den of the way from the ones the tick drew to the ones the bend
//               left (the next tick's), its foot point (the frame's translation and direction) num / den of the way to the
//               next tick's (his foot at the NEXT pose while it still follows it), its fade likewise; the draw mode (opaque /
//               fading) switches on the real frame (at 1 the next tick's); the exe model's vertices are put back after the draw.
//   Pillar    - Meteor Strike's rising wave (0x10E7E18): its turn (0x80 a tick), his hip (at his NEXT pose) and its fade
//               num / den of the way to the next tick's.
//   Target    - Meteor Strike's module-drawn target: its tilt, height offset and squash num / den of the way to the next
//               tick's (the move's own formulas; the height measured at its NEXT pose and, while the throw still moves it, at the throw
//               path's next point), its pose half way (pose_midpoint at
//               num / den; its real pose is rebuilt by the draw itself), through its live matrix (the frame gate's in-between
//               entity).
//   Sparks / hit flashes / rocks - (draw then update) the node num / den of the way to its update (sparks' and rocks'
//               positions, rocks' angles); flipbook frames switch on the real frame; a node that ends on the next tick is
//               not drawn at 1.
// A part the next tick will not draw (the move tick not run: the move ended, the next one not started) is held up to 1 and
// not drawn at 1; a move posted before the next tick (the player's input, unknown on the held frame) is not predicted.
// Zell's / the target's positions and flags (teleports, bounces, the throw), the screen flash and the camera animations are
// the engine's. Nothing here writes game state: the module globals, both packet cursors (private buffers holding the real
// tick's packets under the replay), the Field_Alloc scratch, the shadow-rotation scratch, the screen vertex buffer
// (0x1D98B3C), the word 0x1D9770C, the GTE registers and the look-ahead bone matrices are put back.

namespace ff8fx
{
namespace duel274
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x30000;   // module packet arena per tick (0x14000 for Dolphin Blow)
	static const uint32_t FARENA = 0x30000;  // frame arena (ghosts, the module-drawn target)
	static const uint32_t BSS_LO = 0x2508410, BSS_HI = 0x25084E8;
	enum : uint8_t { REC_LAYOUT = 1, REC_GHOSTS, REC_BARS, REC_WAVE, REC_ENTITY, REC_SPARK, REC_FLASH, REC_ROCK, REC_PILLAR };

	struct Rec
	{
		uint8_t kind;
		int16_t counter;      // the move's counter when the record was made (move parts)
		int site;             // layout site
		uint32_t layout;
		PlayArg arg;          // layout: arg.m = the frame without the view
		uint8_t node[0x2C];   // particles: the node as drawn
		uint8_t level;        // bars
		bool fading;
		uint32_t v;           // wave: counter - 0x1D
		Mat4x3 b;             // wave: the frame without the view
		int32_t angle;        // pillar: its turn
		int16_t hip[4];       // pillar: his hip
		uint8_t *ent;         // target: the entity drawn
		int16_t ang[3], off[3];
		int32_t scale[3];
		int extra;            // ghosts: index in g_ghosts; wave: index in g_verts
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static bool g_in_held = false;
	static int g_nrec = 0, g_nscratch = 0, g_nghosts = 0, g_nverts = 0;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static Rec g_recs[0x200];
	static uint8_t g_scratch[0x100][SCRATCH];
	static uint8_t g_ghosts[4][0x488];
	static uint8_t g_verts[2][0x268];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_morph[0x8000];
	static uint8_t g_held_ghost[0x488];
	// the next-pose predictions of the real tick (made at its first held frame)
	static bool g_next_ok[16];
	static Mat4x3 g_next_rel[16];
	static bool g_next_foot_ok = false, g_next_hip_ok = false;
	static int16_t g_next_foot[4], g_next_hip[4];
	static int32_t g_pillar_angle = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		if (g_in_held) return;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
		g_nghosts = 0;
		g_nverts = 0;
		g_packets = 0;
		g_frame_start = 0;
	}

	static Rec *NewRec(uint8_t kind)
	{
		if (g_in_held || g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.counter = 0;
		const TaskNode *mn = QMoves()->head;
		if (mn) r.counter = ((const MoveNode *)mn)->counter;
		r.extra = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r.scratch = g_nscratch++;
		memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_layout(MoveNode *m, int site, uint32_t layout, const PlayArg *arg)
	{
		(void)m;
		Rec *r = NewRec(REC_LAYOUT);
		if (!r) return;
		r->site = site;
		r->layout = layout;
		r->arg = *arg;
	}

	static void held_note_ghosts(uint8_t *entity, const uint8_t *g)
	{
		(void)entity;
		if (g_nghosts >= (int)(sizeof(g_ghosts) / sizeof(g_ghosts[0]))) return;
		Rec *r = NewRec(REC_GHOSTS);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->extra = g_nghosts++;
		memcpy(g_ghosts[r->extra], g, 0x488);
	}

	static void held_note_bars(uint8_t level, bool fading)
	{
		Rec *r = NewRec(REC_BARS);
		if (!r) return;
		r->level = level;
		r->fading = fading;
	}

	static void held_note_wave(MoveNode *m, uint32_t v, const Mat4x3 *b)
	{
		(void)m;
		if (g_nverts >= (int)(sizeof(g_verts) / sizeof(g_verts[0]))) return;
		Rec *r = NewRec(REC_WAVE);
		if (!r) return;
		r->v = v;
		r->b = *b;
		r->extra = g_nverts++;
		memcpy(g_verts[r->extra], (const void *)(MODEL_Kick + 8), 0x268);
	}

	static void held_pillar_angle(int32_t angle) { g_pillar_angle = angle; }
	static void held_note_pillar(const Mat4x3 *w, const int16_t *pos, int16_t counter)
	{
		(void)w;
		Rec *r = NewRec(REC_PILLAR);
		if (!r) return;
		r->angle = g_pillar_angle;
		memcpy(r->hip, pos, 8);
		r->counter = counter;
	}

	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off, const int32_t *scale)
	{
		Rec *r = NewRec(REC_ENTITY);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ent = e;
		memcpy(r->ang, angles, 6);
		memcpy(r->off, off, 6);
		memcpy(r->scale, scale, 12);
	}

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK);
		if (r) memcpy(r->node, s, sizeof(SparkNode));
	}
	static void held_note_flash(const FlashNode *f)
	{
		Rec *r = NewRec(REC_FLASH);
		if (r) memcpy(r->node, f, sizeof(FlashNode));
	}
	static void held_note_rock(const RockNode *k)
	{
		Rec *r = NewRec(REC_ROCK);
		if (r) memcpy(r->node, k, sizeof(RockNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the next tick (from the state the real tick left)
	static const MoveNode *Move() { const TaskNode *n = QMoves()->head; return n ? (const MoveNode *)n : nullptr; }
	// the move tick runs on the next tick (the counter it runs with); false: the move ended (or none runs)
	static bool NextMoveTick(int16_t *counter)
	{
		const MoveNode *m = Move();
		if (!m || m->wait != 0 || m->mode < 2 || m->mode > 7 || NewMove()) return false;
		if (m->counter > (int16_t)m->length) return false;
		*counter = m->counter;
		return true;
	}
	static bool SiteActive(int site, int16_t c)
	{
		switch (site)
		{
		case SITE_PUNCH_B: return (uint32_t)(c - 0x1B) < 0xF;
		case SITE_PUNCH_A: return (uint32_t)(c - 0x21) < 0xE;
		case SITE_BOOYA: return c >= 0x1F && c < 0x2D;
		case SITE_HEEL_A: case SITE_HEEL_B: case SITE_KICK: return c >= 0x20 && c < 0x2E;
		case SITE_DOLPHIN_A: return c < 0x10;
		case SITE_DOLPHIN_B: return (uint32_t)(c - 0x14) < 0x32;
		case SITE_METEOR_A: return c >= 0x2C && c < 0x3E;
		case SITE_METEOR_B: return (uint32_t)(c - 0x3C) < 0x10;
		}
		return false;
	}
	static bool BoneSite(int site) { return site == SITE_PUNCH_B || site == SITE_PUNCH_A || site == SITE_METEOR_A; }

	// Zell at his next pose (pose_midpoint at 1): the bone-anchored layout frames and Mach Kick's foot point of the next tick;
	// his bone matrices, 0x1D9770C, the Field_Alloc scratch and the GTE registers put back. Once per real tick.
	static void PredictNextPose()
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		memset(g_next_ok, 0, sizeof(g_next_ok));
		g_next_foot_ok = false;
		g_next_hip_ok = false;
		int16_t nc = 0;
		if (!NextMoveTick(&nc)) return;
		bool sites[16] = {}, foot = false, hip = false, any = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_LAYOUT && BoneSite(r.site) && SiteActive(r.site, nc)) { sites[r.site] = true; any = true; }
			if (r.kind == REC_WAVE && r.v + 1 < 4) { foot = true; any = true; }
			if (r.kind == REC_PILLAR && nc >= 0x2C && nc < 0x35) { hip = true; any = true; }
		}
		uint8_t *e = Caster();
		if (!any || !e || !(e[0] & 2) || !*(uint8_t **)(e + 0x64)) return;
		uint8_t *block = **(uint8_t ***)(e + 0x64);
		static uint8_t bones[0x20 + 0x100 * 0x30], fas[0x8000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const uint32_t nb = 0x20 + (uint32_t)block[0] * 0x30;
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		memcpy(bones, block, nb);
		if (fa) memcpy(fas, (const void *)fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
		for (int s = 0; s < 16; s++)
		{
			if (!sites[s]) continue;
			int16_t pos[4];
			BoneFrame(s, &g_next_rel[s], pos);
			g_next_ok[s] = true;
		}
		if (foot)
		{
			GetEffectSpawnPosition(e, 0x14, 0, g_next_foot);
			g_next_foot_ok = true;
		}
		if (hip)
		{
			GetEffectSpawnPosition(e, 0x13, 0, g_next_hip);
			g_next_hip_ok = true;
		}
		memcpy(block, bones, nb);
		var<uint16_t>(0x1D9770C) = w70c;
		var<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static void LerpMat(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out->m[i][j] = lerp16(a.m[i][j], b.m[i][j], num, den);
		for (int k = 0; k < 3; k++) out->t[k] = lerp_i(a.t[k], b.t[k], num, den);
	}

	// ---- layouts
	static void HeldLayout(const Rec &r, int num, int den, bool at_next, bool next_tick, int16_t nc)
	{
		const bool next_draws = next_tick && SiteActive(r.site, nc);
		if (at_next && !next_draws) return;
		PlayArg arg = r.arg;
		if (next_draws && BoneSite(r.site) && g_next_ok[r.site]) LerpMat(&arg.m, r.arg.m, g_next_rel[r.site], num, den);
		if (r.site == SITE_DOLPHIN_B && next_draws)
			arg.scroll = at_next ? (int16_t)((((uint32_t)nc - 0x14) & 0xF) << 3) : (int16_t)(r.arg.scroll + 8 * num / den);
		arg.morph = (uint32_t)g_held_morph;
		if (r.site == SITE_METEOR_A) ViewAt(&arg.m);
		else ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		prim::play_held((prim::Layout *)r.layout, PartCallback, (int)&arg, num, den);
	}

	// ---- ghosts: ghost i of a block = slot ((block[3] - i) & 1)
	static const uint8_t *GhostSlot(const uint8_t *g, int i) { return g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x200; }
	static void HeldGhosts(const Rec &r, int num, int den, bool at_next, bool next_tick)
	{
		const uint8_t *now = g_ghosts[r.extra];
		const MoveNode *m = Move();
		const uint8_t *nx = (next_tick && m && m->ghosts) ? (const uint8_t *)m->ghosts : nullptr;
		if (at_next)
		{
			if (!nx) return;
			memcpy(g_held_ghost, nx, 0x488);
		}
		else
		{
			memcpy(g_held_ghost, now, 0x488);
			for (int i = 0; i < now[1]; i++)
			{
				if (!nx || i >= nx[1]) continue; // (a ghost the next tick drops: held)
				uint8_t *dst = (uint8_t *)GhostSlot(g_held_ghost, i);
				const uint8_t *a = GhostSlot(now, i), *b = GhostSlot(nx, i);
				for (int k = 0; k < now[4]; k++) LerpMat((Mat4x3 *)(dst + k * 0x20), *(const Mat4x3 *)(a + k * 0x20), *(const Mat4x3 *)(b + k * 0x20), num, den);
			}
		}
		GhostDraw(Caster(), g_held_ghost);
	}

	// ---- bars
	static void HeldBars(const Rec &r, int num, int den, bool at_next)
	{
		int32_t next;
		const MoveNode *m = Move();
		if (r.fading || NewMove())
		{
			const uint8_t l = (uint8_t)(r.level - 0x10);
			if (l > 0x80)
			{
				if (at_next) return;
				next = r.level; // (the bars end on the next tick: held)
			}
			else next = l;
		}
		else next = r.level + 0x10 > 0x40 ? 0x40 : r.level + 0x10;
		if (!m || m->wait != 4) next = r.level;
		DrawBars(lerp_i(r.level, next, num, den));
	}

	// ---- Mach Kick's wave
	static void HeldWave(const Rec &r, int num, int den, bool at_next, bool next_tick, int16_t nc)
	{
		const uint32_t v1 = (uint32_t)(nc - 0x1D);
		const bool next_draws = next_tick && v1 < 8;
		if (at_next && !next_draws) return;
		const MoveNode *m = Move();
		const int16_t *from = m ? (const int16_t *)(m->w60 + 4) : nullptr;
		const int16_t *to = m ? (const int16_t *)(m->w60 + 0xC) : nullptr;
		// the next tick's foot point: the foot at the next pose while it follows it (v < 4), else this tick's
		int16_t tn[3] = { (int16_t)r.b.t[0], (int16_t)r.b.t[1], (int16_t)r.b.t[2] };
		if (next_draws && v1 < 4 && g_next_foot_ok) memcpy(tn, g_next_foot, 6);
		static uint8_t keep[0x268], mid[0x268];
		memcpy(keep, (const void *)(MODEL_Kick + 8), 0x268);
		const uint8_t *a = g_verts[r.extra], *b = next_draws ? keep : a;
		memcpy(mid, a, 0x268);
		for (int k = 0; k < 0x268 / 8; k++)
			for (int c = 0; c < 3; c++) ((int16_t *)mid)[k * 4 + c] = lerp16(((const int16_t *)a)[k * 4 + c], ((const int16_t *)b)[k * 4 + c], num, den);
		memcpy((void *)(MODEL_Kick + 8), mid, 0x268);
		// the frame: the direction from the foot at 28 to the foot point, scaled by 1/8, at the foot point
		int16_t t[3];
		for (int c = 0; c < 3; c++) t[c] = lerp16((int16_t)r.b.t[c], tn[c], num, den);
		Mat4x3 bm = r.b;
		if (from && (t[0] != (int16_t)r.b.t[0] || t[1] != (int16_t)r.b.t[1] || t[2] != (int16_t)r.b.t[2]))
		{
			int32_t d[3] = { t[0] - from[0], t[1] - from[1], from[2] - t[2] };
			BuildOrthonormalBasis(d, &bm);
			d[1] = 0x200;
			d[0] = 0x200;
			d[2] = 0x200;
			Scale3DMatrix(&bm, d);
			bm.t[0] = t[0];
			bm.t[1] = t[1];
			bm.t[2] = t[2];
		}
		(void)to;
		ComposeAffineTransform(&Camera(), &bm, &bm);
		GteSetRotMatrix(&bm);
		GteSetTransVector(&bm);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Kick;
		// the draw mode switches on the real frame (at 1 the next tick's: fading from its 5th tick)
		if ((at_next ? v1 : r.v) >= 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(uint32_t *)(h + 8) = 0;
			const int32_t f1 = (int32_t)((v1 - 4) << 10);
			if (at_next) *(uint32_t *)(h + 0xC) = (uint32_t)f1;
			else
			{
				const int32_t f0 = (int32_t)((r.v - 4) << 10);
				*(uint32_t *)(h + 0xC) = (uint32_t)(next_draws ? lerp_i(f0, f1, num, den) : f0);
			}
		}
		else
		{
			*(uint32_t *)(h + 0x1C) = 0x30;
		}
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
		memcpy((void *)(MODEL_Kick + 8), keep, 0x268);
	}

	// ---- Meteor Strike's rising wave: its turn (0x80 a tick), his hip (at his next pose) and its fade num / den of the way
	static void HeldPillar(const Rec &r, int num, int den, bool at_next, bool next_tick, int16_t nc)
	{
		const bool next_draws = next_tick && nc >= 0x2C && nc < 0x35;
		if (at_next && !next_draws) return;
		int32_t angle = r.angle, fade = (int32_t)(r.counter - 0x2C) << 9;
		int16_t hip[4];
		memcpy(hip, r.hip, 8);
		if (next_draws)
		{
			angle = r.angle + 0x80 * num / den;
			fade = lerp_i(fade, (int32_t)(nc - 0x2C) << 9, num, den);
			if (g_next_hip_ok) for (int c = 0; c < 3; c++) hip[c] = lerp16(r.hip[c], g_next_hip[c], num, den);
		}
		Mat4x3 w;
		RotationY(angle, &w);
		w.t[1] = 0;
		w.m[1][1] = (int16_t)(-2 * hip[1]);
		w.t[0] = hip[0];
		w.t[2] = hip[2];
		ComposeAffineTransform(&Camera(), &w, &w);
		GteSetRotMatrix(&w);
		GteSetTransVector(&w);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Wave;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		*(uint32_t *)(h + 0xC) = (uint32_t)fade;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// ---- Meteor Strike's module-drawn target: the next tick's tilt / height offset / squash (its height measured at its
	// next pose for k >= 4); false when the next tick does not draw it
	static bool TargetNext(const Rec &r, int16_t nc, int16_t *ang, int16_t *off, int32_t *scale)
	{
		const MoveNode *m = Move();
		if (!m) return false;
		const uint32_t k = (uint32_t)(nc - 0x37);
		if (k >= 0x100 || !(r.ent[0] & 2)) return false;
		memcpy(ang, r.ang, 6);
		memcpy(off, r.off, 6);
		memcpy(scale, r.scale, 12);
		int16_t h[2] = { m->height[0], m->height[1] };
		int32_t tilt = r.ang[0];
		if (k < 4)
		{
			off[1] = (int16_t)((int16_t)((uint32_t)mul32((int32_t)h[1] - (int32_t)h[0], (int32_t)k) >> 3) + h[0]);
			tilt -= 0x200;
			if (tilt < -0x800) tilt = -0x800;
		}
		else
		{
			uint8_t *t = m->target;
			uint8_t *block = **(uint8_t ***)(t + 0x64);
			static uint8_t bones[0x20 + 0x100 * 0x30];
			const uint32_t nb = 0x20 + (uint32_t)block[0] * 0x30;
			memcpy(bones, block, nb);
			// its world translation on the next tick: the throw path's next point while the move still moves it (to 0x3B)
			int32_t wt[3];
			memcpy(wt, t + 0x54, 12);
			if ((uint16_t)nc <= 0x3B)
			{
				const int16_t *path = THROW_Table + nc * 4;
				W32(t, 0x54) = (int16_t)(m->anchor[0] + path[0]);
				W32(t, 0x58) = (int16_t)((int16_t)(path[1] - m->height[0]) + m->anchor[1]);
				W32(t, 0x5C) = (int16_t)(path[2] + m->anchor[2]);
			}
			pose_midpoint(t + 0x60, t + 0x6C, 1, 1);
			MeasureModelHeight(t, h);
			memcpy(t + 0x54, wt, 12);
			memcpy(block, bones, nb);
			off[1] = (int16_t)(((int32_t)h[1] + (int32_t)h[0]) >> 1);
		}
		scale[1] = 0x1000;
		if (k >= 4 && k < 0xC)
		{
			const int32_t s = 0x1000 - (ComputeSin((int32_t)((k - 4) << 8)) >> 1);
			scale[1] = s;
			off[1] = (int16_t)(mul32(off[1], s) >> 12);
		}
		ang[0] = (int16_t)tilt;
		return true;
	}
	static void HeldTarget(const Rec &r, int num, int den, bool at_next, bool next_tick, int16_t nc)
	{
		int16_t ang[3], off[3];
		int32_t scale[3];
		const bool next_draws = next_tick && TargetNext(r, nc, ang, off, scale);
		if (at_next && !next_draws) return;
		if (next_draws)
		{
			ang[0] = lerp_angle(r.ang[0], ang[0], num, den);
			off[1] = lerp16(r.off[1], off[1], num, den);
			scale[1] = lerp_i(r.scale[1], scale[1], num, den);
		}
		else
		{
			memcpy(ang, r.ang, 6);
			memcpy(off, r.off, 6);
			memcpy(scale, r.scale, 12);
		}
		uint8_t *e = r.ent;
		if (*(uint8_t **)(e + 0x64)) pose_midpoint(e + 0x60, e + 0x6C, num, den);
		DrawEntity(e, ang, off, scale);
	}

	// ---- particles
	static void HeldSpark(const Rec &r, int num, int den, bool at_next, bool next_tick)
	{
		SparkNode s;
		memcpy(&s, r.node, sizeof(s));
		const int16_t frames = *(const int16_t *)(SPARK_Table + s.seq * 8 + 4);
		if (at_next)
		{
			if (!next_tick || s.frame + 1 >= frames) return;
			s.frame++;
			for (int c = 0; c < 3; c++) s.pos[c] = (int16_t)(s.pos[c] + s.vel[c]);
		}
		else if (next_tick && s.frame + 1 < frames)
			for (int c = 0; c < 3; c++) s.pos[c] = lerp16(s.pos[c], (int16_t)(s.pos[c] + s.vel[c]), num, den);
		SparkDraw(&s);
	}
	static void HeldFlash(const Rec &r, bool at_next, bool next_tick)
	{
		FlashNode f;
		memcpy(&f, r.node, sizeof(f));
		if (at_next)
		{
			if (!next_tick || f.frame + 1 >= 8) return;
			f.frame++;
		}
		FlashDraw(&f);
	}
	static void HeldRock(const Rec &r, int num, int den, bool at_next, bool next_tick)
	{
		RockNode k;
		memcpy(&k, r.node, sizeof(k));
		RockNode n = k;
		n.ang[0] = (int16_t)(n.ang[0] + n.spin[0]);
		n.ang[2] = (int16_t)(n.ang[2] + n.spin[2]);
		n.vel[1] = (int16_t)(n.vel[1] + 0xF);
		n.ang[1] = (int16_t)(n.ang[1] + n.spin[1]);
		for (int c = 0; c < 3; c++) n.pos[c] = (int16_t)(n.pos[c] + n.vel[c]);
		if (n.pos[1] > 0) n.pos[1] = 0;
		const bool alive = next_tick && k.life + 1 < 0x14;
		if (at_next)
		{
			if (!alive) return;
			RockDraw(&n);
			return;
		}
		if (alive)
			for (int c = 0; c < 3; c++)
			{
				k.pos[c] = lerp16(k.pos[c], n.pos[c], num, den);
				k.ang[c] = (int16_t)(k.ang[c] + (int16_t)(k.spin[c] * num / den));
			}
		RockDraw(&k);
	}

	// the particle queues run on the next tick (sparks / hit flashes in the moves that run them, rocks in Meteor Strike)
	static bool NextRuns(uint8_t kind)
	{
		int16_t nc = 0;
		if (!NextMoveTick(&nc)) return false;
		const uint8_t mode = Move()->mode;
		if (kind == REC_ROCK) return mode == 7;
		if (kind == REC_FLASH) return mode == 2;
		return mode == 2 || mode == 3 || mode == 6;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		int16_t nc = 0;
		const bool next_tick = NextMoveTick(&nc);
		PredictNextPose();
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8], vbuf[0x8000];
		const uint32_t vb = var<uint32_t>(0x1D98B3C);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		const uint32_t frame_saved = FramePackets();
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		if (vb) memcpy(vbuf, (const void *)vb, sizeof(vbuf));
		g_in_held = true;
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			if (g_frame_start && frame_saved > g_frame_start)
			{
				const uint32_t used = frame_saved - g_frame_start;
				memcpy(g_held_frame, (const void *)g_frame_start, used < FARENA ? used : FARENA);
			}
			FramePackets() = (uint32_t)g_held_frame;
			// the orthonormal view copy of Heel Drop / Dolphin Blow, from the live view
			const MoveNode *m = Move();
			if (m && (m->mode == 4 || m->mode == 6)) EffectCameraMatrix(&Camera(), CamCopy());
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				switch (r.kind)
				{
				case REC_LAYOUT: HeldLayout(r, num, den, at_next, next_tick, nc); break;
				case REC_GHOSTS: HeldGhosts(r, num, den, at_next, next_tick); break;
				case REC_BARS: HeldBars(r, num, den, at_next); break;
				case REC_WAVE: HeldWave(r, num, den, at_next, next_tick, nc); break;
				case REC_ENTITY: HeldTarget(r, num, den, at_next, next_tick, nc); break;
				case REC_SPARK: HeldSpark(r, num, den, at_next, NextRuns(REC_SPARK)); break;
				case REC_FLASH: HeldFlash(r, at_next, NextRuns(REC_FLASH)); break;
				case REC_ROCK: HeldRock(r, num, den, at_next, NextRuns(REC_ROCK)); break;
				case REC_PILLAR: HeldPillar(r, num, den, at_next, next_tick, nc); break;
				}
			}
		}
		g_in_held = false;
		FramePackets() = frame_saved;
		var<uint16_t>(0x1D9770C) = w70c;
		if (vb) memcpy((void *)vb, vbuf, sizeof(vbuf));
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}
}

	static void register_mag274_held()
	{
		mark_held(duel274::ORIG_MoveTask);
		mark_held(duel274::ORIG_SparkTask);
		mark_held(duel274::ORIG_FlashTask);
		mark_held(duel274::ORIG_RockTask);
		register_module_held(274, duel274::HeldReady, duel274::HeldFrame);
	}
}
