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

// 30 fps part of mag308_mobile_type8.cpp: the draw log of the real tick and the held-frame drawing (included at the end of
// mag308_mobile_type8.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every draw of the module draws THEN its state moves (the main draws the three entities with the pose offsets the bone
// scripts left on the previous tick; the scripts, layouts and sprites run after it). A real tick records every draw in
// order (entities, tile, particles, the scripts' quads, the layouts, the sprites) with the GTE registers and the
// Field_Alloc scratch it started from. A held frame replays the log in the same order into a private module arena (the real
// tick's packets under it) and a private frame arena:
//   entities  - each entity's pose offsets half way to the ones the scripts left for the next tick (translations linearly,
//               rotations the short way round on 12 bits; the hide / key words hold), drawn by the module's entity draw into
//               a private projected-vertex buffer; the entities' animation poses hold (as Griever's 064); the last draw holds;
//               the skeletons, the entity records and the object renderer's cursor 0x1D99C18 are put back;
//   tile      - the level half way to the next tick's (rising 0x100 a tick, then falling); the last one holds;
//   particles - half way along their velocity; the sequence frame holds (at fraction 1 the next tick's); a particle on its
//               last tick holds (gone at fraction 1); after the main's last particle draw (counter 0x55) they all hold;
//   quads     - hold;
//   layouts   - prim::play_held: every object's exact in-between record; the node's frame (fixed at spawn) composed with the
//               current camera (the frame gate's), as the layout task builds its block;
//   sprites   - the position half way along the velocity (the update adds 4 x velocity); the frame holds (at fraction 1 the
//               next tick's); a sprite on its last frame holds.
// The main drops the script, sprite and layout queues on counter 0x55 before they run: the layouts and sprites the tick
// before draw for the last time and hold.
// Blended vertex frames go to a private buffer (the original's is magic buffer + 0x22A18). The screen flash level, the
// entity flags / colours and the camera animation are engine state (the gate). No draw-only mode.

namespace ff8fx
{
namespace mobile308
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;         // module packet arena per tick
	static const uint32_t FRAME = 0x40000;         // entity packets (frame arena)
	static const uint32_t VERTS = 0x20000;         // the engine's projected-vertex scratch [0x1D98B3C]
	static const uint32_t SKEL_MAX = 0x20 + 0x30 * 256;
	static const int REC_MAX = 160;

	enum RecKind : uint8_t { REC_ENTS, REC_TILE, REC_PARTS, REC_QUAD, REC_PLAY, REC_SPRITE };
	struct Rec
	{
		RecKind kind;
		int16_t c;                  // ents: main counter
		int32_t level;              // tile
		int16_t quad[3];            // quad: the program's colour words
		LayoutNode *layout;         // play: the live node
		PrimArg arg;                // play: the callback block
		int16_t frame_pos[3];       // play: the node's position and turn (fixed at spawn)
		int16_t frame_turn;
		SpriteNode sprite;          // sprite: the node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static Rec g_recs[REC_MAX];
	static uint32_t g_packets = 0;                 // module packet cursor at the tick's start
	static uint32_t g_frame = 0;                   // frame arena at the entity draws
	static uint8_t g_verts_drawn[VERTS];           // the vertex scratch as the entity draws found it
	static PoseEntry g_pose_drawn[3][0x2D];        // the pose offsets the entities were drawn with
	static PoseEntry g_pose_held[3][0x2D];
	static Particle g_parts[0x80];                 // the particles as drawn
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_verts[VERTS];
	static uint8_t g_held_morph[0x10000];          // blended vertex frames
	static uint8_t g_skel_save[3][SKEL_MAX];
	static uint8_t g_ent_save[3][0x9C];
	static MainNode *g_held_main = nullptr;        // scratch main node (particles), allocated once

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
		g_frame = 0;
	}

