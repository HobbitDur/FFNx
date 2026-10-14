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

// 30 fps part of mag315_fake_president_death.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag315_fake_president_death.cpp; shared pieces in
// mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records every draw in queue order with the GTE
// registers and the Field_Alloc scratch it started from (headers keep the scratch bytes where the
// code does not write). A held frame replays the log in the same order:
//   Pool sprites (drops, bubbles, sparks) - the frame's camera; the entries as drawn, each half way
//     to its live entry when the tick moved it (frame + 1): drop position and size, bubble height,
//     spark angle, height and radius; the flipbook frame holds; an entry the tick freed (sequence
//     ended) or did not move (draw-only tick) holds.
//   Melting model (caster / next slot) - the entity at its in-between pose (pose_midpoint), the
//     floor (node +0x12) and the strength half way to the next tick's, the next slot's tint (its
//     tint task's fade from black, counters 0..7, in step with it) half way to the next tick's
//     level (put back after the draw; the colour-table cycling holds); its vertices go to a private
//     vertex buffer, its packets to a private copy of the frame arena; the shadow temporaries are
//     put back. The last draw (the next tick no longer draws) holds.
//   Prim models (splash ring, puddle ring, column, spinning ring) - rotation, scale, u scroll and
//     depth cue half way to the next tick's (a cue on one side only: from / to level 0); the last
//     draw holds.
// Spawns (random) stay at 15 Hz. The module packet arena alternates per tick: the held packets start
// where the tick's first draw started. The tints (entity +0x28) are entity state, held by the engine's
// own model draws.

namespace ff8fx
{
namespace fp315
{
	static const uint32_t SCRATCH = 0x400;
	static const int MAX_RECS = 0x40, MAX_PARTS = 0x100;

