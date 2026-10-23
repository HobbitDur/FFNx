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

// Effect 198: Hyper Shot (Irvine's Shot limit break, kernel.bin section 21 entry 7 +4; MAG_198_SHOT__HYPER_SHOT).
//
// How the battle drives it (as Normal Shot 188, see mag188_normal_shot.cpp): the opening action is a normal magic cast (mode 0);
// every trigger press of the Shot window becomes an action that BattleActionSequence_Tick_Shot 0x50BEE0 runs: it calls the
// module's _Init again with g_MagicCastContext 0x1D99A78 +8 = 2 (a shot) or 1 (the closing action) and waits for +9 done = 1.
//
// Structure (file loader 0x5AA3C0 = the texture file 0xCFB33C; _Init 0x5AA3E0 keeps the texture base 0x226045C, the context
// 0x225EBD0 and the caster slot 0x22601B8 (context byte 0), then SetupByMode 0x5AA410 (both run original): mode 0 = the root
// pool 0x225A918 (4 x 0x10, queue 0x225B768) with RootTask, the aux queue 0x225DDB0 (pool 0x225DDC0, 100 x 0x24), the shot queue
// 0x225B778 (pool 0x225A958, 100 x 0x24) with StartTask, flags 0x225DDA8 / shot index 0x225A914 / shot count 0x2260454 / particle
// id count 0x2260458 / module tick 0x225A90C cleared, both particle arrays (0x225B788 / 0x225CA68, 200 x 0x18) free,
// resetSomeCameraData, the TIM upload; mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue: +0x0E = shot
// count % 100 (its shot record 0x225EBD8 + 0x38 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the first
// target's slot):
//   RootTask (0x5AEC90) - packet cursor 0x2260460 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); ends when both queues are empty and EndTask finished (flag 2).
//   StartTask (0x5AE860, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xCF3A78, out block 0x225CA48), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x225A908 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5AA570, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his chain
//     (+0x18) and flag 4; the muzzle (bone 2 of his weapon model, entity +0x78, through 0x5AAAE0 / 0x5AAB60 = the Renzokuken
//     finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000) to the
//     direction (+0x18); its own +0x10 = the target's effect anchor (GetDefaultEffectPosition 0x571400) with y clamped to the
//     muzzle's, the record's +8 = the direction muzzle -> anchor, normalized (the distance = Math_SqrtInt of its squared length);
//     +0x16 = the target's size * 2000 / 4096, +0x1C = the target's size clamped to 0x600..0x1100 (0x5AAAB0) * 2000 / 4096, +0x1E
//     = 1 when the target's +0x3C is above -500 (else 0); MuzzleFlashTask (roll, scale = step = (0xC80..0xFFF) / 3), BoneSmokeTask
//     (the next particle id), BeamTask (scale 0x3400, shrink 0x14C, length = its step = the distance / 2), SprayTask (the next
//     particle id); counters 0 / 3 / 6: a MuzzleConeTask (roll, spin 0x6E..0x103, scale = step = (0xC80..0xFFF) / 3); counter 1:
//     the gun sound 0xCFB2FC (volume 1 / 2 alternating); counter 2: ImpactSparkTask and ImpactSmokeTask at the anchor (each the
//     next particle id); counters 3 / 6 / 9 when +0x1E: a GroundRingTask (scale = step = ((0xA00..0xD7F) * (+0x1C * 4096 / 2200)
//     >> 12) / 3); counter 10: the action result (ApplyActionResultToTargets 0x506BA0); counter 22: done, flag 4 cleared; ends at 25.
//   MuzzleFlashTask (0x5AABE0) - model 0xCF9834 at the shot record (its rotation x roll), uniform scale, through the module's
//     scrolling prim renderer (0x5AAD60, header 0x7C: texture window x 0 / y 0x80, w 0x40 / h 0x80; v scroll -16 * counter);
//     scale + step, step - step / 4; faded (counter - 8) << 9 from counter 8; 16 ticks.
//   BoneSmokeTask (0x5AD480) - Armor Shot 197's (0x5AFBF0) with 200-entry arrays and sequence 0xCF6054.
//   BeamTask (0x5AD7C0) - prim model 0xCF6394 at the shot record (rotation (0, 0, -0x1000) -> the shot direction), scale (scale,
//     scale, length * 4096 / 6000); the shot record's matrix translation x / z = scale * 600 / 4096 / the length (read by
//     SprayTask); then (the same header) prim model 0xCF8D2C on the ground under the muzzle (rotation onto the horizontal
//     direction), scale (scale, scale, sqrt(length^2 - y^2) * 4096 / 6000, y = the beam end's height); faded (counter - 8) << 9
//     from counter 8; scale - shrink, shrink + shrink / 8; length + its step on counters 0..3; 16 ticks.
//   SprayTask (0x5ADAD0) - counters 0..5 (not on draw-only ticks): 8 new particles along the beam (offset rand % its length, then
//     rand % its x translation along a random direction turned by the shot rotation; speed 0xF..0x63, size 0x300..0x47F); then
//     every particle of its array (id parity) with its id: sequence 0xCF6200 at its frame, through the camera, R = its size,
//     pulled toward the camera by its own +0x16 (its particle id - 1: vanilla); then (not on draw-only ticks) frame + 1, dies when
//     its sequence ended, else position + velocity; ends when none is left.
//   MuzzleConeTask (0x5AC760) - model 0xCFA52C at the shot record (its rotation x roll), uniform scale, through the module's prim
//     renderer 0x5AC8F0 (= Gastric Juice 169's 0x5E0450 and Dark Shot 193's 0x5B84E0, with its section renderers 0x5AC9E0 /
//     0x5ACC40 / 0x5ACF10 / 0x5AD170); roll + spin, spin - spin / 10, scale + step, step - step / 5; faded (counter - 2) << 9
//     from counter 2; 10 ticks.
//   ImpactSparkTask (0x5ADE70) - counters 0..12 (not on draw-only ticks): 8 new particles at the anchor pulled back along the shot
//     by +0x16 (random direction turned from (0, 0, -0x1000) onto the shot direction, offset 0x12C..0x383, speed 0xC8..0x1F3,
//     size 0x400..0x67F); its particles drawn as SprayTask's (pulled by +0x16); position + velocity, velocity - velocity / 4.
//   ImpactSmokeTask (0x5AE240) - as ImpactSparkTask with 2 new particles (offset 0x64..0x18F, speed 0x64..0x1C1, size
//     0x680..0x177F), sequence 0xCF5E44 at the particle's point through the camera (GTE_TransFromMAC, no pull).
//   GroundRingTask (0x5AE590) - prim model 0xCFAE64 on the ground at the anchor pulled back by +0x16 along the horizontal shot
//     direction (rotation (0, 0, 0x1000) onto it), uniform scale; scale + step, step - step / 4; faded (counter - 4) * 0x2AA from
//     counter 4; 10 ticks.
//   EndTask (0x5AE740, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end tick =
//     the module tick; counter 10: the action result; after 14 ticks: flag 2, done; ends.
//   FadeInTask (0x5AE950) / FadeOutTask (0x5AE800) - screen flash (Effect_SetScreenFlash 0x5713E0, colour 0) 0 -> 0x800 / 0x800
//     -> 0 over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5AE9A0) - the weapon's ghosts as Normal Shot's (ghost slots 0x22601C0, current bones 0x225DD28, colours
//     0xCFB328; BoneSnapshot 0x5AEB50 / BoneRestore 0x5AEB90 / GhostDraw 0x5AEBD0; the ghost count kept in its own argument slot).
// The scrolling prim renderer 0x5AAD60 (header 0x7C on the scratch: +0x00 model, +0x04 vertices (= model + 8), +0x08..+0x0A far
//   colour, +0x0C fade, +0x10 / +0x14 u / v scroll, +0x18 / +0x1A texture window x / y, +0x1C / +0x1E its width / height, +0x20
//   flags, +0x24 section cursor, +0x28 NCLIP, +0x30 OTZ, +0x34 GTE FLAG, +0x5C..+0x68 scrolled u / v, +0x6C / +0x70 the scrolls
//   modulo the window, +0x74 / +0x78 the texture window set / reset words) draws 8 lists: F3 0x5AAF10, F4 0x5AB120, FT3 0x5AB3A0,
//   FT4 0x5AB6D0, G3 0x5ABAD0, G4 0x5ABD00, GT3 0x5ABFC0, GT4 0x5AC320 (the textured ones = MAG_151's 0x64E1D0 / 0x64E500 /
//   0x64E900 / 0x64EC60, instruction for instruction); the textured packets are framed by the texture window words.
// Every task except RootTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// Never-written words: the shot record's +6 and +0x14 (copied, never read), the spray / impact particles' scratch words.
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x225A908..0x2260464 (end tick, module tick, shot index, pools, queues, particle arrays, bind block, saved
//   bones 0x225DD28, flags 0x225DDA8, aux queue, context, shot records, caster slot, texture file, ghost slots, counts,
//   texture base, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace hyper198
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the renderers follow the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t S16v(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x225A908); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x225A90C); }
	inline uint32_t &Flags() { return var<uint32_t>(0x225DDA8); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x225EBD0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x22601B8); }
	inline int32_t &IdCount() { return var<int32_t>(0x2260458); }       // the next particle id - 1
	inline uint32_t &TexBase() { return var<uint32_t>(0x226045C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2260460); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x225B768; }
	inline TaskQueue *QShots() { return (TaskQueue *)0x225B778; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x225DDB0; }
	static const uint32_t PARTS_A = 0x225B788, PARTS_B = 0x225CA68;  // 200 x 0x18 each
	static const int32_t PARTS_N = 200;
	static const uint32_t SHOTS = 0x225EBD8;                         // 100 x 0x38 shot records
	static const uint32_t BIND_OUT = 0x225CA48;
	static const uint32_t BONES_NOW = 0x225DD28;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x22601C0, GHOSTS_END = 0x2260454; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;

	static const uint32_t ORIG_RootTask = 0x5AEC90;
	static const uint32_t ORIG_StartTask = 0x5AE860;
	static const uint32_t ORIG_ShotTask = 0x5AA570;
	static const uint32_t ORIG_EndTask = 0x5AE740;
	static const uint32_t ORIG_FadeOutTask = 0x5AE800;
	static const uint32_t ORIG_FadeInTask = 0x5AE950;
	static const uint32_t ORIG_AfterimageTask = 0x5AE9A0;
	static const uint32_t ORIG_MuzzleFlashTask = 0x5AABE0;
	static const uint32_t ORIG_BoneSmokeTask = 0x5AD480;
	static const uint32_t ORIG_BeamTask = 0x5AD7C0;
	static const uint32_t ORIG_SprayTask = 0x5ADAD0;
	static const uint32_t ORIG_MuzzleConeTask = 0x5AC760;
	static const uint32_t ORIG_ImpactSparkTask = 0x5ADE70;
	static const uint32_t ORIG_ImpactSmokeTask = 0x5AE240;
	static const uint32_t ORIG_GroundRingTask = 0x5AE590;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xCF3A78;
	static const uint32_t MODEL_MuzzleFlash = 0xCF9834, MODEL_Beam = 0xCF6394, MODEL_BeamShadow = 0xCF8D2C, MODEL_Cone = 0xCFA52C, MODEL_GroundRing = 0xCFAE64;
	static const uint32_t SEQ_BoneSmoke = 0xCF6054, SEQ_Spark = 0xCF6200, SEQ_Smoke = 0xCF5E44;
	static const void *const SOUND_Gun = (const void *)0xCFB2FC;
	static const int32_t *const DIR_BACK = (const int32_t *)0xCFB308;  // (0, 0, -0x1000)
	static const int32_t *const DIR_FRONT = (const int32_t *)0xCFB318; // (0, 0, 0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xCFB328;

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
	inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                  // TR = MAC123
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
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }

	// ------------------------------------------------------------------
	// Node / record layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool: 4 x 0x10 (RootTask, EndTask)
	{
		TaskNode hdr;
		int16_t counter;
		int16_t f0E;
	};
	struct Node // shot / aux queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t f0E;     // +0x0E shot: shot number; muzzle / beam / spray / impact / ring tasks: their shot record; bone smoke: its
		                 //       particle id - 1; fades: length
		int16_t pos[4];  // +0x10 shot / impact tasks / ring: the target's anchor (+0x16: the target's size * 2000 / 4096); spray:
		                 //       +0x16 = its particle id - 1
		int16_t f18;     // +0x18 shot: chain; flash / cone: roll; fades: level
		int16_t f1A;     // +0x1A shot: target slot; cone: spin
		int16_t f1C;     // +0x1C scale (shot: the clamped target size)
		int16_t f1E;     // +0x1E scale step (shot: the target's +0x3C above -500)
		int16_t f20;     // +0x20 beam: its length; impact sparks / smoke: their particle id - 1
		int16_t f22;     // +0x22 beam: its length step
	};
	struct ShotRec // 0x225EBD8 + 0x38 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		int32_t dir[3];  // the direction muzzle -> the target's anchor, normalized
		uint8_t pad14[4];
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction (its translation x / z: BeamTask's)
	};
	struct Particle // 0x225B788 / 0x225CA68, 200 x 0x18
	{
		int32_t id;      // 0 = free, else the task's particle id
		int16_t frame;
		int16_t size;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x22601C0, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Hyper Shot nodes");
	static_assert(sizeof(ShotRec) == 0x38 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Hyper Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x38 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t k) { return (Particle *)((k & 1) ? PARTS_B : PARTS_A); }
	inline int32_t FindFree(const Particle *arr) { for (int32_t i = 0; i < PARTS_N; i++) if (arr[i].id == 0) return i; return -1; }
}
}

