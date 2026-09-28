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

// 30 fps part of mag026_clash.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag026_clash.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing Clash task updates THEN draws (its state handler, then 0x888A50 / 0x888C60 / the
// flash quad). A real tick records every draw in order with a copy of the node as drawn, the GTE
// registers and the Field_Alloc scratch it started from, and the next tick's state: the task's own
// state handler run on a copy (tick + 1; the handlers of a drawn node are pure: no random draw, no
// spawn). Sprites hidden on the real tick (waiting for their delay) are recorded too: the next
// tick may show them. A held frame replays the log in the same order from the same GTE state and
// scratch, each part drawn from its own node (the in-between state written over it for the draw,
// then put back):
//   Rings / shock waves - scale (3 x int32) and fade (table / ramp) half way to the next tick's;
//              the position does not move.
//   Sprites  - (burst sparks, dust, landing sparks, walk-back dust) the position half way along
//              their decelerating flight; the sequence frame holds (15 Hz flipbook). A sprite hidden
//              on the real tick stays hidden.
//   Flash    - its colour holds (colour-script steps).
//   A part the next tick does not draw (end of its sequence / fade, flash script end) holds.
//   At fraction 1 (num >= den) every part is drawn as the next tick draws it (frames, colours
//   and sprites whose delay ends included); parts that end draw nothing; parts the next tick
//   spawns are not drawn.
// Not drawn by the module (hold, 15 Hz): the knocked-back target's position / spin / facing
// (entity state the battle draws).
// No Clash task has a draw-only mode. The held frame draws into a private packet buffer
// (HeldScope) and puts back the GTE, the Field_Alloc scratch and the shadow-camera scratch.

namespace ff8fx
{
namespace act
{
namespace clash026
{
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE;
	static const uint32_t GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t SCRATCH = 0x200;   // Field_Alloc scratch under a draw's header
	static const uint32_t NODE_MAX = 0x70;   // sprite nodes (prim 0x6C, flash 0x3C)
	enum : uint32_t { REC_PRIM = 1, REC_SPRITE, REC_FLASH };

	struct Rec
	{
		uint32_t kind;
		uint32_t node, size;                 // the live node
		uint32_t scale;                      // sprite: its scale argument
		uint32_t colour, next_colour;        // flash
		bool drawn;                          // the real tick drew it (a sprite may be hidden)
		bool next_drawn;                     // the next tick draws it
		alignas(4) uint8_t copy[NODE_MAX];   // the node as drawn
		alignas(4) uint8_t next[NODE_MAX];   // the node as the next tick draws it
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_replaying = false;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;           // module packet cursor at the tick's first draw
	static Rec g_recs[768];
	static uint8_t g_held_packets[0x10000];  // module arena (0xC000 per tick)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind, uint32_t node, uint32_t size, bool drawn)
	{
		if (g_replaying || g_ported_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (!g_packets && drawn) g_packets = MEM<uint32_t>(PACKET_CURSOR);
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.size = size;
		r.drawn = drawn;
		r.next_drawn = false;
		memcpy(r.copy, (const void *)node, size);
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)MEM<uint32_t>(0x1D999C4), SCRATCH);
		return &r;
	}

	// state table of a drawing task (by its original function, node +8)
	static const StateFn *TableOf(uint32_t fn)
	{
		switch (fn)
		{
		case ORIG_BurstRing: return T_BurstRing;
		case ORIG_WaveA: return T_WaveA;
		case ORIG_WaveB: return T_WaveB;
		case ORIG_LandRing: return T_LandRing;
		case ORIG_Spark: return T_Spark;
		case ORIG_Dust: return T_Dust;
		case ORIG_LandSpark: return T_LandSpark;
		case ORIG_WalkDust: return T_WalkDust;
		}
		return nullptr;
	}

