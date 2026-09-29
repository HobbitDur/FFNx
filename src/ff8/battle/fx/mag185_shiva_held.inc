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

// 30 fps part of mag185_shiva.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag185_shiva.cpp)

namespace ff8fx
{
namespace s185
{
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// ---- held-frame memos of the sprite records (drawn state, per index) ----
	struct RecMemo { uint32_t tick; const void *owner; Rec r; };
	static RecMemo g_memo_a0[150], g_memo_a4[150];
	static void MemoRec(RecMemo *m, const void *owner, const Rec *r)
	{
		m->tick = g_real_tick;
		m->owner = owner;
		m->r = *r;
	}
	// the record moved on to its next frame on this tick (not freed, not respawned)
	static bool Advanced(const RecMemo &m, const Rec *cur) { return cur->mask == m.r.mask && cur->age == (int16_t)(m.r.age + 1); }

	// ---- held-frame memo of the node a task drew with (whole node, before the update) ----
	static NodeMemo<Node24, 128> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }

	struct RingTimerMemo { uint32_t tick; int32_t arg; };
	static RingTimerMemo g_ring_timer = { 0xFFFFFFFF, 0 };

	struct RecsMemo { uint32_t tick; uint8_t b[0x4000]; };
	static RecsMemo g_recs_shatter, g_recs_mount, g_recs_block0, g_recs_block1;
	static void MemoRecs(RecsMemo &m, uint32_t src, uint32_t size) { m.tick = g_real_tick; memcpy(m.b, (const void *)src, size); }

