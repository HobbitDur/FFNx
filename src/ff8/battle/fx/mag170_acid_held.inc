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

// 30 fps part of mag170_acid.cpp (Acid, effect 170; the design of Gastric Juice 169's mag169_gastric_juice_held.inc).
//
// Every drawing task draws first and updates afterwards. A real tick records each drawing task in order: its node, the droplet
// table as it was drawn (drip emitter / splash), the GTE registers and the Field_Alloc scratch it started from. A held frame
// replays the log in the same order with the vanilla draw code, each part num / den of the way to the state its next tick draws
// (a part whose task ended, or whose counter did not move (draw-only tick), holds):
//   Blob          - each of its 4 sprites along the curve from its sample to the next tick's (between samples: the straight
//                   line between the two around it); flipbook frame (tick % 6) and radii held.
//   Landing mesh  - vertex blend weight between sin(tick * 32) and sin((tick + 1) * 32) (the vertices are linear in it), scale
//                   along its step (ticks 0..7), fade along its ramp inside a segment (0..7 / 8..15 / 16..23 / 24..31: the
//                   renderer and the ramp change at the segment's end: hold); vertices to a private buffer.
//   Puddle        - scale along its step, fade along its ramp (continuous at tick 8).
//   Droplets      - a droplet the tick's update kept moves toward its next position (drips: y, splash: x, y, z; the splash
//                   grey toward the next frame's); one that ends holds; flipbook frames (frame / 2) hold; new ones appear on
//                   the real frame.
//   Melting target - the melt amount along its ramp (held on its last drawn tick), the model at its in-between pose
//                   (pose_midpoint); its shadow redrawn as it is.
// The model draw's side effects are undone (frame packets to a private copy of the frame arena, vertex buffer 0x1D98B3C to a
// private copy, shadow / geometry temporaries put back, bone matrices rebuilt from the real pose); the splash matrix
// (0x23B1350) is put back; the module packets go to a private buffer (HeldScope). Director and stream draw nothing.

namespace ff8fx
{
namespace acid170
{
	static const uint32_t SCRATCH = 0x400;

	struct Rec
	{
		uint32_t fn;
		Node node;
		Drop drops[100];           // droplet table as drawn (drip emitter / splash)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static bool g_overflow = false;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_tick = 0xFFFFFFFF;
	static uint32_t g_frame_packets = 0; // frame packet cursor at the tick's first model draw
	static Rec g_recs[64];
	static const Node *g_recptr[64];        // the drawn nodes (their live state on the held frame)
	static uint8_t g_held_packets[0x10000]; // module arena (0x8000 per tick)
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_verts[0x10000];
	static uint8_t g_held_mesh[0x248];
	static bool g_held = false;
	static int g_hnum = 0, g_hden = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_overflow = false;
		g_packets = 0;
	}

