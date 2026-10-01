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

// 30 fps part of mag042_shoot.cpp: the held-frame drawing of the actor system, the ball, the particle
// bursts / dust and the prim-model tasks (included at the end of mag042_shoot.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the
// real tick notes, in drawing order, every draw with the state it started from (GTE registers,
// module scratch stack, Field_Alloc scratch):
//   - the actor system (task 0x83F170; as mag031_heartbreak_held.inc): each drawn actor from a copy
//     whose view positions and draw matrix are half way to the next tick's (the next tick's update
//     pass on second copies, emitters put back); flipbook frame, model, fade and colours hold (the
//     next tick's at fraction 1); an actor ending on the next tick holds; an actor not in its
//     motion phase holds.
//   - the ball (task 0x844050, drawn by 0x844160): the next tick's ball state is run on the ball
//     block without its side effects (chain transformations, pass-script steps, kick effects,
//     landing dust and sound: the grab, the passes and the shot are pure arithmetic of the block,
//     the caster's bones and the tick), then the ball is redrawn with its rotation, pivot,
//     position, angles (shortest arc), morph and radii half way to the next tick's (the entity of
//     slot 1 placed there for the draw and put back); its pose and the caster's bones hold (engine
//     animation). A ball the next tick does not draw (the entity given back) holds, and is not
//     drawn at fraction 1. On the ticks the ball turns by half a turn (0x845160 between its ticks
//     31 and 38: +0x800 about x a tick, the ball flips about the hand bone) there is no in-between
//     direction, and on its tick 39 its rotation snaps to the identity (another branch of 0x845160):
//     it holds and turns on the real frame. Its packets (frame arena cursor 0x1D8E054)
//     go to a private buffer.
//   - particle bursts (task 0x844E30) and dust puffs (task 0x845B30), sprite sequences 0x844EC0:
//     position half way to the next tick's (the motion step 0x844F30 / 0x845BE0 run on a copy);
//     flipbook frame held (the next tick's at fraction 1); a puff hidden by its last frame on the
//     next tick holds (nothing at fraction 1, as the next tick). Bursts / dust spawned on the next
//     tick are not drawn (random growth stays at 15 Hz).
//   - the prim-model tasks (the prim-model player at node +0x94, callback 0x846090):
//     prim::play_held (the exact in-between records of the real tick's play) with the parameter
//     block rebuilt from the placement and the current camera: the ball model's placement (the
//     ball position, the angles +0xA4 / +0xA8) half way to the next tick's (the ball prediction,
//     the kick script's next angle; shortest arc); the kicks do not move. A play the next tick
//     does not make holds (prim::play_held keeps the last records).
// The packets go to a private buffer (module cursor 0x265910C) that starts with the real tick's
// packets (its start taken at the tick's first draw: the master picks the arena by tick parity);
// the module globals, the entities, the module scratch stack, the Field_Alloc scratch, the
// sprite-camera / geometry temporaries and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace shoot
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x2659108, BSS_HI = 0x267B024;
	static const uint32_t ENT_LO = 0x1D972C0, ENT_HI = 0x1D972C0 + 7 * 0x9C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t FALLOC_PTR = 0x1D999C4;
	static const uint32_t FRAME_CURSOR = 0x1D8E054; // frame arena cursor (the ball's packets)
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_BYTES = 0x1FC, BLK_BYTES = 0xE0, PRIM_BYTES = 0x118, BALL_PKT = 0x8000;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x842620) rewrites them in the actor data
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

	struct SysNote
	{
		uint32_t sys;
		DrawState st;
		VertexBlocks verts; // the vertex blocks its models were drawn with
	};
	struct BallNote
	{
		uint32_t node;
		uint8_t blk[BLK_BYTES];
		uint32_t pkt;            // frame arena cursor at the draw
		uint8_t packets[BALL_PKT];
		DrawState st;
	};
	struct PuffNote
	{
		uint32_t node;
		int32_t i;
		uint8_t bytes[NODE_BYTES]; // the node as the real tick drew puff i
		DrawState st;
	};
	struct PlayNote
	{
		uint32_t node;
		uint8_t place[0x20];       // the placement matrix (before the camera)
		uint16_t ax, az;           // the ball model's angles (block +0xA4 / +0xA8)
		DrawState st;
	};
	enum { EV_SYS, EV_BALL, EV_PUFF, EV_PLAY };
	struct Event { int kind, i; };

	static const int MAX_SYS = 4, MAX_BALL = 4, MAX_PUFF = 256, MAX_PLAY = 16, MAX_EV = MAX_SYS + MAX_BALL + MAX_PUFF + MAX_PLAY;
	static SysNote g_sys[MAX_SYS];
	static BallNote g_ball[MAX_BALL];
	static PuffNote g_puff[MAX_PUFF];
	static PlayNote g_play[MAX_PLAY];
	static Event g_ev[MAX_EV];
	static int g_nsys = 0, g_nball = 0, g_npuff = 0, g_nplay = 0, g_nev = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0; // module cursor at the tick's first draw
	static uint8_t g_held_packets[0x40000];
	static uint8_t g_ball_packets[BALL_PKT * 2];
	static bool g_held_drawing = false;
	// the ball prediction of the held frame (HeldFrame): the next tick's block after each noted ball
	static uint8_t g_next_blk[MAX_BALL][BLK_BYTES];
	static bool g_next_draw[MAX_BALL];
	static bool g_next_spin[MAX_BALL]; // the next tick turns the ball by half a turn or snaps it back (0x845160, ticks 31..39)
	static uint8_t g_next_final[BLK_BYTES]; // the block after all of them (what the prim queue reads)
	static bool g_next_any = false;

	// real tick: the master starts (before any draw of the tick)
	static void held_note_master()
	{
		act::held_note_master();
		g_note_tick = g_real_tick;
		g_nsys = g_nball = g_npuff = g_nplay = g_nev = 0;
		g_pkt_start = 0; // (the master sets the tick's arena after this note: taken at the first draw)
	}

	static bool noting(int n, int max)
	{
		if (g_held_drawing || g_note_tick != g_real_tick || n >= max || g_nev >= MAX_EV) return false;
		if (!g_pkt_start) g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		return true;
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

	// real tick: the ball is about to be drawn (0x844160; its block and the entity placed)
	static void held_note_ball(uint32_t node)
	{
		if (!noting(g_nball, MAX_BALL)) return;
		BallNote &n = g_ball[g_nball];
		n.node = node;
		memcpy(n.blk, (const void *)BLK, BLK_BYTES);
		n.pkt = MEM<uint32_t>(FRAME_CURSOR);
		n.st.save();
		g_ev[g_nev++] = { EV_BALL, g_nball++ };
	}

	// real tick: puff i of a particle-burst / dust node is about to be drawn (0x844EC0)
	static void held_note_puff(uint32_t node, int32_t i)
	{
		if (!noting(g_npuff, MAX_PUFF)) return;
		PuffNote &n = g_puff[g_npuff];
		n.node = node;
		n.i = i;
		memcpy(n.bytes, (const void *)node, NODE_BYTES);
		n.st.save();
		g_ev[g_nev++] = { EV_PUFF, g_npuff++ };
	}

	// real tick: a prim-model task is about to play its model (placement `place`, before the camera)
	static void held_note_play(uint32_t node, uint32_t place)
	{
		if (!noting(g_nplay, MAX_PLAY)) return;
		PlayNote &n = g_play[g_nplay];
		n.node = node;
		memcpy(n.place, (const void *)place, sizeof(n.place));
		n.ax = MEM<uint16_t>(BLK + 0xA4);
		n.az = MEM<uint16_t>(BLK + 0xA8);
		n.st.save();
		g_ev[g_nev++] = { EV_PLAY, g_nplay++ };
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// 12-bit angle half way along the shortest arc
	static uint16_t lerp_angle(uint16_t a, uint16_t b, int num, int den)
	{
		if (num >= den) return b;
		int32_t d = ((int32_t)b - (int32_t)a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		return (uint16_t)(((int32_t)a + d * num / den) & 0xFFF);
	}

	// ---- the actor system (as mag031_heartbreak_held.inc)

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's update step of
	// 0x8415F0 run on it): view positions, draw matrix (rotation / scale), sub-positions; at
	// fraction 1 the whole next actor. P_ENDS: the actor ends on the next tick (it holds);
	// P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x842730 (its end spawns: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x8415F0)
		}
		return P_NEXT;
	}

	// the next tick's step (0x8415F0) of an actor that is not drawn, on the actor itself (the caller
	// puts it back): motion and transform, without the end spawns
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
		// 0x841470 / 0x83FDF0 draw them
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x8415B0) in list order, on the live actors (every one saved
		// first, all put back after): the emitters' placement / motion (0x842880 without its emission
		// and life count) and the particles' steps (0x8415F0 without its end spawns: Predict for the
		// drawn ones, whose next state is written back); an emitter attached to a particle (mode 3,
		// 0x843230) reads the particle's next position, as on the next tick
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
					callp(0x843230, act);
				else if (mode == 4)
					callp(0x843380, act);
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
					sh_842620(P(nxts[i]), DefOf(list[i]));
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
				sh_841530(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					sh_841530(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5) // (an actor ending on the next tick holds)
				sh_83FE50(P(copies[i]), def);
		}
	}

	// ---- the ball

	// the next tick's ball state on the ball block and the node (real memory: the caller puts them
	// and the entities back), without its side effects (chain transformations, pass-script steps,
	// kick effects, landing dust and sound); returns whether the next tick draws the ball
	static bool PredictBall(uint32_t node)
	{
		if (!live(node, ORIG_Ball)) return false;
		switch (S8(node, 0x29))
		{
		case 1: sh_845340(node); break;
		case 2: sh_845380(node); break;
		case 3: // 0x8453E0 without the kick effects
		case 4: // 0x845420 without the kick effects and the pass-script step
			sh_8450A0(node, BLK);
			sh_844AA0(node, BLK);
			sh_845160(node, BLK);
			break;
		case 5: // 0x8456F0: the position step (the pass-script step comes after it)
			sh_8450A0(node, BLK);
			sh_844AA0(node, BLK);
			MEM<uint16_t>(BLK + 0x7C) = (uint16_t)(MEM<uint16_t>(BLK + 0x7C) + MEM<uint16_t>(BLK + 0x94));
			MEM<uint16_t>(BLK + 0x7E) = (uint16_t)(MEM<uint16_t>(BLK + 0x7E) + MEM<uint16_t>(BLK + 0x96));
			MEM<uint16_t>(BLK + 0x80) = (uint16_t)(MEM<uint16_t>(BLK + 0x80) + MEM<uint16_t>(BLK + 0x98));
			break;
		case 6: // 0x845860 without the landing's chain transformation, dust and sound
		{
			sh_8450A0(node, BLK);
			sh_844AA0(node, BLK);
			const uint16_t t = MEM<uint16_t>(BLK + 0xD2);
			if ((int16_t)t <= 0x10)
			{
				MEM<uint16_t>(BLK + 0x9C) = (uint16_t)((uint16_t)(t * 0xFF00u) & 0xFFF);
				if (MEM<uint16_t>(BLK + 0xD4) == 1)
					MEM<uint16_t>(BLK + 0x9E) = (uint16_t)((uint16_t)(0x10 - t) << 7);
			}
			const uint16_t m = (uint16_t)(MEM<uint16_t>(BLK + 0xC8) + 0xFF00);
			MEM<uint16_t>(BLK + 0xC8) = (int16_t)m <= 0 ? 0 : m;
			MEM<uint16_t>(BLK + 0x7C) = (uint16_t)(MEM<uint16_t>(BLK + 0x7C) + MEM<uint16_t>(BLK + 0x94));
			MEM<uint16_t>(BLK + 0x80) = (uint16_t)(MEM<uint16_t>(BLK + 0x80) + MEM<uint16_t>(BLK + 0x98));
			const uint16_t vy = (uint16_t)(MEM<uint16_t>(BLK + 0x96) + 0x30);
			MEM<uint16_t>(BLK + 0x7E) = (uint16_t)(MEM<uint16_t>(BLK + 0x7E) + vy);
			MEM<uint16_t>(BLK + 0x96) = vy;
			if ((int16_t)vy > 0 && (int16_t)MEM<uint16_t>(BLK + 0x7E) >= (int16_t)MEM<uint16_t>(BLK + 0x76))
				MEM<uint16_t>(BLK + 0x7E) = MEM<uint16_t>(BLK + 0x76);
			MEM<uint16_t>(BLK + 0xD2) = (uint16_t)(t + 1);
			break;
		}
		default: // 0x845D10 turns the custom draw off (the later states do not draw)
			return false;
		}
		return (U8(node, 0x26) & 8) != 0;
	}

	// the kick script's prim-model angle the next tick writes (0x844B00 from the ball states 0 and
	// 3..6, ball tick <= 76), else the current one
	static uint16_t NextPrimAngle(uint32_t ball, uint16_t cur)
	{
		if (!live(ball, ORIG_Ball)) return cur;
		const int8_t st = S8(ball, 0x29);
		if (st == 1 || st == 2 || st > 6) return cur;
		const int16_t t = S16(ball, 0x24);
		if (t > 0x4C) return cur;
		return U16(KICK_SCRIPT + (uint32_t)((int32_t)t * 14), 6);
	}

	static void DrawBall(const BallNote &n, int b, int num, int den)
	{
		const bool next = g_next_draw[b];
		if (!next && num >= den) return; // not drawn on the next tick
		// the drawn block, half way to the next one
		memcpy((void *)BLK, n.blk, BLK_BYTES);
		// (a turn of half a turn has no in-between direction: the ball holds and turns on the real frame)
		if (next && (!g_next_spin[b] || num >= den))
		{
			const uint32_t nb = P(g_next_blk[b]);
			for (int k = 0; k < 9; k++) // ball rotation
				S16(BLK, 2 * k) = (int16_t)lerp_i(S16(BLK, 2 * k), S16(nb, 2 * k), num, den);
			for (int k = 0; k < 3; k++) // pivot, position
			{
				S16(BLK, 0x74 + 2 * k) = (int16_t)lerp_i(S16(BLK, 0x74 + 2 * k), S16(nb, 0x74 + 2 * k), num, den);
				S16(BLK, 0x7C + 2 * k) = (int16_t)lerp_i(S16(BLK, 0x7C + 2 * k), S16(nb, 0x7C + 2 * k), num, den);
			}
			for (int k = 0; k < 3; k++) // ball angles
				U16(BLK, 0x9C + 2 * k) = lerp_angle(U16(BLK, 0x9C + 2 * k), U16(nb, 0x9C + 2 * k), num, den);
			for (int k = 0; k < 4; k++) // morph, radii
				S16(BLK, 0xC8 + 2 * k) = (int16_t)lerp_i(S16(BLK, 0xC8 + 2 * k), S16(nb, 0xC8 + 2 * k), num, den);
		}
		// the entity of slot 1 placed at the ball as 0x844050 does
		const uint32_t ent = ENT1;
		const int16_t px = (int16_t)(MEM<uint16_t>(BLK + 0x7C) - MEM<uint16_t>(BLK + 0x74));
		const int16_t py = (int16_t)(MEM<uint16_t>(BLK + 0x7E) - MEM<uint16_t>(BLK + 0x76));
		const int16_t pz = (int16_t)(MEM<uint16_t>(BLK + 0x80) - MEM<uint16_t>(BLK + 0x78));
		S16(ent, 0x1C) = px;
		S32(ent, 0x54) = px;
		S16(ent, 0x1E) = py;
		S32(ent, 0x58) = py;
		S16(ent, 0x20) = pz;
		S32(ent, 0x5C) = pz;
		// the draw starts from the drawn tick's state; its packets go to the private buffer, which
		// starts with the real tick's packets (the halves a draw does not write are the real tick's)
		n.st.restore();
		memcpy(g_ball_packets, n.packets, BALL_PKT);
		MEM<uint32_t>(FRAME_CURSOR) = P(g_ball_packets);
		g_held_drawing = true;
		sh_844160(BLK);
		g_held_drawing = false;
	}

	// ---- particle bursts and dust

	static void DrawPuff(const PuffNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[NODE_BYTES], nxt[NODE_BYTES];
		memcpy(cur, n.bytes, NODE_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		const uint32_t task = U32(c, 8);
		// the motion state: burst state 0 (0x844F30), dust state 1 (0x845BE0)
		const int8_t motion = task == ORIG_Burst ? 0 : 1;
		// (a node released after the real tick holds its last draw)
		if (live(n.node, task) && S8(n.node, 0x29) == motion && (U8(n.node, 0x26) & 4) == 0)
		{
			memcpy(nxt, (const void *)n.node, NODE_BYTES);
			if (task == ORIG_Burst)
				sh_844F30(x1);
			else
				sh_845BE0(x1);
			const uint32_t o = 0x78 + 8 * (uint32_t)n.i;
			if (num >= den)
			{
				// the next tick's draw of puff i (hidden by its last frame: nothing, as the next tick)
				memcpy(cur, nxt, NODE_BYTES);
				U32(c, 0x1C) = U32(x1, o);
				U32(c, 0x20) = U32(x1, o + 4);
			}
			else if ((U8(x1, 0x26) & 4) == 0)
				for (int k = 0; k < 3; k++)
					S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, o + 2 * k), num, den);
		}
		n.st.restore();
		g_held_drawing = true;
		sh_844EC0(c);
		g_held_drawing = false;
	}

	// ---- the prim-model tasks (ball model 0x845F20, kicks 0x846350)

	static void DrawPlay(const PlayNote &n, int num, int den)
	{
		alignas(4) static uint8_t fr[0x5C];
		memset(fr, 0, sizeof(fr));
		const uint32_t L = P(fr);
		if (U32(n.node, 8) == ORIG_BallPrim)
		{
			// the placement as 0x845F70 builds it, from the ball position and the angles half way
			// to the next tick's
			int32_t pos[3] = { S32(P(n.place), 0x14), S32(P(n.place), 0x18), S32(P(n.place), 0x1C) };
			uint16_t ax = n.ax, az = n.az;
			if (g_next_any)
			{
				const uint32_t nb = P(g_next_final);
				for (int k = 0; k < 3; k++)
					pos[k] = lerp_i(pos[k], S16(nb, 0x7C + 2 * k), num, den);
				const uint32_t ball = g_nball ? g_ball[g_nball - 1].node : 0;
				ax = lerp_angle(ax, U16(nb, 0xA4), num, den);
				az = lerp_angle(az, ball ? NextPrimAngle(ball, U16(nb, 0xA8)) : U16(nb, 0xA8), num, den);
			}
			x::MAG_022_sub_8DD770(L);
			S32(L, 0x14) = pos[0];
			S32(L, 0x18) = pos[1];
			S32(L, 0x1C) = pos[2];
			if (ax != 0)
				x::sub_8DD7E0(L, (uint32_t)(int32_t)(int16_t)ax);
			if (az != 0)
				x::sub_8DD960(L, (uint32_t)(int32_t)(int16_t)az);
			x::ComposeAffineTransform(0x1D97778, L, L);
		}
		else
			x::ComposeAffineTransform(0x1D97778, P(n.place), L);
		U32(L, 0x48) = BLEND_BUF;
		n.st.restore();
		void *p = port_of(PRIM_CB);
		prim::play_held((prim::Layout *)(n.node + 0x94), (prim::Callback)(p ? P(p) : PRIM_CB), (int)L, num, den);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_042;
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
			else if (ev.kind == EV_BALL) add_win(g_ball[ev.i].st.fa);
			else if (ev.kind == EV_PUFF) add_win(g_puff[ev.i].st.fa);
			else add_win(g_play[ev.i].st.fa);
		}
		static VertexBlocks verts_now[MAX_SYS]; // the blocks as the real tick left them
		for (int s = 0; s < g_nsys; s++) verts_now[s].save(g_sys[s].sys);
		// the next tick's ball states, in the order the next tick runs them (on the real block, the
		// nodes and the entities, all put back)
		static uint8_t nodes_save[MAX_BALL][NODE_BYTES];
		for (int b = 0; b < g_nball; b++) memcpy(nodes_save[b], (const void *)g_ball[b].node, NODE_BYTES);
		g_next_any = false;
		for (int b = 0; b < g_nball; b++)
		{
			const int8_t st = S8(g_ball[b].node, 0x29);
			const int16_t t = S16(g_ball[b].node, 0x24);
			g_next_spin[b] = live(g_ball[b].node, ORIG_Ball) && st >= 1 && st <= 4 && t > 0x1E && t <= 0x27;
			g_next_draw[b] = PredictBall(g_ball[b].node);
			memcpy(g_next_blk[b], (const void *)BLK, BLK_BYTES);
			g_next_any = true;
		}
		memcpy(g_next_final, (const void *)BLK, BLK_BYTES);
		for (int b = g_nball - 1; b >= 0; b--) memcpy((void *)g_ball[b].node, nodes_save[b], NODE_BYTES);
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		memcpy((void *)ENT_LO, ents, sizeof(ents));
		now.restore();
		// the ball draws' packets as the real tick wrote them (the frame arena from the noted cursor)
		for (int b = 0; b < g_nball; b++)
			memcpy(g_ball[b].packets, (const void *)g_ball[b].pkt, BALL_PKT);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = pkt_cursor - g_pkt_start;
		if (g_pkt_start && pkt_cursor > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_SYS)
				DrawSystem(g_sys[ev.i], num, den);
			else if (ev.kind == EV_BALL)
				DrawBall(g_ball[ev.i], ev.i, num, den);
			else if (ev.kind == EV_PUFF)
				DrawPuff(g_puff[ev.i], num, den);
			else
				DrawPlay(g_play[ev.i], num, den);
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
	}
}
}
	static void register_mag042_held()
	{
		mark_held(act::shoot::ORIG_ActorSystem);
		mark_held(act::shoot::ORIG_Ball);
		mark_held(act::shoot::ORIG_Burst);
		mark_held(act::shoot::ORIG_Dust);
		mark_held(act::shoot::ORIG_BallPrim);
		mark_held(act::shoot::ORIG_KickPrim);
		register_module_held(42, act::shoot::HeldReady, act::shoot::HeldFrame);
	}
}
