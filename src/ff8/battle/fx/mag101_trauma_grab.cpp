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

#include "mag_common.h"

namespace ff8fx
{
namespace grab101
{
	using namespace eng;
	using namespace magc;

	inline int32_t *ScaleVector() { return (int32_t *)us(0x2544E18); }
	inline CastContext *&Ctx() { return var<CastContext *>(us(0x2544E28)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(us(0x2544E2C)); }
	inline uint8_t *&Target() { return var<uint8_t *>(us(0x2544E30)); }
	inline TaskQueue *QGrab() { return (TaskQueue *)us(0x2544E38); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2544E6C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2544E70)); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(us(0x1D8E04C)) + off; }

	static const uint32_t ORIG_RootTask = 0x6DD1A0;
	static const uint32_t ORIG_GrabTask = 0x6DD300;
	static const uint32_t MODEL_Grab = 0x1321BEC;
	static const void *SOUND_Grab() { return (const void *)us(0x13249DC); }

	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(us(0x7016B0))(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x701310))(angles, out); }
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x7015B0))(angles, out); }
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(us(0x701390))(model, f0, f1, t, out); }
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x7043B0))(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(us(0x460850))(); }
	inline void GteLoadIRBytes(const void *rgb) { fn<void (__cdecl *)(const void *)>(us(0x45E0E0))(rgb); }
	inline void GteStoreIRBytes(void *rgb) { fn<void (__cdecl *)(void *)>(us(0x45E410))(rgb); }
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(us(0x56CB90))(a, b, wa, wb, out); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(us(0x505C00))(entity, id); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t pad0E;
		uint8_t started;
		uint32_t arena;
	};
	struct GrabNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad0E;
		int32_t size;
		uint32_t colour;
		uint32_t pos_xy;
		uint32_t pos_z;
		uint8_t layout[0x2B0];
	};
	struct PrimArg
	{
		int16_t pos[4];
		int32_t depth;
		uint32_t morph;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(GrabNode) == 0x2D0, "Trauma grab nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Trauma grab prim block");

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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag101_trauma_grab_held.h"
#endif

namespace ff8fx
{
namespace grab101
{
	static uint32_t FlashLevel(int16_t c)
	{
		if (c < 0xC) return (uint32_t)(((int32_t)c * 0x898) / 12);
		if (c >= 0x2A) return (uint32_t)(((0x36 - (int32_t)c) * 0x898) / 12);
		return 0x898;
	}

