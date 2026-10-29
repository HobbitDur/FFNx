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

// 30 fps part of mag082_ultimecia_blow_away.cpp: the held-frame drawing (included at the end of
// mag082_ultimecia_blow_away.cpp; the same held part as effect 81's mag081_ultimecia_unk4_held.inc with
// this module's addresses)
//
// The drawing tasks update THEN draw in the same tick. The real tick notes each actor-system draw
// (0x7A0100) with the GTE state, the module scratch stack and the Field_Alloc scratch it started from,
// and the master notes the screen tints (engine task 0x8DDC30 of the last queue) before the queues
// run; the held frame replays them in the same order, at tick + num/den:
//   Actor systems - as mag080_ultimecia_junction_held.inc (the same actor system code): each drawn
//                   actor from a copy whose view positions / draw matrix are half way to the next
//                   tick's (the next motion step on a second copy; the whole next actor at fraction 1);
//                   an emitter not initialised yet (+0x62 == 0: its init may draw rand()) is not
//                   stepped; a morphing model's vertex block half way to the next step's (a kind-5
//                   actor born on the next tick rewrites the shared block there: not shown, it does not
//                   exist yet); a model with several copies draws each sub-position half way.
//   Screen tint   - as mag023_psycho_blast_held.inc: its colour half way to the next tick's script
//                   colour, drawn into a private copy of the fixed packets 0x2795110; a tint the real
//                   tick did not draw is not drawn, one the next tick does not draw holds (and is not
//                   drawn at fraction 1).
// The packets go to private buffers (module sprite cursor 0x25CFA8C and frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the exe data the effect rewrites (actor data,
// sequence data), the module scratch stack, the Field_Alloc scratch, the shadow / RenderGeometry
// temporaries and the GTE registers are put back. The master, the emitter, the director and the stage
// darkening tasks draw nothing.

namespace ff8fx
{
namespace act
{
namespace u082
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25CFA88, BSS_HI = 0x25D21E0;
	static const uint32_t FA_CURSOR = 0x1D8E054; // frame arena packet cursor
	// exe data the effect rewrites in place (actor data, sequence data)
	static const uint32_t XDATA[2][2] = { { 0x1741000, 0x1742000 }, { 0x1744100, 0x1744B00 } }; // sequence 0x1741028, actor data 0x174417C
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const uint32_t EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 256;
	static const uint32_t TINT_PKT = 0x2795110, TINT_PKT_N = 0x6C; // the tint task's fixed packets
	static const int MAX_TINTS = 10;

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
				const uint32_t bytes_n = U32(blk, 4) * 8;
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
	static const int MAX_SYSTEMS = 4;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS];
	static int g_nsys_verts = 0;

	struct Note
	{
		uint32_t addr;        // actor system state
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;
	};
	static const int MAX_NOTES = 16;
	static Note g_notes[MAX_NOTES];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0;
	static uint8_t g_held_packets[0x40000], g_held_fa[0x60000];

	// the screen tints (engine task 0x8DDC30 in the last queue): their nodes before the queue runs
	static uint8_t g_tint[MAX_TINTS][0x40];
	static uint32_t g_tint_addr[MAX_TINTS];
	static int g_ntint = 0;
	static uint8_t g_tint_real[TINT_PKT_N];
	alignas(4) static uint8_t g_tint_pkt[TINT_PKT_N];

