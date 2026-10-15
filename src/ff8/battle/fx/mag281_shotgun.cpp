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

// Effect 281: Shotgun (enemy attack 61 of kernel.bin; Esthar Soldier c0m078 / c0m079, Laguna's
// second dream, Centra excavation site; MAG_281_*). No other kernel.bin entry uses effect 281.
//
// Structure (setup MAG_281 0x67DCE0 -> _Init 0x67DD10, file loader 0x67DCF0 = mag280.tim (name at
// 0x10A7968); the setup keeps the CASTER's entity, starts camera animation 0x10A7824 and queues the
// TIM):
//   MasterTask (0x67DD90) - alternates the packet arena (magic buffer + 0x2018 / + 0x12018); on
//     counter 1 sets up the pools once (main 1 x 0x220 at + 0x1BC8, hits 6 x 0x14C at + 0x1400,
//     sparks 0x80 x 0x28 at + 0), starts the main task (target = action 0's first target, muzzle
//     layout 0x10A4D40) and builds the floor-plane basis M (0x2507F90) of caster -> target and R
//     (0x2507FB0, the opposite direction). Then: the caster's world bone matrices (0x5095B0), the
//     main queue, the caster's bone matrices rebuilt from its pose (0x508C90), the hit and spark
//     queues; ends when the main and spark queues are empty (the hit queue's result is ignored).
//   MainTask (0x67DF80) - counters 0..13: the muzzle point P = the caster's bone 23 at offset
//     (-388, -2563, -1896) (0x67E500, into M's translation), the muzzle layout in M at P composed
//     with the camera (depth -0x80); counters 1 / 2: 16 muzzle sparks (CRT rand: a random angle
//     and radius in the plane, depth -128 - rand & 0x7F, through M; drag -v / 8, 8 ticks); counter
//     1: 0x5712C0 (0, 1, 0, 0x60); counters 4..11: a hit at a random bone of the target (rand x
//     the bone count >> 15, retried while the bone's word +0 is negative; 0x67E550) with its layout
//     0x10A3230 and 8 sparks (random direction 0x6A6020 scaled by 0x20 + rand & 0x3F >> 11; drag
//     -v / 8, 10 ticks); sound 0x10A7958 at counter 2; damage of action 0's first target record
//     when it ends (after counter 13).
//   HitTask (0x67E6B0) - its layout in R at its position (written into R's translation) composed
//     with the camera (depth -0x80); ends with the layout.
//   SparkTask (0x67E5F0) - sprite sequence (+0x0C: 0x10A3140 muzzle / 0x10A3030 hit) through the
//     shadow-rotation camera (0x320, -200), flipbook frame = its counter, then moves by its
//     velocity and adds its drag to the velocity; ends when its counter reaches its life (+0x1E).
//   PrimCallback (0x67E2D0, Funguar laser's 0x684870 without the 0x40000 rotation variant, on this
//     module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: the master rebuilds the CASTER entity's bone matrices (0x5095B0 world, 0x508C90
// from the pose) every tick; no task writes an entity position (+0x1C..+0x20 only read) or the
// camera words (the camera animation is the engine's). Module globals written every tick: M's and
// R's translations (0x2507FA4..0x2507FAC, 0x2507FC4..0x2507FCC).
// Module globals: 0x2507F28..0x2507FD4 (texture file 0x2507F28, context 0x2507F2C, caster entity
// 0x2507F30, the queues 0x2507F38 sparks / 0x2507F48 hits / 0x2507F58 main / 0x2507F68 master,
// master pool 0x2507F78, packet cursor 0x2507F8C, M 0x2507F90, R 0x2507FB0, magic buffer base
// 0x2507FD0).
// NEVER-WRITTEN STACK WORDS: none read (stk281.txt): the 4th words of the vectors loaded into GTE
// V0 (the callback's record offset) reach the GTE's VZ0 high half (0x1CA8A16) only; the matrices'
// pad words are never read; the prim header keeps the scratch's +0x10.. (and +0x08 when it does not
// fade), as in the game.

#include "mag_common.h"

