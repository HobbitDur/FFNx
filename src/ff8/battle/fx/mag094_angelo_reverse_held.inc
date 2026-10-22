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

// 30 fps part of mag094_angelo_reverse.cpp: the held-frame drawing (included at the end of mag094_angelo_reverse.cpp)
//
// Every drawing task of Angelo Reverse updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw / held_note_system: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch
// it started from); the held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half
// way to the next tick's state (the task's own update predicted on a copy; no draw, no spawn, no sound).
//   Angelo       - the creature 0x74D740, drawn by the creature renderer 0x74D820 (a_746C10) into the module pool with the
//                  bone matrices of its current pose. Next-pose approach: the next tick's states run on a copy of the node
//                  (their animation reads 0x8DD1C0 / 0x8DD220 / 0x8DD190 on the copy's animation header; the shared pose
//                  block is put back), giving the exact next pose, position and visibility. Held: the bone matrices of the
//                  pose half way to the next one (each angle the short way round, as pose_midpoint) and the position (+0x4C..)
//                  half way; at 1 the next pose and position. An animation that ends / restarts / changes on the next tick
//                  is a discrete switch: this tick's pose holds below 1 (the position still moves). The next tick hides
//                  it (state 3 from animation frame 0x10) or it ends: held below 1, not drawn at 1.
//   Plays        - the particles 0x74DBB0 (play step 0x74DCC0): the module's prim-model play redrawn by prim::play_held
//                  (objects half way to their next keyframe records) with the parameter block built from the node's position
//                  half way to the next tick's (gravity +0xD on the y velocity, then position += velocity).
//   Actor systems- the actor-system task 0x747DD0 (step 0x748940): the act-library actor template (as Mega Phoenix /
//                  X-Potion / The End: every drawn actor from a copy half way to its next motion step, the view of the next tick
//                  from the live camera; flipbook frame, model, fade and colours hold).
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. At 1 every part is the next tick's draw (prim objects that appear / disappear on the next tick do so on the real frame:
// prim::play_held).
// Camera: the module writes no camera word (the camera is the engine's camera animation of the setup): every part is placed
// through the frame's camera (0x1D97778, the frame gate's: the next tick's view at 1); the actor systems through the module's
// camera copy set to the next tick's view for their predictions (NextView, Field_Alloc scratch and GTE put back). Not drawn by
// the module and held at 15 Hz by the engine (as the real ticks set them): the stage draw entries' light words / colour bytes
// (the stage-light task 0x74E0C0), the root-matrix slot 0x1D99AB0.
// The packets go to private copies of the three packet arenas (module cursors 0x258FCFC / 0x258FE04, frame arena 0x1D8E054)
// that start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch,
// the shadow / RenderGeometry temporaries, Angelo's skeleton (pose + bone matrices), the actor data's morph vertex blocks, the
// CRT rand seed and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace angelo94
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x258FCF8, BSS_HI = 0x2592548;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x258FCFC, CUR_B = 0x258FE04, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Angelo = 0x74D740, ORIG_Heal = 0x74DBB0, ORIG_ActorSystem = 0x747DD0;
	static const uint32_t ANGELO_ARG = 0x2591960; // the creature renderer's state block
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_MAX = 0x180; // the largest node a draw reads (heal particle 0x17C)
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
	static const int MAX_RECS = 96, MAX_SYSTEMS = 8;
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

	static uint32_t node_size(uint32_t node) { return U32(node, 8) == ORIG_Heal ? 0x17C : 0x140; }

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

	// Angelo's next tick (0x74D740's states, then position += velocity) on the copy `nx` of its node: the animation reads run on
	// the copy's animation header (+0x90) and write the shared pose block (the caller puts it back). switched = the animation
	// ends / restarts / changes. false = the next tick does not draw it (hidden)
	static bool angelo_next(uint32_t nx, bool &switched)
	{
		switched = false;
		switch (S8(nx, 0x29))
		{
		case 1: // 0x74DAA0: animation step; from frame 0x10 stops (velocity z 0), animation 1
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x10)
			{
				U16(nx, 0x128) = 0;
				x::au_re_Battle_ReadAnimation_7(nx, 1);
				switched = true;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 2: // 0x74DAE0: animation step; when it ends animation 2, velocity z +0x100
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 2);
				U16(nx, 0x128) = 0x100;
				switched = true;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 3: // 0x74E080: animation step; from frame 0x10 hidden and finished
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x10)
			{
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
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

	// the heal particle 0x74DBB0 of the next tick on a copy: state 1 (0x74E030) = gravity, position += velocity, then the play
	static bool heal_next(uint32_t nx)
	{
		if (S8(nx, 0x29) != 1) return false;
		U16(nx, 0x6A) = (uint16_t)(U16(nx, 0x6A) + 0xD);
		U16(nx, 0x1C) = (uint16_t)(U16(nx, 0x1C) + U16(nx, 0x68));
		U16(nx, 0x1E) = (uint16_t)(U16(nx, 0x1E) + U16(nx, 0x6A));
		U16(nx, 0x20) = (uint16_t)(U16(nx, 0x20) + U16(nx, 0x6C));
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// held draws
	// ---------------------------------------------------------------------------------------------------------------
	// the module's heal play (0x74DCC0) of the node `live` with the placement of `place`, in between
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
		U32(L, 0x48) = MEM<uint32_t>(0x2592524);
		U32(L, 0x44) = U32(place, 0x7C);
		U16(L, 0x4C) = U16(place, 0x8C);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x4E) = U16(place, 0x8E);
		U16(L, 0x50) = U16(place, 0x90);
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)av_74DDF0, (int)L, num, den);
	}

	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];

	// Angelo at num / den: the bone matrices of the pose half way to the next tick's, the position half way
	static void AngeloHeld(const Rec &r, int num, int den)
	{
		const uint32_t sk = sk_of(r.node), skn = sk_size(sk);
		if (skn == 0 || skn > SK_MAX) return;
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x140); // the node as the real tick left it
		memcpy(g_mid, r.snap, 0x140);
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
				memcpy(g_mid, g_nx, 0x140);
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

	// a noted heal play at num / den
	static void HealHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x17C);
		memcpy(g_mid, r.snap, 0x17C);
		const bool draws = still_runs(cur, nx) && heal_next(nx);
		if (num >= den && !draws) return;
		if (draws)
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		PlayHeld(r.node, mid, draws ? num : 0, den);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// actor systems: the act-library actor template (Mega Phoenix / X-Potion / The End family; module actor state ACTOR_STATE)
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
	// the view the next tick draws with: the battle builds a tick's view (0x5033E0) from the camera words the tick before
	// left, so with a live camera (this tick's view = the one built from the words after the previous tick) the next view is
	// the one built from the words now; otherwise (a fixed view) it is this tick's. Camera block, words, GTE registers and
	// the Field_Alloc scratch are put back.
	static bool NextView(uint8_t out[0x20])
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

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x74AD30: list order; particles = type 0, actors = type 1) run on copies: a particle moves
	// by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0; 4 = key-position path 0x74CB00; its emission
	// 0x74C080 and life count: not run), an actor by its motion step (0x741050, the end test without its follow-up spawn,
	// 0x740910, 0x741120). A particle's owner and an actor's parent particle (+0x1BC) are their next-tick copies when the pass
	// updates them first, as on the next tick. A morphing model's step rewrites the shared vertex blocks: SystemHeld puts the
	// drawn ones back after the predictions.
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
			av_74CB00(nxt);
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
		NextTick(sys);
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), num, den);
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch and morph vertex blocks
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
		g_sys_verts[r.sysi].restore();
		// the sprite sequences, as 0x74ABF0 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				av_74ACB0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					av_74ACB0(c);
				}
			}
		}
		// the models, as 0x7489D0 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				av_748A30(P(copies[i]), def);
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
		g_mod = &MOD_094;
		g_in_held = true;
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
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			if (r.kind == HK_SYSTEM) { SystemHeld(r, next_view ? view : nullptr, num, den); continue; }
			// each draw starts from the real tick's GTE state, scratch and Field_Alloc scratch
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			if (r.kind == HK_ANGELO) AngeloHeld(r, num, den);
			else if (r.kind == HK_PLAY) HealHeld(r, num, den);
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
	static void register_mag094_held()
	{
		static const uint32_t HELD[] = { act::angelo94::ORIG_Angelo, act::angelo94::ORIG_Heal, act::angelo94::ORIG_ActorSystem, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(94, act::angelo94::HeldReady, act::angelo94::HeldFrame);
	}
}
