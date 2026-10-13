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

// 30 fps part of mag132_dissolving_acid.cpp (Dissolving Acid, effect 132). Exact vanilla shapes: the
// logic stays at 15 Hz, a held frame draws the state num/den of the way from the last real tick to
// the next one.
//
// A real tick records its draws in order (the GTE state and the Field_Alloc scratch each started
// from); a held frame replays them in the same order on private packets:
//   Stream    - (updates THEN draws) every point of the chain as drawn at the Bezier weight num/den of
//               the way to the weight the next tick gives it (the tick's weight loop re-run on the
//               weight table for the next phase: head weight, -0xC0 per point, clamp, a head past
//               0x1080 freed); projected and ribboned by the vanilla code on private copies. The next
//               tick's new point (growth) appears on that tick; at fraction 1 the head the next tick
//               frees is left out; a chain the next tick ends holds its picture (nothing at 1). A
//               tick that could not take a new point (pool full: never with 5 chains of 9 points)
//               holds its chain.
//   Sequences - the stream's sprite (counters 0..7, at the start) and the splash sprites: fixed
//               positions, flipbook frame N held.
//   Layouts   - (splash / puddle, draw THEN update) through prim::play_held (every object's exact
//               in-between record), the anchor fixed; a splash's last draw (counter 15) holds.
//               Blended vertex frames go to a private buffer.
//   Drops     - (draw THEN update) half way along their velocity; the flipbook frame holds; a drop's
//               last draw (frame 7) holds. The drop list header is rebuilt as the tick built it.
// Spawns (streams, drops, layouts: CRT rand) stay at 15 Hz. No task has a draw-only mode. The module
// packet arena alternates per tick: the held packets start where the tick's first draw started.
// The camera is the frame gate's. The GTE, the Field_Alloc scratch, the drop header pointer and the
// engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace acid132
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_SIZE = 0x4BDC - MORPH; // up to the first packet arena
	enum : uint32_t { REC_PLAY = 1, REC_SEQ = 2, REC_STREAM = 3, REC_DROPLIST = 4, REC_DROP = 5 };

	struct Rec
	{
		uint32_t kind;
		LayoutNode *layout;       // play: the live node (prim::play_held knows what it drew)
		int16_t lpos[4];          // play: the anchor
		bool next_plays;          // play: the next tick plays the layout again (else its picture holds)
		bool stream_seq;          // sequence: the stream's (else a splash's)
		StreamNode stream;        // stream: the node after its update; sequence: the stream as drawn
		int first, n;             // stream: its points (as computed) in g_points
		bool grew;                // stream: the tick moved the chain (it could take its new point)
		LayoutNode splash;        // sequence: the splash as drawn (counter, anchor; the layout not copied)
		DropNode drop;            // drop: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npoints = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[0x10 * 2 + 0x10 * 2 + 1 + 0x40];
	static Point g_points[0x200];           // stream points as computed on the tick
	static Point g_copy[0x80];              // held chain
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[MORPH_SIZE];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npoints = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_layout(const LayoutNode *s, const PrimArg *arg, bool splash)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (LayoutNode *)s;
		memcpy(r->lpos, arg->pos, sizeof(r->lpos));
		r->next_plays = splash ? s->counter + 1 < 0x10 : true; // (a splash ends after its draw at 15)
	}

	static void held_note_sequence(const void *node, bool stream)
	{
		Rec *r = NewRec(REC_SEQ);
		if (!r) return;
		r->stream_seq = stream;
		if (stream) r->stream = *(const StreamNode *)node;
		else memcpy(&r->splash, node, 0x18);
	}

	static void held_note_stream(const StreamNode *s, bool grew)
	{
		Rec *r = NewRec(REC_STREAM);
		if (!r) return;
		r->stream = *s;
		r->grew = grew;
		r->first = g_npoints;
		r->n = 0;
		for (const Point *q = s->head; q; q = q->next)
		{
			if (g_npoints >= (int)(sizeof(g_points) / sizeof(g_points[0])) || r->n >= (int)(sizeof(g_copy) / sizeof(g_copy[0])))
			{
				g_nrec--;
				return;
			}
			g_points[g_npoints++] = *q;
			r->n++;
		}
	}

	static void held_note_drop_list()
	{
		NewRec(REC_DROPLIST);
	}

	static void held_note_drop(const DropNode *d)
	{
		Rec *r = NewRec(REC_DROP);
		if (r) r->drop = *d;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// Held frame
	// ------------------------------------------------------------------

	// the stream task's weight loop over n points (head first) from the table weight t0, the chain
	// holding count points: every point's weight; returns the heads it frees (-1: fewer than 2
	// points are left and the task ends there)
	static int SimWeights(int32_t t0, int n, int32_t count, int32_t *w)
	{
		int32_t x = t0;
		int freed = 0;
		for (int j = 0; j < n; j++)
		{
			x -= 0xC0;
			if (x > 0x1080 && j == freed)
			{
				freed++;
				if (--count < 2) return -1;
			}
			if (x >= 0x1080) x = 0x1000;
			w[j] = x;
		}
		return freed;
	}

	static void StreamHeld(const Rec &r, int num, int den)
	{
		const StreamNode &s = r.stream;
		const int m = r.n;
		if (m < 1) return;
		int32_t w0[0x81], w1[0x80];
		int skip = 0;
		bool hold = !r.grew;
		if (!hold)
		{
			// the weights the tick gave the points it kept (a head it freed took the first step)
			const int32_t t0 = WEIGHTS[(int32_t)s.phase / 256];
			if (t0 - 0xC0 > 0x1080) SimWeights(t0, m + 1, 0x7FFF, w0);
			else
			{
				w0[0] = 0;
				SimWeights(t0, m, 0x7FFF, w0 + 1);
			}
			// the next tick's: next phase, its new point taken first (count + 1 while points are added)
			const int freed = SimWeights(WEIGHTS[(int32_t)(int16_t)(s.phase + 0x100) / 256], m, (int32_t)s.count + (s.add > 0 ? 1 : 0), w1);
			if (freed < 0)
			{
				// the chain ends on the next tick: its last picture holds
				if (num >= den) return;
				hold = true;
			}
			else if (num >= den) skip = freed;
		}
		const int n = m - skip;
		for (int j = 0; j < n; j++)
		{
			Point &c = g_copy[j];
			c = g_points[r.first + skip + j];
			if (!hold) BezierPoint(&s, lerp_i(w0[1 + skip + j], w1[skip + j], num, den), c.pos);
			c.next = j + 1 < n ? &g_copy[j + 1] : nullptr;
		}
		const uint32_t slot = ProjectChain(g_copy);
		if (n >= 2) Ribbon(g_copy, n, slot);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// engine words the sprite draws touch, the module's drop header pointer
		uint8_t *const drop_header = DropHeader();
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		uint8_t *h = nullptr; // the drop list header of the held frame
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PLAY:
			{
				PrimArg arg;
				memcpy(arg.pos, r.lpos, sizeof(arg.pos));
				memcpy(g_held_morph, (const void *)(TexBase() + MORPH), sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)r.layout->layout, PartCallback, (int)&arg, r.next_plays ? num : 0, den);
				break;
			}
			case REC_SEQ:
				if (r.stream_seq) StreamSequence(&r.stream);
				else SplashSequences(&r.splash);
				break;
			case REC_STREAM:
				StreamHeld(r, num, den);
				break;
			case REC_DROPLIST:
				if (!h) h = DropListHeader();
				break;
			case REC_DROP:
				if (h)
				{
					DropNode d = r.drop;
					// the drop moves by its velocity (its last frame holds)
					if (d.frame + 1 < 8)
						for (int c = 0; c < 3; c++) d.pos[c] = lerp16(d.pos[c], (int16_t)(d.pos[c] + d.vel[c]), num, den);
					DropDraw(h, &d);
				}
				break;
			}
		}
		if (h) FieldFree(0xB4);
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
		DropHeader() = drop_header;
	}
}

	static void register_mag132_held()
	{
		mark_held(acid132::ORIG_StreamTask);
		mark_held(acid132::ORIG_DropTask);
		mark_held(acid132::ORIG_SplashTask);
		mark_held(acid132::ORIG_PuddleTask);
		register_module_held(132, acid132::HeldReady, acid132::HeldFrame);
	}
}
