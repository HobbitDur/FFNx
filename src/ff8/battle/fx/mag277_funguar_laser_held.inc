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

// 30 fps part of mag277_funguar_laser.cpp (Funguar's laser, effect 277). Exact vanilla shapes: the logic stays
// at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the beam's counter after its draws,
// a sprite after its draw). A real tick records its draws in order with the GTE state and the
// Field_Alloc scratch each one started from; a held frame replays them in the same order on private
// packets, with the current camera (the frame gate's), each layout through prim::play_held (every
// object's exact in-between record):
//   Beam parts - P = the caster model's vertex at the caster's pose midpoint, T = the target's
//                default effect position at the target's pose midpoint (pose_midpoint, the bones put
//                back: the vertex's own 0x508C90 rebuild / an explicit one), the basis M of their
//                floor-plane direction; the counter-driven values half way between this tick's and
//                the next tick's: the target lift, the beam's width scale (the value: its angle
//                restarts at e = 20, an angle step there would pass the sine's peak) and its fade.
//                Muzzle, ray, impact and the beam model are then built as the task builds them
//                (floor projection, length, scale).
//   Hits       - the node's frame (fixed) composed with the camera.
//   Sparks     - their position half way along the velocity they move by (the one before the
//                drag); flipbook frame N held.
//   Glows      - fixed position, flipbook frame N held.
// A part the next tick no longer draws (its counter range or its task ended on the tick) holds its
// picture (state N: records at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only
// mode. The module packet arena alternates per tick: the held packets start where the tick's first
// draw started; the blend buffer (magic buffer + 0xF24) lies 0x188 bytes before arena A (+0x10AC): on
// arena-A ticks the held blend buffer sits 0x188 bytes before the held packets, as in the game. The
// GTE, the Field_Alloc scratch, the word 0x1D9770C, the engine words the sprite draws touch and the
// entities' bone matrices are put back.

namespace ff8fx
{
namespace laser277
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick
	static const uint32_t MORPH_GAP = ARENA_A - MORPH; // blend buffer -> arena A

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // beam: the counter as drawn; sprites: the frame
		int16_t pos[3];           // sprites: the position as drawn
		int16_t vel[3];           // sparks: the velocity they move by after the draw
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[4 + 8 + 0x10 + 0x10];
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

