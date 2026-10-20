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

// 30 fps layer of Blasting Zone (effect 164), included at the end of mag164_blasting_zone.cpp.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with, the Field_Alloc scratch under its draw header, and for the pool
// tasks the pool as drawn (after the tick's spawns, before the update) and their header as set up. A held frame replays
// the log into private packet buffers (module cursor 0x23CA118 and frame arena 0x1D8E054, each holding the real tick's
// packets under the replay), each draw at its exact in-between state (logic stays 15 Hz), with the LIVE camera (the
// held frame's view in 0x1D97778: every draw composes with it, as the real draws do with theirs):
//   Single models - angles, scales, positions (Rock's x drift, ScreenFlash's y) and Wave / Wave2 scroll half way to the
//     values the update left (the next draw's); fades half way while the draw mode stays (a mode switch on the next
//     tick: held). The effect frame products (camera x frame 0x23C9D68: FrameModel, Wave2, Smoke) are rebuilt from the
//     live camera into a private matrix; the director's pulsing colour (Dome, Dome2, Morph, BladeGlow) is half way per
//     channel to the next tick's (the director's DPCS at its next counter).
//   BladeGlow - its anchor follows the weapon: the weapon's bone 1 position / blade direction half way to the NEXT
//     tick's (the weapon at its next pose: pose_midpoint at 1, through the caster's next matrix: the frame gate moves a
//     moving caster to its in-between position, so its next translation is tick N's + (now - tick N's) * den / num;
//     the real pose rebuilt afterwards), the direction renormalised.
//   Morph - the y scale half way, the 105 morphed vertices half way between this tick's and the next tick's (the
//     morph's own formula at counter + 1) on a private vertex buffer; its matrix (camera-facing, live camera) on a
//     private matrix: 0x23C9DA8 / 0x23C9DC8 keep the real tick's.
//   Particles (pools A / B / C) - each task's pool as drawn: the entries that advanced on the tick (same type, life + 1)
//     half way to their post-update orbit angle (OrbitSpark), scroll / angle (Swirl), position (MorphSpark, BurstSpark),
//     y scale (Shard); fades half way while the draw mode stays; flipbook frames, the dots' colours (by life) switch on
//     the real frame; an entry that died on the tick holds its last picture; the tick's spawns are drawn at birth.
//     MorphSpark's dots go through the morph matrix of the morph that draws last before them: the held one of the
//     morph that also draws last on the next tick (its y scale half way between the two ticks' last morphs), the
//     real tick's 0x23C9DA8 when no morph draws on the next tick (the next tick reuses it).
//   Starfield - static dots (the pool as drawn).
// Camera: the shared camera-script task 0x63E9C0 (camscript_held_camera, Doomtrain's port). The camera shake word
// 0x1D97712 (Shake) is a 15 Hz sign-alternating step: the frame gate blends the real ticks' offsets itself.
// The director, the spawner, the shake and the texture restore draw nothing; the caster's animation and the entity /
// stage flags are drawn by the engine. Nothing here writes game state: the scope puts back the cursors, the GTE
// registers, the Field_Alloc scratch and the shadow-rotation scratch; the weapon's bone matrices are rebuilt from the
// real pose after the look-ahead.

namespace ff8fx
{
namespace blast164
{
	static uint32_t g_ported_tick = 0xFFFFFFFFu;
	static bool g_tick_paused = false;   // the last real tick ran draw-only (battle_to_update_flags & 0x201)
	static uint32_t g_pkt_start = 0, g_fa_start = 0; // the cursors when the root started the tick's queue
	static uint8_t g_held_packets[0x20000], g_held_fa[0x20000];

	enum RecKind { REC_NODE, REC_GLOW, REC_POOL };
	static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
	struct HRec
	{
		int kind;
		uint32_t fn;
		const Node24 *node;
		Node24 copy;               // the node as drawn
		uint8_t gte[GTE_BYTES];    // GTE state the draw started with
		uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated (pools: the header as set up)
		Mat4x3 mat;                // the caster's matrix at the draw
		int32_t dir[4];            // BladeGlow: the blade direction it was drawn with
		int16_t pos[4];            // BladeGlow: the weapon point
		uint32_t pool, pool_end;   // pools
		uint32_t snap;             // pools: offset of the pool as drawn in g_snap
	};
	static const int REC_MAX = 64;
	static HRec g_rec[REC_MAX];
	static int g_nrec = 0;
	static uint8_t g_snap[0x8000];
	static uint32_t g_nsnap = 0;
	// the director as it ran on the tick (the pulsing colour)
	static const Node24 *g_dir_node = nullptr;
	static Node24 g_dir_copy;
	static uint32_t g_dir_colour = 0;
	static bool g_dir_noted = false;

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = DrawOnly();
		g_pkt_start = PacketCursor();
		g_fa_start = FrameArena();
		g_nrec = 0;
		g_nsnap = 0;
		g_dir_noted = false;
	}

