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

// 30 fps part of mag076_ultimecia_final_form_spawn.cpp: the draw log of the real tick and the
// held-frame drawing (included at the end of the vanilla file; shared pieces in mag_common_held.h /
// fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records each drawing task's call in order (just
// before its draw header is allocated, after its spawns): the node, the particle pool as drawn, the
// anchor (Halo / Flash1), the GTE registers and the Field_Alloc scratch (its matrix block and the
// header bytes the draws do not write), and whether the tick updated (not the draw-only mode).
// A held frame replays the log in the same order with the same draw code, each part moved num / den
// of the way to the state its own update leaves (the task's update run on a copy):
//   Burst      - position (it accelerates: the chord between the two draws)
//   Spin       - spin angle about its direction (exact angle step)
//   Rise       - y scale
//   Burst2     - y scale, spin angle about its direction; parts still waiting (delay) draw nothing
//   Debris     - position, x and y / z angles (two-axis turn, exact angle steps)
//   Halo       - scale (from counter 0x158); the fade-in ramp; the even / odd flicker holds
//   Flash1/2   - scale; Ring - z angle and scale
// Fades (header +0x0C toward the far colour) move along their ramp when both ticks use the fade
// flags, else hold; anchors hold (Halo / Flash1 follow the caster's effect bone, engine pose).
// A part that ends on the tick (dies, or its task ends) holds its last draw; at fraction 1 it is
// gone and every other part is drawn as the next tick draws it. In the draw-only mode nothing moved:
// everything holds. Packets go to private buffers (module arena: HeldScope; frame packets:
// FrameScope); the module arena is taken at the tick's first module draw (it alternates by tick).

namespace ff8fx
{
namespace uff076
{
	static const uint32_t SCRATCH = 0x100;
	static const uint32_t POOL1_SIZE = POOL1_END - POOL1, POOL2_SIZE = POOL2_END - POOL2;

	struct Rec
	{
		uint32_t fn;
		Node node;
		int16_t pos[4];
		bool moving;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
		uint8_t pool[POOL2_SIZE];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static bool g_overflow = false;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first module draw
	static uint32_t g_frame_packets = 0; // frame packet cursor at the tick's first frame draw
	static Rec g_recs[16];
	static uint8_t g_held_packets[0xC000];
	static uint8_t g_held_frame[0x40000];
	static bool g_held = false;
	static struct { bool on; uint32_t flags; int32_t fade; } g_fo;