	// held-frame memo of the creature: the drawn skeleton (pose values + local matrices) and
	// the reader command; shell phase: its height
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		bool shell;            // drawn as the ice shell (ticks 0-4)
		bool midpoint;         // the pose advanced after the draw and the next tick draws that pose
		bool advanced;         // the anim was read after the draw (non-shell ticks)
		bool ahead;            // skel2 holds the skeleton the next tick's read leaves
		bool next_adv;         // the next tick reads the next pose of the same animation
		int32_t y_drawn, y_next;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
		uint8_t skel2[SKEL_MAX];
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	static uint8_t *Skeleton()
	{
		uint8_t *com = *(uint8_t **)(Creature() + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void CreatureMemoTake(bool shell)
	{
		CreatureMemo &m = g_creature;
		uint8_t *sk = Skeleton();
		m.tick = 0xFFFFFFFF;
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.cmd, Creature() + 0x6C, 8);
		m.shell = shell;
		m.midpoint = false;
		m.advanced = false;
		m.ahead = false;
		m.next_adv = false;
		m.y_drawn = m.y_next = var<int32_t>(0x22BD070);
		m.tick = g_real_tick;
	}

	// ------------------------------------------------------------------
	// Memo writes of the real tick (the FX_HELD statements of mag185_shiva.cpp)
	// ------------------------------------------------------------------
	static void held_note_master() { g_ported_tick = g_real_tick; }
	static void held_note_rec_a0(int i, const void *owner, const Rec *r) { MemoRec(&g_memo_a0[i], owner, r); }
	static void held_note_rec_a4(int i, const void *owner, const Rec *r) { MemoRec(&g_memo_a4[i], owner, r); }
	static void held_note_node(const Node24 *n) { MemoNode(n); }

	static void held_note_ring_timer(int32_t arg)
	{
		g_ring_timer.tick = g_real_tick;
		g_ring_timer.arg = arg;
	}

	static void held_note_shatter_recs() { MemoRecs(g_recs_shatter, Shards(), SHARD_BYTES); }
	static void held_note_mount_recs() { MemoRecs(g_recs_mount, Shards(), SHARD_BYTES); }
	static void held_note_block0_recs() { MemoRecs(g_recs_block0, MB() + 0x20000, BLOCK_BYTES); }
	static void held_note_block1_recs() { MemoRecs(g_recs_block1, MB() + 0x24000, BLOCK_BYTES); }

	static void held_note_creature(bool shell) { CreatureMemoTake(shell); }

	// shell phase: the height the next tick draws with
	static void held_note_shell_next(int32_t c, const Node24 *n)
	{
		if (g_creature.tick == g_real_tick && c + 1 < 5) g_creature.y_next = n->f20;
	}

	// after the tick's read: the read of the next tick run ahead on the real state (the real tick
	// may read: the frame gate lets effect-model reads through on real frames only), its skeleton
	// kept for the hand midpoint of held frames; the skeleton, the model state 0x22BD018, the anim
	// command, the GTE register files and the Field_Alloc scratch above its pointer are put back
	static uint8_t g_ahead_gte[0x1CA9300 - 0x1CA8A10], g_ahead_scr[0x1000], g_ahead_skel[SKEL_MAX], g_ahead_e[0x9C];

	static void held_note_creature_advanced()
	{
		CreatureMemo &m = g_creature;
		m.advanced = true;
		m.ahead = false;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (m.tick != g_real_tick || !sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint32_t scr = var<uint32_t>(0x1D999C4);
		memcpy(g_ahead_gte, (const void *)0x1CA8A10, sizeof(g_ahead_gte));
		memcpy(g_ahead_scr, (const void *)scr, sizeof(g_ahead_scr));
		memcpy(g_ahead_skel, sk, m.skel_size);
		memcpy(g_ahead_e, E, sizeof(g_ahead_e));
		AdvanceModelAnimLoop(E);
		memcpy(m.skel2, sk, m.skel_size);
		memcpy(sk, g_ahead_skel, m.skel_size);
		memcpy(E, g_ahead_e, sizeof(g_ahead_e));
		var<uint32_t>(0x1D999C4) = scr;
		memcpy((void *)scr, g_ahead_scr, sizeof(g_ahead_scr));
		memcpy((void *)0x1CA8A10, g_ahead_gte, sizeof(g_ahead_gte));
		m.ahead = true;
	}

	// after the counter step
	static void held_note_creature_next(const Node24 *n)
	{
		if (g_creature.tick == g_real_tick && !g_creature.shell)
		{
			// the next tick draws the pose just read unless it starts another animation first
			int32_t nc = n->c;
			g_creature.midpoint = g_creature.advanced && nc != 0x3B && nc != 0x96;
			// the next tick draws and reads on (59-138, 145-198) without starting an animation
			g_creature.next_adv = ((nc > 0x3B && nc < 0x8B) || (nc >= 0x91 && nc < 0xC7)) && nc != 0x96;
		}
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames and random
	// choices keep their 15 Hz batches. The frame matrix is rebuilt from the held-frame camera
	// while the timeline (its owner) is alive, into a local (never into 0x22BC130). Renderers
	// that integrate their records run on lerped private copies; the lit sprites' module matrix
	// 0x22BD0E8, 0x22BC154 and the creature state are put back afterwards.
	// ------------------------------------------------------------------
	static uint8_t g_ring_verts[0x1000];
	static uint8_t g_scratch[0x10000];  // RenderGeometry / shell vertex scratch
	static uint8_t g_recs_copy[BLOCK_BYTES], g_recs_copy2[BLOCK_BYTES];

	static int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// Flipbook frames switch on the real frame (the texture of the frame drawn on the tick); their
	// colours go between that frame's and the next frame's, and so does their layout when the
	// next frame shows the same texture (a frame that only scales its picture): the same draw
	// with the next frame goes to a private packet buffer and bucket table (0x1D8E04C pointed at
	// a private table, the execution node such a draw adds dropped, the cursor put back) and the
	// packets just drawn are blended toward its packets (same packet layout only).
	static uint8_t g_priv_pk[0x8000];
	static uint32_t g_priv_ot[0x4000];

	// xy: 0 colours only, 1 vertices too, 2 vertices too when the texture words (uv, clut, page)
	// are the same in both (a flipbook frame that only scales its picture)
	static void LerpPackets(uint8_t *a, uint8_t *ae, const uint8_t *b, const uint8_t *be, int num, int den, int xy)
	{
		while (a + 8 <= ae && b + 8 <= be)
		{
			uint32_t la = a[3];
			if (!la || la != b[3] || a[7] != b[7]) return;
			uint8_t code = a[7];
			if ((code & 0xE0) == 0x20)
			{
				bool gouraud = (code & 0x10) != 0, tex = (code & 4) != 0;
				uint32_t nv = (code & 8) ? 4 : 3;
				uint32_t *wa = (uint32_t *)(a + 4);
				const uint32_t *wb = (const uint32_t *)(b + 4);
				bool pos = xy == 1;
				if (xy == 2 && tex)
				{
					pos = true;
					for (uint32_t v = 0, i = 0; v < nv; v++)
					{
						i += (v == 0 || gouraud ? 1 : 0) + 1;
						uint32_t mask = v < 2 ? 0xFFFFFFFFu : 0xFFFFu; // uv (+ clut / page); the high half of the last two is unused
						if ((wa[i] ^ wb[i]) & mask) pos = false;
						i++;
					}
				}
				for (uint32_t v = 0, i = 0; v < nv; v++)
				{
					if (v == 0 || gouraud)
					{
						uint8_t *ca = (uint8_t *)&wa[i];
						const uint8_t *cb = (const uint8_t *)&wb[i];
						for (int k = 0; k < 3; k++) ca[k] = (uint8_t)lerp_i(ca[k], cb[k], num, den);
					}
					i += v == 0 || gouraud ? 1 : 0;
					if (pos)
					{
						int16_t *pa = (int16_t *)&wa[i];
						const int16_t *pb = (const int16_t *)&wb[i];
						for (int k = 0; k < 2; k++) pa[k] = (int16_t)lerp_i(pa[k], pb[k], num, den);
					}
					i += 1 + (tex ? 1 : 0);
				}
			}
			a += 4 + 4 * la;
			b += 4 + 4 * la;
		}
	}

	// draw(false) draws the frame of the tick, draw(true) the next frame (next: the record shows
	// that frame on the next tick)
	template<typename Draw>
	static void FlipDraw(bool next, int num, int den, Draw draw)
	{
		uint32_t c0 = Cursor();
		draw(false);
		if (!next || !num) return;
		uint32_t c1 = Cursor();
		uint32_t ot = var<uint32_t>(0x1D8E04C), exec = var<uint32_t>(0x1CA8828);
		var<uint32_t>(0x1D8E04C) = (uint32_t)g_priv_ot;
		Cursor() = (uint32_t)g_priv_pk;
		draw(true);
		uint32_t e = Cursor();
		Cursor() = c1;
		var<uint32_t>(0x1D8E04C) = ot;
		var<uint32_t>(0x1CA8828) = exec;
		if (e > (uint32_t)g_priv_pk && e - (uint32_t)g_priv_pk <= sizeof(g_priv_pk))
			LerpPackets((uint8_t *)c0, (uint8_t *)c1, g_priv_pk, (const uint8_t *)e, num, den, 2);
	}

	// lerped copy of integrated records (0x20 bytes, age at +0x0E, -1 = gone): every s16 half
	// way the short way round (fields change by less than half the s16 range per tick, angles
	// wrap), the age kept as drawn (the renderer steps it on the copy); a record that dies on
	// this tick is drawn as on the tick (its last picture), one born on the next is not drawn
	static void LerpRecordsNum(uint8_t *dst, const uint8_t *before, const uint8_t *now, uint32_t size, bool age_field, int num, int den)
	{
		const int16_t *a = (const int16_t *)before, *b = (const int16_t *)now;
		int16_t *d = (int16_t *)dst;
		for (uint32_t r = 0; r + 0x20 <= size; r += 0x20)
		{
			const int16_t *ra = a + r / 2, *rb = b + r / 2;
			int16_t *rd = d + r / 2;
			if (age_field && (ra[7] < 0 || rb[7] < 0))
			{
				memcpy(rd, ra, 0x20); // gone on this tick: its last picture until the real frame; born on the next: not drawn yet
				continue;
			}
			for (int k = 0; k < 16; k++)
				rd[k] = (age_field && k == 7) ? ra[k] : lerp_angle(ra[k], rb[k], num, den);
		}
	}

	// Integrated records (shards) are drawn from the lerped copy with the age they were drawn with
	// on the tick; their renderers take the fade and the shrink from the age (linear in it). Each
	// record alive on both ticks is drawn on its own (the others hidden: age -1; the renderers
	// walk the records in order and draw at most one prim per record, so the packets and bucket
	// insertions are those of one call), then the same record with the age it has on the next
	// tick privately (0x1D8E04C pointed at the private table, the cursor cur put back); its
	// packet is blended toward that one, vertices and colours (same packet layout only).
	// draw(records) draws through cur; the renderers integrate the records they draw (the
	// copies only).
	static uint8_t g_recs_iso[BLOCK_BYTES], g_recs_iso2[BLOCK_BYTES];

	template<typename Draw>
	static void RecordsDraw(uint32_t &cur, const uint8_t *copy, const uint8_t *now, uint32_t size, bool blend, int num, int den, Draw draw)
	{
		if (!blend || !num)
		{
			draw((uint32_t)copy);
			return;
		}
		memcpy(g_recs_iso, copy, size);
		for (uint32_t r = 0; r + 0x20 <= size; r += 0x20) ((int16_t *)(g_recs_iso + r))[7] = -1;
		memcpy(g_recs_iso2, g_recs_iso, size);
		for (uint32_t r = 0; r + 0x20 <= size; r += 0x20)
		{
			const int16_t *a = (const int16_t *)(copy + r), *b = (const int16_t *)(now + r);
			if (a[7] < 0) continue;
			memcpy(g_recs_iso + r, copy + r, 0x20);
			memcpy(g_recs_iso2 + r, copy + r, 0x20);
			if (b[7] >= 0) ((int16_t *)(g_recs_iso2 + r))[7] = b[7]; // the next tick's age (alive on both)
			uint32_t c0 = cur;
			draw((uint32_t)g_recs_iso);
			uint32_t c1 = cur;
			((int16_t *)(g_recs_iso + r))[7] = -1;
			if (b[7] < 0 || c1 == c0) { ((int16_t *)(g_recs_iso2 + r))[7] = -1; continue; }
			uint32_t ot = var<uint32_t>(0x1D8E04C), exec = var<uint32_t>(0x1CA8828);
			var<uint32_t>(0x1D8E04C) = (uint32_t)g_priv_ot;
			cur = (uint32_t)g_priv_pk;
			draw((uint32_t)g_recs_iso2);
			uint32_t e = cur;
			cur = c1;
			var<uint32_t>(0x1D8E04C) = ot;
			var<uint32_t>(0x1CA8828) = exec;
			((int16_t *)(g_recs_iso2 + r))[7] = -1;
			if (e - (uint32_t)g_priv_pk == c1 - c0)
				LerpPackets((uint8_t *)c0, (uint8_t *)c1, g_priv_pk, (const uint8_t *)e, num, den, 1);
		}
	}

	// ---- ring ----
	static void RingHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool moving = n->c != m->c;
		int16_t angle = moving ? lerp_angle(m->f18, n->f18, num, den) : m->f18;
		int16_t ang[4] = { 0, angle, 0, 0 };
		Mat4x3 lm, w;
		NodeMatrix(&lm, ang, true, m->f10, m->f12, m->f14, m->f1C, m->f20, m->f1C);
		ComposeAffineTransform(frame, &lm, &w);
		GteSetRotMatrix(&w);
		GteSetTransVector(&w);
		static uint8_t g128[16], g190[16];
		memcpy(g128, (const void *)0x22BC128, 8);
		memcpy(g190, (const void *)0x22BC190, 8);
		bool ring_ok = g_ring_timer.tick == g_real_tick;
		if (ring_ok)
		{
			// the ring as it grows toward the next tick's (the timer's argument grows by 2 per
			// tick up to 36: +1 half way)
			int32_t arg = g_ring_timer.arg;
			if (moving) arg = lerp_i(arg, arg + 2 > 0x24 ? 0x24 : arg + 2, num, den);
			memcpy(g_ring_verts, RingVerts(), sizeof(g_ring_verts));
			RingGeometry(arg, g_ring_verts, g128, g190);
		}
		if (m->c < 0x3C && ring_ok)
		{
			// faces grown (+2 per tick up to 36) and fade steps left (-2 per tick) toward the
			// next tick's; the fade switching on (tick 18) stays on the real frame
			int32_t h5c = m->f22 >= 0x24 ? 0x24 : (int32_t)m->f22;
			int32_t steps = RingFadeSteps(m->c);
			bool fade = m->c >= 0x12;
			if (moving)
			{
				h5c = lerp_i(h5c, n->f22 >= 0x24 ? 0x24 : (int32_t)n->f22, num, den);
				if (fade) steps = lerp_i(steps, RingFadeSteps(n->c), num, den);
			}
			RingDraw((uint32_t)g_ring_verts, h5c, steps, fade);
		}
		ComposeAffineTransform(&RootMatrix(), &lm, &w);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0xD209F4;
		*(uint16_t *)(h + 0x24) = 0;
		if (m->c < 0x1D)
		{
			int16_t v[4] = { 0, 0, 0, 0 };
			// (the ring's first tick draws it at the height 0xD8F1 the timer set, off screen: it shows
			// from the next real frame on)
			MatrixMultiplyVector(&w, (const int16_t *)(ring_ok && m->c > 0 ? g190 : (uint8_t *)0x22BC190), v);
			v[0] = (int16_t)(v[0] + (int16_t)w.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)w.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)w.t[2]);
			FlipDraw(moving && m->c + 1 < 0x1D, num, den, [&](bool nx) {
				*(int16_t *)(h + 4) = (int16_t)((m->c + (nx ? 1 : 0)) & 7);
				TransformCameraByShadowRotation(v, 0x900, -0x90);
				Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
			});
		}
		*(uint32_t *)h = 0xD20C7C;
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			const Rec *cur = RecA0(i);
			int16_t pos[4] = { mm.r.x, mm.r.y, mm.r.z, mm.r.w0E };
			bool adv = Advanced(mm, cur);
			if (adv)
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
			}
			FlipDraw(adv, num, den, [&](bool nx) { SpriteAt(h, (int16_t)(mm.r.age + (nx ? 1 : 0)), pos, mm.r.size); });
		}
		FieldFree(0xB4);
	}

	// ---- plain sprite pools (puff / mist / sparkle): position and size half way ----
	static void SpriteHeld(RecMemo *memo, int count, uint8_t *pool, const void *owner, uint32_t seq, int16_t h24, bool h14, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = (uint16_t)h24;
		if (h14) *(uint32_t *)(h + 0x14) = 0x1000;
		for (int i = 0; i < count; i++)
		{
			const RecMemo &mm = memo[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			const Rec *cur = (const Rec *)(pool + 0x18 * i);
			int16_t pos[4] = { mm.r.x, mm.r.y, mm.r.z, mm.r.w0E };
			int16_t size = mm.r.size;
			bool adv = Advanced(mm, cur);
			if (adv)
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
				size = Lerp16(mm.r.size, cur->size, num, den);
			}
			FlipDraw(adv, num, den, [&](bool nx) { SpriteAt(h, (int16_t)(mm.r.age + (nx ? 1 : 0)), pos, size); });
		}
		FieldFree(0xB4);
	}

	// ---- node-driven prim models ----
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return n->c == (int16_t)(m->c + 1); }

	static void IcicleHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t a = mv ? lerp_angle(m->f18, n->f18, num, den) : m->f18;
		int16_t s = mv ? Lerp16(m->f1C, n->f1C, num, den) : m->f1C;
		int32_t f0 = mul32(m->c - 8, 682), f = f0;
		if (mv && m->c >= 8) f = lerp_i(f0, mul32(n->c - 8, 682), num, den);
		IcicleDraw(frame, a, m->f10, m->f12, m->f14, s, m->f20, m->c >= 8, f);
	}

	static void ShatterHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		Mat4x3 lm;
		ShatterMatrix(m, &lm);
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		*(int32_t *)(h + 0x20) = m->c;
		ComposeAffineTransform(frame, &lm, (Mat4x3 *)(h + 0x7C));
		*(uint32_t *)h = 0xD2F1C4;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)(h + 0x28) = Shards();
		if (m->c >= 0x12)
		{
			if (g_recs_shatter.tick == g_real_tick)
			{
				*(int32_t *)(h + 0x20) -= 0x12;
				bool mv = NodeMoved(m, n);
				LerpRecordsNum(g_recs_copy, g_recs_shatter.b, (const uint8_t *)Shards(), SHARD_BYTES, true, mv ? num : 0, den);
				RecordsDraw(Cursor(), g_recs_copy, (const uint8_t *)Shards(), SHARD_BYTES, mv, num, den, [&](uint32_t recs) {
					*(uint32_t *)(h + 0x28) = recs;
					Cursor() = ShatterFly(h, OT(), 2, Cursor());
				});
			}
		}
		else Cursor() = ShatterStatic(h, OT(), 2, Cursor());
		FieldFree(0xBC);
	}

	static void MountHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int32_t c = m->c;
		if (c < 5)
		{
			// rising toward the next tick's height and clip plane (on 4 toward the standing
			// model's first draw: height f12, plane 0 = the ground, as unclipped)
			bool rise = mv;
			int32_t y = rise ? lerp_i(m->f12, n->f12, num, den) : m->f12;
			int32_t py0 = mul32(c, 0xA64) / 5 - 0xA64;
			int32_t py = rise ? lerp_i(py0, mul32(c + 1, 0xA64) / 5 - 0xA64, num, den) : py0;
			Mat4x3 lm;
			MountMatrix(m, c, y, &lm);
			ComposeAffineTransform(frame, &lm, &lm);
			GteSetRotMatrix(&lm);
			GteSetTransVector(&lm);
			MountRise((int16_t)py);
		}
		else if (c < 0x40)
		{
			Mat4x3 lm;
			MountMatrix(m, c, m->f12, &lm);
			if (mv && c + 1 < 0x40)
			{
				// toward the next tick's position: its height (3 -> 0 on 5) and the push of 0x46 on
				// z over 52-61
				Mat4x3 ln;
				MountMatrix(n, c + 1, n->f12, &ln);
				for (int k = 0; k < 3; k++) lm.t[k] = lerp_i(lm.t[k], ln.t[k], num, den);
			}
			ComposeAffineTransform(frame, &lm, &lm);
			GteSetRotMatrix(&lm);
			GteSetTransVector(&lm);
			MountStand();
		}
		else if (g_recs_mount.tick == g_real_tick)
		{
			Mat4x3 lm;
			MountMatrix(m, c, m->f12, &lm);
			LerpRecordsNum(g_recs_copy, g_recs_mount.b, (const uint8_t *)Shards(), SHARD_BYTES, true, mv ? num : 0, den);
			RecordsDraw(Cursor(), g_recs_copy, (const uint8_t *)Shards(), SHARD_BYTES, mv, num, den, [&](uint32_t recs) {
				MountShatter(frame, &lm, c, false, recs, false);
			});
		}
	}

	static void GroundRingHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n) && n->c <= 0xC;
		int32_t f0 = 0, f1 = 0;
		bool fade = GroundRingFade(m->c, &f0);
		bool fade1 = GroundRingFade(m->c + 1, &f1);
		int32_t f = f0;
		// fade in 0-3 ends with (4 - 4) << 10 = 0 at 4, drawn as no fade: toward 0 on tick 3
		if (mv && fade && (fade1 || m->c == 3)) f = lerp_i(f0, fade1 ? f1 : 0, num, den);
		int32_t scroll = shl32(m->c, 3);
		if (mv) scroll = lerp_i(scroll, shl32(m->c + 1, 3), num, den);
		GroundRingDraw(frame, m, scroll, fade, f);
	}

	// ---- creature ----
	static uint8_t g_skel_cur[SKEL_MAX];

	// Shiva's skeleton has 69 bones: pose_midpoint (FFNx, up to 64 bones) would hold the pose.
	// The pose the next tick draws is already in the real skeleton (the tick's advance ran after
	// its draw): the drawn pose (sk) is put half way toward it the way pose_midpoint does
	// (angles the short way round, toward whichever spelling of the next rotation is nearer:
	// Ry(-y) Rx(-x) Rz(z) = (2048 - x, y + 2048, z + 2048)), then the bone matrices are built
	// from that pose; the drawn pose values are put back (DrawModel rebuilds from them after
	// its draw, the real skeleton is restored afterwards anyway).
	static int16_t PoseStep(int d, int num, int den) { int t = d * num; return (int16_t)(t >= 0 ? (t + den / 2) / den : -((-t + den / 2) / den)); }

	static void PoseMid(uint8_t *E, uint8_t *sk, const uint8_t *next, int num, int den)
	{
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		static int16_t cur[3 + 6 * 256];
		memcpy(cur, sk + 8, 6);
		for (int b = 0; b < nb; b++) memcpy(cur + 3 + 6 * b, sk + 16 + 48 * b + 4, 12);
		int16_t *root = (int16_t *)(sk + 8);
		const int16_t *nroot = (const int16_t *)(next + 8);
		for (int a = 0; a < 3; a++) root[a] = (int16_t)(root[a] + PoseStep(nroot[a] - root[a], num, den));
		for (int b = 0; b < nb; b++)
		{
			int16_t *v = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *q = (const int16_t *)(next + 16 + 48 * b + 4);
			int d[3], e[3], sd = 0, se = 0;
			for (int a = 0; a < 3; a++) { d[a] = ((q[a] - v[a] + 2048) & 4095) - 2048; sd += d[a] < 0 ? -d[a] : d[a]; }
			e[0] = ((2048 - q[0] - v[0] + 2048) & 4095) - 2048;
			e[1] = ((q[1] - v[1]) & 4095) - 2048;
			e[2] = ((q[2] - v[2]) & 4095) - 2048;
			for (int a = 0; a < 3; a++) se += e[a] < 0 ? -e[a] : e[a];
			const int *dd = se < sd ? e : d;
			for (int a = 0; a < 3; a++) v[a] = (int16_t)(v[a] + PoseStep(dd[a], num, den));
			if (scaled) for (int a = 3; a < 6; a++) v[a] = (int16_t)(v[a] + PoseStep(q[a] - v[a], num, den));
		}
		BuildBoneMatricesFromPose(E + 0x60);
		memcpy(sk + 8, cur, 6);
		for (int b = 0; b < nb; b++) memcpy(sk + 16 + 48 * b + 4, cur + 3 + 6 * b, 12);
	}

	static void CreatureHeld(const Mat4x3 *frame, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		int32_t y_cur = var<int32_t>(0x22BD070);
		uint32_t frame_ptr = var<uint32_t>(0x22BC154);
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		if (m.shell)
		{
			var<int32_t>(0x22BD070) = lerp_i(m.y_drawn, m.y_next, num, den);
			ShellPhase(E, frame, (uint32_t)g_scratch, false);
		}
		else
		{
			if (m.midpoint) PoseMid(E, sk, g_skel_cur, num, den);
			var<int32_t>(0x22BD070) = m.y_drawn;
			DrawModel(E, frame, (uint32_t)g_scratch);
		}
		var<int32_t>(0x22BD070) = y_cur;
		var<uint32_t>(0x22BC154) = frame_ptr;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	// ---- creature-tick effects ----
	// The hand midpoint (Mid 0x22BCFC8 / MidWorld 0x22BCFC0, bones 0x1A and 0x20) is recomputed by
	// the hand glow while its counter is below 47, after the creature's tick: from the bone
	// matrices the creature's anim read left. The next tick's comes from the skeleton its read
	// leaves (run ahead on the real tick, held_note_creature_advanced; a tick that does not read,
	// or starts an animation, leaves it where it is). Drawn half way by the hand glow, the spike
	// ring and the shard burst (the globals are set for the held frame and put back).
	static void HandMid(int16_t *out)
	{
		int16_t a[4], b[4];
		GetEffectSpawnPosition(Creature(), 0x1A, 0x1800, a);
		GetEffectSpawnPosition(Creature(), 0x20, 0x1800, b);
		out[0] = (int16_t)(((int32_t)b[0] + a[0]) / 2);
		out[1] = (int16_t)(((int32_t)b[1] + a[1]) / 2);
		out[2] = (int16_t)(((int32_t)b[2] + a[2]) / 2);
	}

	static uint8_t g_skel_hand[SKEL_MAX];

	// held-frame hand midpoint written into Mid / MidWorld (put back by HeldFrame)
	static void HandMidHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		const CreatureMemo &cm = g_creature;
		if (!m || !NodeMoved(m, n) || n->c >= 0x2F || cm.tick != g_real_tick || !cm.ahead || !cm.next_adv) return;
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != cm.skel_size) return;
		uint8_t v708[8];
		memcpy(v708, (const void *)0x1D97708, sizeof(v708));
		memcpy(g_skel_hand, sk, cm.skel_size);
		memcpy(sk, cm.skel2, cm.skel_size);
		int16_t next[4];
		HandMid(next);
		memcpy(sk, g_skel_hand, cm.skel_size);
		memcpy((void *)0x1D97708, v708, sizeof(v708));
		int16_t *mid = Mid();
		for (int k = 0; k < 3; k++) mid[k] = Lerp16(mid[k], next[k], num, den);
		MatrixMultiplyVector(&RootMatrix(), mid, MidWorld());
		MidWorld()[0] = (int16_t)(MidWorld()[0] + (int16_t)RootT(0));
		MidWorld()[1] = (int16_t)(MidWorld()[1] + (int16_t)RootT(1));
		MidWorld()[2] = (int16_t)(MidWorld()[2] + (int16_t)RootT(2));
	}

	static void HandGlowHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t s = mv ? Lerp16(m->f1C, n->f1C, num, den) : m->f1C;
		int32_t f0 = mul32(m->c - 0x32, 682), f = f0;
		if (mv && m->c >= 0x32) f = lerp_i(f0, mul32(n->c - 0x32, 682), num, den);
		HandGlowDraw(s, m->c >= 0x32, f);
	}

	static void SpikeRingHeld(const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = 0xD3048C;
		*(uint32_t *)(h + 8) = 0;
		SpikeRingSetup(s);
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			const Rec *cur = RecA0(i);
			int16_t angle = mm.r.vz;
			int32_t f = SpikeFade(mm.r.age);
			if (Advanced(mm, cur))
			{
				angle = lerp_angle(mm.r.vz, cur->vz, num, den);
				f = lerp_i(f, SpikeFade(cur->age), num, den);
			}
			SpikeRingOne(h, s, angle, mm.r.size, f);
		}
		FieldFree(0x38);
		FieldFree(0x58);
	}

	// ShardBurstOne with the fade value given (-1 = none)
	static void ShardBurstOneF(const Mat4x3 *frame, uint8_t *h, uint8_t *s, int16_t a1, int16_t a2, int16_t size, int32_t fade)
	{
		*(int16_t *)(s + 2) = a1;
		*(int16_t *)(s + 4) = a2;
		int32_t *sc = (int32_t *)(s + 0x28);
		sc[2] = size;
		sc[1] = size;
		sc[0] = size;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		Scale3DMatrix((Mat4x3 *)(s + 8), sc);
		*(int32_t *)(s + 0x1C) = Mid()[0];
		*(int32_t *)(s + 0x20) = Mid()[1];
		*(int32_t *)(s + 0x24) = Mid()[2];
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetRotMatrix((const Mat4x3 *)(s + 8));
		GteSetTransVector((const Mat4x3 *)(s + 8));
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (fade >= 0)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
	}

	static void ShardBurstHeld(const Mat4x3 *frame, const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = 0xD309A4;
		*(uint32_t *)(h + 8) = 0;
		*(int16_t *)s = 0;
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			const Rec *cur = RecA0(i);
			int16_t size = mm.r.size;
			bool adv = Advanced(mm, cur);
			if (adv) size = Lerp16(mm.r.size, cur->size, num, den);
			// fade 0x1000 - age * 682 for ages 0-5 (from 6 on none: the formula's 4 at age 6),
			// toward the next age's
			int32_t f = -1;
			if (mm.r.age < 6) f = adv ? lerp_i(0x1000 - mul32(mm.r.age, 682), 0x1000 - mul32(mm.r.age + 1, 682), num, den) : 0x1000 - mul32(mm.r.age, 682);
			ShardBurstOneF(frame, h, s, mm.r.vy, mm.r.vz, size, f);
		}
		FieldFree(0x38);
		FieldFree(0x58);
	}

	static void OrbitSnowHeld(const Mat4x3 *frame, const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		*(uint32_t *)h = 0xD20E10;
		*(uint16_t *)(h + 0x24) = 8;
		OrbitSetup(frame, s);
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			const Rec *cur = RecA0(i);
			int16_t rad = mm.r.x, hy = mm.r.y, ang = mm.r.z;
			bool adv = Advanced(mm, cur);
			if (adv)
			{
				rad = Lerp16(mm.r.x, cur->x, num, den);
				hy = Lerp16(mm.r.y, cur->y, num, den);
				ang = lerp_angle(mm.r.z, cur->z, num, den);
			}
			FlipDraw(adv, num, den, [&](bool nx) { OrbitOne(h, s, rad, hy, ang, mm.r.size, (int16_t)(mm.r.age + (nx ? 1 : 0))); });
		}
		FieldFree(0x40);
		FieldFree(0xB4);
	}

	static void GlowRingHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t a = mv ? lerp_angle(m->f18, n->f18, num, den) : m->f18;
		int16_t s = mv ? Lerp16(m->f1C, n->f1C, num, den) : m->f1C;
		int16_t s2 = mv ? Lerp16(m->f20, n->f20, num, den) : m->f20;
		int32_t f0 = mul32(m->c - 0x16, 409), f = f0;
		if (mv && m->c >= 0x16) f = lerp_i(f0, mul32(n->c - 0x16, 409), num, den);
		GlowRingDraw(frame, m, a, s, s2, m->c >= 0x16, f);
	}

	static void GlowIcicleHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t a = mv ? lerp_angle(m->f18, n->f18, num, den) : m->f18;
		int16_t s = mv ? Lerp16(m->f1C, n->f1C, num, den) : m->f1C;
		int32_t f0 = mul32(m->c - 0xC, 682), f = f0;
		if (mv && m->c >= 0xC) f = lerp_i(f0, mul32(n->c - 0xC, 682), num, den);
		GlowIcicleDraw(frame, m, a, s, m->c >= 0xC, f);
	}

	static void LitPoolHeld(const Mat4x3 *frame, const Node24 *n, uint32_t seq, int16_t h24, uint32_t mask_task, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = (uint16_t)h24;
		bool snowfall = mask_task == ORIG_SnowfallTask;
		if (snowfall)
		{
			const Node24 *m = g_node_memo.get(n);
			int16_t a = m ? (NodeMoved(m, n) ? lerp_angle(m->f18, n->f18, num, den) : m->f18) : n->f18;
			SnowfallSetup(frame, m ? m : n, a, s);
		}
		else BlizzardSetup(frame, n, s);
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			const Rec *cur = RecA0(i);
			int16_t pos[4] = { mm.r.x, mm.r.y, mm.r.z, mm.r.w0E };
			bool adv = Advanced(mm, cur);
			if (adv)
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
			}
			FlipDraw(adv, num, den, [&](bool nx) {
				int16_t fr = (int16_t)(mm.r.age + (nx ? 1 : 0));
				if (snowfall && fr < 8) fr = 8;
				LitSprite(h, s, pos, mm.r.size, fr, 0x28);
			});
		}
		FieldFree(0x38);
		FieldFree(0xB4);
	}

	static void HaloHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t s = mv ? Lerp16(m->f1C, n->f1C, num, den) : m->f1C;
		int16_t s2 = mv ? Lerp16(m->f20, n->f20, num, den) : m->f20;
		int32_t scroll = shl32(m->c, 4);
		if (mv) scroll = lerp_i(scroll, shl32(n->c, 4), num, den);
		int32_t f0 = shl32(m->c - 0x16, 9), f = f0;
		if (mv && m->c >= 0x16) f = lerp_i(f0, shl32(n->c - 0x16, 9), num, den);
		HaloDraw(frame, m, s, s2, scroll, m->c >= 0x16, f);
	}

	static void IceBlockHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		Mat4x3 lm;
		IceBlockMatrix(m, &lm);
		int16_t c = m->c;
		if (c < 0x10)
		{
			if (g_recs_block0.tick != g_real_tick || g_recs_block1.tick != g_real_tick) return;
			LerpRecordsNum(g_recs_copy, g_recs_block0.b, (const uint8_t *)(MB() + 0x20000), BLOCK_BYTES, false, mv ? num : 0, den);
			LerpRecordsNum(g_recs_copy2, g_recs_block1.b, (const uint8_t *)(MB() + 0x24000), BLOCK_BYTES, false, mv ? num : 0, den);
			IceBlockBuild(frame, &lm, c, (uint32_t)g_recs_copy, (uint32_t)g_recs_copy2, false);
		}
		else if (c < 0x20)
		{
			// the flash's fade and scroll are linear in 2c: toward the next tick's while it is
			// drawn on both (its switch to the fade at 18 and its end after 26 stay on the real frame)
			bool lin = mv && c >= 0x12 && c + 1 <= 0x1A;
			IceBlockWhole(frame, &lm, c, shl32(c, 1) + (lin ? lerp_i(0, 2, num, den) : 0), lin);
		}
		else if (g_recs_block0.tick == g_real_tick)
		{
			LerpRecordsNum(g_recs_copy, g_recs_block0.b, (const uint8_t *)(MB() + 0x20000), BLOCK_BYTES, true, mv ? num : 0, den);
			RecordsDraw(Cursor(), g_recs_copy, (const uint8_t *)(MB() + 0x20000), BLOCK_BYTES, mv, num, den, [&](uint32_t recs) {
				IceBlockShatter(frame, &lm, c, recs, false);
			});
		}
	}

	static void DustHeld(const Mat4x3 *frame, const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xD208B0;
		*(uint16_t *)(h + 0x24) = 0;
		DustSetup(frame, s);
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			// standing sparkles: the frame drawn, its colours toward the next frame's
			FlipDraw(Advanced(mm, RecA0(i)), num, den, [&](bool nx) { DustOne(h, s, &mm.r.x, mm.r.size, (int16_t)(mm.r.age + (nx ? 1 : 0))); });
		}
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static uint8_t g_held_packets[0x80000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order; packets go to a private buffer (the module draws through the
	// engine cursor battle_texture_data_ptr_1D8E054 and the arena 0x22BD0B8, both redirected
	// and put back)
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor(), arena = Arena();
		Cursor() = (uint32_t)g_held_packets;
		Arena() = (uint32_t)g_held_packets + 0x40000;
		uint8_t sprite_mat[0x20];
		memcpy(sprite_mat, (const void *)SPRITE_MAT, sizeof(sprite_mat));
		bool timeline = false;
		const Node24 *hand = nullptr;
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
			if ((uint32_t)t->func == ORIG_HandGlowTask) hand = (const Node24 *)t;
		}
		int16_t mid_save[8];
		memcpy(mid_save, Mid(), 8);
		memcpy(mid_save + 4, MidWorld(), 8);
		if (hand) HandMidHeld(hand, num, den);
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &frame);
		else frame = Frame();
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_RingTask: RingHeld(&frame, n, num, den); break;
			case ORIG_PuffTask: SpriteHeld(g_memo_a4, 100, PoolA4(), n, 0xD20AD0, 8, false, num, den); break;
			case ORIG_MistTask: SpriteHeld(g_memo_a0, 150, PoolA0(), n, 0xD20AD0, 8, false, num, den); break;
			case ORIG_SparkleTask: SpriteHeld(g_memo_a0, 150, PoolA0(), n, 0xD208B0, 0, true, num, den); break;
			case ORIG_IcicleTask: IcicleHeld(&frame, n, num, den); break;
			case ORIG_ShatterTask: ShatterHeld(&frame, n, num, den); break;
			case ORIG_MountTask: MountHeld(&frame, n, num, den); break;
			case ORIG_GroundRingTask: GroundRingHeld(&frame, n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, num, den); break;
			case ORIG_HandGlowTask: HandGlowHeld(n, num, den); break;
			case ORIG_SpikeRingTask: SpikeRingHeld(n, num, den); break;
			case ORIG_ShardBurstTask: ShardBurstHeld(&frame, n, num, den); break;
			case ORIG_OrbitSnowTask: OrbitSnowHeld(&frame, n, num, den); break;
			case ORIG_GlowRingTask: GlowRingHeld(&frame, n, num, den); break;
			case ORIG_GlowIcicleTask: GlowIcicleHeld(&frame, n, num, den); break;
			case ORIG_SnowfallTask: LitPoolHeld(&frame, n, 0xD20E10, 0x208, ORIG_SnowfallTask, num, den); break;
			case ORIG_HaloTask: HaloHeld(&frame, n, num, den); break;
			case ORIG_BlizzardTask: LitPoolHeld(&frame, n, 0xD20AD0, 8, ORIG_BlizzardTask, num, den); break;
			case ORIG_IceBlockTask: IceBlockHeld(&frame, n, num, den); break;
			case ORIG_DustTask: DustHeld(&frame, n, num, den); break;
			default: break;
			}
		}
		memcpy((void *)SPRITE_MAT, sprite_mat, sizeof(sprite_mat));
		memcpy(Mid(), mid_save, 8);
		memcpy(MidWorld(), mid_save + 4, 8);
		Cursor() = cursor;
		Arena() = arena;
	}
}

	static void register_mag185_held()
	{
		mark_held(s185::ORIG_RingTask);
		mark_held(s185::ORIG_ShatterTask);
		mark_held(s185::ORIG_MountTask);
		mark_held(s185::ORIG_GroundRingTask);
		mark_held(s185::ORIG_PuffTask);
		mark_held(s185::ORIG_MistTask);
		mark_held(s185::ORIG_IcicleTask);
		mark_held(s185::ORIG_CreatureTask);
		mark_held(s185::ORIG_SparkleTask);
		mark_held(s185::ORIG_HandGlowTask);
		mark_held(s185::ORIG_SpikeRingTask);
		mark_held(s185::ORIG_ShardBurstTask);
		mark_held(s185::ORIG_OrbitSnowTask);
		mark_held(s185::ORIG_GlowRingTask);
		mark_held(s185::ORIG_GlowIcicleTask);
		mark_held(s185::ORIG_SnowfallTask);
		mark_held(s185::ORIG_HaloTask);
		mark_held(s185::ORIG_BlizzardTask);
		mark_held(s185::ORIG_IceBlockTask);
		mark_held(s185::ORIG_DustTask);
		register_module_held(185, s185::HeldReady, s185::HeldFrame);
	}
}
