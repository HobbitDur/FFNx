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

// Effect 321: Ker Plunk (enemy attack 231 of kernel.bin; Jumbo Cactuar c0m084; MAG_321_*): the targets lifted and spun (texture
// file "mag320.tim").
//
// Structure (file loader 0x5979E0 = the texture file 0xCD2AFC -> 0x221D524; _Init 0x597A00 keeps the context 0x221D51C, the
// caster's slot 0x221D520 and the first target's slot 0x221BC68, queues the root (pool 0x221BC70, queue 0x221BC90) and the
// director in the effect queue 0x221C600 (100 x 0x24, pool 0x221C610), frees the 100 particles (0x221BCA0, 0x18 bytes, word +0
// = owner mask: 1 dust, 2 debris, 4 << k the k-th target's burst), plays the camera animation 0xCD1284 and queues the TIM;
// both run original):
//   Root (0x598D00) - packet cursor 0x2239528 = 0x221D528 / 0x222B528 on its counter's parity, the effect queue; once it is
//     empty, TexRestoreTask (0x598D60, the texture restore 0x508630 of 0x221D528 on its counter 1) in the root queue; ends.
//   Director (0x597AB0) - nothing on draw-only ticks. Tick 0: the point 0xAF0 in front of the caster (its default effect
//     position moved by -2800 x sin / cos of its y angle), the looping sound 0xCD29DC (voice slot 0x223952C). Tick 1: the
//     sound 0xCD2AF0. Tick 0x18: 3 rings (0x597E30, delays 0 / 3 / 6, size 0x5CC growing by 0x5CC - 1/8 a tick), the dust
//     (0x597F90), the debris (0x598230), the white flash (0x598570) and for every target k: its lift (0x5985C0: spin
//     (1..3) << 12 x tick / 12 about x, tick x 0x2000 / 12 about y, height -0x1770 - 0..0x9C3) and a ring (0x598B90) at it.
//     Tick 0x5C: the sound 0xCD2AF4 at the targets' centre. Tick 0x61: the loop released. Ends after tick 0x64.
//   Rings (0x597E30) - prim model 0xCD153C at the point, uniform scale +0x1C (+ +0x1E, step - step / 8), fade (tick - 0xC)
//     << 9 from tick 0xC; 0x14 ticks after the delay.
//   Target ring (0x598B90) - prim model 0xCD1D5C at the target, scale (+0x1C, +0x20, +0x1C), +0x20 + +0x22 (step - step /
//     10), fade (tick - 6) << 9 from tick 6; 0xE ticks after a delay of 1.
//   Dust (0x597F90) - ticks 0..0xA: 6 new ones (mask 1) around the point (+-0x898 / -0..0x12B, size 0x900..0x12FF, rising
//     -5 - 0..0x2C); sequence 0xCD1390 at each one's point (light matrix + back colour = the camera; rotation scale = size,
//     pulled size >> 3 towards the eye); frame + 1, y + rise; ends once none is left.
//   Debris (0x598230) - ticks 0..5: 5 new ones (mask 2) around the point (+-0x76C / -0..0xF9, size 0x300..0x7FF, rise -0x6E
//     - 0..0xEF (- 1/8 a tick), spins +-0x1E..0x63 / +-0xF..0x36 about x / z); prim model 0xCD1C24 at each one (angles
//     +0x10 / +0x14, scale size shrinking by size / 8 a frame from frame 8); 0x10 frames.
//   Flash (0x598570) - every tick (draw-only too): white screen feedback 0x47CF50(0, 0xFF, 0xFF, 0xFF, 0x3F); 0x14 ticks.
//   Lift (0x5985C0) - EVERY tick (draw-only too): tick 0 keeps the target's flags & 0x1020, position and angles (0x221D420
//     + slot x 0x24); ticks 0..0x41: flags | 0x1020, angles as above, the position turned about the lift point by them,
//     height + sin ramp up (ticks 0..0xB) / hold / down (0x36..0x41) x the height; from tick 0x42 position / angles put back.
//     Not on draw-only ticks: tick 0x42 the burst (0x5988B0, mask 4 << k); after tick 0x43 the flags put back and the action
//     result of target k; ends.
//   Burst (0x5988B0) - ticks 0..3: 8 new ones at the target (size 0x600..0x9FF, a random direction x 0x46..0x9F in xz, y
//     -0..0x77 unused); sequence 0xCD1390 as the dust; x / z + velocity, velocity - velocity / 8; ends once none is left.
// Every drawing task draws first and updates afterwards, except the dust / debris / burst that spawn first; on draw-only ticks
// a task returns after drawing.
// ENTITY WRITES: the targets' flags (0x1020), angles +0x0C / +0x0E, position +0x1C..+0x22 (put back). CAMERA: the camera
// animation 0xCD1284 (engine). Module globals: 0x221BC68..0x2239530.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace kerplunk321
{
	using namespace eng;
	using namespace magc;

	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }

	// --- module globals ---
	inline TaskQueue *RootQueue() { return (TaskQueue *)0x221BC90; }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x221C600; }
	inline uint32_t &Ctx() { return var<uint32_t>(0x221D51C); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x221D520); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2239528); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x223952C); }
	inline uint32_t Saved(int slot) { return 0x221D420 + (uint32_t)slot * 0x24; }   // flags & 0x1020, angles, position
	static const uint32_t PARTS = 0x221BCA0, PARTS_END = 0x221C600;   // 100 x 0x18 bytes
	static const uint32_t ARENA_A = 0x221D528, ARENA_B = 0x222B528;

	static const uint32_t ORIG_Root = 0x598D00, ORIG_TexRestore = 0x598D60, ORIG_Director = 0x597AB0, ORIG_Ring = 0x597E30,
		ORIG_Dust = 0x597F90, ORIG_Debris = 0x598230, ORIG_Flash = 0x598570, ORIG_Lift = 0x5985C0, ORIG_Burst = 0x5988B0,
		ORIG_TargetRing = 0x598B90;
	static const uint32_t MODEL_Ring = 0xCD153C, MODEL_TargetRing = 0xCD1D5C, MODEL_Debris = 0xCD1C24, SEQ_Dust = 0xCD1390;
	static const uint32_t SOUND_Loop = 0xCD29DC, SOUND_Start = 0xCD2AF0, SOUND_End = 0xCD2AF4;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	inline void ScaleMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); } // scale3DMatrix
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void CentrePosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); } // CalculateCenterPosition
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void ScreenFeedback(int32_t a, int32_t r, int32_t g, int32_t b, int32_t n) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t)>(0x47CF50)(a, r, g, b, n); } // Battle_RequestScreenFeedback
	inline int32_t VoiceActive(uint32_t v) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(v); }  // BdSound_IsVoiceSlotActive
	inline void VoiceRelease(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(v); }             // BdSound_ReleaseVoiceSlot
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // MAC = L * V0 + BK
	inline void GteSetBKFromMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = matrix translation
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }               // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v

	// a particle (0x18 bytes): +0 owner mask, +4 frame, +6 size, +8 point (4 words), +0x0E spin x (debris), +0x10 / +0x12 /
	// +0x14 velocity (dust: rise +0x12; burst: x / z) or angles x / z (debris, rise +0x12, spin z +0x16)
	enum { P_MASK = 0, P_FRAME = 4, P_SIZE = 6, P_POS = 8 };
}
}

