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

// 30 fps part of mag186_blaster.cpp (Blaster, effect 186). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws
// the state num/den of the way from the last real tick to the next one.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log) with the GTE
// registers and the Field_Alloc scratch each one started from; a held frame replays them in the same order with the vanilla draw
// code, each at its in-between state:
//   Beams       - as drawn (flipbook frame held; the beam point / direction are the charge's of the tick).
//   Beam rings  - fade along the second half (the switch to it holds).
//   Bolts       - fade along the last 4 ticks (the switch holds); the mesh frame held.
//   Sparks      - still: drawn as the tick drew them (flipbook frames held); the new ones (CRT rand) appear on the real frame.
//   Glow        - colour along its fade (from tick 0xE; the switch holds); flipbook frame held.
//   Shock wave  - fade along its ramp (from tick 0x10; the switch holds); texture frame held. The target it shakes (+0x20)
//                 is listed to the frame gate (HeldEntities: continuous).
//   Screen fade - the fade tile (frame arena) at its level half way to the next tick's (the end holds).
//   Splashes    - offset half way to the update's (point + velocity); flipbook frames held; a splash whose sequence ends holds
//                 below 1; the new ones appear on the real frame. Their packets go to a private copy of the frame arena.
// A part the next tick no longer runs (or whose counter did not move: draw-only tick) holds its picture (state N). The mouth /
// target / beam points and the aim rotation are the director's and the charge's of the tick (they run on every tick, before the
// drawing tasks), held between ticks. The module packets go to a private buffer (HeldScope); the engine words the sprite draws
// touch are put back. The screen flash / fade (director / fade task) are the engine's at 15 Hz.

namespace ff8fx
{
namespace blaster186
{
	static const uint32_t SCRATCH = 0x400;
	static const int MAX_RECS = 0x80, MAX_PARTS = 0x100;

	struct Rec
	{
		int kind;
		uint32_t node;
		uint8_t copy[0x24];       // the node as drawn
		int first, count;         // particle tasks: their particles' records
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PRec { uint32_t addr; uint8_t copy[0x18]; };

	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0;
	static bool g_overflow = false;
	static Rec *g_cur = nullptr;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_tick = 0xFFFFFFFF;
	static uint32_t g_frame_packets = 0;    // frame packet cursor at the tick's first splash / fade draw
	static Rec g_recs[MAX_RECS];
	static PRec g_parts[MAX_PARTS];
	static uint8_t g_held_packets[0xC000];  // one module arena
	static uint8_t g_held_frame[0x8000];
	static bool g_held = false;

