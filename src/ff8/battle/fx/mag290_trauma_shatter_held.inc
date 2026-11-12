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

// 30 fps part of mag290_trauma_shatter.cpp (Trauma's body shattering, effect 290). Exact vanilla shapes: the logic stays at
// 15 Hz, a held frame draws the state num/den of the way from the last real tick to the next one.
//
// Every draw of the module draws THEN its state moves (the master draws the pose entries the bone scripts left on the previous
// tick, then moves the fragments; the scripts run after it). A real tick records every draw in order with the GTE registers and
// the Field_Alloc scratch it started from; a held frame replays them in the same order on private packets, with the current
// camera (the frame gate's):
//   layouts A / B / C, layout tasks - prim::play_held (every object's exact in-between record), the block as the real play
//             read it (anchors fixed at their spawn); blended vertex frames go to a private buffer.
//   under sprite - holds (fixed point, flipbook frame N).
//   body    - the pose entries half way to the ones the scripts left for the next tick (translations linearly, rotations the
//             short way round on 12 bits; the visibility words hold), drawn with the module's own caster draw (shadow + body)
//             into a private copy of the frame arena and a private projected-vertex buffer; the caster's bone matrices are
//             put back. The caster's animation pose (the engine's) is not interpolated: it holds (as effect 64).
//   fragments - position and angles half way to the moved ones (the fall and the bounce are in the next state), the fade half
//             way to the next tick's; a fragment that ends holds; one born by this tick's scripts appears at the next tick.
//   marks   - position half way along their velocity, flipbook frame N held; a mark that ends holds.
//   sprites A / B - position half way along their velocity, flipbook frame N held; a sprite on its last tick holds.
// A part the next tick no longer draws holds its picture (state N). Spawns (CRT rand, shared LCG, module LCG), the shatters and
// the stream stay at 15 Hz. Held without interpolation: the caster's colour +0x2C (15 Hz steps, engine state). No task has a
// draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's first draw started. The
// GTE, the Field_Alloc scratch, the frame arena cursor, the projected-vertex buffer pointer, the word 0x1D9770C and the engine
// words the sprite draws touch are put back.

namespace ff8fx
{
namespace shatter290
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x20000;       // module packet arena per tick (0xC000 between the two arenas)
	static const uint32_t FRAME = 0x40000;       // caster packets (frame arena)
	static const uint32_t VERTS = 0x10000;       // projected vertices of one object

