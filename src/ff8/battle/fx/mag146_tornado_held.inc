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

// 30 fps part of mag146_tornado.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag146_tornado.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Tornado task draws THEN updates (the funnel: updates, then draws). A real tick records
// every draw in order: the node / pool entry as drawn, the GTE registers the draw started with and
// the Field_Alloc scratch under its header (fields a draw does not set keep what the scratch held;
// the sprite pools reuse one header, its bytes are recorded per sprite). A held frame replays the
// log in the same order from the same GTE state and scratch, each part at its in-between state
// (the update computed on a copy of the drawn state, no random draw, nothing written back):
//   Capture  - the director's top-down stage capture (tick 0 of an action): replayed as drawn, into
//              private copies of its packets.
//   Pieces   - position / yaw / spin angles half way along their next update (rise phases); the
//              model (ground or airborne path) follows the in-between height.
//   Ground   - the crater depth factor half way to the next tick's (fade at ticks 64..68);
//              otherwise the crater is fixed.
//   Funnel   - drawn after its update: the next tick's rings are predicted from the deterministic
//              part of 0x617720 (each ring takes the previous ring's tilt; ring 0 the wander offset,
//              which moves along its current wander angles; parameters at counter + 1), the random
//              tilt / wander jitter (-64..63 / -256..255 per tick) is left out; ring positions and
//              radii half way, spin angle and fade half way. Private copy of the funnel packets.
//   Sparks   - size and height half way along the next update (shrink, accelerated fall); the
//              flipbook frame holds; a sprite at its last frame holds.
//   Morph    - vertices half way to the next tick's (base + delta * (d + 1), the target shape at
//              12), fade half way; private vertex buffer.
//   Debris   - x / z half way along the damped slide; the flipbook frame holds.
// Not drawn by the module (hold, 15 Hz): the lifted targets' positions / spin / shadow flag
// (entity state the battle draws), the camera shake word 0x1D97712.
// Draw-only ticks (battle_to_update_flags & 0x201) do not move: everything holds.
// The held frame draws into private packet buffers (module arena, frame arena, funnel and capture
// packets) and puts back what the draws write outside them: the stage vertex workspace
// (0x1D98B3C), the stage ground group's bone matrices, RenderGeometry's 0x1D99C18, the Field_Alloc
// scratch, the shadow matrix scratch and the GTE.

namespace ff8fx
{
namespace tornado146
{
	static const uint32_t SCRATCH = 0x200;   // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 400;

	struct Rec
	{
		uint32_t fn;                           // task function (kind)
		uint32_t live;                         // node / pool entry
		uint8_t node[0x24];                    // node / entry as drawn (debris: life 8 + position 16)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];              // Field_Alloc scratch / sprite or model header
		uint8_t extra[0x3C0];                  // funnel: rings + wander; morph: vertices as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_arena = 0; // module / frame arena cursors at the tick's first draw
	static bool g_replaying = false;
	static Rec g_recs[MAX_RECS];

	static uint8_t g_held_packets[0x10000];
	static uint8_t g_held_arena[0x40000];
	static uint8_t g_funnel_packets[0x5000];
	static uint8_t g_cap_env[0x40], g_cap_black[0x18], g_cap_mask[8], g_cap_area[0xC], g_cap_offset[0xC], g_cap_xy[4];
	static uint8_t g_morph_verts[0x3C0];
	static uint8_t g_rings[17 * RING_SIZE];
	static uint8_t g_workspace_save[0x10000];
	static uint8_t g_bones_save[0x40 + 0x30 * 256];

