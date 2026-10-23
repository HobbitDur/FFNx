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

// Effect 188: Normal Shot (Irvine's Shot limit break, kernel.bin section 21 entry 0 +4; MAG_188_SHOT_NORMAL_SHOT).
//
// How the battle drives it: the opening action is a normal magic cast (BattleActionSequence_Tick_MagicCast 0x50A9A0 sets
// g_MagicCastContext 0x1D99A78 +8 sequence_mode = 0 for the limit's action types); every trigger press of the Shot window (UI
// code: BattleMenu_ShotWindow_Update 0x4AD8D0, the latch 0x1D76762) becomes an action that BattleActionSequence_Tick_Shot
// 0x50BEE0 runs: it calls the module's _Init again with +8 = 2 (a shot, action +1 == 0xED) or 1 (the closing action) and waits
// for +9 done = 1. The Shot window's timer is UI code: no effect module drives it.
//
// Structure (file loader 0x5BE320 = the texture file 0xD1DA54; _Init 0x5BE340 keeps the texture base 0x22B75EC, the context
// 0x22B63A0 and the caster slot 0x22B7348 (context byte 0), then SetupByMode 0x5BE370 (both run original): mode 0 = the
// root pool (4 x 0x10, queue 0x22B3898) with RootTask, the shot queue 0x22B38A8 (100 x 0x24) with StartTask, the aux queue
// 0x22B5580 (100 x 0x24), flags 0x22B5578 / shot index 0x22B2A44 / shot count 0x22B75E4 / impact count 0x22B75E8 / module tick
// 0x22B2A3C cleared, both particle arrays (0x22B38B8 / 0x22B46E8, 150 x 0x18) free, resetSomeCameraData, the TIM upload;
// mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue: +0x0E = shot count % 100 (its shot record
// 0x22B63A8 + 0x28 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the first target's slot):
//   RootTask (0x5BF650) - packet cursor 0x22B75F0 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); ends when both are empty and EndTask finished (flag 2).
//   StartTask (0x5BF220, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xD1B4B8, out block 0x22B46C8), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x22B2A38 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5BE4D0, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his
//     chain (+0x18) and flag 4; the muzzle (bone 2 of his WEAPON model, entity +0x78, through 0x5BE880 / 0x5BE900 = the
//     Renzokuken finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000)
//     to the direction; FlashTask (random roll, scale 0x900..0xDFF), Flash2Task (roll, scale = step = (0x800..0xDFF) / 3),
//     SmokeTask (size 0x400..0xAFF) and GroundSmokeTask (that size * 9 / 8); counter 1: ImpactTask at a random effect anchor of
//     the target (GetEffectSpawnPosition 0x502170, random angle; size 0x800..0xBFF; +0x16 = target size * 2000 / 4096), the
//     action result (ApplyActionResultToTargets 0x506BA0), the gun sound 0xD1DA28 (volume 1 / 2 alternating); counter 8: done,
//     flag 4 cleared (the next shot may start); ends at 10.
//   FlashTask (0x5BE980) - prim model 0xD1D0F0 at the shot record (its rotation x roll), uniform scale; counter 1 faded 0xC00;
//     2 ticks.  Flash2Task (0x5BEAC0) - prim model 0xD1D638, scale + step, step - step / 3, fade (counter - 1) * 0x555 from
//     counter 1; 4 ticks.  SmokeTask (0x5BEC30) - sprite sequence 0xD1CA28 (frame = counter) at the shot record, camera
//     facing (TransformCameraByShadowRotation 0x571BC0); ends with the sequence.  GroundSmokeTask (0x5BECD0) - sequence
//     0xD1CB00 flat on the ground (x 0x400) under the shot record, scaled; ends with the sequence.
//   ImpactTask (0x5BEDE0) - sequence 0xD1CB74 (frame = counter, counters 0..15) at its point, camera facing; then every
//     particle of its array (impact number parity) with its id (impact number + 1): sequence 0xD1CF44 at its frame, then
//     (not on draw-only ticks) frame + 1, dies when its sequence ended, else position + velocity, velocity - velocity / 4;
//     counter 0: 16 new particles (random offset +-150, random direction, speed 0xB4..0x243); ends past counter 3 with no
//     particle left.
//   EndTask (0x5BF100, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick; counter 1: the action result; after 12 ticks: flag 2, done; ends.
//   FadeInTask (0x5BF310) / FadeOutTask (0x5BF1C0) - screen flash (Effect_SetScreenFlash 0x5713E0) 0 -> 0x800 / 0x800 -> 0
//     over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5BF360) - every tick: ghost slot counter % 5 (0x22B7350, 5 x 0x84: used + the weapon's bone matrices)
//     = a snapshot of his weapon's bone matrices (0x5BF510); his current ones kept in 0x22B54F8; then up to 5 ghosts (fewer
//     once the end tick is reached), newest snapshot first from slot counter - 1 down (the 5th is this tick's), each drawn
//     with its snapshot put into the weapon's bones (0x5BF550) and his entity colour +0x28 = 0xD1DA40[k] (0x505050 ..
//     0x101010, additive) through GhostDraw (0x5BF590: the weapon model alone under his world matrix, RenderGeometry into
//     the frame arena 0x1D8E054); his bones and +0x28 put back; ends 5 ticks after the end tick.
// Every task except RootTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x22B2A38..0x22B75F4 (end tick, module tick, shot index, pools, queues, particle arrays, bind block, saved
//   bones 0x22B54F8, flags 0x22B5578, aux queue, context, shot records, caster slot, texture file, ghost slots, counts,
//   texture base, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace shot188
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x22B2A38); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x22B2A3C); }
	inline uint32_t &Flags() { return var<uint32_t>(0x22B5578); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x22B63A0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x22B7348); }
	inline int32_t &ImpactCount() { return var<int32_t>(0x22B75E8); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x22B75EC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x22B75F0); }
	inline TaskQueue *QShots() { return (TaskQueue *)0x22B38A8; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x22B5580; }
	static const uint32_t PARTS_A = 0x22B38B8, PARTS_B = 0x22B46E8;  // 150 x 0x18 each
	static const uint32_t SHOTS = 0x22B63A8;                         // 100 x 0x28 shot records
	static const uint32_t BIND_OUT = 0x22B46C8;
	static const uint32_t BONES_NOW = 0x22B54F8;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x22B7350, GHOSTS_END = 0x22B75E4; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;

	static const uint32_t ORIG_RootTask = 0x5BF650;
	static const uint32_t ORIG_StartTask = 0x5BF220;
	static const uint32_t ORIG_ShotTask = 0x5BE4D0;
	static const uint32_t ORIG_EndTask = 0x5BF100;
	static const uint32_t ORIG_FadeOutTask = 0x5BF1C0;
	static const uint32_t ORIG_FadeInTask = 0x5BF310;
	static const uint32_t ORIG_AfterimageTask = 0x5BF360;
	static const uint32_t ORIG_FlashTask = 0x5BE980;
	static const uint32_t ORIG_Flash2Task = 0x5BEAC0;
	static const uint32_t ORIG_SmokeTask = 0x5BEC30;
	static const uint32_t ORIG_GroundSmokeTask = 0x5BECD0;
	static const uint32_t ORIG_ImpactTask = 0x5BEDE0;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD1B4B8;
	static const uint32_t MODEL_Flash = 0xD1D0F0, MODEL_Flash2 = 0xD1D638;
	static const uint32_t SEQ_Smoke = 0xD1CA28, SEQ_GroundSmoke = 0xD1CB00, SEQ_Impact = 0xD1CB74, SEQ_Spark = 0xD1CF44;
	static const void *const SOUND_Gun = (const void *)0xD1DA28;
	static const int32_t *const MUZZLE_REF = (const int32_t *)0xD1DA30; // (0, 0, -0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD1DA40;

	// engine functions not in fx_port.h / mag_common.h
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t angle, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, angle, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // b = a x b (3x3)
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }

	// ------------------------------------------------------------------
	// Node / record layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool: 4 x 0x10
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad;
	};
	struct Node // shot / aux queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t f0E;     // +0x0E shot: shot number; muzzle tasks: their shot record; impact: impact number; fades: length
		int16_t pos[4];  // +0x10 impact: its point (+0x16: the target's size * 2000 / 4096)
		int16_t f18;     // +0x18 shot: chain; flash: roll; fades: level
		int16_t f1A;     // +0x1A shot: target slot
		int16_t f1C;     // +0x1C scale / size
		int16_t f1E;     // +0x1E Flash2: scale step
		uint8_t pad20[4];
	};
	struct ShotRec // 0x22B63A8 + 0x28 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct Particle // 0x22B38B8 / 0x22B46E8, 150 x 0x18
	{
		int32_t id;      // 0 = free, else impact number + 1
		int16_t frame;
		int16_t size;    // TransformCameraByShadowRotation size
		int16_t pos[4];  // (pos[3] = the impact node's +0x16)
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x22B7350, 5 x 0x84
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Normal Shot nodes");
	static_assert(sizeof(ShotRec) == 0x28 && sizeof(Particle) == 0x18 && sizeof(Ghost) == 0x84, "Normal Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(SHOTS + 0x28 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(GHOSTS + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
}
}

