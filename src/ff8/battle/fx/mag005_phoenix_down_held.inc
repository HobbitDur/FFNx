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

// 30 fps part of mag005_phoenix_down.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag005_phoenix_down.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// (Life's held part, mag035_life_held.inc, relocated.) Every Phoenix Down sprite task updates THEN
// draws, so the node as drawn is the state of tick N; the held
// frame draws the state half way to tick N + 1, derived from the task's own deterministic update:
//   Sparkle (0x8D4E60) - scale +0x68 half way to +0x68 + +0x6A (its position never changes).
//   Ring    (0x8D4CF0) - on its orbit: phase +0x42 + 0x80 * num / den, position = centre + (sin,
//                        cos) / +0x6C of that phase (the exact point of the arc), y - 0x40 * num / den,
//                        scale half way; its first draw (at the spawn point, tick counter 0) moves
//                        in a straight line to the first orbit point.
//   Fall    (0x8D4BA0) - y + +0x5A * num / den.
// Held (15 Hz): every flipbook frame (+0x50), the moment a sprite disappears (its last frame), the
// random spawns, and the anchors (target bones).
// The glints (Effect_Glint_Tick, drawn after all sprites: the master runs the glint queue last) are
// redrawn by the shared glint file's held part (glint::held_draw, the glints whose root is
// Phoenix Down's master) into the same packet cursor, their position half way to the next tick's: the line anchor
// and bounds height their emitter takes from the target's bones, computed by the effect library's own
// functions on a copy of the emitter with the target at its next pose (NextGlintPos).
// A real tick records every sprite draw in order with the GTE registers and the Field_Alloc scratch
// it started from (the draw header keeps the scratch bytes the code does not write); the held frame
// replays the log into a private copy of the module arena (magc::HeldScope). No Phoenix Down task
// writes the camera (its setup plays no camera animation).

namespace ff8fx
{
namespace act
{
namespace phoenixdown
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

	// the glints' position on the next tick (glint::held_draw's next_pos): the glint task takes it from
	// its line's emitter (node +0x14: Phoenix Down's emitter 0x8D4640, whose glints they are) through
	// Effect_CopyMidpointFromSource 0x8DC700 and Effect_GetTargetBoundsCenterWorldY 0x8DCB80; the emitter
	// task updates it first on every tick from the target's bones (MAG_001_CURE_Emitter_UpdatePos 0x8DC610
	// / _ComputeModelBounds 0x8DC870). Here the same functions run on a copy of the emitter with the target
	// at its next pose (pose_midpoint at 1), then the real pose is rebuilt and the word
	// GetEffectSpawnPosition writes (0x1D9770C) put back; once per emitter and held frame.
	static const uint32_t EMITTER_NODE = 0x64;
	struct NextAnchor { uint32_t emitter; bool ok; alignas(4) uint8_t next[EMITTER_NODE]; };
	static NextAnchor g_next[8];
	static int g_nnext = 0;
	static bool NextGlintPos(const void *node, int16_t pos[3])
	{
		const uint32_t e = U32(P(node), 0x14);
		if (!e) return false;
		NextAnchor *na = nullptr;
		for (int i = 0; i < g_nnext; i++) if (g_next[i].emitter == e) na = &g_next[i];
		if (!na)
		{
			if (g_nnext >= (int)(sizeof(g_next) / sizeof(g_next[0]))) return false;
			na = &g_next[g_nnext++];
			na->emitter = e;
			memcpy(na->next, (const void *)e, EMITTER_NODE);
			const uint32_t entity = 0x1D972C0 + 0x9C * (uint32_t)U8(e, 0x2D);
			na->ok = U32(entity, 0x64) != 0;
			if (na->ok)
			{
				const uint16_t w70c = MEM<uint16_t>(0x1D9770C);
				pose_midpoint((void *)(entity + 0x60), (void *)(entity + 0x6C), 1, 1);
				x::MAG_001_CURE_Emitter_UpdatePos(P(na->next));
				x::MAG_001_CURE_Emitter_ComputeModelBounds(P(na->next));
				x::BattleModel_BuildBoneMatricesFromPose(entity + 0x60);
				MEM<uint16_t>(0x1D9770C) = w70c;
			}
		}
		if (!na->ok) return false;
		alignas(4) uint8_t g[0x5C];
		memcpy(g, node, sizeof(g));
		U32(P(g), 0x14) = P(na->next);
		alignas(4) int16_t mid[4];
		fn<void (__cdecl *)(uint32_t, uint32_t)>(0x8DC700)(P(g), P(mid));               // Effect_CopyMidpointFromSource
		pos[0] = mid[0];
		pos[1] = (int16_t)fn<int32_t (__cdecl *)(uint32_t)>(0x8DCB80)(P(g));             // Effect_GetTargetBoundsCenterWorldY
		pos[2] = mid[2];
		return true;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_005;
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
			g_nnext = 0;
			glint::held_draw((const void *)g_master, num, den, NextGlintPos);
		}
		g_mod = saved_mod;
	}
}
}

	static void register_mag005_held()
	{
		mark_held(act::phoenixdown::ORIG_Fall);
		mark_held(act::phoenixdown::ORIG_Ring);
		mark_held(act::phoenixdown::ORIG_Sparkle);
		register_module_held(5, act::phoenixdown::HeldReady, act::phoenixdown::HeldFrame);
	}
}
