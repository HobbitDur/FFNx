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

// 30 fps part of mag324_tonberry_king_death.cpp (Tonberry King's death, effect 324). Exact vanilla shapes: the logic stays
// at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates. A real tick records its draws in order with the GTE state and the Field_Alloc scratch each
// one started from; a held frame replays them in the same order on private packets, with the current camera (the frame
// gate's):
//   Sparks (A / B) - the table as drawn; a cell the tick stepped (the live table: same flag, counter + 1) half way to its
//                next position (A rises by its speed, B stays), its sequence frame held (at fraction 1 the next tick's); a cell the tick freed (its sequence
//                finished) holds its last picture;
//   Ghost        - drawn as on the real tick (its kept pose and matrix, the colour and object mask it had): the caster put
//                into that state around the draw and put back; its packets into a private copy of the frame arena and its
//                vertices into a private vertex buffer. Its fade colour steps with the tint task's 24-colour cycle (a
//                discrete switch): held at tick N's; at fraction 1 the next tick's ghost (the tint task's next colour,
//                faded; the kept matrix from counter 7; none after its last tick);
//   Spin sprites - at the caster's bone 0xF0 point at the caster's in-between pose (pose_midpoint, the bones rebuilt from
//                the pose afterwards), flipbook frame N held.
// Draw-only ticks (battle_to_update_flags & 0x201) step nothing: the held frame equals the tick (the sparks' live table did
// not move). Spawns (CRT rand) stay at 15 Hz. Not drawn by the module (held by the engine at 15 Hz, as the real ticks set
// them): the caster's colour (+0x28) and flags the tint task writes. The GTE, the Field_Alloc scratch, the engine words
// the sprite and model draws touch, the frame arena / vertex buffer cursors and the caster's state are put back.