	static bool FramePackets(uint32_t fn) { return fn == ORIG_Burst || fn == ORIG_Spin || fn == ORIG_Debris; }
	static uint32_t PoolOf(uint32_t fn)
	{
		if (fn == ORIG_Burst || fn == ORIG_Spin || fn == ORIG_Burst2) return POOL1;
		if (fn == ORIG_Rise || fn == ORIG_Debris) return POOL2;
		return 0;
	}

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_overflow = false;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static void held_note_task(uint32_t fn, TaskNode *node, uint32_t scratch, const int16_t *pos)
	{
		if (g_held || g_log_tick != g_real_tick) return;
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0])))
		{
			g_overflow = true;
			return;
		}
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = *(const Node *)node;
		if (pos) memcpy(r.pos, pos, sizeof(r.pos));
		r.moving = !DrawOnly();
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)(scratch ? scratch : var<uint32_t>(0x1D999C4)), SCRATCH);
		const uint32_t pool = PoolOf(fn);
		if (pool) memcpy(r.pool, (const void *)pool, pool == POOL1 ? POOL1_SIZE : POOL2_SIZE);
		if (FramePackets(fn)) { if (!g_frame_packets) g_frame_packets = FrameCursor(); }
		else if (!g_packets) g_packets = PacketCursor();
	}

	// held frame: the header fade chosen for the in-between state
	static void held_fade(uint32_t h)
	{
		if (!g_held || !g_fo.on) return;
		U32(h, 0x1C) = g_fo.flags;
		if (g_fo.flags == FADE) S32(h, 0xC) = g_fo.fade;
	}

	// the fade between two draws: along the ramp when both use the fade flags, else the first's;
	// at fraction 1 the second's
	static void PickFade(uint32_t fa, int32_t a, uint32_t fb, int32_t b, int num, int den)
	{
		g_fo.on = true;
		if (num == den) { g_fo.flags = fb; g_fo.fade = b; return; }
		g_fo.flags = fa;
		g_fo.fade = a;
		if (fa == FADE && fb == FADE) g_fo.fade = lerp_i(a, b, num, den);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick && !g_overflow; }

	// frame packets of a held frame go to a private copy of the real tick's (pad bytes as drawn)
	struct FrameScope
	{
		uint32_t saved;
		FrameScope()
		{
			saved = FrameCursor();
			if (g_frame_packets && saved > g_frame_packets)
			{
				const uint32_t used = saved - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = P(g_held_frame);
		}
		~FrameScope() { FrameCursor() = saved; }
	};

	// ------------------------------------------------------------------
	// pool tasks
	// ------------------------------------------------------------------
	struct PoolKind
	{
		uint32_t fn, pool, size;
		uint8_t bit;
		int32_t block, header;            // Field_Alloc sizes (matrix block, draw header)
		uint32_t model;
		int32_t base_y, life;
		void (*move)(uint32_t e);
		void (*draw)(uint32_t s, uint32_t h, uint32_t e);
		uint32_t (*fade)(int32_t age, int32_t *fade);
		int8_t lerp[6];                   // int16 fields moved toward the next draw (-1 = end)
	};

	static const PoolKind KINDS[] = {
		{ ORIG_Burst, POOL1, POOL1_SIZE, 1, 0x78, 0x58, MODEL_Burst, 0xFA0, 0x18, BurstMove, BurstDraw, BurstFade, { 8, 0xA, 0xC, -1 } },
		{ ORIG_Spin, POOL1, POOL1_SIZE, 2, 0x78, 0x58, MODEL_Spin, 0xFA0, 0x18, SpinMove, SpinDraw, SpinFade, { 0x14, -1 } },
		{ ORIG_Rise, POOL2, POOL2_SIZE, 1, 0x98, 0x58, MODEL_Rise, 0xBB8, 0x14, RiseMove, RiseDraw, RiseFade, { 0x12, -1 } },
		{ ORIG_Burst2, POOL1, POOL1_SIZE, 4, 0x78, 0x58, MODEL_Spin, 0, 0x46, Burst2Move, Burst2Draw, Burst2Fade, { 0x12, 0x14, -1 } },
		{ ORIG_Debris, POOL2, POOL2_SIZE, 1, 0x68, 0x5C, MODEL_Debris, 0, 0x20, DebrisMove, DebrisDraw, DebrisFade, { 8, 0xA, 0xC, 0x18, 0x1C, -1 } },
	};

	static void HeldPool(const Rec &r, const PoolKind &k, int num, int den)
	{
		const bool one = num == den;
		const uint32_t s = P(FieldAlloc(k.block));
		const uint32_t h = P(FieldAlloc(k.header));
		U32(h, 0) = k.model;
		U32(h, 8) = 0;
		BaseMatrix(s, k.base_y);
		if (k.fn == ORIG_Burst) S32(s, 0x70) = 0;
		else if (k.fn == ORIG_Spin) S32(s, 0x6C) = 0x6000;
		else if (k.fn == ORIG_Rise)
		{
			S16(s, 4) = 0;
			S16(s, 0) = 0;
			S32(s, 0x90) = 0;
		}
		for (uint32_t o = 0; o < k.size; o += 0x20)
		{
			const uint32_t a = P(r.pool) + o;
			if (!(U8(a, 0) & k.bit)) continue;
			if (k.fn == ORIG_Burst2 && S16(a, 6) != 0) continue; // still waiting: not drawn
			uint8_t nb[0x20], cb[0x20];
			memcpy(cb, (const void *)a, sizeof(cb));
			const uint32_t c = P(cb), b = P(nb);
			const bool ends = S16(a, 4) >= k.life;
			int32_t fa = 0, fb = 0;
			const uint32_t fla = k.fade(S16(a, 4), &fa);
			if (r.moving && !ends)
			{
				memcpy(nb, (const void *)a, sizeof(nb));
				k.move(b);
				const uint32_t flb = k.fade(S16(b, 4), &fb);
				if (one) memcpy(cb, nb, sizeof(cb));
				else
					for (int i = 0; i < 6 && k.lerp[i] >= 0; i++)
						S16(c, k.lerp[i]) = lerp16(S16(a, k.lerp[i]), S16(b, k.lerp[i]), num, den);
				PickFade(fla, fa, flb, fb, num, den);
			}
			else
			{
				if (one && r.moving) continue; // dies on this tick: not on the next one
				PickFade(fla, fa, fla, fa, 0, 1);
			}
			k.draw(s, h, c);
		}
		FieldFree(k.header);
		FieldFree(k.block);
	}

	// ------------------------------------------------------------------
	// single-node parts
	// ------------------------------------------------------------------
	static bool Flicker(int32_t c) { return c >= 6 && c < 0x158; }

	static void HeldNode(const Rec &r, int num, int den)
	{
		const bool one = num == den;
		Node a = r.node, b = r.node, c = r.node;
		int32_t end = 0;
		void (*move)(Node *) = nullptr;
		uint32_t (*fade)(int32_t, int32_t *) = nullptr;
		switch (r.fn)
		{
		case ORIG_Halo: end = 0x168; move = HaloMove; break;
		case ORIG_Flash1: end = 0x1A; move = Flash1Move; fade = Flash1Fade; break;
		case ORIG_Flash2: end = 0x14; move = Flash2Move; fade = Flash2Fade; break;
		case ORIG_Ring: end = 0x4F; move = RingMove; fade = RingFade; break;
		default: return;
		}
		const bool moves = r.moving && a.counter + 1 < end;
		if (r.moving && !moves && one) return; // the task ends on this tick
		if (moves)
		{
			move(&b);
			if (one) c = b;
			else
			{
				c.scale = lerp16(a.scale, b.scale, num, den);
				c.angle = lerp16(a.angle, b.angle, num, den);
			}
		}
		if (r.fn == ORIG_Halo)
		{
			int32_t fa = HaloFade(a.counter), fb = moves ? HaloFade(b.counter) : fa;
			if (moves && Flicker(a.counter) && Flicker(b.counter) && !one) fb = fa; // the flicker holds
			PickFade(FADE, fa, FADE, fb, moves ? num : 0, moves ? den : 1);
			HaloDraw(&c, r.pos);
			return;
		}
		int32_t fa = 0, fb = 0;
		const uint32_t fla = fade(a.counter, &fa);
		const uint32_t flb = moves ? fade(b.counter, &fb) : fla;
		if (!moves) fb = fa;
		PickFade(fla, fa, flb, fb, moves ? num : 0, moves ? den : 1);
		if (r.fn == ORIG_Flash1) Flash1Draw(&c, r.pos);
		else if (r.fn == ORIG_Flash2) Flash2Draw(&c);
		else RingDraw(&c);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		// at fraction 1 the module packets go where the next tick writes them: the other arena, whose
		// bytes the packets do not write are the ones the next tick finds there
		if (num == den && g_packets && TexBase()) memcpy(g_held_packets, (const void *)(g_packets == TexBase() ? TexBase() + 0xC000 : TexBase()), sizeof(g_held_packets));
		g_held = true;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			bool pool = false;
			for (const PoolKind &k : KINDS)
				if (k.fn == r.fn)
				{
					HeldPool(r, k, num, den);
					pool = true;
				}
			if (!pool) HeldNode(r, num, den);
		}
		g_fo.on = false;
		g_held = false;
	}
}

	static void register_mag076_held()
	{
		mark_held(uff076::ORIG_Burst);
		mark_held(uff076::ORIG_Spin);
		mark_held(uff076::ORIG_Rise);
		mark_held(uff076::ORIG_Halo);
		mark_held(uff076::ORIG_Flash1);
		mark_held(uff076::ORIG_Flash2);
		mark_held(uff076::ORIG_Ring);
		mark_held(uff076::ORIG_Burst2);
		mark_held(uff076::ORIG_Debris);
		register_module_held(76, uff076::HeldReady, uff076::HeldFrame);
	}
}
