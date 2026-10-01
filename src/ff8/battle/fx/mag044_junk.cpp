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

// Effect 44: Junk (enemy attack 226 of kernel.bin, used by Tonberry King c0m083; MAG_044_*): junk
// falls from the sky onto the party, bounces and breaks into debris.
//
// Structure (setup MAG_044_JUNK 0x6E7AE0 -> _Init 0x6E7B10, not ported; file loader 0x6E7AF0 = the
// texture file named at 0x13627D8; the setup queues the TIM, keeps the cast context 0x2545104 and
// the CASTER's entity 0x2545108, starts camera animation 0x1354A30 and the root task):
//   RootTask (0x6E7B90) - alternates the packet arena (magic buffer + 0x12D4 / + 0x112D4); at
//     counter 1 sets up four pools in the magic buffer (debris 32 x 0x58 at + 0, items 16 x 0x34 at
//     + 0xB00, smoke 32 x 0x20 at + 0xE40, one controller of 0x4C at + 0x1240) and the controller
//     (targets of action 0, their saved positions and their average default effect position, a
//     random word, the stream state + 0x128C); runs the controller / smoke / item / debris queues
//     until counter 0x72; at 0x73 loads the battle files 0x168 / 0x169 to + 0x12D4, pumps the
//     loader every tick and ends when it is done.
//   ControlTask (0x6E7E00) - sound 0x1354B7C at 0; smoke bursts at the caster's anchors 0x17 / 0x15
//     on counters 1, 8, 15, 22, 29 (word 0x1D97712 = 150 each time); counters 3..37: the targets
//     bounce (entity +0x1E = saved y - amplitude * sin, a 7-tick cycle with a new random amplitude,
//     at its end a smoke burst at the target and chain transformation 4); counters 15..46 every 3
//     ticks: an item of kind (random + n / 4) % 6 around the targets' average position; counter 62:
//     the big item (kind 6) above every target; ends at 100 (target positions restored).
//   SmokeTask (0x6E82C0) - a smoke sprite (sequence 0x1354B80, frame = 13 - life) through
//     TransformCameraByShadowRotation, draws THEN drifts by its velocity; 13 ticks.
//   ItemTask (0x6E84D0) - a junk model (kind table 0x1362730, 24 bytes: models, model count,
//     smoke count / spread / base) drawn by Effect_RenderPrimModel 0x572200 (fading out over its
//     last 3 ticks), THEN falls (gravity 0x32); at the floor it bounces (speed -1/3): on its target
//     (the big item) debris (8 x 0x6E8810), the damage of that target, then a slow drift; on the
//     floor a smoke burst.
//   DebrisTask (0x6E8810) - after a delay, the prim-model layout 0x1354CE0 played (0x701970) at a
//     fixed position with a turning Y rotation (local copy 0x6E8A70 of 0x701270); PartCallback
//     (0x6E88B0) draws one object (0x701310 rotation, vertex frames blended into magic buffer +
//     0x212D4, scale, colour fade) through 0x572200.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2545100..0x2545184 (texture file, context, caster entity, the five queues, root
// pool 0x2545160, packet cursor 0x2545174, magic buffer base 0x2545180).

#include "mag_common.h"

