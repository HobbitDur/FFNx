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

// 30 fps part of mag216_different_beat.cpp (Zell's Different Beat): the draw log of the real tick, the held-frame drawing and
// the held-frame camera (included at the end of mag216_different_beat.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// A real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the
// Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the same order, with the vanilla draw
// code, each at its in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every draw
// composes with it, as the real draws do with theirs):
//   Layouts   - through prim::play_held (the exact in-between records of what the player drew). Their frames (the director's,
//               without the view) num / den of the way to the next tick's draw of the same layout, from the director's formulas
//               for the next counter: the rise layout at Zell's default position, the fly / drop layouts at his bone 0x14 (both
//               measured at his NEXT pose: pose_midpoint at 1, the look-ahead of the animation reader, with the world matrix
//               the frame holds; his real pose is put back; the drop's tilt angle num / den of the way), the impact layout at
//               his position (its height from the squash of the next counter), the crater layout fixed, the pillar over the
//               target (its height from the pillar's next lift; camera-facing: the same instruction sequence with the live
//               view); texture scrolls num / den of the way; fades num / den of the way where both ticks fade (the switch to /
//               from no fade on the real frame). A layout the next tick does not draw is held below 1, not drawn at 1.
//   Twist     - (0x11..0x15, four of every six ticks 0x17..0x46) the twisted model at his bone 0x14 (his next pose), its twist
//               scale / angle num / den of the way to the next tick's (its last step's flags / fade on the real frame), into a
//               private vertex buffer.
//   Rings     - their layouts at their fixed frames (none at 1 when the next tick clears the ring queue: 0x47).
//   Ghosts    - (draw then record) the afterimages of Zell's model: each ghost's recorded bone matrices num / den of the way
//               to the same ghost's matrices on the next tick (the block after the tick), on a private copy of the block, his
//               world matrix from the one drawn to the one the frame holds (he moves: the ghosts follow him); a ghost the next
//               tick drops holds; none at 1 when the next tick draws none (0x9: the director resets them first).
//   Entities  - the target's chain drawn by the module (0x17..0x66, 0x71..0x89): angles / offset / scale num / den of the way
//               to the next draw's (the sway's angles from the next counter, its offset from the heights the next tick measures
//               at 0x5D: MeasureHeight at the target's NEXT pose; the squash's y scale), the pose half way (pose_midpoint at
//               num / den, body and weapon; the bone matrices put back), the world matrix (+0x40) from the one the real draw
//               used to the one the frame holds (the engine rebuilds it from the position every frame: the next draw's); a
//               world jump over 1500 (0x47: the target moved to the pillar) is a cut (held below 1).
//   Capture   - the stage captured into VRAM (the director's start): replayed below 1 (the word 0x1D8E038 it was made with), not
//               drawn at 1.
//   Tile      - the white tile (0x72 / 0x73) its level num / den of the way to the next tick's.
//   Rocks / sprites - (draw then update) the node num / den of the way to its update (positions, the rocks' angles); flipbook
//               frames switch on the real frame; a node that ends on the next tick is not drawn at 1.
// A part the next tick will not draw is held up to 1 and not drawn at 1. Zell's / the target's positions and flags, the screen
// flash (the root's) and the camera return are the engine's. Nothing here writes game state: the module globals, both packet
// cursors (private buffers holding the real tick's packets under the replay), the frame arena, the Field_Alloc scratch, the
// shadow-rotation scratch, the screen vertex buffer (0x1D98B3C), the render scratch words, the screen flash words (the capture
// turns the flash off), the stage's bone matrices, the word 0x1D9770C, the GTE registers, the entities' world translation / world /
// bone matrices are put back.
// Camera: HeldCamera runs the director's camera code for the next counter on saved words (pure math: CamLerp 0x657DF0; GTE,
// words, field of view, roll and the Field_Alloc scratch put back) and returns the in-between look-at / eye, holding on cuts
// (the direct sets 8 / 0x10 / 0x70, the segment starts 0x16 / 0x46, any jump over 1500). The field of view / roll: the frame
// gate's.

