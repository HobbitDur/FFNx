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

// 30 fps part of mag112_meteor_barret.cpp (Zell's Meteor Barret): the draw log of the real tick and the held-frame drawing
// (included at the end of mag112_meteor_barret.cpp; shared pieces in fx_held.h / mag_common_held.h).
//
// A real tick records every draw of the module in order (the root's seam starts the log) with the GTE registers and the
// Field_Alloc scratch it started from; a held frame at tick + num / den replays them in the same order, with the vanilla draw
// code, each at its in-between state (logic stays 15 Hz), with the LIVE camera (the held frame's view in 0x1D97778: every draw
// composes with it, as the real draws do with theirs; the director writes no camera word):
//   Layouts   - through prim::play_held (the exact in-between records of what the player drew). Their frames: the ground
//               layout at the ground point (fixed), the two hand layouts at the midpoint of Zell's bones 0x15 / 0x16 and the
//               fist layout at his bone 0x15 follow his bones: their next tick's anchor is measured at his NEXT pose
//               (pose_midpoint at 1: the look-ahead of the animation reader, with the world matrix the frame holds; his real
//               pose is put back), the held anchor num / den of the way; the hand layouts' radial word switches on the real
//               frame (at 1 the next tick's); the impact / target frames are fixed points, the billboard is rebuilt from the
//               live view (its unrotated frame keeps the impact frame's last element, as the director's local does).
//               Blended vertex frames go to a private buffer.
//   Column    - its spin (0x40 a tick) num / den of the way, its tip (Zell's spawn point 0xF1, at his NEXT pose) num / den of
//               the way: the bend length and the height scale follow; the fade (from its 7th tick) num / den of the way; the
//               draw mode switches on the real frame. Bent into a private copy of the real tick's vertex buffer.
//   Tiles     - the full-screen fades: each colour num / den of the way to the next tick's (the fade's next level; black
//               when the next tick ends it: not drawn at 1).
//   Flat quad - the grey flash of tick 107 holds up to 1 (not drawn at 1).
//   Smoke / sparks / debris - (draw then update) the node num / den of the way to its update (sparks' positions, debris'
//               positions and angles with the gravity and the bounce); flipbook frames switch on the real frame; a node that
//               ends on the next tick is not drawn at 1.
// A part the next tick will not draw is held up to 1 and not drawn at 1. Zell's / the target's positions, flags and colours,
// the screen flash / fades and the camera animation are the engine's. Nothing here writes game state: the module globals,
// the packet cursor (a private buffer holding the real tick's packets under the replay), the Field_Alloc scratch, the
// shadow-rotation scratch, the render scratch words, the word 0x1D9770C, the GTE registers and the look-ahead bone matrices
// are put back.

namespace ff8fx
{
namespace meteor112
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;   // one packet arena (magic buffer + 0x1E88 / + 0x11E88)
	static const uint32_t BSS_LO = 0x25218E0, BSS_HI = 0x2521970;
	static const uint32_t COLUMN_BYTES = 0x680; // 208 vertices
	enum : uint8_t { REC_LAYOUT = 1, REC_COLUMN, REC_TILE, REC_FLAT, REC_SMOKE, REC_SPARK, REC_DEBRIS };

