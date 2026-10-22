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

// 30 fps part of mag275_electrocute_quistis.cpp (Quistis' Electrocute: Electric Discharge 126 adapted): the draw log of the
// real tick and the held-frame drawing (included at the end of mag275_electrocute_quistis.cpp; shared pieces in fx_held.h /
// mag_common_held.h).
//
// A real tick records every draw in order with the GTE registers it started from; a held frame at tick + num / den replays
// them in the same order on private copies, with the vanilla draw code:
//   Arc      - (updates THEN draws) the chain's points as drawn (position, velocity, fade, fade step). The next tick's
//              chain without its random growth is exact: the points move by their velocity, the fade pass is deterministic,
//              the faded tail the tick freed is gone. Every point num / den of the way (position and fade); at fraction 1
//              the freed tail is left out and a chain that ended draws nothing. New points, forks and sparks (random)
//              appear on the next tick.
//   Crawler  - (updates THEN draws) every point sits on a vertex of the CASTER's model, placed through her bone world
//              matrices at every draw: Quistis animates, so the next tick's place of each point is predicted with her bone
//              matrices at her NEXT pose (pose_midpoint at 1: the look-ahead of the animation reader, then the world
//              matrices as the root builds them before the crawler queue, 0x5095B0) plus the point's drifted offsets; a
//              held frame draws the world position num / den of the way between the drawn one and the predicted one (and
//              the fade half way); at fraction 1 the freed tail is left out, a crawler whose next growth step ends its
//              script with 3 points or fewer (it dies) draws nothing. The ribbon's direction local starts as on the real
//              draw (a stack residue, see CrawlerTask). New points (the script's growth) appear on the next tick.
//   Spark    - (draws THEN updates) the sprite sequence frame of the real tick up to fraction 1, the next one at 1 (none
//              after the last); fixed place. The target tint (engine entity colour) is a real-tick effect only.
//   Glow     - the prim-model layout through prim::play_held (exact in-between records); its anchor is the caster's
//              default effect position (her bone matrices: it follows her animation), num / den of the way to the one at
//              her next pose (predicted with the crawlers'); the Field_Alloc scratch of the real draw. Blended vertex
//              frames go to a private buffer.
// Every draw reads the battle view 0x1D97778 itself (the root sets the GTE camera from it before the arc queue, the crawler
// and layout draws and the sparks' shadow-rotation camera read it), so a held frame draws with the frame's own (in-between)
// view; the module keeps no copy of it. At a restart of her looped cast animation pose_midpoint holds the completed
// animation: the predicted places are then its last pose's (by design, as the battle model's own held frames).
// No task has a draw-only mode. The packets go to a private buffer that starts with the real tick's packets; the module
// globals, the Field_Alloc scratch (the pose prediction's bone rebuilds included), the caster's bone matrices, the word
// GetEffectSpawnPosition writes (0x1D9770C), the shadow / render temporaries and the GTE registers are put back.

namespace ff8fx
{
namespace elec275
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;   // module packet arena per tick
	static const uint32_t BSS_LO = 0x2508348, BSS_HI = 0x2508410;
	enum : uint8_t { REC_ARC = 1, REC_CRAWLER = 2, REC_SPARK = 3, REC_LAYOUT = 4 };

	struct HeldPoint
	{
		Point p;           // as drawn
		int16_t w[4];      // crawler: the vertex through its bone's world matrix (as drawn)
		int16_t wn[4];     // crawler: the same at the caster's next pose (predicted)
	};
	struct Rec
	{
		uint8_t kind;
		bool drawn;        // arc / crawler: the ribbon was drawn (2 points or more)
		int16_t style;     // arc texture row
		const void *node;  // the task node
		uint32_t dir0;     // crawler: the draw's direction local as it started
		const int16_t *script; // crawler: its script (null = finished)
		int16_t steps;     // crawler: script steps per tick
		uint8_t *model;    // crawler: the caster's model block
		int first, n;      // points in g_points
		int scratch;       // index in g_scratch (spark / layout), -1 = none
		SparkNode spark;
		prim::Layout *layout;
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npoints = 0, g_nscratch = 0;
	static int g_crawl_rec = -1, g_crawl_pt = 0; // crawler being drawn (world positions)
	static uint32_t g_packets = 0;               // module packet cursor at the tick's first draw
	static Rec g_recs[0x100];
	static HeldPoint g_points[0x400];
	static uint8_t g_scratch[0x40][SCRATCH];
	static Point g_copy[0x200];
	static int16_t g_world[0x200][4];            // crawler: the copies' world positions
	static int16_t g_anchor_next[4];             // glow layouts: the anchor at the caster's next pose
	static bool g_anchor_ok = false;
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

