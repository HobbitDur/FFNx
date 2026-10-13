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

// 30 fps part of mag129_breath.cpp (Breath, effect 129). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (the main task's parts are drawn before its counter / angles move).
// A real tick records its draws in order (the GTE state and Field_Alloc scratch each started from);
// a held frame replays them in the same order on private packets:
//   Mouths   - the three prim-model layouts at the caster's bone 9 through prim::play_held: every
//              object's exact in-between record; the anchor = bone 9 at the caster's pose midpoint
//              (pose_midpoint + GetEffectSpawnPosition, the bones put back). The looping layout that
//              ends on the tick (decoded again) holds its last frame.
//   Mesh     - rotation angle1 / swirl phase angle2 half way (-100 / +200 per tick), the walk point
//              half way along the targets' path (bone 0xF1 at their pose midpoints; past a target the
//              next segment), bone 9 at the pose midpoint, fade and V scroll half way; vertices go to
//              a private buffer. Ends (no mesh on the next tick): tick N's picture holds.
//   Branches - each chain as the tick drew it (growth and retraction stay 15 Hz batches), every
//              segment's brightness half way to the next tick's fade step, drawn with the half-way
//              rotation / swirl and the half-way mesh frame (tick N's frame when the next tick does
//              not rebuild it), from the direction slot value the real draw read.
//   Arcs     - progress and colours half way to the next tick's (fade after completion), the jitter
//              of the real tick (its random values), the skinned surface at the entity's pose
//              midpoint; an arc that ends on the tick holds its picture; direction slot as drawn.
//   Targets  - the prim-model layout through prim::play_held (anchor fixed at the spawn).
// A layout the next tick no longer plays (mouths after their counters, a target task's last draw) holds its picture.
//   Sprites  - flipbook frame and position hold (fixed sparks).
// Spawns (random) stay at 15 Hz. No task has a draw-only mode. The module packet arena alternates
// per tick: the held packets start where the tick's first draw started. The camera is the frame
// gate's. Module globals, the magic buffer, the GTE, the Field_Alloc scratch, the engine words the
// anchors and sprites touch and the entities' bone matrices are put back.

