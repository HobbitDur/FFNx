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

// 30 fps part of mag215_my_final_heaven.cpp (Zell's My Final Heaven): the draw log of the real tick, the held-frame drawing and
// the held-frame camera (included at the end of mag215_my_final_heaven.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// A real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the
// Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the same order, with the vanilla draw
// code, each at its in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every draw
// composes with it, as the real draws do with theirs):
//   Layouts   - through prim::play_held (the exact in-between records of what the player drew). Their frames num / den of the
//               way to the next tick's: the anchor layout at Zell's default position (camera-aligned), the walk / run / jump
//               layouts at Zell (his next position: the walk's 0x100, the run's 0x4CC, the jump's steps), the far / crater
//               layouts fixed, the screen layout's wave growth (its scroll word), the bone 0x16 layouts (0x18..0x73 camera-
//               aligned, the jump's) at his NEXT pose (pose_midpoint at 1: the look-ahead of the animation reader, with the
//               world translation the next tick draws him with: 0x18..0x73 his moved position (that layout runs after the
//               director's tail), the jump's the one the frame holds; his real pose and translation are put back); texture
//               scrolls num / den of the way; fades num / den of the way where both ticks fade (the switch to / from no fade on
//               the real frame). A layout the next phase decodes anew over the same memory is not drawn at 1. Blended / waved
//               vertices go to a private buffer.
//   Rings     - (0x69CB10) their layouts at their fixed frames; none at 1 when the next tick clears / resets the ring queue
//               (0x4C / 0xB6) or the layout ends.
//   Debris    - (draw then update) each live debris num / den of the way to its update (position, gravity / bounce, angles by
//               the spin); one that ends on the next tick is not drawn at 1.
//   Entities  - Zell drawn by the module (0xB6..0xD3) and the targets (0xDE..0x118): angles / offset num / den of the way to
//               the next draw's (the spin's next angle is the record's), the pose half way (pose_midpoint at num / den, body and
//               weapon; the bone matrices put back), the world matrix (+0x40) from the one the real draw used to the one the
//               frame holds (the director / the engine rebuild it from the position: the next draw's). The targets' switch from
//               the spin to standing (0x106) is a cut: held below 1, the next draw at 1.
// A part the next tick will not draw is held up to 1 and not drawn at 1. Zell's / the targets' positions and flags, the screen
// flash and the flash words are the engine's / the real tick's. Nothing here writes game state: the module globals, the packet
// cursors (a private buffer holding the real tick's packets under the replay), the frame arena, the Field_Alloc scratch, the
// shadow-rotation scratch, the screen vertex buffer (0x1D98B3C), the render scratch words, the word 0x1D9770C, the GTE
// registers, the entities' world / bone matrices are put back.
// Camera: HeldCamera runs the director's camera code for the next counter on saved words (pure math: CamLerp 0x657DF0; GTE,
// words, field of view, roll and the Field_Alloc scratch put back) and returns the in-between look-at / eye, holding on cuts (the
// direct sets 0x3D / 0x4B / 0xA3, any jump over 1500). The roll (0x1D977A2) the director ramps (0..0x17, 0x3D..0x4A, 0xDD..0xE0)
// and the field of view are not returned here: the frame gate interpolates them itself between the last two real ticks.

namespace ff8fx
{
namespace heaven215
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x12000;   // one packet arena (magic buffer + 0x10518 / + 0x22518)
	static const uint32_t FARENA = 0x40000;  // frame arena (module-drawn entities, their shadows, the debris)
	static const uint32_t BSS_LO = 0x2508938, BSS_HI = 0x25089AC;
	enum : uint8_t { REC_PLAY = 1, REC_RING, REC_DEBRIS, REC_WORLD, REC_ENTITY };
	enum : int { ENT_SPIN = 1, ENT_DOWN };

