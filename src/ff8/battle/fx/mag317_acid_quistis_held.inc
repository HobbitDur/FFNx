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

// 30 fps part of mag317_acid_quistis.cpp (Quistis' Acid = Gastric Juice 128 adapted: 128's held part on the 317 code): the draw log of
// the real tick and the held-frame drawing (included at the end of mag317_acid_quistis.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in order with the GTE registers and the Field_Alloc scratch it
// started from (draw headers and the melting target's work block keep the scratch bytes where the
// code does not write). A held frame replays the log in the same order:
//   Layout   - (target task, draws THEN updates) the prim-model layout through prim::play_held:
//              every object's exact in-between record, with the effect-relative camera of the view the
//              frame draws with (128's held part keeps the real tick's); the anchor (taken at the spawn) holds;
//              blended vertex frames go to a private buffer.
//   Melting  - (draws THEN updates) the target at its in-between pose (pose_midpoint), the melt
//              amplitude half way to the next step's sin (the last step holds); the melt amounts,
//              entity matrix, tint and shadow are the real ones. Vertices go to a private vertex
//              buffer, packets to a private copy of the frame arena; the entity-shadow temporaries
//              are put back.
//   Stream   - (updates THEN draws) the chain as drawn: every point num / den of the way to the
//              next tick's draw (position + drift, fade after the next tick's two fade steps of 317),
//              projected and ribboned by the vanilla code on private copies. The next tick's two
//              new points (growth) appear on that tick; at fraction 1 the tail pairs this tick
//              freed are left out and a chain that ended draws nothing.
//   Particle - (draws THEN updates) spray / drip: position half way along the next step (both
//              falling); the flipbook frame and size hold; a particle whose task ends after this
//              draw holds. Splash: fixed.
// The streams and the spray start at a point of the caster's animated model (0x660240), taken once at their spawn: nothing
// drawn follows her afterwards. Spawns (random) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates
// per tick: the held packets start where the tick's first draw started.