#ifdef FF8_FX_HELD
#include "mag198_hyper_shot_held.h"
#endif

namespace ff8fx
{
namespace hyper198
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5AAB60 / 0x5AAAE0 = Rough Divide's 0x5F6500 / 0x5F6480)
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

	// the target's size clamped to 0x600..0x1100 (0x5AAAB0)
	static int32_t ClampedSize(const uint8_t *entity)
	{
		const int32_t s = *(const int16_t *)(entity + 0x26);
		if (s > 0x1100) return 0x1100;
		if (s < 0x600) return 0x600;
		return s;
	}

	// ------------------------------------------------------------------
	// Afterimages (0x5AEB50 / 0x5AEB90 / 0x5AEBD0)
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

	// ==================================================================
	// Common clip test of the prim renderers: x outside 0..0xA00 -> xbit, y outside 0..0x6C0 -> ybit
	// ==================================================================
	static uint32_t ClipX(uint32_t xy, uint32_t bit) { const int16_t x = S16v(xy, 0); return (x < 0 || x > 0xA00) ? bit : 0; }
	static uint32_t ClipY(uint32_t xy, uint32_t bit) { const int16_t y = S16v(xy, 2); return (y < 0 || y > 0x6C0) ? bit : 0; }

	// ==================================================================
	// The scrolling prim renderer (0x5AAD60) and its 8 section renderers (header 0x7C, see the file header)
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

