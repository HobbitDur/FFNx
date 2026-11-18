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

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag263_massive_anchor_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	static void describe(Clone &c)
	{
		c.name = "263 Massive Anchor";
		c.effect_id = 263;
		c.vm_table = us(0x1857C70);
		c.bone_table = us(0x18578E0);
		c.prim_table = us(0x1857928);
		c.prim_size = us(0x1857A18);
		c.spr_table = us(0x1857A2C);
		c.ptype_table = us(0x1857A58);
		c.pop_table = us(0x1857A6C);
		c.draw_table = us(0x1857ACC);
		c.attr_16A = us(0x1857C36);
		c.attr_184 = us(0x1857C50);
		c.attr_186 = us(0x1857C52);
		c.attr_194 = us(0x1857C60);
		c.attr_19C = us(0x1857C68);
		c.desc = us(0x185777C);
		c.ptr_block = us(0x18578B8);
		c.queue = us(0x2795DA8);
		c.file00 = us(0x2795DA4);
		c.file01 = us(0x2795DA0);
		c.tick = us(0x945E80);
		c.load_cb = us(0x94B470);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t COMDATA = 0x1877DAC;

	template<typename F> static inline F fx(uint32_t a) { return (F)a; }
	static inline void xl_BindEntityModelFromComData(void *ent, void *out, uint32_t data) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, void *, uint32_t)>(us(0x5022C0))(ent, out, data); }
	static inline void xl_QueueChainTransformation(void *ent, int32_t id) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, int32_t)>(us(0x505C00))(ent, id); }

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	static void __cdecl op_0AB_BindLimitAnimSet()
	{
		uint8_t *ent = SlotEntity();
		uint8_t *blk = blob::ArenaAlloc(0x20);
		xl_BindEntityModelFromComData(ent, blk, us(COMDATA));
		xl_QueueChainTransformation(ent, S16(STREAM(), 2));
		STREAM() += 4;
	}

	static void __cdecl dh_70_PlaceWeaponAtBone()
	{
		FX_HELD(if (held_active()) return;)
		uint8_t *w = PTR(SlotEntity(), 0x78);
		S16(w, 0x1C) = S16(CUR(), 0x94);
		S16(w, 0x1E) = S16(CUR(), 0x96);
		S16(w, 0x20) = S16(CUR(), 0x98);
	}

	static void apply_slots(Clone &c)
	{
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;
		c.draw[70] = dh_70_PlaceWeaponAtBone;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag263_massive_anchor()
	{
		using namespace gfc;
		anchor263::describe(anchor263::g_c);
		init_clone(anchor263::g_c, nullptr, 0);
		anchor263::apply_slots(anchor263::g_c);
		register_port(0x945E80, (void *)anchor263::SequenceTask, "MAG263 Massive Anchor SequenceTick", 263);
		FX_HELD(register_mag263_massive_anchor_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag263_massive_anchor_held.inc"
#endif
