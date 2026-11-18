/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// Effect 282: Evil-Eye

#include "mag_common.h"

namespace ff8fx
{
namespace evileye282
{
	using namespace eng;
	using namespace magc;

	inline CastContext *&Ctx() { return var<CastContext *>(us(0x2507E94)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(us(0x2507E98)); }
	inline TaskQueue *QSparks() { return (TaskQueue *)us(0x2507EA0); }
	inline TaskQueue *QHits() { return (TaskQueue *)us(0x2507EB0); }
	inline TaskQueue *QMain() { return (TaskQueue *)us(0x2507EC0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2507EF4)); }
	inline uint32_t &FlashLevel() { return var<uint32_t>(us(0x2507EFC)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2507F00)); }
	inline Mat4x3 &EffCam() { return var<Mat4x3>(us(0x2507F08)); }
	inline uint32_t RenderOT44() { return var<uint32_t>(us(0x1D8E04C)) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x67D4C0;
	static const uint32_t ORIG_MainTask = 0x67D680;
	static const uint32_t ORIG_HitTask = 0x67DB20;
	static const uint32_t ORIG_SparkTask = 0x67DC60;
	static const uint32_t ORIG_PrimCallback = 0x67D820;
	static const uint32_t POOL_Hits = 0xA00, POOL_Main = 0xEC8;
	static const uint32_t MORPH = 0x142C;
	static const uint32_t ARENA_A = 0x162C, ARENA_B = 0x1162C;
	static const uint32_t MODEL_Eye = 0x10A0558;
	static const uint32_t MODEL_Hit = 0x109EC64;
	static const uint32_t SEQ_Spark = 0x109EAB8;
	static const int16_t *EYE() { return (const int16_t *)us(0x10A3018); }
	static const void *SOUND_Eye() { return (const void *)us(0x10A2F50); }

	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(us(0x7016B0))(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x701310))(angles, out); }
	inline void RotationFromAnglesYXZ(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x7015B0))(angles, out); }
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(us(0x701390))(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(us(0x460850))(); }
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(us(0x7043B0))(h, ot, mode, cursor); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(us(0x701270))(angle, out); }
	inline void OrthonormalizeCamera(const Mat4x3 *in, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x67AAE0))(in, out); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, flag, out); }

#pragma pack(push, 1)
	struct MasterNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t pad0E;
		uint8_t started;
		uint32_t arena;
	};
	struct MainNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t ntargets;
		uint8_t *ent[3];
		uint8_t eye[0x548];
	};
	struct HitNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t pad12;
		uint8_t *ent;
		uint8_t layout[0x180];
	};
	struct SparkNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t counter;
	};
	struct PrimArg
	{
		Mat4x3 m;
		int32_t depth;
		uint32_t morph;
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x564 && sizeof(HitNode) == 0x198 && sizeof(SparkNode) == 0x14, "Evil-Eye nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Evil-Eye prim block");

	static void BonePoint(const int16_t *v, int32_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(out);
	}

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

	static void EyeArg(PrimArg *arg)
	{
		RotY(-*(const int16_t *)(Caster() + 0xE), &arg->m);
		BonePoint(EYE(), arg->m.t);
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		arg->morph = TexBase() + MORPH;
		arg->depth = -0x80;
	}

	static void HitArg(const HitNode *h, PrimArg *arg)
	{
		RotY(0, &arg->m);
		arg->m.t[0] = h->pos[0];
		arg->m.t[1] = h->pos[1];
		arg->m.t[2] = h->pos[2];
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		arg->morph = TexBase() + MORPH;
		arg->depth = -0x80;
	}

	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, 0x800, -0x200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = us(SEQ_Spark);
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag282_evil_eye_held.h"
#endif

