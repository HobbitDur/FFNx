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

// 30 fps part of mag131_bad_breath.cpp (Bad Breath, effect 131). Exact vanilla shapes: the logic
// stays at 15 Hz, a held frame draws the state num/den of the way from the last real tick to the
// next one.
//
// Every task draws THEN updates. A real tick records its draws in order (the GTE state and the
// Field_Alloc scratch each started from); a held frame replays them in the same order on private
// packets:
//   Emitter - the fade tile level, the mesh rotation angle (-0x32 per tick) and swirl phase (+0xC8),
//             the walk point x, the length scale, the mesh colour (every channel) and the U scroll
//             half way between the tick's and the next tick's values; the mouth = the caster's bone
//             0xB (flag 0x2000) at its pose midpoint (pose_midpoint + GetEffectSpawnPosition, the
//             bones put back); vertices go to a private buffer. The tick before the meshes end
//             (t = 0x31): tick N's picture holds.
//   Targets - the prim-model layout through prim::play_held (every object's exact in-between
//             record), the anchor fixed at the spawn; the task's last draw (counter 0x2E) holds.
//             The target entity's colour (engine entity colour +0x28) is a real-tick effect only.
//   Sprites - the bubble half way up (y - 16 per tick), its flipbook frame N held; a sprite's last
//             draw (frame 0x11) holds.
// Spawns (target tasks, random bubbles) stay at 15 Hz. No task has a draw-only mode. The module
// packet arena alternates per tick: the held packets start where the tick's first draw started.
// The camera is the frame gate's. The mesh frame 0x2520FF0, the GTE, the Field_Alloc scratch, the
// engine words the anchors and the sprite draws touch and the caster's bone matrices are put back.

namespace ff8fx
{
namespace badbreath131
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x18000;       // module packet arena per tick
	enum : uint32_t { REC_EMITTER = 1, REC_PLAY = 2, REC_SPRITE = 3 };

	struct Rec
	{
		uint32_t kind;
		EmitterNode emitter;      // emitter: the node as drawn (mouth taken, before the update)
		TargetNode *target;       // play: the live node (its layout: prim::play_held knows what it drew)
		int16_t tpos[4];          // play: the node's anchor
		bool next_plays;          // play: the next tick plays the layout again (else its picture holds)
		SpriteNode sprite;        // sprite: as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[1 + 3 + 0x40];
	static uint8_t g_held_packets[ARENA];
	static int16_t g_held_verts[0x400];     // swirled mesh vertices (magic buffer + 0xA00)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0xA00)

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

	static void held_note_emitter(const EmitterNode *e)
	{
		Rec *r = NewRec(REC_EMITTER);
		if (r) r->emitter = *e;
	}

	static void held_note_target(const TargetNode *t)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->target = (TargetNode *)t;
		memcpy(r->tpos, t->pos, sizeof(r->tpos));
		r->next_plays = t->counter + 1 < 0x2F; // (the target task ends after its draw at 0x2E)
	}

	static void held_note_sprite(const SpriteNode *p)
	{
		Rec *r = NewRec(REC_SPRITE);
		if (r) r->sprite = *p;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// ------------------------------------------------------------------
	// Held frame
	// ------------------------------------------------------------------
	// the emitter's draw state
	struct MeshState
	{
		int32_t level;      // fade tile level (0x1000 = full)
		int16_t angle, phase;
		int16_t x;          // walk point x
		int16_t z;          // walk point z (the targets' mean)
		int16_t mouth[3];   // the caster's bone 0xB
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

	// the caster's bone at its pose num/den of the way to the next tick (the bones put back)
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

	// the emitter's state num/den of the way from the tick's draw (counter c) to the next one
	static void HalfEmitter(const EmitterNode &e, int num, int den, MeshState &s)
	{
		const int32_t t = e.counter - 0xF;
		if ((uint32_t)(t + 1) >= 0x32) num = 0; // the last mesh picture holds
		s.level = lerp_i(TileLevel(t), TileLevel(t + 1), num, den);
		s.angle = lerp16(e.angle, (int16_t)(e.angle - 0x32), num, den);
		s.phase = lerp16(e.phase, (int16_t)(e.phase + 0xC8), num, den);
		s.x = (int16_t)lerp_i(WalkX(t), WalkX(t + 1), num, den);
		s.z = e.z;
		if (num) Anchor(Caster(), 0xB, 0x2000, num, den, s.mouth);
		else memcpy(s.mouth, e.mouth, sizeof(s.mouth));
		s.growth = lerp_i(Growth(t), Growth(t + 1), num, den);
		uint8_t c0[4] = {}, c1[4] = {};
		MeshColour(t, c0);
		MeshColour(t + 1, c1);
		for (int k = 0; k < 3; k++) s.rgb[k] = (uint8_t)lerp_i(c0[k], c1[k], num, den);
		s.scroll = (uint16_t)(((uint8_t)t * 10 + 10 * num / den) & 0x7F);
	}

	// the two meshes and the fade tile of a state (the emitter's draw, 0x6B2EC0)
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
		const int32_t dy = -500 - (int32_t)s.mouth[1];
		const int32_t dz = (int32_t)s.z - s.mouth[2];
		const int32_t len = Sqrt(mul32(dz, dz) + mul32(dy, dy) + mul32(dx, dx)) + 0x200;
		int16_t scale[9] = { 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, (int16_t)len };
		scale[8] = (int16_t)(mul32(scale[8], s.growth) >> 12);
		const int32_t v[3] = { dx, -dy, -dz };
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
		PacketCursor() = MeshDraw(h, RenderOT(), 2, PacketCursor());
		RotZ((int16_t)-s.angle, &rot);
		GteSetRotMatrix(&rot);
		GteZeroTrans();
		SwirlVertices(MESH2, (int32_t)s.phase + 0x400, (int16_t *)verts);
		GteSetRotMatrix(&MeshFrame());
		GteSetTransVector(&MeshFrame());
		*(uint8_t **)h = MESH2;
		PacketCursor() = MeshDraw(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// engine words the anchors (0x1D9770C) and the sprite draws touch, the module's mesh frame
		const int16_t w70c = var<int16_t>(0x1D9770C);
		const Mat4x3 frame = MeshFrame();
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR: the square roots set LZCS) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			MeshState s;
			if (r.kind == REC_EMITTER) HalfEmitter(r.emitter, num, den, s);
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_EMITTER:
				MeshPass(s, (uint32_t)g_held_verts);
				break;
			case REC_PLAY:
			{
				PrimArg arg;
				memcpy(arg.pos, r.tpos, sizeof(arg.pos));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)r.target->layout, PrimCallback, (int)&arg, r.next_plays ? num : 0, den);
				break;
			}
			case REC_SPRITE:
			{
				SpriteNode p = r.sprite;
				// the bubble rises 16 per tick (its last frame holds)
				if (p.counter + 1 < 0x12) p.pos[1] = lerp16(p.pos[1], (int16_t)(p.pos[1] - 0x10), num, den);
				SpriteDraw(&p);
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

	static void register_mag131_held()
	{
		mark_held(badbreath131::ORIG_EmitterTask);
		mark_held(badbreath131::ORIG_TargetTask);
		mark_held(badbreath131::ORIG_SpriteTask);
		register_module_held(131, badbreath131::HeldReady, badbreath131::HeldFrame);
	}
}
