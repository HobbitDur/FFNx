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

// 30 fps part of mag195_canister_shot.cpp (Irvine's Canister Shot): the draw log of the real tick and the held-frame drawing
// (included at the end of mag195_canister_shot.cpp; shared pieces in fx_held.h / mag_common_held.h). Same design as Normal
// Shot 188 / Flame Shot 194 (mag188_normal_shot_held.inc, mag194_flame_shot_held.inc), with Canister Shot's parts.
//
// Every drawing task draws THEN updates (the steam / bone smoke / debris tasks spawn their new particles BEFORE they draw). A
// real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the Field_Alloc
// scratch it started from (for the particle tasks from the scratch base, under their first allocation); a held frame at tick +
// num / den replays them in the same order with the vanilla draw code, each at its in-between state (logic stays 15 Hz), under
// the LIVE camera (the held frame's view in 0x1D97778):
//   Ground flash / muzzle cone / muzzle ring / impact flash / impact cone - scale num / den of the way to scale + step; the roll /
//               pitch num / den of the way to roll + spin (cones, ring); the fade num / den of the way where both ticks fade (the
//               switches faded <-> unfaded on the real frame); the last tick holds below 1.
//   Smoke / impact / impact smoke - flipbook frames switch on the real frame (frame = counter); an ended sequence holds below 1.
//   Steam     - each particle's point num / den of the way to the next tick's (it rises); its frame on the real frame.
//   Bone smoke - each particle's size num / den of the way to the next tick's (size + size / 16); its frame on the real frame.
//   Debris    - each piece's point and both angles num / den of the way to the next tick's, its fade num / den of the way where
//               both ticks fade; one that dies holds below 1.
//   Steam / bone smoke / debris at 1: exactly the particles in the array for the task's id (the ones they spawn at the START of
//               the next tick, from rand, appear on the real frame: by design).
//   Ghosts    - the afterimages of his weapon, as Normal Shot's (the newest ghost of N + 1 at his weapon's next pose).
// A draw-only (paused) tick holds everything (its next tick draws the same state) except the newest ghost (as 188). Nothing here
// writes game state: the module globals, the packet cursor (a private buffer holding the real tick's packets under the replay),
// the frame arena, the Field_Alloc scratch, the shadow-rotation scratch, the vertex buffer (0x1D98B3C), the render scratch words,
// the GTE registers, his weapon's bone matrices, his colour word and world matrix are put back. The screen flash (the fade tasks)
// is the engine's. No camera word is written by the module: no held camera.

namespace ff8fx
{
namespace canister195
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xE000;   // one packet arena (texture base / + 0xE000)
	static const uint32_t FARENA = 0x10000; // frame arena (the ghosts' RenderGeometry)
	static const uint32_t BSS_LO = 0x226A970, BSS_HI = 0x227146C;
	enum : uint8_t { REC_SMOKE = 1, REC_GFLASH, REC_CONE, REC_RING, REC_STEAM, REC_BSMOKE, REC_IMPACT, REC_ISMOKE, REC_IFLASH, REC_DEBRIS, REC_ICONE, REC_GHOSTS };

	struct Rec
	{
		uint8_t kind;
		bool paused;          // a draw-only tick
		bool last;            // smoke / impact / impact smoke: its sequence ended on this frame
		Node node;            // as drawn
		int32_t alive;        // steam / bone smoke / debris: particles alive after the update (-1: no update)
		int first, count;     // particle records
		int ghost;            // ghosts: index in g_ghosts
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};
	struct PartRec
	{
		uint8_t *slot;        // the array entry
		uint8_t drawn[0x20];  // as drawn (a Particle or a Debris)
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