	static void SaveGte(uint8_t *g)
	{
		memcpy(g, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(g + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}

	static void LoadGte(const uint8_t *g)
	{
		memcpy((void *)GTE_DATA, g, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, g + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static void held_note_root()
	{
		if (g_replaying) return;
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_arena = 0;
	}

	static Rec *held_add(uint32_t fn, uint32_t live, const void *src, uint32_t size, const void *scratch, uint32_t scratch_size)
	{
		if (g_replaying || g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = live;
		memcpy(r.node, src, size);
		SaveGte(r.gte);
		memcpy(r.scratch, scratch, scratch_size);
		return &r;
	}

	static const void *FieldScratch() { return var<const void *>(0x1D999C4); }

	static void held_note_capture(uint32_t director) { held_add(ORIG_Director, director, (const void *)director, 0x24, FieldScratch(), SCRATCH); }

	static void held_note_draw(uint32_t fn, uint32_t node)
	{
		Rec *r = held_add(fn, node, (const void *)node, 0x24, FieldScratch(), SCRATCH);
		if (!r) return;
		if (fn == ORIG_Ground && !g_arena) g_arena = FrameArena();
		if (fn == ORIG_Funnel)
		{
			memcpy(r->extra, (const void *)RINGS, 17 * RING_SIZE);
			memcpy(r->extra + 17 * RING_SIZE, (const void *)WANDER_Angle, 6);
			memcpy(r->extra + 17 * RING_SIZE + 6, (const void *)WANDER_Offset, 6);
		}
	}

	static void held_note_item(uint32_t fn, uint32_t item, uint32_t header)
	{
		if (fn == ORIG_Sparks) held_add(fn, item, (const void *)item, 0x10, (const void *)header, 0xB4);
		else
		{
			Rec *r = held_add(fn, item, (const void *)item, 0x24, (const void *)header, 0x58);
			if (r) memcpy(r->extra, (const void *)U32(header, 4), 120 * 8);
		}
	}

	static void held_note_debris(uint32_t life, uint32_t pos, uint32_t header)
	{
		uint8_t both[0x18];
		memcpy(both, (const void *)life, 8);
		memcpy(both + 8, (const void *)pos, 0x10);
		held_add(ORIG_Debris, life, both, sizeof(both), (const void *)header, 0xB4);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the funnel's next rings: 0x617720 without its random jitter
	static void FunnelPredict(uint32_t rings, int16_t *wander_angle, int16_t *wander_offset, const FunnelParams &P)
	{
		uint8_t h[0x28];
		int32_t k = 16;
		for (uint32_t ring = rings + 16 * RING_SIZE; ring >= rings; ring -= RING_SIZE, k--)
		{
			uint32_t a = ring + 8;
			if (ring > rings)
				for (int m = 0; m < 3; m++) S16(a, 2 * m) = S16(a, 2 * m - 0x14);
			else
			{
				for (int m = 0; m < 3; m++) S16(a, 2 * m) = wander_offset[m];
				for (int m = 0; m < 3; m++) wander_offset[m] = (int16_t)(wander_offset[m] + (int16_t)(shl32(Sin(wander_angle[m]), 9) >> 12));
			}
			RingPlace(ring, k, P, (uint32_t)h);
		}
	}

	static void HeldFunnel(const Rec &r, bool moving, int num, int den)
	{
		uint32_t f = (uint32_t)r.node;
		int32_t c = S16(f, 0xC);
		memcpy(g_rings, r.extra, sizeof(g_rings));
		int32_t angle = c * 100, alpha = FunnelAlpha(c);
		if (moving)
		{
			static uint8_t next[17 * RING_SIZE];
			int16_t wa[3], wo[3];
			memcpy(next, r.extra, sizeof(next));
			memcpy(wa, r.extra + 17 * RING_SIZE, 6);
			memcpy(wo, r.extra + 17 * RING_SIZE + 6, 6);
			FunnelPredict((uint32_t)next, wa, wo, FunnelPhase(c + 1, r.live));
			for (int k = 0; k < 17; k++)
			{
				uint32_t a = (uint32_t)g_rings + k * RING_SIZE, b = (uint32_t)next + k * RING_SIZE;
				for (int m = 0; m < 3; m++) S16(a, 2 * m) = lerp16(S16(a, 2 * m), S16(b, 2 * m), num, den);
				S32(a, 0x10) = lerp_i(S32(a, 0x10), S32(b, 0x10), num, den);
			}
			angle = lerp_i(c * 100, (c + 1) * 100, num, den);
			alpha = lerp_i(FunnelAlpha(c), FunnelAlpha(c + 1), num, den);
		}
		FunnelDraw((uint32_t)g_rings, (uint32_t)g_funnel_packets, angle, alpha);
	}

	static void HeldPiece(const Rec &r, bool moving, int num, int den)
	{
		uint8_t a[0x24], b[0x24];
		memcpy(a, r.node, sizeof(a));
		memcpy(b, r.node, sizeof(b));
		uint32_t pa = (uint32_t)a, pb = (uint32_t)b;
		if (moving)
		{
			PieceUpdate(pb);
			S16(pa, 0x12) = lerp16(S16(pa, 0x12), S16(pb, 0x12), num, den);
			S16(pa, 0x18) = lerp16(S16(pa, 0x18), S16(pb, 0x18), num, den);
			S16(pa, 0x1C) = lerp16(S16(pa, 0x1C), S16(pb, 0x1C), num, den);
			S16(pa, 0x20) = lerp16(S16(pa, 0x20), S16(pb, 0x20), num, den);
		}
		PieceDraw(pa);
	}

	static void HeldMorph(const Rec &r, bool moving, int num, int den)
	{
		uint32_t p = (uint32_t)r.node;
		int32_t d = S16(p, 0xC);
		uint32_t h = Alloc(0x58);
		memcpy((void *)h, r.scratch, 0x58);
		memcpy(g_morph_verts, r.extra, sizeof(g_morph_verts));
		if (moving)
		{
			int32_t n = d + 1;
			for (int i = 0; i < 120; i++)
				for (int c = 0; c < 3; c++)
				{
					int16_t next = n < 12 ? (int16_t)((int16_t)(S16(MORPH_Delta + 8 * i, 2 * c) * n) + S16(MORPH_Base + 8 * i, 2 * c)) : S16(MORPH_Target + 8 * i, 2 * c);
					uint32_t v = (uint32_t)g_morph_verts + 8 * i;
					S16(v, 2 * c) = lerp16(S16(v, 2 * c), next, num, den);
				}
			S32(h, 0xC) = lerp_i(d * 341, n * 341, num, den);
		}
		U32(h, 4) = (uint32_t)g_morph_verts;
		LoadGte(r.gte);
		PacketCursor() = PrimModel(h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void HeldSpark(const Rec &r, bool moving, int num, int den)
	{
		uint8_t a[0x10], b[0x10];
		memcpy(a, r.node, sizeof(a));
		memcpy(b, r.node, sizeof(b));
		uint32_t pa = (uint32_t)a, pb = (uint32_t)b;
		if (moving)
		{
			SparkUpdate(pb);
			if (S32(pb, 0) >= 0)
			{
				S32(pa, 4) = lerp_i(S32(pa, 4), S32(pb, 4), num, den);
				S16(pa, 0xA) = lerp16(S16(pa, 0xA), S16(pb, 0xA), num, den);
			}
		}
		uint32_t h = Alloc(0xB4);
		memcpy((void *)h, r.scratch, 0xB4);
		LoadGte(r.gte);
		SparkDraw(h, pa);
		FieldFree(0xB4);
	}

	static void HeldDebris(const Rec &r, bool moving, int num, int den)
	{
		uint8_t a[0x18], b[0x18];
		memcpy(a, r.node, sizeof(a));
		memcpy(b, r.node, sizeof(b));
		uint32_t pa = (uint32_t)a, pb = (uint32_t)b;
		if (moving)
		{
			DebrisUpdate(pb, pb + 8);
			if (S16(pb, 6) >= 0)
			{
				S16(pa, 8) = lerp16(S16(pa, 8), S16(pb, 8), num, den);
				S16(pa, 0xC) = lerp16(S16(pa, 0xC), S16(pb, 0xC), num, den);
			}
		}
		uint32_t h = Alloc(0xB4);
		memcpy((void *)h, r.scratch, 0xB4);
		LoadGte(r.gte);
		DebrisDraw(h, pa, pa + 8);
		FieldFree(0xB4);
	}

	// vertex workspace bytes the ground draw writes: the largest object of the stage ground group
	static uint32_t WorkspaceBytes()
	{
		uint32_t grp = STAGE_Ground;
		if (!U32(grp, 4)) return 0;
		uint32_t ob = U32(grp, 4);
		int32_t count = S32(ob, 0);
		uint32_t most = 0;
		for (int32_t o = 0; o < count; o++)
		{
			uint32_t d = U32(grp, 4) + U32(ob, 4 + 4 * o);
			int32_t groups = S16(d, 0);
			d += 2;
			uint32_t verts = 0;
			for (; groups > 0; groups--)
			{
				int32_t nv = S16(d, 2);
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

	static void HeldFrame(int num, int den)
	{
		if (g_nrec == 0) return;
		// a held frame larger than the private buffers is not drawn (never seen)
		if (PacketCursor() - g_packets > sizeof(g_held_packets) / 2) return;
		if (g_arena && FrameArena() - g_arena > sizeof(g_held_arena) / 2) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		uint32_t arena = FrameArena();
		if (g_arena && arena > g_arena) memcpy(g_held_arena, (const void *)g_arena, arena - g_arena);
		FrameArena() = (uint32_t)g_held_arena;
		// what the ground / capture draws write outside their packets
		uint32_t ws = var<uint32_t>(0x1D98B3C);
		uint32_t ws_bytes = ws ? WorkspaceBytes() : 0;
		if (ws_bytes > sizeof(g_workspace_save)) ws_bytes = sizeof(g_workspace_save);
		if (ws_bytes) memcpy(g_workspace_save, (const void *)ws, ws_bytes);
		uint8_t *sk = nullptr;
		uint32_t sk_bytes = 0;
		uint8_t **pp = *(uint8_t ***)(STAGE_GroundAnim + 4);
		if (pp && *pp)
		{
			sk = *pp;
			sk_bytes = 0x40 + 0x30 * (uint32_t)sk[0];
			if (sk_bytes > sizeof(g_bones_save)) sk_bytes = sizeof(g_bones_save);
			memcpy(g_bones_save, sk, sk_bytes);
		}
		uint32_t last_packet = var<uint32_t>(0x1D99C18); // RenderGeometry: last packet it wrote
		memcpy(g_funnel_packets, (const void *)FUNNEL_Packets, sizeof(g_funnel_packets));
		bool moving = !DrawOnly();
		g_replaying = true;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.fn == ORIG_Sparks) { HeldSpark(r, moving, num, den); continue; }
			if (r.fn == ORIG_Morph) { HeldMorph(r, moving, num, den); continue; }
			if (r.fn == ORIG_Debris) { HeldDebris(r, moving, num, den); continue; }
			// task draws: from the recorded GTE state and Field_Alloc scratch
			LoadGte(r.gte);
			memcpy((void *)FieldScratch(), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_Director:
			{
				memcpy(g_cap_env, (const void *)PKT_Env, sizeof(g_cap_env));
				memcpy(g_cap_black, (const void *)PKT_Black, sizeof(g_cap_black));
				memcpy(g_cap_mask, (const void *)PKT_Mask, sizeof(g_cap_mask));
				memcpy(g_cap_area, (const void *)PKT_Area, sizeof(g_cap_area));
				memcpy(g_cap_offset, (const void *)PKT_Offset, sizeof(g_cap_offset));
				memcpy(g_cap_xy, (const void *)OFFSET_XY, sizeof(g_cap_xy));
				CapturePackets pk = { (uint32_t)g_cap_env, (uint32_t)g_cap_black, (uint32_t)g_cap_mask, (uint32_t)g_cap_area, (uint32_t)g_cap_offset, (uint32_t)g_cap_xy };
				Capture((uint32_t)r.node + 0x10, pk);
				break;
			}
			case ORIG_Piece:
				HeldPiece(r, moving, num, den);
				break;
			case ORIG_Ground:
			{
				int32_t c = S16((uint32_t)r.node, 0xC);
				int32_t factor = GroundFactor(c);
				if (moving) factor = lerp_i(factor, GroundFactor(c + 1), num, den);
				GroundTaskDraw((uint32_t)r.node, factor);
				break;
			}
			case ORIG_Funnel:
				HeldFunnel(r, moving, num, den);
				break;
			}
		}
		g_replaying = false;
		var<uint32_t>(0x1D99C18) = last_packet;
		if (sk) memcpy(sk, g_bones_save, sk_bytes);
		if (ws_bytes) memcpy((void *)ws, g_workspace_save, ws_bytes);
		FrameArena() = arena;
	}
}

	static void register_mag146_held()
	{
		mark_held(tornado146::ORIG_Director);
		mark_held(tornado146::ORIG_Piece);
		mark_held(tornado146::ORIG_Ground);
		mark_held(tornado146::ORIG_Funnel);
		mark_held(tornado146::ORIG_Sparks);
		mark_held(tornado146::ORIG_Morph);
		mark_held(tornado146::ORIG_Debris);
		register_module_held(146, tornado146::HeldReady, tornado146::HeldFrame);
	}
}
