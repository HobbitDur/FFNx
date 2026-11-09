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

// 30 fps part of mag157_breath.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag157_breath.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick logs each draw in order: the node as drawn
// (and where it lives), the GTE registers and Field_Alloc scratch it started from, the particle
// table as drawn (particle tasks) and the cloud position as drawn (cloud tasks). A held frame
// rebuilds the two breath frames with the current camera (as the director does), then replays the
// log in the same order, each part at its exact in-between state between what tick N drew and what
// tick N+1 draws; discrete things (flipbook frames, model flags) are tick N's up to half way and
// tick N+1's after; a part the next tick does not draw (task ended, particle gone) holds up to half
// way and is gone after:
//   Swirl        - angle half way, fade half way within a fade ramp.
//   Burst        - scale half way, fade half way.
//   Puff         - the cloud position half way (see below); frame discrete.
//   CloudModels  - angles, scale and cloud position half way, fade half way.
//   Rings        - angle, scale and cloud position half way, fade half way.
//   particles    - breath particles turn half way on their circle; puff particles and splatter are
//                  fixed; frames discrete; particles born on the tick (after the draws) appear past
//                  half way.
// The cloud position moves under the puff and the cloud models: the position a task sees on the
// next tick is the position after the tick minus what the movers before it in the queue move it by
// on the next tick (their next steps, from their nodes). The module packets alternate between the
// two halves of the texture-off buffer: the held packets start where the tick's first draw started
// (and past half way sit on the other half's bytes, the next tick's). In the draw-only mode nothing
// moves: every part holds.

namespace ff8fx
{
namespace breath157
{
	static const uint32_t SCRATCH = 0x200;
	static const int MAX_RECS = 48;

	struct Rec
	{
		uint32_t kind;
		EffectNode *at;               // the live node
		EffectNode node;              // the node as drawn
		int16_t cloud[4];             // cloud tasks: the cloud position as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
		Part table[PART_COUNT];       // particle tasks: the table as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_held_packets[ARENA_SIZE];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note(uint32_t kind, const EffectNode *t)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.at = (EffectNode *)t;
		r.node = *t;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		if (kind == REC_PUFF || kind == REC_CLOUDMODELS || kind == REC_RINGS) memcpy(r.cloud, CloudPos(t->w0E), 8);
		if (kind == REC_BREATHPARTS || kind == REC_PUFFPARTS || kind == REC_SPLATTER) memcpy(r.table, Parts(), sizeof(r.table));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the node the task left for the next tick, or nullptr when it ended on the tick
	static const EffectNode *NextNode(const Rec &r, bool moving)
	{
		const EffectNode *a = r.at;
		if (!(a->hdr.flags & 1) || a->hdr.func != r.node.hdr.func) return nullptr;
		if (a->counter != (int16_t)(r.node.counter + (moving ? 1 : 0))) return nullptr;
		return a;
	}

	// fades (header +0x0C) half way when both counters are on the same ramp
	static void LerpFade(uint8_t *h, int32_t f0, int32_t f1, bool same, int num, int den)
	{
		if (same) *(int32_t *)(h + 0xC) = lerp_i(f0, f1, num, den);
	}
	static int SwirlRamp(int16_t c) { return c < 4 ? 1 : (c >= 0x16 ? 2 : 0); }
	static int32_t SwirlFade(int16_t c) { return c < 4 ? (4 - (int32_t)c) << 10 : ((int32_t)c - 0x16) << 9; }

