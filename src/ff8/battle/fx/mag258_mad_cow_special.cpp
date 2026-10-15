/****************************************************************************/
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

// Effect 258: Mad Cow Special (enemy attack 213 of kernel.bin, the Brothers' combo move: Sacred
// c0m063 and Minotaur c0m062, Tomb of the Unknown King; no other kernel.bin entry uses effect 258),
// cinematic-engine module MAG_258 0x9649D0..0x96F260, script data magic/mag257_b.00/.01 (loaded by
// MAG_258_UNKNOWN_FL 0x9649D0; setup MAG_258_UNKNOWN 0x964A00 = effect table index 257).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with Brothers' code
// (mag_brothers.cpp): instruction-level comparison against the ported clones (module-relative
// addresses normalised, table regions symbolic, jump tables and callee trees hashed):
//   - SetupSummon 0x964A60, InitSequenceState 0x96E870 and SequenceTick 0x965070 (the unlit
//     BuildMatricesAndDraw) are byte clones of Brothers' 0xAF4580 / 0xAFF280 / 0xAF4B90
//     (0x96F240 differs only by the initial frame limit, 0x233 instead of 0x27D); lit_dispatcher
//     false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (Brothers'
//     own handlers draw 3 / 38 / 48 and VM 0x09D / 0x0C5 / 0x0D1 / 0x0D5 / 0x0DA are stubs here,
//     as are VM 0x03F / 0x041 / 0x042 / 0x05C / 0x089 / 0x0CF), except
//       VM 0x006 LoadBattleFile 0x96AAD0: same code, own load callback 0x96AC00 (c.load_cb),
//       draw 26 0x965B80 = Cerberus 0xB0E930 / Eden 0xAEA0C0 and VM 0x096 0x96D770 = Cerberus
//         0xB16F60 / Eden 0xAF2760 (apply_shared_misc),
//       draw 43 0x9660C0 = Bahamut 0xB1B9E0 Beam (apply_bahamut, only that slot is taken),
//       draw 53 0x9681E0 = Eden 0xAED040 PixelParticleEmitter (apply_eden_c, only that slot),
//       VM 0x0AD 0x96E450 = Final Form Death 0xB3DA20 PosFromPosedEntityJoint (static in
//         mag077_final_form_death.cpp: same code below).
//     VM 0x022 0x96A5F0 / VM 0x090 0x96D660 are Ifrit's forms (not Brothers'): generic.
// The effect's queue (0x27964D0) holds one task, the engine tick 0x965070.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace mad_cow_special
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag258_mad_cow_special_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_bahamut(Clone &c);  // mag_bahamut.cpp
	void apply_eden_c(Clone &c);   // mag_eden.cpp

namespace mad_cow_special
{
	static void describe(Clone &c)
	{
		c.name = "258 Mad Cow Special";
		c.effect_id = 258;
		c.vm_table = 0x1859B2C;     // MAG_258 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185979C;   // MAG_258 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18597E4;   // bone_table + 0x48
		c.prim_size = 0x18598D4;    // bone_table + 0x138
		c.spr_table = 0x18598E8;    // bone_table + 0x14C
		c.ptype_table = 0x1859914;  // bone_table + 0x178
		c.pop_table = 0x1859928;    // bone_table + 0x18C
		c.draw_table = 0x1859988;   // MAG_258 DrawHandlerTable
		c.attr_16A = 0x1859AF2;
		c.attr_184 = 0x1859B0C;
		c.attr_186 = 0x1859B0E;
		c.attr_194 = 0x1859B1C;
		c.attr_19C = 0x1859B24;
		c.desc = 0x18596D4;         // draw_table - 0x2B4
		c.ptr_block = 0x1859774;    // draw_table - 0x214 (read by 0x964AB0)
		c.queue = 0x27964D0;
		c.file00 = 0x27964F4;
		c.file01 = 0x27964F0;
		c.tick = 0x965070;
		c.load_cb = 0x96AC00;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x96E450 (VM 0x0AD PosFromPosedEntityJoint, = Final Form Death 0xB3DA20): the slot
	// entity's bone matrices rebuilt from its pose (header entity+0x60, or *(entity+0x78) when op
	// bit 15 is set), joint w0 of that model (blob 0xB66B80, composed with the entity matrix
	// +0x40) applied to outPos -> accumPos = the camera-space point << 16. Then the bone handler.
	// Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		uint8_t *hdr = (U8(RT(), 0x4B) & 0x80) ? PTR(ent, 0x78) : ent + 0x60; // esi
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

	// ------------------------------------------------------------------------------------
	// handler sets: the shared / Bahamut / Eden handlers this module carries (only those slots
	// are taken), then this module's own handler
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void save_tables(const Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
	}
	static void restore_tables(Clone &c)
	{
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
	}

	static void apply_slots(Clone &c)
	{
		save_tables(c);
		apply_bahamut(c);
		const VoidFn d43 = c.draw[43];        // 0x9660C0 = Bahamut 0xB1B9E0
		restore_tables(c);
		apply_eden_c(c);
		const VoidFn d53 = c.draw[53];        // 0x9681E0 = Eden 0xAED040
		restore_tables(c);
		apply_shared_misc(c);                 // draw 26 0x965B80, VM 0x096 0x96D770 (the live slots)
		c.draw[43] = d43;
		c.draw[53] = d53;
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;   // 0x96E450
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag258_mad_cow_special()
	{
		using namespace gfc;
		mad_cow_special::describe(mad_cow_special::g_c);
		init_clone(mad_cow_special::g_c, nullptr, 0);
		mad_cow_special::apply_slots(mad_cow_special::g_c);
		register_port(0x965070, (void *)mad_cow_special::SequenceTask, "MAG258 Mad Cow Special SequenceTick", 258);
		// 30 fps layer: see mag258_mad_cow_special_held.inc
		FX_HELD(register_mag258_mad_cow_special_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag258_mad_cow_special_held.inc"
#endif
