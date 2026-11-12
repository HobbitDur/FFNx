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

// 30 fps part of mag301_gargantua_burrow.cpp (Gargantua's burrow, effect 301). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the layout advances in its play, a rock / puff moves after its draw). A real tick records
// its draws in order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in the
// same order on private packets, with the current camera (the frame gate's):
//   Layout  - prim::play_held (every object's exact in-between record), the block as the real play read it; blended vertex
//             frames go to a private buffer.
//   Body    - the caster at its in-between pose (pose_midpoint, the world bone matrices 0x5095B0 as the module builds them,
//             the bones put back from the pose 0x508C90 by the draw itself); polygons culled and sided against the ground
//             line from the in-between vertices; no mark is left (marks are spawned by real ticks only). Vertices go to the
//             private buffer, packets to a private copy of the frame arena.
//   Marks   - the marks as the real tick drew them (static points: the same flipbook frames) through the current camera.
//   Rocks   - position half way along their velocity, angles half way along their spin.
//   Puffs   - position half way along their velocity; flipbook frame N held.
// A part the next tick no longer draws (the master's counter 121, a rock / puff at its 16th tick) holds its picture (state N:
// records at N, pose N). Spawns (stream) and marks stay at 15 Hz. Held without interpolation: the caster's colour +0x2C fade
// (15 Hz steps, engine state). No task has a draw-only mode. The module packet arena alternates per tick: the held packets
// start where the tick's first draw started. The GTE, the Field_Alloc scratch, the frame arena cursor, the word 0x1D9770C and
// the engine words the sprite draws touch are put back.

namespace ff8fx
{
namespace burrow301
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick

	struct Rec
	{
		int kind;                 // DrawKind
		const void *node;
		int16_t counter;          // master: the counter as drawn
		PrimArg arg;              // play: the block as the play read it
		RockNode rock;            // rocks: the node as drawn
		DustNode dust;            // puffs: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0;    // frame arena cursor at the body draw
	static bool g_no_marks = false;         // a held body draw runs
	static Rec g_recs[3 + 0x20 + 0x20];
	static Mark g_marks[0x80];              // the marks as the real tick drew them
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_frame[0x40000];   // body packets (frame arena)
	static uint8_t g_held_verts[0x8000];    // blended vertex frames / the body's vertices (magic buffer + 0x1494)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static bool held_no_marks() { return g_no_marks; }

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

	static void held_note_play(const MasterNode *m, const PrimArg *arg)
	{
		Rec *r = NewRec(PLAY, m);
		if (!r) return;
		r->counter = m->counter;
		r->arg = *arg;
	}

	static void held_note_body(const MasterNode *m)
	{
		Rec *r = NewRec(BODY, m);
		if (!r) return;
		r->counter = m->counter;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
	}

	static void held_note_marks(const MasterNode *m)
	{
		Rec *r = NewRec(MARKS, m);
		if (!r) return;
		r->counter = m->counter;
		memcpy(g_marks, m->marks, sizeof(g_marks));
	}

	static void held_note_rock(const RockNode *n)
	{
		Rec *r = NewRec(ROCK, n);
		if (!r) return;
		r->rock = *n;
	}

	static void held_note_dust(const DustNode *n)
	{
		Rec *r = NewRec(DUST, n);
		if (!r) return;
		r->dust = *n;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		if (r.kind == PLAY || r.kind == BODY) return (uint32_t)(r.counter + 1) < 0x7A;
		if (r.kind == ROCK) return r.rock.counter + 1 < 0x10;
		if (r.kind == DUST) return r.dust.counter + 1 < 0x10;
		return false; // marks: static
	}

	// a held frame's frame arena: a copy of the real tick's (so bytes the draw leaves alone are the real ones), put back
	struct FrameScope
	{
		uint32_t cursor;
		FrameScope()
		{
			cursor = FrameCursor();
			if (g_frame_packets && cursor > g_frame_packets)
			{
				const uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
		}
		~FrameScope() { FrameCursor() = cursor; }
	};

	// angle bytes (x 16) num/den of the way along their spin (the byte wraps: the angle goes on past it)
	static int16_t HeldAngle(int8_t a, int8_t spin, int k, int den) { return (int16_t)((a << 4) + (spin << 4) * k / den); }

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		const int16_t w70c = var<int16_t>(0x1D9770C);
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
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case PLAY:
			{
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_verts;
				prim::play_held((prim::Layout *)((const MasterNode *)r.node)->layout, PrimCallback, (int)&arg, k, den);
				break;
			}
			case BODY:
			{
				uint8_t *c = Caster();
				pose_midpoint(c + 0x60, c + 0x6C, k, den);
				g_no_marks = true;
				DrawBody(c, r.counter, GROUND, (uint32_t)g_held_verts);
				g_no_marks = false;
				break;
			}
			case MARKS:
			{
				uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
				F<uint16_t>(h, 0x92) = 0;
				F<uint16_t>(h, 0x90) = 0;
				F<uint16_t>(h, 0x8E) = 0;
				F<uint16_t>(h, 0x8A) = 0;
				F<uint16_t>(h, 0x88) = 0;
				F<uint16_t>(h, 0x86) = 0;
				F<uint16_t>(h, 0x60) = 0;
				F<uint16_t>(h, 0x58) = 0;
				F<uint16_t>(h, 0x50) = 0;
				F<uint16_t>(h, 0x48) = 0;
				for (int j = 0; j < 0x80; j++)
					if (g_marks[j].life != 0) MarkSprite(h, &g_marks[j]);
				FieldFree(0xB4);
				break;
			}
			case ROCK:
			{
				const RockNode &n = r.rock;
				int16_t pos[3];
				pos[0] = lerp16(n.pos[0], (int16_t)(n.pos[0] + (int16_t)(n.vel[0] << 4)), k, den);
				pos[1] = lerp16(n.pos[1], (int16_t)(n.pos[1] + (int16_t)(n.vel[1] << 2)), k, den);
				pos[2] = lerp16(n.pos[2], (int16_t)(n.pos[2] + (int16_t)(n.vel[2] << 4)), k, den);
				RockDraw(pos, HeldAngle(n.angle[0], n.spin[0], k, den), HeldAngle(n.angle[1], n.spin[1], k, den));
				break;
			}
			default:
			{
				const DustNode &n = r.dust;
				int16_t pos[4] = {};
				for (int j = 0; j < 3; j++) pos[j] = lerp16(n.pos[j], (int16_t)(n.pos[j] + n.vel[j]), k, den);
				pos[3] = n.counter;
				DustDraw(pos, n.counter);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag301_held()
	{
		mark_held(burrow301::ORIG_MasterTask);
		mark_held(burrow301::ORIG_RockTask);
		mark_held(burrow301::ORIG_DustTask);
		register_module_held(301, burrow301::HeldReady, burrow301::HeldDraw);
	}
}
