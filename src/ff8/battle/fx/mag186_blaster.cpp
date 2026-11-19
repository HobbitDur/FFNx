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

// Effect 186: Blaster (enemy attacks 122 / 123 of kernel.bin; Torama c0m057; MAG_186_*).
//
// Structure (file loader 0x5BF6D0 = the texture file 0xD208A4 -> 0x22BC100; _Init 0x5BF6F0 keeps the texture base 0x22BC120,
// the context 0x22BC0F8, the caster's slot 0x22BC0FC and the first target's slot 0x22B7600, queues the root (pool 0x22B7608,
// queue 0x22B7628) and the director in the effect queue 0x22B9930 (100 x 0x24, pool 0x22B9940), frees the 100 particles
// (0x22B7638, 0x18 bytes, word +0 = owner mask: 1 = sparks, 2 = splashes), plays the camera animation 0xD1DA60 and queues the
// TIM; both run original):
//   Root (0x5C0C80) - packet cursor 0x22BC124 = the texture base (+ 0xC000 on odd counters), the effect queue; once it is
//     empty, TexRestoreTask (0x5C0CF0, the texture restore 0x508630 on its counter 1) in the root queue; ends.
//   Director (0x5BF7B0) - EVERY tick (draw-only ones too): the mouth point 0x22BC108 = the caster's point 0x15 (flag 0x1000),
//     on tick 0 the target's default effect position 0x22BC0F0; the aim 0x22BC110 = target - mouth (normalized; its length
//     - 300 in 0x22BC10E); 0x22BBA70 = the rotation turning 0xD20800 onto the aim (in xz, then up / down); the mouth point
//     moved 300 along the aim; on tick 0 the target point pulled 400 back. Not on draw-only ticks: tick 0: the 16 in-between
//     meshes of the 4 key meshes of the sparks (0x5C0110: 0xD1E274 -> 0xD1E5AC -> 0xD1E8E4 -> 0xD1EC1C -> 0xD1E274, at
//     t = 0x333 x 1..4, into the vertex buffers of the table 0xD20814); tick 7: the charge (0x5BFCF0) and the sound
//     0xD207F4; tick 0x27: the screen fade (0x5BFC50), the sparks (0x5C0410), the glow (0x5C0690); tick 0x28: the bolts
//     (0x5C0190), the shock wave (0x5C0750), the splashes (0x5C0900); tick 0x3C: the action result; screen flash tick << 7
//     to tick 0x10, (0x41 - tick) << 8 from 0x39; ends after tick 0x41 (flash off).
//   Screen fade (0x5BFC50) - up / hold / down by the nibbles of +0xE (0xE10) to +0x1C (0xFF): the fade tile 0x571250 (v, v / 4,
//     v / 4) in the frame arena, not on draw-only ticks.
//   Charge (0x5BFCF0) - EVERY tick: the beam point 0x22B7F98 = the middle of the caster's points 0x31 / 0x32, its direction
//     0x22B7FA0 (0x31 - 0x32, normalized; length in 0x22B7F9E). Not on draw-only ticks: at each boundary of the table 0xD2087C
//     (+0xE = index) an odd interval spawns a beam (0x5BFEC0) and its ring (0x5C0040) for the interval; ends at tick 0x28.
//   Beam (0x5BFEC0) - sequence 0xD1DD5C (frame tick & 3, base 8) scaled (length x 0x680 / 820, 0x400, 0) along the beam
//     direction at the beam point; +0xE ticks.
//   Beam ring (0x5C0040) - prim model 0xD20204 through the shadow-rotation camera at the beam point (R = size, rand 0x900..
//     0xBFF, pull size >> 3); fades out over its second half ((0x1000 / half) x (tick - half), flags 0xF3); +0xE ticks.
//   Bolts (0x5C0190) - ticks 1, 3 .. 0xB: a bolt (0x5C0280: 10..19 ticks, spin +-0x200 by parity, mesh frame rand % 5, size
//     0xB00..0x14FF, fade over its last 4 ticks); ends at tick 0x1E.
//   Bolt (0x5C0280) - prim model 0xD1EF4C with the mesh 0xD20810[frame % 20] (z angle +0x18, scale size / size / aim length x
//     4096 / 5000, turned by 0x22BBA70) at the mouth point; mesh frame + 1 each tick.
//   Sparks (0x5C0410) - sequence 0xD1DF7C of mask 1 at its point (R = size, pull size >> 4), still; ticks 0..0x10: one at the
//     mouth (+-0xA0, size 0x500..0x6FF) and one at the target (+-0x104, size 0x700..0xBFF); ends from tick 4 once none is left.
//   Glow (0x5C0690) - sequence 0xD1DDD0 (frame tick & 0xF) at the mouth point (R = 0xE00, pull >> 4); from tick 0xE colour
//     (0x16 - tick) << 4 (mode 4); 0x16 ticks.
//   Shock wave (0x5C0750) - prim model 0xD1F754 (texture 0xD20860[tick & 7], +0x10 = even tick) at the target point turned by
//     the caster's y angle + 0x800, scale 0x1600; fade (tick - 0x10) << 9 from tick 0x10 (flags 0x14F3); not on draw-only
//     ticks: the target's +0x20 word shaken by +-sin(tick x 2048 / 24) x 40 (sign by parity); 0x18 ticks (put back).
//   Splashes (0x5C0900) - sequence 0xD1E128 of mask 2: each one's offset turned by 0x22BBA70 at the target point (light
//     matrix + back colour = camera x that), size as the rotation scale, pulled size >> 3 towards the eye; drawn with the
//     FRAME packet cursor 0x1D8E054; point + velocity, velocity - velocity / 4; ticks 0..0xE: four new ones (offset +-0xC8 /
//     -0..0x63, a random direction x 0xC8..0x1CB, size 0x900..0x10FF); ends from tick 4 once none is left.
// Every drawing task draws first and updates afterwards; on draw-only ticks it returns after drawing.
// ENTITY WRITES: the target's +0x20 word (shock wave; put back at its end). CAMERA: the camera animation 0xD1DA60 (engine),
// the screen flash / fade (engine). Module globals: 0x22B7600..0x22BC128.
// FUJIN: Fujin's described Blaster (effect_186.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn(), the file loader (0x5BF6D0) and the setup (0x5BF6F0) ported below; its parameters
// (BlasterParams: the sound and the director's ticks, the screen flash and fade, the mouth, the charge's beams and rings,
// the bolts, sparks, glow, shock wave and splashes, four flipbooks, three meshes, the bolt's four key vertex sets, the
// texture name and the camera track). The bolt's in-between meshes are made from the key sets in use; its key frames
// draw them (the game's: the table 0xD20810's keys, in the module the in-between buffers). FX_PART tags its parts in
// the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace blaster186
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
	inline TaskQueue *RootQueue() { return (TaskQueue *)own::R(0x22B7628); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)own::R(0x22B9930); }
	inline uint32_t &Ctx() { return var<uint32_t>(own::R(0x22BC0F8)); }
	inline int32_t &CasterSlot() { return var<int32_t>(own::R(0x22BC0FC)); }
	inline int32_t &TargetSlot() { return var<int32_t>(own::R(0x22B7600)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x22BC120)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x22BC124)); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline uint8_t *Target() { return Entity(TargetSlot()); }
	inline Mat4x3 *Aim() { return (Mat4x3 *)own::R(0x22BBA70); }
	static const uint32_t PARTS = 0x22B7638, PARTS_END = 0x22B7F98;   // 100 x 0x18 bytes
	static const uint32_t MOUTH = 0x22BC108;    // 3 words + the aim length - 300 (+6)
	static const uint32_t AIMV = 0x22BC110;     // 3 dwords
	static const uint32_t TGT = 0x22BC0F0;      // 4 words
	static const uint32_t BEAM = 0x22B7F98;     // 3 words + its length (+6)
	static const uint32_t BEAMV = 0x22B7FA0;    // 3 dwords

	static const uint32_t ORIG_Root = 0x5C0C80, ORIG_TexRestore = 0x5C0CF0, ORIG_Director = 0x5BF7B0, ORIG_Fade = 0x5BFC50,
		ORIG_Charge = 0x5BFCF0, ORIG_Beam = 0x5BFEC0, ORIG_BeamRing = 0x5C0040, ORIG_Bolts = 0x5C0190, ORIG_Bolt = 0x5C0280,
		ORIG_Sparks = 0x5C0410, ORIG_Glow = 0x5C0690, ORIG_Wave = 0x5C0750, ORIG_Splashes = 0x5C0900;
	static const uint32_t SEQ_Beam = 0xD1DD5C, SEQ_Spark = 0xD1DF7C, SEQ_Glow = 0xD1DDD0, SEQ_Splash = 0xD1E128;
	static const uint32_t MODEL_BeamRing = 0xD20204, MODEL_Bolt = 0xD1EF4C, MODEL_Wave = 0xD1F754;
	static const uint32_t SOUND_Charge = 0xD207F4, AIM_FROM = 0xD20800, BOUNDS = 0xD2087C;
	static const uint32_t BOLT_MESHES = 0xD20810, WAVE_TEXTURES = 0xD20860;
	static const uint32_t ORIG_FileLoader = 0x5BF6D0, ORIG_Setup = 0x5BF6F0;
	static const uint32_t TEXTURE_NAME = 0xD208A4, CAMERA = 0xD1DA60;
	// the bolt's four key vertex sets {size, count, 102 vertices} (the table 0xD20810's keys are their vertices)
	static const uint32_t KEY_A = 0xD1E26C, KEY_B = 0xD1E5A4, KEY_C = 0xD1E8DC, KEY_D = 0xD1EC14;
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x22BC100)); }

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_FADE = 1, PART_BEAMS = 2, PART_BOLTS = 3, PART_SPARKS = 4, PART_GLOW = 5, PART_WAVE = 6, PART_SPLASHES = 7 };
	struct BlasterParams
	{
		uint32_t sound;                                     // at the director's sound tick
		// the director's ticks: the charge, the sound, the screen fade, the sparks, the glow, the bolts, the shock wave, the
		// splashes, the damage, the end
		struct { int16_t charge, sound, fade, sparks, glow, bolts, wave, splashes, damage, end; } timing;
		struct { int16_t up, down; } flash;                  // the screen flash: rising until tick up, falling from tick down
		struct { int16_t point, out, pull; } mouth;          // the caster's point, moved out along the aim; the target point pulled back
		struct { int16_t point_a, point_b, ticks; } charge;  // the beam between two points of the caster, its spawner's ticks
		struct { int16_t length, width; } beams;             // a beam's flipbook scale
		struct { int16_t size, size_rand; } beam_rings;      // a beam's ring
		struct { int16_t ticks, end, life, life_rand, spin, size, size_rand, fade, frames; } bolts; // one on each odd tick
		struct { int16_t ticks, size, size_rand, spread, target_size, target_size_rand, target_spread; } sparks;
		struct { int16_t size, ticks, fade; } glow;          // at the mouth
		struct { int16_t size, ticks, fade, shake, turn; } wave; // the shock wave at the target, shaking it
		struct { int16_t ticks, count, size, size_rand, spread, depth, speed, speed_rand; } splashes;
		struct { int16_t up, hold, down, level; } fade;     // the red screen fade
		struct { uint32_t beam, spark, glow, splash; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t beam_ring, bolt, wave; } models;  // the prim models drawn (meshes)
		struct { uint32_t a, b, c, d; } keys;               // the bolt's key vertex sets (data)
		struct { uint32_t texture_name, camera; } res;      // its resources (data): texture file name, camera track
	};
	static const BlasterParams GAME = { SOUND_Charge, { 7, 7, 0x27, 0x27, 0x27, 0x28, 0x28, 0x28, 0x3C, 0x41 }, { 0x10, 0x39 },
		{ 0x15, 300, 400 }, { 0x31, 0x32, 0x28 }, { 0x680, 0x400 }, { 0x900, 0x300 },
		{ 0xD, 0x1E, 10, 10, 0x200, 0xB00, 0xA00, 4, 5 }, { 0x11, 0x500, 0x200, 0xA0, 0x700, 0x500, 0x104 }, { 0xE00, 0x16, 0xE },
		{ 0x1600, 0x18, 0x10, 40, 0x800 }, { 0xF, 4, 0x900, 0x800, 0xC8, 0x64, 0xC8, 0x104 }, { 0, 1, 0xE, 0xFF },
		{ SEQ_Beam, SEQ_Spark, SEQ_Glow, SEQ_Splash }, { MODEL_BeamRing, MODEL_Bolt, MODEL_Wave }, { KEY_A, KEY_B, KEY_C, KEY_D },
		{ TEXTURE_NAME, CAMERA } };
	static BlasterParams live = GAME;
	static const BlasterParams &PG() { return live; }
	// the vertices of the bolt's key k (0..3) in use
	inline uint32_t Key(int k)
	{
		static const uint32_t GAME_KEYS[4] = { KEY_A, KEY_B, KEY_C, KEY_D };
		const uint32_t set[4] = { PG().keys.a, PG().keys.b, PG().keys.c, PG().keys.d };
		return fxp::vertices(set[k], GAME_KEYS[k]) + 8;
	}
	// the vertices of the bolt's mesh frame f (of 20: each key, then its 4 in-betweens in the module)
	inline uint32_t BoltVertices(int32_t f)
	{
		f %= 20;
		return f % 5 ? own::R(*(const uint32_t *)(BOLT_MESHES + 4 * f)) : Key(f / 5);
	}

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }                     // Math_SqrtInt
	inline void ScaleMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); } // scale3DMatrix
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b = a x b (3x3)
	inline int32_t SpawnPosition(uint8_t *entity, int32_t point, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, point, flag, out); }
	inline void ScreenFadeColour(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x571250)(r, g, b); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // MAC = L * V0 + BK
	inline void GteSetBKFromMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = matrix translation
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }               // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v

	// a particle (0x18 bytes): +0 owner mask, +4 frame, +6 size, +8 point (4 words), +0x10 velocity (3 words, splashes)
	enum { P_MASK = 0, P_FRAME = 4, P_SIZE = 6, P_POS = 8, P_VEL = 0x10 };
}
}

