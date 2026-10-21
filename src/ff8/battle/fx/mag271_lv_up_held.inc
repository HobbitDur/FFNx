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

// 30 fps part of mag271_lv_up.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag271_lv_up.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The play task draws THEN advances. A real tick records the play task call with the counter it drew with, the
// GTE registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes where the code
// does not write) and the module packet cursor of the tick's first draw. A held frame replays it the way the play
// task draws:
// - the layout (prim::play_held: every object's exact in-between record; objects appearing / disappearing switch
//   on the real frame; mesh numbers never blended). The block is fixed (anchor, extents and depth taken once at
//   the task's start from the target's bounding box: nothing follows the target's model); the texture scroll
//   counter is discrete: this tick's before fraction 1, the next tick's at 1 (= the next tick's picture).
// - when the layout drew its last frame (the task ends on that tick) the held frame holds this tick's picture and
//   draws nothing at fraction 1.
// Every object is placed through the frame's camera (the frame gate's: the next tick's view at fraction 1) and the
// orthonormalised camera the root task derives from it (0x67AAE0 on the frame's camera matrix, the module's copy
// 0x25085D0 put back afterwards), as the root and the callback read the camera when they draw. Blended vertex
// frames go to a private buffer; the packets go to a private copy of the module arena (magc::HeldScope); the GTE
// registers 28..31 are put back. No task of the module has a draw-only mode.

namespace ff8fx
{
namespace lv271
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const int MAX_PLAYS = 1;

	struct Rec
	{
		const PlayNode *node;     // the live node (its layout: prim::play_held knows what it drew)
		int16_t counter;          // the counter it drew with
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_PLAYS];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x1000]; // blended vertex frames (magic buffer + MORPH)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(const PlayNode *p)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_PLAYS) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.node = p;
		r.counter = p->counter;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the layout plays again on the next tick (it has frames left)
	static bool LayoutGoesOn(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// the play block the play task builds at counter c
	static void Block(const PlayNode *p, int32_t c, PrimArg *a)
	{
		a->morph = (uint32_t)g_held_morph;
		a->depth = -(int32_t)*(const int16_t *)(p->entity + 0x26) >> 4;
		a->scale[0] = p->scale[0];
		a->scale[1] = p->scale[1];
		a->scale[2] = p->scale[2];
		memcpy(a->pos, p->anchor, sizeof(a->pos));
		a->scaled = 1;
		a->scroll = (int16_t)c;
		a->pad1E = 0;
	}

	static void HeldPlay(const Rec &r, int num, int den)
	{
		const PlayNode *p = r.node;
		prim::Layout *l = (prim::Layout *)p->layout;
		PrimArg now, next;
		Block(p, r.counter, &now);
		Block(p, r.counter + 1, &next);
		// in between (the scroll counter switches on the real frame); at 1 the next tick's block
		if (LayoutGoesOn(l)) prim::play_held(l, PrimCallback, (int)(num < den ? &now : &next), num, den);
		else if (num < den) prim::play_held(l, PrimCallback, (int)&now, 0, den);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// the orthonormalised camera of the frame's camera (the root task's 0x67AAE0), the module's copy put back
		const Mat4x3 camn = CameraN();
		EffectCameraMatrix(&Camera(), &CameraN());
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed task starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			HeldPlay(r, num, den);
		}
		CameraN() = camn;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag271_held()
	{
		mark_held(lv271::ORIG_PlayTask);
		register_module_held(271, lv271::HeldReady, lv271::HeldFrame);
	}
}
