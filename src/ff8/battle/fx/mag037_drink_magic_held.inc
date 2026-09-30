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

// 30 fps part of mag037_drink_magic.cpp: the held-frame drawing of the actor systems, the streaks, the
// ribbons, their sparks and the orb (included at the end of mag037_drink_magic.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// Field_Alloc scratch):
//   - actor systems (task 0x856E90; the design of mag048_magma_breath_held.inc): each noted system's
//     particle actors are redrawn from copies whose view positions and draw matrix (rotation /
//     scale) are half way to the next tick's (the next tick's update pass runs on second copies, in
//     list order, with the emitters' placement / motion applied to the emitters and put back); a
//     morphing model's vertex block is half way to the next tick's morph (0x85A230); flipbook frame,
//     fade and colours hold (the next tick's at fraction 1); an actor ending on the next tick holds
//     (at fraction 1 too); an actor not in its motion phase holds.
//   - streaks (task 0x85BD50): the head sprite half way to the next tick's head (its acceleration
//     0x85C0C0 and head step 0x85BEC0 run on a copy); the flipbook frame holds (the next tick's at
//     fraction 1); a streak that ends on the next tick holds.
//   - ribbons (task 0x85E930, Draw's ribbon): the node as noted before its draws (head step done)
//     with its head half way to the next tick's (0x85EA30 run on a copy of the node as the tick left
//     it), the history entries half way (only the capped head entry moves) and the twist +0x126
//     advanced by the fraction of its 0x180 step; the drawn range, the fade-in and the flipbook
//     frame hold (a segment born or dropped on the next tick is 15 Hz growth; at fraction 1 the next
//     tick's entries and twist with the real tick's range); a ribbon / head the next tick does not
//     draw holds.
//   - sparks (task 0x85F060): at their ribbon's head half way to the next tick's (the ribbon's
//     predicted head when it runs before the spark in the queue, else its current one); rotation
//     and flipbook frame hold (the next tick's at fraction 1); a spark that ends then holds.
//   - the orb (task 0x85C1F0): its next tick (state step on a copy of the node, the model rewrite
//     0x85E4E0 run into the model and put back): position, texture scroll +0x5E and the model's
//     vertices half way; the fade +0x50 holds (it is the renderer's far colour word); at fraction 1
//     the next tick's orb; an orb the next tick does not draw holds.
// The packets go to private buffers (module cursor 0x26A05CC, frame arena 0x1D8E054); the module
// globals, the module scratch stack, the Field_Alloc scratch, the orb model's vertices and the GTE
// registers are put back.

namespace ff8fx
{
namespace act
{
namespace drinkmagic
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x26A05C8, BSS_HI = 0x26A8DB4;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_BYTES = 0x130; // ribbon queue nodes (spawner, streaks, ribbons, sparks)

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x85A230) rewrites them in the actor data, which every actor system of
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
	// a ribbon / spark before its draws: the node and the draw state
	struct NodeNote
	{
		uint32_t node;
		DrawState st;
		alignas(4) uint8_t bytes[NODE_BYTES];
	};
	// the orb before its draw (model rewritten): the node, the draw state and the model's vertices
	static const uint32_t ORB_BYTES = 0x60, ORB_VERTS_MAX = 0x4000;
	struct OrbNote
	{
		uint32_t node;
		DrawState st;
		alignas(4) uint8_t bytes[ORB_BYTES];
		uint32_t nverts;
		alignas(4) uint8_t verts[ORB_VERTS_MAX];
	};
	enum { EV_SYS, EV_RIBBON, EV_SPARK, EV_STREAK, EV_ORB };
	struct Event { int kind, i; };