#ifdef FF8_FX_HELD
#include "mag186_blaster_held.h"
#endif

namespace ff8fx
{
namespace blaster186
{
	static uint32_t NewTask(uint32_t f)
	{
		const uint32_t t = P(AddTaskToQueue(EffectQueue(), own::Fn(f)));
		S16(t, 0xC) = 0;
		return t;
	}

	static uint32_t FreeParticle()
	{
		for (uint32_t p = own::R(PARTS); p < own::R(PARTS_END); p += 0x18)
			if (!U32(p, P_MASK)) return p;
		return 0;
	}

	// ------------------------------------------------------------------
	// Bolt meshes (0x5C0110): out = a + (b - a) x t (0x66 vertices of 8 bytes, the pad word kept)
	// ------------------------------------------------------------------
	static void MorphMesh(uint32_t a, uint32_t b, uint32_t out, int32_t t)
	{
		for (int i = 0; i < 0x66; i++, a += 8, b += 8, out += 8)
			for (int j = 0; j < 3; j++)
				S16(out, 2 * j) = (int16_t)((mul32(S16(b, 2 * j) - S16(a, 2 * j), t) >> 12) + S16(a, 2 * j));
	}

	// ------------------------------------------------------------------
	// Beam (0x5BFEC0) and its ring (0x5C0040)
	// ------------------------------------------------------------------
	static void BeamDraw(uint32_t n)
	{
		const int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m, r;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = S16(own::R(BEAM), 0);
		m.t[1] = S16(own::R(BEAM), 2);
		m.t[2] = S16(own::R(BEAM), 4);
		const int32_t sc[3] = { mul32(S16(n, 0x1C), S16(own::R(BEAM), 6)) / 820, S16(n, 0x20), 0 };
		ScaleMatrix(&m, sc);
		const int32_t from[3] = { -0x1000, 0, 0 };
		int32_t axis[3];
		const int32_t a = RotationBetweenVectors(from, (const int32_t *)own::R(BEAMV), axis);
		BuildAxisAngleRotationMatrix(a, &r, axis);
		GteMatrixMultiply(&r, &m);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0xB4));
		U16(h, 4) = (uint16_t)(U8(n, 0xC) & 3);
		U32(h, 0) = fxp::sequence(PG().seqs.beam, SEQ_Beam);
		U16(h, 0x24) = 8;
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl BeamTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_BEAM, n);)
		FX_PART(PART_BEAMS);
		BeamDraw(n);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= S16(n, 0xE) ? TASK_END : 0;
	}

	// fade of the second half: (0x1000 / half) x (tick - half)
	static int32_t RingFade(uint32_t n, int32_t c) { return (0x1000 / S16(n, 0x22)) * (c - S16(n, 0x20)); }

	static void BeamRingDraw(uint32_t n, int32_t fade)
	{
		const int16_t r = S16(n, 0x1C);
		TransformCameraByShadowRotation((const void *)own::R(BEAM), (int32_t)(uint16_t)r, -(r >> 3));
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = fxp::model(PG().models.beam_ring, MODEL_BeamRing);
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x33;
		if (S16(n, 0xC) >= S16(n, 0x20))
		{
			U32(h, 0x1C) = 0xF3;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl BeamRingTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_RING, n);)
		FX_PART(PART_BEAMS);
		BeamRingDraw(n, S16(n, 0xC) >= S16(n, 0x20) ? RingFade(n, S16(n, 0xC)) : 0);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= S16(n, 0xE) ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Charge (0x5BFCF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ChargeTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		int16_t a[4];
		SpawnPosition(Caster(), PG().charge.point_a, 0x1000, a);
		SpawnPosition(Caster(), PG().charge.point_b, 0x1000, (int16_t *)own::R(BEAM));
		S32(own::R(BEAMV), 0) = a[0] - S16(own::R(BEAM), 0);
		S32(own::R(BEAMV), 4) = a[1] - S16(own::R(BEAM), 2);
		S32(own::R(BEAMV), 8) = a[2] - S16(own::R(BEAM), 4);
		S16(own::R(BEAM), 6) = (int16_t)Sqrt(Normalize((const int32_t *)own::R(BEAMV), (int32_t *)own::R(BEAMV)));
		S16(own::R(BEAM), 0) = (int16_t)((S16(own::R(BEAM), 0) + a[0]) / 2);
		S16(own::R(BEAM), 2) = (int16_t)((S16(own::R(BEAM), 2) + a[1]) / 2);
		S16(own::R(BEAM), 4) = (int16_t)((S16(own::R(BEAM), 4) + a[2]) / 2);
		if (DrawOnly()) return 0;
		const int16_t idx = S16(n, 0xE);
		const int32_t end = S32(BOUNDS + 4, 4 * idx), start = S32(BOUNDS, 4 * idx);
		const int32_t c = S16(n, 0xC);
		if (c < end && c >= start && c == start)
		{
			if ((idx & 1) == 1)
			{
				const int32_t len = end - start;
				uint32_t t = NewTask(ORIG_Beam);
				S16(t, 0xE) = (int16_t)len;
				S16(t, 0x1C) = PG().beams.length;
				S16(t, 0x20) = PG().beams.width;
				t = NewTask(ORIG_BeamRing);
				S16(t, 0xE) = (int16_t)len;
				S16(t, 0x1C) = (int16_t)(CrtRand() % PG().beam_rings.size_rand + PG().beam_rings.size);
				S16(t, 0x20) = S16(t, 0x22) = (int16_t)(S16(t, 0xE) / 2);
			}
			S16(n, 0xE) = (int16_t)(S16(n, 0xE) + 1);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= PG().charge.ticks ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bolts (0x5C0190 / 0x5C0280)
	// ------------------------------------------------------------------
	static int32_t BoltFade(uint32_t n, int32_t c) { return (0x1000 / (S16(n, 0x22) > 0 ? S16(n, 0x22) : 1)) * (c - S16(n, 0x20)); }

	static void BoltDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, 0, S16(n, 0x18) };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = S16(own::R(MOUTH), 0);
		m.t[1] = S16(own::R(MOUTH), 2);
		m.t[2] = S16(own::R(MOUTH), 4);
		const int32_t sc[3] = { S16(n, 0x1C), S16(n, 0x1C), shl32(S16(own::R(MOUTH), 6), 12) / 5000 };
		ScaleMatrix(&m, sc);
		GteMatrixMultiply(Aim(), &m);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 8) = 0;
		U32(h, 0) = fxp::model(PG().models.bolt, MODEL_Bolt);
		U32(h, 0x1C) = 0x2033;
		U32(h, 4) = BoltVertices(S16(n, 0x1A));
		if (S16(n, 0xC) >= S16(n, 0x20))
		{
			U32(h, 0x1C) = 0x20F3;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl BoltTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_BOLT, n);)
		FX_PART(PART_BOLTS);
		BoltDraw(n, S16(n, 0xC) >= S16(n, 0x20) ? BoltFade(n, S16(n, 0xC)) : 0);
		if (DrawOnly()) return 0;
		S16(n, 0x1A) = (int16_t)(S16(n, 0x1A) + 1);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= S16(n, 0xE) ? TASK_END : 0;
	}

	static uint32_t __cdecl BoltsTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = P(tn);
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c < PG().bolts.ticks && (c & 1))
		{
			const uint32_t t = NewTask(ORIG_Bolt);
			S16(t, 0xE) = (int16_t)(CrtRand() % PG().bolts.life_rand + PG().bolts.life);
			S16(t, 0x18) = (int16_t)((U8(n, 0xE) & 1) ? -PG().bolts.spin : PG().bolts.spin);
			S16(t, 0x1A) = (int16_t)(CrtRand() % PG().bolts.frames);
			S16(t, 0x22) = PG().bolts.fade;
			S16(t, 0x1C) = (int16_t)(CrtRand() % PG().bolts.size_rand + PG().bolts.size);
			S16(n, 0xE) = (int16_t)(S16(n, 0xE) + 1);
			S16(t, 0x1E) = (int16_t)(S16(t, 0x1C) / 4);
			S16(t, 0x20) = (int16_t)(S16(t, 0xE) - PG().bolts.fade);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= PG().bolts.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparks (0x5C0410)
	// ------------------------------------------------------------------
	static void SparkDraw(uint32_t h, uint32_t p)
	{
		U16(h, 4) = U16(p, P_FRAME);
		const int16_t size = S16(p, P_SIZE);
		TransformCameraByShadowRotation((const void *)(p + P_POS), (int32_t)(uint16_t)size, -(size >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t __cdecl SparksTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_parts(REC_SPARKS, n);)
		FX_PART(PART_SPARKS);
		const uint32_t h = P(FieldAlloc(0xB4));
		U32(h, 0) = fxp::sequence(PG().seqs.spark, SEQ_Spark);
		U16(h, 0x24) = 0;
		int alive = 0;
		for (uint32_t p = own::R(PARTS); p < own::R(PARTS_END); p += 0x18)
		{
			if (!(U8(p, P_MASK) & 1)) continue;
			// 30 fps layer: see mag186_blaster_held.inc
			FX_HELD(held_note_particle(p);)
			SparkDraw(h, p);
			if (DrawOnly()) continue;
			S16(p, P_FRAME) = (int16_t)(S16(p, P_FRAME) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(p, P_MASK) = 0;
				continue;
			}
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c < PG().sparks.ticks)
		{
			uint32_t p = FreeParticle();
			if (p)
			{
				U32(p, P_MASK) = 1;
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % PG().sparks.size_rand + PG().sparks.size);
				U32(p, P_POS) = U32(own::R(MOUTH), 0);
				U32(p, P_POS + 4) = U32(own::R(MOUTH), 4);
				S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % (2 * PG().sparks.spread) - PG().sparks.spread);
				S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + CrtRand() % (2 * PG().sparks.spread) - PG().sparks.spread);
				S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % (2 * PG().sparks.spread) - PG().sparks.spread);
				p = FreeParticle();
				if (p)
				{
					U32(p, P_MASK) = 1;
					S16(p, P_FRAME) = 0;
					S16(p, P_SIZE) = (int16_t)(CrtRand() % PG().sparks.target_size_rand + PG().sparks.target_size);
					U32(p, P_POS) = U32(own::R(TGT), 0);
					U32(p, P_POS + 4) = U32(own::R(TGT), 4);
					S16(p, P_POS) = (int16_t)(S16(p, P_POS) + CrtRand() % (2 * PG().sparks.target_spread) - PG().sparks.target_spread);
					S16(p, P_POS + 2) = (int16_t)(S16(p, P_POS + 2) + CrtRand() % (2 * PG().sparks.target_spread) - PG().sparks.target_spread);
					S16(p, P_POS + 4) = (int16_t)(S16(p, P_POS + 4) + CrtRand() % (2 * PG().sparks.target_spread) - PG().sparks.target_spread);
				}
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Glow (0x5C0690)
	// ------------------------------------------------------------------
	static int32_t GlowColour(int32_t c) { return shl32(PG().glow.ticks - c, 4); }

	static void GlowDraw(uint32_t n, int32_t colour)
	{
		const int16_t r = S16(n, 0x1C);
		TransformCameraByShadowRotation((const void *)own::R(MOUTH), (int32_t)(uint16_t)r, -(r >> 4));
		const uint32_t h = P(FieldAlloc(0xB4));
		const int16_t c = S16(n, 0xC);
		U32(h, 0) = fxp::sequence(PG().seqs.glow, SEQ_Glow);
		U16(h, 4) = (uint16_t)(c & 0xF);
		U16(h, 0x24) = 0;
		if (c >= PG().glow.fade)
		{
			U16(h, 0x24) = 4;
			U8(h, 0x1E) = U8(h, 0x1D) = U8(h, 0x1C) = (uint8_t)colour;
		}
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_GLOW, n);)
		FX_PART(PART_GLOW);
		GlowDraw(n, GlowColour(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= PG().glow.ticks ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Shock wave (0x5C0750)
	// ------------------------------------------------------------------
	static int32_t WaveFade(int32_t c) { return shl32(c - PG().wave.fade, 9); }

	static void WaveDraw(uint32_t n, int32_t fade)
	{
		const int16_t ang[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = S16(own::R(TGT), 0);
		m.t[1] = S16(own::R(TGT), 2);
		const int32_t s = S16(n, 0x1C);
		const int32_t sc[3] = { s, s, s };
		m.t[2] = S16(own::R(TGT), 4);
		ScaleMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		const int16_t c = S16(n, 0xC);
		U32(h, 0) = fxp::model(PG().models.wave, MODEL_Wave);
		U32(h, 8) = 0;
		U32(h, 0x10) = (uint32_t)(~((int32_t)c % 8) & 1);
		U32(h, 0x1C) = 0x1433;
		U32(h, 0x18) = U32(WAVE_TEXTURES, 4 * (c & 7));
		if (c >= PG().wave.fade)
		{
			U32(h, 0x1C) = 0x14F3;
			S32(h, 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl WaveTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_WAVE, n);)
		FX_PART(PART_WAVE);
		WaveDraw(n, WaveFade(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		int32_t d = ComputeSin(shl32(c, 11) / (PG().wave.ticks > 0 ? PG().wave.ticks : 1));
		d = (d * PG().wave.shake) >> 12;
		int16_t &w = *(int16_t *)(Target() + 0x20);
		const int16_t base = S16(n, 0x14);
		w = base;
		if (!(c & 1)) d = -d;
		w = (int16_t)(d + base);
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) < PG().wave.ticks) return 0;
		w = base;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Splashes (0x5C0900)
	// ------------------------------------------------------------------
	// h: the sequence header (0xB4), w: work (0x48): +8 camera x aim at the target point, +0x28 direction, +0x38 point
	static void SplashSetup(uint32_t h, uint32_t w)
	{
		U32(h, 0) = fxp::sequence(PG().seqs.splash, SEQ_Splash);
		U16(h, 0x24) = 0;
		memcpy((void *)(w + 8), (const void *)Aim(), 0x20);
		S32(w, 0x1C) = S16(own::R(TGT), 0);
		S32(w, 0x20) = S16(own::R(TGT), 2);
		S32(w, 0x24) = S16(own::R(TGT), 4);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(w + 8), (Mat4x3 *)(w + 8));
		GteSetLightMatrix((const void *)(w + 8));
		GteSetBKFromMatrix((const void *)(w + 8));
	}

	static void SplashDraw(uint32_t h, uint32_t w, uint32_t p)
	{
		GteLoadV0((const void *)(p + P_POS));
		GteMVMVA_LightV0Bk();
		GteSetRotScale(S16(p, P_SIZE));
		U16(h, 4) = U16(p, P_FRAME);
		GteReadMAC123((int32_t *)(w + 0x38));
		NormalizeVector((const int32_t *)(w + 0x38), (int32_t *)(w + 0x28));
		const int32_t k = -(S16(p, P_SIZE) >> 3);
		S32(w, 0x38) += mul32(k, S32(w, 0x28)) >> 12;
		S32(w, 0x3C) += mul32(S32(w, 0x2C), k) >> 12;
		S32(w, 0x40) += mul32(S32(w, 0x30), k) >> 12;
		GteSetTransFromVec32((const int32_t *)(w + 0x38));
		FrameCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, FrameCursor());
	}

	static uint32_t __cdecl SplashesTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_parts(REC_SPLASHES, n);)
		FX_PART(PART_SPLASHES);
		const uint32_t h = P(FieldAlloc(0xB4));
		uint32_t w = P(FieldAlloc(0x48));
		int alive = 0;
		SplashSetup(h, w);
		for (uint32_t p = own::R(PARTS); p < own::R(PARTS_END); p += 0x18)
		{
			if (!(U8(p, P_MASK) & 2)) continue;
			// 30 fps layer: see mag186_blaster_held.inc
			FX_HELD(held_note_particle(p);)
			SplashDraw(h, w, p);
			if (DrawOnly()) continue;
			S16(p, P_FRAME) = (int16_t)(S16(p, P_FRAME) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(p, P_MASK) = 0;
				continue;
			}
			for (int j = 0; j < 3; j++)
			{
				S16(p, P_POS + 2 * j) = (int16_t)(S16(p, P_POS + 2 * j) + S16(p, P_VEL + 2 * j));
				S16(p, P_VEL + 2 * j) = (int16_t)(S16(p, P_VEL + 2 * j) - (int16_t)(S16(p, P_VEL + 2 * j) >> 2));
			}
			alive++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		w = P(FieldAlloc(0x48));
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c < PG().splashes.ticks)
		{
			for (int k = 0; k < PG().splashes.count; k++)
			{
				const uint32_t p = FreeParticle();
				if (!p) break;
				U32(p, P_MASK) = 2;
				S16(p, P_FRAME) = 0;
				S16(p, P_SIZE) = (int16_t)(CrtRand() % PG().splashes.size_rand + PG().splashes.size);
				S16(p, P_POS) = (int16_t)(CrtRand() % (2 * PG().splashes.spread) - PG().splashes.spread);
				S16(p, P_POS + 2) = (int16_t)(CrtRand() % (2 * PG().splashes.spread) - PG().splashes.spread);
				S16(p, P_POS + 4) = (int16_t)-(CrtRand() % PG().splashes.depth);
				S32(w, 0x28) = CrtRand() % 0x1000 - 0x800;
				S32(w, 0x2C) = CrtRand() % 0x1000 - 0x800;
				S32(w, 0x30) = -(CrtRand() % 0x500);
				NormalizeVector((const int32_t *)(w + 0x28), (int32_t *)(w + 0x28));
				const int32_t sp = CrtRand() % PG().splashes.speed_rand + PG().splashes.speed;
				S16(p, P_VEL) = (int16_t)(mul32(sp, S32(w, 0x28)) >> 12);
				S16(p, P_VEL + 2) = (int16_t)(mul32(S32(w, 0x2C), sp) >> 12);
				S16(p, P_VEL + 4) = (int16_t)(mul32(S32(w, 0x30), sp) >> 12);
			}
		}
		FieldFree(0x48);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 4 && !alive ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen fade (0x5BFC50)
	// ------------------------------------------------------------------
	// the fade level at counter c: up / hold / down by the nibbles of +0xE to +0x1C; -1 once past
	static int32_t FadeLevel(uint32_t n, int32_t c)
	{
		const int32_t e = S16(n, 0xE), top = S16(n, 0x1C);
		const int32_t up = e & 0xF, hold = (e >> 4) & 0xF, down = (e >> 8) & 0xF;
		if (c < up) return top / up * c;
		if ((c -= up) < hold) return top;
		if ((c -= hold) < down) return top - top / down * c;
		return -1;
	}

	// the fade tile (0x571250: a full-screen tile in the frame arena, colour (v, v / 4, v / 4))
	static void FadeDraw(int32_t v) { ScreenFadeColour(v, v >> 2, v >> 2); }

	static uint32_t __cdecl FadeTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		const int32_t v = FadeLevel(n, S16(n, 0xC));
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (v < 0) return TASK_END;
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_node(REC_FADE, n);)
		FX_PART(PART_FADE);
		FadeDraw(v);
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5BF7B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		SpawnPosition(Caster(), PG().mouth.point, 0x1000, (int16_t *)own::R(MOUTH));
		if (S16(n, 0xC) == 0) GetDefaultEffectPosition(Target(), (int16_t *)own::R(TGT));
		S32(own::R(AIMV), 0) = S16(own::R(TGT), 0) - S16(own::R(MOUTH), 0);
		S32(own::R(AIMV), 4) = 0;
		S32(own::R(AIMV), 8) = S16(own::R(TGT), 4) - S16(own::R(MOUTH), 4);
		int32_t flat[3], axis[3];
		Mat4x3 m;
		NormalizeVector((const int32_t *)own::R(AIMV), flat);
		int32_t a = RotationBetweenVectors((const int32_t *)AIM_FROM, flat, axis);
		BuildAxisAngleRotationMatrix(a, Aim(), axis);
		S32(own::R(AIMV), 4) = S16(own::R(TGT), 2) - S16(own::R(MOUTH), 2);
		S16(own::R(MOUTH), 6) = (int16_t)Sqrt(Normalize((const int32_t *)own::R(AIMV), (int32_t *)own::R(AIMV)));
		a = RotationBetweenVectors(flat, (const int32_t *)own::R(AIMV), axis);
		BuildAxisAngleRotationMatrix(a, &m, axis);
		GteMatrixMultiply(&m, Aim());
		const int32_t dx = S32(own::R(AIMV), 0), dz = S32(own::R(AIMV), 8);
		const int16_t c = S16(n, 0xC);
		const int32_t out = PG().mouth.out, pull = -PG().mouth.pull;
		S16(own::R(MOUTH), 6) = (int16_t)(S16(own::R(MOUTH), 6) - out);
		S16(own::R(MOUTH), 0) = (int16_t)(S16(own::R(MOUTH), 0) + (mul32(dx, out) >> 12));
		const int32_t dy = S32(own::R(AIMV), 4);
		S16(own::R(MOUTH), 2) = (int16_t)(S16(own::R(MOUTH), 2) + (mul32(dy, out) >> 12));
		S16(own::R(MOUTH), 4) = (int16_t)(S16(own::R(MOUTH), 4) + (mul32(dz, out) >> 12));
		if (c == 0)
		{
			S16(own::R(TGT), 0) = (int16_t)(S16(own::R(TGT), 0) + (mul32(dx, pull) >> 12));
			S16(own::R(TGT), 2) = (int16_t)(S16(own::R(TGT), 2) + (mul32(dy, pull) >> 12));
			S16(own::R(TGT), 4) = (int16_t)(S16(own::R(TGT), 4) + (mul32(dz, pull) >> 12));
		}
		if (DrawOnly()) return 0;
		if (c == 0)
		{
			// (the game's keys: 0xD1E274, 0xD1E5AC, 0xD1E8E4, 0xD1EC1C, the first again)
			const uint32_t keys[5] = { Key(0), Key(1), Key(2), Key(3), Key(0) };
			for (int k = 0; k < 4; k++)
				for (int i = 0; i < 4; i++)
					MorphMesh(keys[k], keys[k + 1], own::R(U32(0xD20814 + 0x14 * k, 4 * i)), 0x333 * (i + 1));
		}
		if (S16(n, 0xC) == PG().timing.charge)
		{
			const uint32_t t = NewTask(ORIG_Charge);
			S16(t, 0xE) = 1;
		}
		if (S16(n, 0xC) == PG().timing.fade)
		{
			const uint32_t t = NewTask(ORIG_Fade);
			S16(t, 0xE) = (int16_t)((PG().fade.up & 0xF) | (PG().fade.hold & 0xF) << 4 | (PG().fade.down & 0xF) << 8);
			S16(t, 0x1C) = PG().fade.level;
		}
		if (S16(n, 0xC) == PG().timing.bolts)
		{
			const uint32_t t = NewTask(ORIG_Bolts);
			S16(t, 0xE) = 1;
		}
		if (S16(n, 0xC) == PG().timing.sparks) NewTask(ORIG_Sparks);
		if (S16(n, 0xC) == PG().timing.glow)
		{
			const uint32_t t = NewTask(ORIG_Glow);
			S16(t, 0x1C) = PG().glow.size;
		}
		if (S16(n, 0xC) == PG().timing.wave)
		{
			const uint32_t t = NewTask(ORIG_Wave);
			S16(t, 0x1C) = PG().wave.size;
			U32(t, 0x10) = *(const uint32_t *)(Target() + 0x1C);
			U32(t, 0x14) = *(const uint32_t *)(Target() + 0x20);
			S16(t, 0x18) = (int16_t)(*(const int16_t *)(Caster() + 0xE) + PG().wave.turn);
		}
		if (S16(n, 0xC) == PG().timing.splashes) NewTask(ORIG_Splashes);
		if (S16(n, 0xC) == PG().timing.damage) ApplyActionResultToTarget((uint8_t *)U32(U32(Ctx(), 4), 8));
		const int16_t t = S16(n, 0xC);
		if (t <= PG().flash.up) SetScreenFlash((uint32_t)shl32(t, 7), 0);
		else if (t >= PG().flash.down) SetScreenFlash((uint32_t)shl32(PG().timing.end - t, 8), 0);
		if (S16(n, 0xC) == PG().timing.sound) BdPlaySE((const void *)fxp::sound(PG().sound, SOUND_Charge), 0, 0x80);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) <= PG().timing.end) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Root (0x5C0C80) and texture restore (0x5C0CF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0xC) & 1) ? TexBase() + 0xC000 : TexBase();
		const int running = ExecuteTaskQueue(EffectQueue());
		if (!running)
		{
			const uint32_t w = P(AddTaskToQueue(RootQueue(), own::Fn(ORIG_TexRestore)));
			S16(w, 0xC) = 0;
			S16(w, 0xE) = 0;
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *tn)
	{
		const uint32_t n = P(tn);
		if (S16(n, 0xC) == 1) TextureRestoreTask((void *)TexBase(), (void *)(n + 0xE));
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x5BF6D0) and setup (0x5BF6F0): the game runs its own; Fujin's programs these (its memory may be a
	// copy's): the texture, the magic buffer, the cast, its first target's and its caster's slots, the root and the
	// director, the particles freed, the camera and the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = layouts::MagicBuffer();
		Ctx() = (uint32_t)ctx;
		TargetSlot() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(RootQueue(), (void *)own::R(0x22B7608), 0x10, 2);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_Root)) + 0xC) = 0;
		InitTaskQueuePool(EffectQueue(), (void *)own::R(0x22B9940), 0x24, 100);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Director)) + 0xC) = 0;
		for (uint32_t a = own::R(PARTS); a < own::R(PARTS_END); a += 0x18) *(int32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA));
		QueueTimUpload(TexFile());
		return RootQueue();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The director's ticks: each part's start, the sound, the damage, the end; the mouth point and the aim (every tick), the screen flash; the bolt's in-between meshes (tick 0); the charge's beams and rings; the texture and the camera" },
			{ PART_FADE, "fade", "Screen fade", "A red full-screen fade: up, held, down (its colour v, v / 4, v / 4)" },
			{ PART_BEAMS, "beams", "Charge beams", "Beams between two of the caster's points, each with a ring fading over its second half, at the boundaries of the exe's interval table" },
			{ PART_BOLTS, "bolts", "Bolts", "Bolt meshes from the mouth to the target, flickering through their key and in-between meshes, turning, fading over their last ticks" },
			{ PART_SPARKS, "sparks", "Sparks", "Still sparks at the mouth and at the target, each playing its flipbook once" },
			{ PART_GLOW, "glow", "Glow", "A glow flipbook at the mouth, dimming at the end" },
			{ PART_WAVE, "wave", "Shock wave", "A shock wave mesh at the target, its texture cycling, fading at the end; the target shaken" },
			{ PART_SPLASHES, "splashes", "Splashes", "Splashes thrown out from the target along the aim, slowing" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The director's ticks" },
			{ "flash", "Screen flash", "Up by 128 a tick until tick up, down from tick down to 0 at the end" },
			{ "mouth", "Mouth", "The point the bolts leave" },
		};
