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

// 30 fps part of mag194_flame_shot.cpp (Irvine's Flame Shot): the draw log of the real tick and the held-frame drawing
// (included at the end of mag194_flame_shot.cpp; shared pieces in fx_held.h / mag_common_held.h). Same design as Normal
// Shot 188 (mag188_normal_shot_held.inc), with Flame Shot's parts.
//
// Every drawing task draws THEN updates (the ember quads read their point before the update; the fire spawns before it draws). A
// real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the Field_Alloc
// scratch it started from; a held frame at tick + num / den replays them in the same order with the vanilla draw code, each at its
// in-between state (logic stays 15 Hz), under the LIVE camera (the held frame's view in 0x1D97778):
//   Flash / spin / burst / ring / pillar - scale num / den of the way to scale + step; the roll / yaw num / den of the way to
//               roll + spin (spin, ring, pillar); the pillar's v scroll num / den of the way to the next counter's; the fade num /
//               den of the way where both ticks fade (the switch to faded on the real frame); the last tick holds.
//   Smoke / ground smoke - flipbook frames switch on the real frame (frame = counter); an ended sequence holds below 1.
//   Head      - its point num / den of the way to the flight point the next tick draws (point + speed); past the last point it
//               holds below 1 and is not drawn at 1.
//   Trail     - each particle's size num / den of the way to the next tick's (size + size / 32); its frame on the real frame.
//   Embers    - each quad's point num / den of the way to the next tick's (point + velocity); its colour (frame) on the real
//               frame; one that dies holds below 1.
//   Fire      - nothing moves (the frame switches on the real frame); one that dies holds below 1.
//   Trail / embers / fire at 1: exactly the particles in the array for the task's id (the trail / ember particles born at the
//               end of this tick too; the fire's are born at the START of the next tick, from rand: they appear on the real frame).
//   Ghosts    - the afterimages of his weapon, as Normal Shot's (the newest ghost of N + 1 at his weapon's next pose).
// A draw-only (paused) tick holds everything (its next tick draws the same state) except the newest ghost (as 188). Nothing here
// writes game state: the module globals, the packet cursor (a private buffer holding the real tick's packets under the replay),
// the frame arena, the Field_Alloc scratch, the shadow-rotation scratch, the vertex buffer (0x1D98B3C), the render scratch words,
// the GTE registers, his weapon's bone matrices, his colour word and world matrix are put back. The screen flash (the fade tasks)
// is the engine's. No camera word is written by the module: no held camera.

namespace ff8fx
{
namespace flame194
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xC000;   // one packet arena (texture base / + 0xC000)
	static const uint32_t FARENA = 0x10000; // frame arena (the ghosts' RenderGeometry)
	static const uint32_t BSS_LO = 0x2271470, BSS_HI = 0x2276778;
	enum : uint8_t { REC_FLASH = 1, REC_SPIN, REC_SMOKE, REC_GSMOKE, REC_HEAD, REC_TRAIL, REC_EMBER, REC_BURST, REC_RING, REC_PILLAR, REC_FIRE, REC_GHOSTS };

	struct Rec
	{
		uint8_t kind;
		bool paused;          // a draw-only tick
		bool last;            // smoke: its sequence ended on this frame
		Node node;            // as drawn
		int16_t pt[4];        // head: the flight point drawn
		int32_t alive;        // trail / fire: particles alive after the update; embers: quads drawn (-1: no update)
		int first, count;     // particle records
		int ghost;            // ghosts: index in g_ghosts
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};
	struct PartRec
	{
		Particle *slot;       // the array entry
		Particle drawn;
		bool killed;          // its update freed it
	};
	struct GhostRec
	{
		int32_t counter, slot, count, end;
		uint32_t colour;      // his colour word
		Mat4x3 world;         // his world matrix as drawn
		Ghost ring[5];        // the ghost slots as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static bool g_in_held = false;
	static int g_nrec = 0, g_nscratch = 0, g_npart = 0, g_nghosts = 0, g_cur_parts = -1;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static Rec g_recs[0x400];
	static uint8_t g_scratch[0x400][SCRATCH];
	static PartRec g_parts[0x1000];
	static GhostRec g_ghosts[4];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_skel_save[0x40 + 0x30 * 256];
	static uint8_t g_ghost_bones[0x80];
	// the next tick's newest snapshot (his weapon at its next pose), made at the tick's first held frame
	static bool g_next_snap_ok = false;
	static uint8_t g_next_snap[0x30 * 256];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		if (g_in_held) return;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
		g_npart = 0;
		g_nghosts = 0;
		g_cur_parts = -1;
		g_packets = 0;
		g_frame_start = 0;
	}

