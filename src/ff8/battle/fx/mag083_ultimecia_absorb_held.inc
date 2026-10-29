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

// 30 fps part of mag083_ultimecia_absorb.cpp: the held-frame drawing (included at the end of
// mag083_ultimecia_absorb.cpp; the actor systems as effect 82's mag082_ultimecia_blow_away_held.inc with this
// module's addresses, plus the creature and the target absorption)
//
// The drawing tasks update THEN draw in the same tick. The real tick notes each draw (creature 0x79E690,
// actor-system draw 0x798A40, target absorption 0x79D9B0) with the GTE state, the module scratch stack and the
// Field_Alloc scratch it started from; the held frame replays them in the same order, at tick + num/den:
//   Creature        - its next tick's update from its pure parts (fade step +0x13A, velocity change at the end
//                     of its animation, hidden when its fade-out ends): position, fade (colours 0x79E9E0 /
//                     0x79EB00) half way, the midpoint pose (pose_midpoint), drawn by DrawModel (a_746C10);
//                     the node is put back. Hidden on the next tick: not drawn at fraction 1.
//   Actor systems   - as mag080_ultimecia_junction_held.inc / mag082_ultimecia_blow_away_held.inc (the same
//                     actor-system code): each drawn actor from a copy whose view positions / draw matrix are
//                     half way to the next tick's; a morphing model's vertex block half way.
//   Target          - the polygon states of the real tick (the body pass noted after its draw, the weapon pass
//   absorption        = the live states) and of the next tick (the state step of 0x79DB00 run on copies with
//                     the next tick's threshold): a polygon the next tick draws spinning is drawn spinning
//                     with its angle (the short way round, forwards), height and offsets half way; any other
//                     polygon at its place on the target's midpoint pose (pose_midpoint). Its semi-transparency
//                     is the real tick's. The next tick does not draw it (countdown end): held, not drawn at 1.
// The packets go to private buffers (module sprite cursor 0x25C9F74 and frame arena 0x1D8E054) that start with
// the real tick's packets; the module globals, the exe data the effect rewrites, the module scratch stack, the
// Field_Alloc scratch, the shadow / RenderGeometry temporaries, the effect light/position block, the target's
// skeletons and the GTE registers are put back. The master, the emitter, the director and the stage light
// tasks draw nothing.

namespace ff8fx
{
namespace act
{
namespace u083
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25C9F70, BSS_HI = 0x25CFA88;
	static const uint32_t FA_CURSOR = 0x1D8E054; // frame arena packet cursor
	// exe data the effect rewrites in place: the actor system's sequence 0x172C818 (relocated by its first use, actor
	// data, morph vertex blocks .. 0x1738BF8) and the creature's model container 0x15786D4 (pose)
	static const uint32_t XDATA[2][2] = { { 0x172C800, 0x1738C00 }, { 0x15786C0, 0x1578C40 } };
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const uint32_t EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 256;
	static const uint32_t POLY_STATES = 0x7D00; // the polygon state buffer [0x25CF3F0] (0x209FAB8)

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

	enum { K_SYSTEM, K_CREATURE, K_ABSORB };
	struct Note
	{
		int kind;
		uint32_t addr;        // actor system state / creature node / absorption node
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;            // actor system: its vertex blocks; absorption: its body-pass state snapshot
	};
	static const int MAX_NOTES = 16;
	static Note g_notes[MAX_NOTES];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0;
	static uint8_t g_held_packets[0x40000], g_held_fa[0x60000];
	// the polygon states after the body pass of each absorption draw of the real tick
	static const int MAX_ABSORB = 2;
	alignas(4) static uint8_t g_absorb_body[MAX_ABSORB][POLY_STATES];
	static int g_nabsorb = 0;