	static void held_note_draw(uint32_t fn, const Node *node)
	{
		if (g_held || g_log_tick != g_real_tick) return;
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0])))
		{
			g_overflow = true;
			return;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		g_recptr[g_nrec] = node;
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = *node;
		if (fn == ORIG_Drips || fn == ORIG_Splash) memcpy(r.drops, (const void *)DROPS, sizeof(r.drops));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	// the target model: at its in-between pose on a held frame; the real tick notes where the
	// frame arena was at its first model draw
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
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick && !g_overflow; }

	static int32_t hlerp(int32_t a, int32_t b) { return g_hden ? lerp_i(a, b, g_hnum, g_hden) : a; }

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
			// the model's faces read the vertex buffer's pad words (never written by the draw): the
			// private buffer starts as a copy of the real one
			const uint32_t e = P(Entity(TargetSlot()));
			uint32_t n = MaxObjectVertices(U32(e + 0x60, 4), U32(e, 0x7C));
			const uint32_t weapon = U32(e, 0x78);
			if (weapon && !(U8(e, 1) & 2))
			{
				const uint32_t k = MaxObjectVertices(U32(weapon, 4), 0xFFFFFFFF);
				if (k > n) n = k;
			}
			n *= 8;
			if (verts) memcpy(g_held_verts, (const void *)verts, n < sizeof(g_held_verts) ? n : sizeof(g_held_verts));
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

	// a droplet drawn on the real tick that the tick's update kept (same owner, next frame)
	static bool Survived(const Drop *drawn, int i)
	{
		const Drop &live = Drops()[i];
		return live.flags == drawn->flags && live.frame == (int16_t)(drawn->frame + 1);
	}

	static int MeshSegment(int32_t c) { return c < 8 ? 0 : (c < 0x10 ? 1 : (c < 0x18 ? 2 : 3)); }

	static void HeldFrame(int num, int den)
	{
		bool melt = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].fn == ORIG_Melt) melt = true;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		ModelScope model(melt && g_frame_tick == g_real_tick);
		uint8_t splash_save[sizeof(Mat4x3)];
		memcpy(splash_save, (const void *)SPLASH_MATRIX, sizeof(splash_save));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		const bool moving = !DrawOnly();
		g_held = true;
		g_hnum = moving ? num : 0;
		g_hden = moving ? den : 0;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			Node a = r.node;
			const int32_t c = a.counter;
			// the live node (the same pool node, its task still running and its counter one on)
			const Node *live = (const Node *)P(g_recptr[i]);
			const bool next = moving && live->hdr.func == (void *)r.fn && (live->hdr.flags & 1) && live->counter == (int16_t)(c + 1);
			const int k = next ? num : 0;
			switch (r.fn)
			{
			case ORIG_Melt:
			{
				int32_t amount = MeltAmount(c);
				if (k && c + 1 < 0x22) amount = lerp_i(amount, MeltAmount(c + 1), k, den);
				g_hnum = k && c + 1 < 0x22 ? num : 0;
				g_hden = k && c + 1 < 0x22 ? den : 0;
				MeltDraw(&a, amount);
				g_hnum = moving ? num : 0;
				g_hden = moving ? den : 0;
				break;
			}
			case ORIG_Mesh:
			{
				int32_t s = MeshBlend(c), fade = MeshFade(c);
				if (k && c + 1 < 0x20)
				{
					s = lerp_i(s, MeshBlend(c + 1), k, den);
					if (MeshSegment(c) == MeshSegment(c + 1)) fade = lerp_i(fade, MeshFade(c + 1), k, den);
					if (c < 8) a.a1c = lerp16(a.a1c, live->a1c, k, den);
				}
				MeshDraw(&a, P(g_held_mesh), s, fade);
				break;
			}
			case ORIG_Puddle:
			{
				int32_t fade = PuddleFade(c);
				if (k)
				{
					fade = lerp_i(fade, PuddleFade(c + 1), k, den);
					a.a1c = lerp16(a.a1c, live->a1c, k, den);
				}
				PuddleDraw(&a, fade);
				break;
			}
			case ORIG_Blob:
			{
				// each sprite num / den of the way along the curve from its sample to the next tick's (between samples: the
				// straight line between the two around it)
				const int32_t s0 = a.a18, s1 = k ? live->a18 : s0;
				int16_t pts[4][4];
				const int16_t *pos[4];
				for (int j = 0; j < 4; j++)
				{
					const int32_t f = (s0 - j) * den + (s1 - s0) * k;
					const int32_t q = f / den, fr = f % den;
					pos[j] = StreamOf(a.idx)->curve[0] + (s0 - j) * 4;
					if (q - (s0 - j) == 0 && !fr) continue;
					if (q < 0 || q >= 0x1C) continue;
					memcpy(pts[j], StreamOf(a.idx)->curve[q], 8);
					if (fr && q + 1 < 0x1C)
						for (int w = 0; w < 3; w++) pts[j][w] = lerp16(StreamOf(a.idx)->curve[q][w], StreamOf(a.idx)->curve[q + 1][w], fr, den);
					pos[j] = pts[j];
				}
				BlobDraw(&a, pos);
				break;
			}
			case ORIG_Drips:
			case ORIG_Splash:
			{
				const bool splash = r.fn == ORIG_Splash;
				const uint32_t h = P(FieldAlloc(0xB4));
				U32(h, 0) = splash ? SEQ_Blob : SEQ_Drip;
				U16(h, 0x24) = splash ? 4 : 0;
				if (splash) SplashSetup();
				for (int j = 0; j < 100; j++)
				{
					Drop e = r.drops[j];
					if (!(e.flags & (uint32_t)(int32_t)a.idx)) continue;
					const bool moved = moving && Survived(&r.drops[j], j);
					int32_t grey = SplashGrey(e.frame);
					if (moved)
					{
						const Drop &l = Drops()[j];
						if (splash)
						{
							for (int w = 0; w < 3; w++) e.pos[w] = lerp16(e.pos[w], l.pos[w], num, den);
							grey = lerp_i(grey, SplashGrey(e.frame + 1), num, den);
						}
						else e.pos[1] = lerp16(e.pos[1], l.pos[1], num, den);
					}
					if (splash) SplashDraw(h, &e, grey);
					else DripDraw(h, &e);
				}
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_hden = 0;
		g_hnum = 0;
		g_held = false;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)SPLASH_MATRIX, splash_save, sizeof(splash_save));
	}

	static void register_mag170_held()
	{
		mark_held(ORIG_Melt);
		mark_held(ORIG_Mesh);
		mark_held(ORIG_Puddle);
		mark_held(ORIG_Blob);
		mark_held(ORIG_Drips);
		mark_held(ORIG_Splash);
		register_module_held(170, HeldReady, HeldFrame);
	}
}
}
