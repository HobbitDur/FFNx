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

// 30 fps part of mag280_full_cure.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag280_full_cure.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The target and sparkle tasks draw THEN advance. A real tick records every target / sparkle task call in execution
// order (the target queue, then the sparkle queue) with the counter / frame it drew with, the GTE registers and the
// Field_Alloc scratch it started from (draw headers keep the scratch bytes the code does not write) and the module packet
// cursor of the tick's first draw. A held frame replays the calls in the same order, each part as its task draws it:
// - the target layout (prim::play_held: every object's exact in-between record; objects appearing / disappearing switch
//   on the real frame; mesh numbers never blended) at its anchor: the target's effect point taken once at the task's start
//   (nothing follows the target's model); the play block is tick N's below 1 and the next tick's at 1 (its counter word
//   only reaches the GTE VZ0 high half).
// - the sparkles: fixed points (taken once at their start), flipbook frame N below 1, the next tick's frame N + 1 at 1.
// A part the next tick does not draw (a layout at its last frame, the target task's last tick 0x1D, a sparkle's last frame
// 7) holds this tick's picture and is not drawn at fraction 1; parts the next tick starts are not drawn (fraction 1 = a
// subsequence of the next tick). Every part is placed through the frame's camera (the frame gate's: the next tick's view
// at fraction 1), as the tasks and the callback read the camera matrix when they draw. Not drawn by the module (held by
// the engine at 15 Hz, as the real ticks set it): the screen flash level of the entity draw entries
// (Effect_SetScreenFlash). Blended vertex frames go to a private buffer; the packets go to a private copy of the module
// arena (magc::HeldScope); the GTE registers 28..31 are put back. No task of the module has a draw-only mode.

namespace ff8fx
{
namespace fullcure280
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const int MAX_RECS = 3 + 16;     // target pool + sparkle pool

	struct Rec
	{
		const void *node;         // the live node
		bool sparkle;             // SparkleTask (else TargetTask)
		int16_t counter;          // the target counter / sparkle frame it drew with
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x1000]; // blended vertex frames (magic buffer + 0x1BF0)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note(const void *node, bool sparkle, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.node = node;
		r.sparkle = sparkle;
		r.counter = counter;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}
	static void held_note_target(const TargetNode *t) { held_note(t, false, t->counter); }
	static void held_note_sparkle(const SparkleNode *s) { held_note(s, true, s->frame); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the layout plays again on the next tick (it has frames left)
	static bool LayoutGoesOn(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	static void HeldTarget(const Rec &r, int num, int den)
	{
		const TargetNode *t = (const TargetNode *)r.node;
		const int32_t c = r.counter;
		const bool goes_on = c + 1 < TARGET_TICKS && LayoutGoesOn((const prim::Layout *)t->layout);
		if (!goes_on && num == den) return;
		PrimArg arg;
		memcpy(arg.pos, t->anchor, 6);
		arg.counter = (int16_t)(num == den ? c + 1 : c);
		arg.depth = -0x80;
		arg.morph = (uint32_t)g_held_morph;
		prim::play_held((prim::Layout *)t->layout, PrimCallback, (int)&arg, goes_on ? num : 0, den);
	}

	static void HeldSparkle(const Rec &r, int num, int den)
	{
		const SparkleNode *s = (const SparkleNode *)r.node;
		const int32_t f = r.counter;
		const bool goes_on = f + 1 < SPARKLE_FRAMES;
		if (num == den && !goes_on) return;
		TransformCameraByShadowRotation(s->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQUENCE;
		*(int16_t *)(h + 4) = (int16_t)(num == den ? f + 1 : f);
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed task starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.sparkle) HeldSparkle(r, num, den);
			else HeldTarget(r, num, den);
		}
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag280_held()
	{
		mark_held(fullcure280::ORIG_TargetTask);
		mark_held(fullcure280::ORIG_SparkleTask);
		register_module_held(280, fullcure280::HeldReady, fullcure280::HeldFrame);
	}
}
