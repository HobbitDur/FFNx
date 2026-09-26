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

// Effect 103: Blizzara (spell, MAG_103_*).
//
// Structure (setup MAG_103_BLIZZARA 0x6DA300 -> Init 0x6DA310, file loader 0x6DA2E0 = mag102.tim):
//   Director (0x6DA3A0) - the root task: alternates the packet arena (magic buffer + 0x196D4 /
//     + 0x296D4); at its counter 1 (a 20-tick cycle) builds five task pools in the magic buffer
//     and starts the next action's ice block on its first target (at most 3 at a time, one per
//     target, a target keeps one block until its counter passes 60); runs the five queues and
//     ends when they are all empty.
//   IceBlock (0x6DAA30, 0x53CC-byte node) - a copy of the ice block model (0x131ABB0, second
//     UV model 0x131C86C) with private vertices per triangle: 0..12 four ice pillars (every 4
//     ticks), 6 sound, 20 burst, 20..30 the block grows out of the ground (y scale 0..1 over 3
//     ticks) with falling chunks, 20..40 mist, 30..70 the block breaks from the top: every
//     triangle whose vertices are above a rising plane becomes a shard (random spin, velocity
//     away from the centre, gravity) fading out; 60 damage; ends after 70.
//   Pillar (0x6DBB20) and Burst (0x6DBD20) - prim models; Chunk (0x6DBEA0) and Mist (0x6DBFF0) -
//     sprite sequences.
// Module globals: 0x2543E90..0x2543F18 (file, texture pointer, context, packet cursor
// 0x2543EB4, the six queues); every node pool and the model copies live in the magic buffer.
// None of these tasks tests the battle's draw-only flag.

#include "mag_ice_common.h"

namespace ff8fx
{
namespace blizzara103
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline uint8_t *&TexBase() { return var<uint8_t *>(0x2543E94); }       // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2543E98); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2543EB4); }
	inline TaskQueue *Queue(int i) { return (TaskQueue *)(0x2543EC8 + 0x10 * i); } // 0 blocks, 1 pillars, 2 bursts, 3 chunks, 4 mist

	static const uint32_t ORIG_Director = 0x6DA3A0;
	static const uint32_t ORIG_IceBlockTask = 0x6DAA30;
	static const uint32_t ORIG_PillarTask = 0x6DBB20;
	static const uint32_t ORIG_BurstTask = 0x6DBD20;
	static const uint32_t ORIG_ChunkTask = 0x6DBEA0;
	static const uint32_t ORIG_MistTask = 0x6DBFF0;
	static const uint8_t *const MODEL_Block = (const uint8_t *)0x131ABB0;   // 0x72F dwords
	static const uint8_t *const MODEL_BlockUV = (const uint8_t *)0x131C86C; // 0x465 dwords
	static const void *const MODEL_Pillar = (const void *)0x1319990;
	static const void *const MODEL_Burst = (const void *)0x131A678;
	static const void *const SEQ_Ice = (const void *)0x13197E4;
	static const void *const SOUND_Blizzara = (const void *)0x131E750;
	static const uint32_t MODEL_OFFSET[3] = { 0x10BE4, 0x13A34, 0x16884 };   // per action, in the magic buffer
	static const uint32_t MODEL_UV_OFFSET[3] = { 0x128A0, 0x156F0, 0x18540 };
	static const int SHARDS = 0xDF;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline int16_t EntityHeight2(const uint8_t *entity) { return *(const int16_t *)(entity + 0x24); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C cycles 0..19
		uint8_t action;    // +0x0E next action
		uint8_t pools;     // +0x0F pools built
		uint32_t arena;    // +0x10 packet arena toggle
	};
	// one triangle of the block turned into a shard (0x48 bytes)
	struct Shard
	{
		int16_t pos[3];    // +0x00 centre
		int16_t active;    // +0x06
		int16_t vel[3];    // +0x08
		int16_t pad0E;
		int16_t angle[3];  // +0x10
		int16_t pad16;
		int16_t spin[3];   // +0x18
		int16_t pad1E;
		int16_t v[3][4];   // +0x20 vertices around the centre
		uint8_t fade[4][4];// +0x38 colour steps: triangle colours 0..2, second UV model colour
	};
	struct IceBlockNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E
		uint8_t *entity;   // +0x10 target
		uint8_t *model;    // +0x14 model copy
		uint8_t *model2;   // +0x18 second UV model copy
		int16_t pos[3];    // +0x1C x, ground height, z
		uint8_t pad22[4];
		int16_t clip_y;    // +0x26 pillars stop rising below it (block top)
		uint8_t used[4];   // +0x28 pillar slots taken
		Shard shards[SHARDS]; // +0x2C
		int16_t verts[SHARDS * 3][4]; // +0x3EE4 private vertices of the model copy
	};
	struct PillarNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		IceBlockNode *block; // +0x10
		int16_t pos[3];    // +0x14
		int16_t rise;      // +0x1A
		int16_t yaw;       // +0x1C
		int16_t yaw_vel;   // +0x1E
		int16_t pad20;
		int16_t scale;     // +0x22
		int16_t rise_acc;  // +0x24
		uint8_t stopped;   // +0x26
		uint8_t pad27;
	};
	struct BurstNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		uint8_t pad16[4];
		int16_t scale;     // +0x1A
		int16_t vel;       // +0x1C
		int16_t acc;       // +0x1E
	};
	struct ChunkNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *entity;   // +0x10
		int16_t pos[3];    // +0x14
		int16_t scale;     // +0x1A
		int16_t vel[3];    // +0x1C (y = fall speed)
		int16_t bounce;    // +0x22 (-1 = not bounced yet)
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
		int16_t acc_x;     // +0x20
		int16_t pad22;
		int16_t acc_z;     // +0x24
		int16_t pad26;
	};
