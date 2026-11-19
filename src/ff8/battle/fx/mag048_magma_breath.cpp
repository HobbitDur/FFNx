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

// Effect 48: Magma Breath: a copy

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace magmabreath
{
	static const Mod MOD_048 = { "magmabreath", 48, 0x824CD0, 0x82A7D0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26495D0, 0x26495DC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26481D8, 0x0, 0x26489A8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26491A8, 0x0, 0x2649448, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26495C2, 0x26495C4, 0x0, 0x26495D2, 0x0, 0x26495D8, 0x0, 0x26495E8 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x825440, 0x825E90, 0x825FD0, 0x826030, 0x0, 0x8262B0, 0x8264C0, 0x826740, 0x826980, 0x826C20, 0x826E50, 0x827110, 0x827370, 0x827650, 0x827790, 0x8277D0, 0x828E00, 0x828E50, 0x828E80, 0x828F00, 0x8295F0, 0x829640, 0x8296E0, 0x8297A0, 0x829FF0, 0x82A3F0, 0x82A480, 0x0, 0x0, 0x0, 0x0, 0x0, 0x825170, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8255F0, 0x8256D0, 0x825830, 0x825900, 0x825A20, 0x825C30, 0x827710, 0x827860, 0x827EC0, 0x827F10, 0x827FA0, 0x828070, 0x828BA0, 0x828CB0, 0x828D30, 0x828D60, 0x829580, 0x8295B0, 0x829690, 0x8297B0, 0x829900, 0x829F90, 0x829FC0, 0x82A1A0, 0x82A2E0, 0x0, 0x825E90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	static const uint32_t PACKET_CURSOR = 0x26481B8;
	static const uint32_t ACTOR_STATE = 0x26495E8;
	static const uint32_t SCRATCH_SP = 0x26495D8;
	static const uint32_t Q_EMITTER = 0x2649448, Q_FLASH = 0x26480F0, Q_ACTORS = 0x26491A8;
	static const uint32_t Q_SHAKE = 0x26482B8, Q_STAGE = 0x2649438;
	static const uint32_t ORIG_Master = 0x824E40;
	static const uint32_t ORIG_Emitter = 0x825170;
	static const uint32_t ORIG_Flash = 0x825220;
	static const uint32_t ORIG_ActorSystem = 0x8253B0;
	static const uint32_t ORIG_Shake = 0x82A540;
	static const void *SOUND_Magma() { return (const void *)us(0x15ADC68); }
	static const uint32_t ACTOR_DATA = 0x15B7C88;
	static const uint32_t FLASH_SCRIPT = 0x15ADC28;

	namespace gx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(us(0x45E0B0))(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(us(0x45E9D0))(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(us(0x45EBF0))(); }
	}

	static inline void e6_copy16(uint32_t dst, uint32_t src)
	{
		U32(dst, 0) = U32(src, 0);
		U32(dst, 4) = U32(src, 4);
		U32(dst, 8) = U32(src, 8);
		U32(dst, 12) = U32(src, 12);
	}
	static inline void e6_rotate_zxy(uint32_t m, uint32_t ang)
	{
		x::MAG_022_sub_8DD770(m);
		if (U16(ang, 4) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(ang, 4));
		if (U16(ang, 0) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(ang, 0));
		if (U16(ang, 2) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ang, 2));
	}
	static inline void e6_rotate_down_vector(uint32_t sp)
	{
		uint32_t v = sp + 0x20;
		U16(sp, 0x22) = 0xF000;
		U16(sp, 0x24) = 0;
		U16(v, 0) = 0;
		x::GTE_SetRotMatrix(sp);
		x::GTE_LoadV0(v);
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(v);
	}
	static inline void e6_rand_mod(uint32_t sp, int32_t off)
	{
		if (U16(sp, off) != 0)
		{
			int32_t r = (int32_t)x::CrtRand();
			U16(sp, off) = (uint16_t)(r % (int32_t)S16(sp, off));
		}
	}
	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	static inline bool b1_out(int16_t v, int16_t hi) { return v < 0 || v > hi; }
	static inline bool b2_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool b2_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
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

	static inline int32_t e4_div65536(int32_t v) { return v / 65536; }
	static void e4_world_to_view(uint32_t node, uint32_t src)
	{
		U32(node, 0xA0) = (uint32_t)e4_div65536(S32(src, 0));
		U32(node, 0xA4) = (uint32_t)e4_div65536(S32(src, 4));
		U32(node, 0xA8) = (uint32_t)e4_div65536(S32(src, 8));
		x::GTE_SetRotMatrix(MEM<uint32_t>(us(0x26481D8)));
		x::GTE_LoadIRFromMatrixColumn(node + 0x8C);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xAC);
		x::GTE_LoadIRFromMatrixColumn(node + 0x8E);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xAE);
		x::GTE_LoadIRFromMatrixColumn(node + 0x90);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(node + 0xB0);
		x::GTE_SetTransVector(MEM<uint32_t>(us(0x26481D8)));
		x::GTE_LoadV0FromDwords(node + 0xA0);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(node + 0xC0);
	}
	void e4_build_rot(uint32_t node, int32_t sub, uint32_t parent)
	{
		uint32_t m = node + 0x8C;
		switch ((uint32_t)sub)
		{
			case 0:
				if (parent != 0)
					ff8fx::copy<(32)>((void *)m, (void *)(parent + 0x2C));
				else
					x::MAG_022_sub_8DD770(m);
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				break;
			case 1:
				if (parent != 0)
					ff8fx::copy<(32)>((void *)m, (void *)(parent + 0x2C));
				else
					x::MAG_022_sub_8DD770(m);
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			case 2:
				x::UnpackRotationMatrix(MEM<uint32_t>(us(0x26481D8)), m);
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				break;
			case 3:
				x::UnpackRotationMatrix(MEM<uint32_t>(us(0x26481D8)), m);
				if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			default:
				break;
		}
	}

	static void FlashDraw(uint32_t colour)
	{
		const uint32_t bucket = MEM<uint32_t>(us(0x1D8E04C)) + 0x1C;
		uint32_t pkt = MEM<uint32_t>(us(PACKET_CURSOR));
		U32(pkt, 4) = colour;
		U32(pkt, 0) = 0x5000000;
		U8(pkt, 7) = 0x2A;
		U32(pkt, 8) = 0;
		U32(pkt, 0xC) = 0x140;
		U32(pkt, 0x10) = 0xD80000;
		U32(pkt, 0x14) = 0xD80140;
		x::SSIGPU_InsertPrimAltViewport(bucket, pkt);
		pkt += 0x18;
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAutoDepth(bucket, pkt);
		MEM<uint32_t>(us(PACKET_CURSOR)) = pkt + 0xC;
	}

	uint32_t __cdecl mb_824E40(uint32_t a1);
	uint32_t __cdecl mb_824FC0(uint32_t a1);
	uint32_t __cdecl mb_825010(void);
	uint32_t __cdecl mb_8250F0(uint32_t a1);
	uint32_t __cdecl mb_825170(uint32_t a1);
	uint32_t __cdecl mb_825220(uint32_t a1);
	uint32_t __cdecl mb_825320(uint32_t a1);
	uint32_t __cdecl mb_825410(uint32_t a1);
	uint32_t __cdecl mb_825440(uint32_t a1);
	uint32_t __cdecl mb_8255F0(uint32_t a1);
	uint32_t __cdecl mb_8256D0(void);
	uint32_t __cdecl mb_825830(void);
	uint32_t __cdecl mb_825A20(void);
	uint32_t __cdecl mb_825E90(uint32_t a1);
	uint32_t __cdecl mb_825FD0(void);
	uint32_t __cdecl mb_826030(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_8262B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_8264C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_826C20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_826E50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_827110(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_827370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mb_827650(void);
	uint32_t __cdecl mb_827710(uint32_t a1);
	uint32_t __cdecl mb_827790(void);
	uint32_t __cdecl mb_8277D0(uint32_t a1);
	uint32_t __cdecl mb_828070(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_828BA0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_828CB0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_828D30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_828D60(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl mb_828E00(uint32_t a1);
	uint32_t __cdecl mb_828E80(uint32_t a1);
	uint32_t __cdecl mb_828F00(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl mb_829900(uint32_t a1);
	uint32_t __cdecl mb_82A3F0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mb_82A480(void);
	uint32_t __cdecl mb_82A510(uint32_t a1);
	uint32_t __cdecl mb_82A540(uint32_t a1);
	uint32_t __cdecl mb_82A5C0(uint32_t a1);
	uint32_t __cdecl mb_82A5E0(uint32_t a1);
	uint32_t __cdecl mb_82A630(uint32_t a1);
	uint32_t __cdecl mb_82A670(uint32_t a1);
	uint32_t __cdecl mb_82A700(uint32_t a1);
	uint32_t __cdecl mb_82A720(uint32_t a1);
}
}
}

namespace ff8fx
{
namespace act
{
namespace magmabreath
{
	uint32_t __cdecl mb_824E40(uint32_t a1)
	{
		g_mod = &MOD_048;

		uint32_t states[11];
		ff8fx::copy<(8 * 4)>((void *)us(0x2793E58), (const void *)us(0x1D97778));
		const uint32_t node = a1;
		MEM<uint32_t>(us(SCRATCH_SP)) = us(0x2793E58);
		MEM<uint32_t>(us(0x26481D8)) = us(0x2793E58);
		states[0] = us(0x824FA0);
		states[1] = us(0x824FB0);
		const uint8_t parity = U8(node, 0x5C);
		states[2] = us(0x824FC0);
		states[3] = us(0x825140);
		states[4] = us(0x82A630);
		states[5] = us(0x82A670);
		states[6] = us(0x82A770);
		states[7] = us(0x82A780);
		states[8] = us(0x82A790);
		states[9] = us(0x82A7A0);
		states[10] = us(0x82A7C0);
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x26495BC));
			const uint32_t v2 = MEM<uint32_t>(us(0x26489B4));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x26481D4)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x26495B8));
			const uint32_t v2 = MEM<uint32_t>(us(0x26489B0));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x26481D4)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(us(0x26495D2)) = 0;
		MEM<uint16_t>(us(0x26495C2)) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_EMITTER)));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_FLASH)));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_ACTORS)));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_SHAKE)));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_STAGE)));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl mb_8250F0(uint32_t a1)
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

	uint32_t __cdecl mb_825220(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF)
			U8(node, 0x26) |= 1;
		else
		{
			if ((hi >> 8) == 0xFE)
			{
				if (S16(node, 0x24) >= (int16_t)(hi & 0xFF))
				{
					const uint16_t next = (uint16_t)(idx + 1);
					U16(node, 0x38) = next;
					U32(node, 0x34) = U32(script, (int32_t)(int16_t)next * 4);
				}
			}
			else
			{
				U32(node, 0x34) = entry;
				U16(node, 0x38) = (uint16_t)(idx + 1);
			}
			FlashDraw(U32(node, 0x34));
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl mb_825410(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			mb_825440(a1);
			mb_825E90(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	uint32_t __cdecl mb_825440(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;
		const uint32_t root = U32(node, 0x10);
		MEM<uint32_t>(us(0x26495E8)) = st;
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
			st = MEM<uint32_t>(us(0x26495E8));
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
		mb_8256D0();
		st = MEM<uint32_t>(us(0x26495E8));
		if (U16(st, 0x24) == 4)
			callp(us(0x8255F0), node);
		mb_825A20();
		st = MEM<uint32_t>(us(0x26495E8));
		if (U16(st, 0x24) == 1)
		{
			callp(us(0x825830), node);
			st = MEM<uint32_t>(us(0x26495E8));
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(us(0x26495E8));
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
		st = MEM<uint32_t>(us(0x26495E8));
		U32(st, 0) = dist;
		return 0;
	}

	uint32_t __cdecl mb_8255F0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(us(0x26495E8));
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

	uint32_t __cdecl mb_8256D0(void)
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

	uint32_t __cdecl mb_825830(void)
	{
		const uint32_t arena = MEM<uint32_t>(us(0x26495E8));
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

	uint32_t __cdecl mb_825A20(void)
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

	uint32_t __cdecl mb_825FD0(void)
	{
		uint32_t act = U32(U32(us(0x26495E8), 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(us(0x26495E8), 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					mb_826030(act, def);
			}
			act = U32(act, 4);
		}
		return 0;
	}

	uint32_t __cdecl mb_826030(uint32_t a1, uint32_t a2)
	{
		uint32_t ctx = MEM<uint32_t>(us(SCRATCH_SP)) - 0x58;
		const bool def3a = U16(a2, 0x3A) != 0;
		const uint32_t model = U32(a1, 0x170);
		MEM<uint32_t>(us(SCRATCH_SP)) = ctx;
		const uint32_t saved_ctx = ctx;
		U32(ctx, 0) = model;
		U32(ctx, 0x1C) = 0;
		if (!def3a)
			U32(ctx, 0x1C) = 0x30;
		const uint16_t col = U16(a1, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(a1, 0x16C);
			const uint32_t fl = U32(ctx, 0x1C);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x1C) = fl | 0xC0;
		}
		const int8_t copies = S8(a1, 0x1D8);
		if (copies == 1)
		{
			const uint32_t m = a1 + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			const uint32_t cursor = MEM<uint32_t>(us(PACKET_CURSOR));
			const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
			MEM<uint32_t>(us(PACKET_CURSOR)) = a_7355E0(ctx, ot, 2, cursor);
		}
		else if (copies > 0)
		{
			const uint32_t m = a1 + 0xAC;
			uint32_t pos = a1 + 0x19E;
			int32_t i = 0;
			int32_t n;
			do
			{
				S32(a1, 0xC0) = S16(pos, -2);
				S32(a1, 0xC4) = S16(pos, 0);
				S32(a1, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				const uint32_t cursor = MEM<uint32_t>(us(PACKET_CURSOR));
				const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
				const uint32_t r = a_7355E0(saved_ctx, ot, 2, cursor);
				n = S8(a1, 0x1D8);
				i++;
				pos += 8;
				MEM<uint32_t>(us(PACKET_CURSOR)) = r;
			} while (i < n);
		}
		const uint32_t top = MEM<uint32_t>(us(SCRATCH_SP)) + 0x58;
		MEM<uint32_t>(us(SCRATCH_SP)) = top;
		return top;
	}

	uint32_t __cdecl mb_8262B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x4000000;
			U32(pk, -8) = code;
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					x::GTE_AVSZ3();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x14;
						pk += 0x14;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	uint32_t __cdecl mb_8264C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x5000000;
			U32(pk, -8) = code;
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					const uint32_t v3 = vbase + (uint32_t)U16(face, 0xA) * 4;
					x::GTE_LoadV0(v3);
					x::GTE_RTPS();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					x::GTE_ReadSXY2(pk + 8);
					x::GTE_AVSZ4();
					if (b1_out(S16(pk, 8), 0xA00)) clip |= 8;
					if (b1_out(S16(pk, 0xA), 0x6C0)) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x18;
						pk += 0x18;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	uint32_t __cdecl mb_826C20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x6000000;
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x1C;
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	uint32_t __cdecl mb_826E50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x8000000;
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x20))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, rec + 0x14);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 0x1C);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10), c3 = U32(rec, 0x14);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
							U32(pkt, 0x1C) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	uint32_t __cdecl mb_827110(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t uv2 = U32(rec, 8) >> 16;
			U32(pkt, 0x18) = uv1 + toff;
			U32(pkt, 0x24) = uv2 + toff;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	uint32_t __cdecl mb_827370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t w23 = (toff << 16) + toff;
			uv1 += toff;
			w23 += U32(rec, 0x14);
			U32(pkt, 0x24) = w23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = w23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x18, rec + 0x1C, rec + 0x20);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	uint32_t __cdecl mb_827650(void)
	{
		uint32_t act = U32(MEM<uint32_t>(us(0x26495E8)), 0x2C);
		if (act == 0)
			return 0;
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(us(0x26495E8));
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						mb_827710(act);
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
							mb_827710(act);
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

	uint32_t __cdecl mb_827710(uint32_t a1)
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

	uint32_t __cdecl mb_827790(void)
	{
		uint32_t act = U32(U32(us(0x26495E8), 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				mb_828E00(act);
			else if (type == 1)
				mb_8277D0(act);
			act = U32(act, 4);
		}
		return 0;
	}

	uint32_t __cdecl mb_8277D0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(us(0x26495E8));
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (mb_828CB0(act, bone) == 0)
			{
				a_740910(act, bone);
				mb_828070(act, bone);
				U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
				return 0;
			}
			U16(act, 0x1CC) = (uint16_t)(U16(act, 0x1CC) + 1);
			U32(act, 0x170) = 0;
			return 0;
		}
		if (phase == 1)
		{
			a_742CD0(act);
			st = MEM<uint32_t>(us(0x26495E8));
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0;
	}

	uint32_t __cdecl mb_828BA0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(us(0x26495E8), 0);
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
				gx::sub_45E0B0(in2 + delta);
				gx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				gx::sub_45E0B0(in2);
				gx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0;
	}

	uint32_t __cdecl mb_828CB0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				mb_828D30(a1, a2);
				return 1;
			}
		}
		else if (kind == 1)
		{
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) >= 0)
				return 0;
			mb_828D30(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		if (U8(a1, 0x1D2) == 1)
		{
			mb_828D30(a1, a2);
			return 1;
		}
		return 0;
	}

	uint32_t __cdecl mb_828D30(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return mb_828D60(a1, desc_index, anchor);
		}
		return a1;
	}

	uint32_t __cdecl mb_828D60(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(us(0x26495C4));
		int32_t idx = MEM<int16_t>(us(0x26489A8));
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t dir = MEM<uint32_t>(us(ACTOR_STATE));
				U8(slot, 0x69) = 1;
				U16(dir, 0x14) = (uint16_t)(U16(dir, 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			idx++;
			if (idx >= 0x13)
				idx = 0;
			tries++;
		} while (tries < 0x14);
		idx++;
		if (idx < 0x13)
			MEM<uint16_t>(us(0x26489A8)) = (uint16_t)idx;
		else
			MEM<uint16_t>(us(0x26489A8)) = 0;
		return slot;
	}

	uint32_t __cdecl mb_828E00(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(us(0x8297B0), a1);
		else if (mode == 4)
			callp(us(0x829900), a1);
		mb_828E80(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0;
	}

	uint32_t __cdecl mb_828E80(uint32_t a1)
	{
		uint32_t sys = U32(us(0x26495E8), 0);
		int32_t k = S8(a1, 0x6A);
		uint32_t def = U32(U32(sys, 0x224), k * 4);
		int16_t frame = S16(a1, 0x60);
		if (frame > 0x27)
			return 0;
		uint32_t total = U8(U32(def, 0xC8), (int32_t)frame);
		if (total == 0)
			return 0;
		int32_t batch = S8(def, 0x36);
		int32_t full = (int32_t)total / batch;
		int32_t rest = (int32_t)total % batch;
		if (full > 0)
		{
			do
			{
				int32_t b = S8(def, 0x36);
				mb_828F00(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			mb_828F00(a1, def, (uint32_t)rest);
		return 0;
	}

	uint32_t __cdecl mb_828F00(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t sp = U32(us(0x26495D8), 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(us(0x26495D8), 0) = sp;
		uint32_t node = a_7387B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;
		U16(sp, 0x4E) = w60;
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(us(0x26495E8), 0);
			int32_t k = S8(a1, 0x6A);
			uint32_t v = U32(U32(ctx, 0x228), shl32(k, 2));
			U32(node, 0x170) = v;
			U8(node, 0x1D1) = (uint8_t)a_7424B0(v);
			if (U8(desc, 0x16) == 2)
			{
				uint8_t b17 = U8(desc, 0x17);
				uint8_t b18 = U8(desc, 0x18);
				int32_t s1a = S8(desc, 0x1A);
				U8(node, 0x1D3) = b17;
				int32_t s19 = S8(desc, 0x19);
				U8(node, 0x1D4) = b18;
				U8(node, 0x1D5) = (uint8_t)a_742290((uint32_t)s19, (uint32_t)s1a);
			}
		}
		U16(node, 0x1DA) = U16(desc, 0x64);
		if ((int32_t)a3 > 0)
		{
			uint32_t src = a1 + 0x4C;
			uint32_t emat = a1 + 0x2C;
			uint32_t out_speed = node + 0x184;
			uint32_t mat = node + 0x0C;
			uint32_t pos = node + 0x12C;
			for (int32_t n = (int32_t)a3; n != 0; n--)
			{
				e6_copy16(pos - 0x50, src);
				if (U8(desc, 0x22) == 0)
					e6_copy16(pos, src);
				uint8_t spread = U8(desc, 0x34);
				if (spread == 1)
				{
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2C) = (uint16_t)(x::CrtRand() & 0xFFF);
					e6_rotate_zxy(sp, sp + 0x28);
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (U8(desc, 0x33) != 0)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					e6_rotate_down_vector(sp);
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = mul32(S16(sp, 0x4A), S16(sp, 0x22));
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					int32_t old = S32(pos, 0);
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(old, ex);
					old = S32(pos, 4);
					ey = shl32(ey, 4);
					S32(sp, 0x3C) = ey;
					S32(pos, 4) = add32(old, ey);
					old = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(old, ez);
				}
				else if (spread == 2)
				{
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = 0;
					U16(sp, 0x2C) = 0x400;
					e6_rotate_zxy(sp, sp + 0x28);
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (S8(desc, 0x33) != 1)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						e6_rand_mod(sp, 0x4A);
						int32_t e = S16(sp, 0x4E);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), shl32(e, 1));
					}
					if ((x::CrtRand() & 1) != 0)
						U16(sp, 0x4A) = (uint16_t)(0 - U16(sp, 0x4A));
					e6_rotate_down_vector(sp);
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = shl32(S16(sp, 0x4A), 16);
					int32_t oldx = S32(pos, 0);
					S32(sp, 0x3C) = ey;
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(oldx, ex);
					int32_t ey2 = S32(sp, 0x3C);
					S32(pos, 4) = add32(S32(pos, 4), ey2);
					int32_t oldz = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(oldz, ez);
				}
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					uint32_t c0 = U32(sp, 0x28);
					uint32_t c1 = U32(sp, 0x2C);
					U32(sp, 0x30) = c0;
					U32(sp, 0x34) = c1;
				}
				e6_rotate_zxy(mat, sp + 0x30);
				x::GTE_SetRotMatrix(emat);
				x::GTE_LoadIRFromMatrixColumn(mat);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat);
				x::GTE_LoadIRFromMatrixColumn(mat + 2);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 2);
				x::GTE_LoadIRFromMatrixColumn(mat + 4);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 4);
				if (U8(desc, 0x27) == 0)
				{
					uint32_t r1 = a_7422C0(U32(desc, 0x4C), U32(desc, 0x50));
					U32(out_speed, -0x10) = r1;
					uint32_t r2 = a_7422C0(U32(desc, 0x54), U32(desc, 0x58));
					U32(out_speed, 0) = r2;
				}
				out_speed += 4;
				pos += 0x10;
				mat += 0x20;
			}
		}
		a_742350(node + 0x194, desc + 0x148, desc + 0x150);
		uint8_t m1e = U8(desc, 0x1E);
		if (m1e == 1 || m1e == 2)
		{
			int32_t s20 = S8(desc, 0x20);
			int32_t s1f = S8(desc, 0x1F);
			U16(node, 0x1C8) = (uint16_t)a_742290((uint32_t)s1f, (uint32_t)s20);
		}
		if (U8(desc, 0x27) == 0)
		{
			U32(node, 0x1DC) = a_7422C0(U32(desc, 0x5C), U32(desc, 0x60));
			if ((int32_t)a3 > 0)
			{
				uint32_t q = node + 0x220;
				for (int32_t n = (int32_t)a3; n != 0; n--)
				{
					a_742300(q - 0x40, desc + 0x68, desc + 0x78);
					a_742300(q, desc + 0x88, desc + 0x98);
					a_742300(q + 0x40, desc + 0xA8, desc + 0xB8);
					q += 0x10;
				}
			}
		}
		mb_82A3F0(node, desc);
		U32(us(0x26495D8), 0) = U32(us(0x26495D8), 0) + 0x50;
		return 0;
	}

	uint32_t __cdecl mb_829900(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t st = U32(us(0x26495E8), 0);
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
				uint32_t g = U32(us(0x26495E8), 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };
				uint32_t g = U32(us(0x26495E8), 0);
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

	uint32_t __cdecl mb_82A3F0(uint32_t a1, uint32_t a2)
	{
		const uint32_t obj = a1;
		const uint32_t desc = a2;
		if (U8(desc, 0x28) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2C);
			mb_828D60(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			mb_828D60(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			mb_828D60(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			mb_828D60(obj, id, variant);
		}
		return 0;
	}

	uint32_t __cdecl mb_82A480(void)
	{
		uint32_t dir = MEM<uint32_t>(us(0x26495E8));
		for (int32_t slot = 0; slot < 0x10; slot++)
		{
			uint32_t table = U32(dir, 0x224);
			uint32_t entry = U32(table, slot * 4);
			if (entry == 0)
				continue;
			uint32_t bit = (uint32_t)shl32(1, slot);
			uint32_t mask = U16(dir, 0x22);
			if (bit & mask)
				continue;
			uint8_t kind = U8(entry, 0x11);
			if (kind != 0 && kind != 1 && kind != 4)
				continue;
			int16_t group = (int16_t)S8(entry, 0x13);
			if (group != S16(dir, 0x18))
				continue;
			if (U8(entry, 0x37) == 1)
			{
				int32_t n = 0;
				if (S16(dir, 0x1C) <= 0)
					continue;
				do
				{
					mb_828D60(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(us(0x26495E8));
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				mb_828D60(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(us(0x26495E8));
			}
		}
		return 0;
	}

	uint32_t __cdecl mb_82A510(uint32_t a1)
	{
		if (mb_825E90(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;
	}

	uint32_t __cdecl mb_82A540(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = us(0x82A590);
		states[1] = us(0x82A5B0);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl mb_82A670(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0;
		x::Effect_AddTaskAndInitFromCtx(us(Q_STAGE), us(0x82A6A0), 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl mb_82A700(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl mb_82A720(uint32_t a1)
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

	uint32_t __cdecl mb_824FC0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0;
		uint32_t p = MEM<uint32_t>(us(0x26495E4));
		MEM<uint32_t>(us(0x26495C4)) = p;
		p += 0x870;
		MEM<uint32_t>(us(0x26495DC)) = p;
		p += 0x10680;
		MEM<uint32_t>(us(0x26495E4)) = p;
		mb_825010();
		x::Effect_AddTaskAndInitFromCtx(us(Q_STAGE), us(0x825050), 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl mb_825010(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(us(0x26495C4)), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(us(0x26495DC)), 0x10680);
		MEM<uint16_t>(us(0x26489A8)) = 0;
		MEM<uint16_t>(us(0x26495D0)) = 0;
		MEM<uint16_t>(us(0x26495C0)) = 0;
		MEM<uint16_t>(us(0x26481BC)) = 0;
		return 0;
	}

	uint32_t __cdecl mb_825170(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = us(0x825320);
		states[1] = us(0x82A5C0);
		states[2] = us(0x82A5E0);
		states[3] = us(0x82A620);
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Magma()), 0, 0x80);
		if (U16(node, 0x24) == 4)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(us(Q_FLASH), us(ORIG_Flash), 0x3C, node);
			U32(t, 0x30) = us(FLASH_SCRIPT);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl mb_825320(uint32_t a1)
	{
		a_73C100(a1, us(ORIG_ActorSystem), us(ACTOR_DATA), 0xF, 0x28, 0);
		x::Effect_AddTaskAndInitFromCtx(us(Q_SHAKE), us(ORIG_Shake), 0x58, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl mb_82A5C0(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x1C)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	uint32_t __cdecl mb_82A5E0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x37)
			return 0;
		const int32_t ai = S8(node, 0x2A);
		const int32_t ti = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t recs = U32(acts + (uint32_t)(ai * 5) * 4, 8);
		x::ApplyActionResultToTarget(recs + (uint32_t)(ti * 3) * 8);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0;
	}

	uint32_t __cdecl mb_82A630(uint32_t a1)
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
		U16(node, 0x60) = 0x1B;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl mb_825E90(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		MEM<uint32_t>(us(ACTOR_STATE)) = sys;
		int16_t pos[4] = { 0, 0, 0, 0 };
		const uint32_t pp = P(pos);
		if (U16(sys, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(us(0x1D972C0) + (uint32_t)((int32_t)S16(sys, 0x1E) * 0x9C), 0x15, 0, pp);
			sys = MEM<uint32_t>(us(ACTOR_STATE));
		}
		S16(pp, 4) = (int16_t)(U16(pp, 4) + 0x100);
		S32(sys, 0x1F4) = shl32(S16(pp, 0), 16);
		S32(sys, 0x1F8) = shl32(S16(pp, 2), 16);
		S32(sys, 0x1FC) = shl32(S16(pp, 4), 16);
		if (U16(sys, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(us(0x1D972C0) + (uint32_t)((int32_t)S16(sys, 0x1E) * 0x9C), 0x15, 0, pp);
			sys = MEM<uint32_t>(us(ACTOR_STATE));
		}
		S16(pp, 4) = (int16_t)(U16(pp, 4) + 0x100);
		S32(sys, 0x204) = shl32(S16(pp, 0), 16);
		S32(sys, 0x208) = 0;
		S32(sys, 0x20C) = shl32(S16(pp, 4), 16);
		const uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			mb_82A480();
			mb_827790();
			mb_827650();
			mb_825FD0();
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
		const uint16_t c14 = U16(sys, 0x14);
		const uint16_t c16 = U16(sys, 0x16);
		MEM<uint16_t>(us(0x26495D2)) = (uint16_t)(MEM<uint16_t>(us(0x26495D2)) + c14);
		MEM<uint16_t>(us(0x26495C2)) = (uint16_t)(MEM<uint16_t>(us(0x26495C2)) + c16);
		return done;
	}

	uint32_t __cdecl mb_828070(uint32_t a1, uint32_t a2)
	{
		uint32_t node = a1;
		uint32_t desc = a2;
		uint8_t mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			uint32_t tex = U8(U32(desc, 0x12C) + frame, 0);
			uint32_t dir = MEM<uint32_t>(us(ACTOR_STATE));
			U32(node, 0x170) = U32(U32(dir, 0x22C) + tex * 4, 0);
			if (mode == 5)
				mb_828BA0(node, desc);
			bool scroll = U8(node, 0x1D6) == 0;
			if (scroll)
				a_7340A0(U32(node, 0x170), (uint32_t)-8);
		}

		int32_t rot_mode = (int32_t)S8(desc, 0x1B);
		uint32_t m = node + 0x8C;
		switch ((uint32_t)rot_mode)
		{
			case 0:
				x::UnpackRotationMatrix(MEM<uint32_t>(us(0x26481D8)), m);
				if (U16(node, 0xD0) != 0)
					x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			case 1:
				x::MAG_022_sub_8DD770(m);
				x::sub_8DD7E0(m, 0x400);
				break;
			case 2:
			{
				uint32_t yaw = x::sub_8DD7B0(MEM<uint32_t>(us(0x26481D8)));
				x::MAG_022_sub_8DD770(m);
				if ((uint16_t)yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)(int16_t)yaw);
				break;
			}
			case 3:
			case 4:
			{
				int32_t sub = (int32_t)S8(desc, 0x1D);
				uint32_t parent = U32(node, 0x1BC);
				e4_build_rot(node, sub, parent);
				break;
			}
			default:
				break;
		}

		mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			U8(node, 0x16C) = U8(U32(desc, 0xF0) + frame, 0);
			U8(node, 0x16D) = U8(U32(desc, 0xF4) + frame, 0);
			U8(node, 0x16E) = U8(U32(desc, 0xF8) + frame, 0);
			U16(node, 0x1CE) = U16(U32(desc, 0xFC) + frame * 2, 0);
			U16(node, 0xD4) = U16(U32(desc, 0xE4) + frame * 2, 0);
			if (U8(node, 0x1D6) == 0)
				U16(node, 0xD4) = (uint16_t)(mul32(S32(MEM<uint32_t>(us(ACTOR_STATE)), 0), 0x700) / 4096);
			U16(node, 0xD6) = U16(U32(desc, 0xE8) + frame * 2, 0);
			uint16_t sz = U16(U32(desc, 0xEC) + frame * 2, 0);
			int32_t scale[4];
			scale[0] = (int32_t)S16(node, 0xD4);
			scale[1] = (int32_t)S16(node, 0xD6);
			U16(node, 0xD8) = sz;
			scale[2] = (int32_t)(int16_t)sz;
			scale[3] = 0;
			x::scale3DMatrix(m, P(scale));
		}

		uint8_t count = U8(node, 0x1D8);
		if (count == 1)
		{
			e4_world_to_view(node, node + 0xDC);
			U32(node, 0xC8) = (uint32_t)add32(S32(node, 0xC8), (int32_t)S16(desc, 0x38));
			return 0;
		}
		if ((int8_t)count > 0)
		{
			int32_t i = 0;
			uint32_t src = node + 0xDC;
			uint32_t dst = node + 0x19C;
			do
			{
				e4_world_to_view(node, src);
				U16(dst, 0) = U16(node, 0xC0);
				U16(dst, 2) = U16(node, 0xC4);
				uint16_t z = (uint16_t)(U16(node, 0xC8) + U16(desc, 0x38));
				i++;
				src += 0x10;
				U16(dst, 4) = z;
				dst += 8;
			} while (i < (int32_t)S8(node, 0x1D8));
		}
		return 0;
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x824FA0, (void *)a_73A0D0, "048 MAG_048_sub_824FA0" },
		{ 0x824FB0, (void *)a_73A0D0, "048 MAG_048_sub_824FB0" },
		{ 0x825050, (void *)a_73A380, "048 MAG_048_sub_825050" },
		{ 0x8250B0, (void *)a_7335D0, "048 MAG_048_sub_8250B0" },
		{ 0x825130, (void *)a_73A6D0, "048 nullsub_1552" },
		{ 0x825140, (void *)a_73A170, "048 MAG_048_sub_825140" },
		{ 0x825360, (void *)a_73C100, "048 MAG_048_sub_825360" },
		{ 0x8253B0, (void *)a_73A380, "048 MAG_048_sub_8253B0" },
		{ 0x825900, (void *)a_73FE90, "048 sub_825900" },
		{ 0x825C30, (void *)a_740210, "048 sub_825C30" },
		{ 0x826170, (void *)a_7355E0, "048 sub_826170" },
		{ 0x826740, (void *)a_735BB0, "048 sub_826740" },
		{ 0x826980, (void *)a_735DF0, "048 sub_826980" },
		{ 0x827860, (void *)a_740910, "048 sub_827860" },
		{ 0x827EC0, (void *)a_740F70, "048 sub_827EC0" },
		{ 0x827F10, (void *)a_740FC0, "048 sub_827F10" },
		{ 0x827FA0, (void *)a_741050, "048 sub_827FA0" },
		{ 0x828840, (void *)a_7340A0, "048 sub_828840" },
		{ 0x828E50, (void *)a_741B60, "048 sub_828E50" },
		{ 0x829580, (void *)a_742290, "048 au_re__rand_43" },
		{ 0x8295B0, (void *)a_7422C0, "048 sub_8295B0" },
		{ 0x8295F0, (void *)a_742300, "048 sub_8295F0" },
		{ 0x829640, (void *)a_742350, "048 sub_829640" },
		{ 0x829690, (void *)a_7423A0, "048 au_re__rand_43_0" },
		{ 0x8296E0, (void *)a_7387B0, "048 sub_8296E0" },
		{ 0x8297A0, (void *)a_7424B0, "048 sub_8297A0" },
		{ 0x8297B0, (void *)a_7424C0, "048 sub_8297B0" },
		{ 0x829F90, (void *)a_742CA0, "048 sub_829F90" },
		{ 0x829FC0, (void *)a_742CD0, "048 sub_829FC0" },
		{ 0x829FF0, (void *)a_742D00, "048 sub_829FF0" },
		{ 0x82A1A0, (void *)a_742EB0, "048 sub_82A1A0" },
		{ 0x82A2E0, (void *)a_742FF0, "048 sub_82A2E0" },
		{ 0x82A530, (void *)a_73A6D0, "048 nullsub_1555" },
		{ 0x82A590, (void *)a_7475A0, "048 sub_82A590" },
		{ 0x82A5B0, (void *)a_73A6D0, "048 nullsub_1556" },
		{ 0x82A620, (void *)a_73A6D0, "048 nullsub_1553" },
		{ 0x82A6A0, (void *)a_73A380, "048 MAG_048_sub_82A6A0" },
		{ 0x82A760, (void *)a_73A6D0, "048 nullsub_1557" },
		{ 0x82A770, (void *)a_747550, "048 MAG_048_sub_82A770" },
		{ 0x82A780, (void *)a_73A0D0, "048 MAG_048_sub_82A780" },
		{ 0x82A790, (void *)a_73A0D0, "048 MAG_048_sub_82A790" },
		{ 0x82A7A0, (void *)a_7475A0, "048 MAG_048_sub_82A7A0" },
		{ 0x82A7C0, (void *)a_73A6D0, "048 nullsub_1554" },
		{ 0, nullptr, nullptr }
	};
	static const ModPort PORTS[] = {
		{ 0x824E40, (void *)mb_824E40, "048 mb_824E40" },
		{ 0x824FC0, (void *)mb_824FC0, "048 mb_824FC0" },
		{ 0x825010, (void *)mb_825010, "048 mb_825010" },
		{ 0x8250F0, (void *)mb_8250F0, "048 mb_8250F0" },
		{ 0x825170, (void *)mb_825170, "048 mb_825170" },
		{ 0x825220, (void *)mb_825220, "048 mb_825220" },
		{ 0x825320, (void *)mb_825320, "048 mb_825320" },
		{ 0x825410, (void *)mb_825410, "048 mb_825410" },
		{ 0x825440, (void *)mb_825440, "048 mb_825440" },
		{ 0x8255F0, (void *)mb_8255F0, "048 mb_8255F0" },
		{ 0x8256D0, (void *)mb_8256D0, "048 mb_8256D0" },
		{ 0x825830, (void *)mb_825830, "048 mb_825830" },
		{ 0x825A20, (void *)mb_825A20, "048 mb_825A20" },
		{ 0x825E90, (void *)mb_825E90, "048 mb_825E90" },
		{ 0x825FD0, (void *)mb_825FD0, "048 mb_825FD0" },
		{ 0x826030, (void *)mb_826030, "048 mb_826030" },
		{ 0x8262B0, (void *)mb_8262B0, "048 mb_8262B0" },
		{ 0x8264C0, (void *)mb_8264C0, "048 mb_8264C0" },
		{ 0x826C20, (void *)mb_826C20, "048 mb_826C20" },
		{ 0x826E50, (void *)mb_826E50, "048 mb_826E50" },
		{ 0x827110, (void *)mb_827110, "048 mb_827110" },
		{ 0x827370, (void *)mb_827370, "048 mb_827370" },
		{ 0x827650, (void *)mb_827650, "048 mb_827650" },
		{ 0x827710, (void *)mb_827710, "048 mb_827710" },
		{ 0x827790, (void *)mb_827790, "048 mb_827790" },
		{ 0x8277D0, (void *)mb_8277D0, "048 mb_8277D0" },
		{ 0x828070, (void *)mb_828070, "048 mb_828070" },
		{ 0x828BA0, (void *)mb_828BA0, "048 mb_828BA0" },
		{ 0x828CB0, (void *)mb_828CB0, "048 mb_828CB0" },
		{ 0x828D30, (void *)mb_828D30, "048 mb_828D30" },
		{ 0x828D60, (void *)mb_828D60, "048 mb_828D60" },
		{ 0x828E00, (void *)mb_828E00, "048 mb_828E00" },
		{ 0x828E80, (void *)mb_828E80, "048 mb_828E80" },
		{ 0x828F00, (void *)mb_828F00, "048 mb_828F00" },
		{ 0x829900, (void *)mb_829900, "048 mb_829900" },
		{ 0x82A3F0, (void *)mb_82A3F0, "048 mb_82A3F0" },
		{ 0x82A480, (void *)mb_82A480, "048 mb_82A480" },
		{ 0x82A510, (void *)mb_82A510, "048 mb_82A510" },
		{ 0x82A540, (void *)mb_82A540, "048 mb_82A540" },
		{ 0x82A5C0, (void *)mb_82A5C0, "048 mb_82A5C0" },
		{ 0x82A5E0, (void *)mb_82A5E0, "048 mb_82A5E0" },
		{ 0x82A630, (void *)mb_82A630, "048 mb_82A630" },
		{ 0x82A670, (void *)mb_82A670, "048 mb_82A670" },
		{ 0x82A700, (void *)mb_82A700, "048 mb_82A700" },
		{ 0x82A720, (void *)mb_82A720, "048 mb_82A720" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag048_magma_breath()
	{
		act::register_module(48);
		for (const act::magmabreath::ModPort *p = act::magmabreath::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(48, p->addr, p->port, p->name);
		for (const act::magmabreath::ModPort *p = act::magmabreath::PORTS; p->addr; p++)
			act::register_module_port(48, p->addr, p->port, p->name);
	}
}

