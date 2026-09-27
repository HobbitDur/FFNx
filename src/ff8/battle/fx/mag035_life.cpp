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

// Effect 35: Life (spell, MAG_035_*): a copy of the actor/heal effect library (see act_engine.h).
//
// Setup MAG_035_LIFE 0x8649C0 (runs once, not ported; file loader 0x8649A0 = the sprite file
// 0x15E702C, its pointer in 0x26AC780) creates the root queue 0x26AC868 with the master (node 0x64
// bytes: +0x58 = actions - 1, +0x5A = target count, +0x2D = first target), plays the camera
// animation 0x15E69FC and uploads the file's TIMs (unless cast flag bit0), takes two packet arenas
// in the magic texture buffer (0x26B7174 = buffer, 0x26B7170 = buffer + 0x2000) and four task pools:
//   0x26B4BD0 emitters (4 x 0x64), 0x26B7178 (300 x 0x30, never used), 0x26B4BB8 sprite particles
//   (300 x 0x70), 0x26B7160 glints (100 x 0x5C, the engine task Effect_Glint_Tick 0x8DCC20).
//   Master (0x864AB0) - packet cursor 0x26AC784 = one of the two arenas by tick parity, follows
//     the caster's bones, runs its 5-state table (0x864B90 spawns one emitter per action and holds
//     itself on +0x63, 0x865580 goes on to the next action once the emitter releases it, 0x8655B0
//     finishes when no task is left), runs the four queues (live task count -> +0x5E), ends when
//     nothing is left.
//   Emitter (0x864BC0), one per action (target 0 of the action): bone-follow anchor + model bounds
//     (CURE_Emitter helpers), 5-state table animating its spawn parameters (+0x58 radius, +0x5A
//     height spread, +0x5C count, +0x5E / +0x60 sprite scale ramp, +0x62 ring height); while +0x26
//     bit1 is set (states 0..3) it spawns every tick: 0x864DD0 rising sparkles, 0x864C80 revive
//     sparkles, 0x864F00 ring sparkles, 0x864F90 falling sparkles, 0x865050 glints. State 3 (tick
//     > 30) applies the action's result to its target (ApplyActionResultToTarget) and releases the
//     master. Sound 0x15E6388 at its first tick.
//   Sprite particles (0x70-byte nodes), all update THEN draw one sprite sequence (flipbook frame
//     +0x50 of the sequence +0x4C, InitEffectSequenceFromData) at +0x1C placed by
//     TransformCameraByShadowRotation (scale, camera offset +0x54):
//       0x8653E0 static sparkle (scale +0x68 += +0x6A), 0x865270 ring sparkle (orbits the centre
//       +0x60/+0x64 by 0x80 per tick at radius sin/cos / +0x6C, rises 0x40 per tick, scale ramp),
//       0x865120 falling sparkle (y += +0x5A per tick, fixed scale 0x1000).
//   Glints (Effect_Glint_Tick 0x8DCC20, engine range, shared with the other heal spells: ported in
//     the shared glint file, not here): a spinning gouraud star at the target's bounds centre; its
//     packet cursor is the global pointer 0x2792E74, which Life points at 0x26AC784 before each spawn.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26AC780..0x26BA9C8 (file pointer, packet cursor 0x26AC784, pools, queues, arena
// pointers 0x26B7170 / 0x26B7174); engine global 0x2792E74 (glint packet cursor pointer).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace life
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_035 = { "life", 35, 0x8649A0, 0x8655E0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x865230, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26AC784;   // module packet cursor (sprites and glints)
	static const uint32_t ARENA_ODD = 0x26B7174;       // packet arena of odd ticks (magic texture buffer)
	static const uint32_t ARENA_EVEN = 0x26B7170;      // packet arena of even ticks (buffer + 0x2000)
	static const uint32_t Q_EMITTER = 0x26B4BD0;       // emitter queue (0x64-byte nodes)
	static const uint32_t Q_UNUSED = 0x26B7178;        // 0x30-byte nodes, nothing is ever spawned there
	static const uint32_t Q_SPRITE = 0x26B4BB8;        // sprite particle queue (0x70-byte nodes)
	static const uint32_t Q_GLINT = 0x26B7160;         // glint queue (0x5C-byte nodes)
	static const uint32_t GLINT_CURSOR_PTR = 0x2792E74; // engine: pointer to the glints' packet cursor

	static const uint32_t ORIG_Master = 0x864AB0;
	static const uint32_t ORIG_Emitter = 0x864BC0;
	static const uint32_t ORIG_Fall = 0x865120;       // falling sparkle task
	static const uint32_t ORIG_Ring = 0x865270;       // ring sparkle task
	static const uint32_t ORIG_Sparkle = 0x8653E0;    // static sparkle task
	static const uint32_t ORIG_Glint = 0x8DCC20;      // Effect_Glint_Tick (engine)
	static const void *const SOUND_Life = (const void *)0x15E6388;
	// sprite sequences in the file
	static const uint32_t SEQ_Revive = 0x15E6890;
	static const uint32_t SEQ_RiseA = 0x15E638C;
	static const uint32_t SEQ_RiseB = 0x15E6538;
	static const uint32_t SEQ_Ring = 0x15E66E4;
	static const uint32_t SEQ_Fall = 0x15E68EC;

	// engine functions not in the library's wrapper list
	inline uint32_t Effect_CopyAnchorXZFromSource(uint32_t node, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DC6E0)(node, out); }
	inline uint32_t TransformCameraByShadowRotation(uint32_t pos, uint32_t scale, uint32_t offset) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(pos, scale, offset); }
	inline uint32_t InitEffectSequenceFromData(uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(hdr, ot, mode, cursor); }

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
}
}
}

