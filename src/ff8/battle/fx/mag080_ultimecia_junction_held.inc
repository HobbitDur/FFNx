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

// 30 fps part of mag080_ultimecia_junction.cpp: the held-frame drawing (included at the end of
// mag080_ultimecia_junction.cpp)
//
// Every drawing task of the effect updates THEN draws in the same tick, except the bolts (drawn THEN
// advanced). The real tick notes each draw in queue order (actor systems, prim-model plays, flakes,
// orbiting particles, bolts, the target's model copy) with the GTE state, the module scratch stack and
// the Field_Alloc scratch it started from; the held frame replays them in the same order, each at
// tick + num/den:
//   Actor systems - as Doom (mag008_doom_held.inc): each drawn actor from a copy whose view positions /
//                   draw matrix are half way to the next tick's (the next motion step on a second copy;
//                   the whole next actor at fraction 1); an emitter not initialised yet (+0x62 == 0:
//                   its init may draw rand()) is not stepped.
//   Prim plays    - prim::play_held with the node's placement; the cut-2 model (task 0x7ABF90) rises
//                   on the caster by the height table: it moves half way to the next step's height.
//   Flakes        - static sprites: the flipbook frame steps at fraction 1.
//   Particles     - orbit the model position (director +0x10): the turning angle +0x42 half way to the
//                   next tick's (+0x40 per tick), the model position half way to the next tick's; the
//                   flipbook frame steps at fraction 1; a particle the next tick hides holds its draw.
//   Bolts         - the chains as the real tick drew them (segments are appended / freed in 15 Hz
//                   batches), every segment half way along its next motion (position += velocity,
//                   gradient += step, floor 0) and each chain's fade half way (-0x100 per tick).
//   Model copy    - the target's model growing / spinning / morphing (0x7AFEC0 / 0x7AFFB0): the target
//                   at its in-between pose (pose_midpoint; the engine's own next step at fraction 1);
//                   the block's pivot, spin, morph weight and radii and the target's matrix translation
//                   half way to the next tick's (the next state step of the junction task run on copies:
//                   pivot 0x7B4750 at the next pose, scale / spin / morph steps; its spawns, sounds and
//                   chain transformation are not run); the random shake (+0xB4..+0xB8) stays the real
//                   tick's, a spin that snaps back to 0 holds; a copy the next tick no longer draws holds
//                   its last draw and is not drawn at fraction 1.
// The packets go to private buffers (module sprite cursor 0x25D493C and frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the exe data the effect rewrites (actor data,
// sequence data), the bolt buffers, the target's skeleton, the module scratch stack, the Field_Alloc
// scratch, the shadow / RenderGeometry temporaries and the GTE registers are put back. The master, the
// director and the stage wobble tasks (0x7AE690 / 0x7AF1D0 / 0x7AF2C0) draw nothing.

namespace ff8fx
{
namespace act
{
namespace ujunct
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25D4938, BSS_HI = 0x25DF338;
	// exe data the effect rewrites in place (actor data, sequence data)
	static const uint32_t XDATA[2][2] = { { 0x1749C00, 0x1754C00 }, { 0x175AC00, 0x175E800 } };
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const uint32_t EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 128;
	static const uint32_t NODE_BYTES = 0xA0;
	static const uint32_t BLK_BYTES = 0xC4, ENT_BYTES = 0x9C;
	static const uint32_t BOLT_BYTES = 0x3000 + 0x40 * 0x30; // segments + chains

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

	enum { K_SYSTEM, K_PLAY, K_FLAKE, K_PARTICLE, K_BOLTS, K_COPY };
	struct Note
	{
		int kind;
		uint32_t addr;        // node / actor system state
		uint8_t node[NODE_BYTES];
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;
		uint32_t d10[2];      // particles: director +0x10 / +0x14 (the model position) they drew with
	};
	static const int MAX_NOTES = 200;
	static Note g_notes[MAX_NOTES];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0;
	static uint8_t g_held_packets[0x40000], g_held_fa[0x60000];

	// the bolts as the real tick drew them (segments + chains)
	static uint8_t g_bolts[BOLT_BYTES];
	static uint32_t g_bolts_tick = 0xFFFFFFFF, g_bolts_base = 0;

	// the junction task's model copy draw of this real tick (one task)
	struct ModelNote
	{
		uint32_t node;
		uint8_t blk[BLK_BYTES];   // the block as drawn
		uint8_t ent[ENT_BYTES];   // the target entity as drawn (matrix translation +0x54..)
	};
	static ModelNote g_model;
	static uint32_t g_model_tick = 0xFFFFFFFF;

