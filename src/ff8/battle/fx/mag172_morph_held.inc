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

// 30 fps part of mag172_morph.cpp (Morph, effect 172). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws the
// state num/den of the way from the last real tick to the next one.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log) with the GTE
// registers and the Field_Alloc scratch each one started from; a held frame replays them in the same order with the vanilla draw
// code, each at its in-between state:
//   Rings         - angle half way along its spin, fade along its ramp (a segment end holds); the shadow-rotation camera of
//                   the current view.
//   Sparkles      - still: drawn as the tick drew them (flipbook frames held); the new ones (CRT rand) appear on the real frame.
//   Tinted caster - the fade along its ramp (a segment end holds), the cycle colour (0xD655E4[tick % 20]) per channel half way to
//                   the next tick's, the model at its in-between pose (pose_midpoint); its shadow redrawn as it is.
// A part the next tick no longer runs (or whose counter did not move: draw-only tick) holds its picture (state N). The model draw's
// side effects are undone (frame packets to a private copy of the frame arena, vertex buffer 0x1D98B3C to a private copy, shadow /
// geometry temporaries put back, bone matrices rebuilt from the real pose); the module packets go to a private buffer (HeldScope);
// the engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace morph172
{
	static const uint32_t SCRATCH = 0x400;
	static const int MAX_RECS = 0x40, MAX_PARTS = 0x100;

	struct Rec
	{
		int kind;
		uint32_t node;
		uint8_t copy[0x24];       // the node as drawn
		int first, count;         // particle tasks: their particles' records
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PRec { uint32_t addr; uint8_t copy[0x18]; };

	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0;
	static bool g_overflow = false;
	static Rec *g_cur = nullptr;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_tick = 0xFFFFFFFF;
	static uint32_t g_frame_packets = 0;    // frame packet cursor at the tick's first model draw
	static Rec g_recs[MAX_RECS];
	static PRec g_parts[MAX_PARTS];
	static uint8_t g_held_packets[0x8000];  // one module arena
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_verts[0x10000];
	static uint8_t g_rec_verts[0x10000];    // the vertex buffer as the real tick's model draw found it (its pad words)
	static uint32_t g_rec_verts_n = 0, g_rec_verts_tick = 0xFFFFFFFF;
	static bool g_held = false;
	static int g_hnum = 0, g_hden = 0;

	static void held_note_root()
	{
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_overflow = false;
		g_cur = nullptr;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, uint32_t n)
	{
		g_cur = nullptr;
		if (g_held || g_log_tick != g_real_tick) return nullptr;
		if (g_nrec >= MAX_RECS)
		{
			g_overflow = true;
			return nullptr;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.copy, (const void *)n, sizeof(r.copy));
		r.first = g_npart;
		r.count = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_node(int kind, uint32_t n) { NewRec(kind, n); }
	static void held_note_parts(int kind, uint32_t n) { g_cur = NewRec(kind, n); }

	static void held_note_particle(uint32_t p)
	{
		if (!g_cur) return;
		if (g_npart >= MAX_PARTS)
		{
			g_overflow = true;
			return;
		}
		PRec &q = g_parts[g_npart++];
		q.addr = p;
		memcpy(q.copy, (const void *)p, sizeof(q.copy));
		g_cur->count++;
	}

	static uint32_t VertexBytes();

	// the caster model: at its in-between pose on a held frame; the real tick notes where the frame arena was at its first
	// model draw and the vertex buffer as that draw found it (the overlay corners out of range use its pad words, which the
	// draw never writes: later draws of the frame can change them before a held frame)
	static void held_model_pose(uint32_t entity)
	{
		if (g_held)
		{
			if (g_hden) pose_midpoint((void *)(entity + 0x60), (void *)(entity + 0x6C), g_hnum, g_hden);
			return;
		}
		if (g_frame_tick != g_real_tick)
		{
			g_frame_tick = g_real_tick;
			g_frame_packets = FrameCursor();
			const uint32_t verts = var<uint32_t>(0x1D98B3C);
			g_rec_verts_n = verts ? VertexBytes() : 0;
			if (g_rec_verts_n) memcpy(g_rec_verts, (const void *)verts, g_rec_verts_n);
			g_rec_verts_tick = g_real_tick;
		}
	}

	static bool HeldReady() { return g_log_tick == g_real_tick && !g_overflow; }

	// largest vertex count of one drawn object of a model (the vertex buffer each object is built in)
	static uint32_t MaxObjectVertices(uint32_t model, uint32_t visible)
	{
		uint32_t best = 0;
		const uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		for (int32_t i = 0; i < count && i < 64; i++)
		{
			if (!(visible & (1u << (i & 31)))) continue;
			uint32_t obj = table + U32(table, 4 + 4 * i);
			int32_t groups = S16(obj, 0);
			obj += 2;
			uint32_t n = 0;
			for (; groups > 0; groups--)
			{
				const int32_t nv = S16(obj, 2);
				obj += 4 + (nv > 0 ? nv : 0) * 6;
				if (nv > 0) n += (uint32_t)nv;
			}
			if (n > best) best = n;
		}
		return best;
	}

	// bytes of the vertex buffer the caster's draw uses (its largest object, or its weapon's)
	static uint32_t VertexBytes()
	{
		const uint32_t e = P(TintEntity());
		uint32_t n = MaxObjectVertices(U32(e + 0x60, 4), U32(e, 0x7C));
		const uint32_t weapon = U32(e, 0x78);
		if (weapon && !(U8(e, 1) & 2))
		{
			const uint32_t k = MaxObjectVertices(U32(weapon, 4), 0xFFFFFFFF);
			if (k > n) n = k;
		}
		n *= 8;
		return n < sizeof(g_held_verts) ? n : sizeof(g_held_verts);
	}

	// the engine state a model draw changes, private or put back for the held frame
	struct ModelScope
	{
		bool on;
		uint32_t frame, verts, geom[2], shadow_words[2];
		uint8_t shadow[0x80];
		ModelScope(bool model_drawn) : on(model_drawn)
		{
			if (!on) return;
			frame = FrameCursor();
			if (g_frame_packets && frame > g_frame_packets)
			{
				uint32_t used = frame - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = P(g_held_frame);
			verts = var<uint32_t>(0x1D98B3C);
			geom[0] = var<uint32_t>(0x1D99C18);
			geom[1] = var<uint32_t>(0x1D99C1C);
			shadow_words[0] = var<uint32_t>(0xB8B9E4);
			shadow_words[1] = var<uint32_t>(0xB8B9EC);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			// the model's faces read the vertex buffer's pad words (never written by the draw): the private buffer starts
			// as the real tick's draw found the real one (else as a copy of the real one now)
			if (g_rec_verts_tick == g_real_tick && g_rec_verts_n) memcpy(g_held_verts, g_rec_verts, g_rec_verts_n);
			else if (verts) memcpy(g_held_verts, (const void *)verts, VertexBytes());
			var<uint32_t>(0x1D98B3C) = P(g_held_verts);
		}
		~ModelScope()
		{
			if (!on) return;
			var<uint32_t>(0x1D98B3C) = verts;
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0xB8B9EC) = shadow_words[1];
			var<uint32_t>(0xB8B9E4) = shadow_words[0];
			var<uint32_t>(0x1D99C1C) = geom[1];
			var<uint32_t>(0x1D99C18) = geom[0];
			FrameCursor() = frame;
		}
	};

	// the node runs on: same task, its counter one on (not on a draw-only tick)
	static bool Next(const Rec &r)
	{
		const TaskNode *t = (const TaskNode *)r.node;
		return !DrawOnly() && (t->flags & 1) && t->func == ((const TaskNode *)r.copy)->func && S16(r.node, 0xC) == (int16_t)(*(const int16_t *)(r.copy + 0xC) + 1);
	}

	static int RingSegment(int32_t c) { return c < 8 ? 0 : (c < 0x30 ? 1 : 2); }

	static void HeldFrame(int num, int den)
	{
		bool tint = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_TINT) tint = true;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		ModelScope model(tint && g_frame_tick == g_real_tick);
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		g_held = true;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			uint8_t cp[0x24];
			memcpy(cp, r.copy, sizeof(cp));
			const uint32_t c = P(cp), n = r.node;
			const int16_t cnt = S16(c, 0xC);
			const int k = Next(r) ? num : 0;
			switch (r.kind)
			{
			case REC_TINT:
			{
				const bool on = k && cnt + 1 < 0x38;
				int32_t fade = TintFade(cnt);
				if (on && RingSegment(cnt) == RingSegment(cnt + 1)) fade = lerp_i(fade, TintFade(cnt + 1), num, den);
				g_hnum = on ? num : 0;
				g_hden = on ? den : 0;
				// the cycle colour num / den of the way to the next tick's (a smooth 20-step gradient)
				uint32_t colour = TintColour(cnt);
				if (on)
				{
					const uint32_t next = TintColour(cnt + 1);
					uint32_t mixed = colour & 0xFF000000u;
					for (int sh = 0; sh < 24; sh += 8) mixed |= (uint32_t)lerp_i((colour >> sh) & 0xFF, (next >> sh) & 0xFF, num, den) << sh;
					colour = mixed;
				}
				TintDraw(c, fade, colour);
				g_hnum = g_hden = 0;
				break;
			}
			case REC_RING:
			case REC_RING2:
			{
				int32_t fade = RingFade(cnt);
				if (k)
				{
					S16(c, 0x18) = lerp16(S16(c, 0x18), S16(n, 0x18), k, den);
					if (RingSegment(cnt) == RingSegment(cnt + 1)) fade = lerp_i(fade, RingFade(cnt + 1), k, den);
				}
				const bool second = r.kind == REC_RING2;
				RingDraw(c, second ? -3 : -2, second ? RING_VERTS_B : RING_VERTS_A, fade);
				break;
			}
			default:
			{
				// the sparkles as drawn (still)
				const uint32_t h = P(FieldAlloc(0xB4));
				U32(h, 0) = SEQ_Spark;
				U16(h, 0x24) = 2;
				S32(h, 0x14) = 0x1000;
				for (int q = r.first; q < r.first + r.count; q++)
				{
					uint8_t pc[0x18];
					memcpy(pc, g_parts[q].copy, sizeof(pc));
					SparkDraw(h, P(pc));
				}
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_held = false;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
	}

	static void register_mag172_held()
	{
		mark_held(ORIG_Tint);
		mark_held(ORIG_Ring);
		mark_held(ORIG_Ring2);
		mark_held(ORIG_Sparks);
		register_module_held(172, HeldReady, HeldFrame);
	}
}
}
