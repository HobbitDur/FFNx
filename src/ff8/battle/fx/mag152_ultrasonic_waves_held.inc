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

// 30 fps part of mag152_ultrasonic_waves.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag152_ultrasonic_waves.cpp; the shared machinery is in
// mag_common_held.h).
//
// Every drawing task draws THEN updates. A held frame replays the draws of the last real tick in
// order (see mag_common_held.h), each from its node as drawn:
//   Wave  - fixed position, roll and size; the flipbook frame holds; from frame 8 the grey level
//           is half way to the next step when the flipbook has a next frame (else it holds).
//   Ring  - position half way along its velocity, angle half way along its spin, size half way
//           along its growth, each colour channel half way to the next entry of the colour cycle,
//           the fade (from counter 16) half way to the next step; its last tick (counter 20) holds.
//   Burst - the particles do not move: the draw of the real tick is replayed from a copy of the
//           particle pool as it was when the task drew (flipbook frames hold).
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.

namespace ff8fx
{
namespace ultrasonic152
{
	// real tick on which the ported root task last ran
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<256> g_draws;
	static uint8_t g_held_packets[0x8000]; // one packet arena (magic buffer + 0 / + 0x8000)
	// particle pool as each burst task drew it (bursts: at most 3 per tick)
	static Particle g_snaps[8][PART_COUNT];
	static int g_nsnap = 0;
	static int g_snap_of[256];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
		g_nsnap = 0;
	}

	static void held_note_draw(uint32_t fn, const void *node) { g_draws.add(fn, node, PacketCursor()); }

	static void held_note_burst(uint32_t fn, const void *node)
	{
		const int before = g_draws.n;
		g_draws.add(fn, node, PacketCursor());
		if (g_draws.n == before) return;
		if (g_nsnap < (int)(sizeof(g_snaps) / sizeof(g_snaps[0])))
		{
			memcpy(g_snaps[g_nsnap], Parts(), sizeof(g_snaps[0]));
			g_snap_of[g_draws.n - 1] = g_nsnap++;
		}
		else g_snap_of[g_draws.n - 1] = -1;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	// colour channels (bytes 0..2) half way, the code byte of a
	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t c = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			c |= (uint32_t)(lerp_i((int32_t)((a >> k) & 0xFF), (int32_t)((b >> k) & 0xFF), num, den) & 0xFF) << k;
		return c;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		const bool moving = !DrawOnly();
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			switch (r.fn)
			{
			case ORIG_WaveTask:
			{
				WaveNode a;
				memcpy(&a, r.node, sizeof(a));
				int32_t level = shl32(0xC - a.counter, 5);
				// the flipbook's next-frame word (what the draw leaves at header +0x28)
				const int16_t next = *(const int16_t *)(SEQ_Wave + 0xC + 2 * (uint32_t)(uint16_t)a.counter);
				if (moving && next >= 0) level = lerp_i(level, shl32(0xC - (a.counter + 1), 5), num, den);
				WaveDraw(&a, level);
				break;
			}
			case ORIG_RingTask:
			{
				RingNode a;
				memcpy(&a, r.node, sizeof(a));
				RingNode m = a;
				uint32_t colour = RingColours[a.counter % 10];
				int32_t fade = shl32(a.counter - 0x10, 10);
				if (moving && a.counter < 0x14)
				{
					RingNode b = a;
					RingUpdate(&b);
					b.counter++;
					for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
					m.angle = lerp16(a.angle, b.angle, num, den);
					m.scale = lerp16(a.scale, b.scale, num, den);
					colour = LerpColour(colour, RingColours[b.counter % 10], num, den);
					fade = lerp_i(fade, shl32(b.counter - 0x10, 10), num, den);
				}
				RingDraw(&m, colour, fade);
				break;
			}
			case ORIG_BurstA:
			case ORIG_BurstB:
			case ORIG_BurstC:
			{
				if (g_snap_of[i] < 0)
				{
					g_hdr_slot = nullptr;
					break;
				}
				static Particle parts[PART_COUNT];
				memcpy(parts, g_snaps[g_snap_of[i]], sizeof(parts));
				if (r.fn == ORIG_BurstA) BurstDraw(parts, SEQ_BurstA, 8, 1);
				else if (r.fn == ORIG_BurstB) BurstDraw(parts, SEQ_BurstB, 0, 2);
				else BurstDraw(parts, SEQ_BurstC, 0, 4);
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag152_held()
	{
		mark_held(ultrasonic152::ORIG_WaveTask);
		mark_held(ultrasonic152::ORIG_RingTask);
		mark_held(ultrasonic152::ORIG_BurstA);
		mark_held(ultrasonic152::ORIG_BurstB);
		mark_held(ultrasonic152::ORIG_BurstC);
		register_module_held(152, ultrasonic152::HeldReady, ultrasonic152::HeldFrame);
	}
}
