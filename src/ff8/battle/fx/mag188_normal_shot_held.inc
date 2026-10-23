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

// 30 fps part of mag188_normal_shot.cpp (Irvine's Normal Shot): the draw log of the real tick and the held-frame drawing
// (included at the end of mag188_normal_shot.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// Every drawing task draws THEN updates. A real tick records every draw of the module in order (the root's seam starts the
// log) with the GTE registers and the Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the
// same order with the vanilla draw code, each at its in-between state (logic stays 15 Hz), under the LIVE camera (the held
// frame's view in 0x1D97778: the camera-facing sprites and every model compose with it, as the real draws do with theirs):
//   Flash     - nothing moves (its scale / roll are constant); the unfaded -> faded switch (counter 0 -> 1) on the real frame;
//               its last tick holds below 1 and is not drawn at 1.
//   Flash2    - scale num / den of the way to scale + step; fade num / den of the way where both ticks fade (counters 1..3; the
//               switch to faded on the real frame); its last tick holds.
//   Smoke / ground smoke / the impact flash - flipbook frames switch on the real frame (frame = counter); position and size are
//               constant; a sequence the next tick does not draw (ended / counter 16) is held below 1, not drawn at 1.
//   Sparks    - each particle num / den of the way to the state the next tick draws (its array entry after the update:
//               position + velocity, frame + 1 on the real frame); one that died holds below 1. At 1 the impact draws exactly
//               the particles the next tick draws (its array entries in order: the ones born on this tick too).
//   Ghosts    - the afterimages of his weapon: ghost k of the next tick is the bone snapshot one tick newer (ghost k at tick N
//               = snapshot c - 1 - k, at N + 1 snapshot c - k; the newest ghost of N + 1 is the snapshot that tick takes: his
//               weapon at its NEXT pose, pose_midpoint at 1, the look-ahead of the animation reader, the real bones put back);
//               each ghost's bone matrices num / den of the way to its next ones, his world matrix from the one drawn to the
//               one the frame holds; a ghost the next tick drops holds below 1; at 1 exactly the next tick's ghost list.
// A draw-only (paused) tick holds everything (its next tick draws the same state) except the newest ghost: the animations do not
// step on draw-only ticks (0x502AB0 skips AnimSeq_UpdateEntityPerFrame under 0x201), so the next real tick retakes the same ghost
// slot at his next pose. (A next tick that is itself draw-only cannot be foreseen: its newest ghost is his current pose.) Nothing here writes game state: the module
// globals, the packet cursor (a private buffer holding the real tick's packets under the replay), the frame arena, the
// Field_Alloc scratch, the shadow-rotation scratch, the vertex buffer (0x1D98B3C), the render scratch words, the GTE registers,
// his weapon's bone matrices, his colour word and world matrix are put back. The screen flash (the fade tasks) is the engine's.
// No camera word is written by the module: no held camera.

namespace ff8fx
{
namespace shot188
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0xC000;   // one packet arena (texture base / + 0xC000)
	static const uint32_t FARENA = 0x10000; // frame arena (the ghosts' RenderGeometry)
	static const uint32_t BSS_LO = 0x22B2A38, BSS_HI = 0x22B75F4;
	enum : uint8_t { REC_FLASH = 1, REC_FLASH2, REC_SMOKE, REC_GSMOKE, REC_IMPACT, REC_GHOSTS };

	struct Rec
	{
		uint8_t kind;
		bool paused;          // a draw-only tick
		bool last;            // smoke: its sequence ended on this frame
		Node node;            // as drawn
		int32_t alive;        // impact: particles alive after the update (-1: no update)
		int first, count;     // impact: its particle records
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
	static int g_nrec = 0, g_nscratch = 0, g_nspark = 0, g_nghosts = 0, g_cur_impact = -1;
	static uint32_t g_packets = 0, g_frame_start = 0;
	static Rec g_recs[0x400];
	static uint8_t g_scratch[0x400][SCRATCH];
	static SparkRec g_sparks[0x800];
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
		g_cur_impact = -1;
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
		r.first = g_nspark;
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
		g_cur_impact = -1;
		const uint8_t kind = fn == ORIG_FlashTask ? REC_FLASH : fn == ORIG_Flash2Task ? REC_FLASH2 : fn == ORIG_SmokeTask ? REC_SMOKE :
			fn == ORIG_GroundSmokeTask ? REC_GSMOKE : fn == ORIG_ImpactTask ? REC_IMPACT : 0;
		if (!kind) return;
		Rec *r = NewRec(kind);
		if (!r) return;
		r->node = *n;
		if (kind == REC_IMPACT) g_cur_impact = (int)(r - g_recs);
	}

