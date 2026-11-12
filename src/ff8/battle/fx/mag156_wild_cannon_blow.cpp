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

// Effect 156: Wild Cannon Blow (enemy attack 70 of kernel.bin; SAM08G c0m007; MAG_156_WILD_CANNON_BLOW*). Four Cannon Blow
// shots (mag151_cannon_blow.cpp) from alternating muzzles.
//
// Structure (file loader 0x605510 = the texture file 0xDB7DB8 -> 0x23F03C0; _Init 0x605530 keeps the texture base 0x23F07E8, the
// context 0x23F03B8, the first target's slot 0x23ECE40 and the caster's slot 0x23F03BC, sets up the root queue 0x23ECE68 (pool
// 0x23ECE48, 2 x 0x10) with RootTask, the shot queue 0x23EF478 (pool 0x23EF488, 100 x 0x24) with MasterTask, frees the three
// particle arrays (A 0x23ECE78 and B 0x23EDC88: 150 x 0x18, C 0x23EEA98: 100 x 0x18), plays the camera animation 0xDB4CD4 and
// queues the TIM upload; both run original):
//   RootTask (0x606C20) / TexRestoreTask (0x606C90) - as Cannon Blow's.
//   MasterTask (0x605620) - nothing on draw-only ticks; counters 1 / 4 / 7 / 10: a ShotTask with the next shot index; counter 1:
//     the sounds 0xDB7DAC and 0xDB7DB0; counter 0x24: the action result of every target; ends after counter 0x2D.
//   ShotTask (0x6056E0) - nothing on draw-only ticks. Counter 0: Cannon Blow's shot set-up from the caster's effect point 0xF
//     with x offset 0xC8 (odd shots) or point 9 with x offset -0xC8 (even shots), target x +-800 / z +-400, then FlashTask,
//     BurstTask, RingTask, BallTask, TrailTask, Trail2Task. Counter 0xD: ImpactTask and ImpactRingTask at the target. Counter
//     0xF: an EmitterTask at every target's effect point 0xF1. Counters 0xD..0x10: the camera shake word 0x1D97712 +-0x40 (odd
//     +, even -). Ends at counter 0x14.
//   FlashTask (0x605DF0, model 0xDB6DFC), BurstTask (0x605F40, particles C, sequence 0xDB5300), RingTask (0x606200, model
//     0xDB68EC), BallTask (0x606320, sequence 0xDB528C), ImpactRingTask (0x606910, model 0xDB7484) - Cannon Blow's 0x60D260 /
//     0x60D3B0 / 0x60D670 / 0x60D790 / 0x60E060 instruction for instruction.
//   TrailTask (0x6063E0, particles A, sequence 0xDB4F34) / Trail2Task (0x6065A0, particles B, sequence 0xDB50E0, frame base 0) -
//     Cannon Blow's trails with 150-particle arrays.
//   ImpactTask (0x6067B0) - prim model 0xDB54AC at the target (flags 3, faded 0xC3 (counter - 8) << 9 from 8), y angle + spin,
//     scale + step as Cannon Blow's (no scrolling model); ends after counter 16.
//   EmitterTask (0x606A60) - particles C of its mask (sequence 0xDB4D88, R = size, pull -size >> 4; frame + 1, freed when ended);
//     counters 0..2: one new particle at its point +-300 on each axis (size 0x500..0x9FF); ends from counter 4 once none is left.
// DRAW ORDER: every task draws in the tick it updates (the particle tasks spawn after their draw); the order of the shot queue.
// ENTITY WRITES: none. CAMERA: the camera animation 0xDB4CD4 (engine) and the shake word 0x1D97712 (ShotTask).
// Module globals: 0x23ECE40..0x23F07F0.
// NEVER-WRITTEN STACK WORDS: none read (the shot set-up's offset vector's 4th word is not read by 0x56C4F0).

#include "mag_common.h"

