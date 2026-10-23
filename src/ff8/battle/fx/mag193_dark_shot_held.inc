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

// 30 fps part of mag193_dark_shot.cpp (Irvine's Dark Shot): the draw log of the real tick and the held-frame drawing (included
// at the end of mag193_dark_shot.cpp; shared pieces in fx_held.h / mag_common_held.h). Same design as Normal Shot 188 /
// Scatter Shot 192 (mag188_normal_shot_held.inc, mag192_scatter_shot_held.inc), with Dark Shot's parts.
//
// Every drawing task draws THEN updates (the target sparks spawn their new particles BEFORE they draw). A real tick records
// every draw of the module in order (the root's seam starts the log) with the GTE registers and the Field_Alloc scratch it
// started from (the target sparks from the scratch base, under their light scratch); a held frame at tick + num / den replays
// them in the same order with the vanilla draw code, each at its in-between state (logic stays 15 Hz), under the LIVE camera
// (the held frame's view in 0x1D97778):
//   Flash2 / Blast / target model - scale num / den of the way to scale + step; fade num / den of the way where both ticks fade
//               (the switch to faded on the real frame); the last tick holds below 1.
//   Smokes / ground smoke / the impact flash - flipbook frames switch on the real frame (frame = counter); a sequence the next
//               tick does not draw (ended / counter 7) is held below 1, not drawn at 1.
//   Sparks    - each spark's position num / den of the way to the next tick's (position + velocity); its frame on the real
//               frame; one that died holds below 1. At 1 exactly the array's sparks for the impact (the ones born at the END of
//               this tick too).
//   Target sparks - nothing moves (the frame switches on the real frame); one that dies holds below 1; at 1 exactly the
//               array's sparks for the id (the ones they spawn at the START of the next tick, from rand, appear on the real
//               frame: by design).
//   Ghosts    - the afterimages of his weapon, as Normal Shot's (the newest ghost of N + 1 at his weapon's next pose).
// A draw-only (paused) tick holds everything (its next tick draws the same state) except the newest ghost (as 188). Nothing here
// writes game state: the module globals, the packet cursor (a private buffer holding the real tick's packets under the replay),
// the frame arena, the Field_Alloc scratch, the shadow-rotation scratch, the vertex buffer (0x1D98B3C), the render scratch words,
// the GTE registers, his weapon's bone matrices, his colour word and world matrix are put back. The screen flash (the fade tasks)
// is the engine's. No camera word is written by the module: no held camera.

namespace ff8fx
{
namespace dark193
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xC000;   // one packet arena (texture base / + 0xC000)
	static const uint32_t FARENA = 0x10000; // frame arena (the ghosts' RenderGeometry)
	static const uint32_t BSS_LO = 0x2276778, BSS_HI = 0x227B334;
	enum : uint8_t { REC_SMOKE = 1, REC_FLASH2, REC_SMOKE2, REC_GSMOKE, REC_IMPACT, REC_TSPARK, REC_GHOSTS, REC_BLAST, REC_MODEL };