	struct Rec
	{
		uint8_t kind;
		int16_t counter;      // the director's counter on the tick (-1: no director)
		int site;             // layout site; entity draw (ENT_*)
		uint8_t *layout;      // layouts / rings: the decoded layout
		bool b;               // layout: the camera-aligned callback (ArgB)
		ArgA a;               // layout: the block as handed over (the frame without the view)
		ArgB bb;
		uint8_t node[0x18];   // ring (+0x0C pos, scale, angle) / debris (0x16): the node as drawn
		uint8_t *ent;         // entity
		int ti;               // entity: its target record (-1: Zell)
		int16_t ang[3], off[3];
		Mat4x3 world;         // entity: its world matrix as drawn
		int extra;
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static bool g_in_held = false;
	static int g_nrec = 0, g_nscratch = 0;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static DirectorNode *g_dir = nullptr;   // the director that ran on the tick
	static int16_t g_dir_counter = -1;      // its counter on the tick
	static Rec g_recs[0x400];
	static uint8_t g_scratch[0x400][SCRATCH];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_morph[0x2000];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];
	// the next-pose predictions of the real tick (made at its first held frame)
	static bool g_next_bone_ok = false, g_next_anchor_ok = false;
	static int16_t g_next_bone[4], g_next_anchor[4];