namespace ff8fx
{
namespace breath129
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PLAY = 1, REC_MESH = 2, REC_BRANCH = 3, REC_ARC = 4, REC_SPRITE = 5 };

	struct Rec
	{
		uint32_t kind;
		prim::Layout *layout;     // play
		int play;
		bool restart;             // play: the layout ended on the tick and was decoded again (its played state kept)
		bool next_plays;          // play: the next tick plays the layout again (else its picture holds)
		uint8_t played[0x68];
		int16_t tpos[3], tangle;  // play (target): the node's anchor and angle
		int first, n;             // branch: its segments (as drawn) in g_segs
		int16_t count;
		uint32_t slot;            // branch: the direction slot the draw read
		int16_t angle1, angle2;   // branch: the main task's rotation / swirl phase
		ArcNode arc;              // arc: the node as drawn
		int32_t rnd[14][3];       // arc: its jitter
		uint32_t dir;             // arc: the direction slot (the curve's Field_Alloc block)
		SpriteNode sprite;        // sprite: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct MeshMemo
	{
		bool on;
		int16_t pos[3], angle1, angle2, walk, step, cur, next, count;
		int32_t c1e;
		uint8_t *ent[3];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nsegs = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[0x80];
	static Rec *g_arc_rec = nullptr;
	static MeshMemo g_mesh;
	static BoltSeg g_segs[0x400];           // branch chains as drawn
	static BoltSeg g_copy[0x100];           // held chain
	static uint8_t g_held_packets[0x18000]; // module arena (0x18000 per tick)
	static int16_t g_held_verts[0x400];     // mesh vertices (magic buffer + 0x35F0)
	static uint8_t g_held_morph[0x800];     // blended vertex frames (magic buffer + 0x35F0)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nsegs = 0;
		g_packets = 0;
		g_arc_rec = nullptr;
		g_mesh.on = false;
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

	static void held_note_play(prim::Layout *l, int kind, const TargetNode *t)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = l;
		r->play = kind;
		r->restart = false;
		if (t)
		{
			memcpy(r->tpos, t->pos, sizeof(r->tpos));
			r->tangle = t->angle;
			r->next_plays = t->counter + 1 < 0x18; // (the target task ends after its draw at 0x17)
		}
		else
		{
			// the mouths: counters 0x1C..0x48, the third one 0x1C..0x3C
			const int32_t c1c = ((const MainNode *)(TexBase() + 0x1690))->counter - 0x1C;
			r->next_plays = kind == PLAY_MOUTH3 ? (uint32_t)(c1c + 1) <= 0x20 : (uint32_t)(c1c + 1) < 0x2D;
		}
	}

	// the looping mouth layout ended on the tick (play returned 0): its state as played, before the new decode
	static void held_note_restart(prim::Layout *l)
	{
		for (int i = g_nrec - 1; i >= 0; i--)
			if (g_recs[i].kind == REC_PLAY && g_recs[i].layout == l)
			{
				g_recs[i].restart = true;
				memcpy(g_recs[i].played, l, sizeof(g_recs[i].played));
				return;
			}
	}

	// the main task's state as the mesh is drawn (walk before the tick's step)
	static void held_note_mesh(const MainNode *m, int32_t c1e)
	{
		if (!NewRec(REC_MESH)) return;
		g_mesh.on = true;
		memcpy(g_mesh.pos, m->pos, sizeof(g_mesh.pos));
		g_mesh.angle1 = m->angle1;
		g_mesh.angle2 = m->angle2;
		g_mesh.walk = m->walk;
		g_mesh.step = m->step;
		g_mesh.cur = m->cur;
		g_mesh.next = m->next;
		g_mesh.count = m->count;
		g_mesh.c1e = c1e;
		memcpy(g_mesh.ent, m->ent, sizeof(g_mesh.ent));
	}

	static void held_note_sprite(const SpriteNode *p)
	{
		Rec *r = NewRec(REC_SPRITE);
		if (r) r->sprite = *p;
	}

	static void held_note_branch(const BranchNode *b, uint32_t slot)
	{
		Rec *r = NewRec(REC_BRANCH);
		if (!r) return;
		r->first = g_nsegs;
		r->n = 0;
		r->count = b->count;
		r->slot = slot;
		const MainNode *m = (const MainNode *)(TexBase() + 0x1690);
		r->angle1 = m->angle1;
		r->angle2 = m->angle2;
		for (const BoltSeg *s = b->head; s; s = s->next)
		{
			if (g_nsegs >= (int)(sizeof(g_segs) / sizeof(g_segs[0])))
			{
				g_nrec--;
				return;
			}
			g_segs[g_nsegs++] = *s;
			r->n++;
		}
	}

	static void held_note_arc(const ArcNode *arc)
	{
		Rec *r = NewRec(REC_ARC);
		g_arc_rec = r;
		if (!r) return;
		r->arc = *arc;
		r->dir = var<uint32_t>(0x1D999C4); // the curve's Field_Alloc block (allocated first)
		memset(r->rnd, 0, sizeof(r->rnd));
	}

	static void held_note_arc_jitter(int i, const int32_t *rv)
	{
		if (g_arc_rec) memcpy(g_arc_rec->rnd[i - 1], rv, sizeof(g_arc_rec->rnd[i - 1]));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// Held frame
	// ------------------------------------------------------------------
	// position of an entity's bone at its pose num/den of the way to the next tick (the bones put back)
	static void Anchor(uint8_t *e, int32_t bone, int32_t flag, int num, int den, int16_t *out)
	{
		pose_midpoint(e + 0x60, e + 0x6C, num, den);
		int16_t sp[4] = {};
		GetEffectSpawnPosition(e, bone, flag, sp);
		BuildBoneMatricesFromPose(e + 0x60);
		out[0] = sp[0];
		out[1] = sp[1];
		out[2] = sp[2];
	}

	static int32_t FadeOf(int32_t c1e)
	{
		uint8_t h[0x10] = {};
		MeshFade(h, c1e);
		return *(const int32_t *)(h + 8);
	}

	// the main task's half-way state
	struct HalfMain
	{
		bool caster;          // pos valid
		int16_t pos[3];       // bone 9 at the caster's pose midpoint
		int16_t pos0[3];      // bone 9 as the tick drew it (layouts the next tick no longer plays hold)
		bool mesh;            // the mesh was drawn on the tick
		bool frame_ok;        // the branch frame is rebuilt (the next tick draws the mesh too)
		int mnum, mden;       // the mesh's fraction (0: holds tick N's)
		int16_t mpos[3], x, y, z, angle1, angle2;
		int32_t fade;
		uint16_t scroll;
		Mat4x3 frame;
	};

	static void ComputeHalfMain(HalfMain &hm, int num, int den)
	{
		hm.caster = false;
		hm.mesh = g_mesh.on;
		hm.frame_ok = false;
		bool need_caster = g_mesh.on;
		for (int i = 0; i < g_nrec && !need_caster; i++)
			if (g_recs[i].kind == REC_PLAY && g_recs[i].play != PLAY_TARGET) need_caster = true;
		if (need_caster && Caster())
		{
			Anchor(Caster(), 9, 0x2000, num, den, hm.pos);
			Anchor(Caster(), 9, 0x2000, 0, 1, hm.pos0);
			hm.caster = true;
		}
		if (!g_mesh.on) return;
		const MeshMemo &m = g_mesh;
		const bool next_mesh = (uint32_t)(m.c1e + 1) < 0x2B;
		hm.mnum = next_mesh ? num : 0;
		hm.mden = den;
		memcpy(hm.mpos, next_mesh ? hm.pos : hm.pos0, sizeof(hm.mpos));
		// the walk: the tick's step half way; past the target the next segment of the path
		int32_t w = m.walk + ((uint32_t)m.c1e >= 5 ? (int32_t)m.step * hm.mnum / den : 0);
		int16_t cur = m.cur, nx = m.next;
		if (w >= 0x1000)
		{
			w -= 0x1000;
			cur = nx;
			nx = (int16_t)(nx + 1);
			if (nx >= m.count) nx = (int16_t)(m.count - 1);
		}
		int16_t from[3], to[3];
		Anchor(m.ent[cur], 0xF1, 0, hm.mnum, den, from);
		Anchor(m.ent[nx], 0xF1, 0, hm.mnum, den, to);
		hm.x = (int16_t)(from[0] + ((((int32_t)to[0] - from[0]) * w) >> 12));
		hm.y = (int16_t)(from[1] + ((((int32_t)to[1] - from[1]) * w) >> 12));
		hm.z = (int16_t)(from[2] + ((((int32_t)to[2] - from[2]) * w) >> 12));
		hm.angle1 = (int16_t)(m.angle1 - 100 * hm.mnum / den);
		hm.angle2 = (int16_t)(m.angle2 + 200 * hm.mnum / den);
		const int32_t f0 = FadeOf(m.c1e);
		hm.fade = lerp_i(f0, next_mesh ? FadeOf(m.c1e + 1) : f0, hm.mnum, den);
		hm.scroll = (uint16_t)((((m.c1e & 7) << 4) + 16 * hm.mnum / den) & 0x7F);
		hm.frame_ok = next_mesh;
	}

	static void MeshHeld(HalfMain &hm)
	{
		uint8_t *h = AllocHeader(0x5C);
		MeshSwirl(hm.angle1, hm.angle2, g_held_verts);
		*(int32_t *)(h + 8) = hm.fade;
		if (hm.fade) *(int32_t *)(h + 4) = 0;
		MeshDraw(h, hm.mpos, hm.x, hm.y, hm.z, hm.scroll, &hm.frame, (uint32_t)g_held_verts);
		FieldFree(0x5C);
	}

	static void PlayHeld(const Rec &r, const HalfMain &hm, int num, int den)
	{
		if (!r.next_plays) num = 0; // the last picture holds
		PrimArg arg;
		if (r.play == PLAY_TARGET)
		{
			RotY(r.tangle, &arg.m);
			arg.m.t[0] = r.tpos[0];
			arg.m.t[1] = r.tpos[1];
			arg.m.t[2] = r.tpos[2];
			arg.depth = 0;
		}
		else
		{
			if (!hm.caster) return;
			if (r.play == PLAY_MOUTH3) RotY(*(int16_t *)(Caster() + 0xE), &arg.m);
			else
			{
				arg.m.m[0][0] = 0x1000; arg.m.m[0][1] = 0; arg.m.m[0][2] = 0;
				arg.m.m[1][0] = 0; arg.m.m[1][1] = 0x1000; arg.m.m[1][2] = 0;
				arg.m.m[2][0] = 0; arg.m.m[2][1] = 0; arg.m.m[2][2] = 0x1000;
				arg.m.pad = 0;
			}
			const int16_t *pos = r.next_plays ? hm.pos : hm.pos0;
			arg.m.t[0] = pos[0];
			arg.m.t[1] = pos[1];
			arg.m.t[2] = pos[2];
			arg.depth = r.play == PLAY_MOUTH3 ? 0 : -0x80;
		}
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		if (r.restart)
		{
			// the layout as the tick played it (its last frame: it holds), then the new decode back
			uint8_t live[sizeof(r.played)];
			memcpy(live, r.layout, sizeof(live));
			memcpy(r.layout, r.played, sizeof(live));
			prim::play_held(r.layout, PrimCallback, (int)&arg, num, den);
			memcpy(r.layout, live, sizeof(live));
		}
		else prim::play_held(r.layout, PrimCallback, (int)&arg, num, den);
	}

	static void BranchHeld(const Rec &r, const HalfMain &hm, int num, int den)
	{
		const int n = r.n;
		if (n < 2 || n > (int)(sizeof(g_copy) / sizeof(g_copy[0]))) return;
		for (int i = 0; i < n; i++)
		{
			const BoltSeg &s = g_segs[r.first + i];
			BoltSeg &c = g_copy[i];
			c = s;
			// the next tick's fade step (before its draw)
			int16_t next = s.intensity;
			if (next > 0)
			{
				next = (int16_t)(next + s.decay);
				if (next <= 0) next = 0;
			}
			c.intensity = lerp16(s.intensity, next, num, den);
			c.next = i + 1 < n ? &g_copy[i + 1] : nullptr;
		}
		const Mat4x3 rot = BranchRot(), frame = BranchFrame();
		RotZ((int16_t)-lerp16(r.angle1, (int16_t)(r.angle1 - 100), num, den), &BranchRot());
		if (hm.mesh && hm.frame_ok) BranchFrame() = hm.frame;
		BranchDraw(g_copy, lerp16(r.angle2, (int16_t)(r.angle2 + 200), num, den), r.count, r.slot);
		BranchFrame() = frame;
		BranchRot() = rot;
	}

	// the entity whose model an arc rides
	static uint8_t *ArcEntity(uint8_t **bones)
	{
		for (int i = 0; i < 8; i++)
			if (*(uint8_t ***)(Entity(i) + 0x64) == bones) return Entity(i);
		return nullptr;
	}

	static void ArcHeld(const Rec &r, int num, int den)
	{
		ArcNode a = r.arc;
		int16_t prog1 = (int16_t)(a.progress + a.speed);
		const bool done = prog1 > 0x1000;
		if (done) prog1 = 0x1000;
		if (!(done && a.age + 1 >= 10)) // (an arc that ends on the tick holds its picture)
		{
			for (int k = 0; k < 16; k++)
			{
				int16_t c1 = a.pts[k].color;
				if (done)
				{
					c1 = (int16_t)(c1 - (int16_t)((0x1400 - Profile()[k]) >> a.fade_shift));
					if (c1 < 0) c1 = 0;
				}
				a.pts[k].color = lerp16(a.pts[k].color, c1, num, den);
			}
			a.progress = lerp16(a.progress, prog1, num, den);
		}
		int32_t length;
		ArcSeg *pts = ArcCurve(&a, a.progress, &length);
		for (int i = 1; i < 15; i++)
		{
			int32_t amp = mul32(Profile()[i], length) >> 12;
			pts[i].sx += (int16_t)(mul32(r.rnd[i - 1][0], amp) >> 20);
			pts[i].sy += (int16_t)(mul32(r.rnd[i - 1][1], amp) >> 20);
			pts[i].otz += (int16_t)(mul32(r.rnd[i - 1][2], amp) >> 20);
		}
		const int16_t dir0[2] = { (int16_t)r.dir, (int16_t)(r.dir >> 16) };
		ArcDraw(pts, dir0);
		FieldFree(0x100);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// engine words the anchors (0x1D9770C) and the sprite draws touch
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR: the arcs' square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		HalfMain hm;
		ComputeHalfMain(hm, num, den);
		uint8_t *posed[8];
		int nposed = 0;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			if (r.kind == REC_ARC)
			{
				// the arcs read the entity's world bone matrices (the master composes them before the
				// arc queue): the pose num/den of the way, composed once per entity
				uint8_t *e = ArcEntity(r.arc.bones);
				bool have = false;
				for (int k = 0; k < nposed; k++) have |= posed[k] == e;
				if (e && !have && nposed < 8)
				{
					posed[nposed++] = e;
					pose_midpoint(e + 0x60, e + 0x6C, num, den);
					ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
				}
			}
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PLAY: PlayHeld(r, hm, num, den); break;
			case REC_MESH: if (hm.mesh) MeshHeld(hm); break;
			case REC_BRANCH: BranchHeld(r, hm, num, den); break;
			case REC_ARC: ArcHeld(r, num, den); break;
			case REC_SPRITE: SpriteDraw(&r.sprite); break;
			}
		}
		for (int k = 0; k < nposed; k++) BuildBoneMatricesFromPose(posed[k] + 0x60);
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag129_held()
	{
		mark_held(breath129::ORIG_MainTask);
		mark_held(breath129::ORIG_TargetTask);
		mark_held(breath129::ORIG_SpriteTask);
		mark_held(breath129::ORIG_BranchTask);
		mark_held(breath129::ORIG_ArcTask);
		register_module_held(129, breath129::HeldReady, breath129::HeldFrame);
	}
}
