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

// 30 fps part of mag035_life.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag035_life.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Life sprite task updates THEN draws, so the node as drawn is the state of tick N; the held
// frame draws the state half way to tick N + 1, derived from the task's own deterministic update:
//   Sparkle (0x8653E0) - scale +0x68 half way to +0x68 + +0x6A (its position never changes).
//   Ring    (0x865270) - on its orbit: phase +0x42 + 0x80 * num / den, position = centre + (sin,
//                        cos) / +0x6C of that phase (the exact point of the arc), y - 0x40 * num / den,
//                        scale half way; its first draw (at the spawn point, tick counter 0) moves
//                        in a straight line to the first orbit point.
//   Fall    (0x865120) - y + +0x5A * num / den.
// Held (15 Hz): every flipbook frame (+0x50), the moment a sprite disappears (its last frame), the
// random spawns, and the anchors (target bones).
// The glints (Effect_Glint_Tick, drawn after all sprites: the master runs the glint queue last) are
// redrawn by the shared glint file's held part (glint::held_draw, the glints whose root is Life's
// master) into the same packet cursor.
// A real tick records every sprite draw in order with the GTE registers and the Field_Alloc scratch
// it started from (the draw header keeps the scratch bytes the code does not write); the held frame
// replays the log into a private copy of the module arena (magc::HeldScope). No Life task writes
// the camera (the setup's camera animation is the engine's).

namespace ff8fx
{
namespace act
{
namespace life
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t NODE = 0x70;

	struct Rec
	{
		uint32_t task;              // original task function (ORIG_Fall / ORIG_Ring / ORIG_Sparkle)
		uint8_t node[NODE];         // the node as drawn
		uint8_t gte[magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_packets = 0;          // the tick's arena (module packet cursor after the pick)
	static uint32_t g_master = 0;           // the master node (root of the effect's glints)
	static int g_nrec = 0;
	static Rec g_recs[512];
	static uint8_t g_held_packets[0x20000];  // module arena (0x2000 per tick)
	alignas(4) static uint8_t g_held_node[NODE];

	// master: the ported effect runs on this real tick (new draw log, the tick's arena)
	static void held_note_master(uint32_t node)
	{
		g_ported_tick = g_real_tick;
		g_master = node;
		g_nrec = 0;
		g_packets = MEM<uint32_t>(PACKET_CURSOR);
	}

	// a sprite task is about to draw (its draw is skipped when hidden, +0x26 bit2)
	static void held_note_sprite(uint32_t task, uint32_t node)
	{
		if (g_ported_tick != g_real_tick || (U8(node, 0x26) & 4) != 0) return;
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		Rec &r = g_recs[g_nrec++];
		r.task = task;
		memcpy(r.node, (const void *)node, NODE);
		memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
		memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
		memcpy(r.scratch, (const void *)MEM<uint32_t>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_035;
		{
			magc::HeldScope scope(MEM<uint32_t>(PACKET_CURSOR), g_held_packets, sizeof(g_held_packets), g_packets);
			for (int i = 0; i < g_nrec; i++)
			{
				const Rec &r = g_recs[i];
				// a replayed draw starts from the GTE state and the scratch of the real one
				memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
				memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
				memcpy((void *)MEM<uint32_t>(0x1D999C4), r.scratch, SCRATCH);
				memcpy(g_held_node, r.node, NODE);
				const uint32_t n = P(g_held_node);
				switch (r.task)
				{
				case ORIG_Fall:
					// FallStep: y += +0x5A per tick
					U16(n, 0x1E) = (uint16_t)(S16(n, 0x1E) + S16(n, 0x5A) * num / den);
					DrawFall(n);
					break;
				case ORIG_Ring:
				{
					// RingStep: phase += 0x80, position on the orbit, y -= 0x40; the task: scale += +0x6A
					const int16_t x0 = S16(n, 0x1C), z0 = S16(n, 0x20);
					if (U16(n, 0x24) == 0)
					{
						// first draw at the spawn point: straight to the first orbit point
						const int32_t a1 = (U16(n, 0x42) + 0x80) & 0xFFF;
						const int16_t x1 = (int16_t)(U16(n, 0x60) + (uint16_t)((int32_t)x::computeSin((uint32_t)a1) / (int32_t)S16(n, 0x6C)));
						const int16_t z1 = (int16_t)(U16(n, 0x64) + (uint16_t)((int32_t)x::computeCosine((uint32_t)a1) / (int32_t)S16(n, 0x6C)));
						U16(n, 0x1C) = (uint16_t)magc::lerp16(x0, x1, num, den);
						U16(n, 0x20) = (uint16_t)magc::lerp16(z0, z1, num, den);
					}
					else
					{
						const int32_t ah = (U16(n, 0x42) + 0x80 * num / den) & 0xFFF;
						U16(n, 0x1C) = (uint16_t)(U16(n, 0x60) + (uint16_t)((int32_t)x::computeSin((uint32_t)ah) / (int32_t)S16(n, 0x6C)));
						U16(n, 0x20) = (uint16_t)(U16(n, 0x64) + (uint16_t)((int32_t)x::computeCosine((uint32_t)ah) / (int32_t)S16(n, 0x6C)));
					}
					U16(n, 0x1E) = (uint16_t)(S16(n, 0x1E) - 0x40 * num / den);
					U16(n, 0x68) = (uint16_t)(S16(n, 0x68) + S16(n, 0x6A) * num / den);
					DrawScaled(n, U16(n, 0x68));
					break;
				}
				case ORIG_Sparkle:
					// the task: scale += +0x6A
					U16(n, 0x68) = (uint16_t)(S16(n, 0x68) + S16(n, 0x6A) * num / den);
					DrawScaled(n, U16(n, 0x68));
					break;
				}
			}
			// the glint queue runs after the sprite queue (shared glint task: this effect's glints)
			glint::held_draw((const void *)g_master, num, den);
		}
		g_mod = saved_mod;
	}
}
}

	static void register_mag035_held()
	{
		mark_held(act::life::ORIG_Fall);
		mark_held(act::life::ORIG_Ring);
		mark_held(act::life::ORIG_Sparkle);
		register_module_held(35, act::life::HeldReady, act::life::HeldFrame);
	}
}
