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

// 30 fps part of mag316_red_giant.cpp: the draw notes of the real tick and the held-frame drawing (included at the end of
// mag316_red_giant.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The main task draws the caster (a sweep) and then the particles, each THEN updates. A real tick notes, before each of the
// two draws, the GTE registers and the Field_Alloc scratch it starts from; for the sweep its counter, the frame arena cursor
// and the vertex buffer it projects into (the bytes a vertex entry keeps: +6 / +7 of a vertex behind the camera, B's +7),
// for the particles the pool as drawn (after the sweep's spawns). A held frame redraws, into a private module arena (the
// real tick's packets under it) and a private frame arena:
//   sweep     - the same sweep with the caster at its in-between pose (pose_midpoint at num / den: fraction 1 = the next
//               tick's pose) and the sweep level half way to the next tick's (A: 40 a tick, B: the 150-tick ramp); when
//               the next tick draws the other sweep or none, the level holds. The vertices go to a private copy of the
//               noted buffer; no particle is thrown (the module LCG stays 15 Hz). The caster's skeleton, its entity
//               record and the object renderer's cursor 0x1D99C18 are put back afterwards;
//   particles - each live particle half way along its velocity (the update adds 4 x velocity); its sequence frame holds;
//               a particle on its last tick (life 1) holds.
// The screen flash level, the caster's colour word +0x2C / flags and the camera animation are engine state (the gate).

namespace ff8fx
{
namespace redgiant316
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;         // module packet arena per tick
	static const uint32_t FRAME = 0x40000;         // caster packets (frame arena)
	static const uint32_t VERTS_ENG = 0x20000;     // sweep A: the engine's projected-vertex scratch [0x1D98B3C]
	static const uint32_t VERTS_MOD = ARENA_A - VERTS_B; // sweep B: magic buffer + 0x81C .. the first arena
	static const uint32_t SKEL_MAX = 0x20 + 0x30 * 256;

	enum SweepKind { SWEEP_NONE, SWEEP_A, SWEEP_B };
	static SweepKind SweepOf(int32_t c)
	{
		if ((uint32_t)(c - 0x14) < 0x82) return SWEEP_A;
		if ((uint32_t)(c - 0x96) < 0x96) return SWEEP_B;
		return SWEEP_NONE;
	}

