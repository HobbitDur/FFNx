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

// 30 fps part of mag038_quake.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag038_quake.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in order: each ground pass (GTE state at its start) and each
// ground edge it drew (the inputs the draw reads: zone height, state, crack tick, crack steps),
// each debris and each sprite (node copy, GTE state, the Field_Alloc scratch under the sprite
// header). A held frame replays the log in the same order, each part at its in-between state:
//   Ground  - the mesh entries are fixed after the build; once the zones are raised (state >= 5)
//             every sub-triangle and side wall is drawn at its zone height half way between the
//             height drawn on the real tick and the one the next tick draws (the zones move after
//             the ground is drawn, so that is the current height). Crack lines / growing crack
//             walls (states 3, 4) grow in 15 Hz steps: they hold.
//   Debris  - drawn after its update: half way along its next spin / fall step (a landed piece
//             holds).
//   Rock / dust / chip sprites - drawn after their update: position half way along their next
//             velocity step (rock: damped rise, dust: accelerated drift, chip: damped throw);
//             the flipbook frame holds.
// Camera shake (a per-tick table value), the entities' fades and bounces (drawn by the battle)
// stay at 15 Hz. No Quake task has a draw-only mode: everything moves on every tick.

namespace ff8fx
{
namespace quake038
{
	static const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;
	enum : uint8_t { REC_GROUND = 1, REC_EDGE = 2, REC_DEBRIS = 3, REC_SPRITE = 4 };

	struct Gte { uint8_t b[GTE_DATA_SIZE + GTE_CTRL_SIZE]; };
	struct EdgeRec
	{
		int16_t id, st, h, tick;
		uint8_t k, a, b, n;
		uint8_t line_step[3];  // crack steps the lines read (before this edge's walls grew)
		uint8_t wall_step[3];  // crack wall drawn at this step (0xFF: none)
	};
	struct DebrisRec { uint8_t node[0xB0]; Gte gte; };
	struct SpriteRec { uint8_t node[0xAC]; uint8_t hdr[0xB4]; Gte gte; };
	struct Order { uint8_t kind; uint16_t idx; };

	static const int MAX_ORDER = 20000, MAX_EDGES = 16384, MAX_GROUND = 16, MAX_DEBRIS = 128, MAX_SPRITES = 1024;
	static Order g_order[MAX_ORDER];
	static EdgeRec g_edges[MAX_EDGES];
	static Gte g_ground[MAX_GROUND];
	static DebrisRec g_debris[MAX_DEBRIS];
	static SpriteRec g_sprites[MAX_SPRITES];
	static int g_norder = 0, g_nedges = 0, g_nground = 0, g_ndebris = 0, g_nsprites = 0;
	static EdgeRec *g_cur_edge = nullptr;

	// real tick on which the ported root task last ran, and the tick of the log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_packets = 0; // packet cursor at the tick's first draw
	static bool g_replaying = false;
	static uint8_t g_held_packets[0x40000];