namespace ff8fx
{
namespace junk044
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2545104); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2545108); }          // caster entity (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2545174); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2545180); }           // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QDebris() { return (TaskQueue *)0x2545110; }
	inline TaskQueue *QItems() { return (TaskQueue *)0x2545120; }
	inline TaskQueue *QSmoke() { return (TaskQueue *)0x2545130; }
	inline TaskQueue *QControl() { return (TaskQueue *)0x2545140; }
	inline int16_t &BattleWord1D97712() { return var<int16_t>(0x1D97712); }   // set to 150 with every smoke wave

	static const uint32_t ORIG_RootTask = 0x6E7B90;
	static const uint32_t ORIG_ControlTask = 0x6E7E00;
	static const uint32_t ORIG_SmokeTask = 0x6E82C0;
	static const uint32_t ORIG_ItemTask = 0x6E84D0;
	static const uint32_t ORIG_DebrisTask = 0x6E8810;
	static const void *const SOUND_Junk = (const void *)0x1354B7C;
	static const uint32_t SEQ_Smoke = 0x1354B80;      // smoke sprite sequence (+8: frame count word)
	static const uint32_t MODEL_Debris = 0x1354CE0;   // prim-model layout data (0x40-byte layout)
	static const uint32_t ITEM_KINDS = 0x1362730;     // 7 x 24 bytes

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// battle file streaming (0x5341D0 load a file, 0x534210 pump, 0x5342C0 still busy)
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t n) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, n); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamBusy(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct ControlTarget // 16 bytes
	{
		uint8_t *entity;   // +0x00
		uint32_t save1c;   // +0x04 entity +0x1C (x, y) at the start
		uint32_t save20;   // +0x08 entity +0x20 (z, +0x22)
		int16_t amp;       // +0x0C bounce amplitude
		int16_t pad;
	};
	struct ControlNode // pool of 1 node of 0x4C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t rnd;       // +0x10 random word (item kinds)
		int16_t pad12;
		ControlTarget tg[3]; // +0x14
		int16_t avg[3];    // +0x44 average default effect position of the targets
		int16_t pad4A;
	};
	struct SmokeNode // pool of 32 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t life;      // +0x12 13..1
		int16_t vel[3];    // +0x14
		int16_t pad1A;
		int16_t rot;       // +0x1C
		int16_t pad1E;
	};
	struct ItemNode // pool of 16 nodes of 0x34 bytes
	{
		TaskNode hdr;
		int16_t x, y, z;   // +0x0C
		int16_t life;      // +0x12
		int16_t vx, vy, vz; // +0x14
		int16_t pad1A;
		int16_t rot[3];    // +0x1C
		int16_t pad22;
		int16_t drot[2];   // +0x24 rotation x / y per tick
		uint8_t pad28[4];
		const uint8_t *kind; // +0x2C kind (ITEM_KINDS entry)
		uint8_t *entity;   // +0x30 target hit on the first bounce (0 after it)
	};
	struct DebrisNode // pool of 32 nodes of 0x58 bytes
	{
		TaskNode hdr;
		int16_t x, y, z;   // +0x0C
		int16_t delay;     // +0x12 ticks before the first draw
		int16_t angle;     // +0x14 Y rotation
		int16_t speed;     // +0x16 per tick
		uint8_t layout[0x40]; // +0x18 prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the debris task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 camera * Y rotation at the debris position
		uint32_t morph;    // +0x20 blended vertex frames (magic buffer + 0x212D4)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(ControlNode) == 0x4C && sizeof(SmokeNode) == 0x20, "Junk nodes");
	static_assert(sizeof(ItemNode) == 0x34 && sizeof(DebrisNode) == 0x58 && sizeof(PrimArg) == 0x24, "Junk nodes");

	// ------------------------------------------------------------------
	// Module helpers
	// ------------------------------------------------------------------
	// 0x6E8A70 (copy of MAG_063_sub_701270): rotation about Y by -angle (3x3 and pad; translation not written)
	static void RotationY(int16_t angle, Mat4x3 *out)
	{
		const int32_t a = -(int32_t)angle;
		uint32_t *w = (uint32_t *)out;
		w[0] = 0; w[1] = 0; w[2] = 0; w[3] = 0; w[4] = 0;
		const int32_t s = ComputeSin(a);
		const int32_t c = ComputeCos(a);
		int16_t *m = (int16_t *)out;
		m[6] = (int16_t)s;
		m[2] = (int16_t)-s;
		m[0] = (int16_t)c;
		m[4] = 0x1000;
		m[8] = (int16_t)c;
	}

	// 0x6E87A0: the damage of every target record of action 0 that is this entity
	static void DamageEntity(uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)t[0] == slot) ApplyActionResultToTarget(t);
		}
	}

	// smoke burst: count smoke tasks on a circle of `radius` around (x, z), rising, life 13
	static void SmokeOne(SmokeNode *s, int16_t x, int16_t z, int32_t radius, int32_t spread, int32_t base)
	{
		const int32_t a = CrtRand();
		s->vel[0] = (int16_t)(ComputeCos(a) * radius / 4096);
		const int32_t sz = ComputeSin(a) * radius / 4096;
		s->vel[2] = (int16_t)sz;
		s->pos[0] = (int16_t)(x + s->vel[0]);
		s->pos[1] = 0;
		s->pos[2] = (int16_t)(z + sz);
		s->life = 0xD;
		s->vel[0] = (int16_t)(s->vel[0] / 8);
		s->vel[2] = (int16_t)((int16_t)sz / 8);
		s->vel[1] = (int16_t)-(radius / 8);
		s->rot = (int16_t)(CrtRand() * spread / 32768 + base);
	}

	// 0x6E81A0: smoke burst at the caster's effect anchor `bone`
	static void SmokeAtCaster(int32_t count, int32_t bone, int32_t radius, int32_t spread, int32_t base)
	{
		int16_t pos[4];
		GetEffectSpawnPosition(Caster(), bone, 0, pos);
		while (count-- != 0)
		{
			SmokeNode *s = (SmokeNode *)AddTaskToQueue(QSmoke(), ORIG_SmokeTask);
			if (!s) break;
			SmokeOne(s, pos[0], pos[2], radius, spread, base);
		}
	}

	// 0x6E8360: smoke burst around pos (x = pos[0], z = pos[2])
	static void SmokeAt(int32_t count, const int16_t *pos, int32_t radius, int32_t spread, int32_t base)
	{
		while (count-- != 0)
		{
			SmokeNode *s = (SmokeNode *)AddTaskToQueue(QSmoke(), ORIG_SmokeTask);
			if (!s) break;
			SmokeOne(s, pos[0], pos[2], radius, spread, base);
		}
	}

	// 0x6E8470: a falling item of `kind` at (x, -4096, z), life 20
	static ItemNode *NewItem(int32_t x, int32_t z, uint8_t *entity, int32_t kind)
	{
		ItemNode *it = (ItemNode *)AddTaskToQueue(QItems(), ORIG_ItemTask);
		if (!it) return nullptr;
		it->y = (int16_t)0xF000;
		it->life = 0x14;
		it->vz = 0;
		it->vx = 0;
		it->vy = 0x320;
		it->kind = (const uint8_t *)(ITEM_KINDS + kind * 24);
		it->entity = entity;
		it->x = (int16_t)x;
		it->z = (int16_t)z;
		return it;
	}

	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	// smoke sprite (0x6E82C0 draw part)
	static void DrawSmoke(const SmokeNode *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(s->pos, (int32_t)s->rot, -((int32_t)s->rot >> 2));
		*(uint32_t *)h = SEQ_Smoke;
		*(int16_t *)(h + 0x24) = 0;
		*(int16_t *)(h + 4) = (int16_t)(*(const uint16_t *)(SEQ_Smoke + 8) - s->life);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// item model (0x6E84D0 draw part): matrix 0x78 header (+0x20 = prim-model header)
	static void DrawItem(const ItemNode *it)
	{
		const uint8_t *kind = it->kind;
		uint8_t *b = (uint8_t *)FieldAlloc(0x78);
		Mat4x3 *m = (Mat4x3 *)b;
		ComposeZYXRotationMatrix(it->rot, m);
		m->t[0] = it->x;
		m->t[1] = it->y;
		m->t[2] = it->z;
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		uint8_t *h = b + 0x20;
		if (it->life < 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xC3;
			*(uint32_t *)(h + 8) = 0;
			*(int32_t *)(h + 0xC) = (4 - (int32_t)it->life) << 10;
		}
		else *(uint32_t *)(h + 0x1C) = 0;
		const int32_t k = (int32_t)it->life % (int32_t)*(const int16_t *)(kind + 0x10);
		*(uint32_t *)h = *(const uint32_t *)(kind + k * 4);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x78);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6E88B0): one debris object
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		m.t[0] = r->pos[0];
		m.t[1] = r->pos[1];
		m.t[2] = r->pos[2];
		ComposeAffineTransform(&arg->m, &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2000;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20C0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		// +0x18 (depth offset) is not written: the scratch word under the header
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// debris arg block (0x6E8810): Y rotation by angle at (x, y, z), through the camera
	static void DebrisMatrix(const DebrisNode *d, int16_t angle, PrimArg *arg)
	{
		RotationY(angle, &arg->m);
		arg->m.t[0] = d->x;
		arg->m.t[1] = d->y;
		arg->m.t[2] = d->z;
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag044_junk_held.h"
#endif

namespace ff8fx
{
namespace junk044
{
	// ------------------------------------------------------------------
	// Debris (0x6E8810)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		if (d->delay <= 0)
		{
			PrimArg arg;
			DebrisMatrix(d, d->angle, &arg);
			arg.morph = TexBase() + 0x212D4;
			// 30 fps layer: see mag044_junk_held.inc
			FX_HELD(held_note_debris(d, &arg);)
			if (prim::play((prim::Layout *)d->layout, PartCallback, (int)&arg, 0) == 0) return TASK_END;
		}
		d->angle = (int16_t)(d->angle + d->speed);
		d->delay--;
		return 0;
	}

	// ------------------------------------------------------------------
	// Item (0x6E84D0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ItemTask(TaskNode *n)
	{
		ItemNode *it = (ItemNode *)n;
		// 30 fps layer: see mag044_junk_held.inc
		FX_HELD(held_note_item(it);)
		DrawItem(it);
		it->life--;
		if (it->life == 0) return TASK_END;
		const int16_t vx = it->vx;
		it->vy = (int16_t)(it->vy + 0x32);
		const int16_t vy = it->vy;
		it->x = (int16_t)(it->x + vx);
		it->y = (int16_t)(it->y + vy);
		const int32_t floor = it->entity ? *(const int16_t *)(it->entity + 0x92) : 0;
		if ((int32_t)it->y >= floor)
		{
			it->y = (int16_t)(it->y - vy);
			// vy * -1/3 (0x55555555 magic, truncated toward zero)
			int32_t q = (int32_t)(((int64_t)0x55555555 * (int32_t)vy) >> 32);
			q -= (int32_t)vy;
			q >>= 1;
			q += (int32_t)((uint32_t)q >> 31);
			it->vy = (int16_t)q;
			it->drot[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			if (it->entity)
			{
				it->y = (int16_t)floor;
				int16_t pos[4];
				GetDefaultEffectPosition(it->entity, pos);
				int32_t a = CrtRand();
				int32_t count = 8;
				DebrisNode *d = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
				while (d)
				{
					d->x = (int16_t)(ComputeSin(a) / 16 + pos[0]);
					d->z = (int16_t)(ComputeCos(a) / 16 + pos[2]);
					const uint8_t *e = it->entity;
					const int32_t r = CrtRand();
					d->y = (int16_t)((int16_t)(*(const int16_t *)(e + 0x92) + *(const int16_t *)(e + 0x1E)) - (r & 0xFF) - 0x80);
					d->delay = (int16_t)(count + 10);
					d->angle = (int16_t)CrtRand();
					d->speed = (int16_t)((CrtRand() & 0xFF) + 0x100);
					if (CrtRand() < 0x4000) d->speed = (int16_t)-d->speed;
					DecodeModelPrimLayout(MODEL_Debris, d->layout, 0x40);
					a = a + (CrtRand() & 0xFF) + 0x180;
					if (--count == 0) break;
					d = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
				}
				DamageEntity(it->entity);
				it->vz = 0x50;
				it->entity = nullptr;
			}
			else
			{
				const int32_t c = *(const int16_t *)(it->kind + 0x12);
				SmokeAt(c, &it->x, c * 200, *(const int16_t *)(it->kind + 0x14), *(const int16_t *)(it->kind + 0x16));
			}
		}
		it->z = (int16_t)(it->z + it->vz);
		it->rot[0] = (int16_t)(it->rot[0] + it->drot[0]);
		it->rot[1] = (int16_t)(it->rot[1] + it->drot[1]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Smoke (0x6E82C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SmokeTask(TaskNode *n)
	{
		SmokeNode *s = (SmokeNode *)n;
		// 30 fps layer: see mag044_junk_held.inc
		FX_HELD(held_note_smoke(s);)
		DrawSmoke(s);
		s->life--;
		if (s->life == 0) return TASK_END;
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Controller (0x6E7E00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ControlTask(TaskNode *n)
	{
		ControlNode *c = (ControlNode *)n;
		if (c->counter == 0) BdPlaySE(SOUND_Junk, 0, 0x80);
		switch ((uint32_t)((int32_t)c->counter - 1))
		{
		case 0:
			SmokeAtCaster(3, 0x17, 0xC8, 0x7D0, 0x7D0);
			SmokeAtCaster(3, 0x15, 0xC8, 0x7D0, 0x7D0);
			BattleWord1D97712() = 0x96;
			break;
		case 7: case 21:
			SmokeAtCaster(3, 0x15, 0xC8, 0x7D0, 0x7D0);
			BattleWord1D97712() = 0x96;
			break;
		case 14: case 28:
			SmokeAtCaster(3, 0x17, 0xC8, 0x7D0, 0x7D0);
			BattleWord1D97712() = 0x96;
			break;
		default:
			break;
		}
		// the targets bounce: counters 3..37, a cycle of 7 ticks
		const uint32_t b = (uint32_t)((int32_t)c->counter - 3);
		if (b < 0x23)
		{
			const uint32_t phase = b % 7;
			if (phase == 0)
			{
				for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
					c->tg[i].amp = (int16_t)((CrtRand() & 0x1FF) + 0x100);
			}
			const int32_t s = ComputeSin((int32_t)(phase << 11) / 7 + 0x124);
			for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
			{
				uint8_t *e = c->tg[i].entity;
				*(int16_t *)(e + 0x1E) = (int16_t)((int16_t)(c->tg[i].save1c >> 16) - (int16_t)(mul32(c->tg[i].amp, s) / 4096));
				if (phase == 6)
				{
					SmokeAt(3, (const int16_t *)(e + 0x1C), 0x12C, 0x7D0, 0x7D0);
					QueueChainTransformation(e, 4);
				}
			}
		}
		// items: counters 15..46, every 3 ticks
		const uint32_t f = (uint32_t)((int32_t)c->counter - 0xF);
		if (f < 0x20 && f % 3 == 0)
		{
			const uint32_t kind = ((uint32_t)(int32_t)c->rnd + (f >> 2)) % 6;
			const int32_t rz = CrtRand();
			const int32_t z = (rz & 0x7FF) + c->avg[2] - 0x400;
			const int32_t rx = CrtRand();
			const int32_t x = (rx & 0xFFF) + c->avg[0] - 0x800;
			ItemNode *it = NewItem(x, z, nullptr, (int32_t)kind);
			if (it)
			{
				it->rot[0] = (int16_t)(((CrtRand() * 800) >> 15) - 0x190);
				it->rot[1] = (int16_t)CrtRand();
				it->rot[2] = 0;
				it->drot[0] = (int16_t)((CrtRand() & 0xFF) - 0x80);
				it->drot[1] = (int16_t)((CrtRand() & 0xFF) - 0x80);
			}
		}
		// the big item above every target
		if (c->counter == 0x3E)
		{
			for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
			{
				uint8_t *e = c->tg[i].entity;
				int16_t pos[4];
				GetDefaultEffectPosition(e, pos);
				ItemNode *it = NewItem(pos[0], pos[2], e, 6);
				if (it)
				{
					it->life = 0x18;
					it->rot[0] = (int16_t)((CrtRand() & 0x1FF) - 0x500);
					it->rot[1] = (int16_t)CrtRand();
					it->rot[2] = 0;
					it->drot[0] = 0;
					it->drot[1] = 0;
				}
			}
		}
		c->counter++;
		if (c->counter >= 0x64)
		{
			for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
			{
				uint8_t *e = c->tg[i].entity;
				*(uint32_t *)(e + 0x1C) = c->tg[i].save1c;
				*(uint32_t *)(e + 0x20) = c->tg[i].save20;
			}
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6E7B90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag044_junk_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x12D4;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x112D4;
			r->arena = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QDebris(), (void *)TexBase(), 0x58, 0x20);
			InitTaskQueuePool(QItems(), (void *)(TexBase() + 0xB00), 0x34, 0x10);
			InitTaskQueuePool(QSmoke(), (void *)(TexBase() + 0xE40), 0x20, 0x20);
			InitTaskQueuePool(QControl(), (void *)(TexBase() + 0x1240), 0x4C, 1);
			ControlNode *c = (ControlNode *)AddTaskToQueue(QControl(), ORIG_ControlTask);
			Memset32(&c->counter, 0, 0x10);
			c->avg[2] = 0;
			c->avg[1] = 0;
			c->avg[0] = 0;
			const int32_t count = Ctx()->actions[0].target_count;
			for (int32_t i = 0; i < count; i++)
			{
				uint8_t *e = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
				c->tg[i].entity = e;
				c->tg[i].save1c = *(const uint32_t *)(e + 0x1C);
				c->tg[i].save20 = *(const uint32_t *)(e + 0x20);
				int16_t pos[4];
				GetDefaultEffectPosition(e, pos);
				c->avg[0] = (int16_t)(c->avg[0] + pos[0]);
				c->avg[1] = (int16_t)(c->avg[1] + pos[1]);
				c->avg[2] = (int16_t)(c->avg[2] + pos[2]);
			}
			c->avg[0] = (int16_t)((int32_t)c->avg[0] / count);
			c->avg[1] = (int16_t)((int32_t)c->avg[1] / count);
			c->avg[2] = (int16_t)((int32_t)c->avg[2] / count);
			c->rnd = (int16_t)CrtRand();
			StreamStateInit((void *)(TexBase() + 0x128C));
		}
		if (r->started && r->counter < 0x72)
		{
			ExecuteTaskQueue(QControl());
			ExecuteTaskQueue(QSmoke());
			ExecuteTaskQueue(QItems());
			ExecuteTaskQueue(QDebris());
		}
		if (r->counter == 0x73)
		{
			StreamLoad(0x168, TexBase() + 0x12D4, 1);
			StreamLoad(0x169, TexBase() + 0x12D4, 0);
		}
		if (r->started) StreamPump();
		if (r->counter > 0x73) return StreamBusy(1) != 0 ? TASK_END : 0;
		r->counter++;
		return 0;
	}
}

	void register_mag044_junk()
	{
		register_port(junk044::ORIG_RootTask, (void *)junk044::RootTask, "J044 RootTask", 44);
		register_port(junk044::ORIG_ControlTask, (void *)junk044::ControlTask, "J044 ControlTask", 44);
		register_port(junk044::ORIG_SmokeTask, (void *)junk044::SmokeTask, "J044 SmokeTask", 44);
		register_port(junk044::ORIG_ItemTask, (void *)junk044::ItemTask, "J044 ItemTask", 44);
		register_port(junk044::ORIG_DebrisTask, (void *)junk044::DebrisTask, "J044 DebrisTask", 44);
		// 30 fps layer: see mag044_junk_held.inc
		FX_HELD(register_mag044_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag044_junk_held.inc"
#endif