	struct Rec
	{
		uint8_t kind;
		bool paused;          // a draw-only tick
		bool last;            // smokes: its sequence ended on this frame
		Node node;            // as drawn
		int32_t alive;        // impact / target sparks: particles alive after the update (-1: no update)
		int first, count;     // impact / target sparks: its particle records
		int ghost;            // ghosts: index in g_ghosts
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};
	struct SparkRec
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
	static int g_nrec = 0, g_nscratch = 0, g_nspark = 0, g_nghosts = 0, g_cur_parts = -1;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static Rec g_recs[0x400];
	static uint8_t g_scratch[0x400][SCRATCH];
	static SparkRec g_sparks[0x1000];
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
		g_nspark = 0;
		g_nghosts = 0;
		g_cur_parts = -1;
		g_packets = 0;
		g_frame_start = 0;
	}

	// the scratch the task allocated before its seam (the target sparks: their light scratch), replayed from that base
	static uint32_t PreAlloc(uint8_t kind) { return kind == REC_TSPARK ? 0x48u : 0u; }

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
		r.first = g_nspark;
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
		const uint8_t kind = fn == ORIG_SmokeTask ? REC_SMOKE : fn == ORIG_Flash2Task ? REC_FLASH2 : fn == ORIG_Smoke2Task ? REC_SMOKE2 :
			fn == ORIG_GroundSmokeTask ? REC_GSMOKE : fn == ORIG_ImpactTask ? REC_IMPACT : fn == ORIG_TargetSparkTask ? REC_TSPARK :
			fn == ORIG_BlastTask ? REC_BLAST : fn == ORIG_ModelTask ? REC_MODEL : 0;
		if (!kind) return;
		Rec *r = NewRec(kind);
		if (!r) return;
		r->node = *n;
		if (kind == REC_IMPACT || kind == REC_TSPARK) g_cur_parts = (int)(r - g_recs);
	}

	static void held_note_last(bool last)
	{
		if (g_in_held || g_log_tick != g_real_tick || !g_nrec) return;
		Rec &r = g_recs[g_nrec - 1];
		if (r.kind == REC_SMOKE || r.kind == REC_SMOKE2 || r.kind == REC_GSMOKE) r.last = last;
	}

	static void held_note_spark(const Particle *e)
	{
		if (g_in_held || g_cur_parts < 0 || g_nspark >= (int)(sizeof(g_sparks) / sizeof(g_sparks[0]))) return;
		SparkRec &s = g_sparks[g_nspark++];
		s.slot = (Particle *)e;
		s.drawn = *e;
		s.killed = false;
		g_recs[g_cur_parts].count++;
	}

	static void held_note_spark_next(const Particle *e)
	{
		if (g_in_held || g_cur_parts < 0 || !g_recs[g_cur_parts].count) return;
		SparkRec &s = g_sparks[g_nspark - 1];
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

	// ---- the growing models: scale + step (step - step / div), fade from counter `fade_from`; `ticks` ticks
	struct Grow { int32_t div, ticks, fade_from; int32_t (*fade)(int32_t); };
	static const Grow GROW_FLASH2 = { 3, 4, 1, Flash2Fade }, GROW_BLAST = { 4, 7, 1, BlastFade }, GROW_MODEL = { 5, 12, 4, ModelFade };
	static const Grow &GrowOf(uint8_t kind) { return kind == REC_FLASH2 ? GROW_FLASH2 : kind == REC_BLAST ? GROW_BLAST : GROW_MODEL; }
	static void GrowDraw(uint8_t kind, const Node *n, bool faded, int32_t fade)
	{
		if (kind == REC_FLASH2) MuzzleModel(n, n->f18, MODEL_Flash2, faded, fade);
		else if (kind == REC_BLAST) MuzzleModel(n, 0, MODEL_Blast, faded, fade);
		else TargetModelDraw(n, faded, fade);
	}
	static void HeldGrow(const Rec &r, int num, int den, bool at_next)
	{
		const Grow &gr = GrowOf(r.kind);
		const bool next = r.paused || r.node.counter + 1 < gr.ticks;
		if (at_next && !next) return;
		Node n = r.node;
		bool faded = n.counter >= gr.fade_from;
		int32_t fade = gr.fade(n.counter);
		if (!r.paused && next)
		{
			Node x = n;
			const int16_t step = x.f1E;
			x.f1C = (int16_t)(x.f1C + step);
			x.f1E = (int16_t)(step - (int16_t)(step / gr.div));
			x.counter++;
			if (at_next)
			{
				n = x;
				faded = n.counter >= gr.fade_from;
				fade = gr.fade(n.counter);
			}
			else
			{
				n.f1C = lerp16i(n.f1C, x.f1C, num, den);
				if (faded) fade = lerp_i(gr.fade(n.counter), gr.fade(x.counter), num, den);
			}
		}
		GrowDraw(r.kind, &n, faded, fade);
	}

	// ---- smokes (flipbook frame = counter)
	static void HeldSmoke(const Rec &r, bool at_next)
	{
		const bool next = r.paused || !r.last;
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		if (r.kind == REC_SMOKE) SmokeDraw(&n, SEQ_Smoke);
		else if (r.kind == REC_SMOKE2) SmokeDraw(&n, SEQ_Smoke2);
		else GroundSmokeDraw(&n);
	}

	// ---- the impact flash and its sparks / the target sparks
	static void HeldParts(const Rec &r, int num, int den, bool at_next)
	{
		const bool tspark = r.kind == REC_TSPARK;
		const bool ends = tspark ? r.alive == 0 : (r.node.counter + 1 >= 4 && r.alive == 0);
		const bool next = r.paused || !ends;
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		uint8_t *s = nullptr;
		if (tspark) s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (tspark) TargetSparkBegin(s, h);
		else
		{
			if (n.counter < 7) ImpactFlashDraw(&n, h);
			SparkHeader(h);
		}
		if (at_next && !r.paused)
		{
			// exactly the particles the next tick draws (impact: its array entries with the id, in order, the ones born at the
			// end of this tick too; target sparks: the ones it spawns at the start of the next tick are not there yet)
			Particle *arr = PartArray(r.node.f0E);
			const int32_t id = r.node.f0E + 1;
			for (Particle *e = arr; e < arr + PARTS_N; e++)
				if (e->id == id)
				{
					if (tspark) TargetSparkDraw(e, r.node.pos[3], s, h);
					else SparkDraw(e, r.node.pos[3], h);
				}
		}
		else
		{
			for (int i = r.first; i < r.first + r.count; i++)
			{
				const SparkRec &q = g_sparks[i];
				Particle p = q.drawn;
				if (!tspark && !r.paused && !q.killed && q.slot->id == q.drawn.id)
					for (int c = 0; c < 3; c++) p.pos[c] = lerp16i(q.drawn.pos[c], q.slot->pos[c], num, den);
				if (tspark) TargetSparkDraw(&p, r.node.pos[3], s, h);
				else SparkDraw(&p, r.node.pos[3], h);
			}
		}
		FieldFree(0xB4);
		if (tspark) FieldFree(0x48);
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
				case REC_FLASH2:
				case REC_BLAST:
				case REC_MODEL: HeldGrow(r, num, den, at_next); break;
				case REC_SMOKE:
				case REC_SMOKE2:
				case REC_GSMOKE: HeldSmoke(r, at_next); break;
				case REC_IMPACT:
				case REC_TSPARK: HeldParts(r, num, den, at_next); break;
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

	static void register_mag193_held()
	{
		using namespace dark193;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_RestoreTask);
		mark_held(ORIG_StartTask);
		mark_held(ORIG_ShotTask);
		mark_held(ORIG_EndTask);
		mark_held(ORIG_FadeOutTask);
		mark_held(ORIG_FadeInTask);
		mark_held(ORIG_AfterimageTask);
		mark_held(ORIG_SmokeTask);
		mark_held(ORIG_Flash2Task);
		mark_held(ORIG_Smoke2Task);
		mark_held(ORIG_BlastTask);
		mark_held(ORIG_GroundSmokeTask);
		mark_held(ORIG_ImpactTask);
		mark_held(ORIG_TargetSparkTask);
		mark_held(ORIG_ModelTask);
		register_module_held(193, HeldReady, HeldFrame);
	}
}