#ifdef FF8_FX_HELD
#include "mag035_life_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace life
{
	// 0x864AB0 (MAG_035_LIFE_Tick): MASTER task - packet arena by tick parity, bone follow, 5-state
	// table, the four queues (live task count -> node +0x5E)
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_035;
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x864B80;
		states[1] = 0x864B90;
		states[2] = 0x865580;
		states[3] = 0x8655B0;
		states[4] = 0x8655D0; // nullsub (ret)
		if ((U8(node, 0x5C) & 1) != 0)
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_ODD);
		else
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_EVEN);
		// 30 fps layer: see mag035_life_held.inc
		FX_HELD(held_note_master(node);)
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = (uint16_t)x::ExecuteTaskQueue(Q_EMITTER);
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_UNUSED));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SPRITE));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_GLINT));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x864B90 (MAG_035_sub_864B90): master state 1 - holds the master (+0x63), spawns the emitter of
	// the current action (+0x2A), next state
	static uint32_t __cdecl SpawnEmitter(uint32_t a1)
	{
		U8(a1, 0x63) = 1;
		x::Effect_AddTaskAndInitFromCtx(Q_EMITTER, ORIG_Emitter, 0x64, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x865580 (MAG_035_sub_865580): master state 2 - once the emitter released the master (+0x63
	// == 0): next action (back to state 1) while +0x2A < +0x58 (actions - 1), else next state
	static uint32_t __cdecl NextAction(uint32_t a1)
	{
		if (U8(a1, 0x63) != 0)
			return a1;
		const int8_t action = S8(a1, 0x2A);
		if ((int16_t)action < S16(a1, 0x58))
		{
			U8(a1, 0x2A) = (uint8_t)(action + 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) - 1);
		}
		else
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8655B0 (MAG_035_sub_8655B0): master state 3 - finished once no task was alive (+0x5E == 0)
	static uint32_t __cdecl WaitChildren(uint32_t a1)
	{
		if (U16(a1, 0x5E) == 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x864D80 (MAG_035_sub_864D80): spawns a static sparkle task (0x8653E0) under `parent` at the
	// parent's anchor: +0x4C sequence, +0x52 last frame, +0x54 camera offset, +0x68 scale 0x1000
	static uint32_t SpawnSparkle(uint32_t parent, uint32_t seq, uint32_t last, uint32_t offset)
	{
		const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Sparkle, 0x70, parent);
		Effect_CopyAnchorXZFromSource(s, s + 0x1C);
		U16(s, 0x54) = (uint16_t)offset;
		U32(s, 0x4C) = seq;
		U16(s, 0x52) = (uint16_t)last;
		U16(s, 0x68) = 0x1000;
		return s;
	}

	// 0x864DD0 (MAG_035_sub_864DD0): up to tick 30, +0x5C rising sparkles per tick: random point in
	// the disc of radius +0x58 (or half of it), random height below -0x100 (spread +0x5A), scale
	// ramp from the emitter's +0x5E toward +0x60 in 16 steps
	static void SpawnRising(uint32_t node)
	{
		if (S16(node, 0x24) > 0x1E)
			return;
		int16_t count = S16(node, 0x5A);
		if (count == 0)
			count = 1;
		if (S16(node, 0x5C) <= 0)
			return;
		const int32_t d = count;
		int32_t i = 0;
		do
		{
			int16_t radius;
			if ((x::CrtRand() & 1) != 0)
				radius = (int16_t)(S16(node, 0x58) / 2);
			else
				radius = S16(node, 0x58);
			if (radius == 0)
				radius = 1;
			const uint32_t seq = (x::CrtRand() & 1) != 0 ? SEQ_RiseA : SEQ_RiseB;
			const uint32_t s = SpawnSparkle(node, seq, 0xF, 0xFFFFFE00);
			const int32_t angle = (int32_t)(x::CrtRand() & 0xFFF);
			const int32_t dist = (int16_t)((int32_t)x::CrtRand() % (int32_t)radius);
			U16(s, 0x68) = U16(node, 0x5E);
			U16(s, 0x6A) = (uint16_t)((S16(node, 0x60) - S16(node, 0x5E)) / 16);
			U16(s, 0x1C) = (uint16_t)(U16(s, 0x1C) + (uint16_t)(mul32((int32_t)x::computeSin((uint32_t)angle), dist) / 4096));
			U16(s, 0x20) = (uint16_t)(U16(s, 0x20) + (uint16_t)(mul32((int32_t)x::computeCosine((uint32_t)angle), dist) / 4096));
			const int32_t rem = (int32_t)x::CrtRand() % d;
			U16(s, 0x1E) = (uint16_t)(U16(s, 0x1E) + (uint16_t)(-0x100 - rem));
			i++;
		} while (i < S16(node, 0x5C));
	}

	// 0x864C80 (MAG_035_LIFE_SpawnReviveSparkles): a revive sparkle at tick 0 (at the anchor) and on
	// the even ticks 12..20 (random offset: x/z within +0x58 / 2, above by up to +0x5A)
	static void SpawnRevive(uint32_t node)
	{
		if (U16(node, 0x24) == 0)
			SpawnSparkle(node, SEQ_Revive, 2, 0xFFFFFC00);
		const int16_t t = S16(node, 0x24);
		if (t <= 0xA || t >= 0x16 || (t & 1) != 0)
			return;
		const uint32_t s = SpawnSparkle(node, SEQ_Revive, 2, 0xFFFFFC00);
		int16_t dx = (int16_t)((int32_t)(x::CrtRand() & 0x7FFF) % (S16(node, 0x58) / 2 + 1));
		const int16_t dy = (int16_t)((int32_t)(x::CrtRand() & 0x7FFF) % (S16(node, 0x5A) + 1));
		int16_t dz = (int16_t)((int32_t)(x::CrtRand() & 0x7FFF) % (S16(node, 0x58) / 2 + 1));
		if ((x::CrtRand() & 1) != 0)
			dx = (int16_t)-dx;
		if ((x::CrtRand() & 1) != 0)
			dz = (int16_t)-dz;
		U16(s, 0x1C) = (uint16_t)(U16(s, 0x1C) + (uint16_t)dx);
		U16(s, 0x1E) = (uint16_t)(U16(s, 0x1E) - (uint16_t)dy);
		U16(s, 0x20) = (uint16_t)(U16(s, 0x20) + (uint16_t)dz);
	}

	// 0x864F00 (MAG_035_sub_864F00): ticks 11..29, one ring sparkle per tick (0x865270) at the anchor
	// raised by the emitter's +0x62 (orbit centre +0x60/+0x64, random phase +0x42, radius divisor
	// +0x6C = (tick - 10) / 3 + 8)
	static void SpawnRing(uint32_t node)
	{
		const int16_t t = S16(node, 0x24);
		if (t <= 0xA || t >= 0x1E)
			return;
		const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Ring, 0x70, node);
		Effect_CopyAnchorXZFromSource(s, s + 0x1C);
		U16(s, 0x1E) = (uint16_t)(U16(s, 0x1E) + U16(node, 0x62));
		U32(s, 0x60) = U32(s, 0x1C);
		U32(s, 0x64) = U32(s, 0x20);
		U32(s, 0x4C) = SEQ_Ring;
		U16(s, 0x52) = 0xF;
		U16(s, 0x68) = 0x1000;
		U16(s, 0x42) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(s, 0x6C) = (uint16_t)((S16(node, 0x24) - 0xA) / 3 + 8);
	}

	// 0x864F90 (MAG_035_sub_864F90): ticks 21..36 (while spawning), one falling sparkle per tick
	// (0x865120): random point on the circle of radius 1/8 around the anchor, above it by
	// 0x80..0x27F, fall speed +0x5A = 16..31
	static void SpawnFall(uint32_t node)
	{
		const int16_t t = S16(node, 0x24);
		if (t <= 0x14 || t >= 0x25)
			return;
		const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Fall, 0x70, node);
		U32(s, 0x4C) = SEQ_Fall;
		U16(s, 0x52) = 9;
		U16(s, 0x68) = 0x1000;
		U16(s, 0x42) = (uint16_t)(x::CrtRand() & 0xFFF);
		Effect_CopyAnchorXZFromSource(s, s + 0x1C);
		U16(s, 0x1C) = (uint16_t)(U16(s, 0x1C) + (uint16_t)((int32_t)x::computeSin((uint32_t)(int32_t)S16(s, 0x42)) / 8));
		U16(s, 0x20) = (uint16_t)(U16(s, 0x20) + (uint16_t)((int32_t)x::computeCosine((uint32_t)(int32_t)S16(s, 0x42)) / 8));
		U16(s, 0x1E) = (uint16_t)(U16(s, 0x1E) + (uint16_t)(-0x80 - (int32_t)(x::CrtRand() & 0x1FF)));
		U16(s, 0x5A) = (uint16_t)((x::CrtRand() & 0xF) + 0x10);
	}

	// 0x865050 (MAG_035_sub_865050): ticks 4..15, 1 / 2 / 3 / 1 glints per tick (Effect_Glint_Tick
	// in the glint queue, packet cursor pointer 0x2792E74 = this module's cursor): 8 ticks growth of
	// 0x100 / 0x10 (size) and 0x10 0x10 0x10 / 0x10 5 8 (the two colours), then 8 ticks shrink of
	// 0x40 / 0x10, spin +0x58 = +-0x40 per tick
	static void SpawnGlints(uint32_t node)
	{
		const int16_t t = S16(node, 0x24);
		if (t <= 3 || t >= 0x10)
			return;
		int32_t count;
		if (t > 0xE)
			count = 1;
		else if (t > 0xA)
			count = 3;
		else
			count = (t > 5 ? 1 : 0) + 1;
		if (count <= 0)
			return;
		do
		{
			MEM<uint32_t>(GLINT_CURSOR_PTR) = PACKET_CURSOR;
			const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_GLINT, ORIG_Glint, 0x5C, node);
			U16(s, 0x56) = 8;
			U8(s, 0x4C) = 0x10;
			U8(s, 0x4D) = 0x10;
			U8(s, 0x4E) = 0x10;
			U8(s, 0x50) = 0x10;
			U8(s, 0x51) = 5;
			U8(s, 0x52) = 8;
			U16(s, 0x3C) = 0x100;
			U16(s, 0x3E) = 0x10;
			U16(s, 0x40) = 0x40;
			U16(s, 0x42) = 0x10;
			if ((x::CrtRand() & 1) != 0)
				U16(s, 0x58) = 0x40;
			else
				U16(s, 0x58) = 0xFFC0;
		} while (--count != 0);
	}

	// 0x864BC0 (MAG_035_sub_864BC0): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x865490 init, 0x8654D0 grow, 0x8654F0 spread, 0x865520 fade + result, ret}, the
	// five spawners while +0x26 bit1 is set, sound at its first tick
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x865490;
		states[1] = 0x8654D0;
		states[2] = 0x8654F0;
		states[3] = 0x865520;
		states[4] = 0x865570; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 2) != 0)
		{
			SpawnRising(node);
			SpawnRevive(node);
			SpawnRing(node);
			SpawnFall(node);
			SpawnGlints(node);
		}
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Life), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x865490 (MAG_035_sub_865490): emitter state 0 - spawning on (+0x26 bit1), radius / spread /
	// ring height 0, count 1, scale ramp 0x100 -> 0x1000, next state
	static uint32_t __cdecl EmitterInit(uint32_t a1)
	{
		U8(a1, 0x26) |= 2;
		U16(a1, 0x58) = 0;
		U16(a1, 0x5A) = 0;
		U16(a1, 0x62) = 0;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x5C) = 1;
		U16(a1, 0x5E) = 0x100;
		U16(a1, 0x60) = 0x1000;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x8654D0 (MAG_035_sub_8654D0): emitter state 1 - scale start += 0x180, radius += 0x66; after
	// tick 10 next state
	static uint32_t __cdecl EmitterGrow(uint32_t a1)
	{
		U16(a1, 0x5E) = (uint16_t)(U16(a1, 0x5E) + 0x180);
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + 0x66);
		if (S16(a1, 0x24) > 0xA)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8654F0 (MAG_035_sub_8654F0): emitter state 2 - radius -= 0x4C, spread += 0x40, ring height
	// -= 0x26; after tick 20: count 1, scale end 0x10, next state
	static uint32_t __cdecl EmitterSpread(uint32_t a1)
	{
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) - 0x4C);
		U16(a1, 0x5A) = (uint16_t)(U16(a1, 0x5A) + 0x40);
		U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) - 0x26);
		if (S16(a1, 0x24) > 0x14)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x5C) = 1;
			U16(a1, 0x60) = 0x10;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x865520 (MAG_035_sub_865520): emitter state 3 - scale start -= 0xFF; after tick 30: releases
	// the master (root +0x63 = 0), applies the result of its action to the action's first target,
	// spawning off (+0x26 bit1), finished, next state
	static uint32_t __cdecl EmitterEnd(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + 0xFF01);
		if (S16(node, 0x24) <= 0x1E)
			return 0; // void
		U8(U32(node, 0x10), 0x63) = 0;
		const int32_t action = S8(node, 0x2A);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		x::ApplyActionResultToTarget(U32(acts + (uint32_t)(action * 20), 8));
		const uint16_t flags = U16(node, 0x26);   // word access: +0x27 is written back unchanged
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x26) = (uint16_t)((flags & 0xFFFD) | 1);
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x865180 (sub_865180): draw of a falling sparkle (unless hidden, +0x26 bit2): one sprite
	// sequence (+0x4C, frame +0x50) at +0x1C, scale 0x1000, camera offset +0x54
	static void DrawFall(uint32_t node)
	{
		if ((U8(node, 0x26) & 4) != 0)
			return;
		const uint32_t hdr = x::Field_Alloc(0xB4);
		TransformCameraByShadowRotation(node + 0x1C, 0x1000, (uint32_t)(int32_t)S16(node, 0x54));
		U32(hdr, 0) = U32(node, 0x4C);
		U16(hdr, 4) = U16(node, 0x50);
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = InitEffectSequenceFromData(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// 0x8652E0 (sub_8652E0): draw of a ring / static sparkle (unless hidden, +0x26 bit2): scale =
	// the low word of the second argument (the task passes +0x68; the high word of that argument is
	// a leftover the placement ignores), camera offset +0x54, one sprite sequence
	static void DrawScaled(uint32_t node, uint16_t scale)
	{
		if ((U8(node, 0x26) & 4) != 0)
			return;
		TransformCameraByShadowRotation(node + 0x1C, scale, (uint32_t)(int32_t)S16(node, 0x54));
		const uint32_t hdr = x::Field_Alloc(0xB4);
		U32(hdr, 0) = U32(node, 0x4C);
		U16(hdr, 4) = U16(node, 0x50);
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = InitEffectSequenceFromData(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// 0x865120 (MAG_035_sub_865120): FALLING SPARKLE task - state {0x8651F0 next, 0x865200 fall,
	// ret}, then its draw 0x865180
	static uint32_t __cdecl Fall(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x8651F0;
		states[1] = 0x865200;
		states[2] = 0x865260; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag035_life_held.inc
		FX_HELD(held_note_sprite(ORIG_Fall, node);)
		DrawFall(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x865200 (sub_865200): falling sparkle state 1 - y += +0x5A, next flipbook frame; finished
	// after the last one
	static uint32_t __cdecl FallStep(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x5A));
		if (a_743C20(node) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x865270 (MAG_035_sub_865270): RING SPARKLE task - state {0x865350 next, 0x865360 orbit, ret},
	// scale += +0x6A, then its draw 0x8652E0
	static uint32_t __cdecl Ring(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x865350;
		states[1] = 0x865360;
		states[2] = 0x8653D0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x68) = (uint16_t)(U16(node, 0x68) + U16(node, 0x6A));
		// 30 fps layer: see mag035_life_held.inc
		FX_HELD(held_note_sprite(ORIG_Ring, node);)
		DrawScaled(node, U16(node, 0x68));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x865360 (sub_865360): ring sparkle state 1 - phase += 0x80, position = centre + (sin, cos) /
	// +0x6C, y -= 0x40, next flipbook frame; finished after the last one
	static uint32_t __cdecl RingStep(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t angle = (uint16_t)((U16(node, 0x42) + 0x80) & 0xFFF);
		U16(node, 0x1C) = U16(node, 0x60);
		U16(node, 0x42) = angle;
		U16(node, 0x20) = U16(node, 0x64);
		const int32_t sx = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)angle) / (int32_t)S16(node, 0x6C);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)sx);
		const int32_t cz = (int32_t)x::computeCosine((uint32_t)(int32_t)S16(node, 0x42)) / (int32_t)S16(node, 0x6C);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) - 0x40);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)cz);
		if (a_743C20(node) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x8653E0 (sub_8653E0): STATIC SPARKLE task - state {0x865450 next, 0x865460 next frame /
	// finished, ret}, scale += +0x6A, then its draw 0x8652E0
	static uint32_t __cdecl Sparkle(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x865450;
		states[1] = 0x865460;
		states[2] = 0x865480; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x68) = (uint16_t)(U16(node, 0x68) + U16(node, 0x6A));
		// 30 fps layer: see mag035_life_held.inc
		FX_HELD(held_note_sprite(ORIG_Sparkle, node);)
		DrawScaled(node, U16(node, 0x68));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x864B80, (void *)a_73A0D0, "035 MAG_035_sub_864B80" },
		{ 0x8651F0, (void *)a_73A0D0, "035 sub_8651F0" },
		{ 0x865230, (void *)a_743C20, "035 sub_865230" },
		{ 0x865260, (void *)a_73A6D0, "035 nullsub_1647" },
		{ 0x865350, (void *)a_73A0D0, "035 sub_865350" },
		{ 0x8653D0, (void *)a_73A6D0, "035 nullsub_1648" },
		{ 0x865450, (void *)a_73A0D0, "035 sub_865450" },
		{ 0x865460, (void *)a_743C00, "035 sub_865460" },
		{ 0x865480, (void *)a_73A6D0, "035 nullsub_1649" },
		{ 0x865570, (void *)a_73A6D0, "035 nullsub_1650" },
		{ 0x8655D0, (void *)a_73A6D0, "035 nullsub_1651" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (task functions and state handlers; helpers are called directly)
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "035 Life Master" },
		{ 0x864B90, (void *)SpawnEmitter, "035 SpawnEmitter" },
		{ 0x865580, (void *)NextAction, "035 NextAction" },
		{ 0x8655B0, (void *)WaitChildren, "035 WaitChildren" },
		{ ORIG_Emitter, (void *)Emitter, "035 Emitter" },
		{ 0x865490, (void *)EmitterInit, "035 EmitterInit" },
		{ 0x8654D0, (void *)EmitterGrow, "035 EmitterGrow" },
		{ 0x8654F0, (void *)EmitterSpread, "035 EmitterSpread" },
		{ 0x865520, (void *)EmitterEnd, "035 EmitterEnd" },
		{ ORIG_Fall, (void *)Fall, "035 Fall" },
		{ 0x865200, (void *)FallStep, "035 FallStep" },
		{ ORIG_Ring, (void *)Ring, "035 Ring" },
		{ 0x865360, (void *)RingStep, "035 RingStep" },
		{ ORIG_Sparkle, (void *)Sparkle, "035 Sparkle" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag035_life()
	{
		act::register_module(35);
		for (const act::life::ModPort *p = act::life::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(35, p->addr, p->port, p->name);
		for (const act::life::ModPort *p = act::life::PORTS; p->addr; p++)
			act::register_module_port(35, p->addr, p->port, p->name);
		// 30 fps layer: see mag035_life_held.inc
		FX_HELD(register_mag035_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag035_life_held.inc"
#endif
