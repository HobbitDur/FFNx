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

// 30 fps part of mag070_shockwave_pulsar.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag070_shockwave_pulsar.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in execution order (master, then the sub-task queue): its kind and
// parameters as drawn, the GTE registers and the Field_Alloc scratch it started from. A held frame
// replays the log in the same order with the module's own draw code, on the state half way (num / den)
// to the one the next tick draws; nothing random is drawn, nothing is written back:
//   layouts  - prim::play_held (every object's exact in-between record); the Lift / Rise anchors move
//              half way to the target height the task set (entity +0x1E), the shockwave layout's anchor
//              and depth half way on its 0xD5 -> 0xD6 switch; a layout the next tick does not draw
//              (centre after 0x4F, shockwave after 0x105, Lift's last tick, the sub-tasks when the
//              master ends) holds its real frame exactly.
//   models   - the targets redrawn by 0x693170: angles half way (the master's draw: to the angles the
//              Lift task left; the Rise task's: to the angles its next update computes), pose half way
//              (pose_midpoint); their last draw holds.
//   sparks   - a spark the tick drew and moved (same flipbook, life - 1, still alive): position half way,
//              flipbook frame of the real tick; one that dies holds.
//   shards   - the same with their two scales.
//   tile     - the white tile's level half way to the next step; its last step holds.
// The held frame draws into private packet buffers (module arena, frame arena, blended vertex frames)
// and puts back what the draws write outside them: GTE, Field_Alloc scratch, shadow-rotation scratch
// (HeldScope), the targets' bone matrices, the vertex workspace (0x1D98B3C), RenderGeometry's 0x1D99C18.

namespace ff8fx
{
namespace sp070
{
	using namespace magc;

	static const uint32_t SCRATCH = 0x200;      // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 64;

