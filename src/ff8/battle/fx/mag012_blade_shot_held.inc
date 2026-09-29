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

// 30 fps part of mag012_blade_shot.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag012_blade_shot.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in order (root draws, then the trail and particle queues): the node
// as drawn, the GTE state the draw starts from and the scratch under its header. A held frame
// replays the log in the same order, each draw at the exact in-between state:
//   blade: position and rotation half way to the next tick's (the root's own next step run on a
//     copy: flight out, the turn, flight back; the hand anchor fetched on the copy with the engine's
//     scratch put back - it holds the caster's current pose); after its last draw it holds;
//   impact / flash sprites: fixed position, flipbook frames hold; catch sprite: follows the
//     in-between blade;
//   rings and wing pairs: expansion s half way (scale, push-out, fade and morph weight follow; the
//     morph goes into the model's vertex table and the real tick's table is put back); at their
//     last draw they hold;
//   particles / glints / streaks: position half way along the next step (velocity + acceleration),
//     the streak's screen direction follows; flipbook frames hold;
//   after-images: fade angle half way.
// Spawns (random) stay at 15 Hz. No Blade Shot task has a draw-only mode.

namespace ff8fx
{
namespace blade012
{
	static const int MAXREC = 160;
	static const uint32_t FAR_COLOR = 0x1CA92D0, FAR_COLOR_SIZE = 12; // GTE RFC / GFC / BFC (control 21..23)

	struct Rec
	{
		int kind;
		int32_t k;                 // root draws: the counter parameter
		uint8_t node[0x64];        // the node as drawn
		uint8_t hdr[0xB4];         // scratch under the draw header
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static Rec g_recs[MAXREC];
	static uint8_t g_held_arena[0x10000];
	static RootNode *g_root = nullptr;
	static bool g_mid_valid = false;        // the in-between blade of this held frame
	static int16_t g_mid_blade[3];

	static void held_note_root(RootNode *n)
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_root = n;
	}

	static Rec *held_add(int kind, const void *node, int size, int32_t k)
	{
		g_hdr_slot = nullptr;
		if (g_log_tick != g_real_tick || g_nrec >= MAXREC) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.k = k;
		memcpy(r.node, node, size);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		return &r;
	}

	// a task is about to allocate its header and draw
	static void held_note(int kind, const void *node, int size)
	{
		Rec *r = held_add(kind, node, size, 0);
		if (r) g_hdr_slot = r->hdr;
	}

	// a root draw whose header is already allocated (its bytes are still the scratch's)
	static void held_note_h(int kind, const RootNode *n, const uint8_t *h, int hsize, int32_t k)
	{
		Rec *r = held_add(kind, n, sizeof(RootNode), k);
		if (r) memcpy(r->hdr, h, hsize);
	}

