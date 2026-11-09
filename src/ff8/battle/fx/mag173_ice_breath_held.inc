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

// 30 fps part of mag173_ice_breath.cpp (Ice Breath, effect 173): the draw log of the real tick and the
// held-frame drawing (included at the end of mag173_ice_breath.cpp; shared pieces in mag_common_held.h /
// fx_held.h). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws the state num/den of
// the way from the last real tick to the next one.
//
// Every drawing task draws THEN updates. A real tick records every draw in queue order with the GTE
// registers and the Field_Alloc scratch it started from (headers keep the scratch bytes where the code
// does not write). A held frame replays the log in the same order:
//   Anchor (the caster's mouth, which the director takes again every tick from the animated caster) -
//     the caster's bone 0xF0 at its in-between pose (pose_midpoint + 0x502170 + the (0, 0, -0x96)
//     offset through its rotation; the real bones, the word 0x1D9770C and the scratch put back).
//   Table sprites (mouth swirl, breath puffs, ice shards) - the frame's camera; the entries as drawn,
//     each half way to its live entry when the tick moved it (frame + 1): swirl angle and radius (the
//     radius vector turned about its axis, at the in-between anchor), puff size and position, shard
//     position; the flipbook frame (and the puff colour of the frame) holds below 1. An entry the tick
//     freed (sequence ended, task end) holds and is not drawn at 1; a draw-only tick holds.
//   Mouth glow / breath cones (prim models) - size and depth cue half way to the next tick's (a cue on
//     one side only: from / to level 0); the glow at the in-between anchor; the last draw holds (not
//     drawn at 1).
//   Ice trail - the header as the task builds it (the frame's camera), every polygon the tick drew at
//     the in-between of its draw on this tick and on the next one (the next tick's branch emulated on
//     the live polygon state; after a draw-only tick the counter stays and the next tick only turns
//     its flying polygons before drawing them): offset, x / z angles and depth cue half way; a polygon
//     that starts flying on the next tick goes from its whole draw (offset 0, angles 0, cue 0) to its
//     first flying draw; one the next tick no longer draws holds (not drawn at 1); polygons the next
//     tick adds (growth) appear on the real frame (at 1: the next tick's draw, the added ones included).
//   Crystal players - prim::play_held (every object's exact in-between record); blended vertex frames
//     go to a private buffer; a layout that ends on the tick holds (not drawn at 1).
// Spawns (random) stay at 15 Hz; parts spawned on the next tick are not drawn before they exist
// (fraction 1 = a subsequence of the next tick). The module packet arena alternates per tick: the held
// packets start where the tick's first draw started. The screen flash is engine state (15 Hz).

namespace ff8fx
{
namespace ib173
{
	static const uint32_t SCRATCH = 0x400;
	static const int MAX_RECS = 0x80, MAX_PARTS = 320;
	enum : int { REC_POOL = 1, REC_NODE = 2, REC_TRAIL = 3, REC_CRYSTAL = 4 };

