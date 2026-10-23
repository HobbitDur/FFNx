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

// Effect 197: Armor Shot (Irvine's Shot limit break, kernel.bin section 21 entry 6 +4; MAG_197_SHOT__ARMOR_SHOT).
//
// How the battle drives it (as Normal Shot 188, see mag188_normal_shot.cpp): the opening action is a normal magic cast (mode 0);
// every trigger press of the Shot window becomes an action that BattleActionSequence_Tick_Shot 0x50BEE0 runs: it calls the
// module's _Init again with g_MagicCastContext 0x1D99A78 +8 = 2 (a shot) or 1 (the closing action) and waits for +9 done = 1.
//
// Structure (file loader 0x5AED10 = the texture file 0xD0527C; _Init 0x5AED30 keeps the texture base 0x2265DA4, the context
// 0x2264518 and the caster slot 0x2265B00 (context byte 0), then SetupByMode 0x5AED60 (both run original): mode 0 = the root
// pool 0x2260BC0 (4 x 0x10, queue 0x2261A10) with RootTask, the aux queue 0x22636F8 (pool 0x2263708, 100 x 0x24), the shot queue
// 0x2261A20 (pool 0x2260C00, 100 x 0x24) with StartTask, flags 0x22636F0 / shot index 0x2260BB8 / shot count 0x2265D9C / particle
// id count 0x2265DA0 / arc count 0x2265DAC / module tick 0x2260BB0 cleared, both particle arrays (0x2261A30 / 0x2262860, 150 x
// 0x18) free, resetSomeCameraData, the TIM upload; mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue:
// +0x0E = shot count % 100 (its shot record 0x2264520 + 0x38 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the
// first target's slot):
//   RootTask (0x5B17F0) - packet cursor 0x2265DA8 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); every tick the aux queue is empty after EndTask finished (flag 2): a
//     RestoreTask in the root queue; ends when both queues are empty and flag 2 is set.
//   RestoreTask (0x5B18A0, root queue) - Battle_RestoreDefaultEffectTexture (0x508630) on its counter 1; ends when it is done.
//   StartTask (0x5B13C0, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xCFB348, out block 0x2262840), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x2260468 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5AEEC0, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his chain
//     (+0x18) and flag 4; the muzzle (bone 2 of his weapon model, entity +0x78, through 0x5AF6E0 / 0x5AF760 = the Renzokuken
//     finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000) to the
//     direction (+0x18); its own +0x10 = the target's effect anchor (GetDefaultEffectPosition 0x571400) with y clamped to the
//     muzzle's (never below it), the record's +8 = the direction muzzle -> anchor, normalized; the distance (Math_SqrtInt of
//     the squared length) is kept in its own argument slot (reproduced); +0x16 = the target's size * 2000 / 4096, +0x1C = the
//     target's size clamped to 0x600..0x1100 (0x5AF6B0) * 2000 / 4096; MuzzleFlashTask (yaw, scale = step = (0xA80..0xDFF) / 3),
//     MuzzleConeTask (roll, scale = step = (0xA00..0xBFF) / 3), GroundFlashTask (yaw, scale 0x980..0xDFF), BoneSmokeTask (the
//     next particle id), 5 x PathRingTask along the way at 1 / 6 .. 5 / 6 of the distance (delay 0..4, roll, spin
//     0x78..0x153, scale = step = ((0xE00..0x117F) * (0x1000 - sin(0x200 * k / 5)) >> 12) / 3, depth scale 0x1100..0x16FF),
//     BeamTask (scale 0x480, length = its step = the distance / 2); counter 1: ImpactRingTask (roll, scale = step = (0x500..0x77F)
//     / 3; that size is kept in its own argument slot, reproduced) and ImpactSpinTask (roll, spin 0xA0..0x1A3, scale = step =
//     (that size + 0x300) / 4) at the anchor, the gun sound 0xD05230 (volume 1 / 2 alternating); counter 6: ArmorFlashTask (yaw,
//     scale = step = ((0xA00..0xE7F) * (+0x1C * 4096 / 2400) >> 12) / 3), DebrisTask and TargetSparkTask (each the next particle
//     id; the target sparks with the anchor's +0x16 and the target slot), 3 x ArcTask (the next arc record, arc count % 8; the
//     target slot), the action result (ApplyActionResultToTargets 0x506BA0); counter 22: done, flag 4 cleared; ends at 25.
//   MuzzleFlashTask (0x5AF7E0) - prim model 0xD012B0 at the shot record (its rotation x yaw), uniform scale; scale + step, step -
//     step / 3; faded (counter - 2) << 9 from counter 2; 10 ticks.
//   MuzzleConeTask (0x5AF950) - prim model 0xD03940 at the shot record (its rotation x roll), uniform scale; scale + step, step -
//     step / 5; faded (counter - 6) << 9 from counter 6; 14 ticks.
//   GroundFlashTask (0x5AFAC0) - prim model 0xD02180 on the ground under the shot record (x, 0, z), yawed, uniform scale; faded
//     (counter - 2) << 9 from counter 2 (only the fade changes); 10 ticks.
//   BoneSmokeTask (0x5AFBF0) - counters 0..3 (not on draw-only ticks): 2 x a pair of particles at Irvine's effect points 0x1D /
//     0x1E (GetEffectSpawnPosition 0x502170, angle 0xC00, offset +-25, size 0x320..0x59F; the second of a pair takes the entry
//     AFTER the free one found, unchecked: vanilla); then every particle of its array (id parity) with its id: sequence 0xCFDA74
//     at its frame, its point through the camera (light matrix), R = its size, pulled toward the camera by size / 16; then (not
//     on draw-only ticks) frame + 1, dies when its sequence ended, else size + size / 16; ends when none is left.
//   PathRingTask (0x5B0160) - a delay first (+0x16 ticks, nothing drawn); prim model 0xD04B88 at its point on the way (rotation
//     (0, 0, -0x1000) -> the shot direction x roll), scale (scale, scale, depth +0x20); roll + spin, spin - spin / 10, scale +
//     step, step - step / 4; faded (counter - 4) << 9 from counter 4; 12 ticks.
//   BeamTask (0x5AFF30) - prim model 0xCFDD18 at the shot record (rotation (0, 0, -0x1000) -> the shot direction), scale (0x480,
//     0x480, length * 4096 / 6000), then (the same header) prim model 0xCFEDE8 camera facing at the muzzle + direction * length,
//     a random size 0x800..0xAFF every draw (draw-only ticks too; depth -(size / 8)); faded (counter - 4) << 9 from counter 4;
//     length + its step on counters 0..3; 12 ticks.
//   ImpactRingTask (0x5B0340) - prim model 0xD02570 at the anchor, camera facing translation (TransformCameraByShadowRotation
//     0x571BC0 with its scale, depth -(+0x16)), rotation (0, 0, 0x1000) -> the shot direction x roll, uniform scale; scale +
//     step, step - step / 4; faded (counter - 2) << 10 from counter 2; 6 ticks.
//   ImpactSpinTask (0x5B04C0) - prim model 0xD03298 as ImpactRingTask's (from (0, 0, -0x1000)); roll + spin, spin - spin / 16,
//     scale + step, step - step / 4; faded (counter - 6) * 0x2AA from counter 6; 12 ticks.
//   ArmorFlashTask (0x5B0660) - prim model 0xCFF2F8 camera facing at the anchor (TransformCameraByShadowRotation with its scale,
//     depth -(+0x16); its yawed scaled matrix is built and not used: vanilla); scale + step, step - step / 3; faded (counter -
//     6) * 0x2AA from counter 6; 12 ticks.
//   DebrisTask (0x5B0780) - every particle of its array (id parity) with its id: sequence 0xCFDC20 (+0x24 = 0x208) at the anchor
//     + its offset, scale (1, size, 1) turned from (0, -0x1000, 0) onto its velocity (0x5B0B60: identity); then (not on
//     draw-only ticks) frame + 1, dies when its sequence ended, else offset + velocity, velocity - velocity / 4; counters 0..2
//     (not on draw-only ticks, AFTER the draws): 8 new particles (random direction, offset 0x12C..0x2BB, speed 0xDC..0x1DF, size
//     0x1400..0x23FF); ends past counter 3 with no particle left.
//   TargetSparkTask (0x5B0B90) - Dark Shot 193's (0x5B8120) with 3 new particles on counters 0..2 (size 0x580..0xF7F) and
//     sequence 0xCFD714.
//   ArcTask (0x5B0E80; nothing on draw-only ticks) - counter 0: an arc record (0x2260470 + 0xE8 * its +0x18): A = a random
//     effect anchor of the target (random angle), B = A + (A - its point, normalized) * (0x5DC..0xF9F) raised by (0x1F4..0x6A3) /
//     2, the control points A, the raised midpoint (by 0x1F4..0x6A3), B; 24 spline points (Effect_SplineSetup 0x571620 /
//     Effect_SplineEval 0x571690, t = i * 0x1000 / 23); an ArcSparkTask (the next particle id, head 0, speed 5); 10 ticks.
//   ArcSparkTask (0x5B10C0) - every particle of its array (id parity; the array pointer kept in its own argument slot,
//     reproduced) with its id: sequence 0xCFD714 at frame * 2 + 1, camera facing at its point (its size, depth -(size / 16));
//     then (not on draw-only ticks) frame + 1, dies when its sequence ended; then (not on draw-only ticks) head + speed, speed - 1
//     down to 2; while the head is below 24: a particle (size 0x480..0x67F) on each spline point from the head down to head -
//     speed; ends past counter 3 with no particle left.
//   EndTask (0x5B12A0, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick; counter 6: the action result; after 14 ticks: flag 2, done; ends.
//   FadeInTask (0x5B14B0) / FadeOutTask (0x5B1360) - screen flash (Effect_SetScreenFlash 0x5713E0, colour 0) 0 -> 0x800 / 0x800
//     -> 0 over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5B1500) - the weapon's ghosts as Normal Shot's (ghost slots 0x2265B08, current bones 0x2263670, colours
//     0xD05268; BoneSnapshot 0x5B16B0 / BoneRestore 0x5B16F0 / GhostDraw 0x5B1730; the ghost count kept in its own argument slot).
// Every task except RootTask / RestoreTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// Never-written words: the shot record's +6 and +0x14 (never read), an arc record's +6 / +0xE / +0x1E (A's / B's 4th word and the
//   midpoint's: copied, never read by the spline), the target sparks' node +0x10..+0x15 (never read).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x2260468..0x2265DB0 (end tick, arc records 0x2260470 (8 x 0xE8), module tick, shot index, pools, queues,
//   particle arrays, bind block, saved bones 0x2263670, flags 0x22636F0, aux queue, context, shot records, caster slot, texture
//   file, ghost slots, counts, texture base, packet cursor, arc count).