#pragma pack(pop)
	static_assert(sizeof(DirectorNode) == 0x14 && sizeof(Shard) == 0x48 && sizeof(IceBlockNode) == 0x53CC && sizeof(PillarNode) == 0x28
		&& sizeof(BurstNode) == 0x20 && sizeof(ChunkNode) == 0x24 && sizeof(MistNode) == 0x28, "Blizzara node layouts");
}
}

#ifdef FF8_FX_HELD
#include "mag103_blizzara_held.h"
#endif

namespace ff8fx
{
namespace blizzara103
{
	// ------------------------------------------------------------------
	// Director (0x6DA3A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(held_note_root();)
		if (d->arena)
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x196D4);
			d->arena = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x296D4);
			d->arena = 1;
		}
		if (d->counter == 1)
		{
			if (!d->pools)
			{
				d->pools = 1;
				InitTaskQueuePool(Queue(0), TexBase(), 0x53CC, 3);
				InitTaskQueuePool(Queue(1), TexBase() + 0xFB64, 0x28, 0xC);
				InitTaskQueuePool(Queue(2), TexBase() + 0xFD44, 0x20, 1);
				InitTaskQueuePool(Queue(3), TexBase() + 0xFD64, 0x24, 0x20);
				InitTaskQueuePool(Queue(4), TexBase() + 0x101E4, 0x28, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (d->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[d->action].targets[0]);
				bool busy = false;
				uint8_t *slot = TexBase();
				for (int i = 0; i < 3; i++, slot += 0x53CC)
				{
					IceBlockNode *b = (IceBlockNode *)slot;
					if ((b->hdr.flags & 1) && b->entity == entity && b->counter <= 0x3C)
					{
						busy = true;
						break;
					}
				}
				if (busy) d->counter = 0;
				else
				{
					IceBlockNode *b = (IceBlockNode *)AddTaskToQueue(Queue(0), ORIG_IceBlockTask);
					FillDwords(&b->counter, 0, 0x14F0);
					int16_t action = (int16_t)(uint16_t)d->action;
					b->action = action;
					b->entity = entity;
					b->clip_y = 0x7FFF;
					if (action >= 0 && action < 3)
					{
						CopyDwords(TexBase() + MODEL_OFFSET[action], MODEL_Block, 0x72F);
						CopyDwords(TexBase() + MODEL_UV_OFFSET[action], MODEL_BlockUV, 0x465);
						b->model = TexBase() + MODEL_OFFSET[action];
						b->model2 = TexBase() + MODEL_UV_OFFSET[action];
					}
					uint8_t *prim = ModelList(6, b->model, nullptr);
					uint8_t *uv = ModelList(2, b->model2, nullptr);
					for (int i = 0; i < SHARDS; i++, prim += 0x1C, uv += 0x14)
					{
						b->shards[i].active = 0;
						prim[0x17] = 0;
						prim[0x1B] = 0;
						uv[2] = 0xFF;
						uv[1] = 0xFF;
						uv[0] = 0xFF;
					}
					uint8_t *h = (uint8_t *)FieldAlloc(0x58);
					*(uint32_t *)(h + 0x1C) = 0xC;
					Cursor(h, 0) = b->model;
					PrepareModel(h, b->model, b->verts[0]);
					FieldFree(0x58);
					d->action++;
				}
			}
		}
		int r0, r1, r2, r3, r4;
		if (d->pools)
		{
			r0 = ExecuteTaskQueue(Queue(0));
			r1 = ExecuteTaskQueue(Queue(1));
			r2 = ExecuteTaskQueue(Queue(2));
			r3 = ExecuteTaskQueue(Queue(3));
			r4 = ExecuteTaskQueue(Queue(4));
		}
		else r0 = r1 = r2 = r3 = r4 = (int)(uint32_t)n;
		if (d->pools && !r0 && !r1 && !r2 && !r3 && !r4) return TASK_END;
		d->counter++;
		if (d->counter >= 0x14) d->counter = 0;
		return 0;
	}

	// block renderer (0x6DB470 -> list 6 by 0x6DB5D0, see mag_ice_common.h)
	static uint32_t RenderPolys103(uint8_t *h, uint32_t ot, int shift, uint32_t cursor) { return RenderPolys(h, ot, shift, cursor, true); }
	static uint32_t RenderBlock(uint8_t *h, uint32_t ot, int shift, uint32_t cursor) { return RenderModel(h, ot, shift, cursor, RenderPolys103); }

	// MAG_103_sub_6DC240: colour c (3 bytes) down by step d, clamped at 0; true when all 3 are 0
	static bool FadeColor(uint8_t *c, const uint8_t *d)
	{
		bool gone = true;
		for (int k = 0; k < 3; k++)
		{
			int32_t v = (int32_t)c[k] - (int32_t)d[k];
			if (v > 0)
			{
				c[k] = (uint8_t)v;
				gone = false;
			}
			else c[k] = 0;
		}
		return gone;
	}

	// the shard's three vertices in the vertex buffer: rotated by its angles, at its centre
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

	// ------------------------------------------------------------------
	// Shard update (0x6DC0A0): every active shard fades (ends when its three colours are black),
	// falls (gravity 5), moves and spins; its three vertices are rewritten in the vertex buffer
	// ------------------------------------------------------------------
	static void UpdateShards(Shard *s, int16_t (*vbuf)[4], uint8_t *prim, uint8_t *ext)
	{
		for (int i = 0; i < SHARDS; i++, s++, prim += 0x1C, ext += 0x14)
		{
			if (!s->active) continue;
			if (FadeColor(prim, s->fade[0]) && FadeColor(prim + 0x14, s->fade[1]) && FadeColor(prim + 0x18, s->fade[2]))
			{
				s->active = 0;
				continue;
			}
			FadeColor(ext, s->fade[3]);
			int32_t g = (int32_t)prim[0x17] - 0x20;
			prim[0x17] = (uint8_t)(g > 0 ? g : 0);
			s->vel[1] = (int16_t)(s->vel[1] - 5);
			s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
			s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
			s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
			s->angle[1] = (int16_t)(s->angle[1] + s->spin[1]);
			s->angle[0] = (int16_t)(s->angle[0] + s->spin[0]);
			s->angle[2] = (int16_t)(s->angle[2] + s->spin[2]);
			PoseShard(s, vbuf, prim);
		}
	}

	// the block's model matrix: x / z mirrored, y scaled, at the target's ground
	static void BlockMatrix(const IceBlockNode *b, int16_t scale_y)
	{
		Mat4x3 m = {};
		m.m[0][0] = -0x1000;
		m.m[1][1] = scale_y;
		m.m[2][2] = -0x1000;
		m.t[0] = b->pos[0];
		m.t[1] = b->pos[1];
		m.t[2] = b->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	static void BlockHeader(uint8_t *h, const IceBlockNode *b, int16_t counter, uint32_t flags)
	{
		Cursor(h, 0) = b->model;
		Cursor(h, 4) = b->model2;
		*(const void **)(h + 8) = b->verts;
		*(uint16_t *)(h + 0x1C) = (uint16_t)((counter & 0x1F) << 1);
		*(uint16_t *)(h + 0x2A) = 0;
		*(uint16_t *)(h + 0x1E) = (uint16_t)((counter & 0xF) << 3);
		*(uint16_t *)(h + 0x28) = 0;
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x26) = 0x100;
		*(uint16_t *)(h + 0x24) = 0x100;
		*(uint16_t *)(h + 0x2C) = 0x40;
		*(uint16_t *)(h + 0x2E) = 0x80;
		*(uint32_t *)(h + 0x18) = flags;
	}

	// x / 3 (0x55555556)
	static int32_t Div3(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x55555556) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// ------------------------------------------------------------------
	// Ice block (0x6DAA30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl IceBlockTask(TaskNode *n)
	{
		IceBlockNode *b = (IceBlockNode *)n;
		if (b->counter == 0)
		{
			GetEffectSpawnPosition(b->entity, 0xF1, 0, b->pos);
			b->pos[1] = EntityHeight2(b->entity);
		}
		if (b->counter >= 0 && b->counter <= 0xC)
		{
			int32_t c = b->counter;
			if (c == 6) BdPlaySE3D(SOUND_Blizzara, 0x101, b->pos);
			if (!(c & 3))
			{
				PillarNode *p = (PillarNode *)AddTaskToQueue(Queue(1), ORIG_PillarTask);
				FillDwords(&p->counter, 0, 7);
				p->block = b;
				GetEffectSpawnPosition(b->entity, 0xF1, 0, p->pos);
				int32_t i = shl32(CrtRand(), 2) >> 15;
				while (b->used[i]) i = shl32(CrtRand(), 2) >> 15;
				b->used[i] = 1;
				p->yaw_vel = (i & 1) ? 0x1F : (int16_t)0xFFE1;
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(((i << 10) - 0x5FF) >> 1));
				int32_t a = i < 0 ? -i : i;
				int32_t s = ComputeSin((a << 11) / 4 + 0x100);
				int32_t x = shl32(s, 10);
				int32_t hi = (int32_t)(((int64_t)x * 0x5F533929) >> 32) >> 12;
				hi += (int32_t)((uint32_t)hi >> 31);
				p->rise_acc = 0;
				p->scale = (int16_t)hi;
				p->rise = 6;
			}
		}
		if (b->counter == 0x14)
		{
			BurstNode *u = (BurstNode *)AddTaskToQueue(Queue(2), ORIG_BurstTask);
			FillDwords(&u->counter, 0, 5);
			GetEffectSpawnPosition(b->entity, 0xF1, 0, u->pos);
			u->scale = 0x64;
			u->pos[1] = EntityHeight2(b->entity);
			u->vel = 0xC3;
			u->acc = (int16_t)0xFFF0;
		}
		int32_t grow;
		if (b->counter >= 0x14 && b->counter <= 0x1E)
		{
			int32_t t = b->counter - 0x14;
			// 30 fps layer: see mag103_blizzara_held.inc
			FX_HELD(held_note_block(b, false);)
			uint8_t *h = (uint8_t *)FieldAlloc(0x64);
			int16_t sy;
			if (t < 3)
			{
				sy = (int16_t)Div3(t << 12);
				grow = sy;
			}
			else
			{
				sy = 0x1000;
				grow = 0x1000;
			}
			BlockMatrix(b, sy);
			BlockHeader(h, b, b->counter, 0x6600C);
			PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
			FieldFree(0x64);
			int32_t x = grow * 3400;
			int32_t q = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 2;
			q += (int32_t)((uint32_t)q >> 31);
			b->clip_y = (int16_t)(EntityHeight2(b->entity) - (int16_t)q);
		}
		else grow = 0x1000;
		if (b->counter >= 0x1E && b->counter <= 0x46)
		{
			int32_t t = b->counter - 0x1E;
			uint8_t *prims = ModelList(6, b->model, nullptr);
			uint8_t *uvs = ModelList(2, b->model2, nullptr);
			if (t <= 0x14)
			{
				int32_t x = t * 3400;
				int32_t q = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 3;
				int32_t level = q + (int32_t)((uint32_t)q >> 31) - 3400; // the plane rises from 3400 above the ground
				Shard *s = b->shards;
				uint8_t *prim = prims, *uv = uvs;
				for (int i = 0; i < SHARDS; i++, s++, prim += 0x1C, uv += 0x14)
				{
					if (s->active) continue;
					int16_t *va = b->verts[*(uint16_t *)(prim + 4) >> 1];
					int16_t *vb = b->verts[*(uint16_t *)(prim + 6) >> 1];
					int16_t *vc = b->verts[*(uint16_t *)(prim + 8) >> 1];
					if (va[1] > level || vb[1] > level || vc[1] > level) continue;
					int16_t cx = (int16_t)Div3(va[0] + vc[0] + vb[0]);
					s->pos[0] = cx;
					int16_t cy = (int16_t)Div3(vc[1] + vb[1] + va[1]);
					s->pos[1] = cy;
					int16_t cz = (int16_t)Div3(va[2] + vc[2] + vb[2]);
					s->active = 1;
					s->pos[2] = cz;
					s->vel[2] = (int16_t)(-(cz * cy) >> 15);
					s->vel[0] = (int16_t)(-(cy * cx) >> 15);
					s->vel[1] = 0;
					s->angle[2] = 0;
					s->angle[1] = 0;
					s->angle[0] = 0;
					s->spin[0] = (int16_t)((CrtRand() - 0x4000) >> 7);
					s->spin[1] = (int16_t)((CrtRand() - 0x4000) >> 7);
					s->spin[2] = (int16_t)((CrtRand() - 0x4000) >> 7);
					*(uint32_t *)prim |= 0x02000000;
					prim[0x17] = 0xFF;
					prim[0x1B] = 0;
					s->fade[0][0] = (uint8_t)((prim[0] + 15) >> 3);
					s->fade[0][1] = (uint8_t)((prim[1] + 15) >> 3);
					s->fade[0][2] = (uint8_t)((prim[2] + 15) >> 3);
					s->fade[1][0] = (uint8_t)((prim[0x14] + 15) >> 3);
					s->fade[1][1] = (uint8_t)((prim[0x15] + 15) >> 3);
					s->fade[1][2] = (uint8_t)((prim[0x16] + 15) >> 3);
					s->fade[2][0] = (uint8_t)((prim[0x18] + 15) >> 3);
					s->fade[2][1] = (uint8_t)((prim[0x19] + 15) >> 3);
					s->fade[2][2] = (uint8_t)((prim[0x1A] + 15) >> 3);
					s->fade[3][0] = (uint8_t)((uv[0] + 15) >> 2);
					s->fade[3][1] = (uint8_t)((uv[1] + 15) >> 2);
					s->fade[3][2] = (uint8_t)((uv[2] + 15) >> 2);
					s->v[0][0] = (int16_t)(va[0] - cx);
					s->v[0][1] = (int16_t)(va[1] - cy);
					s->v[0][2] = (int16_t)(va[2] - cz);
					s->v[1][0] = (int16_t)(vb[0] - cx);
					s->v[1][1] = (int16_t)(vb[1] - cy);
					s->v[1][2] = (int16_t)(vb[2] - cz);
					s->v[2][0] = (int16_t)(vc[0] - cx);
					s->v[2][1] = (int16_t)(vc[1] - cy);
					s->v[2][2] = (int16_t)(vc[2] - cz);
				}
			}
			UpdateShards(b->shards, b->verts, prims, uvs);
			// 30 fps layer: see mag103_blizzara_held.inc
			FX_HELD(held_note_block(b, true);)
			uint8_t *h = (uint8_t *)FieldAlloc(0x64);
			BlockMatrix(b, 0x1000);
			BlockHeader(h, b, b->counter, 0x660C0);
			PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
			FieldFree(0x64);
		}
		if (b->counter >= 0x14 && b->counter <= 0x1E)
		{
			int32_t angle = CrtRand();
			for (int k = 0; k < 2; k++)
			{
				ChunkNode *c = (ChunkNode *)AddTaskToQueue(Queue(3), ORIG_ChunkTask);
				if (!c) continue;
				FillDwords(&c->counter, 0, 6);
				c->entity = b->entity;
				int32_t r1 = CrtRand();
				int32_t r2 = CrtRand();
				int32_t a = ((r1 + r2 - 0x8000) >> 8) + angle;
				angle += 0x800;
				c->pos[0] = (int16_t)(ComputeCos(a) >> 3);
				c->pos[2] = (int16_t)(ComputeSin(a) >> 3);
				c->pos[1] = 0;
				c->scale = (int16_t)(0x1000 - (CrtRand() >> 4));
				NormalizeVector16(c->pos, c->vel);
				c->pos[0] = (int16_t)(c->pos[0] + b->pos[0]);
				c->pos[2] = (int16_t)(c->pos[2] + b->pos[2]);
				int16_t y = (int16_t)((CrtRand() >> 3) - (grow >> 1) - 0x400);
				c->pos[1] = y;
				if (y > -1000) c->pos[1] = -1000;
				c->vel[1] = 0x20;
				c->pos[1] = (int16_t)(c->pos[1] + EntityHeight2(b->entity));
				c->bounce = -1;
				c->vel[0] = (int16_t)(c->vel[0] >> 6);
				c->vel[2] = (int16_t)(c->vel[2] >> 6);
			}
		}
		if (b->counter >= 0x14 && b->counter <= 0x28)
		{
			int32_t angle = CrtRand();
			for (int k = 0; k < 4; k++)
			{
				MistNode *m = (MistNode *)AddTaskToQueue(Queue(4), ORIG_MistTask);
				if (!m) continue;
				FillDwords(&m->counter, 0, 7);
				int32_t r1 = CrtRand();
				int32_t r2 = CrtRand();
				int32_t a = ((r1 + r2 - 0x8000) >> 8) + angle;
				angle += 0x400;
				m->pos[0] = (int16_t)(ComputeCos(a) >> 4);
				m->pos[2] = (int16_t)(ComputeSin(a) >> 4);
				m->pos[1] = 0;
				m->scale = (int16_t)(0x1000 - (CrtRand() >> 4));
				NormalizeVector16(m->pos, m->vel);
				m->pos[0] = (int16_t)(m->pos[0] + b->pos[0]);
				m->pos[2] = (int16_t)(m->pos[2] + b->pos[2]);
				m->pos[1] = b->pos[1];
				int32_t x = m->vel[0];
				int32_t q = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 2;
				int16_t vx = (int16_t)(q + (int32_t)((uint32_t)q >> 31));   // / 24
				m->vel[0] = vx;
				x = m->vel[2];
				q = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 2;
				int16_t vz = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
				m->vel[2] = vz;
				// - v / 14 (0x6DB6DB6D)
				x = vx;
				q = ((int32_t)(((int64_t)x * 0x6DB6DB6D) >> 32) - x) >> 3;
				m->acc_x = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
				x = vz;
				q = ((int32_t)(((int64_t)x * 0x6DB6DB6D) >> 32) - x) >> 3;
				m->acc_z = (int16_t)(q + (int32_t)((uint32_t)q >> 31));
			}
		}
		if (b->counter == 0x3C) ApplyActionResultToTarget(Ctx()->actions[b->action].targets);
		if (b->counter >= 0x46) return TASK_END;
		b->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Pillar (0x6DBB20): spinning ice column rising out of the ground (prim model 0x1319990),
	// fading in over 0..5 and out from 18; stops rising (braking) once it passes the block top
	// ------------------------------------------------------------------
	static int32_t Div6(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// MAG_103_sub_6DBCC0: yaw rotation scaled by s
	static void YawScaleMatrix(int16_t yaw, Mat4x3 *m, int16_t s)
	{
		int32_t sn = mul32(ComputeSin(yaw), s) >> 12;
		int32_t cs = mul32(ComputeCos(yaw), s) >> 12;
		m->m[0][2] = (int16_t)sn;
		m->m[2][0] = (int16_t)-sn;
		m->m[1][1] = s;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	static int32_t PillarFade(int16_t c)
	{
		if (c < 6) return 0x1000 - ComputeSin(Div6(c << 10));
		if (c >= 0x12)
		{
			int32_t q = (int32_t)(((int64_t)((c - 0x12) << 10) * 0x2AAAAAAB) >> 32) >> 1;
			return ComputeSin(q + (int32_t)((uint32_t)q >> 31)); // / 12
		}
		return 0;
	}

	static void PillarDraw(const PillarNode *p, uint8_t *h)
	{
		Mat4x3 m = {};
		YawScaleMatrix(p->yaw, &m, p->scale);
		m.t[0] = p->pos[0];
		m.t[1] = p->pos[1];
		m.t[2] = p->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(const void **)h = MODEL_Pillar;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
	}

	static void PillarUpdate(PillarNode *p)
	{
		if (!p->stopped && p->block->clip_y <= p->pos[1])
		{
			p->rise = 0xF0;
			p->rise_acc = (int16_t)0xFF80;
			p->stopped = 1;
		}
		p->rise_acc = (int16_t)(p->rise_acc + 8);
		if (p->rise_acc > 0) p->rise_acc = 0;
		p->rise = (int16_t)(p->rise + p->rise_acc);
		if (p->rise < 5) p->rise = 5;
		p->scale = (int16_t)(p->scale + p->rise);
	}

	static uint32_t __cdecl PillarTask(TaskNode *n)
	{
		PillarNode *p = (PillarNode *)n;
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(held_note_draw(ORIG_PillarTask, p);)
		uint8_t *h = AllocHeader(0x58);
		*(int32_t *)(h + 0xC) = 0;
		*(int32_t *)(h + 0xC) = PillarFade(p->counter);
		if (*(int32_t *)(h + 0xC) < 0x1000) PillarDraw(p, h);
		FieldFree(0x58);
		p->yaw = (int16_t)(p->yaw + p->yaw_vel);
		p->counter++;
		if (p->counter >= 0x1E) return TASK_END;
		PillarUpdate(p);
		return 0;
	}

	// ------------------------------------------------------------------
	// Burst (0x6DBD20): prim model 0x131A678 growing (decelerating), fading in 0..5, out from 14
	// ------------------------------------------------------------------
	static int32_t BurstFade(int16_t c)
	{
		if (c < 6) return 0x1000 - ComputeSin(Div6(c << 10));
		if (c >= 0xE) return ComputeSin(Div6((c - 0xE) << 10));
		return 0;
	}

	static void BurstDraw(const BurstNode *u, uint8_t *h)
	{
		Mat4x3 m = {};
		m.m[0][0] = u->scale;
		m.m[1][1] = u->scale;
		m.m[2][2] = u->scale;
		m.t[0] = u->pos[0];
		m.t[1] = u->pos[1];
		m.t[2] = u->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(const void **)h = MODEL_Burst;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
	}

	static void BurstUpdate(BurstNode *u)
	{
		u->vel = (int16_t)(u->vel + u->acc);
		if (u->vel < 0x1E) u->vel = 0x1E;
		u->scale = (int16_t)(u->scale + u->vel);
	}

	static uint32_t __cdecl BurstTask(TaskNode *n)
	{
		BurstNode *u = (BurstNode *)n;
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(held_note_draw(ORIG_BurstTask, u);)
		uint8_t *h = AllocHeader(0x58);
		*(int32_t *)(h + 0xC) = 0;
		*(int32_t *)(h + 0xC) = BurstFade(u->counter);
		if (*(int32_t *)(h + 0xC) < 0x1000) BurstDraw(u, h);
		FieldFree(0x58);
		u->counter++;
		if (u->counter >= 0x14) return TASK_END;
		BurstUpdate(u);
		return 0;
	}

	// ------------------------------------------------------------------
	// Chunk (0x6DBEA0): ice sequence 0x13197E4 frame counter & 15, falling until the ground
	// (speed grows by 4), then sliding outwards (a sixth of the fall speed, along its direction)
	// ------------------------------------------------------------------
	static void ChunkDraw(const ChunkNode *c)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(c->pos, c->scale, -(c->scale >> 2));
		*(uint16_t *)(h + 0x24) = 0;
		*(const void **)h = SEQ_Ice;
		*(uint16_t *)(h + 4) = (uint16_t)(c->counter & 0xF);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void ChunkUpdate(ChunkNode *c)
	{
		int16_t vy = c->vel[1];
		if (c->pos[1] < EntityHeight2(c->entity) - vy * 4) c->vel[1] = (int16_t)(vy + 4);
		else if (vy != 0)
		{
			if (c->bounce == -1) c->bounce = (int16_t)Div6(vy);
			int16_t d[3] = { c->vel[0], 0, c->vel[2] };
			c->vel[1] = 0;
			NormalizeVector16(d, d);
			c->vel[0] = (int16_t)(c->vel[0] + (int16_t)((d[0] * c->bounce) >> 10));
			c->vel[2] = (int16_t)(c->vel[2] + (int16_t)((d[2] * c->bounce) >> 10));
		}
		c->pos[1] = (int16_t)(c->pos[1] + c->vel[1]);
		c->pos[2] = (int16_t)(c->pos[2] + (int16_t)(c->vel[2] >> 2));
		c->pos[0] = (int16_t)(c->pos[0] + (int16_t)(c->vel[0] >> 2));
	}

	static uint32_t __cdecl ChunkTask(TaskNode *n)
	{
		ChunkNode *c = (ChunkNode *)n;
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(held_note_draw(ORIG_ChunkTask, c);)
		ChunkDraw(c);
		c->counter++;
		if (c->counter >= 0x10) return TASK_END;
		ChunkUpdate(c);
		return 0;
	}

	// ------------------------------------------------------------------
	// Mist (0x6DBFF0): ice sequence frame counter & 15, drifting outwards, decelerating
	// ------------------------------------------------------------------
	static void MistDraw(const MistNode *m)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(m->pos, m->scale, -(m->scale >> 2));
		*(uint16_t *)(h + 0x24) = 0;
		*(const void **)h = SEQ_Ice;
		*(uint16_t *)(h + 4) = (uint16_t)(m->counter & 0xF);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void MistUpdate(MistNode *m)
	{
		m->vel[2] = (int16_t)(m->vel[2] + m->acc_z);
		m->vel[0] = (int16_t)(m->vel[0] + m->acc_x);
		m->pos[2] = (int16_t)(m->pos[2] + m->vel[2]);
		m->pos[0] = (int16_t)(m->pos[0] + m->vel[0]);
	}

	static uint32_t __cdecl MistTask(TaskNode *n)
	{
		MistNode *m = (MistNode *)n;
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(held_note_draw(ORIG_MistTask, m);)
		MistDraw(m);
		m->counter++;
		if (m->counter >= 0x10) return TASK_END;
		MistUpdate(m);
		return 0;
	}
}

	void register_mag103_blizzara()
	{
		register_port(blizzara103::ORIG_Director, (void *)blizzara103::DirectorTask, "B103 Director", 103);
		register_port(blizzara103::ORIG_IceBlockTask, (void *)blizzara103::IceBlockTask, "B103 IceBlockTask", 103);
		register_port(blizzara103::ORIG_PillarTask, (void *)blizzara103::PillarTask, "B103 PillarTask", 103);
		register_port(blizzara103::ORIG_BurstTask, (void *)blizzara103::BurstTask, "B103 BurstTask", 103);
		register_port(blizzara103::ORIG_ChunkTask, (void *)blizzara103::ChunkTask, "B103 ChunkTask", 103);
		register_port(blizzara103::ORIG_MistTask, (void *)blizzara103::MistTask, "B103 MistTask", 103);
		// 30 fps layer: see mag103_blizzara_held.inc
		FX_HELD(register_mag103_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag103_blizzara_held.inc"
#endif
