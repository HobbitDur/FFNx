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

// 30 fps part of mag314_abadon.cpp: the draw log of the real tick and the held-frame drawing (included at the end of
// mag314_abadon.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The main task draws, in order, sweep A or the caster with the pose offsets, the white tiles, the particles and the
// shards, each THEN updates (the bone scripts, run after the main, move the pose offsets the next tick draws). A real tick
// records every draw in order with the GTE registers and the Field_Alloc scratch it started from. A held frame replays the
// log into a private module arena (the real tick's packets under it) and a private frame arena:
//   sweep     - as 316's: the caster at its in-between pose (pose_midpoint at num / den), the level half way to the next
//               tick's (84 a tick) while the next tick sweeps too; the vertices into a private copy of the noted buffer;
//   caster    - the pose offsets half way to the ones the scripts left for the next tick (translations linearly, rotations
//               the short way round on 12 bits; the hide words hold), drawn by the module's caster draw into a private
//               projected-vertex buffer; the caster's animation pose holds (as Griever's 064); the last draw holds; the
//               caster's skeleton, its entity record and the object renderer's cursor 0x1D99C18 are put back;
//   tiles     - the level of 0x659A10 (in 1, out 4) half way to the next tick's; the last tile of a window holds;
//   particles - half way along their velocity; the sequence frame holds; a particle on its last tick holds;
//   shards    - a shard that lives on (same slot, life one less): position and corners half way to the update's (the
//               spin is a matrix product: the corners move on the chord); one that dies or whose slot is reused holds.
// The bone scripts' flat quad (0x10B0) is not in the effect's 29 programs: a held frame does not draw it. The screen flash
// level, the entity flags / colours and the camera animation are engine state (the gate). No draw-only mode.

namespace ff8fx
{
namespace abadon314
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;         // module packet arena per tick
	static const uint32_t FRAME = 0x40000;         // caster packets (frame arena)
	static const uint32_t VERTS = 0x20000;         // the engine's projected-vertex scratch [0x1D98B3C]
	static const uint32_t SKEL_MAX = 0x20 + 0x30 * 256;
	static const int REC_MAX = 8;

	enum RecKind : uint8_t { REC_SWEEP, REC_CASTER, REC_TILE, REC_PARTS };
	struct Rec
	{
		RecKind kind;
		int16_t c;                  // main counter (sweep / caster), tile step (tile)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static Rec g_recs[REC_MAX];
	static uint32_t g_packets = 0;                 // module packet cursor at the tick's start
	static uint32_t g_frame = 0;                   // frame arena at the tick's first caster draw
	static uint8_t g_verts_drawn[VERTS];           // the vertex scratch as the caster / sweep draw found it
	static PoseEntry g_pose_drawn[0x70];           // the pose offsets the caster was drawn with
	static PoseEntry g_pose_held[0x70];
	static Particle g_parts[0x80];                 // the particles as drawn
	static Shard g_shards[0x100];                  // the shards as drawn
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_verts[VERTS];
	static uint8_t g_skel_save[SKEL_MAX];
	static uint8_t g_ent_save[0x9C];
	static MainNode *g_held_main = nullptr;        // scratch main node (particles), allocated once
	static const int POLYS_MAX = 0x4000;
	static uint8_t g_polyflags[POLYS_MAX];         // the caster's polygons' +0x0F bytes as drawn (the shard sweep hides some after the draw)
	static uint8_t g_polyflags_now[POLYS_MAX];
	static int g_npolys = 0;
	static Shard g_held_shards[0x100];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
		g_frame = 0;
	}

