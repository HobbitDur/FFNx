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
// Effect 159: Renzokuken vs X-ATM092 (MAG_159_RENZOKUKEN_VS_XATM092). Launched as Squall's Renzokuken command:
// computeCommandAction (0x48DE0B) takes the effect id from the TARGET monster's info section (+0x150, 8 x u16, indexed by
// the caster's renzokuken tier byte + a random 0..1): X-ATM092 c0m092 has 159 in all 8. Same source as the generic
// Renzokuken 160 (mag160_renzokuken.cpp) and 161 (vs Elvoret, mag161_renzokuken_elvoret.cpp): the hit sparks, sparkles,
// trail (161's: the hit point is the tip spline's first point), weapon bone, spark spawner, slash, particle draws and
// master are the same code on this module's globals; the controller, the hit scheduler and four ring tasks are its own,
// and the boss (the first target) gets its own animation set as in 161.
//
// Structure (file loader 0x600BA0 = the texture file named at 0xDAFBD8; setup MAG_159 0x600BC0, not a task: keeps the
// cast context 0x23D7A58, the caster slot 0x23D7A5C and the first target 0x23D56C8, starts the master in its 1-node pool
// 0x23D63C0 (queue 0x23D63D0) and the controller in the main pool 0x23D6C48 (100 x 0x24, queue 0x23D6C38), the total
// length 0x23D7AF4 = 4 for now, the finisher flag 0x23D7AF8 = (action 0 word +4 == 0xFFFE: a finisher follows), the three
// particle pools free, the camera animation 0xEA3468 and the TIM upload):
//   MasterTask (0x602C40) - packet arena parity (texture base 0x23D7AFC, + 0x10000 on odd ticks; cursor 0x23D7B00), the
//     main queue, then the three particle pools (ParticleDraws 0x602C90); ends with the main queue.
//   ControllerTask (0x600CC0) - the choreography of the caster (Squall) and the boss:
//     tick 0: sets the caster's entity flag 0x800 (its old value kept in the node);
//     tick 1: BindBoth (0x602BF0): the module's animation container 0xEA3A18 (2 com-data blocks: +4 the caster's, +8 the
//       boss's) bound to the caster and to the boss (out blocks 0x24FEBF8 / the next one), the caster's sequence 1 (2 when
//       a finisher follows) and the boss's sequence 1 started;
//     tick 2: the stage durations give the length; starts the trail, the spark spawner and the hit scheduler;
//     tick = length with a finisher: the hit-spark task in the hit-effect queue (0x209FAA8);
//     tick 0x1A: ring A (RingATask, model 0xDAD510) at the trail's hit point;
//     ticks 0x29..0x32: one ring B (RingBTask, model 0xDAD928) per tick at the hit point, and 4 debris (pool C) thrown
//       out of it (direction random in the upper half, speed 0x8C..0x167);
//     tick 0x33: ring C (RingCTask, model 0xDAD9C8) at the hit point and 16 debris thrown out of it (speed 0xAA..0x1E9);
//     tick 0x54: 8 rings D (RingDTask, model 0xDAE9B8) scattered (+-0x384) around the boss's effect position on the
//       ground, each delayed 2 k ticks;
//     screen flash up over ticks 0..0x10 (counter << 7), down over the last 8 (no finisher);
//     the caster's model tint (entity +0x28) fades to the far colour (0xF0, 0x60, 0x40) over ticks 0..8, back over the
//       last 8 (GTE DPCS from the stage colour 0xB8B7D8); sound 0xEA32CC at tick 1;
//     after the last tick: flash off (no finisher), tint back to 0xB8B7D8, entity flag 0x800 restored; ends.
//   TrailTask (0x601980) - as 161's (the hit point is the tip spline's FIRST point, 0x23D6B20).
//   SparkSpawnTask (0x601EC0), SlashTask (0x602390), HitSparkTask (0x6013C0), SparkleTask (0x601650): as effect 160's
//     (see mag160_renzokuken.cpp), on this module's globals and models.
//   HitTask (0x601FF0) - per hit b (< n): at counter = (stage sums up to b) - 0xDAFB38[b]: a slash (scale 0x500..0x7FF)
//     with record b (0x23D5C70 + 0x9C b: 16 streaks, width 0xC00..0x17FF, offset width * 1000 >> 12), 3 flashes in
//     pool A (0x23D56D0) and 12 debris in pool C (0x23D63E0, 80 entries) thrown along the direction with a random part
//     +-0xDAFB68[b] (thrown back for hit 0).
//   RingATask (0x602670) - flat ring growing (all axes, rate 0x580, - rate / 4 each tick), 10 ticks, fading from tick 4.
//   RingBTask (0x6027C0) - z 0x800, x / y growing (rate 0x600, - rate / 6), 8 ticks, fading from tick 4 (step 0x400).
//   RingCTask (0x602910) - all axes (rate 0x1300, - rate / 3), 12 ticks, fading from tick 4 (step 0x200).
//   RingDTask (0x602A50) - after its delay: x / z and y growing apart (- rate / 3 each), 10 ticks, fading from tick 4.
// Every task but the master and RingDTask's drawing part tests the draw-only flags (battle_to_update_flags 0x1D96A9C &
// 0x201).
// ENTITY WRITES: the caster (entity 0x1D972C0 + 0x9C slot): +0 bit 0x800 (set on tick 0, put back at the end), +0x28
// model tint, BS_BindEntityModelFromComData (+2 |= 8, the animation / sequence / weapon animation pointers, its state block
// +0x12 = +2, +2 = 1), the AnimSeq chain start (0x505C00) and the hit-effect AnimSeq task (0x571840). The boss (first
// target): BS_BindEntityModelFromComData (+2 |= 8, +0x64 -> its out block: 0xEA3A18's second block) and its sequence 1
// started (0x505C00; it turns the boss, E5 0x17 = entity +0xE, and plays its 6 bound animations). The module never writes a
// position (+0x1C..+0x20 / the matrices). CAMERA: the camera animation 0xEA3468 started by the setup (engine camera player);
// reads the eye x / z (0xB8B7F0 / 0xB8B7F4) and the camera matrix 0x1D97778; no camera write.
// Module globals 0x23D5648..0x23D7B04: gathered trail points 0x23D5648 / 0x23D5688 (7 x 8), target 0x23D56C8, pool A
// 0x23D56D0 (40 x 0xC), pool B 0x23D58B0 (60 x 0x10, 40 spawned), hit records 0x23D5C70 (12 x 0x9C), master pool / queue
// 0x23D63C0 / 0x23D63D0, pool C 0x23D63E0 (80 x 0x14), splines 0x23D6A20 / 0x23D6B20, hit point 0x23D6C20, direction
// 0x23D6C28, main queue / pool 0x23D6C38 / 0x23D6C48, context 0x23D7A58, caster slot 0x23D7A5C, file 0x23D7A60, trail ring
// 0x23D7A68 (7 x 0x14), length 0x23D7AF4, finisher flag 0x23D7AF8, texture base 0x23D7AFC, packet cursor 0x23D7B00.