namespace ff8fx
{
namespace acid_quistis
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PARTICLE = 1, REC_STREAM = 2, REC_MELT = 3, REC_PLAY = 4 };

	struct Rec
	{
		uint32_t kind;
		uint32_t fn;              // particle: its task function
		ParticleNode node;        // particle: as drawn
		int first, n;             // stream: its points (as drawn) in g_points
		int16_t count;            // stream: the chain's count (2 or more: ribbon)
		uint8_t *entity;          // melting: the target
		int32_t step;             // melting: e (0..39)
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_npoints = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the tick's first melting draw
	static Rec g_recs[0xA0];
	static Point g_points[0x200];        // stream points as drawn
	static Point g_copy[0x100];          // held chain
	static uint8_t g_held_packets[0x10000]; // module arena (0x10000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + melting target packets
	static uint8_t g_held_verts[0x10000];   // melting target vertices (indices are 12-bit: 0x8000)
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x1508)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_npoints = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_particle(uint32_t fn, const ParticleNode *p)
	{
		Rec *r = NewRec(REC_PARTICLE);
		if (!r) return;
		r->fn = fn;
		r->node = *p;
	}

	static void held_note_stream(const StreamNode *s)
	{
		Rec *r = NewRec(REC_STREAM);
		if (!r) return;
		r->first = g_npoints;
		r->n = 0;
		r->count = s->count;
		for (const Point *q = s->head; q; q = q->next)
		{
			if (g_npoints >= (int)(sizeof(g_points) / sizeof(g_points[0])))
			{
				g_nrec--;
				return;
			}
			g_points[g_npoints++] = *q;
			r->n++;
		}
	}

	static void held_note_melt(uint8_t *entity, int32_t e)
	{
		Rec *r = NewRec(REC_MELT);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->entity = entity;
		r->step = e;
	}

	static void held_note_play(const TargetNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->layout;
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a held frame's melting-target outputs: frame arena packets (a copy of the real tick's, so
	// bytes the draw leaves alone are the real ones), the vertex buffer, the entity-shadow
	// temporaries; all put back
	struct FrameScope
	{
		uint32_t cursor, bones, words[4];
		uint8_t shadow[0x80];
		FrameScope()
		{
			bones = BoneWorkspace();
			cursor = FrameCursor();
			words[0] = var<uint32_t>(0xB8B9E4);
			words[1] = var<uint32_t>(0xB8B9EC);
			words[2] = var<uint32_t>(0x1D99C18);
			words[3] = var<uint32_t>(0x1D99C1C);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			if (g_frame_packets && cursor > g_frame_packets)
			{
				uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			BoneWorkspace() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0x1D99C1C) = words[3];
			var<uint32_t>(0x1D99C18) = words[2];
			var<uint32_t>(0xB8B9EC) = words[1];
			var<uint32_t>(0xB8B9E4) = words[0];
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
		}
	};

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		ParticleNode m = r.node;
		const int32_t end = r.fn == ORIG_SprayTask ? 9 : r.fn == ORIG_DripTask ? 0xD : 0x10;
		if (m.counter + 1 < end)
		{
			ParticleNode next = m;
			if (r.fn == ORIG_SprayTask) SprayStep(&next);
			else if (r.fn == ORIG_DripTask) DripStep(&next);
			for (int c = 0; c < 3; c++) m.pos[c] = lerp16(m.pos[c], next.pos[c], num, den);
		}
		ParticleDraw(&m, r.fn == ORIG_SprayTask ? SEQ_Spray : r.fn == ORIG_DripTask ? SEQ_Drip : SEQ_Splash);
	}

	static void MeltHeld(const Rec &r, int num, int den)
	{
		const int32_t e = r.step;
		int32_t sinv = MeltSin(e);
		if (e + 1 < 0x28) sinv = lerp_i(sinv, MeltSin(e + 1), num, den);
		pose_midpoint(r.entity + 0x60, r.entity + 0x6C, num, den);
		DrawMeltingTarget(r.entity, sinv);
	}

	// the points the tick kept (the free loop after the draw: faded tail pairs), -1 = the task ended
	static int KeptFrom(const Rec &r)
	{
		const Point *p = &g_points[r.first];
		int i = 0, count = r.count;
		if (p[0].fade != 0) return 0;
		while (i + 1 < r.n && p[i + 1].fade == 0)
		{
			i++;
			if (--count < 2) return -1;
			if (p[i].fade != 0) break;
		}
		return i;
	}

	static void StreamHeld(const Rec &r, int num, int den)
	{
		int from = 0;
		if (num >= den)
		{
			from = KeptFrom(r);
			if (from < 0) return;
		}
		const int n = r.n - from;
		if (n < 1 || n > (int)(sizeof(g_copy) / sizeof(g_copy[0]))) return;
		for (int i = 0; i < n; i++)
		{
			const Point &h = g_points[r.first + from + i];
			Point &c = g_copy[i];
			c = h;
			// next tick: drifted by the velocity (after this draw), then two fade steps (before its draw)
			for (int k = 0; k < 3; k++) c.pos[k] = lerp16(h.pos[k], (int16_t)(h.pos[k] + h.vel[k]), num, den);
			c.fade = lerp16(h.fade, FadeStep(FadeStep(h.fade)), num, den);
			c.next = i + 1 < n ? &g_copy[i + 1] : nullptr;
		}
		// the chain's count drops with the freed points (2 or more: ribbon)
		StreamDraw(g_copy, (int32_t)r.count - from);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PARTICLE: ParticleHeld(r, num, den); break;
			case REC_STREAM: StreamHeld(r, num, den); break;
			case REC_MELT: MeltHeld(r, num, den); break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, 0x200);
				arg.morph = (uint32_t)g_held_morph;
				// the effect-relative camera of the view this frame draws with (the root derives it from the camera every tick,
				// 0x67AAE0, before its draws): flag-0x1000 records place their offsets with it; put back after
				const Mat4x3 ec = EffectCamera();
				EffectCameraMatrix(&Camera(), &EffectCamera());
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				EffectCamera() = ec;
				break;
			}
			}
		}
	}
}

	static void register_mag317_held()
	{
		mark_held(acid_quistis::ORIG_TargetTask);
		mark_held(acid_quistis::ORIG_StreamTask);
		mark_held(acid_quistis::ORIG_SprayTask);
		mark_held(acid_quistis::ORIG_DripTask);
		mark_held(acid_quistis::ORIG_SplashTask);
		register_module_held(317, acid_quistis::HeldReady, acid_quistis::HeldFrame);
	}
}
