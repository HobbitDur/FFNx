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

// 30 fps part of mag085_the_end.cpp: the held-frame drawing (included at the end of mag085_the_end.cpp)
//
// Every drawing task of The End updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw / held_note_system / held_note_tints: the node as drawn, the GTE registers, the module scratch stack and
// the Field_Alloc scratch it started from); the held frame redraws them in the same order, each at tick + num/den, from
// copies of the nodes moved half way to the next tick's state (the task's own update run on a copy: NextNode; no draw, no
// spawn, no sound, no music). The director's cut test of the next tick (0x73B7E0) is its requested cut now (+0x42: the
// director runs first in the master's queues and makes it current at the start of its next run).
//   Prim plays   - the cut-3 play (0x785560), the two orbiting plays (0x787B50) and the meteor impacts (0x788A90): the
//                  module's prim-model play (0x785660 = 0x73C280) redrawn by prim::play_held (objects half way to their
//                  next keyframe records) with the parameter block built from the node's placement and the frame's camera;
//                  an orbiting play's position (+0x1C..) half way to its next orbit step (the yaw +0x62 is constant).
//   Shadows      - the battle entities' shadows the cut-3 play draws before each play (0x785620): redrawn as they are.
//   Meteors      - the falling models (0x788580, draw 0x788700): position, scale (from the height) and fade half way.
//   Petals       - the petal groups (0x78CFA0, four petals each, draw 0x788700): each petal's position, scale and the
//                  group's fade half way.
//   Ground       - the ground model (0x78C4C0, renderer 0x78C660): its fade level +0xE0 half way (grey levels, steps of 1).
//   Glow         - the glow quad (0x78D250 states, 0x78D2E0): width +0x58 and right level +0x5A half way.
//   Letterbox    - the black bars (0x78D410 states, 0x78D4D0): height +0x1E half way.
//   Actor systems- the impacts' actor system (0x788B90, step 0x789700): the act-library actor template (as Mega Phoenix /
//                  X-Potion: every drawn actor from a copy half way to its next motion step, the view of the next tick
//                  from the live camera; flipbook frame, model, fade and colours hold).
//   Screen tint  - the engine task 0x8DDC30 of the last queue (0x25C19D8): the four quads with the colour half way to
//                  the next script step's.
// A part the next tick does not draw (its task finishes, a cut ends it, a meteor reaches the ground, a play's last frame)
// holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not drawn. At 1 every part is the
// next tick's draw (prim objects that appear / disappear on the next tick do so on the real frame: prim::play_held).
// Camera: every part is placed through the frame's camera (0x1D97778, the frame gate's: the next tick's view at 1); the
// actor systems through the module's camera copy set to the next tick's view for their predictions. Not drawn by the
// module and held at 15 Hz by the engine (as the real ticks set them): the stage fade / wobble levels of the stage draw
// entries, the battle entities' colour mode, the music volume, the battle UI switch.
// The packets go to private copies of the three packet arenas (module cursors 0x25BC394 / 0x25BC49C, frame arena 0x1D8E054)
// that start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc
// scratch, the shadow / RenderGeometry temporaries, the actor data's morph vertex blocks, the CRT rand seed and the GTE
// registers are put back.

namespace ff8fx
{
namespace act
{
namespace theend
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25BC390, BSS_HI = 0x25C2350;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x25BC394, CUR_B = 0x25BC49C, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_Play = 0x785560, ORIG_Orbit = 0x787B50, ORIG_Impact = 0x788A90, ORIG_Meteor = 0x788580;
	static const uint32_t ORIG_ActorSystem = 0x788B90, ORIG_Ground = 0x78C4C0, ORIG_Petals = 0x78CFA0, ORIG_Glow = 0x78D250;
	static const uint32_t ORIG_Bars = 0x78D410, ORIG_Tint = 0x8DDC30;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_MAX = 0x2E0; // the largest node a draw reads (prim-play tasks 0x2DC)

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a kind-5 model's step
	// rewrites them in the actor data, which every actor system of the cast shares
	struct VertexBlocks
	{
		uint32_t n;
		uint32_t addr[16], len[16];
		uint8_t bytes[0x4000];
		void save(uint32_t sys)
		{
			n = 0;
			const uint32_t tab = U32(sys, 0x22C);
			if (!tab) return;
			uint32_t used = 0;
			for (int k = 0; k < 16; k++)
			{
				const uint32_t blk = U32(tab, k * 4);
				if (!blk) continue;
				const uint32_t bytes_n = U32(blk, 4) * 8; // vertex count +4, 8-byte vertices from +8
				if (bytes_n == 0 || used + bytes_n > sizeof(bytes)) continue;
				addr[n] = blk + 8;
				len[n] = bytes_n;
				memcpy(bytes + used, (const void *)(blk + 8), bytes_n);
				used += bytes_n;
				n++;
			}
		}
		void restore() const
		{
			uint32_t used = 0;
			for (uint32_t i = 0; i < n; i++)
			{
				memcpy((void *)addr[i], bytes + used, len[i]);
				used += len[i];
			}
		}
	};

