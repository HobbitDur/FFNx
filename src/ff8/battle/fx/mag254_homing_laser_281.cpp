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
namespace laser254
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace laser254
{
	static void describe(Clone &c)
	{
		c.name = "254 Homing Laser (attack 281)";
		c.effect_id = 254;
		c.vm_table = us(0x185C298);
		c.bone_table = us(0x185BF08);
		c.prim_table = us(0x185BF50);
		c.prim_size = us(0x185C040);
		c.spr_table = us(0x185C054);
		c.ptype_table = us(0x185C080);
		c.pop_table = us(0x185C094);
		c.draw_table = us(0x185C0F4);
		c.attr_16A = us(0x185C25E);
		c.attr_184 = us(0x185C278);
		c.attr_186 = us(0x185C27A);
		c.attr_194 = us(0x185C288);
		c.attr_19C = us(0x185C290);
		c.desc = us(0x185BDF0);
		c.ptr_block = us(0x185BEE0);
		c.queue = us(0x27965B8);
		c.file00 = us(0x27965C8);
		c.file01 = us(0x27965CC);
		c.tick = us(0x98D820);
		c.load_cb = us(0x9936B0);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(us(0xB66B80))(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;
		x::BuildBoneMatricesFromPose(hdr);
		uint8_t *m = xl_JointMatrix_B66B80(hdr, S16(STREAM(), 2), 0x1000, ent + 0x40);
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		uint8_t *w = WS();
		x::TransformToCamera(CUR() + 0x94, w + 0xF0, w + 0xFC);
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 4;
	}

	static void apply_slots(Clone &c)
	{
		apply_shared_ribbon(c);
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag254_homing_laser_281()
	{
		using namespace gfc;
		laser254::describe(laser254::g_c);
		init_clone(laser254::g_c, nullptr, 0);
		laser254::apply_slots(laser254::g_c);
		register_port(0x98D820, (void *)laser254::SequenceTask, "MAG254 Homing Laser (attack 281) SequenceTick", 254);
	}
}