	static Rec *NewRec(uint8_t kind)
	{
		if (g_in_held || g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.paused = (UpdateFlags() & 0x201) != 0;
		r.last = false;
		r.alive = -1;
		r.first = g_npart;
		r.count = 0;
		r.ghost = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r.scratch = g_nscratch++;
		memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_node(uint32_t fn, const Node *n)
	{
		g_cur_parts = -1;
		const uint8_t kind = fn == ORIG_FlashTask ? REC_FLASH : fn == ORIG_SpinTask ? REC_SPIN : fn == ORIG_SmokeTask ? REC_SMOKE :
			fn == ORIG_GroundSmokeTask ? REC_GSMOKE : fn == ORIG_HeadTask ? REC_HEAD : fn == ORIG_TrailTask ? REC_TRAIL :
			fn == ORIG_EmberTask ? REC_EMBER : fn == ORIG_BurstTask ? REC_BURST : fn == ORIG_RingTask ? REC_RING :
			fn == ORIG_PillarTask ? REC_PILLAR : fn == ORIG_FireTask ? REC_FIRE : 0;
		if (!kind) return;
		Rec *r = NewRec(kind);
		if (!r) return;
		r->node = *n;
		if (kind == REC_HEAD) memcpy(r->pt, FlightRec(n->f0E)->pts[n->f18], 8);
		if (kind == REC_TRAIL || kind == REC_EMBER || kind == REC_FIRE) g_cur_parts = (int)(r - g_recs);
	}

	static void held_note_last(bool last)
	{
		if (g_in_held || g_log_tick != g_real_tick || !g_nrec) return;
		Rec &r = g_recs[g_nrec - 1];
		if (r.kind == REC_SMOKE || r.kind == REC_GSMOKE) r.last = last;
	}

	static void held_note_part(const Particle *e)
	{
		if (g_in_held || g_cur_parts < 0 || g_npart >= (int)(sizeof(g_parts) / sizeof(g_parts[0]))) return;
		PartRec &s = g_parts[g_npart++];
		s.slot = (Particle *)e;
		s.drawn = *e;
		s.killed = false;
		g_recs[g_cur_parts].count++;
	}

	static void held_note_part_next(const Particle *e)
	{
		if (g_in_held || g_cur_parts < 0 || !g_recs[g_cur_parts].count) return;
		PartRec &s = g_parts[g_npart - 1];
		if (s.slot == e) s.killed = true;
	}

	static void held_note_alive(int32_t alive)
	{
		if (g_in_held || g_cur_parts < 0) return;
		Rec &r = g_recs[g_cur_parts];
		if (!r.paused) r.alive = alive;
		g_cur_parts = -1;
	}

	static void held_note_ghosts(int32_t counter, int32_t slot, int32_t count, uint32_t colour)
	{
		g_cur_parts = -1;
		if (g_nghosts >= (int)(sizeof(g_ghosts) / sizeof(g_ghosts[0]))) return;
		Rec *r = NewRec(REC_GHOSTS);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ghost = g_nghosts++;
		GhostRec &g = g_ghosts[r->ghost];
		g.counter = counter;
		g.slot = slot;
		g.count = count;
		g.end = EndTick();
		g.colour = colour;
		g.world = *(const Mat4x3 *)(Caster() + 0x40);
		memcpy(g.ring, (const void *)GHOSTS, sizeof(g.ring));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static int16_t lerp16i(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + ((int32_t)b - a) * num / den); }
	static void LerpMat(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out->m[i][j] = lerp16i(a.m[i][j], b.m[i][j], num, den);
		out->pad = a.pad;
		for (int k = 0; k < 3; k++) out->t[k] = lerp_i(a.t[k], b.t[k], num, den);
	}

	// the skeleton block of an animation header (count byte, bone records of 0x30 from +0x10)
	static uint8_t *Skeleton(uint8_t *anim, uint32_t *bytes)
	{
		if (!anim) return nullptr;
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_skel_save)) n = sizeof(g_skel_save);
		*bytes = n;
		return sk;
	}

