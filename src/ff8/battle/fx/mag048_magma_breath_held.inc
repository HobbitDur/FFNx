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

// 30 fps part of mag048_magma_breath.cpp: the held-frame drawing of the screen flashes and the
// actor systems (included at the end of mag048_magma_breath.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// Field_Alloc scratch):
//   - screen flashes (task 0x825220): the quad's colour half way (per channel) to the next tick's
//     (the colour script step run on the node's state); a flash the next tick does not draw (end
//     of its script) holds, and is not drawn at fraction 1.
//   - actor systems (task 0x8253B0; the design of mag030_counter_laser_eye_held.inc): each noted
//     system's particle actors are redrawn from copies whose view positions and draw matrix
//     (rotation / scale) are half way to the next tick's (the next tick's update pass runs on
//     second copies, in list order, with the emitters' placement / motion applied to the emitters
//     and put back); a morphing model's vertex block is half way to the next tick's morph
//     (0x828BA0); flipbook frame, fade and colours hold (the next tick's at fraction 1); an actor
//     ending on the next tick holds (a model: not drawn at fraction 1); an actor not in its motion
//     phase holds; an actor whose model (+0x170, the lava stream's mesh frames) changes on the
//     next tick holds (no mesh lies in between).
//   - the lava stream's texture scroll (0x828070: -8 V on the model record per update of a
//     definition-0 actor, shared by every system of the cast): each system draws from its model
//     records as noted at its draw, scrolled by the fraction of every scroll between that draw and
//     its draw on the next tick (the real tick's later ones, then the predicted ones: the
//     prediction logs them through held_defer_scroll instead of doing them); the records are put
//     back after the held frame.
// The packets go to a private buffer (module cursor 0x26481B8); the module globals, the module
// scratch stack, the Field_Alloc scratch and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace magmabreath
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x26480F0, BSS_HI = 0x26495EC;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t FLASH_BYTES = 0x3C;


	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x828BA0) rewrites them in the actor data, which every actor system of
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
	// bytes of a model record from its start to the end of its last primitive list (the lists
	// 0x828840 = engine 0x7340A0 walks)
	static uint32_t ModelBytes(uint32_t a1)
	{
		uint32_t p = a1 + (uint32_t)(S32(a1, 0) / 4) * 4;
		p += U32(p, 0) * 12 + 4;
		p += U32(p, 0) * 12 + 4;
		int32_t c = S32(p, 0);
		p += 4 + (c > 0 ? (uint32_t)c * 0x14 : 0);
		c = S32(p, 0);
		p += 4 + (c > 0 ? (uint32_t)c * 0x18 : 0);
		p += U32(p, 0) * 20 + 4;
		p += U32(p, 0) * 24 + 4;
		c = S32(p, 0);
		p += 4 + (c > 0 ? (uint32_t)c * 0x1C : 0);
		c = S32(p, 0);
		p += 4 + (c > 0 ? (uint32_t)c * 0x24 : 0);
		return p - a1;
	}

	// the model records of an actor system (state +0x22C table, 16 entries) as a draw used them: the
	// lava stream's texture scroll rewrites a record's UVs on every update of an actor using it, and
	// the records are shared by every actor system of the cast, so a later system's update changes
	// what an earlier one drew with (as VertexBlocks for the morph)
	struct ModelBlocks
	{
		uint32_t n;
		uint32_t addr[16], len[16];
		uint8_t bytes[0x10000];
		void save(uint32_t sys)
		{
			n = 0;
			const uint32_t tab = U32(sys, 0x22C);
			if (!tab) return;
			uint32_t used = 0;
			for (int k = 0; k < 16; k++)
			{
				const uint32_t m = U32(tab, k * 4);
				if (!m) continue;
				bool dup = false;
				for (uint32_t q = 0; q < n && !dup; q++) dup = addr[q] == m;
				if (dup) continue;
				const uint32_t bytes_n = ModelBytes(m);
				if (bytes_n == 0 || used + bytes_n > sizeof(bytes)) continue;
				addr[n] = m;
				len[n] = bytes_n;
				memcpy(bytes + used, (const void *)m, bytes_n);
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

	struct FlashNote
	{
		uint32_t node;
		uint8_t bytes[FLASH_BYTES]; // the node after its tick (colour +0x34 = the drawn one)
	};
	enum { EV_SYS, EV_FLASH };
	struct Event { int kind, i; };

	static const int MAX_SYS = 8, MAX_FLASH = 8, MAX_EV = MAX_SYS + MAX_FLASH;
	static SysNote g_sys[MAX_SYS];
	static FlashNote g_flash[MAX_FLASH];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nflash = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint8_t g_held_packets[0x40000];
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw
	static bool g_predicting = false; // the next tick's update pass runs on copies
	static ModelBlocks g_models[MAX_SYS]; // per noted system: its model records as it drew them
	static int g_scroll_at[MAX_SYS];      // per noted system: scroll events of its tick before its draws

	// the lava stream's texture scroll (0x828070, -8 V on the actor's model record per update of a
	// definition-0 actor): the real ticks log them (model, in order); the prediction notes the next
	// tick's instead of doing them (a side effect on the shared records); DrawSystem replays, from
	// the system's noted records, the scrolls between its draw on the real tick and its draw on the
	// next one (later systems' of the real tick, then the predicted ones up to this system) by the
	// fraction of -8 each, and the records are put back after the held frame
	static const int MAX_SCROLL = 4 * MAX_ACTORS;
	static uint32_t g_real_scroll[MAX_SCROLL];
	static int g_nreal_scroll = 0;
	static uint32_t g_real_scroll_tick = 0xFFFFFFFF;
	static uint32_t g_scroll[MAX_SCROLL]; // predicted (next tick), in the held frame's system order
	static int g_nscroll = 0;
	static bool held_defer_scroll(uint32_t model)
	{
		if (g_predicting)
		{
			if (g_nscroll < MAX_SCROLL) g_scroll[g_nscroll++] = model;
			return true;
		}
		if (g_real_scroll_tick != g_real_tick) { g_real_scroll_tick = g_real_tick; g_nreal_scroll = 0; }
		if (g_nreal_scroll < MAX_SCROLL) g_real_scroll[g_nreal_scroll++] = model;
		return false;
	}

	static void held_begin_tick()
	{
		if (g_note_tick == g_real_tick) return;
		g_note_tick = g_real_tick;
		g_nsys = g_nflash = g_nev = 0;
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
		g_models[g_nsys].save(sys);
		g_scroll_at[g_nsys] = g_real_scroll_tick == g_real_tick ? g_nreal_scroll : 0;
		g_ev[g_nev++] = { EV_SYS, g_nsys++ };
	}

	// real tick: a screen flash is about to draw its quad (colour +0x34, state after its step)
	static void held_note_flash(uint32_t node)
	{
		held_begin_tick();
		if (g_nflash >= MAX_FLASH || g_nev >= MAX_EV) return;
		FlashNote &n = g_flash[g_nflash];
		n.node = node;
		memcpy(n.bytes, (const void *)node, FLASH_BYTES);
		g_ev[g_nev++] = { EV_FLASH, g_nflash++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// ---- screen flashes

	// the colour the flash draws on the next tick: the script step of 0x825220 on the node as its
	// tick left it (frame counter +0x24 already advanced); false when it does not draw then
	static bool FlashNext(uint32_t node, uint32_t &colour)
	{
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF) return false;
		colour = U32(node, 0x34);
		if ((hi >> 8) == 0xFE)
		{
			if (S16(node, 0x24) >= (int16_t)(hi & 0xFF))
				colour = U32(script, (int32_t)(int16_t)(uint16_t)(idx + 1) * 4);
		}
		else
			colour = entry;
		return true;
	}

	static void DrawFlash(const FlashNote &n, int num, int den)
	{
		uint32_t col = U32(P(n.bytes), 0x34), next = 0;
		const bool drawn = live(n.node, ORIG_Flash) && FlashNext(P(n.bytes), next);
		if (!drawn)
		{
			if (num >= den) return;
		}
		else if (num >= den)
			col = next;
		else
		{
			uint32_t c = col & 0xFF000000;
			for (int k = 0; k < 3; k++)
			{
				const int32_t a = (int32_t)((col >> (8 * k)) & 0xFF), b = (int32_t)((next >> (8 * k)) & 0xFF);
				c |= (uint32_t)(lerp_i(a, b, num, den) & 0xFF) << (8 * k);
			}
			col = c;
		}
		FlashDraw(col);
	}

	// ---- actor systems (as mag030_counter_laser_eye_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x8277D0 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor (flipbook frame, variant, fade, colour). P_ENDS: the actor ends on
	// the next tick (not drawn there); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7419C0 (= 0x828CB0) (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		g_predicting = true; // (0x828070 notes its texture scroll instead: held_defer_scroll)
		mb_828070(nxt, def); // (a morphing model's step rewrites its vertex block: put back by the caller)
		g_predicting = false;
		// a model-frame step (the lava stream's mesh +0x170 changes on the next tick): no mesh lies in
		// between, the actor holds (the next tick's at fraction 1)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x8277D0)
		}
		return P_NEXT;
	}

	// the actors of a noted system redrawn half way (see mag029_wind_blast_held.inc and mag030_counter_laser_eye_held.inc)
	static void DrawSystem(int si, int num, int den)
	{
		const SysNote &n = g_sys[si];
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(n.verts.bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.sys;
		g_models[si].restore(); // the model records as this system drew them
		// the drawn actors: sprite sequences (definition kinds 0..2) then models (4 / 5), as
		// 0x827650 / 0x825FD0 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x827790) in list order: the emitters' placement / motion
		// (0x828E00 without its emission and life count, applied to the emitters themselves and put
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
					mb_829900(act);
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
		// (the next tick's morph steps 0x828BA0 in list order, as its update pass writes them)
		if (morph)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					mb_828BA0(P(nxts[i]), DefOf(list[i]));
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
		// the lava stream's scrolls between the system's draw on the real tick and on the next tick
		// (the real tick's after its draw, then the predicted ones up to this system), each by the
		// fraction of its -8, on the system's noted records
		const int32_t shift = -8 * num / den;
		if (shift != 0)
		{
			if (g_real_scroll_tick == g_real_tick)
				for (int k = g_scroll_at[si]; k < g_nreal_scroll; k++) a_7340A0(g_real_scroll[k], (uint32_t)shift);
			for (int k = 0; k < g_nscroll; k++) a_7340A0(g_scroll[k], (uint32_t)shift);
		}
		// the draws start from the real tick's GTE state and scratch
		n.st.restore();
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				mb_827710(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					mb_827710(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				mb_826030(P(copies[i]), def);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_048;
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
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		static ModelBlocks models_now[MAX_SYS]; // the model records as the real tick left them
		for (int s = 0; s < g_nsys; s++) models_now[s].save(g_sys[s].sys);
		g_nscroll = 0;
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(ev.i, num, den);
			else
				DrawFlash(g_flash[ev.i], num, den);
		}
		for (int s = g_nsys - 1; s >= 0; s--) models_now[s].restore();
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
	static void register_mag048_held()
	{
		mark_held(act::magmabreath::ORIG_Flash);
		mark_held(act::magmabreath::ORIG_ActorSystem);
		register_module_held(48, act::magmabreath::HeldReady, act::magmabreath::HeldFrame);
	}
}