	static void held_note_last(bool last)
	{
		if (g_in_held || g_log_tick != g_real_tick || !g_nrec) return;
		Rec &r = g_recs[g_nrec - 1];
		if (r.kind == REC_SMOKE || r.kind == REC_GSMOKE) r.last = last;
	}

	static void held_note_spark(const Particle *e)
	{
		if (g_in_held || g_cur_impact < 0 || g_nspark >= (int)(sizeof(g_sparks) / sizeof(g_sparks[0]))) return;
		SparkRec &s = g_sparks[g_nspark++];
		s.slot = (Particle *)e;
		s.drawn = *e;
		s.killed = false;
		g_recs[g_cur_impact].count++;
	}

	static void held_note_spark_next(const Particle *e)
	{
		if (g_in_held || g_cur_impact < 0 || !g_recs[g_cur_impact].count) return;
		SparkRec &s = g_sparks[g_nspark - 1];
		if (s.slot == e) s.killed = true;
	}

	static void held_note_alive(int32_t alive)
	{
		if (g_in_held || g_cur_impact < 0) return;
		Rec &r = g_recs[g_cur_impact];
		if (!r.paused) r.alive = alive;
		g_cur_impact = -1;
	}

	static void held_note_ghosts(int32_t counter, int32_t slot, int32_t count, uint32_t colour)
	{
		g_cur_impact = -1;
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

	// ---- muzzle flashes
	static void HeldFlash(const Rec &r, bool at_next)
	{
		const bool next = r.paused || r.node.counter + 1 < 2;
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		MuzzleModel(&n, MODEL_Flash, n.counter == 1, 0xC00);
	}
	static void HeldFlash2(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = r.paused || r.node.counter + 1 < 4;
		if (at_next && !next) return;
		Node n = r.node;
		int32_t fade = Flash2Fade(n.counter);
		if (!r.paused && next)
		{
			Node x = n;
			const int16_t step = x.f1E;
			x.f1C = (int16_t)(x.f1C + step);
			x.f1E = (int16_t)(step - (int16_t)(step / 3));
			x.counter++;
			if (at_next)
			{
				n = x;
				fade = Flash2Fade(n.counter);
			}
			else
			{
				n.f1C = lerp16i(n.f1C, x.f1C, num, den);
				if (n.counter >= 1) fade = lerp_i(Flash2Fade(n.counter), Flash2Fade(x.counter), num, den);
			}
		}
		MuzzleModel(&n, MODEL_Flash2, n.counter >= 1, fade);
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

	// ---- the impact flash and its particles
	static void HeldImpact(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = r.paused || !(r.node.counter + 1 >= 4 && r.alive == 0);
		if (at_next && !next) return;
		Node n = r.node;
		if (at_next && !r.paused) n.counter++;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (n.counter < 0x10) ImpactFlashDraw(&n, h);
		*(int16_t *)(h + 0x24) = 8;
		*(uint32_t *)h = SEQ_Spark;
		if (at_next && !r.paused)
		{
			// exactly the particles the next tick draws: its array entries with the id, in order
			Particle *arr = Sparks(&r.node);
			const int32_t id = r.node.f0E + 1;
			for (Particle *e = arr; e < arr + 150; e++)
				if (e->id == id) SparkDraw(e, r.node.pos[3], h);
		}
		else
		{
			for (int i = r.first; i < r.first + r.count; i++)
			{
				const SparkRec &s = g_sparks[i];
				Particle p = s.drawn;
				if (!r.paused && !s.killed && s.slot->id == s.drawn.id)
					for (int c = 0; c < 3; c++) p.pos[c] = lerp16i(s.drawn.pos[c], s.slot->pos[c], num, den);
				SparkDraw(&p, r.node.pos[3], h);
			}
		}
		FieldFree(0xB4);
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
				case REC_FLASH: HeldFlash(r, at_next); break;
				case REC_FLASH2: HeldFlash2(r, num, den, at_next); break;
				case REC_SMOKE:
				case REC_GSMOKE: HeldSmoke(r, at_next); break;
				case REC_IMPACT: HeldImpact(r, num, den, at_next); break;
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

	static void register_mag188_held()
	{
		using namespace shot188;
		mark_held(ORIG_RootTask);
		mark_held(ORIG_StartTask);
		mark_held(ORIG_ShotTask);
		mark_held(ORIG_EndTask);
		mark_held(ORIG_FadeOutTask);
		mark_held(ORIG_FadeInTask);
		mark_held(ORIG_AfterimageTask);
		mark_held(ORIG_FlashTask);
		mark_held(ORIG_Flash2Task);
		mark_held(ORIG_SmokeTask);
		mark_held(ORIG_GroundSmokeTask);
		mark_held(ORIG_ImpactTask);
		register_module_held(188, HeldReady, HeldFrame);
	}
}
