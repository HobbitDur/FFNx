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

// 30 fps part of mag189_wall.cpp: the draw log of the real tick and the held-frame drawing (included at the end of
// mag189_wall.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The wall, shard, dust and debris tasks draw THEN update. A real tick records every call of those tasks in execution order
// (the node as drawn, whether the tick was draw-only, the GTE registers and the Field_Alloc scratch it started from: the draw
// headers keep the scratch bytes the code does not write) and, for the particle tasks, every particle each call drew (as
// drawn and as its update left it). A held frame replays the calls in the same order, each part as its task draws it, at
// the exact in-between state (fraction num / den):
// - the wall: its height (rising 100 a tick, then 0) and the model-space clip-plane height half way; the fade ramp half way
//   while the next tick uses the same draw mode. The clipped -> whole switch (counter 0x13) and the fade switch (0x2F) hold
//   this tick's picture below 1.
// - the shards: the growing y scale and the fade ramp half way (the fade switch at 0x0F holds); a shard still in its delay
//   draws nothing (it appears on the real frame).
// - the dust: positions half way to the next tick's (x / z speed), flipbook frame N below 1, N + 1 at 1.
// - the debris: angles x / z (spin, the short 16-bit way), height and scale half way.
// A part the next tick does not draw (a task's last tick, a particle that dies) holds this tick's picture and is not drawn at
// fraction 1; parts the next tick starts are not drawn (fraction 1 = a subsequence of the next tick). A draw-only tick holds
// (the next tick draws the same state). Every part is placed through the frame's camera (the frame gate's: the next tick's
// view at fraction 1), as the tasks read the camera matrix when they draw (the debris task copies it into its work block).
// The one-shot stage capture of the main task (counter 0) runs on the real tick only: held frames draw nothing for it (the
// captured picture stays in VRAM and the wall model draws it as it is). Not drawn by the module (held by the engine at
// 15 Hz, as the real ticks set it): the camera shake word 0x1D97712 (a per-tick jitter, as effects 36 / 38). The packets
// go to a private copy of the module arena (magc::HeldScope); the GTE registers 28..31 are put back.

namespace ff8fx
{
namespace wall189
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xE000;   // module packet arena per tick
	static const int MAX_RECS = 32;         // wall + 24 shards + dust + debris (+ spare)
	static const int MAX_PARTS = DUST_COUNT + DEBRIS_COUNT;

	struct Rec
	{
		uint32_t fn;
		const void *live;    // the live node
		Node node;           // as drawn
		bool paused;         // draw-only tick
		int first, count;    // particles drawn (dust / debris)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PartRec
	{
		const void *at;      // the live pool record
		uint8_t drawn[0x18], next[0x18];
		bool alive;          // still live after the update
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0, g_cur_part = -1;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first logged draw
	static Rec g_recs[MAX_RECS];
	static PartRec g_parts[MAX_PARTS];
	static uint8_t g_held_packets[ARENA];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_cur_part = -1;
		g_packets = 0;
	}

