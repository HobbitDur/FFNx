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

// 30 fps part of mag057_treatment.cpp: the held-frame drawing of the actor systems (included at the end of
// mag057_treatment.cpp)
//
// Held frames (30 fps): Treatment's particles are drawn by its actor systems (task 0x7FD6A0): every
// particle actor (sprite sequences 0x800540, prim models 0x7FE270) is updated THEN drawn in the same
// tick. The real tick notes, per actor system that drew, the GTE state, the module scratch stack,
// the Field_Alloc scratch and the morph vertex blocks its draws started from. The held frame
// redraws each noted system's actors, in the same order (sprite sequences, then models), from
// copies whose draw matrix (+0xAC: rotation / scale), view positions (+0xC0..+0xC8) and per-copy
// positions (+0x19C..) are half way to the next tick's: the next tick's update pass (0x8005C0) runs
// on copies of the system's particles and actors, in list order (NextTick: the particles' placement,
// the actors' motion step; no emission, no spawn), so a model placed relative to a particle (+0x1BC)
// that rides a moving actor (placement kind 3) moves with the next tick's particle; at fraction 1
// the whole next actor. An actor ending on the next tick (not drawn at fraction 1, as on
// the next tick) and an actor not in its motion phase hold; an actor whose sequence (+0x170)
// changes on the next tick holds until fraction 1. Flipbook frame, model and colours hold; a
// morphing model (definition kind 5) moves like the others but keeps the mesh it was drawn with
// (its step's morph 0x801630 rewrites the destination vertex block of its next keyframe in the
// loaded actor data, which every actor system of the cast shares: the blocks each system drew
// with are put back before its draws, so the mesh keyframe switches on the real tick); particles
// spawned on the next tick are not drawn (random growth stays at 15 Hz).
// Texture scroll: the model draw (0x7FE270) advances its actor system's V scroll counter (state +0x2A,
// + 2, 7-bit wrap) at every draw of a definition-index-1 or 2 actor; a discrete step like the flipbook
// frame: before fraction 1 the held frame replays the real tick's scroll values (the counter put
// back to its value before the tick's draws), at fraction 1 the next tick's (the replay starts from
// the counter the real tick left, as the next tick's draws do).
// The actors are placed by key positions the director set-up (0x7FD730) takes ONCE from the caster
// and the target entities: nothing the module draws follows the battle models afterwards.
// The packets go to private buffers (module cursor 0x260D5F4, frame arena 0x1D8E054) that start
// with the real tick's packets from the cursors of the tick's first draw (the master picks the module
// arena by tick parity before its queues run); the module globals, the module scratch stack, the
// Field_Alloc scratch, the shadow / RenderGeometry temporaries and the GTE registers are put back.
// The master, the emitters (0x7FD590) and the stage wobble tasks (0x7FD470 / 0x8030B0: the amplitude words
// 0x1D98992 + k * 0x2C read by the stage renderer, 4 steps of 0x100) draw nothing; the wobble holds (15 Hz).

namespace ff8fx
{
namespace act
{
namespace treatment
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x260D5F0, BSS_HI = 0x260E334;
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
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		VertexBlocks verts; // the vertex blocks its models were drawn with
		uint16_t scroll;    // its texture scroll counter (state +0x2A) before the tick's draws
	};
	static const int MAX_SYSTEMS = 8;
	static SysNote g_sys[MAX_SYSTEMS];
	static int g_nsys = 0;
	static uint32_t g_sys_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // the cursors at the tick's first draw
	static uint8_t g_held_packets[0x40000], g_held_fa[0x20000];

	// the battle camera words a view is built from (0x5033E0): eye 0xB8B7F0, look-at 0xB8B7F8, roll
	// 0x1D977A2, shake 0x1D97710
	struct CamWords
	{
		int16_t w[10];
		void io(bool set)
		{
			static const uint32_t a[10] = { 0xB8B7F0, 0xB8B7F2, 0xB8B7F4, 0xB8B7F8, 0xB8B7FA, 0xB8B7FC, 0x1D977A2, 0x1D97710, 0x1D97712, 0x1D97714 };
			for (int i = 0; i < 10; i++)
			{
				if (set) MEM<int16_t>(a[i]) = w[i];
				else w[i] = MEM<int16_t>(a[i]);
			}
		}
	};
	static const uint32_t CAM_COPY = 0x2793E58; // the module's camera snapshot (*G_257F9B0, the master copies 0x1D97778)
	// the words after each real tick (noted at its held frames: HeldReady): the view of the next tick
	// is built from them
	static CamWords g_cw_held, g_cw_prev;
	static uint32_t g_cw_held_tick = 0xFFFFFFFF, g_cw_prev_tick = 0xFFFFFFFF;
	static uint8_t g_cam_tick[0x20];             // the view this real tick drew with
	static void note_cam_words()
	{
		if (g_cw_held_tick == g_real_tick) return;
		g_cw_prev = g_cw_held;
		g_cw_prev_tick = g_cw_held_tick;
		g_cw_held.io(false);
		g_cw_held_tick = g_real_tick;
	}

