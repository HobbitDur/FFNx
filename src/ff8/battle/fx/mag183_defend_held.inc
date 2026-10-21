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

// 30 fps layer of Defend (effect 183), included at the end of mag183_defend.cpp.
//
// Both shields draw THEN update. A real tick records each shield draw in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with and the Field_Alloc scratch under its draw header. A held frame
// replays the log into a private packet buffer (module cursor 0x230421C; past half way over a copy of the other arena of
// the pair, the next tick's), each shield at its exact in-between state, with the LIVE camera (0x1D97778: the draw composes
// its model matrix with it, as the real draw does):
//   ShieldA - the scale half way to the value the update left (scale - step), the v scroll half way to the next tick's
//     (-10 * counter), the fade half way inside its ramp ((4 - counter) << 10 below 4; the mode switch at counter 4 holds).
//   ShieldB - the v scroll half way (-6 * counter), the colour half way per channel to the next tick's table entry
//     (0xD3933C, counter % 10), the fade half way inside each ramp (in below 4, out from 0x10; the switches hold).
// A shield that ended on the tick holds its last picture; after a draw-only tick everything holds. Nothing here writes
// game state: the scope puts back the cursor, the GTE registers, the Field_Alloc scratch and the shadow-rotation scratch.

namespace ff8fx
{
namespace defend183
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_pkt_start = 0;     // the module cursor at the tick's first draw
	static uint8_t g_held_packets[ARENA_SIZE];

	struct HRec
	{
		uint32_t fn;
		const EffectNode *node;
		EffectNode copy;           // the node as drawn
		uint8_t gte[magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE]; // GTE state the draw started with
		uint8_t hdr[0x90];         // scratch under its draw header when it was allocated
	};
	static const int REC_MAX = 8;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_pkt_start = 0;
		g_nrec = 0;
	}

	static void held_note_shield(uint32_t fn, const EffectNode *t)
	{
		magc::g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return;
		if (g_nrec == 0) g_pkt_start = PacketCursor();
		HRec &r = g_rec[g_nrec++];
		r.fn = fn;
		r.node = t;
		r.copy = *t;
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = r.hdr;
	}

	// the node still runs the same task on the next tick (it did not end on this one) and advanced
	static bool Alive(const HRec &r) { return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == r.fn && r.node->counter == (int16_t)(r.copy.counter + 1); }

	// fade half way when the next tick stays in the same ramp, else tick N's
	static int32_t FadeLerp(bool same, int32_t f0, int32_t f1, bool next, int num, int den) { return next && same ? lerp_i(f0, f1, num, den) : f0; }

	static uint32_t ColourLerp(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000u;
		for (int k = 0; k < 24; k += 8)
		{
			const int32_t x = (int32_t)(a >> k & 0xFF), y = (int32_t)(b >> k & 0xFF);
			r |= (uint32_t)(lerp_i(x, y, num, den) & 0xFF) << k;
		}
		return r;
	}

	// a shield's held draw inputs
	enum { IN_S, IN_SCROLL, IN_FADE, IN_COLOUR, IN_N };
	struct NodeIn { int32_t v[IN_N]; };

	static void NodeInputs(const HRec &r, int num, int den, NodeIn *in)
	{
		const EffectNode &d = r.copy;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const int16_t c = d.counter, c1 = (int16_t)(c + 1);
		if (r.fn == ORIG_ShieldA)
		{
			const ShieldDraw a = ShieldAState(c, d.scale), b = ShieldAState(c1, r.node->scale);
			in->v[IN_S] = next ? magc::lerp16(d.scale, r.node->scale, num, den) : d.scale;
			in->v[IN_SCROLL] = FadeLerp(true, a.scroll, b.scroll, next, num, den);
			in->v[IN_FADE] = FadeLerp(a.fade_on && b.fade_on, a.fade, b.fade, next, num, den); // (one ramp: below 4)
			in->v[IN_COLOUR] = (int32_t)a.colour;
		}
		else
		{
			const ShieldDraw a = ShieldBState(c, d.scale), b = ShieldBState(c1, d.scale);
			in->v[IN_S] = d.scale;
			in->v[IN_SCROLL] = FadeLerp(true, a.scroll, b.scroll, next, num, den);
			in->v[IN_FADE] = FadeLerp(a.fade_on && b.fade_on && (c < 4) == (c1 < 4), a.fade, b.fade, next, num, den); // (same ramp)
			in->v[IN_COLOUR] = (int32_t)(next ? ColourLerp(a.colour, b.colour, num, den) : a.colour);
		}
	}

	static void NodeDraw(const HRec &r, const NodeIn &in)
	{
		const EffectNode &d = r.copy;
		ShieldDraw s = r.fn == ORIG_ShieldA ? ShieldAState(d.counter, d.scale) : ShieldBState(d.counter, d.scale);
		s.scale = (int16_t)in.v[IN_S];
		s.scroll = in.v[IN_SCROLL];
		s.fade = in.v[IN_FADE];
		s.colour = (uint32_t)in.v[IN_COLOUR];
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = (uint8_t *)r.hdr;
		DrawShield(&d, s);
	}

	static void HeldFrame(int num, int den)
	{
		magc::HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start);
		// past half way the draws are the next tick's: under them, what the next tick's arena (the other one of the
		// pair) holds, as the bytes its packets do not write
		if (2 * num > den && g_pkt_start)
		{
			const uint32_t other = g_pkt_start >= ARENA1 ? g_pkt_start - ARENA_SIZE : g_pkt_start + ARENA_SIZE;
			memcpy(g_held_packets, (const void *)other, sizeof(g_held_packets));
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			NodeIn in;
			NodeInputs(r, num, den, &in);
			NodeDraw(r, in);
		}
	}
}

	static void register_mag183_held()
	{
		mark_held(defend183::ORIG_ShieldA);
		mark_held(defend183::ORIG_ShieldB);
		register_module_held(183, defend183::HeldReady, defend183::HeldFrame);
	}
}
