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

// Effect 30: Counter Laser-Eye: a copy

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace lasereye
{
	static const Mod MOD_030 = { "lasereye", 30, 0x8735D0, 0x879480,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D40DC, 0x0, 0x26D4768, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D65F8, 0x0, 0x26D6898, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26D6A12, 0x26D6A14, 0x0, 0x26D6A22, 0x0, 0x26D6A28, 0x0, 0x26D6A38 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x873C20, 0x874670, 0x874700, 0x874760, 0x0, 0x8749E0, 0x874BF0, 0x874E70, 0x8750B0, 0x875350, 0x875580, 0x875840, 0x875AA0, 0x875D80, 0x875EC0, 0x875F00, 0x877190, 0x8771E0, 0x877210, 0x877290, 0x877980, 0x8779D0, 0x877A70, 0x877B30, 0x878380, 0x8787E0, 0x878870, 0x0, 0x0, 0x0, 0x0, 0x0, 0x873A70, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x873DD0, 0x873EB0, 0x874010, 0x8740E0, 0x874200, 0x874410, 0x875E40, 0x875F90, 0x8765F0, 0x876640, 0x8766D0, 0x8767A0, 0x876F30, 0x877040, 0x8770C0, 0x8770F0, 0x877910, 0x877940, 0x877A20, 0x877B40, 0x877C90, 0x878320, 0x878350, 0x878590, 0x8786D0, 0x0, 0x874670, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	static const uint32_t PACKET_CURSOR = 0x26D40BC;
	static const uint32_t ACTOR_STATE = 0x26D6A38;
	static const uint32_t SCRATCH_SP = 0x26D6A28;
	static const uint32_t Q_EMITTER = 0x26D6898, Q_ACTORS = 0x26D65F8, Q_LASER = 0x26D41B8;
	static const uint32_t Q_SPARK = 0x26D4770, Q_STAGE = 0x26D6888;
	static const uint32_t ORIG_Master = 0x873740;
	static const uint32_t ORIG_Emitter = 0x873A70;
	static const uint32_t ORIG_ActorSystem = 0x873B90;
	static const uint32_t ORIG_Laser = 0x878930;
	static const uint32_t ORIG_Spark = 0x878F20;
	static const void *const SOUND_LaserEye = (const void *)0x160D3E8;
	static const uint32_t ACTOR_DATA = 0x17CC72C;
	static const uint32_t SPARK_TABLE = 0x160DED8;

	namespace gx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t TransformCameraByShadowRotation(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(a1, a2, a3); }
		static inline uint32_t InitEffectSequenceFromData(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(a1, a2, a3, a4); }
		static inline uint32_t Effect_BuildMatrixFromDirAndUp(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DDA50)(a1, a2, a3); }
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
	static inline int32_t div4864(int32_t v)
	{
		const int32_t hi = (int32_t)(((int64_t)v * 0x6BCA1AF3) >> 32);
		const int32_t q = hi >> 11;
		return q + (int32_t)((uint32_t)q >> 31);
	}
	static inline uint32_t sq3(uint32_t v)
	{
		const int32_t x = S16(v, 0), y = S16(v, 2), z = S16(v, 4);
		return (uint32_t)add32(add32(mul32(x, x), mul32(y, y)), mul32(z, z));
	}

	uint32_t __cdecl cl_873740(uint32_t a1);
	uint32_t __cdecl cl_8738C0(uint32_t a1);
	uint32_t __cdecl cl_873910(void);
	uint32_t __cdecl cl_873A70(uint32_t a1);
	uint32_t __cdecl cl_873B00(uint32_t a1);
	uint32_t __cdecl cl_879270(uint32_t a1);
	uint32_t __cdecl cl_879290(uint32_t a1);
	uint32_t __cdecl cl_878380(uint32_t a1);
	uint32_t __cdecl cl_878930(uint32_t a1);
	uint32_t __cdecl cl_878CC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_878D80(uint32_t a1);
	uint32_t __cdecl cl_878E70(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_879140(uint32_t a1);
	uint32_t __cdecl cl_879180(uint32_t a1);
	uint32_t __cdecl cl_8791B0(uint32_t a1);
	uint32_t __cdecl cl_8791F0(uint32_t a1);
	uint32_t __cdecl cl_879220(uint32_t a1);
	uint32_t __cdecl cl_878F80(uint32_t a1);
	uint32_t __cdecl cl_878FF0(uint32_t a1);
	uint32_t __cdecl cl_879050(uint32_t a1);
	uint32_t __cdecl cl_8739F0(uint32_t a1);
	uint32_t __cdecl cl_873BF0(uint32_t a1);
	uint32_t __cdecl cl_873C20(uint32_t a1);
	uint32_t __cdecl cl_873DD0(uint32_t a1);
	uint32_t __cdecl cl_873EB0(void);
	uint32_t __cdecl cl_874010(void);
	uint32_t __cdecl cl_874200(void);
	uint32_t __cdecl cl_874670(uint32_t a1);
	uint32_t __cdecl cl_874700(void);
	uint32_t __cdecl cl_874760(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_8749E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_874BF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_875350(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_875580(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_875840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_875AA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl cl_875D80(void);
	uint32_t __cdecl cl_875E40(uint32_t a1);
	uint32_t __cdecl cl_875EC0(void);
	uint32_t __cdecl cl_875F00(uint32_t a1);
	uint32_t __cdecl cl_876F30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_877040(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_8770C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_8770F0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl cl_877190(uint32_t a1);
	uint32_t __cdecl cl_877210(uint32_t a1);
	uint32_t __cdecl cl_877290(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl cl_877A70(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_877C90(uint32_t a1);
	uint32_t __cdecl cl_8787E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl cl_878870(void);
	uint32_t __cdecl cl_878900(uint32_t a1);
	uint32_t __cdecl cl_878F20(uint32_t a1);
	uint32_t __cdecl cl_8792E0(uint32_t a1);
	uint32_t __cdecl cl_879320(uint32_t a1);
	uint32_t __cdecl cl_8793B0(uint32_t a1);
	uint32_t __cdecl cl_8793D0(uint32_t a1);
	static void laser_update(uint32_t node);
}
}
}

#ifdef FF8_FX_HELD
#include "mag030_counter_laser_eye_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace lasereye
{

	uint32_t __cdecl cl_873740(uint32_t a1)
	{
		g_mod = &MOD_030;
		FX_HELD(held_note_master();)

		uint32_t states[11];
		ff8fx::copy<(8 * 4)>((void *)0x2793E58, (const void *)0x1D97778);
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x26D40DC) = 0x2793E58;
		states[0] = 0x8738A0;
		states[1] = 0x8738B0;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x8738C0;
		states[3] = 0x873A40;
		states[4] = 0x8792E0;
		states[5] = 0x879320;
		states[6] = 0x879420;
		states[7] = 0x879430;
		states[8] = 0x879440;
		states[9] = 0x879450;
		states[10] = 0x879470;
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26D6A0C);
			const uint32_t v2 = MEM<uint32_t>(0x26D5E04);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26D40D8) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26D6A08);
			const uint32_t v2 = MEM<uint32_t>(0x26D5E00);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26D40D8) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x26D6A22) = 0;
		MEM<uint16_t>(0x26D6A12) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_LASER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SPARK));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl cl_8738C0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0;
		uint32_t p = MEM<uint32_t>(0x26D6A34);
		MEM<uint32_t>(0x26D6A14) = p;
		p += 0x870;
		MEM<uint32_t>(0x26D6A2C) = p;
		p += 0x8340;
		MEM<uint32_t>(0x26D6A34) = p;
		cl_873910();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x873950, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl cl_873910(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26D6A14), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x26D6A2C), 0x8340);
		MEM<uint16_t>(0x26D4768) = 0;
		MEM<uint16_t>(0x26D6A20) = 0;
		MEM<uint16_t>(0x26D6A10) = 0;
		MEM<uint16_t>(0x26D40C0) = 0;
		return 0;
	}

	uint32_t __cdecl cl_873A70(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x873B00;
		states[1] = 0x879270;
		states[2] = 0x879290;
		states[3] = 0x8792D0;
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_LaserEye), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl cl_873B00(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, ACTOR_DATA, 0, 0x2D, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_LASER, ORIG_Laser, 0x90, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl cl_879270(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0xA)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	uint32_t __cdecl cl_879290(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0xA)
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

	uint32_t __cdecl cl_878380(uint32_t a1)
	{
		alignas(4) uint8_t frame[0x10] = {};
		const uint32_t f = P(frame);
		uint32_t dir = MEM<uint32_t>(ACTOR_STATE);
		const uint32_t table = U32(dir, 0x224);
		const int32_t di = (int32_t)S8(a1, 0x6A);
		const uint32_t owner = U32(a1, 0x5C);
		const uint32_t desc = U32(table + di * 4, 0);
		U8(a1, 0x68) = U8(desc, 0x11);
		const uint32_t m = a1 + 0x2C;
		switch ((uint32_t)(int32_t)S8(desc, 0x1C))
		{
			case 0:
				x::MAG_022_sub_8DD770(m);
				break;
			case 1:
			{
				x::MAG_022_sub_8DD770(m);
				const int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				const uint32_t d = MEM<uint32_t>(ACTOR_STATE);
				const uint32_t e = (uint32_t)k + d;
				const int32_t dx = sub32(S32(d, 0x1C4), S32(e, 0x54));
				const int32_t dz = sub32(S32(d, 0x1CC), S32(e, 0x5C));
				const uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 2:
			{
				S16(f, 2) = 0x1000;
				S16(f, 0) = 0;
				S16(f, 4) = 0;
				const int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				const uint32_t e = (uint32_t)k + dir;
				S16(f, 8) = (int16_t)b2_fx((uint32_t)sub32(S32(e, 0xD4), S32(dir, 0x1F4)));
				S16(f, 0xA) = (int16_t)b2_fx((uint32_t)sub32(S32(e, 0xD8), S32(dir, 0x1F8)));
				S16(f, 0xC) = (int16_t)b2_fx((uint32_t)sub32(S32(e, 0xDC), S32(dir, 0x1FC)));
				gx::Effect_BuildMatrixFromDirAndUp(m, f + 8, f);
				break;
			}
			case 3:
				ff8fx::copy<(32)>((void *)m, (void *)(owner + 0x8C));
				break;
			case 4:
			{
				x::MAG_022_sub_8DD770(m);
				dir = MEM<uint32_t>(ACTOR_STATE);
				const int32_t ent = (int32_t)S16(dir, 0x1E);
				const int32_t yaw = (int32_t)S16(0x1D972CE + (uint32_t)(ent * 39) * 4, 0);
				if (yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)yaw);
				break;
			}
			default:
				break;
		}
		switch ((uint32_t)(int32_t)S8(a1, 0x68))
		{
			case 0: a_742EB0(a1); break;
			case 1: a_742FF0(a1); break;
			case 2:
			case 3: a_7424C0(a1); break;
			case 4: cl_877C90(a1); break;
			default: break;
		}
		U16(a1, 0x64) = (uint16_t)(int16_t)S8(desc, 0x12);
		return 0;
	}

	static void laser_update(uint32_t node)
	{
		alignas(4) uint8_t frame[0x48] = {};
		const uint32_t f = P(frame);
		U32(f, 0x28) = 0x879140;
		U32(f, 0x2C) = 0x879180;
		U32(f, 0x30) = 0x8791B0;
		U32(f, 0x34) = 0x8791F0;
		U32(f, 0x38) = 0x879220;
		U32(f, 0x3C) = 0x879260;
		if (U16(node, 0x24) == 0)
			x::GetEffectSpawnPosition(0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C, 0xF1, 0, node + 0x7C);
		x::GetEffectSpawnPosition(0x1D972C0 + (uint32_t)U8(U32(node, 0xC), 0) * 0x9C, 0xF1, 0, node + 0x74);
		S16(f, 8) = 0;
		S16(f, 0xA) = 0x80;
		S16(f, 0xC) = 0;
		x::matrixMultiplyVector(0x1D97300 + (uint32_t)U8(U32(node, 0xC), 0) * 0x9C, f + 8, f + 0x10);
		U16(node, 0x74) = (uint16_t)(U16(node, 0x74) + U16(f, 0x10));
		const uint16_t ex = U16(node, 0x74);
		const uint16_t tx = U16(node, 0x7C);
		U16(node, 0x76) = (uint16_t)(U16(node, 0x76) + U16(f, 0x12));
		const uint16_t ey = U16(node, 0x76);
		U16(node, 0x78) = (uint16_t)(U16(node, 0x78) + U16(f, 0x14));
		const uint16_t ez = U16(node, 0x78);
		U16(f, 2) = (uint16_t)(U16(node, 0x7E) - ey);
		U16(f, 0) = (uint16_t)(tx - ex);
		S16(f, 0x18) = 0;
		S16(f, 0x1A) = 0x1000;
		S16(f, 0x1C) = 0;
		U16(f, 4) = (uint16_t)(U16(node, 0x80) - ez);
		gx::Effect_BuildMatrixFromDirAndUp(node + 0x54, f, f + 0x18);
		U16(node, 0x8E) = (uint16_t)x::Sqrt(sq3(f));
		callp(U32(f, 0x28 + 4 * S8(node, 0x29)), node);
		x::MAG_022_sub_8DD8A0(node + 0x54, (uint32_t)(int32_t)S16(node, 0x46));
		const int32_t w = S16(node, 0x8C);
		S32(node, 0x38) = div4864(mul32(w, S16(node, 0x8E)));
		U32(node, 0x4C) = 0x160D538;
		S16(f, 0x18) = 0;
		S16(f, 0x1A) = 0;
		S16(f, 0x1C) = (int16_t)(-0x80 - mul32(w, 0x1300) / 4096);
		x::matrixMultiplyVector(node + 0x54, f + 0x18, f + 0x10);
		U16(node, 0x84) = (uint16_t)(U16(node, 0x1C) + U16(f, 0x10));
		U16(node, 0x86) = (uint16_t)(U16(node, 0x1E) + U16(f, 0x12));
		U16(node, 0x88) = (uint16_t)(U16(node, 0x20) + U16(f, 0x14));
		cl_878CC0(node, node + 0x54);
		const uint16_t dx = (uint16_t)(U16(node, 0x7C) - U16(node, 0x74));
		const uint16_t dz = (uint16_t)(U16(node, 0x80) - U16(node, 0x78));
		S16(f, 0x18) = 0;
		U16(f, 0) = dx;
		S16(f, 0x1A) = 0x1000;
		S16(f, 0x1C) = 0;
		S16(f, 2) = 0;
		U16(f, 4) = dz;
		gx::Effect_BuildMatrixFromDirAndUp(node + 0x54, f, f + 0x18);
		x::MAG_022_sub_8DD8A0(node + 0x54, (uint32_t)(int32_t)S16(node, 0x46));
		const uint16_t len = (uint16_t)x::Sqrt(sq3(f));
		const int32_t w2 = S16(node, 0x8C);
		U16(node, 0x8E) = len;
		U16(node, 0x1E) = 0;
		S32(node, 0x38) = div4864(mul32(w2, (int16_t)len));
		U32(node, 0x4C) = 0x160DCF8;
		cl_878CC0(node, node + 0x54);
		if (U16(node, 0x8C) != 0x1000)
		{
			U32(node, 0x4C) = 0x160DC50;
			const uint32_t s0 = U32(node, 0x30);
			U32(f, 0x1C) = U32(node, 0x34);
			U32(f, 0x20) = U32(node, 0x38);
			const uint32_t p0 = U32(node, 0x84);
			const uint32_t s3 = U32(node, 0x3C);
			U32(node, 0x38) = 0x1000;
			U32(node, 0x34) = 0x1000;
			U32(node, 0x30) = 0x1000;
			U32(f, 0x24) = s3;
			const uint32_t p1 = U32(node, 0x88);
			U32(node, 0x1C) = p0;
			U32(node, 0x20) = p1;
			x::UnpackRotationMatrix(0x1D97778, f + 0x28);
			cl_878CC0(node, f + 0x28);
			U32(node, 0x30) = s0;
			U32(node, 0x34) = U32(f, 0x1C);
			U32(node, 0x38) = U32(f, 0x20);
			U32(node, 0x3C) = U32(f, 0x24);
		}
	}

	uint32_t __cdecl cl_878930(uint32_t a1)
	{
		const uint32_t node = a1;
		laser_update(node);
		cl_878D80(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl cl_878CC0(uint32_t a1, uint32_t a2)
	{
		FX_HELD(if (held_laser_draw(a1, a2)) return 0;)
		if ((U8(a1, 0x26) & 4) != 0)
			return 0;
		alignas(4) uint8_t mtx[0x20];
		const uint32_t m = P(mtx);
		const int32_t px = S16(a1, 0x1C);
		const int32_t pz = S16(a1, 0x20);
		ff8fx::copy<(0x20)>(mtx, (const void *)a2);
		const int32_t py = S16(a1, 0x1E);
		S32(m, 0x14) = px;
		S32(m, 0x18) = py;
		S32(m, 0x1C) = pz;
		x::scale3DMatrix(m, a1 + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		x::GTE_SetRotMatrix_W(m);
		x::GTE_SetTransVector_W(m);
		const uint32_t hdr = x::Field_Alloc(0x58);
		U32(hdr, 0) = U32(a1, 0x4C);
		U32(hdr, 8) = U32(a1, 0x40);
		S32(hdr, 0xC) = S16(a1, 0x50);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(hdr, 0x1C) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, cursor);
		x::Field_Free(0x58);
		return 0;
	}

	uint32_t __cdecl cl_878D80(uint32_t a1)
	{
		const int16_t tick = S16(a1, 0x24);
		if (tick > 0x12)
			return 0;
		const uint32_t rec = SPARK_TABLE + (uint32_t)((int32_t)tick * 8);
		int32_t count = S16(rec, 4);
		const uint32_t sp = MEM<uint32_t>(SCRATCH_SP) - 8;
		MEM<uint32_t>(SCRATCH_SP) = sp;
		if (count > 0)
		{
			const uint32_t pt = sp + 4;
			do
			{
				const int32_t r = (int32_t)x::CrtRand();
				const int32_t t = S16(rec, 0) + r % (int32_t)S16(rec, 2);
				S32(pt, 0) = t;
				S32(sp, 0) = 0x1000 - t;
				const uint32_t spark = x::Effect_AddTaskAndInitFromCtx(Q_SPARK, ORIG_Spark, 0x60, a1);
				x::set_dword_1CA8A30(U32(sp, 0));
				gx::sub_45E0B0(a1 + 0x74);
				gx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(pt, 0));
				gx::sub_45E0B0(a1 + 0x7C);
				gx::sub_45EBF0();
				x::GTE_StoreIR123(spark + 0x1C);
				cl_878E70(spark, 0x80);
				count--;
			} while (count != 0);
		}
		MEM<uint32_t>(SCRATCH_SP) = MEM<uint32_t>(SCRATCH_SP) + 8;
		return 0;
	}

	uint32_t __cdecl cl_878E70(uint32_t a1, uint32_t a2)
	{
		alignas(4) uint8_t frame[0x30] = {};
		const uint32_t f = P(frame);
		uint32_t range = a2;
		if ((uint16_t)range == 0)
			range = 1;
		const uint32_t yaw = x::CrtRand() & 0xFFF;
		const uint32_t pitch = x::CrtRand() & 0xFFF;
		x::MAG_022_sub_8DD770(f + 0x10);
		x::MAG_022_sub_8DD8A0(f + 0x10, (uint32_t)(int32_t)(int16_t)yaw);
		x::sub_8DD7E0(f + 0x10, (uint32_t)(int32_t)(int16_t)pitch);
		S16(f, 0) = 0;
		S16(f, 2) = 0;
		const int32_t r = (int32_t)x::CrtRand();
		S16(f, 4) = (int16_t)(r % (int32_t)(int16_t)range);
		x::matrixMultiplyVector(f + 0x10, f, f + 8);
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + U16(f, 8));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + U16(f, 0xA));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + U16(f, 0xC));
		return 0;
	}

	uint32_t __cdecl cl_879140(uint32_t a1)
	{
		const uint32_t e0 = U32(a1, 0x74);
		U8(a1, 0x26) |= 4;
		const uint32_t e1 = U32(a1, 0x78);
		U32(a1, 0x1C) = e0;
		U16(a1, 0x46) = 0x800;
		U32(a1, 0x38) = 0x1000;
		U32(a1, 0x34) = 0x1000;
		U32(a1, 0x30) = 0x1000;
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x20) = e1;
		U32(a1, 0x4C) = 0x160D538;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl cl_879180(uint32_t a1)
	{
		const uint32_t e0 = U32(a1, 0x74);
		const uint32_t e1 = U32(a1, 0x78);
		const int16_t tick = S16(a1, 0x24);
		U32(a1, 0x1C) = e0;
		U16(a1, 0x46) = 0x800;
		U32(a1, 0x20) = e1;
		if (tick >= 4)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) &= 0xFB;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	uint32_t __cdecl cl_8791B0(uint32_t a1)
	{
		const uint32_t e0 = U32(a1, 0x74);
		U16(a1, 0x8C) = (uint16_t)(U16(a1, 0x8C) + 0x400);
		const uint32_t e1 = U32(a1, 0x78);
		U32(a1, 0x1C) = e0;
		const int16_t w = S16(a1, 0x8C);
		U16(a1, 0x46) = 0x800;
		U32(a1, 0x20) = e1;
		if (w >= 0x1000)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x8C) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	uint32_t __cdecl cl_8791F0(uint32_t a1)
	{
		const uint32_t e0 = U32(a1, 0x74);
		const uint32_t e1 = U32(a1, 0x78);
		const int16_t tick = S16(a1, 0x24);
		U32(a1, 0x1C) = e0;
		U16(a1, 0x46) = 0x800;
		U32(a1, 0x20) = e1;
		if (tick >= 0xE)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	uint32_t __cdecl cl_879220(uint32_t a1)
	{
		const uint32_t t0 = U32(a1, 0x7C);
		U16(a1, 0x8C) = (uint16_t)(U16(a1, 0x8C) + 0xF800);
		const int16_t w = S16(a1, 0x8C);
		U32(a1, 0x1C) = t0;
		const uint32_t t1 = U32(a1, 0x80);
		U16(a1, 0x46) = 0;
		U32(a1, 0x20) = t1;
		if (w <= 0)
		{
			U8(a1, 0x26) |= 5;
			U16(a1, 0x8C) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	uint32_t __cdecl cl_878F80(uint32_t a1)
	{
		FX_HELD(held_note_spark(a1);)
		if ((U8(a1, 0x26) & 4) != 0)
			return 0;
		const uint32_t hdr = x::Field_Alloc(0xB4);
		gx::TransformCameraByShadowRotation(a1 + 0x1C, 0x1000, (uint32_t)(int32_t)S16(a1, 0x54));
		U32(hdr, 0) = U32(a1, 0x4C);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		U16(hdr, 4) = U16(a1, 0x50);
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = gx::InitEffectSequenceFromData(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, cursor);
		x::Field_Free(0xB4);
		return 0;
	}

	uint32_t __cdecl cl_878FF0(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x160DE10;
		U16(a1, 0x52) = 6;
		U16(a1, 0x58) = (uint16_t)((x::CrtRand() & 0x3FF) - 0x200);
		U16(a1, 0x5A) = (uint16_t)((x::CrtRand() & 0x3FF) - 0x200);
		U16(a1, 0x5C) = (uint16_t)((x::CrtRand() & 0x3FF) - 0x200);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl cl_879050(uint32_t a1)
	{
		int16_t vx = S16(a1, 0x58);
		int16_t vy = S16(a1, 0x5A);
		int16_t vz = S16(a1, 0x5C);
		vx = (int16_t)(vx - (int16_t)(((int32_t)vx << 8) / 4096));
		vy = (int16_t)(vy - (int16_t)(((int32_t)vy << 8) / 4096));
		S16(a1, 0x58) = vx;
		vz = (int16_t)(vz - (int16_t)(((int32_t)vz << 8) / 4096));
		S16(a1, 0x5A) = vy;
		S16(a1, 0x5C) = vz;
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + (uint16_t)((int32_t)vx / 16));
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + (uint16_t)((int32_t)vy / 16));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + (uint16_t)((int32_t)vz / 16));
		if (a_743C20(a1) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	uint32_t __cdecl cl_8739F0(uint32_t a1)
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
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0;
	}

	uint32_t __cdecl cl_873BF0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			cl_873C20(a1);
			cl_874670(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;
	}

	uint32_t __cdecl cl_873C20(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;
		const uint32_t root = U32(node, 0x10);
		MEM<uint32_t>(0x26D6A38) = st;
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
			st = MEM<uint32_t>(0x26D6A38);
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
		cl_873EB0();
		st = MEM<uint32_t>(0x26D6A38);
		if (U16(st, 0x24) == 4)
			callp(0x873DD0, node);
		cl_874200();
		st = MEM<uint32_t>(0x26D6A38);
		if (U16(st, 0x24) == 1)
		{
			callp(0x874010, node);
			st = MEM<uint32_t>(0x26D6A38);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(0x26D6A38);
		}
		int32_t s1 = S16(st, 0x1E);
		uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		uint32_t d10 = U32(e1, 0x1C);
		uint32_t d14 = U32(e1, 0x20);
		uint32_t s2 = U32(st, 4);
		uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		uint32_t d18 = U32(e2, 0x1C);
		int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(0x26D6A38);
		U32(st, 0) = dist;
		return 0;
	}

	uint32_t __cdecl cl_873DD0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x26D6A38);
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

	uint32_t __cdecl cl_873EB0(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		const int32_t slot = S16(st, 0x1E);
		const uint32_t ent = 0x1D972C0 + (uint32_t)mul32(slot, 0x9C);
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
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
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		S32(st, 0x1E4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1E8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1EC) = shl32(S16(pp, 4), 16);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
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

	uint32_t __cdecl cl_874010(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x26D6A38);
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

	uint32_t __cdecl cl_874200(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (S16(st, 0x1C) > 0)
		{
			uint32_t o = 0;
			for (int32_t i = 0; ; )
			{
				const uint32_t slot = U32(st, 4 + 4 * i);
				const uint32_t ent = 0x1D972C0 + slot * 0x9C;
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
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
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				S32(st, o + 0xD4) = shl32(S16(pp, 0), 16);
				S32(st, o + 0xD8) = shl32(S16(pp, 2), 16);
				S32(st, o + 0xDC) = shl32(S16(pp, 4), 16);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
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

	uint32_t __cdecl cl_874670(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x26D6A38, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			cl_878870();
			cl_875EC0();
			FX_HELD(held_note_system(sys);)
			cl_875D80();
			cl_874700();
			sys = U32(0x26D6A38, 0);
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
		U16(0x26D6A22, 0) = (uint16_t)(U16(0x26D6A22, 0) + c14);
		U16(0x26D6A12, 0) = (uint16_t)(U16(0x26D6A12, 0) + c16);
		return done;
	}

	uint32_t __cdecl cl_874700(void)
	{
		uint32_t act = U32(U32(0x26D6A38, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x26D6A38, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					cl_874760(act, def);
			}
			act = U32(act, 4);
		}
		return 0;
	}

	uint32_t __cdecl cl_874760(uint32_t a1, uint32_t a2)
	{
		uint32_t ctx = MEM<uint32_t>(SCRATCH_SP) - 0x58;
		const bool def3a = U16(a2, 0x3A) != 0;
		const uint32_t model = U32(a1, 0x170);
		MEM<uint32_t>(SCRATCH_SP) = ctx;
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
			const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
			const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			MEM<uint32_t>(PACKET_CURSOR) = a_7355E0(ctx, ot, 2, cursor);
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
				const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
				const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
				const uint32_t r = a_7355E0(saved_ctx, ot, 2, cursor);
				n = S8(a1, 0x1D8);
				i++;
				pos += 8;
				MEM<uint32_t>(PACKET_CURSOR) = r;
			} while (i < n);
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x58;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	uint32_t __cdecl cl_8749E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_874BF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_875350(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_875580(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_875840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_875AA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	uint32_t __cdecl cl_875D80(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x26D6A38), 0x2C);
		if (act == 0)
			return 0;
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x26D6A38);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						cl_875E40(act);
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
							cl_875E40(act);
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

	uint32_t __cdecl cl_875E40(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t hdr = MEM<uint32_t>(SCRATCH_SP) - 0xB4;
		MEM<uint32_t>(SCRATCH_SP) = hdr;
		const uint32_t mtx = act + 0xAC;
		x::GTE_SetRotMatrix(mtx);
		x::GTE_SetTransVector(mtx);
		const uint32_t seq = U32(act, 0x170);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const int16_t variant = (int16_t)S8(act, 0x1D0);
		U32(hdr, 0) = seq;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		S16(hdr, 4) = variant;
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = x::InitEffectSequenceFromData_c19(hdr, ot, 2, cursor);
		MEM<uint32_t>(SCRATCH_SP) = MEM<uint32_t>(SCRATCH_SP) + 0xB4;
		return 0;
	}

	uint32_t __cdecl cl_875EC0(void)
	{
		uint32_t act = U32(U32(0x26D6A38, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				cl_877190(act);
			else if (type == 1)
				cl_875F00(act);
			act = U32(act, 4);
		}
		return 0;
	}

	uint32_t __cdecl cl_875F00(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x26D6A38);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (cl_877040(act, bone) == 0)
			{
				a_740910(act, bone);
				a_741120(act, bone);
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
			st = MEM<uint32_t>(0x26D6A38);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0;
	}

	uint32_t __cdecl cl_876F30(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x26D6A38, 0);
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

	uint32_t __cdecl cl_877040(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				cl_8770C0(a1, a2);
				return 1;
			}
		}
		else if (kind == 1)
		{
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) >= 0)
				return 0;
			cl_8770C0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		if (U8(a1, 0x1D2) == 1)
		{
			cl_8770C0(a1, a2);
			return 1;
		}
		return 0;
	}

	uint32_t __cdecl cl_8770C0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return cl_8770F0(a1, desc_index, anchor);
		}
		return a1;
	}

	uint32_t __cdecl cl_8770F0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x26D6A14);
		int32_t idx = MEM<int16_t>(0x26D4768);
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t dir = MEM<uint32_t>(ACTOR_STATE);
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
			MEM<uint16_t>(0x26D4768) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x26D4768) = 0;
		return slot;
	}

	uint32_t __cdecl cl_877190(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			cl_878380(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x877B40, a1);
		else if (mode == 4)
			callp(0x877C90, a1);
		cl_877210(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0;
	}

	uint32_t __cdecl cl_877210(uint32_t a1)
	{
		uint32_t sys = U32(0x26D6A38, 0);
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
				cl_877290(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			cl_877290(a1, def, (uint32_t)rest);
		return 0;
	}

	uint32_t __cdecl cl_877290(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t sp = U32(0x26D6A28, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x26D6A28, 0) = sp;
		uint32_t node = cl_877A70(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;
		U16(sp, 0x4E) = w60;
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x26D6A38, 0);
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
		cl_8787E0(node, desc);
		U32(0x26D6A28, 0) = U32(0x26D6A28, 0) + 0x50;
		return 0;
	}

	uint32_t __cdecl cl_877A70(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x26D6A2C);
		int32_t i = MEM<int16_t>(0x26D6A20);
		uint32_t slot = 0;
		int32_t tries = 0;
		for (;;)
		{
			const uint32_t off = (uint32_t)mul32(i, 0x2A0);
			if (U8(off + pool, 0x1D7) == 0)
			{
				slot = (uint32_t)mul32(i, 0x2A0) + pool;
				x::MAG_007_sub_8DCC00(slot, 0x2A0);
				const uint8_t def_idx = (uint8_t)a2;
				U32(slot, 0x1BC) = a1;
				const uint8_t b6b = U8(a1, 0x6B);
				const uint32_t sys = MEM<uint32_t>(ACTOR_STATE);
				U8(slot, 0x1D7) = 1;
				U16(sys, 0x16) = (uint16_t)(U16(sys, 0x16) + 1);
				U8(slot, 0x1D6) = def_idx;
				U8(slot, 0x1D9) = b6b;
				a_742CA0(slot, 1);
				break;
			}
			i++;
			if (i >= 0x31)
				i = 0;
			tries++;
			if (tries >= 0x32)
				break;
		}
		i++;
		if (i >= 0x31)
			MEM<uint16_t>(0x26D6A20) = 0;
		else
			MEM<uint16_t>(0x26D6A20) = (uint16_t)i;
		return slot;
	}

	uint32_t __cdecl cl_877C90(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t st = U32(0x26D6A38, 0);
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
				uint32_t g = U32(0x26D6A38, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };
				uint32_t g = U32(0x26D6A38, 0);
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

	uint32_t __cdecl cl_8787E0(uint32_t a1, uint32_t a2)
	{
		const uint32_t obj = a1;
		const uint32_t desc = a2;
		if (U8(desc, 0x28) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2C);
			cl_8770F0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			cl_8770F0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			cl_8770F0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			cl_8770F0(obj, id, variant);
		}
		return 0;
	}

	uint32_t __cdecl cl_878870(void)
	{
		uint32_t dir = MEM<uint32_t>(0x26D6A38);
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
					cl_8770F0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x26D6A38);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				cl_8770F0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x26D6A38);
			}
		}
		return 0;
	}

	uint32_t __cdecl cl_878900(uint32_t a1)
	{
		if (cl_874670(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;
	}

	uint32_t __cdecl cl_878F20(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x878FF0;
		states[1] = 0x879050;
		states[2] = 0x879130;
		callp(states[S8(node, 0x29)], node);
		cl_878F80(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	uint32_t __cdecl cl_8792E0(uint32_t a1)
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

	uint32_t __cdecl cl_879320(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0;
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x879350, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;
	}

	uint32_t __cdecl cl_8793B0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	uint32_t __cdecl cl_8793D0(uint32_t a1)
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
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0;
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8738A0, (void *)a_73A0D0, "030 MAG_030_sub_8738A0" },
		{ 0x8738B0, (void *)a_73A0D0, "030 MAG_030_sub_8738B0" },
		{ 0x873950, (void *)a_73A380, "030 MAG_030_sub_873950" },
		{ 0x8739B0, (void *)a_7335D0, "030 MAG_030_sub_8739B0" },
		{ 0x873A30, (void *)a_73A6D0, "030 nullsub_1683" },
		{ 0x873A40, (void *)a_73A170, "030 MAG_030_sub_873A40" },
		{ 0x873B40, (void *)a_73C100, "030 MAG_030_sub_873B40" },
		{ 0x873B90, (void *)a_73A380, "030 MAG_030_sub_873B90" },
		{ 0x8740E0, (void *)a_73FE90, "030 sub_8740E0" },
		{ 0x874410, (void *)a_740210, "030 sub_874410" },
		{ 0x8748A0, (void *)a_7355E0, "030 sub_8748A0" },
		{ 0x874E70, (void *)a_735BB0, "030 sub_874E70" },
		{ 0x8750B0, (void *)a_735DF0, "030 sub_8750B0" },
		{ 0x875F90, (void *)a_740910, "030 sub_875F90" },
		{ 0x8765F0, (void *)a_740F70, "030 sub_8765F0" },
		{ 0x876640, (void *)a_740FC0, "030 sub_876640" },
		{ 0x8766D0, (void *)a_741050, "030 sub_8766D0" },
		{ 0x8767A0, (void *)a_741120, "030 sub_8767A0" },
		{ 0x8771E0, (void *)a_741B60, "030 sub_8771E0" },
		{ 0x877910, (void *)a_742290, "030 au_re__rand_56" },
		{ 0x877940, (void *)a_7422C0, "030 sub_877940" },
		{ 0x877980, (void *)a_742300, "030 sub_877980" },
		{ 0x8779D0, (void *)a_742350, "030 sub_8779D0" },
		{ 0x877A20, (void *)a_7423A0, "030 au_re__rand_56_0" },
		{ 0x877B30, (void *)a_7424B0, "030 sub_877B30" },
		{ 0x877B40, (void *)a_7424C0, "030 sub_877B40" },
		{ 0x878320, (void *)a_742CA0, "030 sub_878320" },
		{ 0x878350, (void *)a_742CD0, "030 sub_878350" },
		{ 0x878590, (void *)a_742EB0, "030 sub_878590" },
		{ 0x8786D0, (void *)a_742FF0, "030 sub_8786D0" },
		{ 0x878920, (void *)a_73A6D0, "030 nullsub_1684" },
		{ 0x879100, (void *)a_743C20, "030 sub_879100" },
		{ 0x879130, (void *)a_73A6D0, "030 nullsub_1685" },
		{ 0x879260, (void *)a_73A6D0, "030 nullsub_1686" },
		{ 0x8792D0, (void *)a_73A6D0, "030 nullsub_1687" },
		{ 0x879350, (void *)a_73A380, "030 MAG_030_sub_879350" },
		{ 0x879410, (void *)a_73A6D0, "030 nullsub_1689" },
		{ 0x879420, (void *)a_747550, "030 MAG_030_sub_879420" },
		{ 0x879430, (void *)a_73A0D0, "030 MAG_030_sub_879430" },
		{ 0x879440, (void *)a_73A0D0, "030 MAG_030_sub_879440" },
		{ 0x879450, (void *)a_7475A0, "030 MAG_030_sub_879450" },
		{ 0x879470, (void *)a_73A6D0, "030 nullsub_1688" },
		{ 0, nullptr, nullptr }
	};
	static const ModPort PORTS[] = {
		{ 0x873740, (void *)cl_873740, "030 cl_873740" },
		{ 0x8738C0, (void *)cl_8738C0, "030 cl_8738C0" },
		{ 0x873910, (void *)cl_873910, "030 cl_873910" },
		{ 0x8739F0, (void *)cl_8739F0, "030 cl_8739F0" },
		{ 0x873A70, (void *)cl_873A70, "030 cl_873A70" },
		{ 0x873B00, (void *)cl_873B00, "030 cl_873B00" },
		{ 0x873BF0, (void *)cl_873BF0, "030 cl_873BF0" },
		{ 0x873C20, (void *)cl_873C20, "030 cl_873C20" },
		{ 0x873DD0, (void *)cl_873DD0, "030 cl_873DD0" },
		{ 0x873EB0, (void *)cl_873EB0, "030 cl_873EB0" },
		{ 0x874010, (void *)cl_874010, "030 cl_874010" },
		{ 0x874200, (void *)cl_874200, "030 cl_874200" },
		{ 0x874670, (void *)cl_874670, "030 cl_874670" },
		{ 0x874700, (void *)cl_874700, "030 cl_874700" },
		{ 0x874760, (void *)cl_874760, "030 cl_874760" },
		{ 0x8749E0, (void *)cl_8749E0, "030 cl_8749E0" },
		{ 0x874BF0, (void *)cl_874BF0, "030 cl_874BF0" },
		{ 0x875350, (void *)cl_875350, "030 cl_875350" },
		{ 0x875580, (void *)cl_875580, "030 cl_875580" },
		{ 0x875840, (void *)cl_875840, "030 cl_875840" },
		{ 0x875AA0, (void *)cl_875AA0, "030 cl_875AA0" },
		{ 0x875D80, (void *)cl_875D80, "030 cl_875D80" },
		{ 0x875E40, (void *)cl_875E40, "030 cl_875E40" },
		{ 0x875EC0, (void *)cl_875EC0, "030 cl_875EC0" },
		{ 0x875F00, (void *)cl_875F00, "030 cl_875F00" },
		{ 0x876F30, (void *)cl_876F30, "030 cl_876F30" },
		{ 0x877040, (void *)cl_877040, "030 cl_877040" },
		{ 0x8770C0, (void *)cl_8770C0, "030 cl_8770C0" },
		{ 0x8770F0, (void *)cl_8770F0, "030 cl_8770F0" },
		{ 0x877190, (void *)cl_877190, "030 cl_877190" },
		{ 0x877210, (void *)cl_877210, "030 cl_877210" },
		{ 0x877290, (void *)cl_877290, "030 cl_877290" },
		{ 0x877A70, (void *)cl_877A70, "030 cl_877A70" },
		{ 0x877C90, (void *)cl_877C90, "030 cl_877C90" },
		{ 0x878380, (void *)cl_878380, "030 cl_878380" },
		{ 0x8787E0, (void *)cl_8787E0, "030 cl_8787E0" },
		{ 0x878870, (void *)cl_878870, "030 cl_878870" },
		{ 0x878900, (void *)cl_878900, "030 cl_878900" },
		{ 0x878930, (void *)cl_878930, "030 cl_878930" },
		{ 0x878CC0, (void *)cl_878CC0, "030 cl_878CC0" },
		{ 0x878D80, (void *)cl_878D80, "030 cl_878D80" },
		{ 0x878E70, (void *)cl_878E70, "030 cl_878E70" },
		{ 0x878F20, (void *)cl_878F20, "030 cl_878F20" },
		{ 0x878F80, (void *)cl_878F80, "030 cl_878F80" },
		{ 0x878FF0, (void *)cl_878FF0, "030 cl_878FF0" },
		{ 0x879050, (void *)cl_879050, "030 cl_879050" },
		{ 0x879140, (void *)cl_879140, "030 cl_879140" },
		{ 0x879180, (void *)cl_879180, "030 cl_879180" },
		{ 0x8791B0, (void *)cl_8791B0, "030 cl_8791B0" },
		{ 0x8791F0, (void *)cl_8791F0, "030 cl_8791F0" },
		{ 0x879220, (void *)cl_879220, "030 cl_879220" },
		{ 0x879270, (void *)cl_879270, "030 cl_879270" },
		{ 0x879290, (void *)cl_879290, "030 cl_879290" },
		{ 0x8792E0, (void *)cl_8792E0, "030 cl_8792E0" },
		{ 0x879320, (void *)cl_879320, "030 cl_879320" },
		{ 0x8793B0, (void *)cl_8793B0, "030 cl_8793B0" },
		{ 0x8793D0, (void *)cl_8793D0, "030 cl_8793D0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag030_counter_laser_eye()
	{
		act::register_module(30);
		for (const act::lasereye::ModPort *p = act::lasereye::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(30, p->addr, p->port, p->name);
		for (const act::lasereye::ModPort *p = act::lasereye::PORTS; p->addr; p++)
			act::register_module_port(30, p->addr, p->port, p->name);
		FX_HELD(register_mag030_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag030_counter_laser_eye_held.inc"
#endif
