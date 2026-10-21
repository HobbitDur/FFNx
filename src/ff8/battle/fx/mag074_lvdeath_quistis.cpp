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

// Effect 74: LV?Death (Quistis' Blue Magic, kernel.bin section 19 "LV?Death", magic +4 = 74;
// MAG_074_LVDEATH_QUISTIS*), cast by a party member (Quistis, her whip) on the enemies: a copy of the
// actor/heal effect library (see act_engine.h), built from Death 14 (mag014_death.cpp).
//
// The module 0x7C4440..0x7CBD90 is Death's module 0x8B68A0..0x8BE1D0 function for function (w74\amap_74.txt:
// code -0xF2460..-0xF2420 per function, exe data -0xA97F8 (reaper data / prim model / camera animation),
// -0xAA620 (reaper model container, spark tables), -0x90074 (actor data files, the reaper's TIMs: byte-identical
// to Death's), module globals -0x14E968 up to the scratch stack pointer 0x25F3EFC, -0x14E948 from the actor pool
// pointer 0x25F3F20 on: the bind block 0x25F3F00..0x25F3F20 is new). 112 of its 121 functions are identical to
// Death's up to addresses, 6 more differ only in their switch jump tables (addresses); the three that differ:
//   0x7C4A90 (Death 0x8B6EF0, director state 0) - the first emitter's director (+0x2E == 0) uploads the reaper's
//     two TIMs, binds the module's animation set (com data 0x15863B0, out block 0x25F3F00) to the caster
//     (BS_BindEntityModelFromComData 0x5022C0), starts its chain transformation 1 (QueueChainTransformation
//     0x505C00: Quistis' casting animation) and waits 0xC ticks (Death: 1) before the reaper; the others wait 1.
//   0x7CBCC0 (Death 0x8BE100 = a_747500, master state 4) - one emitter per TARGET (+0x2B up to targets - 1,
//     +0x5A) instead of one per action (+0x2A up to +0x58).
//   0x7C5530 (Death 0x8B7950 = a_73F990, actor director set-up) - the director's target list is the ONE target of
//     its emitter (count 1, slot = action +0x2A's target +0x2B) instead of all the action's targets.
// So every target gets its own emitter, director, reaper (placed in front of it), dark actor system, spark bursts
// and damage (0x7CBC30 = Death 0x8BE070: action +0x2A, target +0x2B); they share the reaper data block [0x1582068]
// (rewritten by each emitter's placement).
//
// Setup MAG_074_LVDEATH_QUISTIS_Init 0x7C4460 (runs once, not ported; file loader 0x7C4440 = the effect's data
// file, packet arenas at file + 0xE000 / + 0x1C000; camera animation 0x1582184 unless the context's flag +1 bit 0)
// creates the root queue 0x25EC750 with the master and seven task pools: 0x25F2F10 (4 x 0x58 emitters),
// 0x25EC658 (3 x 0x48 directors), 0x25F3EA8 (3 x 0x140 creature), 0x25F2C70 (4 x 0x2A4 actor systems), 0x25EC760
// (10 x 0x70 stage wobbles), 0x25ECBD8 (0x32 x 0x118 dark sparks), 0x25F2F00 (10 x 0x40, unused).
//   Master (0x7C45E0) - camera copy 0x2793E58 (the module scratch stack 0x25F3EFC grows down from it), packet
//     arena by tick parity (cursor 0x25EC51C), bone follow, 11-state table: 0x7C4780 carves the actor pools after
//     the arena, 0x7C4800 one emitter per target (loop 0x7CBCC0), then waits, restores the battle textures
//     (0x508630) and ends.
//   Emitter (0x7C4830, engine task a_73A1A0): 0x7C48B0 places the reaper in front of the target (0x7C48E0, data
//     block [0x1582068]) and starts the director; 0x7CBC30 applies the damage once the reaper struck (data +0x48),
//     0x7CBC70 waits for the director / reaper's end (+0x4C).
//   Director (0x7C49D0): state 0 0x7C4A90 (above), then after the wait spawns the reaper creature (0x7C4B50, model
//     container 0x158233C, animation 3).
//   Reaper (0x7C4B50): state machine {appear 0x7C5360, 0x7CB2D0, strike 0x7CB320, vanish 0x7CBA50, 0x7CBAB0};
//     drawn each tick (0x7C4C10) either as the engine's creature (a_746C10) or, while it fades in (+0x26 bit 3),
//     with the module's own dark renderer (0x7C4E70 / 0x7C4F70: every vertex lit by the bone matrices through
//     the GTE light matrix, triangles / quads above the ground as flat-textured packets in the entity colour).
//     At the strike it spawns the dark actor system (0x7C54A0, data 0x1776950) and the dark spark bursts
//     (0x7CB400 -> 0x7CB790: 4 sparks each, decelerating, drawn as prim models through 0x7C6240); at its
//     appearance (first target only: +0x2E == 0) the aura actor system (data 0x1771F5C) and the stage wobble in
//     (0x7CB140), at the last target's end the stage wobble out (0x7CBB00).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Entity writes: the caster's animation set and chain transformation (director 0x7C4A90, through the engine); the
// effect light block 0x1D99AB0 (reaper); the stage wobble words 0x1D98992 + k * 0x2C (stage wobble tasks).
// Camera: the setup's camera animation 0x1582184 (engine camera script); the module only reads the camera
// (copy 0x2793E58 per tick, 0x1D97778 in its draws).
// Module globals: 0x25EC518..0x25F3F58 (file pointer 0x25EC518, packet cursor 0x25EC51C, pools, queues, arenas,
// actor state 0x25F3F2C, scratch stack pointer 0x25F3EFC, the bind block 0x25F3F00, reaper render context
// 0x25EC528 / 0x25F3F30 and its vertex buffer 0x25ECBE8), the module scratch stack below the camera copy
// 0x2793E58; the pointers 0x1582068 / 0x1586388 (exe data) lead to the reaper data 0x25F3A90 / 0x25F3ED0. The
// reaper model container (0x158233C..) and the actor data files (0x1771F5C.., 0x1776950.., 0x1778BFC..) in exe
// data are rewritten in place (skeleton, relocation, fade).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace lvdeath_quistis
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_074 = { "lvdeath_quistis", 74, 0x7C4440, 0x7CBD90,
		{ 0x0, 0x0, 0x1582068, 0x0, 0x0, 0x25EC67C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25EC518, 0x0, 0x0, 0x0, 0x25EC680, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25F2C70, 0x0, 0x25F2F10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25F3EB8, 0x0, 0x0, 0x25F3EF4, 0x0, 0x25F3EFC, 0x0, 0x25F3F2C },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7C5530, 0x7C5FF0, 0x7C6080, 0x7C60E0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7C8460, 0x7C85A0, 0x7C85E0, 0x7C9870, 0x7C98C0, 0x7C98F0, 0x7C9970, 0x7CA060, 0x7CA0B0, 0x7CA150, 0x7CA210, 0x7CAA60, 0x7CAE60, 0x7CAEF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7C4830, 0x7C48B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7C56C0, 0x7C57A0, 0x7C5940, 0x7C5A10, 0x7C5B30, 0x7C5D90, 0x7C8520, 0x7C8670, 0x7C8CD0, 0x7C8D20, 0x7C8DB0, 0x7C8E80, 0x7C9610, 0x7C9720, 0x7C97A0, 0x7C97D0, 0x7C9FF0, 0x7CA020, 0x7CA100, 0x7CA220, 0x7CA370, 0x0, 0x7CAA30, 0x7CAC10, 0x7CAD50, 0x0, 0x7C5FF0, 0x7CBC30, 0x7CBC70, 0x7CBC90, 0x7CBCB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x25EC51C;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x25F3F2C;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x25F3EFC;      // module scratch stack pointer
	static const uint32_t REAPER_DATA = 0x1582068;     // -> the reaper's placement / phase block
	static const uint32_t ORIG_Master = 0x7C45E0;
	static const uint32_t ORIG_Reaper = 0x7C4B50;
	static const uint32_t ORIG_ActorSystem = 0x7C54A0;
	static const uint32_t ORIG_Spark = 0x7CB790;
	static const uint32_t REAPER_CTX = 0x25EC528;      // render context of the dark reaper draw
	static const uint32_t BIND_Block = 0x25F3F00;      // 0x20-byte block BS_BindEntityModelFromComData fills
	static const uint32_t COM_Anim = 0x15863B0;        // the module's animation set (com data)

	// engine functions not in act::x
	namespace dx
	{
		static inline uint32_t BS_BindEntityModelFromComData(uint32_t entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
		static inline uint32_t QueueChainTransformation(uint32_t entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(entity, id); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2E0)(a1); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3_2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E320)(a1); }
	}
	using namespace dx;

	// helpers of the library code (as in act_engine.cpp / mag090_tonberry.cpp / mag097_boko.cpp)
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
	// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}
	// screen-space clip tests of the flat-triangle list (x 0..0xA00, y 0..0x6C0)
	static inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	// GP0 0xE2 texture-window word from a RECT {x,y,w,h} (u16 each) at ctx+b
	// (0x764E3D-0x764E7C / 0x764EDF-0x764F1E; 0x76540C-0x76544A / 0x7654B3-0x7654F1)
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
	// (0x765E84 / 0x765F39 / 0x766527 / 0x7665EC): offsets = low byte & 0xF8, masks from
	// ~(w-1) / ~(h-1); 0xFFFE2000 << 12 = 0xE2000000
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
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

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

// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl d_7C47C0(void);
	uint32_t __cdecl d_7C4780(uint32_t a1);
	uint32_t __cdecl d_7C48E0(uint32_t a1);
	uint32_t __cdecl d_7C48B0(uint32_t a1);
	uint32_t __cdecl d_7C4A40(uint32_t a1);
	uint32_t __cdecl d_7C49D0(uint32_t a1);
	uint32_t __cdecl d_7C4A90(uint32_t a1);
	uint32_t __cdecl d_7C4B10(uint32_t a1);
	uint32_t __cdecl d_7C4F70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C4E70(uint32_t a1);
	uint32_t __cdecl d_7C4C10(uint32_t a1);
	uint32_t __cdecl d_7C4B50(uint32_t a1);
	uint32_t __cdecl d_7CB2C0(uint32_t a1);
	uint32_t __cdecl d_7CB260(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7CB230(uint32_t a1);
	uint32_t __cdecl d_7C5360(uint32_t a1);
	uint32_t __cdecl d_7CB2D0(uint32_t a1);
	uint32_t __cdecl d_7CB320(uint32_t a1);
	uint32_t __cdecl d_7CBA50(uint32_t a1);
	uint32_t __cdecl d_7CBAB0(uint32_t a1);
	uint32_t __cdecl d_7CB1A0(uint32_t a1);
	uint32_t __cdecl d_7CB1E0(uint32_t a1);
	uint32_t __cdecl d_7CBB60(uint32_t a1);
	uint32_t __cdecl d_7CB460(uint32_t a1);
	uint32_t __cdecl d_7CB4A0(uint32_t a1);
	uint32_t __cdecl d_7CB8E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7CB790(uint32_t a1);
	uint32_t __cdecl d_7CB9E0(uint32_t a1);
	uint32_t __cdecl d_7CBA10(uint32_t a1);
	uint32_t __cdecl d_7C60E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7C8520(uint32_t a1);
	uint32_t __cdecl d_7C97D0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl d_7CA150(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7C45E0(uint32_t a1);
	uint32_t __cdecl d_7C5500(uint32_t a1);
	uint32_t __cdecl d_7C56C0(uint32_t a1);
	uint32_t __cdecl d_7C5940(void);
	uint32_t __cdecl d_7C5FF0(uint32_t a1);
	uint32_t __cdecl d_7C6080(void);
	uint32_t __cdecl d_7C6240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C6370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C6590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C6820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C6D10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C7830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C7D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7C8460(void);
	uint32_t __cdecl d_7C85A0(void);
	uint32_t __cdecl d_7C85E0(uint32_t a1);
	uint32_t __cdecl d_7C9610(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7C9720(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7C97A0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7C9870(uint32_t a1);
	uint32_t __cdecl d_7C98F0(uint32_t a1);
	uint32_t __cdecl d_7C9970(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl d_7CA370(uint32_t a1);
	uint32_t __cdecl d_7CAE60(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7CAEF0(void);
	uint32_t __cdecl d_7CAF80(uint32_t a1);
	uint32_t __cdecl d_7CAFB0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_7CB0D0(uint32_t a1);
	uint32_t __cdecl d_7CBB80(uint32_t a1);
	uint32_t __cdecl d_7CBC30(uint32_t a1);
	uint32_t __cdecl d_7C5530(uint32_t a1);
	uint32_t __cdecl d_7CBCC0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag074_lvdeath_quistis_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace lvdeath_quistis
{
	// ====================================================================================
	// master states, emitter, director, reaper (module code)
	// ====================================================================================

	// 0x7C47C0 (MAG_074_sub_7C47C0): clears the two actor pools (0x870 bytes at [0x25F3EBC], 0x9D80
	// at [0x25F3F20]) and the pool cursors / counters
	uint32_t __cdecl d_7C47C0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25F3EBC), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25F3F20), 0x9D80);
		MEM<uint16_t>(0x25ECBD0) = 0;
		MEM<uint16_t>(0x25F3EC8) = 0;
		MEM<uint16_t>(0x25F3A88) = 0;
		MEM<uint16_t>(0x25EC520) = 0;
		return 0; // void
	}

	// 0x7C4780 (MAG_074_sub_7C4780): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x25F3F28] (0x870 + 0x9D80 bytes) and cleared, next state
	uint32_t __cdecl d_7C4780(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25F3F28);
		MEM<uint32_t>(0x25F3EBC) = p;
		p += 0x870;
		MEM<uint32_t>(0x25F3F20) = p;
		p += 0x9D80;
		MEM<uint32_t>(0x25F3F28) = p;
		d_7C47C0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7C48E0 (MAG_074_sub_7C48E0; Death 0x7C48E0): clears the reaper data block [0x1582068]
	// (0x54 bytes), +8 = the target's effect bone 0xF1; +0 / +4 = the target's position with the
	// reaper 0x200 + size * 125 / 256 in front of it (towards -z for a party target, +z for an
	// enemy), +0x4A its yaw facing the target (angle of +8 - +0 in x / z, minus 0x800)
	uint32_t __cdecl d_7C48E0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(REAPER_DATA), 0x54);
		const uint32_t slot0 = U8(a1, 0x2D);
		x::GetEffectSpawnPosition(0x1D972C0 + slot0 * 0x9C, 0xF1, 0, MEM<uint32_t>(REAPER_DATA) + 8);
		const uint8_t slot = U8(a1, 0x2D);
		const uint32_t d = MEM<uint32_t>(REAPER_DATA);
		U16(d, 0xA) = 0;
		const uint32_t ent = 0x1D972C0 + (uint32_t)slot * 0x9C;
		U32(d, 0) = U32(ent, 0x1C);
		U32(d, 4) = U32(ent, 0x20);
		U16(d, 2) = 0;
		const int32_t size125 = mul32(S16(ent, 0x26), 125);
		const int32_t off = shl32(size125, 4) >> 12;
		if (slot <= 2)
			U16(d, 4) = (uint16_t)(U16(d, 4) + (uint16_t)(-0x200 - off));
		else
			U16(d, 4) = (uint16_t)(U16(d, 4) + (uint16_t)(off + 0x200));
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dxv = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t angle = x::CartesianToGameAngle((uint32_t)(int32_t)dxv, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(REAPER_DATA), 0x4A) = (uint16_t)((angle - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x7C48B0 (MAG_074_sub_7C48B0): emitter state 0 - places the reaper, spawns the director
	// 0x7C49D0 (0x48 bytes, director queue), next state
	uint32_t __cdecl d_7C48B0(uint32_t a1)
	{
		d_7C48E0(a1);
		x::Effect_AddTaskAndInitFromCtx(0x25EC658, 0x7C49D0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7C4A40 (sub_7C4A40): the reaper data's phase bookkeeping - ++ticks in phase (+0x46); a new
	// requested phase (+0x42) becomes current (+0x40, timer reset); the queued phase (+0x44) becomes
	// the requested one (the per-phase hooks of this copy are empty)
	uint32_t __cdecl d_7C4A40(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(REAPER_DATA);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			a_73A6D0(); // nullsub 0x7CBC20 (a1 pushed, unused)
			d = MEM<uint32_t>(REAPER_DATA);
		}
		const uint16_t next = U16(d, 0x44);
		if (U16(d, 0x42) != next)
		{
			U16(d, 0x42) = next;
			a_73A6D0(); // nullsub 0x7CBC10 (a1 pushed, unused)
		}
		return a1;
	}

	// 0x7C49D0 (MAG_074_sub_7C49D0): DIRECTOR task - phase bookkeeping, state {0x7C4A90 TIM uploads,
	// 0x7C4B10 spawn the reaper, finished, ret}
	uint32_t __cdecl d_7C49D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7C4A90;
		states[1] = 0x7C4B10;
		states[2] = 0x7CBBE0;
		states[3] = 0x7CBC00; // nullsub (ret)
		d_7C4A40(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7C4A90 (MAG_074_UploadReaperTIMs; Death 0x8B6EF0): director state 0 - the first emitter (+0x2E == 0)
	// uploads the reaper's two TIMs, binds the module's animation set (com data 0x15863B0, out block
	// 0x25F3F00) to the caster entity (slot +0x2C) and starts its chain transformation 1 (Quistis'
	// casting animation): wait counter +0x44 = 0xC; the others: +0x44 = 1; next state
	uint32_t __cdecl d_7C4A90(uint32_t a1)
	{
		if (U8(a1, 0x2E) == 0)
		{
			x::Battle_QueueTIMUpload_GetEOF(0x1778F3C);
			x::Battle_QueueTIMUpload_GetEOF(0x178135C);
			const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
			BS_BindEntityModelFromComData(ent, BIND_Block, COM_Anim);
			QueueChainTransformation(ent, 1);
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0xC;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x44) = 1;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7C4B10 (MAG_074_SpawnReaperCreature; Death 0x7C4B10): director state 1 - after the wait, the reaper creature
	// 0x7C4B50 (0x140 bytes, creature queue) bound to the model container 0x158233C, animation 3
	uint32_t __cdecl d_7C4B10(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) > 0)
			return 0; // void
		const uint32_t n = x::Effect_AddTaskAndInitFromCtx(0x25F3EA8, ORIG_Reaper, 0x140, a1);
		x::Effect_BindModelContainerSetAnim(n, 0x158233C, 3);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7C4F70 (sub_7C4F70): the module's dark model renderer (model a1, OT a2; a3 unused; render
	// context a4: +0x20 camera * model matrix, +0x44 -> {+4 vertex buffer}, +0x48 the model block
	// (entity layout: +0x28 colour, +0x7C object mask), +0x4C -> packet cursor). Per visible
	// object: its vertices through their bone matrices as the GTE light matrix / background vector
	// (world space) into the vertex buffer; then its textured triangles and quads whose vertices are
	// all at or above the ground (y <= 0), front-facing, as flat-textured packets (colour = entity
	// colour, codes 0x24 / 0x2C), OT z = OTZ >> 2. Header (0x5C bytes, Field_Alloc): +0x28 vertex /
	// IR scratch, +0x38..+0x44 vertex indices, +0x48 / +0x4C packet words, +0x50 object mask, +0x54
	// NCLIP, +0x58 OTZ.
	uint32_t __cdecl d_7C4F70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t ctx = a4;
		const uint32_t blk = U32(ctx, 0x48);
		uint32_t cursor = MEM<uint32_t>(U32(ctx, 0x4C));        // [esp+0x14]
		const uint32_t vbuf = U32(U32(ctx, 0x44), 4);            // [esp+0x10]
		uint32_t table = U32(a1, 4);
		const uint32_t bones = U32(a1, 0) + 0x10;                // [esp+0x28]
		const int32_t count = S32(table, 0);                     // [esp+0x24]
		const uint32_t hdr = x::Field_Alloc(0x5C);
		const uint32_t mask = U32(blk, 0x7C);
		const uint32_t colour = U32(blk, 0x28) & 0xFFFFFF;
		table += 4;
		U32(hdr, 0x50) = mask;
		U32(hdr, 0x4C) = colour | 0x2C000000;
		U32(hdr, 0x48) = colour | 0x24000000;
		x::GTE_SetRotMatrix(ctx + 0x20);
		x::GTE_SetTransVector(ctx + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t v = vbuf;
			table += 4;
			uint32_t obj = U32(a1, 4) + U32(table, -4);
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
			int32_t n4 = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			uint32_t pkt = cursor;
			for (int32_t k = n3; k > 0; k--, rec += 0x10)
			{
				const uint32_t i0 = U16(rec, 0) & 0xFFF, i1 = U16(rec, 2) & 0xFFF, i2 = U16(rec, 4) & 0xFFF;
				U32(hdr, 0x38) = i0;
				U32(hdr, 0x3C) = i1;
				U32(hdr, 0x40) = i2;
				if (S16(vbuf + i0 * 8, 2) > 0 || S16(vbuf + i1 * 8, 2) > 0 || S16(vbuf + i2 * 8, 2) > 0)
					continue;
				x::GTE_LoadV012(vbuf + i0 * 8, vbuf + i1 * 8, vbuf + i2 * 8);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(hdr + 0x54);
				if (S32(hdr, 0x54) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(hdr + 0x58);
				U32(hdr, 0x58) = U32(hdr, 0x58) >> 2;
				GTE_StoreSXY012_PolyFT3(pkt);
				U32(pkt, 0xC) = U32(rec, 8);
				U32(pkt, 0x14) = U32(rec, 0xC);
				U16(pkt, 0x1C) = U16(rec, 6);
				U32(pkt, 4) = U32(hdr, 0x48);
				U32(pkt, 0) = 0x7000000;
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(hdr, 0x58) * 4, pkt);
				pkt += 0x20;
			}
			if (n4 > 0)
			{
				rec += 4;
				for (; n4 > 0; n4--, rec += 0x14)
				{
					const uint32_t i0 = U16(rec, -4) & 0xFFF, i1 = U16(rec, -2) & 0xFFF, i2 = U16(rec, 0) & 0xFFF;
					U32(hdr, 0x38) = i0;
					U32(hdr, 0x3C) = i1;
					U32(hdr, 0x40) = i2;
					if (S16(vbuf + i0 * 8, 2) > 0 || S16(vbuf + i1 * 8, 2) > 0 || S16(vbuf + i2 * 8, 2) > 0)
						continue;
					// quirk 0x7C523D: tests the 4th vertex of the PREVIOUS quad (+0x44 is set below;
					// for the first quad it is what the header scratch held)
					if (S16(vbuf + U32(hdr, 0x44) * 8, 2) > 0)
						continue;
					x::GTE_LoadV012(vbuf + i0 * 8, vbuf + i1 * 8, vbuf + i2 * 8);
					x::GTE_RTPT();
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(hdr + 0x54);
					if (S32(hdr, 0x54) <= 0)
						continue;
					GTE_StoreSXY012_PolyFT3_2(pkt);
					const uint32_t i3 = U16(rec, 2) & 0xFFF;
					U32(hdr, 0x44) = i3;
					x::GTE_LoadV0(vbuf + i3 * 8);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(hdr + 0x58);
					const uint32_t z = U32(hdr, 0x58) >> 2;
					U32(hdr, 0x58) = z;
					U32(pkt, 0xC) = U32(rec, 4);
					U16(pkt, 0x1C) = U16(rec, 0xC);
					U16(pkt, 0x24) = U16(rec, 0xE);
					U32(pkt, 0x14) = U32(rec, 8);
					U32(pkt, 0) = 0x9000000;
					U32(pkt, 4) = U32(hdr, 0x4C);
					x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, pkt);
					pkt += 0x28;
				}
			}
			cursor = pkt;
		}
		const uint32_t out = U32(ctx, 0x4C);
		x::Field_Free(0x5C);
		MEM<uint32_t>(out) = cursor;
		return out;
	}

	// 0x7C4E70 (sub_7C4E70): the dark reaper draw of render context a1 (0x25EC528): matrix = the
	// model block's (+0x48 -> +0x40) rotation through the camera, its translation through the camera
	// (MAC -> +0x34); the block's shadow (0x5088A0), both models (+0x64 and the second +0x78) through
	// 0x7C4F70, then the bone matrices back from the pose
	uint32_t __cdecl d_7C4E70(uint32_t a1)
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
		d_7C4F70(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t second = U32(blk, 0x78);
		if (second != 0)
			d_7C4F70(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		return 0; // void
	}

	// 0x7C4C10 (sub_7C4C10): the reaper's draw (hidden: +0x26 bit 2) - dark (bit 3): matrix of the
	// rotation +0x3C / position +0x4C / scale +0x114, drawn by 0x7C4E70; else the engine's creature
	// draw (a_746C10) into the module arena
	uint32_t __cdecl d_7C4C10(uint32_t a1)
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
			return d_7C4E70(REAPER_CTX);
		}
		MEM<uint32_t>(PACKET_CURSOR) = a_746C10(a1, 0x25F3088, MEM<uint32_t>(PACKET_CURSOR));
		return 0; // void
	}

	// 0x7C4B50 (sub_7C4B50): REAPER creature task - state {0x7C5360 appear, 0x7CB2D0, 0x7CB320
	// strike, 0x7CBA50 vanish, 0x7CBAB0 end, ret}; the effect light block 0x1D99AB0 = its matrix
	// +0x70, translation +0x84.. = position +0x4C..; drawn unless hidden (0x7C4C10)
	uint32_t __cdecl d_7C4B50(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x7C5360;
		states[1] = 0x7CB2D0;
		states[2] = 0x7CB320;
		states[3] = 0x7CBA50;
		states[4] = 0x7CBAB0;
		states[5] = 0x7CBBD0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const int32_t pz = S16(node, 0x50);
		const int32_t py = S16(node, 0x4E);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 32);
		const int32_t px = S16(node, 0x4C);
		S32(node, 0x8C) = pz;
		const uint8_t f = U8(node, 0x26);
		S32(node, 0x84) = px;
		S32(node, 0x88) = py;
		if (!(f & 4))
		{
			// 30 fps layer: see mag074_lvdeath_quistis_held.inc
			FX_HELD(held_note_reaper(node);)
			d_7C4C10(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7CB2C0 (sub_7CB2C0): [0x1586388] +0x20 = 0
	uint32_t __cdecl d_7CB2C0(uint32_t a1)
	{
		(void)a1;
		U16(MEM<uint32_t>(0x1586388), 0x20) = 0;
		return 0; // void
	}

	// 0x7CB260 (sub_7CB260): render context a1 of the dark draw: cleared (0x54 bytes), +0x40 = the
	// vertex buffer a2, +0x44 = the object block a3 (+4 = a2, screen box 0 / 0 / 0x140 / 0, colour
	// 0x80 x 3, mask all, +0x24 0), +0x48 = the model block a4, +0x4C / +0x50 -> the packet cursor
	uint32_t __cdecl d_7CB260(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x54);
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

	// 0x7CB230 (sub_7CB230): the reaper's dark draw context (0x25EC528, vertex buffer 0x25ECBE8,
	// object block 0x25F3F30, model block = the reaper node +0x30); dark flag +0x26 bit 3
	uint32_t __cdecl d_7CB230(uint32_t a1)
	{
		d_7CB260(REAPER_CTX, 0x25ECBE8, 0x25F3F30, a1 + 0x30);
		U8(a1, 0x26) |= 8;
		return 0; // void
	}

	// 0x7C5360 (sub_7C5360): reaper state 0 - position / yaw from the reaper data, scale 1:1:1
	// (+0x114, +0x60 -> it); first action: animation 5, dark draw, the aura actor system (data
	// 0x1771F5C), the stage wobble in (0x7CB140), sound 0x158206C, next state; later actions:
	// animation 6, fully shown (+0x13A / +0x13C = 0x1000), sound 0x1582070, two states on
	uint32_t __cdecl d_7C5360(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(REAPER_DATA);
		U32(a1, 0x4C) = U32(d, 0);
		const uint32_t w4 = U32(d, 4);
		const uint16_t yaw = U16(d, 0x4A);
		U32(a1, 0x60) = a1 + 0x114;
		U32(a1, 0x50) = w4;
		U16(a1, 0x3E) = yaw;
		U32(a1, 0x11C) = 0x1000;
		U32(a1, 0x118) = 0x1000;
		U32(a1, 0x114) = 0x1000;
		d_7CB2C0(a1);
		if (U8(a1, 0x2E) == 0)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 5);
			d_7CB230(a1);
			a_73C100(a1, ORIG_ActorSystem, 0x1771F5C, 0, 0x14, 2);
			x::Effect_AddTaskAndInitFromCtx(0x25EC760, 0x7CB140, 0x70, a1);
			x::BdPlaySE(0x158206C, 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		x::au_re_Battle_ReadAnimation_7(a1, 6);
		U16(a1, 0x13A) = 0x1000;
		U16(a1, 0x13C) = 0x1000;
		d_7CAFB0(a1, 1);
		d_7CB0D0(a1);
		x::BdPlaySE(0x1582070, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 2);
		return 0; // void
	}

	// 0x7CB2D0 (sub_7CB2D0): reaper state 1 - the dark draw ends at animation frame 0x14 (+0x136);
	// when the appearance animation ends: animation 6, sound 0x1582070, next state
	uint32_t __cdecl d_7CB2D0(uint32_t a1)
	{
		if (U16(a1, 0x136) == 0x14)
			U8(a1, 0x26) &= 0xF7;
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			U8(a1, 0x26) &= 0xF7;
			x::au_re_Battle_ReadAnimation_7(a1, 6);
			x::BdPlaySE(0x1582070, 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7CB320 (sub_7CB320): reaper state 2 (strike) - visibility +0x13A down by 0x400 per tick (to
	// 0, copied to +0x13C, applied by 0x7CAFB0 / 0x7CB0D0); at animation frame 5: the reaper data's
	// strike flag (+0x48) and, unless the target is immune (target record +3 bit 2), the dark actor
	// system (data 0x1776950) and the spark bursts (0x7CB400); next state when the animation ends
	// (the last action) or from frame 0xB (other actions)
	uint32_t __cdecl d_7CB320(uint32_t a1)
	{
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0xFC00);
		if (S16(a1, 0x13A) <= 0)
			U16(a1, 0x13A) = 0;
		U16(a1, 0x13C) = U16(a1, 0x13A);
		d_7CAFB0(a1, 1);
		d_7CB0D0(a1);
		if (U16(a1, 0x136) == 5)
		{
			const int32_t action = S8(a1, 0x2A);
			U16(MEM<uint32_t>(REAPER_DATA), 0x48) = 1;
			const uint32_t targets = U32(U32(U32(a1, 0xC), 4) + (uint32_t)(action * 20), 8);
			const int32_t target = S8(a1, 0x2B);
			if (!(U8(targets + (uint32_t)(target * 24), 3) & 4))
			{
				a_73C100(a1, ORIG_ActorSystem, 0x1776950, 0, 0x14, 0);
				x::Effect_AddTaskAndInitFromCtx(0x25ECBD8, 0x7CB400, 0x118, a1);
			}
		}
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(a1);
		if (U8(a1, 0x2E) == U8(a1, 0x2F))
		{
			if ((uint16_t)r == 1)
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (S16(a1, 0x136) >= 0xB)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7CBA50 (sub_7CBA50): reaper state 3 (vanish) - animation step; visibility +0x13A up by 0x200
	// per tick to 0x1000 (then hidden: +0x26 bit 2, next state), copied to +0x13C and applied
	uint32_t __cdecl d_7CBA50(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0x200);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 4;
			U16(a1, 0x13A) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		d_7CAFB0(a1, 1);
		d_7CB0D0(a1);
		return 0; // void
	}

	// 0x7CBAB0 (sub_7CBAB0): reaper state 4 - the last action starts the stage wobble out
	// (0x7CBB00); finished, the reaper data's end flag (+0x4C), next state
	uint32_t __cdecl d_7CBAB0(uint32_t a1)
	{
		if (U8(a1, 0x2E) == U8(a1, 0x2F))
			x::Effect_AddTaskAndInitFromCtx(0x25EC760, 0x7CBB00, 0x70, a1);
		const uint32_t d = MEM<uint32_t>(REAPER_DATA);
		U16(a1, 0x26) |= 1;
		U16(d, 0x4C) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7CB1A0 (sub_7CB1A0): stage wobble state 0 - from tick 0x1C: level +0x1C = 0 and the four
	// stage wobble blocks cleared (words 0x1D98992 + k * 0x2C, bytes +0x28..+0x2A), next state
	uint32_t __cdecl d_7CB1A0(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x1C)
			return 0; // void
		U16(a1, 0x1C) = 0;
		uint32_t p = 0x1D989BA;
		for (int i = 4; i != 0; i--, p += 0x2C)
		{
			MEM<uint16_t>(p - 0x28) = 0;
			MEM<uint8_t>(p) = 0;
			MEM<uint8_t>(p - 1) = 0;
			MEM<uint8_t>(p - 2) = 0;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7CB1E0 (sub_7CB1E0): stage wobble state 1 - level +0x1C up by 0x200 per tick to 0x600 (then
	// finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl d_7CB1E0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
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

	// 0x7CBB60 (sub_7CBB60): stage wobble out state 0 - level +0x1C = 0x600, next state
	uint32_t __cdecl d_7CBB60(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0)
			return a1;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x600;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7CB460 (sub_7CB460): spark burst state 0 - position = the target's effect bone 0xF1, next state
	uint32_t __cdecl d_7CB460(uint32_t a1)
	{
		const uint32_t slot = U8(a1, 0x2D);
		x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0xF1, 0, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// one group of four sparks (0x7CB790, 0x118 bytes, spark queue) from the burst a1: each spark k
	// starts at the burst position (+0xD4 + 8k), with the base orientation `base` turned by a random
	// yaw (0..0xFFF) and pitch (`pitch_mask` / `pitch_add` of a random word) as its matrix (+0x54 +
	// 0x20k), and a velocity (+0xF4 + 8k) of -(0x1000 + random 0..0x1FFF) along y through it
	static void SparkGroup(uint32_t a1, uint32_t base, uint32_t m2, uint32_t pitch_mask, uint32_t pitch_add, int32_t pitch_sub)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25ECBD8, ORIG_Spark, 0x118, a1);
		uint32_t vel = t + 0xF4;
		uint32_t mat = t + 0x54;
		for (int k = 4; k != 0; k--, mat += 0x20, vel += 8)
		{
			U32(vel, -0x20) = U32(a1, 0x1C);
			U32(vel, -0x1C) = U32(a1, 0x20);
			const int16_t yaw = (int16_t)(x::CrtRand() & 0xFFF);
			const uint32_t r = x::CrtRand();
			memcpy((void *)m2, (const void *)base, 32);
			const uint32_t pitch = ((r & pitch_mask) + (uint32_t)pitch_add - (uint32_t)pitch_sub) & 0xFFF;
			x::MAG_022_sub_8DD8A0(m2, (uint32_t)(int32_t)yaw);
			x::sub_8DD7E0(m2, (uint32_t)(int32_t)(int16_t)pitch);
			memcpy((void *)mat, (const void *)m2, 32);
			const uint32_t r3 = x::CrtRand() & 0x1FFF;
			U16(vel, 2) = (uint16_t)(0xFFFFF000u - r3);
			x::matrixMultiplyVector(m2, vel, vel);
		}
	}

	// 0x7CB4A0 (sub_7CB4A0): spark burst state 1 - base orientation = the target's yaw (+0xE) then
	// roll 0x155 / pitch 0xEAB; per tick 1..4: 4 / 3 / 2 / 1 groups of steep sparks and 2 / 2 / 1 / 1
	// groups of flatter ones; finished (next state) from tick 4
	uint32_t __cdecl d_7CB4A0(uint32_t a1)
	{
		alignas(4) uint8_t base[32] = {}, m2[32] = {};
		const uint32_t slot = U8(a1, 0x2D);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		x::MAG_022_sub_8DD770(P(base));
		x::MAG_022_sub_8DD8A0(P(base), (uint32_t)(int32_t)S16(ent, 0xE));
		x::sub_8DD960(P(base), 0x155);
		x::sub_8DD7E0(P(base), 0xEAB);
		const int16_t c = S16(a1, 0x24);
		int32_t n = 0;
		if (c == 1) n = 4;
		else if (c == 2) n = 3;
		else if (c == 3) n = 2;
		else if (c == 4) n = 1;
		for (; n > 0; n--)
			SparkGroup(a1, P(base), P(m2), 0x1FF, 0, 0x100);
		n = 0;
		if (c == 1 || c == 2) n = 2;
		else if (c == 3 || c == 4) n = 1;
		for (; n > 0; n--)
			SparkGroup(a1, P(base), P(m2), 0xFF, 0x380, 0);
		if (S16(a1, 0x24) >= 4)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7CB8E0 (sub_7CB8E0): draws one spark (prim model +0x4C, colour +0x40, fade +0x50, depth bias
	// +0x52) with matrix a2 scaled by +0x30.. at the position +0x1C.., through the camera, via the
	// module's prim renderer 0x7C6240 (header 0x68 bytes on the Field_Alloc stack) into the arena
	uint32_t __cdecl d_7CB8E0(uint32_t a1, uint32_t a2)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		alignas(4) uint8_t mb[32];
		const uint32_t M = P(mb);
		memcpy(mb, (const void *)a2, 32);
		S32(M, 0x14) = S16(a1, 0x1C);
		S32(M, 0x18) = S16(a1, 0x1E);
		S32(M, 0x1C) = S16(a1, 0x20);
		x::scale3DMatrix(M, a1 + 0x30);
		x::ComposeAffineTransform(0x1D97778, M, M);
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t h = x::Field_Alloc(0x68);
		U32(h, 0) = U32(a1, 0x4C);
		U32(h, 8) = U32(a1, 0x40);
		U32(h, 0xC) = (uint32_t)(int32_t)S16(a1, 0x50);
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		U32(h, 0x10) = (uint32_t)(int32_t)S16(a1, 0x52);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U32(h, 0x14) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = d_7C6240(h, ot, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// the sparks' motion: velocity (+0xF4 + 8k, s16) loses a quarter, position (+0xD4 + 8k) moves by
	// velocity / 16 (C divisions)
	static void SparkMove(uint32_t node)
	{
		uint32_t v = node + 0xF4;
		for (int k = 4; k != 0; k--, v += 8)
		{
			for (int j = 0; j < 3; j++)
			{
				const int16_t s = S16(v, 2 * j);
				S16(v, 2 * j) = (int16_t)(s - (int16_t)(s / 4));
			}
			for (int j = 0; j < 3; j++)
				S16(v, -0x20 + 2 * j) = (int16_t)(S16(v, -0x20 + 2 * j) + (int16_t)(S16(v, 2 * j) / 16));
		}
	}

	// the sparks' scale (+0x30.. from the tables 0x1586308 / 948 / 968 by tick) and fade (+0x50,
	// 0x1586368), then the four draws
	static void SparkDraw(uint32_t node)
	{
		const int32_t e = S16(node, 0x24) * 2;
		S32(node, 0x30) = MEM<int16_t>(0x1586308 + e);
		S32(node, 0x34) = MEM<int16_t>(0x1586328 + e);
		S32(node, 0x38) = MEM<int16_t>(0x1586348 + e);
		U16(node, 0x50) = MEM<uint16_t>(0x1586368 + e);
		uint32_t pos = node + 0xD4;
		uint32_t mat = node + 0x54;
		for (int k = 4; k != 0; k--, pos += 8, mat += 0x20)
		{
			U32(node, 0x1C) = U32(pos, 0);
			U32(node, 0x20) = U32(pos, 4);
			d_7CB8E0(node, mat);
		}
	}

	// 0x7CB790 (sub_7CB790): SPARK task (four sparks) - state {0x7CB9E0 set-up, 0x7CBA10 finished at
	// tick 14, ret}, motion, draw
	uint32_t __cdecl d_7CB790(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7CB9E0;
		states[1] = 0x7CBA10;
		states[2] = 0x7CBA30; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		SparkMove(node);
		// 30 fps layer: see mag074_lvdeath_quistis_held.inc
		FX_HELD(held_note_spark(node);)
		SparkDraw(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7CB9E0 (sub_7CB9E0): spark state 0 - scale 1:1:1, prim model 0x1582074, colour (0x28, 0,
	// 0x0A), next state
	uint32_t __cdecl d_7CB9E0(uint32_t a1)
	{
		U32(a1, 0x38) = 0x1000;
		U32(a1, 0x34) = 0x1000;
		U32(a1, 0x30) = 0x1000;
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x1582074;
		U8(a1, 0x40) = 0x28;
		U8(a1, 0x41) = 0;
		U8(a1, 0x42) = 0xA;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7CBA10 (sub_7CBA10): spark state 1 - finished from tick 14, next state
	uint32_t __cdecl d_7CBA10(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xE)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// actor system drawing and allocation (the engine's code with the module's own packet cursor
	// 0x25EC51C, draw header and pool sizes)
	// ====================================================================================

	// 0x7C60E0 (sub_7C60E0): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x7C6240
	uint32_t __cdecl d_7C60E0(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(PACKET_CURSOR) = d_7C6240(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
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
				MEM<uint32_t>(PACKET_CURSOR) = d_7C6240(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
			}
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x7C8520 (sub_7C8520): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl d_7C8520(uint32_t a1)
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

	// 0x7C97D0 (sub_7C97D0): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x25F3EBC], cursor 0x25ECBD0) with the module's pool of 0x14 records
	uint32_t __cdecl d_7C97D0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25F3EBC);
		int32_t idx = MEM<int16_t>(0x25ECBD0);
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
			MEM<uint16_t>(0x25ECBD0) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25ECBD0) = 0;
		return slot;
	}

	// 0x7CA150 (sub_7CA150): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x25F3F20], cursor 0x25F3EC8) with the module's pool of 0x3C records
	uint32_t __cdecl d_7CA150(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25F3F20);
		int32_t i = MEM<int16_t>(0x25F3EC8);
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
			if (i >= 0x3B)
				i = 0;
			tries++;
			if (tries >= 0x3C)
				break;
		}
		i++;
		if (i >= 0x3B)
			MEM<uint16_t>(0x25F3EC8) = 0;
		else
			MEM<uint16_t>(0x25F3EC8) = (uint16_t)i;
		return slot;
	}

	// ====================================================================================
	// the library functions of this module whose code matches an engine / Tonberry / Boko /
	// MiniMog port up to the module's addresses and callees (the port's text with the module's
	// addresses)
	// ====================================================================================
	// 0x7C45E0 (copy of b_729BD0 0x729BD0; Boko master task, 097 MAG_097_sub_729BD0, 098 0x7219D0): copies the camera matrix,
	// picks this tick's double-buffered globals, runs the state handler +0x29, then the 7 task queues
	uint32_t __cdecl d_7C45E0(uint32_t a1)
	{
		g_mod = &MOD_074;
		// 30 fps layer: see act_engine_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25F3EFC) = 0x2793E58;
		MEM<uint32_t>(0x25EC680) = 0x2793E58;
		states[0] = 0x7C4760;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x7C4770;
		states[2] = 0x7C4780;
		states[3] = 0x7C4800;
		states[4] = 0x7CBCC0;
		states[5] = 0x7CBD00;
		states[6] = 0x7CBD10;
		states[7] = 0x7CBD20;
		states[8] = 0x7CBD50;
		states[9] = 0x7CBD60;
		states[10] = 0x7CBD80;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25F3084);
			const uint32_t v2 = MEM<uint32_t>(0x25F21DC);
			MEM<uint32_t>(0x25EC51C) = v1;
			MEM<uint32_t>(0x25EC67C) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25F3080);
			const uint32_t v2 = MEM<uint32_t>(0x25F21D8);
			MEM<uint32_t>(0x25EC51C) = v1;
			MEM<uint32_t>(0x25EC67C) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x25F3EF4) = 0;
		MEM<uint16_t>(0x25F3EB8) = 0;
		const uint32_t queues[7] = { 0x25F2F10, 0x25EC658, 0x25F3EA8, 0x25F2C70,
			0x25EC760, 0x25ECBD8, 0x25F2F00 };
		for (int i = 0; i < 7; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x7C5500 (copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs the director set-up 0x7C5530 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl d_7C5500(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			d_7C5530(a1);
			d_7C5FF0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7C56C0 (copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl d_7C56C0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25F3F2C);
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

	// 0x7C5940 (copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl d_7C5940(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25F3F2C);
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

	// 0x7C5FF0 (copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl d_7C5FF0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25F3F2C, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			d_7CAEF0();
			d_7C85A0();
			// 30 fps layer: see mag074_lvdeath_quistis_held.inc
			FX_HELD(held_note_system(sys);)
			d_7C8460();
			d_7C6080();
			sys = U32(0x25F3F2C, 0);
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
		U16(0x25F3EF4, 0) = (uint16_t)(U16(0x25F3EF4, 0) + c14);
		U16(0x25F3EB8, 0) = (uint16_t)(U16(0x25F3EB8, 0) + c16);
		return done;
	}

	// 0x7C6080 (copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl d_7C6080(void)
	{
		uint32_t act = U32(U32(0x25F3F2C, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25F3F2C, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					d_7C60E0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x7C6240 (copy of t_764540 0x764540; module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x7C6370, 0x7C6590, 0x7C6820, 0x7C6D10, a_73D770, a_73D9C0, 0x7C7830,
	// 0x7C7D90; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl d_7C6240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { d_7C6370, d_7C6590, d_7C6820, d_7C6D10, a_73D770, a_73D9C0, d_7C7830, d_7C7D90 };
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

	// 0x7C6370 (copy of t_764670 0x764670; module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl d_7C6370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C6590 (copy of t_764890 0x764890; module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl d_7C6590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C6820 (copy of t_764B20 0x764B20; module 090 sub_764B20): draws the textured-triangle list of render context a1
	// (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage,
	// uv2 in the high half of +8): RTPT, rejects on GTE FLAG / back-face / fully off-screen, optional
	// depth cue, emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ(AVSZ3) + bias) >> a3.
	// With a UV scroll (+0x18/+0x1A) the texel bytes are scrolled (wrapped by +0x28/+0x2A) and the
	// poly is bracketed by two 0xE2 texture-window prims (0xC B each, windows +0x1C / +0x24);
	// without one a DR_MODE prim (0xC B, tpage abr 1) is inserted before the poly.
	uint32_t __cdecl d_7C6820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C6D10 (copy of t_765010 0x765010; module 090 sub_765010): draws the textured-quad list of render context a1 (count,
	// then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16):
	// RTPT + RTPS, rejects on GTE FLAG / back-face / fully off-screen, optional depth cue, emits one
	// POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. With a UV scroll the texel
	// bytes are scrolled and the poly is bracketed by two 0xE2 texture-window prims; without one a
	// DR_MODE prim (tpage abr 1) is inserted before the poly.
	uint32_t __cdecl d_7C6D10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C7830 (copy of t_765B30 0x765B30; module 090 sub_765B30): prim-list block "Gouraud-textured triangles, UV scroll":
	// 0x1C-byte records (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, rgb1 +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face
	// (NCLIP, unless ctx+0x14 bit 0x20) and off-screen culling, optional lighting (bit 0x80),
	// z = OTZ + ctx+0x10 (>= 0) >> a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the
	// UVs are scrolled (wrapped by ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window
	// packets (ctx+0x1C set, ctx+0x24 restore); else followed by one draw-mode packet (tpage abr 1).
	// Returns the new packet cursor; ctx+0x2C advances past the block.
	uint32_t __cdecl d_7C7830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C7D90 (copy of t_766090 0x766090; module 090 sub_766090): prim-list block "Gouraud-textured quads, UV scroll":
	// 0x24-byte records (code/rgb0 +0, vertex indices +4/+6/+8/+0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, uv2 +0x14 / uv3 +0x16, rgb1..3 +0x18/+0x1C/+0x20) -> POLY_GT4 (tag
	// 0x0C000000), RTPT + RTPS for the 4th corner, AVSZ4, same culling / lighting / z / scroll /
	// texture-window framing as 0x765B30. Returns the new packet cursor; ctx+0x2C advances.
	uint32_t __cdecl d_7C7D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7C8460 (copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl d_7C8460(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25F3F2C), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25F3F2C);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						d_7C8520(act);   // (the original also pushes the bone entry, unused)
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
							d_7C8520(act);
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

	// 0x7C85A0 (copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl d_7C85A0(void)
	{
		uint32_t act = U32(U32(0x25F3F2C, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				d_7C9870(act);
			else if (type == 1)
				d_7C85E0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x7C85E0 (copy of a_740880 0x740880; engine; Siren sub_740880): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl d_7C85E0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25F3F2C);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (d_7C9720(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25F3F2C);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x7C9610 (copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl d_7C9610(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25F3F2C, 0);                  // actor state block
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
				dx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				dx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				dx::sub_45E0B0(in2);
				dx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x7C9720 (copy of a_7419C0 0x7419C0; engine; Siren sub_7419C0, in 095-100): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl d_7C9720(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				d_7C97A0(a1, a2);
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
			d_7C97A0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			d_7C97A0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x7C97A0 (copy of a_741A40 0x741A40; engine; Siren sub_741A40, in 095-100): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl d_7C97A0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return d_7C97D0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x7C9870 (copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl d_7C9870(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x7CA220, a1);
		else if (mode == 4)
			callp(0x7CA370, a1);
		d_7C98F0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x7C98F0 (copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl d_7C98F0(uint32_t a1)
	{
		uint32_t sys = U32(0x25F3F2C, 0);
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
				d_7C9970(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			d_7C9970(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x7C9970 (copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl d_7C9970(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25F3EFC, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25F3EFC, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = d_7CA150(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25F3F2C, 0);
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
		d_7CAE60(node, desc);
		U32(0x25F3EFC, 0) = U32(0x25F3EFC, 0) + 0x50;
		return 0; // void
	}

	// 0x7CA370 (copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl d_7CA370(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25F3F2C, 0);            // ecx
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
				uint32_t g = U32(0x25F3F2C, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25F3F2C, 0);
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

	// ====================================================================================
	// part e5
	// ====================================================================================
	// Part e5: actor state-machine engine helpers 0x743100-0x7475A0 (Siren canonical addresses):
	// sprite-strip sequence init/draw (0x7435E0/0x743720), creature model draw (0x746C10), prim-model
	// draw (0x7458E0), sub-effect spawners (0x743100/0x743190) and the small state-table handlers.
	// 0x743100 (engine; Siren sub_743100): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl d_7CAE60(uint32_t a1, uint32_t a2)
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
			d_7C97D0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			d_7C97D0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			d_7C97D0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			d_7C97D0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x7CAEF0 (copy of a_743190 0x743190; engine; Siren sub_743190): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via d_7C97D0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl d_7CAEF0(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25F3F2C);  // eax (re-read only after the calls)
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
					d_7C97D0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25F3F2C);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				d_7C97D0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25F3F2C);
			}
		}
		return 0; // void
	}

	// 0x7CAF80 (copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl d_7CAF80(uint32_t a1)
	{
		if (d_7C5FF0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7CAFB0 (copy of t_7678B0 0x7678B0; module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl d_7CAFB0(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x7CB0D0 (copy of t_768130 0x768130; module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl d_7CB0D0(uint32_t a1)
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

	// 0x7CBB80 (copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl d_7CBB80(uint32_t a1)
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

	// 0x7CBC30 (copy of t_768370 0x768370; module 090 MAG_090_sub_768370): damage state - when [[0x1582068]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl d_7CBC30(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1582068);
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
	// LV?Death's own versions of two engine functions (Death serves them with a_73F990 / a_747500)
	// ====================================================================================
	// 0x7C5530 (sub_7C5530; Death 0x8B7950 = a_73F990 0x73F990): actor director set-up for node a1, as
	// a_73F990 but with ONE target: *0x25F3F2C = state block node+0x34; copies node+0x29A / 0x29C (mode)
	// into it, points its 4 tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once
	// by 0x7C5D90, flag data+0), the caster (+0x2C) and the target list = count 1, the slot of target
	// +0x2B of action +0x2A of the cast context, +0x1A = node+0x29E; start / per-target / bounds positions
	// (0x7C57A0, 0x7C5B30; modes 1 / 3 / 4 extra set-ups) and the caster-to-target distance in state+0.
	uint32_t __cdecl d_7C5530(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		const uint16_t w29a = U16(node, 0x29A);
		const uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint16_t caster = (uint16_t)U8(node, 0x2C); // bx (movzx)
		U16(st, 0x22) = w29a;
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		const uint32_t relocated = U32(data, 0);
		MEM<uint32_t>(ACTOR_STATE) = st;
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(ACTOR_STATE);
			U32(data, 0) = 1;
		}
		const int32_t action = S8(node, 0x2A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = 1;
		const uint32_t rec = U32(U32(U32(node, 0x0C), 4) + (uint32_t)mul32(action, 5) * 4, 8);
		const int32_t target = S8(node, 0x2B);
		U32(st, 4) = U8(rec, (int32_t)mul32(target, 3) * 8);   // target slot
		U16(st, 0x1A) = U16(node, 0x29E);
		a_73FC20();
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 4)
			callp(F(F_73FB40), node);
		a_73FFB0();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 1)
		{
			callp(F(F_73FDC0), node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		// distance caster entity -> the target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		const int32_t s1 = S16(st, 0x1E);
		const uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		const uint32_t d0c = U32(e1, 0x1C);
		const uint32_t d10 = U32(e1, 0x20);
		const uint32_t s2 = U32(st, 4);
		const uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		const uint32_t d14 = U32(e2, 0x1C);
		const int32_t dx = (int16_t)(uint16_t)((uint16_t)d0c - (uint16_t)d14);
		const int32_t dy = (int16_t)(uint16_t)((uint16_t)(d0c >> 16) - (uint16_t)(d14 >> 16));
		const int32_t dz = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)U32(e2, 0x20));
		const int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		const uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(ACTOR_STATE);
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x7CBCC0 (MAG_074_sub_7CBCC0; Death 0x8BE100 = a_747500 0x747500): master state 4 - unless paused
	// (+0x63): while the target index +0x2B < target count +0x5A - 1: next target (+0x2B), ++emitter count
	// +0x2E and back to the spawn state (+0x29 - 1); then next state
	uint32_t __cdecl d_7CBCC0(uint32_t a1)
	{
		if (U8(a1, 0x63) != 0)
			return 0; // void
		const int8_t t = S8(a1, 0x2B);
		if ((int32_t)t < (int32_t)S16(a1, 0x5A) - 1)
		{
			U8(a1, 0x2B) = (uint8_t)(t + 1);
			U8(a1, 0x2E) = (uint8_t)(U8(a1, 0x2E) + 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) - 1);
			return 0; // void
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7C4760, (void *)a_73A0D0, "074 MAG_074_sub_7C4760" },
		{ 0x7C4770, (void *)a_73A0D0, "074 MAG_074_sub_7C4770" },
		{ 0x7C4800, (void *)a_73A170, "074 MAG_074_sub_7C4800" },
		{ 0x7C4830, (void *)a_73A1A0, "074 MAG_074_sub_7C4830" },
		{ 0x7C4C90, (void *)a_746C10, "074 sub_7C4C90" },
		{ 0x7C5450, (void *)a_73C100, "074 sub_7C5450" },
		{ 0x7C54A0, (void *)a_73A380, "074 sub_7C54A0" },
		{ 0x7C57A0, (void *)a_73FC20, "074 sub_7C57A0" },
		{ 0x7C5A10, (void *)a_73FE90, "074 sub_7C5A10" },
		{ 0x7C5B30, (void *)a_73FFB0, "074 sub_7C5B30" },
		{ 0x7C5D90, (void *)a_740210, "074 sub_7C5D90" },
		{ 0x7C7300, (void *)a_73D770, "074 sub_7C7300" },
		{ 0x7C7550, (void *)a_73D9C0, "074 sub_7C7550" },
		{ 0x7C8670, (void *)a_740910, "074 sub_7C8670" },
		{ 0x7C8CD0, (void *)a_740F70, "074 sub_7C8CD0" },
		{ 0x7C8D20, (void *)a_740FC0, "074 sub_7C8D20" },
		{ 0x7C8DB0, (void *)a_741050, "074 sub_7C8DB0" },
		{ 0x7C8E80, (void *)a_741120, "074 sub_7C8E80" },
		{ 0x7C98C0, (void *)a_741B60, "074 sub_7C98C0" },
		{ 0x7C9FF0, (void *)a_742290, "074 au_re__rand_28" },
		{ 0x7CA020, (void *)a_7422C0, "074 sub_7CA020" },
		{ 0x7CA060, (void *)a_742300, "074 sub_7CA060" },
		{ 0x7CA0B0, (void *)a_742350, "074 sub_7CA0B0" },
		{ 0x7CA100, (void *)a_7423A0, "074 au_re__rand_28_0" },
		{ 0x7CA210, (void *)a_7424B0, "074 sub_7CA210" },
		{ 0x7CA220, (void *)a_7424C0, "074 sub_7CA220" },
		{ 0x7CAA00, (void *)a_742CA0, "074 sub_7CAA00" },
		{ 0x7CAA30, (void *)a_742CD0, "074 sub_7CAA30" },
		{ 0x7CAA60, (void *)a_742D00, "074 sub_7CAA60" },
		{ 0x7CAC10, (void *)a_742EB0, "074 sub_7CAC10" },
		{ 0x7CAD50, (void *)a_742FF0, "074 sub_7CAD50" },
		{ 0x7CAFA0, (void *)a_73A6D0, "074 nullsub_1401" },
		{ 0x7CAFD0, (void *)a_733950, "074 sub_7CAFD0" },
		{ 0x7CB140, (void *)a_73A380, "074 sub_7CB140" },
		{ 0x7CB220, (void *)a_73A6D0, "074 nullsub_1402" },
		{ 0x7CB400, (void *)a_73A380, "074 sub_7CB400" },
		{ 0x7CBA30, (void *)a_73A6D0, "074 nullsub_1403" },
		{ 0x7CBA40, (void *)a_73A6D0, "074 nullsub_1404" },
		{ 0x7CBB00, (void *)a_73A380, "074 sub_7CBB00" },
		{ 0x7CBBC0, (void *)a_73A6D0, "074 nullsub_1405" },
		{ 0x7CBBD0, (void *)a_73A6D0, "074 nullsub_1406" },
		{ 0x7CBBE0, (void *)a_7475A0, "074 sub_7CBBE0" },
		{ 0x7CBC00, (void *)a_73A6D0, "074 nullsub_1407" },
		{ 0x7CBC10, (void *)a_73A6D0, "074 nullsub_1408" },
		{ 0x7CBC20, (void *)a_73A6D0, "074 nullsub_1409" },
		{ 0x7CBC70, (void *)a_7474B0, "074 MAG_074_sub_7CBC70" },
		{ 0x7CBC90, (void *)a_7474D0, "074 MAG_074_sub_7CBC90" },
		{ 0x7CBCB0, (void *)a_73A6D0, "074 nullsub_1410" },
		{ 0x7CBD00, (void *)a_73A0D0, "074 MAG_074_sub_7CBD00" },
		{ 0x7CBD10, (void *)a_747550, "074 MAG_074_sub_7CBD10" },
		{ 0x7CBD20, (void *)a_747560, "074 MAG_074_sub_7CBD20" },
		{ 0x7CBD50, (void *)a_747590, "074 MAG_074_sub_7CBD50" },
		{ 0x7CBD60, (void *)a_7475A0, "074 MAG_074_sub_7CBD60" },
		{ 0x7CBD80, (void *)a_73A6D0, "074 nullsub_1400" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x7C47C0, (void *)d_7C47C0, "d_7C47C0" },
		{ 0x7C4780, (void *)d_7C4780, "d_7C4780" },
		{ 0x7C48E0, (void *)d_7C48E0, "d_7C48E0" },
		{ 0x7C48B0, (void *)d_7C48B0, "d_7C48B0" },
		{ 0x7C4A40, (void *)d_7C4A40, "d_7C4A40" },
		{ 0x7C49D0, (void *)d_7C49D0, "d_7C49D0" },
		{ 0x7C4A90, (void *)d_7C4A90, "d_7C4A90" },
		{ 0x7C4B10, (void *)d_7C4B10, "d_7C4B10" },
		{ 0x7C4F70, (void *)d_7C4F70, "d_7C4F70" },
		{ 0x7C4E70, (void *)d_7C4E70, "d_7C4E70" },
		{ 0x7C4C10, (void *)d_7C4C10, "d_7C4C10" },
		{ 0x7C4B50, (void *)d_7C4B50, "d_7C4B50" },
		{ 0x7CB2C0, (void *)d_7CB2C0, "d_7CB2C0" },
		{ 0x7CB260, (void *)d_7CB260, "d_7CB260" },
		{ 0x7CB230, (void *)d_7CB230, "d_7CB230" },
		{ 0x7C5360, (void *)d_7C5360, "d_7C5360" },
		{ 0x7CB2D0, (void *)d_7CB2D0, "d_7CB2D0" },
		{ 0x7CB320, (void *)d_7CB320, "d_7CB320" },
		{ 0x7CBA50, (void *)d_7CBA50, "d_7CBA50" },
		{ 0x7CBAB0, (void *)d_7CBAB0, "d_7CBAB0" },
		{ 0x7CB1A0, (void *)d_7CB1A0, "d_7CB1A0" },
		{ 0x7CB1E0, (void *)d_7CB1E0, "d_7CB1E0" },
		{ 0x7CBB60, (void *)d_7CBB60, "d_7CBB60" },
		{ 0x7CB460, (void *)d_7CB460, "d_7CB460" },
		{ 0x7CB4A0, (void *)d_7CB4A0, "d_7CB4A0" },
		{ 0x7CB8E0, (void *)d_7CB8E0, "d_7CB8E0" },
		{ 0x7CB790, (void *)d_7CB790, "d_7CB790" },
		{ 0x7CB9E0, (void *)d_7CB9E0, "d_7CB9E0" },
		{ 0x7CBA10, (void *)d_7CBA10, "d_7CBA10" },
		{ 0x7C60E0, (void *)d_7C60E0, "d_7C60E0" },
		{ 0x7C8520, (void *)d_7C8520, "d_7C8520" },
		{ 0x7C97D0, (void *)d_7C97D0, "d_7C97D0" },
		{ 0x7CA150, (void *)d_7CA150, "d_7CA150" },
		{ 0x7C45E0, (void *)d_7C45E0, "d_7C45E0" },
		{ 0x7C5500, (void *)d_7C5500, "d_7C5500" },
		{ 0x7C56C0, (void *)d_7C56C0, "d_7C56C0" },
		{ 0x7C5940, (void *)d_7C5940, "d_7C5940" },
		{ 0x7C5FF0, (void *)d_7C5FF0, "d_7C5FF0" },
		{ 0x7C6080, (void *)d_7C6080, "d_7C6080" },
		{ 0x7C6240, (void *)d_7C6240, "d_7C6240" },
		{ 0x7C6370, (void *)d_7C6370, "d_7C6370" },
		{ 0x7C6590, (void *)d_7C6590, "d_7C6590" },
		{ 0x7C6820, (void *)d_7C6820, "d_7C6820" },
		{ 0x7C6D10, (void *)d_7C6D10, "d_7C6D10" },
		{ 0x7C7830, (void *)d_7C7830, "d_7C7830" },
		{ 0x7C7D90, (void *)d_7C7D90, "d_7C7D90" },
		{ 0x7C8460, (void *)d_7C8460, "d_7C8460" },
		{ 0x7C85A0, (void *)d_7C85A0, "d_7C85A0" },
		{ 0x7C85E0, (void *)d_7C85E0, "d_7C85E0" },
		{ 0x7C9610, (void *)d_7C9610, "d_7C9610" },
		{ 0x7C9720, (void *)d_7C9720, "d_7C9720" },
		{ 0x7C97A0, (void *)d_7C97A0, "d_7C97A0" },
		{ 0x7C9870, (void *)d_7C9870, "d_7C9870" },
		{ 0x7C98F0, (void *)d_7C98F0, "d_7C98F0" },
		{ 0x7C9970, (void *)d_7C9970, "d_7C9970" },
		{ 0x7CA370, (void *)d_7CA370, "d_7CA370" },
		{ 0x7CAE60, (void *)d_7CAE60, "d_7CAE60" },
		{ 0x7CAEF0, (void *)d_7CAEF0, "d_7CAEF0" },
		{ 0x7CAF80, (void *)d_7CAF80, "d_7CAF80" },
		{ 0x7CAFB0, (void *)d_7CAFB0, "d_7CAFB0" },
		{ 0x7CB0D0, (void *)d_7CB0D0, "d_7CB0D0" },
		{ 0x7CBB80, (void *)d_7CBB80, "d_7CBB80" },
		{ 0x7CBC30, (void *)d_7CBC30, "d_7CBC30" },
		{ 0x7C5530, (void *)d_7C5530, "d_7C5530" },
		{ 0x7CBCC0, (void *)d_7CBCC0, "d_7CBCC0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag074_lvdeath_quistis()
	{
		act::register_module(74);
		for (const act::lvdeath_quistis::ModPort *p = act::lvdeath_quistis::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(74, p->addr, p->port, p->name);
		for (const act::lvdeath_quistis::ModPort *p = act::lvdeath_quistis::PORTS; p->addr; p++)
			act::register_module_port(74, p->addr, p->port, p->name);
		// 30 fps layer: see mag074_lvdeath_quistis_held.inc
		FX_HELD(register_mag074_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag074_lvdeath_quistis_held.inc"
#endif
