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

// 30 fps part of mag087_angelo_strike.cpp: the held-frame drawing (included at the end of mag087_angelo_strike.cpp)
//
// Every drawing task of Angelo Strike updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw / held_note_system: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch
// it started from); the held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half
// way to the next tick's state (the task's own update predicted on a copy; no draw, no spawn, no sound, no random number, no
// entity or camera write).
//   Angelo      - the creature 0x7751A0, drawn by the creature renderer 0x775330 (a_746C10) with the bone matrices of his current
//                 pose. Next-pose approach: the next tick's states run on a copy of the node (their animation reads 0x8DD1C0 /
//                 0x8DD220 / 0x8DD190 on the copy's animation header; the shared pose block is put back; his run / lift / jump
//                 (0x7762D0) and carry (0x77E260's own part) on the copy; the module pause switch of the next tick predicted (his
//                 state 2 stops while it is set); the target's transform, Rinoa's chain transformation, the spawns, the ground
//                 capture and the sounds are not run), then his velocity: the exact next pose, position, angles and visibility.
//                 Held: the bone matrices of the pose half way (each angle the short way round, as pose_midpoint), position
//                 (+0x4C..) and angles (+0x3C.., 12 bits, the short way) half way; at 1 the next pose. An animation change /
//                 restart (header +0x90.., animation +0x9C, loop count +0x138) = discrete switch (this pose below 1). Hidden or
//                 released on the next tick: held below 1, not drawn at 1.
//   Plays       - the two dust plays 0x7764B0 and the impact play 0x77E6C0 (a_73C280 copy 0x776580, object callback 0x7766B0):
//                 fixed placements; prim::play_held (objects half way to their next keyframe records). Released or finished on
//                 the next tick: held below 1, not drawn at 1. The plays' pause word 0x25AECD4 (a_73BB60 while the director waits
//                 for the loads): a play paused on this tick holds (prim::play_held: not moving); no prediction of the next
//                 tick's word (prim::play_held takes the next tick as unpaused).
//   Rings       - the ring particles 0x775720 (prim sequence drawn by 0x775780): position half way to their next step (state 1
//                 0x775E70 on a copy); the sequence frame switches on the real frame; the sequence's end hides it at 1.
//   Models      - the halo 0x77DFC0 and the debris 0x77EAB0 / 0x77EC00 (drawn by 0x77E060): their next step on a copy (the halo's
//                 states, spin +0x46 += 0x100, height +0x34 = Angelo's bone point 0x1D on the next tick through his next pose,
//                 position and the next camera block; the debris' finish, gravity, tumble +0x44 and motion); spin / tumble (12
//                 bits, the short way), height, fade +0x50 and position half way.
//   Actor systems - the four 0x779050 systems (a_73C100: the act-library actor template: every drawn actor from a copy half
//                 way to its next motion step, the view of the next tick from the live camera (NextView), the +0x170 ENDS rule,
//                 no actor drawn at 1 when the system leaves phase 1).
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. Spawns, sounds, random numbers, sync requests and entity writes stay on the real ticks; the ground capture (0x778AB0, a
// one-tick render of the stage into a texture) is not redrawn.
// Camera: the module's camera script (camera task 0x7743A0 through the library stepper a_73AE10 on the camera block [0x155B1E0]):
// HeldCamera = act::held_camera (the stepper run once more on the saved camera block / words / GTE / Field_Alloc scratch, eye and
// look-at half way; a cut holds), held when the camera task is in its states 2 / 3 and the next tick's pause switch is set.
// Every part is placed through the frame's camera (0x1D97778, the frame gate's); the actor systems through the module's camera
// copy set to the next tick's view (NextView); the halo's next height through the next tick's camera block (NextCamBlock); the
// camera block, the Field_Alloc scratch and the GTE registers are put back.
// The module's pause switch on the next tick (0x25B4B4C, a_73BB60): the director runs first in the master's queues; in its state
// 3 (0x775010), from its frame 0x1C (the next tick's: its bookkeeping counts first), it is set while the loads run (loader
// block +0x166) and cleared when they are done; the loads are taken as they are now (a load completing before the next tick
// switches on the real frame).
// Not drawn by the module and held at 15 Hz by the engine: the root-matrix slot 0x1D99AB0, Rinoa's entity (bound animation set,
// chain transformations 1 / 2), the target's entity (carried by Angelo: its position / angles / matrix, written by his real
// ticks only).
// The packets go to private copies of the three packet arenas (module cursors 0x25ACEB4 / 0x25ACFBC, frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch, the
// shadow / RenderGeometry temporaries, Angelo's pose block, the actor data's morph vertex blocks, the CRT rand seed and the GTE
// registers are put back.

namespace ff8fx
{
namespace act
{
namespace strike87
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25ACEB0, BSS_HI = 0x25B4CC4;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x25ACEB4, CUR_B = 0x25ACFBC, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Angelo = 0x7751A0, ORIG_DustPlay = 0x7764B0, ORIG_ImpactPlay = 0x77E6C0, ORIG_Ring = 0x775720;
	static const uint32_t ORIG_Halo = 0x77DFC0, ORIG_Debris1 = 0x77EAB0, ORIG_Debris2 = 0x77EC00, ORIG_ActorSystem = 0x779050;
	static const uint32_t ORIG_Director = 0x774240, ORIG_Camera = 0x7743A0;
	static const uint32_t PLAY_BLEND = 0x25B46A8;     // the plays' vertex lerp scratch (G_258EF98: block +0x48)
	static const uint32_t LOADER_PTR = 0x25B4448;     // the file loader block (+0x166: loads running)
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t ANGELO_BYTES = 0x178;
	static const uint32_t NODE_MAX = 0x260; // the largest node a draw reads (the plays 0x25C)
	static const int MAX_BONES = 256;
	static const uint32_t SK_MAX = 16 + 48 * MAX_BONES;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a kind-5 model's step
	// rewrites them in the actor data, which every actor system of the cast shares
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

	// ---------------------------------------------------------------------------------------------------------------
	// Angelo's skeleton: the battle model's animation header (node +0x90: +4 -> its pose block: bone count +0, root
	// position +8 (s16 x3), 48-byte bone records from +16: angles +4 (s16 x3, + scale x3 when flag +1 bit 0), the bone
	// matrix built from them)
	// ---------------------------------------------------------------------------------------------------------------
	static uint32_t sk_of(uint32_t node)
	{
		const uint32_t com = U32(node, 0x94);
		return com ? U32(com, 0) : 0;
	}
	static uint32_t sk_size(uint32_t sk)
	{
		if (!sk) return 0;
		const uint32_t nb = U8(sk, 0);
		return nb == 0 ? 0 : 16 + 48 * nb;
	}
	// the pose values (root position + every bone's angles / scales) of a pose block
	struct Pose
	{
		int nb = 0;
		bool scaled = false;
		int16_t root[3] = {};
		int16_t v[MAX_BONES][6] = {};
		void get(uint32_t sk)
		{
			nb = (int)U8(sk, 0);
			scaled = (U8(sk, 1) & 1) != 0;
			memcpy(root, (const void *)(sk + 8), 6);
			for (int b = 0; b < nb; b++) memcpy(v[b], (const void *)(sk + 16 + 48 * b + 4), scaled ? 12 : 6);
		}
		void set(uint32_t sk) const
		{
			memcpy((void *)(sk + 8), root, 6);
			for (int b = 0; b < nb; b++) memcpy((void *)(sk + 16 + 48 * b + 4), v[b], scaled ? 12 : 6);
		}
	};
	static int pm_round(int v, int num, int den) { const int t = v * num; return (t >= 0) ? (t + den / 2) / den : -((-t + den / 2) / den); }
	// the pose at num / den from a to b (as pose_midpoint: a bone rotation (x, y, z) is also written (2048 - x, y + 2048,
	// z + 2048); each bone blends toward the spelling of b nearer a, each angle the shortest way round)
	static void pose_lerp(const Pose &a, const Pose &b, int num, int den, Pose &out)
	{
		out = a;
		for (int k = 0; k < 3; k++) out.root[k] = (int16_t)(a.root[k] + pm_round(b.root[k] - a.root[k], num, den));
		for (int i = 0; i < a.nb; i++)
		{
			int dd[3], ee[3], sd = 0, se = 0;
			for (int k = 0; k < 3; k++) { dd[k] = ((b.v[i][k] - a.v[i][k] + 2048) & 4095) - 2048; sd += dd[k] < 0 ? -dd[k] : dd[k]; }
			ee[0] = ((2048 - b.v[i][0] - a.v[i][0] + 2048) & 4095) - 2048;
			ee[1] = ((b.v[i][1] - a.v[i][1]) & 4095) - 2048;
			ee[2] = ((b.v[i][2] - a.v[i][2]) & 4095) - 2048;
			for (int k = 0; k < 3; k++) se += ee[k] < 0 ? -ee[k] : ee[k];
			const int *d = se < sd ? ee : dd;
			for (int k = 0; k < 3; k++) out.v[i][k] = (int16_t)(a.v[i][k] + pm_round(d[k], num, den));
			if (a.scaled) for (int k = 3; k < 6; k++) out.v[i][k] = (int16_t)(a.v[i][k] + pm_round(b.v[i][k] - a.v[i][k], num, den));
		}
	}

	// one draw of the real tick (execution order)
	struct Rec
	{
		int kind;
		uint32_t node;              // the task node (HK_SYSTEM: the actor state block)
		uint8_t snap[NODE_MAX];     // the node as drawn
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int sysi;                   // HK_SYSTEM: its vertex-block note
	};
	static const int MAX_RECS = 256, MAX_SYSTEMS = 8;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS];
	static int g_nsys = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[3], g_end[3];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[3] = { CUR_A, CUR_B, CUR_FA };
	static uint8_t g_priv[3][0x40000];
	static uint8_t g_cam_tick[0x20];                       // the module's camera copy this real tick drew with
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes
	static uint32_t g_director = 0, g_director_tick = 0xFFFFFFFF; // the director task's node (noted by its task each tick)
	static uint32_t g_camtask = 0, g_camtask_tick = 0xFFFFFFFF;   // the camera task's node

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_DustPlay || f == ORIG_ImpactPlay) return 0x25C;
		if (f == ORIG_Ring) return 0x90;
		if (f == ORIG_Halo || f == ORIG_Debris1 || f == ORIG_Debris2) return 0x98;
		return ANGELO_BYTES; // Angelo
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = g_nsys = 0;
		g_full = false;
		for (int i = 0; i < 3; i++) g_start[i] = g_end[i] = MEM<uint32_t>(CURSORS[i]);
		memcpy(g_cam_tick, (const void *)CAM_COPY, sizeof(g_cam_tick));
	}

	static Rec *new_rec(int kind, uint32_t node)
	{
		if (g_in_held) return nullptr;
		begin_tick();
		if (g_nrec >= MAX_RECS) { g_full = true; return nullptr; }
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.sysi = -1;
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)SCRATCH_LO, sizeof(r.scratch));
		memcpy(r.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(r.falloc));
		return &r;
	}

	static void held_note_draw(int kind, uint32_t node)
	{
		Rec *r = new_rec(kind, node);
		if (!r) return;
		memcpy(r->snap, (const void *)node, node_size(node));
	}

	static void held_note_system(uint32_t sys)
	{
		Rec *r = new_rec(HK_SYSTEM, sys);
		if (!r || g_nsys >= MAX_SYSTEMS) { if (r) { g_nrec--; g_full = true; } return; }
		r->sysi = g_nsys;
		g_sys_verts[g_nsys++].save(sys);
	}

	static void held_note_director(uint32_t node)
	{
		if (g_in_held) return;
		g_director = node;
		g_director_tick = g_real_tick;
	}

	static void held_note_camera(uint32_t node)
	{
		if (g_in_held) return;
		g_camtask = node;
		g_camtask_tick = g_real_tick;
	}

	static void held_note_end()
	{
		if (g_in_held) return;
		begin_tick();
		for (int i = 0; i < 3; i++) g_end[i] = MEM<uint32_t>(CURSORS[i]);
		g_end_tick = g_real_tick;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the next tick's state of a node (its task's update predicted on a copy: no draw, no spawn, no sound)
	// ---------------------------------------------------------------------------------------------------------------
	// the task of a node drawn on the real tick (`snap`: as drawn, before its frame counter +0x24 counted the tick) runs on the
	// next tick: its node still holds it (same task, counter one on) and it was not released after the tick (finished with no
	// child alive: the task tail returns 2)
	static bool still_runs(uint32_t snap, uint32_t now)
	{
		return U32(now, 8) == U32(snap, 8) && S16(now, 0x24) == (int16_t)(S16(snap, 0x24) + 1) && !((U8(now, 0x26) & 1) && U8(now, 0x28) == 0);
	}

	// the module's pause switch 0x25B4B4C (a_73BB60) on the next tick: the director runs first in the master's queues; in its
	// state 3 (0x775010) from its next frame 0x1C (its bookkeeping 0x7742E0 counts +0x46 first: 0 on a phase change) it is set
	// while the loads run (loader block +0x166, taken as now) and cleared when they are done; else as now
	static uint32_t pause_next()
	{
		const uint32_t now = MEM<uint32_t>(PAUSE_SWITCH);
		const uint32_t dn = g_director;
		if (g_director_tick != g_real_tick || !dn || U32(dn, 8) != ORIG_Director || S8(dn, 0x29) != 3) return now;
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const int16_t f = S16(d, 0x40) != S16(d, 0x42) ? 0 : (int16_t)(S16(d, 0x46) + 1);
		if (f < 0x1C) return now;
		return U16(MEM<uint32_t>(LOADER_PTR), 0x166) != 0 ? 1 : 0;
	}

	// Angelo's carry on the next tick: 0x77E260's own part (the factor: a2 == 0 the accelerating slam, else the eased table
	// 0x1561744; position from +0x140.. to +0x148..; y += the height +0x16E); the target's transform is not touched
	static void carry_next(uint32_t n, int a2)
	{
		if (a2 == 0)
		{
			U16(n, 0x16A) = (uint16_t)(U16(n, 0x16A) + U16(n, 0x16C));
			U16(n, 0x168) = (uint16_t)(U16(n, 0x168) + U16(n, 0x16A));
		}
		else
		{
			const int16_t i = S16(n, 0x174);
			const uint16_t f = MEM<uint16_t>(0x1561744 + (uint32_t)(int32_t)i * 2);
			U16(n, 0x168) = f;
			if (f != 0x1000) U16(n, 0x174) = (uint16_t)(i + 1);
		}
		const int16_t f = S16(n, 0x168);
		if (f >= 0x1000)
		{
			U32(n, 0x4C) = U32(n, 0x148);
			U16(n, 0x168) = 0x1000;
			U32(n, 0x50) = U32(n, 0x14C);
		}
		else
		{
			const int32_t k = f;
			for (uint32_t o = 0; o < 6; o += 2)
			{
				const int16_t a = S16(n, 0x140 + o);
				U16(n, 0x4C + o) = (uint16_t)(((int32_t)S16(n, 0x148 + o) - a) * k / 4096 + a);
			}
		}
		U16(n, 0x4E) = (uint16_t)(U16(n, 0x4E) + U16(n, 0x16E));
	}

	// Angelo's next tick (0x7751A0's states, then his velocity) on the copy `nx` of his node: the animation reads run on the copy's
	// animation header (+0x90) and write the shared pose block (the caller puts it back). switched = the animation changes or
	// restarts (header +0x90..+0x9B, animation +0x9C or loop count +0x138 differ). false = the next tick does not draw him (hidden)
	static bool angelo_next(uint32_t nx, bool &switched)
	{
		uint8_t hdr0[0x10];
		memcpy(hdr0, (const void *)(nx + 0x90), 0xD);
		const int16_t loops0 = S16(nx, 0x138);
		const int8_t st0 = S8(nx, 0x29);
		switch (st0)
		{
		case 1: ak_7760E0(nx); break;                // from frame 0xF: placed, shown, animation 0 (reads the target only)
		case 2: // 0x776260: unless the next tick is paused: height, run, animation step; from frame 0x14 animation 0xA
			if (pause_next() == 0)
			{
				U16(nx, 0x170) = (uint16_t)(U16(nx, 0x170) + U16(nx, 0x172));
				U16(nx, 0x16E) = (uint16_t)(U16(nx, 0x16E) + U16(nx, 0x170));
				if (S16(nx, 0x16E) > 0) U16(nx, 0x16E) = 0;
				ak_7762D0(nx);
				x::sub_8DD1C0(nx);
				if (S16(nx, 0x136) >= 0x14)
				{
					x::au_re_Battle_ReadAnimation_7(nx, 0xA);
					U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				}
			}
			break;
		case 3: // 0x7763B0: height, run; at the animation's end animation 1 (the play, the capture, the sound: not run)
			U16(nx, 0x170) = (uint16_t)(U16(nx, 0x170) + U16(nx, 0x172));
			U16(nx, 0x16E) = (uint16_t)(U16(nx, 0x16E) + U16(nx, 0x170));
			if (S16(nx, 0x16E) > 0) U16(nx, 0x16E) = 0;
			ak_7762D0(nx);
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 1);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 4: ak_778E90(nx); break;                // at the animation's end: the lift towards the target's point (reads the target)
		case 5: // 0x778F90: the lift; at the animation's end animation 3 (the actor systems: not spawned)
			ak_7762D0(nx);
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 3);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 6: // 0x77DE70: at the animation's end animation 4, 0x200 higher, the carry set up and its first eased step (the halo,
			// the play and the target: not run)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 4);
				U16(nx, 0x4E) = (uint16_t)(U16(nx, 0x4E) + 0xFE00);
				U32(nx, 0x148) = U32(nx, 0x4C);
				U32(nx, 0x140) = U32(nx, 0x4C);
				U32(nx, 0x14C) = U32(nx, 0x50);
				U32(nx, 0x144) = U32(nx, 0x50);
				U16(nx, 0x168) = 0;
				U16(nx, 0x174) = 0;
				U16(nx, 0x3C) = 0xC00;
				U16(nx, 0x14A) = 0xE800;
				U16(nx, 0x134) = 8;
				carry_next(nx, 1);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 7: // 0x77E490: the eased carry, animation step, countdown
			carry_next(nx, 1);
			x::sub_8DD1C0(nx);
			U16(nx, 0x134) = (uint16_t)(U16(nx, 0x134) - 1);
			if (S16(nx, 0x134) <= 0)
			{
				U16(nx, 0x134) = 0;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 8: // 0x77E4D0: the spin; after 4 turns the slam set up (from / to swapped), else the eased carry; animation step
		{
			U16(nx, 0x176) = (uint16_t)(U16(nx, 0x176) - 0x80);
			if (S16(nx, 0x176) <= (int16_t)0xFD00) U16(nx, 0x176) = 0xFD00;
			U16(nx, 0x3C) = (uint16_t)(U16(nx, 0x3C) + U16(nx, 0x176));
			const uint16_t p = U16(nx, 0x3C);
			if ((int16_t)p < 0)
			{
				U16(nx, 0x134) = (uint16_t)(U16(nx, 0x134) + 1);
				U16(nx, 0x3C) = (uint16_t)(p & 0xFFF);
			}
			U16(nx, 0x3C) = (uint16_t)(U16(nx, 0x3C) & 0xFFF);
			if (S16(nx, 0x134) >= 4)
			{
				U16(nx, 0x3C) = 0;
				x::au_re_Battle_ReadAnimation_7(nx, 5);
				const uint32_t e0 = U32(nx, 0x148), s0 = U32(nx, 0x140), s1 = U32(nx, 0x144), e1 = U32(nx, 0x14C);
				U32(nx, 0x140) = e0;
				U32(nx, 0x148) = s0;
				U32(nx, 0x14C) = s1;
				U32(nx, 0x144) = e1;
				U16(nx, 0x14A) = 0;
				U16(nx, 0x168) = 0;
				U16(nx, 0x16A) = 0;
				U16(nx, 0x16C) = 0xC0;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			else
				carry_next(nx, 1);
			x::sub_8DD1C0(nx);
			break;
		}
		case 9: // 0x77E600: animation step, the accelerating slam; when it lands animation 6 (the play, the debris, the sound: not
			// run)
			x::sub_8DD1C0(nx);
			carry_next(nx, 0);
			if (U16(nx, 0x168) == 0x1000)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 6);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 10: ak_77ED50(nx); break;               // at the animation's end: the jump back to Rinoa (reads the caster)
		case 11: ak_77EDF0(nx); break;
		case 12: ak_77EE50(nx); break;
		case 13: // 0x77EEC0: height, the jump; at the animation's end animation 8 (the target's transform and Rinoa's chain
			// transformation 2: not run)
			U16(nx, 0x170) = (uint16_t)(U16(nx, 0x170) + U16(nx, 0x172));
			U16(nx, 0x16E) = (uint16_t)(U16(nx, 0x16E) + U16(nx, 0x170));
			if (S16(nx, 0x16E) > 0) U16(nx, 0x16E) = 0;
			ak_7762D0(nx);
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 8);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 14: ak_77EF90(nx); break;
		case 15: ak_77F030(nx); break;
		default: // 0 (placed hidden: never drawn), 16 (ret)
			break;
		}
		switched = memcmp(hdr0, (const void *)(nx + 0x90), 0xD) != 0 || S16(nx, 0x138) != loops0;
		U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + U16(nx, 0x128));
		U16(nx, 0x4C) = (uint16_t)(U16(nx, 0x4C) + U16(nx, 0x124));
		U16(nx, 0x4E) = (uint16_t)(U16(nx, 0x4E) + U16(nx, 0x126));
		return (U8(nx, 0x26) & 4) == 0;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the camera of the next tick
	// ---------------------------------------------------------------------------------------------------------------
	// the battle camera words a view is built from (0x5033E0): eye 0xB8B7F0, look-at 0xB8B7F8, roll 0x1D977A2, shake 0x1D97710
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
	// the words after each real tick (noted at its held frames: HeldReady): the view of the next tick is built from them
	static CamWords g_cw_held, g_cw_prev;
	static uint32_t g_cw_held_tick = 0xFFFFFFFF, g_cw_prev_tick = 0xFFFFFFFF;
	static void note_cam_words()
	{
		if (g_cw_held_tick == g_real_tick) return;
		g_cw_prev = g_cw_held;
		g_cw_prev_tick = g_cw_held_tick;
		g_cw_held.io(false);
		g_cw_held_tick = g_real_tick;
	}

	// the camera block (0x1D97700, 0xB0 bytes) of the next tick: the battle builds a tick's view (0x5033E0) from the camera words
	// the tick before left, so with a live camera (this tick's view = the one built from the words after the previous tick) the
	// next block is the one built from the words now; false = no live camera noted (the current block is the next one). Camera
	// block, words, GTE registers and the Field_Alloc scratch are put back.
	static bool NextCamBlock(uint8_t out[0xB0])
	{
		if (g_cw_prev_tick + 1 != g_real_tick) return false;
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
			memcpy(out, (const void *)0x1D97700, 0xB0);
		}
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)0xB8B7F0, words, sizeof(words));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		MEM<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, falloc, sizeof(falloc));
		return live;
	}

	// the view the next tick draws with (the camera matrix 0x1D97778 of NextCamBlock's block); false = a fixed view (this tick's)
	static bool NextView(uint8_t out[0x20])
	{
		alignas(4) static uint8_t blk[0xB0];
		if (!NextCamBlock(blk)) return false;
		memcpy(out, blk + 0x78, 0x20);
		return true;
	}

	// Angelo's bone point `bone` on the next tick (the halo runs after his draw): his model matrix as the renderer builds it
	// (0x775330: rotation +0x3C, the next position, scale +0x114) and the next pose's bone matrices (set by the caller), through
	// GetEffectSpawnPosition with the next tick's camera block. The camera block and the Field_Alloc scratch are put back
	static void angelo_point_next(uint32_t angelo_nx, uint32_t bone, uint32_t out)
	{
		alignas(4) static uint8_t cam[0xB0], ncam[0xB0];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		static uint8_t falloc[0x400];
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		if (fa) memcpy(falloc, (const void *)fa, sizeof(falloc));
		if (NextCamBlock(ncam)) memcpy((void *)0x1D97700, ncam, sizeof(ncam));
		const uint32_t mdl = angelo_nx + 0x30;
		x::ComposeZYXRotationMatrix(mdl + 0xC, mdl + 0x40);
		S32(mdl, 0x54) = S16(mdl, 0x1C);
		S32(mdl, 0x58) = S16(mdl, 0x1E);
		S32(mdl, 0x5C) = S16(mdl, 0x20);
		x::scale3DMatrix(mdl + 0x40, angelo_nx + 0x114);
		x::GetEffectSpawnPosition(mdl, bone, 0, out);
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		MEM<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, falloc, sizeof(falloc));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// held draws
	// ---------------------------------------------------------------------------------------------------------------
	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }
	static void lerp32(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S32(mid, o) = lerp_i(S32(a, o), S32(b, o), num, den); }
	// a 12-bit angle at num / den from a to b (the short way round 0x1000)
	static void lerp_a12(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den)
	{
		const int32_t s = U16(a, o) & 0xFFF, d = ((int32_t)(U16(b, o) & 0xFFF) - s + 0x800 & 0xFFF) - 0x800;
		U16(mid, o) = (uint16_t)((s + d * num / den) & 0xFFF);
	}

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];
	// Angelo's next-tick copy and pose (the halo follows his bone point on the next tick)
	alignas(4) static uint8_t g_angelo_nx[ANGELO_BYTES];
	static Pose g_angelo_next;
	static bool g_angelo_known = false;
	static uint32_t g_angelo_node = 0;

	// Angelo at num / den: the bone matrices of the pose half way to the next tick's, position and angles half way
	static void CreatureHeld(const Rec &r, int num, int den)
	{
		g_angelo_known = false;
		const uint32_t sk = sk_of(r.node), skn = sk_size(sk);
		if (skn == 0 || skn > SK_MAX) return;
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, ANGELO_BYTES); // the node as the real tick left it
		memcpy(g_mid, r.snap, ANGELO_BYTES);
		static uint8_t sk_now[SK_MAX];
		static Pose now, next, half;
		memcpy(sk_now, (const void *)sk, skn); // the pose block as the real tick left it (angles + bone matrices)
		now.get(sk);
		bool switched = false;
		const bool draws = still_runs(cur, nx) && angelo_next(nx, switched);
		next.get(sk);                           // the next tick's pose (the copy's animation reads)
		memcpy((void *)sk, sk_now, skn);
		memcpy(g_angelo_nx, g_nx, sizeof(g_angelo_nx));
		g_angelo_next = next;
		g_angelo_known = draws;
		g_angelo_node = r.node;
		if (num >= den && !draws) return;
		if (draws)
		{
			if (num >= den)
			{
				memcpy(g_mid, g_nx, ANGELO_BYTES);
				next.set(sk);
				x::BattleModel_BuildBoneMatricesFromPose(r.node + 0x90);
			}
			else
			{
				for (uint32_t o = 0x4C; o <= 0x50; o += 2) lerp16(mid, cur, nx, o, num, den);
				for (uint32_t o = 0x3C; o <= 0x40; o += 2) lerp_a12(mid, cur, nx, o, num, den);
				if (!switched && next.nb == now.nb)
				{
					pose_lerp(now, next, num, den, half);
					half.set(sk);
					x::BattleModel_BuildBoneMatricesFromPose(r.node + 0x90);
				}
			}
			now.set(sk); // (the renderer rebuilds the bone matrices from the pose after its render: the real pose)
		}
		MEM<uint32_t>(CUR_A) = a_746C10(mid, CREATURE_ARG, MEM<uint32_t>(CUR_A));
		memcpy((void *)sk, sk_now, skn);
	}

	// Angelo's bone point `bone` on the next tick (his next copy and pose) into out (s16 x3); false = not known (hidden or
	// released)
	static bool angelo_point_known(uint32_t model_ptr, uint32_t bone, uint32_t out)
	{
		if (!g_angelo_known || model_ptr != g_angelo_node + 0x30) return false;
		const uint32_t angelo = g_angelo_node;
		const uint32_t sk = sk_of(angelo), skn = sk_size(sk);
		if (!skn || skn > SK_MAX) return false;
		static uint8_t sk_now[SK_MAX];
		memcpy(sk_now, (const void *)sk, skn);
		g_angelo_next.set(sk);
		x::BattleModel_BuildBoneMatricesFromPose(angelo + 0x90);
		alignas(4) static uint8_t an[ANGELO_BYTES];
		memcpy(an, g_angelo_nx, sizeof(an));
		angelo_point_next(P(an), bone, out);
		memcpy((void *)sk, sk_now, skn);
		return true;
	}

	// a play (0x776580 = a_73C280) of the node `live` with the placement of `place`, in between
	static void PlayHeld(uint32_t live, uint32_t place, int num, int den)
	{
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		int16_t ang = S16(place, 0x62);
		if (ang != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)ang);
		ang = S16(place, 0x60);
		if (ang != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)ang);
		ang = S16(place, 0x64);
		if (ang != 0) x::sub_8DD960(L, (uint32_t)(int32_t)ang);
		x::scale3DMatrix(L, place + 0x50);
		S32(L, 0x14) = S16(place, 0x1C);
		S32(L, 0x18) = S16(place, 0x1E);
		S32(L, 0x1C) = S16(place, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(place, 0x86);
		U32(L, 0x44) = U32(place, 0x7C);
		U16(L, 0x4C) = U16(place, 0x8C);
		U16(L, 0x4E) = U16(place, 0x8E);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x50) = U16(place, 0x90);
		U32(L, 0x48) = PLAY_BLEND;
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)ak_7766B0, (int)L, num, den);
	}

	// a noted play at num / den: fixed placement; the next tick plays it (state 1) unless its task was released (see the file
	// comment for the pause word)
	static void ItemHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x25C);
		memcpy(g_mid, r.snap, 0x25C);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (num >= den && !draws) return;
		PlayHeld(r.node, mid, draws ? num : 0, den);
	}

	// a ring particle (0x775720, draw 0x775780) at num / den: position half way to its next step (its state 1 0x775E70 on a copy:
	// sequence frame, rise, damping); the sequence frame (+0x50) switches on the real frame
	static void RingHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x90);
		memcpy(g_mid, r.snap, 0x90);
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws)
		{
			ak_775E70(nx);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) ak_775780(nx); return; }
		if (draws) for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		ak_775780(mid);
	}

	// the halo's next tick on the copy `nx` (0x77DFC0: its state, spin, height = - Angelo's bone point 0x1D on the next tick)
	static void halo_next(uint32_t nx)
	{
		switch (S8(nx, 0x29))
		{
		case 1: ak_77E1E0(nx); break;
		case 2: ak_77E210(nx); break;
		case 3: ak_77E220(nx); break;
		default: break; // (0: placed on its first tick; 4: ret)
		}
		U16(nx, 0x46) = (uint16_t)((U16(nx, 0x46) + 0x100) & 0xFFF);
		alignas(4) uint8_t pt[8] = {};
		if (angelo_point_known(U32(nx, 0x8C), 0x1D, P(pt)))
			S32(nx, 0x34) = -(int32_t)S16(P(pt), 2);
	}

	// a debris piece's next tick on the copy `nx` (0x77EAB0 / 0x77EC00: finished at frame 0x19 / 0x10; gravity, tumble, motion)
	static void debris_next(uint32_t nx)
	{
		const bool big = U32(nx, 8) == ORIG_Debris1;
		if (S8(nx, 0x29) == 1)
		{
			if (big) ak_77EBD0(nx);
			else ak_77ED10(nx);
		}
		const int32_t vx = S16(nx, 0x7C);
		U16(nx, 0x7E) = (uint16_t)(U16(nx, 0x7E) + (big ? 0x200 : 0x400));
		U16(nx, 0x44) = (uint16_t)((U16(nx, 0x96) + U16(nx, 0x44)) & 0xFFF);
		U16(nx, 0x1C) = (uint16_t)(U16(nx, 0x1C) + (uint16_t)(vx / 16));
		U16(nx, 0x1E) = (uint16_t)(U16(nx, 0x1E) + (uint16_t)((int32_t)S16(nx, 0x7E) / 16));
		U16(nx, 0x20) = (uint16_t)(U16(nx, 0x20) + (uint16_t)((int32_t)S16(nx, 0x80) / 16));
	}

	// a model piece (the halo / a debris piece, draw 0x77E060) at num / den: its next step on a copy; spin +0x46 / tumble +0x44
	// (12 bits, the short way), height +0x34, fade +0x50 and position half way; at 1 the next state
	static void ModelHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x98);
		memcpy(g_mid, r.snap, 0x98);
		const bool halo = U32(cur, 8) == ORIG_Halo;
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) >= 1;
		if (draws)
		{
			if (halo) halo_next(nx);
			else debris_next(nx);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) ak_77E060(nx); return; }
		if (draws)
		{
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
			lerp_a12(mid, cur, nx, 0x44, num, den);
			lerp_a12(mid, cur, nx, 0x46, num, den);
			lerp32(mid, cur, nx, 0x34, num, den);
			lerp16(mid, cur, nx, 0x50, num, den);
		}
		ak_77E060(mid);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// actor systems: the act-library actor template (Mega Phoenix / X-Potion / The End family; module actor state ACTOR_STATE)
	// ---------------------------------------------------------------------------------------------------------------
	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x77B460: list order; particles = type 0, actors = type 1) run on copies: a particle moves
	// by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0; 4 = key-position path 0x77D230; its emission
	// and life count: not run), an actor by its motion step (0x741050, the end test without its follow-up spawn, 0x740910,
	// 0x741120). A particle's owner and an actor's parent particle (+0x1BC) are their next-tick copies when the pass updates
	// them first, as on the next tick. A morphing model's step rewrites the shared vertex blocks: SystemHeld puts the drawn ones
	// back after the predictions.
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
			ak_77D230(nxt);
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

	// the drawn actor's copy `cur` half way to its next-tick copy: draw matrix, view positions, sub-positions; at fraction 1
	// the whole next actor. Holds: not in its motion phase, ends on the next tick, sequence change before fraction 1
	static int Predict(uint32_t act, uint32_t cur, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		const int ai = g_next.act_index(act);
		if (ai < 0) return P_HOLD;
		if (g_next.cls[ai] != P_NEXT) return g_next.cls[ai];
		const uint32_t nxt = P(g_next.ac[ai]);
		if (U32(nxt, 0x170) != U32(act, 0x170) && num < den) return P_HOLD;
		for (int k = 0; k < 9; k++)
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
		// an actor whose motion step clears its sequence / model (+0x170: its last frame) is not drawn on the next tick
		if (num >= den && U32(nxt, 0x170) == 0) return P_ENDS;
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
		return P_NEXT;
	}

	static void SystemHeld(const Rec &r, const uint8_t *next_view, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		const uint32_t sys = r.node;
		MEM<uint32_t>(ACTOR_STATE) = sys;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		if (next_view) memcpy((void *)CAM_COPY, next_view, 0x20); // (the scratch, camera copy included, is put back below)
		// the system draws on the next tick only while its step stays in phase 1 (0x779C10: phase 2 = done, nothing drawn): else
		// every actor holds below 1 and nothing is drawn at 1
		const bool sys_draws = U16(sys, 0x20) == 1;
		if (!sys_draws && num >= den) return;
		NextTick(sys);
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), num, den);
		if (!sys_draws)
			for (int i = 0; i < nl; i++) { memcpy(copies[i], (const void *)list[i], ACTOR_BYTES); cls[i] = P_HOLD; }
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch and morph vertex blocks
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
		g_sys_verts[r.sysi].restore();
		// the sprite sequences, as 0x77B320 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				ak_77B3E0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					ak_77B3E0(c);
				}
			}
		}
		// the models, as 0x779CA0 (a_735440) draws them (0x779D00 = a_7354A0)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				a_7354A0(P(copies[i]), def);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the real tick's draws fit the notes and the private arenas (a quarter of each kept as margin: the in-between draws may
	// write a little more than the real ones)
	static bool fits()
	{
		if (g_full) return false;
		for (int i = 0; i < 3; i++)
			if (g_end[i] >= g_start[i] && g_end[i] - g_start[i] > sizeof(g_priv[i]) / 4 * 3) return false;
		return true;
	}

	static bool HeldReady()
	{
		note_cam_words(); // (every held frame, drawn or not)
		return held_ready() && g_end_tick == g_real_tick && g_tick == g_real_tick && fits();
	}

	static uint32_t *crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_087;
		g_in_held = true;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80], cam[0xB0];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		uint8_t sprite_cam[12]; // written by the sprite draws (0x571BC0)
		memcpy(sprite_cam, (const void *)0x21DFEE4, sizeof(sprite_cam));
		const uint32_t seed = *crt_seed();
		static VertexBlocks verts_now[MAX_SYSTEMS]; // the blocks as the real tick left them
		for (int i = 0; i < g_nrec; i++)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].save(g_rec[i].node);
		// the private arenas start with the real tick's packets: the redraws write the same packets at the same offsets over
		// them (the bytes a draw does not write are the real tick's)
		uint32_t cur[3];
		for (int i = 0; i < 3; i++)
		{
			cur[i] = MEM<uint32_t>(CURSORS[i]);
			const uint32_t used = g_end[i] - g_start[i];
			if (g_start[i] && g_end[i] > g_start[i] && used <= sizeof(g_priv[i])) memcpy(g_priv[i], (const void *)g_start[i], used);
			MEM<uint32_t>(CURSORS[i]) = P(g_priv[i]);
		}
		uint8_t view[0x20];
		const bool next_view = g_nsys > 0 && NextView(view);
		g_angelo_known = false;
		g_angelo_node = 0;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			if (r.kind == HK_SYSTEM) { SystemHeld(r, next_view ? view : nullptr, num, den); continue; }
			// each draw starts from the real tick's GTE state, scratch and Field_Alloc scratch
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			switch (r.kind)
			{
			case HK_ANGELO: CreatureHeld(r, num, den); break;
			case HK_PLAY: ItemHeld(r, num, den); break;
			case HK_RING: RingHeld(r, num, den); break;
			case HK_MODEL: ModelHeld(r, num, den); break;
			default: break;
			}
		}
		for (int i = 0; i < 3; i++) MEM<uint32_t>(CURSORS[i]) = cur[i];
		for (int i = g_nrec - 1; i >= 0; i--)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].restore();
		*crt_seed() = seed;
		memcpy((void *)0x21DFEE4, sprite_cam, sizeof(sprite_cam));
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_in_held = false;
		g_mod = saved_mod;
	}

	// the held-frame camera of the module's camera script (the library camera stepper a_73AE10 on the camera block [0x155B1E0]):
	// act::held_camera with this module current (it saves / puts back the camera block, the words, the GTE registers and the
	// Field_Alloc scratch); the camera task's states 2 / 3 (0x774CB0 / 0x774CE0) do not step while the module is paused: when
	// the next tick's pause switch is set there, the camera holds
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_camtask_tick == g_real_tick && g_camtask && U32(g_camtask, 8) == ORIG_Camera)
		{
			const int8_t st = S8(g_camtask, 0x29);
			if ((st == 2 || st == 3) && pause_next() != 0)
			{
				const int16_t *eye = (const int16_t *)0xB8B7F0, *at = (const int16_t *)0xB8B7F8;
				for (int k = 0; k < 3; k++) { world[k] = eye[k]; lookat[k] = at[k]; }
				return true;
			}
		}
		const Mod *m = g_mod;
		g_mod = &MOD_087;
		const bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag087_held()
	{
		static const uint32_t HELD[] = { act::strike87::ORIG_Angelo, act::strike87::ORIG_DustPlay, act::strike87::ORIG_ImpactPlay,
			act::strike87::ORIG_Ring, act::strike87::ORIG_Halo, act::strike87::ORIG_Debris1, act::strike87::ORIG_Debris2,
			act::strike87::ORIG_ActorSystem, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(87, act::strike87::HeldReady, act::strike87::HeldFrame);
		register_module_camera(87, act::strike87::HeldCamera);
	}
}
