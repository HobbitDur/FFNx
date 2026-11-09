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

// 30 fps layer of the generic effect sprites (fx_sprite.cpp), included at its end. The tasks live in the hit-effect
// queue, which belongs to no effect: they are drawn on held frames task by task (ff8fx::register_eq_task, fx_held.h), the
// frame gate calling HeldDraw for each node at the place where the node drew on the last real tick.
//
// The task never moves: its sequence does. A real tick draws frame N of the sequence and the next tick draws frame N + 1
// (not after a paused tick: draw only, the frame stays; not after the last frame: the task ended). The frame is discrete,
// so the held frame rebuilds frame N as a one-frame sequence of its own and draws it through the game's sequence code
// (InitEffectSequenceFromData) from the real draw's GTE state and header scratch:
//   - prim i of frame N that is the same image in frame N + 1 (width, height, gpu code, u0 / v0, clut, flags / tpage
//     equal: same index first, else the nearest unpaired one) moves num / den of the way: centre offset, angle (the short
//     way), scale x / y, intensity;
//   - the other prims cross-fade (CROSS_FADE): frame N's at intensity x (den - num) / den, frame N + 1's at intensity x
//     num / den. Every prim of the table's 32 sequences is semi-transparent, modulated and additive (tpage mode 1), so the
//     two draws add up to exactly the in-between light (a modulated prim's colour scales linearly with its intensity).
//     Only for prims that are not (mode 0 / raw / opaque) and between two frames whose draw-mode flags differ is frame
//     N's picture kept.
// At fraction 0 (and on a paused tick, and on the last frame) the held draw is frame N exactly. The camera: the real tick
// drew with the view of its frame; TransformCameraByShadowRotation loads the view matrix 0x1D97778 again, which on a held
// frame is the frame gate's in-between camera, and the projection registers (screen offset, H = field of view, depth
// scales) are this frame's too (the real draw's other GTE registers are restored): the sprites stay on their world points
// while the camera moves, like the entities around them.
//
// A real tick notes each draw in order (held_note_draw / held_note_drawn): the node as drawn, the GTE state the draw
// started with, the Field_Alloc scratch under its draw header and the packet bytes it wrote. A node can draw twice in one
// tick (it ended and the pool gave it to a new sprite a later request added): the gate asks for the node's draws in their
// order, so the k-th held draw of a node is its k-th noted draw that put primitives in the OT. Nothing here writes game
// state: the packet cursor, the scratch, the shadow scratch and the GTE registers are put back after each draw; no
// rand(), no task.

namespace ff8fx
{
namespace sprite
{
	// held frames cross-fade the prims a frame switch replaces (additive prims only); false: frame N's picture is kept
	static const bool CROSS_FADE = true;

	// a prim of a sequence frame (EffectSprite_EmitFramePrims 0x571DC0)
#pragma pack(push, 1)
	struct SeqPrim
	{
		uint8_t w, intensity, h, code; // half extents x 1/8, intensity (the colour, x the header's RGB / 128), gpu code
		uint8_t u0, v0;
		uint16_t clut;
		int16_t ox, oy;                // centre offset
		int16_t angle;                 // flags 0xC00: rotated and scaled
		uint16_t flags;                // bits 0..8 tpage, 0xC00 rotation + scale, bits 12..15 palette select byte
		int16_t sx, sy;
	};
#pragma pack(pop)
	static_assert(sizeof(SeqPrim) == 20, "sequence prim");
	static const int MAX_PRIMS = 64; // per frame (the table's largest frame has 15)

