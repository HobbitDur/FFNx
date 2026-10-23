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

// Effect 194: Flame Shot (Irvine's Shot limit break, kernel.bin section 21 entry 3 +4; MAG_194_SHOT_FLAME_SHOT).
//
// How the battle drives it (as Normal Shot 188, see mag188_normal_shot.cpp): the opening action is a normal magic cast (mode 0);
// every trigger press of the Shot window becomes an action that BattleActionSequence_Tick_Shot 0x50BEE0 runs: it calls the
// module's _Init again with g_MagicCastContext 0x1D99A78 +8 = 2 (a shot) or 1 (the closing action) and waits for +9 done = 1.
//
// Structure (file loader 0x5B4C40 = the texture file 0xD12EB4; _Init 0x5B4C60 keeps the texture base 0x227676C, the context
// 0x2275520 and the caster slot 0x22764C8 (context byte 0), then SetupByMode 0x5B4C90 (both run original): mode 0 = the root
// pool (4 x 0x10, queue 0x2272A18) with RootTask, the shot queue 0x2272A28 (pool 0x2271C08, 100 x 0x24) with StartTask, the aux
// queue 0x2274700 (pool 0x2274710, 100 x 0x24), flags 0x22746F8 / shot index 0x2271BC0 / shot count 0x2276764 / particle id
// count 0x2276768 / flight count 0x2276774 / module tick 0x2271BB8 cleared, both particle arrays (0x2272A38 / 0x2273868, 150 x
// 0x18) free, resetSomeCameraData, the TIM upload; mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue:
// +0x0E = shot count % 100 (its shot record 0x2275528 + 0x28 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the
// first target's slot):
//   RootTask (0x5B7080) - packet cursor 0x2276770 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); every tick the aux queue is empty after EndTask finished (flag 2): a
//     RestoreTask in the root queue; ends when both queues are empty and flag 2 is set.
//   RestoreTask (0x5B7130, root queue) - Battle_RestoreDefaultEffectTexture (0x508630) on its counter 1; ends when it is done.
//   StartTask (0x5B6C50, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xD0BD78, out block 0x2273848), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x2271470 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5B4DF0, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his chain
//     (+0x18) and flag 4; the muzzle (bone 2 of his weapon model, entity +0x78, through 0x5B5540 / 0x5B55C0 = the Renzokuken
//     finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000) to the
//     direction (the rotation angle is kept in its own argument slot, reproduced); its aim point (+0x10) = the centroid of the
//     action's targets' effect anchors (0x5B54A0; +0x16 = the last target's height) + random x / y / z +-1000 (y at most
//     -500); FlashTask (roll, scale = step = (0xB00..0x10FF) / 3), SmokeTask (size 0x400..0xAFF), SpinTask (roll, spin
//     +-(0x3C..0xA9), scale = step = (0x600..0x8FF) / 3) and FlightTask (the aim point; its flight record = the flight count %
//     8); counters 0..2: one more SpinTask each; counter 1: GroundSmokeTask (size 0xB00..0xE7F), the gun sound 0xD12E64
//     (volume 1 / 2 alternating); counter 8: BurstTask at the aim point (scale = step = (0x1400..0x19FF) / 4); counter 9: two
//     RingTasks (the second: spin = 2 / 3 of the first's x the word in its argument slot, which on that call holds the node
//     pointer the executor passed - a never-written local of the original, reproduced; scale - scale / 16), PillarTask and
//     FireTask (the next particle id) at the aim point; counter 11: the action result (ApplyActionResultToTargets 0x506BA0);
//     counter 15: done, flag 4 cleared (the next shot may start); ends at 20.
//   FlashTask (0x5B5640) - prim model 0xD106DC at the shot record (its rotation x roll), scale + step, step - step / 4; faded
//     (counter - 2) << 10 from counter 2; 6 ticks.  SpinTask (0x5B5840) - prim model 0xD11B54 the same way, roll + spin, spin -
//     spin / 16, scale + step, step - step / 4; faded (counter - 2) << 9 from counter 2; 10 ticks.
//   SmokeTask (0x5B57A0) / GroundSmokeTask (0x5B59B0) - sequences 0xD0DB5C / 0xD0DC34 at / flat under the shot record, as Normal
//     Shot's; end with the sequence.
//   FlightTask (0x5B5AC0; nothing drawn) - counter 0: its flight record 0x2271478 + 0xE8 * k (start = the shot record point,
//     end = the aim point, a 3-point spline start -> start + muzzle direction * half the x / z distance -> aim point through
//     Effect_SplineSetup 0x571620, 24 points by Effect_SplineEval 0x571690, t = i * 0x1000 / 23), then HeadTask (size
//     0x400..0x57F), TrailTask and EmberTask (each the next particle id) on that record, all at point 0 with speed 5; ends at 18.
//   HeadTask (0x5B5CF0) - sequence 0xD0DCA8 (frame 0) at flight point +0x18, camera facing; point + speed, speed - 1 while > 2;
//     ends past point 23.
//   TrailTask (0x5B5DB0) - every particle of its array (flight record parity; the array pointer is kept in its own argument slot,
//     reproduced) with its id: sequence 0xD0D938 at frame * 2 + 1, camera facing with its size (depth size / 16); then (not on
//     draw-only ticks) frame + 1, dies when its sequence ended, else size + size / 32; point + speed (as the head); new particles
//     on every flight point the head passed this tick (size 0x300..0x3FF); ends past counter 3 with no particle left.
//   EmberTask (0x5B5FB0) - raw GTE quads (Field_Alloc scratch: the camera as light matrix and back colour; three corners
//     (-0x100,-0x100) / (0x100,-0x100) / (-0x100,0x100)): every particle with its id: V0 = its point through the light matrix
//     (MAC = the view-space point), then (not on draw-only ticks) frame + 1 (dies at 8), else point + velocity, velocity -
//     velocity / 16; R = its size, TR = MAC, RTPT of the corners, a 0x28-byte packet (colour 0xD12E94[frame], uv / clut words of
//     the scratch, the 4th corner = (x1, y2)) inserted by its depth unless the GTE flag rejects it; point + speed; 4 new
//     particles on every flight point the head passed (offset +-200, size 0x180..0x27F, velocity along a random direction
//     around the flight direction, speed 0xF..0x31; at point 23 the direction of 22 -> 23 and every later particle of that
//     point on point 22 - vanilla index swap, reproduced); ends past counter 7 when it drew no quad.
//   BurstTask (0x5B64A0) - prim model 0xD0FB0C at the aim point, camera facing (TransformCameraByShadowRotation with its scale);
//     scale + step, step - step / 6; faded (counter - 1) * 0x2AA from counter 1; 7 ticks.
//   RingTask (0x5B6580) - prim model 0xD0DCE4 at the aim point (yaw), uniform scale; yaw + spin, spin - spin / 12, scale + step,
//     step - step / 4; faded (counter - 2) * 0x155 from counter 2; 14 ticks.
//   PillarTask (0x5B66F0) - model 0xD1283C through MAG_151's renderer 0x64E080 at the aim point (yaw), uniform scale, v scroll
//     -12 * counter; yaw + spin (0), scale + step, step - step / 8; faded (counter - 8) << 9 from counter 8; 16 ticks.
//   FireTask (0x5B6880) - counters 0..9 (not on draw-only ticks): 3 new particles around the aim point (radius 400 * (counter + 1)
//     along a random angle, y +-550, size 0xA00..0x12FF); then every particle with its id: sequence 0xD0D938 at its frame, its
//     point through the camera (light matrix), R = its size, pulled toward the camera by size / 16; then (not on draw-only
//     ticks) frame + 1, dies when its sequence ended; ends when none is left.
//   EndTask (0x5B6B30, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick; counter 4: the action result; after 12 ticks: flag 2, done; ends.
//   FadeInTask (0x5B6D40) / FadeOutTask (0x5B6BF0) - screen flash (Effect_SetScreenFlash 0x5713E0) 0 -> 0x800 / 0x800 -> 0
//     over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5B6D90) - the weapon's ghosts as Normal Shot's (ghost slots 0x22764D0, current bones 0x2274678, colours
//     0xD12E80; BoneSnapshot 0x5B6F40 / BoneRestore 0x5B6F80 / GhostDraw 0x5B6FC0; the ghost count kept in its own argument slot).
// Every task except RootTask / RestoreTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x2271470..0x2276778 (end tick, flight records, module tick, shot index, pools, queues, particle arrays, bind
//   block, saved bones 0x2274678, flags 0x22746F8, aux queue, context, shot records, caster slot, texture file, ghost slots,
//   counts, texture base, packet cursor, flight count).

