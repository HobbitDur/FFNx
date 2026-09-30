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

// 30 fps part of mag116_quezacotl.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag116_quezacotl.cpp)

namespace ff8fx
{
namespace q116
{
	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// The nodes of the queues whose tasks end on a tick after drawing on it (arcs, flashes,
	// debris: draw, update, TASK_END), as the tick starts. A task that ends on tick N has drawn
	// its last picture on N, which vanilla keeps on screen for the whole tick: the held frame
	// after N draws that picture again (from these copies and the memos of the tick).
	template<typename N, int CAP> struct QueueSnap { int n; TaskNode *node[CAP]; N copy[CAP]; };
	static QueueSnap<ArcNode, 16> g_snap_arc;       // pool 0x10 x 0x80 (creature parts)
	static QueueSnap<FlashNode, 64> g_snap_flash;   // pool 0x40 x 0x14
	static QueueSnap<DebrisNode, 32> g_snap_debris; // pool 0x20 x 0x2C

	template<typename N, int CAP> static void SnapQueue(QueueSnap<N, CAP> &q, TaskNode *head, uint32_t func)
	{
		q.n = 0;
		for (TaskNode *t = head; t && q.n < CAP; t = t->next)
			if ((uint32_t)t->func == func) { q.node[q.n] = t; memcpy(&q.copy[q.n], t, sizeof(N)); q.n++; }
	}

	static bool Queued(const TaskNode *head, const TaskNode *n)
	{
		for (const TaskNode *t = head; t; t = t->next) if (t == n) return true;
		return false;
	}

	template<typename N, int CAP> static int SnapIndex(const QueueSnap<N, CAP> &q, const TaskNode *n)
	{
		for (int i = 0; i < q.n; i++) if (q.node[i] == n) return i;
		return -1;
	}

	// The bolt (0x6C7EA0) and branch (0x6C7800) tasks as the tick starts, with their chains. A tick
	// grows a chain, fades it, draws it, then frees its dark root links (the bolt task ends when
	// fewer than two are left, and a freed link may be reused by a later task of the same tick):
	// the chain tick N drew = the root links of these copies that are gone now + the chain left.
	template<typename Node, typename V, int NCAP, int VCAP> struct ChainSnap
	{
		int n, nv;
		Node *node[NCAP]; Node copy[NCAP]; int first[NCAP], cnt[NCAP];
		V *vp[VCAP]; V v[VCAP];
		void take(TaskNode *head, uint32_t func)
		{
			n = nv = 0;
			for (TaskNode *t = head; t && n < NCAP; t = t->next)
			{
				if ((uint32_t)t->func != func) continue;
				Node *b = (Node *)t;
				node[n] = b; copy[n] = *b; first[n] = nv; cnt[n] = 0;
				for (V *x = b->head; x && nv < VCAP; x = x->next) { vp[nv] = x; v[nv] = *x; nv++; cnt[n]++; }
				n++;
			}
		}
		int index(const TaskNode *t) const { for (int i = 0; i < n; i++) if ((const TaskNode *)node[i] == t) return i; return -1; }
		int find(int i, const V *x) const { for (int k = 0; k < cnt[i]; k++) if (vp[first[i] + k] == x) return k; return -1; }
	};
	static ChainSnap<BoltNode, BoltVert, 32, 256> g_snap_bolt;      // pool 0x20 nodes, 256 vertices
	static ChainSnap<BranchNode, BoltSeg, 128, 512> g_snap_branch; // pool 0x80 nodes, 512 segments

	// the branch node pool as the tick starts: a task spawned on the tick that also ended on it
	// before drawing (path too short) is gone from the queue, not from the pool
	static uint8_t g_pool0_branch[0x80 * 0x20];
	static const void *g_pool0_at = nullptr;

	// The ribbon joint direction each draw function carries from chain to chain (the vanilla
	// uninitialised stack slot, g_dir_*), as the tick starts: the held frame chains it through the
	// tick's chains in the tick's order, so a chain whose first point is behind the camera gets
	// the value the real tick gave it (the port's own carries are not touched)
	static int16_t g_carry0_bolt[2], g_carry0_branch[2], g_carry0_arc[2];

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		SnapQueue(g_snap_arc, QueueCreature().first.head, ORIG_ArcTask);
		SnapQueue(g_snap_flash, QueueDebris().second.head, ORIG_FlashTask);
		SnapQueue(g_snap_debris, QueueDebris().first.head, ORIG_DebrisTask);
		g_snap_bolt.take(QueueBolts().second.head, ORIG_BoltTask);
		g_snap_branch.take(QueueBolts().first.head, ORIG_BranchTask);
		const TaskQueue &bq = QueueBolts().first;
		g_pool0_at = bq.pool && bq.node_size == 0x20 && bq.capacity == 0x80 ? bq.pool : nullptr;
		if (g_pool0_at) memcpy(g_pool0_branch, g_pool0_at, sizeof(g_pool0_branch));
		memcpy(g_carry0_bolt, g_dir_bolt, sizeof(g_carry0_bolt));
		memcpy(g_carry0_branch, g_dir_branch, sizeof(g_carry0_branch));
		memcpy(g_carry0_arc, g_dir_arc, sizeof(g_carry0_arc));
	}

	struct DebrisMemo { int16_t pos[3], rot[3]; };
	static NodeMemo<DebrisMemo, 256> g_debris_memo;

	// debris: the chunk as drawn, before the tick's update
	static void held_note_debris(DebrisNode *p)
	{
		if (DebrisMemo *m = g_debris_memo.put(p))
		{
			memcpy(m->pos, p->pos, sizeof(m->pos));
			memcpy(m->rot, p->rot, sizeof(m->rot));
		}
	}

	// jitter of the 14 inner points: the random values of the real tick are kept so a held
	// frame redraws the same arc shape without touching the module generator
	struct ArcMemo { int16_t progress; int16_t color[16]; int32_t rnd[14][3]; };
	static NodeMemo<ArcMemo, 256> g_arc_memo;
	static ArcMemo g_arc_local, *g_arc_cur = &g_arc_local;

	// arc: progress and colours it is drawn with (the memo the jitter values of this draw go to)
	static void held_note_arc(ArcNode *arc)
	{
		ArcMemo *m = g_arc_memo.put(arc);
		if (!m) m = &g_arc_local;
		m->progress = arc->progress;
		for (int k = 0; k < 16; k++) m->color[k] = arc->pts[k].color;
		g_arc_cur = m;
	}

	// the three random values of inner point i (1..14) of the arc being drawn
	struct ArcNext { int32_t rnd[14][3]; };

