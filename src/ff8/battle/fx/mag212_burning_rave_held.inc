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

// 30 fps part of mag212_burning_rave.cpp (Zell's Burning Rave): the draw log of the real tick, the held-frame drawing and the
// held-frame camera (included at the end of mag212_burning_rave.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// A real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the
// Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the same order, with the vanilla draw
// code, each at its in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every draw
// composes with it, as the real draws do with theirs):
//   Layouts   - through prim::play_held (the exact in-between records of what the player drew). Their frames (the director's,
//               without the view): the ground layout at Zell's anchor (fixed; its texture scroll 0x20 a tick num / den of the
//               way, the next tick's at 1), the layout at his bone 0x16 follows the bone: its next tick's point is measured at
//               his NEXT pose (pose_midpoint at 1: the look-ahead of the animation reader, with the world matrix the frame holds;
//               his real pose is put back), the held point num / den of the way; the layout before him num / den of the way to
//               the next tick's (his position words); the burning layout's frames are fixed points: its move to the box's far end
//               (0x6D, decoded again) is a cut (held below 1, not drawn at 1). The debris pieces (0x6A3510) are not spawned by a
//               held frame. Blended vertex frames go to a private buffer.
//   Glow      - (0x51..0x5C) its turn, its z scale (the cosine's argument) and its fade num / den of the way to the next tick's
//               (the fade switches on the real frame where the draw mode changes: at 4), its texture v the real one; its bone 18
//               matrix num / den of the way to the one at his NEXT pose, his world matrix to the one the frame holds.
//   Entities  - the module-drawn Zell (0x5E..0x6C) and targets (0x6D..0xAD): angles and offset num / den of the way to the next
//               draw's (the throw's spin from 0x86, its centre height measured at the target's NEXT pose on the switch to it),
//               the pose half way (pose_midpoint at num / den, body and weapon: the next-pose approach; the bone matrices put
//               back), the world matrix (+0x40) from the one the real draw used to the one the frame holds (the engine rebuilds
//               +0x40 from the entity's position every frame, before the effect: the next draw's; the throw's height is a
//               position word); a draw the next tick does not make holds below 1 and is not drawn at 1.
//   Ghosts    - (draw then record) the afterimages of Zell's model (274's code): each ghost's recorded bone matrices num / den
//               of the way to the same ghost's matrices on the next tick (the block after the tick: its newest record is this
//               tick's pose), on a private copy of the block; a ghost the next tick drops holds; at 1 the next tick's ghosts.
//   Capture   - the stage captured into VRAM (the root's start, 0x6D): replayed below 1 (the same capture, with the word 0x1D8E038
//               it was made with: the root's capture precedes the director's field of view), not drawn at 1.
//   Rocks / pieces / sprites - (draw then update) the node num / den of the way to its update (positions with the gravity /
//               the bounce, the rocks' angles); flipbook frames switch on the real frame; a node that ends on the next tick (or
//               the rock pool reset at 0x86) is not drawn at 1.
// A part the next tick will not draw is held up to 1 and not drawn at 1. Zell's / the targets' positions and flags, the screen
// flash and the camera return are the engine's. Nothing here writes game state: the module globals, both packet cursors and
// the capture cursor (private buffers holding the real tick's packets under the replay), the Field_Alloc scratch, the
// shadow-rotation scratch, the screen vertex buffer (0x1D98B3C), the render scratch words, the screen flash words, the stage's
// bone matrices, the word 0x1D9770C, the GTE registers, the entities' world / bone matrices are put back.
// Camera: HeldCamera runs the director's camera code for the next counter on saved words (pure math: CamLerp 0x657DF0; GTE,
// words, field of view, roll and the Field_Alloc scratch put back) and returns the in-between look-at / eye, holding on cuts
// (the direct set 0x86, the segment starts 0x12 / 0x2B / 0x51 / 0x6D, any jump over 1500); the shake y of 0x51..0x56 goes to
// held_shake_next. After the director the root puts back the saved view: the engine's.

