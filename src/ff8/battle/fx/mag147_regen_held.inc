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

// 30 fps part of mag147_regen.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag147_regen.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing Regen task draws THEN updates. A real tick records every draw in order with the
// GTE registers and the Field_Alloc scratch it started from (the sprite header keeps the scratch
// bytes where the code does not write, and the trail draws reuse the freed leaf header). A held
// frame replays the log in the same order, each draw on copies moved half way to the state the
// task's own update computes:
//   Envelope - rotation angle, x / z and y scale half way along their steps, fade half way to the
//              next tick's (a counter ramp); its last draw (tick 29) holds.
//   Leaf     - position half way along its velocity (the leaf sprite, and its shadow's x / z),
//              the shadow's grey half way to the next tick's; the flipbook frame holds.
//   Trail    - every particle drawn on the real tick: size and position half way to their next
//              values (rising / shrinking by 0x10, or drifting / shrinking by 1/16); its flipbook
//              frame holds; a particle that dies on the next tick holds.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing updates: everything holds. The
// director and the root draw nothing; a leaf still waiting its delay draws nothing.

namespace ff8fx
{
namespace regen147
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_ENVELOPE = 1, REC_LEAF = 2 };

	struct Rec
	{
		uint32_t kind;
		bool updated;             // the task updates after this draw (not the draw-only mode)
		EnvelopeNode env;         // envelope, as drawn
		LeafNode leaf;            // leaf, as drawn
		TrailPart trail[16];      // its trail, as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[128];
	static uint8_t g_held_packets[0x20000]; // module arena (0xC000 per tick)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.updated = !DrawOnly();
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_envelope(const EnvelopeNode *e)
	{
		Rec *r = NewRec(REC_ENVELOPE);
		if (!r) return;
		r->env = *e;
	}

	static void held_note_leaf(const LeafNode *p)
	{
		Rec *r = NewRec(REC_LEAF);
		if (!r) return;
		r->leaf = *p;
		memcpy(r->trail, TrailOf(p->action, p->leaf), sizeof(r->trail));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void EnvelopeHeld(const Rec &r, int num, int den)
	{
		const EnvelopeNode &a = r.env;
		EnvelopeNode m = a;
		int32_t fade = EnvelopeFade(a.counter, a.kind);
		if (r.updated)
		{
			EnvelopeNode b = a;
			EnvelopeUpdate(&b);
			if (b.counter < 0x1E)
			{
				m.angle = lerp16(a.angle, b.angle, num, den);
				m.sxz = lerp16(a.sxz, b.sxz, num, den);
				m.sy = lerp16(a.sy, b.sy, num, den);
				fade = lerp_i(fade, EnvelopeFade(b.counter, b.kind), num, den);
			}
		}
		EnvelopeDraw(&m, fade);
	}

	static void LeafHeld(const Rec &r, int num, int den)
	{
		const LeafNode &a = r.leaf;
		LeafNode m = a;
		int32_t shade = LeafShade(a.counter);
		if (r.updated)
		{
			LeafNode b = a;
			LeafMove(&b);
			for (int i = 0; i < 3; i++) m.pos[i] = lerp16(a.pos[i], b.pos[i], num, den);
			shade = lerp_i(shade, LeafShade((int16_t)(a.counter + 1)), num, den);
		}
		uint8_t *h = LeafDraw(&m, shade);
		for (int k = 0; k < 16; k++)
		{
			const TrailPart &t = r.trail[k];
			if (t.counter < 0) continue;
			TrailPart mt = t;
			if (r.updated)
			{
				TrailPart b = t;
				TrailUpdate(&b, a.counter);
				if (b.counter >= 0)
				{
					mt.size = lerp16(t.size, b.size, num, den);
					for (int i = 0; i < 3; i++) mt.pos[i] = lerp16(t.pos[i], b.pos[i], num, den);
				}
			}
			TrailDraw(h, &mt);
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_ENVELOPE: EnvelopeHeld(r, num, den); break;
			case REC_LEAF: LeafHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag147_held()
	{
		mark_held(regen147::ORIG_Envelope);
		mark_held(regen147::ORIG_Leaf);
		register_module_held(147, regen147::HeldReady, regen147::HeldFrame);
	}
}
