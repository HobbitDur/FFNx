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

// 30 fps part of mag169_gastric_juice.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag169_gastric_juice.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing Gastric Juice task draws first and updates afterwards. A real tick records each
// drawing task in order: its node, the droplet table as it was drawn (drip emitter / splash), the
// GTE registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes
// the code does not write). A held frame replays the log in the same order, each part moved
// num / den of the way to the state the next tick draws:
//   Blob          - between the curve sample drawn and the one its speed reaches (hold on its last
//                   tick); its flipbook frame's squash / stretch toward the next frame's (the
//                   texture holds); scale and first-tick black hold.
//   Landing mesh  - vertex blend weight between sin(tick * 32) and sin((tick + 1) * 32) (the
//                   vertices are linear in it), fade along its ramp inside a segment (the renderer
//                   changes at tick 16: hold); vertices go to a private copy of its buffer.
//   Puddle        - scale along its step, fade along its ramp.
//   Droplets      - a droplet that survives the tick moves toward its next position (drips: y,
//                   splash: x, y, z; the splash frame's scale / rotation / colour toward the next
//                   frame's); one that ends holds; textures (flipbook frames) hold.
//   Tinted target - fade along its ramp (hold on its last tick), the model at its in-between pose
//                   (pose_midpoint); its shadow is redrawn as it is.
//   At fraction 1 (num == den) the frame is the next tick's: every recorded part as its own update
//   left it (ended parts draw nothing), the droplet pools as they are now (new droplets and
//   flipbook frames included).
// The model draw's side effects are undone (frame packets to a private copy of the frame arena,
// vertex buffer 0x1D98B3C to a private copy, shadow / geometry temporaries put back, bone matrices
// rebuilt from the real pose); the splash matrix translation (0x23B3C04) is put back; the module
// packets go to a private buffer (HeldScope). Director and streams draw nothing.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.

namespace ff8fx
{
namespace gastric169
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

	static int MeshSegment(int32_t c) { return c < 8 ? 0 : (c < 0x10 ? 1 : 2); }

	// Flipbook frames of different shapes (the blob's squash / stretch scale, the splash's scale,
	// rotation and colour): a held frame draws each frame's texture with its part records moved
	// num / den of the way to the next frame's (scale x / y, rotation, colour G byte; a frame
	// without the scale flag has scale 0x1000), so every sprite corner lies between its two real
	// draws. The sequence is copied (frame offsets are relative to it) with every frame f turned
	// toward frame f + 1 (cyclic: toward frame 0 after the last one; else the last frame stays).
	static uint8_t g_seq_blob[0x400], g_seq_splash[0x400];

	static uint32_t SeqFrame(uint32_t seq, int32_t f)
	{
		const int32_t n = U16(seq, 8);
		if (f < 0 || f >= n) return 0;
		const int16_t off = S16(seq, 0xA + f * 2);
		return off < 0 ? 0 : seq + (uint32_t)off;
	}

