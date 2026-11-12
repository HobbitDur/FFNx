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

// Effect 157: Breath (enemy attack 37 of kernel.bin, used by the Hexadragon c0m024; MAG_157_*).
// A swirl gathers at the caster's mouth, then three clouds of breath roll toward the target: each
// one a burst, a lit puff, smoke particles, a rolling cloud model pair and a ring pair, with
// splatter sprites on the target.
//
// Structure (setup MAG_157_BREATH_Init 0x603E90, file loader 0x603E70 = the texture file named at
// 0xDB4CC8 (mag156.tim, file 0x23ECDF4); the setup keeps the texture-off buffer (0x571B50) in
// 0x23ECE18, the cast context 0x23ECDEC, the first target's slot 0x23EB5F0 and the caster's slot
// 0x23ECDF0, sets up the root queue 0x23EB618 (2 nodes) and the effect queue 0x23EBF90 (0x64 nodes
// of 0x24 bytes) with the director, clears the particle table, starts camera animation 0xDB0828 and
// queues the TIM):
//   RootTask (0x605460) - packets into the texture-off buffer (its first or second 0xC000 bytes on
//     the counter's parity, cursor 0x23ECE1C), runs the effect queue; once it is empty adds the
//     texture restore task to the root queue and ends.
//   TexRestore (0x6054D0) - counter 1 = Battle_RestoreDefaultEffectTexture (done flag +0x0E); ends
//     once it is done.
//   Director (0x603F50) - every tick (also in the draw-only mode) rebuilds the two breath frames:
//     A (0x23ECDB0) = camera o (rotation from (0, 0, -1) onto direction A 0x23ECDD8, at mouth
//     position A 0x23ECDF8), B (0x23EB5C0) likewise from direction B 0x23EB5E0 / position B
//     0x23EBF88. Then, not in the draw-only mode: counter 0 = the target's default effect position
//     (0x23ECDD0), mouth A = the caster's effect position (bone 0xF0) + (0, 0x226, -0x280) in the
//     caster's frame, direction A toward the target (length root 0x23ECDE8); 1 = two swirls and
//     sound 0xDB4CAC; 2 = the breath particles; 0x13 = mouth B (+ (0, 0x384, -0x546)), direction B
//     (length root 0x23EB5F4); 0x14 / 0x20 / 0x2B = one cloud each (cloud 0..2) and at 0x14 sound
//     0xDB4CB0; 0x41 = damage (action 0's targets); ends after counter 0x46.
//   Swirl (0x6043A0) - prim model 0xDB0DBC in frame A turned by +0x18 (+0x1A a tick), scale +0x1C,
//     fading in (counters 0..3) and out (from 0x16); 30 ticks.
//   BreathParticles (0x6044D0) - particle type 1: sprites (sequence 0xDB0B34) on circles in frame A
//     (angle +8 turning by +0xA, radius +0xE, depth +0xC), lit through the GTE light matrix; counters
//     0..0x1E three a tick; ends from counter 4 once none is left.
//   Cloud (0x604780) - cloud position (0x23ECE00 + 8 x cloud) cleared at 0, the burst at 0, the puff
//     at 8 (speed = 11 x length root B / 16 / 4), the puff particles at 9, the cloud model pair and
//     the ring pair at 0xC, the splatter at 0xF; ends at counter 0x16. Draws nothing.
//   Burst (0x6049F0) - prim model 0xDB42C4 in frame B, scale +0x1C shrinking, fading in; 10 ticks.
//   Puff (0x604B30) - sprite sequence 0xDB0AC0 (frame = counter & 3) at the cloud position in frame
//     B; the cloud moves forward by the puff's speed a tick; 5 ticks.
//   PuffParticles (0x604C80) - particle type 2: sprites (sequence 0xDB0CE0) at their position in
//     frame B; counters 0..4 six a tick around the cloud position; ends from counter 4 once none is left.
//   CloudModels (0x604E90) - prim models 0xDB20DC and 0xDB2F54 at the cloud position (frame B, then
//     the camera), rolling, growing, fading from counter 8; the cloud moves forward (speed -1/6 a
//     tick); 16 ticks.
//   Rings (0x6050A0) - prim model 0xDB3BDC twice at the cloud position (frame B, then frame A),
//     turning, growing, fading from counter 8; 18 ticks.
//   Splatter (0x6052A0) - particle type 4: sprites (sequence 0xDB0914) around the target's default
//     effect position; counters 0..4 six a tick; ends from counter 4 once none is left.
// Particle table 0x23EB628: 100 entries of 0x18 bytes (+0 = type bits).
// Module globals: 0x23EB5C0..0x23ECE40 (frame B, direction B, target slot, length root B, root pool
// / queue, particle table, mouth B, effect queue / pool, frame A, target position, direction A,
// length root A, context, caster slot, texture file, mouth A, cloud positions, texture-off buffer,
// packet cursor, sprite frame 0x23ECE20).
// Fujin's described Breath (effect_157.json) runs this code: its memory through own::R() (fx_port.h), its spawns through
// own::Fn(), the setup and the file loader ported below; its parameters (BreathParams: the director's and each cloud's
// ticks, the mouth offsets, each part's sizes, spins, spreads and lives, the two sounds, four flipbooks, five meshes,
// the texture name, the camera track) are set from the program's. FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace breath157
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline Mat4x3 &FrameB() { return var<Mat4x3>(own::R(0x23EB5C0)); }
	inline int32_t *DirB() { return (int32_t *)own::R(0x23EB5E0); }
	inline uint32_t &TargetSlot() { return var<uint32_t>(own::R(0x23EB5F0)); }
	inline int32_t &LenB() { return var<int32_t>(own::R(0x23EB5F4)); }
	inline TaskQueue *RootQueue() { return (TaskQueue *)own::R(0x23EB618); }
	inline int16_t *MouthB() { return (int16_t *)own::R(0x23EBF88); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)own::R(0x23EBF90); }
	inline Mat4x3 &FrameA() { return var<Mat4x3>(own::R(0x23ECDB0)); }
	inline int16_t *TargetPos() { return (int16_t *)own::R(0x23ECDD0); }
	inline int32_t *DirA() { return (int32_t *)own::R(0x23ECDD8); }
	inline int32_t &LenA() { return var<int32_t>(own::R(0x23ECDE8)); }
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x23ECDEC)); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(own::R(0x23ECDF0)); }
	inline int16_t *MouthA() { return (int16_t *)own::R(0x23ECDF8); }
	inline int16_t *CloudPos(int16_t i) { return (int16_t *)(own::R(0x23ECE00) + (uint32_t)(int32_t)i * 8); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x23ECE18)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x23ECE1C)); }
	inline Mat4x3 &SpriteFrame() { return var<Mat4x3>(own::R(0x23ECE20)); }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x23ECDF4)); }          // the effect's file (its TIM)  // rotation never written (0); translation per sprite
	static const uint32_t PARTS = 0x23EB628, PARTS_END = 0x23EBF88;  // 100 x 0x18
	static const int PART_COUNT = 100;
	static const uint32_t ARENA_SIZE = 0xC000;

	static const uint32_t ORIG_RootTask = 0x605460;
	static const uint32_t ORIG_TexRestore = 0x6054D0;
	static const uint32_t ORIG_Director = 0x603F50;
	static const uint32_t ORIG_Swirl = 0x6043A0;
	static const uint32_t ORIG_BreathParticles = 0x6044D0;
	static const uint32_t ORIG_Cloud = 0x604780;
	static const uint32_t ORIG_Burst = 0x6049F0;
	static const uint32_t ORIG_Puff = 0x604B30;
	static const uint32_t ORIG_PuffParticles = 0x604C80;
	static const uint32_t ORIG_CloudModels = 0x604E90;
	static const uint32_t ORIG_Rings = 0x6050A0;
	static const uint32_t ORIG_Splatter = 0x6052A0;
	static const uint32_t REF_DIR = 0xDB4CB8;          // (0, 0, -0x1000)
	static const uint32_t MODEL_Swirl = 0xDB0DBC, MODEL_Burst = 0xDB42C4, MODEL_Cloud1 = 0xDB20DC, MODEL_Cloud2 = 0xDB2F54, MODEL_Ring = 0xDB3BDC;
	static const uint32_t SEQ_Breath = 0xDB0B34, SEQ_Puff = 0xDB0AC0, SEQ_PuffPart = 0xDB0CE0, SEQ_Splatter = 0xDB0914;
	static const void *const SOUND_Start = (const void *)0xDB4CAC;
	static const void *const SOUND_Cloud = (const void *)0xDB4CB0;
	static const uint32_t ORIG_FileLoader = 0x603E70, ORIG_Setup = 0x603E90;
	static const uint32_t TEXTURE_NAME = 0xDB4CC8, CAMERA_ANIM = 0xDB0828;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	struct BreathParams
	{
		struct { uint32_t start, cloud; } sounds;
		struct { int16_t swirl, breath, mouth_b, cloud1, cloud2, cloud3, start_sound, cloud_sound, damage, end; } director;
		struct { int16_t a_y, a_z, b_y, b_z; } mouth;                     // the mouths' offsets in the caster's frame
		struct { int16_t size_a, spin_a_min, spin_a_rand, size_b, spin_b_min, spin_b_rand, fade_in, fade_out, life; } swirl;
		struct { int16_t last_tick, per_tick, size_min, size_rand, turn_min, turn_rand, depth_min, depth_rand, radius_min, radius_rand; } breath;
		struct { int16_t burst, puff, puff_parts, models, splatter, life; } cloud;   // each cloud's ticks
		struct { int16_t size, shrink, fade_in, life; } burst;
		struct { int16_t size, speed, life; } puff;
		struct { int16_t last_tick, per_tick, spread, spread_step, size_min, size_rand; } puff_parts;
		struct { int16_t roll_min, roll_rand, spin_min, spin_rand, size_min, size_rand, outer, fade_from, life; } clouds;
		struct { int16_t spin_min, spin_rand, size, fade_from, life; } rings;
		struct { int16_t last_tick, per_tick, spread, size_min, size_rand; } splatter;
		struct { uint32_t breath, puff, puff_part, splatter; } seqs;      // the sprite sequences drawn (flipbooks)
		struct { uint32_t swirl, burst, cloud1, cloud2, ring; } models;   // the prim models drawn (meshes)
		struct { uint32_t texture_name, camera; } res;                    // its resources (data): texture file name, camera track
	};
	static const BreathParams GAME = { { 0xDB4CAC, 0xDB4CB0 },
		{ 1, 2, 0x13, 0x14, 0x20, 0x2B, 1, 0x14, 0x41, 0x46 },
		{ 0x226, -0x280, 0x384, -0x546 },
		{ 0x1400, 0x1E, 0x32, 0xF00, 0x14, 0x3C, 4, 0x16, 0x1E },
		{ 0x1E, 3, 0x300, 0x400, 0xA, 0x32, 10, 0x258, 0x12C, 0x514 },
		{ 0, 8, 9, 0xC, 0xF, 0x16 },
		{ 0x4000, 0x800, 8, 0xA },
		{ 0xD00, 11, 5 },
		{ 4, 6, 0x15E, 30, 0x200, 0x380 },
		{ 0x19, 0x46, 0x19, 0x46, 0x700, 0x600, 0x2A0, 8, 0x10 },
		{ 0x14, 0x32, 0xA00, 8, 0x12 },
		{ 4, 6, 0x4B0, 0x600, 0xB00 },
		{ SEQ_Breath, SEQ_Puff, SEQ_PuffPart, SEQ_Splatter }, { MODEL_Swirl, MODEL_Burst, MODEL_Cloud1, MODEL_Cloud2, MODEL_Ring },
		{ TEXTURE_NAME, CAMERA_ANIM } };
	static BreathParams live = GAME;
	static const BreathParams &P() { return live; }
	// the parts (FX_PART tags: what draws now, in the lab)
	enum { PART_SWIRL = 1, PART_BREATH, PART_BURST, PART_PUFF, PART_CLOUDS, PART_RINGS, PART_SPLATTER };

	// ---- engine functions (original addresses)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
	inline int32_t NormalizeVectorLen(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t ComputeSin(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D130)(a); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
	inline void GteSetRotDiag(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DDD0)(s); }     // MAG_148_sub_64DDD0: R11 = R22 = R33 = s
	inline void GteTransFromIR() { fn<void (__cdecl *)()>(0x64DE90)(); }                     // MAG_148_sub_64DE90: TR = IR1..3
	inline void RestoreDefaultEffectTexture(uint32_t tex, void *done) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(tex, done); }

	// ------------------------------------------------------------------
	// Node / table layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 2 nodes of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t done;      // +0x0E texture restore: done flag
	};
	struct EffectNode // pool of 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t w0E;       // +0x0E cloud index (director: next cloud)
		int16_t w10, w12, w14, w16, w18, w1A, w1C, w1E, w20, w22; // task values (see each task)
	};
	struct Part // 0x18 bytes
	{
		uint32_t type;     // +0x00 bit 0 breath, bit 1 puff particle, bit 2 splatter (0 = free)
		int16_t frame;     // +0x04
		int16_t size;      // +0x06
		int16_t v[8];      // +0x08 breath: angle, turn, depth, radius; others: position (4 words)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "Breath nodes");
	static_assert(sizeof(Part) == 0x18, "Breath particle");

	inline Part *Parts() { return (Part *)own::R(PARTS); }
	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }
	inline uint8_t *Target() { return Entity((int)TargetSlot()); }

	static int FreePart()
	{
		int i = 0;
		for (uint32_t a = own::R(PARTS); a < own::R(PARTS_END); a += sizeof(Part), i++)
			if (*(uint32_t *)a == 0) return i < PART_COUNT ? i : -1;
		return -1;
	}

	// ------------------------------------------------------------------
	// Draws (each one task's draw, from its node)
	// ------------------------------------------------------------------
	static uint8_t *ModelHeader(uint32_t model, uint32_t flags)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(int32_t *)(h + 8) = 0;
		*(int32_t *)(h + 0x1C) = (int32_t)flags;
		return h;
	}
	static void Render(uint8_t *h)
	{
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}
	// rotation from angles, scale s, no translation, composed with the frame f; loaded into the GTE
	static void ModelMatrix(Mat4x3 &m, const int16_t ang[3], int32_t s, const Mat4x3 *f)
	{
		ComposeZYXRotationMatrix(ang, &m);
		int32_t sc[3];
		sc[2] = s;
		sc[1] = s;
		sc[0] = s;
		m.t[2] = 0;
		m.t[1] = 0;
		m.t[0] = 0;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(f, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// Swirl (0x6043A0): fading in (counters 0..3) and out (from 0x16)
	static void SwirlMatrix(const EffectNode *t)
	{
		int16_t ang[3];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = t->w18;
		Mat4x3 m;
		ModelMatrix(m, ang, t->w1C, &FrameA());
	}
	static uint8_t *SwirlHeader(int16_t c)
	{
		uint8_t *h = ModelHeader(fxp::model(P().models.swirl, MODEL_Swirl), 0x33);
		if (c < P().swirl.fade_in)
		{
			*(int32_t *)(h + 0xC) = (P().swirl.fade_in - (int32_t)c) << 10;
			*(int32_t *)(h + 0x1C) = 0xF3;
		}
		else if (c >= P().swirl.fade_out)
		{
			*(int32_t *)(h + 0xC) = ((int32_t)c - P().swirl.fade_out) << 9;
			*(int32_t *)(h + 0x1C) = 0xF3;
		}
		return h;
	}
	static void SwirlDraw(const EffectNode *t)
	{
		SwirlMatrix(t);
		Render(SwirlHeader(t->counter));
		FieldFree(0x58);
	}

	// Burst (0x6049F0): fading in (counters 0..7)
	static void BurstMatrix(const EffectNode *t)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m;
		ModelMatrix(m, ang, t->w1C, &FrameB());
	}
	static uint8_t *BurstHeader(int16_t c)
	{
		uint8_t *h = ModelHeader(fxp::model(P().models.burst, MODEL_Burst), 0x33);
		if (c < P().burst.fade_in)
		{
			*(int32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = (P().burst.fade_in - (int32_t)c) << 9;
		}
		return h;
	}
	static void BurstDraw(const EffectNode *t)
	{
		BurstMatrix(t);
		Render(BurstHeader(t->counter));
		FieldFree(0x58);
	}

	// CloudModels (0x604E90): model 1 in frame B (fading from counter 8), then model 2 (scale + 0x2A0)
	// by the camera with the translation of model 1 (the header kept after its free)
	static void CloudModel1Matrix(const EffectNode *t, Mat4x3 &m)
	{
		const int16_t *c = CloudPos(t->w0E);
		int16_t ang[3];
		ang[0] = t->w10;
		ang[1] = 0;
		ang[2] = t->w18;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = c[0];
		m.t[2] = c[2];
		m.t[1] = c[1];
		int32_t sc[3];
		sc[2] = t->w1C;
		sc[1] = t->w1C;
		sc[0] = t->w1C;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&FrameB(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}
	static uint8_t *CloudHeader(int16_t c)
	{
		uint8_t *h = ModelHeader(fxp::model(P().models.cloud1, MODEL_Cloud1), 3);
		if (c >= P().clouds.fade_from)
		{
			*(int32_t *)(h + 0x1C) = 0xC3;
			*(int32_t *)(h + 0xC) = ((int32_t)c - P().clouds.fade_from) << 9;
		}
		return h;
	}
	static void CloudModel2(const EffectNode *t, Mat4x3 &m, uint8_t *h)
	{
		int16_t ang[3];
		ang[0] = 0;
		ang[1] = t->w10;
		ang[2] = 0;
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t s2 = t->w1C + P().clouds.outer;
		int32_t sc[3];
		sc[2] = s2;
		sc[1] = s2;
		sc[0] = s2;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		*(uint32_t *)h = fxp::model(P().models.cloud2, MODEL_Cloud2);
		Render(h);
	}
	static void CloudModelsDraw(const EffectNode *t)
	{
		Mat4x3 m;
		CloudModel1Matrix(t, m);
		uint8_t *h = CloudHeader(t->counter);
		Render(h);
		FieldFree(0x58);
		CloudModel2(t, m, h);
	}

	// Rings (0x6050A0): the ring in frame B (fading from counter 8), then again (x + 0x400) in
	// frame A with the translation of the first (the header kept after its free)
	static void Ring1Matrix(const EffectNode *t, Mat4x3 &m)
	{
		const int16_t *c = CloudPos(t->w0E);
		int16_t ang[3];
		ang[0] = t->w20;
		ang[1] = 0;
		ang[2] = t->w18;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[2] = c[2];
		m.t[0] = c[0];
		m.t[1] = c[1];
		const int32_t s = t->w1C + P().rings.size;
		int32_t sc[3];
		sc[2] = s;
		sc[1] = s;
		sc[0] = s;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&FrameB(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}
	static uint8_t *RingHeader(int16_t c)
	{
		uint8_t *h = ModelHeader(fxp::model(P().models.ring, MODEL_Ring), 0x33);
		if (c >= P().rings.fade_from)
		{
			*(int32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = ((int32_t)c - P().rings.fade_from) * 409;
		}
		return h;
	}
	static void Ring2(const EffectNode *t, Mat4x3 &m, uint8_t *h)
	{
		int16_t ang[3];
		ang[0] = (int16_t)(t->w20 + 0x400);
		ang[1] = 0;
		ang[2] = t->w18;
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t s = t->w1C + P().rings.size;
		int32_t sc[3];
		sc[2] = s;
		sc[1] = s;
		sc[0] = s;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&FrameA(), &m, &m);
		GteSetRotMatrix(&m);
		Render(h);
	}
	static void RingsDraw(const EffectNode *t)
	{
		Mat4x3 m;
		Ring1Matrix(t, m);
		uint8_t *h = RingHeader(t->counter);
		Render(h);
		FieldFree(0x58);
		Ring2(t, m, h);
	}

	// sprite frame of the lit sprites: the frame's light matrix / back colour, the particle at v in
	// it, scale s on the rotation diagonal, the translation pulled toward the camera by s / 8
	static void LitSpriteSetup(const Mat4x3 *frame)
	{
		GteSetRotMatrixCtrl(&SpriteFrame());
		GteSetLightMatrix(frame);
		GteSetBackColorFromTrans(frame);
	}
	static void LitSpriteAt(const int16_t *v, int32_t s)
	{
		GteLoadV0(v);
		GteMVMVA_LightV0Bk();
		GteSetRotDiag(s);
		GteReadMAC123(SpriteFrame().t);
	}
	static void LitSpritePull(int32_t s)
	{
		int32_t n[3];
		NormalizeVector(SpriteFrame().t, n);
		const int32_t k = -(s >> 3);
		SpriteFrame().t[0] += mul32(n[0], k) >> 12;
		SpriteFrame().t[1] += mul32(n[1], k) >> 12;
		SpriteFrame().t[2] += mul32(n[2], k) >> 12;
		GteSetTransVectorCtrl(&SpriteFrame());
	}
	static void SeqDraw(uint8_t *h)
	{
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static uint8_t *SeqHeader(uint32_t seq)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 0x24) = 0;
		return h;
	}

	// one breath particle (0x6044D0's draw)
	static void BreathPartDraw(uint8_t *h, const Part *p)
	{
		int16_t v[4];
		v[0] = (int16_t)(mul32(ComputeCos(p->v[0]), p->v[3]) >> 12);
		v[1] = (int16_t)(mul32(ComputeSin(p->v[0]), p->v[3]) >> 12);
		v[2] = p->v[2];
		LitSpriteAt(v, p->size);
		*(int16_t *)(h + 4) = p->frame;
		LitSpritePull(p->size);
		SeqDraw(h);
	}
	// Puff (0x604B30)
	static void PuffDraw(const EffectNode *t)
	{
		LitSpriteSetup(&FrameB());
		LitSpriteAt(CloudPos(t->w0E), t->w1C);
		LitSpritePull(t->w1C);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(int16_t *)(h + 4) = (int16_t)(t->counter & 3);
		*(uint32_t *)h = fxp::sequence(P().seqs.puff, SEQ_Puff);
		*(int16_t *)(h + 0x24) = 0;
		SeqDraw(h);
		FieldFree(0xB4);
	}
	// one puff particle (0x604C80's draw)
	static void PuffPartDraw(uint8_t *h, const Part *p)
	{
		GteLoadV0(p->v);
		GteMVMVA_LightV0Bk();
		GteSetRotDiag(p->size);
		*(int16_t *)(h + 4) = p->frame;
		GteReadMAC123(SpriteFrame().t);
		GteTransFromIR();
		SeqDraw(h);
	}
	// one splatter particle (0x6052A0's draw)
	static void SplatterDraw(uint8_t *h, const Part *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		TransformCameraByShadowRotation(p->v, p->size, -((int32_t)p->size >> 4));
		SeqDraw(h);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag157_breath_held.h"
#endif

namespace ff8fx
{
namespace breath157
{
	// ------------------------------------------------------------------
	// Swirl (0x6043A0), Burst (0x6049F0), Puff (0x604B30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SwirlTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_SWIRL, t);)
		FX_PART(PART_SWIRL);
		SwirlDraw(t);
		FX_PART(0);
		if (DrawOnly()) return 0;
		t->w18 = (int16_t)(t->w18 + t->w1A);
		t->counter++;
		return t->counter >= P().swirl.life ? TASK_END : 0;
	}

	static uint32_t __cdecl BurstTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_BURST, t);)
		FX_PART(PART_BURST);
		BurstDraw(t);
		FX_PART(0);
		if (DrawOnly()) return 0;
		const int16_t d = t->w1E;
		t->w1C = (int16_t)(t->w1C - d);
		t->w1E = (int16_t)(d - (int16_t)(d / 24));
		t->counter++;
		return t->counter >= P().burst.life ? TASK_END : 0;
	}

	static uint32_t __cdecl PuffTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_PUFF, t);)
		FX_PART(PART_PUFF);
		PuffDraw(t);
		FX_PART(0);
		if (DrawOnly()) return 0;
		int16_t *c = CloudPos(t->w0E);
		c[2] = (int16_t)(c[2] - t->w14);
		t->counter++;
		return t->counter >= P().puff.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// CloudModels (0x604E90), Rings (0x6050A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CloudModelsTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_CLOUDMODELS, t);)
		FX_PART(PART_CLOUDS);
		CloudModelsDraw(t);
		FX_PART(0);
		if (DrawOnly()) return 0;
		int16_t *c = CloudPos(t->w0E);
		const int16_t v = t->w14;
		c[2] = (int16_t)(c[2] - v);
		t->w14 = (int16_t)(v - (int16_t)(v / 6));
		t->w10 = (int16_t)(t->w10 + t->w12);
		const int16_t g = t->w1E;
		t->w18 = (int16_t)(t->w18 + t->w1A);
		t->w1C = (int16_t)(t->w1C + g);
		t->w1E = (int16_t)(g - (int16_t)(g / 8));
		t->counter++;
		return t->counter >= P().clouds.life ? TASK_END : 0;
	}

	static uint32_t __cdecl RingsTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_RINGS, t);)
		FX_PART(PART_RINGS);
		RingsDraw(t);
		FX_PART(0);
		if (DrawOnly()) return 0;
		const int16_t a = t->w1A;
		t->w18 = (int16_t)(t->w18 + a);
		t->w1A = (int16_t)(a - (int16_t)(a >> 4));
		const int16_t g = t->w1E;
		t->w1C = (int16_t)(t->w1C + g);
		t->w1E = (int16_t)(g - (int16_t)(g / 8));
		t->counter++;
		return t->counter >= P().rings.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// BreathParticles (0x6044D0): type 1
	// ------------------------------------------------------------------
	static uint32_t __cdecl BreathParticlesTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_BREATHPARTS, t);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int alive = 0;
		*(uint32_t *)h = fxp::sequence(P().seqs.breath, SEQ_Breath);
		*(int16_t *)(h + 0x24) = 0;
		LitSpriteSetup(&FrameA());
		for (Part *p = Parts(); (uint32_t)p < own::R(PARTS_END); p++)
		{
			if (!(p->type & 1)) continue;
			FX_PART(PART_BREATH);
			BreathPartDraw(h, p);
			FX_PART(0);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->type = 0;
				continue;
			}
			p->v[0] = (int16_t)(p->v[0] + p->v[1]);
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= P().breath.last_tick)
		{
			for (int k = 0; k < P().breath.per_tick; k++)
			{
				const int i = FreePart();
				if (i < 0) break;
				Part *p = &Parts()[i];
				p->type = 1;
				p->frame = 0;
				p->size = (int16_t)(CrtRand() % P().breath.size_rand + P().breath.size_min);
				p->v[0] = (int16_t)(CrtRand() % 0x1000);
				p->v[1] = (int16_t)(CrtRand() % P().breath.turn_rand + P().breath.turn_min);
				p->v[2] = (int16_t)(-P().breath.depth_min - CrtRand() % P().breath.depth_rand);
				p->v[3] = (int16_t)(CrtRand() % P().breath.radius_rand + P().breath.radius_min);
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// PuffParticles (0x604C80): type 2
	// ------------------------------------------------------------------
	static uint32_t __cdecl PuffParticlesTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_PUFFPARTS, t);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int alive = 0;
		*(uint32_t *)h = fxp::sequence(P().seqs.puff_part, SEQ_PuffPart);
		*(int16_t *)(h + 0x24) = 0;
		LitSpriteSetup(&FrameB());
		for (Part *p = Parts(); (uint32_t)p < own::R(PARTS_END); p++)
		{
			if (!(p->type & 2)) continue;
			FX_PART(PART_PUFF);
			PuffPartDraw(h, p);
			FX_PART(0);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(const int16_t *)(h + 0x28) < 0) p->type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= P().puff_parts.last_tick)
		{
			const int32_t spread = (int32_t)t->counter * P().puff_parts.spread_step + P().puff_parts.spread;
			const int16_t *c = CloudPos(t->w0E);
			for (int k = 0; k < P().puff_parts.per_tick; k++)
			{
				const int i = FreePart();
				if (i < 0) break;
				Part *p = &Parts()[i];
				p->type = 2;
				p->frame = 0;
				const int32_t half = spread >> 1;
				p->size = (int16_t)(CrtRand() % P().puff_parts.size_rand + P().puff_parts.size_min);
				*(uint32_t *)&p->v[0] = *(const uint32_t *)&c[0];
				*(uint32_t *)&p->v[2] = *(const uint32_t *)&c[2];
				p->v[0] = (int16_t)(p->v[0] + (CrtRand() % spread - half));
				p->v[1] = (int16_t)(p->v[1] + (CrtRand() % spread - half));
				p->v[2] = (int16_t)(p->v[2] + CrtRand() % (t->w14 ? (int32_t)t->w14 : 1));   // (0: no depth - the game's divides by it)
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Splatter (0x6052A0): type 4
	// ------------------------------------------------------------------
	static uint32_t __cdecl SplatterTask(TaskNode *nd)
	{
		EffectNode *t = (EffectNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note(REC_SPLATTER, t);)
		uint8_t *h = SeqHeader(fxp::sequence(P().seqs.splatter, SEQ_Splatter));
		int alive = 0;
		for (Part *p = Parts(); (uint32_t)p < own::R(PARTS_END); p++)
		{
			if (!(p->type & 4)) continue;
			FX_PART(PART_SPLATTER);
			SplatterDraw(h, p);
			FX_PART(0);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(const int16_t *)(h + 0x28) < 0) p->type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= P().splatter.last_tick)
		{
			for (int k = 0; k < P().splatter.per_tick; k++)
			{
				const int i = FreePart();
				if (i < 0) break;
				Part *p = &Parts()[i];
				p->type = 4;
				p->frame = 0;
				p->size = (int16_t)(CrtRand() % P().splatter.size_rand + P().splatter.size_min);
				*(uint32_t *)&p->v[0] = *(const uint32_t *)&TargetPos()[0];
				*(uint32_t *)&p->v[2] = *(const uint32_t *)&TargetPos()[2];
				const int32_t sp = P().splatter.spread, half = sp / 2;
				p->v[0] = (int16_t)(p->v[0] + (CrtRand() % sp - half));
				p->v[1] = (int16_t)(p->v[1] + (CrtRand() % sp - half));
				p->v[2] = (int16_t)(p->v[2] + (CrtRand() % sp - half));
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Cloud (0x604780)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CloudTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)nd;
		if (t->counter == 0)
		{
			int16_t *c = CloudPos(t->w0E);
			c[2] = 0;
			c[1] = 0;
			c[0] = 0;
		}
		if (t->counter == P().cloud.burst)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Burst));
			n->counter = 0;
			n->w1C = P().burst.size;
			n->w1E = P().burst.shrink;
		}
		if (t->counter == P().cloud.puff)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Puff));
			n->w0E = t->w0E;
			n->counter = 0;
			n->w1C = P().puff.size;
			const int16_t v = (int16_t)((shl32(mul32(LenB(), P().puff.speed), 8) >> 12) / 4);
			n->w14 = v;
			t->w14 = v;
		}
		if (t->counter == P().cloud.puff_parts)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_PuffParticles));
			n->counter = 0;
			n->w0E = t->w0E;
			n->w14 = t->w14;
		}
		if (t->counter == P().cloud.models)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_CloudModels));
			n->counter = 0;
			n->w0E = t->w0E;
			n->w10 = (int16_t)(CrtRand() % 0x1000);
			n->w12 = (int16_t)(CrtRand() % P().clouds.roll_rand + P().clouds.roll_min);
			const int32_t r = CrtRand() % 0x258;
			n->w14 = (int16_t)((shl32(mul32(r + LenB() - 0x12C, 5), 8) >> 12) / 5);
			n->w18 = (int16_t)(CrtRand() % 0x1000);
			n->w1A = (int16_t)(CrtRand() % P().clouds.spin_rand + P().clouds.spin_min);
			const int16_t g = (int16_t)((CrtRand() % P().clouds.size_rand + P().clouds.size_min) / 8);
			n->w1E = g;
			n->w1C = g;
			EffectNode *m = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Rings));
			m->counter = 0;
			m->w0E = t->w0E;
			m->w18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t spin = CrtRand() % P().rings.spin_rand;
			m->w1E = n->w1E;
			m->w1C = n->w1E;
			m->w1A = (int16_t)(spin + P().rings.spin_min);
			m->w20 = (int16_t)(CrtRand() % 0x1000);
		}
		if (t->counter == P().cloud.splatter)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Splatter));
			n->counter = 0;
		}
		t->counter++;
		return t->counter >= P().cloud.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x603F50)
	// ------------------------------------------------------------------
	// a mouth position: the caster's effect position (bone 0xF0) + off in the caster's frame; its
	// direction toward the target (normalised) and the root of its length
	static void Mouth(int16_t *mouth, int32_t *dir, int32_t &len, int32_t off_y, int32_t off_z)
	{
		GetEffectSpawnPosition(Caster(), 0xF0, 0x1000, mouth);
		int32_t v[3];
		v[0] = 0;
		v[1] = off_y;
		v[2] = off_z;
		TransformVectorBy3x3Matrix((const Mat4x3 *)(Caster() + 0x40), v, v);
		mouth[0] = (int16_t)(mouth[0] + v[0]);
		mouth[1] = (int16_t)(mouth[1] + v[1]);
		mouth[2] = (int16_t)(mouth[2] + v[2]);
		dir[1] = TargetPos()[1] - mouth[1];
		dir[0] = TargetPos()[0] - mouth[0];
		dir[2] = TargetPos()[2] - mouth[2];
		len = SqrtInt(NormalizeVectorLen(dir, dir));
	}
	static void BreathFrame(Mat4x3 &f, const int32_t *dir, const int16_t *mouth)
	{
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)REF_DIR, dir, axis);
		BuildAxisAngleRotationMatrix(angle, &f, axis);
		f.t[0] = mouth[0];
		f.t[1] = mouth[1];
		f.t[2] = mouth[2];
		ComposeAffineTransform(&Camera(), &f, &f);
	}

	static uint32_t __cdecl DirectorTask(TaskNode *nd)
	{
		BreathFrame(FrameA(), DirA(), MouthA());
		BreathFrame(FrameB(), DirB(), MouthB());
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)nd;
		if (t->counter == 0)
		{
			GetDefaultEffectPosition(Target(), TargetPos());
			Mouth(MouthA(), DirA(), LenA(), P().mouth.a_y, P().mouth.a_z);
		}
		if (t->counter == P().director.swirl)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Swirl));
			n->counter = 0;
			n->w18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t r = CrtRand() % P().swirl.spin_a_rand;
			n->w1C = P().swirl.size_a;
			n->w1A = (int16_t)(r + P().swirl.spin_a_min);
			n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Swirl));
			n->counter = 0;
			n->w18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t r2 = CrtRand() % P().swirl.spin_b_rand;
			n->w1C = P().swirl.size_b;
			n->w1A = (int16_t)(-P().swirl.spin_b_min - r2);
		}
		if (t->counter == P().director.breath)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_BreathParticles));
			n->counter = 0;
		}
		if (t->counter == P().director.mouth_b) Mouth(MouthB(), DirB(), LenB(), P().mouth.b_y, P().mouth.b_z);
		if (t->counter == P().director.cloud1 || t->counter == P().director.cloud2 || t->counter == P().director.cloud3)
		{
			EffectNode *n = (EffectNode *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Cloud));
			n->w0E = t->w0E;
			n->counter = 0;
			t->w0E++;
		}
		if (t->counter == P().director.damage) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == P().director.start_sound) BdPlaySE((const void *)fxp::sound(P().sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		if (t->counter == P().director.cloud_sound) BdPlaySE((const void *)fxp::sound(P().sounds.cloud, (uint32_t)SOUND_Cloud), 0, 0x80);
		t->counter++;
		return t->counter > P().director.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x603E70) and setup (0x603E90): the game runs its own; Fujin's programs these (its memory may be a
	// copy's): the magic buffer, the cast, its first target's and its caster's slots, the root and the director, the
	// particle table cleared, the camera and the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = (uint32_t)layouts::MagicBuffer();
		Ctx() = ctx;
		TargetSlot() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(RootQueue(), (void *)own::R(0x23EB5F8), 0x10, 2);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_RootTask)) + 0xC) = 0;
		InitTaskQueuePool(EffectQueue(), (void *)own::R(0x23EBFA0), 0x24, 0x64);
		uint8_t *director = (uint8_t *)AddTaskToQueue(EffectQueue(), own::Fn(ORIG_Director));
		*(uint16_t *)(director + 0xC) = 0;
		*(uint16_t *)(director + 0xE) = 0;
		for (uint32_t a = own::R(PARTS); a < own::R(PARTS_END); a += 0x18) *(uint32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA_ANIM));
		QueueTimUpload(TexFile());
		return RootQueue();
	}

	// ------------------------------------------------------------------
	// Root task (0x605460) and texture restore (0x6054D0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		RootNode *r = (RootNode *)nd;
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + ARENA_SIZE;
		else PacketCursor() = TexBase();
		const int a = ExecuteTaskQueue(EffectQueue());
		if (!a)
		{
			RootNode *w = (RootNode *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_TexRestore));
			w->counter = 0;
			w->done = 0;
		}
		r->counter++;
		return a ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *nd)
	{
		RootNode *r = (RootNode *)nd;
		if (r->counter == 1) RestoreDefaultEffectTexture(TexBase(), &r->done);
		const int16_t done = r->done;
		r->counter++;
		return done ? TASK_END : 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The timeline: the two mouths (the caster's effect point and an offset, aimed at the target), when the swirls, the breath and the three clouds start, the sounds; the damage" },
			{ PART_SWIRL, "swirl", "Swirls", "Two swirl models at mouth A spinning both ways, fading in and out" },
			{ PART_BREATH, "breath", "Breath", "Lit particles circling the breath's axis at mouth A" },
			{ 0, "cloud", "Clouds", "Each cloud's timeline: its burst, puff, puff particles, model and ring pairs, the splatter on the target (draws nothing itself)" },
			{ PART_BURST, "burst", "Burst", "A shrinking burst model at mouth B, fading in" },
			{ PART_PUFF, "puff", "Puff", "The lit puff rolling the cloud forward and its particles" },
			{ PART_CLOUDS, "clouds", "Cloud models", "Two cloud models rolling, growing, fading, moving forward" },
			{ PART_RINGS, "rings", "Rings", "A ring model pair turning, growing, fading" },
			{ PART_SPLATTER, "splatter", "Splatter", "Sprites around the target's effect position" },
		};
		static const Field FIELDS[] = {
			{ "director", "sounds.start", "Start sound", "At the director's start sound tick", (uint32_t)offsetof(BreathParams, sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.cloud", "Cloud sound", "At the director's cloud sound tick", (uint32_t)offsetof(BreathParams, sounds.cloud), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.swirl", "Swirl tick", "The two mouth swirls start at this tick of the director (mouth A is taken at tick 0)", (uint32_t)offsetof(BreathParams, director.swirl), fxp::I16, 0, 300 },
			{ "director", "director.breath", "Breath tick", "The breath particles start at this tick", (uint32_t)offsetof(BreathParams, director.breath), fxp::I16, 0, 300 },
			{ "director", "director.mouth_b", "Mouth B tick", "Mouth B (the clouds' frame) is taken at this tick", (uint32_t)offsetof(BreathParams, director.mouth_b), fxp::I16, 0, 300 },
			{ "director", "director.cloud1", "Cloud 1 tick", "The first cloud starts at this tick", (uint32_t)offsetof(BreathParams, director.cloud1), fxp::I16, 0, 300 },
			{ "director", "director.cloud2", "Cloud 2 tick", "The second cloud", (uint32_t)offsetof(BreathParams, director.cloud2), fxp::I16, 0, 300 },
			{ "director", "director.cloud3", "Cloud 3 tick", "The third cloud", (uint32_t)offsetof(BreathParams, director.cloud3), fxp::I16, 0, 300 },
			{ "director", "director.start_sound", "Start sound tick", "The start sound at this tick", (uint32_t)offsetof(BreathParams, director.start_sound), fxp::I16, 0, 300 },
			{ "director", "director.cloud_sound", "Cloud sound tick", "The cloud sound at this tick", (uint32_t)offsetof(BreathParams, director.cloud_sound), fxp::I16, 0, 300 },
			{ "director", "director.damage", "Damage tick", "The damage at this tick", (uint32_t)offsetof(BreathParams, director.damage), fxp::I16, 0, 300 },
			{ "director", "director.end", "End tick", "The director's last tick (the effect ends once every part has)", (uint32_t)offsetof(BreathParams, director.end), fxp::I16, 0, 300 },
			{ "director", "mouth.a_y", "Mouth A height", "Mouth A (the swirls and the breath): the caster's effect point 0xF0 moved by this along its y (negative = up)...", (uint32_t)offsetof(BreathParams, mouth.a_y), fxp::I16, -0x7FFF, 0x7FFF },
			{ "director", "mouth.a_z", "Mouth A ahead", "...and this along its z (negative = ahead)", (uint32_t)offsetof(BreathParams, mouth.a_z), fxp::I16, -0x7FFF, 0x7FFF },
			{ "director", "mouth.b_y", "Mouth B height", "Mouth B (the clouds): likewise", (uint32_t)offsetof(BreathParams, mouth.b_y), fxp::I16, -0x7FFF, 0x7FFF },
			{ "director", "mouth.b_z", "Mouth B ahead", "", (uint32_t)offsetof(BreathParams, mouth.b_z), fxp::I16, -0x7FFF, 0x7FFF },
			{ "swirl", "swirl.size_a", "Size A", "The first swirl's size (4096 = its model's)", (uint32_t)offsetof(BreathParams, swirl.size_a), fxp::I16, 0, 0x7FFF },
			{ "swirl", "swirl.spin_a_min", "Spin A", "Its spin a tick: this + a random below the next", (uint32_t)offsetof(BreathParams, swirl.spin_a_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "swirl", "swirl.spin_a_rand", "Spin A random", "(at least 1)", (uint32_t)offsetof(BreathParams, swirl.spin_a_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "swirl", "swirl.size_b", "Size B", "The second swirl's size", (uint32_t)offsetof(BreathParams, swirl.size_b), fxp::I16, 0, 0x7FFF },
			{ "swirl", "swirl.spin_b_min", "Spin B", "Its spin a tick, the other way: this + a random below the next", (uint32_t)offsetof(BreathParams, swirl.spin_b_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "swirl", "swirl.spin_b_rand", "Spin B random", "(at least 1)", (uint32_t)offsetof(BreathParams, swirl.spin_b_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "swirl", "swirl.fade_in", "Fade in until", "It fades in up to this tick of its own", (uint32_t)offsetof(BreathParams, swirl.fade_in), fxp::I16, 0, 300 },
			{ "swirl", "swirl.fade_out", "Fade out from", "From this tick it fades out", (uint32_t)offsetof(BreathParams, swirl.fade_out), fxp::I16, 0, 300 },
			{ "swirl", "swirl.life", "End tick", "The swirl ends at this tick of its own", (uint32_t)offsetof(BreathParams, swirl.life), fxp::I16, 1, 300 },
			{ "breath", "breath.last_tick", "Last spawn tick", "Particles spawn from its tick 0 to this one", (uint32_t)offsetof(BreathParams, breath.last_tick), fxp::I16, -1, 300 },
			{ "breath", "breath.per_tick", "A tick", "How many spawn a tick (100 particles at most, shared with the puffs and the splatter)", (uint32_t)offsetof(BreathParams, breath.per_tick), fxp::I16, 0, 100 },
			{ "breath", "breath.size_min", "Size", "A particle's size: this + a random below the next", (uint32_t)offsetof(BreathParams, breath.size_min), fxp::I16, 0, 0x7FFF },
			{ "breath", "breath.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(BreathParams, breath.size_rand), fxp::I16, 1, 0x7FFF },
			{ "breath", "breath.turn_min", "Turn", "It turns about the breath's axis a tick by this + a random below the next", (uint32_t)offsetof(BreathParams, breath.turn_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "breath", "breath.turn_rand", "Turn random", "(at least 1)", (uint32_t)offsetof(BreathParams, breath.turn_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "breath", "breath.depth_min", "Depth", "It lies this far along the breath (toward the target) + a random below the next", (uint32_t)offsetof(BreathParams, breath.depth_min), fxp::I16, 0, 0x7FFF },
			{ "breath", "breath.depth_rand", "Depth random", "(at least 1)", (uint32_t)offsetof(BreathParams, breath.depth_rand), fxp::I16, 1, 0x7FFF },
			{ "breath", "breath.radius_min", "Radius", "Its distance from the axis: this + a random below the next", (uint32_t)offsetof(BreathParams, breath.radius_min), fxp::I16, 0, 0x7FFF },
			{ "breath", "breath.radius_rand", "Radius random", "(at least 1)", (uint32_t)offsetof(BreathParams, breath.radius_rand), fxp::I16, 1, 0x7FFF },
			{ "cloud", "cloud.burst", "Burst tick", "At this tick of each cloud its burst (the cloud's position is reset at 0)", (uint32_t)offsetof(BreathParams, cloud.burst), fxp::I16, 0, 300 },
			{ "cloud", "cloud.puff", "Puff tick", "Its puff (it sets the cloud's speed)", (uint32_t)offsetof(BreathParams, cloud.puff), fxp::I16, 0, 300 },
			{ "cloud", "cloud.puff_parts", "Puff particles tick", "Its puff particles (spread along the cloud's speed: after the puff)", (uint32_t)offsetof(BreathParams, cloud.puff_parts), fxp::I16, 0, 300 },
			{ "cloud", "cloud.models", "Models tick", "Its cloud model pair and ring pair", (uint32_t)offsetof(BreathParams, cloud.models), fxp::I16, 0, 300 },
			{ "cloud", "cloud.splatter", "Splatter tick", "Its splatter on the target", (uint32_t)offsetof(BreathParams, cloud.splatter), fxp::I16, 0, 300 },
			{ "cloud", "cloud.life", "End tick", "The cloud ends at this tick of its own", (uint32_t)offsetof(BreathParams, cloud.life), fxp::I16, 1, 300 },
			{ "burst", "burst.size", "Size", "The burst's starting size (4096 = its model's)", (uint32_t)offsetof(BreathParams, burst.size), fxp::I16, 0, 0x7FFF },
			{ "burst", "burst.shrink", "Shrink", "It shrinks a tick by this (which loses 1/24 a tick)", (uint32_t)offsetof(BreathParams, burst.shrink), fxp::I16, -0x7FFF, 0x7FFF },
			{ "burst", "burst.fade_in", "Fade in until", "It fades in up to this tick of its own", (uint32_t)offsetof(BreathParams, burst.fade_in), fxp::I16, 0, 300 },
			{ "burst", "burst.life", "End tick", "The burst ends at this tick of its own", (uint32_t)offsetof(BreathParams, burst.life), fxp::I16, 1, 300 },
			{ "puff", "puff.size", "Size", "The puff sprite's size", (uint32_t)offsetof(BreathParams, puff.size), fxp::I16, 0, 0x7FFF },
			{ "puff", "puff.speed", "Speed", "The cloud's speed: the root of the mouth-target distance x this / 64 (11: the game's)", (uint32_t)offsetof(BreathParams, puff.speed), fxp::I16, 0, 0x7FFF },
			{ "puff", "puff.life", "End tick", "The puff ends at this tick of its own", (uint32_t)offsetof(BreathParams, puff.life), fxp::I16, 1, 300 },
			{ "puff", "puff_parts.last_tick", "Particles: last spawn tick", "Puff particles spawn from their tick 0 to this one", (uint32_t)offsetof(BreathParams, puff_parts.last_tick), fxp::I16, -1, 300 },
			{ "puff", "puff_parts.per_tick", "Particles: a tick", "How many a tick", (uint32_t)offsetof(BreathParams, puff_parts.per_tick), fxp::I16, 0, 100 },
			{ "puff", "puff_parts.spread", "Particles: spread", "Their spread across the breath: this + the tick x the next", (uint32_t)offsetof(BreathParams, puff_parts.spread), fxp::I16, 1, 0x7FFF },
			{ "puff", "puff_parts.spread_step", "Particles: spread step", "", (uint32_t)offsetof(BreathParams, puff_parts.spread_step), fxp::I16, 0, 0x7FFF },
			{ "puff", "puff_parts.size_min", "Particles: size", "A puff particle's size: this + a random below the next", (uint32_t)offsetof(BreathParams, puff_parts.size_min), fxp::I16, 0, 0x7FFF },
			{ "puff", "puff_parts.size_rand", "Particles: size random", "(at least 1)", (uint32_t)offsetof(BreathParams, puff_parts.size_rand), fxp::I16, 1, 0x7FFF },
			{ "clouds", "clouds.roll_min", "Roll", "The cloud models' roll a tick: this + a random below the next", (uint32_t)offsetof(BreathParams, clouds.roll_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "clouds", "clouds.roll_rand", "Roll random", "(at least 1)", (uint32_t)offsetof(BreathParams, clouds.roll_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "clouds", "clouds.spin_min", "Spin", "Their spin a tick: this + a random below the next", (uint32_t)offsetof(BreathParams, clouds.spin_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "clouds", "clouds.spin_rand", "Spin random", "(at least 1)", (uint32_t)offsetof(BreathParams, clouds.spin_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "clouds", "clouds.size_min", "Size", "Their size and growth: (this + a random below the next) / 8 (the growth loses 1/8 a tick; the ring pair's too)", (uint32_t)offsetof(BreathParams, clouds.size_min), fxp::I16, 0, 0x7FFF },
			{ "clouds", "clouds.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(BreathParams, clouds.size_rand), fxp::I16, 1, 0x7FFF },
			{ "clouds", "clouds.outer", "Outer model size", "The second model's size: the first's + this", (uint32_t)offsetof(BreathParams, clouds.outer), fxp::I16, -0x7FFF, 0x7FFF },
			{ "clouds", "clouds.fade_from", "Fade from", "From this tick of its own the pair fades", (uint32_t)offsetof(BreathParams, clouds.fade_from), fxp::I16, 0, 300 },
			{ "clouds", "clouds.life", "End tick", "The pair ends at this tick of its own", (uint32_t)offsetof(BreathParams, clouds.life), fxp::I16, 1, 300 },
			{ "rings", "rings.spin_min", "Spin", "The ring pair's spin a tick: this + a random below the next (it loses 1/16 a tick)", (uint32_t)offsetof(BreathParams, rings.spin_min), fxp::I16, 0, 4096, fxp::Angle },
			{ "rings", "rings.spin_rand", "Spin random", "(at least 1)", (uint32_t)offsetof(BreathParams, rings.spin_rand), fxp::I16, 1, 4096, fxp::Angle },
			{ "rings", "rings.size", "Size", "Their size: the cloud pair's size + this", (uint32_t)offsetof(BreathParams, rings.size), fxp::I16, -0x7FFF, 0x7FFF },
			{ "rings", "rings.fade_from", "Fade from", "From this tick of its own the pair fades", (uint32_t)offsetof(BreathParams, rings.fade_from), fxp::I16, 0, 300 },
			{ "rings", "rings.life", "End tick", "The pair ends at this tick of its own", (uint32_t)offsetof(BreathParams, rings.life), fxp::I16, 1, 300 },
			{ "splatter", "splatter.last_tick", "Last spawn tick", "Splatter sprites spawn from its tick 0 to this one", (uint32_t)offsetof(BreathParams, splatter.last_tick), fxp::I16, -1, 300 },
			{ "splatter", "splatter.per_tick", "A tick", "How many a tick", (uint32_t)offsetof(BreathParams, splatter.per_tick), fxp::I16, 0, 100 },
			{ "splatter", "splatter.spread", "Spread", "Around the target's effect position: a random of this width on each axis (at least 1)", (uint32_t)offsetof(BreathParams, splatter.spread), fxp::I16, 1, 0x7FFF },
			{ "splatter", "splatter.size_min", "Size", "A splatter sprite's size: this + a random below the next", (uint32_t)offsetof(BreathParams, splatter.size_min), fxp::I16, 0, 0x7FFF },
			{ "splatter", "splatter.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(BreathParams, splatter.size_rand), fxp::I16, 1, 0x7FFF },
			{ "breath", "seqs.breath", "Breath flipbook", "The sprite sequence of the breath particles (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(BreathParams, seqs.breath), fxp::Seq, 0, 0x7FFFFFFF },
			{ "puff", "seqs.puff", "Puff flipbook", "The sprite sequence of the puff (a flipbook of 4 frames: an edited one keeps as many)", (uint32_t)offsetof(BreathParams, seqs.puff), fxp::Seq, 0, 0x7FFFFFFF },
			{ "puff", "seqs.puff_part", "Puff particle flipbook", "The sprite sequence of the puff particles (a flipbook of 8 frames: an edited one keeps as many)", (uint32_t)offsetof(BreathParams, seqs.puff_part), fxp::Seq, 0, 0x7FFFFFFF },
			{ "splatter", "seqs.splatter", "Splatter flipbook", "The sprite sequence of the splatter (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(BreathParams, seqs.splatter), fxp::Seq, 0, 0x7FFFFFFF },
			{ "swirl", "models.swirl", "Swirl model", "The prim model of the swirls (a mesh of 145 vertices, 144 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(BreathParams, models.swirl), fxp::Model, 0, 0x7FFFFFFF },
			{ "burst", "models.burst", "Burst model", "The prim model of the burst (a mesh of 96 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(BreathParams, models.burst), fxp::Model, 0, 0x7FFFFFFF },
			{ "clouds", "models.cloud1", "Cloud model", "The prim model of the first cloud model (a mesh of 110 vertices, 120 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(BreathParams, models.cloud1), fxp::Model, 0, 0x7FFFFFFF },
			{ "clouds", "models.cloud2", "Outer cloud model", "The prim model of the second one (a mesh of 108 vertices, 96 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(BreathParams, models.cloud2), fxp::Model, 0, 0x7FFFFFFF },
			{ "rings", "models.ring", "Ring model", "The prim model of the rings (a mesh of 72 vertices, 48 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(BreathParams, models.ring), fxp::Model, 0, 0x7FFFFFFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(BreathParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(BreathParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag157_breath()
	{
		fxp::register_params({ 157, "Breath", &breath157::live, &breath157::GAME, sizeof(breath157::BreathParams),
		                       breath157::table::PARTS, (int)(sizeof(breath157::table::PARTS) / sizeof(breath157::table::PARTS[0])),
		                       breath157::table::FIELDS, (int)(sizeof(breath157::table::FIELDS) / sizeof(breath157::table::FIELDS[0])),
		                       nullptr, 0 });
		own::note(breath157::ORIG_FileLoader, (void *)breath157::FileLoader);
		own::note(breath157::ORIG_Setup, (void *)breath157::Setup);
		own::note(breath157::ORIG_RootTask, (void *)breath157::RootTask);
		register_port(breath157::ORIG_RootTask, (void *)breath157::RootTask, "B157 RootTask", 157);
		own::note(breath157::ORIG_TexRestore, (void *)breath157::TexRestoreTask);
		register_port(breath157::ORIG_TexRestore, (void *)breath157::TexRestoreTask, "B157 TexRestore", 157);
		own::note(breath157::ORIG_Director, (void *)breath157::DirectorTask);
		register_port(breath157::ORIG_Director, (void *)breath157::DirectorTask, "B157 Director", 157);
		own::note(breath157::ORIG_Swirl, (void *)breath157::SwirlTask);
		register_port(breath157::ORIG_Swirl, (void *)breath157::SwirlTask, "B157 Swirl", 157);
		own::note(breath157::ORIG_BreathParticles, (void *)breath157::BreathParticlesTask);
		register_port(breath157::ORIG_BreathParticles, (void *)breath157::BreathParticlesTask, "B157 BreathParticles", 157);
		own::note(breath157::ORIG_Cloud, (void *)breath157::CloudTask);
		register_port(breath157::ORIG_Cloud, (void *)breath157::CloudTask, "B157 Cloud", 157);
		own::note(breath157::ORIG_Burst, (void *)breath157::BurstTask);
		register_port(breath157::ORIG_Burst, (void *)breath157::BurstTask, "B157 Burst", 157);
		own::note(breath157::ORIG_Puff, (void *)breath157::PuffTask);
		register_port(breath157::ORIG_Puff, (void *)breath157::PuffTask, "B157 Puff", 157);
		own::note(breath157::ORIG_PuffParticles, (void *)breath157::PuffParticlesTask);
		register_port(breath157::ORIG_PuffParticles, (void *)breath157::PuffParticlesTask, "B157 PuffParticles", 157);
		own::note(breath157::ORIG_CloudModels, (void *)breath157::CloudModelsTask);
		register_port(breath157::ORIG_CloudModels, (void *)breath157::CloudModelsTask, "B157 CloudModels", 157);
		own::note(breath157::ORIG_Rings, (void *)breath157::RingsTask);
		register_port(breath157::ORIG_Rings, (void *)breath157::RingsTask, "B157 Rings", 157);
		own::note(breath157::ORIG_Splatter, (void *)breath157::SplatterTask);
		register_port(breath157::ORIG_Splatter, (void *)breath157::SplatterTask, "B157 Splatter", 157);
		// 30 fps layer: see mag157_breath_held.inc
		FX_HELD(register_mag157_held();)
	}
	FX_LAB_MODULE(register_mag157_breath);
}

#ifdef FF8_FX_HELD
#include "mag157_breath_held.inc"
#endif
