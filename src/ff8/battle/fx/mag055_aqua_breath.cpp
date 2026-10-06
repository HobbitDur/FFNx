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

// Effect 55: Aqua Breath (enemy attack 146 of kernel.bin, used by Chimera c0m047; MAG_055_*; no other
// kernel.bin entry uses effect 55): a copy of the actor/heal effect library (see act_engine.h) built
// like Resonance (mag049_resonance.cpp: master + director stepping phases) with Drink Magic's orb
// (mag037_drink_magic.cpp) and sprite drops. Quistis' Aqua Breath (effect 72, MAG_072_AQUA_BREATH_QUISTIS
// 0x7D1E80, not ported) is a near-identical copy of this module (same code up to its addresses). It
// shares no module code with the other breath ports (235 Breath, 231 Disease Breath, 232 Breath of
// Death, 11 Storm Breath, 48 Magma Breath) besides the library's stage-light fade (0x808FD0 =
// Magma Breath's 0x82A720).
//
// Setup MAG_055_AQUA_BREATH 0x808820 (runs once, not ported; file loader MAG_055_AQUA_BREATH_FL
// 0x808800 = the effect's data file 0x15A75EC (mag054-071.tim) -> [0x2610FA8], TIM uploaded by the setup, packet arenas at file +
// 0xC000 / + 0x18000 by tick parity, the second arena cursor 0x26110AC) plays the camera animation
// 0x15A7498 and creates the root queue 0x2611180 with the master and five task pools: 0x2615838
// (4 x 0x58: emitters), 0x2611088 (3 x 0x48: director), 0x2611190 (0x32 x 0x154: the tasks),
// 0x2615410 (4 x 0x60: orb) and 0x2615828 (10 x 0x40, unused).
//   Master (0x808970 = Resonance's 0x821690) - camera copy 0x2793E58, packet cursor 0x2610FAC by tick
//     parity, bone follow, 11-state table: 0x808AE0 waits for the children, 0x808AF0 one emitter per
//     action (a_73A170), then the engine's end states.
//   Emitter (0x808B20 = the engine's a_73A1A0): state 0x808BA0 sets up the director block
//     [0x15A38D4] (0x808BD0: the caster's position, the party's centre, the facing, the farthest
//     target's distance clamped to 0x1000..0x2000 = the orb's scale) and spawns the director;
//     0x80C920 waits for the director's cue +0x48 (the damage).
//   Director (0x808DD0): phase bookkeeping 0x808E50 (a new phase spawns its tasks: 0x809150), state
//     0x808EA0 starts the stage darkening task 0x808ED0 (the four stage light words 0x1D98992 +
//     k * 0x2C to 0x600, held to its tick 0x29, back to 0), 0x809020 the cue to phase 1, 0x809080
//     the sound 0x15A38D8, 0x8090D0 the damage cue +0x48 after phase tick 0x32, 0x8090F0 the cue to
//     phase 2 (no tasks), finish.
//   Phase 1 (0x809150): the orb 0x809250, the stream 0x80B730 and per target a splash task 0x80C1B0
//     and a rain task 0x80C5A0.
//   Orb (0x809250, node 0x60 = Drink Magic's orb with its own states): the water ball at the caster
//     (spawn point 0x18 + (0, 0xB0, -0x20) turned by its facing), model 0x15A4168 rewritten every
//     tick from the template 0x15A6AD0 (0x80B490: turned about z, x waved by a sine of z), drawn
//     by the module's prim-model renderer (0x809300 -> 0x809430 = Granaldo's eight primitive-list
//     renderers) into the frame arena 0x1D8E054; fade +0x50 0x1000 -> 0 by 0x200, 15 ticks, back.
//   Stream (0x80B730): 16 ticks of drop tasks 0x80B8E0 (four drops thrown from the caster along
//     random directions, sprites [0x15A75CC + 4k] drawn by 0x80BA30 = Granaldo's sparkle sprite
//     draw through the engine's sequence player a_7435E0).
//   Splash (0x80C1B0): from tick 4 the target's hit animation (4 / 5 / 6), then every other tick a
//     task 0x80C430 of two drops at random bones of the target, thrown back (gravity), to tick 0x12.
//   Rain (0x80C5A0): from tick 0x14 every other tick a task 0x80C7B0 of four drops around random
//     bones of the target (sprites [0x15A75DC + 4k]), to tick 0x1C.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2610FA8..0x2616420 (file pointer 0x2610FA8, packet cursor 0x2610FAC, second arena
// cursor 0x26110AC, pools, queues, arena bases 0x26159A8 / 0x26159AC / 0x26155A0 / 0x26155A4, the
// director block 0x26163B0 = [0x15A38D4]); the stage light words 0x1D98992 + k * 0x2C; the orb model
// 0x15A4168 and its template 0x15A6AD0, the drop sprites (exe data).
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (ab_XXXXXX), the library ones
// the text of the matching Resonance / Granaldo / Doom / Drink Magic / Curse / Stare / Tonberry /
// Siren port with this module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace aqua
{
	static const Mod MOD_055 = { "aqua", 55, 0x808800, 0x80CA90,
		{ 0x0, 0x0, 0x15A38D4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2615838, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x808B20, 0x808BA0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x809050, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80BC90, 0x0, 0x80C920, 0x80C990, 0x80C9B0, 0x80C9D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2610FAC;   // module packet cursor (tick parity: file + 0xC000 / + 0x18000)
	static const uint32_t DIR_PTR = 0x15A38D4;         // director block pointer (0x54 bytes at 0x26163B0)
	static const uint32_t PHASE_DATA = DIR_PTR;
	static const uint32_t Q_EMITTER = 0x2615838, Q_DIRECTOR = 0x2611088, Q_TASKS = 0x2611190, Q_ORB = 0x2615410, Q_SPARE = 0x2615828;
	static const uint32_t ORIG_Master = 0x808970;
	static const uint32_t ORIG_Darken = 0x808ED0;      // stage darkening (engine task a_73F110)
	static const uint32_t ORIG_Orb = 0x809250;
	static const uint32_t ORIG_Stream = 0x80B730;      // (engine task a_73A380)
	static const uint32_t ORIG_Drops = 0x80B8E0;
	static const uint32_t ORIG_Splash = 0x80C1B0;      // (engine task a_73A380)
	static const uint32_t ORIG_SplashDrops = 0x80C430;
	static const uint32_t ORIG_Rain = 0x80C5A0;        // (engine task a_73A380)
	static const uint32_t ORIG_RainDrops = 0x80C7B0;
	static const uint32_t ORB_MODEL = 0x15A4168;       // the orb's model (exe data, rewritten every tick)
	static const uint32_t ORB_TEMPLATE = 0x15A6AD0;    // its vertex template
	static const uint32_t DROP_SPRITES = 0x15A75CC;    // 4 sprite pointers of the stream's drops
	static const uint32_t RAIN_SPRITES = 0x15A75DC;    // 4 sprite pointers of the rain drops
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x (the primitive-list renderers)
	namespace gx
	{
		static inline uint32_t GTE_MVMVA_ColorV0() { return fn<uint32_t (__cdecl *)()>(0x4607D0)(); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t sub_45DEA0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DEA0)(a1); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E160(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a1, a2, a3); }
		static inline uint32_t sub_45E220(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E220)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t sub_56BDE0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(a1, a2); }
	}

	// the orb's vertex pad: the stack word 0x80B490 stores into each vertex's pad (+6) is never written
	// by it. The orb task's states 0..2 call 0x80B5E0 three words below the orb task's frame: its saved
	// esi (the node) lies on the word, whose high half is the pad. States 3 / 4 write nothing there: the
	// word is what the previous code at that depth left. The game's executor 0x508420 calls every task
	// of a queue at the same esp and the master runs its queues 4 bytes apart (emitters X, director
	// X - 4, tasks X - 8, orbs X - 0xC), so the word (orb frame - 0x5A) is task frame - 0x5E of the task
	// queue, last written by its drop tasks' sprite draws 0x80BA30 (in queue order):
	//   - splash / rain drops (0x80C430 / 0x80C7B0, a drawn drop): the draw's argument hdr + 0xCC (hdr =
	//     its Field_Alloc(0xD8) block = the Field_Alloc pointer after the draw) -> its high half;
	//   - a drawn orb (0x809300, for the orbs after it in the orb queue): its Field_Free(0x68) argument -> 0.
	// While the orb is in states 3 / 4 (its ticks 24..31) the rain drops (spawned from the rain tasks'
	// tick 0x14, alive 13 ticks) are the queue's last tasks. Not modelled: the stream drops' draws
	// (0x80B8E0 leaves the high half of its own frame address there: they all precede the rain drops
	// in the queue), the other tasks' deeper frames (overwritten by the drop draws) and whatever code
	// outside the effect left there when no drop draws (0 then).
	static const uint32_t FALLOC_PTR = 0x1D999C4;      // Field_Alloc pointer
	static uint16_t g_pad_residue = 0;
	static uint16_t ORB_VERTEX_PAD = 0;
	static inline uint16_t orb_vertex_pad(uint32_t node, int32_t state)
	{
		if (state <= 2)
			return (uint16_t)(node >> 16);
		return g_pad_residue;
	}


	// helpers of the library code (as in act_engine.cpp / mag097_boko.cpp)
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
	static inline bool b1_out(int16_t v, int16_t hi) { return v < 0 || v > hi; }
	static inline bool b2_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool b2_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
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

	// C truncating divisions as compiled (magic-number multiplications)
	static inline int32_t div3(int32_t v)
	{
		const int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return add32(hi, (int32_t)((uint32_t)hi >> 31));
	}
	static inline int32_t div10(int32_t v)
	{
		const int32_t hi = (int32_t)(((int64_t)v * 0x66666667LL) >> 32) >> 2;
		return add32(hi, (int32_t)((uint32_t)hi >> 31));
	}

	// the prim-model renderer helpers (as in mag014_death.cpp / mag090_tonberry.cpp)
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

	// the Siren renderer helpers (as in mag095_siren.cpp)
	namespace
	{
		// GP0 0xE2 texture-window word built from a RECT {x,y,w,h} (u16 each) at ctx+b
		// (0x73D082-0x73D0C1 / 0x73D124-0x73D163; same sequence at 0x73D5EA / 0x73D696)
		uint32_t s2_twin(uint32_t ctx, int32_t b)
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
	}
	namespace
	{
		// PSX texture-window word (GP0 0xE2) built from a {u8 x, pad, u8 y, pad, u16 w, u16 h}
		// block at ctx+o, exactly as 0x73E625..0x73E664 / 0x73E6EF..0x73E72E compute it
		uint32_t s3_texwin(uint32_t ctx, int32_t o)
		{
			uint32_t c = ((uint32_t)U8(ctx, o + 2) & 0xF8) | 0xFFFE2000u;
			c = (c << 5) | ((uint32_t)U8(ctx, o) & 0xF8);
			c = (c << 5) | (~((uint32_t)U16(ctx, o + 6) - 1u) & 0xF8);
			c = (c << 2) | ((~((uint32_t)U16(ctx, o + 4) - 1u) >> 3) & 0x1F);
			return c;
		}

		// 0x73E4A0..0x73E50B / 0x73E517..0x73E582: add d to the 4 texcoord bytes pkt+o, +o+0xC,
		// +o+0x18, +o+0x24 (u or v of the 4 corners); when any sum exceeds 0xFF all four are
		// wrapped back by the window size byte at ctx+wo
		void s3_scroll_uv(uint32_t pkt, int32_t o, uint32_t d, uint32_t ctx, int32_t wo)
		{
			int32_t a = (int32_t)(U8(pkt, o) + d);
			int32_t c = (int32_t)(U8(pkt, o + 0xC) + d);
			int32_t e = (int32_t)(U8(pkt, o + 0x18) + d);
			int32_t b = (int32_t)(U8(pkt, o + 0x24) + d);
			if ((b | e | c | a) > 0xFF)
			{
				uint8_t w = U8(ctx, wo);
				U8(pkt, o) = (uint8_t)(a - w);
				U8(pkt, o + 0xC) = (uint8_t)(c - w);
				U8(pkt, o + 0x18) = (uint8_t)(e - w);
				U8(pkt, o + 0x24) = (uint8_t)(b - w);
			}
			else
			{
				U8(pkt, o) = (uint8_t)a;
				U8(pkt, o + 0xC) = (uint8_t)c;
				U8(pkt, o + 0x18) = (uint8_t)e;
				U8(pkt, o + 0x24) = (uint8_t)b;
			}
		}

		inline bool s3_off(int16_t v, int16_t lim) { return v < 0 || v > lim; }

		// one iteration of the 0x73E20F loop: projects one quad record (0x24 bytes at data) into the
		// POLY_GT4 at pkt; returns the packet cursor after what it emitted (pkt itself when culled)
		uint32_t s3_quad(uint32_t ctx, uint32_t data, uint32_t vbase, uint32_t ot_base, uint32_t shift, uint32_t pkt)
		{
			x::GTE_LoadV012(vbase + ((uint32_t)U16(data, 4) << 2), vbase + ((uint32_t)U16(data, 6) << 2), vbase + ((uint32_t)U16(data, 8) << 2));
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x14);
			uint32_t col0 = U32(data, 0);
			U32(pkt, 0) = 0x0C000000;  // tag: 12 words
			U32(pkt, 4) = col0;        // rgb0 + code
			if (flags & 2)
			{
				col0 |= 0x2000000;     // semi-transparent
				U32(pkt, 4) = col0;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t uv0 = U32(data, 0xC);
			uint32_t uv1 = U32(data, 0x10);
			U32(pkt, 0xC) = uv0;           // uv0 + clut
			uint32_t uv23 = U32(data, 0x14);
			U32(pkt, 0x24) = uv23;         // uv2 (the pad half gets the uv3 bits)
			U32(pkt, 0x18) = uv1;          // uv1 + tpage
			U32(pkt, 0x30) = uv23 >> 16;   // uv3
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (U32(ctx, 0x3C) & 0x60000)
				return pkt;
			x::GTE_NCLIP();
			uint32_t clip = 0;
			x::GTE_ReadMAC0(ctx + 0x30);
			if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x20))
				return pkt;  // back face, not double-sided
			x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
			x::GTE_LoadV0(vbase + ((uint32_t)U16(data, 0xA) << 2));
			x::GTE_RTPS();
			if (s3_off(S16(pkt, 0x8), 0xA00)) clip = 1;
			if (s3_off(S16(pkt, 0x14), 0xA00)) clip |= 2;
			if (s3_off(S16(pkt, 0x20), 0xA00)) clip |= 4;
			if (s3_off(S16(pkt, 0xA), 0x6C0)) clip |= 0x10;
			if (s3_off(S16(pkt, 0x16), 0x6C0)) clip |= 0x20;
			if (s3_off(S16(pkt, 0x22), 0x6C0)) clip |= 0x40;
			x::GTE_ReadSXY2(pkt + 0x2C);
			x::GTE_AVSZ4();
			uint32_t c = clip;
			if (s3_off(S16(pkt, 0x2C), 0xA00)) c |= 8;
			if (s3_off(S16(pkt, 0x2E), 0x6C0)) c |= 0x80;
			if ((c & 0xF) == 0xF)
				return pkt;  // all 4 x off screen
			if ((c & 0xF0) == 0xF0)
				return pkt;  // all 4 y off screen
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x80)
			{
				// depth-cued colours: rgb1..3 from the record's 3 colours, rgb0 from the packet
				x::sub_45E120(data + 0x18, data + 0x1C, data + 0x20);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F4C0();
				x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
				x::set_unk_1CA8A28(pkt + 4);
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pkt + 4);
			}
			else
			{
				uint32_t c1 = U32(data, 0x18);
				uint32_t c2 = U32(data, 0x1C);
				U32(pkt, 0x10) = c1;
				uint32_t c3 = U32(data, 0x20);
				U32(pkt, 0x1C) = c2;
				U32(pkt, 0x28) = c3;
			}
			int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));  // OTZ + bias
			S32(ctx, 0x38) = z;
			if (z < 0)
				S32(ctx, 0x38) = 0;
			uint32_t ot = ot_base + ((uint32_t)(S32(ctx, 0x38) >> (shift & 31)) << 2);
			uint16_t du = U16(ctx, 0x18);
			uint16_t dv = U16(ctx, 0x1A);
			if ((uint16_t)(du | dv) == 0)
			{
				x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
				return pkt + 0x34;
			}
			if (du != 0)
				s3_scroll_uv(pkt, 0xC, du, ctx, 0x28);
			uint16_t dv2 = U16(ctx, 0x1A);
			if (dv2 != 0)
				s3_scroll_uv(pkt, 0xD, dv2, ctx, 0x2A);
			// scrolling quad: texture window A, the quad, texture window B (OT prepends, so
			// the GPU sees B, quad, A)
			uint32_t prim = pkt;
			uint32_t twa = pkt + 0x34;
			U32(twa, 0) = 0x2000000;
			uint32_t w = (ctx + 0x1C != 0) ? s3_texwin(ctx, 0x1C) : 0;
			U32(twa, 4) = w;
			U32(twa, 8) = 0;
			x::SSIGPU_InsertPrimAutoDepth(ot, twa);
			x::SSIGPU_InsertPrimAutoDepth(ot, prim);
			uint32_t twb = pkt + 0x40;
			U32(twb, 0) = 0x2000000;
			w = (ctx + 0x24 != 0) ? s3_texwin(ctx, 0x24) : 0;
			U32(twb, 4) = w;
			U32(twb, 8) = 0;
			x::SSIGPU_InsertPrimAutoDepth(ot, twb);
			return pkt + 0x4C;
		}

		// common task epilogue of the four model tasks (after the state call)
		inline uint32_t s3_task_end(uint32_t n, uint8_t status)
		{
			if ((status & 1) && U8(n, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(n);
				return 2;
			}
			return 0;
		}
	}

	// director block and state helpers of the Tonberry / Death copies (the module's director block [0x15A38D4])
	static inline uint32_t t1_data() { return MEM<uint32_t>(DIR_PTR); }
	static inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ab_808970(uint32_t a1);
	uint32_t __cdecl ab_808AE0(uint32_t a1);
	uint32_t __cdecl ab_808BA0(uint32_t a1);
	uint32_t __cdecl ab_808BD0(uint32_t a1);
	uint32_t __cdecl ab_808D10(uint32_t a1);
	uint32_t __cdecl ab_808DD0(uint32_t a1);
	uint32_t __cdecl ab_808E50(uint32_t a1);
	uint32_t __cdecl ab_808EA0(uint32_t a1);
	uint32_t __cdecl ab_808F80(uint32_t a1);
	uint32_t __cdecl ab_808FC0(uint32_t a1);
	uint32_t __cdecl ab_808FD0(uint32_t a1);
	uint32_t __cdecl ab_809020(uint32_t a1);
	uint32_t __cdecl ab_809080(uint32_t a1);
	uint32_t __cdecl ab_8090D0(uint32_t a1);
	uint32_t __cdecl ab_809150(uint32_t a1);
	uint32_t __cdecl ab_809250(uint32_t a1);
	uint32_t __cdecl ab_809300(uint32_t a1);
	uint32_t __cdecl ab_809430(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_809560(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_809780(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_809A10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_809E90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_80A940(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_80AE40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_80B490(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ab_80B580(uint32_t a1);
	uint32_t __cdecl ab_80B5E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ab_80B680(uint32_t a1);
	uint32_t __cdecl ab_80B6C0(uint32_t a1);
	uint32_t __cdecl ab_80B6F0(uint32_t a1);
	uint32_t __cdecl ab_80B790(uint32_t a1);
	uint32_t __cdecl ab_80B7D0(uint32_t a1);
	uint32_t __cdecl ab_80B8E0(uint32_t a1);
	uint32_t __cdecl ab_80BA30(uint32_t a1);
	uint32_t __cdecl ab_80C100(uint32_t a1);
	uint32_t __cdecl ab_80C140(uint32_t a1);
	uint32_t __cdecl ab_80C210(uint32_t a1);
	uint32_t __cdecl ab_80C2A0(uint32_t a1);
	uint32_t __cdecl ab_80C3D0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ab_80C430(uint32_t a1);
	uint32_t __cdecl ab_80C540(uint32_t a1);
	uint32_t __cdecl ab_80C560(uint32_t a1);
	uint32_t __cdecl ab_80C600(uint32_t a1);
	uint32_t __cdecl ab_80C650(uint32_t a1);
	uint32_t __cdecl ab_80C7B0(uint32_t a1);
	uint32_t __cdecl ab_80C8C0(uint32_t a1);
	uint32_t __cdecl ab_80C8E0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag055_aqua_breath_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace aqua
{
	// ====================================================================================
	// the module's own code
	// ====================================================================================

	// 0x808BD0 (MAG_055_sub_808BD0): sets up the director block [0x15A38D4] (cleared, 0x54 bytes): +8
	// the centre of the party (0x808C80), +0 the caster's position (entity +0x1C / +0x20 dwords),
	// +0x4A the facing from the caster towards the party ((angle - 0x800) & 0xFFF), +0x3C the
	// farthest target's ground distance (0x808D10) clamped to [0x1000, 0x2000] (the orb / drop scale)
	uint32_t __cdecl ab_808BD0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		a_73A720(MEM<uint32_t>(DIR_PTR) + 8);
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2C));
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U32(d, 0) = U32(ent, 0x1C);
		U32(d, 4) = U32(ent, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		const uint32_t dist = ab_808D10(node);
		const uint32_t d2 = MEM<uint32_t>(DIR_PTR);
		U32(d2, 0x3C) = dist;
		if ((int32_t)dist < 0x1000)
			U32(d2, 0x3C) = 0x1000;
		else if ((int32_t)dist > 0x2000)
			U32(d2, 0x3C) = 0x2000;
		return dist;
	}

	// 0x808DD0 (MAG_055_sub_808DD0): DIRECTOR task - phase bookkeeping (0x808E50), then the state
	// {0x808EA0 stage darkening, 0x809020 cue to phase 1, 0x809080 sound, 0x8090D0 damage cue,
	// 0x8090F0 cue to phase 2, 0x809110 finish, ret}
	uint32_t __cdecl ab_808DD0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x808EA0;
		states[1] = 0x809020;
		states[2] = 0x809080;
		states[3] = 0x8090D0;
		states[4] = 0x8090F0;
		states[5] = 0x809110;
		states[6] = 0x809130; // nullsub (ret)
		ab_808E50(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x808EA0 (sub_808EA0): director state 0 - spawns the stage darkening task 0x808ED0 (0x154 bytes,
	// task queue), a one-tick wait (+0x44 = 1), next state
	uint32_t __cdecl ab_808EA0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Darken, 0x154, node);
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x44) = 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x809020 (sub_809020): director state 1 - once the wait +0x44 has run out, requests phase 1
	// (0x809050); next state when accepted
	uint32_t __cdecl ab_809020(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x44) = (uint16_t)(U16(node, 0x44) - 1);
		if (S16(node, 0x44) > 0)
			return 0; // void
		if (a_73B640(1) != 0)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x809150 (sub_809150): the tasks of a new phase (phase 1 only): the orb 0x809250 (0x60 bytes, orb
	// queue), the stream 0x80B730, one splash task 0x80C1B0 per target, then one rain task 0x80C5A0
	// per target (0x154 bytes, task queue; +0x2D = the target's slot)
	uint32_t __cdecl ab_809150(uint32_t a1)
	{
		if ((int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x40) - 1 != 0)
			return 0; // void
		const uint32_t node = a1;
		x::Effect_AddTaskAndInitFromCtx(Q_ORB, ORIG_Orb, 0x60, node);
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Stream, 0x154, node);
		uint32_t act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
		if (U8(act, 0x10) != 0)
		{
			int32_t k = 0;
			do
			{
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Splash, 0x154, node);
				act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
				U8(t, 0x2D) = U8(U32(act, 8), k * 0x18);
				k++;
			} while (k < (int32_t)U8(act, 0x10));   // the count re-read every pass
		}
		act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
		if (U8(act, 0x10) != 0)
		{
			int32_t k = 0;
			do
			{
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Rain, 0x154, node);
				act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
				U8(t, 0x2D) = U8(U32(act, 8), k * 0x18);
				k++;
			} while (k < (int32_t)U8(act, 0x10));
		}
		return 0; // void
	}

	// ---- the orb ----

	// 0x809250 (sub_809250): ORB task - state {0x80B580 appear, 0x80B680 fade, 0x80B6C0 hold,
	// 0x80B6F0 fade back, ret}; every tick: angles +0x54 / +0x58 += 0xA0, its model [+0x4C] rewritten
	// from the template 0x15A6AD0 (0x80B490), texture offset +0x5E += 8 (& 0x7F), draw (0x809300)
	uint32_t __cdecl ab_809250(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x80B580;
		states[1] = 0x80B680;
		states[2] = 0x80B6C0;
		states[3] = 0x80B6F0;
		states[4] = 0x80B720; // nullsub (ret)
		const int32_t st = S8(node, 0x29);
		callp(states[st], node);
		ORB_VERTEX_PAD = orb_vertex_pad(node, st);
		const uint32_t b = (uint32_t)(uint16_t)(U16(node, 0x58) + 0xA0) & 0xFFF;
		const uint32_t a = (uint32_t)(uint16_t)(U16(node, 0x54) + 0xA0) & 0xFFF;
		U16(node, 0x58) = (uint16_t)b;
		U16(node, 0x54) = (uint16_t)a;
		ab_80B490(ORB_TEMPLATE, U32(node, 0x4C), a, b);
		U16(node, 0x5E) = (uint16_t)((uint8_t)(U8(node, 0x5E) + 8) & 0x7F);
		// 30 fps layer: see mag055_aqua_breath_held.inc
		FX_HELD(held_note_orb(node);)
		ab_809300(node);
		if ((U8(node, 0x26) & 4) == 0)
			g_pad_residue = 0;   // (its Field_Free(0x68) argument; see orb_vertex_pad)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x80B490 (sub_80B490): rewrites the vertices of model a2 from the template a1 (count +4, 8-byte
	// vertices from +8): each vertex turned by a4 about z (0x8DD960 through the GTE), then x +=
	// sin((z / 2 + a3) & 0xFFF) * (z * 5 / 16) / 4096; the vertex pad (+6) gets the never-written
	// stack word ORB_VERTEX_PAD (see orb_vertex_pad)
	uint32_t __cdecl ab_80B490(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t src = a1 + 4;
		const int32_t count = S32(src, 0);
		alignas(4) uint8_t m[0x20];
		x::MAG_022_sub_8DD770(P(m));
		x::sub_8DD960(P(m), (uint32_t)(int32_t)(int16_t)a4);
		if (count <= 0)
			return 0; // void
		const uint32_t delta = a2 - src;
		uint32_t v = src + 4;
		for (int32_t n = count; n != 0; n--, v += 8)
		{
			const int32_t z = S16(v, 4);
			const uint32_t ang = (uint32_t)add32((z - (z >> 31)) >> 1, (int16_t)a3) & 0xFFF;
			const int32_t z5 = z * 0x500;
			const int16_t amp = (int16_t)((z5 + ((z5 >> 31) & 0xFFF)) >> 12);
			alignas(4) uint8_t in[8];
			alignas(4) uint16_t out[4];
			out[3] = ORB_VERTEX_PAD;
			U32(P(in), 0) = U32(v, 0);
			U32(P(in), 4) = U32(v, 4);
			x::GTE_SetRotMatrix(P(m));
			x::GTE_LoadV0(P(in));
			x::GTE_MVMVA_RotV0();
			x::GTE_StoreIR123(P(out));
			const int32_t s = (int32_t)x::computeSin(ang);
			const int32_t p = mul32(s, amp);
			const uint32_t zw = U32(P(out), 4);
			out[0] = (uint16_t)(out[0] + (uint16_t)((p + ((p >> 31) & 0xFFF)) >> 12));
			U32(v + delta, 4) = U32(P(out), 0);
			U32(v + delta, 8) = zw;
		}
		return 0; // void
	}

	// 0x80B580 (sub_80B580): orb state 0 - scale +0x30..+0x38 = the director block's +0x3C, model
	// 0x15A4168, facing +0x46 = the caster's, at the caster (0x80B5E0), fade +0x50 = 0x1000, next state
	uint32_t __cdecl ab_80B580(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t s = U32(MEM<uint32_t>(DIR_PTR), 0x3C);
		const uint8_t slot = U8(node, 0x2C);
		U32(node, 0x38) = s;
		U32(node, 0x34) = s;
		U32(node, 0x30) = s;
		U32(node, 0x4C) = ORB_MODEL;
		U16(node, 0x46) = U16(ENT(slot), 0xE);
		ab_80B5E0(node + 0x1C, slot);
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x50) = 0x1000;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x80B5E0 (sub_80B5E0): a2 = entity slot (s16): the three s16 at a1 = its spawn point 0x18 (flag
	// 0x1000) plus (0, 0xB0, -0x20) turned by its facing (entity +0xE)
	uint32_t __cdecl ab_80B5E0(uint32_t a1, uint32_t a2)
	{
		const uint32_t pos = a1;
		const uint32_t ent = ENT((uint32_t)(int32_t)(int16_t)a2);
		x::GetEffectSpawnPosition(ent, 0x18, 0x1000, pos);
		alignas(4) uint16_t v[4];
		alignas(4) uint8_t m[0x20];
		v[0] = 0;
		v[1] = 0xB0;
		v[2] = 0xFFE0;
		v[3] = 0;   // (pad, never written by the original: only the GTE VZ0 high half, unused)
		x::MAG_022_sub_8DD770(P(m));
		x::MAG_022_sub_8DD8A0(P(m), (uint32_t)(int32_t)S16(ent, 0xE));
		x::GTE_SetRotMatrix(P(m));
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(v));
		U16(pos, 0) = (uint16_t)(U16(pos, 0) + v[0]);
		U16(pos, 2) = (uint16_t)(U16(pos, 2) + v[1]);
		U16(pos, 4) = (uint16_t)(U16(pos, 4) + v[2]);
		return 0; // void
	}

	// 0x80B680 (sub_80B680): orb state 1 - at the caster, fade +0x50 down by 0x200 per tick; at 0 a
	// 15-tick hold (+0x5C), next state
	uint32_t __cdecl ab_80B680(uint32_t a1)
	{
		ab_80B5E0(a1 + 0x1C, (uint32_t)U8(a1, 0x2C));
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
		if (S16(a1, 0x50) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x50) = 0;
			U16(a1, 0x5C) = 0xF;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ---- the stream (the breath: drops thrown from the caster) ----

	// 0x80B790 (sub_80B790): stream state 0 - its matrix +0x58 = the caster's facing (identity turned
	// about y), next state
	uint32_t __cdecl ab_80B790(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2C));
		x::MAG_022_sub_8DD770(node + 0x58);
		x::MAG_022_sub_8DD8A0(node + 0x58, (uint32_t)(int32_t)S16(ent, 0xE));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x80B7D0 (sub_80B7D0): stream state 1 - every tick at the caster (0x80B5E0) and one drop task
	// 0x80B8E0 (0x154 bytes, task queue) of four drops: each its matrix (+0xC0 + k * 0x20) = the
	// stream's turned by a random yaw (+-0x180) then a random pitch (+-0xC0), a random spin
	// (+0x148 + k * 2), the drop task's origin +0x78 = the stream's position; after tick 15 finished,
	// next state
	uint32_t __cdecl ab_80B7D0(uint32_t a1)
	{
		const uint32_t node = a1;
		ab_80B5E0(node + 0x1C, (uint32_t)U8(node, 0x2C));
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Drops, 0x154, node);
		uint32_t spin = t + 0x148;
		uint32_t mt = t + 0xC0;
		for (int32_t n = 4; n != 0; n--)
		{
			const int32_t r1 = (int32_t)x::CrtRand();
			const int32_t yaw = r1 % 0x300 - 0x180;
			const int32_t r2 = (int32_t)x::CrtRand();
			const int32_t pitch = r2 % 0x180;
			memcpy((void *)mt, (const void *)(node + 0x58), 0x20); // rep movsd
			const int16_t pitch16 = (int16_t)(pitch - 0xC0);
			U32(t, 0x78) = U32(node, 0x1C);
			U32(t, 0x7C) = U32(node, 0x20);
			x::MAG_022_sub_8DD8A0(mt, (uint32_t)(int32_t)(int16_t)yaw);
			x::sub_8DD7E0(mt, (uint32_t)(int32_t)pitch16);
			const uint32_t r3 = x::CrtRand();
			mt += 0x20;
			U16(spin, 0) = (uint16_t)(r3 & 0xFFF);
			spin += 2;
		}
		if (S16(node, 0x24) > 0xF)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x80B8E0 (sub_80B8E0): DROP task (four drops) - state {0x80C100 set-up, 0x80C140 the sprite
	// animation (a_743C20), ret}; every tick the fall +0x150 += 0x10, then each drop k: spin +0x148 + 2k
	// += 0x100, distance +0x140 + 2k += speed +0x152, its offset (0, fall, distance) turned by its
	// matrix (+0xC0 + 0x20k, then its spin about z) -> +0x1C plus the origin +0x78, sprite
	// [0x15A75CC + 4k], drawn (0x80BA30)
	uint32_t __cdecl ab_80B8E0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x80C100;
		states[1] = 0x80C140;
		states[2] = 0x80C190; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x150) = (uint16_t)(U16(node, 0x150) + 0x10);
		uint32_t mt = node + 0xC0;
		uint32_t spin = node + 0x148;
		for (uint32_t tbl = DROP_SPRITES; tbl < DROP_SPRITES + 0x10; tbl += 4, mt += 0x20, spin += 2)
		{
			alignas(4) uint16_t v[4];
			alignas(4) uint8_t m[0x20];
			const uint16_t s = (uint16_t)((U16(spin, 0) + 0x100) & 0xFFF);
			v[0] = 0;
			U16(spin, 0) = s;
			U16(spin, -8) = (uint16_t)(U16(spin, -8) + U16(node, 0x152));
			v[2] = U16(spin, -8);
			v[1] = U16(node, 0x150);
			v[3] = 0;   // (pad, never written by the original: only the GTE VZ0 high half, unused)
			memcpy(m, (const void *)mt, 0x20); // rep movsd
			x::sub_8DD960(P(m), (uint32_t)(int32_t)(int16_t)s);
			x::GTE_SetRotMatrix(P(m));
			x::GTE_LoadV0(P(v));
			x::GTE_MVMVA_RotV0();
			x::GTE_StoreIR123(node + 0x1C);
			U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + U16(node, 0x78));
			U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x7A));
			U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + U16(node, 0x7C));
			U32(node, 0x4C) = MEM<uint32_t>(tbl);
			// 30 fps layer: see mag055_aqua_breath_held.inc
			FX_HELD(held_note_drop(node, (int)((tbl - DROP_SPRITES) / 4));)
			ab_80BA30(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x80C100 (sub_80C100): drop state 0 - sprite 0x15A3A3C (+0x4C, replaced per drop), depth
	// +0x56 = -0x10, last frame +0x52 = 0xC, speed +0x152 = the director block's +0x3C / -3, next state
	uint32_t __cdecl ab_80C100(uint32_t a1)
	{
		const uint32_t node = a1;
		const int32_t v = S32(MEM<uint32_t>(DIR_PTR), 0x3C);
		int32_t hi = (int32_t)(((int64_t)v * (int64_t)(int32_t)0xD5555555) >> 32);
		hi >>= 1;
		const int32_t q = add32(hi, (int32_t)((uint32_t)hi >> 31));
		U32(node, 0x4C) = 0x15A3A3C;
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x56) = 0xFFF0;
		U16(node, 0x52) = 0xC;
		U16(node, 0x152) = (uint16_t)q;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// ---- the targets: splash (0x80C1B0) and rain (0x80C5A0), one each per target ----

	// 0x80C210 (sub_80C210): splash state 0 - from tick 4: its matrix +0x58 = the caster's facing, the
	// target (+0x2D) plays hit animation 4, 5 or 6 (rand % 3, 0x505CE0), next state
	uint32_t __cdecl ab_80C210(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 4)
			return 0; // void
		const uint32_t caster = ENT(U8(node, 0x2C));
		const uint32_t target = ENT(U8(node, 0x2D));
		x::MAG_022_sub_8DD770(node + 0x58);
		x::MAG_022_sub_8DD8A0(node + 0x58, (uint32_t)(int32_t)S16(caster, 0xE));
		const int32_t r = (int32_t)x::CrtRand() % 3;
		if (r == 0)
			x::queueChainTransformationConditional(target, 4);
		else if (r == 1)
			x::queueChainTransformationConditional(target, 5);
		else if (r == 2)
			x::queueChainTransformationConditional(target, 6);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x80C2A0 (sub_80C2A0): splash state 1 - every other tick (target slot + tick odd) one splash
	// task 0x80C430 of two drops at random bones of the target (0x80C3D0), each thrown back along
	// the matrix +0x58 turned by a random yaw (+-0x200) and pitch (0xE00..0xFFF), speed 0x400..0xBFF;
	// at tick 0x12 finished, next state
	uint32_t __cdecl ab_80C2A0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (((U8(node, 0x2D) + U8(node, 0x24)) & 1) != 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_SplashDrops, 0x154, node);
			ab_80C3D0(ENT(U8(node, 0x2D)), t + 0x80, 2);
			uint32_t vel = t + 0xA0;
			for (int32_t n = 2; n != 0; n--, vel += 8)
			{
				const uint32_t r1 = x::CrtRand();
				const int32_t pitch = (int32_t)(r1 & 0x1FF) + 0xE00;
				const uint32_t r2 = x::CrtRand();
				const int16_t yaw = (int16_t)((r2 & 0x3FF) - 0x200);
				alignas(4) uint8_t m[0x20];
				alignas(4) uint16_t v[4];
				memcpy(m, (const void *)(node + 0x58), 0x20); // rep movsd
				x::MAG_022_sub_8DD8A0(P(m), (uint32_t)(int32_t)yaw);
				x::sub_8DD7E0(P(m), (uint32_t)(int32_t)(int16_t)pitch);
				v[0] = 0;
				v[1] = 0;
				const uint32_t r3 = x::CrtRand();
				v[2] = (uint16_t)(0xFFFFFC00u - (r3 & 0x7FF));
				v[3] = 0;   // (pad, never written by the original: only the GTE VZ0 high half, unused)
				x::GTE_SetRotMatrix(P(m));
				x::GTE_LoadV0(P(v));
				x::GTE_MVMVA_RotV0();
				x::GTE_StoreIR123(vel);
			}
		}
		if (S16(node, 0x24) >= 0x12)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x80C3D0 (sub_80C3D0): a3 (s16) positions at a2 (8 bytes apart): spawn points of entity a1 at a
	// random bone (rand % the bone count [[a1 + 0x64]]) with a random flag word (rand & 0xFFF)
	uint32_t __cdecl ab_80C3D0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		int32_t n = (int16_t)a3;
		const int32_t bones = (int16_t)(uint16_t)U8(U32(U32(a1, 0x64), 0), 0);
		if (n <= 0)
			return 0; // void
		uint32_t out = a2;
		for (; n != 0; n--, out += 8)
		{
			const int32_t r1 = (int32_t)x::CrtRand();
			const int32_t bone = r1 % bones;
			const uint32_t r2 = x::CrtRand();
			x::GetEffectSpawnPosition(a1, (uint32_t)(int32_t)(int16_t)bone, (uint32_t)(int32_t)(int16_t)(r2 & 0xFFF), out);
		}
		return 0; // void
	}

	// the drop physics of the splash / rain tasks: velocity (s16 x 3 at vel) damped by 1/8, the
	// position (s16 x 3 at vel - 0x20) moved by velocity / 16
	static inline void drop_step(uint32_t vel)
	{
		for (int k = 0; k < 3; k++)
		{
			const int16_t c = S16(vel, 2 * k);
			const int32_t e = c;
			U16(vel, 2 * k) = (uint16_t)(c - (uint16_t)((e + ((e >> 31) & 7)) >> 3));
		}
		for (int k = 0; k < 3; k++)
		{
			const int32_t e = S16(vel, 2 * k);
			U16(vel, -0x20 + 2 * k) = (uint16_t)(U16(vel, -0x20 + 2 * k) + (uint16_t)((e + ((e >> 31) & 0xF)) >> 4));
		}
	}

	// 0x80C430 (sub_80C430): SPLASH DROPS task (two drops) - state {0x80C540 set-up, 0x80C560 the
	// sprite animation, ret}; each drop: its position (+0x80 + 8k) -> +0x1C, drawn (0x80BA30), then
	// gravity (velocity +0xA0 + 8k y += 0x50) and the drop physics
	uint32_t __cdecl ab_80C430(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x80C540;
		states[1] = 0x80C560;
		states[2] = 0x80C580; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		uint32_t vel = node + 0xA0;
		for (int32_t n = 2; n != 0; n--, vel += 8)
		{
			U32(node, 0x1C) = U32(vel, -0x20);
			U32(node, 0x20) = U32(vel, -0x1C);
			// 30 fps layer: see mag055_aqua_breath_held.inc
			FX_HELD(held_note_drop(node, 2 - n);)
			ab_80BA30(node);
			if ((U8(node, 0x26) & 4) == 0)
				g_pad_residue = (uint16_t)((MEM<uint32_t>(FALLOC_PTR) + 0xCC) >> 16); // (see orb_vertex_pad)
			U16(vel, 2) = (uint16_t)(U16(vel, 2) + 0x50);
			drop_step(vel);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x80C540 (sub_80C540): splash drops state 0 - sprite 0x15A3E5C, depth +0x56 = -0x40, last frame
	// +0x52 = 0xF, next state
	uint32_t __cdecl ab_80C540(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x15A3E5C;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 0xF;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x80C600 (sub_80C600): rain state 0 - from tick 0x14: its matrix +0x58 = the caster's facing,
	// next state
	uint32_t __cdecl ab_80C600(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x14)
			return 0; // void
		const uint32_t ent = ENT(U8(node, 0x2C));
		x::MAG_022_sub_8DD770(node + 0x58);
		x::MAG_022_sub_8DD8A0(node + 0x58, (uint32_t)(int32_t)S16(ent, 0xE));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x80C650 (sub_80C650): rain state 1 - every other tick one rain task 0x80C7B0 of four drops at
	// random bones of the target (0x80C3D0) moved by a random offset (x +-0x200, y -0x300..+0xFF, z
	// +-0x100), each thrown along the matrix +0x58 turned by a random yaw and pitch (0..0xFFF), speed
	// 0..0x3FF; at tick 0x1C finished, next state
	uint32_t __cdecl ab_80C650(uint32_t a1)
	{
		const uint32_t node = a1;
		if (((U8(node, 0x2D) + U8(node, 0x24)) & 1) != 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_RainDrops, 0x154, node);
			ab_80C3D0(ENT(U8(node, 0x2D)), t + 0x80, 4);
			uint32_t pos = t + 0x80;
			for (int32_t n = 4; n != 0; n--, pos += 8)
			{
				const uint32_t r1 = x::CrtRand();
				U16(pos, 0) = (uint16_t)(U16(pos, 0) + (uint16_t)((r1 & 0x3FF) - 0x200));
				const uint32_t r2 = x::CrtRand();
				U16(pos, 2) = (uint16_t)(U16(pos, 2) + (uint16_t)((r2 & 0x3FF) - 0x300));
				const uint32_t r3 = x::CrtRand();
				U16(pos, 4) = (uint16_t)(U16(pos, 4) + (uint16_t)((r3 & 0x1FF) - 0x100));
				const uint32_t r4 = x::CrtRand();
				const int16_t pitch = (int16_t)(r4 & 0xFFF);
				const uint32_t r5 = x::CrtRand();
				const int16_t yaw = (int16_t)(r5 & 0xFFF);
				alignas(4) uint8_t m[0x20];
				alignas(4) uint16_t v[4];
				memcpy(m, (const void *)(node + 0x58), 0x20); // rep movsd
				x::MAG_022_sub_8DD8A0(P(m), (uint32_t)(int32_t)yaw);
				x::sub_8DD7E0(P(m), (uint32_t)(int32_t)pitch);
				v[0] = 0;
				v[1] = 0;
				const uint32_t r6 = x::CrtRand();
				v[2] = (uint16_t)(0u - (r6 & 0x3FF));
				v[3] = 0;   // (pad, never written by the original: only the GTE VZ0 high half, unused)
				x::GTE_SetRotMatrix(P(m));
				x::GTE_LoadV0(P(v));
				x::GTE_MVMVA_RotV0();
				x::GTE_StoreIR123(pos + 0x20);
			}
		}
		if (S16(node, 0x24) >= 0x1C)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x80C7B0 (sub_80C7B0): RAIN DROPS task (four drops) - state {0x80C8C0 set-up, 0x80C8E0 the sprite
	// animation, ret}; each drop: its position (+0x80 + 8k) -> +0x1C, sprite [0x15A75DC + 4k], drawn
	// (0x80BA30), then the drop physics (no gravity)
	uint32_t __cdecl ab_80C7B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x80C8C0;
		states[1] = 0x80C8E0;
		states[2] = 0x80C900; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		uint32_t vel = node + 0xA0;
		for (uint32_t tbl = RAIN_SPRITES; tbl < RAIN_SPRITES + 0x10; tbl += 4, vel += 8)
		{
			U32(node, 0x1C) = U32(vel, -0x20);
			U32(node, 0x20) = U32(vel, -0x1C);
			U32(node, 0x4C) = MEM<uint32_t>(tbl);
			// 30 fps layer: see mag055_aqua_breath_held.inc
			FX_HELD(held_note_drop(node, (int)((tbl - RAIN_SPRITES) / 4));)
			ab_80BA30(node);
			if ((U8(node, 0x26) & 4) == 0)
				g_pad_residue = (uint16_t)((MEM<uint32_t>(FALLOC_PTR) + 0xCC) >> 16); // (see orb_vertex_pad)
			drop_step(vel);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x80C8C0 (sub_80C8C0): rain drops state 0 - sprite 0x15A3B9C (replaced per drop), depth +0x56 =
	// -0x10, last frame +0x52 = 0xC, next state
	uint32_t __cdecl ab_80C8C0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x15A3B9C;
		U16(a1, 0x56) = 0xFFF0;
		U16(a1, 0x52) = 0xC;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// ====================================================================================
	// the library functions of this module whose code matches a Resonance / Granaldo / Doom /
	// Drink Magic / Curse / Stare / Tonberry / Siren port up to the module's
	// addresses and callees (the port's text with the module's addresses; the comments describe the
	// source original)
	// ====================================================================================

	// 0x821690 (MAG_049_sub_821690; Protect's master 0x86BB00 with this module's table and queues):
	// MASTER task - camera copy, packet arena by tick parity (cursor 0x26464DC = file + 0 / + 0xC000,
	// 0x26465DC = file + 0xC000 / + 0x18000), bone follow, 11-state table {0x8217E0, 0x8217F0
	// (a_73A0D0), 0x821800 wait, 0x821810 emitters (a_73A170), 0x824C00 .. 0x824CA0 (the engine's
	// end states), ret}, the five queues (live task count -> node +0x5E)
	uint32_t __cdecl ab_808970(uint32_t a1)
	{
		g_mod = &MOD_055;
		// 30 fps layer: see mag055_aqua_breath_held.inc
		FX_HELD(held_note_tick();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x2616410) = 0x2793E58;
		MEM<uint32_t>(0x26110B0) = 0x2793E58;
		states[0] = 0x808AC0;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x808AD0;
		states[2] = 0x808AE0;
		states[3] = 0x808AF0;
		states[4] = 0x80C9E0;
		states[5] = 0x80CA20;
		states[6] = 0x80CA30;
		states[7] = 0x80CA40;
		states[8] = 0x80CA50;
		states[9] = 0x80CA60;
		states[10] = 0x80CA80;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26159AC);
			const uint32_t v2 = MEM<uint32_t>(0x26155A4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26110AC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26159A8);
			const uint32_t v2 = MEM<uint32_t>(0x26155A0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26110AC) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_TASKS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ORB));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SPARE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl ab_808AE0(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8218C0 (MAG_049_sub_8218C0; Psycho Blast 0x893450 = Granaldo 0x8A36A0): emitter state 0 - sets up the
	// director block (0x8218F0), spawns the director 0x821970 (0x48 bytes, director queue), next state
	uint32_t __cdecl ab_808BA0(uint32_t a1)
	{
		ab_808BD0(a1);
		x::Effect_AddTaskAndInitFromCtx(0x2611088, 0x808DD0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x85E6C0 (sub_85E6C0): the largest ground distance (x / z, 0x56BEC0) from the entity +0x2C to the
	// targets of the node's action
	uint32_t __cdecl ab_808D10(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		int32_t best = 0;
		uint32_t act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
		if (U8(act, 0x10) == 0)
			return 0;
		for (uint32_t i = 0; ; )
		{
			const uint32_t t = 0x1D972C0 + (uint32_t)U8(U32(act, 8), (int32_t)(i * 0x18)) * 0x9C;
			const int32_t dx = sub32(S16(t, 0x1C), S16(ent, 0x1C));
			const int32_t dz = sub32(S16(t, 0x20), S16(ent, 0x20));
			const int32_t d = (int32_t)x::Sqrt((uint32_t)add32(mul32(dz, dz), mul32(dx, dx)));
			if (best < d)
				best = d;
			i++;
			act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
			if ((int32_t)i >= (int32_t)U8(act, 0x10))
				break;
		}
		return (uint32_t)best;
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ab_808E50(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ab_809150(a1);
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

	// 0x839740 (sub_839740; = modules 047 / 055 / 072): stage darkening state - level +0x1C +=
	// 0x200 up to 0x600 (then next state); writes the level to the 4 words 0x1D98992 + k * 0x2C
	uint32_t __cdecl ab_808F80(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
		if (S16(a1, 0x1C) >= 0x600)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x839780 (sub_839780; = modules 055 / 072): stage darkening state - holds until tick 0x29,
	// next state
	uint32_t __cdecl ab_808FC0(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x28)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl ab_808FD0(uint32_t a1)
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

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl ab_809080(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x15A38D8, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x82B460 (sub_82B460): director state - after phase tick 0x32 the damage cue (phase data
	// +0x48 = 1), next state
	uint32_t __cdecl ab_8090D0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		if (S16(d, 0x46) > 0x32)
		{
			U16(d, 0x48) = 1;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x85C350 (sub_85C350): orb draw, unless hidden (+0x26 bit 2) - matrix rotated by +0x46 (y), +0x44
	// (x), +0x48 (z) at +0x1C, scaled by +0x30.. (0x56BEF0), composed with the battle camera 0x1D97778 into
	// the GTE; render header (model +0x4C, colour +0x40, fade +0x50, +0x52, texture offset +0x5E, 0xF0,
	// scales 0x100) through the module's prim-model renderer 0x85C480 into the frame arena 0x1D8E054
	uint32_t __cdecl ab_809300(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		alignas(4) uint8_t m[0x20];
		x::MAG_022_sub_8DD770(P(m));
		x::MAG_022_sub_8DD8A0(P(m), (uint32_t)(int32_t)S16(node, 0x46));
		x::sub_8DD7E0(P(m), (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD960(P(m), (uint32_t)(int32_t)S16(node, 0x48));
		S32(P(m), 0x14) = S16(node, 0x1C);
		S32(P(m), 0x18) = S16(node, 0x1E);
		S32(P(m), 0x1C) = S16(node, 0x20);
		x::scale3DMatrix(P(m), node + 0x30);
		x::ComposeAffineTransform(0x1D97778, P(m), P(m));
		x::GTE_SetRotMatrix_W(P(m));
		x::GTE_SetTransVector_W(P(m));
		const uint32_t hdr = x::Field_Alloc(0x68);
		U32(hdr, 0) = U32(node, 0x4C);
		U32(hdr, 8) = U32(node, 0x40);
		S32(hdr, 0xC) = S16(node, 0x50);
		U16(hdr, 0x18) = U16(node, 0x5E);
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x24) = 0;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		S32(hdr, 0x10) = S16(node, 0x52);
		U32(hdr, 0x14) = 0xF0;
		U16(hdr, 0x28) = 0x80;
		MEM<uint32_t>(0x1D8E054) = ab_809430(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x8A4830 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8A4960, 0x8A4B80, 0x8A4E10, 0x8A5290, a_73D770, a_73D9C0, 0x8A5D40,
	// 0x8A6240; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl ab_809430(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { ab_809560, ab_809780, ab_809A10, ab_809E90, a_73D770, a_73D9C0, ab_80A940, ab_80AE40 };
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
	uint32_t __cdecl ab_809560(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ab_809780(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A4E10 (copy of s_73CD70 0x73CD70: module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl ab_809A10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		// field pointers of the current packet (the original keeps them in stack slots; always pkt + const)
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x7000000;  // tag: 7 words
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;  // semi-transparent on
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;  // semi-transparent off
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
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_AVSZ3();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				if ((clip & 7) == 7)
					goto next;
				if ((clip & 0x70) == 0x70)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				// depth cue of the packet colour towards the far colour
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x20;
					pkt += 0x20;
					pSXY1 += 0x20;
					pSXY2 += 0x20;
					pCode += 0x20;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0x1C) = (uint8_t)c2;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x73CFDF)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x1D) = (uint8_t)c2;
						}
					}
					const uint32_t poly = pkt;
					pSXY1 += 0x2C;
					pSXY2 += 0x2C;
					pCode += 0x2C;
					pSXY0 += 0x2C;
					uint32_t prim = pkt + 0x20;
					pkt += 0x2C;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY2 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
			}
		next:
			rec += 0x14;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8A5290 (copy of s_73D1F0 0x73D1F0: module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl ab_809E90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		uint32_t pSXY3 = pkt + 0x20;
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;  // tag: 9 words
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;
				U32(pkt, 0xC) = U32(rec, 0xC);    // uv0 + clut
				const uint32_t uv1 = U32(rec, 0x10);
				const uint32_t uv23 = U32(rec, 0x14);
				U32(pkt, 0x1C) = uv23;             // uv2 (+ uv3 in the pad half)
				U32(pkt, 0x14) = uv1;              // uv1 + tpage
				U32(pkt, 0x24) = uv23 >> 16;       // uv3
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
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				x::GTE_ReadSXY2(pSXY3);
				x::GTE_AVSZ4();
				v = S16(pSXY3, 0);
				if (v < 0 || v > 0xA00)
					clip |= 8;
				v = S16(pkt, 0x22);
				if (v < 0 || v > 0x6C0)
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x28;
					pSXY1 += 0x28;
					pkt += 0x28;
					pSXY2 += 0x28;
					pSXY3 += 0x28;
					pCode += 0x28;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c3 = U8(pkt, 0x24) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
							U8(pkt, 0x24) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x1C) = (uint8_t)c2;
							U8(pkt, 0x24) = (uint8_t)c3;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x73D508)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c3 = U8(pkt, 0x25) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
							U8(pkt, 0x25) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x1D) = (uint8_t)c2;
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x25) = (uint8_t)c3;
						}
					}
					pSXY0 += 0x34;
					pSXY3 += 0x34;
					pSXY2 += 0x34;
					const uint32_t poly = pkt;
					pCode += 0x34;
					pSXY1 += 0x34;
					uint32_t prim = pkt + 0x28;
					pkt += 0x34;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					pSXY2 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY3 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
			}
		next:
			rec += 0x18;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8A5D40 (sub_8A5D40; same code as Siren's 0x73DCA0 / Drink Magic's 0x85D990): prim-list block
	// "Gouraud-textured triangles, UV scroll" of the module renderer 0x8A4830: 0x1C-byte records
	// (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C, uv1+tpage +0x10, rgb1
	// +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face (NCLIP, unless ctx+0x14
	// bit 0x20) and off-screen culling, optional lighting (bit 0x80), z = OTZ + ctx+0x10 (>= 0) >>
	// a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the UVs are scrolled (wrapped by
	// ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window packets (ctx+0x1C set, ctx+0x24
	// restore): 0x40 bytes; else the poly alone (0x28 bytes; unlike Tonberry's 0x765B30, no
	// draw-mode packet). The original keeps seven packet-field pointers apart from the cursor and
	// advances them together (always cursor + fixed offsets). Returns the new packet cursor; ctx+0x2C
	// advances past the block.
	uint32_t __cdecl ab_80A940(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		static const int32_t U_OFF[3] = { 0x0C, 0x18, 0x24 };
		static const int32_t V_OFF[3] = { 0x0D, 0x19, 0x25 };
		const uint32_t ctx = a1;
		uint32_t pkt = a4;                              // esi
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;                        // ebp / [esp+0x28]
		const uint32_t vbase = U32(ctx, 4);             // (in the a4 slot)
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
						const uint32_t ot = a2 + (uint32_t)z * 4;   // [esp+0x24]
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
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							pkt += 0x28;
						}
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x8A6240 (copy of s_73E1A0 0x73E1A0: module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl ab_80AE40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t data = U32(ctx, 0x2C);
		int32_t count = S32(data, 0);
		data += 4;
		// quirk 0x73E1C2: the vertex base overwrites the caller's a4 argument slot and 0x73E299
		// reuses the a1 slot as the clip-flag local (caller stack only, not reproducible/needed)
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = data;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = data;
			return pkt;
		}
		do
		{
			pkt = s3_quad(ctx, data, vbase, a2, a3, pkt);
			data += 0x24;
		} while (--count);
		U32(ctx, 0x2C) = data;
		return pkt;
	}

	// 0x85E860 (sub_85E860): orb state 3 - at the caster for +0x5C ticks, next state
	uint32_t __cdecl ab_80B6C0(uint32_t a1)
	{
		ab_80B5E0(a1 + 0x1C, (uint32_t)U8(a1, 0x2C));
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
		if (S16(a1, 0x5C) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x745610 (module 095): +0x50 += 0x200; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl ab_80B6F0(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) >= 0x1000)
		{
			int8_t st = S8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x50) = 0x1000;
			S8(a1, 0x29) = (int8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x8AA2E0 (copy of s_7434C0 0x7434C0: module Siren, sub_7434C0): draws one sprite of a sparkle node (unless +0x26 bit 2):
	// 0xD8-byte scratch header, camera matrix (unpacked, rotated by itself) projects the node
	// position +0x1C/+0x1E/+0x20 (-> header +0xCC), sprite data ptr +0x4C, frame +0x50, depth
	// +0x56 -> a_7435E0(header, OT+0x44, 2, packet cursor), cursor updated
	uint32_t __cdecl ab_80BA30(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		uint32_t hdr = x::Field_Alloc(0xD8);
		uint32_t m = hdr + 0xB8;
		x::UnpackRotationMatrix(0x1D97778, m);
		int32_t px = S16(a1, 0x1C);
		int32_t py = S16(a1, 0x1E);
		int32_t pz = S16(a1, 0x20);
		uint32_t vec = hdr + 0xCC;
		S32(hdr, 0xD0) = py;
		S32(hdr, 0xD4) = pz;
		S32(vec, 0) = px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(m);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(m);
		x::GTE_LoadIRFromMatrixColumn(hdr + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(hdr + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(hdr + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(hdr + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(vec);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(vec);
		x::GTE_SetRotMatrix(m);
		x::GTE_SetTransVector(m);
		uint32_t data = U32(a1, 0x4C);
		uint16_t frame = U16(a1, 0x50);
		uint16_t depth = U16(a1, 0x56);
		U32(hdr, 0) = data;
		uint32_t cursor = U32(0x1D8E054, 0);
		U16(hdr, 4) = frame;
		uint32_t ot = U32(0x1D8E04C, 0) + 0x44;
		U16(hdr, 0x24) = 0;
		U16(hdr, 0xB4) = depth;
		U32(0x1D8E054, 0) = a_7435E0(hdr, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ab_80C140(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ab_80C560(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ab_80C8E0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// ---- generated (gendesc.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x808AC0, (void *)a_73A0D0, "055 MAG_055_sub_808AC0" },
		{ 0x808AD0, (void *)a_73A0D0, "055 MAG_055_sub_808AD0" },
		{ 0x808AF0, (void *)a_73A170, "055 MAG_055_sub_808AF0" },
		{ 0x808B20, (void *)a_73A1A0, "055 MAG_055_sub_808B20" },
		{ 0x808C80, (void *)a_73A720, "055 sub_808C80" },
		{ 0x808ED0, (void *)a_73F110, "055 sub_808ED0" },
		{ 0x808F40, (void *)a_7335D0, "055 sub_808F40" },
		{ 0x809010, (void *)a_73A6D0, "055 nullsub_1501" },
		{ 0x809050, (void *)a_73B640, "055 sub_809050" },
		{ 0x8090B0, (void *)a_73B7E0, "055 sub_8090B0" },
		{ 0x8090F0, (void *)a_73B8A0, "055 sub_8090F0" },
		{ 0x809110, (void *)a_7475A0, "055 sub_809110" },
		{ 0x809130, (void *)a_73A6D0, "055 nullsub_1502" },
		{ 0x809140, (void *)a_73A6D0, "055 nullsub_1503" },
		{ 0x80A410, (void *)a_73D770, "055 sub_80A410" },
		{ 0x80A660, (void *)a_73D9C0, "055 sub_80A660" },
		{ 0x80B720, (void *)a_73A6D0, "055 nullsub_1504" },
		{ 0x80B730, (void *)a_73A380, "055 sub_80B730" },
		{ 0x80BB50, (void *)a_7435E0, "055 InitEffectSequenceFromData_c13" },
		{ 0x80BC90, (void *)a_743720, "055 sub_80BC90" },
		{ 0x80C160, (void *)a_743C20, "055 sub_80C160" },
		{ 0x80C190, (void *)a_73A6D0, "055 nullsub_1505" },
		{ 0x80C1A0, (void *)a_73A6D0, "055 nullsub_1506" },
		{ 0x80C1B0, (void *)a_73A380, "055 sub_80C1B0" },
		{ 0x80C580, (void *)a_73A6D0, "055 nullsub_1507" },
		{ 0x80C590, (void *)a_73A6D0, "055 nullsub_1508" },
		{ 0x80C5A0, (void *)a_73A380, "055 sub_80C5A0" },
		{ 0x80C900, (void *)a_73A6D0, "055 nullsub_1509" },
		{ 0x80C910, (void *)a_73A6D0, "055 nullsub_1510" },
		{ 0x80C920, (void *)a_747440, "055 MAG_055_sub_80C920" },
		{ 0x80C990, (void *)a_7474B0, "055 MAG_055_sub_80C990" },
		{ 0x80C9B0, (void *)a_7474D0, "055 MAG_055_sub_80C9B0" },
		{ 0x80C9D0, (void *)a_73A6D0, "055 nullsub_1511" },
		{ 0x80C9E0, (void *)a_747500, "055 MAG_055_sub_80C9E0" },
		{ 0x80CA20, (void *)a_73A0D0, "055 MAG_055_sub_80CA20" },
		{ 0x80CA30, (void *)a_747550, "055 MAG_055_sub_80CA30" },
		{ 0x80CA40, (void *)a_73A0D0, "055 MAG_055_sub_80CA40" },
		{ 0x80CA50, (void *)a_73A0D0, "055 MAG_055_sub_80CA50" },
		{ 0x80CA60, (void *)a_7475A0, "055 MAG_055_sub_80CA60" },
		{ 0x80CA80, (void *)a_73A6D0, "055 nullsub_1500" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x808970, (void *)ab_808970, "055 ab_808970" },
		{ 0x808AE0, (void *)ab_808AE0, "055 ab_808AE0" },
		{ 0x808BA0, (void *)ab_808BA0, "055 ab_808BA0" },
		{ 0x808BD0, (void *)ab_808BD0, "055 ab_808BD0" },
		{ 0x808D10, (void *)ab_808D10, "055 ab_808D10" },
		{ 0x808DD0, (void *)ab_808DD0, "055 ab_808DD0" },
		{ 0x808E50, (void *)ab_808E50, "055 ab_808E50" },
		{ 0x808EA0, (void *)ab_808EA0, "055 ab_808EA0" },
		{ 0x808F80, (void *)ab_808F80, "055 ab_808F80" },
		{ 0x808FC0, (void *)ab_808FC0, "055 ab_808FC0" },
		{ 0x808FD0, (void *)ab_808FD0, "055 ab_808FD0" },
		{ 0x809020, (void *)ab_809020, "055 ab_809020" },
		{ 0x809080, (void *)ab_809080, "055 ab_809080" },
		{ 0x8090D0, (void *)ab_8090D0, "055 ab_8090D0" },
		{ 0x809150, (void *)ab_809150, "055 ab_809150" },
		{ 0x809250, (void *)ab_809250, "055 ab_809250" },
		{ 0x809300, (void *)ab_809300, "055 ab_809300" },
		{ 0x809430, (void *)ab_809430, "055 ab_809430" },
		{ 0x809560, (void *)ab_809560, "055 ab_809560" },
		{ 0x809780, (void *)ab_809780, "055 ab_809780" },
		{ 0x809A10, (void *)ab_809A10, "055 ab_809A10" },
		{ 0x809E90, (void *)ab_809E90, "055 ab_809E90" },
		{ 0x80A940, (void *)ab_80A940, "055 ab_80A940" },
		{ 0x80AE40, (void *)ab_80AE40, "055 ab_80AE40" },
		{ 0x80B490, (void *)ab_80B490, "055 ab_80B490" },
		{ 0x80B580, (void *)ab_80B580, "055 ab_80B580" },
		{ 0x80B5E0, (void *)ab_80B5E0, "055 ab_80B5E0" },
		{ 0x80B680, (void *)ab_80B680, "055 ab_80B680" },
		{ 0x80B6C0, (void *)ab_80B6C0, "055 ab_80B6C0" },
		{ 0x80B6F0, (void *)ab_80B6F0, "055 ab_80B6F0" },
		{ 0x80B790, (void *)ab_80B790, "055 ab_80B790" },
		{ 0x80B7D0, (void *)ab_80B7D0, "055 ab_80B7D0" },
		{ 0x80B8E0, (void *)ab_80B8E0, "055 ab_80B8E0" },
		{ 0x80BA30, (void *)ab_80BA30, "055 ab_80BA30" },
		{ 0x80C100, (void *)ab_80C100, "055 ab_80C100" },
		{ 0x80C140, (void *)ab_80C140, "055 ab_80C140" },
		{ 0x80C210, (void *)ab_80C210, "055 ab_80C210" },
		{ 0x80C2A0, (void *)ab_80C2A0, "055 ab_80C2A0" },
		{ 0x80C3D0, (void *)ab_80C3D0, "055 ab_80C3D0" },
		{ 0x80C430, (void *)ab_80C430, "055 ab_80C430" },
		{ 0x80C540, (void *)ab_80C540, "055 ab_80C540" },
		{ 0x80C560, (void *)ab_80C560, "055 ab_80C560" },
		{ 0x80C600, (void *)ab_80C600, "055 ab_80C600" },
		{ 0x80C650, (void *)ab_80C650, "055 ab_80C650" },
		{ 0x80C7B0, (void *)ab_80C7B0, "055 ab_80C7B0" },
		{ 0x80C8C0, (void *)ab_80C8C0, "055 ab_80C8C0" },
		{ 0x80C8E0, (void *)ab_80C8E0, "055 ab_80C8E0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag055_aqua_breath()
	{
		act::register_module(55);
		for (const act::aqua::ModPort *p = act::aqua::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(55, p->addr, p->port, p->name);
		for (const act::aqua::ModPort *p = act::aqua::PORTS; p->addr; p++)
			act::register_module_port(55, p->addr, p->port, p->name);
		// 30 fps layer: see mag055_aqua_breath_held.inc
		FX_HELD(register_mag055_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag055_aqua_breath_held.inc"
#endif
