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

// 30 fps part of mag273_mad_rush.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag273_mad_rush.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every task draws THEN updates. A real tick records its draws in queue order (the play tasks, then the dust), each with the
// GTE registers and the Field_Alloc scratch it started from and the module packet cursor of the tick's first draw. A held
// frame replays them in the same order:
// - Play - the layout (prim::play_held: every object's exact in-between record; objects appearing / disappearing switch on
//   the real frame; mesh numbers never blended) at the anchor taken once at the task's start (the target's bone 0xF1 then:
//   nothing follows the target's model). A layout that drew its last frame (the task ends on that tick) holds this tick's
//   picture and is not drawn at fraction 1. A task still waiting (counter < 0) draws nothing (it appears on the real frame).
// - Dust - half way up (y + its speed, the update's step; flipbook frame N held: discrete); a dust's last draw (frame 12)
//   holds. The tick's spawns are drawn by the tick (logged), random generation stays 15 Hz.
// Every part is placed through the frame's camera (the frame gate's: the next tick's view at fraction 1): the callback
// composes with the camera matrix, the dust's TransformCameraByShadowRotation reads it. Blended vertex frames go to a private
// buffer; the packets go to a private copy of the module arena (magc::HeldScope, which also puts back the Field_Alloc and
// shadow-rotation scratch and the GTE); the sprite draw's engine words and the GTE registers 28..31 are put back. No task of
// the module has a draw-only mode.

namespace ff8fx
{
namespace madrush273
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	enum : uint32_t { REC_PLAY = 1, REC_DUST = 2 };

	struct Rec
	{
		uint32_t kind;
		const PlayNode *node;     // play: the live node (its layout: prim::play_held knows what it drew)
		int16_t anchor[4];        // play: the anchor it drew with
		DustNode dust;            // dust: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 0x40];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x1000]; // blended vertex frames (magic buffer + MORPH)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(const PlayNode *p)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->node = p;
		memcpy(r->anchor, p->anchor, sizeof(r->anchor));
	}

	static void held_note_dust(const DustNode *d)
	{
		Rec *r = NewRec(REC_DUST);
		if (r) r->dust = *d;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the layout plays again on the next tick (it has frames left)
	static bool LayoutGoesOn(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == REC_PLAY)
			{
				PrimArg arg;
				memcpy(arg.pos, r.anchor, sizeof(arg.pos));
				arg.morph = (uint32_t)g_held_morph;
				prim::Layout *l = (prim::Layout *)r.node->layout;
				if (LayoutGoesOn(l)) prim::play_held(l, PrimCallback, (int)&arg, num, den);
				else if (num < den) prim::play_held(l, PrimCallback, (int)&arg, 0, den);
			}
			else
			{
				DustNode d = r.dust;
				// half way up by its speed (its last frame holds)
				if (d.counter + 1 < DUST_FRAMES) d.pos[1] = lerp16(d.pos[1], (int16_t)(d.pos[1] + d.vy), num, den);
				DustDraw(&d);
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag273_held()
	{
		mark_held(madrush273::ORIG_PlayTask);
		mark_held(madrush273::ORIG_DustTask);
		register_module_held(273, madrush273::HeldReady, madrush273::HeldFrame);
	}
}
