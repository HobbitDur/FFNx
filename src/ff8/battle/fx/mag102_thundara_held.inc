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

// 30 fps part of mag102_thundara.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag102_thundara.cpp; shared pieces in mag_common_held.h).
//
// A real tick records every draw in order: the task's node as drawn (up to 0x2C bytes), the GTE
// registers and the Field_Alloc scratch the draw started with (the bolt nests prim-model headers
// in its sprite header's scratch; fields a draw does not set keep what the scratch held). A held
// frame replays the log in the same order from the same GTE state and scratch:
//   Bolt   - flash quad and sprite layers are flipbooks keyed on the frame: hold. The prim models
//            (tilted glows, column, flash ball) and the darkening layer's colour are sin() of the
//            frame: drawn with the exact fractional frame (frame + num / den).
//   Spark  - position half way along its velocity (+ the acceleration the update adds first),
//            brightness half way to the next (clamped) step; flipbook frame (anim % 20) holds.
//   Flash  - flipbook at a fixed place: holds.
// No Thundara task has a draw-only mode: everything moves on every tick.

namespace ff8fx
{
namespace thundara102
{
	static const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;
	static const uint32_t NODE_MAX = 0x2C, SCRATCH = 0x200;

	struct Rec
	{
		uint32_t fn;
		uint8_t node[NODE_MAX];
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	// real tick on which the ported root task last ran, and its draw log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0; // packet cursor at the tick's first draw
	static Rec g_recs[256];
	static uint8_t g_held_packets[0x10000];
	// held frame being drawn: frame + num / den (den 0 = real tick)
	static int g_hnum = 0, g_hden = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_draw(uint32_t fn, const void *node, uint32_t size)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		memset(r.node, 0, sizeof(r.node));
		memcpy(r.node, node, size < NODE_MAX ? size : NODE_MAX);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	// a sin() parameter (frame << shift) / duration at the held frame's fractional frame
	static int32_t held_frame_arg(int32_t t, int32_t frame, int shift, int32_t duration)
	{
		if (!g_hden) return t;
		return shl32(frame * g_hden + g_hnum, shift) / (duration * g_hden);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		g_hnum = num;
		g_hden = den;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_BoltTask:
			{
				BoltNode a;
				memcpy(&a, r.node, NODE_MAX);
				if (a.counter == 0xC) PacketCursor() = ScreenFlashQuad(FLASH_Colour, PacketCursor());
				if (a.counter > 0xA)
				{
					int32_t f = a.counter - 0xB;
					uint8_t *h = AllocHeader(0xB4);
					BoltLayers(&a, f, h);
					if (f == 0xC) PacketCursor() = ScreenFlashQuad(FLASH_Colour, PacketCursor());
					FieldFree(0xB4);
				}
				break;
			}
			case ORIG_SparkTask:
			{
				SparkNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				SparkUpdate(&b);
				SparkNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				m.level = lerp16(a.level, b.level, num, den);
				SparkDraw(&m);
				break;
			}
			case ORIG_FlashTask:
			{
				FlashNode a;
				memcpy(&a, r.node, sizeof(a));
				FlashDraw(&a);
				break;
			}
			}
		}
		g_hden = 0;
		g_hnum = 0;
	}
}

	static void register_mag102_held()
	{
		mark_held(thundara102::ORIG_BoltTask);
		mark_held(thundara102::ORIG_SparkTask);
		mark_held(thundara102::ORIG_FlashTask);
		register_module_held(102, thundara102::HeldReady, thundara102::HeldFrame);
	}
}