#ifdef FF8_FX_HELD
#include "mag321_ker_plunk_held.h"
#endif

namespace ff8fx
{
namespace kerplunk321
{
	static uint32_t NewTask(uint32_t f)
	{
		const uint32_t t = P(AddTaskToQueue(EffectQueue(), f));
		S16(t, 0xC) = 0;
		return t;
	}

	static uint32_t FreeParticle()
	{
		for (uint32_t p = PARTS; p < PARTS_END; p += 0x18)
			if (!U32(p, P_MASK)) return p;
		return 0;
	}

	// ------------------------------------------------------------------
	// Rings (0x597E30 / 0x598B90)
	// ------------------------------------------------------------------
	static void RingDraw(uint32_t n, uint32_t model, const int32_t *sc, int32_t from, int32_t fade)
	{
		const int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = S16(n, 0x12);
		m.t[0] = S16(n, 0x10);
		m.t[2] = S16(n, 0x14);
		ScaleMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = model;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x33;
		if (S16(n, 0xC) >= from)
		{
			U32(h, 0x1C) = 0xF3;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static int32_t RingFade(int32_t c, int32_t from) { return shl32(c - from, 9); }

	static void PointRingDraw(uint32_t n, int32_t fade)
	{
		const int32_t s = S16(n, 0x1C);
		const int32_t sc[3] = { s, s, s };
		RingDraw(n, MODEL_Ring, sc, 0xC, fade);
	}

	static void TargetRingDraw(uint32_t n, int32_t fade)
	{
		const int32_t sc[3] = { S16(n, 0x1C), S16(n, 0x20), S16(n, 0x1C) };
		RingDraw(n, MODEL_TargetRing, sc, 6, fade);
	}

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		if (S16(n, 0xE) > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(S16(n, 0xE) - 1);
			return 0;
		}
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_node(REC_RING, n);)
		PointRingDraw(n, RingFade(S16(n, 0xC), 0xC));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 8);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x14 ? TASK_END : 0;
	}