	struct Snap { uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE]; uint8_t scratch[SCRATCH]; };
	static void SnapTake(Snap &s)
	{
		memcpy(s.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(s.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(s.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}
	static void SnapPut(const Snap &s)
	{
		memcpy((void *)GTE_DATA, s.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, s.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), s.scratch, SCRATCH);
	}

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_sweep_tick = 0xFFFFFFFF;
	static uint32_t g_part_tick = 0xFFFFFFFF;
	static bool g_drawing = false;
	static uint32_t g_packets = 0;                 // module packet cursor at the tick's start
	static Snap g_snap_sweep, g_snap_part;
	static SweepKind g_sweep = SWEEP_NONE;
	static int16_t g_counter = 0;
	static const MainNode *g_main = nullptr;
	static uint32_t g_frame = 0;                   // frame arena at the sweep
	static uint8_t g_verts_drawn[VERTS_ENG];       // the vertex buffer as the sweep found it
	static Particle g_parts[0x80];                 // the particles as drawn
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_verts[VERTS_ENG];
	static uint8_t g_skel_save[SKEL_MAX];
	static uint8_t g_ent_save[0x9C];
	static MainNode g_held_main;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_packets = PacketCursor();
		g_sweep_tick = g_part_tick = 0xFFFFFFFF;
	}

	static void held_note_main(const MainNode *m)
	{
		g_sweep = SweepOf(m->counter);
		if (g_sweep == SWEEP_NONE) return;
		g_sweep_tick = g_real_tick;
		g_counter = m->counter;
		g_main = m;
		g_frame = FrameArena();
		SnapTake(g_snap_sweep);
		if (g_sweep == SWEEP_A)
		{
			const uint32_t v = var<uint32_t>(0x1D98B3C);
			if (v) memcpy(g_verts_drawn, (const void *)v, VERTS_ENG);
		}
		else memcpy(g_verts_drawn, (const void *)(TexBase() + VERTS_B), VERTS_MOD);
	}

	static void held_note_particles(const MainNode *m)
	{
		g_part_tick = g_real_tick;
		memcpy(g_parts, m->parts, sizeof(g_parts));
		SnapTake(g_snap_part);
	}

	static bool held_drawing() { return g_drawing; }

	static int16_t LevelOf(SweepKind k, int32_t c) { return k == SWEEP_A ? LevelA(c) : LevelB(c); }

	static void SweepHeld(int num, int den)
	{
		SnapPut(g_snap_sweep);
		uint8_t *ent = Caster();
		uint8_t *skel = *(uint8_t **)(ent + 0x64) ? **(uint8_t ***)(ent + 0x64) : nullptr;
		uint32_t skel_n = skel ? 0x20 + 0x30 * (uint32_t)skel[0] : 0;
		if (skel_n > SKEL_MAX) skel_n = 0;
		if (skel_n) memcpy(g_skel_save, skel, skel_n);
		memcpy(g_ent_save, ent, sizeof(g_ent_save));
		const uint32_t cursor_c18 = var<uint32_t>(0x1D99C18);
		// the real tick's frame-arena packets under the held ones (bytes a draw leaves alone)
		const uint32_t frame = FrameArena();
		if (g_frame && frame > g_frame)
		{
			const uint32_t used = frame - g_frame;
			memcpy(g_held_frame, (const void *)g_frame, used < FRAME ? used : FRAME);
		}
		FrameArena() = (uint32_t)g_held_frame;
		int16_t level = LevelOf(g_sweep, g_counter);
		if (num != 0 && SweepOf(g_counter + 1) == g_sweep) level = (int16_t)lerp_i(level, LevelOf(g_sweep, g_counter + 1), num, den);
		if (skel_n) pose_midpoint(ent + 0x60, ent + 0x6C, num, den);
		g_drawing = true;
		if (g_sweep == SWEEP_A)
		{
			const uint32_t verts = var<uint32_t>(0x1D98B3C);
			memcpy(g_held_verts, g_verts_drawn, VERTS_ENG);
			var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_verts;
			SweepA(ent, level, PAGE_MOVE, CLUT_MOVE, PacketCursor());
			var<uint32_t>(0x1D98B3C) = verts;
		}
		else
		{
			memcpy(g_held_verts, g_verts_drawn, VERTS_MOD);
			SweepB((MainNode *)g_main, ent, (uint32_t)g_held_verts, level, PAGE_MOVE, CLUT_MOVE);
		}
		g_drawing = false;
		FrameArena() = frame;
		if (skel_n) memcpy(skel, g_skel_save, skel_n);
		memcpy(ent, g_ent_save, sizeof(g_ent_save));
		var<uint32_t>(0x1D99C18) = cursor_c18;
	}

	static void ParticlesHeld(int num, int den)
	{
		SnapPut(g_snap_part);
		memcpy(g_held_main.parts, g_parts, sizeof(g_parts));
		for (int k = 0; k < 0x80; k++)
		{
			Particle &p = g_held_main.parts[k];
			if (p.life == 0 || p.life == 1) continue;
			for (int a = 0; a < 3; a++) p.pos[a] = lerp16(p.pos[a], (int16_t)(p.pos[a] + (int16_t)(p.vel[a] << 2)), num, den);
		}
		Particles(&g_held_main);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		if (g_sweep_tick == g_real_tick) SweepHeld(num, den);
		if (g_part_tick == g_real_tick) ParticlesHeld(num, den);
	}
}

	static void register_mag316_held()
	{
		mark_held(redgiant316::ORIG_MainTask);
		register_module_held(316, redgiant316::HeldReady, redgiant316::HeldFrame);
	}
}
