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

// 30 fps part of mag025_cura.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag025_cura.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Cura task updates THEN draws. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes where
// the code does not write) and the next tick's state of what moves (the task's own motion step,
// pure math: no random draw, no spawn). A held frame replays the log in the same order:
//   Sprites  - (glow, swirl sparks + their ground sprites, trail motes, particles, sparkles + their
//              spinning ground sprites) from a copy of the node as drawn, half way to the next
//              tick's position (swirl: radius / angle step and falling velocity; motes: decelerating
//              fall; particles: constant fall) and spin angle (sparkles: + 0x40 per tick). A sprite
//              that the next tick does not draw (end of its sequence, below the ground) holds.
//              Sequence frames, colours and scales hold (15 Hz steps).
//   Aura / rings - the prim-model layouts through prim::play_held (every object's exact in-between
//              record); the anchor matrix is rebuilt with the live camera, the rings' spin angles
//              half way to the next tick's (held when the layout ended). The layout bytes of the
//              real play are put under the replay (a finished player's node may be reused within
//              the tick by a new player decoding its layout), then the live bytes back.
//   Target overlay - the target's model at its in-between pose (pose_midpoint), the rising light
//              / lift offset half way to the next tick's; the light position (camera yaw) holds.
//              Its flat packets go to a private frame-arena buffer holding the real tick's bytes.
// The shared battle light words (light task) are not drawn by the module: nothing to redraw.
// No Cura task has a draw-only mode.

namespace ff8fx
{
namespace act
{
namespace cura
{
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE;
	static const uint32_t GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t SCRATCH = 0x400;       // Field_Alloc scratch under a draw's headers
	static const uint32_t NODE_BYTES = 0x84;     // sprite nodes (the players' fields used are below)
	static const uint32_t LAYOUT_MAX = 0x64;     // aura layout (ring: 0x34)
	static const uint32_t OVL_LO = OVL_LIGHT, OVL_HI = OVL_STATE + 0xF8; // light block + overlay state
	enum : uint32_t { REC_SPRITE = 1, REC_GROUND, REC_SPIN, REC_PLAY, REC_OVERLAY };

	struct Rec
	{
		uint32_t kind;
		uint32_t node;                 // the live node (plays: its layout at +0x60)
		uint32_t seq, colour, angle, scale; // flat sprites
		int16_t next[3];               // sprites: next tick's position
		bool moves;                    // the next tick draws it (sprites: moved; plays: ring spins)
		uint32_t layout_n;             // play: layout bytes
		alignas(4) uint8_t copy[NODE_BYTES];   // the node as drawn
		alignas(4) uint8_t frame[FR_SIZE];     // play: its frame (object flags, fade base)
		alignas(4) uint8_t layout[LAYOUT_MAX]; // play: the layout after the play
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH];
	};
	struct OvlRec
	{
		alignas(4) uint8_t st[OVL_HI - OVL_LO]; // light block + overlay state as drawn
		int16_t y, y_next;
		bool moves;
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static bool g_replaying = false;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;        // module packet cursor at the tick's first draw
	static Rec g_recs[512];
	static Rec *g_play = nullptr;         // the play being recorded (held_note_play -> held_note_played)
	static OvlRec g_ovl[4];
	static int g_novl = 0;
	static uint32_t g_fa_start = 0;       // frame arena cursor at the tick's first overlay draw
	static uint32_t g_fa_n = 0;
	static uint8_t g_fa_bytes[0x40000];   // the frame arena bytes the overlay draws wrote
	static uint8_t g_held_packets[0x20000]; // module arena (0x12000 per tick)
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_blend[0x8000];  // blended vertex frames of the replayed plays

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_play = nullptr;
		g_novl = 0;
		g_fa_start = 0;
		g_fa_n = 0;
	}

	static Rec *NewRec(uint32_t kind, uint32_t node)
	{
		if (g_replaying || g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = MEM<uint32_t>(PACKET_CURSOR);
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.moves = false;
		memcpy(r.copy, (const void *)node, NODE_BYTES);
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)MEM<uint32_t>(0x1D999C4), SCRATCH);
		return &r;
	}

	// a_743C20's end test: the next frame passes the last one
	static bool EndsNext(uint32_t node) { return (int16_t)(U16(node, 0x50) + 1) > S16(node, 0x52); }

