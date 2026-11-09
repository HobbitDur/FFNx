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

// Effect 257: Mega Flare (Bahamut) (enemy attack 234 of kernel.bin, Bahamut c0m067), cinematic-engine module MAG_257 0x96F260..0x9793D0,
// script data magic/mag256_b.00/.01 (loaded by MAG_257_UNKNOWN_FL 0x96F260; setup MAG_257_UNKNOWN 0x96F290 =
// effect table index 256).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x185A3C0; its descriptor block 0x185A064 sits 0x35C below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcA\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0x96F2F0, BindContext 0x96F340, the section pointers 0x96F3E0, InitSequenceState
//     0x9789E0, SequenceTick 0x96F900 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0x9793B0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (its table
//     equals Massive Anchor 263's but for VM 0x0AB, prim 9 and draw 70, stubs here, and prim 58, live
//     here), except
//       VM 0x006 LoadBattleFile 0x974D50: same code, own load callback 0x974E80 (c.load_cb),
//       prim 58 0x9725F0 TintedFT4Abr = Breath of Death 232's 0xA5F610 / Disease Breath 231's 0xA69D60
//         (static there: same code below; its light set-up call is the engine's 0xB29450, h_B29450).
// The effect's queue (0x2796510) holds one task, the engine tick 0x96F900.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace megaflare257
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag257_mega_flare_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace megaflare257
{
	static void describe(Clone &c)
	{
		c.name = "257 Mega Flare (Bahamut)";
		c.effect_id = 257;
		c.vm_table = 0x185A564;      // MAG_257 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x185A1D4;    // MAG_257 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x185A21C;    // bone_table + 0x48
		c.prim_size = 0x185A30C;     // bone_table + 0x138
		c.spr_table = 0x185A320;     // bone_table + 0x14C
		c.ptype_table = 0x185A34C;   // bone_table + 0x178
		c.pop_table = 0x185A360;     // bone_table + 0x18C
		c.draw_table = 0x185A3C0;    // MAG_257 DrawHandlerTable
		c.attr_16A = 0x185A52A;
		c.attr_184 = 0x185A544;
		c.attr_186 = 0x185A546;
		c.attr_194 = 0x185A554;
		c.attr_19C = 0x185A55C;
		c.desc = 0x185A064;          // draw_table - 0x35C (read by 0x96F3E0)
		c.ptr_block = 0x185A1AC;     // draw_table - 0x214 (read by 0x96F340)
		c.queue = 0x2796510;
		c.file00 = 0x279650C;
		c.file01 = 0x2796508;
		c.tick = 0x96F900;
		c.load_cb = 0x974E80;        // passed by VM 0x006 0x974D50
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// GTE data registers (0x1CA8A10 + 4 * reg)
	inline uint32_t &GteData(int reg) { return MEM<uint32_t>(0x1CA8A10 + 4 * reg); }

	// fild qword [u16 DEPTHARR[off >> 3]]; fmul dword [0x1877DA8]; call __ftol (low dword kept;
	// u16 x float = at most 40 significant bits, exact in a double)
	static inline int32_t DepthKey(uint32_t off)
	{
		uint32_t v = DEPTHARR()[(int32_t)off / 8] & 0xFFFF;
		return (int32_t)(int64_t)((double)v * (double)FLT_1877DA8());
	}

	// ------------------------------------------------------------------------------------
	// 0x9725F0 (prim 58, = Breath of Death 232's 0xA5F610 / Disease Breath 231's 0xA69D60):
	// semi-transparent tinted FT4. Record 0x18 as the generic FT4 (+0 u32
	// colour, +4..+0xA u16 UV 0..3, +0xC..+0x12 u16 vertex offsets, +0x14 CLUT, +0x16 tpage).
	// Packet 0x2C (len 0xA): +4 RGB (lit, code 0x2C | ws+0x8C), +8/+0x10/+0x18/+0x20 SXY,
	// +0xC/+0x14/+0x1C/+0x24 UV + ws+0x98, +0xE CLUT + ws+0x54, +0x16 tpage | ws+0x50,
	// +0x28 draw mode 0xE1000220.
	// ------------------------------------------------------------------------------------
	static void __cdecl pr_58_TintedFT4Abr()
	{
		h_B29450((int32_t)U32(WS(), 0x84));                                 // 0x9728D0 (= the engine's 0xB29450)
		uint8_t *ws = WS();                                                 // [ebp-0x10]
		uint8_t *pkt = PTR(ws, 0x60);                                       // esi
		uint8_t *rec = PTR(ws, 0x6C);                                       // edi
		int32_t count = S32(ws, 0x70);                                      // [ebp-0x1c]
		uint32_t ot = U32(ws, 0x5C);                                        // [ebp-0x24]
		U32(ws, 0xF0) = 0;
		uint32_t nocull = U32(ws, 0x90) & 0x10;                             // [ebp-0x20]
		do
		{
			uint32_t o0 = U16(rec, 0xC);                                    // ebx
			uint32_t o1 = U16(rec, 0xE);                                    // [ebp-0x14]
			uint32_t o2 = U16(rec, 0x10);                                   // [ebp-4]
			uint32_t o3 = U16(rec, 0x12);                                   // [ebp-8]
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *vb = PTR(ws, 0x7C);
			uint8_t *p0 = vb + o0;
			uint8_t *p1 = vb + o1;
			uint8_t *p2 = vb + o2;
			uint8_t *p3 = vb + o3;
			uint32_t s0 = U32(p0, 0);
			uint32_t s1 = U32(p1, 0);
			uint32_t s2 = U32(p2, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			U32(pkt, 0x20) = s3;
			U32(pkt, 0x10) = s1;
			if (nocull == 0)
			{
				GteData(12) = s0;
				GteData(13) = s1;
				GteData(14) = s2;
				x::GteNCLIP();
				if ((int32_t)GteData(24) < 0) goto next;
			}
			{
				uint32_t a0 = U32(p0, 4), a2 = U32(p2, 4), a1 = U32(p1, 4), a3 = U32(p3, 4);
				if ((a3 | a2 | a1 | a0) & 0x460000) goto next;
				U8(pkt, 3) = 0xA;
				U32(pkt, 0x28) = 0xE1000220;
				uint32_t slot = (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC) + ot;
				U16(pkt, 0x16) = (uint16_t)(U32(ws, 0x50) | U16(rec, 0x16));
				U16(pkt, 0xE) = (uint16_t)(U16(rec, 0x14) + (uint16_t)U32(ws, 0x54));
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv0 = U16(rec, 4) + uvofs;
				uint32_t uv1 = U16(rec, 6) + uvofs;
				uint32_t uv2 = U16(rec, 8) + uvofs;
				uint32_t uv3 = U16(rec, 0xA) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x24) = (uint16_t)uv3;
				U16(pkt, 0x14) = (uint16_t)uv1;
				U16(pkt, 0x1C) = (uint16_t)uv2;
				GteData(6) = U32(ws, 0x8C) | U32(rec, 0) | 0x2C000000;
				x::Gte_4601B0();
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				U32(pkt, 4) = GteData(22);
				pkt += 0x2C;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// the module's own handler (no shared handler is live in this module)
	static void apply_slots(Clone &c)
	{
		c.prim[58] = pr_58_TintedFT4Abr;                // 0x9725F0
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag257_mega_flare()
	{
		using namespace gfc;
		megaflare257::describe(megaflare257::g_c);
		init_clone(megaflare257::g_c, nullptr, 0);
		megaflare257::apply_slots(megaflare257::g_c);
		register_port(0x96F900, (void *)megaflare257::SequenceTask, "MAG257 Mega Flare (Bahamut) SequenceTick", 257);
		// 30 fps layer: see mag257_mega_flare_held.inc
		FX_HELD(register_mag257_mega_flare_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag257_mega_flare_held.inc"
#endif