	// Every task of a particle type draws and updates every particle of that type (two clouds' puff particle / splatter
	// tasks both do): m = the updates of the type from this record's draw to the end of the tick (its own included),
	// k = the updates of the type the next tick makes before this task draws (the tasks before it that go on)
	static void PartsHeld(const Rec &r, uint32_t bit, uint8_t *h, int num, int den, bool late, int m, int k)
	{
		const Part *pre = r.table, *post = Parts();
		for (int i = 0; i < PART_COUNT; i++)
		{
			const bool pa = (pre[i].type & bit) != 0, pb = (post[i].type & bit) != 0;
			const bool cont = pa && pb && post[i].frame == (int16_t)(pre[i].frame + m);
			Part next = post[i];
			next.frame = (int16_t)(post[i].frame + k);
			// a task before this one drew its last frame (sequence +8 = the frame count): freed before this task draws
			const bool gone = next.frame >= *(const int16_t *)(*(const uint32_t *)h + 8);
			if (bit == 1) next.v[0] = (int16_t)(post[i].v[0] + k * post[i].v[1]);
			Part q;
			if (cont)
			{
				if (late && gone) continue;
				q = late ? next : pre[i];
				if (bit == 1) q.v[0] = lerp16(pre[i].v[0], next.v[0], num, den);
			}
			else if (pa && !late) q = pre[i];
			else if (pb && late && m > 0 && !pa && !gone) q = next; // born on the tick (after the draws of the tasks of its type before it)
			else continue;
			if (bit == 1) BreathPartDraw(h, &q);
			else if (bit == 2) PuffPartDraw(h, &q);
			else SplatterDraw(h, &q);
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const bool late = 2 * num > den;
		// past half way the draws are the next tick's: under them, what the other half of the buffer
		// holds, as the bytes its packets do not write
		if (late && g_packets)
		{
			const uint32_t other = g_packets >= TexBase() + ARENA_SIZE ? g_packets - ARENA_SIZE : g_packets + ARENA_SIZE;
			memcpy(g_held_packets, (const void *)other, sizeof(g_held_packets));
		}
		const bool moving = !DrawOnly();
		// the breath frames with this frame's camera; the frames, the sprite frame and the cloud positions are put back after
		Mat4x3 keep_a = FrameA(), keep_b = FrameB(), keep_s = SpriteFrame();
		int16_t keep_cloud[12];
		memcpy(keep_cloud, CloudPos(0), sizeof(keep_cloud));
		BreathFrame(FrameA(), DirA(), MouthA());
		BreathFrame(FrameB(), DirB(), MouthB());
		int32_t acc[3] = { 0, 0, 0 }; // per cloud: what the movers before in the queue move it by on the next tick
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			// the scratch as the tick's first draw found it (past half way: as the next tick's first draw finds it, the
			// scratch now); after that each replayed draw leaves what the next one finds (only the drawing tasks use the
			// scratch in this module), so the draw headers keep the residue (the fields a header does not set) of the
			// tick they stand for
			if (i == 0 && !late) memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			const EffectNode *nx = NextNode(r, moving);
			const EffectNode &n = r.node;
			if (r.kind == REC_BREATHPARTS || r.kind == REC_PUFFPARTS || r.kind == REC_SPLATTER)
			{
				if (!nx && late) continue; // the task ended on the tick: the next tick does not draw it
				uint8_t *h;
				if (r.kind == REC_SPLATTER) h = SeqHeader(SEQ_Splatter);
				else
				{
					h = (uint8_t *)FieldAlloc(0xB4);
					*(uint32_t *)h = r.kind == REC_BREATHPARTS ? SEQ_Breath : SEQ_PuffPart;
					*(int16_t *)(h + 0x24) = 0;
					LitSpriteSetup(r.kind == REC_BREATHPARTS ? &FrameA() : &FrameB());
				}
				int m = 0, k = 0;
				for (int j = 0; moving && j < g_nrec; j++)
				{
					if (g_recs[j].kind != r.kind) continue;
					if (j >= i) m++;
					else if (NextNode(g_recs[j], moving)) k++;
				}
				PartsHeld(r, r.kind == REC_BREATHPARTS ? 1 : (r.kind == REC_PUFFPARTS ? 2 : 4), h, num, den, late, m, k);
				FieldFree(0xB4);
				continue;
			}
			// cloud tasks: the cloud position this task sees on the next tick
			if (r.kind == REC_PUFF || r.kind == REC_CLOUDMODELS || r.kind == REC_RINGS)
			{
				const int16_t k = n.w0E;
				int16_t *c = CloudPos(k);
				if (k >= 0 && k < 3 && nx)
				{
					const int16_t next_z = (int16_t)(keep_cloud[k * 4 + 2] - acc[k]);
					c[0] = r.cloud[0];
					c[1] = r.cloud[1];
					c[2] = lerp16(r.cloud[2], next_z, num, den);
					if (r.kind != REC_RINGS && moving) acc[k] += nx->w14;
				}
				else memcpy(c, r.cloud, 8);
			}
			if (!nx && late) continue; // the task ended on the tick: the next tick does not draw it
			const int16_t c0 = n.counter, c1 = nx ? nx->counter : c0;
			const int16_t hc = late ? c1 : c0;
			EffectNode q = n;
			switch (r.kind)
			{
			case REC_SWIRL:
			{
				if (nx) q.w18 = lerp16(n.w18, nx->w18, num, den);
				SwirlMatrix(&q);
				uint8_t *h = SwirlHeader(hc);
				LerpFade(h, SwirlFade(c0), SwirlFade(c1), nx && SwirlRamp(c0) != 0 && SwirlRamp(c0) == SwirlRamp(c1), num, den);
				Render(h);
				FieldFree(0x58);
				break;
			}
			case REC_BURST:
			{
				if (nx) q.w1C = lerp16(n.w1C, nx->w1C, num, den);
				BurstMatrix(&q);
				uint8_t *h = BurstHeader(hc);
				LerpFade(h, (8 - (int32_t)c0) << 9, (8 - (int32_t)c1) << 9, nx && c0 < 8 && c1 < 8, num, den);
				Render(h);
				FieldFree(0x58);
				break;
			}
			case REC_PUFF:
				q.counter = hc;
				PuffDraw(&q);
				break;
			case REC_CLOUDMODELS:
			{
				if (nx)
				{
					q.w10 = lerp16(n.w10, nx->w10, num, den);
					q.w18 = lerp16(n.w18, nx->w18, num, den);
					q.w1C = lerp16(n.w1C, nx->w1C, num, den);
				}
				Mat4x3 m;
				CloudModel1Matrix(&q, m);
				uint8_t *h = CloudHeader(hc);
				LerpFade(h, ((int32_t)c0 - 8) << 9, ((int32_t)c1 - 8) << 9, nx && c0 >= 8 && c1 >= 8, num, den);
				Render(h);
				FieldFree(0x58);
				CloudModel2(&q, m, h);
				break;
			}
			case REC_RINGS:
			{
				if (nx)
				{
					q.w18 = lerp16(n.w18, nx->w18, num, den);
					q.w1C = lerp16(n.w1C, nx->w1C, num, den);
				}
				Mat4x3 m;
				Ring1Matrix(&q, m);
				uint8_t *h = RingHeader(hc);
				LerpFade(h, ((int32_t)c0 - 8) * 409, ((int32_t)c1 - 8) * 409, nx && c0 >= 8 && c1 >= 8, num, den);
				Render(h);
				FieldFree(0x58);
				Ring2(&q, m, h);
				break;
			}
			}
		}
		memcpy(CloudPos(0), keep_cloud, sizeof(keep_cloud));
		FrameA() = keep_a;
		FrameB() = keep_b;
		SpriteFrame() = keep_s;
	}
}

	static void register_mag157_held()
	{
		mark_held(breath157::ORIG_Swirl);
		mark_held(breath157::ORIG_Burst);
		mark_held(breath157::ORIG_Puff);
		mark_held(breath157::ORIG_CloudModels);
		mark_held(breath157::ORIG_Rings);
		mark_held(breath157::ORIG_BreathParticles);
		mark_held(breath157::ORIG_PuffParticles);
		mark_held(breath157::ORIG_Splatter);
		register_module_held(157, breath157::HeldReady, breath157::HeldFrame);
	}
}