#include "mag_common.h"

namespace ff8fx
{
namespace renzo159
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &CasterSlot() { return var<int32_t>(0x23D7A5C); }
	inline int32_t &TargetSlot() { return var<int32_t>(0x23D56C8); }  // the boss (first target)
	inline int32_t &Length() { return var<int32_t>(0x23D7AF4); }      // sum of the stage durations (4 until tick 2)
	inline int32_t &Finisher() { return var<int32_t>(0x23D7AF8); }    // a finisher follows (action 0 word +4 == 0xFFFE)
	inline uint32_t &TexBase() { return var<uint32_t>(0x23D7AFC); }   // Magic_TextureOFF
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23D7B00); }
	inline TaskQueue *QMain() { return (TaskQueue *)0x23D6C38; }
	inline TaskQueue *QHit() { return (TaskQueue *)0x209FAA8; }      // the battle's hit-effect queue
	static const uint32_t POOL_A = 0x23D56D0, POOL_A_END = 0x23D58B0;  // flashes, 0xC bytes
	static const uint32_t POOL_B = 0x23D58B0, POOL_B_SPAWN_END = 0x23D5B30, POOL_B_END = 0x23D5C70; // sparks, 0x10 bytes
	static const uint32_t POOL_C = 0x23D63E0, POOL_C_END = 0x23D6A20;  // debris, 0x14 bytes (80)
	static const uint32_t HIT_REC = 0x23D5C70;     // per hit, 0x9C bytes
	static const uint32_t PTS_A = 0x23D5648, PTS_B = 0x23D5688;  // gathered trail points (8 bytes each)
	static const uint32_t SPL_A = 0x23D6A20, SPL_B = 0x23D6B20;  // 32 spline points each (8 bytes)
	static const uint32_t HIT_POS = 0x23D6C20;     // the tip spline's 1st point (x, y, z, pad)
	static const uint32_t HIT_DIR = 0x23D6C28;     // unit direction from the 2nd point to it (3 x int32 + one word never written)
	static const uint32_t RING = 0x23D7A68, RING_END = 0x23D7AF4; // 7 x 0x14: alive, base (8 bytes), tip (8 bytes)
	// stage durations of the caster's sequence (AnimSeq opcode AB: 0x1D98214 = n, n + 1 frame counts from 0x1D98215)
	inline uint8_t StageCount() { return var<uint8_t>(0x1D98214); }
	inline uint8_t StageLen(int i) { return var<uint8_t>(0x1D98215 + i); }

	static const uint32_t ORIG_Controller = 0x600CC0, ORIG_HitSpark = 0x6013C0, ORIG_Sparkle = 0x601650,
		ORIG_Trail = 0x601980, ORIG_SparkSpawn = 0x601EC0, ORIG_Hit = 0x601FF0, ORIG_Slash = 0x602390,
		ORIG_RingA = 0x602670, ORIG_RingB = 0x6027C0, ORIG_RingC = 0x602910, ORIG_RingD = 0x602A50, ORIG_Master = 0x602C40;

	// data (FF8_EN.exe)
	static const void *const SOUND = (const void *)0xEA32CC;
	static const uint32_t MODEL_SPARKLE = 0xEA32D0, SPARKLE_FADE = 0xEA3458, SPARKLE_DEPTH = 0xEA3448;
	static const uint32_t ANIM_SETS = 0xEA3A18, BIND_OUT = 0x24FEBF8; // container: +4 the caster's com data, +8 the boss's
	static const uint32_t MODEL_SLASH = 0xDAC858, MODEL_STREAK = 0xDAD240;
	static const uint32_t MODEL_RING_A = 0xDAD510, MODEL_RING_B = 0xDAD928, MODEL_RING_C = 0xDAD9C8, MODEL_RING_D = 0xDAE9B8;
	static const uint32_t SEQ_FLASH = 0xDAC2D0, SEQ_DEBRIS = 0xDAC5A0, SEQ_SPARK = 0xDAC47C, SPARK_FRAMES = 0xDAFAC4;
	static const uint32_t SLASH_REF = 0xDAFB28;   // reference direction of the slash model
	static const uint32_t HIT_LEAD = 0xDAFB38;    // per hit: ticks before its stage end
	static const uint32_t HIT_SPREAD = 0xDAFB68;  // per hit: the debris velocity's random part +- this

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }                                       // Math_SqrtInt
	inline void GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }       // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }   // GTE_SetFarColor
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60 (calls 0x45DD60)
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }                    // IR = MAC = R * V0
	inline uint32_t BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData (returns the next out block)
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, seq); }
	inline void AddAnimSeqTask(uint8_t *entity, int32_t type) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x571840)(entity, type); }          // AddTaskToQueueAnimSeq_99_B0_B1_B4_A5

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // master pool, 1 x 0x10 (0x23D63C0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
	};
	struct Node24 // main pool (0x23D6C48) and the hit-effect queue pool (0x209D078), 0x24 bytes
	{
		TaskNode hdr;
		int16_t c;         // +0x0C counter
		int16_t e;         // +0x0E controller: saved flag 0x800 / spawner: frame phase / slash: hit / sparkle, ring D: delay
		int16_t w10, w12, w14; // +0x10 position (rings, sparkle)
		int16_t w16;       // +0x16 rings A..C: the hit point's 4th word
		int16_t w18;       // +0x18 sparkle: bone
		int16_t w1A;       // +0x1A hit spark / sparkle: entity slot
		int16_t w1C;       // +0x1C slash / rings: scale / sparkle: depth frame
		int16_t w1E;       // +0x1E slash / rings: scale rate
		int16_t w20;       // +0x20 ring D: y scale / sparkle: scale
		int16_t w22;       // +0x22 ring D: y scale rate
	};
	struct FlashP  // pool A
	{
		int16_t life, scale;
		int16_t pos[4];    // x, y, z, the 4th word of the hit point
	};
	struct SparkP  // pool B
	{
		int16_t life, scale;
		int16_t pos[3];
		int16_t vy;        // +0x0A
		int16_t frame;     // +0x0C 0..23
		int16_t pad0E;
	};
	struct DebrisP // pool C
	{
		int16_t life, scale;
		int16_t pos[4];    // x, y, z, the 4th word of the hit point
		int16_t vel[3];    // +0x0C
		int16_t pad12;
	};
	struct HitRec  // 0x9C bytes per hit
	{
		int16_t pos[4];    // +0x00 the hit point (0x23D6C20)
		int32_t dir[4];    // +0x08 the direction (0x23D6C28, 16 bytes)
		int32_t count;     // +0x18 streaks (16)
		int16_t angle[16]; // +0x1C streak angle about the direction
		int16_t width[16]; // +0x3C streak width (the length * 1.5 across, the length along)
		int16_t off[16];   // +0x5C streak offset along its axis
		int16_t speed[16]; // +0x7C added to the offset each tick, slowing
	};
	struct RingEntry   // trail ring, 0x14 bytes
	{
		int32_t alive;
		uint32_t base[2];  // x, y, z, pad
		uint32_t tip[2];
	};
	struct TrailWork   // the trail's Field_Alloc block (0x40 bytes)
	{
		uint8_t pad00[8];
		int32_t otz;       // +0x08
		uint32_t flag;     // +0x0C GTE FLAG
		uint32_t col0;     // +0x10 the previous quad's colour
		uint32_t col1;     // +0x14 the colour of this quad
		uint32_t pad18;
		uint32_t base_col; // +0x1C
		int16_t base[4];   // +0x20 the weapon point (4th word: the scratch's)
		int16_t tip[4];    // +0x28 666 along the blade (4th word: the scratch's)
		int32_t dir[4];    // +0x30 the blade direction (unit), 4th word unused
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24, "Renzokuken nodes");
	static_assert(sizeof(FlashP) == 0xC && sizeof(SparkP) == 0x10 && sizeof(DebrisP) == 0x14, "Renzokuken pools");
	static_assert(sizeof(HitRec) == 0x9C && sizeof(RingEntry) == 0x14 && sizeof(TrailWork) == 0x40, "Renzokuken records");

	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline uint8_t *Target() { return Entity(TargetSlot()); }

	// the four ring tasks: model, how the scale applies, how long, the fade step, the rate divisor
	enum RingKind { RING_A, RING_B, RING_C, RING_D };
	struct RingDef { uint32_t orig, model; int32_t fade_mul, fade_shift, ticks, rate_div; };
	static const RingDef RINGS[4] = {
		{ ORIG_RingA, MODEL_RING_A, 682, 0, 10, 4 },  // all axes; - rate / 4 (rounded toward 0)
		{ ORIG_RingB, MODEL_RING_B, 1, 10, 8, 6 },    // x / y, z 0x800; - rate / 6
		{ ORIG_RingC, MODEL_RING_C, 1, 9, 12, 3 },    // all axes; - rate / 3
		{ ORIG_RingD, MODEL_RING_D, 682, 0, 10, 3 },  // x / z and y apart; - rate / 3 each
	};

	// ------------------------------------------------------------------
	// Weapon bone (0x601E40 / 0x601DC0)
	// ------------------------------------------------------------------
	// BoneFrame (0x601E40): bone `bone` of a model (0xF0 / 0xF1: the skeleton's effect bone selectors): its z axis
	// (0, 0, -0x1000) through the bone's rotation, normalized, and its translation (3 words)
	static void BoneFrame(uint8_t *anim_header, int32_t bone, int32_t *dir, int16_t *pos)
	{
		const uint8_t *sk = *(uint8_t **)*(uint8_t **)(anim_header + 4);
		if (bone >= 0xF0) bone = sk[bone - 0xEC];
		const uint8_t *b = sk + 0x10 + bone * 0x30;
		dir[1] = 0;
		dir[0] = 0;
		dir[2] = -0x1000;
		TransformVectorBy3x3Matrix((const Mat4x3 *)(b + 0x10), dir, dir);
		Normalize(dir, dir);
		pos[0] = *(const int16_t *)(b + 0x24);
		pos[1] = *(const int16_t *)(b + 0x28);
		pos[2] = *(const int16_t *)(b + 0x2C);
	}

	// WeaponBone (0x601DC0): the weapon model's bone (entity +0x78) in the world: through its attach transform
	// (+0x24) and the entity's matrix (+0x40); nothing when the entity has no weapon model
	static void WeaponBone(uint8_t *entity, int32_t bone, int32_t *dir, int16_t *pos)
	{
		uint8_t *w = *(uint8_t **)(entity + 0x78);
		if (!w) return;
		BoneFrame(w, bone, dir, pos);
		Mat4x3 m; // (pad +0x12: ComposeAffineTransform's)
		ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), (const Mat4x3 *)(w + 0x24), &m);
		TransformVectorBy3x3Matrix(&m, dir, dir);
		Normalize(dir, dir);
		MatVec(&m, pos, pos);
		pos[0] = (int16_t)(pos[0] + (int16_t)m.t[0]);
		pos[1] = (int16_t)(pos[1] + (int16_t)m.t[1]);
		pos[2] = (int16_t)(pos[2] + (int16_t)m.t[2]);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag159_renzokuken_xatm_held.h"