#include "mag_common.h"

namespace ff8fx
{
namespace flame194
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x2271470); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x2271BB8); }
	inline uint32_t &Flags() { return var<uint32_t>(0x22746F8); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x2275520); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x22764C8); }
	inline int32_t &PartCount() { return var<int32_t>(0x2276768); }     // particle ids (+ 1)
	inline uint32_t &TexBase() { return var<uint32_t>(0x227676C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2276770); }
	inline int32_t &FlightCount() { return var<int32_t>(0x2276774); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2272A18; }
	inline TaskQueue *QShots() { return (TaskQueue *)0x2272A28; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x2274700; }
	static const uint32_t PARTS_A = 0x2272A38, PARTS_B = 0x2273868;  // 150 x 0x18 each
	static const int32_t PARTS_N = 150;
	static const uint32_t FLIGHTS = 0x2271478;                       // 8 x 0xE8 flight records
	static const uint32_t SHOTS = 0x2275528;                         // 100 x 0x28 shot records
	static const uint32_t BIND_OUT = 0x2273848;
	static const uint32_t BONES_NOW = 0x2274678;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x22764D0, GHOSTS_END = 0x2276764; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;
	static const int32_t FLIGHT_POINTS = 24;

	static const uint32_t ORIG_RootTask = 0x5B7080;
	static const uint32_t ORIG_RestoreTask = 0x5B7130;
	static const uint32_t ORIG_StartTask = 0x5B6C50;
	static const uint32_t ORIG_ShotTask = 0x5B4DF0;
	static const uint32_t ORIG_EndTask = 0x5B6B30;
	static const uint32_t ORIG_FadeOutTask = 0x5B6BF0;
	static const uint32_t ORIG_FadeInTask = 0x5B6D40;
	static const uint32_t ORIG_AfterimageTask = 0x5B6D90;
	static const uint32_t ORIG_FlashTask = 0x5B5640;
	static const uint32_t ORIG_SmokeTask = 0x5B57A0;
	static const uint32_t ORIG_SpinTask = 0x5B5840;
	static const uint32_t ORIG_GroundSmokeTask = 0x5B59B0;
	static const uint32_t ORIG_FlightTask = 0x5B5AC0;
	static const uint32_t ORIG_HeadTask = 0x5B5CF0;
	static const uint32_t ORIG_TrailTask = 0x5B5DB0;
	static const uint32_t ORIG_EmberTask = 0x5B5FB0;
	static const uint32_t ORIG_BurstTask = 0x5B64A0;
	static const uint32_t ORIG_RingTask = 0x5B6580;
	static const uint32_t ORIG_PillarTask = 0x5B66F0;
	static const uint32_t ORIG_FireTask = 0x5B6880;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD0BD78;
	static const uint32_t MODEL_Flash = 0xD106DC, MODEL_Spin = 0xD11B54, MODEL_Burst = 0xD0FB0C, MODEL_Ring = 0xD0DCE4, MODEL_Pillar = 0xD1283C;
	static const uint32_t SEQ_Smoke = 0xD0DB5C, SEQ_GroundSmoke = 0xD0DC34, SEQ_Head = 0xD0DCA8, SEQ_Flame = 0xD0D938;
	static const void *const SOUND_Gun = (const void *)0xD12E64;
	static const int32_t *const MUZZLE_REF = (const int32_t *)0xD12E70; // (0, 0, -0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD12E80;
	static const uint32_t *const EMBER_COLOURS = (const uint32_t *)0xD12E94; // 8 packet colour words (code 0x2E)

	// engine functions not in fx_port.h / mag_common.h
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); } // Math_SqrtInt
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); } // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline uint32_t Mag151Render(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64E080)(h, ot, mode, cursor); } // MAG_151_sub_64E080
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = m.t
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }             // MAC = LLM * V0 + BK
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }   // R = s * I
	inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                   // TR = MAC1..3
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteReadSZ3(void *dst) { fn<void (__cdecl *)(void *)>(0x45E220)(dst); }
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
		int16_t f0E;     // +0x0E shot: shot number; muzzle tasks / flight / aim-point tasks: their shot record; head / trail /
		                 //       ember: their flight record; fades: length
		int16_t pos[4];  // +0x10 shot / flight / aim-point tasks: the aim point; trail / ember / fire: +0x16 = particle id - 1
		int16_t f18;     // +0x18 shot: chain; flash / spin: roll; ring / pillar: yaw; flight: its flight record; head / trail /
		                 //       ember: flight point; fades: level
		int16_t f1A;     // +0x1A shot: target slot; spin / ring / pillar: spin; head / trail / ember: speed
		int16_t f1C;     // +0x1C scale / size
		int16_t f1E;     // +0x1E scale step
		uint8_t pad20[4];
	};
	struct ShotRec // 0x2275528 + 0x28 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct Flight // 0x2271478 + 0xE8 * k
	{
		int16_t start[4];      // the shot record point
		int16_t target[4];     // the aim point
		int16_t ctrl[3][4];    // the spline's control points (ctrl[1][3] never written)
		int16_t pts[24][4];    // the flight points
	};
	struct Particle // 0x2272A38 / 0x2273868, 150 x 0x18
	{
		int32_t id;      // 0 = free, else the task's +0x16 + 1
		int16_t frame;
		int16_t size;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x22764D0, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Flame Shot nodes");
	static_assert(sizeof(ShotRec) == 0x28 && sizeof(Flight) == 0xE8 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Flame Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x28 * i); }
	inline Flight *FlightRec(int32_t k) { return (Flight *)(FLIGHTS + 0xE8 * k); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t k) { return (Particle *)((k & 1) ? PARTS_B : PARTS_A); }
	inline Particle *FindFree(Particle *arr) { for (int32_t i = 0; i < PARTS_N; i++) if (arr[i].id == 0) return &arr[i]; return nullptr; }
}
}

