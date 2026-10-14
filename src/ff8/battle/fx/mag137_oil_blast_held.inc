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

// 30 fps part of mag137_oil_blast.cpp (Oil Blast, effect 137). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the blast counter after its draws, a
// chunk / spray after its draw). A real tick records its draws in order with the GTE state and the
// Field_Alloc scratch each one started from; a held frame replays them in the same order on private
// packets, with the current camera (the frame gate's), each layout through prim::play_held (every
// object's exact in-between record):
//   Puddle / spout / blast layouts - their points (fixed; the spout turned by the caster's facing).
//   Oil mesh     - its state at counter + num/den: progress s (the scale through the exe matrix
//                  diagonal 0x11F0360, put back; the point blended by s) and the U scroll, from the
//                  same formulas at the in-between counter.
//   Chunks       - position, angles and scale half way to the next tick's (after the gravity / bounce
//                  step; the scale of the last frames (0x1E - frame) << 10 at the in-between frame).
//   Sprays       - position half way to the next tick's; flipbook frame N held.
// A part the next tick no longer draws (its counter range, its layout or its task ended on the tick;
// every part on the tick the blast ends: the master ends with it) holds its picture (state N).
// Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates
// per tick: the held packets start where the tick's first draw started; the blend buffer (magic
// buffer + 0x1390) lies 0x2C8 bytes before arena A (+0x1658): on arena-A ticks the held buffer sits
// 0x2C8 bytes before the held packets, as in the game. The GTE, the Field_Alloc scratch, the exe
// matrix diagonal and the engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace oil137
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = ARENA_B - ARENA_A;   // module packet arena per tick
	static const uint32_t VERTS_GAP = ARENA_A - VERTS; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // blast: the counter as drawn; chunks / sprays: the frame
		int16_t pos[3];           // chunks / sprays: the position as drawn
		int16_t angle[3];         // chunks: the angles as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[5 + 0x20 + 0x3C];
	static uint8_t g_held_arena[VERTS_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_verts[0x1000];    // blend buffer on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = nullptr;
		r.counter = counter;
		memset(r.pos, 0, sizeof(r.pos));
		memset(r.angle, 0, sizeof(r.angle));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a blast layout is about to play
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		Rec *r = NewRec(kind, n, ((const BlastNode *)n)->counter);
		if (r) r->layout = l;
	}

	static void held_note_mesh(TaskNode *n) { NewRec(DRAW_MESH, n, ((const BlastNode *)n)->counter); }

	static void held_note_chunk(TaskNode *n)
	{
		const ChunkNode *c = (const ChunkNode *)n;
		Rec *r = NewRec(DRAW_CHUNK, n, c->frame);
		if (!r) return;
		memcpy(r->pos, c->pos, 6);
		memcpy(r->angle, c->angle, 6);
	}

	static void held_note_spray(TaskNode *n)
	{
		const SprayNode *s = (const SprayNode *)n;
		Rec *r = NewRec(SPR_SPRAY, n, s->frame);
		if (r) memcpy(r->pos, s->pos, 6);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static bool Alive(const Rec &r) { return (r.node->flags & 1) != 0; }
	// the layout has frames left after this tick's play (it draws again on the next tick)
	static bool LayoutLive(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}
	static bool PuddleDraws(int32_t c) { return (uint32_t)(c - 0x12) < 9; }
	static bool SpoutDraws(int32_t c) { return (uint32_t)(c - 0x15) < 0x14; }
	static bool MeshDraws(int32_t c) { return (uint32_t)(c - 0x15) < 8; }
	static bool BlastDraws(int32_t c) { return (uint32_t)(c - 0x1C) < 0x1A; }

	// a chunk's scale at frame f (none before frame 0x1B)
	static int32_t ChunkScale(int32_t f) { return f > 0x1A ? shl32(0x1E - f, 0xA) : 0x1000; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldDraw(int num, int den)
	{
		// the blast ended on this tick (its queue is empty): the master ends and no task draws again, every part holds
		if (!QBlast()->head) num = 0;
		// arena A (magic buffer + 0x1658) follows the blend buffer (+ 0x1390) at 0x2C8 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t verts = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_verts;
		HeldScope scope(PacketCursor(), g_held_arena + VERTS_GAP, ARENA, g_packets);
		// GTE data registers 28..31 (square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch, the exe matrix diagonal
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		const int16_t diag[3] = { SCALE_DIAG[0], SCALE_DIAG[4], SCALE_DIAG[8] };
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			PrimArg arg = {};
			arg.morph = verts;
			int k = num;
			switch (r.kind)
			{
			case SPR_SPRAY:
			{
				int16_t pos[4] = { r.pos[0], r.pos[1], r.pos[2], r.counter };
				const SprayNode *s = (const SprayNode *)r.node;
				if (Alive(r))
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], s->pos[j], num, den);
				Replay(r);
				SprayDraw(pos, s->scale, r.counter);
				continue;
			}
			case DRAW_CHUNK:
			{
				int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] }, angle[3] = { r.angle[0], r.angle[1], r.angle[2] };
				const ChunkNode *c = (const ChunkNode *)r.node;
				if (!Alive(r)) k = 0;
				for (int j = 0; j < 3 && k; j++)
				{
					pos[j] = lerp16(r.pos[j], c->pos[j], k, den);
					angle[j] = lerp16(r.angle[j], c->angle[j], k, den);
				}
				const int32_t f = r.counter;
				const bool scaled = k ? f + 1 > 0x1A : f > 0x1A;
				const int32_t scale = k ? lerp_i(ChunkScale(f), ChunkScale(f + 1), k, den) : ChunkScale(f);
				Replay(r);
				ChunkDraw(angle, scaled, scale, pos);
				continue;
			}
			case DRAW_MESH:
			{
				const BlastNode *w = (const BlastNode *)r.node;
				const int32_t kk = r.counter - 0x15;
				if (!MeshDraws(r.counter + 1)) k = 0;
				const int32_t xn = kk * den + k;
				Mat4x3 M;
				int16_t Q[4] = {};
				MeshAt(w, (int32_t)(((int64_t)xn << 9) / den), &M, Q);
				Replay(r);
				MeshRender(&M, (uint16_t)(((xn << 5) / den) & 0x7F));
				continue;
			}
			case PLAY_PUDDLE1:
			case PLAY_PUDDLE2:
			{
				const BlastNode *w = (const BlastNode *)r.node;
				k = PuddleDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				BlockAt(&arg, r.kind == PLAY_PUDDLE1 ? w->puddle1 : w->puddle2);
				arg.mode = 0;
				break;
			}
			case PLAY_SPOUT:
				k = SpoutDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				SpoutBlock(&arg, ((const BlastNode *)r.node)->spout);
				arg.mode = 0;
				break;
			case PLAY_BLAST:
				k = BlastDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				ComposeAffineTransform(&Camera(), &((const BlastNode *)r.node)->basisD, &arg.m);
				arg.depth = -0x100;
				arg.mode = 1;
				break;
			}
			Replay(r);
			prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		SCALE_DIAG[0] = diag[0];
		SCALE_DIAG[4] = diag[1];
		SCALE_DIAG[8] = diag[2];
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag137_held()
	{
		mark_held(oil137::ORIG_BlastTask);
		mark_held(oil137::ORIG_ChunkTask);
		mark_held(oil137::ORIG_SprayTask);
		register_module_held(137, oil137::HeldReady, oil137::HeldDraw);
	}
}
