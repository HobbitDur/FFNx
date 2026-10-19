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

// 30 fps part of mag310_holy_war.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag310_holy_war.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The play task draws THEN advances. A real tick records every play task call (queue order) with the
// counter and shard angles it drew, the GTE registers and the Field_Alloc scratch it started from
// (draw headers keep the scratch bytes where the code does not write) and the module packet cursor
// of the tick's first draw. A held frame replays the calls in the same order, each part as the play
// task draws it:
// - the layouts (prim::play_held: every object's exact in-between record; objects appearing /
//   disappearing switch on the real frame; mesh numbers never blended). The orbit layouts' block
//   positions are the spiral's in-between point: angle and radius half way between this tick's and
//   the next tick's (the play task's formula), through sin / cos; the main layout's block is fixed
//   (anchor and scale taken once at the task's start: nothing follows the target's model).
// - the shards: angles half way along their spin (angle N + spin * num / den), fade half way between
//   this tick's and the next tick's ramp values; scale and anchor are fixed.
// A part the next tick does not draw (the orbits after counter 0x11, the shards after 0x23, a layout
// at its last frame, everything when the task ends) holds this tick's picture and is not drawn at
// fraction 1 (= the next tick's picture). Every part is placed through the frame's camera (the frame
// gate's: the next tick's view at fraction 1), as the play task and the callback read the camera
// matrix when they draw. Blended vertex frames go to a private buffer; the packets go to a private
// copy of the module arena (magc::HeldScope); the GTE registers 28..31 are put back. No task of the
// module has a draw-only mode.

namespace ff8fx
{
namespace holywar310
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick
	static const int MAX_PLAYS = 6;

	struct Rec
	{
		const PlayNode *node;     // the live node (its layouts: prim::play_held knows what they drew)
		int16_t counter;          // the counter it drew with
		int16_t ang[16][3];       // the shard angles it drew with
		int32_t scaled;           // the play block's word +0x14 it drew with
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_PLAYS];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x1000]; // blended vertex frames (magic buffer + 0x16E0)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(const PlayNode *p)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_PLAYS) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.node = p;
		r.counter = p->counter;
		for (int i = 0; i < 16; i++) memcpy(r.ang[i], p->shard[i].ang, sizeof(r.ang[i]));
		r.scaled = PrimScaled();
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the layout plays again on the next tick (it has frames left)
	static bool LayoutGoesOn(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// one layout part: in between when the next tick draws it too, else this tick's picture before 1
	static void HeldLayout(prim::Layout *l, const PrimArg *now, const PrimArg *next, bool part_next, int num, int den)
	{
		if (part_next && LayoutGoesOn(l)) prim::play_held(l, PrimCallback, (int)next, num, den);
		else if (num < den) prim::play_held(l, PrimCallback, (int)now, 0, den);
	}

	static void HeldPlay(const Rec &r, int num, int den)
	{
		const PlayNode *p = r.node;
		const int32_t c = r.counter;
		const bool ends = c + 1 >= MAIN_TICKS; // the task's last tick
		PrimArg now, at;
		now.pos[3] = 0;
		now.scaled = r.scaled;
		now.scale[2] = now.scale[1] = now.scale[0] = p->scale;
		now.morph = (uint32_t)g_held_morph;
		at = now;
		if ((uint32_t)c < (uint32_t)ORBIT_TICKS)
		{
			const bool goes_on = !ends && (uint32_t)(c + 1) < (uint32_t)ORBIT_TICKS;
			for (int j = 0; j < 2; j++)
			{
				OrbitPos(p, c, j, now.pos);
				now.depth = 0;
				at = now;
				if (goes_on)
				{
					// the spiral's in-between point (the play task's angle and radius, half way)
					const uint32_t q0 = (uint32_t)shl32(c, 12) / 18u, q1 = (uint32_t)shl32(c + 1, 12) / 18u;
					const int32_t r0 = mul32((int32_t)(0x1000 - q0), p->scale) / 0x1000;
					const int32_t r1 = mul32((int32_t)(0x1000 - q1), p->scale) / 0x1000;
					const int32_t a0 = ((int32_t)q0 >> 1) + 0x800 * j, a1 = ((int32_t)q1 >> 1) + 0x800 * j;
					const int32_t a = lerp_i(a0, a1, num, den), rad = lerp_i(r0, r1, num, den);
					at.pos[0] = (int16_t)(mul32(ComputeSin(a), rad) / 0x1000 + p->anchor[0]);
					at.pos[1] = p->anchor[1];
					at.pos[2] = (int16_t)(mul32(ComputeCos(a), rad) / 0x1000 + p->anchor[2]);
				}
				HeldLayout((prim::Layout *)p->orbit[j], &now, &at, goes_on, num, den);
			}
		}
		if ((uint32_t)c < (uint32_t)MAIN_TICKS)
		{
			memcpy(now.pos, p->anchor, sizeof(now.pos));
			now.depth = -(p->scale / 8);
			HeldLayout((prim::Layout *)p->main, &now, &now, !ends, num, den);
		}
		const int32_t k = c - DEBRIS_FIRST;
		if ((uint32_t)k < (uint32_t)DEBRIS_TICKS)
		{
			const bool goes_on = !ends && (uint32_t)(k + 1) < (uint32_t)DEBRIS_TICKS;
			if (!goes_on && num == den) return;
			const int f = goes_on ? num : 0;
			ShardDraw *d = (ShardDraw *)FieldAlloc(0x88);
			*(uint32_t *)(d->h + 0) = MODEL_DEBRIS;
			*(uint32_t *)(d->h + 8) = 0;
			*(int32_t *)(d->h + 0xC) = goes_on ? lerp_i(ShardFade(k), ShardFade(k + 1), num, den) : ShardFade(k);
			*(uint32_t *)(d->h + 0x1C) = 0xF0;
			*(int32_t *)(d->h + 0x18) = -(p->scale / 8);
			for (int i = 0; i < 16; i++)
			{
				const Shard &s = p->shard[i];
				int16_t ang[3];
				for (int a = 0; a < 3; a++) ang[a] = (int16_t)(r.ang[i][a] + s.spin[a] * f / den);
				BuildRotationMatrixFromAngles(ang, &d->m);
				d->scale[2] = p->scale;
				d->scale[1] = p->scale;
				d->scale[0] = p->scale;
				Scale3DMatrix(&d->m, d->scale);
				d->m.t[0] = p->anchor[0];
				d->m.t[1] = p->anchor[1];
				d->m.t[2] = p->anchor[2];
				ComposeAffineTransform(&Camera(), &d->m, &d->m);
				GteSetRotMatrix(&d->m);
				GteSetTransVector(&d->m);
				PacketCursor() = RenderPrimModel2(d->h, RenderOT(), 2, PacketCursor());
			}
			FieldFree(0x88);
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed task starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			HeldPlay(r, num, den);
		}
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag310_held()
	{
		mark_held(holywar310::ORIG_PlayTask);
		register_module_held(310, holywar310::HeldReady, holywar310::HeldFrame);
	}
}
