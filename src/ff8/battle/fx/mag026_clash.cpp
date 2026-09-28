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

// Effect 26: Clash (enemy attack, MAG_026_*; X-ATM092): built on the heal-spell effect library
// (master / emitter framework, Effect_* helpers 0x8DC5xx, node layout of act_engine.h), but every
// task and state handler is the module's own code (no engine task body is shared).
//
// Setup MAG_026_CLASH 0x8885D0 (runs once, not ported; file loader 0x8885B0 = the texture file
// named at 0x161078C, TIM upload) creates the master (pool 0x26DBC00, 2 x 0x64: +0x2D = first
// target, +0x58 / +0x5A = action 0's last action / target count) and five queues: 0x26E3F68
// (4 x 0x58: emitters), 0x26E40E0 (0x40 x 0x60: the knock-back of a target), 0x26E3F50
// (0x12C x 0x70: sprites), 0x26DBB10 (0x64 x 0x6C: prim models), 0x26D9050 (2 x 0x3C: screen
// flash); packet arenas = the magic buffer + 0 / + 0xC000 (0x26E40D8 / 0x26E40DC).
//   Master (0x8886C0) - packet arena by tick parity (cursor 0x26D90DC), bone follow, 5-state
//     table: one emitter per target of action 0, then waits for the queues (live task count ->
//     +0x5E); runs the five queues, ends when nothing is left.
//   Emitter (0x8887F0), one per target: bone-follow anchor + model bounds (CURE_Emitter helpers),
//     4-state table: anchor copy; from tick 4 the two shock waves (0x889080 / 0x889170), the
//     knock-back (0x889410), 4 dust sprites (0x889250) and, for target 0, the screen flash
//     (0x888F80, colour script 0x1610774); ends after tick 30. Target 0's emitter also plays the
//     sound 0x160F6C8 at tick 4 and makes bursts (0x8888F0) at the caster's bones: ticks 4 and 40
//     bones 0x34 / 0x35 / 0x36 / 0x33 with rings, tick 17 bone 0x36, tick 20 bone 0x34.
//   Burst (0x8888F0): a ring (0x8889F0) when asked and 8 sparks (0x888BF0) on a random eighth-turn
//     star (random radius < 0x100) that fly outwards, decelerating, after a random delay 0..3.
//   Rings (0x8889F0 at a burst, 0x8897A0 at a landing): prim model 0x16104B4 growing (+0x300 /
//     +0x200 per tick), fade from table 0x161075C by tick; shock waves 0x889080 (model 0x160FA94,
//     growing, fading in 0x200 per tick) and 0x889170 (model 0x160FF3C, shrinking): one prim
//     model draw (0x888A50).
//   Sprites (0x888BF0 sparks, 0x889250 dust, 0x8898B0 landing sparks, 0x889BF0 walk-back dust):
//     sprite sequences (0x160F7A8 / 0x160F6CC) drawn through 0x888C60, hidden until their delay
//     ran out, gone after their last frame.
//   Knock-back (0x889410) - no draw: moves the TARGET entity (the battle draws it): chain
//     transformation 6, launched along its facing with a size-dependent upward speed, spinning
//     about x, falls back to its ground height (landing: chain transformation 6, a ring and 3
//     sparks); then (unless its anim state +0x74 -> byte 0 is 6) turns round (chain
//     transformation 0x1A), walks back to its start position in 16 ticks (a dust sprite every 4th
//     tick), restores its facing / flag 0x1000, its idle chain (+0x74 -> byte 2) and applies the
//     action result.
//   Screen flash (0x888F80): colour script (colour words, 0xFE000000 | t = wait for tick t,
//     0xFF...... = end), a full-screen flat quad at OT bucket + 0x1C.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26D9050..0x26E58F0 (flash queue + pool, file pointer 0x26D90D8, packet cursor
// 0x26D90DC, prim pool, master / sprite / emitter pools and queues, arena pointers, knock-back
// queue + pool). The target entity (position, angles, flags) is engine state.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace clash026
{
	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26D90DC;
	static const uint32_t ARENA_EVEN = 0x26E40D8, ARENA_ODD = 0x26E40DC;
	static const uint32_t Q_EMITTER = 0x26E3F68;   // 4 x 0x58
	static const uint32_t Q_CHARGE = 0x26E40E0;    // 0x40 x 0x60
	static const uint32_t Q_SPRITE = 0x26E3F50;    // 0x12C x 0x70
	static const uint32_t Q_PRIM = 0x26DBB10;      // 0x64 x 0x6C
	static const uint32_t Q_FLASH = 0x26D9050;     // 2 x 0x3C
	static const uint32_t SOUND_Clash = 0x160F6C8;
	static const uint32_t FLASH_SCRIPT = 0x1610774;
	static const uint32_t FADE_TABLE = 0x161075C;  // s16 x 12
	static const uint32_t MODEL_Ring = 0x16104B4, MODEL_WaveA = 0x160FA94, MODEL_WaveB = 0x160FF3C;
	static const uint32_t SEQ_Spark = 0x160F7A8, SEQ_Dust = 0x160F6CC;

	static const uint32_t ORIG_Master = 0x8886C0;
	static const uint32_t ORIG_Emitter = 0x8887F0;
	static const uint32_t ORIG_BurstRing = 0x8889F0;
	static const uint32_t ORIG_Spark = 0x888BF0;
	static const uint32_t ORIG_Flash = 0x888F80;
	static const uint32_t ORIG_WaveA = 0x889080;
	static const uint32_t ORIG_WaveB = 0x889170;
	static const uint32_t ORIG_Dust = 0x889250;
	static const uint32_t ORIG_Charge = 0x889410;
	static const uint32_t ORIG_LandRing = 0x8897A0;
	static const uint32_t ORIG_LandSpark = 0x8898B0;
	static const uint32_t ORIG_WalkDust = 0x889BF0;

	// engine / effect-library functions not in act::x (original addresses)
	namespace cx
	{
		inline uint32_t CopyAnchorFromSource(uint32_t node, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DC6E0)(node, out); } // Effect_CopyAnchorXZFromSource: 8 bytes of the root emitter +0x30
		inline uint32_t TransformCameraByShadowRotation(uint32_t pos, uint32_t a, uint32_t b) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(pos, a, b); }
		inline uint32_t InitEffectSequenceFromData(uint32_t h, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(h, ot, mode, cursor); }
		inline uint32_t BuildRotationMatrixFromAngles(uint32_t angles, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56CD50)(angles, out); }
		inline uint32_t QueueChainTransformation(uint32_t entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(entity, id); }
	}

	typedef uint32_t (__cdecl *StateFn)(uint32_t);

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

	static inline uint32_t RenderOT(uint32_t off) { return MEM<uint32_t>(0x1D8E04C) + off; }
	static inline uint32_t EntityOf(uint32_t node) { return 0x1D972C0 + 0x9C * (uint32_t)U8(node, 0x2D); }
	static uint32_t __cdecl Nop(uint32_t a1) { return a1; } // the tables' nullsubs (ret)
}
}
}

