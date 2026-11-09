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

// 30 fps part of mag285_thunder_summon.cpp (Thunder Summon, effect 285). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play). A real tick records its draws in order with the GTE state and
// the Field_Alloc scratch each one started from; a held frame replays them in the same order on private packets, with the
// current camera (the frame gate's: in game the gate's lerp of the camera words the main task writes; the effect camera
// 0x2507D70 rebuilt from it by 0x67AAE0 and put back):
//   Layouts - their fixed blocks (point, depth; the ground's texture scroll of counter N), through prim::play_held (every
//             object's exact in-between record).
//   Tiles   - the one-tick white tiles of counters 82 / 86 drawn again (state N).
//   Sparks  - they do not move: their picture of tick N (flipbook frame N held).
// A layout the next tick no longer plays (main counters 86 / 133) holds its picture (records at N). Spawns (CRT rand) stay at
// 15 Hz; the screen-flash level is the engine's. No task has a draw-only mode. The module packet arena alternates per tick: the
// held packets start where the tick's first draw started. The GTE, the Field_Alloc scratch, the engine words the sprite draws
// touch and the effect camera are put back.

namespace ff8fx
{
namespace thunder285
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick

	struct Rec
	{
		int kind;                 // DrawKind
		const void *node;
		int16_t counter;          // layouts: the main counter as drawn; sparks: the frame
		int16_t pos[3];           // sparks: as drawn
		PrimArg arg;              // layouts: the block as played
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[4 + 0x40];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x1AB8)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, const void *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(const MainNode *m, int kind, const PrimArg *arg)
	{
		Rec *r = NewRec(kind, m);
		if (!r) return;
		r->counter = m->counter;
		r->arg = *arg;
	}

	static void held_note_tile() { NewRec(TILE, nullptr); }

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(SPR_SPARK, s);
		if (!r) return;
		r->counter = s->counter;
		memcpy(r->pos, s->pos, sizeof(r->pos));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const Mat4x3 effcam = EffCam();
		OrthonormalizeCamera(&Camera(), &EffCam());
		// GTE data registers 28..31 are not in GTE_DATA
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
			switch (r.kind)
			{
			case PLAY_BOLT:
			case PLAY_GROUND:
			{
				const MainNode *m = (const MainNode *)r.node;
				const bool next = r.kind == PLAY_BOLT ? (uint32_t)(r.counter + 1 - 0x30) < 0x27 : (uint32_t)(r.counter + 1 - 0x56) < 0x30;
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)(r.kind == PLAY_BOLT ? m->bolt : m->ground), PrimCallback, (int)&arg, next ? num : 0, den);
				break;
			}
			case TILE:
				PacketCursor() = Tile(0x80, 0x80, 0x80, 0x1000, PacketCursor());
				break;
			default:
			{
				SparkNode s = {};
				s.counter = r.counter;
				memcpy(s.pos, r.pos, sizeof(s.pos));
				SparkDraw(&s);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
		EffCam() = effcam;
	}
}

	static void register_mag285_held()
	{
		mark_held(thunder285::ORIG_MainTask);
		mark_held(thunder285::ORIG_SparkTask);
		register_module_held(285, thunder285::HeldReady, thunder285::HeldDraw);
	}
}
