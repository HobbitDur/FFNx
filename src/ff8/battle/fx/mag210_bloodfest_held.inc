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

// 30 fps part of mag210_bloodfest.cpp (Bloodfest, effect 210; Demon Slice's held part, 251, plus Burning Rave's afterimage /
// module-drawn entity / stage capture handling, 212). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws
// the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the main task draws each phase before its moves). A real tick records its draws in order
// with the GTE state and the Field_Alloc scratch each one started from; a held frame replays them into private packets
// (packet arena, frame arena, capture cursor), with the live camera (the orthonormal view 0x2556560 rebuilt from it):
//   Layouts  - prim::play_held at their place: 0x146D8A4 at the caster's effect point, the burning layout (its texture
//              scroll and kind-0x33 window num/den of the way), 0x146A668; layout tasks at their point.
//   Bone model - (0x41..0x48) at the caster's bone 0x25 num/den of the way to the bone at his NEXT pose (pose_midpoint at
//              1), its turn and fade num/den of the way.
//   Afterimages - each ghost's recorded bone matrices num/den of the way to the same ghost's on the next tick (the block
//              after the tick), drawn by the tick's code on a private copy of the block; a ghost the next tick drops holds.
//   Targets  - (0x73..0xEA, drawn by the module) their spin and height num/den of the way to the next draw's, their pose
//              num/den of the way (pose_midpoint), their world matrix x / z from the drawn one to the one the frame holds.
//   Stage capture - replayed below 1 (the same capture), not drawn at 1.
//   Three-part draws - their depth / v scroll / fade num/den of the way (a switch between the prim set and the ring +
//              quads holds).
//   Tiles    - the white tile series holds its step (at 1 the next step).
//   Trail    - 251's: the next tick's ring predicted exactly, each spline point num/den of the way.
//   Flames / sparks / embers / sparkles / rocks - the node num/den of the way to its update; flipbook frames switch on the
//              real frame; a part ending on the tick (or its pool dropped: flames 0x21, rocks 0xC3 / 0xEB) holds up to 1 and
//              is not drawn at 1.
// A part the next tick does not draw holds up to 1 and is not drawn at 1. Spawns, loads, sounds, the result and the
// screen flash are real-tick only.
// Entities: the targets are module-drawn (hidden) while lifted / dropped; from the put-back (0xEB, a jump) to the end they
// are drawn by the engine, raised 0x200 a tick: HeldEntities lists them to the frame gate.
// Nothing here writes game state: the module globals, the packet / capture / frame-arena cursors (private buffers holding
// the real tick's packets under the replay), the Field_Alloc scratch, the shadow-rotation scratch, the screen vertex
// buffer, the sprite words, the screen flash words, the stage's bone matrices, the word 0x1D9770C, the GTE registers, the
// entities' bone / world matrices and the screen offset are put back.

namespace ff8fx
{
namespace blood210
{
	enum RecKind : uint8_t { R_SPRITE, R_LAYOUT, R_BONE, R_GHOSTS, R_ENTITY, R_CAPTURE, R_THREE, R_TILE, R_TRAIL, R_TASKLAYOUT };
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x20000;
	static const uint32_t FARENA = 0x40000;
	static const uint32_t BSS_LO = 0x2556478, BSS_HI = 0x2556580;
	static const uint32_t FLASH_LO = 0x1D9898C, FLASH_HI = 0x1D98A68;
	static const int MAX_RECS = 0x180;