	static void held_note_arc_jitter(int i, const int32_t *rv)
	{
		memcpy(g_arc_cur->rnd[i - 1], rv, sizeof(g_arc_cur->rnd[i - 1]));
	}

	// ---- the next tick's arc jitter ----
	// The arcs take their jitter from the module generator (0x6C7CF0, seed 0x25217A0) right after
	// the creature task, the only other user before them in the master's order. Its draws on the
	// next tick come from its timeline: 8 at the bolt spawns of 88..100 (even), 7 at 157..175
	// (odd), 5 at 232..266, plus SpawnBolt's 3 (shape) when it gets a vertex and a task node.
	// Then every queued arc takes 42 values (14 inner points x 3) in queue order (new arcs are
	// appended behind). Not predicted (held frames replay the tick's jitter): the branch spawns of
	// 257..302 (no arcs then) and 247 / 256, where the tick may stop on a busy stream load first.
	static uint32_t g_arcnext_key = 0xFFFFFFFF;
	static bool g_arc_next_ok = false;
	static NodeMemo<ArcNext, 256> g_arc_next;

	static bool PoolHasFree(const TaskQueue &q)
	{
		const uint8_t *p = (const uint8_t *)q.pool;
		for (int i = 0; p && i < q.capacity; i++, p += q.node_size) if (!(*p & 1)) return true;
		return false;
	}

	static bool VertFree()
	{
		if (VertFreeHint()) return true;
		const BoltVert *v = VertPool();
		for (int i = 0; i < 256; i++, v++) if (v->width == -1) return true;
		return false;
	}

	// the creature task's generator draws on the next tick (false: not predictable)
	static bool CreatureRands(int *k)
	{
		*k = 0;
		if (!QueueCreature().second.head) return true; // the creature task has ended
		int16_t c = *(int16_t *)(ModelBuffer() + 0x0C); // the next tick's counter
		bool paused = Pause() != 0;
		uint32_t v7 = (uint32_t)((int32_t)c - 86), tb = (uint32_t)((int32_t)c - 157);
		uint32_t tc = (uint32_t)((int32_t)c - 232), td = (uint32_t)((int32_t)c - 257);
		if (td < 0x2E || c == 247 || c == 256) return false;
		int n = 0;
		bool spawn = false;
		if (!paused && v7 != 0 && v7 < 15 && (v7 & 1) == 0) { n += 8; spawn = true; }
		if (!paused && tb < 0x14 && (tb & 1) == 0) { n += 7; spawn = true; }
		if (tc < 0x23) { n += 5; spawn = true; }
		if (spawn && VertFree() && PoolHasFree(QueueBolts().second)) n += 3;
		*k = n;
		return true;
	}

