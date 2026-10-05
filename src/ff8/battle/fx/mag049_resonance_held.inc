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

// 30 fps part of mag049_resonance.cpp: the held-frame drawing of the screen warp and of the two prim
// models (included at the end of mag049_resonance.cpp)
//
// Held frames (30 fps). Both drawing tasks update THEN draw in the same tick.
//   Screen warp (0x824260) - the wave tables are stepped by random phase steps (0x8246D0) before the
//     draw: the next tick's waves cannot be known without the RNG, so the held frame redraws the
//     real tick's grid (the waves stay 15 Hz, like random growth): the two quad passes of
//     0x824520 / 0x8245B0 from the unchanged wave tables, and the two draw environments of the
//     buffer being drawn now (0x824410's packets built in private copies: the module's packets stay
//     linked into the real frame's OT).
//   Prim models (0x821C50 / 0x824140) - the real tick notes, in drawing order, every play with the
//     node as it was drawn and the state the draw started from (GTE registers, Field_Alloc pointer and
//     the scratch above it: the render header's fields the callback does not set are that scratch);
//     the held frame replays them with prim::play_held (the exact in-between records of what the real
//     play drew; a play the next tick does not make holds its last records) and the parameter block
//     rebuilt from the node's placement and the current camera (the placement is fixed once the
//     model started).
// The quads go to a private frame arena buffer (cursor 0x1D8E054) that starts with the real tick's
// warp packets (the quads' colour bytes and CLUT word are never written), the prim packets to a
// private module arena buffer (cursor 0x26464DC) that starts with the real tick's module arena from
// the cursor of the tick's first play (the master picks the arena by tick parity after the tick
// started); the module globals, the Field_Alloc scratch and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace resonance
{
	static const uint32_t BSS_LO = 0x26464D8, BSS_HI = 0x26480F0;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t PRIM_BYTES = 0x368;
	static const uint32_t WARP_PACKETS = 2 * 10 * 7 * 0x34;  // the two passes of 0x8245B0
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
	static uint32_t g_arena_start = 0;   // module packet cursor at the tick's first play
	static uint32_t g_warp_node = 0;     // the warp drew on this tick (its node)
	static uint32_t g_warp_start = 0;    // frame arena cursor at the warp's draw
	static uint8_t g_held_frame[WARP_PACKETS + 0x100];
	static uint8_t g_held_arena[0xC000];
	static uint8_t g_env[2][0x5C];       // private draw environments (scene, back) of the held frame
	static uint8_t g_env_pkt[2][0x40];   // and their packets

	// real tick: the master starts (before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nplay = 0;
		g_arena_start = 0;
		g_warp_node = 0;
	}

	// real tick: a prim-model task is about to play its model (0x821D40; its placement set)
	static void held_note_play(uint32_t node)
	{
		if (g_note_tick != g_real_tick || g_nplay >= MAX_PLAY) return;
		if (g_nplay == 0) g_arena_start = MEM<uint32_t>(PACKET_CURSOR);
		PlayNote &n = g_play[g_nplay++];
		n.node = node;
		memcpy(n.bytes, (const void *)node, PRIM_BYTES);
		n.st.save();
	}

	// real tick: the warp is about to step its waves and draw (0x8244E0)
	static void held_note_warp(uint32_t node)
	{
		if (g_note_tick != g_real_tick) return;
		g_warp_node = node;
		g_warp_start = MEM<uint32_t>(FRAME_CURSOR);
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// the parameter block of 0x821D40 for the node n (L: 0x5C bytes)
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

	// the warp of the real tick (held: its grid, see above): the quads of both passes (0x8244E0
	// without the wave step) then the draw environments of the buffer drawn now (0x824410 into the
	// private copies)
	static void DrawWarp()
	{
		const uint32_t node = g_warp_node;
		rs_824520(1);
		rs_8245B0(node, 5, 1);
		rs_824520(0);
		rs_8245B0(node, 5, 0);
		const uint32_t k = (uint32_t)(MEM<uint8_t>(0x1D96A80) - 1) & 1;
		const uint32_t env_disp = 0x1D969C8 + k * 0x5C;
		const uint32_t env_back = P(g_env[1]), env_scene = P(g_env[0]);
		memcpy(g_env[1], (const void *)env_disp, 0x5C);
		memcpy(g_env[0], (const void *)env_disp, 0x5C);
		U16(env_scene, 0) = 0x240;
		U16(env_scene, 2) = 0x100;
		U16(env_scene, 4) = 0x140;
		U16(env_scene, 6) = 0xF0;
		U16(env_scene, 8) = 0x240;
		U16(env_scene, 0xA) = 0x100;
		U8(env_scene, 0x18) = 1;
		U8(env_scene, 0x19) = 0;
		U8(env_scene, 0x1A) = 0;
		U8(env_scene, 0x1B) = 0;
		U8(env_back, 0x18) = 0;
		rx::SetDrawEnv(P(g_env_pkt[0]), env_scene);
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x4484, P(g_env_pkt[0]));
		rx::SetDrawEnv(P(g_env_pkt[1]), env_back);
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x14, P(g_env_pkt[1]));
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_049;
		static uint8_t bss[BSS_HI - BSS_LO];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		now.save();
		const uint32_t frame_cursor = MEM<uint32_t>(FRAME_CURSOR);
		const uint32_t arena_cursor = MEM<uint32_t>(PACKET_CURSOR);
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
		// the buffers get the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them
		if (g_warp_node)
		{
			if (g_warp_start) memcpy(g_held_frame, (const void *)g_warp_start, WARP_PACKETS);
			MEM<uint32_t>(FRAME_CURSOR) = P(g_held_frame);
			DrawWarp();
		}
		if (g_nplay)
		{
			const uint32_t used = arena_cursor - g_arena_start;
			if (g_arena_start && used < sizeof(g_held_arena)) memcpy(g_held_arena, (const void *)g_arena_start, used);
			MEM<uint32_t>(PACKET_CURSOR) = P(g_held_arena);
			for (int e = 0; e < g_nplay; e++)
				DrawPlay(g_play[e], num, den);
		}
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag049_held()
	{
		mark_held(act::resonance::ORIG_Prim1);
		mark_held(act::resonance::ORIG_Prim2);
		mark_held(act::resonance::ORIG_Warp);
		register_module_held(49, act::resonance::HeldReady, act::resonance::HeldFrame);
	}
}
