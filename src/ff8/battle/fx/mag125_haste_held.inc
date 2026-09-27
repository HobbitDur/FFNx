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

// 30 fps part of mag125_haste.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag125_haste.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Haste draws only through its two prim-model layouts per target (the task draws THEN updates:
// the player steps each layout's frame after drawing it). A real tick records every play in
// order with its parameter block BEFORE the camera (turn + position), the GTE registers and the
// Field_Alloc scratch it started from. A held frame replays the log in the same order:
//   Clock / lower layout - prim::play_held: every object's exact in-between record (position,
//              rotation, scale, colour, fade, vertex-frame blend); the block is composed again with
//              the current battle camera, the effect camera matrix (camera-facing objects) is
//              rebuilt from it for the held draw and put back; blended vertex frames go to a
//              private buffer.
// No Haste task has a draw-only mode.

namespace ff8fx
{
namespace haste125
{
	static const uint32_t SCRATCH = 0x400;

	struct Rec
	{
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		PrimArg arg;              // its block before the camera (+ scale, scaled, morph)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[16];
	static uint8_t g_held_packets[0x20000]; // module arena (0x18000 per tick)
	static uint8_t g_held_morph[0x4000];    // blended vertex frames (magic buffer + 0x1770, 0x300 bytes)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(uint8_t *layout, const PrimArg *local)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.layout = (prim::Layout *)layout;
		r.arg = *local;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const Mat4x3 effcam = *EffCam();
		EffectCameraMatrix(&Camera(), EffCam());
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			PrimArg arg = r.arg;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			memcpy(g_held_morph, (const void *)r.arg.morph, 0x300);
			arg.morph = (uint32_t)g_held_morph;
			prim::play_held(r.layout, ClockPart, (int)&arg, num, den);
		}
		*EffCam() = effcam;
	}
}

	static void register_mag125_held()
	{
		mark_held(haste125::ORIG_TargetTask);
		register_module_held(125, haste125::HeldReady, haste125::HeldFrame);
	}
}
