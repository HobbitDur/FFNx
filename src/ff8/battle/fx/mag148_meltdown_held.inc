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

// 30 fps part of mag148_meltdown.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag148_meltdown.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Melting target - the target model redrawn with the melt strength half way to the next tick's
//                    (its pose is the engine's, the melt plane is fixed); its shadow is redrawn as
//                    it is; the frame packet cursor and the engine's projected / melted vertex
//                    buffer (0x1D98B3C) are private, the shadow and model-geometry temporaries are
//                    put back.
//   Disc           - size, UV scroll and fade half way (the fade only while both ticks fade).
//   Caster glow    - size (x / y, and the second model's z) half way along its decelerating
//                    step, UV scroll and fade half way.
//   Acid particles - position half way along the next tick's move (pool A: straight; pool B:
//                    outwards, then back decelerating, velocity x16 at tick 0x23); the flipbook
//                    frame and the size hold; a particle that dies on the next tick holds.
// The last tick of a task (it ends after its draw) holds; in the draw-only mode
// (battle_to_update_flags & 0x201) nothing moves: everything holds. The scale matrix 0x2476640
// the draws rewrite is put back.

namespace ff8fx
{
namespace melt148
{
	// real tick on which the ported root task last ran
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<1024> g_draws;
	static uint8_t g_held_packets[0x40000];
	static uint8_t g_held_frame_packets[0x40000];
	static uint8_t g_held_work[0x20000];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
	}

	static void held_note_draw(uint32_t fn, uint32_t node) { g_draws.add(fn, (const void *)node, PacketCursor()); }

