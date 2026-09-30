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

// 30 fps part of mag020_norg_pod_opening.cpp: the held-frame drawing of the pod lid, the debris, the
// chunks and the screen tint (included at the end of mag020_norg_pod_opening.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the node as drawn and the state it started from
// (GTE registers, Field_Alloc scratch); the held frame redraws them in place (the nodes are put
// back):
//   - the lid (task 0x8A0070, drawn by 0x8A2D90): in flight (state 2) its offset +0x58..+0x5C half
//     way to the next tick's and its angles +0x68 / +0x6C half way along the shortest arc (the next
//     tick's flight step 0x8A2FE0 without its end calls); before the flight and on its last tick it
//     holds. The caster's pose holds (engine animation).
//   - debris (task 0x8A01B0, sprite sequences 0x8A0230): they do not move; the flipbook frame holds
//     (the next tick's at fraction 1, where one hidden by its last frame draws nothing).
//   - chunks (task 0x8A0990, two prim models each, 0x8A0AA0): piece positions, spin +0x48 and scale
//     +0x30..+0x38 half way to the next tick's (its state step 0x8A2C60 and motion run on a copy);
//     a chunk ending after the real tick holds.
//   - the screen tint (engine task 0x8DDC30 in the stage queue, fixed packets 0x2795110): its colour
//     half way to the next tick's script colour, drawn into a private copy; a tint the real tick did
//     not draw is not drawn, one the next tick does not draw holds.
// A part the next tick no longer draws (released lid / debris / chunk, ended tint) holds before
// fraction 1 and is not drawn at fraction 1, so a frame at fraction 1 is the next tick's.
// Debris and chunks spawned on the next tick are not drawn (random growth stays at 15 Hz). The
// stage light words (0x1D98992 + k * 0x2C, a 0x100-per-tick ramp read by the stage renderer, not
// drawn here) hold.
// The packets go to private buffers (frame arena cursor 0x1D8E054, module cursor 0x2721594) that
// start with the real tick's packets; the module globals, the entities, the Field_Alloc scratch,
// the shadow / geometry temporaries, RenderGeometry's scratch below the end of the battle model
// memory ([0xB6D080] - 0x4000..) and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace norgpod
{
	static const uint32_t BSS_LO = 0x2721590, BSS_HI = 0x2726900;
	static const uint32_t ENT_LO = 0x1D972C0, ENT_HI = 0x1D972C0 + 7 * 0x9C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t FRAME_CURSOR = 0x1D8E054; // frame arena cursor (the lid)
	static const uint32_t TINT_PKT = 0x2795110, TINT_PKT_N = 0x6C; // the tint task's fixed packets
	static const uint32_t LID_BYTES = 0x78, DEBRIS_BYTES = 0x78, CHUNK_BYTES = 0x98;
	static const uint32_t MODEL_END = 0xB6D080, MODEL_TOP_N = 0x4000; // RenderGeometry's scratch below the end of the battle model memory

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

	struct NodeNote
	{
		uint32_t node;
		uint8_t bytes[CHUNK_BYTES]; // (the lid / debris use their first 0x78 bytes)
		DrawState st;
	};
	enum { EV_LID, EV_DEBRIS, EV_CHUNK };
	struct Event { int kind, i; };

	static const int MAX_LID = 4, MAX_DEBRIS = 40, MAX_CHUNK = 100, MAX_EV = MAX_LID + MAX_DEBRIS + MAX_CHUNK;
	static const int MAX_TINTS = 10;
	static NodeNote g_lid[MAX_LID], g_debris[MAX_DEBRIS], g_chunk[MAX_CHUNK];
	static Event g_ev[MAX_EV];
	static int g_nlid = 0, g_ndebris = 0, g_nchunk = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static bool g_in_held = false;                       // the held frame's own draws are not noted
	static uint32_t g_frame_start = 0, g_pkt_start = 0; // frame arena / module cursors at the tick's start
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_packets[0x10000];
	// the screen tints (engine task 0x8DDC30 in the stage queue): their nodes before the queue runs
	static uint8_t g_tint[MAX_TINTS][0x40];
	static uint32_t g_tint_addr[MAX_TINTS];
	static int g_ntint = 0;
	static uint8_t g_tint_real[TINT_PKT_N];
	alignas(4) static uint8_t g_tint_pkt[TINT_PKT_N];

	// real tick: the master starts (its packet arena chosen, before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nlid = g_ndebris = g_nchunk = g_nev = 0;
		g_frame_start = MEM<uint32_t>(FRAME_CURSOR);
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		g_ntint = 0;
		for (uint32_t node = MEM<uint32_t>(Q_STAGE); node && g_ntint < MAX_TINTS; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30) continue;
			memcpy(g_tint[g_ntint], (const void *)node, 0x40);
			g_tint_addr[g_ntint++] = node;
		}
	}

	static void note(int kind, NodeNote *arr, int &cnt, int max, uint32_t node, uint32_t bytes)
	{
		if (g_in_held || g_note_tick != g_real_tick || cnt >= max || g_nev >= MAX_EV) return;
		NodeNote &n = arr[cnt];
		n.node = node;
		memcpy(n.bytes, (const void *)node, bytes);
		n.st.save();
		g_ev[g_nev++] = { kind, cnt++ };
	}
	// real tick: the lid is about to be drawn (0x8A2D90, after its state step)
	static void held_note_lid(uint32_t node) { note(EV_LID, g_lid, g_nlid, MAX_LID, node, LID_BYTES); }
	// real tick: a debris sprite is about to be drawn (0x8A0230; a hidden one draws nothing)
	static void held_note_debris(uint32_t node) { note(EV_DEBRIS, g_debris, g_ndebris, MAX_DEBRIS, node, DEBRIS_BYTES); }
	// real tick: a chunk is about to draw its two pieces (after its state step and motion)
	static void held_note_chunk(uint32_t node) { note(EV_CHUNK, g_chunk, g_nchunk, MAX_CHUNK, node, CHUNK_BYTES); }

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}
	// a u16 angle word half way along the shortest arc of its 16 bits (the next word at fraction 1)
	static uint16_t lerp_w(uint16_t a, uint16_t b, int num, int den)
	{
		if (num >= den) return b;
		return (uint16_t)((int32_t)a + (int32_t)(int16_t)(uint16_t)(b - a) * num / den);
	}
	// a 12-bit angle (the word stays in 0..0xFFF) half way along the shortest arc
	static uint16_t lerp_a12(uint16_t a, uint16_t b, int num, int den)
	{
		if (num >= den) return b;
		int32_t d = ((int32_t)b - (int32_t)a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		return (uint16_t)(((int32_t)a + d * num / den) & 0xFFF);
	}

	// ---- the lid

	// the flight step of 0x8A2FE0 on node b (without its end calls and the draw)
	static void LidFlight(uint32_t b)
	{
		const uint16_t ax0 = U16(b, 0x68);
		U16(b, 0x62) = (uint16_t)(U16(b, 0x62) + 0x28);
		const uint16_t cz0 = U16(b, 0x6C);
		U16(b, 0x58) = (uint16_t)(U16(b, 0x58) + U16(b, 0x60));
		U16(b, 0x68) = (uint16_t)((uint16_t)(ax0 - 0x40) & 0xFFF);
		U16(b, 0x5A) = (uint16_t)(U16(b, 0x5A) + U16(b, 0x62));
		U16(b, 0x5C) = (uint16_t)(U16(b, 0x5C) + U16(b, 0x64));
		U16(b, 0x70) = (uint16_t)(U16(b, 0x70) - 1);
		U16(b, 0x6C) = (uint16_t)((uint16_t)(cz0 + 0x30) & 0xFFF);
	}

	static void DrawLid(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t nxt[LID_BYTES];
		const uint32_t node = n.node, x1 = P(nxt);
		// (a lid finishing on the real tick is released: not drawn at fraction 1)
		if (num >= den && ((U8(P(n.bytes), 0x26) & 1) != 0 || !live(node, ORIG_Lid))) return;
		// the node as drawn (in place: the module globals are put back after the held frame)
		memcpy((void *)node, n.bytes, LID_BYTES);
		// in flight and not on its last tick: the next tick's flight step on the node as the real tick left it
		if (S8(P(n.bytes), 0x29) == 2 && (U8(P(n.bytes), 0x26) & 1) == 0 && live(node, ORIG_Lid))
		{
			memcpy(nxt, n.bytes, LID_BYTES);
			LidFlight(x1);
			for (int k = 0; k < 3; k++)
				S16(node, 0x58 + 2 * k) = (int16_t)lerp_i(S16(node, 0x58 + 2 * k), S16(x1, 0x58 + 2 * k), num, den);
			U16(node, 0x68) = lerp_a12(U16(node, 0x68), U16(x1, 0x68), num, den);
			U16(node, 0x6C) = lerp_a12(U16(node, 0x6C), U16(x1, 0x6C), num, den);
		}
		n.st.restore();
		np_8A2D90(node);
	}

	// ---- debris

	static void DrawDebris(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t nxt[DEBRIS_BYTES];
		const uint32_t node = n.node, x1 = P(nxt);
		// (a node finishing on the real tick is released: it holds its last draw, not drawn at fraction 1)
		const bool next = (U8(P(n.bytes), 0x26) & 1) == 0 && live(node, ORIG_Debris);
		if (!next && num >= den) return;
		if (next && num >= den)
		{
			// the next tick's state step (flipbook frame) on the node as the real tick left it (hidden
			// by its last frame on the next tick, it draws nothing there)
			memcpy(nxt, (const void *)node, DEBRIS_BYTES);
			if (S8(x1, 0x29) == 1)
				np_8A2CD0(x1);
			memcpy((void *)node, nxt, DEBRIS_BYTES);
		}
		else
			memcpy((void *)node, n.bytes, DEBRIS_BYTES); // (it does not move: the frame holds)
		n.st.restore();
		np_8A0230(node);
	}

	// ---- chunks

	static void DrawChunk(const NodeNote &n, int num, int den)
	{
		alignas(4) static uint8_t nxt[CHUNK_BYTES];
		const uint32_t node = n.node, x1 = P(nxt);
		// (a chunk finishing on the real tick is released: it holds its last draw, not drawn at fraction 1)
		const bool next = (U8(P(n.bytes), 0x26) & 1) == 0 && live(node, ORIG_Chunk);
		if (!next && num >= den) return;
		if (next)
		{
			// the next tick's state step and motion (0x8A2C60, 0x8A09C6..) on the node as the real tick left it
			memcpy(nxt, (const void *)node, CHUNK_BYTES);
			if (S8(x1, 0x29) == 1)
				np_8A2C60(x1);
			for (int k = 0; k < 2; k++)
				chunk_move(x1 + 8 * k);
		}
		memcpy((void *)node, n.bytes, CHUNK_BYTES);
		if (next)
		{
			if (num >= den)
				memcpy((void *)node, nxt, CHUNK_BYTES);
			else
			{
				U16(node, 0x48) = lerp_w(U16(node, 0x48), U16(x1, 0x48), num, den);
				for (int k = 0; k < 3; k++)
					S32(node, 0x30 + 4 * k) = lerp_i(S32(node, 0x30 + 4 * k), S32(x1, 0x30 + 4 * k), num, den);
				for (int p = 0; p < 2; p++)
					for (int k = 0; k < 3; k++)
					{
						const int32_t o = 0x54 + 8 * p + 2 * k;
						S16(node, o) = (int16_t)lerp_i(S16(node, o), S16(x1, o), num, den);
					}
			}
		}
		n.st.restore();
		for (int k = 0; k < 2; k++)
		{
			const uint32_t p = node + 0x54 + 8 * k;
			U32(node, 0x1C) = U32(p, 0);
			U32(node, 0x20) = U32(p, 4);
			np_8A0AA0(node);
		}
	}

	// ---- screen tint (0x8DDC30 state 1 = 0x8DDCD0: colour script step, four semi-transparent flat
	// quads over the screen + a draw mode into OT base + 0x1C, fixed packets 0x2795110; as
	// mag019_raldo_throw_held.inc)

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
			// (a tint the real tick did not draw is not drawn; one the next tick does not draw holds, and
			// is not drawn at fraction 1)
			if (!drew || (!drew1 && num >= den)) continue;
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
		g_mod = &MOD_020;
		g_in_held = true;
		static uint8_t bss[BSS_HI - BSS_LO], ents[ENT_HI - ENT_LO], shadow[0x80];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(ents, (const void *)ENT_LO, sizeof(ents));
		now.save();
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		static uint8_t model_top[MODEL_TOP_N];
		const uint32_t top = MEM<uint32_t>(MODEL_END) - MODEL_TOP_N;
		memcpy(model_top, (const void *)top, MODEL_TOP_N);
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
			add_win(ev.kind == EV_LID ? g_lid[ev.i].st.fa : ev.kind == EV_DEBRIS ? g_debris[ev.i].st.fa : g_chunk[ev.i].st.fa);
		}
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
			if (ev.kind == EV_LID)
			{
				DrawLid(g_lid[ev.i], num, den);
				memcpy((void *)ENT_LO, ents, sizeof(ents));
			}
			else if (ev.kind == EV_DEBRIS)
				DrawDebris(g_debris[ev.i], num, den);
			else
				DrawChunk(g_chunk[ev.i], num, den);
		}
		TintHeld(num, den);
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		memcpy((void *)top, model_top, MODEL_TOP_N);
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)ENT_LO, ents, sizeof(ents));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_in_held = false;
		g_mod = saved_mod;
	}
}
}
	static void register_mag020_held()
	{
		mark_held(act::norgpod::ORIG_Lid);
		mark_held(act::norgpod::ORIG_Debris);
		mark_held(act::norgpod::ORIG_Chunk);
		mark_held(0x8DDC30);
		register_module_held(20, act::norgpod::HeldReady, act::norgpod::HeldFrame);
	}
}