	static void ComputeArcNext()
	{
		if (g_arcnext_key == g_real_tick) return;
		g_arcnext_key = g_real_tick;
		g_arc_next_ok = false;
		int k;
		if (!CreatureRands(&k)) return;
		uint32_t seed = var<uint32_t>(0x25217A0);
		for (int i = 0; i < k; i++) seed = (seed * 125 + 14) & 0x7FFF;
		for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
		{
			if ((uint32_t)t->func != ORIG_ArcTask) continue;
			ArcNext *a = g_arc_next.put(t);
			if (!a) return; // memo full: no prediction
			for (int i = 0; i < 14; i++)
				for (int c = 0; c < 3; c++)
				{
					seed = (seed * 125 + 14) & 0x7FFF;
					a->rnd[i][c] = (int32_t)seed - 0x4000;
				}
		}
		g_arc_next_ok = true;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn half way
	// between the state drawn on the last real tick and the next one; what the vanilla code
	// creates at random steps (bolt growth, forks, flashes) keeps the 15 Hz batches.
	// ------------------------------------------------------------------
	static void DebrisHeld(DebrisNode *p, int num, int den)
	{
		const DebrisMemo *m = g_debris_memo.get(p);
		if (!m) return;
		int16_t pos[3], rot[3];
		for (int k = 0; k < 3; k++)
		{
			pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
			rot[k] = lerp_angle(m->rot[k], p->rot[k], num, den);
		}
		DebrisDraw(pos, rot);
	}

	// Bolt vertices: drawn at P, then drifted by vel at the end of the tick; the next draw
	// applies one fade step first. Drawn on copies (the real vertices keep their state).
	static BoltVert g_held_verts[256];

	// one tick's fade of a bolt vertex (0x6C7EA0)
	static void BoltFade(BoltVert &v)
	{
		if (v.intensity <= 0) return;
		v.intensity += v.decay;
		if (v.intensity <= 0) { v.intensity = 0; return; }
		if (v.hold != 0 && v.decay <= 0) { v.hold--; return; }
		if (v.decay > -0x180) v.decay -= 0x140;
	}

	// held link k: src as drawn on tick N (brightness, fade step, drift) at pos, drifted part of the
	// way to the next tick and half way to the next tick's first fade step
	static void BoltPut(int k, const BoltVert &src, const int16_t *pos, int num, int den)
	{
		BoltVert &h = g_held_verts[k];
		h = src;
		h.next = nullptr;
		if (k) g_held_verts[k - 1].next = &h;
		for (int c = 0; c < 3; c++) h.pos[c] = (int16_t)(pos[c] + src.vel[c] * num / den);
		int32_t i0 = src.intensity, i1 = i0;
		if (i0 > 0)
		{
			i1 = (int16_t)(i0 + src.decay);
			if (i1 <= 0) i1 = 0;
		}
		h.intensity = (int16_t)lerp_i(i0, i1, num, den);
		GteLoadV0(h.pos);
		GteRTPS();
		GteReadSXY2(&h.sx);
		GteReadOTZ(&h.otz);
	}

	static bool SameBolt(const BoltNode *a, const BoltNode *b)
	{
		return !memcmp(a->target, b->target, sizeof(a->target)) && a->dt == b->dt && a->kind == b->kind && a->width == b->width;
	}

	// the chain one bolt task drew on tick N, held: si = its tick-start copy (-1: spawned on the
	// tick), b = the task now (null: it ended on the tick, after drawing)
	static void BoltChainHeld(const BoltNode *b, int si, int num, int den, int16_t *carry)
	{
		const auto &sn = g_snap_bolt;
		int k = 0, gone = 0;
		if (si >= 0)
		{
			gone = b ? sn.find(si, b->head) : sn.cnt[si];
			if (gone < 0) gone = 0;
		}
		for (int j = 0; j < gone && k < 256; j++, k++) // root links freed after the draw
		{
			BoltVert f = sn.v[sn.first[si] + j];
			int16_t pos[3];
			memcpy(pos, f.pos, sizeof(pos)); // tick start = where the tick drew it
			BoltFade(f);
			BoltPut(k, f, pos, num, den);
		}
		if (b)
			for (const BoltVert *v = b->head; v && k < 256; v = v->next, k++)
			{
				int16_t pos[3];
				for (int c = 0; c < 3; c++) pos[c] = (int16_t)(v->pos[c] - v->vel[c]);
				BoltPut(k, *v, pos, num, den);
			}
		if (k >= 2) DrawRibbon<BoltVert, 0x200>(g_held_verts, k, carry);
	}

	// every bolt the tick drew, in its order: the tasks it started with, then the ones spawned on it
	static void BoltsHeld(int num, int den)
	{
		int16_t carry[2] = { g_carry0_bolt[0], g_carry0_bolt[1] };
		TaskNode *head = QueueBolts().second.head;
		for (int i = 0; i < g_snap_bolt.n; i++)
		{
			BoltNode *b = g_snap_bolt.node[i];
			bool same = (uint32_t)b->hdr.func == ORIG_BoltTask && SameBolt(b, &g_snap_bolt.copy[i]);
			bool alive = same && Queued(head, &b->hdr);
			if (!alive && same && b->count >= 2) continue; // (a bolt ends after drawing, root retracted to one vertex)
			BoltChainHeld(alive ? b : nullptr, i, num, den, carry);
		}
		for (TaskNode *t = head; t; t = t->next)
		{
			if ((uint32_t)t->func != ORIG_BoltTask) continue;
			int i = g_snap_bolt.index(t);
			if (i >= 0 && SameBolt((BoltNode *)t, &g_snap_bolt.copy[i])) continue;
			BoltChainHeld((BoltNode *)t, -1, num, den, carry); // (one vertex: not run yet, not drawn)
		}
	}

	// Branch: the path grows in 15 Hz batches (vanilla shapes); held frames draw the chain tick N
	// drew with each segment's brightness half way to the next fade step.
	static BoltSeg g_held_segs[512];

	static void SegFade(BoltSeg &x)
	{
		if (x.intensity > 0)
		{
			x.intensity += x.decay;
			if (x.intensity <= 0) x.intensity = 0;
		}
	}

	static void BranchPut(int k, const BoltSeg &src, int num, int den)
	{
		BoltSeg &h = g_held_segs[k];
		h = src;
		h.next = nullptr;
		if (k) g_held_segs[k - 1].next = &h;
		int32_t i0 = src.intensity, i1 = i0;
		if (i0 > 0)
		{
			i1 = (int16_t)(i0 + src.decay);
			if (i1 <= 0) i1 = 0;
		}
		h.intensity = (int16_t)lerp_i(i0, i1, num, den);
		GteLoadV0(BOLT_VERTICES + 8 * (int32_t)h.vertex);
		GteRTPS();
		GteReadSXY2(&h.sx);
		GteReadOTZ(&h.otz);
	}

	// the growth of a branch task on a tick (0x6C7800) replayed on its path script: the side-branch
	// spawns (their scripts; each one leaves 2, 0 in the draw's joint slot) and the script / count
	// after it. False: the task ends before drawing (path end at 3 segments or fewer).
	// joint slot (BranchDraw's direction local, task entry esp - 0x24) as the tick's growth leaves it:
	// each spawn (SpawnBranch 0x6C76E0 saves EBX = 2 there) writes 2, 0; each SegAlloc call (0x6C78B3)
	// its return address 0x006C78B8; slot_set = false: the growth wrote nothing
	struct BranchGrowth { int spawns; const int16_t *spawned[16]; const int16_t *script; int count; bool slot_set; int16_t slot[2]; };
	static void GrowSlot(BranchGrowth &g, bool alloc) { g.slot_set = true; g.slot[0] = alloc ? (int16_t)0x78B8 : 2; g.slot[1] = alloc ? 0x006C : 0; }
	static void CarryGrowth(int16_t *carry, const BranchGrowth &g) { if (g.slot_set) { carry[0] = g.slot[0]; carry[1] = g.slot[1]; } }

	static bool BranchGrow(const int16_t *script, int count, BranchGrowth &g)
	{
		g.spawns = 0;
		g.slot_set = false;
		for (int iter = 4; iter; iter--)
		{
			if (!script) continue;
			int16_t v = *script++;
			while (v < 0)
			{
				if (g.spawns < 16) g.spawned[g.spawns] = script;
				g.spawns++;
				GrowSlot(g, false);
				script += -1 - v;
				v = *script++;
			}
			const int16_t *peek = script;
			int16_t c = *peek;
			if (c < 0) { peek -= c; c = *peek; }
			if (c == 0x7FFF)
			{
				if (count <= 3) { g.script = script; g.count = count; return false; }
				script = nullptr;
			}
			GrowSlot(g, true);
			count++;
		}
		g.script = script;
		g.count = count;
		return true;
	}

	static bool SameBranch(const BranchNode *a, const BranchNode *b)
	{
		return a->width == b->width && a->intensity == b->intensity && a->decay == b->decay;
	}

	// a branch spawned on the tick (by a branch task's growth, or by the creature): its first
	// script, found as the one whose growth from one segment gives the task's script and count now
	static const int16_t *BranchStart(const BranchNode *b, const int16_t *const *spawned, int nsp)
	{
		BranchGrowth g;
		for (int i = 0; i < nsp; i++)
			if (spawned[i][0] == b->head->vertex && BranchGrow(spawned[i] + 1, 1, g) && g.script == b->script && g.count == b->count)
				return spawned[i] + 1;
		if (b->script)
			for (const int16_t *s0 = b->script; s0 > b->script - 2048; s0--)
				if (s0[-1] == b->head->vertex && BranchGrow(s0, 1, g) && g.script == b->script && g.count == b->count)
					return s0;
		return nullptr;
	}

	// the chain one branch task drew on tick N, held (si / b as BoltChainHeld)
	static void BranchChainHeld(const BranchNode *b, int si, int num, int den, int16_t *carry)
	{
		const auto &sn = g_snap_branch;
		int k = 0, gone = 0;
		if (si >= 0)
		{
			gone = b ? sn.find(si, b->head) : sn.cnt[si];
			if (gone < 0) gone = 0;
		}
		for (int j = 0; j < gone && k < 512; j++, k++) // root segments freed after the draw
		{
			BoltSeg f = sn.v[sn.first[si] + j];
			SegFade(f);
			BranchPut(k, f, num, den);
		}
		if (b)
			for (const BoltSeg *x = b->head; x && k < 512; x = x->next, k++) BranchPut(k, *x, num, den);
		if (k >= 2) DrawRibbon<BoltSeg, 0x400>(g_held_segs, k, carry);
	}

	// A task spawned on the tick that ended on it before drawing (path end at 3 segments or fewer, or
	// no free segment: the chain is freed, the node keeps the script read so far and its count): its
	// first script (a spawn of the tick, or a path the creature starts branches on: 0x6C76C0 on the
	// table 0x12AFE58 = count, then the s16 offset of each path) is the one whose growth stops there
	static bool BranchGrowTo(const int16_t *script, const BranchNode *b, BranchGrowth &g)
	{
		g.spawns = 0;
		g.slot_set = false;
		int count = 1;
		for (int iter = 4; iter; iter--)
		{
			if (!script) return false;
			int16_t v = *script++;
			while (v < 0)
			{
				if (g.spawns < 16) g.spawned[g.spawns] = script;
				g.spawns++;
				GrowSlot(g, false);
				script += -1 - v;
				v = *script++;
			}
			const int16_t *peek = script;
			int16_t c = *peek;
			if (c < 0) { peek -= c; c = *peek; }
			if (script == b->script && count == b->count)
			{
				// ended here: at a path end of 3 segments or fewer (no call), else on a failed SegAlloc
				if (!(c == 0x7FFF && count <= 3)) GrowSlot(g, true);
				return true;
			}
			if (c == 0x7FFF)
			{
				if (count <= 3) return false;
				script = nullptr;
			}
			GrowSlot(g, true);
			count++;
		}
		return false;
	}

	static const int16_t *BranchStartEnded(const BranchNode *b, const int16_t *const *spawned, int nsp)
	{
		BranchGrowth g;
		for (int i = 0; i < nsp; i++)
			if (BranchGrowTo(spawned[i] + 1, b, g)) return spawned[i] + 1;
		const int16_t *tb = (const int16_t *)0x12AFE58;
		if (tb[0] > 0 && tb[0] <= 64)
			for (int k = 0; k < tb[0]; k++)
				if (BranchGrowTo(tb + tb[k + 1] + 1, b, g)) return tb + tb[k + 1] + 1;
		return nullptr;
	}

	// every branch the tick drew, in its order, with the growth between the draws (a side-branch
	// spawn leaves 2, 0 in the draw's joint slot, a segment allocation 0x006C78B8): the tasks it started with, then the ones spawned on
	// it in queue order (the creature's, lowest free node first, then the side branches in spawn
	// order), including those that ended before drawing
	static void BranchesHeld(int num, int den)
	{
		int16_t carry[2] = { g_carry0_branch[0], g_carry0_branch[1] };
		TaskNode *head = QueueBolts().first.head;
		static const int16_t *spawned[256];
		int nsp = 0;
		for (int i = 0; i < g_snap_branch.n; i++)
		{
			BranchNode *b = g_snap_branch.node[i];
			const BranchNode &c0 = g_snap_branch.copy[i];
			BranchGrowth g;
			bool draws = BranchGrow(c0.script, c0.count, g);
			for (int k = 0; k < g.spawns && k < 16 && nsp < 256; k++) spawned[nsp++] = g.spawned[k];
			CarryGrowth(carry, g);
			if (!draws) continue;
			bool same = (uint32_t)b->hdr.func == ORIG_BranchTask && SameBranch(b, &c0);
			bool alive = same && Queued(head, &b->hdr);
			// ended on the tick: after drawing, its root retracted to one segment (the path was done:
			// no growth); before drawing when no segment was free (the chain is freed, the node keeps
			// its count, or a spawn of the tick took the node back)
			if (!alive && (same ? b->count >= 2 : c0.script != nullptr)) continue;
			BranchChainHeld(alive ? b : nullptr, i, num, den, carry);
		}
		// the tasks spawned on the tick, their first scripts and spawns
		struct NewTask { BranchNode *b; const int16_t *s0; bool alive, child, queued; };
		static NewTask nt[128];
		int nn = 0;
		for (TaskNode *t = head; t && nn < 128; t = t->next)
		{
			if ((uint32_t)t->func != ORIG_BranchTask) continue;
			BranchNode *b = (BranchNode *)t;
			int i = g_snap_branch.index(t);
			if (i >= 0 && SameBranch(b, &g_snap_branch.copy[i])) continue;
			if (b->count < 2 || !b->head) continue; // not run on the tick
			nt[nn++] = { b, nullptr, true, false, false };
		}
		const TaskQueue &bq = QueueBolts().first;
		if (g_pool0_at && bq.pool == g_pool0_at)
			for (int k = 0; k < 0x80 && nn < 128; k++)
			{
				BranchNode *b = (BranchNode *)((uint8_t *)bq.pool + 0x20 * k);
				if ((b->hdr.flags & 1) || (uint32_t)b->hdr.func != ORIG_BranchTask || !b->script) continue;
				if (!memcmp(b, g_pool0_branch + 0x20 * k, 0x20) || g_snap_branch.index(&b->hdr) >= 0) continue;
				nt[nn++] = { b, nullptr, false, false, false };
			}
		// first scripts: a task's spawns give its side branches theirs (a few passes)
		for (int pass = 0, found = 1; pass < 4 && found; pass++)
		{
			found = 0;
			for (int i = 0; i < nn; i++)
			{
				if (nt[i].s0) continue;
				nt[i].s0 = nt[i].alive ? BranchStart(nt[i].b, spawned, nsp) : BranchStartEnded(nt[i].b, spawned, nsp);
				if (!nt[i].s0) continue;
				found = 1;
				BranchGrowth g;
				if (nt[i].alive) BranchGrow(nt[i].s0, 1, g); else BranchGrowTo(nt[i].s0, nt[i].b, g);
				for (int k = 0; k < g.spawns && k < 16 && nsp < 256; k++) spawned[nsp++] = g.spawned[k];
			}
		}
		for (int i = 0; i < nn; i++)
			for (int k = 0; k < nsp && !nt[i].child; k++) nt[i].child = nt[i].s0 == spawned[k] + 1;
		// queue order: the creature's (node order), then the side branches as their spawns come
		static NewTask *fifo[128];
		int nf = 0;
		for (int i = 0; i < nn; i++) // (nodes are listed in queue order, then pool order: sort by node)
			if (!nt[i].child) { int k = nf++; while (k && fifo[k - 1]->b > nt[i].b) { fifo[k] = fifo[k - 1]; k--; } fifo[k] = &nt[i]; }
		auto queue_children = [&](const BranchGrowth &g)
		{
			for (int k = 0; k < g.spawns && k < 16; k++)
				for (int i = 0; i < nn; i++)
					if (nt[i].child && !nt[i].queued && nt[i].s0 == g.spawned[k] + 1 && nf < 128) { nt[i].queued = true; fifo[nf++] = &nt[i]; break; }
		};
		for (int i = 0; i < g_snap_branch.n; i++)
		{
			BranchGrowth g;
			BranchGrow(g_snap_branch.copy[i].script, g_snap_branch.copy[i].count, g);
			queue_children(g);
		}
		for (int f = 0; f < nf; f++)
		{
			NewTask &n = *fifo[f];
			if (n.s0)
			{
				BranchGrowth g;
				if (n.alive) BranchGrow(n.s0, 1, g); else BranchGrowTo(n.s0, n.b, g);
				queue_children(g);
				CarryGrowth(carry, g);
			}
			if (n.alive) BranchChainHeld(n.b, -1, num, den, carry);
		}
		for (int i = 0; i < nn; i++) // (a side branch whose spawn was not found)
			if (nt[i].child && !nt[i].queued && nt[i].alive) BranchChainHeld(nt[i].b, -1, num, den, carry);
	}

	// impact flash: a flipbook, redrawn on its current frame with the held-frame camera
	static void FlashDrawFrame(int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(void **)h = SEQ_ImpactFlash;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void FlashHeld(FlashNode *f, int, int)
	{
		if (f->age <= 0) return;
		FlashDrawFrame(f->pos, (int16_t)(f->age - 1));
	}

	// arc drawn half way: the curve at the in-between progress; each inner point's jitter half
	// way from the real tick's to the next tick's (nx, each with its own tick's amplitude), or the
	// real tick's replayed when the next one is not known (the module generator is not touched)
	static void ArcDrawMemo(ArcNode *arc, const ArcMemo *m, const ArcNext *nx, int num, int den, int16_t *carry)
	{
		const int16_t *profile = (const int16_t *)(ModelBuffer() + 0x3C25C);
		int32_t len0 = 0, len1 = 0;
		if (nx)
		{
			ArcCurve(arc, m->progress, &len0);
			FieldFree(0x100);
			ArcCurve(arc, arc->progress, &len1);
			FieldFree(0x100);
		}
		int32_t length;
		ArcSeg *pts = ArcCurve(arc, (int16_t)lerp_i(m->progress, arc->progress, num, den), &length);
		for (int k = 0; k < 16; k++) pts[k].color = (int16_t)lerp_i(m->color[k], arc->pts[k].color, num, den);
		for (int i = 1; i < 15; i++)
		{
			const int32_t *rv = m->rnd[i - 1];
			int16_t d[3];
			if (nx)
			{
				int32_t a0 = mul32(profile[i], len0) >> 12, a1 = mul32(profile[i], len1) >> 12;
				for (int c = 0; c < 3; c++)
					d[c] = (int16_t)lerp_i((int16_t)(mul32(rv[c], a0) >> 18), (int16_t)(mul32(nx->rnd[i - 1][c], a1) >> 18), num, den);
			}
			else
			{
				int32_t amp = mul32(profile[i], length) >> 12;
				for (int c = 0; c < 3; c++) d[c] = (int16_t)(mul32(rv[c], amp) >> 18);
			}
			pts[i].sx += d[0];
			pts[i].sy += d[1];
			pts[i].otz += d[2];
		}
		ArcDraw(pts, carry);
		FieldFree(0x100);
	}

	static void ArcHeld(ArcNode *arc, int num, int den, int16_t *carry)
	{
		const ArcMemo *m = g_arc_memo.get(arc);
		if (!m) return;
		ArcDrawMemo(arc, m, g_arc_next_ok ? g_arc_next.get(arc) : nullptr, num, den, carry);
	}

	static uint8_t g_held_packets[0x60000];

	// ---- held-frame memo of what the real tick drew ----
	struct CreaturePlay { prim::Layout *l; uint32_t cb; bool compose; uint8_t arg[0x2C]; };
	struct CreatureMemo
	{
		uint32_t tick;
		bool model;  int32_t v3, v4, acc; // materialisation draw (0x6C6770)
		bool orbit;                        // plain model draw (0x6A79B0)
		int16_t c;                         // the tick's counter
		int nplays; CreaturePlay plays[4];
	};
	static CreatureMemo g_cm;

	static uint8_t *CreatureSkeleton(uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(ModelBuffer() + 0x88 + 4); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0 || sk[0] > 64) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return sk;
	}

	// creature: the tick's counter (the node is at the start of the model buffer)
	static void held_creature_begin()
	{
		g_cm.tick = g_real_tick;
		g_cm.model = g_cm.orbit = false;
		g_cm.nplays = 0;
		g_cm.c = *(int16_t *)(ModelBuffer() + 0x0C);
	}

	// materialisation draw (0x6C6770) of this tick: its opacity step and accumulator
	static void held_note_materialise(int32_t v3, int32_t v4, int32_t acc)
	{
		g_cm.model = true; g_cm.v3 = v3; g_cm.v4 = v4; g_cm.acc = acc;
	}

	static void held_note_orbit() { g_cm.orbit = true; }

	// the matrix of the next play before it is composed with the camera: the held frame
	// composes it with the held-frame camera
	static uint8_t g_uncomposed[0x2C];
	static bool g_uncomposed_next = false;

	static void held_note_uncomposed(const uint8_t *mat)
	{
		memcpy(g_uncomposed, mat, sizeof(g_uncomposed));
		g_uncomposed_next = true;
	}

	// every play of the tick (up to 4): layout, callback and its argument (0x14 bytes for
	// CB_PrimDraw, a 0x2C-byte matrix for CB_PrimMatrix; the uncomposed one when noted)
	static void held_note_play(prim::Layout *l, uint32_t cb, void *arg)
	{
		bool compose = g_uncomposed_next;
		g_uncomposed_next = false;
		uint32_t arg_size = cb == CB_PrimMatrix ? 0x2C : 0x14;
		if (g_cm.nplays < 4)
		{
			CreaturePlay &p = g_cm.plays[g_cm.nplays++];
			p.l = l;
			p.cb = cb;
			p.compose = compose;
			memcpy(p.arg, arg, arg_size);
		}
		if (compose && g_cm.nplays) memcpy(g_cm.plays[g_cm.nplays - 1].arg, g_uncomposed, sizeof(g_uncomposed));
	}

	// ---- held frame of the creature: what the real tick drew, in between ----
	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose after the tick (the one the next tick draws), from the pose drawn
		// this tick: write the midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *p = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*p = (int16_t)(f + ((int32_t)*p - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *p = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			// the engine's bone rotation Ry(-y) Rx(-x) Rz(z) is the same with (2048 - x, y + 2048,
			// z + 2048): the anim data switches spelling near x = +-1024 (a bone would spin half round
			// on the held frame); blend toward whichever spelling of the next pose is nearer (as the
			// gate's pose look-ahead)
			int32_t d[3], e[3], sd = 0, se = 0;
			for (int a = 0; a < 3; a++) { d[a] = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048; sd += abs(d[a]); }
			e[0] = ((2048 - (int32_t)p[0] - f[0] + 2048) & 4095) - 2048;
			e[1] = (((int32_t)p[1] + 2048 - f[1] + 2048) & 4095) - 2048;
			e[2] = (((int32_t)p[2] + 2048 - f[2] + 2048) & 4095) - 2048;
			for (int a = 0; a < 3; a++) se += abs(e[a]);
			const int32_t *dd = (se < sd && num < den) ? e : d;
			for (int a = 0; a < 3; a++) p[a] = (int16_t)(f[a] + dd[a] * num / den);
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	// the creature node, its model header (+0x88) and the model work buffer (+0x8D0: bone
	// matrices from BuildBoneMatrices, DrawModel/DrawMaterialise scratch) up to the bolt
	// vertex pool: the held redraw writes them, and the next real tick draws with the
	// matrices the previous tick built, so they must be exactly what the real tick left
	static const uint32_t CREATURE_AREA = 0x3C9FC;
	static uint8_t g_area_save[CREATURE_AREA];

	// ---- the next tick's creature pose and head vertex, computed once per real tick ----
	// A real tick draws the pose its own anim read produced (AdvanceAnim runs before the
	// draw): the held frame after tick N draws half way from that pose to the one tick N+1
	// reads. The head vertex (0x32) is where the camera looks at 207..226 and where the prim
	// models of 212..246 are placed: tick N+1 recomputes it from its new pose.
	static uint32_t g_next_key = 0xFFFFFFFF;
	static bool g_next_pose_ok = false, g_next_head_ok = false;
	static uint8_t g_next_pose[16 + 48 * 64];
	static int16_t g_next_head[3];

	// the next tick's anim read as its creature code does it (anim switches, then AdvanceAnim)
	static bool NextAnimRead(uint8_t *model, int16_t c)
	{
		if (Pause()) return false;
		uint32_t v3 = (uint32_t)((int32_t)c - 86), v5 = (uint32_t)((int32_t)c - 257);
		if (!((v3 < 0xAB && v3 != 0) || v5 < 0x14)) return false;
		if (v3 == 0x20) cx::SetAnim(model, 1);
		else if (v3 == 0x98) cx::SetAnim(model, 2);
		cx::AdvanceAnim(model);
		return true;
	}

	// run on the real state (after the tick: node counter = the next tick's) and put back
	static void ComputeNext()
	{
		if (g_next_key == g_real_tick) return;
		g_next_key = g_real_tick;
		g_next_pose_ok = g_next_head_ok = false;
		if (!QueueCreature().second.head) return;
		uint8_t *nd = ModelBuffer();
		uint8_t *model = nd + 0x28;
		int16_t c = *(int16_t *)(nd + 0x0C);
		if ((int32_t)c <= 86) return; // the model is bound at 86
		uint32_t size = 0;
		uint8_t *sk = CreatureSkeleton(&size);
		if (!sk || size > sizeof(g_next_pose)) return;
		static uint8_t sk_save[16 + 48 * 64];
		memcpy(g_area_save, nd, CREATURE_AREA);
		memcpy(sk_save, sk, size);
		g_next_pose_ok = NextAnimRead(model, c);
		memcpy(g_next_pose, sk, size);
		int16_t v[8] = {};
		cx::BuildBoneMatrices(nd + 0x88);
		cx::VertexPos(model, 0, 0x32, v);
		memcpy(g_next_head, v, sizeof(g_next_head));
		g_next_head_ok = true;
		memcpy(sk, sk_save, size);
		memcpy(nd, g_area_save, CREATURE_AREA);
	}

	// the skeleton's pose half way to the next tick's (sk holds the current pose)
	static void PoseToNext(uint8_t *sk, uint32_t size, int num, int den)
	{
		static uint8_t from[16 + 48 * 64];
		if (!g_next_pose_ok || size > sizeof(from)) return;
		memcpy(from, sk, size);
		memcpy(sk, g_next_pose, size);
		PoseBlend(sk, from, num, den);
	}

	static void CreatureHeld(int num, int den)
	{
		if (g_cm.tick != g_real_tick || !QueueCreature().second.head) return;
		uint8_t *nd = ModelBuffer();
		uint8_t *model = nd + 0x28;
		Mat4x3 effect_camera = EffectCamera();
		uint8_t *area_save = g_area_save;
		memcpy(area_save, nd, CREATURE_AREA);
		EffectCameraMatrix(&Camera(), &EffectCamera());
		uint32_t size = 0;
		uint8_t *sk = (g_cm.model || g_cm.orbit) ? CreatureSkeleton(&size) : nullptr;
		static uint8_t sk_save[16 + 48 * 64];
		if (sk)
		{
			memcpy(sk_save, sk, size);
			// the pose drawn this tick is the one its anim read produced: half way to the next
			PoseToNext(sk, size, num, den);
			cx::BuildBoneMatrices(nd + 0x88);
			if (g_cm.model)
			{
				// next tick draws with the accumulator + v3 and the next opacity step
				int32_t v3n = g_cm.v3 + 1;
				bool next = !Pause() && v3n < 0xAB;
				int32_t v4n = v3n < 0x32 ? (int32_t)(-3500 * v3n) / 50 : -4096;
				int32_t v4 = next ? lerp_i(g_cm.v4, v4n, num, den) : g_cm.v4;
				int32_t acc = next ? lerp_i(g_cm.acc, g_cm.acc + g_cm.v3, num, den) : g_cm.acc;
				static const int16_t a3[4] = { 0x1C0, 0x180, 0x80, 0x80 };
				static const uint8_t a7[4] = { 0x80, 0x80, 0x80, 0 };
				cx::DrawMaterialise(model, v4, a3, 0x3F54, acc >> 6, acc >> 5, a7);
			}
			if (g_cm.orbit)
				var<uint32_t>(0x1D8E054) = cx::DrawModel(model, nd + 0x8D0, var<uint32_t>(0x1D8E054), var<uint32_t>(0x1D969A8));
			memcpy(sk, sk_save, size);
		}
		for (int i = 0; i < g_cm.nplays; i++)
		{
			CreaturePlay &p = g_cm.plays[i];
			static uint8_t arg[0x2C];
			memcpy(arg, p.arg, sizeof(arg));
			// 212..246: placed at the head vertex, which the next tick recomputes from its pose
			if (p.compose && g_next_head_ok && (uint32_t)((int32_t)g_cm.c - 212) < 0x23)
				for (int k = 0; k < 3; k++)
				{
					int32_t *t = (int32_t *)(arg + 0x14) + k;
					*t = lerp_i(*t, g_next_head[k], num, den);
				}
			if (p.compose) ComposeAffineTransform(&Camera(), (Mat4x3 *)arg, (Mat4x3 *)arg);
			prim::play_held(p.l, (prim::Callback)p.cb, (int)arg, num, den);
		}
		// the tick's one-tick screen flashes (full-screen translucent tiles 0x6C6530 / 0x6C65A0,
		// the table at the end of the creature task): on screen for the whole tick, held too
		switch ((uint16_t)g_cm.c)
		{
		case 0x49: case 0x4F: case 0xF8: case 0xFA: case 0xFD: case 0x102: case 0x13D: cx::Sub6C6530(0x12AE7E0); break;
		case 0x4E: cx::Sub6C65A0(0x12AE7E4); break;
		case 0x50: case 0xE3: case 0x101: case 0x103: case 0x13C: case 0x13E: cx::Sub6C6530(0x12AE7E4); break;
		case 0x51: case 0xE4: case 0x13B: case 0x13F: cx::Sub6C6530(0x12AE7E8); break;
		case 0xE5: cx::Sub6C6530(0x12AE7EC); break;
		default: break;
		}
		// BoltFrame (written by the object-0 play of 257..302) stays for the branches and flashes
		// the held frame draws next, as in the real tick; HeldFrame puts it back
		EffectCamera() = effect_camera;
		memcpy(nd, area_save, CREATURE_AREA);
	}

	// ---- held-frame camera: the creature's camera code for the next tick, run on the real
	// globals and put back (all it calls is pure math), then the in-between camera ----
	static bool CreatureCameraNext(int16_t w[3], int16_t l[3])
	{
		uint8_t *nd = ModelBuffer();
		auto W16 = [nd](int o) -> int16_t & { return *(int16_t *)(nd + o); };
		auto D32 = [nd](int o) -> int32_t & { return *(int32_t *)(nd + o); };
		int16_t c = W16(0x0C);
		bool cut = false;
		static uint8_t cam_save[0x10], node_save[0x10], fr_save[sizeof(g_fr)];
		memcpy(cam_save, (void *)CAM_EYE, 0x10);
		memcpy(node_save, nd + 0x18, 0x10);
		memcpy(fr_save, g_fr, sizeof(g_fr));
		bool paused = Pause() != 0;
		uint32_t v5 = (uint32_t)((int32_t)c - 257);
		if (v5 < 0x14 && !paused)
		{
			F16(0x18) = G16(CAM_EYE);
			F16(0x1A) = G16(CAM_EYE + 2);
			F16(0x1C) = (int16_t)(G32(CAM_EYE + 4) - G32(BASE_Y));
			int32_t s = ComputeSin((int32_t)((v5 << 12) / 40));
			cx::RotMatrixY(s >> 7, FP(0x44));
			cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_EYE);
			G16(CAM_EYE + 4) += G16(BASE_Y);
		}
		uint32_t v7 = (uint32_t)((int32_t)c - 86);
		if (v7 < 0x4C && v7 != 0 && !paused)
		{
			int32_t v10 = (int32_t)((v7 << 12) / 76);
			W16(0x1A) -= 17;
			W16(0x22) -= 5;
			W16(0x24) -= G16(BASE_Y3);
			int32_t s = ComputeSin(v10 >> 1);
			cx::RotMatrixY(s / 90, FP(0x44));
			cx::MatVec(FP(0x44), nd + 0x20, nd + 0x20);
			W16(0x24) += G16(BASE_Y3);
			if (v7 == 31) cut = true;
			else if (v7 > 31 && v7 <= 40)
			{
				F16(0x18) = G16(CAM_EYE);
				F16(0x1A) = G16(CAM_EYE + 2);
				F16(0x1C) = (int16_t)(G32(CAM_EYE + 4) - G32(BASE_Y3));
				cx::RotMatrixY(0x40, FP(0x44));
				cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_EYE);
				G16(CAM_EYE + 4) += (int16_t)G32(BASE_Y3);
				F16(0x18) = G16(CAM_AT);
				F16(0x1A) = G16(CAM_AT + 2);
				F16(0x1C) = (int16_t)(G32(CAM_AT + 4) - G32(BASE_Y3));
				cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_AT);
				G16(CAM_AT + 4) += G16(BASE_Y3);
			}
			else if (v10 < 0x800)
			{
				G32(CAM_AT) = D32(0x18);
				G32(CAM_AT + 4) = D32(0x1C);
				G32(CAM_EYE) = D32(0x20);
				G32(CAM_EYE + 4) = D32(0x24);
			}
			else
			{
				int32_t v12 = 2 * v10 - 0x1000;
				F16(0x2C) = (int16_t)(G32(BASE_Y3) - 0xA14);
				F16(0x20) = (int16_t)0xFD69; F16(0x22) = (int16_t)0xF544; F16(0x24) = (int16_t)(G32(BASE_Y3) + 0x6F7);
				F16(0x28) = 0x547; F16(0x2A) = (int16_t)0xF9F4;
				cx::Blend(nd + 0x18, FP(0x20), 0x1000 - v12, v12, (void *)CAM_AT);
				cx::Blend(nd + 0x20, FP(0x28), 0x1000 - v12, v12, (void *)CAM_EYE);
			}
		}
		if ((uint32_t)((int32_t)c - 207) < 0x14)
		{
			// the look-at follows the head vertex of the posed creature: the next tick's (from its
			// pose, ComputeNext); the eye cut at 207 is a cut
			if (c == 207) cut = true;
			else if (g_next_head_ok) memcpy((void *)CAM_AT, g_next_head, sizeof(g_next_head));
		}
		{
			uint32_t t = (uint32_t)((int32_t)c - 257);
			if (t < 0x2E && t >= 0x14 && !paused)
			{
				int16_t d = t < 0x26 ? -64 : 0x180;
				G16(CAM_EYE + 2) += d;
				G16(CAM_AT + 2) += d;
			}
		}
		{
			uint32_t t = (uint32_t)((int32_t)c - 319);
			if (t < 0x24 && !paused)
			{
				int16_t d = (int16_t)(0x80 - (t << 7) / 36);
				G16(CAM_EYE) += d;
				G16(CAM_EYE + 2) -= d;
				G16(CAM_EYE + 4) -= d;
			}
		}
		switch ((uint16_t)c)
		{
		case 0: case 0x4C: case 0xF6: case 0xE2: case 0x100: case 0x114: case 0x12E: case 0x13E: cut = true; break;
		default: break;
		}
		const int16_t *e = (const int16_t *)CAM_EYE, *a = (const int16_t *)CAM_AT;
		const int16_t *e0 = (const int16_t *)cam_save, *a0 = (const int16_t *)(cam_save + 8);
		for (int i = 0; i < 3; i++)
		{
			// a jump the timeline makes on its own (e.g. back to the saved camera at 127) is a cut too
			if (abs((int32_t)e[i] - e0[i]) > 1500 || abs((int32_t)a[i] - a0[i]) > 1500) cut = true;
			w[i] = e[i];
			l[i] = a[i];
		}
		memcpy((void *)CAM_EYE, cam_save, 0x10);
		memcpy(nd + 0x18, node_save, 0x10);
		memcpy(g_fr, fr_save, sizeof(g_fr));
		return !cut;
	}

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cm.tick != g_real_tick || g_ported_tick != g_real_tick || !QueueCreature().second.head) return false;
		ComputeNext();
		int16_t w[3], l[3];
		const int16_t *e0 = (const int16_t *)CAM_EYE, *a0 = (const int16_t *)CAM_AT;
		bool move = CreatureCameraNext(w, l);
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(e0[i], w[i], num, den) : e0[i];
			lookat[i] = move ? (int16_t)lerp_i(a0[i], l[i], num, den) : a0[i];
		}
		return true;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master's queue order and GTE setup; packets go to a private buffer so the
	// real tick's arenas are untouched
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = PacketCursor(), frame_cursor = var<uint32_t>(0x1D8E054);
		PacketCursor() = (uint32_t)g_held_packets;
		var<uint32_t>(0x1D8E054) = (uint32_t)g_held_packets + 0x40000;
		Mat4x3 bolt_frame = BoltFrame();
		if (g_cm.tick == g_real_tick) ComputeNext();
		ComputeArcNext();
		CreatureHeld(num, den);
		if (QueueCreature().first.head || g_snap_arc.n)
		{
			// arcs read the bones in the composed (world) form the master gives them during the
			// tick: compose, draw, then put the skeleton bytes back exactly
			uint8_t *com = *(uint8_t **)(ModelBuffer() + 0x88 + 4);   // BattleAnimHeader.comFileData
			uint8_t *skel = com ? *(uint8_t **)com : nullptr;           // skeleton section
			static uint8_t skel_save[16 + 48 * 256];
			uint32_t skel_size = skel ? 16 + 48 * (uint32_t)skel[0] : 0;
			if (skel && skel_size <= sizeof(skel_save))
			{
				memcpy(skel_save, skel, skel_size);
				// the arcs follow the bones: the held pose (half way to the next tick's)
				bool posed = g_next_pose_ok && g_cm.tick == g_real_tick;
				if (posed)
				{
					memcpy(g_area_save, ModelBuffer(), CREATURE_AREA);
					PoseToNext(skel, skel_size, num, den);
					cx::BuildBoneMatrices(ModelBuffer() + 0x88);
				}
				ComputeBonesWorldMatrices(ModelBuffer() + 0x88, ModelBuffer() + 0x68);
				// the tick's arcs in its order: those it started with (the ones that ended on it: their
				// last picture), then the ones the creature spawned on it
				int16_t carry[2] = { g_carry0_arc[0], g_carry0_arc[1] };
				for (int i = 0; i < g_snap_arc.n; i++)
				{
					TaskNode *t = g_snap_arc.node[i];
					if (Queued(QueueCreature().first.head, t) && (uint32_t)t->func == ORIG_ArcTask) ArcHeld((ArcNode *)t, num, den, carry);
					else if (const ArcMemo *m = g_arc_memo.get(t)) ArcDrawMemo(&g_snap_arc.copy[i], m, nullptr, 0, 1, carry);
				}
				for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
					if ((uint32_t)t->func == ORIG_ArcTask && SnapIndex(g_snap_arc, t) < 0) ArcHeld((ArcNode *)t, num, den, carry);
				memcpy(skel, skel_save, skel_size);
				if (posed) memcpy(ModelBuffer(), g_area_save, CREATURE_AREA);
			}
		}
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		BoltsHeld(num, den);
		GteSetRotMatrix(&BoltFrame());
		GteSetTransVector(&BoltFrame());
		BranchesHeld(num, den);
		// flashes and debris in the tick's order: those it started with (the ones that ended on it:
		// their last picture), then the ones spawned on it
		TaskNode *fq = QueueDebris().second.head, *dq = QueueDebris().first.head;
		for (int i = 0; i < g_snap_flash.n; i++)
		{
			TaskNode *t = g_snap_flash.node[i];
			if (Queued(fq, t) && (uint32_t)t->func == ORIG_FlashTask) FlashHeld((FlashNode *)t, num, den);
			else FlashDrawFrame(g_snap_flash.copy[i].pos, g_snap_flash.copy[i].age);
		}
		for (TaskNode *t = fq; t; t = t->next)
			if ((uint32_t)t->func == ORIG_FlashTask && SnapIndex(g_snap_flash, t) < 0) FlashHeld((FlashNode *)t, num, den);
		for (int i = 0; i < g_snap_debris.n; i++)
		{
			TaskNode *t = g_snap_debris.node[i];
			if (Queued(dq, t) && (uint32_t)t->func == ORIG_DebrisTask) DebrisHeld((DebrisNode *)t, num, den);
			else if (const DebrisMemo *m = g_debris_memo.get(t))
			{
				int16_t pos[3], rot[3];
				memcpy(pos, m->pos, sizeof(pos));
				memcpy(rot, m->rot, sizeof(rot));
				DebrisDraw(pos, rot);
			}
		}
		for (TaskNode *t = dq; t; t = t->next)
			if ((uint32_t)t->func == ORIG_DebrisTask && SnapIndex(g_snap_debris, t) < 0) DebrisHeld((DebrisNode *)t, num, den);
		BoltFrame() = bolt_frame;
		PacketCursor() = cursor;
		var<uint32_t>(0x1D8E054) = frame_cursor;
	}
}

	static void register_mag116_held()
	{
		mark_held(q116::ORIG_CreatureTask);
		mark_held(q116::ORIG_DebrisTask);
		mark_held(q116::ORIG_BranchTask);
		mark_held(q116::ORIG_BoltTask);
		mark_held(q116::ORIG_FlashTask);
		mark_held(q116::ORIG_ArcTask);
		register_module_held(116, q116::HeldReady, q116::HeldFrame);
		register_module_camera(116, q116::HeldCamera);
	}
}