	static void held_note_director(const Node24 *n)
	{
		if (g_ported_tick != g_real_tick) return;
		g_dir_node = n;
		g_dir_copy = *n;
		g_dir_colour = Colour();
		g_dir_noted = true;
	}

	static HRec *NoteRec(int kind, uint32_t fn, const Node24 *n, bool header)
	{
		magc::g_hdr_slot = nullptr;
		if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		HRec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.fn = fn;
		r.node = n;
		memcpy(&r.copy, n, sizeof(Node24));
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		r.mat = *(const Mat4x3 *)(Caster() + 0x40);
		if (header) magc::g_hdr_slot = r.hdr;
		return &r;
	}

	static void held_note_node(uint32_t fn, const Node24 *n) { NoteRec(REC_NODE, fn, n, true); }

	static void held_note_glow(const Node24 *n, const int32_t *dir, const int16_t *pos)
	{
		HRec *r = NoteRec(REC_GLOW, ORIG_BladeGlow, n, true);
		if (!r) return;
		memcpy(r->dir, dir, sizeof(r->dir));
		memcpy(r->pos, pos, sizeof(r->pos));
	}

	static uint32_t PoolOf(uint32_t fn, uint32_t *end)
	{
		if (fn == ORIG_Shard || fn == ORIG_Smoke) { *end = POOL_B_END; return POOL_B; }
		if (fn == ORIG_Starfield) { *end = POOL_C_END; return POOL_C; }
		*end = POOL_A_END;
		return POOL_A;
	}

	static uint32_t PoolHeaderSize(uint32_t fn)
	{
		if (fn == ORIG_Swirl) return 0x7C;
		if (fn == ORIG_Shard) return 0x58;
		if (fn == ORIG_MorphSpark || fn == ORIG_Starfield) return 0;
		return 0xB4;
	}

	static void held_note_pool(uint32_t fn, const Node24 *n, const uint8_t *h)
	{
		uint32_t end;
		const uint32_t pool = PoolOf(fn, &end);
		if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
		HRec *r = NoteRec(REC_POOL, fn, n, false);
		if (!r) return;
		r->pool = pool;
		r->pool_end = end;
		r->snap = g_nsnap;
		memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
		g_nsnap += end - pool;
		if (h) memcpy(r->hdr, h, PoolHeaderSize(fn));
	}

	// a held draw starts like the real one: its GTE state and header scratch
	static void Replay(const HRec &r, bool header)
	{
		memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
		memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
		magc::g_hdr_slot = header ? (uint8_t *)r.hdr : nullptr;
	}

	// the node still runs the same task on the next tick (it did not end on this one) and advanced
	static bool Alive(const HRec &r) { return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == r.fn && r.node->c == (int16_t)(r.copy.c + 1); }