namespace ff8fx
{
namespace rave212
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;   // one packet arena (magic buffer + 0xAAE0 / + 0x1AAE0)
	static const uint32_t FARENA = 0x30000;  // frame arena (ghosts, module-drawn entities and their shadows)
	static const uint32_t CARENA = 0x10000;  // the stage capture's arena (magic buffer + 0x1AAE0 / + 0x2AAE0)
	static const uint32_t BSS_LO = 0x25101F8, BSS_HI = 0x25102C0;
	static const uint32_t FLASH_LO = 0x1D9898C, FLASH_HI = 0x1D98A68; // the screen flash words (0x5713E0)
	enum : uint8_t { REC_LAYOUT = 1, REC_GHOSTS, REC_GLOW, REC_CAPTURE, REC_ENTITY = 6, REC_SPRITE, REC_ROCK };
	enum : int { ENT_CASTER = 1, ENT_STILL, ENT_SPIN };
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2510238; }

	struct Rec
	{
		uint8_t kind;
		int16_t counter;      // the director's counter on the tick (0: no director yet)
		int site;             // layout site; entity draw (ENT_*)
		uint8_t *layout;      // layout: the decoded layout
		PlayArg arg;          // layout: the block as handed over (the frame without the view)
		int32_t b;            // glow
		Mat4x3 bone, world;   // glow: Zell's bone 18 / world matrices as drawn; entity: its world matrix as drawn
		uint8_t *ent;         // entity
		int ti;               // entity: its target record (-1: Zell)
		int16_t ang[3], off[3];
		int16_t box_a[6], box_b[6]; // capture
		int16_t view;         // capture: the word 0x1D8E038 it was made with (its depth: the root's capture comes before the director's field of view)
		uint8_t node[0x24];   // particles: the node as drawn
		uint32_t model;       // rock model / sprite sequence
		int32_t depth;        // sprite depth
		int extra;            // ghosts: index in g_ghosts
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static bool g_in_held = false;
	static int g_nrec = 0, g_nscratch = 0, g_nghosts = 0;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static DirectorNode *g_dir = nullptr;   // the director that ran on the tick
	static int16_t g_dir_counter = 0;       // its counter on the tick
	static Rec g_recs[0x200];
	static uint8_t g_scratch[0x200][SCRATCH];
	static uint8_t g_ghosts[2][0x488];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_capture[CARENA];
	static uint8_t g_held_morph[0x2000];
	static uint8_t g_held_ghost[0x488];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];
	// the next-pose predictions of the real tick (made at its first held frame)
	static bool g_next_point_ok = false, g_next_bone_ok = false, g_next_h_ok[10];
	static int16_t g_next_point[4], g_next_h[10][2];
	static Mat4x3 g_next_bone;

	static void held_note_root(RootNode *r)
	{
		(void)r;
		g_ported_tick = g_real_tick;
		if (g_in_held) return;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
		g_nghosts = 0;
		g_packets = 0;
		g_frame_start = 0;
		g_dir = nullptr;
		g_dir_counter = 0;
	}

	static void held_note_director(DirectorNode *d)
	{
		if (g_in_held || g_log_tick != g_real_tick) return;
		g_dir = d;
		g_dir_counter = (int16_t)(d->counter + 1); // (the counter steps first)
	}

	static Rec *NewRec(uint8_t kind)
	{
		if (g_in_held || g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.counter = g_dir_counter;
		r.extra = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r.scratch = g_nscratch++;
		memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_layout(DirectorNode *d, int site, uint8_t *layout, const PlayArg *arg)
	{
		(void)d;
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

	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off)
	{
		Rec *r = NewRec(REC_ENTITY);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ent = e;
		r->ti = -1;
		if (g_dir)
			for (int i = 0; i < 10; i++)
				if (g_dir->recs[i].e == e) { r->ti = i; break; }
		const int16_t c = r->counter;
		r->site = (uint32_t)(c - 0x86) < 0x28 ? ENT_SPIN : (uint32_t)(c - 0x6D) < 0x19 ? ENT_STILL : ENT_CASTER;
		memcpy(r->ang, angles, 6);
		memcpy(r->off, off, 6);
		r->world = *(const Mat4x3 *)(e + 0x40);
	}

	static void held_note_capture(const int16_t *a, const int16_t *b)
	{
		Rec *r = NewRec(REC_CAPTURE);
		if (!r) return;
		memcpy(r->box_a, a, 12);
		memcpy(r->box_b, b, 12);
		r->view = var<int16_t>(0x1D8E038);
	}

	static void held_note_glow(int32_t b)
	{
		Rec *r = NewRec(REC_GLOW);
		if (!r) return;
		r->b = b;
		uint8_t *skel = **(uint8_t ***)(Caster() + 0x64);
		r->bone = *(const Mat4x3 *)(skel + 0x380);
		r->world = *(const Mat4x3 *)(Caster() + 0x40);
	}

	static void held_note_rock(const RockNode *k, uint32_t model)
	{
		Rec *r = NewRec(REC_ROCK);
		if (!r) return;
		memcpy(r->node, k, sizeof(RockNode));
		r->model = model;
	}

	static void held_note_sprite(const SpriteNode *s, uint32_t seq, int32_t depth)
	{
		Rec *r = NewRec(REC_SPRITE);
		if (!r) return;
		memcpy(r->node, s, sizeof(SpriteNode));
		r->model = seq;
		r->depth = depth;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the next tick (from the state the real tick left)
	// the counter the director runs with on the next tick (0: none)
	static int16_t NextCounter()
	{
		const TaskNode *n = QDirector()->head;
		return n ? (int16_t)(((const DirectorNode *)n)->counter + 1) : 0;
	}
	// the director draws on the next tick (it returns before any draw from its damage tick 0xAE)
	static bool DirDraws(int16_t nc) { return nc != 0 && nc < 0xAE; }
	// the root runs the rock / sprite queues on the next tick
	static bool RootNext() { return QRoot()->head != nullptr; }
	static bool SiteNext(int site, int16_t c, int16_t nc)
	{
		if (!DirDraws(nc)) return false;
		switch (site)
		{
		case SITE_GROUND: return (uint32_t)(nc - 2) < 0x38;
		case SITE_BONE: return (uint32_t)(nc - 0x51) < 0x18;
		case SITE_FRONT: return (uint32_t)(nc - 0x54) < 0x1A;
		case SITE_BURN: return (uint32_t)(nc - 0x5E) < 0x46 && ((uint32_t)(c - 0x5E) < 0xF) == ((uint32_t)(nc - 0x5E) < 0xF); // (0x6D: decoded again at the far end: a cut)
		}
		return false;
	}
	static bool GlowNext(int16_t nc) { return DirDraws(nc) && (uint32_t)(nc - 0x51) < 0xC; }
	// the layout has a frame left to play (prim::play: frames 0..total - 1, a finished layout draws nothing)
	static bool LayoutMore(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// skeleton of an anim header (BattleAnimHeader: +4 -> pointer -> bone count, matrices)
	static uint8_t *Skeleton(uint32_t anim, uint32_t *bytes)
	{
		if (!anim) return nullptr;
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_bones_save[0])) n = sizeof(g_bones_save[0]);
		*bytes = n;
		return sk;
	}

	static void LerpMat(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out->m[i][j] = lerp16(a.m[i][j], b.m[i][j], num, den);
		for (int k = 0; k < 3; k++) out->t[k] = lerp_i(a.t[k], b.t[k], num, den);
	}

	// Zell at his next pose (pose_midpoint at 1): the bone 0x16 point and the bone 18 matrix of the next tick; the targets'
	// centre heights at their next pose when the throw starts on the next tick (0x86). Bone matrices, 0x1D9770C, the Field_Alloc
	// scratch and the GTE registers put back. Once per real tick.
	static void PredictNextPose(int16_t nc)
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_next_point_ok = g_next_bone_ok = false;
		memset(g_next_h_ok, 0, sizeof(g_next_h_ok));
		bool point = false, bone = false, heights = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_LAYOUT && r.site == SITE_BONE && SiteNext(SITE_BONE, r.counter, nc)) point = true;
			if (r.kind == REC_GLOW && GlowNext(nc)) bone = true;
			if (r.kind == REC_ENTITY && r.site == ENT_STILL && nc == 0x86 && r.ti >= 0) heights = true;
		}
		if (!point && !bone && !heights) return;
		static uint8_t fas[0x8000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		if (fa) memcpy(fas, (const void *)fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		uint8_t *e = Caster();
		uint32_t nb = 0;
		uint8_t *sk = (point || bone) && e && (e[0] & 2) ? Skeleton((uint32_t)(e + 0x60), &nb) : nullptr;
		if (sk)
		{
			memcpy(g_bones_save[0], sk, nb);
			pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
			if (bone)
			{
				g_next_bone = *(const Mat4x3 *)(sk + 0x380);
				g_next_bone_ok = true;
			}
			if (point)
			{
				GetEffectSpawnPosition(e, 0x16, 0, g_next_point);
				g_next_point_ok = true;
			}
			memcpy(sk, g_bones_save[0], nb);
		}
		if (heights && g_dir)
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				if (r.kind != REC_ENTITY || r.site != ENT_STILL || r.ti < 0 || g_next_h_ok[r.ti]) continue;
				uint8_t *t = r.ent;
				uint32_t tb = 0;
				uint8_t *tsk = (t[0] & 2) ? Skeleton((uint32_t)(t + 0x60), &tb) : nullptr;
				if (!tsk) continue;
				memcpy(g_bones_save[0], tsk, tb);
				pose_midpoint(t + 0x60, t + 0x6C, 1, 1);
				MeasureModelHeight(t, g_next_h[r.ti]);
				memcpy(tsk, g_bones_save[0], tb);
				g_next_h_ok[r.ti] = true;
			}
		var<uint16_t>(0x1D9770C) = w70c;
		var<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	// ---- layouts
	static void HeldLayout(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const bool next = SiteNext(r.site, r.counter, nc) && LayoutMore((const prim::Layout *)r.layout);
		if (at_next && !next) return;
		const int pn = next ? num : 0;
		PlayArg arg = r.arg;
		switch (r.site)
		{
		case SITE_GROUND:
			// its texture v scrolls 0x20 a tick
			*(uint16_t *)&arg.scroll = (uint16_t)(*(const uint16_t *)&r.arg.scroll + 0x20 * pn / den);
			break;
		case SITE_BONE:
			// his bone 0x16 point at the next pose
			if (next && g_next_point_ok) for (int c = 0; c < 3; c++) arg.m.t[c] = lerp_i(r.arg.m.t[c], g_next_point[c], num, den);
			break;
		case SITE_FRONT:
			if (next)
			{
				arg.m.t[0] = lerp_i(r.arg.m.t[0], *(const int16_t *)(Caster() + 0x1C) - 0xC3, num, den);
				arg.m.t[2] = lerp_i(r.arg.m.t[2], *(const int16_t *)(Caster() + 0x20) - 0x48F, num, den);
			}
			break;
		}
		arg.pieces = nullptr; // (the debris pieces are spawned by the real tick only)
		arg.morph = (uint32_t)g_held_morph;
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		prim::play_held((prim::Layout *)r.layout, PartCallback, (int)&arg, pn, den);
	}

	// ---- the glow at his bone 18
	static void HeldGlow(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const bool next = GlowNext(nc);
		if (at_next && !next) return;
		GlowArgs g0, g1;
		GlowParams(r.b, &g0);
		GlowParams(r.b + 1, &g1);
		GlowArgs g = at_next ? g1 : g0;
		Mat4x3 bone = r.bone, world = r.world;
		if (next)
		{
			if (!at_next)
			{
				g.spin = lerp_i(g0.spin, g1.spin, num, den);
				g.cosarg = lerp_i(g0.cosarg, g1.cosarg, num, den);
				if (g0.mode == 0xF0 && g1.mode == 0xF0) g.fade = (uint32_t)lerp_i((int32_t)g0.fade, (int32_t)g1.fade, num, den);
			}
			if (g_next_bone_ok) LerpMat(&bone, r.bone, g_next_bone, num, den);
			LerpMat(&world, r.world, *(const Mat4x3 *)(Caster() + 0x40), num, den);
		}
		GlowDraw(g, &bone, &world);
	}

	// ---- ghosts: ghost i of a block = slot ((block[3] - i) & 1)
	static const uint8_t *GhostSlot(const uint8_t *g, int i) { return g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x200; }
	static void HeldGhosts(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const uint8_t *now = g_ghosts[r.extra];
		const uint8_t *nx = (DirDraws(nc) && nc > 0x40 && nc <= 0x50 && g_dir && QDirector()->head == (TaskNode *)g_dir) ? g_dir->ghosts : nullptr;
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

	// ---- the module-drawn entities: the next draw's angles / offset (false: the next tick does not draw it)
	static bool EntityNext(const Rec &r, int16_t nc, int16_t *ang, int16_t *off)
	{
		if (!DirDraws(nc)) return false;
		memset(ang, 0, 6);
		memset(off, 0, 6);
		switch (r.site)
		{
		case ENT_CASTER: return (uint32_t)(nc - 0x5E) < 0xF;
		case ENT_STILL:
			if ((uint32_t)(nc - 0x6D) < 0x19) return true;
			if (nc != 0x86 || r.ti < 0 || !g_next_h_ok[r.ti]) return false;
			// the throw's first draw: his angles, about the centre height measured then
			ang[0] = *(const int16_t *)(Caster() + 0xC);
			ang[1] = *(const int16_t *)(Caster() + 0xE);
			off[1] = (int16_t)(((int32_t)g_next_h[r.ti][1] + (int32_t)g_next_h[r.ti][0]) >> 1);
			return true;
		case ENT_SPIN:
		{
			const uint32_t t1 = (uint32_t)(nc - 0x86);
			if (t1 >= 0x28) return false;
			const int32_t k = (int32_t)((t1 << 12) / 40);
			ang[0] = (int16_t)(*(const int16_t *)(Caster() + 0xC) + k);
			ang[1] = (int16_t)(*(const int16_t *)(Caster() + 0xE) + 2 * k);
			ang[2] = r.ang[2];
			memcpy(off, r.off, 6);
			return true;
		}
		}
		return false;
	}
	static void HeldEntity(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		int16_t na[3], no[3];
		const bool next = EntityNext(r, nc, na, no);
		if (at_next && !next) return;
		uint8_t *e = r.ent;
		const int pn = next ? num : 0;
		int16_t ang[3], off[3];
		for (int c = 0; c < 3; c++)
		{
			ang[c] = next ? lerp_angle(r.ang[c], na[c], num, den) : r.ang[c];
			off[c] = next ? lerp16(r.off[c], no[c], num, den) : r.off[c];
		}
		// the world matrix: from the one drawn to the one the frame holds (the next draw's)
		const Mat4x3 keep = *(const Mat4x3 *)(e + 0x40);
		LerpMat((Mat4x3 *)(e + 0x40), r.world, keep, pn, den);
		uint32_t b0 = 0, b1 = 0;
		uint8_t *sk0 = Skeleton((uint32_t)(e + 0x60), &b0);
		const uint32_t w = *(const uint32_t *)(e + 0x78);
		uint8_t *sk1 = Skeleton(w, &b1);
		if (sk0) memcpy(g_bones_save[0], sk0, b0);
		if (sk1) memcpy(g_bones_save[1], sk1, b1);
		if (pn)
		{
			pose_midpoint(e + 0x60, e + 0x6C, pn, den);
			if (w) pose_midpoint((void *)w, (void *)(w + 0xC), pn, den);
		}
		DrawEntity(e, ang, off);
		if (sk0) memcpy(sk0, g_bones_save[0], b0);
		if (sk1) memcpy(sk1, g_bones_save[1], b1);
		*(Mat4x3 *)(e + 0x40) = keep;
	}

	// ---- the stage capture: the same capture below 1 (the next tick captures nothing)
	static void HeldCapture(const Rec &r, bool at_next)
	{
		if (at_next) return;
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(0x1D989D0, &nb);
		if (sk) memcpy(g_bones_save[0], sk, nb);
		const int16_t view = var<int16_t>(0x1D8E038);
		var<int16_t>(0x1D8E038) = r.view;
		CaptureStage(r.box_a, r.box_b);
		var<int16_t>(0x1D8E038) = view;
		if (sk) memcpy(sk, g_bones_save[0], nb);
	}

	// ---- particles (the root runs the queues on the next tick while it lives; the director resets the rock pool at 0x86)
	static void HeldSprite(const Rec &r, int num, int den, bool at_next)
	{
		SpriteNode s;
		memcpy(&s, r.node, sizeof(s));
		const bool next = RootNext() && s.life != 1;
		if (at_next && !next) return;
		SpriteNode n = s;
		SpriteMove(&n);
		n.life--;
		if (at_next) s = n;
		else if (next) for (int c = 0; c < 3; c++) s.pos[c] = lerp16(s.pos[c], n.pos[c], num, den);
		SpriteDraw(&s, r.model, r.depth);
	}
	static void HeldRock(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		RockNode k;
		memcpy(&k, r.node, sizeof(k));
		const bool next = RootNext() && k.life != 1 && nc != 0x86;
		if (at_next && !next) return;
		RockNode n = k;
		if ((uint32_t)k.hdr.func == ORIG_RockTask) RockMoveBounce(&n);
		else RockMove(&n);
		n.life--;
		if (at_next)
		{
			RockDrawAt(n.pos, (int32_t)(int8_t)n.rx << 4, (int32_t)(int8_t)n.ry << 4, n.scale, r.model);
			return;
		}
		int16_t pos[3];
		int32_t ax = (int32_t)(int8_t)k.rx << 4, ay = (int32_t)(int8_t)k.ry << 4;
		for (int c = 0; c < 3; c++) pos[c] = next ? lerp16(k.pos[c], n.pos[c], num, den) : k.pos[c];
		if (next)
		{
			ax += ((int32_t)k.spin[0] << 4) * num / den;
			ay += ((int32_t)k.spin[1] << 4) * num / den;
		}
		RockDrawAt(pos, ax, ay, k.scale, r.model);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		const int16_t nc = NextCounter();
		PredictNextPose(nc);
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8], flash[FLASH_HI - FLASH_LO], vbuf[0x8000];
		const uint32_t vb = var<uint32_t>(0x1D98B3C);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		const uint32_t frame_saved = FramePackets();
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		memcpy(flash, (const void *)FLASH_LO, sizeof(flash));
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
			CaptureCursor() = (uint32_t)g_held_capture;
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				switch (r.kind)
				{
				case REC_LAYOUT: HeldLayout(r, num, den, at_next, nc); break;
				case REC_GHOSTS: HeldGhosts(r, num, den, at_next, nc); break;
				case REC_GLOW: HeldGlow(r, num, den, at_next, nc); break;
				case REC_CAPTURE: HeldCapture(r, at_next); break;
				case REC_ENTITY: HeldEntity(r, num, den, at_next, nc); break;
				case REC_SPRITE: HeldSprite(r, num, den, at_next); break;
				case REC_ROCK: HeldRock(r, num, den, at_next, nc); break;
				}
			}
		}
		g_in_held = false;
		FramePackets() = frame_saved;
		var<uint16_t>(0x1D9770C) = w70c;
		if (vb) memcpy((void *)vb, vbuf, sizeof(vbuf));
		memcpy((void *)FLASH_LO, flash, sizeof(flash));
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}

	// ------------------------------------------------------------------
	// held-frame camera
	// ------------------------------------------------------------------
	// counters on which the director writes the camera words (1 is the director's first tick: never a next one)
	static bool CameraWritten(int32_t c)
	{
		return (uint32_t)(c - 0x12) < 0x18 || (uint32_t)(c - 0x2B) < 0x26 || (uint32_t)(c - 0x51) < 0x1C || (uint32_t)(c - 0x6D) < 0x19 || c == 0x86;
	}
	// the segment starts and the direct set: the camera jumps
	static bool CameraCut(int32_t c) { return c == 0x12 || c == 0x2B || c == 0x51 || c == 0x6D || c == 0x86; }
	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (!HeldReady()) return false;
		const int16_t nc = NextCounter(); // the counter the next tick runs
		if (!nc || !CameraWritten(nc)) return false;
		int16_t *eye = CamEye(), *at = CamLookAt();
		int16_t e0[3], a0[3];
		for (int k = 0; k < 3; k++) { e0[k] = eye[k]; a0[k] = at[k]; }
		bool cut = CameraCut(nc);
		int16_t e1[3], a1[3];
		if (!cut)
		{
			// the director's camera code for the next counter on the saved words (the scratch above the Field_Alloc pointer,
			// the GTE, the words, field of view and roll put back)
			static uint8_t cam_save[0x10], fa_save[0x1000];
			static uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
			uint8_t *fa = var<uint8_t *>(0x1D999C4);
			if (fa) memcpy(fa_save, fa, sizeof(fa_save));
			memcpy(cam_save, (const void *)0xB8B7F0, sizeof(cam_save));
			const int16_t fov = var<int16_t>(0x1D8E038), roll = var<int16_t>(0x1D977A2);
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			DirectorCamera(nc, false);
			for (int k = 0; k < 3; k++) { e1[k] = eye[k]; a1[k] = at[k]; }
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			var<int16_t>(0x1D977A2) = roll;
			var<int16_t>(0x1D8E038) = fov;
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			var<uint8_t *>(0x1D999C4) = fa;
			if (fa) memcpy(fa, fa_save, sizeof(fa_save));
			// a keyed segment that starts away from the current camera is a cut too
			for (int k = 0; k < 3; k++)
				if (AbsI(e1[k] - e0[k]) > 1500 || AbsI(a1[k] - a0[k]) > 1500) cut = true;
		}
		// the shake y the next tick writes (0x51..0x56)
		if ((uint32_t)(nc - 0x51) < 6)
		{
			const int16_t sh[3] = { 0, ShakeY(nc - 0x51), 0 };
			held_shake_next(sh, 2);
		}
		for (int k = 0; k < 3; k++)
		{
			world[k] = cut ? e0[k] : lerp16(e0[k], e1[k], num, den);
			lookat[k] = cut ? a0[k] : lerp16(a0[k], a1[k], num, den);
		}
		return true;
	}
}

	static void register_mag212_held()
	{
		using namespace rave212;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_DirectorTask);
		mark_held(ORIG_RockTask);
		mark_held(ORIG_RockATask);
		mark_held(ORIG_RockBTask);
		mark_held(ORIG_RockCTask);
		mark_held(ORIG_PieceTask);
		mark_held(ORIG_SparkTask);
		mark_held(ORIG_EmberTask);
		mark_held(ORIG_FlameTask);
		register_module_held(212, HeldReady, HeldFrame);
		register_module_camera(212, HeldCamera);
	}
}
