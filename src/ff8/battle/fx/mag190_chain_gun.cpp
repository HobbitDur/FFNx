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

// Effect 190: Chain Gun (enemy attack 154 of kernel.bin, GIM47N; MAG_190_CHAIN_GUN).
//
// Structure (setup MAG_190_CHAIN_GUN 0x5BB2C0, file loader 0x5BB2A0 = the texture file named at
// 0xD19574; the setup keeps the context, the first target's slot and the caster slot, clears both
// particle arrays, starts camera animation 0xD186A0 and queues the TIM). All tasks live in one queue
// 0x2283108 (pool 100 x 0x24):
//   RootTask (0x5BC900) - alternates the packet arena (0x228BF40 on odd counters, else 0x2283F40),
//     runs the effect queue, ends when it is empty.
//   MasterTask (0x5BB380) - every tick (also in draw-only mode) the caster's effect anchor 0xB
//     (muzzle) into 0x2283F38; counter 0: the 12 shots' impact points (BuildShots); 1: sound
//     0xD1956C; 2: the smoke; every odd counter 3..25: a muzzle flash and the next shot (a hit on
//     the target or a miss on the ground next to it); 30: damage; ends after 36.
//   MuzzleTask (0x5BB5A0) - draws THEN updates: prim model 0xD18CDC at the muzzle (drawn twice),
//     caster facing + random roll, random scale, fade counter << 11; ends at 2.
//   SmokeTask (0x5BB6F0) - draws THEN updates every particle of array A (0x22824A0, 100 x 0x18)
//     whose id has bit 0 set: flipbook 0xD1899C (frame++, dies when the flipbook ends), size +0x18,
//     position += velocity, x / z velocity - 1/8. Counters 0..23: three new particles (id 1) near
//     the muzzle, random direction. Ends past counter 15 with no live particle. (The miss dust
//     groups have odd ids too: while the smoke runs it also draws and moves them.)
//   MissTask (0x5BB9B0) / HitTask (0x5BBEC0) - one tick: the impact tasks of one shot.
//     Miss: ImpactTask (model 0xD18FDC on the ground, growing), FlashTask (y -200) and DustTask.
//     Hit: a point near the target (random direction through the target's matrix), SparkTask
//     (model 0xD192A4), FlashTask and SparksTask.
//   ImpactTask (0x5BBAA0) / SparkTask (0x5BC080) - draw THEN update: prim model, scale growing by a
//     step that decays by 1/8, fade from the counter; end at 9 / 5.
//   FlashTask (0x5BBBE0) - flipbook 0xD187A0 at the impact, frame = counter; ends at 16.
//   DustTask (0x5BBC70) - draws THEN updates the array A particles with its shot's id (flipbook
//     0xD1899C, colour 0x404040); counter 0: ten new ones around the impact point.
//   SparksTask (0x5BC1F0) - the same over array B (0x2281B40, flipbook 0xD18B48, velocity - 1/3);
//     counter 0: twelve new ones.
// Every drawing task tests the draw-only flags (battle_to_update_flags 0x201) after drawing.
// Module globals: 0x2281B18..0x2293F44 (target slot, root pool/queue, particle arrays B and A, the
// shot table, effect queue + pool, context, caster slot, texture, muzzle, the two packet arenas,
// packet cursor).

#include "mag_common.h"