#include "mag_common.h"

namespace ff8fx
{
namespace armor197
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x2260468); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x2260BB0); }
	inline uint32_t &Flags() { return var<uint32_t>(0x22636F0); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x2264518); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x2265B00); }
	inline int32_t &IdCount() { return var<int32_t>(0x2265DA0); }       // the next particle id - 1
	inline uint32_t &TexBase() { return var<uint32_t>(0x2265DA4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2265DA8); }
	inline int32_t &ArcCount() { return var<int32_t>(0x2265DAC); }      // the next arc record (% 8)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2261A10; }
	inline TaskQueue *QShots() { return (TaskQueue *)0x2261A20; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x22636F8; }
	static const uint32_t PARTS_A = 0x2261A30, PARTS_B = 0x2262860;  // 150 x 0x18 each
	static const int32_t PARTS_N = 150;
	static const uint32_t SHOTS = 0x2264520;                         // 100 x 0x38 shot records
	static const uint32_t ARCS = 0x2260470;                          // 8 x 0xE8 arc records
	static const int32_t ARC_POINTS = 24;
	static const uint32_t BIND_OUT = 0x2262840;
	static const uint32_t BONES_NOW = 0x2263670;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x2265B08, GHOSTS_END = 0x2265D9C; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;

	static const uint32_t ORIG_RootTask = 0x5B17F0;
	static const uint32_t ORIG_RestoreTask = 0x5B18A0;
	static const uint32_t ORIG_StartTask = 0x5B13C0;
	static const uint32_t ORIG_ShotTask = 0x5AEEC0;
	static const uint32_t ORIG_EndTask = 0x5B12A0;
	static const uint32_t ORIG_FadeOutTask = 0x5B1360;
	static const uint32_t ORIG_FadeInTask = 0x5B14B0;
	static const uint32_t ORIG_AfterimageTask = 0x5B1500;
	static const uint32_t ORIG_MuzzleFlashTask = 0x5AF7E0;
	static const uint32_t ORIG_MuzzleConeTask = 0x5AF950;
	static const uint32_t ORIG_GroundFlashTask = 0x5AFAC0;
	static const uint32_t ORIG_BoneSmokeTask = 0x5AFBF0;
	static const uint32_t ORIG_BeamTask = 0x5AFF30;
	static const uint32_t ORIG_PathRingTask = 0x5B0160;
	static const uint32_t ORIG_ImpactRingTask = 0x5B0340;
	static const uint32_t ORIG_ImpactSpinTask = 0x5B04C0;
	static const uint32_t ORIG_ArmorFlashTask = 0x5B0660;
	static const uint32_t ORIG_DebrisTask = 0x5B0780;
	static const uint32_t ORIG_TargetSparkTask = 0x5B0B90;
	static const uint32_t ORIG_ArcTask = 0x5B0E80;
	static const uint32_t ORIG_ArcSparkTask = 0x5B10C0;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xCFB348;
	static const uint32_t MODEL_MuzzleFlash = 0xD012B0, MODEL_Cone = 0xD03940, MODEL_GroundFlash = 0xD02180, MODEL_PathRing = 0xD04B88;
	static const uint32_t MODEL_Beam = 0xCFDD18, MODEL_BeamHead = 0xCFEDE8, MODEL_ImpactRing = 0xD02570, MODEL_ImpactSpin = 0xD03298;
	static const uint32_t MODEL_Armor = 0xCFF2F8;
	static const uint32_t SEQ_BoneSmoke = 0xCFDA74, SEQ_Debris = 0xCFDC20, SEQ_Spark = 0xCFD714;
	static const void *const SOUND_Gun = (const void *)0xD05230;
	static const int32_t *const DIR_BACK = (const int32_t *)0xD05238;  // (0, 0, -0x1000)
	static const int32_t *const DIR_FRONT = (const int32_t *)0xD05248; // (0, 0, 0x1000)
	static const int32_t *const DIR_DOWN = (const int32_t *)0xD05258;  // (0, -0x1000, 0)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD05268;

	// engine functions not in fx_port.h / mag_common.h
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t angle, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, angle, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint (returns the squared length)
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); } // Math_SqrtInt
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = m.t
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }             // MAC = LLM * V0 + BK
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }   // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); } // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }

	// ------------------------------------------------------------------
	// Node / record layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool: 4 x 0x10 (RootTask, EndTask, RestoreTask)
	{
		TaskNode hdr;
		int16_t counter;
		int16_t done;    // +0x0E RestoreTask: set by Battle_RestoreDefaultEffectTexture
	};
	struct Node // shot / aux queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t f0E;     // +0x0E shot: shot number; muzzle / path / beam / impact tasks: their shot record; particle tasks: their
		                 //       particle id - 1; arc spark: its arc record; fades: length
		int16_t pos[4];  // +0x10 shot / impact tasks / arcs: the target's anchor (+0x16: the target's size * 2000 / 4096); path
		                 //       ring: its point (+0x16: its delay); arc spark: +0x16 = its particle id - 1
		int16_t f18;     // +0x18 shot: chain; models: yaw / roll; arc: its arc record; arc spark: its head; fades: level
		int16_t f1A;     // +0x1A shot / target sparks / arcs: target slot; path ring / impact spin: spin; arc spark: speed
		int16_t f1C;     // +0x1C scale / size (shot: the clamped target size)
		int16_t f1E;     // +0x1E scale step
		int16_t f20;     // +0x20 path ring: depth scale; beam: its length
		int16_t f22;     // +0x22 beam: its length step
	};
	struct ShotRec // 0x2264520 + 0x38 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		int32_t dir[3];  // the direction muzzle -> the target's anchor, normalized
		uint8_t pad14[4];
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct ArcRec // 0x2260470 + 0xE8 * n
	{
		int16_t a[4];    // a random effect anchor of the target (a[3] never written)
		int16_t b[4];    // the far end (b[3] never written)
		int16_t ctrl[3][4]; // A, the raised midpoint (ctrl[1][3] never written), B
		int16_t pts[ARC_POINTS][4];
	};
	struct Particle // 0x2261A30 / 0x2262860, 150 x 0x18
	{
		int32_t id;      // 0 = free, else the task's particle id
		int16_t frame;
		int16_t size;
		int16_t pos[4];  // (debris: its offset from the anchor; pos[3] never written)
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x2265B08, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Armor Shot nodes");
	static_assert(sizeof(ShotRec) == 0x38 && sizeof(ArcRec) == 0xE8 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Armor Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x38 * i); }
	inline ArcRec *Arc(int32_t i) { return (ArcRec *)(ARCS + 0xE8 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t k) { return (Particle *)((k & 1) ? PARTS_B : PARTS_A); }
	inline int32_t FindFree(const Particle *arr) { for (int32_t i = 0; i < PARTS_N; i++) if (arr[i].id == 0) return i; return -1; }
}
}