#endif

namespace ff8fx
{
namespace renzo159
{
	// ------------------------------------------------------------------
	// BindBoth (0x602BF0): the animation container's two com-data blocks bound to the caster and to the boss, both
	// sequences started (the caster's `seq`, the boss's 1)
	// ------------------------------------------------------------------
	static void BindBoth(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		const uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		QueueChainTransformation(caster, seq);
		QueueChainTransformation(boss, 1);
	}

	// first free entry (life < 0) of a pool, or -1 (none in the scanned part / index >= cap)
	static int32_t FreeSlot(uint32_t pool, uint32_t end, uint32_t stride, int32_t cap = 0x28)
	{
		int32_t i = 0;
		for (uint32_t a = pool; *(const int16_t *)a >= 0; i++)
		{
			a += stride;
			if (a >= end) return -1;
		}
		return i >= cap ? -1 : i;
	}

	// a ring task at the hit point (rings A..C): position = the hit point (8 bytes), scale / rate = s
	static void AddRingAtHit(uint32_t orig, int16_t s)
	{
		Node24 *t = (Node24 *)AddTaskToQueue(QMain(), orig);
		memcpy(&t->w10, (const void *)HIT_POS, 8);
		t->c = 0;
		t->w1E = s;
		t->w1C = s;
	}

