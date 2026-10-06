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

// 30 fps part of mag063_helix_attack.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag063_helix_attack.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The master and the particles draw THEN update. A real tick records every draw in order (layout
// plays, beams, the tile, the particles) with the GTE registers and the Field_Alloc scratch it
// started from (draw headers keep the scratch bytes where the code does not write). A held frame
// replays the log in the same order:
//   layouts   - prim::play_held: every object's exact in-between record; the anchors (the caster's
//               points P1 / P2 through its current pose, their midpoint, the target) hold; the
//               impact layout's v scroll goes half way to the next tick's (+32 a tick);
//   beams     - the arguments of the beam half way to the ones the next tick draws (start point,
//               v scroll, tilt, turn, stretch, fade: the master's own formulas at counter + 1, the
//               start points from the same caster points); a beam the next tick does not draw holds;
//   tile      - the level half way to the next tick's; the last one holds;
//   particles - the position half way along the velocity (the update adds it); the sprite frame
//               holds; a particle on its last frame holds.
// Blended vertex frames go to a private buffer (the original's is magic buffer + 0x21150).
// The screen flash level (one engine word a tick) is not redrawn: it holds.
// No Helix task has a draw-only mode.

namespace ff8fx
{
namespace helix063
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick

	enum RecKind : uint8_t { REC_PLAY, REC_BEAM, REC_TILE, REC_PARTICLE };
	struct BeamArgs
	{
		int16_t from[3];
		int32_t scroll, tilt, turn, len, fade;
	};
	struct Rec
	{
		RecKind kind;
		int8_t index;               // beam: 0 / 1 in the tick
		int16_t counter;            // master counter of the draw
		prim::Layout *layout;       // layout play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		Points pts;                 // beam: the caster points of the tick
		int16_t to[4];              // beam: the target point
		BeamArgs beam;
		int32_t tile;               // tile: step
		ParticleNode particle;      // particle: the node as drawn
		const ParticleNode *node;   // particle: the live node
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static int g_nbeam = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[48];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_morph[0x10000]; // blended vertex frames

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nbeam = 0;
		g_packets = 0;
	}

	static Rec *held_add(RecKind kind)
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

	static void held_note_play(prim::Layout *layout, const PrimArg *arg)
	{
		Rec *r = held_add(REC_PLAY);
		if (!r) return;
		r->layout = layout;
		r->arg = *arg;
		const MasterNode *m = (const MasterNode *)QMaster()->head; // (only the master plays layouts)
		r->counter = m ? m->counter : 0;
	}

	static void held_note_beam(const MasterNode *m, const Points *pts, const int16_t *from, int32_t scroll, int32_t tilt, int32_t turn, int32_t len, int32_t fade)
	{
		Rec *r = held_add(REC_BEAM);
		if (!r) return;
		r->index = (int8_t)(g_nbeam++ & 1);
		r->counter = m->counter;
		r->pts = *pts;
		memcpy(r->to, m->pos, sizeof(r->to));
		memcpy(r->beam.from, from, sizeof(r->beam.from));
		r->beam.scroll = scroll;
		r->beam.tilt = tilt;
		r->beam.turn = turn;
		r->beam.len = len;
		r->beam.fade = fade;
	}

	static void held_note_tile(const MasterNode *m, int32_t i)
	{
		Rec *r = held_add(REC_TILE);
		if (!r) return;
		r->counter = m->counter;
		r->tile = i;
	}

	static void held_note_particle(const ParticleNode *p)
	{
		Rec *r = held_add(REC_PARTICLE);
		if (!r) return;
		r->particle = *p;
		r->node = p;
	}

	// the beams the master draws at counter c (its own formulas), from the caster points
	static int BeamsAt(int32_t c, const Points &pts, BeamArgs out[2])
	{
		const int32_t k = c - 0x10;
		if ((uint32_t)k < 0x14)
		{
			const BeamCurve cv = Curve(k);
			const int32_t scroll = shl32(k, 5);
			out[0] = { { pts.p1[0], pts.p1[1], pts.p1[2] }, scroll, -cv.tilt, cv.turn, cv.len, cv.fade };
			out[1] = { { pts.p2[0], pts.p2[1], pts.p2[2] }, scroll, cv.tilt, -cv.turn, cv.len, cv.fade };
			return 2;
		}
		const int32_t e = c - 0x24;
		if ((uint32_t)e < 8)
		{
			const int32_t fade = (int32_t)((uint32_t)shl32(e, 12) >> 3);
			const int32_t scroll = shl32(e, 5);
			out[0] = { { pts.p1[0], pts.p1[1], pts.p1[2] }, scroll, 0, 0, 0x1000, fade };
			out[1] = { { pts.p2[0], pts.p2[1], pts.p2[2] }, scroll, 0, 0, 0x1000, fade };
			return 2;
		}
		return 0;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				// the impact layout's v scroll (+32 a tick) while the next tick draws it
				if (arg.objs == OBJS_Impact && (uint32_t)(r.counter + 1 - 0x24) < 0x1C)
					arg.scroll = (int16_t)lerp_i(arg.scroll, arg.scroll + 0x20, num, den);
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				break;
			}
			case REC_BEAM:
			{
				BeamArgs b = r.beam;
				BeamArgs next[2];
				if (BeamsAt(r.counter + 1, r.pts, next) > r.index)
				{
					const BeamArgs &n = next[r.index];
					for (int a = 0; a < 3; a++) b.from[a] = (int16_t)lerp_i(b.from[a], n.from[a], num, den);
					b.scroll = lerp_i(b.scroll, n.scroll, num, den);
					b.tilt = lerp_i(b.tilt, n.tilt, num, den);
					b.turn = lerp_i(b.turn, n.turn, num, den);
					b.len = lerp_i(b.len, n.len, num, den);
					b.fade = lerp_i(b.fade, n.fade, num, den);
				}
				Beam(b.from, r.to, b.scroll, b.tilt, b.turn, b.len, b.fade);
				break;
			}
			case REC_TILE:
			{
				int32_t level = TileLevel(r.tile, 0, 1, 3);
				if (r.tile + 1 < 4) level = lerp_i(level, TileLevel(r.tile + 1, 0, 1, 3), num, den);
				PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
				break;
			}
			case REC_PARTICLE:
			{
				ParticleNode p = r.particle;
				if (p.frame + 1 < 0xC)
					for (int a = 0; a < 3; a++) p.pos[a] = (int16_t)lerp_i(p.pos[a], (int16_t)(p.pos[a] + p.vel[a]), num, den);
				ParticleDraw(&p);
				break;
			}
			}
		}
	}
}

	static void register_mag063_held()
	{
		mark_held(helix063::ORIG_MasterTask);
		mark_held(helix063::ORIG_ParticleTask);
		register_module_held(63, helix063::HeldReady, helix063::HeldFrame);
	}
}