#ifdef FF8_FX_HELD
#include "mag188_normal_shot_held.h"
#endif

namespace ff8fx
{
namespace shot188
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5BE900 / 0x5BE880 = Rough Divide's 0x5F6500 / 0x5F6480)
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
	// Afterimages (0x5BF510 / 0x5BF550 / 0x5BF590)
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

	// a muzzle flash model at the shot record: its rotation x roll, uniform scale
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
	static int32_t Flash2Fade(int32_t c) { return mul32(c, 0x555) - 0x555; }

	// a sprite sequence at frame `frame` through the camera-facing shadow rotation
	static void SequenceDraw(uint32_t seq, int16_t frame, int16_t kind, uint8_t *h)
	{
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = kind;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
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
	inline Particle *Sparks(const Node *n) { return (Particle *)((n->f0E & 1) ? PARTS_B : PARTS_A); }

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// FlashTask (0x5BE980): 2 ticks, faded 0xC00 on counter 1
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_node(ORIG_FlashTask, n);)
		MuzzleModel(n, MODEL_Flash, n->counter == 1, 0xC00);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// Flash2Task (0x5BEAC0): 4 ticks, scale + step, step - step / 3, fade from counter 1
	static uint32_t __cdecl Flash2Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_node(ORIG_Flash2Task, n);)
		MuzzleModel(n, MODEL_Flash2, n->counter >= 1, Flash2Fade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 3));
		n->counter++;
		return n->counter >= 4 ? TASK_END : 0;
	}

	// SmokeTask (0x5BEC30): ends with its sequence
	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_node(ORIG_SmokeTask, n);)
		const int16_t next = SmokeDraw(n);
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// GroundSmokeTask (0x5BECD0): ends with its sequence
	static uint32_t __cdecl GroundSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_node(ORIG_GroundSmokeTask, n);)
		const int16_t next = GroundSmokeDraw(n);
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// ImpactTask (0x5BEDE0): the impact flash, then its particles (draw THEN update)
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = Sparks(n);
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_node(ORIG_ImpactTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (n->counter < 0x10) ImpactFlashDraw(n, h);
		*(int16_t *)(h + 0x24) = 8;
		*(uint32_t *)h = SEQ_Spark;
		int32_t alive = 0;
		const int32_t id = n->f0E + 1;
		for (Particle *e = arr; e < arr + 150; e++)
		{
			if (e->id != id) continue;
			// 30 fps layer: see mag188_normal_shot_held.inc
			FX_HELD(held_note_spark(e);)
			SparkDraw(e, n->pos[3], h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag188_normal_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			SparkMove(e);
			alive++;
		}
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 0x10; k++)
			{
				int32_t i = 0;
				while (i < 150 && arr[i].id != 0) i++;
				if (i >= 150) break;
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

	// ShotTask (0x5BE4D0): one trigger press
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
			d->counter = 0;
			d->f0E = n->f0E;
			d->f1C = (int16_t)(mul32((int32_t)k->f1C * 9, 0x200) >> 12);
		}
		if (n->counter == 1)
		{
			uint8_t *t = Entity(n->f1A);
			int32_t bone = 0;
			const uint8_t anchors = ***(const uint8_t ***)(t + 0x64);
			if (anchors) bone = CrtRand() % anchors;
			Node *i = AddTask(QAux(), ORIG_ImpactTask);
			i->f0E = (int16_t)ImpactCount();
			ImpactCount()++;
			i->counter = 0;
			GetEffectSpawnPosition(t, bone, CrtRand() % 0x1000, i->pos);
			i->pos[3] = (int16_t)(mul32(*(const int16_t *)(Entity(n->f1A) + 0x26), 2000) >> 12);
			i->f1C = (int16_t)(CrtRand() % 0xC00 + 0x800);
			ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
			BdPlaySE(SOUND_Gun, (n->f0E & 1) ? 1 : 2, 0x80);
		}
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	// FadeOutTask (0x5BF1C0): level - level / length * counter, 0 at the end
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

	// FadeInTask (0x5BF310): level / length * counter
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5BF360): the weapon's ghosts
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
		// 30 fps layer: see mag188_normal_shot_held.inc
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

	// StartTask (0x5BF220): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5BF100, root queue): his chain 2, the fade-out, the afterimages' end, the action result; done after 12 ticks
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
		if (n->counter == 1) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xC) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	// RootTask (0x5BF650): packet arena parity, the shot queue, the aux queue, the module tick
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (*(const uint8_t *)&r->counter & 1) ? TexBase() + ARENA_ODD : TexBase();
		const int shots = ExecuteTaskQueue(QShots());
		const int aux = ExecuteTaskQueue(QAux());
		if (!(UpdateFlags() & 0x201)) ModuleTick()++;
		r->counter++;
		return (aux == 0 && shots == 0 && (Flags() & 2)) ? TASK_END : 0;
	}
}

	void register_mag188_normal_shot()
	{
		register_port(shot188::ORIG_RootTask, (void *)shot188::RootTask, "188 RootTask", 188);
		register_port(shot188::ORIG_StartTask, (void *)shot188::StartTask, "188 StartTask", 188);
		register_port(shot188::ORIG_ShotTask, (void *)shot188::ShotTask, "188 ShotTask", 188);
		register_port(shot188::ORIG_EndTask, (void *)shot188::EndTask, "188 EndTask", 188);
		register_port(shot188::ORIG_FadeOutTask, (void *)shot188::FadeOutTask, "188 FadeOutTask", 188);
		register_port(shot188::ORIG_FadeInTask, (void *)shot188::FadeInTask, "188 FadeInTask", 188);
		register_port(shot188::ORIG_AfterimageTask, (void *)shot188::AfterimageTask, "188 AfterimageTask", 188);
		register_port(shot188::ORIG_FlashTask, (void *)shot188::FlashTask, "188 FlashTask", 188);
		register_port(shot188::ORIG_Flash2Task, (void *)shot188::Flash2Task, "188 Flash2Task", 188);
		register_port(shot188::ORIG_SmokeTask, (void *)shot188::SmokeTask, "188 SmokeTask", 188);
		register_port(shot188::ORIG_GroundSmokeTask, (void *)shot188::GroundSmokeTask, "188 GroundSmokeTask", 188);
		register_port(shot188::ORIG_ImpactTask, (void *)shot188::ImpactTask, "188 ImpactTask", 188);
		// 30 fps layer: see mag188_normal_shot_held.inc
		FX_HELD(register_mag188_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag188_normal_shot_held.inc"
#endif
