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

// 30 fps part of mag114_zombie.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag114_zombie.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The target task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes
// where the code does not write). A held frame replays the log in the same order:
//   Mist layout - through prim::play_held: every object's exact in-between record (position,
//                 rotation, scale, colour, fade, vertex-frame blend); blended vertex frames go to
//                 a private buffer. The anchor (the target's effect position) and the size-based
//                 scale / depth hold (engine-driven target).
//   Overlay     - the target at its in-between pose (pose_midpoint), lift, colour weight, sway
//                 amplitude and wave phase half way to the next tick's step (the step after the
//                 last one draws nothing: the last draw holds); the entity matrix and tint hold.
//                 Vertices go to a private vertex buffer (the bone workspace pointer is put back).
// No Zombie task has a draw-only mode.

namespace ff8fx
{
namespace zombie114
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PLAY = 1, REC_OVERLAY = 2 };

	struct Rec
	{
		uint32_t kind;
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t *entity;          // overlay
		OverlayArg now, next;     // overlay: drawn / next tick's parameters
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[16];
	static uint8_t g_held_packets[0x18000]; // module arena (0x18000 per tick)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0x1530)
	static uint8_t g_held_verts[0x10000];   // overlay vertices (indices are 12-bit: 0x8000)

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

	static void held_note_play(const TargetNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->mist;
		r->arg = *arg;
	}

	// the overlay's parameters of the next step are pure math (ComputeSin only reads its table)
	static void held_note_overlay(const TargetNode *t, const OverlayArg *o, int32_t e, int32_t reach)
	{
		Rec *r = NewRec(REC_OVERLAY);
		if (!r) return;
		r->entity = t->entity;
		r->now = *o;
		if (e + 1 < 0x1E) OverlayParams(e + 1, reach, &r->next);
		else r->next = *o;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void OverlayHeld(const Rec &r, int num, int den)
	{
		OverlayArg m = r.now;
		m.lift[1] = lerp16(r.now.lift[1], r.next.lift[1], num, den);
		m.weight = lerp_i(r.now.weight, r.next.weight, num, den);
		m.amp = lerp_i(r.now.amp, r.next.amp, num, den);
		m.phase = lerp_i(r.now.phase, r.next.phase, num, den);
		uint8_t *entity = r.entity;
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		const uint32_t bones = BoneWorkspace();
		BoneWorkspace() = (uint32_t)g_held_verts;
		DrawOverlay(entity, &m);
		BoneWorkspace() = bones;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, MistPart, (int)&arg, num, den);
				break;
			}
			case REC_OVERLAY: OverlayHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag114_held()
	{
		mark_held(zombie114::ORIG_TargetTask);
		register_module_held(114, zombie114::HeldReady, zombie114::HeldFrame);
	}
}
