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

// Effect 196: Quick Shot (Irvine's Shot limit break, kernel.bin section 21 entry 5 +4; MAG_196_SHOT_QUICK_SHOT). Normal
// Shot 188's text (mag188_normal_shot.cpp) with a 2-tick shot: everything on its first tick, no smoke.
//
// How the battle drives it: the opening action is a normal magic cast (BattleActionSequence_Tick_MagicCast 0x50A9A0 sets
// g_MagicCastContext 0x1D99A78 +8 sequence_mode = 0 for the limit's action types); every trigger press of the Shot window (UI
// code: BattleMenu_ShotWindow_Update 0x4AD8D0, the latch 0x1D76762) becomes an action that BattleActionSequence_Tick_Shot
// 0x50BEE0 runs: it calls the module's _Init again with +8 = 2 (a shot, action +1 == 0xED) or 1 (the closing action) and waits
// for +9 done = 1. The Shot window's timer is UI code: no effect module drives it.
//
// Structure (file loader 0x5B18E0 = the texture file 0xD073F4; _Init 0x5B1900 keeps the texture base 0x226A964, the context
// 0x2269718 and the caster slot 0x226A6C0 (context byte 0), then SetupByMode 0x5B1930 (both run original): mode 0 = the
// root pool (4 x 0x10, queue 0x2266C10) with RootTask, the shot queue 0x2266C20 (100 x 0x24) with StartTask, the aux queue
// 0x22688F8 (100 x 0x24), flags 0x22688F0 / shot index 0x2265DBC / shot count 0x226A95C / impact count 0x226A960 / module tick
// 0x2265DB4 cleared, both particle arrays (0x2266C30 / 0x2267A60, 150 x 0x18) free, resetSomeCameraData, the TIM upload;
// mode 1 = EndTask in the root queue; mode 2 = a ShotTask in the shot queue: +0x0E = shot count % 100 (its shot record
// 0x2269720 + 0x28 * n), +0x18 = chain 3 + shot index (0, 1, 2 cycling), +0x1A = the first target's slot):
//   RootTask (0x5B29A0) - packet cursor 0x226A968 = the texture base (+ 0xC000 on odd counters), the shot queue then the aux
//     queue, the module tick + 1 (not on draw-only ticks); ends when both are empty and EndTask finished (flag 2).
//   StartTask (0x5B2570, shot queue) - counter 0: binds the module's animation set to Irvine (BattleEntity_BindExternalAnimSet
//     0x5022C0, com data 0xD05288, out block 0x2267A40), his chain 1, FadeInTask + AfterimageTask in the aux queue, the
//     afterimage end tick 0x2265DB0 = 999999, the 5 ghost slots free; after 7 ticks: flag 1 (ready), done; ends.
//   ShotTask (0x5B1A90, shot queue; nothing until flag 1) - counter 0 waits while another shot is busy (flag 4), then his
//     chain (+0x18) and flag 4; the muzzle (bone 2 of his WEAPON model, entity +0x78, through 0x5B1DD0 / 0x5B1E50 = the
//     Renzokuken finishers' weapon-bone helpers) into the shot record: point + direction * 350, rotation from (0, 0, -0x1000)
//     to the direction; done and flag 4 cleared at once (the next shot may start on the next tick); FlashTask (random roll,
//     scale 0x900..0xDFF), Flash2Task (random yaw, scale 0x800..0xA7F); ImpactTask at a random effect anchor of the target
//     (GetEffectSpawnPosition 0x502170, random angle; size 0x800..0xBFF; +0x16 = target size * 2000 / 4096), the action
//     result (ApplyActionResultToTargets 0x506BA0), the gun sound 0xD073C4 (volume 1 / 2 alternating); ends at 2.
//   FlashTask (0x5B1ED0) - prim model 0xD0609C at the shot record (its rotation x roll), uniform scale; counter 1 faded 0xC00;
//     2 ticks.  Flash2Task (0x5B2010) - prim model 0xD06FD4 at the shot record's point, turned about y only (yaw), uniform
//     scale; counter 1 faded 0x800; 2 ticks.
//   ImpactTask (0x5B2130) - sequence 0xD05DB0 (frame = counter, counters 0..15) at its point, camera facing; then every
//     particle of its array (impact number parity) with its id (impact number + 1): sequence 0xD05FC0 at its frame, then
//     (not on draw-only ticks) frame + 1, dies when its sequence ended, else position + velocity, velocity - velocity / 4;
//     counter 0: 12 new particles (random offset +-150, random direction, speed 0xB4..0x243); ends past counter 3 with no
//     particle left.
//   EndTask (0x5B2450, root queue; nothing while a shot is busy) - counter 0: his chain 2, FadeOutTask, the afterimage end
//     tick = the module tick, the action result; after 12 ticks: flag 2, done; ends.
//   FadeInTask (0x5B2660) / FadeOutTask (0x5B2510) - screen flash (Effect_SetScreenFlash 0x5713E0) 0 -> 0x800 / 0x800 -> 0
//     over 8 ticks (the last fade-out tick sets 0).
//   AfterimageTask (0x5B26B0) - every tick: ghost slot counter % 5 (0x226A6C8, 5 x 0x84: used + the weapon's bone matrices)
//     = a snapshot of his weapon's bone matrices (0x5B2860); his current ones kept in 0x2268870; then up to 5 ghosts (fewer
//     once the end tick is reached), newest snapshot first from slot counter - 1 down (the 5th is this tick's), each drawn
//     with its snapshot put into the weapon's bones (0x5B28A0) and his entity colour +0x28 = 0xD073E0[k] (0x505050 ..
//     0x101010, additive) through GhostDraw (0x5B28E0: the weapon model alone under his world matrix, RenderGeometry into
//     the frame arena 0x1D8E054); his bones and +0x28 put back; ends 5 ticks after the end tick.
// Every task except RootTask tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// ENTITY WRITES: Irvine (caster): the animation set bind (StartTask), chains 1 / 3..5 / 2 (QueueChainTransformation 0x505C00),
//   per ghost his weapon's bone matrices and his colour word +0x28 (both put back in the same call); targets: action results.
// CAMERA: none written (the view 0x1D97778 is read; resetSomeCameraData is setup code).
// Module globals: 0x2265DB0..0x226A96C (end tick, module tick, shot index, pools, queues, particle arrays, bind block, saved
//   bones 0x2268870, flags 0x22688F0, aux queue, context, shot records, caster slot, texture file, ghost slots, counts,
//   texture base, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace quick196
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &EndTick() { return var<int32_t>(0x2265DB0); }      // afterimages end here (999999 until EndTask)
	inline int32_t &ModuleTick() { return var<int32_t>(0x2265DB4); }
	inline uint32_t &Flags() { return var<uint32_t>(0x22688F0); }       // 1 ready, 2 ended, 4 a shot is busy
	inline CastContext *&Ctx() { return var<CastContext *>(0x2269718); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x226A6C0); }
	inline int32_t &ImpactCount() { return var<int32_t>(0x226A960); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x226A964); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x226A968); }
	inline TaskQueue *QShots() { return (TaskQueue *)0x2266C20; }
	inline TaskQueue *QAux() { return (TaskQueue *)0x22688F8; }
	static const uint32_t PARTS_A = 0x2266C30, PARTS_B = 0x2267A60;  // 150 x 0x18 each
	static const uint32_t SHOTS = 0x2269720;                         // 100 x 0x28 shot records
	static const uint32_t BIND_OUT = 0x2267A40;
	static const uint32_t BONES_NOW = 0x2268870;                     // his weapon's current bone matrices
	static const uint32_t GHOSTS = 0x226A6C8, GHOSTS_END = 0x226A95C; // 5 x 0x84
	static const uint32_t ARENA_ODD = 0xC000;

	static const uint32_t ORIG_RootTask = 0x5B29A0;
	static const uint32_t ORIG_StartTask = 0x5B2570;
	static const uint32_t ORIG_ShotTask = 0x5B1A90;
	static const uint32_t ORIG_EndTask = 0x5B2450;
	static const uint32_t ORIG_FadeOutTask = 0x5B2510;
	static const uint32_t ORIG_FadeInTask = 0x5B2660;
	static const uint32_t ORIG_AfterimageTask = 0x5B26B0;
	static const uint32_t ORIG_FlashTask = 0x5B1ED0;
	static const uint32_t ORIG_Flash2Task = 0x5B2010;
	static const uint32_t ORIG_ImpactTask = 0x5B2130;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD05288;
	static const uint32_t MODEL_Flash = 0xD0609C, MODEL_Flash2 = 0xD06FD4;
	static const uint32_t SEQ_Impact = 0xD05DB0, SEQ_Spark = 0xD05FC0;
	static const void *const SOUND_Gun = (const void *)0xD073C4;
	static const int32_t *const MUZZLE_REF = (const int32_t *)0xD073D0; // (0, 0, -0x1000)
	static const uint32_t *const GHOST_COLOURS = (const uint32_t *)0xD073E0;

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
	struct ShotRec // 0x2269720 + 0x28 * n
	{
		int16_t pos[4];  // the muzzle point (pos[3] never written)
		Mat4x3 m;        // rotation (0, 0, -0x1000) -> the muzzle direction
	};
	struct Particle // 0x2266C30 / 0x2267A60, 150 x 0x18
	{
		int32_t id;      // 0 = free, else impact number + 1
		int16_t frame;
		int16_t size;    // TransformCameraByShadowRotation size
		int16_t pos[4];  // (pos[3] = the impact node's +0x16)
		int16_t vel[3];
		int16_t pad;
	};
	struct Ghost // 0x226A6C8, 5 x 0x84
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
#include "mag196_quick_shot_held.h"
#endif

