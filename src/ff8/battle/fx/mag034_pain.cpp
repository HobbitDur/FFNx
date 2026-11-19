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

// Effect 34: Pain: a copy of the actor/heal effect library

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace pain
{
	static const Mod MOD_034 = { "pain", 34, 0x8655E0, 0x865FF0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26C0118, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8658A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	static const uint32_t PACKET_CURSOR = 0x26BA9E4;
	static const uint32_t Q_EMITTER = 0x26C0118;
	static const uint32_t Q_PLAYER = 0x26C0100;
	static const uint32_t BLEND_BUFFER = 0x26C0298;
	static uint32_t g_block40 = 0;

	static const uint32_t ORIG_Master = 0x865720;
	static const uint32_t ORIG_Emitter = 0x8658A0;
	static const uint32_t ORIG_GlowPlayer = 0x8659D0;
	static const uint32_t ORIG_ModelPlayer = 0x865DB0;
	static const uint32_t ORIG_DrawCallback = 0x865AE0;
	static const void *SOUND_Pain() { return (const void *)us(0x15E703C); }

	inline uint32_t RenderPrimModelGlow(uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(us(0x8DE9D0))(hdr, ot, mode, cursor); }

	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
}
}
}

namespace ff8fx
{
namespace act
{
namespace pain
{
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_034;

