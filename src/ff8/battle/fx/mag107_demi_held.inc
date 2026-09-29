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
//                current camera, in the colour it was drawn in; its shadow is drawn again. The
//                pull (weight row and radius switch per tick) is projected twice per bone group:
//                with the real draw's step / radius / mote bytes and with the next tick's (its
//                step from the counter, the radius the real draw returned, the mote bytes the
//                tick left); the vertices go half way between the two (clip flags from the
//                in-between position), then the polygons are culled and drawn from them. The
//                last silhouette (none on the next tick) holds. Vertices go to a private copy of
//                the bone workspace, packets to a private copy of the frame arena. The pull reads
//                the mote matrix in exe data (past its distance table): each pass gets its bytes.
//   Mote       - radius half way to the next tick's (the shrink accelerates); the frame switches
//                on the real frame, its picture (same texture, only the streak changes) goes
//                half way to the next frame's (FlipDraw); the camera-facing matrix it rewrites in
//                exe data is put back.
//   Orb        - angle half way (+4), distance from the vortex's in-between size.
//   Spark, the two target sequences - fixed place, 15 Hz frames; colours (and the layout when
//                the next frame shows the same texture) half way to the next frame's (FlipDraw).
// FlipDraw: the same draw with the next frame goes to a private packet buffer and bucket table
// (0x1D8E04C pointed at a private table, the execution node it adds dropped, the cursor put back)
// and the packets just drawn are blended toward its packets (same packet layout only).
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

	// ---- flipbook frames (sprite sequences) ----
	static uint8_t g_priv_pk[0x8000];
	static uint32_t g_priv_ot[0x4000];

	// packets a (just drawn) toward packets b (the same draw with the next frame): colours always,
	// vertices when the texture words (uv, clut, page) are the same in both (a frame that only
	// changes the picture's shape); stops at the first packet whose layout differs
	static void LerpPackets(uint8_t *a, uint8_t *ae, const uint8_t *b, const uint8_t *be, int num, int den)
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
				bool pos = true;
				if (tex)
					for (uint32_t v = 0, i = 0; v < nv; v++)
					{
						i += (v == 0 || gouraud ? 1 : 0) + 1;
						uint32_t mask = v < 2 ? 0xFFFFFFFFu : 0xFFFFu; // uv (+ clut / page); the high half of the last two is unused
						if ((wa[i] ^ wb[i]) & mask) pos = false;
						i++;
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

	// draw(false) draws the frame of the tick (from the real draw's GTE state and scratch),
	// draw(true) the next frame (next: the task shows it on the next tick) privately, from the same
	// start; the first draw's packets are blended toward the second's
	template<typename Draw>
	static void FlipDraw(const Rec &r, bool next, int num, int den, Draw draw)
	{
		uint32_t c0 = PacketCursor();
		draw(false);
		if (!next || !num) return;
		uint32_t c1 = PacketCursor();
		uint32_t ot = var<uint32_t>(0x1D8E04C), exec = var<uint32_t>(0x1CA8828);
		Replay(r);
		var<uint32_t>(0x1D8E04C) = (uint32_t)g_priv_ot;
		PacketCursor() = (uint32_t)g_priv_pk;
		draw(true);
		uint32_t e = PacketCursor();
		PacketCursor() = c1;
		var<uint32_t>(0x1D8E04C) = ot;
		var<uint32_t>(0x1CA8828) = exec;
		if (e > (uint32_t)g_priv_pk && e - (uint32_t)g_priv_pk <= sizeof(g_priv_pk))
			LerpPackets((uint8_t *)c0, (uint8_t *)c1, g_priv_pk, (const uint8_t *)e, num, den);
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

	// ---- silhouette ----
	// one pull pass: the weight row, the radius and the mote bytes the pull reads past its table
	struct Pull { int32_t step; int16_t radius; const int32_t *mote_t; };
	static uint8_t g_verts_next[0x10000]; // the next tick's pull of a bone group's vertices

	static void SetPull(uint8_t *s, const Pull &p)
	{
		F<uint32_t>(s, 0x54) = PULL_Weights + (uint32_t)shl32(p.step, 8);
		F<int16_t>(s, 0x58) = p.radius;
		memcpy(MOTE_Matrix->t, p.mote_t, sizeof(MOTE_Matrix->t));
	}

	// the bone groups of one object (PullModel's loop), pulled into out; returns the end of out
	static uint8_t *PullGroups(const uint8_t *bones, uint8_t **objp, uint8_t *out, uint8_t *s)
	{
		uint8_t *obj = *objp;
		int32_t groups = *(const int16_t *)obj;
		obj += 2;
		if (groups > 0)
		{
			do
			{
				const int32_t bone = *(const int16_t *)obj;
				obj += 2;
				const Mat4x3 *bm = (const Mat4x3 *)(bones + bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(bm);
				GteSetTransVectorCtrl(bm);
				PullVertices(&obj, &out, s);
			} while (--groups != 0);
		}
		*objp = obj;
		return out;
	}

	// PullModel with every vertex half way between the tick's pull (a) and the next tick's (b,
	// nullptr: none): screen position and depth in between, clip flags from the in-between position;
	// a vertex behind the camera in either pass keeps the tick's
	static uint32_t PullModelHeld(const uint8_t *model, uint8_t *s, uint32_t ot, uint32_t cursor, const Pull &a, const Pull *b, int num, int den)
	{
		const uint8_t *table = *(uint8_t *const *)(model + 4);
		const uint8_t *bones = *(uint8_t *const *)model + 0x10;
		const int32_t count = *(const int32_t *)table;
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t i = 0; i < count; i++)
		{
			uint8_t *obj = (uint8_t *)(*(uint8_t *const *)(model + 4) + offs[i]);
			if (!(F<uint32_t>(s, 0x20) & (1u << (i & 31)))) continue;
			uint8_t *out = F<uint8_t *>(s, 4);
			uint8_t *obj_b = obj;
			if (b)
			{
				SetPull(s, *b);
				PullGroups(bones, &obj_b, g_verts_next, s);
			}
			SetPull(s, a);
			uint8_t *end = PullGroups(bones, &obj, out, s);
			if (b && num)
			{
				uint32_t n = (uint32_t)(end - out) / 8;
				if (n > sizeof(g_verts_next) / 8) n = sizeof(g_verts_next) / 8;
				for (uint32_t k = 0; k < n; k++)
				{
					uint8_t *va = out + k * 8;
					const uint8_t *vb = g_verts_next + k * 8;
					if ((va[6] | vb[6]) & 0x10) continue;
					int16_t *pa = (int16_t *)va;
					const int16_t *pb = (const int16_t *)vb;
					pa[0] = (int16_t)lerp_i(pa[0], pb[0], num, den);
					pa[1] = (int16_t)lerp_i(pa[1], pb[1], num, den);
					*(uint16_t *)(va + 4) = (uint16_t)lerp_i(*(const uint16_t *)(va + 4), *(const uint16_t *)(vb + 4), num, den);
					const int16_t sx = (int16_t)(pa[0] >> 3), sy = (int16_t)(pa[1] >> 3);
					uint8_t f = 0;
					if (sx < F<int16_t>(s, 0x14)) f |= 1;
					else if (sx >= F<int16_t>(s, 0x18)) f |= 2;
					if (sy < F<int16_t>(s, 0x16)) f |= 4;
					else if (sy >= F<int16_t>(s, 0x1A)) f |= 8;
					va[6] = f;
				}
			}
			obj = (uint8_t *)(((uintptr_t)obj + 3) & ~(uintptr_t)3);
			for (int w = 0; w < 6; w++)
			{
				F<uint16_t>(s, 8 + 2 * w) = *(const uint16_t *)obj;
				obj += 2;
			}
			F<uint8_t *>(s, 0) = obj;
			cursor = FlatPolygons(s, ot, 0, cursor);
		}
		return cursor;
	}

	// DrawSilhouette with PullModelHeld
	static void DrawSilhouetteHeld(uint8_t *entity, int16_t sx, int16_t sy, const Pull &a, const Pull *b, int num, int den)
	{
		uint8_t *c = (uint8_t *)FieldAlloc(0x80);
		uint8_t *s = c + 0x20;
		F<int16_t>(c, 0x7A) = sx;
		F<int16_t>(c, 0x7C) = sy;
		F<uint32_t>(c, 0x74) = PULL_Weights + (uint32_t)shl32(a.step, 8);
		F<int16_t>(c, 0x78) = a.radius;
		F<int16_t>(c, 0x7E) = 0;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)c);
		FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
		ComputeBonesWorldMatrices(entity + 0x60, c);
		F<int16_t>(c, 0x34) = 0;
		F<uint32_t>(c, 0x24) = BoneWorkspace();
		F<int16_t>(c, 0x36) = 0;
		F<int16_t>(c, 0x38) = 0x140;
		F<int16_t>(c, 0x3A) = 0xD8;
		F<uint32_t>(c, 0x3C) = *(const uint32_t *)(entity + 0x28);
		F<uint32_t>(c, 0x40) = *(const uint32_t *)(entity + 0x7C);
		FrameCursor() = PullModelHeld(*(const uint8_t *const *)(entity + 0x64), s, RenderOT(0x44), FrameCursor(), a, b, num, den);
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *second = *(uint8_t *const *)(entity + 0x78);
		if (second)
		{
			F<uint32_t>(c, 0x40) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(second, c);
			FrameCursor() = PullModelHeld(*(const uint8_t *const *)(second + 4), s, RenderOT(0x44), FrameCursor(), a, b, num, den);
			BuildBoneMatricesFromPose(second);
		}
		FieldFree(0x80);
	}

	static void SilhouetteHeld(const Rec &r, const int32_t *mote_now, int num, int den)
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
		// tick's pass sees them as the real draw did, the next tick's as this tick left them (the
		// target tasks run before the motes; FrameScope puts the current ones back)
		const Pull a = { r.step, r.radius, r.mote_t };
		// the next tick's silhouette: the counter has moved on, the radius is the one the real
		// draw returned
		const uint32_t ps = (uint32_t)(t->counter - 0x1E);
		const Pull b = { ps < 0x3C ? PullStep(ps) : 0, t->radius, mote_now };
		DrawSilhouetteHeld(entity, sxy[0], sxy[1], a, ps < 0x3C ? &b : nullptr, num, den);
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
			const bool alive = b.radius > 0; // it ends after this draw otherwise: its last picture holds
			if (alive) m.radius = lerp16(a.radius, b.radius, num, den);
			FlipDraw(r, alive, num, den, [&](bool next)
			{
				MoteNode d = m;
				if (next) d.frame = (int16_t)(d.frame + 1 >= 0xB ? 0 : d.frame + 1);
				MoteDraw(&d);
			});
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
		case ORIG_SparkTask:
		{
			const SparkNode &a = *(const SparkNode *)r.node;
			FlipDraw(r, a.frame + 1 < a.end, num, den, [&](bool next)
			{
				SparkNode d = a;
				if (next) d.frame++;
				SparkDraw(&d);
			});
			break;
		}
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
			case REC_BURST:
				// steps 0..5, one per tick
				FlipDraw(r, r.step + 1 < 6, num, den, [&](bool next) { BurstDraw(r.t, r.seq, r.step + (next ? 1 : 0)); });
				break;
			case REC_SILHOUETTE: SilhouetteHeld(r, frame.mote_t, num, den); break;
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