	struct Rec
	{
		uint8_t kind;
		const DirectorNode *dir; // director parts
		int site;             // layout site
		PlayArg arg;          // layout: the block as handed to the player
		int16_t anchor[3];    // layout: the bone anchor drawn (hands: the block's position; fist: the frame's translation)
		int32_t k;            // column: its tick (counter - 10)
		int16_t spawn[3];     // column: its tip (Zell's spawn point 0xF1)
		int32_t rgb[3], n, d; // tile
		uint8_t node[0x30];   // particles: the node as drawn
		int extra;            // column: index in g_cols
		int scratch;          // index in g_scratch
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static uint32_t g_pred_tick = 0xFFFFFFFF;
	static bool g_in_held = false;
	static int g_nrec = 0, g_nscratch = 0, g_ncols = 0;
	static uint32_t g_packets = 0;
	static Rec g_recs[0x200];
	static uint8_t g_scratch[0x200][SCRATCH];
	static uint8_t g_cols[2][COLUMN_BYTES];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_morph[0x8000];
	static uint8_t g_held_col[COLUMN_BYTES];
	// the next-pose predictions of the real tick (made at its first held frame)
	static bool g_next_hands_ok = false, g_next_fist_ok = false, g_next_spawn_ok = false;
	static int16_t g_next_hands[4], g_next_fist[4], g_next_spawn[4];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		if (g_in_held) return;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nscratch = 0;
		g_ncols = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint8_t kind)
	{
		if (g_in_held || g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nscratch >= (int)(sizeof(g_scratch) / sizeof(g_scratch[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.dir = nullptr;
		r.extra = -1;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		r.scratch = g_nscratch++;
		memcpy(g_scratch[r.scratch], var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_layout(const DirectorNode *d, int site, const PlayArg *arg)
	{
		Rec *r = NewRec(REC_LAYOUT);
		if (!r) return;
		r->dir = d;
		r->site = site;
		r->arg = *arg;
		memcpy(r->anchor, site == SITE_FIST ? d->fist : arg->pos, 6);
	}

	static void held_note_tile(int32_t r_, int32_t g, int32_t b, int32_t n, int32_t d)
	{
		Rec *r = NewRec(REC_TILE);
		if (!r) return;
		r->rgb[0] = r_;
		r->rgb[1] = g;
		r->rgb[2] = b;
		r->n = n;
		r->d = d;
	}

	static void held_note_column(const DirectorNode *d, int32_t k, const int16_t *spawn)
	{
		if (g_ncols >= (int)(sizeof(g_cols) / sizeof(g_cols[0]))) return;
		Rec *r = NewRec(REC_COLUMN);
		if (!r) return;
		r->dir = d;
		r->k = k;
		memcpy(r->spawn, spawn, 6);
		r->extra = g_ncols++;
		memcpy(g_cols[r->extra], (const void *)(TexBase() + 0x1808), COLUMN_BYTES); // (the bend keeps the 4th words)
	}

	static void held_note_flat() { NewRec(REC_FLAT); }

	static void held_note_smoke(const SmokeNode *s)
	{
		Rec *r = NewRec(REC_SMOKE);
		if (r) memcpy(r->node, s, sizeof(SmokeNode));
	}
	static void held_note_spark(const SparkNode *s)
	{
		Rec *r = NewRec(REC_SPARK);
		if (r) memcpy(r->node, s, sizeof(SparkNode));
	}
	static void held_note_debris(const DebrisNode *d)
	{
		Rec *r = NewRec(REC_DEBRIS);
		if (r) memcpy(r->node, d, sizeof(DebrisNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ---- the next tick (from the state the real tick left)
	// the director that runs on the next tick (the counter it runs with); null: it ended (or none runs)
	static const DirectorNode *NextDirector(int16_t *counter)
	{
		const TaskNode *n = QDirector()->head;
		if (!n) return nullptr;
		const DirectorNode *d = (const DirectorNode *)n;
		*counter = d->counter;
		return d;
	}
	static bool SiteActive(int site, int16_t c)
	{
		switch (site)
		{
		case SITE_GROUND: return c >= 0xB && c <= 0x19;
		case SITE_HAND_A: return c >= 0x11 && c <= 0x34;
		case SITE_HAND_B: return c >= 0x16 && c <= 0x34;
		case SITE_FIST: return c >= 0x30 && c < 0x62;
		case SITE_IMPACT_A: case SITE_BILLBOARD: return c >= 0x45 && c < 0x86;
		case SITE_IMPACT_B: return c >= 0x6E && c < 0x86;
		case SITE_TARGET: return c >= 0x6B && c < 0x86;
		}
		return false;
	}

	// Zell at his next pose (pose_midpoint at 1): the hands' midpoint, his bone 0x15 and his spawn point 0xF1 of the next tick;
	// his bone matrices, 0x1D9770C, the Field_Alloc scratch and the GTE registers put back. Once per real tick.
	static void PredictNextPose()
	{
		if (g_pred_tick == g_real_tick) return;
		g_pred_tick = g_real_tick;
		g_next_hands_ok = g_next_fist_ok = g_next_spawn_ok = false;
		int16_t nc = 0;
		if (!NextDirector(&nc)) return;
		bool hands = false, fist = false, spawn = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_LAYOUT && (r.site == SITE_HAND_A || r.site == SITE_HAND_B) && SiteActive(r.site, nc)) hands = true;
			if (r.kind == REC_LAYOUT && r.site == SITE_FIST && SiteActive(SITE_FIST, nc)) fist = true;
			if (r.kind == REC_COLUMN && nc >= 0xB && nc <= 0x19) spawn = true;
		}
		uint8_t *e = Caster();
		if (!(hands || fist || spawn) || !e || !(e[0] & 2) || !*(uint8_t **)(e + 0x64)) return;
		uint8_t *block = **(uint8_t ***)(e + 0x64);
		static uint8_t bones[0x20 + 0x100 * 0x30], fas[0x8000], gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		const uint32_t nb = 0x20 + (uint32_t)block[0] * 0x30;
		const uint32_t fa = var<uint32_t>(0x1D999C4);
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		memcpy(bones, block, nb);
		if (fa) memcpy(fas, (const void *)fa, sizeof(fas));
		memcpy(gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		pose_midpoint(e + 0x60, e + 0x6C, 1, 1);
		if (hands)
		{
			HandsMidpoint(e, g_next_hands);
			g_next_hands_ok = true;
		}
		if (fist)
		{
			GetEffectSpawnPosition(e, 0x15, 0, g_next_fist);
			g_next_fist_ok = true;
		}
		if (spawn)
		{
			GetEffectSpawnPosition(e, 0xF1, 0, g_next_spawn);
			g_next_spawn_ok = true;
		}
		memcpy(block, bones, nb);
		var<uint16_t>(0x1D9770C) = w70c;
		var<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, fas, sizeof(fas));
		memcpy((void *)GTE_DATA, gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	// ---- layouts
	static void HeldLayout(const Rec &r, int num, int den, bool at_next, const DirectorNode *nd, int16_t nc)
	{
		const DirectorNode *d = r.dir;
		const bool next_draws = nd == d && SiteActive(r.site, nc);
		if (at_next && !next_draws) return;
		PlayArg arg = r.arg;
		Mat4x3 f, m;
		switch (r.site)
		{
		case SITE_GROUND:
			// the ground point and Zell's height: the same on the next tick
			arg.pos[0] = d->ground[0];
			arg.pos[1] = W16(Caster(), 0x24);
			arg.pos[2] = d->ground[2];
			break;
		case SITE_HAND_A:
		case SITE_HAND_B:
			if (next_draws && g_next_hands_ok) for (int c = 0; c < 3; c++) arg.pos[c] = lerp16(r.anchor[c], g_next_hands[c], num, den);
			if (at_next) arg.radial = (uint32_t)(nc - 0x11) < 0x16 ? 0 : (int16_t)-0x400;
			break;
		case SITE_FIST:
			f = d->fist_m;
			for (int c = 0; c < 3; c++) f.t[c] = (next_draws && g_next_fist_ok) ? lerp16(r.anchor[c], g_next_fist[c], num, den) : r.anchor[c];
			ComposeAffineTransform(&Camera(), &f, &m);
			arg.m = &m;
			break;
		case SITE_IMPACT_A:
		case SITE_IMPACT_B:
		case SITE_TARGET:
			f = d->impact_m;
			for (int c = 0; c < 3; c++) f.t[c] = r.site == SITE_TARGET ? d->anchor[c] : d->impact[c];
			ComposeAffineTransform(&Camera(), &f, &m);
			arg.m = &m;
			break;
		case SITE_BILLBOARD:
			f = d->impact_m;
			for (int c = 0; c < 3; c++) f.t[c] = d->impact[c];
			ComposeAffineTransform(&Camera(), &f, &m);
			GteSetRotMatrixCtrl(&Camera());
			m.m[1][1] = 0x1000;
			m.m[0][0] = 0x1000;
			m.m[2][1] = 0;
			m.m[2][0] = 0;
			m.m[1][2] = 0;
			m.m[1][0] = 0;
			m.m[0][2] = 0;
			m.m[0][1] = 0;
			GteLoadV0(d->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
			arg.m = &m;
			break;
		}
		arg.morph = (uint32_t)g_held_morph;
		prim::play_held((prim::Layout *)((uint8_t *)d + r.site), PartCallback, (int)&arg, num, den);
	}

	// ---- the column: spin, tip (bend length / height scale), fade num / den of the way; the draw mode switches on the real frame
	static inline int32_t ColumnFade(int32_t k) { return (int32_t)((uint32_t)(shl32(k, 9) - 0xC01) & 0x1FFFFE00u); }
	static void HeldColumn(const Rec &r, int num, int den, bool at_next, const DirectorNode *nd, int16_t nc)
	{
		const DirectorNode *d = r.dir;
		const int32_t k1 = (int32_t)nc - 0xA;
		const bool next_draws = nd == d && (uint32_t)(k1 - 1) <= 0xE;
		if (at_next && !next_draws) return;
		uint8_t *caster = Caster();
		int16_t sp[3], delta[4];
		for (int c = 0; c < 3; c++) sp[c] = (next_draws && g_next_spawn_ok) ? lerp16(r.spawn[c], g_next_spawn[c], num, den) : r.spawn[c];
		delta[0] = (int16_t)(sp[0] - d->ground[0]);
		delta[1] = (int16_t)(sp[1] - d->ground[1]);
		delta[2] = (int16_t)(sp[2] - d->ground[2]);
		delta[3] = 0;
		const int32_t spin = next_draws ? lerp_i(shl32(r.k, 6), shl32(k1, 6), num, den) : shl32(r.k, 6);
		Mat4x3 m;
		YawMatrix(spin, &m);
		GteSetRotMatrix(&m);
		m.t[2] = 0;
		m.t[1] = 0;
		m.t[0] = 0;
		GteSetTransVector(&m);
		int32_t len = SqrtInt(mul32(delta[0], delta[0]) + mul32(delta[2], delta[2]));
		if (delta[2] > 0) len = -len;
		memcpy(g_held_col, g_cols[r.extra], COLUMN_BYTES);
		ColumnBend(MODEL_Column, (int16_t *)g_held_col, len);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		YawMatrix(W16(caster, 0xE), &m);
		m.m[1][1] = (int16_t)shl32(-*(const int32_t *)&delta[1], 2);
		m.t[0] = d->ground[0];
		m.t[1] = W16(caster, 0x24);
		m.t[2] = d->ground[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_Column;
		*(uint32_t *)(h + 4) = (uint32_t)g_held_col;
		const int32_t kk = at_next ? k1 : r.k;
		if ((uint32_t)kk >= 7)
		{
			B8(h, 0xA) = 0;
			B8(h, 9) = 0;
			B8(h, 8) = 0;
			*(uint32_t *)(h + 0xC) = (uint32_t)(at_next ? ColumnFade(k1) : next_draws ? lerp_i(ColumnFade(r.k), ColumnFade(k1), num, den) : ColumnFade(r.k));
			*(uint32_t *)(h + 0x1C) = 0x20F3;
		}
		else *(uint32_t *)(h + 0x1C) = 0x2033;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// ---- tiles: colour (r, g, b) * (d - n) / d num / den of the way to the next level's (black when the fade ends)
	static void HeldTile(const Rec &r, int num, int den, bool at_next, const DirectorNode *nd)
	{
		const int32_t k0 = r.d - r.n, k1 = k0 - 1;
		const bool next_draws = nd != nullptr && k1 > 0;
		if (at_next && !next_draws) return;
		int32_t c[3];
		for (int i = 0; i < 3; i++)
		{
			const int32_t a = (uint8_t)(mul32(r.rgb[i], k0) / r.d);
			const int32_t b = nd ? (k1 > 0 ? (uint8_t)(mul32(r.rgb[i], k1) / r.d) : 0) : a;
			c[i] = at_next ? b : lerp_i(a, b, num, den);
		}
		PacketCursor() = ScreenTile(c[0], c[1], c[2], 0, 1, PacketCursor());
	}

	// ---- the flat grey quad of tick 107 (held, not drawn at 1)
	static void HeldFlat(bool at_next)
	{
		if (at_next) return;
		uint8_t *p = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		W16(p, 0x14) = 0x140;
		W16(p, 0xC) = 0x140;
		W16(p, 8) = 0;
		W16(p, 0x10) = 0;
		W16(p, 0xE) = 0;
		W16(p, 0xA) = 0;
		W16(p, 0x16) = 0xD8;
		W16(p, 0x12) = 0xD8;
		p[6] = 0x80;
		p[5] = 0x80;
		p[4] = 0x80;
		uint32_t *q = (uint32_t *)PacketCursor();
		PacketCursor() += 8;
		q[0] = 0x01000000;
		q[1] = 0xE1000220;
		InsertPrimAutoDepth(OTBase() + 0x20, q);
		U32(p, 0) = 0x05000000;
		p[7] = 0x2A;
		InsertPrimAltViewport(OTBase() + 0x20, p);
		q = (uint32_t *)PacketCursor();
		PacketCursor() += 8;
		q[0] = 0x01000000;
		q[1] = 0xE1000240;
		InsertPrimAutoDepth(OTBase() + 0x20, q);
	}

	// ---- particles (the queues run on the next tick while a node lives: the root runs them until they are all empty)
	static void HeldSmoke(const Rec &r, bool at_next)
	{
		SmokeNode s;
		memcpy(&s, r.node, sizeof(s));
		if (at_next)
		{
			if (s.counter + 1 >= s.end) return;
			s.counter++;
		}
		SmokeDraw(&s);
	}
	static void HeldSpark(const Rec &r, int num, int den, bool at_next)
	{
		SparkNode s;
		memcpy(&s, r.node, sizeof(s));
		const bool alive = s.counter + 1 < s.end;
		if (at_next)
		{
			if (!alive) return;
			SparkMove(&s);
			s.counter++;
		}
		else if (alive)
			for (int c = 0; c < 3; c++) s.pos[c] = lerp16(s.pos[c], (int16_t)(s.pos[c] + s.vel[c]), num, den);
		SparkDraw(&s);
	}
	static void HeldDebris(const Rec &r, int num, int den, bool at_next)
	{
		DebrisNode k;
		memcpy(&k, r.node, sizeof(k));
		DebrisNode n = k;
		DebrisMove(&n);
		const bool alive = k.counter + 1 < 0x1E;
		if (at_next)
		{
			if (!alive) return;
			DebrisDraw(&n);
			return;
		}
		if (alive)
			for (int c = 0; c < 3; c++)
			{
				k.pos[c] = lerp16(k.pos[c], n.pos[c], num, den);
				k.rot[c] = (int16_t)(k.rot[c] + (int16_t)(k.spin[c] * num / den));
			}
		DebrisDraw(&k);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const bool at_next = num >= den;
		int16_t nc = 0;
		const DirectorNode *nd = NextDirector(&nc);
		PredictNextPose();
		static uint8_t bss[BSS_HI - BSS_LO], d9c8[0x80], b9e4[0xC];
		const uint16_t w70c = var<uint16_t>(0x1D9770C);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(d9c8, (const void *)0x1D999C8, sizeof(d9c8));
		memcpy(b9e4, (const void *)0xB8B9E4, sizeof(b9e4));
		g_in_held = true;
		{
			HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
				memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
				memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
				memcpy(var<uint8_t *>(0x1D999C4), g_scratch[r.scratch], SCRATCH);
				switch (r.kind)
				{
				case REC_LAYOUT: HeldLayout(r, num, den, at_next, nd, nc); break;
				case REC_COLUMN: HeldColumn(r, num, den, at_next, nd, nc); break;
				case REC_TILE: HeldTile(r, num, den, at_next, nd); break;
				case REC_FLAT: HeldFlat(at_next); break;
				case REC_SMOKE: HeldSmoke(r, at_next); break;
				case REC_SPARK: HeldSpark(r, num, den, at_next); break;
				case REC_DEBRIS: HeldDebris(r, num, den, at_next); break;
				}
			}
		}
		g_in_held = false;
		var<uint16_t>(0x1D9770C) = w70c;
		memcpy((void *)0xB8B9E4, b9e4, sizeof(b9e4));
		memcpy((void *)0x1D999C8, d9c8, sizeof(d9c8));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
	}
}

	static void register_mag112_held()
	{
		mark_held(meteor112::ORIG_DirectorTask);
		mark_held(meteor112::ORIG_SmokeTask);
		mark_held(meteor112::ORIG_SparkTask);
		mark_held(meteor112::ORIG_DebrisTask);
		register_module_held(112, meteor112::HeldReady, meteor112::HeldFrame);
	}
}
