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

// 30 fps part of mag107_demi.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag107_demi.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Demi task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers and the silhouette's work
// block keep the scratch bytes where the code does not write). A held frame replays the log in
// the same order:
//   Vortex     - the prim-model layout through prim::play_held: every object's exact in-between
//                record (position, rotation, scale, colour, fade, vertex-frame blend); the held
//                callback only draws (object 0's orb / spark spawns and its size write are logic);
//                the in-between size of object 0 is kept for the orbs; blended vertex frames go to
//                a private buffer.
//   Silhouette - the target at its in-between pose (pose_midpoint), the anchor projected with the
//                current camera, the pull strength and radius of the real draw, in the colour it
//                was drawn in; its shadow is drawn again. Vertices go to a private copy of the
//                bone workspace, packets to a private copy of the frame arena. The pull reads the
//                mote matrix in exe data (past its distance table): it gets the real draw's bytes.
//   Mote       - radius half way to the next tick's (the shrink accelerates); the frame steps at
//                15 Hz; the camera-facing matrix it rewrites in exe data is put back.
//   Orb        - angle half way (+4), distance from the vortex's in-between size.
//   Spark, the two target sequences - drawn exactly as on the real tick (fixed place, 15 Hz
//                frames).
// The last draw of a task (it ends after its draw) holds. No Demi task has a draw-only mode.

