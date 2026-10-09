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

// 30 fps part of mag078_mighty_guard.cpp: the held-frame drawing of the actor systems (included at the
// end of mag078_mighty_guard.cpp)
//
// Held frames (30 fps): Mighty Guard is drawn by its actor systems (task 0x7BE820) only; every
// particle actor (sprite sequences 0x7C16D0, prim models 0x7BF400) is updated THEN drawn in the same
// tick. The real tick notes, per actor system that drew, the GTE state, the module scratch stack, the
// Field_Alloc scratch, the texture scroll counter (state +0x28: 0x7BF400 advances it at every draw of
// an actor of definition index 4 / 5) and the morph vertex blocks its draws started from. The held
// frame redraws each noted system's actors, in the same order (sprite sequences, then models), from
// copies whose draw matrix (+0xAC: rotation / scale), view positions (+0xC0..+0xC8) and per-copy
// positions (+0x19C..) are half way to the next tick's: the next tick's motion step (0x7C1F60, the end
// test of 0x7C28E0 without its follow-up spawn, 0x7C1820, the scaled instance transform 0x7C2030)
// runs on a second copy; at fraction 1 the whole next actor. An actor ending on the next tick (not
// drawn at fraction 1, as on the next tick), a morphing model (definition kind 5) and an actor not in its motion phase hold; an actor whose
// sequence (+0x170) changes on the next tick holds until fraction 1. Flipbook frame, model, colours
// and the texture scroll hold (the scroll counter is put back before each system's replay, so the
// replay draws exactly the real tick's scroll; at fraction 1 the next tick's: the counter as the
// real tick left it); particles spawned on the next tick are not drawn
// (random growth stays at 15 Hz).
// The packets go to private buffers (module cursor 0x25EB534, frame arena 0x1D8E054) that start with
// the real tick's packets from the cursors of the tick's first draw (the master picks the module
// arena by tick parity before its queues run); the module globals, the module scratch stack, the
// Field_Alloc scratch, the shadow / RenderGeometry temporaries and the GTE registers are put back.
// The master, the emitters (0x7BE710) and the stage darkening tasks (0x7BE5F0 / 0x7C4310) draw
// nothing.

namespace ff8fx
{
namespace act
{
namespace mguard
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25EB530, BSS_HI = 0x25EC514;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t FA_CURSOR = 0x1D8E054;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step rewrites them in the actor data, which every actor system of the cast
	// shares, so a later system's update overwrites what an earlier one drew with
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

	struct SysNote
	{
		uint32_t sys;
		uint16_t scroll; // texture scroll counter (state +0x28) before the draws
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		VertexBlocks verts; // the vertex blocks its models were drawn with
	};
	static const int MAX_SYSTEMS = 8;
	static SysNote g_sys[MAX_SYSTEMS];
	static int g_nsys = 0;
	static uint32_t g_sys_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // the cursors at the tick's first draw
	static uint8_t g_held_packets[0x40000], g_held_fa[0x20000];

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		if (g_sys_tick != g_real_tick)
		{
			g_sys_tick = g_real_tick;
			g_nsys = 0;
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
			g_fa_start = MEM<uint32_t>(FA_CURSOR);
		}
		if (g_nsys >= MAX_SYSTEMS) return;
		SysNote &n = g_sys[g_nsys++];
		n.sys = sys;
		n.scroll = U16(sys, 0x28);
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		n.verts.save(sys);
	}

	static bool HeldReady() { return held_ready() && g_sys_tick == g_real_tick; }

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x7C1790 run on it): draw matrix, view positions, sub-positions; at fraction 1 the whole next
	// actor. Holds (cur = the drawn actor): not in its motion phase, a morphing model (kind 5), ends
	// on the next tick, sequence change before fraction 1
	enum { P_HOLD, P_NEXT, P_ENDS };
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0 || U8(def, 0x16) == 5) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7C28E0 (its follow-up spawn: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		mg_7C2030(nxt, def);
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
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x7C1790)
		}
		return P_NEXT;
	}

	static void SystemHeld(const SysNote &n, uint16_t scroll_next, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxt[ACTOR_BYTES];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		// the drawn actors: sprite sequences (definition kinds 0..2) and models (4 / 5)
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), P(nxt), num, den);
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch, texture
		// scroll counter and morph vertex blocks
		memcpy((void *)GTE_D, n.gte, GTE_D_N);
		memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), n.falloc, sizeof(n.falloc));
		U16(n.sys, 0x28) = num >= den ? scroll_next : n.scroll;
		n.verts.restore();
		// the sprite sequences, as 0x7C1610 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				mg_7C16D0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					mg_7C16D0(c);
				}
			}
		}
		// the models, as 0x7BF3A0 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				mg_7BF400(P(copies[i]), def);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_078;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		static VertexBlocks verts_now[MAX_SYSTEMS]; // the blocks as the real tick left them
		uint16_t scroll_now[MAX_SYSTEMS];
		for (int s = 0; s < g_nsys; s++)
		{
			verts_now[s].save(g_sys[s].sys);
			scroll_now[s] = U16(g_sys[s].sys, 0x28);
		}
		const uint32_t pc = MEM<uint32_t>(PACKET_CURSOR), fc = MEM<uint32_t>(FA_CURSOR);
		{
			// the buffers get the real tick's packets: the replayed draws write the same packets at
			// the same offsets over them (the halves a draw does not write are the real tick's)
			const uint32_t used = pc - g_pkt_start;
			if (g_pkt_start && pc > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
			const uint32_t used2 = fc - g_fa_start;
			if (g_fa_start && fc > g_fa_start && used2 < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used2);
		}
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int s = 0; s < g_nsys; s++)
			SystemHeld(g_sys[s], scroll_now[s], num, den);
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		for (int s = g_nsys - 1; s >= 0; s--)
		{
			verts_now[s].restore();
			U16(g_sys[s].sys, 0x28) = scroll_now[s];
		}
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag078_held()
	{
		mark_held(act::mguard::ORIG_ActorSystem);
		register_module_held(78, act::mguard::HeldReady, act::mguard::HeldFrame);
	}
}