	static Rec *NewRec(uint8_t kind, const void *node, bool scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (scratch && g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.drawn = false;
		r.script = nullptr;
		r.model = nullptr;
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
			memset(h.wn, 0, sizeof(h.wn));
			r->n++;
		}
		r->drawn = count >= 2;
		return true;
	}

	static void held_note_arc(const ArcNode *a)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_ARC, a, false);
		if (!r) return;
		r->style = a->style;
		NotePoints(r, a->tail, a->count);
	}

	static void held_note_crawler(const CrawlerNode *c, uint32_t dir0, uint32_t slot0)
	{
		(void)slot0; // (= dir0 for a finished crawler: its task changes no word before the draw)
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_CRAWLER, c, false);
		if (!r) return;
		r->dir0 = dir0;
		r->script = c->script;
		r->steps = c->steps;
		r->model = c->model;
		if (!NotePoints(r, c->tail, c->count)) return;
		g_crawl_rec = g_nrec - 1;
		g_crawl_pt = 0;
	}

	static void held_note_crawler_point(const int16_t *w)
	{
		if (g_crawl_rec < 0 || g_log_tick != g_real_tick) return;
		const Rec &r = g_recs[g_crawl_rec];
		if (g_crawl_pt < r.n)
		{
			HeldPoint &h = g_points[r.first + g_crawl_pt++];
			memcpy(h.w, w, 8);
			memcpy(h.wn, w, 8);
		}
	}

	static void held_note_spark(const SparkNode *s)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_SPARK, s, true);
		if (r) r->spark = *s;
	}

	static void held_note_play(prim::Layout *layout, const PrimArg *arg)
	{
		g_crawl_rec = -1;
		Rec *r = NewRec(REC_LAYOUT, layout, true);
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

	// crawler: its next growth steps (the script words, as CrawlerTask reads them) end it (a script end with 3 points or
	// fewer kills the crawler: nothing drawn on the next tick)
	static bool DiesNext(const Rec &r, int count)
	{
		const int16_t *s = r.script;
		if (!s) return false;
		int32_t k = r.steps;
		do
		{
			if (!s) continue;
			int16_t bone = *s++;
			while (bone < 0)
			{
				s += -1 - bone;
				bone = *s++;
			}
			s++; // vertex
			const int16_t *peek = s;
			int16_t w = *peek;
			if (w < 0)
			{
				peek -= w;
				w = *peek;
			}
			if (w == 0x7FFF)
			{
				if (count <= 3) return true;
				s = nullptr;
			}
			count++;
		} while (--k != 0);
		return false;
	}

	// the caster's next pose (once per real tick, at its first held frame): pose_midpoint at 1 (the look-ahead of the
	// animation reader) builds her bone matrices at the next tick's pose; the glow anchor (GetDefaultEffectPosition, as the
	// layout task takes it) and the crawlers' vertices through the world matrices (0x5095B0 as the root builds them) are
	// taken there; then her bone matrices, the word 0x1D9770C, the Field_Alloc scratch and the GTE registers are put back.
	// No pose (the caster gone): everything holds.
	static void PredictNextPose()
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_anchor_ok = false;
		bool crawlers = false, layouts = false;
		for (int i = 0; i < g_nrec; i++)
		{
			if (g_recs[i].kind == REC_CRAWLER) crawlers = true;
			if (g_recs[i].kind == REC_LAYOUT) layouts = true;
		}
		uint8_t *e = Caster();
		if ((!crawlers && !layouts) || !e || !(e[0] & 2) || !*(uint8_t **)(e + 0x64)) return;
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
		if (layouts)
		{
			GetDefaultEffectPosition(e, g_anchor_next);
			g_anchor_next[1] = (int16_t)(g_anchor_next[1] + 0xC8);
			g_anchor_ok = true;
		}
		if (crawlers)
		{
			ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				if (r.kind != REC_CRAWLER || !r.model) continue;
				for (int k = 0; k < r.n; k++)
				{
					HeldPoint &h = g_points[r.first + k];
					CrawlerPointWorld(r.model, &h.p, h.wn);
				}
			}
		}
		memcpy(block, bones, nb);
		var<uint16_t>(0x1D9770C) = w70c;
		var<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	// private copies of the points [from, n) at tick + num / den, linked in order; crawlers: their world positions
	// (vertex through the bone matrix + offsets) num / den of the way to the next tick's in g_world
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
			if (fade > 0)
			{
				fade = (int16_t)(fade + h.p.step);
				if (fade <= 0) fade = 0;
			}
			if (r.kind == REC_ARC)
			{
				// next tick: moved by the velocity
				for (int k = 0; k < 3; k++) c.a[k] = lerp16(h.p.a[k], (int16_t)(h.p.a[k] + h.p.c[k]), num, den);
			}
			else
			{
				// next tick: the offsets drift, the vertex sits where the next pose puts it
				const int16_t off[3] = { h.p.a[2], h.p.b, h.p.c[0] };
				for (int k = 0; k < 3; k++)
				{
					const int16_t now = (int16_t)(h.w[k] + off[k]);
					const int16_t next = (int16_t)(h.wn[k] + (int16_t)(off[k] + CrawlDrift(&h.p, k)));
					g_world[i][k] = lerp16(now, next, num, den);
				}
				g_world[i][3] = h.w[3];
				// (the copy's offsets are in g_world)
				c.a[2] = 0;
				c.b = 0;
				c.c[0] = 0;
			}
			c.fade = lerp16(h.p.fade, fade, num, den);
			c.next = i + 1 < n ? &g_copy[i + 1] : nullptr;
		}
		return n;
	}

	static void HeldFrame(int num, int den)
	{
		const bool at_next = num >= den;
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8];
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			PredictNextPose();
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				if (r.kind == REC_ARC || r.kind == REC_CRAWLER)
				{
					if (!r.drawn) continue;
					int from = 0;
					if (at_next)
					{
						from = KeptFrom(r);
						if (from < 0) continue;
						if (r.kind == REC_CRAWLER && DiesNext(r, r.n - from)) continue;
					}
					const int n = CopyChain(r, from, num, den);
					if (n < 2) continue;
					if (r.kind == REC_ARC)
					{
						GteSetRotMatrix(&Camera());
						GteSetTransVector(&Camera());
						ArcProject(g_copy);
						ArcRibbon(g_copy, n, r.style);
					}
					else
					{
						for (int k = 0; k < n; k++) CrawlerPointProject(&g_copy[k], g_world[k]);
						CrawlerRibbon(g_copy, n, r.dir0);
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
					if (g_anchor_ok)
						for (int k = 0; k < 3; k++) arg.pos[k] = lerp16(r.arg.pos[k], g_anchor_next[k], num, den);
					arg.morph = (uint32_t)g_held_morph;
					prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				}
			}
		}
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}
}

	static void register_mag275_held()
	{
		mark_held(elec275::ORIG_ArcTask);
		mark_held(elec275::ORIG_CrawlerTask);
		mark_held(elec275::ORIG_SparkTask);
		mark_held(elec275::ORIG_LayoutTask);
		register_module_held(275, elec275::HeldReady, elec275::HeldFrame);
	}
}
