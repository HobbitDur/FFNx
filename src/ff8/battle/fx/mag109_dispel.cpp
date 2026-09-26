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

// Effect 109: Dispel (spell, MAG_109_*).
//
// Structure (setup MAG_109_DISPEL 0x6CF4A0 -> _Init 0x6CF4D0, file loader 0x6CF4B0 = the texture
// file named at 0x12E6C3C; the setup starts camera animation 0x12E6410 and queues the TIM):
//   RootTask (0x6CF540) - alternates the packet arena (magic buffer + 0x10E0 / + 0x110E0); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the target task of the
//     next action on its first target, unless a target task of 67 ticks or less still runs on that
//     target (the cycle then restarts); runs the target and particle queues, ends when both are
//     empty.
//   Target (0x6CF700) - node 0x348, one per action: the screen flash (up over 0..7, 0x8FC, down
//     from 60); particles: one every other tick over 26..41 around the target (random), one every
//     other tick over 23..28 on a small circle; a screen fade at 27; the ring prim-model layout
//     0x12E3600 played with the callback RingPart (0x6CF9A0) from tick 0 (sound at 0); damage at
//     67; ends at 72 (flash off).
//   Particle (0x6CFC50) - node 0x1C: a sprite sequence (0x12E3440 or 0x12E337C) drawn in place,
//     frame = its counter, until its end frame.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521A68..0x2521AC0 (texture file, magic buffer base, context, root pool,
// packet cursor 0x2521A8C, particle / target / root queues). The target pool (3 x 0x348), the
// particle pool (64 x 0x1C), the morph vertex buffer (+0x10D8) and the packet arenas are in the
// magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace dispel109
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521A6C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521A70); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521A8C); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521A90; }       // pool: magic buffer, 0x40 x 0x1C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521AA0; }         // pool: + 0x700, 3 x 0x348
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6CF540;
	static const uint32_t ORIG_TargetTask = 0x6CF700;
	static const uint32_t ORIG_ParticleTask = 0x6CFC50;
	static const uint32_t MODEL_Ring = 0x12E3600;        // prim-model layout data (0x328-byte layout)
	static const uint32_t SEQ_Sparkle = 0x12E3440;
	static const uint32_t SEQ_Circle = 0x12E337C;
	static const void *const SOUND_Dispel = (const void *)0x12E6C38;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x348 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t pad10[4];
		uint8_t *entity;   // +0x14 target entity
		int16_t pos[3];    // +0x18 effect anchor x, y, z (GetDefaultEffectPosition)
		int16_t size;      // +0x1E (the anchor's height word, replaced by (entity size + 0x100) * 3)
		uint8_t ring[0x328]; // +0x20 prim-model layout (prim::Layout: data, frame, state)
	};
	struct ParticleNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame
		int16_t end;       // +0x0E frames
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		uint32_t seq;      // +0x18 sprite sequence data
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 x, y, z, size word
		int32_t scale[3];  // +0x08 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x14 scale present
		int32_t depth;     // +0x18 prim header +0x18
		uint32_t morph;    // +0x1C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x348 && sizeof(ParticleNode) == 0x1C, "Dispel nodes");
	static_assert(sizeof(PrimArg) == 0x20, "Dispel prim block");

	// x / 12 (0x2AAAAAAB, sar 1)
	static inline int32_t Div12(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 1;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6CF9A0): one object of the ring model
	// ------------------------------------------------------------------
	static void __cdecl RingPart(prim::Layout *l, prim::Record *r, int arg_)
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (scaled by the block's scale) + the block position, through the camera
		int16_t off[4] = {};
		const int32_t scaled = arg->scaled;
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		off[0] = (int16_t)(off[0] + arg->pos[0]);
		off[1] = (int16_t)(off[1] + arg->pos[1]);
		off[2] = (int16_t)(off[2] + arg->pos[2]);
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(off);
		GteMVMVA_RotV0();
		GteReadMAC123(m.t);
		if (!(r->flags & 0x8000)) MatrixMultiply(&Camera(), &m);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Particle (0x6CFC50): a sprite sequence in place
	// ------------------------------------------------------------------
	static void ParticleDraw(const ParticleNode *p)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(p->pos, p->scale, (int32_t)0xFFFFF800);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag109_dispel_held.h"
#endif

namespace ff8fx
{
namespace dispel109
{
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(held_note_particle(p);)
		ParticleDraw(p);
		p->counter++;
		return p->counter < p->end ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Target (0x6CF700)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		int32_t flash;
		const int16_t c0 = t->counter;
		if (c0 < 8) flash = (int32_t)c0 * 2300 / 8;
		else if (c0 >= 0x3C) flash = Div12((0x48 - (int32_t)c0) * 2300);
		else flash = 0x8FC;
		SetScreenFlash((uint32_t)flash, 0);
		// a random sparkle around the target every other tick over 26..41
		const int32_t e1 = t->counter - 0x1A;
		if ((uint32_t)e1 < 0x10 && !(e1 & 1))
		{
			ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			Memset32(&p->counter, 0, 4);
			const int32_t r1 = CrtRand();
			const int32_t r2 = CrtRand();
			p->pos[0] = (int16_t)((mul32(r1 + r2 - 0x8000, t->size) >> 18) + t->pos[0]);
			const int32_t r3 = CrtRand();
			p->pos[1] = (int16_t)(t->pos[1] - (int16_t)(r3 >> 6) + 0x100);
			const int32_t r4 = CrtRand();
			const int32_t r5 = CrtRand();
			p->scale = 0x400;
			p->seq = SEQ_Sparkle;
			p->end = 0x10;
			p->pos[2] = (int16_t)((mul32(r4 + r5 - 0x8000, t->size) >> 18) + t->pos[2]);
		}
		// one every other tick over 23..28 on a circle (sixths of a turn), rising
		const int32_t e2 = t->counter - 0x17;
		if ((uint32_t)e2 < 6 && !(e2 & 1))
		{
			const uint32_t angle = (uint32_t)(((uint64_t)((uint32_t)e2 << 12) * 0xAAAAAAABu) >> 32) >> 2;
			ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			Memset32(&p->counter, 0, 4);
			p->pos[0] = (int16_t)((int16_t)(ComputeSin((int32_t)angle) >> 4) + t->pos[0]);
			p->pos[2] = (int16_t)((int16_t)(ComputeCos((int32_t)angle) >> 4) + t->pos[2]);
			p->scale = 0x800;
			p->seq = SEQ_Circle;
			p->pos[1] = (int16_t)(t->pos[1] - (int16_t)((int32_t)angle >> 3) + 0x100);
			p->end = 7;
		}
		if (t->counter == 0x1B) ScreenFadeTask(0, 1, 1, 0x60);
		if (t->counter >= 0)
		{
			PrimArg arg;
			*(uint32_t *)&arg.pos[0] = *(const uint32_t *)&t->pos[0];
			*(uint32_t *)&arg.pos[2] = *(const uint32_t *)&t->pos[2];
			arg.scale[0] = t->size;
			arg.scale[2] = t->size;
			arg.scale[1] = 0xBB8;
			// UNINIT 0x6CF8C5..0x6CF90F: the original never writes the block's "scaled" word
			// ([esp+0x20], read by the callback at 0x6CFA9C): it holds what older code left on the
			// stack (0 in every harness run and after fx_verify's stack scrub), taken as 0 here
			arg.scaled = 0;
			arg.depth = (int32_t)0xFFFFFF00;
			arg.morph = TexBase() + 0x10D8;
			// 30 fps layer: see mag109_dispel_held.inc
			FX_HELD(held_note_play(t, &arg);)
			prim::play((prim::Layout *)t->ring, RingPart, (int)&arg, 0);
			if (t->counter == 0)
			{
				int16_t pos[4];
				GetDefaultEffectPosition(t->entity, pos);
				BdPlaySE3D(SOUND_Dispel, 0x101, pos);
			}
		}
		if (t->counter == 0x43) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= 0x48)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CF540)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x10E0;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x110E0;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)(TexBase() + 0x700), 0x348, 3);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x1C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a target task of 67 ticks or less runs on this target
				TargetNode *pool = (TargetNode *)(TexBase() + 0x700);
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= 0x43)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					Memset32(&t->counter, 0, 0xCF);
					t->action = (int16_t)(uint16_t)r->action;
					t->entity = entity;
					GetDefaultEffectPosition(entity, t->pos);
					t->size = (int16_t)((*(const int16_t *)(t->entity + 0x26) + 0x100) * 3);
					DecodeModelPrimLayout(MODEL_Ring, t->ring, 0x328);
					r->action++;
				}
			}
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QParticles());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag109_dispel()
	{
		register_port(dispel109::ORIG_RootTask, (void *)dispel109::RootTask, "D109 RootTask", 109);
		register_port(dispel109::ORIG_TargetTask, (void *)dispel109::TargetTask, "D109 TargetTask", 109);
		register_port(dispel109::ORIG_ParticleTask, (void *)dispel109::ParticleTask, "D109 ParticleTask", 109);
		// 30 fps layer: see mag109_dispel_held.inc
		FX_HELD(register_mag109_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag109_dispel_held.inc"
#endif