	static uint32_t __cdecl TargetRingTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		if (S16(n, 0xE) > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(S16(n, 0xE) - 1);
			return 0;
		}
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_node(REC_TRING, n);)
		TargetRingDraw(n, RingFade(S16(n, 0xC), 6));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x22);
		S16(n, 0x20) = (int16_t)(S16(n, 0x20) + d);
		S16(n, 0x22) = (int16_t)(d - d / 10);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0xE ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dust (0x597F90) / burst (0x5988B0): sequence 0xCD1390 at each point, lit by the camera
	// ------------------------------------------------------------------
	// h: sequence header (0xB4), w: work (0x48): +8 camera copy, +0x28 direction, +0x38 point
	static void SeqSetup(uint32_t h, uint32_t w)
	{
		U16(h, 0x24) = 0;
		memcpy((void *)(w + 8), (const void *)&Camera(), 0x20);
		U32(h, 0) = SEQ_Dust;
		GteSetLightMatrix((const void *)(w + 8));
		GteSetBKFromMatrix((const void *)(w + 8));
	}

	static void SeqDraw(uint32_t h, uint32_t w, uint32_t p)
	{
		GteLoadV0((const void *)(p + P_POS));
		GteMVMVA_LightV0Bk();
		GteSetRotScale(S16(p, P_SIZE));
		U16(h, 4) = U16(p, P_FRAME);
		GteReadMAC123((int32_t *)(w + 0x38));
		NormalizeVector((const int32_t *)(w + 0x38), (int32_t *)(w + 0x28));
		const int32_t k = -(S16(p, P_SIZE) >> 3);
		S32(w, 0x38) += mul32(S32(w, 0x28), k) >> 12;
		S32(w, 0x3C) += mul32(S32(w, 0x2C), k) >> 12;
		S32(w, 0x40) += mul32(S32(w, 0x30), k) >> 12;
		GteSetTransFromVec32((const int32_t *)(w + 0x38));
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		const uint32_t w = P(FieldAlloc(0x48));
		if (!DrawOnly() && S16(n, 0xC) <= 0xA)
		{
			U32(w, 0) = U32(n, 0x10);
			U32(w, 4) = U32(n, 0x14);
			for (int k = 0; k < 6; k++)
			{
				const uint32_t p = FreeParticle();
				if (!p) break;
				U32(p, P_MASK) = 1;
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % 0xA00 + 0x900);
				U32(p, P_POS) = U32(w, 0);
				U32(p, P_POS + 4) = U32(w, 4);
				S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % 0x1130 - 0x898);
				S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) - CrtRand() % 0x12C);
				S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % 0x1130 - 0x898);
				S16(p, 0x12) = (int16_t)(-5 - CrtRand() % 0x2D);
			}
		}
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_parts(REC_DUST, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int alive = 0;
		SeqSetup(h, w);
		for (uint32_t p = PARTS; p < PARTS_END; p += 0x18)
		{
			if (!(U8(p, P_MASK) & 1)) continue;
			// 30 fps layer: see mag321_ker_plunk_held.inc
			FX_HELD(held_note_particle(p);)
			SeqDraw(h, w, p);
			if (DrawOnly()) continue;
			if (S16(h, 0x28) < 0)
			{
				U32(p, P_MASK) = 0;
				continue;
			}
			S16(p, P_FRAME) = (int16_t)(S16(p, P_FRAME) + 1);
			S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + S16(p, 0x12));
			S16(p, 0x10) = (int16_t)(S16(p, 0x10) - (int16_t)(S16(p, 0x10) >> 4));
			alive++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl BurstTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		const uint32_t w = P(FieldAlloc(0x48));
		if (!DrawOnly() && S16(n, 0xC) <= 3)
		{
			U32(w, 0) = U32(n, 0x10);
			U32(w, 4) = U32(n, 0x14);
			for (int k = 0; k < 8; k++)
			{
				const uint32_t p = FreeParticle();
				if (!p) break;
				U32(p, P_MASK) = (uint32_t)(int32_t)S16(n, 0xE);
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % 0x400 + 0x600);
				U32(p, P_POS) = U32(w, 0);
				U32(p, P_POS + 4) = U32(w, 4);
				const int32_t a = CrtRand() % 0x1000;
				const int32_t sp = CrtRand() % 0x5A + 0x46;
				S16(p, 0x10) = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
				S16(p, 0x12) = (int16_t)-(CrtRand() % 0x78);
				S16(p, 0x14) = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
			}
		}
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_parts(REC_BURST, n);)
		const uint32_t h = P(FieldAlloc(0xB4));
		int alive = 0;
		SeqSetup(h, w);
		const uint32_t mask = (uint32_t)(int32_t)S16(n, 0xE);
		for (uint32_t p = PARTS; p < PARTS_END; p += 0x18)
		{
			if (!(mask & U32(p, P_MASK))) continue;
			// 30 fps layer: see mag321_ker_plunk_held.inc
			FX_HELD(held_note_particle(p);)
			SeqDraw(h, w, p);
			if (DrawOnly()) continue;
			if (S16(h, 0x28) < 0)
			{
				U32(p, P_MASK) = 0;
				continue;
			}
			S16(p, P_FRAME) = (int16_t)(S16(p, P_FRAME) + 1);
			S16(p, P_POS) = (int16_t)(S16(p, P_POS) + S16(p, 0x10));
			S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + S16(p, 0x14));
			S16(p, 0x10) = (int16_t)(S16(p, 0x10) - (int16_t)(S16(p, 0x10) >> 3));
			S16(p, 0x14) = (int16_t)(S16(p, 0x14) - (int16_t)(S16(p, 0x14) >> 3));
			alive++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Debris (0x598230)
	// ------------------------------------------------------------------
	// size of a debris at frame f: shrinking by size / 8 a frame from frame 8
	static int32_t DebrisScale(int32_t s, int32_t f) { return f >= 8 ? s + mul32(f - 8, -(s / 8)) : s; }

	// h: prim header (0x58), w: work (0x68): +0 angles, +8 camera copy, +0x28 matrix, +0x48 scale
	static void DebrisDraw(uint32_t h, uint32_t w, uint32_t p, int32_t s)
	{
		U16(w, 0) = U16(p, 0x10);
		U16(w, 4) = U16(p, 0x14);
		ComposeZYXRotationMatrix((const int16_t *)w, (Mat4x3 *)(w + 0x28));
		S32(w, 0x44) = S16(p, P_POS + 4);
		S32(w, 0x3C) = S16(p, P_POS);
		S32(w, 0x40) = S16(p, P_POS + 2);
		S32(w, 0x50) = s;
		S32(w, 0x4C) = s;
		S32(w, 0x48) = s;
		ScaleMatrix((Mat4x3 *)(w + 0x28), (const int32_t *)(w + 0x48));
		ComposeAffineTransform((const Mat4x3 *)(w + 8), (const Mat4x3 *)(w + 0x28), (Mat4x3 *)(w + 0x28));
		GteSetRotMatrix((const Mat4x3 *)(w + 0x28));
		GteSetTransVector((const Mat4x3 *)(w + 0x28));
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static void DebrisSetup(uint32_t h, uint32_t w)
	{
		memcpy((void *)(w + 8), (const void *)&Camera(), 0x20);
		U32(h, 0) = MODEL_Debris;
		U32(h, 0x1C) = 0x30;
		U16(w, 2) = 0;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		const uint32_t w = P(FieldAlloc(0x68));
		if (!DrawOnly() && S16(n, 0xC) <= 5)
		{
			U32(w, 0) = U32(n, 0x10);
			U32(w, 4) = U32(n, 0x14);
			for (int k = 0; k < 5; k++)
			{
				const uint32_t p = FreeParticle();
				if (!p) break;
				U32(p, P_MASK) = 2;
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % 0x500 + 0x300);
				U32(p, P_POS) = U32(w, 0);
				U32(p, P_POS + 4) = U32(w, 4);
				S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % 0xED8 - 0x76C);
				S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) - CrtRand() % 0xFA);
				S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % 0xED8 - 0x76C);
				S16(p, 0x10) = (int16_t)(CrtRand() % 0x1000);
				S16(p, 0x12) = (int16_t)(-0x6E - CrtRand() % 0xF0);
				S16(p, 0x14) = (int16_t)(S16(p, 0x10) >> 1);
				int16_t v = (int16_t)(CrtRand() % 0x46 + 0x1E);
				S16(p, 0xE) = v;
				if (U8(p, 0x10) & 1) S16(p, 0xE) = (int16_t)-v;
				v = (int16_t)(CrtRand() % 0x28 + 0xF);
				S16(p, 0x16) = v;
				if (U8(p, 0x14) & 1) S16(p, 0x16) = (int16_t)-v;
			}
		}
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_parts(REC_DEBRIS, n);)
		const uint32_t h = P(FieldAlloc(0x58));
		int alive = 0;
		DebrisSetup(h, w);
		for (uint32_t p = PARTS; p < PARTS_END; p += 0x18)
		{
			if (!(U8(p, P_MASK) & 2)) continue;
			// 30 fps layer: see mag321_ker_plunk_held.inc
			FX_HELD(held_note_particle(p);)
			const int16_t f = S16(p, P_FRAME);
			DebrisDraw(h, w, p, DebrisScale(S16(p, P_SIZE), f));
			if (DrawOnly()) continue;
			if (f >= 0x10)
			{
				U32(p, P_MASK) = 0;
				continue;
			}
			S16(p, P_FRAME) = (int16_t)(f + 1);
			S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + S16(p, 0x12));
			S16(p, 0x12) = (int16_t)(S16(p, 0x12) - (int16_t)(S16(p, 0x12) >> 3));
			S16(p, 0x10) = (int16_t)(S16(p, 0x10) + S16(p, 0xE));
			S16(p, 0x14) = (int16_t)(S16(p, 0x14) + S16(p, 0x16));
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Flash (0x598570) and lift (0x5985C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		ScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (DrawOnly()) return 0;
		const uint32_t n = P(tn);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x14 ? TASK_END : 0;
	}

	static uint32_t __cdecl LiftTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		const int16_t c = S16(n, 0xC);
		const int slot = S16(n, 0xE);
		const uint32_t e = P(Entity(slot)), s = Saved(slot);
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_node(REC_LIFT, n);)
		if (c == 0)
		{
			U16(s, 0) = (uint16_t)(U16(e, 0) & 0x1020);
			U32(s, 0xC) = U32(e, 0x1C);
			U32(s, 0x10) = U32(e, 0x20);
			U32(s, 4) = U32(e, 0xC);
			U32(s, 8) = U32(e, 0x10);
		}
		if (c < 0x42)
		{
			U16(e, 0) |= 0x1020;
			S16(e, 0xC) = (int16_t)(mul32(S16(n, 0x18), c) / 12);
			S16(e, 0xE) = (int16_t)(shl32(c, 13) / 12);
			int16_t d[3] = { (int16_t)(S16(n, 0x10) - S16(s, 0xC)), (int16_t)(S16(n, 0x12) - S16(s, 0xE)), (int16_t)(S16(n, 0x14) - S16(s, 0x10)) };
			Mat4x3 m;
			ComposeZYXRotationMatrix((const int16_t *)(e + 0xC), &m);
			MatVec(&m, d, d);
			S16(e, 0x1C) = (int16_t)(S16(n, 0x10) - d[0]);
			S16(e, 0x1E) = (int16_t)(S16(n, 0x12) - d[1]);
			S16(e, 0x20) = (int16_t)(S16(n, 0x14) - d[2]);
		}
		if (c < 0xC) S16(e, 0x1E) = (int16_t)(S16(e, 0x1E) + (mul32(ComputeSin(shl32(c, 10) / 12), S16(n, 0x20)) >> 12));
		else if (c < 0x36) S16(e, 0x1E) = (int16_t)(S16(e, 0x1E) + S16(n, 0x20));
		else if (c < 0x42) S16(e, 0x1E) = (int16_t)(S16(e, 0x1E) + (mul32(ComputeSin(shl32(c - 0x36, 10) / 12 + 0x400), S16(n, 0x20)) >> 12));
		else
		{
			U32(e, 0x1C) = U32(s, 0xC);
			U32(e, 0x20) = U32(s, 0x10);
			U32(e, 0xC) = U32(s, 4);
			U32(e, 0x10) = U32(s, 8);
		}
		if (DrawOnly()) return 0;
		if (S16(n, 0xC) == 0x42)
		{
			const uint32_t t = NewTask(ORIG_Burst);
			S16(t, 0x12) = 0;
			S16(t, 0x10) = S16(n, 0x10);
			S16(t, 0x14) = S16(n, 0x14);
			S16(t, 0xE) = (int16_t)(4 << (U8(n, 0x1A) & 31));
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 0x44) return 0;
		U16(e, 0) = (uint16_t)((U16(e, 0) & 0xEFDF) | U16(s, 0));
		ApplyActionResultToTarget((uint8_t *)(U32(U32(Ctx(), 4), 8) + (uint32_t)(S16(n, 0x1A) * 0x18)));
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Director (0x597AB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = P(tn);
		uint8_t *caster = Entity(CasterSlot());
		if (S16(n, 0xC) == 0)
		{
			GetDefaultEffectPosition(caster, (int16_t *)(n + 0x10));
			const int32_t sn = ComputeSin(*(const int16_t *)(caster + 0xE));
			S16(n, 0x12) = 0;
			S16(n, 0x10) = (int16_t)(S16(n, 0x10) + (mul32(sn, -2800) >> 12));
			const int32_t cs = ComputeCos(*(const int16_t *)(caster + 0xE));
			S16(n, 0x14) = (int16_t)(S16(n, 0x14) + (mul32(cs, -2800) >> 12));
			VoiceSlot() = ClaimVoiceSlot((const void *)SOUND_Loop, 1, 0x80);
		}
		if (S16(n, 0xC) == 0x18)
		{
			for (int i = 0; i < 3; i++)
			{
				const uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_Ring));
				S16(t, 0xE) = (int16_t)(3 * i);
				U32(t, 0x10) = U32(n, 0x10);
				S16(t, 0xC) = 0;
				U32(t, 0x14) = U32(n, 0x14);
				S16(t, 0x1E) = S16(t, 0x1C) = 0x5CC;
			}
			uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_Dust));
			U32(t, 0x10) = U32(n, 0x10);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(n, 0x14);
			t = P(AddTaskToQueue(EffectQueue(), ORIG_Debris));
			U32(t, 0x10) = U32(n, 0x10);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(n, 0x14);
			NewTask(ORIG_Flash);
			const uint32_t tg = U32(Ctx(), 4);
			for (int k = 0; k < U8(tg, 0x10); k++)
			{
				const int slot = *(const uint8_t *)(U32(tg, 8) + (uint32_t)k * 0x18);
				const uint32_t l = NewTask(ORIG_Lift);
				S16(l, 0xE) = (int16_t)slot;
				GetDefaultEffectPosition(Entity(slot), (int16_t *)(l + 0x10));
				int32_t v = shl32(CrtRand() % 3 + 1, 12);
				S16(l, 0x18) = (int16_t)v;
				if (v & 1) S16(l, 0x18) = (int16_t)-v;
				S16(l, 0x1A) = (int16_t)k;
				S16(l, 0x20) = (int16_t)(-0x1770 - CrtRand() % 0x9C4);
				const uint32_t r = NewTask(ORIG_TargetRing);
				S16(r, 0xE) = 1;
				S16(r, 0x10) = S16(l, 0x10);
				S16(r, 0x12) = 0;
				S16(r, 0x14) = S16(l, 0x14);
				S16(r, 0x18) = (int16_t)(CrtRand() % 0x1000);
				S16(r, 0x1C) = (int16_t)(CrtRand() % 0x280 + 0x900);
				S16(r, 0x20) = S16(r, 0x22) = (int16_t)((CrtRand() % 0x800 + 0x1800) / 8);
			}
		}
		if (S16(n, 0xC) == 1) BdPlaySE((const void *)SOUND_Start, 0, 0x80);
		if (S16(n, 0xC) == 0x5C)
		{
			int16_t at[4];
			CentrePosition(U16(Ctx(), 2), at);
			BdPlaySE3D((const void *)SOUND_End, 0, at);
		}
		if (S16(n, 0xC) == 0x61 && VoiceActive(VoiceSlot())) VoiceRelease(VoiceSlot());
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x64 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x598D00) and texture restore (0x598D60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? ARENA_B : ARENA_A;
		const int running = ExecuteTaskQueue(EffectQueue());
		if (!running)
		{
			const uint32_t w = P(AddTaskToQueue(RootQueue(), ORIG_TexRestore));
			S16(w, 0xC) = 0;
			S16(w, 0xE) = 0;
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		if (S16(n, 0xC) == 1) TextureRestoreTask((void *)ARENA_A, (void *)(n + 0xE));
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}

	void register_mag321_ker_plunk_ports()
	{
		register_port(ORIG_Root, (void *)RootTask, "321 RootTask", 321);
		register_port(ORIG_TexRestore, (void *)TexRestoreTask, "321 TexRestoreTask", 321);
		register_port(ORIG_Director, (void *)DirectorTask, "321 DirectorTask", 321);
		register_port(ORIG_Ring, (void *)RingTask, "321 RingTask", 321);
		register_port(ORIG_Dust, (void *)DustTask, "321 DustTask", 321);
		register_port(ORIG_Debris, (void *)DebrisTask, "321 DebrisTask", 321);
		register_port(ORIG_Flash, (void *)FlashTask, "321 FlashTask", 321);
		register_port(ORIG_Lift, (void *)LiftTask, "321 LiftTask", 321);
		register_port(ORIG_Burst, (void *)BurstTask, "321 BurstTask", 321);
		register_port(ORIG_TargetRing, (void *)TargetRingTask, "321 TargetRingTask", 321);
		// 30 fps layer: see mag321_ker_plunk_held.inc
		FX_HELD(register_mag321_held();)
	}
}

	void register_mag321_ker_plunk() { kerplunk321::register_mag321_ker_plunk_ports(); }
	FX_LAB_MODULE(register_mag321_ker_plunk);
}

#ifdef FF8_FX_HELD
#include "mag321_ker_plunk_held.inc"
#endif
