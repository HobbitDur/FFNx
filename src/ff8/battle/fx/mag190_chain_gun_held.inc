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

// 30 fps part of mag190_chain_gun.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag190_chain_gun.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick logs each task's draw in queue order: the
// node as drawn, the GTE registers and Field_Alloc scratch it started from, and per particle task
// every particle as drawn (and whether an update of this tick killed it afterwards). Held frame:
//   muzzle    - fade (counter << 11) lerped to the next tick's; a flash that ends holds.
//   impact    - scale (s + step) and fade (counter << 9) lerped; the last frame holds.
//   spark     - scale lerped, fade lerped from counter 1 on (the unfaded -> faded switch at 1
//               holds the unfaded header); the last frame holds.
//   flash     - nothing moves: the same draw (flipbook frames stay 15 Hz).
//   particles - each particle lerped (position and size) to the state the same task will draw on
//               the next tick: the array entry after this tick, plus the updates of the tasks
//               that run BEFORE this one in the queue and also move it (the smoke moves the odd
//               dust groups too); flipbook frames stay 15 Hz; a particle that died (or that an
//               earlier task will kill first: its flipbook ends) holds.
// At fraction 0 a held frame draws exactly the real tick's primitives, at fraction 1 exactly the
// next tick's (new particles and tasks, particles / tasks that end: the held frame keeps its last
// state). Draw-only ticks hold.

namespace ff8fx
{
namespace cg190
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x8000;  // module packet arena per tick
	static const int MAX_RECS = 128, MAX_PARTS = 512;

