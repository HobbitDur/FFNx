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

// 30 fps part of mag039_drain.cpp: the held-frame drawing of the actor systems (included at the end
// of mag039_drain.cpp)
//
// Held frames (30 fps): the drain stream is drawn by the actor systems (task 0x851A80): every
// particle actor (sprite sequences 0x853D30, prim models 0x852650) is updated THEN drawn in the same
// tick. The real tick notes, per actor system that drew, the GTE state, the module scratch stack
// and the Field_Alloc scratch its draws started from. The held frame redraws each noted system's
// actors, in the same order, from copies whose view positions (+0xC0..+0xC8 and the per-copy
// positions +0x19C..) are half way to the next tick's: the next tick's motion step (0x741050, the
// end test of 0x7419C0 without its spawn, 0x740910, 0x741120) runs on a second copy. An actor ending
// on the next tick, a morphing model (definition kind 5: its step rewrites the shared vertex block)
// and an actor not in its motion phase hold; rotation, flipbook frame, model and colours hold.
// Screen flashes (0x851900): redrawn with their colour held. The packets go to a private buffer
// (module cursor 0x269DA20); the module globals, the module scratch stack, the Field_Alloc scratch
// and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace drain
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x269D958, BSS_HI = 0x269E764;
	static const uint32_t GTE_D = 0x1CA8A10, GTE_D_N = 0x70, GTE_C = 0x1CA9230, GTE_C_N = 0xD0;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;

	struct SysNote
	{
		uint32_t sys;
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
	};
	static SysNote g_sys[8];
	static int g_nsys = 0;
	static uint32_t g_sys_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000];

	static uint32_t g_flash[8];
	static int g_nflash = 0;
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw

	static void held_begin_tick()
	{
		if (g_sys_tick == g_real_tick) return;
		g_sys_tick = g_real_tick;
		g_nsys = 0;
		g_nflash = 0;
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
	}

	// real tick: a screen flash draws its quad in colour `colour`
	static void held_note_flash(uint32_t colour)
	{
		held_begin_tick();
		if (g_nflash < 8) g_flash[g_nflash++] = colour;
	}

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		held_begin_tick();
		if (g_nsys >= 8) return;
		SysNote &n = g_sys[g_nsys++];
		n.sys = sys;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
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
		// end test of 0x7419C0 (its end spawns: not called)
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
		g_mod = &MOD_039;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x400];
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxt[ACTOR_BYTES];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		// screen flashes: their colour holds (the flash queue runs before the actor systems)
		for (int f = 0; f < g_nflash; f++) FlashDraw(g_flash[f]);
		for (int s = 0; s < g_nsys; s++)
		{
			const SysNote &n = g_sys[s];
			MEM<uint32_t>(ACTOR_STATE) = n.sys;
			// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
			// 0x853C70 / 0x8525F0 draw them
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
			// the draws start from the real tick's GTE state and scratch
			memcpy((void *)GTE_D, n.gte, GTE_D_N);
			memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
			memcpy((void *)fa, n.falloc, sizeof(n.falloc));
			for (int i = 0; i < nl; i++)
			{
				const uint32_t c = P(copies[i]);
				if (U8(DefOf(list[i]), 0x16) > 2) continue;
				const int8_t cnt = S8(c, 0x1D8);
				if (cnt == 1)
					r_853D30(c);
				else if (cnt > 0)
				{
					uint32_t sub = c + 0x19E;
					for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
					{
						S32(c, 0xC0) = S16(sub, -2);
						S32(c, 0xC4) = S16(sub, 0);
						S32(c, 0xC8) = S16(sub, 2);
						r_853D30(c);
					}
				}
			}
			for (int i = 0; i < nl; i++)
			{
				const uint32_t def = DefOf(list[i]);
				if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5)
					r_852650(P(copies[i]), def);
			}
		}
		memcpy((void *)fa, falloc, sizeof(falloc));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag039_held()
	{
		mark_held(act::drain::ORIG_ActorSystem);
		mark_held(act::drain::ORIG_Flash);
		register_module_held(39, act::drain::HeldReady, act::drain::HeldFrame);
	}
}
