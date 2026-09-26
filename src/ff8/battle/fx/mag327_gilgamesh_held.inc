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

// 30 fps part of mag327_gilgamesh.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag327_gilgamesh.cpp)
//
// Held frames (30 fps). Everything is drawn half way between the state drawn on the last real
// tick (memos) and the state the next tick will draw; flipbook frames, spawns, the flare's
// flicker and the materialise threshold keep their 15 Hz steps. The frame matrix 0x21FF488 is
// rebuilt from the held-frame camera while the timeline (its owner) is alive and put back after
// (the original renderers read it by address), like the screen-sprite matrix 0x2201060 and the
// clip renderer's frame pointer 0x21FF3C0. Model and entity skeletons touched by a draw are put
// back; model renderers get a private vertex scratch.

namespace ff8fx
{
namespace g327
{
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false; // the 0x201 flags were set on that tick (nothing advanced)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
	}

	static int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// ---- held-frame memos ----
	static NodeMemo<Node24, 256> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return !g_tick_paused && n->c == (int16_t)(m->c + 1); }

	// pool records as drawn on the current real tick (before their update)
	struct RecMemoA { uint32_t tick; const void *owner; RecA r; };
	struct RecMemoB { uint32_t tick; const void *owner; RecB r; };
	static RecMemoA g_memo_a[POOL_N];
	static RecMemoB g_memo_b[POOL_N];
	static void MemoA(int i, const void *owner) { g_memo_a[i].tick = g_real_tick; g_memo_a[i].owner = owner; g_memo_a[i].r = *RA(i); }
	static void MemoB(int i, const void *owner) { g_memo_b[i].tick = g_real_tick; g_memo_b[i].owner = owner; g_memo_b[i].r = *RB(i); }
	// the record moved on on the last tick (alive, same owner bits, next frame)
	static bool AdvancedA(const RecMemoA &m, const RecA *r) { return !g_tick_paused && r->mask == m.r.mask && r->age != m.r.age; }
	static bool AdvancedB(const RecMemoB &m, const RecB *r) { return !g_tick_paused && r->mask == m.r.mask && r->age != m.r.age; }

	// held draws give the model renderers a private vertex scratch (through g_vertex_scratch_override)
	static uint8_t g_vertex_scratch[0x20000];
	static bool g_held = false;

	// in-between value of a fade that is linear in the counter, when both ticks fade
	static int32_t FadeLerp(bool (*f)(int32_t, int32_t *), int32_t c, bool moved, int num, int den, bool *fade)
	{
		int32_t v0 = 0, v1 = 0;
		*fade = f(c, &v0);
		if (moved && *fade && f(c + 1, &v1)) return lerp_i(v0, v1, num, den);
		return v0;
	}

	// ---- trail: the splines drawn on the real tick ----
	struct TrailMemo { uint32_t tick; bool drawn; uint8_t sa[0x100], sb[0x100]; };
	static TrailMemo g_trail = { 0xFFFFFFFF };

	static void held_note_trail(int32_t cnt)
	{
		g_trail.tick = g_real_tick;
		g_trail.drawn = cnt > 1;
	}

	static void held_note_trail_splines()
	{
		memcpy(g_trail.sa, (const void *)TRAIL_SPL_A, 0x100);
		memcpy(g_trail.sb, (const void *)TRAIL_SPL_B, 0x100);
	}

	// ---- sword particles: the sword tip (ground point) the real tick used ----
	struct TipMemo { int16_t x, z; };
	static NodeMemo<TipMemo, 64> g_tip_memo;

	// SwordTipFrame left the tip's x / z at s+0 / s+4
	static void held_note_tip(const Node24 *n, const uint8_t *s)
	{
		if (TipMemo *m = g_tip_memo.put(n))
		{
			m->x = *(const int16_t *)(s + 0);
			m->z = *(const int16_t *)(s + 4);
		}
	}

	// SwordTipFrame at a remembered tip (the sword's bones have moved on since)
	static void SwordTipFrameAt(uint8_t *s, const TipMemo &tip)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int16_t *)(s + 0) = tip.x;
		*(int16_t *)(s + 4) = tip.z;
		*(int32_t *)(s + 0x1C) = *(const int16_t *)(s + 0);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = *(const int16_t *)(s + 4);
		ComposeAffineTransform(&Frame(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
	}

	// SwordPuffSetup at a remembered tip
	static void SwordPuffSetupAt(uint8_t *h, uint8_t *s, const TipMemo &tip)
	{
		*(uint32_t *)h = 0xCC6C34;
		*(uint16_t *)(h + 0x24) = 8;
		SwordTipFrameAt(s, tip);
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	// ---- held-frame memo of every model block: the head, skeleton and reader command as drawn ----
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct ModelMemo
	{
		uint32_t tick;
		const void *owner;    // task node that drew it
		bool midpoint;        // advanced after the draw: the next tick draws the pose just read
		int kind;
		int32_t threshold;    // materialise pass
		uint32_t skel_size;
		uint8_t head[0x9C];
		uint8_t skel[SKEL_MAX];
	};
	static ModelMemo g_model[11];

	static int ModelSlot(const uint8_t *E)
	{
		uint32_t a = (uint32_t)E;
		if (a >= 0x21FF4F0 && a < 0x21FF760 && (a - 0x21FF4F0) % 0x9C == 0) return (int)((a - 0x21FF4F0) / 0x9C);
		if (E == GIL1) return 4;
		if (E == GIL2) return 5;
		if (a >= 0x21FF800 && a < 0x21FFA70 && (a - 0x21FF800) % 0x9C == 0) return 6 + (int)((a - 0x21FF800) / 0x9C);
		if (E == GIL3) return 10;
		return -1;
	}

	static uint8_t *Skeleton(uint8_t *E)
	{
		uint8_t *com = *(uint8_t **)(E + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void ModelMemoTake(uint8_t *E, const void *owner, int kind, int32_t threshold)
	{
		if (g_held) return;
		int k = ModelSlot(E);
		if (k < 0) return;
		ModelMemo &m = g_model[k];
		m.tick = 0xFFFFFFFF;
		uint8_t *sk = Skeleton(E);
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.head, E, sizeof(m.head));
		m.owner = owner;
		m.kind = kind;
		m.threshold = threshold;
		m.midpoint = false;
		m.tick = g_real_tick;
	}

	// AdvanceModel read the next pose after the draw: the next tick draws the pose just read
	static void held_note_advanced(uint8_t *E)
	{
		int k = ModelSlot(E);
		if (k >= 0 && g_model[k].tick == g_real_tick) g_model[k].midpoint = true;
	}

	// ---- held frames: the node tasks ----
	static const Node24 *HeldMemo(const Node24 *n, bool *mv)
	{
		const Node24 *m = g_node_memo.get(n);
		if (m) *mv = NodeMoved(m, n);
		return m;
	}

	static void FadeTileHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t v = FadeTileLevel(m->c);
		if (mv) v = lerp_i(v, FadeTileLevel(m->c + 1), num, den);
		Tile(v, v, v, 0);
	}

	static void ScreenPrimHeld(const Node24 *n, uint32_t orig, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int16_t s = (mv && m->c < 6) ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C;
		ScreenPrimDraw(m, ScreenPrimModel(m, orig), s);
	}

	static int32_t SliceFadeW(int32_t c) { return 0x1000 - ComputeSin(shl32(c - 0x18, 10) / 16); }

	static uint8_t g_ent_head[0x30];
	static uint8_t g_skel_save[4][SKEL_MAX];

	static uint8_t *EntitySkeleton(uint8_t *anim_header, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(anim_header + 4);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	static void SliceHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t slot = m->e;
		uint8_t *ent = Entity((uint32_t)slot);
		// the cut half's matrix half way (its cut task's memo), else the one drawn
		Mat4x3 moved = *(const Mat4x3 *)(0x2200B94 + 60 * slot);
		for (TaskNode *t = Q1().head; t; t = t->next)
		{
			const Node24 *cut = (const Node24 *)t;
			if ((uint32_t)t->func != ORIG_Zan_CutTask || cut->e != m->e) continue;
			bool cmv = false;
			const Node24 *cm = HeldMemo(cut, &cmv);
			if (cm && cmv && cm->c < 0x28)
			{
				int16_t A[4];
				int16_t a0 = Lerp16(CutAngle0(cm->c, cm->a18), CutAngle0(cm->c + 1, cm->a18), num, den);
				int16_t a1 = Lerp16(CutAngle1(cm->c, cm->a18), CutAngle1(cm->c + 1, cm->a18), num, den);
				CutMatrix(cm, a0, a1, *(const int16_t *)(0x2200B80 + 60 * slot), lerp_i(cm->s20, cut->s20, num, den), &moved, A);
			}
			break;
		}
		int32_t w = -1;
		if (m->c >= 0x18)
		{
			w = SliceFadeW(m->c);
			if (mv && m->c + 1 < 0x28) w = lerp_i(w, SliceFadeW(m->c + 1), num, den);
		}
		// the draw recolours the target and rebuilds its local bone matrices: put both back
		uint32_t size = 0;
		uint8_t *sk = EntitySkeleton(ent + 0x60, &size);
		if (!sk) return;
		memcpy(g_ent_head, ent, sizeof(g_ent_head));
		memcpy(g_skel_save[0], sk, size);
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
		SliceDraw(h, m, ent, &moved, m->c, w);
		FieldFree(0x1D8);
		memcpy(sk, g_skel_save[0], size);
		memcpy(ent, g_ent_head, sizeof(g_ent_head));
	}

	static void PartyClipHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t z = mv ? lerp_i(m->s20, n->s20, num, den) : m->s20;
		uint8_t *sk[4];
		uint32_t size[4];
		for (int i = 0; i < 4; i++)
		{
			sk[i] = EntitySkeleton((uint8_t *)(0x1D98991 + 0x2C * i + 0x13), &size[i]);
			if (sk[i]) memcpy(g_skel_save[i], sk[i], size[i]);
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
		PartyClipDraw(h, z);
		FieldFree(0x1D8);
		for (int i = 0; i < 4; i++)
			if (sk[i]) memcpy(sk[i], g_skel_save[i], size[i]);
	}

	static void MasaPrimHeld(const Node24 *n, bool b, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(b ? MasaFadeB : MasaFadeA, m->c, mv, num, den, &fade);
		MasaPrimDraw(m, b, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	static void ExcaPillarHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(MasaFadeB, m->c, mv, num, den, &fade);
		int16_t s1C = mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C;
		int16_t s20 = mv ? Lerp16(m->s20, n->s20, num, den) : m->s20;
		ExcaPillarDraw(m, m->a18, s1C, s20, fade, f);
	}

	static void RingHeld(const Node24 *n)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (m) RingDraw(m);
	}

	static void RingSpinHeld(const Node24 *n, bool b, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		RingSpinDraw(m, b, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18);
	}

	static void FlareHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t spread = m->s20, depth = FlareDepth(m->c);
		if (mv)
		{
			spread = lerp_i(m->s20, n->s20, num, den);
			depth = lerp_i(depth, FlareDepth(m->c + 1), num, den);
		}
		FlareDraw(m, spread, depth, FlareColour(m->c));
	}

	static void RiserHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(RiserFade, m->c, mv, num, den, &fade);
		RiserDraw(m, mv ? Lerp16(m->p12, n->p12, num, den) : m->p12, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	static void RisingRingHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(RisingRingFade, m->c, mv, num, den, &fade);
		RisingRingDraw(m, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18, mv ? Lerp16(m->s20, n->s20, num, den) : m->s20, fade, f);
	}

	static void SpinnerHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(SpinnerFade, m->c, mv, num, den, &fade);
		SpinnerDraw(m, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	// ---- pool records: the memo record half way to the record now (when it moved on), frame kept ----
	static RecA HeldRecA(const RecMemoA &mm, int i, int num, int den)
	{
		RecA t = mm.r;
		const RecA *cur = RA(i);
		if (AdvancedA(mm, cur))
		{
			t.x = Lerp16(mm.r.x, cur->x, num, den);
			t.y = Lerp16(mm.r.y, cur->y, num, den);
			t.z = Lerp16(mm.r.z, cur->z, num, den);
			t.size = Lerp16(mm.r.size, cur->size, num, den);
			t.w18 = lerp_angle(mm.r.w18, cur->w18, num, den);
			t.w1C = lerp_angle(mm.r.w1C, cur->w1C, num, den);
		}
		return t;
	}

	static RecB HeldRecB(const RecMemoB &mm, int i, int num, int den)
	{
		RecB t = mm.r;
		const RecB *cur = RB(i);
		if (AdvancedB(mm, cur))
		{
			t.x = Lerp16(mm.r.x, cur->x, num, den);
			t.y = Lerp16(mm.r.y, cur->y, num, den);
			t.z = Lerp16(mm.r.z, cur->z, num, den);
			t.size = Lerp16(mm.r.size, cur->size, num, den);
		}
		return t;
	}

	enum { LIT_MAC3, LIT_MAC3_ANGLE, LIT_MAC2, LIT_PLAIN };

	static void LitPoolHeldA(const void *owner, uint8_t *h, uint8_t *s, int kind, int num, int den)
	{
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			RecA t = HeldRecA(mm, i, num, den);
			if (kind == LIT_PLAIN) LitSprite(h, &t.x, t.size, t.age);
			else LitSpriteMAC(h, s, &t.x, t.size, t.age, kind == LIT_MAC2 ? 2 : 3, kind == LIT_MAC3_ANGLE, t.w1C);
		}
	}

	static void LitPoolHeldB(const void *owner, uint8_t *h, uint8_t *s, int kind, int num, int den)
	{
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoB &mm = g_memo_b[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			RecB t = HeldRecB(mm, i, num, den);
			if (kind == LIT_PLAIN) LitSprite(h, &t.x, t.size, t.age);
			else LitSpriteMAC(h, s, &t.x, t.size, t.age, kind == LIT_MAC2 ? 2 : 3, false, 0);
		}
	}

	static void MasaSparkHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		MasaSparkSetup(h, s, n);
		LitPoolHeldA(n, h, s, LIT_MAC3_ANGLE, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void MasaDustHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		MasaDustSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_MAC3, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaBurstHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaBurstSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_MAC3, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaSmokeHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaSmokeSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_PLAIN, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaStreakHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x88);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaStreakSetup(h, s, n);
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			ExcaStreakOne(h, s, &mm.r.w18, &t.x, t.size);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		FieldFree(0xB4);
		FieldFree(0x88);
	}

	static void PuffHeld(const Node24 *n, bool a, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		PuffSetup(h, s);
		if (a) LitPoolHeldA(n, h, s, LIT_PLAIN, num, den);
		else LitPoolHeldB(n, h, s, LIT_PLAIN, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void DotHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x58);
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		uint32_t pk = Cursor();
		*(uint32_t *)(s + 0x48) = OT();
		*(uint32_t *)(s + 0x4C) = 4;
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			GteLoadV0(&t.x);
			GteRTPS();
			*(uint32_t *)pk = 0x2000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)(0xCD0428 + 4 * mm.r.age);
			pk = DotEmit(s, pk);
		}
		Cursor() = pk;
		FieldFree(0x58);
	}

	static void SwordShardHeld(const Node24 *n, int num, int den)
	{
		const TipMemo *tm = g_tip_memo.get(n);
		if (!tm) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = 0xCC8E64;
		SwordTipFrameAt(s, *tm);
		*(int16_t *)(s + 2) = 0;
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			ShardDrawOne(h, s, t.w18, t.w1C, &t.x, t.size);
			Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		}
		FieldFree(0x58);
		FieldFree(0x68);
	}

	static void SwordPuffHeld(const Node24 *n, int num, int den)
	{
		const TipMemo *tm = g_tip_memo.get(n);
		if (!tm) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		SwordPuffSetupAt(h, s, *tm);
		LitPoolHeldB(n, h, s, LIT_MAC2, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	// ---- trail: the next tick's history is predicted exactly (the sword's base and tip now go into
	// slot c % 10), both splined on private buffers, the samples drawn half way ----
	static uint8_t g_trail_ring[0xC8], g_trail_pa[0x50], g_trail_pb[0x50];
	static uint8_t g_trail_na[0x100], g_trail_nb[0x100], g_trail_ma[0x100], g_trail_mb[0x100];

	static void TrailHeld(const Node24 *n, int num, int den)
	{
		if (g_trail.tick != g_real_tick || !g_trail.drawn) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		const uint8_t *sa = g_trail.sa, *sb = g_trail.sb;
		if (!g_tick_paused)
		{
			memcpy(g_trail_ring, (const void *)TRAIL_RING, sizeof(g_trail_ring));
			TrailPoints(s);
			int32_t idx = (int32_t)n->c % 10;
			uint8_t *e = g_trail_ring + 0x14 * idx;
			*(uint32_t *)e = 1;
			memcpy(e + 4, s + 0x20, 8);
			memcpy(e + 0xC, s + 0x28, 8);
			int32_t cnt = TrailGather(g_trail_ring, idx, g_trail_pa, g_trail_pb);
			if (cnt > 1)
			{
				TrailSplines(cnt, g_trail_pa, g_trail_pb, g_trail_na, g_trail_nb);
				const int16_t *a0 = (const int16_t *)g_trail.sa, *a1 = (const int16_t *)g_trail_na;
				const int16_t *b0 = (const int16_t *)g_trail.sb, *b1 = (const int16_t *)g_trail_nb;
				int16_t *ma = (int16_t *)g_trail_ma, *mb = (int16_t *)g_trail_mb;
				for (int k = 0; k < 0x80; k++)
				{
					ma[k] = Lerp16(a0[k], a1[k], num, den);
					mb[k] = Lerp16(b0[k], b1[k], num, den);
				}
				sa = g_trail_ma;
				sb = g_trail_mb;
			}
		}
		TrailDraw(sa, sb, s);
		FieldFree(0x40);
	}

	// ---- models: the drawn head, skeleton and command put back, the midpoint pose, the same draw ----
	static uint8_t g_head_cur[0x9C];
	static uint8_t g_skel_cur[SKEL_MAX];

	static void ModelHeld(uint8_t *E, const void *owner, int num, int den)
	{
		int k = ModelSlot(E);
		if (k < 0) return;
		ModelMemo &m = g_model[k];
		if (m.tick != g_real_tick || m.owner != owner) return;
		uint8_t *sk = Skeleton(E);
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		memcpy(g_head_cur, E, sizeof(g_head_cur));
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(E, m.head, sizeof(m.head));
		memcpy(sk, m.skel, m.skel_size);
		if (m.midpoint && !g_tick_paused) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		switch (m.kind)
		{
		case DK_NORMAL4: DrawModel(E, &Frame(), 4); break;
		case DK_NORMAL3: DrawModel(E, &Frame(), 3); break;
		case DK_CLIP:
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
			ClipSetup(h);
			ClipDraw(h, E, &Frame());
			FieldFree(0x1A8);
			break;
		}
		case DK_MATERIAL:
			if (m.threshold > 0)
			{
				uint8_t *h = (uint8_t *)FieldAlloc(0xB0);
				*(uint32_t *)(h + 0x2C) = MB() + 0x22000;
				*(int32_t *)(h + 0x44) = m.threshold;
				MaterialDraw(h, E);
				FieldFree(0xB0);
			}
			break;
		}
		memcpy(E, g_head_cur, sizeof(g_head_cur));
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void HeldTask(const Node24 *n, uint32_t f, int num, int den)
	{
		switch (f)
		{
		case ORIG_FeedbackTask: RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F); break;
		case ORIG_Feedback2Task:
		{
			int32_t c = g_tick_paused ? n->c : n->c - 1; // the counter the last tick issued with
			if (c > 0) RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
			break;
		}
		case ORIG_FadeTileTask: FadeTileHeld(n, num, den); break;
		case ORIG_Zan_ScreenPrimTask:
		case ORIG_Masa_ScreenPrimTask:
		case ORIG_Exca_ScreenPrimTask: ScreenPrimHeld(n, f, num, den); break;
		case ORIG_Zan_SliceTask: SliceHeld(n, num, den); break;
		case ORIG_Zan_PartyClipTask: PartyClipHeld(n, num, den); break;
		case ORIG_Masa_PrimATask: MasaPrimHeld(n, false, num, den); break;
		case ORIG_Masa_PrimBTask: MasaPrimHeld(n, true, num, den); break;
		case ORIG_Masa_SparkTask: MasaSparkHeld(n, num, den); break;
		case ORIG_Masa_DustTask: MasaDustHeld(n, num, den); break;
		case ORIG_Exca_BurstTask: ExcaBurstHeld(n, num, den); break;
		case ORIG_Exca_SmokeTask: ExcaSmokeHeld(n, num, den); break;
		case ORIG_Exca_StreakTask: ExcaStreakHeld(n, num, den); break;
		case ORIG_Exca_PillarTask: ExcaPillarHeld(n, num, den); break;
		case ORIG_RingTask: RingHeld(n); break;
		case ORIG_RingSpinTask: RingSpinHeld(n, false, num, den); break;
		case ORIG_RingSpin2Task: RingSpinHeld(n, true, num, den); break;
		case ORIG_FlareTask: FlareHeld(n, num, den); break;
		case ORIG_RiserTask: RiserHeld(n, num, den); break;
		case ORIG_PuffTask: PuffHeld(n, false, num, den); break;
		case ORIG_SparkTask: PuffHeld(n, true, num, den); break;
		case ORIG_RisingRingTask: RisingRingHeld(n, num, den); break;
		case ORIG_DotTask: DotHeld(n, num, den); break;
		case ORIG_SpinnerTask: SpinnerHeld(n, num, den); break;
		case ORIG_TrailTask: TrailHeld(n, num, den); break;
		case ORIG_SwordShardTask: SwordShardHeld(n, num, den); break;
		case ORIG_SwordPuffTask: SwordPuffHeld(n, num, den); break;
		case ORIG_SwordClipTask:
		case ORIG_SwordDrawTask:
			for (int i = 0; i < 4; i++) ModelHeld(Sword(i), n, num, den);
			break;
		case ORIG_Gil1Task:
		case ORIG_Gil1DrawTask: ModelHeld(GIL1, n, num, den); break;
		case ORIG_Gil2DrawTask:
			ModelHeld(GIL2, n, num, den);
			for (int j = 0; j < 4; j++) ModelHeld(Arm(j), n, num, den);
			break;
		case ORIG_Gil3DrawTask:
			ModelHeld(GIL3, n, num, den);
			for (int i = 0; i < 4; i++) ModelHeld(Sword(i), n, num, den);
			break;
		default: break;
		}
	}

	static uint8_t g_held_packets[0x100000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order (Q1 then Q2); packets go to a private buffer (the module draws
	// through the engine cursor battle_texture_data_ptr_1D8E054, redirected and put back)
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		g_held = true;
		g_vertex_scratch_override = (uint32_t)g_vertex_scratch;
		Mat4x3 frame = Frame();
		uint8_t screen_mat[0x20];
		memcpy(screen_mat, (const void *)0x2201060, sizeof(screen_mat));
		uint32_t frame_ptr = var<uint32_t>(0x21FF3C0);
		bool timeline = false;
		for (TaskNode *t = Q1().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		for (TaskNode *t = Q1().head; t; t = t->next) HeldTask((const Node24 *)t, (uint32_t)t->func, num, den);
		for (TaskNode *t = Q2().head; t; t = t->next) HeldTask((const Node24 *)t, (uint32_t)t->func, num, den);
		var<uint32_t>(0x21FF3C0) = frame_ptr;
		memcpy((void *)0x2201060, screen_mat, sizeof(screen_mat));
		Frame() = frame;
		g_vertex_scratch_override = 0;
		g_held = false;
		Cursor() = cursor;
	}
}

	// One set of ports serves the four ids 327-330 (the tasks are shared, the variant is data):
	// the held-frame functions are registered under each id.
	static void register_mag327_held()
	{
		mark_held(g327::ORIG_FadeTileTask);
		mark_held(g327::ORIG_RingTask);
		mark_held(g327::ORIG_RingSpinTask);
		mark_held(g327::ORIG_RingSpin2Task);
		mark_held(g327::ORIG_FlareTask);
		mark_held(g327::ORIG_RiserTask);
		mark_held(g327::ORIG_PuffTask);
		mark_held(g327::ORIG_SparkTask);
		mark_held(g327::ORIG_RisingRingTask);
		mark_held(g327::ORIG_DotTask);
		mark_held(g327::ORIG_SpinnerTask);
		mark_held(g327::ORIG_TrailTask);
		mark_held(g327::ORIG_SwordShardTask);
		mark_held(g327::ORIG_SwordPuffTask);
		mark_held(g327::ORIG_SwordClipTask);
		mark_held(g327::ORIG_SwordDrawTask);
		mark_held(g327::ORIG_Gil1Task);
		mark_held(g327::ORIG_Gil1DrawTask);
		mark_held(g327::ORIG_Gil2DrawTask);
		mark_held(g327::ORIG_Gil3DrawTask);
		mark_held(g327::ORIG_Zan_ScreenPrimTask);
		mark_held(g327::ORIG_Zan_SliceTask);
		mark_held(g327::ORIG_Zan_PartyClipTask);
		mark_held(g327::ORIG_Masa_ScreenPrimTask);
		mark_held(g327::ORIG_Masa_PrimATask);
		mark_held(g327::ORIG_Masa_PrimBTask);
		mark_held(g327::ORIG_Masa_SparkTask);
		mark_held(g327::ORIG_Masa_DustTask);
		mark_held(g327::ORIG_Exca_ScreenPrimTask);
		mark_held(g327::ORIG_Exca_BurstTask);
		mark_held(g327::ORIG_Exca_SmokeTask);
		mark_held(g327::ORIG_Exca_StreakTask);
		mark_held(g327::ORIG_Exca_PillarTask);
		for (int id = 327; id <= 330; id++) register_module_held(id, g327::HeldReady, g327::HeldFrame);
	}
}