	static void SaveGte(Gte &g)
	{
		memcpy(g.b, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(g.b + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}

	static void LoadGte(const Gte &g)
	{
		memcpy((void *)GTE_DATA, g.b, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, g.b + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static bool Logging() { return !g_replaying && g_log_tick == g_real_tick && g_norder < MAX_ORDER; }

	static void AddOrder(uint8_t kind, int idx, uint32_t cursor)
	{
		if (g_norder == 0) g_packets = cursor;
		g_order[g_norder].kind = kind;
		g_order[g_norder].idx = (uint16_t)idx;
		g_norder++;
	}

	static void held_note_root()
	{
		if (g_replaying) return;
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_norder = g_nedges = g_nground = g_ndebris = g_nsprites = 0;
		g_cur_edge = nullptr;
		g_packets = 0;
	}

	static void held_note_ground()
	{
		g_cur_edge = nullptr;
		if (!Logging() || g_nground >= MAX_GROUND) return;
		SaveGte(g_ground[g_nground]);
		AddOrder(REC_GROUND, g_nground++, PacketCursor());
	}

	static void held_note_edge(int32_t id, int32_t k, int32_t a, int32_t b, int32_t n, int16_t st, int16_t h)
	{
		g_cur_edge = nullptr;
		if (!Logging() || g_nedges >= MAX_EDGES) return;
		EdgeRec &r = g_edges[g_nedges];
		r.id = (int16_t)id;
		r.k = (uint8_t)k;
		r.a = (uint8_t)a;
		r.b = (uint8_t)b;
		r.n = (uint8_t)n;
		r.st = st;
		r.h = h;
		r.tick = CrackTick();
		uint32_t e = Entry(id);
		for (int m = 0; m < 3; m++)
		{
			r.line_step[m] = U8(e, 3 * k + m + 0x40);
			r.wall_step[m] = 0xFF;
		}
		g_cur_edge = &r;
		AddOrder(REC_EDGE, g_nedges++, PacketCursor());
	}

	static void held_note_wall(int32_t m, uint8_t step)
	{
		if (g_replaying || !g_cur_edge) return;
		g_cur_edge->wall_step[m] = step;
	}

	static void held_note_debris(uint32_t n)
	{
		if (!Logging() || g_ndebris >= MAX_DEBRIS) return;
		DebrisRec &r = g_debris[g_ndebris];
		memcpy(r.node, (const void *)n, sizeof(r.node));
		SaveGte(r.gte);
		AddOrder(REC_DEBRIS, g_ndebris++, PacketCursor());
	}

	static void held_note_sprite(uint32_t n)
	{
		if (g_replaying) return;
		g_hdr_slot = nullptr;
		if (!Logging() || g_nsprites >= MAX_SPRITES) return;
		SpriteRec &r = g_sprites[g_nsprites];
		memcpy(r.node, (const void *)n, sizeof(r.node));
		SaveGte(r.gte);
		g_hdr_slot = r.hdr; // the header scratch goes to the record (AllocHeader)
		AddOrder(REC_SPRITE, g_nsprites++, PacketCursor());
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// 12-bit angle half way (num / den) the short way round
	static int16_t LerpAngle12(int32_t a, int32_t b, int num, int den)
	{
		int32_t d = ((b - a + 0x800) & 0xFFF) - 0x800;
		return (int16_t)((a + d * num / den) & 0xFFF);
	}

	static void HeldEdge(const EdgeRec &r, int num, int den)
	{
		uint32_t e = Entry(r.id);
		uint32_t ot = RenderOT44();
		int16_t h = r.h;
		if (r.st > 4) h = lerp16(r.h, ZoneHeight(U8(e, r.k + 0x58)), num, den);
		int16_t v[12] = {};
		if (r.st > 2)
		{
			memcpy(v, (const void *)(e + r.a * 8), 8);
			memcpy(v + 4, (const void *)(e + r.b * 8), 8);
			memcpy(v + 8, (const void *)(e + r.n * 8), 8);
			v[1] = (int16_t)(v[1] + h);
			v[5] = (int16_t)(v[5] + h);
			v[9] = (int16_t)(v[9] + h);
		}
		if (r.st > 4) PacketCursor() = RaisedEdge(e, r.k, r.a, r.b, r.n, v, h, ot, PacketCursor());
		if (r.st == 3 || r.st == 4)
		{
			// crack lines and walls in their recorded steps (they grow in 15 Hz steps)
			static const int P[3][2] = { { 4, 0 }, { 8, 4 }, { 0, 8 } };
			for (int m = 0; m < 3; m++)
				if (U8(e, r.k + 0x60) & (1 << m))
					PacketCursor() = CrackLine(e, PacketCursor(), v + P[m][0], v + P[m][1], r.k, r.tick, r.line_step[m]);
			if (r.st == 4)
				for (int m = 0; m < 3; m++)
				{
					if (r.wall_step[m] == 0xFF || !(U8(e, r.k + 0x60) & (1 << m))) continue;
					int16_t w[16];
					bool drawn;
					PacketCursor() = CrackWallDraw(e, PacketCursor(), v + P[m][0], v + P[m][1], r.wall_step[m], w, &drawn);
				}
		}
	}

	static void HeldDebris(const DebrisRec &r, int num, int den)
	{
		uint8_t a[0xB0], b[0xB0];
		memcpy(a, r.node, sizeof(a));
		memcpy(b, r.node, sizeof(b));
		uint32_t pa = (uint32_t)a, pb = (uint32_t)b;
		if (PhaseOf(pa) == 1)
		{
			DebrisMove(pb, false);
			S16(pa, 0x1E) = lerp16(S16(pa, 0x1E), S16(pb, 0x1E), num, den);
			S16(pa, 0x44) = LerpAngle12(S16(pa, 0x44), S16(pb, 0x44), num, den);
			S16(pa, 0x46) = LerpAngle12(S16(pa, 0x46), S16(pb, 0x46), num, den);
		}
		LoadGte(r.gte);
		DebrisDraw(pa);
	}

	static void HeldSprite(const SpriteRec &r, int num, int den)
	{
		uint8_t a[0xAC];
		memcpy(a, r.node, sizeof(a));
		uint32_t n = (uint32_t)a;
		int16_t dx = 0, dy = 0, dz = 0;
		switch (U32(n, 8))
		{
		case ORIG_Rock:
			dy = RockSpeed(n);
			break;
		case ORIG_Dust:
			dx = S16(n, 0xA0);
			dy = (int16_t)(S16(n, 0xA2) + S16(n, 0xA6));
			dz = S16(n, 0xA4);
			break;
		case ORIG_Chip:
		{
			int16_t v = S16(n, 0xA2);
			dx = S16(n, 0xA0);
			dy = (int16_t)(v - (int16_t)(v / 16));
			dz = S16(n, 0xA4);
			break;
		}
		}
		S16(n, 0x1C) = lerp16(S16(n, 0x1C), (int16_t)(S16(n, 0x1C) + dx), num, den);
		S16(n, 0x1E) = lerp16(S16(n, 0x1E), (int16_t)(S16(n, 0x1E) + dy), num, den);
		S16(n, 0x20) = lerp16(S16(n, 0x20), (int16_t)(S16(n, 0x20) + dz), num, den);
		LoadGte(r.gte);
		g_hdr_slot = (uint8_t *)r.hdr;
		SpriteDraw(n);
		g_hdr_slot = nullptr;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// the crack lines load the GTE light matrix: its float mirror (0x1CA9210..0x1CA922F) lies
		// below the control-register window HeldScope restores
		uint8_t light_mirror[0x20];
		memcpy(light_mirror, (const void *)0x1CA9210, sizeof(light_mirror));
		g_replaying = true;
		for (int i = 0; i < g_norder; i++)
		{
			const Order &o = g_order[i];
			switch (o.kind)
			{
			case REC_GROUND:
				LoadGte(g_ground[o.idx]);
				break;
			case REC_EDGE:
				HeldEdge(g_edges[o.idx], num, den);
				break;
			case REC_DEBRIS:
				HeldDebris(g_debris[o.idx], num, den);
				break;
			case REC_SPRITE:
				HeldSprite(g_sprites[o.idx], num, den);
				break;
			}
		}
		g_replaying = false;
		memcpy((void *)0x1CA9210, light_mirror, sizeof(light_mirror));
	}
}

	static void register_mag038_held()
	{
		mark_held(quake038::ORIG_Sequence);
		mark_held(quake038::ORIG_Debris);
		mark_held(quake038::ORIG_Rock);
		mark_held(quake038::ORIG_Dust);
		mark_held(quake038::ORIG_Chip);
		register_module_held(38, quake038::HeldReady, quake038::HeldFrame);
	}
}
