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

// 30 fps part of mag136_oil_shot.cpp (Oil Shot, effect 136). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates (a layout advances in its play, the shot counter after its draws, a
// sprite / drop after its draw). A real tick records its draws in order with the GTE state and the
// Field_Alloc scratch each one started from; a held frame replays them in the same order on private
// packets, with the current camera (the frame gate's), each layout through prim::play_held (every
// object's exact in-between record):
//   Mouth layout - the caster's mouth (bone 27 through 0x11F81F0) at the caster's pose midpoint
//                  (pose_midpoint, the bones rebuilt from the pose afterwards), the facing frame.
//   Oil mesh     - the stream's state at counter + num/den: progress s, its sine, the RotZ angle, the
//                  wobble, the point P and the previous point (the state one counter earlier), the z
//                  scale and the U scroll, all from the same formulas at the in-between counter (the
//                  exe matrix word 0x11F81E0 put back).
//   Hit / pool layouts - the target point (fixed).
//   Stream drops - their position half way to the next tick's (linear motion).
//   Falling drops - their position half way to the next tick's (after the gravity step); the tick a
//                  drop lands (its splash layout replaces the drop's) the drop holds its picture (the
//                  drop layout as played, put back in the node for the draw).
//   Sprays       - position and rotation half way to the next tick's; flipbook frame N held.
//   Splats       - fixed position, flipbook frame N held.
// A part the next tick no longer draws (its counter range, its layout or its task ended on the tick;
// every part on the tick the shot ends: the master ends with it) holds its picture (state N: records
// at N, pose N). Spawns (CRT rand) stay at 15 Hz. No task has a
// draw-only mode. The module packet arena alternates per tick: the held packets start where the
// tick's first draw started; the vertex / blend buffer (magic buffer + 0x3E0C) lies 0x360 bytes
// before arena A (+0x416C): on arena-A ticks the held buffer sits 0x360 bytes before the held packets,
// as in the game. The GTE, the Field_Alloc scratch, the exe word 0x11F81E0, the engine words the
// sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace oil136
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = ARENA_B - ARENA_A;   // module packet arena per tick
	static const uint32_t VERTS_GAP = ARENA_A - VERTS; // vertex / blend buffer -> arena A
	static const uint32_t FALL_LAYOUT = 0x50;          // a falling drop's layout bytes

	struct Rec
	{
		int kind;                 // DrawKind
		TaskNode *node;
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int16_t counter;          // shot: the counter as drawn; sprites: the frame
		int16_t pos[4];           // mouth / drops / sprites: the position as drawn
		int16_t angle;            // spray: the rotation as drawn
		int16_t landed;           // falling drop: its flag as drawn
		bool landing;             // falling drop: landed after this draw (its drop layout below)
		uint8_t drop[FALL_LAYOUT];
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[4 + 8 + 0x10 + 8 + 0x10 + 8];
	static uint8_t g_held_arena[VERTS_GAP + ARENA]; // [vertex / blend buffer][packets] (arena A layout)
	static uint8_t g_held_verts[0x1000];    // vertex / blend buffer on arena-B ticks

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, TaskNode *n, int16_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.layout = nullptr;
		r.counter = counter;
		memset(r.pos, 0, sizeof(r.pos));
		r.angle = 0;
		r.landed = 0;
		r.landing = false;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a layout is about to play: the position it plays at (shot layouts: the mouth / the target point)
	static void held_note_play(int kind, TaskNode *n, prim::Layout *l, const int16_t *pos)
	{
		Rec *r = NewRec(kind, n, kind == PLAY_STREAM || kind == PLAY_FALL ? 0 : ((const ShotNode *)n)->counter);
		if (!r) return;
		r->layout = l;
		memcpy(r->pos, pos, 6);
		if (kind == PLAY_FALL) r->landed = ((const FallNode *)n)->landed;
	}

	// a falling drop lands: its drop layout as played this tick (the splash layout replaces it next)
	static void held_note_landing(TaskNode *n)
	{
		for (int i = g_nrec - 1; i >= 0; i--)
		{
			Rec &r = g_recs[i];
			if (r.kind != PLAY_FALL || r.node != n) continue;
			r.landing = true;
			memcpy(r.drop, ((const FallNode *)n)->layout, FALL_LAYOUT);
			break;
		}
	}

	static void held_note_mesh(TaskNode *n) { NewRec(DRAW_MESH, n, ((const ShotNode *)n)->counter); }

	static void held_note_sprite(int kind, TaskNode *n)
	{
		if (kind == SPR_SPRAY)
		{
			const SprayNode *s = (const SprayNode *)n;
			Rec *r = NewRec(kind, n, s->frame);
			if (r)
			{
				memcpy(r->pos, s->pos, 6);
				r->angle = s->angle;
			}
		}
		else
		{
			const SplatNode *s = (const SplatNode *)n;
			Rec *r = NewRec(kind, n, s->frame);
			if (r) memcpy(r->pos, s->pos, 6);
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static bool Alive(const Rec &r) { return (r.node->flags & 1) != 0; }
	// the layout has frames left after this tick's play (it draws again on the next tick)
	static bool LayoutLive(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}
	static bool MouthDraws(int32_t c) { return (uint32_t)(c - 0x1E) < 8; }
	static bool MeshDraws(int32_t c) { return (uint32_t)(c - 0x1F) < 0xC; }
	static bool HitDraws(int32_t c) { return (uint32_t)(c - 0x26) < 0x1A; }
	static bool PoolDraws(int32_t c) { return (uint32_t)(c - 0x2C) < 0x26; }

	// BonePoint (0x6AEE20) on the bone matrices as they are (no rebuild from the pose)
	static void BonePointNow(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		const uint32_t bones = **(const uint32_t *const *)(c + 0x64) + 0x10;
		Mat4x3 m;
		const int32_t bone = v[3];
		ComposeAffineTransform((const Mat4x3 *)(Caster() + 0x40), (const Mat4x3 *)(bones + (uint32_t)bone * 0x30 + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// the mouth at pose k/den
	static void MouthAt(int k, int den, int16_t *P)
	{
		uint8_t *c = Caster();
		pose_midpoint(c + 0x60, c + 0x6C, k, den);
		BonePointNow(MOUTH, P);
		BuildBoneMatricesFromPose(c + 0x60);
	}

	// the stream at counter k + num/den (k = counter - 0x1E): progress, its sine, the point
	static int32_t HeldS(int32_t k, int num, int den) { return (int32_t)(((int64_t)(k * den + num) << 12) / (12 * den)); }

	struct HeldMesh
	{
		int32_t angle;     // RotZ
		int32_t sine;      // wobble amplitude, sin(s / 2)
		int16_t P[4];
		Mat4x3 M;          // basis scaled in z, at P (before the camera)
		uint16_t u;        // U scroll
	};
	static void MeshAt(const ShotNode *w, int32_t k, int num, int den, HeldMesh *o)
	{
		const int32_t s = HeldS(k, num, den);
		o->sine = ComputeSin(s >> 1);
		o->angle = (int32_t)(((int64_t)(k * den + num) << 8) / den);
		o->u = (uint16_t)((((k * den + num) << 4) / den) & 0x7F);
		const int32_t sp = HeldS(k - 1, num, den);
		int16_t prev[4] = {};
		StreamPoint(w, sp, ComputeSin(sp >> 1), prev);
		memset(o->P, 0, sizeof(o->P));
		StreamPoint(w, s, o->sine, o->P);
		StreamBasis(o->P, prev, &o->M);
		const int16_t z = ScaleZ();
		ScaleZ() = MeshScaleZ(s, o->sine);
		MulMatrix3(&o->M, SCALE_Z);
		ScaleZ() = z;
		o->M.t[0] = o->P[0];
		o->M.t[1] = o->P[1];
		o->M.t[2] = o->P[2];
	}

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldDraw(int num, int den)
	{
		// the shot ended on this tick (its queue is empty): the master ends and no task draws again, every part holds
		if (!QShot()->head) num = 0;
		// arena A (magic buffer + 0x416C) follows the vertex / blend buffer (+ 0x3E0C) at 0x360 bytes
		const bool arena_a = g_packets == TexBase() + ARENA_A;
		const uint32_t verts = arena_a ? (uint32_t)g_held_arena : (uint32_t)g_held_verts;
		HeldScope scope(PacketCursor(), g_held_arena + VERTS_GAP, ARENA, g_packets);
		// GTE data registers 28..31 (square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch, the exe matrix word
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		const int16_t scale_z = ScaleZ();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			PrimArg arg = {};
			arg.morph = verts;
			int k = num;
			switch (r.kind)
			{
			case SPR_SPRAY:
			{
				int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
				int16_t angle = r.angle;
				const SprayNode *s = (const SprayNode *)r.node;
				if (Alive(r))
				{
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], s->pos[j], num, den);
					angle = lerp16(r.angle, s->angle, num, den);
				}
				Replay(r);
				SprayDraw(pos, s->scale, r.counter, angle);
				continue;
			}
			case SPR_SPLAT:
				Replay(r);
				SplatDraw(r.pos, r.counter);
				continue;
			case DRAW_MESH:
			{
				const ShotNode *w = (const ShotNode *)r.node;
				const int32_t kk = r.counter - 0x1E;
				HeldMesh o;
				MeshAt(w, kk, MeshDraws(r.counter + 1) ? num : 0, den, &o);
				Mat4x3 Z;
				RotZ(o.angle & 0xFFFF, &Z);
				GteSetRotMatrix(&Z);
				GteZeroTrans();
				WobbleMesh(MESH, o.sine, (int16_t *)verts);
				Replay(r);
				MeshRender(&o.M, o.u, verts);
				continue;
			}
			case PLAY_MOUTH:
			{
				k = MouthDraws(r.counter + 1) && LayoutLive(r.layout) ? num : 0;
				int16_t P[4] = { r.pos[0], r.pos[1], r.pos[2], 0 };
				if (k) MouthAt(k, den, P);
				Mat4x3 M;
				RotXY(*(const int16_t *)(Caster() + 0xE), -0x200, &M);
				M.t[0] = P[0];
				M.t[1] = P[1];
				M.t[2] = P[2];
				ComposeAffineTransform(&Camera(), &M, &M);
				arg.m = M;
				arg.depth = -0x80;
				arg.mode = 0;
				break;
			}
			case PLAY_HIT:
				k = HitDraws(r.counter + 1) ? num : 0;
				BlockAt(&arg, ((const ShotNode *)r.node)->pos);
				arg.depth = -0x80;
				arg.mode = 1;
				break;
			case PLAY_POOL:
			{
				k = PoolDraws(r.counter + 1) ? num : 0;
				const ShotNode *w = (const ShotNode *)r.node;
				const int16_t p[4] = { w->pos[0], (int16_t)(w->pos[1] - 0xFA), w->pos[2], 0 };
				BlockAt(&arg, p);
				arg.depth = 0;
				arg.mode = 1;
				break;
			}
			case PLAY_STREAM:
			{
				int16_t pos[4] = { r.pos[0], r.pos[1], r.pos[2], 0 };
				if (Alive(r))
				{
					const StreamNode *d = (const StreamNode *)r.node;
					for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], d->pos[j], num, den);
				}
				else k = 0;
				BlockAt(&arg, pos);
				arg.depth = 0;
				arg.mode = 2;
				break;
			}
			case PLAY_FALL:
			{
				int16_t pos[4] = { r.pos[0], r.pos[1], r.pos[2], r.landed };
				const FallNode *d = (const FallNode *)r.node;
				if (!Alive(r) || r.landing) k = 0;
				else for (int j = 0; j < 3; j++) pos[j] = lerp16(r.pos[j], d->pos[j], num, den);
				BlockAt(&arg, pos);
				arg.depth = 0;
				arg.mode = r.landed != 0 ? 1 : 2;
				break;
			}
			}
			Replay(r);
			if (r.kind == PLAY_FALL && r.landing)
			{
				// the drop layout as played this tick, in its node for the draw
				uint8_t splash[FALL_LAYOUT];
				memcpy(splash, r.layout, FALL_LAYOUT);
				memcpy(r.layout, r.drop, FALL_LAYOUT);
				prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
				memcpy(r.layout, splash, FALL_LAYOUT);
				continue;
			}
			prim::play_held(r.layout, PrimCallback, (int)&arg, k, den);
		}
		ScaleZ() = scale_z;
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag136_held()
	{
		mark_held(oil136::ORIG_ShotTask);
		mark_held(oil136::ORIG_SprayTask);
		mark_held(oil136::ORIG_StreamTask);
		mark_held(oil136::ORIG_SplatTask);
		mark_held(oil136::ORIG_FallTask);
		register_module_held(136, oil136::HeldReady, oil136::HeldDraw);
	}
}