#ifdef FF8_FX_HELD
#include "mag197_armor_shot_held.h"
#endif

namespace ff8fx
{
namespace armor197
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5AF760 / 0x5AF6E0 = Rough Divide's 0x5F6500 / 0x5F6480)
	// ------------------------------------------------------------------
	// bone `bone` of an animation header's skeleton: the direction (0, 0, -0x1000) through its matrix, normalized, and its point
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

	// the entity's weapon (+0x78) bone in world space (nothing written without a weapon)
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

	// the target's size clamped to 0x600..0x1100 (0x5AF6B0)
	static int32_t ClampedSize(const uint8_t *entity)
	{
		const int32_t s = *(const int16_t *)(entity + 0x26);
		if (s > 0x1100) return 0x1100;
		if (s < 0x600) return 0x600;
		return s;
	}

	// 0x5B0B60 (= Ultra Waves' identity helper): the 3x3 identity (the translation untouched)
	static void IdentityRotation(Mat4x3 *m)
	{
		uint32_t *w = (uint32_t *)m;
		w[3] = 0;
		w[2] = 0;
		w[1] = 0;
		w[0] = 0;
		m->m[2][2] = 0x1000;
		m->m[1][1] = 0x1000;
		m->m[0][0] = 0x1000;
	}

	// ------------------------------------------------------------------
	// Afterimages (0x5B16B0 / 0x5B16F0 / 0x5B1730)
	// ------------------------------------------------------------------
	// the weapon's bone matrices (0x20 bytes of each 0x30-byte bone record) into dst
	static void BoneSnapshot(uint8_t *w, uint8_t *dst)
	{
		const uint8_t *sk = *(const uint8_t **)*(uint8_t **)(w + 4);
		const int32_t n = sk[0];
		const uint8_t *src = sk + 0x20;
		for (int32_t k = 0; k < n; k++, src += 0x30, dst += 0x20) memcpy(dst, src, 0x20);
	}
	// ... and back
	static void BoneRestore(uint8_t *w, const uint8_t *src)
	{
		uint8_t *sk = *(uint8_t **)*(uint8_t **)(w + 4);
		uint8_t *dst = sk + 0x20;
		for (int32_t k = 0; k < (int32_t)sk[0]; k++, src += 0x20, dst += 0x30) memcpy(dst, src, 0x20);
	}

