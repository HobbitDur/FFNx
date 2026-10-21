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

// Effect 305: Gatling Gun (Quistis' Blue Magic, kernel.bin section 19 "Gatling Gun", magic +4 = 305;
// MAG_305_GATLING_GUN_QUISTIS*), cast by a party member (Quistis, her whip) on one enemy.
//
// The module 0x5A57B0..0x5A6C00 adapts enemy Chain Gun 190 (mag190_chain_gun.cpp, GIM47N's attack): the
// muzzle flash, impact, impact flash, spark and root tasks are identical up to addresses (exe data
// -0x2B644, module globals -0x3C4C8 / -0x3C4A8 / -0x3C7B0); the master is rewritten (190's shot table and
// its builders are gone) and five functions differ:
//   Director (0x5A5890; IDA counts it in the setup MAG_305_UNKNOWN 0x5A57D0) - every tick (also in the
//     draw-only mode) the muzzle 0x2247788 = the caster's effect point 0xF0 (flag 0xC00) 100 units ahead
//     along her facing (190: anchor 0xB). Timeline: 1 the caster's animation set (com data 0xCEB5E8, out
//     block 0x2245FD8) bound (BS_BindEntityModelFromComData) + chain transformation 1 (Quistis' casting
//     animation); every odd counter 9..0x1F a muzzle flash; 9 the smoke; 8 five misses on the line from
//     the muzzle to the target (k / 6 of the way, delays 2k) and the sound 0xCEDF28; 0x12 seven hits on
//     the target rising by 1/8 of its height (delays 2k); 0x1E damage; screen flash ramps 0..8 / 0x1C..0x24;
//     ends after 0x24.
//   SmokeTask (0x5A5DA0) - the smoke is drawn grey (sequence colour 0x404040, mode 4; 190: none); new
//     particles: size 0x400..0x9FF, 0..19 from the muzzle, speed 0x14..0x45, rising 10..44 (190: 0x800..,
//     0..199, 0x1E..0x6D, 15..54).
//   MissTask (0x5A6070) / HitTask (0x5A6580) - the dust / sparks task's id is the BIT 1 << shot (190: the
//     shot number); DustTask (0x5A6330) / SparksTask (0x5A68C0) - draw the particles whose id has that bit
//     (190: equal id). (No dust group shares the smoke's bit 0: the smoke moves smoke particles only.)
//
// Structure (setup MAG_305_GATLING_GUN_QUISTIS 0x5A57D0, file loader MAG_305_GATLING_GUN_QUISTIS_FL
// 0x5A57B0 = the texture file named at 0xCEDF30; the setup keeps the context, the first target's slot and
// the caster slot, sets up the root queue 0x2245668 (pool 1 x 0x10) and the effect queue 0x2246958 (pool
// 100 x 0x24) with the root and the director, clears both particle arrays, starts camera animation
// 0xCEB2DC and queues the TIM):
//   RootTask (0x5A6BC0) - alternates the packet arena (0x224F790 on odd counters, else 0x2247790),
//     runs the effect queue, ends when it is empty.
//   MuzzleTask (0x5A5C50) - draws THEN updates: prim model 0xCED698 at the muzzle copied at its spawn
//     (drawn twice), caster facing + random roll, random scale, fade counter << 11; ends at 2.
//   SmokeTask (0x5A5DA0) - draws THEN updates every particle of array A (0x2245FF8, 100 x 0x18) whose id
//     has bit 0 set: flipbook 0xCED358 (frame++, dies when the flipbook ends), size +0x18, position +=
//     velocity, x / z velocity - 1/8. Counters 0..23: three new particles (id 1) near the muzzle, random
//     direction. Ends past counter 15 with no live particle.
//   MissTask (0x5A6070) / HitTask (0x5A6580) - one tick after their delay: the impact tasks of one shot.
//     Miss: ImpactTask (model 0xCED998 on the ground, growing), FlashTask (y -200) and DustTask.
//     Hit: a point near the target (random direction through the target's matrix), SparkTask
//     (model 0xCEDC60), FlashTask and SparksTask.
//   ImpactTask (0x5A6160) / SparkTask (0x5A6750) - draw THEN update: prim model, scale growing by a
//     step that decays by 1/8, fade from the counter; end at 9 / 5.
//   FlashTask (0x5A62A0) - flipbook 0xCED15C at the impact, frame = counter; ends at 16.
//   DustTask (0x5A6330) - draws THEN updates the array A particles with its shot's bit (flipbook
//     0xCED358, colour 0x404040); counter 0: ten new ones around the impact point.
//   SparksTask (0x5A68C0) - the same over array B (0x2245678, flipbook 0xCED504, velocity - 1/3);
//     counter 0: twelve new ones.
// Every drawing task tests the draw-only flags (battle_to_update_flags 0x201) after drawing.
// Entity writes: the caster's animation set and chain transformation (director counter 1, through the
// engine); the target's action result (counter 0x1E, engine). Camera: the setup's camera animation
// 0xCEB2DC (engine); no camera word is written by the tasks. Stage: the screen flash level words.
// Module globals: 0x2245650..0x2257794 (target slot, root pool/queue, particle array B, the bind block
// 0x2245FD8, particle array A, effect queue + pool, context, caster slot, texture, muzzle, the two packet
// arenas, packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace gg305
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x2245650); }  // the first target
	inline CastContext *&Ctx() { return var<CastContext *>(0x2247778); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x224777C); }
	inline int16_t *Muzzle() { return (int16_t *)0x2247788; }            // caster effect point 0xF0, 100 ahead
	inline uint32_t &Cursor() { return var<uint32_t>(0x2257790); }
	inline TaskQueue *QEffects() { return (TaskQueue *)0x2246958; }     // pool 0x2246968, 100 x 0x24
	static const uint32_t ARENA_A = 0x224F790, ARENA_B = 0x2247790;     // 0x8000 bytes each

	static const uint32_t ORIG_RootTask = 0x5A6BC0;
	static const uint32_t ORIG_MasterTask = 0x5A5890;   // the director (IDA: inside the setup MAG_305_UNKNOWN 0x5A57D0)
	static const uint32_t ORIG_MuzzleTask = 0x5A5C50;
	static const uint32_t ORIG_SmokeTask = 0x5A5DA0;
	static const uint32_t ORIG_MissTask = 0x5A6070;
	static const uint32_t ORIG_ImpactTask = 0x5A6160;
	static const uint32_t ORIG_FlashTask = 0x5A62A0;
	static const uint32_t ORIG_DustTask = 0x5A6330;
	static const uint32_t ORIG_HitTask = 0x5A6580;
	static const uint32_t ORIG_SparkTask = 0x5A6750;
	static const uint32_t ORIG_SparksTask = 0x5A68C0;
	static const uint32_t MODEL_Muzzle = 0xCED698;
	static const uint32_t MODEL_Impact = 0xCED998;
	static const uint32_t MODEL_Spark = 0xCEDC60;
	static const uint32_t SEQ_Smoke = 0xCED358;  // smoke and dust
	static const uint32_t SEQ_Flash = 0xCED15C;
	static const uint32_t SEQ_Sparks = 0xCED504;
	static const void *const SOUND_Gun = (const void *)0xCEDF28;
	static const uint32_t BIND_Block = 0x2245FD8;   // 0x20-byte block BS_BindEntityModelFromComData fills
	static const uint32_t COM_Anim = 0xCEB5E8;      // the module's animation set (com data)

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect point `bone`
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); } // sub_56C220
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad;
	};
	struct Node // effect queue nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t f0E;     // +0x0E miss, hit: delay / dust, sparks: their shot's bit
		int16_t f10;     // +0x10 x
		int16_t f12;     // +0x12 y
		int16_t f14;     // +0x14 z
		int16_t f16;     // +0x16 (4th word of the copied position)
		int16_t f18;     // +0x18 yaw / miss, hit: shot number
		int16_t f1A;     // +0x1A roll
		int16_t f1C;     // +0x1C scale
		int16_t f1E;     // +0x1E scale step
		uint8_t pad20[4];
	};
	struct Particle // arrays A 0x2245FF8 and B 0x2245678, 100 x 0x18 each
	{
		int32_t id;      // 0 = free; A: 1 = smoke, else the dust's shot bit; B: the sparks' shot bit
		int16_t frame;   // flipbook frame
		int16_t angle;   // TransformCameraByShadowRotation angle (size)
		int16_t pos[4];
		int16_t vel[3];
		int16_t f16;     // B: sequence header +8
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Gatling Gun (Quistis) nodes");
	static_assert(sizeof(Particle) == 0x18, "Gatling Gun (Quistis) records");

	inline Particle *PartsA() { return (Particle *)0x2245FF8; }
	inline Particle *PartsB() { return (Particle *)0x2245678; }
	inline int16_t *EntPos(int32_t slot) { return (int16_t *)(Entity(slot) + 0x1C); }
	inline uint32_t rd32(const void *p) { return *(const uint32_t *)p; }
	inline Node *AddTask(uint32_t fn) { return (Node *)AddTaskToQueue(QEffects(), fn); }
}
}

