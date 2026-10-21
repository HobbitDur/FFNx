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

// 30 fps layer of Card (effect 182), included at the end of mag182_card.cpp.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw header, and for the pool
// tasks the pool as drawn. A held frame replays the log into a private packet buffer (module cursor 0x23065D0; past half
// way over a copy of the other half of the arena pair, the next tick's), each draw at its exact in-between state (logic
// stays 15 Hz), with the LIVE camera (0x1D97778: the models compose with it, the sprites' TransformCameraByShadowRotation
// reads it, as the real draws do):
//   Rings - angle and scale half way to the values the update left; the fade half way inside one ramp ((0x18 - c) << 8
//     below 8, 0x800, (c - 0x12) << 8 from 0x1A; the switch at 8 holds).
//   Shards - angle, y scale (below counter 0xA) and scale (from 0xE) half way to the post-update node's.
//   CardModel - angle and height half way; the fade (from 0x16) half way while the next tick still draws the model; each
//     after-image at the in-between of the state it shows on this tick and the one it shows on the next tick (the states
//     of the last ticks kept by counter, the model's world matrix rebuilt from them on a private copy of the after-image
//     ring); their number and fades are this tick's (discrete).
//   Sparkles - the entries that advanced on the tick (same slot, frame + 1) half way up (y + vy); flipbook frame and
//     colour (by frame) switch on the real frame; an entry that ended holds its last picture; the tick's spawns appear
//     on the next real tick.
//   Cards - the card sprite half way up (y - vy; its frame = the counter: discrete); its trail entries are static (only
//     the camera moves them), frames discrete, ended entries hold.
// A task that ended on the tick holds its last picture; after a draw-only tick everything holds. The target's fade
// (TargetFade: entity colours / flags), the screen flash and the camera animation are the engine's (15 Hz). Nothing here
// writes game state: the scope puts back the cursor, the GTE registers, the Field_Alloc scratch and the shadow-rotation
// scratch; the after-image ring is a private copy.

namespace ff8fx
{
namespace card182
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_pkt_start = 0;     // the module cursor when the root started the tick's queue
	static uint8_t g_held_packets[ARENA_HALF];