namespace ff8fx
{
namespace tonk324
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x8000;        // module packet arena per tick

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		uint8_t body[0x24];       // the task node as drawn (spin: counter, side, scale)
		Cell cells[CELLS];        // sparks: the table as drawn
		uint8_t matrix[0x20];     // ghost: the caster's matrix, colour, mask, +0x07 at the draw
		uint32_t tint, mask;
		uint8_t b07, b2b;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0;    // frame arena cursor at the tick's ghost draw
	static Rec g_recs[12];
	static Cell g_cells_held[CELLS];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[0x60000];   // ghost model packets
	static uint8_t g_held_verts[0x10000];   // ghost model vertices
	static uint8_t g_bones_live[0x20 * 256];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.body, n, sizeof(r.body));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_sparks(TaskNode *n, int which)
	{
		Rec *r = NewRec(which == 1 ? SPARKS_A : SPARKS_B, n);
		if (r) memcpy(r->cells, Cells(), sizeof(r->cells));
	}

	static void held_note_ghost(TaskNode *n)
	{
		Rec *r = NewRec(GHOST, n);
		if (!r) return;
		const uint8_t *e = Caster();
		memcpy(r->matrix, e + 0x40, sizeof(r->matrix));
		r->tint = *(const uint32_t *)(e + 0x28);
		r->mask = *(const uint32_t *)(e + 0x7C);
		r->b07 = e[7];
		r->b2b = e[0x2B];
		if (!g_frame_packets) g_frame_packets = FrameArena();
	}

	static void held_note_spin(TaskNode *n) { NewRec(SPIN, n); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the spark table at k/den: a cell the tick stepped (live: same flag, counter + 1) half way to its live position
	static void HeldSparks(const Rec &r, uint32_t bit, int num, int den)
	{
		memcpy(g_cells_held, r.cells, sizeof(g_cells_held));
		for (int i = 0; i < CELLS; i++)
		{
			Cell &c = g_cells_held[i];
			const Cell &live = Cells()[i];
			if (!((uint8_t)c.flag & bit)) continue;
			if (live.flag != c.flag)
			{
				if (num >= den) c.flag = 0; // freed by the tick: holds, gone at fraction 1 (the next tick)
				continue;
			}
			if (live.counter != (int16_t)(c.counter + 1)) continue; // not stepped (draw-only): holds
			for (int j = 0; j < 3; j++) c.pos[j] = lerp16(c.pos[j], live.pos[j], num, den);
			if (num >= den) c.counter = live.counter; // (at 1: the next tick's frame)
		}
	}

	// the frame arena, vertex buffer and the engine words a held ghost draw writes: private copies, all put back
	struct FrameScope
	{
		uint32_t cursor, verts, words[4];
		uint8_t shadow[0x80];
		FrameScope()
		{
			verts = var<uint32_t>(0x1D98B3C);
			cursor = FrameArena();
			words[0] = var<uint32_t>(0xB8B9E4);
			words[1] = var<uint32_t>(0xB8B9EC);
			words[2] = var<uint32_t>(0x1D99C18);
			words[3] = var<uint32_t>(0x1D99C1C);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			if (g_frame_packets && cursor > g_frame_packets)
			{
				const uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameArena() = (uint32_t)g_held_frame;
			var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0x1D99C1C) = words[3];
			var<uint32_t>(0x1D99C18) = words[2];
			var<uint32_t>(0xB8B9EC) = words[1];
			var<uint32_t>(0xB8B9E4) = words[0];
			var<uint32_t>(0x1D98B3C) = verts;
			FrameArena() = cursor;
		}
	};

	// the ghost's state on the next tick (fraction 1): its matrix (the kept one from counter 7 on), the colour the tint task
	// (if it runs before it in the queue) gives the caster, faded from counter 0x10 on; false: it ended on the tick
	static bool NextGhost(const Rec &r, Rec &next)
	{
		const TaskBody *g = (const TaskBody *)r.node;
		if (!(g->hdr.flags & 1)) return false;
		uint8_t *e = Caster();
		const uint32_t tint = *(const uint32_t *)(e + 0x28);
		const uint8_t b07 = e[7];
		for (TaskNode *t = QTasks()->head; t && t != r.node; t = t->next)
			if ((t->flags & 1) && (uint32_t)t->func == ORIG_TintTask)
			{
				TaskBody copy;
				memcpy(&copy, t, sizeof(copy));
				TintColour(&copy, Entity(copy.word0E), copy.counter);
			}
		if (g->counter >= 0x10)
		{
			GteSetFarColor(0, 0, 0);
			GteLoadRGBC(e + 0x28);
			GteSetIR0((g->counter - 0x10) << 8);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
			e[0x2B] = 2;
			e[7] = 0;
		}
		next = r;
		memcpy(next.matrix, g->counter > 6 ? (const void *)KEPT_MATRIX : (const void *)(e + 0x40), sizeof(next.matrix));
		next.tint = *(const uint32_t *)(e + 0x28);
		next.b07 = e[7];
		next.b2b = e[0x2B];
		*(uint32_t *)(e + 0x28) = tint;
		e[7] = b07;
		return true;
	}
	static Rec g_next_ghost;

	// the ghost as drawn on the real tick: the caster in that state (kept pose, matrix, colour, mask, +0x07 / +0x2B) around
	// the draw, then put back
	static void HeldGhost(const Rec &r)
	{
		uint8_t *e = Caster();
		uint8_t m[0x20];
		memcpy(m, e + 0x40, sizeof(m));
		const uint32_t tint = *(const uint32_t *)(e + 0x28), mask = *(const uint32_t *)(e + 0x7C);
		const uint8_t b07 = e[7], b2b = e[0x2B];
		BonesOut(e + 0x60, g_bones_live);
		memcpy(e + 0x40, r.matrix, sizeof(r.matrix));
		*(uint32_t *)(e + 0x28) = r.tint;
		*(uint32_t *)(e + 0x7C) = r.mask;
		e[7] = r.b07;
		e[0x2B] = r.b2b;
		BonesIn(e + 0x60, (const uint8_t *)KEPT_BONES);
		{
			FrameScope fs;
			ModelDraw(e);
		}
		BonesIn(e + 0x60, g_bones_live);
		e[0x2B] = b2b;
		e[7] = b07;
		*(uint32_t *)(e + 0x7C) = mask;
		*(uint32_t *)(e + 0x28) = tint;
		memcpy(e + 0x40, m, sizeof(m));
	}

	// the bone 0xF0 point at the caster's pose num/den of the way to the next tick's (its bones rebuilt from the pose after)
	static void HeldPoint(int num, int den, int16_t *out)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, num, den);
		GetEffectSpawnPosition(c, 0xF0, 0x1000, out);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		int16_t point[4] = {};
		bool have_point = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == SPIN && !have_point)
			{
				HeldPoint(DrawOnly() ? 0 : num, den, point);
				have_point = true;
			}
			if (r.kind == SPARKS_A || r.kind == SPARKS_B) HeldSparks(r, r.kind == SPARKS_A ? 1 : 2, num, den);
			// a replayed draw starts from the GTE state and scratch of the real one (the emitters record theirs after
			// their 0x48-byte block)
			uint8_t *w = (r.kind == SPARKS_A || r.kind == SPARKS_B) ? (uint8_t *)FieldAlloc(0x48) : nullptr;
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (w)
			{
				// (draw-only: the draw steps nothing; else it steps the private copy only)
				SparkDraw(w, g_cells_held, r.kind == SPARKS_A ? 1 : 2, r.kind == SPARKS_A ? SEQ_SparkA : SEQ_SparkB, r.kind == SPARKS_A);
				FieldFree(0x48);
			}
			else if (r.kind == GHOST)
			{
				// at fraction 1 the next tick's ghost (nothing when it ended); below 1 its colour steps (24-colour cycle): N's
				if (num < den) HeldGhost(r);
				else if (NextGhost(r, g_next_ghost)) HeldGhost(g_next_ghost);
			}
			else SpinDraw((const TaskBody *)r.body, point);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag324_held()
	{
		mark_held(tonk324::ORIG_EmitterA);
		mark_held(tonk324::ORIG_EmitterB);
		mark_held(tonk324::ORIG_GhostTask);
		mark_held(tonk324::ORIG_SpinTask);
		register_module_held(324, tonk324::HeldReady, tonk324::HeldDraw);
	}
}