	// debris thrown out of the hit point: direction (x random, y from y0, z random), speed sp0 + rand % spn
	static void DebrisBurst(int32_t count, int32_t scale_n, int32_t scale0, bool wide_y, int32_t spn, int32_t sp0)
	{
		for (int32_t j = 0; j < count; j++)
		{
			const int32_t i = FreeSlot(POOL_C, POOL_C_END, 0x14, 0x50);
			if (i < 0) break;
			DebrisP *p = (DebrisP *)(POOL_C + 0x14 * i);
			p->life = 0;
			p->scale = (int16_t)(CrtRand() % scale_n + scale0);
			memcpy(p->pos, (const void *)HIT_POS, 8);
			int32_t v[3];
			v[0] = CrtRand() % 0x1000 - 0x800;
			if (wide_y)
			{
				const int32_t r = CrtRand() % 0x1000;
				v[1] = r - CrtRand() % 0x800 - 0x800;
				v[2] = -(CrtRand() % 0x1000);
			}
			else
			{
				v[1] = -(CrtRand() % 0x1000);
				v[2] = CrtRand() % 0x1000;
			}
			Normalize(v, v);
			const int32_t sp = CrtRand() % spn + sp0;
			p->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
			p->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
			p->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
		}
	}

	// ------------------------------------------------------------------
	// Controller (0x600CC0): the choreography, flash, tint, the rings and bursts, the other tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl ControllerTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			uint16_t *flags = (uint16_t *)Caster();
			const uint16_t f = *flags;
			n->e = (int16_t)(f & 0x800);
			*flags = (uint16_t)(f | 0x800);
		}
		if (n->c == 1) BindBoth(Caster(), Target(), BIND_OUT, ANIM_SETS, (Finisher() != 0) + 1);
		if (n->c == 2)
		{
			// the length: the n + 1 stage durations of the sequence's opcode AB
			const int32_t last = (int32_t)StageCount() + 1;
			Length() = 0;
			for (int32_t i = 1; i <= last; i++) Length() = Length() + var<uint8_t>(0x1D98214 + i);
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_Trail);
			t->c = 0;
			for (uint32_t a = RING; a < RING_END; a += 0x14) var<int32_t>(a) = 0;
			t = (Node24 *)AddTaskToQueue(QMain(), ORIG_SparkSpawn);
			t->c = 0;
			t->e = (int16_t)(CrtRand() % 0x18);
			t = (Node24 *)AddTaskToQueue(QMain(), ORIG_Hit);
			t->c = 0;
		}
		if (Finisher() != 0 && (int32_t)n->c == Length())
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QHit(), ORIG_HitSpark);
			t->c = 0;
			t->w1A = (int16_t)CasterSlot();
		}
		if (n->c == 0x1A) AddRingAtHit(ORIG_RingA, 0x580);
		if (n->c >= 0x29 && n->c <= 0x32)
		{
			AddRingAtHit(ORIG_RingB, 0x600);
			DebrisBurst(4, 0xE00, 0x1000, false, 0xDC, 0x8C);
		}
		if (n->c == 0x33)
		{
			AddRingAtHit(ORIG_RingC, 0x1300);
			DebrisBurst(0x10, 0x1200, 0x1900, true, 0x140, 0xAA);
		}
		if (n->c == 0x54)
		{
			// 8 rings around the boss on the ground, delayed 2 k ticks
			int16_t p[4];
			GetDefaultEffectPosition(Target(), p);
			for (int32_t k = 0; k < 8; k++)
			{
				Node24 *t = (Node24 *)AddTaskToQueue(QMain(), ORIG_RingD);
				t->c = 0;
				t->e = (int16_t)(k + k);
				const int32_t rx = CrtRand() % 0x708;
				t->w12 = 0;
				t->w10 = (int16_t)(rx + p[0] - 0x384);
				const int32_t rz = CrtRand() % 0x708;
				t->w14 = (int16_t)(rz + p[2] - 0x384);
				const int16_t s = (int16_t)((CrtRand() % 0xD00 + 0x700) / 4);
				t->w1E = s;
				t->w1C = s;
				const int16_t sy = (int16_t)((CrtRand() % 0xB00 + 0x700) / 4);
				t->w22 = sy;
				t->w20 = sy;
			}
		}
		// screen flash: up over ticks 0..0x10, down over the last 8 (not before a finisher)
		if (n->c <= 0x10)
			SetScreenFlash((uint32_t)shl32(n->c, 7), 0);
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4 && Finisher() == 0)
			SetScreenFlash((uint32_t)shl32(Length() - n->c, 8), 0);
		// the caster's model tint: toward the far colour over ticks 0..8, back over the last 8
		bool tint = false;
		int32_t k = 0;
		if (n->c <= 8)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = n->c;
			tint = true;
		}
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = Length() - n->c;
			tint = true;
		}
		if (tint)
		{
			uint32_t colour = var<uint32_t>(0xB8B7D8); // (the original keeps it in a local)
			GteLoadRGBC(&colour);
			GteSetIR0(shl32(k, 9));
			GteDPCS();
			GteStoreRGB2(Caster() + 0x28);
		}
		if (n->c == 1) BdPlaySE(SOUND, 0x8000, 0x80);
		n->c++;
		if ((int32_t)n->c <= Length()) return 0;
		if (Finisher() == 0) SetScreenFlash(0, 0);
		*(uint32_t *)(Caster() + 0x28) = var<uint32_t>(0xB8B7D8);
		uint16_t *flags = (uint16_t *)Caster();
		*flags = (uint16_t)((*flags & 0xF7FF) | (uint16_t)n->e);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Hit sparks (0x6013C0, hit-effect queue): 6 sparkles per tick on ticks 4..8
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitSparkTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c >= 4 && n->c <= 8)
		{
			// the entity's default effect position with its height and y swapped: x, height, z, y
			int16_t p[4];
			GetDefaultEffectPosition(Entity(n->w1A), p);
			const int16_t h = p[3];
			p[3] = p[1];
			p[1] = h;
			for (int32_t i = 0; i < 6; i++)
			{
				Node24 *s = (Node24 *)AddTaskToQueue(QHit(), ORIG_Sparkle);
				s->c = 0;
				s->e = (int16_t)(CrtRand() % 6 + i);
				const uint8_t *sk = **(uint8_t ***)(Entity(n->w1A) + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				int16_t q[4];
				GetEffectSpawnPosition(Entity(n->w1A), bone, 0, q);
				q[0] = (int16_t)(q[0] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[1] = (int16_t)(q[1] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[2] = (int16_t)(q[2] + (int16_t)(CrtRand() % 0xAF - 0x57));
				int16_t y = q[1];
				if (y > -600)
				{
					y = (int16_t)(y - 600);
					q[1] = y;
				}
				int32_t v[3] = { (int32_t)q[0] - p[0], (int32_t)y - p[1], (int32_t)q[2] - p[2] };
				Normalize(v, v);
				v[0] = shl32(v[0], 2);
				v[2] = shl32(v[2], 2);
				Normalize(v, v);
				const int32_t len = CrtRand() % 0x258 + 0x15E;
				s->w10 = (int16_t)((mul32(len, v[0]) >> 12) + q[0]);
				s->w12 = (int16_t)((mul32(len, v[1]) >> 12) + q[1]);
				s->w18 = (int16_t)bone;
				s->w14 = (int16_t)((mul32(len, v[2]) >> 12) + q[2]);
				s->w1A = n->w1A;
				s->w1C = (int16_t)(CrtRand() % 8);
				s->w20 = (int16_t)(CrtRand() % 0x680 + 0x80);
			}
		}
		if (n->c == 1) AddAnimSeqTask(Entity(n->w1A), 3);
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparkle (0x601650, hit-effect queue): one model facing the camera, spinning, stretched toward its point
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		int16_t q[4];
		GetEffectSpawnPosition(Entity(n->w1A), n->w18, shl32(n->c, 9), q);
		int32_t d[3] = { (int32_t)var<int16_t>(0xB8B7F0) - q[0], 0, (int32_t)var<int16_t>(0xB8B7F4) - q[2] };
		int32_t u[3] = { 0, 0, -0x1000 };
		Normalize(d, d);
		int32_t axis[3];
		int32_t angle = RotationBetweenVectors(u, d, axis);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m1; // (pad +0x12 never written in the original: GTE R33 high half only)
		BuildRotationMatrixFromAngles(ang, &m1);
		Mat4x3 m2;
		m1.t[0] = 0xF0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		MatrixMultiply(&m2, &m1);
		// spin about the view direction, 0x40 per tick, its sense from the turn's axis
		int32_t spin = -shl32(n->c, 6);
		if (axis[1] < 0) spin = -spin;
		BuildAxisAngleRotationMatrix(spin, &m2, d);
		MatrixMultiply(&m2, &m1);
		// stretched toward the sparkle's point: up (0, -0x1000, 0) turned onto it, length * 0x1000 / 2000
		int32_t e[3] = { (int32_t)n->w10 - q[0], (int32_t)n->w12 - q[1], (int32_t)n->w14 - q[2] };
		u[0] = 0;
		u[1] = -0x1000;
		u[2] = 0;
		const int32_t len = SqrtInt(Normalize(e, e));
		angle = RotationBetweenVectors(u, e, axis);
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		m2.t[0] = q[0];
		m2.t[2] = q[2];
		int32_t sc[3];
		sc[2] = n->w20;
		sc[0] = n->w20;
		m2.t[1] = q[1];
		sc[1] = shl32(len, 12) / 2000;
		Scale3DMatrix(&m2, sc);
		ComposeAffineTransform(&Camera(), &m2, &m2);
		ComposeAffineTransform(&m2, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_SPARKLE;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0xC) = var<uint16_t>(SPARKLE_FADE + 2 * n->c);
		*(uint32_t *)(h + 0x18) = var<uint16_t>(SPARKLE_DEPTH + 2 * n->w1C);
		*(uint32_t *)(h + 0x1C) = 0x10F3;
		var<uint32_t>(0x1D8E054) = RenderPrimModel(h, RenderOT(), 2, var<uint32_t>(0x1D8E054));
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		n->w1C++;
		if (n->w1C >= 8) n->w1C = 0;
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Trail (0x601980)
	// ------------------------------------------------------------------
	// the weapon point and the tip 666 along the blade into the work block
	static void TrailPoints(TrailWork *w)
	{
		WeaponBone(Caster(), 1, w->dir, w->base);
		w->tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 666) >> 12) + w->base[0]);
		w->tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 666) >> 12) + w->base[1]);
		w->tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 666) >> 12) + w->base[2]);
	}

	// newest-first gather of at most `want` ring entries from slot idx into the two point lists; returns the count
	static int32_t TrailGather(const uint8_t *ring, int32_t idx, int32_t want, uint8_t *pa, uint8_t *pb)
	{
		int32_t cnt = 0;
		if (want <= 0) return 0;
		do
		{
			const RingEntry *e = (const RingEntry *)(ring + 0x14 * idx);
			if (!e->alive) break;
			idx--;
			*(uint32_t *)(pa + 8 * cnt) = e->base[0];
			*(uint32_t *)(pa + 8 * cnt + 4) = e->base[1];
			*(uint32_t *)(pb + 8 * cnt) = e->tip[0];
			*(uint32_t *)(pb + 8 * cnt + 4) = e->tip[1];
			if (idx < 0) idx = 6;
			cnt++;
		} while (cnt < want);
		return cnt;
	}

	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb)
	{
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sa + 8 * k, t / 31);
		SplineSetup(cnt, pb, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sb + 8 * k, t / 31);
		FieldFree(0x190);
	}

	// the hit point (the tip spline's 1st point, as 161) and the unit direction from the 2nd point to it
	static void TrailHitPoint(const uint8_t *sb, uint8_t *pos, int32_t *dir)
	{
		*(uint32_t *)pos = *(const uint32_t *)sb;
		*(uint32_t *)(pos + 4) = *(const uint32_t *)(sb + 4);
		dir[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)(sb + 8);
		dir[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sb + 0xA);
		dir[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sb + 0xC);
		Normalize(dir, dir);
	}

	// 30 quad pairs between the two splines: gouraud from the base colour 0x3A903030 fading toward the far colour
	// (black) along the trail, and a copy whose leading edge is 0x3A000000
	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk + 9;
		w->col1 = 0x3A903030;
		w->base_col = 0x3A903030;
		int32_t tacc = 0;
		for (int32_t off = 0; off < 0xF0; off += 8, tacc += 0x1000)
		{
			w->col0 = w->col1;
			pk[0] = 0x8000000;
			GteLoadV012(sa + off, sa + off + 8, sb + off);
			GteRTPT();
			GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb + off + 8);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(&w->otz);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(&w->base_col);
			GteDPCS();
			GteStoreRGB2(&w->col1);
			pk[5] = pk[1] = w->col0;
			pk[7] = pk[3] = w->col1;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk);
			pk2[5] = w->col0;
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = w->col1;
			pk2[4] = pk[4];
			pk2[2] = pk[2];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailPoints(w);
		const int32_t c = n->c;
		const int32_t idx = c % 7;
		RingEntry *e = (RingEntry *)(RING + 0x14 * idx);
		e->alive = 1;
		e->base[0] = *(const uint32_t *)&w->base[0];
		e->base[1] = *(const uint32_t *)&w->base[2];
		e->tip[0] = *(const uint32_t *)&w->tip[0];
		e->tip[1] = *(const uint32_t *)&w->tip[2];
		const int32_t want = c < Length() - 7 ? 7 : Length() - c;
		const int32_t cnt = TrailGather((const uint8_t *)RING, idx, want, (uint8_t *)PTS_A, (uint8_t *)PTS_B);
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)PTS_A, (const uint8_t *)PTS_B, (uint8_t *)SPL_A, (uint8_t *)SPL_B);
			TrailHitPoint((const uint8_t *)SPL_B, (uint8_t *)HIT_POS, (int32_t *)HIT_DIR);
			// 30 fps layer: see mag159_renzokuken_xatm_held.inc
			FX_HELD(held_note_trail(n);)
			TrailDraw((const uint8_t *)SPL_A, (const uint8_t *)SPL_B, w);
		}
		FieldFree(0x40);
		if (DrawOnly()) return 0;
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark spawner (0x601EC0): 3 sparks per tick at random bones of the caster
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkSpawnTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if ((int32_t)n->c < Length() - 8)
		{
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(POOL_B, POOL_B_SPAWN_END, 0x10);
				if (i < 0) break;
				SparkP *p = (SparkP *)(POOL_B + 0x10 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x300 + 0x400);
				const uint8_t *sk = **(uint8_t ***)(Caster() + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				GetEffectSpawnPosition(Caster(), bone, 0, p->pos);
				p->vy = (int16_t)(CrtRand() % 10 + 2);
				p->frame = (int16_t)((int32_t)n->e % 0x18);
			}
		}
		n->c++;
		n->e++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Hits (0x601FF0): per hit, a slash, 3 flashes and 12 debris at the trail's hit point
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		int32_t sum = 0;
		for (int32_t b = 0; b < (int32_t)StageCount(); b++)
		{
			const int32_t lead = var<int32_t>(HIT_LEAD + 4 * b);
			sum += StageLen(b);
			if ((int32_t)n->c != sum - lead) continue;
			Node24 *s = (Node24 *)AddTaskToQueue(QMain(), ORIG_Slash);
			s->c = 0;
			s->e = (int16_t)b;
			const int16_t sc = (int16_t)(CrtRand() % 0x300 + 0x500);
			s->w1E = sc;
			s->w1C = sc;
			HitRec *r = (HitRec *)(HIT_REC + 0x9C * b);
			memcpy(r->pos, (const void *)HIT_POS, 8);
			memcpy(r->dir, (const void *)HIT_DIR, 16);
			r->count = 0x10;
			for (int32_t k = 0; k < r->count; k++)
			{
				r->angle[k] = (int16_t)(CrtRand() % 0x1000);
				const int32_t wv = CrtRand() % 0xC00 + 0xC00;
				r->width[k] = (int16_t)wv;
				r->off[k] = (int16_t)(mul32(wv, 1000) >> 12);
				r->speed[k] = (int16_t)(CrtRand() % 200 + 0x78);
			}
			// 3 flashes around the hit point
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(POOL_A, POOL_A_END, 0xC);
				if (i < 0) break;
				FlashP *p = (FlashP *)(POOL_A + 0xC * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x600 + 0xB00);
				memcpy(p->pos, (const void *)HIT_POS, 8);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x226 - 0x113));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x226 - 0x113));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x226 - 0x113));
			}
			// 12 debris thrown along the direction with a random part +- spread (back the other way on hit 0)
			const int32_t spread = var<int32_t>(HIT_SPREAD + 4 * b);
			for (int32_t j = 0; j < 0xC; j++)
			{
				const int32_t i = FreeSlot(POOL_C, POOL_C_END, 0x14, 0x50);
				if (i < 0) break;
				DebrisP *p = (DebrisP *)(POOL_C + 0x14 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0xD00 + 0x1200);
				memcpy(p->pos, (const void *)HIT_POS, 8);
				const int32_t sp = CrtRand() % 200 + 0x8C;
				const int32_t *d = (const int32_t *)HIT_DIR;
				// hit 0: thrown back (the original negates the three words after storing them; written as a sign here:
				// MSVC /O1 dropped the store-then-negate form of this loop)
				const int16_t sgn = b == 0 ? -1 : 1;
				const int32_t r0 = CrtRand() % shl32(spread, 1);
				p->vel[0] = (int16_t)(sgn * (int16_t)((int16_t)(r0 + (mul32(d[0], sp) >> 12)) - (int16_t)spread));
				const int32_t r1 = CrtRand() % shl32(spread, 1);
				p->vel[1] = (int16_t)(sgn * (int16_t)((int16_t)(r1 + (mul32(d[1], sp) >> 12)) - (int16_t)spread));
				const int32_t r2 = CrtRand() % shl32(spread, 1);
				p->vel[2] = (int16_t)(sgn * (int16_t)((int16_t)(r2 + (mul32(d[2], sp) >> 12)) - (int16_t)spread));
			}
		}
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Slash (0x602390): the flash along the hit direction and its 16 streaks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		HitRec *r = (HitRec *)(HIT_REC + 0x9C * n->e);
		// 30 fps layer: see mag159_renzokuken_xatm_held.inc
		FX_HELD(held_note_slash(n, r);)
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)SLASH_REF, r->dir, axis);
		Mat4x3 m; // the hit frame through the camera
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = r->pos[0];
		m.t[1] = r->pos[1];
		m.t[2] = r->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m3;
		BuildRotationMatrixFromAngles(ang, &m3);
		int32_t sv[3];
		m3.t[0] = 0;
		m3.t[1] = 0;
		m3.t[2] = 0;
		sv[1] = n->w1C;
		sv[0] = n->w1C;
		sv[2] = 0x1000;
		Scale3DMatrix(&m3, sv);
		ComposeAffineTransform(&m, &m3, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_SLASH;
		*(uint32_t *)(h + 0x1C) = 0x30;
		if (n->c >= 2)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(int32_t *)(h + 0xC) = mul32(n->c - 2, 682);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		*(uint32_t *)h = MODEL_STREAK;
		*(uint32_t *)(h + 0x1C) |= 3;
		for (int32_t k = 0; k < r->count; k++)
		{
			ang[2] = r->angle[k];
			BuildRotationMatrixFromAngles(ang, &m3);
			int16_t v[4]; // (4th word never written in the original: GTE VZ0 high half only)
			v[3] = 0;
			v[2] = 0;
			v[0] = 0;
			v[1] = r->off[k];
			GteLoadV0(v);
			GteSetRotMatrix(&m3);
			GteMVMVA_RotV0();
			GteReadMAC123(m3.t);
			const int32_t wd = r->width[k];
			sv[1] = wd;
			sv[0] = (wd >> 1) + wd;
			Scale3DMatrix(&m3, sv);
			ComposeAffineTransform(&m, &m3, &m3);
			GteSetRotMatrix(&m3);
			GteSetTransVector(&m3);
			PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
			if (!DrawOnly())
			{
				r->off[k] = (int16_t)(r->off[k] + r->speed[k]);
				r->speed[k] = (int16_t)(r->speed[k] - (int16_t)(r->speed[k] >> 4));
			}
		}
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - rate / 6);
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Rings (0x602670 / 0x6027C0 / 0x602910 / 0x602A50)
	// ------------------------------------------------------------------
	// the draw of a ring: model at (w10, w12, w14), scaled by sx (x, and z but for ring B: 0x800) and sy (y), fading from tick 4
	static void RingDraw(int kind, const Node24 *n, int32_t sx, int32_t sy, int32_t fade)
	{
		int16_t a[3] = { 0, 0, 0 };
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only)
		BuildRotationMatrixFromAngles(a, &m);
		int32_t sv[3];
		m.t[1] = n->w12;
		m.t[0] = n->w10;
		sv[2] = kind == RING_B ? 0x800 : sx;
		sv[1] = sy;
		sv[0] = sx;
		m.t[2] = n->w14;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = RINGS[kind].model;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (n->c >= 4)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	// the fade of tick c (from tick 4)
	static int32_t RingFade(int kind, int32_t c) { return shl32(mul32(c - 4, RINGS[kind].fade_mul), RINGS[kind].fade_shift); }
	// the scale of a ring's axes from its node
	static int32_t RingSX(const Node24 *n) { return n->w1C; }
	static int32_t RingSY(int kind, const Node24 *n) { return kind == RING_D ? n->w20 : n->w1C; }
	// rate - rate / k (rounded toward 0)
	static int16_t RingSlow(int16_t rate, int32_t k) { return (int16_t)(rate - (int16_t)(rate / k)); }

	static uint32_t RingTask(int kind, Node24 *n)
	{
		// 30 fps layer: see mag159_renzokuken_xatm_held.inc
		FX_HELD(held_note_ring(kind, n);)
		RingDraw(kind, n, RingSX(n), RingSY(kind, n), RingFade(kind, n->c));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = RingSlow(rate, RINGS[kind].rate_div);
		if (kind == RING_D)
		{
			const int16_t ry = n->w22;
			n->w20 = (int16_t)(n->w20 + ry);
			n->w22 = RingSlow(ry, 3);
		}
		n->c++;
		return n->c >= RINGS[kind].ticks ? TASK_END : 0;
	}
	static uint32_t __cdecl RingATask(TaskNode *tn) { return RingTask(RING_A, (Node24 *)tn); }
	static uint32_t __cdecl RingBTask(TaskNode *tn) { return RingTask(RING_B, (Node24 *)tn); }
	static uint32_t __cdecl RingCTask(TaskNode *tn) { return RingTask(RING_C, (Node24 *)tn); }
	static uint32_t __cdecl RingDTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		return RingTask(RING_D, n);
	}

	// ------------------------------------------------------------------
	// Particles (0x602C90): flashes (pool A), debris (pool C), sparks (pool B)
	// ------------------------------------------------------------------
	static void ParticleDraws()
	{
		// 30 fps layer: see mag159_renzokuken_xatm_held.inc
		FX_HELD(held_note_particles();)
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_FLASH;
		*(int16_t *)(h + 0x24) = 0;
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0xC)
		{
			FlashP *p = (FlashP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0) p->life = -1;
		}
		*(uint32_t *)h = SEQ_DEBRIS;
		for (uint32_t a = POOL_C; a < POOL_C_END; a += 0x14)
		{
			DebrisP *p = (DebrisP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->scale = (int16_t)(p->scale - (int16_t)(p->scale >> 5));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 3));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 3));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 3));
		}
		*(uint32_t *)h = SEQ_SPARK;
		*(int16_t *)(h + 0x24) = 4;
		for (uint32_t a = POOL_B; a < POOL_B_END; a += 0x10)
		{
			SparkP *p = (SparkP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(SPARK_FRAMES + 4 * p->frame);
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->pos[1] = (int16_t)(p->pos[1] + p->vy);
			p->frame++;
			if (p->frame >= 0x18) p->frame = 0;
		}
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Master (0x602C40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter & 1) PacketCursor() = TexBase() + 0x10000;
		else PacketCursor() = TexBase();
		// 30 fps layer: see mag159_renzokuken_xatm_held.inc
		FX_HELD(held_note_master();)
		const int alive = ExecuteTaskQueue(QMain());
		ParticleDraws();
		r->counter++;
		return alive ? 0 : TASK_END;
	}
}

	void register_mag159_renzokuken_xatm()
	{
		register_port(renzo159::ORIG_Master, (void *)renzo159::MasterTask, "159 Master", 159);
		register_port(renzo159::ORIG_Controller, (void *)renzo159::ControllerTask, "159 Controller", 159);
		register_port(renzo159::ORIG_Trail, (void *)renzo159::TrailTask, "159 Trail", 159);
		register_port(renzo159::ORIG_SparkSpawn, (void *)renzo159::SparkSpawnTask, "159 SparkSpawn", 159);
		register_port(renzo159::ORIG_Hit, (void *)renzo159::HitTask, "159 Hit", 159);
		register_port(renzo159::ORIG_Slash, (void *)renzo159::SlashTask, "159 Slash", 159);
		register_port(renzo159::ORIG_RingA, (void *)renzo159::RingATask, "159 RingA", 159);
		register_port(renzo159::ORIG_RingB, (void *)renzo159::RingBTask, "159 RingB", 159);
		register_port(renzo159::ORIG_RingC, (void *)renzo159::RingCTask, "159 RingC", 159);
		register_port(renzo159::ORIG_RingD, (void *)renzo159::RingDTask, "159 RingD", 159);
		register_port(renzo159::ORIG_HitSpark, (void *)renzo159::HitSparkTask, "159 HitSpark", 159);
		register_port(renzo159::ORIG_Sparkle, (void *)renzo159::SparkleTask, "159 Sparkle", 159);
		// 30 fps layer: see mag159_renzokuken_xatm_held.inc
		FX_HELD(register_mag159_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag159_renzokuken_xatm_held.inc"
#endif