	static const int MAX_SYS = 8, MAX_NODES = 128, MAX_ORB = 4, MAX_EV = MAX_SYS + MAX_NODES + MAX_ORB;
	static OrbNote g_orb[MAX_ORB];
	static int g_norb = 0;
	static SysNote g_sys[MAX_SYS];
	static NodeNote g_nodes[MAX_NODES];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nnodes = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000], g_held_fa[0x40000];
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // module packet cursor / frame arena at the tick's first draw
	static const uint32_t FA_CURSOR = 0x1D8E054; // frame arena cursor (the orb's draws)

	static void held_begin_tick()
	{
		if (g_note_tick == g_real_tick) return;
		g_note_tick = g_real_tick;
		g_nsys = g_nnodes = g_nev = g_norb = 0;
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		g_fa_start = MEM<uint32_t>(FA_CURSOR);
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

	static void note_node(uint32_t node, int kind)
	{
		held_begin_tick();
		if (g_nnodes >= MAX_NODES || g_nev >= MAX_EV) return;
		NodeNote &n = g_nodes[g_nnodes];
		n.node = node;
		n.st.save();
		memcpy(n.bytes, (const void *)node, NODE_BYTES);
		g_ev[g_nev++] = { kind, g_nnodes++ };
	}
	// real tick: a ribbon is about to draw its head sprite and its ribbon (head step done)
	static void held_note_ribbon(uint32_t node) { note_node(node, EV_RIBBON); }
	// real tick: a spark is about to draw (state step done)
	static void held_note_spark(uint32_t node) { note_node(node, EV_SPARK); }
	// real tick: a streak is about to draw its head sprite (head step done)
	static void held_note_streak(uint32_t node) { note_node(node, EV_STREAK); }
	// real tick: the orb is about to draw (its model rewritten by 0x85E4E0)
	static void held_note_orb(uint32_t node)
	{
		held_begin_tick();
		if (g_norb >= MAX_ORB || g_nev >= MAX_EV) return;
		OrbNote &n = g_orb[g_norb];
		n.node = node;
		n.st.save();
		memcpy(n.bytes, (const void *)node, ORB_BYTES);
		const uint32_t model = U32(node, 0x4C);
		n.nverts = (uint32_t)S32(model, 4) * 8;
		if (n.nverts > ORB_VERTS_MAX) n.nverts = 0;
		memcpy(n.verts, (const void *)(model + 8), n.nverts);
		g_ev[g_nev++] = { EV_ORB, g_norb++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// ---- actor systems (as mag048_magma_breath_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x859200 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor (flipbook frame, variant, fade, colour). P_ENDS: the actor ends on
	// the next tick (not drawn there); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7419C0 (= 0x85A340) (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		a_741120(nxt, def); // (a morphing model's step rewrites its vertex block: put back by the caller)
		// a model-frame step (+0x170 changes on the next tick): no mesh lies in between, the actor
		// holds (the next tick's at fraction 1)
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
		// (fade +0x1CE and colour +0x16C hold: the model renderer's depth-cued colours are not monotonic
		// in the fade across draws, a lerped fade can leave [N, N+1])
		// at fraction 1 the next tick's actor (same draws: same copy count, still drawn)
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x859200)
		}
		return P_NEXT;
	}

	// the actors of a noted system redrawn half way
	static void DrawSystem(int si, int num, int den)
	{
		const SysNote &n = g_sys[si];
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(n.verts.bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
		// 0x859080 / 0x857A00 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x8591C0) in list order: the emitters' placement / motion
		// (0x85A490 without its emission and life count, applied to the emitters themselves and put
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
					a_7424C0(act);
				else if (mode == 4)
					dk_85AF90(act);
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
		// (the next tick's morph steps 0x85A230 in list order, as its update pass writes them)
		if (morph)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					dk_85A230(P(nxts[i]), DefOf(list[i]));
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
				dk_859140(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					dk_859140(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5)
				a_7354A0(P(copies[i]), def);
		}
	}

	// ---- ribbons and sparks (ribbon queue 0x26A06C8)

	// the ribbons' heads as the next tick's step 0x85EA30 leaves them, for the sparks
	struct RibbonNext { uint32_t node; int16_t pos[3]; };
	static RibbonNext g_rnext[MAX_NODES];
	static int g_nrnext = 0;

	// position of `node` in the ribbon queue's execution order (-1: not queued)
	static int QueueIndex(uint32_t node)
	{
		int i = 0;
		for (uint32_t t = MEM<uint32_t>(Q_RIBBON); t && i < 0x1000; t = U32(t, 4), i++)
			if (t == node) return i;
		return -1;
	}

	// the ribbon's next tick up to its draws, on a copy `nxt` of the node as the tick left it:
	// state 1 (0x85F020) past step 10 hides the head (its spark spawn is not done), head step
	static void RibbonNextState(uint32_t node, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)node, NODE_BYTES);
		if (S8(nxt, 0x29) == 1 && S16(nxt, 0x124) >= 0xA)
		{
			U8(nxt, 0x26) |= 5;
			U16(nxt, 0x124) = 0xA;
			U8(nxt, 0x29) = (uint8_t)(U8(nxt, 0x29) + 1);
		}
		dk_85EA30(nxt);
	}

	static void DrawRibbon(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t spr[NODE_BYTES], rib[NODE_BYTES], nxt[NODE_BYTES];
		memcpy(spr, n.bytes, NODE_BYTES);
		memcpy(rib, n.bytes, NODE_BYTES);
		const uint32_t s = P(spr), r = P(rib);
		// the next tick: its head sprite (not hidden) and its ribbon (range not empty)
		const bool next = live(n.node, ORIG_Ribbon);
		if (next) RibbonNextState(n.node, P(nxt));
		const bool head_next = next && (U8(P(nxt), 0x26) & 4) == 0;
		const bool rib_next = next && S16(P(nxt), 0x12A) != 0 && S16(P(nxt), 0x128) < 0xA;
		// head sprite: half way to the next tick's head, the next tick's at fraction 1; a head the
		// next tick does not draw holds
		if (head_next)
		{
			if (num >= den)
				memcpy(spr, nxt, NODE_BYTES);
			else
				for (int k = 0; k < 3; k++)
					S16(s, 0x1C + 2 * k) = (int16_t)lerp_i(S16(s, 0x1C + 2 * k), S16(P(nxt), 0x1C + 2 * k), num, den);
		}
		// ribbon: the drawn range, fade-in and structure of the real tick (a segment born or dropped on
		// the next tick is 15 Hz growth), its history entries half way to the next tick's and the twist
		// advanced by the fraction of the next 0x180 step (at fraction 1 the next tick's entries and
		// twist); a ribbon the next tick does not draw holds
		int32_t twist = 0x180; // the tick's step before the ribbon draw
		if (rib_next)
		{
			for (int i = 0; i < 11; i++)
				for (int k = 0; k < 3; k++)
				{
					const int32_t o = 0x90 + 8 * i + 2 * k;
					S16(r, o) = (int16_t)lerp_i(S16(r, o), S16(P(nxt), o), num, den);
				}
			twist += 0x180 * num / den;
		}
		n.st.restore();
		dk_85BE30(s);
		U16(r, 0x126) = (uint16_t)((U16(r, 0x126) + twist) & 0xFFF);
		dk_85EB60(r);
	}

	static void DrawSpark(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[NODE_BYTES];
		memcpy(cur, n.bytes, NODE_BYTES);
		const uint32_t c = P(cur);
		// the next tick: head position of its ribbon (as the ribbon's step leaves it when the ribbon runs
		// before the spark, else as it is), flipbook step of state 1 (0x85F1F0: hidden past the last
		// frame); a spark the next tick does not draw holds
		if (live(n.node, ORIG_Spark) && (U8(n.node, 0x26) & 4) == 0)
		{
			int16_t frame = S16(n.node, 0x50);
			bool draws = true;
			if (S8(n.node, 0x29) == 1)
			{
				frame = (int16_t)(frame + 1);
				if (frame > S16(n.node, 0x52)) draws = false;
			}
			if (draws)
			{
				const uint32_t owner = U32(n.node, 0x18);
				int16_t pos[3] = { S16(owner, 0x1C), S16(owner, 0x1E), S16(owner, 0x20) };
				const int self = QueueIndex(n.node);
				for (int i = 0; i < g_nrnext; i++)
					if (g_rnext[i].node == owner && QueueIndex(owner) < self)
						for (int k = 0; k < 3; k++) pos[k] = g_rnext[i].pos[k];
				if (num >= den)
				{
					U16(c, 0x50) = (uint16_t)frame;
					for (int k = 0; k < 3; k++) S16(c, 0x1C + 2 * k) = pos[k];
				}
				else
					for (int k = 0; k < 3; k++) S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), pos[k], num, den);
			}
		}
		n.st.restore();
		dk_85F0D0(c);
	}

	// ---- streaks (task 0x85BD50: head sprite only)

	static void DrawStreak(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[NODE_BYTES], nxt[NODE_BYTES];
		memcpy(cur, n.bytes, NODE_BYTES);
		const uint32_t c = P(cur);
		// the next tick up to its draw: acceleration (state 1, 0x85C0C0: hidden at its end), head step
		// 0x85BEC0; a streak the next tick does not draw holds
		if (live(n.node, ORIG_Streak) && S8(n.node, 0x29) == 1)
		{
			memcpy(nxt, (const void *)n.node, NODE_BYTES);
			dk_85C0C0(P(nxt));
			dk_85BEC0(P(nxt));
			if ((U8(P(nxt), 0x26) & 4) == 0)
			{
				if (num >= den)
					memcpy(cur, nxt, NODE_BYTES);
				else
					for (int k = 0; k < 3; k++)
						S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(P(nxt), 0x1C + 2 * k), num, den);
			}
		}
		n.st.restore();
		dk_85BE30(c);
	}

	// ---- the orb (task 0x85C1F0)

	// the orb half way to the next tick's: position and texture offset +0x5E (+8 per tick; the fade +0x50
	// holds: it is the renderer's far colour word (+0xC), a lerp would carry into its low byte)
	// half way, its model's vertices half way to the next tick's rewrite (0x85E4E0 run into the model
	// and put back); at fraction 1 the next tick's orb; an orb the next tick does not draw holds
	static void DrawOrb(const OrbNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[ORB_BYTES], nxt[ORB_BYTES];
		alignas(4) static uint8_t vnext[ORB_VERTS_MAX];
		memcpy(cur, n.bytes, ORB_BYTES);
		const uint32_t c = P(cur);
		const uint32_t model = U32(c, 0x4C);
		bool next = false;
		if (live(n.node, ORIG_Orb) && n.nverts != 0)
		{
			// the next tick's state step (the states only write the node) and model rewrite
			memcpy(nxt, (const void *)n.node, ORB_BYTES);
			static const uint32_t states[6] = { 0x85E620, 0x85E640, 0x85E820, 0x85E860, 0x85E890, 0x85E8D0 };
			const int32_t st = S8(P(nxt), 0x29);
			if (st >= 0 && st < 6) callp(states[st], P(nxt));
			if ((U8(P(nxt), 0x26) & 4) == 0 && U32(P(nxt), 0x4C) == model)
			{
				const uint32_t b = (uint32_t)(uint16_t)(U16(P(nxt), 0x58) - 0xA0) & 0xFFF;
				const uint32_t a = (uint32_t)(uint16_t)(U16(P(nxt), 0x54) + 0xA0) & 0xFFF;
				U16(P(nxt), 0x58) = (uint16_t)b;
				U16(P(nxt), 0x54) = (uint16_t)a;
				U16(P(nxt), 0x5E) = (uint16_t)((uint8_t)(U8(P(nxt), 0x5E) + 8) & 0x7F);
				dk_85E4E0(ORB_TEMPLATE, model, a, b);
				memcpy(vnext, (const void *)(model + 8), n.nverts);
				next = true;
			}
		}
		if (next)
		{
			if (num >= den)
				memcpy(cur, nxt, ORB_BYTES);
			else
			{
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(P(nxt), 0x1C + 2 * k), num, den);
				U16(c, 0x5E) = (uint16_t)((U16(c, 0x5E) + 8 * num / den) & 0x7F);
			}
			for (uint32_t o = 0; o < n.nverts; o += 2)
				S16(model + 8, o) = (int16_t)lerp_i(S16(P(n.verts), o), S16(P(vnext), o), num, den);
		}
		else
			memcpy((void *)(model + 8), n.verts, n.nverts);
		n.st.restore();
		dk_85C350(c);
		memcpy((void *)(model + 8), n.verts, n.nverts); // the model as the real tick left it
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_037;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
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
		for (int s = 0; s < g_nsys; s++) add_win(g_sys[s].st.fa);
		for (int s = 0; s < g_nnodes; s++) add_win(g_nodes[s].st.fa);
		for (int s = 0; s < g_norb; s++) add_win(g_orb[s].st.fa);
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the ribbons' next heads (for the sparks), before any draw
		g_nrnext = 0;
		{
			alignas(4) static uint8_t nxt[NODE_BYTES];
			for (int s = 0; s < g_nnodes; s++)
			{
				const NodeNote &n = g_nodes[s];
				if (U32(P(n.bytes), 8) != ORIG_Ribbon || !live(n.node, ORIG_Ribbon)) continue;
				RibbonNextState(n.node, P(nxt));
				RibbonNext &r = g_rnext[g_nrnext++];
				r.node = n.node;
				r.pos[0] = S16(P(nxt), 0x1C); r.pos[1] = S16(P(nxt), 0x1E); r.pos[2] = S16(P(nxt), 0x20);
			}
		}
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		const uint32_t fc = MEM<uint32_t>(FA_CURSOR);
		const uint32_t used2 = fc - g_fa_start;
		if (g_fa_start && fc > g_fa_start && used2 < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used2);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(ev.i, num, den);
			else if (ev.kind == EV_RIBBON)
				DrawRibbon(g_nodes[ev.i], num, den);
			else if (ev.kind == EV_SPARK)
				DrawSpark(g_nodes[ev.i], num, den);
			else if (ev.kind == EV_STREAK)
				DrawStreak(g_nodes[ev.i], num, den);
			else
				DrawOrb(g_orb[ev.i], num, den);
		}
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		MEM<uint32_t>(FA_CURSOR) = fc;
		now.restore();
		memcpy((void *)SPRITE_CAM, sprite_cam, SPRITE_CAM_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag037_held()
	{
		mark_held(act::drinkmagic::ORIG_ActorSystem);
		mark_held(act::drinkmagic::ORIG_Ribbon);
		mark_held(act::drinkmagic::ORIG_Spark);
		mark_held(act::drinkmagic::ORIG_Streak);
		mark_held(act::drinkmagic::ORIG_Orb);
		register_module_held(37, act::drinkmagic::HeldReady, act::drinkmagic::HeldFrame);
	}
}
