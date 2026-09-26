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

// Effect 104: Blizzaga (spell, MAG_104_*).
//
// Structure (setup MAG_104_BLIZZAGA 0x6D6310 -> Init 0x6D6320, file loader 0x6D62F0 = mag103.tim):
//   Director (0x6D6400) - the root task: alternates the packet arena (magic buffer + 0x18928 /
//     + 0x28928), resets the screen flash level, at its counter 1 (a 50-tick cycle) builds six
//     task pools in the magic buffer and starts the next action's freeze on its first target
//     (at most 2 at a time, one per target); runs the six queues, sets the screen flash and ends
//     when they are all empty.
//   Freeze (0x6D67E0, 0x6428-byte node, one per target): the target model is drawn frozen from
//     the bottom up (8..68: its triangles within a rising band get the ice texture, the target's
//     own draw is hidden), ribbons (0..2) and spiral ribbons (0..1) swirl around it, an ice block
//     model (copy of 0x1312918) grows around it (28..30 vertex morph, 31 shape), 33..97 the block
//     breaks from the bottom into 320 falling, spinning, fading shards; a ring (46..70) and a
//     flash model (31..40), mist (28..43) and flakes (38..68); sound at 0, damage at 68, ends
//     after 98. The screen flash rises to 0x400 over 0..3 and falls over 95..98.
//   Ribbon (0x6D97A0) and Spiral (0x6DA0D0) - trails of 30 positions drawn as Gouraud quads
//     (0x6D9A40); a ribbon throws sparkles (0x6D9DD0). Mist (0x6D95E0), Flake (0x6D96A0) -
//     sprite sequences.
// Module globals: 0x2543DF0..0x2543E90 (screen flash level 0x2543DF0, caster entity, file,
// texture pointer, context, the freeze band range 0x2543E04 / 0x2543E06, packet cursor
// 0x2543E1C, the seven queues); node pools and model copies live in the magic buffer. The
// sparkles write the translation of the static matrix 0x13197B0; the block's shape step writes
// the vertex indices of the static UV model 0x1315760. None of these tasks tests the battle's
// draw-only flag.

#include "mag_ice_common.h"

namespace ff8fx
{
namespace blizzaga104
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline int32_t &FlashLevel() { return var<int32_t>(0x2543DF0); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2543DF4); }
	inline uint8_t *&TexBase() { return var<uint8_t *>(0x2543DFC); }       // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2543E00); }
	inline int16_t &BandBase() { return var<int16_t>(0x2543E04); }
	inline int16_t &BandRange() { return var<int16_t>(0x2543E06); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2543E1C); }
	inline TaskQueue *Queue(int i) { return (TaskQueue *)(0x2543E30 + 0x10 * i); } // 0 freezes, 1 ribbons, 2 spirals, 3 sparkles, 4 mist, 5 flakes
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }   // frame packet cursor (battle models)

	static const uint32_t ORIG_Director = 0x6D6400;
	static const uint32_t ORIG_FreezeTask = 0x6D67E0;
	static const uint32_t ORIG_MistTask = 0x6D95E0;
	static const uint32_t ORIG_FlakeTask = 0x6D96A0;
	static const uint32_t ORIG_RibbonTask = 0x6D97A0;
	static const uint32_t ORIG_SparkleTask = 0x6D9DD0;
	static const uint32_t ORIG_SpiralTask = 0x6DA0D0;
	static const uint8_t *const MODEL_Block = (const uint8_t *)0x1312918;   // 0x8CA dwords
	static uint8_t *const MODEL_BlockUV = (uint8_t *)0x1315760;            // static, list 2 indices rewritten
	static const uint8_t *const BLOCK_VERTS = (const uint8_t *)0x13151D0;  // shape vertices (+8)
	static const uint32_t *const MORPH_TABLE = (const uint32_t *)0x13197D0; // growth vertex sets
	static const void *const MODEL_Ring = (const void *)0x1317088;
	static const void *const MODEL_Flash = (const void *)0x1318490;
	static const void *const SEQ_Mist = (const void *)0x1312690;
	static const void *const SEQ_Flake = (const void *)0x13127A0;
	static const uint32_t SPARKLE_MATRIX = 0x13197B0;
	static const void *const SOUND_Blizzaga = (const void *)0x13197A8;
	static const uint32_t MODEL_OFFSET[3] = { 0x11FB0, 0x142D8, 0x16600 };   // per action, in the magic buffer
	static const int SHARDS = 0x140;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline int16_t EntityHeight2(const uint8_t *entity) { return *(const int16_t *)(entity + 0x24); }

	// engine functions used by this module only
	inline int32_t CartesianToGameAngle(int32_t dx, int32_t dz) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(dx, dz); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void InsertPrimDepthKeys(uint32_t ot, void *prim, int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(uint32_t, void *, int32_t, int32_t, int32_t, int32_t)>(0x45C870)(ot, prim, a, b, c, d); }
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0
	inline void GteReadMAC123(void *dst) { fn<void (__cdecl *)(void *)>(0x45E450)(dst); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }
	inline uint32_t DrawShadow(void *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, shift, cursor); } // sub_5088A0
	inline void ComputeBonesWorldMatrices(void *anim, void *m) { fn<void (__cdecl *)(void *, void *)>(0x5095B0)(anim, m); }
	inline void BuildBoneMatricesFromPose(void *anim) { fn<void (__cdecl *)(void *)>(0x508C90)(anim); }
	inline void BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C cycles 0..49
		uint8_t action;    // +0x0E next action
		uint8_t pools;     // +0x0F pools built
		uint32_t arena;    // +0x10 packet arena toggle
	};
	// one triangle of the ice block turned into a shard (0x38 bytes)
	struct Shard
	{
		int16_t pos[3];    // +0x00 centre
		int16_t active;    // +0x06
		int16_t vel[3];    // +0x08
		int16_t live;      // +0x0E cleared when faded out
		int16_t angle[3];  // +0x10
		int16_t pad16;
		int16_t spin[3];   // +0x18
		int16_t pad1E;
		int16_t v[3][4];   // +0x20 vertices around the centre (v[2][3] = fade step)
	};
	struct FreezeNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E
		int16_t angle;     // +0x10 towards the caster
		int16_t band_lo;   // +0x12 frozen model: camera-space y range
		int16_t band_hi;   // +0x14
		int16_t pad16;
		uint8_t *entity;   // +0x18 target
		uint8_t *model;    // +0x1C ice block copy
		int16_t pos[3];    // +0x20 x, ground height, z
		int16_t pad26;
		Shard shards[SHARDS];          // +0x28
		int16_t verts[SHARDS * 3][4];  // +0x4628
	};
	// a trail position (0x24 bytes): two points, their projections, fade
	struct TrailEntry
	{
		int16_t v0[4];     // +0x00
		int16_t v1[4];     // +0x08
		uint32_t sxy0;     // +0x10
		int16_t otz0, pad16; // +0x14
		uint32_t sxy1;     // +0x18
		int16_t otz1, pad1E; // +0x1C
		uint8_t fade;      // +0x20
		int8_t fade_vel;   // +0x21
		uint8_t u;         // +0x22
		uint8_t pad23;
	};
	struct RibbonNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t angle;     // +0x10
		int16_t angle_vel; // +0x12
		int16_t angle_acc; // +0x14
		int16_t wave;      // +0x16
		int32_t count;     // +0x18
		uint8_t *entity;   // +0x1C
		Mat4x3 m;          // +0x20 (translation = camera-space position)
		TrailEntry trail[30]; // +0x40
	};
	struct SpiralNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t angle;     // +0x10
		int16_t angle_vel; // +0x12
		int16_t yaw;       // +0x14
		int16_t yaw_vel;   // +0x16
		int16_t scale;     // +0x18
		int16_t scale_vel; // +0x1A
		int16_t scale_acc; // +0x1C
		int16_t pad1E;
		int32_t count;     // +0x20
		FreezeNode *freeze;// +0x24
		uint8_t *entity;   // +0x28
		TrailEntry trail[30]; // +0x2C
	};
	struct SparkleNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t pad16;
		int16_t vel[3];    // +0x18
		int16_t pad1E;
		int16_t acc[3];    // +0x20
		int16_t pad26;
		Mat4x3 *m;         // +0x28 the ribbon's matrix
	};
	struct MistNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18
		int16_t pad1E;
		int16_t acc[3];    // +0x20
		int16_t pad26;
	};
	struct FlakeNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		FreezeNode *freeze;// +0x10
		int16_t pos[3];    // +0x14
		int16_t scale;     // +0x1A
		int16_t vel[3];    // +0x1C
		int16_t pad22;
		int16_t acc[3];    // +0x24
		int16_t blown;     // +0x2A
	};
