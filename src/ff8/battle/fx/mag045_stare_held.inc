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

// 30 fps part of mag045_stare.cpp: the held-frame drawing of the prim-model tasks (the two eye stares
// and the target's mark) (included at the end of mag045_stare.cpp)
//
// Held frames (30 fps). The prim-model tasks update THEN draw in the same tick (0x836EB0: the
// prim-model player at node +0x94). The real tick notes, in drawing order, every play with the node
// as it was drawn and the state the draw started from (GTE registers, Field_Alloc pointer and the
// scratch above it: the render header's fields the callback does not set are that scratch); the held
// frame replays them in the same order with prim::play_held (the exact in-between records of what the
// real play drew; a play the next tick does not make holds its last records) and the parameter block
// rebuilt from the node's placement and the current camera (the eye stares follow the caster's
// points, read from its bones once per tick: the placement holds). The view-axis matrix 0x2655758 is
// rebuilt from the current camera first, as the master does before the prim-model queue.
// The stage darkening (0x839690: the stage group words 0x1D98992 + k * 0x2C, a ramp read by the
// stage renderer, not drawn here) holds.
// The packets go to a private buffer (frame arena cursor 0x1D8E054) that starts with the real tick's
// packets; the module globals (blend buffer, view-axis matrix), the Field_Alloc scratch and the GTE
// registers are put back.

namespace ff8fx
{
namespace act
{
namespace stare
{
	static const uint32_t BSS_LO = 0x26543B0, BSS_HI = 0x2655778;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t PRIM_BYTES = 0x2D0;
	static const int MAX_PLAY = 8;

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
	struct PlayNote
	{
		uint32_t node;
		uint8_t bytes[PRIM_BYTES]; // the node as the real tick drew it
		DrawState st;
	};

	static PlayNote g_play[MAX_PLAY];
	static int g_nplay = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_frame_start = 0; // frame arena cursor at the tick's start
	static uint8_t g_held_frame[0x40000];

	// real tick: the master starts (before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nplay = 0;
		g_frame_start = MEM<uint32_t>(FRAME_CURSOR);
	}

	// real tick: a prim-model task is about to play its model (0x836EB0; its placement set)
	static void held_note_play(uint32_t node)
	{
		if (g_note_tick != g_real_tick || g_nplay >= MAX_PLAY) return;
		PlayNote &n = g_play[g_nplay++];
		n.node = node;
		memcpy(n.bytes, (const void *)node, PRIM_BYTES);
		n.st.save();
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// the parameter block of 0x836EB0 for the node n (L: 0x5C bytes)
	static void PlayBlock(uint32_t n, uint32_t L)
	{
		x::MAG_022_sub_8DD770(L);
		if (S16(n, 0x62) != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)S16(n, 0x62));
		if (S16(n, 0x60) != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)S16(n, 0x60));
		if (S16(n, 0x64) != 0) x::sub_8DD960(L, (uint32_t)(int32_t)S16(n, 0x64));
		x::scale3DMatrix(L, n + 0x50);
		S32(L, 0x14) = S16(n, 0x1C);
		S32(L, 0x18) = S16(n, 0x1E);
		S32(L, 0x1C) = S16(n, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(n, 0x86);
		U32(L, 0x44) = U32(n, 0x7C);
		U32(L, 0x38) = U32(n, 0x70);
		U16(L, 0x4C) = U16(n, 0x8C);
		U16(L, 0x52) = U16(n, 0x92);
		U16(L, 0x4E) = U16(n, 0x8E);
		U16(L, 0x50) = U16(n, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(n, 0x80);
		U16(L, 0x58) = U16(n, 0x88);
	}

	static void DrawPlay(const PlayNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[PRIM_BYTES];
		memcpy(cur, n.bytes, PRIM_BYTES);
		alignas(4) static uint8_t fr[0x5C];
		memset(fr, 0, sizeof(fr));
		const uint32_t L = P(fr);
		n.st.restore();
		PlayBlock(P(cur), L);
		void *p = port_of(PRIM_CB);
		prim::play_held((prim::Layout *)(n.node + 0x94), (prim::Callback)(p ? P(p) : PRIM_CB), (int)L, num, den);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_045;
		static uint8_t bss[BSS_HI - BSS_LO];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		now.save();
		const uint32_t frame_cursor = MEM<uint32_t>(FRAME_CURSOR);
		// the Field_Alloc scratch the replayed draws may write: the windows at the noted pointers
		// and at the current one
		static const int MAX_WIN = MAX_PLAY + 1;
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
		for (int e = 0; e < g_nplay; e++)
			add_win(g_play[e].st.fa);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them
		const uint32_t used = frame_cursor - g_frame_start;
		if (g_frame_start && used < sizeof(g_held_frame)) memcpy(g_held_frame, (const void *)g_frame_start, used);
		MEM<uint32_t>(FRAME_CURSOR) = P(g_held_frame);
		st_836840(0x1D97778, VIEW_MATRIX);
		for (int e = 0; e < g_nplay; e++)
			DrawPlay(g_play[e], num, den);
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag045_held()
	{
		mark_held(act::stare::ORIG_Eye);
		mark_held(act::stare::ORIG_Mark);
		register_module_held(45, act::stare::HeldReady, act::stare::HeldFrame);
	}
}
