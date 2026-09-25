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

// Effect 116: Quezacotl - Thunder Storm (timeline-family GF, source module "aoy", MAG_116_*).
//
// Structure (see gf_study/gf_inventory_timeline.md):
//   SequenceTask (master, 0x6C3760) - flips the packet arena, spawns the creature at tick 2,
//     runs the creature queue, then the lightning (0x2521708) and debris (0x25216E8) queues.
//   CreatureTask (0x6C3940) - 355-tick timeline: Quezacotl model, camera moves, loads, sounds.
//   particle tasks: debris 0x6C6660, bolts 0x6C7EA0, 0x6C7800, 0x6C8850.
// Module globals: 0x25216D8..0x25217D0 (gf_study/gf_global_ranges.md).

#include "fx_port.h"
#include "../../../log.h"

namespace ff8fx
{
namespace q116
{
	using namespace eng;

	// --- module globals ---
	inline uint32_t &Pause() { return var<uint32_t>(0x25216DC); }          // debug pause, never set by the game
	inline TaskQueuePair &QueueDebris() { return var<TaskQueuePair>(0x25216E8); }
	inline TaskQueuePair &QueueBolts() { return var<TaskQueuePair>(0x2521708); }
	inline TaskQueuePair &QueueCreature() { return var<TaskQueuePair>(0x2521728); } // .first = creature parts, .second = creature
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x252176C); }
	inline Mat4x3 &BoltFrame() { return var<Mat4x3>(0x2521780); }
	inline uint32_t &ScreenFlash() { return var<uint32_t>(0x25217A4); }
	inline uint8_t &DoneFlag() { return var<uint8_t>(0x25217A8); }
	inline uint8_t *&ModelBuffer() { return var<uint8_t *>(0x25217AC); }   // = MAGIC_TEXTURE_BUFFER_BASE
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x25217B0); }

	static const uint32_t ORIG_SequenceTask = 0x6C3760;
	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static const uint32_t ORIG_CreatureTask = 0x6C3940;
	static const uint32_t ORIG_DebrisTask = 0x6C6660;
	static const void *SOUND_CreatureVoice = (const void *)0x12AE500;
	static void *const MODEL_Debris = (void *)0x12AC8B8; // prim model (off_12AC8B8)

	// ------------------------------------------------------------------
	// Master task (0x6C3760). Node: +0x0C u16 counter, +0x0F u8 creature spawned,
	// +0x10 u32 packet arena parity.
	// ------------------------------------------------------------------
	struct MasterNode
	{
		TaskNode hdr;
		uint16_t counter; // +0x0C
		uint8_t pad0E;
		uint8_t spawned;  // +0x0F
		uint32_t parity;  // +0x10
	};

	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		MasterNode *node = (MasterNode *)n;
		g_ported_tick = g_real_tick;
		// double-buffered packet arena inside the model buffer
		if (node->parity)
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + 0x2E50);
			node->parity = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + 0x15E50);
			node->parity = 1;
		}
		ScreenFlash() = 0;

		// creature spawn, once, at master tick 2
		if (node->counter == 2 && !Pause() && !node->spawned)
		{
			node->spawned = 1;
			InitTaskQueuePool(&QueueCreature().second, ModelBuffer(), 0x8D0, 1);
			TaskNode *creature = AddTaskToQueue(&QueueCreature().second, ORIG_CreatureTask);
			Memset32((uint8_t *)creature + 0x0C, 0, 0x231);
			*(uint32_t *)((uint8_t *)creature + 0x10) = ClaimVoiceSlot(SOUND_CreatureVoice, 1, 0x80);
			StreamStateInit(ModelBuffer() + 0x28E54);
			*(uint32_t *)(ModelBuffer() + 0x28E50) = 1;
		}

		int creature_left = 1; // the original keeps the node pointer here: non-zero
		if (node->spawned)
		{
			EffectCameraMatrix(&Camera(), &EffectCamera());
			creature_left = ExecuteTaskQueue(&QueueCreature().second);
			if (QueueCreature().first.head)
			{
				ComputeBonesWorldMatrices(ModelBuffer() + 0x88, ModelBuffer() + 0x68);
				ExecuteTaskQueue(&QueueCreature().first);
				BuildBoneMatricesFromPose(ModelBuffer() + 0x88);
			}
			GteSetRotMatrix(&Camera());
			GteSetTransVector(&Camera());
			ExecuteTaskQueue(&QueueBolts().second);
			GteSetRotMatrix(&BoltFrame());
			GteSetTransVector(&BoltFrame());
			ExecuteTaskQueue(&QueueBolts().first);
			ExecuteTaskQueue(&QueueDebris().second);
			ExecuteTaskQueue(&QueueDebris().first);
		}

		SetScreenFlash(ScreenFlash(), 0);
		if (Pause()) return 0;
		if (node->spawned && creature_left == 0)
			return DoneFlag() ? TASK_END : 0; // creature finished: end once the streams are done
		node->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Debris chunk (0x6C6660): a prim model that falls under gravity and bounces on the
	// ground plane (y > 0 is below ground), spinning; 30 ticks. Spawned in pairs by the
	// creature task. Node 0x2C bytes.
	// ------------------------------------------------------------------
	struct DebrisNode
	{
		TaskNode hdr;
		int16_t age;      // +0x0C
		int16_t pad0E;
		int16_t pos[3];   // +0x10 x, y, z
		int16_t pad16;
		int16_t vel[3];   // +0x18
		int16_t pad1E;
		int16_t rot[3];   // +0x20
		int16_t pad26;
		int8_t spin[3];   // +0x28
		int8_t pad2B;
	};
	static_assert(sizeof(DebrisNode) == 0x2C, "debris node is 0x2C bytes");

	static void DebrisDraw(const int16_t pos[3], const int16_t rot[3])
	{
		uint8_t *header = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m;
		BuildRotationMatrixFromAngles(rot, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(void **)header = MODEL_Debris;
		*(uint32_t *)(header + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(header, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void DebrisUpdate(DebrisNode *p)
	{
		p->rot[0] += p->spin[0];
		p->rot[2] += p->spin[2];
		p->vel[1] += 10; // gravity
		p->rot[1] += p->spin[1];
		p->pos[0] += p->vel[0];
		p->pos[1] += p->vel[1];
		p->pos[2] += p->vel[2];
		if (p->pos[1] > 0) // hit the ground: bounce
		{
			p->pos[1] = 0;
			p->vel[1] = -p->vel[1];
		}
	}

	struct DebrisMemo { int16_t pos[3], rot[3]; };
	static NodeMemo<DebrisMemo, 256> g_debris_memo;

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *p = (DebrisNode *)n;
		DebrisDraw(p->pos, p->rot);
		if (Pause()) return 0;
		if (DebrisMemo *m = g_debris_memo.put(n))
		{
			memcpy(m->pos, p->pos, sizeof(m->pos));
			memcpy(m->rot, p->rot, sizeof(m->rot));
		}
		DebrisUpdate(p);
		p->age++;
		return p->age < 30 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Module random generator (0x6C7CF0): 15-bit LCG, seed 0x25217A0
	// ------------------------------------------------------------------
	static int32_t Rand()
	{
		uint32_t &seed = var<uint32_t>(0x25217A0);
		seed = (seed * 125 + 14) & 0x7FFF;
		return (int32_t)seed;
	}

	// ------------------------------------------------------------------
	// Branching lightning. A branch is a chain of segments that grows along a precomputed
	// path script (s16 vertex indices; a negative entry -k starts a side branch whose path
	// is the next k-1 entries, 0x7FFF ends the path). Each segment is a vertex of the bolt
	// mesh (table 0x1298C70, loaded with the summon's data) projected by the GTE; consecutive
	// segments are joined by gouraud quads whose half-width falls off with depth. Brightness
	// starts at 0x100..0x6FE and decays by 1/8 of it per tick; the branch retracts from its
	// root once the root segments are dark.
	// ------------------------------------------------------------------
	struct BoltSeg // 28 bytes, pool of 512 at modelBuffer + 0x40200
	{
		int16_t vertex;    // +0x00 vertex index; -1 = free slot
		int16_t width;     // +0x02 half-width at depth 0 (x32 / (otz + 0x400))
		int16_t sx, sy;    // +0x04 projected position, then one edge of the ribbon
		int16_t otz;       // +0x08 GTE OTZ (SZ3 / 4); -1 = hidden
		int16_t pad0A;
		int16_t sx2, sy2;  // +0x0C other edge
		int16_t pad10[2];
		int16_t intensity; // +0x14 brightness (x16)
		int16_t decay;     // +0x16 added each tick while bright
		BoltSeg *next;     // +0x18
	};
	static_assert(sizeof(BoltSeg) == 28, "bolt segment is 28 bytes");

	struct BranchNode // task node, 0x20 bytes (pool of queue 0x2521708)
	{
		TaskNode hdr;
		int16_t count;          // +0x0C segments in the chain
		int16_t width;          // +0x0E
		int16_t intensity;      // +0x10 brightness of new segments
		int16_t decay;          // +0x12
		const int16_t *script;  // +0x14 path, null once finished
		BoltSeg *head, *tail;   // +0x18, +0x1C
	};
	static_assert(sizeof(BranchNode) == 0x20, "branch node is 0x20 bytes");

	static const uint32_t ORIG_BranchTask = 0x6C7800;
	static const uint8_t *const BOLT_VERTICES = (const uint8_t *)0x1298C70; // 8 bytes each
	static const uint8_t *const DIST_TABLE = (const uint8_t *)0x12B05F0;    // [|dy|][|dx|] -> length, 128 x 128

	inline BoltSeg *&SegFreeHint() { return *(BoltSeg **)(ModelBuffer() + 0x43A00); }
	inline BoltSeg *SegPool() { return (BoltSeg *)(ModelBuffer() + 0x40200); }

	// 0x6C7790
	static BoltSeg *SegAlloc(int16_t vertex, int16_t width, int16_t intensity, int16_t decay)
	{
		BoltSeg *s = SegFreeHint();
		if (s) SegFreeHint() = nullptr;
		else
		{
			s = SegPool();
			int i = 0;
			while (i < 512 && (uint16_t)s->vertex != 0xFFFF) { s++; i++; }
			if (i == 512) return nullptr;
		}
		s->next = nullptr;
		s->vertex = vertex;
		s->width = width;
		s->intensity = intensity;
		s->decay = decay;
		return s;
	}

	// 0x6C76E0: new branch starting at script[0], as its own task
	static void SpawnBranch(const int16_t *script, int16_t width)
	{
		int32_t r1 = Rand() & 0x3FF;
		int32_t r2 = Rand() & 0x1FF;
		int16_t intensity = (int16_t)(r1 + r2 + 0x100);
		int16_t decay = (int16_t)-(intensity >> 3);
		BoltSeg *s = SegAlloc(script[0], width, intensity, decay);
		if (!s) return;
		BranchNode *b = (BranchNode *)AddTaskToQueue(&QueueBolts().first, ORIG_BranchTask);
		if (!b) return;
		Memset32(&b->count, 0, 5);
		b->tail = s;
		b->head = s;
		b->intensity = intensity;
		b->decay = decay;
		b->script = script + 1;
		b->width = width;
		b->count = 1;
	}

	// Ribbon edges of segment s towards n (0x6C7B30 for branches, 0x6C8620 for bolts: the
	// same code on two record layouts, BIAS = depth added before dividing the half-width).
	// dir carries the previous segment's direction so joints are mitred (normalised average
	// of both directions).
	template<typename Seg, int BIAS>
	static void SegEdges(Seg *s, const Seg *n, const int16_t *prev_dir, int16_t *dir)
	{
		if (s->otz < 0) return;
		int32_t ox = 0, oy = 0;
		if (n)
		{
			if (n->otz < 0) return;
			int32_t dx = n->sx - s->sx;
			int32_t ndy = s->sy - n->sy;
			int32_t adx = dx < 0 ? -dx : dx, ady = ndy < 0 ? -ndy : ndy;
			if (adx >= 0x80) adx = 0x7F;
			if (ady >= 0x80) ady = 0x7F;
			int32_t len = DIST_TABLE[ady * 128 + adx];
			int32_t px, py; // unit normal (4.12)
			if (prev_dir)
			{
				if (len == 0)
				{
					dir[0] = prev_dir[0];
					dir[1] = prev_dir[1];
					px = prev_dir[0];
					py = prev_dir[1];
				}
				else
				{
					int32_t cx = shl32(ndy, 12) / len, cy = shl32(dx, 12) / len;
					int32_t sx = cx + prev_dir[0], sy = cy + prev_dir[1];
					dir[0] = (int16_t)cx;
					dir[1] = (int16_t)cy;
					if (sx == 0 && sy == 0) { sx = cx * 2; sy = cy * 2; }
					int32_t sq = (int32_t)((uint32_t)mul32(sx, sx) + (uint32_t)mul32(sy, sy)); // wraps, as the original
					int32_t k = 0x4000 - (sq >> 13); // one Newton step of 1/|v|
					px = mul32(k, sx) >> 13;
					py = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0) { s->otz = -1; return; }
				px = shl32(ndy, 12) / len;
				py = shl32(dx, 12) / len;
				dir[0] = (int16_t)px;
				dir[1] = (int16_t)py;
			}
			int32_t w = shl32(n->width, 5) / (s->otz + BIAS);
			ox = mul32(w, px) >> 12;
			oy = mul32(w, py) >> 12;
		}
		int16_t x = s->sx, y = s->sy;
		s->sx = (int16_t)(x - ox);
		s->sx2 = (int16_t)(x + ox);
		s->sy2 = (int16_t)(y + oy);
		s->sy = (int16_t)(y - oy);
	}

	// TEMP verification: run the original edge routine (ORIG) on copies next to the port and
	// log the inputs of the first disagreements (the tick harness only sees the final packets)
	static int g_edge_logs = 0;
	template<typename Seg, int BIAS, uint32_t ORIG>
	static void SegEdgesV(Seg *s, const Seg *n, const int16_t *prev_dir, int16_t *dir)
	{
		if (ORIG == 0 || g_edge_logs >= 24) { SegEdges<Seg, BIAS>(s, n, prev_dir, dir); return; }
		Seg in = *s, so = *s;
		int16_t pin[2] = { prev_dir ? prev_dir[0] : (int16_t)0, prev_dir ? prev_dir[1] : (int16_t)0 };
		int16_t dor[2] = { dir ? dir[0] : (int16_t)0, dir ? dir[1] : (int16_t)0 }, por[2] = { pin[0], pin[1] };
		const int16_t *pa = prev_dir ? (prev_dir == dir ? dor : por) : nullptr;
		fn<void (__cdecl *)(Seg *, const Seg *, const int16_t *, int16_t *)>(ORIG)(&so, n, pa, dir ? dor : nullptr);
		SegEdges<Seg, BIAS>(s, n, prev_dir, dir);
		if (memcmp(&so, s, sizeof(Seg)) != 0 || (dir && (dor[0] != dir[0] || dor[1] != dir[1])))
		{
			g_edge_logs++;
			ffnx_info("30fps q116 edges %08X: in s=(%d,%d otz %d w %d) n=(%d,%d otz %d w %d) prev=%s(%d,%d) | original (%d,%d)(%d,%d) otz %d dir (%d,%d) | port (%d,%d)(%d,%d) otz %d dir (%d,%d)\n",
				ORIG, in.sx, in.sy, in.otz, in.width, n ? n->sx : 0, n ? n->sy : 0, n ? n->otz : 0, n ? n->width : 0, prev_dir ? "" : "none", pin[0], pin[1],
				so.sx, so.sy, so.sx2, so.sy2, so.otz, dor[0], dor[1], s->sx, s->sy, s->sx2, s->sy2, s->otz, dir ? dir[0] : 0, dir ? dir[1] : 0);
		}
	}

	// Edges of the whole chain, then one GP0 0x3E quad (gouraud, textured, semi-transparent)
	// per segment pair, grey level = brightness / 16. Shared tail of 0x6C79A0 and 0x6C84C0.
	// VANILLA UNINITIALISED READ: the draw functions keep the joint direction in a stack local
	// they never initialise. When a chain's first segment is behind the camera the edge routine
	// returns before writing it, and the next joint reads what the stack slot held: the final
	// direction of the previous chain drawn by the same function (consecutive tasks run at the
	// same stack depth). The ports carry that value explicitly, one slot per draw function;
	// held frames use their own slot so they never change what the real ticks see.
	static int16_t g_dir_branch[2], g_dir_bolt[2], g_dir_arc[2];

	template<typename Seg, int BIAS, uint32_t ORIG = 0>
	static void DrawRibbon(Seg *head, int count, int16_t *carry)
	{
		int16_t dir[2] = { carry[0], carry[1] };
		SegEdgesV<Seg, BIAS, ORIG>(head, head->next, nullptr, dir);
		Seg *s = head->next;
		for (int i = count - 2; i > 0; i--)
		{
			SegEdgesV<Seg, BIAS, ORIG>(s, s->next, dir, dir);
			s = s->next;
		}
		SegEdgesV<Seg, BIAS, ORIG>(s, nullptr, dir, nullptr);

		Seg *a = head;
		for (int i = count - 1; i > 0; i--)
		{
			Seg *b = a->next;
			if (a->otz > 0 && b->otz > 0)
			{
				uint32_t *p = (uint32_t *)PacketCursor();
				PacketCursor() += 0x34;
				p[2] = *(uint32_t *)&a->sx;
				p[5] = *(uint32_t *)&a->sx2;
				p[3] = 0x3E94C008;  // uv0 + CLUT
				p[6] = 0x00B6C038;  // uv1 + texture page
				*(uint16_t *)((uint8_t *)p + 0x24) = 0xC008; // uv2
				*(uint16_t *)((uint8_t *)p + 0x30) = 0xC038; // uv3
				p[8] = *(uint32_t *)&b->sx;
				p[11] = *(uint32_t *)&b->sx2;
				uint32_t ca = (uint32_t)(int32_t)(int16_t)(a->intensity >> 4);
				ca = (((ca << 8) | ca) << 8) | ca;
				p[4] = ca;
				p[1] = ca;
				uint32_t cb = (uint32_t)(int32_t)(int16_t)(b->intensity >> 4);
				cb = (((cb << 8) | cb) << 8) | cb;
				p[0] = 0x0C000000;
				((uint8_t *)p)[7] = 0x3E;
				p[10] = cb;
				p[7] = cb;
				InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 0x44 + 4 * ((a->otz + b->otz) >> 3), p);
			}
			a = b;
		}
		carry[0] = dir[0];
		carry[1] = dir[1];
	}

	// 0x6C79A0: project the chain, then the ribbon
	static void BranchDraw(BoltSeg *head, int count)
	{
		for (BoltSeg *s = head; s; s = s->next)
		{
			GteLoadV0(BOLT_VERTICES + 8 * (int32_t)s->vertex);
			GteRTPS();
			GteReadSXY2(&s->sx);
			GteReadOTZ(&s->otz);
		}
		DrawRibbon<BoltSeg, 0x400, 0x6C7B30>(head, count, g_dir_branch);
	}

	static void BranchFreeAll(BranchNode *b)
	{
		SegFreeHint() = b->head;
		for (BoltSeg *s = b->head; s; s = s->next) s->vertex = -1;
	}

	// 0x6C7800: grow the branch by up to 4 path entries per tick, fade, draw, retract
	static uint32_t __cdecl BranchTask(TaskNode *n)
	{
		BranchNode *b = (BranchNode *)n;
		if (!Pause())
		{
			for (int iter = 4; iter; iter--)
			{
				if (!b->script) continue;
				int16_t v = *b->script++;
				while (v < 0) // side branch: its path is the next -v-1 entries
				{
					SpawnBranch(b->script, b->width);
					// SpawnBranch (0x6C76E0) is called at the same stack depth as BranchDraw and its
					// prologue saves EBX (= 2 here) exactly where BranchDraw's direction local lives
					g_dir_branch[0] = 2;
					g_dir_branch[1] = 0;
					b->script += -1 - v;
					v = *b->script++;
				}
				const int16_t *peek = b->script;
				int16_t c = *peek;
				if (c < 0) { peek -= c; c = *peek; }
				if (c == 0x7FFF) // end of the path
				{
					if (b->count <= 3) { BranchFreeAll(b); return TASK_END; }
					b->script = nullptr;
				}
				int32_t w = b->width;
				int16_t intensity = b->intensity, decay = b->decay;
				BoltSeg *s = SegAlloc(v, (int16_t)(w + ((Rand() * w) >> 15)), intensity, decay);
				if (!s) { BranchFreeAll(b); return TASK_END; }
				b->tail->next = s;
				b->count++;
				b->tail = s;
			}
			for (BoltSeg *s = b->head; s; s = s->next)
				if (s->intensity > 0)
				{
					s->intensity += s->decay;
					if (s->intensity <= 0) s->intensity = 0;
				}
		}
		if (b->count >= 2) BranchDraw(b->head, b->count);
		if (Pause()) return 0;
		BoltSeg *s = b->head;
		if (s->intensity > 0x20) return 0;
		for (;;)
		{
			if (s->next->intensity > 0x20) return 0;
			SegFreeHint() = s;
			s->vertex = -1;
			s = s->next;
			b->count--;
			b->head = s;
			if (b->count < 2) return TASK_END;
			if (s->intensity > 0x20) return 0;
		}
	}

	// ------------------------------------------------------------------
	// Main bolts (0x6C7EA0). A bolt is a "stepping leader": a point that walks from its start
	// to a target over t = 0..0x1000 (dt per step, 8 steps per tick) with a jittered velocity;
	// every step drops a vertex. Before the target is reached the vertex is placed on the
	// straight start-target line (GTE interpolation), afterwards on the target itself. Each
	// vertex drifts by shape * sin(t) and flares up then fades (hold ticks, then an
	// accelerating decay). Kind 1 bolts spawn 3 child bolts and an impact flash on arrival;
	// all kinds randomly fork side bolts while t < 0xC00 (budget = number of forks left).
	// ------------------------------------------------------------------
	struct BoltVert // 0x28 bytes, pool of 256 at modelBuffer + 0x3C9FC
	{
		int16_t pos[3];    // +0x00 world position (GTE V0)
		int16_t width;     // +0x06 half-width at depth 0; -1 = free slot
		int16_t vel[3];    // +0x08 drift per tick
		int16_t hold;      // +0x0E ticks at full brightness before the decay accelerates
		int16_t sx, sy;    // +0x10 projected position, then one edge of the ribbon
		int16_t otz;       // +0x14
		int16_t pad16;
		int16_t sx2, sy2;  // +0x18 other edge
		int16_t pad1C[2];
		int16_t intensity; // +0x20
		int16_t decay;     // +0x22
		BoltVert *next;    // +0x24
	};
	static_assert(sizeof(BoltVert) == 0x28, "bolt vertex is 0x28 bytes");

	struct BoltNode // task node, 0x3C bytes (pool of queue 0x2521718)
	{
		TaskNode hdr;
		int16_t unk0C;          // +0x0C always 0
		int16_t count;          // +0x0E vertices in the chain
		BoltVert *head, *tail;  // +0x10, +0x14
		int16_t pos[3];         // +0x18 leader position
		int16_t t;              // +0x1E progress 0..0x1000
		int16_t target[3];      // +0x20
		int16_t dt;             // +0x26
		int16_t vel[3];         // +0x28 leader velocity (jittered every step)
		int16_t budget;         // +0x2E forks left
		int16_t shape[3];       // +0x30 vertex drift amplitude
		int16_t unk36;          // +0x36
		int16_t width;          // +0x38
		int16_t kind;           // +0x3A 1 = main bolt (children + flash), 2 = main bolt's fork, 0 = other
	};
	static_assert(sizeof(BoltNode) == 0x3C, "bolt node is 0x3C bytes");

	struct FlashNode // task node, 0x14 bytes (pool of queue 0x25216F8)
	{
		TaskNode hdr;
		int16_t pos[3]; // +0x0C
		int16_t age;    // +0x12
	};
	static_assert(sizeof(FlashNode) == 0x14, "flash node is 0x14 bytes");

	static const uint32_t ORIG_BoltTask = 0x6C7EA0;
	static const uint32_t ORIG_FlashTask = 0x6C8430;
	static const int16_t *const SIN_TABLE = (const int16_t *)0x12B4770; // 4096 entries, 4.12
	static void *const SEQ_ImpactFlash = (void *)0x12AE400;

	inline BoltVert *&VertFreeHint() { return *(BoltVert **)(ModelBuffer() + 0x3F1FC); }
	inline BoltVert *VertPool() { return (BoltVert *)(ModelBuffer() + 0x3C9FC); }

	// 0x6C7E30: new vertex at pos (8 bytes are copied, then width overwrites the 4th word)
	static BoltVert *VertAlloc(const int16_t *pos, int16_t width)
	{
		BoltVert *v = VertFreeHint();
		if (v) VertFreeHint() = nullptr;
		else
		{
			v = VertPool();
			int i = 0;
			while (i < 256 && v->width != -1) { v++; i++; }
			if (i == 256) return nullptr;
		}
		memcpy(v->pos, pos, 8);
		v->width = width;
		v->hold = 2;
		v->intensity = 0x200;
		v->decay = 0x300;
		v->next = nullptr;
		return v;
	}

	// 0x6C7D10: new bolt task; pos/target/vel are 8-byte records whose 4th word is replaced
	static BoltVert *SpawnBolt(const int16_t *vel, const int16_t *pos, const int16_t *target, const int16_t *shape,
		int16_t unk36, int16_t t, int16_t dt, int16_t width, int16_t kind, int16_t budget)
	{
		BoltVert *v = VertAlloc(pos, width);
		if (!v) return nullptr;
		BoltNode *b = (BoltNode *)AddTaskToQueue(&QueueBolts().second, ORIG_BoltTask);
		if (!b) { v->width = -1; return nullptr; }
		b->unk0C = 0;
		memcpy(b->pos, pos, 8);
		memcpy(b->target, target, 8);
		memcpy(b->vel, vel, 8);
		b->t = t;
		b->dt = dt;
		b->budget = budget;
		b->tail = v;
		b->head = v;
		b->count = 1;
		if (!shape)
		{
			v->vel[2] = 0;
			v->vel[1] = 0;
			v->vel[0] = 0;
			b->shape[0] = (int16_t)((Rand() & 0xFF) - 0x80);
			b->shape[1] = (int16_t)((Rand() & 0xFF) - 0x80);
			b->shape[2] = (int16_t)((Rand() & 0xFF) - 0x80);
		}
		else memcpy(b->shape, shape, 8);
		b->unk36 = unk36;
		b->width = width;
		b->kind = kind;
		return v;
	}

	// random offset of +-(0x200..0x3FF) around base, sign drawn first (0x6C81B1..)
	static int16_t ForkOffset(int16_t base)
	{
		int32_t o = Rand() > 0x4000 ? -0x200 - (Rand() & 0x1FF) : (Rand() & 0x1FF) + 0x200;
		return (int16_t)(o + base);
	}

	static uint32_t __cdecl BoltTask(TaskNode *n)
	{
		BoltNode *b = (BoltNode *)n;
		if (!Pause())
		{
			for (int iter = 8; iter; iter--)
			{
				if (b->t >= 0x1000) continue;
				b->pos[0] += b->vel[0];
				b->t = (int16_t)(b->t + b->dt);
				b->pos[1] += b->vel[1];
				b->pos[2] += b->vel[2];
				b->vel[0] += (int16_t)((Rand() & 0x7F) - 0x40);
				b->vel[1] += (int16_t)((Rand() & 0x7F) - 0x40);
				b->vel[2] += (int16_t)((Rand() & 0x7F) - 0x40);
				if (Rand() < 0x1000)
				{
					b->pos[0] += (int16_t)((Rand() & 0x1FF) - 0x100);
					b->pos[1] += (int16_t)((Rand() & 0x1FF) - 0x100);
					b->pos[2] += (int16_t)((Rand() & 0x1FF) - 0x100);
				}
				int16_t at[4]; // where this step's vertex goes (4th word unused, overwritten)
				if (b->t >= 0x1000)
				{
					memcpy(at, b->target, 8);
					b->t = 0x1000;
					if (b->kind != 0 && *(uint32_t *)(ModelBuffer() + 0x28E50) != 0)
					{
						FlashNode *f = (FlashNode *)AddTaskToQueue(&QueueDebris().second, ORIG_FlashTask);
						if (f)
						{
							memcpy(f->pos, at, 8);
							f->age = 0;
						}
						if (b->kind == 1)
							for (int k = 3; k; k--)
							{
								int16_t p[4], v[4];
								p[0] = (int16_t)((Rand() & 0x7FF) + at[0] - 0x400);
								p[1] = (int16_t)(at[1] - (Rand() & 0x3FF) - 0x800);
								p[2] = (int16_t)((Rand() & 0x7FF) + at[2] - 0x400);
								v[0] = (int16_t)((Rand() & 0x7F) - 0x40);
								v[1] = (int16_t)(-0x40 - (Rand() & 0x3F));
								v[2] = (int16_t)((Rand() & 0x7F) - 0x40);
								SpawnBolt(v, at, p, nullptr, 0, 0, 0x100, 0xFA, 0, 0);
							}
					}
				}
				else
				{
					// on the start-target line: pos * (1 - t) + target * t
					GteSetIR0(0x1000 - b->t);
					GteLoadIR123(b->pos);
					GteGPF();
					GteSetIR0(b->t);
					GteLoadIR123(b->target);
					GteGPL();
					GteStoreIR123(at);
				}
				BoltVert *v = VertAlloc(at, b->width);
				if (!v) { b->t = 0x1000; continue; }
				b->tail->next = v;
				b->tail = v;
				GteSetIR0(SIN_TABLE[(b->t >> 1) & 0xFFF]);
				GteLoadIR123(b->shape);
				GteGPF();
				b->count++;
				GteStoreIR123(v->vel);
				if (b->t >= 0xC00) continue;
				if (Rand() >= shl32(b->budget, 13)) continue;
				// fork a side bolt from here
				int16_t p[4], nv[4];
				if (b->kind != 0)
				{
					p[0] = (int16_t)((Rand() & 0xFFF) + b->target[0] - 0x800);
					p[1] = (int16_t)(b->target[1] - (Rand() & 0x7FF) - 0x100);
					p[2] = (int16_t)(b->target[2] - (Rand() & 0x1FF) - 0x100);
				}
				else
				{
					p[0] = ForkOffset(at[0]);
					p[1] = ForkOffset(at[1]);
					p[2] = ForkOffset(at[2]);
				}
				nv[0] = (int16_t)((Rand() & 0x1FF) + b->vel[0] - 0x100);
				nv[1] = (int16_t)((Rand() & 0x1FF) + b->vel[1] - 0x100);
				nv[2] = (int16_t)((Rand() & 0x1FF) + b->vel[2] - 0x100);
				BoltVert *c = SpawnBolt(nv, at, p, b->shape, -1, b->t, 0x200, (int16_t)(b->width >> 1), b->kind == 1 ? 2 : 0, 0);
				if (c) memcpy(c->vel, v->vel, 8); // the fork starts with its parent vertex's drift (and hold)
				b->budget--;
			}
			for (BoltVert *v = b->head; v; v = v->next)
			{
				if (v->intensity <= 0) continue;
				v->intensity += v->decay;
				if (v->intensity <= 0) { v->intensity = 0; continue; }
				if (v->hold != 0 && v->decay <= 0) { v->hold--; continue; }
				if (v->decay > -0x180) v->decay -= 0x140;
			}
		}
		for (BoltVert *v = b->head; v; v = v->next)
		{
			GteLoadV0(v->pos);
			GteRTPS();
			GteReadSXY2(&v->sx);
			GteReadOTZ(&v->otz);
		}
		if (b->count >= 2) DrawRibbon<BoltVert, 0x200>(b->head, b->count, g_dir_bolt); // 0x6C84C0
		if (Pause()) return 0;
		for (BoltVert *v = b->head; v; v = v->next)
		{
			v->pos[0] += v->vel[0];
			v->pos[1] += v->vel[1];
			v->pos[2] += v->vel[2];
		}
		BoltVert *v = b->head;
		if (v->intensity != 0) return 0;
		for (;;) // retract dark vertices from the root
		{
			BoltVert *nx = v->next;
			if (!nx || nx->intensity != 0) return 0;
			VertFreeHint() = v;
			v->width = -1;
			v = nx;
			b->count--;
			b->head = v;
			if (b->count < 2) return TASK_END;
			if (v->intensity != 0) return 0;
		}
	}

	// 0x6C8430: impact flash at a bolt's target, 8-frame sprite sequence
	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		FlashNode *f = (FlashNode *)n;
		TransformCameraByShadowRotation(f->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(void **)h = SEQ_ImpactFlash;
		*(int16_t *)(h + 4) = f->age;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		if (Pause()) return 0;
		f->age++;
		return f->age < 8 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Electric arcs on Quezacotl's body (0x6C8850). Both ends ride along strips of the
	// creature's skinned triangles (a face index, a strip length and a shared progress
	// 0..0x1000 pick a point interpolated between two consecutive faces, with the surface
	// normal). The arc is a curve of 16 points through the two ends, bulging along both
	// normals by distance >> reach_shift, jittered by an amplitude profile (model buffer
	// +0x3C25C, 16 x s16) scaled by the arc length. When progress is complete the arc fades
	// over 10 ticks.
	// ------------------------------------------------------------------
	struct ArcPoint { int16_t width, color, pad; }; // per point of the curve, 6 bytes

	struct ArcNode // task node, 0x80 bytes (pool of queue 0x2521728, creature parts)
	{
		TaskNode hdr;
		int16_t age;            // +0x0C ticks since complete
		int16_t progress;       // +0x0E 0..0x1000 along the face strips
		int16_t speed;          // +0x10
		uint8_t reach_shift;    // +0x12 bulge = distance >> reach_shift
		uint8_t fade_shift;     // +0x13
		const int16_t *strips;  // +0x14 first face A, strip length A, first face B, strip length B
		const uint8_t *faces;   // +0x18 24 bytes per face: 3 x {s16 x, y, z, bone}
		uint8_t **bones;        // +0x1C *bones + 0x20 = bone matrices, 48 bytes each
		ArcPoint pts[16];       // +0x20
	};
	static_assert(sizeof(ArcNode) == 0x80, "arc node is 0x80 bytes");

	struct ArcSeg // 16 bytes on the scratch stack: world position, then projected ribbon
	{
		int16_t sx, sy;   // +0 world x, y -> projected position -> one edge
		int16_t otz;      // +4 world z -> OTZ
		int16_t width;    // +6
		int16_t sx2, sy2; // +8 other edge
		int16_t padC;
		int16_t color;    // +E grey level
	};
	static_assert(sizeof(ArcSeg) == 16, "arc point is 16 bytes");

	static const uint32_t ORIG_ArcTask = 0x6C8850;
	static const uint16_t *const SQRT_TABLE = (const uint16_t *)0x12B4570;

	// square root through the GTE leading-zero counter and the sqrt table, as the original:
	// returns sqrt(v) in 4.12 (not yet shifted down)
	static int32_t SqrtLz(int32_t v)
	{
		int32_t lz;
		GteSetLZCS(v);
		GteReadLZCR(&lz);
		int32_t e = lz & ~1;
		int32_t sh = (0x1F - e) >> 1;
		int32_t idx = e - 0x18 > 0 ? shl32(v, e - 0x18) : v >> (0x18 - e);
		return shl32(SQRT_TABLE[idx], sh);
	}

	// trunc(i * scale / 15) with the compiler's magic-number division, exactly
	static int32_t Div15(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * (int32_t)0x88888889) >> 32);
		int32_t d = (hi + x) >> 3;
		return d + (int32_t)((uint32_t)d >> 31);
	}

	// 0x6C9100: centroid and unit normal (4.12) of a skinned triangle
	static void TriangleFrame(const uint8_t *bone_mats, const uint8_t *face, int16_t *pos, int16_t *normal)
	{
		int16_t w[3][4];
		for (int k = 0; k < 3; k++)
		{
			const int16_t *v = (const int16_t *)(face + 8 * k);
			const Mat4x3 *m = (const Mat4x3 *)(bone_mats + 0x10 + 48 * (int32_t)v[3]);
			GteSetRotMatrixCtrl(m);
			GteSetTransVectorCtrl(m);
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(w[k]);
		}
		int32_t e1[3] = { w[0][0] - w[1][0], w[0][1] - w[1][1], w[0][2] - w[1][2] };
		int32_t e2[3] = { w[2][0] - w[0][0], w[2][1] - w[0][1], w[2][2] - w[0][2] };
		int32_t n[3];
		GteSetRotDiagonal(e1);
		GteLoadIR123_32(e2);
		GteOP();
		GteReadMAC123(n);
		int32_t m = n[0] < 0 ? -n[0] : n[0];
		int32_t a = n[1] < 0 ? -n[1] : n[1];
		if (a > m) m = a;
		a = n[2] < 0 ? -n[2] : n[2];
		if (a > m) m = a;
		GteSetLZCS(m);
		for (int k = 0; k < 3; k++)
		{
			int32_t c = w[2][k] + w[1][k] + w[0][k];
			pos[k] = (int16_t)(mul32(c, 0x5555) >> 16); // / 3
		}
		int32_t lz;
		GteReadLZCR(&lz);
		if (lz < 0x12) // keep the squares inside the GTE's range
		{
			int sh = 0x12 - lz;
			n[0] >>= sh;
			n[1] >>= sh;
			n[2] >>= sh;
		}
		int32_t len2;
		GteLoadIR123_32b(n);
		GteSQR();
		GteSumMAC123(&len2);
		int32_t len = SqrtLz(len2) >> 12;
		normal[0] = (int16_t)(shl32(n[0], 12) / len);
		normal[1] = (int16_t)(shl32(n[1], 12) / len);
		normal[2] = (int16_t)(shl32(n[2], 12) / len);
	}

	// 0x6C8F70: point and normal at progress along a strip of n faces
	static void StripPoint(const uint8_t *bone_mats, const uint8_t *faces, int32_t n, int32_t progress, int16_t *pos, int16_t *normal)
	{
		int32_t at = mul32(n - 1, progress);
		int32_t frac = at & 0xFFF;
		const uint8_t *face = faces + 24 * (at >> 12);
		if (frac == 0)
		{
			TriangleFrame(bone_mats, face, pos, normal);
			return;
		}
		int16_t p1[4], p2[4], n1[4], n2[4], nl[4];
		TriangleFrame(bone_mats, face, p1, n1);
		TriangleFrame(bone_mats, face + 24, p2, n2);
		int32_t inv = 0x1000 - frac;
		GteSetIR0(inv);
		GteLoadIR123(p1);
		GteGPF();
		GteSetIR0(frac);
		GteLoadIR123(p2);
		GteGPL();
		GteStoreIR123(pos);
		GteSetIR0(inv);
		GteLoadIR123(n1);
		GteGPF();
		GteSetIR0(frac);
		GteLoadIR123(n2);
		GteGPL();
		GteStoreIR123(nl);
		int32_t len2;
		GteLoadIR123(nl); // (zero-extends the components, as the original does)
		GteSQR();
		GteSumMAC123(&len2);
		int32_t len = SqrtLz(len2) >> 12;
		normal[0] = (int16_t)(shl32(nl[0], 12) / len);
		normal[1] = (int16_t)(shl32(nl[1], 12) / len);
		normal[2] = (int16_t)(shl32(nl[2], 12) / len);
	}

	// 0x6C8C50: project the 16 points, ribbon edges, one quad per pair into the frame arena
	static void ArcDraw(ArcSeg *pts, int16_t *carry)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		for (int k = 0; k < 16; k++)
		{
			int32_t sz3;
			GteLoadV0(&pts[k]);
			GteRTPS();
			GteReadDataReg(14, &pts[k]);     // SXY2
			GteReadDataRegTo(&sz3, 19);      // SZ3
			pts[k].otz = (int16_t)(sz3 >> 2);
		}
		int16_t dir[2] = { carry[0], carry[1] };
		SegEdgesV<ArcSeg, 0x400, 0x6C8DE0>(&pts[0], &pts[1], nullptr, dir);
		for (int k = 1; k < 15; k++)
			SegEdgesV<ArcSeg, 0x400, 0x6C8DE0>(&pts[k], &pts[k + 1], dir, dir);
		SegEdgesV<ArcSeg, 0x400, 0x6C8DE0>(&pts[15], nullptr, dir, nullptr);
		carry[0] = dir[0];
		carry[1] = dir[1];
		for (int k = 0; k < 15; k++)
		{
			const ArcSeg *a = &pts[k], *b = &pts[k + 1];
			if (a->otz <= 0 || b->otz <= 0) continue;
			uint32_t *p = (uint32_t *)var<uint32_t>(0x1D8E054); // battle frame packet arena
			var<uint32_t>(0x1D8E054) += 0x34;
			p[2] = *(const uint32_t *)&a->sx;
			p[5] = *(const uint32_t *)&a->sx2;
			p[3] = 0x3E94C008;
			p[6] = 0x00B6C038;
			*(uint16_t *)((uint8_t *)p + 0x24) = 0xC008;
			*(uint16_t *)((uint8_t *)p + 0x30) = 0xC038;
			p[8] = *(const uint32_t *)&b->sx;
			p[11] = *(const uint32_t *)&b->sx2;
			uint32_t ca = (uint32_t)(int32_t)a->color;
			ca = (((ca << 8) | ca) << 8) | ca;
			p[4] = ca;
			p[1] = ca;
			uint32_t cb = (uint32_t)(int32_t)b->color;
			cb = (((cb << 8) | cb) << 8) | cb;
			p[0] = 0x0C000000;
			((uint8_t *)p)[7] = 0x3E;
			p[10] = cb;
			p[7] = cb;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 0x44 + 4 * ((b->otz + a->otz) >> 5), p);
		}
	}

	// jitter of the 14 inner points: the random values of the real tick are kept so a held
	// frame redraws the same arc shape without touching the module generator
	struct ArcMemo { int16_t progress; int16_t color[16]; int32_t rnd[14][3]; };
	static NodeMemo<ArcMemo, 256> g_arc_memo;

	// the arc for a given progress and colours; fresh = draw the jitter from the generator
	// (real tick, same order as the original), else replay rnd (held frame)
	static void ArcBuildDraw(ArcNode *arc, int16_t progress, const int16_t *color, int32_t (*rnd)[3], bool fresh)
	{
		const uint8_t *bone_mats = *arc->bones + 0x10;
		int16_t p1[4], n1[4], p2[4], n2[4];
		StripPoint(bone_mats, arc->faces + 24 * (int32_t)arc->strips[0], arc->strips[1], progress, p1, n1);
		StripPoint(bone_mats, arc->faces + 24 * (int32_t)arc->strips[2], arc->strips[3], progress, p2, n2);
		int32_t d[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
		GteLoadIR123_32b(d);
		GteSQR();
		GteReadMAC123(d);
		int32_t r = SqrtLz(d[2] + d[1] + d[0]);
		int32_t length = r >> 12;
		GteSetIR0(r >> arc->reach_shift);
		GteLoadIR123(n1);
		GteGPF();
		GteStoreIR123(n1);
		GteLoadIR123(n2);
		GteGPF();
		GteStoreIR123(n2);
		for (int k = 0; k < 3; k++)
		{
			n1[k] += p1[k]; // control points
			n2[k] += p2[k];
		}
		ArcSeg *pts = (ArcSeg *)FieldAlloc(0x100);
		memcpy(&pts[0], p1, 8);
		pts[0].width = arc->pts[0].width;
		pts[0].color = color[0];
		memcpy(&pts[15], p2, 8);
		pts[15].width = arc->pts[15].width;
		pts[15].color = color[15];
		for (int i = 1; i < 15; i++)
		{
			int32_t t = Div15(shl32(i, 12)), u = 0x1000 - t;
			int16_t q1[4], q2[4];
			GteSetIR0(u);
			GteLoadIR123(p1);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123(n1);
			GteGPL();
			GteStoreIR123(q1);
			GteSetIR0(t);
			GteLoadIR123(p2);
			GteGPF();
			GteSetIR0(u);
			GteLoadIR123(n2);
			GteGPL();
			GteStoreIR123(q2);
			GteSetIR0(u);
			GteLoadIR123(q1);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123(q2);
			GteGPL();
			GteStoreIR123(&pts[i]);
			pts[i].width = arc->pts[i].width;
			pts[i].color = color[i];
		}
		const int16_t *profile = (const int16_t *)(ModelBuffer() + 0x3C25C);
		for (int i = 1; i < 15; i++)
		{
			int32_t amp = mul32(profile[i], length) >> 12;
			int32_t *rv = rnd[i - 1];
			if (fresh) rv[0] = Rand() - 0x4000;
			pts[i].sx += (int16_t)(mul32(rv[0], amp) >> 18);
			if (fresh) rv[1] = Rand() - 0x4000;
			pts[i].sy += (int16_t)(mul32(rv[1], amp) >> 18);
			if (fresh) rv[2] = Rand() - 0x4000;
			pts[i].otz += (int16_t)(mul32(rv[2], amp) >> 18);
		}
		int16_t held_dir[2] = { g_dir_arc[0], g_dir_arc[1] };
		ArcDraw(pts, fresh ? g_dir_arc : held_dir);
		FieldFree(0x100);
	}

	static uint32_t __cdecl ArcTask(TaskNode *n)
	{
		ArcNode *arc = (ArcNode *)n;
		ArcMemo local, *m = g_arc_memo.put(n);
		if (!m) m = &local;
		m->progress = arc->progress;
		for (int k = 0; k < 16; k++) m->color[k] = arc->pts[k].color;
		ArcBuildDraw(arc, arc->progress, m->color, m->rnd, true);
		if (Pause()) return 0;
		const int16_t *profile = (const int16_t *)(ModelBuffer() + 0x3C25C);
		arc->progress += arc->speed;
		if (arc->progress <= 0x1000) return 0;
		arc->progress = 0x1000;
		for (int k = 0; k < 16; k++)
		{
			int32_t dec = (0x1400 - profile[k]) >> arc->fade_shift;
			arc->pts[k].color -= (int16_t)dec;
			if (arc->pts[k].color < 0) arc->pts[k].color = 0;
		}
		arc->age++;
		return arc->age < 10 ? 0 : TASK_END;
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

	static void BoltHeld(BoltNode *b, int num, int den)
	{
		if (b->count < 2) return;
		int k = 0;
		for (BoltVert *v = b->head; v && k < 256; v = v->next, k++)
		{
			BoltVert &h = g_held_verts[k];
			h = *v;
			h.next = nullptr;
			if (k) g_held_verts[k - 1].next = &h;
			for (int c = 0; c < 3; c++)
				h.pos[c] = (int16_t)(v->pos[c] - v->vel[c] + v->vel[c] * num / den);
			int32_t i0 = v->intensity, i1 = i0;
			if (i0 > 0)
			{
				i1 = (int16_t)(i0 + v->decay);
				if (i1 <= 0) i1 = 0;
			}
			h.intensity = (int16_t)lerp_i(i0, i1, num, den);
			GteLoadV0(h.pos);
			GteRTPS();
			GteReadSXY2(&h.sx);
			GteReadOTZ(&h.otz);
		}
		int count = b->count < k ? b->count : k;
		static int16_t held_dir[2];
		held_dir[0] = g_dir_bolt[0];
		held_dir[1] = g_dir_bolt[1];
		if (count >= 2) DrawRibbon<BoltVert, 0x200>(g_held_verts, count, held_dir);
	}

	// Branch: the path grows in 15 Hz batches (vanilla shapes); held frames draw the current
	// chain with each segment's brightness half way to the next fade step.
	static BoltSeg g_held_segs[512];

	static void BranchHeld(BranchNode *b, int num, int den)
	{
		if (b->count < 2) return;
		int k = 0;
		for (BoltSeg *s = b->head; s && k < 512; s = s->next, k++)
		{
			BoltSeg &h = g_held_segs[k];
			h = *s;
			h.next = nullptr;
			if (k) g_held_segs[k - 1].next = &h;
			int32_t i0 = s->intensity, i1 = i0;
			if (i0 > 0)
			{
				i1 = (int16_t)(i0 + s->decay);
				if (i1 <= 0) i1 = 0;
			}
			h.intensity = (int16_t)lerp_i(i0, i1, num, den);
			GteLoadV0(BOLT_VERTICES + 8 * (int32_t)h.vertex);
			GteRTPS();
			GteReadSXY2(&h.sx);
			GteReadOTZ(&h.otz);
		}
		static int16_t held_dir[2];
		held_dir[0] = g_dir_branch[0];
		held_dir[1] = g_dir_branch[1];
		int count = b->count < k ? b->count : k;
		if (count >= 2) DrawRibbon<BoltSeg, 0x400>(g_held_segs, count, held_dir);
	}

	// impact flash: a flipbook, redrawn on its current frame with the held-frame camera
	static void FlashHeld(FlashNode *f, int, int)
	{
		if (f->age <= 0) return;
		TransformCameraByShadowRotation(f->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(void **)h = SEQ_ImpactFlash;
		*(int16_t *)(h + 4) = (int16_t)(f->age - 1);
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void ArcHeld(ArcNode *arc, int num, int den)
	{
		const ArcMemo *m = g_arc_memo.get(arc);
		if (!m) return;
		int16_t color[16];
		for (int k = 0; k < 16; k++) color[k] = (int16_t)lerp_i(m->color[k], arc->pts[k].color, num, den);
		ArcBuildDraw(arc, (int16_t)lerp_i(m->progress, arc->progress, num, den), color, (int32_t (*)[3])m->rnd, false);
	}

	static uint8_t g_held_packets[0x60000];

	// ------------------------------------------------------------------
	// Creature timeline (0x6C3940): one node at modelBuffer + 0 (0x8D0 bytes), 355 ticks.
	// Node: +0x0C u16 tick, +0x10 voice slot, +0x14 materialisation accumulator,
	//   +0x18..+0x27 saved camera (look-at XZ/Y, eye XZ/Y), +0x28 creature model
	//   (+0x68 model matrix, +0x88 BattleAnimHeader, +0x94 BattleAnimCmd), +0x34/+0x36/+0x38,
	//   +0x44/+0x46/+0x48, +0x7C/+0x80/+0x84, +0x8C arc bone table, +0xC4 model sections,
	//   +0xD4 prim-model player layout.
	// Timeline: tick 1-76 intro prim model, 77-100 ring, 86-256 the creature materialises
	// (anims 0/1/2 at 86/118/238) while the camera sweeps (86-161) and arcs crawl over it;
	// bolts at 86-100 / 157-176 / 232-266, 207-226 camera on the creature's head, 257-276
	// orbit, 277-302 ascent, 303-318 debris, 319-354 final prim model; streams are loaded at
	// fixed ticks (the timeline waits for each load), damage at 345, end at 355.
	// Locals live in a persistent frame (g_fr, same offsets as the original stack frame):
	// a few fields are never written before being copied (the 4th word of the 8-byte bolt
	// records), so like the original they keep what the previous tick left there.
	// ------------------------------------------------------------------
	namespace cx
	{
		inline void Sub508500() { fn<void (__cdecl *)()>(0x508500)(); }
		inline void RotMatrixY(int32_t angle, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(angle, m); }
		inline void BindModel(void *m, void *sections, void *data) { fn<void (__cdecl *)(void *, void *, void *)>(0x6EC060)(m, sections, data); }
		inline void SetAnim(void *m, int id) { fn<void (__cdecl *)(void *, int)>(0x6574D0)(m, id); }
		inline void AdvanceAnim(void *m) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(m); }
		inline void DrawMaterialise(void *m, int32_t a2, const void *a3, int32_t a4, int32_t a5, int32_t a6, const void *a7)
		{ fn<void (__cdecl *)(void *, int32_t, const void *, int32_t, int32_t, int32_t, const void *)>(0x6C6770)(m, a2, a3, a4, a5, a6, a7); }
		inline void MatVec(const void *m, const void *v, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C4F0)(m, v, out); }
		inline uint32_t DrawModel(void *m, void *buf, uint32_t cursor, uint32_t a4) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, uint32_t)>(0x6A79B0)(m, buf, cursor, a4); }
		inline void Decode(uint32_t src, void *layout, int n) { fn<void (__cdecl *)(uint32_t, void *, int)>(0x7016B0)(src, layout, n); }
		inline void Sub6C6500(int a) { fn<void (__cdecl *)(int)>(0x6C6500)(a); }
		inline void Sub6C6530(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x6C6530)(p); }
		inline void Sub6C65A0(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x6C65A0)(p); }
		inline void VertexPos(void *m, int a, int b, void *out) { fn<void (__cdecl *)(void *, int, int, void *)>(0x6C6440)(m, a, b, out); }
		inline void RotMatrixXY(int a, int b, void *out) { fn<void (__cdecl *)(int, int, void *)>(0x6C64C0)(a, b, out); }
		inline void SpawnArcO(uint32_t bones, uint32_t strips, uint32_t faces, int a4, int a5, int a6) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, int, int, int)>(0x6C87B0)(bones, strips, faces, a4, a5, a6); }
		inline void SpawnBoltO(void *vel, void *pos, void *target, int shape, int unk, int t, int dt, int width, int kind, int budget)
		{ fn<void (__cdecl *)(void *, void *, void *, int, int, int, int, int, int, int)>(0x6C7D10)(vel, pos, target, shape, unk, t, dt, width, kind, budget); }
		inline void Sub6C76C0(uint32_t p, int a) { fn<void (__cdecl *)(uint32_t, int)>(0x6C76C0)(p, a); }
		inline void Sub6C7CC0() { fn<void (__cdecl *)()>(0x6C7CC0)(); }
		inline void Sub6C7690() { fn<void (__cdecl *)()>(0x6C7690)(); }
		inline void Blend(void *a, void *b, int wa, int wb, void *out) { fn<void (__cdecl *)(void *, void *, int, int, void *)>(0x56CB90)(a, b, wa, wb, out); }
		inline int LoadBusy() { return fn<int (__cdecl *)()>(0x534270)(); }
		inline void Load(int id, uint32_t dst, int n) { fn<void (__cdecl *)(int, uint32_t, int)>(0x5341D0)(id, dst, n); }
		inline void LoadPump() { fn<void (__cdecl *)()>(0x534210)(); }
		inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void PlaySE(uint32_t se, int a, int b) { fn<void (__cdecl *)(uint32_t, int, int)>(0x501330)(se, a, b); }
		inline void PlayStream(int a, int b, int c) { fn<void (__cdecl *)(int, int, int)>(0x5018C0)(a, b, c); }
		inline void ApplyResult(uint32_t targets, int n) { fn<void (__cdecl *)(uint32_t, int)>(0x506BA0)(targets, n); }
		inline uint8_t *SummonData() { return fn<uint8_t *(__cdecl *)()>(0x571B70)(); }
		inline void Sub508630(void *a, void *b) { fn<void (__cdecl *)(void *, void *)>(0x508630)(a, b); }
		inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }
		inline void Sub6C5300() { fn<void (__cdecl *)()>(0x6C5300)(); }
		inline void Sub6C5350() { fn<void (__cdecl *)()>(0x6C5350)(); }
		inline void BuildBoneMatrices(void *header) { fn<void (__cdecl *)(void *)>(0x508C90)(header); }
	}

	static const uint32_t CB_PrimDraw = 0x6C6060;   // record -> prim model, relative to the camera
	static const uint32_t CB_PrimMatrix = 0x6C53A0; // record -> prim model, relative to a given matrix
	inline int16_t &G16(uint32_t a) { return var<int16_t>(a); }
	inline int32_t &G32(uint32_t a) { return var<int32_t>(a); }
	static const uint32_t CAM_EYE = 0xB8B7F0, CAM_AT = 0xB8B7F8; // XZ (2 x s16) then Y (s32)
	static const uint32_t BASE_Y = 0x2521770, BASE_Y2 = 0x2521774, BASE_Y3 = 0x2521778;

	static uint8_t g_fr[0x90]; // the original's stack frame, esp-relative offsets
	inline int16_t &F16(int o) { return *(int16_t *)(g_fr + o); }
	inline int32_t &F32(int o) { return *(int32_t *)(g_fr + o); }
	inline uint8_t *FP(int o) { return g_fr + o; }

	// ---- held-frame memo of what the real tick drew ----
	struct CreaturePlay { prim::Layout *l; uint32_t cb; bool compose; uint8_t arg[0x2C]; };
	struct CreatureMemo
	{
		uint32_t tick;
		bool model;  int32_t v3, v4, acc; // materialisation draw (0x6C6770)
		bool orbit;                        // plain model draw (0x6A79B0)
		bool pose_ok; uint32_t pose_size;  // skeleton at the start of the tick: the pose drawn this tick
		uint8_t pose[16 + 48 * 64];
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

	static int Play(prim::Layout *l, uint32_t cb, void *arg, uint32_t arg_size, bool compose = false)
	{
		if (g_cm.nplays < 4)
		{
			CreaturePlay &p = g_cm.plays[g_cm.nplays++];
			p.l = l;
			p.cb = cb;
			p.compose = compose;
			memcpy(p.arg, arg, arg_size);
		}
		return prim::play(l, (prim::Callback)cb, (int)arg, Pause());
	}

	static uint32_t __cdecl CreatureTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		auto W16 = [nd](int o) -> int16_t & { return *(int16_t *)(nd + o); };
		auto D32 = [nd](int o) -> int32_t & { return *(int32_t *)(nd + o); };
		uint8_t *model = nd + 0x28;
		prim::Layout *L = (prim::Layout *)(nd + 0xD4);
		uint8_t *MB = ModelBuffer();

		// held memo: the skeleton as the tick starts is the pose this tick draws
		g_cm.tick = g_real_tick;
		g_cm.model = g_cm.orbit = false;
		g_cm.nplays = 0;
		{
			uint32_t size = 0;
			uint8_t *sk = CreatureSkeleton(&size);
			g_cm.pose_ok = sk && size <= sizeof(g_cm.pose);
			if (g_cm.pose_ok) { memcpy(g_cm.pose, sk, size); g_cm.pose_size = size; }
		}

		int16_t c = W16(0x0C);
		// screen flash: full until 351, then fading out
		if (c <= 351)
		{
			if ((int32_t)ScreenFlash() < 2400) ScreenFlash() = 2400;
		}
		else
		{
			int32_t v = ((355 - (int32_t)c) * 2400) >> 2;
			if (v > (int32_t)ScreenFlash()) ScreenFlash() = (uint32_t)v;
		}

		// 86..256: the creature materialises
		uint32_t v3 = (uint32_t)((int32_t)c - 86);
		if (v3 < 0xAB)
		{
			int32_t v4;
			if (v3 == 0)
			{
				cx::Sub508500();
				cx::RotMatrixY(0, nd + 0x68);
				D32(0x80) = 0; W16(0x46) = 0; D32(0x7C) = 0; W16(0x44) = 0;
				D32(0x84) = G32(BASE_Y3);
				W16(0x48) = G16(BASE_Y3);
				W16(0x36) = 0; W16(0x38) = 0; W16(0x34) = 0;
				cx::BindModel(model, nd + 0xC4, MB + 0x3169C);
				cx::SetAnim(model, 0);
				D32(0x14) = 0;
				v4 = (int32_t)(-3500 * (int32_t)v3) / 50;
			}
			else if (v3 == 0x20) { cx::SetAnim(model, 1); v4 = (int32_t)(-3500 * (int32_t)v3) / 50; }
			else if (v3 == 0x98) { cx::SetAnim(model, 2); v4 = -4096; }
			else v4 = v3 < 0x32 ? (int32_t)(-3500 * (int32_t)v3) / 50 : -4096;
			F16(0x18) = 0x1C0; F16(0x1A) = 0x180; F16(0x1C) = 0x80; F16(0x1E) = 0x80;
			g_fr[0x10] = g_fr[0x11] = g_fr[0x12] = 0x80; g_fr[0x13] = 0;
			if (!Pause()) cx::AdvanceAnim(model);
			g_cm.model = true; g_cm.v3 = (int32_t)v3; g_cm.v4 = v4; g_cm.acc = D32(0x14);
			cx::DrawMaterialise(model, v4, FP(0x18), 0x3F54, D32(0x14) >> 6, D32(0x14) >> 5, FP(0x10));
			if (!Pause()) D32(0x14) += (int32_t)v3;
		}

		// 257..276: orbit around the creature
		uint32_t v5 = (uint32_t)((int32_t)W16(0x0C) - 257);
		if (v5 < 0x14)
		{
			if (!Pause())
			{
				F16(0x18) = G16(CAM_EYE);
				F16(0x1A) = G16(CAM_EYE + 2);
				F16(0x1C) = (int16_t)(G32(CAM_EYE + 4) - G32(BASE_Y));
				int32_t s = ComputeSin((int32_t)((v5 << 12) / 40));
				cx::RotMatrixY(s >> 7, FP(0x44));
				cx::MatVec(FP(0x44), FP(0x18), (void *)CAM_EYE);
				G16(0x1D977A2) += 8; // roll
				G16(CAM_EYE + 4) += G16(BASE_Y);
				cx::AdvanceAnim(model);
			}
			g_cm.orbit = true;
			var<uint32_t>(0x1D8E054) = cx::DrawModel(model, MB + 0x8D0, var<uint32_t>(0x1D8E054), var<uint32_t>(0x1D969A8));
		}

		// 1..76: intro prim model
		if ((uint32_t)((int32_t)W16(0x0C) - 1) < 0x4C)
		{
			if (W16(0x0C) == 1)
			{
				cx::Decode(0x1298C68, L, 0x7D4);
				cx::Sub6C6500(0);
			}
			F32(0x40) = (int32_t)(MB + 0x8D0);
			F32(0x38) = 0; F16(0x3C) = 0; F16(0x34) = 0; F16(0x30) = 0; F16(0x32) = -5000;
			Play(L, CB_PrimDraw, FP(0x30), 0x14);
		}

		// 77..100: ring, turned half round, in front of the camera
		if ((uint32_t)((int32_t)W16(0x0C) - 77) < 0x18)
		{
			if (W16(0x0C) == 77)
			{
				cx::Decode(0x12A9664, L, 0x7FC);
				cx::Sub6C6500(1);
			}
			F32(0x8C) = (int32_t)(MB + 0x8D0);
			F32(0x84) = 0; F16(0x88) = 0;
			F32(0x78) = 500; F32(0x7C) = 0; F32(0x80) = G32(BASE_Y3);
			cx::RotMatrixY(-2048, FP(0x64));
			uint8_t local[0x2C];
			memcpy(local, FP(0x64), sizeof(local));
			ComposeAffineTransform(&Camera(), (Mat4x3 *)FP(0x64), (Mat4x3 *)FP(0x64));
			Play(L, CB_PrimMatrix, FP(0x64), 0x2C, true);
			if (g_cm.nplays) memcpy(g_cm.plays[g_cm.nplays - 1].arg, local, sizeof(local));
		}

		// 86..161: camera sweep, arcs, first bolts
		uint32_t v7 = (uint32_t)((int32_t)W16(0x0C) - 86);
		if (v7 < 0x4C)
		{
			if (v7 == 0)
			{
				for (uint32_t i = 0x3C25C; i < 0x3C27C; i += 2)
				{
					int16_t s = (int16_t)ComputeSin((int32_t)(v7 << 11) / 15);
					v7++;
					*(int16_t *)(ModelBuffer() + i) = s;
				}
				InitTaskQueuePool(&QueueCreature().first, ModelBuffer() + 0x3BA5C, 0x80, 0x10);
				InitTaskQueuePool(&QueueBolts().second, ModelBuffer() + 0x3C27C, 0x3C, 0x20);
				InitTaskQueuePool(&QueueBolts().first, ModelBuffer() + 0x3F200, 0x20, 0x80);
				cx::Sub6C7CC0();
				cx::Sub6C7690();
				D32(0x18) = G32(CAM_AT);
				D32(0x1C) = G32(CAM_AT + 4);
				D32(0x20) = G32(CAM_EYE);
				D32(0x24) = G32(CAM_EYE + 4);
			}
			else if (!Pause())
			{
				int32_t v10 = (int32_t)((v7 << 12) / 76);
				W16(0x1A) -= 17;
				W16(0x22) -= 5;
				W16(0x24) -= G16(BASE_Y3);
				int32_t s = ComputeSin(v10 >> 1);
				cx::RotMatrixY(s / 90, FP(0x44));
				cx::MatVec(FP(0x44), nd + 0x20, nd + 0x20);
				W16(0x24) += G16(BASE_Y3);
				if (v7 == 31)
				{
					G16(CAM_AT) = (int16_t)0xEBF9; G16(CAM_AT + 2) = (int16_t)0xF9BA; G16(CAM_EYE) = 0x5FB;
					G16(CAM_AT + 4) = (int16_t)(G32(BASE_Y3) + 0x606);
					G16(CAM_EYE + 2) = (int16_t)0xF665;
					G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y3) - 0x206);
				}
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
					F32(0x10) = 0x1000 - v12;
					F16(0x20) = (int16_t)0xFD69; F16(0x22) = (int16_t)0xF544; F16(0x24) = (int16_t)(G32(BASE_Y3) + 0x6F7);
					F16(0x28) = 0x547; F16(0x2A) = (int16_t)0xF9F4;
					cx::Blend(nd + 0x18, FP(0x20), 0x1000 - v12, v12, (void *)CAM_AT);
					cx::Blend(nd + 0x20, FP(0x28), F32(0x10), v12, (void *)CAM_EYE);
				}
				cx::SpawnArcO((uint32_t)D32(0x8C), 0x12AE7F0 + 8 * (v7 % 10), 0x12AE840, 12, 7, 0x100);
				if (v7 < 15 && (v7 & 1) == 0)
				{
					int32_t r = Rand();
					F16(0x12) = 0;
					F16(0x10) = (int16_t)((int16_t)((r & 0x3FF) + W16(0x44)) - 0x200);
					r = Rand();
					F16(0x14) = (int16_t)((int16_t)((r & 0x3FF) + W16(0x48)) - 0x200);
					r = Rand();
					F16(0x28) = (int16_t)((r & 0x7FF) - 0x400);
					r = Rand();
					F16(0x2A) = (int16_t)(-0x800 - (r & 0x3FF));
					r = Rand();
					F16(0x2C) = (int16_t)((r & 0x7FF) + F32(0x14) - 0x400);
					r = Rand();
					F16(0x20) = (int16_t)((r & 0x7F) - 0x40);
					r = Rand();
					F16(0x22) = (int16_t)(-0x40 - (r & 0x3F));
					r = Rand();
					F16(0x24) = (int16_t)((r & 0x7F) - 0x40);
					cx::SpawnBoltO(FP(0x20), FP(0x10), FP(0x28), 0, 0, 0, 0x100, 0xFA, 0, 3);
				}
			}
		}

		// 101..164: prim model at the base height
		if ((uint32_t)((int32_t)W16(0x0C) - 101) < 0x40)
		{
			if (W16(0x0C) == 101) cx::Decode(0x1298C68, L, 0x158);
			F32(0x40) = (int32_t)(MB + 0x8D0);
			F16(0x34) = G16(BASE_Y3);
			F32(0x38) = 0; F16(0x3C) = 0; F16(0x32) = 0; F16(0x30) = 0;
			Play(L, CB_PrimDraw, FP(0x30), 0x14);
		}

		// 157..176, every other tick: bolt from a random vertex of the creature
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 157);
			if (t < 0x14 && (t & 1) == 0 && !Pause())
			{
				int32_t r = Rand();
				int32_t vtx = *(int16_t *)(0x12AFE40 + 2 * ((uint32_t)(r * 11) >> 15));
				cx::VertexPos(model, 0, vtx, FP(0x18));
				r = Rand();
				F16(0x28) = (int16_t)((r & 0x7FF) + F32(0x18) - 0x400);
				r = Rand();
				F16(0x2A) = (int16_t)(*(int32_t *)FP(0x1A) - (r & 0x3FF) - 0x400);
				r = Rand();
				F16(0x2C) = (int16_t)((r & 0x7FF) + F32(0x1C) - 0x400);
				r = Rand();
				F16(0x20) = (int16_t)((r & 0x7F) - 0x40);
				r = Rand();
				F16(0x22) = (int16_t)(-0x40 - (r & 0x3F));
				r = Rand();
				F16(0x24) = (int16_t)((r & 0x7F) - 0x40);
				cx::SpawnBoltO(FP(0x20), FP(0x18), FP(0x28), 0, 0, 0, 0x100, 0xFA, 0, 3);
			}
		}

		// 207..226: camera on the creature's head, arcs
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 207);
			if (t < 0x14)
			{
				if (t == 0)
				{
					G16(CAM_EYE) = 0x2A5;
					G16(CAM_EYE + 2) = (int16_t)0xF20D;
					G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y3) - 200);
				}
				cx::VertexPos(ModelBuffer() + 0x28, 0, 0x32, (void *)CAM_AT);
				if (!Pause())
					cx::SpawnArcO((uint32_t)D32(0x8C), 0x12AF800 + 8 * (t % 5), 0x12AF828, 12, 7, 0x100);
			}
		}

		// 212..226: prim model at the look-at point, facing the camera
		if ((uint32_t)((int32_t)W16(0x0C) - 212) < 0xF)
		{
			if (W16(0x0C) == 212) cx::Decode(0x12A1164, L, 0x1A0);
			F32(0x7C) = G16(CAM_AT + 2);
			F32(0x78) = G16(CAM_AT);
			F32(0x8C) = (int32_t)(MB + 0x8D0);
			int32_t yaw = -(int32_t)W16(0x36);
			F32(0x84) = 0; F16(0x88) = 0;
			F32(0x80) = G16(CAM_AT + 4);
			cx::RotMatrixXY(-0x80, yaw, FP(0x64));
			uint8_t local[0x2C];
			memcpy(local, FP(0x64), sizeof(local));
			ComposeAffineTransform(&Camera(), (Mat4x3 *)FP(0x64), (Mat4x3 *)FP(0x64));
			Play(L, CB_PrimMatrix, FP(0x64), 0x2C, true);
			if (g_cm.nplays) memcpy(g_cm.plays[g_cm.nplays - 1].arg, local, sizeof(local));
		}

		// 227..246: prim model at the head vertex
		if ((uint32_t)((int32_t)W16(0x0C) - 227) < 0x14)
		{
			if (W16(0x0C) == 227) cx::Decode(0x12A9664, L, 0x1D8);
			F32(0x8C) = (int32_t)(MB + 0x8D0);
			F32(0x84) = 0; F16(0x88) = 0;
			cx::VertexPos(model, 0, 0x32, FP(0x28));
			F32(0x7C) = F16(0x2A);
			F32(0x78) = F16(0x28);
			F32(0x80) = F16(0x2C);
			cx::RotMatrixXY(-0x80, -(int32_t)W16(0x36), FP(0x64));
			uint8_t local[0x2C];
			memcpy(local, FP(0x64), sizeof(local));
			ComposeAffineTransform(&Camera(), (Mat4x3 *)FP(0x64), (Mat4x3 *)FP(0x64));
			Play(L, CB_PrimMatrix, FP(0x64), 0x2C, true);
			if (g_cm.nplays) memcpy(g_cm.plays[g_cm.nplays - 1].arg, local, sizeof(local));
		}

		// 247..256: screen-space prim model (waits for the load at 247)
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 237);
			if (t < 0x14 && t >= 0xA)
			{
				if (t == 0xA)
				{
					if (cx::LoadBusy()) return 0;
					cx::Decode(0x129BA94, L, 0x128);
				}
				F16(0x74) = 0x1000; F16(0x6C) = 0x1000; F16(0x64) = 0x1000;
				F32(0x8C) = (int32_t)(MB + 0x8D0);
				F32(0x84) = 0; F16(0x88) = 0;
				F16(0x72) = 0; F16(0x70) = 0; F16(0x6E) = 0; F16(0x6A) = 0; F16(0x68) = 0; F16(0x66) = 0;
				F32(0x78) = 0; F32(0x7C) = 2000; F32(0x80) = 4000;
				Play(L, CB_PrimMatrix, FP(0x64), 0x2C);
			}
		}

		// 257..302: branches, orbit prim model, then the ascent (second player at 0x129F4E8)
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 257);
			if (t < 0x2E)
			{
				if (t == 0) cx::Sub508500();
				if (!Pause())
				{
					for (int k = 0; k < 4; k++)
					{
						int32_t r = Rand();
						cx::Sub6C76C0(0x12AFE58, (r * (int32_t)*(int16_t *)0x12AFE58) >> 15);
					}
				}
				if (t == 0) cx::Decode(0x1299718, L, 0xB4);
				F32(0x40) = (int32_t)(MB + 0x8D0);
				F16(0x32) = G16(BASE_Y2);
				F16(0x34) = G16(BASE_Y);
				F32(0x38) = -128; F16(0x3C) = 1; F16(0x3E) = (int16_t)t; F16(0x30) = 0;
				Play(L, CB_PrimDraw, FP(0x30), 0x14);
				if (t >= 0x14)
				{
					if (t == 0x14) cx::Decode(0x129CD84, (void *)0x129F4E8, 0x4BC);
					uint32_t p = Pause();
					if (!p)
					{
						int16_t d = t < 0x26 ? -64 : 0x180;
						G16(CAM_EYE + 2) += d;
						G16(CAM_AT + 2) += d;
					}
					F32(0x38) = 0; F16(0x3C) = 0;
					F32(0x8C) = (int32_t)(MB + 0x8D0);
					F32(0x40) = (int32_t)(MB + 0x8D0);
					F16(0x30) = 0;
					F16(0x34) = G16(BASE_Y);
					F16(0x32) = (int16_t)(G32(BASE_Y2) - 3000);
					Play((prim::Layout *)0x129F4E8, CB_PrimDraw, FP(0x30), 0x14);
				}
			}
		}

		// 303..318: debris chunks, 2 per tick
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 303);
			if (t < 0x10)
			{
				if (t == 0)
				{
					cx::Decode(0x12A9664, L, 0x124);
					InitTaskQueuePool(&QueueDebris().first, (void *)0x12A53E0, 0x2C, 0x20);
				}
				for (int k = 0; k < 2; k++)
				{
					uint8_t *d = (uint8_t *)AddTaskToQueue(&QueueDebris().first, ORIG_DebrisTask);
					if (!d) continue;
					*(int16_t *)(d + 0x0C) = 0;
					*(int16_t *)(d + 0x10) = (int16_t)((cx::SharedRand() & 0xFFF) - 0x800);
					int32_t r = cx::SharedRand();
					*(int16_t *)(d + 0x12) = 0;
					*(int16_t *)(d + 0x14) = (int16_t)((r & 0xFFF) + G32(BASE_Y) - 0x800);
					*(int16_t *)(d + 0x18) = (int16_t)((cx::SharedRand() & 0x3F) - 0x20);
					*(int16_t *)(d + 0x1C) = (int16_t)((cx::SharedRand() & 0x3F) - 0x20);
					*(int16_t *)(d + 0x1A) = (int16_t)(-0x80 - (cx::SharedRand() & 0x7F));
					*(int16_t *)(d + 0x20) = (int16_t)cx::SharedRand();
					*(int16_t *)(d + 0x22) = (int16_t)cx::SharedRand();
					*(int16_t *)(d + 0x24) = (int16_t)cx::SharedRand();
					d[0x28] = (uint8_t)cx::SharedRand();
					d[0x29] = (uint8_t)cx::SharedRand();
					d[0x2A] = (uint8_t)cx::SharedRand();
				}
				F16(0x34) = G16(BASE_Y);
				F32(0x40) = (int32_t)(MB + 0x8D0);
				F32(0x8C) = (int32_t)(MB + 0x8D0);
				F32(0x38) = -256; F16(0x3C) = 0; F16(0x32) = 0; F16(0x30) = 0;
				Play(L, CB_PrimDraw, FP(0x30), 0x14);
			}
		}

		// 319..354: final prim model, camera drifts back
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 319);
			if (t < 0x24)
			{
				if (t == 0) cx::Decode((uint32_t)(ModelBuffer() + 0x28E9C), L, 0x354);
				F16(0x34) = G16(BASE_Y);
				F32(0x40) = (int32_t)(ModelBuffer() + 0x8D0);
				F32(0x8C) = (int32_t)(ModelBuffer() + 0x8D0);
				F32(0x38) = -1024;
				F16(0x3C) = 0; F16(0x32) = 0; F16(0x30) = 0;
				Play(L, CB_PrimDraw, FP(0x30), 0x14);
				if (!Pause())
				{
					int16_t d = (int16_t)(0x80 - (t << 7) / 36);
					G16(CAM_EYE) += d;
					G16(CAM_EYE + 2) -= d;
					G16(CAM_EYE + 4) -= d;
				}
			}
		}

		// one-shot sound cues
		switch ((uint16_t)W16(0x0C))
		{
		case 0x49: case 0x4F: case 0xF8: case 0xFA: case 0xFD: case 0x102: case 0x13D: cx::Sub6C6530(0x12AE7E0); break;
		case 0x4E: cx::Sub6C65A0(0x12AE7E4); break;
		case 0x50: case 0xE3: case 0x101: case 0x103: case 0x13C: case 0x13E: cx::Sub6C6530(0x12AE7E4); break;
		case 0x51: case 0xE4: case 0x13B: case 0x13F: cx::Sub6C6530(0x12AE7E8); break;
		case 0xE5: cx::Sub6C6530(0x12AE7EC); break;
		default: break;
		}

		// stream loads: the timeline waits (repeats the tick) while a load is still busy
		{
			uint32_t D = (uint32_t)(ModelBuffer() + 0x28E9C);
			int32_t cc = W16(0x0C);
			switch (cc)
			{
			case 0x00: if (cx::LoadBusy()) return 0; cx::Load(0x1F9, D, 1); cx::Load(0x1FA, (uint32_t)(ModelBuffer() + 0x28E9C), 1); cx::Load(0x1FB, (uint32_t)(ModelBuffer() + 0x28E9C), 2); break;
			case 0x1E: if (cx::LoadBusy()) return 0; cx::Load(0x1FC, D, 2); cx::Load(0x1FD, (uint32_t)(ModelBuffer() + 0x28E9C), 1); cx::Load(0x1FE, (uint32_t)(ModelBuffer() + 0x28E9C), 3); break;
			case 0x3C: if (cx::LoadBusy()) return 0; cx::Load(0x1FF, D, 1); cx::Load(0x200, (uint32_t)(ModelBuffer() + 0x3169C), 5); break;
			case 0x4C: if (cx::LoadBusy()) return 0; cx::Load(0x201, 0x1298C68, 5); break;
			case 0x55: if (cx::LoadBusy()) return 0; cx::Load(0x202, D, 0); cx::Load(0x203, 0x12A1164, 5); break;
			case 0xA1:
				if (cx::LoadBusy()) return 0;
				cx::Load(0x204, D, 0); cx::Load(0x205, 0x12A9664, 5);
				cx::Load(0x206, (uint32_t)(ModelBuffer() + 0x28E9C), 1);
				cx::Load(0x207, (uint32_t)(ModelBuffer() + 0x28E9C), 1);
				cx::Load(0x208, (uint32_t)(ModelBuffer() + 0x28E9C), 3);
				break;
			case 0xD3: if (cx::LoadBusy()) return 0; cx::Load(0x209, 0x1298C68, 5); cx::Load(0x20A, (uint32_t)(ModelBuffer() + 0x28E9C), 1); break;
			case 0x100: if (cx::LoadBusy()) return 0; cx::Load(0x20B, D, 1); cx::Load(0x20C, (uint32_t)(ModelBuffer() + 0x28E9C), 3); break;
			case 0x114:
				if (cx::LoadBusy()) return 0;
				G32(0x25216FC) = 0; G32(0x25216F8) = 0;
				*(int32_t *)(ModelBuffer() + 0x28E50) = 0;
				cx::Load(0x20D, 0x12A9664, 5); cx::Load(0x20E, (uint32_t)(ModelBuffer() + 0x28E9C), 2);
				break;
			case 0x12E:
				if (cx::LoadBusy()) return 0;
				G32(0x252172C) = 0; G32(0x2521728) = 0; G32(0x252171C) = 0; G32(0x2521718) = 0; G32(0x252170C) = 0; G32(0x2521708) = 0;
				cx::Load(0x20F, D, 0x10);
				break;
			case 0x13E: if (cx::LoadBusy()) return 0; break;
			default: break;
			}
		}
		cx::LoadPump();
		if (W16(0x0C) == 351) cx::ReleaseVoice((uint32_t)D32(0x10));

		// 232..266: bolts from the head to the ground
		{
			uint32_t t = (uint32_t)((int32_t)W16(0x0C) - 232);
			if (t < 0x23)
			{
				if (t == 0) InitTaskQueuePool(&QueueDebris().second, (void *)0x12A53E0, 0x14, 0x40);
				cx::VertexPos(model, 0, 0x32, FP(0x18));
				int32_t r = Rand();
				F16(0x2A) = 0;
				F16(0x28) = (int16_t)((r & 0x7FF) - 0x3FF);
				r = Rand();
				F16(0x2C) = (int16_t)((r & 0x3FF) + G32(BASE_Y) - 0x200);
				r = Rand();
				F16(0x20) = (int16_t)((r & 0xFF) - 0x80);
				r = Rand();
				F16(0x22) = (int16_t)(-0x40 - (r & 0x7F));
				r = Rand();
				F16(0x24) = (int16_t)(-0x40 - (r & 0x3F));
				cx::SpawnBoltO(FP(0x20), FP(0x18), FP(0x28), 0, 0, 0, 0x100, 0xFA, 1, 2);
			}
		}

		// camera cuts and sound cues
		switch ((uint16_t)W16(0x0C))
		{
		case 0:
			cx::Sub6C5300();
			cx::PlaySE(0x12AE4F0, 1, 0x80);
			G16(0x1D977A2) = 0;
			G16(0x1D8E038) = 0x120;
			G16(CAM_AT) = 0; G16(CAM_AT + 2) = (int16_t)0xEBE2; G16(CAM_AT + 4) = (int16_t)0xFD2D;
			G16(CAM_EYE) = 0; G16(CAM_EYE + 2) = (int16_t)0xF10D; G16(CAM_EYE + 4) = (int16_t)0xF841;
			break;
		case 0x41: cx::PlaySE(0x12AE4F4, 1, 0x80); break;
		case 0x45: cx::Sub6C5350(); cx::PlayStream(0x80, 1, 0x7F); break;
		case 0x4C:
			G16(CAM_AT) = 0; G16(CAM_AT + 4) = (int16_t)G32(BASE_Y3); G16(CAM_AT + 2) = (int16_t)0xFD74;
			G16(CAM_EYE) = (int16_t)0xF8EF; G16(CAM_EYE + 2) = (int16_t)0xFE9C; G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y3) + 0x8E4);
			break;
		case 0x7E: cx::Sub6C5300(); break;
		case 0xD1: cx::PlaySE(0x12AE4F8, 1, 0x80); break;
		case 0xD4: cx::Sub6C5350(); break;
		case 0xF6:
			G16(CAM_AT) = (int16_t)0xFB92; G16(CAM_AT + 2) = (int16_t)0xFD39; G16(CAM_EYE) = 0x10D9;
			G16(CAM_AT + 4) = (int16_t)(G32(BASE_Y) - 0x108);
			G16(CAM_EYE + 2) = (int16_t)0xFC8B; G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y) + 0xCA4);
			break;
		case 0xE2:
			G16(CAM_AT) = (int16_t)0xFFAD; G16(CAM_AT + 2) = (int16_t)0xF9BC; G16(CAM_EYE) = 0xB29; G16(CAM_EYE + 2) = (int16_t)0xFDD7;
			G16(CAM_AT + 4) = (int16_t)(G32(BASE_Y3) - 0x832);
			G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y3) + 0xA08);
			cx::PlayStream(0x80, 1, 0x7F);
			break;
		case 0x100:
			G16(CAM_AT + 4) = (int16_t)G32(BASE_Y);
			G16(CAM_AT) = 0; G16(CAM_AT + 2) = 0; G16(CAM_EYE) = (int16_t)0xF060; G16(CAM_EYE + 2) = (int16_t)0xF060;
			G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y) + 0xFA0);
			break;
		case 0x114:
			G32(BASE_Y2) = -4096;
			G16(0x1D977A2) = G16(0x1D9771C);
			G16(CAM_AT) = (int16_t)0xFFEF; G16(CAM_AT + 2) = (int16_t)0xEAF5; G16(CAM_AT + 4) = (int16_t)(G32(BASE_Y) + 0x130);
			G16(CAM_EYE) = (int16_t)0xFE52; G16(CAM_EYE + 2) = (int16_t)0xEFA4; G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y) + 0x498);
			break;
		case 0x115: cx::PlaySE(0x12AE4FC, 1, 0x80); break;
		case 0x12E:
			G32(BASE_Y2) = 0;
			G16(CAM_AT) = 0; G16(CAM_AT + 2) = (int16_t)0xFAF5; G16(CAM_AT + 4) = (int16_t)(G32(BASE_Y3) - 0x93A);
			G16(CAM_EYE) = 0; G16(CAM_EYE + 2) = (int16_t)0xF6B6; G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y3) + 0xDE3);
			break;
		case 0x13E:
			G16(CAM_AT) = 0; G16(CAM_AT + 4) = (int16_t)G32(BASE_Y); G16(CAM_AT + 2) = 0;
			G16(CAM_EYE) = 0x1000; G16(CAM_EYE + 2) = (int16_t)0xF000; G16(CAM_EYE + 4) = (int16_t)(G32(BASE_Y) - 0x1000);
			cx::PlayStream(0x80, 1, 0x7F);
			break;
		default: break;
		}

		if (W16(0x0C) == 345)
		{
			uint8_t *ctx = *(uint8_t **)(*(uint8_t **)0x25216E0 + 4);
			cx::ApplyResult(*(uint32_t *)(ctx + 8), ctx[0x10]);
		}
		if (W16(0x0C) >= 355)
		{
			memcpy((void *)0x1298C68, cx::SummonData(), 0x427F * 4);
			memcpy((void *)0x12A9664, cx::SummonData() + 0x109FC, 0x1367 * 4);
			cx::Sub508630(ModelBuffer() + 0x2E50, &DoneFlag());
			cx::Sub6C6500(1);
			return TASK_END;
		}
		W16(0x0C) = (int16_t)(W16(0x0C) + 1);
		return 0;
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
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048;
				p[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	static void CreatureHeld(int num, int den)
	{
		if (g_cm.tick != g_real_tick || !QueueCreature().second.head) return;
		uint8_t *nd = ModelBuffer();
		uint8_t *model = nd + 0x28;
		Mat4x3 bolt_frame = BoltFrame(), effect_camera = EffectCamera();
		EffectCameraMatrix(&Camera(), &EffectCamera());
		uint32_t size = 0;
		uint8_t *sk = (g_cm.model || g_cm.orbit) ? CreatureSkeleton(&size) : nullptr;
		static uint8_t sk_save[16 + 48 * 64];
		if (sk)
		{
			memcpy(sk_save, sk, size);
			// the pose drawn this tick is the one the tick started with (the draw uses the
			// matrices built before the anim read); no memo (tick 86 binds the model): as is
			if (g_cm.pose_ok && size == g_cm.pose_size)
			{
				PoseBlend(sk, g_cm.pose, num, den);
				cx::BuildBoneMatrices(nd + 0x88);
			}
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
			if (p.compose) ComposeAffineTransform(&Camera(), (Mat4x3 *)arg, (Mat4x3 *)arg);
			prim::play_held(p.l, (prim::Callback)p.cb, (int)arg, num, den);
		}
		BoltFrame() = bolt_frame;
		EffectCamera() = effect_camera;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master's queue order and GTE setup; packets go to a private buffer so the
	// real tick's arenas are untouched
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = PacketCursor(), frame_cursor = var<uint32_t>(0x1D8E054);
		PacketCursor() = (uint32_t)g_held_packets;
		var<uint32_t>(0x1D8E054) = (uint32_t)g_held_packets + 0x40000;
		CreatureHeld(num, den);
		if (QueueCreature().first.head)
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
				ComputeBonesWorldMatrices(ModelBuffer() + 0x88, ModelBuffer() + 0x68);
				for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
					if ((uint32_t)t->func == ORIG_ArcTask) ArcHeld((ArcNode *)t, num, den);
				memcpy(skel, skel_save, skel_size);
			}
		}
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		for (TaskNode *t = QueueBolts().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_BoltTask) BoltHeld((BoltNode *)t, num, den);
		GteSetRotMatrix(&BoltFrame());
		GteSetTransVector(&BoltFrame());
		for (TaskNode *t = QueueBolts().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_BranchTask) BranchHeld((BranchNode *)t, num, den);
		for (TaskNode *t = QueueDebris().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_FlashTask) FlashHeld((FlashNode *)t, num, den);
		for (TaskNode *t = QueueDebris().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_DebrisTask) DebrisHeld((DebrisNode *)t, num, den);
		PacketCursor() = cursor;
		var<uint32_t>(0x1D8E054) = frame_cursor;
	}

	// 0x6C87B0: new arc (called by the creature timeline)
	[[maybe_unused]] static void SpawnArc(uint8_t **bones, const int16_t *strips, const uint8_t *faces, uint8_t reach_shift, uint8_t fade_shift, int16_t speed)
	{
		ArcNode *arc = (ArcNode *)AddTaskToQueue(&QueueCreature().first, ORIG_ArcTask);
		if (!arc) return;
		arc->reach_shift = reach_shift;
		arc->fade_shift = fade_shift;
		arc->speed = speed;
		arc->strips = strips;
		arc->age = 0;
		arc->progress = 0;
		arc->faces = faces;
		arc->bones = bones;
		for (int i = 0; i < 16; i++)
		{
			int32_t s = ComputeSin(Div15(shl32(i, 11)));
			arc->pts[i].color = 0x80;
			arc->pts[i].width = (int16_t)(shl32(shl32(s, 6) + s, 2) >> 12);
			arc->pts[i].pad = 0;
		}
	}
}

	void register_mag116_quezacotl()
	{
		register_port(q116::ORIG_SequenceTask, (void *)q116::SequenceTask, "Q116 SequenceTask", 116);
		register_port(q116::ORIG_CreatureTask, (void *)q116::CreatureTask, "Q116 CreatureTask", 116, true);
		register_port(q116::ORIG_DebrisTask, (void *)q116::DebrisTask, "Q116 DebrisTask", 116, true);
		register_port(q116::ORIG_BranchTask, (void *)q116::BranchTask, "Q116 BranchTask", 116, true);
		register_port(q116::ORIG_BoltTask, (void *)q116::BoltTask, "Q116 BoltTask", 116, true);
		register_port(q116::ORIG_FlashTask, (void *)q116::FlashTask, "Q116 FlashTask", 116, true);
		register_port(q116::ORIG_ArcTask, (void *)q116::ArcTask, "Q116 ArcTask", 116, true);
		register_module_held(116, q116::HeldReady, q116::HeldFrame);
	}
}
