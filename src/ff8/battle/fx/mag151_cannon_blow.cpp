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

// Effect 151: Cannon Blow (enemy attack 69 of kernel.bin; SAM08G c0m007; MAG_151_CANNON_BLOW*).
//
// Structure (file loader 0x60C9A0 = the texture file 0xDC1DC8 -> 0x244F930; _Init 0x60C9C0 keeps the texture base 0x244FD58,
// the context 0x244F928, the first target's slot 0x244D670 and the caster's slot 0x244F92C (context byte 0), sets up the root
// queue 0x244D698 (pool 0x244D678, 2 x 0x10) with RootTask, the shot queue 0x244E9E8 (pool 0x244E9F8, 100 x 0x24) with MainTask
// (+0x0E = shot index 0), frees both particle arrays (A 0x244D6A8, B 0x244E008: 100 x 0x18 each, word +0 = owner mask), plays the
// camera animation 0xDBEC8C and queues the TIM upload; both run original):
//   RootTask (0x60E270) - packet cursor 0x244FD5C = the texture base (+ 0xC000 on odd counters), the shot queue; once it is
//     empty, TexRestoreTask (0x60E2E0, the texture restore 0x508630 on its counter 1) in the root queue; ends.
//   MainTask (0x60CA90) - counter 0: the shot's start (the caster's effect point 0xF with the offset (0xC8, -250, -250) through
//     his world matrix: 0x244F938[i]), the target (the targets' centre 0x5020A0 with x / z +-400 at random: 0x244F888[i]), the
//     4 control points of a curve start -> start + 600 * his forward (y - 500) -> the middle (y 0x578 higher) -> target
//     (0x244F808[i]), its 32 points (Effect_SplineSetup / SplineEval: 0x244F958[i]), the rotation from (0, 0, -0x1000) onto
//     the first control leg (start -> the second point; 0x244F8A8[i], translation = the start). Counter 1: FlashTask, BurstTask, RingTask, BallTask, TrailTask,
//     Trail2Task, Trail3Task, the sound 0xDC1DC0. Counter 14: ImpactTask and ImpactRingTask at the target, 5 DebrisTasks around
//     every target's effect point 0xF1. Counters 14..17: the battle camera shake word 0x1D97712 +-0x40 (odd +, even -). Counter
//     0x1E: the action result. Ends after counter 0x28. Nothing on draw-only ticks.
//   FlashTask (0x60D260) - prim model 0xDC0E10 at the shot's rotation, z angle +0x18, uniform scale +0x1C; faded (counter - 4)
//     << 9 from counter 4; scale + step, step - step / 10; 12 ticks.
//   BurstTask (0x60D3B0) - particles B of its mask: sequence 0xDBF314 at the shot's rotation of their point + the start, through
//     the shadow-rotation camera (R = size, pull -size >> 4); frame + 1, a particle whose sequence ended is freed, point +
//     velocity, velocity x / y - v >> 2, z - v >> 4; counters 0..1: 12 new particles (size 0x400..0xBFF, point +-100 / +-100 /
//     0..99, a random direction in the xy plane at 90..199, z velocity 20..59); ends from counter 4 once none is left.
//   RingTask (0x60D670) - prim model 0xDC0900 on the ground under the start (y 0), y angle +0x18, scale +0x1C, faded counter
//     << 10; 4 ticks.
//   BallTask (0x60D790) - sequence 0xDBF2A0 (frame counter & 3) at the curve point +0x18 (R = +0x1C, pull -R >> 3); point +
//     step, step - 1 down to 2; ends once past the 32nd point.
//   TrailTask (0x60D850) / Trail2Task (0x60DA10) - particles A of its mask (sequence 0xDBEF48 / 0xDBF0F4 + frame 8 base; R =
//     size, pull -size >> 4; frame + 1, freed when ended, size + size >> 5); then the progress + step, step - 1 down to 2, while
//     before the 32nd point: a particle at every point from the progress back to progress - step (size 0xB00..0x11FF; Trail2:
//     +-100 on each axis); no free particle stops the spawning; ends from counter 4 once none is left.
//   Trail3Task (0x60DC20) - particles B of its mask as the trails (sequence 0xDBF314, pull -size >> 3), 2 particles a point
//     (size 0x400..0xBFF, +-150 on each axis); no free particle skips to the next point.
//   ImpactTask (0x60DE40) - prim model 0xDBF4C0 at the target, y angle +0x18, scale +0x1C, faded (counter - 8) << 9 from 8; then
//     the scrolling model 0xDC00F0 (0x64E080) with the same rotation, scale +0x1C - 0x100, the camera's rotation, the first
//     model's translation, v scroll 14 * counter in a 0x40 x 0x80 window, faded as the first; angle + spin, spin - spin >> 3,
//     scale + step, step - step / 5; ends after counter 16.
//   ImpactRingTask (0x60E060) - prim model 0xDC1498 at the target, y angle +0x18, scale +0x1C, faded (counter - 3) * 0x199
//     from 3; scale + step, step - step / 7; ends after counter 13.
//   DebrisTask (0x60E1B0) - waits +0x0E ticks, then sequence 0xDBED9C (frame counter) at its point (R = +0x1C, pull -R >> 4)
//     until the sequence ends.
//   0x64E080 - the scrolling prim renderer (Hyper Shot 198's 0x5AAD60 with only its 4 textured lists; the section renderers
//     0x64E1D0 / 0x64E500 / 0x64E900 / 0x64EC60 = 198's 0x5AB3A0 / 0x5AB6D0 / 0x5ABFC0 / 0x5AC320 instruction for instruction).
// DRAW ORDER: every task draws in the tick it updates (draw then update; BurstTask spawns AFTER its draw, the trails after
// theirs); the order of the shot queue.
// ENTITY WRITES: none. CAMERA: the camera animation 0xDBEC8C (engine) and the shake word 0x1D97712 (MainTask).
// Module globals: 0x244D670..0x244FD60.
// FUJIN: the file loader (0x60C9A0) and the setup (0x60C9C0) are ported too, for Fujin's programs (their memory may be a
// copy's: own::R). Parameters (CannonBlowParams): the shot sound, the texture name, the camera track, and the SMOKE
// GRENADE options - not the game's (smoke.on 0 = the game's effect, every value below unused): a new effect's look
// (Cronos's Smoke Grenade, effect 360): no flash, burst, ring, fire ball or fire trails at the caster; a canister
// spinning along the shot's curve leaving a thin smoke trail; at the landing (counter 14) no impact, debris or camera
// shake but a smoke cloud billowing out of the landing point, hanging, thinning out. Its puffs are the debris
// flipbook's frames redrawn with their palette row moved to smoke.row (the smoke texture's grey ramp: brightness =
// opacity) and drawn twice, subtracting then adding (soft, see-through smoke), in static memory (one cast at a time).
// NEVER-WRITTEN STACK WORDS: the 4th word of MainTask's local point (entry esp - 0x32): the effect point 0xF1
// (GetEffectSpawnPosition writes 3 words) is copied with it into the DebrisTasks' +0x16 (only ever loaded into the GTE VZ0
// pad: never read); modelled 0 (DebrisPad).

