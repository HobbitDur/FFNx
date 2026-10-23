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

// Effect 193: Dark Shot (Irvine's Shot limit break, kernel.bin section 21 entry 2 +4; MAG_193_SHOT_DARK_SHOT).
//
// How the battle drives it (as Normal Shot 188, see mag188_normal_shot.cpp): the opening action is a normal magic cast (mode 0);
// every trigger press of the Shot window becomes an action that BattleActionSequence_Tick_Shot 0x50BEE0 runs: it calls the
// module's _Init again with g_MagicCastContext 0x1D99A78 +8 = 2 (a shot) or 1 (the closing action) and waits for +9 done = 1.
//
// Structure (file loader 0x5B7170 = the texture file 0xD155DC; _Init 0x5B7190 keeps the texture base 0x227B32C, the context
// 0x227A0E0 and the caster slot 0x227B088 (context byte 0), then SetupByMode 0x5B71C0 (both run original): mode 0 = the root
// pool (4 x 0x10, queue 0x22775D8) with RootTask, the aux queue 0x22792C0 (pool 0x22792D0, 100 x 0x24), the shot queue 0x22775E8
// (pool 0x22767C8, 100 x 0x24) with StartTask, flags 0x22792B8 / shot index 0x2276784 / shot count 0x227B324 / impact count
// 0x227B328 / module tick 0x227677C cleared, both particle arrays (0x22775F8 / 0x2278428, 150 x 0x18) free,
// resetSomeCameraData, the TIM upload; mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue: +0x0E = shot
// count % 100 (its shot record 0x227A0E8 + 0x28 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the first
// target's slot):
//   RootTask (0x5B95E0) - packet cursor 0x227B330 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); every tick the aux queue is empty after EndTask finished (flag 2): a
//     RestoreTask in the root queue; ends when both queues are empty and flag 2 is set.
//   RestoreTask (0x5B9690, root queue) - Battle_RestoreDefaultEffectTexture (0x508630) on its counter 1; ends when it is done.
//   StartTask (0x5B91A0, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xD12EC0, out block 0x2278408), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x2276778 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5B7320, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his chain
//     (+0x18) and flag 4; the muzzle (bone 2 of his weapon model, entity +0x78, through 0x5B77E0 / 0x5B7860 = the Renzokuken
//     finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000) to the
//     direction; its own +0x10 = the target's effect anchor (GetDefaultEffectPosition 0x571400), +0x16 = the target's size *
//     2000 / 4096, +0x1C = the target's size clamped to 0x600..0x1100 (0x5B77B0) * 2000 / 4096 (never read); SmokeTask (size
//     0x400..0xAFF), Flash2Task (roll, scale = step = (0x800..0xDFF) / 3), Smoke2Task (size 0x400..0xAFF), GroundSmokeTask
//     (that size * 7 / 4) and BlastTask (scale = step = (0x780..0xA7F) / 3); counter 1: an ImpactTask at its anchor (the next
//     impact number, size 0x700..0x10FF), a TargetSparkTask (the next impact number, the impact's +0x16, the target slot) and a
//     ModelTask at the target's anchor (+0x16 as the impact's, scale = step = (0x1600..0x1BFF) * (+0x16 * 4096 / 1000) / 4096
//     / 4), the action result (ApplyActionResultToTargets 0x506BA0), the gun sound 0xD155B0 (volume 1 / 2 alternating);
//     counter 8: done, flag 4 cleared; ends at 10.
//   SmokeTask (0x5B78E0) / Smoke2Task (0x5B7AF0) - sequence 0xD14430 / 0xD1450C (frame = counter) at the shot record, camera
//     facing (TransformCameraByShadowRotation 0x571BC0); ends with the sequence.
//   Flash2Task (0x5B7980) - prim model 0xD14D18 at the shot record (its rotation x roll), uniform scale; scale + step, step -
//     step / 3; faded (counter - 1) * 0x555 from counter 1; 4 ticks.
//   BlastTask (0x5B7B90) - prim model 0xD15108 at the shot record (its rotation), uniform scale; scale + step, step - step / 4;
//     faded (counter - 1) * 0x2AA from counter 1; 7 ticks.
//   GroundSmokeTask (0x5B7CF0) - sequence 0xD145E4 flat on the ground (x 0x400) under the shot record, scaled; ends with it.
//   ImpactTask (0x5B7E00) - counters 0..6: sequence 0xD1450C (frame = counter, +0x24 = 8) at its point, camera facing (depth
//     -(+0x16)); then every particle of its array (impact number parity; the array pointer is kept in its own argument slot,
//     reproduced) with its id (impact number + 1): sequence 0xD149B0 at its frame, then (not on draw-only ticks) frame + 1, dies
//     when its sequence ended, else position + velocity, velocity - velocity / 4; counter 0: 16 new particles (random offset
//     +-160, random direction, speed 0xC8..0x257); ends past counter 3 with no particle left.
//   TargetSparkTask (0x5B8120) - counters 0..2 (not on draw-only ticks): 4 new particles at random effect anchors of the
//     target (GetEffectSpawnPosition 0x502170, random angle, offset +-500, size 0x400..0xCFF); then every particle of its
//     array (impact number parity) with its id (impact number + 1): sequence 0xD14658 at its frame, its point through the
//     camera (light matrix), R = its size, pulled toward the camera by +0x16; then (not on draw-only ticks) frame + 1, dies
//     when its sequence ended; ends when none is left.
//   ModelTask (0x5B8410) - model 0xD14AA8 through the module's prim renderer (0x5B84E0 = Gastric Juice 169's 0x5E0450, with its
//     section renderers 0x5B85D0 / 0x5B8830 / 0x5B8B00 / 0x5B8D60) at its point, camera facing (TransformCameraByShadowRotation
//     with its scale, depth +(+0x16)); scale + step, step - step / 5; faded (counter - 4) << 9 from counter 4; 12 ticks.
//   EndTask (0x5B9070, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick; counter 4: the action result; after 12 ticks: flag 2, done; ends.
//   FadeInTask (0x5B9290) / FadeOutTask (0x5B9130) - screen flash (Effect_SetScreenFlash 0x5713E0, colour 0x204820) 0 -> 0x800 /
//     0x800 -> 0 over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5B92F0) - the weapon's ghosts as Normal Shot's (ghost slots 0x227B090, current bones 0x2279238, colours
//     0xD155C8; BoneSnapshot 0x5B94A0 / BoneRestore 0x5B94E0 / GhostDraw 0x5B9520; the ghost count kept in its own argument slot).
// Every task except RootTask / RestoreTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// Never-written node fields: TargetSparkTask's +0x10..+0x15 (never read).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x2276778..0x227B334 (end tick, module tick, shot index, pools, queues, particle arrays, bind block, saved
//   bones 0x2279238, flags 0x22792B8, aux queue, context, shot records, caster slot, texture file, ghost slots, counts,
//   texture base, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace dark193
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
	inline int32_t &EndTick() { return var<int32_t>(0x2276778); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x227677C); }
	inline uint32_t &Flags() { return var<uint32_t>(0x22792B8); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x227A0E0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x227B088); }
	inline int32_t &ImpactCount() { return var<int32_t>(0x227B328); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x227B32C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x227B330); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x22775D8; }
	inline TaskQueue *QShots() { return (TaskQueue *)0x22775E8; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x22792C0; }
	static const uint32_t PARTS_A = 0x22775F8, PARTS_B = 0x2278428;  // 150 x 0x18 each
	static const int32_t PARTS_N = 150;
	static const uint32_t SHOTS = 0x227A0E8;                         // 100 x 0x28 shot records
	static const uint32_t BIND_OUT = 0x2278408;
	static const uint32_t BONES_NOW = 0x2279238;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x227B090, GHOSTS_END = 0x227B324; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;
	static const uint32_t FADE_COLOUR = 0x204820;                    // the fades' screen-flash colour

	static const uint32_t ORIG_RootTask = 0x5B95E0;
	static const uint32_t ORIG_RestoreTask = 0x5B9690;
	static const uint32_t ORIG_StartTask = 0x5B91A0;
	static const uint32_t ORIG_ShotTask = 0x5B7320;
	static const uint32_t ORIG_EndTask = 0x5B9070;
	static const uint32_t ORIG_FadeOutTask = 0x5B9130;
	static const uint32_t ORIG_FadeInTask = 0x5B9290;
	static const uint32_t ORIG_AfterimageTask = 0x5B92F0;
	static const uint32_t ORIG_SmokeTask = 0x5B78E0;
	static const uint32_t ORIG_Flash2Task = 0x5B7980;
	static const uint32_t ORIG_Smoke2Task = 0x5B7AF0;
	static const uint32_t ORIG_BlastTask = 0x5B7B90;
	static const uint32_t ORIG_GroundSmokeTask = 0x5B7CF0;
	static const uint32_t ORIG_ImpactTask = 0x5B7E00;
	static const uint32_t ORIG_TargetSparkTask = 0x5B8120;
	static const uint32_t ORIG_ModelTask = 0x5B8410;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD12EC0;
	static const uint32_t MODEL_Flash2 = 0xD14D18, MODEL_Blast = 0xD15108, MODEL_Model = 0xD14AA8;
	static const uint32_t SEQ_Smoke = 0xD14430, SEQ_Smoke2 = 0xD1450C, SEQ_GroundSmoke = 0xD145E4, SEQ_Spark = 0xD149B0, SEQ_TargetSpark = 0xD14658;
	static const void *const SOUND_Gun = (const void *)0xD155B0;
	static const int32_t *const MUZZLE_REF = (const int32_t *)0xD155B8; // (0, 0, -0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD155C8;

	// engine functions not in fx_port.h / mag_common.h
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t angle, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, angle, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = m.t
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }             // MAC = LLM * V0 + BK
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }   // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v
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
		int16_t f0E;     // +0x0E shot: shot number; muzzle tasks: their shot record; impact / target sparks: impact number; fades: length
		int16_t pos[4];  // +0x10 shot / impact / model: the target's anchor (+0x16: the target's size * 2000 / 4096)
		int16_t f18;     // +0x18 shot: chain; flash2: roll; fades: level
		int16_t f1A;     // +0x1A shot / target sparks: target slot
		int16_t f1C;     // +0x1C scale / size (shot: the clamped target size, never read)
		int16_t f1E;     // +0x1E flash2 / blast / model: scale step
		uint8_t pad20[4];
	};
	struct ShotRec // 0x227A0E8 + 0x28 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct Particle // 0x22775F8 / 0x2278428, 150 x 0x18
	{
		int32_t id;      // 0 = free, else impact number + 1
		int16_t frame;
		int16_t size;
		int16_t pos[4];  // (sparks: pos[3] = the impact's +0x16)
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x227B090, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Dark Shot nodes");
	static_assert(sizeof(ShotRec) == 0x28 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Dark Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x28 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t number) { return (Particle *)((number & 1) ? PARTS_B : PARTS_A); }
}
}