namespace ff8fx
{
namespace quick196
{
	// ------------------------------------------------------------------
	// Weapon bones (0x5B1E50 / 0x5B1DD0 = Rough Divide's 0x5F6500 / 0x5F6480)
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
	// Afterimages (0x5B2860 / 0x5B28A0 / 0x5B28E0)
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
	// the yawed flash model at the shot record's point (y 0 rotation only), uniform scale
	static void YawModel(const Node *n, uint32_t model, bool faded, int32_t fade)
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
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		m.t[2] = s->pos[2];
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		ModelDraw(model, faded, fade);
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
	// FlashTask (0x5B1ED0): 2 ticks, faded 0xC00 on counter 1
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag196_quick_shot_held.inc
		FX_HELD(held_note_node(ORIG_FlashTask, n);)
		MuzzleModel(n, MODEL_Flash, n->counter == 1, 0xC00);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// Flash2Task (0x5B2010): 2 ticks, faded 0x800 on counter 1
	static uint32_t __cdecl Flash2Task(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag196_quick_shot_held.inc
		FX_HELD(held_note_node(ORIG_Flash2Task, n);)
		YawModel(n, MODEL_Flash2, n->counter == 1, 0x800);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// ImpactTask (0x5B2130): the impact flash, then its particles (draw THEN update)
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = Sparks(n);
		// 30 fps layer: see mag196_quick_shot_held.inc
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
			// 30 fps layer: see mag196_quick_shot_held.inc
			FX_HELD(held_note_spark(e);)
			SparkDraw(e, n->pos[3], h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				// 30 fps layer: see mag196_quick_shot_held.inc
				FX_HELD(held_note_spark_next(e);)
				continue;
			}
			SparkMove(e);
			alive++;
		}
		// 30 fps layer: see mag196_quick_shot_held.inc
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int32_t k = 0; k < 0xC; k++)
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