	// a particle: its pool entry and its task's counter / action (+0x14 / +0x16); the sprite
	// header its task set up (shared by its particles) as it is at this particle's draw
	static void held_note_particle(uint32_t fn, uint32_t entry, uint32_t node, uint32_t header)
	{
		uint8_t rec[NODE_BYTES] = {};
		memcpy(rec, (const void *)entry, 0x14);
		memcpy(rec + 0x14, (const void *)(node + 0xC), 4);
		const int n = g_draws.n;
		g_draws.add(fn, rec, PacketCursor());
		g_hdr_slot = nullptr;
		if (g_draws.n > n) memcpy(g_draws.r[n].hdr, (const void *)header, 0xB4);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	// fade (+0x0C with the 0xF0 flags) of the disc / caster glow at tick c; false = no fade
	static bool DiscFade(int32_t c, int32_t *fade)
	{
		if ((int16_t)c < 8) { *fade = shl32(8 - c, 9); return true; }
		if ((int16_t)c >= 0x41) { *fade = shl32(c - 0x41, 9); return true; }
		return false;
	}
	static bool GlowFade(int32_t c, int32_t *fade)
	{
		if ((int16_t)c < 6) { *fade = 0x1000 - shl32(c * 341, 1); return true; }
		if ((int16_t)c >= 0x20) { *fade = shl32(c - 0x20, 9); return true; }
		return false;
	}

	// DiscDraw at tick c + num/den
	static void DiscHeld(uint32_t node, int num, int den)
	{
		const int32_t c = S16(node, 0xC);
		const uint16_t s = (uint16_t)lerp_i(U16(node, 0x1C), (uint16_t)(U16(node, 0x1C) + U16(node, 0x1E)), num, den);
		SetScaledFrame(S16(node, 0xE), s, s, false);
		const uint32_t hdr = P(AllocHeader(0x7C));
		U32(hdr, 0) = MODEL_Disc;
		U32(hdr, 0x20) = 0x30;
		U32(hdr, 0x10) = 0;
		U16(hdr, 0x18) = 0;
		U32(hdr, 0x14) = (uint32_t)lerp_i(shl32(-(c * 3), 2), shl32(-((c + 1) * 3), 2), num, den);
		U16(hdr, 0x1A) = 0x80;
		U16(hdr, 0x1C) = 0x80;
		U16(hdr, 0x1E) = 0x80;
		U8(hdr, 0xA) = 0;
		U8(hdr, 9) = 0;
		U8(hdr, 8) = 0;
		int32_t f0, f1;
		if (DiscFade(c, &f0))
		{
			U32(hdr, 0xC) = (uint32_t)(DiscFade(c + 1, &f1) ? lerp_i(f0, f1, num, den) : f0);
			U32(hdr, 0x20) = 0xF0;
		}
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	// CasterGlowDraw at tick c + num/den
	static void CasterGlowHeld(uint32_t node, int num, int den)
	{
		const int32_t c = S16(node, 0xC);
		const uint16_t s0 = U16(node, 0x1C);
		const uint16_t s = (int16_t)c >= 0 ? (uint16_t)lerp_i(s0, (uint16_t)(s0 + U16(node, 0x1E)), num, den) : s0;
		U16(SCALE_MATRIX, 0x10) = 0x4000;
		SetScaledFrame(S16(node, 0xE), s, 0x4000, false);
		const uint32_t hdr = P(AllocHeader(0x7C));
		U32(hdr, 0) = MODEL_GlowFlat;
		U32(hdr, 0x20) = 0x30;
		U32(hdr, 0x10) = (uint32_t)lerp_i(shl32(c * 3, 3), shl32((c + 1) * 3, 3), num, den);
		U32(hdr, 0x14) = 0;
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1C) = 0x80;
		U16(hdr, 0x1E) = 0x80;
		U8(hdr, 0xA) = 0;
		U8(hdr, 9) = 0;
		U8(hdr, 8) = 0;
		int32_t f0, f1;
		if (GlowFade(c, &f0))
		{
			U32(hdr, 0xC) = (uint32_t)(GlowFade(c + 1, &f1) ? lerp_i(f0, f1, num, den) : f0);
			U32(hdr, 0x20) = 0xF0;
		}
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		SetScaledFrame(S16(node, 0xE), s, s, true);
		U16(hdr, 0x18) = 0x40;
		U16(hdr, 0x1C) = 0x40;
		U32(hdr, 0) = MODEL_Glow;
		U32(hdr, 0x10) = 0;
		U32(hdr, 0x14) = (uint32_t)lerp_i(shl32(c, 3), shl32(c + 1, 3), num, den);
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0x80;
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	// the melting target at tick c + num/den, with private vertex / packet buffers; what the
	// shadow and the geometry renderer leave in engine temporaries is put back
	static void MeltHeld(uint32_t node, bool moving, int num, int den)
	{
		const int32_t c = S16(node, 0xC);
		int32_t melt = MeltStrength(c);
		if (moving) melt = lerp_i(melt, MeltStrength(c + 1), num, den);
		const uint32_t work = var<uint32_t>(0x1D98B3C);
		const uint32_t frame = FrameCursor();
		const uint32_t geom[2] = { var<uint32_t>(0x1D99C18), var<uint32_t>(0x1D99C1C) };
		static uint8_t shadow[0x80];
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { var<uint32_t>(0xB8B9E4), var<uint32_t>(0xB8B9EC) };
		var<uint32_t>(0x1D98B3C) = P(g_held_work);
		FrameCursor() = P(g_held_frame_packets);
		MeltDraw(node, melt, true);
		var<uint32_t>(0xB8B9E4) = shadow_words[0];
		var<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		var<uint32_t>(0x1D99C18) = geom[0];
		var<uint32_t>(0x1D99C1C) = geom[1];
		FrameCursor() = frame;
		var<uint32_t>(0x1D98B3C) = work;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		uint8_t scale[0x20];
		memcpy(scale, (const void *)SCALE_MATRIX, sizeof(scale));
		const bool moving = !DrawOnly();
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			alignas(4) uint8_t a[NODE_BYTES];
			memcpy(a, r.node, sizeof(a));
			const uint32_t node = P(a);
			switch (r.fn)
			{
			case ORIG_Melt:
				MeltHeld(node, moving, num, den);
				break;
			case ORIG_Disc:
				if (!moving || S16(node, 0xC) + 1 >= 0x49) DiscDraw(node);
				else DiscHeld(node, num, den);
				break;
			case ORIG_CasterGlow:
				if (!moving || S16(node, 0xC) + 1 >= 0x28) CasterGlowDraw(node);
				else CasterGlowHeld(node, num, den);
				break;
			case ORIG_ParticlesA:
			case ORIG_ParticlesB:
			{
				g_hdr_slot = nullptr;
				alignas(4) uint8_t e[0x14];
				memcpy(e, a, sizeof(e));
				if (moving)
				{
					alignas(4) uint8_t b[0x14];
					memcpy(b, a, sizeof(b));
					if (r.fn == ORIG_ParticlesA) ParticleAUpdate(P(b));
					else ParticleBUpdate(P(b), S16(node, 0x14));
					if (S16(P(b), 0) >= 0)
						for (int k = 0; k < 3; k++) S16(P(e), 4 + 2 * k) = lerp16(S16(node, 4 + 2 * k), S16(P(b), 4 + 2 * k), num, den);
				}
				const uint32_t h = P(FieldAlloc(0xB4));
				memcpy((void *)h, r.hdr, 0xB4);
				ParticleDraw(h, P(e));
				FieldFree(0xB4);
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
		memcpy((void *)SCALE_MATRIX, scale, sizeof(scale));
	}
}

	static void register_mag148_held()
	{
		mark_held(melt148::ORIG_Melt);
		mark_held(melt148::ORIG_Disc);
		mark_held(melt148::ORIG_CasterGlow);
		mark_held(melt148::ORIG_ParticlesA);
		mark_held(melt148::ORIG_ParticlesB);
		register_module_held(148, melt148::HeldReady, melt148::HeldFrame);
	}
}
