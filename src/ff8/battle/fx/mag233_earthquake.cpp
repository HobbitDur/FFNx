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

// Effect 233: Earthquake (enemy attack, kernel attack 49, Armadodo), cinematic-engine module
// MAG_233 0xA51F10..0xA5C2B0, script data magic/mag232_b.00/.01 (loaded by MAG_233_UNKNOWN_FL
// 0xA51F10), streamed parts battle/MA8DEF_P.0..2.
//
// Earthquake runs the GF cinematic engine (gfc_engine.cpp), not the Quake spell's code (effect 38,
// mag038_quake.cpp, is an unrelated effect-library module): the module is one more compile of the
// engine library. Instruction-level comparison against the seven GF clones (module-relative
// addresses normalised, callee trees hashed):
//   - SetupSummon 0xA51F40, BindContext 0xA51FF0, ReleaseContext 0xA52080, AnimChannelsNeg
//     0xA5BCE0, AnimIntegrator 0xA528D0, AnimChannelsPos 0xA5C100, the debug pad control 0xA527D0
//     and the section pointer set-up 0xA52090 are byte clones of Ifrit's; SequenceTick 0xA525B0
//     differs only by its BuildMatricesAndDraw 0xA56E80 = the unlit variant (= Brothers 0xAF9ED0),
//     lit_dispatcher false;
//   - every table handler is the generic (Ifrit) one except
//       VM 0x006 LoadBattleFile 0xA57B70: same code, own load callback 0xA57CA0 (c.load_cb),
//       VM 0x06F 0xA5A6F0: OffscreenStageGround (a nop in Ifrit), ported below,
//       VM 0x070 0xA53BA0: RideWaterColumn (a stub in Ifrit), ported below,
//       draw 18 0xA52E90: WaterColumn, a byte clone of Leviathan's 0xB594B0 (apply_lev_a in
//         mag_leviathan.cpp; only that slot is taken).
// The effect's queue (0x2796A20) holds one task, the engine tick 0xA525B0.
// The ground ripple: VM 0x06F renders the stage ground group (stage slot 1, 0x1D989C0) off
// screen into an arena buffer once; draw 18 draws the rippling ring mesh; VM 0x070 puts a node on
// the ripple surface (its height at the node's distance from the ripple centre).

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag233_earthquake_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_lev_a(Clone &c);  // mag_leviathan.cpp

namespace earthquake
{
	static Clone g_eq = {};