	// the next tick's update on a copy of the node (tick + 1, its current state's handler)
	static void Predict(Rec &r)
	{
		memcpy(r.next, r.copy, r.size);
		const uint32_t n = P(r.next);
		const StateFn *t = TableOf(U32(n, 8));
		if (!t) return;
		U16(n, 0x24) = (uint16_t)(U16(n, 0x24) + 1);
		t[S8(n, 0x29)](n);
		r.next_drawn = (U8(n, 0x26) & 4) == 0;
	}

	static void held_note_prim(uint32_t node)
	{
		Rec *r = NewRec(REC_PRIM, node, 0x6C, true);
		if (!r) return;
		Predict(*r);
	}

	static void held_note_sprite(uint32_t node, uint32_t scale, bool hidden)
	{
		Rec *r = NewRec(REC_SPRITE, node, 0x70, !hidden);
		if (!r) return;
		r->scale = scale;
		Predict(*r);
		// a hidden sprite is only drawn when the next tick shows it (fraction 1)
		if (hidden && !r->next_drawn) g_nrec--;
	}

	static void held_note_flash(uint32_t node)
	{
		Rec *r = NewRec(REC_FLASH, node, 0x3C, true);
		if (!r) return;
		r->colour = U32(node, 0x34);
		memcpy(r->next, r->copy, 0x3C);
		const uint32_t n = P(r->next);
		U16(n, 0x24) = (uint16_t)(U16(n, 0x24) + 1);
		r->next_drawn = FlashStep(n);
		r->next_colour = U32(n, 0x34);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool one = num >= den;
		magc::HeldScope scope(MEM<uint32_t>(PACKET_CURSOR), g_held_packets, sizeof(g_held_packets), g_packets);
		g_replaying = true;
		alignas(4) static uint8_t buf[NODE_MAX], live[NODE_MAX];
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_FLASH)
			{
				if (!one) FlashDraw(r.colour);
				else if (r.next_drawn) FlashDraw(r.next_colour);
				continue;
			}
			const uint32_t b = P(buf), nx = P(r.next);
			if (one)
			{
				if (!r.next_drawn) continue;
				memcpy(buf, r.next, r.size);
			}
			else
			{
				if (!r.drawn) continue;
				memcpy(buf, r.copy, r.size);
				if (r.next_drawn)
				{
					for (int k = 0; k < 3; k++)
						S16(b, 0x1C + 2 * k) = magc::lerp16(S16(b, 0x1C + 2 * k), S16(nx, 0x1C + 2 * k), num, den);
					if (r.kind == REC_PRIM)
					{
						for (int k = 0; k < 3; k++)
							S32(b, 0x30 + 4 * k) = lerp_i(S32(b, 0x30 + 4 * k), S32(nx, 0x30 + 4 * k), num, den);
						S16(b, 0x50) = (int16_t)lerp_i(S16(b, 0x50), S16(nx, 0x50), num, den);
					}
				}
			}
			// drawn from the task's own node (the draw only reads it), put back afterwards
			memcpy(live, (const void *)r.node, r.size);
			memcpy((void *)r.node, buf, r.size);
			Replay(r);
			if (r.kind == REC_PRIM) PrimDraw(r.node);
			else SpriteDraw(r.node, r.scale);
			memcpy((void *)r.node, live, r.size);
		}
		g_replaying = false;
	}
}
}

	static void register_mag026_held()
	{
		mark_held(act::clash026::ORIG_BurstRing);
		mark_held(act::clash026::ORIG_WaveA);
		mark_held(act::clash026::ORIG_WaveB);
		mark_held(act::clash026::ORIG_LandRing);
		mark_held(act::clash026::ORIG_Spark);
		mark_held(act::clash026::ORIG_Dust);
		mark_held(act::clash026::ORIG_LandSpark);
		mark_held(act::clash026::ORIG_WalkDust);
		mark_held(act::clash026::ORIG_Flash);
		register_module_held(26, act::clash026::HeldReady, act::clash026::HeldFrame);
	}
}