	// 0x5AAF10: flat triangles (0xC-byte records -> 0x14-byte packets)
	static uint32_t ScrF3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		for (; count > 0; count--, rec += 0xC)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x4000000;
			U32(pk, 4) = U32(rec, 0);
			ScrSemi(h, pk, 4, 1, 4);
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x10)) continue;
			GteReadSXY012u(pk + 8, pk + 0xC, pk + 0x10);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0xC, 2) | ClipX(pk + 0x10, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0xC, 0x20) | ClipY(pk + 0x10, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x40) ScrCueFlat(h, pk + 4);
			ScrInsert(h, ot, shift, pk);
			pk += 0x14;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x5AB120: flat quads (0xC-byte records -> 0x18-byte packets)
	static uint32_t ScrF4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		for (; count > 0; count--, rec += 0xC)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x5000000;
			U32(pk, 4) = U32(rec, 0);
			ScrSemi(h, pk, 4, 1, 4);
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x10)) continue;
			GteReadSXY012u(pk + 8, pk + 0xC, pk + 0x10);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0xC, 2) | ClipX(pk + 0x10, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0xC, 0x20) | ClipY(pk + 0x10, 0x40);
			GteReadSXY2u(pk + 0x14);
			GteAVSZ4x();
			m |= ClipX(pk + 0x14, 8) | ClipY(pk + 0x14, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x40) ScrCueFlat(h, pk + 4);
			ScrInsert(h, ot, shift, pk);
			pk += 0x18;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x5AB3A0 (= MAG_151's 0x64E1D0): textured flat triangles with the texture window (0x14-byte records -> 0x28-byte packets)
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

	// 0x5AB6D0 (= MAG_151's 0x64E500): textured flat quads with the texture window (0x18-byte records -> 0x30-byte packets)
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

	// 0x5ABAD0: Gouraud triangles (0x14-byte records -> 0x1C-byte packets)
	static uint32_t ScrG3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		for (; count > 0; count--, rec += 0x14)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x6000000;
			U32(pk, 4) = U32(rec, 0);
			ScrSemi(h, pk, 4, 2, 8);
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x20)) continue;
			GteReadSXY012u(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x80)
			{
				GteLoadRGB012u(rec + 0xC, rec + 0x10, pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0xC, pk + 0x14, pk + 4);
			}
			else
			{
				U32(pk, 0xC) = U32(rec, 0xC);
				U32(pk, 0x14) = U32(rec, 0x10);
			}
			ScrInsert(h, ot, shift, pk);
			pk += 0x1C;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x5ABD00: Gouraud quads (0x18-byte records -> 0x24-byte packets)
	static uint32_t ScrG4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		int32_t count = S32(U32(h, 0x24), 0);
		uint32_t rec = U32(h, 0x24) + 4;
		U32(h, 0x24) = rec;
		const uint32_t verts = U32(h, 4);
		for (; count > 0; count--, rec += 0x18)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = U32(rec, 0);
			ScrSemi(h, pk, 4, 2, 8);
			GteReadFLAGu(h + 0x34);
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIPx();
			GteReadMAC0u(h + 0x28);
			const int32_t nc = S32(h, 0x28);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x20) & 0x20)) continue;
			GteReadSXY012u(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4x();
			m |= ClipX(pk + 0x20, 8) | ClipY(pk + 0x20, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x30);
			if (U8(h, 0x20) & 0x80)
			{
				GteLoadRGB012u(rec + 0xC, rec + 0x10, rec + 0x14);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0xC, pk + 0x14, pk + 0x1C);
				GteLoadRGBCu(pk + 4);
				GteDPCSx();
				GteStoreRGB2u(pk + 4);
			}
			else
			{
				U32(pk, 0xC) = U32(rec, 0xC);
				U32(pk, 0x14) = U32(rec, 0x10);
				U32(pk, 0x1C) = U32(rec, 0x14);
			}
			ScrInsert(h, ot, shift, pk);
			pk += 0x24;
		}
		U32(h, 0x24) = rec;
		return pk;
	}

	// 0x5ABFC0 (= MAG_151's 0x64E900): textured Gouraud triangles with the texture window (0x1C-byte records -> 0x30-byte packets)
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

	// 0x5AC320 (= MAG_151's 0x64EC60): textured Gouraud quads with the texture window (0x24-byte records -> 0x3C-byte packets)
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

	// 0x5AAD60: the scrolling renderer: vertices = model + 8, the section cursor, the texture window words, the scrolls modulo
	// the window, the far colour, then the 8 lists (an empty list is one zero word)
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
		if (U32(U32(h, 0x24), 0)) cursor = ScrF3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrF4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrFT3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrFT4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrG3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrG4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrGT3(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		if (U32(U32(h, 0x24), 0)) cursor = ScrGT4(h, ot, shift, cursor);
		else U32(h, 0x24) += 4;
		return cursor;
	}

	// ==================================================================
	// Module prim renderer (0x5AC8F0 = Gastric Juice 169's 0x5E0450 = Dark Shot 193's 0x5B84E0, the section renderers 0x5AC9E0 /
	// 0x5ACC40 / 0x5ACF10 / 0x5AD170, instruction for instruction; header 0x5C bytes on the scratch: +0x00 model, +0x04
	// vertices, +0x08..+0x0A far colour, +0x0C fade, +0x10 / +0x14 texture page / clut words, +0x18 texture offset, +0x1C flags,
	// +0x20 section cursor, +0x24 NCLIP, +0x2C OTZ, +0x30 GTE FLAG, +0x58 draw mode word)
	// ==================================================================

	// the texture page / clut words of a packet (flags 0x100 / 0x400, 0x200 / 0x800)
	static void PageWords(uint32_t h, uint32_t pk, int32_t page_off)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & 0x400) U16(pk, page_off) = (uint16_t)(U16(pk, page_off) + U16(h, 0x10));
		else if (f & 0x100) U16(pk, page_off) = U16(h, 0x10);
		if (f & 0x800) U16(pk, 0xE) = (uint16_t)(U16(pk, 0xE) + U16(h, 0x14));
		else if (f & 0x200) U16(pk, 0xE) = U16(h, 0x14);
	}

	// the face's semi-transparency bit (flags bit 0 / 1 sets it, bit 2 / 3 clears it)
	static void SemiTrans(uint32_t h, uint32_t pk, uint32_t set, uint32_t clear)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & set) U32(pk, 4) = U32(pk, 4) | 0x2000000;
		if (f & clear) U32(pk, 4) &= 0xFDFFFFFF;
	}

	// 0x5AC9E0: flat textured triangles (0x14-byte records -> 0x24-byte packets)
	static uint32_t PrimFT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x14)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x1C) = (U32(rec, 8) >> 16) + d;
			GteReadFLAGu(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIPx();
			PageWords(h, pk, 0x16);
			GteReadMAC0u(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012u(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBCu(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCSx();
				GteStoreRGB2u(pk + 4);
			}
			U32(pk, 0x20) = U32(h, 0x58);
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), (void *)pk);
			pk += 0x24;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5ACC40: flat textured quads (0x18-byte records -> 0x2C-byte packets)
	static uint32_t PrimFT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x18)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x1C) = uv3;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = uv3 >> 16;
			GteReadFLAGu(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIPx();
			PageWords(h, pk, 0x16);
			GteReadMAC0u(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012u(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4x();
			m |= ClipX(pk + 0x20, 8) | ClipY(pk + 0x20, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBCu(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCSx();
				GteStoreRGB2u(pk + 4);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), (void *)pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5ACF10: Gouraud textured triangles (0x1C-byte records -> 0x2C-byte packets)
	static uint32_t PrimGT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x1C)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = (U32(rec, 8) >> 16) + d;
			GteReadFLAGu(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIPx();
			PageWords(h, pk, 0x1A);
			GteReadMAC0u(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012u(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3x();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012u(rec + 0x14, rec + 0x18, pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x14);
				U32(pk, 0x1C) = U32(rec, 0x18);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), (void *)pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5AD170: Gouraud textured quads (0x24-byte records -> 0x38-byte packets)
	static uint32_t PrimGT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x24)
		{
			GteLoadV012u(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPTx();
			U32(pk, 0) = 0xD000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x24) = uv3;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x30) = uv3 >> 16;
			GteReadFLAGu(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIPx();
			PageWords(h, pk, 0x1A);
			GteReadMAC0u(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012u(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			GteReadSXY2u(pk + 0x2C);
			GteAVSZ4x();
			m |= ClipX(pk + 0x2C, 8) | ClipY(pk + 0x2C, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012u(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(S32(h, 0xC));
				GteDPCTx();
				GteStoreRGB012u(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBCu(pk + 4);
				GteDPCSx();
				GteStoreRGB2u(pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x18);
				U32(pk, 0x1C) = U32(rec, 0x1C);
				U32(pk, 0x28) = U32(rec, 0x20);
			}
			U32(pk, 0x34) = U32(h, 0x58);
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), (void *)pk);
			pk += 0x38;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x5AC8F0: the model's sections (two skipped words, FT3, FT4, two skipped words, GT3, GT4; an empty section is one zero
	// word); vertices = model + 8 unless flag 0x2000, texture offset 0 unless flag 0x1000
	static uint32_t PrimRender(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t f = U32(h, 0x1C);
		if (!(f & 0x2000)) U32(h, 4) = U32(h, 0) + 8;
		U32(h, 0x20) = U32(U32(h, 0), 0) + U32(h, 0);
		if (!(f & 0x1000)) U32(h, 0x18) = 0;
		U32(h, 0x58) = 0xE1000220;
		GteSetFarColor(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		return cursor;
	}

	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	static uint8_t *ModelHeader(uint32_t bytes, uint32_t model, bool faded, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(bytes);
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
	// the GTE rotation / translation = the camera x m
	static void ModelMatrix(Mat4x3 *m)
	{
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}
	// the shot record's rotation x ZYX(0, 0, roll), at the muzzle point, uniform scale
	static void MuzzleMatrix(const Node *n, Mat4x3 *m)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->f18;
		ang[3] = 0; // never written by the original (not read)
		ComposeZYXRotationMatrix(ang, m);
		GteMatrixMultiply(&s->m, m);
		m->t[0] = s->pos[0];
		m->t[1] = s->pos[1];
		m->t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(m, v);
		ModelMatrix(m);
	}

	static int32_t FadeFrom2(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 9); }
	static int32_t FadeFrom8(int32_t c) { return (int32_t)((uint32_t)(c - 8) << 9); }
	static int32_t RingFade(int32_t c) { return mul32(c - 4, 0x2AA); }
	static int32_t FlashScroll(int32_t c) { return (int32_t)((uint32_t)(-c) << 4); }

	// the muzzle flash through the scrolling renderer (header 0x7C: window x 0 / y 0x80, w 0x40 / h 0x80; v scroll `scroll`)
	static void MuzzleFlashDraw(const Node *n, bool faded, int32_t fade, int32_t scroll)
	{
		Mat4x3 m;
		MuzzleMatrix(n, &m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x7C);
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)h = MODEL_MuzzleFlash;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(int32_t *)(h + 0x10) = 0;
		*(int32_t *)(h + 0x14) = scroll;
		if (faded)
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = ScrollRender((uint32_t)h, RenderOT(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	// the muzzle cone through the module prim renderer (header 0x5C)
	static void MuzzleConeDraw(const Node *n, bool faded, int32_t fade)
	{
		Mat4x3 m;
		MuzzleMatrix(n, &m);
		uint8_t *h = ModelHeader(0x5C, MODEL_Cone, faded, fade);
		PacketCursor() = PrimRender((uint32_t)h, RenderOT(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	// the beam (rotation (0, 0, -0x1000) -> the shot direction, depth length * 4096 / 6000) and its shadow on the ground (rotation
	// onto the horizontal direction, depth sqrt(length^2 - y^2) * 4096 / 6000); the shot record's translation x / z set on the way
	static void BeamDraw(const Node *n, bool faded, int32_t fade)
	{
		ShotRec *s = Shot(n->f0E);
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
		int32_t v[3] = { n->f1C, n->f1C, ((int32_t)n->f20 << 12) / 6000 };
		Scale3DMatrix(&m, v);
		s->m.t[0] = mul32(n->f1C, 600) >> 12;
		ComposeAffineTransform(&Camera(), &m, &m);
		s->m.t[2] = n->f20;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ModelHeader(0x58, MODEL_Beam, faded, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		int32_t d[4];
		d[0] = s->dir[0];
		d[1] = s->dir[1];
		d[2] = s->dir[2];
		d[3] = *(const int32_t *)s->pad14; // (copied, never read)
		d[1] = 0;
		Normalize(d, d);
		const int32_t angle2 = RotationBetweenVectors(DIR_BACK, d, axis);
		BuildAxisAngleRotationMatrix(angle2, &m, axis);
		m.t[0] = s->pos[0];
		m.t[1] = 0;
		m.t[2] = s->pos[2];
		v[1] = n->f1C;
		v[0] = n->f1C;
		const int32_t y = (mul32(s->dir[1], n->f20) >> 12) + s->pos[1];
		v[2] = (Sqrt(mul32(n->f20, n->f20) - mul32(y, y)) << 12) / 6000;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_BeamShadow;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the ground ring at the anchor pulled back by +0x16 along the horizontal shot direction (rotation (0, 0, 0x1000) onto it)
	static void GroundRingDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int32_t d[4];
		d[0] = s->dir[0];
		d[1] = s->dir[1];
		d[2] = s->dir[2];
		d[3] = *(const int32_t *)s->pad14; // (copied, never read)
		d[1] = 0;
		Normalize(d, d);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(DIR_FRONT, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		const int32_t k = -(int32_t)n->pos[3];
		m.t[1] = 0;
		m.t[0] = (mul32(d[0], k) >> 12) + n->pos[0];
		m.t[2] = (mul32(d[2], k) >> 12) + n->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		ModelMatrix(&m);
		uint8_t *h = ModelHeader(0x58, MODEL_GroundRing, faded, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the particle draws: the light matrix / back colour = the camera (scratch 0x48, +8); a header (0xB4) with +0x24 = 0
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
	// one smoke particle: its point through the camera, R = its size (no pull)
	static void SmokeDraw(const Particle *e, uint8_t *h)
	{
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(e->size);
		*(int16_t *)(h + 4) = e->frame;
		GteTransFromMAC();
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void MoveDecay(Particle *e)
	{
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 2));
		e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 2));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 2));
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// MuzzleFlashTask (0x5AABE0): 16 ticks, roll, scale + step, step - step / 4, faded from counter 8, v scroll -16 * counter
	static uint32_t __cdecl MuzzleFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_MuzzleFlashTask, n);)
		MuzzleFlashDraw(n, n->counter >= 8, FadeFrom8(n->counter), FlashScroll(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 16 ? TASK_END : 0;
	}

	// MuzzleConeTask (0x5AC760): 10 ticks, roll + spin, spin - spin / 10, scale + step, step - step / 5, faded from counter 2
	static uint32_t __cdecl MuzzleConeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_MuzzleConeTask, n);)
		MuzzleConeDraw(n, n->counter >= 2, FadeFrom2(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 10));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// BeamTask (0x5AD7C0): 16 ticks, scale - shrink, shrink + shrink / 8, length + step on counters 0..3, faded from counter 8
	static uint32_t __cdecl BeamTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_BeamTask, n);)
		BeamDraw(n, n->counter >= 8, FadeFrom8(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t shrink = n->f1E;
		n->f1C = (int16_t)(n->f1C - shrink);
		n->f1E = (int16_t)(shrink + (int16_t)(shrink / 8));
		if (n->counter < 4) n->f20 = (int16_t)(n->f20 + n->f22);
		n->counter++;
		return n->counter >= 16 ? TASK_END : 0;
	}

	// GroundRingTask (0x5AE590): 10 ticks, scale + step, step - step / 4, faded from counter 4
	static uint32_t __cdecl GroundRingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundRingTask, n);)
		GroundRingDraw(n, n->counter >= 4, RingFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// BoneSmokeTask (0x5AD480): new pairs on counters 0..3, then its particles (draw THEN update); ends when none is left
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
				// (the next entry, unchecked: vanilla; at entry 199 it is the first word past the array)
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
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_BoneSmokeTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_BoneSmoke);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag198_hyper_shot_held.inc
			FX_HELD(held_note_part(e);)
			SparkDraw(e, BoneSmokePull(e), s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag198_hyper_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->size = (int16_t)(e->size + (int16_t)(e->size >> 4));
			alive++;
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// SprayTask (0x5ADAD0): 8 new particles along the beam on counters 0..5, then its particles (draw THEN update)
	static uint32_t __cdecl SprayTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		const ShotRec *rec = Shot(n->f0E);
		Particle *arr = PartArray(n->pos[3]);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->pos[3] + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 5)
		{
			memcpy(s, rec->pos, 8);
			memcpy(s + 0x28, rec->dir, 0x10);
			memcpy(s + 8, &rec->m, 0x20);
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x180 + 0x300);
				const int32_t along = CrtRand() % *(const int32_t *)(s + 0x24); // (rand % the beam length: the record's z translation)
				e->pos[0] = (int16_t)((mul32(*(const int32_t *)(s + 0x28), along) >> 12) + *(const int16_t *)s);
				e->pos[1] = (int16_t)((mul32(*(const int32_t *)(s + 0x2C), along) >> 12) + *(const int16_t *)(s + 2));
				e->pos[2] = (int16_t)((mul32(*(const int32_t *)(s + 0x30), along) >> 12) + *(const int16_t *)(s + 4));
				const int32_t out = CrtRand() % *(const int32_t *)(s + 0x1C); // (rand % the record's x translation)
				int32_t *v = (int32_t *)(s + 0x38);
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x680 - 0x340;
				Normalize(v, v);
				TransformVectorBy3x3Matrix((const Mat4x3 *)(s + 8), v, v);
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(out, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(out, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(out, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x55 + 0xF;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_SprayTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_Spark);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag198_hyper_shot_held.inc
			FX_HELD(held_note_part(e);)
			SparkDraw(e, n->pos[3], s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag198_hyper_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
			e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
			e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
			alive++;
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// the impact particles' spawn frame: the anchor pulled back by +0x16 along the shot (scratch +0), rotation (0, 0, -0x1000) ->
	// the shot direction (scratch +8, axis +0x28)
	static void ImpactFrame(const Node *n, uint8_t *s)
	{
		const ShotRec *rec = Shot(n->f0E);
		memcpy(s, n->pos, 8);
		const int32_t k = -(int32_t)n->pos[3];
		int16_t *p = (int16_t *)s;
		p[0] = (int16_t)(p[0] + (mul32(k, rec->dir[0]) >> 12));
		p[1] = (int16_t)(p[1] + (mul32(rec->dir[1], k) >> 12));
		p[2] = (int16_t)(p[2] + (mul32(rec->dir[2], k) >> 12));
		const int32_t angle = RotationBetweenVectors(DIR_BACK, rec->dir, (int32_t *)(s + 0x28));
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(s + 8), (const int32_t *)(s + 0x28));
	}

	// ImpactSparkTask (0x5ADE70): 8 new particles on counters 0..12, then its particles (draw THEN update)
	static uint32_t __cdecl ImpactSparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f20);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->f20 + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 0xC)
		{
			ImpactFrame(n, s);
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x280 + 0x400);
				memcpy(e->pos, s, 8);
				int32_t *v = (int32_t *)(s + 0x38);
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = -0x80 - CrtRand() % 0x400;
				Normalize(v, v);
				TransformVectorBy3x3Matrix((const Mat4x3 *)(s + 8), v, v);
				const int32_t r = CrtRand() % 0x258 + 0x12C;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(r, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x12C + 0xC8;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactSparkTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_Spark);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag198_hyper_shot_held.inc
			FX_HELD(held_note_part(e);)
			SparkDraw(e, n->pos[3], s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag198_hyper_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			MoveDecay(e);
			alive++;
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ImpactSmokeTask (0x5AE240): 2 new particles on counters 0..12, then its particles (draw THEN update)
	static uint32_t __cdecl ImpactSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f20);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->f20 + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 0xC)
		{
			ImpactFrame(n, s);
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x1100 + 0x680);
				memcpy(e->pos, s, 8);
				int32_t *v = (int32_t *)(s + 0x38);
				v[0] = CrtRand() % 0x700 - 0x380;
				v[1] = CrtRand() % 0x700 - 0x380;
				v[2] = -0x400 - CrtRand() % 0x1200;
				Normalize(v, v);
				TransformVectorBy3x3Matrix((const Mat4x3 *)(s + 8), v, v);
				const int32_t r = CrtRand() % 0x12C + 0x64;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(r, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x15E + 0x64;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactSmokeTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		SparkBegin(s, h, SEQ_Smoke);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag198_hyper_shot_held.inc
			FX_HELD(held_note_part(e);)
			SmokeDraw(e, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag198_hyper_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			MoveDecay(e);
			alive++;
		}
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ShotTask (0x5AA570): one trigger press
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
			const int32_t dist = Sqrt(Normalize(s->dir, s->dir));
			uint8_t *t = Entity(n->f1A);
			n->pos[3] = (int16_t)(mul32(*(const int16_t *)(t + 0x26), 2000) >> 12);
			n->f1C = (int16_t)(mul32(ClampedSize(t), 2000) >> 12);
			n->f1E = *(const int16_t *)(Entity(n->f1A) + 0x3C) > -500 ? 1 : 0;
			Node *f = AddTask(QAux(), ORIG_MuzzleFlashTask);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third = (int16_t)((CrtRand() % 0x380 + 0xC80) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *b = AddTask(QAux(), ORIG_BoneSmokeTask);
			const int32_t num = IdCount();
			b->f0E = (int16_t)num;
			IdCount() = num + 1;
			b->counter = 0;
			Node *m = AddTask(QAux(), ORIG_BeamTask);
			m->f0E = n->f0E;
			m->counter = 0;
			m->f1C = 0x3400;
			m->f1E = 0x14C;
			const int16_t half = (int16_t)(dist / 2);
			m->f22 = half;
			m->f20 = half;
			Node *p = AddTask(QAux(), ORIG_SprayTask);
			p->f0E = n->f0E;
			const int32_t num2 = IdCount();
			p->pos[3] = (int16_t)num2;
			IdCount() = num2 + 1;
			p->counter = 0;
		}
		if (n->counter >= 0 && n->counter <= 8 && n->counter % 3 == 0)
		{
			Node *c = AddTask(QAux(), ORIG_MuzzleConeTask);
			c->counter = 0;
			c->f0E = n->f0E;
			c->f18 = (int16_t)(CrtRand() % 0x1000);
			c->f1A = (int16_t)(CrtRand() % 0x96 + 0x6E);
			const int16_t third = (int16_t)((CrtRand() % 0x380 + 0xC80) / 3);
			c->f1E = third;
			c->f1C = third;
		}
		if (n->counter == 2)
		{
			Node *k = AddTask(QAux(), ORIG_ImpactSparkTask);
			k->f0E = n->f0E;
			memcpy(k->pos, n->pos, 8);
			const int32_t num = IdCount();
			k->f20 = (int16_t)num;
			IdCount() = num + 1;
			k->counter = 0;
			Node *g = AddTask(QAux(), ORIG_ImpactSmokeTask);
			memcpy(g->pos, n->pos, 4);
			g->f0E = n->f0E;
			const int32_t num2 = IdCount();
			g->f20 = (int16_t)num2;
			IdCount() = num2 + 1;
			g->counter = 0;
			memcpy(g->pos + 2, n->pos + 2, 4);
		}
		if (n->counter >= 2 && n->counter <= 0xA && n->counter % 3 == 0 && n->f1E != 0)
		{
			Node *r = AddTask(QAux(), ORIG_GroundRingTask);
			memcpy(r->pos, n->pos, 8);
			const int32_t k = ((int32_t)n->f1C << 12) / 2200;
			r->f0E = n->f0E;
			r->counter = 0;
			const int16_t third = (int16_t)((mul32(CrtRand() % 0x380 + 0xA00, k) >> 12) / 3);
			r->f1E = third;
			r->f1C = third;
		}
		if (n->counter == 0xA) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (n->counter == 1) BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		n->counter++;
		return n->counter >= 25 ? TASK_END : 0;
	}

	// FadeOutTask (0x5AE800): level - level / length * counter, 0 at the end (colour 0)
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

	// FadeInTask (0x5AE950): level / length * counter (colour 0)
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5AE9A0): the weapon's ghosts
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
		// 30 fps layer: see mag198_hyper_shot_held.inc
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

	// StartTask (0x5AE860): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5AE740, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 14 ticks
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
		if (n->counter == 0xA) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xE) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	// RootTask (0x5AEC90): packet arena parity, the shot queue, the aux queue, the module tick
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (*(const uint8_t *)&r->counter & 1) ? TexBase() + ARENA_ODD : TexBase();
		const int shots = ExecuteTaskQueue(QShots());
		const int aux = ExecuteTaskQueue(QAux());
		if (!(UpdateFlags() & 0x201)) ModuleTick()++;
		r->counter++;
		return (aux == 0 && shots == 0 && (Flags() & 2)) ? TASK_END : 0;
	}
}

	void register_mag198_hyper_shot()
	{
		register_port(hyper198::ORIG_RootTask, (void *)hyper198::RootTask, "198 RootTask", 198);
		register_port(hyper198::ORIG_StartTask, (void *)hyper198::StartTask, "198 StartTask", 198);
		register_port(hyper198::ORIG_ShotTask, (void *)hyper198::ShotTask, "198 ShotTask", 198);
		register_port(hyper198::ORIG_EndTask, (void *)hyper198::EndTask, "198 EndTask", 198);
		register_port(hyper198::ORIG_FadeOutTask, (void *)hyper198::FadeOutTask, "198 FadeOutTask", 198);
		register_port(hyper198::ORIG_FadeInTask, (void *)hyper198::FadeInTask, "198 FadeInTask", 198);
		register_port(hyper198::ORIG_AfterimageTask, (void *)hyper198::AfterimageTask, "198 AfterimageTask", 198);
		register_port(hyper198::ORIG_MuzzleFlashTask, (void *)hyper198::MuzzleFlashTask, "198 MuzzleFlashTask", 198);
		register_port(hyper198::ORIG_BoneSmokeTask, (void *)hyper198::BoneSmokeTask, "198 BoneSmokeTask", 198);
		register_port(hyper198::ORIG_BeamTask, (void *)hyper198::BeamTask, "198 BeamTask", 198);
		register_port(hyper198::ORIG_SprayTask, (void *)hyper198::SprayTask, "198 SprayTask", 198);
		register_port(hyper198::ORIG_MuzzleConeTask, (void *)hyper198::MuzzleConeTask, "198 MuzzleConeTask", 198);
		register_port(hyper198::ORIG_ImpactSparkTask, (void *)hyper198::ImpactSparkTask, "198 ImpactSparkTask", 198);
		register_port(hyper198::ORIG_ImpactSmokeTask, (void *)hyper198::ImpactSmokeTask, "198 ImpactSmokeTask", 198);
		register_port(hyper198::ORIG_GroundRingTask, (void *)hyper198::GroundRingTask, "198 GroundRingTask", 198);
		// 30 fps layer: see mag198_hyper_shot_held.inc
		FX_HELD(register_mag198_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag198_hyper_shot_held.inc"
#endif