	static void held_note_play(int kind, TaskNode *n, prim::Layout *l)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		r->layout = l;
		if (kind != PLAY_HIT) r->counter = ((const BeamNode *)n)->counter;
	}

	static void held_note_beam(TaskNode *n)
	{
		Rec *r = NewRec(DRAW_BEAM, n);
		if (!r) return;
		r->counter = ((const BeamNode *)n)->counter;
	}

	static void held_note_sprite(int kind, TaskNode *n, const int16_t *pos, const int16_t *vel, int16_t counter)
	{
		Rec *r = NewRec(kind, n);
		if (!r) return;
		r->counter = counter;
		memcpy(r->pos, pos, sizeof(r->pos));
		if (vel) memcpy(r->vel, vel, sizeof(r->vel));
		else memset(r->vel, 0, sizeof(r->vel));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		const int32_t c1 = r.counter + 1;
		switch (r.kind)
		{
		case PLAY_MUZZLE:
		case PLAY_RAY: return c1 <= 0x32 && (uint32_t)(c1 - 5) < 0x28;
		case DRAW_BEAM: return (uint32_t)(c1 - 15) < 0x1E;
		case PLAY_IMPACT: return c1 <= 0x32 && (uint32_t)(c1 - 0x21) < 0x18; // (the task ends after counter 0x32)
		default: return (r.node->flags & 1) != 0;
		}
	}

	// the beam's frame at k/den: P at the caster's pose midpoint (VertexPoint rebuilds the caster's
	// bone matrices from its pose at its end), T at the target's
	static void HeldFrame(const BeamNode *b, int k, int den, BeamFrame *f)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		VertexPoint(c, P_OBJECT, P_VERTEX, f->P);
		uint8_t *t = b->ent;
		pose_midpoint(t + 0x60, t + 0x6C, k, den);
		GetDefaultEffectPosition(t, f->T);
		BuildBoneMatricesFromPose(t + 0x60);
		f->v[0] = (int32_t)f->T[0] - f->P[0];
		f->v[1] = 0;
		f->v[2] = (int32_t)f->P[2] - f->T[2];
		BuildOrthonormalBasis(f->v, &f->M);
		for (int i = 0; i < 3; i++)
		{
			f->R.m[0][i] = (int16_t)(0 - f->M.m[0][i]);
			f->R.m[1][i] = f->M.m[1][i];
			f->R.m[2][i] = (int16_t)(0 - f->M.m[2][i]);
		}
	}

	// a beam part at k/den: its parameter block (layouts) or its beam model state
	struct HeldBeam
	{
		Mat4x3 M2;
		int32_t len, fade;
		int16_t width;     // the width scale (not its angle: the angle jumps between counters 34 and 35)
	};
	static void HeldBeamPart(const Rec &r, int k, int den, PrimArg *arg, HeldBeam *hb)
	{
		const BeamNode *b = (const BeamNode *)r.node;
		const int32_t c = r.counter;
		BeamFrame f;
		HeldFrame(b, k, den, &f);
		if (r.kind == PLAY_IMPACT)
		{
			ImpactFrame(b, &f.R);
			ComposeAffineTransform(&Camera(), &f.R, &arg->m);
			arg->depth = -0x80;
			return;
		}
		Mat4x3 M;
		BuildOrthonormalBasis(f.v, &M);
		int16_t v[4] = {};
		PointFrame(&M, f.P, v, arg);
		if (r.kind == PLAY_MUZZLE) return;
		const int16_t size = *(const int16_t *)(b->ent + 0x26);
		const Ease e0 = BeamEase(c, size), e1 = BeamEase(c + 1, size);
		int16_t T[4] = { f.T[0], (int16_t)(f.T[1] + lerp_i(e0.lift, e1.lift, k, den)), f.T[2], f.T[3] };
		Mat4x3 M2;
		const int32_t u[3] = { (int32_t)T[0] - f.P[0], (int32_t)f.P[1] - T[1], (int32_t)f.P[2] - T[2] };
		BuildOrthonormalBasis(u, &M2);
		M2.t[0] = v[0];
		M2.t[1] = v[1];
		M2.t[2] = v[2];
		if (r.kind == PLAY_RAY)
		{
			ComposeAffineTransform(&Camera(), &M2, &arg->m);
			arg->depth = 0;
			return;
		}
		if (T[1] >= 0) FloorHit(f.P, T);
		BeamDraw d;
		d.M2 = M2;
		BeamShape(b, c, f.P, T, &d);
		hb->M2 = d.M2;
		hb->len = d.len;
		hb->width = (int16_t)lerp_i(BeamWidth(e0.width), BeamWidth(e1.width), k, den);
		hb->fade = lerp_i(e0.fade, e1.fade, k, den);
	}

	static void HeldDraw(int num, int den)
	{
		// arena A (magic buffer + 0x10AC) follows the blend buffer (+ 0xF24) at 0x188 bytes
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
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			PrimArg arg = {}; // (the matrix pad +0x12 is never read)
			HeldBeam hb = {};
			int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
			if (r.kind == PLAY_HIT)
			{
				ComposeAffineTransform(&Camera(), &((const HitNode *)r.node)->frame, &arg.m);
				arg.depth = 0;
			}
			else if (r.kind == SPR_SPARK)
			{
				for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
			}
			else if (r.kind != SPR_GLOW) HeldBeamPart(r, k, den, &arg, &hb);
			arg.morph = morph;
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == SPR_GLOW) SpriteDraw(pos, r.counter, SEQ_Glow, 0xAF0, -0x400);
			else if (r.kind == SPR_SPARK) SpriteDraw(pos, r.counter, SEQ_Spark, 0xC00, -0x300);
			else if (r.kind == DRAW_BEAM) BeamRender(&hb.M2, hb.len, hb.width, hb.fade);
			else prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag277_held()
	{
		mark_held(laser277::ORIG_BeamTask);
		mark_held(laser277::ORIG_HitTask);
		mark_held(laser277::ORIG_SparkTask);
		mark_held(laser277::ORIG_GlowTask);
		register_module_held(277, laser277::HeldReady, laser277::HeldDraw);
	}
}