	static uint32_t __cdecl GrabTask(TaskNode *n)
	{
		GrabNode *g = (GrabNode *)n;
		*(uint32_t *)(Target() + 0x30) = 0;
		SetScreenFlash(FlashLevel(g->counter), 0);
		const int32_t c = g->counter;
		if ((uint32_t)c < 0x26)
		{
			if ((uint32_t)c <= 0x14)
			{
				const int32_t s = 0x1000 - ComputeSin(((0x14 - c) << 10) / 20);
				GteSetIR0(s);
				GteLoadIRBytes(&g->colour);
				GteGPF();
				GteStoreIRBytes(Target() + 0x2C);
				*(uint8_t *)(Target() + 0x2F) = 0x32;
				*(int32_t **)(Target() + 0x30) = ScaleVector();
				ScaleVector()[2] = s;
				ScaleVector()[1] = s;
				ScaleVector()[0] = s;
				*(int16_t *)(Target() + 0x1E) = (int16_t)((s * 3 - 0x3000) * 900 / 4096 + 0x12C);
				if (c == 0x14) *(uint32_t *)(Target() + 0x2C) = g->colour;
			}
			PrimArg arg;
			arg.pos[0] = *(const int16_t *)(Caster() + 0x1C);
			arg.pos[1] = (int16_t)0xF574;
			arg.pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x20) + 0x3E8);
			arg.pos[3] = 0;
			arg.depth = -0x80;
			arg.morph = TexBase() + 0x2D0;
			FX_HELD(held_note_play((prim::Layout *)g->layout, &arg);)
			prim::play((prim::Layout *)g->layout, PartCallback, (int)&arg, 0);
		}
		if (g->counter == 0) BdPlaySE(SOUND_Grab(), 0, 0x80);
		const int32_t e = (int32_t)g->counter - 0x26;
		if (e < 0x10)
		{
			const int32_t t = (e << 12) / 16;
			if (t < 0)
			{
				const uint8_t *hdr = **(uint8_t *const *const *)(Target() + 0x64);
				*(int16_t *)(Target() + 0x1C) = (int16_t)(*(const int16_t *)(hdr + 0x34) + *(const int16_t *)(Caster() + 0x1C));
				*(int16_t *)(Target() + 0x20) = (int16_t)(*(const int16_t *)(Caster() + 0x20) - 0x190);
				*(int16_t *)(Target() + 0x1E) = 0x4B0;
				*(int32_t *)(Target() + 0x54) = *(const int16_t *)(Target() + 0x1C);
				*(int32_t *)(Target() + 0x58) = *(const int16_t *)(Target() + 0x1E);
				*(int32_t *)(Target() + 0x5C) = *(const int16_t *)(Target() + 0x20);
				*(int16_t *)(Target() + 0x40) = (int16_t)0xF000;
				*(int16_t *)(Target() + 0x46) = 0;
				*(int16_t *)(Target() + 0x4C) = 0;
				*(int16_t *)(Target() + 0x42) = 0;
				*(int16_t *)(Target() + 0x48) = 0x1000;
				*(int16_t *)(Target() + 0x4E) = 0;
				*(int16_t *)(Target() + 0x44) = 0;
				*(int16_t *)(Target() + 0x4A) = 0;
				*(int16_t *)(Target() + 0x50) = (int16_t)0xF000;
				if (e == -0xF) *(uint16_t *)Target() &= 0xFFFB;
			}
			else
			{
				*(uint16_t *)Target() &= 0xFFF7;
				VecLerp(Target() + 0x1C, &g->pos_xy, 0x1000 - t, t, Target() + 0x1C);
			}
		}
		g->counter++;
		if (g->counter >= 0x36)
		{
			*(uint32_t *)(Target() + 0x1C) = g->pos_xy;
			*(uint32_t *)(Target() + 0x20) = g->pos_z;
			*(uint32_t *)(Target() + 0x2C) = g->colour;
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		return 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x450;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x10450;
			r->arena = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QGrab(), (void *)TexBase(), 0x2D0, 1);
			GrabNode *g = (GrabNode *)AddTaskToQueue(QGrab(), us(ORIG_GrabTask));
			Memset32(&g->counter, 0, 0xB1);
			DecodeModelPrimLayout(us(MODEL_Grab), g->layout, 0x2B0);
			g->pos_xy = *(const uint32_t *)(Target() + 0x1C);
			g->pos_z = *(const uint32_t *)(Target() + 0x20);
			g->colour = *(const uint32_t *)(Target() + 0x2C);
			g->size = *(const int16_t *)(Target() + 0x26);
			*(int16_t *)(Target() + 0x1C) = *(const int16_t *)(Caster() + 0x1C);
			*(int16_t *)(Target() + 0x20) = (int16_t)(*(const int16_t *)(Caster() + 0x20) - 0x3E8);
			*(int16_t *)(Target() + 0x1E) = 0x12C;
			QueueChainTransformation(Target(), 0xE);
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QGrab());
		else a = (int)n;
		if (r->started && a == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag101_trauma_grab()
	{
		register_port(grab101::ORIG_RootTask, (void *)grab101::RootTask, "101 RootTask", 101);
		register_port(grab101::ORIG_GrabTask, (void *)grab101::GrabTask, "101 GrabTask", 101);
		FX_HELD(register_mag101_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag101_trauma_grab_held.inc"
#endif