namespace ff8fx
{
namespace wildcannon156
{
	using namespace eng;
	using namespace magc;

	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t S16v(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x23F03B8); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x23F03BC); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x23F07E8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23F07EC); }
	inline TaskQueue *RootQueue() { return (TaskQueue *)0x23ECE68; }
	inline TaskQueue *ShotQueue() { return (TaskQueue *)0x23EF478; }
	static const uint32_t PARTS_A = 0x23ECE78, PARTS_B = 0x23EDC88, PARTS_C = 0x23EEA98, PARTS_END = 0x23EF3F8; // 150 / 150 / 100 x 0x18
	inline uint32_t Ctrl(int i) { return 0x23F0298 + (uint32_t)i * 0x20; }   // 4 control points (8 bytes)
	inline uint32_t Target(int i) { return 0x23F0318 + (uint32_t)i * 8; }
	inline Mat4x3 *Rot(int i) { return (Mat4x3 *)(0x23F0338 + (uint32_t)i * 0x20); }
	inline uint32_t Start(int i) { return 0x23F03C8 + (uint32_t)i * 8; }
	inline uint32_t Point(int i, int k) { return 0x23F03E8 + ((uint32_t)i * 0x20 + (uint32_t)k) * 8; } // 32 curve points

	static const uint32_t ORIG_Root = 0x606C20, ORIG_TexRestore = 0x606C90, ORIG_Master = 0x605620, ORIG_Shot = 0x6056E0,
		ORIG_Flash = 0x605DF0, ORIG_Burst = 0x605F40, ORIG_Ring = 0x606200, ORIG_Ball = 0x606320, ORIG_Trail = 0x6063E0,
		ORIG_Trail2 = 0x6065A0, ORIG_Impact = 0x6067B0, ORIG_ImpactRing = 0x606910, ORIG_Emitter = 0x606A60;
	static const uint32_t MODEL_Flash = 0xDB6DFC, MODEL_Ring = 0xDB68EC, MODEL_Impact = 0xDB54AC, MODEL_ImpactRing = 0xDB7484;
	static const uint32_t SEQ_Burst = 0xDB5300, SEQ_Ball = 0xDB528C, SEQ_Trail = 0xDB4F34, SEQ_Trail2 = 0xDB50E0, SEQ_Emitter = 0xDB4D88;
	static const uint32_t SOUND_A = 0xDB7DAC, SOUND_B = 0xDB7DB0;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline int32_t SpawnPosition(uint8_t *entity, int32_t point, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, point, flag, out); }
	inline void CentrePosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); } // computeCosine

	// a pool / particle record (0x18 bytes): +0 owner mask, +4 frame, +6 size, +8 point (4 words), +0x10 velocity (3 words)
	enum { P_MASK = 0, P_FRAME = 4, P_SIZE = 6, P_POS = 8, P_VEL = 0x10 };

}
}

#ifdef FF8_FX_HELD
#include "mag156_wild_cannon_blow_held.h"
#endif

