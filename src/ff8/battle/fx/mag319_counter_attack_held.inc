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

// 30 fps part of mag319_counter_attack.cpp (Counter-attack, effect 319). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the main counter after its draws, a sprite after its
// draw). A real tick records its draws in order with the GTE state and the Field_Alloc scratch each one started from; a
// held frame replays them in the same order on private packets, with the current camera (the frame gate's), each layout
// through prim::play_held (every object's exact in-between record):
//   Layout A + spike model (counters 2..8) - the point L half way between this tick's and the next tick's (the weight
//                (c - 2) / 6 steps per tick; S, T, M and the offsets are fixed from counter 2 on);
//   Layout B (counters 7..0x26) - its frame is fixed (M2 at T - offset 0);
//   Sprites    - fixed position, flipbook frame N held.
// A part the next tick no longer draws (its counter range ends) holds its picture below fraction 1 and is gone at 1 (the next tick); an ended sprite holds its last frame. Spawns (CRT rand) stay at 15 Hz.
// No task has a draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's
// first draw started; the blend buffer (magic buffer + 0xAEC) lies 0x230 bytes before arena A (+0xD1C): on arena-A ticks
// the held blend buffer sits 0x230 bytes before the held packets, as in the game. The GTE, the Field_Alloc scratch and the
// engine words the sprite draws touch are put back.
// BONE SCALES (the caster's skeleton, scaled in place by the bone scale tasks after the engine built the matrices):
// HeldBones lists each live task's bone matrix with the scale tick N applied and the one tick N + 1 applies (its delay /
// growth / cap / end run on the node's values: an ending task leaves the bone unscaled, 0x1000); the frame gate applies
// the in-between scale around the caster's render on the held frame (fx_held.h HeldBoneScale). The object visibility bit
// (+0x7C) is a discrete switch: held frames show tick N's.

namespace ff8fx
{
namespace counter319
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		int16_t counter;          // main: the counter as drawn; sprites: the frame
		int16_t pos[3];           // sprites: the position as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 0x10];
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = 0;
		memset(r.pos, 0, sizeof(r.pos));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_main(int kind, TaskNode *n)
	{
		Rec *r = NewRec(kind, n);
		if (r) r->counter = ((const MainNode *)n)->counter;
	}

	static void held_note_sprite(TaskNode *n, const int16_t *pos, int16_t counter)
	{
		Rec *r = NewRec(SPR, n);
		if (!r) return;
		r->counter = counter;
		memcpy(r->pos, pos, sizeof(r->pos));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again (the main task ends after counter 0x26)
	static bool NextDraws(const Rec &r)
	{
		const int32_t c1 = r.counter + 1;
		switch (r.kind)
		{
		case PLAY_A:
		case DRAW_SPIKE: return (uint32_t)(c1 - 2) <= 6;
		case PLAY_B: return (uint32_t)(c1 - 7) < 0x20;
		default: return (r.node->flags & 1) != 0;
		}
	}

	// the spike point at k/den of the way from counter c to c + 1
	static void HeldSpikePoint(const MainNode *m, int32_t c, int k, int den, int16_t *L)
	{
		int16_t L0[4], L1[4];
		SpikePoint(m, c - 2, L0);
		if (k == 0)
		{
			memcpy(L, L0, 6);
			return;
		}
		SpikePoint(m, c - 1, L1);
		for (int j = 0; j < 3; j++) L[j] = lerp16(L0[j], L1[j], k, den);
	}

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0xD1C) follows the blend buffer (+ 0xAEC) at 0x230 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
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
			const bool next = NextDraws(r);
			if (!next && num >= den && r.kind != SPR) continue; // fraction 1 = the next tick: a part it no longer draws is gone (an ended sprite holds its last frame)
			const int k = next ? num : 0; // the last picture holds
			PrimArg arg = {}; // (the matrix pad +0x12 is never read)
			Mat4x3 M = {};
			int16_t L[4] = {};
			const MainNode *m = (const MainNode *)r.node;
			if (r.kind == PLAY_A || r.kind == DRAW_SPIKE)
			{
				HeldSpikePoint(m, r.counter, k, den, L);
				M = m->M;
				if (r.kind == PLAY_A)
				{
					M.t[0] = (int32_t)m->off[0][0] + L[0];
					M.t[1] = (int32_t)m->off[0][1] + L[1];
					M.t[2] = (int32_t)m->off[0][2] + L[2];
					ComposeAffineTransform(&Camera(), &M, &arg.m);
				}
			}
			else if (r.kind == PLAY_B) ComposeAffineTransform(&Camera(), &m->M2, &arg.m);
			arg.depth = -0x80;
			arg.morph = morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == SPR) SpriteDraw(r.pos, r.counter);
			else if (r.kind == DRAW_SPIKE) SpikeDraw(&M, L, &arg);
			else prim::play_held((prim::Layout *)(r.kind == PLAY_A ? m->layout_a : m->layout_b), PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}

	// the scale a bone scale task applies on its next tick (BoneScaleTask on the node's values; 0x1000 when it ends)
	static void NextBoneScale(const BoneNode *b, int32_t out[3])
	{
		int16_t delay = (int16_t)(b->delay - 1), sy = b->sy, sx = b->sx;
		if (delay <= 0)
		{
			sy = (int16_t)(sy + b->dy);
			if (sy > 0x1000) sy = 0x1000;
			sx = (int16_t)(sx + b->dx);
			if (sx > 0x1000) sx = 0x1000;
			if (sx >= 0x1000 && sy >= 0x1000) sx = sy = 0x1000;
		}
		out[0] = sx;
		out[1] = sx;
		out[2] = sy;
	}

	// the caster's bone matrices the live bone scale tasks scaled on the last real tick (they all ran: the root runs
	// their queue every tick once set up)
	static int HeldBones(HeldBoneScale *out, int max)
	{
		if (!HeldReady()) return 0;
		int n = 0;
		for (TaskNode *t = QBones()->head; t && n < max; t = t->next)
		{
			const BoneNode *b = (const BoneNode *)t;
			if (!(t->flags & 1) || (uint32_t)t->func != ORIG_BoneScaleTask) continue;
			HeldBoneScale &h = out[n++];
			h.ent = Caster();
			h.matrix = b->bone + 0x10;
			h.from[0] = b->sx;
			h.from[1] = b->sx;
			h.from[2] = b->sy;
			NextBoneScale(b, h.next);
		}
		return n;
	}
}

	static void register_mag319_held()
	{
		mark_held(counter319::ORIG_MainTask);
		mark_held(counter319::ORIG_SpriteTask);
		register_module_held(319, counter319::HeldReady, counter319::HeldDraw);
		register_module_bonescale(319, counter319::HeldBones);
	}
}