	static void held_note_root(RootNode *r)
	{
		(void)r;
		g_ported_tick = g_real_tick;
		if (g_in_held) return;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
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

	static void held_note_play(DirectorNode *d, int site, uint8_t *layout, const ArgA *a, const ArgB *b)
	{
		(void)d;
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->site = site;
		r->layout = layout;
		r->b = b != nullptr;
		if (a) r->a = *a;
		if (b) r->bb = *b;
	}

	static void held_note_ring(RingNode *k)
	{
		Rec *r = NewRec(REC_RING);
		if (!r) return;
		r->layout = k->layout;
		memcpy(r->node, k, 0x18);
	}

	static void held_note_debris(const Debris *k)
	{
		Rec *r = NewRec(REC_DEBRIS);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		memcpy(r->node, k, sizeof(Debris));
	}

	static void held_note_world(uint8_t *e)
	{
		Rec *r = NewRec(REC_WORLD);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ent = e;
		r->world = *(const Mat4x3 *)(e + 0x40);
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
		r->site = (uint32_t)(r->counter - 0xDE) < 0x28 ? ENT_SPIN : ENT_DOWN;
		memcpy(r->ang, angles, 6);
		memcpy(r->off, off, 6);
		r->world = *(const Mat4x3 *)(e + 0x40);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the next tick (from the state the real tick left)
	// the counter the director runs with on the next tick (-1: none)
	static int16_t NextCounter()
	{
		const TaskNode *n = QDirector()->head;
		return n ? ((const DirectorNode *)n)->counter : (int16_t)-1;
	}
	// the root runs the ring queue on the next tick
	static bool RootNext() { return QRoot()->head != nullptr; }

	// the layout has a frame left to play (prim::play: frames 0..total - 1, a finished layout draws nothing)
	static bool LayoutMore(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// the jump: Zell's z step on jump tick b (after his draw)
	static int32_t JumpStep(int32_t b)
	{
		if ((uint32_t)b < 0x14) return -0x320;
		if ((uint32_t)b < 0x19) return -0x640;
		if ((uint32_t)b < 0x1D)
		{
			int32_t step = b == 0x19 ? -0x400 : 0;
			int32_t v = shl32(b - 0x19, 12);
			v = (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & 3u)) >> 2;
			return step - (mul32(0xE00 - v, 600) >> 12);
		}
		return 0;
	}
	static int16_t JumpFade(int32_t b)
	{
		if ((uint32_t)b < 8) return (int16_t)((8 - b) << 9);
		if ((uint32_t)b > 0x16) return (int16_t)((b - 0x16) << 9);
		return 0;
	}

	// Zell's z after the next tick's moves (the director moves him before it draws at him and writes his world translation)
	static int32_t NextZellZ(int16_t nc)
	{
		const DirectorNode *d = (const DirectorNode *)TexBase();
		int32_t z = W16(Caster(), 0x20);
		if ((uint32_t)(nc - 0x3E) < 0xE) return (int16_t)(z - 0x100);
		if (nc == 0x4C) return (int16_t)(d->centre[2] + 0x1800 - 0x4CC);
		if ((uint32_t)(nc - 0x4C) < 0xA) return (int16_t)(z - 0x4CC);
		const int32_t b = nc - 0xB6;
		if ((uint32_t)b < 0x1D)
		{
			if (b == 0) z = (int16_t)(d->centre[2] + 0x5AA0);
			return (int16_t)(z + JumpStep(b));
		}
		return z;
	}

	// a layout site drawn on the next tick (its phase goes on: the same decoded layout)
	static bool SiteNext(int site, int16_t nc)
	{
		if (nc < 0) return false;
		switch (site)
		{
		case SITE_ANCHOR: return (uint32_t)(nc - 1) < 0x3D;
		case SITE_BACK: return (uint32_t)(nc - 0x3E) < 0xE;
		case SITE_RUN: return (uint32_t)(nc - 0x4C) < 0x28;
		case SITE_SCREEN: return (uint32_t)(nc - 0x74) < 0x30;
		case SITE_FAR: return (uint32_t)(nc - 0xA4) < 0x12;
		case SITE_JUMP: return (uint32_t)(nc - 0xB6) < 0x1E;
		case SITE_JUMPBONE: return (uint32_t)(nc - 0xB6 - 0xA) < 0x14;
		case SITE_CRATER: return (uint32_t)(nc - 0xDE) < 0x20;
		case SITE_BONE: return (uint32_t)(nc - 0x18) < 0x5C;
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

	// Zell at his next pose (pose_midpoint at 1) and the world translation the next tick draws him with: the bone 0x16 point of
	// the next tick (the bone layouts) and his default effect position (the anchor layout: it follows his pose). His world
	// translation, bone matrices, 0x1D9770C, the Field_Alloc scratch and the GTE registers put back. Once per real tick.
	static void PredictNextPose(int16_t nc)
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_next_bone_ok = g_next_anchor_ok = false;
		bool bone = false, anchor = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind != REC_PLAY || !SiteNext(r.site, nc)) continue;
			if (r.site == SITE_BONE || r.site == SITE_JUMPBONE) bone = true;
			if (r.site == SITE_ANCHOR) anchor = true;
		}
		if (!bone && !anchor) return;
		static uint8_t fas[0x8000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		if (fa) memcpy(fas, (const void *)fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		uint8_t *e = Caster();
		if (!e) return;
		uint32_t nb = 0;
		uint8_t *sk = (e[0] & 2) ? Skeleton((uint32_t)(e + 0x60), &nb) : nullptr;
		// his world translation as the next tick uses it: the bone layout (0x18..0x73) runs after the director's tail has written
		// his moved position; the anchor and the jump's bone layouts run before it (the translation the last tail wrote)
		uint8_t wt[12];
		memcpy(wt, e + 0x54, 12);
		if ((uint32_t)(nc - 0x18) < 0x5C)
		{
			W32(e, 0x54) = W16(e, 0x1C);
			W32(e, 0x58) = W16(e, 0x1E);
			W32(e, 0x5C) = NextZellZ(nc);
		}
		if (sk)
		{
			memcpy(g_bones_save[0], sk, nb);
			pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
			if (bone)
			{
				GetEffectSpawnPosition(e, 0x16, 0, g_next_bone);
				g_next_bone_ok = true;
			}
			if (anchor)
			{
				// (his default effect position follows his pose)
				GetDefaultEffectPosition(e, g_next_anchor);
				g_next_anchor[1] = 0;
				g_next_anchor_ok = true;
			}
			memcpy(sk, g_bones_save[0], nb);
		}
		memcpy(e + 0x54, wt, 12);
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
	static void HeldPlay(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const bool next = SiteNext(r.site, nc) && LayoutMore((const prim::Layout *)r.layout);
		if (at_next && !next) return;
		const int pn = next ? num : 0;
		if (r.b)
		{
			ArgB arg = r.bb;
			const int32_t s = r.counter - (r.site == SITE_ANCHOR ? 1 : 0x18);
			if (r.site == SITE_ANCHOR)
			{
				arg.scroll = (int16_t)(r.bb.scroll + 0x20 * pn / den);
				if (next && g_next_anchor_ok) for (int c = 0; c < 3; c++) arg.pos[c] = lerp16(r.bb.pos[c], g_next_anchor[c], num, den);
			}
			else if (next && g_next_bone_ok) for (int c = 0; c < 3; c++) arg.pos[c] = lerp16(r.bb.pos[c], g_next_bone[c], num, den);
			(void)s;
			arg.morph = (uint32_t)g_held_morph;
			prim::play_held((prim::Layout *)r.layout, CallbackB, (int)&arg, pn, den);
			return;
		}
		ArgA arg = r.a;
		switch (r.site)
		{
		case SITE_BACK:
			arg.scroll = (int16_t)(r.a.scroll + 0x20 * pn / den);
			if (next) arg.m.t[2] = lerp_i(r.a.m.t[2], NextZellZ(nc), num, den);
			break;
		case SITE_RUN:
		{
			arg.scroll = (int16_t)(r.a.scroll + 0x40 * pn / den);
			if (next) arg.m.t[2] = lerp_i(r.a.m.t[2], NextZellZ(nc), num, den);
			break;
		}
		case SITE_SCREEN:
		{
			const int32_t s1 = nc - 0x74;
			const int16_t scroll1 = (int16_t)((int32_t)(((uint32_t)s1 << 12) / 48) + 0x4D);
			const int16_t fade1 = (uint32_t)s1 > 0x2C ? (int16_t)((s1 << 10) + 0x5200) : (int16_t)0;
			if (next) arg.scroll = at_next ? scroll1 : lerp16(r.a.scroll, scroll1, num, den);
			arg.fade = LerpFade(r.a.fade, fade1, next, at_next, num, den);
			break;
		}
		case SITE_JUMP:
		{
			const int32_t b1 = nc - 0xB6;
			arg.scroll = (int16_t)(r.a.scroll + 0x20 * pn / den);
			if (next) arg.m.t[2] = lerp_i(r.a.m.t[2], NextZellZ(nc), num, den);
			arg.fade = LerpFade(r.a.fade, JumpFade(b1), next, at_next, num, den);
			break;
		}
		case SITE_JUMPBONE:
		{
			const int32_t b1 = nc - 0xB6;
			arg.fade = LerpFade(r.a.fade, JumpFade(b1), next, at_next, num, den);
			if (next && g_next_bone_ok) for (int c = 0; c < 3; c++) arg.m.t[c] = lerp_i(r.a.m.t[c], g_next_bone[c], num, den);
			break;
		}
		case SITE_CRATER:
			arg.scroll = (int16_t)(r.a.scroll + 0x20 * pn / den);
			break;
		}
		arg.morph = (uint32_t)g_held_morph;
		if (r.site != SITE_SCREEN) ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		prim::play_held((prim::Layout *)r.layout, CallbackA, (int)&arg, pn, den);
	}

	// ---- rings: their layouts at their fixed frames
	static void HeldRing(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		const RingNode *k = (const RingNode *)r.node;
		const bool next = RootNext() && nc != 0x4C && nc != 0xB6 && LayoutMore((const prim::Layout *)r.layout);
		if (at_next && !next) return;
		ArgA arg;
		memset(&arg, 0, sizeof(arg));
		ScaledRotY(k->angle, k->scale, &arg.m);
		arg.m.t[0] = k->pos[0];
		arg.m.t[1] = k->pos[1];
		arg.m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		arg.depth = -0x80;
		arg.kind = (const uint8_t *)KIND_Ring;
		prim::play_held((prim::Layout *)r.layout, CallbackA, (int)&arg, next ? num : 0, den);
	}

	// ---- debris: num / den of the way to its update
	static void HeldDebris(const Rec &r, int num, int den, bool at_next, int16_t nc, uint8_t *h)
	{
		Debris k;
		memcpy(&k, r.node, sizeof(k));
		const bool next = nc >= 0 && k.life != 1;
		if (at_next && !next) return;
		Debris n = k;
		DebrisStep(&n);
		n.life--;
		if (at_next) k = n;
		else if (next)
		{
			for (int c = 0; c < 3; c++) k.pos[c] = lerp16(k.pos[c], n.pos[c], num, den);
			k.rx = (int16_t)(k.rx + ((int32_t)k.spin[0] << 2) * num / den);
			k.ry = (int16_t)(k.ry + ((int32_t)k.spin[1] << 2) * num / den);
		}
		DebrisDrawOne(&k, h);
	}

	// ---- the module-drawn entities
	static bool EntityNext(const Rec &r, int16_t nc, int16_t *ang, int16_t *off, bool *cut)
	{
		*cut = false;
		if (nc < 0) return false;
		memset(ang, 0, 6);
		memset(off, 0, 6);
		const int32_t t1 = nc - 0xDE;
		if (r.kind == REC_WORLD) return (uint32_t)(nc - 0xB6) < 0x1E;
		if (r.site == ENT_SPIN)
		{
			if ((uint32_t)t1 < 0x28)
			{
				if (r.ti < 0 || !g_dir) return false;
				memcpy(ang, g_dir->recs[r.ti].ang, 6);
				memcpy(off, r.off, 6);
				return true;
			}
			if (t1 == 0x28) { *cut = true; return true; } // (standing: zero angles / offset)
			return false;
		}
		return (uint32_t)t1 < 0x3B;
	}
	static void HeldEntity(const Rec &r, int num, int den, bool at_next, int16_t nc)
	{
		int16_t na[3], no[3];
		bool cut = false;
		const bool next = EntityNext(r, nc, na, no, &cut);
		if (at_next && !next) return;
		uint8_t *e = r.ent;
		const int pn = next ? (cut ? (at_next ? num : 0) : num) : 0;
		int16_t ang[3], off[3];
		for (int c = 0; c < 3; c++)
		{
			ang[c] = pn ? lerp_angle(r.ang[c], na[c], pn, den) : r.ang[c];
			off[c] = pn ? lerp16(r.off[c], no[c], pn, den) : r.off[c];
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
		if (r.kind == REC_WORLD) DrawEntityWorld(e);
		else DrawEntity(e, ang, off);
		if (sk0) memcpy(sk0, g_bones_save[0], b0);
		if (sk1) memcpy(sk1, g_bones_save[1], b1);
		*(Mat4x3 *)(e + 0x40) = keep;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		const int16_t nc = NextCounter();
		PredictNextPose(nc);
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
			uint8_t *dh = nullptr; // (the debris draws share one Field_Alloc block, as 0x69CC90)
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				if (r.kind != REC_DEBRIS || !dh) memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				switch (r.kind)
				{
				case REC_PLAY: HeldPlay(r, num, den, at_next, nc); break;
				case REC_RING: HeldRing(r, num, den, at_next, nc); break;
				case REC_DEBRIS:
					if (!dh) dh = (uint8_t *)FieldAlloc(0x58);
					HeldDebris(r, num, den, at_next, nc, dh);
					if (i + 1 >= g_nrec || g_recs[i + 1].kind != REC_DEBRIS) { FieldFree(0x58); dh = nullptr; }
					break;
				case REC_WORLD:
				case REC_ENTITY: HeldEntity(r, num, den, at_next, nc); break;
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

	// ------------------------------------------------------------------
	// held-frame camera
	// ------------------------------------------------------------------
	// counters on which the director writes the camera words
	static bool CameraWritten(int32_t c) { return (uint32_t)c < 0x18 || c == 0x3D || c == 0x4B || c == 0xA3 || (uint32_t)(c - 0xDD) < 4; }
	// the direct sets: the camera jumps
	static bool CameraCut(int32_t c) { return c == 0 || c == 0x3D || c == 0x4B || c == 0xA3; }
	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

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

	static void register_mag215_held()
	{
		using namespace heaven215;
		mark_held(ORIG_DirectorTask);
		mark_held(ORIG_RingTask);
		register_module_held(215, HeldReady, HeldFrame);
		register_module_camera(215, HeldCamera);
	}
}
