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

// 30 fps part of mag139_sonic_wave.cpp (Sonic Wave, effect 139). Exact vanilla shapes: the logic
// stays at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the
// next one.
//
// Every task draws THEN updates (the main counter after its draws, a layout in its play). A real
// tick records its draws in order with the GTE state and the Field_Alloc scratch each one started
// from; a held frame replays them in the same order on private packets, with the current camera
// (the frame gate's):
//   Main layout - the caster's bone point at its pose midpoint (pose_midpoint + 0x6A9200, the bones
//                 put back), RotY(-facing), with the camera; prim::play_held (every object's exact
//                 in-between record); the last play (counter 0x23) holds.
//   Warp A      - the counter c + num/den in the task's own formulas: t = (c den + num) 4096 /
//                 (30 den), the frame blend weight (c den + num) 512 / den (frames k / k + 1), the
//                 point t of the way from the start to the average, the wave amplitude / fade from
//                 sin(t / 2); the last warp A (counter 0x1D) holds.
//   Warp B      - t = ((c - 0x1E) den + num) 4096 / (40 den), the targets' head / feet box with the
//                 current camera; the last one (counter 0x45) holds.
//   Targets     - the fixed spawn position through the camera; prim::play_held.
// The warp's blended frame / the layouts' morph buffer (magic buffer + 0xF3C) and its vertex
// buffer (+ 0x1584: the out-of-screen flags keep their bits when outside) are private copies of
// the real ones; the warp frame 0x2520C28 is a private copy. The target entities' colour (+0x28)
// and flag 0x800 are real-tick effects only. No task has a draw-only mode. The module packet arena
// alternates per tick: the held packets start where the tick's first draw started. The GTE, the
// Field_Alloc scratch and the word 0x1D9770C are put back.

namespace ff8fx
{
namespace sonic139
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick

	struct Rec
	{
		int kind;                 // PlayKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // main: the counter as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 4];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_verts[VERTS_BYTES]; // blended wave frame / morph buffer (magic buffer + 0xF3C)
	static uint8_t g_held_vbuf[VBUF_BYTES];   // warp vertices (magic buffer + 0x1584)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = l;
		r.counter = kind == PLAY_TARGET ? 0 : ((const MainNode *)n)->counter;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws this part again (else the last picture holds)
	static bool NextDraws(const Rec &r)
	{
		switch (r.kind)
		{
			case PLAY_MAIN: return r.counter + 1 < 0x24;
			case WARP_A: return r.counter + 1 < 0x1E;
			case WARP_B: return r.counter + 1 < 0x46;
			default: return (r.node->flags & 1) != 0;
		}
	}

	static void HeldWarpA(const Rec &r, int num, int den)
	{
		const MainNode *w = (const MainNode *)r.node;
		const uint32_t x = (uint32_t)(r.counter * den + num);
		const int32_t t = (int32_t)((x << 12) / (uint32_t)(30 * den));
		Mat4x3 frame = WarpFrame();
		int16_t z;
		if (!WarpAFrame(w, t, r.counter == 0 ? StartResidue() : 0, frame, &z)) return;
		const int32_t half = t >> 1;
		const int32_t s = ComputeSin(half);
		const uint32_t phase = (x << 9) / (uint32_t)den;
		const uint32_t k = (phase >> 12) & 3;
		BlendFrames((const uint8_t *)(FRAMES + k * FRAME_BYTES), (const uint8_t *)(FRAMES + ((k + 1) & 3) * FRAME_BYTES), g_held_verts, (int32_t)(phase & 0xFFF));
		const int32_t s2 = ComputeSin(half);
		ScreenWarp(MESH, g_held_verts, (((int16_t)(z - 0x200)) >> 2) + 0x11, 0x1000 - s2, s + s, t, g_held_vbuf);
	}

	static void HeldWarpB(const Rec &r, int num, int den)
	{
		const MainNode *w = (const MainNode *)r.node;
		Mat4x3 frame = WarpFrame();
		int32_t z;
		if (!WarpBFrame(w, frame, &z)) return;
		const uint32_t x = (uint32_t)((r.counter - 0x1E) * den + num);
		const int32_t t = (int32_t)((x << 12) / (uint32_t)(40 * den));
		const int32_t half = t >> 1;
		const int32_t s = ComputeSin(half);
		const int32_t s2 = ComputeSin(half);
		ScreenWarp(MESH, MESH + 8, (z >> 2) + 0x11, 0x1000 - s2, s, t, g_held_vbuf);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		memcpy(g_held_verts, (const void *)(TexBase() + VERTS), sizeof(g_held_verts));
		memcpy(g_held_vbuf, (const void *)(TexBase() + VBUF), sizeof(g_held_vbuf));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == WARP_A) HeldWarpA(r, k, den);
			else if (r.kind == WARP_B) HeldWarpB(r, k, den);
			else
			{
				PrimArg arg = {}; // (the matrix pad +0x12 is never read)
				if (r.kind == PLAY_MAIN)
				{
					uint8_t *e = Caster();
					int16_t pos[4] = {};
					pos[3] = StartResidue();
					pose_midpoint(e + 0x60, e + 0x6C, k, den);
					BonePoint(BONE_ENTRY, pos);
					BuildBoneMatricesFromPose(e + 0x60);
					MainBlock(pos, &arg);
					arg.depth = -0x80;
				}
				else
				{
					TargetBlock(((const TargetNode *)r.node)->pos, &arg);
					arg.depth = 0;
				}
				arg.morph = (uint32_t)g_held_verts;
				prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
			}
		}
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag139_held()
	{
		mark_held(sonic139::ORIG_MainTask);
		mark_held(sonic139::ORIG_TargetTask);
		register_module_held(139, sonic139::HeldReady, sonic139::HeldFrame);
	}
}