namespace ff8fx
{
namespace evileye282
{
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
		if (r->flags & 0x40000) RotationFromAnglesYXZ(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x1000)
		{
			GteSetRotMatrixCtrl(&EffCam());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t w[3];
			GteReadMAC123(w);
			m.t[0] = w[0];
			m.t[1] = w[1];
			m.t[2] = w[2];
			if (!(r->flags & 0x8000)) MatrixMultiply(&EffCam(), &m);
		}
		else if (r->flags & 0x200)
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
		PacketCursor() = RenderPrim2(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		FX_HELD(held_note_spark(s);)
		SparkDraw(s);
		s->pos[1] = (int16_t)(s->pos[1] - 0xA);
		s->counter++;
		return s->counter < 0x10 ? 0 : TASK_END;
	}

	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		HitArg(h, &arg);
		FX_HELD(held_note_hit(h);)
		const int left = prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0);
		if (!left) return TASK_END;
		if (left < 0x1C && !(left & 3))
		{
			int32_t count = 1;
			SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), us(ORIG_SparkTask));
			while (s)
			{
				uint8_t *ent = h->ent;
				while (RandomBonePoint(ent, mul32(CrtRand(), ***(const uint8_t *const *const *)(ent + 0x64)) >> 15, s->pos)) ent = h->ent;
				s->counter = 0;
				if (--count == 0) break;
				s = (SparkNode *)AddTaskToQueue(QSparks(), us(ORIG_SparkTask));
			}
		}
		return 0;
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		if (m->counter > 0x29) FlashLevel() = (uint32_t)((int32_t)((uint32_t)mul32(0x31 - m->counter, 0x113) << 3) >> 3);
		else FlashLevel() = 0x898;
		const int32_t c = m->counter - 2;
		if ((uint32_t)c < 0x1A)
		{
			PrimArg arg;
			EyeArg(&arg);
			FX_HELD(held_note_eye(m);)
			prim::play((prim::Layout *)m->eye, PrimCallback, (int)&arg, 0);
		}
		if (c == 3) ScreenFadeTask(0, 1, 0, 0x60);
		if (m->counter == 2) BdPlaySE(SOUND_Eye(), 0, 0x80);
		if (m->counter == 0xA)
		{
			for (int i = 0; i < m->ntargets; i++)
			{
				HitNode *h = (HitNode *)AddTaskToQueue(QHits(), us(ORIG_HitTask));
				if (!h) continue;
				uint8_t *ent = m->ent[i];
				h->ent = ent;
				GetEffectSpawnPosition(ent, 0xF1, 0, h->pos);
				h->pad12 = 0;
				DecodeModelPrimLayout(us(MODEL_Hit), h->layout, 0x180);
			}
		}
		m->counter++;
		if (m->counter >= 0x31)
		{
			ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
			FlashLevel() = 0;
			return TASK_END;
		}
		return 0;
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		FX_HELD(held_note_master();)
		if (mn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			mn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			mn->arena = 1;
		}
		FlashLevel() = 0;
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x564, 1);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x198, 3);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x14, 0x80);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), us(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x156);
			m->ntargets = Ctx()->actions[0].target_count;
			for (int i = 0; i < m->ntargets; i++) m->ent[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
			DecodeModelPrimLayout(us(MODEL_Eye), m->eye, 0x548);
		}
		int a0, a1, a2;
		if (mn->started)
		{
			OrthonormalizeCamera(&Camera(), &EffCam());
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QHits());
			a2 = ExecuteTaskQueue(QSparks());
		}
		else a0 = a1 = a2 = (int)n;
		SetScreenFlash(FlashLevel(), 0);
		if (mn->started && !a0 && !a1 && !a2) return TASK_END;
		mn->counter++;
		return 0;
	}
}

	void register_mag282_evil_eye()
	{
		register_port(evileye282::ORIG_MasterTask, (void *)evileye282::MasterTask, "282 MasterTask", 282);
		register_port(evileye282::ORIG_MainTask, (void *)evileye282::MainTask, "282 MainTask", 282);
		register_port(evileye282::ORIG_HitTask, (void *)evileye282::HitTask, "282 HitTask", 282);
		register_port(evileye282::ORIG_SparkTask, (void *)evileye282::SparkTask, "282 SparkTask", 282);
		FX_HELD(register_mag282_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag282_evil_eye_held.inc"
#endif
