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

// 30 fps part of mag030_counter_laser_eye.cpp: the held-frame drawing of the actor systems, the laser
// beams and the sparks (included at the end of mag030_counter_laser_eye.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// Field_Alloc scratch):
//   - actor systems (task 0x873B90; the design of mag029_wind_blast_held.inc): each noted system's
//     particle actors are redrawn from copies whose view positions and draw matrix (rotation /
//     scale) are half way to the next tick's (the next tick's update pass runs on second copies, in
//     list order, with the emitters' placement / motion applied to the emitters and put back);
//     a morphing model's vertex block is half way to the next tick's morph (0x876F30); flipbook
//     frame, model, fade and colours hold (the next tick's at fraction 1); an actor ending on the
//     next tick holds (a model: not drawn at fraction 1); an actor not in its motion phase holds.
//   - laser beams (0x878930): the node and the rotation of each of its (up to three) draws
//     (0x878CC0). The next tick's draws are predicted by running the task's update (laser_update:
//     eye position, state, matrices; no sparks) on a copy of the node with the draws captured; each
//     draw is redrawn with its position, scale and rotation matrix half way to the same draw of the
//     next tick (model, colour: the drawn tick's, the next tick's at fraction 1; hidden flag: the drawn tick's). A draw
//     the next tick does not make (the flash once the width is full, a finished beam) holds.
//   - sparks (0x878F20, sprite sequences 0x878F80): position half way to the next tick's (its state
//     1 step run on a copy); flipbook frame held (the next one at fraction 1); a spark ending on the
//     next tick holds. Sparks born on the next tick are not drawn (random growth stays at 15 Hz).
// The packets go to a private buffer (module cursor 0x26D40BC); the module globals, the module scratch
// stack, the Field_Alloc scratch and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace lasereye
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x26D40B8, BSS_HI = 0x26D6A3C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t LASER_BYTES = 0x90, SPARK_BYTES = 0x60;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x876F30) rewrites them in the actor data, which every actor system of
	// the cast shares, so a later system's update overwrites what an earlier one drew with
	struct VertexBlocks
	{
		uint32_t n;
		uint32_t addr[16], len[16];
		uint8_t bytes[0x4000];
		void save(uint32_t sys)
		{
			n = 0;
			const uint32_t tab = U32(sys, 0x22C);
			if (!tab) return;
			uint32_t used = 0;
			for (int k = 0; k < 16; k++)
			{
				const uint32_t blk = U32(tab, k * 4);
				if (!blk) continue;
				const uint32_t bytes_n = U32(blk, 4) * 8; // vertex count +4, 8-byte vertices from +8
				if (bytes_n == 0 || used + bytes_n > sizeof(bytes)) continue;
				addr[n] = blk + 8;
				len[n] = bytes_n;
				memcpy(bytes + used, (const void *)(blk + 8), bytes_n);
				used += bytes_n;
				n++;
			}
		}
		void restore() const
		{
			uint32_t used = 0;
			for (uint32_t i = 0; i < n; i++)
			{
				memcpy((void *)addr[i], bytes + used, len[i]);
				used += len[i];
			}
		}
	};

	// the state a draw of the real tick started from
	struct DrawState
	{
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint32_t fa;             // Field_Alloc pointer
		uint8_t falloc[0x400];   // and the scratch above it
		void save()
		{
			memcpy(gte, (const void *)GTE_D, GTE_D_N);
			memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
			fa = MEM<uint32_t>(FALLOC_PTR);
			memcpy(falloc, (const void *)fa, sizeof(falloc));
		}
		void restore() const
		{
			memcpy((void *)GTE_D, gte, GTE_D_N);
			memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
			MEM<uint32_t>(FALLOC_PTR) = fa;
			memcpy((void *)fa, falloc, sizeof(falloc));
		}
	};

	struct SysNote
	{
		uint32_t sys;
		DrawState st;
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		VertexBlocks verts; // the vertex blocks its models were drawn with
	};
	struct LaserNote
	{
		uint32_t node;
		int idx;                  // draw index of the node on that tick (0 beam, 1 head, 2 flash)
		uint8_t bytes[LASER_BYTES];
		uint8_t mtx[0x20];
		DrawState st;
	};
	struct SparkNote
	{
		uint32_t node;
		uint8_t bytes[SPARK_BYTES];
		DrawState st;
	};
	enum { EV_SYS, EV_LASER, EV_SPARK };
	struct Event { int kind, i; };

	static const int MAX_SYS = 8, MAX_LASER = 64, MAX_SPARK = 128, MAX_EV = MAX_SYS + MAX_LASER + MAX_SPARK;
	static SysNote g_sys[MAX_SYS];
	static LaserNote g_laser[MAX_LASER];
	static SparkNote g_spark[MAX_SPARK];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nlaser = 0, g_nspark = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000];
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw
	static bool g_held_drawing = false;

	// prediction of a laser node's next tick: its draws captured
	struct Captured { uint8_t bytes[LASER_BYTES]; uint8_t mtx[0x20]; };
	static DrawState g_cap_st[4]; // the state each captured draw would start from
	static bool g_capturing = false;
	static Captured g_cap[4];
	static int g_ncap = 0;

	static void held_begin_tick()
	{
		if (g_note_tick == g_real_tick) return;
		g_note_tick = g_real_tick;
		g_nsys = g_nlaser = g_nspark = g_nev = 0;
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
	}

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		held_begin_tick();
		if (g_nsys >= MAX_SYS || g_nev >= MAX_EV) return;
		SysNote &n = g_sys[g_nsys];
		n.sys = sys;
		n.st.save();
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		n.verts.save(sys);
		g_ev[g_nev++] = { EV_SYS, g_nsys++ };
	}

	// a laser draw (0x878CC0): captured while predicting (not drawn), noted on a real tick
	static bool held_laser_draw(uint32_t node, uint32_t matrix)
	{
		if (g_capturing)
		{
			if (g_ncap < 4)
			{
				memcpy(g_cap[g_ncap].bytes, (const void *)node, LASER_BYTES);
				memcpy(g_cap[g_ncap].mtx, (const void *)matrix, 0x20);
				g_cap_st[g_ncap].save();
				g_ncap++;
			}
			return true;
		}
		if (g_held_drawing) return false;
		held_begin_tick();
		if (g_nlaser >= MAX_LASER || g_nev >= MAX_EV) return false;
		LaserNote &n = g_laser[g_nlaser];
		n.node = node;
		n.idx = 0;
		for (int i = 0; i < g_nlaser; i++)
			if (g_laser[i].node == node) n.idx++;
		memcpy(n.bytes, (const void *)node, LASER_BYTES);
		memcpy(n.mtx, (const void *)matrix, 0x20);
		n.st.save();
		g_ev[g_nev++] = { EV_LASER, g_nlaser++ };
		return false;
	}

	// a spark draw (0x878F80) on a real tick
	static void held_note_spark(uint32_t node)
	{
		if (g_held_drawing) return;
		held_begin_tick();
		if (g_nspark >= MAX_SPARK || g_nev >= MAX_EV) return;
		SparkNote &n = g_spark[g_nspark];
		n.node = node;
		memcpy(n.bytes, (const void *)node, SPARK_BYTES);
		n.st.save();
		g_ev[g_nev++] = { EV_SPARK, g_nspark++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x875F00 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor (flipbook frame, variant, fade, colour). P_ENDS: the actor ends on
	// the next tick (not drawn there); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7419C0 (= 0x877040) (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		a_741120(nxt, def); // (a morphing model's step rewrites its vertex block: put back by the caller)
		for (int k = 0; k < 9; k++) // rotation / scale of the draw matrix +0xAC
			S16(cur, 0xAC + 2 * k) = (int16_t)lerp_i(S16(cur, 0xAC + 2 * k), S16(nxt, 0xAC + 2 * k), num, den);
		for (int k = 0; k < 3; k++)
			S32(cur, 0xC0 + 4 * k) = lerp_i(S32(cur, 0xC0 + 4 * k), S32(nxt, 0xC0 + 4 * k), num, den);
		const int32_t copies = S8(cur, 0x1D8);
		for (int32_t i = 0; i < copies && i < 16; i++)
			for (int k = 0; k < 3; k++)
			{
				const int32_t o = 0x19C + 8 * i + 2 * k;
				S16(cur, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den);
			}
		// (fade +0x1CE and colour +0x16C hold: the model renderer's depth-cued colours are not monotonic
		// in the fade across draws, a lerped fade can leave [N, N+1])
		// at fraction 1 the next tick's actor (same draws: same copy count, still drawn)
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x875F00)
		}
		return P_NEXT;
	}

	// the actors of a noted system redrawn half way (see mag029_wind_blast_held.inc)
	static void DrawSystem(const SysNote &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(n.verts.bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
		// 0x875D80 / 0x874700 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x875EC0) in list order: the emitters' placement / motion
		// (0x877190 without its emission and life count, applied to the emitters themselves and put
		// back after: their particles read them) and the drawn particles' steps (Predict)
		static const int MAX_EMIT = 64;
		static uint8_t esave[MAX_EMIT][EMIT_BYTES];
		uint32_t eaddr[MAX_EMIT];
		int ne = 0, pi = 0;
		int pr[MAX_ACTORS];
		bool morph = false;
		for (uint32_t act = U32(n.sys, 0x2C); act; act = U32(act, 4))
		{
			if (S16(act, 8) == 0)
			{
				if (ne >= MAX_EMIT) continue;
				eaddr[ne] = act;
				memcpy(esave[ne++], (const void *)act, EMIT_BYTES);
				if (U16(act, 0x62) == 0)
				{
					cl_878380(act);
					U16(act, 0x62) = (uint16_t)(U16(act, 0x62) + 1);
				}
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					callp(0x877B40, act);
				else if (mode == 4)
					callp(0x877C90, act);
			}
			else if (pi < nl && act == list[pi])
			{
				pr[pi] = Predict(list[pi], P(copies[pi]), P(nxts[pi]), num, den);
				if (pr[pi] == P_NEXT && U8(DefOf(list[pi]), 0x16) == 5) morph = true;
				pi++;
			}
		}
		for (int e = ne - 1; e >= 0; e--)
			memcpy((void *)eaddr[e], esave[e], EMIT_BYTES);
		// morphing models: the vertex blocks half way between the drawn ones and the next tick's
		// (the next tick's morph steps 0x876F30 in list order, as its update pass writes them)
		if (morph)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					cl_876F30(P(nxts[i]), DefOf(list[i]));
			uint32_t used = 0;
			for (uint32_t b = 0; b < n.verts.n; b++)
			{
				memcpy(verts_next + used, (const void *)n.verts.addr[b], n.verts.len[b]);
				used += n.verts.len[b];
			}
			n.verts.restore();
			used = 0;
			for (uint32_t b = 0; b < n.verts.n; b++)
			{
				for (uint32_t o = 0; o < n.verts.len[b]; o += 2)
					S16(n.verts.addr[b], o) = (int16_t)lerp_i(S16(n.verts.addr[b], o), S16(P(verts_next) + used, o), num, den);
				used += n.verts.len[b];
			}
		}
		else
			n.verts.restore();
		// the draws start from the real tick's GTE state and scratch
		n.st.restore();
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				cl_875E40(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					cl_875E40(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				cl_874760(P(copies[i]), def);
		}
	}

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// the next tick's draws of a laser node (laser_update on a copy, draws captured)
	static void PredictLaser(uint32_t node)
	{
		g_ncap = 0;
		if (!live(node, ORIG_Laser)) return;
		alignas(4) static uint8_t copy[LASER_BYTES];
		memcpy(copy, (const void *)node, LASER_BYTES);
		g_capturing = true;
		laser_update(P(copy));
		g_capturing = false;
	}

	static void DrawLaser(const LaserNote &n, int num, int den)
	{
		alignas(4) static uint8_t fake[LASER_BYTES];
		alignas(4) static uint8_t mtx[0x20];
		memcpy(fake, n.bytes, LASER_BYTES);
		memcpy(mtx, n.mtx, 0x20);
		// the same draw of the next tick (made unless the beam is hidden then)
		const bool next = n.idx < g_ncap && (U8(P(g_cap[n.idx].bytes), 0x26) & 4) == 0;
		if (next)
		{
			const Captured &c = g_cap[n.idx];
			const uint32_t f = P(fake), b = P(c.bytes), m = P(mtx), bm = P(c.mtx);
			for (int k = 0; k < 3; k++)
				S16(f, 0x1C + 2 * k) = (int16_t)lerp_i(S16(f, 0x1C + 2 * k), S16(b, 0x1C + 2 * k), num, den);
			for (int k = 0; k < 4; k++)
				S32(f, 0x30 + 4 * k) = lerp_i(S32(f, 0x30 + 4 * k), S32(b, 0x30 + 4 * k), num, den);
			for (int k = 0; k < 9; k++)
				S16(m, 2 * k) = (int16_t)lerp_i(S16(m, 2 * k), S16(bm, 2 * k), num, den);
			if (num >= den)
			{
				U32(f, 0x40) = U32(b, 0x40);
				U32(f, 0x4C) = U32(b, 0x4C);
				U16(f, 0x50) = U16(b, 0x50);
			}
		}
		// the draw starts from the drawn tick's state (at fraction 1: the next tick's, as captured)
		if (num >= den && next)
			g_cap_st[n.idx].restore();
		else
			n.st.restore();
		g_held_drawing = true;
		cl_878CC0(P(fake), P(mtx));
		g_held_drawing = false;
	}

	static void DrawSpark(const SparkNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[SPARK_BYTES], nxt[SPARK_BYTES];
		memcpy(cur, n.bytes, SPARK_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		if (live(n.node, ORIG_Spark) && S8(n.node, 0x29) == 1 && (U8(n.node, 0x26) & 4) == 0)
		{
			// the next tick's state 1 step (0x879050) on a copy; drawn there unless it ends
			memcpy(nxt, (const void *)n.node, SPARK_BYTES);
			cl_879050(x1);
			if ((U8(x1, 0x26) & 4) == 0)
			{
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
				if (num >= den)
				{
					U16(c, 0x50) = U16(x1, 0x50);
					U32(c, 0x4C) = U32(x1, 0x4C);
					U16(c, 0x54) = U16(x1, 0x54);
				}
			}
		}
		n.st.restore();
		g_held_drawing = true;
		cl_878F80(c);
		g_held_drawing = false;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_030;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		now.save();
		uint8_t sprite_cam[SPRITE_CAM_N];
		memcpy(sprite_cam, (const void *)SPRITE_CAM, SPRITE_CAM_N);
		// the Field_Alloc scratch the replayed draws may write: the windows at the noted pointers and
		// at the current one (predictions, drawn at fraction 1 from their captured state)
		static const int MAX_WIN = 8;
		static const uint32_t WIN = 0x800;
		static uint8_t fa_win[MAX_WIN][WIN];
		uint32_t fa_base[MAX_WIN];
		int nwin = 0;
		auto add_win = [&](uint32_t a)
		{
			for (int w = 0; w < nwin; w++)
				if (a >= fa_base[w] && a + 0x400 <= fa_base[w] + WIN) return;
			if (nwin < MAX_WIN) { fa_base[nwin] = a; memcpy(fa_win[nwin], (const void *)a, WIN); nwin++; }
		};
		add_win(now.fa);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			add_win(ev.kind == EV_SYS ? g_sys[ev.i].st.fa : ev.kind == EV_LASER ? g_laser[ev.i].st.fa : g_spark[ev.i].st.fa);
		}
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		uint32_t predicted = 0;
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(g_sys[ev.i], num, den);
			else if (ev.kind == EV_LASER)
			{
				const LaserNote &n = g_laser[ev.i];
				if (n.idx == 0 || predicted != n.node)
				{
					PredictLaser(n.node);
					predicted = n.node;
				}
				DrawLaser(n, num, den);
			}
			else
				DrawSpark(g_spark[ev.i], num, den);
		}
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		memcpy((void *)SPRITE_CAM, sprite_cam, SPRITE_CAM_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag030_held()
	{
		mark_held(act::lasereye::ORIG_ActorSystem);
		mark_held(act::lasereye::ORIG_Laser);
		mark_held(act::lasereye::ORIG_Spark);
		register_module_held(30, act::lasereye::HeldReady, act::lasereye::HeldFrame);
	}
}
