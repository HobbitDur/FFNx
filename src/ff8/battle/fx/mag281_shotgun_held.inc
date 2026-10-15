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

// 30 fps part of mag281_shotgun.cpp (Shotgun, effect 281). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, a sprite moves after its draw). A real tick records
// its draws in order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them
// in the same order on private packets, with the current camera (the frame gate's), each layout through
// prim::play_held (every object's exact in-between record):
//   Muzzle  - the muzzle point (the caster's bone 23) at the caster's pose midpoint (pose_midpoint, the world bone
//             matrices 0x5095B0 as the master builds them, the bones put back from the pose 0x508C90), in the fixed
//             basis M, composed with the camera.
//   Hits    - the node's frame (the fixed basis R at the hit's position) composed with the camera.
//   Sparks  - their position half way along the velocity they move by (the one before the drag); flipbook frame N
//             held.
// A part the next tick no longer draws (the main task's last counter, an ended hit layout or spark) holds its
// picture (state N: records at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The module
// packet arena alternates per tick: the held packets start where the tick's first draw started; the blend buffer
// (magic buffer + 0x1DE8) lies 0x230 bytes before arena A (+0x2018): on arena-A ticks the held blend buffer sits
// 0x230 bytes before the held packets, as in the game. The GTE, the Field_Alloc scratch, the word 0x1D9770C, the
// engine words the sprite draws touch and the caster's bone matrices are put back; the module's matrices M / R
// (their translations are written by every real tick) are not touched.

namespace ff8fx
{
namespace shotgun281
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // muzzle: the main counter as drawn; sparks: the frame
		int16_t pos[3];           // sparks: the position as drawn
		int16_t vel[3];           // sparks: the velocity they move by after the draw
		uint32_t seq;             // sparks: the sprite sequence
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 6 + 0x80];
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = nullptr;
		r.counter = 0;
		r.seq = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		r->layout = l;
		if (kind == PLAY_MUZZLE) r->counter = ((const MainNode *)n)->counter;
	}

	static void held_note_spark(TaskNode *n)
	{
		Rec *r = NewRec(SPR_SPARK, n);
		if (!r) return;
		const SparkNode *s = (const SparkNode *)n;
		r->counter = s->counter;
		r->seq = s->seq;
		memcpy(r->pos, s->pos, sizeof(r->pos));
		memcpy(r->vel, s->vel, sizeof(r->vel));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		if (r.kind == PLAY_MUZZLE) return r.counter + 1 < 0xE;
		return (r.node->flags & 1) != 0;
	}

	// the muzzle layout's parameter block at k/den: the muzzle point at the caster's pose midpoint
	static void HeldMuzzle(int k, int den, PrimArg *arg)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		ComputeBonesWorldMatrices(c + 0x60, c + 0x40);
		Mat4x3 M = BasisM();
		BonePoint(MUZZLE, M.t);
		BuildBoneMatricesFromPose(c + 0x60);
		ComposeAffineTransform(&Camera(), &M, &arg->m);
		arg->depth = -0x80;
	}

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0x2018) follows the blend buffer (+ 0x1DE8) at 0x230 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			PrimArg arg = {}; // (the matrix pad +0x12 is never read)
			int16_t pos[4] = { r.pos[0], r.pos[1], r.pos[2], 0 };
			if (r.kind == PLAY_MUZZLE) HeldMuzzle(k, den, &arg);
			else if (r.kind == PLAY_HIT)
			{
				Mat4x3 R = BasisR();
				const HitNode *h = (const HitNode *)r.node;
				R.t[0] = h->pos[0];
				R.t[1] = h->pos[1];
				R.t[2] = h->pos[2];
				ComposeAffineTransform(&Camera(), &R, &arg.m);
				arg.depth = -0x80;
			}
			else
			{
				for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
				pos[3] = r.counter; // (the node's frame word follows its position: the shadow camera's V0 pad)
			}
			arg.morph = morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == SPR_SPARK) SpriteDraw(pos, r.counter, r.seq);
			else prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag281_held()
	{
		mark_held(shotgun281::ORIG_MainTask);
		mark_held(shotgun281::ORIG_HitTask);
		mark_held(shotgun281::ORIG_SparkTask);
		register_module_held(281, shotgun281::HeldReady, shotgun281::HeldDraw);
	}
}