#define BPF(member) (uint32_t)offsetof(BlasterParams, member)
		static const Field FIELDS[] = {
			{ "director", "sound", "Sound", "At the director's sound tick", BPF(sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "timing.charge", "Charge", "The charge (its beams and rings) starts", BPF(timing.charge), fxp::I16, 0, 200 },
			{ "director", "timing.sound", "Sound", "The sound", BPF(timing.sound), fxp::I16, 0, 200 },
			{ "director", "timing.fade", "Screen fade", "The screen fade starts", BPF(timing.fade), fxp::I16, 0, 200 },
			{ "director", "timing.sparks", "Sparks", "The sparks start", BPF(timing.sparks), fxp::I16, 0, 200 },
			{ "director", "timing.glow", "Glow", "The glow starts", BPF(timing.glow), fxp::I16, 0, 200 },
			{ "director", "timing.bolts", "Bolts", "The bolts start", BPF(timing.bolts), fxp::I16, 0, 200 },
			{ "director", "timing.wave", "Shock wave", "The shock wave starts", BPF(timing.wave), fxp::I16, 0, 200 },
			{ "director", "timing.splashes", "Splashes", "The splashes start", BPF(timing.splashes), fxp::I16, 0, 200 },
			{ "director", "timing.damage", "Damage", "The damage to the target", BPF(timing.damage), fxp::I16, 0, 200 },
			{ "director", "timing.end", "End", "The director's last tick (the screen flash back to 0)", BPF(timing.end), fxp::I16, 0, 200 },
			{ "director", "flash.up", "Up until", "The flash at tick x 128 until this tick", BPF(flash.up), fxp::I16, 0, 200 },
			{ "director", "flash.down", "Down from", "From this tick (end - tick) x 256", BPF(flash.down), fxp::I16, 0, 200 },
			{ "director", "mouth.point", "Point", "The caster's effect point of the mouth", BPF(mouth.point), fxp::I16, 0, 0xFF },
			{ "director", "mouth.out", "Out", "The mouth point moved this far along the aim (the bolts this much shorter)", BPF(mouth.out), fxp::I16, -0x2000, 0x2000 },
			{ "director", "mouth.pull", "Target pull", "The target point (the sparks', wave's and splashes') pulled back this far towards the caster", BPF(mouth.pull), fxp::I16, -0x2000, 0x2000 },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", BPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", BPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "fade", "fade.up", "Up", "Ticks up to the level (0: at once)", BPF(fade.up), fxp::I16, 0, 15 },
			{ "fade", "fade.hold", "Hold", "Ticks held", BPF(fade.hold), fxp::I16, 0, 15 },
			{ "fade", "fade.down", "Down", "Ticks down to none", BPF(fade.down), fxp::I16, 0, 15 },
			{ "fade", "fade.level", "Level", "Its red at the top (green and blue a quarter of it)", BPF(fade.level), fxp::I16, 0, 0xFF },
			{ "beams", "charge.point_a", "Point 1", "The caster's effect point the beam ends at (its middle: the beam's point)", BPF(charge.point_a), fxp::I16, 0, 0xFF },
			{ "beams", "charge.point_b", "Point 2", "The other one", BPF(charge.point_b), fxp::I16, 0, 0xFF },
			{ "beams", "charge.ticks", "Ticks", "The charge's ticks (a beam and its ring at each odd interval's start of the exe's table)", BPF(charge.ticks), fxp::I16, 0, 200 },
			{ "beams", "beams.length", "Length", "A beam's length scale (x the points' distance / 820)", BPF(beams.length), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "beams", "beams.width", "Width", "A beam's width", BPF(beams.width), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "beams", "beam_rings.size", "Ring size", "A ring's size: this + a random below the next", BPF(beam_rings.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "beams", "beam_rings.size_rand", "Ring size random", "(at least 1)", BPF(beam_rings.size_rand), fxp::I16, 1, 0x7FFF, fxp::Scale },
			{ "beams", "seqs.beam", "Beam", "The sprite sequence of the beams (a flipbook: an edited one keeps as many frames)", BPF(seqs.beam), fxp::Seq, 0, 0x7FFFFFFF },
			{ "beams", "models.beam_ring", "Ring", "The prim model of the beams' rings (a mesh: an edited one keeps its vertices)", BPF(models.beam_ring), fxp::Model, 0, 0x7FFFFFFF },
			{ "bolts", "bolts.ticks", "Ticks", "A bolt on each odd tick before this one", BPF(bolts.ticks), fxp::I16, 0, 200 },
			{ "bolts", "bolts.end", "End", "Their spawner's last tick", BPF(bolts.end), fxp::I16, 0, 200 },
			{ "bolts", "bolts.life", "Life", "A bolt's ticks: this + a random below the next", BPF(bolts.life), fxp::I16, 0, 200 },
			{ "bolts", "bolts.life_rand", "Life random", "(at least 1)", BPF(bolts.life_rand), fxp::I16, 1, 200 },
			{ "bolts", "bolts.spin", "Spin", "Its z angle step a tick (either way by turns)", BPF(bolts.spin), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "bolts", "bolts.size", "Size", "Its width: this + a random below the next (its length: the aim's)", BPF(bolts.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "bolts", "bolts.size_rand", "Size random", "(at least 1)", BPF(bolts.size_rand), fxp::I16, 1, 0x7FFF, fxp::Scale },
			{ "bolts", "bolts.fade", "Fade", "Fading out over its last ticks (at least 1)", BPF(bolts.fade), fxp::I16, 1, 200 },
			{ "bolts", "bolts.frames", "First frames", "Its first mesh frame: a random below this (of 20: the keys and their in-betweens; at least 1)", BPF(bolts.frames), fxp::I16, 1, 20 },
			{ "bolts", "models.bolt", "Bolt", "The prim model of the bolts (a mesh of 102 vertices: its vertices are the key sets' and their in-betweens; edit its faces)", BPF(models.bolt), fxp::Model, 0, 0x7FFFFFFF },
			{ "bolts", "keys.a", "Key 1", "The bolt's first key vertices (102 vertices x, y, z after its size and count: an edited one keeps the count)", BPF(keys.a), fxp::Data, 0, 0x7FFFFFFF },
			{ "bolts", "keys.b", "Key 2", "The second key (the in-betweens: 4 between each key and the next, the last back to the first)", BPF(keys.b), fxp::Data, 0, 0x7FFFFFFF },
			{ "bolts", "keys.c", "Key 3", "The third key", BPF(keys.c), fxp::Data, 0, 0x7FFFFFFF },
			{ "bolts", "keys.d", "Key 4", "The fourth key", BPF(keys.d), fxp::Data, 0, 0x7FFFFFFF },
			{ "sparks", "sparks.ticks", "Ticks", "One spark at the mouth and one at the target a tick on its first ticks (100 alive at most, with the splashes)", BPF(sparks.ticks), fxp::I16, 0, 200 },
			{ "sparks", "sparks.size", "Mouth size", "A mouth spark's size: this + a random below the next", BPF(sparks.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "sparks", "sparks.size_rand", "Mouth size random", "(at least 1)", BPF(sparks.size_rand), fxp::I16, 1, 0x7FFF, fxp::Scale },
			{ "sparks", "sparks.spread", "Mouth spread", "Up to this far from the mouth on each axis (at least 1)", BPF(sparks.spread), fxp::I16, 1, 0x3FFF },
			{ "sparks", "sparks.target_size", "Target size", "A target spark's size: this + a random below the next", BPF(sparks.target_size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "sparks", "sparks.target_size_rand", "Target size random", "(at least 1)", BPF(sparks.target_size_rand), fxp::I16, 1, 0x7FFF, fxp::Scale },
			{ "sparks", "sparks.target_spread", "Target spread", "Up to this far from the target point on each axis (at least 1)", BPF(sparks.target_spread), fxp::I16, 1, 0x3FFF },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence of the sparks (a flipbook: an edited one keeps as many frames)", BPF(seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "glow", "glow.size", "Size", "Its size", BPF(glow.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "glow", "glow.ticks", "Ticks", "Its ticks (its frame: the tick's low 4 bits)", BPF(glow.ticks), fxp::I16, 0, 200 },
			{ "glow", "glow.fade", "Dim from", "From this tick drawn at (ticks - tick) x 16", BPF(glow.fade), fxp::I16, 0, 200 },
			{ "glow", "seqs.glow", "Glow", "The sprite sequence of the glow (a flipbook: an edited one keeps as many frames)", BPF(seqs.glow), fxp::Seq, 0, 0x7FFFFFFF },
			{ "wave", "wave.size", "Size", "Its size", BPF(wave.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "wave", "wave.ticks", "Ticks", "Its ticks (the shake: half a sine over them)", BPF(wave.ticks), fxp::I16, 1, 200 },
			{ "wave", "wave.fade", "Fade from", "Fading out by 512 a tick from this tick", BPF(wave.fade), fxp::I16, 0, 200 },
			{ "wave", "wave.shake", "Shake", "The target's z shaken by up to this (either way by turns)", BPF(wave.shake), fxp::I16, -0x1000, 0x1000 },
			{ "wave", "wave.turn", "Turn", "Its y angle: the caster's + this", BPF(wave.turn), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "wave", "models.wave", "Shock wave", "The prim model of the shock wave (a mesh: an edited one keeps its vertices; its texture words from the exe's table)", BPF(models.wave), fxp::Model, 0, 0x7FFFFFFF },
			{ "splashes", "splashes.ticks", "Ticks", "New splashes on its first ticks", BPF(splashes.ticks), fxp::I16, 0, 200 },
			{ "splashes", "splashes.count", "Count", "Splashes a tick", BPF(splashes.count), fxp::I16, 0, 100 },
			{ "splashes", "splashes.size", "Size", "A splash's size: this + a random below the next", BPF(splashes.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "splashes", "splashes.size_rand", "Size random", "(at least 1)", BPF(splashes.size_rand), fxp::I16, 1, 0x7FFF, fxp::Scale },
			{ "splashes", "splashes.spread", "Spread", "Its start up to this far sideways from the target point (at least 1)", BPF(splashes.spread), fxp::I16, 1, 0x3FFF },
			{ "splashes", "splashes.depth", "Depth", "And up to this far back along the aim (at least 1)", BPF(splashes.depth), fxp::I16, 1, 0x3FFF },
			{ "splashes", "splashes.speed", "Speed", "Its speed: this + a random below the next (losing a quarter a tick)", BPF(splashes.speed), fxp::I16, 0, 0x3FFF },
			{ "splashes", "splashes.speed_rand", "Speed random", "(at least 1)", BPF(splashes.speed_rand), fxp::I16, 1, 0x3FFF },
			{ "splashes", "seqs.splash", "Splash", "The sprite sequence of the splashes (a flipbook: an edited one keeps as many frames)", BPF(seqs.splash), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef BPF
	}

	void register_mag186_blaster_ports()
	{
		fxp::register_params({ 186, "Blaster", &live, &GAME, sizeof(BlasterParams),
		                       table::PARTS, (int)(sizeof(table::PARTS) / sizeof(table::PARTS[0])),
		                       table::FIELDS, (int)(sizeof(table::FIELDS) / sizeof(table::FIELDS[0])),
		                       table::GROUPS, (int)(sizeof(table::GROUPS) / sizeof(table::GROUPS[0])) });
		own::note(ORIG_FileLoader, (void *)FileLoader);
		own::note(ORIG_Setup, (void *)Setup);
		own::note(ORIG_Root, (void *)RootTask);
		register_port(ORIG_Root, (void *)RootTask, "186 RootTask", 186);
		own::note(ORIG_TexRestore, (void *)TexRestoreTask);
		register_port(ORIG_TexRestore, (void *)TexRestoreTask, "186 TexRestoreTask", 186);
		own::note(ORIG_Director, (void *)DirectorTask);
		register_port(ORIG_Director, (void *)DirectorTask, "186 DirectorTask", 186);
		own::note(ORIG_Fade, (void *)FadeTask);
		register_port(ORIG_Fade, (void *)FadeTask, "186 FadeTask", 186);
		own::note(ORIG_Charge, (void *)ChargeTask);
		register_port(ORIG_Charge, (void *)ChargeTask, "186 ChargeTask", 186);
		own::note(ORIG_Beam, (void *)BeamTask);
		register_port(ORIG_Beam, (void *)BeamTask, "186 BeamTask", 186);
		own::note(ORIG_BeamRing, (void *)BeamRingTask);
		register_port(ORIG_BeamRing, (void *)BeamRingTask, "186 BeamRingTask", 186);
		own::note(ORIG_Bolts, (void *)BoltsTask);
		register_port(ORIG_Bolts, (void *)BoltsTask, "186 BoltsTask", 186);
		own::note(ORIG_Bolt, (void *)BoltTask);
		register_port(ORIG_Bolt, (void *)BoltTask, "186 BoltTask", 186);
		own::note(ORIG_Sparks, (void *)SparksTask);
		register_port(ORIG_Sparks, (void *)SparksTask, "186 SparksTask", 186);
		own::note(ORIG_Glow, (void *)GlowTask);
		register_port(ORIG_Glow, (void *)GlowTask, "186 GlowTask", 186);
		own::note(ORIG_Wave, (void *)WaveTask);
		register_port(ORIG_Wave, (void *)WaveTask, "186 WaveTask", 186);
		own::note(ORIG_Splashes, (void *)SplashesTask);
		register_port(ORIG_Splashes, (void *)SplashesTask, "186 SplashesTask", 186);
		// 30 fps layer: see mag186_blaster_held.inc
		FX_HELD(register_mag186_held();)
	}
}

	void register_mag186_blaster() { blaster186::register_mag186_blaster_ports(); }
	FX_LAB_MODULE(register_mag186_blaster);
}

#ifdef FF8_FX_HELD
#include "mag186_blaster_held.inc"
#endif
