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

// Effect 192: Scatter Shot (Irvine's Shot limit break, kernel.bin section 21 entry 1 +4; MAG_192_SHOT_SCATTER_SHOT).
//
// How the battle drives it (as Normal Shot 188, see mag188_normal_shot.cpp): the opening action is a normal magic cast (mode 0);
// every trigger press of the Shot window becomes an action that BattleActionSequence_Tick_Shot 0x50BEE0 runs: it calls the
// module's _Init again with g_MagicCastContext 0x1D99A78 +8 = 2 (a shot) or 1 (the closing action) and waits for +9 done = 1.
//
// Structure (file loader 0x5B96D0 = the texture file 0xD18694; _Init 0x5B96F0 keeps the texture base 0x2281B0C, the context
// 0x22808C0 and the caster slot 0x2281868 (context byte 0), then SetupByMode 0x5B9720 (both run original): mode 0 = the root
// pool (4 x 0x10, queue 0x227C198) with RootTask, the shot queue 0x227C1A8 (pool 0x227B388, 100 x 0x24) with StartTask, the aux
// queue 0x227FAA0 (pool 0x227FAB0, 100 x 0x24), flags 0x227FA98 / shot index 0x227B344 / shot count 0x2281B04 / impact count
// 0x2281B08 / module tick 0x227B33C cleared, both particle arrays (0x227C1B8 / 0x227DDF8, 300 x 0x18) free,
// resetSomeCameraData, the TIM upload; mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue: +0x0E = shot
// count % 100 (its shot record 0x22808C8 + 0x28 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the first
// target's slot):
//   RootTask (0x5BB1B0) - packet cursor 0x2281B10 = the texture base (+ 0x10000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); every tick the aux queue is empty after EndTask finished (flag 2): a
//     RestoreTask in the root queue; ends when both queues are empty and flag 2 is set.
//   RestoreTask (0x5BB260, root queue) - Battle_RestoreDefaultEffectTexture (0x508630) on its counter 1; ends when it is done.
//   StartTask (0x5BAD80, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xD155E8, out block 0x227DDD8), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x227B338 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5B9880, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his chain
//     (+0x18) and flag 4; the muzzle (bone 2 of his weapon model, entity +0x78, through 0x5B9DF0 / 0x5B9E70 = the Renzokuken
//     finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000) to the
//     direction; FlashTask (random roll, scale 0x900..0xDFF), Flash2Task (roll, scale = step = (0x800..0xDFF) / 3), SmokeTask
//     (size 0x400..0xAFF), GroundSmokeTask (that size * 13 / 8) and BlastTask (scale = step = (0xA00..0xCFF) / 3); counter 1:
//     for EVERY target of the action: an ImpactTask at a random effect anchor of it (GetEffectSpawnPosition 0x502170, random
//     angle; size 0x800..0x13FF, its delay = size & 1; +0x16 = the target's size * 2000 / 4096), then on a random odd roll a
//     second ImpactTask at another random anchor (same +0x16), else a MissTask beside it (GetDefaultEffectPosition 0x571400
//     with x / z offset by +-(+0x16), y = 0, delay = x & 1); every new node takes the next impact number; then the action
//     result (ApplyActionResultToTargets 0x506BA0), the gun sound 0xD18668 (volume 1 / 2 alternating); counter 8: done, flag 4
//     cleared; ends at 10. The anchor count / bone index and the offset range are kept in its own argument slot (reproduced).
//   FlashTask (0x5B9EF0) - prim model 0xD17220 at the shot record (its rotation x roll), uniform scale; counter 1 faded 0xC00;
//     2 ticks.  Flash2Task (0x5BA030) - prim model 0xD17768, scale + step, step - step / 3, fade (counter - 1) * 0x555 from
//     counter 1; 4 ticks.  BlastTask (0x5BA240) - prim model 0xD17DC0 at the shot record (its rotation, no roll), scale + step,
//     step - step / 4, fade (counter - 1) * 0x2AA from counter 1; 7 ticks.  SmokeTask (0x5BA1A0) - sprite sequence 0xD16B58
//     (frame = counter) at the shot record, camera facing (TransformCameraByShadowRotation 0x571BC0); ends with the sequence.
//     GroundSmokeTask (0x5BA3A0) - sequence 0xD16C30 flat on the ground (x 0x400) under the shot record, scaled; ends with it.
//   ImpactTask (0x5BA4B0) - while its delay (+0x18) is > 0: nothing drawn, the delay - 1; then the sequence 0xD16CA4 (frame =
//     counter, counters 0..15) at its point, camera facing; then every particle of its array (impact number parity; the array
//     pointer is kept in its own argument slot, reproduced) with its id (impact number + 1): sequence 0xD17074 at its frame,
//     then (not on draw-only ticks) frame + 1, dies when its sequence ended, else position + velocity, velocity - velocity / 4;
//     counter 0: 10 new particles (random offset +-150, random direction, speed 0xB4..0x243); ends past counter 3 with no
//     particle left.
//   MissTask (0x5BA800) - while its delay is > 0: the delay - 1; then a MissRingTask (its point, random yaw, scale = step =
//     (0x700..0xCFF) / 4) and a MissDustTask (its point and impact number) in the aux queue; ends at once. (The ring node's
//     +0x0E is not written: not read.)
//   MissRingTask (0x5BA8C0) - prim model 0xD17B58 at its point (yaw), uniform scale; scale + step, step - step / 5; faded
//     (counter - 4) << 10 from counter 4; 8 ticks.
//   MissDustTask (0x5BAA00) - every particle of its array (impact number parity) with id = the impact number (no + 1): sequence
//     0xD16EC8 (header +0x1C = 0x404040) at its frame, camera facing with its size (depth size / 16); then (not on draw-only
//     ticks) frame + 1, dies when its sequence ended, else size + 0x20, x / z + velocity, velocity - velocity / 8; counter 0: 10
//     new particles at its point (size 0x800..0xDFF) - the random x / z offset (radius 50..99) and the velocity (20..49) along
//     a random angle are written into the SAME index of the ODD array 0x227DDF8 whichever the dust's array is (vanilla quirk,
//     reproduced: the even dust's particles keep the stale velocity of their entries); ends past counter 3 with no particle left.
//   EndTask (0x5BAC60, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick; counter 4: the action result; after 12 ticks: flag 2, done; ends.
//   FadeInTask (0x5BAE70) / FadeOutTask (0x5BAD20) - screen flash (Effect_SetScreenFlash 0x5713E0) 0 -> 0x800 / 0x800 -> 0
//     over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5BAEC0) - every tick: ghost slot counter % 5 (0x2281870, 5 x 0x84: used + the weapon's bone matrices)
//     = a snapshot of his weapon's bone matrices (0x5BB070); his current ones kept in 0x227FA18; then up to 5 ghosts (fewer
//     once the end tick is reached), newest snapshot first, each drawn with its snapshot put into the weapon's bones (0x5BB0B0)
//     and his entity colour +0x28 = 0xD18680[k] through GhostDraw (0x5BB0F0: the weapon model alone under his world matrix,
//     RenderGeometry into the frame arena 0x1D8E054); his bones and +0x28 put back; ends 5 ticks after the end tick. (The ghost
//     count is kept in its own argument slot, reproduced.)
// Every task except RootTask / RestoreTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x227B338..0x2281B14 (end tick, module tick, shot index, pools, queues, particle arrays, bind block, saved
//   bones 0x227FA18, flags 0x227FA98, aux queue, context, shot records, caster slot, texture file, ghost slots, counts,
//   texture base, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace scatter192
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x227B338); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x227B33C); }
	inline uint32_t &Flags() { return var<uint32_t>(0x227FA98); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x22808C0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x2281868); }
	inline int32_t &ImpactCount() { return var<int32_t>(0x2281B08); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2281B0C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2281B10); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x227C198; }
	inline TaskQueue *QShots() { return (TaskQueue *)0x227C1A8; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x227FAA0; }
	static const uint32_t PARTS_A = 0x227C1B8, PARTS_B = 0x227DDF8;  // 300 x 0x18 each
	static const int32_t PARTS_N = 300;
	static const uint32_t SHOTS = 0x22808C8;                         // 100 x 0x28 shot records
	static const uint32_t BIND_OUT = 0x227DDD8;
	static const uint32_t BONES_NOW = 0x227FA18;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x2281870, GHOSTS_END = 0x2281B04; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0x10000;

	static const uint32_t ORIG_RootTask = 0x5BB1B0;
	static const uint32_t ORIG_RestoreTask = 0x5BB260;
	static const uint32_t ORIG_StartTask = 0x5BAD80;
	static const uint32_t ORIG_ShotTask = 0x5B9880;
	static const uint32_t ORIG_EndTask = 0x5BAC60;
	static const uint32_t ORIG_FadeOutTask = 0x5BAD20;
	static const uint32_t ORIG_FadeInTask = 0x5BAE70;
	static const uint32_t ORIG_AfterimageTask = 0x5BAEC0;
	static const uint32_t ORIG_FlashTask = 0x5B9EF0;
	static const uint32_t ORIG_Flash2Task = 0x5BA030;
	static const uint32_t ORIG_SmokeTask = 0x5BA1A0;
	static const uint32_t ORIG_BlastTask = 0x5BA240;
	static const uint32_t ORIG_GroundSmokeTask = 0x5BA3A0;
	static const uint32_t ORIG_ImpactTask = 0x5BA4B0;
	static const uint32_t ORIG_MissTask = 0x5BA800;
	static const uint32_t ORIG_MissRingTask = 0x5BA8C0;
	static const uint32_t ORIG_MissDustTask = 0x5BAA00;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD155E8;
	static const uint32_t MODEL_Flash = 0xD17220, MODEL_Flash2 = 0xD17768, MODEL_Blast = 0xD17DC0, MODEL_Ring = 0xD17B58;
	static const uint32_t SEQ_Smoke = 0xD16B58, SEQ_GroundSmoke = 0xD16C30, SEQ_Impact = 0xD16CA4, SEQ_Spark = 0xD17074, SEQ_Dust = 0xD16EC8;
	static const void *const SOUND_Gun = (const void *)0xD18668;
	static const int32_t *const MUZZLE_REF = (const int32_t *)0xD18670; // (0, 0, -0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD18680;

	// engine functions not in fx_port.h / mag_common.h
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t angle, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, angle, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
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
		int16_t f0E;     // +0x0E shot: shot number; muzzle tasks: their shot record; impact / miss / dust: impact number; fades: length
		int16_t pos[4];  // +0x10 impact / miss / ring / dust: its point (+0x16: the target's size * 2000 / 4096)
		int16_t f18;     // +0x18 shot: chain; flash: roll; ring: yaw; impact / miss: delay; fades: level
		int16_t f1A;     // +0x1A shot: target slot
		int16_t f1C;     // +0x1C scale / size
		int16_t f1E;     // +0x1E Flash2 / Blast / ring: scale step
		uint8_t pad20[4];
	};
	struct ShotRec // 0x22808C8 + 0x28 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct Particle // 0x227C1B8 / 0x227DDF8, 300 x 0x18
	{
		int32_t id;      // 0 = free, else impact number + 1 (sparks) / impact number (dust)
		int16_t frame;
		int16_t size;    // TransformCameraByShadowRotation size
		int16_t pos[4];  // (pos[3] = the node's +0x16)
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x2281870, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Scatter Shot nodes");
	static_assert(sizeof(ShotRec) == 0x28 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Scatter Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x28 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t number) { return (Particle *)((number & 1) ? PARTS_B : PARTS_A); }
	inline Particle *PartsB() { return (Particle *)PARTS_B; }
}
}

