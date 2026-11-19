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

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace treatment
{
	static const Mod MOD_057 = { "treatment", 57, 0x7FD130, 0x8031E0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x260D5F8, 0x260E308, 0x260E318, 0x260E324, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x260D614, 0x0, 0x260D6F0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x260DEF0, 0x0, 0x260E190, 0x0, 0x0, 0x0, 0x0, 0x0, 0x260E30A, 0x260E30C, 0x0, 0x260E31A, 0x0, 0x260E320, 0x0, 0x260E330 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7FD730, 0x7FE180, 0x7FE210, 0x7FE270, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x800480, 0x8005C0, 0x800600, 0x801890, 0x8018E0, 0x801910, 0x801990, 0x802080, 0x8020D0, 0x802170, 0x802230, 0x802A80, 0x802E80, 0x802F10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7FD590, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7FD8E0, 0x7FD9C0, 0x7FDB20, 0x7FDBF0, 0x7FDD10, 0x7FDF20, 0x800540, 0x800690, 0x800CF0, 0x800D40, 0x800DD0, 0x800EA0, 0x801630, 0x801740, 0x8017C0, 0x8017F0, 0x802010, 0x802040, 0x802120, 0x802240, 0x802390, 0x802A20, 0x802A50, 0x802C30, 0x802D70, 0x0, 0x7FE180, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	static const uint32_t PACKET_CURSOR = 0x260D5F4;
	static const uint32_t ACTOR_STATE = 0x260E330;
	static const uint32_t SCRATCH_SP = 0x260E320;
	static const uint32_t Q_EMITTER = 0x260E190, Q_ACTORS = 0x260DEF0, Q_STAGE = 0x260E180;
	static const uint32_t ORIG_Master = 0x7FD280;
	static const uint32_t ORIG_Emitter = 0x7FD590;
	static const uint32_t ORIG_ActorSystem = 0x7FD6A0;
	static const void *SOUND_Treatment() { return (const void *)us(0x159E3BC); }

	namespace rx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(us(0x45E0B0))(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(us(0x45E9D0))(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(us(0x45EBF0))(); }
	}

	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}
	static inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	static inline bool t2_outx(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0xA00; }
	static inline bool t2_outy(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0x6C0; }
	static inline void t2_depth_cue(uint32_t ctx, uint32_t pCode)
	{
		x::set_unk_1CA8A28(pCode);
		x::set_dword_1CA8A30(U32(ctx, 0xC));
		x::sub_45F270();
		x::set_param_with_dword_1CA8A68(pCode);
	}
	static inline uint32_t t2_ot(uint32_t ctx, uint32_t a2, uint32_t a3)
	{
		const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
		S32(ctx, 0x38) = z;
		if (z < 0)
			S32(ctx, 0x38) = 0;
		return a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
	}
	static uint32_t s2_twin(uint32_t ctx, int32_t b)
	{
		uint32_t ecx = (uint32_t)(U8(ctx, b + 2) & 0xF8) | 0xFFFE2000u;
		uint32_t edx = (uint32_t)(U8(ctx, b) & 0xF8);
		ecx <<= 5;
		ecx |= edx;
		edx = U16(ctx, b + 6);
		ecx <<= 5;
		edx = ~(edx - 1) & 0xF8;
		ecx |= edx;
		edx = U16(ctx, b + 4);
		ecx <<= 2;
		edx = (uint32_t)((int32_t)~(edx - 1) >> 3) & 0x1F;
		ecx |= edx;
		return ecx;
	}
	static uint32_t s3_texwin(uint32_t ctx, int32_t o)
	{
		uint32_t c = ((uint32_t)U8(ctx, o + 2) & 0xF8) | 0xFFFE2000u;
		c = (c << 5) | ((uint32_t)U8(ctx, o) & 0xF8);
		c = (c << 5) | (~((uint32_t)U16(ctx, o + 6) - 1u) & 0xF8);
		c = (c << 2) | ((~((uint32_t)U16(ctx, o + 4) - 1u) >> 3) & 0x1F);
		return c;
	}

	static void s3_scroll_uv(uint32_t pkt, int32_t o, uint32_t d, uint32_t ctx, int32_t wo)
	{
		int32_t a = (int32_t)(U8(pkt, o) + d);
		int32_t c = (int32_t)(U8(pkt, o + 0xC) + d);
		int32_t e = (int32_t)(U8(pkt, o + 0x18) + d);
		int32_t b = (int32_t)(U8(pkt, o + 0x24) + d);
		if ((b | e | c | a) > 0xFF)
		{
			uint8_t w = U8(ctx, wo);
			U8(pkt, o) = (uint8_t)(a - w);
			U8(pkt, o + 0xC) = (uint8_t)(c - w);
			U8(pkt, o + 0x18) = (uint8_t)(e - w);
			U8(pkt, o + 0x24) = (uint8_t)(b - w);
		}
		else
		{
			U8(pkt, o) = (uint8_t)a;
			U8(pkt, o + 0xC) = (uint8_t)c;
			U8(pkt, o + 0x18) = (uint8_t)e;
			U8(pkt, o + 0x24) = (uint8_t)b;
		}
	}

	static inline bool s3_off(int16_t v, int16_t lim) { return v < 0 || v > lim; }

	static uint32_t s3_quad(uint32_t ctx, uint32_t data, uint32_t vbase, uint32_t ot_base, uint32_t shift, uint32_t pkt)
	{
		x::GTE_LoadV012(vbase + ((uint32_t)U16(data, 4) << 2), vbase + ((uint32_t)U16(data, 6) << 2), vbase + ((uint32_t)U16(data, 8) << 2));
		x::GTE_RTPT();
		uint32_t flags = U32(ctx, 0x14);
		uint32_t col0 = U32(data, 0);
		U32(pkt, 0) = 0x0C000000;
		U32(pkt, 4) = col0;
		if (flags & 2)
		{
			col0 |= 0x2000000;
			U32(pkt, 4) = col0;
		}
		if (flags & 8)
			U32(pkt, 4) &= 0xFDFFFFFF;
		uint32_t uv0 = U32(data, 0xC);
		uint32_t uv1 = U32(data, 0x10);
		U32(pkt, 0xC) = uv0;
		uint32_t uv23 = U32(data, 0x14);
		U32(pkt, 0x24) = uv23;
		U32(pkt, 0x18) = uv1;
		U32(pkt, 0x30) = uv23 >> 16;
		x::GTE_ReadFLAG(ctx + 0x3C);
		if (U32(ctx, 0x3C) & 0x60000)
			return pkt;
		x::GTE_NCLIP();
		uint32_t clip = 0;
		x::GTE_ReadMAC0(ctx + 0x30);
		if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x20))
			return pkt;
		x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
		x::GTE_LoadV0(vbase + ((uint32_t)U16(data, 0xA) << 2));
		x::GTE_RTPS();
		if (s3_off(S16(pkt, 0x8), 0xA00)) clip = 1;
		if (s3_off(S16(pkt, 0x14), 0xA00)) clip |= 2;
		if (s3_off(S16(pkt, 0x20), 0xA00)) clip |= 4;
		if (s3_off(S16(pkt, 0xA), 0x6C0)) clip |= 0x10;
		if (s3_off(S16(pkt, 0x16), 0x6C0)) clip |= 0x20;
		if (s3_off(S16(pkt, 0x22), 0x6C0)) clip |= 0x40;
		x::GTE_ReadSXY2(pkt + 0x2C);
		x::GTE_AVSZ4();
		uint32_t c = clip;
		if (s3_off(S16(pkt, 0x2C), 0xA00)) c |= 8;
		if (s3_off(S16(pkt, 0x2E), 0x6C0)) c |= 0x80;
		if ((c & 0xF) == 0xF)
			return pkt;
		if ((c & 0xF0) == 0xF0)
			return pkt;
		x::GTE_ReadOTZ(ctx + 0x38);
		if (U8(ctx, 0x14) & 0x80)
		{
			x::sub_45E120(data + 0x18, data + 0x1C, data + 0x20);
			x::set_dword_1CA8A30(U32(ctx, 0xC));
			x::sub_45F4C0();
			x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
			x::set_unk_1CA8A28(pkt + 4);
			x::sub_45F270();
			x::set_param_with_dword_1CA8A68(pkt + 4);
		}
		else
		{
			uint32_t c1 = U32(data, 0x18);
			uint32_t c2 = U32(data, 0x1C);
			U32(pkt, 0x10) = c1;
			uint32_t c3 = U32(data, 0x20);
			U32(pkt, 0x1C) = c2;
			U32(pkt, 0x28) = c3;
		}
		int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
		S32(ctx, 0x38) = z;
		if (z < 0)
			S32(ctx, 0x38) = 0;
		uint32_t ot = ot_base + ((uint32_t)(S32(ctx, 0x38) >> (shift & 31)) << 2);
		uint16_t du = U16(ctx, 0x18);
		uint16_t dv = U16(ctx, 0x1A);
		if ((uint16_t)(du | dv) == 0)
		{
			x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
			return pkt + 0x34;
		}
		if (du != 0)
			s3_scroll_uv(pkt, 0xC, du, ctx, 0x28);
		uint16_t dv2 = U16(ctx, 0x1A);
		if (dv2 != 0)
			s3_scroll_uv(pkt, 0xD, dv2, ctx, 0x2A);
		uint32_t prim = pkt;
		uint32_t twa = pkt + 0x34;
		U32(twa, 0) = 0x2000000;
		uint32_t w = (ctx + 0x1C != 0) ? s3_texwin(ctx, 0x1C) : 0;
		U32(twa, 4) = w;
		U32(twa, 8) = 0;
		x::SSIGPU_InsertPrimAutoDepth(ot, twa);
		x::SSIGPU_InsertPrimAutoDepth(ot, prim);
		uint32_t twb = pkt + 0x40;
		U32(twb, 0) = 0x2000000;
		w = (ctx + 0x24 != 0) ? s3_texwin(ctx, 0x24) : 0;
		U32(twb, 4) = w;
		U32(twb, 8) = 0;
		x::SSIGPU_InsertPrimAutoDepth(ot, twb);
		return pkt + 0x4C;
	}

	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	uint32_t __cdecl tr_7FD280(uint32_t a1);
	uint32_t __cdecl tr_7FD3E0(uint32_t a1);
	uint32_t __cdecl tr_7FD590(uint32_t a1);
	uint32_t __cdecl tr_802FD0(uint32_t a1);
	uint32_t __cdecl tr_803040(uint32_t a1);
	uint32_t __cdecl tr_7FE270(uint32_t a1, uint32_t a2);
	uint32_t __cdecl tr_7FD510(uint32_t a1);
	uint32_t __cdecl tr_7FD620(uint32_t a1);
	uint32_t __cdecl tr_7FD700(uint32_t a1);
	uint32_t __cdecl tr_7FD730(uint32_t a1);
	uint32_t __cdecl tr_7FD8E0(uint32_t a1);
	uint32_t __cdecl tr_7FD9C0(void);
	uint32_t __cdecl tr_7FDB20(void);
	uint32_t __cdecl tr_7FDD10(void);
	uint32_t __cdecl tr_7FE180(uint32_t a1);
	uint32_t __cdecl tr_7FE210(void);
	uint32_t __cdecl tr_7FE420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_7FE550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_7FE770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_7FEA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_7FEE80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_7FFE30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl tr_800480(void);
	uint32_t __cdecl tr_800540(uint32_t a1);
	uint32_t __cdecl tr_801630(uint32_t a1, uint32_t a2);
	uint32_t __cdecl tr_802390(uint32_t a1);
	uint32_t __cdecl tr_802FA0(uint32_t a1);
	uint32_t __cdecl tr_802FF0(uint32_t a1);
	uint32_t __cdecl tr_803080(uint32_t a1);
	uint32_t __cdecl tr_803110(uint32_t a1);
	uint32_t __cdecl tr_803130(uint32_t a1);
}
}
}