	// the next tick's position of a sprite task (its own motion step) and whether that tick draws it
	static bool PredictSprite(uint32_t node, int16_t next[3])
	{
		next[0] = S16(node, 0x1C);
		next[1] = S16(node, 0x1E);
		next[2] = S16(node, 0x20);
		const int32_t s = S8(node, 0x29);
		switch (U32(node, 8))
		{
		case ORIG_Swirl:
		{
			// state 1 loops, state 2 plays the end sequence (a_743C00)
			if (s != 1 && s != 2) return false;
			if (s == 2 && EndsNext(node)) return false;
			int16_t r = (int16_t)(U16(node, 0x7C) + U16(node, 0x7E));
			if (r < 0) r = 0;
			const int32_t sn = mul32((int32_t)x::computeSin((uint32_t)(int32_t)S16(node, 0x42)), r);
			const int32_t cs = mul32((int32_t)x::computeCosine((uint32_t)(int32_t)S16(node, 0x42)), r);
			next[0] = (int16_t)(U16(node, 0x68) + (uint16_t)(sn / 4096));
			next[2] = (int16_t)(U16(node, 0x6C) + (uint16_t)(cs / 4096));
			next[1] = (int16_t)(U16(node, 0x1E) + (uint16_t)(U16(node, 0x5A) + 0xA));
			return next[1] <= 0;
		}
		case ORIG_Mote:
		{
			if (s != 1 || EndsNext(node)) return false;
			int16_t v = (int16_t)(U16(node, 0x5A) + U16(node, 0x62));
			if (v < 2) v = 2;
			next[1] = (int16_t)(U16(node, 0x1E) + (uint16_t)v);
			return next[1] <= 0;
		}
		case ORIG_Particle:
			if (s != 1 || EndsNext(node)) return false;
			next[1] = (int16_t)(U16(node, 0x1E) + U16(node, 0x5A));
			return next[1] <= 0;
		case ORIG_Sparkle:
			return s == 1 && !EndsNext(node); // no motion: only its spin moves
		}
		return false; // glow: no motion
	}

	static void held_note_sprite(uint32_t node)
	{
		Rec *r = NewRec(REC_SPRITE, node);
		if (!r) return;
		r->moves = PredictSprite(node, r->next);
	}

	static void held_note_ground(uint32_t node, uint32_t seq, uint32_t colour)
	{
		Rec *r = NewRec(REC_GROUND, node);
		if (!r) return;
		r->seq = seq;
		r->colour = U32(colour, 0);
		r->moves = PredictSprite(node, r->next);
	}

	static void held_note_spin(uint32_t node, uint32_t seq, uint32_t colour, uint32_t angle, uint32_t scale)
	{
		Rec *r = NewRec(REC_SPIN, node);
		if (!r) return;
		r->seq = seq;
		r->colour = U32(colour, 0);
		r->angle = angle;
		r->scale = scale;
		r->moves = PredictSprite(node, r->next);
	}

	// total frames of a prim-model layout (0x88A980: nothing drawn once the frame reaches it)
	static int32_t LayoutFrames(uint32_t layout)
	{
		const uint32_t data = U32(layout, 0);
		return S32(data + U32(data, 4), 4);
	}

	static void held_note_play(uint32_t node, uint32_t frame)
	{
		g_play = nullptr;
		const uint32_t layout = node + 0x60;
		if (S32(layout, 4) >= LayoutFrames(layout)) return; // draws nothing
		Rec *r = NewRec(REC_PLAY, node);
		if (!r) return;
		memcpy(r->frame, (const void *)frame, FR_SIZE);
		r->layout_n = U32(node, 8) == ORIG_Ring ? 0x34 : 0x64;
		g_play = r;
	}

	static void held_note_played(uint32_t node)
	{
		Rec *r = g_play;
		g_play = nullptr;
		if (!r || r->node != node) return;
		memcpy(r->layout, (const void *)(node + 0x60), r->layout_n);
		r->moves = S32(node + 0x60, 4) < LayoutFrames(node + 0x60);
	}

