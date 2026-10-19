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

// 30 fps part of mag293_eye_drops.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag293_eye_drops.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The play task draws THEN advances its layout. A real tick records the play (once it started:
// counter 1 on) with its anchor, the GTE registers and the Field_Alloc scratch it started from (draw
// headers keep the scratch bytes where the code does not write). A held frame replays it through
// prim::play_held: every object's exact in-between record (position, rotation, scale, colour, fade,
// vertex-frame blend; objects appearing / disappearing switch on the real frame; the layout's last
// drawn frame holds its picture). The anchor is fixed (taken once on counter 1: nothing follows the
// target's model afterwards); the play block is rebuilt exactly as the play task builds it, and every
// object is placed through the frame's camera (the frame gate's) as the callback places it.
// Blended vertex frames go to a private buffer; the packets go to a private copy of the module arena
// (magc::HeldScope). TheThe GTE registers 28..31 are put back. No Eye Drops task has a
// draw-only mode.

namespace ff8fx
{
namespace eyedrops293
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick

	struct Rec
	{
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t anchor[4];
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[2];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x400];  // blended vertex frames (magic buffer + 0x4E4: 0x8 bytes)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(prim::Layout *layout, const int16_t *anchor)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.layout = layout;
		memcpy(r.anchor, anchor, sizeof(r.anchor));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// the play block, as the play task builds it
			PrimArg arg;
			memcpy(arg.anchor, r.anchor, sizeof(arg.anchor));
			arg.depth = -0x80;
			arg.morph = (uint32_t)g_held_morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			prim::play_held(r.layout, PrimCallback, (int)&arg, num, den);
		}
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag293_held()
	{
		mark_held(eyedrops293::ORIG_PlayTask);
		register_module_held(293, eyedrops293::HeldReady, eyedrops293::HeldFrame);
	}
}
