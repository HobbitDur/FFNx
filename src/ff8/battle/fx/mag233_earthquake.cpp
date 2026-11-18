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

// Effect 233: Earthquake

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag233_earthquake_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_lev_a(Clone &c);

namespace earthquake
{
	static Clone g_eq = {};

	static void describe(Clone &c)
	{
		c.name = "233 Earthquake";
		c.effect_id = 233;
		c.vm_table = us(0x1868750);
		c.bone_table = us(0x18683C0);
		c.prim_table = us(0x1868408);
		c.prim_size = us(0x18684F8);
		c.spr_table = us(0x186850C);
		c.ptype_table = us(0x1868538);
		c.pop_table = us(0x186854C);
		c.draw_table = us(0x18685AC);
		c.attr_16A = us(0x1868716);
		c.attr_184 = us(0x1868730);
		c.attr_186 = us(0x1868732);
		c.attr_194 = us(0x1868740);
		c.attr_19C = us(0x1868748);
		c.desc = us(0x18682F8);
		c.ptr_block = us(0x1868398);
		c.queue = us(0x2796A20);
		c.file00 = us(0x2796A34);
		c.file01 = us(0x2796A30);
		c.tick = us(0xA525B0);
		c.load_cb = us(0xA57CA0);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static inline void xl_OffscreenStage_B65B00(void *blk, void *pos, void *angles)
	{
		FX_HELD(if (held_predicting()) return;)
		x::f<int32_t (__cdecl *)(void *, void *, void *)>(us(0xB65B00))(blk, pos, angles);
	}

	static inline int32_t xl_Sqrt(int32_t v) { return x::f<int32_t (__cdecl *)(int32_t)>(us(0x56BEC0))(v); }

	static void __cdecl op_06F_OffscreenStageGround()
	{
		if (U16(RCTX(), 0x30) != 0)
		{
			STREAM() += 6;
			return;
		}
		U16(WS(), 0xD0) = U16(STREAM(), 2);
		U16(WS(), 0xD2) = U16(STREAM(), 4);
		uint8_t *blk = PTR(CTX(), 0x74);
		FX_HELD(guard(blk, 0x10);)
		U32(blk, 0) = 0;
		U32(blk, 4) = 0;
		U32(blk, 0) = U32(CTX(), 0x74) + 0xEC;
		U32(blk, 0xC) = 0x5BEC;
		U32(blk, 8) = U32(CTX(), 0x74) + 0x10EC;
		U32(CUR(), 0xC4) = 0x5BEC;
		blob::ArenaAlloc((int32_t)U32(blk, 0xC));
		U32(blk, 0x14) = U32(CTX(), 0x74);
		uint8_t *cur = CUR();
		xl_OffscreenStage_B65B00(blk, cur + 0x94, cur + 0x8C);
		STREAM() += 6;
	}

	static void __cdecl op_070_RideWaterColumn()
	{
		uint8_t *saved = CUR();
		uint8_t *col = blob::GetBone(S16(STREAM(), 2));
		CUR() = col;
		uint8_t *par = blob::GetBone((int32_t)U16(col, 0x9C));
		CUR() = saved;
		int32_t dx = S16(saved, 0x94) - S16(par, 0x94);
		dx = dx - S16(col, 0x94);
		int32_t dz = S16(saved, 0x98) - S16(par, 0x98);
		dz = dz - S16(col, 0x98);
		int32_t d = xl_Sqrt(add32(mul32(dz, dz), mul32(dx, dx)));
		uint8_t *blk = PTR(col, 0xBC);
		int32_t h = (int32_t)(uint32_t)RT();
		if (blk != nullptr)
		{
			if (d > S16(blk, 0x9E))
				h = 0;
			else
			{
				for (int32_t o = 2; o < 0x40; o += 2)
				{
					if (d < S16(blk, o + 0x60))
					{
						h = S16(blk, o + 0x1E);
						break;
					}
				}
			}
		}
		h = add32(h, S16(par, 0x96));
		int32_t y = add32(S16(col, 0x96), h);
		U32(CUR(), 0x60) = (uint32_t)shl32(y, 0x10);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 4;
	}

	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_draw18(Clone &c)
	{
		ff8fx::copy<(sizeof(g_save_vm))>(g_save_vm, c.vm);
		ff8fx::copy<(sizeof(g_save_big))>(g_save_big, c.bigtab);
		ff8fx::copy<(sizeof(g_save_draw))>(g_save_draw, c.draw);
		apply_lev_a(c);
		VoidFn d18 = c.draw[18];
		ff8fx::copy<(sizeof(g_save_vm))>(c.vm, g_save_vm);
		ff8fx::copy<(sizeof(g_save_big))>(c.bigtab, g_save_big);
		ff8fx::copy<(sizeof(g_save_draw))>(c.draw, g_save_draw);
		c.draw[18] = d18;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_eq);
	}
}
}

	void register_mag233_earthquake()
	{
		using namespace gfc;
		earthquake::describe(earthquake::g_eq);
		init_clone(earthquake::g_eq, nullptr, 0);
		earthquake::apply_draw18(earthquake::g_eq);
		earthquake::g_eq.vm[0x06F] = earthquake::op_06F_OffscreenStageGround;
		earthquake::g_eq.vm[0x070] = earthquake::op_070_RideWaterColumn;
		register_port(0xA525B0, (void *)earthquake::SequenceTask, "MAG233 Earthquake SequenceTick", 233);
		FX_HELD(register_mag233_earthquake_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag233_earthquake_held.inc"
#endif
