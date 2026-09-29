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

// 30 fps part of mag_cerberus.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_cerberus.cpp)
//
// Held-frame behaviour of the Cerberus handlers (the engine draws the held frames):
//   Draw 45 ScreenMelt: dh_45_ScreenMeltHeld below (installed over the port in the clone's draw
//     table; real ticks run the port unchanged). The vanilla draw integrates every column
//     (speed += accel, bottom += speed, past the fall limit the top follows) and counts the red
//     fade up AFTER drawing, so the frozen path alone drew the columns where the real tick drew
//     them (held still) with the NEXT tick's fade colour. Held: the next tick's integration step
//     is computed per column, the columns are set to num/den of the way (8.8 fixed point) for
//     the frozen draw, and the fade colour words of the packets it emitted are set to the
//     in-between of the real tick's colour (counter - 1) and the next tick's (counter). The
//     column flags (gone / cap line) stay the real tick's: they switch on the real frame. The
//     block is put back after the draw. A first call on a held frame (bone+0xBC still 0) runs
//     the port (allocate + seed as vanilla; HeldFrame restores the arena, the bones and the CRT
//     seed, so nothing of it persists).
//   Prim 28 / 38 / 39: pure draw code.

namespace ff8fx
{
namespace gfc
{
namespace cerberus
{
	static inline int32_t melt_mix(int32_t a, int32_t b, int num, int den) { return a + (int32_t)((int64_t)(b - a) * num / den); }

	// fade colour word of the melt (vanilla: n = min(counter, 15), rgb 0x80+8n / 0x80-8n / 0x80-8n)
	static uint32_t melt_colour(int32_t fade)
	{
		if (fade >= 0xF) fade = 0xF;
		uint32_t e8 = (uint32_t)fade * 8, a = 0x80u - e8, c = e8 + 0x80u;
		return 0x3C000000u | (a << 16) | (a << 8) | (c & 0xFFFFFFu);
	}

	static uint8_t g_melt_save[0x2810];

	static void __cdecl dh_45_ScreenMeltHeld()
	{
		uint8_t *blk = PTR(CUR(), 0xBC);
		if (!held_active() || blk == nullptr || g_held.den <= 0)
		{
			part_cerberus::dh_45_ScreenMelt();
			return;
		}
		const int num = g_held.num, den = g_held.den;
		const bool adv = g_held.real_skip == 0;
		memcpy(g_melt_save, blk, sizeof(g_melt_save));
		if (adv)
		{
			// the next tick's step of every column (vanilla non-frozen path), num/den of the way
			const int32_t lim = (int32_t)((uint32_t)U16(PTR(CUR(), 0xB8), 0x14) << 8);
			uint8_t *col = blk + 0x10;
			for (int i = 0; i < 0x140; i++, col += 0x20)
			{
				uint16_t spd = (uint16_t)(U16(col, 4) + U16(col, 6));
				int32_t bot = add32(S32(col, 8), (int16_t)spd);
				uint16_t tsp = bot >= lim ? spd : U16(col, 0xC);
				tsp = (uint16_t)(tsp + U16(col, 0xE));
				int32_t top = add32(S32(col, 0x10), (int16_t)tsp);
				S32(col, 8) = melt_mix(S32(col, 8), bot, num, den);
				S32(col, 0x10) = melt_mix(S32(col, 0x10), top, num, den);
			}
		}
		uint8_t *pkt0 = PTR(CTX(), 0x7C);
		part_cerberus::dh_45_ScreenMelt();
		uint8_t *pkt1 = PTR(CTX(), 0x7C);
		// fade: the real tick drew with the counter before its increment
		const int32_t f1 = S16(blk, 0), f0 = adv ? f1 - 1 : f1;
		const uint32_t c0 = melt_colour(f0), c1 = melt_colour(f1);
		uint32_t cc = 0x3C000000u;
		for (int s = 0; s < 24; s += 8)
			cc |= (uint32_t)(melt_mix((c0 >> s) & 0xFF, (c1 >> s) & 0xFF, num, den) & 0xFF) << s;
		// the packets of this draw: POLY_GT4 (len 0xC, 0x34 bytes: colours +4 / +0x10 = the fade
		// word) each optionally followed by its cap line (len 6, 0x1C bytes)
		for (uint8_t *p = pkt0; p < pkt1;)
		{
			if (U8(p, 3) == 0xC)
			{
				U32(p, 4) = cc;
				U32(p, 0x10) = cc;
				p += 0x34;
			}
			else p += 0x1C;
		}
		memcpy(blk, g_melt_save, sizeof(g_melt_save));
	}

	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_cerberus_held()
	{
		using namespace gfc;
		mark_held(0xB0C820);
		cerberus::g_c.draw[45] = cerberus::dh_45_ScreenMeltHeld;
		register_module_held(203, cerberus::Ready, cerberus::Frame);
		register_module_camera(203, cerberus::Camera);
	}
}
