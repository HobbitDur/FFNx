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

// 30 fps part of mag130_eerie_sound_wave.cpp (Eerie Sound Wave, effect 130). Exact vanilla shapes:
// the logic stays at 15 Hz, a held frame draws the state num/den of the way from the last real tick
// to the next one.
//
// Every task draws THEN updates (its layout is played before its counter moves). A real tick
// records every layout play in order with the object frame it built (before the camera is composed
// in), the GTE state and the Field_Alloc scratch it started from. A held frame replays them in the
// same order on private packets, each through prim::play_held (every object's exact in-between
// record), the frame composed with the current camera (the frame gate's):
//   Emitter - the anchor = the caster's bone 0xB at its pose midpoint (pose_midpoint +
//             GetEffectSpawnPosition, the bones put back), lifted by 200, turned by the facing.
//   Waves   - the target's default effect position at its pose midpoint, the ease weight half way
//             between the tick's and the next tick's sine (on the target from counter 0x18), the
//             start -> target basis.
//   Impacts - the frame as the tick built it (fixed anchor, m[2][2] as the tick read it).
// A layout the next tick no longer plays (its task ended on the tick) holds its picture (tick N's
// frame, records at N). Spawns stay at 15 Hz. No task has a draw-only mode. The module packet arena
// alternates per tick: the held packets start where the tick's first draw started; the blend buffer
// (magic buffer + 0xC7C) lies 0x300 bytes before arena A (+0xF7C): on arena-A ticks the held blend
// buffer sits 0x300 bytes before the held packets, as in the game. Module globals, the magic buffer,
// the GTE, the Field_Alloc scratch, the word 0x1D9770C (anchors) and the entities' bone matrices
// are put back.

namespace ff8fx
{
namespace eerie130
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x18000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = 0xF7C - 0xC7C; // blend buffer -> arena A

	struct Rec
	{
		int kind;
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		Mat4x3 local;             // the object frame before the camera
		int16_t counter;          // emitter / wave counter as drawn
		int16_t start[3];         // wave: start
		uint8_t *entity;          // wave: target
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 0xC + 3];
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const Mat4x3 *local)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = l;
		r.local = *local;
		r.counter = 0;
		r.entity = nullptr;
		if (kind == PLAY_EMITTER) r.counter = ((const EmitterNode *)n)->counter;
		if (kind == PLAY_WAVE)
		{
			const WaveNode *w = (const WaveNode *)n;
			r.counter = w->counter;
			memcpy(r.start, w->start, sizeof(r.start));
			r.entity = w->entity;
		}
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick plays the layout again: its task is alive and moved on
	static bool NextPlays(const Rec &r)
	{
		if (!(r.node->flags & 1)) return false;
		if (r.kind == PLAY_EMITTER) return ((const EmitterNode *)r.node)->counter == (int16_t)(r.counter + 1);
		if (r.kind == PLAY_WAVE) return ((const WaveNode *)r.node)->counter == (int16_t)(r.counter + 1);
		return true;
	}

	// the wave's ease weight at counter c (on the target from 0x18)
	static int32_t WaveWeight(int32_t c)
	{
		if (c >= 0x18) return 0x1000;
		return ComputeSin(shl32(c, 10) / 24);
	}

	// the object frame num/den of the way to the next tick (before the camera)
	static void HalfLocal(const Rec &r, int num, int den, Mat4x3 *m)
	{
		*m = r.local;
		if (num == 0) return;
		if (r.kind == PLAY_EMITTER)
		{
			int16_t sp[4] = {};
			uint8_t *e = Caster();
			pose_midpoint(e + 0x60, e + 0x6C, num, den);
			GetEffectSpawnPosition(e, 0xB, 0x4000, sp);
			BuildBoneMatricesFromPose(e + 0x60);
			sp[1] = (int16_t)(sp[1] - 200);
			RotY((int16_t)-*(const int16_t *)(e + 0xE), m);
			m->t[0] = sp[0];
			m->t[1] = sp[1];
			m->t[2] = sp[2];
		}
		else if (r.kind == PLAY_WAVE)
		{
			int16_t pos[4] = {};
			uint8_t *e = r.entity;
			pose_midpoint(e + 0x60, e + 0x6C, num, den);
			GetDefaultEffectPosition(e, pos);
			BuildBoneMatricesFromPose(e + 0x60);
			int32_t d[3];
			d[0] = (int32_t)pos[0] - r.start[0];
			d[1] = (int32_t)pos[1] - r.start[1];
			d[2] = (int32_t)pos[2] - r.start[2];
			if (r.counter < 0x18)
			{
				const int32_t s0 = WaveWeight(r.counter), s1 = WaveWeight(r.counter + 1);
				const int32_t s = lerp_i(s0, s1, num, den);
				BlendVectors(r.start, pos, 0x1000 - s, s, pos);
			}
			BuildOrthonormalBasis(d, m);
			m->t[0] = pos[0];
			m->t[1] = pos[1];
			m->t[2] = pos[2];
		}
	}

	static void HeldFrame(int num, int den)
	{
		// arena A (magic buffer + 0xF7C) follows the blend buffer (+ 0xC7C) at 0x300 bytes
		const bool arena_a = g_packets == TexBase() + 0xF7C;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 (the basis' square root sets LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextPlays(r) ? num : 0; // the last picture holds
			PrimArg arg;
			HalfLocal(r, k, den, &arg.m);
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.depth = r.kind == PLAY_WAVE ? 0 : -0x80;
			arg.morph = morph;
			prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag130_held()
	{
		mark_held(eerie130::ORIG_EmitterTask);
		mark_held(eerie130::ORIG_WaveTask);
		mark_held(eerie130::ORIG_ImpactTask);
		register_module_held(130, eerie130::HeldReady, eerie130::HeldFrame);
	}
}
