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

// 30 fps part of mag126_electric_discharge.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag126_electric_discharge.cpp; shared pieces in fx_held.h /
// mag_common_held.h).
//
// A real tick records every draw in order with the GTE registers it started from:
//   Arc      - (updates THEN draws) the chain's points as drawn (position, velocity, fade, fade
//              step / hold). The next tick's chain without its random growth is exact: points
//              move by their velocity, the fade pass is deterministic, the faded tail the tick
//              freed is gone. A held frame draws the chain at tick + num / den: every point
//              num / den of the way (position and fade), projected and ribboned by the vanilla
//              code on private copies. New points, forks and sparks (random) appear on the next
//              tick; at fraction 1 the freed tail is left out and a chain that ended draws nothing.
//   Crawler  - (updates THEN draws) the same with the points' model positions (the vertex through
//              its bone's matrix, as drawn: the caster's pose holds) plus their drifting offsets.
//   Spark    - (draws THEN updates) the sprite sequence frame of the real tick up to half way, the
//              next one at fraction 1 (none after the last); fixed place. The target tint (engine
//              entity colour) is a real-tick effect only.
//   Glow     - the prim-model layout through prim::play_held (exact in-between records), the
//              anchor and the Field_Alloc scratch of the real draw.
// The arcs and crawlers are projected with the current camera (the real tick sets it before the
// arc queue). Blended vertex frames of the layout (not used by its data) go to a private buffer.
// No task has a draw-only mode.

namespace ff8fx
{
namespace disch126
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;   // module packet arena per tick
	enum : uint8_t { REC_ARC = 1, REC_CRAWLER = 2, REC_SPARK = 3, REC_LAYOUT = 4 };

	struct HeldPoint
	{
		Point p;           // as drawn
		int16_t w[4];      // crawler: the vertex through its bone's matrix
	};
	struct Rec
	{
		uint8_t kind;
		bool drawn;        // arc / crawler: the ribbon was drawn (2 points or more)
		int16_t style;     // arc texture row
		uint32_t dir0;     // crawler: the draw's direction local as it started
		uint32_t slot0;    // crawler: that stack slot when the task started
		bool finished;     // crawler: its script ended (it no longer grows)
		int first, n;      // points in g_points
		int scratch;       // index in g_scratch (spark / layout), -1 = none
		SparkNode spark;
		prim::Layout *layout;
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npoints = 0, g_nscratch = 0;
	static int g_crawl_rec = -1, g_crawl_pt = 0; // crawler being drawn (world positions)
	static uint32_t g_packets = 0;               // module packet cursor at the tick's first draw
	static Rec g_recs[0x100];
	static HeldPoint g_points[0x400];
	static uint8_t g_scratch[0x40][SCRATCH];
	static Point g_copy[0x200];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_morph[0x2000];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npoints = 0;
		g_nscratch = 0;
		g_crawl_rec = -1;
		g_packets = 0;
	}

