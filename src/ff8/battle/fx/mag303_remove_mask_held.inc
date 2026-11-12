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

// 30 fps part of mag303_remove_mask.cpp (Remove mask, effect 303). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the main draws the shards first and moves them last). A real tick records its draws in
// order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in the same order
// on private packets, with the current camera (the frame gate's), each layout through prim::play_held:
//   Shards     - every shard drawn on the tick moved by its own update run on a copy (age, spin, fall, bounce): position
//                and corners half way; a shard that dies on the next tick holds below fraction 1 and is gone at 1;
//   Layouts    - the main's (fixed point) and the mask pieces' (fixed point and scale) through prim::play_held;
//   Mask pieces - the prim model half way along the velocity it moves by (the one before the fall);
//   Redraw     - the unmasked form at its pose midpoint (pose_midpoint, the bones rebuilt from the real pose by the redraw
//                itself), its uv scrolls (2 / 4 a tick) and grey (sin of 0x80 a tick) half way; its vertex buffer starts
//                as the real tick's (vertices behind the camera keep the buffer's bytes);
//   Sparks     - half way along the velocity they move by (the one before the acceleration); flipbook frame N held;
//   Sprites    - fixed position, flipbook frame N held (sparks and sprites: at fraction 1 the next tick's frame).
// A part the next tick no longer draws holds its picture below fraction 1 and is gone at 1; an ended sprite / spark holds
// its last frame. Spawns (CRT rand, shatters, module LCG) stay at 15 Hz. The module packet arena alternates per tick: the
// held packets start where the tick's draws started; the blend / redraw vertex buffer (magic buffer + 0x3688) lies 0x2000
// bytes before arena A (+0x5688): on arena-A ticks the held one sits 0x2000 bytes before the held packets, as in the game.
// The redraw's frame-arena packets (cursor 0x1D8E054) go to a private buffer that starts with the real tick's. The GTE,
// the Field_Alloc scratch, the shadow / sprite temporaries and the unmasked form's bone matrices are put back.

namespace ff8fx
{
namespace mask303
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A
	static const uint32_t VB_BYTES = MORPH_GAP;  // the redraw's vertex buffer (up to arena A)

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		int16_t counter;          // main: the counter as drawn; sprites / sparks: the frame
		int16_t pos[3];           // pieces / sparks / sprites: as drawn
		int16_t vel[3];
		uint32_t model;           // pieces
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's start (the arena)
	static uint32_t g_frame_packets = 0;    // the engine's frame cursor at the redraw
	static Rec g_recs[3 + 2 * 4 + 0x40 + 8];
	static Shard g_shards[0x80];            // the shards as drawn
	static uint8_t g_vb[VB_BYTES];          // the redraw's vertex buffer before it ran
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[VB_BYTES];  // blend / vertex buffer on arena-B ticks
	static uint8_t g_held_frame[0x40000];   // the redraw's frame-arena packets

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
		g_frame_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		Rec &r = g_recs[g_nrec++];
		memset(&r, 0, offsetof(Rec, gte));
		r.kind = kind;
		r.node = n;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note(int kind, TaskNode *n)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		switch (kind)
		{
		case SHARDS:
			memcpy(g_shards, ((const MainNode *)n)->shards, sizeof(g_shards));
			r->counter = ((const MainNode *)n)->counter;
			break;
		case MAIN_PLAY:
			r->counter = ((const MainNode *)n)->counter;
			break;
		case REDRAW:
			r->counter = ((const MainNode *)n)->counter;
			memcpy(g_vb, (const void *)(TexBase() + MORPH), sizeof(g_vb));
			g_frame_packets = FrameCursor();
			break;
		case PIECE:
		{
			const LayoutNode *L = (const LayoutNode *)n;
			memcpy(r->pos, L->pos, sizeof(r->pos));
			memcpy(r->vel, L->vel, sizeof(r->vel));
			r->model = L->model;
			break;
		}
		case SPARK:
		{
			const SparkNode *s = (const SparkNode *)n;
			memcpy(r->pos, s->pos, sizeof(r->pos));
			memcpy(r->vel, s->vel, sizeof(r->vel));
			r->counter = s->frame;
			break;
		}
		case SPRITE:
		{
			const SpriteNode *s = (const SpriteNode *)n;
			memcpy(r->pos, s->pos, sizeof(r->pos));
			r->counter = s->frame;
			break;
		}
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		const bool alive = (r.node->flags & 1) != 0;
		const int32_t c1 = r.counter + 1;
		switch (r.kind)
		{
		case SHARDS: return alive && c1 < 0x29;
		case MAIN_PLAY: return alive && (uint32_t)c1 < 0x16;
		case REDRAW: return alive && (uint32_t)(c1 - 0x18) < 0x10;
		case PIECE: return alive && ((const LayoutNode *)r.node)->life > 0;
		default: return alive;
		}
	}

