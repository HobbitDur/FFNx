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

// 30 fps part of mag278_carbuncle.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag278_carbuncle.cpp)

namespace ff8fx
{
namespace c278
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;     // real tick on which the ported master last ran

	static void held_note_master() { g_ported_tick = g_real_tick; }

	// ------------------------------------------------------------------
	// Held-frame memos (what the real tick drew)
	// ------------------------------------------------------------------
	struct CreatureMemo
	{
		uint32_t tick;
		const void *node;
		int16_t c0;   // counter the tick ran with
		bool model;   // model drawn
		bool dome;    // dome drawn
		int prim;     // last prim stage played: 0 none, 1 = 110..145, 2 = 145..176
		Mat4x3 root;  // root the model was drawn with
	};
	static CreatureMemo g_cm = { 0xFFFFFFFF };

	// the creature skeleton (pose + bone matrices) as the real tick drew it: the tick may start a new
	// animation after its draw (110 / 136: ReadAnimationStart reads the new animation's first frame)
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	static uint8_t g_cm_skel[SKEL_MAX];
	static uint32_t g_cm_skel_size = 0;
	static uint8_t *Skel(uint8_t *e, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(e + 0x64);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	// the tasks that drew on the real tick, in their execution order (= the master's queue order): a
	// task that ends on the tick (TASK_END after its draw) is no longer queued on the held frames but
	// its last picture stays until the next tick, as at 15 fps
	enum { DL_SPARKLE = 1, DL_SHARD, DL_RUBY, DL_ORBIT, DL_RING };
	static struct { uint32_t tick; int n; uint8_t kind[256]; void *node[256]; } g_dl = { 0xFFFFFFFF, 0 };
	static void held_note_draw(int kind, void *node)
	{
		if (g_dl.tick != g_real_tick) { g_dl.tick = g_real_tick; g_dl.n = 0; }
		if (g_dl.n < 256) { g_dl.kind[g_dl.n] = (uint8_t)kind; g_dl.node[g_dl.n++] = node; }
	}
	static bool Queued(const TaskNode *head, const void *n)
	{
		for (const TaskNode *t = head; t; t = t->next) if (t == n) return true;
		return false;
	}

	struct RubyMemo { int16_t c; };
	struct OrbitMemo { int16_t c, angle, tilt; };
	struct RingMemo { int16_t c, scale; };
	struct BeamMemo { int16_t p[4]; int16_t frame; };
	struct SparkleMemo { int16_t pos[4]; };
	struct ShardMemo { int16_t pos[4]; };
	static NodeMemo<RubyMemo, 16> g_ruby_memo;
	static NodeMemo<OrbitMemo, 64> g_orbit_memo;
	static NodeMemo<RingMemo, 32> g_ring_memo;
	static NodeMemo<BeamMemo, 512> g_beam_memo;
	static NodeMemo<SparkleMemo, 128> g_sparkle_memo;
	static NodeMemo<ShardMemo, 16> g_shard_memo;

	static void held_note_sparkle(SparkleNode *p)
	{
		if (SparkleMemo *m = g_sparkle_memo.put(p)) memcpy(m->pos, p->pos, 8);
		held_note_draw(DL_SPARKLE, p);
	}

	static void held_note_shard(ShardNode *p)
	{
		if (ShardMemo *m = g_shard_memo.put(p)) memcpy(m->pos, p->pos, 8);
		held_note_draw(DL_SHARD, p);
	}

	static void held_note_orbit(OrbitNode *o)
	{
		if (OrbitMemo *m = g_orbit_memo.put(o)) { m->c = o->counter; m->angle = o->angle; m->tilt = o->tilt; }
		held_note_draw(DL_ORBIT, o);
	}

	static void held_note_ring(RingNode *r)
	{
		if (RingMemo *m = g_ring_memo.put(r)) { m->c = r->counter; m->scale = r->scale; }
		held_note_draw(DL_RING, r);
	}

	// beam: the sprites' position and frame as drawn
	static void held_note_beam(BeamNode *b, const int16_t *out)
	{
		if (BeamMemo *m = g_beam_memo.put(b)) { memcpy(m->p, out, 8); m->frame = b->counter; }
	}

	static void held_note_ruby(RubyNode *r)
	{
		if (RubyMemo *m = g_ruby_memo.put(r)) m->c = r->counter;
		held_note_draw(DL_RUBY, r);
	}

	static uint8_t g_stage1_last[0x514];
	static bool g_stage1_last_ok = false;

	// creature: the counter the tick runs with; what it draws is noted below
	static void held_note_creature(CreatureNode *cn)
	{
		CreatureMemo &memo = g_cm;
		memo.tick = g_real_tick;
		memo.node = cn;
		memo.c0 = cn->counter;
		memo.model = memo.dome = false;
		memo.prim = 0;
		g_stage1_last_ok = false;
	}

	static void held_note_model(CreatureNode *cn)
	{
		g_cm.model = true;
		g_cm.root = Root(cn);
		uint32_t size = 0;
		uint8_t *sk = Skel(E(cn), &size);
		g_cm_skel_size = sk ? size : 0;
		if (sk) memcpy(g_cm_skel, sk, size);
	}

	static void held_note_dome() { g_cm.dome = true; }

	// prim stage 1's last play (145) is followed in the same tick by the stage-2 layout decoded over it:
	// its layout as that play started, to draw that last picture again on the held frames
	static void held_note_prim_stage(int stage)
	{
		g_cm.prim = stage;
		const CreatureNode *cn = (const CreatureNode *)g_cm.node;
		if (stage != 1 || !cn || cn->counter != 0x91) return;
		memcpy(g_stage1_last, cn->prim, sizeof(g_stage1_last));
		g_stage1_last_ok = true;
	}

	// ruby view matrix for a held frame: RubyMatrices on a copy of the node's model matrix
	static void RubyView(const RubyNode *r, Mat4x3 *view)
	{
		Mat4x3 m = r->model;
		RubyMatrices(r, &m, view);
	}

	// RingDraw (vanilla file) with a given fade (the in-between one)
	static void RingDrawFade(int16_t height, int16_t scale, int32_t fade, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t V[4] = { 0, 0, height, 0 }; // 4th word: vanilla stack garbage
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(V);
		GteMVMVA_RotV0Tr();
		Mat4x3 T = {};
		GteReadMAC123(T.t);
		GteSetTransVector(&T);
		Mat4x3 S = {};
		S.m[0][0] = scale;
		S.m[1][1] = scale;
		S.m[2][2] = scale;
		MatMul56C220(&S, view);
		GteSetRotMatrix(&S);
		*(uint32_t *)h = 0x10BF480;
		*(int32_t *)(h + 0xC) = fade;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}


	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn half way
	// between what the real tick drew and what the next tick will draw; flipbook frames,
	// texture scrolls, spawns and cuts keep their 15 Hz steps. Nothing here changes game
	// state: no RNG, no spawn, no sound, no camera write.
	// ------------------------------------------------------------------
	static uint8_t g_held_packets[0x60000];
	static uint8_t g_scratch_save[0x1000];
	static uint8_t g_skel_save[16 + 48 * 256];

	// the mirror pass (0x6812E0) sets the screen flash to 0 and rebuilds the bone matrices of
	// battle model 0x1D989D0: both are put back after a held draw
	struct MirrorGuard
	{
		uint16_t level[4];
		uint32_t mode[4];
		uint8_t *skel;
		uint32_t skel_size;
		MirrorGuard()
		{
			for (int i = 0; i < 4; i++)
			{
				level[i] = var<uint16_t>(0x1D98992 + 0x2C * i);
				mode[i] = var<uint32_t>(0x1D989B8 + 0x2C * i);
			}
			uint8_t *com = var<uint8_t *>(0x1D989D0 + 4);
			skel = com ? *(uint8_t **)com : nullptr;
			skel_size = skel ? 16 + 48 * (uint32_t)skel[0] : 0;
			if (skel_size > sizeof(g_skel_save)) skel = nullptr;
			if (skel) memcpy(g_skel_save, skel, skel_size);
		}
		~MirrorGuard()
		{
			for (int i = 0; i < 4; i++)
			{
				var<uint16_t>(0x1D98992 + 0x2C * i) = level[i];
				var<uint32_t>(0x1D989B8 + 0x2C * i) = mode[i];
			}
			if (skel) memcpy(skel, g_skel_save, skel_size);
		}
	};

	// The creature model at tick + num / den: between the pose the real tick drew (g_cm_skel) and the one
	// the next tick's AdvanceModelAnim (0x6FBDB0) reads, run ahead on the real skeleton and command and
	// put back: it restarts a completed animation (the two-frame loops from 110 on) and continues one
	// the tick started after its draw (110 / 136); paused, the drawn pose holds. PoseEnd puts the whole
	// real skeleton back.
	static uint8_t g_ps_save[SKEL_MAX], g_ps_next[SKEL_MAX];
	static uint32_t g_ps_size = 0;
	static void PoseBegin(CreatureNode *cn, int num, int den)
	{
		uint8_t *e = E(cn);
		uint32_t size = 0;
		uint8_t *sk = Skel(e, &size);
		g_ps_size = 0;
		if (!sk || size != g_cm_skel_size) { pose_midpoint(e + 0x60, e + 0x6C, num, den); return; }
		memcpy(g_ps_save, sk, size);
		uint8_t cmd[8];
		memcpy(cmd, e + 0x6C, sizeof(cmd));
		if (!Pause()) AdvanceModelAnim(e);
		memcpy(g_ps_next, sk, size);
		memcpy(e + 0x6C, cmd, sizeof(cmd));
		memcpy(sk, g_cm_skel, size);
		pose_midpoint_to(e + 0x60, g_ps_next, num, den);
		g_ps_size = size;
	}
	static void PoseEnd(CreatureNode *cn)
	{
		uint32_t size = 0;
		uint8_t *sk = Skel(E(cn), &size);
		if (sk && g_ps_size == size) memcpy(sk, g_ps_save, size);
		g_ps_size = 0;
	}

	static void CreatureHeld(CreatureNode *cn, int num, int den)
	{
		const CreatureMemo &m = g_cm;
		if (m.tick != g_real_tick || m.node != cn) return;
		int32_t c0 = m.c0, c1 = cn->counter;
		bool step = c1 == c0 + 1;
		uint8_t *e = E(cn);

		// model: midpoint pose, the root it was drawn with (the 215 floor drop is a step)
		if (m.model)
		{
			Mat4x3 save = Root(cn);
			Root(cn) = m.root;
			PoseBegin(cn, num, den);
			FrameCursor() = DrawModel(e, Scratch(), FrameCursor(), var<uint32_t>(0x1D969A8));
			PoseEnd(cn);
			Root(cn) = save;
		}

		// dome: brightness and morph in between (both continuous across the stage changes)
		if (m.dome)
		{
			int32_t flash = FlashAt(c0), s = DomeMorph(c0);
			if (step && DomeDrawn(c1))
			{
				flash = lerp_i(flash, FlashAt(c1), num, den);
				s = lerp_i(s, DomeMorph(c1), num, den);
			}
			MirrorGuard guard;
			DomeDraw(c0, flash, s);
		}

		// prim stages (only the last play of the tick: at 145 the layout is replaced in place)
		if (m.prim == 1)
		{
			PrimCtx ctx;
			PoseBegin(cn, num, den); // vertex 0xB0 at the in-between pose
			CtxStage110(ctx, cn);
			PoseEnd(cn);
			prim::play_held((prim::Layout *)cn->prim, (prim::Callback)CB_PrimObject, (int)&ctx, num, den);
		}
		else if (m.prim == 2)
		{
			if (g_stage1_last_ok && m.c0 == 0x91)
			{
				// stage 1's last picture (vanilla shows it until the next tick): its play again from the layout
				// it started with (the play steps, then draws), the stage-2 layout put back after
				static uint8_t now[0x514];
				memcpy(now, cn->prim, sizeof(now));
				memcpy(cn->prim, g_stage1_last, sizeof(now));
				PrimCtx ctx1;
				Mat4x3 root = Root(cn);
				if (m.model) Root(cn) = m.root; // the root that play used (145 moves it after)
				PoseBegin(cn, 0, den);
				CtxStage110(ctx1, cn);
				PoseEnd(cn);
				Root(cn) = root;
				prim::play((prim::Layout *)cn->prim, (prim::Callback)CB_PrimObject, (int)&ctx1, (int)Pause());
				memcpy(cn->prim, now, sizeof(now));
			}
			PrimCtx ctx;
			CtxStage145(ctx);
			prim::play_held((prim::Layout *)cn->prim, (prim::Callback)CB_PrimObject, (int)&ctx, num, den);
		}
	}

	static void SparkleHeld(SparkleNode *p)
	{
		const SparkleMemo *m = g_sparkle_memo.get(p);
		if (!m) return;
		SparkleDraw(m->pos); // static point: the frame drawn, with the held-frame camera
	}

	static void ShardHeld(ShardNode *p, int num, int den)
	{
		const ShardMemo *m = g_shard_memo.get(p);
		if (!m) return;
		int16_t pos[4];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		pos[3] = m->pos[3]; // flipbook frame keeps its 15 Hz step
		ShardDraw(pos, p->variant);
	}

	// orbit centre the next tick uses (the ruby moves it along the path for its ticks 15..34)
	static void CenterNext(const RubyNode *r, int16_t out[3])
	{
		if ((uint32_t)(r->counter - 0xF) < 0x14) RubyCenter(r->counter, out);
		else memcpy(out, r->center, 6);
	}

	static void RubyHeld(RubyNode *r, int num, int den)
	{
		const RubyMemo *m = g_ruby_memo.get(r);
		if (!m) return;
		int32_t c0 = m->c;
		bool step = r->counter == c0 + 1;
		if (c0 < 0x15)
		{
			PrimCtx ctx;
			CtxRuby(ctx, r);
			prim::play_held((prim::Layout *)r->prim, (prim::Callback)CB_PrimObject, (int)&ctx, num, den);
		}
		int32_t e = c0 - 0x14;
		if ((uint32_t)e < 0x23)
		{
			Mat4x3 view;
			RubyView(r, &view);
			int32_t height = WallHeight(e), fade = WallFade(e);
			if (step && (uint32_t)(e + 1) < 0x23)
			{
				height = lerp_i(height, WallHeight(e + 1), num, den);
				fade = lerp_i(fade, WallFade(e + 1), num, den);
			}
			WallDraw(c0, height, fade, &view); // texture scroll keeps its 15 Hz step
			FieldFree(0x60);
		}
	}

	static void OrbitHeld(OrbitNode *o, int num, int den)
	{
		const OrbitMemo *m = g_orbit_memo.get(o);
		if (!m) return;
		Mat4x3 view;
		RubyView(o->parent, &view);
		int32_t fade, sx;
		OrbitFade(m->c, fade, sx);
		int16_t angle = m->angle, tilt = m->tilt;
		int16_t center[3];
		memcpy(center, o->parent->center, 6);
		if (o->counter == m->c + 1)
		{
			int32_t fade1, sx1;
			OrbitFade(o->counter, fade1, sx1);
			fade = lerp_i(fade, fade1, num, den);
			sx = lerp_i(sx, sx1, num, den);
			angle = lerp_angle(angle, o->angle, num, den);
			tilt = lerp_angle(tilt, o->tilt, num, den);
			int16_t next[3];
			CenterNext(o->parent, next);
			for (int k = 0; k < 3; k++) center[k] = (int16_t)lerp_i(center[k], next[k], num, den);
		}
		OrbitDraw(center, angle, tilt, fade, (int16_t)sx, &view);
	}

	static void RingHeld(RingNode *g, int num, int den)
	{
		const RingMemo *m = g_ring_memo.get(g);
		if (!m) return;
		Mat4x3 view;
		RubyView(g->parent, &view);
		int32_t fade = RingFade(m->c), scale = m->scale;
		if (g->counter == m->c + 1)
		{
			fade = lerp_i(fade, RingFade(g->counter), num, den);
			scale = lerp_i(scale, g->scale, num, den);
		}
		RingDrawFade(g->height, (int16_t)scale, fade, &view);
	}

	// (never reached in vanilla, see AppendByValue) the sprites where they were drawn
	static void BeamHeld(uint8_t *h, BeamNode *b)
	{
		const BeamMemo *m = g_beam_memo.get(b);
		if (!m) return;
		Mat4x3 view;
		RubyView(b->parent, &view);
		BeamDraw(h, m->p, b->scale, m->frame, &view);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static MasterNode *Master()
	{
		for (TaskNode *t = QueueRoot().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_SequenceTask) return (MasterNode *)t;
		return nullptr;
	}

	// mirrors the master's queue order; packets go to a private buffer (both module cursors
	// and the frame arena are redirected and put back); the model/morph scratch and the beam
	// header global are put back too
	static void HeldFrame(int num, int den)
	{
		MasterNode *master = Master();
		if (!master || !master->spawned) return;
		uint32_t cursor = PacketCursor(), cursor2 = PacketCursor2(), frame_cursor = FrameCursor();
		uint8_t *beam_header = BeamHeader();
		memcpy(g_scratch_save, Scratch(), sizeof(g_scratch_save));
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x20000;
		PacketCursor2() = (uint32_t)g_held_packets + 0x40000;
		for (TaskNode *t = QueueCreature().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask) CreatureHeld((CreatureNode *)t, num, den);
		// the small tasks as they drew on the real tick (draw list): one that ended on the tick keeps its
		// last picture (fraction 0: the drawn state; an ended orbit / ring keeps its counter, which holds)
		if (g_dl.tick == g_real_tick)
			for (int i = 0; i < g_dl.n; i++)
			{
				void *p = g_dl.node[i];
				switch (g_dl.kind[i])
				{
				case DL_SPARKLE: SparkleHeld((SparkleNode *)p); break;
				case DL_SHARD: ShardHeld((ShardNode *)p, Queued(QueueRubies().second.head, p) ? num : 0, den); break;
				case DL_RUBY: if (Queued(QueueRubies().first.head, p)) RubyHeld((RubyNode *)p, num, den); break;
				case DL_ORBIT: OrbitHeld((OrbitNode *)p, Queued(QueueOrbits().first.head, p) ? num : 0, den); break;
				case DL_RING: RingHeld((RingNode *)p, Queued(QueueOrbits().second.head, p) ? num : 0, den); break;
				default: break;
				}
			}
		if (QueueBeams().head)
		{
			uint8_t *h = BeamHeaderInit();
			for (TaskNode *t = QueueBeams().head; t; t = t->next)
				if ((uint32_t)t->func == ORIG_BeamTask) BeamHeld(h, (BeamNode *)t);
			FieldFree(0xB4);
		}
		BeamHeader() = beam_header;
		memcpy(Scratch(), g_scratch_save, sizeof(g_scratch_save));
		PacketCursor() = cursor;
		PacketCursor2() = cursor2;
		FrameCursor() = frame_cursor;
	}

	// ------------------------------------------------------------------
	// Held-frame camera. The creature writes the battle camera every tick of 1..36, 86..109,
	// 122..145 and 215..282 (relative swings through MAG_278_sub_683D10) plus cuts at 1, 145,
	// 215 and 231. The next tick's camera is predicted from the current one by the creature's
	// camera code for the counter it holds now (pure math on copies); cuts hold.
	// The look-at follows vertex 0xB0 of the posed model on 86..91 and 215..230: kept as drawn
	// (it would need the next pose), the eye still swings about it.
	// ------------------------------------------------------------------
	struct Cam { int16_t w[3], l[3]; };

	static bool PredictCamera(int32_t c, Cam &k)
	{
		bool cut = false;
		int32_t v = c - 1;
		if ((uint32_t)v < 0x24)
		{
			uint32_t s = ((uint32_t)v << 12) / 36u;
			if (v == 0)
			{
				cut = true;
				k.l[2] = (int16_t)Base(); k.l[0] = 0; k.l[1] = 0;
				k.w[0] = 0; k.w[1] = (int16_t)0xF060; k.w[2] = (int16_t)(Base() + 0xBB8);
			}
			CamRotate(k.w, k.l, ComputeCos((int32_t)s) >> 6, 0xFAA);
			k.w[1] += 0x32;
		}
		int32_t e = c - 0x56;
		if ((uint32_t)e < 0x18)
		{
			if (e == 0) cut = true; // the look-at jumps onto the model
			CamRotate(k.w, k.l, -0x20, 0xFA0);
			k.w[1] += -30;
		}
		e = c - 0x6E;
		if ((uint32_t)e < 0x24 && (uint32_t)e > 0xB)
		{
			int32_t t = shl32(0xB - e, 12) / 25 + 0x1000;
			int16_t y = k.w[1];
			CamRotate(k.w, k.l, t >> 5, t / 8 + 0x1000);
			k.w[1] = y;
		}
		if (c == 0x91) cut = true;
		e = c - 0xD7;
		if ((uint32_t)e < 0x10)
		{
			if (e == 0) cut = true;
			CamRotate(k.w, k.l, 0, 0xFA0);
		}
		if (c == 0xE7) cut = true;
		e = c - 0xF7;
		if ((uint32_t)e < 0x11B)
		{
			CamRotate(k.w, k.l, ComputeSin(shl32(e & 0x1FFFFF, 7)) >> 7, 0x1036);
			k.l[1] += -0x32;
			k.w[1] += -30;
		}
		return cut;
	}

	static CreatureNode *Creature()
	{
		for (TaskNode *t = QueueCreature().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask && t == g_cm.node) return (CreatureNode *)t;
		return nullptr;
	}


	// held-frame camera of effect 278 at tick + num / den: false = the module does not drive
	// the camera now (creature not running / not ported this tick)
	static bool mag278_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cm.tick != g_real_tick || !HeldReady()) return false;
		CreatureNode *cn = Creature();
		if (!cn) return false;
		Cam now, next;
		for (int i = 0; i < 3; i++) { now.w[i] = CamWorld(i); now.l[i] = CamLookAt(i); }
		next = now;
		bool cut = PredictCamera(cn->counter, next);
		for (int i = 0; i < 3; i++)
		{
			int32_t dw = (int32_t)next.w[i] - now.w[i], dl = (int32_t)next.l[i] - now.l[i];
			if (dw > 1500 || dw < -1500 || dl > 1500 || dl < -1500) cut = true;
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = cut ? now.w[i] : (int16_t)lerp_i(now.w[i], next.w[i], num, den);
			lookat[i] = cut ? now.l[i] : (int16_t)lerp_i(now.l[i], next.l[i], num, den);
		}
		return true;
	}
}

	static void register_mag278_held()
	{
		mark_held(c278::ORIG_CreatureTask);
		mark_held(c278::ORIG_RubyTask);
		mark_held(c278::ORIG_SparkleTask);
		mark_held(c278::ORIG_ShardTask);
		mark_held(c278::ORIG_OrbitTask);
		mark_held(c278::ORIG_RingTask);
		mark_held(c278::ORIG_BeamTask);
		register_module_held(278, c278::HeldReady, c278::HeldFrame);
		register_module_camera(278, c278::mag278_held_camera);
	}
}
