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

// 30 fps part of mag031_heartbreak.cpp: the held-frame drawing of the screen flashes, the actor system,
// the prim-model tasks (heart, ring, arrow, burst) and the sparks (included at the end of
// mag031_heartbreak.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// module scratch stack, Field_Alloc scratch):
//   - screen flashes (task 0x86CF60): the quad's colour half way (per channel) to the next tick's
//     (the colour script step run on the node as the tick left it); a flash the next tick does not
//     draw (end of its script) holds, and is not drawn at fraction 1.
//   - the actor system (task 0x86D1D0, as mag023_psycho_blast_held.inc): each drawn actor from a
//     copy whose view positions and draw matrix are half way to the next tick's (the next tick's
//     update pass on second copies, emitters put back); flipbook frame, model, fade and colours hold
//     (the next tick's at fraction 1); an actor ending on the next tick holds; an actor not in its
//     motion phase holds.
//   - the prim-model tasks (the prim-model player at node +0x94, callback 0x872700): prim::play_held
//     (the exact in-between records of the real tick's play) with the parameter block rebuilt from
//     the noted placement and the current camera; the arrow's position half way to the next tick's
//     (its flight step on the arc, 0x872CB0); the heart, the ring and the burst do not move. A play
//     the next tick does not make holds (prim::play_held keeps the last records).
//   - sparks (0x8723E0 / 0x873010 / 0x8730C0, sprite sequences 0x872440): position half way to the
//     next tick's (its state step run on a copy: 0x8724B0 / 0x873070 + frame step / 0x873150);
//     flipbook frame held (the next one at fraction 1). A spark hidden by its last frame on the next
//     tick holds (nothing at fraction 1, as the next tick); a rising spark whose first step (random
//     velocity) has not run holds. Sparks spawned on the next tick are not drawn (random growth stays
//     at 15 Hz).
// The packets go to a private buffer (module cursor 0x26CDD90) that starts with the real tick's
// packets; the modelled stack words (RESIDUE), the module globals, the entities, the module
// scratch stack, the Field_Alloc scratch, the
// shadow / sprite-camera / geometry temporaries and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace heartbreak
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x26CDCC8, BSS_HI = 0x26D33E4;
	static const uint32_t ENT_LO = 0x1D972C0, ENT_HI = 0x1D972C0 + 7 * 0x9C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t FRAME_CURSOR = 0x1D8E054; // frame arena cursor
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t FLASH_BYTES = 0x3C, PRIM_BYTES = 0x1B0, SPARK_BYTES = 0x80;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x870570) rewrites them in the actor data
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

	struct FlashNote
	{
		uint32_t node;
		uint32_t colour; // the drawn colour
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
		uint8_t place[0x20];       // the placement matrix (before the camera; the arrow: none)
		DrawState st;
	};
	struct SparkNote
	{
		uint32_t node;
		uint8_t bytes[SPARK_BYTES]; // the node as the real tick drew it
		DrawState st;
	};
	enum { EV_FLASH, EV_SYS, EV_PLAY, EV_SPARK };
	struct Event { int kind, i; };

	static const int MAX_FLASH = 8, MAX_SYS = 4, MAX_PLAY = 32, MAX_SPARK = 256, MAX_EV = MAX_FLASH + MAX_SYS + MAX_PLAY + MAX_SPARK;
	static FlashNote g_flash[MAX_FLASH];
	static SysNote g_sys[MAX_SYS];
	static PlayNote g_play[MAX_PLAY];
	static SparkNote g_spark[MAX_SPARK];
	static Event g_ev[MAX_EV];
	static int g_nflash = 0, g_nsys = 0, g_nplay = 0, g_nspark = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0; // module cursor at the tick's first draw
	static uint8_t g_held_packets[0x40000];
	static bool g_held_drawing = false;

	// real tick: the master starts (before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nflash = g_nsys = g_nplay = g_nspark = g_nev = 0;
		g_pkt_start = 0; // (the master sets the tick's arena after this note: taken at the first draw)
	}

	static bool noting(int n, int max)
	{
		if (g_held_drawing || g_note_tick != g_real_tick || n >= max || g_nev >= MAX_EV) return false;
		if (!g_pkt_start) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		return true;
	}

	// real tick: a screen flash is about to draw its quad (colour +0x34)
	static void held_note_flash(uint32_t node)
	{
		if (!noting(g_nflash, MAX_FLASH)) return;
		g_flash[g_nflash] = { node, U32(node, 0x34) };
		g_ev[g_nev++] = { EV_FLASH, g_nflash++ };
	}

	// real tick: the actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		if (!noting(g_nsys, MAX_SYS)) return;
		SysNote &n = g_sys[g_nsys];
		n.sys = sys;
		n.st.save();
		n.verts.save(sys);
		g_ev[g_nev++] = { EV_SYS, g_nsys++ };
	}

	// real tick: a prim-model task is about to play its model (placement `place`, 0 = the arrow)
	static void held_note_play(uint32_t node, uint32_t place)
	{
		if (!noting(g_nplay, MAX_PLAY)) return;
		PlayNote &n = g_play[g_nplay];
		n.node = node;
		memcpy(n.bytes, (const void *)node, PRIM_BYTES);
		if (place)
			memcpy(n.place, (const void *)place, sizeof(n.place));
		else
			memset(n.place, 0, sizeof(n.place));
		n.st.save();
		g_ev[g_nev++] = { EV_PLAY, g_nplay++ };
	}

	// real tick: a spark is about to draw its sprite (after its state step)
	static void held_note_spark(uint32_t node)
	{
		if (!noting(g_nspark, MAX_SPARK)) return;
		SparkNote &n = g_spark[g_nspark];
		n.node = node;
		memcpy(n.bytes, (const void *)node, SPARK_BYTES);
		n.st.save();
		g_ev[g_nev++] = { EV_SPARK, g_nspark++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// ---- screen flashes

	// the colour the flash draws on the next tick: the script step of 0x86CF60 on the node as its tick
	// left it (frame counter +0x24 already advanced); false when it does not draw then
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
		uint32_t col = n.colour, next = 0;
		const bool drawn = live(n.node, ORIG_Flash) && FlashNext(n.node, next);
		if (!drawn)
		{
			if (num >= den) return; // (the next tick draws nothing)
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

	// ---- the actor system (as mag023_psycho_blast_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x86F540 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor. P_ENDS: the actor ends on the next tick (it holds);
	// P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x870680 (its end spawns: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x86F540)
		}
		return P_NEXT;
	}

	// the next tick's step (0x86F540) of an actor that is not drawn, on the actor itself (the caller
	// puts it back): motion and transform, without the end spawns (0x870700)
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
		// 0x86F3C0 / 0x86DD40 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x86F500) in list order, on the live actors (every one saved
		// first, all put back after): the emitters' placement / motion (0x8707D0 without its emission
		// and life count) and the particles' steps (0x86F540 without its end spawns: Predict for the
		// drawn ones, whose next state is written back); an emitter attached to a particle (mode 3,
		// 0x871180) reads the particle's next position, as on the next tick
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
					callp(0x871180, act);
				else if (mode == 4)
					callp(0x8712D0, act);
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
					hb_870570(P(nxts[i]), DefOf(list[i]));
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
				hb_86F480(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					hb_86F480(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5) // (an actor ending on the next tick holds)
				hb_86DDA0(P(copies[i]), def);
		}
	}

	// ---- the prim-model tasks (heart 0x871F60, ring 0x872920, arrow 0x872B10, burst 0x8731D0)

	// the arrow's next position (+0x1C..+0x20) on the node copy nxt: the flight step of 0x872CB0 the
	// next tick runs (the node as the real tick left it); false = the next tick does not play it
	static bool PredictArrow(uint32_t node, uint32_t nxt)
	{
		if (!live(node, ORIG_Arrow) || S8(node, 0x29) != 2 || (U8(node, 0x26) & 1) != 0) return false;
		memcpy((void *)nxt, (const void *)node, PRIM_BYTES);
		const int32_t k = S16(nxt, 0x1AE);
		const int16_t t = MEM<int16_t>(FLIGHT_T + (uint32_t)k * 2);
		if (t >= 0x1000) return false; // (arrived: finished, not played)
		arrow_pos(nxt, k, t);
		return true;
	}

	static void DrawPlay(const PlayNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[PRIM_BYTES], nxt[PRIM_BYTES];
		alignas(4) static uint8_t fr[0x5C];
		memcpy(cur, n.bytes, PRIM_BYTES);
		memset(fr, 0, sizeof(fr));
		const uint32_t c = P(cur), x1 = P(nxt), L = P(fr);
		const uint32_t task = U32(c, 8);
		if (task == ORIG_Arrow)
		{
			if (PredictArrow(n.node, x1))
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
			arrow_matrix(c, caster_entity(c), L);
			x::ComposeAffineTransform(0x1D97778, L, L);
		}
		else if (task == ORIG_Burst)
			x::ComposeAffineTransform(0x1D97778, P(n.place), L);
		else
		{
			memcpy(fr, n.place, sizeof(n.place));
			x::ComposeAffineTransform(0x1D97778, L, L);
		}
		U32(L, 0x48) = BLEND_BUF;
		n.st.restore();
		void *p = port_of(PRIM_CB);
		prim::play_held((prim::Layout *)(n.node + 0x94), (prim::Callback)(p ? P(p) : PRIM_CB), (int)L, num, den);
	}

	// ---- sparks

	// the next tick's state step of a spark on the copy x1; false = not predictable (the rising
	// spark's first step draws its random velocity: it holds)
	static bool StepSpark(uint32_t x1)
	{
		const uint32_t task = U32(x1, 8);
		const int8_t st = S8(x1, 0x29);
		if (task == ORIG_SparkA)
		{
			if (st == 0) hb_8724B0(x1);
			return true;
		}
		if (task == ORIG_SparkB)
		{
			if (st == 0) hb_873070(x1);
			else if (st == 1) a_743C00(x1);
			return true;
		}
		if (st == 1) hb_873150(x1);
		return st != 0;
	}

	static void DrawSpark(const SparkNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[SPARK_BYTES], nxt[SPARK_BYTES];
		memcpy(cur, n.bytes, SPARK_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		// (a node released after the real tick holds its last draw)
		if (live(n.node, U32(c, 8)) && (U8(n.node, 0x26) & 4) == 0)
		{
			memcpy(nxt, (const void *)n.node, SPARK_BYTES);
			if (StepSpark(x1))
			{
				if (num >= den)
					memcpy(cur, nxt, SPARK_BYTES); // (hidden by its last frame: nothing, as the next tick)
				else if ((U8(x1, 0x26) & 4) == 0)
					for (int k = 0; k < 3; k++)
						S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
			}
		}
		n.st.restore();
		g_held_drawing = true;
		hb_872440(c);
		g_held_drawing = false;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_031;
		// the modelled stack words (RESIDUE) as the real tick left them: the replayed actor sprites
		// (0x86F480) update them, the next real tick must not see it
		uint32_t res_save[27];
		memcpy(res_save, g_shadow, sizeof(res_save));
		const uint32_t sprite_ebp = g_sprite_ebp;
		static uint8_t bss[BSS_HI - BSS_LO], ents[ENT_HI - ENT_LO], shadow[0x80];
		static DrawState now;
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(ents, (const void *)ENT_LO, sizeof(ents));
		now.save();
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		uint8_t sprite_cam[SPRITE_CAM_N];
		memcpy(sprite_cam, (const void *)SPRITE_CAM, SPRITE_CAM_N);
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t frame_cursor = MEM<uint32_t>(FRAME_CURSOR);
		const uint32_t pkt_cursor = MEM<uint32_t>(PACKET_CURSOR);
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
			if (ev.kind == EV_SYS) add_win(g_sys[ev.i].st.fa);
			else if (ev.kind == EV_PLAY) add_win(g_play[ev.i].st.fa);
			else if (ev.kind == EV_SPARK) add_win(g_spark[ev.i].st.fa);
		}
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = pkt_cursor - g_pkt_start;
		if (g_pkt_start && pkt_cursor > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_FLASH)
				DrawFlash(g_flash[ev.i], num, den);
			else if (ev.kind == EV_SYS)
				DrawSystem(g_sys[ev.i], num, den);
			else if (ev.kind == EV_PLAY)
				DrawPlay(g_play[ev.i], num, den);
			else
				DrawSpark(g_spark[ev.i], num, den);
		}
		for (int s = g_nsys - 1; s >= 0; s--) verts_now[s].restore();
		for (int w = nwin - 1; w >= 0; w--) memcpy((void *)fa_base[w], fa_win[w], WIN);
		now.restore();
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)SPRITE_CAM, sprite_cam, SPRITE_CAM_N);
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)ENT_LO, ents, sizeof(ents));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
		memcpy(g_shadow, res_save, sizeof(res_save));
		g_sprite_ebp = sprite_ebp;
	}
}
}
	static void register_mag031_held()
	{
		mark_held(act::heartbreak::ORIG_Flash);
		mark_held(act::heartbreak::ORIG_ActorSystem);
		mark_held(act::heartbreak::ORIG_Heart);
		mark_held(act::heartbreak::ORIG_Ring);
		mark_held(act::heartbreak::ORIG_Arrow);
		mark_held(act::heartbreak::ORIG_Burst);
		mark_held(act::heartbreak::ORIG_SparkA);
		mark_held(act::heartbreak::ORIG_SparkB);
		mark_held(act::heartbreak::ORIG_SparkC);
		register_module_held(31, act::heartbreak::HeldReady, act::heartbreak::HeldFrame);
	}
}
