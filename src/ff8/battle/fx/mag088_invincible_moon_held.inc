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

// 30 fps part of mag088_invincible_moon.cpp: the held-frame drawing (included at the end of mag088_invincible_moon.cpp)
//
// Every drawing task of Invincible Moon updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw / held_note_system: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch
// it started from); the held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half
// way to the next tick's state (the task's own update predicted on a copy; no draw, no spawn, no sound, no random number, no
// entity or camera write).
//   Angelo / Rinoa's double - the creatures 0x771DB0 / 0x7718D0, drawn by the creature renderer 0x7719A0 (a_746C10) with the bone
//                 matrices of their current pose. Next-pose approach: the next tick's states run on a copy of the node (their
//                 animation reads 0x8DD1C0 / 0x8DD220 / 0x8DD190 on the copy's animation header; the shared pose block is put
//                 back; the director cues predicted as the next tick's director makes the requested phase current first; Rinoa's
//                 chain transformations, the spawns, the sounds and the sync requests are not run), then the motion: the exact
//                 next pose, position, glow (0x773400 on the copy) and visibility. Held: the bone matrices of the pose half way
//                 (each angle the short way round, as pose_midpoint), position (+0x4C..) half way; at 1 the next pose and position.
//                 An animation end / restart / change = discrete switch (this pose below 1). Hidden or released on the next
//                 tick: held below 1, not drawn at 1.
//   Plays       - the moon play 0x76A180 and the howl play 0x772C50 (a_73C280 copy 0x76A250, object callback 0x76A380): fixed
//                 placements; prim::play_held (objects half way to their next keyframe records); the moon play's texture scroll
//                 (+0x8E, 7 bits, -2 a tick) half way. Released, finished (cue 5) or paused (the module pause words, set while
//                 Angelo waits for sync point 3) on the next tick: held below 1 (paused: the same frame at 1).
//   Rings       - the dust rings 0x772060 / 0x772990 (prim sequence drawn by 0x7720C0): position half way to the next step (their
//                 state 1 on a copy); the sequence frame switches on the real frame; the sequence's end hides it at 1.
//   Moon models - the moon and its glows 0x771660 (drawn by 0x7716C0): x drift half way; hidden at cue 4.
//   Halo        - 0x772D90 (drawn by 0x772E30): spin +0x46 and height +0x34 (Angelo's bone point 0x1D on the next tick, through
//                 the next tick's camera block) half way, fade +0x50 half way; hidden at cue 3.
//   Screen flash- the module's flash 0x773090 (script 0x1557CAC): its four quads with the colour half way to the next script
//                 step's (the step on a copy, with the next tick's pause word).
//   Actor system- 0x76C700: the act-library actor template (every drawn actor from a copy half way to its next motion step, the
//                 view of the next tick from the live camera).
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. Spawns, sounds, random numbers, sync requests and entity writes stay on the real ticks.
// Camera: the module's camera script (camera task 0x769060 through the library stepper a_73AE10): HeldCamera = act::held_camera
// (the stepper run once more on the saved camera block / words / GTE / Field_Alloc scratch, eye and look-at half way; a cut
// holds). Every part is placed through the frame's camera (0x1D97778, the frame gate's); the actor system through the module's
// camera copy set to the next tick's view for its predictions (NextView); the halo's next height through the next tick's
// camera block (NextCamBlock); the camera block, the Field_Alloc scratch and the GTE registers are put back.
// Not drawn by the module and held at 15 Hz by the engine: the root-matrix slot 0x1D99AB0, the stage light words 0x1D98992..,
// the party entities' hide flags (cues 3 / 4), Rinoa's entity (bound animation set, chain transformations 1 / 2 / 4).
// The packets go to private copies of the three packet arenas (module cursors 0x25A4D9C / 0x25A4EA4, frame arena 0x1D8E054)
// that start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch,
// the shadow / RenderGeometry temporaries, the creatures' pose blocks, the actor data's morph vertex blocks, the CRT rand seed
// and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace moon88
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25A4D98, BSS_HI = 0x25ACEAC;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x25A4D9C, CUR_B = 0x25A4EA4, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Angelo = 0x771DB0, ORIG_Double = 0x7718D0, ORIG_MoonPlay = 0x76A180, ORIG_HowlPlay = 0x772C50;
	static const uint32_t ORIG_Ring1 = 0x772060, ORIG_Ring2 = 0x772990, ORIG_Moon = 0x771660, ORIG_Halo = 0x772D90, ORIG_Flash = 0x773090;
	static const uint32_t ORIG_ActorSystem = 0x76C700;
	static const uint32_t PLAY_PAUSE = 0x25A6BBC;     // the plays' pause word (G_2585E38; with FLASH_PAUSE, a_73BB60)
	static const uint32_t PLAY_BLEND = 0x25AC750;     // the plays' vertex lerp scratch (G_258EF98: block +0x48)
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_MAX = 0x480; // the largest node a draw reads (the plays 0x474)
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
		uint8_t snap[NODE_MAX];     // the node as drawn (HK_FLASH: after its script step)
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int sysi;                   // HK_SYSTEM: its vertex-block note
	};
	static const int MAX_RECS = 200, MAX_SYSTEMS = 8;
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

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_MoonPlay || f == ORIG_HowlPlay) return 0x474;
		if (f == ORIG_Ring1 || f == ORIG_Ring2) return 0x90;
		if (f == ORIG_Moon || f == ORIG_Halo) return 0x9C;
		if (f == ORIG_Flash) return 0x40;
		return 0x160; // Angelo / Rinoa's double
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
	// bookkeeping (0x768F90) makes the requested phase (+0x42) current
	static bool phase_next(int32_t k)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const int16_t cur = S16(d, 0x40), req = S16(d, 0x42);
		return (cur != req ? req : cur) >= (int16_t)k;
	}

	// the module pause words (0x25ACD34 flash / 0x25A6BBC plays, a_73BB60) on the next tick: while Angelo waits for director
	// sync point 3 (his state 8, 0x773320) they are set (a_73BB60(1)) every tick and cleared (a_73BB60(0)) when the sync point
	// is granted: a_73B640(3) after the next tick's director bookkeeping (requested -> current, next -> requested); else as now
	static uint32_t pause_next()
	{
		const uint32_t now = MEM<uint32_t>(FLASH_PAUSE);
		const uint32_t a = U32(MEM<uint32_t>(DIR_PTR), 0x18); // Angelo's node (0x772AB0)
		if (!a || U32(a, 8) != ORIG_Angelo || S8(a, 0x29) != 8 || ((U8(a, 0x26) & 1) && U8(a, 0x28) == 0)) return now;
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const int16_t c40 = S16(d, 0x40), c42 = S16(d, 0x42), c44 = S16(d, 0x44);
		const int16_t n40 = c40 != c42 ? c42 : c40;
		const bool granted = c44 == n40 && c44 == 2;
		if (granted) return now == 1 ? 0 : now;
		return now == 0 ? 1 : now;
	}

	// Angelo's next tick (0x771DB0's states, then his motion) on the copy `nx` of his node: the animation reads run on the copy's
	// animation header (+0x90) and write the shared pose block (the caller puts it back). switched = the animation ends / restarts
	// / changes. false = the next tick does not draw him (hidden)
	static bool angelo_next(uint32_t nx, bool &switched)
	{
		switched = false;
		const uint32_t e = ENT(U8(nx, 0x2C));
		switch (S8(nx, 0x29))
		{
		case 1: // 0x772B40: animation step (0x8DD220); from frame 0xB Rinoa's chain transformation 2 (not run)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) switched = true;
			break;
		case 2: // 0x772B80: at the animation's end: velocity 0, animation 1, at Rinoa's position
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1)
			{
				U16(nx, 0x128) = 0;
				x::au_re_Battle_ReadAnimation_7(nx, 1);
				U32(nx, 0x4C) = U32(e, 0x1C);
				U32(nx, 0x50) = U32(e, 0x20);
				switched = true;
			}
			break;
		case 3: // 0x772BE0: at the animation's end animation 2 (the howl play: not spawned)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { x::au_re_Battle_ReadAnimation_7(nx, 2); switched = true; }
			break;
		case 4: // 0x772D40: animation step; from frame 0x14 animation 3 (the halo: not spawned)
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x14) { x::au_re_Battle_ReadAnimation_7(nx, 3); switched = true; }
			break;
		case 5: // 0x773010: at the animation's end animation 8
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { x::au_re_Battle_ReadAnimation_7(nx, 8); switched = true; }
			break;
		case 6: // 0x773040 / 0x7732F0 / 0x773320: animation steps (the flash, the countdown, the sync point: no draw change)
		case 7:
		case 8:
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			break;
		case 9: // 0x7733B0: animation step; at cue 3: animation 8, glow 0x800
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (phase_next(3))
			{
				x::au_re_Battle_ReadAnimation_7(nx, 8);
				U16(nx, 0x13A) = 0x800;
				im_773400(nx, 0);
				switched = true;
			}
			break;
		case 10: // 0x773520: animation step; from frame 6: animation 4, glow 0x800
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 6)
			{
				x::au_re_Battle_ReadAnimation_7(nx, 4);
				U16(nx, 0x13A) = 0x800;
				im_773400(nx, 0);
				switched = true;
			}
			break;
		case 11: // 0x773560: animation step (0x8DD220); at cue 4 hidden
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) switched = true;
			if (phase_next(4)) U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 4);
			break;
		case 12: // 0x773590: at cue 5: shown, glow 0, animation 5, at Rinoa's position (her chain transformation 4: not run)
			if (phase_next(5))
			{
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) & 0xFB);
				U16(nx, 0x13A) = 0;
				im_773400(nx, 0);
				x::au_re_Battle_ReadAnimation_7(nx, 5);
				U32(nx, 0x4C) = U32(e, 0x1C);
				U32(nx, 0x50) = U32(e, 0x20);
				switched = true;
			}
			break;
		case 13: // 0x773600: at the animation's end animation 6
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { x::au_re_Battle_ReadAnimation_7(nx, 6); switched = true; }
			break;
		case 14: // 0x773630: at the animation's end animation 7, velocity z 0x100
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { x::au_re_Battle_ReadAnimation_7(nx, 7); U16(nx, 0x128) = 0x100; switched = true; }
			break;
		case 15: // 0x773670: animation step; from frame 0x12 animation 3, hidden + finished
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x12) { x::au_re_Battle_ReadAnimation_7(nx, 3); U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5); switched = true; }
			break;
		default:
			break;
		}
		U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + U16(nx, 0x128));
		U16(nx, 0x4C) = (uint16_t)(U16(nx, 0x4C) + U16(nx, 0x124));
		U16(nx, 0x4E) = (uint16_t)(U16(nx, 0x4E) + U16(nx, 0x126));
		return (U8(nx, 0x26) & 4) == 0;
	}

	// Rinoa's double's next tick (0x7718D0's states) on a copy (no motion)
	static bool double_next(uint32_t nx, bool &switched)
	{
		switched = false;
		switch (S8(nx, 0x29))
		{
		case 1: // 0x771BD0: from frame 0xC shown, animation 0
			if (S16(nx, 0x24) >= 0xC) { U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) & 0xFB); x::au_re_Battle_ReadAnimation_7(nx, 0); switched = true; }
			break;
		case 2: // 0x771C00: at the animation's end: velocity 0, animation 1
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { U16(nx, 0x128) = 0; x::au_re_Battle_ReadAnimation_7(nx, 1); switched = true; }
			break;
		case 3: // 0x771C30: animation step; from frame 0x14 animation 2
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 0x14) { x::au_re_Battle_ReadAnimation_7(nx, 2); switched = true; }
			break;
		case 4: // 0x771C60: at the animation's end animation 5
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { x::au_re_Battle_ReadAnimation_7(nx, 5); switched = true; }
			break;
		case 5: // 0x771C90: animation step; at cue 3 animation 5
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (phase_next(3)) { x::au_re_Battle_ReadAnimation_7(nx, 5); switched = true; }
			break;
		case 6: // 0x771CC0: animation step; from frame 6 animation 3
			if (x::sub_8DD1C0(nx) == 1) switched = true;
			if (S16(nx, 0x136) >= 6) { x::au_re_Battle_ReadAnimation_7(nx, 3); switched = true; }
			break;
		case 7: // 0x771CF0: animation step (0x8DD220); at cue 4 hidden
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) switched = true;
			if (phase_next(4)) U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 4);
			break;
		case 8: // 0x771D20: at cue 5 shown, animation 4
			if (phase_next(5)) { U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) & 0xFB); x::au_re_Battle_ReadAnimation_7(nx, 4); switched = true; }
			break;
		case 9: // 0x771D50: at the animation's end (next state)
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) switched = true;
			break;
		case 10: // 0x771D70: at the animation's end hidden + finished
			if (x::au_re_Battle_ReadAnimation_8(nx) == 1) { U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5); switched = true; }
			break;
		default:
			break;
		}
		return (U8(nx, 0x26) & 4) == 0;
	}

	// the camera block (0x1D97700, 0xB0 bytes) of the next tick: the battle builds a tick's view (0x5033E0) from the camera words
	// the tick before left (CamWords below); false = no live camera noted (the current block is used)
	static bool NextCamBlock(uint8_t out[0xB0]);

	// Angelo's bone point `bone` on the next tick (the halo's 0x772D90 runs after his draw): his model matrix as the renderer
	// builds it (0x7719A0: rotation +0x3C, the next position, scale +0x114) and the next pose's bone matrices (set by the caller),
	// through GetEffectSpawnPosition with the next tick's camera block. The camera block and the Field_Alloc scratch are put back
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

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];
	// Angelo's next-tick copy and pose (the halo's height on the next tick follows them)
	alignas(4) static uint8_t g_angelo_nx[0x160];
	static Pose g_angelo_next;
	static bool g_angelo_known = false;
	static uint32_t g_angelo_node = 0;

	// a creature (Angelo / Rinoa's double) at num / den: the bone matrices of the pose half way to the next tick's, the position
	// half way
	static void CreatureHeld(const Rec &r, int num, int den)
	{
		const bool angelo = r.kind == HK_ANGELO;
		if (angelo) g_angelo_known = false;
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
		const bool draws = still_runs(cur, nx) && (angelo ? angelo_next(nx, switched) : double_next(nx, switched));
		next.get(sk);                           // the next tick's pose (the copy's animation reads)
		memcpy((void *)sk, sk_now, skn);
		if (angelo)
		{
			memcpy(g_angelo_nx, g_nx, sizeof(g_angelo_nx));
			g_angelo_next = next;
			g_angelo_known = draws;
			g_angelo_node = r.node;
		}
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

	// a play (0x76A250 = a_73C280) of the node `live` with the placement of `place`, in between
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
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)im_76A380, (int)L, num, den);
	}

	// a noted play at num / den: fixed placement; the next tick plays it (state 1) unless its task was released or the moon play
	// finishes at cue 5 (0x76C6B0); paused on the next tick: the same frame; the moon play's texture scroll +0x8E (7 bits, the
	// task's -2 after the play: the live node holds the next tick's) half way
	static void ItemHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x474);
		memcpy(g_mid, r.snap, 0x474);
		const bool moon = U32(cur, 8) == ORIG_MoonPlay;
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1 && !(moon && phase_next(5));
		if (num >= den && !draws) return;
		const bool paused = pause_next() != 0;
		if (draws && !paused && moon)
		{
			const int32_t s = U16(cur, 0x8E) & 0x7F, n = U16(nx, 0x8E) & 0x7F;
			const int32_t d = ((n - s + 0x40) & 0x7F) - 0x40;
			U16(mid, 0x8E) = (uint16_t)((s + d * num / den) & 0x7F);
		}
		PlayHeld(r.node, mid, draws && !paused ? num : 0, den);
	}

	// a ring particle (0x772060 / 0x772990, draw 0x7720C0) at num / den: position half way to its next step (its state 1
	// 0x7727B0 / 0x772A10 on a copy: sequence frame, rise, damping); the sequence frame (+0x50) switches on the real frame
	static void RingHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x90);
		memcpy(g_mid, r.snap, 0x90);
		bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (draws)
		{
			if (U32(nx, 8) == ORIG_Ring1) im_7727B0(nx);
			else im_772A10(nx);
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) im_7720C0(nx); return; }
		if (draws) for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		im_7720C0(mid);
	}

	// a moon model (0x771660, draw 0x7716C0) at num / den: x drift half way to its next step (0x771880 on a copy: +0x9A += +0x7C,
	// x = +0x9A / 16; hidden at cue 4)
	static void MoonHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x9C);
		memcpy(g_mid, r.snap, 0x9C);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1 && !phase_next(4);
		if (draws)
		{
			U16(nx, 0x9A) = (uint16_t)(U16(nx, 0x9A) + U16(nx, 0x7C));
			U16(nx, 0x1C) = (uint16_t)(S16(nx, 0x9A) / 16);
		}
		if (num >= den) { if (draws) im_7716C0(nx); return; }
		if (draws) lerp16(mid, cur, nx, 0x1C, num, den);
		im_7716C0(mid);
	}

	// the halo (0x772D90, draw 0x772E30) at num / den: its next step on a copy (state 1 fade -0x400 to 0; state 2 hidden at cue 3;
	// spin +0x46 += 0x100; height +0x34 = - the y of Angelo's bone point 0x1D on the next tick); spin, height and fade half way
	static void HaloHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, 0x9C);
		memcpy(g_mid, r.snap, 0x9C);
		bool draws = still_runs(cur, nx);
		if (draws)
		{
			const int32_t st = S8(nx, 0x29);
			if (st == 1)
			{
				U16(nx, 0x50) = (uint16_t)(U16(nx, 0x50) + 0xFC00);
				if (S16(nx, 0x50) <= 0) U16(nx, 0x50) = 0;
			}
			else if (st == 2 && phase_next(3))
				U8(nx, 0x26) = (uint8_t)(U8(nx, 0x26) | 5);
			U16(nx, 0x46) = (uint16_t)((U16(nx, 0x46) + 0x100) & 0xFFF);
			if (g_angelo_known && U32(nx, 0x8C) == g_angelo_node + 0x30)
			{
				const uint32_t angelo = U32(nx, 0x8C) - 0x30;
				const uint32_t sk = sk_of(angelo), skn = sk_size(sk);
				static uint8_t sk_now[SK_MAX];
				if (skn && skn <= SK_MAX)
				{
					memcpy(sk_now, (const void *)sk, skn);
					g_angelo_next.set(sk);
					x::BattleModel_BuildBoneMatricesFromPose(angelo + 0x90);
					alignas(4) static uint8_t an[0x160];
					alignas(4) uint8_t pt[8] = {};
					memcpy(an, g_angelo_nx, sizeof(an));
					angelo_point_next(P(an), 0x1D, P(pt));
					S32(nx, 0x34) = -(int32_t)S16(P(pt), 2);
					memcpy((void *)sk, sk_now, skn);
				}
			}
			draws = (U8(nx, 0x26) & 4) == 0;
		}
		if (num >= den) { if (draws) im_772E30(nx); return; }
		if (draws)
		{
			const int32_t a0 = S16(cur, 0x46), d = ((S16(nx, 0x46) - a0 + 0x800) & 0xFFF) - 0x800; // (the spin wraps at 0x1000)
			U16(mid, 0x46) = (uint16_t)((a0 + d * num / den) & 0xFFF);
			lerp32(mid, cur, nx, 0x34, num, den);
			lerp16(mid, cur, nx, 0x50, num, den);
		}
		im_772E30(mid);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the module's screen flash (0x773090; the engine's 0x8DDEC0 with the pause word 0x25ACD34): its script step on a copy; the
	// draw as 0x773130 draws it (draw-mode packet, four semi-transparent tiles, draw-mode packet, OT base + 0x1C) into the module
	// packet pool
	// ---------------------------------------------------------------------------------------------------------------
	static bool flash_step(uint32_t a, uint32_t paused)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF) return false;
		if (op == 0xFE)
		{
			if (S16(a, 0x3C) >= (int16_t)(uint16_t)b2 && paused == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(a, 0x3A) = ni;
				U32(a, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a, 0x30) = ent;
			if (paused == 0) U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	// the next tick of a flash (0x773090's state, the step) on a copy: false = it does not draw
	static bool flash_next(uint32_t a)
	{
		const uint32_t paused = pause_next();
		const int32_t st = S8(a, 0x29);
		if (st == 0)
		{
			return false; // (0x773100: the delay; the step and its draw come the tick after)
		}
		if (st != 1) return false;
		return flash_step(a, paused);
	}
	static void flash_draw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		uint32_t pkt = MEM<uint32_t>(CUR_A);
		x::sub_45BFC0(pkt, 0, 0, x::sub_45C690(0, 2, 0x280, 0) & 0xFFFF, 0);
		x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
		pkt += 0xC;
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++) U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		x::sub_45BFC0(pkt, 0, 0, x::sub_45C690(0, 2, 0x280, 0) & 0xFFFF, 0);
		x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
		MEM<uint32_t>(CUR_A) = pkt + 0xC;
	}
	static void FlashHeld(const Rec &r, int num, int den)
	{
		alignas(4) uint8_t b[0x40];
		memcpy(b, (const void *)r.node, sizeof(b));
		const bool draws = still_runs(P(r.snap), P(b)) && flash_next(P(b));
		const uint32_t col = U32(P(r.snap), 0x30), next = U32(P(b), 0x30);
		if (num >= den) { if (draws) flash_draw(next); return; }
		if (!draws) { flash_draw(col); return; }
		uint32_t c = col & 0xFF000000;
		for (int k = 0; k < 3; k++)
			c |= (uint32_t)(lerp_i((int32_t)((col >> (8 * k)) & 0xFF), (int32_t)((next >> (8 * k)) & 0xFF), num, den) & 0xFF) << (8 * k);
		flash_draw(c);
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

	// the camera block of the next tick (see NextView): false = no live camera noted (the current block is the next one)
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

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x76EAC0: list order; particles = type 0, actors = type 1) run on copies: a particle moves
	// by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0; 4 = key-position path 0x770890; its emission
	// 0x76FE10 and life count: not run), an actor by its motion step (0x741050, the end test without its follow-up spawn,
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
			im_770890(nxt);
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
		// fx091: an actor whose motion step clears its sequence / model (+0x170: its last frame) is not drawn on the next tick
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
		// the system draws on the next tick only while its step stays in phase 1 (0x76D270: phase 2 = done, nothing drawn): else
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
		// the sprite sequences, as 0x76E980 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				im_76EA40(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					im_76EA40(c);
				}
			}
		}
		// the models, as 0x76D300 draws them (0x76D360)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				im_76D360(P(copies[i]), def);
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
		g_mod = &MOD_088;
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
			case HK_ANGELO:
			case HK_DOUBLE: CreatureHeld(r, num, den); break;
			case HK_PLAY: ItemHeld(r, num, den); break;
			case HK_RING: RingHeld(r, num, den); break;
			case HK_MOON: MoonHeld(r, num, den); break;
			case HK_HALO: HaloHeld(r, num, den); break;
			case HK_FLASH: FlashHeld(r, num, den); break;
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

	// the held-frame camera of the module's camera script (the library camera stepper a_73AE10 on the camera block [0x154F918]):
	// act::held_camera with this module current (it saves / puts back the camera block, the words, the GTE registers and the
	// Field_Alloc scratch)
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_088;
		const bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag088_held()
	{
		static const uint32_t HELD[] = { act::moon88::ORIG_Angelo, act::moon88::ORIG_Double, act::moon88::ORIG_MoonPlay, act::moon88::ORIG_HowlPlay,
			act::moon88::ORIG_Ring1, act::moon88::ORIG_Ring2, act::moon88::ORIG_Moon, act::moon88::ORIG_Halo, act::moon88::ORIG_Flash,
			act::moon88::ORIG_ActorSystem, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(88, act::moon88::HeldReady, act::moon88::HeldFrame);
		register_module_camera(88, act::moon88::HeldCamera);
	}
}