	struct Rec
	{
		int kind;
		prim::Layout *layout;                   // layouts: the live layout (prim::play_held knows what it drew)
		PrimArg arg;                            // layouts: the parameter block as drawn
		Target *rec;                            // Lift / Rise / models: the target
		int16_t ang[3], vec[3];                 // models: angles as drawn, offset
		int32_t scale, counter, len, level;     // models: scale; the drawing task's counter; Rise: lift length; tile level
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];               // Field_Alloc scratch under the draw's allocations
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static int32_t g_master_counter = -1;       // the master's counter on this tick
	static uint32_t g_packets = 0, g_frame = 0; // module / frame arena cursors at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static Spark g_sparks[0x80];                // the pools as drawn
	static Shard g_shards[0x40];
	static uint8_t g_held_packets[0x10000];
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_morph[0x2000];
	static uint8_t g_ws_save[0x10000];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];
	static uint8_t g_shard_bones[4][0x40 + 0x30 * 256];

	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }
	static void *FieldScratch() { return var<void *>(0x1D999C4); }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_master_counter = -1;
		g_packets = 0;
		g_frame = 0;
	}

	static Rec *held_add(int kind, int32_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = PacketCursor();
			g_frame = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.counter = counter;
		r.layout = nullptr;
		r.rec = nullptr;
		r.len = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, FieldScratch(), SCRATCH);
		return &r;
	}

	static void held_note_play(int kind, const void *node, const PrimArg *arg, int32_t counter)
	{
		Rec *r = held_add(kind, counter);
		if (!r) return;
		if (kind == K_CENTRE || kind == K_WAVE) r->layout = (prim::Layout *)((const MasterNode *)node)->layout;
		else
		{
			r->layout = (prim::Layout *)((const SubNode *)node)->layout;
			if (kind != K_RING) r->rec = ((const SubNode *)node)->t.rec;
		}
		r->arg = *arg;
	}

	static void held_note_tile(int32_t level, int32_t counter)
	{
		Rec *r = held_add(K_TILE, counter);
		if (r) r->level = level;
	}

	static void held_note_model(int kind, const Target *rec, const int16_t *vec, int32_t scale, int32_t counter, int32_t len)
	{
		Rec *r = held_add(kind, counter);
		if (!r) return;
		r->rec = (Target *)rec;
		memcpy(r->ang, rec->ang, sizeof(r->ang));
		memcpy(r->vec, vec, sizeof(r->vec));
		r->scale = scale;
		r->len = len;
	}

	static void held_note_sparks(const MasterNode *m)
	{
		g_master_counter = m->counter;
		if (held_add(K_SPARKS, m->counter)) memcpy(g_sparks, (const void *)(TexBase() + 0x181C), sizeof(g_sparks));
	}

	static void held_note_shards(const MasterNode *m)
	{
		if (held_add(K_SHARDS, m->counter)) memcpy(g_shards, (const void *)(TexBase() + 0x201C), sizeof(g_shards));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the sub-tasks are dropped when the master ends (its counter 0x106)
	static bool SubsEnd() { return g_master_counter >= 0x105; }

	// ------------------------------------------------------------------
	// layouts
	// ------------------------------------------------------------------
	static void HeldPlay(const Rec &r, int num, int den)
	{
		PrimArg a = r.arg;
		bool ended = false;
		switch (r.kind)
		{
		case K_CENTRE: ended = r.counter >= 0x4F; break;
		case K_WAVE:
			ended = r.counter >= 0x105;
			if (r.counter == 0xD5)
			{
				// the next tick draws the layout at the centre, deeper
				a.pos[2] = lerp16(a.pos[2], (int16_t)CentreZ(), num, den);
				a.depth = lerp16(a.depth, (int16_t)0xFC00, num, den);
			}
			break;
		case K_LIFT:
		case K_RISE:
		{
			ended = SubsEnd() || (r.kind == K_LIFT && r.counter >= 0x1D);
			// the anchor height the task set for the next tick
			PrimArg n;
			TargetArg(n, r.rec, r.rec->entity);
			if (!ended) a.pos[1] = lerp16(a.pos[1], n.pos[1], num, den);
			break;
		}
		default: ended = SubsEnd(); break;
		}
		a.morph = (uint32_t)g_held_morph;
		prim::play_held(r.layout, PrimPart, (int)&a, ended ? 0 : num, den);
	}

	// ------------------------------------------------------------------
	// the white tile (master 0xCE..0xD5)
	// ------------------------------------------------------------------
	static int32_t TileLevel(int32_t w)
	{
		if (w < 7)
		{
			const int32_t v = shl32(w + 1, 12);
			int32_t q = (mulhi_s(v, (int32_t)0x92492493) + v) >> 2;
			return q + (int32_t)((uint32_t)q >> 31);
		}
		return 0x1000;
	}

	static void HeldTile(const Rec &r, int num, int den)
	{
		const int32_t w = r.counter - 0xCE;
		const int32_t level = w >= 7 ? r.level : lerp_i(r.level, TileLevel(w + 1), num, den);
		PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
	}

	// ------------------------------------------------------------------
	// the targets' redrawn models (0x693170)
	// ------------------------------------------------------------------
	static uint32_t WorkspaceBytes(uint32_t model)
	{
		if (!model || !*(const uint32_t *)(model + 4)) return 0;
		const uint32_t ob = *(const uint32_t *)(model + 4);
		const int32_t count = *(const int32_t *)ob;
		uint32_t most = 0;
		for (int32_t o = 0; o < count; o++)
		{
			uint32_t d = ob + *(const uint32_t *)(ob + 4 + 4 * (uint32_t)o);
			int32_t groups = *(const int16_t *)d;
			d += 2;
			uint32_t verts = 0;
			for (; groups > 0; groups--)
			{
				const int32_t nv = *(const int16_t *)(d + 2);
				d += 4;
				if (nv > 0)
				{
					verts += (uint32_t)nv;
					d += 6 * (uint32_t)nv;
				}
			}
			if (verts > most) most = verts;
		}
		return most * 8;
	}

	// skeleton of an anim header (BattleAnimHeader: +4 -> pointer -> bone count, matrices)
	static uint8_t *Skeleton(uint32_t anim, uint32_t *bytes)
	{
		if (!anim) return nullptr;
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_bones_save[0])) n = sizeof(g_bones_save[0]);
		*bytes = n;
		return sk;
	}

	static void HeldModel(const Rec &r, int num, int den)
	{
		Target *rec = r.rec;
		uint8_t *e = rec->entity;
		bool ended;
		int16_t next[3] = { rec->ang[0], rec->ang[1], rec->ang[2] };
		if (r.kind == K_MODEL) ended = r.counter >= 0xD5;   // the master redraws the targets up to 0xD5
		else
		{
			// the Rise task turns the target then draws it: the next tick's angles
			const uint32_t i = (uint32_t)(r.counter - 0xD);
			ended = SubsEnd() || i + 1 >= (uint32_t)r.len;
			if (!ended)
			{
				const int32_t t = (int32_t)(((i + 1) << 12) / (uint32_t)r.len);
				const int16_t d = (int16_t)sdiv_pow2(t, 3);
				const int32_t sgn = (SlotOf(e) & 1) ? 1 : -1;
				next[0] = (int16_t)(r.ang[0] + sgn * d);
				next[1] = (int16_t)(r.ang[1] + sgn * d);
				next[2] = r.ang[2];
			}
		}
		if (ended) num = 0;
		int16_t ang[3];
		for (int k = 0; k < 3; k++) ang[k] = lerp16(r.ang[k], next[k], num, den);
		uint32_t b0 = 0, b1 = 0;
		uint8_t *sk0 = Skeleton((uint32_t)(e + 0x60), &b0);
		uint8_t *sk1 = Skeleton(*(const uint32_t *)(e + 0x78), &b1);
		if (sk0) memcpy(g_bones_save[0], sk0, b0);
		if (sk1) memcpy(g_bones_save[1], sk1, b1);
		if (num) pose_midpoint(e + 0x60, e + 0x6C, num, den);
		DrawTargetModel(e, ang, r.vec, r.scale);
		if (sk0) memcpy(sk0, g_bones_save[0], b0);
		if (sk1) memcpy(sk1, g_bones_save[1], b1);
	}

	// ------------------------------------------------------------------
	// pools: an entry the tick drew and updated moves half way
	// ------------------------------------------------------------------
	static void HeldSparks(int num, int den)
	{
		Spark tmp[0x80];
		memcpy(tmp, g_sparks, sizeof(tmp));
		const Spark *cur = (const Spark *)(TexBase() + 0x181C);
		for (int k = 0; k < 0x80; k++)
		{
			Spark &s = tmp[k];
			const Spark &c = cur[k];
			if (s.life == 0 || c.life == 0 || c.life != (uint8_t)(s.life - 1) || c.seq != s.seq) continue;
			if (c.vel[0] != s.vel[0] || c.vel[1] != s.vel[1] || c.vel[2] != s.vel[2]) continue;
			for (int a = 0; a < 3; a++) s.pos[a] = lerp16(s.pos[a], c.pos[a], num, den);
		}
		SparksDraw(tmp);
	}

	static void HeldShards(int num, int den)
	{
		Shard tmp[0x40];
		memcpy(tmp, g_shards, sizeof(tmp));
		const Shard *cur = (const Shard *)(TexBase() + 0x201C);
		for (int k = 0; k < 0x40; k++)
		{
			Shard &s = tmp[k];
			const Shard &c = cur[k];
			if (s.life == 0 || c.life == 0 || c.life != (int16_t)(s.life - 1) || c.rec != s.rec || c.bone != s.bone) continue;
			s.sy = lerp16(s.sy, c.sy, num, den);
			s.sxz = lerp16(s.sxz, c.sxz, num, den);
		}
		// the shards sit on their targets' bones: the targets' pose half way (put back afterwards)
		uint8_t *ents[4];
		uint8_t *sks[4];
		uint32_t bytes[4];
		int ne = 0;
		for (int k = 0; k < 0x40 && num; k++)
		{
			if (tmp[k].life == 0) continue;
			uint8_t *e = tmp[k].rec->entity;
			bool seen = false;
			for (int q = 0; q < ne; q++) seen |= ents[q] == e;
			if (seen || ne >= 4) continue;
			uint32_t b = 0;
			uint8_t *sk = Skeleton((uint32_t)(e + 0x60), &b);
			if (!sk || b > sizeof(g_shard_bones[0])) continue;
			memcpy(g_shard_bones[ne], sk, b);
			ents[ne] = e;
			sks[ne] = sk;
			bytes[ne] = b;
			ne++;
			pose_midpoint(e + 0x60, e + 0x6C, num, den);
		}
		ShardsDraw(tmp);
		for (int q = 0; q < ne; q++) memcpy(sks[q], g_shard_bones[q], bytes[q]);
	}

	// ------------------------------------------------------------------
	static void HeldFrame(int num, int den)
	{
		if (g_nrec == 0) return;
		// a held frame larger than the private buffers is not drawn (never seen)
		if (g_packets && PacketCursor() - g_packets > sizeof(g_held_packets)) return;
		if (g_frame && FrameArena() - g_frame > sizeof(g_held_frame)) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const uint32_t arena = FrameArena();
		// the private frame arena starts as the real one from where the tick's draws started, so the
		// bytes a packet does not write (pad halves) are the real ones
		const uint32_t base = g_frame ? g_frame : arena;
		const uint32_t limit = var<uint32_t>(0x1D969A8);
		uint32_t fill = limit > base ? limit - base : 0;
		if (fill > sizeof(g_held_frame)) fill = sizeof(g_held_frame);
		memcpy(g_held_frame, (const void *)base, fill);
		FrameArena() = (uint32_t)g_held_frame;
		// what the model draws write outside the buffers
		const uint32_t last_packet = var<uint32_t>(0x1D99C18);
		const uint32_t ws = var<uint32_t>(0x1D98B3C);
		uint32_t ws_bytes = 0;
		if (ws)
		{
			for (int i = 0; i < g_nrec; i++)
			{
				if (g_recs[i].kind != K_MODEL && g_recs[i].kind != K_RMODEL) continue;
				const uint8_t *e = g_recs[i].rec->entity;
				uint32_t b = WorkspaceBytes(*(const uint32_t *)(e + 0x64));
				const uint32_t w2 = *(const uint32_t *)(e + 0x78);
				if (w2)
				{
					const uint32_t b2 = WorkspaceBytes(*(const uint32_t *)(w2 + 4));
					if (b2 > b) b = b2;
				}
				if (b > ws_bytes) ws_bytes = b;
			}
			if (ws_bytes > sizeof(g_ws_save)) ws_bytes = sizeof(g_ws_save);
			if (ws_bytes) memcpy(g_ws_save, (const void *)ws, ws_bytes);
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(FieldScratch(), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case K_TILE: HeldTile(r, num, den); break;
			case K_MODEL: case K_RMODEL: HeldModel(r, num, den); break;
			case K_SPARKS: HeldSparks(num, den); break;
			case K_SHARDS: HeldShards(num, den); break;
			default: HeldPlay(r, num, den); break;
			}
		}
		if (ws_bytes) memcpy((void *)ws, g_ws_save, ws_bytes);
		var<uint32_t>(0x1D99C18) = last_packet;
		FrameArena() = arena;
	}
}

	static void register_mag070_held()
	{
		using namespace sp070;
		mark_held(ORIG_MasterTask);
		mark_held(ORIG_LiftTask);
		mark_held(ORIG_RiseTask);
		mark_held(ORIG_RingTask);
		register_module_held(70, HeldReady, HeldFrame);
	}
}
