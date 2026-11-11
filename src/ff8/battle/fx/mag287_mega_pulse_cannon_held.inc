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

// 30 fps part of mag287_mega_pulse_cannon.cpp (Mega Pulse Cannon, effect 287). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, sparks / rocks move after their draw, an arc fades after
// its draw). A real tick records its draws in order with the GTE state and the Field_Alloc scratch each one started from;
// a held frame replays them in the same order on private packets, with the current camera (the frame gate's):
//   Cannon / beam - at the muzzle point: until counter 0x23 the caster's bone point at the caster's pose midpoint
//             (pose_midpoint, the world bone matrices 0x5095B0 as the master builds them, the bones put back from the pose
//             0x508C90), then the stored one; RotY(caster facing); texture scrolls of counter N (15 Hz steps); each layout
//             through prim::play_held (the exact in-between records).
//   Arcs    - the points rebuilt from the caster's world bone matrices at its pose num/den (pose_midpoint, 0x5095B0, the
//             bones put back from the pose 0x508C90) with the shared-LCG jitter values of tick N (random: 15 Hz; the LCG
//             0x7059E0 stepped back over the tick's 18 calls), brightness half way to the fade the arc's update left; an
//             arc the next tick does not run (ended, or its queue emptied on counter 0x1E) holds the points it drew.
//   Spots   - their fixed frame through the current camera, prim::play_held.
//   Rocks   - position half way to the update's (a bounce included: the update's own step), x angle half way along its
//             step (a lit model: the vertex colours follow the rotation).
//   Sparks  - position half way along their velocity; flipbook frame N held.
// A part the next tick no longer draws holds its picture (state N). Spawns (CRT rand) stay at 15 Hz. Not drawn by the
// module (held by the engine at 15 Hz, as the real ticks set it): the screen flash level (Effect_SetScreenFlash). No task
// has a draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's first draw
// started; the arcs' frame-arena packets go to a private copy of the frame arena the same way. The GTE, the Field_Alloc
// scratch, the word 0x1D9770C, the engine words the sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace megapulse287
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const int MAX_RECS = 2 + 0x40 + 3 + 0x10 + 0x40;

	struct Rec
	{
		int kind;                 // DrawKind
		uint32_t node;
		int aux;                  // arc: its memo
		int16_t counter;          // main: the counter as drawn
		PrimArg arg;              // main / spot: the block as played
		uint8_t copy[0x24];       // rock / spark: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct ArcMemo { uint8_t points[0x80]; int16_t bright[8]; uint32_t residue; uint32_t seed; };

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_narc = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_start = 0;      // frame arena cursor at the tick's first arc
	static Rec g_recs[MAX_RECS];
	static ArcMemo g_arcs[0x40];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_frame[0x40 * 7 * 0x34 + 0x100];
	static uint8_t g_held_morph[0x4000];    // blended vertex frames (magic buffer + 0x1A18)
	static uint8_t g_held_arcs[0x40][0x80]; // the arcs' points at the in-between pose

	// the shared LCG 0x7059E0 (seed 0x2557098): x = (x * 125 + 14) & 0x7FFF; back one step: (x - 14) * 125^-1
	static uint32_t LcgBack(uint32_t x, int steps)
	{
		for (int i = 0; i < steps; i++) x = ((x - 14) * 0x78D5u) & 0x7FFF;
		return x;
	}

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_narc = 0;
		g_packets = 0;
		g_frame_start = 0;
	}

	static Rec *NewRec(int kind, const void *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = (uint32_t)n;
		r.aux = 0;
		r.counter = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_main(const MainNode *m, int kind, const PrimArg *arg)
	{
		Rec *r = NewRec(kind, m);
		if (!r) return;
		r->counter = m->counter;
		r->arg = *arg;
	}

	static void held_note_arc(const ArcNode *a, uint32_t points, uint32_t residue)
	{
		if (g_narc >= 0x40) return;
		Rec *r = NewRec(REC_ARC, a);
		if (!r) return;
		r->aux = g_narc;
		ArcMemo &m = g_arcs[g_narc++];
		memcpy(m.points, (const void *)points, sizeof(m.points));
		for (int k = 0; k < 8; k++) m.bright[k] = a->bright[k][0];
		m.residue = residue;
		m.seed = LcgBack(var<uint32_t>(0x2557098), 18); // the seed before the arc's 6 x 3 jitter calls
		if (!g_frame_start) g_frame_start = FrameCursor();
	}

	static void held_note_spot(const SpotNode *p, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_SPOT, p);
		if (r) r->arg = *arg;
	}

	static void held_note_rock(const RockNode *k)
	{
		Rec *r = NewRec(REC_ROCK, k);
		if (r) memcpy(r->copy, k, sizeof(RockNode));
	}

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK, s);
		if (r) memcpy(r->copy, s, sizeof(SparkNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a node still running after the tick: its task did not end and its queue still holds it
	static bool Alive(uint32_t n, uint32_t orig, const TaskQueue *q)
	{
		const TaskNode *t = (const TaskNode *)n;
		if (!(t->flags & 1) || (uint32_t)t->func != orig) return false;
		int guard = 0;
		for (const TaskNode *x = q->head; x && guard < 0x100; x = x->next, guard++)
			if (x == t) return true;
		return false;
	}

	static void MainHeld(const Rec &r, int num, int den)
	{
		const MainNode *m = (const MainNode *)r.node;
		const int32_t c = r.counter;
		const int k = c + 1 < 0x4B ? num : 0;
		PrimArg arg = r.arg;
		arg.morph = (uint32_t)g_held_morph;
		Mat4x3 M = {};
		if (c + 1 < 0x24)
		{
			// the muzzle point at the caster's pose k/den (the bones put back)
			uint8_t *e = Caster();
			pose_midpoint(e + 0x60, e + 0x6C, k, den);
			ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
			BonePoint((const int16_t *)POINT_Muzzle, M.t);
			BuildBoneMatricesFromPose(e + 0x60);
		}
		else
		{
			M.t[0] = m->point[0];
			M.t[1] = m->point[1];
			M.t[2] = m->point[2];
		}
		RotY(*(const int16_t *)(Caster() + 0xE), &M);
		if (r.kind == REC_BEAM) M.t[1] = *(const int16_t *)(Caster() + 0x24);
		ComposeAffineTransform(&Camera(), &M, &arg.m);
		prim::play_held((prim::Layout *)(r.kind == REC_CANNON ? m->cannon : m->beam), PrimCallback, (int)&arg, k, den);
	}

	static void ArcHeld(const Rec &r, int num, int den)
	{
		const ArcNode *a = (const ArcNode *)r.node;
		const int k = Alive(r.node, ORIG_ArcTask, QArcs()) ? num : 0;
		const ArcMemo &m = g_arcs[r.aux];
		const uint32_t b = (uint32_t)FieldAlloc(0x80);
		memcpy((void *)b, k ? g_held_arcs[r.aux] : m.points, sizeof(m.points));
		for (int j = 0; j < 8; j++) S16(b, 0x10 * j + 0xE) = lerp16(m.bright[j], k ? a->bright[j][0] : m.bright[j], k, den);
		ArcDraw(b, m.residue);
		FieldFree(0x80);
	}

	static void SpotHeld(const Rec &r, int num, int den)
	{
		const SpotNode *p = (const SpotNode *)r.node;
		PrimArg arg = r.arg;
		RotY(0, &arg.m);
		arg.m.t[0] = p->pos[0];
		arg.m.t[1] = p->pos[1];
		arg.m.t[2] = p->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		prim::play_held((prim::Layout *)p->layout, PrimCallback, (int)&arg, num, den);
	}

	static void RockHeld(const Rec &r, int num, int den)
	{
		const RockNode *d = (const RockNode *)r.copy;
		RockNode k = *d;
		if (Alive(r.node, ORIG_RockTask, QRocks()))
		{
			const RockNode *n = (const RockNode *)r.node;
			for (int j = 0; j < 3; j++) k.pos[j] = lerp16(d->pos[j], n->pos[j], num, den);
			// the x angle num/den of its step (0x200..0x3FF: less than half a turn)
			k.angles[0] = (int16_t)(d->angles[0] + d->spin * num / den);
		}
		RockDraw(&k);
	}

	static void SparkHeld(const Rec &r, int num, int den)
	{
		SparkNode s = *(const SparkNode *)r.copy;
		const int k = s.frame + 1 < s.life ? num : 0;
		for (int j = 0; j < 3; j++) s.pos[j] = lerp16(s.pos[j], (int16_t)(s.pos[j] + s.vel[j]), k, den);
		SparkDraw(&s);
	}

	// the points of the arcs the next tick runs, at the caster's pose num/den with their tick-N jitter
	static void ArcPointsHeld(int num, int den)
	{
		bool any = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_ARC && Alive(g_recs[i].node, ORIG_ArcTask, QArcs())) any = true;
		if (!any) return;
		uint8_t *e = Caster();
		pose_midpoint(e + 0x60, e + 0x6C, num, den);
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind != REC_ARC || !Alive(r.node, ORIG_ArcTask, QArcs())) continue;
			uint32_t x = g_arcs[r.aux].seed;
			const uint32_t b = ArcPoints((const ArcNode *)r.node, [&x]() { x = (x * 125 + 14) & 0x7FFF; return (int32_t)x; });
			memcpy(g_held_arcs[r.aux], (const void *)b, 0x80);
			FieldFree(0x80);
		}
		BuildBoneMatricesFromPose(e + 0x60);
	}

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		// the arcs' frame-arena packets go to a copy of what the tick wrote there
		const uint32_t frame = FrameCursor();
		if (g_frame_start && frame > g_frame_start)
		{
			const uint32_t used = frame - g_frame_start;
			memcpy(g_held_frame, (const void *)g_frame_start, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
		}
		FrameCursor() = (uint32_t)g_held_frame;
		ArcPointsHeld(num, den);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_CANNON:
			case REC_BEAM: MainHeld(r, num, den); break;
			case REC_ARC: ArcHeld(r, num, den); break;
			case REC_SPOT: SpotHeld(r, num, den); break;
			case REC_ROCK: RockHeld(r, num, den); break;
			default: SparkHeld(r, num, den); break;
			}
		}
		FrameCursor() = frame;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}

	static void register_mag287_held()
	{
		mark_held(ORIG_MainTask);
		mark_held(ORIG_ArcTask);
		mark_held(ORIG_SpotTask);
		mark_held(ORIG_RockTask);
		mark_held(ORIG_SparkTask);
		register_module_held(287, HeldReady, HeldDraw);
	}
}
}