	// the shards at k/den: each one's next state from its own update on a copy (GTE work: before the draw's GTE state is
	// put back)
	static Shard g_tmp[0x80];
	static void HeldShards(int k, int num, int den)
	{
		for (int i = 0; i < 0x80; i++)
		{
			Shard &t = g_tmp[i];
			t = g_shards[i];
			if (t.kind == 0 || k == 0) continue;
			Shard nx = t, *cur = nullptr;
			if (!ShardMove(&nx, &cur))
			{
				if (num >= den) t.kind = 0; // dies on the next tick: gone at 1, holds below
				continue;
			}
			for (int j = 0; j < 3; j++) t.pos[j] = lerp16(t.pos[j], nx.pos[j], k, den);
			for (int c = 0; c < 4; c++)
				for (int j = 0; j < 3; j++) t.corner[c][j] = lerp16(t.corner[c][j], nx.corner[c][j], k, den);
		}
	}

	// the redraw's frame-arena packets go to a private buffer starting with the real tick's
	struct FrameScope
	{
		uint32_t saved;
		FrameScope()
		{
			saved = FrameCursor();
			if (g_frame_packets && saved > g_frame_packets)
			{
				const uint32_t used = saved - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
		}
		~FrameScope() { FrameCursor() = saved; }
	};

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0x5688) follows the blend buffer (+ 0x3688) at 0x2000 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		uint8_t *morph = arena_a ? g_held_arena : g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite and shadow draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const bool next = NextDraws(r);
			if (!next && num >= den && r.kind != SPARK && r.kind != SPRITE) continue; // fraction 1 = the next tick: a part it no longer draws is gone (an ended sprite holds its last frame)
			const int k = next ? num : 0; // the last picture holds
			int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
			PrimArg a = {};
			prim::Layout *layout = nullptr;
			int32_t uoff = 0, voff = 0, angle = 0;
			if (r.kind == SHARDS) HeldShards(k, num, den);
			else if (r.kind == MAIN_PLAY)
			{
				const MainNode *m = (const MainNode *)r.node;
				memcpy(a.pos, m->pt, 8);
				a.depth = -0x80;
				layout = (prim::Layout *)m->layout;
			}
			else if (r.kind == PIECE_PLAY)
			{
				const LayoutNode *L = (const LayoutNode *)r.node;
				PieceArg(L, &a);
				layout = (prim::Layout *)L->layout;
			}
			else if (r.kind == REDRAW)
			{
				const int32_t c = r.counter;
				uoff = c * 2 + 2 * k / den;
				voff = c * 4 + 4 * k / den;
				angle = shl32((c - 0x18) & 0x1FFFFF, 7) + 0x80 * k / den;
				memcpy(morph, g_vb, VB_BYTES);
				if (k) pose_midpoint(Unmasked() + 0x60, Unmasked() + 0x6C, k, den);
			}
			else if (r.kind == PIECE || r.kind == SPARK)
			{
				for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
			}
			a.morph = (uint32_t)morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case SHARDS: ShardsDraw(g_tmp); break;
			case MAIN_PLAY:
			case PIECE_PLAY: prim::play_held(layout, PrimCallback, (int)&a, k, den); break;
			case REDRAW:
			{
				FrameScope fs;
				const int16_t rect[4] = { 0x2C0, 0x100, 0x80, 0x80 };
				const int32_t g = ComputeSin(angle) >> 6;
				const uint8_t rgb[4] = { (uint8_t)g, (uint8_t)g, (uint8_t)g, 0 };
				Redraw(Unmasked(), rect, REDRAW_Clut, uoff, voff, rgb, (uint32_t)morph);
				break;
			}
			case PIECE: PieceDraw(r.model, pos); break;
			case SPARK: SpriteDraw(pos, (int16_t)(r.counter + (next && num >= den ? 1 : 0)), SEQ_Spark); break; // at 1 the next tick's frame
			case SPRITE: SpriteDraw(pos, (int16_t)(r.counter + (next && num >= den ? 1 : 0)), SEQ_Sprite); break;
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag303_held()
	{
		mark_held(mask303::ORIG_MainTask);
		mark_held(mask303::ORIG_LayoutTask);
		mark_held(mask303::ORIG_SparkTask);
		mark_held(mask303::ORIG_SpriteTask);
		register_module_held(303, mask303::HeldReady, mask303::HeldDraw);
	}
}