#pragma pack(pop)
	static_assert(sizeof(DirectorNode) == 0x14 && sizeof(Shard) == 0x38 && sizeof(FreezeNode) == 0x6428 && sizeof(TrailEntry) == 0x24
		&& sizeof(RibbonNode) == 0x478 && sizeof(SpiralNode) == 0x464 && sizeof(SparkleNode) == 0x2C && sizeof(MistNode) == 0x28
		&& sizeof(FlakeNode) == 0x2C, "Blizzaga node layouts");
}
}

#ifdef FF8_FX_HELD
#include "mag104_blizzaga_held.h"
#endif

namespace ff8fx
{
namespace blizzaga104
{
	// ------------------------------------------------------------------
	// Director (0x6D6400)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_root();)
		bool may_end = true;
		if (d->arena)
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x18928);
			d->arena = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x28928);
			d->arena = 1;
		}
		FlashLevel() = 0;
		if (d->counter == 1)
		{
			if (!d->pools)
			{
				d->pools = 1;
				InitTaskQueuePool(Queue(0), TexBase(), 0x6428, 2);
				InitTaskQueuePool(Queue(1), TexBase() + 0xC850, 0x478, 6);
				InitTaskQueuePool(Queue(2), TexBase() + 0xE320, 0x464, 4);
				InitTaskQueuePool(Queue(3), TexBase() + 0xF4B0, 0x2C, 0x60);
				InitTaskQueuePool(Queue(4), TexBase() + 0x10530, 0x28, 0x40);
				InitTaskQueuePool(Queue(5), TexBase() + 0x10F30, 0x2C, 0x60);
			}
			ActionData *acts = Ctx()->actions;
			if (d->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[d->action].targets[0]);
				bool busy = false;
				uint8_t *slot = TexBase();
				for (int i = 0; i < 2; i++, slot += 0x6428)
				{
					FreezeNode *f = (FreezeNode *)slot;
					if ((f->hdr.flags & 1) && f->entity == entity)
					{
						busy = true;
						break;
					}
				}
				if (busy)
				{
					may_end = false;
					d->counter = 0;
				}
				else
				{
					FreezeNode *f = (FreezeNode *)AddTaskToQueue(Queue(0), ORIG_FreezeTask);
					FillDwords(&f->counter, 0, 0x1907);
					f->action = (int16_t)(uint16_t)d->action;
					f->entity = entity;
					GetEffectSpawnPosition(entity, 0xF1, 0, f->pos);
					f->pos[1] = EntityHeight2(entity);
					int16_t cpos[4];
					GetEffectSpawnPosition(Caster(), 0xF1, 0, cpos);
					cpos[1] = EntityHeight2(Caster());
					f->angle = (int16_t)CartesianToGameAngle(f->pos[0] - cpos[0], f->pos[2] - cpos[2]);
					f->band_hi = (int16_t)0x8001;
					f->band_lo = (int16_t)0x8001;
					int action = d->action;
					if (action < 3)
					{
						CopyDwords(TexBase() + MODEL_OFFSET[action], MODEL_Block, 0x8CA);
						f->model = TexBase() + MODEL_OFFSET[action];
					}
					uint8_t *prim = ModelList(6, f->model, nullptr);
					for (int i = 0; i < SHARDS; i++, prim += 0x1C)
					{
						prim[0x17] = 0;
						prim[0x1B] = 0x40;
					}
					d->action++;
				}
			}
		}
		int r0, r1, r2, r3, r4, r5;
		if (d->pools)
		{
			r0 = ExecuteTaskQueue(Queue(0));
			r1 = ExecuteTaskQueue(Queue(1));
			r2 = ExecuteTaskQueue(Queue(2));
			r3 = ExecuteTaskQueue(Queue(3));
			r4 = ExecuteTaskQueue(Queue(4));
			r5 = ExecuteTaskQueue(Queue(5));
		}
		else r0 = r1 = r2 = r3 = r4 = r5 = 1;
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (may_end && d->pools && !r0 && !r1 && !r2 && !r3 && !r4 && !r5) return TASK_END;
		d->counter++;
		if (d->counter >= 0x32) d->counter = 0;
		return 0;
	}

	// ------------------------------------------------------------------
	// Frozen target model (0x6D8C20 -> 0x6D8D80 -> 0x6D8EE0 / 0x6D9390): the target's battle
	// model (and its second model) drawn by the effect with depth keys; triangles with a vertex
	// whose camera-space y is inside [lo, hi] get the ice texture (clut +0x66, tpage +0x64, U
	// wrapped by +0x60). Draw header (0x88 bytes): +0x00 camera * entity matrix, +0x20.. the
	// object walker q: +0 primitive cursor, +4 projected vertex buffer (8 bytes each: SXY,
	// OTZ, flags), +8.. list counts, +0x14..+0x1A screen box / 8, +0x1C colour, +0x20 object
	// mask, +0x24.. current vertices, +0x54 lo, +0x56 hi, +0x58 / +0x5A y range seen.
	// ------------------------------------------------------------------
	static void ProjectVertices(uint8_t **cursor, uint8_t **work, uint8_t *q)
	{
		uint8_t *v = *cursor;
		int16_t count = *(int16_t *)v;
		uint8_t *w = *work;
		v += 2;
		for (int32_t left = count; left; left--, w += 8)
		{
			GteLoadV0u(v);
			GteRTPS();
			v += 6;
			GteReadSXY2(q + 0x24);
			GteReadOTZ(q + 0x28);
			if (*(uint16_t *)(q + 0x28) == 0)
			{
				*(uint16_t *)(w + 6) = 0x10;
				continue;
			}
			GteStoreIR123(q + 0x34);
			int16_t y = *(int16_t *)(q + 0x36);
			if (*(int16_t *)(q + 0x58) > y) *(int16_t *)(q + 0x58) = y;
			else if (*(int16_t *)(q + 0x5A) < y) *(int16_t *)(q + 0x5A) = y;
			if (y >= *(int16_t *)(q + 0x54) && y <= *(int16_t *)(q + 0x56)) *(uint16_t *)(q + 0x2A) = 0x100;
			else *(uint16_t *)(q + 0x2A) = 0;
			int32_t sx = *(int16_t *)(q + 0x24);
			if (sx < (*(int16_t *)(q + 0x14) << 3)) q[0x2A] |= 1;
			else if (sx >= (*(int16_t *)(q + 0x18) << 3)) q[0x2A] |= 2;
			int32_t sy = *(int16_t *)(q + 0x26);
			if (sy < (*(int16_t *)(q + 0x16) << 3)) q[0x2A] |= 4;
			else if (sy >= (*(int16_t *)(q + 0x1A) << 3)) q[0x2A] |= 8;
			*(uint32_t *)w = *(uint32_t *)(q + 0x24);
			*(uint32_t *)(w + 4) = *(uint32_t *)(q + 0x28);
		}
		*cursor = v;
		*work = w;
	}

	// ice texture on the packet's U bytes (in the band)
	static void IceUV(uint8_t *p, const uint8_t *q, const int *us, int nu)
	{
		*(uint16_t *)(p + 0xE) = *(uint16_t *)(q + 0x66);
		*(uint16_t *)(p + 0x16) = *(uint16_t *)(q + 0x64);
		uint8_t o = 0;
		for (int k = 0; k < nu; k++) o |= p[us[k]];
		int16_t wrap = *(int16_t *)(q + 0x60);
		if ((int16_t)o < wrap) return;
		for (int k = 0; k < nu; k++)
		{
			p[us[k]] = (uint8_t)(p[us[k]] - q[0x60]);
			if ((int16_t)p[us[k]] >= wrap) p[us[k]] = 0;
		}
	}

	static uint32_t DrawObjectPrims(uint8_t *q, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *w = *(uint8_t **)(q + 4);
		uint8_t *prim = *(uint8_t **)q;
		uint8_t *p = (uint8_t *)cursor;
		uint16_t n3 = *(uint16_t *)(q + 8);
		for (uint32_t i = 0; i < n3; i++, prim += 0x10)
		{
			memcpy(q + 0x24, w + 8 * (*(uint16_t *)prim & 0xFFF), 8);
			memcpy(q + 0x2C, w + 8 * (*(uint16_t *)(prim + 2) & 0xFFF), 8);
			memcpy(q + 0x34, w + 8 * (*(uint16_t *)(prim + 4) & 0xFFF), 8);
			if ((uint8_t)(q[0x2A] | q[0x32] | q[0x3A]) >= 0x10) continue;
			if (q[0x3A] & (q[0x2A] & q[0x32])) continue;
			GteSetSXY012(*(uint32_t *)(q + 0x24), *(uint32_t *)(q + 0x2C), *(uint32_t *)(q + 0x34));
			GteNCLIP();
			GteReadMAC0(q + 0x44);
			if (*(int32_t *)(q + 0x44) < 0) continue;
			uint32_t za = *(uint16_t *)(q + 0x28), zb = *(uint16_t *)(q + 0x30), zc = *(uint16_t *)(q + 0x38);
			GteSetSZ123(za, zb, zc);
			GteAVSZ3();
			*(uint32_t *)p = 0x07000000;
			*(uint32_t *)(p + 4) = (((uint32_t)(*(uint16_t *)(prim + 0xE) & 0x200) | 0x2400) << 16) | *(uint32_t *)(q + 0x1C);
			*(uint32_t *)(p + 8) = *(uint32_t *)(q + 0x24);
			*(uint32_t *)(p + 0x10) = *(uint32_t *)(q + 0x2C);
			*(uint32_t *)(p + 0x18) = *(uint32_t *)(q + 0x34);
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 8);
			*(uint32_t *)(p + 0x14) = *(uint32_t *)(prim + 0xC);
			*(uint16_t *)(p + 0x16) &= 0xFDFF;
			*(uint16_t *)(p + 0x1C) = *(uint16_t *)(prim + 6);
			if ((uint16_t)(*(uint16_t *)(q + 0x32) | *(uint16_t *)(q + 0x3A) | *(uint16_t *)(q + 0x2A)) >= 0x100)
			{
				static const int us[3] = { 0xC, 0x14, 0x1C };
				IceUV(p, q, us, 3);
			}
			GteReadOTZReg(q + 0x4C);
			InsertPrimDepthKeys(ot + 4 * (*(int32_t *)(q + 0x4C) >> shift), p, (int32_t)za * 4, (int32_t)zb * 4, (int32_t)zc * 4, 0);
			p += 0x20;
		}
		uint16_t n4 = *(uint16_t *)(q + 0xA);
		prim += 4;
		for (uint32_t i = 0; i < n4; i++, prim += 0x14)
		{
			memcpy(q + 0x24, w + 8 * (*(uint16_t *)(prim - 4) & 0xFFF), 8);
			memcpy(q + 0x2C, w + 8 * (*(uint16_t *)(prim - 2) & 0xFFF), 8);
			memcpy(q + 0x34, w + 8 * (*(uint16_t *)prim & 0xFFF), 8);
			memcpy(q + 0x3C, w + 8 * (*(uint16_t *)(prim + 2) & 0xFFF), 8);
			if ((uint8_t)(q[0x42] | q[0x2A] | q[0x32] | q[0x3A]) >= 0x10) continue;
			if (q[0x3A] & (q[0x42] & q[0x2A] & q[0x32])) continue;
			GteSetSXY012(*(uint32_t *)(q + 0x24), *(uint32_t *)(q + 0x2C), *(uint32_t *)(q + 0x34));
			GteNCLIP();
			GteReadMAC0(q + 0x44);
			if (*(int32_t *)(q + 0x44) < 0) continue;
			uint32_t za = *(uint16_t *)(q + 0x28), zb = *(uint16_t *)(q + 0x30), zc = *(uint16_t *)(q + 0x38), zd = *(uint16_t *)(q + 0x40);
			GteSetSZ0123(za, zb, zc, zd);
			GteAVSZ4();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = (((uint32_t)(*(uint16_t *)(prim + 0xA) & 0x200) | 0x2C00) << 16) | *(uint32_t *)(q + 0x1C);
			*(uint32_t *)(p + 8) = *(uint32_t *)(q + 0x24);
			*(uint32_t *)(p + 0x10) = *(uint32_t *)(q + 0x2C);
			*(uint32_t *)(p + 0x18) = *(uint32_t *)(q + 0x34);
			*(uint32_t *)(p + 0x20) = *(uint32_t *)(q + 0x3C);
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 4);
			*(uint32_t *)(p + 0x14) = *(uint32_t *)(prim + 8);
			*(uint16_t *)(p + 0x1C) = *(uint16_t *)(prim + 0xC);
			*(uint16_t *)(p + 0x16) &= 0xFDFF;
			*(uint16_t *)(p + 0x24) = *(uint16_t *)(prim + 0xE);
			if ((uint16_t)(*(uint16_t *)(q + 0x32) | *(uint16_t *)(q + 0x3A) | *(uint16_t *)(q + 0x2A) | *(uint16_t *)(q + 0x42)) >= 0x100)
			{
				static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 };
				IceUV(p, q, us, 4);
			}
			GteReadOTZReg(q + 0x4C);
			InsertPrimDepthKeys(ot + 4 * (*(int32_t *)(q + 0x4C) >> shift), p, (int32_t)za * 4, (int32_t)zb * 4, (int32_t)zc * 4, (int32_t)zd * 4);
			p += 0x28;
		}
		return (uint32_t)p;
	}

	static uint32_t DrawModelObjects(uint8_t *model, uint8_t *q, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *bones = *(uint8_t **)model + 0x10;
		uint8_t *table = *(uint8_t **)(model + 4);
		int32_t count = *(int32_t *)table;
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint8_t *obj = *(uint8_t **)(model + 4) + *(uint32_t *)table;
			table += 4;
			if (!(*(uint32_t *)(q + 0x20) & (1u << (i & 31)))) continue;
			uint8_t *work = *(uint8_t **)(q + 4);
			int16_t nb = *(int16_t *)obj;
			obj += 2;
			for (int16_t b = nb; b > 0; b--)
			{
				int16_t bone = *(int16_t *)obj;
				obj += 2;
				Mat4x3 *m = (Mat4x3 *)(bones + 0x30 * bone + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				ProjectVertices(&obj, &work, q);
			}
			obj = (uint8_t *)(((uint32_t)obj + 3) & ~3u);
			for (int k = 0; k < 6; k++, obj += 2) *(uint16_t *)(q + 8 + 2 * k) = *(uint16_t *)obj;
			*(uint8_t **)q = obj;
			cursor = DrawObjectPrims(q, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x6D8C20
	static void DrawFrozenEntity(uint8_t *entity, int16_t lo, int16_t hi, int16_t *range_lo, int16_t *range_hi, const int16_t *uv)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x88);
		*(uint32_t *)(h + 0x7C) = *(const uint32_t *)uv;
		*(uint16_t *)(h + 0x86) = 0x3D14;
		*(uint32_t *)(h + 0x80) = *(const uint32_t *)(uv + 2);
		*(uint16_t *)(h + 0x84) = 0xBA;
		*(int16_t *)(h + 0x74) = lo;
		*(int16_t *)(h + 0x76) = hi;
		*(uint16_t *)(h + 0x78) = 0x7FFF;
		*(uint16_t *)(h + 0x7A) = 0x8001;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)h);
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(if (!held_drawing()))
		FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4040, 0x10, FrameCursor());
		uint8_t *anim = entity + 0x60;
		ComputeBonesWorldMatrices(anim, h);
		*(uint32_t *)(h + 0x24) = var<uint32_t>(0x1D98B3C);
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(entity + 0x28);
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(entity + 0x7C);
		FrameCursor() = DrawModelObjects(*(uint8_t **)(anim + 4), h + 0x20, RenderOT44(), 0, FrameCursor());
		BuildBoneMatricesFromPose(anim);
		uint8_t *second = *(uint8_t **)(entity + 0x78);
		if (second)
		{
			*(uint32_t *)(h + 0x40) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(second, h);
			FrameCursor() = DrawModelObjects(*(uint8_t **)(second + 4), h + 0x20, RenderOT44(), 0, FrameCursor());
			BuildBoneMatricesFromPose(second);
		}
		FieldFree(0x88);
		*range_lo = *(int16_t *)(h + 0x78);
		*range_hi = *(int16_t *)(h + 0x7A);
	}

	// the target and the entities linked to it (+0x8C ring): flags
	static void TargetFlags(uint8_t *entity, uint16_t set, uint16_t clear_all, uint16_t set_all)
	{
		*(uint16_t *)entity |= set;
		uint8_t *e = entity;
		do
		{
			*(uint16_t *)e = (uint16_t)((*(uint16_t *)e & ~clear_all) | set_all);
			e = *(uint8_t **)(e + 0x8C);
		} while (e && e != entity);
	}

	// 0x6D9510 / 0x6D94B0: yaw rotation (0x6D94B0: scaled by s, y scale sy)
	static void YawMatrix(int16_t yaw, Mat4x3 *m)
	{
		int32_t sn = ComputeSin(yaw);
		int32_t cs = ComputeCos(yaw);
		m->m[0][2] = (int16_t)sn;
		m->m[2][0] = (int16_t)-sn;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = 0x1000;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	static void YawScaleMatrix(int16_t yaw, Mat4x3 *m, int16_t s, int16_t sy)
	{
		int32_t sn = mul32(ComputeSin(yaw), s) >> 12;
		int32_t cs = mul32(ComputeCos(yaw), s) >> 12;
		m->m[0][2] = (int16_t)sn;
		m->m[1][1] = sy;
		m->m[2][0] = (int16_t)-sn;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	// 0x6D9560: vertices of the model copy = a + (b - a) * frac (GTE interpolation)
	static void MorphVertices(uint8_t *h, const uint8_t *a, const uint8_t *b, int32_t frac)
	{
		int16_t *out = *(int16_t **)(h + 8);
		uint32_t n = *(uint32_t *)(Cursor(h, 0) + 4);
		const uint8_t *va = a + 8, *vb = b + 8;
		for (; n; n--, out += 4, va += 8, vb += 8)
		{
			GteSetIR0(0x1000 - frac);
			GteLoadIR123(va);
			GteGPF();
			GteSetIR0(frac);
			GteLoadIR123(vb);
			GteGPL();
			GteStoreIR123(out);
		}
	}

	static uint32_t RenderPolys104(uint8_t *h, uint32_t ot, int shift, uint32_t cursor) { return RenderPolys(h, ot, shift, cursor, false); }
	static uint32_t RenderBlock(uint8_t *h, uint32_t ot, int shift, uint32_t cursor) { return RenderModel(h, ot, shift, cursor, RenderPolys104); }

	static void BlockHeader(uint8_t *h, const FreezeNode *f, int16_t counter)
	{
		Cursor(h, 0) = f->model;
		Cursor(h, 4) = MODEL_BlockUV;
		*(const void **)(h + 8) = f->verts;
		*(uint32_t *)(h + 0x18) = 0x66000;
		*(uint16_t *)(h + 0x1C) = 0;
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x1E) = (uint16_t)((counter & 0x1F) << 2);
		*(uint16_t *)(h + 0x26) = 0x100;
		*(uint16_t *)(h + 0x24) = 0x100;
		*(uint16_t *)(h + 0x28) = 0;
		*(uint16_t *)(h + 0x2A) = 0x80;
		*(uint16_t *)(h + 0x2C) = 0x40;
		*(uint16_t *)(h + 0x2E) = 0x80;
	}

	// ------------------------------------------------------------------
	// Shard update (0x6D9EC0): every live shard fades (its 9 colour bytes down by its step; dead
	// when all are 0), falls (z velocity += 8 in the block's frame), moves, spins; its three
	// vertices are rewritten in the vertex buffer
	// ------------------------------------------------------------------
	static void PoseShard(const Shard *s, int16_t (*vbuf)[4], const uint8_t *prim)
	{
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(s->angle, &m);
		m.t[0] = s->pos[0];
		m.t[1] = s->pos[1];
		m.t[2] = s->pos[2];
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		for (int k = 0; k < 3; k++)
		{
			int16_t *v = vbuf[*(const uint16_t *)(prim + 4 + 2 * k) >> 1];
			GteLoadV0(s->v[k]);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(v);
		}
	}

	static void UpdateShards(Shard *s, int16_t (*vbuf)[4], uint8_t *prim)
	{
		static const uint8_t colour[9] = { 0, 1, 2, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A };
		for (int i = 0; i < SHARDS; i++, s++, prim += 0x1C)
		{
			if (!s->active || !s->live) continue;
			bool gone = true;
			for (int k = 0; k < 9; k++)
			{
				int32_t v = (int32_t)prim[colour[k]] - s->v[2][3];
				if (v > 0)
				{
					prim[colour[k]] = (uint8_t)v;
					gone = false;
				}
				else prim[colour[k]] = 0;
			}
			if (gone)
			{
				s->live = 0;
				continue;
			}
			s->vel[2] = (int16_t)(s->vel[2] + 8);
			s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
			s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
			s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
			s->angle[1] = (int16_t)(s->angle[1] + s->spin[1]);
			s->angle[0] = (int16_t)(s->angle[0] + s->spin[0]);
			s->angle[2] = (int16_t)(s->angle[2] + s->spin[2]);
			PoseShard(s, vbuf, prim);
		}
	}

	// ------------------------------------------------------------------
	// Ring (0x6D7CB0 -> 0x6D7D00 / 0x6D81F0 / 0x6D8680): model 0x1317088's textured quads,
	// Gouraud triangles and Gouraud quads, depth cued towards the colour +4 by +8, UVs scrolled by
	// +0x0C / +0x0E (wrapping by +0x1C / +0x1E), each between two texture windows (+0x10, +0x18).
	// Header (0x5C): +0x20 vertices, +0x24 list cursor, +0x30 OTZ, +0x34 FLAG, +0x38.. vertices.
	// ------------------------------------------------------------------
	static void ScrollBytes(uint8_t *p, const int *off, int n, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < n; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < n; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	static uint8_t *TextureWindows(uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 0x10);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0xC;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x18);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x18;
	}

	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	static uint32_t RingQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *verts = Cursor(h, 0x20);
		uint8_t *prim = Cursor(h, 0x24);
		int32_t count = *(int32_t *)prim;
		prim += 4;
		Cursor(h, 0x24) = prim;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, prim += 0x18)
		{
			memcpy(h + 0x38, verts + 4 * *(uint16_t *)(prim + 4), 8);
			memcpy(h + 0x40, verts + 4 * *(uint16_t *)(prim + 6), 8);
			memcpy(h + 0x48, verts + 4 * *(uint16_t *)(prim + 8), 8);
			memcpy(h + 0x50, verts + 4 * *(uint16_t *)(prim + 0xA), 8);
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(uint32_t *)prim;
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 0xC);
			*(uint32_t *)(p + 0x14) = *(uint32_t *)(prim + 0x10);
			uint32_t uv = *(uint32_t *)(prim + 0x14);
			*(uint32_t *)(p + 0x1C) = uv;
			*(uint32_t *)(p + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(h + 0x50);
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(int16_t *)(p + 8))) clip = 1;
			if (OffX(*(int16_t *)(p + 0x10))) clip |= 2;
			if (OffX(*(int16_t *)(p + 0x18))) clip |= 4;
			if (OffY(*(int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(int16_t *)(p + 0x12))) clip |= 0x20;
			if (OffY(*(int16_t *)(p + 0x1A))) clip |= 0x40;
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (OffX(*(int16_t *)(p + 0x20))) clip |= 8;
			if (OffY(*(int16_t *)(p + 0x22))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x30);
			if (*(int32_t *)(h + 8))
			{
				GteSetFarColor(h[4], h[5], h[6]);
				GteLoadRGBC(p + 4);
				GteSetIR0(*(int32_t *)(h + 8));
				GteDPCS();
				GteStoreRGB2(p + 4);
			}
			static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 }, vs[4] = { 0xD, 0x15, 0x1D, 0x25 };
			ScrollBytes(p, us, 4, *(uint16_t *)(h + 0xC), h[0x1C]);
			ScrollBytes(p, vs, 4, *(uint16_t *)(h + 0xE), h[0x1E]);
			uint32_t otp = ot + 4 * (*(int32_t *)(h + 0x30) >> shift);
			p = TextureWindows(h, otp, p, p + 0x28);
		}
		Cursor(h, 0x24) = prim;
		return (uint32_t)p;
	}

	static uint32_t RingTriangles(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *verts = Cursor(h, 0x20);
		uint8_t *prim = Cursor(h, 0x24);
		int32_t count = *(int32_t *)prim;
		prim += 4;
		Cursor(h, 0x24) = prim;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, prim += 0x1C)
		{
			memcpy(h + 0x38, verts + 4 * *(uint16_t *)(prim + 4), 8);
			memcpy(h + 0x40, verts + 4 * *(uint16_t *)(prim + 6), 8);
			memcpy(h + 0x48, verts + 4 * *(uint16_t *)(prim + 8), 8);
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(uint32_t *)prim;
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 0xC);
			*(uint32_t *)(p + 0x18) = *(uint32_t *)(prim + 0x10);
			*(uint32_t *)(p + 0x24) = *(uint32_t *)(prim + 8) >> 16;
			GteReadFLAG(h + 0x34);
			if (*(uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteAVSZ3();
			uint32_t clip = 0;
			if (OffX(*(int16_t *)(p + 8))) clip = 1;
			if (OffX(*(int16_t *)(p + 0x14))) clip |= 2;
			if (OffX(*(int16_t *)(p + 0x20))) clip |= 4;
			if (OffY(*(int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(int16_t *)(p + 0x16))) clip |= 0x20;
			if (OffY(*(int16_t *)(p + 0x22))) clip |= 0x40;
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteReadOTZReg(h + 0x30);
			if (*(int32_t *)(h + 8))
			{
				GteSetFarColor(h[4], h[5], h[6]);
				GteLoadRGB012(prim + 0x14, prim + 0x18, p + 4);
				GteSetIR0(*(int32_t *)(h + 8));
				GteDPCT();
				GteStoreRGB012(p + 0x10, p + 0x1C, p + 4);
			}
			else
			{
				*(uint32_t *)(p + 0x10) = *(uint32_t *)(prim + 0x14);
				*(uint32_t *)(p + 0x1C) = *(uint32_t *)(prim + 0x18);
			}
			static const int us[3] = { 0xC, 0x18, 0x24 }, vs[3] = { 0xD, 0x19, 0x25 };
			ScrollBytes(p, us, 3, *(uint16_t *)(h + 0xC), h[0x1C]);
			ScrollBytes(p, vs, 3, *(uint16_t *)(h + 0xE), h[0x1E]);
			uint32_t otp = ot + 4 * (*(int32_t *)(h + 0x30) >> shift);
			p = TextureWindows(h, otp, p, p + 0x28);
		}
		Cursor(h, 0x24) = prim;
		return (uint32_t)p;
	}

	static uint32_t RingGouraudQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *verts = Cursor(h, 0x20);
		uint8_t *prim = Cursor(h, 0x24);
		int32_t count = *(int32_t *)prim;
		prim += 4;
		Cursor(h, 0x24) = prim;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, prim += 0x24)
		{
			memcpy(h + 0x38, verts + 4 * *(uint16_t *)(prim + 4), 8);
			memcpy(h + 0x40, verts + 4 * *(uint16_t *)(prim + 6), 8);
			memcpy(h + 0x48, verts + 4 * *(uint16_t *)(prim + 8), 8);
			memcpy(h + 0x50, verts + 4 * *(uint16_t *)(prim + 0xA), 8);
			GteLoadV012(h + 0x38, h + 0x40, h + 0x48);
			GteRTPT();
			*(uint32_t *)p = 0x0C000000;
			*(uint32_t *)(p + 4) = *(uint32_t *)prim;
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 0xC);
			*(uint32_t *)(p + 0x18) = *(uint32_t *)(prim + 0x10);
			uint32_t uv = *(uint32_t *)(prim + 0x14);
			*(uint32_t *)(p + 0x24) = uv;
			*(uint32_t *)(p + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(h + 0x50);
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(int16_t *)(p + 8))) clip = 1;
			if (OffX(*(int16_t *)(p + 0x14))) clip |= 2;
			if (OffX(*(int16_t *)(p + 0x20))) clip |= 4;
			if (OffY(*(int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(int16_t *)(p + 0x16))) clip |= 0x20;
			if (OffY(*(int16_t *)(p + 0x22))) clip |= 0x40;
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (OffX(*(int16_t *)(p + 0x2C))) clip |= 8;
			if (OffY(*(int16_t *)(p + 0x2E))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x30);
			if (*(int32_t *)(h + 8))
			{
				GteSetFarColor(h[4], h[5], h[6]);
				GteLoadRGB012(prim + 0x18, prim + 0x1C, prim + 0x20);
				GteSetIR0(*(int32_t *)(h + 8));
				GteDPCT();
				GteStoreRGB012(p + 0x10, p + 0x1C, p + 0x28);
				GteLoadRGBC(p + 4);
				GteDPCS();
				GteStoreRGB2(p + 4);
			}
			else
			{
				*(uint32_t *)(p + 0x10) = *(uint32_t *)(prim + 0x18);
				*(uint32_t *)(p + 0x1C) = *(uint32_t *)(prim + 0x1C);
				*(uint32_t *)(p + 0x28) = *(uint32_t *)(prim + 0x20);
			}
			static const int us[4] = { 0xC, 0x18, 0x24, 0x30 }, vs[4] = { 0xD, 0x19, 0x25, 0x31 };
			ScrollBytes(p, us, 4, *(uint16_t *)(h + 0xC), h[0x1C]);
			ScrollBytes(p, vs, 4, *(uint16_t *)(h + 0xE), h[0x1E]);
			uint32_t otp = ot + 4 * (*(int32_t *)(h + 0x30) >> shift);
			p = TextureWindows(h, otp, p, p + 0x34);
		}
		Cursor(h, 0x24) = prim;
		return (uint32_t)p;
	}

	static uint32_t RenderRing(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x20) = m + 8;
		Cursor(h, 0x24) = m + *(uint32_t *)m + 0xC;
		uint32_t r = RingQuads(h, ot, shift, cursor);
		Cursor(h, 0x24) = Cursor(h, 0x24) + 8;
		r = RingTriangles(h, ot, shift, r);
		return RingGouraudQuads(h, ot, shift, r);
	}

	static void RingHeader(uint8_t *h, int16_t c)
	{
		int32_t t = c - 0x2E;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(const void **)h = MODEL_Ring;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1C) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x40;
		h[6] = 0;
		h[5] = 0;
		h[4] = 0;
		*(uint16_t *)(h + 0xC) = (uint16_t)((c & 0xF) << 3);
		if ((uint32_t)t < 4) *(int32_t *)(h + 8) = 0x1000 - ((t & 0xFFFFF) << 10);
		else if ((uint32_t)t > 0x11) *(int32_t *)(h + 8) = 0x1000 - (((0x19 - t) << 9) & 0x1FFFFE00);
		else *(int32_t *)(h + 8) = 0;
	}

	static int16_t RingScale(int16_t c)
	{
		uint32_t t = (uint32_t)(c - 0x2E);
		uint32_t a = (uint32_t)(((uint64_t)(t << 10) * 0x51EB851Fu) >> 32) >> 3; // / 25
		return (int16_t)((ComputeSin((int32_t)a) >> 2) + 0xC00);
	}

	static void RingDraw(const FreezeNode *f, uint8_t *h, int16_t s)
	{
		Mat4x3 m = {};
		YawScaleMatrix(f->angle, &m, s, s);
		m.t[0] = f->pos[0];
		m.t[1] = f->pos[1];
		m.t[2] = f->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PacketCursor() = RenderRing(h, RenderOT44(), 2, PacketCursor());
	}

	// flash model 0x1318490 (31..40): x / z scale 1 + sin, fading out from 38
	static int16_t FlashScale(int16_t c)
	{
		uint32_t t = (uint32_t)(c - 0x1F);
		uint32_t a = (uint32_t)(((uint64_t)(t << 10) * 0xCCCCCCCDu) >> 32) >> 3; // / 10
		return (int16_t)(ComputeSin((int32_t)a) * 4 + 0x1000);
	}

	static void FlashDraw(const FreezeNode *f, uint8_t *h, int16_t s, int32_t fade)
	{
		Mat4x3 m = {};
		m.m[0][0] = s;
		m.m[1][1] = 0x1000;
		m.m[2][2] = s;
		m.t[0] = f->pos[0];
		m.t[1] = f->pos[1];
		m.t[2] = f->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		*(const void **)h = MODEL_Flash;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
	}

	static int32_t FlashFade(int16_t c)
	{
		uint32_t t = (uint32_t)(c - 0x1F);
		return t > 6 ? (int32_t)(0x1000 - (((0xA - t) << 10) & 0x3FFFFC00)) : 0;
	}

	// y / 18 and y / 9 (0x38E38E39)
	static int32_t DivMagic9(int32_t x, int sh)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x38E38E39) >> 32) >> sh;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// the freeze band: from the model's y range, rising 8..25, falling back 60..68
	static int16_t FreezeLevel(const FreezeNode *f, int32_t t)
	{
		if ((uint32_t)t < 0x12) return (int16_t)(DivMagic9((f->band_lo - f->band_hi) * t, 2) + f->band_hi);
		if ((uint32_t)t >= 0x34) return (int16_t)(DivMagic9((f->band_lo - f->band_hi) * (0x3D - t), 1) + f->band_hi);
		return f->band_lo;
	}

	// x / 3 (0x55555556)
	static int32_t Div3(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x55555556) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// growth morph step of the block (28..30): table index and fraction
	static uint32_t MorphStep(uint32_t t) { return (uint32_t)(((uint64_t)(t << 12) * 0xAAAAAAABu) >> 33) + 0x2AA; }

	// ------------------------------------------------------------------
	// Freeze (0x6D67E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FreezeTask(TaskNode *n)
	{
		FreezeNode *f = (FreezeNode *)n;
		Mat4x3 m = {};
		YawMatrix(f->angle, &m);
		m.t[0] = f->pos[0];
		m.t[1] = f->pos[1];
		m.t[2] = f->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		int16_t c = f->counter;
		{
			int32_t level;
			if (c < 4) level = c << 8;
			else if (c > 0x5E) level = (0x62 - c) << 8;
			else level = -1;
			if (level >= 0) { if (level > FlashLevel()) FlashLevel() = level; }
			else if (FlashLevel() < 0x400) FlashLevel() = 0x400;
		}
		int32_t t8 = c - 8;
		if ((uint32_t)t8 < 0x3D)
		{
			*(uint16_t *)f->entity |= 4;
			TargetFlags(f->entity, 0, 8, 0);
			bool blink = (uint32_t)t8 < 0x12 || (uint32_t)t8 >= 0x34;
			if (!blink || !(t8 & 1)) TargetFlags(f->entity, 0, 0, 8);
			int16_t uv[4] = { 0, 0, 0x40, 0x100 };
			// 30 fps layer: see mag104_blizzaga_held.inc
			FX_HELD(held_note_frozen(f, FreezeLevel(f, t8));)
			DrawFrozenEntity(f->entity, FreezeLevel(f, t8), f->band_hi, &f->band_lo, &f->band_hi, uv);
		}
		else
		{
			*(uint16_t *)f->entity &= 0xFFFB;
			TargetFlags(f->entity, 0, 8, 0);
		}
		if ((uint32_t)c < 3)
		{
			RibbonNode *r = (RibbonNode *)AddTaskToQueue(Queue(1), ORIG_RibbonTask);
			if (r)
			{
				FillDwords(&r->counter, 0, 0x11B);
				r->entity = f->entity;
				r->angle = (int16_t)((uint32_t)(c << 12) / 3);
				r->angle_vel = 0x100;
				r->angle_acc = -10;
				r->wave = 0x800;
			}
		}
		if ((uint32_t)c < 2)
		{
			SpiralNode *s = (SpiralNode *)AddTaskToQueue(Queue(2), ORIG_SpiralTask);
			if (s)
			{
				FillDwords(&s->counter, 0, 0x116);
				s->freeze = f;
				s->entity = f->entity;
				int16_t a = (int16_t)(c << 11);
				s->angle = a;
				s->yaw = a;
				s->scale = 0x1000;
				s->scale_acc = 0;
				s->scale_vel = 0;
				s->angle_vel = 0xC0;
				s->yaw_vel = 0x14;
				if (c & 1)
				{
					s->angle_vel = (int16_t)0xFF40;
					s->yaw_vel = (int16_t)0xFFEC;
				}
			}
		}
		if ((uint32_t)(c - 0x1C) < 5)
		{
			uint32_t t = c - 0x1C;
			uint8_t *h = (uint8_t *)FieldAlloc(0x64);
			BlockHeader(h, f, c);
			if (t < 3)
			{
				uint32_t v = MorphStep(t);
				uint32_t idx = v >> 12;
				MorphVertices(h, (const uint8_t *)MORPH_TABLE[idx], (const uint8_t *)MORPH_TABLE[idx + 1], v & 0xFFF);
			}
			else if (t == 3) CopyDwords(f->verts, BLOCK_VERTS + 8, *(uint32_t *)(Cursor(h, 0) + 4) * 2);
			// 30 fps layer: see mag104_blizzaga_held.inc
			FX_HELD(held_note_block(f, t < 3 ? 1 : 0, &m);)
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
			FieldFree(0x64);
		}
		int32_t t21 = c - 0x21;
		if ((uint32_t)t21 < 0x41)
		{
			uint8_t *prims = ModelList(6, f->model, nullptr);
			if (t21 == 0)
			{
				uint8_t *h = (uint8_t *)FieldAlloc(0x58);
				Cursor(h, 0) = f->model;
				*(uint32_t *)(h + 0x1C) = 0xC;
				PrepareModel(h, (uint8_t *)BLOCK_VERTS, f->verts[0]);
				FieldFree(0x58);
				uint8_t *uv = ModelList(2, MODEL_BlockUV, nullptr);
				uint8_t *prim = prims;
				for (int i = 0; i < SHARDS; i++, prim += 0x1C, uv += 0x14)
				{
					f->shards[i].active = 0;
					f->shards[i].live = 1;
					*(uint32_t *)(uv + 4) = *(uint32_t *)(prim + 4);
					*(uint16_t *)(uv + 8) = *(uint16_t *)(prim + 8);
				}
			}
			else if (t21 >= 0x12)
			{
				int32_t level = (int32_t)((uint32_t)((t21 - 0x12) * BandRange()) >> 5) + BandBase();
				Shard *s = f->shards;
				uint8_t *prim = prims;
				for (int i = 0; i < SHARDS; i++, s++, prim += 0x1C)
				{
					if (s->active) continue;
					int16_t *va = f->verts[*(uint16_t *)(prim + 4) >> 1];
					int16_t *vb = f->verts[*(uint16_t *)(prim + 6) >> 1];
					int16_t *vc = f->verts[*(uint16_t *)(prim + 8) >> 1];
					if (va[1] > level && vb[1] > level && vc[1] > level && t21 != 0x31) continue;
					int16_t cx = (int16_t)Div3(va[0] + vc[0] + vb[0]);
					s->pos[0] = cx;
					int16_t cy = (int16_t)Div3(vc[1] + vb[1] + va[1]);
					s->pos[1] = cy;
					int16_t cz = (int16_t)Div3(vc[2] + va[2] + vb[2]);
					s->pos[2] = cz;
					s->active = 1;
					s->v[0][0] = (int16_t)(va[0] - cx);
					s->v[0][1] = (int16_t)(va[1] - cy);
					s->v[0][2] = (int16_t)(va[2] - cz);
					s->v[1][0] = (int16_t)(vb[0] - cx);
					s->v[1][1] = (int16_t)(vb[1] - cy);
					s->v[1][2] = (int16_t)(vb[2] - cz);
					s->v[2][0] = (int16_t)(vc[0] - cx);
					s->v[2][1] = (int16_t)(vc[1] - cy);
					s->v[2][2] = (int16_t)(vc[2] - cz);
					s->vel[0] = (int16_t)(-(cy * cx) >> 15);
					s->vel[2] = 0;
					s->vel[1] = (int16_t)(cy >> 4);
					s->angle[2] = 0;
					s->angle[1] = 0;
					s->angle[0] = 0;
					s->spin[0] = (int16_t)((CrtRand() - 0x4000) >> 7);
					s->spin[1] = (int16_t)((CrtRand() - 0x4000) >> 7);
					s->spin[2] = (int16_t)((CrtRand() - 0x4000) >> 7);
					s->v[2][3] = (int16_t)(8 - (CrtRand() & 3));
					prim[0x17] = 0x80;
					*(uint32_t *)prim |= 0x02000000;
					prim[0x1B] = 0;
				}
			}
			UpdateShards(f->shards, f->verts, prims);
			uint8_t *h = (uint8_t *)FieldAlloc(0x64);
			BlockHeader(h, f, c);
			// 30 fps layer: see mag104_blizzaga_held.inc
			FX_HELD(held_note_block(f, 2, &m);)
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
			FieldFree(0x64);
		}
		if ((uint32_t)(c - 0x2E) < 0x19)
		{
			// 30 fps layer: see mag104_blizzaga_held.inc
			FX_HELD(held_note_part(ORIG_FreezeTask + 1, f);)
			uint8_t *h = AllocHeader(0x5C);
			RingHeader(h, c);
			RingDraw(f, h, RingScale(c));
			FieldFree(0x5C);
		}
		if ((uint32_t)(c - 0x1F) < 0xA)
		{
			// 30 fps layer: see mag104_blizzaga_held.inc
			FX_HELD(held_note_part(ORIG_FreezeTask + 2, f);)
			uint8_t *h = AllocHeader(0x58);
			FlashDraw(f, h, FlashScale(c), FlashFade(c));
			FieldFree(0x58);
		}
		if ((uint32_t)(c - 0x1C) <= 0xF)
		{
			int32_t angle = CrtRand();
			for (int k = 0; k < 8; k++)
			{
				FlakeNode *fl = (FlakeNode *)AddTaskToQueue(Queue(5), ORIG_FlakeTask);
				if (!fl) continue;
				FillDwords(&fl->counter, 0, 8);
				int32_t r1 = CrtRand();
				int32_t r2 = CrtRand();
				int32_t a = ((r1 + r2 - 0x8000) >> 8) + angle;
				angle += 0x200;
				fl->freeze = f;
				fl->pos[0] = (int16_t)Div3(ComputeCos(a));
				fl->pos[1] = 0;
				fl->pos[2] = (int16_t)Div3(ComputeSin(a));
				fl->scale = (int16_t)(0x1770 - (CrtRand() >> 4));
				NormalizeVector16(fl->pos, fl->vel);
				fl->pos[0] = (int16_t)(fl->pos[0] + f->pos[0]);
				fl->pos[2] = (int16_t)(fl->pos[2] + f->pos[2]);
				fl->pos[1] = f->pos[1];
				int32_t q = (int32_t)(((int64_t)fl->vel[0] * 0x2AAAAAAB) >> 32) >> 3;
				int16_t vx = (int16_t)(q + (int32_t)((uint32_t)q >> 31)); // / 48
				q = (int32_t)(((int64_t)fl->vel[2] * 0x2AAAAAAB) >> 32) >> 3;
				int16_t vz = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
				fl->vel[0] = vx;
				fl->vel[2] = vz;
				q = ((int32_t)(((int64_t)vx * 0x6DB6DB6D) >> 32) - vx) >> 3; // - v / 14
				fl->acc[0] = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
				q = ((int32_t)(((int64_t)vz * 0x6DB6DB6D) >> 32) - vz) >> 3;
				fl->acc[2] = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
			}
		}
		if ((uint32_t)(c - 0x26) <= 0x1E)
		{
			int32_t angle = CrtRand();
			for (int k = 0; k < 8; k++)
			{
				MistNode *mi = (MistNode *)AddTaskToQueue(Queue(4), ORIG_MistTask);
				if (!mi) continue;
				FillDwords(&mi->counter, 0, 7);
				int32_t r1 = CrtRand();
				int32_t r2 = CrtRand();
				int32_t a = ((r1 + r2 - 0x8000) >> 8) + angle;
				angle += 0x200;
				mi->pos[0] = (int16_t)(ComputeCos(a) >> 1);
				mi->pos[2] = (int16_t)(ComputeSin(a) >> 1);
				mi->pos[1] = 0;
				mi->scale = (int16_t)((CrtRand() >> 4) + 0x1000);
				NormalizeVector16(mi->pos, mi->vel);
				mi->pos[0] = (int16_t)(mi->pos[0] + f->pos[0]);
				mi->pos[2] = (int16_t)(mi->pos[2] + f->pos[2]);
				int32_t r = CrtRand() & 0x1FF;
				mi->vel[0] = (int16_t)(mi->vel[0] >> 5);
				mi->vel[2] = (int16_t)(mi->vel[2] >> 5);
				mi->vel[1] = -100;
				mi->pos[1] = (int16_t)(f->pos[1] - r - 0x200);
				mi->acc[0] = (int16_t)(-ComputeSin(f->angle) >> 8);
				mi->acc[2] = (int16_t)(ComputeCos(f->angle) >> 8);
				mi->acc[1] = 10;
			}
		}
		if (c == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(f->entity, pos);
			BdPlaySE3D(SOUND_Blizzaga, 0x101, pos);
		}
		if (c == 0x44) ApplyActionResultToTarget(Ctx()->actions[f->action].targets);
		if (c >= 0x62) return TASK_END;
		f->counter = (int16_t)(c + 1);
		return 0;
	}

	// ------------------------------------------------------------------
	// Trails (0x6D99D0 push, 0x6D9A40 draw)
	// ------------------------------------------------------------------
	// a new position in front (the others move back, at most `max`)
	static int32_t TrailPush(int32_t max, int32_t n, TrailEntry *t, const int16_t *a, const int16_t *b, uint8_t fade, uint8_t fade_vel, uint8_t u)
	{
		if (n < max)
		{
			uint32_t *d = (uint32_t *)t;
			for (int32_t k = n * 9; k > 0; k--) d[k + 8] = d[k - 1];
			n++;
		}
		memcpy(t[0].v0, a, 8);
		memcpy(t[0].v1, b, 8);
		t[0].fade = fade;
		t[0].fade_vel = (int8_t)fade_vel;
		t[0].u = u;
		return n;
	}

	// every position fades by fade_vel (accelerated by d each tick); the strip between two
	// positions is a semi-transparent Gouraud quad (colour = fade) with depth keys
	static uint32_t TrailDraw(int32_t *pn, TrailEntry *t, int8_t d, uint32_t ot, int32_t zbias, int32_t shift, uint32_t cursor)
	{
		int32_t n = *pn;
		zbias <<= 2;
		for (int32_t k = 0; k < n; k++)
		{
			int8_t v = (int8_t)(t[k].fade_vel + d);
			t[k].fade_vel = v;
			int32_t f = (v >> 3) + t[k].fade;
			t[k].fade = (uint8_t)(f < 0 ? 0 : f);
		}
		if (n >= 2)
		{
			if (!t[n - 1].fade && !t[n - 2].fade) n--;
			if (n >= 2)
			{
				for (int32_t k = 0; k < n; k++)
				{
					GteLoadV0(t[k].v0);
					GteRTPS();
					GteReadSXY2(&t[k].sxy0);
					GteReadOTZ(&t[k].otz0);
					GteLoadV0(t[k].v1);
					GteRTPS();
					GteReadSXY2(&t[k].sxy1);
					GteReadOTZ(&t[k].otz1);
				}
				shift += 2;
				uint8_t *p = (uint8_t *)cursor;
				for (int32_t k = 0; k < n - 1; k++)
				{
					TrailEntry &a = t[k], &b = t[k + 1];
					if (a.otz0 <= 0 || a.otz1 <= 0 || b.otz0 <= 0 || b.otz1 <= 0) continue;
					uint8_t *q = p;
					p += 0x34;
					uint32_t u = a.u;
					*(uint32_t *)(q + 0x18) = u | 0xB87F00;
					*(uint32_t *)(q + 0xC) = u | 0x3E144000;
					*(uint32_t *)(q + 8) = a.sxy0;
					*(uint32_t *)(q + 0x14) = a.sxy1;
					int32_t mac1, mac2;
					GteSetSXY012(a.sxy0, a.sxy1, b.sxy0);
					GteNCLIP();
					GteReadMAC0(&mac1);
					GteSetSXY012(a.sxy1, b.sxy1, b.sxy0);
					GteNCLIP();
					GteReadMAC0(&mac2);
					uint32_t s1 = mac1 > 0, s2 = mac2 > 0;
					uint32_t u2 = a.u + 0x1F;
					if (s1 == s2)
					{
						*(uint32_t *)(q + 0x20) = b.sxy0;
						*(uint32_t *)(q + 0x2C) = b.sxy1;
						*(uint32_t *)(q + 0x24) = u2 | 0x4000;
						u2 |= 0x7F00;
					}
					else
					{
						*(uint32_t *)(q + 0x20) = b.sxy1;
						*(uint32_t *)(q + 0x2C) = b.sxy0;
						*(uint32_t *)(q + 0x24) = u2 | 0x7F00;
						u2 |= 0x4000;
					}
					*(uint32_t *)(q + 0x30) = u2;
					uint32_t ca = a.fade * 0x10101u;
					*(uint32_t *)(q + 0x10) = ca;
					*(uint32_t *)(q + 4) = ca;
					*(uint32_t *)q = 0x0C000000;
					q[7] = 0x3E;
					uint32_t cb = b.fade * 0x10101u;
					*(uint32_t *)(q + 0x28) = cb;
					*(uint32_t *)(q + 0x1C) = cb;
					int32_t z = (a.otz0 + a.otz1 + b.otz0 + b.otz1) >> shift;
					InsertPrimDepthKeys(ot + 4 * z, q, zbias + 4 * a.otz0, zbias + 4 * a.otz1, zbias + 4 * b.otz0, zbias + 4 * b.otz1);
				}
				cursor = (uint32_t)p;
			}
		}
		*pn = n;
		return cursor;
	}

	// 0x6D9CF0
	static void RibbonPoint(int16_t *out, int16_t angle, int32_t z, int32_t r)
	{
		out[0] = (int16_t)(mul32(ComputeCos(angle), r) >> 14);
		out[1] = (int16_t)(mul32(ComputeSin(angle), r) >> 14);
		out[2] = (int16_t)((z >> 2) - z);
	}

	// 0x6D9D40
	static void SpawnSparkle(const int16_t *v, Mat4x3 *m)
	{
		SparkleNode *s = (SparkleNode *)AddTaskToQueue(Queue(3), ORIG_SparkleTask);
		if (!s) return;
		FillDwords(&s->counter, 0, 8);
		s->pos[0] = v[0];
		s->pos[1] = v[1];
		s->pos[2] = v[2];
		s->vel[0] = (int16_t)(v[0] >> 4);
		s->vel[1] = (int16_t)(v[1] >> 4);
		s->acc[0] = (int16_t)(v[0] >> 8);
		s->vel[2] = (int16_t)(v[2] >> 4);
		s->acc[1] = (int16_t)(v[1] >> 8);
		s->acc[2] = (int16_t)(v[2] >> 8);
		s->m = m;
	}

	// ------------------------------------------------------------------
	// Ribbon (0x6D97A0): a trail of two points waving around the target (swinging angle,
	// width shrinking until 20), facing the camera; throws two sparkles per tick
	// ------------------------------------------------------------------
	static void RibbonDraw(RibbonNode *r)
	{
		int32_t v[3] = { -r->m.t[0], -r->m.t[1], r->m.t[2] };
		BuildOrthonormalBasis(v, &r->m);
		GteSetRotMatrix(&r->m);
		GteSetTransVector(&r->m);
		PacketCursor() = TrailDraw(&r->count, r->trail, -8, RenderOT44(), 0x11, 2, PacketCursor());
	}

	static uint32_t __cdecl RibbonTask(TaskNode *n)
	{
		RibbonNode *r = (RibbonNode *)n;
		int16_t pos[4];
		GetEffectSpawnPosition(r->entity, 0xF1, 0, pos);
		pos[1] = EntityHeight2(r->entity);
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(r->m.t);
		if (r->counter <= 0x14)
		{
			int32_t x = r->counter << 12;
			int32_t q = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 3;
			int32_t w = 0x1000 - (q + (int32_t)((uint32_t)q >> 31));
			int32_t z = mul32(r->m.t[2], w) >> 12;
			w += mul32(ComputeSin(w >> 1), r->wave) >> 12;
			int16_t a[4], b[4];
			a[3] = 0; // never written by the original (stack garbage in the trail's 4th words)
			b[3] = 0;
			RibbonPoint(a, r->angle, z, w);
			RibbonPoint(b, r->angle, z, w * 4);
			int16_t av = r->angle_vel;
			r->angle = (int16_t)(r->angle + av);
			r->angle_vel = (int16_t)(r->angle_acc + av);
			r->count = TrailPush(0x1E, r->count, r->trail, a, b, 0x10, 0x60, (uint8_t)((3 - (r->counter & 3)) << 5));
			if (r->count > 4)
			{
				int16_t s[4];
				memcpy(s, r->trail[3].v0, 8);
				s[0] = (int16_t)(s[0] + ((CrtRand() - 0x4000) >> 6));
				s[1] = (int16_t)(s[1] + ((CrtRand() - 0x4000) >> 6));
				SpawnSparkle(s, &r->m);
				memcpy(s, r->trail[3].v1, 8);
				s[0] = (int16_t)(s[0] + ((CrtRand() - 0x4000) >> 6));
				s[1] = (int16_t)(s[1] + ((CrtRand() - 0x4000) >> 6));
				SpawnSparkle(s, &r->m);
			}
		}
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_trail(ORIG_RibbonTask, r);)
		RibbonDraw(r);
		r->counter++;
		if (r->counter > 8 && r->count < 2) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Spiral (0x6DA0D0): a trail of two points circling the target, lowering; its model matrix
	// spins (yaw) and, once the block is out (freeze counter 28), grows
	// ------------------------------------------------------------------
	static void SpiralDraw(SpiralNode *s)
	{
		Mat4x3 m = {};
		YawScaleMatrix(s->yaw, &m, s->scale, (int16_t)((s->scale >> 2) + 0xC00));
		int16_t pos[4];
		GetEffectSpawnPosition(s->entity, 0xF1, 0, pos);
		m.t[0] = pos[0];
		m.t[2] = pos[2];
		m.t[1] = EntityHeight2(s->entity);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PacketCursor() = TrailDraw(&s->count, s->trail, -4, RenderOT44(), 0x11, 2, PacketCursor());
	}

	static uint32_t __cdecl SpiralTask(TaskNode *n)
	{
		SpiralNode *s = (SpiralNode *)n;
		if (s->counter <= 0x14)
		{
			int32_t x = s->counter << 12;
			int32_t q = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 3;
			int32_t w = q + (int32_t)((uint32_t)q >> 31);
			int16_t a[4], b[4];
			int16_t sn = (int16_t)(ComputeSin(s->angle) >> 1);
			a[0] = sn;
			b[0] = sn;
			int16_t cs = (int16_t)(ComputeCos(s->angle) >> 1);
			a[2] = cs;
			b[2] = cs;
			s->angle = (int16_t)(s->angle + s->angle_vel);
			a[1] = (int16_t)(-w >> 2);
			int16_t c = s->counter;
			if (c < 8) b[1] = (int16_t)(a[1] - ((c * 300) >> 3) - 0x12);
			else if (c <= 0xC) b[1] = (int16_t)(a[1] - 300);
			else b[1] = (int16_t)(a[1] - (((0x14 - c) * 300) >> 3) - 0x12);
			a[3] = 0; // never written by the original (stack garbage in the trail's 4th words)
			b[3] = 0;
			s->count = TrailPush(0x1E, s->count, s->trail, b, a, 0x10, 0x58, (uint8_t)((3 - (c & 3)) << 5));
		}
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_trail(ORIG_SpiralTask, s);)
		SpiralDraw(s);
		s->yaw = (int16_t)(s->yaw + s->yaw_vel);
		int16_t fc = s->freeze->counter;
		if (fc >= 0x1C)
		{
			if (fc == 0x1C)
			{
				s->scale = 0x1000;
				s->scale_vel = 0x1F4;
				s->scale_acc = -32;
			}
			int16_t v = s->scale_vel;
			s->scale = (int16_t)(s->scale + v);
			v = (int16_t)(s->scale_acc + v);
			s->scale_vel = v;
			if (v < 0x40) s->scale_vel = 0x40;
		}
		s->counter++;
		if (s->counter > 8 && s->count < 2) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Sparkle (0x6D9DD0): sequence 0x1312690 frame counter, moving in its ribbon's frame
	// ------------------------------------------------------------------
	static void SparkleDraw(const SparkleNode *s)
	{
		uint8_t *h = AllocHeader(0xB4);
		GteSetRotMatrix(s->m);
		GteSetTransVector(s->m);
		GteLoadV0(s->pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((void *)(SPARKLE_MATRIX + 0x14));
		GteSetRotMatrix((const Mat4x3 *)SPARKLE_MATRIX);
		GteSetTransVector((const Mat4x3 *)SPARKLE_MATRIX);
		*(const void **)h = SEQ_Mist;
		*(uint16_t *)(h + 4) = (uint16_t)s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void SparkleUpdate(SparkleNode *s)
	{
		for (int k = 0; k < 3; k++) s->vel[k] = (int16_t)(s->vel[k] + s->acc[k]);
		for (int k = 0; k < 3; k++) s->pos[k] = (int16_t)(s->pos[k] + (int16_t)(s->vel[k] >> 4));
		s->counter++;
	}

	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_draw(ORIG_SparkleTask, s);)
		SparkleDraw(s);
		SparkleUpdate(s);
		return s->counter < 0xA ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Mist (0x6D95E0) and Flake (0x6D96A0): sequences 0x1312690 / 0x13127A0 frame counter
	// ------------------------------------------------------------------
	static void MistDraw(const MistNode *m)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(m->pos, m->scale, -(m->scale >> 2));
		*(uint16_t *)(h + 0x24) = 0;
		*(const void **)h = SEQ_Mist;
		*(uint16_t *)(h + 4) = (uint16_t)m->counter;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void MistUpdate(MistNode *m)
	{
		for (int k = 0; k < 3; k++) m->vel[k] = (int16_t)(m->vel[k] + m->acc[k]);
		m->pos[1] = (int16_t)(m->pos[1] + m->vel[1]);
		m->pos[2] = (int16_t)(m->pos[2] + m->vel[2]);
		m->pos[0] = (int16_t)(m->pos[0] + m->vel[0]);
	}

	static uint32_t __cdecl MistTask(TaskNode *n)
	{
		MistNode *m = (MistNode *)n;
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_draw(ORIG_MistTask, m);)
		MistDraw(m);
		m->counter++;
		if (m->counter >= 0xA) return TASK_END;
		MistUpdate(m);
		return 0;
	}

	static void FlakeDraw(const FlakeNode *fl)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(fl->pos, fl->scale, -(fl->scale >> 2));
		*(uint16_t *)(h + 0x24) = 0;
		*(const void **)h = SEQ_Flake;
		*(uint16_t *)(h + 4) = (uint16_t)fl->counter;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// blown away from the caster once the block breaks (freeze counter 38)
	static void FlakeUpdate(FlakeNode *fl)
	{
		if (!fl->blown && fl->freeze->counter >= 0x26)
		{
			fl->blown = 1;
			fl->acc[0] = (int16_t)(-ComputeSin(fl->freeze->angle) >> 9);
			fl->acc[1] = -10;
			fl->acc[2] = (int16_t)(ComputeCos(fl->freeze->angle) >> 9);
		}
		for (int k = 0; k < 3; k++) fl->vel[k] = (int16_t)(fl->vel[k] + fl->acc[k]);
		fl->pos[1] = (int16_t)(fl->pos[1] + fl->vel[1]);
		fl->pos[2] = (int16_t)(fl->pos[2] + fl->vel[2]);
		fl->pos[0] = (int16_t)(fl->pos[0] + fl->vel[0]);
	}

	static uint32_t __cdecl FlakeTask(TaskNode *n)
	{
		FlakeNode *fl = (FlakeNode *)n;
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(held_note_draw(ORIG_FlakeTask, fl);)
		FlakeDraw(fl);
		fl->counter++;
		if (fl->counter >= 0xE) return TASK_END;
		FlakeUpdate(fl);
		return 0;
	}
}

	void register_mag104_blizzaga()
	{
		register_port(blizzaga104::ORIG_Director, (void *)blizzaga104::DirectorTask, "B104 Director", 104);
		register_port(blizzaga104::ORIG_FreezeTask, (void *)blizzaga104::FreezeTask, "B104 FreezeTask", 104);
		register_port(blizzaga104::ORIG_MistTask, (void *)blizzaga104::MistTask, "B104 MistTask", 104);
		register_port(blizzaga104::ORIG_FlakeTask, (void *)blizzaga104::FlakeTask, "B104 FlakeTask", 104);
		register_port(blizzaga104::ORIG_RibbonTask, (void *)blizzaga104::RibbonTask, "B104 RibbonTask", 104);
		register_port(blizzaga104::ORIG_SparkleTask, (void *)blizzaga104::SparkleTask, "B104 SparkleTask", 104);
		register_port(blizzaga104::ORIG_SpiralTask, (void *)blizzaga104::SpiralTask, "B104 SpiralTask", 104);
		// 30 fps layer: see mag104_blizzaga_held.inc
		FX_HELD(register_mag104_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag104_blizzaga_held.inc"
#endif
