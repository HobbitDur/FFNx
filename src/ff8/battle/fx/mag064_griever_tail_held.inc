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

// 30 fps part of mag064_griever_tail.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag064_griever_tail.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every draw of the module draws THEN its state moves (the master draws the pose the bone scripts
// left on the previous tick; the scripts run after it). A real tick records every draw in order (the
// tiles, the caster, the scripts' quads, the layouts, the sprites) with the GTE registers and the
// Field_Alloc scratch it started from. A held frame replays the log in the same order:
//   caster  - the pose offsets half way to the ones the scripts left for the next tick (translations
//             linearly, rotations the short way round on 12 bits; the visibility words hold), drawn
//             with the module's own caster draw (shadow + model) into a private frame arena and a
//             private projected-vertex buffer; the caster's bone matrices are put back afterwards.
//             The last draw (the next tick ends the master) holds;
//   tiles   - the level half way to the next tick's; the last one of a window holds;
//   quads   - hold (the scripts' flat quads; no script of the data uses them);
//   layouts - prim::play_held: every object's exact in-between record; the anchor holds (fixed at
//             spawn); the v scroll goes half way to the next tick's while the layout plays on;
//   sprites - the position half way along the velocity (the update adds 4 x velocity); the sprite
//             frame holds; a sprite on its last frame holds.
// Blended vertex frames go to a private buffer (the original's is magic buffer + 0x26F20).
// The caster's animation pose (the engine's, bone words +4..+8) is not interpolated: it holds.
// No task of the module has a draw-only mode; the module does not write the battle camera.

namespace ff8fx
{
namespace gtail064
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;        // module packet arena per tick
	static const uint32_t FRAME = 0x40000;        // caster packets (frame arena)
	static const uint32_t VERTS = 0x20000;        // projected vertices of one object

	enum RecKind : uint8_t { REC_CASTER, REC_TILE, REC_QUAD, REC_PLAY, REC_SPRITE };
	struct Rec
	{
		RecKind kind;
		int32_t t;                  // tile: step
		int16_t quad[3];            // quad: the program's colour words
		const MasterNode *master;   // caster: the live master (the next pose)
		uint32_t frame;             // caster: frame arena at the draw
		LayoutNode *layout;         // layout: the live node
		int16_t counter;            // layout: its counter at the draw
		PrimArg arg;
		SpriteNode sprite;          // sprite: the node as drawn
		const SpriteNode *node;     // sprite: the live node
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[200];
	static PoseEntry g_pose_drawn[71];   // the pose offsets the caster was drawn with
	static PoseEntry g_pose_held[71];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_verts[VERTS];
	static uint8_t g_held_morph[0x10000]; // blended vertex frames
	static uint8_t g_bones_save[256 * 0x30];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *held_add(RecKind kind)
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

	static void held_note_caster(const MasterNode *m)
	{
		Rec *r = held_add(REC_CASTER);
		if (!r) return;
		r->master = m;
		r->frame = FrameArena();
		memcpy(g_pose_drawn, m->pose, sizeof(g_pose_drawn));
	}

	static void held_note_tile(int32_t t)
	{
		Rec *r = held_add(REC_TILE);
		if (!r) return;
		r->t = t;
	}

	static void held_note_quad(const int16_t *ip)
	{
		Rec *r = held_add(REC_QUAD);
		if (!r) return;
		memcpy(r->quad, ip, sizeof(r->quad));
	}

	static void held_note_play(LayoutNode *L, const PrimArg *arg)
	{
		Rec *r = held_add(REC_PLAY);
		if (!r) return;
		r->layout = L;
		r->counter = L->counter;
		r->arg = *arg;
	}

	static void held_note_sprite(const SpriteNode *s)
	{
		Rec *r = held_add(REC_SPRITE);
		if (!r) return;
		r->sprite = *s;
		r->node = s;
	}

	// 0x700150's level with the master's arguments (in 0, hold 1, out 3)
	static int32_t TileLevel(int32_t t)
	{
		if (t < 1) return 0x1000;
		return (shl32(1 - t, 12) - 0x1000) / 3 + 0x1000;
	}
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // MAG_063_sub_7040B0

	// 12-bit angle half way the short way round
	static int16_t LerpAngle12(int16_t a, int16_t b, int num, int den)
	{
		const int32_t d = (((int32_t)(int16_t)(b - a) + 0x800) & 0xFFF) - 0x800;
		return (int16_t)(a + d * num / den);
	}

	static void CasterHeld(const Rec &r, int num, int den)
	{
		const MasterNode *m = r.master;
		const bool next = m->counter < 0xAA; // the next tick draws the caster again
		for (int i = 0; i < 71; i++)
		{
			PoseEntry p = g_pose_drawn[i];
			if (next)
			{
				const PoseEntry &b = m->pose[i];
				for (int k = 0; k < 3; k++)
				{
					p.pos[k] = lerp16(p.pos[k], b.pos[k], num, den);
					p.rot[k] = LerpAngle12(p.rot[k], b.rot[k], num, den);
				}
			}
			g_pose_held[i] = p;
		}
		uint8_t *entity = Caster();
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		const uint32_t bones = (uint32_t)skel[0] * 0x30;
		memcpy(g_bones_save, skel + 0x10, bones);
		// the real tick's packets / projected vertices under the held ones (bytes a draw leaves alone)
		const uint32_t frame = FrameArena();
		if (frame > r.frame)
		{
			const uint32_t used = frame - r.frame;
			memcpy(g_held_frame, (const void *)r.frame, used < FRAME ? used : FRAME);
		}
		const uint32_t verts = var<uint32_t>(0x1D98B3C);
		if (verts) memcpy(g_held_verts, (const void *)verts, VERTS);
		FrameArena() = (uint32_t)g_held_frame;
		var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_verts;
		CasterDraw(entity, g_pose_held);
		var<uint32_t>(0x1D98B3C) = verts;
		FrameArena() = frame;
		memcpy(skel + 0x10, g_bones_save, bones);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_CASTER:
				CasterHeld(r, num, den);
				break;
			case REC_TILE:
			{
				int32_t level = TileLevel(r.t);
				if (r.t + 1 < 4) level = lerp_i(level, TileLevel(r.t + 1), num, den);
				PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
				break;
			}
			case REC_QUAD:
				ScriptQuad(r.quad);
				break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				arg.morph = (uint32_t)g_held_morph;
				if (r.layout->counter != r.counter) // the layout plays on: v scroll half way
					arg.scroll = (int16_t)lerp_i(arg.scroll, shl32(r.counter + 1, 6), num, den);
				prim::play_held((prim::Layout *)r.layout->layout, PartCallback, (int)&arg, num, den);
				break;
			}
			case REC_SPRITE:
			{
				SpriteNode s = r.sprite;
				const int32_t frames = *(const int16_t *)(*(const uint8_t *const *)(SEQUENCES + s.seq * 4) + 8);
				if (s.frame + 1 < frames)
				{
					for (int k = 0; k < 3; k++)
						s.pos[k] = lerp16(s.pos[k], (int16_t)(s.pos[k] + (int16_t)(s.vel[k] * 4)), num, den);
				}
				SpriteDraw(&s);
				break;
			}
			}
		}
	}
}

	static void register_mag064_held()
	{
		mark_held(gtail064::ORIG_MasterTask);
		mark_held(gtail064::ORIG_ScriptTask);
		mark_held(gtail064::ORIG_LayoutTask);
		mark_held(gtail064::ORIG_SpriteTask);
		register_module_held(64, gtail064::HeldReady, gtail064::HeldFrame);
	}
}
