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

// 30 fps part of mag023_psycho_blast.cpp: the held-frame drawing of the actor system, the prim-model
// tasks (ring, orb, burst), the debris and the screen tint (included at the end of mag023_psycho_blast.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// module scratch stack, Field_Alloc scratch):
//   - the actor system (task 0x896250, as mag019_raldo_throw_held.inc): each drawn actor from a copy
//     whose view positions and draw matrix are half way to the next tick's (the next tick's update
//     pass on second copies, emitters put back); flipbook frame, model, fade and colours hold (the
//     next tick's at fraction 1); an actor ending on the next tick holds; an actor not in its motion
//     phase holds.
//   - the prim-model tasks (0x893A30 = the prim-model player at node +0x94): prim::play_held with the
//     node's placement; the orb's position half way to the next tick's (its flight step +0x556 /
//     0x1000 of the way, or the end point 0x895E40 on arrival) and, while it flattens, its y scale
//     and position half way to the next step's; the ring and the burst do not move. A play the next
//     tick does not make holds (prim::play_held keeps the last records).
//   - debris (task 0x899CC0, sprite sequences 0x899DD0): the 4 sprite positions half way to the next
//     tick's (velocity - velocity / 8, position + velocity / 16); flipbook frame held (the next one
//     at fraction 1). A node hidden by its last frame on the next tick holds (nothing at fraction
//     1, as the next tick); a released node holds. Debris spawned on the next tick is not drawn
//     (random growth stays at 15 Hz).
//   - the screen tint (engine task 0x8DDC30 in the last queue 0x2704028, fixed packets 0x2795110):
//     its colour half way to the next tick's script colour, drawn into a private copy; a tint the
//     real tick did not draw is not drawn, one the next tick does not draw holds (nothing at
//     fraction 1).
// The packets go to private buffers (frame arena cursor 0x1D8E054, module cursor 0x27011BC) that
// start with the real tick's packets; the module globals, the director and geometry blocks, the
// entities, the module scratch stack, the Field_Alloc scratch, the shadow / geometry temporaries and
// the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace psycho
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x27011B8, BSS_HI = 0x27044E0;
	static const uint32_t ENT_LO = 0x1D972C0, ENT_HI = 0x1D972C0 + 7 * 0x9C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t FRAME_CURSOR = 0x1D8E054; // frame arena cursor (actor models)
	static const uint32_t TINT_PKT = 0x2795110, TINT_PKT_N = 0x6C; // the tint task's fixed packets
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t DEBRIS_BYTES = 0xB0, PRIM_BYTES = 0x180, GEO_BYTES = 0x40;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x8981E0) rewrites them in the actor data
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
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		void save()
		{
			memcpy(gte, (const void *)GTE_D, GTE_D_N);
			memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
			fa = MEM<uint32_t>(FALLOC_PTR);
			memcpy(falloc, (const void *)fa, sizeof(falloc));
			memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		}
		void restore() const
		{
			memcpy((void *)GTE_D, gte, GTE_D_N);
			memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
			MEM<uint32_t>(FALLOC_PTR) = fa;
			memcpy((void *)fa, falloc, sizeof(falloc));
			memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		}
	};

	struct SysNote
	{
		uint32_t sys;
		DrawState st;
		VertexBlocks verts; // the vertex blocks its models were drawn with
	};
	struct PlayNote
	{
		uint32_t node;
		uint8_t bytes[PRIM_BYTES]; // the node as the real tick drew it
		DrawState st;
	};
	struct DebrisNote
	{
		uint32_t node;
		uint8_t bytes[DEBRIS_BYTES];
		DrawState st;
	};
	enum { EV_SYS, EV_PLAY, EV_DEBRIS };
	struct Event { int kind, i; };

	static const int MAX_SYS = 4, MAX_PLAY = 8, MAX_DEBRIS = 64, MAX_EV = MAX_SYS + MAX_PLAY + MAX_DEBRIS;
	static const int MAX_TINTS = 10;
	static SysNote g_sys[MAX_SYS];
	static PlayNote g_play[MAX_PLAY];
	static DebrisNote g_debris[MAX_DEBRIS];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nplay = 0, g_ndebris = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_frame_start = 0, g_pkt_start = 0; // frame arena / module cursors at the tick's start
	static uint8_t g_held_frame[0x80000];
	static uint8_t g_held_packets[0x10000];
	// the screen tints (engine task 0x8DDC30 in the last queue): their nodes before the queue runs
	static uint8_t g_tint[MAX_TINTS][0x40];
	static uint32_t g_tint_addr[MAX_TINTS];
	static int g_ntint = 0;
	static uint8_t g_tint_real[TINT_PKT_N];
	alignas(4) static uint8_t g_tint_pkt[TINT_PKT_N];

	// real tick: the master starts (before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nsys = g_nplay = g_ndebris = g_nev = 0;
		g_frame_start = MEM<uint32_t>(FRAME_CURSOR);
		g_pkt_start = 0; // (the master sets the tick's arena after this note: taken at the first draw)
		g_ntint = 0;
		for (uint32_t node = MEM<uint32_t>(Q_STAGE); node && g_ntint < MAX_TINTS; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30) continue;
			memcpy(g_tint[g_ntint], (const void *)node, 0x40);
			g_tint_addr[g_ntint++] = node;
		}
	}

	// real tick: the actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		if (g_note_tick != g_real_tick || g_nsys >= MAX_SYS || g_nev >= MAX_EV) return;
		if (!g_pkt_start) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		SysNote &n = g_sys[g_nsys];
		n.sys = sys;
		n.st.save();
		n.verts.save(sys);
		g_ev[g_nev++] = { EV_SYS, g_nsys++ };
	}

	// real tick: a prim-model task is about to play its model (0x893A30; its placement set)
	static void held_note_play(uint32_t node)
	{
		if (g_note_tick != g_real_tick || g_nplay >= MAX_PLAY || g_nev >= MAX_EV) return;
		if (!g_pkt_start) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		PlayNote &n = g_play[g_nplay];
		n.node = node;
		memcpy(n.bytes, (const void *)node, PRIM_BYTES);
		n.st.save();
		g_ev[g_nev++] = { EV_PLAY, g_nplay++ };
	}

	// real tick: a debris node is about to draw its 4 sprites (after its state step and motion)
	static void held_note_debris(uint32_t node)
	{
		if (g_note_tick != g_real_tick || g_ndebris >= MAX_DEBRIS || g_nev >= MAX_EV) return;
		if (!g_pkt_start) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		DebrisNote &n = g_debris[g_ndebris];
		n.node = node;
		memcpy(n.bytes, (const void *)node, DEBRIS_BYTES);
		n.st.save();
		g_ev[g_nev++] = { EV_DEBRIS, g_ndebris++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// ---- the actor system (as mag019_raldo_throw_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x8971B0 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor. P_ENDS: the actor ends on the next tick (it holds);
	// P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x8982F0 (its end spawns: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x8971B0)
		}
		return P_NEXT;
	}

	// the next tick's step (0x8971B0) of an actor that is not drawn, on the actor itself (the caller
	// puts it back): motion and transform, without the end spawns (0x898370)
	static void StepLive(uint32_t act)
	{
		if (S16(act, 0x1CC) != 0) return;
		const uint32_t def = DefOf(act);
		a_741050(act, def);
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(act, 0x1C8) = (uint16_t)(U16(act, 0x1C8) - 1);
			if (S16(act, 0x1C8) < 0) return;
		}
		if ((kind == 0 || kind == 2) && U8(act, 0x1D2) == 1) return;
		a_740910(act, def);
		a_741120(act, def);
		U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
	}

	// the actors of a noted system redrawn half way
	static void DrawSystem(const SysNote &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(n.verts.bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
		// 0x897030 / 0x896E70 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x897170) in list order, on the live actors (every one saved
		// first, all put back after): the emitters' placement / motion (0x898440 without its emission
		// and life count) and the particles' steps (0x8971B0 without its end spawns: Predict for the
		// drawn ones, whose next state is written back); an emitter attached to a particle (mode 3,
		// 0x898DF0) reads the particle's next position, as on the next tick
		static const int MAX_SAVE = 2 * MAX_ACTORS;
		static uint8_t asave[MAX_SAVE][ACTOR_BYTES];
		uint32_t aaddr[MAX_SAVE], alen[MAX_SAVE];
		int ns = 0, pi = 0;
		int pr[MAX_ACTORS];
		bool morph = false;
		for (uint32_t act = U32(n.sys, 0x2C); act; act = U32(act, 4))
		{
			const int16_t type = S16(act, 8);
			if (type != 0 && type != 1) continue;
			if (ns >= MAX_SAVE) break;
			aaddr[ns] = act;
			alen[ns] = type == 0 ? EMIT_BYTES : ACTOR_BYTES;
			memcpy(asave[ns], (const void *)act, alen[ns]);
			ns++;
			if (type == 0)
			{
				if (U16(act, 0x62) == 0)
				{
					a_742D00(act);
					U16(act, 0x62) = (uint16_t)(U16(act, 0x62) + 1);
				}
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					callp(0x898DF0, act);
				else if (mode == 4)
					callp(0x898F40, act);
			}
			else if (pi < nl && act == list[pi])
			{
				pr[pi] = Predict(list[pi], P(copies[pi]), P(nxts[pi]), num, den);
				if (pr[pi] == P_NEXT)
				{
					memcpy((void *)act, nxts[pi], ACTOR_BYTES);
					U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
					if (U8(DefOf(list[pi]), 0x16) == 5) morph = true;
				}
				pi++;
			}
			else
				StepLive(act);
		}
		for (int e = ns - 1; e >= 0; e--)
			memcpy((void *)aaddr[e], asave[e], alen[e]);
		// morphing models: the vertex blocks half way between the drawn ones and the next tick's
		if (morph)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					pb_8981E0(P(nxts[i]), DefOf(list[i]));
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
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				pb_8970F0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					pb_8970F0(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5) // (an actor ending on the next tick holds)
				pb_896ED0(P(copies[i]), def);
		}
	}

	// ---- the prim-model tasks (ring 0x896150, orb 0x893950, burst 0x895FF0)

	// the orb's next placement (position +0x1C..+0x20, y scale +0x58) on the node copy nxt, from the
	// state the next tick runs (the node as the real tick left it); false = the next tick does not
	// move it
	static bool PredictOrb(uint32_t node, uint32_t nxt)
	{
		if (!live(node, ORIG_Orb)) return false;
		memcpy((void *)nxt, (const void *)node, PRIM_BYTES);
		switch (S8(node, 0x29))
		{
		case 1: // 0x895ED0 the flight
		{
			U16(nxt, 0x17C) = (uint16_t)(U16(nxt, 0x17C) + 0x556);
			const int16_t t = S16(nxt, 0x17C);
			if (t >= 0x1000)
			{
				U16(nxt, 0x17C) = 0x1000;
				pb_895E40(nxt, nxt + 0x1C);
			}
			else
				for (int k = 0; k < 3; k++)
				{
					const int32_t s = S16(nxt, 0x16C + 2 * k);
					const int32_t e = S16(nxt, 0x174 + 2 * k);
					U16(nxt, 0x1C + 2 * k) = (uint16_t)(mul32(e - s, (int32_t)t) / 0x1000 + s);
				}
			return true;
		}
		case 2: // 0x8960F0 flattening
		{
			const uint32_t sy = U32(nxt, 0x58);
			if (sy == 0x1000)
				U32(nxt, 0x58) = 0x800;
			else if (sy == 0x800)
				U32(nxt, 0x58) = 0x400;
			pb_895E40(nxt, nxt + 0x1C);
			return true;
		}
		default:
			return false;
		}
	}

	// the parameter block of 0x893A30 for the node n (L: 0x5C bytes)
	static void PlayBlock(uint32_t n, uint32_t L)
	{
		memcpy((void *)L, (const void *)(n + 0x30), 0x20);
		x::scale3DMatrix(L, n + 0x50);
		S32(L, 0x14) = S16(n, 0x1C);
		S32(L, 0x18) = S16(n, 0x1E);
		S32(L, 0x1C) = S16(n, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(n, 0x86);
		U32(L, 0x44) = U32(n, 0x7C);
		U32(L, 0x38) = U32(n, 0x70);
		U16(L, 0x4C) = U16(n, 0x8C);
		U16(L, 0x4E) = U16(n, 0x8E);
		U16(L, 0x52) = U16(n, 0x92);
		U16(L, 0x50) = U16(n, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(n, 0x80);
		U16(L, 0x58) = U16(n, 0x88);
	}

	static void DrawPlay(const PlayNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[PRIM_BYTES], nxt[PRIM_BYTES];
		memcpy(cur, n.bytes, PRIM_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		if (U32(c, 8) == ORIG_Orb && PredictOrb(n.node, x1))
		{
			for (int k = 0; k < 3; k++)
				S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
			S32(c, 0x58) = lerp_i(S32(c, 0x58), S32(x1, 0x58), num, den);
		}
		alignas(4) static uint8_t fr[0x5C];
		memset(fr, 0, sizeof(fr));
		const uint32_t L = P(fr);
		n.st.restore();
		PlayBlock(c, L);
		void *p = port_of(PRIM_CB);
		prim::play_held((prim::Layout *)(n.node + 0x94), (prim::Callback)(p ? P(p) : PRIM_CB), (int)L, num, den);
	}

	// ---- debris

	static void DrawDebris(const DebrisNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[DEBRIS_BYTES], nxt[DEBRIS_BYTES];
		memcpy(cur, n.bytes, DEBRIS_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		// (a node released after the real tick holds its last draw)
		const bool next = live(n.node, ORIG_Debris) && (U8(c, 0x26) & 4) == 0;
		if (next)
		{
			// the next tick's state step (flipbook frame) and motion on a copy; hidden by its last
			// frame on the next tick, it holds
			memcpy(nxt, (const void *)n.node, DEBRIS_BYTES);
			if (S8(x1, 0x29) == 1)
				pb_89A4C0(x1);
			for (int k = 0; k < 4; k++)
			{
				const uint32_t v = x1 + 0x8C + 8 * k;
				const uint32_t p = x1 + 0x6C + 8 * k;
				for (int j = 0; j < 3; j++)
				{
					const int16_t s = S16(v, 2 * j);
					U16(v, 2 * j) = (uint16_t)(s - s / 8);
				}
				for (int j = 0; j < 3; j++)
					U16(p, 2 * j) = (uint16_t)(U16(p, 2 * j) + (uint16_t)(S16(v, 2 * j) / 16));
			}
			if (num >= den)
				memcpy(cur, nxt, DEBRIS_BYTES); // (hidden by its last frame: nothing, as the next tick)
			else if ((U8(x1, 0x26) & 4) == 0)
				for (int k = 0; k < 4; k++)
					for (int j = 0; j < 3; j++)
					{
						const int32_t o = 0x6C + 8 * k + 2 * j;
						S16(c, o) = (int16_t)lerp_i(S16(c, o), S16(x1, o), num, den);
					}
		}
		n.st.restore();
		for (int k = 0; k < 4; k++)
		{
			U32(c, 0x1C) = U32(c, 0x6C + 8 * k);
			U32(c, 0x20) = U32(c, 0x70 + 8 * k);
			pb_899DD0(c);
		}
	}

	// ---- screen tint (0x8DDC30 state 1 = 0x8DDCD0: colour script step, four semi-transparent flat
	// quads over the screen + a draw mode into OT base + 0x1C, fixed packets 0x2795110)

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
		g_mod = &MOD_023;
		static uint8_t bss[BSS_HI - BSS_LO], ents[ENT_HI - ENT_LO], dir[0x54], geo[GEO_BYTES], shadow[0x80];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(ents, (const void *)ENT_LO, sizeof(ents));
		const uint32_t dir_addr = MEM<uint32_t>(DIR_PTR);
		memcpy(dir, (const void *)dir_addr, sizeof(dir));
		const uint32_t geo_addr = MEM<uint32_t>(GEO_PTR);
		memcpy(geo, (const void *)geo_addr, sizeof(geo));
		now.save();
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t frame_cursor = MEM<uint32_t>(FRAME_CURSOR);
		const uint32_t pkt_cursor = MEM<uint32_t>(PACKET_CURSOR);
		memcpy(g_tint_real, (const void *)TINT_PKT, TINT_PKT_N);
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
			add_win(ev.kind == EV_SYS ? g_sys[ev.i].st.fa : ev.kind == EV_PLAY ? g_play[ev.i].st.fa : g_debris[ev.i].st.fa);
		}
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the prim callback's vertex blend buffer (inside the module globals, put back with them)
		// the buffers get the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		uint32_t used = frame_cursor - g_frame_start;
		if (g_frame_start && used < sizeof(g_held_frame)) memcpy(g_held_frame, (const void *)g_frame_start, used);
		MEM<uint32_t>(FRAME_CURSOR) = P(g_held_frame);
		used = pkt_cursor - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(g_sys[ev.i], num, den);
			else if (ev.kind == EV_PLAY)
				DrawPlay(g_play[ev.i], num, den);
			else
				DrawDebris(g_debris[ev.i], num, den);
		}
		TintHeld(num, den);
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)ENT_LO, ents, sizeof(ents));
		memcpy((void *)geo_addr, geo, sizeof(geo));
		memcpy((void *)dir_addr, dir, sizeof(dir));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag023_held()
	{
		mark_held(0x896250); // the actor system
		mark_held(act::psycho::ORIG_Ring);
		mark_held(act::psycho::ORIG_Orb);
		mark_held(act::psycho::ORIG_Burst);
		mark_held(act::psycho::ORIG_Debris);
		mark_held(0x8DDC30);
		register_module_held(23, act::psycho::HeldReady, act::psycho::HeldFrame);
	}
}