	static void held_begin(uint32_t fn, const Node *n)
	{
		g_cur_part = -1;
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return;
		if (g_nrec == 0) g_packets = Cursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = n;
		r.node = *n;
		r.paused = (UpdateFlags() & 0x201) != 0;
		r.first = g_npart;
		r.count = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_part(const void *p)
	{
		g_cur_part = -1;
		Rec *r = g_nrec ? &g_recs[g_nrec - 1] : nullptr;
		if (g_log_tick != g_real_tick || !r || (r->fn != ORIG_DustTask && r->fn != ORIG_DebrisTask)) return;
		if (g_npart >= MAX_PARTS) return;
		g_cur_part = g_npart++;
		g_parts[g_cur_part].at = p;
		memcpy(g_parts[g_cur_part].drawn, p, 0x18);
		r->count++;
	}

	static void held_note_part_next(const void *p)
	{
		if (g_cur_part < 0) return;
		PartRec &x = g_parts[g_cur_part];
		memcpy(x.next, p, 0x18);
		x.alive = *(const int32_t *)p != 0;
		g_cur_part = -1;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldWall(const Rec &r, int num, int den)
	{
		const Node &n = r.node;
		const int32_t c = n.counter;
		if (r.paused) { WallDraw(&n, c, WallPlane(c), WallFade(c)); return; }
		if (c + 1 >= WALL_TICKS)
		{
			if (num < den) WallDraw(&n, c, WallPlane(c), WallFade(c));
			return;
		}
		Node m1 = n;
		m1.y = c < WALL_CLIPPED ? (int16_t)(n.y - n.step) : (int16_t)0;
		m1.counter = (int16_t)(c + 1);
		if (num >= den) { WallDraw(&m1, c + 1, WallPlane(c + 1), WallFade(c + 1)); return; }
		if ((c < WALL_CLIPPED) != (c + 1 < WALL_CLIPPED)) { WallDraw(&n, c, WallPlane(c), WallFade(c)); return; }
		Node m = n;
		m.y = lerp16(n.y, m1.y, num, den);
		const int16_t plane = lerp16(WallPlane(c), WallPlane(c + 1), num, den);
		const int32_t fade = (c >= WALL_FADE) == (c + 1 >= WALL_FADE) ? lerp_i(WallFade(c), WallFade(c + 1), num, den) : WallFade(c);
		WallDraw(&m, c, plane, fade);
	}

	static void HeldShard(const Rec &r, int num, int den)
	{
		const Node &n = r.node;
		const int32_t c = n.counter;
		if (r.paused) { ShardDraw(&n, c, ShardFade(c)); return; }
		if (c + 1 >= SHARD_TICKS)
		{
			if (num < den) ShardDraw(&n, c, ShardFade(c));
			return;
		}
		Node m1 = n;
		if (c < SHARD_GROW) m1.scale_y = (int16_t)(n.scale_y + n.step);
		m1.counter = (int16_t)(c + 1);
		if (num >= den) { ShardDraw(&m1, c + 1, ShardFade(c + 1)); return; }
		Node m = n;
		m.scale_y = lerp16(n.scale_y, m1.scale_y, num, den);
		const int32_t fade = (c >= SHARD_FADE) == (c + 1 >= SHARD_FADE) ? lerp_i(ShardFade(c), ShardFade(c + 1), num, den) : ShardFade(c);
		ShardDraw(&m, c, fade);
	}

	static void HeldDust(const Rec &r, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Dust;
		*(int16_t *)(h + 0x24) = 8;
		for (int i = r.first; i < r.first + r.count; i++)
		{
			const PartRec &x = g_parts[i];
			const Dust &d = *(const Dust *)x.drawn, &e = *(const Dust *)x.next;
			if (!r.paused && !x.alive && num >= den) continue;
			const bool moves = x.alive && !r.paused;
			int16_t pos[4];
			memcpy(pos, d.pos, sizeof(pos));
			if (moves)
			{
				pos[0] = lerp16(d.pos[0], e.pos[0], num, den);
				pos[2] = lerp16(d.pos[2], e.pos[2], num, den);
			}
			DustDraw(h, moves && num >= den ? e.frame : d.frame, pos, d.angle);
		}
		FieldFree(0xB4);
	}

	static void HeldDebris(const Rec &r, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *w = (uint8_t *)FieldAlloc(0x88);
		memcpy(w + 0x28, &Camera(), 0x20);
		*(uint32_t *)h = MODEL_Debris;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(int16_t *)(w + 2) = 0;
		for (int i = r.first; i < r.first + r.count; i++)
		{
			const PartRec &x = g_parts[i];
			const Debris &d = *(const Debris *)x.drawn, &e = *(const Debris *)x.next;
			if (!r.paused && !x.alive && num >= den) continue;
			Debris q = d;
			if (x.alive && !r.paused)
			{
				q.ang_x = lerp16(d.ang_x, e.ang_x, num, den);
				q.ang_z = lerp16(d.ang_z, e.ang_z, num, den);
				q.y = lerp16(d.y, e.y, num, den);
				q.scale = lerp16(d.scale, e.scale, num, den);
			}
			DebrisDraw(h, w, &q);
		}
		FieldFree(0x88);
		FieldFree(0x58);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(Cursor(), g_held_packets, sizeof(g_held_packets), g_packets);
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
			switch (r.fn)
			{
			case ORIG_WallTask: HeldWall(r, num, den); break;
			case ORIG_ShardTask: HeldShard(r, num, den); break;
			case ORIG_DustTask: HeldDust(r, num, den); break;
			case ORIG_DebrisTask: HeldDebris(r, num, den); break;
			}
		}
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag189_held()
	{
		mark_held(wall189::ORIG_MainTask);   // the stage capture: real ticks only
		mark_held(wall189::ORIG_WallTask);
		mark_held(wall189::ORIG_ShardTask);
		mark_held(wall189::ORIG_DustTask);
		mark_held(wall189::ORIG_DebrisTask);
		register_module_held(189, wall189::HeldReady, wall189::HeldFrame);
	}
}