#ifdef FF8_FX_HELD
#include "mag305_gatling_gun_quistis_held.h"
#endif

namespace ff8fx
{
namespace gg305
{
	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	// prim model header (0x58 bytes on the scratch stack): +0x0C fade, +0x1C flags
	static void ModelDraw(uint32_t model, bool faded, int32_t fade, int times)
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
		for (int k = 0; k < times; k++) Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		FieldFree(0x58);
	}

	// muzzle flash (drawn twice): yaw f18, roll f1A, uniform scale f1C
	static void MuzzleDraw(const Node *n, bool faded, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = n->f1A;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->f10;
		m.t[2] = n->f14;
		m.t[1] = n->f12;
		const int32_t s = n->f1C;
		const int32_t v[3] = { s, s, s };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		TransformCameraByShadowRotation(&n->f10, 0, -((int32_t)n->f1C >> 3));
		GteSetRotMatrix(&m);
		ModelDraw(MODEL_Muzzle, faded, fade, 2);
	}
	static int32_t MuzzleFade(int32_t c) { return shl32(c, 11); }

	// ground impact: yaw f18, uniform scale f1C
	static void ImpactDraw(const Node *n, bool faded, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->f10;
		m.t[1] = n->f12;
		const int32_t s = n->f1C;
		const int32_t v[3] = { s, s, s };
		m.t[2] = n->f14;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		ModelDraw(MODEL_Impact, faded, fade, 1);
	}
	static int32_t ImpactFade(int32_t c) { return shl32(c, 9); }