#ifdef FF8_FX_HELD
#include "mag193_dark_shot_held.h"
#endif

namespace ff8fx
{
namespace dark193
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5B7860 / 0x5B77E0 = Rough Divide's 0x5F6500 / 0x5F6480)
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

	// the target's size clamped to 0x600..0x1100 (0x5B77B0)
	static int32_t ClampedSize(const uint8_t *entity)
	{
		const int32_t s = *(const int16_t *)(entity + 0x26);
		if (s > 0x1100) return 0x1100;
		if (s < 0x600) return 0x600;
		return s;
	}

	// ------------------------------------------------------------------
	// Afterimages (0x5B94A0 / 0x5B94E0 / 0x5B9520)
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
	// Module prim renderer (0x5B84E0 = Gastric Juice 169's 0x5E0450, the section renderers 0x5B85D0 / 0x5B8830 / 0x5B8B00 /
	// 0x5B8D60 = 169's 0x5E0540 / 0x5E07A0 / 0x5E0A70 / 0x5E0CD0, instruction for instruction; header 0x5C bytes on the scratch:
	// +0x00 model, +0x04 vertices, +0x08..+0x0A far colour, +0x0C fade, +0x10 / +0x14 texture page / clut words, +0x18 texture
	// offset, +0x1C flags, +0x20 section cursor, +0x24 NCLIP, +0x2C OTZ, +0x30 GTE FLAG, +0x58 draw mode word)
	// ==================================================================

