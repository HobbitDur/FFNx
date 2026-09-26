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
		int32_t y_drawn, y_next;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
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

	static void held_note_creature_advanced() { g_creature.advanced = true; }

	// after the counter step
	static void held_note_creature_next(const Node24 *n)
	{
		if (g_creature.tick == g_real_tick && !g_creature.shell)
		{
			// the next tick draws the pose just read unless it starts another animation first
			int32_t nc = n->c;
			g_creature.midpoint = g_creature.advanced && nc != 0x3B && nc != 0x96;
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

	// lerped copy of integrated records (0x20 bytes, age at +0x0E, -1 = gone): every s16 half
	// way the short way round (fields change by less than half the s16 range per tick, angles
	// wrap), the age kept as drawn (the renderer steps it on the copy)
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
				memcpy(rd, ra, 0x20);
				if (rb[7] < 0) rd[7] = -1; // gone on this tick: not drawn in between
				continue;
			}
			for (int k = 0; k < 16; k++)
				rd[k] = (age_field && k == 7) ? ra[k] : lerp_angle(ra[k], rb[k], num, den);
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
			// the ring as it grows half a step: the timer's argument + 1 (it grows by 2 per tick)
			int32_t arg = g_ring_timer.arg;
			if (moving && arg < 0x24) arg++;
			memcpy(g_ring_verts, RingVerts(), sizeof(g_ring_verts));
			RingGeometry(arg, g_ring_verts, g128, g190);
		}
		if (m->c < 0x3C && ring_ok)
		{
			int32_t h5c = m->f22 >= 0x24 ? 0x24 : (int32_t)m->f22;
			if (moving && h5c < 0x24) h5c++;
			int32_t steps = RingFadeSteps(m->c);
			bool fade = m->c >= 0x12;
			if (moving && fade && steps > 0) steps--;
			RingDraw((uint32_t)g_ring_verts, h5c, steps, fade);
		}
		ComposeAffineTransform(&RootMatrix(), &lm, &w);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0xD209F4;
		*(uint16_t *)(h + 0x24) = 0;
		if (m->c < 0x1D)
		{
			int16_t v[4] = { 0, 0, 0, 0 };
			MatrixMultiplyVector(&w, (const int16_t *)(ring_ok ? g190 : (uint8_t *)0x22BC190), v);
			v[0] = (int16_t)(v[0] + (int16_t)w.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)w.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)w.t[2]);
			*(int16_t *)(h + 4) = (int16_t)(m->c & 7);
			TransformCameraByShadowRotation(v, 0x900, -0x90);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		*(uint32_t *)h = 0xD20C7C;
		for (int i = 0; i < 150; i++)
		{
			const RecMemo &mm = g_memo_a0[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			const Rec *cur = RecA0(i);
			int16_t pos[4] = { mm.r.x, mm.r.y, mm.r.z, mm.r.w0E };
			if (Advanced(mm, cur))
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
			}
			SpriteAt(h, mm.r.age, pos, mm.r.size);
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
			if (Advanced(mm, cur))
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
				size = Lerp16(mm.r.size, cur->size, num, den);
			}
			SpriteAt(h, mm.r.age, pos, size);
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
				LerpRecordsNum(g_recs_copy, g_recs_shatter.b, (const uint8_t *)Shards(), SHARD_BYTES, true, NodeMoved(m, n) ? num : 0, den);
				*(uint32_t *)(h + 0x28) = (uint32_t)g_recs_copy;
				Cursor() = ShatterFly(h, OT(), 2, Cursor());
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
			bool rise = mv && c + 1 < 5;
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
			MountShatter(frame, &lm, c, false, (uint32_t)g_recs_copy, false);
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
		if (mv && fade && fade1) f = lerp_i(f0, f1, num, den);
		int32_t scroll = shl32(m->c, 3);
		if (mv) scroll = lerp_i(scroll, shl32(m->c + 1, 3), num, den);
		GroundRingDraw(frame, m, scroll, fade, f);
	}

	// ---- creature ----
	static uint8_t g_skel_cur[SKEL_MAX];

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
			if (m.midpoint) pose_midpoint(E + 0x60, E + 0x6C, num, den);
			var<int32_t>(0x22BD070) = m.y_drawn;
			DrawModel(E, frame, (uint32_t)g_scratch);
		}
		var<int32_t>(0x22BD070) = y_cur;
		var<uint32_t>(0x22BC154) = frame_ptr;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	// ---- creature-tick effects ----
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
			if (Advanced(mm, cur)) size = Lerp16(mm.r.size, cur->size, num, den);
			ShardBurstOne(frame, h, s, mm.r.vy, mm.r.vz, size, mm.r.age);
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
			if (Advanced(mm, cur))
			{
				rad = Lerp16(mm.r.x, cur->x, num, den);
				hy = Lerp16(mm.r.y, cur->y, num, den);
				ang = lerp_angle(mm.r.z, cur->z, num, den);
			}
			OrbitOne(h, s, rad, hy, ang, mm.r.size, mm.r.age);
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
			if (Advanced(mm, cur))
			{
				pos[0] = Lerp16(mm.r.x, cur->x, num, den);
				pos[1] = Lerp16(mm.r.y, cur->y, num, den);
				pos[2] = Lerp16(mm.r.z, cur->z, num, den);
			}
			int16_t fr = mm.r.age;
			if (snowfall && fr < 8) fr = 8;
			LitSprite(h, s, pos, mm.r.size, fr, 0x28);
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
			// the flash's fade and scroll are linear in the tick: half a step on held frames
			bool half = mv && c >= 0x12 && c + 1 <= 0x1A && num * 2 >= den;
			IceBlockWhole(frame, &lm, c, shl32(c, 1) + (half ? 1 : 0), half);
		}
		else if (g_recs_block0.tick == g_real_tick)
		{
			LerpRecordsNum(g_recs_copy, g_recs_block0.b, (const uint8_t *)(MB() + 0x20000), BLOCK_BYTES, true, mv ? num : 0, den);
			IceBlockShatter(frame, &lm, c, (uint32_t)g_recs_copy, false);
		}
	}

	static void DustHeld(const Mat4x3 *frame, const void *owner)
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
			DustOne(h, s, &mm.r.x, mm.r.size, mm.r.age); // standing sparkles: the frame drawn
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
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
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
			case ORIG_DustTask: DustHeld(&frame, n); break;
			default: break;
			}
		}
		memcpy((void *)SPRITE_MAT, sprite_mat, sizeof(sprite_mat));
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