#ifdef FF8_FX_HELD
#include "mag194_flame_shot_held.h"
#endif

namespace ff8fx
{
namespace flame194
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5B55C0 / 0x5B5540 = Rough Divide's 0x5F6500 / 0x5F6480)
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

	// the centroid of the targets' effect anchors (0x5B54A0): 16-bit sums / count; out[3] = the last target's height
	static void Centroid(const uint8_t *targets, int32_t count, int16_t *out)
	{
		int16_t sx = 0, sy = 0, sz = 0;
		int16_t p[4];
		for (int32_t i = 0; i < count; i++)
		{
			GetDefaultEffectPosition(Entity(targets[i * TARGET_STRIDE]), p);
			sx = (int16_t)(sx + p[0]);
			sy = (int16_t)(sy + p[1]);
			sz = (int16_t)(sz + p[2]);
		}
		p[0] = (int16_t)(sx / count);
		p[1] = (int16_t)(sy / count);
		p[2] = (int16_t)(sz / count);
		memcpy(out, p, 8);
	}

	// ------------------------------------------------------------------
	// Afterimages (0x5B6F40 / 0x5B6F80 / 0x5B6FC0)
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
	// prim model header (0x58 bytes on the scratch stack): +0x0C fade, +0x1C flags
	static void ModelDraw(uint32_t model, bool faded, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (faded)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// a muzzle model at the shot record: its rotation x roll, uniform scale
	static void MuzzleModel(const Node *n, uint32_t model, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->f18;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply(&s->m, &m);
		m.t[0] = s->pos[0];
		m.t[1] = s->pos[1];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		m.t[2] = s->pos[2];
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		ModelDraw(model, faded, fade);
	}
	static int32_t FlashFade(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 10); }
	static int32_t SpinFade(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 9); }
	static int32_t BurstFade(int32_t c) { return mul32(c, 0x2AA) - 0x2AA; }
	static int32_t RingFade(int32_t c) { return mul32(c - 2, 0x155); }
	static int32_t PillarFade(int32_t c) { return (int32_t)((uint32_t)(c - 8) << 9); }

	// the aim-point models: yaw, uniform scale; the world matrix into the GTE
	static void AimMatrix(const Node *n)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->pos[0];
		m.t[1] = n->pos[1];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		m.t[2] = n->pos[2];
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}
	static void RingDraw(const Node *n, bool faded, int32_t fade)
	{
		AimMatrix(n);
		ModelDraw(MODEL_Ring, faded, fade);
	}
	// the pillar: MAG_151's renderer, v scroll -12 * counter
	static void PillarDraw(const Node *n, int32_t scroll, bool faded, int32_t fade)
	{
		AimMatrix(n);
		uint8_t *h = (uint8_t *)FieldAlloc(0x7C);
		*(uint32_t *)h = MODEL_Pillar;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x1A) = 0;
		*(int16_t *)(h + 0x1C) = 0x80;
		*(int16_t *)(h + 0x1E) = 0x40;
		*(int32_t *)(h + 0x10) = scroll;
		*(int32_t *)(h + 0x14) = 0;
		if (faded)
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = Mag151Render(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x7C);
	}
	// the burst: camera facing at the aim point (the shadow rotation scaled by its scale)
	static void BurstDraw(const Node *n, bool faded, int32_t fade)
	{
		TransformCameraByShadowRotation(n->pos, n->f1C, -((int32_t)n->f1C >> 3));
		ModelDraw(MODEL_Burst, faded, fade);
	}

	// the smoke at the shot record (camera facing): returns the sequence's +0x28 (< 0: ended)
	static int16_t SmokeDraw(const Node *n)
	{
		TransformCameraByShadowRotation(Shot(n->f0E)->pos, n->f1C, -((int32_t)n->f1C >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Smoke;
		*(int16_t *)(h + 4) = n->counter;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		return *(const int16_t *)(h + 0x28);
	}

	// the ground smoke under the shot record (flat: x 0x400), scaled
	static int16_t GroundSmokeDraw(const Node *n)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0x400;
		ang[1] = 0;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = s->pos[0];
		m.t[1] = 0;
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		m.t[2] = s->pos[2];
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_GroundSmoke;
		*(int16_t *)(h + 4) = n->counter;
		*(int16_t *)(h + 0x24) = 8;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		return *(const int16_t *)(h + 0x28);
	}

	// the flight head at a point (camera facing, frame 0)
	static void HeadDraw(const int16_t *pt, int16_t size)
	{
		TransformCameraByShadowRotation(pt, size, -((int32_t)size >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(int16_t *)(h + 4) = 0;
		*(int16_t *)(h + 0x24) = 0;
		*(uint32_t *)h = SEQ_Head;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// one trail particle (frame * 2 + 1, camera facing with its size, depth size / 16)
	static void TrailDraw(const Particle *e, uint8_t *h)
	{
		*(int16_t *)(h + 4) = (int16_t)((e->frame << 1) + 1);
		TransformCameraByShadowRotation(e->pos, e->size, -((int32_t)e->size >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// the ember quads' scratch (0x44 bytes): +0 the OT, +4 the depth shift, +8 the GTE flag, +0xC SZ3, the three corners at
	// +0x10 / +0x18 / +0x20, the packet uv / clut / tpage words +0x30..+0x43
	static void EmberSetup(uint8_t *q)
	{
		q[0x3C] = 0x80;
		q[0x34] = 0x80;
		*(uint32_t *)(q + 0x30) = 0x2E808080;
		q[0x40] = 0xA0;
		q[0x38] = 0xA0;
		*(uint16_t *)(q + 0x36) = 0x3E14;
		q[0x39] = 0x40;
		q[0x35] = 0x40;
		*(uint16_t *)(q + 0x3A) = 0x38;
		q[0x41] = 0x60;
		q[0x3D] = 0x60;
		*(int16_t *)(q + 0x18) = 0x100;
		*(int16_t *)(q + 0x10) = -0x100;
		*(int16_t *)(q + 0x12) = -0x100;
		*(int16_t *)(q + 0x1A) = -0x100;
		*(int16_t *)(q + 0x20) = -0x100;
		*(int16_t *)(q + 0x22) = 0x100;
		*(int16_t *)(q + 0x24) = 0;
		*(int16_t *)(q + 0x1C) = 0;
		*(int16_t *)(q + 0x14) = 0;
	}
	// the light matrix / back colour = the camera (into the 0x50-byte scratch at +0x10)
	static void EmberLight(uint8_t *s)
	{
		memcpy(s + 0x10, &Camera(), 0x20);
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
	}
	// one ember: V0 through the light matrix (MAC = the view-space point)
	static void EmberLoad(const int16_t *pos)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
	}
	// ... its quad at the MAC point, scaled by its size, colour `colour`: true when inserted
	static bool EmberQuad(uint8_t *q, int16_t size, uint32_t colour, uint8_t *pk)
	{
		*(uint32_t *)(pk + 4) = colour;
		GteSetRotScale(size);
		GteTransFromMAC();
		GteLoadV012(q + 0x10, q + 0x18, q + 0x20);
		GteRTPT();
		*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(q + 0x3C);
		*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(q + 0x34);
		*(uint32_t *)pk = 0x9000000;
		*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(q + 0x38);
		*(uint32_t *)(pk + 0x24) = *(const uint32_t *)(q + 0x40);
		GteReadFLAG(q + 8);
		if (*(const uint32_t *)(q + 8) & 0x60000) return false;
		GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
		*(uint16_t *)(pk + 0x20) = *(const uint16_t *)(pk + 0x10);
		*(uint16_t *)(pk + 0x22) = *(const uint16_t *)(pk + 0x1A);
		GteReadSZ3(q + 0xC);
		InsertPrimAutoDepth(*(const uint32_t *)q + (*(const int32_t *)(q + 0xC) >> *(const int32_t *)(q + 4)) * 4, pk);
		return true;
	}

	// one fire particle: its point through the camera, R = its size, pulled toward the camera by size / 16
	static void FireDraw(const Particle *e, uint8_t *s, uint8_t *h)
	{
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(e->size);
		*(int16_t *)(h + 4) = e->frame;
		int32_t *v = (int32_t *)(s + 0x38), *d = (int32_t *)(s + 0x28);
		GteReadMAC123(v);
		Normalize(v, d);
		const int32_t k = -((int32_t)e->size >> 4);
		v[0] += mul32(d[0], k) >> 12;
		v[1] += mul32(d[1], k) >> 12;
		v[2] += mul32(d[2], k) >> 12;
		GteSetTransFromVec32(v);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void FireLight(uint8_t *s)
	{
		memcpy(s + 8, &Camera(), 0x20);
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// FlashTask (0x5B5640): 6 ticks, scale + step, step - step / 4, faded from counter 2
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_FlashTask, n);)
		MuzzleModel(n, MODEL_Flash, n->counter >= 2, FlashFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 6 ? TASK_END : 0;
	}

	// SpinTask (0x5B5840): 10 ticks, roll + spin, spin - spin / 16, scale + step, step - step / 4, faded from counter 2
	static uint32_t __cdecl SpinTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_SpinTask, n);)
		MuzzleModel(n, MODEL_Spin, n->counter >= 2, SpinFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A, step = n->f1E;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin >> 4));
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// SmokeTask (0x5B57A0): ends with its sequence
	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_SmokeTask, n);)
		const int16_t next = SmokeDraw(n);
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// GroundSmokeTask (0x5B59B0): ends with its sequence
	static uint32_t __cdecl GroundSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundSmokeTask, n);)
		const int16_t next = GroundSmokeDraw(n);
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// FlightTask (0x5B5AC0): the flight record and its three flame tasks; ends at 18
	static uint32_t __cdecl FlightTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Flight *t = FlightRec(n->f18);
		const ShotRec *s = Shot(n->f0E);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			memcpy(t->start, s->pos, 8);
			memcpy(t->target, n->pos, 8);
			int32_t v[3];
			TransformVectorBy3x3Matrix(&s->m, MUZZLE_REF, v);
			const int32_t dx = t->target[0] - t->start[0];
			const int32_t dz = t->target[2] - t->start[2];
			const int32_t half = Sqrt(mul32(dz, dz) + mul32(dx, dx)) >> 1;
			memcpy(t->ctrl[0], t->start, 8);
			t->ctrl[1][0] = (int16_t)((mul32(v[0], half) >> 12) + t->start[0]);
			t->ctrl[1][1] = (int16_t)((mul32(v[1], half) >> 12) + t->start[1]);
			t->ctrl[1][2] = (int16_t)((mul32(v[2], half) >> 12) + t->start[2]);
			memcpy(t->ctrl[2], t->target, 8);
			void *work = FieldAlloc(0x190);
			SplineSetup(3, t->ctrl, work);
			for (int32_t i = 0; i < FLIGHT_POINTS; i++) SplineEval(3, work, t->pts[i], (i * 0x1000) / 23);
			FieldFree(0x190);
			Node *hd = AddTask(QAux(), ORIG_HeadTask);
			hd->counter = 0;
			hd->f0E = n->f18;
			hd->f18 = 0;
			hd->f1A = 5;
			hd->f1C = (int16_t)(CrtRand() % 0x180 + 0x400);
			Node *tr = AddTask(QAux(), ORIG_TrailTask);
			tr->pos[3] = (int16_t)PartCount();
			PartCount()++;
			tr->counter = 0;
			tr->f0E = n->f18;
			tr->f18 = 0;
			tr->f1A = 5;
			Node *em = AddTask(QAux(), ORIG_EmberTask);
			em->f0E = n->f18;
			em->pos[3] = (int16_t)PartCount();
			PartCount()++;
			em->counter = 0;
			em->f18 = 0;
			em->f1A = 5;
		}
		n->counter++;
		return n->counter >= 18 ? TASK_END : 0;
	}

	// the head / trail / ember flight step: point + speed, speed - 1 while > 2
	static void FlightStep(Node *n)
	{
		const int16_t speed = n->f1A;
		n->f18 = (int16_t)(n->f18 + speed);
		if (speed > 2) n->f1A = (int16_t)(speed - 1);
	}

	// HeadTask (0x5B5CF0): the flight head; ends past point 23
	static uint32_t __cdecl HeadTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_HeadTask, n);)
		HeadDraw(FlightRec(n->f0E)->pts[n->f18], n->f1C);
		if (UpdateFlags() & 0x201) return 0;
		FlightStep(n);
		n->counter++;
		return n->f18 >= FLIGHT_POINTS ? TASK_END : 0;
	}

	// TrailTask (0x5B5DB0): its particles (draw THEN update), then new ones on the points the head passed
	static uint32_t __cdecl TrailTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		// (the original keeps the array pointer in its own argument slot)
		*(volatile uint32_t *)&n_ = (uint32_t)arr;
		Flight *t = FlightRec(n->f0E);
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_TrailTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(int16_t *)(h + 0x24) = 0;
		const int32_t id = n->pos[3] + 1;
		*(uint32_t *)h = SEQ_Flame;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag194_flame_shot_held.inc
			FX_HELD(held_note_part(e);)
			TrailDraw(e, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag194_flame_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->size = (int16_t)(e->size + (int16_t)(e->size >> 5));
			alive++;
		}
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		FlightStep(n);
		if (n->f18 < FLIGHT_POINTS)
		{
			for (int32_t k = n->f18; k >= n->f18 - n->f1A; k--)
			{
				Particle *e = FindFree(arr);
				if (!e) break;
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x100 + 0x300);
				memcpy(e->pos, t->pts[k], 8);
			}
		}
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// EmberTask (0x5B5FB0): its quads (the update between the view-space point and the quad), then new embers
	static uint32_t __cdecl EmberTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		Flight *t = FlightRec(n->f0E);
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_EmberTask, n);)
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		uint8_t *q = (uint8_t *)FieldAlloc(0x44);
		EmberSetup(q);
		int32_t prims = 0;
		EmberLight(s);
		uint8_t *pk = (uint8_t *)PacketCursor();
		const int32_t id = n->pos[3] + 1;
		*(uint32_t *)q = RenderOT();
		*(int32_t *)(q + 4) = 4;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			const int32_t frame = e->frame;
			// 30 fps layer: see mag194_flame_shot_held.inc
			FX_HELD(held_note_part(e);)
			EmberLoad(e->pos);
			const uint32_t colour = EMBER_COLOURS[frame];
			if (!(UpdateFlags() & 0x201))
			{
				e->frame = (int16_t)(frame + 1);
				if (frame + 1 >= 8)
				{
					e->id = 0;
					// 30 fps layer: see mag194_flame_shot_held.inc
					FX_HELD(held_note_part_next(e);)
				}
				else
				{
					e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
					e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
					e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
					e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 4));
					e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 4));
					e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 4));
				}
			}
			if (EmberQuad(q, e->size, colour, pk))
			{
				pk += 0x28;
				prims++;
			}
		}
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_alive(prims);)
		FieldFree(0x44);
		PacketCursor() = (uint32_t)pk;
		FieldFree(0x50);
		if (UpdateFlags() & 0x201) return 0;
		uint8_t *w = (uint8_t *)FieldAlloc(0x50);
		FlightStep(n);
		if (n->f18 < FLIGHT_POINTS)
		{
			int32_t k = n->f18, b = k;
			if (k >= n->f18 - n->f1A)
			{
				do
				{
					for (int32_t j = 0; j < 4; j++)
					{
						Particle *e = FindFree(arr);
						if (!e) break;
						e->id = id;
						e->frame = 0;
						e->size = (int16_t)(CrtRand() % 0x100 + 0x180);
						memcpy(e->pos, t->pts[k], 8);
						e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 400 - 200));
						e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 400 - 200));
						e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 400 - 200));
						// the flight direction at the point: k -> k + 1 (at the last point: k - 1 -> k, and from then on the
						// point index is k - 1: vanilla register swap)
						int32_t a = k + 1;
						if (a >= FLIGHT_POINTS)
						{
							a = k;
							k = k - 1;
							b = k;
						}
						int32_t *dir = (int32_t *)(w + 0x30), *axis = (int32_t *)(w + 0x40);
						dir[0] = t->pts[a][0] - t->pts[b][0];
						dir[1] = t->pts[a][1] - t->pts[b][1];
						dir[2] = t->pts[a][2] - t->pts[b][2];
						Normalize(dir, dir);
						const int32_t angle = RotationBetweenVectors(MUZZLE_REF, dir, axis);
						BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(w + 0x10), axis);
						const int32_t r = CrtRand() % 0x1000;
						dir[0] = ComputeSin(r);
						dir[1] = ComputeCos(r);
						dir[2] = CrtRand() % 0x800;
						TransformVectorBy3x3Matrix((const Mat4x3 *)(w + 0x10), dir, dir);
						const int32_t sp = CrtRand() % 0x23 + 0xF;
						e->vel[0] = (int16_t)(mul32(dir[0], sp) >> 12);
						e->vel[1] = (int16_t)(mul32(dir[1], sp) >> 12);
						e->vel[2] = (int16_t)(mul32(dir[2], sp) >> 12);
						k = b;
					}
					k--;
					b = k;
				} while (k >= n->f18 - n->f1A);
			}
		}
		FieldFree(0x50);
		n->counter++;
		return (n->counter >= 8 && prims == 0) ? TASK_END : 0;
	}

	// BurstTask (0x5B64A0): 7 ticks, scale + step, step - step / 6, faded from counter 1
	static uint32_t __cdecl BurstTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_BurstTask, n);)
		BurstDraw(n, n->counter >= 1, BurstFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 6));
		n->counter++;
		return n->counter >= 7 ? TASK_END : 0;
	}

	// RingTask (0x5B6580): 14 ticks, yaw + spin, spin - spin / 12, scale + step, step - step / 4, faded from counter 2
	static uint32_t __cdecl RingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_RingTask, n);)
		RingDraw(n, n->counter >= 2, RingFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 12));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 14 ? TASK_END : 0;
	}

	// PillarTask (0x5B66F0): 16 ticks, yaw + spin, spin - spin / 12, scale + step, step - step / 8, faded from counter 8
	static uint32_t __cdecl PillarTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_PillarTask, n);)
		PillarDraw(n, mul32(n->counter, -12), n->counter >= 8, PillarFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 12));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 8));
		n->counter++;
		return n->counter >= 16 ? TASK_END : 0;
	}

	// FireTask (0x5B6880): new flames on counters 0..9, then its flames (draw THEN update); ends when none is left
	static uint32_t __cdecl FireTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->pos[3] + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 9)
		{
			for (int32_t j = 0; j < 3; j++)
			{
				Particle *e = FindFree(arr);
				if (!e) break;
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x900 + 0xA00);
				const int32_t a = CrtRand() % 0x1000;
				const int32_t r = (n->counter + 1) * 400;
				memcpy(e->pos, n->pos, 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(mul32(ComputeSin(a), r) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 0x44C - 0x226));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(mul32(ComputeCos(a), r) >> 12));
			}
		}
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_node(ORIG_FireTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(int16_t *)(h + 0x24) = 0;
		FireLight(s);
		*(uint32_t *)h = SEQ_Flame;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag194_flame_shot_held.inc
			FX_HELD(held_note_part(e);)
			FireDraw(e, s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag194_flame_shot_held.inc
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			alive++;
		}
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// a spinning muzzle model: roll, spin +-(0x3C..0xA9), scale = step = (0x600..0x8FF) / 3
	static void AddSpin(const Node *n)
	{
		Node *p = AddTask(QAux(), ORIG_SpinTask);
		p->counter = 0;
		p->f0E = n->f0E;
		p->f18 = (int16_t)(CrtRand() % 0x1000);
		p->f1A = (int16_t)(CrtRand() % 0x6E + 0x3C);
		if (p->f18 & 1) p->f1A = (int16_t)-p->f1A;
		const int16_t third = (int16_t)((CrtRand() % 0x300 + 0x600) / 3);
		p->f1E = third;
		p->f1C = third;
	}
	// an aim-point node: the shot's aim point, counter 0, its shot record
	static Node *AddAim(const Node *n, uint32_t fn)
	{
		Node *p = AddTask(QAux(), fn);
		memcpy(p->pos, n->pos, 8);
		p->counter = 0;
		p->f0E = n->f0E;
		return p;
	}

	// ShotTask (0x5B4DF0): one trigger press
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
		if (n->counter == 15)
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
			const int32_t angle = RotationBetweenVectors(MUZZLE_REF, dir, axis);
			// (the original keeps the rotation angle in its own argument slot)
			*(volatile int32_t *)&n_ = angle;
			BuildAxisAngleRotationMatrix(angle, &s->m, axis);
			Centroid(Ctx()->actions->targets, Ctx()->actions->target_count, n->pos);
			n->pos[0] = (int16_t)(n->pos[0] + (CrtRand() % 2000 - 1000));
			n->pos[1] = (int16_t)(n->pos[1] + (CrtRand() % 2000 - 1000));
			if (n->pos[1] > -500) n->pos[1] = -500;
			n->pos[2] = (int16_t)(n->pos[2] + (CrtRand() % 2000 - 1000));
			Node *f = AddTask(QAux(), ORIG_FlashTask);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third = (int16_t)((CrtRand() % 0x600 + 0xB00) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *k = AddTask(QAux(), ORIG_SmokeTask);
			k->counter = 0;
			k->f0E = n->f0E;
			k->f1C = (int16_t)(CrtRand() % 0x700 + 0x400);
			AddSpin(n);
			Node *l = AddTask(QAux(), ORIG_FlightTask);
			l->f0E = n->f0E;
			memcpy(l->pos, n->pos, 8);
			const int32_t fc = FlightCount();
			l->counter = 0;
			l->f18 = (int16_t)(fc % 8);
			FlightCount() = fc + 1;
		}
		if (n->counter == 1)
		{
			Node *d = AddTask(QAux(), ORIG_GroundSmokeTask);
			d->counter = 0;
			d->f0E = n->f0E;
			d->f1C = (int16_t)(CrtRand() % 0x380 + 0xB00);
		}
		if (n->counter >= 0 && n->counter <= 2) AddSpin(n);
		if (n->counter == 8)
		{
			Node *b = AddAim(n, ORIG_BurstTask);
			const int16_t quarter = (int16_t)((CrtRand() % 0x600 + 0x1400) / 4);
			b->f1E = quarter;
			b->f1C = quarter;
		}
		if (n->counter == 9)
		{
			Node *r = AddAim(n, ORIG_RingTask);
			r->f18 = (int16_t)(CrtRand() % 0x1000);
			r->f1A = (int16_t)(CrtRand() % 0xA0 + 0x5A);
			if (r->f18 & 1) r->f1A = (int16_t)-r->f1A;
			const int16_t third = (int16_t)((CrtRand() % 0x400 + 0xB80) / 3);
			r->f1E = third;
			r->f1C = third;
			Node *r2 = AddAim(n, ORIG_RingTask);
			r2->f18 = (int16_t)(CrtRand() % 0x1000);
			// (2 / 3 of the first ring's spin times a never-written local of the original: its argument slot, which on this call
			// holds the node pointer the executor passed)
			r2->f1A = (int16_t)mul32((2 * (int32_t)r->f1A) / 3, *(volatile int32_t *)&n_);
			r2->f1C = (int16_t)(r->f1C - (int16_t)(r->f1C >> 4));
			r2->f1E = r->f1E;
			Node *p = AddAim(n, ORIG_PillarTask);
			p->f18 = (int16_t)(CrtRand() % 0x1000);
			p->f1A = 0;
			const int16_t fifth = (int16_t)((CrtRand() % 0x400 + 0x1200) / 5);
			p->f1E = fifth;
			p->f1C = fifth;
			Node *fi = AddTask(QAux(), ORIG_FireTask);
			fi->f0E = n->f0E;
			memcpy(fi->pos, n->pos, 8);
			fi->pos[3] = (int16_t)PartCount();
			PartCount()++;
			fi->counter = 0;
		}
		if (n->counter == 0xB) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (n->counter == 1) BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		n->counter++;
		return n->counter >= 20 ? TASK_END : 0;
	}

	// FadeOutTask (0x5B6BF0): level - level / length * counter, 0 at the end
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

	// FadeInTask (0x5B6D40): level / length * counter
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5B6D90): the weapon's ghosts
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
		// 30 fps layer: see mag194_flame_shot_held.inc
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

	// StartTask (0x5B6C50): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5B6B30, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 12 ticks
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
		if (n->counter == 4) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xC) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	// RestoreTask (0x5B7130, root queue): the default effect texture back on its counter 1; ends when done
	static uint32_t __cdecl RestoreTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		if (r->counter == 1) TextureRestoreTask((void *)TexBase(), &r->done);
		const int16_t done = r->done;
		r->counter++;
		return done ? TASK_END : 0;
	}

	// RootTask (0x5B7080): packet arena parity, the shot queue, the aux queue, the module tick, the texture restore
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag194_flame_shot_held.inc
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

	void register_mag194_flame_shot()
	{
		register_port(flame194::ORIG_RootTask, (void *)flame194::RootTask, "194 RootTask", 194);
		register_port(flame194::ORIG_RestoreTask, (void *)flame194::RestoreTask, "194 RestoreTask", 194);
		register_port(flame194::ORIG_StartTask, (void *)flame194::StartTask, "194 StartTask", 194);
		register_port(flame194::ORIG_ShotTask, (void *)flame194::ShotTask, "194 ShotTask", 194);
		register_port(flame194::ORIG_EndTask, (void *)flame194::EndTask, "194 EndTask", 194);
		register_port(flame194::ORIG_FadeOutTask, (void *)flame194::FadeOutTask, "194 FadeOutTask", 194);
		register_port(flame194::ORIG_FadeInTask, (void *)flame194::FadeInTask, "194 FadeInTask", 194);
		register_port(flame194::ORIG_AfterimageTask, (void *)flame194::AfterimageTask, "194 AfterimageTask", 194);
		register_port(flame194::ORIG_FlashTask, (void *)flame194::FlashTask, "194 FlashTask", 194);
		register_port(flame194::ORIG_SmokeTask, (void *)flame194::SmokeTask, "194 SmokeTask", 194);
		register_port(flame194::ORIG_SpinTask, (void *)flame194::SpinTask, "194 SpinTask", 194);
		register_port(flame194::ORIG_GroundSmokeTask, (void *)flame194::GroundSmokeTask, "194 GroundSmokeTask", 194);
		register_port(flame194::ORIG_FlightTask, (void *)flame194::FlightTask, "194 FlightTask", 194);
		register_port(flame194::ORIG_HeadTask, (void *)flame194::HeadTask, "194 HeadTask", 194);
		register_port(flame194::ORIG_TrailTask, (void *)flame194::TrailTask, "194 TrailTask", 194);
		register_port(flame194::ORIG_EmberTask, (void *)flame194::EmberTask, "194 EmberTask", 194);
		register_port(flame194::ORIG_BurstTask, (void *)flame194::BurstTask, "194 BurstTask", 194);
		register_port(flame194::ORIG_RingTask, (void *)flame194::RingTask, "194 RingTask", 194);
		register_port(flame194::ORIG_PillarTask, (void *)flame194::PillarTask, "194 PillarTask", 194);
		register_port(flame194::ORIG_FireTask, (void *)flame194::FireTask, "194 FireTask", 194);
		// 30 fps layer: see mag194_flame_shot_held.inc
		FX_HELD(register_mag194_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag194_flame_shot_held.inc"
#endif