namespace ff8fx
{
namespace beat216
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x1A000;   // one packet arena (magic buffer + 0x3198 / + 0x1D198)
	static const uint32_t CARENA = 0x1A000;  // the stage capture's arena (magic buffer + 0x1D198 / + 0x37198)
	static const uint32_t FARENA = 0x30000;  // frame arena (module-drawn entities and their shadows)
	static const uint32_t BSS_LO = 0x2508870, BSS_HI = 0x2508930;
	static const uint32_t FLASH_LO = 0x1D9898C, FLASH_HI = 0x1D98A68; // the screen flash words (0x5713E0)
	enum : uint8_t { REC_PLAY = 1, REC_GHOSTS, REC_TWIST, REC_CAPTURE, REC_RING, REC_ENTITY, REC_SPRITE, REC_ROCK, REC_TILE };

	struct Rec
	{
		uint8_t kind;
		int16_t counter;      // the director's counter on the tick (-1: no director)
		int site;             // layout site
		uint8_t *layout;      // layout / ring: the decoded layout
		PlayArg arg;          // layout: the block as handed over (its frame composed with the view)
		Mat4x3 frame;         // layout: the block's frame without the view
		int16_t point[4];     // twist: the bone point; capture: the position
		int16_t box[6];       // capture
		int16_t view;         // capture: the word 0x1D8E038 it was made with
		int32_t a, b;         // twist: scale / angle; tile: frame
		bool last;            // twist: its last step
		uint8_t *ent;         // entity
		int16_t ang[3], off[3];
		int32_t scale[3];
		Mat4x3 world;         // entity: its world matrix as drawn
		uint8_t node[0x24];   // ring / particles: the node as drawn
		uint32_t model;       // sprite sequence
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
	static int16_t g_dir_counter = -1;      // its counter on the tick
	static Rec g_recs[0x200];
	static uint8_t g_scratch[0x200][SCRATCH];
	static uint8_t g_ghosts[2][0x988];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_capture[CARENA];
	static uint8_t g_held_morph[0x2000];
	static uint8_t g_held_twist[0x2000];
	static uint8_t g_held_ghost[0x988];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];
	// the next-pose predictions of the real tick (made at its first held frame)
	static bool g_next_b14_ok = false, g_next_def_ok = false, g_next_h_ok = false;
	static int16_t g_next_b14[4], g_next_def[4], g_next_h[2];

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
		g_dir_counter = -1;
	}

	static void held_note_director(DirectorNode *d)
	{
		if (g_in_held || g_log_tick != g_real_tick) return;
		g_dir = d;
		g_dir_counter = d->counter; // (the counter steps last)
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

	static void held_note_play(int site, uint8_t *layout, const PlayArg *arg, const Mat4x3 *frame)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->site = site;
		r->layout = layout;
		r->arg = *arg;
		r->frame = *frame;
	}

	static void held_note_twist(const int16_t *p, int32_t scale, int32_t angle, bool last)
	{
		Rec *r = NewRec(REC_TWIST);
		if (!r) return;
		memcpy(r->point, p, 6);
		r->a = scale;
		r->b = angle;
		r->last = last;
	}

	static void held_note_tile(int32_t f)
	{
		Rec *r = NewRec(REC_TILE);
		if (!r) return;
		r->a = f;
	}

	static void held_note_ghosts(uint8_t *entity, uint8_t *g)
	{
		(void)entity;
		if (g_nghosts >= (int)(sizeof(g_ghosts) / sizeof(g_ghosts[0]))) return;
		Rec *r = NewRec(REC_GHOSTS);
		if (!r) return;
		r->extra = g_nghosts++;
		memcpy(g_ghosts[r->extra], g, 0x988);
		r->world = *(const Mat4x3 *)(entity + 0x40);
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
		r->world = *(const Mat4x3 *)(e + 0x40);
	}

	static void held_note_capture(const int16_t *pos, const int16_t *box)
	{
		Rec *r = NewRec(REC_CAPTURE);
		if (!r) return;
		memcpy(r->point, pos, 6);
		memcpy(r->box, box, 12);
		r->view = var<int16_t>(0x1D8E038);
	}

	static void held_note_ring(RingNode *k)
	{
		Rec *r = NewRec(REC_RING);
		if (!r) return;
		r->layout = k->layout;
		memcpy(r->node, k, 0x18);
	}

	static void held_note_rock(const RockNode *k)
	{
		Rec *r = NewRec(REC_ROCK);
		if (!r) return;
		memcpy(r->node, k, sizeof(RockNode));
	}

	static void held_note_sprite(const SpriteNode *s, uint32_t seq)
	{
		Rec *r = NewRec(REC_SPRITE);
		if (!r) return;
		memcpy(r->node, s, sizeof(SpriteNode));
		r->model = seq;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the next tick (from the state the real tick left)
	// the counter the director runs with on the next tick (-1: none)
	static int16_t NextCounter()
	{
		const TaskNode *n = QDirector()->head;
		return n ? ((const DirectorNode *)n)->counter : (int16_t)-1;
	}
	// the root runs the ring / rock / sprite queues on the next tick
	static bool RootNext() { return QRoot()->head != nullptr; }
	// the layout has a frame left to play (prim::play: frames 0..total - 1, a finished layout draws nothing)
	static bool LayoutMore(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}
	// the site the next tick draws the same decoded layout with (0: none)
	static int SiteOf(const uint8_t *layout, int16_t c)
	{
		if (c < 0 || !g_dir) return 0;
		if (layout == g_dir->layout_a)
		{
			if ((uint32_t)(c - 9) < 0xE) return SITE_RISE;
			if ((uint32_t)(c - 0x5D) < 0xA) return SITE_FLY;
			if ((uint32_t)(c - 0x67) < 0x12) return SITE_DROP_A;
			return 0;
		}
		if (layout == g_dir->layout_b) return (uint32_t)(c - 0x67) < 0x12 ? SITE_DROP_B : 0;
		if (layout == g_dir->layout_c) return (uint32_t)(c - 0x71) < 0xC ? SITE_IMPACT : 0;
		if (layout == g_dir->layout_d) return (uint32_t)(c - 0x71) < 0x19 ? SITE_CRATER : 0;
		if (layout == g_dir->layout_e) return (uint32_t)(c - 0x29) < 0x48 ? SITE_PILLAR : 0;
		return 0;
	}
	// a new decode of the layout before the next draw: the next tick starts it over (a cut)
	static bool Redecoded(int site, int16_t nc)
	{
		return (site == SITE_FLY && nc == 0x5D) || (site == SITE_DROP_B && nc == 0x67) || (site == SITE_IMPACT && nc == 0x71) || (site == SITE_CRATER && nc == 0x71) || (site == SITE_PILLAR && nc == 0x29) || (site == SITE_RISE && nc == 9);
	}
	// the impact squash's y scale of counter c (0x71..0x89)
	static int32_t Squash(int32_t c)
	{
		const int32_t e24 = (int32_t)(((uint32_t)(c - 0x71) << 12) / 24);
		int32_t v = 0x1000 - ComputeSin(e24 >> 1);
		v = mul32(v, v) >> 12;
		v = mul32(v, v) >> 12;
		v = mul32(v, v) >> 12;
		return (v >> 2) + (v >> 1) + 0x400;
	}
	// the pillar's lift on the next tick (0x47: set from the height word; 0x47..0x66: + cos / 12)
	static int16_t NextLift(int16_t nc)
	{
		int16_t lift = g_dir->lift;
		const int32_t s = nc - 0x47;
		if (s == 0) lift = (int16_t)(Height() - 0x1055);
		if ((uint32_t)s < 0x20) lift = (int16_t)(lift + (int16_t)(ComputeCos((int32_t)(((uint32_t)s << 12) >> 5) >> 2) / 12));
		return lift;
	}
	static int16_t PillarFade(int32_t s)
	{
		if ((uint32_t)s < 0xC) return (int16_t)((uint32_t)(0xC000 - ((uint32_t)s << 12)) / 12 + 0xAA);
		if ((uint32_t)s >= 0x3C) return (int16_t)(((uint32_t)(s - 0x3C) << 12) / 12 + 0xAA);
		return 0;
	}
	// the twist of counter c (false: none): scale / angle / its last step
	static bool TwistOf(int32_t c, int32_t *scale, int32_t *angle, bool *last)
	{
		const int32_t s = c - 9;
		if ((uint32_t)s >= 8 && (uint32_t)s < 0xD)
		{
			const int32_t t = s - 8;
			*scale = shl32(t, 12) / 5 / 2 + 0x800;
			*angle = 0x400 - shl32(t, 10);
			*last = s == 0xC;
			return true;
		}
		const int32_t s2 = c - 0x17;
		if ((uint32_t)s2 < 0x30 && (uint32_t)s2 % 6 <= 3)
		{
			const int32_t r = (int32_t)((uint32_t)s2 % 6);
			*scale = shl32(r, 12) / 3 / 2 + 0x800;
			*angle = 0x400 - shl32(r, 10);
			*last = r == 3;
			return true;
		}
		return false;
	}
	static bool GhostsDrawn(int32_t c) { return (c > 0 && c <= 0xC) || (c > 0x7B && c <= 0x82) || (c > 0x17 && c < 0x47); }
	// the target's module draw on counter c (false: none): angles / offset / scale (offset: the heights `h`)
	static bool EntityOf(int32_t c, const int16_t *h, int16_t *ang, int16_t *off, int32_t *scale)
	{
		memset(ang, 0, 6);
		memset(off, 0, 6);
		scale[0] = scale[1] = scale[2] = 0x1000;
		if ((uint32_t)(c - 0x17) < 0x30) return true;
		const int32_t s = c - 0x47;
		if ((uint32_t)s < 0x20)
		{
			const int32_t a = (int32_t)(((uint32_t)s << 12) >> 5);
			ang[0] = (int16_t)(ComputeSin(a * 2) >> 4);
			off[1] = (int16_t)((h[1] + h[0]) >> 1);
			return true;
		}
		if ((uint32_t)(c - 0x71) < 0x19)
		{
			scale[1] = Squash(c);
			return true;
		}
		return false;
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
	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

	// Zell at his next pose (pose_midpoint at 1): his bone 0x14 point and default effect position of the next tick (the frame's
	// world matrix: the engine built it from his position after this tick); the target's heights the next tick measures (0x5D:
	// MeasureHeight at its next pose). Bone matrices, the target's world translation, 0x1D9770C, the Field_Alloc scratch and the
	// GTE registers put back. Once per real tick.
	static void PredictNextPose(int16_t nc)
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_next_b14_ok = g_next_def_ok = g_next_h_ok = false;
		bool b14 = false, def = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_TWIST) b14 = true;
			if (r.kind != REC_PLAY) continue;
			const int ns = SiteOf(r.layout, nc);
			if (ns == SITE_RISE) def = true;
			if (ns == SITE_FLY || ns == SITE_DROP_A || ns == SITE_DROP_B) b14 = true;
		}
		const bool heights = nc == 0x5D && g_dir;
		if (!b14 && !def && !heights) return;
		static uint8_t fas[0x8000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		if (fa) memcpy(fas, (const void *)fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		uint8_t *e = Caster();
		uint32_t nb = 0;
		uint8_t *sk = (b14 || def) && e && (e[0] & 2) ? Skeleton((uint32_t)(e + 0x60), &nb) : nullptr;
		if (sk)
		{
			memcpy(g_bones_save[0], sk, nb);
			pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
			if (b14)
			{
				GetEffectSpawnPosition(e, 0x14, 0, g_next_b14);
				g_next_b14_ok = true;
			}
			if (def)
			{
				GetDefaultEffectPosition(e, g_next_def);
				g_next_def_ok = true;
			}
			memcpy(sk, g_bones_save[0], nb);
		}
		if (heights)
		{
			uint8_t *t = Target();
			uint32_t tb = 0;
			uint8_t *tsk = (t[0] & 2) ? Skeleton((uint32_t)(t + 0x60), &tb) : nullptr;
			if (tsk)
			{
				uint8_t wt[12];
				memcpy(wt, t + 0x54, 12);
				memcpy(g_bones_save[0], tsk, tb);
				pose_midpoint(t + 0x60, t + 0x6C, 1, 1);
				MeasureHeight(t, g_next_h);
				g_next_h[0] = (int16_t)(g_next_h[0] - W16(t, 0x1E));
				g_next_h[1] = (int16_t)(g_next_h[1] - W16(t, 0x1E));
				g_next_h_ok = true;
				memcpy(tsk, g_bones_save[0], tb);
				memcpy(t + 0x54, wt, 12);
			}
		}
		var<uint16_t>(0x1D9770C) = w70c;
		var<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	// ---- layouts
	static int16_t LerpFade(int16_t f0, int16_t f1, bool next, bool at_next, int num, int den)
	{
		if (!next) return f0;
		if (at_next) return f1;
		if (f0 != 0 && f1 != 0) return lerp16(f0, f1, num, den);
		return f0; // (the switch to / from no fade on the real frame)
	}
	// the drop's tilt about X of counter c
	static int32_t DropTilt(int32_t c) { return (uint32_t)(c - 0x67) < 0xA ? -0x200 : -0x400; }
	static int16_t DropFade(int32_t c) { const int32_t s = c - 0x67; return (uint32_t)s < 0xA ? 0 : (int16_t)((s - 0xA) << 9); }
	static void HeldPlay(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const int ns = SiteOf(r.layout, nc);
		const bool next = ns != 0 && !Redecoded(ns, nc) && LayoutMore((const prim::Layout *)r.layout);
		if (at_next && !next) return;
		const int pn = next ? num : 0;
		PlayArg arg = r.arg;
		Mat4x3 f = r.frame;
		const int32_t c = r.counter;
		switch (r.site)
		{
		case SITE_RISE:
			arg.scroll = (int16_t)(r.arg.scroll + 0x20 * pn / den);
			if (next && g_next_def_ok)
			{
				f.t[0] = lerp_i(r.frame.t[0], g_next_def[0], num, den);
				f.t[2] = lerp_i(r.frame.t[2], g_next_def[2], num, den);
			}
			break;
		case SITE_FLY:
		case SITE_DROP_A:
		case SITE_DROP_B:
		{
			arg.scroll = (int16_t)(r.arg.scroll + 0x20 * pn / den);
			if (ns == SITE_DROP_A && r.site == SITE_FLY && next) arg.scroll = at_next ? 0 : r.arg.scroll; // (the drop restarts the scroll on the real frame)
			const int32_t a0 = r.site == SITE_FLY ? -0x200 : DropTilt(c);
			const int32_t a1 = next ? (ns == SITE_FLY ? -0x200 : DropTilt(nc)) : a0;
			if (a1 != a0) RotationX(lerp_i(a0, a1, pn, den), &f);
			if (next && g_next_b14_ok) for (int k = 0; k < 3; k++) f.t[k] = lerp_i(r.frame.t[k], g_next_b14[k], num, den);
			const int16_t f1 = ns == SITE_FLY ? 0 : DropFade(nc);
			arg.fade = LerpFade(r.arg.fade, f1, next, at_next, num, den);
			break;
		}
		case SITE_IMPACT:
			if (next)
			{
				const int32_t y1 = mul32(g_dir->height[1] - g_dir->height[0], Squash(nc)) / 0x1000;
				f.t[1] = lerp_i(r.frame.t[1], (int16_t)y1, num, den);
				f.t[0] = lerp_i(r.frame.t[0], W16(Caster(), 0x1C), num, den);
				f.t[2] = lerp_i(r.frame.t[2], W16(Caster(), 0x20) + 0x1F4, num, den);
			}
			break;
		case SITE_PILLAR:
			if (next)
			{
				f.t[1] = lerp_i(r.frame.t[1], NextLift(nc) - 0x3A80, num, den);
				arg.fade = LerpFade(r.arg.fade, PillarFade(nc - 0x29), next, at_next, num, den);
			}
			break;
		}
		arg.morph = (uint32_t)g_held_morph;
		arg.m = f;
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		if (r.site == SITE_PILLAR)
		{
			// camera-facing: the rotation reset, stretched 9 / 8 in y (the director's sequence)
			RotationY(0, &arg.m);
			arg.m.m[0][1] = (int16_t)(arg.m.m[0][1] + (int16_t)sdiv_pow2(arg.m.m[0][1], 3));
			arg.m.m[1][1] = (int16_t)(arg.m.m[1][1] + (int16_t)sdiv_pow2(arg.m.m[1][1], 3));
			arg.m.m[2][1] = (int16_t)(arg.m.m[2][1] + (int16_t)sdiv_pow2(arg.m.m[2][1], 3));
		}
		prim::play_held((prim::Layout *)r.layout, Callback, (int)&arg, pn, den);
	}

	// ---- the twisted model at his bone 0x14
	static void HeldTwist(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		int32_t s1 = 0, a1 = 0;
		bool l1 = false;
		const bool next = nc >= 0 && TwistOf(nc, &s1, &a1, &l1);
		if (at_next && !next) return;
		int16_t p[4] = { r.point[0], r.point[1], r.point[2], 0 };
		int32_t sc = r.a, an = r.b;
		bool last = r.last;
		if (next)
		{
			if (g_next_b14_ok) for (int k = 0; k < 3; k++) p[k] = lerp16(r.point[k], g_next_b14[k], num, den);
			sc = lerp_i(r.a, s1, num, den);
			an = lerp_i(r.b, a1, num, den);
			if (at_next) last = l1;
		}
		DrawTwist(sc, an, last, p, (uint32_t)g_held_twist);
	}

	// ---- rings: their layouts at their fixed frames
	static void HeldRing(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const RingNode *k = (const RingNode *)r.node;
		const bool next = RootNext() && nc != 0x47 && LayoutMore((const prim::Layout *)r.layout);
		if (at_next && !next) return;
		PlayArg arg;
		memset(&arg, 0, sizeof(arg));
		ScaledRotY(k->angle, k->scale, &arg.m);
		arg.m.t[0] = k->pos[0];
		arg.m.t[1] = k->pos[1];
		arg.m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		arg.depth = -0x80;
		arg.fade = 0;
		arg.kind = (const uint8_t *)KIND_Ring;
		prim::play_held((prim::Layout *)r.layout, Callback, (int)&arg, next ? num : 0, den);
	}

	// ---- ghosts: ghost i of a block = slot ((block[3] - i) & 1)
	static const uint8_t *GhostSlot(const uint8_t *g, int i) { return g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x480; }
	static void HeldGhosts(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const uint8_t *now = g_ghosts[r.extra];
		const uint8_t *nx = (nc >= 0 && nc != 9 && GhostsDrawn(nc) && g_dir && QDirector()->head == (TaskNode *)g_dir && g_dir->ghosts[1]) ? g_dir->ghosts : nullptr;
		if (at_next)
		{
			if (!nx) return;
			memcpy(g_held_ghost, nx, 0x988);
		}
		else
		{
			memcpy(g_held_ghost, now, 0x988);
			for (int i = 0; i < now[1]; i++)
			{
				if (!nx || i >= nx[1]) continue; // (a ghost the next tick drops: held)
				uint8_t *dst = (uint8_t *)GhostSlot(g_held_ghost, i);
				const uint8_t *a = GhostSlot(now, i), *b = GhostSlot(nx, i);
				for (int k = 0; k < now[4]; k++) LerpMat((Mat4x3 *)(dst + k * 0x20), *(const Mat4x3 *)(a + k * 0x20), *(const Mat4x3 *)(b + k * 0x20), num, den);
			}
		}
		// his world matrix: from the one the real draw used to the one the frame holds (the next draw's; he moves)
		uint8_t *z = Caster();
		const Mat4x3 keep = *(const Mat4x3 *)(z + 0x40);
		LerpMat((Mat4x3 *)(z + 0x40), r.world, keep, nx ? num : 0, den);
		GhostDraw(z, g_held_ghost);
		*(Mat4x3 *)(z + 0x40) = keep;
	}

	// ---- the target's module draws
	static void HeldEntity(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		int16_t na[3], no[3];
		int32_t nsc[3];
		const int16_t *hh = g_dir ? g_dir->height : nullptr;
		bool next = nc >= 0 && hh && EntityOf(nc, (nc == 0x5D && g_next_h_ok) ? g_next_h : hh, na, no, nsc);
		uint8_t *e = r.ent;
		const Mat4x3 keep = *(const Mat4x3 *)(e + 0x40);
		bool cut = false;
		for (int k = 0; k < 3; k++)
			if (AbsI(keep.t[k] - r.world.t[k]) > 1500) cut = true;
		if (at_next && !next) return;
		const int pn = next && !(cut && !at_next) ? num : 0;
		int16_t ang[3], off[3];
		int32_t sc[3];
		for (int k = 0; k < 3; k++)
		{
			ang[k] = pn ? lerp_angle(r.ang[k], na[k], pn, den) : r.ang[k];
			off[k] = pn ? lerp16(r.off[k], no[k], pn, den) : r.off[k];
			sc[k] = pn ? lerp_i(r.scale[k], nsc[k], pn, den) : r.scale[k];
		}
		// the world matrix: from the one drawn to the one the frame holds (the next draw's)
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
		DrawEntity(e, ang, off, sc);
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
		CaptureStage(r.point, r.box);
		var<int16_t>(0x1D8E038) = view;
		if (sk) memcpy(sk, g_bones_save[0], nb);
	}

	// ---- the white tile (frames 0 / 1: levels 0x1000 / 0)
	static void HeldTile(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const bool next = nc == 0x73 && r.a == 0;
		if (at_next && !next) return;
		const int32_t l0 = r.a == 0 ? 0x1000 : 0, l1 = 0;
		const int32_t level = next ? lerp_i(l0, l1, num, den) : l0;
		PacketCursor() = FullScreenTile(0x80, 0x80, 0x80, level, PacketCursor());
	}

	// ---- particles (the root runs the queues on the next tick while it lives)
	static void HeldSprite(const Rec &r, int num, int den, bool at_next)
	{
		SpriteNode s;
		memcpy(&s, r.node, sizeof(s));
		const int32_t frames = r.model == SEQ_Spark ? 0xC : 8;
		const bool next = RootNext() && (int8_t)(s.frame + 1) < frames;
		if (at_next && !next) return;
		SpriteNode n = s;
		for (int c = 0; c < 3; c++) n.pos[c] = (int16_t)(n.pos[c] + (int16_t)(n.vel[c] << 2));
		if (r.model == SEQ_Ember)
			for (int c = 0; c < 3; c++) n.vel[c] = (int8_t)(n.acc[c] + n.vel[c]);
		n.frame++;
		if (at_next) s = n;
		else if (next) for (int c = 0; c < 3; c++) s.pos[c] = lerp16(s.pos[c], n.pos[c], num, den);
		SpriteDraw(&s, r.model);
	}
	static void HeldRock(const Rec &r, int num, int den, bool at_next)
	{
		RockNode k;
		memcpy(&k, r.node, sizeof(k));
		const bool next = RootNext() && k.life != 1;
		if (at_next && !next) return;
		RockNode n = k;
		RockMove(&n);
		n.life--;
		if (at_next) { RockDraw(&n); return; }
		int16_t pos[3];
		int32_t ax = (int32_t)(int8_t)k.rx << 4, ay = (int32_t)(int8_t)k.ry << 4;
		for (int c = 0; c < 3; c++) pos[c] = next ? lerp16(k.pos[c], n.pos[c], num, den) : k.pos[c];
		if (next)
		{
			ax += ((int32_t)k.spin[0] << 4) * num / den;
			ay += ((int32_t)k.spin[1] << 4) * num / den;
		}
		RockDrawAt(pos, ax, ay, k.scale);
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
				case REC_PLAY: HeldPlay(r, num, den, at_next, nc); break;
				case REC_TWIST: HeldTwist(r, num, den, at_next, nc); break;
				case REC_TILE: HeldTile(r, num, den, at_next, nc); break;
				case REC_GHOSTS: HeldGhosts(r, num, den, at_next, nc); break;
				case REC_ENTITY: HeldEntity(r, num, den, at_next, nc); break;
				case REC_CAPTURE: HeldCapture(r, at_next); break;
				case REC_RING: HeldRing(r, num, den, at_next, nc); break;
				case REC_SPRITE: HeldSprite(r, num, den, at_next); break;
				case REC_ROCK: HeldRock(r, num, den, at_next); break;
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
	// counters on which the director writes the camera words (0 is the director's first tick: never a next one)
	static bool CameraWritten(int32_t c) { return c == 8 || c == 0x10 || c == 0x70 || (uint32_t)(c - 0x16) < 0x5A; }
	// the direct sets and the segment starts: the camera jumps
	static bool CameraCut(int32_t c) { return c == 8 || c == 0x10 || c == 0x16 || c == 0x46 || c == 0x70; }

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (!HeldReady()) return false;
		const int16_t nc = NextCounter(); // the counter the next tick runs
		if (nc < 0 || !CameraWritten(nc)) return false;
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
			const int16_t fov = ViewWord(), roll = CamRoll();
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			DirectorCamera(nc, false);
			for (int k = 0; k < 3; k++) { e1[k] = eye[k]; a1[k] = at[k]; }
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			CamRoll() = roll;
			ViewWord() = fov;
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			var<uint8_t *>(0x1D999C4) = fa;
			if (fa) memcpy(fa, fa_save, sizeof(fa_save));
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

	static void register_mag216_held()
	{
		using namespace beat216;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_DirectorTask);
		mark_held(ORIG_RingTask);
		mark_held(ORIG_RockATask);
		mark_held(ORIG_RockBTask);
		mark_held(ORIG_SparkTask);
		mark_held(ORIG_EmberTask);
		register_module_held(216, HeldReady, HeldFrame);
		register_module_camera(216, HeldCamera);
	}
}