namespace ff8fx
{
namespace demi107
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_NODE = 1, REC_BURST = 2, REC_SILHOUETTE = 3, REC_PLAY = 4 };

	struct Rec
	{
		uint32_t kind;
		uint32_t fn;              // node: its task
		uint8_t node[0x20];       // node: as drawn
		int16_t size;             // orb: its target's vortex size when drawn
		const TargetNode *t;      // burst, silhouette, play
		uint32_t seq;             // burst
		int32_t step;             // burst frame / silhouette pull step
		int16_t radius;           // silhouette: radius it was drawn with
		uint32_t colour;          // silhouette: the target's colour
		int32_t mote_t[3];        // silhouette: the mote matrix translation when drawn (see SilhouetteHeld)
		prim::Layout *layout;     // play
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the first silhouette
	static Rec g_recs[256];
	static uint8_t g_held_packets[0x20000]; // module arena (0x10000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + silhouette packets
	static uint8_t g_held_verts[0x10000];   // silhouette vertices (indices are 12-bit: 0x8000)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0x1DC8)

	// the vortex's in-between size (object 0's x scale) per target task, from the held replay
	struct MidSize { const TargetNode *t; int16_t size; };
	static MidSize g_mid[8];
	static int g_nmid = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
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

	static void held_note_node(uint32_t fn, const void *node)
	{
		Rec *r = NewRec(REC_NODE);
		if (!r) return;
		r->fn = fn;
		memcpy(r->node, node, fn == ORIG_OrbTask ? sizeof(OrbNode) : sizeof(MoteNode));
		if (fn == ORIG_OrbTask) r->size = ((const OrbNode *)node)->owner->size;
	}

	static void held_note_burst(const TargetNode *t, uint32_t seq, int32_t e)
	{
		Rec *r = NewRec(REC_BURST);
		if (!r) return;
		r->t = t;
		r->seq = seq;
		r->step = e;
	}

	static void held_note_silhouette(const TargetNode *t, int32_t step)
	{
		Rec *r = NewRec(REC_SILHOUETTE);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->t = t;
		r->step = step;
		r->radius = t->radius;
		r->colour = *(const uint32_t *)(t->entity + 0x28);
		memcpy(r->mote_t, MOTE_Matrix->t, sizeof(r->mote_t));
	}

	static void held_note_play(const TargetNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->t = t;
		r->layout = (prim::Layout *)t->vortex;
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the held callback: draws only (the spawns and the size write are logic); keeps object 0's
	// in-between x scale for the orbs
	static void NoteMidSize(const TargetNode *t, int16_t size)
	{
		for (int i = 0; i < g_nmid; i++)
			if (g_mid[i].t == t)
			{
				g_mid[i].size = size;
				return;
			}
		if (g_nmid < 8) g_mid[g_nmid++] = { t, size };
	}

	static void __cdecl VortexPartHeld(prim::Layout *l, prim::Record *r, int arg_)
	{
		PrimArg *arg = (PrimArg *)arg_;
		if (VortexSkips(r))
		{
			if (r->index == 0) NoteMidSize(arg->owner, 0);
			return;
		}
		if (r->index == 0) NoteMidSize(arg->owner, r->scale[0]);
		VortexDraw(l, r, arg);
	}

	// a held frame's other outputs: frame arena packets (a copy of the real tick's, so bytes the
	// draw leaves alone are the real ones), the silhouette's vertex buffer, the mote matrix
	// translation in exe data; all put back
	struct FrameScope
	{
		uint32_t cursor, bones;
		int32_t mote_t[3];
		FrameScope()
		{
			memcpy(mote_t, MOTE_Matrix->t, sizeof(mote_t));
			bones = BoneWorkspace();
			cursor = FrameCursor();
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
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
			memcpy(MOTE_Matrix->t, mote_t, sizeof(mote_t));
		}
	};

	static void SilhouetteHeld(const Rec &r, int num, int den)
	{
		const TargetNode *t = r.t;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(t->pos);
		GteRTPS();
		int16_t sxy[2];
		uint32_t otz;
		GteReadSXY2(sxy);
		GteReadOTZ(&otz);
		sxy[0] = (int16_t)(sxy[0] >> 3);
		sxy[1] = (int16_t)(sxy[1] >> 3);
		if ((uint16_t)otz == 0) return;
		uint8_t *entity = t->entity;
		// the colour it was drawn in
		uint32_t &colour = *(uint32_t *)(entity + 0x28);
		const uint32_t now = colour;
		colour = r.colour;
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		// a vertex pulled with |dy| clamped to 0x80 reads the distance table's row 128, i.e. the
		// bytes after it: the mote matrix whose translation the motes rewrite (0x12F9978..): the
		// draw sees them as the real one did (FrameScope puts the current ones back)
		memcpy(MOTE_Matrix->t, r.mote_t, sizeof(r.mote_t));
		DrawSilhouette(entity, sxy[0], sxy[1], r.radius, r.step);
		colour = now;
	}

	static void NodeHeld(const Rec &r, int num, int den)
	{
		switch (r.fn)
		{
		case ORIG_MoteTask:
		{
			const MoteNode &a = *(const MoteNode *)r.node;
			MoteNode m = a;
			MoteNode b = a;
			MoteUpdate(&b);
			if (b.radius > 0) m.radius = lerp16(a.radius, b.radius, num, den);
			MoteDraw(&m);
			break;
		}
		case ORIG_OrbTask:
		{
			const OrbNode &a = *(const OrbNode *)r.node;
			OrbNode m = a;
			int16_t size = r.size;
			if (r.size >= 0x200)
			{
				m.angle = lerp16(a.angle, (int16_t)(a.angle + 8), num, den);
				for (int i = 0; i < g_nmid; i++)
					if (g_mid[i].t == a.owner) size = g_mid[i].size;
			}
			OrbDraw(&m, size);
			break;
		}
		case ORIG_SparkTask: SparkDraw((const SparkNode *)r.node); break;
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		g_nmid = 0;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_BURST: BurstDraw(r.t, r.seq, r.step); break;
			case REC_SILHOUETTE: SilhouetteHeld(r, num, den); break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, VortexPartHeld, (int)&arg, num, den);
				break;
			}
			}
		}
	}
}

	static void register_mag107_held()
	{
		mark_held(demi107::ORIG_TargetTask);
		mark_held(demi107::ORIG_MoteTask);
		mark_held(demi107::ORIG_OrbTask);
		mark_held(demi107::ORIG_SparkTask);
		register_module_held(107, demi107::HeldReady, demi107::HeldFrame);
	}
}
