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

// 30 fps part of mag101_trauma_grab.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag101_trauma_grab.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The grab task draws its layout once a tick (counters 0..0x25) through the shared prim player,
// which draws THEN steps the records. A real tick records the play with its parameter block, the
// GTE registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes
// where the code does not write). A held frame replays it through prim::play_held: every object's
// exact in-between record (position, rotation, scale, colour, fade, vertex-frame blend). The anchor
// (the caster's position words) and the depth offset hold.
// Blended vertex frames go to a private buffer placed like the original's (magic buffer + 0x2D0,
// 0x180 bytes before arena A): on arena-A ticks the held blend buffer is the 0x180 bytes before the
// held packet copy, as in the game (a blend larger than that runs into the packets there too).
// Not redrawn (they hold): the screen flash level (one engine word a tick) and the target entity's
// position / colour / rotation words (the battle draws the entity itself).
// No grab task has a draw-only mode.

namespace ff8fx
{
namespace grab101
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const uint32_t BLEND = 0x180;    // blend buffer head before arena A

	struct Rec
	{
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[2];
	static uint8_t g_held_arena[BLEND + ARENA]; // [blend buffer head][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(prim::Layout *layout, const PrimArg *arg)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.layout = layout;
		r.arg = *arg;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		// arena A (magic buffer + 0x450) follows the blend buffer (+ 0x2D0) directly
		const bool arena_a = g_packets == TexBase() + 0x450;
		uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + BLEND, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			PrimArg arg = r.arg;
			arg.morph = morph;
			prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
		}
	}
}

	static void register_mag101_held()
	{
		mark_held(grab101::ORIG_GrabTask);
		register_module_held(101, grab101::HeldReady, grab101::HeldFrame);
	}
}