namespace ff8fx
{
namespace shotgun281
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507F2C); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507F30); }        // caster entity (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507F8C); }
	inline Mat4x3 &BasisM() { return var<Mat4x3>(0x2507F90); }             // caster -> target, t = muzzle point
	inline Mat4x3 &BasisR() { return var<Mat4x3>(0x2507FB0); }             // target -> caster, t = a hit
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507FD0); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507F38; }         // pool: magic buffer, 0x80 x 0x28
	inline TaskQueue *QHits() { return (TaskQueue *)0x2507F48; }           // pool: + 0x1400, 6 x 0x14C
	inline TaskQueue *QMain() { return (TaskQueue *)0x2507F58; }           // pool: + 0x1BC8, 1 x 0x220

	static const uint32_t ORIG_MasterTask = 0x67DD90;
	static const uint32_t ORIG_MainTask = 0x67DF80;
	static const uint32_t ORIG_SparkTask = 0x67E5F0;
	static const uint32_t ORIG_HitTask = 0x67E6B0;
	static const uint32_t POOL_Hits = 0x1400;
	static const uint32_t POOL_Main = 0x1BC8;
	static const uint32_t MORPH = 0x1DE8;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x2018;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x12018;
	static const uint32_t MODEL_Muzzle = 0x10A4D40;      // prim-model layouts
	static const uint32_t MODEL_Hit = 0x10A3230;
	static const uint32_t SEQ_MuzzleSpark = 0x10A3140;   // sprite sequences
	static const uint32_t SEQ_HitSpark = 0x10A3030;
	static const int16_t *const MUZZLE = (const int16_t *)0x10A7960; // offset (-388, -2563, -1896), bone 23
	static const void *const SOUND_Shot = (const void *)0x10A7958;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507F78)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x220 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *ent;      // +0x10 target entity (action 0's first target)
		uint8_t muzzle[0x20C]; // +0x14
	};
	struct HitNode // pool of 6 nodes of 0x14C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C the target's bone
		int16_t pad12;     // +0x12 0
		uint8_t layout[0x138]; // +0x14
	};
	struct SparkNode // pool of 0x80 nodes of 0x28 bytes
	{
		TaskNode hdr;
		uint32_t seq;      // +0x0C sprite sequence
		int16_t pos[3];    // +0x10
		int16_t counter;   // +0x16 flipbook frame
		int16_t vel[3];    // +0x18
		int16_t life;      // +0x1E
		int16_t drag[3];   // +0x20 -velocity / 8, added to the velocity every tick
		int16_t pad26;
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x220 && sizeof(HitNode) == 0x14C, "Shotgun nodes");
	static_assert(sizeof(SparkNode) == 0x28, "Shotgun nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Shotgun prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x67E2D0): one object of a layout. Vertex frame (or blend into the
	// parameter block's morph buffer), rotation from the record's angles, the record's offset
	// through the parameter matrix (flag 0x200: as is), the matrix composed with it (unless flag
	// 0x200 or 0x8000), scale (scale3DMatrix), fade, drawn by 0x7043B0.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
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
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x67E500: the offset v (word +6 = bone) through the caster's bone matrix (world matrices of
	// this tick) -> out (3 int32)
	static void BonePoint(const int16_t *v, int32_t *out)
	{
		const uint8_t *bones = *(uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
		const Mat4x3 *bm = (const Mat4x3 *)(bones + v[3] * 0x30 + 0x20);
		GteSetRotMatrixCtrl(bm);
		GteSetTransVectorCtrl(bm);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(out);
	}

	// 0x67E550: bone idx of the entity in world space (out: 3 words); 1 = the bone's word +0 is
	// negative (no point, out untouched)
	static int RandomBonePoint(uint8_t *ent, int32_t idx, int16_t *out)
	{
		int32_t *blk = (int32_t *)FieldAlloc(0x14);
		const uint8_t *skel = **(uint8_t *const *const *)(ent + 0x64);
		if (idx >= 0xF0) idx = skel[idx - 0xEC];
		const uint8_t *bone = skel + 0x10 + idx * 0x30;
		if (*(const int16_t *)bone < 0)
		{
			FieldFree(0x14);
			return 1;
		}
		TransformVectorBy3x3Matrix((const Mat4x3 *)(ent + 0x40), (const int32_t *)(bone + 0x24), blk);
		out[0] = (int16_t)(*(const int16_t *)(ent + 0x54) + (int16_t)blk[0]);
		out[1] = (int16_t)(*(const int16_t *)(ent + 0x58) + (int16_t)blk[1]);
		out[2] = (int16_t)(*(const int16_t *)(ent + 0x5C) + (int16_t)blk[2]);
		FieldFree(0x14);
		return 0;
	}

	// 0x6A6020 (MAG_063_sub_6A6020): unit direction of the angles a (elevation) and b (heading)
	static void Direction(int32_t a, int32_t b, int16_t *out)
	{
		const int32_t c = ComputeCos(a);
		out[1] = (int16_t)(0 - ComputeSin(a));
		out[0] = (int16_t)(mul32(ComputeSin(b), c) >> 12);
		out[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(b), c)) >> 12);
	}

	// the sprites (0x67E5F3..0x67E64F)
	static void SpriteDraw(int16_t *pos, int16_t frame, uint32_t seq)
	{
		TransformCameraByShadowRotation(pos, 0x320, -200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// the main layout's parameter block: M (translation = the muzzle point) composed with the camera
	static void MainArg(PrimArg *arg)
	{
		ComposeAffineTransform(&Camera(), &BasisM(), &arg->m);
		arg->depth = -0x80;
		arg->morph = TexBase() + MORPH;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag281_shotgun_held.h"
#endif

namespace ff8fx
{
namespace shotgun281
{
	// ------------------------------------------------------------------
	// Spark (0x67E5F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag281_shotgun_held.inc
		FX_HELD(held_note_spark(n);)
		SpriteDraw(s->pos, s->counter, s->seq);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		s->vel[1] = (int16_t)(s->drag[1] + s->vel[1]);
		s->vel[0] = (int16_t)(s->drag[0] + s->vel[0]);
		s->vel[2] = (int16_t)(s->drag[2] + s->vel[2]);
		return s->counter < s->life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Hit (0x67E6B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		BasisR().t[0] = h->pos[0];
		BasisR().t[1] = h->pos[1];
		BasisR().t[2] = h->pos[2];
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &BasisR(), &arg.m);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag281_shotgun_held.inc
		FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)h->layout);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x67DF80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c = m->counter;
		if ((uint32_t)c < 0xE)
		{
			BonePoint(MUZZLE, BasisM().t);
			PrimArg arg;
			MainArg(&arg);
			// 30 fps layer: see mag281_shotgun_held.inc
			FX_HELD(held_note_play(PLAY_MUZZLE, n, (prim::Layout *)m->muzzle);)
			prim::play((prim::Layout *)m->muzzle, PrimCallback, (int)&arg, 0);
			if ((uint32_t)c >= 1 && (uint32_t)c <= 2)
			{
				for (int k = 0; k < 0x10; k++)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (!s) break;
					s->pos[0] = (int16_t)BasisM().t[0];
					s->pos[1] = (int16_t)BasisM().t[1];
					s->pos[2] = (int16_t)BasisM().t[2];
					const int32_t angle = CrtRand();
					const int32_t radius = CrtRand() & 0x7F;
					int16_t v[3];
					v[0] = (int16_t)(mul32(ComputeSin(angle), radius) >> 12);
					v[1] = (int16_t)(mul32(ComputeCos(angle), radius) >> 12);
					v[2] = (int16_t)(-0x80 - (CrtRand() & 0x7F));
					s->counter = 0;
					s->life = 8;
					s->seq = SEQ_MuzzleSpark;
					MatrixMulVector(&BasisM(), v, s->vel);
					s->drag[0] = (int16_t)((int32_t)(0 - (int32_t)s->vel[0]) >> 3);
					s->drag[1] = (int16_t)((int32_t)(0 - (int32_t)s->vel[1]) >> 3);
					s->drag[2] = (int16_t)((int32_t)(0 - (int32_t)s->vel[2]) >> 3);
				}
			}
		}
		if (c == 1) ScreenFadeTask(0, 1, 0, 0x60);
		if ((uint32_t)(m->counter - 4) < 8)
		{
			HitNode *h = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
			if (h)
			{
				do
				{
					uint8_t *ent = m->ent;
					const int32_t idx = mul32(CrtRand(), ***(const uint8_t *const *const *)(ent + 0x64)) >> 15; // the bone count
					if (!RandomBonePoint(ent, idx, h->pos)) break;
				} while (true);
				h->pad12 = 0;
				DecodeModelPrimLayout(MODEL_Hit, h->layout, 0x138);
				for (int k = 0; k < 8; k++)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (!s) break;
					memcpy(s->pos, h->pos, 4);
					memcpy(&s->pos[2], &h->pos[2], 4);
					const int32_t b = CrtRand();
					const int32_t a = CrtRand();
					Direction(a, b, s->vel);
					const int32_t scale = (CrtRand() & 0x3F) + 0x20;
					s->counter = 0;
					s->life = 0xA;
					s->vel[0] = (int16_t)(mul32(s->vel[0], scale) >> 11);
					s->seq = SEQ_HitSpark;
					s->vel[1] = (int16_t)(mul32(s->vel[1], scale) >> 11);
					s->vel[2] = (int16_t)(mul32(s->vel[2], scale) >> 11);
					s->drag[0] = (int16_t)((int32_t)(0 - (int32_t)s->vel[0]) >> 3);
					s->drag[1] = (int16_t)((int32_t)(0 - (int32_t)s->vel[1]) >> 3);
					s->drag[2] = (int16_t)((int32_t)(0 - (int32_t)s->vel[2]) >> 3);
				}
			}
		}
		if (m->counter == 2) BdPlaySE(SOUND_Shot, 0, 0x80);
		m->counter++;
		if (m->counter >= 0xE)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x67DD90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag281_shotgun_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		if (m->counter == 1 && !m->started)
		{
			m->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x220, 1);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x14C, 6);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x28, 0x80);
			MainNode *mn = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&mn->counter, 0, 0x85);
			mn->ent = Entity(Ctx()->actions[0].targets[0]);
			DecodeModelPrimLayout(MODEL_Muzzle, mn->muzzle, 0x20C);
			uint8_t *caster = Caster();
			int32_t v[3];
			v[0] = (int32_t)*(const int16_t *)(mn->ent + 0x1C) - *(const int16_t *)(caster + 0x1C);
			v[1] = 0;
			v[2] = (int32_t)*(const int16_t *)(caster + 0x20) - *(const int16_t *)(mn->ent + 0x20);
			BuildOrthonormalBasis(v, &BasisM());
			v[0] = (int32_t)(0u - (uint32_t)v[0]);
			v[2] = (int32_t)(0u - (uint32_t)v[2]);
			BuildOrthonormalBasis(v, &BasisR());
		}
		int a0, a2;
		if (m->started)
		{
			uint8_t *caster = Caster();
			ComputeBonesWorldMatrices(caster + 0x60, caster + 0x40);
			a0 = ExecuteTaskQueue(QMain());
			BuildBoneMatricesFromPose(Caster() + 0x60);
			ExecuteTaskQueue(QHits());
			a2 = ExecuteTaskQueue(QSparks());
		}
		else a0 = a2 = (int)n;
		if (m->started && a0 == 0 && a2 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag281_shotgun()
	{
		register_port(shotgun281::ORIG_MasterTask, (void *)shotgun281::MasterTask, "281 MasterTask", 281);
		register_port(shotgun281::ORIG_MainTask, (void *)shotgun281::MainTask, "281 MainTask", 281);
		register_port(shotgun281::ORIG_HitTask, (void *)shotgun281::HitTask, "281 HitTask", 281);
		register_port(shotgun281::ORIG_SparkTask, (void *)shotgun281::SparkTask, "281 SparkTask", 281);
		// 30 fps layer: see mag281_shotgun_held.inc
		FX_HELD(register_mag281_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag281_shotgun_held.inc"
#endif