	struct Rec
	{
		const void *node;
		uint32_t orig;             // task function
		int next_same;             // the node's next draw of the tick (-1: none)
		bool paused;               // the tick ran paused (draw only)
		uint8_t copy[0x24];        // the node as drawn
		uint8_t hdr[HEADER_BYTES]; // scratch under its draw header
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE]; // GTE state the draw started with
		uint32_t pkt_need;         // packet bytes the real draw wrote (0: every prim off screen, nothing in the OT)
		uint32_t pkt_off, pkt_len; // their copy in g_pkt_memo (len 0: not copied)
	};
	static const int REC_MAX = 1024; // draws per real tick (the queue holds 300 nodes)
	static Rec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint32_t g_rec_tick = 0xFFFFFFFFu; // g_eq_tick of the notes
	// node -> its draws of the tick (first / last record), and how many of them the current held frame asked for
	struct Head { const void *node; uint32_t tick; int first, last; uint32_t ask_tick; int ask_num, ask_den, asked; };
	static const int HEAD_SLOTS = 2048; // power of two
	static Head g_head[HEAD_SLOTS];
	static Rec *g_noting = nullptr;
	static uint32_t g_noting_pkt0 = 0;
	static uint8_t g_pkt_memo[0x40000];
	static uint32_t g_pkt_memo_used = 0;
	// held packets: one held frame at a time (the frame of tick g_held_tick, fraction g_held_num / den)
	static uint8_t g_held_pkt[0x40000];
	static uint32_t g_held_used = 0, g_held_tick = 0xFFFFFFFFu;
	static int g_held_num = -1;
	// the in-between frame as a one-frame sequence: header, frame count, frame table {0x10, the real next offset}, frame
	static uint8_t g_seq[0x14 + 2 * MAX_PRIMS * sizeof(SeqPrim)];
	// GTE projection registers (control 24..30: OFX, OFY, H, DQA, DQB, ZSF3, ZSF4; no float mirrors)
	static const uint32_t GTE_PROJ = 0x1CA927C + 24 * 4, GTE_PROJ_SIZE = 7 * 4;

	static inline uint32_t HeadHash(const void *p) { return ((uint32_t)p * 2654435761u) >> 10; }

	static Head *HeadFind(const void *node, bool add)
	{
		for (uint32_t h = HeadHash(node), i = 0; i < (uint32_t)HEAD_SLOTS; i++)
		{
			Head &x = g_head[(h + i) & (HEAD_SLOTS - 1)];
			if (x.tick != g_rec_tick)
			{
				if (!add) return nullptr;
				x.node = node;
				x.tick = g_rec_tick;
				x.first = x.last = -1;
				x.ask_tick = 0xFFFFFFFFu;
				x.ask_num = x.ask_den = x.asked = 0;
				return &x;
			}
			if (x.node == node) return &x;
		}
		return nullptr;
	}

