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

// 30 fps part of mag138_saliva.cpp (Saliva, effect 138). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the main counter after its draws, a
// sprite after its draw, the string counter after its draws). A real tick records its draws in order
// with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in
// the same order on private packets, with the current camera (the frame gate's), each layout through
// prim::play_held (every object's exact in-between record):
//   Mouth layouts (+0x2C / +0xD4) - the caster's bone 13 point 0x11E3188 at the caster's pose
//                  midpoint (pose_midpoint, the bones rebuilt from the pose afterwards), RotY(-facing).
//   Target layout (+0x268) - the target point (fixed).
//   String mesh  - the string's state at counter + num/den: progress t of its second sub-step, the
//                  sine, the RotZ angle, the wobble, the point P and the previous point (the first
//                  sub-step's), the z scale and the U scroll, all from the same formulas at the
//                  in-between counter (the exe matrix word 0x11E3148 put back). At 1/2 this is the
//                  state the next tick's first sub-step computes.
//   String ribbon - the chain as drawn: every point num/den of the way to the next tick's (drip,
//                  fade step), projected and ribboned by the vanilla code on private copies. The next
//                  tick's two new points (growth) appear on that tick; at fraction 1 the head pairs
//                  the next tick frees are left out and a chain the next tick does not draw is not
//                  drawn.
//   Sprays       - position and rotation half way to the next tick's; flipbook frame N held.
//   Splats       - fixed position, flipbook frame N held.
// A part the next tick no longer draws (its counter range, its layout or its task ended on the tick;
// every part on the tick the master ends: the main and splat queues empty) holds its picture (state
// N: records at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The
// module packet arena alternates per tick: the held packets start where the tick's first draw
// started; the vertex / blend buffer (magic buffer + 0x11B0) lies 0x360 bytes before arena A
// (+0x1510): on arena-A ticks the held buffer sits 0x360 bytes before the held packets, as in the
// game. The GTE, the Field_Alloc scratch, the exe word 0x11E3148, the engine words the sprite draws
// touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace saliva138
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = ARENA_B - ARENA_A;   // module packet arena per tick
	static const uint32_t VERTS_GAP = ARENA_A - VERTS; // vertex / blend buffer -> arena A
	static const int MAX_POINTS = 0x60;

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // main / string: the counter as drawn; sprites: the frame
		int16_t pos[4];           // mouth / sprites: the position as drawn
		int16_t angle;            // spray: the rotation as drawn
		int first, n;             // ribbon: its points (as drawn) in g_points
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npoints = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 3 * 2 + 8 + 8];
	static Point g_points[MAX_POINTS];      // ribbon points as drawn
	static Point g_copy[MAX_POINTS];        // held chain
	static uint8_t g_held_arena[VERTS_GAP + ARENA]; // [vertex / blend buffer][packets] (arena A layout)
	static uint8_t g_held_verts[0x1000];    // vertex / blend buffer on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npoints = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = nullptr;
		r.counter = counter;
		memset(r.pos, 0, sizeof(r.pos));
		r.angle = 0;
		r.first = 0;
		r.n = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a main-task layout is about to play: the position it plays at (the bone point / the target point)
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const int16_t *pos)
	{
		Rec *r = NewRec(kind, n, ((const MainNode *)n)->counter);
		if (!r) return;
		r->layout = l;
		memcpy(r->pos, pos, 6);
	}

	static void held_note_mesh(TaskNode *n) { NewRec(DRAW_MESH, n, ((const StringNode *)n)->counter); }

	// the string's chain as it is about to be projected and drawn
	static void held_note_ribbon(TaskNode *n)
	{
		const StringNode *s = (const StringNode *)n;
		Rec *r = NewRec(DRAW_RIBBON, n, s->counter);
		if (!r) return;
		r->first = g_npoints;
		for (const Point *p = s->head; p && g_npoints < MAX_POINTS; p = p->next) g_points[g_npoints++] = *p;
		r->n = g_npoints - r->first;
	}

	static void held_note_sprite(int kind, TaskNode *n)
	{
		if (kind == SPR_SPRAY)
		{
			const SprayNode *s = (const SprayNode *)n;
			Rec *r = NewRec(kind, n, s->frame);
			if (r)
			{
				memcpy(r->pos, s->pos, 6);
				r->angle = s->angle;
			}
		}
		else
		{
			const SplatNode *s = (const SplatNode *)n;
			Rec *r = NewRec(kind, n, s->frame);
			if (r) memcpy(r->pos, s->pos, 6);
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static bool Alive(const Rec &r) { return (r.node->flags & 1) != 0; }
	// the layout has frames left after this tick's play (it draws again on the next tick)
	static bool LayoutLive(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}
	static bool MouthDraws(int32_t c) { return (uint32_t)(c - 0x17) < 0xE; }
	static bool TargetDraws(int32_t c) { return (uint32_t)(c - 0x1D) < 0x1A; }
	static bool MeshDraws(int32_t c) { return c >= 1 && c < 0xA; }

	// the caster's bone point at pose k/den (the bones rebuilt from the pose afterwards)
	static void PointAt(int k, int den, int16_t *P)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		BonePoint(POINT, P);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// the string mesh at counter c + num/den (its second sub-step; the first one's point = prev)
	struct HeldMesh
	{
		int32_t angle;     // RotZ
		int32_t wave;      // amp x sin(t / 2)
		int16_t sine;      // sin(t / 2)
		int16_t P[4];
		Mat4x3 M;          // basis scaled in z, at P (before the camera)
		uint16_t u;        // U scroll
	};
	static int32_t HeldT(int32_t x2, int den) { return (int32_t)(((int64_t)x2 << 11) / (10 * den)); }
	static void MeshAt(const StringNode *s, int32_t c, int num, int den, HeldMesh *o)
	{
		const int32_t x = c * den + num;
		const int32_t t = HeldT(2 * x + den, den);
		const int32_t tp = HeldT(2 * x, den);
		const int32_t sine = ComputeSin(t >> 1);
		o->sine = (int16_t)sine;
		o->wave = mul32(s->amp, sine);
		o->angle = (int32_t)(((int64_t)x << 8) / den);
		o->u = (uint16_t)(((x << 4) / den) & 0x7F);
		int16_t prev[4] = {};
		StringPoint(s, tp, mul32(s->amp, ComputeSin(tp >> 1)), prev);
		memset(o->P, 0, sizeof(o->P));
		StringPoint(s, t, o->wave, o->P);
		StringBasis(o->P, prev, &o->M);
		const int16_t z = ScaleZ();
		ScaleZ() = (int16_t)(sine + 0x800);
		MulMatrix3(&o->M, SCALE_Z);
		ScaleZ() = z;
		o->M.t[0] = o->P[0];
		o->M.t[1] = o->P[1];
		o->M.t[2] = o->P[2];
	}

	// the next tick's fade of a point
	static int16_t NextFade(int16_t f) { const int16_t g = (int16_t)(f - 4); return g < 0 ? 0 : g; }
	// the head points the next tick frees (its fade step, then the head pairs at 0x20 or less); -1 =
	// the next tick draws no ribbon (the task ends or keeps fewer than 3 points)
	static int KeptFrom(const Rec &r)
	{
		const Point *p = &g_points[r.first];
		int total = r.n + (r.counter + 1 < 0xA ? 2 : 0);
		int i = 0;
		if (NextFade(p[0].fade) <= 0x20)
		{
			while (i + 1 < r.n && NextFade(p[i + 1].fade) <= 0x20)
			{
				i++;
				if (--total < 2) return -1;
				if (NextFade(p[i].fade) > 0x20) break;
			}
		}
		return total - i < 3 ? -1 : i;
	}

	static void RibbonHeld(const Rec &r, int num, int den)
	{
		int from = 0;
		if (num >= den)
		{
			from = KeptFrom(r);
			if (from < 0) return;
		}
		const int n = r.n - from;
		if (n < 2) return;
		const bool drips = r.counter + 1 >= 0xA;
		for (int i = 0; i < n; i++)
		{
			const Point &h = g_points[r.first + from + i];
			Point &c = g_copy[i];
			c = h;
			// next tick: the drip (counters 10 and up), then the fade step
			if (drips) c.pos[1] = lerp16(h.pos[1], (int16_t)(h.pos[1] + DRIP[h.idx]), num, den);
			c.fade = lerp16(h.fade, NextFade(h.fade), num, den);
			c.next = i + 1 < n ? &g_copy[i + 1] : nullptr;
		}
		StringDraw(g_copy, n);
	}

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldDraw(int num, int den)
	{
		// the master ended on this tick (the main and splat queues are empty): no task draws again, every part holds
		if (!QMain()->head && !QSplats()->head) num = 0;
		// arena A (magic buffer + 0x1510) follows the vertex / blend buffer (+ 0x11B0) at 0x360 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t verts = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_verts;
		HeldScope scope(PacketCursor(), g_held_arena + VERTS_GAP, ARENA, g_packets);
		// GTE data registers 28..31 (square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch, the exe matrix word
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		const int16_t scale_z = ScaleZ();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			PrimArg arg = {};
			arg.morph = verts;
			int k = num;
			switch (r.kind)
			{
			case SPR_SPRAY:
			{
				int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
				int16_t angle = r.angle;
				const SprayNode *s = (const SprayNode *)r.node;
				if (Alive(r))
				{
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], s->pos[j], num, den);
					angle = lerp16(r.angle, s->angle, num, den);
				}
				Replay(r);
				SprayDraw(pos, s->scale, r.counter, angle);
				continue;
			}
			case SPR_SPLAT:
				Replay(r);
				SplatDraw(r.pos, r.counter);
				continue;
			case DRAW_MESH:
			{
				const StringNode *s = (const StringNode *)r.node;
				HeldMesh o;
				MeshAt(s, r.counter, MeshDraws(r.counter + 1) ? num : 0, den, &o);
				Mat4x3 Z;
				RotZ((int16_t)o.angle, &Z);
				GteSetRotMatrix(&Z);
				GteZeroTrans();
				WobbleMesh(MESH, o.wave >> 13, (int16_t *)verts);
				Replay(r);
				MeshRender(&o.M, o.u, verts);
				continue;
			}
			case DRAW_RIBBON:
				Replay(r);
				RibbonHeld(r, num, den);
				continue;
			case PLAY_A:
			case PLAY_B:
			{
				k = MouthDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				int16_t P[4] = { r.pos[0], r.pos[1], r.pos[2], 0 };
				if (k) PointAt(k, den, P);
				RotY((int16_t)-*(const int16_t *)(Caster() + 0xE), &arg.m);
				arg.m.t[0] = P[0];
				arg.m.t[1] = P[1];
				arg.m.t[2] = P[2];
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				arg.depth = -0x80;
				break;
			}
			case PLAY_C:
				k = TargetDraws(r.counter + 1) ? num : 0;
				BlockAt(&arg, ((const MainNode *)r.node)->pos);
				arg.depth = 0;
				break;
			}
			Replay(r);
			prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		ScaleZ() = scale_z;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag138_held()
	{
		mark_held(saliva138::ORIG_MainTask);
		mark_held(saliva138::ORIG_StringTask);
		mark_held(saliva138::ORIG_SprayTask);
		mark_held(saliva138::ORIG_SplatTask);
		register_module_held(138, saliva138::HeldReady, saliva138::HeldDraw);
	}
}
