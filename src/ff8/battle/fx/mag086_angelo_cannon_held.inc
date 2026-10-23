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

// 30 fps part of mag086_angelo_cannon.cpp: the held-frame drawing (included at the end of mag086_angelo_cannon.cpp)
//
// Every drawing task of Angelo Cannon updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch it started from); the
// held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half way to the next tick's
// state (the task's own update predicted on a copy; no draw, no spawn, no sound, no random number, no entity or camera write).
//   Angelo      - the creature 0x77FF90, drawn by the creature renderer 0x780130 (a_746C10) with the bone matrices of his current
//                 pose. Next-pose approach: the next tick's states run on a copy of the node (their animation reads 0x8DD1C0 /
//                 0x8DD220 / 0x8DD190 on the copy's animation header; the shared pose block is put back; the director cues
//                 predicted as the next tick's director makes the requested phase current first; the dash / jump of 0x784070 on
//                 the copy; Rinoa's chain transformations and entity word, the spawns, the sounds and the pause switch are not
//                 run), then his velocity: the exact next pose, position and visibility. Held: the bone matrices of the pose half
//                 way (each angle the short way round, as pose_midpoint), position (+0x4C..) half way; at 1 the next pose and
//                 position. An animation end / restart / change = discrete switch (this pose below 1). Hidden or released on the
//                 next tick: held below 1, not drawn at 1.
//   Plays       - the three prim plays 0x7836E0 / 0x783C90 / 0x783F80 (a_73C280 copy 0x783790, object callback 0x7838C0): fixed
//                 placements; prim::play_held (objects half way to their next keyframe records); the third play's texture scroll
//                 (+0x8C, 7 bits, +0x10 a tick) half way. Released or finished on the next tick: held below 1, not drawn at 1.
//                 The plays' pause word (0x25B7080, set / cleared by Angelo's state 4 through a_73BB60) needs no prediction: a
//                 paused play still draws the frame its player reached (it only stops advancing), so the next tick draws the
//                 advanced frame paused or not; a play paused on this tick holds (prim::play_held: not moving).
//   Rings       - the dust rings 0x782930 (prim sequence drawn by 0x782990): position half way to their next step (state 1
//                 0x783080 on a copy); the sequence frame switches on the real frame; the sequence's end hides it at 1.
//   Dust puffs  - 0x780640 (model drawn by 0x7806A0): position, roll +0x48 and scale +0x30.. half way to their next step (state 1
//                 0x782850 on a copy).
//   Glows       - the muzzle glow 0x783350 and the dash glow 0x783DC0 (models drawn by 0x7833D0): their next step on a copy (fade,
//                 finish); position = Angelo's bone point 0xF1 on the next tick (his next copy and pose, through the next tick's
//                 camera block); the muzzle glow's z = the camera eye's z (0xB8B7F4) the next tick reads + 0x600; position, fade
//                 +0x50 and texture scroll +0x92 (7 bits, -0x10 a tick) half way.
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. Spawns, sounds, random numbers, sync requests and entity writes stay on the real ticks.
// Camera: none written by the module (the setup's engine camera animation); every part is placed through the frame's camera
// (0x1D97778, the frame gate's). The eye's z the muzzle glow reads on the next tick: the camera words the battle left after this
// tick (noted at the held frames): when the glow read the words of this tick (the camera stepped before it), the next tick's
// are one step on (this tick's step repeated); when it read the previous tick's, the next tick reads the current ones. Angelo's
// next bone point goes through the next tick's camera block (NextCamBlock); the camera block, the Field_Alloc scratch and the GTE
// registers are put back.
// Not drawn by the module and held at 15 Hz by the engine: the root-matrix slot 0x1D99AB0, Rinoa's entity (bound animation set,
// chain transformations 1 / 2 / 3, entity word bit 0x200).
// The packets go to private copies of the three packet arenas (module cursors 0x25B4CCC / 0x25B4DCC, frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch, the
// shadow / RenderGeometry temporaries, Angelo's pose block, the CRT rand seed and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace cannon86
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25B4CC8, BSS_HI = 0x25BC38C;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x25B4CCC, CUR_B = 0x25B4DCC, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Angelo = 0x77FF90, ORIG_Play1 = 0x7836E0, ORIG_Play2 = 0x783C90, ORIG_Play3 = 0x783F80;
	static const uint32_t ORIG_Ring = 0x782930, ORIG_Puff = 0x780640, ORIG_Glow1 = 0x783350, ORIG_Glow2 = 0x783DC0;
	static const uint32_t PLAY_BLEND = 0x25BBF60;     // the plays' vertex lerp scratch (G_258EF98: block +0x48)
	static const uint32_t NODE_MAX = 0x450; // the largest node a draw reads (the plays)
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
	static const int MAX_RECS = 200;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[3], g_end[3];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[3] = { CUR_A, CUR_B, CUR_FA };
	static uint8_t g_priv[3][0x40000];
	static uint8_t g_cam_tick[0x20];                       // the module's camera copy this real tick drew with
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_Play1 || f == ORIG_Play2 || f == ORIG_Play3) return 0x450;
		if (f == ORIG_Ring) return 0x90;
		if (f == ORIG_Puff || f == ORIG_Glow1 || f == ORIG_Glow2) return 0x98;
		return 0x160; // Angelo
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = 0;
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

	// the director phase the next tick's tasks test (a_73B7E0): the director runs first in the master's queues and its phase
	// bookkeeping (0x77FCB0) makes the requested phase (+0x42) current
	static bool phase_next(int32_t k)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const int16_t cur = S16(d, 0x40), req = S16(d, 0x42);
		return (cur != req ? req : cur) >= (int16_t)k;
	}

	// Angelo's next tick (0x77FF90's states, then his velocity) on the copy `nx` of his node: the animation reads run on the copy's
	// animation header (+0x90) and write the shared pose block (the caller puts it back). switched = the animation ends / restarts
	// / changes. false = the next tick does not draw him (hidden)
	static bool angelo_next(uint32_t nx, bool &switched)
	{
		switched = false;
		switch (S8(nx, 0x29))
		{
		case 1: // 0x7832D0: counter; at cue 2 shown, velocity z -0x100 (the sound and the muzzle glow: not run)
			U16(nx, 0x134) = (uint16_t)(U16(nx, 0x134) + 1);
			if (phase_next(2))
			{
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) & 0xFB);
				U16(nx, 0x128) = 0xFF00;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			break;
		case 2: // 0x7835E0: animation step; from frame 0x12 animation 1, velocity 0 (Rinoa's chain transformation 2: not run)
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x12)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 1);
				U16(nx, 0x128) = 0;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		case 3: // 0x783640: animation step (0x8DD220); at frame 0xC next state (the first play: not spawned)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) switched = true;
			if (U16(nx, 0x136) == 0xC) U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			break;
		case 4: // 0x783BA0: when the animation ends at cue 3: animation 2, the dash from his position to the director's point +8..
			// (the plays' pause switch, the sound, the second play and the dash glow: not run)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				switched = true;
				if (phase_next(3))
				{
					x::au_re_Battle_ReadAnimation_7(nx, 2);
					const uint32_t d = MEM<uint32_t>(DIR_PTR);
					U32(nx, 0x140) = U32(nx, 0x4C);
					U32(nx, 0x148) = U32(d, 8);
					U32(nx, 0x144) = U32(nx, 0x50);
					U32(nx, 0x14C) = U32(d, 0xC);
					U16(nx, 0x154) = 0;
					U16(nx, 0x156) = 0x201;
					U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				}
			}
			break;
		case 5: // 0x783F00: animation step and the dash (0x784070); at its end animation 3 (the third play and Rinoa's entity word:
			// not run)
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			ac_784070(nx);
			if (U16(nx, 0x154) == 0x1000)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 3);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		case 6: // 0x784150: at the animation's end animation 4, the jump back (from / to swapped, height velocity -0x200, gravity
			// 0x40)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 4);
				const uint32_t to0 = U32(nx, 0x148), from0 = U32(nx, 0x140), from1 = U32(nx, 0x144), to1 = U32(nx, 0x14C);
				U32(nx, 0x140) = to0;
				U32(nx, 0x148) = from0;
				U16(nx, 0x154) = 0;
				U16(nx, 0x158) = 0;
				U32(nx, 0x144) = to1;
				U32(nx, 0x14C) = from1;
				U16(nx, 0x156) = 0x112;
				U16(nx, 0x15A) = 0xFE00;
				U16(nx, 0x15C) = 0x40;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		case 7: // 0x7841E0: height, animation step and the jump; at its end animation 5 (Rinoa's chain transformation 3: not run)
			U16(nx, 0x15A) = (uint16_t)(U16(nx, 0x15A) + U16(nx, 0x15C));
			U16(nx, 0x158) = (uint16_t)(U16(nx, 0x158) + U16(nx, 0x15A));
			if (S16(nx, 0x158) > 0) U16(nx, 0x158) = 0;
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			ac_784070(nx);
			if (U16(nx, 0x154) == 0x1000)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 5);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		case 8: // 0x784270: at the animation's end animation 6, velocity z 0x100
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 6);
				U16(nx, 0x128) = 0x100;
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		case 9: // 0x7842B0: animation step; from frame 0x12 hidden + finished
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x12)
			{
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
				switched = true;
			}
			break;
		default: // 0 (placed hidden: never drawn), 10 (ret)
			break;
		}
		U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + U16(nx, 0x128));
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

	// the camera eye's z (0xB8B7F4) the muzzle glow reads on the next tick, from `base` (the one it read on this tick) and the
	// words the battle left after this tick / the previous one: the glow read this tick's words (the camera stepped before it):
	// the next tick's are this tick's step on; it read the previous tick's: the next tick reads the current ones
	static int16_t eye_z_next(int16_t base)
	{
		if (g_cw_held_tick != g_real_tick) return base;
		const int16_t zn = g_cw_held.w[2];
		if (g_cw_prev_tick + 1 == g_real_tick)
		{
			const int16_t zp = g_cw_prev.w[2];
			if (base == zn) return (int16_t)(zn + (zn - zp));
			if (base == zp) return zn;
		}
		return base;
	}

	// Angelo's bone point `bone` on the next tick (the glows run after his draw): his model matrix as the renderer builds it
	// (0x780130: rotation +0x3C, the next position, scale +0x114) and the next pose's bone matrices (set by the caller), through
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
	// a 7-bit texture scroll at num / den from s to n (the step the short way round the 0x80 wrap)
	static uint16_t scroll7(uint16_t s, uint16_t n, int num, int den)
	{
		const int32_t a = s & 0x7F, b = n & 0x7F, d = ((b - a + 0x40) & 0x7F) - 0x40;
		return (uint16_t)((a + d * num / den) & 0x7F);
	}

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];
	// Angelo's next-tick copy and pose (the glows follow his bone point on the next tick)
	alignas(4) static uint8_t g_angelo_nx[0x160];
	static Pose g_angelo_next;
	static bool g_angelo_known = false;
	static uint32_t g_angelo_node = 0;

	// Angelo at num / den: the bone matrices of the pose half way to the next tick's, the position half way
	static void CreatureHeld(const Rec &r, int num, int den)
	{
		g_angelo_known = false;
		const uint32_t sk = sk_of(r.node), skn = sk_size(sk);
		if (skn == 0 || skn > SK_MAX) return;
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x160); // the node as the real tick left it
		memcpy(g_mid, r.snap, 0x160);
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
				memcpy(g_mid, g_nx, 0x160);
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
		MEM<uint32_t>(CUR_A) = a_746C10(mid, CREATURE_ARG, MEM<uint32_t>(CUR_A));
		memcpy((void *)sk, sk_now, skn);
	}

	// Angelo's bone point 0xF1 on the next tick (his next copy and pose) into out (s16 x3); false = not known (hidden or released)
	static bool angelo_f1_next(uint32_t model_ptr, uint32_t out)
	{
		if (!g_angelo_known || model_ptr != g_angelo_node + 0x30) return false;
		const uint32_t angelo = g_angelo_node;
		const uint32_t sk = sk_of(angelo), skn = sk_size(sk);
		if (!skn || skn > SK_MAX) return false;
		static uint8_t sk_now[SK_MAX];
		memcpy(sk_now, (const void *)sk, skn);
		g_angelo_next.set(sk);
		x::BattleModel_BuildBoneMatricesFromPose(angelo + 0x90);
		alignas(4) static uint8_t an[0x160];
		memcpy(an, g_angelo_nx, sizeof(an));
		angelo_point_next(P(an), 0xF1, out);
		memcpy((void *)sk, sk_now, skn);
		return true;
	}

	// a play (0x783790 = a_73C280) of the node `live` with the placement of `place`, in between
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
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)ac_7838C0, (int)L, num, den);
	}

	// a noted play at num / den: fixed placement; the next tick plays it (state 1) unless its task was released (paused or not:
	// see the file comment); the third play's texture scroll +0x8C (7 bits, the task's +0x10 after the play: the live node holds
	// the next tick's) half way
	static void ItemHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x450);
		memcpy(g_mid, r.snap, 0x450);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (num >= den && !draws) return;
		if (draws && U32(cur, 8) == ORIG_Play3)
			U16(mid, 0x8C) = scroll7(U16(cur, 0x8C), U16(nx, 0x8C), num, den);
		PlayHeld(r.node, mid, draws ? num : 0, den);
	}

	// a ring particle (0x782930, draw 0x782990) at num / den: position half way to its next step (its state 1 0x783080 on a copy:
	// sequence frame, rise, damping); the sequence frame (+0x50) switches on the real frame
	static void RingHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x90);
		memcpy(g_mid, r.snap, 0x90);
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws)
		{
			ac_783080(nx);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) ac_782990(nx); return; }
		if (draws) for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		ac_782990(mid);
	}

	// a dust puff (0x780640, draw 0x7806A0) at num / den: position, roll +0x48 (the short way) and scale +0x30.. half way to its
	// next step (state 1 0x782850 on a copy)
	static void PuffHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x98);
		memcpy(g_mid, r.snap, 0x98);
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws)
		{
			ac_782850(nx);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) ac_7806A0(nx); return; }
		if (draws)
		{
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
			S16(mid, 0x48) = lerp_angle(S16(cur, 0x48), S16(nx, 0x48), num, den);
			for (uint32_t o = 0x30; o <= 0x38; o += 4) lerp32(mid, cur, nx, o, num, den);
		}
		ac_7806A0(mid);
	}

	// a glow (0x783350 muzzle / 0x783DC0 dash, draw 0x7833D0) at num / den: its next step on a copy (state 1: the muzzle glow's
	// fade +0x50 -= 0x100 down to 0x800; finished from frame 0x10; placed at Angelo's bone point 0xF1 on the next tick, the muzzle
	// glow at y -0x100 and the next tick's eye z + 0x600, the dash glow 0x40 lower) and its texture scroll +0x92 -= 0x10 (7 bits);
	// position, fade and scroll half way
	static void GlowHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x98);
		memcpy(g_mid, r.snap, 0x98);
		const bool muzzle = U32(cur, 8) == ORIG_Glow1;
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws)
		{
			if (muzzle)
			{
				U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + 0xFF00);
				if (S16(nx, 0x50) < 0x800) U16(nx, 0x50) = 0x800;
			}
			alignas(4) uint8_t pt[8] = {};
			if (angelo_f1_next(U32(nx, 0x8C), P(pt)))
			{
				memcpy((void *)(nx + 0x1C), pt, 6);
				if (muzzle)
				{
					U16(nx, 0x1E) = 0xFF00;
					U16(nx, 0x20) = (uint16_t)(eye_z_next((int16_t)(U16(cur, 0x20) - 0x600)) + 0x600);
				}
				else
					U16(nx, 0x1E) = (uint16_t)(U16(nx, 0x1E) + 0x40);
			}
			if (S16(nx, 0x24) >= 0x10)
			{
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 1);
				U8(nx, 0x29) = (uint8_t)(U8(nx, 0x29) + 1);
			}
			U16(nx, 0x92) = (uint16_t)((uint8_t)(U8(nx, 0x92) - 0x10) & 0x7F);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) ac_7833D0(nx, nx + 0x54, U16(nx, 0x90), U16(nx, 0x92)); return; }
		if (draws)
		{
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
			lerp16(mid, cur, nx, 0x50, num, den);
			U16(mid, 0x92) = scroll7(U16(cur, 0x92), U16(nx, 0x92), num, den);
		}
		ac_7833D0(mid, mid + 0x54, U16(mid, 0x90), U16(mid, 0x92));
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
		g_mod = &MOD_086;
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
		g_angelo_known = false;
		g_angelo_node = 0;
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
			case HK_ANGELO: CreatureHeld(r, num, den); break;
			case HK_PLAY: ItemHeld(r, num, den); break;
			case HK_RING: RingHeld(r, num, den); break;
			case HK_MODEL: PuffHeld(r, num, den); break;
			case HK_GLOW: GlowHeld(r, num, den); break;
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
	static void register_mag086_held()
	{
		static const uint32_t HELD[] = { act::cannon86::ORIG_Angelo, act::cannon86::ORIG_Play1, act::cannon86::ORIG_Play2, act::cannon86::ORIG_Play3,
			act::cannon86::ORIG_Ring, act::cannon86::ORIG_Puff, act::cannon86::ORIG_Glow1, act::cannon86::ORIG_Glow2, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(86, act::cannon86::HeldReady, act::cannon86::HeldFrame);
	}
}