	static void held_begin_tick()
	{
		if (g_sys_tick == g_real_tick) return;
		memcpy(g_cam_tick, (const void *)CAM_COPY, sizeof(g_cam_tick));
		g_sys_tick = g_real_tick;
		g_nsys = 0;
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		g_fa_start = MEM<uint32_t>(FA_CURSOR);
	}

	// the view the next tick draws with (the actors' view positions / draw matrices are built with
	// it): the battle builds a tick's view (0x5033E0) from the camera words the tick before left, so
	// with a live camera (this tick's view = the one built from the words after the previous tick)
	// the next view is the one built from the words now; otherwise (a fixed view) it is this tick's. The
	// camera block, the words, the GTE registers and the Field_Alloc scratch are put back.
	static bool NextView(uint8_t out[0x20])
	{
		if (g_cw_prev_tick + 1 != g_real_tick) return false; // (the words after the previous tick: noted at its held frame)
		static uint8_t cam[0xB0], words[0x20], gte[GTE_D_N + GTE_C_N], falloc[0x400];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		memcpy(words, (const void *)0xB8B7F0, sizeof(words));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		if (fa) memcpy(falloc, (const void *)fa, sizeof(falloc));
		CamWords now;
		now.io(false);
		g_cw_prev.io(true);
		fn<void (__cdecl *)()>(0x5033E0)(); // BS_Camera_BuildViewMatrixApplyShake
		const bool live = memcmp((const void *)0x1D97778, g_cam_tick, sizeof(g_cam_tick)) == 0;
		if (live)
		{
			now.io(true);
			fn<void (__cdecl *)()>(0x5033E0)();
			memcpy(out, (const void *)0x1D97778, 0x20);
		}
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)0xB8B7F0, words, sizeof(words));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		MEM<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, falloc, sizeof(falloc));
		return live;
	}

	// real tick: an actor system is about to draw (after its update pass)
	static void held_note_system(uint32_t sys)
	{
		held_begin_tick();
		if (g_nsys >= MAX_SYSTEMS) return;
		SysNote &n = g_sys[g_nsys++];
		n.sys = sys;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		n.verts.save(sys);
		n.scroll = U16(sys, 0x2A);
	}

	static bool HeldReady()
	{
		note_cam_words(); // (every held frame, drawn or not)
		return held_ready() && g_sys_tick == g_real_tick;
	}

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x8005C0: list order; particles = type 0, actors = type 1) run on
	// copies: a particle moves by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0;
	// 4 = key-position path 0x802390; its emission 0x801910 and life count 0x741B60: not run), an actor
	// by its motion step (0x800600: 0x741050, the end test of 0x801740 without its follow-up spawn,
	// 0x740910, 0x741120). A particle's owner and an actor's parent particle (+0x1BC: placement
	// relative to it) are their next-tick copies when the pass updates them first (earlier in the
	// list), as on the next tick. A morphing model's step (kind 5: 0x741120 -> morph) rewrites the
	// shared vertex blocks: SystemHeld puts the drawn ones back after the predictions
	enum { P_HOLD, P_NEXT, P_ENDS };
	static const uint32_t PART_BYTES = 0x6C;
	static const int MAX_PARTS = 128;
	struct NextPass
	{
		uint32_t part[MAX_PARTS], act[MAX_ACTORS];
		int np, na, cls[MAX_ACTORS];
		alignas(4) uint8_t pc[MAX_PARTS][PART_BYTES];
		alignas(4) uint8_t ac[MAX_ACTORS][ACTOR_BYTES];
		uint32_t part_copy(uint32_t real) const { for (int i = 0; i < np; i++) if (part[i] == real) return P(pc[i]); return 0; }
		int act_index(uint32_t real) const { for (int i = 0; i < na; i++) if (act[i] == real) return i; return -1; }
	};
	static NextPass g_next;

	// the motion step of actor `act` on its copy `nxt`
	static int StepActor(uint32_t act, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		const uint32_t parent = U32(nxt, 0x1BC);
		const uint32_t pc = parent ? g_next.part_copy(parent) : 0;
		if (pc) U32(nxt, 0x1BC) = pc;
		int r = P_NEXT;
		a_741050(nxt, def);
		// end test of 0x801740 (its follow-up spawn: not called)
		const int32_t kind = S8(def, 0x1E);
		bool ends = false;
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			ends = S16(nxt, 0x1C8) < 0;
		}
		if (!ends && (kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) ends = true;
		if (ends)
			r = P_ENDS;
		else
		{
			a_740910(nxt, def);
			a_741120(nxt, def);
			U16(nxt, 0x1CA) = (uint16_t)(U16(nxt, 0x1CA) + 1); // frame count of the step
		}
		U32(nxt, 0x1BC) = parent;
		return r;
	}

	// the placement step of particle `part` on its copy `nxt` (as 0x801890 without its emission / life count)
	static void StepParticle(uint32_t part, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)part, PART_BYTES);
		if (U16(part, 0x62) == 0) return; // (not initialised yet: none after a real tick)
		const int32_t mode = S8(nxt, 0x68);
		if (mode == 3)
		{
			const uint32_t owner = U32(nxt, 0x5C);
			const int oi = owner ? g_next.act_index(owner) : -1;
			if (oi >= 0) U32(nxt, 0x5C) = P(g_next.ac[oi]);
			a_7424C0(nxt);
			U32(nxt, 0x5C) = owner;
		}
		else if (mode == 4)
			tr_802390(nxt);
		U16(nxt, 0x60) = (uint16_t)(U16(nxt, 0x60) + 1);
	}

	static void NextTick(uint32_t sys)
	{
		g_next.np = g_next.na = 0;
		for (uint32_t node = U32(sys, 0x2C); node; node = U32(node, 4))
		{
			const int32_t type = S16(node, 8);
			if (type == 0 && g_next.np < MAX_PARTS)
			{
				const int i = g_next.np++;
				g_next.part[i] = node;
				StepParticle(node, P(g_next.pc[i]));
			}
			else if (type == 1 && g_next.na < MAX_ACTORS)
			{
				const int i = g_next.na++;
				g_next.act[i] = node;
				g_next.cls[i] = StepActor(node, P(g_next.ac[i]));
			}
		}
	}

	// the drawn actor's copy `cur` half way to its next-tick copy: draw matrix, view positions,
	// sub-positions; at fraction 1 the whole next actor. Holds (cur = the drawn actor): not in its
	// motion phase, ends on the next tick, sequence change before fraction 1
	static int Predict(uint32_t act, uint32_t cur, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		const int ai = g_next.act_index(act);
		if (ai < 0) return P_HOLD;
		if (g_next.cls[ai] != P_NEXT) return g_next.cls[ai];
		const uint32_t nxt = P(g_next.ac[ai]);
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
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
		return P_NEXT;
	}

	static void SystemHeld(const SysNote &n, const uint8_t *next_view, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
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
		if (next_view) memcpy((void *)CAM_COPY, next_view, 0x20); // (the system's scratch, camera snapshot included, is put back below)
		NextTick(n.sys);
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), num, den);
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch and morph
		// vertex blocks (the predictions above ran the morphs of the next keyframes)
		memcpy((void *)GTE_D, n.gte, GTE_D_N);
		memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), n.falloc, sizeof(n.falloc));
		n.verts.restore();
		if (num < den) U16(n.sys, 0x2A) = n.scroll; // the real tick's scroll (at fraction 1: the next tick's)
		// the sprite sequences, as 0x800480 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				tr_800540(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					tr_800540(c);
				}
			}
		}
		// the models, as 0x7FE210 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				tr_7FE270(P(copies[i]), def);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_057;
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
		for (int s = 0; s < g_nsys; s++)
			verts_now[s].save(g_sys[s].sys);
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
		uint8_t view[0x20];
		const bool next_view = g_nsys > 0 && NextView(view);
		for (int s = 0; s < g_nsys; s++)
			SystemHeld(g_sys[s], next_view ? view : nullptr, num, den);
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		for (int s = g_nsys - 1; s >= 0; s--)
			verts_now[s].restore();
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
	static void register_mag057_held()
	{
		mark_held(act::treatment::ORIG_ActorSystem);
		register_module_held(57, act::treatment::HeldReady, act::treatment::HeldFrame);
	}
}
