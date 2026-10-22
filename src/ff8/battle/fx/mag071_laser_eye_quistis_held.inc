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

// 30 fps part of mag071_laser_eye_quistis.cpp (Quistis' Laser Eye: Counter Laser-Eye 30 relocated): the held-frame drawing
// of the actor systems, the laser beams and the sparks (included at the end of mag071_laser_eye_quistis.cpp)
//
// Held frames (30 fps). Every drawing task of the module updates THEN draws in the same tick; the real tick notes, in
// drawing order, every draw with the state it started from (GTE registers, module scratch stack, Field_Alloc scratch);
// the held frame replays them in the same order:
//   - actor systems (task 0x7D66D0; the actor template of mag053_megapotion_held.inc): each noted system's particle
//     actors (sprite sequences 0x7D8980, prim models 0x7D72A0) are redrawn from copies whose draw matrix (+0xAC:
//     rotation / scale), view positions (+0xC0..+0xC8) and per-copy positions (+0x19C..) are half way to the next
//     tick's: the next tick's update pass (0x7D8A00) runs on copies of the system's particles and actors, in list
//     order (the particles' placement, the actors' motion step; no emission, no spawn), with the view the next tick
//     builds (NextView: from the live camera words; a fixed view: this tick's); at fraction 1 the whole next actor.
//     An actor ending on the next tick (not drawn at fraction 1) and an actor not in its motion phase hold; an actor
//     whose sequence (+0x170) changes on the next tick holds until fraction 1. Flipbook frame, model and colours
//     hold; a morphing model (kind 5, morph 0x7D9A70) keeps the mesh it was drawn with. The actors are placed by
//     key positions the director set-up (0x7D6760) takes ONCE from the caster and the target entities.
//   - laser beams (task 0x7DB5C0; Counter Laser-Eye's held part, fixed): the node and the rotation of each of its (up
//     to three) draws (0x7DB9B0: beam, head, flash). The beam starts at the caster's effect position 0xF0, a point of
//     her ANIMATED model (GetEffectSpawnPosition 0x502170 on her bone matrices): the next tick's draws are predicted
//     by running the task's update (laser_update: eye position, state, matrices; no sparks) on a copy of the node with
//     her bone matrices at her NEXT pose (pose_midpoint at 1, the look-ahead of the animation reader; then her real
//     pose rebuilt and the word 0x502170 writes (0x1D9770C) put back), the draws captured; each draw is redrawn with
//     its position, scale and rotation matrix half way to the same draw of the next tick (the flash faces the camera:
//     its rotation is the view's, predicted with the view the next tick draws with, NextView; model, colour and hidden
//     flag: the drawn tick's); at fraction 1 the next tick's draw as captured. A draw the next tick does not make
//     (the flash once the width is full, a finished or hidden beam) holds before fraction 1 and is not drawn at 1.
//     At a restart of her looped cast animation pose_midpoint holds the completed animation (as the battle model's
//     own held frames do): the predicted eye is then the completed animation's last pose (by design).
//   - sparks (task 0x7DBC10, sprite sequences 0x7DBC70): position half way to the next tick's (its state 1 step run
//     on a copy); flipbook frame held (the next one at fraction 1); a spark ending on the next tick holds before 1
//     and is not drawn at 1. Sparks born on the next tick are not drawn (random growth stays at 15 Hz).
// The caster task (0x7DB470: animation bind, sound, the two lasers), the master, the emitters and the stage light
// ramps (0x7D64B0 / 0x7DC050: the level words 0x1D98992 + k * 0x2C read by the stage renderer) draw nothing; the
// light ramp holds (15 Hz).
// The packets go to private buffers (module cursor 0x25FA40C, frame arena 0x1D8E054) that start with the real tick's
// packets from the cursors of the tick's first draw; the module globals, the module scratch stack, the Field_Alloc
// scratch (the bone rebuilds of the predictions included), the sprite camera words, the shadow / RenderGeometry
// temporaries and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace laser_eye_quistis
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25FA408, BSS_HI = 0x25FCDD8;
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

	// ---- the laser beams and the sparks (Counter Laser-Eye's held part) and the real tick's draw order
	static const uint32_t LASER_BYTES = 0x94, SPARK_BYTES = 0x60;
	static const uint32_t SPRITE_CAM = 0x21DFEE4, SPRITE_CAM_N = 12; // written by the sprite draws (0x571BC0)
	// the state a draw of the real tick started from
	struct DrawCtx
	{
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint32_t fa;             // Field_Alloc pointer
		uint8_t falloc[0x400];   // and the scratch above it
		void save()
		{
			memcpy(gte, (const void *)GTE_D, GTE_D_N);
			memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
			memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
			fa = MEM<uint32_t>(0x1D999C4);
			memcpy(falloc, (const void *)fa, sizeof(falloc));
		}
		void restore() const
		{
			memcpy((void *)GTE_D, gte, GTE_D_N);
			memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
			MEM<uint32_t>(0x1D999C4) = fa;
			memcpy((void *)fa, falloc, sizeof(falloc));
		}
	};
	struct LaserNote
	{
		uint32_t node;
		int idx;                 // draw index of the node on that tick (0 beam, 1 head, 2 flash)
		uint8_t bytes[LASER_BYTES];
		uint8_t mtx[0x20];
		DrawCtx st;
	};
	struct SparkNote
	{
		uint32_t node;
		uint8_t bytes[SPARK_BYTES];
		DrawCtx st;
	};
	enum { K_SYSTEM, K_LASER, K_SPARK };
	struct Order { int kind, idx; };
	static const int MAX_LASER = 64, MAX_SPARK = 128, MAX_ORDER = MAX_SYSTEMS + MAX_LASER + MAX_SPARK;
	static LaserNote g_laser[MAX_LASER];
	static SparkNote g_spark[MAX_SPARK];
	static Order g_order[MAX_ORDER];
	static int g_nlaser = 0, g_nspark = 0, g_norder = 0;
	static bool g_held_drawing = false;
	// the next tick's draws of a laser node (laser_update on a copy, with the caster at her next pose; draws captured)
	struct Captured { uint8_t bytes[LASER_BYTES]; uint8_t mtx[0x20]; DrawCtx st; };
	struct LaserNext { uint32_t node; int n; Captured cap[4]; };
	static const int MAX_LNEXT = 16;
	static LaserNext g_lnext[MAX_LNEXT];
	static int g_nlnext = 0;
	static LaserNext *g_capturing = nullptr;
	static void held_begin_tick();
	// a laser draw (0x7DB9B0): captured while predicting (not drawn), noted on a real tick
	static bool held_laser_draw(uint32_t node, uint32_t matrix)
	{
		if (g_capturing)
		{
			if (g_capturing->n < 4)
			{
				Captured &c = g_capturing->cap[g_capturing->n++];
				memcpy(c.bytes, (const void *)node, LASER_BYTES);
				memcpy(c.mtx, (const void *)matrix, 0x20);
				c.st.save();
			}
			return true;
		}
		if (g_held_drawing) return false;
		held_begin_tick();
		if (g_nlaser >= MAX_LASER || g_norder >= MAX_ORDER) return false;
		LaserNote &n = g_laser[g_nlaser];
		n.node = node;
		n.idx = 0;
		for (int i = 0; i < g_nlaser; i++)
			if (g_laser[i].node == node) n.idx++;
		memcpy(n.bytes, (const void *)node, LASER_BYTES);
		memcpy(n.mtx, (const void *)matrix, 0x20);
		n.st.save();
		g_order[g_norder++] = { K_LASER, g_nlaser++ };
		return false;
	}
	// a spark draw (0x7DBC70) on a real tick (after its state step)
	static void held_note_spark(uint32_t node)
	{
		if (g_held_drawing) return;
		held_begin_tick();
		if (g_nspark >= MAX_SPARK || g_norder >= MAX_ORDER) return;
		SparkNote &n = g_spark[g_nspark];
		n.node = node;
		memcpy(n.bytes, (const void *)node, SPARK_BYTES);
		n.st.save();
		g_order[g_norder++] = { K_SPARK, g_nspark++ };
	}

	static void held_begin_tick()
	{
		if (g_sys_tick == g_real_tick) return;
		g_nlaser = 0;
		g_nspark = 0;
		g_norder = 0;
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
		if (g_nsys >= MAX_SYSTEMS || g_norder >= MAX_ORDER) return;
		g_order[g_norder++] = { K_SYSTEM, g_nsys };
		SysNote &n = g_sys[g_nsys++];
		n.sys = sys;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		n.verts.save(sys);
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

	// the next tick's update pass (0x7D8A00: list order; particles = type 0, actors = type 1) run on
	// copies: a particle moves by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0;
	// 4 = key-position path 0x7DA7D0; its emission 0x7D9D50 and life count 0x741B60: not run), an actor
	// by its motion step (0x7D8A40: 0x741050, the end test of 0x7D9B80 without its follow-up spawn,
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
		// end test of 0x7D9B80 (its follow-up spawn: not called)
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

	// the placement step of particle `part` on its copy `nxt` (as 0x7D9CD0 without its emission / life count)
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
			cl_7DA7D0(nxt);
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
		// the sprite sequences, as 0x7D88C0 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				cl_7D8980(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					cl_7D8980(c);
				}
			}
		}
		// the models, as 0x7D7240 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				cl_7D72A0(P(copies[i]), def);
		}
	}

	// ---- laser beams (task 0x7DB5C0) and sparks (task 0x7DBC10)

	static bool live(uint32_t node, uint32_t task)
	{
		return (U16(node, 0) & 1) != 0 && U32(node, 8) == task;
	}

	// the next tick's draws of every noted laser node (once per held frame, before the replay): laser_update on a copy
	// of the live node, with the caster's bone matrices at her next pose (pose_midpoint at 1: the look-ahead of the
	// animation reader; the eye is her effect position 0xF0), the draws captured; then her real pose rebuilt and the word
	// GetEffectSpawnPosition writes (0x1D9770C) put back (the GTE registers and the Field_Alloc scratch the update and the
	// bone rebuilds write are put back by HeldFrame); the camera rotation the flash draw faces (UnpackRotationMatrix of the view
	// 0x1D97778) is the view the next tick draws with (`cam`: NextView, a fixed view: this tick's), put back after
	static void PredictLasers(const uint8_t *cam)
	{
		alignas(4) static uint8_t copy[LASER_BYTES];
		uint8_t view_now[0x20];
		memcpy(view_now, (const void *)0x1D97778, sizeof(view_now));
		memcpy((void *)0x1D97778, cam, sizeof(view_now));
		g_nlnext = 0;
		for (int i = 0; i < g_nlaser && g_nlnext < MAX_LNEXT; i++)
		{
			const uint32_t node = g_laser[i].node;
			bool seen = false;
			for (int k = 0; k < g_nlnext; k++)
				if (g_lnext[k].node == node) seen = true;
			if (seen) continue;
			LaserNext &L = g_lnext[g_nlnext++];
			L.node = node;
			L.n = 0;
			if (!live(node, ORIG_Laser)) continue;
			memcpy(copy, (const void *)node, LASER_BYTES);
			const uint32_t entity = 0x1D972C0 + (uint32_t)U8(U32(node, 0xC), 0) * 0x9C;
			const bool pose = (U8(entity, 0) & 2) != 0 && U32(entity, 0x64) != 0;
			const uint16_t w70c = MEM<uint16_t>(0x1D9770C);
			if (pose) pose_midpoint((void *)(entity + 0x60), (void *)(entity + 0x6C), 1, 1);
			g_capturing = &L;
			laser_update(P(copy));
			g_capturing = nullptr;
			if (pose) x::BattleModel_BuildBoneMatricesFromPose(entity + 0x60);
			MEM<uint16_t>(0x1D9770C) = w70c;
		}
		memcpy((void *)0x1D97778, view_now, sizeof(view_now));
	}

	static const LaserNext *NextOf(uint32_t node)
	{
		for (int k = 0; k < g_nlnext; k++)
			if (g_lnext[k].node == node) return &g_lnext[k];
		return nullptr;
	}

	// a laser draw half way to the same draw (index) of the next tick: position +0x1C..+0x20, scale +0x30..+0x3C and the
	// rotation matrix (model, colour, hidden flag: the drawn tick's); at fraction 1 the next tick's draw as captured, from
	// the state it would start from. No such draw on the next tick (or hidden there): held, not drawn at fraction 1
	static void LaserHeld(const LaserNote &n, int num, int den)
	{
		alignas(4) static uint8_t fake[LASER_BYTES];
		alignas(4) static uint8_t mtx[0x20];
		const LaserNext *L = NextOf(n.node);
		const bool next = L && n.idx < L->n && (U8(P(L->cap[n.idx].bytes), 0x26) & 4) == 0;
		if (!next && num >= den) return;
		if (next && num >= den)
		{
			const Captured &c = L->cap[n.idx];
			memcpy(fake, c.bytes, LASER_BYTES);
			memcpy(mtx, c.mtx, 0x20);
			c.st.restore();
		}
		else
		{
			memcpy(fake, n.bytes, LASER_BYTES);
			memcpy(mtx, n.mtx, 0x20);
			if (next)
			{
				const Captured &c = L->cap[n.idx];
				const uint32_t f = P(fake), b = P(c.bytes), m = P(mtx), bm = P(c.mtx);
				for (int k = 0; k < 3; k++)
					S16(f, 0x1C + 2 * k) = (int16_t)lerp_i(S16(f, 0x1C + 2 * k), S16(b, 0x1C + 2 * k), num, den);
				for (int k = 0; k < 4; k++)
					S32(f, 0x30 + 4 * k) = lerp_i(S32(f, 0x30 + 4 * k), S32(b, 0x30 + 4 * k), num, den);
				for (int k = 0; k < 9; k++)
					S16(m, 2 * k) = (int16_t)lerp_i(S16(m, 2 * k), S16(bm, 2 * k), num, den);
			}
			n.st.restore();
		}
		g_held_drawing = true;
		cl_7DB9B0(P(fake), P(mtx));
		g_held_drawing = false;
	}

	// a spark half way to its next tick's position (its state 1 step 0x7DBD40 on a copy); flipbook frame held (the next
	// one at fraction 1); a spark the next tick does not draw (finished on this tick: released) holds, not drawn at 1
	static void SparkHeld(const SparkNote &n, int num, int den)
	{
		alignas(4) static uint8_t cur[SPARK_BYTES], nxt[SPARK_BYTES];
		memcpy(cur, n.bytes, SPARK_BYTES);
		const uint32_t c = P(cur), x1 = P(nxt);
		const bool next = live(n.node, ORIG_Spark) && (U8(n.node, 0x26) & 1) == 0 && S8(n.node, 0x29) == 1;
		if (!next && num >= den) return;
		if (next)
		{
			memcpy(nxt, (const void *)n.node, SPARK_BYTES);
			cl_7DBD40(x1);
			for (int k = 0; k < 3; k++)
				S16(c, 0x1C + 2 * k) = (int16_t)lerp_i(S16(c, 0x1C + 2 * k), S16(x1, 0x1C + 2 * k), num, den);
			if (num >= den)
			{
				U16(c, 0x50) = U16(x1, 0x50);
				U32(c, 0x4C) = U32(x1, 0x4C);
				U16(c, 0x54) = U16(x1, 0x54);
				U8(c, 0x26) = U8(x1, 0x26);
			}
		}
		n.st.restore();
		g_held_drawing = true;
		cl_7DBC70(c);
		g_held_drawing = false;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_071;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		uint8_t sprite_cam[SPRITE_CAM_N];
		memcpy(sprite_cam, (const void *)SPRITE_CAM, SPRITE_CAM_N);
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
		const bool next_view = (g_nsys > 0 || g_nlaser > 0) && NextView(view);
		// the lasers' next ticks (caster at her next pose: bone rebuilds on the Field_Alloc scratch, put back)
		static uint8_t pose_scratch[0x8000];
		if (g_nlaser > 0)
		{
			memcpy(pose_scratch, (const void *)fa, sizeof(pose_scratch));
			PredictLasers(next_view ? view : g_cam_tick);
			memcpy((void *)fa, pose_scratch, sizeof(pose_scratch));
			MEM<uint32_t>(0x1D999C4) = fa;
		}
		for (int i = 0; i < g_norder; i++)
		{
			const Order &o = g_order[i];
			if (o.kind == K_SYSTEM)
				SystemHeld(g_sys[o.idx], next_view ? view : nullptr, num, den);
			else if (o.kind == K_LASER)
				LaserHeld(g_laser[o.idx], num, den);
			else
				SparkHeld(g_spark[o.idx], num, den);
		}
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		for (int s = g_nsys - 1; s >= 0; s--)
			verts_now[s].restore();
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		memcpy((void *)SPRITE_CAM, sprite_cam, SPRITE_CAM_N);
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
	static void register_mag071_held()
	{
		mark_held(act::laser_eye_quistis::ORIG_ActorSystem);
		mark_held(act::laser_eye_quistis::ORIG_Laser);
		mark_held(act::laser_eye_quistis::ORIG_Spark);
		register_module_held(71, act::laser_eye_quistis::HeldReady, act::laser_eye_quistis::HeldFrame);
	}
}