	static void describe(Clone &c)
	{
		c.name = "233 Earthquake";
		c.effect_id = 233;
		c.vm_table = 0x1868750;     // MAG_233 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18683C0;   // MAG_233 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1868408;   // bone_table + 0x48
		c.prim_size = 0x18684F8;    // bone_table + 0x138
		c.spr_table = 0x186850C;    // bone_table + 0x14C
		c.ptype_table = 0x1868538;  // bone_table + 0x178
		c.pop_table = 0x186854C;    // bone_table + 0x18C
		c.draw_table = 0x18685AC;   // MAG_233 DrawHandlerTable
		c.attr_16A = 0x1868716;
		c.attr_184 = 0x1868730;
		c.attr_186 = 0x1868732;
		c.attr_194 = 0x1868740;
		c.attr_19C = 0x1868748;
		c.desc = 0x18682F8;         // draw_table - 0x2B4 (read by 0xA52090)
		c.ptr_block = 0x1868398;    // draw_table - 0x214 (read by 0xA51FF0)
		c.queue = 0x2796A20;
		c.file00 = 0x2796A34;
		c.file01 = 0x2796A30;
		c.tick = 0xA525B0;
		c.load_cb = 0xA57CA0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// 0xB65B00 (shared blob): off-screen render of the stage ground: args (block, pos, angles).
	// GTE H = 0x100 (0x56CD00), rotation from angles into ws+0xB0, ws+0xC4..0xCC = -pos.x, -pos.z,
	// pos.y, ws+0xE4 = block+8 (packet buffer); two DRAWENVs at block+0x50 (from the battle's
	// current one 0x1D969C8 + 0x1C * ((0x1D96A80 - 1) & 1), origin ws+0xD0/0xD2, 256 x 256) set by
	// 0x45C0F0; ws+0xE0 = block+0 (OT, 0x400 entries) cleared; poses and renders the stage ground
	// group (0x508C90 / 0x5095B0 / 0x5099D0 on 0x1D989D0 / 0x1D989C0) and executes the OT
	// (0x45CEE0): GPU work + stage model writes.
	// ------------------------------------------------------------------------------------
	static inline void xl_OffscreenStage_B65B00(void *blk, void *pos, void *angles)
	{
		// 30 fps layer: see mag233_earthquake_held.inc
		FX_HELD(if (held_predicting()) return;)
		x::f<int32_t (__cdecl *)(void *, void *, void *)>(0xB65B00)(blk, pos, angles);
	}

	// 0x56BEC0: (int)sqrt((double)v) through fild / fsqrt / __ftol (signed argument)
	static inline int32_t xl_Sqrt(int32_t v) { return x::f<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }

	// ------------------------------------------------------------------------------------
	// 0xA5A6F0 (VM 0x06F OffscreenStageGround): cursor+2 s16 / +4 s16 -> ws+0xD0 / ws+0xD2 (screen
	// origin of the off-screen area). Skipped (cursor += 6) while RenderCtx+0x30 != 0. Arena block
	// at the arena top: {+0 OT (0x1000 B, = top + 0xEC), +4 0, +8 packet buffer (= top + 0x10EC),
	// +0xC size 0x5BEC (also bone+0xC4, for VM 0x050 FreeArenaBlock), +0x14 arena top after the
	// allocation}; header 0xEC B (two DRAWENVs from +0x50). Then 0xB65B00 renders the stage ground
	// from the bone's position (+0x94) and angles (+0x8C). Cursor += 6.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_06F_OffscreenStageGround()
	{
		if (U16(RCTX(), 0x30) != 0)
		{
			STREAM() += 6;
			return;
		}
		U16(WS(), 0xD0) = U16(STREAM(), 2);
		U16(WS(), 0xD2) = U16(STREAM(), 4);
		uint8_t *blk = PTR(CTX(), 0x74);                                  // esi
		// 30 fps layer: see mag233_earthquake_held.inc
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

	// ------------------------------------------------------------------------------------
	// 0xA53BA0 (VM 0x070 RideWaterColumn): cursor+2 s16 = water-column node ref (a node drawn by
	// draw 18; resolved with GetBone while the current node is the caller), its parent ref = the
	// node's +0x9C (u16, resolved with the water-column node as current node). Distance in x / z of
	// the current node from (column + parent) position: d = sqrt(dz^2 + dx^2) (32-bit products).
	// Height h: column state block (column +0xBC, draw 18's) absent -> the never-written local
	// [ebp-4] (= the caller's ecx at entry: the Neg / Pos dispatcher's RuntimeSlot pointer); d >
	// radius[31] (+0x9E) -> 0; else the first ring i = 1..31 with d < radius[i] (+0x60 + 2i) ->
	// wave height[i - 1] (+0x20 + 2(i - 1)); none -> [ebp-4] as well. Current node +0x60 =
	// (h + parent+0x96 + column+0x96) << 16 (the accumulated y), then the node's bone handler
	// (BoneHandlerTable[+0x18]). Cursor += 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_070_RideWaterColumn()
	{
		uint8_t *saved = CUR();                                           // edi
		uint8_t *col = blob::GetBone(S16(STREAM(), 2));                   // esi
		CUR() = col;
		uint8_t *par = blob::GetBone((int32_t)U16(col, 0x9C));            // ebx
		CUR() = saved;
		int32_t dx = S16(saved, 0x94) - S16(par, 0x94);
		dx = dx - S16(col, 0x94);
		int32_t dz = S16(saved, 0x98) - S16(par, 0x98);
		dz = dz - S16(col, 0x98);
		int32_t d = xl_Sqrt(add32(mul32(dz, dz), mul32(dx, dx)));
		uint8_t *blk = PTR(col, 0xBC);                                    // ecx
		int32_t h = (int32_t)(uint32_t)RT();                              // [ebp-4]: the dispatcher's ecx
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

	// draw 18 (WaterColumn) is the only Leviathan handler Earthquake carries: take that one slot
	// from apply_lev_a and keep every other slot as init_clone built it
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_draw18(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_lev_a(c);
		VoidFn d18 = c.draw[18];
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
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
		earthquake::g_eq.vm[0x06F] = earthquake::op_06F_OffscreenStageGround;   // 0xA5A6F0
		earthquake::g_eq.vm[0x070] = earthquake::op_070_RideWaterColumn;        // 0xA53BA0
		register_port(0xA525B0, (void *)earthquake::SequenceTask, "MAG233 Earthquake SequenceTick", 233);
		// 30 fps layer: see mag233_earthquake_held.inc
		FX_HELD(register_mag233_earthquake_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag233_earthquake_held.inc"
#endif
