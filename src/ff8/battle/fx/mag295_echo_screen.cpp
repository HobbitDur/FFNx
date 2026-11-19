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

// Effect 295: Echo Screen

#include "mag_common.h"

namespace ff8fx
{
namespace echoscreen295
{
	using namespace eng;
	using namespace magc;

	inline CastContext *&Ctx() { return var<CastContext *>(us(0x25079DC)); }
	inline TaskQueue *QPlay() { return (TaskQueue *)us(0x25079E8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2507A1C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2507A20)); }
	inline Mat4x3 &ModMatrix() { return var<Mat4x3>(us(0x2507A28)); }

	static const uint32_t ORIG_RootTask = 0x673F10;
	static const uint32_t ORIG_PlayTask = 0x674030;
	static const uint32_t MODEL = 0x105DAC0;
	static const int32_t LAYOUT_BYTES = 0x9CC;
	static const uint32_t NODE_BYTES = 0x9E0;
	static const void *SOUND() { return (const void *)us(0x1060C40); }
	static const uint32_t ARENA_A = 0x9E8;
	static const uint32_t ARENA_B = 0x109E8;

	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(us(0x7016B0))(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x701310))(angles, out); }
	inline void RotationFromAnglesZYX(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x701430))(angles, out); }
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(us(0x701390))(model, f0, f1, t, out); }
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x7043B0))(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline void MulMatrix3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(us(0x56C220))(a, b); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(us(0x460850))(); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t pad0E;
		uint8_t started;
		uint32_t arena;
	};
	struct PlayNode
	{
		TaskNode hdr;
		int16_t anchor[4];
		uint8_t layout[LAYOUT_BYTES];
	};
	struct PrimArg
	{
		int16_t anchor[4];
		int32_t depth;
		uint32_t morph;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == NODE_BYTES, "Echo Screen nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Echo Screen prim block");

	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		if (r->flags & 0x400) RotationFromAnglesZYX(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		if (r->flags & 0x1000)
		{
			const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
			int32_t v[3];
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&ModMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(v);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)v[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)v[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)v[2]);
			if (!(r->flags & 0x8000)) MatrixMultiply(&ModMatrix(), &m);
		}
		else
		{
			const int16_t off[4] = { (int16_t)(r->pos[0] + arg->anchor[0]), (int16_t)(r->pos[1] + arg->anchor[1]), (int16_t)(r->pos[2] + arg->anchor[2]), 0 };
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

namespace ff8fx
{
namespace echoscreen295
{
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		PlayNode *p = (PlayNode *)n;
		PrimArg arg;
		ff8fx::copy<(sizeof(arg.anchor))>(arg.anchor, p->anchor);
		arg.depth = -0x80;
		arg.morph = TexBase() + NODE_BYTES;
		if (prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0) == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		return 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QPlay(), (void *)TexBase(), NODE_BYTES, 1);
			PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), us(ORIG_PlayTask));
			uint8_t *e = Entity(Ctx()->actions[0].targets[0]);
			GetDefaultEffectPosition(e, p->anchor);
			DecodeModelPrimLayout(us(MODEL), p->layout, LAYOUT_BYTES);
		}
		int a;
		if (r->started)
		{
			EffectCameraMatrix(&Camera(), &ModMatrix());
			a = ExecuteTaskQueue(QPlay());
			if (r->counter == 1) BdPlaySE(SOUND(), 0, 0x80);
		}
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag295_echo_screen()
	{
		register_port(echoscreen295::ORIG_RootTask, (void *)echoscreen295::RootTask, "295 RootTask", 295);
		register_port(echoscreen295::ORIG_PlayTask, (void *)echoscreen295::PlayTask, "295 PlayTask", 295);
	}
}