	// the entity's weapon model alone under its world matrix, with its current bone matrices and colour word +0x28
	static void GhostDraw(uint8_t *entity)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		uint8_t *w = *(uint8_t **)(entity + 0x78);
		if (w && !(entity[1] & 2))
		{
			Mat4x3 *m = (Mat4x3 *)h;
			ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), m);
			*(uint16_t *)(h + 0x34) = 0;
			*(uint16_t *)(h + 0x36) = 0;
			*(uint32_t *)(h + 0x24) = var<uint32_t>(0x1D98B3C);
			*(uint32_t *)(h + 0x3C) = *(const uint32_t *)(entity + 0x28);
			*(uint16_t *)(h + 0x38) = 0x140;
			*(uint16_t *)(h + 0x3A) = 0xD8;
			*(uint32_t *)(h + 0x30) = var<uint32_t>(0x1D969A8);
			*(uint32_t *)(h + 0x40) = 0xFFFFFFFF;
			const uint8_t c = entity[7];
			h[0x4A] = c;
			h[0x49] = c;
			h[0x48] = c;
			ComposeAffineTransform(m, (const Mat4x3 *)(w + 0x24), m);
			ComputeBonesWorldMatrices(w, m);
			FramePackets() = RenderGeometry(*(const uint32_t *)(w + 4), h + 0x20, RenderOT(), 4, FramePackets());
		}
		FieldFree(0x4C);
	}

	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	// prim model header (0x58 bytes on the scratch stack): +0x0C fade, +0x1C flags (0x33 / faded 0xF3)
	static uint8_t *ModelHeader(uint32_t model, bool faded, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (faded)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		return h;
	}
	static void ModelDraw(uint32_t model, bool faded, int32_t fade)
	{
		uint8_t *h = ModelHeader(model, faded, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	// the GTE rotation / translation = the camera x m
	static void ModelMatrix(Mat4x3 *m)
	{
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	static int32_t FadeFrom2(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 9); }
	static int32_t FadeFrom4(int32_t c) { return (int32_t)((uint32_t)(c - 4) << 9); }
	static int32_t FadeFrom6(int32_t c) { return (int32_t)((uint32_t)(c - 6) << 9); }
	static int32_t IringFade(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 10); }
	static int32_t Fade2AA(int32_t c) { return mul32(c - 6, 0x2AA); }

	// a muzzle model at the shot record: its rotation x ZYX(ang), uniform scale
	static void MuzzleModel(const Node *n, int16_t yaw, int16_t roll, uint32_t model, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = yaw;
		ang[2] = roll;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply(&s->m, &m);
		m.t[0] = s->pos[0];
		m.t[1] = s->pos[1];
		m.t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		ModelMatrix(&m);
		ModelDraw(model, faded, fade);
	}

	// the ground flash under the shot record (x, 0, z), yawed, uniform scale
	static void GroundFlashDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = s->pos[0];
		m.t[1] = 0;
		m.t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		ModelMatrix(&m);
		ModelDraw(MODEL_GroundFlash, faded, fade);
	}

	// a path ring at its point: rotation (0, 0, -0x1000) -> the shot direction x roll, scale (scale, scale, depth)
	static void PathRingDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->f18;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(DIR_BACK, s->dir, axis);
		Mat4x3 r;
		BuildAxisAngleRotationMatrix(angle, &r, axis);
		GteMatrixMultiply(&r, &m);
		m.t[1] = n->pos[1];
		m.t[0] = n->pos[0];
		m.t[2] = n->pos[2];
		const int32_t v[3] = { n->f1C, n->f1C, n->f20 };
		Scale3DMatrix(&m, v);
		ModelMatrix(&m);
		ModelDraw(MODEL_PathRing, faded, fade);
	}

	// the beam: the stretched model from the muzzle (length * 4096 / 6000 deep), then (same header) the head model camera facing
	// at the muzzle + direction * length with a size of `head_size` (a new random 0x800..0xAFF when it is < 0, as the original
	// does on every draw); returns the head size drawn
	static int16_t BeamDraw(const Node *n, bool faded, int32_t fade, int32_t head_size)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(DIR_BACK, s->dir, axis);
		Mat4x3 r;
		BuildAxisAngleRotationMatrix(angle, &r, axis);
		GteMatrixMultiply(&r, &m);
		m.t[0] = s->pos[0];
		m.t[1] = s->pos[1];
		m.t[2] = s->pos[2];
		const int32_t v[3] = { n->f1C, n->f1C, ((int32_t)n->f20 << 12) / 6000 };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ModelHeader(MODEL_Beam, faded, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		int16_t p[4];
		p[0] = (int16_t)((mul32(s->dir[0], n->f20) >> 12) + s->pos[0]);
		p[1] = (int16_t)((mul32(s->dir[1], n->f20) >> 12) + s->pos[1]);
		p[2] = (int16_t)((mul32(s->dir[2], n->f20) >> 12) + s->pos[2]);
		p[3] = (int16_t)(head_size < 0 ? CrtRand() % 0x300 + 0x800 : head_size);
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_head(p[3]);)
		TransformCameraByShadowRotation(p, p[3], -((int32_t)p[3] >> 3));
		*(uint32_t *)h = MODEL_BeamHead;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
		return p[3];
	}

	// a model at the node's anchor: camera facing translation (scale, depth -(+0x16)), rotation `ref` -> the shot direction x
	// roll, uniform scale
	static void ImpactModelDraw(const Node *n, const int32_t *ref, uint32_t model, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->f18;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(ref, s->dir, axis);
		Mat4x3 r;
		BuildAxisAngleRotationMatrix(angle, &r, axis);
		GteMatrixMultiply(&r, &m);
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		TransformCameraByShadowRotation(n->pos, n->f1C, -(int32_t)n->pos[3]);
		ComposeAffineTransform(&Camera(), &m, &m); // (its translation, never set, is not used)
		GteSetRotMatrix(&m);
		ModelDraw(model, faded, fade);
	}

	// the armor flash: camera facing at the anchor (scale, depth -(+0x16)); the yawed scaled matrix is built and not used
	static void ArmorFlashDraw(const Node *n, bool faded, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		TransformCameraByShadowRotation(n->pos, n->f1C, -(int32_t)n->pos[3]);
		ModelDraw(MODEL_Armor, faded, fade);
	}

	// the bone smoke / target sparks: the light matrix / back colour = the camera (scratch 0x48, +8); a header (0xB4) with
	// frame 0 and +0x24 = 0
	static void SparkBegin(uint8_t *s, uint8_t *h, uint32_t seq)
	{
		*(int16_t *)(h + 0x24) = 0;
		memcpy(s + 8, &Camera(), 0x20);
		*(uint32_t *)h = seq;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}
	// one particle: its point through the camera, R = its size, pulled toward the camera by `pull`
	static void SparkDraw(const Particle *e, int32_t pull, uint8_t *s, uint8_t *h)
	{
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(e->size);
		*(int16_t *)(h + 4) = e->frame;
		int32_t *v = (int32_t *)(s + 0x38), *d = (int32_t *)(s + 0x28);
		GteReadMAC123(v);
		Normalize(v, d);
		const int32_t k = -pull;
		v[0] += mul32(d[0], k) >> 12;
		v[1] += mul32(d[1], k) >> 12;
		v[2] += mul32(d[2], k) >> 12;
		GteSetTransFromVec32(v);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static int32_t BoneSmokePull(const Particle *e) { return (int32_t)e->size >> 4; }

	// the debris: a header (0xB4: sequence 0xCFDC20, +0x24 = 0x208) and the anchor's camera matrix (scratch 0x98: +8)
	static void DebrisBegin(const Node *n, uint8_t *s, uint8_t *h)
	{
		*(uint32_t *)h = SEQ_Debris;
		*(int16_t *)(h + 0x24) = 0x208;
		int16_t *ang = (int16_t *)s;
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = 0;
		Mat4x3 *m0 = (Mat4x3 *)(s + 8);
		ComposeZYXRotationMatrix(ang, m0);
		m0->t[0] = n->pos[0];
		m0->t[1] = n->pos[1];
		m0->t[2] = n->pos[2];
		ComposeAffineTransform(&Camera(), m0, m0);
		GteSetRotMatrix(m0);
		GteSetTransVector(m0);
		*(int32_t *)(s + 0x70) = 0x1000;
		*(int32_t *)(s + 0x68) = 0x1000;
	}
	// one debris: at the anchor + its offset, scale (1, size, 1) turned from (0, -0x1000, 0) onto its velocity
	static void DebrisDraw(const Particle *e, uint8_t *s, uint8_t *h)
	{
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		IdentityRotation(m);
		m->t[0] = e->pos[0];
		m->t[1] = e->pos[1];
		m->t[2] = e->pos[2];
		*(int32_t *)(s + 0x6C) = e->size;
		Scale3DMatrix(m, (const int32_t *)(s + 0x68));
		int32_t *v = (int32_t *)(s + 0x78);
		v[0] = e->vel[0];
		v[1] = e->vel[1];
		v[2] = e->vel[2];
		Normalize(v, v);
		const int32_t angle = RotationBetweenVectors(DIR_DOWN, v, (int32_t *)(s + 0x88));
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(s + 0x48), (const int32_t *)(s + 0x88));
		GteMatrixMultiply((const Mat4x3 *)(s + 0x48), m);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(int16_t *)(h + 4) = e->frame;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void DebrisMove(Particle *e)
	{
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 2));
		e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 2));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 2));
	}

	// the arc sparks: a header (0xB4: sequence 0xCFD714, +0x24 = 0)
	static void ArcSparkBegin(uint8_t *h)
	{
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 0x24) = 0;
	}
	// one arc spark: frame * 2 + 1, camera facing at its point (its size, depth -(size / 16))
	static void ArcSparkDraw(const Particle *e, uint8_t *h)
	{
		*(int16_t *)(h + 4) = (int16_t)(e->frame * 2 + 1);
		TransformCameraByShadowRotation(e->pos, e->size, -((int32_t)e->size >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// MuzzleFlashTask (0x5AF7E0): 10 ticks, yaw, scale + step, step - step / 3, faded from counter 2
	static uint32_t __cdecl MuzzleFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_MuzzleFlashTask, n);)
		MuzzleModel(n, n->f18, 0, MODEL_MuzzleFlash, n->counter >= 2, FadeFrom2(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 3));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// MuzzleConeTask (0x5AF950): 14 ticks, roll, scale + step, step - step / 5, faded from counter 6
	static uint32_t __cdecl MuzzleConeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_MuzzleConeTask, n);)
		MuzzleModel(n, 0, n->f18, MODEL_Cone, n->counter >= 6, FadeFrom6(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 14 ? TASK_END : 0;
	}

	// GroundFlashTask (0x5AFAC0): 10 ticks, only the fade changes (from counter 2)
	static uint32_t __cdecl GroundFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundFlashTask, n);)
		GroundFlashDraw(n, n->counter >= 2, FadeFrom2(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// BoneSmokeTask (0x5AFBF0): new pairs on counters 0..3, then its particles (draw THEN update); ends when none is left
	static uint32_t __cdecl BoneSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->f0E + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 3)
		{
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x280 + 0x320);
				GetEffectSpawnPosition(Caster(), 0x1D, 0xC00, e->pos);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 0x32 - 0x19));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 0x32 - 0x19));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 0x32 - 0x19));
				// (the next entry, unchecked: vanilla; at entry 149 it is the first word past the array)
				Particle *e2 = (Particle *)((uint32_t)arr + 0x18 * (uint32_t)(i + 1));
				e2->id = id;
				e2->frame = 0;
				e2->size = (int16_t)(CrtRand() % 0x280 + 0x320);
				GetEffectSpawnPosition(Caster(), 0x1E, 0xC00, e2->pos);
				e2->pos[0] = (int16_t)(e2->pos[0] + (CrtRand() % 0x32 - 0x19));
				e2->pos[1] = (int16_t)(e2->pos[1] + (CrtRand() % 0x32 - 0x19));
				e2->pos[2] = (int16_t)(e2->pos[2] + (CrtRand() % 0x32 - 0x19));
			}
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_BoneSmokeTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_BoneSmoke);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag197_armor_shot_held.inc
			FX_HELD(held_note_part(e);)
			SparkDraw(e, BoneSmokePull(e), s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag197_armor_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->size = (int16_t)(e->size + (int16_t)(e->size >> 4));
			alive++;
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// BeamTask (0x5AFF30): 12 ticks, length + step on counters 0..3, faded from counter 4; a random head size every draw
	static uint32_t __cdecl BeamTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_BeamTask, n);)
		BeamDraw(n, n->counter >= 4, FadeFrom4(n->counter), -1);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter < 4) n->f20 = (int16_t)(n->f20 + n->f22);
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	// PathRingTask (0x5B0160): a delay, then 12 ticks: roll + spin, spin - spin / 10, scale + step, step - step / 4, faded from 4
	static uint32_t __cdecl PathRingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->pos[3] > 0)
		{
			if (UpdateFlags() & 0x201) return 0;
			n->pos[3]--;
			return 0;
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_PathRingTask, n);)
		PathRingDraw(n, n->counter >= 4, FadeFrom4(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 10));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	// ImpactRingTask (0x5B0340): 6 ticks, scale + step, step - step / 4, faded from counter 2
	static uint32_t __cdecl ImpactRingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactRingTask, n);)
		ImpactModelDraw(n, DIR_FRONT, MODEL_ImpactRing, n->counter >= 2, IringFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 6 ? TASK_END : 0;
	}

	// ImpactSpinTask (0x5B04C0): 12 ticks, roll + spin, spin - spin / 16, scale + step, step - step / 4, faded from counter 6
	static uint32_t __cdecl ImpactSpinTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactSpinTask, n);)
		ImpactModelDraw(n, DIR_BACK, MODEL_ImpactSpin, n->counter >= 6, Fade2AA(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 16));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	// ArmorFlashTask (0x5B0660): 12 ticks, scale + step, step - step / 3, faded from counter 6
	static uint32_t __cdecl ArmorFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_ArmorFlashTask, n);)
		ArmorFlashDraw(n, n->counter >= 6, Fade2AA(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 3));
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	// DebrisTask (0x5B0780): its particles (draw THEN update), then 8 new ones on counters 0..2; ends past counter 3 with none left
	static uint32_t __cdecl DebrisTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_DebrisTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x98);
		int32_t alive = 0;
		DebrisBegin(n, s, h);
		const int32_t id = n->f0E + 1;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag197_armor_shot_held.inc
			FX_HELD(held_note_part(e);)
			DebrisDraw(e, s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag197_armor_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			DebrisMove(e);
			alive++;
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0x98);
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		uint8_t *t = (uint8_t *)FieldAlloc(0x98);
		if (n->counter >= 0 && n->counter <= 2)
		{
			for (int32_t k = 0; k < 8; k++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x1000 + 0x1400);
				int32_t *v = (int32_t *)(t + 0x68);
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				Normalize(v, v);
				const int32_t r = CrtRand() % 0x190 + 0x12C;
				e->pos[0] = (int16_t)(mul32(r, v[0]) >> 12);
				e->pos[1] = (int16_t)(mul32(v[1], r) >> 12);
				e->pos[2] = (int16_t)(mul32(v[2], r) >> 12);
				const int32_t sp = CrtRand() % 0x104 + 0xDC;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(v[1], sp) >> 12);
				e->vel[2] = (int16_t)(mul32(v[2], sp) >> 12);
			}
		}
		FieldFree(0x98);
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// TargetSparkTask (0x5B0B90): new sparks on counters 0..2, then its sparks (draw THEN update); ends when none is left
	static uint32_t __cdecl TargetSparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->f0E + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 2)
		{
			const int32_t anchors = ***(const uint8_t ***)(Entity(n->f1A) + 0x64);
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0xA00 + 0x580);
				const int32_t bone = anchors ? CrtRand() % anchors : 0;
				GetEffectSpawnPosition(Entity(n->f1A), bone, CrtRand() % 0x1000, e->pos);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 1000 - 500));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 1000 - 500));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 1000 - 500));
			}
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_TargetSparkTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_Spark);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag197_armor_shot_held.inc
			FX_HELD(held_note_part(e);)
			SparkDraw(e, n->pos[3], s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag197_armor_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			alive++;
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ArcTask (0x5B0E80): counter 0: the arc record and its ArcSparkTask; 10 ticks (nothing on draw-only ticks)
	static uint32_t __cdecl ArcTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		ArcRec *a = Arc(n->f18);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			uint8_t *t = Entity(n->f1A);
			const int32_t anchors = ***(const uint8_t ***)(t + 0x64);
			const int32_t bone = anchors ? CrtRand() % anchors : 0;
			GetEffectSpawnPosition(Entity(n->f1A), bone, CrtRand() % 0x1000, a->a);
			int32_t d[3];
			d[0] = a->a[0] - n->pos[0];
			d[1] = a->a[1] - n->pos[1];
			d[2] = a->a[2] - n->pos[2];
			Normalize(d, d);
			const int32_t r = CrtRand() % 0x9C4 + 0x5DC;
			a->b[0] = (int16_t)((mul32(d[0], r) >> 12) + a->a[0]);
			a->b[1] = (int16_t)(a->a[1] - (int16_t)((CrtRand() % 0x4B0 + 0x1F4) >> 1));
			a->b[2] = (int16_t)((mul32(d[2], r) >> 12) + a->a[2]);
			memcpy(a->ctrl[0], a->a, 8);
			a->ctrl[1][0] = (int16_t)(((int32_t)a->a[0] + a->b[0]) >> 1);
			const int32_t up = CrtRand() % 0x4B0;
			a->ctrl[1][1] = (int16_t)(((((int32_t)a->b[1] + a->a[1]) >> 1) - up) - 0x1F4);
			a->ctrl[1][2] = (int16_t)(((int32_t)a->b[2] + a->a[2]) >> 1);
			memcpy(a->ctrl[2], a->b, 8);
			void *work = FieldAlloc(0x190);
			SplineSetup(3, a->ctrl, work);
			for (int32_t i = 0; i < ARC_POINTS; i++) SplineEval(3, work, a->pts[i], (i * 0x1000) / 23);
			FieldFree(0x190);
		}
		if (n->counter == 0)
		{
			Node *k = AddTask(QAux(), ORIG_ArcSparkTask);
			k->f0E = n->f18;
			const int32_t num = IdCount();
			k->pos[3] = (int16_t)num;
			IdCount() = num + 1;
			k->counter = 0;
			k->f18 = 0;
			k->f1A = 5;
		}
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// ArcSparkTask (0x5B10C0): its sparks (draw THEN update), then the head moves along the arc leaving new sparks
	static uint32_t __cdecl ArcSparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// (the original keeps the array pointer in its own argument slot, and reads it back from there)
		*(volatile uint32_t *)&n_ = (uint32_t)PartArray(n->pos[3]);
		const ArcRec *a = Arc(n->f0E);
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_node(ORIG_ArcSparkTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		const int32_t id = n->pos[3] + 1;
		ArcSparkBegin(h);
		int32_t alive = 0;
		Particle *arr = (Particle *)*(volatile uint32_t *)&n_;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag197_armor_shot_held.inc
			FX_HELD(held_note_part(e);)
			ArcSparkDraw(e, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag197_armor_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			alive++;
		}
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		const int16_t speed = n->f1A;
		n->f18 = (int16_t)(n->f18 + speed);
		if (speed > 2) n->f1A = (int16_t)(speed - 1);
		if (n->f18 < ARC_POINTS)
		{
			Particle *ar = (Particle *)*(volatile uint32_t *)&n_;
			for (int32_t p = n->f18; p >= n->f18 - n->f1A; p--)
			{
				const int32_t i = FindFree(ar);
				if (i < 0) break;
				Particle *e = &ar[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x200 + 0x480);
				memcpy(e->pos, a->pts[p], 8);
			}
		}
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// ShotTask (0x5AEEC0): one trigger press
	static uint32_t __cdecl ShotTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (!(Flags() & 1)) return 0;
		if (n->counter == 0)
		{
			if (Flags() & 4) return 0; // another shot is busy: wait
			QueueChainTransformation(Caster(), n->f18);
			Flags() |= 4;
		}
		if (n->counter == 0x16)
		{
			Ctx()->done = 1;
			Flags() &= ~4u;
		}
		if (n->counter == 0)
		{
			ShotRec *s = Shot(n->f0E);
			int16_t pos[4];
			int32_t dir[3];
			WeaponBone(Caster(), 2, dir, pos);
			s->pos[0] = (int16_t)((mul32(dir[0], 350) >> 12) + pos[0]);
			s->pos[1] = (int16_t)((mul32(dir[1], 350) >> 12) + pos[1]);
			s->pos[2] = (int16_t)((mul32(dir[2], 350) >> 12) + pos[2]);
			int32_t axis[3];
			const int32_t angle = RotationBetweenVectors(DIR_BACK, dir, axis);
			BuildAxisAngleRotationMatrix(angle, &s->m, axis);
			GetDefaultEffectPosition(Entity(n->f1A), n->pos);
			if (n->pos[1] > s->pos[1]) n->pos[1] = s->pos[1];
			s->dir[0] = n->pos[0] - s->pos[0];
			s->dir[1] = n->pos[1] - s->pos[1];
			s->dir[2] = n->pos[2] - s->pos[2];
			// (the original keeps the distance in its own argument slot)
			*(volatile int32_t *)&n_ = Sqrt(Normalize(s->dir, s->dir));
			uint8_t *t = Entity(n->f1A);
			n->pos[3] = (int16_t)(mul32(*(const int16_t *)(t + 0x26), 2000) >> 12);
			n->f1C = (int16_t)(mul32(ClampedSize(t), 2000) >> 12);
			Node *f = AddTask(QAux(), ORIG_MuzzleFlashTask);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third = (int16_t)((CrtRand() % 0x380 + 0xA80) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *c = AddTask(QAux(), ORIG_MuzzleConeTask);
			c->counter = 0;
			c->f0E = n->f0E;
			c->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third2 = (int16_t)((CrtRand() % 0x200 + 0xA00) / 3);
			c->f1E = third2;
			c->f1C = third2;
			Node *g = AddTask(QAux(), ORIG_GroundFlashTask);
			g->counter = 0;
			g->f0E = n->f0E;
			g->f18 = (int16_t)(CrtRand() % 0x1000);
			g->f1C = (int16_t)(CrtRand() % 0x480 + 0x980);
			Node *b = AddTask(QAux(), ORIG_BoneSmokeTask);
			const int32_t num = IdCount();
			b->counter = 0;
			b->f0E = (int16_t)num;
			IdCount() = num + 1;
			const int32_t sixth = *(volatile int32_t *)&n_ / 6;
			for (int32_t k = 1, frac = 0x200; frac <= 0xA00; k++, frac += 0x200)
			{
				Node *r = AddTask(QAux(), ORIG_PathRingTask);
				memcpy(r->pos, s->pos, 8);
				r->f0E = n->f0E;
				r->pos[0] = (int16_t)(r->pos[0] + (mul32(mul32(k, sixth), s->dir[0]) >> 12));
				r->pos[1] = (int16_t)(r->pos[1] + (mul32(mul32(s->dir[1], k), sixth) >> 12));
				r->pos[2] = (int16_t)(r->pos[2] + (mul32(mul32(s->dir[2], k), sixth) >> 12));
				r->counter = 0;
				r->pos[3] = (int16_t)(k - 1);
				r->f18 = (int16_t)(CrtRand() % 0x1000);
				r->f1A = (int16_t)(CrtRand() % 0xDC + 0x78);
				const int32_t shrink = 0x1000 - ComputeSin(frac / 5);
				const int16_t sc = (int16_t)((mul32(CrtRand() % 0x380 + 0xE00, shrink) >> 12) / 3);
				r->f1E = sc;
				r->f1C = sc;
				r->f20 = (int16_t)(CrtRand() % 0x600 + 0x1100);
			}
			Node *m = AddTask(QAux(), ORIG_BeamTask);
			m->f0E = n->f0E;
			m->counter = 0;
			const int16_t half = (int16_t)(*(volatile int32_t *)&n_ / 2);
			m->f1C = 0x480;
			m->f22 = half;
			m->f20 = half;
		}
		if (n->counter == 1)
		{
			Node *r = AddTask(QAux(), ORIG_ImpactRingTask);
			memcpy(r->pos, n->pos, 8);
			r->counter = 0;
			r->f0E = n->f0E;
			r->f18 = (int16_t)(CrtRand() % 0x1000);
			// (the original keeps the ring size in its own argument slot)
			*(volatile int32_t *)&n_ = CrtRand() % 0x280 + 0x500;
			const int16_t third = (int16_t)(*(volatile int32_t *)&n_ / 3);
			r->f1E = third;
			r->f1C = third;
			Node *p = AddTask(QAux(), ORIG_ImpactSpinTask);
			memcpy(p->pos, r->pos, 8);
			p->counter = 0;
			p->f0E = n->f0E;
			p->f18 = (int16_t)(CrtRand() % 0x1000);
			p->f1A = (int16_t)(CrtRand() % 0x104 + 0xA0);
			const int16_t quarter = (int16_t)((*(volatile int32_t *)&n_ + 0x300) / 4);
			p->f1E = quarter;
			p->f1C = quarter;
		}
		if (n->counter == 6)
		{
			Node *f = AddTask(QAux(), ORIG_ArmorFlashTask);
			memcpy(f->pos, n->pos, 8);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t k = ((int32_t)n->f1C << 12) / 2400;
			const int16_t third = (int16_t)((mul32(CrtRand() % 0x480 + 0xA00, k) >> 12) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *d = AddTask(QAux(), ORIG_DebrisTask);
			const int32_t num = IdCount();
			d->f0E = (int16_t)num;
			IdCount() = num + 1;
			memcpy(d->pos, n->pos, 4);
			d->counter = 0;
			memcpy(d->pos + 2, n->pos + 2, 4);
			Node *sp = AddTask(QAux(), ORIG_TargetSparkTask);
			const int32_t num2 = IdCount();
			sp->f0E = (int16_t)num2;
			IdCount() = num2 + 1;
			sp->counter = 0;
			sp->pos[3] = n->pos[3];
			sp->f1A = n->f1A;
			for (int32_t j = 0; j < 3; j++)
			{
				Node *a = AddTask(QAux(), ORIG_ArcTask);
				memcpy(a->pos, n->pos, 8);
				const int32_t arc = ArcCount();
				a->counter = 0;
				a->f18 = (int16_t)(arc % 8);
				ArcCount() = arc + 1;
				a->f1A = n->f1A;
			}
		}
		if (n->counter == 6) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (n->counter == 1) BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		n->counter++;
		return n->counter >= 25 ? TASK_END : 0;
	}

	// FadeOutTask (0x5B1360): level - level / length * counter, 0 at the end (colour 0)
	static uint32_t __cdecl FadeOutTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 - n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		if (n->counter < n->f0E) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// FadeInTask (0x5B14B0): level / length * counter (colour 0)
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5B1500): the weapon's ghosts
	static uint32_t __cdecl AfterimageTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		const int32_t c = n->counter;
		Ghost *g = GhostSlot(c % 5);
		g->used = 1;
		uint8_t *e = Caster();
		BoneSnapshot(*(uint8_t **)(e + 0x78), g->bones);
		BoneSnapshot(*(uint8_t **)(Caster() + 0x78), (uint8_t *)BONES_NOW);
		const uint32_t colour = *(const uint32_t *)(Caster() + 0x28);
		int32_t slot = c - 1 >= 0 ? (c - 1) % 5 : 0;
		const int32_t end = EndTick();
		const int32_t count = c >= end ? end - c + 5 : 5;
		// (the original keeps the ghost count in its own argument slot)
		*(volatile int32_t *)&n_ = count;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_ghosts(c, slot, count, colour);)
		for (int32_t k = 0; k < count; k++)
		{
			const Ghost *s = GhostSlot(slot);
			if (!s->used) break;
			BoneRestore(*(uint8_t **)(Caster() + 0x78), s->bones);
			*(uint32_t *)(Caster() + 0x28) = GHOST_COLOURS[k];
			GhostDraw(Caster());
			if (--slot < 0) slot = 4;
		}
		BoneRestore(*(uint8_t **)(Caster() + 0x78), (const uint8_t *)BONES_NOW);
		*(uint32_t *)(Caster() + 0x28) = colour;
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= EndTick() + 5 ? TASK_END : 0;
	}

	// StartTask (0x5B13C0): his animation set, the fade-in, the afterimages; ready after 7 ticks
	static uint32_t __cdecl StartTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			BindExternalAnimSet(Caster(), BIND_OUT, ANIM_DATA);
			QueueChainTransformation(Caster(), 1);
			Node *f = AddTask(QAux(), ORIG_FadeInTask);
			f->counter = 0;
			f->f0E = 8;
			f->f18 = 0x800;
			AddTask(QAux(), ORIG_AfterimageTask)->counter = 0;
			EndTick() = 999999;
			for (uint32_t a = GHOSTS; a < GHOSTS_END; a += 0x84) *(int32_t *)a = 0;
		}
		n->counter++;
		if (n->counter < 7) return 0;
		Flags() |= 1;
		Ctx()->done = 1;
		return TASK_END;
	}

	// EndTask (0x5B12A0, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 14 ticks
	static uint32_t __cdecl EndTask(TaskNode *n_)
	{
		if (UpdateFlags() & 0x201) return 0;
		if (Flags() & 4) return 0;
		Node *n = (Node *)n_;
		if (n->counter == 0)
		{
			QueueChainTransformation(Caster(), 2);
			Node *f = AddTask(QAux(), ORIG_FadeOutTask);
			f->counter = 0;
			f->f0E = 8;
			f->f18 = 0x800;
			EndTick() = ModuleTick();
		}
		if (n->counter == 6) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xE) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	// RestoreTask (0x5B18A0, root queue): the default effect texture back on its counter 1; ends when done
	static uint32_t __cdecl RestoreTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		if (r->counter == 1) TextureRestoreTask((void *)TexBase(), &r->done);
		const int16_t done = r->done;
		r->counter++;
		return done ? TASK_END : 0;
	}

	// RootTask (0x5B17F0): packet arena parity, the shot queue, the aux queue, the module tick, the texture restore
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (*(const uint8_t *)&r->counter & 1) ? TexBase() + ARENA_ODD : TexBase();
		const int shots = ExecuteTaskQueue(QShots());
		const int aux = ExecuteTaskQueue(QAux());
		if (!(UpdateFlags() & 0x201)) ModuleTick()++;
		if (aux == 0 && (Flags() & 2))
		{
			RootNode *t = (RootNode *)AddTaskToQueue(QRoot(), ORIG_RestoreTask);
			t->counter = 0;
			t->done = 0;
		}
		r->counter++;
		return (aux == 0 && shots == 0 && (Flags() & 2)) ? TASK_END : 0;
	}
}

	void register_mag197_armor_shot()
	{
		register_port(armor197::ORIG_RootTask, (void *)armor197::RootTask, "197 RootTask", 197);
		register_port(armor197::ORIG_RestoreTask, (void *)armor197::RestoreTask, "197 RestoreTask", 197);
		register_port(armor197::ORIG_StartTask, (void *)armor197::StartTask, "197 StartTask", 197);
		register_port(armor197::ORIG_ShotTask, (void *)armor197::ShotTask, "197 ShotTask", 197);
		register_port(armor197::ORIG_EndTask, (void *)armor197::EndTask, "197 EndTask", 197);
		register_port(armor197::ORIG_FadeOutTask, (void *)armor197::FadeOutTask, "197 FadeOutTask", 197);
		register_port(armor197::ORIG_FadeInTask, (void *)armor197::FadeInTask, "197 FadeInTask", 197);
		register_port(armor197::ORIG_AfterimageTask, (void *)armor197::AfterimageTask, "197 AfterimageTask", 197);
		register_port(armor197::ORIG_MuzzleFlashTask, (void *)armor197::MuzzleFlashTask, "197 MuzzleFlashTask", 197);
		register_port(armor197::ORIG_MuzzleConeTask, (void *)armor197::MuzzleConeTask, "197 MuzzleConeTask", 197);
		register_port(armor197::ORIG_GroundFlashTask, (void *)armor197::GroundFlashTask, "197 GroundFlashTask", 197);
		register_port(armor197::ORIG_BoneSmokeTask, (void *)armor197::BoneSmokeTask, "197 BoneSmokeTask", 197);
		register_port(armor197::ORIG_BeamTask, (void *)armor197::BeamTask, "197 BeamTask", 197);
		register_port(armor197::ORIG_PathRingTask, (void *)armor197::PathRingTask, "197 PathRingTask", 197);
		register_port(armor197::ORIG_ImpactRingTask, (void *)armor197::ImpactRingTask, "197 ImpactRingTask", 197);
		register_port(armor197::ORIG_ImpactSpinTask, (void *)armor197::ImpactSpinTask, "197 ImpactSpinTask", 197);
		register_port(armor197::ORIG_ArmorFlashTask, (void *)armor197::ArmorFlashTask, "197 ArmorFlashTask", 197);
		register_port(armor197::ORIG_DebrisTask, (void *)armor197::DebrisTask, "197 DebrisTask", 197);
		register_port(armor197::ORIG_TargetSparkTask, (void *)armor197::TargetSparkTask, "197 TargetSparkTask", 197);
		register_port(armor197::ORIG_ArcTask, (void *)armor197::ArcTask, "197 ArcTask", 197);
		register_port(armor197::ORIG_ArcSparkTask, (void *)armor197::ArcSparkTask, "197 ArcSparkTask", 197);
		// 30 fps layer: see mag197_armor_shot_held.inc
		FX_HELD(register_mag197_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag197_armor_shot_held.inc"
#endif