	// clip bits of one corner: x outside 0..0xA00 -> xbit, y outside 0..0x6C0 -> ybit
	static uint32_t ClipX(uint32_t xy, uint32_t bit) { const int16_t x = S16v(xy, 0); return (x < 0 || x > 0xA00) ? bit : 0; }
	static uint32_t ClipY(uint32_t xy, uint32_t bit) { const int16_t y = S16v(xy, 2); return (y < 0 || y > 0x6C0) ? bit : 0; }

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

	// 0x5B85D0: flat textured triangles (0x14-byte records -> 0x24-byte packets)
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

	// 0x5B8830: flat textured quads (0x18-byte records -> 0x2C-byte packets)
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

	// 0x5B8B00: Gouraud textured triangles (0x1C-byte records -> 0x2C-byte packets)
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

	// 0x5B8D60: Gouraud textured quads (0x24-byte records -> 0x38-byte packets)
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

	// 0x5B84E0: the model's sections (two skipped words, FT3, FT4, two skipped words, GT3, GT4; an empty section is one zero
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
	// prim model header (0x58 bytes on the scratch stack): +0x0C fade, +0x1C flags (0x33 / faded 0xF3)
	static void ModelDraw(uint32_t model, bool faded, int32_t fade)
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
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// a muzzle model at the shot record: its rotation x roll `roll`, uniform scale
	static void MuzzleModel(const Node *n, int16_t roll, uint32_t model, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = roll;
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
	static int32_t Flash2Fade(int32_t c) { return mul32(c, 0x555) - 0x555; }
	static int32_t BlastFade(int32_t c) { return mul32(c, 0x2AA) - 0x2AA; }
	static int32_t ModelFade(int32_t c) { return (int32_t)((uint32_t)(c - 4) << 9); }

	// the module-renderer model at the node's point: camera facing (the shadow rotation scaled by its scale, depth +(+0x16))
	static void TargetModelDraw(const Node *n, bool faded, int32_t fade)
	{
		TransformCameraByShadowRotation(n->pos, n->f1C, n->pos[3]);
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		*(uint32_t *)h = MODEL_Model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (faded)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = PrimRender((uint32_t)h, RenderOT(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	// a smoke at the shot record (camera facing): returns the sequence's +0x28 (< 0: ended)
	static int16_t SmokeDraw(const Node *n, uint32_t seq)
	{
		TransformCameraByShadowRotation(Shot(n->f0E)->pos, n->f1C, -((int32_t)n->f1C >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
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

	// the impact flash (counters 0..6) at the node's point
	static void ImpactFlashDraw(const Node *n, uint8_t *h)
	{
		*(int16_t *)(h + 4) = n->counter;
		*(uint32_t *)h = SEQ_Smoke2;
		*(int16_t *)(h + 0x24) = 8;
		TransformCameraByShadowRotation(n->pos, n->f1C, -(int32_t)n->pos[3]);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void SparkHeader(uint8_t *h)
	{
		*(int16_t *)(h + 0x24) = 8;
		*(uint32_t *)h = SEQ_Spark;
	}

	// one impact particle
	static void SparkDraw(const Particle *e, int16_t depth, uint8_t *h)
	{
		*(int16_t *)(h + 4) = e->frame;
		TransformCameraByShadowRotation(e->pos, e->size, -(int32_t)depth);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void SparkMove(Particle *e)
	{
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 2));
		e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 2));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 2));
	}

	// the target sparks: the light matrix / back colour = the camera (scratch 0x48, +8); a header (0xB4) with frame 0, +0x24 0
	static void TargetSparkBegin(uint8_t *s, uint8_t *h)
	{
		*(int16_t *)(h + 0x24) = 0;
		memcpy(s + 8, &Camera(), 0x20);
		*(uint32_t *)h = SEQ_TargetSpark;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}
	// one target spark: its point through the camera, R = its size, pulled toward the camera by `pull` (the task's +0x16)
	static void TargetSparkDraw(const Particle *e, int32_t pull, uint8_t *s, uint8_t *h)
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

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// SmokeTask (0x5B78E0) / Smoke2Task (0x5B7AF0): end with their sequence
	static uint32_t SmokeStep(Node *n, uint32_t orig, uint32_t seq)
	{
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(orig, n);)
		const int16_t next = SmokeDraw(n, seq);
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}
	static uint32_t __cdecl SmokeTask(TaskNode *n_) { return SmokeStep((Node *)n_, ORIG_SmokeTask, SEQ_Smoke); }
	static uint32_t __cdecl Smoke2Task(TaskNode *n_) { return SmokeStep((Node *)n_, ORIG_Smoke2Task, SEQ_Smoke2); }

	// Flash2Task (0x5B7980): 4 ticks, scale + step, step - step / 3, fade from counter 1
	static uint32_t __cdecl Flash2Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_Flash2Task, n);)
		MuzzleModel(n, n->f18, MODEL_Flash2, n->counter >= 1, Flash2Fade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 3));
		n->counter++;
		return n->counter >= 4 ? TASK_END : 0;
	}

	// BlastTask (0x5B7B90): 7 ticks, no roll, scale + step, step - step / 4, fade from counter 1
	static uint32_t __cdecl BlastTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_BlastTask, n);)
		MuzzleModel(n, 0, MODEL_Blast, n->counter >= 1, BlastFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 7 ? TASK_END : 0;
	}

	// GroundSmokeTask (0x5B7CF0): ends with its sequence
	static uint32_t __cdecl GroundSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundSmokeTask, n);)
		const int16_t next = GroundSmokeDraw(n);
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// ImpactTask (0x5B7E00): the impact flash and its particles (draw THEN update); 16 new particles on counter 0
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		// (the original keeps the array pointer in its own argument slot)
		*(volatile uint32_t *)&n_ = (uint32_t)arr;
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (n->counter < 7) ImpactFlashDraw(n, h);
		SparkHeader(h);
		int32_t alive = 0;
		const int32_t id = n->f0E + 1;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag193_dark_shot_held.inc
			FX_HELD(held_note_spark(e);)
			SparkDraw(e, n->pos[3], h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag193_dark_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			SparkMove(e);
			alive++;
		}
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 0x10; k++)
			{
				int32_t i = 0;
				while (i < PARTS_N && arr[i].id != 0) i++;
				if (i >= PARTS_N) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x480 + 0x100);
				*(uint32_t *)&e->pos[0] = *(const uint32_t *)&n->pos[0];
				*(uint32_t *)&e->pos[2] = *(const uint32_t *)&n->pos[2];
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 0x140 - 0xA0));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 0x140 - 0xA0));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 0x140 - 0xA0));
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				Normalize(v, v);
				const int32_t sp = CrtRand() % 0x190 + 0xC8;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// TargetSparkTask (0x5B8120): new sparks on counters 0..2, then its sparks (draw THEN update); ends when none is left
	static uint32_t __cdecl TargetSparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->f0E + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 2)
		{
			const int32_t anchors = ***(const uint8_t ***)(Entity(n->f1A) + 0x64);
			for (int32_t j = 0; j < 4; j++)
			{
				int32_t i = 0;
				while (i < PARTS_N && arr[i].id != 0) i++;
				if (i >= PARTS_N) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x900 + 0x400);
				const int32_t bone = anchors ? CrtRand() % anchors : 0;
				GetEffectSpawnPosition(Entity(n->f1A), bone, CrtRand() % 0x1000, e->pos);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 1000 - 500));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 1000 - 500));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 1000 - 500));
			}
		}
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_TargetSparkTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		TargetSparkBegin(s, h);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag193_dark_shot_held.inc
			FX_HELD(held_note_spark(e);)
			TargetSparkDraw(e, n->pos[3], s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag193_dark_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			alive++;
		}
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ModelTask (0x5B8410): 12 ticks, scale + step, step - step / 5, faded from counter 4
	static uint32_t __cdecl ModelTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(held_note_node(ORIG_ModelTask, n);)
		TargetModelDraw(n, n->counter >= 4, ModelFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	// ShotTask (0x5B7320): one trigger press
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
		if (n->counter == 8)
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
			BuildAxisAngleRotationMatrix(angle, &s->m, axis);
			uint8_t *t = Entity(n->f1A);
			GetDefaultEffectPosition(t, n->pos);
			n->pos[3] = (int16_t)(mul32(*(const int16_t *)(t + 0x26), 2000) >> 12);
			n->f1C = (int16_t)(mul32(ClampedSize(t), 2000) >> 12);
			Node *k = AddTask(QAux(), ORIG_SmokeTask);
			k->counter = 0;
			k->f0E = n->f0E;
			k->f1C = (int16_t)(CrtRand() % 0x700 + 0x400);
			Node *f = AddTask(QAux(), ORIG_Flash2Task);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third = (int16_t)((CrtRand() % 0x600 + 0x800) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *k2 = AddTask(QAux(), ORIG_Smoke2Task);
			k2->counter = 0;
			k2->f0E = n->f0E;
			k2->f1C = (int16_t)(CrtRand() % 0x700 + 0x400);
			Node *d = AddTask(QAux(), ORIG_GroundSmokeTask);
			d->f0E = n->f0E;
			d->counter = 0;
			d->f1C = (int16_t)(mul32((int32_t)k2->f1C * 7, 0x400) >> 12);
			Node *b = AddTask(QAux(), ORIG_BlastTask);
			b->counter = 0;
			b->f0E = n->f0E;
			const int16_t third2 = (int16_t)((CrtRand() % 0x300 + 0x780) / 3);
			b->f1E = third2;
			b->f1C = third2;
		}
		if (n->counter == 1)
		{
			Node *im = AddTask(QAux(), ORIG_ImpactTask);
			const int32_t num = ImpactCount();
			im->f0E = (int16_t)num;
			ImpactCount() = num + 1;
			memcpy(im->pos, n->pos, 8);
			im->counter = 0;
			im->f1C = (int16_t)(CrtRand() % 0xA00 + 0x700);
			Node *sp = AddTask(QAux(), ORIG_TargetSparkTask);
			const int32_t num2 = ImpactCount();
			sp->f0E = (int16_t)num2;
			ImpactCount() = num2 + 1;
			sp->counter = 0;
			sp->pos[3] = im->pos[3];
			sp->f1A = n->f1A;
			Node *m = AddTask(QAux(), ORIG_ModelTask);
			m->counter = 0;
			GetDefaultEffectPosition(Entity(n->f1A), m->pos);
			const int16_t depth = im->pos[3];
			const int32_t k = ((int32_t)depth << 12) / 1000;
			m->pos[3] = depth;
			const int16_t quarter = (int16_t)((mul32(CrtRand() % 0x600 + 0x1600, k) >> 12) / 4);
			m->f1E = quarter;
			m->f1C = quarter;
			ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
			BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		}
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// FadeOutTask (0x5B9130): level - level / length * counter, 0 at the end (colour 0x204820)
	static uint32_t __cdecl FadeOutTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 - n->f18 / n->f0E * n->counter, FADE_COLOUR);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		if (n->counter < n->f0E) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// FadeInTask (0x5B9290): level / length * counter (colour 0x204820)
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, FADE_COLOUR);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5B92F0): the weapon's ghosts
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
		// 30 fps layer: see mag193_dark_shot_held.inc
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

	// StartTask (0x5B91A0): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5B9070, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 12 ticks
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

	// RestoreTask (0x5B9690, root queue): the default effect texture back on its counter 1; ends when done
	static uint32_t __cdecl RestoreTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		if (r->counter == 1) TextureRestoreTask((void *)TexBase(), &r->done);
		const int16_t done = r->done;
		r->counter++;
		return done ? TASK_END : 0;
	}

	// RootTask (0x5B95E0): packet arena parity, the shot queue, the aux queue, the module tick, the texture restore
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag193_dark_shot_held.inc
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

	void register_mag193_dark_shot()
	{
		register_port(dark193::ORIG_RootTask, (void *)dark193::RootTask, "193 RootTask", 193);
		register_port(dark193::ORIG_RestoreTask, (void *)dark193::RestoreTask, "193 RestoreTask", 193);
		register_port(dark193::ORIG_StartTask, (void *)dark193::StartTask, "193 StartTask", 193);
		register_port(dark193::ORIG_ShotTask, (void *)dark193::ShotTask, "193 ShotTask", 193);
		register_port(dark193::ORIG_EndTask, (void *)dark193::EndTask, "193 EndTask", 193);
		register_port(dark193::ORIG_FadeOutTask, (void *)dark193::FadeOutTask, "193 FadeOutTask", 193);
		register_port(dark193::ORIG_FadeInTask, (void *)dark193::FadeInTask, "193 FadeInTask", 193);
		register_port(dark193::ORIG_AfterimageTask, (void *)dark193::AfterimageTask, "193 AfterimageTask", 193);
		register_port(dark193::ORIG_SmokeTask, (void *)dark193::SmokeTask, "193 SmokeTask", 193);
		register_port(dark193::ORIG_Flash2Task, (void *)dark193::Flash2Task, "193 Flash2Task", 193);
		register_port(dark193::ORIG_Smoke2Task, (void *)dark193::Smoke2Task, "193 Smoke2Task", 193);
		register_port(dark193::ORIG_BlastTask, (void *)dark193::BlastTask, "193 BlastTask", 193);
		register_port(dark193::ORIG_GroundSmokeTask, (void *)dark193::GroundSmokeTask, "193 GroundSmokeTask", 193);
		register_port(dark193::ORIG_ImpactTask, (void *)dark193::ImpactTask, "193 ImpactTask", 193);
		register_port(dark193::ORIG_TargetSparkTask, (void *)dark193::TargetSparkTask, "193 TargetSparkTask", 193);
		register_port(dark193::ORIG_ModelTask, (void *)dark193::ModelTask, "193 ModelTask", 193);
		// 30 fps layer: see mag193_dark_shot_held.inc
		FX_HELD(register_mag193_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag193_dark_shot_held.inc"
#endif
