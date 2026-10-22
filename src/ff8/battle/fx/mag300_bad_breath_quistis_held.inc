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

// 30 fps part of mag300_bad_breath_quistis.cpp (Quistis' Bad Breath, effect 300). Exact vanilla shapes: the
// logic stays at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every task draws THEN updates. A real tick records its draws in order (the GTE state and the Field_Alloc
// scratch each started from); a held frame replays them in the same order on private packets:
//   Breath - the fade tile level, the mesh rotation angle (-0x32 per tick) and swirl phase (+0xC8), the sweep
//            point x, the length scale, the mesh colour (every channel) and the U scroll half way between the
//            tick's and the next tick's values. The mouth is a point of Quistis' ANIMATED model (bone 9,
//            0x66FC40): half way to the mouth the next tick draws from, taken with her bone matrices at her
//            NEXT pose (pose_midpoint at 1: the look-ahead of the animation reader; her real pose rebuilt after,
//            as 0x66FC40 itself rebuilds it every tick); at fraction 1 the next tick's mouth. At a restart of
//            her looped cast animation pose_midpoint holds the completed animation (as the battle model's own
//            held frames do): the predicted mouth is then the completed animation's last pose (by design).
//            The tick before the meshes end (t = 0x31): tick N's picture holds. Vertices go to a private buffer.
//   Hits   - the prim-model layout through prim::play_held (every object's exact in-between record), the
//            parameter block of the real draw (anchor fixed at the spawn, scale, the stack word +0x14 as the
//            real draw had it), a private morph buffer; the task's last draw (counter 0x24) holds.
//   Smoke  - half way up (y - 16 per tick), its flipbook frame N held; a sprite's last draw (frame 0x11) holds.
// Spawns (hit tasks, random smoke) stay at 15 Hz. No task has a draw-only mode. The module packet arena
// alternates per tick: the held packets start where the tick's first draw started. The camera is the frame
// gate's (every draw composes with it when it draws; no prediction depends on the view). The mesh frame
// 0x2507758, the GTE, the Field_Alloc scratch (the bone rebuilds of the mouth prediction included), the engine
// words the sprite draws touch, 0x1D9770C and the caster's bone matrices are put back.

namespace ff8fx
{
namespace badbreath300
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x18000;       // module packet arena per tick
	enum : uint32_t { REC_BREATH = 1, REC_PLAY = 2, REC_SMOKE = 3 };

	struct Rec
	{
		uint32_t kind;
		BreathNode breath;        // breath: the node as drawn (mouth taken, before the update)
		HitNode *hit;             // play: the live node (its layout: prim::play_held knows what it drew)
		PrimArg arg;              // play: the parameter block of the real draw
		bool next_plays;          // play: the next tick plays the layout again (else its picture holds)
		SmokeNode smoke;          // smoke: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 4 + 0x20];
	static uint8_t g_held_packets[ARENA];
	static int16_t g_held_verts[0x400];     // swirled mesh vertices (magic buffer + 0x85C)
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x85C)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
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

	static void held_note_breath(const BreathNode *e)
	{
		Rec *r = NewRec(REC_BREATH);
		if (r) r->breath = *e;
	}

	static void held_note_hit(const HitNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->hit = (HitNode *)t;
		r->arg = *arg;
		r->next_plays = t->counter + 1 < 0x25; // (the hit task ends after its draw at 0x24)
	}

