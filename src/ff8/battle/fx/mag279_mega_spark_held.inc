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

// 30 fps part of mag279_mega_spark.cpp (Mega Spark, effect 279). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, a bolt's points move by their velocity after the ribbon,
// an arc fades after its draw). A real tick records its draws in order with the GTE state and the Field_Alloc scratch each
// one started from; a held frame replays them in the same order on private packets, with the current camera (the frame
// gate's):
//   Bolts   - the ribbon of the points the tick drew: each point half way along its velocity, its brightness half way to
//             the next tick's fade step (exact: the fade is deterministic); projected (through the GTE matrix half way from
//             tick N's to the next tick's: the module never loads it for the bolts) and edged like the real ribbon. The
//             points the next tick appends (module LCG) and frees stay at 15 Hz.
//   Rings   - (main counters 0x0F..0x1E) at the caster's bone anchors at the caster's pose midpoint (pose_midpoint, the
//             world bone matrices 0x5095B0 as the master builds them, the bones put back from the pose 0x508C90), RotX of
//             the angle half way (4 x counter); each layout through prim::play_held (the exact in-between records).
//   Orb     - (counters 0x18..0x3B) the same at the bone point 0x10B6F80, RotY(0).
//   Arcs    - the points the tick drew (bones of tick N, shared-LCG jitter of tick N: random, 15 Hz), brightness half way
//             to the fade the arc's update left.
//   Prims   - their fixed frame through the current camera, prim::play_held.
//   Sparks  - flipbook frame N held (they do not move).
// A part the next tick no longer draws (a bolt or arc that ended, the last ring / orb counter) holds its picture (state N).
// No task has a draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's first
// draw started; the arcs' frame-arena packets go to a private copy of the frame arena the same way. The GTE, the
// Field_Alloc scratch, the word 0x1D9770C, the engine words the sprite draws touch and the caster's bone matrices are put
// back.
// The caster (moved and turned by the main task: +0x20 / +0x0C, continuous at every counter) is listed to the frame gate
// as an entity the effect drives (HeldEntities), with no discrete change.

namespace ff8fx
{
namespace megaspark279
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const int MAX_RECS = 0x30 + 4 + 0x40 + 0x13 + 0x10;
	static const int MAX_POINTS = 0x180;

	struct PointMemo { int16_t pos[3], vel[3], bright, step; };
	struct Rec
	{
		int kind;                 // DrawKind
		uint32_t node;
		int aux;                  // bolt: first point memo; ring: its index; arc: its memo
		int count;                // bolt: points
		int16_t counter;          // main: the counter as drawn
		int16_t angle;            // ring: the RotX angle as drawn
		uint8_t copy[0x40];       // prim: its frame; spark: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct ArcMemo { uint8_t points[0x80]; int16_t bright[8]; uint32_t residue; };

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npts = 0, g_narc = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_start = 0;      // frame arena cursor at the tick's first arc
	static Rec g_recs[MAX_RECS];
	static PointMemo g_pts[MAX_POINTS];
	static ArcMemo g_arcs[0x40];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_frame[0x40 * 7 * 0x34 + 0x100];
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x7A6C)
	static uint8_t g_held_points[MAX_POINTS * 0x28];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npts = 0;
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
		r.count = 0;
		r.counter = 0;
		r.angle = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a bolt about to draw its ribbon: its points as drawn (position before the move, velocity, brightness, fade step)
	static void held_note_bolt(const BoltNode *b)
	{
		if (b->count < 2 || g_npts + b->count > MAX_POINTS) return;
		Rec *r = NewRec(REC_BOLT, b);
		if (!r) return;
		r->aux = g_npts;
		for (uint32_t p = b->head; p; p = U32(p, 0x24))
		{
			PointMemo &m = g_pts[g_npts++];
			for (int k = 0; k < 3; k++)
			{
				m.pos[k] = S16(p, 2 * k);
				m.vel[k] = S16(p, 8 + 2 * k);
			}
			m.bright = S16(p, 0x20);
			m.step = S16(p, 0x22);
			if (++r->count == b->count) break;
		}
	}

	static void held_note_main(const MainNode *m, int kind, int ring, const PrimArg *)
	{
		Rec *r = NewRec(kind, m);
		if (!r) return;
		r->aux = ring;
		r->counter = m->counter;
		r->angle = *(const int16_t *)(Caster() + 0xC);
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
		if (!g_frame_start) g_frame_start = FrameCursor();
	}

