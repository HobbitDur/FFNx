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

// 30 fps part of mag119_stop.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag119_stop.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw of the target tasks in order with the GTE registers and the
// Field_Alloc scratch it started from. A held frame replays the log in the same order:
//   Clock  - the prim-model layout draws THEN updates: prim::play_held draws every object's exact
//            in-between record (position, rotation, scale, colour, fade, vertex-frame blend); the
//            block is composed again with the current battle camera; blended vertex frames go to a
//            private buffer. The callback runs with the frame the real tick drew, except that from
//            frame 53 on the broken pieces are skipped without building their shards again (the
//            shard set-up draws random spins: never on a held frame).
//   Shards - update THEN draw: the next tick's state is derived from the drawn one by the task's
//            own update (move by velocity, vertices turned by the spin, fade + 0x100) run on a
//            copy; the held frame draws every shard half way (centre, vertex offsets, fade); the
//            last draw (tick 70) holds.
// No Stop task has a draw-only mode.

namespace ff8fx
{
namespace stop119
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PLAY = 1, REC_SHARDS = 2 };

	struct Rec
	{
		uint32_t kind;
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;              // play: its block before the camera
		int32_t frame;            // play: the layout frame drawn
		int snap;                 // shards: the snapshot of the shard buffer as drawn
		bool next;                // shards: the next tick updates and draws them again
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nsnap = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[16];
	static Shard g_snap[3][SHARD_COUNT];
	static Shard g_next[SHARD_COUNT], g_mid[SHARD_COUNT];
	static int32_t g_held_frame = 0;
	static uint8_t g_held_packets[0x20000]; // module arena (0x18000 per tick)
	static uint8_t g_held_morph[0x4000];    // blended vertex frames (magic buffer + 0xF0C, 0x6F0 bytes)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nsnap = 0;
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

	static void held_note_play(const TargetNode *t, const PrimArg *local)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->clock;
		r->arg = *local;
		r->frame = r->layout->frame;
	}

	static void held_note_shards(const TargetNode *t)
	{
		if (g_nsnap >= 3) return;
		Rec *r = NewRec(REC_SHARDS);
		if (!r) return;
		r->snap = g_nsnap++;
		r->next = t->counter < 0x46;
		memcpy(g_snap[r->snap], (const void *)(TexBase() + SHARDS), sizeof(g_snap[0]));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void __cdecl ClockPartHeld(prim::Layout *l, prim::Record *r, int arg)
	{
		ClockPartBody(l, r, (const PrimArg *)arg, g_held_frame);
	}

	static void ShardsHeld(const Rec &r, int num, int den)
	{
		const Shard *cur = g_snap[r.snap];
		memcpy(g_mid, cur, sizeof(g_mid));
		if (r.next)
		{
			memcpy(g_next, cur, sizeof(g_next));
			ShardsUpdate(g_next, SHARD_COUNT);
			for (int i = 0; i < SHARD_COUNT; i++)
			{
				Shard &m = g_mid[i];
				const Shard &b = g_next[i];
				for (int c = 0; c < 3; c++) m.pos[c] = lerp16(m.pos[c], b.pos[c], num, den);
				for (int k = 0; k < 4; k++)
					for (int c = 0; c < 3; c++) m.v[k][c] = lerp16(m.v[k][c], b.v[k][c], num, den);
				m.fade = lerp16(m.fade, b.fade, num, den);
			}
		}
		Replay(r);
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		ShardsDrawAll((uint8_t *)g_mid);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			switch (r.kind)
			{
			case REC_PLAY:
			{
				Replay(r);
				PrimArg arg = r.arg;
				ComposeAffineTransform(&Camera(), &arg.local, &arg.m);
				memcpy(g_held_morph, (const void *)(TexBase() + 0xF0C), 0x6F0);
				arg.morph = (uint32_t)g_held_morph;
				g_held_frame = r.frame >= 0x35 ? 0x36 : r.frame;
				prim::play_held(r.layout, ClockPartHeld, (int)&arg, num, den);
				break;
			}
			case REC_SHARDS: ShardsHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag119_held()
	{
		mark_held(stop119::ORIG_TargetTask);
		register_module_held(119, stop119::HeldReady, stop119::HeldFrame);
	}
}
