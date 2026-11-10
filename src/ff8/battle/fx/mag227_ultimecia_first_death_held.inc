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

// 30 fps part of mag227_ultimecia_first_death.cpp (Ultimecia First Death, effect 227). Exact vanilla shapes: the logic
// stays at 15 Hz; a held frame redraws the real tick's draws (logged in order with the GTE registers and the Field_Alloc
// scratch they started from) at num / den of the way to the next tick's draw:
//   layouts  prim::play_held (every object's exact in-between record) at the block position half way to the one the
//            next tick draws at: layout A follows the caster point (0x693990) - the next tick's point is taken from her
//            next pose (pose_midpoint at 1, her real bone matrices rebuilt after) - and rises / flies to the layout
//            point (its 0x106 start = the next tick's caster point); layouts B / C are fixed. A layout the next tick
//            does not draw holds below 1 and is not drawn at 1. Blended vertex frames: arena-A ticks into the private
//            arena's head (the original's blend buffer runs into packet arena A), else a private buffer.
//   sparks   (drawn THEN moved) position half way along the move (4 x velocity), flipbook frame N (N + 1's at 1); a
//            spark whose life ends with this tick holds below 1 and is not drawn at 1; sparks born this tick (after
//            the draw: random points of her model) appear on the next real tick.
//   feathers (draw THEN update) angles half way to the turned ones, the x / y scale and the fade half way to the next
//            counter's, the translation half way to the next tick's caster point; the last draw holds below 1 and is
//            not drawn at 1.
// Spawns, sounds, battle text, the caster's flag bit 2 and the voice slot stay on the real ticks. No task has a
// draw-only mode. The camera is the engine's camera animation (no camera word written by the module). The module
// packet arena alternates per tick: the held packets start where the tick's first draw started.

namespace ff8fx
{
namespace ultimecia227
{
	enum DrawKind { K_LAYOUT = 1, K_SPARKS = 2, K_FEATHER = 3 };
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		MainNode *main;           // layout: the main node
		int which;                // layout: 0 = A, 1 = B, 2 = C
		int16_t counter;          // layout: the main task's counter as drawn
		int16_t pos[4];           // layout: the block position as drawn
		int32_t depth;            // layout: the block depth
		FeatherNode feather;      // feather: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_end_tick = 0xFFFFFFFF;  // the master ended on this tick (its feather queue is dropped)
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[3 + 1 + 0x10];
	static Spark g_sparks[0x80];            // the spark pool as drawn
	static Spark g_held_sparks[0x80];       // the in-between pool a held frame draws
	static uint8_t g_held_arena[MORPH_GAP + ARENA]; // [blend buffer][packets] (arena A layout)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_end()
	{
		g_end_tick = g_real_tick;
	}