	// ---- the FX_HELD statements of the real tick
	// the master starts (before any draw of the tick; it picks the tick's packet arena after this note:
	// the held packet arena start is taken at the first draw)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nnotes = 0;
		g_nsys_verts = 0;
		g_pkt_start = 0;
		g_fa_start = 0;
		g_ntint = 0;
		for (uint32_t node = MEM<uint32_t>(Q_TINT); node && g_ntint < MAX_TINTS; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30) continue;
			memcpy(g_tint[g_ntint], (const void *)node, 0x40);
			g_tint_addr[g_ntint++] = node;
		}
	}
	// an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		if (g_note_tick != g_real_tick || g_nnotes >= MAX_NOTES) return;
		if (!g_pkt_start)
		{
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
			g_fa_start = MEM<uint32_t>(FA_CURSOR);
		}
		Note &n = g_notes[g_nnotes++];
		n.addr = sys;
		n.verts = -1;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		if (g_nsys_verts < MAX_SYSTEMS)
		{
			n.verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// the draw starts from the real tick's GTE state and scratch
	static void Restore(const Note &n)
	{
		memcpy((void *)GTE_D, n.gte, GTE_D_N);
		memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), n.falloc, sizeof(n.falloc));
	}

	// ====================================================================================
	// actor systems (as mag080_ultimecia_junction_held.inc: its actor-system code is this module's)
	// ====================================================================================
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x7A2530 run on it): view positions, draw matrix, sub-positions; at fraction 1 the whole next
	// actor. P_ENDS: the actor ends on the next tick (its sprite holds its last draw, its model is not
	// drawn at fraction 1); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7A3670 (its follow-up spawn 0x7A36F0: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		a_741120(nxt, def); // (a morphing model's step rewrites its vertex block: put back by the caller)
		if (U32(nxt, 0x170) != U32(act, 0x170) && num < den) return P_HOLD;
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
		// fx091 (backport from 91): an actor whose motion step clears its sequence / model (+0x170: its last frame) is not drawn on the next tick
		if (num >= den && U32(nxt, 0x170) == 0) return P_ENDS;
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x7A2530)
		}
		return P_NEXT;
	}

	static void SystemHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(g_sys_verts[0].bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.addr;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.addr, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x7A24F0) in list order: the emitters' placement / motion
		// (0x7A37C0 without its emission and life count, put back after: their particles read them)
		// and the drawn particles' steps (Predict)
		static const int MAX_EMIT = 64;
		static uint8_t esave[MAX_EMIT][EMIT_BYTES];
		uint32_t eaddr[MAX_EMIT];
		int ne = 0, pi = 0;
		int pr[MAX_ACTORS];
		bool morph = false;
		for (uint32_t act = U32(n.addr, 0x2C); act; act = U32(act, 4))
		{
			if (S16(act, 8) == 0)
			{
				if (ne >= MAX_EMIT) continue;
				eaddr[ne] = act;
				memcpy(esave[ne++], (const void *)act, EMIT_BYTES);
				if (U16(act, 0x62) == 0)
					continue; // (its init 0x7A49B0 may draw rand(): not stepped)
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					a_7424C0(act);
				else if (mode == 4)
					ud_7A42C0(act);
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
		const VertexBlocks *vb = n.verts >= 0 ? &g_sys_verts[n.verts] : nullptr;
		if (morph && vb)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					ud_7A3560(P(nxts[i]), DefOf(list[i]));
			uint32_t used = 0;
			for (uint32_t b = 0; b < vb->n; b++)
			{
				memcpy(verts_next + used, (const void *)vb->addr[b], vb->len[b]);
				used += vb->len[b];
			}
			vb->restore();
			used = 0;
			for (uint32_t b = 0; b < vb->n; b++)
			{
				for (uint32_t o = 0; o < vb->len[b]; o += 2)
					S16(vb->addr[b], o) = (int16_t)lerp_i(S16(vb->addr[b], o), S16(P(verts_next) + used, o), num, den);
				used += vb->len[b];
			}
		}
		else if (vb)
			vb->restore();
		Restore(n);
		// sprite sequences (0x7A23B0: one draw, or one per sub-position)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				ud_7A2470(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					ud_7A2470(c);
				}
			}
		}
		// models (0x7A0190)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				ud_7A01F0(P(copies[i]), def);
		}
	}

	// ====================================================================================
	// screen tint (0x8DDC30 state 1 = 0x8DDCD0: colour script step, four semi-transparent flat quads
	// over the screen + a draw mode into OT base + 0x1C, fixed packets 0x2795110)
	// ====================================================================================
	// the colour step of 0x8DDCD0 on node a (no draw); false = the script ended (nothing drawn)
	static bool TintStep(uint32_t a)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF)
		{
			U8(a, 0x26) |= 1;
			return false;
		}
		if (op == 0xFE)
		{
			if (S16(a, 0x24) >= (int16_t)(uint16_t)b2 && MEM<uint32_t>(0x2795914) == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(a, 0x3A) = ni;
				U32(a, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a, 0x30) = ent;
			if (MEM<uint32_t>(0x2795914) == 0)
				U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	// the packets of 0x8DDD33.. into the private copy of the fixed buffer (which holds the real
	// tick's packets: the words a draw does not write are the real tick's)
	static void TintDraw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		memcpy(g_tint_pkt, g_tint_real, TINT_PKT_N);
		uint32_t pkt = P(g_tint_pkt);
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++)
				U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, pkt);
	}
	static void TintHeld(int num, int den)
	{
		alignas(4) static uint8_t a[0x40], b[0x40];
		const uint32_t A = P(a), B = P(b);
		for (int i = 0; i < g_ntint; i++)
		{
			// the real tick: state 1 drew (unless its script ended)
			memcpy(a, g_tint[i], 0x40);
			const bool drew = S8(A, 0x29) == 1 && TintStep(A);
			// the next tick: the node as the real tick left it (released when it finished)
			bool drew1 = false;
			if (!(U8(A, 0x26) & 1) && (S8(A, 0x29) == 1 || S8(A, 0x29) == 0))
			{
				memcpy(b, (const void *)g_tint_addr[i], 0x40);
				drew1 = S8(B, 0x29) == 1 && TintStep(B);
			}
			// (a tint the real tick did not draw is not drawn; one the next tick does not draw holds)
			if (!drew) continue;
			if (!drew1 && num >= den)
				continue; // (the next tick draws nothing: at fraction 1 nothing either)
			uint32_t col = U32(A, 0x30);
			if (drew1)
			{
				const uint32_t next = U32(B, 0x30);
				uint32_t cc = col & 0xFF000000;
				for (int k = 0; k < 3; k++)
				{
					const int32_t x0 = (int32_t)((col >> (8 * k)) & 0xFF), x1 = (int32_t)((next >> (8 * k)) & 0xFF);
					cc |= (uint32_t)(lerp_i(x0, x1, num, den) & 0xFF) << (8 * k);
				}
				col = cc;
			}
			TintDraw(col);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_082;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		static uint8_t xdata[2][0x1800];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		for (int i = 0; i < 2; i++)
			memcpy(xdata[i], (const void *)XDATA[i][0], XDATA[i][1] - XDATA[i][0]);
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		static VertexBlocks verts_now[MAX_SYSTEMS];
		for (int i = 0; i < g_nnotes; i++)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].save(g_notes[i].addr);
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t pc = MEM<uint32_t>(PACKET_CURSOR), fc = MEM<uint32_t>(FA_CURSOR);
		memcpy(g_tint_real, (const void *)TINT_PKT, TINT_PKT_N);
		{
			const uint32_t used = pc - g_pkt_start;
			if (g_pkt_start && pc > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
			const uint32_t used2 = fc - g_fa_start;
			if (g_fa_start && fc > g_fa_start && used2 < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used2);
		}
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int i = 0; i < g_nnotes; i++)
		{
			Restore(g_notes[i]);
			SystemHeld(g_notes[i], num, den);
		}
		TintHeld(num, den);
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		for (int i = g_nnotes - 1; i >= 0; i--)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].restore();
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		for (int i = 0; i < 2; i++)
			memcpy((void *)XDATA[i][0], xdata[i], XDATA[i][1] - XDATA[i][0]);
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag082_held()
	{
		// the drawing tasks: the engine task body running this module's actor system (0x79F590) and
		// the screen tint 0x8DDC30
		static const uint32_t HELD[] = { 0x79F590, 0x8DDC30, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(82, act::u082::HeldReady, act::u082::HeldFrame);
	}
}