	// the scratch the task allocated before its seam (the particle tasks: their light / angle scratch), replayed from that base
	static uint32_t PreAlloc(uint8_t kind) { return (kind == REC_STEAM || kind == REC_BSMOKE) ? 0x48u : kind == REC_DEBRIS ? 0x68u : 0u; }

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
		memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4) - PreAlloc(kind), SCRATCH);
		return &r;
	}

	static void held_note_node(uint32_t fn, const Node *n)
	{
		g_cur_parts = -1;
		const uint8_t kind = fn == ORIG_SmokeTask ? REC_SMOKE : fn == ORIG_GroundFlashTask ? REC_GFLASH : fn == ORIG_MuzzleConeTask ? REC_CONE :
			fn == ORIG_MuzzleRingTask ? REC_RING : fn == ORIG_SteamTask ? REC_STEAM : fn == ORIG_BoneSmokeTask ? REC_BSMOKE :
			fn == ORIG_ImpactTask ? REC_IMPACT : fn == ORIG_ImpactSmokeTask ? REC_ISMOKE : fn == ORIG_ImpactFlashTask ? REC_IFLASH :
			fn == ORIG_DebrisTask ? REC_DEBRIS : fn == ORIG_ImpactConeTask ? REC_ICONE : 0;
		if (!kind) return;
		Rec *r = NewRec(kind);
		if (!r) return;
		r->node = *n;
		if (kind == REC_STEAM || kind == REC_BSMOKE || kind == REC_DEBRIS) g_cur_parts = (int)(r - g_recs);
	}

	static void held_note_last(bool last)
	{
		if (g_in_held || g_log_tick != g_real_tick || !g_nrec) return;
		Rec &r = g_recs[g_nrec - 1];
		if (r.kind == REC_SMOKE || r.kind == REC_IMPACT || r.kind == REC_ISMOKE) r.last = last;
	}

	static void NotePart(const void *e, uint32_t size)
	{
		if (g_in_held || g_cur_parts < 0 || g_npart >= (int)(sizeof(g_parts) / sizeof(g_parts[0]))) return;
		PartRec &s = g_parts[g_npart++];
		s.slot = (uint8_t *)e;
		memset(s.drawn, 0, sizeof(s.drawn));
		memcpy(s.drawn, e, size);
		s.killed = false;
		g_recs[g_cur_parts].count++;
	}
	static void NotePartNext(const void *e)
	{
		if (g_in_held || g_cur_parts < 0 || !g_recs[g_cur_parts].count) return;
		PartRec &s = g_parts[g_npart - 1];
		if (s.slot == (const uint8_t *)e) s.killed = true;
	}
	static void held_note_part(const Particle *e) { NotePart(e, sizeof(Particle)); }
	static void held_note_part_next(const Particle *e) { NotePartNext(e); }
	static void held_note_debris(const Debris *e) { NotePart(e, sizeof(Debris)); }
	static void held_note_debris_next(const Debris *e) { NotePartNext(e); }

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

	// ---- the growing models: scale + step (step - step / step_div, none for the ground flash), roll / pitch + spin (spin -
	// spin / 10; cones and ring), faded per counter; `ticks` ticks
	struct Grow { int32_t ticks, spin, step_div; bool (*faded)(int32_t); int32_t (*fade)(int32_t); };
	static bool FadedFrom2(int32_t c) { return c >= 2; }
	static bool FadedFrom4(int32_t c) { return c >= 4; }
	static bool FadedFrom6(int32_t c) { return c >= 6; }
	static bool FadedAlways(int32_t c) { return c >= 0; }
	static const Grow GROW_GFLASH = { 7, 0, 0, GroundFlashFaded, GroundFlashFade }, GROW_CONE = { 10, 1, 5, FadedFrom2, ConeFade },
		GROW_RING = { 14, 1, 8, FadedFrom6, RingFade }, GROW_IFLASH = { 12, 0, 4, FadedFrom4, ImpactFlashFade },
		GROW_ICONE = { 10, 1, 5, FadedAlways, ImpactConeFade };
	static const Grow &GrowOf(uint8_t kind)
	{
		return kind == REC_GFLASH ? GROW_GFLASH : kind == REC_CONE ? GROW_CONE : kind == REC_RING ? GROW_RING : kind == REC_IFLASH ? GROW_IFLASH : GROW_ICONE;
	}
	static void GrowDraw(uint8_t kind, const Node *n, bool faded, int32_t fade)
	{
		switch (kind)
		{
		case REC_GFLASH: GroundFlashDraw(n, faded, fade); break;
		case REC_CONE: ConeDraw(n, faded, fade); break;
		case REC_RING: RingDraw(n, faded, fade); break;
		case REC_IFLASH: ImpactModelDraw(n, 0, MODEL_ImpactFlash, faded, fade); break;
		default: ImpactModelDraw(n, n->f18, MODEL_Cone, faded, fade); break;
		}
	}
	static void HeldGrow(const Rec &r, int num, int den, bool at_next)
	{
		const Grow &gr = GrowOf(r.kind);
		const bool next = r.paused || r.node.counter + 1 < gr.ticks;
		if (at_next && !next) return;
		Node n = r.node;
		bool faded = gr.faded(n.counter);
		int32_t fade = gr.fade(n.counter);
		if (!r.paused && next)
		{
			Node x = n;
			if (gr.spin)
			{
				const int16_t spin = x.f1A;
				x.f18 = (int16_t)(x.f18 + spin);
				x.f1A = (int16_t)(spin - (int16_t)(spin / 10));
			}
			if (gr.step_div)
			{
				const int16_t step = x.f1E;
				x.f1C = (int16_t)(x.f1C + step);
				x.f1E = (int16_t)(step - (int16_t)(step / gr.step_div));
			}
			x.counter++;
			if (at_next)
			{
				n = x;
				faded = gr.faded(n.counter);
				fade = gr.fade(n.counter);
			}
			else
			{
				n.f1C = lerp16i(n.f1C, x.f1C, num, den);
				if (gr.spin) n.f18 = lerp16i(n.f18, x.f18, num, den);
				if (faded && gr.faded(x.counter)) fade = lerp_i(gr.fade(n.counter), gr.fade(x.counter), num, den);
			}
		}
		GrowDraw(r.kind, &n, faded, fade);
	}

	// ---- smoke / impact / impact smoke (flipbook frame = counter)
	static void HeldSeq(const Rec &r, bool at_next)
	{
		const bool next = r.paused || !r.last;
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		if (r.kind == REC_SMOKE) SmokeDraw(&n);
		else if (r.kind == REC_IMPACT) ImpactDraw(&n);
		else ImpactSmokeDraw(&n);
	}

	// ---- steam / bone smoke / debris particles
	static void PartsBegin(uint8_t kind, uint8_t **s, uint8_t **h)
	{
		if (kind == REC_DEBRIS)
		{
			*s = (uint8_t *)FieldAlloc(0x68);
			*h = (uint8_t *)FieldAlloc(0x58);
			DebrisBegin(*s, *h);
		}
		else
		{
			*s = (uint8_t *)FieldAlloc(0x48);
			*h = (uint8_t *)FieldAlloc(0xB4);
			*(int16_t *)(*h + 0x24) = kind == REC_BSMOKE ? 8 : 0;
			PartLight(*s);
			*(uint32_t *)*h = kind == REC_BSMOKE ? SEQ_BoneSmoke : SEQ_Steam;
		}
	}
	static void PartOne(uint8_t kind, const uint8_t *e, bool faded, int32_t fade, uint8_t *s, uint8_t *h)
	{
		if (kind == REC_DEBRIS) DebrisDraw((const Debris *)e, s, h, faded, fade);
		else PartDraw((const Particle *)e, s, h);
	}
	static void PartsEnd(uint8_t kind)
	{
		if (kind == REC_DEBRIS)
		{
			FieldFree(0x58);
			FieldFree(0x68);
		}
		else
		{
			FieldFree(0xB4);
			FieldFree(0x48);
		}
	}
	static void HeldParts(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = r.paused || r.alive != 0;
		if (at_next && !next) return;
		uint8_t *s = nullptr, *h = nullptr;
		PartsBegin(r.kind, &s, &h);
		const bool debris = r.kind == REC_DEBRIS;
		if (at_next && !r.paused)
		{
			// exactly the particles in the array for the task's id, in order
			if (debris)
			{
				Debris *arr = DebrisArray(r.node.f0E);
				for (Debris *e = arr; e < arr + DEBRIS_N; e++)
					if (e->id == r.node.f20) PartOne(r.kind, (const uint8_t *)e, e->counter >= 6, DebrisFade(e->counter), s, h);
			}
			else
			{
				Particle *arr = PartArray(r.node.f0E);
				const int32_t id = r.node.pos[3] + 1;
				for (Particle *e = arr; e < arr + PARTS_N; e++)
					if (e->id == id) PartOne(r.kind, (const uint8_t *)e, false, 0, s, h);
			}
		}
		else
		{
			for (int i = r.first; i < r.first + r.count; i++)
			{
				const PartRec &p = g_parts[i];
				uint8_t x[0x20];
				memcpy(x, p.drawn, sizeof(x));
				bool faded = false;
				int32_t fade = 0;
				const bool lerp = !r.paused && !p.killed && *(const int32_t *)p.slot == *(const int32_t *)p.drawn;
				if (debris)
				{
					Debris *d = (Debris *)x;
					const Debris *d1 = (const Debris *)p.slot;
					faded = d->counter >= 6;
					fade = DebrisFade(d->counter);
					if (lerp)
					{
						for (int c = 0; c < 3; c++) d->pos[c] = lerp16i(d->pos[c], d1->pos[c], num, den);
						d->ax = lerp16i(d->ax, d1->ax, num, den);
						d->az = lerp16i(d->az, d1->az, num, den);
						if (faded && d1->counter >= 6) fade = lerp_i(DebrisFade(d->counter), DebrisFade(d1->counter), num, den);
					}
				}
				else if (lerp)
				{
					Particle *e = (Particle *)x;
					const Particle *e1 = (const Particle *)p.slot;
					if (r.kind == REC_STEAM)
						for (int c = 0; c < 3; c++) e->pos[c] = lerp16i(e->pos[c], e1->pos[c], num, den);
					else e->size = lerp16i(e->size, e1->size, num, den);
				}
				PartOne(r.kind, x, faded, fade, s, h);
			}
		}
		PartsEnd(r.kind);
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
				case REC_GFLASH:
				case REC_CONE:
				case REC_RING:
				case REC_IFLASH:
				case REC_ICONE: HeldGrow(r, num, den, at_next); break;
				case REC_SMOKE:
				case REC_IMPACT:
				case REC_ISMOKE: HeldSeq(r, at_next); break;
				case REC_STEAM:
				case REC_BSMOKE:
				case REC_DEBRIS: HeldParts(r, num, den, at_next); break;
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

	static void register_mag195_held()
	{
		using namespace canister195;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_StartTask);
		mark_held(ORIG_ShotTask);
		mark_held(ORIG_EndTask);
		mark_held(ORIG_FadeOutTask);
		mark_held(ORIG_FadeInTask);
		mark_held(ORIG_AfterimageTask);
		mark_held(ORIG_SmokeTask);
		mark_held(ORIG_GroundFlashTask);
		mark_held(ORIG_MuzzleConeTask);
		mark_held(ORIG_MuzzleRingTask);
		mark_held(ORIG_SteamTask);
		mark_held(ORIG_BoneSmokeTask);
		mark_held(ORIG_ImpactTask);
		mark_held(ORIG_ImpactSmokeTask);
		mark_held(ORIG_ImpactFlashTask);
		mark_held(ORIG_DebrisTask);
		mark_held(ORIG_ImpactConeTask);
		register_module_held(195, HeldReady, HeldFrame);
	}
}