	struct Rec
	{
		int kind;                 // DrawKind
		const void *node;         // master / layout: the live node
		int16_t counter;          // master: the counter as drawn
		int16_t pos[4];           // under sprite: its point
		PrimArg arg;              // plays: the block as the play read it
		SpriteNode sprite;        // sprites: the node as drawn
		uint32_t frame;           // body: the frame arena at the draw
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[8 + 3 + 0x10 + 0x20];
	static PoseEntry g_pose_drawn[28];      // the pose entries the caster was drawn with
	static PoseEntry g_pose_held[28];
	static Fragment g_frags_drawn[0xC0];    // the fragments as drawn
	static Mark g_marks_drawn[0x80];        // the marks as drawn
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_verts[VERTS];
	static uint8_t g_held_morph[0x10000];   // blended vertex frames (magic buffer + 0x2060C)
	static uint8_t g_bones_save[256 * 0x30];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(int kind, const void *n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		r.counter = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(const MasterNode *m, int kind, const PrimArg *arg)
	{
		Rec *r = NewRec(kind, m);
		if (!r) return;
		r->counter = m->counter;
		r->arg = *arg;
	}

	static void held_note_under(const MasterNode *m, const int16_t *pos)
	{
		Rec *r = NewRec(UNDER, m);
		if (!r) return;
		r->counter = m->counter;
		memcpy(r->pos, pos, sizeof(r->pos));
	}

	static void held_note_body(const MasterNode *m)
	{
		Rec *r = NewRec(BODY, m);
		if (!r) return;
		r->counter = m->counter;
		r->frame = FrameCursor();
		memcpy(g_pose_drawn, m->pose, sizeof(g_pose_drawn));
	}

	static void held_note_frags(const MasterNode *m)
	{
		Rec *r = NewRec(FRAGS, m);
		if (!r) return;
		r->counter = m->counter;
		memcpy(g_frags_drawn, m->frags, sizeof(g_frags_drawn));
	}

	static void held_note_marks(const MasterNode *m)
	{
		Rec *r = NewRec(MARKS, m);
		if (!r) return;
		r->counter = m->counter;
		memcpy(g_marks_drawn, m->marks, sizeof(g_marks_drawn));
	}

	static void held_note_sprite(const SpriteNode *s, int kind)
	{
		Rec *r = NewRec(kind, s);
		if (!r) return;
		r->sprite = *s;
	}

	static void held_note_layout(LayoutNode *L, const PrimArg *arg)
	{
		Rec *r = NewRec(LAYOUT, L);
		if (!r) return;
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// 12-bit angle half way the short way round
	static int16_t LerpAngle12(int16_t a, int16_t b, int num, int den)
	{
		const int32_t d = (((int32_t)(int16_t)(b - a) + 0x800) & 0xFFF) - 0x800;
		return (int16_t)(a + d * num / den);
	}

	// the body with the pose entries half way to the next tick's
	static void BodyHeld(const Rec &r, int k, int den)
	{
		const MasterNode *m = (const MasterNode *)r.node;
		for (int i = 0; i < 28; i++)
		{
			PoseEntry p = g_pose_drawn[i];
			if (k)
			{
				const PoseEntry &b = m->pose[i];
				for (int j = 0; j < 3; j++)
				{
					p.pos[j] = lerp16(p.pos[j], b.pos[j], k, den);
					p.rot[j] = LerpAngle12(p.rot[j], b.rot[j], k, den);
				}
			}
			g_pose_held[i] = p;
		}
		uint8_t *entity = Caster();
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		const uint32_t bones = (uint32_t)skel[0] * 0x30;
		memcpy(g_bones_save, skel + 0x10, bones);
		// the real tick's packets / projected vertices under the held ones (bytes a draw leaves alone)
		const uint32_t frame = FrameCursor();
		if (frame > r.frame)
		{
			const uint32_t used = frame - r.frame;
			memcpy(g_held_frame, (const void *)r.frame, used < FRAME ? used : FRAME);
		}
		const uint32_t verts = BoneWorkspace();
		if (verts) memcpy(g_held_verts, (const void *)verts, VERTS);
		FrameCursor() = (uint32_t)g_held_frame;
		BoneWorkspace() = (uint32_t)g_held_verts;
		CasterDraw(entity, g_pose_held);
		BoneWorkspace() = verts;
		FrameCursor() = frame;
		memcpy(skel + 0x10, g_bones_save, bones);
	}

	// the fragments half way to the moved ones (an ending one holds)
	static void FragsHeld(const MasterNode *m, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x68);
		for (int i = 0; i < 0xC0; i++)
		{
			Fragment f = g_frags_drawn[i];
			if (f.life == 0) continue;
			int32_t fade = FragFade(f.life);
			if (f.life > 1 && num)
			{
				const Fragment &b = m->frags[i];
				for (int j = 0; j < 3; j++) f.pos[j] = lerp16(f.pos[j], b.pos[j], num, den);
				f.angle[0] = lerp16(f.angle[0], b.angle[0], num, den);
				f.angle[1] = lerp16(f.angle[1], b.angle[1], num, den);
				fade = lerp_i(fade, FragFade(b.life), num, den);
			}
			FragmentDraw(h, &f, fade);
		}
		FieldFree(0x68);
	}

	// the marks half way along their velocity (an ending one holds)
	static void MarksHeld(const MasterNode *m, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = SEQ_Mark;
		F<uint16_t>(h, 0x92) = 0;
		F<uint16_t>(h, 0x90) = 0;
		F<uint16_t>(h, 0x8E) = 0;
		F<uint16_t>(h, 0x8A) = 0;
		F<uint16_t>(h, 0x88) = 0;
		F<uint16_t>(h, 0x86) = 0;
		F<uint16_t>(h, 0x60) = 0;
		F<uint16_t>(h, 0x58) = 0;
		F<uint16_t>(h, 0x50) = 0;
		F<uint16_t>(h, 0x48) = 0;
		for (int i = 0; i < 0x80; i++)
		{
			Mark k = g_marks_drawn[i];
			if (k.life == 0) continue;
			if (k.life > 1 && num)
				for (int j = 0; j < 3; j++) k.pos[j] = lerp16(k.pos[j], m->marks[i].pos[j], num, den);
			MarkSprite(h, &k);
		}
		FieldFree(0xB4);
	}

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const int16_t w70c = var<int16_t>(0x1D9770C);
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// engine words the sprite draws touch
		uint8_t seq_words[0xC], seq_tmp[0x80], seq_c18[8];
		memcpy(seq_words, (const void *)0xB8B9E4, sizeof(seq_words));
		memcpy(seq_tmp, (const void *)0x1D999C8, sizeof(seq_tmp));
		memcpy(seq_c18, (const void *)0x1D99C18, sizeof(seq_c18));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			const MasterNode *m = (const MasterNode *)r.node;
			switch (r.kind)
			{
			case PLAY_A:
			case PLAY_B:
			case PLAY_C:
			{
				const bool next = r.kind == PLAY_A ? r.counter + 1 < 0x10 : (uint32_t)(r.counter + 1 - 0x14) < 0x16;
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				const uint8_t *l = r.kind == PLAY_A ? m->layoutA : r.kind == PLAY_B ? m->layoutB : m->layoutC;
				prim::play_held((prim::Layout *)l, PrimCallback, (int)&arg, next ? num : 0, den);
				break;
			}
			case UNDER:
				SpriteDraw(r.pos, r.counter, SEQ_Under, 0x4000);
				break;
			case BODY:
				BodyHeld(r, r.counter + 1 < 0x5A ? num : 0, den);
				break;
			case FRAGS:
				FragsHeld(m, num, den);
				break;
			case MARKS:
				MarksHeld(m, num, den);
				break;
			case SPRITE_A:
			case SPRITE_B:
			{
				SpriteNode s = r.sprite;
				const int16_t life = r.kind == SPRITE_A ? 0xC : 0xD;
				if (s.counter + 1 < life)
					for (int j = 0; j < 3; j++) s.pos[j] = lerp16(s.pos[j], (int16_t)(s.pos[j] + s.vel[j]), num, den);
				if (r.kind == SPRITE_A) SpriteDraw(s.pos, s.counter, SEQ_A, 0x800);
				else SpriteDraw(s.pos, s.counter, SEQ_B, 0x1770);
				break;
			}
			default:
			{
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)((LayoutNode *)r.node)->layout, PrimCallback, (int)&arg, num, den);
				break;
			}
			}
		}
		memcpy((void *)0x1D99C18, seq_c18, sizeof(seq_c18));
		memcpy((void *)0x1D999C8, seq_tmp, sizeof(seq_tmp));
		memcpy((void *)0xB8B9E4, seq_words, sizeof(seq_words));
		var<int16_t>(0x1D9770C) = w70c;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag290_held()
	{
		mark_held(shatter290::ORIG_MasterTask);
		mark_held(shatter290::ORIG_ScriptTask);
		mark_held(shatter290::ORIG_LayoutTask);
		mark_held(shatter290::ORIG_SpriteTaskA);
		mark_held(shatter290::ORIG_SpriteTaskB);
		register_module_held(290, shatter290::HeldReady, shatter290::HeldDraw);
	}
}