	static void held_note_root()
	{
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_overflow = false;
		g_cur = nullptr;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, uint32_t n)
	{
		g_cur = nullptr;
		if (g_held || g_log_tick != g_real_tick) return nullptr;
		if (g_nrec >= MAX_RECS)
		{
			g_overflow = true;
			return nullptr;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		if ((kind == REC_SPLASHES || kind == REC_FADE) && g_frame_tick != g_real_tick)
		{
			g_frame_tick = g_real_tick;
			g_frame_packets = FrameCursor();
		}
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.copy, (const void *)n, sizeof(r.copy));
		r.first = g_npart;
		r.count = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_node(int kind, uint32_t n) { NewRec(kind, n); }
	static void held_note_parts(int kind, uint32_t n) { g_cur = NewRec(kind, n); }

	static void held_note_particle(uint32_t p)
	{
		if (!g_cur) return;
		if (g_npart >= MAX_PARTS)
		{
			g_overflow = true;
			return;
		}
		PRec &q = g_parts[g_npart++];
		q.addr = p;
		memcpy(q.copy, (const void *)p, sizeof(q.copy));
		g_cur->count++;
	}

	static bool HeldReady() { return g_log_tick == g_real_tick && !g_overflow; }

	// the splashes' frame packets: to a private copy of the part of the frame arena the tick wrote from its first splash draw
	struct FrameScope
	{
		bool on;
		uint32_t frame;
		FrameScope(bool drawn) : on(drawn)
		{
			if (!on) return;
			frame = FrameCursor();
			if (g_frame_packets && frame > g_frame_packets)
			{
				const uint32_t used = frame - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = P(g_held_frame);
		}
		~FrameScope()
		{
			if (on) FrameCursor() = frame;
		}
	};

	// the node runs on: same task, its counter one on (not on a draw-only tick)
	static bool Next(const Rec &r)
	{
		const TaskNode *t = (const TaskNode *)r.node;
		return !DrawOnly() && (t->flags & 1) && t->func == ((const TaskNode *)r.copy)->func && S16(r.node, 0xC) == (int16_t)(*(const int16_t *)(r.copy + 0xC) + 1);
	}

	// a particle the tick drew that lives on (same owner, its frame one on: not a new one in a freed slot)
	static bool PartAlive(const PRec &q)
	{
		return U32(q.addr, P_MASK) == *(const uint32_t *)(q.copy + P_MASK) && S16(q.addr, P_FRAME) == (int16_t)(*(const int16_t *)(q.copy + P_FRAME) + 1);
	}

	static void HeldFrame(int num, int den)
	{
		bool splash = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_SPLASHES || g_recs[i].kind == REC_FADE) splash = true;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame(splash && g_frame_tick == g_real_tick);
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		g_held = true;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			uint8_t cp[0x24];
			memcpy(cp, r.copy, sizeof(cp));
			const uint32_t c = P(cp);
			const int16_t cnt = S16(c, 0xC);
			const int k = Next(r) ? num : 0;
			switch (r.kind)
			{
			case REC_BEAM:
				BeamDraw(c);
				break;
			case REC_RING:
			case REC_BOLT:
			{
				const bool ring = r.kind == REC_RING;
				int32_t fade = 0;
				if (cnt >= S16(c, 0x20))
				{
					fade = ring ? RingFade(c, cnt) : BoltFade(c, cnt);
					if (k) fade = lerp_i(fade, ring ? RingFade(c, cnt + 1) : BoltFade(c, cnt + 1), k, den);
				}
				if (ring) BeamRingDraw(c, fade);
				else BoltDraw(c, fade);
				break;
			}
			case REC_GLOW:
			{
				int32_t col = GlowColour(cnt);
				if (k && cnt >= 0xE) col = lerp_i(col, GlowColour(cnt + 1), k, den);
				GlowDraw(c, col);
				break;
			}
			case REC_WAVE:
			{
				int32_t fade = WaveFade(cnt);
				if (k && cnt >= 0x10) fade = lerp_i(fade, WaveFade(cnt + 1), k, den);
				WaveDraw(c, fade);
				break;
			}
			case REC_FADE:
			{
				// the copy's counter is one on (the task counts before it draws)
				int32_t v = FadeLevel(c, cnt - 1);
				const int32_t nv = FadeLevel(c, cnt);
				if (k && nv >= 0) v = lerp_i(v, nv, k, den);
				FadeDraw(v);
				break;
			}
			case REC_SPARKS:
			{
				const uint32_t h = P(FieldAlloc(0xB4));
				U32(h, 0) = SEQ_Spark;
				U16(h, 0x24) = 0;
				for (int q = r.first; q < r.first + r.count; q++)
				{
					uint8_t pc[0x18];
					memcpy(pc, g_parts[q].copy, sizeof(pc));
					SparkDraw(h, P(pc));
				}
				FieldFree(0xB4);
				break;
			}
			default:
			{
				const uint32_t h = P(FieldAlloc(0xB4));
				const uint32_t w = P(FieldAlloc(0x48));
				SplashSetup(h, w);
				for (int q = r.first; q < r.first + r.count; q++)
				{
					const PRec &pr = g_parts[q];
					uint8_t pc[0x18];
					memcpy(pc, pr.copy, sizeof(pc));
					const uint32_t p = P(pc);
					if (!DrawOnly() && PartAlive(pr))
						for (int j = 0; j < 3; j++) S16(p, P_POS + 2 * j) = lerp16(S16(p, P_POS + 2 * j), S16(pr.addr, P_POS + 2 * j), num, den);
					SplashDraw(h, w, p);
				}
				FieldFree(0x48);
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_held = false;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
	}

	// held-frame entities (fx_held.h HeldEntity, drawn by the frame gate): the target while the shock wave shakes its +0x20 word
	// (+-sin x 40 by parity, put back at the end): continuous (its held frames draw the shake half way between the ticks)
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (max < 1 || !HeldReady()) return 0;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_WAVE)
			{
				held_entity_init(out[0], Target());
				return 1;
			}
		return 0;
	}

	static void register_mag186_held()
	{
		mark_held(ORIG_Beam);
		mark_held(ORIG_BeamRing);
		mark_held(ORIG_Bolt);
		mark_held(ORIG_Sparks);
		mark_held(ORIG_Glow);
		mark_held(ORIG_Wave);
		mark_held(ORIG_Splashes);
		mark_held(ORIG_Fade);
		register_module_held(186, HeldReady, HeldFrame);
		register_module_entity(186, HeldEntities);
	}
}
}