	static uint32_t HeldSeq(uint32_t seq, uint8_t *out, uint32_t out_size, bool cyclic)
	{
		const int32_t n = U16(seq, 8);
		uint32_t size = 0xC + (uint32_t)n * 2;
		for (int32_t f = 0; f < n; f++)
		{
			const uint32_t fr = SeqFrame(seq, f);
			if (!fr) break;
			const uint32_t end = fr - seq + 4 + (U32(fr, 0) & 0x7FFFFFFF) * 0x14;
			if (end > size) size = end;
		}
		if (size > out_size) return seq;
		memcpy(out, (const void *)seq, size);
		for (int32_t f = 0; f < n; f++)
		{
			const uint32_t a = SeqFrame(seq, f);
			const uint32_t b = SeqFrame(seq, (f + 1 < n) ? f + 1 : (cyclic ? 0 : -1));
			if (!a || !b || (U32(a, 0) & 0x7FFFFFFF) != (U32(b, 0) & 0x7FFFFFFF)) continue;
			const uint32_t o = P(out) + (a - seq);
			for (uint32_t k = 0; k < (U32(a, 0) & 0x7FFFFFFF); k++)
			{
				const uint32_t ra = a + 4 + k * 0x14, rb = b + 4 + k * 0x14, ro = o + 4 + k * 0x14;
				const bool sa = (U16(ra, 0xE) & 0xC00) != 0, sb = (U16(rb, 0xE) & 0xC00) != 0;
				if (sa || sb)
				{
					const int32_t ax = sa ? S16(ra, 0x10) : 0x1000, ay = sa ? S16(ra, 0x12) : 0x1000;
					const int32_t bx = sb ? S16(rb, 0x10) : 0x1000, by = sb ? S16(rb, 0x12) : 0x1000;
					U16(ro, 0xE) = (uint16_t)(U16(ra, 0xE) | (sa ? 0 : 0x400)); // bits 10-11 never reach the packet (tpage & 0x1FF)
					S16(ro, 0x10) = (int16_t)lerp_i(ax, bx, g_hnum, g_hden);
					S16(ro, 0x12) = (int16_t)lerp_i(ay, by, g_hnum, g_hden);
					S16(ro, 0xC) = lerp_angle(S16(ra, 0xC), S16(rb, 0xC), g_hnum, g_hden);
				}
				U8(ro, 1) = (uint8_t)lerp_i(U8(ra, 1), U8(rb, 1), g_hnum, g_hden);
			}
		}
		return P(out);
	}

