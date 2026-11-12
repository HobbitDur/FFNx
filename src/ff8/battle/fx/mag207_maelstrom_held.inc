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

// 30 fps part of mag207_maelstrom.cpp (Maelstrom, effect 207; the held part of Gravija 214 with this module's lens warp,
// layout block and particles). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws the state num/den of
// the way from the last real tick to the next one.
//
// Every task draws THEN updates. A real tick records its draws in order with the GTE state and the Field_Alloc scratch
// each one started from; a held frame replays them in the same order on private packets, with the current camera (the
// frame gate's) and the module matrix of the real tick:
//   Layout    - the targets' average point (fixed); prim::play_held (every object's exact in-between record); the
//               texture scroll of the block (counter x 32) num/den of the way to the next tick's; the last play
//               (counter 0x4F) holds.
//   Warp      - the targets' head / feet box with the current camera, the scale ramp of the fractional b = counter -
//               0x20 + num/den (S (b den + num) / (8 den) below b = 8), the fade 0x1000 - sin((b den + num) x 2048 /
//               (48 den)), the lens row (0x80 scales by vertex length) num/den of the way from the tick's row to the
//               next tick's, scale by scale; the last warp (b = 0x2F) holds. The screen copy it draws is the one VRAM image
//               taken at master counter 1.
//   Particles - point + (velocity + acceleration) x num/den (the next tick's motion), the sequence frame stays the
//               tick's; a particle's last draw holds. Spawns (CRT rand) stay at 15 Hz.
// The layout's morph buffer (magic buffer + 0xCE8), the warp vertices (+ 0xEE0: the out-of-screen flags keep their bits
// when outside) and the warp frame 0x2517A58 are private copies of the real ones. No task has a draw-only mode. The
// module packet arena alternates per tick: the held packets start where the tick's first draw started. The GTE, the
// Field_Alloc scratch, the word 0x1D9770C and the engine words the sprite draws touch are put back; no node, module
// global or entity word is written; the screen-copy request (master, counter 1) is logic: not repeated.

namespace ff8fx
{
namespace maelstrom207
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		int16_t counter;          // main: the counter as drawn
		ParticleNode part;        // particle: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[2 + 0x40];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[MORPH_BYTES]; // layout morph buffer (magic buffer + 0xCE8)
	static uint8_t g_held_vbuf[VBUF_BYTES];   // warp vertices (magic buffer + 0xEE0)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *held_rec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_main(int kind, TaskNode *n)
	{
		Rec *r = held_rec(kind, n);
		if (r) r->counter = ((const MainNode *)n)->counter;
	}

	static void held_note_particle(TaskNode *n)
	{
		Rec *r = held_rec(K_PARTICLE, n);
		if (r) memcpy(&r->part, n, sizeof(ParticleNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws this part again (else the last picture holds)
	static bool NextDraws(const Rec &r)
	{
		switch (r.kind)
		{
			case K_PLAY: return r.counter + 1 < 0x50;
			case K_WARP: return r.counter + 1 - 0x20 < 0x30;
			default: return r.part.counter + 1 < 0x10;
		}
	}

	static void HeldWarp(const Rec &r, int num, int den)
	{
		const MainNode *w = (const MainNode *)r.node;
		const int32_t b = r.counter - 0x20;
		Mat4x3 frame = WarpFrame();
		int32_t z;
		if (!WarpBox(w, frame, &z)) return;
		const int32_t x = b * den + num;
		const int32_t fade = 0x1000 - ComputeSin((int32_t)(((uint32_t)x << 11) / (uint32_t)(48 * den)));
		if (b < 8)
		{
			frame.m[0][0] = (int16_t)((uint32_t)mul32(frame.m[0][0], x) / (uint32_t)(8 * den));
			frame.m[1][1] = (int16_t)((uint32_t)mul32(frame.m[1][1], x) / (uint32_t)(8 * den));
		}
		GteSetRotMatrix(&frame);
		GteSetTransVector(&frame);
		// the lens row num/den of the way from the tick's row to the next tick's (each of its 0x80 scales)
		const int16_t *r0 = LENS_TABLE + WarpRow(b) * 0x80, *r1 = LENS_TABLE + WarpRow(b + 1) * 0x80;
		static int16_t row[0x80];
		for (int i = 0; i < 0x80; i++) row[i] = (int16_t)(r0[i] + (r1[i] - r0[i]) * num / den);
		Warp(MESH, MESH + 8, (z >> 2) + 0x11, fade, row, g_held_vbuf);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		memcpy(g_held_morph, (const void *)(TexBase() + MORPH), sizeof(g_held_morph));
		memcpy(g_held_vbuf, (const void *)(TexBase() + VBUF), sizeof(g_held_vbuf));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == K_PLAY)
			{
				const MainNode *m = (const MainNode *)r.node;
				PrimArg arg;
				memcpy(arg.pos, m->avg, 8);
				arg.kinds = KINDS;
				arg.depth = -0x80;
				arg.scroll = (int16_t)((r.counter << 5) + 32 * k / den);
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)m->layout, PrimCallback, (int)&arg, k, den);
			}
			else if (r.kind == K_WARP) HeldWarp(r, k, den);
			else
			{
				const ParticleNode &p = r.part;
				int16_t pos[4]; // (+3: the node's next word, the counter, as the real draw's point is followed by it)
				pos[3] = p.counter;
				for (int c = 0; c < 3; c++) pos[c] = (int16_t)(p.pos[c] + (int16_t)(p.vel[c] + (int16_t)p.acc[c]) * k / den);
				ParticleDraw(pos, p.counter);
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag207_held()
	{
		mark_held(maelstrom207::ORIG_MainTask);
		mark_held(maelstrom207::ORIG_ParticleTask);
		register_module_held(207, maelstrom207::HeldReady, maelstrom207::HeldFrame);
	}
}