	static Rec *NewRec(int kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(MainNode *m, int which, const int16_t *pos, int32_t depth)
	{
		Rec *r = NewRec(K_LAYOUT);
		if (!r) return;
		r->main = m;
		r->which = which;
		r->counter = m->counter;
		memcpy(r->pos, pos, sizeof(r->pos));
		r->depth = depth;
	}

	static void held_note_sparks()
	{
		if (!NewRec(K_SPARKS)) return;
		memcpy(g_sparks, (const void *)(TexBase() + SPARKS), sizeof(g_sparks));
	}

	static void held_note_feather(TaskNode *n)
	{
		Rec *r = NewRec(K_FEATHER);
		if (r) r->feather = *(const FeatherNode *)n;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the caster point the next tick's master takes (0x693990 on her next pose); her real bone matrices rebuilt after
	static void NextCasterPoint(int16_t *out)
	{
		uint8_t *caster = Caster();
		pose_midpoint(caster + 0x60, caster + 0x6C, 1, 1);
		const uint8_t *bones = **(uint8_t *const *const *)(caster + 0x64) + 0x10;
		const int16_t *v = (const int16_t *)POINT_Caster;
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), (const Mat4x3 *)(bones + (uint32_t)mul32(v[3], 0x30) + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
		BuildBoneMatricesFromPose(caster + 0x60);
	}

	// the position the next tick draws the record's layout at; false when the next tick does not draw it
	static bool LayoutNext(const Rec &r, const int16_t *next_point, int16_t *out)
	{
		const int32_t c = r.counter + 1;
		const uint8_t *e = Caster();
		if (r.which == 0)
		{
			if (c > 0x10D) return false;
			if (c == 0x106)
			{
				// layout A's start and the layout point are taken on that tick
				const int16_t from[4] = { next_point[0], next_point[1], next_point[2], CasterPoint()[3] };
				const int16_t lp[4] = { *(const int16_t *)(e + 0x1C), 0, (int16_t)(*(const int16_t *)(e + 0x20) + 0x3E8), LayoutPoint()[3] };
				BlendVectors(from, lp, 0x1000 - 0x100, 0x100, out);
			}
			else LayoutAPos(r.main, c, next_point, out);
			return true;
		}
		if (r.which == 1)
		{
			if (c > 0x115) return false;
			out[0] = (int16_t)(*(const int16_t *)(e + 0x1C) + 0x2C);
			out[1] = (int16_t)(*(const int16_t *)(e + 0x1E) - 0x8EE);
			out[2] = (int16_t)(*(const int16_t *)(e + 0x20) + 0x19A);
			return true;
		}
		if (c > 0x11B) return false;
		memcpy(out, LayoutPoint(), 6);
		return true;
	}

	static void HeldLayout(const Rec &r, const int16_t *next_point, uint32_t morph, int num, int den)
	{
		int16_t next[3];
		const bool nd = LayoutNext(r, next_point, next);
		if (!nd && num == den) return;
		const int k = nd ? num : 0; // the last picture holds
		PrimArg arg;
		for (int j = 0; j < 3; j++) arg.pos[j] = k ? lerp16(r.pos[j], next[j], k, den) : r.pos[j];
		arg.pos[3] = r.pos[3];
		arg.depth = r.depth;
		arg.morph = morph;
		uint8_t *layout = r.which == 0 ? r.main->layout_a : r.which == 1 ? r.main->layout_b : r.main->layout_c;
		prim::play_held((prim::Layout *)layout, PrimCallback, (int)&arg, k, den);
	}

	static void HeldSparks(int num, int den)
	{
		memcpy(g_held_sparks, g_sparks, sizeof(g_held_sparks));
		for (int j = 0; j < 0x80; j++)
		{
			Spark &p = g_held_sparks[j];
			if (p.life == 0) continue;
			if (p.life == 1)
			{
				if (num == den) p.life = 0; // ends with this tick: holds below 1, not drawn at 1
				continue;
			}
			for (int a = 0; a < 3; a++) p.pos[a] = lerp16(p.pos[a], (int16_t)(p.pos[a] + (int16_t)((int16_t)p.vel[a] << 2)), num, den);
			if (num == den) p.life--;
		}
		SparksDraw(g_held_sparks);
	}

	static void HeldFeather(const Rec &r, const int16_t *next_point, int num, int den)
	{
		const FeatherNode &f = r.feather;
		const int32_t c = f.counter;
		const bool nd = c < 0x10 && g_end_tick != g_real_tick;
		if (!nd && num == den) return;
		const int k = nd ? num : 0; // the last picture holds
		int16_t sx, sy;
		FeatherScale(c, sx, sy);
		int32_t t[3];
		FeatherAt(CasterPoint(), t);
		int32_t fade = FeatherFade(c);
		int16_t ry = f.ry, rz = f.rz;
		if (k)
		{
			int16_t sx1, sy1;
			int32_t t1[3];
			FeatherScale(c + 1, sx1, sy1);
			FeatherAt(next_point, t1);
			for (int j = 0; j < 3; j++) t[j] = lerp_i(t[j], t1[j], k, den);
			fade = lerp_i(fade, FeatherFade(c + 1), k, den);
			sx = lerp16(sx, sx1, k, den);
			sy = lerp16(sy, sy1, k, den);
			ry = lerp_angle(f.ry, (int16_t)(f.ry + f.dry), k, den);
			rz = lerp_angle(f.rz, (int16_t)(f.rz + f.drz), k, den);
		}
		FeatherDraw(fade, sx, sy, ry, rz, t);
	}

	static void HeldDraw(int num, int den)
	{
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t morph = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_morph;
		HeldScope scope(PacketCursor(), g_held_arena + MORPH_GAP, ARENA, g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		// the next tick's caster point (layout A and the feathers follow it)
		int16_t next_point[4];
		memcpy(next_point, CasterPoint(), sizeof(next_point));
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == K_FEATHER || (g_recs[i].kind == K_LAYOUT && g_recs[i].which == 0))
			{
				NextCasterPoint(next_point);
				break;
			}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == K_LAYOUT) HeldLayout(r, next_point, morph, num, den);
			else if (r.kind == K_SPARKS) HeldSparks(num, den);
			else HeldFeather(r, next_point, num, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag227_held()
	{
		mark_held(ultimecia227::ORIG_MainTask);
		mark_held(ultimecia227::ORIG_FeatherTask);
		register_module_held(227, ultimecia227::HeldReady, ultimecia227::HeldDraw);
	}
}
