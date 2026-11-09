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

// 30 fps layer of the magic-cast glow (fx_cast.cpp), included at its end. The tasks live in the hit-effect queue,
// which belongs to no effect: they are drawn on held frames task by task (ff8fx::register_eq_task, fx_held.h), the
// frame gate calling HeldDraw for each node at the place where the node drew on the last real tick.
//
// Every drawing task draws THEN updates. A real tick notes each draw (held_note_draw / held_note_drawn): the node as
// drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw header (fields the draw does not
// set keep what the scratch held) and the packet bytes it wrote. HeldDraw draws a node again, from that GTE state and
// header scratch, into a private packet buffer (pre-filled with the real draw's bytes), at its exact in-between state
// num/den of the way to the next tick (the task's own update, RingStep / GlowStep / SparkleStep / TrailStep, on a
// copy: logic stays 15 Hz, nothing calls rand()):
//   Ring    - y scale half way; the fade (header +0x0C) half way when the next tick keeps the mode (from its 6th tick:
//             the switch 0x230 -> 0x2F0 holds); the angle and the x / z scale do not change.
//   Glow    - x / z scale and fade half way.
//   Sparkle - height, y angle (the short way) and radius half way: the point moves on its arc; the sprite frame
//             switches on the real frame.
//   Trail   - height and size half way; the sprite frame (9 - ticks left) switches on the real frame.
// A node holds its real picture (fraction 0) when the tick ran paused (draw only: the next tick may not advance) and
// when the tick ended it (no next tick: the gate keeps those packets itself, keep_ended). Nothing here writes game
// state: the packet cursor, the scratch, the shadow scratch and the GTE registers are put back after each draw.

namespace ff8fx
{
namespace cast
{
	struct Rec
	{
		const void *node;
		uint32_t tick;             // g_eq_tick of the draw (older entries are free slots)
		uint32_t orig;             // task function
		bool paused;               // the tick ran paused
		uint8_t copy[0x24];        // the node as drawn
		uint8_t hdr[HEADER_BYTES]; // scratch under its draw header
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE]; // GTE state the draw started with
		uint32_t pkt_need;         // packet bytes the real draw wrote
		uint32_t pkt_off, pkt_len; // their copy in g_pkt_memo (len 0: not copied)
	};
	static const int REC_SLOTS = 1024; // power of two (the queue holds 300 nodes)
	static Rec g_rec[REC_SLOTS];
	static Rec *g_noting = nullptr;
	static uint32_t g_noting_pkt0 = 0;
	static uint8_t g_pkt_memo[0x80000];
	static uint32_t g_pkt_memo_used = 0, g_pkt_memo_tick = 0xFFFFFFFFu;
	// held packets: one held frame at a time (the frame of tick g_held_tick, fraction g_held_num / den)
	static uint8_t g_held_pkt[0x80000];
	static uint32_t g_held_used = 0, g_held_tick = 0xFFFFFFFFu;
	static int g_held_num = -1;

	static inline uint32_t RecHash(const void *p) { return ((uint32_t)p * 2654435761u) >> 10; }

	static Rec *RecPut(const void *node)
	{
		for (uint32_t h = RecHash(node), i = 0; i < (uint32_t)REC_SLOTS; i++)
		{
			Rec &x = g_rec[(h + i) & (REC_SLOTS - 1)];
			if (x.tick != g_eq_tick || x.node == node) { x.node = node; x.tick = g_eq_tick; return &x; }
		}
		return nullptr;
	}

	static const Rec *RecGet(const void *node)
	{
		for (uint32_t h = RecHash(node), i = 0; i < (uint32_t)REC_SLOTS; i++)
		{
			const Rec &x = g_rec[(h + i) & (REC_SLOTS - 1)];
			if (x.tick != g_eq_tick) return nullptr;
			if (x.node == node) return &x;
		}
		return nullptr;
	}