	static Rec *held_add(RecKind kind, int16_t c)
	{
		if (g_log_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.c = c;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void NoteVerts()
	{
		if (!g_frame) g_frame = FrameArena();
		const uint32_t v = var<uint32_t>(0x1D98B3C);
		if (v) memcpy(g_verts_drawn, (const void *)v, VERTS);
	}

	static void held_note_main(const MainNode *m)
	{
		if ((uint32_t)(m->counter - 0x10) >= 0x32) return;
		if (!held_add(REC_SWEEP, m->counter)) return;
		NoteVerts();
	}

	// every polygon's flag byte (+0x0F) of a model, in object order (triangles 0x10 bytes, quads 0x14)
	template<typename V> static int ForEachPolyFlag(const uint8_t *model, V visit)
	{
		const uint32_t *tab = *(const uint32_t *const *)(model + 4);
		const int32_t count = (int32_t)*tab++;
		int k = 0;
		for (int32_t n = 0; n < count; n++)
		{
			uint8_t *o = (uint8_t *)(*tab++ + *(const uint32_t *)(model + 4));
			const int32_t groups = *(const int16_t *)o;
			o += 2;
			for (int32_t g = 0; g < groups; g++) o += 4 + *(const int16_t *)(o + 2) * 6;
			o = (uint8_t *)(((uint32_t)o + 3) & ~3u);
			const uint32_t tris = *(const uint16_t *)o, quads = *(const uint16_t *)(o + 2);
			uint8_t *p = o + 0xC;
			for (uint32_t i = 0; i < tris; i++, p += 0x10) if (k < POLYS_MAX) visit(k++, p + 0xF);
			for (uint32_t i = 0; i < quads; i++, p += 0x14) if (k < POLYS_MAX) visit(k++, p + 0xF);
		}
		return k;
	}

	static void held_note_caster(const MainNode *m)
	{
		if (!held_add(REC_CASTER, m->counter)) return;
		NoteVerts();
		memcpy(g_pose_drawn, m->pose, sizeof(g_pose_drawn));
		g_npolys = ForEachPolyFlag(*(const uint8_t *const *)(Caster() + 0x64), [](int k, uint8_t *f) { g_polyflags[k] = *f; });
	}

	static void held_note_tiles(int32_t c)
	{
		if ((uint32_t)c < 5) held_add(REC_TILE, (int16_t)c);
		if ((uint32_t)(c - 0xF) < 5) held_add(REC_TILE, (int16_t)(c - 0xF));
		if ((uint32_t)(c - 0x13) < 5) held_add(REC_TILE, (int16_t)(c - 0x13));
	}

	static void held_note_parts(const MainNode *m)
	{
		if (!held_add(REC_PARTS, m->counter)) return;
		memcpy(g_parts, m->parts, sizeof(g_parts));
		memcpy(g_shards, m->shards, sizeof(g_shards));
	}

	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// 0x659A10's level at step t with in 1, out 4
	static int32_t TileLevel(int32_t t)
	{
		if (t < 1) return shl32(t + 1, 12) / 1;
		return shl32(1 - t - 1, 12) / 4 + 0x1000;
	}
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile

	// 12-bit angle half way the short way round
	static int16_t LerpAngle12(int16_t a, int16_t b, int num, int den)
	{
		const int32_t d = (((int32_t)(int16_t)(b - a) + 0x800) & 0xFFF) - 0x800;
		return (int16_t)(a + d * num / den);
	}

	// the caster draws (sweep / pose offsets) with everything they write outside the scope put back
	template<typename D> static void CasterScope(D draw)
	{
		uint8_t *ent = Caster();
		uint8_t *skel = *(uint8_t **)(ent + 0x64) ? **(uint8_t ***)(ent + 0x64) : nullptr;
		uint32_t skel_n = skel ? 0x20 + 0x30 * (uint32_t)skel[0] : 0;
		if (skel_n > SKEL_MAX) skel_n = 0;
		if (skel_n) memcpy(g_skel_save, skel, skel_n);
		memcpy(g_ent_save, ent, sizeof(g_ent_save));
		const uint32_t cursor_c18 = var<uint32_t>(0x1D99C18);
		const uint32_t frame = FrameArena();
		if (g_frame && frame > g_frame)
		{
			const uint32_t used = frame - g_frame;
			memcpy(g_held_frame, (const void *)g_frame, used < FRAME ? used : FRAME);
		}
		FrameArena() = (uint32_t)g_held_frame;
		const uint32_t verts = var<uint32_t>(0x1D98B3C);
		memcpy(g_held_verts, g_verts_drawn, VERTS);
		var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_verts;
		draw(ent, skel_n != 0);
		var<uint32_t>(0x1D98B3C) = verts;
		FrameArena() = frame;
		if (skel_n) memcpy(skel, g_skel_save, skel_n);
		memcpy(ent, g_ent_save, sizeof(g_ent_save));
		var<uint32_t>(0x1D99C18) = cursor_c18;
	}

	static void SweepHeld(const Rec &r, int num, int den)
	{
		int16_t level = LevelA(r.c);
		if (num != 0 && (uint32_t)(r.c + 1 - 0x10) < 0x32) level = (int16_t)lerp_i(level, LevelA(r.c + 1), num, den);
		CasterScope([&](uint8_t *ent, bool posed)
		{
			if (posed) pose_midpoint(ent + 0x60, ent + 0x6C, num, den);
			SweepA(ent, level, PAGE_MOVE, CLUT_MOVE, PacketCursor());
		});
	}

	static void CasterHeld(const Rec &r, int num, int den)
	{
		const MainNode *m = (const MainNode *)(TexBase() + MAIN);
		const bool next = r.c + 1 < 0xE2;   // the next tick draws the caster again
		for (int i = 0; i < 0x70; i++)
		{
			PoseEntry p = g_pose_drawn[i];
			if (next)
			{
				const PoseEntry &b = m->pose[i];
				for (int k = 0; k < 3; k++)
				{
					p.pos[k] = lerp16(p.pos[k], b.pos[k], num, den);
					p.rot[k] = LerpAngle12(p.rot[k], b.rot[k], num, den);
				}
			}
			g_pose_held[i] = p;
		}
		// the polygons as drawn: the shard sweep after the draw hid some (they fly off as shards from the next tick)
		const uint8_t *model = *(const uint8_t *const *)(Caster() + 0x64);
		ForEachPolyFlag(model, [](int k, uint8_t *f) { g_polyflags_now[k] = *f; if (k < g_npolys) *f = g_polyflags[k]; });
		CasterScope([&](uint8_t *ent, bool) { CasterDraw(ent, g_pose_held, PAGE_MOVE, CLUT_MOVE); });
		ForEachPolyFlag(model, [](int k, uint8_t *f) { *f = g_polyflags_now[k]; });
	}

	static void PartsHeld(int num, int den)
	{
		const MainNode *m = (const MainNode *)(TexBase() + MAIN);
		memcpy(g_held_main->parts, g_parts, sizeof(g_parts));
		for (int k = 0; k < 0x80; k++)
		{
			Particle &p = g_held_main->parts[k];
			if (p.life == 0 || p.life == 1) continue;
			for (int a = 0; a < 3; a++) p.pos[a] = lerp16(p.pos[a], (int16_t)(p.pos[a] + (int16_t)(p.vel[a] << 2)), num, den);
		}
		Particles(g_held_main);
		// shards: the live ones that moved on, half way
		memcpy(g_held_shards, g_shards, sizeof(g_shards));
		for (int k = 0; k < 0x100; k++)
		{
			Shard &s = g_held_shards[k];
			const Shard &b = m->shards[k];
			if (s.kind == 0 || b.kind != s.kind || b.life != (int16_t)(s.life - 1)) continue;
			for (int a = 0; a < 3; a++)
			{
				s.pos[a] = lerp16(s.pos[a], b.pos[a], num, den);
				for (int c = 0; c < 4; c++) s.c[c][a] = lerp16(s.c[c][a], b.c[c][a], num, den);
			}
		}
		memcpy(g_held_main->shards, g_held_shards, sizeof(g_held_shards));
		ShardsDraw(g_held_main);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		if (!g_held_main) g_held_main = new MainNode();
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_SWEEP:
				SweepHeld(r, num, den);
				break;
			case REC_CASTER:
				CasterHeld(r, num, den);
				break;
			case REC_TILE:
			{
				int32_t level = TileLevel(r.c);
				if (r.c + 1 < 5) level = lerp_i(level, TileLevel(r.c + 1), num, den);
				PacketCursor() = Tile(0xFF, 0x80, 0x80, level, PacketCursor());
				break;
			}
			case REC_PARTS:
				PartsHeld(num, den);
				break;
			}
		}
	}
}

	static void register_mag314_held()
	{
		mark_held(abadon314::ORIG_MainTask);
		register_module_held(314, abadon314::HeldReady, abadon314::HeldFrame);
	}
}