	static void held_note_draw(uint32_t orig, const void *node)
	{
		g_hdr_slot = nullptr;
		g_noting = nullptr;
		if (!g_eq_active) return; // the hit-effect queue's real tick only
		if (g_rec_tick != g_eq_tick)
		{
			g_rec_tick = g_eq_tick;
			g_nrec = 0;
			g_pkt_memo_used = 0;
		}
		if (g_nrec >= REC_MAX) return;
		Head *hd = HeadFind(node, true);
		if (!hd) return;
		const int k = g_nrec++;
		Rec *r = &g_rec[k];
		r->node = node;
		r->orig = orig;
		r->next_same = -1;
		if (hd->last >= 0) g_rec[hd->last].next_same = k;
		else hd->first = k;
		hd->last = k;
		r->paused = Paused();
		memcpy(r->copy, node, sizeof(r->copy));
		memcpy(r->gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r->gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r->pkt_need = r->pkt_off = r->pkt_len = 0;
		g_noting = r;
		g_noting_pkt0 = PacketCursor();
		g_hdr_slot = r->hdr;
	}

	static void held_note_drawn(const void *node)
	{
		Rec *r = g_noting;
		g_noting = nullptr;
		g_hdr_slot = nullptr;
		if (!r || r->node != node) return;
		const uint32_t p0 = g_noting_pkt0, p1 = PacketCursor();
		if (p1 <= p0 || p1 - p0 > sizeof(g_held_pkt) / 4) return;
		r->pkt_need = p1 - p0;
		if (r->pkt_need <= sizeof(g_pkt_memo) - g_pkt_memo_used)
		{
			memcpy(g_pkt_memo + g_pkt_memo_used, (const void *)p0, r->pkt_need);
			r->pkt_off = g_pkt_memo_used;
			r->pkt_len = r->pkt_need;
			g_pkt_memo_used += r->pkt_need;
		}
	}

	// the node's next noted draw that the held frame (g_eq_tick, num / den) has not drawn yet
	static const Rec *RecAsk(const void *node, int num, int den)
	{
		if (g_rec_tick != g_eq_tick) return nullptr;
		Head *hd = HeadFind(node, false);
		if (!hd) return nullptr;
		if (hd->ask_tick != g_eq_tick || hd->ask_num != num || hd->ask_den != den)
		{
			hd->ask_tick = g_eq_tick;
			hd->ask_num = num;
			hd->ask_den = den;
			hd->asked = 0;
		}
		int k = hd->asked++;
		for (int i = hd->first; i >= 0; i = g_rec[i].next_same)
			if (g_rec[i].pkt_need && k-- == 0) return &g_rec[i];
		return nullptr;
	}

	static bool SameImage(const SeqPrim &a, const SeqPrim &b)
	{
		return a.w == b.w && a.h == b.h && a.code == b.code && a.u0 == b.u0 && a.v0 == b.v0 && a.clut == b.clut && a.flags == b.flags;
	}

	// semi-transparent, modulated (not raw), tpage mode 1 / 2 / 3 (B + F, B - F, B + F / 4): its light is linear in its
	// intensity, so two draws at complementary intensities add up to the in-between
	static bool Linear(const SeqPrim &p)
	{
		const int abr = (p.flags >> 5) & 3;
		return (p.code & 2) && !(p.code & 1) && abr != 0;
	}

	static uint8_t Scaled(uint8_t v, int num, int den) { return (uint8_t)((int32_t)v * num / den); }

	// frame `frame` of seq as a one-frame sequence in g_seq (frame index 0), num / den of the way to the next frame when
	// step; false when the frame does not read as one the held draw can rebuild (more than MAX_PRIMS prims)
	static bool BuildFrame(const uint8_t *seq, int frame, bool step, int num, int den)
	{
		if (!seq || frame < 0 || frame > 0x4000) return false;
		const uint16_t *offs = (const uint16_t *)(seq + 0xA);
		const uint32_t ca = *(const uint32_t *)(seq + offs[frame]);
		const int na = (int)(ca & 0x7FFFFFFF);
		if (na > MAX_PRIMS) return false;
		const SeqPrim *A = (const SeqPrim *)(seq + offs[frame] + 4);
		const int16_t next = (int16_t)offs[frame + 1];
		const SeqPrim *B = nullptr;
		int nb = 0;
		bool xfade = false;
		if (step && num > 0 && next >= 0)
		{
			const uint32_t cb = *(const uint32_t *)(seq + (uint16_t)next);
			nb = (int)(cb & 0x7FFFFFFF);
			if (nb <= MAX_PRIMS)
			{
				B = (const SeqPrim *)(seq + (uint16_t)next + 4);
				xfade = CROSS_FADE && (ca >> 31) == (cb >> 31); // same draw-mode flag (the extra draw-mode prims)
			}
			else nb = 0;
		}
		memcpy(g_seq, seq, 10); // header (palette select bytes) and frame count, as the real data
		*(uint16_t *)(g_seq + 0xA) = 0x10;
		*(uint16_t *)(g_seq + 0xC) = offs[frame + 1];
		*(uint16_t *)(g_seq + 0xE) = 0;
		SeqPrim *out = (SeqPrim *)(g_seq + 0x14);
		int n = 0;
		int match[MAX_PRIMS];
		bool used[MAX_PRIMS] = {};
		for (int i = 0; i < na; i++)
		{
			match[i] = -1;
			if (B && i < nb && SameImage(A[i], B[i]))
			{
				match[i] = i;
				used[i] = true;
			}
		}
		for (int i = 0; B && i < na; i++)
		{
			if (match[i] >= 0) continue;
			int best = -1, bd = 0x7FFFFFFF;
			for (int j = 0; j < nb; j++)
			{
				if (used[j] || !SameImage(A[i], B[j])) continue;
				const int32_t dx = (int32_t)A[i].ox - B[j].ox, dy = (int32_t)A[i].oy - B[j].oy;
				const int32_t d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
				if (d < bd) { bd = d; best = j; }
			}
			if (best >= 0)
			{
				match[i] = best;
				used[best] = true;
			}
		}
		for (int i = 0; i < na; i++)
		{
			SeqPrim p = A[i];
			if (B && match[i] >= 0)
			{
				const SeqPrim &b = B[match[i]];
				p.intensity = (uint8_t)lerp_i(p.intensity, b.intensity, num, den);
				p.ox = lerp16(p.ox, b.ox, num, den);
				p.oy = lerp16(p.oy, b.oy, num, den);
				p.angle = (int16_t)(p.angle + ((((int32_t)b.angle - p.angle + 2048) & 4095) - 2048) * num / den);
				p.sx = lerp16(p.sx, b.sx, num, den);
				p.sy = lerp16(p.sy, b.sy, num, den);
			}
			else if (xfade && Linear(p))
			{
				p.intensity = Scaled(p.intensity, den - num, den);
				if (!p.intensity) continue;
			}
			out[n++] = p;
		}
		for (int j = 0; xfade && j < nb; j++)
		{
			if (used[j] || !Linear(B[j])) continue;
			SeqPrim p = B[j];
			p.intensity = Scaled(p.intensity, num, den);
			if (!p.intensity) continue;
			out[n++] = p;
		}
		*(uint32_t *)(g_seq + 0x10) = (ca & 0x80000000u) | (uint32_t)n;
		return true;
	}

	// the node of `orig` drawn at num/den of the way to its next tick (see the top of this file)
	static bool HeldDraw(uint32_t orig, const void *node, int num, int den)
	{
		if (den <= 0 || num < 0 || num > den) return false;
		const Rec *r = RecAsk(node, num, den);
		if (!r || r->orig != orig) return false;
		SpriteNode a;
		memcpy(&a, r->copy, sizeof(a));
		const bool shared = orig == ORIG_SharedSprite;
		// the next tick draws the next frame unless this one ran paused (draw only)
		if (!BuildFrame(shared ? SharedSequence() : a.seq, a.frame, !r->paused, num, den)) return false;
		if (g_held_tick != g_eq_tick || g_held_num != num)
		{
			g_held_tick = g_eq_tick;
			g_held_num = num;
			g_held_used = 0;
		}
		// a prim: 10 words, + 3 with the draw-mode flag
		const uint32_t room = (*(const uint32_t *)(g_seq + 0x10) & 0x7FFFFFFF) * 0x34 + 0x100;
		if (g_held_used + room > sizeof(g_held_pkt)) return false;
		uint8_t *buf = g_held_pkt + g_held_used;
		if (r->pkt_len) memcpy(buf, g_pkt_memo + r->pkt_off, r->pkt_len < room ? r->pkt_len : room);
		uint32_t end;
		{
			HeldScope scope(PacketCursor(), buf, room, 0);
			uint8_t proj[GTE_PROJ_SIZE];
			memcpy(proj, (const void *)GTE_PROJ, sizeof(proj));
			memcpy((void *)GTE_DATA, r->gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r->gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy((void *)GTE_PROJ, proj, sizeof(proj)); // this frame's projection
			g_hdr_slot = (uint8_t *)r->hdr;
			Draw(&a, g_seq, 0, shared);
			end = PacketCursor();
		}
		g_hdr_slot = nullptr;
		const uint32_t used = end > (uint32_t)buf ? end - (uint32_t)buf : 0;
		g_held_used += used < room ? used : room;
		return true;
	}
}

	static void register_fx_sprite_held()
	{
		// tasks that end on a tick are drawn by HeldDraw too (their last frame, with the held frame's camera): no keep_ended
		static const uint32_t DRAWING[] = { sprite::ORIG_Sprite, sprite::ORIG_SharedSprite };
		for (uint32_t f : DRAWING)
		{
			register_eq_task(f, sprite::HeldDraw);
			mark_held(f);
		}
	}
}