	static void held_note_draw(uint32_t orig, const void *node)
	{
		g_hdr_slot = nullptr;
		if (g_pkt_memo_tick != g_eq_tick)
		{
			g_pkt_memo_tick = g_eq_tick;
			g_pkt_memo_used = 0;
		}
		Rec *r = RecPut(node);
		g_noting = r;
		if (!r) return;
		r->orig = orig;
		r->paused = Paused();
		memcpy(r->copy, node, sizeof(r->copy));
		memcpy(r->gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r->gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r->pkt_need = r->pkt_off = r->pkt_len = 0;
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

	// the node of `orig` drawn at num/den of the way to its next tick (see the top of this file)
	static bool HeldDraw(uint32_t orig, const void *node, int num, int den)
	{
		const Rec *r = RecGet(node);
		if (!r || r->orig != orig || !r->pkt_need || den <= 0) return false;
		if (g_held_tick != g_eq_tick || g_held_num != num)
		{
			g_held_tick = g_eq_tick;
			g_held_num = num;
			g_held_used = 0;
		}
		// room for the draw (an in-between model may keep a few more faces than the real one)
		const uint32_t room = r->pkt_need * 2 + 0x400;
		if (g_held_used + room > sizeof(g_held_pkt)) return false;
		uint8_t *buf = g_held_pkt + g_held_used;
		if (r->pkt_len) memcpy(buf, g_pkt_memo + r->pkt_off, r->pkt_len);
		const bool step = !r->paused && num > 0;
		uint32_t end;
		{
			HeldScope scope(PacketCursor(), buf, room, 0);
			memcpy((void *)GTE_DATA, r->gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r->gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			g_hdr_slot = (uint8_t *)r->hdr;
			uint8_t next_hdr[0x58];
			bool fade = false;
			if (orig == ORIG_Ring)
			{
				RingNode a, m;
				memcpy(&a, r->copy, sizeof(a));
				m = a;
				RingNode b = a;
				if (step && !(RingStep(&b) & TASK_END))
				{
					m.scale_y = lerp16(a.scale_y, b.scale_y, num, den);
					memcpy(next_hdr, r->hdr, sizeof(next_hdr));
					RingHeader(next_hdr, &b);
					fade = true;
				}
				ModelMatrix(m.angle, m.pos, m.scale_xz, m.scale_y);
				uint8_t *h = AllocHeader(0x58);
				RingHeader(h, &m);
				if (fade) LerpPrimFade(h, next_hdr, num, den);
				RenderModel(h);
			}
			else if (orig == ORIG_Glow)
			{
				GlowNode a, m;
				memcpy(&a, r->copy, sizeof(a));
				m = a;
				GlowNode b = a;
				if (step && !(GlowStep(&b) & TASK_END))
				{
					m.scale = lerp16(a.scale, b.scale, num, den);
					memcpy(next_hdr, r->hdr, sizeof(next_hdr));
					GlowHeader(next_hdr, &b);
					fade = true;
				}
				ModelMatrix(0, m.pos, m.scale, 0x1000);
				uint8_t *h = AllocHeader(0x58);
				GlowHeader(h, &m);
				if (fade) LerpPrimFade(h, next_hdr, num, den);
				RenderModel(h);
			}
			else if (orig == ORIG_Sparkle)
			{
				SparkleNode a, m;
				memcpy(&a, r->copy, sizeof(a));
				m = a;
				SparkleNode b = a;
				if (step && !(SparkleStep(&b) & TASK_END))
				{
					m.pos[1] = lerp16(a.pos[1], b.pos[1], num, den);
					m.angle = lerp16(a.angle, b.angle, num, den);
					m.radius = lerp16(a.radius, b.radius, num, den);
				}
				int16_t v[4];
				SparklePoint(&m, v);
				SpriteDraw(v, m.size, m.frame);
			}
			else if (orig == ORIG_Trail)
			{
				TrailNode a, m;
				memcpy(&a, r->copy, sizeof(a));
				m = a;
				TrailNode b = a;
				if (step && !(TrailStep(&b) & TASK_END))
				{
					m.pos[1] = lerp16(a.pos[1], b.pos[1], num, den);
					m.size = lerp16(a.size, b.size, num, den);
				}
				SpriteDraw(m.pos, m.size, (int16_t)(9 - a.left));
			}
			end = PacketCursor();
		}
		g_hdr_slot = nullptr;
		const uint32_t used = end > (uint32_t)buf ? end - (uint32_t)buf : 0;
		g_held_used += used < room ? used : room;
		return true;
	}
}

	static void register_fx_cast_held()
	{
		// the spawner's port runs in the queue too (it draws nothing: no held draw)
		register_eq_task(cast::ORIG_Spawner, nullptr);
		static const uint32_t DRAWING[] = { cast::ORIG_Sparkle, cast::ORIG_Trail, cast::ORIG_Glow, cast::ORIG_Ring };
		for (uint32_t f : DRAWING)
		{
			register_eq_task(f, cast::HeldDraw);
			mark_held(f);
			keep_ended(f);
		}
	}
}