	static void held_note_overlay(uint32_t node)
	{
		if (g_replaying || g_log_tick != g_real_tick || g_novl >= (int)(sizeof(g_ovl) / sizeof(g_ovl[0]))) return;
		Rec *r = NewRec(REC_OVERLAY, node);
		if (!r) return;
		if (g_novl == 0) g_fa_start = MEM<uint32_t>(FRAME_CURSOR);
		r->seq = (uint32_t)g_novl;
		OvlRec &o = g_ovl[g_novl++];
		memcpy(o.st, (const void *)OVL_LO, sizeof(o.st));
		o.y = S16(node, 0x1E);
		o.y_next = (int16_t)(U16(node, 0x1E) + U16(node, 0x42));
		// the next tick rises it and draws it unless it ends there (counter + 1 >= 18)
		o.moves = S8(node, 0x29) == 1 && (int16_t)(U16(node, 0x24) + 1) < 0x12;
	}

	static void held_note_overlay_end()
	{
		if (g_replaying || g_log_tick != g_real_tick || !g_fa_start) return;
		uint32_t n = MEM<uint32_t>(FRAME_CURSOR) - g_fa_start;
		if (n > sizeof(g_fa_bytes)) n = sizeof(g_fa_bytes);
		memcpy(g_fa_bytes, (const void *)g_fa_start, n);
		g_fa_n = n;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.scratch, SCRATCH);
	}

	static int16_t lerp12(int16_t a, int16_t b, int num, int den)
	{
		const int32_t d = (((int32_t)b - a + 0x800) & 0xFFF) - 0x800; // 12-bit angles, the short way
		return (int16_t)((a + d * num / den) & 0xFFF);
	}

	// a prim-model play of the real tick in between: its anchor matrix rebuilt (live camera, ring
	// angles half way), then prim::play_held on the layout as the real play left it
	static void PlayHeld(const Rec &r, int num, int den)
	{
		alignas(4) static uint8_t fr[FR_SIZE];
		memcpy(fr, r.frame, FR_SIZE);
		const uint32_t f = P(fr), n = P(r.copy), m = f + FR_MATRIX;
		// the V scroll (0x88B200) rewrote the model on the real tick: the replay draws it as it is
		U32(f, FR_FLAGS) &= ~0x20000000u;
		if (U32(n, 8) == ORIG_Ring)
		{
			int16_t ang[4] = { S16(n, 0x50), S16(n, 0x52), S16(n, 0x54), 0 };
			if (r.moves)
			{
				ang[0] = lerp12(ang[0], (int16_t)((U16(n, 0x50) + U16(n, 0x58)) & 0xFFF), num, den);
				ang[2] = lerp12(ang[2], (int16_t)((U16(n, 0x54) + U16(n, 0x5C)) & 0xFFF), num, den);
			}
			x::ComposeZYXRotationMatrix(P(ang), m);
		}
		else
			x::MAG_022_sub_8DD770(m);
		S32(m, 0x14) = S16(n, 0x1C);
		S32(m, 0x18) = S16(n, 0x1E);
		S32(m, 0x1C) = S16(n, 0x20);
		x::scale3DMatrix(m, n + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		U32(f, FR_BLOCK) = m;
		U32(f, FR_BLOCK + 4) = f + FR_FLAGS;
		U32(f, FR_BLOCK + 8) = P(g_held_blend);
		Replay(r);
		const uint32_t layout = r.node + 0x60;
		alignas(4) uint8_t live[LAYOUT_MAX];
		memcpy(live, (const void *)layout, r.layout_n);
		memcpy((void *)layout, r.layout, r.layout_n);
		prim::play_held((prim::Layout *)layout, DrawCallback, (int)(f + FR_BLOCK), num, den);
		memcpy((void *)layout, live, r.layout_n);
	}

	// the overlay in between: 0x88BDA0 with the target at its in-between pose and the rising light
	// / lift offset half way (the overlay state as drawn, put back by the held frame)
	static void OverlayHeld(const Rec &r, int num, int den)
	{
		const OvlRec &o = g_ovl[r.seq];
		memcpy((void *)OVL_LO, o.st, sizeof(o.st));
		const uint32_t st = OVL_STATE;
		if (o.moves)
		{
			const int16_t y = magc::lerp16(o.y, o.y_next, num, den);
			S16(st, 0xB6) = y;
			S16(st, 0xDE) = y;
		}
		Replay(r);
		const uint32_t entity = U32(st, 8);
		OverlayMatrices(st, entity);
		const uint32_t anim = entity + 0x60;
		// (animation slot 0xFF: the overlay steps the animation itself - not on a held frame: the
		// pose holds)
		if (U16(st, 0xDA) != 0xFF)
			pose_midpoint((void *)anim, (void *)(entity + 0x6C), num, den);
		x::ComposeAffineTransform(0x1D97778, st + 0x14, st + 0x34);
		if ((U8(entity, 0) & 0x20) == 0)
			U32(U32(st, 0xC), 0) = x::sub_5088A0(entity, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, U32(U32(st, 0xC), 0));
		OverlayLight(st);
		RenderModel(U32(entity, 0x64), RenderOT44(), st);
		const uint32_t second = U32(entity, 0x78);
		if (second != 0)
			RenderModel(U32(second, 4), RenderOT44(), st);
		if (U16(st, 0xDA) != 0xFF)
			x::BattleModel_BuildBoneMatricesFromPose(anim);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		// everything the replayed draws write besides packets is put back: the overlay's light
		// block / state and lit vertices, the shadow temporaries of 0x5088A0, a wide Field_Alloc
		// scratch (bone matrices from a pose), the frame arena cursor
		static uint8_t ovl[OVL_HI - OVL_LO], lit[Q_EMITTER - LIT_VERTICES], shadow[0x80], scratch[0x8000];
		const bool overlay = g_novl > 0;
		if (overlay)
		{
			memcpy(ovl, (const void *)OVL_LO, sizeof(ovl));
			memcpy(lit, (const void *)LIT_VERTICES, sizeof(lit));
		}
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(scratch, (const void *)fa, sizeof(scratch));
		const uint32_t frame_cursor = MEM<uint32_t>(FRAME_CURSOR);
		if (g_fa_n) memcpy(g_held_frame, g_fa_bytes, g_fa_n);
		MEM<uint32_t>(FRAME_CURSOR) = P(g_held_frame);
		{
			magc::HeldScope scope(MEM<uint32_t>(PACKET_CURSOR), g_held_packets, sizeof(g_held_packets), g_packets);
			g_replaying = true;
			alignas(4) static uint8_t copy[NODE_BYTES];
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				switch (r.kind)
				{
				case REC_SPRITE:
				case REC_GROUND:
				case REC_SPIN:
				{
					memcpy(copy, r.copy, NODE_BYTES);
					const uint32_t c = P(copy);
					uint32_t angle = r.angle;
					if (r.moves)
					{
						S16(c, 0x1C) = magc::lerp16(S16(c, 0x1C), r.next[0], num, den);
						S16(c, 0x1E) = magc::lerp16(S16(c, 0x1E), r.next[1], num, den);
						S16(c, 0x20) = magc::lerp16(S16(c, 0x20), r.next[2], num, den);
						angle = (r.angle + (uint32_t)(0x40 * num / den)) & 0xFFF; // sparkle spin (+0x40 per tick)
					}
					const uint32_t colour = r.colour;
					Replay(r);
					if (r.kind == REC_SPRITE) DrawSprite(c);
					else if (r.kind == REC_GROUND) DrawGroundSprite(c, r.seq, P(&colour));
					else DrawSpinSprite(c, r.seq, P(&colour), angle, r.scale);
					break;
				}
				case REC_PLAY: PlayHeld(r, num, den); break;
				case REC_OVERLAY: OverlayHeld(r, num, den); break;
				}
			}
			g_replaying = false;
		}
		MEM<uint32_t>(FRAME_CURSOR) = frame_cursor;
		memcpy((void *)fa, scratch, sizeof(scratch));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		if (overlay)
		{
			memcpy((void *)LIT_VERTICES, lit, sizeof(lit));
			memcpy((void *)OVL_LO, ovl, sizeof(ovl));
		}
	}
}
}

	static void register_mag025_held()
	{
		mark_held(act::cura::ORIG_Glow);
		mark_held(act::cura::ORIG_Swirl);
		mark_held(act::cura::ORIG_Mote);
		mark_held(act::cura::ORIG_Aura);
		mark_held(act::cura::ORIG_Ring);
		mark_held(act::cura::ORIG_Sparkle);
		mark_held(act::cura::ORIG_Overlay);
		mark_held(act::cura::ORIG_Particle);
		register_module_held(25, act::cura::HeldReady, act::cura::HeldFrame);
	}
}