	static Rec *held_add(RecKind kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_entities(const MainNode *m)
	{
		Rec *r = held_add(REC_ENTS);
		if (!r) return;
		r->c = m->counter;
		g_frame = FrameArena();
		const uint32_t v = var<uint32_t>(0x1D98B3C);
		if (v) memcpy(g_verts_drawn, (const void *)v, VERTS);
		memcpy(g_pose_drawn, m->pose, sizeof(g_pose_drawn));
	}

	static void held_note_tile(int32_t level)
	{
		Rec *r = held_add(REC_TILE);
		if (r) r->level = level;
	}

	static void held_note_parts(const MainNode *m)
	{
		if (!held_add(REC_PARTS)) return;
		memcpy(g_parts, m->parts, sizeof(g_parts));
	}

	static void held_note_quad(const int16_t *ip)
	{
		Rec *r = held_add(REC_QUAD);
		if (r) memcpy(r->quad, ip, sizeof(r->quad));
	}

	static void held_note_play(LayoutNode *L, const PrimArg *arg)
	{
		Rec *r = held_add(REC_PLAY);
		if (!r) return;
		r->layout = L;
		r->arg = *arg;
		memcpy(r->frame_pos, L->pos, sizeof(r->frame_pos));
		r->frame_turn = L->turn;
	}

	static void held_note_sprite(const SpriteNode *s)
	{
		Rec *r = held_add(REC_SPRITE);
		if (r) r->sprite = *s;
	}

	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// 12-bit angle half way the short way round
	static int16_t LerpAngle12(int16_t a, int16_t b, int num, int den)
	{
		const int32_t d = (((int32_t)(int16_t)(b - a) + 0x800) & 0xFFF) - 0x800;
		return (int16_t)(a + d * num / den);
	}

	// the main's tile level at counter c (0: no tile)
	static bool TileAt(int32_t c, int32_t *level)
	{
		const uint32_t t = (uint32_t)(c - 0x46), t2 = (uint32_t)(c - 0x56);
		if (t < 0x10) { *level = (int32_t)((t & 0xFFFFF) << 8); return true; }
		if (t2 < 0x10) { *level = (int32_t)(0x1000 - ((t2 & 0xFFFFF) << 8)); return true; }
		return false;
	}

	static void EntitiesHeld(const Rec &r, int num, int den)
	{
		const MainNode *m = (const MainNode *)(TexBase() + MAIN);
		const bool next = (uint16_t)(r.c + 1) < 0x56;   // the next tick draws the entities again
		for (int k = 0; k < 3; k++)
			for (int i = 0; i < 0x2D; i++)
			{
				PoseEntry p = g_pose_drawn[k][i];
				if (next)
				{
					const PoseEntry &b = m->pose[k][i];
					for (int a = 0; a < 3; a++)
					{
						p.pos[a] = lerp16(p.pos[a], b.pos[a], num, den);
						p.rot[a] = LerpAngle12(p.rot[a], b.rot[a], num, den);
					}
				}
				g_pose_held[k][i] = p;
			}
		uint32_t skel_n[3];
		uint8_t *skel[3];
		for (int k = 0; k < 3; k++)
		{
			uint8_t *ent = Caster() + k * 0x9C;
			skel[k] = *(uint8_t **)(ent + 0x64) ? **(uint8_t ***)(ent + 0x64) : nullptr;
			skel_n[k] = skel[k] ? 0x20 + 0x30 * (uint32_t)skel[k][0] : 0;
			if (skel_n[k] > SKEL_MAX) skel_n[k] = 0;
			if (skel_n[k]) memcpy(g_skel_save[k], skel[k], skel_n[k]);
			memcpy(g_ent_save[k], ent, 0x9C);
		}
		const uint32_t cursor_c18 = var<uint32_t>(0x1D99C18);
		const uint32_t frame = FrameArena();
		if (g_frame && frame > g_frame)
		{
			const uint32_t used = frame - g_frame;
			memcpy(g_held_frame, (const void *)g_frame, used < FRAME ? used : FRAME);
		}
		FrameArena() = (uint32_t)g_held_frame;
		const uint32_t verts = var<uint32_t>(0x1D98B3C);
		memcpy(g_held_verts, g_verts_drawn, VERTS);
		var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_verts;
		for (int k = 0; k < 3; k++) EntityDraw(Caster() + k * 0x9C, g_pose_held[k]);
		var<uint32_t>(0x1D98B3C) = verts;
		FrameArena() = frame;
		for (int k = 2; k >= 0; k--)
		{
			if (skel_n[k]) memcpy(skel[k], g_skel_save[k], skel_n[k]);
			memcpy(Caster() + k * 0x9C, g_ent_save[k], 0x9C);
		}
		var<uint32_t>(0x1D99C18) = cursor_c18;
	}

	// the particles at num/den: each live one half way along its step, its frame (life) held; one on its last tick holds. At
	// fraction 1 the next tick's (its step made, its frame stepped, a last-tick one gone). next = 0: the next tick draws none
	static void PartsHeld(int num, int den, bool next)
	{
		memcpy(g_held_main->parts, g_parts, sizeof(g_parts));
		if (next)
			for (int k = 0; k < 0x80; k++)
			{
				Particle &p = g_held_main->parts[k];
				if (p.life == 0) continue;
				if (num >= den)
				{
					if (--p.life == 0) continue;
				}
				else if (p.life == 1) continue;
				for (int a = 0; a < 3; a++) p.pos[a] = lerp16(p.pos[a], (int16_t)(p.pos[a] + (int16_t)(p.vel[a] << 2)), num, den);
			}
		Particles(g_held_main);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		if (!g_held_main) g_held_main = new MainNode();
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		const int16_t c = ((const MainNode *)(TexBase() + MAIN))->counter; // the next tick's main counter
		const bool dropped = c == 0x55;   // the next tick's main drops the script / sprite / layout queues before they run
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_ENTS:
				EntitiesHeld(r, num, den);
				break;
			case REC_TILE:
			{
				int32_t level = r.level, next;
				if (TileAt(c, &next) && num != 0) level = lerp_i(level, next, num, den);
				PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
				break;
			}
			case REC_PARTS:
				PartsHeld(num, den, (uint16_t)c < 0x56);
				break;
			case REC_QUAD:
			{
				int16_t q[3];
				memcpy(q, r.quad, sizeof(q));
				uint8_t *pk = (uint8_t *)PacketCursor();
				PacketCursor() += 0x18;
				F<int16_t>(pk, 8) = 0;
				F<int16_t>(pk, 0x10) = 0;
				F<int16_t>(pk, 0xE) = 0;
				F<int16_t>(pk, 0xA) = 0;
				F<int16_t>(pk, 0x14) = 0x140;
				F<int16_t>(pk, 0xC) = 0x140;
				F<int16_t>(pk, 0x16) = 0xD8;
				F<int16_t>(pk, 0x12) = 0xD8;
				pk[4] = (uint8_t)q[0];
				pk[5] = (uint8_t)q[1];
				pk[6] = (uint8_t)q[2];
				F<uint32_t>(pk, 0) = 0x5000000;
				pk[7] = 0x2A;
				InsertPrimAltViewport(OT(0x20), pk);
				break;
			}
			case REC_PLAY:
			{
				// the block as the layout task builds it, at the current camera
				PrimArg arg = r.arg;
				RotationY(r.frame_turn, &arg.m);
				arg.m.t[0] = r.frame_pos[0];
				arg.m.t[1] = r.frame_pos[1];
				arg.m.t[2] = r.frame_pos[2];
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				arg.morph = (uint32_t)g_held_morph;
				Replay(r);
				prim::play_held((prim::Layout *)r.layout->layout, PartCallback, (int)&arg, dropped ? 0 : num, den);
				break;
			}
			case REC_SPRITE:
			{
				SpriteNode s = r.sprite;
				const int32_t frames = *(const int16_t *)(var<uint32_t>(SPRITES + s.seq * 4) + 8);
				if (s.frame + 1 < frames && !dropped)
				{
					for (int a = 0; a < 3; a++) s.pos[a] = lerp16(s.pos[a], (int16_t)(s.pos[a] + (int16_t)(s.vel[a] * 4)), num, den);
					if (num >= den) s.frame++; // (at fraction 1: the next tick's frame)
				}
				SpriteDraw(&s);
				break;
			}
			}
		}
	}
}

	static void register_mag308_held()
	{
		mark_held(mobile308::ORIG_MainTask);
		mark_held(mobile308::ORIG_ScriptTask);
		mark_held(mobile308::ORIG_LayoutTask);
		mark_held(mobile308::ORIG_SpriteTask);
		register_module_held(308, mobile308::HeldReady, mobile308::HeldFrame);
	}
}
