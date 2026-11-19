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

// Effect 229: Homing Laser

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace homing_laser_q
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace homing_laser_q
{
	static void describe(Clone &c)
	{
		c.name = "229 Homing Laser (Quistis)";
		c.effect_id = 229;
		c.vm_table = us(0x186AF00);
		c.bone_table = us(0x186AB70);
		c.prim_table = us(0x186ABB8);
		c.prim_size = us(0x186ACA8);
		c.spr_table = us(0x186ACBC);
		c.ptype_table = us(0x186ACE8);
		c.pop_table = us(0x186ACFC);
		c.draw_table = us(0x186AD5C);
		c.attr_16A = us(0x186AEC6);
		c.attr_184 = us(0x186AEE0);
		c.attr_186 = us(0x186AEE2);
		c.attr_194 = us(0x186AEF0);
		c.attr_19C = us(0x186AEF8);
		c.desc = us(0x186AA40);
		c.ptr_block = us(0x186AB48);
		c.queue = us(0x2796B00);
		c.file00 = us(0x2796B14);
		c.file01 = us(0x2796B10);
		c.tick = us(0xA7B090);
		c.load_cb = us(0xA80F60);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static inline void BindEntityModelFromComData(uint8_t *entity, uint8_t *out, uint32_t data)
	{
		x::f<void (__cdecl *)(uint8_t *, uint8_t *, uint32_t)>(us(0x5022C0))(entity, out, data);
	}
	static inline void QueueChainTransformation(uint8_t *entity, int32_t id)
	{
		x::f<void (__cdecl *)(uint8_t *, int32_t)>(us(0x505C00))(entity, id);
	}
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(us(0xB66B80))(hdr, joint, scale, m);
	}

	static const uint32_t COM_ANIM = 0x188062C;

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	static void __cdecl op_0AB_BindEntityAnimSet()
	{
		uint8_t *ent = SlotEntity();
		uint8_t *blk = blob::ArenaAlloc(0x20);
		BindEntityModelFromComData(ent, blk, us(COM_ANIM));
		QueueChainTransformation(ent, (int32_t)S16(STREAM(), 2));
		STREAM() += 4;
	}

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
		c.vm[0x0AB] = op_0AB_BindEntityAnimSet;
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag229_homing_laser_quistis()
	{
		using namespace gfc;
		homing_laser_q::describe(homing_laser_q::g_c);
		init_clone(homing_laser_q::g_c, nullptr, 0);
		homing_laser_q::apply_slots(homing_laser_q::g_c);
		register_port(0xA7B090, (void *)homing_laser_q::SequenceTask, "MAG229 Homing Laser (Quistis) SequenceTick", 229);
	}
}

