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

// 30 fps part of mag017_elvoret_entrance.cpp: the draw log of the real tick and the held-frame
// drawing (included at the end of mag017_elvoret_entrance.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// The main task draws its layout after its spawns, then the particle queue draws THEN updates every
// particle. A real tick records every draw in order with the GTE registers and the Field_Alloc
// scratch it started from (draw headers keep the scratch bytes where the code does not write). A
// held frame replays the log in the same order:
//   Particle - position half way along its next step (position + velocity); the flipbook frame,
//              size and roll hold; a particle whose task ends after this draw holds. New particles
//              (random spawns) appear on the real tick.
//   Layout   - the prim-model layout through prim::play_held: every object's exact in-between record
//              (position, rotation, scale, colour, fade, vertex-frame blend); blended vertex frames
//              go to a private copy of the morph buffer. The anchor (the caster's x, height, z) is
//              the real tick's: the module never changes those words (it moves the caster's y,
//              +0x1E, which the layout does not read).
// Not interpolated (the module writes them, the engine draws them): the caster's descent (entity
// +0x1E, 0x40 per tick until counter 0x54: the entity-position held API is not integrated, the
// engine draws Elvoret at its real-tick position) and the ambient colour 0xB8B9A8 (ticks 0..0x20).
// No camera of its own (the engine camera animation 0x14D8BAC). No draw-only mode. The module packet
// arena alternates per tick: the held packets start where the tick's first draw started.

namespace ff8fx
{
namespace elvoret017
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PARTICLE = 1, REC_PLAY = 2 };

	struct Rec
	{
		uint32_t kind;
		ParticleNode node;        // particle: as drawn
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[0x90];             // 0x80 particles + the layout
	static uint8_t g_held_packets[0x10000]; // module arena (0x10000 per tick)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0x1B9C)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
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

	static void held_note_particle(const ParticleNode *p)
	{
		Rec *r = NewRec(REC_PARTICLE);
		if (!r) return;
		r->node = *p;
	}

	static void held_note_play(const MainNode *m, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)m->layout;
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		ParticleNode m = r.node;
		if (m.counter + 1 < 0xD)
		{
			ParticleNode next = m;
			ParticleStep(&next);
			for (int c = 0; c < 3; c++) m.pos[c] = lerp16(m.pos[c], next.pos[c], num, den);
		}
		ParticleDraw(&m);
	}

	// held-frame entity (fx_held.h HeldEntity, drawn by the frame gate): Elvoret, whose +0x1E the main task raises
	// by 0x40 a tick (counters 0..0x54): continuous, listed while the module runs
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (max < 1 || !HeldReady() || !Caster()) return 0;
		held_entity_init(out[0], Caster());
		return 1;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PARTICLE: ParticleHeld(r, num, den); break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				break;
			}
			}
		}
	}
}

	static void register_mag017_held()
	{
		mark_held(elvoret017::ORIG_ParticleTask);
		mark_held(elvoret017::ORIG_MainTask);
		register_module_held(17, elvoret017::HeldReady, elvoret017::HeldFrame);
		register_module_entity(17, elvoret017::HeldEntities);
	}
}
