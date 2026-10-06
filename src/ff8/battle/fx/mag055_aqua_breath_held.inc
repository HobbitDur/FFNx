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

// 30 fps part of mag055_aqua_breath.cpp: the held-frame drawing of the orb and of the drops (included
// at the end of mag055_aqua_breath.cpp)
//
// Held frames (30 fps). Every drawing task of the module draws in the same tick as its update; the
// real tick notes, in drawing order, every draw with the node and the state it started from (GTE
// registers, Field_Alloc scratch):
//   - the orb (task 0x809250, updates then draws): its next tick (state step on a copy of the node):
//     position and texture scroll +0x5E half way, its model rewritten (0x80B490 run into the model and
//     put back) at the in-between wave / turn angles (+0x54 / +0x58, 0xA0 per tick); the fade +0x50 holds (it is the renderer's far colour word
//     (+0xC), a lerp would carry into its low byte); at fraction 1 the next tick's orb; an orb that
//     ends holds.
//   - the stream's drops (task 0x80B8E0, four sprites, update then draw): each drop half way to its
//     next tick's position (the state step, the fall, its spin and distance steps and its placement
//     run on a copy); the sprite frame holds (the next tick's at fraction 1); a drop the next tick
//     does not draw (its task ended or hidden) holds.
//   - the splash / rain drops (tasks 0x80C430 / 0x80C7B0, two / four sprites, draw then update):
//     each drop half way from its drawn position to the one its update left for the next tick; the
//     sprite frame holds (the next tick's at fraction 1); a drop the next tick does not draw holds.
// Drop tasks born on the next tick are 15 Hz growth (not drawn before their first real tick).
// The packets go to a private frame arena (cursor 0x1D8E054, start taken at the tick's first draw);
// the module globals, the camera copy, the Field_Alloc scratch, the sprite camera words, the orb
// model's vertices and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace aqua
{
	static const uint32_t BSS_LO = 0x2610FA8, BSS_HI = 0x2616420;
	static const uint32_t SCRATCH_LO = 0x2793E58, SCRATCH_HI = 0x2793E78; // camera copy
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t FA_CURSOR = 0x1D8E054; // frame arena cursor (every draw of the module)
	static const uint32_t ORB_BYTES = 0x60, DROP_BYTES = 0x154, ORB_VERTS_MAX = 0x4000;

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
	// the orb before its draw (model rewritten): the node, the draw state and the model's vertices
	struct OrbNote
	{
		uint32_t node;
		DrawState st;
		alignas(4) uint8_t bytes[ORB_BYTES];
		uint32_t nverts;
		alignas(4) uint8_t verts[ORB_VERTS_MAX];
	};
	// a drop before its draw: the task node (its position +0x1C and sprite +0x4C set), the drop
	// index and the draw state
	struct DropNote
	{
		uint32_t node;
		int k;
		DrawState st;
		alignas(4) uint8_t bytes[DROP_BYTES];
	};
	enum { EV_ORB, EV_DROP };
	struct Event { int kind, i; };

	static const int MAX_ORB = 4, MAX_DROPS = 400, MAX_EV = MAX_ORB + MAX_DROPS;
	static OrbNote g_orb[MAX_ORB];
	static DropNote g_drop[MAX_DROPS];
	static Event g_ev[MAX_EV];
	static int g_norb = 0, g_ndrop = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint8_t g_held_fa[0x40000];
	static uint32_t g_fa_start = 0; // frame arena cursor at the tick's first draw

	static bool g_tick_open = false; // the master ran: the next note starts the tick's list
	static void held_begin_tick()
	{
		if (g_note_tick == g_real_tick && !g_tick_open) return;
		g_note_tick = g_real_tick;
		g_tick_open = false;
		g_norb = g_ndrop = g_nev = 0;
		g_fa_start = MEM<uint32_t>(FA_CURSOR);
	}
	// real tick: the master runs (before its queues)
	static void held_note_tick()
	{
		held_note_master();
		g_tick_open = true;
	}

	// real tick: the orb is about to draw (its model rewritten by 0x80B490)
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

	// real tick: drop k of a drop task is about to draw
	static void held_note_drop(uint32_t node, int k)
	{
		held_begin_tick();
		if (g_ndrop >= MAX_DROPS || g_nev >= MAX_EV) return;
		DropNote &n = g_drop[g_ndrop];
		n.node = node;
		n.k = k;
		n.st.save();
		memcpy(n.bytes, (const void *)node, DROP_BYTES);
		g_ev[g_nev++] = { EV_DROP, g_ndrop++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// ---- the orb (task 0x809250)

	static void DrawOrb(const OrbNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[ORB_BYTES], nxt[ORB_BYTES];
		memcpy(cur, n.bytes, ORB_BYTES);
		const uint32_t c = P(cur);
		const uint32_t model = U32(c, 0x4C);
		bool next = false;
		if (live(n.node, ORIG_Orb) && n.nverts != 0)
		{
			// the next tick's state step (the states only write the node)
			memcpy(nxt, (const void *)n.node, ORB_BYTES);
			static const uint32_t states[5] = { 0x80B580, 0x80B680, 0x80B6C0, 0x80B6F0, 0x80B720 };
			const int32_t st = S8(P(nxt), 0x29);
			if (st >= 0 && st < 5) callp(states[st], P(nxt));
			next = (U8(P(nxt), 0x26) & 4) == 0 && U32(P(nxt), 0x4C) == model;
		}
		if (next)
		{
			// the model rewritten (0x80B490 into the model, put back below) at the in-between angles: the
			// wave +0x54 and the turn +0x58 step by 0xA0 per tick (exact shape, not a vertex lerp)
			const uint32_t a = (uint32_t)(U16(c, 0x54) + 0xA0 * num / den) & 0xFFF;
			const uint32_t b = (uint32_t)(U16(c, 0x58) + 0xA0 * num / den) & 0xFFF;
			ab_80B490(ORB_TEMPLATE, model, a, b);
			if (num >= den)
			{
				memcpy(cur, nxt, ORB_BYTES);
				U16(c, 0x54) = (uint16_t)a;
				U16(c, 0x58) = (uint16_t)b;
				U16(c, 0x5E) = (uint16_t)((uint8_t)(U8(c, 0x5E) + 8) & 0x7F);
			}
			else
			{
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(P(nxt), 0x1C + 2 * k), num, den);
				U16(c, 0x5E) = (uint16_t)((U16(c, 0x5E) + 8 * num / den) & 0x7F);
			}
		}
		else
			memcpy((void *)(model + 8), n.verts, n.nverts);
		n.st.restore();
		ab_809300(c);
		memcpy((void *)(model + 8), n.verts, n.nverts); // the model as the real tick left it
	}

	// ---- the drops (tasks 0x80B8E0, 0x80C430, 0x80C7B0)

	// the next tick's draw of drop k of the drop task `node`: its position (and the node with the
	// state step: sprite frame / hidden); false when the next tick does not draw it
	static bool DropNext(uint32_t node, int k, uint32_t nxt, int16_t pos[3])
	{
		const uint32_t task = U32(node, 8);
		memcpy((void *)nxt, (const void *)node, DROP_BYTES);
		const int32_t st = S8(nxt, 0x29);
		if (task == ORIG_Drops)
		{
			static const uint32_t states[3] = { 0x80C100, 0x80C140, 0x80C190 };
			if (st >= 0 && st < 3) callp(states[st], nxt);
			// 0x80B8E0's placement of drop k (on the copy)
			U16(nxt, 0x150) = (uint16_t)(U16(nxt, 0x150) + 0x10);
			const uint32_t spin = nxt + 0x148 + 2 * k;
			const uint16_t s = (uint16_t)((U16(spin, 0) + 0x100) & 0xFFF);
			U16(spin, 0) = s;
			U16(spin, -8) = (uint16_t)(U16(spin, -8) + U16(nxt, 0x152));
			alignas(4) uint16_t v[4] = { 0, U16(nxt, 0x150), U16(spin, -8), 0 };
			alignas(4) uint8_t m[0x20];
			alignas(4) uint16_t out[4] = { 0, 0, 0, 0 };
			memcpy(m, (const void *)(nxt + 0xC0 + 0x20 * k), 0x20);
			x::sub_8DD960(P(m), (uint32_t)(int32_t)(int16_t)s);
			x::GTE_SetRotMatrix(P(m));
			x::GTE_LoadV0(P(v));
			x::GTE_MVMVA_RotV0();
			x::GTE_StoreIR123(P(out));
			pos[0] = (int16_t)(out[0] + U16(nxt, 0x78));
			pos[1] = (int16_t)(out[1] + U16(nxt, 0x7A));
			pos[2] = (int16_t)(out[2] + U16(nxt, 0x7C));
		}
		else
		{
			static const uint32_t splash[3] = { 0x80C540, 0x80C560, 0x80C580 };
			static const uint32_t rain[3] = { 0x80C8C0, 0x80C8E0, 0x80C900 };
			if (st >= 0 && st < 3) callp(task == ORIG_SplashDrops ? splash[st] : rain[st], nxt);
			// drawn from the position its update left (draw then update)
			for (int c = 0; c < 3; c++) pos[c] = S16(nxt, 0x80 + 8 * k + 2 * c);
		}
		return (U8(nxt, 0x26) & 4) == 0;
	}

	static void DrawDrop(const DropNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[DROP_BYTES], nxt[DROP_BYTES];
		memcpy(cur, n.bytes, DROP_BYTES);
		const uint32_t c = P(cur);
		const uint32_t task = U32(c, 8);
		int16_t pos[3];
		if (live(n.node, task) && DropNext(n.node, n.k, P(nxt), pos))
		{
			if (num >= den)
				U16(c, 0x50) = U16(P(nxt), 0x50);
			for (int k = 0; k < 3; k++)
				S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), pos[k], num, den);
		}
		n.st.restore();
		ab_80BA30(c);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_055;
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
		for (int s = 0; s < g_norb; s++) add_win(g_orb[s].st.fa);
		for (int s = 0; s < g_ndrop; s++) add_win(g_drop[s].st.fa);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them
		const uint32_t fc = MEM<uint32_t>(FA_CURSOR);
		const uint32_t used = fc - g_fa_start;
		if (g_fa_start && fc > g_fa_start && used < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_ORB)
				DrawOrb(g_orb[ev.i], num, den);
			else
				DrawDrop(g_drop[ev.i], num, den);
		}
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
	static void register_mag055_held()
	{
		mark_held(act::aqua::ORIG_Orb);
		mark_held(act::aqua::ORIG_Drops);
		mark_held(act::aqua::ORIG_SplashDrops);
		mark_held(act::aqua::ORIG_RainDrops);
		register_module_held(55, act::aqua::HeldReady, act::aqua::HeldFrame);
	}
}