#ifdef FF8_FX_HELD
#include "mag192_scatter_shot_held.h"
#endif

namespace ff8fx
{
namespace scatter192
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5B9E70 / 0x5B9DF0 = Rough Divide's 0x5F6500 / 0x5F6480)
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

	// ------------------------------------------------------------------
	// Afterimages (0x5BB070 / 0x5BB0B0 / 0x5BB0F0)
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
	static int32_t RingFade(int32_t c) { return (int32_t)((uint32_t)(c - 4) << 10); }

	// the miss ring at its point: yaw, uniform scale
	static void RingDraw(const Node *n, bool faded, int32_t fade)
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
		ModelDraw(MODEL_Ring, faded, fade);
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

	// the impact flash (counters 0..15) at the node's point
	static void ImpactFlashDraw(const Node *n, uint8_t *h)
	{
		*(int16_t *)(h + 4) = n->counter;
		*(uint32_t *)h = SEQ_Impact;
		*(int16_t *)(h + 0x24) = 8;
		TransformCameraByShadowRotation(n->pos, n->f1C, -(int32_t)n->pos[3]);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
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

	// one miss dust particle (camera facing with its size, depth size / 16)
	static void DustDraw(const Particle *e, uint8_t *h)
	{
		*(int16_t *)(h + 4) = e->frame;
		TransformCameraByShadowRotation(e->pos, e->size, -((int32_t)e->size >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}
	static void DustMove(Particle *e)
	{
		e->size = (int16_t)(e->size + 0x20);
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 3));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 3));
	}
	static void DustHeader(uint8_t *h)
	{
		*(uint32_t *)h = SEQ_Dust;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// FlashTask (0x5B9EF0): 2 ticks, faded 0xC00 on counter 1
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_FlashTask, n);)
		MuzzleModel(n, n->f18, MODEL_Flash, n->counter == 1, 0xC00);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// Flash2Task (0x5BA030): 4 ticks, scale + step, step - step / 3, fade from counter 1
	static uint32_t __cdecl Flash2Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_Flash2Task, n);)
		MuzzleModel(n, n->f18, MODEL_Flash2, n->counter >= 1, Flash2Fade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 3));
		n->counter++;
		return n->counter >= 4 ? TASK_END : 0;
	}

	// BlastTask (0x5BA240): 7 ticks, no roll, scale + step, step - step / 4, fade from counter 1
	static uint32_t __cdecl BlastTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_BlastTask, n);)
		MuzzleModel(n, 0, MODEL_Blast, n->counter >= 1, BlastFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 7 ? TASK_END : 0;
	}

	// SmokeTask (0x5BA1A0): ends with its sequence
	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_SmokeTask, n);)
		const int16_t next = SmokeDraw(n);
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// GroundSmokeTask (0x5BA3A0): ends with its sequence
	static uint32_t __cdecl GroundSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundSmokeTask, n);)
		const int16_t next = GroundSmokeDraw(n);
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// ImpactTask (0x5BA4B0): its delay, then the impact flash and its particles (draw THEN update)
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->f18 > 0)
		{
			if (!(UpdateFlags() & 0x201)) n->f18--;
			return 0;
		}
		Particle *arr = PartArray(n->f0E);
		// (the original keeps the array pointer in its own argument slot)
		*(volatile uint32_t *)&n_ = (uint32_t)arr;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (n->counter < 0x10) ImpactFlashDraw(n, h);
		*(int16_t *)(h + 0x24) = 8;
		*(uint32_t *)h = SEQ_Spark;
		int32_t alive = 0;
		const int32_t id = n->f0E + 1;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag192_scatter_shot_held.inc
			FX_HELD(held_note_spark(e);)
			SparkDraw(e, n->pos[3], h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag192_scatter_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			SparkMove(e);
			alive++;
		}
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 10; k++)
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
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 300 - 0x96));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 300 - 0x96));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 300 - 0x96));
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				Normalize(v, v);
				const int32_t sp = CrtRand() % 400 + 0xB4;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// MissTask (0x5BA800): its delay, then the miss ring and the miss dust; ends at once
	static uint32_t __cdecl MissTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->f18 > 0)
		{
			if (UpdateFlags() & 0x201) return 0;
			n->f18--;
			return 0;
		}
		Node *r = AddTask(QAux(), ORIG_MissRingTask);
		*(uint32_t *)&r->pos[0] = *(const uint32_t *)&n->pos[0];
		r->counter = 0;
		*(uint32_t *)&r->pos[2] = *(const uint32_t *)&n->pos[2];
		r->f18 = (int16_t)(CrtRand() % 0x1000);
		const int16_t quarter = (int16_t)((CrtRand() % 0x600 + 0x700) / 4);
		r->f1E = quarter;
		r->f1C = quarter;
		Node *d = AddTask(QAux(), ORIG_MissDustTask);
		d->f0E = n->f0E;
		*(uint32_t *)&d->pos[0] = *(const uint32_t *)&n->pos[0];
		d->counter = 0;
		*(uint32_t *)&d->pos[2] = *(const uint32_t *)&n->pos[2];
		return TASK_END;
	}

	// MissRingTask (0x5BA8C0): 8 ticks, scale + step, step - step / 5, faded from counter 4
	static uint32_t __cdecl MissRingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_MissRingTask, n);)
		RingDraw(n, n->counter >= 4, RingFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 8 ? TASK_END : 0;
	}

	// MissDustTask (0x5BAA00): its particles (draw THEN update)
	static uint32_t __cdecl MissDustTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_node(ORIG_MissDustTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		DustHeader(h);
		int32_t alive = 0;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != (int32_t)n->f0E) continue;
			// 30 fps layer: see mag192_scatter_shot_held.inc
			FX_HELD(held_note_spark(e);)
			DustDraw(e, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag192_scatter_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			DustMove(e);
			alive++;
		}
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 10; k++)
			{
				int32_t i = 0;
				while (i < PARTS_N && arr[i].id != 0) i++;
				if (i >= PARTS_N) break;
				Particle *e = &arr[i];
				e->id = n->f0E;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x600 + 0x800);
				*(uint32_t *)&e->pos[0] = *(const uint32_t *)&n->pos[0];
				*(uint32_t *)&e->pos[2] = *(const uint32_t *)&n->pos[2];
				const int32_t a = CrtRand() % 0x1000;
				const int32_t c = ComputeCos(a);
				const int32_t s = ComputeSin(a);
				// (the offset and the velocity go to the same index of the ODD array, whichever this dust's array is)
				Particle *b = &PartsB()[i];
				const int32_t r = CrtRand() % 50 + 50;
				b->pos[0] = (int16_t)(b->pos[0] + (int16_t)(mul32(r, c) >> 12));
				b->pos[2] = (int16_t)(b->pos[2] + (int16_t)(mul32(r, s) >> 12));
				const int32_t sp = CrtRand() % 30 + 20;
				b->vel[0] = (int16_t)(mul32(sp, c) >> 12);
				b->vel[2] = (int16_t)(mul32(sp, s) >> 12);
			}
		}
		n->counter++;
		return (n->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	// ShotTask (0x5B9880): one trigger press
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
			Node *f = AddTask(QAux(), ORIG_FlashTask);
			f->counter = 0;
			f->f0E = n->f0E;
			f->f18 = (int16_t)(CrtRand() % 0x1000);
			f->f1C = (int16_t)(CrtRand() % 0x500 + 0x900);
			Node *g = AddTask(QAux(), ORIG_Flash2Task);
			g->counter = 0;
			g->f0E = n->f0E;
			g->f18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t third = (int16_t)((CrtRand() % 0x600 + 0x800) / 3);
			g->f1E = third;
			g->f1C = third;
			Node *k = AddTask(QAux(), ORIG_SmokeTask);
			k->counter = 0;
			k->f0E = n->f0E;
			k->f1C = (int16_t)(CrtRand() % 0x700 + 0x400);
			Node *d = AddTask(QAux(), ORIG_GroundSmokeTask);
			d->f0E = n->f0E;
			d->counter = 0;
			d->f1C = (int16_t)(mul32((int32_t)k->f1C * 13, 0x200) >> 12);
			Node *b = AddTask(QAux(), ORIG_BlastTask);
			b->counter = 0;
			b->f0E = n->f0E;
			const int16_t bthird = (int16_t)((CrtRand() % 0x300 + 0xA00) / 3);
			b->f1E = bthird;
			b->f1C = bthird;
		}
		if (n->counter == 1)
		{
			for (int32_t i = 0; i < (int32_t)Ctx()->actions->target_count; i++)
			{
				const int32_t slot = Ctx()->actions->targets[i * TARGET_STRIDE];
				uint8_t *t = Entity(slot);
				// (the original keeps the anchor count / bone index in its own argument slot)
				int32_t bone = ***(const uint8_t ***)(t + 0x64);
				*(volatile int32_t *)&n_ = bone;
				if (bone)
				{
					bone = CrtRand() % bone;
					*(volatile int32_t *)&n_ = bone;
				}
				Node *im = AddTask(QAux(), ORIG_ImpactTask);
				im->f0E = (int16_t)ImpactCount();
				ImpactCount()++;
				im->counter = 0;
				GetEffectSpawnPosition(t, bone, CrtRand() % 0x1000, im->pos);
				im->pos[3] = (int16_t)(mul32(*(const int16_t *)(t + 0x26), 2000) >> 12);
				const int32_t size = CrtRand() % 0xC00 + 0x800;
				im->f1C = (int16_t)size;
				im->f18 = (int16_t)(size & 1);
				if (CrtRand() & 1)
				{
					// a second pellet on the target: another random anchor
					int32_t bone2 = ***(const uint8_t ***)(t + 0x64);
					*(volatile int32_t *)&n_ = bone2;
					if (bone2)
					{
						bone2 = CrtRand() % bone2;
						*(volatile int32_t *)&n_ = bone2;
					}
					Node *i2 = AddTask(QAux(), ORIG_ImpactTask);
					i2->f0E = (int16_t)ImpactCount();
					i2->counter = 0;
					ImpactCount()++;
					GetEffectSpawnPosition(t, bone2, CrtRand() % 0x1000, i2->pos);
					i2->pos[3] = im->pos[3];
					const int32_t size2 = CrtRand() % 0xC00 + 0x800;
					i2->f1C = (int16_t)size2;
					i2->f18 = (int16_t)(size2 & 1);
				}
				else
				{
					// a pellet that misses: on the ground beside the target
					Node *m = AddTask(QAux(), ORIG_MissTask);
					m->counter = 0;
					m->f0E = (int16_t)ImpactCount();
					ImpactCount()++;
					GetDefaultEffectPosition(t, m->pos);
					const int16_t s = im->pos[3];
					m->pos[3] = s;
					*(volatile int16_t *)&n_ = s; // (low word of the argument slot)
					const int32_t dx = CrtRand() % (2 * (int32_t)s) - s;
					m->pos[1] = 0;
					m->pos[0] = (int16_t)(m->pos[0] + (int16_t)dx);
					const int32_t dz = CrtRand() % (2 * (int32_t)s) - s;
					m->pos[2] = (int16_t)(m->pos[2] + (int16_t)dz);
					m->f18 = (int16_t)(*(const uint8_t *)&m->pos[0] & 1);
				}
			}
			ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
			BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		}
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// FadeOutTask (0x5BAD20): level - level / length * counter, 0 at the end
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

	// FadeInTask (0x5BAE70): level / length * counter
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5BAEC0): the weapon's ghosts
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
		// 30 fps layer: see mag192_scatter_shot_held.inc
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

	// StartTask (0x5BAD80): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5BAC60, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 12 ticks
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

	// RestoreTask (0x5BB260, root queue): the default effect texture back on its counter 1; ends when done
	static uint32_t __cdecl RestoreTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		if (r->counter == 1) TextureRestoreTask((void *)TexBase(), &r->done);
		const int16_t done = r->done;
		r->counter++;
		return done ? TASK_END : 0;
	}

	// RootTask (0x5BB1B0): packet arena parity, the shot queue, the aux queue, the module tick, the texture restore
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag192_scatter_shot_held.inc
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

	void register_mag192_scatter_shot()
	{
		register_port(scatter192::ORIG_RootTask, (void *)scatter192::RootTask, "192 RootTask", 192);
		register_port(scatter192::ORIG_RestoreTask, (void *)scatter192::RestoreTask, "192 RestoreTask", 192);
		register_port(scatter192::ORIG_StartTask, (void *)scatter192::StartTask, "192 StartTask", 192);
		register_port(scatter192::ORIG_ShotTask, (void *)scatter192::ShotTask, "192 ShotTask", 192);
		register_port(scatter192::ORIG_EndTask, (void *)scatter192::EndTask, "192 EndTask", 192);
		register_port(scatter192::ORIG_FadeOutTask, (void *)scatter192::FadeOutTask, "192 FadeOutTask", 192);
		register_port(scatter192::ORIG_FadeInTask, (void *)scatter192::FadeInTask, "192 FadeInTask", 192);
		register_port(scatter192::ORIG_AfterimageTask, (void *)scatter192::AfterimageTask, "192 AfterimageTask", 192);
		register_port(scatter192::ORIG_FlashTask, (void *)scatter192::FlashTask, "192 FlashTask", 192);
		register_port(scatter192::ORIG_Flash2Task, (void *)scatter192::Flash2Task, "192 Flash2Task", 192);
		register_port(scatter192::ORIG_BlastTask, (void *)scatter192::BlastTask, "192 BlastTask", 192);
		register_port(scatter192::ORIG_SmokeTask, (void *)scatter192::SmokeTask, "192 SmokeTask", 192);
		register_port(scatter192::ORIG_GroundSmokeTask, (void *)scatter192::GroundSmokeTask, "192 GroundSmokeTask", 192);
		register_port(scatter192::ORIG_ImpactTask, (void *)scatter192::ImpactTask, "192 ImpactTask", 192);
		register_port(scatter192::ORIG_MissTask, (void *)scatter192::MissTask, "192 MissTask", 192);
		register_port(scatter192::ORIG_MissRingTask, (void *)scatter192::MissRingTask, "192 MissRingTask", 192);
		register_port(scatter192::ORIG_MissDustTask, (void *)scatter192::MissDustTask, "192 MissDustTask", 192);
		// 30 fps layer: see mag192_scatter_shot_held.inc
		FX_HELD(register_mag192_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag192_scatter_shot_held.inc"
#endif
