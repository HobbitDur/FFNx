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

// 30 fps part of mag024_esuna.cpp: the held-frame drawing of the actor systems (included at the end
// of mag024_esuna.cpp)
//
// Held frames (30 fps): Esuna is drawn by its actor systems (task 0x88D4C0) only; every particle
// actor (sprite sequences 0x890360, prim models 0x88E090) is updated THEN drawn in the same tick.
// The real tick notes, per actor system that drew, the GTE state, the module scratch stack, the
// Field_Alloc scratch, the texture scroll counter (state +0x2A: 0x88E090 advances it at every draw
// of an actor of definition index 1 / 2) and the morph vertex blocks its draws started from. The
// held frame redraws each noted system's actors, in the same order, from copies whose view positions
// (+0xC0..+0xC8 and the per-copy positions +0x19C..) are half way to the next tick's: the next
// tick's motion step (0x890BF0, the end test of 0x891560 without its spawn, 0x8904B0, 0x890CC0) runs
// on a second copy. An actor ending on the next tick, a morphing model (definition kind 5: its step
// rewrites the shared vertex block) and an actor not in its motion phase hold; rotation, flipbook
// frame, model, colours and the texture scroll hold (the scroll counter is put back before each
// system's replay, so the replay draws exactly the real tick's scroll). The morph (0x891450) writes
// its destination vertex blocks in the loaded actor data, which every actor system of the cast
// shares: each system's replay puts back the blocks it drew with, and the held frame then puts back
// the current ones. The packets go to a private buffer (module cursor 0x2700474); the module
// globals, the module scratch stack, the Field_Alloc scratch and the GTE registers are put back.
// The emitters (0x88D3B0) and the stage wobble tasks (0x88D290 / 0x892ED0) draw nothing.

namespace ff8fx
{
namespace act
{
namespace esuna
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x2700470, BSS_HI = 0x27011B4;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x891450) rewrites them in the actor data, which every actor system of
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

	struct SysNote
	{
		uint32_t sys;
		uint16_t scroll; // texture scroll counter (state +0x2A) before the draws
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		VertexBlocks verts; // the vertex blocks its models were drawn with
	};
	static SysNote g_sys[8];
	static int g_nsys = 0;
	static uint32_t g_sys_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000];
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		if (g_sys_tick != g_real_tick)
		{
			g_sys_tick = g_real_tick;
			g_nsys = 0;
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		}
		if (g_nsys >= 8) return;
		SysNote &n = g_sys[g_nsys++];
		n.sys = sys;
		n.scroll = U16(sys, 0x2A);
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

	// the actor's copy `cur` with its view positions half way to the next tick's
	static void Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0 || U8(def, 0x16) == 5) return;
		a_741050(nxt, def);
		// end test of 0x891560 (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return;
		a_740910(nxt, def);
		a_741120(nxt, def);
		for (int k = 0; k < 3; k++)
			S32(cur, 0xC0 + 4 * k) = lerp_i(S32(cur, 0xC0 + 4 * k), S32(nxt, 0xC0 + 4 * k), num, den);
		const int32_t copies = S8(cur, 0x1D8);
		for (int32_t i = 0; i < copies && i < 16; i++)
			for (int k = 0; k < 3; k++)
			{
				const int32_t o = 0x19C + 8 * i + 2 * k;
				S16(cur, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den);
			}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_024;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x400];
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxt[ACTOR_BYTES];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		static VertexBlocks verts_now[8]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int s = 0; s < g_nsys; s++)
		{
			const SysNote &n = g_sys[s];
			MEM<uint32_t>(ACTOR_STATE) = n.sys;
			// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
			// 0x8902A0 / 0x88E030 draw them
			uint32_t list[MAX_ACTORS];
			int nl = 0;
			for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
			{
				if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
				const uint8_t kind = U8(DefOf(act), 0x16);
				if (kind > 5 || kind == 3) continue;
				list[nl++] = act;
			}
			for (int i = 0; i < nl; i++)
				Predict(list[i], P(copies[i]), P(nxt), num, den);
			// the draws start from the real tick's GTE state, scratch and texture scroll counter
			memcpy((void *)GTE_D, n.gte, GTE_D_N);
			memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
			memcpy((void *)fa, n.falloc, sizeof(n.falloc));
			U16(n.sys, 0x2A) = n.scroll;
			n.verts.restore();
			for (int i = 0; i < nl; i++)
			{
				const uint32_t c = P(copies[i]);
				if (U8(DefOf(list[i]), 0x16) > 2) continue;
				const int8_t cnt = S8(c, 0x1D8);
				if (cnt == 1)
					e_890360(c);
				else if (cnt > 0)
				{
					uint32_t sub = c + 0x19E;
					for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
					{
						S32(c, 0xC0) = S16(sub, -2);
						S32(c, 0xC4) = S16(sub, 0);
						S32(c, 0xC8) = S16(sub, 2);
						e_890360(c);
					}
				}
			}
			for (int i = 0; i < nl; i++)
			{
				const uint32_t def = DefOf(list[i]);
				if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5)
					e_88E090(P(copies[i]), def);
			}
		}
		memcpy((void *)fa, falloc, sizeof(falloc));
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag024_held()
	{
		mark_held(act::esuna::ORIG_ActorSystem);
		register_module_held(24, act::esuna::HeldReady, act::esuna::HeldFrame);
	}
}