#include "mag_common.h"
#include <algorithm>
#include <cstring>
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace cannonblow151
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
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x244F928)); }
	inline int32_t &CasterSlot() { return var<int32_t>(own::R(0x244F92C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x244FD58)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x244FD5C)); }
	inline TaskQueue *RootQueue() { return (TaskQueue *)own::R(0x244D698); }
	inline TaskQueue *ShotQueue() { return (TaskQueue *)own::R(0x244E9E8); }
	static const uint32_t PARTS_A = 0x244D6A8, PARTS_B = 0x244E008, PARTS_END = 0x244E968; // 100 x 0x18 each
	inline uint32_t Ctrl(int i) { return own::R(0x244F808) + (uint32_t)i * 0x20; }   // 4 control points (8 bytes)
	inline uint32_t Target(int i) { return own::R(0x244F888) + (uint32_t)i * 8; }
	inline Mat4x3 *Rot(int i) { return (Mat4x3 *)(own::R(0x244F8A8) + (uint32_t)i * 0x20); }
	inline uint32_t Start(int i) { return own::R(0x244F938) + (uint32_t)i * 8; }
	inline uint32_t Point(int i, int k) { return own::R(0x244F958) + ((uint32_t)i * 0x20 + (uint32_t)k) * 8; } // 32 curve points

	static const uint32_t ORIG_Root = 0x60E270, ORIG_TexRestore = 0x60E2E0, ORIG_Main = 0x60CA90, ORIG_Flash = 0x60D260,
		ORIG_Burst = 0x60D3B0, ORIG_Ring = 0x60D670, ORIG_Ball = 0x60D790, ORIG_Trail = 0x60D850, ORIG_Trail2 = 0x60DA10,
		ORIG_Trail3 = 0x60DC20, ORIG_Impact = 0x60DE40, ORIG_ImpactRing = 0x60E060, ORIG_Debris = 0x60E1B0;
	static const uint32_t MODEL_Flash = 0xDC0E10, MODEL_Ring = 0xDC0900, MODEL_Impact = 0xDBF4C0, MODEL_ImpactScroll = 0xDC00F0,
		MODEL_ImpactRing = 0xDC1498;
	static const uint32_t SEQ_Burst = 0xDBF314, SEQ_Ball = 0xDBF2A0, SEQ_Trail = 0xDBEF48, SEQ_Trail2 = 0xDBF0F4, SEQ_Debris = 0xDBED9C;
	static const uint32_t SOUND_Shot = 0xDC1DC0;
	static const uint32_t ORIG_FileLoader = 0x60C9A0, ORIG_Setup = 0x60C9C0;
	static const uint32_t TEXTURE_NAME = 0xDC1DC8, CAMERA = 0xDBEC8C;
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x244F930)); }
	inline int32_t &FirstTarget() { return var<int32_t>(own::R(0x244D670)); }

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_CANISTER = 1, PART_TRAIL = 2, PART_CLOUD = 3 };
	struct CannonBlowParams
	{
		struct { uint32_t shot; } sounds;  // MainTask's counter 1
		// the smoke grenade (not the game's: on 0 = the game's effect)
		struct { int16_t on, row, hold, thin_rand, thin_ticks; } smoke;
		struct { int16_t count, delay, jitter, speed_min, speed_rand, rise_min, rise_rand, brake, size_start, size_min, size_rand,
		         grow, frame_min, frame_rand, spin, drift_up, turb, swell, r, g, b, sub, shade_rand, top_light; } cloud;
		struct { int16_t size, spin, shade, sub, frame; } canister;
		struct { int16_t per_point, jitter, size, size_rand, grow, life, rise, shade, sub; } trail;
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track
	};
	static const CannonBlowParams GAME = {
		{ SOUND_Shot },
		{ 0, 249, 14, 6, 8 },
		{ 110, 5, 60, 60, 200, 20, 170, 5, 0x500, 0x1000, 0x1000, 4, 2, 14, 6, 4, 6, 4, 118, 118, 120, 150, 60, 40 },
		{ 0x400, 300, 40, 255, 9 },
		{ 2, 60, 0x500, 0x300, 0x50, 12, 6, 110, 120 },
		{ TEXTURE_NAME, CAMERA } };
	static CannonBlowParams live = GAME;
	static const CannonBlowParams &P() { return live; }

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline int32_t SpawnPosition(uint8_t *entity, int32_t point, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, point, flag, out); }
	inline void CentrePosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); } // computeCosine
	inline void GteLoadV012u(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteRTPTx() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIPx() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadFLAGu(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteReadSXY012u(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteAVSZ3x() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4x() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBCu(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }
	inline void GteDPCSx() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCTx() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2u(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }
	inline void GteLoadRGB012u(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteStoreRGB012u(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }

	// the 4th word of MainTask's effect-point local (never written; see the module comment)
	static int16_t DebrisPad() { return 0; }

	// a pool / particle record (0x18 bytes): +0 owner mask, +4 frame, +6 size, +8 point (4 words), +0x10 velocity (3 words)
	enum { P_MASK = 0, P_FRAME = 4, P_SIZE = 6, P_POS = 8, P_VEL = 0x10 };

	static uint32_t ClipX(uint32_t xy, uint32_t bit) { const int16_t x = S16v(xy, 0); return (x < 0 || x > 0xA00) ? bit : 0; }
	static uint32_t ClipY(uint32_t xy, uint32_t bit) { const int16_t y = S16v(xy, 2); return (y < 0 || y > 0x6C0) ? bit : 0; }

	// ==================================================================
	// The scrolling prim renderer (0x64E080, header 0x7C on the scratch: +0x00 model, +0x04 vertices, +0x08..+0x0A far colour,
	// +0x0C fade, +0x10 / +0x14 u / v scroll, +0x18 / +0x1A texture window x / y, +0x1C / +0x1E its width / height, +0x20 flags,
	// +0x24 section cursor) and its 4 textured section renderers = Hyper Shot 198's (mag198_hyper_shot.cpp)
	// ==================================================================
	// the face's semi-transparency bit (flags bit `set` sets it, bit `clear` clears it) of the code word at pk + off
	static void ScrSemi(uint32_t h, uint32_t pk, int32_t off, uint32_t set, uint32_t clear)
	{
		const uint32_t f = U32(h, 0x20);
		if (f & set) U32(pk, off) = U32(pk, off) | 0x2000000;
		if (f & clear) U32(pk, off) &= 0xFDFFFFFF;
	}
	// the u (o = 0) / v (o = 1) scroll of n texture coordinates (bytes at pk + off[i] + o): + the scroll (+0x6C / +0x70), then
	// - the window span when one reaches 0x100, else + it when one is negative (the sums at +0x5C..)
	static void ScrScroll(uint32_t h, uint32_t pk, const int32_t *off, int n, int32_t o, int32_t span)
	{
		const int32_t s = S32(h, o ? 0x70 : 0x6C);
		if (!s) return;
		int32_t a[4];
		for (int i = 0; i < n; i++) { a[i] = (int32_t)U8(pk, off[i] + o) + s; S32(h, 0x5C + 4 * i) = a[i]; }
		bool hi = false, lo = false;
		for (int i = 0; i < n; i++) if (a[i] >= 0x100) hi = true;
		if (!hi) for (int i = 0; i < n; i++) if (a[i] < 0) lo = true;
		if (hi) for (int i = 0; i < n; i++) S32(h, 0x5C + 4 * i) = a[i] - span;
		else if (lo) for (int i = 0; i < n; i++) S32(h, 0x5C + 4 * i) = a[i] + span;
		for (int i = 0; i < n; i++) U8(pk, off[i] + o) = U8(h, 0x5C + 4 * i);
	}
	// the depth cue of one colour word (flags 0x40)
	static void ScrCueFlat(uint32_t h, uint32_t c)
	{
		GteLoadRGBCu(c);
		GteSetIR0(S32(h, 0xC));
		GteDPCSx();
		GteStoreRGB2u(c);
	}
	static void ScrInsert(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(h, 0x30) >> (shift & 31), 2), (void *)pk);
	}


	// 0x64E1D0 (= Hyper Shot 198's 0x5AB3A0): textured flat triangles with the texture window (0x14-byte records -> 0x28-byte packets)
	static uint32_t ScrFT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const int32_t w = S16v(h, 0x1C), hh = S16v(h, 0x1E);
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		static const int32_t UV[3] = { 0x10, 0x18, 0x20 };
		for (; count > 0; count--, rec += 0x14)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x9000000;
			U32(pk, 8) = U32(rec, 0);
			ScrSemi(h, pk, 8, 1, 4);
			U32(pk, 0x10) = U32(rec, 0xC);
			U32(pk, 0x18) = U32(rec, 0x10);
			U32(pk, 0x20) = U32(rec, 8) >> 16;
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			ScrScroll(h, pk, UV, 3, 0, w);
			ScrScroll(h, pk, UV, 3, 1, hh);
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x10)) continue;
			GteReadSXY012u(pk + 0xC, pk + 0x14, pk + 0x1C);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 0xC, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x1C, 4);
			m |= ClipY(pk + 0xC, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x1C, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x40) ScrCueFlat(h, pk + 8);
			U32(pk, 4) = U32(h, 0x74);
			U32(pk, 0x24) = U32(h, 0x78);
			ScrInsert(h, ot, shift, pk);
			pk += 0x28;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x64E500 (= Hyper Shot 198's 0x5AB6D0): textured flat quads with the texture window (0x18-byte records -> 0x30-byte packets)
	static uint32_t ScrFT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const int32_t w = S16v(h, 0x1C), hh = S16v(h, 0x1E);
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		static const int32_t UV[4] = { 0x10, 0x18, 0x20, 0x28 };
		for (; count > 0; count--, rec += 0x18)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xB000000;
			U32(pk, 8) = U32(rec, 0);
			ScrSemi(h, pk, 8, 1, 4);
			U32(pk, 0x10) = U32(rec, 0xC);
			U32(pk, 0x20) = U32(rec, 0x14);
			U32(pk, 0x18) = U32(rec, 0x10);
			U32(pk, 0x28) = U32(rec, 0x14) >> 16;
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			ScrScroll(h, pk, UV, 4, 0, w);
			ScrScroll(h, pk, UV, 4, 1, hh);
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x10)) continue;
			GteReadSXY012u(pk + 0xC, pk + 0x14, pk + 0x1C);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 0xC, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x1C, 4);
			m |= ClipY(pk + 0xC, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x1C, 0x40);
			GteReadSXY2u(pk + 0x24);
			GteAVSZ4x();
			m |= ClipX(pk + 0x24, 8) | ClipY(pk + 0x24, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x40) ScrCueFlat(h, pk + 8);
			U32(pk, 4) = U32(h, 0x74);
			U32(pk, 0x2C) = U32(h, 0x78);
			ScrInsert(h, ot, shift, pk);
			pk += 0x30;
		}
		U32(h, 0x24) = rec;
		return pk;
	}


	// 0x64E900 (= Hyper Shot 198's 0x5ABFC0): textured Gouraud triangles with the texture window (0x1C-byte records -> 0x30-byte packets)
	static uint32_t ScrGT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const int32_t w = S16v(h, 0x1C), hh = S16v(h, 0x1E);
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		static const int32_t UV[3] = { 0x10, 0x1C, 0x28 };
		for (; count > 0; count--, rec += 0x1C)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xB000000;
			U32(pk, 8) = U32(rec, 0);
			ScrSemi(h, pk, 8, 2, 8);
			U32(pk, 0x10) = U32(rec, 0xC);
			U32(pk, 0x1C) = U32(rec, 0x10);
			U32(pk, 0x28) = U32(rec, 8) >> 16;
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			ScrScroll(h, pk, UV, 3, 0, w);
			ScrScroll(h, pk, UV, 3, 1, hh);
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x20)) continue;
			GteReadSXY012u(pk + 0xC, pk + 0x18, pk + 0x24);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 0xC, 1) | ClipX(pk + 0x18, 2) | ClipX(pk + 0x24, 4);
			m |= ClipY(pk + 0xC, 0x10) | ClipY(pk + 0x18, 0x20) | ClipY(pk + 0x24, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x80)
			{
				GteLoadRGB012u(rec + 0x14, rec + 0x18, pk + 8);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0x14, pk + 0x20, pk + 8);
			}
			else
			{
				U32(pk, 0x14) = U32(rec, 0x14);
				U32(pk, 0x20) = U32(rec, 0x18);
			}
			U32(pk, 4) = U32(h, 0x74);
			U32(pk, 0x2C) = U32(h, 0x78);
			ScrInsert(h, ot, shift, pk);
			pk += 0x30;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x64EC60 (= Hyper Shot 198's 0x5AC320): textured Gouraud quads with the texture window (0x24-byte records -> 0x3C-byte packets)
	static uint32_t ScrGT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const int32_t w = S16v(h, 0x1C), hh = S16v(h, 0x1E);
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		static const int32_t UV[4] = { 0x10, 0x1C, 0x28, 0x34 };
		for (; count > 0; count--, rec += 0x24)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xE000000;
			U32(pk, 8) = U32(rec, 0);
			ScrSemi(h, pk, 8, 2, 8);
			U32(pk, 0x10) = U32(rec, 0xC);
			U32(pk, 0x28) = U32(rec, 0x14);
			U32(pk, 0x1C) = U32(rec, 0x10);
			U32(pk, 0x34) = U32(rec, 0x14) >> 16;
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			ScrScroll(h, pk, UV, 4, 0, w);
			ScrScroll(h, pk, UV, 4, 1, hh);
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x20)) continue;
			GteReadSXY012u(pk + 0xC, pk + 0x18, pk + 0x24);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 0xC, 1) | ClipX(pk + 0x18, 2) | ClipX(pk + 0x24, 4);
			m |= ClipY(pk + 0xC, 0x10) | ClipY(pk + 0x18, 0x20) | ClipY(pk + 0x24, 0x40);
			GteReadSXY2u(pk + 0x30);
			GteAVSZ4x();
			m |= ClipX(pk + 0x30, 8) | ClipY(pk + 0x30, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x80)
			{
				GteLoadRGB012u(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0x14, pk + 0x20, pk + 0x2C);
				GteLoadRGBCu(pk + 8);
				GteDPCSx();
				GteStoreRGB2u(pk + 8);
			}
			else
			{
				U32(pk, 0x14) = U32(rec, 0x18);
				U32(pk, 0x20) = U32(rec, 0x1C);
				U32(pk, 0x2C) = U32(rec, 0x20);
			}
			U32(pk, 4) = U32(h, 0x74);
			U32(pk, 0x38) = U32(h, 0x78);
			ScrInsert(h, ot, shift, pk);
			pk += 0x3C;
		}
		U32(h, 0x24) = rec;
		return pk;
	}


	// 0x64E080: the scrolling renderer (198's 0x5AAD60 with only its textured lists: the untextured counts are skipped unread)
	static uint32_t ScrollRender(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t model = U32(h, 0);
		U32(h, 4) = model + 8;
		U32(h, 0x24) = model + U32(model, 0);
		uint32_t tw = ((uint32_t)U8(h, 0x1A) & 0xF8) | 0xFFFE2000u;
		tw = (tw << 5) | ((uint32_t)U8(h, 0x18) & 0xF8);
		tw = (tw << 5) | (~((uint32_t)U16(h, 0x1E) - 1) & 0xF8);
		tw = (tw << 2) | ((~((uint32_t)U16(h, 0x1C) - 1) >> 3) & 0x1F);
		U32(h, 0x74) = tw;
		U32(h, 0x78) = 0xE2000000;
		S32(h, 0x6C) = S32(h, 0x10) % (int32_t)S16v(h, 0x1C);
		S32(h, 0x70) = S32(h, 0x14) % (int32_t)S16v(h, 0x1E);
		GteSetFarColor(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x24) += 8;
		if (U32(U32(h, 0x24), 0)) cursor = ScrFT3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrFT4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		U32(h, 0x24) += 8;
		if (U32(U32(h, 0x24), 0)) cursor = ScrGT3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrGT4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		return cursor;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag151_cannon_blow_held.h"
#endif

namespace ff8fx
{
namespace cannonblow151
{
	// ------------------------------------------------------------------
	// Shared draws
	// ------------------------------------------------------------------
	// a prim model (header 0x58) at the GTE matrix set by the caller: flags 0x33, faded (0xF3, +0x0C) when fade >= 0
	static void PrimDraw(uint32_t model, int32_t fade)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = model;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x33;
		if (fade >= 0)
		{
			U32(h, 0x1C) = 0xF3;
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

	// the first free particle of [lo, hi) (mask 0), or 0 (none in the first 100)
	static uint32_t FreeParticle(uint32_t lo, uint32_t hi)
	{
		int32_t i = 0;
		uint32_t p = lo;
		for (; U32(p, P_MASK) != 0; p += 0x18, i++)
			if (p + 0x18 >= hi) return 0;
		return i < 0x64 ? p : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x60E270) and texture restore (0x60E2E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? TexBase() + 0xC000 : TexBase();
		const int running = ExecuteTaskQueue(ShotQueue());
		if (!running)
		{
			const uint32_t w = (uint32_t)AddTaskToQueue(RootQueue(), own::Fn(ORIG_TexRestore));
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
	// Flash (0x60D260), Ring (0x60D670), ImpactRing (0x60E060)
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
		// 30 fps layer: see mag151_cannon_blow_held.inc
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
		// 30 fps layer: see mag151_cannon_blow_held.inc
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
		// 30 fps layer: see mag151_cannon_blow_held.inc
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
	// Impact (0x60DE40): a prim model, then the scrolling model with the same rotation
	// ------------------------------------------------------------------
	// fade: (counter - 8) << 9 from counter 8 (-1: none); vscroll: 14 * counter
	static void ImpactDraw(uint32_t n, int32_t fade, int32_t vscroll)
	{
		const int16_t ang[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m, r;
		ComposeZYXRotationMatrix(ang, &m);
		r = m;
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		const int32_t s[3] = { S16(n, 0x1C), S16(n, 0x1C), S16(n, 0x1C) };
		Scale3DMatrix(&m, s);
		m.t[2] = S16(n, 0x14);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(MODEL_Impact, fade);
		const int32_t s2 = S16(n, 0x1C) - 0x100;
		const int32_t v[3] = { s2, s2, s2 };
		Scale3DMatrix(&r, v);
		GteMatrixMultiply(&Camera(), &r);
		GteSetRotMatrix(&r);
		const uint32_t h = (uint32_t)FieldAlloc(0x7C);
		U32(h, 0x20) = 0x33;
		U32(h, 0) = MODEL_ImpactScroll;
		U32(h, 8) = 0;
		S32(h, 0x10) = 0;
		S32(h, 0x14) = vscroll;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1C) = 0x40;
		U16(h, 0x1E) = 0x80;
		if (fade >= 0)
		{
			U32(h, 0x20) = 0xF3;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = ScrollRender(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl ImpactTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_IMPACT, n);)
		const int16_t c = S16(n, 0xC);
		ImpactDraw(n, c >= 8 ? shl32(c - 8, 9) : -1, mul32(c, 14));
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
	// Ball (0x60D790) and Debris (0x60E1B0): single sequences
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
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_BALL, n);)
		BallDraw(n, (const void *)Point(S16(n, 0xE), S16(n, 0x18)));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1A);
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + d);
		if (d > 2) S16(n, 0x1A) = (int16_t)(d - 1);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0x18) >= 0x20 ? TASK_END : 0;
	}

	// the sequence draw (its header's +0x28 < 0 once the sequence ended)
	static uint32_t DebrisDraw(uint32_t n)
	{
		const int16_t r = S16(n, 0x1C);
		TransformCameraByShadowRotation((const void *)(n + 0x10), (int32_t)(uint16_t)r, -(r >> 4));
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		U32(h, 0) = SEQ_Debris;
		U16(h, 4) = U16(n, 0xC);
		U16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
		return h;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int16_t wait = S16(n, 0xE);
		if (wait > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(wait - 1);
			return 0;
		}
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_node(REC_DEBRIS, n);)
		const uint32_t h = DebrisDraw(n);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(h, 0x28) < 0 ? TASK_END : 0;
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

	// Burst (0x60D3B0): particles B in the shot's frame
	static int BurstParticles(uint32_t n)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int alive = 0;
		U32(h, 0) = SEQ_Burst;
		U16(h, 0x24) = 0;
		const int i = S16(n, 0xE);
		for (uint32_t p = own::R(PARTS_B); p < own::R(PARTS_END); p += 0x18)
		{
			if (!(U32(p, P_MASK) & (uint32_t)(int32_t)S16(n, 0x18))) continue;
			// 30 fps layer: see mag151_cannon_blow_held.inc
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
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_parts(REC_BURST, n);)
		const int alive = BurstParticles(n);
		if (DrawOnly()) return 0;
		if (S16(n, 0xC) < 2)
		{
			for (int k = 0; k < 0xC; k++)
			{
				const uint32_t p = FreeParticle(own::R(PARTS_B), own::R(PARTS_END));
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
			// 30 fps layer: see mag151_cannon_blow_held.inc
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

	// Trail (0x60D850) / Trail2 (0x60DA10): one particle A a curve point (Trail2: jittered)
	static uint32_t TrailTaskImpl(uint32_t n, bool jitter)
	{
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_parts(jitter ? REC_TRAIL2 : REC_TRAIL, n);)
		const int alive = TrailParticles(n, own::R(PARTS_A), own::R(PARTS_B), jitter ? SEQ_Trail2 : SEQ_Trail, jitter ? 8 : 0, 4);
		if (DrawOnly()) return 0;
		TrailStep(n);
		const int32_t at = S16(n, 0x1C);
		if (at < 0x20)
		{
			for (int32_t k = at; k >= S16(n, 0x1C) - S16(n, 0x1E); k--)
			{
				const uint32_t p = FreeParticle(own::R(PARTS_A), own::R(PARTS_B));
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

	// Trail3 (0x60DC20): two particles B a curve point
	static uint32_t __cdecl Trail3Task(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(held_note_parts(REC_TRAIL3, n);)
		const int alive = TrailParticles(n, own::R(PARTS_B), own::R(PARTS_END), SEQ_Burst, 0, 3);
		if (DrawOnly()) return 0;
		TrailStep(n);
		const int32_t at = S16(n, 0x1C);
		if (at < 0x20)
		{
			for (int32_t k = at; k >= S16(n, 0x1C) - S16(n, 0x1E); k--)
			{
				for (int j = 0; j < 2; j++)
				{
					const uint32_t p = FreeParticle(own::R(PARTS_B), own::R(PARTS_END));
					if (!p) break;
					U32(p, P_MASK) = (uint32_t)(int32_t)S16(n, 0x18);
					S16(p, P_FRAME) = 0;
					S16(p, P_SIZE) = (int16_t)((CrtRand() & 0x7FF) + 0x400);
					U32(p, P_POS) = U32(Point(S16(n, 0xE), k), 0);
					U32(p, P_POS + 4) = U32(Point(S16(n, 0xE), k), 4);
					S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % 0x12C - 0x96);
					S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + CrtRand() % 0x12C - 0x96);
					S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % 0x12C - 0x96);
				}
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}


	// ------------------------------------------------------------------
	// The smoke grenade (smoke.on; not the game's): the canister, its trail, the cloud - puffs in static memory
	// ------------------------------------------------------------------
	// a puff: cloud (life 0: thins out from thin_at) or trail (life > 0: fades as its ticks run out)
	struct Puff { int32_t x, y, z, vx, vy, vz; int16_t size, target, frame, angle, spin, wait, thin_at, alive, shade, life, left, grow; };
	enum { MAX_PUFFS = 256 };
	static Puff g_puffs[MAX_PUFFS];
	static int g_tick;   // the puffs task's tick
	static uint8_t g_seq_sub[0x800], g_seq_add[0x800];
	static inline int Rnd(int n) { return n > 0 ? (int)(CrtRand() % (uint32_t)n) : 0; }

	// the debris flipbook copied with its pieces' palette row moved to smoke.row and their blend mode set (2 subtract, 1 add)
	static void SmokeSequence(uint8_t *out, int mode)
	{
		const uint8_t *src = (const uint8_t *)SEQ_Debris;
		const uint16_t frames = *(const uint16_t *)(src + 8);
		const uint16_t *offs = (const uint16_t *)(src + 10);
		uint32_t end = 0;
		for (int i = 0; i < frames; i++)
		{
			const uint32_t at = offs[i];
			end = std::max(end, at + 4 + 20 * (*(const uint32_t *)(src + at) & 0x7FFFFFFF));
		}
		if (end > sizeof(g_seq_sub)) end = sizeof(g_seq_sub);
		memcpy(out, src, end);
		for (int i = 0; i < frames; i++)
		{
			const uint32_t at = offs[i];
			const uint32_t n = *(const uint32_t *)(out + at) & 0x7FFFFFFF;
			for (uint32_t k = 0; k < n && at + 4 + 20 * (k + 1) <= end; k++)
			{
				uint8_t *pc = out + at + 4 + 20 * k;
				uint16_t &clut = *(uint16_t *)(pc + 6);
				clut = (uint16_t)((clut & 0x3F) | ((P().smoke.row & 0x1FF) << 6));
				uint16_t &fl = *(uint16_t *)(pc + 14);
				fl = (uint16_t)((fl & ~0x60) | ((mode & 3) << 5));
				pc[3] |= 2;   // semi-transparent
			}
		}
	}

	static void SmokeInit()
	{
		SmokeSequence(g_seq_sub, 2);
		SmokeSequence(g_seq_add, 1);
		memset(g_puffs, 0, sizeof(g_puffs));
		g_tick = 0;
	}

	static Puff *NewPuff()
	{
		for (int k = 0; k < MAX_PUFFS; k++)
			if (!g_puffs[k].alive)
			{
				memset(&g_puffs[k], 0, sizeof(Puff));
				g_puffs[k].alive = 1;
				return &g_puffs[k];
			}
		return nullptr;
	}

	static void PuffPass(int32_t x, int32_t y, int32_t z, int16_t size, int16_t frame, int16_t angle, const uint8_t *seq, int rr, int gg, int bb)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		U32(h, 0) = (uint32_t)seq;
		U16(h, 4) = (uint16_t)frame;
		U8(h, 0x1C) = (uint8_t)std::min(std::max(rr, 0), 255);
		U8(h, 0x1D) = (uint8_t)std::min(std::max(gg, 0), 255);
		U8(h, 0x1E) = (uint8_t)std::min(std::max(bb, 0), 255);
		S32(h, 8) = angle;
		U16(h, 0x24) = 0x5;   // own z angle, own colour
		const int16_t pos[4] = { (int16_t)x, (int16_t)y, (int16_t)z, 0 };
		TransformCameraByShadowRotation(pos, (int32_t)(uint16_t)size, -(size >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// subtract (the background darkened by the texel's brightness: its opacity), then add the grey (the ordering table
	// draws what is inserted last first)
	static void SoftDraw(int32_t x, int32_t y, int32_t z, int16_t size, int16_t frame, int16_t angle, int rr, int gg, int bb, int sub)
	{
		PuffPass(x, y, z, size, frame, angle, g_seq_add, rr, gg, bb);
		PuffPass(x, y, z, size, frame, angle, g_seq_sub, sub, sub, sub);
	}

	static void PuffDraw(const Puff &q)
	{
		const CannonBlowParams &v = P();
		int k = 256;   // 256 = whole
		int rr, gg, bb, sub;
		if (q.life > 0)
		{
			FX_PART(PART_TRAIL);
			k = 256 * q.left / q.life;
			rr = gg = bb = v.trail.shade;
			sub = v.trail.sub;
		}
		else
		{
			FX_PART(PART_CLOUD);
			if (g_tick >= q.thin_at) k = 256 * std::max(0, q.thin_at + v.smoke.thin_ticks - g_tick) / std::max(1, (int)v.smoke.thin_ticks);
			int sh = q.shade + (v.cloud.top_light * (0 - (q.y >> 4)) / 1000);
			sh = std::min(std::max(sh, 64), 400);
			rr = v.cloud.r * sh >> 8; gg = v.cloud.g * sh >> 8; bb = v.cloud.b * sh >> 8;
			sub = v.cloud.sub;
		}
		SoftDraw(q.x >> 4, q.y >> 4, q.z >> 4, q.size, q.frame, q.angle, rr * k >> 8, gg * k >> 8, bb * k >> 8, sub * k >> 8);
		FX_PART(0);
	}

	// the cloud's puffs at the landing point of shot i (on the floor)
	static void CloudStart(int i)
	{
		const CannonBlowParams &v = P();
		for (int k = 0; k < v.cloud.count; k++)
		{
			Puff *q = NewPuff();
			if (!q) break;
			q->shade = (int16_t)(256 - Rnd(v.cloud.shade_rand + 1));
			q->x = (S16(Target(i), 0) + Rnd(2 * v.cloud.jitter + 1) - v.cloud.jitter) << 4;
			q->z = (S16(Target(i), 4) + Rnd(2 * v.cloud.jitter + 1) - v.cloud.jitter) << 4;
			q->y = -40 << 4;
			const int32_t a = CrtRand() & 0xFFF;
			const int32_t speed = v.cloud.speed_min + Rnd(v.cloud.speed_rand);
			q->vx = (ComputeCos(a) * speed) >> 8;
			q->vz = (ComputeSin(a) * speed) >> 8;
			q->vy = -((v.cloud.rise_min + Rnd(v.cloud.rise_rand)) << 4);
			q->size = v.cloud.size_start;
			q->target = (int16_t)(v.cloud.size_min + Rnd(v.cloud.size_rand));
			q->frame = (int16_t)(v.cloud.frame_min + Rnd(v.cloud.frame_rand));
			q->angle = (int16_t)(CrtRand() & 0xFFF);
			q->spin = (int16_t)(Rnd(2 * v.cloud.spin + 1) - v.cloud.spin);
			q->wait = (int16_t)Rnd(v.cloud.delay + 1);
			q->thin_at = (int16_t)(g_tick + v.smoke.hold + Rnd(v.smoke.thin_rand + 1));
		}
	}

	// a trail puff about a point
	static void TrailPuff(const int16_t *pt)
	{
		const CannonBlowParams &v = P();
		Puff *q = NewPuff();
		if (!q) return;
		const int j = v.trail.jitter;
		q->x = (pt[0] + Rnd(2 * j + 1) - j) << 4;
		q->y = (pt[1] + Rnd(2 * j + 1) - j) << 4;
		q->z = (pt[2] + Rnd(2 * j + 1) - j) << 4;
		q->vy = -(v.trail.rise << 4);
		q->size = q->target = (int16_t)(v.trail.size + Rnd(v.trail.size_rand));
		q->grow = v.trail.grow;
		q->frame = (int16_t)(v.cloud.frame_min + Rnd(v.cloud.frame_rand));
		q->angle = (int16_t)(CrtRand() & 0xFFF);
		q->spin = (int16_t)(Rnd(2 * v.cloud.spin + 1) - v.cloud.spin);
		q->life = q->left = (int16_t)std::max(1, (int)v.trail.life);
	}

	// every puff (trail and cloud): node +0xC its tick; ends once no puff is left (after counter 16)
	static uint32_t __cdecl PuffsTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const CannonBlowParams &v = P();
		int alive = 0;
		for (int k = 0; k < MAX_PUFFS; k++)
		{
			Puff &q = g_puffs[k];
			if (!q.alive) continue;
			alive++;
			if (q.wait > 0) { if (!DrawOnly()) q.wait--; continue; }
			PuffDraw(q);
			if (DrawOnly()) continue;
			q.x += q.vx; q.y += q.vy; q.z += q.vz;
			q.angle = (int16_t)((q.angle + q.spin) & 0xFFF);
			if (q.life > 0)
			{
				q.size = (int16_t)std::min(0x7000, q.size + q.grow);
				if (--q.left <= 0) q.alive = 0;
				continue;
			}
			const int brake = std::max(1, (int)v.cloud.brake);
			q.vx -= q.vx / brake;
			q.vz -= q.vz / brake;
			q.vy -= q.vy / brake;
			if (q.vy > -(v.cloud.drift_up << 4)) q.vy = -(v.cloud.drift_up << 4);
			q.size = (int16_t)(q.size + (q.target - q.size) / std::max(1, (int)v.cloud.grow));
			if (v.cloud.turb > 0)
			{
				q.vx += (Rnd(2 * v.cloud.turb + 1) - v.cloud.turb) << 3;
				q.vz += (Rnd(2 * v.cloud.turb + 1) - v.cloud.turb) << 3;
			}
			q.target = (int16_t)std::min(0x7000, q.target + v.cloud.swell * 16);
			if (g_tick >= q.thin_at + v.smoke.thin_ticks) q.alive = 0;
		}
		if (DrawOnly()) return 0;
		g_tick++;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return (S16(n, 0xC) > 0x10 && !alive) ? TASK_END : 0;
	}

	// the canister on shot i's curve (node +0x18 the point, +0x1A the step: the fire ball's), leaving trail puffs
	static uint32_t __cdecl CanisterTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const CannonBlowParams &v = P();
		const int i = S16(n, 0xE);
		const int16_t at = S16(n, 0x18);
		const int16_t *pt = (const int16_t *)Point(i, std::min<int>(at, 0x1F));
		const int16_t angle = (int16_t)((S16(n, 0xC) * v.canister.spin) & 0xFFF);
		FX_PART(PART_CANISTER);
		SoftDraw(pt[0], pt[1], pt[2], v.canister.size, v.canister.frame, angle, v.canister.shade, v.canister.shade, v.canister.shade,
		         v.canister.sub);
		FX_PART(0);
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1A);
		for (int k = std::max(0, at - d + 1); k <= std::min<int>(at, 0x1F); k++)
			for (int j = 0; j < v.trail.per_point; j++) TrailPuff((const int16_t *)Point(i, k));
		S16(n, 0x18) = (int16_t)(at + d);
		if (d > 2) S16(n, 0x1A) = (int16_t)(d - 1);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0x18) >= 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Main (0x60CA90)
	// ------------------------------------------------------------------
	static uint32_t NewTask(uint32_t f, int16_t index)
	{
		const uint32_t t = (uint32_t)AddTaskToQueue(ShotQueue(), own::Fn(f));
		S16(t, 0xC) = 0;
		S16(t, 0xE) = index;
		return t;
	}

	// the shot's start, target, curve and rotation (counter 0)
	static void ShotSetup(int i)
	{
		const uint32_t ent = (uint32_t)Entity(CasterSlot());
		SpawnPosition((uint8_t *)ent, 0xF, 0x1000, (int16_t *)Start(i));
		int16_t off[4];
		off[1] = -250;
		off[2] = -250;
		off[0] = 0xC8;
		off[3] = 0; // never written, never read (0x56C4F0 reads 3 words)
		MatVec((const Mat4x3 *)(ent + 0x40), off, off);
		S16(Start(i), 0) = (int16_t)(S16(Start(i), 0) + off[0]);
		S16(Start(i), 2) = (int16_t)(S16(Start(i), 2) + off[1]);
		S16(Start(i), 4) = (int16_t)(S16(Start(i), 4) + off[2]);
		CentrePosition(Ctx()->target_mask, (int16_t *)Target(i));
		S16(Target(i), 0) = (int16_t)(S16(Target(i), 0) + CrtRand() % 0x320 - 0x190);
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

	static uint32_t __cdecl MainTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int16_t i = S16(n, 0xE);
		if (S16(n, 0xC) == 0) ShotSetup(i);
		if (S16(n, 0xC) == 1 && P().smoke.on)
		{
			SmokeInit();
			NewTask((uint32_t)PuffsTask, i);
			const uint32_t c = NewTask((uint32_t)CanisterTask, i);
			S16(c, 0x18) = 0;
			S16(c, 0x1A) = 5;
		}
		if (S16(n, 0xC) == 1 && !P().smoke.on)
		{
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
			t = NewTask(ORIG_Trail3, i);
			S16(t, 0x1C) = 0;
			S16(t, 0x1E) = 5;
			S16(t, 0x18) = (int16_t)(0x100 << i);
		}
		if (S16(n, 0xC) == 0xE && P().smoke.on) CloudStart(i);
		if (S16(n, 0xC) == 0xE && !P().smoke.on)
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
			const ActionData *a = Ctx()->actions;
			for (int j = 0; j < a->target_count; j++)
			{
				int16_t pt[4];
				pt[3] = DebrisPad();
				SpawnPosition(Entity(a->targets[j * TARGET_STRIDE]), 0xF1, 0, pt);
				for (int k = 0; k < 5; k++)
				{
					const uint32_t d = (uint32_t)AddTaskToQueue(ShotQueue(), own::Fn(ORIG_Debris));
					S16(d, 0xC) = 0;
					S16(d, 0xE) = (int16_t)(CrtRand() % 6 + 2 * k);
					memcpy((void *)(d + 0x10), pt, 8);
					S16(d, 0x10) = (int16_t)(S16(d, 0x10) + CrtRand() % 0x320 - 0x190);
					S16(d, 0x12) = (int16_t)(S16(d, 0x12) + CrtRand() % 0x320 - 0x190);
					S16(d, 0x14) = (int16_t)(S16(d, 0x14) + CrtRand() % 0x320 - 0x190);
					S16(d, 0x1C) = (int16_t)((CrtRand() & 0x7FF) + 0x800);
				}
			}
		}
		const int16_t c = S16(n, 0xC);
		if (!P().smoke.on && c >= 0xE && c <= 0x11) var<int16_t>(0x1D97712) = (int16_t)(var<int16_t>(0x1D97712) + ((c & 1) ? 0x40 : -0x40));
		if (c == 0x1E) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (S16(n, 0xC) == 1) BdPlaySE((const void *)fxp::sound(P().sounds.shot, SOUND_Shot), 0, 0x80);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x28 ? TASK_END : 0;
	}


	// ------------------------------------------------------------------
	// File loader (0x60C9A0) and setup (0x60C9C0): the game runs its own; Fujin's programs these (its memory may be a
	// copy's): the texture, the magic buffer, the cast, its first target's and its caster's slots, the root and the
	// main task, both particle arrays freed, the camera and the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = layouts::MagicBuffer();
		Ctx() = ctx;
		FirstTarget() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(RootQueue(), (void *)own::R(0x244D678), 0x10, 2);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_Root)) + 0xC) = 0;
		InitTaskQueuePool(ShotQueue(), (void *)own::R(0x244E9F8), 0x24, 0x64);
		const uint32_t t = (uint32_t)AddTaskToQueue(ShotQueue(), own::Fn(ORIG_Main));
		S16(t, 0xC) = 0;
		S16(t, 0xE) = 0;
		for (uint32_t a = own::R(PARTS_B); a < own::R(PARTS_END); a += 0x18) *(int32_t *)a = 0;
		for (uint32_t a = own::R(PARTS_A); a < own::R(PARTS_B); a += 0x18) *(int32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA));
		QueueTimUpload(TexFile());
		return RootQueue();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "shot", "Shot", "The game's Cannon Blow: a fire ball shot on a curve from in front of the caster to near the targets' centre, "
			  "its flash, burst, ring and fire trails, an impact, rings and debris at the targets, the camera shake; the damage at counter 30; "
			  "its sound and texture, the camera track. The smoke grenade's switch and timing" },
			{ PART_CANISTER, "canister", "Canister", "Smoke grenade: a small dark canister spinning along the shot's curve in place of the fire ball" },
			{ PART_TRAIL, "trail", "Smoke trail", "Smoke grenade: puffs the canister leaves along its way, growing, rising and fading out" },
			{ PART_CLOUD, "cloud", "Smoke cloud", "Smoke grenade: puffs billowing out of the landing point, slowing, swelling, rising, then thinning out" },
		};
		static const fxp::Group GROUPS[] = {
			{ "sounds", "Sounds", "The shot" },
		};