namespace ff8fx
{
namespace cg190
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x2281B18); }  // the first target
	inline CastContext *&Ctx() { return var<CastContext *>(0x2283F28); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x2283F2C); }
	inline int16_t *Muzzle() { return (int16_t *)0x2283F38; }            // caster anchor 0xB
	inline uint32_t &Cursor() { return var<uint32_t>(0x2293F40); }
	inline TaskQueue *QEffects() { return (TaskQueue *)0x2283108; }     // pool 0x2283118, 100 x 0x24
	static const uint32_t ARENA_A = 0x228BF40, ARENA_B = 0x2283F40;     // 0x8000 bytes each

	static const uint32_t ORIG_RootTask = 0x5BC900;
	static const uint32_t ORIG_MasterTask = 0x5BB380;
	static const uint32_t ORIG_MuzzleTask = 0x5BB5A0;
	static const uint32_t ORIG_SmokeTask = 0x5BB6F0;
	static const uint32_t ORIG_MissTask = 0x5BB9B0;
	static const uint32_t ORIG_ImpactTask = 0x5BBAA0;
	static const uint32_t ORIG_FlashTask = 0x5BBBE0;
	static const uint32_t ORIG_DustTask = 0x5BBC70;
	static const uint32_t ORIG_HitTask = 0x5BBEC0;
	static const uint32_t ORIG_SparkTask = 0x5BC080;
	static const uint32_t ORIG_SparksTask = 0x5BC1F0;
	static const uint32_t MODEL_Muzzle = 0xD18CDC;
	static const uint32_t MODEL_Impact = 0xD18FDC;
	static const uint32_t MODEL_Spark = 0xD192A4;
	static const uint32_t SEQ_Smoke = 0xD1899C;  // smoke and dust
	static const uint32_t SEQ_Flash = 0xD187A0;
	static const uint32_t SEQ_Sparks = 0xD18B48;
	static const void *const SOUND_Gun = (const void *)0xD1956C;

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone`
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); } // sub_56C220
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }

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
		int16_t f0E;     // +0x0E master: next shot / miss, hit: delay / dust, sparks: shot id
		int16_t f10;     // +0x10 x
		int16_t f12;     // +0x12 y
		int16_t f14;     // +0x14 z
		int16_t f16;     // +0x16 (4th word of the copied position)
		int16_t f18;     // +0x18 yaw / miss, hit: shot id
		int16_t f1A;     // +0x1A roll
		int16_t f1C;     // +0x1C scale
		int16_t f1E;     // +0x1E scale step
		uint8_t pad20[4];
	};
	struct Particle // arrays A 0x22824A0 and B 0x2281B40, 100 x 0x18 each
	{
		int32_t id;      // 0 = free; A: 1 = smoke, else the dust's shot id; B: the sparks' shot id
		int16_t frame;   // flipbook frame
		int16_t angle;   // TransformCameraByShadowRotation angle (size)
		int16_t pos[4];
		int16_t vel[3];
		int16_t f16;     // B: sequence header +8
	};
	struct Shot // 0x2282E08, 48 x 0x10 (12 used)
	{
		int32_t active;
		int32_t kind;    // target slot (hit) or -1 (miss)
		int16_t pos[4];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Chain Gun nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(Shot) == 0x10, "Chain Gun records");

	inline Particle *PartsA() { return (Particle *)0x22824A0; }
	inline Particle *PartsB() { return (Particle *)0x2281B40; }
	inline Shot *Shots() { return (Shot *)0x2282E08; }
	inline int16_t *EntPos(int32_t slot) { return (int16_t *)(Entity(slot) + 0x1C); }
	inline uint32_t rd32(const void *p) { return *(const uint32_t *)p; }
	inline Node *AddTask(uint32_t fn) { return (Node *)AddTaskToQueue(QEffects(), fn); }
}
}

#ifdef FF8_FX_HELD
#include "mag190_chain_gun_held.h"
#endif

namespace ff8fx
{
namespace cg190
{
	// ------------------------------------------------------------------
	// Shot table (0x5BC4F0 and helpers)
	// ------------------------------------------------------------------
	// MAG_190_sub_5BC770: shots start..start+count-1 on the target in slot: odd shots hit, even
	// ones miss (the target's position) half of the time; a hit is a random effect anchor of the
	// target (anchor count = byte at **(entity +0x64)) at a random angle
	static void FillShots(int32_t start, int32_t count, int32_t slot)
	{
		const int32_t end = start + count;
		for (int32_t i = start; i < end; i++)
		{
			Shot *s = &Shots()[i];
			s->active = 1;
			if (!(i & 1) && CrtRand() % 2 == 0)
			{
				s->kind = -1;
				*(uint32_t *)&s->pos[0] = *(const uint32_t *)(Entity(slot) + 0x1C);
				*(uint32_t *)&s->pos[2] = *(const uint32_t *)(Entity(slot) + 0x20);
				continue;
			}
			s->kind = slot;
			int32_t bone = 0;
			const uint8_t anchors = ***(const uint8_t ***)(Entity(slot) + 0x64);
			if (anchors) bone = CrtRand() % anchors;
			const int32_t angle = CrtRand() % 0x1000;
			GetEffectSpawnPosition(Entity(slot), bone, angle, s->pos);
		}
	}

	// MAG_190_sub_5BC850: shots start..start+count-1 = misses evenly spaced from a to b (x / z,
	// y 0), a and b excluded
	static void LineShots(int32_t start, int32_t count, const int16_t *a, const int16_t *b)
	{
		const int16_t ax = a[0];
		const int16_t dx = (int16_t)(((int32_t)b[0] - ax) / (count + 1));
		const int16_t az = a[2];
		const int16_t dz = (int16_t)(((int32_t)b[2] - az) / (count + 1));
		const int32_t end = start + count;
		// (the x86 multiplies by the dword under the 16-bit dx / dz: only the low half is stored)
		for (int32_t k = 1; start + k - 1 < end; k++)
		{
			Shot *s = &Shots()[start + k - 1];
			s->active = 1;
			s->kind = -1;
			s->pos[0] = (int16_t)(k * dx + ax);
			s->pos[1] = 0;
			s->pos[2] = (int16_t)(k * dz + az);
		}
	}

	// MAG_190_sub_5BC4F0: the 12 shots, sweeping across the targets (by slot order)
	static void BuildShots()
	{
		for (Shot *s = Shots(); s < Shots() + 48; s++) s->active = 0;
		const ActionData *act = Ctx()->actions;
		const uint8_t *t = act->targets;
		switch (act->target_count)
		{
		case 1:
			FillShots(0, 0xC, t[0]);
			return;
		case 2:
		{
			int32_t lo = t[0], hi = t[TARGET_STRIDE];
			if (hi < lo) { const int32_t x = lo; lo = hi; hi = x; }
			switch ((1 << hi) | (1 << lo))
			{
			case 3:
				FillShots(0, 5, hi);
				LineShots(5, 2, EntPos(hi), EntPos(lo));
				FillShots(7, 5, lo);
				break;
			case 5:
			{
				int16_t mid[4];
				mid[0] = (int16_t)(((int32_t)EntPos(lo)[0] + EntPos(hi)[0]) / 2);
				mid[1] = 0;
				mid[2] = (int16_t)(((int32_t)EntPos(lo)[2] + EntPos(hi)[2]) / 2);
				mid[3] = 0; // never written by the original (not read)
				LineShots(0, 2, mid, EntPos(hi));
				FillShots(2, 3, hi);
				LineShots(5, 4, EntPos(hi), EntPos(lo));
				FillShots(9, 3, lo);
				break;
			}
			case 6:
				FillShots(0, 2, lo);
				LineShots(2, 2, EntPos(lo), EntPos(hi));
				FillShots(4, 3, hi);
				LineShots(7, 2, EntPos(hi), EntPos(lo));
				FillShots(9, 3, lo);
				break;
			}
			// the original falls through to the three-target code (0x5BC6A3): it reads a third
			// target record and rebuilds all 12 shots
			break;
		}
		case 3:
			break;
		default:
			return;
		}
		int32_t lo = t[0], mid = t[TARGET_STRIDE], hi = t[2 * TARGET_STRIDE];
		if (mid < lo) { const int32_t x = lo; lo = mid; mid = x; }
		if (hi < mid) { const int32_t x = mid; mid = hi; hi = x; }
		if (mid < lo) { const int32_t x = lo; lo = mid; mid = x; }
		FillShots(0, 2, mid);
		LineShots(2, 2, EntPos(mid), EntPos(hi));
		FillShots(4, 3, hi);
		LineShots(7, 2, EntPos(hi), EntPos(lo));
		FillShots(9, 3, lo);
	}

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
	// Muzzle flash (0x5BB5A0)
	static uint32_t __cdecl MuzzleTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_MuzzleTask, n);)
		MuzzleDraw(n, n->counter >= 0, MuzzleFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 2 ? TASK_END : 0;
	}

	// Smoke (0x5BB6F0): array A, id bit 0
	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_SmokeTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Smoke;
		*(int16_t *)(h + 0x24) = 0;
		for (Particle *e = PartsA(); e < PartsA() + 100; e++)
		{
			if (!(*(const uint8_t *)&e->id & 1)) continue;
			// 30 fps layer: see mag190_chain_gun_held.inc
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
			// 30 fps layer: see mag190_chain_gun_held.inc
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
				e->angle = (int16_t)(CrtRand() % 0x600 + 0x800);
				*(uint32_t *)&e->pos[0] = rd32(&Muzzle()[0]);
				*(uint32_t *)&e->pos[2] = rd32(&Muzzle()[2]);
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(v, v);
				const int32_t r = CrtRand() % 200;
				e->pos[0] = (int16_t)(e->pos[0] + (mul32(r, v[0]) >> 12));
				e->pos[1] = (int16_t)(e->pos[1] + (mul32(r, v[1]) >> 12));
				e->pos[2] = (int16_t)(e->pos[2] + (mul32(r, v[2]) >> 12));
				const int32_t sp = CrtRand() % 0x50 + 0x1E;
				e->vel[0] = (int16_t)(mul32(sp, v[0]) >> 12);
				const int32_t up = CrtRand() % 0x28;
				e->vel[1] = (int16_t)(-15 - up);
				e->vel[2] = (int16_t)(mul32(sp, v[2]) >> 12);
			}
		}
		n->counter++;
		if (n->counter < 0x10 || alive != 0) return 0;
		return TASK_END;
	}

	// Miss (0x5BB9B0): after f0E ticks, the ground impact, its flash and its dust
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
		d->f0E = n->f18;
		*(uint32_t *)&d->f10 = rd32(&n->f10);
		d->counter = 0;
		*(uint32_t *)&d->f14 = rd32(&n->f14);
		return TASK_END;
	}

	// Ground impact model (0x5BBAA0)
	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_ImpactTask, n);)
		ImpactDraw(n, n->counter >= 0, ImpactFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int32_t)step / 8);
		n->counter++;
		return n->counter > 8 ? TASK_END : 0;
	}

	// Impact flash (0x5BBBE0)
	static uint32_t __cdecl FlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_FlashTask, n);)
		FlashDraw(n, n->counter);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 0x10 ? TASK_END : 0;
	}

	// Dust of a miss (0x5BBC70): array A, id = f0E
	static uint32_t __cdecl DustTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_DustTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Smoke;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		for (Particle *e = PartsA(); e < PartsA() + 100; e++)
		{
			if (e->id != (int32_t)n->f0E) continue;
			// 30 fps layer: see mag190_chain_gun_held.inc
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
			// 30 fps layer: see mag190_chain_gun_held.inc
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

	// Hit (0x5BBEC0): after f0E ticks, a point near the target: the spark, its flash and sparks
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
		g->f0E = n->f18;
		g->counter = 0;
		*(uint32_t *)&g->f10 = rd32(&s->f10);
		*(uint32_t *)&g->f14 = rd32(&s->f14);
		return TASK_END;
	}

	// Spark model (0x5BC080)
	static uint32_t __cdecl SparkTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_SparkTask, n);)
		SparkDraw(n, n->counter >= 1, SparkFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int32_t)step / 8);
		n->counter++;
		return n->counter >= 5 ? TASK_END : 0;
	}

	// Sparks of a hit (0x5BC1F0): array B, id = f0E
	static uint32_t __cdecl SparksTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_begin(ORIG_SparksTask, n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Sparks;
		*(int16_t *)(h + 0x24) = 1;
		for (Particle *e = PartsB(); e < PartsB() + 100; e++)
		{
			if (e->id != (int32_t)n->f0E) continue;
			// 30 fps layer: see mag190_chain_gun_held.inc
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
			// 30 fps layer: see mag190_chain_gun_held.inc
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

	static uint32_t __cdecl MasterTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		GetEffectSpawnPosition(Entity(CasterSlot()), 0xB, 0x1800, Muzzle());
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			BuildShots();
			n->f0E = 0;
		}
		if (n->counter >= 2 && n->counter <= 0x19 && (n->counter & 1))
		{
			Node *m = AddTask(ORIG_MuzzleTask);
			*(uint32_t *)&m->f10 = rd32(&Muzzle()[0]);
			m->counter = 0;
			*(uint32_t *)&m->f14 = rd32(&Muzzle()[2]);
			m->f18 = *(const int16_t *)(Entity(CasterSlot()) + 0xE);
			m->f1A = (int16_t)(CrtRand() % 0x800);
			m->f1C = (int16_t)(CrtRand() % 0x500 + 0xA00);
			const int32_t i = n->f0E;
			const Shot *s = &Shots()[i];
			if (s->active)
			{
				if (s->kind >= 0)
				{
					Node *t = AddTask(ORIG_HitTask);
					*(uint32_t *)&t->f10 = rd32(&s->pos[0]);
					t->counter = 0;
					t->f0E = 0;
					*(uint32_t *)&t->f14 = rd32(&s->pos[2]);
					t->f18 = (int16_t)(i + 1);
				}
				else
				{
					Node *t = AddTask(ORIG_MissTask);
					t->counter = 0;
					t->f0E = 0;
					*(uint32_t *)&t->f10 = rd32(&s->pos[0]);
					*(uint32_t *)&t->f14 = rd32(&s->pos[2]);
					t->f10 = (int16_t)(t->f10 + (CrtRand() % 1000 - 500));
					t->f14 = (int16_t)(t->f14 + (CrtRand() % 1000 - 500));
					t->f18 = (int16_t)(i + 1);
				}
				n->f0E++;
			}
		}
		if (n->counter == 2) AddTask(ORIG_SmokeTask)->counter = 0;
		if (n->counter == 0x1E) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		if (n->counter == 1) BdPlaySE(SOUND_Gun, 0, 0x80);
		n->counter++;
		return n->counter > 0x24 ? TASK_END : 0;
	}

	// Root (0x5BC900, au_re_BdlinkTask_4)
	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(held_note_root();)
		Cursor() = ARENA_A;
		if (!(*(const uint8_t *)&r->counter & 1)) Cursor() = ARENA_B;
		const int a = ExecuteTaskQueue(QEffects());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag190_chain_gun()
	{
		register_port(cg190::ORIG_RootTask, (void *)cg190::RootTask, "C190 RootTask", 190);
		register_port(cg190::ORIG_MasterTask, (void *)cg190::MasterTask, "C190 MasterTask", 190);
		register_port(cg190::ORIG_MuzzleTask, (void *)cg190::MuzzleTask, "C190 MuzzleTask", 190);
		register_port(cg190::ORIG_SmokeTask, (void *)cg190::SmokeTask, "C190 SmokeTask", 190);
		register_port(cg190::ORIG_MissTask, (void *)cg190::MissTask, "C190 MissTask", 190);
		register_port(cg190::ORIG_ImpactTask, (void *)cg190::ImpactTask, "C190 ImpactTask", 190);
		register_port(cg190::ORIG_FlashTask, (void *)cg190::FlashTask, "C190 FlashTask", 190);
		register_port(cg190::ORIG_DustTask, (void *)cg190::DustTask, "C190 DustTask", 190);
		register_port(cg190::ORIG_HitTask, (void *)cg190::HitTask, "C190 HitTask", 190);
		register_port(cg190::ORIG_SparkTask, (void *)cg190::SparkTask, "C190 SparkTask", 190);
		register_port(cg190::ORIG_SparksTask, (void *)cg190::SparksTask, "C190 SparksTask", 190);
		// 30 fps layer: see mag190_chain_gun_held.inc
		FX_HELD(register_mag190_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag190_chain_gun_held.inc"
#endif