	// one draw of the real tick (execution order)
	struct Rec
	{
		int kind;
		uint32_t node;              // the task node (HK_SYSTEM: the actor state block; HK_TINT: the tint node)
		int sub;                    // HK_MODEL: the k-th draw of the node on this tick (a petal group's petal k)
		uint8_t snap[NODE_MAX];     // the node as drawn
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int sysi;                   // HK_SYSTEM: its vertex-block note
	};
	static const int MAX_RECS = 320, MAX_SYSTEMS = 8;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS];
	static int g_nsys = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[3], g_end[3];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[3] = { CUR_A, CUR_B, CUR_FA };
	static uint8_t g_priv[3][0x40000];
	static uint8_t g_cam_tick[0x20];                       // the module's camera copy this real tick drew with
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_Play || f == ORIG_Orbit || f == ORIG_Impact) return 0x2DC;
		if (f == ORIG_Meteor || f == ORIG_Ground || f == ORIG_Petals) return 0xE4;
		return 0x60; // glow / letterbox
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = g_nsys = 0;
		g_full = false;
		for (int i = 0; i < 3; i++) g_start[i] = g_end[i] = MEM<uint32_t>(CURSORS[i]);
		memcpy(g_cam_tick, (const void *)CAM_COPY, sizeof(g_cam_tick));
	}

	static Rec *new_rec(int kind, uint32_t node)
	{
		if (g_in_held) return nullptr;
		begin_tick();
		if (g_nrec >= MAX_RECS) { g_full = true; return nullptr; }
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.sub = 0;
		r.sysi = -1;
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)SCRATCH_LO, sizeof(r.scratch));
		memcpy(r.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(r.falloc));
		return &r;
	}

	static void held_note_draw(int kind, uint32_t node)
	{
		Rec *r = new_rec(kind, node);
		if (!r) return;
		memcpy(r->snap, (const void *)node, node_size(node));
		if (kind == HK_MODEL)
			for (int i = 0; i < g_nrec - 1; i++)
				if (g_rec[i].kind == HK_MODEL && g_rec[i].node == node) r->sub++;
	}

	static void held_note_system(uint32_t sys)
	{
		Rec *r = new_rec(HK_SYSTEM, sys);
		if (!r || g_nsys >= MAX_SYSTEMS) { if (r) { g_nrec--; g_full = true; } return; }
		r->sysi = g_nsys;
		g_sys_verts[g_nsys++].save(sys);
	}

	// the tints that draw on this tick: state 1 (0x8DDCD0) with a script entry that is not the end marker
	static void held_note_tints(uint32_t queue)
	{
		if (g_in_held) return;
		begin_tick();
		for (uint32_t node = MEM<uint32_t>(queue); node; node = U32(node, 4))
		{
			if (U32(node, 8) != ORIG_Tint || S8(node, 0x29) != 1) continue;
			const uint32_t e = U32(U32(node, 0x34), (int32_t)(int16_t)U16(node, 0x3A) * 4);
			if ((e >> 24) == 0xFF) continue;
			if (Rec *r = new_rec(HK_TINT, node)) memcpy(r->snap, (const void *)node, 0x40);
		}
	}

	static void held_note_end()
	{
		if (g_in_held) return;
		begin_tick();
		for (int i = 0; i < 3; i++) g_end[i] = MEM<uint32_t>(CURSORS[i]);
		g_end_tick = g_real_tick;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the next tick's state of a node (its task's update on a copy: no draw, no spawn); true = the next tick draws it
	// ---------------------------------------------------------------------------------------------------------------
	// the director's cut test of the next tick (0x73B7E0): the requested cut becomes current at the start of its next run
	static bool cut_next(int32_t c) { return S16(MEM<uint32_t>(0x156C7F0), 0x42) >= c; }
	// (next_state(n): the module's ++state +0x29)

	// the meteor's motion (0x788580 after its state): swing, tilt, the step turned into its velocity, position, scale
	static void meteor_motion(uint32_t a1)
	{
		alignas(4) uint8_t sp[0x2C] = {};
		const uint32_t s = P(sp);
		const uint16_t sw = (uint16_t)((U16(a1, 0xB4) + 0x20) & 0xFFF);
		U16(a1, 0xB4) = sw;
		const int32_t sn = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)sw);
		U16(s, 0) = 0;
		U16(s, 4) = 0;
		U16(a1, 0x94) = (uint16_t)(mul32(sn, 0x300) / 4096);
		U16(s, 2) = U16(a1, 0xD4);
		x::MAG_022_sub_8DD770(s + 0xC);
		x::MAG_022_sub_8DD8A0(s + 0xC, (uint32_t)(int32_t)S16(a1, 0x96));
		x::sub_8DD7E0(s + 0xC, (uint32_t)(int32_t)S16(a1, 0x94));
		x::matrixMultiplyVector(s + 0xC, s, a1 + 0x74);
		U16(a1, 0x54) = (uint16_t)(U16(a1, 0x54) + (uint16_t)(S16(a1, 0x74) / 16));
		U16(a1, 0x56) = (uint16_t)(U16(a1, 0x56) + (uint16_t)(S16(a1, 0x76) / 16));
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + (uint16_t)(S16(a1, 0x78) / 16));
		U32(a1, 0x1C) = U32(a1, 0x54);
		U32(a1, 0x20) = U32(a1, 0x58);
		const int32_t y = S16(a1, 0x20);
		if (y >= 0)
		{
			S32(a1, 0x38) = 2 * y + 0x1000;
			if (S32(a1, 0x38) > 0x4000) S32(a1, 0x38) = 0x4000;
		}
		else
		{
			S32(a1, 0x38) = y + 0x1000;
			if (S32(a1, 0x38) < 0x800) S32(a1, 0x38) = 0x800;
		}
		S32(a1, 0x34) = S32(a1, 0x38);
		S32(a1, 0x30) = S32(a1, 0x38);
	}

	// the meteor task 0x788580 of the next tick on a copy (state 1 = 0x788960 without the impact's spawns)
	static bool meteor_next(uint32_t a1)
	{
		if (S8(a1, 0x29) == 1)
		{
			if (cut_next(3)) { U8(a1, 0x26) |= 1; next_state(a1); }
			else if (S16(a1, 0x1E) >= 0) { U16(a1, 0x1E) = 0; U8(a1, 0x26) |= 5; next_state(a1); }
			else if (S16(a1, 0x24) <= 0x20)
			{
				U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
				if (S16(a1, 0x50) < 0) U16(a1, 0x50) = 0;
			}
			else
			{
				U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
				if (S16(a1, 0x50) >= 0x1000) { U16(a1, 0x50) = 0x1000; U8(a1, 0x26) |= 5; next_state(a1); }
			}
		}
		meteor_motion(a1);
		return (U8(a1, 0x26) & 4) == 0;
	}

	// the petal group 0x78CFA0 of the next tick on a copy (states, then the four petals' motion)
	static bool petals_next(uint32_t a1)
	{
		switch (S8(a1, 0x29))
		{
		case 1:
			if (cut_next(6)) { U8(a1, 0x26) |= 5; break; }
			U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
			if (S16(a1, 0x50) <= 0) { U16(a1, 0x50) = 0; next_state(a1); }
			break;
		case 2:
			if (cut_next(6)) { U8(a1, 0x26) |= 5; break; }
			if (S16(a1, 0x24) >= 0x18) next_state(a1);
			break;
		case 3:
			if (cut_next(6)) { U8(a1, 0x26) |= 5; break; }
			U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
			if (S16(a1, 0x50) >= 0x1000) { U8(a1, 0x26) |= 5; next_state(a1); U16(a1, 0x50) = 0x1000; }
			break;
		default:
			break;
		}
		alignas(4) uint8_t sp[0x28] = {};
		const uint32_t s = P(sp);
		uint32_t p = a1 + 0x98, st = a1 + 0xD4;
		for (int k = 0; k < 4; k++, st += 2, p += 8)
		{
			const uint16_t sw = (uint16_t)((U16(p, 0x20) + 0x40) & 0xFFF);
			U16(p, 0x20) = sw;
			const int32_t sn = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)sw);
			U16(p, 0) = (uint16_t)(shl32(sn, 8) / 4096 - 0xC0);
			U16(s, 0) = U16(st, 0);
			U16(s, 2) = 0;
			U16(s, 4) = 0;
			x::MAG_022_sub_8DD770(s + 8);
			x::sub_8DD960(s + 8, (uint32_t)(int32_t)S16(p, 0));
			const uint32_t v = p - 0x24;
			x::matrixMultiplyVector(s + 8, s, v);
			U16(p, -0x44) = (uint16_t)(U16(p, -0x44) + (uint16_t)(S16(v, 0) / 16));
			U16(p, -0x42) = (uint16_t)(U16(p, -0x42) + (uint16_t)(S16(v, 2) / 16));
			U16(p, -0x40) = (uint16_t)(U16(p, -0x40) + (uint16_t)(S16(v, 4) / 16));
			if (S16(p, -0x42) >= 0) U16(p, -0x42) = 0;
		}
		return (U8(a1, 0x26) & 4) == 0;
	}

	// petal k of a group node as 0x78CFA0 draws it (position +0x1C.., scale from the height)
	static void petal_place(uint32_t a1, int k)
	{
		const uint32_t q = a1 + 0x54 + 8 * (uint32_t)k;
		U32(a1, 0x1C) = U32(q, 0);
		U32(a1, 0x20) = U32(q, 4);
		const int32_t y = S16(a1, 0x20);
		S32(a1, 0x38) = y + 0x1000;
		if (y >= 0) { if (S32(a1, 0x38) > 0x2000) S32(a1, 0x38) = 0x2000; }
		else if (S32(a1, 0x38) < 0x400) S32(a1, 0x38) = 0x400;
		S32(a1, 0x34) = S32(a1, 0x38);
		S32(a1, 0x30) = S32(a1, 0x38);
	}

	// the ground 0x78C4C0 of the next tick (state 1 = 0x78CE30)
	static bool ground_next(uint32_t a1)
	{
		if (S8(a1, 0x29) == 1)
		{
			U16(a1, 0xE0) = (uint16_t)(U16(a1, 0xE0) - 1);
			if (S16(a1, 0xE0) < 0x6C) U16(a1, 0xE0) = 0x6C;
			if (cut_next(3)) { U8(a1, 0x26) |= 5; next_state(a1); }
		}
		return (U8(a1, 0x26) & 4) == 0;
	}

	// the glow 0x78D250 of the next tick (its states draw)
	static bool glow_next(uint32_t a1)
	{
		switch (S8(a1, 0x29))
		{
		case 0:
			U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + 8);
			if (S16(a1, 0x58) >= 0x80) { U16(a1, 0x58) = 0x80; next_state(a1); }
			return true;
		case 1:
			U16(a1, 0x5A) = (uint16_t)(U16(a1, 0x5A) + 0x10);
			if (S16(a1, 0x5A) >= 0x80) { U16(a1, 0x5A) = 0x80; next_state(a1); }
			return true;
		case 2:
			if (cut_next(6)) { U8(a1, 0x26) |= 1; next_state(a1); return false; }
			return true;
		default:
			return false;
		}
	}

	// the letterbox 0x78D410 of the next tick (its states draw)
	static bool bars_next(uint32_t a1)
	{
		switch (S8(a1, 0x29))
		{
		case 0:
			if (S16(a1, 0x24) >= 0x10) { U16(a1, 0x1E) = 0x18; next_state(a1); }
			return false;
		case 1:
			if (cut_next(6)) { U16(a1, 0x5C) = 4; next_state(a1); }
			return true;
		case 2:
			U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
			if (S16(a1, 0x5C) <= 0) next_state(a1);
			return true;
		case 3:
			U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) - 4);
			if (S16(a1, 0x1E) <= 0) { U8(a1, 0x26) |= 1; next_state(a1); return false; }
			return true;
		default:
			return false;
		}
	}

	// the orbit step of 0x787C90 (position +0x1C.. from the orbit centre, angle and tilt)
	static void orbit_motion(uint32_t a1)
	{
		alignas(4) uint8_t sp[0x38] = {};
		const uint32_t s = P(sp);
		U16(a1, 0x2D4) = (uint16_t)(U16(a1, 0x2D4) + 1);
		if (S16(a1, 0x2D4) >= 0x1A)
		{
			U16(a1, 0x2C4) = (uint16_t)(U16(a1, 0x2C4) - 0x40);
			if (S16(a1, 0x2C4) <= 0) U16(a1, 0x2C4) = 0;
		}
		U16(s, 0) = 0;
		U16(s, 2) = 0;
		U16(s, 4) = U16(a1, 0x2DA);
		x::MAG_022_sub_8DD770(s + 0x18);
		x::MAG_022_sub_8DD8A0(s + 0x18, (uint32_t)(int32_t)S16(a1, 0x2C6));
		x::sub_8DD7E0(s + 0x18, (uint32_t)(int32_t)S16(a1, 0x2C4));
		x::matrixMultiplyVector(s + 0x18, s, a1 + 0x68);
		U16(s, 8) = 0;
		U16(s, 0xA) = 0xFF40;
		U16(s, 0xC) = 0;
		U16(a1, 0x2CC) = (uint16_t)(U16(a1, 0x2CC) + (uint16_t)(S16(a1, 0x68) / 16));
		U16(a1, 0x2CE) = (uint16_t)(U16(a1, 0x2CE) + (uint16_t)(S16(a1, 0x6A) / 16));
		U16(a1, 0x2D0) = (uint16_t)(U16(a1, 0x2D0) + (uint16_t)(S16(a1, 0x6C) / 16));
		const uint16_t ang = (uint16_t)((U16(a1, 0x2D8) + U16(a1, 0x2D6)) & 0xFFF);
		U16(a1, 0x2D6) = ang;
		x::sub_8DD960(s + 0x18, (uint32_t)(int32_t)(int16_t)ang);
		x::matrixMultiplyVector(s + 0x18, s + 8, s + 0x10);
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x2CC) + U16(s, 0x10));
		U16(a1, 0x1E) = (uint16_t)(U16(s, 0x12) + U16(a1, 0x2CE));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x2D0) + U16(s, 0x14));
		U16(a1, 0x62) = U16(a1, 0x2C6);
	}

	// a prim-play task of the next tick: does it play (draw)? (the orbit moves on the copy)
	static bool play_next(uint32_t a1)
	{
		const uint32_t f = U32(a1, 8);
		const int32_t st = S8(a1, 0x29);
		if (f == ORIG_Play) return st == 1 && !cut_next(6);              // 0x787AF0
		if (f == ORIG_Impact) return st == 1 && !cut_next(3);            // 0x788B40
		if (f == ORIG_Orbit) { if (st != 2) return false; orbit_motion(a1); return !cut_next(6); } // 0x787C90
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// held draws
	// ---------------------------------------------------------------------------------------------------------------
	// the module's prim-model play (0x785660 = 0x73C280) of the node `live` with the placement of `place`, in between
	static void PlayHeld(uint32_t live, uint32_t place, int num, int den)
	{
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		uint16_t ax = U16(place, 0x62);
		if (ax != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(place, 0x60);
		if (ax != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(place, 0x64);
		if (ax != 0) x::sub_8DD960(L, (uint32_t)(int32_t)(int16_t)ax);
		x::scale3DMatrix(L, place + 0x50);
		S32(L, 0x14) = S16(place, 0x1C);
		S32(L, 0x18) = S16(place, 0x1E);
		S32(L, 0x1C) = S16(place, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(place, 0x86);
		U32(L, 0x44) = U32(place, 0x7C);
		U16(L, 0x4C) = U16(place, 0x8C);
		U16(L, 0x4E) = U16(place, 0x8E);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x50) = U16(place, 0x90);
		U32(L, 0x48) = G(G_258EF98);
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)te_785790, (int)L, num, den);
	}

	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }
	static void lerp32(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S32(mid, o) = lerp_i(S32(a, o), S32(b, o), num, den); }

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];

	// the task of a node drawn on the real tick (`snap`: as drawn, before its frame counter +0x24 counted the tick) runs on the
	// next tick: its node still holds it (same task, counter one on) and it was not released after the tick (finished with no
	// child alive: the task tail returns 2)
	static bool still_runs(uint32_t snap, uint32_t now)
	{
		return U32(now, 8) == U32(snap, 8) && S16(now, 0x24) == (int16_t)(S16(snap, 0x24) + 1) && !((U8(now, 0x26) & 1) && U8(now, 0x28) == 0);
	}

	// a noted node draw (not an actor system / tint) at num / den
	static void DrawHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		const uint32_t n = node_size(cur);
		memcpy(g_nx, (const void *)r.node, n); // the node as the real tick left it
		memcpy(g_mid, r.snap, n);
		const bool alive = still_runs(cur, nx);
		switch (r.kind)
		{
		case HK_PLAY:
		{
			const bool draws = alive && play_next(nx);
			if (num >= den && !draws) return;
			if (U32(cur, 8) == ORIG_Orbit && draws)
				for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
			PlayHeld(r.node, mid, draws ? num : 0, den);
			return;
		}
		case HK_SHADOW:
		{
			if (num >= den && !(alive && play_next(nx))) return;
			te_785620();
			return;
		}
		case HK_MODEL:
		{
			const bool meteor = U32(cur, 8) == ORIG_Meteor;
			const bool draws = alive && (meteor ? meteor_next(nx) : petals_next(nx));
			if (!meteor) petal_place(nx, r.sub);
			if (num >= den) { if (draws) te_788700(nx); return; }
			if (draws)
			{
				for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
				for (uint32_t o = 0x30; o <= 0x38; o += 4) lerp32(mid, cur, nx, o, num, den);
				lerp16(mid, cur, nx, 0x50, num, den);
			}
			te_788700(mid);
			return;
		}
		case HK_GROUND:
		{
			const bool draws = alive && ground_next(nx);
			if (num >= den) { if (draws) te_78C530(nx, (uint32_t)U16(nx, 0xE0)); return; }
			if (draws) lerp16(mid, cur, nx, 0xE0, num, den);
			te_78C530(mid, (uint32_t)U16(mid, 0xE0));
			return;
		}
		case HK_GLOW:
		{
			const bool draws = alive && glow_next(nx);
			if (num >= den) { if (draws) te_78D2E0(nx); return; }
			if (draws) { lerp16(mid, cur, nx, 0x58, num, den); lerp16(mid, cur, nx, 0x5A, num, den); }
			te_78D2E0(mid);
			return;
		}
		case HK_BARS:
		{
			const bool draws = alive && bars_next(nx);
			if (num >= den) { if (draws) te_78D4D0(nx, (uint32_t)U16(nx, 0x1E)); return; }
			if (draws) lerp16(mid, cur, nx, 0x1E, num, den);
			te_78D4D0(mid, (uint32_t)U16(mid, 0x1E));
			return;
		}
		default:
			return;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// screen tint (engine task 0x8DDC30): its colour script step on a copy; the four quads + the draw-mode packet
	// ---------------------------------------------------------------------------------------------------------------
	static bool tint_step(uint32_t a)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF) return false;
		if (op == 0xFE)
		{
			if (S16(a, 0x24) >= (int16_t)(uint16_t)b2 && MEM<uint32_t>(0x2795914) == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(a, 0x3A) = ni;
				U32(a, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a, 0x30) = ent;
			if (MEM<uint32_t>(0x2795914) == 0) U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	static void tint_draw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		uint32_t pkt = MEM<uint32_t>(CUR_FA);
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++) U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, pkt);
		MEM<uint32_t>(CUR_FA) = pkt + 0xC;
	}
	static void TintHeld(const Rec &r, int num, int den)
	{
		alignas(4) uint8_t b[0x40];
		memcpy(b, (const void *)r.node, sizeof(b));
		const bool draws = still_runs(P(r.snap), P(b)) && tint_step(P(b));
		const uint32_t col = U32(r.node, 0x30), next = U32(P(b), 0x30);
		if (num >= den) { if (draws) tint_draw(next); return; }
		if (!draws) { tint_draw(col); return; }
		uint32_t c = col & 0xFF000000;
		for (int k = 0; k < 3; k++)
			c |= (uint32_t)(lerp_i((int32_t)((col >> (8 * k)) & 0xFF), (int32_t)((next >> (8 * k)) & 0xFF), num, den) & 0xFF) << (8 * k);
		tint_draw(c);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// actor systems: the act-library actor template (Mega Phoenix / X-Potion family; module actor state ACTOR_STATE)
	// ---------------------------------------------------------------------------------------------------------------
	// the battle camera words a view is built from (0x5033E0): eye 0xB8B7F0, look-at 0xB8B7F8, roll 0x1D977A2, shake 0x1D97710
	struct CamWords
	{
		int16_t w[10];
		void io(bool set)
		{
			static const uint32_t a[10] = { 0xB8B7F0, 0xB8B7F2, 0xB8B7F4, 0xB8B7F8, 0xB8B7FA, 0xB8B7FC, 0x1D977A2, 0x1D97710, 0x1D97712, 0x1D97714 };
			for (int i = 0; i < 10; i++)
			{
				if (set) MEM<int16_t>(a[i]) = w[i];
				else w[i] = MEM<int16_t>(a[i]);
			}
		}
	};
	// the words after each real tick (noted at its held frames: HeldReady): the view of the next tick is built from them
	static CamWords g_cw_held, g_cw_prev;
	static uint32_t g_cw_held_tick = 0xFFFFFFFF, g_cw_prev_tick = 0xFFFFFFFF;
	static void note_cam_words()
	{
		if (g_cw_held_tick == g_real_tick) return;
		g_cw_prev = g_cw_held;
		g_cw_prev_tick = g_cw_held_tick;
		g_cw_held.io(false);
		g_cw_held_tick = g_real_tick;
	}
	// the view the next tick draws with: the battle builds a tick's view (0x5033E0) from the camera words the tick before
	// left, so with a live camera (this tick's view = the one built from the words after the previous tick) the next view is
	// the one built from the words now; otherwise (a fixed view) it is this tick's. Camera block, words, GTE registers and
	// the Field_Alloc scratch are put back.
	static bool NextView(uint8_t out[0x20])
	{
		if (g_cw_prev_tick + 1 != g_real_tick) return false;
		static uint8_t cam[0xB0], words[0x20], gte[GTE_D_N + GTE_C_N], falloc[0x400];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		memcpy(words, (const void *)0xB8B7F0, sizeof(words));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		if (fa) memcpy(falloc, (const void *)fa, sizeof(falloc));
		CamWords now;
		now.io(false);
		g_cw_prev.io(true);
		fn<void (__cdecl *)()>(0x5033E0)(); // BS_Camera_BuildViewMatrixApplyShake
		const bool live = memcmp((const void *)0x1D97778, g_cam_tick, sizeof(g_cam_tick)) == 0;
		if (live)
		{
			now.io(true);
			fn<void (__cdecl *)()>(0x5033E0)();
			memcpy(out, (const void *)0x1D97778, 0x20);
		}
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)0xB8B7F0, words, sizeof(words));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		MEM<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, falloc, sizeof(falloc));
		return live;
	}

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x789A90: list order; particles = type 0, actors = type 1) run on copies: a particle
	// moves by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0; 4 = key-position path 0x78B860; its
	// emission 0x78ACC0 and life count: not run), an actor by its motion step (0x741050, the end test without its follow-up
	// spawn, 0x740910, 0x741120). A particle's owner and an actor's parent particle (+0x1BC) are their next-tick copies when
	// the pass updates them first, as on the next tick. A morphing model's step rewrites the shared vertex blocks:
	// SystemHeld puts the drawn ones back after the predictions.
	enum { P_HOLD, P_NEXT, P_ENDS };
	static const uint32_t PART_BYTES = 0x6C;
	static const int MAX_PARTS = 128;
	struct NextPass
	{
		uint32_t part[MAX_PARTS], act[MAX_ACTORS];
		int np, na, cls[MAX_ACTORS];
		alignas(4) uint8_t pc[MAX_PARTS][PART_BYTES];
		alignas(4) uint8_t ac[MAX_ACTORS][ACTOR_BYTES];
		uint32_t part_copy(uint32_t real) const { for (int i = 0; i < np; i++) if (part[i] == real) return P(pc[i]); return 0; }
		int act_index(uint32_t real) const { for (int i = 0; i < na; i++) if (act[i] == real) return i; return -1; }
	};
	static NextPass g_next;

	static int StepActor(uint32_t act, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		const uint32_t parent = U32(nxt, 0x1BC);
		const uint32_t pc = parent ? g_next.part_copy(parent) : 0;
		if (pc) U32(nxt, 0x1BC) = pc;
		int r = P_NEXT;
		a_741050(nxt, def);
		const int32_t kind = S8(def, 0x1E);
		bool ends = false;
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			ends = S16(nxt, 0x1C8) < 0;
		}
		if (!ends && (kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) ends = true;
		if (ends)
			r = P_ENDS;
		else
		{
			a_740910(nxt, def);
			a_741120(nxt, def);
			U16(nxt, 0x1CA) = (uint16_t)(U16(nxt, 0x1CA) + 1); // frame count of the step
		}
		U32(nxt, 0x1BC) = parent;
		return r;
	}

	static void StepParticle(uint32_t part, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)part, PART_BYTES);
		if (U16(part, 0x62) == 0) return; // (not initialised yet: none after a real tick)
		const int32_t mode = S8(nxt, 0x68);
		if (mode == 3)
		{
			const uint32_t owner = U32(nxt, 0x5C);
			const int oi = owner ? g_next.act_index(owner) : -1;
			if (oi >= 0) U32(nxt, 0x5C) = P(g_next.ac[oi]);
			a_7424C0(nxt);
			U32(nxt, 0x5C) = owner;
		}
		else if (mode == 4)
			te_78B860(nxt);
		U16(nxt, 0x60) = (uint16_t)(U16(nxt, 0x60) + 1);
	}

	static void NextTick(uint32_t sys)
	{
		g_next.np = g_next.na = 0;
		for (uint32_t node = U32(sys, 0x2C); node; node = U32(node, 4))
		{
			const int32_t type = S16(node, 8);
			if (type == 0 && g_next.np < MAX_PARTS)
			{
				const int i = g_next.np++;
				g_next.part[i] = node;
				StepParticle(node, P(g_next.pc[i]));
			}
			else if (type == 1 && g_next.na < MAX_ACTORS)
			{
				const int i = g_next.na++;
				g_next.act[i] = node;
				g_next.cls[i] = StepActor(node, P(g_next.ac[i]));
			}
		}
	}

	// the drawn actor's copy `cur` half way to its next-tick copy: draw matrix, view positions, sub-positions; at fraction 1
	// the whole next actor. Holds: not in its motion phase, ends on the next tick, sequence change before fraction 1
	static int Predict(uint32_t act, uint32_t cur, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		const int ai = g_next.act_index(act);
		if (ai < 0) return P_HOLD;
		if (g_next.cls[ai] != P_NEXT) return g_next.cls[ai];
		const uint32_t nxt = P(g_next.ac[ai]);
		if (U32(nxt, 0x170) != U32(act, 0x170) && num < den) return P_HOLD;
		for (int k = 0; k < 9; k++)
			S16(cur, 0xAC + 2 * k) = (int16_t)lerp_i(S16(cur, 0xAC + 2 * k), S16(nxt, 0xAC + 2 * k), num, den);
		for (int k = 0; k < 3; k++)
			S32(cur, 0xC0 + 4 * k) = lerp_i(S32(cur, 0xC0 + 4 * k), S32(nxt, 0xC0 + 4 * k), num, den);
		const int32_t copies = S8(cur, 0x1D8);
		for (int32_t i = 0; i < copies && i < 16; i++)
			for (int k = 0; k < 3; k++)
			{
				const int32_t o = 0x19C + 8 * i + 2 * k;
				S16(cur, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den);
			}
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
		return P_NEXT;
	}

	static void SystemHeld(const Rec &r, const uint8_t *next_view, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		const uint32_t sys = r.node;
		MEM<uint32_t>(ACTOR_STATE) = sys;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		if (next_view) memcpy((void *)CAM_COPY, next_view, 0x20); // (the scratch, camera copy included, is put back below)
		NextTick(sys);
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), num, den);
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch and morph vertex blocks
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
		g_sys_verts[r.sysi].restore();
		// the sprite sequences, as 0x789950 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				te_789A10(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					te_789A10(c);
				}
			}
		}
		// the models, as 0x789790 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				te_7897F0(P(copies[i]), def);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the real tick's draws fit the notes and the private arenas (a quarter of each kept as margin: the in-between draws may
	// write a little more than the real ones)
	static bool fits()
	{
		if (g_full) return false;
		for (int i = 0; i < 3; i++)
			if (g_end[i] >= g_start[i] && g_end[i] - g_start[i] > sizeof(g_priv[i]) / 4 * 3) return false;
		return true;
	}

	static bool HeldReady()
	{
		note_cam_words(); // (every held frame, drawn or not)
		return held_ready() && g_end_tick == g_real_tick && g_tick == g_real_tick && fits();
	}

	static uint32_t *crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_085;
		g_in_held = true;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		uint8_t sprite_cam[12]; // written by the sprite draws (0x571BC0)
		memcpy(sprite_cam, (const void *)0x21DFEE4, sizeof(sprite_cam));
		const uint32_t seed = *crt_seed();
		static VertexBlocks verts_now[MAX_SYSTEMS]; // the blocks as the real tick left them
		for (int i = 0; i < g_nrec; i++)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].save(g_rec[i].node);
		// the private arenas start with the real tick's packets: the redraws write the same packets at the same offsets
		// over them (the bytes a draw does not write are the real tick's)
		uint32_t cur[3];
		for (int i = 0; i < 3; i++)
		{
			cur[i] = MEM<uint32_t>(CURSORS[i]);
			const uint32_t used = g_end[i] - g_start[i];
			if (g_start[i] && g_end[i] > g_start[i] && used <= sizeof(g_priv[i])) memcpy(g_priv[i], (const void *)g_start[i], used);
			MEM<uint32_t>(CURSORS[i]) = P(g_priv[i]);
		}
		uint8_t view[0x20];
		const bool next_view = g_nsys > 0 && NextView(view);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			if (r.kind == HK_SYSTEM) { SystemHeld(r, next_view ? view : nullptr, num, den); continue; }
			// each draw starts from the real tick's GTE state, scratch and Field_Alloc scratch
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			if (r.kind == HK_TINT) TintHeld(r, num, den);
			else DrawHeld(r, num, den);
		}
		for (int i = 0; i < 3; i++) MEM<uint32_t>(CURSORS[i]) = cur[i];
		for (int i = g_nrec - 1; i >= 0; i--)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].restore();
		*crt_seed() = seed;
		memcpy((void *)0x21DFEE4, sprite_cam, sizeof(sprite_cam));
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_in_held = false;
		g_mod = saved_mod;
	}
}
}
	static void register_mag085_held()
	{
		static const uint32_t HELD[] = { act::theend::ORIG_Play, act::theend::ORIG_Orbit, act::theend::ORIG_Impact, act::theend::ORIG_Meteor,
			act::theend::ORIG_ActorSystem, act::theend::ORIG_Ground, act::theend::ORIG_Petals, act::theend::ORIG_Glow, act::theend::ORIG_Bars,
			act::theend::ORIG_Tint, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(85, act::theend::HeldReady, act::theend::HeldFrame);
	}
}