	static Rec *NewRec(uint8_t kind, bool scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (scratch && g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.drawn = false;
		r.first = g_npoints;
		r.n = 0;
		r.scratch = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		if (scratch)
		{
			r.scratch = g_nscratch++;
			memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
		}
		return &r;
	}

	// the chain as drawn (false: no room, the record is dropped)
	static bool NotePoints(Rec *r, const Point *q, int16_t count)
	{
		for (; q; q = q->next)
		{
			if (g_npoints >= (int)(sizeof(g_points) / sizeof(g_points[0])))
			{
				g_nrec--;
				return false;
			}
			HeldPoint &h = g_points[g_npoints++];
			h.p = *q;
			memset(h.w, 0, sizeof(h.w));
			r->n++;
		}
		r->drawn = count >= 2;
		return true;
	}

	static void held_note_arc(const ArcNode *a)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_ARC, false);
		if (!r) return;
		r->style = a->style;
		NotePoints(r, a->tail, a->count);
	}

	static void held_note_crawler(const CrawlerNode *c, uint32_t dir0, uint32_t slot0)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_CRAWLER, false);
		if (!r) return;
		r->dir0 = dir0;
		r->slot0 = slot0;
		r->finished = c->script == nullptr;
		if (!NotePoints(r, c->tail, c->count)) return;
		g_crawl_rec = g_nrec - 1;
		g_crawl_pt = 0;
	}

	static void held_note_crawler_point(const int16_t *w)
	{
		if (g_crawl_rec < 0 || g_log_tick != g_real_tick) return;
		const Rec &r = g_recs[g_crawl_rec];
		if (g_crawl_pt < r.n) memcpy(g_points[r.first + g_crawl_pt++].w, w, 8);
	}

	static void held_note_spark(const SparkNode *s)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_SPARK, true);
		if (r) r->spark = *s;
	}

	static void held_note_play(prim::Layout *layout, const PrimArg *arg)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_LAYOUT, true);
		if (!r) return;
		r->layout = layout;
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the points the tick kept (the free loop after the draw: faded tail pairs: 0 for arcs,
	// 0x10 or less for crawlers), -1 = the task ended
	static bool Faded(const Rec &r, const Point &p) { return r.kind == REC_ARC ? p.fade == 0 : p.fade <= 0x10; }
	static int KeptFrom(const Rec &r)
	{
		const HeldPoint *p = &g_points[r.first];
		int i = 0, count = r.n;
		while (i + 1 < r.n && Faded(r, p[i].p) && Faded(r, p[i + 1].p))
		{
			i++;
			if (--count < 2) return -1;
		}
		return i;
	}

	// private copies of the points [from, n) at tick + num / den, linked in order
	static int CopyChain(const Rec &r, int from, int num, int den)
	{
		const int n = r.n - from;
		if (n > (int)(sizeof(g_copy) / sizeof(g_copy[0]))) return 0;
		for (int i = 0; i < n; i++)
		{
			const HeldPoint &h = g_points[r.first + from + i];
			Point &c = g_copy[i];
			c = h.p;
			int16_t fade = h.p.fade;
			if (r.kind == REC_ARC)
			{
				// next tick: moved by the velocity, then the fade pass
				for (int k = 0; k < 3; k++) c.a[k] = lerp16(h.p.a[k], (int16_t)(h.p.a[k] + h.p.c[k]), num, den);
				if (fade > 0)
				{
					fade = (int16_t)(fade + h.p.step);
					if (fade <= 0) fade = 0;
				}
			}
			else
			{
				// next tick: the offsets drift, then the fade
				c.a[2] = lerp16(h.p.a[2], (int16_t)(h.p.a[2] + CrawlDrift(&h.p, 0)), num, den);
				c.b = lerp16(h.p.b, (int16_t)(h.p.b + CrawlDrift(&h.p, 1)), num, den);
				c.c[0] = lerp16(h.p.c[0], (int16_t)(h.p.c[0] + CrawlDrift(&h.p, 2)), num, den);
				if (fade > 0)
				{
					fade = (int16_t)(fade + h.p.step);
					if (fade <= 0) fade = 0;
				}
			}
			c.fade = lerp16(h.p.fade, fade, num, den);
			c.next = i + 1 < n ? &g_copy[i + 1] : nullptr;
		}
		return n;
	}

	static void HeldFrame(int num, int den)
	{
		const bool at_next = num >= den;
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			if (r.kind == REC_ARC || r.kind == REC_CRAWLER)
			{
				// crawler draw's direction local (a stack word, see CrawlerTask): at fraction 1 a
				// finished crawler's next draw starts with the task's entry stack word (its value on
				// this tick is used); a crawler still growing is new on the next tick anyway
				const uint32_t dir0 = r.kind == REC_CRAWLER && at_next && r.finished ? r.slot0 : r.dir0;
				// an arc of one point projects it without a ribbon (the GTE FIFO the next draw starts from)
				if (!r.drawn && r.kind != REC_ARC) continue;
				int from = 0;
				if (at_next)
				{
					from = KeptFrom(r);
					if (from < 0) continue;
				}
				const int n = CopyChain(r, from, num, den);
				if (r.kind == REC_ARC && n >= 1)
				{
					GteSetRotMatrix(&Camera());
					GteSetTransVector(&Camera());
					ArcProject(g_copy);
					if (r.drawn && n >= 2) ArcRibbon(g_copy, n, r.style);
					continue;
				}
				if (n < 2) continue;
				if (r.kind == REC_ARC)
				{
				}
				else
				{
					for (int k = 0; k < n; k++)
					{
						int16_t w[4];
						memcpy(w, g_points[r.first + from + k].w, 8);
						CrawlerPointProject(&g_copy[k], w);
					}
					CrawlerRibbon(g_copy, n, dir0);
				}
			}
			else if (r.kind == REC_SPARK)
			{
				SparkNode s = r.spark;
				if (at_next)
				{
					s.frame++;
					if (s.frame >= 8) continue;
				}
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				SparkDraw(&s);
			}
			else if (r.kind == REC_LAYOUT)
			{
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
			}
		}
	}
}

	static void register_mag126_held()
	{
		mark_held(disch126::ORIG_ArcTask);
		mark_held(disch126::ORIG_CrawlerTask);
		mark_held(disch126::ORIG_SparkTask);
		mark_held(disch126::ORIG_LayoutTask);
		register_module_held(126, disch126::HeldReady, disch126::HeldFrame);
	}
}