	struct Rec
	{
		uint32_t fn;
		Node node;          // as drawn
		bool paused;        // draw-only tick
		int first, count;   // particles drawn
		int alive;          // particles still alive after their update
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct PartRec
	{
		Particle *slot;     // array entry
		Particle drawn;
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npart = 0, g_cur_part = -1;
	static uint32_t g_packets = 0;
	static Rec g_recs[MAX_RECS];
	static PartRec g_parts[MAX_PARTS];
	static int g_kill[200];  // per entry (A 0..99, B 100..199): last particle record whose update killed it
	static uint8_t g_held_arena[ARENA];

	static int SlotIndex(const Particle *e) { return e >= PartsA() && e < PartsA() + 100 ? (int)(e - PartsA()) : 100 + (int)(e - PartsB()); }
	static bool IsParticleTask(uint32_t fn) { return fn == ORIG_SmokeTask || fn == ORIG_DustTask || fn == ORIG_SparksTask; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npart = 0;
		g_cur_part = -1;
		g_packets = 0;
		for (int i = 0; i < 200; i++) g_kill[i] = -1;
	}

	static void held_begin(uint32_t fn, const Node *n)
	{
		g_cur_part = -1;
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return;
		if (g_nrec == 0) g_packets = Cursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = *n;
		r.paused = (UpdateFlags() & 0x201) != 0;
		r.first = g_npart;
		r.count = 0;
		r.alive = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_part(const Particle *e)
	{
		g_cur_part = -1;
		Rec *r = g_nrec ? &g_recs[g_nrec - 1] : nullptr;
		if (g_log_tick != g_real_tick || !r || !IsParticleTask(r->fn)) return;
		if (g_npart >= MAX_PARTS) return;
		g_cur_part = g_npart++;
		g_parts[g_cur_part].slot = (Particle *)e;
		g_parts[g_cur_part].drawn = *e;
		r->count++;
	}

	static void held_note_part_next(const Particle *e)
	{
		if (g_cur_part < 0) return;
		if (e->id == 0) g_kill[SlotIndex(e)] = g_cur_part;
		else g_recs[g_nrec - 1].alive++;
		g_cur_part = -1;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a particle task of this tick that runs again on the next one (it ends past its last counter
	// when no particle it drew is left)
	static bool Continues(const Rec &r)
	{
		const int32_t last = r.fn == ORIG_SparksTask ? 0xF : 0x10;
		return !(r.node.counter + 1 >= last && r.alive == 0);
	}

	// does task r update particle e (array entry state)
	static bool Moves(const Rec &r, const Particle *slot, const Particle &e)
	{
		const bool a = slot >= PartsA() && slot < PartsA() + 100;
		if (r.fn == ORIG_SmokeTask) return a && (*(const uint8_t *)&e.id & 1);
		if (r.fn == ORIG_DustTask) return a && e.id == (int32_t)r.node.f0E;
		if (r.fn == ORIG_SparksTask) return !a && e.id == (int32_t)r.node.f0E;
		return false;
	}

	// the flipbook ends with frame f (the sequence player's +0x28: the next frame's offset < 0)
	static bool SeqEnds(uint32_t seq, int16_t frame) { return *(const int16_t *)(seq + 0xA + 2 * ((uint32_t)(uint16_t)frame + 1)) < 0; }

	// what the task of record k draws for particle record i on the next tick; false = a task that
	// runs before it kills the particle first (its flipbook ends)
	static bool PredictParticle(int k, int i, Particle &out)
	{
		const PartRec &p = g_parts[i];
		out = *p.slot;
		for (int j = 0; j < k; j++)
		{
			const Rec &r = g_recs[j];
			if (!IsParticleTask(r.fn) || !Continues(r) || !Moves(r, p.slot, out)) continue;
			if (SeqEnds(r.fn == ORIG_SparksTask ? SEQ_Sparks : SEQ_Smoke, out.frame)) return false;
			out.frame++;
			if (r.fn == ORIG_SmokeTask) SmokeMove(&out);
			else if (r.fn == ORIG_DustTask) DustMove(&out);
			else SparksMove(&out);
		}
		return true;
	}

	static void HeldParticles(int k, int num, int den)
	{
		const Rec &r = g_recs[k];
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (r.fn == ORIG_SparksTask)
		{
			*(uint32_t *)h = SEQ_Sparks;
			*(int16_t *)(h + 0x24) = 1;
		}
		else if (r.fn == ORIG_DustTask)
		{
			*(uint32_t *)h = SEQ_Smoke;
			*(uint32_t *)(h + 0x1C) = 0x404040;
			*(int16_t *)(h + 0x24) = 4;
		}
		else
		{
			*(uint32_t *)h = SEQ_Smoke;
			*(int16_t *)(h + 0x24) = 0;
		}
		for (int i = r.first; i < r.first + r.count; i++)
		{
			const PartRec &p = g_parts[i];
			const int slot = SlotIndex(p.slot);
			Particle e = p.drawn, nx;
			if (!r.paused && g_kill[slot] < i && PredictParticle(k, i, nx))
			{
				if (num >= den) e = nx;
				else
				{
					for (int c = 0; c < 3; c++) e.pos[c] = lerp16(p.drawn.pos[c], nx.pos[c], num, den);
					e.angle = lerp16(p.drawn.angle, nx.angle, num, den);
				}
			}
			if (r.fn == ORIG_SparksTask) *(int32_t *)(h + 8) = e.f16;
			ParticleDraw(h, e.frame, e.pos, e.angle);
		}
		FieldFree(0xB4);
	}

	static void HeldMuzzle(const Rec &r, int num, int den)
	{
		const int32_t c = r.node.counter;
		if (r.paused || c + 1 >= 2) MuzzleDraw(&r.node, c >= 0, MuzzleFade(c));
		else if (num >= den) MuzzleDraw(&r.node, c + 1 >= 0, MuzzleFade(c + 1));
		else MuzzleDraw(&r.node, c >= 0, lerp_i(MuzzleFade(c), MuzzleFade(c + 1), num, den));
	}

	static void HeldImpact(const Rec &r, int num, int den)
	{
		const int32_t c = r.node.counter;
		if (r.paused || c + 1 > 8) { ImpactDraw(&r.node, c >= 0, ImpactFade(c)); return; }
		Node m = r.node;
		const int16_t s1 = (int16_t)(m.f1C + m.f1E);
		if (num >= den)
		{
			m.f1C = s1;
			ImpactDraw(&m, c + 1 >= 0, ImpactFade(c + 1));
			return;
		}
		m.f1C = lerp16(r.node.f1C, s1, num, den);
		ImpactDraw(&m, c >= 0, lerp_i(ImpactFade(c), ImpactFade(c + 1), num, den));
	}

	static void HeldSpark(const Rec &r, int num, int den)
	{
		const int32_t c = r.node.counter;
		if (r.paused || c + 1 >= 5) { SparkDraw(&r.node, c >= 1, SparkFade(c)); return; }
		Node m = r.node;
		const int16_t s1 = (int16_t)(m.f1C + m.f1E);
		if (num >= den)
		{
			m.f1C = s1;
			SparkDraw(&m, c + 1 >= 1, SparkFade(c + 1));
			return;
		}
		m.f1C = lerp16(r.node.f1C, s1, num, den);
		if (c >= 1) SparkDraw(&m, true, lerp_i(SparkFade(c), SparkFade(c + 1), num, den));
		else SparkDraw(&m, false, SparkFade(c));
	}

	static void HeldFlash(const Rec &r, int num, int den)
	{
		const int32_t c = r.node.counter;
		const bool next = !r.paused && c + 1 < 0x10 && num >= den;
		FlashDraw(&r.node, (int16_t)(next ? c + 1 : c));
	}

	static void HeldFrame(int num, int den)
	{
		// packet bytes a draw does not write (pad halves) as the real tick left them; at fraction 1
		// as the next tick will find them (it draws into the other arena)
		HeldScope scope(Cursor(), g_held_arena, ARENA, num >= den ? 0 : g_packets);
		if (num >= den && g_packets) memcpy(g_held_arena, (const void *)(g_packets == ARENA_A ? ARENA_B : ARENA_A), ARENA);
		for (int k = 0; k < g_nrec; k++)
		{
			const Rec &r = g_recs[k];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_MuzzleTask: HeldMuzzle(r, num, den); break;
			case ORIG_ImpactTask: HeldImpact(r, num, den); break;
			case ORIG_SparkTask: HeldSpark(r, num, den); break;
			case ORIG_FlashTask: HeldFlash(r, num, den); break;
			case ORIG_SmokeTask:
			case ORIG_DustTask:
			case ORIG_SparksTask: HeldParticles(k, num, den); break;
			}
		}
	}
}

	static void register_mag190_held()
	{
		mark_held(cg190::ORIG_MuzzleTask);
		mark_held(cg190::ORIG_SmokeTask);
		mark_held(cg190::ORIG_ImpactTask);
		mark_held(cg190::ORIG_FlashTask);
		mark_held(cg190::ORIG_DustTask);
		mark_held(cg190::ORIG_SparkTask);
		mark_held(cg190::ORIG_SparksTask);
		register_module_held(190, cg190::HeldReady, cg190::HeldFrame);
	}
}