	static void HeldFrame(int num, int den)
	{
		bool tint = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].fn == ORIG_Tint) tint = true;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		ModelScope model(tint && g_frame_tick == g_real_tick);
		uint8_t splash_save[sizeof(Mat4x3)];
		memcpy(splash_save, (const void *)SPLASH_MATRIX, sizeof(splash_save));
		const bool moving = !DrawOnly();
		g_held = true;
		g_hnum = moving ? num : 0;
		g_hden = moving ? den : 0;
		const bool one = moving && num == den;
		// at fraction 1 the packets go where the next tick writes them: the other arena, whose
		// bytes the packets do not write (pad halves) are the ones the next tick finds there
		if (num == den && g_packets && TexBase()) memcpy(g_held_packets, (const void *)(g_packets == TexBase() ? TexBase() + 0x8000 : TexBase()), 0x8000);
		// morphed flipbooks (not at fraction 0 / 1, nor in the draw-only mode)
		const bool morph = g_hden && num > 0 && !one;
		const uint32_t seq_blob = morph ? HeldSeq(SEQ_Blob, g_seq_blob, sizeof(g_seq_blob), true) : SEQ_Blob;
		const uint32_t seq_splash = morph ? HeldSeq(SEQ_Splash, g_seq_splash, sizeof(g_seq_splash), false) : SEQ_Splash;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			Node a = r.node;
			const int32_t c = a.counter;
			switch (r.fn)
			{
			case ORIG_Tint:
			{
				if (one)
				{
					// the next tick's draw (none once it ends)
					if (c + 1 >= 0x16) break;
					a.counter = (int16_t)(c + 1);
					TintDraw(&a, TintFade(c + 1));
					break;
				}
				int32_t fade = TintFade(c);
				if (c + 1 < 0x16) fade = hlerp(fade, TintFade(c + 1));
				else g_hden = 0; // last draw: the pose holds too
				TintDraw(&a, fade);
				g_hden = moving ? den : 0;
				break;
			}
			case ORIG_Mesh:
			{
				memcpy(g_held_mesh, (const void *)(MESH_VERTS + (uint32_t)(a.idx * 0x248)), sizeof(g_held_mesh));
				if (one)
				{
					if (c + 1 >= 0x20) break;
					a.counter = (int16_t)(c + 1);
					MeshDraw(&a, P(g_held_mesh), MeshBlend(c + 1), MeshFade(c + 1));
					break;
				}
				int32_t s = MeshBlend(c), fade = MeshFade(c);
				if (c + 1 < 0x20)
				{
					s = hlerp(s, MeshBlend(c + 1));
					if (MeshSegment(c) == MeshSegment(c + 1)) fade = hlerp(fade, MeshFade(c + 1));
				}
				MeshDraw(&a, P(g_held_mesh), s, fade);
				break;
			}
			case ORIG_Puddle:
			{
				if (one)
				{
					if (c + 1 >= 0x10) break;
					a.counter = (int16_t)(c + 1);
					a.a1c = (int16_t)(a.a1c + a.a1e);
					PuddleDraw(&a, PuddleFade(c + 1));
					break;
				}
				int32_t fade = PuddleFade(c);
				if (c + 1 < 0x10)
				{
					fade = hlerp(fade, PuddleFade(c + 1));
					a.a1c = (int16_t)hlerp(a.a1c, (int16_t)(a.a1c + a.a1e));
				}
				PuddleDraw(&a, fade);
				break;
			}
			case ORIG_Blob:
			{
				const int16_t *p0 = StreamOf(a.idx)->curve[0] + a.a18 * 4;
				int16_t pos[4] = { p0[0], p0[1], p0[2], p0[3] };
				const int32_t k = (int16_t)(a.a18 + a.a1a);
				const int16_t *p1 = StreamOf(a.idx)->curve[0] + k * 4;
				if (one)
				{
					if (k >= 0x1B) break;
					a.counter = (int16_t)(c + 1);
					BlobDraw(&a, p1, SEQ_Blob);
					break;
				}
				if (k >= 0x1B)
				{
					BlobDraw(&a, pos, SEQ_Blob); // its last draw: holds
					break;
				}
				for (int w = 0; w < 3; w++) pos[w] = (int16_t)hlerp(p0[w], p1[w]);
				BlobDraw(&a, pos, seq_blob);
				break;
			}
			case ORIG_Drips:
			{
				const uint32_t h = P(FieldAlloc(0xB4));
				U32(h, 0) = SEQ_Drip;
				U16(h, 0x24) = 0;
				for (int k = 0; k < 100; k++)
				{
					// at fraction 1: the pool as the next tick draws it
					Drop e = one ? Drops()[k] : r.drops[k];
					if (!(e.flags & (uint32_t)(int32_t)a.idx)) continue;
					if (!one && Survived(&r.drops[k], k)) e.pos[1] = (int16_t)hlerp(e.pos[1], Drops()[k].pos[1]);
					DripDraw(h, &e);
				}
				FieldFree(0xB4);
				break;
			}
			case ORIG_Splash:
			{
				const uint32_t h = P(FieldAlloc(0xB4));
				U32(h, 0) = SEQ_Splash;
				U16(h, 0x24) = 0;
				SplashSetup(StreamOf(a.idx));
				for (int k = 0; k < 100; k++)
				{
					Drop e = one ? Drops()[k] : r.drops[k];
					if (!(e.flags & 0x100)) continue;
					const bool moved = !one && Survived(&r.drops[k], k);
					if (moved)
						for (int w = 0; w < 3; w++) e.pos[w] = (int16_t)hlerp(e.pos[w], Drops()[k].pos[w]);
					U32(h, 0) = moved ? seq_splash : SEQ_Splash; // a droplet that ends holds its frame
					SplashDraw(h, &e);
				}
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_hden = 0;
		g_hnum = 0;
		g_held = false;
		memcpy((void *)SPLASH_MATRIX, splash_save, sizeof(splash_save));
	}
}

	static void register_mag169_held()
	{
		mark_held(gastric169::ORIG_Tint);
		mark_held(gastric169::ORIG_Mesh);
		mark_held(gastric169::ORIG_Puddle);
		mark_held(gastric169::ORIG_Blob);
		mark_held(gastric169::ORIG_Drips);
		mark_held(gastric169::ORIG_Splash);
		register_module_held(169, gastric169::HeldReady, gastric169::HeldFrame);
	}
}