	// ShotTask (0x5B1A90): one trigger press, all on its first tick
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
		if (n->counter == 0)
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
			g->f1C = (int16_t)(CrtRand() % 0x280 + 0x800);
		}
		if (n->counter == 0)
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
		return n->counter >= 2 ? TASK_END : 0;
	}

	// FadeOutTask (0x5B2510): level - level / length * counter, 0 at the end
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

	// FadeInTask (0x5B2660): level / length * counter
	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	// AfterimageTask (0x5B26B0): the weapon's ghosts
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
		// 30 fps layer: see mag196_quick_shot_held.inc
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

	// StartTask (0x5B2570): his animation set, the fade-in, the afterimages; ready after 7 ticks
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

	// EndTask (0x5B2450, root queue): his chain 2, the fade-out, the afterimages' end and the action result on its first
	// tick; done after 12 ticks
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
		if (n->counter == 0) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xC) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	// RootTask (0x5B29A0): packet arena parity, the shot queue, the aux queue, the module tick
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag196_quick_shot_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (*(const uint8_t *)&r->counter & 1) ? TexBase() + ARENA_ODD : TexBase();
		const int shots = ExecuteTaskQueue(QShots());
		const int aux = ExecuteTaskQueue(QAux());
		if (!(UpdateFlags() & 0x201)) ModuleTick()++;
		r->counter++;
		return (aux == 0 && shots == 0 && (Flags() & 2)) ? TASK_END : 0;
	}
}

	void register_mag196_quick_shot()
	{
		register_port(quick196::ORIG_RootTask, (void *)quick196::RootTask, "196 RootTask", 196);
		register_port(quick196::ORIG_StartTask, (void *)quick196::StartTask, "196 StartTask", 196);
		register_port(quick196::ORIG_ShotTask, (void *)quick196::ShotTask, "196 ShotTask", 196);
		register_port(quick196::ORIG_EndTask, (void *)quick196::EndTask, "196 EndTask", 196);
		register_port(quick196::ORIG_FadeOutTask, (void *)quick196::FadeOutTask, "196 FadeOutTask", 196);
		register_port(quick196::ORIG_FadeInTask, (void *)quick196::FadeInTask, "196 FadeInTask", 196);
		register_port(quick196::ORIG_AfterimageTask, (void *)quick196::AfterimageTask, "196 AfterimageTask", 196);
		register_port(quick196::ORIG_FlashTask, (void *)quick196::FlashTask, "196 FlashTask", 196);
		register_port(quick196::ORIG_Flash2Task, (void *)quick196::Flash2Task, "196 Flash2Task", 196);
		register_port(quick196::ORIG_ImpactTask, (void *)quick196::ImpactTask, "196 ImpactTask", 196);
		// 30 fps layer: see mag196_quick_shot_held.inc
		FX_HELD(register_mag196_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag196_quick_shot_held.inc"
#endif