#define CB(member) (uint32_t)offsetof(CannonBlowParams, member)
		static const Field FIELDS[] = {
			{ "shot", "sounds.shot", "Shot", "At the main task's counter 1", CB(sounds.shot), fxp::Sound, 0, 0x7FFFFFFF },
			{ "shot", "smoke.on", "Smoke grenade", "0: the game's Cannon Blow. 1: a smoke grenade - no fire, flash or explosion: a canister, its smoke trail, a smoke cloud at the landing (a new effect's look; the next values)", CB(smoke.on), fxp::I16, 0, 1 },
			{ "shot", "smoke.row", "Smoke palette row", "The puffs' palette row (VRAM y): the smoke texture's grey ramp, brightness = opacity (the game's texture has no such row: row 249 of a smoke texture)", CB(smoke.row), fxp::I16, 0, 511 },
			{ "cloud", "smoke.hold", "Holds for", "Ticks after the landing before a cloud puff starts thinning out (+ the next random)", CB(smoke.hold), fxp::I16, 0, 300 },
			{ "cloud", "smoke.thin_rand", "Holds, random", "Up to this many more ticks, each puff its own", CB(smoke.thin_rand), fxp::I16, 0, 300 },
			{ "cloud", "smoke.thin_ticks", "Thins out over", "Ticks a puff takes to vanish", CB(smoke.thin_ticks), fxp::I16, 1, 300 },
			{ "cloud", "cloud.count", "Puffs", "How many puffs the cloud has (256 at most, shared with the trail)", CB(cloud.count), fxp::I16, 0, 256 },
			{ "cloud", "cloud.delay", "Released over", "The puffs come out over this many ticks after the landing, at random", CB(cloud.delay), fxp::I16, 0, 100 },
			{ "cloud", "cloud.jitter", "Start spread", "Each puff starts this far (at most) from the landing point, sideways", CB(cloud.jitter), fxp::I16, 0, 4000 },
			{ "cloud", "cloud.speed_min", "Speed", "Outward speed at the start (units a tick; + the next random)", CB(cloud.speed_min), fxp::I16, 0, 2000 },
			{ "cloud", "cloud.speed_rand", "Speed, random", "", CB(cloud.speed_rand), fxp::I16, 0, 2000 },
			{ "cloud", "cloud.rise_min", "Rise", "Upward speed at the start (units a tick; + the next random)", CB(cloud.rise_min), fxp::I16, 0, 2000 },
			{ "cloud", "cloud.rise_rand", "Rise, random", "", CB(cloud.rise_rand), fxp::I16, 0, 2000 },
			{ "cloud", "cloud.brake", "Slows by", "A tick the speeds lose 1 / this of themselves (small = stops fast)", CB(cloud.brake), fxp::I16, 1, 100 },
			{ "cloud", "cloud.size_start", "Size at the start", "4096 = the puff picture's own size (64 texels x 16 units)", CB(cloud.size_start), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "cloud", "cloud.size_min", "Size", "The size a puff grows to (+ the next random)", CB(cloud.size_min), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "cloud", "cloud.size_rand", "Size, random", "", CB(cloud.size_rand), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "cloud", "cloud.grow", "Grows by", "A tick a puff grows 1 / this of what it still lacks", CB(cloud.grow), fxp::I16, 1, 100 },
			{ "cloud", "cloud.frame_min", "Picture", "The flipbook frame a puff shows (+ the next random; the frame stays)", CB(cloud.frame_min), fxp::I16, 0, 15 },
			{ "cloud", "cloud.frame_rand", "Picture, random", "", CB(cloud.frame_rand), fxp::I16, 0, 16 },
			{ "cloud", "cloud.spin", "Spin", "A puff turns up to this much a tick, either way (the trail's too)", CB(cloud.spin), fxp::I16, 0, 0x400, fxp::Angle },
			{ "cloud", "cloud.drift_up", "Drifts up", "Once slowed, a puff still rises this much a tick", CB(cloud.drift_up), fxp::I16, 0, 200 },
			{ "cloud", "cloud.turb", "Sway", "A random push sideways a tick (the cloud churning)", CB(cloud.turb), fxp::I16, 0, 200 },
			{ "cloud", "cloud.swell", "Swells", "A puff keeps growing this much a tick (x 16)", CB(cloud.swell), fxp::I16, 0, 200 },
			{ "cloud", "cloud.r", "Red", "The grey added (128 = the texture as it is)", CB(cloud.r), fxp::I16, 0, 255 },
			{ "cloud", "cloud.g", "Green", "", CB(cloud.g), fxp::I16, 0, 255 },
			{ "cloud", "cloud.b", "Blue", "", CB(cloud.b), fxp::I16, 0, 255 },
			{ "cloud", "cloud.sub", "Thickness", "How much of what is behind a puff it hides (x the texel / 128; at least the grey, or the puffs pile up to white)", CB(cloud.sub), fxp::I16, 0, 255 },
			{ "cloud", "cloud.shade_rand", "Shade, random", "A puff is up to this much darker (256 = black)", CB(cloud.shade_rand), fxp::I16, 0, 255 },
			{ "cloud", "cloud.top_light", "Lighter on top", "A puff is this much lighter for every 1000 units over the floor (256 = twice)", CB(cloud.top_light), fxp::I16, 0, 256 },
			{ "canister", "canister.size", "Size", "4096 = the puff picture's own size", CB(canister.size), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "canister", "canister.spin", "Spin", "Its turn a tick", CB(canister.spin), fxp::I16, 0, 0x800, fxp::Angle },
			{ "canister", "canister.shade", "Grey", "Its colour (128 = the texture as it is)", CB(canister.shade), fxp::I16, 0, 255 },
			{ "canister", "canister.sub", "Thickness", "How much of what is behind it it hides", CB(canister.sub), fxp::I16, 0, 255 },
			{ "canister", "canister.frame", "Picture", "The flipbook frame it shows", CB(canister.frame), fxp::I16, 0, 15 },
			{ "trail", "trail.per_point", "Puffs a point", "Puffs left at each point of the curve the canister passes", CB(trail.per_point), fxp::I16, 0, 8 },
			{ "trail", "trail.jitter", "Spread", "Each puff this far (at most) from the curve, on each axis", CB(trail.jitter), fxp::I16, 0, 2000 },
			{ "trail", "trail.size", "Size", "(+ the next random)", CB(trail.size), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "trail", "trail.size_rand", "Size, random", "", CB(trail.size_rand), fxp::I16, 0, 0x7000, fxp::Scale },
			{ "trail", "trail.grow", "Grows", "Its size grows this much a tick", CB(trail.grow), fxp::I16, 0, 0x1000, fxp::Scale },
			{ "trail", "trail.life", "Lives for", "Ticks; it fades out over them", CB(trail.life), fxp::I16, 1, 300 },
			{ "trail", "trail.rise", "Rise", "It rises this much a tick", CB(trail.rise), fxp::I16, 0, 200 },
			{ "trail", "trail.shade", "Grey", "Its colour (128 = the texture as it is)", CB(trail.shade), fxp::I16, 0, 255 },
			{ "trail", "trail.sub", "Thickness", "How much of what is behind it it hides", CB(trail.sub), fxp::I16, 0, 255 },
			{ "shot", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters; a smoke grenade's: one with the smoke puffs and their grey row)", CB(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "shot", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", CB(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
		};
#undef CB
	}

	void register_mag151_cannon_blow_ports()
	{
		fxp::register_params({ 151, "Cannon Blow", &live, &GAME, sizeof(CannonBlowParams),
		                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
		                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
		                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
		own::note(ORIG_FileLoader, (void *)FileLoader);
		own::note(ORIG_Setup, (void *)Setup);
		own::note(ORIG_Root, (void *)RootTask);
		register_port(ORIG_Root, (void *)RootTask, "151 RootTask", 151);
		own::note(ORIG_TexRestore, (void *)TexRestoreTask);
		register_port(ORIG_TexRestore, (void *)TexRestoreTask, "151 TexRestoreTask", 151);
		own::note(ORIG_Main, (void *)MainTask);
		register_port(ORIG_Main, (void *)MainTask, "151 MainTask", 151);
		own::note(ORIG_Flash, (void *)FlashTask);
		register_port(ORIG_Flash, (void *)FlashTask, "151 FlashTask", 151);
		own::note(ORIG_Burst, (void *)BurstTask);
		register_port(ORIG_Burst, (void *)BurstTask, "151 BurstTask", 151);
		own::note(ORIG_Ring, (void *)RingTask);
		register_port(ORIG_Ring, (void *)RingTask, "151 RingTask", 151);
		own::note(ORIG_Ball, (void *)BallTask);
		register_port(ORIG_Ball, (void *)BallTask, "151 BallTask", 151);
		own::note(ORIG_Trail, (void *)TrailTask);
		register_port(ORIG_Trail, (void *)TrailTask, "151 TrailTask", 151);
		own::note(ORIG_Trail2, (void *)Trail2Task);
		register_port(ORIG_Trail2, (void *)Trail2Task, "151 Trail2Task", 151);
		own::note(ORIG_Trail3, (void *)Trail3Task);
		register_port(ORIG_Trail3, (void *)Trail3Task, "151 Trail3Task", 151);
		own::note(ORIG_Impact, (void *)ImpactTask);
		register_port(ORIG_Impact, (void *)ImpactTask, "151 ImpactTask", 151);
		own::note(ORIG_ImpactRing, (void *)ImpactRingTask);
		register_port(ORIG_ImpactRing, (void *)ImpactRingTask, "151 ImpactRingTask", 151);
		own::note(ORIG_Debris, (void *)DebrisTask);
		register_port(ORIG_Debris, (void *)DebrisTask, "151 DebrisTask", 151);
		// 30 fps layer: see mag151_cannon_blow_held.inc
		FX_HELD(register_mag151_held();)
	}
}

	void register_mag151_cannon_blow() { cannonblow151::register_mag151_cannon_blow_ports(); }
	FX_LAB_MODULE(register_mag151_cannon_blow);
}

#ifdef FF8_FX_HELD
#include "mag151_cannon_blow_held.inc"
#endif