	struct PartSnap { uint32_t addr; uint8_t e[0x18]; };
	struct Rec
	{
		int kind;
		uint32_t fn;                // original task function
		uint32_t node_addr;         // the live node
		uint8_t node[0x24];         // the node as drawn
		int first, n;               // table sprites: the entries as drawn (g_parts)
		bool draw_only;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];   // Field_Alloc scratch at the draw's first allocation
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nparts = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static PartSnap g_parts[MAX_PARTS];
	static uint8_t g_trail[5][0x1000];      // polygon states of each trail mesh as drawn (0xF60 at most)
	static uint8_t g_held_packets[0xC000];  // module arena (0xC000 per tick)
	static uint8_t g_held_morph[0x4000];    // crystal vertex frames blended on held frames

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nparts = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, uint32_t fn, uint32_t n, uint32_t scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.fn = fn;
		r.node_addr = n;
		memcpy(r.node, (const void *)n, sizeof(r.node));
		r.first = g_nparts;
		r.n = 0;
		r.draw_only = DrawOnly();
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)scratch, SCRATCH);
		return &r;
	}

	static uint8_t KindBit(uint32_t fn) { return fn == ORIG_Swirl ? 1 : fn == ORIG_Puffs ? 2 : 4; }

	// a table-sprite task about to draw its entries (h = its first allocation)
	static void held_note_pool(uint32_t fn, uint32_t n, uint32_t h)
	{
		Rec *r = NewRec(REC_POOL, fn, n, h);
		if (!r) return;
		const uint8_t bit = KindBit(fn);
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & bit)) continue;
			if (g_nparts >= MAX_PARTS)
			{
				g_nrec--;
				g_nparts = r->first;
				return;
			}
			PartSnap &p = g_parts[g_nparts++];
			p.addr = e;
			memcpy(p.e, (const void *)e, 0x18);
			r->n++;
		}
	}

	// a cone / glow about to draw (scratch from the Field_Alloc pointer)
	static void held_note_node(uint32_t fn, uint32_t n)
	{
		NewRec(REC_NODE, fn, n, var<uint32_t>(0x1D999C4));
	}

	// a trail mesh about to draw (h = its header, the first allocation): its polygon states as drawn
	static void held_note_trail(uint32_t n, uint32_t h)
	{
		const int idx = S16(n, 0xE);
		if (idx < 0 || idx > 4) return;
		if (!NewRec(REC_TRAIL, ORIG_Trail, n, h)) return;
		const uint32_t mesh = U32(TRAIL_MESHES + (uint32_t)idx * 4, 0);
		const int32_t count = S32(U32(mesh, 0) + mesh + 0x18, 0);
		const uint32_t size = (uint32_t)(count > 0 ? count : 0) * 0x20;
		memcpy(g_trail[idx], (const void *)U32(TRAIL_STATES + (uint32_t)idx * 4, 0), size < sizeof(g_trail[idx]) ? size : sizeof(g_trail[idx]));
	}

	// a crystal player about to play its layout
	static void held_note_crystal(uint32_t n)
	{
		NewRec(REC_CRYSTAL, ORIG_Crystal, n, var<uint32_t>(0x1D999C4));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// in-between helpers
	// ------------------------------------------------------------------
	// the anchor at the caster's pose num/den of the way to the next tick (bones, 0x1D9770C put back)
	static void AnchorAt(int num, int den, uint32_t out)
	{
		const uint32_t e = Ent(CasterSlot());
		const uint32_t w170c = var<uint32_t>(0x1D9770C);
		const bool bones = (U8(e, 0) & 2) != 0;
		if (bones) pose_midpoint((void *)(e + 0x60), (void *)(e + 0x6C), num, den);
		U32(out, 0) = U32(ANCHOR, 0);
		U32(out, 4) = U32(ANCHOR, 4);
		ComputeAnchor(out);
		if (bones) BuildBoneMatricesFromPose((void *)(e + 0x60));
		var<uint32_t>(0x1D9770C) = w170c;
	}

	static Cue LerpCue(Cue a, Cue b, int num, int den)
	{
		if (num <= 0) return a;
		if (num >= den) return b;
		const bool ca = a.flags == 0xF3, cb = b.flags == 0xF3;
		if (!ca && !cb) return a;
		return { 0xF3u, lerp_i(ca ? a.fade : 0, cb ? b.fade : 0, num, den) };
	}

	// ------------------------------------------------------------------
	// table sprites
	// ------------------------------------------------------------------
	static void PoolHeld(const Rec &r, int num, int den, uint32_t anchor)
	{
		const uint8_t bit = KindBit(r.fn);
		uint32_t h = 0, w = 0;
		uint32_t wsize = 0;
		if (r.fn == ORIG_Swirl)
		{
			h = (uint32_t)FieldAlloc(0xB4);
			wsize = 0x40;
			w = (uint32_t)FieldAlloc(wsize);
			U32(w, 8) = U32(anchor, 0);
			U32(h, 0) = SEQ_Swirl;
			S16(h, 0x24) = 0;
			U32(w, 0xC) = U32(anchor, 4);
		}
		else if (r.fn == ORIG_Puffs)
		{
			h = (uint32_t)FieldAlloc(0xB4);
			U32(h, 0) = SEQ_Puff;
			S16(h, 0x24) = 4;
		}
		else
		{
			h = (uint32_t)FieldAlloc(0xB4);
			wsize = 0x88;
			w = (uint32_t)FieldAlloc(wsize);
			U32(h, 0) = SEQ_Shard;
			S16(h, 0x24) = 8;
			S32(w, 0x58) = -0x1000;
			S32(w, 0x5C) = 0;
			S32(w, 0x60) = 0;
			S16(w, 0) = 0;
			S16(w, 2) = 0;
			S16(w, 4) = 0;
			BuildRotationMatrixFromAngles((const int16_t *)w, (Mat4x3 *)(w + 8));
		}
		uint8_t buf[0x18];
		const uint32_t e = (uint32_t)buf;
		for (int i = 0; i < r.n; i++)
		{
			const PartSnap &p = g_parts[r.first + i];
			memcpy(buf, p.e, sizeof(buf));
			const uint32_t l = p.addr;
			const bool moved = !r.draw_only && (U8(l, 0) & bit) && S16(l, 4) == (int16_t)(S16(e, 4) + 1);
			if (!moved && !r.draw_only && num >= den) continue; // freed on the tick: the next tick does not draw it
			if (moved && num >= den) memcpy(buf, (const void *)l, sizeof(buf)); // the next tick's draw
			else if (moved)
			{
				if (r.fn == ORIG_Swirl)
				{
					S16(e, 0xE) = lerp16(S16(e, 0xE), S16(l, 0xE), num, den);
					S16(e, 0x10) = lerp16(S16(e, 0x10), S16(l, 0x10), num, den);
				}
				else if (r.fn == ORIG_Puffs)
				{
					S16(e, 6) = lerp16(S16(e, 6), S16(l, 6), num, den);
					S16(e, 8) = lerp16(S16(e, 8), S16(l, 8), num, den);
					S16(e, 0xA) = lerp16(S16(e, 0xA), S16(l, 0xA), num, den);
					S16(e, 0xC) = lerp16(S16(e, 0xC), S16(l, 0xC), num, den);
				}
				else
				{
					S16(e, 8) = lerp16(S16(e, 8), S16(l, 8), num, den);
					S16(e, 0xC) = lerp16(S16(e, 0xC), S16(l, 0xC), num, den);
				}
			}
			if (r.fn == ORIG_Swirl) SwirlDraw(h, w, e);
			else if (r.fn == ORIG_Puffs) PuffDraw(h, e);
			else ShardDraw(h, w, e);
		}
		if (wsize) FieldFree(wsize);
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// cones / glow
	// ------------------------------------------------------------------
	static void NodeHeld(const Rec &r, int num, int den, uint32_t anchor)
	{
		uint8_t copy[0x24], next[0x24];
		memcpy(copy, r.node, sizeof(copy));
		memcpy(next, r.node, sizeof(next));
		const uint32_t n = (uint32_t)copy, x = (uint32_t)next;
		const bool cone = r.fn == ORIG_Cone;
		if (!r.draw_only)
		{
			if (cone) ConeMove(x);
			else GlowMove(x);
			const bool ends = S16(x, 0xC) >= (cone ? 0x10 : 0x24);
			if (ends && num >= den) return; // its last draw: the next tick does not draw it
			if (!ends)
			{
				S16(n, 0x1C) = lerp16(S16(n, 0x1C), S16(x, 0x1C), num, den);
				const Cue a = cone ? ConeCue(S16(n, 0xC)) : GlowCue(S16(n, 0xC));
				const Cue b = cone ? ConeCue(S16(x, 0xC)) : GlowCue(S16(x, 0xC));
				const Cue cue = LerpCue(a, b, num, den);
				if (cone) ConeDraw(n, cue);
				else GlowDraw(n, anchor, cue);
				return;
			}
		}
		if (cone) ConeDraw(n, ConeCue(S16(n, 0xC)));
		else GlowDraw(n, anchor, GlowCue(S16(n, 0xC)));
	}

	// ------------------------------------------------------------------
	// ice trail
	// ------------------------------------------------------------------
	// one polygon's draw: visible, flying (else whole), offset, x / z angles, depth cue
	struct PolyDraw { bool vis, fly; int16_t off[3], ax, az; int32_t cue; };

	// what the trail draws for polygon state st at trail counter c (st = the state the draw starts
	// from); moves = the draw moves it (no draw-only tick): its angles are drawn after the move
	static PolyDraw PolyAt(uint32_t st, int32_t c, bool moves)
	{
		PolyDraw d = {};
		if (c >= 0xA)
		{
			const int32_t t = c - 0xA;
			if (S16(st, 0xE) < 0) return d;
			d.vis = true;
			if (S16(st, 6) < t)
			{
				d.fly = true;
				d.off[0] = S16(st, 8);
				d.off[1] = S16(st, 0xA);
				d.off[2] = S16(st, 0xC);
				const int16_t ph = S16(st, 0xE);
				d.cue = ph != 0 ? shl32(ph, 8) : -0x1000;
				d.ax = moves ? (int16_t)(S16(st, 0x18) + S16(st, 0x1A)) : S16(st, 0x18);
				d.az = moves ? (int16_t)(S16(st, 0x1C) + S16(st, 0x1E)) : S16(st, 0x1C);
			}
			return d;
		}
		d.vis = S16(st, 6) < c || c >= 0xC;
		return d;
	}

	static uint32_t TrailHeldPolys(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor, const Rec &r, int num, int den)
	{
		const int idx = *(const int16_t *)(r.node + 0xE);
		const int32_t c = *(const int16_t *)(r.node + 0xC);
		const uint32_t verts = U32(h, 4);
		uint32_t poly = U32(h, 0x2C);
		const int32_t count = S32(poly, 0);
		poly += 4;
		const uint32_t pre = (uint32_t)g_trail[idx], live = U32(TRAIL_STATES + (uint32_t)idx * 4, 0);
		for (int32_t k = 0; k < count && (uint32_t)(k + 1) * 0x20 <= sizeof(g_trail[idx]); k++, poly += 0x1C)
		{
			// the next tick's draw: counter + 1 (a draw-only tick does not count: the counter stays, nothing moved, and the
			// next tick turns its flying polygons before drawing them)
			const int32_t cn = r.draw_only ? c : c + 1;
			const PolyDraw a = PolyAt(pre + (uint32_t)k * 0x20, c, !r.draw_only);
			PolyDraw d = a;
			if (num >= den)
			{
				// fraction 1: the next tick's draw (the polygons it adds included: the mesh's visible set grows in 15 Hz steps)
				d = PolyAt(live + (uint32_t)k * 0x20, cn, true);
				if (!d.vis) continue;
			}
			else if (!a.vis) continue; // not drawn on the tick (polygons the next tick adds appear on the real frame)
			else
			{
				const PolyDraw b = PolyAt(live + (uint32_t)k * 0x20, cn, true);
				if (num > 0 && b.vis && b.fly)
				{
					d.fly = true;
					for (int i = 0; i < 3; i++) d.off[i] = lerp16(a.off[i], b.off[i], num, den);
					d.ax = lerp16(a.ax, b.ax, num, den);
					d.az = lerp16(a.az, b.az, num, den);
					d.cue = lerp_i(a.cue, b.cue, num, den);
				}
			}
			const uint32_t st = (uint32_t)g_trail[idx] + (uint32_t)k * 0x20;
			if (d.fly)
			{
				U32(h, 0x64) = U32(st, 0);
				U32(h, 0x68) = U32(st, 4);
				TrailVerts(h, poly, verts);
				for (int v = 0x40; v <= 0x50; v += 8)
				{
					S16(h, v) = (int16_t)(S16(h, v) - S16(h, 0x64));
					S16(h, v + 2) = (int16_t)(S16(h, v + 2) - S16(h, 0x66));
					S16(h, v + 4) = (int16_t)(S16(h, v + 4) - S16(h, 0x68));
				}
				S32(h, 0xA0) = d.off[0] + S16(h, 0x64);
				S32(h, 0xA4) = d.off[1] + S16(h, 0x66);
				S32(h, 0xA8) = d.off[2] + S16(h, 0x68);
				S32(h, 0xC) = d.cue;
				S16(h, 0x66) = 0;
				S16(h, 0x68) = d.az;
				S16(h, 0x64) = d.ax;
				BuildRotationMatrixFromAngles((const int16_t *)(h + 0x64), (Mat4x3 *)(h + 0x8C));
				ComposeAffineTransform((const Mat4x3 *)(h + 0x6C), (const Mat4x3 *)(h + 0x8C), (Mat4x3 *)(h + 0x8C));
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x8C));
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x8C));
			}
			else
			{
				TrailVerts(h, poly, verts);
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x6C));
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x6C));
				S32(h, 0xC) = 0;
			}
			cursor = TrailPoly(h, poly, cursor, ot, mode);
		}
		return cursor;
	}

	static void TrailHeld(const Rec &r, int num, int den)
	{
		uint8_t copy[0x24];
		memcpy(copy, r.node, sizeof(copy));
		const uint32_t n = (uint32_t)copy;
		int16_t angles[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		const int32_t s = S16(n, 0x1C);
		const int32_t sv[3] = { s, s, s };
		m.t[2] = S16(n, 0x14);
		Scale3DMatrix(&m, sv);
		const uint32_t h = (uint32_t)FieldAlloc(0xAC);
		S32(h, 0x24) = S16(n, 0xE);
		U32(h, 0x1C) = 0x30;
		U32(h, 0) = U32(TRAIL_MESHES + (uint32_t)S16(n, 0xE) * 4, 0);
		U32(h, 8) = 0;
		ComposeAffineTransform(&Camera(), &m, (Mat4x3 *)(h + 0x6C));
		const int16_t c = S16(n, 0xC);
		S32(h, 0x20) = c >= 0xA ? c - 0xA : c;
		const uint32_t mesh = U32(h, 0);
		U32(h, 4) = mesh + 8;
		U32(h, 0x2C) = U32(mesh, 0) + mesh;
		GteSetFarColour(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x2C) = U32(h, 0x2C) + 0x18;
		U32(h, 0x28) = (uint32_t)g_trail[S16(n, 0xE)];
		PacketCursor() = TrailHeldPolys(h, OT44(), 2, PacketCursor(), r, num, den);
		FieldFree(0xAC);
	}

	// ------------------------------------------------------------------
	// crystal players
	// ------------------------------------------------------------------
	static void CrystalHeld(const Rec &r, int num, int den)
	{
		prim::Layout *l = (prim::Layout *)(r.node_addr + 0x1C);
		const uint8_t *data = l->data;
		const uint8_t *hdr = data + *(const int32_t *)(data + 4);
		const int32_t total = *(const int32_t *)(hdr + 4);
		if (l->frame >= total && num >= den) return; // the layout ended on the tick
		uint8_t blk[0x28];
		CrystalBlock((uint32_t)r.node, (uint32_t)blk);
		U32((uint32_t)blk, 0xC) = (uint32_t)g_held_morph;
		prim::play_held(l, CrystalCallback, (int)(uint32_t)blk, num, den);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		uint8_t anchor[8];
		AnchorAt(num, den, (uint32_t)anchor);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_POOL: PoolHeld(r, num, den, (uint32_t)anchor); break;
			case REC_NODE: NodeHeld(r, num, den, (uint32_t)anchor); break;
			case REC_TRAIL: TrailHeld(r, num, den); break;
			case REC_CRYSTAL: CrystalHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag173_held()
	{
		mark_held(ib173::ORIG_Swirl);
		mark_held(ib173::ORIG_Cone);
		mark_held(ib173::ORIG_Glow);
		mark_held(ib173::ORIG_Puffs);
		mark_held(ib173::ORIG_Trail);
		mark_held(ib173::ORIG_Shards);
		mark_held(ib173::ORIG_Crystal);
		register_module_held(173, ib173::HeldReady, ib173::HeldFrame);
	}
}
