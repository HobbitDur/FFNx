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

// Effect 228: Ice Strike

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace icestrike228
{
	static Clone g_c = {};
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace icestrike228
{
	static void describe(Clone &c)
	{
		c.name = "228 Ice Strike";
		c.effect_id = 228;
		c.vm_table = us(0x186B8FC);
		c.bone_table = us(0x186B56C);
		c.prim_table = us(0x186B5B4);
		c.prim_size = us(0x186B6A4);
		c.spr_table = us(0x186B6B8);
		c.ptype_table = us(0x186B6E4);
		c.pop_table = us(0x186B6F8);
		c.draw_table = us(0x186B758);
		c.attr_16A = us(0x186B8C2);
		c.attr_184 = us(0x186B8DC);
		c.attr_186 = us(0x186B8DE);
		c.attr_194 = us(0x186B8EC);
		c.attr_19C = us(0x186B8F4);
		c.desc = us(0x186B438);
		c.ptr_block = us(0x186B544);
		c.queue = us(0x2796B38);
		c.file00 = us(0x2796B4C);
		c.file01 = us(0x2796B48);
		c.tick = us(0xA85C30);
		c.load_cb = us(0xA8B230);
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t COMDATA = 0x18821A0;
	static const uint32_t EFFECT_LIGHT = 0x1D99AB0;

	template<typename F> static inline F fx(uint32_t a) { return (F)a; }
	static inline void xl_BindEntityModelFromComData(void *ent, void *out, uint32_t data) { fx<void (__cdecl *)(void *, void *, uint32_t)>(us(0x5022C0))(ent, out, data); }
	static inline void xl_QueueChainTransformation(void *ent, int32_t id) { fx<void (__cdecl *)(void *, int32_t)>(us(0x505C00))(ent, id); }
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(us(0xB66B80))(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }
	static inline uint32_t &CamWord32(int i) { return MEM<uint32_t>(us(0xB8B7F0) + 4 * i); }
	static inline uint16_t &Word1D977A0() { return MEM<uint16_t>(us(0x1D977A0)); }
	static inline uint16_t &Word1D977A2() { return MEM<uint16_t>(us(0x1D977A2)); }
	static inline uint16_t &Word1D9771C() { return MEM<uint16_t>(us(0x1D9771C)); }

	static void __cdecl op_0AB_BindLimitAnimSet()
	{
		uint8_t *ent = SlotEntity();
		uint8_t *blk = blob::ArenaAlloc(0x20);
		xl_BindEntityModelFromComData(ent, blk, us(COMDATA));
		xl_QueueChainTransformation(ent, S16(STREAM(), 2));
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

	static void __cdecl op_039_CameraAndSound()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		switch (sub - 1)
		{
		case 0:
		{
			uint8_t *f = RESFILE()[0];
			x::PlayCameraAnimation(f + U32(f, 0x20));
			break;
		}
		case 1:
		{
			uint8_t *f = RESFILE()[0];
			uint8_t *s = STREAM();
			uint32_t slot = U8(s, 2);
			uint8_t *snd = f + U32(f, 0x24);
			int32_t n = (int32_t)U8(s, 3) + 1;
			uint32_t v = x::ClaimVoiceSlot(snd, n, 0x80);
			U32(SCENE(), slot * 4 + 0x168) = v;
			break;
		}
		case 2:
			for (int32_t off = 0x168; off < 0x174; off += 4)
			{
				uint32_t v = U32(SCENE(), off);
				if (v) x::Snd_4A2940((int32_t)v);
			}
			break;
		case 3:
		{
			uint32_t e0 = CamWord32(0), e1 = CamWord32(1), a0 = CamWord32(2);
			CamWord32(4) = e0;
			uint32_t a1 = CamWord32(3);
			CamWord32(5) = e1;
			uint16_t h = (uint16_t)PROJH();
			CamWord32(7) = a1;
			CamWord32(6) = a0;
			uint16_t w = Word1D977A2();
			Word1D977A0() = h;
			Word1D9771C() = w;
			break;
		}
		case 4:
		{
			blob::CopyMatrix((void *)us(EFFECT_LIGHT), BBMAT());
			uint8_t *b = CUR();
			MEM<int32_t>(us(EFFECT_LIGHT) + 0x14) = S16(b, 0x94);
			MEM<int32_t>(us(EFFECT_LIGHT) + 0x18) = S16(b, 0x96);
			MEM<int32_t>(us(EFFECT_LIGHT) + 0x1C) = S16(b, 0x98);
			break;
		}
		case 5:
			x::Camera_504270(1);
			break;
		case 6:
			x::Camera_504270(0);
			break;
		case 7:
			x::CameraArmReturn();
			break;
		default:
			if (!(U8(SEQ(), 9) & 0x20))
			{
				uint8_t *b = CUR();
				CamWord32(0) = U32(b, 0x94);
				CAMEYE()[2] = S16(b, 0x98);
				PROJH() = S16(b, 0x8C);
				Word1D977A2() = U16(b, 0x8E);
				uint8_t *t = blob::GetBone(S16(STREAM(), 2));
				CamWord32(2) = U32(t, 0x94);
				CAMAT()[2] = S16(t, 0x98);
			}
			break;
		}
		STREAM() += 4;
	}

	static void apply_slots(Clone &c)
	{
		c.vm[0x039] = op_039_CameraAndSound;
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag228_ice_strike()
	{
		using namespace gfc;
		icestrike228::describe(icestrike228::g_c);
		init_clone(icestrike228::g_c, nullptr, 0);
		icestrike228::apply_slots(icestrike228::g_c);
		register_port(0xA85C30, (void *)icestrike228::SequenceTask, "MAG228 Ice Strike SequenceTick", 228);
	}
}