		uint32_t states[11];
		ff8fx::copy<(8 * 4)>((void *)us(0x2793E58), (const void *)us(0x1D97778));
		const uint32_t node = a1;
		MEM<uint32_t>(us(0x26C0374)) = us(0x2793E58);
		MEM<uint32_t>(us(0x26BAA00)) = us(0x2793E58);
		states[0] = us(0x865840);
		const uint8_t parity = U8(node, 0x5C);
		states[1] = us(0x865850);
		states[2] = us(0x865860);
		states[3] = us(0x865870);
		states[4] = us(0x865F40);
		states[5] = us(0x865F80);
		states[6] = us(0x865F90);
		states[7] = us(0x865FA0);
		states[8] = us(0x865FB0);
		states[9] = us(0x865FC0);
		states[10] = us(0x865FE0);
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x26C028C));
			const uint32_t v2 = MEM<uint32_t>(us(0x26C0114));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x26BA9FC)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(us(0x26C0288));
			const uint32_t v2 = MEM<uint32_t>(us(0x26C0110));
			MEM<uint32_t>(us(PACKET_CURSOR)) = v1;
			MEM<uint32_t>(us(0x26BA9FC)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		g_block40 = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_EMITTER)));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(us(Q_PLAYER)));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	static uint32_t __cdecl WaitChildren(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = us(0x865930);
		states[1] = us(0x865ED0);
		states[2] = us(0x865EF0);
		states[3] = us(0x865F30);
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Pain()), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	static uint32_t SpawnPlayer(uint32_t parent, uint32_t fn_addr, uint32_t model, uint32_t anim, uint32_t w80, uint32_t w82)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(us(Q_PLAYER), fn_addr, 0x89C, parent);
		U16(t, 0x80) = (uint16_t)w80;
		U32(t, 0x74) = model;
		S32(t, 0x78) = (int16_t)anim;
		U16(t, 0x82) = (uint16_t)w82;
		return t;
	}

	static uint32_t __cdecl SpawnModels(uint32_t a1)
	{
		SpawnPlayer(a1, us(ORIG_GlowPlayer), us(0x15E7040), 0x808, 0, 1);
		SpawnPlayer(a1, us(ORIG_ModelPlayer), us(0x15EA2E8), 0x280, 1, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	static uint32_t __cdecl PlayerInit(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		const uint32_t anim = U32(node, 0x78);
		const uint32_t entity = slot * 0x9C + us(0x1D972C0);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, anim);
		x::GetEffectSpawnPosition(entity, 0xF1, 0, node + 0x1C);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0;
	}

	static uint32_t __cdecl PlayerPlay(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		U32(L, 0x48) = us(BLEND_BUFFER);
		U32(L, 0x20) = U32(node, 0x1C);
		U32(L, 0x24) = U32(node, 0x20);
		U32(L, 0x40) = U16(node, 0x82) == 1 ? 1 : g_block40;
		g_block40 = U32(L, 0x40);
		if (prim_play(node + 0x94, us(ORIG_DrawCallback), L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0;
	}

	static uint32_t __cdecl DrawCallback(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0;
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0;

		const uint32_t hdr = x::Field_Alloc(0x58);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);
		const int16_t f0 = S16(rec, 0x28);
		U32(hdr, 0) = base;
		auto frame_ptr = [base](int16_t f) -> uint32_t {
			if (f == 0)
				return base + 0xC;
			const int32_t n = mul32(S32(base, 4), (int32_t)f);
			return base + (uint32_t)shl32(n, 3) + 0xC;
		};
		if (f0 == f1)
			U32(hdr, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);
			if (t == 0)
				U32(hdr, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(hdr, 4) = frame_ptr(f1);
			else
			{
				x::MAG_017_sub_701390(base, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, U32(blk, 0x48));
				U32(hdr, 4) = U32(blk, 0x48);
			}
		}

		x::MAG_017_sub_701310(rec + 0x10, M);
		const uint32_t flags = U32(rec, 4);
		uint16_t vx = U16(rec, 8);
		uint16_t vy = U16(rec, 0xA);
		uint16_t vz = U16(rec, 0xC);
		U16(L, 0x10) = vx;
		U16(L, 0x12) = vy;
		U16(L, 0x14) = vz;
		if ((flags & 0x200) != 0)
		{
			x::GTE_SetRotMatrix(us(0x1D97778));
			x::GTE_LoadV0(blk + 0x20);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			S32(M, 0x14) = add32(S32(M, 0x14), S16(L, 0x10));
			S32(M, 0x18) = add32(S32(M, 0x18), S16(L, 0x12));
			S32(M, 0x1C) = add32(S32(M, 0x1C), S16(L, 0x14));
		}
		else
		{
			vx = (uint16_t)(vx + U16(blk, 0x20));
			vy = (uint16_t)(vy + U16(blk, 0x22));
			vz = (uint16_t)(vz + U16(blk, 0x24));
			U16(L, 0x10) = vx;
			U16(L, 0x12) = vy;
			U16(L, 0x14) = vz;
			x::GTE_SetRotMatrix(us(0x1D97778));
			x::GTE_LoadV0(L + 0x10);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(us(0x1D97778), M);
		}
		S32(M, 0x14) = add32(S32(M, 0x14), MEM<int32_t>(us(0x1D9778C)));
		S32(M, 0x18) = add32(S32(M, 0x18), MEM<int32_t>(us(0x1D97790)));
		S32(M, 0x1C) = add32(S32(M, 0x1C), MEM<int32_t>(us(0x1D97794)));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(L, 0x18) = S16(rec, 0x18);
			S32(L, 0x1C) = S16(rec, 0x1A);
			S32(L, 0x20) = S16(rec, 0x1C);
			x::scale3DMatrix(M, L + 0x18);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const int32_t fade = S16(rec, 0x24);
		U32(hdr, 0x1C) = 0x2030;
		U32(hdr, 0xC) = (uint32_t)fade;
		if (fade != 0)
		{
			U32(hdr, 8) = U32(rec, 0x20);
			U32(hdr, 0x1C) = 0x20F0;
		}
		const uint32_t ot = MEM<uint32_t>(us(0x1D8E04C)) + 0x44;
		if (U32(blk, 0x40) == 1)
		{
			U32(hdr, 0x1C) |= 0x400;
			U32(hdr, 0x10) = 0;
			MEM<uint32_t>(us(PACKET_CURSOR)) = RenderPrimModelGlow(hdr, ot, 2, MEM<uint32_t>(us(PACKET_CURSOR)));
		}
		else
			MEM<uint32_t>(us(PACKET_CURSOR)) = x::Effect_RenderPrimModel(hdr, ot, 2, MEM<uint32_t>(us(PACKET_CURSOR)));
		x::Field_Free(0x58);
		return 0;
	}

	static uint32_t __cdecl ReleaseMaster(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x14)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	static uint32_t __cdecl Damage(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x14)
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

	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x865840, (void *)a_73A0D0, "034 MAG_034_sub_865840" },
		{ 0x865850, (void *)a_73A0D0, "034 MAG_034_sub_865850" },
		{ 0x865870, (void *)a_73A170, "034 MAG_034_sub_865870" },
		{ 0x8659D0, (void *)a_73A380, "034 MAG_034_sub_8659D0" },
		{ 0x865DA0, (void *)a_73A6D0, "034 nullsub_1653" },
		{ 0x865DB0, (void *)a_73A380, "034 MAG_034_sub_865DB0" },
		{ 0x865EC0, (void *)a_73A6D0, "034 nullsub_1654" },
		{ 0x865F30, (void *)a_73A6D0, "034 nullsub_1655" },
		{ 0x865F40, (void *)a_747500, "034 MAG_034_sub_865F40" },
		{ 0x865F80, (void *)a_73A0D0, "034 MAG_034_sub_865F80" },
		{ 0x865F90, (void *)a_747550, "034 MAG_034_sub_865F90" },
		{ 0x865FA0, (void *)a_73A0D0, "034 MAG_034_sub_865FA0" },
		{ 0x865FB0, (void *)a_73A0D0, "034 MAG_034_sub_865FB0" },
		{ 0x865FC0, (void *)a_7475A0, "034 MAG_034_sub_865FC0" },
		{ 0x865FE0, (void *)a_73A6D0, "034 nullsub_1652" },
		{ 0, nullptr, nullptr }
	};
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "034 Pain Master" },
		{ 0x865860, (void *)WaitChildren, "034 WaitChildren" },
		{ ORIG_Emitter, (void *)Emitter, "034 Emitter" },
		{ 0x865930, (void *)SpawnModels, "034 SpawnModels" },
		{ 0x865A30, (void *)PlayerInit, "034 GlowPlayer Init" },
		{ 0x865E10, (void *)PlayerInit, "034 ModelPlayer Init" },
		{ 0x865A80, (void *)PlayerPlay, "034 GlowPlayer Play" },
		{ 0x865E60, (void *)PlayerPlay, "034 ModelPlayer Play" },
		{ ORIG_DrawCallback, (void *)DrawCallback, "034 DrawCallback" },
		{ 0x865ED0, (void *)ReleaseMaster, "034 ReleaseMaster" },
		{ 0x865EF0, (void *)Damage, "034 Damage" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag034_pain()
	{
		act::register_module(34);
		for (const act::pain::ModPort *p = act::pain::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(34, p->addr, p->port, p->name);
		for (const act::pain::ModPort *p = act::pain::PORTS; p->addr; p++)
			act::register_module_port(34, p->addr, p->port, p->name);
	}
}

