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

// 30 fps part of mag133_hypnotize.cpp (Hypnotize, effect 133). Exact vanilla shapes: the logic stays
// at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, a sprite's counter after its draw).
// A real tick records its draws in order with the GTE state and the Field_Alloc scratch each play
// started from; a held frame replays them in the same order on private packets, the frame built
// with the current camera (the frame gate's), each layout through prim::play_held (every object's
// exact in-between record):
//   Main    - the caster's default effect position at its pose midpoint (pose_midpoint +
//             GetDefaultEffectPosition, the bones put back), projected and turned into the view-space
//             frame as the task does; the last play (counter 0x1D) holds.
//   Rings   - the table point through the caster's bone at its pose midpoint, through the camera.
//   Targets - the fixed spawn position, the camera rows scaled by the node's word.
//   Sprites - fixed position, flipbook frame N held.
// A layout whose task ended on the tick holds its picture (records at N, pose N). Spawns (CRT
// rand) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates per tick:
// the held packets start where the tick's first draw started; the blend buffer (magic buffer +
// 0xE04) lies 8 bytes before arena A (+0xE0C): on arena-A ticks the held blend buffer sits 8 bytes
// before the held packets, as in the game. The GTE, the Field_Alloc scratch, the word 0x1D9770C,
// the engine words the sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace hypno133
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // PlayKind, 0 = sprite
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // main: the counter as drawn
		SpriteNode sprite;        // sprite: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 8 + 0x10 + 0x20];
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
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const PrimArg *)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		r->layout = l;
		if (kind == PLAY_MAIN) r->counter = ((const MainNode *)n)->counter;
	}

	static void held_note_sprite(const SpriteNode *s)
	{
		Rec *r = NewRec(0, (TaskNode *)s);
		if (!r) return;
		r->sprite = *s;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick plays the layout again
	static bool NextPlays(const Rec &r)
	{
		if (r.kind == PLAY_MAIN) return r.counter + 1 < 0x1E;
		return (r.node->flags & 1) != 0;
	}

	// the parameter block num/den of the way to the next tick, with the current camera
	static void HeldBlock(const Rec &r, int num, int den, PrimArg *arg)
	{
		if (r.kind == PLAY_TARGET)
		{
			const TargetNode *p = (const TargetNode *)r.node;
			ScaledCamera(p->scale, &arg->m);
			CameraAnchor(p->pos, &arg->m);
			arg->depth = -0x80;
			return;
		}
		uint8_t *e = Caster();
		int16_t pos[4] = {};
		pose_midpoint(e + 0x60, e + 0x6C, num, den);
		if (r.kind == PLAY_MAIN) GetDefaultEffectPosition(e, pos);
		else BonePoint(((const RingNode *)r.node)->point, pos);
		BuildBoneMatricesFromPose(e + 0x60);
		if (r.kind == PLAY_MAIN)
		{
			MainFrame(pos, arg);
			arg->depth = 0;
		}
		else
		{
			memcpy(&arg->m.m, &Camera().m, sizeof(arg->m.m));
			CameraAnchor(pos, &arg->m);
			arg->depth = -0x80;
		}
	}

	static void HeldFrame(int num, int den)
	{
		// arena A (magic buffer + 0xE0C) follows the blend buffer (+ 0xE04) at 8 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 (the basis' square root sets LZCS) are not in GTE_DATA
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
			PrimArg arg = {}; // (the matrix pad +0x12 is never read)
			int k = 0;
			if (r.kind != 0)
			{
				k = NextPlays(r) ? num : 0; // the last picture holds
				HeldBlock(r, k, den, &arg);
				arg.morph = morph;
			}
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == 0) SpriteDraw(&r.sprite);
			else prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag133_held()
	{
		mark_held(hypno133::ORIG_MainTask);
		mark_held(hypno133::ORIG_RingTask);
		mark_held(hypno133::ORIG_TargetLayoutTask);
		mark_held(hypno133::ORIG_SpriteTask);
		register_module_held(133, hypno133::HeldReady, hypno133::HeldFrame);
	}
}