	enum RecKind { REC_NODE, REC_POOL };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node *node;
		Node copy;                 // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated
		uint32_t snap;             // pools: offset of the pool as drawn in g_snap
	};
	static const int REC_MAX = 48;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint8_t g_snap[0x10000];
	static uint32_t g_nsnap = 0;
	// CardModel: its states as drawn by counter (& 7) and the counter each after-image slot holds
	static Node g_hist[8];
	static int16_t g_slot_c[5];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_pkt_start = PacketCursor();
		g_nrec = 0;
		g_nsnap = 0;
	}

	static HRec *NoteRec(int kind, uint32_t fn, const Node *n)
	{
		magc::g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		HRec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.fn = fn;
		r.node = n;
		r.copy = *n;
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_node(uint32_t fn, const Node *n)
	{
		if (fn == ORIG_CardModel)
		{
			g_hist[n->c & 7] = *n;
			g_slot_c[(int32_t)n->c % 5] = n->c;
		}
		NoteRec(REC_NODE, fn, n);
	}

	static uint32_t PoolBase(uint32_t fn, uint32_t *end) { if (fn == ORIG_Sparkles) { *end = SPARK_END; return SPARK_POOL; } *end = TRAIL_END; return TRAIL_POOL; }

	static void held_note_pool(uint32_t fn, const Node *n)
	{
		uint32_t end;
		const uint32_t pool = PoolBase(fn, &end);
		if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
		HRec *r = NoteRec(REC_POOL, fn, n);
		if (!r) return;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
		g_nsnap += end - pool;
	}

	// a held draw starts like the real one: its GTE state and header scratch
	static void Replay(const HRec &r)
	{
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = (uint8_t *)r.hdr;
	}

	// the node still runs the same task on the next tick (it did not end on this one) and advanced
	static bool Alive(const HRec &r) { return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == r.fn && r.node->c == (int16_t)(r.copy.c + 1); }

	static int32_t FadeLerp(bool same, int32_t f0, int32_t f1, bool next, int num, int den) { return next && same ? lerp_i(f0, f1, num, den) : f0; }

	// CardModel's world matrix of a state (rotation (0, angle, 0), translation, uniform scale), as the draw builds it
	static void WorldMatrix(int16_t angle, int16_t x, int16_t y, int16_t z, int16_t s, Mat4x3 *m)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = angle;
		ang[2] = 0;
		ComposeZYXRotationMatrix(ang, m);
		m->t[0] = x;
		m->t[1] = y;
		m->t[2] = z;
		int32_t sv[3];
		sv[0] = s;
		sv[1] = s;
		sv[2] = s;
		Scale3DMatrix(m, sv);
	}

	// a node draw's held inputs: angle, scale, y scale, height, fade; CardModel: each after-image's angle / height
	enum { IN_A, IN_S, IN_SY, IN_Y, IN_FADE, IN_AFA, IN_AFY = IN_AFA + 5, IN_N = IN_AFY + 5 };
	struct NodeIn { int32_t v[IN_N]; };

	static int RingRamp(int32_t c) { return c < 8 ? 1 : c >= 0x1A ? 2 : 3; }

	static void NodeInputs(const HRec &r, int num, int den, NodeIn *in)
	{
		const Node &d = r.copy;
		const Node *now = r.node;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const int32_t c = d.c;
		auto L = [&](int16_t a, int16_t b) { return (int32_t)(next ? magc::lerp16(a, b, num, den) : a); };
		for (int k = 0; k < IN_N; k++) in->v[k] = 0;
		in->v[IN_A] = L(d.w18, now->w18);
		in->v[IN_S] = L(d.w1C, now->w1C);
		in->v[IN_SY] = L(d.w20, now->w20);
		in->v[IN_Y] = L(d.y, now->y);
		switch (r.fn)
		{
		case ORIG_Ring:
			in->v[IN_FADE] = FadeLerp(RingRamp(c) == RingRamp(c + 1), RingFade(c), RingFade(c + 1), next, num, den);
			break;
		case ORIG_CardModel:
		{
			in->v[IN_FADE] = FadeLerp(c >= 0x16 && c + 1 < 0x1E, CardModelFade(c), CardModelFade(c + 1), next, num, den);
			// after-image i shows the state in slot (first - i) on this tick and the one in slot (first' - i) on the next
			const AfterImage *ring = After();
			const int32_t f0 = AfterFirst(c), f1 = AfterFirst(c + 1);
			for (int i = 0; i < 5; i++)
			{
				const int32_t k0 = (f0 - i + 5) % 5, k1 = (f1 - i + 5) % 5;
				const Node &s0 = g_hist[g_slot_c[k0] & 7];
				in->v[IN_AFA + i] = s0.w18;
				in->v[IN_AFY + i] = s0.y;
				if (!next || !ring[k0].used) continue;
				// on the next tick slot (c + 1) % 5 holds its own state (written before the after-images)
				const Node *s1 = k1 == (c + 1) % 5 ? now : ring[k1].used ? &g_hist[g_slot_c[k1] & 7] : nullptr;
				if (!s1) continue;
				in->v[IN_AFA + i] = magc::lerp16(s0.w18, s1->w18, num, den);
				in->v[IN_AFY + i] = magc::lerp16(s0.y, s1->y, num, den);
			}
			break;
		}
		default:
			break;
		}
	}

	static AfterImage g_ring[5];

	static void NodeDraw(const HRec &r, const NodeIn &in)
	{
		const Node &d = r.copy;
		Replay(r);
		switch (r.fn)
		{
		case ORIG_Ring: RingDraw(&d, (int16_t)in.v[IN_A], (int16_t)in.v[IN_S], in.v[IN_FADE]); break;
		case ORIG_Shard: ShardDraw(&d, (int16_t)in.v[IN_A], (int16_t)in.v[IN_S], (int16_t)in.v[IN_SY]); break;
		case ORIG_CardModel:
		{
			// the after-image ring on a private copy: the slots this tick's draw reads, at their held states (the
			// main model's slot c % 5 is written by the draw itself)
			memcpy(g_ring, (const void *)AFTER, sizeof(g_ring));
			const int32_t f0 = AfterFirst(d.c);
			for (int i = 0; i < 5; i++)
			{
				const int32_t k = (f0 - i + 5) % 5;
				if (k == (int32_t)d.c % 5 || !g_ring[k].used) continue;
				Mat4x3 m = g_ring[k].m;
				WorldMatrix((int16_t)in.v[IN_AFA + i], d.x, (int16_t)in.v[IN_AFY + i], d.z, d.w1C, &m);
				g_ring[k].m = m;
			}
			CardModelDraw(d.c, (int16_t)in.v[IN_A], d.x, (int16_t)in.v[IN_Y], d.z, d.w1C, g_ring, in.v[IN_FADE]);
			break;
		}
		default: break;
		}
	}

	static void NodeHeld(const HRec &r, int num, int den)
	{
		NodeIn in;
		NodeInputs(r, num, den, &in);
		NodeDraw(r, in);
	}

	// ---- pools: the entries the task drew, from the pool as drawn ----
	// the entry advanced on the tick (same tag, frame + 1): its post-update state is the next draw's
	static const Particle *NextEntry(const HRec &r, uint32_t o, const Particle *p, bool move)
	{
		if (!move) return nullptr;
		uint32_t end;
		const Particle *now = (const Particle *)(PoolBase(r.fn, &end) + o);
		return now->tag == p->tag && now->frame == (int16_t)(p->frame + 1) ? now : nullptr;
	}

	static void EntryDraw(uint32_t fn, uint8_t *h, const Particle *q)
	{
		if (fn == ORIG_Sparkles) SparkDraw(h, q);
		else TrailDraw(h, q);
	}

	static void PoolHeld(const HRec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		Replay(r);
		uint8_t *h = r.fn == ORIG_Sparkles ? SparkHeader() : CardHeader();
		if (r.fn == ORIG_Card && r.copy.c < 8)
		{
			// the card sprite: half way up (its frame = the counter: this tick's)
			NodeIn in;
			NodeInputs(r, num, den, &in);
			int16_t pos[4];
			memcpy(pos, &r.copy.x, sizeof(pos));
			pos[1] = (int16_t)in.v[IN_Y];
			CardSpriteDraw(h, r.copy.c, pos, r.copy.w1C);
		}
		uint32_t end;
		const uint32_t pool = PoolBase(r.fn, &end);
		for (uint32_t o = 0; o < end - pool; o += 0x18)
		{
			Particle q = *(const Particle *)(g_snap + r.snap + o);
			const bool drawn = r.fn == ORIG_Sparkles ? (q.tag & 1) != 0 : (q.tag & (uint32_t)(int32_t)r.copy.e) != 0;
			if (!drawn) continue;
			const Particle *now = NextEntry(r, o, &q, move);
			if (now && r.fn == ORIG_Sparkles) q.pos[1] = magc::lerp16(q.pos[1], now->pos[1], num, den);
			EntryDraw(r.fn, h, &q);
		}
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		magc::HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start);
		// past half way the draws are the next tick's: under them, what the next tick's half of the arena holds, as the
		// bytes its packets do not write
		if (2 * num > den && g_pkt_start)
		{
			const uint32_t other = g_pkt_start == TexBase() ? TexBase() + ARENA_HALF : TexBase();
			memcpy(g_held_packets, (const void *)other, sizeof(g_held_packets));
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			if (r.kind == REC_NODE) NodeHeld(r, num, den);
			else PoolHeld(r, num, den);
		}
	}
}

	static void register_mag182_held()
	{
		mark_held(card182::ORIG_Sparkles);
		mark_held(card182::ORIG_Ring);
		mark_held(card182::ORIG_Shard);
		mark_held(card182::ORIG_Card);
		mark_held(card182::ORIG_CardModel);
		register_module_held(182, card182::HeldReady, card182::HeldFrame);
	}
}