#ifdef FF8_FX_HELD
#include "mag026_clash_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace clash026
{
	// ------------------------------------------------------------------------------------
	// draws
	// ------------------------------------------------------------------------------------
	// 0x888A50 (sub_888A50): prim model +0x4C at +0x1C, angles +0x44, scale +0x30 (3 x int32),
	// colour +0x40, fade +0x50 (mode 0xF0); not when hidden (+0x26 bit 2)
	static void PrimDraw(uint32_t node)
	{
		if (U8(node, 0x26) & 4)
			return;
		// 30 fps layer: see mag026_clash_held.inc
		FX_HELD(held_note_prim(node);)
		Mat4x3 m = {};
		cx::BuildRotationMatrixFromAngles(node + 0x44, P(&m));
		m.t[0] = S16(node, 0x1C);
		m.t[1] = S16(node, 0x1E);
		m.t[2] = S16(node, 0x20);
		x::scale3DMatrix(P(&m), node + 0x30);
		x::ComposeAffineTransform(0x1D97778, P(&m), P(&m));
		x::GTE_SetRotMatrix_W(P(&m));
		x::GTE_SetTransVector_W(P(&m));
		const uint32_t h = x::Field_Alloc(0x58);
		U32(h, 0) = U32(node, 0x4C);
		U32(h, 8) = U32(node, 0x40);
		S32(h, 0xC) = S16(node, 0x50);
		U32(h, 0x1C) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(h, RenderOT(0x44), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x58);
	}

	// 0x888C60 (sub_888C60): the node's sprite sequence (+0x4C, frame +0x50) at +0x1C facing the
	// camera, scale `scale` (only its low word is read), camera offset +0x54; not when hidden
	static void SpriteDraw(uint32_t node, uint32_t scale)
	{
		if (U8(node, 0x26) & 4)
		{
			// 30 fps layer: see mag026_clash_held.inc
			FX_HELD(held_note_sprite(node, scale, true);)
			return;
		}
		// 30 fps layer: see mag026_clash_held.inc
		FX_HELD(held_note_sprite(node, scale, false);)
		cx::TransformCameraByShadowRotation(node + 0x1C, scale, (uint32_t)(int32_t)S16(node, 0x54));
		const uint32_t h = x::Field_Alloc(0xB4);
		U32(h, 0) = U32(node, 0x4C);
		U16(h, 4) = U16(node, 0x50);
		U16(h, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = cx::InitEffectSequenceFromData(h, RenderOT(0x44), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// the screen flash's full-screen flat quad (320 x 216, code 0x2A, colour `colour`) and its draw
	// mode (0x45C690 / 0x45BFC0) into the module arena, OT bucket base + 0x1C
	static void FlashDraw(uint32_t colour)
	{
		const uint32_t bucket = RenderOT(0x1C);
		uint32_t pkt = MEM<uint32_t>(PACKET_CURSOR);
		U32(pkt, 4) = colour;
		U32(pkt, 0) = 0x5000000;
		U8(pkt, 7) = 0x2A;
		U32(pkt, 8) = 0;
		U32(pkt, 0xC) = 0x140;
		U32(pkt, 0x10) = 0xD80000;
		U32(pkt, 0x14) = 0xD80140;
		x::SSIGPU_InsertPrimAltViewport(bucket, pkt);
		pkt += 0x18;
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAutoDepth(bucket, pkt);
		MEM<uint32_t>(PACKET_CURSOR) = pkt + 0xC;
	}

	// ------------------------------------------------------------------------------------
	// shared state handlers
	// ------------------------------------------------------------------------------------
	// 0x888BA0 (sub_888BA0): fade table 0x161075C at index clamp(i, 0, 11)
	static int16_t FadeAt(int16_t i)
	{
		if (i > 0xB) i = 0xB;
		else if (i < 0) i = 0;
		return MEM<int16_t>(FADE_TABLE + 2 * (int32_t)i);
	}

	// 0x888E70 (sub_888E70): next sequence frame; past the last one (+0x52): hidden, last frame, 1
	static uint32_t FrameStep(uint32_t node)
	{
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 1);
		const int16_t f = S16(node, 0x50), last = S16(node, 0x52);
		if (f > last)
		{
			U8(node, 0x26) |= 4;
			U16(node, 0x50) = (uint16_t)last;
			return 1;
		}
		return 0;
	}

	// 0x888DA0 (sub_888DA0): a random distance < r along `angle` added to x / z
	static void RandomOffset(uint32_t node, uint32_t r, uint32_t angle)
	{
		const int32_t q = (int32_t)x::CrtRand();
		const int32_t d = (int16_t)(q % (int32_t)(int16_t)r);
		const int32_t a = (int16_t)angle;
		const int32_t s = (int32_t)x::computeSin((uint32_t)a);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)(mul32(s, d) / 4096));
		const int32_t c = (int32_t)x::computeCosine((uint32_t)a);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)(mul32(c, d) / 4096));
	}

	// the sparks' start (0x888CD0 shift 3, 0x889920 shift 4): origin +0x58 = the position, a random
	// offset (radius +0x6C, angle +0x6A), hidden, spark sequence (16 frames); speed = offset / 2^shift,
	// deceleration = -speed / 16
	static void SparkStart(uint32_t node, int shift)
	{
		U32(node, 0x58) = U32(node, 0x1C);
		U32(node, 0x5C) = U32(node, 0x20);
		RandomOffset(node, U16(node, 0x6C), U16(node, 0x6A));
		const int32_t dx = S16(node, 0x1C) - S16(node, 0x58);
		U8(node, 0x26) |= 4;
		U32(node, 0x4C) = SEQ_Spark;
		U16(node, 0x52) = 0xF;
		const int32_t dy = S16(node, 0x1E) - S16(node, 0x5A);
		const int32_t dz = S16(node, 0x20) - S16(node, 0x5C);
		const int32_t div = 1 << shift;
		const int16_t vx = (int16_t)(dx / div), vy = (int16_t)(dy / div), vz = (int16_t)(dz / div);
		U16(node, 0x58) = (uint16_t)vx;
		U16(node, 0x60) = (uint16_t)(-((int32_t)vx / 16));
		U16(node, 0x5A) = (uint16_t)vy;
		U16(node, 0x62) = (uint16_t)(-((int32_t)vy / 16));
		U16(node, 0x5C) = (uint16_t)vz;
		U16(node, 0x64) = (uint16_t)(-((int32_t)vz / 16));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
	}

	// 0x888CD0 (sub_888CD0): burst spark start
	static uint32_t __cdecl SparkStart8(uint32_t a1) { SparkStart(a1, 3); return 0; }

	// 0x889920 (sub_889920): landing spark start (at the root emitter's anchor)
	static uint32_t __cdecl SparkStart16(uint32_t a1)
	{
		cx::CopyAnchorFromSource(a1, a1 + 0x1C);
		SparkStart(a1, 4);
		return 0;
	}

	// 0x888E00 / 0x8893C0 / 0x8899F0: delay +0x68 counts down, then shown
	static uint32_t __cdecl Delay(uint32_t a1)
	{
		U16(a1, 0x68) = (uint16_t)(U16(a1, 0x68) - 1);
		if (S16(a1, 0x68) <= 0)
		{
			U8(a1, 0x26) &= 0xFB;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	// 0x888E20 / 0x889A10: speed += deceleration, position += speed, next frame (end after the last)
	static uint32_t __cdecl SparkMove(uint32_t a1)
	{
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + U16(a1, 0x60));
		U16(a1, 0x5A) = (uint16_t)(U16(a1, 0x5A) + U16(a1, 0x62));
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) + U16(a1, 0x64));
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + U16(a1, 0x58));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + U16(a1, 0x5A));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + U16(a1, 0x5C));
		if (FrameStep(a1))
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	// 0x8893E0 / 0x889C90: next frame (end after the last)
	static uint32_t __cdecl FramePlay(uint32_t a1)
	{
		if (FrameStep(a1))
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	// 0x888B10 (sub_888B10) / 0x889800 (sub_889800, at the root emitter's anchor): ring start,
	// scale 0x100, model 0x16104B4
	static uint32_t __cdecl RingStart(uint32_t a1)
	{
		S32(a1, 0x38) = 0x100;
		S32(a1, 0x34) = 0x100;
		S32(a1, 0x30) = 0x100;
		U32(a1, 0x4C) = MODEL_Ring;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}
	static uint32_t __cdecl LandRingStart(uint32_t a1)
	{
		cx::CopyAnchorFromSource(a1, a1 + 0x1C);
		return RingStart(a1);
	}

	// 0x888B40 (+0x300) / 0x889840 (+0x200): fade from the table at tick - 3, grow; at full fade
	// hidden and finished
	static void RingGrow(uint32_t a1, int32_t step)
	{
		const int16_t fade = FadeAt((int16_t)(U16(a1, 0x24) - 3));
		S32(a1, 0x30) = add32(S32(a1, 0x30), step);
		U16(a1, 0x50) = (uint16_t)fade;
		S32(a1, 0x34) = add32(S32(a1, 0x34), step);
		S32(a1, 0x38) = add32(S32(a1, 0x38), step);
		if (fade >= 0x1000)
		{
			U8(a1, 0x26) |= 5;
			U16(a1, 0x50) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
	}
	static uint32_t __cdecl RingGrow300(uint32_t a1) { RingGrow(a1, 0x300); return 0; }
	static uint32_t __cdecl RingGrow200(uint32_t a1) { RingGrow(a1, 0x200); return 0; }

	// 0x8890E0 / 0x8891D0: shock wave start, 0x300 up, scale 0x1000, model
	static void WaveStart(uint32_t a1, uint32_t model)
	{
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0xFD00);
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U32(a1, 0x4C) = model;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
	}
	static uint32_t __cdecl WaveAStart(uint32_t a1) { WaveStart(a1, MODEL_WaveA); return 0; }
	static uint32_t __cdecl WaveBStart(uint32_t a1) { WaveStart(a1, MODEL_WaveB); return 0; }

	// 0x889110: fade in 0x200 per tick, grow 0x400; at full fade hidden and finished
	static uint32_t __cdecl WaveAGrow(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		const int16_t fade = S16(a1, 0x50);
		S32(a1, 0x30) = add32(S32(a1, 0x30), 0x400);
		S32(a1, 0x34) = add32(S32(a1, 0x34), 0x400);
		S32(a1, 0x38) = add32(S32(a1, 0x38), 0x400);
		if (fade >= 0x1000)
		{
			U8(a1, 0x26) |= 5;
			U16(a1, 0x50) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	// 0x889200: shrink 0x200 per tick; gone at scale 0
	static uint32_t __cdecl WaveBShrink(uint32_t a1)
	{
		const int32_t sx = sub32(S32(a1, 0x30), 0x200);
		S32(a1, 0x34) = sub32(S32(a1, 0x34), 0x200);
		S32(a1, 0x38) = sub32(S32(a1, 0x38), 0x200);
		S32(a1, 0x30) = sx;
		if (sx <= 0)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			S32(a1, 0x38) = 0;
			S32(a1, 0x34) = 0;
			S32(a1, 0x30) = 0;
		}
		return 0;
	}

	// 0x889310 (sub_889310): a random offset of length < r (0 -> 1) in a random direction
	static void DustOffset(uint32_t node, uint32_t r)
	{
		int32_t b = (int32_t)r;
		if ((int16_t)b == 0) b = 1;
		const int32_t yaw = (int32_t)(x::CrtRand() & 0xFFF);
		const int32_t roll = (int32_t)(x::CrtRand() & 0xFFF);
		Mat4x3 m = {};
		x::MAG_022_sub_8DD770(P(&m));
		x::MAG_022_sub_8DD8A0(P(&m), (uint32_t)(int32_t)(int16_t)yaw);
		x::sub_8DD7E0(P(&m), (uint32_t)(int32_t)(int16_t)roll);
		// 4th word never written by the original (stack), never read by matrixMultiplyVector
		int16_t v[4] = { 0, 0, 0, 0 };
		const int32_t q = (int32_t)x::CrtRand();
		v[2] = (int16_t)(q % (int32_t)(int16_t)b);
		int16_t w[4] = { 0, 0, 0, 0 };
		x::matrixMultiplyVector(P(&m), P(v), P(w));
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)w[0]);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + (uint16_t)w[1]);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)w[2]);
	}

	// 0x8892C0: dust start - at the root emitter's anchor, 0x300 up, random offset < 0x180, hidden,
	// dust sequence (8 frames), camera offset -0x200
	static uint32_t __cdecl DustStart(uint32_t a1)
	{
		cx::CopyAnchorFromSource(a1, a1 + 0x1C);
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0xFD00);
		DustOffset(a1, 0x180);
		U8(a1, 0x26) |= 4;
		U32(a1, 0x4C) = SEQ_Dust;
		U16(a1, 0x52) = 7;
		U16(a1, 0x54) = 0xFE00;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// 0x889C60: walk-back dust start - at the root emitter's anchor, spark sequence (16 frames)
	static uint32_t __cdecl WalkDustStart(uint32_t a1)
	{
		cx::CopyAnchorFromSource(a1, a1 + 0x1C);
		U32(a1, 0x4C) = SEQ_Spark;
		U16(a1, 0x52) = 0xF;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// state tables of the drawing tasks (the original's stack tables; nullsubs = Nop)
	static const StateFn T_BurstRing[] = { RingStart, RingGrow300, Nop };           // 0x888B10 0x888B40 0x888BE0
	static const StateFn T_Spark[] = { SparkStart8, Delay, SparkMove, Nop };        // 0x888CD0 0x888E00 0x888E20 0x888EA0
	static const StateFn T_WaveA[] = { WaveAStart, WaveAGrow, Nop };                // 0x8890E0 0x889110 0x889160
	static const StateFn T_WaveB[] = { WaveBStart, WaveBShrink, Nop };              // 0x8891D0 0x889200 0x889240
	static const StateFn T_Dust[] = { DustStart, Delay, FramePlay, Nop };           // 0x8892C0 0x8893C0 0x8893E0 0x889400
	static const StateFn T_LandRing[] = { LandRingStart, RingGrow200, Nop };        // 0x889800 0x889840 0x8898A0
	static const StateFn T_LandSpark[] = { SparkStart16, Delay, SparkMove, Nop };   // 0x889920 0x8899F0 0x889A10 0x889A60
	static const StateFn T_WalkDust[] = { WalkDustStart, FramePlay, Nop };          // 0x889C60 0x889C90 0x889CB0

	// ------------------------------------------------------------------------------------
	// drawing tasks: state handler, then the draw
	// ------------------------------------------------------------------------------------
	static inline uint32_t PrimTask(uint32_t node, const StateFn *t)
	{
		t[S8(node, 0x29)](node);
		PrimDraw(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}
	static inline uint32_t SpriteTask(uint32_t node, const StateFn *t)
	{
		t[S8(node, 0x29)](node);
		SpriteDraw(node, U16(node, 0x6E));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}
	static uint32_t __cdecl BurstRing(uint32_t a1) { return PrimTask(a1, T_BurstRing); }   // 0x8889F0
	static uint32_t __cdecl WaveA(uint32_t a1) { return PrimTask(a1, T_WaveA); }           // 0x889080
	static uint32_t __cdecl WaveB(uint32_t a1) { return PrimTask(a1, T_WaveB); }           // 0x889170
	static uint32_t __cdecl LandRing(uint32_t a1) { return PrimTask(a1, T_LandRing); }     // 0x8897A0
	static uint32_t __cdecl Spark(uint32_t a1) { return SpriteTask(a1, T_Spark); }         // 0x888BF0
	static uint32_t __cdecl Dust(uint32_t a1) { return SpriteTask(a1, T_Dust); }           // 0x889250
	static uint32_t __cdecl LandSpark(uint32_t a1) { return SpriteTask(a1, T_LandSpark); } // 0x8898B0
	static uint32_t __cdecl WalkDust(uint32_t a1) { return SpriteTask(a1, T_WalkDust); }   // 0x889BF0

	// ------------------------------------------------------------------------------------
	// screen flash (0x888F80): colour script +0x30, current colour +0x34, index +0x38
	// ------------------------------------------------------------------------------------
	// the script step; true = the task draws its quad (the original also stores the script word
	// over its own stack argument slot: never read again)
	static bool FlashStep(uint32_t node)
	{
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF)
		{
			U8(node, 0x26) |= 1;
			return false;
		}
		if ((hi >> 8) == 0xFE)
		{
			if (S16(node, 0x24) >= (int16_t)(hi & 0xFF))
			{
				const uint16_t next = (uint16_t)(idx + 1);
				U16(node, 0x38) = next;
				U32(node, 0x34) = U32(script, (int32_t)(int16_t)next * 4);
			}
		}
		else
		{
			U32(node, 0x34) = entry;
			U16(node, 0x38) = (uint16_t)(idx + 1);
		}
		return true;
	}

	static uint32_t __cdecl Flash(uint32_t a1)
	{
		const uint32_t node = a1;
		if (FlashStep(node))
		{
			// 30 fps layer: see mag026_clash_held.inc
			FX_HELD(held_note_flash(node);)
			FlashDraw(U32(node, 0x34));
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// ------------------------------------------------------------------------------------
	// knock-back (0x889410): moves the target entity (+0x2D); no draw
	// ------------------------------------------------------------------------------------
	// 0x889480: chain transformation 6, start position (+0x48 / +0x50), flag 0x1000 set (old word
	// +0x58), velocity along the facing (0x100), upward speed / spin divisor by the entity's size
	// class (+4)
	static uint32_t __cdecl ChargeStart(uint32_t a1)
	{
		const uint32_t e = EntityOf(a1);
		cx::QueueChainTransformation(e, 6);
		const uint32_t p0 = U32(e, 0x1C), p1 = U32(e, 0x20);
		U32(a1, 0x50) = p0;
		U32(a1, 0x54) = p1;
		const uint16_t w0 = U16(e, 0);
		U32(a1, 0x48) = p0;
		U16(e, 0) = (uint16_t)(w0 | 0x1000);
		const int16_t face = S16(e, 0xE);
		U32(a1, 0x4C) = p1;
		U32(a1, 0x58) = w0;
		U16(a1, 0x42) = (uint16_t)face;
		const int32_t s = (int32_t)x::computeSin((uint32_t)(int32_t)face);
		U16(a1, 0x30) = (uint16_t)(shl32(s, 8) / 4096);
		const int32_t c = (int32_t)x::computeCosine((uint32_t)(int32_t)S16(a1, 0x42));
		U16(a1, 0x34) = (uint16_t)(shl32(c, 8) / 4096);
		switch (U8(e, 4))
		{
		case 0:
			U16(a1, 0x32) = 0xFEB0;
			U16(a1, 0x3A) = 0x30;
			U16(a1, 0x5E) = 0xC;
			break;
		case 1:
			U16(a1, 0x32) = 0xFE80;
			U16(a1, 0x3A) = 0x30;
			U16(a1, 0x5E) = 0xE;
			break;
		default:
			U16(a1, 0x32) = 0xFE40;
			U16(a1, 0x3A) = 0x30;
			U16(a1, 0x5E) = 0x10;
			break;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// one landing spark (0x8898B0) of the three: delay, angle base + random 0..0x1FF, scale
	static void LandSparkSpawn(uint32_t node, int16_t delay, uint16_t angle)
	{
		const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_LandSpark, 0x70, node);
		U16(s, 0x68) = (uint16_t)delay;
		const uint32_t r = x::CrtRand() & 0x3FF;
		U16(s, 0x6C) = 0xC0;
		U16(s, 0x6E) = (uint16_t)(r + 0x600);
		U16(s, 0x6A) = angle;
		const uint32_t r2 = x::CrtRand() & 0x1FF;
		U16(s, 0x6A) = (uint16_t)(((uint16_t)(r2 + U16(s, 0x6A))) & 0xFFF);
	}

	// 0x889580: spin about x (entity +0xC, a full turn in +0x5E ticks), fly (gravity +0x3A); at the
	// entity's ground height (+0x24): landing (chain transformation 6, ring, 3 sparks), next state;
	// the entity follows the node, turned about a point 0x200 above its feet
	static uint32_t __cdecl ChargeFly(uint32_t a1)
	{
		const uint32_t e = EntityOf(a1);
		const int32_t q = (int32_t)0xFFFFF000 / (int32_t)S16(a1, 0x5E);
		U16(a1, 0x40) = (uint16_t)(U16(a1, 0x40) + (uint16_t)q);
		const int16_t spin = S16(a1, 0x40);
		if (spin <= (int16_t)0xF000)
			U16(e, 0xC) = 0;
		else
			U16(e, 0xC) = (uint16_t)(spin & 0xFFF);
		U16(a1, 0x32) = (uint16_t)(U16(a1, 0x32) + U16(a1, 0x3A));
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + U16(a1, 0x30));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + U16(a1, 0x32));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + U16(a1, 0x34));
		const int16_t ground = S16(e, 0x24);
		if (S16(a1, 0x1E) >= ground)
		{
			U16(a1, 0x1E) = (uint16_t)ground;
			cx::QueueChainTransformation(e, 6);
			x::Effect_AddTaskAndInitFromCtx(Q_PRIM, ORIG_LandRing, 0x6C, a1);
			const uint32_t base = x::CrtRand() & 0xFFF;
			LandSparkSpawn(a1, 0, (uint16_t)base);
			LandSparkSpawn(a1, 2, (uint16_t)(base + 0x500));
			LandSparkSpawn(a1, 4, (uint16_t)(base + 0xA00));
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		Mat4x3 m = {};
		x::ComposeZYXRotationMatrix(e + 0xC, P(&m));
		// 4th word never written by the original (stack), never read by matrixMultiplyVector
		int16_t v[4] = { 0, 0x200, 0, 0 };
		int16_t w[4] = { 0, 0, 0, 0 };
		x::matrixMultiplyVector(P(&m), P(v), P(w));
		U16(e, 0x1C) = (uint16_t)(U16(a1, 0x1C) + (uint16_t)w[0]);
		U16(e, 0x1E) = (uint16_t)((uint16_t)(U16(a1, 0x1E) + (uint16_t)w[1]) - 0x200);
		U16(e, 0x20) = (uint16_t)(U16(a1, 0x20) + (uint16_t)w[2]);
		return 0;
	}

	// 0x889A70: unless the entity's anim state (+0x74 -> byte 0) is 6: chain transformation 0x1A,
	// turned round, walk-back speed = (start - position) / 16 for 16 ticks
	static uint32_t __cdecl ChargeTurn(uint32_t a1)
	{
		const uint32_t e = EntityOf(a1);
		if (U8(U32(e, 0x74), 0) == 6)
			return 0;
		cx::QueueChainTransformation(e, 0x1A);
		U16(e, 0xE) = (uint16_t)((U16(e, 0xE) - 0x800) & 0xFFF);
		U16(a1, 0x5C) = 0x10;
		U16(a1, 0x30) = (uint16_t)((S16(a1, 0x48) - S16(e, 0x1C)) / 16);
		U16(a1, 0x34) = (uint16_t)((S16(a1, 0x4C) - S16(e, 0x20)) / 16);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// 0x889B00: walk back; at the end flag 0x1000 restored, idle chain (+0x74 -> byte 2), facing,
	// start position, action result, finished; a dust sprite every 4th tick
	static uint32_t __cdecl ChargeWalk(uint32_t a1)
	{
		const uint32_t e = EntityOf(a1);
		U16(e, 0x1C) = (uint16_t)(U16(e, 0x1C) + U16(a1, 0x30));
		U16(e, 0x20) = (uint16_t)(U16(e, 0x20) + U16(a1, 0x34));
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
		if (S16(a1, 0x5C) <= 0)
		{
			const uint16_t bit = (uint16_t)((U16(a1, 0x58) ^ U16(e, 0)) & 0x1000);
			U16(e, 0) = (uint16_t)(U16(e, 0) ^ bit);
			cx::QueueChainTransformation(e, U8(U32(e, 0x74), 2));
			U16(e, 0xE) = (uint16_t)((U16(e, 0xE) - 0x800) & 0xFFF);
			U16(e, 0x1C) = U16(a1, 0x48);
			U16(e, 0x20) = U16(a1, 0x4C);
			const uint32_t actions = U32(U32(a1, 0xC), 4);
			const uint32_t targets = U32(actions + 20 * (int32_t)S8(a1, 0x2A), 8);
			x::ApplyActionResultToTarget(targets + 24 * (int32_t)S8(a1, 0x2B));
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		if ((U8(a1, 0x24) & 3) == 0)
		{
			const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_WalkDust, 0x70, a1);
			U16(s, 0x6E) = (uint16_t)((x::CrtRand() & 0x1FF) + 0x400);
		}
		return 0;
	}

	static const StateFn T_Charge[] = { ChargeStart, ChargeFly, ChargeTurn, ChargeWalk, Nop }; // 0x889480 0x889580 0x889A70 0x889B00 0x889CC0

	static uint32_t __cdecl Charge(uint32_t a1)
	{
		T_Charge[S8(a1, 0x29)](a1);
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		return task_end(a1, status);
	}

	// ------------------------------------------------------------------------------------
	// emitter
	// ------------------------------------------------------------------------------------
	// 0x8888F0 (MAG_026_sub_8888F0): a burst at the caster's bone `bone`: a ring (flag 1) and 8
	// sparks on an eighth-turn star from a random angle (jitter -0x80..0x7F), random delay 0..3,
	// radius 0x100, random scale 0x600..0x9FF; the position is taken at ground level (y = 0).
	// The position's 4th word (copied into the ring / sparks +0x22) is never written by the
	// original: a stack word (`stale`, see Emitter).
	static void Burst(uint32_t node, int16_t bone, int16_t flag, uint16_t stale)
	{
		int16_t pos[4];
		pos[3] = (int16_t)stale;
		const uint32_t caster = 0x1D972C0 + 0x9C * (uint32_t)U8(U32(node, 0xC), 0);
		x::GetEffectSpawnPosition(caster, (uint32_t)(int32_t)bone, 0x1800, P(pos));
		pos[1] = 0;
		if (flag == 1)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, ORIG_BurstRing, 0x6C, node);
			U32(t, 0x1C) = U32(P(pos), 0);
			U32(t, 0x20) = U32(P(pos), 4);
		}
		uint32_t angle = x::CrtRand() & 0xFFF;
		for (int i = 0; i < 8; i++)
		{
			const uint32_t s = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Spark, 0x70, node);
			U32(s, 0x1C) = U32(P(pos), 0);
			U32(s, 0x20) = U32(P(pos), 4);
			U16(s, 0x68) = (uint16_t)(x::CrtRand() & 3);
			const uint32_t r = x::CrtRand() & 0x3FF;
			U16(s, 0x6C) = 0x100;
			U16(s, 0x6A) = (uint16_t)angle;
			U16(s, 0x6E) = (uint16_t)(r + 0x600);
			const uint32_t r2 = x::CrtRand() & 0xFF;
			angle += 0x200;
			U16(s, 0x6A) = (uint16_t)(((uint32_t)(uint16_t)(r2 + U16(s, 0x6A)) - 0x80) & 0xFFF);
		}
	}

	// 0x888EB0 (MAG_026_sub_888EB0): emitter state 0 - its position = its anchor
	static uint32_t __cdecl EmitterStart(uint32_t a1)
	{
		cx::CopyAnchorFromSource(a1, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// 0x888ED0 (MAG_026_sub_888ED0): emitter state 1 - from tick 4: the shock waves, the knock-back,
	// 4 dust sprites (delays 0 / 2 / 4 / 6) and, for target 0, the screen flash
	static uint32_t __cdecl EmitterSpawn(uint32_t a1)
	{
		if (S16(a1, 0x24) < 4)
			return 0;
		x::Effect_AddTaskAndInitFromCtx(Q_PRIM, ORIG_WaveA, 0x6C, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_PRIM, ORIG_WaveB, 0x6C, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_CHARGE, ORIG_Charge, 0x60, a1);
		for (int i = 0; i < 4; i++)
		{
			const uint32_t d = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Dust, 0x70, a1);
			U16(d, 0x68) = (uint16_t)(i * 2);
			U16(d, 0x6E) = (uint16_t)((x::CrtRand() & 0x3FF) + 0x600);
		}
		if (U8(a1, 0x2B) == 0)
		{
			const uint32_t f = x::Effect_AddTaskAndInitFromCtx(Q_FLASH, ORIG_Flash, 0x3C, a1);
			U32(f, 0x30) = FLASH_SCRIPT;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// 0x889CD0 (MAG_026_sub_889CD0): emitter state 2 - finished after tick 30
	static uint32_t __cdecl EmitterWait(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x1E)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	static const StateFn T_Emitter[] = { EmitterStart, EmitterSpawn, EmitterWait, Nop }; // 0x888EB0 0x888ED0 0x889CD0 0x889CF0

	// 0x8887F0 (MAG_026_sub_8887F0): EMITTER task (one per target)
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		T_Emitter[S8(node, 0x29)](node);
		if (U8(node, 0x2B) == 0)
		{
			if (U16(node, 0x24) == 4)
				x::BdPlaySE(SOUND_Clash, 0, 0x80);
			if (U8(node, 0x2B) == 0)
			{
				const uint16_t c = U16(node, 0x24);
				// the 4th word of a burst's position (never written, see Burst): the original makes
				// the four calls without popping their arguments, so the 2nd..4th burst's word is the
				// high half of the ebp the previous burst saved = the executor's queue pointer (the
				// emitter queue 0x26E3F68); for the first / a single burst it is the stack left by the
				// emitter's earlier calls (0)
				const uint16_t queue_hi = (uint16_t)(Q_EMITTER >> 16);
				if (c == 4 || c == 0x28)
				{
					Burst(node, 0x34, 1, 0);
					Burst(node, 0x35, 1, queue_hi);
					Burst(node, 0x36, 1, queue_hi);
					Burst(node, 0x33, 1, queue_hi);
				}
				else if (c == 0x11)
					Burst(node, 0x36, 0, 0);
				else if (c == 0x14)
					Burst(node, 0x34, 0, 0);
			}
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// ------------------------------------------------------------------------------------
	// master
	// ------------------------------------------------------------------------------------
	// 0x8887A0 / 0x889D00: next state
	static uint32_t __cdecl NextState(uint32_t a1)
	{
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8887B0 (MAG_026_sub_8887B0): one emitter per target of action 0 (+0x2B = target index)
	static uint32_t __cdecl SpawnEmitters(uint32_t a1)
	{
		for (int32_t i = 0; i < (int32_t)S16(a1, 0x5A); i++)
		{
			x::Effect_AddTaskAndInitFromCtx(Q_EMITTER, ORIG_Emitter, 0x58, a1);
			U8(a1, 0x2B) = (uint8_t)(U8(a1, 0x2B) + 1);
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	// 0x889D10 (MAG_026_sub_889D10): no task left (+0x5E == 0): finished
	static uint32_t __cdecl WaitQueues(uint32_t a1)
	{
		if (U16(a1, 0x5E) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	static const StateFn T_Master[] = { NextState, SpawnEmitters, NextState, WaitQueues, Nop }; // 0x8887A0 0x8887B0 0x889D00 0x889D10 0x889D30

	// 0x8886C0 (MAG_026_sub_8886C0): MASTER task
	static uint32_t __cdecl Master(uint32_t a1)
	{
		const uint32_t node = a1;
		// 30 fps layer: see mag026_clash_held.inc
		FX_HELD(held_note_master();)
		if (U8(node, 0x5C) & 1)
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_ODD);
		else
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_EVEN);
		x::Effect_UpdateTargetPosFromBones(node);
		T_Master[S8(node, 0x29)](node);
		U16(node, 0x5E) = (uint16_t)x::ExecuteTaskQueue(Q_EMITTER);
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_CHARGE));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SPRITE));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PRIM));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_FLASH));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}
}
}

	void register_mag026_clash()
	{
		register_port(act::clash026::ORIG_Master, (void *)act::clash026::Master, "026 Clash Master", 26);
		register_port(act::clash026::ORIG_Emitter, (void *)act::clash026::Emitter, "026 Emitter", 26);
		register_port(act::clash026::ORIG_Charge, (void *)act::clash026::Charge, "026 KnockBack", 26);
		register_port(act::clash026::ORIG_BurstRing, (void *)act::clash026::BurstRing, "026 BurstRing", 26);
		register_port(act::clash026::ORIG_WaveA, (void *)act::clash026::WaveA, "026 WaveA", 26);
		register_port(act::clash026::ORIG_WaveB, (void *)act::clash026::WaveB, "026 WaveB", 26);
		register_port(act::clash026::ORIG_LandRing, (void *)act::clash026::LandRing, "026 LandRing", 26);
		register_port(act::clash026::ORIG_Spark, (void *)act::clash026::Spark, "026 Spark", 26);
		register_port(act::clash026::ORIG_Dust, (void *)act::clash026::Dust, "026 Dust", 26);
		register_port(act::clash026::ORIG_LandSpark, (void *)act::clash026::LandSpark, "026 LandSpark", 26);
		register_port(act::clash026::ORIG_WalkDust, (void *)act::clash026::WalkDust, "026 WalkDust", 26);
		register_port(act::clash026::ORIG_Flash, (void *)act::clash026::Flash, "026 Flash", 26);
		// 30 fps layer: see mag026_clash_held.inc
		FX_HELD(register_mag026_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag026_clash_held.inc"
#endif