	static inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return magc::lerp16(a, b, num, den); }

	// the caster's matrix translation of the next tick (the frame gate moved a moving caster to its in-between position:
	// tick N's + (now - tick N's) * den / num), put in place for a look-ahead; restored by the caller
	static void NextCasterMatrix(Mat4x3 *em, const Mat4x3 &at_tick, int num, int den)
	{
		const Mat4x3 now = *em;
		for (int k = 0; k < 3; k++) em->t[k] = at_tick.t[k] + (int32_t)((int64_t)(now.t[k] - at_tick.t[k]) * den / num);
	}

	// ---- held frame surroundings: the module cursor (magc::HeldScope: + scratch, shadow scratch, GTE) and the frame arena
	struct HeldScope164
	{
		magc::HeldScope base;
		uint32_t fa_saved;
		HeldScope164() : base(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start)
		{
			fa_saved = FrameArena();
			if (g_fa_start && fa_saved > g_fa_start)
			{
				const uint32_t used = fa_saved - g_fa_start;
				memcpy(g_held_fa, (const void *)g_fa_start, used < sizeof(g_held_fa) ? used : sizeof(g_held_fa));
			}
			FrameArena() = (uint32_t)g_held_fa;
		}
		~HeldScope164() { FrameArena() = fa_saved; }
	};

	// ---- the director's products ----
	static Mat4x3 g_cam_frame;     // camera (live) x effect frame
	static uint32_t g_colour = 0;  // the pulsing colour of the frame

	// the director's colour at counter c (its DPCS; the GTE is put back by the scope)
	static uint32_t DirectorColour(int16_t c)
	{
		const int32_t ir0 = (ComputeCos(shl32(c, 9)) + 0x1000) >> 1;
		uint32_t col = 0x30587F;
		GteSetFarColor(0x30, 0x30, 0x80);
		GteLoadRGBC(&col);
		GteSetIR0(ir0);
		GteDPCS();
		GteStoreRGB2(&col);
		return col;
	}

	static uint32_t ColourLerp(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000u;
		for (int k = 0; k < 24; k += 8)
		{
			const int32_t x = (int32_t)(a >> k & 0xFF), y = (int32_t)(b >> k & 0xFF);
			r |= (uint32_t)(lerp_i(x, y, num, den) & 0xFF) << k;
		}
		return r;
	}

	static void FrameProducts(int num, int den, bool next)
	{
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)FRAME, &g_cam_frame);
		g_colour = g_dir_noted ? g_dir_colour : Colour();
		const Node24 *d = g_dir_node;
		if (g_dir_noted && next && (d->hdr.flags & 1) && (uint32_t)d->hdr.func == ORIG_Director && d->c == (int16_t)(g_dir_copy.c + 1))
			g_colour = ColourLerp(g_dir_colour, DirectorColour(d->c), num, den);
	}

	// ---- single models ----
	// fade half way when the draw mode stays (both ticks in the same fade ramp), else tick N's
	static int32_t FadeLerp(bool same, int32_t f0, int32_t f1, bool next, int num, int den) { return next && same ? lerp_i(f0, f1, num, den) : f0; }

	// the weapon's bone 1 on the next tick: the weapon at its next pose through the caster's next matrix
	static bool GlowNext(const HRec &r, int num, int den, int32_t *dir, int16_t *pos)
	{
		uint8_t *e = Caster();
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (!w) return false;
		Mat4x3 *em = (Mat4x3 *)(e + 0x40);
		const Mat4x3 now = *em;
		NextCasterMatrix(em, r.mat, num, den);
		pose_midpoint((void *)w, (void *)(w + 0xC), 1, 1);
		WeaponBone(e, 1, dir, pos);
		BuildBoneMatricesFromPose(w);
		*em = now;
		return true;
	}

	static uint8_t g_morph_vtx[0x69 * 8], g_morph_vtx1[0x69 * 8];
	static Mat4x3 g_morph_mat;

	// the morph's vertices half way between counter c's and c + 1's (a private buffer; the pad words as the real buffer's)
	static void MorphHeldVertices(int32_t c, bool next, int num, int den)
	{
		memcpy(g_morph_vtx, (const void *)MORPH_VTX, sizeof(g_morph_vtx));
		MorphVertices(c, g_morph_vtx);
		if (!next) return;
		memcpy(g_morph_vtx1, g_morph_vtx, sizeof(g_morph_vtx1));
		MorphVertices(c + 1, g_morph_vtx1);
		const int32_t count = var<int32_t>(MORPH_COUNT);
		for (int32_t k = 0; k < count && k < 0x69; k++)
			for (int q = 0; q < 2; q++)
			{
				int16_t *a = (int16_t *)(g_morph_vtx + 8 * k) + q;
				*a = Lerp16(*a, *((const int16_t *)(g_morph_vtx1 + 8 * k) + q), num, den);
			}
	}

	// a single model's held draw inputs: angles, scales, position words, fade, scroll and colour
	struct NodeIn
	{
		int16_t a, b;      // angles (yaw / roll; Beam: its second yaw)
		int16_t sx, sy;    // x (and z) / y scale
		int16_t x, y;      // Rock: x; ScreenFlash: y
		int32_t fade, scroll;
		uint32_t colour;   // the pulsing colour (module renderer draws)
	};

	static void NodeInputs(const HRec &r, int num, int den, NodeIn *in)
	{
		const Node24 &d = r.copy;
		const Node24 *now = r.node;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		const int32_t c = d.c;
		auto L = [&](int16_t a, int16_t b) { return next ? Lerp16(a, b, num, den) : a; };
		in->a = d.w18;
		in->b = d.w1A;
		in->sx = d.w1C;
		in->sy = d.w20;
		in->x = d.w10;
		in->y = d.w12;
		in->fade = 0;
		in->scroll = 0;
		in->colour = g_colour;
		switch (r.fn)
		{
		case ORIG_FlareA:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c >= 8, mul32(c - 8, 682), mul32(c + 1 - 8, 682), next, num, den);
			break;
		case ORIG_FlareB:
			in->a = L(d.w18, now->w18);
			in->fade = FadeLerp(c + 1 < 8, shl32(8 - c, 9), shl32(8 - c - 1, 9), next, num, den);
			break;
		case ORIG_Ring:
			in->a = L(d.w18, now->w18);
			in->sy = L(d.w20, now->w20);
			in->fade = FadeLerp(c + 1 < 6 || c >= 0x18, RingFade(c), RingFade(c + 1), next, num, den);
			break;
		case ORIG_PulseA:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->sy = L(d.w20, now->w20);
			in->fade = FadeLerp(c >= 6, shl32(c - 6, 9), shl32(c + 1 - 6, 9), next, num, den);
			break;
		case ORIG_PulseB:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c >= 4, shl32(c - 4, 9), shl32(c + 1 - 4, 9), next, num, den);
			break;
		case ORIG_Ball:
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c >= 0x14, shl32(c - 0x14, 9), shl32(c + 1 - 0x14, 9), next, num, den);
			break;
		case ORIG_FrameModel:
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c >= 4, mul32(c - 4, 682), mul32(c + 1 - 4, 682), next, num, den);
			break;
		case ORIG_Dome:
			in->sx = L(d.w1C, now->w1C);
			break;
		case ORIG_Dome2:
			in->sy = L(d.w20, now->w20);
			break;
		case ORIG_Beam:
			in->a = L(d.w18, now->w18);
			in->b = L(d.w1A, now->w1A);
			break;
		case ORIG_Rock:
			in->x = L(d.w10, now->w10);
			break;
		case ORIG_Debris:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->sy = L(d.w20, now->w20);
			in->fade = FadeLerp(c >= 0xC, shl32(c - 0xC, 9), shl32(c + 1 - 0xC, 9), next, num, den);
			break;
		case ORIG_ScreenFlash:
			in->y = L(d.w12, now->w12);
			in->fade = FadeLerp(true, shl32(0x50 - c, 5), shl32(0x50 - c - 1, 5), next, num, den);
			break;
		case ORIG_Morph:
			in->sy = L(d.w20, now->w20);
			break;
		case ORIG_RingFlat:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c + 1 < 4, shl32(4 - c, 0xA), shl32(4 - c - 1, 0xA), next, num, den);
			break;
		case ORIG_RingFlat2:
			in->a = L(d.w18, now->w18);
			in->sx = L(d.w1C, now->w1C);
			in->fade = FadeLerp(c >= 2, shl32(c - 2, 8), shl32(c + 1 - 2, 8), next, num, den);
			break;
		case ORIG_Wave:
			in->scroll = FadeLerp(true, mul32(c, 24), mul32(c + 1, 24), next, num, den);
			in->fade = FadeLerp(c >= 0xE, mul32(c - 0xE, 341), mul32(c + 1 - 0xE, 341), next, num, den);
			break;
		case ORIG_Wave2:
			in->scroll = L(d.w1C, now->w1C);
			in->sy = L(d.w20, now->w20);
			in->fade = FadeLerp(c >= 0x12, shl32(c - 0x12, 9), shl32(c + 1 - 0x12, 9), next, num, den);
			break;
		default:
			break;
		}
	}

	static void NodeDraw(const HRec &r, const NodeIn &in, int num, int den)
	{
		const Node24 &d = r.copy;
		const int32_t c = d.c;
		Replay(r, true);
		switch (r.fn)
		{
		case ORIG_FlareA: FlareADraw(&d, in.a, in.sx, c, in.fade); break;
		case ORIG_FlareB: FlareBDraw(&d, in.a, in.sx, c, in.fade); break;
		case ORIG_Ring: RingDraw(&d, in.a, in.sy, c, in.fade); break;
		case ORIG_PulseA: PulseDraw(&d, in.a, in.sx, in.sy, c >= 6, in.fade); break;
		case ORIG_PulseB: PulseDraw(&d, in.a, in.sx, in.sy, c >= 4, in.fade); break;
		case ORIG_Ball: BallDraw(&d, in.sx, c, in.fade); break;
		case ORIG_FrameModel: FrameModelDraw(&d, in.sx, c, in.fade, &g_cam_frame); break;
		case ORIG_Dome: PacketCursor() = DomeDraw(&d, in.sx, in.sy, in.colour, 3, PacketCursor()); break;
		case ORIG_Dome2: FrameArena() = DomeDraw(&d, in.sx, in.sy, in.colour, 2, FrameArena()); break;
		case ORIG_Beam: BeamDraw(&d, in.a, in.b); break;
		case ORIG_Rock:
		{
			const int16_t pos[3] = { in.x, d.w12, d.w14 };
			RockDraw(&d, pos);
			break;
		}
		case ORIG_Debris: DebrisDraw(&d, in.a, in.sx, in.sy, c, in.fade); break;
		case ORIG_ScreenFlash: ScreenFlashDraw(in.y, in.sx, in.fade); break;
		case ORIG_Morph:
			MorphMatrix(&d, in.sy, &g_morph_mat);
			MorphHeldVertices(c, num != 0 && !g_tick_paused && Alive(r), num, den);
			PacketCursor() = MorphRender(&d, g_morph_vtx, in.colour, PacketCursor());
			break;
		case ORIG_RingFlat: RingFlatDraw(&d, MODEL_RINGFLAT, in.a, in.sx, c < 4, in.fade); break;
		case ORIG_RingFlat2: RingFlatDraw(&d, MODEL_RINGFLAT2, in.a, in.sx, c >= 2, in.fade); break;
		case ORIG_Wave: WaveDraw(&d, c, in.scroll, in.fade); break;
		case ORIG_Wave2: Wave2Draw(&d, (int16_t)in.scroll, in.sy, c, in.fade, &g_cam_frame); break;
		default: break;
		}
	}

	static void NodeHeld(const HRec &r, int num, int den)
	{
		NodeIn in;
		NodeInputs(r, num, den, &in);
		NodeDraw(r, in, num, den);
	}

	static void GlowHeld(const HRec &r, int num, int den)
	{
		const Node24 &d = r.copy;
		const int32_t c = d.c;
		const bool next = num != 0 && !g_tick_paused && Alive(r);
		int32_t dir[4];
		int16_t pos[4];
		memcpy(dir, r.dir, sizeof(dir));
		memcpy(pos, r.pos, sizeof(pos));
		int32_t dn[4] = {};
		int16_t pn[4] = {};
		if (next && GlowNext(r, num, den, dn, pn))
		{
			for (int k = 0; k < 3; k++)
			{
				dir[k] = lerp_i(r.dir[k], dn[k], num, den);
				pos[k] = Lerp16(r.pos[k], pn[k], num, den);
			}
			Normalize(dir, dir);
		}
		const int32_t fade = FadeLerp(c >= 0x39, shl32(c - 0x39, 9), shl32(c + 1 - 0x39, 9), next, num, den);
		Replay(r, true);
		BladeGlowDraw(&d, dir, pos, g_colour, c, fade);
	}

	// ---- particles: the draws of the pool tasks from the pools as drawn ----
	// the entry advanced on the tick (still the type, life + 1): its post-update state is the next draw's
	static const PoolE *NextEntry(const HRec &r, uint32_t o, uint32_t type, const PoolE *p, bool move)
	{
		if (!move) return nullptr;
		const PoolE *now = (const PoolE *)(r.pool + o);
		return (now->flags & type) && now->life == (int16_t)(p->life + 1) ? now : nullptr;
	}

	// MorphSpark's matrix on the held frame (see the header)
	static void MorphSparkMatrix(const HRec &spark, int rec_index, int num, int den, Mat4x3 *out)
	{
		*out = *(const Mat4x3 *)MORPH_MAT;
		const HRec *last = nullptr;
		for (int i = rec_index - 1; i >= 0 && !last; i--)
			if (g_rec[i].fn == ORIG_Morph) last = &g_rec[i];
		if (!last || num == 0 || g_tick_paused) return;
		// the last morph drawing on the next tick before the spark task (queue order): alive, its delay over
		const Node24 *nx = nullptr;
		for (const TaskNode *t = QMain()->head; t && t != &spark.node->hdr; t = t->next)
			if ((t->flags & 1) && (uint32_t)t->func == ORIG_Morph && ((const Node24 *)t)->e == 0) nx = (const Node24 *)t;
		if (!nx) return;
		// (the same morph: its own y scale half way; another one: half way from the last drawn to the next drawer's)
		MorphMatrix(nx, Lerp16(last->copy.w20, nx->w20, num, den), out);
	}

	// one sprite of OrbitSpark / BurstSpark / Smoke as its task draws it
	static void SpriteHeld(uint32_t fn, ModelWork *w, uint8_t *h, const PoolE *q)
	{
		if (fn == ORIG_OrbitSpark) OrbitDraw((OrbitWork *)w, h, q, q->a[4]);
		else if (fn == ORIG_BurstSpark)
		{
			SpriteAt(h, &q->a[0], q->scale, q->life);
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		else SmokeDraw(w, h, q);
	}

	static void PoolHeld(const HRec &r, int rec_index, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		uint8_t *snap = g_snap + r.snap;
		const uint32_t size = r.pool_end - r.pool;
		Replay(r, false);
		if (r.fn == ORIG_Starfield)
		{
			FieldAlloc(0x48);
			FrameArena() = StarfieldDraw((uint32_t)snap, (uint32_t)snap + size, FrameArena());
			FieldFree(0x48);
			return;
		}
		if (r.fn == ORIG_MorphSpark)
		{
			ModelWork *w = (ModelWork *)FieldAlloc(0x58);
			Mat4x3 m;
			MorphSparkMatrix(r, rec_index, num, den, &m);
			MorphSparkSetup(&m);
			uint32_t pk = FrameArena();
			w->v[0] = (int32_t)RenderOT();
			w->v[1] = 4;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const PoolE *p = (const PoolE *)(snap + o);
				if (!(p->flags & 1)) continue;
				PoolE q = *p;
				if (const PoolE *now = NextEntry(r, o, 1, p, move))
					for (int k = 0; k < 3; k++) q.a[k] = Lerp16(p->a[k], now->a[k], num, den);
				pk = MorphSparkDraw(w, &q, &q.a[0], pk, false);
			}
			FrameArena() = pk;
			FieldFree(0x58);
			return;
		}
		const uint32_t hsize = PoolHeaderSize(r.fn);
		if (r.fn == ORIG_Shard)
		{
			ModelWork *w = (ModelWork *)FieldAlloc(0x68);
			uint8_t *h = (uint8_t *)FieldAlloc(hsize);
			memcpy(h, r.hdr, hsize);
			ShardFrame(w, &Camera());
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const PoolE *p = (const PoolE *)(snap + o);
				if (!(p->flags & 1)) continue;
				PoolE q = *p;
				const int32_t life = p->life;
				int32_t fade = mul32(life - 4, 682);
				if (const PoolE *now = NextEntry(r, o, 1, p, move))
				{
					q.a[5] = Lerp16(p->a[5], now->a[5], num, den);
					if (life >= 4) fade = lerp_i(fade, mul32(life + 1 - 4, 682), num, den);
				}
				ShardDraw(w, h, &q, q.a[5], life, fade);
			}
			FieldFree(hsize);
			FieldFree(0x68);
			return;
		}
		if (r.fn == ORIG_Swirl)
		{
			ModelWork *w = (ModelWork *)FieldAlloc(0x68);
			uint8_t *h = (uint8_t *)FieldAlloc(hsize);
			memcpy(h, r.hdr, hsize);
			TransformCameraByShadowRotation((const void *)POINT, 0x1000, 0);
			w->v[2] = 0x1000;
			w->v[1] = 0x1000;
			w->ang[1] = 0;
			w->ang[0] = 0;
			for (uint32_t o = 0; o < size; o += 0x18)
			{
				const PoolE *p = (const PoolE *)(snap + o);
				if (!(p->flags & 2)) continue;
				PoolE q = *p;
				const int32_t life = p->life;
				int32_t fade = SwirlFade(life);
				if (const PoolE *now = NextEntry(r, o, 2, p, move))
				{
					q.a[2] = Lerp16(p->a[2], now->a[2], num, den);
					q.a[1] = Lerp16(p->a[1], now->a[1], num, den);
					if (life + 1 < 6 || life >= 0xA) fade = lerp_i(fade, SwirlFade(life + 1), num, den);
				}
				SwirlDraw(w, h, q.a[2], q.scale, life, q.a[1], fade);
			}
			FieldFree(hsize);
			FieldFree(0x68);
			return;
		}
		// sprite pools (OrbitSpark, BurstSpark, Smoke)
		const uint32_t wsize = r.fn == ORIG_OrbitSpark ? 0x68 : 0x48;
		ModelWork *w = (ModelWork *)FieldAlloc(wsize);
		uint8_t *h = (uint8_t *)FieldAlloc(hsize);
		memcpy(h, r.hdr, hsize);
		if (r.fn == ORIG_OrbitSpark) PoolLight(&((OrbitWork *)w)->m1, ((OrbitWork *)w)->pos, PointX(), var<int16_t>(POINT + 2), PointZ(), &Camera());
		else if (r.fn == ORIG_BurstSpark) PoolLight(&w->m1, w->ang, PointX(), 0, PointZ(), &Camera());
		else PoolLight(&w->m1, w->ang, PointX(), 0, PointZ(), &g_cam_frame);
		for (uint32_t o = 0; o < size; o += 0x18)
		{
			const PoolE *p = (const PoolE *)(snap + o);
			if (!(p->flags & 1)) continue;
			PoolE q = *p;
			if (const PoolE *now = NextEntry(r, o, 1, p, move))
			{
				if (r.fn == ORIG_OrbitSpark) q.a[4] = Lerp16(p->a[4], now->a[4], num, den);
				else if (r.fn == ORIG_BurstSpark) for (int k = 0; k < 3; k++) q.a[k] = Lerp16(p->a[k], now->a[k], num, den);
			}
			SpriteHeld(r.fn, w, h, &q);
		}
		FieldFree(hsize);
		FieldFree(wsize);
	}

	// the held-frame camera: the shared camera-script task's prediction (Doomtrain's port) runs the script's maths on the
	// Field_Alloc scratch stack (its 0x1B0-byte work block): the scratch above the pointer is put back as the real tick
	// left it (the next real tick's draw headers read what their fields were not set to)
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		static uint8_t save[0x1000];
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		if (scratch) memcpy(save, scratch, sizeof(save));
		const bool r = camscript_held_camera(num, den, world, lookat);
		if (scratch) memcpy(scratch, save, sizeof(save));
		return r;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		HeldScope164 scope;
		FrameProducts(num, den, num != 0 && !g_tick_paused);
		for (int i = 0; i < g_nrec; i++)
		{
			const HRec &r = g_rec[i];
			switch (r.kind)
			{
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_GLOW: GlowHeld(r, num, den); break;
			case REC_POOL: PoolHeld(r, i, num, den); break;
			}
		}
	}
}

	static void register_mag164_held()
	{
		using namespace blast164;
		mark_held(ORIG_Root);
		mark_held(ORIG_FlareA);
		mark_held(ORIG_FlareB);
		mark_held(ORIG_OrbitSpark);
		mark_held(ORIG_Swirl);
		mark_held(ORIG_Ring);
		mark_held(ORIG_PulseA);
		mark_held(ORIG_PulseB);
		mark_held(ORIG_Shard);
		mark_held(ORIG_Ball);
		mark_held(ORIG_FrameModel);
		mark_held(ORIG_Dome);
		mark_held(ORIG_Dome2);
		mark_held(ORIG_Beam);
		mark_held(ORIG_Rock);
		mark_held(ORIG_Debris);
		mark_held(ORIG_Starfield);
		mark_held(ORIG_ScreenFlash);
		mark_held(ORIG_Morph);
		mark_held(ORIG_RingFlat);
		mark_held(ORIG_RingFlat2);
		mark_held(ORIG_MorphSpark);
		mark_held(ORIG_BladeGlow);
		mark_held(ORIG_BurstSpark);
		mark_held(ORIG_Wave);
		mark_held(ORIG_Wave2);
		mark_held(ORIG_Smoke);
		register_module_held(164, HeldReady, HeldFrame);
		// the camera: the shared camera-script task 0x63E9C0 (Doomtrain's port), scratch put back
		register_module_camera(164, HeldCamera);
	}
}