	// ---- the FX_HELD statements of the real tick
	// the master starts (before any draw of the tick; it picks the tick's packet arena after this note:
	// the held packet arena start is taken at the first draw)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nnotes = 0;
		g_nsys_verts = 0;
		g_nabsorb = 0;
		g_pkt_start = 0;
		g_fa_start = 0;
	}
	static Note *NoteDraw(int kind, uint32_t addr)
	{
		if (g_note_tick != g_real_tick || g_nnotes >= MAX_NOTES) return nullptr;
		if (!g_pkt_start)
		{
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
			g_fa_start = MEM<uint32_t>(FA_CURSOR);
		}
		Note &n = g_notes[g_nnotes++];
		n.kind = kind;
		n.addr = addr;
		n.verts = -1;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		return &n;
	}
	// an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		Note *n = NoteDraw(K_SYSTEM, sys);
		if (n && g_nsys_verts < MAX_SYSTEMS)
		{
			n->verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}
	// the creature is about to draw (after its state and its position step)
	static void held_note_creature(uint32_t node) { NoteDraw(K_CREATURE, node); }
	// the target absorption is about to draw (after its state)
	static void held_note_absorb(uint32_t node)
	{
		Note *n = NoteDraw(K_ABSORB, node);
		if (n && g_nabsorb < MAX_ABSORB) n->verts = g_nabsorb++;
	}
	// its body pass drew (the weapon pass may step the same polygon states again)
	static void held_note_absorb_body()
	{
		if (g_note_tick != g_real_tick || g_nnotes == 0) return;
		const Note &n = g_notes[g_nnotes - 1];
		if (n.kind != K_ABSORB || n.verts < 0) return;
		memcpy(g_absorb_body[n.verts], (const void *)MEM<uint32_t>(0x25CF3F0), POLY_STATES);
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
	// actor systems
	// ====================================================================================
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x79AE90 run on it): view positions, draw matrix, sub-positions; at fraction 1 the whole next
	// actor. P_ENDS: the actor ends on the next tick (its sprite holds its last draw, its model is not
	// drawn at fraction 1); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x79BFD0 (its follow-up spawn 0x79C050: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x79AE90)
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
		// the next tick's update pass (0x79AE50) in list order: the emitters' placement / motion
		// (0x79C120 without its emission and life count, put back after: their particles read them)
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
					continue; // (its init 0x79D310 may draw rand(): not stepped)
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					a_7424C0(act);
				else if (mode == 4)
					ud_79CC20(act);
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
					ud_79BEC0(P(nxts[i]), DefOf(list[i]));
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
		// sprite sequences (0x79AD10: one draw, or one per sub-position)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				ud_79ADD0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					ud_79ADD0(c);
				}
			}
		}
		// models (0x798AD0)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				ud_798B30(P(copies[i]), def);
		}
	}


	// ====================================================================================
	// creature (0x79E690)
	// ====================================================================================
	static void CreatureHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		if (U32(node, 8) != 0x79E690) return;
		// the next tick's state (0x79EB70 fade in / 0x79EBE0 / 0x79EC20 / 0x79EC40 fade out): fade, y velocity
		// (changed when its animation read finds the animation complete: frame +0xA2 >= count +0xA3), hidden
		const int16_t f = S16(node, 0x13A);
		int16_t f1 = f, vy1 = S16(node, 0x126);
		bool hide1 = false;
		const bool ends = U8(node, 0xA2) >= U8(node, 0xA3);
		switch (S8(node, 0x29))
		{
		case 1:
			f1 = (int16_t)(f - 0x100);
			if (f1 <= 0) f1 = 0;
			if (ends) vy1 = 0;
			break;
		case 2:
			if (ends) vy1 = -0x20;
			break;
		case 4:
			f1 = (int16_t)(f + 0x100);
			if (f1 >= 0x1000) { f1 = 0x1000; hide1 = true; }
			break;
		default:
			break;
		}
		if (hide1 && num >= den) return; // (hidden on the next tick)
		alignas(4) static uint8_t save[0x140];
		memcpy(save, (const void *)node, sizeof(save));
		const int16_t v1[3] = { S16(node, 0x124), vy1, S16(node, 0x128) };
		for (int k = 0; k < 3; k++)
			S16(node, 0x4C + 2 * k) = (int16_t)(S16(node, 0x4C + 2 * k) + v1[k] * num / den);
		S16(node, 0x13A) = (int16_t)lerp_i(f, f1, num, den);
		S16(node, 0x13C) = S16(node, 0x13A);
		ud_79E9E0(node, 3);
		ud_79EB00(node);
		pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
		MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, 0x25CF3F8, MEM<uint32_t>(PACKET_CURSOR));
		memcpy((void *)node, save, sizeof(save));
	}

	// ====================================================================================
	// target absorption (0x79D9B0: draw 0x79DA20 / 0x79DB00)
	// ====================================================================================
	// the polygon state steps of one pass of 0x79DB00 over model object obj (no draw): threshold thr
	static void AbsorbStep(uint32_t obj, uint32_t mask, uint32_t states, int16_t thr)
	{
		alignas(4) static uint8_t sbuf[0x94];
		const uint32_t s = P(sbuf);
		memset(sbuf, 0, sizeof(sbuf));
		S16(s, 0x90) = thr;
		const uint32_t table = U32(obj, 4);
		const int32_t count = S32(table, 0);
		uint32_t st = states;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t p = table + U32(table + 4 + 4 * (uint32_t)i, 0);
			if (((mask >> (i & 31)) & 1) == 0) continue;
			const int32_t groups = S16(p, 0);
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t nv = S16(p, 2);
				p += 4;
				if (nv > 0) p += 6 * (uint32_t)nv;
			}
			p = (p + 3) & ~3u;
			const int32_t tris = S16(p, 0), quads = S16(p, 2);
			for (int32_t t = 0; t < tris; t++) { st += 0x20; absorb_step(s, st, false); }
			for (int32_t t = 0; t < quads; t++) { st += 0x20; absorb_step(s, st, true); }
		}
	}
	// a polygon drawn spinning (0x79DB00 phase 2 from its first step on): its state's draw
	static bool Spins(uint32_t st) { return S16(st, 6) >= 2 && S16(st, 8) >= 1; }
	// a polygon's vertices (scratch s +0x54.., as loaded at the midpoint pose) at the held state between a
	// (the real tick's state) and b (the next tick's): spinning on both ticks -> its angle (forwards), height and
	// offsets half way (the spin's own arc); starting to spin on the next tick -> its vertices half way from its
	// place to their spin positions there; else at its place
	static void AbsorbPlace(uint32_t s, uint32_t a, uint32_t b, bool quad, int num, int den)
	{
		if (!Spins(b)) return;
		const bool spun = Spins(a);
		const int32_t r = S16(b, 2);
		const uint32_t da = spun ? ((uint32_t)(U16(b, 4) - U16(a, 4)) & 0xFFF) : 0;
		const uint32_t ang = spun ? (uint32_t)(U16(a, 4) + (int32_t)da * num / den) & 0xFFF : U16(b, 4);
		const int32_t y = spun ? lerp_i(S16(a, 0), S16(b, 0), num, den) : S16(b, 0);
		const int32_t px = (int32_t)x::computeSin(ang) * r / 4096;
		const int32_t pz = (int32_t)x::computeCosine(ang) * r / 4096;
		static const int32_t OFF[9] = { 0xA, 0x10, 0x16, 0xC, 0x12, 0x18, 0xE, 0x14, 0x1A };
		int32_t o[9];
		for (int k = 0; k < 9; k++) o[k] = spun ? lerp_i(S16(a, OFF[k]), S16(b, OFF[k]), num, den) : S16(b, OFF[k]);
		const int32_t base[3] = { px, y, pz };
		static const int32_t VO[4] = { 0x54, 0x5C, 0x64, 0x6C };
		for (int v = 0; v < (quad ? 4 : 3); v++)
			for (int c = 0; c < 3; c++)
			{
				const int32_t w = (int16_t)(base[c] + (v ? o[3 * (v - 1) + c] : 0)); // v1 / v2 / v3: offsets +0xA.. / +0xC.. / +0xE..
				S16(s, VO[v] + 2 * c) = (int16_t)(spun ? w : lerp_i(S16(s, VO[v] + 2 * c), w, num, den));
			}
	}
	// 0x79DB00 on a held frame: the same vertices / packets, the polygons placed by AbsorbPlace (states a / b:
	// the pass's states on the real tick / the next tick), no state step
	static void AbsorbObjectHeld(uint32_t a1, uint32_t a2, uint32_t m, uint32_t A, uint32_t B, int num, int den)
	{
		uint32_t cursor = MEM<uint32_t>(U32(m, 0x4C));
		const uint32_t verts = U32(U32(m, 0x44), 4);
		const uint32_t ent = U32(m, 0x48);
		const uint32_t table = U32(a1, 4);
		const uint32_t bones = U32(a1, 0) + 0x10;
		const int32_t count = S32(table, 0);
		alignas(4) static uint8_t sbuf[0x94];
		const uint32_t s = P(sbuf);
		const uint32_t col = U32(ent, 0x28) & 0xFFFFFF;
		const uint32_t mask = U32(ent, 0x7C);
		uint32_t sa = A, sb = B;
		x::GTE_SetRotMatrix(m + 0x20);
		x::GTE_SetTransVector(m + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t dst = verts;
			uint32_t p = table + U32(table + 4 + 4 * (uint32_t)i, 0);
			if (((mask >> (i & 31)) & 1) == 0) continue;
			const int32_t groups = S16(p, 0);
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t bone = S16(p, 0);
				p += 2;
				const uint32_t bm = (uint32_t)(bone * 0x30) + bones + 0x10;
				gx::GTE_SetLightMatrix(bm);
				gx::GTE_SetBackgroundVector(U32(bm, 0x14), U32(bm, 0x18), U32(bm, 0x1C));
				const int32_t nv = S16(p, 0);
				p += 2;
				for (int32_t k = nv; k > 0; k--)
				{
					U16(s, 0x28) = U16(p, 0);
					U16(s, 0x2A) = U16(p, 2);
					U16(s, 0x2C) = U16(p, 4);
					p += 6;
					x::GTE_LoadV0(s + 0x28);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(s + 0x28);
					U32(dst, 0) = U32(s, 0x28);
					U32(dst, 4) = U32(s, 0x2C);
					dst += 0x10;
				}
			}
			p = (p + 3) & ~3u;
			const int32_t tris = S16(p, 0);
			const int32_t quads = S16(p, 2);
			uint32_t poly = p + 0xC;
			for (int32_t t = tris; t > 0; t--, poly += 0x10)
			{
				const uint32_t i0 = U16(poly, 0) & 0xFFF, i1 = U16(poly, 2) & 0xFFF, i2 = U16(poly, 4) & 0xFFF;
				U32(s, 0x54) = U32(verts + i0 * 16, 0);
				U32(s, 0x58) = U32(verts + i0 * 16, 4);
				U32(s, 0x5C) = U32(verts + i1 * 16, 0);
				U32(s, 0x60) = U32(verts + i1 * 16, 4);
				U32(s, 0x64) = U32(verts + i2 * 16, 0);
				U32(s, 0x68) = U32(verts + i2 * 16, 4);
				sa += 0x20;
				sb += 0x20;
				AbsorbPlace(s, sa, sb, false, num, den);
				gx::GTE_LoadV012Dwords(s + 0x54);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(s + 0x74);
				if (S32(s, 0x74) <= 0) continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(s + 0x78);
				const uint32_t z = U32(s, 0x78) >> 2;
				gx::GTE_StoreSXY012_PolyFT3(cursor);
				U32(cursor, 0) = 0x7000000;
				U32(cursor, 0xC) = U32(poly, 8);
				U32(cursor, 0x14) = U32(poly, 0xC);
				U16(cursor, 0x1C) = U16(poly, 6);
				U32(cursor, 4) = col | 0x24000000;
				if ((U8(poly, 0xF) & 2) != 0) U8(cursor, 7) |= 2;
				if (S16(num >= den ? sb : sa, 6) >= 2) U8(cursor, 7) |= 2; // (the real tick's; the next tick's at fraction 1)
				x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, cursor);
				cursor += 0x20;
			}
			for (int32_t q = quads; q > 0; q--, poly += 0x14)
			{
				const uint32_t i0 = U16(poly, 0) & 0xFFF, i1 = U16(poly, 2) & 0xFFF, i2 = U16(poly, 4) & 0xFFF, i3 = U16(poly, 6) & 0xFFF;
				U32(s, 0x54) = U32(verts + i0 * 16, 0);
				U32(s, 0x58) = U32(verts + i0 * 16, 4);
				U32(s, 0x5C) = U32(verts + i1 * 16, 0);
				U32(s, 0x60) = U32(verts + i1 * 16, 4);
				U32(s, 0x64) = U32(verts + i2 * 16, 0);
				U32(s, 0x68) = U32(verts + i2 * 16, 4);
				U32(s, 0x6C) = U32(verts + i3 * 16, 0);
				U32(s, 0x70) = U32(verts + i3 * 16, 4);
				sa += 0x20;
				sb += 0x20;
				AbsorbPlace(s, sa, sb, true, num, den);
				gx::GTE_LoadV012Dwords(s + 0x54);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(s + 0x74);
				if (S32(s, 0x74) <= 0) continue;
				gx::GTE_StoreSXY012_PolyFT3_2(cursor);
				x::GTE_LoadV0(s + 0x6C);
				x::GTE_RTPS();
				x::GTE_ReadSXY2(cursor + 0x20);
				x::GTE_AVSZ4();
				x::GTE_ReadOTZ(s + 0x78);
				const uint32_t z = U32(s, 0x78) >> 2;
				U32(cursor, 0) = 0x9000000;
				U32(cursor, 0xC) = U32(poly, 8);
				U32(cursor, 0x14) = U32(poly, 0xC);
				U16(cursor, 0x1C) = U16(poly, 0x10);
				U16(cursor, 0x24) = U16(poly, 0x12);
				U32(cursor, 4) = col | 0x2C000000;
				if ((U8(poly, 0xF) & 2) != 0) U8(cursor, 7) |= 2;
				if (S16(num >= den ? sb : sa, 6) >= 2) U8(cursor, 7) |= 2; // (the real tick's; the next tick's at fraction 1)
				x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, cursor);
				cursor += 0x28;
			}
		}
		MEM<uint32_t>(U32(m, 0x4C)) = cursor;
	}
	// the skeleton of a model object (bone records rebuilt from the pose by 0x508C90; pose_midpoint changes them)
	static uint32_t SkelBytes(uint32_t sk) { return sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0; }
	static void AbsorbHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		if (U32(node, 8) != 0x79D9B0 || n.verts < 0) return;
		const uint32_t m = node + 0x6C;
		const uint32_t ent = U32(m, 0x48);
		const uint32_t live = MEM<uint32_t>(0x25CF3F0);
		// the next tick's state 0x79E610: threshold -0x10 (to 0), countdown -1 (its end: no draw)
		int16_t thr = S16(m, 0x54);
		bool draws1 = true;
		if (S8(node, 0x29) == 1)
		{
			thr = (int16_t)(thr - 0x10);
			if (thr < 0) thr = 0;
			draws1 = (int16_t)(S16(node, 0xC4) - 1) > 0;
		}
		if (!draws1 && num >= den) return;
		const uint32_t body = U32(ent, 0x64), weapon = U32(ent, 0x78);
		const uint32_t mask = U32(ent, 0x7C);
		alignas(4) static uint8_t nb[POLY_STATES], nw[POLY_STATES];
		const uint32_t A_b = P(g_absorb_body[n.verts]), A_w = live;
		memcpy(nw, (const void *)live, POLY_STATES);
		if (draws1)
		{
			AbsorbStep(body, mask, P(nw), thr);
			memcpy(nb, nw, POLY_STATES);
			if (weapon) AbsorbStep(U32(weapon, 4), mask, P(nw), thr);
		}
		else
		{
			memcpy(nb, (const void *)A_b, POLY_STATES);
		}
		// the skeletons (body, weapon) as the real tick left them
		static uint8_t skel[2][0x10 + 0x30 * 256];
		const uint32_t sk[2] = { body ? U32(body, 0) : 0, weapon && U32(weapon, 4) ? U32(U32(weapon, 4), 0) : 0 };
		for (int k = 0; k < 2; k++)
			if (sk[k]) memcpy(skel[k], (const void *)sk[k], SkelBytes(sk[k]));
		// 0x79DA20 at the midpoint pose
		memcpy((void *)m, (const void *)(ent + 0x40), 8 * 4);
		pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), num, den);
		x::GTE_SetRotMatrix(0x1D97778);
		for (int c = 0; c < 3; c++)
		{
			x::GTE_LoadIRFromMatrixColumn(m + 2 * c);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(m + 0x20 + 2 * c);
		}
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(m + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(m + 0x34);
		AbsorbObjectHeld(body, MEM<uint32_t>(0x1D8E04C) + 0x44, m, A_b, P(nb), num, den);
		if (weapon)
			AbsorbObjectHeld(U32(weapon, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, m, A_w, P(nw), num, den);
		for (int k = 0; k < 2; k++)
			if (sk[k]) memcpy((void *)sk[k], skel[k], SkelBytes(sk[k]));
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_083;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80], light[0x20];
		static uint8_t xdata[2][0xC400];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		for (int i = 0; i < 2; i++)
			memcpy(xdata[i], (const void *)XDATA[i][0], XDATA[i][1] - XDATA[i][0]);
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(light, (const void *)0x1D99AB0, sizeof(light));
		static VertexBlocks verts_now[MAX_SYSTEMS];
		for (int i = 0; i < g_nnotes; i++)
			if (g_notes[i].kind == K_SYSTEM && g_notes[i].verts >= 0) verts_now[g_notes[i].verts].save(g_notes[i].addr);
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t pc = MEM<uint32_t>(PACKET_CURSOR), fc = MEM<uint32_t>(FA_CURSOR);
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
			const Note &n = g_notes[i];
			Restore(n);
			if (n.kind == K_SYSTEM) SystemHeld(n, num, den);
			else if (n.kind == K_CREATURE) CreatureHeld(n, num, den);
			else AbsorbHeld(n, num, den);
		}
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		for (int i = g_nnotes - 1; i >= 0; i--)
			if (g_notes[i].kind == K_SYSTEM && g_notes[i].verts >= 0) verts_now[g_notes[i].verts].restore();
		memcpy((void *)0x1D99AB0, light, sizeof(light));
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
	static void register_mag083_held()
	{
		// the drawing tasks: the engine task body running this module's actor system (0x797ED0), the
		// creature 0x79E690 and the target absorption 0x79D9B0
		static const uint32_t HELD[] = { 0x797ED0, 0x79E690, 0x79D9B0, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(83, act::u083::HeldReady, act::u083::HeldFrame);
	}
}
