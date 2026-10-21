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

// 30 fps part of mag062_devour.cpp: the picture memo of the real tick and the held-frame drawing (included at the end
// of mag062_devour.cpp)
//
// Held frames (30 fps): the only thing Devour draws is the picture task (0x7E4F60): two screen-space layers (the
// picture 0x7E5010 and the title strip 0x7E51C0, textured quads 0x7E5090) that it updates THEN draws. Nothing follows a
// model or the camera (fixed screen positions): the camera, the bone follow and the emitter anchors play no part.
// A real tick notes, per picture task in queue order, the state it ran (drawn or not) and its node after the tick
// (scroll +0x1C, brightness +0x50, state). A held frame draws each one at the exact in-between of the state it drew
// with and the state its next tick draws with (the task's own deterministic step: scroll -4; in the fade-out state
// brightness -0x10, nothing drawn once it reaches 0): scroll and brightness half way at 1/2. A picture that the next
// tick no longer draws (its last fade step) holds its last picture and draws nothing at fraction 1; one that starts
// drawing on the next tick (end of its wait) appears on the real frame (drawn only at fraction 1).
// The packets go to a private buffer (module cursor 0x260131C) that starts with the real tick's packets from the
// cursor of the tick's arena; the cursor is put back. The quads use neither the GTE nor the Field_Alloc scratch.
// 15 Hz by design (logic only, nothing drawn by the module): the darkening task 0x7E5290 (the four fade layers'
// colour bytes and level words 0x1D98992 + k * 0x2C, steps of 0x200; the entities' tint +0x28..+0x2A / +0x2B / +7 and
// flags 0x800 / 4) and the music volume (Music_SetVolumeImmediate, steps of 8) change on real ticks only.

namespace ff8fx
{
namespace act
{
namespace devour
{
	static const int MAX_PICTURES = 8;

	// a picture task as the real tick left it
	struct PicRec
	{
		uint32_t node;
		bool drew;        // it drew on this tick (state 1, or state 2 still above brightness 0)
		int8_t state;     // its state after the tick
		int16_t scroll;   // +0x1C after the tick (what it drew with)
		int16_t bright;   // +0x50 after the tick
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_packets = 0;       // module packet cursor of the tick (the master's arena pick)
	static int g_npic = 0;
	static PicRec g_pics[MAX_PICTURES];
	static uint8_t g_held_packets[0x1000];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_npic = 0;
		g_packets = MEM<uint32_t>(PACKET_CURSOR);
	}

	static void held_note_picture(uint32_t node, int8_t state, uint32_t r)
	{
		(void)r;
		if (g_ported_tick != g_real_tick || g_npic >= MAX_PICTURES)
			return;
		PicRec &p = g_pics[g_npic++];
		p.node = node;
		p.state = S8(node, 0x29);
		p.scroll = S16(node, 0x1C);
		p.bright = S16(node, 0x50);
		p.drew = state == 1 || (state == 2 && p.state == 2);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the picture's next tick (its task's step): whether it draws, and the scroll / brightness it draws with
	static bool NextDraw(const PicRec &p, int16_t *scroll, int16_t *bright)
	{
		if (p.state == 1)
		{
			*scroll = (int16_t)(p.scroll - 4);
			*bright = p.bright;
			return true;
		}
		if (p.state == 2)
		{
			const int16_t b = (int16_t)(p.bright - 0x10);
			if (b <= 0)
				return false;
			*scroll = (int16_t)(p.scroll - 4);
			*bright = b;
			return true;
		}
		return false;
	}

	// the picture task's draws (strip, then picture) for a scroll / brightness
	static void HeldPicture(int16_t scroll, int16_t bright)
	{
		alignas(4) uint8_t n[0x54] = {};
		S16(P(n), 0x1C) = scroll;
		S16(P(n), 0x50) = bright;
		Strip(P(n));
		Picture(P(n));
	}

	static void HeldFrame(int num, int den)
	{
		if (g_npic == 0)
			return;
		uint32_t &cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t saved = cursor;
		// the real tick's packets first: the held quads are written over them at the same offsets (pad bytes kept)
		if (g_packets && saved > g_packets)
		{
			const uint32_t used = saved - g_packets;
			memcpy(g_held_packets, (const void *)g_packets, used < sizeof(g_held_packets) ? used : sizeof(g_held_packets));
		}
		cursor = P(g_held_packets);
		for (int i = 0; i < g_npic; i++)
		{
			const PicRec &p = g_pics[i];
			int16_t scroll = 0, bright = 0;
			const bool next = NextDraw(p, &scroll, &bright);
			if (num >= den)
			{
				// fraction 1: the next tick's picture
				if (next)
					HeldPicture(scroll, bright);
				continue;
			}
			if (!p.drew)
				continue;
			if (next)
				HeldPicture((int16_t)lerp_i(p.scroll, scroll, num, den), (int16_t)lerp_i(p.bright, bright, num, den));
			else
				HeldPicture(p.scroll, p.bright); // its last picture
		}
		cursor = saved;
	}
}
}
	static void register_mag062_held()
	{
		mark_held(act::devour::ORIG_Picture);
		register_module_held(62, act::devour::HeldReady, act::devour::HeldFrame);
	}
}
