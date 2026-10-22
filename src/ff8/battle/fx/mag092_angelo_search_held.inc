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

// 30 fps part of mag092_angelo_search.cpp: the held-frame drawing (included at the end of mag092_angelo_search.cpp)
//
// Every drawing task of Angelo Search updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch it started from);
// the held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half way to the next
// tick's state (the task's own update predicted on a copy; no draw, no spawn, no sound, no random number, no entity write).
//   Angelo      - the creature 0x7557B0, drawn by the creature renderer 0x7558A0 (a_746C10) with the bone matrices of its current
//                 pose. Next-pose approach: the next tick's states run on a copy of the node (their animation reads 0x8DD1C0 /
//                 0x8DD220 / 0x8DD190 on the copy's animation header; the shared pose block is put back; the walks of states
//                 1 / 3 / 5 (+0x150 factor), the end points and the turn +0x3E as the states set them; Rinoa's chain
//                 transformation and the spawns are not run), then its motion: the exact next pose, position and visibility.
//                 Held: the bone matrices of the pose half way (each angle the short way round, as pose_midpoint), position
//                 (+0x4C..) half way; at 1 the next pose and position. An animation end / restart / change = discrete switch
//                 (this pose below 1). Hidden on the next tick (the end of the walk off) or released: held below 1, not drawn at 1.
//   Item play   - the found item's prim-model play 0x758CB0 (0x758D70, object callback 0x758EA0) left on the ground (its placement
//                 does not move): prim::play_held (objects half way to their next keyframe records). Released on the next tick:
//                 held below 1, not drawn at 1.
//   Rings       - the sniff rings 0x7581D0 (prim sequence drawn by 0x7582A0): position half way to the next step (sequence step
//                 0x758990 and the task's motion on a copy); the sequence frame switches on the real frame; the sequence's end
//                 hides it at 1.
//   Dust puffs  - the puffs 0x755EE0 (model drawn by 0x755FC0): position, roll and scale half way to the next step (0x758170 and
//                 the task's motion on a copy).
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. Spawns (sniff bursts, the item play), sounds, random numbers and Rinoa's chain transformations stay on the real ticks.
// Camera: the module writes no camera word (the camera is the engine's camera animation of the setup): every part is placed
// through the frame's camera (0x1D97778, the frame gate's: the next tick's view at 1).
// Not drawn by the module and held at 15 Hz by the engine: the root-matrix slot 0x1D99AB0, Rinoa's entity (her bound animation
// set and chain transformations are the battle engine's).
// The packets go to private copies of the three packet arenas (module cursors 0x2594D9C / 0x2594E9C, frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch, the
// shadow / RenderGeometry temporaries, Angelo's pose block, the CRT rand seed and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace angelo92
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x2594D98, BSS_HI = 0x2598F20;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CUR_A = 0x2594D9C, CUR_B = 0x2594E9C, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Angelo = 0x7557B0, ORIG_Play = 0x758CB0, ORIG_Ring = 0x7581D0, ORIG_Dust = 0x755EE0;
	static const uint32_t ANGELO_ARG = 0x2598320; // the creature renderer's state block
	static const uint32_t NODE_MAX = 0x180; // the largest node a draw reads (the item play 0x17C)
	static const int MAX_BONES = 256;
	static const uint32_t SK_MAX = 16 + 48 * MAX_BONES;

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
		uint32_t node;              // the task node
		uint8_t snap[NODE_MAX];     // the node as drawn
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
	};
	static const int MAX_RECS = 160;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[3], g_end[3];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[3] = { CUR_A, CUR_B, CUR_FA };
	static uint8_t g_priv[3][0x40000];
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_Play) return 0x17C;
		if (f == ORIG_Ring) return 0x90;
		if (f == ORIG_Dust) return 0x8C;
		return 0x154; // Angelo
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = 0;
		g_full = false;
		for (int i = 0; i < 3; i++) g_start[i] = g_end[i] = MEM<uint32_t>(CURSORS[i]);
	}

	static void held_note_draw(int kind, uint32_t node)
	{
		if (g_in_held) return;
		begin_tick();
		if (g_nrec >= MAX_RECS) { g_full = true; return; }
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = node;
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)SCRATCH_LO, sizeof(r.scratch));
		memcpy(r.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(r.falloc));
		memcpy(r.snap, (const void *)node, node_size(node));
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

	// Angelo's walk on a copy (as_walk: +0x4C.. from +0x140.. to +0x148.. by +0x150 / 4096); true = the walk ended (+0x150 at 0x1000)
	static bool walk_next(uint32_t nx, uint16_t step)
	{
		U16(nx, 0x150) = (uint16_t)(U16(nx, 0x150) + step);
		const int16_t t = S16(nx, 0x150);
		if (t >= 0x1000) { U16(nx, 0x150) = 0x1000; return true; }
		as_walk(nx, t);
		return false;
	}

	// Angelo's next tick (0x7557B0's states, then its motion) on the copy `nx` of its node: the animation reads run on the copy's
	// animation header (+0x90) and write the shared pose block (the caller puts it back). switched = the animation ends /
	// restarts / changes. false = the next tick does not draw it (hidden)
	static bool angelo_next(uint32_t nx, bool &switched)
	{
		switched = false;
		const uint32_t e = ENT(U8(nx, 0x2D));
		switch (S8(nx, 0x29))
		{
		case 1: // 0x755B70: animation step; walk in; at its end animation 1 (the sniff task spawn: not run)
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (walk_next(nx, 0x100))
			{
				U32(nx, 0x4C) = U32(nx, 0x148);
				U32(nx, 0x50) = U32(nx, 0x14C);
				x::au_re_Battle_ReadAnimation_7(nx, 1);
				switched = true;
			}
			break;
		case 2: // 0x758A00: when the animation ends animation 2, the walk to Rinoa, the turn +0x3E (the sound: not played)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 2);
				U32(nx, 0x140) = U32(nx, 0x4C);
				U32(nx, 0x144) = U32(nx, 0x50);
				U32(nx, 0x148) = U32(e, 0x1C);
				U32(nx, 0x14C) = U32(e, 0x20);
				U16(nx, 0x14C) = (uint16_t)(U16(nx, 0x14C) + 0xFD80);
				U16(nx, 0x3E) = 0xA0;
				U16(nx, 0x150) = 0;
				switched = true;
			}
			break;
		case 3: // 0x758AA0: animation step; walk to Rinoa; at its end on her, animation 3, turn 0 (her chain transformation: not run)
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (walk_next(nx, 0x2AB))
			{
				U32(nx, 0x4C) = U32(e, 0x1C);
				U32(nx, 0x50) = U32(e, 0x20);
				x::au_re_Battle_ReadAnimation_7(nx, 3);
				U16(nx, 0x3E) = 0;
				switched = true;
			}
			break;
		case 4: // 0x758BB0: when the animation ends animation 4, the walk off (the stream and the item play: not run)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 4);
				const uint32_t p = U32(nx, 0x4C), q = U32(nx, 0x50);
				U32(nx, 0x140) = p;
				U32(nx, 0x148) = p;
				U32(nx, 0x14C) = q;
				U16(nx, 0x14C) = (uint16_t)(U16(nx, 0x14C) + 0x1000);
				U32(nx, 0x144) = q;
				U16(nx, 0x150) = 0;
				switched = true;
			}
			break;
		case 5: // 0x759110: animation step; walk off; at its end hidden + finished
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (walk_next(nx, 0x100))
			{
				U32(nx, 0x4C) = U32(nx, 0x148);
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5);
				U32(nx, 0x50) = U32(nx, 0x14C);
			}
			break;
		default:
			break;
		}
		U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + U16(nx, 0x128));
		U16(nx, 0x4C) = (uint16_t)(U16(nx, 0x4C) + U16(nx, 0x124));
		U16(nx, 0x4E) = (uint16_t)(U16(nx, 0x4E) + U16(nx, 0x126));
		return (U8(nx, 0x26) & 4) == 0;
	}

	// a ring's next tick on a copy (0x7581D0: its state step 0x758990, then rise / damping / motion)
	static void ring_next(uint32_t n)
	{
		as_758990(n);
		U16(n, 0x7A) = (uint16_t)(U16(n, 0x7A) + U16(n, 0x82));
		int16_t vx = S16(n, 0x78);
		int16_t vz = S16(n, 0x7C);
		const int16_t vy = S16(n, 0x7A);
		vx = (int16_t)(vx - vx / 8);
		U16(n, 0x78) = (uint16_t)vx;
		vz = (int16_t)(vz - vz / 8);
		U16(n, 0x7C) = (uint16_t)vz;
		U16(n, 0x1C) = (uint16_t)(U16(n, 0x1C) + (uint16_t)(vx / 16));
		U16(n, 0x1E) = (uint16_t)(U16(n, 0x1E) + (uint16_t)(vy / 16));
		U16(n, 0x20) = (uint16_t)(U16(n, 0x20) + (uint16_t)(vz / 16));
	}

	// a dust puff's next tick on a copy (0x755EE0: its state step 0x758170, then damping / motion)
	static void dust_next(uint32_t c)
	{
		as_758170(c);
		int16_t vx = S16(c, 0x74);
		int16_t vy = (int16_t)(S16(c, 0x76) + 0x80);
		int16_t vz = S16(c, 0x78);
		vx = (int16_t)(vx - vx / 16);
		U16(c, 0x74) = (uint16_t)vx;
		vy = (int16_t)(vy - vy / 16);
		U16(c, 0x76) = (uint16_t)vy;
		vz = (int16_t)(vz - vz / 16);
		U16(c, 0x78) = (uint16_t)vz;
		U16(c, 0x1C) = (uint16_t)(U16(c, 0x1C) + (uint16_t)(vx / 16));
		U16(c, 0x1E) = (uint16_t)(U16(c, 0x1E) + (uint16_t)(vy / 16));
		U16(c, 0x20) = (uint16_t)(U16(c, 0x20) + (uint16_t)(vz / 16));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// held draws
	// ---------------------------------------------------------------------------------------------------------------
	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }
	static void lerp32(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S32(mid, o) = lerp_i(S32(a, o), S32(b, o), num, den); }

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];

	// Angelo at num / den: the bone matrices of the pose half way to the next tick's, the position half way
	static void AngeloHeld(const Rec &r, int num, int den)
	{
		const uint32_t sk = sk_of(r.node), skn = sk_size(sk);
		if (skn == 0 || skn > SK_MAX) return;
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x154); // the node as the real tick left it
		memcpy(g_mid, r.snap, 0x154);
		static uint8_t sk_now[SK_MAX];
		static Pose now, next, half;
		memcpy(sk_now, (const void *)sk, skn); // the pose block as the real tick left it (angles + bone matrices)
		now.get(sk);
		bool switched = false;
		const bool draws = still_runs(cur, nx) && angelo_next(nx, switched);
		next.get(sk);                           // the next tick's pose (the copy's animation reads)
		memcpy((void *)sk, sk_now, skn);
		if (num >= den && !draws) return;
		if (draws)
		{
			if (num >= den)
			{
				memcpy(g_mid, g_nx, 0x154);
				next.set(sk);
				x::BattleModel_BuildBoneMatricesFromPose(r.node + 0x90);
			}
			else
			{
				for (uint32_t o = 0x4C; o <= 0x50; o += 2) lerp16(mid, cur, nx, o, num, den);
				if (!switched && next.nb == now.nb)
				{
					pose_lerp(now, next, num, den, half);
					half.set(sk);
					x::BattleModel_BuildBoneMatricesFromPose(r.node + 0x90);
				}
			}
			now.set(sk); // (the renderer rebuilds the bone matrices from the pose after its render: the real pose)
		}
		MEM<uint32_t>(CUR_A) = a_746C10(mid, ANGELO_ARG, MEM<uint32_t>(CUR_A));
		memcpy((void *)sk, sk_now, skn);
	}

	// the item play (0x758D70) of the node `live` with the placement of `place`, in between
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
		U32(L, 0x48) = MEM<uint32_t>(0x2598EE8);
		U32(L, 0x44) = U32(place, 0x7C);
		U16(L, 0x4C) = U16(place, 0x8C);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x4E) = U16(place, 0x8E);
		U16(L, 0x50) = U16(place, 0x90);
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)as_758EA0, (int)L, num, den);
	}

	// a noted item play at num / den: its placement does not move; the next tick plays it (state 1, 0x7590E0) unless its task
	// was released
	static void ItemHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx);
		memcpy(g_nx, (const void *)r.node, 0x17C);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (num >= den && !draws) return;
		PlayHeld(r.node, cur, draws ? num : 0, den);
	}

	// a ring particle (0x7581D0, draw 0x7582A0) at num / den: position half way to its next step; the sequence frame (+0x50)
	// switches on the real frame
	static void RingHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x90);
		memcpy(g_mid, r.snap, 0x90);
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws) { ring_next(nx); draws = (U8(nx, 0x26) & 4) == 0; }
		if (num >= den) { if (draws) as_7582A0(nx); return; }
		if (draws) for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		as_7582A0(mid);
	}

	// a dust puff (0x755EE0, draw 0x755FC0) at num / den: position, roll and scale half way to its next step
	static void DustHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x8C);
		memcpy(g_mid, r.snap, 0x8C);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws) dust_next(nx);
		if (num >= den) { if (draws) as_755FC0(nx); return; }
		if (draws)
		{
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
			S16(mid, 0x48) = lerp_angle(S16(cur, 0x48), S16(nx, 0x48), num, den);
			for (uint32_t o = 0x30; o <= 0x38; o += 4) lerp32(mid, cur, nx, o, num, den);
		}
		as_755FC0(mid);
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
		return held_ready() && g_end_tick == g_real_tick && g_tick == g_real_tick && fits();
	}

	static uint32_t *crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_092;
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
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			// each draw starts from the real tick's GTE state, scratch and Field_Alloc scratch
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			switch (r.kind)
			{
			case HK_ANGELO: AngeloHeld(r, num, den); break;
			case HK_PLAY: ItemHeld(r, num, den); break;
			case HK_RING: RingHeld(r, num, den); break;
			case HK_MODEL: DustHeld(r, num, den); break;
			default: break;
			}
		}
		for (int i = 0; i < 3; i++) MEM<uint32_t>(CURSORS[i]) = cur[i];
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
}
}
	static void register_mag092_held()
	{
		static const uint32_t HELD[] = { act::angelo92::ORIG_Angelo, act::angelo92::ORIG_Play, act::angelo92::ORIG_Ring, act::angelo92::ORIG_Dust, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(92, act::angelo92::HeldReady, act::angelo92::HeldFrame);
	}
}