	struct Rec
	{
		RecKind kind;
		uint32_t fn;
		const void *node;
		uint8_t *layout;
		uint8_t *ent;
		int site;
		int32_t i, c;
		int32_t x, z;
		int32_t p[6];
		int16_t ang[3], off[3], y;
		int16_t box_a[6], box_b[6];
		int16_t sp[4];
		Mat4x3 world;
		PlayArg arg;
		uint8_t snode[0x24];
		uint8_t hdr[0xB4];
		uint8_t ghost[0x108];
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_paused = false, g_in_held = false;
	static int32_t g_main_c = -1;
	static const MainNode *g_main = nullptr;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_frame_start = 0, g_capture_start = 0;
	static Rec g_recs[MAX_RECS];
	static int16_t g_trail_sa[32][4], g_trail_sb[32][4];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FARENA];
	static uint8_t g_held_capture[0x10000];
	static uint8_t g_held_morph[0x4000];
	static uint8_t g_held_ghost[0x108];
	static int16_t g_na[32][4], g_nb[32][4];
	static uint8_t g_bones_save[16 + 48 * 256];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_paused = Pause() != 0;
		g_main_c = -1;
		g_main = nullptr;
		g_nrec = 0;
		g_packets = 0;
		g_frame_start = 0;
		g_capture_start = CaptureCursor();
	}

	static void held_note_main(const MainNode *mn)
	{
		if (g_ported_tick != g_real_tick || g_in_held) return;
		g_main_c = mn->counter;
		g_main = mn;
	}