namespace ff8fx
{
namespace act
{
namespace treatment
{
	uint32_t __cdecl tr_7FD280(uint32_t a1)
	{
		g_mod = &MOD_057;
		uint32_t states[11];
		ff8fx::copy<(8 * 4)>((void *)us(0x2793E58), (const void *)us(0x1D97778));
		const uint32_t node = a1;
		MEM<uint32_t>(us(SCRATCH_SP)) = us(0x2793E58);
		MEM<uint32_t>(us(0x260D614)) = us(0x2793E58);
		states[0] = us(0x7FD3C0);
		const uint8_t parity = U8(node, 0x5C);
		states[1] = us(0x7FD3D0);
		states[2] = us(0x7FD3E0);
		states[3] = us(0x7FD560);
		states[4] = us(0x803040);
		states[5] = us(0x803080);
		states[6] = us(0x803180);
		states[7] = us(0x803190);
		states[8] = us(0x8031A0);
		states[9] = us(0x8031B0);
		states[10] = us(0x8031D0);
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x260E304));
			const uint32_t v2 = MEM<uint32_t>(us(0x260D6FC));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x260D610)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x260E300));
			const uint32_t v2 = MEM<uint32_t>(us(0x260D6F8));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x260D610)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(us(0x260E31A)) = 0;
		MEM<uint16_t>(us(0x260E30A)) = 0;
		static const uint32_t queues[3] = { us(Q_EMITTER), us(Q_ACTORS), us(Q_STAGE) };
		for (int i = 0; i < 3; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl tr_7FD3E0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0;
		uint32_t p = MEM<uint32_t>(us(0x260E32C));
		MEM<uint32_t>(us(0x260E30C)) = p;
		p += 0x10E0;
		MEM<uint32_t>(us(0x260E324)) = p;
		p += 0x10680;
		MEM<uint32_t>(us(0x260E32C)) = p;
		a_732160();
		x::Effect_AddTaskAndInitFromCtx(us(Q_STAGE), us(0x7FD470), 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl tr_7FD590(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = us(0x7FD620);
		states[1] = us(0x802FD0);
		states[2] = us(0x802FF0);
		states[3] = us(0x803030);
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Treatment()), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl tr_802FD0(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x14)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	uint32_t __cdecl tr_803040(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return a1;
		const uint8_t idx = U8(node, 0x2A);
		if ((int16_t)(int8_t)idx < S16(node, 0x58))
		{
			const uint8_t loops = U8(node, 0x2E);
			U8(node, 0x2A) = (uint8_t)(idx + 1);
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x2E) = (uint8_t)(loops + 1);
			U8(node, 0x29) = (uint8_t)(st - 1);
			return a1;
		}
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x60) = 0x14;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl tr_7FE270(uint32_t a1, uint32_t a2)
	{
		const uint32_t act = a1;
		const uint32_t ctx = MEM<uint32_t>(us(SCRATCH_SP)) - 0x68;
		MEM<uint32_t>(us(SCRATCH_SP)) = ctx;
		U32(ctx, 0) = U32(act, 0x170);
		U32(ctx, 0x14) = 0;
		if (U16(a2, 0x3A) == 0)
			U32(ctx, 0x14) = 0x30;
		const uint16_t col = U16(act, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(act, 0x16C);
			const uint32_t fl = U32(ctx, 0x14);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x14) = fl | 0xC0;
		}
		const uint8_t def_idx = U8(act, 0x1D6);
		U32(ctx, 0x10) = 0;
		U16(ctx, 0x1E) = 0;
		U16(ctx, 0x1C) = 0;
		U16(ctx, 0x22) = 0x100;
		U16(ctx, 0x20) = 0x100;
		if (def_idx == 1 || def_idx == 2)
		{
			const uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
			U16(ctx, 0x28) = 0x80;
			U16(ctx, 0x2A) = 0x80;
			const uint16_t scroll = (uint16_t)((uint8_t)(U8(st, 0x2A) + 2) & 0x7F);
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x18) = 0;
			U16(st, 0x2A) = scroll;
			U16(ctx, 0x1A) = scroll;
		}
		else
		{
			U16(ctx, 0x18) = 0;
			U16(ctx, 0x1A) = 0;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x2A) = 0x100;
			U16(ctx, 0x28) = 0x100;
		}
		const int8_t copies = S8(act, 0x1D8);
		if (copies == 1)
		{
			const uint32_t m = act + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			const uint32_t cursor = MEM<uint32_t>(us(PACKET_CURSOR));
			const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
			MEM<uint32_t>(us(PACKET_CURSOR)) = tr_7FE420(ctx, ot, 2, cursor);
		}
		else if (copies > 0)
		{
			const uint32_t m = act + 0xAC;
			uint32_t pos = act + 0x19E;
			int32_t i = 0;
			do
			{
				S32(act, 0xC0) = S16(pos, -2);
				S32(act, 0xC4) = S16(pos, 0);
				S32(act, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				const uint32_t cursor = MEM<uint32_t>(us(PACKET_CURSOR));
				const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
				const uint32_t r = tr_7FE420(ctx, ot, 2, cursor);
				i++;
				pos += 8;
				MEM<uint32_t>(us(PACKET_CURSOR)) = r;
			} while (i < S8(act, 0x1D8));
		}
		const uint32_t top = MEM<uint32_t>(us(SCRATCH_SP)) + 0x68;
		MEM<uint32_t>(us(SCRATCH_SP)) = top;
		return top;
	}

	uint32_t __cdecl tr_7FD510(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x400)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x400;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = us(0x1D98992);
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0;
	}

	uint32_t __cdecl tr_7FD620(uint32_t a1)
	{
		a_73C100(a1, us(ORIG_ActorSystem), us(0x17A3DD0), 0, 0x2D, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl tr_7FD700(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			tr_7FD730(a1);
			tr_7FE180(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	uint32_t __cdecl tr_7FD730(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;
		const uint32_t root = U32(node, 0x10);
		MEM<uint32_t>(us(ACTOR_STATE)) = st;
		U16(st, 0x22) = w29a;
		uint16_t caster = (uint16_t)U8(node, 0x2C);
		U16(st, 0x24) = w29c;
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		uint32_t relocated = U32(data, 0);
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(us(ACTOR_STATE));
			U32(data, 0) = 1;
		}
		uint16_t ntargets = U16(root, 0x5A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = ntargets;
		if ((int16_t)ntargets > 0)
		{
			int32_t i = 0;
			do
			{
				int32_t action = S8(node, 0x2A);
				uint32_t tab = U32(U32(node, 0x0C), 4);
				uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
				U32(st, 4 + 4 * i) = U8(rec, 0x18 * i);
				i++;
			} while (i < S16(st, 0x1C));
		}
		U16(st, 0x1A) = U16(node, 0x29E);
		tr_7FD9C0();
		st = MEM<uint32_t>(us(ACTOR_STATE));
		if (U16(st, 0x24) == 4)
			callp(us(0x7FD8E0), node);
		tr_7FDD10();
		st = MEM<uint32_t>(us(ACTOR_STATE));
		if (U16(st, 0x24) == 1)
		{
			callp(us(0x7FDB20), node);
			st = MEM<uint32_t>(us(ACTOR_STATE));
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(us(ACTOR_STATE));
		}
		int32_t s1 = S16(st, 0x1E);
		uint32_t e1 = us(0x1D972C0) + (uint32_t)mul32(s1, 0x9C);
		uint32_t d10 = U32(e1, 0x1C);
		uint32_t d14 = U32(e1, 0x20);
		uint32_t s2 = U32(st, 4);
		uint32_t e2 = us(0x1D972C0) + s2 * 0x9C;
		uint32_t d18 = U32(e2, 0x1C);
		int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(us(ACTOR_STATE));
		U32(st, 0) = dist;
		return 0;
	}

	uint32_t __cdecl tr_7FD8E0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(us(ACTOR_STATE));
		const uint32_t v = arena + 0x1B4;
		U32(v, 0) = (uint32_t)shl32((int32_t)S16(a1, 0x290), 16);
		const int32_t y = S16(a1, 0x292);
		const int32_t z = S16(a1, 0x294);
		U32(arena, 0x1B8) = (uint32_t)shl32(y, 16);
		U32(arena, 0x1BC) = (uint32_t)shl32(z, 16);
		b1_copy16(arena + 0x204, v);
		b1_copy16(arena + 0x1F4, v);
		b1_copy16(arena + 0x1E4, v);
		b1_copy16(arena + 0x1D4, v);
		b1_copy16(arena + 0x1C4, v);
		return U32(v, 0xC);
	}

	uint32_t __cdecl tr_7FD9C0(void)
	{
		uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
		const int32_t slot = S16(st, 0x1E);
		const uint32_t ent = us(0x1D972C0) + (uint32_t)mul32(slot, 0x9C);
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(us(ACTOR_STATE));
		}
		const int32_t px = S16(pp, 0);
		U16(pp, 2) = U16(ent, 0x24);
		S32(st, 0x1C4) = shl32(px, 16);
		S32(st, 0x1C8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1CC) = shl32(S16(pp, 4), 16);
		U32(st, 0x1D4) = U32(st, 0x1C4);
		U32(st, 0x1D8) = 0;
		U32(st, 0x1DC) = U32(st, 0x1CC);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(us(ACTOR_STATE));
		}
		S32(st, 0x1E4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1E8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1EC) = shl32(S16(pp, 4), 16);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
			st = MEM<uint32_t>(us(ACTOR_STATE));
		}
		S32(st, 0x1F4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1F8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1FC) = shl32(S16(pp, 4), 16);
		const uint32_t sx = U32(st, 0x1C4);
		const int32_t h = shl32(S16(ent, 0x3C), 16);
		U32(st, 0x204) = sx;
		S32(st, 0x208) = h;
		U32(st, 0x20C) = U32(st, 0x1CC);
		return 0;
	}

	uint32_t __cdecl tr_7FDB20(void)
	{
		const uint32_t arena = MEM<uint32_t>(us(ACTOR_STATE));
		int32_t i = 0;
		if (S16(arena, 0x1C) <= 0)
			return arena;
		const uint32_t src = arena + 0x194;
		uint32_t dst = arena + 0x94;
		do
		{
			i++;
			b1_copy16(dst - 0x40, src);
			b1_copy16(dst, src);
			b1_copy16(dst + 0x40, src);
			b1_copy16(dst + 0x80, src);
			b1_copy16(dst + 0xC0, src);
			dst += 0x10;
		} while (i < (int32_t)S16(arena, 0x1C));
		return dst;
	}

	uint32_t __cdecl tr_7FDD10(void)
	{
		uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (S16(st, 0x1C) > 0)
		{
			uint32_t o = 0;
			for (int32_t i = 0; ; )
			{
				const uint32_t slot = U32(st, 4 + 4 * i);
				const uint32_t ent = us(0x1D972C0) + slot * 0x9C;
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(us(ACTOR_STATE));
				}
				const int32_t px = S16(pp, 0);
				U16(pp, 2) = U16(ent, 0x24);
				S32(st, o + 0x54) = shl32(px, 16);
				S32(st, o + 0x58) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x5C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x94) = U32(st, o + 0x54);
				U32(st, o + 0x98) = 0;
				U32(st, o + 0x9C) = U32(st, o + 0x5C);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(us(ACTOR_STATE));
				}
				S32(st, o + 0xD4) = shl32(S16(pp, 0), 16);
				S32(st, o + 0xD8) = shl32(S16(pp, 2), 16);
				S32(st, o + 0xDC) = shl32(S16(pp, 4), 16);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
					st = MEM<uint32_t>(us(ACTOR_STATE));
				}
				S32(st, o + 0x114) = shl32(S16(pp, 0), 16);
				S32(st, o + 0x118) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x11C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x154) = U32(st, o + 0x54);
				S32(st, o + 0x158) = shl32(S16(ent, 0x3C), 16);
				U32(st, o + 0x15C) = U32(st, o + 0x5C);
				o += 0x10;
				i++;
				if (i >= S16(st, 0x1C)) break;
			}
		}
		const int32_t n = S16(st, 0x1C);
		int32_t min_x = 0, max_x = 0, max_z = 0, min_z = 0;
		for (int32_t i = 0; i < n; i++)
		{
			const int32_t vx = S32(st, 0x54 + 0x10 * i);
			const int32_t vz = S32(st, 0x5C + 0x10 * i);
			if ((uint16_t)i == 0)
			{
				max_x = vx;
				max_z = vz;
				min_x = vx;
				min_z = vz;
				continue;
			}
			if (vx < min_x) min_x = vx;
			else if (vx > max_x) max_x = vx;
			if (vz < min_z) min_z = vz;
			else if (vz > max_z) max_z = vz;
		}
		S32(st, 0x194) = add32(min_x, max_x) / 2;
		U32(st, 0x198) = 0;
		S32(st, 0x19C) = add32(max_z, min_z) / 2;
		return 0;
	}

	uint32_t __cdecl tr_7FE180(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		MEM<uint32_t>(us(ACTOR_STATE)) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			a_743190();
			a_736C00();
			tr_800480();
			tr_7FE210();
			sys = MEM<uint32_t>(us(ACTOR_STATE));
			U16(sys, 0x18) = (uint16_t)(U16(sys, 0x18) + 1);
			if (S16(sys, 0x18) >= S16(sys, 0x1A) && U16(sys, 0x14) == 0 && U16(sys, 0x16) == 0)
				U16(sys, 0x20) = (uint16_t)(U16(sys, 0x20) + 1);
			break;
		case 2:
			done = 1;
			break;
		default:
			break;
		}
		uint16_t c14 = U16(sys, 0x14);
		uint16_t c16 = U16(sys, 0x16);
		MEM<uint16_t>(us(0x260E31A)) = (uint16_t)(MEM<uint16_t>(us(0x260E31A)) + c14);
		MEM<uint16_t>(us(0x260E30A)) = (uint16_t)(MEM<uint16_t>(us(0x260E30A)) + c16);
		return done;
	}

	uint32_t __cdecl tr_7FE210(void)
	{
		uint32_t act = U32(MEM<uint32_t>(us(ACTOR_STATE)), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = MEM<uint32_t>(us(ACTOR_STATE));
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					tr_7FE270(act, def);
			}
			act = U32(act, 4);
		}
		return 0;
	}

	uint32_t __cdecl tr_7FE420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		if (!(U32(ctx, 0x14) & 0x2000))
			U32(ctx, 4) = U32(ctx, 0) + 8;
		const uint32_t model = U32(ctx, 0);
		const uint32_t fb = U8(ctx, 0xA);
		const uint32_t lists = U32(model, 0) + model;
		const uint32_t fg = U8(ctx, 9);
		U32(ctx, 0x2C) = lists;
		const uint32_t fr = U8(ctx, 8);
		x::someCameraWork_45DD60(fr, fg, fb);

		uint32_t cur = a4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = tr_7FE550( ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = tr_7FE770( ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = tr_7FEA00(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = tr_7FEE80(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D770(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D9C0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = callo(us(0x7FF930), ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return tr_7FFE30(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	uint32_t __cdecl tr_7FE550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return a4;
		}
		uint32_t pkt = cursor + 0xC;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x4000000;
			U32(pkt, -8) = code;
			if (flags & 1)
			{
				code |= 0x2000000;
				U32(pkt, -8) = code;
			}
			if (flags & 4)
				U32(pkt, -8) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt - 4, pkt, pkt + 4);
					x::GTE_AVSZ3();
					if (t1_out_x(S16(pkt, -4))) clip = 1;
					if (t1_out_x(S16(pkt, 0))) clip |= 2;
					if (t1_out_x(S16(pkt, 4))) clip |= 4;
					if (t1_out_y(S16(pkt, -2))) clip |= 0x10;
					if (t1_out_y(S16(pkt, 2))) clip |= 0x20;
					if (t1_out_y(S16(pkt, 6))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x40)
						{
							x::set_unk_1CA8A28(pkt - 8);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt - 8);
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x14;
						pkt += 0x14;
					}
				}
			}
			rec += 0xC;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return cursor;
	}

	uint32_t __cdecl tr_7FE770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x5000000;
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0xC, pkt + 0x10);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0xC))
					clip |= 2;
				if (t2_outx(pkt + 0x10))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0xE))
					clip |= 0x20;
				if (t2_outy(pkt + 0x12))
					clip |= 0x40;
				x::GTE_ReadSXY2(pkt + 0x14);
				x::GTE_AVSZ4();
				if (t2_outx(pkt + 0x14))
					clip |= 8;
				if (t2_outy(pkt + 0x16))
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
				pkt += 0x18;
			}
		next:
			rec += 0xC;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	uint32_t __cdecl tr_7FEA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x7000000;
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;
				const uint32_t uv2 = U32(rec, 8) >> 16;
				U32(pkt, 0xC) = U32(rec, 0xC);
				U32(pkt, 0x14) = U32(rec, 0x10);
				U32(pkt, 0x1C) = uv2;
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_AVSZ3();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				if ((clip & 7) == 7)
					goto next;
				if ((clip & 0x70) == 0x70)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x20;
					pkt += 0x20;
					pSXY1 += 0x20;
					pSXY2 += 0x20;
					pCode += 0x20;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0x1C) = (uint8_t)c2;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x1D) = (uint8_t)c2;
						}
					}
					const uint32_t poly = pkt;
					pSXY1 += 0x2C;
					pSXY2 += 0x2C;
					pCode += 0x2C;
					pSXY0 += 0x2C;
					uint32_t prim = pkt + 0x20;
					pkt += 0x2C;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY2 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
			}
		next:
			rec += 0x14;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	uint32_t __cdecl tr_7FEE80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		uint32_t pSXY3 = pkt + 0x20;
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;
				U32(pkt, 0xC) = U32(rec, 0xC);
				const uint32_t uv1 = U32(rec, 0x10);
				const uint32_t uv23 = U32(rec, 0x14);
				U32(pkt, 0x1C) = uv23;
				U32(pkt, 0x14) = uv1;
				U32(pkt, 0x24) = uv23 >> 16;
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				x::GTE_ReadSXY2(pSXY3);
				x::GTE_AVSZ4();
				v = S16(pSXY3, 0);
				if (v < 0 || v > 0xA00)
					clip |= 8;
				v = S16(pkt, 0x22);
				if (v < 0 || v > 0x6C0)
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x28;
					pSXY1 += 0x28;
					pkt += 0x28;
					pSXY2 += 0x28;
					pSXY3 += 0x28;
					pCode += 0x28;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c3 = U8(pkt, 0x24) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
							U8(pkt, 0x24) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x1C) = (uint8_t)c2;
							U8(pkt, 0x24) = (uint8_t)c3;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c3 = U8(pkt, 0x25) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
							U8(pkt, 0x25) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x1D) = (uint8_t)c2;
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x25) = (uint8_t)c3;
						}
					}
					pSXY0 += 0x34;
					pSXY3 += 0x34;
					pSXY2 += 0x34;
					const uint32_t poly = pkt;
					pCode += 0x34;
					pSXY1 += 0x34;
					uint32_t prim = pkt + 0x28;
					pkt += 0x34;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					pSXY2 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY3 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
			}
		next:
			rec += 0x18;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	uint32_t __cdecl tr_7FFE30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t data = U32(ctx, 0x2C);
		int32_t count = S32(data, 0);
		data += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = data;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = data;
			return pkt;
		}
		do
		{
			pkt = s3_quad(ctx, data, vbase, a2, a3, pkt);
			data += 0x24;
		} while (--count);
		U32(ctx, 0x2C) = data;
		return pkt;
	}

	uint32_t __cdecl tr_800480(void)
	{
		uint32_t act = U32(MEM<uint32_t>(us(ACTOR_STATE)), 0x2C);
		if (act == 0)
			return 0;
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						tr_800540(act);
					}
					else if (n > 0)
					{
						int32_t i = 0;
						uint32_t sub = act + 0x19E;
						do
						{
							S32(act, 0xC0) = S16(sub, -2);
							S32(act, 0xC4) = S16(sub, 0);
							S32(act, 0xC8) = S16(sub, 2);
							tr_800540(act);
							i++;
							sub += 8;
						} while (i < S8(act, 0x1D8));
					}
				}
			}
			act = U32(act, 4);
		} while (act != 0);
		return 0;
	}

	uint32_t __cdecl tr_800540(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t hdr = MEM<uint32_t>(us(SCRATCH_SP)) - 0xB4;
		MEM<uint32_t>(us(SCRATCH_SP)) = hdr;
		const uint32_t mtx = act + 0xAC;
		x::GTE_SetRotMatrix(mtx);
		x::GTE_SetTransVector(mtx);
		const uint32_t seq = U32(act, 0x170);
		const uint32_t cursor = MEM<uint32_t>(us(PACKET_CURSOR));
		const int16_t variant = (int16_t)S8(act, 0x1D0);
		U32(hdr, 0) = seq;
		const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
		S16(hdr, 4) = variant;
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(us(PACKET_CURSOR)) = x::InitEffectSequenceFromData_c19(hdr, ot, 2, cursor);
		MEM<uint32_t>(us(SCRATCH_SP)) = MEM<uint32_t>(us(SCRATCH_SP)) + 0xB4;
		return 0;
	}

	uint32_t __cdecl tr_801630(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
		uint32_t dst_tab = U32(st, 0x22C);
		uint32_t src_tab = U32(st, 0x230);
		uint32_t dst = U32(dst_tab, i_dst * 4);
		uint32_t i_src1 = U8(U32(data, 0x130), frame);
		uint32_t src1 = U32(src_tab, i_src1 * 4);
		uint32_t i_src2 = U8(U32(data, 0x134), frame);
		uint32_t src2 = U32(src_tab, i_src2 * 4);
		if (src1 == 0 || src2 == 0)
			return 0;
		uint32_t w_blk = x::Field_Alloc(8);
		int32_t frame2 = S16(a1, 0x1CA);
		uint32_t n = U32(dst, 4);
		int32_t w = S16(U32(data, 0x138), frame2 * 2);
		U32(w_blk, 4) = (uint32_t)w;
		U32(w_blk, 0) = (uint32_t)sub32(0x1000, w);
		if ((int32_t)n > 0)
		{
			uint32_t out = dst + 8;
			uint32_t in2 = src2 + 8;
			uint32_t delta = src1 - src2;
			do
			{
				x::set_dword_1CA8A30(U32(w_blk, 0));
				rx::sub_45E0B0(in2 + delta);
				rx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				rx::sub_45E0B0(in2);
				rx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0;
	}

	uint32_t __cdecl tr_802390(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t st = MEM<uint32_t>(us(ACTOR_STATE));
		int32_t aidx = S8(act, 0x6A);
		int32_t key = S16(act, 0x60);
		const uint32_t data = U32(U32(st, 0x224), aidx * 4);
		uint8_t sel = U8(U32(data, 0x108), key);
		uint8_t kind = U8(U32(data, 0x100), key);
		int16_t ax = 0, ay = 0, az = 0;
		uint32_t base = 0;
		bool have = false;
		if (kind == 0)
		{
			if (sel <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };
				base = st + (uint32_t)((int32_t)S8(act, 0x6B) << 4) + offs0[sel];
				have = true;
			}
		}
		else if (kind == 1)
		{
			if (sel <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };
				base = st + offs1[sel];
				have = true;
			}
		}
		if (have)
		{
			int32_t vx = b2_fx(U32(base, 0));
			int32_t vy = b2_fx(U32(base, 4));
			ay = (int16_t)vy;
			int32_t vz = b2_fx(U32(base, 8));
			az = (int16_t)vz;
			ax = (int16_t)vx;
		}
		int16_t v[4];
		int16_t ir[8] = {};
		v[0] = (int16_t)U16(U32(data, 0x110), key * 2);
		v[1] = (int16_t)U16(U32(data, 0x114), key * 2);
		v[2] = (int16_t)U16(U32(data, 0x118), key * 2);
		v[3] = 0;
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		az = (int16_t)(az + ir[2]);
		ay = (int16_t)(ay + ir[1]);
		const int32_t key2 = S16(act, 0x60);
		uint8_t kind2 = U8(U32(data, 0x104), key2);
		uint8_t sel2 = U8(U32(data, 0x10C), key2);
		ax = (int16_t)(ax + ir[0]);
		int16_t bx, by, bz;
		uint32_t base2 = 0;
		bool have2 = false;
		if (kind2 == 0)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };
				uint32_t g = MEM<uint32_t>(us(ACTOR_STATE));
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };
				uint32_t g = MEM<uint32_t>(us(ACTOR_STATE));
				base2 = g + offs1[sel2];
				have2 = true;
			}
		}
		if (have2)
		{
			int32_t vx = b2_fx(U32(base2, 0));
			int32_t vy = b2_fx(U32(base2, 4));
			int32_t vz = b2_fx(U32(base2, 8));
			bx = (int16_t)vx;
			by = (int16_t)vy;
			bz = (int16_t)vz;
		}
		else
		{
			bz = ir[2];
			by = ir[1];
			bx = ir[0];
		}
		v[0] = (int16_t)U16(U32(data, 0x11C), key2 * 2);
		v[1] = (int16_t)U16(U32(data, 0x120), key2 * 2);
		v[2] = (int16_t)U16(U32(data, 0x124), key2 * 2);
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		bx = (int16_t)(bx + ir[0]);
		by = (int16_t)(by + ir[1]);
		const int32_t key3 = S16(act, 0x60);
		const uint32_t ttab = U32(data, 0x128);
		bz = (int16_t)(bz + ir[2]);
		const int32_t t = S16(ttab, key3 * 2);
		int32_t a = ax;
		S32(act, 0x4C) = shl32(add32(mul32(sub32(bx, a), t), shl32(a, 12)), 4);
		a = ay;
		S32(act, 0x50) = shl32(add32(mul32(sub32(by, a), t), shl32(a, 12)), 4);
		a = az;
		S32(act, 0x54) = shl32(add32(mul32(sub32(bz, a), t), shl32(a, 12)), 4);
		return 0;
	}

	uint32_t __cdecl tr_802FA0(uint32_t a1)
	{
		if (tr_7FE180(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;
	}

	uint32_t __cdecl tr_802FF0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x1E)
			return 0;
		const int32_t action = S8(node, 0x2A);
		const int32_t target = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t targets = U32(acts + (uint32_t)(action * 20), 8);
		x::ApplyActionResultToTarget(targets + (uint32_t)(target * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0;
	}

	uint32_t __cdecl tr_803080(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0;
		x::Effect_AddTaskAndInitFromCtx(us(Q_STAGE), us(0x8030B0), 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl tr_803110(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl tr_803130(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = us(0x1D98992);
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0;
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7FD3C0, (void *)a_73A0D0, "057 MAG_057_sub_88D1E0" },
		{ 0x7FD3D0, (void *)a_73A0D0, "057 MAG_057_sub_88D1F0" },
		{ 0x7FD430, (void *)a_732160, "057 MAG_057_sub_88D250" },
		{ 0x7FD470, (void *)a_73A380, "057 MAG_057_sub_88D290" },
		{ 0x7FD4D0, (void *)a_7335D0, "057 MAG_057_sub_88D2F0" },
		{ 0x7FD550, (void *)a_73A6D0, "057 nullsub_1728" },
		{ 0x7FD560, (void *)a_73A170, "057 MAG_057_sub_88D380" },
		{ 0x7FD650, (void *)a_73C100, "057 MAG_057_sub_88D470" },
		{ 0x7FD6A0, (void *)a_73A380, "057 MAG_057_TREATMENT_Emitter_Tick" },
		{ 0x7FDBF0, (void *)a_73FE90, "057 sub_88DA10" },
		{ 0x7FDF20, (void *)a_740210, "057 Effect_ParticleEmitter_RelocateDescriptor" },
		{ 0x7FF400, (void *)a_73D770, "057 sub_88F220" },
		{ 0x7FF650, (void *)a_73D9C0, "057 sub_88F470" },
		{ 0x8005C0, (void *)a_736C00, "057 sub_8903E0" },
		{ 0x800600, (void *)a_740880, "057 sub_890420" },
		{ 0x800690, (void *)a_740910, "057 sub_8904B0" },
		{ 0x800CF0, (void *)a_740F70, "057 sub_890B10" },
		{ 0x800D40, (void *)a_740FC0, "057 sub_890B60" },
		{ 0x800DD0, (void *)a_741050, "057 sub_890BF0" },
		{ 0x800EA0, (void *)a_741120, "057 sub_890CC0" },
		{ 0x801740, (void *)a_7419C0, "057 sub_891560" },
		{ 0x8017C0, (void *)a_741A40, "057 sub_8915E0" },
		{ 0x8017F0, (void *)a_741A70, "057 sub_891610" },
		{ 0x801890, (void *)a_737ED0, "057 sub_8916B0" },
		{ 0x8018E0, (void *)a_741B60, "057 sub_891700" },
		{ 0x801910, (void *)a_737F50, "057 sub_891730" },
		{ 0x801990, (void *)a_737FD0, "057 sub_8917B0" },
		{ 0x802010, (void *)a_742290, "057 au_re__rand_61" },
		{ 0x802040, (void *)a_7422C0, "057 sub_891E60" },
		{ 0x802080, (void *)a_742300, "057 sub_891EA0" },
		{ 0x8020D0, (void *)a_742350, "057 sub_891EF0" },
		{ 0x802120, (void *)a_7423A0, "057 au_re__rand_61_0" },
		{ 0x802170, (void *)a_7387B0, "057 sub_891F90" },
		{ 0x802230, (void *)a_7424B0, "057 sub_892050" },
		{ 0x802240, (void *)a_7424C0, "057 sub_892060" },
		{ 0x802A20, (void *)a_742CA0, "057 sub_892840" },
		{ 0x802A50, (void *)a_742CD0, "057 sub_892870" },
		{ 0x802A80, (void *)a_742D00, "057 sub_8928A0" },
		{ 0x802C30, (void *)a_742EB0, "057 sub_892A50" },
		{ 0x802D70, (void *)a_742FF0, "057 sub_892B90" },
		{ 0x802E80, (void *)a_743100, "057 sub_892CA0" },
		{ 0x802F10, (void *)a_743190, "057 sub_892D30" },
		{ 0x802FC0, (void *)a_73A6D0, "057 nullsub_1729" },
		{ 0x803030, (void *)a_73A6D0, "057 nullsub_1730" },
		{ 0x8030B0, (void *)a_73A380, "057 MAG_057_TREATMENT_Glint_Tick" },
		{ 0x803170, (void *)a_73A6D0, "057 nullsub_1732" },
		{ 0x803180, (void *)a_747550, "057 MAG_057_TREATMENT_State6_WaitSubtasksDone" },
		{ 0x803190, (void *)a_73A0D0, "057 MAG_057_sub_892FB0" },
		{ 0x8031A0, (void *)a_73A0D0, "057 MAG_057_sub_892FC0" },
		{ 0x8031B0, (void *)a_7475A0, "057 MAG_057_sub_892FD0" },
		{ 0x8031D0, (void *)a_73A6D0, "057 nullsub_1731" },
		{ 0, nullptr, nullptr }
	};
	static const ModPort PORTS[] = {
		{ 0x7FD280, (void *)tr_7FD280, "057 tr_7FD280" },
		{ 0x7FD3E0, (void *)tr_7FD3E0, "057 tr_7FD3E0" },
		{ 0x7FD590, (void *)tr_7FD590, "057 tr_7FD590" },
		{ 0x802FD0, (void *)tr_802FD0, "057 tr_802FD0" },
		{ 0x803040, (void *)tr_803040, "057 tr_803040" },
		{ 0x7FE270, (void *)tr_7FE270, "057 tr_7FE270" },
		{ 0x7FD510, (void *)tr_7FD510, "057 tr_7FD510" },
		{ 0x7FD620, (void *)tr_7FD620, "057 tr_7FD620" },
		{ 0x7FD700, (void *)tr_7FD700, "057 tr_7FD700" },
		{ 0x7FD730, (void *)tr_7FD730, "057 tr_7FD730" },
		{ 0x7FD8E0, (void *)tr_7FD8E0, "057 tr_7FD8E0" },
		{ 0x7FD9C0, (void *)tr_7FD9C0, "057 tr_7FD9C0" },
		{ 0x7FDB20, (void *)tr_7FDB20, "057 tr_7FDB20" },
		{ 0x7FDD10, (void *)tr_7FDD10, "057 tr_7FDD10" },
		{ 0x7FE180, (void *)tr_7FE180, "057 tr_7FE180" },
		{ 0x7FE210, (void *)tr_7FE210, "057 tr_7FE210" },
		{ 0x7FE420, (void *)tr_7FE420, "057 tr_7FE420" },
		{ 0x7FE550, (void *)tr_7FE550, "057 tr_7FE550" },
		{ 0x7FE770, (void *)tr_7FE770, "057 tr_7FE770" },
		{ 0x7FEA00, (void *)tr_7FEA00, "057 tr_7FEA00" },
		{ 0x7FEE80, (void *)tr_7FEE80, "057 tr_7FEE80" },
		{ 0x7FFE30, (void *)tr_7FFE30, "057 tr_7FFE30" },
		{ 0x800480, (void *)tr_800480, "057 tr_800480" },
		{ 0x800540, (void *)tr_800540, "057 tr_800540" },
		{ 0x801630, (void *)tr_801630, "057 tr_801630" },
		{ 0x802390, (void *)tr_802390, "057 tr_802390" },
		{ 0x802FA0, (void *)tr_802FA0, "057 tr_802FA0" },
		{ 0x802FF0, (void *)tr_802FF0, "057 tr_802FF0" },
		{ 0x803080, (void *)tr_803080, "057 tr_803080" },
		{ 0x803110, (void *)tr_803110, "057 tr_803110" },
		{ 0x803130, (void *)tr_803130, "057 tr_803130" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag057_treatment()
	{
		act::register_module(57);
		for (const act::treatment::ModPort *p = act::treatment::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(57, p->addr, p->port, p->name);
		for (const act::treatment::ModPort *p = act::treatment::PORTS; p->addr; p++)
			act::register_module_port(57, p->addr, p->port, p->name);
	}
}