	struct PartSnap { uint32_t addr; uint8_t e[0x18]; };
	struct Rec
	{
		uint32_t fn;                // original task function
		uint32_t node_addr;         // the live node
		uint8_t node[0x24];         // the node as drawn
		int first, n;               // pool sprites: the entries as drawn (g_parts)
		bool draw_only;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];   // Field_Alloc scratch at the draw's first allocation
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nparts = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the tick's first melting draw
	static Rec g_recs[MAX_RECS];
	static PartSnap g_parts[MAX_PARTS];
	static uint8_t g_held_packets[0xC000];  // module arena (0xC000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + melting model packets
	static uint8_t g_held_verts[0x10000];   // melting model vertices (indices are 12-bit: 0x8000)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nparts = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(uint32_t fn, uint32_t n, uint32_t scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node_addr = n;
		memcpy(r.node, (const void *)n, sizeof(r.node));
		r.first = g_nparts;
		r.n = 0;
		r.draw_only = DrawOnly();
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)scratch, SCRATCH);
		return &r;
	}

	// a pool-sprite task about to draw its entries (w = its first allocation, h its sequence header)
	static void held_note_parts(uint32_t fn, uint32_t n, uint32_t w, uint32_t h, int32_t mask)
	{
		(void)h;
		Rec *r = NewRec(fn, n, w);
		if (!r) return;
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			const bool mine = fn == ORIG_Drops ? (S32(e, 0) & mask) != 0 : (U8(e, 0) & mask) != 0;
			if (!mine) continue;
			if (g_nparts >= MAX_PARTS)
			{
				g_nrec--;
				g_nparts = r->first;
				return;
			}
			PartSnap &p = g_parts[g_nparts++];
			p.addr = e;
			memcpy(p.e, (const void *)e, 0x18);
			r->n++;
		}
	}

	// a melting-model or prim-model task about to draw
	static void held_note_draw(uint32_t fn, uint32_t n)
	{
		if (!NewRec(fn, n, var<uint32_t>(0x1D999C4))) return;
		if ((fn == ORIG_Melt || fn == ORIG_Rise) && !g_frame_packets) g_frame_packets = FrameCursor();
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// in-between helpers
	// ------------------------------------------------------------------
	// the live node is the drawn one one update later, and the next tick draws it again
	static bool NodeNext(const Rec &r, int32_t end)
	{
		if (r.draw_only) return false;
		const int32_t c = *(const int16_t *)(r.node + 0xC);
		return S16(r.node_addr, 0xC) == c + 1 && c + 1 < end;
	}

	static Cue LerpCue(Cue a, Cue b, int num, int den)
	{
		if (num <= 0) return a;
		if (num >= den) return b;
		const bool ca = a.flags == 0xF3, cb = b.flags == 0xF3;
		if (!ca && !cb) return a;
		return { 0xF3u, lerp_i(ca ? a.fade : 0, cb ? b.fade : 0, num, den) };
	}

	// a held frame's melting-model outputs: frame arena packets (a copy of the real tick's, so bytes
	// the draw leaves alone are the real ones), the vertex buffer, the entity-shadow temporaries;
	// all put back
	struct FrameScope
	{
		uint32_t cursor, verts, words[4];
		uint8_t shadow[0x80];
		FrameScope()
		{
			verts = VertexBuffer();
			cursor = FrameCursor();
			words[0] = var<uint32_t>(0xB8B9E4);
			words[1] = var<uint32_t>(0xB8B9EC);
			words[2] = var<uint32_t>(0x1D99C18);
			words[3] = var<uint32_t>(0x1D99C1C);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			if (g_frame_packets && cursor > g_frame_packets)
			{
				const uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			VertexBuffer() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0x1D99C1C) = words[3];
			var<uint32_t>(0x1D99C18) = words[2];
			var<uint32_t>(0xB8B9EC) = words[1];
			var<uint32_t>(0xB8B9E4) = words[0];
			VertexBuffer() = verts;
			FrameCursor() = cursor;
		}
	};

	// ------------------------------------------------------------------
	// held draws
	// ------------------------------------------------------------------
	static void PartsHeld(const Rec &r, int num, int den)
	{
		const uint32_t wsize = r.fn == ORIG_Sparks ? 0x50 : 0x48;
		const uint32_t w = (uint32_t)FieldAlloc(wsize);
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		memcpy((void *)w, r.scratch, wsize + 0xB4);
		// the frame's camera, as the real draw copies it
		const uint32_t cam = w + (r.fn == ORIG_Sparks ? 0x10 : 8);
		memcpy((void *)cam, (const void *)&Camera(), 0x20);
		GteSetLightMatrix(cam);
		GteSetBkFromTranslation(cam);
		uint8_t buf[0x18];
		const uint32_t e = (uint32_t)buf;
		for (int i = 0; i < r.n; i++)
		{
			const PartSnap &p = g_parts[r.first + i];
			memcpy(buf, p.e, sizeof(buf));
			// the live entry one update later (same owner, frame + 1): half way to it
			const uint32_t l = p.addr;
			const bool moved = S32(l, 0) == S32(e, 0) && S16(l, 4) == (int16_t)(S16(e, 4) + 1);
			if (r.fn == ORIG_Drops)
			{
				if (moved)
				{
					S16(e, 6) = lerp16(S16(e, 6), S16(l, 6), num, den);
					S16(e, 8) = lerp16(S16(e, 8), S16(l, 8), num, den);
					S16(e, 0xA) = lerp16(S16(e, 0xA), S16(l, 0xA), num, den);
					S16(e, 0xC) = lerp16(S16(e, 0xC), S16(l, 0xC), num, den);
				}
				DropDraw(w, h, e);
			}
			else if (r.fn == ORIG_Sparks)
			{
				if (moved)
				{
					S16(e, 8) = lerp16(S16(e, 8), S16(l, 8), num, den);
					S16(e, 0xA) = lerp16(S16(e, 0xA), S16(l, 0xA), num, den);
					S16(e, 0xC) = lerp16(S16(e, 0xC), S16(l, 0xC), num, den);
				}
				SparkDraw(w, h, e);
			}
			else
			{
				if (moved) S16(e, 0xA) = lerp16(S16(e, 0xA), S16(l, 0xA), num, den);
				BubbleDraw(h, e);
			}
		}
		FieldFree(0xB4);
		FieldFree(wsize);
	}

	static void MeltHeld(const Rec &r, int num, int den)
	{
		uint8_t copy[0x24];
		memcpy(copy, r.node, sizeof(copy));
		const uint32_t n = (uint32_t)copy;
		const bool rise = r.fn == ORIG_Rise;
		const int32_t c = S16(n, 0xC);
		int32_t strength = rise ? RiseStrength(c) : CasterStrength(c);
		int pnum = 0;
		if (NodeNext(r, rise ? 0x6E : 0x41))
		{
			strength = lerp_i(strength, rise ? RiseStrength(c + 1) : CasterStrength(c + 1), num, den);
			S16(n, 0x12) = lerp16(S16(n, 0x12), S16(r.node_addr, 0x12), num, den);
			pnum = num;
		}
		const uint32_t entity = Ent(S16(n, 0xE));
		// the next slot's tint task runs in step with it (same spawn tick, before it in the queue): its
		// fade from black (counters 0..7) half way to the next tick's level
		const uint32_t tint = U32(entity, 0x28);
		const bool fade = rise && pnum && c + 1 < 8;
		if (fade)
		{
			GteSetFarColour(0, 0, 0);
			GteLoadRGBC(TINT_Next);
			GteSetIR0(lerp_i(shl32(8 - c, 9), shl32(7 - c, 9), num, den));
			GteDPCS();
			GteStoreRGB2(entity + 0x28);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		}
		pose_midpoint((void *)(entity + 0x60), (void *)(entity + 0x6C), pnum, den);
		MeltDraw(n, strength, rise ? 0x384 : 0x2BC, rise ? 0xB : 0xA);
		if (fade) U32(entity, 0x28) = tint;
	}

	static void PrimHeld(const Rec &r, int num, int den)
	{
		uint8_t copy[0x24];
		memcpy(copy, r.node, sizeof(copy));
		const uint32_t n = (uint32_t)copy, l = r.node_addr;
		const int32_t c = S16(n, 0xC);
		switch (r.fn)
		{
		case ORIG_Splash:
		{
			Cue cue = SplashCue(c);
			if (NodeNext(r, 0x10))
			{
				cue = LerpCue(cue, SplashCue(c + 1), num, den);
				S16(n, 0x18) = lerp16(S16(n, 0x18), S16(l, 0x18), num, den);
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(l, 0x1C), num, den);
			}
			SplashDraw(n, cue);
			break;
		}
		case ORIG_Puddle:
		{
			Cue cue = PuddleCue(c);
			int32_t u = shl32(c, 2);
			if (NodeNext(r, 0x50))
			{
				cue = LerpCue(cue, PuddleCue(c + 1), num, den);
				u = lerp_i(u, shl32(c + 1, 2), num, den);
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(l, 0x1C), num, den);
				S16(n, 0x20) = lerp16(S16(n, 0x20), S16(l, 0x20), num, den);
			}
			PuddleDraw(n, u, cue);
			break;
		}
		case ORIG_Column:
		{
			Cue cue = ColumnCue(c);
			if (NodeNext(r, 0x14))
			{
				cue = LerpCue(cue, ColumnCue(c + 1), num, den);
				S16(n, 0x20) = lerp16(S16(n, 0x20), S16(l, 0x20), num, den);
			}
			ColumnDraw(n, cue);
			break;
		}
		case ORIG_Ring:
		{
			int32_t fade = RingFade(c);
			if (NodeNext(r, 0x58))
			{
				fade = lerp_i(fade, RingFade(c + 1), num, den);
				S16(n, 0x18) = lerp16(S16(n, 0x18), S16(l, 0x18), num, den);
			}
			RingDraw(n, fade);
			break;
		}
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_Drops:
			case ORIG_Bubbles:
			case ORIG_Bubbles2:
			case ORIG_Sparks: PartsHeld(r, num, den); break;
			case ORIG_Melt:
			case ORIG_Rise: MeltHeld(r, num, den); break;
			default: PrimHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag315_held()
	{
		mark_held(fp315::ORIG_Drops);
		mark_held(fp315::ORIG_Bubbles);
		mark_held(fp315::ORIG_Bubbles2);
		mark_held(fp315::ORIG_Sparks);
		mark_held(fp315::ORIG_Melt);
		mark_held(fp315::ORIG_Rise);
		mark_held(fp315::ORIG_Splash);
		mark_held(fp315::ORIG_Puddle);
		mark_held(fp315::ORIG_Column);
		mark_held(fp315::ORIG_Ring);
		register_module_held(315, fp315::HeldReady, fp315::HeldFrame);
	}
}