	static Rec *NewRec(RecKind kind)
	{
		g_hdr_slot = nullptr;
		if (g_in_held || g_ported_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.fn = 0;
		r.node = nullptr;
		r.layout = nullptr;
		r.ent = nullptr;
		r.c = g_main_c;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_draw(uint32_t fn, const void *node)
	{
		Rec *r = NewRec(R_SPRITE);
		if (!r) return;
		r->fn = fn;
		r->node = node;
		memcpy(r->snode, node, fn == ORIG_SparkleTask ? sizeof(SparkleNode) : fn == ORIG_RockTask ? sizeof(RockNode) : 0x24);
		g_hdr_slot = r->hdr;  // the draw's header scratch (AllocHeader)
	}

	static void held_note_layout(MainNode *mn, int site, uint8_t *layout, const PlayArg *arg, int32_t x, int32_t z)
	{
		Rec *r = NewRec(R_LAYOUT);
		if (!r) return;
		static const int32_t start[3] = { 0x40, 0x73, 0xC3 };
		r->site = site;
		r->layout = layout;
		r->c = mn->counter;
		r->i = mn->counter - start[site];
		r->arg = *arg;
		r->x = x;
		r->z = z;
	}

	static Rec *g_bone_rec = nullptr;
	static void held_note_bone(int32_t i)
	{
		Rec *r = NewRec(R_BONE);
		g_bone_rec = r;
		if (!r) return;
		r->i = i;
	}
	static void held_note_bone_point(const int16_t *sp)
	{
		if (g_bone_rec) memcpy(g_bone_rec->sp, sp, 8);
		g_bone_rec = nullptr;
	}

	static void held_note_ghosts(uint8_t *entity, const uint8_t *g)
	{
		Rec *r = NewRec(R_GHOSTS);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ent = entity;
		r->node = g;
		memcpy(r->ghost, g, sizeof(r->ghost));
	}

	static void held_note_entity(uint8_t *e, const int16_t *angles, const int16_t *off)
	{
		Rec *r = NewRec(R_ENTITY);
		if (!r) return;
		if (!g_frame_start) g_frame_start = FramePackets();
		r->ent = e;
		memcpy(r->ang, angles, 6);
		memcpy(r->off, off, 6);
		r->y = *(const int16_t *)(e + 0x1E);
		r->world = *(const Mat4x3 *)(e + 0x40);
		r->site = 0;
		if (g_main)
			for (int k = 0; k < g_main->ntargets; k++)
				if (g_main->recs[k].e == e) r->site = k;
	}

	static void held_note_capture(const int16_t *a, const int16_t *b)
	{
		Rec *r = NewRec(R_CAPTURE);
		if (!r) return;
		memcpy(r->box_a, a, 6);
		memcpy(r->box_b, b, 12);
	}

	static void held_note_three(const int16_t *c, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6)
	{
		(void)c;
		Rec *r = NewRec(R_THREE);
		if (!r) return;
		r->p[0] = a2;
		r->p[1] = a3;
		r->p[2] = a4;
		r->p[3] = a5;
		r->p[4] = a6;
		r->site = a2 == 0x800 ? 0 : 1;
	}

	static void held_note_tile(int32_t t)
	{
		Rec *r = NewRec(R_TILE);
		if (r) r->i = t;
	}

	static void held_note_trail(const TrailNode *t)
	{
		Rec *r = NewRec(R_TRAIL);
		if (!r) return;
		r->node = t;
		r->i = t->counter;
		memcpy(g_trail_sa, t->spl_a, sizeof(g_trail_sa));
		memcpy(g_trail_sb, t->spl_b, sizeof(g_trail_sb));
	}

	static void held_note_task_layout(const LayoutNode *t, const PlayArg *arg)
	{
		Rec *r = NewRec(R_TASKLAYOUT);
		if (!r) return;
		r->node = t;
		r->layout = (uint8_t *)t->layout;
		r->arg = *arg;
		r->x = t->pos[0];
		r->y = t->pos[1];
		r->z = t->pos[2];
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the next tick runs the main task again (and so the master's queues)
	static bool MainNext() { return g_main_c >= 0 && (g_main_c < 0xF3 || g_main_c == 0x133); }
	static int32_t NextCounter() { return g_main_c >= 0xF3 ? 0x133 : g_main_c + 1; }

	static uint8_t *Skeleton(uint8_t *hdr, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(hdr + 4);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= sizeof(g_bones_save) ? sk : nullptr;
	}

	static bool NodeAlive(const void *node, uint32_t fn)
	{
		const TaskNode *t = (const TaskNode *)node;
		return (t->flags & 1) != 0 && (uint32_t)t->func == fn;
	}

	static void LerpMat(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out->m[i][j] = lerp16(a.m[i][j], b.m[i][j], num, den);
		for (int i = 0; i < 3; i++) out->t[i] = lerp_i(a.t[i], b.t[i], num, den);
	}

	// a layout at k/den: below 1 the player's in-between records (play_held: an object appearing or disappearing on the next
	// tick does so on the real frame); at 1 (the harness's look at the next tick; the frame gate draws num < den only) the
	// next tick's draw exactly: the player run on the layout (its state is inline in the Layout block), the block put back
	// (play notes it drew the next frame: the last held frame of the tick)
	static uint8_t g_layout_save[0x800];
	static void PlayAt(uint8_t *layout, uint32_t size, prim::Callback cb, int arg, int k, int den)
	{
		if (k == den && size <= sizeof(g_layout_save))
		{
			memcpy(g_layout_save, layout, size);
			prim::play((prim::Layout *)layout, cb, arg, 0);
			memcpy(layout, g_layout_save, size);
		}
		else prim::play_held((prim::Layout *)layout, cb, arg, k, den);
	}

	// ---- layouts of the main task (their places are fixed: the caster does not move) ----
	static void HeldLayout(const Rec &r, int num, int den, bool at_next)
	{
		static const int32_t length[3] = { 0xF, 0x50, 0x28 };
		const int32_t ni = r.i + 1;
		const bool next = !g_paused && MainNext() && ni < length[r.site];
		if (at_next && !next) return;
		const int k = next ? num : 0;
		PlayArg a = r.arg;
		if (k && r.site == 1)
		{
			a.scroll = (uint16_t)lerp16((int16_t)(r.i << 4), (int16_t)(ni << 4), k, den);
			const uint32_t d0 = (uint32_t)(r.i - 0x1A), d1 = (uint32_t)(ni - 0x1A);
			const int16_t w0 = d0 < 0x64 ? (int16_t)((d0 * d0) >> 4) : 0, w1 = d1 < 0x64 ? (int16_t)((d1 * d1) >> 4) : 0;
			a.w28 = (uint16_t)lerp16(w0, w1, k, den);
		}
		RotationY(0, &a.m);
		a.m.t[0] = r.x;
		a.m.t[1] = 0;
		a.m.t[2] = r.z;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		static const uint32_t size[3] = { 0xE0, 0x244, 0x258 }; // (their decode sizes)
		PlayAt(r.layout, size[r.site], PrimCallback, (int)&a, k, den);
	}

	static void HeldTaskLayout(const Rec &r, int num, int den, bool at_next)
	{
		// (the master runs the layout-task queue until it is empty, after the main task too: 0xF3 / the preload wait)
		const bool next = !g_paused && NodeAlive(r.node, ORIG_LayoutTask);
		if (at_next && !next) return;
		const int k = next ? num : 0;
		PlayArg a = r.arg;
		memset(&a.m, 0, sizeof(a.m));
		a.m.m[0][0] = 0x1000;
		a.m.m[1][1] = 0x1000;
		a.m.m[2][2] = 0x1000;
		a.m.t[0] = r.x;
		a.m.t[1] = r.y;
		a.m.t[2] = r.z;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = (uint32_t)g_held_morph;
		PlayAt(r.layout, 0x1C0, PrimCallback, (int)&a, k, den); // (its decode size)
	}

	// ---- the bone-0x25 prim model (0x41..0x48) ----
	static void HeldBone(const Rec &r, int num, int den, bool at_next)
	{
		const bool next = !g_paused && MainNext() && r.i + 1 < 8;
		if (at_next && !next) return;
		const int k = next ? num : 0;
		int16_t sp[4];
		memcpy(sp, r.sp, 8);
		if (k)
		{
			// the bone's point at the caster's next pose, through his matrix as the frame holds it
			uint8_t *e = Caster();
			uint32_t nb = 0;
			uint8_t *sk = Skeleton(e + 0x60, &nb);
			if (sk) memcpy(g_bones_save, sk, nb);
			const uint16_t w70c = var<uint16_t>(0x1D9770C);
			if (sk) pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
			int16_t nx[4];
			GetEffectSpawnPosition(e, 0x25, 0, nx);
			var<uint16_t>(0x1D9770C) = w70c;
			if (sk) memcpy(sk, g_bones_save, nb);
			for (int q = 0; q < 3; q++) sp[q] = lerp16(r.sp[q], nx[q], k, den);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x80);
		memcpy(h + 0x78, sp, 8);
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		RotationY(lerp_i(r.i << 8, (r.i + 1) << 8, k, den), m);
		m->m[1][1] = (int16_t)-sp[1];
		m->t[0] = sp[0];
		m->t[1] = 0;
		m->t[2] = sp[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = MODEL_Bone;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		*(uint32_t *)(h + 0xC) = (uint32_t)lerp_i(((uint32_t)r.i << 12) >> 3, ((uint32_t)(r.i + 1) << 12) >> 3, k, den);
		*(uint32_t *)(h + 8) = 0;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x80);
	}

	// ---- afterimages ----
	static const uint8_t *GhostSlot(const uint8_t *g, int i) { return g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x40; }
	// the next tick draws afterimages (0x37..0x3D, 0x49..0x63: 0x48 starts a new empty block)
	static bool GhostsNext(int32_t nc) { return (uint32_t)(nc - 0x37) < 7 || (uint32_t)(nc - 0x48) < 0x1C; }
	static void HeldGhosts(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t nc = NextCounter();
		const uint8_t *nx = (!g_paused && MainNext() && GhostsNext(nc)) ? (const uint8_t *)r.node : nullptr;
		if (at_next)
		{
			if (!nx) return;
			memcpy(g_held_ghost, nx, sizeof(g_held_ghost));
			if (nc == 0x48) g_held_ghost[1] = 0; // (the block starts again empty: nothing drawn)
		}
		else
		{
			memcpy(g_held_ghost, r.ghost, sizeof(g_held_ghost));
			for (int i = 0; i < r.ghost[1]; i++)
			{
				if (!nx || nc == 0x48 || i >= nx[1]) continue; // (a ghost the next tick drops: held)
				uint8_t *dst = (uint8_t *)GhostSlot(g_held_ghost, i);
				const uint8_t *a = GhostSlot(r.ghost, i), *b = GhostSlot(nx, i);
				for (int q = 0; q < r.ghost[4]; q++) LerpMat((Mat4x3 *)(dst + q * 0x20), *(const Mat4x3 *)(a + q * 0x20), *(const Mat4x3 *)(b + q * 0x20), num, den);
			}
		}
		GhostDraw(r.ent, g_held_ghost);
	}

	// ---- the module-drawn targets (0x73..0xEA) ----
	static bool EntityNext(const Rec &r, int16_t *ang, int16_t *y)
	{
		if (g_paused || !MainNext()) return false;
		const int32_t nc = NextCounter();
		const TargetRec *t = g_main ? &g_main->recs[r.site] : nullptr;
		if (!t) return false;
		memcpy(ang, r.ang, 6);
		ang[0] = (int16_t)(ang[0] + ((r.site & 1) ? 0x100 : -0x100));
		const int16_t y0 = (int16_t)(t->pos01 >> 16);
		const uint32_t i1 = (uint32_t)(nc - 0x73), i2 = (uint32_t)(nc - 0xC3);
		if (i1 < 0x50) *y = LiftHeight((int32_t)((i1 << 12) / 80) + 0x19, y0);
		else if (i2 < 0x28) *y = (int16_t)(y0 + (int16_t)(((int32_t)((i2 << 12) / 40) + 0x33) * 4 - 0x2000));
		else return false;
		return true;
	}

	static void HeldTarget(const Rec &r, int num, int den, bool at_next)
	{
		int16_t na[3], ny = r.y;
		const bool next = EntityNext(r, na, &ny);
		if (at_next && !next) return;
		const int k = next ? num : 0;
		uint8_t *e = r.ent;
		int16_t ang[3];
		for (int q = 0; q < 3; q++) ang[q] = next ? lerp_angle(r.ang[q], na[q], k, den) : r.ang[q];
		const Mat4x3 keep = *(const Mat4x3 *)(e + 0x40);
		Mat4x3 *w = (Mat4x3 *)(e + 0x40);
		*w = r.world;
		w->t[0] = lerp_i(r.world.t[0], keep.t[0], k, den);
		w->t[2] = lerp_i(r.world.t[2], keep.t[2], k, den);
		w->t[1] = lerp_i(r.y, ny, k, den);
		uint32_t b0 = 0, b1 = 0;
		uint8_t *sk0 = Skeleton(e + 0x60, &b0);
		uint8_t *wp = *(uint8_t **)(e + 0x78);
		uint8_t *sk1 = wp ? Skeleton(wp, &b1) : nullptr;
		static uint8_t save1[16 + 48 * 256];
		if (sk0) memcpy(g_bones_save, sk0, b0);
		if (sk1) memcpy(save1, sk1, b1);
		if (k)
		{
			if (sk0) pose_midpoint(e + 0x60, e + 0x6C, k, den);
			if (sk1) pose_midpoint(wp, wp + 0xC, k, den);
		}
		DrawEntity(e, ang, r.off);
		if (sk0) memcpy(sk0, g_bones_save, b0);
		if (sk1) memcpy(sk1, save1, b1);
		*w = keep;
	}

	// ---- the stage capture: the same capture below 1 (the next tick captures nothing) ----
	static void HeldCapture(const Rec &r, bool at_next)
	{
		if (at_next) return;
		uint32_t nb = 0;
		uint8_t *sk = Skeleton((uint8_t *)0x1D989D0, &nb);
		if (sk) memcpy(g_bones_save, sk, nb);
		CaptureStage(r.box_a, r.box_b);
		if (sk) memcpy(sk, g_bones_save, nb);
	}

	// ---- the three-part draws (0x55..0x72) ----
	static void ThreeOf(int site, int32_t i, int32_t *a4, int32_t *a5, int32_t *a6)
	{
		*a6 = i >= 0x16 ? shl32(i - 0x16, 12) / 8 : 0;
		*a4 = site == 0 ? (int32_t)(((uint32_t)i << 12) >> 3) : (int32_t)(((uint32_t)(i - 0xB) << 12) >> 3);
		*a5 = i << 5;
	}
	static void HeldThree(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t ni = r.c + 1 - 0x55;
		const bool next = !g_paused && MainNext() && ni < 0x1E;
		if (at_next && !next) return;
		int32_t a4 = r.p[2], a5 = r.p[3], a6 = r.p[4];
		if (next)
		{
			int32_t n4, n5, n6;
			ThreeOf(r.site, ni, &n4, &n5, &n6);
			if ((a6 > 0) == (n6 > 0) && (a4 < 0x1000) == (n4 < 0x1000))
			{
				a4 = lerp_i(a4, n4, num, den);
				a5 = lerp_i(a5, n5, num, den);
				a6 = lerp_i(a6, n6, num, den);
			}
			else if (at_next)
			{
				a4 = n4;
				a5 = n5;
				a6 = n6;
			}
		}
		ThreePart(g_main ? g_main->centre : Centre(), r.p[0], r.p[1], a4, a5, a6);
	}

	// ---- white tiles: the step holds (at 1 the next step) ----
	static void HeldTile(const Rec &r, bool at_next)
	{
		const bool next = !g_paused && MainNext() && r.i + 1 < 0x10;
		if (at_next && !next) return;
		PacketCursor() = FadeTileInHoldOut(at_next ? r.i + 1 : r.i, 0xF0, 0xF0, 0xF0, 6, 4, 6, PacketCursor());
	}

	// ---- the trail (251's) ----
	// cn: the counter the next tick draws with (the recorded one + 1; the live node already holds it after its update)
	static bool TrailNext(const TrailNode *t, int32_t cn)
	{
		if (cn >= 0x3B) return false;
		uint8_t *e = TRAIL_ENTITY;
		uint32_t nb = 0;
		uint8_t *sk = Skeleton(e + 0x60, &nb);
		if (sk) memcpy(g_bones_save, sk, nb);
		if (sk) pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		WeaponBone(e, 1, w->dir, w->base);
		RingEntry ring[7];
		memcpy(ring, t->ring, sizeof(ring));
		int32_t idx = cn % 7;
		ring[idx].alive = 1;
		memcpy(ring[idx].base, w->base, 8);
		ring[idx].tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 777) >> 12) + w->base[0]);
		ring[idx].tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 777) >> 12) + w->base[1]);
		ring[idx].tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 777) >> 12) + w->base[2]);
		ring[idx].tip[3] = w->tip[3];
		FieldFree(0x40);
		if (sk) memcpy(sk, g_bones_save, nb);
		const int32_t want = cn >= 0x35 ? 0x3C - cn : 7;
		static int16_t pa[8][4], pb[8][4];
		int32_t cnt = 0;
		while (cnt < want)
		{
			const RingEntry *q = &ring[idx];
			if (!q->alive) break;
			idx--;
			memcpy(pa[cnt], q->base, 8);
			memcpy(pb[cnt], q->tip, 8);
			if (idx < 0) idx = 6;
			cnt++;
		}
		if (cnt <= 1) return false;
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, wk);
		for (int32_t j = 0, u = 0; j < 32; j++, u += 0x1000) SplineEval(cnt, wk, g_na[j], u / 31);
		SplineSetup(cnt, pb, wk);
		for (int32_t j = 0, u = 0; j < 32; j++, u += 0x1000) SplineEval(cnt, wk, g_nb[j], u / 31);
		FieldFree(0x190);
		return true;
	}

	static void HeldTrail(const Rec &r, int num, int den, bool at_next)
	{
		const TrailNode *t = (const TrailNode *)r.node;
		const bool next = !g_paused && MainNext() && NodeAlive(t, ORIG_TrailTask);
		if (at_next && !next) return;
		const int16_t (*sa)[4] = g_trail_sa;
		const int16_t (*sb)[4] = g_trail_sb;
		static int16_t ma[32][4], mb[32][4];
		if (next && num != 0)
		{
			if (TrailNext(t, r.i + 1))
			{
				for (int j = 0; j < 32; j++)
					for (int q = 0; q < 4; q++)
					{
						ma[j][q] = lerp16(g_trail_sa[j][q], g_na[j][q], num, den);
						mb[j][q] = lerp16(g_trail_sb[j][q], g_nb[j][q], num, den);
					}
				sa = ma;
				sb = mb;
			}
			else if (at_next) return;
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		}
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailDraw(sa, sb, w);
		FieldFree(0x40);
	}

	// ---- sprites (Fire's parts, sparkles) and rocks ----
	static void HeldSprite(const Rec &r, int num, int den, bool at_next)
	{
		const int32_t nc = NextCounter();
		const bool flames = MainNext() && nc != 0x21;          // the main task drops the flame queue at 0x21
		const bool rocks = MainNext() && nc != 0xC3 && nc != 0xEB; // ... the rock pool at 0xC3 / 0xEB
		g_hdr_slot = (uint8_t *)r.hdr;
		switch (r.fn)
		{
		case ORIG_FlameTask:
		{
			FlameNode a;
			memcpy(&a, r.snode, sizeof(a));
			const bool next = flames && a.frame + 1 < 0x10;
			if (at_next && !next) break;
			if (at_next && !g_paused) a.frame++;
			FlameDraw(&a.f, a.frame, FLAME_LOOK, PacketCursor());
			return;
		}
		case ORIG_SparkTask:
		{
			SparkNode a, b;
			memcpy(&a, r.snode, sizeof(a));
			b = a;
			if (!g_paused) FireSparkMove(&b.s, &b.life, 4, 0x40);
			const bool next = flames && b.life >= 0;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			SparkNode m = a;
			for (int q = 0; q < 3; q++) m.s.pos[q] = lerp16(a.s.pos[q], b.s.pos[q], k, den);
			m.s.scale = lerp16(a.s.scale, b.s.scale, k, den);
			if (at_next) m.life = b.life;
			FireSparkDraw(&m.s, m.life, SPARK_LOOK, PacketCursor());
			return;
		}
		case ORIG_EmberTask:
		{
			EmberNode a, b;
			memcpy(&a, r.snode, sizeof(a));
			b = a;
			if (!g_paused)
			{
				EmberRise(&b.e, 5, 6);
				b.frame++;
			}
			const bool next = flames && b.frame < 0x10;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			EmberNode m = a;
			m.e.pos[1] = lerp16(a.e.pos[1], b.e.pos[1], k, den);
			m.e.scale = lerp16(a.e.scale, b.e.scale, k, den);
			if (at_next) m.frame = b.frame;
			EmberDraw(&m.e, m.frame, SEQ_Ember, 3, PacketCursor());
			return;
		}
		case ORIG_SparkleTask:
		{
			SparkleNode a;
			memcpy(&a, r.snode, sizeof(a));
			const bool next = !g_paused && MainNext() && a.frame + 1 < 0x10;
			if (at_next && !next) break;
			const int k = next ? num : 0;
			int16_t pos[3] = { a.pos[0], lerp16(a.pos[1], (int16_t)(a.pos[1] + a.vy), k, den), a.pos[2] };
			SparkleDraw(pos, at_next ? (int16_t)(a.frame + 1) : a.frame);
			return;
		}
		case ORIG_RockTask:
		{
			g_hdr_slot = nullptr;
			RockNode a;
			memcpy(&a, r.snode, sizeof(a));
			const bool next = !g_paused && rocks && (int16_t)(a.life - 1) != 0;
			if (at_next && !next) return;
			const int k = next ? num : 0;
			RockNode m = a;
			if (at_next)
			{
				m.ax = (int8_t)(a.ax + a.dax);
				m.ay = (int8_t)(a.ay + a.day);
				m.pos[1] = (int16_t)(a.pos[1] + a.vy);
				RockDraw(&m);
				return;
			}
			m.pos[1] = lerp16(a.pos[1], (int16_t)(a.pos[1] + a.vy), k, den);
			RockDrawAngles(&m, (int16_t)((int16_t)a.ax * 16 + (int16_t)a.dax * 16 * k / den), (int16_t)((int16_t)a.ay * 16 + (int16_t)a.day * 16 * k / den));
			return;
		}
		default:
			break;
		}
		g_hdr_slot = nullptr;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC], c18[8], flash[FLASH_HI - FLASH_LO], vbuf[0x8000];
		const uint32_t vb = var<uint32_t>(0x1D98B3C);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		const uint32_t frame_saved = FramePackets();
		int32_t ox, oy;
		GetScreenOffset(&ox, &oy);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		memcpy(c18, (const void *)0x1D99C18, sizeof(c18));
		memcpy(flash, (const void *)FLASH_LO, sizeof(flash));
		if (vb) memcpy(vbuf, (const void *)vb, sizeof(vbuf));
		g_in_held = true;
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			if (g_frame_start && frame_saved > g_frame_start)
			{
				const uint32_t used = frame_saved - g_frame_start;
				memcpy(g_held_frame, (const void *)g_frame_start, used < FARENA ? used : FARENA);
			}
			if (g_frame_start) FramePackets() = (uint32_t)g_held_frame;
			CaptureCursor() = (uint32_t)g_held_capture;
			EffectCameraMatrix(&Camera(), OrthoView());
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
				switch (r.kind)
				{
				case R_SPRITE: HeldSprite(r, num, den, at_next); break;
				case R_LAYOUT: HeldLayout(r, num, den, at_next); break;
				case R_BONE: HeldBone(r, num, den, at_next); break;
				case R_GHOSTS: HeldGhosts(r, num, den, at_next); break;
				case R_ENTITY: HeldTarget(r, num, den, at_next); break;
				case R_CAPTURE: HeldCapture(r, at_next); break;
				case R_THREE: HeldThree(r, num, den, at_next); break;
				case R_TILE: HeldTile(r, at_next); break;
				case R_TRAIL: HeldTrail(r, num, den, at_next); break;
				case R_TASKLAYOUT: HeldTaskLayout(r, num, den, at_next); break;
				}
			}
			g_hdr_slot = nullptr;
		}
		g_in_held = false;
		FramePackets() = frame_saved;
		SetScreenOffset(ox, oy);
		var<uint16_t>(0x1D9770C) = w70c;
		if (vb) memcpy((void *)vb, vbuf, sizeof(vbuf));
		memcpy((void *)FLASH_LO, flash, sizeof(flash));
		memcpy((void *)0x1D99C18, c18, sizeof(c18));
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}

	// ---- the targets, drawn by the engine from the put-back (0xEB) to the end: raised 0x200 a tick ----
	static int HeldEntities(HeldEntity *out, int max)
	{
		if (!HeldReady() || g_main_c < 0xEB || !g_main) return 0;
		int n = 0;
		for (int k = 0; k < g_main->ntargets && n < max; k++)
		{
			HeldEntity &h = out[n++];
			held_entity_init(h, g_main->recs[k].e);
			if (g_main_c == 0xEB) for (int q = 0; q < 3; q++) held_entity_jump(h, q);
		}
		return n;
	}
}

	static void register_mag210_held()
	{
		mark_held(blood210::ORIG_FlameTask);
		mark_held(blood210::ORIG_SparkTask);
		mark_held(blood210::ORIG_EmberTask);
		mark_held(blood210::ORIG_MainTask);
		mark_held(blood210::ORIG_TrailTask);
		mark_held(blood210::ORIG_LayoutTask);
		mark_held(blood210::ORIG_RockTask);
		mark_held(blood210::ORIG_SparkleTask);
		register_module_held(210, blood210::HeldReady, blood210::HeldFrame);
		register_module_entity(210, blood210::HeldEntities);
	}
}
