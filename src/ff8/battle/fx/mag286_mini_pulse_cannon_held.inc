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

// 30 fps part of mag286_mini_pulse_cannon.cpp (Mini Pulse Cannon, effect 286). Exact vanilla shapes: the logic stays at 15 Hz,
// a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, a spark moves after its draw). A real tick records its draws in
// order with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them in the same order on
// private packets, with the current camera (the frame gate's), each layout through prim::play_held (every object's exact
// in-between record):
//   Muzzle  - the muzzle point P (the caster's bone 2) at the caster's pose midpoint (pose_midpoint, the world bone matrices
//             0x5095B0 as the master builds them, the bones put back from the pose 0x508C90), RotY(caster facing) at P.
//   Beam    - from P (pose midpoint) to the target's position; its y fade runs on the fractional counter c + num/den (the
//             game's (0x18 - c) / 10 is linear in c), the basis and length of that in-between vector, the z scale rule of
//             counter N (its steps at counters 40 / 52 stay at 15 Hz); texture scroll of counter N.
//   Impact  - the flat basis of P (pose midpoint) -> the target's position; the block words the real play read (depth,
//             scroll) held. Ground - the same at the target's height.
//   Sparks  - their position half way along their velocity; flipbook frame N held.
// A part the next tick no longer draws (the main counters 66 / 68, an ended spark) holds its picture (state N: records at N,
// pose N). Spawns (CRT rand) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates per tick: the
// held packets start where the tick's first draw started. The GTE, the Field_Alloc scratch, the word 0x1D9770C, the engine
// words the sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace minipulse286
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;       // module packet arena per tick

	struct Rec
	{
		int kind;                 // DrawKind
		const void *node;
		int16_t counter;          // main: the counter as drawn; sparks: the frame
		int16_t pos[3];           // sparks: the position as drawn
		int16_t vel[3];           // sparks: the velocity they move by after the draw
		int16_t life;             // sparks
		uint32_t seq;             // sparks
		int32_t shadow;           // sparks
		int16_t depth, scroll;    // main: the block words the real play read
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[4 + 0x20];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0xB54)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, const void *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = 0;
		r.seq = 0;
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
		r->depth = arg->depth;
		r->scroll = arg->scroll;
	}

	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(SPR_SPARK, s);
		if (!r) return;
		r->counter = s->counter;
		r->life = s->life;
		r->seq = s->seq;
		r->shadow = s->shadow;
		memcpy(r->pos, s->pos, sizeof(r->pos));
		memcpy(r->vel, s->vel, sizeof(r->vel));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		if (r.kind == PLAY_MUZZLE || r.kind == PLAY_BEAM) return (uint32_t)(r.counter + 1 - 0x19) < 0x2A;
		if (r.kind == PLAY_IMPACT || r.kind == PLAY_GROUND) return (uint32_t)(r.counter + 1 - 0x31) < 0x14;
		return r.counter + 1 < r.life;
	}

	// the muzzle point at the caster's pose k/den (the bones put back)
	static void HeldMuzzlePoint(int k, int den, int32_t P[3])
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		ComputeBonesWorldMatrices(c + 0x60, c + 0x40);
		BonePoint(MUZZLE, P);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// the beam vector at the fractional counter c + k/den (BeamVector at k = 0)
	static void HeldBeamVector(const MainNode *m, int32_t c, int k, int den, const int32_t P[3], int32_t v[3])
	{
		BeamVector(m, c, P, v);
		if ((uint32_t)c >= 0xE && (uint32_t)c < 0x18 && k)
		{
			const int32_t dy = P[1] - (int32_t)*(const int16_t *)(m->ent + 0x24);
			v[1] = (int32_t)((int64_t)dy * ((int64_t)(0x18 - c) * den - k) / ((int64_t)10 * den));
		}
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
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			PrimArg arg = {};
			Mat4x3 M = {};
			int32_t P[3] = {};
			const MainNode *m = (const MainNode *)r.node;
			const int32_t c = r.counter - 0x19;
			if (r.kind != SPR_SPARK) HeldMuzzlePoint(k, den, P);
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			arg.morph = (uint32_t)g_held_morph;
			arg.depth = r.depth;
			arg.scroll = r.scroll;
			switch (r.kind)
			{
			case PLAY_MUZZLE:
			case PLAY_BEAM:
				M.t[0] = P[0];
				M.t[1] = P[1];
				M.t[2] = P[2];
				if (r.kind == PLAY_MUZZLE) RotY(*(const int16_t *)(Caster() + 0xE), &M);
				else
				{
					int32_t v[3];
					HeldBeamVector(m, c, k, den, P, v);
					const int32_t len = BuildOrthonormalBasis(v, &M);
					const int32_t sv[3] = { 0x1000, 0x1000, BeamScale(c, len) };
					Scale3DMatrix(&M, sv);
				}
				ComposeAffineTransform(&Camera(), &M, &arg.m);
				prim::play_held((prim::Layout *)(r.kind == PLAY_MUZZLE ? m->muzzle : m->beam), PrimCallback, (int)&arg, k, den);
				break;
			case PLAY_IMPACT:
			case PLAY_GROUND:
				ImpactFrame(m, P, &M);
				if (r.kind == PLAY_GROUND) M.t[1] = *(const int16_t *)(m->ent + 0x24);
				ComposeAffineTransform(&Camera(), &M, &arg.m);
				prim::play_held((prim::Layout *)(r.kind == PLAY_IMPACT ? m->impact : m->ground), PrimCallback, (int)&arg, k, den);
				break;
			default:
			{
				SparkNode s = {};
				s.seq = r.seq;
				s.shadow = r.shadow;
				s.counter = r.counter;
				for (int j = 0; j < 3; j++) s.pos[j] = lerp16(r.pos[j], (int16_t)(r.pos[j] + r.vel[j]), k, den);
				SparkDraw(&s);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag286_held()
	{
		mark_held(minipulse286::ORIG_MainTask);
		mark_held(minipulse286::ORIG_SparkTask);
		register_module_held(286, minipulse286::HeldReady, minipulse286::HeldDraw);
	}
}
