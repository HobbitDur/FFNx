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

// Effect 8: Doom (enemy attack 263 of kernel.bin, used by Sphinxara c0m012 and c0m122; MAG_008_*):
// a copy of the actor/heal effect library (see act_engine.h) built like Tonberry's Chef's Knife
// (mag090_tonberry.cpp: master + director stepping a phase script, a creature actor, prim-model
// particle tasks with the eight primitive-list renderers) with Death's reaper
// (mag014_death.cpp: the dark renderer, the stage wobble) and a few pieces of its own.
//
// Setup MAG_008_DOOM_Init 0x8C4980 (runs once, not ported; file loader MAG_008_DOOM_FL 0x8C4960 =
// the effect's data file -> 0x2758500, packet arenas at file + 0 / + 0x8000 (module cursor
// 0x2758504 by tick parity), file arena cursor 0x275F9D8 = file + 0x10000) plays the camera
// animation 0x16358D8, uploads the file's TIM and creates the root queue 0x2758958 with the master
// and nine task pools: 0x275E498 (4 x 0x58 emitters), 0x2758860 (3 x 0x48 director), 0x275F918
// (2 x 0x30 load script), 0x275F3D8 (1 x 0x140 reaper), 0x275E1F8 (1 x 0x2A4 actor system),
// 0x2758968 (0x32 x 0xB0 dust / stage wobble tasks), 0x275ABE0 (8 x 0x58 smoke), 0x275DF38
// (3 x 0x458 prim-model tasks) and 0x275E488 (10 x 0x40 screen tints, engine task 0x8DDC30).
//   Master (0x8C4B30 = the engine's a_739F40 with this module's globals) - camera copy 0x2793E58
//     (the module scratch stack 0x275F92C grows down from it), packet arena by tick parity, bone
//     follow, 11-state table: 0x8C4E10 sets up the phase data / asset table and spawns the load
//     script 0x8C4F60 and the director, 0x8C4CE0 carves the actor pools after the arena (0x438 +
//     0x6900 bytes: 10 emitter records of 0x6C, 40 actors of 0x2A0), 0x8C4D60 one emitter per
//     action, then the engine's end states.
//   Director (0x8C5350, Tonberry 0x762C60): phase data [0x1634F20] (0x8C52C0: the target's
//     position, the facing angle towards it +0x4A, the effect light matrix 0x1D99AB0), 15 states
//     (the reaper's TIM uploads 0x8C5460, script marks a_73B640 / a_73B7E0, sound 0x1634F24); each
//     new phase spawns its tasks (0x8C5710): phase 2 = the reaper (0x8CC3A0, model container
//     0x1636CE8 animation 3), its aura actor system (0x8C88D0: actor data 0x1818B78), the dust
//     (0x8CD4B0), smoke (0x8CDED0) and stage wobble in (0x8CC1E0) tasks; phase 4 = the scythe prim
//     model (0x8C58B0), a screen tint (0x8DDC30, colour script 0x163ACB4) and the stage wobble out
//     (0x8CC2D0).
//   Prim-model tasks (0x275DF38, node 0x458): the prim-model player (0x8C5B10 callback, the
//     module renderer 0x8C5DD0 = Tonberry's eight primitive-list renderers). The scythe (0x8C58B0:
//     0x8C5910 / 0x8C7FF0) stands in front of the caster and spawns the doom clock (0x8C8050:
//     0x8C80C0 / 0x8C8400 / 0x8C8870 / 0x8C8890) at its model step 0x13: the clock builds two rings
//     of 17 x 4 points around the target (0x8C8120: block [0x1636C04], 0x275D010 / 0x275CDF0),
//     moves its hand along the step table 0x1636C34 and draws the ring strips step by step
//     (0x8C84A0 -> 0x8C8500: Gouraud quads, row colours 0x275F3F8, table 0x1636C40); at the end of
//     the hand table the hand jumps back and the strike prim model (0x8C8790) starts at the target.
//   Reaper (0x8CC3A0, node 0x140): {0x8CCF80 appear (dark render context 0x2758510, 0x8CD020),
//     0x8CD090 rise, 0x8CD0E0 turn, 0x8CD120 / 0x8CD1A0 script waits, 0x8CD1D0 / 0x8CD200 /
//     0x8CD250 animations, 0x8CD290, 0x8CD2B0 fade out}; drawn each tick (0x8CC450) as the engine's
//     creature (a_746C10) or, while it rises (+0x26 bit 3), by the dark renderer (0x8CC6B0 =
//     Death's 0x8B7290, 0x8CC7B0): the reaper materialises from the ground up - once the rising
//     height (context +0x54 = 0x2758564) passes a face, the face's step byte (context +0x5D..)
//     counts to 0x13: it appears, spirals into place (the spiral angle 0x2758568 turns every tick)
//     and brightens from black, semi-transparent until it lands.
//   Dust (0x8CD540) / smoke (0x8CDF60): particle tasks at the reaper's feet (0x8CD670 = Siren's
//     0x7433C0 with velocity / 16 decay: 4 sprites each; 0x8CDFB0: prim-model puffs 0x16350F0
//     drawn by 0x8CE040).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2758500..0x275FB80 (file pointer 0x2758500, packet cursor 0x2758504, pools,
// queues, arenas, reaper render context 0x2758510, particle pool 0x275F3EC / cursor 0x275ABD8,
// actor pool 0x275F980 / cursor 0x275F508, actor state 0x275F9DC, scratch stack pointer
// 0x275F92C, asset table 0x275FA10), the module scratch stack below the camera copy 0x2793E58;
// the pointers 0x1634F20 (phase data) / 0x1636C04 (clock block) lead into module memory.
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (dm_XXXXXX), most of them
// the text of the matching Tonberry / Death / Magma Breath / Siren / Boko port with this module's
// addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace doom
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_008 = { "doom", 8, 0x8C4960, 0x8CE3B0,
		{ 0x0, 0x0, 0x1634F20, 0x0, 0x0, 0x2758884, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2758500, 0x2758504, 0x2758860, 0x2758884, 0x2758888, 0x275ABE0, 0x0, 0x275ABDC, 0x275DF38, 0x2758968, 0x275DF48, 0x275DF4C, 0x275E1F8, 0x275E488, 0x275E498, 0x275E828, 0x275E82C, 0x275E830, 0x275F23C, 0x275F3D8, 0x275F3E8, 0x0, 0x275F5C8, 0x275F910, 0x275F918, 0x275F92C, 0x275F9D4, 0x275F9DC },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C8960, 0x8C9440, 0x8C94D0, 0x8C9530, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C9690, 0x8C97D0, 0x8C9810, 0x8CAAA0, 0x8CAAF0, 0x8CAB20, 0x8CABA0, 0x8CB290, 0x8CB2E0, 0x8CB380, 0x8CB440, 0x8CBC90, 0x8CC090, 0x8CC120, 0x0, 0x8C4CC0, 0x8C4CD0, 0x8C4CE0, 0x8C4D60, 0x8C4D90, 0x8C4E10, 0x0, 0x8C5160, 0x8C5190, 0x8C51C0, 0x8C51D0, 0x8C51E0, 0x8C51F0, 0x8C5200, 0x0, 0x0, 0x8C5550, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C54B0, 0x0, 0x8C5510, 0x8C5B10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C8B10, 0x8C8BF0, 0x8C8D90, 0x8C8E60, 0x8C8F80, 0x8C91E0, 0x8C9750, 0x8C98A0, 0x8C9F00, 0x8C9F50, 0x8C9FE0, 0x8CA0B0, 0x8CA840, 0x8CA950, 0x8CA9D0, 0x8CAA00, 0x8CB220, 0x8CB250, 0x8CB330, 0x8CB450, 0x8CB5A0, 0x0, 0x8CBC60, 0x8CBE40, 0x8CBF80, 0x8CD9D0, 0x8C9440, 0x8CE250, 0x8CE290, 0x8CE2B0, 0x8CE2D0, 0x8CE2E0, 0x8CE320, 0x8CE330, 0x8CE340, 0x8CE370, 0x8CE380, 0x8CE3A0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2758504;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x275F9DC;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x275F92C;      // module scratch stack pointer
	static const uint32_t PHASE_DATA = 0x1634F20;      // -> the director's phase data
	static const uint32_t CLOCK_BLOCK = 0x1636C04;     // -> the doom clock's placement block
	static const uint32_t REAPER_CTX = 0x2758510;      // render context of the dark reaper draw
	static const uint32_t ORIG_Master = 0x8C4B30;
	static const uint32_t ORIG_Reaper = 0x8CC3A0;
	static const uint32_t ORIG_Prim = 0x8C58B0;        // scythe prim-model task (engine task)
	static const uint32_t DUST_FLIPBOOK = 0x1634F28;

	// engine functions not in act::x
	namespace dx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2E0)(a1); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3_2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E320)(a1); }
		// GTE_LoadV012 of three consecutive 8-byte vectors at a1
		static inline uint32_t GTE_LoadV012_Packed(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E020)(a1); }
		// SXY0..2 -> the three vertex slots of a POLY_G4 at a1 (+8 / +0x10 / +0x18)
		static inline uint32_t GTE_StoreSXY012_PolyG4(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2C0)(a1); }
		static inline uint32_t Effect_BuildMatrixFromDirAndUp(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DDA50)(a1, a2, a3); }
	}
	using namespace dx;
	namespace gx = dx;

	// helpers of the library code (as in act_engine.cpp / mag090_tonberry.cpp / mag097_boko.cpp /
	// mag048_magma_breath.cpp)
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
	// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
	static inline void next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	// the names of the ports the adapted texts come from
	static inline uint32_t t1_data() { return MEM<uint32_t>(PHASE_DATA); }
	static inline void t1_next_state(uint32_t node) { next_state(node); }
	static inline void t4_next_state(uint32_t node) { next_state(node); }
	static inline void b103_next_state(uint32_t node) { next_state(node); }
	// screen-space clip tests of the flat-triangle list (x 0..0xA00, y 0..0x6C0)
	static inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }

	// the primitive-list renderers' helpers (mag090_tonberry.cpp parts t2 / t3)
	// GP0 0xE2 texture-window word from a RECT {x,y,w,h} (u16 each) at ctx+b
	static uint32_t t2_twin(uint32_t ctx, int32_t b)
	{
		uint32_t ecx = (uint32_t)(U8(ctx, b + 2) & 0xF8) | 0xFFFE2000u;  // y
		uint32_t edx = (uint32_t)(U8(ctx, b) & 0xF8);                     // x
		ecx <<= 5;
		ecx |= edx;
		edx = U16(ctx, b + 6);                                            // h
		ecx <<= 5;
		edx = ~(edx - 1) & 0xF8;
		ecx |= edx;
		edx = U16(ctx, b + 4);                                            // w
		ecx <<= 2;
		edx = (uint32_t)((int32_t)~(edx - 1) >> 3) & 0x1F;
		ecx |= edx;
		return ecx;
	}
	// screen x outside [0, 0xA00] / y outside [0, 0x6C0] (signed 16-bit SXY halves)
	static inline bool t2_outx(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0xA00; }
	static inline bool t2_outy(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0x6C0; }
	// depth cue of the packet colour word at pCode towards the far colour ctx+0x0C
	static inline void t2_depth_cue(uint32_t ctx, uint32_t pCode)
	{
		x::set_unk_1CA8A28(pCode);
		x::set_dword_1CA8A30(U32(ctx, 0xC));
		x::sub_45F270();
		x::set_param_with_dword_1CA8A68(pCode);
	}
	// OTZ += bias (clamped to 0 when negative); returns the OT entry a2 + (OTZ >> a3) * 4
	static inline uint32_t t2_ot(uint32_t ctx, uint32_t a2, uint32_t a3)
	{
		const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
		S32(ctx, 0x38) = z;
		if (z < 0)
			S32(ctx, 0x38) = 0;
		return a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
	}
	// screen bounds test of a projected coordinate (x: 0..0xA00, y: 0..0x6C0, signed 16-bit)
	static inline bool t3_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t3_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	// E2 texture-window command word from a {u16 x, u16 y, u16 w, u16 h} block at p
	static inline uint32_t t3_texwin(uint32_t p)
	{
		uint32_t t = ((uint32_t)U8(p, 2) & 0xF8) | 0xFFFE2000u;
		t = (t << 5) | ((uint32_t)U8(p, 0) & 0xF8);
		t = (t << 5) | (~((uint32_t)U16(p, 6) - 1) & 0xF8);
		t = (t << 2) | ((~((uint32_t)U16(p, 4) - 1) >> 3) & 0x1F);
		return t;
	}
	// adds d to the n (3 or 4) texture bytes pkt+off[i]; when any sum exceeds 0xFF, every sum
	// is wrapped by subtracting the byte `wrap` (8-bit stores)
	static inline void t3_scroll(uint32_t pkt, const int32_t *off, int n, uint16_t d, uint8_t wrap)
	{
		uint32_t s[4];
		uint32_t any = 0;
		for (int i = 0; i < n; i++)
		{
			s[i] = (uint32_t)U8(pkt, off[i]) + (uint32_t)d;
			any |= s[i];
		}
		const bool over = (int32_t)any > 0xFF;
		for (int i = 0; i < n; i++)
			U8(pkt, off[i]) = over ? (uint8_t)(s[i] - wrap) : (uint8_t)s[i];
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl dm_8C4B30(uint32_t a1);
	uint32_t __cdecl dm_8C4CE0(uint32_t a1);
	uint32_t __cdecl dm_8C4D20(void);
	uint32_t __cdecl dm_8C4E10(uint32_t a1);
	uint32_t __cdecl dm_8C4E60(void);
	uint32_t __cdecl dm_8C52C0(uint32_t a1);
	uint32_t __cdecl dm_8C5350(uint32_t a1);
	uint32_t __cdecl dm_8C5410(uint32_t a1);
	uint32_t __cdecl dm_8C5460(uint32_t a1);
	uint32_t __cdecl dm_8C54E0(uint32_t a1);
	uint32_t __cdecl dm_8C5610(uint32_t a1);
	uint32_t __cdecl dm_8C5650(uint32_t a1);
	uint32_t __cdecl dm_8C56A0(uint32_t a1);
	uint32_t __cdecl dm_8C5710(uint32_t a1);
	uint32_t __cdecl dm_8C5860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl dm_8C5910(uint32_t a1);
	uint32_t __cdecl dm_8C5B10(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl dm_8C5DD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C5F00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C6120(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C63B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C68A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C73C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C7920(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8C7FF0(uint32_t a1);
	uint32_t __cdecl dm_8C80C0(uint32_t a1);
	uint32_t __cdecl dm_8C8120(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8C8350(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8C8400(uint32_t a1);
	uint32_t __cdecl dm_8C84A0(void);
	uint32_t __cdecl dm_8C8500(uint32_t a1);
	uint32_t __cdecl dm_8C87F0(uint32_t a1);
	uint32_t __cdecl dm_8C8840(uint32_t a1);
	uint32_t __cdecl dm_8C8870(uint32_t a1);
	uint32_t __cdecl dm_8C8890(uint32_t a1);
	uint32_t __cdecl dm_8C8930(uint32_t a1);
	uint32_t __cdecl dm_8C8B10(uint32_t a1);
	uint32_t __cdecl dm_8C8D90(void);
	uint32_t __cdecl dm_8C9440(uint32_t a1);
	uint32_t __cdecl dm_8C94D0(void);
	uint32_t __cdecl dm_8C9530(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8C9690(void);
	uint32_t __cdecl dm_8C9750(uint32_t a1);
	uint32_t __cdecl dm_8C97D0(void);
	uint32_t __cdecl dm_8C9810(uint32_t a1);
	uint32_t __cdecl dm_8CA840(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CA950(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CA9D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CAA00(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl dm_8CAAA0(uint32_t a1);
	uint32_t __cdecl dm_8CAB20(uint32_t a1);
	uint32_t __cdecl dm_8CABA0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl dm_8CB380(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CB5A0(uint32_t a1);
	uint32_t __cdecl dm_8CC090(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CC120(void);
	uint32_t __cdecl dm_8CC1B0(uint32_t a1);
	uint32_t __cdecl dm_8CC280(uint32_t a1);
	uint32_t __cdecl dm_8CC330(uint32_t a1);
	uint32_t __cdecl dm_8CC350(uint32_t a1);
	uint32_t __cdecl dm_8CC3A0(uint32_t a1);
	uint32_t __cdecl dm_8CC450(uint32_t a1);
	uint32_t __cdecl dm_8CC6B0(uint32_t a1);
	uint32_t __cdecl dm_8CC7B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8CCF80(uint32_t a1);
	uint32_t __cdecl dm_8CCFE0(uint32_t a1);
	uint32_t __cdecl dm_8CD020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl dm_8CD090(uint32_t a1);
	uint32_t __cdecl dm_8CD0E0(uint32_t a1);
	uint32_t __cdecl dm_8CD120(uint32_t a1);
	uint32_t __cdecl dm_8CD1A0(uint32_t a1);
	uint32_t __cdecl dm_8CD1D0(uint32_t a1);
	uint32_t __cdecl dm_8CD200(uint32_t a1);
	uint32_t __cdecl dm_8CD250(uint32_t a1);
	uint32_t __cdecl dm_8CD290(uint32_t a1);
	uint32_t __cdecl dm_8CD2B0(uint32_t a1);
	uint32_t __cdecl dm_8CD310(uint32_t a1, uint32_t a2);
	uint32_t __cdecl dm_8CD430(uint32_t a1);
	uint32_t __cdecl dm_8CD510(uint32_t a1);
	uint32_t __cdecl dm_8CD540(uint32_t a1);
	uint32_t __cdecl dm_8CD670(uint32_t a1);
	uint32_t __cdecl dm_8CD770(uint32_t a1);
	uint32_t __cdecl dm_8CDE40(uint32_t a1);
	uint32_t __cdecl dm_8CDE60(uint32_t a1);
	uint32_t __cdecl dm_8CDF30(uint32_t a1);
	uint32_t __cdecl dm_8CDF60(uint32_t a1);
	uint32_t __cdecl dm_8CDFB0(uint32_t a1);
	uint32_t __cdecl dm_8CE040(uint32_t a1);
	uint32_t __cdecl dm_8CE130(uint32_t a1);
	uint32_t __cdecl dm_8CE1B0(uint32_t a1);
	uint32_t __cdecl dm_8CE1E0(uint32_t a1);
	uint32_t __cdecl dm_8CE200(uint32_t a1);
	uint32_t __cdecl dm_8CE250(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag008_doom_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace doom
{
	// ====================================================================================
	// the library functions of this module whose code matches a Tonberry / Death / Magma Breath /
	// Siren / Boko / Confuse port up to the module's addresses and callees (the port's text with
	// the module's addresses; "copy of" names the source)
	// ====================================================================================
	// 0x8C4CE0 (copy of d_8B6BE0 0x8B6BE0: MAG_014_sub_8B6BE0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x2742870] (0x438 + 0x6900 bytes) and cleared, next state
	uint32_t __cdecl dm_8C4CE0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x275F9D8);
		MEM<uint32_t>(0x275F3EC) = p;
		p += 0x438;
		MEM<uint32_t>(0x275F980) = p;
		p += 0x6900;
		MEM<uint32_t>(0x275F9D8) = p;
		dm_8C4D20();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8C4D20 (copy of d_8B6C20 0x8B6C20: MAG_014_sub_8B6C20): clears the two actor pools (0x438 bytes at [0x275F3EC], 0x6900
	// at [0x275F980]) and the pool cursors / counters
	uint32_t __cdecl dm_8C4D20(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x275F3EC), 0x438);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x275F980), 0x6900);
		MEM<uint16_t>(0x275ABD8) = 0;
		MEM<uint16_t>(0x275F508) = 0;
		MEM<uint16_t>(0x275F238) = 0;
		MEM<uint16_t>(0x2758508) = 0;
		return 0; // void
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl dm_8C4E10(uint32_t a1)
	{
		dm_8C52C0(a1);
		dm_8C4E60();
		x::Effect_AddTaskAndInitFromCtx(0x275F918, 0x8C4F60, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x2758860, 0x8C5350, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl dm_8C5410(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			dm_8C5710(a1);
			d = t1_data();
		}
		const uint16_t nxt = U16(d, 0x44);
		if (U16(d, 0x42) != nxt)
		{
			U16(d, 0x42) = nxt;
			a_73A6D0();
		}
		return 0; // void
	}

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl dm_8C54E0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x1634F24, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8C5610 (copy of s_73BA30 0x73BA30: module; Siren sub_73BA30): director state: advances after cut frame 0x2D
	uint32_t __cdecl dm_8C5610(uint32_t a1)
	{
		uint32_t blk = U32(0x1634F20, 0);
		if (S16(blk, 0x46) <= 0x2D) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8C5650 (copy of b7_7315A0 0x7315A0: module 097 sub_7315A0): director state - at script mark 5 (a_73B640) sets the
	// countdown +0x44 = 1, next state
	uint32_t __cdecl dm_8C5650(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x8C5860 (copy of t_763E60 0x763E60: module 090 sub_763E60): spawns task a2 (0x458 B, queue 0x25A2B60, parent a1) and
	// sets its model data +0x74 = a3, +0x78 = (s16)a4, +0x80 = a5, +0x82 = a6; returns the node.
	uint32_t __cdecl dm_8C5860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x275DF38, a2, 0x458, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x8C5DD0 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8C5F00, 0x8C6120, 0x8C63B0, 0x8C68A0, a_73D770, a_73D9C0, 0x8C73C0,
	// 0x8C7920; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl dm_8C5DD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t hdr = a1;
		if ((U32(hdr, 0x14) & 0x2000) == 0)
			U32(hdr, 4) = U32(hdr, 0) + 8;
		{
			const uint32_t model = U32(hdr, 0);
			const uint32_t b = U8(hdr, 0xA);
			U32(hdr, 0x2C) = U32(model, 0) + model;   // primitive lists
			const uint32_t g = U8(hdr, 9);
			const uint32_t r = U8(hdr, 8);
			x::someCameraWork_45DD60(r, g, b);
		}
		uint32_t cursor = a4;
		typedef uint32_t (__cdecl *Draw)(uint32_t, uint32_t, uint32_t, uint32_t);
		static const Draw lists[8] = { dm_8C5F00, dm_8C6120, dm_8C63B0, dm_8C68A0, a_73D770, a_73D9C0, dm_8C73C0, dm_8C7920 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t list = U32(hdr, 0x2C);
			if (U32(list, 0) != 0)
				cursor = lists[i](hdr, a2, a3, cursor);
			else
				U32(hdr, 0x2C) = list + 4;
		}
		return cursor;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl dm_8C5F00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // (the original keeps it in its a1 slot)
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // ebp / [esp+0xc]
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a4 slot)
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return a4;
		}
		uint32_t pkt = cursor + 0xC;          // edi (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x4000000;       // tag: 4 words
			U32(pkt, -8) = code;
			if (flags & 1)
			{
				code |= 0x2000000;
				U32(pkt, -8) = code;
			}
			if (flags & 4)
				U32(pkt, -8) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt - 4, pkt, pkt + 4);
					x::GTE_AVSZ3();
					if (t1_out_x(S16(pkt, -4))) clip = 1;
					if (t1_out_x(S16(pkt, 0))) clip |= 2;
					if (t1_out_x(S16(pkt, 4))) clip |= 4;
					if (t1_out_y(S16(pkt, -2))) clip |= 0x10;
					if (t1_out_y(S16(pkt, 2))) clip |= 0x20;
					if (t1_out_y(S16(pkt, 6))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x40)
						{
							x::set_unk_1CA8A28(pkt - 8);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt - 8);
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x14;
						pkt += 0x14;
					}
				}
			}
			rec += 0xC;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return cursor;
	}

	// 0x8C6120 (copy of t_764890 0x764890: module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl dm_8C6120(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x5000000;  // tag: 5 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;  // semi-transparent on
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;  // semi-transparent off
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0xC, pkt + 0x10);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0xC))
					clip |= 2;
				if (t2_outx(pkt + 0x10))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0xE))
					clip |= 0x20;
				if (t2_outy(pkt + 0x12))
					clip |= 0x40;
				x::GTE_ReadSXY2(pkt + 0x14);
				x::GTE_AVSZ4();
				if (t2_outx(pkt + 0x14))
					clip |= 8;
				if (t2_outy(pkt + 0x16))
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
				pkt += 0x18;
			}
		next:
			rec += 0xC;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8C63B0 (copy of t_764B20 0x764B20: module 090 sub_764B20): draws the textured-triangle list of render context a1
	// (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage,
	// uv2 in the high half of +8): RTPT, rejects on GTE FLAG / back-face / fully off-screen, optional
	// depth cue, emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ(AVSZ3) + bias) >> a3.
	// With a UV scroll (+0x18/+0x1A) the texel bytes are scrolled (wrapped by +0x28/+0x2A) and the
	// poly is bracketed by two 0xE2 texture-window prims (0xC B each, windows +0x1C / +0x24);
	// without one a DR_MODE prim (0xC B, tpage abr 1) is inserted before the poly.
	uint32_t __cdecl dm_8C63B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x7000000;  // tag: 7 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;
				const uint32_t uv2 = U32(rec, 8) >> 16;
				U32(pkt, 0xC) = U32(rec, 0xC);   // uv0 + clut
				U32(pkt, 0x14) = U32(rec, 0x10); // uv1 + tpage
				U32(pkt, 0x1C) = uv2;            // uv2
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
				x::GTE_AVSZ3();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0x10))
					clip |= 2;
				if (t2_outx(pkt + 0x18))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0x12))
					clip |= 0x20;
				if (t2_outy(pkt + 0x1A))
					clip |= 0x40;
				if ((clip & 7) == 7)
					goto next;
				if ((clip & 0x70) == 0x70)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) != 0)
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						uint8_t s = 0;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x28);  // u wrap
						U8(pkt, 0xC) = (uint8_t)(c0 - s);
						U8(pkt, 0x14) = (uint8_t)(c1 - s);
						U8(pkt, 0x1C) = (uint8_t)(c2 - s);
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x764D8F)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						uint8_t s = 0;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x2A);  // v wrap
						U8(pkt, 0xD) = (uint8_t)(c0 - s);
						U8(pkt, 0x15) = (uint8_t)(c1 - s);
						U8(pkt, 0x1D) = (uint8_t)(c2 - s);
					}
					const uint32_t poly = pkt;
					uint32_t prim = pkt + 0x20;
					pkt += 0x2C;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = t2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					prim = pkt;
					pkt += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = t2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
				else
				{
					const uint32_t poly = pkt;
					const uint32_t mode = pkt + 0x20;
					pkt += 0x2C;
					const uint32_t tpage = x::sub_45C690(0, 1, 0, 0);  // GetTPage(tp 0, abr 1, x 0, y 0)
					x::sub_45BFC0(mode, 0, 0, tpage & 0xFFFF, 0);        // SetDrawMode(mode, dfe 0, dtd 0, tpage, tw NULL)
					x::SSIGPU_InsertPrimAutoDepth(ot, mode);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
				}
			}
		next:
			rec += 0x14;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8C68A0 (copy of t_765010 0x765010: module 090 sub_765010): draws the textured-quad list of render context a1 (count,
	// then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16):
	// RTPT + RTPS, rejects on GTE FLAG / back-face / fully off-screen, optional depth cue, emits one
	// POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. With a UV scroll the texel
	// bytes are scrolled and the poly is bracketed by two 0xE2 texture-window prims; without one a
	// DR_MODE prim (tpage abr 1) is inserted before the poly.
	uint32_t __cdecl dm_8C68A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;  // tag: 9 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;
				U32(pkt, 0xC) = U32(rec, 0xC);   // uv0 + clut
				const uint32_t uv1 = U32(rec, 0x10);
				const uint32_t uv23 = U32(rec, 0x14);
				U32(pkt, 0x1C) = uv23;           // uv2 (uv3 lands in the pad half, as in the original)
				U32(pkt, 0x14) = uv1;            // uv1 + tpage
				U32(pkt, 0x24) = uv23 >> 16;     // uv3
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0x10))
					clip |= 2;
				if (t2_outx(pkt + 0x18))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0x12))
					clip |= 0x20;
				if (t2_outy(pkt + 0x1A))
					clip |= 0x40;
				x::GTE_ReadSXY2(pkt + 0x20);
				x::GTE_AVSZ4();
				if (t2_outx(pkt + 0x20))
					clip |= 8;
				if (t2_outy(pkt + 0x22))
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) != 0)
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c3 = U8(pkt, 0x24) + a;
						uint8_t s = 0;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x28);  // u wrap
						U8(pkt, 0xC) = (uint8_t)(c0 - s);
						U8(pkt, 0x14) = (uint8_t)(c1 - s);
						U8(pkt, 0x1C) = (uint8_t)(c2 - s);
						U8(pkt, 0x24) = (uint8_t)(c3 - s);
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x765326)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c3 = U8(pkt, 0x25) + a;
						uint8_t s = 0;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x2A);  // v wrap
						U8(pkt, 0xD) = (uint8_t)(c0 - s);
						U8(pkt, 0x15) = (uint8_t)(c1 - s);
						U8(pkt, 0x1D) = (uint8_t)(c2 - s);
						U8(pkt, 0x25) = (uint8_t)(c3 - s);
					}
					const uint32_t poly = pkt;
					uint32_t prim = pkt + 0x28;
					pkt += 0x34;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = t2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					prim = pkt;
					pkt += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = t2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
				else
				{
					const uint32_t poly = pkt;
					const uint32_t mode = pkt + 0x28;
					pkt += 0x34;
					const uint32_t tpage = x::sub_45C690(0, 1, 0, 0);  // GetTPage(tp 0, abr 1, x 0, y 0)
					x::sub_45BFC0(mode, 0, 0, tpage & 0xFFFF, 0);        // SetDrawMode(mode, dfe 0, dtd 0, tpage, tw NULL)
					x::SSIGPU_InsertPrimAutoDepth(ot, mode);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
				}
			}
		next:
			rec += 0x18;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8C73C0 (copy of t_765B30 0x765B30: module 090 sub_765B30): prim-list block "Gouraud-textured triangles, UV scroll":
	// 0x1C-byte records (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, rgb1 +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face
	// (NCLIP, unless ctx+0x14 bit 0x20) and off-screen culling, optional lighting (bit 0x80),
	// z = OTZ + ctx+0x10 (>= 0) >> a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the
	// UVs are scrolled (wrapped by ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window
	// packets (ctx+0x1C set, ctx+0x24 restore); else followed by one draw-mode packet (tpage abr 1).
	// Returns the new packet cursor; ctx+0x2C advances past the block.
	uint32_t __cdecl dm_8C73C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		static const int32_t U_OFF[3] = { 0x0C, 0x18, 0x24 };
		static const int32_t V_OFF[3] = { 0x0D, 0x19, 0x25 };
		const uint32_t ctx = a1;
		uint32_t pkt = a4;                              // esi
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;                        // ebx / [esp+0x28]
		const uint32_t vbase = U32(ctx, 4);             // [esp+0x40]
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;                    // tag: 9 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;                      // semi-transparent
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t uv0 = U32(rec, 0x0C);
			const uint32_t w8 = U32(rec, 8);
			const uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x24) = w8 >> 16;                  // uv2 (record +0x0A)
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;                      // [esp+0x24]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					if (t3_out_x(S16(pkt, 8))) clip = 1;
					if (t3_out_x(S16(pkt, 0x14))) clip |= 2;
					if (t3_out_x(S16(pkt, 0x20))) clip |= 4;
					if (t3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (t3_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (t3_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
						{
							// lit: colours rgb1, rgb2 and the packet's rgb0
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							const uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						const uint32_t ot = a2 + (uint32_t)z * 4;   // [esp+0x20]
						const uint16_t du = U16(ctx, 0x18);
						const uint16_t dv = U16(ctx, 0x1A);
						if ((uint16_t)(du | dv) != 0)
						{
							if (du != 0)
								t3_scroll(pkt, U_OFF, 3, du, U8(ctx, 0x28));
							const uint16_t dv2 = U16(ctx, 0x1A);
							if (dv2 != 0)
								t3_scroll(pkt, V_OFF, 3, dv2, U8(ctx, 0x2A));
							// OT is LIFO: texture window set (ctx+0x1C), poly, window restore (ctx+0x24)
							const uint32_t w1 = pkt + 0x28;
							U32(w1, 0) = 0x2000000;
							const uint32_t t1 = (ctx + 0x1C) != 0 ? t3_texwin(ctx + 0x1C) : 0;
							U32(w1, 4) = t1;
							U32(w1, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w1);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							const uint32_t w2 = pkt + 0x34;
							U32(w2, 0) = 0x2000000;
							const uint32_t t2 = (ctx + 0x24) != 0 ? t3_texwin(ctx + 0x24) : 0;
							U32(w2, 4) = t2;
							U32(w2, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w2);
							pkt += 0x40;
						}
						else
						{
							// draw-mode packet (tpage abr 1) inserted before the poly (drawn after it)
							const uint32_t dm = pkt + 0x28;
							const uint32_t tp = x::sub_45C690(0, 1, 0, 0);
							x::sub_45BFC0(dm, 0, 0, tp & 0xFFFF, 0);
							x::SSIGPU_InsertPrimAutoDepth(ot, dm);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							pkt += 0x34;
						}
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8C7920 (copy of t_766090 0x766090: module 090 sub_766090): prim-list block "Gouraud-textured quads, UV scroll":
	// 0x24-byte records (code/rgb0 +0, vertex indices +4/+6/+8/+0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, uv2 +0x14 / uv3 +0x16, rgb1..3 +0x18/+0x1C/+0x20) -> POLY_GT4 (tag
	// 0x0C000000), RTPT + RTPS for the 4th corner, AVSZ4, same culling / lighting / z / scroll /
	// texture-window framing as 0x765B30. Returns the new packet cursor; ctx+0x2C advances.
	uint32_t __cdecl dm_8C7920(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		static const int32_t U_OFF[4] = { 0x0C, 0x18, 0x24, 0x30 };
		static const int32_t V_OFF[4] = { 0x0D, 0x19, 0x25, 0x31 };
		const uint32_t ctx = a1;
		uint32_t pkt = a4;                              // esi
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;                        // ebp / [esp+0x30]
		const uint32_t vbase = U32(ctx, 4);             // [esp+0x4c]
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;                    // tag: 12 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;                      // semi-transparent
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t uv0 = U32(rec, 0x0C);
			const uint32_t uv1 = U32(rec, 0x10);
			const uint32_t uv23 = U32(rec, 0x14);
			U32(pkt, 0x0C) = uv0;
			U32(pkt, 0x24) = uv23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = uv23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;                      // [esp+0x40]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (t3_out_x(S16(pkt, 8))) clip = 1;
					if (t3_out_x(S16(pkt, 0x14))) clip |= 2;
					if (t3_out_x(S16(pkt, 0x20))) clip |= 4;
					if (t3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (t3_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (t3_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (t3_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (t3_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
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
							const uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						const uint32_t ot = a2 + (uint32_t)z * 4;   // ebp
						const uint16_t du = U16(ctx, 0x18);
						const uint16_t dv = U16(ctx, 0x1A);
						if ((uint16_t)(du | dv) != 0)
						{
							if (du != 0)
								t3_scroll(pkt, U_OFF, 4, du, U8(ctx, 0x28));
							const uint16_t dv2 = U16(ctx, 0x1A);
							if (dv2 != 0)
								t3_scroll(pkt, V_OFF, 4, dv2, U8(ctx, 0x2A));
							const uint32_t w1 = pkt + 0x34;
							U32(w1, 0) = 0x2000000;
							const uint32_t t1 = (ctx + 0x1C) != 0 ? t3_texwin(ctx + 0x1C) : 0;
							U32(w1, 4) = t1;
							U32(w1, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w1);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							const uint32_t w2 = pkt + 0x40;
							U32(w2, 0) = 0x2000000;
							const uint32_t t2 = (ctx + 0x24) != 0 ? t3_texwin(ctx + 0x24) : 0;
							U32(w2, 4) = t2;
							U32(w2, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w2);
							pkt += 0x4C;
						}
						else
						{
							const uint32_t dm = pkt + 0x34;
							const uint32_t tp = x::sub_45C690(0, 1, 0, 0);
							x::sub_45BFC0(dm, 0, 0, tp & 0xFFFF, 0);
							x::SSIGPU_InsertPrimAutoDepth(ot, dm);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							pkt += 0x40;
						}
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl dm_8C8840(uint32_t a1)
	{
		const uint32_t r = a_73C280(a1);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(a1);)
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl dm_8C8930(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			dm_8C9440(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl dm_8C8B10(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x275F9DC);
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

	// 0x8C8D90 (copy of mb_825830 0x825830: copy of db_846EA0 0x846EA0: copy of cg_87E620 0x87E620: sub_87E620; = Confuse c_85FFC0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl dm_8C8D90(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x275F9DC);
		int32_t i = 0;
		if (S16(arena, 0x1C) <= 0)
			return arena; // void (eax = arena)
		const uint32_t src = arena + 0x194;
		uint32_t dst = arena + 0x94;
		do
		{
			i++;
			// quirk: for i >= 4 the destinations reach the source (+0x194): the copies are done
			// dword by dword in listing order, so the overlap behaves like the original
			b1_copy16(dst - 0x40, src);
			b1_copy16(dst, src);
			b1_copy16(dst + 0x40, src);
			b1_copy16(dst + 0x80, src);
			b1_copy16(dst + 0xC0, src);
			dst += 0x10;
		} while (i < (int32_t)S16(arena, 0x1C));
		return dst; // void (eax = last destination pointer)
	}

	// 0x8C9440 (copy of c_860620 0x860620: sub_860620, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl dm_8C9440(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x275F9DC, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			dm_8CC120();
			dm_8C97D0();
			// 30 fps layer: see mag008_doom_held.inc
			FX_HELD(held_note_system(sys);)
			dm_8C9690();
			dm_8C94D0();
			sys = U32(0x275F9DC, 0);
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
		U16(0x275F910, 0) = (uint16_t)(U16(0x275F910, 0) + c14);
		U16(0x275F3E8, 0) = (uint16_t)(U16(0x275F3E8, 0) + c16);
		return done;
	}

	// 0x8C94D0 (copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl dm_8C94D0(void)
	{
		uint32_t act = U32(U32(0x275F9DC, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x275F9DC, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					dm_8C9530(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9530 (copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl dm_8C9530(uint32_t a1, uint32_t a2)
	{
		const uint32_t h = MEM<uint32_t>(SCRATCH_SP) - 0x68;
		U32(h, 0) = U32(a1, 0x170);
		const bool def3a = U16(a2, 0x3A) != 0;
		MEM<uint32_t>(SCRATCH_SP) = h;
		U32(h, 0x14) = 0;
		if (!def3a)
			U32(h, 0x14) = 0x30;
		const uint16_t fade = U16(a1, 0x1CE);
		if (fade != 0)
		{
			U32(h, 8) = U32(a1, 0x16C);
			const uint32_t fl = U32(h, 0x14);
			U32(h, 0xC) = (uint32_t)(int32_t)(int16_t)fade;
			U32(h, 0x14) = fl | 0xC0;
		}
		U32(h, 0x10) = 0;
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		const int8_t copies = S8(a1, 0x1D8);
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		if (copies == 1)
		{
			x::GTE_SetRotMatrix(a1 + 0xAC);
			x::GTE_SetTransVector(a1 + 0xAC);
			MEM<uint32_t>(0x1D8E054) = dm_8C5DD0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		}
		else if (copies > 0)
		{
			uint32_t pos = a1 + 0x19E;
			for (int32_t i = 0; i < S8(a1, 0x1D8); i++, pos += 8)
			{
				S32(a1, 0xC0) = S16(pos, -2);
				S32(a1, 0xC4) = S16(pos, 0);
				S32(a1, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(a1 + 0xAC);
				x::GTE_SetTransVector(a1 + 0xAC);
				MEM<uint32_t>(0x1D8E054) = dm_8C5DD0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
			}
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x8C9690 (copy of mb_827650 0x827650: copy of db_848D70 0x848D70: copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl dm_8C9690(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x275F9DC), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x275F9DC);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						dm_8C9750(act);   // (the original also pushes the bone entry, unused)
					}
					else if (n > 0)
					{
						int32_t i = 0;
						uint32_t sub = act + 0x19E;   // edi
						do
						{
							S32(act, 0xC0) = S16(sub, -2);
							S32(act, 0xC4) = S16(sub, 0);
							S32(act, 0xC8) = S16(sub, 2);
							dm_8C9750(act);
							i++;
							sub += 8;
						} while (i < S8(act, 0x1D8));
					}
				}
			}
			act = U32(act, 4);
		} while (act != 0);
		return 0; // void
	}

	// 0x8C9750 (copy of mb_827710 0x827710: copy of db_848E30 0x848E30: copy of cg_880450 0x880450: sub_880450; = Confuse c_861DF0): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl dm_8C9750(uint32_t a1)
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
		return 0; // void
	}

	// 0x8C97D0 (copy of mb_827790 0x827790: copy of db_848EB0 0x848EB0: copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl dm_8C97D0(void)
	{
		uint32_t act = U32(U32(0x275F9DC, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				dm_8CAAA0(act);
			else if (type == 1)
				dm_8C9810(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9810 (copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl dm_8C9810(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x275F9DC);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (dm_8CA950(act, bone) == 0)
			{
				a_740910(act, bone);
				a_741120(act, bone);
				U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
				return 0; // void
			}
			U16(act, 0x1CC) = (uint16_t)(U16(act, 0x1CC) + 1);
			U32(act, 0x170) = 0;
			return 0; // void
		}
		if (phase == 1)
		{
			a_742CD0(act);
			st = MEM<uint32_t>(0x275F9DC);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x8CA840 (copy of mb_828BA0 0x828BA0: copy of db_849F20 0x849F20: copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl dm_8CA840(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x275F9DC, 0);                  // actor state block
		uint32_t dst_tab = U32(st, 0x22C);
		uint32_t src_tab = U32(st, 0x230);
		uint32_t dst = U32(dst_tab, i_dst * 4);                // ebx
		uint32_t i_src1 = U8(U32(data, 0x130), frame);
		uint32_t src1 = U32(src_tab, i_src1 * 4);              // ebp
		uint32_t i_src2 = U8(U32(data, 0x134), frame);
		uint32_t src2 = U32(src_tab, i_src2 * 4);              // [esp+0x18] (a2 slot)
		if (src1 == 0 || src2 == 0)
			return 0; // void
		uint32_t w_blk = x::Field_Alloc(8);                    // esi: {+0 4096 - w, +4 w}
		int32_t frame2 = S16(a1, 0x1CA);                       // (re-read through the a1 slot)
		uint32_t n = U32(dst, 4);
		int32_t w = S16(U32(data, 0x138), frame2 * 2);
		U32(w_blk, 4) = (uint32_t)w;
		U32(w_blk, 0) = (uint32_t)sub32(0x1000, w);
		if ((int32_t)n > 0)
		{
			uint32_t out = dst + 8;                            // ebx
			uint32_t in2 = src2 + 8;                           // edi
			uint32_t delta = src1 - src2;                      // ebp
			do
			{
				x::set_dword_1CA8A30(U32(w_blk, 0));
				gx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
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
		return 0; // void
	}

	// 0x8CA950 (copy of mb_828CB0 0x828CB0: copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl dm_8CA950(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				dm_8CA9D0(a1, a2);
				return 1;
			}
			// -> 0x741A21
		}
		else if (kind == 1)
		{
			// 0x7419F8
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) >= 0)
				return 0;
			dm_8CA9D0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			dm_8CA9D0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x8CA9D0 (copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl dm_8CA9D0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return dm_8CAA00(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x8CAA00 (copy of d_8BBC10 0x8BBC10: sub_8BBC10): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 10 records
	uint32_t __cdecl dm_8CAA00(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x275F3EC);
		int32_t idx = MEM<int16_t>(0x275ABD8);
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
			if (idx >= 9)
				idx = 0;
			tries++;
		} while (tries < 0xA);
		idx++;
		if (idx < 9)
			MEM<uint16_t>(0x275ABD8) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x275ABD8) = 0;
		return slot;
	}

	// 0x8CAAA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl dm_8CAAA0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x8CB450, a1);
		else if (mode == 4)
			callp(0x8CB5A0, a1);
		dm_8CAB20(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8CAB20 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl dm_8CAB20(uint32_t a1)
	{
		uint32_t sys = U32(0x275F9DC, 0);
		int32_t k = S8(a1, 0x6A);
		uint32_t def = U32(U32(sys, 0x224), k * 4);
		int16_t frame = S16(a1, 0x60);
		if (frame > 0x27)
			return 0; // void
		uint32_t total = U8(U32(def, 0xC8), (int32_t)frame);
		if (total == 0)
			return 0; // void
		int32_t batch = S8(def, 0x36);
		int32_t full = (int32_t)total / batch;
		int32_t rest = (int32_t)total % batch;
		if (full > 0)
		{
			do
			{
				int32_t b = S8(def, 0x36);
				dm_8CABA0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			dm_8CABA0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8CABA0 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl dm_8CABA0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x275F92C, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x275F92C, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = dm_8CB380(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x275F9DC, 0);
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
			// (the original keeps these pointers in its own argument slots a1 / a2: compiler reuse)
			uint32_t src = a1 + 0x4C;          // emitter position (16 bytes)
			uint32_t emat = a1 + 0x2C;         // emitter matrix
			uint32_t out_speed = node + 0x184;
			uint32_t mat = node + 0x0C;
			uint32_t pos = node + 0x12C;
			for (int32_t n = (int32_t)a3; n != 0; n--)
			{
				e6_copy16(pos - 0x50, src);    // +0xDC+0x10i: origin
				if (U8(desc, 0x22) == 0)
					e6_copy16(pos, src);       // +0x12C+0x10i: current position
				uint8_t spread = U8(desc, 0x34);
				if (spread == 1)
				{
					// 0x738110: random direction over the whole sphere
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
					// 0x7382AD: random direction on a ring (angles rand, 0, 0x400)
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
					// the y offset is the raw key << 16 (the rotated vector's y is not used)
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
				// 0x738461: particle rotation angles sp+0x30 (random range, or a copy of the
				// spread angles; any other mode keeps the stale scratch words)
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					// quirk 0x73846D: copies the dwords sp+0x28 / sp+0x2C, including the word
					// +0x2E this function never writes (stale module scratch, deterministic;
					// +0x28..+0x2C are also stale when desc+0x34 is neither 1 nor 2)
					uint32_t c0 = U32(sp, 0x28);
					uint32_t c1 = U32(sp, 0x2C);
					U32(sp, 0x30) = c0;
					U32(sp, 0x34) = c1;
				}
				e6_rotate_zxy(mat, sp + 0x30);
				// particle matrix = emitter matrix * particle matrix (column by column)
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
					U32(out_speed, -0x10) = r1;           // +0x174+4i
					uint32_t r2 = a_7422C0(U32(desc, 0x54), U32(desc, 0x58));
					U32(out_speed, 0) = r2;               // +0x184+4i
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
				// three key-colour sets per particle (+0x1E0 / +0x220 / +0x260, 16 bytes each)
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
		dm_8CC090(node, desc);
		U32(0x275F92C, 0) = U32(0x275F92C, 0) + 0x50;
		return 0; // void
	}

	// 0x8CB380 (copy of d_8BC590 0x8BC590: sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x28 records
	uint32_t __cdecl dm_8CB380(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x275F980);
		int32_t i = MEM<int16_t>(0x275F508);
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
			if (i >= 0x27)
				i = 0;
			tries++;
			if (tries >= 0x28)
				break;
		}
		i++;
		if (i >= 0x27)
			MEM<uint16_t>(0x275F508) = 0;
		else
			MEM<uint16_t>(0x275F508) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl dm_8CB5A0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x275F9DC, 0);            // ecx
		int32_t aidx = S8(act, 0x6A);
		int32_t key = S16(act, 0x60);                          // ebx
		const uint32_t data = U32(U32(st, 0x224), aidx * 4);   // edi / [esp+0x10]
		uint8_t sel = U8(U32(data, 0x108), key);               // bp (movzx)
		uint8_t kind = U8(U32(data, 0x100), key);
		// UNINIT 0x72F1CE: the start point is a stack SVECTOR ([esp+0x14] x, [esp+0x16] y,
		// [esp+0x18] z) that only the switch cases fill; an unknown kind / index > 3 leaves it
		// uninitialised (0 here)
		int16_t ax = 0, ay = 0, az = 0;
		uint32_t base = 0;
		bool have = false;
		if (kind == 0)
		{
			if (sel <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5DC
				base = st + (uint32_t)((int32_t)S8(act, 0x6B) << 4) + offs0[sel];
				have = true;
			}
		}
		else if (kind == 1)
		{
			if (sel <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5CC
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
		// first offset (SVECTOR [esp+0x1C]) rotated by the actor matrix -> IR1..3 ([esp+0x24], shorts)
		int16_t v[4];
		int16_t ir[8] = {};
		v[0] = (int16_t)U16(U32(data, 0x110), key * 2);
		v[1] = (int16_t)U16(U32(data, 0x114), key * 2);
		v[2] = (int16_t)U16(U32(data, 0x118), key * 2);
		v[3] = 0;                                              // (pad, not read by LoadV0)
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		az = (int16_t)(az + ir[2]);
		ay = (int16_t)(ay + ir[1]);
		const int32_t key2 = S16(act, 0x60);                   // ecx
		uint8_t kind2 = U8(U32(data, 0x104), key2);
		uint8_t sel2 = U8(U32(data, 0x10C), key2);
		ax = (int16_t)(ax + ir[0]);
		int16_t bx, by, bz;                                    // si, di, bx
		uint32_t base2 = 0;
		bool have2 = false;
		if (kind2 == 0)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5FC
				uint32_t g = U32(0x275F9DC, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x275F9DC, 0);
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
			// quirk 0x72F4EA: an unknown end kind / index > 3 reuses the rotated first offset (the IR
			// of the first MVMVA, still in [esp+0x24..0x29])
			bz = ir[2];
			by = ir[1];
			bx = ir[0];
		}
		// second offset rotated by the actor matrix
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
		return 0; // void
	}

	// 0x8CC090 (copy of mb_82A3F0 0x82A3F0: copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl dm_8CC090(uint32_t a1, uint32_t a2)
	{
		const uint32_t obj = a1;   // edi
		const uint32_t desc = a2;  // esi
		// quirk 0x743112..0x743179: args 2/3 are pushed as 32-bit registers of which only the low
		// 16 bits are set (movsx r16); the high halves are leftover register bits. a_741A70 only
		// reads their low byte, so the zero-extended 16-bit value is passed here.
		if (U8(desc, 0x28) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2C);
			dm_8CAA00(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			dm_8CAA00(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			dm_8CAA00(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			dm_8CAA00(obj, id, variant);
		}
		return 0; // void
	}

	// 0x8CC120 (copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via dm_8CAA00(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl dm_8CC120(void)
	{
		uint32_t dir = MEM<uint32_t>(0x275F9DC);  // eax (re-read only after the calls)
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
					dm_8CAA00(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x275F9DC);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				dm_8CAA00(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x275F9DC);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl dm_8CC1B0(uint32_t a1)
	{
		if (dm_8C9440(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x8CC280 (copy of d_8BD620 0x8BD620: sub_8BD620): stage wobble state 1 - level +0x1C up by 0xA0 per tick to 0x600 (then
	// finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl dm_8CC280(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xA0);
		if (S16(a1, 0x1C) >= 0x600)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl dm_8CC350(uint32_t a1)
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
		return 0; // void
	}

	// 0x8CC450 (copy of d_8B7030 0x8B7030: sub_8B7030): the reaper's draw (hidden: +0x26 bit 2) - dark (bit 3): matrix of the
	// rotation +0x3C / position +0x4C / scale +0x114, drawn by 0x8B7290; else the engine's creature
	// draw (a_746C10) into the module arena
	uint32_t __cdecl dm_8CC450(uint32_t a1)
	{
		const uint16_t f = U16(a1, 0x26);
		if (f & 4)
			return 0; // void
		if (f & 8)
		{
			x::ComposeZYXRotationMatrix(a1 + 0x3C, a1 + 0x70);
			S32(a1, 0x84) = S16(a1, 0x4C);
			S32(a1, 0x88) = S16(a1, 0x4E);
			S32(a1, 0x8C) = S16(a1, 0x50);
			x::scale3DMatrix(a1 + 0x70, a1 + 0x114);
			return dm_8CC6B0(REAPER_CTX);
		}
		MEM<uint32_t>(PACKET_CURSOR) = a_746C10(a1, 0x275E838, MEM<uint32_t>(PACKET_CURSOR));
		return 0; // void
	}

	// 0x8CC6B0 (copy of d_8B7290 0x8B7290: sub_8B7290): the dark reaper draw of render context a1 (0x273AE90): matrix = the
	// model block's (+0x48 -> +0x40) rotation through the camera, its translation through the camera
	// (MAC -> +0x34); the block's shadow (0x5088A0), both models (+0x64 and the second +0x78) through
	// 0x8B7390, then the bone matrices back from the pose
	uint32_t __cdecl dm_8CC6B0(uint32_t a1)
	{
		const uint32_t ctx = a1;
		const uint32_t blk = U32(ctx, 0x48);
		memcpy((void *)ctx, (const void *)(blk + 0x40), 32);
		const uint32_t anim = blk + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(ctx);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x20);
		x::GTE_LoadIRFromMatrixColumn(ctx + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x22);
		x::GTE_LoadIRFromMatrixColumn(ctx + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(ctx + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(ctx + 0x34);
		const uint32_t shadow = x::sub_5088A0(blk, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, MEM<uint32_t>(U32(ctx, 0x4C)));
		MEM<uint32_t>(U32(ctx, 0x4C)) = shadow;
		dm_8CC7B0(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t second = U32(blk, 0x78);
		if (second != 0)
			dm_8CC7B0(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		return 0; // void
	}

	// 0x8CD020 (copy of d_8BD6A0 0x8BD6A0: sub_8BD6A0): render context a1 of the dark draw: cleared (0x278 bytes), +0x40 = the
	// vertex buffer a2, +0x44 = the object block a3 (+4 = a2, screen box 0 / 0 / 0x140 / 0, colour
	// 0x80 x 3, mask all, +0x24 0), +0x48 = the model block a4, +0x4C / +0x50 -> the packet cursor
	uint32_t __cdecl dm_8CD020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x278);
		U32(a1, 0x48) = a4;
		U32(a1, 0x40) = a2;
		U32(a1, 0x4C) = PACKET_CURSOR;
		U32(a1, 0x50) = PACKET_CURSOR;
		U32(a3, 4) = a2;
		U32(a1, 0x44) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFF;
		U16(a3, 0x24) = 0;
		return a3;
	}

	// 0x8CD1D0 (copy of a_739B40 0x739B40: engine; MiniMog sub_739B40): state: when the node's model animation reports
	// completion (au_re_Battle_ReadAnimation_8 == 1), calls au_re_Battle_ReadAnimation_7(node, 2)
	// and advances the state index
	uint32_t __cdecl dm_8CD1D0(uint32_t a1)
	{
		uint32_t r = x::au_re_Battle_ReadAnimation_8(a1);
		if (r == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 2);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8CD250 (copy of b10_717980 0x717980: module 100 sub_717980): creature state: when the animation ends plays anim 4, timer
	// +0x134 = 7, next state
	uint32_t __cdecl dm_8CD250(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) != 1)
			return 0;
		x::au_re_Battle_ReadAnimation_7(a1, 4);
		U16(a1, 0x134) = 7;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x8CD290 (copy of t_7680B0 0x7680B0: module 090 sub_7680B0): creature state 11 - counts +0x134 down, next state at 0.
	uint32_t __cdecl dm_8CD290(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x134) = (uint16_t)(U16(node, 0x134) - 1);
		if (S16(node, 0x134) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x8CD2B0 (copy of b10_7179F0 0x7179F0: module 100 sub_7179F0): creature state: fade-out +0x13A += 0x100 up to 0x1000 (then
	// finishes: +0x26 |= 5, next state); darkness +0x13C = +0x13A, tints the model (0x8CD310 mode 3)
	// and applies the colour (0x717770)
	uint32_t __cdecl dm_8CD2B0(uint32_t a1)
	{
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0x100);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			U8(a1, 0x26) |= 5;
			U16(a1, 0x13A) = 0x1000;
			b103_next_state(a1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		dm_8CD310(a1, 3);
		return dm_8CD430(a1);
	}

	// 0x8CD310 (copy of t_7678B0 0x7678B0: module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl dm_8CD310(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x8CD430 (copy of t_768130 0x768130: module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl dm_8CD430(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t amb = MEM<uint32_t>(0xB8B9A8);
		const int32_t level = S16(node, 0x13C);
		const uint8_t c0 = (uint8_t)(amb & 0xFF);
		const uint8_t c1 = (uint8_t)((amb >> 8) & 0xFF);
		const int32_t d0 = mul32((int32_t)c0, level) / 4096;
		U8(node, 0x5C) = (uint8_t)(c0 - (uint8_t)d0);
		const int32_t d1 = mul32((int32_t)c1, level) / 4096;
		U8(node, 0x5D) = (uint8_t)(c1 - (uint8_t)d1);
		const uint8_t c2 = MEM<uint8_t>(0xB8B9AA);
		const int32_t d2 = mul32((int32_t)c2, level) / 4096;
		U8(node, 0x5E) = (uint8_t)(c2 - (uint8_t)d2);
		return 0; // void
	}

	// 0x8CD670 (copy of s_7433C0 0x7433C0: module Siren, sub_7433C0 TASK): bounding-box sparkle: state (0x743B90 pick sprite,
	// 0x743C00 wait, 0x743C50), then for each of its 4 sprites: velocity (+0x8C+8i) *= 15/16,
	// position (+0x6C+8i) += velocity / 16, draw it (s_7434C0); ++frame; ends when finished
	uint32_t __cdecl dm_8CD670(uint32_t a1)
	{
		uint32_t tab[3] = { 0x8CDE40, 0x8CDE60, 0x8CDEB0 };
		callp(tab[S8(a1, 0x29)], a1);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_sparkle(a1);)
		uint32_t v = a1 + 0x8E;
		for (int32_t n = 4; n != 0; n--)
		{
			int16_t c = S16(v, -2);
			U16(v, -2) = (uint16_t)((uint16_t)c - (int32_t)c / 16);
			c = S16(v, 0);
			U16(v, 0) = (uint16_t)((uint16_t)c - (int32_t)c / 16);
			c = S16(v, 2);
			U16(v, 2) = (uint16_t)((uint16_t)c - (int32_t)c / 16);
			U16(v, -0x22) = (uint16_t)(U16(v, -0x22) + (int32_t)S16(v, -2) / 16);
			U16(v, -0x20) = (uint16_t)(U16(v, -0x20) + (int32_t)S16(v, 0) / 16);
			U16(v, -0x1E) = (uint16_t)(U16(v, -0x1E) + (int32_t)S16(v, 2) / 16);
			U32(a1, 0x1C) = U32(v, -0x22);
			U32(a1, 0x20) = U32(v, -0x1E);
			dm_8CD770(a1);
			v += 8;
		}
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl dm_8CD770(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		const uint32_t blk = x::Field_Alloc(0xD8);
		const uint32_t mat = blk + 0xB8;
		x::UnpackRotationMatrix(0x1D97778, mat);
		const int32_t px = S16(node, 0x1C);
		const int32_t py = S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		const uint32_t tr = blk + 0xCC;
		S32(blk, 0xD0) = py;
		S32(blk, 0xD4) = pz;
		S32(tr, 0) = px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(mat);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(mat);
		x::GTE_LoadIRFromMatrixColumn(blk + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(blk + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(tr);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(tr);
		x::GTE_SetRotMatrix(mat);
		x::GTE_SetTransVector(mat);
		U32(blk, 0) = U32(node, 0x4C);
		const uint16_t frame = U16(node, 0x50);
		const uint16_t colour = U16(node, 0x56);
		U16(blk, 4) = frame;
		const uint32_t cursor = MEM<uint32_t>(0x2758504);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x2758504) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x8CDE40 (copy of db_84CB70 0x84CB70: sub_84CB70): dust state 0 - flipbook 0x1634F28, last frame 16, next state
	uint32_t __cdecl dm_8CDE40(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = DUST_FLIPBOOK;
		U16(a1, 0x52) = 0x10;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl dm_8CDE60(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8CE040 (copy of a_7458E0 0x7458E0: engine; Siren sub_7458E0): unless +0x26 bit2 (hidden): builds a local matrix from
	// the s16 angles +0x44 (x) / +0x48 (z) / +0x46 (y), position +0x1C..0x20 and scale +0x30, composes it with the
	// camera 0x1D97778, loads it into the GTE and draws the prim model (+0x4C model, +0x40, +0x50)
	// with Effect_RenderPrimModel into the effect OT (0x1D8E04C+0x44) at packet cursor 0x1D8E054.
	uint32_t __cdecl dm_8CE040(uint32_t a1)
	{
		const uint32_t node = a1;  // esi
		if (U8(node, 0x26) & 4)
			return 0; // void
		// local Mat4x3 [esp+4] (3x3 s16 + pad + VECTOR t at +0x14); written by 0x8DD770 and the
		// translation stores (UNINIT 0x7458F2: pad bytes +0x12/+0x13, zeroed here, never read)
		uint32_t mbuf[8] = {};
		const uint32_t m = P(mbuf);
		x::MAG_022_sub_8DD770(m);
		x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0x48));
		x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0x46));
		int32_t tx = S16(node, 0x1C);
		int32_t ty = S16(node, 0x1E);
		int32_t tz = S16(node, 0x20);
		S32(m, 0x14) = tx;
		S32(m, 0x18) = ty;
		S32(m, 0x1C) = tz;
		x::scale3DMatrix(m, node + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		x::GTE_SetRotMatrix_W(m);
		x::GTE_SetTransVector_W(m);
		uint32_t prm = x::Field_Alloc(0x58);
		uint32_t model = U32(node, 0x4C);
		uint32_t v40 = U32(node, 0x40);
		U32(prm, 0) = model;
		U32(prm, 8) = v40;
		int32_t v50 = S16(node, 0x50);
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		S32(prm, 0xC) = v50;
		uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(prm, 0x1C) = 0xF0;
		uint32_t next = x::Effect_RenderPrimModel(prm, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = next;
		x::Field_Free(0x58);
		return 0; // void
	}

	// 0x8CE1E0 (copy of t_7680B0 0x7680B0: module 090 sub_7680B0): creature state 11 - counts +0x54 down, next state at 0.
	uint32_t __cdecl dm_8CE1E0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x54) = (uint16_t)(U16(node, 0x54) - 1);
		if (S16(node, 0x54) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x8CE200 (copy of s_745610 0x745610: module 095): +0x50 += 0x400; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl dm_8CE200(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x400);
		if (S16(a1, 0x50) >= 0x1000)
		{
			int8_t st = S8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x50) = 0x1000;
			S8(a1, 0x29) = (int8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl dm_8CE250(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1634F20);
		if (U16(d, 0x48) != 1)
			return 0; // void
		const uint32_t node = a1;
		const int32_t act = S8(node, 0x2A);
		const uint32_t ctx = U32(node, 0xC);
		const int32_t sub = S8(node, 0x2B);
		const uint32_t results = U32(ctx, 4);
		const uint32_t arr = U32(results + (uint32_t)(act * 20), 8);
		x::ApplyActionResultToTarget(arr + (uint32_t)(sub * 24));
		t4_next_state(node);
		return 0; // void
	}

	// ====================================================================================
	// the module's own functions (no matching port elsewhere)
	// ====================================================================================

	// 0x8C4B30 (MAG_008_sub_8C4B30): MASTER task - the engine's a_739F40 with this module's
	// globals: copies the camera matrix, picks this tick's double-buffered pointers (+0x5C parity),
	// runs its 11-state table, then the nine task queues (sum of their results in +0x5E)
	uint32_t __cdecl dm_8C4B30(uint32_t a1)
	{
		g_mod = &MOD_008;
		// 30 fps layer: see act_engine_held.inc
		FX_HELD(held_note_master();)

		const uint32_t node = a1;
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		MEM<uint32_t>(0x275F92C) = 0x2793E58;
		MEM<uint32_t>(0x2758888) = 0x2793E58;
		const uint32_t states[11] = { 0x8C4CC0, 0x8C4CD0, 0x8C4CE0, 0x8C4D60, 0x8CE2E0, 0x8CE320,
			0x8CE330, 0x8CE340, 0x8CE370, 0x8CE380, 0x8CE3A0 };
		if ((U8(node, 0x5C) & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x275E82C);
			const uint32_t v2 = MEM<uint32_t>(0x275DF4C);
			MEM<uint32_t>(0x2758504) = v1;
			MEM<uint32_t>(0x2758884) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x275E828);
			const uint32_t v2 = MEM<uint32_t>(0x275DF48);
			MEM<uint32_t>(0x2758504) = v1;
			MEM<uint32_t>(0x2758884) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x275F910) = 0;
		MEM<uint16_t>(0x275F3E8) = 0;
		static const uint32_t queues[9] = { 0x275E498, 0x2758860, 0x275F918, 0x275F3D8, 0x275E1F8,
			0x2758968, 0x275ABE0, 0x275DF38, 0x275E488 };
		for (int i = 0; i < 9; i++)
		{
			// 30 fps layer: see mag008_doom_held.inc
			FX_HELD(if (i == 8) held_note_tints();)
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C4E60 (MAG_008_sub_8C4E60): the asset table 0x275FA10 (0x16C bytes, cleared; [0x275F23C]):
	// +0x100 = a 0xA000-byte arena carved from the file arena cursor 0x275F9D8, the slot words
	// +0x104..+0x160 = the exe-data model / layout pointers of the effect (0x19F0324 / 0x19EB018 /
	// 0x19ECBB8 / 0x19ED83C), +0x164 / +0x166 = 0; [0x275E830] = 0x1636C78
	uint32_t __cdecl dm_8C4E60(void)
	{
		MEM<uint32_t>(0x275E830) = 0x1636C78;
		MEM<uint32_t>(0x275F23C) = 0x275FA10;
		x::MAG_007_sub_8DCC00(0x275FA10, 0x16C);
		const uint32_t t = MEM<uint32_t>(0x275F23C);
		uint32_t c = MEM<uint32_t>(0x275F9D8);
		U32(t, 0x100) = c;
		c += 0xA000;
		U32(t, 0x114) = 0x19EB018;
		MEM<uint32_t>(0x275F9D8) = c;
		U32(t, 0x118) = 0x19EB018;
		U32(t, 0x11C) = 0x19EB018;
		U32(t, 0x120) = 0x19EB018;
		U32(t, 0x104) = 0x19F0324;
		U32(t, 0x108) = 0x19F0324;
		U32(t, 0x10C) = 0x19F0324;
		U32(t, 0x110) = 0x19F0324;
		U32(t, 0x124) = 0x19EB018;
		U32(t, 0x128) = 0x19ECBB8;
		U32(t, 0x12C) = 0x19ED83C;
		for (int32_t o = 0x130; o <= 0x160; o += 4)
			U32(t, o) = 0x19F0324;
		U16(t, 0x164) = 0;
		U16(t, 0x166) = 0;
		return t;
	}

	// 0x8C52C0 (MAG_008_sub_8C52C0): phase data init - cleared (0x54 bytes), +0 = 0 / +8 = the
	// position of the node's target slot (+0x2D; entity +0x1C / +0x20), +0x4A = the facing angle
	// from +0 to +8 (CartesianToGameAngle - 0x800); the effect light matrix 0x1D99AB0 = rotation
	// about y by that angle
	uint32_t __cdecl dm_8C52C0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(PHASE_DATA), 0x54);
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		const uint32_t ent = 0x1D972C0u + (uint32_t)U8(a1, 0x2D) * 0x9Cu;
		U32(d, 8) = U32(ent, 0x1C);
		U32(d, 0xC) = U32(ent, 0x20);
		U16(d, 0) = 0;
		U16(d, 2) = 0;
		U16(d, 4) = 0;
		const int16_t dx_ = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx_, (uint32_t)(int32_t)S16(d, 0xC));
		const uint32_t d2 = MEM<uint32_t>(PHASE_DATA);
		U16(d2, 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		x::MAG_022_sub_8DD770(0x1D99AB0);
		return x::MAG_022_sub_8DD8A0(0x1D99AB0, (uint32_t)(int32_t)S16(MEM<uint32_t>(PHASE_DATA), 0x4A));
	}

	// 0x8C5350 (MAG_008_sub_8C5350): DIRECTOR task - phase bookkeeping (0x8C5410), then its
	// 15-state table
	uint32_t __cdecl dm_8C5350(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[15] = { 0x8C5460, 0x8C5490, 0x8C54E0, 0x8C5530, 0x8C5590, 0x8C55B0,
			0x8C55D0, 0x8C55F0, 0x8C5610, 0x8C5630, 0x8C5650, 0x8C5680, 0x8C56A0, 0x8C56D0,
			0x8C56F0 };
		dm_8C5410(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5460 (MAG_008_UploadReaperTIMs): director state 0 - uploads the reaper's two TIMs
	// (0x181CFAC / 0x18253CC), countdown +0x44 = 12, next state
	uint32_t __cdecl dm_8C5460(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x181CFAC);
		x::Battle_QueueTIMUpload_GetEOF(0x18253CC);
		U16(a1, 0x44) = 0xC;
		next_state(a1);
		return a1;
	}

	// 0x8C56A0 (sub_8C56A0): director state - at script mark 6 (a_73B640) sets phase data +0x48 = 1,
	// next state
	uint32_t __cdecl dm_8C56A0(uint32_t a1)
	{
		if (a_73B640(6) == 0)
			return 0;
		U16(MEM<uint32_t>(PHASE_DATA), 0x48) = 1;
		next_state(a1);
		return a1;
	}

	// 0x8C5710 (MAG_008_SpawnReaperCreature): spawns the tasks of the new phase (phase data +0x40):
	// 2 = the aura actor system 0x8C88D0 (actor data 0x1818B78), the reaper 0x8CC3A0 (0x140 B, queue
	// 0x275F3D8, model container 0x1636CE8 animation 3), the dust task 0x8CD4B0, the smoke task
	// 0x8CDED0 and the stage wobble in 0x8CC1E0; 4 = the scythe prim-model task 0x8C58B0 (layout
	// [asset table +0x124], 0xC0), a screen tint (engine task 0x8DDC30, colour script 0x163ACB4,
	// delay 0x1A) and the stage wobble out 0x8CC2D0
	uint32_t __cdecl dm_8C5710(uint32_t a1)
	{
		const int32_t k = S16(MEM<uint32_t>(PHASE_DATA), 0x40);
		if (k == 2)
		{
			a_73C100(a1, 0x8C88D0, 0x1818B78, 0, 0x2D, 2);
			const uint32_t reaper = x::Effect_AddTaskAndInitFromCtx(0x275F3D8, 0x8CC3A0, 0x140, a1);
			x::Effect_BindModelContainerSetAnim(reaper, 0x1636CE8, 3);
			x::Effect_AddTaskAndInitFromCtx(0x2758968, 0x8CD4B0, 0xB0, a1);
			x::Effect_AddTaskAndInitFromCtx(0x275ABE0, 0x8CDED0, 0x58, a1);
			return x::Effect_AddTaskAndInitFromCtx(0x2758968, 0x8CC1E0, 0xB0, a1);
		}
		if (k == 4)
		{
			dm_8C5860(a1, 0x8C58B0, U32(MEM<uint32_t>(0x275F23C), 0x124), 0xC0, 0, 0);
			const uint32_t w = x::Effect_AddTaskAndInitFromCtx(0x275E488, 0x8DDC30, 0x40, a1);
			U32(w, 0x34) = 0x163ACB4;
			U16(w, 0x38) = 0x1A;
			return x::Effect_AddTaskAndInitFromCtx(0x2758968, 0x8CC2D0, 0xB0, a1);
		}
		return (uint32_t)(k - 4);
	}

	// 0x8C5910 (sub_8C5910): scythe state 0 - at its tick 11: decodes the prim layout (+0x74 / +0x78
	// -> +0x94), faces the phase data's angle (+0x62 = +0x4A), stands at the phase data's position
	// moved by the angle's rotation of (-17, -1076, -409), scale 1.0, +0x60 = 0xF00, +0x88 = -16,
	// first model step (a_73C280), next state
	uint32_t __cdecl dm_8C5910(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U16(node, 0x24) != 0xB)
			return 0;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		U16(node, 0x62) = U16(d, 0x4A);
		U32(node, 0x1C) = U32(d, 0);
		U32(node, 0x20) = U32(d, 4);
		// stack: [esp] vector (pad +6 unwritten), [esp+8] result, [esp+0x10] Mat4x3
		alignas(4) uint8_t loc[0x30] = {};
		const uint32_t L = P(loc);
		x::MAG_022_sub_8DD770(L + 0x10);
		x::MAG_022_sub_8DD8A0(L + 0x10, (uint32_t)(int32_t)S16(node, 0x62));
		U16(L, 0) = 0xFFEF;
		U16(L, 2) = 0xFBCC;
		U16(L, 4) = 0xFE67;
		x::matrixMultiplyVector(L + 0x10, L, L + 8);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + U16(L, 0xC));
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + U16(L, 8));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(L, 0xA));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x60) = 0xF00;
		U16(node, 0x88) = 0xFFF0;
		a_73C280(node);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(node);)
		next_state(node);
		return 0; // void
	}

	// 0x8C5B10 (sub_8C5B10): prim-model player draw callback (layout a1, record a2, block a3) -
	// picks the object's vertex frame (lerped by MAG_017_sub_701390), builds its matrix (rotation
	// 0x701310, or 0x701430 with record flag 0x400; parent-rotated position unless record flag
	// 0x200; scale: a diagonal matrix product 0x56C220 with flag 0x100, else scale3DMatrix), fills a
	// 0x68-byte Field_Alloc render header (flags 0x2000 / 0x2030 |0xC0 with alpha, colour, depth
	// offset block +0x58, scale words 0x100) and draws it with 0x8C5DD0 into OT base+0x44 (shift 2)
	// at the module packet cursor 0x2758504
	uint32_t __cdecl dm_8C5B10(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten), +8 scale (3 x int32, or the 3x3 s16
		// diagonal matrix of the 0x100 path), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t V = L;
		const uint32_t S = L + 8;
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void

		const uint32_t hdr = x::Field_Alloc(0x68);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);   // second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // first vertex frame
		U32(hdr, 0) = base;
		// vertex frame f: base + 0xC + f * vertex_count * 8
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
			const int16_t t = S16(rec, 0x26);   // blend factor 0..0x1000
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

		if ((U32(rec, 4) & 0x400) != 0)
			x::MAG_117_sub_701430(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint32_t flags0 = U32(rec, 4);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		int32_t t0, t1, t2;
		if ((flags0 & 0x200) != 0)
		{
			// record flag 0x200: position not rotated by the parent (no GTE_MatrixMultiply either)
			t0 = (int16_t)vx;
			t1 = (int16_t)vy;
			t2 = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(V);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(blk, M);
			t2 = S32(M, 0x1C);
			t1 = S32(M, 0x18);
			t0 = S32(M, 0x14);
		}
		S32(M, 0x14) = add32(t0, S32(blk, 0x14));
		S32(M, 0x18) = add32(t1, S32(blk, 0x18));
		S32(M, 0x1C) = add32(t2, S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			if ((U32(rec, 4) & 0x100) != 0)
			{
				// diagonal s16 matrix {sx, sy, sz} at [esp+0x18] (the 0x56C220 product)
				U16(S, 0) = U16(rec, 0x18);
				U16(S, 8) = U16(rec, 0x1A);
				U16(S, 0x10) = U16(rec, 0x1C);
				U16(S, 0xA) = 0;
				U16(S, 4) = 0;
				U16(S, 0xE) = 0;
				U16(S, 2) = 0;
				U16(S, 0xC) = 0;
				U16(S, 6) = 0;
				x::sub_56C220(M, S);
			}
			else
			{
				S32(S, 0) = S16(rec, 0x18);
				S32(S, 4) = S16(rec, 0x1A);
				S32(S, 8) = S16(rec, 0x1C);
				x::scale3DMatrix(M, S);
			}
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t flags = U32(rec, 4);
		U32(hdr, 0x14) = (flags & 0x4000) != 0 ? 0x2000u : 0x2030u;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			const uint32_t fl = U32(hdr, 0x14);
			U32(hdr, 8) = U32(rec, 0x20);   // colour
			U32(hdr, 0x14) = fl | 0xC0;
		}
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x24) = 0;
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		MEM<uint32_t>(PACKET_CURSOR) = dm_8C5DD0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8C7FF0 (sub_8C7FF0): scythe state 1 - model step (a_73C280): at step 0x13 spawns the doom
	// clock prim-model task 0x8C8050 (layout [asset table +0x128], 0x8C, 1) at the scythe's position; when
	// the model ended (0) finishes (+0x26 |= 1), next state
	uint32_t __cdecl dm_8C7FF0(uint32_t a1)
	{
		const uint32_t r = a_73C280(a1);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(a1);)
		if ((uint16_t)r == 0x13)
		{
			const uint32_t t = dm_8C5860(a1, 0x8C8050, U32(MEM<uint32_t>(0x275F23C), 0x128), 0x8C, 1, 0);
			U32(t, 0x1C) = U32(a1, 0x1C);
			U32(t, 0x20) = U32(a1, 0x20);
			return t;
		}
		if ((uint16_t)r == 0)
		{
			U8(a1, 0x26) |= 1;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8C80C0 (sub_8C80C0): doom clock state 0 - decodes the prim layout, scale 1.0, builds the
	// clock rings around the target slot (+0x2D, 0x8C8120), places the hand (0x8C8350 with the step
	// +0x454), first model step (a_73C280), next state
	uint32_t __cdecl dm_8C80C0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		dm_8C8120(node + 0x1C, (uint32_t)U8(node, 0x2D));
		dm_8C8350(node + 0x1C, (uint32_t)U16(node, 0x454));
		a_73C280(node);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(node);)
		next_state(node);
		return 0; // void
	}

	// 0x8C8120 (sub_8C8120): doom clock set-up - the clock block [0x1636C04] (cleared, 0x58 bytes):
	// colours +0x40 / +0x44 / +0x48, +0x30 = the position a1, +0x38 = the spawn position 0xF1 of
	// the entity a2 (GetEffectSpawnPosition), +0x4C = their distance, +0x20 / +0x24 / +0x28 = scale
	// (1.0, 1.0, distance), +0 = the matrix from the direction (+0x38 - +0x30) and up (0, 1.0, 0),
	// its translation = +0x30. Ring template 0x275E4A8: 17 rows x 4 points {x = 0x1636C08[k], 0,
	// z = row * 0xF0}; ring 0x275D010 = template rotated by 0 about z, y += 0x1636C10[row]; ring
	// 0x275CDF0 = template rotated by 0x400 about z, y += 0x1636C10[row] * distance / 4096
	uint32_t __cdecl dm_8C8120(uint32_t a1, uint32_t a2)
	{
		// stack: [esp+0] up vector (+6 pad unwritten), [esp+8] direction (+0xE pad unwritten),
		// [esp+0x10] rotation matrix
		alignas(4) uint8_t loc[0x30] = {};
		const uint32_t L = P(loc);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(CLOCK_BLOCK), 0x58);
		uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
		U8(b, 0x48) = 0x60;
		U8(b, 0x49) = 0;
		U8(b, 0x4A) = 0x20;
		U8(b, 0x44) = 0x30;
		U8(b, 0x45) = 0;
		U8(b, 0x46) = 0x10;
		U8(b, 0x40) = 0;
		U8(b, 0x41) = 0;
		U8(b, 0x42) = 0;
		U32(b, 0x30) = U32(a1, 0);
		U32(b, 0x34) = U32(a1, 4);
		const uint32_t ent = 0x1D972C0u + (uint32_t)((int32_t)(int16_t)a2 * 0x9C);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, b + 0x38);
		b = MEM<uint32_t>(CLOCK_BLOCK);
		const int32_t ddx = sub32(S16(b, 0x38), S16(b, 0x30));
		const int32_t ddy = sub32(S16(b, 0x3A), S16(b, 0x32));
		const int32_t ddz = sub32(S16(b, 0x3C), S16(b, 0x34));
		const int32_t sq = add32(add32(mul32(ddz, ddz), mul32(ddy, ddy)), mul32(ddx, ddx));
		U32(b, 0x4C) = x::Sqrt((uint32_t)sq);
		b = MEM<uint32_t>(CLOCK_BLOCK);
		U16(L, 0) = 0;
		U32(b, 0x20) = 0x1000;
		U32(b, 0x24) = 0x1000;
		U16(L, 2) = 0x1000;
		U32(b, 0x28) = U32(b, 0x4C);
		U16(L, 4) = 0;
		U16(L, 8) = (uint16_t)(U16(b, 0x38) - U16(b, 0x30));
		U16(L, 0xA) = (uint16_t)(U16(b, 0x3A) - U16(b, 0x32));
		U16(L, 0xC) = (uint16_t)(U16(b, 0x3C) - U16(b, 0x34));
		Effect_BuildMatrixFromDirAndUp(b, L + 8, L);
		b = MEM<uint32_t>(CLOCK_BLOCK);
		S32(b, 0x14) = S16(b, 0x30);
		S32(b, 0x18) = S16(b, 0x32);
		S32(b, 0x1C) = S16(b, 0x34);
		// ring template: 17 rows x 4 points (x from 0x1636C08, y 0, z = row * 0xF0; 4th word kept)
		uint32_t p = 0x275E4A8;
		for (int32_t row = 0; row < 0x11; row++)
		{
			const uint16_t z = (uint16_t)(row * 0xF0);
			for (uint32_t s = 0x1636C08; s < 0x1636C10; s += 2)
			{
				U16(p, 0) = MEM<uint16_t>(s);
				U16(p, 2) = 0;
				U16(p, 4) = z;
				p += 8;
			}
		}
		const uint32_t M = L + 0x10;
		{
			x::MAG_022_sub_8DD770(M);
			x::sub_8DD960(M, 0);
			uint32_t src = 0x275E4A8, dst = 0x275D010;
			for (uint32_t w = 0x1636C10; w < 0x1636C32; w += 2)
				for (int k = 4; k != 0; k--)
				{
					x::matrixMultiplyVector(M, src, dst);
					U16(dst, 2) = (uint16_t)(U16(dst, 2) + MEM<uint16_t>(w));
					src += 8;
					dst += 8;
				}
		}
		{
			x::MAG_022_sub_8DD770(M);
			x::sub_8DD960(M, 0x400);
			uint32_t src = 0x275E4A8, dst = 0x275CDF0;
			for (uint32_t w = 0x1636C10; w < 0x1636C32; w += 2)
				for (int k = 4; k != 0; k--)
				{
					x::matrixMultiplyVector(M, src, dst);
					const int32_t sc = mul32(S32(MEM<uint32_t>(CLOCK_BLOCK), 0x28), (int32_t)MEM<int16_t>(w));
					src += 8;
					dst += 8;
					U16(dst, -6) = (uint16_t)(U16(dst, -6) + (uint16_t)(sc / 4096));
				}
		}
		return 0; // void
	}

	// 0x8C8350 (sub_8C8350): the clock hand's position a1 (s16 x3) for step a2: the clock block's
	// matrix scaled by +0x20 applied to (0, 0x1636C10[a2] * distance / 4096, a2 * 0xF0), plus the
	// block's origin +0x30
	uint32_t __cdecl dm_8C8350(uint32_t a1, uint32_t a2)
	{
		// stack: [esp] vector (+6 pad unwritten), [esp+8] Mat4x3
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t L = P(loc);
		const uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
		U16(L, 0) = 0;
		const int32_t y = mul32((int32_t)MEM<int16_t>(0x1636C10 + (uint32_t)((int32_t)(int16_t)a2 * 2)), S32(b, 0x28)) / 4096;
		U16(L, 2) = (uint16_t)y;
		U16(L, 4) = (uint16_t)(a2 * 0xF0);
		x::MAG_022_sub_8DD770(L + 8);
		memcpy((void *)(L + 8), (const void *)MEM<uint32_t>(CLOCK_BLOCK), 8 * 4);
		x::scale3DMatrix(L + 8, MEM<uint32_t>(CLOCK_BLOCK) + 0x20);
		x::matrixMultiplyVector(L + 8, L, a1);
		const uint32_t b2 = MEM<uint32_t>(CLOCK_BLOCK);
		U16(a1, 0) = (uint16_t)(U16(a1, 0) + U16(b2, 0x30));
		U16(a1, 2) = (uint16_t)(U16(a1, 2) + U16(b2, 0x32));
		U16(a1, 4) = (uint16_t)(U16(a1, 4) + U16(b2, 0x34));
		return 0; // void
	}

	// 0x8C8400 (sub_8C8400): doom clock state 1 - next hand step (+0x456) from the table 0x1636C34
	// into +0x454; at the end mark (>= 0x7F): the hand goes back to its start position (+0x44C),
	// step 0x11, spawns the strike prim-model task 0x8C8790 (layout [asset table +0x12C], 0x3B0, 2)
	// at the clock's end +0x38, next state; places the hand (0x8C8350), model step (a_73C280) and
	// draws the next ring step (0x8C84A0)
	uint32_t __cdecl dm_8C8400(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x456) = (uint16_t)(U16(node, 0x456) + 1);
		const uint16_t step = MEM<uint16_t>(0x1636C34 + (uint32_t)((int32_t)S16(node, 0x456) * 2));
		U16(node, 0x454) = step;
		if ((int16_t)step >= 0x7F)
		{
			U32(node, 0x1C) = U32(node, 0x44C);
			const uint32_t layout = U32(MEM<uint32_t>(0x275F23C), 0x12C);
			U16(node, 0x454) = 0x11;
			U32(node, 0x20) = U32(node, 0x450);
			const uint32_t t = dm_8C5860(node, 0x8C8790, layout, 0x3B0, 2, 0);
			const uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
			U32(t, 0x1C) = U32(b, 0x38);
			U32(t, 0x20) = U32(b, 0x3C);
			next_state(node);
		}
		dm_8C8350(node + 0x1C, (uint32_t)U16(node, 0x454));
		a_73C280(node);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(node);)
		FX_HELD(held_note_rings(node);)
		return dm_8C84A0();
	}

	// 0x8C84A0 (sub_8C84A0): next ring step of the doom clock: rows [0x1636C40 + 4 * n] .. [+2]
	// (n = clock block +0x54; end mark 0x7F -> returns 0) into +0x50 / +0x52, draws both rings
	// (0x8C8500), ++n, returns 1
	uint32_t __cdecl dm_8C84A0(void)
	{
		uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
		const uint16_t first = MEM<uint16_t>(0x1636C40 + (uint32_t)((int32_t)S16(b, 0x54) * 4));
		if (first == 0x7F)
			return 0;
		U16(b, 0x50) = first;
		U16(b, 0x52) = MEM<uint16_t>(0x1636C42 + (uint32_t)((int32_t)S16(b, 0x54) * 4));
		dm_8C8500(0x275D010);
		dm_8C8500(0x275CDF0);
		b = MEM<uint32_t>(CLOCK_BLOCK);
		U16(b, 0x54) = (uint16_t)(U16(b, 0x54) + 1);
		return 1;
	}

	// 0x8C8500 (sub_8C8500): draws the ring a1 (17 rows x 4 points of 8 bytes) from row +0x50 to
	// row +0x52 of the clock block: row colours 0x275F3F8 (16 bytes per row: black, (r, 0, b) twice,
	// black; r += 6, b += 2 per row from row +0x50), the block's matrix scaled by +0x20 through the
	// camera 0x1D97778, then per row three Gouraud quads (POLY_G4, 0x24 bytes, code 0x3A) of the
	// points k, k+1 and the next row's, OT z = OTZ(AVSZ4) >> 2, into the frame arena 0x1D8E054
	uint32_t __cdecl dm_8C8500(uint32_t a1)
	{
		// stack: [esp] colour bytes r, g, b (+3: see below), [esp+4] OTZ, [esp+8] row,
		// [esp+0xC] OT base, [esp+0x10] Mat4x3
		alignas(4) uint8_t loc[0x30] = {};
		const uint32_t L = P(loc);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C);
		uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
		const int32_t first = S16(b, 0x50);
		const int32_t last1 = S16(b, 0x52) + 1;
		uint32_t pkt = MEM<uint32_t>(0x1D8E054);
		uint32_t col = (uint32_t)(first << 4) + 0x275F3F8;
		U8(L, 0) = 0;
		U8(L, 1) = 0;
		U8(L, 2) = 0;
		// UNINIT 0x8C8545: the colour dword's 4th byte ([esp+0x13]) is never written: the middle
		// two colours of each row keep what older code left there (0 in every harness run and
		// after fx_verify's stack scrub), taken as 0 here; the draw reads only the rgb bytes
		U8(L, 3) = 0;
		if (first < last1)
		{
			for (int32_t n = last1 - first; n != 0; n--)
			{
				const uint32_t c = U32(L, 0);
				for (int32_t j = 0; j < 4; j++, col += 4)
				{
					if (j == 0 || j == 3)
					{
						U8(col, 0) = 0;
						U8(col, 1) = 0;
						U8(col, 2) = 0;
					}
					else
						U32(col, 0) = c;
				}
				U8(L, 0) = (uint8_t)(U8(L, 0) + 6);
				U8(L, 2) = (uint8_t)(U8(L, 2) + 2);
			}
		}
		const uint32_t M = L + 0x10;
		memcpy((void *)M, (const void *)b, 8 * 4);
		x::scale3DMatrix(M, b + 0x20);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(M);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(M);
		x::GTE_LoadIRFromMatrixColumn(M + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(M + 2);
		x::GTE_LoadIRFromMatrixColumn(M + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(M + 4);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(M + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(M + 0x14);
		x::GTE_SetRotMatrix(M);
		x::GTE_SetTransVector(M);
		b = MEM<uint32_t>(CLOCK_BLOCK);
		int32_t row = S16(b, 0x50);
		uint32_t crow = (uint32_t)(row << 4) + 0x275F3F8;
		uint32_t vrow = a1 + (uint32_t)(row << 5);
		if (row < S16(b, 0x52))
		{
			do
			{
				uint32_t v = vrow;
				uint32_t sxy = pkt + 0x20;
				uint32_t vn = vrow + 8;
				for (int32_t k = 3; k != 0; k--)
				{
					U32(pkt, 0) = 0x8000000;
					U8(sxy, -0x19) = 0x3A;
					x::GTE_LoadV012(v, vn, vn + 0x18);
					x::GTE_RTPT();
					GTE_StoreSXY012_PolyG4(pkt);
					x::GTE_LoadV0(vn + 0x20);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(sxy);
					U8(sxy, -0x1C) = U8(crow, 0);
					U8(sxy, -0x1B) = U8(crow, 1);
					U8(sxy, -0x1A) = U8(crow, 2);
					U8(sxy, -0x14) = U8(crow, 4);
					U8(sxy, -0x13) = U8(crow, 5);
					U8(sxy, -0x12) = U8(crow, 6);
					U8(sxy, -0xC) = U8(crow, 0x10);
					U8(sxy, -0xB) = U8(crow, 0x11);
					U8(sxy, -0xA) = U8(crow, 0x12);
					U8(sxy, -4) = U8(crow, 0x14);
					U8(sxy, -3) = U8(crow, 0x15);
					U8(sxy, -2) = U8(crow, 0x16);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(L + 4);
					S32(L, 4) = S32(L, 4) >> 2;
					x::SSIGPU_InsertPrimAutoDepth(ot + (uint32_t)S32(L, 4) * 4, pkt);
					pkt += 0x24;
					sxy += 0x24;
					v += 8;
					vn += 8;
					crow += 4;
				}
				crow += 4;
				vrow = v + 8;
				row++;
			} while (row < S16(MEM<uint32_t>(CLOCK_BLOCK), 0x52));
		}
		MEM<uint32_t>(0x1D8E054) = pkt;
		return 0; // void
	}

	// 0x8C87F0 (sub_8C87F0): strike state 0 - decodes the prim layout, scale 1.0, +0x88 = -0x200,
	// first model step (a_73C280), next state
	uint32_t __cdecl dm_8C87F0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFE00;
		a_73C280(node);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(node);)
		next_state(node);
		return 0; // void
	}

	// 0x8C8870 (sub_8C8870): doom clock state 2 - model step (a_73C280; ended -> next state), next
	// ring step (0x8C84A0)
	uint32_t __cdecl dm_8C8870(uint32_t a1)
	{
		const uint32_t r = a_73C280(a1);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_play(a1);)
		if (r == 0)
			next_state(a1);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_rings(a1);)
		return dm_8C84A0();
	}

	// 0x8C8890 (sub_8C8890): doom clock state 3 - ring steps until the end mark, then finished
	// (+0x26 |= 1), next state
	uint32_t __cdecl dm_8C8890(uint32_t a1)
	{
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_rings(a1);)
		const uint32_t r = dm_8C84A0();
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			next_state(a1);
		}
		return r;
	}

	// 0x8CC330 (sub_8CC330): stage wobble state 0 - after tick 0x2D: level +0x1C = 0x600, next state
	uint32_t __cdecl dm_8CC330(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x2D)
		{
			U16(a1, 0x1C) = 0x600;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CC3A0 (sub_8CC3A0): REAPER task - state {0x8CCF80 appear, 0x8CD090 rise, 0x8CD0E0 turn,
	// 0x8CD120 / 0x8CD1A0 script waits, 0x8CD1D0, 0x8CD200, 0x8CD250, 0x8CD290, 0x8CD2B0 fade out,
	// ret}; drawn unless hidden (+0x26 bit 2, 0x8CC450)
	uint32_t __cdecl dm_8CC3A0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[11] = { 0x8CCF80, 0x8CD090, 0x8CD0E0, 0x8CD120, 0x8CD1A0, 0x8CD1D0,
			0x8CD200, 0x8CD250, 0x8CD290, 0x8CD2B0, 0x8CD4A0 };
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag008_doom_held.inc
			FX_HELD(held_note_reaper(node);)
			dm_8CC450(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CC7B0 (sub_8CC7B0): the module's dark model renderer (model a1, OT a2; a3 unused; render
	// context a4: +0x20 camera * model matrix, +0x44 -> {+4 vertex buffer}, +0x48 the model block
	// (entity layout: +0x28 colour, +0x7C object mask), +0x4C -> packet cursor, +0x54 the rising
	// height, +0x58 / +0x5A spiral angle / step, +0x5D.. one step byte per drawn face). Colour
	// tables 0x275F984 / 0x275F930: 20 grey levels (0, 8, .., 0x80) for triangles (code 0x26) /
	// quads (0x2E), the last one opaque (0x24 / 0x2C). Header (0x100 bytes, Field_Alloc): +0x28
	// vertex scratch, +0x38..+0x44 vertex indices, +0x48 / +0x4C packet words, +0x50 object mask,
	// +0x54.. the face's vertices, +0x74 / +0x9C / +0xC4 the spiral offsets (x / y / z, 20 steps),
	// +0xEC NCLIP, +0xF0 OTZ, +0xF4 height, +0xF6 the face's mean height, +0xF8..+0xFC its offset,
	// +0xFE face counter. Per visible object: its vertices through their bone matrices (GTE light
	// matrix / background vector) into the vertex buffer; then its triangles and quads: a face whose
	// mean height is at or below the rising height advances its step (up to 0x13); step 0 = not
	// drawn, 1..0x12 = moved by the spiral offset of the step (mirrored on odd faces), 0x13 = in
	// place; front faces only, flat-textured in the step's grey, OT z = OTZ >> 2
	uint32_t __cdecl dm_8CC7B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t ctx = a4;
		const uint32_t blk = U32(ctx, 0x48);
		uint32_t cursor = MEM<uint32_t>(U32(ctx, 0x4C));        // [esp+0x14]
		const uint32_t vbuf = U32(U32(ctx, 0x44), 4);            // edi
		uint32_t table = U32(a1, 4);
		const uint32_t bones = U32(a1, 0) + 0x10;                // [esp+0x20]
		const int32_t count = S32(table, 0);                     // [esp+0x1C]
		const uint32_t hdr = x::Field_Alloc(0x100);
		table += 4;
		const uint32_t colour = U32(blk, 0x28) & 0xFFFFFF;
		U32(hdr, 0x50) = U32(blk, 0x7C);
		U32(hdr, 0x48) = colour | 0x24000000;
		U32(hdr, 0x4C) = colour | 0x2C000000;
		U16(hdr, 0xF4) = U16(ctx, 0x54);
		U16(hdr, 0xFE) = 0;
		uint32_t face = ctx + 0x5C;                              // [esp+0x10]
		// grey levels
		uint8_t grey = 0;
		for (int32_t k = 0; k < 0x14; k++)
		{
			const uint32_t g = (uint32_t)grey;
			const uint32_t rgb = ((g << 8) + g) << 8 | 0;
			const uint32_t w = rgb + g;
			MEM<uint32_t>(0x275F984 + k * 4) = w;
			MEM<uint32_t>(0x275F930 + k * 4) = w;
			if (k == 0x13)
			{
				MEM<uint32_t>(0x275F9D0) |= 0x24000000;
				MEM<uint32_t>(0x275F97C) = w | 0x2C000000;
			}
			else
			{
				MEM<uint32_t>(0x275F984 + k * 4) = w | 0x26000000;
				MEM<uint32_t>(0x275F930 + k * 4) = w | 0x2E000000;
			}
			grey = (uint8_t)(grey + 8);
			if (grey > 0x80)
				grey = 0x80;
		}
		// spiral offsets: steps 0..15 on a shrinking circle, 16..19 zero
		uint32_t ang = U16(ctx, 0x58);                           // [esp+0x18]
		for (int32_t i = 0; i < 0x14; i++)
		{
			const uint32_t o = hdr + 0x74 + (uint32_t)i * 2;
			if (i < 0x10)
			{
				const int32_t k = 0xF - i;
				const int32_t radius = (int16_t)(k * 0x30);
				const int32_t s = mul32((int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)ang), radius);
				U16(o, 0) = (uint16_t)(s / 4096);
				U16(o, 0x28) = (uint16_t)(k << 6);
				if (S16(o, 0x28) > 0)
					U16(o, 0x28) = 0;
				const int32_t c = mul32((int32_t)x::computeCosine((uint32_t)(int32_t)(int16_t)ang), radius);
				U16(o, 0x50) = (uint16_t)(c / 4096);
				ang = (uint32_t)(uint16_t)(U16(ctx, 0x5A) + (uint16_t)ang) & 0xFFF;
			}
			else
			{
				U16(o, 0) = 0;
				U16(o, 0x28) = 0;
				U16(o, 0x50) = 0;
			}
		}
		x::GTE_SetRotMatrix(ctx + 0x20);
		x::GTE_SetTransVector(ctx + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t v = vbuf;
			const uint32_t off = U32(table, 0);
			table += 4;
			uint32_t obj = U32(a1, 4) + off;
			if (!((U32(hdr, 0x50) >> (i & 31)) & 1))
				continue;
			int32_t groups = S16(obj, 0);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = bones + (uint32_t)(bone * 0x30) + 0x10;
				GTE_SetLightMatrix(m);
				GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					U16(hdr, 0x28) = U16(obj, 0);
					U16(hdr, 0x2A) = U16(obj, 2);
					U16(hdr, 0x2C) = U16(obj, 4);
					obj += 6;
					x::GTE_LoadV0(hdr + 0x28);
					GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(hdr + 0x28);
					// the 4th word (+0x2E) is what the scratch held (the original copies it along)
					U32(v, 0) = U32(hdr, 0x28);
					U32(v, 4) = U32(hdr, 0x2C);
					v += 8;
				}
			}
			obj = (obj + 3) & ~3u;
			const int32_t n3 = S16(obj, 0);
			const int32_t n4 = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			uint32_t pkt = cursor;
			for (int32_t k = n3; k > 0; k--, rec += 0x10)
			{
				U16(hdr, 0xFE) = (uint16_t)(U16(hdr, 0xFE) + 1);
				face++;
				const uint32_t i0 = U16(rec, 0) & 0xFFF, i1 = U16(rec, 2) & 0xFFF, i2 = U16(rec, 4) & 0xFFF;
				U32(hdr, 0x38) = i0;
				U32(hdr, 0x54) = U32(vbuf + i0 * 8, 0);
				U32(hdr, 0x58) = U32(vbuf + i0 * 8, 4);
				U32(hdr, 0x3C) = i1;
				U32(hdr, 0x5C) = U32(vbuf + i1 * 8, 0);
				U32(hdr, 0x60) = U32(vbuf + i1 * 8, 4);
				U32(hdr, 0x64) = U32(vbuf + i2 * 8, 0);
				U32(hdr, 0x40) = i2;
				U32(hdr, 0x68) = U32(vbuf + i2 * 8, 4);
				const int32_t mean = (S16(hdr, 0x56) + S16(hdr, 0x5E) + S16(hdr, 0x66)) / 3;
				U16(hdr, 0xF6) = (uint16_t)mean;
				if ((int16_t)mean >= S16(hdr, 0xF4) && U8(face, 0) < 0x13)
					U8(face, 0) = (uint8_t)(U8(face, 0) + 1);
				const uint8_t st = U8(face, 0);
				if (st == 0)
					continue;
				if (st != 0x13)
				{
					U16(hdr, 0xF8) = U16(hdr, 0x74 + st * 2);
					U16(hdr, 0xFA) = U16(hdr, 0x9C + st * 2);
					U16(hdr, 0xFC) = U16(hdr, 0xC4 + st * 2);
					if (U16(hdr, 0xFE) & 1)
					{
						U16(hdr, 0xF8) = (uint16_t)-U16(hdr, 0xF8);
						U16(hdr, 0xFC) = (uint16_t)-U16(hdr, 0xFC);
					}
					const uint16_t ox = U16(hdr, 0xF8), oy = U16(hdr, 0xFA), oz = U16(hdr, 0xFC);
					U16(hdr, 0x54) = (uint16_t)(U16(hdr, 0x54) + ox);
					U16(hdr, 0x5C) = (uint16_t)(U16(hdr, 0x5C) + ox);
					U16(hdr, 0x64) = (uint16_t)(U16(hdr, 0x64) + ox);
					U16(hdr, 0x56) = (uint16_t)(U16(hdr, 0x56) + oy);
					U16(hdr, 0x58) = (uint16_t)(U16(hdr, 0x58) + oz);
					U16(hdr, 0x60) = (uint16_t)(U16(hdr, 0x60) + oz);
					U16(hdr, 0x68) = (uint16_t)(U16(hdr, 0x68) + oz);
					U16(hdr, 0x5E) = (uint16_t)(U16(hdr, 0x5E) + oy);
					U16(hdr, 0x66) = (uint16_t)(U16(hdr, 0x66) + oy);
				}
				GTE_LoadV012_Packed(hdr + 0x54);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(hdr + 0xEC);
				if (S32(hdr, 0xEC) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(hdr + 0xF0);
				U32(hdr, 0xF0) = U32(hdr, 0xF0) >> 2;
				GTE_StoreSXY012_PolyFT3(pkt);
				U32(pkt, 0) = 0x7000000;
				U32(pkt, 0x14) = U32(rec, 0xC);
				U32(pkt, 0xC) = U32(rec, 8);
				U16(pkt, 0x1C) = U16(rec, 6);
				U32(pkt, 4) = MEM<uint32_t>(0x275F984 + (uint32_t)U8(face, 0) * 4);
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(hdr, 0xF0) * 4, pkt);
				pkt += 0x20;
			}
			if (n4 > 0)
			{
				rec += 4;
				for (int32_t k = n4; k > 0; k--, rec += 0x14)
				{
					U16(hdr, 0xFE) = (uint16_t)(U16(hdr, 0xFE) + 1);
					face++;
					const uint32_t i0 = U16(rec, -4) & 0xFFF, i1 = U16(rec, -2) & 0xFFF;
					const uint32_t i2 = U16(rec, 0) & 0xFFF, i3 = U16(rec, 2) & 0xFFF;
					U32(hdr, 0x40) = i2;
					U32(hdr, 0x38) = i0;
					U32(hdr, 0x44) = i3;
					U32(hdr, 0x54) = U32(vbuf + i0 * 8, 0);
					U32(hdr, 0x58) = U32(vbuf + i0 * 8, 4);
					U32(hdr, 0x3C) = i1;
					U32(hdr, 0x5C) = U32(vbuf + i1 * 8, 0);
					U32(hdr, 0x60) = U32(vbuf + i1 * 8, 4);
					U32(hdr, 0x64) = U32(vbuf + i2 * 8, 0);
					U32(hdr, 0x68) = U32(vbuf + i2 * 8, 4);
					U32(hdr, 0x6C) = U32(vbuf + i3 * 8, 0);
					U32(hdr, 0x70) = U32(vbuf + i3 * 8, 4);
					const int32_t mean = (S16(hdr, 0x6E) + S16(hdr, 0x56) + S16(hdr, 0x5E) + S16(hdr, 0x66)) / 4;
					U16(hdr, 0xF6) = (uint16_t)mean;
					if ((int16_t)mean >= S16(hdr, 0xF4) && U8(face, 0) < 0x13)
						U8(face, 0) = (uint8_t)(U8(face, 0) + 1);
					const uint8_t st = U8(face, 0);
					if (st == 0)
						continue;
					if (st != 0x13)
					{
						U16(hdr, 0xF8) = U16(hdr, 0x74 + st * 2);
						U16(hdr, 0xFA) = U16(hdr, 0x9C + st * 2);
						U16(hdr, 0xFC) = U16(hdr, 0xC4 + st * 2);
						if (U16(hdr, 0xFE) & 1)
						{
							U16(hdr, 0xF8) = (uint16_t)-U16(hdr, 0xF8);
							U16(hdr, 0xFC) = (uint16_t)-U16(hdr, 0xFC);
						}
						const uint16_t ox = U16(hdr, 0xF8), oy = U16(hdr, 0xFA), oz = U16(hdr, 0xFC);
						U16(hdr, 0x54) = (uint16_t)(U16(hdr, 0x54) + ox);
						U16(hdr, 0x5C) = (uint16_t)(U16(hdr, 0x5C) + ox);
						U16(hdr, 0x64) = (uint16_t)(U16(hdr, 0x64) + ox);
						U16(hdr, 0x6C) = (uint16_t)(U16(hdr, 0x6C) + ox);
						U16(hdr, 0x5E) = (uint16_t)(U16(hdr, 0x5E) + oy);
						U16(hdr, 0x56) = (uint16_t)(U16(hdr, 0x56) + oy);
						U16(hdr, 0x58) = (uint16_t)(U16(hdr, 0x58) + oz);
						U16(hdr, 0x60) = (uint16_t)(U16(hdr, 0x60) + oz);
						U16(hdr, 0x68) = (uint16_t)(U16(hdr, 0x68) + oz);
						U16(hdr, 0x70) = (uint16_t)(U16(hdr, 0x70) + oz);
						U16(hdr, 0x66) = (uint16_t)(U16(hdr, 0x66) + oy);
						U16(hdr, 0x6E) = (uint16_t)(U16(hdr, 0x6E) + oy);
					}
					GTE_LoadV012_Packed(hdr + 0x54);
					x::GTE_RTPT();
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(hdr + 0xEC);
					if (S32(hdr, 0xEC) <= 0)
						continue;
					GTE_StoreSXY012_PolyFT3_2(pkt);
					x::GTE_LoadV0(hdr + 0x6C);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(hdr + 0xF0);
					const uint32_t z = U32(hdr, 0xF0) >> 2;
					U32(hdr, 0xF0) = z;
					U32(pkt, 0xC) = U32(rec, 4);
					U32(pkt, 0x14) = U32(rec, 8);
					U16(pkt, 0x1C) = U16(rec, 0xC);
					U16(pkt, 0x24) = U16(rec, 0xE);
					U32(pkt, 0) = 0x9000000;
					U32(pkt, 4) = MEM<uint32_t>(0x275F930 + (uint32_t)U8(face, 0) * 4);
					x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, pkt);
					pkt += 0x28;
				}
			}
			cursor = pkt;
		}
		const uint32_t out = U32(ctx, 0x4C);
		MEM<uint32_t>(out) = cursor;
		return x::Field_Free(0x100);
	}

	// 0x8CCF80 (sub_8CCF80): reaper state 0 - stands at the phase data's position facing its angle
	// (+0x3E = +0x4A), scale +0x114.. = 1.0 (pointer +0x60), animation 0 (au_re_Battle_ReadAnimation_7),
	// prepares the dark draw (0x8CCFE0), next state
	uint32_t __cdecl dm_8CCF80(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		U32(node, 0x4C) = U32(d, 0);
		const uint32_t w4 = U32(d, 4);
		U16(node, 0x3E) = U16(d, 0x4A);
		U32(node, 0x60) = node + 0x114;
		U32(node, 0x50) = w4;
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		dm_8CCFE0(node);
		next_state(node);
		return 0; // void
	}

	// 0x8CCFE0 (sub_8CCFE0): the reaper's dark draw: render context 0x2758510 (0x8CD020: vertex
	// buffer 0x275ABF0, object block 0x275F9E0, model block = the node +0x30), dark (+0x26 |= 8),
	// rising height 0x2758564 = 0, spiral step 0x275856A = 0x100
	uint32_t __cdecl dm_8CCFE0(uint32_t a1)
	{
		dm_8CD020(REAPER_CTX, 0x275ABF0, 0x275F9E0, a1 + 0x30);
		U8(a1, 0x26) |= 8;
		MEM<uint16_t>(0x2758564) = 0;
		MEM<uint16_t>(0x275856A) = 0x100;
		return 0; // void
	}

	// 0x8CD090 (sub_8CD090): reaper state 1 - the spiral angle 0x2758568 turns by the step, the
	// rising height 0x2758564 goes down by 0x48 per tick; at -0x500 or below: timer +0x134 = 16,
	// next state
	uint32_t __cdecl dm_8CD090(uint32_t a1)
	{
		const uint16_t a = (uint16_t)(MEM<uint16_t>(0x2758568) + MEM<uint16_t>(0x275856A));
		const uint16_t h = (uint16_t)(MEM<uint16_t>(0x2758564) - 0x48);
		MEM<uint16_t>(0x2758564) = h;
		MEM<uint16_t>(0x2758568) = (uint16_t)(a & 0xFFF);
		if ((int16_t)h <= (int16_t)0xFB00)
		{
			U16(a1, 0x134) = 0x10;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CD0E0 (sub_8CD0E0): reaper state 2 - the spiral angle turns; after 16 ticks (+0x134) the
	// dark draw ends (+0x26 bit 3 cleared), next state
	uint32_t __cdecl dm_8CD0E0(uint32_t a1)
	{
		MEM<uint16_t>(0x2758568) = (uint16_t)((MEM<uint16_t>(0x2758568) + MEM<uint16_t>(0x275856A)) & 0xFFF);
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) - 1);
		if (S16(a1, 0x134) <= 0)
		{
			U8(a1, 0x26) &= 0xF7;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CD120 (sub_8CD120): reaper state 3 - at script mark 4 (a_73B640) a_73BB60(0), next state;
	// else a_73BB60(1)
	uint32_t __cdecl dm_8CD120(uint32_t a1)
	{
		if (a_73B640(4) != 0)
		{
			a_73BB60(0);
			next_state(a1);
			return a1;
		}
		return a_73BB60(1);
	}

	// 0x8CD1A0 (sub_8CD1A0): reaper state 4 - at script mark 4 (a_73B7E0) animation 1, next state
	uint32_t __cdecl dm_8CD1A0(uint32_t a1)
	{
		if (a_73B7E0(4) == 0)
			return 0;
		x::au_re_Battle_ReadAnimation_7(a1, 1);
		next_state(a1);
		return 0; // void
	}

	// 0x8CD200 (sub_8CD200): reaper state 6 - spawn position 0x1B of the model block +0x30 (result
	// unused), animation step (0x8DD1C0); once the animation looped (+0x138 >= 1) animation 3, next
	// state
	uint32_t __cdecl dm_8CD200(uint32_t a1)
	{
		alignas(4) uint8_t loc[8] = {};
		x::GetEffectSpawnPosition(a1 + 0x30, 0x1B, 0, P(loc));
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x138) >= 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 3);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CD510 (sub_8CD510) / 0x8CDF30 (sub_8CDF30): dust / smoke state 0 - at the phase data's
	// position (+0x1C / +0x20, height +0x1E = 0), next state
	uint32_t __cdecl dm_8CD510(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		U32(a1, 0x1C) = U32(d, 0);
		U32(a1, 0x20) = U32(d, 4);
		U16(a1, 0x1E) = 0;
		next_state(a1);
		return 0; // void
	}
	uint32_t __cdecl dm_8CDF30(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		U32(a1, 0x1C) = U32(d, 0);
		U32(a1, 0x20) = U32(d, 4);
		U16(a1, 0x1E) = 0;
		next_state(a1);
		return 0; // void
	}

	// 0x8CD540 (sub_8CD540): dust state 1 - height +0x1E = rising height + 0x100 (at most 0); every
	// other tick spawns a dust task 0x8CD670 (0xB0 B, queue 0x2758968) with 4 sparkles: position =
	// this one moved by (sin, rand & 0xFF - 0x80, cos) of a random angle times a random radius
	// 0x100..0x1FF, velocity 3 x (rand & 0x3FF) - 0x200; after tick 0x28 finished, next state
	uint32_t __cdecl dm_8CD540(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t h = (uint16_t)(MEM<uint32_t>(0x2758564) + 0x100);
		U16(node, 0x1E) = h;
		if ((int16_t)h >= 0)
			U16(node, 0x1E) = 0;
		if ((U8(node, 0x24) & 1) == 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2758968, 0x8CD670, 0xB0, node);
			uint32_t p = t + 0x6C;
			for (int32_t n = 4; n != 0; n--)
			{
				const int32_t r1 = (int32_t)x::CrtRand();
				const int32_t radius = (int16_t)((r1 & 0xFF) + 0x100);
				const int32_t r2 = (int32_t)x::CrtRand();
				U32(p, 0) = U32(node, 0x1C);
				const int32_t angle = (int16_t)(r2 & 0xFFF);
				U32(p, 4) = U32(node, 0x20);
				const int32_t s = mul32((int32_t)x::computeSin((uint32_t)angle), radius);
				U16(p, 0) = (uint16_t)(U16(p, 0) + (uint16_t)(s / 4096));
				const int32_t r3 = (int32_t)x::CrtRand();
				U16(p, 2) = (uint16_t)(U16(p, 2) + (uint16_t)((r3 & 0xFF) - 0x80));
				const int32_t c = mul32((int32_t)x::computeCosine((uint32_t)angle), radius);
				U16(p, 4) = (uint16_t)(U16(p, 4) + (uint16_t)(c / 4096));
				const int32_t r4 = (int32_t)x::CrtRand();
				U16(p, 0x20) = (uint16_t)((r4 & 0x3FF) - 0x200);
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(p, 0x22) = (uint16_t)((r5 & 0x3FF) - 0x200);
				const int32_t r6 = (int32_t)x::CrtRand();
				p += 8;
				U16(p, 0x1C) = (uint16_t)((r6 & 0x3FF) - 0x200);
			}
		}
		if (S16(node, 0x24) >= 0x28)
		{
			U8(node, 0x26) |= 1;
			next_state(node);
		}
		return 0; // void
	}

	// 0x8CDF60 (sub_8CDF60): smoke state 1 - height +0x1E = rising height + 0x100; every other tick
	// spawns a smoke puff 0x8CDFB0 (0x58 B, queue 0x275ABE0) at its position; after tick 0x23
	// finished, next state
	uint32_t __cdecl dm_8CDF60(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(MEM<uint32_t>(0x2758564) + 0x100);
		if ((U8(node, 0x24) & 1) == 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x275ABE0, 0x8CDFB0, 0x58, node);
			U32(t, 0x1C) = U32(node, 0x1C);
			U32(t, 0x20) = U32(node, 0x20);
		}
		if (S16(node, 0x24) >= 0x23)
		{
			U8(node, 0x26) |= 1;
			next_state(node);
		}
		return 0; // void
	}

	// 0x8CDFB0 (sub_8CDFB0): SMOKE PUFF task (node 0x58) - state {0x8CE130 init, 0x8CE1B0 fade in,
	// 0x8CE1E0 wait, 0x8CE200 fade out, ret}, spin +0x46 += 0x80, drawn by 0x8CE040
	uint32_t __cdecl dm_8CDFB0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[5] = { 0x8CE130, 0x8CE1B0, 0x8CE1E0, 0x8CE200, 0x8CE230 };
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x46) = (uint16_t)((U16(node, 0x46) + 0x80) & 0xFFF);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(held_note_puff(node);)
		dm_8CE040(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CE130 (sub_8CE130): smoke puff state 0 - prim model 0x16350F0, fade +0x50 = 0x1000, random
	// scale 0xC00 + rand % 0x3FF (x3), spin +0x46 = rand & 0xFFF, tilts +0x44 / +0x48 = rand % 0x300
	// - 0x180, next state
	uint32_t __cdecl dm_8CE130(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = 0x16350F0;
		const int32_t r1 = (int32_t)x::CrtRand();
		U16(node, 0x50) = 0x1000;
		const int32_t sc = r1 % 0x3FF + 0xC00;
		U32(node, 0x38) = (uint32_t)sc;
		U32(node, 0x34) = (uint32_t)sc;
		U32(node, 0x30) = (uint32_t)sc;
		const int32_t r2 = (int32_t)x::CrtRand();
		U16(node, 0x46) = (uint16_t)(r2 & 0xFFF);
		const int32_t r3 = (int32_t)x::CrtRand();
		U16(node, 0x44) = (uint16_t)(r3 % 0x300 - 0x180);
		const int32_t r4 = (int32_t)x::CrtRand();
		U16(node, 0x48) = (uint16_t)(r4 % 0x300 - 0x180);
		next_state(node);
		return 0; // void
	}

	// 0x8CE1B0 (sub_8CE1B0): smoke puff state 1 - fade +0x50 -= 0x400; at 0 or below: 0, timer
	// +0x54 = 1, next state
	uint32_t __cdecl dm_8CE1B0(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFC00);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			U16(a1, 0x54) = 1;
			next_state(a1);
		}
		return 0; // void
	}

	// ---- generated (gendesc.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8C4CC0, (void *)a_73A0D0, "008 MAG_008_sub_8C4CC0" },
		{ 0x8C4CD0, (void *)a_73A0D0, "008 MAG_008_sub_8C4CD0" },
		{ 0x8C4D60, (void *)a_73A170, "008 MAG_008_sub_8C4D60" },
		{ 0x8C4D90, (void *)a_73A1A0, "008 MAG_008_sub_8C4D90" },
		{ 0x8C4F60, (void *)a_73A380, "008 MAG_008_sub_8C4F60" },
		{ 0x8C4FC0, (void *)a_73A3E0, "008 sub_8C4FC0" },
		{ 0x8C5160, (void *)a_73A580, "008 sub_8C5160" },
		{ 0x8C5190, (void *)a_73A5B0, "008 sub_8C5190" },
		{ 0x8C51C0, (void *)a_73A5E0, "008 sub_8C51C0" },
		{ 0x8C51D0, (void *)a_73A5E0, "008 sub_8C51D0" },
		{ 0x8C51E0, (void *)a_73A5E0, "008 sub_8C51E0" },
		{ 0x8C51F0, (void *)a_73A5E0, "008 sub_8C51F0" },
		{ 0x8C5200, (void *)a_73A620, "008 sub_8C5200" },
		{ 0x8C5240, (void *)a_73A660, "008 sub_8C5240" },
		{ 0x8C52B0, (void *)a_73A6D0, "008 nullsub_1834" },
		{ 0x8C5490, (void *)a_73B790, "008 sub_8C5490" },
		{ 0x8C54B0, (void *)a_73B640, "008 sub_8C54B0" },
		{ 0x8C5510, (void *)a_73B7E0, "008 sub_8C5510" },
		{ 0x8C5530, (void *)a_73A930, "008 sub_8C5530" },
		{ 0x8C5550, (void *)a_73A950, "008 sub_8C5550" },
		{ 0x8C5590, (void *)a_73B8A0, "008 sub_8C5590" },
		{ 0x8C55B0, (void *)a_733AC0, "008 sub_8C55B0" },
		{ 0x8C55D0, (void *)a_733B70, "008 sub_8C55D0" },
		{ 0x8C55F0, (void *)a_733760, "008 sub_8C55F0" },
		{ 0x8C5630, (void *)a_73B620, "008 sub_8C5630" },
		{ 0x8C5680, (void *)a_73BC40, "008 sub_8C5680" },
		{ 0x8C56D0, (void *)a_7475A0, "008 sub_8C56D0" },
		{ 0x8C56F0, (void *)a_73A6D0, "008 nullsub_1835" },
		{ 0x8C5700, (void *)a_73A6D0, "008 nullsub_1836" },
		{ 0x8C5810, (void *)a_73C100, "008 sub_8C5810" },
		{ 0x8C58B0, (void *)a_73A380, "008 sub_8C58B0" },
		{ 0x8C59E0, (void *)a_73C280, "008 sub_8C59E0" },
		{ 0x8C6E90, (void *)a_73D770, "008 sub_8C6E90" },
		{ 0x8C70E0, (void *)a_73D9C0, "008 sub_8C70E0" },
		{ 0x8C8050, (void *)a_73F110, "008 sub_8C8050" },
		{ 0x8C8790, (void *)a_73A380, "008 sub_8C8790" },
		{ 0x8C8860, (void *)a_73A6D0, "008 nullsub_1837" },
		{ 0x8C88B0, (void *)a_73A6D0, "008 nullsub_1838" },
		{ 0x8C88C0, (void *)a_73A6D0, "008 nullsub_1839" },
		{ 0x8C88D0, (void *)a_73A380, "008 sub_8C88D0" },
		{ 0x8C8960, (void *)a_73F990, "008 sub_8C8960" },
		{ 0x8C8BF0, (void *)a_73FC20, "008 sub_8C8BF0" },
		{ 0x8C8E60, (void *)a_73FE90, "008 sub_8C8E60" },
		{ 0x8C8F80, (void *)a_73FFB0, "008 sub_8C8F80" },
		{ 0x8C91E0, (void *)a_740210, "008 sub_8C91E0" },
		{ 0x8C98A0, (void *)a_740910, "008 sub_8C98A0" },
		{ 0x8C9F00, (void *)a_740F70, "008 sub_8C9F00" },
		{ 0x8C9F50, (void *)a_740FC0, "008 sub_8C9F50" },
		{ 0x8C9FE0, (void *)a_741050, "008 sub_8C9FE0" },
		{ 0x8CA0B0, (void *)a_741120, "008 sub_8CA0B0" },
		{ 0x8CAAF0, (void *)a_741B60, "008 sub_8CAAF0" },
		{ 0x8CB220, (void *)a_742290, "008 au_re__rand_69" },
		{ 0x8CB250, (void *)a_7422C0, "008 sub_8CB250" },
		{ 0x8CB290, (void *)a_742300, "008 sub_8CB290" },
		{ 0x8CB2E0, (void *)a_742350, "008 sub_8CB2E0" },
		{ 0x8CB330, (void *)a_7423A0, "008 au_re__rand_69_0" },
		{ 0x8CB440, (void *)a_7424B0, "008 sub_8CB440" },
		{ 0x8CB450, (void *)a_7424C0, "008 sub_8CB450" },
		{ 0x8CBC30, (void *)a_742CA0, "008 sub_8CBC30" },
		{ 0x8CBC60, (void *)a_742CD0, "008 sub_8CBC60" },
		{ 0x8CBC90, (void *)a_742D00, "008 sub_8CBC90" },
		{ 0x8CBE40, (void *)a_742EB0, "008 sub_8CBE40" },
		{ 0x8CBF80, (void *)a_742FF0, "008 sub_8CBF80" },
		{ 0x8CC1D0, (void *)a_73A6D0, "008 nullsub_1840" },
		{ 0x8CC1E0, (void *)a_73A380, "008 sub_8CC1E0" },
		{ 0x8CC240, (void *)a_7335D0, "008 sub_8CC240" },
		{ 0x8CC2C0, (void *)a_73A6D0, "008 nullsub_1841" },
		{ 0x8CC2D0, (void *)a_73A380, "008 sub_8CC2D0" },
		{ 0x8CC390, (void *)a_73A6D0, "008 nullsub_1842" },
		{ 0x8CC4D0, (void *)a_746C10, "008 sub_8CC4D0" },
		{ 0x8CD150, (void *)a_73BB60, "008 sub_8CD150" },
		{ 0x8CD330, (void *)a_733950, "008 sub_8CD330" },
		{ 0x8CD4A0, (void *)a_73A6D0, "008 nullsub_1843" },
		{ 0x8CD4B0, (void *)a_73A380, "008 sub_8CD4B0" },
		{ 0x8CD890, (void *)a_7435E0, "008 InitEffectSequenceFromData_c18" },
		{ 0x8CD9D0, (void *)a_743720, "008 sub_8CD9D0" },
		{ 0x8CDE80, (void *)a_743C20, "008 sub_8CDE80" },
		{ 0x8CDEB0, (void *)a_73A6D0, "008 nullsub_1844" },
		{ 0x8CDEC0, (void *)a_73A6D0, "008 nullsub_1845" },
		{ 0x8CDED0, (void *)a_73A380, "008 sub_8CDED0" },
		{ 0x8CE230, (void *)a_73A6D0, "008 nullsub_1846" },
		{ 0x8CE240, (void *)a_73A6D0, "008 nullsub_1847" },
		{ 0x8CE290, (void *)a_7474B0, "008 MAG_008_sub_8CE290" },
		{ 0x8CE2B0, (void *)a_7474D0, "008 MAG_008_sub_8CE2B0" },
		{ 0x8CE2D0, (void *)a_73A6D0, "008 nullsub_1848" },
		{ 0x8CE2E0, (void *)a_747500, "008 MAG_008_sub_8CE2E0" },
		{ 0x8CE320, (void *)a_73A0D0, "008 MAG_008_sub_8CE320" },
		{ 0x8CE330, (void *)a_747550, "008 MAG_008_sub_8CE330" },
		{ 0x8CE340, (void *)a_747560, "008 MAG_008_sub_8CE340" },
		{ 0x8CE370, (void *)a_747590, "008 MAG_008_sub_8CE370" },
		{ 0x8CE380, (void *)a_7475A0, "008 MAG_008_sub_8CE380" },
		{ 0x8CE3A0, (void *)a_73A6D0, "008 nullsub_1833" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x8C4B30, (void *)dm_8C4B30, "008 dm_8C4B30" },
		{ 0x8C4CE0, (void *)dm_8C4CE0, "008 dm_8C4CE0" },
		{ 0x8C4D20, (void *)dm_8C4D20, "008 dm_8C4D20" },
		{ 0x8C4E10, (void *)dm_8C4E10, "008 dm_8C4E10" },
		{ 0x8C4E60, (void *)dm_8C4E60, "008 dm_8C4E60" },
		{ 0x8C52C0, (void *)dm_8C52C0, "008 dm_8C52C0" },
		{ 0x8C5350, (void *)dm_8C5350, "008 dm_8C5350" },
		{ 0x8C5410, (void *)dm_8C5410, "008 dm_8C5410" },
		{ 0x8C5460, (void *)dm_8C5460, "008 dm_8C5460" },
		{ 0x8C54E0, (void *)dm_8C54E0, "008 dm_8C54E0" },
		{ 0x8C5610, (void *)dm_8C5610, "008 dm_8C5610" },
		{ 0x8C5650, (void *)dm_8C5650, "008 dm_8C5650" },
		{ 0x8C56A0, (void *)dm_8C56A0, "008 dm_8C56A0" },
		{ 0x8C5710, (void *)dm_8C5710, "008 dm_8C5710" },
		{ 0x8C5860, (void *)dm_8C5860, "008 dm_8C5860" },
		{ 0x8C5910, (void *)dm_8C5910, "008 dm_8C5910" },
		{ 0x8C5B10, (void *)dm_8C5B10, "008 dm_8C5B10" },
		{ 0x8C5DD0, (void *)dm_8C5DD0, "008 dm_8C5DD0" },
		{ 0x8C5F00, (void *)dm_8C5F00, "008 dm_8C5F00" },
		{ 0x8C6120, (void *)dm_8C6120, "008 dm_8C6120" },
		{ 0x8C63B0, (void *)dm_8C63B0, "008 dm_8C63B0" },
		{ 0x8C68A0, (void *)dm_8C68A0, "008 dm_8C68A0" },
		{ 0x8C73C0, (void *)dm_8C73C0, "008 dm_8C73C0" },
		{ 0x8C7920, (void *)dm_8C7920, "008 dm_8C7920" },
		{ 0x8C7FF0, (void *)dm_8C7FF0, "008 dm_8C7FF0" },
		{ 0x8C80C0, (void *)dm_8C80C0, "008 dm_8C80C0" },
		{ 0x8C8120, (void *)dm_8C8120, "008 dm_8C8120" },
		{ 0x8C8350, (void *)dm_8C8350, "008 dm_8C8350" },
		{ 0x8C8400, (void *)dm_8C8400, "008 dm_8C8400" },
		{ 0x8C84A0, (void *)dm_8C84A0, "008 dm_8C84A0" },
		{ 0x8C8500, (void *)dm_8C8500, "008 dm_8C8500" },
		{ 0x8C87F0, (void *)dm_8C87F0, "008 dm_8C87F0" },
		{ 0x8C8840, (void *)dm_8C8840, "008 dm_8C8840" },
		{ 0x8C8870, (void *)dm_8C8870, "008 dm_8C8870" },
		{ 0x8C8890, (void *)dm_8C8890, "008 dm_8C8890" },
		{ 0x8C8930, (void *)dm_8C8930, "008 dm_8C8930" },
		{ 0x8C8B10, (void *)dm_8C8B10, "008 dm_8C8B10" },
		{ 0x8C8D90, (void *)dm_8C8D90, "008 dm_8C8D90" },
		{ 0x8C9440, (void *)dm_8C9440, "008 dm_8C9440" },
		{ 0x8C94D0, (void *)dm_8C94D0, "008 dm_8C94D0" },
		{ 0x8C9530, (void *)dm_8C9530, "008 dm_8C9530" },
		{ 0x8C9690, (void *)dm_8C9690, "008 dm_8C9690" },
		{ 0x8C9750, (void *)dm_8C9750, "008 dm_8C9750" },
		{ 0x8C97D0, (void *)dm_8C97D0, "008 dm_8C97D0" },
		{ 0x8C9810, (void *)dm_8C9810, "008 dm_8C9810" },
		{ 0x8CA840, (void *)dm_8CA840, "008 dm_8CA840" },
		{ 0x8CA950, (void *)dm_8CA950, "008 dm_8CA950" },
		{ 0x8CA9D0, (void *)dm_8CA9D0, "008 dm_8CA9D0" },
		{ 0x8CAA00, (void *)dm_8CAA00, "008 dm_8CAA00" },
		{ 0x8CAAA0, (void *)dm_8CAAA0, "008 dm_8CAAA0" },
		{ 0x8CAB20, (void *)dm_8CAB20, "008 dm_8CAB20" },
		{ 0x8CABA0, (void *)dm_8CABA0, "008 dm_8CABA0" },
		{ 0x8CB380, (void *)dm_8CB380, "008 dm_8CB380" },
		{ 0x8CB5A0, (void *)dm_8CB5A0, "008 dm_8CB5A0" },
		{ 0x8CC090, (void *)dm_8CC090, "008 dm_8CC090" },
		{ 0x8CC120, (void *)dm_8CC120, "008 dm_8CC120" },
		{ 0x8CC1B0, (void *)dm_8CC1B0, "008 dm_8CC1B0" },
		{ 0x8CC280, (void *)dm_8CC280, "008 dm_8CC280" },
		{ 0x8CC330, (void *)dm_8CC330, "008 dm_8CC330" },
		{ 0x8CC350, (void *)dm_8CC350, "008 dm_8CC350" },
		{ 0x8CC3A0, (void *)dm_8CC3A0, "008 dm_8CC3A0" },
		{ 0x8CC450, (void *)dm_8CC450, "008 dm_8CC450" },
		{ 0x8CC6B0, (void *)dm_8CC6B0, "008 dm_8CC6B0" },
		{ 0x8CC7B0, (void *)dm_8CC7B0, "008 dm_8CC7B0" },
		{ 0x8CCF80, (void *)dm_8CCF80, "008 dm_8CCF80" },
		{ 0x8CCFE0, (void *)dm_8CCFE0, "008 dm_8CCFE0" },
		{ 0x8CD020, (void *)dm_8CD020, "008 dm_8CD020" },
		{ 0x8CD090, (void *)dm_8CD090, "008 dm_8CD090" },
		{ 0x8CD0E0, (void *)dm_8CD0E0, "008 dm_8CD0E0" },
		{ 0x8CD120, (void *)dm_8CD120, "008 dm_8CD120" },
		{ 0x8CD1A0, (void *)dm_8CD1A0, "008 dm_8CD1A0" },
		{ 0x8CD1D0, (void *)dm_8CD1D0, "008 dm_8CD1D0" },
		{ 0x8CD200, (void *)dm_8CD200, "008 dm_8CD200" },
		{ 0x8CD250, (void *)dm_8CD250, "008 dm_8CD250" },
		{ 0x8CD290, (void *)dm_8CD290, "008 dm_8CD290" },
		{ 0x8CD2B0, (void *)dm_8CD2B0, "008 dm_8CD2B0" },
		{ 0x8CD310, (void *)dm_8CD310, "008 dm_8CD310" },
		{ 0x8CD430, (void *)dm_8CD430, "008 dm_8CD430" },
		{ 0x8CD510, (void *)dm_8CD510, "008 dm_8CD510" },
		{ 0x8CD540, (void *)dm_8CD540, "008 dm_8CD540" },
		{ 0x8CD670, (void *)dm_8CD670, "008 dm_8CD670" },
		{ 0x8CD770, (void *)dm_8CD770, "008 dm_8CD770" },
		{ 0x8CDE40, (void *)dm_8CDE40, "008 dm_8CDE40" },
		{ 0x8CDE60, (void *)dm_8CDE60, "008 dm_8CDE60" },
		{ 0x8CDF30, (void *)dm_8CDF30, "008 dm_8CDF30" },
		{ 0x8CDF60, (void *)dm_8CDF60, "008 dm_8CDF60" },
		{ 0x8CDFB0, (void *)dm_8CDFB0, "008 dm_8CDFB0" },
		{ 0x8CE040, (void *)dm_8CE040, "008 dm_8CE040" },
		{ 0x8CE130, (void *)dm_8CE130, "008 dm_8CE130" },
		{ 0x8CE1B0, (void *)dm_8CE1B0, "008 dm_8CE1B0" },
		{ 0x8CE1E0, (void *)dm_8CE1E0, "008 dm_8CE1E0" },
		{ 0x8CE200, (void *)dm_8CE200, "008 dm_8CE200" },
		{ 0x8CE250, (void *)dm_8CE250, "008 dm_8CE250" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag008_doom()
	{
		act::register_module(8);
		for (const act::doom::ModPort *p = act::doom::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(8, p->addr, p->port, p->name);
		for (const act::doom::ModPort *p = act::doom::PORTS; p->addr; p++)
			act::register_module_port(8, p->addr, p->port, p->name);
		// 30 fps layer: see mag008_doom_held.inc
		FX_HELD(register_mag008_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag008_doom_held.inc"
#endif