	static void held_note_smoke(const SmokeNode *p)
	{
		Rec *r = NewRec(REC_SMOKE);
		if (r) r->smoke = *p;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// Held frame
	// ------------------------------------------------------------------
	// the breath's draw state
	struct MeshState
	{
		int32_t level;      // fade tile level (0x1000 = full)
		int16_t angle, phase;
		int16_t x, y, z;    // sweep point
		int16_t mouth[3];
		int32_t growth;     // length scale (0x1000 = the full length)
		uint8_t rgb[3];     // mesh colour
		uint16_t scroll;    // U scroll
	};

	// 0x700150's level at t (fade in 14, hold 22, out 14)
	static int32_t TileLevel(int32_t t)
	{
		if (t < 0xE) return shl32(t + 1, 12) / 0xE;
		if (t < 0xE + 0x16) return 0x1000;
		return (shl32(0xE - t + 0x16, 12) - 0x1000) / 0xE + 0x1000;
	}

	// the mouth the next tick draws from: 0x66FC40 with the caster's bone matrices at her NEXT pose (pose_midpoint
	// at 1), her real pose rebuilt after (the GTE and the Field_Alloc scratch: put back by HeldFrame)
	static bool g_mouth_next_ok = false;
	static int16_t g_mouth_next[3];
	static void MouthNext()
	{
		g_mouth_next_ok = false;
		uint8_t *caster = Caster();
		if (!caster || !(caster[0] & 2) || !*(uint32_t *)(caster + 0x64)) return;
		const int16_t w70c = var<int16_t>(0x1D9770C);
		pose_midpoint(caster + 0x60, caster + 0x6C, 1, 1);
		uint8_t *bones = *(uint8_t **)*(uint8_t **)(caster + 0x64) + 0x10;
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), (const Mat4x3 *)(bones + MOUTH[3] * 0x30 + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(MOUTH);
		GteMVMVA_RotV0Tr();
		int16_t out[4] = {};
		GteStoreIR123(out);
		BuildBoneMatricesFromPose(caster + 0x60);
		var<int16_t>(0x1D9770C) = w70c;
		memcpy(g_mouth_next, out, sizeof(g_mouth_next));
		g_mouth_next_ok = true;
	}

	// the breath's state num/den of the way from the tick's draw (counter c) to the next one
	static void HalfBreath(const BreathNode &e, int num, int den, MeshState &s)
	{
		const int32_t t = e.counter - 8;
		if ((uint32_t)(t + 1) >= 0x32) num = 0; // the last mesh picture holds
		s.level = lerp_i(TileLevel(t), TileLevel(t + 1), num, den);
		s.angle = lerp_angle(e.angle, (int16_t)(e.angle - 0x32), num, den);
		s.phase = lerp_angle(e.phase, (int16_t)(e.phase + 0xC8), num, den);
		s.x = (int16_t)lerp_i(SweepX(t, e.width), SweepX(t + 1, e.width), num, den);
		s.y = e.y;
		s.z = e.z;
		for (int k = 0; k < 3; k++)
			s.mouth[k] = num && g_mouth_next_ok ? (int16_t)lerp_i(e.mouth[k], g_mouth_next[k], num, den) : e.mouth[k];
		s.growth = lerp_i(Growth(t), Growth(t + 1), num, den);
		uint8_t c0[4] = {}, c1[4] = {};
		MeshColour(t, c0);
		MeshColour(t + 1, c1);
		for (int k = 0; k < 3; k++) s.rgb[k] = (uint8_t)lerp_i(c0[k], c1[k], num, den);
		s.scroll = (uint16_t)(((uint8_t)t * 10 + 10 * num / den) & 0x7F);
	}

	// the two meshes and the fade tile of a state (the breath's draw, 0x66F270)
	static void MeshPass(const MeshState &s, uint32_t verts)
	{
		uint8_t *h = AllocHeader(0x5C);
		PacketCursor() = Tile(0x40, 0x40, 0, s.level, PacketCursor());
		Mat4x3 rot;
		RotZ(s.angle, &rot);
		GteSetRotMatrix(&rot);
		GteZeroTrans();
		SwirlVertices(MESH1, s.phase, (int16_t *)verts);
		const int32_t dx = (int32_t)s.x - s.mouth[0];
		const int32_t dy = (int32_t)s.y - s.mouth[1];
		const int32_t dz = (int32_t)s.z - s.mouth[2];
		const int32_t len = Sqrt(mul32(dz, dz) + mul32(dy, dy) + mul32(dx, dx)) + 0x200;
		int16_t scale[9] = { 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, (int16_t)len };
		scale[8] = (int16_t)(mul32(scale[8], s.growth) >> 12);
		const int32_t v[3] = { dx, dy, -dz };
		Mat4x3 basis;
		BuildOrthonormalBasis(v, &basis);
		MulMatrix3(&basis, scale);
		basis.t[0] = s.mouth[0];
		basis.t[1] = s.mouth[1];
		basis.t[2] = s.mouth[2];
		ComposeAffineTransform(&Camera(), &basis, &MeshFrame());
		GteSetRotMatrix(&MeshFrame());
		GteSetTransVector(&MeshFrame());
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(uint16_t *)(h + 0x1E) = 0x100;
		h[4] = s.rgb[0];
		h[5] = s.rgb[1];
		h[6] = s.rgb[2];
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x1C) = 0x80;
		*(uint16_t *)(h + 0xC) = s.scroll;
		*(uint8_t **)h = MESH1;
		*(uint32_t *)(h + 0x20) = verts;
		PacketCursor() = MeshDraw(h, RenderOT44(), 2, PacketCursor());
		RotZ((int16_t)-s.angle, &rot);
		GteSetRotMatrix(&rot);
		GteZeroTrans();
		SwirlVertices(MESH2, (int32_t)s.phase + 0x400, (int16_t *)verts);
		GteSetRotMatrix(&MeshFrame());
		GteSetTransVector(&MeshFrame());
		*(uint8_t **)h = MESH2;
		PacketCursor() = MeshDraw(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// engine words the mouth prediction (0x1D9770C) and the sprite draws touch, the module's mesh frame
		const int16_t w70c = var<int16_t>(0x1D9770C);
		const Mat4x3 frame = MeshFrame();
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR: the square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		// the next tick's mouth (caster at her next pose: bone rebuilds on the Field_Alloc scratch, put back)
		g_mouth_next_ok = false;
		bool breath = false;
		for (int i = 0; i < g_nrec; i++)
			if (g_recs[i].kind == REC_BREATH) breath = true;
		if (breath && num > 0)
		{
			static uint8_t pose_scratch[0x8000];
			uint8_t *fa = var<uint8_t *>(0x1D999C4);
			if (fa) memcpy(pose_scratch, fa, sizeof(pose_scratch));
			MouthNext();
			if (fa) memcpy(fa, pose_scratch, sizeof(pose_scratch));
			var<uint8_t *>(0x1D999C4) = fa;
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			MeshState s;
			if (r.kind == REC_BREATH) HalfBreath(r.breath, num, den, s);
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_BREATH:
				MeshPass(s, (uint32_t)g_held_verts);
				break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)r.hit->layout, PrimCallback, (int)&arg, r.next_plays ? num : 0, den);
				break;
			}
			case REC_SMOKE:
			{
				SmokeNode p = r.smoke;
				// the smoke rises 16 per tick (its last frame holds)
				if (p.counter + 1 < 0x12) p.pos[1] = lerp_angle(p.pos[1], (int16_t)(p.pos[1] - 0x10), num, den);
				SmokeDraw(&p);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		MeshFrame() = frame;
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag300_held()
	{
		mark_held(badbreath300::ORIG_BreathTask);
		mark_held(badbreath300::ORIG_HitTask);
		mark_held(badbreath300::ORIG_SmokeTask);
		register_module_held(300, badbreath300::HeldReady, badbreath300::HeldFrame);
	}
}