	static Note *held_note(int kind, uint32_t addr, uint32_t bytes)
	{
		if (g_note_tick != g_real_tick)
		{
			g_note_tick = g_real_tick;
			g_nnotes = 0;
			g_nsys_verts = 0;
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
			g_fa_start = MEM<uint32_t>(FA_CURSOR);
		}
		if (g_nnotes >= MAX_NOTES) return nullptr;
		Note &n = g_notes[g_nnotes++];
		n.kind = kind;
		n.addr = addr;
		n.verts = -1;
		if (bytes) memcpy(n.node, (const void *)addr, bytes);
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		return &n;
	}

	// ---- the FX_HELD statements of the real tick
	static void held_note_system(uint32_t sys)
	{
		Note *n = held_note(K_SYSTEM, sys, 0);
		if (n && g_nsys_verts < MAX_SYSTEMS)
		{
			n->verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}
	static void held_note_play(uint32_t node) { held_note(K_PLAY, node, 0x94); }
	static void held_note_flake(uint32_t node) { held_note(K_FLAKE, node, NODE_BYTES); }
	static void held_note_particle(uint32_t node)
	{
		Note *n = held_note(K_PARTICLE, node, NODE_BYTES);
		if (!n) return;
		n->d10[0] = U32(MEM<uint32_t>(DIR_PTR), 0x10);
		n->d10[1] = U32(MEM<uint32_t>(DIR_PTR), 0x14);
	}
	static void held_note_bolts()
	{
		if (!held_note(K_BOLTS, 0, 0)) return;
		g_bolts_base = MEM<uint32_t>(BOLT_BASE);
		memcpy(g_bolts, (const void *)g_bolts_base, BOLT_BYTES);
		g_bolts_tick = g_real_tick;
	}
	static void held_note_copy(uint32_t node)
	{
		if (!held_note(K_COPY, node, NODE_BYTES)) return;
		g_model.node = node;
		memcpy(g_model.blk, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(g_model.ent, (const void *)(0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C), ENT_BYTES);
		g_model_tick = g_real_tick;
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
	// actor systems (as mag008_doom_held.inc: Doom's actor-system code is this module's)
	// ====================================================================================
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x7B1A20 run on it): view positions, draw matrix, sub-positions; at fraction 1 the whole next
	// actor. P_ENDS: the actor ends on the next tick (its sprite holds its last draw, its model is not
	// drawn at fraction 1); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7B2B60 (its follow-up spawn 0x7B2BE0: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x7B1A20)
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
		// the next tick's update pass (0x7B19E0) in list order: the emitters' placement / motion
		// (0x7B2CB0 without its emission and life count, put back after: their particles read them)
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
					continue; // (its init 0x7B3EA0 may draw rand(): not stepped)
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					a_7424C0(act);
				else if (mode == 4)
					ud_7B37B0(act);
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
					ud_7B2A50(P(nxts[i]), DefOf(list[i]));
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
		// sprite sequences (0x7B18A0: one draw, or one per sub-position)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				ud_7B1960(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					ud_7B1960(c);
				}
			}
		}
		// models (0x7B16E0)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				ud_7B1740(P(copies[i]), def);
		}
	}

	// ====================================================================================
	// flakes (0x7AE7D0) and orbiting particles (0x7AF030): sprite tasks updated then drawn (0x7AE840)
	// ====================================================================================
	// the next tick's model position (director +0x10 / +0x14): written by the prim-model tasks 0x7ABF90 /
	// 0x7B4540 before the particles run (the last one noted on this tick wins, as in the queue)
	static bool g_d10_valid = false;
	static uint32_t g_d10_next[2];

	// the position a prim-model task places itself at on the next tick (0x7ABF90: the caster raised by
	// the height table at director +0x50 + 1; 0x7B4540: unchanged)
	static void PlayNextPos(uint32_t node, const uint8_t *cur, uint32_t out[2])
	{
		out[0] = U32(P(cur), 0x1C);
		out[1] = U32(P(cur), 0x20);
		// (a node released on the real tick: its play ended, it is not drawn on the next tick)
		if (U32(node, 8) != U32(P(cur), 8) || (U8(node, 0x26) & 1) != 0)
			return;
		if (U32(node, 8) == 0x7ABF90 && S8(node, 0x29) == 1)
		{
			alignas(4) uint8_t v[8];
			ud_7AC060(node, P(v), (uint32_t)(uint16_t)(U16(MEM<uint32_t>(DIR_PTR), 0x50) + 1));
			out[0] = U32(P(v), 0);
			out[1] = U32(P(v), 4);
		}
	}

	static void D10Predict()
	{
		g_d10_valid = false;
		for (int i = 0; i < g_nnotes; i++)
		{
			const Note &n = g_notes[i];
			if (n.kind != K_PLAY) continue;
			const uint32_t fn = U32(n.addr, 8);
			if (fn != 0x7ABF90 && fn != 0x7B4540) continue;
			PlayNextPos(n.addr, n.node, g_d10_next);
			g_d10_valid = true;
		}
	}

	static void SpriteHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t a[NODE_BYTES], b[NODE_BYTES], c[NODE_BYTES];
		memcpy(a, n.node, NODE_BYTES);           // as drawn on the real tick
		const uint32_t A = P(a), B = P(b), C = P(c);
		static const uint32_t FLAKE[3] = { 0x7AEF10, 0x7AEF30, 0x7AEF80 };
		static const uint32_t PART[3] = { 0x7AF130, 0x7AF190, 0x7AF1B0 };
		memcpy(b, a, NODE_BYTES);                // the next tick: its state step
		callp((n.kind == K_FLAKE ? FLAKE : PART)[S8(B, 0x29)], B);
		// a part the next tick hides (+0x26 bit 2: its sequence ended) holds its last draw
		if (U8(B, 0x26) & 4)
		{
			memcpy(b, a, NODE_BYTES);
			num = 0;
		}
		const bool last = num == den;
		memcpy(c, last ? b : a, NODE_BYTES);
		if (n.kind == K_FLAKE)
		{
			U32(C, 0x1C) = U32(C, 0x5C);
			U32(C, 0x20) = U32(C, 0x60);
			ud_7AE840(C);
			return;
		}
		// the orbit: the turning angle and the model position half way to the next tick's
		if (num != 0)
		{
			alignas(4) uint8_t loc[0x34] = {};
			const uint32_t V = P(loc), O = V + 8, M = V + 0x14;
			const int16_t a42 = S16(A, 0x42);
			const int16_t ang = (int16_t)(a42 + (int16_t)(0x40 * num / den));
			uint32_t dn[2] = { n.d10[0], n.d10[1] };
			if (g_d10_valid) { dn[0] = g_d10_next[0]; dn[1] = g_d10_next[1]; }
			int16_t dpos[3];
			for (int k = 0; k < 3; k++)
				dpos[k] = (int16_t)lerp_i(S16(P(n.d10), 2 * k), S16(P(dn), 2 * k), num, den);
			U16(C, 0x42) = (uint16_t)ang;
			x::MAG_022_sub_8DD770(M);
			x::sub_8DD960(M, (uint32_t)(int32_t)S16(C, 0x44));
			x::sub_8DD7E0(M, (uint32_t)(int32_t)S16(C, 0x40));
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)ang);
			U16(V, 4) = U16(C, 0x9E);
			U16(V, 0) = 0;
			U16(V, 2) = 0;
			x::matrixMultiplyVector(M, V, O);
			for (int k = 0; k < 3; k++)
				U16(C, 0x1C + 2 * k) = (uint16_t)((uint16_t)dpos[k] + U16(O, 2 * k));
		}
		ud_7AE840(C);
	}

	// ====================================================================================
	// bolts (0x7AF4D0): the real tick's chains, each segment / chain half way along its next motion
	// ====================================================================================
	static void BoltsHeld(int num, int den)
	{
		if (g_bolts_tick != g_real_tick || g_bolts_base != MEM<uint32_t>(BOLT_BASE)) return;
		alignas(4) static uint8_t now[BOLT_BYTES];
		const uint32_t base = g_bolts_base;
		memcpy(now, (const void *)base, BOLT_BYTES);
		memcpy((void *)base, g_bolts, BOLT_BYTES);
		uint32_t c = base + 0x3000;
		for (int k = 0; k < 0x40; k++, c += 0x30)
		{
			if (U32(c, 0) == 0) continue;
			const int16_t fade = (int16_t)(U16(c, 0x2E) + 0xFF00);
			if (fade <= 0)
				continue; // (the chain is freed on the next tick: it holds its last draw)
			U16(c, 0x2E) = (uint16_t)lerp_i(S16(c, 0x2E), fade, num, den);
			for (uint32_t s = U32(c, 0); s != 0; s = U32(s, 0x14))
			{
				int16_t g = (int16_t)(U16(s, 0x10) + U16(s, 0x12));
				if (g < 0) g = 0;
				U16(s, 0x10) = (uint16_t)lerp_i(S16(s, 0x10), g, num, den);
				for (int j = 0; j < 3; j++)
					U16(s, 2 * j) = (uint16_t)lerp_i(S16(s, 2 * j), (int16_t)(U16(s, 2 * j) + U16(s, 8 + 2 * j)), num, den);
			}
		}
		ud_7AF4D0();
		memcpy((void *)base, now, BOLT_BYTES);
	}

	// ====================================================================================
	// the target's model copy (0x7AF390 -> 0x7AFEC0 / 0x7AFFB0)
	// ====================================================================================
	struct ModelNext
	{
		bool valid, drawn;
		uint8_t blk[BLK_BYTES];
		int32_t tr[3];       // the target's matrix translation the next tick draws with
	};
	static ModelNext g_mnext;

	static uint32_t ModelEntity(uint32_t node) { return 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C; }
	// the target's skeleton (bone records rebuilt by 0x508C90; pose_midpoint changes them)
	static uint32_t SkelOf(uint32_t ent) { const uint32_t m = U32(ent, 0x64); return m ? U32(m, 0) : 0; }
	static uint32_t SkelBytes(uint32_t sk) { return sk ? 16 + 48 * (uint32_t)U8(sk, 0) : 0; }
	// the target's next pose exactly as the next tick has it: the battle loop's animation step (the engine reader
	// 0x508F90 on the animation command +0x6C, then the bone matrices 0x508C90); the caller puts the skeleton and the
	// entity back
	static void EnginePoseNext(uint32_t ent)
	{
		if (U8(ent, 0x72) < U8(ent, 0x73))
			fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x508F90)(ent + 0x60, ent + 0x6C);
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
	}
	static uint8_t g_skel_save[16 + 48 * 256];

	// the next tick's state step of the junction task on copies (its pure parts: pivot 0x7B4750 at the next
	// pose, scale / spin / morph steps; no spawn, sound, chain transformation or rand()) -> g_mnext; everything
	// it touched is put back
	static void ModelPredict()
	{
		g_mnext.valid = false;
		if (g_model_tick != g_real_tick) return;
		const uint32_t node = g_model.node;
		const uint32_t ent = ModelEntity(node);
		const uint32_t sk = SkelOf(ent);
		const uint32_t skn = SkelBytes(sk);
		if (skn > sizeof(g_skel_save)) return;
		alignas(4) static uint8_t blk_save[BLK_BYTES], ent_save[ENT_BYTES], nc[NODE_BYTES];
		memcpy(blk_save, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(ent_save, (const void *)ent, ENT_BYTES);
		if (sk) memcpy(g_skel_save, (const void *)sk, skn);
		memcpy(nc, (const void *)node, NODE_BYTES);
		const uint32_t NC = P(nc), B = MODEL_BLOCK;
		const int16_t spin0 = S16(B, 0x98);
		const uint16_t w770c = MEM<uint16_t>(0x1D9770C); // (written by the bone query of 0x7B4750: GetEffectSpawnPosition)
		// the next pose (the bones the next tick's pivot reads)
		EnginePoseNext(ent);
		bool spin_snaps = false;
		switch (S8(NC, 0x29))
		{
		case 5: // 0x7B4820
		{
			ud_7B4750(NC, B);
			uint16_t v = (uint16_t)(U16(B, 0xA4) + 0x40);
			if ((int16_t)v >= 0x500) { v = 0x500; U8(NC, 0x29) = (uint8_t)(U8(NC, 0x29) + 1); }
			U16(B, 0xA4) = v;
			U16(B, 0xA6) = v;
			U16(B, 0xA2) = v;
			break;
		}
		case 6: // 0x7B4880 (its shake stays the real tick's: rand())
		{
			U16(NC, 0x9C) = (uint16_t)(U16(NC, 0x9C) + 1);
			const int16_t k = S16(NC, 0x9C);
			if (k <= 0x2D)
				U16(B, 0x98) = (uint16_t)((U16(B, 0x98) + 0x5B) & 0xFFF);
			else
				U16(B, 0x98) = 0;
			ud_7B4750(NC, B);
			uint16_t v = (uint16_t)(U16(B, 0xA4) + 0x40);
			if ((int16_t)v >= 0x600) v = 0x600;
			U16(B, 0xA4) = v;
			U16(B, 0xA6) = v;
			U16(B, 0xA2) = v;
			const uint16_t m = (uint16_t)(U16(B, 0xC2) + 0xFFC0);
			if ((int16_t)m <= 0)
			{
				U8(NC, 0x29) = (uint8_t)(U8(NC, 0x29) + 1);
				U16(B, 0xC2) = 0;
				U16(B, 0xA0) = 0;
				U16(B, 0x98) = 0;
			}
			else
			{
				U16(B, 0xC2) = m;
				U16(B, 0xA0) = m;
			}
			break;
		}
		case 7: // 0x7B49F0: the copy is no longer drawn
			U8(NC, 0x26) &= 0xF7;
			break;
		default:
			break;
		}
		spin_snaps = S16(B, 0x98) != (int16_t)((spin0 + 0x5B) & 0xFFF) && S16(B, 0x98) != spin0;
		if (spin_snaps)
			U16(B, 0x98) = (uint16_t)spin0; // (snaps back: held)
		const uint8_t fl = U8(NC, 0x26);
		g_mnext.drawn = (fl & 8) != 0;
		memcpy(g_mnext.blk, (const void *)B, BLK_BYTES);
		// 0x7AF390: the target's position words = anchor - pivot + shake; +0x54.. = the sign-extended words
		g_mnext.tr[0] = (int16_t)(uint16_t)(U16(B, 0x78) - U16(B, 0x70) + U16(B, 0xB4));
		g_mnext.tr[1] = (int16_t)(uint16_t)(U16(B, 0x7A) - U16(B, 0x72) + U16(B, 0xB6));
		g_mnext.tr[2] = (int16_t)(uint16_t)(U16(B, 0x7C) - U16(B, 0x74) + U16(B, 0xB8));
		g_mnext.valid = true;
		// put back
		MEM<uint16_t>(0x1D9770C) = w770c;
		if (sk) memcpy((void *)sk, g_skel_save, skn);
		memcpy((void *)ent, ent_save, ENT_BYTES);
		memcpy((void *)MODEL_BLOCK, blk_save, BLK_BYTES);
	}

	// 0x7AFEC0 without its bone builds (the caller set the pose)
	static void ModelDraw(uint32_t blk)
	{
		const int32_t idx = S16(blk, 0xA8);
		const uint32_t ent = 0x1D972C0 + (uint32_t)(idx * 0x9C);
		memcpy((void *)(blk + 0x20), (const void *)(ent + 0x40), 0x20);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x40);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x42);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(blk + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(blk + 0x54);
		ud_7AFFB0(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			ud_7AFFB0(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
	}

	static void ModelHeld(const Note &n, int num, int den)
	{
		if (g_model_tick != g_real_tick || !g_mnext.valid) return;
		const bool ends = !g_mnext.drawn;
		if (ends && num >= den) return; // the next tick does not draw it
		const uint32_t node = n.addr;
		const uint32_t ent = ModelEntity(node);
		const uint32_t sk = SkelOf(ent);
		const uint32_t skn = SkelBytes(sk);
		alignas(4) static uint8_t blk_save[BLK_BYTES], ent_save[ENT_BYTES];
		memcpy(blk_save, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(ent_save, (const void *)ent, ENT_BYTES);
		if (sk) memcpy(g_skel_save, (const void *)sk, skn);
		// the block and the target as drawn on the real tick
		memcpy((void *)MODEL_BLOCK, g_model.blk, BLK_BYTES);
		memcpy((void *)ent, g_model.ent, ENT_BYTES);
		if (ends)
			x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60); // held: the real tick's pose
		else
		{
			const uint32_t B = MODEL_BLOCK, Q = P(g_mnext.blk);
			for (int k = 0; k < 3; k++) // pivot +0x70
				S16(B, 0x70 + 2 * k) = (int16_t)lerp_i(S16(B, 0x70 + 2 * k), S16(Q, 0x70 + 2 * k), num, den);
			// spin +0x98 (+0x5B per tick, 12-bit wrap: the short way round)
			{
				const int32_t d = ((S16(Q, 0x98) - S16(B, 0x98) + 0x800) & 0xFFF) - 0x800;
				S16(B, 0x98) = (int16_t)(S16(B, 0x98) + d * num / den);
			}
			for (int k = 0; k < 4; k++) // morph weight +0xA0, radii +0xA2..+0xA6
				S16(B, 0xA0 + 2 * k) = (int16_t)lerp_i(S16(B, 0xA0 + 2 * k), S16(Q, 0xA0 + 2 * k), num, den);
			for (int k = 0; k < 3; k++) // the target's matrix translation
				S32(ent, 0x54 + 4 * k) = lerp_i(S32(ent, 0x54 + 4 * k), g_mnext.tr[k], num, den);
			if (num >= den)
				EnginePoseNext(ent); // fraction 1: the next tick's pose
			else
				pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), num, den);
		}
		ModelDraw(MODEL_BLOCK);
		if (sk) memcpy((void *)sk, g_skel_save, skn);
		memcpy((void *)ent, ent_save, ENT_BYTES);
		memcpy((void *)MODEL_BLOCK, blk_save, BLK_BYTES);
	}

	// ====================================================================================
	// prim-model plays (0x7AC0B0 on the node)
	// ====================================================================================
	static void PlayHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		alignas(4) static uint8_t cp[0x94];
		memcpy(cp, n.node, 0x94);                // the placement the real tick played with
		const uint32_t N = P(cp);
		// the rising model (0x7ABF90) moves half way to the next step's height
		uint32_t nx[2];
		PlayNextPos(node, n.node, nx);
		for (int j = 0; j < 3; j++)
			S16(N, 0x1C + j * 2) = (int16_t)lerp_i(S16(N, 0x1C + j * 2), S16(P(nx), j * 2), num, den);
		// the parameter block of 0x7AC0B0
		alignas(4) static uint8_t fr[0x5C];
		memset(fr, 0, sizeof(fr));
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		if (U16(N, 0x62) != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)S16(N, 0x62));
		if (U16(N, 0x60) != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)S16(N, 0x60));
		if (U16(N, 0x64) != 0) x::sub_8DD960(L, (uint32_t)(int32_t)S16(N, 0x64));
		x::scale3DMatrix(L, N + 0x50);
		S32(L, 0x14) = S16(N, 0x1C);
		S32(L, 0x18) = S16(N, 0x1E);
		S32(L, 0x1C) = S16(N, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(N, 0x86);
		U32(L, 0x44) = U32(N, 0x7C);
		U32(L, 0x38) = U32(N, 0x70);
		U16(L, 0x4C) = U16(N, 0x8C);
		U16(L, 0x52) = U16(N, 0x92);
		U16(L, 0x4E) = U16(N, 0x8E);
		U16(L, 0x50) = U16(N, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(N, 0x80);
		U16(L, 0x58) = U16(N, 0x88);
		void *p = port_of(PRIM_CB);
		prim::play_held((prim::Layout *)(node + 0x94), (prim::Callback)(p ? P(p) : PRIM_CB), (int)L, num, den);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_080;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		static uint8_t xdata[2][0xB000];
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
		{
			const uint32_t used = pc - g_pkt_start;
			if (g_pkt_start && pc > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
			const uint32_t used2 = fc - g_fa_start;
			if (g_fa_start && fc > g_fa_start && used2 < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used2);
		}
		ModelPredict();
		D10Predict();
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int i = 0; i < g_nnotes; i++)
		{
			const Note &n = g_notes[i];
			Restore(n);
			switch (n.kind)
			{
			case K_SYSTEM: SystemHeld(n, num, den); break;
			case K_PLAY: PlayHeld(n, num, den); break;
			case K_FLAKE:
			case K_PARTICLE: SpriteHeld(n, num, den); break;
			case K_BOLTS: BoltsHeld(num, den); break;
			case K_COPY: ModelHeld(n, num, den); break;
			}
		}
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
	static void register_mag080_held()
	{
		// the drawing tasks: the module's own (flakes, particles, the junction task: bolts + model copy) and the
		// engine task bodies running this module's states (actor systems 0x7B0AE0, the prim-model tasks
		// 0x7ABF90 / 0x7B4540)
		static const uint32_t HELD[] = { 0x7AE7D0, 0x7AF030, 0x7AF390, 0x7B0AE0, 0x7ABF90, 0x7B4540, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(80, act::ujunct::HeldReady, act::ujunct::HeldFrame);
	}
}
