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

// 30 fps part of mag282_evil_eye.cpp (Evil-Eye, effect 282). Exact vanilla shapes: the logic stays at 15 Hz, a held frame
// draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, a spark rises after its draw). A real tick records its draws in
// order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in the same order on
// private packets, with the current camera (the frame gate's; the effect camera 0x2507F08 rebuilt from it by 0x67AAE0 and put
// back), each layout through prim::play_held (every object's exact in-between record):
//   Eye    - the eye point (the caster's bone 3) at the caster's pose midpoint (pose_midpoint, the bones rebuilt from the pose
//            afterwards), RotY(-caster facing) at it, composed with the camera.
//   Hits   - their fixed point (RotY(0)) composed with the camera.
//   Sparks - half way to their next height (-10), flipbook frame N held.
// A part the next tick no longer draws (the eye's counter 27, an ended hit layout or spark) holds its picture (state N:
// records at N, pose N). Spawns (CRT rand) stay at 15 Hz; the screen-flash level is the engine's (its real-tick value). No task
// has a draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's first draw
// started. The GTE, the Field_Alloc scratch, the word 0x1D9770C, the engine words the sprite draws touch, the effect camera
// and the caster's bone matrices are put back.

namespace ff8fx
{
namespace evileye282
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	enum : uint32_t { REC_EYE = 1, REC_HIT = 2, REC_SPARK = 3 };

	struct Rec
	{
		uint32_t kind;
		const void *node;
		int16_t counter;          // eye: the main counter as drawn; spark: the frame
		int16_t pos[3];           // spark: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 3 + 0x80];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x142C)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind, const void *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_eye(const MainNode *m)
	{
		Rec *r = NewRec(REC_EYE, m);
		if (r) r->counter = m->counter;
	}

	static void held_note_hit(const HitNode *h) { NewRec(REC_HIT, h); }

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK, s);
		if (!r) return;
		r->counter = s->counter;
		memcpy(r->pos, s->pos, sizeof(r->pos));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the node runs its task on the next tick (the executor clears word +0 of a node that ended; nodes are only reused by spawns
	// of a later tick)
	static bool Live(const void *node, uint32_t task) { return (var<uint16_t>((uint32_t)node) & 1) != 0 && var<uint32_t>((uint32_t)node + 8) == task; }

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		const Mat4x3 effcam = EffCam();
		OrthonormalizeCamera(&Camera(), &EffCam());
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
			PrimArg arg;
			int k = 0;
			if (r.kind == REC_EYE)
			{
				k = (uint32_t)(r.counter + 1 - 2) < 0x1A ? num : 0; // the last picture holds
				uint8_t *c = Caster();
				pose_midpoint(c + 0x60, c + 0x6C, k, den);
				EyeArg(&arg);
				BuildBoneMatricesFromPose(c + 0x60);
			}
			else if (r.kind == REC_HIT)
			{
				k = Live(r.node, ORIG_HitTask) ? num : 0;
				HitArg((const HitNode *)r.node, &arg);
			}
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == REC_SPARK)
			{
				SparkNode s = {};
				s.counter = r.counter;
				memcpy(s.pos, r.pos, sizeof(s.pos));
				// half way to the next height (its last frame holds)
				if (r.counter + 1 < 0x10) s.pos[1] = lerp16(r.pos[1], (int16_t)(r.pos[1] - 0xA), num, den);
				SparkDraw(&s);
				continue;
			}
			arg.morph = (uint32_t)g_held_morph;
			prim::play_held((prim::Layout *)(r.kind == REC_EYE ? ((const MainNode *)r.node)->eye : ((const HitNode *)r.node)->layout), PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
		EffCam() = effcam;
		var<int16_t>(0x1D9770C) = w70c;
	}
}

	static void register_mag282_held()
	{
		mark_held(evileye282::ORIG_MainTask);
		mark_held(evileye282::ORIG_HitTask);
		mark_held(evileye282::ORIG_SparkTask);
		register_module_held(282, evileye282::HeldReady, evileye282::HeldDraw);
	}
}