namespace ff8fx
{
namespace wildcannon156
{
	// ------------------------------------------------------------------
	// Shared draws
	// ------------------------------------------------------------------
	// a prim model (header 0x58) at the GTE matrix set by the caller: flags `flags`, faded (flags | 0xC0, +0x0C) when fade >= 0
	static void PrimDraw(uint32_t model, int32_t fade, uint32_t flags = 0x33)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = model;
		U32(h, 8) = 0;
		U32(h, 0x1C) = flags;
		if (fade >= 0)
		{
			U32(h, 0x1C) = flags | 0xC0;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// a model frame: angles (0, y, 0) or (0, 0, z), uniform scale, translation t
	static void ScaledFrame(const int16_t *angles, int16_t scale, Mat4x3 *m)
	{
		ComposeZYXRotationMatrix(angles, m);
		const int32_t s[3] = { scale, scale, scale };
		Scale3DMatrix(m, s);
	}

	// a sequence frame at the shadow-rotation camera of `pos` (header 0xB4 already filled but its frame)
	static void SeqDraw(uint32_t h, const void *pos, int16_t r, int32_t pull, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, (int32_t)(uint16_t)r, pull);
		U16(h, 4) = (uint16_t)frame;
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	// the first free particle of [lo, hi) (mask 0), or 0 (none in the first `cap`)
	static uint32_t FreeParticle(uint32_t lo, uint32_t hi, int32_t cap)
	{
		int32_t i = 0;
		uint32_t p = lo;
		for (; U32(p, P_MASK) != 0; p += 0x18, i++)
			if (p + 0x18 >= hi) return 0;
		return i < cap ? p : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x606C20) and texture restore (0x606C90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? TexBase() + 0xC000 : TexBase();
		const int running = ExecuteTaskQueue(ShotQueue());
		if (!running)
		{
			const uint32_t w = (uint32_t)AddTaskToQueue(RootQueue(), ORIG_TexRestore);
			S16(w, 0xC) = 0;
			S16(w, 0xE) = 0;
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		if (S16(n, 0xC) == 1) TextureRestoreTask((void *)TexBase(), (void *)(n + 0xE));
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Flash (0x605DF0), Ring (0x606200), ImpactRing (0x606910)
	// ------------------------------------------------------------------
	static void FlashDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, 0, S16(n, 0x18) };
		Mat4x3 m, v;
		ScaledFrame(ang, S16(n, 0x1C), &m);
		m.t[0] = m.t[1] = m.t[2] = 0;
		ComposeAffineTransform(Rot(S16(n, 0xE)), &m, &m);
		ComposeAffineTransform(&Camera(), &m, &v);
		GteSetRotMatrix(&v);
		GteSetTransVector(&v);
		PrimDraw(MODEL_Flash, fade);
	}

	static uint32_t __cdecl FlashTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_FLASH, n);)
		FlashDraw(n, S16(n, 0xC) >= 4 ? shl32(S16(n, 0xC) - 4, 9) : -1);
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 10);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0xC ? TASK_END : 0;
	}

	static void RingDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		const Mat4x3 *r = Rot(S16(n, 0xE));
		m.t[0] = r->t[0];
		m.t[1] = 0;
		const int32_t s[3] = { S16(n, 0x1C), S16(n, 0x1C), S16(n, 0x1C) };
		Scale3DMatrix(&m, s);
		m.t[2] = r->t[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(MODEL_Ring, fade);
	}

	static uint32_t __cdecl RingTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_RING, n);)
		RingDraw(n, S16(n, 0xC) >= 0 ? shl32(S16(n, 0xC), 10) : -1);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 ? TASK_END : 0;
	}

	static void ImpactRingDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		const int32_t s[3] = { S16(n, 0x1C), S16(n, 0x1C), S16(n, 0x1C) };
		Scale3DMatrix(&m, s);
		m.t[2] = S16(n, 0x14);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(MODEL_ImpactRing, fade);
	}

	static uint32_t __cdecl ImpactRingTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_IRING, n);)
		ImpactRingDraw(n, S16(n, 0xC) >= 3 ? mul32(S16(n, 0xC) - 3, 0x199) : -1);
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 7);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0xD ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Impact (0x6067B0): a prim model at the target
	// ------------------------------------------------------------------
	static void ImpactDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		const int32_t s[3] = { S16(n, 0x1C), S16(n, 0x1C), S16(n, 0x1C) };
		Scale3DMatrix(&m, s);
		m.t[2] = S16(n, 0x14);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(MODEL_Impact, fade, 3);
	}

	static uint32_t __cdecl ImpactTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_IMPACT, n);)
		const int16_t c = S16(n, 0xC);
		ImpactDraw(n, c >= 8 ? shl32(c - 8, 9) : -1);
		if (DrawOnly()) return 0;
		const int16_t sp = S16(n, 0x1A);
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + sp);
		S16(n, 0x1A) = (int16_t)(sp - (int16_t)(sp >> 3));
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 5);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ball (0x606320)
	// ------------------------------------------------------------------
	static void BallDraw(uint32_t n, const void *pos)
	{
		const int16_t r = S16(n, 0x1C);
		TransformCameraByShadowRotation(pos, (int32_t)(uint16_t)r, -(r >> 3));
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		U16(h, 4) = (uint16_t)(U8(n, 0xC) & 3);
		U32(h, 0) = SEQ_Ball;
		U16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl BallTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_BALL, n);)
		BallDraw(n, (const void *)Point(S16(n, 0xE), S16(n, 0x18)));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1A);
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + d);
		if (d > 2) S16(n, 0x1A) = (int16_t)(d - 1);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0x18) >= 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	// one particle's draw (and, on an updating tick, frame + 1 / freed when its sequence ended): 1 = it lives on
	static bool ParticleDraw(uint32_t h, uint32_t p, const void *pos, int32_t pull_shift)
	{
		const int16_t size = S16(p, P_SIZE);
		SeqDraw(h, pos, size, -(size >> pull_shift), S16(p, P_FRAME));
		if (DrawOnly()) return false;
		S16(p, P_FRAME) = (int16_t)(S16(p, P_FRAME) + 1);
		if (S16(h, 0x28) < 0)
		{
			U32(p, P_MASK) = 0;
			return false;
		}
		return true;
	}

	// a burst particle's point: its own through the shot's rotation, + the start
	static void BurstPoint(int i, const int16_t *local, int16_t *w)
	{
		MatVec(Rot(i), local, w);
		w[0] = (int16_t)(w[0] + (int16_t)Rot(i)->t[0]);
		w[2] = (int16_t)(w[2] + (int16_t)Rot(i)->t[2]);
		w[1] = (int16_t)(w[1] + (int16_t)Rot(i)->t[1]);
	}

	// Burst (0x605F40): particles C in the shot's frame
	static int BurstParticles(uint32_t n)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int alive = 0;
		U32(h, 0) = SEQ_Burst;
		U16(h, 0x24) = 0;
		const int i = S16(n, 0xE);
		for (uint32_t p = PARTS_C; p < PARTS_END; p += 0x18)
		{
			if (!(U32(p, P_MASK) & (uint32_t)(int32_t)S16(n, 0x18))) continue;
			// 30 fps layer: see mag156_wild_cannon_blow_held.inc
			FX_HELD(held_note_particle(p);)
			int16_t w[3];
			BurstPoint(i, (const int16_t *)(p + P_POS), w);
			if (!ParticleDraw(h, p, w, 4)) continue;
			S16(p, P_POS) = (int16_t)(S16(p, P_POS) + S16(p, P_VEL));
			S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + S16(p, P_VEL + 2));
			S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + S16(p, P_VEL + 4));
			S16(p, P_VEL) = (int16_t)(S16(p, P_VEL) - (int16_t)(S16(p, P_VEL) >> 2));
			S16(p, P_VEL + 2) = (int16_t)(S16(p, P_VEL + 2) - (int16_t)(S16(p, P_VEL + 2) >> 2));
			S16(p, P_VEL + 4) = (int16_t)(S16(p, P_VEL + 4) - (int16_t)(S16(p, P_VEL + 4) >> 4));
			alive++;
		}
		FieldFree(0xB4);
		return alive;
	}

	static uint32_t __cdecl BurstTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_parts(REC_BURST, n);)
		const int alive = BurstParticles(n);
		if (DrawOnly()) return 0;
		if (S16(n, 0xC) < 2)
		{
			for (int k = 0; k < 0xC; k++)
			{
				const uint32_t p = FreeParticle(PARTS_C, PARTS_END, 0x64);
				if (!p) break;
				U32(p, P_MASK) = (uint32_t)(int32_t)S16(n, 0x18);
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)((CrtRand() & 0x7FF) + 0x400);
				S16(p, P_POS) = (int16_t)(CrtRand() % 0xC8 - 0x64);
				S16(p, P_POS + 2) = (int16_t)(CrtRand() % 0xC8 - 0x64);
				S16(p, P_POS + 4) = (int16_t)(CrtRand() % 0x64);
				const int32_t a = CrtRand() & 0xFFF;
				const int32_t v = CrtRand() % 0x6E + 0x5A;
				S16(p, P_VEL) = (int16_t)(mul32(ComputeCos(a), v) >> 12);
				S16(p, P_VEL + 2) = (int16_t)(mul32(ComputeSin(a), v) >> 12);
				S16(p, P_VEL + 4) = (int16_t)(CrtRand() % 0x28 + 0x14);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	// particles of [lo, hi) with the node's mask: sequence seq (frame base `base`), pull -size >> pull_shift, size + size >> 5
	static int TrailParticles(uint32_t n, uint32_t lo, uint32_t hi, uint32_t seq, uint16_t base, int32_t pull_shift)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int alive = 0;
		U32(h, 0) = seq;
		U16(h, 0x24) = base;
		for (uint32_t p = lo; p < hi; p += 0x18)
		{
			if (!(U32(p, P_MASK) & (uint32_t)(int32_t)S16(n, 0x18))) continue;
			// 30 fps layer: see mag156_wild_cannon_blow_held.inc
			FX_HELD(held_note_particle(p);)
			if (!ParticleDraw(h, p, (const void *)(p + P_POS), pull_shift)) continue;
			S16(p, P_SIZE) = (int16_t)(S16(p, P_SIZE) + (int16_t)(S16(p, P_SIZE) >> 5));
			alive++;
		}
		FieldFree(0xB4);
		return alive;
	}

	// the trail's progress: + step, step - 1 down to 2
	static void TrailStep(uint32_t n)
	{
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		if (d > 2) S16(n, 0x1E) = (int16_t)(d - 1);
	}

	// Trail (0x6063E0) / Trail2 (0x6065A0): one particle A / B a curve point (Trail2: jittered)
	static uint32_t TrailTaskImpl(uint32_t n, bool jitter)
	{
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_parts(jitter ? REC_TRAIL2 : REC_TRAIL, n);)
		const uint32_t lo = jitter ? PARTS_B : PARTS_A, hi = jitter ? PARTS_C : PARTS_B;
		const int alive = TrailParticles(n, lo, hi, jitter ? SEQ_Trail2 : SEQ_Trail, 0, 4);
		if (DrawOnly()) return 0;
		TrailStep(n);
		const int32_t at = S16(n, 0x1C);
		if (at < 0x20)
		{
			for (int32_t k = at; k >= S16(n, 0x1C) - S16(n, 0x1E); k--)
			{
				const uint32_t p = FreeParticle(lo, hi, 0x96);
				if (!p) break;
				U32(p, P_MASK) = (uint32_t)(int32_t)S16(n, 0x18);
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % 0x700 + 0xB00);
				U32(p, P_POS) = U32(Point(S16(n, 0xE), k), 0);
				U32(p, P_POS + 4) = U32(Point(S16(n, 0xE), k), 4);
				if (jitter)
				{
					S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % 0xC8 - 0x64);
					S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + CrtRand() % 0xC8 - 0x64);
					S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % 0xC8 - 0x64);
				}
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	static uint32_t __cdecl TrailTask(TaskNode *nd) { return TrailTaskImpl((uint32_t)nd, false); }
	static uint32_t __cdecl Trail2Task(TaskNode *nd) { return TrailTaskImpl((uint32_t)nd, true); }

	// Emitter (0x606A60): particles C of its mask, one new a tick on counters 0..2 around its point
	static int EmitterParticles(uint32_t n)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int alive = 0;
		U32(h, 0) = SEQ_Emitter;
		U16(h, 0x24) = 0;
		for (uint32_t p = PARTS_C; p < PARTS_END; p += 0x18)
		{
			if (!(U32(p, P_MASK) & (uint32_t)(int32_t)S16(n, 0x18))) continue;
			// 30 fps layer: see mag156_wild_cannon_blow_held.inc
			FX_HELD(held_note_particle(p);)
			if (!ParticleDraw(h, p, (const void *)(p + P_POS), 4)) continue;
			alive++;
		}
		FieldFree(0xB4);
		return alive;
	}

	static uint32_t __cdecl EmitterTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(held_note_parts(REC_EMITTER, n);)
		const int alive = EmitterParticles(n);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 2)
		{
			const uint32_t p = FreeParticle(PARTS_C, PARTS_END, 0x64);
			if (p)
			{
				U32(p, P_MASK) = (uint32_t)(int32_t)S16(n, 0x18);
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % 0x500 + 0x500);
				U32(p, P_POS) = U32(n, 0x10);
				U32(p, P_POS + 4) = U32(n, 0x14);
				S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % 0x258 - 0x12C);
				S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + CrtRand() % 0x258 - 0x12C);
				S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % 0x258 - 0x12C);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Shot (0x6056E0) and Master (0x605620)
	// ------------------------------------------------------------------
	static uint32_t NewTask(uint32_t f, int16_t index)
	{
		const uint32_t t = (uint32_t)AddTaskToQueue(ShotQueue(), f);
		S16(t, 0xC) = 0;
		S16(t, 0xE) = index;
		return t;
	}

	// the shot's start, target, curve and rotation (counter 0): odd shots from point 0xF (x offset 0xC8), even from point 9 (-0xC8)
	static void ShotSetup(int i)
	{
		const uint32_t ent = (uint32_t)Entity(CasterSlot());
		int16_t off[4];
		if (i & 1)
		{
			SpawnPosition((uint8_t *)ent, 0xF, 0x1000, (int16_t *)Start(i));
			off[0] = 0xC8;
		}
		else
		{
			SpawnPosition((uint8_t *)ent, 9, 0x1000, (int16_t *)Start(i));
			off[0] = -0xC8;
		}
		off[1] = -250;
		off[2] = -250;
		off[3] = 0; // never written, never read (0x56C4F0 reads 3 words)
		MatVec((const Mat4x3 *)(ent + 0x40), off, off);
		S16(Start(i), 0) = (int16_t)(S16(Start(i), 0) + off[0]);
		S16(Start(i), 2) = (int16_t)(S16(Start(i), 2) + off[1]);
		S16(Start(i), 4) = (int16_t)(S16(Start(i), 4) + off[2]);
		CentrePosition(Ctx()->target_mask, (int16_t *)Target(i));
		S16(Target(i), 0) = (int16_t)(S16(Target(i), 0) + CrtRand() % 0x640 - 0x320);
		const int32_t rz = CrtRand() % 0x320;
		int32_t fwd[3] = { 0, 0, -0x1000 };
		S16(Target(i), 4) = (int16_t)(S16(Target(i), 4) + rz - 0x190);
		TransformVectorBy3x3Matrix((const Mat4x3 *)(ent + 0x40), fwd, fwd);
		const uint32_t c = Ctrl(i);
		U32(c, 0) = U32(Start(i), 0);
		U32(c, 4) = U32(Start(i), 4);
		U32(c, 8) = U32(Start(i), 0);
		U32(c, 0xC) = U32(Start(i), 4);
		S16(c, 8) = (int16_t)(S16(c, 8) + (mul32(fwd[0], 600) >> 12));
		S16(c, 0xA) = (int16_t)(S16(c, 0xA) - 500);
		S16(c, 0xC) = (int16_t)(S16(c, 0xC) + (mul32(fwd[2], 600) >> 12));
		S16(c, 0x10) = (int16_t)(((int32_t)S16(Start(i), 0) + S16(Target(i), 0)) / 2);
		S16(c, 0x12) = (int16_t)(S16(c, 0xA) - 0x578);
		S16(c, 0x14) = (int16_t)(((int32_t)S16(Target(i), 4) + S16(Start(i), 4)) / 2);
		U32(c, 0x18) = U32(Target(i), 0);
		U32(c, 0x1C) = U32(Target(i), 4);
		void *work = FieldAlloc(0x190);
		SplineSetup(4, (const void *)c, work);
		for (int32_t k = 0, t = 0; t < 0x20000; t += 0x1000, k++)
			SplineEval(4, work, (void *)Point(i, k), t / 31);
		FieldFree(0x190);
		int32_t z[3] = { 0, 0, -0x1000 };
		int32_t d[3] = { (int32_t)S16(c, 8) - S16(c, 0), (int32_t)S16(c, 0xA) - S16(c, 2), (int32_t)S16(c, 0xC) - S16(c, 4) }; // along the first control leg
		NormalizeVector(d, d);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(z, d, axis);
		BuildAxisAngleRotationMatrix(angle, Rot(i), axis);
		Rot(i)->t[0] = S16(c, 0);
		Rot(i)->t[1] = S16(c, 2);
		Rot(i)->t[2] = S16(c, 4);
	}

	static uint32_t __cdecl ShotTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int16_t i = S16(n, 0xE);
		if (S16(n, 0xC) == 0)
		{
			ShotSetup(i);
			uint32_t t = NewTask(ORIG_Flash, i);
			S16(t, 0x18) = (int16_t)(CrtRand() & 0xFFF);
			S16(t, 0x1E) = S16(t, 0x1C) = (int16_t)((CrtRand() % 0x300 + 0x800) / 4);
			t = NewTask(ORIG_Burst, i);
			S16(t, 0x18) = (int16_t)(1 << i);
			t = NewTask(ORIG_Ring, i);
			S16(t, 0x18) = (int16_t)(CrtRand() & 0xFFF);
			S16(t, 0x1C) = (int16_t)(CrtRand() % 0x600 + 0x1900);
			t = NewTask(ORIG_Ball, i);
			S16(t, 0x18) = 0;
			S16(t, 0x1A) = 5;
			S16(t, 0x1C) = (int16_t)((CrtRand() & 0x1FF) + 0xC00);
			t = NewTask(ORIG_Trail, i);
			S16(t, 0x1C) = 0;
			S16(t, 0x1E) = 5;
			S16(t, 0x18) = (int16_t)(1 << i);
			t = NewTask(ORIG_Trail2, i);
			S16(t, 0x1C) = 0;
			S16(t, 0x1E) = 5;
			S16(t, 0x18) = (int16_t)(0x100 << i);
		}
		if (S16(n, 0xC) == 0xD)
		{
			uint32_t t = NewTask(ORIG_Impact, i);
			U32(t, 0x10) = U32(Target(i), 0);
			U32(t, 0x14) = U32(Target(i), 4);
			S16(t, 0x18) = (int16_t)(CrtRand() & 0xFFF);
			S16(t, 0x1A) = (int16_t)(CrtRand() % 0x8C + 0x50);
			S16(t, 0x1E) = S16(t, 0x1C) = (int16_t)((CrtRand() % 0x500 + 0xE00) / 4);
			t = NewTask(ORIG_ImpactRing, i);
			U32(t, 0x10) = U32(Target(i), 0);
			U32(t, 0x14) = U32(Target(i), 4);
			S16(t, 0x18) = (int16_t)(CrtRand() & 0xFFF);
			S16(t, 0x1E) = S16(t, 0x1C) = (int16_t)(((CrtRand() & 0x3FF) + 0x1E00) / 4);
		}
		if (S16(n, 0xC) == 0xF)
		{
			for (int j = 0; j < Ctx()->actions->target_count; j++)
			{
				const uint8_t slot = Ctx()->actions->targets[j * TARGET_STRIDE];
				const uint32_t e = (uint32_t)AddTaskToQueue(ShotQueue(), ORIG_Emitter);
				S16(e, 0xC) = 0;
				S16(e, 0x18) = (int16_t)(0x100 << (U8(n, 0xE) & 31));
				SpawnPosition(Entity(slot), 0xF1, 0, (int16_t *)(e + 0x10));
			}
		}
		const int16_t c = S16(n, 0xC);
		if (c >= 0xD && c <= 0x10) var<int16_t>(0x1D97712) = (int16_t)(var<int16_t>(0x1D97712) + ((c & 1) ? 0x40 : -0x40));
		S16(n, 0xC) = (int16_t)(c + 1);
		return S16(n, 0xC) >= 0x14 ? TASK_END : 0;
	}

	static uint32_t __cdecl MasterTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int16_t c = S16(n, 0xC);
		if (c == 1 || c == 4 || c == 7 || c == 0xA)
		{
			const uint32_t s = (uint32_t)AddTaskToQueue(ShotQueue(), ORIG_Shot);
			S16(s, 0xE) = S16(n, 0xE);
			S16(s, 0xC) = 0;
			S16(n, 0xE) = (int16_t)(S16(n, 0xE) + 1);
		}
		if (S16(n, 0xC) == 0x24) ApplyActionResultToTargets(Ctx()->actions->targets, Ctx()->actions->target_count);
		if (S16(n, 0xC) == 1)
		{
			BdPlaySE((const void *)SOUND_A, 0, 0x80);
			BdPlaySE((const void *)SOUND_B, 0, 0x80);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x2D ? TASK_END : 0;
	}

	void register_mag156_wild_cannon_blow_ports()
	{
		register_port(ORIG_Root, (void *)RootTask, "156 RootTask", 156);
		register_port(ORIG_TexRestore, (void *)TexRestoreTask, "156 TexRestoreTask", 156);
		register_port(ORIG_Master, (void *)MasterTask, "156 MasterTask", 156);
		register_port(ORIG_Shot, (void *)ShotTask, "156 ShotTask", 156);
		register_port(ORIG_Flash, (void *)FlashTask, "156 FlashTask", 156);
		register_port(ORIG_Burst, (void *)BurstTask, "156 BurstTask", 156);
		register_port(ORIG_Ring, (void *)RingTask, "156 RingTask", 156);
		register_port(ORIG_Ball, (void *)BallTask, "156 BallTask", 156);
		register_port(ORIG_Trail, (void *)TrailTask, "156 TrailTask", 156);
		register_port(ORIG_Trail2, (void *)Trail2Task, "156 Trail2Task", 156);
		register_port(ORIG_Impact, (void *)ImpactTask, "156 ImpactTask", 156);
		register_port(ORIG_ImpactRing, (void *)ImpactRingTask, "156 ImpactRingTask", 156);
		register_port(ORIG_Emitter, (void *)EmitterTask, "156 EmitterTask", 156);
		// 30 fps layer: see mag156_wild_cannon_blow_held.inc
		FX_HELD(register_mag156_held();)
	}
}

	void register_mag156_wild_cannon_blow() { wildcannon156::register_mag156_wild_cannon_blow_ports(); }
}

#ifdef FF8_FX_HELD
#include "mag156_wild_cannon_blow_held.inc"
#endif