	// a ring / wing draw of parameter k (0..19) is about to allocate its header
	static void held_note_k(int kind, const RootNode *n, int32_t k)
	{
		if ((uint32_t)k >= 0x14) return;
		Rec *r = held_add(kind, n, sizeof(RootNode), k);
		if (r) g_hdr_slot = r->hdr;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the caster's hand anchor (what the next tick fetches): the engine call has no other effect
	// than the GTE, the Field_Alloc scratch and the word 0x1D9770C, the last two put back here (the
	// GTE by the replay)
	static void HeldAnchor(uint8_t *entity, int16_t *out)
	{
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		uint8_t save[0x100];
		memcpy(save, scratch, sizeof(save));
		const int16_t w = var<int16_t>(0x1D9770C);
		GetEffectSpawnPosition(entity, 7, 0, out);
		var<int16_t>(0x1D9770C) = w;
		memcpy(scratch, save, sizeof(save));
	}

	// the blade as the next tick draws it (false: it does not draw it)
	static bool BladeNextTick(RootNode &nx)
	{
		if (!g_root) return false;
		nx = *g_root;
		if ((uint16_t)nx.counter < 0x20)
		{
			BladeOut(&nx);
			return true;
		}
		nx.rot[2] = 0;
		const int32_t k = (int32_t)nx.counter - 0x20;
		if ((uint32_t)k >= 0x1E) return false;
		if (k == 0 && nx.turned == 0) BladeTurn(&nx);
		HeldAnchor(Caster(), nx.mot.end);
		BladeBack(&nx);
		return true;
	}

	// ring / wing expansion half way to the next tick's (held at the last one)
	static uint32_t HeldScale(int32_t k, int num, int den)
	{
		const uint32_t s0 = RingScale(k);
		if ((uint32_t)(k + 1) >= 0x14) return s0;
		return (uint32_t)lerp_i((int32_t)s0, (int32_t)RingScale(k + 1), num, den);
	}

	static void HeldWing(const RootNode *n, uint32_t s, uint32_t model, uint32_t k0, uint32_t k1, int16_t yaw)
	{
		uint8_t save[0x200];
		const uint32_t bytes = *(const uint32_t *)(model + 4) * 8;
		const uint32_t keep = bytes < sizeof(save) ? bytes : (uint32_t)sizeof(save);
		memcpy(save, (const void *)(model + 8), keep);
		WingPair(n, s, model, k0, k1, yaw);
		memcpy((void *)(model + 8), save, keep);
	}

	// the next position of a particle (velocity += acceleration, position += velocity), num/den of the way
	static void HeldPartMove(PartNode &c, int num, int den)
	{
		for (int i = 0; i < 3; i++)
		{
			const int16_t v = (int16_t)(c.vel[i] + c.acc[i]);
			c.pos[i] = lerp16(c.pos[i], (int16_t)(c.pos[i] + v), num, den);
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, sizeof(g_held_arena), g_packets);
		g_mid_valid = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			RootNode c;
			memcpy(&c, r.node, sizeof(c));
			if (r.kind == K_BLADE)
			{
				// the next tick's blade (engine math on a copy) before the replay's GTE state
				RootNode nx;
				if (BladeNextTick(nx))
				{
					for (int j = 0; j < 3; j++)
					{
						c.blade[j] = lerp16(c.blade[j], nx.blade[j], num, den);
						c.rot[j] = lerp16(c.rot[j], nx.rot[j], num, den);
					}
				}
				g_mid_valid = true;
				memcpy(g_mid_blade, c.blade, sizeof(g_mid_blade));
			}
			// a replayed draw starts from the GTE state and the header scratch of the real one, except
			// the far colour after the first draw: it carries over from the previous replayed draw as it
			// does in the real tick (the after-image quads depth-cue towards the far colour of the last
			// triangle inserted before them, often another after-image's)
			uint8_t fc[FAR_COLOR_SIZE];
			memcpy(fc, (const void *)FAR_COLOR, FAR_COLOR_SIZE);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			if (i > 0) memcpy((void *)FAR_COLOR, fc, FAR_COLOR_SIZE);
			g_hdr_slot = (uint8_t *)r.hdr;
			switch (r.kind)
			{
			case K_BLADE:
			{
				uint8_t *h = AllocHeader(0x58);
				BladeDraw(&c, h);
				FieldFree(0x58);
				break;
			}
			case K_HIT:
			case K_FLASH:
			{
				uint8_t *h = AllocHeader(0xB4);
				SeqDraw(c.hit, h, r.kind == K_HIT ? SEQ_Hit : SEQ_Flash, (int16_t)r.k);
				FieldFree(0xB4);
				break;
			}
			case K_CATCH:
			{
				uint8_t *h = AllocHeader(0xB4);
				int16_t pos[4];
				memcpy(pos, g_mid_valid ? g_mid_blade : c.blade, 6);
				pos[1] = (int16_t)(pos[1] + 0x64);
				pos[3] = 0;
				SeqDraw(pos, h, SEQ_Hit, (int16_t)r.k);
				FieldFree(0xB4);
				break;
			}
			case K_RING:
				RingDraw(&c, HeldScale(r.k, num, den));
				break;
			case K_WING_A:
				HeldWing(&c, HeldScale(r.k, num, den), MODEL_WingA, KEYS_WingA0, KEYS_WingA1, 0x71);
				break;
			case K_WING_B:
				HeldWing(&c, HeldScale(r.k, num, den), MODEL_WingB, KEYS_WingB0, KEYS_WingB1, 0x11C);
				break;
			case K_PART:
			case K_STREAK:
			{
				PartNode p;
				memcpy(&p, r.node, sizeof(p));
				HeldPartMove(p, num, den);
				uint8_t *h = AllocHeader(0xB4);
				if (r.kind == K_PART) PartDraw(&p, h);
				else StreakDraw(&p, h);
				FieldFree(0xB4);
				break;
			}
			case K_TRAIL:
			{
				TrailNode t;
				memcpy(&t, r.node, sizeof(t));
				const int32_t a = lerp_i(TrailAngle(t.counter), TrailAngle((int16_t)(t.counter + 1)), num, den);
				uint8_t *h = AllocHeader(0x58);
				TrailDraw(&t, h, a);
				FieldFree(0x58);
				break;
			}
			}
			g_hdr_slot = nullptr;
		}
	}
}

	static void register_mag012_held()
	{
		mark_held(blade012::ORIG_RootTask);
		mark_held(blade012::ORIG_PartTask);
		mark_held(blade012::ORIG_StreakTask);
		mark_held(blade012::ORIG_TrailTask);
		register_module_held(12, blade012::HeldReady, blade012::HeldFrame);
	}
}