	// spark on the target: (0x400, yaw f18, 0) times a roll f1A about y, uniform scale f1C
	static void SparkDraw(const Node *n, bool faded, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f1A;
		ang[2] = 0;
		ang[3] = 0; // never written by the original (not read)
		Mat4x3 roll;
		BuildRotationMatrixFromAngles(ang, &roll);
		ang[0] = 0x400;
		ang[1] = n->f18;
		ang[2] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->f10;
		m.t[1] = n->f12;
		const int32_t s = n->f1C;
		const int32_t v[3] = { s, s, s };
		m.t[2] = n->f14;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		MatrixMultiply3(&m, &roll);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		ModelDraw(MODEL_Spark, faded, fade, 1);
	}
	static int32_t SparkFade(int32_t c) { return shl32(c, 10) - 0x400; }

	// impact flash: flipbook frame `frame` at the node position, size f1C
	static void FlashDraw(const Node *n, int16_t frame)
	{
		TransformCameraByShadowRotation(&n->f10, n->f1C, -((int32_t)n->f1C >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(int16_t *)(h + 4) = frame;
		*(uint32_t *)h = SEQ_Flash;
		*(int16_t *)(h + 0x24) = 0;
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
		FieldFree(0xB4);
	}

	// one particle: flipbook frame at pos (header allocated by the task, 0xB4 bytes)
	static void ParticleDraw(uint8_t *h, int16_t frame, const int16_t *pos, int16_t angle)
	{
		*(int16_t *)(h + 4) = frame;
		TransformCameraByShadowRotation(pos, angle, -((int32_t)angle >> 4));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}

	// the particle updates (flipbook still running)
	static void SmokeMove(Particle *e)
	{
		e->angle = (int16_t)(e->angle + 0x18);
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 3));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 3));
	}
	static void DustMove(Particle *e)
	{
		e->angle = (int16_t)(e->angle + 0x20);
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 3));
		e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 3));
	}
	static void SparksMove(Particle *e)
	{
		e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
		e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
		e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
		e->vel[0] = (int16_t)(e->vel[0] - e->vel[0] / 3);
		e->vel[1] = (int16_t)(e->vel[1] - e->vel[1] / 3);
		e->vel[2] = (int16_t)(e->vel[2] - e->vel[2] / 3);
	}

	inline Particle *FreeParticle(Particle *a)
	{
		int idx = 0;
		while (idx < 100 && a[idx].id != 0) idx++;
		return idx < 100 ? &a[idx] : nullptr;
	}

	// ------------------------------------------------------------------
	// Tasks
	// ------------------------------------------------------------------
	// Muzzle flash (0x5A5C50)
	static uint32_t __cdecl MuzzleTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_MuzzleTask, n);)
		MuzzleDraw(n, n->counter >= 0, MuzzleFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// Smoke (0x5A5DA0): array A, id bit 0, grey
	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_SmokeTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Smoke;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		for (Particle *e = PartsA(); e < PartsA() + 100; e++)
		{
			if (!(*(const uint8_t *)&e->id & 1)) continue;
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part(e);)
			ParticleDraw(h, e->frame, e->pos, e->angle);
			if (!(UpdateFlags() & 0x201))
			{
				e->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) e->id = 0;
				else
				{
					SmokeMove(e);
					alive++;
				}
			}
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part_next(e);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter >= 0 && n->counter <= 0x17)
		{
			for (int k = 0; k < 3; k++)
			{
				Particle *e = FreeParticle(PartsA());
				if (!e) break;
				e->id = 1;
				e->frame = 0;
				e->angle = (int16_t)(CrtRand() % 0x600 + 0x400);
				*(uint32_t *)&e->pos[0] = rd32(&Muzzle()[0]);
				*(uint32_t *)&e->pos[2] = rd32(&Muzzle()[2]);
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(v, v);
				const int32_t r = CrtRand() % 20;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(r, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x32 + 0x14;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				const int32_t up = CrtRand() % 0x23;
				e->vel[1] = (int16_t)(-10 - up);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		n->counter++;
		if (n->counter < 0x10 || alive != 0) return 0;
		return TASK_END;
	}

	// Miss (0x5A6070): after f0E ticks, the ground impact, its flash and its dust
	static uint32_t __cdecl MissTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (n->f0E > 0)
		{
			n->f0E--;
			return 0;
		}
		Node *a = AddTask(ORIG_ImpactTask);
		*(uint32_t *)&a->f10 = rd32(&n->f10);
		a->counter = 0;
		*(uint32_t *)&a->f14 = rd32(&n->f14);
		a->f18 = (int16_t)(CrtRand() % 0x800);
		a->f1C = 0x400;
		a->f1E = 0x100;
		Node *f = AddTask(ORIG_FlashTask);
		*(uint32_t *)&f->f10 = rd32(&n->f10);
		*(uint32_t *)&f->f14 = rd32(&n->f14);
		f->counter = 0;
		f->f12 = (int16_t)0xFF38;
		f->f1C = (int16_t)(CrtRand() % 0x900 + 0xC00);
		Node *d = AddTask(ORIG_DustTask);
		d->f0E = (int16_t)(1 << (n->f18 & 31));
		*(uint32_t *)&d->f10 = rd32(&n->f10);
		d->counter = 0;
		*(uint32_t *)&d->f14 = rd32(&n->f14);
		return TASK_END;
	}

	// Ground impact model (0x5A6160)
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_ImpactTask, n);)
		ImpactDraw(n, n->counter >= 0, ImpactFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int32_t)step / 8);
		n->counter++;
		return n->counter > 8 ? TASK_END : 0;
	}

	// Impact flash (0x5A62A0)
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_FlashTask, n);)
		FlashDraw(n, n->counter);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 0x10 ? TASK_END : 0;
	}

	// Dust of a miss (0x5A6330): array A, id & f0E (its shot's bit)
	static uint32_t __cdecl DustTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_DustTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Smoke;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		for (Particle *e = PartsA(); e < PartsA() + 100; e++)
		{
			if (!(e->id & (int32_t)n->f0E)) continue;
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part(e);)
			ParticleDraw(h, e->frame, e->pos, e->angle);
			if (!(UpdateFlags() & 0x201))
			{
				e->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) e->id = 0;
				else
				{
					DustMove(e);
					alive++;
				}
			}
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part_next(e);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int k = 0; k < 10; k++)
			{
				Particle *e = FreeParticle(PartsA());
				if (!e) break;
				e->id = n->f0E;
				e->frame = 0;
				e->angle = (int16_t)(CrtRand() % 0x600 + 0x800);
				*(uint32_t *)&e->pos[0] = rd32(&n->f10);
				*(uint32_t *)&e->pos[2] = rd32(&n->f14);
				const int32_t a = CrtRand() % 0x1000;
				const int32_t c = ComputeCos(a);
				const int32_t s = ComputeSin(a);
				const int32_t r = CrtRand() % 0x32 + 0x32;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, c) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, s) >> 12));
				const int32_t sp = CrtRand() % 0x1E + 0x14;
				e->vel[0] = (int16_t)(mul32(sp, c) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, s) >> 12);
			}
		}
		n->counter++;
		if (n->counter < 0x10 || alive != 0) return 0;
		return TASK_END;
	}

	// Hit (0x5A6580): after f0E ticks, a point near the target: the spark, its flash and sparks
	static uint32_t __cdecl HitTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (n->f0E > 0)
		{
			n->f0E--;
			return 0;
		}
		int32_t v[3];
		v[0] = CrtRand() % 0x1000 - 0x800;
		v[1] = 0;
		v[2] = -(CrtRand() % 0x800);
		TransformVectorBy3x3Matrix((const Mat4x3 *)(Entity(TargetSlot()) + 0x40), v, v);
		NormalizeVector(v, v);
		const int32_t r = CrtRand() % 0xD2 + 0x3C;
		Node *s = AddTask(ORIG_SparkTask);
		s->counter = 0;
		s->f10 = (int16_t)((mul32(r, v[0]) >> 12) + n->f10);
		s->f12 = n->f12;
		s->f14 = (int16_t)((mul32(r, v[2]) >> 12) + n->f14);
		s->f18 = (int16_t)(*(const int16_t *)(Entity(CasterSlot()) + 0xE) + 0x800);
		s->f1A = (int16_t)(CrtRand() % 0x800);
		const int16_t scale = (int16_t)(CrtRand() % 0x360 + 0x440);
		s->f1C = scale;
		s->f1E = (int16_t)((int32_t)scale / 4);
		Node *f = AddTask(ORIG_FlashTask);
		*(uint32_t *)&f->f10 = rd32(&s->f10);
		f->counter = 0;
		*(uint32_t *)&f->f14 = rd32(&s->f14);
		f->f1C = (int16_t)(CrtRand() % 0xC00 + 0x1100);
		Node *g = AddTask(ORIG_SparksTask);
		g->f0E = (int16_t)(1 << (n->f18 & 31));
		g->counter = 0;
		*(uint32_t *)&g->f10 = rd32(&s->f10);
		*(uint32_t *)&g->f14 = rd32(&s->f14);
		return TASK_END;
	}

	// Spark model (0x5A6750)
	static uint32_t __cdecl SparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_SparkTask, n);)
		SparkDraw(n, n->counter >= 1, SparkFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int32_t)step / 8);
		n->counter++;
		return n->counter >= 5 ? TASK_END : 0;
	}

	// Sparks of a hit (0x5A68C0): array B, id & f0E (its shot's bit)
	static uint32_t __cdecl SparksTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_begin(ORIG_SparksTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Sparks;
		*(int16_t *)(h + 0x24) = 1;
		for (Particle *e = PartsB(); e < PartsB() + 100; e++)
		{
			if (!(e->id & (int32_t)n->f0E)) continue;
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part(e);)
			*(int32_t *)(h + 8) = e->f16;
			ParticleDraw(h, e->frame, e->pos, e->angle);
			if (!(UpdateFlags() & 0x201))
			{
				e->frame++;
				if (*(const int16_t *)(h + 0x28) < 0) e->id = 0;
				else
				{
					SparksMove(e);
					alive++;
				}
			}
			// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
			FX_HELD(held_note_part_next(e);)
		}
		FieldFree(0xB4);
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			for (int k = 0; k < 0xC; k++)
			{
				Particle *e = FreeParticle(PartsB());
				if (!e) break;
				e->id = n->f0E;
				e->frame = 0;
				e->angle = (int16_t)(CrtRand() % 0x400 + 0x500);
				*(uint32_t *)&e->pos[0] = rd32(&n->f10);
				*(uint32_t *)&e->pos[2] = rd32(&n->f14);
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(v, v);
				const int32_t r = CrtRand() % 100 + 0x5A;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(r, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x17C + 0x82;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				e->vel[1] = (int16_t)(mul32(sp, v[1]) >> 12);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
				e->f16 = (int16_t)(CrtRand() % 0x800);
			}
		}
		n->counter++;
		if (n->counter < 0xF || alive != 0) return 0;
		return TASK_END;
	}

	// the director's first statements (every tick, also in the draw-only mode): the muzzle = the caster's
	// effect point 0xF0 (flag 0xC00), 100 units ahead along her facing (-sin / -cos of her y rotation)
	static void ComputeMuzzle()
	{
		GetEffectSpawnPosition(Entity(CasterSlot()), 0xF0, 0xC00, Muzzle());
		const int32_t ry = *(const int16_t *)(Entity(CasterSlot()) + 0xE);
		Muzzle()[0] = (int16_t)(Muzzle()[0] + (int16_t)(mul32(ComputeSin(ry), -100) >> 12));
		const int32_t rz = *(const int16_t *)(Entity(CasterSlot()) + 0xE);
		Muzzle()[2] = (int16_t)(Muzzle()[2] + (int16_t)(mul32(ComputeCos(rz), -100) >> 12));
	}

	// Director (0x5A5890; IDA counts it in the setup MAG_305_UNKNOWN 0x5A57D0): node +0xC counter
	static uint32_t __cdecl MasterTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		ComputeMuzzle();
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 1)
		{
			BindEntityModelFromComData(Entity(CasterSlot()), BIND_Block, COM_Anim);
			QueueChainTransformation(Entity(CasterSlot()), 1);
		}
		if (n->counter >= 9 && n->counter <= 0x20 && (n->counter & 1))
		{
			Node *m = AddTask(ORIG_MuzzleTask);
			*(uint32_t *)&m->f10 = rd32(&Muzzle()[0]);
			m->counter = 0;
			*(uint32_t *)&m->f14 = rd32(&Muzzle()[2]);
			m->f18 = *(const int16_t *)(Entity(CasterSlot()) + 0xE);
			m->f1A = (int16_t)(CrtRand() % 0x800);
			m->f1C = (int16_t)(CrtRand() % 0x280 + 0x600);
		}
		if (n->counter == 9) AddTask(ORIG_SmokeTask)->counter = 0;
		if (n->counter == 8)
		{
			// five misses on the line from the muzzle to the target (1/6 .. 5/6 of the way, y 0); the x86
			// multiplies k by the dword under the 16-bit steps (the upper word is the target's y / a word
			// GetDefaultEffectPosition never writes): only the low half is stored
			int16_t t[4];
			GetDefaultEffectPosition(Entity(TargetSlot()), t);
			const int16_t dx = (int16_t)(((int32_t)t[0] - Muzzle()[0]) / 6);
			const int16_t dz = (int16_t)(((int32_t)t[2] - Muzzle()[2]) / 6);
			for (int k = 1; k <= 5; k++)
			{
				Node *a = AddTask(ORIG_MissTask);
				a->counter = 0;
				a->f0E = (int16_t)(2 * k);
				a->f12 = 0;
				a->f10 = (int16_t)(k * dx + Muzzle()[0]);
				a->f18 = (int16_t)k;
				a->f14 = (int16_t)(k * dz + Muzzle()[2]);
			}
		}
		if (n->counter == 0x12)
		{
			// seven hits on the target, rising by 1/8 of its height (+0x36 - +0x3C) each, minus 0..99
			int16_t t[4];
			GetDefaultEffectPosition(Entity(TargetSlot()), t);
			const uint8_t *e = Entity(TargetSlot());
			const int16_t dy = (int16_t)(((int32_t)*(const int16_t *)(e + 0x36) - *(const int16_t *)(e + 0x3C)) / 8);
			for (int k = 1; k <= 7; k++)
			{
				Node *h = AddTask(ORIG_HitTask);
				h->counter = 0;
				h->f0E = (int16_t)(2 * k);
				h->f10 = t[0];
				h->f12 = *(const int16_t *)(Entity(TargetSlot()) + 0x3C);
				const int32_t r = CrtRand() % 100;
				h->f12 = (int16_t)(h->f12 + (int16_t)(k * dy - r));
				h->f18 = (int16_t)k;
				h->f14 = t[2];
			}
		}
		if (n->counter <= 8) SetScreenFlash((uint32_t)(n->counter * 0xC0), 0);
		else if (n->counter >= 0x1C) SetScreenFlash((uint32_t)((0x24 - n->counter) * 0xC0), 0);
		if (n->counter == 0x1E) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (n->counter == 8) BdPlaySE(SOUND_Gun, 0, 0x80);
		n->counter++;
		if (n->counter <= 0x24) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}
	// Root (0x5A6BC0, au_re_BdlinkTask_4)
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(held_note_root();)
		Cursor() = ARENA_A;
		if (!(*(const uint8_t *)&r->counter & 1)) Cursor() = ARENA_B;
		const int a = ExecuteTaskQueue(QEffects());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag305_gatling_gun_quistis()
	{
		register_port(gg305::ORIG_RootTask, (void *)gg305::RootTask, "G305 RootTask", 305);
		register_port(gg305::ORIG_MasterTask, (void *)gg305::MasterTask, "G305 MasterTask", 305);
		register_port(gg305::ORIG_MuzzleTask, (void *)gg305::MuzzleTask, "G305 MuzzleTask", 305);
		register_port(gg305::ORIG_SmokeTask, (void *)gg305::SmokeTask, "G305 SmokeTask", 305);
		register_port(gg305::ORIG_MissTask, (void *)gg305::MissTask, "G305 MissTask", 305);
		register_port(gg305::ORIG_ImpactTask, (void *)gg305::ImpactTask, "G305 ImpactTask", 305);
		register_port(gg305::ORIG_FlashTask, (void *)gg305::FlashTask, "G305 FlashTask", 305);
		register_port(gg305::ORIG_DustTask, (void *)gg305::DustTask, "G305 DustTask", 305);
		register_port(gg305::ORIG_HitTask, (void *)gg305::HitTask, "G305 HitTask", 305);
		register_port(gg305::ORIG_SparkTask, (void *)gg305::SparkTask, "G305 SparkTask", 305);
		register_port(gg305::ORIG_SparksTask, (void *)gg305::SparksTask, "G305 SparksTask", 305);
		// 30 fps layer: see mag305_gatling_gun_quistis_held.inc
		FX_HELD(register_mag305_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag305_gatling_gun_quistis_held.inc"
#endif