	// his weapon at its next pose (pose_midpoint at 1): the snapshot the next tick's AfterimageTask takes; the bone matrices,
	// the GTE and the Field_Alloc scratch put back
	static void PredictNextSnapshot()
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_next_snap_ok = false;
		bool any = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_GHOSTS) any = true;
		if (!any) return;
		uint8_t *w = *(uint8_t **)(Caster() + 0x78);
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(w, &nb);
		if (!sk) return;
		static uint8_t fas[0x1000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t *fa = var<uint8_t *>(0x1D999C4);
		if (fa) memcpy(fas, fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(g_skel_save, sk, nb);
		pose_midpoint(w, w + 0xC, 1, 1);
		memset(g_next_snap, 0, sizeof(g_next_snap));
		BoneSnapshot(w, g_next_snap);
		memcpy(sk, g_skel_save, nb);
		var<uint8_t *>(0x1D999C4) = fa;
		if (fa) memcpy(fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		g_next_snap_ok = true;
	}

	// ---- the growing models: scale + step (step - step / div), roll / yaw + spin (spin - spin >> 4 or spin / 12), fade from
	// counter `fade_from`; `ticks` ticks
	struct Grow { int32_t div, ticks, fade_from, spin; int32_t (*fade)(int32_t); };
	static const Grow GROW_FLASH = { 4, 6, 2, 0, FlashFade }, GROW_SPIN = { 4, 10, 2, 1, SpinFade }, GROW_BURST = { 6, 7, 1, 0, BurstFade },
		GROW_RING = { 4, 14, 2, 2, RingFade }, GROW_PILLAR = { 8, 16, 8, 2, PillarFade };
	static const Grow &GrowOf(uint8_t kind)
	{
		return kind == REC_FLASH ? GROW_FLASH : kind == REC_SPIN ? GROW_SPIN : kind == REC_BURST ? GROW_BURST : kind == REC_RING ? GROW_RING : GROW_PILLAR;
	}
	static void GrowDraw(uint8_t kind, const Node *n, int32_t fade, int32_t scroll)
	{
		switch (kind)
		{
		case REC_FLASH: MuzzleModel(n, MODEL_Flash, n->counter >= 2, fade); break;
		case REC_SPIN: MuzzleModel(n, MODEL_Spin, n->counter >= 2, fade); break;
		case REC_BURST: BurstDraw(n, n->counter >= 1, fade); break;
		case REC_RING: RingDraw(n, n->counter >= 2, fade); break;
		default: PillarDraw(n, scroll, n->counter >= 8, fade); break;
		}
	}
	static void HeldGrow(const Rec &r, int num, int den, bool at_next)
	{
		const Grow &gr = GrowOf(r.kind);
		const bool next = r.paused || r.node.counter + 1 < gr.ticks;
		if (at_next && !next) return;
		Node n = r.node;
		int32_t fade = gr.fade(n.counter);
		int32_t scroll = mul32(n.counter, -12);
		if (!r.paused && next)
		{
			Node x = n;
			if (gr.spin)
			{
				const int16_t spin = x.f1A;
				x.f18 = (int16_t)(x.f18 + spin);
				x.f1A = (int16_t)(spin - (int16_t)(gr.spin == 1 ? (spin >> 4) : (spin / 12)));
			}
			const int16_t step = x.f1E;
			x.f1C = (int16_t)(x.f1C + step);
			x.f1E = (int16_t)(step - (int16_t)(step / gr.div));
			x.counter++;
			if (at_next)
			{
				n = x;
				fade = gr.fade(n.counter);
				scroll = mul32(n.counter, -12);
			}
			else
			{
				n.f1C = lerp16i(n.f1C, x.f1C, num, den);
				if (gr.spin) n.f18 = lerp16i(n.f18, x.f18, num, den);
				scroll = lerp_i(scroll, mul32(x.counter, -12), num, den);
				if (n.counter >= gr.fade_from) fade = lerp_i(gr.fade(n.counter), gr.fade(x.counter), num, den);
			}
		}
		GrowDraw(r.kind, &n, fade, scroll);
	}

	// ---- smoke (flipbook frame = counter)
	static void HeldSmoke(const Rec &r, bool at_next)
	{
		const bool next = r.paused || !r.last;
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		if (r.kind == REC_SMOKE) SmokeDraw(&n);
		else GroundSmokeDraw(&n);
	}

	// ---- the flight head: its point toward the next tick's flight point
	static void HeldHead(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t f1 = r.node.f18 + r.node.f1A;
		const bool next = r.paused || f1 < FLIGHT_POINTS;
		if (at_next && !next) return;
		int16_t pt[4];
		memcpy(pt, r.pt, 8);
		if (!r.paused && next)
		{
			const int16_t *p1 = FlightRec(r.node.f0E)->pts[f1];
			for (int c = 0; c < 3; c++) pt[c] = at_next ? p1[c] : lerp16i(r.pt[c], p1[c], num, den);
		}
		HeadDraw(pt, r.node.f1C);
	}

	// ---- trail / embers / fire particles
	static void PartsBegin(uint8_t kind, uint8_t **s, uint8_t **q, uint8_t **h, uint8_t **pk)
	{
		if (kind == REC_EMBER)
		{
			*s = (uint8_t *)FieldAlloc(0x50);
			*q = (uint8_t *)FieldAlloc(0x44);
			EmberSetup(*q);
			EmberLight(*s);
			*pk = (uint8_t *)PacketCursor();
			*(uint32_t *)*q = RenderOT();
			*(int32_t *)(*q + 4) = 4;
		}
		else if (kind == REC_FIRE)
		{
			*s = (uint8_t *)FieldAlloc(0x48);
			*h = (uint8_t *)FieldAlloc(0xB4);
			*(int16_t *)(*h + 0x24) = 0;
			FireLight(*s);
			*(uint32_t *)*h = SEQ_Flame;
		}
		else
		{
			*h = (uint8_t *)FieldAlloc(0xB4);
			*(int16_t *)(*h + 0x24) = 0;
			*(uint32_t *)*h = SEQ_Flame;
		}
	}
	static void PartDraw(uint8_t kind, const Particle *e, uint8_t *s, uint8_t *q, uint8_t *h, uint8_t **pk)
	{
		if (kind == REC_EMBER)
		{
			EmberLoad(e->pos);
			if (EmberQuad(q, e->size, EMBER_COLOURS[e->frame], *pk)) *pk += 0x28;
		}
		else if (kind == REC_FIRE) FireDraw(e, s, h);
		else TrailDraw(e, h);
	}
	static void PartsEnd(uint8_t kind, uint8_t *pk)
	{
		if (kind == REC_EMBER)
		{
			FieldFree(0x44);
			PacketCursor() = (uint32_t)pk;
			FieldFree(0x50);
		}
		else if (kind == REC_FIRE)
		{
			FieldFree(0xB4);
			FieldFree(0x48);
		}
		else FieldFree(0xB4);
	}
	static void HeldParts(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t c1 = r.node.counter + 1;
		const bool ends = r.kind == REC_FIRE ? r.alive == 0 : r.kind == REC_EMBER ? (c1 >= 8 && r.alive == 0) : (c1 >= 4 && r.alive == 0);
		const bool next = r.paused || !ends;
		if (at_next && !next) return;
		uint8_t *s = nullptr, *q = nullptr, *h = nullptr, *pk = nullptr;
		PartsBegin(r.kind, &s, &q, &h, &pk);
		if (at_next && !r.paused)
		{
			// exactly the particles in the array for the task's id, in order
			Particle *arr = PartArray(r.node.f0E);
			const int32_t id = r.node.pos[3] + 1;
			for (Particle *e = arr; e < arr + PARTS_N; e++)
				if (e->id == id) PartDraw(r.kind, e, s, q, h, &pk);
		}
		else
		{
			for (int i = r.first; i < r.first + r.count; i++)
			{
				const PartRec &p = g_parts[i];
				Particle x = p.drawn;
				if (!r.paused && !p.killed && p.slot->id == p.drawn.id)
				{
					if (r.kind == REC_TRAIL) x.size = lerp16i(p.drawn.size, p.slot->size, num, den);
					else if (r.kind == REC_EMBER)
						for (int c = 0; c < 3; c++) x.pos[c] = lerp16i(p.drawn.pos[c], p.slot->pos[c], num, den);
				}
				PartDraw(r.kind, &x, s, q, h, &pk);
			}
		}
		PartsEnd(r.kind, pk);
	}

	// ---- the weapon's ghosts
	// the ghost list of a tick: slots from `start` down while used
	static int GhostList(const Ghost *ring, int32_t start, int32_t count, int32_t *slots)
	{
		int n = 0;
		int32_t s = start;
		for (int32_t k = 0; k < count && k < 5; k++)
		{
			if (!ring[s].used) break;
			slots[n++] = s;
			if (--s < 0) s = 4;
		}
		return n;
	}
	static void HeldGhosts(const Rec &r, int num, int den, bool at_next)
	{
		const GhostRec &g = g_ghosts[r.ghost];
		uint8_t *e = Caster();
		uint8_t *w = *(uint8_t **)(e + 0x78);
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(w, &nb);
		if (!sk) return;
		int32_t s0[5], s1[5];
		const int n0 = GhostList(g.ring, g.slot, g.count, s0);
		// the next tick: its snapshot slot (after a draw-only tick the counter has not moved: the same slot, retaken at his next
		// pose: the animations do not step on draw-only ticks, 0x502AB0), its count (the end tick as it is now), whether the task
		// still runs (it never ends on a draw-only tick)
		Ghost ring1[5];
		memcpy(ring1, g.ring, sizeof(ring1));
		const int32_t c1 = r.paused ? g.counter : g.counter + 1, snap1 = c1 % 5;
		ring1[snap1].used = 1;
		if (g_next_snap_ok) memcpy(ring1[snap1].bones, g_next_snap, sizeof(ring1[snap1].bones));
		const int32_t end1 = EndTick();
		const bool next = (r.paused || c1 < g.end + 5) && g_next_snap_ok;
		const bool hold = r.paused && !g_next_snap_ok; // (no prediction: a draw-only tick holds)
		const int n1 = next ? GhostList(ring1, c1 - 1 >= 0 ? (c1 - 1) % 5 : 0, c1 >= end1 ? end1 - c1 + 5 : 5, s1) : 0;
		if (at_next && !hold && !next) return;
		memcpy(g_skel_save, sk, nb);
		const uint32_t colour = *(const uint32_t *)(e + 0x28);
		const Mat4x3 keep = *(const Mat4x3 *)(e + 0x40);
		LerpMat((Mat4x3 *)(e + 0x40), g.world, keep, hold ? 0 : num, den);
		const int nd = (at_next && !hold) ? n1 : n0;
		const int nbones = sk[0];
		for (int k = 0; k < nd; k++)
		{
			if (at_next && !hold) memcpy(g_ghost_bones, ring1[s1[k]].bones, sizeof(g_ghost_bones));
			else
			{
				memcpy(g_ghost_bones, g.ring[s0[k]].bones, sizeof(g_ghost_bones));
				if (!hold && k < n1)
				{
					const uint8_t *b = ring1[s1[k]].bones;
					for (int j = 0; j < nbones && (j + 1) * 0x20 <= (int)sizeof(g_ghost_bones); j++)
						LerpMat((Mat4x3 *)(g_ghost_bones + j * 0x20), *(const Mat4x3 *)(g.ring[s0[k]].bones + j * 0x20), *(const Mat4x3 *)(b + j * 0x20), num, den);
				}
			}
			BoneRestore(w, g_ghost_bones);
			*(uint32_t *)(e + 0x28) = GHOST_COLOURS[k];
			GhostDraw(e);
		}
		memcpy(sk, g_skel_save, nb);
		*(uint32_t *)(e + 0x28) = colour;
		*(Mat4x3 *)(e + 0x40) = keep;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		PredictNextSnapshot();
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8], vbuf[0x8000];
		const uint32_t vb = var<uint32_t>(0x1D98B3C);
		const uint32_t frame_saved = FramePackets();
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		if (vb) memcpy(vbuf, (const void *)vb, sizeof(vbuf));
		g_in_held = true;
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			if (g_frame_start && frame_saved > g_frame_start)
			{
				const uint32_t used = frame_saved - g_frame_start;
				memcpy(g_held_frame, (const void *)g_frame_start, used < FARENA ? used : FARENA);
			}
			FramePackets() = (uint32_t)g_held_frame;
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				switch (r.kind)
				{
				case REC_FLASH:
				case REC_SPIN:
				case REC_BURST:
				case REC_RING:
				case REC_PILLAR: HeldGrow(r, num, den, at_next); break;
				case REC_SMOKE:
				case REC_GSMOKE: HeldSmoke(r, at_next); break;
				case REC_HEAD: HeldHead(r, num, den, at_next); break;
				case REC_TRAIL:
				case REC_EMBER:
				case REC_FIRE: HeldParts(r, num, den, at_next); break;
				case REC_GHOSTS: HeldGhosts(r, num, den, at_next); break;
				}
			}
		}
		g_in_held = false;
		FramePackets() = frame_saved;
		if (vb) memcpy((void *)vb, vbuf, sizeof(vbuf));
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}
}

	static void register_mag194_held()
	{
		using namespace flame194;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_RestoreTask);
		mark_held(ORIG_StartTask);
		mark_held(ORIG_ShotTask);
		mark_held(ORIG_EndTask);
		mark_held(ORIG_FadeOutTask);
		mark_held(ORIG_FadeInTask);
		mark_held(ORIG_AfterimageTask);
		mark_held(ORIG_FlashTask);
		mark_held(ORIG_SmokeTask);
		mark_held(ORIG_SpinTask);
		mark_held(ORIG_GroundSmokeTask);
		mark_held(ORIG_FlightTask);
		mark_held(ORIG_HeadTask);
		mark_held(ORIG_TrailTask);
		mark_held(ORIG_EmberTask);
		mark_held(ORIG_BurstTask);
		mark_held(ORIG_RingTask);
		mark_held(ORIG_PillarTask);
		mark_held(ORIG_FireTask);
		register_module_held(194, HeldReady, HeldFrame);
	}
}
