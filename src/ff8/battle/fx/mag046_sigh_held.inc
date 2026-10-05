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

// 30 fps part of mag046_sigh.cpp: the held-frame drawing of the actor systems, the sigh's prim
// models and the breath puffs (included at the end of mag046_sigh.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// Field_Alloc scratch):
//   - actor systems (task 0x832100; the design of mag041_dribble_held.inc /
//     mag030_counter_laser_eye_held.inc): each noted system's particle actors are redrawn from
//     copies whose view positions and draw matrix are half way to the next tick's (the next tick's
//     update pass on second copies, emitters put back); flipbook frame, fade and colours hold (the
//     next tick's at fraction 1); an actor ending on the next tick holds.
//   - the sigh (task 0x8359C0, six draws 0x835B30): the two prim models redrawn at the three
//     angles with the fade +0x50 half way to the next tick's (its fade state 0x8362C0 predicted
//     from the table) and the vertex colours half way between the colour frame drawn and the next
//     one (written into the models for the draw and put back). The texture scrolls (VRAM blits)
//     stay at 15 Hz. A sigh ending on the next tick holds.
//   - breath puffs (task 0x836090, sprite sequences 0x8360F0): position half way to the next
//     tick's (its motion state 0x8361C0 run on a copy of the noted node); flipbook frame held (the
//     next one at fraction 1); a puff ending or hidden on the next tick holds. Puffs spawned on the
//     next tick are not drawn (random growth stays at 15 Hz).
// The packets go to a private buffer (module cursor 0x264EC64, started from the real tick's
// packets at the tick's first draw); the module globals, the entities, the module scratch stack,
// the Field_Alloc scratch, the models' colour words and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace sigh
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x264EC60, BSS_HI = 0x26527B4;
	static const uint32_t ENT_LO = 0x1D972C0, ENT_HI = 0x1D972C0 + 7 * 0x9C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_BYTES = 0x8C;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries)
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
	struct NodeNote
	{
		uint32_t node;
		uint8_t bytes[NODE_BYTES];
		DrawState st;
	};
	enum { EV_SYS, EV_SIGH, EV_PUFF };
	struct Event { int kind, i; };

	static const int MAX_SYS = 8, MAX_SIGH = 8, MAX_PUFF = 96, MAX_EV = MAX_SYS + MAX_SIGH + MAX_PUFF;
	static SysNote g_sys[MAX_SYS];
	static NodeNote g_sigh[MAX_SIGH];
	static NodeNote g_puff[MAX_PUFF];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nsigh = 0, g_npuff = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000];
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw

	// real tick: the master runs (new tick: no draw noted yet)
	static void held_note_master()
	{
		ff8fx::act::held_note_master(); // the ported master ran on this tick (act::held_ready)
		g_note_tick = g_real_tick;
		g_nsys = g_nsigh = g_npuff = g_nev = 0;
		g_pkt_start = 0;
	}

	// the first draw of the tick fixes the packet arena start (the master picks the arena by tick
	// parity after its note)
	static void held_first_draw()
	{
		if (g_note_tick != g_real_tick)
		{
			g_note_tick = g_real_tick;
			g_nsys = g_nsigh = g_npuff = g_nev = 0;
		}
		if (g_nev == 0) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
	}

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		held_first_draw();
		if (g_nsys >= MAX_SYS || g_nev >= MAX_EV) return;
		SysNote &n = g_sys[g_nsys];
		n.sys = sys;
		n.st.save();
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		n.verts.save(sys);
		g_ev[g_nev++] = { EV_SYS, g_nsys++ };
	}

	static void note_node(NodeNote *arr, int &cnt, int max, int kind, uint32_t node)
	{
		held_first_draw();
		if (cnt >= max || g_nev >= MAX_EV) return;
		NodeNote &n = arr[cnt];
		n.node = node;
		memcpy(n.bytes, (const void *)node, NODE_BYTES);
		n.st.save();
		g_ev[g_nev++] = { kind, cnt++ };
	}
	// real tick: the sigh's six model draws are about to run (colour frame +0x82 written)
	static void held_note_sigh(uint32_t node) { note_node(g_sigh, g_nsigh, MAX_SIGH, EV_SIGH, node); }
	// real tick: a breath puff is about to be drawn (0x8360F0)
	static void held_note_puff(uint32_t node) { note_node(g_puff, g_npuff, MAX_PUFF, EV_PUFF, node); }

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// ---- actor systems (as mag041_dribble_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x832FF0 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor. P_ENDS: the actor ends on the next tick (not drawn there);
	// P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7419C0 (= 0x834130) (its end spawns: not called)
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
		// (fade +0x1CE and colour +0x16C hold)
		// at fraction 1 the next tick's actor (same draws: same copy count, still drawn)
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x832FF0)
		}
		return P_NEXT;
	}

	// the actors of a noted system redrawn half way
	static void DrawSystem(const SysNote &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(n.verts.bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
		// 0x832E70 / 0x832C70 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x832FB0) in list order: the emitters' placement / motion
		// (0x834280 without its emission and life count, applied to the emitters themselves and put
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
					a_742D00(act);
					U16(act, 0x62) = (uint16_t)(U16(act, 0x62) + 1);
				}
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					callp(0x834C30, act);
				else if (mode == 4)
					callp(0x834D80, act);
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
		if (morph)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					sg_834020(P(nxts[i]), DefOf(list[i]));
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
				sg_832F30(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					sg_832F30(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				sg_832CD0(P(copies[i]), def);
		}
	}

	// ---- the sigh

	// the primitive records of a prim model's colour words (as 0x835C20 addresses them)
	static uint32_t ColorRecs(uint32_t model, int32_t *n)
	{
		const int32_t idx = S32(model, 0) / 4;
		*n = S32(model, 0x1C + idx * 4);
		return model + 0x1C + (uint32_t)idx * 4 + 4;
	}

	static void DrawSigh(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[NODE_BYTES];
		alignas(4) static uint8_t col[2][0x4000];
		alignas(4) static uint8_t rec_save[2][0x9000];
		memcpy(cur, n.bytes, NODE_BYTES);
		const uint32_t c = P(cur);
		// the next tick: drawn unless the sigh ends after this tick
		const bool next = (U8(c, 0x26) & 1) == 0;
		const int32_t f0 = S16(c, 0x82);                     // colour frame drawn
		const int32_t f1 = f0 + 1 > 0x10 ? 0x10 : f0 + 1;   // the next tick's
		uint32_t recs[2] = { 0, 0 };
		int32_t nrec[2] = { 0, 0 };
		if (next)
		{
			// fade: the next tick's fade state 0x8362C0
			if (S8(c, 0x29) == 3)
			{
				int16_t f = MEM<int16_t>(FADE_TABLE + (uint32_t)(int32_t)(int16_t)(S16(c, 0x88) + 1) * 2);
				if (f >= 0x1000) f = 0x1000;
				S16(c, 0x50) = (int16_t)lerp_i(S16(c, 0x50), f, num, den);
			}
			// the colour words half way between the frames f0 and f1 (written into the models, put
			// back below)
			const uint32_t tabs[2] = { COLOR_TABLE_A, COLOR_TABLE_B };
			for (int m = 0; m < 2; m++)
			{
				const uint32_t model = U32(c, 0x74 + 4 * m);
				recs[m] = ColorRecs(model, &nrec[m]);
				if (nrec[m] <= 0 || (uint32_t)nrec[m] * 0x24 > sizeof(rec_save[m]) || (uint32_t)nrec[m] * 0x10 > sizeof(col[m]))
				{
					nrec[m] = 0;
					continue;
				}
				memcpy(rec_save[m], (const void *)recs[m], (uint32_t)nrec[m] * 0x24);
				const uint32_t s0 = MEM<uint32_t>(tabs[m] + (uint32_t)f0 * 4);
				const uint32_t s1 = MEM<uint32_t>(tabs[m] + (uint32_t)f1 * 4);
				for (uint32_t b = 0; b < (uint32_t)nrec[m] * 0x10; b++)
					col[m][b] = (uint8_t)lerp_i(U8(s0, b), U8(s1, b), num, den);
				sg_835C20(model, P(col[m]));
			}
		}
		// the six draws of 0x8359C0 from the real tick's state
		n.st.restore();
		uint32_t ang = c + 0x7C;
		for (int k = 3; k != 0; k--, ang += 2)
		{
			U16(c, 0x46) = U16(ang, 0);
			U32(c, 0x4C) = U32(c, 0x74);
			sg_835B30(c);
			U32(c, 0x4C) = U32(c, 0x78);
			sg_835B30(c);
		}
		for (int m = 1; m >= 0; m--)
			if (nrec[m] > 0)
				memcpy((void *)recs[m], rec_save[m], (uint32_t)nrec[m] * 0x24);
	}

	// ---- breath puffs

	static void DrawPuff(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[NODE_BYTES], nxt[NODE_BYTES];
		memcpy(cur, n.bytes, NODE_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		// the next tick runs the motion state 0x8361C0 unless the puff ends after this tick
		if (S8(c, 0x29) == 1 && (U8(c, 0x26) & 5) == 0)
		{
			memcpy(nxt, n.bytes, NODE_BYTES);
			sg_8361C0(x1);
			if ((U8(x1, 0x26) & 4) == 0) // drawn on the next tick
			{
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
				if (num >= den)
					U16(c, 0x50) = U16(x1, 0x50);
			}
		}
		n.st.restore();
		sg_8360F0(c);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_046;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], ents[ENT_HI - ENT_LO];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(ents, (const void *)ENT_LO, sizeof(ents));
		now.save();
		uint8_t sprite_cam[SPRITE_CAM_N];
		memcpy(sprite_cam, (const void *)SPRITE_CAM, SPRITE_CAM_N);
		// the Field_Alloc scratch the replayed draws may write: the windows at the noted pointers
		// and at the current one
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
			add_win(ev.kind == EV_SYS ? g_sys[ev.i].st.fa : ev.kind == EV_SIGH ? g_sigh[ev.i].st.fa : g_puff[ev.i].st.fa);
		}
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(g_sys[ev.i], num, den);
			else if (ev.kind == EV_SIGH)
				DrawSigh(g_sigh[ev.i], num, den);
			else
				DrawPuff(g_puff[ev.i], num, den);
		}
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		memcpy((void *)SPRITE_CAM, sprite_cam, SPRITE_CAM_N);
		memcpy((void *)ENT_LO, ents, sizeof(ents));
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag046_held()
	{
		mark_held(act::sigh::ORIG_ActorSystem);
		mark_held(act::sigh::ORIG_Sigh);
		mark_held(act::sigh::ORIG_Breath);
		register_module_held(46, act::sigh::HeldReady, act::sigh::HeldFrame);
	}
}
