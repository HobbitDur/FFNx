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

// 30 fps part of mag135_reflect_beam.cpp (Reflect Beam, effect 135). Exact vanilla shapes: the logic stays
// at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the sweep / shot counters after their
// draws, a sprite after its draw). A real tick records its draws in order with the GTE state and the
// Field_Alloc scratch each one started from; a held frame replays them in the same order on private
// packets, with the current camera (the frame gate's), each layout through prim::play_held (every
// object's exact in-between record):
//   Sweep      - P = the caster's bone 0x1F at the caster's pose midpoint (pose_midpoint, the bones put
//                back); each segment's end X and its sine half way between this tick's and the next
//                tick's (the values: the sine's angle passes its peak); the layout at P.
//   Shots      - the caster's world bone at its pose midpoint (pose_midpoint, 0x5095B0, the bones
//                rebuilt from the pose afterwards): Q and the basis as the task builds them; the angle
//                a half way (E moves linearly along the shot), the width scale half way (the value);
//                the floor projection and length from that state (an in-between shot parallel to the
//                floor, the game's division by zero, holds state N); the hit layout at the hit point of
//                that state while the shot aims on both ticks (else the hit point as drawn).
//   Reflection layouts - the node's anchor (fixed).
//   Sparks     - their position half way to the next tick's (velocity after its drag); flipbook frame N
//                held. Bursts / glints: fixed position, flipbook frame N held.
// A part the next tick no longer draws (its counter range, its layout or its task ended on the tick) holds its
// picture (state N: records at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only
// mode. The module packet arena alternates per tick: the held packets start where the tick's first
// draw started; the blend buffer (magic buffer + 0x2D44) lies 0x220 bytes before arena A (+0x2F64):
// on arena-A ticks the held blend buffer sits 0x220 bytes before the held packets, as in the game. The
// GTE, the Field_Alloc scratch, the word 0x1D9770C, the beam scale matrix 0x2520E48, the engine words
// the sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace reflect135
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = ARENA_B - ARENA_A;   // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // sweep / shot / reflection: the counter as drawn; sprites: the frame
		int16_t pos[3];           // sprites: the position as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[2 + 8 * 3 + 3 + 0x60];
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
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
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		const int16_t c = kind == PLAY_SWEEP ? ((const SweepNode *)n)->counter : kind == PLAY_REFLECT ? ((const ReflectNode *)n)->counter : ((const ShotNode *)n)->counter;
		Rec *r = NewRec(kind, n, c);
		if (r) r->layout = l;
	}

	static void held_note_sweep(TaskNode *n) { NewRec(DRAW_SWEEP, n, ((const SweepNode *)n)->counter); }
	static void held_note_shot_beam(TaskNode *n) { NewRec(DRAW_SHOT, n, ((const ShotNode *)n)->counter); }

	static void held_note_sprite(int kind, TaskNode *n)
	{
		const SpriteNode *s = (const SpriteNode *)n;
		Rec *r = NewRec(kind, n, s->counter);
		if (r) memcpy(r->pos, s->pos, sizeof(r->pos));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static bool Alive(const Rec &r) { return (r.node->flags & 1) != 0; }
	static bool SweepDraws(int32_t c) { return (uint32_t)(c - 15) < 0x14; }
	static bool ShotAims(int32_t c) { return c < 0xD; }
	// the layout has frames left after this tick's play (it draws again on the next tick)
	static bool LayoutLive(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// P (the caster's bone 0x1F) at pose k/den
	static void SweepAnchor(int k, int den, int16_t *P)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		GetEffectSpawnPosition(c, 0x1F, 0x1000, P);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// the shot's frame at pose k/den: the caster's world bone (0x5095B0 as the master), Q, the basis
	static void ShotFrameAt(const ShotNode *s, int k, int den, int16_t *Q, Mat4x3 *m)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		ComputeBonesWorldMatrices(c + 0x60, c + 0x40);
		Mat4x3 bone;
		memcpy(&bone, ShotBone(s), sizeof(bone));
		BuildBoneMatricesFromPose(c + 0x60);
		ShotFrame(&bone, Q, m);
	}

	// the shot's beam (and its end point) at k/den
	struct HeldShot
	{
		Mat4x3 m;
		int16_t E[4];
		int32_t len;
		int16_t w;
		bool hit;
	};
	static void ShotAt(const ShotNode *s, int32_t c, int k, int den, HeldShot *o)
	{
		const int32_t a0 = ShotAngle(c), a1 = ShotAngle(c + 1);
		for (;;)
		{
			int16_t Q[4] = {};
			ShotFrameAt(s, k, den, Q, &o->m);
			ShotBasis(&o->m, Q);
			const int32_t a = lerp_i(a0, a1, k, den);
			memset(o->E, 0, sizeof(o->E));
			ShotPoint(&o->m, a, o->E);
			// an in-between shot parallel to the floor has no floor point (the game's division by zero):
			// the shot holds state N
			if (k && o->E[1] <= 0 && o->E[1] == Q[1])
			{
				k = 0;
				continue;
			}
			o->hit = ShotProject(Q, a, o->E, &o->len);
			o->w = lerp16(ShotWidth(a0), ShotWidth(a1), k, den);
			return;
		}
	}

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldSweep(const Rec &r, int num, int den)
	{
		const SweepNode *w = (const SweepNode *)r.node;
		const int32_t s0 = r.counter - 15;
		struct Seg { bool on; int16_t P[4]; int16_t X; int32_t sine; } seg[3] = {};
		for (int i = 2; i >= 0; i--)
		{
			const int32_t s = s0 - i;
			if ((uint32_t)s >= 10) continue;
			const int k = (uint32_t)(s + 1) < 10 ? num : 0; // a segment that ends holds
			Seg &g = seg[i];
			g.on = true;
			SweepAnchor(k, den, g.P);
			const int32_t t0 = SegT(s), t1 = SegT(s + 1);
			g.X = lerp16(SegX(w, t0), SegX(w, t1), k, den);
			g.sine = lerp_i(SegSine(t0), SegSine(t1), k, den);
		}
		Replay(r);
		uint8_t *h = AllocHeader(0x58);
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		for (int i = 2; i >= 0; i--)
		{
			if (!seg[i].on) continue;
			if (i != 0)
			{
				*(uint32_t *)(h + 0x1C) = 0xF0;
				*(int32_t *)(h + 0xC) = (i + 4) << 9;
			}
			else *(uint32_t *)(h + 0x1C) = 0x30;
			SegmentRender(h, w, seg[i].P, seg[i].X, seg[i].sine);
		}
		FieldFree(0x58);
	}

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0x2F64) follows the blend buffer (+ 0x2D44) at 0x220 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 (square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch, the module's beam scale matrix
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8], scale[0x12];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		memcpy(scale, &Scale(), sizeof(scale));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			PrimArg arg = {};
			arg.morph = morph;
			int k = num;
			switch (r.kind)
			{
			case SPR_BURST:
			case SPR_GLINT:
			case SPR_SPARK:
			{
				int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
				if (r.kind == SPR_SPARK && Alive(r))
				{
					const SpriteNode *s = (const SpriteNode *)r.node;
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], s->pos[j], num, den);
				}
				Replay(r);
				if (r.kind == SPR_BURST) SpriteDraw(pos, r.counter, SEQ_Burst, 0x1770, -0x400);
				else if (r.kind == SPR_GLINT) SpriteDraw(pos, r.counter, SEQ_Glint, 0x800, -0x400);
				else SpriteDraw(pos, r.counter, SEQ_Spark, 0x800, -0x200);
				continue;
			}
			case DRAW_SWEEP:
				HeldSweep(r, num, den);
				continue;
			case DRAW_SHOT:
			{
				HeldShot o;
				ShotAt((const ShotNode *)r.node, r.counter, ShotAims(r.counter + 1) ? num : 0, den, &o);
				Replay(r);
				ShotBeam(&o.m, o.w, (int16_t)o.len);
				continue;
			}
			case PLAY_SWEEP:
				k = SweepDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				SweepAnchor(k, den, arg.pos);
				arg.depth = -0x80;
				break;
			case PLAY_SHOT:
			{
				k = ShotAims(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				Mat4x3 m;
				ShotFrameAt((const ShotNode *)r.node, k, den, arg.pos, &m);
				arg.depth = -0x80;
				break;
			}
			case PLAY_SHOTHIT:
			{
				const ShotNode *s = (const ShotNode *)r.node;
				k = Alive(r) && s->hit && LayoutLive(r.layout) ? num : 0;
				memcpy(arg.pos, s->hitpos, 8);
				if (k && ShotAims(r.counter) && ShotAims(r.counter + 1))
				{
					// the hit point of the in-between shot (the shot aims on both ticks)
					HeldShot o;
					ShotAt(s, r.counter, num, den, &o);
					if (o.hit) memcpy(arg.pos, o.E, 6);
				}
				arg.depth = 0;
				break;
			}
			case PLAY_REFLECT:
				k = Alive(r) ? num : 0;
				memcpy(arg.pos, ((const ReflectNode *)r.node)->pos, 8);
				arg.depth = -0x80;
				break;
			}
			Replay(r);
			prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		memcpy(&Scale(), scale, sizeof(scale));
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag135_held()
	{
		mark_held(reflect135::ORIG_SweepTask);
		mark_held(reflect135::ORIG_ShotTask);
		mark_held(reflect135::ORIG_ReflectTask);
		mark_held(reflect135::ORIG_BurstTask);
		mark_held(reflect135::ORIG_GlintTask);
		mark_held(reflect135::ORIG_SparkTask);
		register_module_held(135, reflect135::HeldReady, reflect135::HeldDraw);
	}
}