	static void held_note_prim(const PrimNode *p, const PrimArg *)
	{
		Rec *r = NewRec(REC_PRIM, p);
		if (r) memcpy(r->copy, &p->m, sizeof(Mat4x3));
	}

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK, s);
		if (r) memcpy(r->copy, s, sizeof(SparkNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a node still running after the tick (its task did not end)
	static bool Alive(uint32_t n, uint32_t orig)
	{
		const TaskNode *t = (const TaskNode *)n;
		return (t->flags & 1) && (uint32_t)t->func == orig;
	}

	// the brightness the next tick's fade gives (BoltTask: the fade runs before the projection)
	static int16_t FadeNext(int16_t b, int16_t step)
	{
		if (b <= 0) return b;
		const int16_t nb = (int16_t)(b + step);
		return nb <= 0 ? (int16_t)0 : nb;
	}

	// the caster's bone point v at the caster's pose k/den (the bones put back)
	static void HeldBonePoint(const int16_t *v, int k, int den, int32_t P[3])
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		ComputeBonesWorldMatrices(c + 0x60, c + 0x40);
		BonePoint(v, P);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// the GTE rotation / translation in a copy of the control register block (registers 0..7 at GTE_CTRL + 0x6C)
	static uint8_t g_gte_next[GTE_CTRL_SIZE];   // the control registers at the held draw's entry
	static void GteCtrlMatrix(const uint8_t *ctrl, Mat4x3 *m)
	{
		memcpy(&m->m[0][0], ctrl + 0x6C, 0x12);
		memcpy(m->t, ctrl + 0x80, 0xC);
	}

	static void BoltHeld(const Rec &r, int num, int den)
	{
		const int k = Alive(r.node, ORIG_BoltTask) ? num : 0;
		const uint32_t base = (uint32_t)g_held_points;
		for (int i = 0; i < r.count; i++)
		{
			const PointMemo &m = g_pts[r.aux + i];
			const uint32_t p = base + 0x28 * i;
			memset((void *)p, 0, 0x28);
			for (int j = 0; j < 3; j++) S16(p, 2 * j) = lerp16(m.pos[j], (int16_t)(m.pos[j] + m.vel[j]), k, den);
			S16(p, 0x20) = lerp16(m.bright, FadeNext(m.bright, m.step), k, den);
			U32(p, 0x24) = i + 1 < r.count ? p + 0x28 : 0;
		}
		// no task of the module loads the GTE matrix before the bolts project their points: they use the one the GTE holds (left
		// by the battle / the previous tick's last draw); the held frame takes it half way from tick N's to the one the next
		// tick starts with (the GTE state at the held draw's entry)
		Mat4x3 m0, m1, mh;
		GteCtrlMatrix(r.gte + GTE_DATA_SIZE, &m0);
		GteCtrlMatrix(g_gte_next, &m1);
		for (int j = 0; j < 9; j++) (&mh.m[0][0])[j] = lerp16((&m0.m[0][0])[j], (&m1.m[0][0])[j], k, den);
		for (int j = 0; j < 3; j++) mh.t[j] = lerp_i(m0.t[j], m1.t[j], k, den);
		GteSetRotMatrixCtrl(&mh);
		GteSetTransVectorCtrl(&mh);
		for (int i = 0; i < r.count; i++)
		{
			const uint32_t p = base + 0x28 * i;
			GteLoadV0((void *)p);
			GteRTPS();
			GteReadSXY2((void *)(p + 0x10));
			GteReadOTZ((void *)(p + 0x14));
		}
		BoltRibbon(base, r.count);
	}

	static void MainHeld(const Rec &r, int num, int den)
	{
		const MainNode *m = (const MainNode *)r.node;
		const int32_t c = r.counter;
		PrimArg arg = {};
		arg.depth = -0x80;
		arg.morph = (uint32_t)g_held_morph;
		if (r.kind == REC_RING)
		{
			const int k = (uint32_t)(c + 1) >= 0xF && (uint32_t)(c + 1) < 0x1F ? num : 0;
			HeldBonePoint((const int16_t *)(ANCHOR_0 + 8 * r.aux), k, den, arg.m.t);
			RotX(lerp_i(r.angle, (int16_t)(((uint32_t)(c + 1) << 7) >> 5), k, den), &arg.m);
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			prim::play_held((prim::Layout *)m->ring[r.aux], PrimCallback, (int)&arg, k, den);
		}
		else
		{
			const int k = (uint32_t)(c + 1 - 0x18) < 0x24 ? num : 0;
			HeldBonePoint((const int16_t *)POINT_Orb, k, den, arg.m.t);
			RotY(0, &arg.m);
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			prim::play_held((prim::Layout *)m->orb, PrimCallback, (int)&arg, k, den);
		}
	}

	static void ArcHeld(const Rec &r, int num, int den)
	{
		const ArcNode *a = (const ArcNode *)r.node;
		const int k = Alive(r.node, ORIG_ArcTask) ? num : 0;
		const ArcMemo &m = g_arcs[r.aux];
		const uint32_t b = (uint32_t)FieldAlloc(0x80);
		memcpy((void *)b, m.points, sizeof(m.points));
		for (int j = 0; j < 8; j++) S16(b, 0x10 * j + 0xE) = lerp16(m.bright[j], k ? a->bright[j][0] : m.bright[j], k, den);
		ArcDraw(b, m.residue);
		FieldFree(0x80);
	}

	static void PrimHeld(const Rec &r, int num, int den)
	{
		const PrimNode *p = (const PrimNode *)r.node;
		PrimArg arg = {};
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)r.copy, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		arg.depth = -0x80;
		prim::play_held((prim::Layout *)p->layout, PrimCallback, (int)&arg, num, den);
	}

	static void HeldDraw(int num, int den)
	{
		memcpy(g_gte_next, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
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
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_BOLT: BoltHeld(r, num, den); break;
			case REC_RING:
			case REC_ORB: MainHeld(r, num, den); break;
			case REC_ARC: ArcHeld(r, num, den); break;
			case REC_PRIM: PrimHeld(r, num, den); break;
			default: SparkDraw((const SparkNode *)r.copy); break;
			}
		}
		FrameCursor() = frame;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}

	// held-frame entities (fx_held.h HeldEntity, drawn by the frame gate): the caster, moved (+0x20) and turned (+0x0C) by
	// the main task in steps of the counter, continuous everywhere (its words go back at the end on the last step's way)
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (max < 1 || !HeldReady() || !Caster()) return 0;
		held_entity_init(out[0], Caster());
		return 1;
	}

	static void register_mag279_held()
	{
		mark_held(ORIG_BoltTask);
		mark_held(ORIG_MainTask);
		mark_held(ORIG_ArcTask);
		mark_held(ORIG_PrimTask);
		mark_held(ORIG_SparkTask);
		register_module_held(279, HeldReady, HeldDraw);
		register_module_entity(279, HeldEntities);
	}
}
}
