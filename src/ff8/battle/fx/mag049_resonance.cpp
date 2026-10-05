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

// Effect 49: Resonance (enemy attack 121 of kernel.bin, used by Iguion c0m093; MAG_049_*; no other
// kernel.bin entry uses effect 49): a copy of the actor/heal effect library (see act_engine.h) built
// like Stare (mag045_stare.cpp: master + director stepping one phase, prim-model tasks drawn by the
// eight primitive-list renderers) without an actor system, plus a full-screen warp and a hit task per
// target. It shares no module code with Magma Breath (48, the same enemy): only its texture file.
//
// Setup MAG_049_RESONANCE 0x821540 (runs once, not ported; file loader MAG_049_RESONANCE_FL 0x821520 =
// the effect's data file 0x15ADC1C (mag048.tim, Magma Breath's texture file) -> 0x26464D8, TIM
// uploaded by the setup, packet arenas at file + 0 / + 0xC000 / + 0x18000) plays the camera animation
// 0x15ADAF8 and creates the root queue 0x26466C0 with the master and five task pools: 0x26475F0
// (4 x 0x58 emitters), 0x26465B8 (3 x 0x48 director), 0x26466D0 (8 x 0x80: the screen warp and the
// hit tasks), 0x26472C0 (2 x 0x368 prim-model tasks) and 0x26475E0 (10 x 0x40, unused).
//   Master (0x821690 = Protect's 0x86BB00) - camera copy 0x2793E58, packet cursor 0x26464DC by tick
//     parity, bone follow, 11-state table: 0x821800 waits for the children, 0x821810 one emitter per
//     action (a_73A170), then the engine's end states.
//   Emitter (0x821840 = the engine's a_73A1A0): state 0x8218C0 sets up the director block
//     [0x15ADAF0] = 0x26478A0 (0x8218F0: the target's position, the facing towards it) and spawns
//     the director; 0x824B90 waits for the director's cue +0x48.
//   Director (0x821970 = Stare's 0x836AC0 table shape): phase bookkeeping 0x8219E0 (a new phase
//     spawns its tasks: 0x821B30), the cue to phase 1 (a_73B790), director cue 1 (0x821A80), the
//     sound 0x15ADAF4 0xE ticks into the phase (0x821AC0), the cue +0x48 = 1 and finish (0x821AF0).
//   Phase 1 (0x821B30): the screen warp 0x824260, two prim-model tasks (0x821C00: 0x821C50 with
//     layout 0x17B329C at slot 3, 0x824140 with layout 0x17B656C at slot 4; both placed at their
//     tick 0x14 at the anchor 0xF0 of that entity, 0x821CB0 / 0x8241A0) and one hit task 0x824970
//     per target of the action.
//   Prim-model tasks (node 0x368): the prim-model player (0x821D40 -> 0x701970, callback 0x821E70,
//     vertex blend buffer 0x2647D20) drawn by the module renderer 0x8220B0 (= Granaldo's eight
//     primitive-list renderers) into the module arena (cursor 0x26464DC).
//   Screen warp (0x824260, node 0x80): from its tick 0x20 the scene is drawn into the VRAM area
//     (0x240, 0x100) 320 x 240 (draw environments of the current buffer 0x824410: off-screen at OT
//     +0x4484, back to the screen at OT +0x14) and copied back as two passes (semi-transparent, then
//     opaque) of a 10 x 7 grid of textured quads (0x8245B0, frame arena 0x1D8E054, OT entry 5) whose
//     inner vertices wave by sine tables (0x8246D0: random phase steps; x waves 0x26466E0, y waves
//     0x26467B8, grid 0x2647900); amplitude [0x15ADC18] = 0x26466B0 ramps 0 -> 0x1000 by 0x100,
//     holds to its tick 0x39, back to 0 by 0x80.
//   Hit task (0x824970 = Siren's 0x73F6A0 body): the target's model plays animation 4 or 5 (rand)
//     0x1C ticks into the phase, then 6, and once 6 has ended (after phase tick 0x4B) the damage of
//     that target (0x506690).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26464D8..0x26480F0 (file pointer 0x26464D8, packet cursor 0x26464DC, second
// arena cursor 0x26465DC, pools, queues, arenas, warp block 0x26466B0, wave tables 0x26466E0 /
// 0x26467B8, draw environments 0x2646700 / 0x2647760 and their packets 0x26472D8 / 0x2647820, grid
// 0x2647900, blend buffer 0x2647D20, director block 0x26478A0); the pointers 0x15ADAF0 / 0x15ADC18
// (exe data) lead to the director block / the warp block.
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (rs_XXXXXX), the library ones
// the text of the matching Stare / Granaldo / Doom / Curse / Tonberry / Siren port with this
// module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace resonance
{
	static const Mod MOD_049 = { "resonance", 49, 0x821520, 0x824CD0,
		{ 0x0, 0x0, 0x15ADAF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26464D8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26475F0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x821840, 0x8218C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x821A50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x824B90, 0x824BB0, 0x824BD0, 0x824BF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26464DC;   // module packet cursor (tick parity; the prim draws' packets)
	static const uint32_t FRAME_CURSOR = 0x1D8E054;    // frame arena cursor (the screen warp's quads)
	static const uint32_t DIR_PTR = 0x15ADAF0;         // director block pointer (-> 0x26478A0, 0x54 bytes)
	static const uint32_t WARP_PTR = 0x15ADC18;        // warp block pointer (-> 0x26466B0, 0x10 bytes; +0 amplitude)
	static const uint32_t SOUND = 0x15ADAF4;           // the sound of the phase
	static const uint32_t BLEND_BUF = 0x2647D20;       // vertex blend buffer of the prim callback
	static const uint32_t PRIM_CB = 0x821E70;          // the prim-model player's object callback
	static const uint32_t GRID = 0x2647900;            // warp grid (11 x 8 entries of 0xC bytes)
	static const uint32_t XWAVE = 0x26466E0, YWAVE = 0x26467B8; // warp wave tables (2 rows, stride 4)
	static const uint32_t Q_EMITTER = 0x26475F0, Q_DIRECTOR = 0x26465B8, Q_MISC = 0x26466D0, Q_PRIM = 0x26472C0, Q_TINT = 0x26475E0;
	static const uint32_t ORIG_Master = 0x821690;
	static const uint32_t ORIG_Warp = 0x824260;        // screen warp task
	static const uint32_t ORIG_Hit = 0x824970;         // hit task (one per target)
	static const uint32_t ORIG_Prim1 = 0x821C50, ORIG_Prim2 = 0x824140; // prim-model tasks (engine task a_73A380)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
	namespace rx
	{
		// SetDrawEnv: the draw-environment packet a1 from the DRAWENV a2
		static inline uint32_t SetDrawEnv(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(a1, a2); }
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

	// director block and state helpers of the Tonberry / Death copies (the module's director block [DIR_PTR])
	static inline uint32_t t1_data() { return MEM<uint32_t>(DIR_PTR); }
	static inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	static inline void next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl rs_821690(uint32_t a1);
	uint32_t __cdecl rs_821800(uint32_t a1);
	uint32_t __cdecl rs_8218C0(uint32_t a1);
	uint32_t __cdecl rs_8218F0(uint32_t a1);
	uint32_t __cdecl rs_821970(uint32_t a1);
	uint32_t __cdecl rs_8219E0(uint32_t a1);
	uint32_t __cdecl rs_821A80(uint32_t a1);
	uint32_t __cdecl rs_821AC0(uint32_t a1);
	uint32_t __cdecl rs_821AF0(uint32_t a1);
	uint32_t __cdecl rs_821B30(uint32_t a1);
	uint32_t __cdecl rs_821C00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl rs_821CB0(uint32_t a1);
	uint32_t __cdecl rs_821D40(uint32_t a1);
	uint32_t __cdecl rs_821E70(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rs_8220B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_8221E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_822400(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_822690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_822B10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_8235C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_823AC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rs_824110(uint32_t a1);
	uint32_t __cdecl rs_8241A0(uint32_t a1);
	uint32_t __cdecl rs_824230(uint32_t a1);
	uint32_t __cdecl rs_824260(uint32_t a1);
	uint32_t __cdecl rs_8242E0(uint32_t a1);
	uint32_t __cdecl rs_824300();
	uint32_t __cdecl rs_8243B0(uint32_t a1);
	uint32_t __cdecl rs_824410();
	uint32_t __cdecl rs_8244E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rs_824520(uint32_t a1);
	uint32_t __cdecl rs_8245B0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rs_8246D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rs_8248A0(uint32_t a1);
	uint32_t __cdecl rs_8248E0(uint32_t a1);
	uint32_t __cdecl rs_824900(uint32_t a1);
	uint32_t __cdecl rs_824970(uint32_t a1);
	uint32_t __cdecl rs_8249E0(uint32_t a1);
	uint32_t __cdecl rs_824A10(uint32_t a1);
	uint32_t __cdecl rs_824A70(uint32_t a1);
	uint32_t __cdecl rs_824AB0(uint32_t a1);
	uint32_t __cdecl rs_824AE0(uint32_t a1);
	uint32_t __cdecl rs_824B00(uint32_t a1);
	uint32_t __cdecl rs_824B20(uint32_t a1);
	uint32_t __cdecl rs_824B90(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag049_resonance_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace resonance
{
	// ====================================================================================
	// master, director, phase spawn, prim-model placement and draw callback
	// ====================================================================================

	// 0x821690 (MAG_049_sub_821690; Protect's master 0x86BB00 with this module's table and queues):
	// MASTER task - camera copy, packet arena by tick parity (cursor 0x26464DC = file + 0 / + 0xC000,
	// 0x26465DC = file + 0xC000 / + 0x18000), bone follow, 11-state table {0x8217E0, 0x8217F0
	// (a_73A0D0), 0x821800 wait, 0x821810 emitters (a_73A170), 0x824C00 .. 0x824CA0 (the engine's
	// end states), ret}, the five queues (live task count -> node +0x5E)
	uint32_t __cdecl rs_821690(uint32_t a1)
	{
		g_mod = &MOD_049;
		// 30 fps layer: see mag049_resonance_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x26480E4) = 0x2793E58;
		MEM<uint32_t>(0x26465E0) = 0x2793E58;
		states[0] = 0x8217E0;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x8217F0;
		states[2] = 0x821800;
		states[3] = 0x821810;
		states[4] = 0x824C00;
		states[5] = 0x824C40;
		states[6] = 0x824C50;
		states[7] = 0x824C60;
		states[8] = 0x824C90;
		states[9] = 0x824CA0;
		states[10] = 0x824CC0;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x264781C);
			const uint32_t v2 = MEM<uint32_t>(0x26472D4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26465DC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2647818);
			const uint32_t v2 = MEM<uint32_t>(0x26472D0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26465DC) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_MISC));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PRIM));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_TINT));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x821AC0 (sub_821AC0): director state - the sound 0x15ADAF4 (BdPlaySE, volume 0x80) once the
	// phase has run 0xE ticks, next state
	uint32_t __cdecl rs_821AC0(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0xE)
		{
			x::BdPlaySE(SOUND, 0, 0x80);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x821AF0 (sub_821AF0): director state - damage cue (director +0x48 = 1), finished, next state
	uint32_t __cdecl rs_821AF0(uint32_t a1)
	{
		U16(t1_data(), 0x48) = 1;
		U16(a1, 0x26) = (uint16_t)(U16(a1, 0x26) | 1);
		next_state(a1);
		return 0; // void
	}

	// 0x821B30 (sub_821B30): a new director phase spawns its tasks - phase 1: the screen warp task
	// 0x824260 (0x80 B), the two prim-model tasks (0x821C00: 0x821C50 = layout 0x17B329C at the
	// entity of slot 3, 0x824140 = layout 0x17B656C at the entity of slot 4) and one hit task
	// 0x824970 (0x80 B) per target of the current action (+0x2B = target index, +0x2D = its slot)
	uint32_t __cdecl rs_821B30(uint32_t a1)
	{
		if ((int32_t)S16(t1_data(), 0x40) - 1 != 0)
			return 0; // void
		const uint32_t node = a1;
		x::Effect_AddTaskAndInitFromCtx(Q_MISC, ORIG_Warp, 0x80, node);
		const uint32_t p1 = rs_821C00(node, ORIG_Prim1, 0x17B329C, 0x2D4, 0, 0);
		U8(p1, 0x2C) = 3;
		const uint32_t p2 = rs_821C00(node, ORIG_Prim2, 0x17B656C, 0x2C8, 1, 0);
		U8(p2, 0x2C) = 4;
		const uint32_t act0 = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
		if (U8(act0, 0x10) == 0)
			return 0; // void
		int32_t k = 0;
		uint32_t act;
		do
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_MISC, ORIG_Hit, 0x80, node);
			act = U32(U32(node, 0xC), 4) + (uint32_t)((int32_t)S8(node, 0x2A) * 20);
			const uint8_t slot = U8(U32(act, 8), k * 0x18);
			U8(t, 0x2B) = (uint8_t)k;
			U8(t, 0x2D) = slot;
			k++;
		} while (k < (int32_t)U8(act, 0x10));   // the count re-read every pass
		return 0; // void
	}

	// 0x821C00 (sub_821C00): spawns a prim-model task a2 (0x368 B, queue 0x26472C0) from a1:
	// +0x74 = layout a3, +0x78 = (int16) a4, +0x80 = a5, +0x82 = a6; returns it
	uint32_t __cdecl rs_821C00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x368, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x821CB0 / 0x8241A0 (sub_821CB0 / sub_8241A0, the same code): prim-model task state 0 - from its
	// tick 0x14: decodes the layout +0x74 (+0x78) into +0x94, places it at the anchor 0xF0 of the
	// entity of slot +0x2C (scale 0x1000, depth offset +0x88 = -0x80, facing +0x62 = the entity's
	// +0x0E), plays its first frame (0x821D40), next state
	static uint32_t rs_place_and_play(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x14)
			return 0; // void
		const uint32_t ent = ENT(U8(node, 0x2C));
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ent, 0xF0, 0x1000, node + 0x1C);
		const uint16_t facing = U16(ent, 0xE);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFF80;
		U16(node, 0x62) = facing;
		rs_821D40(node);
		next_state(node);
		return 0; // void
	}
	uint32_t __cdecl rs_821CB0(uint32_t a1) { return rs_place_and_play(a1); }
	uint32_t __cdecl rs_8241A0(uint32_t a1) { return rs_place_and_play(a1); }

	// 0x821E70 (sub_821E70): prim-model player draw callback (layout a1, record a2, block a3) - as
	// Curse's 0x82B870 with record flag 0x8000 (instead of 0x200: the position is always turned by the
	// parent; flag 0x8000 skips only the GTE_MatrixMultiply) and the module packet arena: picks the
	// object's vertex frame (lerped by MAG_017_sub_701390 into the block's +0x48 buffer), builds its
	// matrix (rotation 0x701310, parent-rotated position, scale3DMatrix), fills a 0x68-byte Field_Alloc
	// render header (flags 0x2030, 0x20F0 with alpha + colour, depth offset block +0x58, scale words
	// 0x100) and draws it with 0x8220B0 into OT base+0x44 (shift 2) at the module cursor 0x26464DC
	uint32_t __cdecl rs_821E70(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +8 SVECTOR position (+0xE pad unwritten, not read), +0x10 scale (3 x int32), +0x20
		// Mat4x3 object matrix (+0x34 translation)
		alignas(4) uint8_t loc[0x40] = {};
		const uint32_t L = P(loc);
		const uint32_t V = L + 8;
		const uint32_t S = L + 0x10;
		const uint32_t M = L + 0x20;
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

		x::MAG_017_sub_701310(rec + 0x10, M);
		U16(V, 0) = U16(rec, 8);
		U16(V, 2) = U16(rec, 0xA);
		U16(V, 4) = U16(rec, 0xC);
		x::GTE_SetRotMatrix(blk);
		x::GTE_LoadV0(V);
		x::GTE_MVMVA_RotV0();
		x::GTE_ReadMAC123(M + 0x14);
		if ((U32(rec, 4) & 0x8000) == 0)
			x::GTE_MatrixMultiply(blk, M);
		S32(M, 0x14) = add32(S32(M, 0x14), S32(blk, 0x14));
		S32(M, 0x18) = add32(S32(M, 0x18), S32(blk, 0x18));
		S32(M, 0x1C) = add32(S32(M, 0x1C), S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(S, 0) = S16(rec, 0x18);
			S32(S, 4) = S16(rec, 0x1A);
			S32(S, 8) = S16(rec, 0x1C);
			x::scale3DMatrix(M, S);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		U32(hdr, 0x14) = 0x2030;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x14) = 0x20F0;
			U32(hdr, 8) = U32(rec, 0x20);   // colour
		}
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = rs_8220B0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// ====================================================================================
	// the screen warp task (0x824260, node 0x80): the scene is drawn into the VRAM area (0x240,
	// 0x100) 320 x 240 (a draw environment inserted at the far end of the OT) and copied back to
	// the screen as a 10 x 7 grid of textured quads whose inner vertices wave by sine tables
	// ====================================================================================

	// 0x824260 (sub_824260): TASK, screen warp: state table {0x8242E0 wait, 0x8243B0 set up + draw,
	// 0x8248A0 ramp up, 0x8248E0 hold, 0x824900 ramp down, 0x824940 finish (a_7475A0), ret}
	uint32_t __cdecl rs_824260(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x8242E0;
		states[1] = 0x8243B0;
		states[2] = 0x8248A0;
		states[3] = 0x8248E0;
		states[4] = 0x824900;
		states[5] = 0x824940;
		states[6] = 0x824960; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8242E0 (sub_8242E0): warp state 0 - from its tick 0x20: initialises the warp (0x824300), next state
	uint32_t __cdecl rs_8242E0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x20)
		{
			rs_824300();
			next_state(a1);
		}
		return 0; // void
	}

	// 0x824300 (sub_824300): clears the warp block [0x15ADC18] = 0x26466B0 (0x10 bytes; +0 = the
	// amplitude 0..0x1000), +6 = rand & 0x3F + 0x20, +8 / +0xC = rand & 0xFFF (not read by the
	// module), and fills the texture coordinates of the 11 x 8 grid 0x2647900 (12-byte entries
	// {x, y, u0 v0, u1 v0, u0 v1, u1 v1}: column i u = i * 0x20 .. + 0x20 (0x00 -> 0xFF), row j v =
	// j * 0x20 .. + 0x20, 8-bit)
	uint32_t __cdecl rs_824300()
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(WARP_PTR), 0x10);
		const uint32_t r0 = x::CrtRand();
		U16(MEM<uint32_t>(WARP_PTR), 6) = (uint16_t)((r0 & 0x3F) + 0x20);
		const uint32_t r1 = x::CrtRand();
		U16(MEM<uint32_t>(WARP_PTR), 8) = (uint16_t)(r1 & 0xFFF);
		const uint32_t r2 = x::CrtRand();
		U16(MEM<uint32_t>(WARP_PTR), 0xC) = (uint16_t)(r2 & 0xFFF);
		uint32_t e = GRID + 4;
		for (uint32_t i = 0; e < GRID + 4 + 88 * 12; i++)
		{
			const uint8_t u0 = (uint8_t)(i << 5);
			const uint8_t u1 = (uint8_t)(u0 + 0x20);
			for (uint32_t j = 0; j < 8; j++, e += 0xC)
			{
				const uint8_t v0 = (uint8_t)(j << 5);
				U8(e, 0) = u0;
				U8(e, 2) = u1;
				if (u1 == 0)
					U8(e, 2) = 0xFF;
				U8(e, 4) = U8(e, 0);
				U8(e, 6) = U8(e, 2);
				U8(e, 3) = v0;
				U8(e, 1) = v0;
				const uint8_t v1 = (uint8_t)(v0 + 0x20);
				U8(e, 7) = v1;
				U8(e, 5) = v1;
			}
		}
		return 0; // void
	}

	// 0x8243B0 (sub_8243B0): warp state 1 - wave increments +0x60/+0x62/+0x64/+0x70/+0x72/+0x74 =
	// 0x200, phases +0x58/+0x5A/+0x5C = 0 / 0x400 / 0x800, colour bytes +0x7C..+0x7E = 0x80 (not read),
	// draws (0x8244E0 at OT entry 5, 0x824410), next state
	uint32_t __cdecl rs_8243B0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x64) = 0x200;
		U16(node, 0x62) = 0x200;
		U16(node, 0x60) = 0x200;
		U16(node, 0x74) = 0x200;
		U16(node, 0x72) = 0x200;
		U16(node, 0x70) = 0x200;
		U16(node, 0x58) = 0;
		U16(node, 0x5A) = 0x400;
		U16(node, 0x5C) = 0x800;
		U8(node, 0x7C) = 0x80;
		U8(node, 0x7D) = 0x80;
		U8(node, 0x7E) = 0x80;
		rs_8244E0(node, 5);
		rs_824410();
		next_state(node);
		return 0; // void
	}

	// 0x824410 (sub_824410): the two draw environments of the current buffer k = ([0x1D96A80] - 1) & 1:
	// the display's draw environment 0x1D969C8 + k * 0x5C copied to 0x2646700 + k * 0x5C (dtd 0: the
	// screen back) and to 0x2647760 + k * 0x5C changed to the VRAM area (0x240, 0x100) 320 x 240 (clip
	// + offset, dtd 1, no colour/mask bytes), their packets 0x26472D8 + k * 0x40 inserted at OT
	// +0x4484 (the off-screen scene) and 0x2647820 + k * 0x40 at OT +0x14 (back to the screen)
	uint32_t __cdecl rs_824410()
	{
		const uint32_t k = (uint32_t)(MEM<uint8_t>(0x1D96A80) - 1) & 1;
		const uint32_t pkt_scene = 0x26472D8 + (k << 6);
		const uint32_t pkt_back = 0x2647820 + (k << 6);
		const uint32_t env_back = 0x2646700 + k * 0x5C;
		const uint32_t env_disp = 0x1D969C8 + k * 0x5C;
		const uint32_t env_scene = 0x2647760 + k * 0x5C;
		memcpy((void *)env_back, (const void *)env_disp, 0x5C);
		memcpy((void *)env_scene, (const void *)env_disp, 0x5C);
		U16(env_scene, 0) = 0x240;
		U16(env_scene, 2) = 0x100;
		U16(env_scene, 4) = 0x140;
		U16(env_scene, 6) = 0xF0;
		U16(env_scene, 8) = 0x240;
		U16(env_scene, 0xA) = 0x100;
		U8(env_scene, 0x18) = 1;
		U8(env_scene, 0x19) = 0;
		U8(env_scene, 0x1A) = 0;
		U8(env_scene, 0x1B) = 0;
		U8(env_back, 0x18) = 0;
		rx::SetDrawEnv(pkt_scene, env_scene);
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x4484, pkt_scene);
		rx::SetDrawEnv(pkt_back, env_back);
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x14, pkt_back);
		return 0; // void
	}

	// 0x8244E0 (sub_8244E0): warp draw - steps the waves (0x8246D0), then the semi-transparent pass
	// (grid of table row 1, 0x824520(1), quads 0x8245B0(.., 1)) and the opaque pass (row 0) into
	// OT entry a2
	uint32_t __cdecl rs_8244E0(uint32_t a1, uint32_t a2)
	{
		// 30 fps layer: see mag049_resonance_held.inc
		FX_HELD(held_note_warp(a1);)
		rs_8246D0(a1, a2);
		rs_824520(1);
		rs_8245B0(a1, a2, 1);
		rs_824520(0);
		rs_8245B0(a1, a2, 0);
		return 0; // void
	}

	// 0x824520 (sub_824520): grid vertices 0x2647900 (11 columns i x 8 rows j, 12-byte entries) of
	// table row a1: x = 0 / 0x140 at the edges, else i * 0x20 + the x wave of row j
	// (0x26466E0 + 2 * a1 + 4 * j); y = 0 / 0xF0 at the edges, else j * 0x20 + the y wave of column
	// i (0x26467B8 + 2 * a1 + 4 * i)
	uint32_t __cdecl rs_824520(uint32_t a1)
	{
		const uint32_t r = a1;
		for (uint32_t i = 0; i < 0xB; i++)
		{
			const uint16_t bx = (uint16_t)(i * 0x20);
			uint16_t by = 0;
			for (uint32_t j = 0; j < 8; j++)
			{
				const uint32_t e = GRID + (j + i * 8) * 0xC;
				if (i == 0)
					U16(e, 0) = 0;
				else if (i == 0xA)
					U16(e, 0) = 0x140;
				else
					U16(e, 0) = (uint16_t)(U16(XWAVE + (r + j * 2) * 2, 0) + bx);
				if (j == 0)
					U16(e, 2) = 0;
				else if (j == 7)
					U16(e, 2) = 0xF0;
				else
					U16(e, 2) = (uint16_t)(U16(YWAVE + (r + i * 2) * 2, 0) + by);
				by = (uint16_t)(by + 0x20);
			}
		}
		return 0; // void
	}

	// 0x8245B0 (sub_8245B0): the 10 x 7 quads of the grid into the frame arena 0x1D8E054: POLY_FT4
	// (tag 0x09000000, code 0x2F semi-transparent when a3 == 1 else 0x2D, raw texture; the colour
	// bytes +4..+6 and the CLUT word +0xE are never written), texture page 0x119 (VRAM x 0x240, 15-bit)
	// for columns 0..7, 0x11D (x 0x340) for columns 8..9, each followed by a draw mode prim
	// (0x45BFC0 with GetTPage(0, 1, 0, 0)) inserted first, the quad after it (0x45C8E0), both in OT
	// entry a2; the cursor 0x1D8E054 advances by 0x34 per quad
	uint32_t __cdecl rs_8245B0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		(void)a1;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + a2 * 4;
		uint32_t pkt = MEM<uint32_t>(FRAME_CURSOR);
		const uint8_t code = ((uint16_t)a3 == 1) ? 0x2F : 0x2D;
		uint16_t tpage = 0x119;
		for (uint32_t i = 0; i < 0xA; i++)
		{
			if (i == 8)
				tpage = 0x11D;
			uint32_t e = GRID + i * 0x60 + 6;   // the entry (i, j) + 6
			for (int j = 0; j < 7; j++)
			{
				U8(pkt, 7) = code;
				U32(pkt, 0) = 0x9000000;
				U16(pkt, 0x14) = U16(e, 0);          // u1 v0
				U32(pkt, 8) = U32(e, -6);            // x, y (i, j)
				U16(pkt, 0xC) = U16(e, -2);          // u0 v0
				U16(pkt, 0x1C) = U16(e, 2);          // u0 v1
				U16(pkt, 0x24) = U16(e, 4);          // u1 v1
				U32(pkt, 0x10) = U32(e, 0x5A);       // x, y (i + 1, j)
				U32(pkt, 0x20) = U32(e, 0x66);       // x, y (i + 1, j + 1)
				U32(pkt, 0x18) = U32(e, 6);          // x, y (i, j + 1)
				U16(pkt, 0x16) = tpage;
				const uint32_t mode = pkt + 0x28;
				const uint32_t tp = x::sub_45C690(0, 1, 0, 0) & 0xFFFF;
				x::sub_45BFC0(mode, 0, 0, tp, 0);
				x::SSIGPU_InsertPrimAutoDepth(ot, mode);
				x::SSIGPU_InsertPrimAltViewport(ot, pkt);
				e += 0xC;
				pkt = mode + 0xC;
			}
		}
		MEM<uint32_t>(FRAME_CURSOR) = pkt;
		return 0; // void
	}

	// 0x8246D0 (sub_8246D0): steps the waves: phases +0x58/+0x5A/+0x5C and +0x68/+0x6A/+0x6C each
	// += rand & 0xFF + 0x100 (12 bits); then the x wave rows r = 0, 1 (8 entries, stride 4 from
	// 0x26466E0 + 2 * r; phase +0x58 + 2 * r stepping by +0x60 + 2 * r) and the y wave rows (11
	// entries from 0x26467B8 + 2 * r; phase +0x68 + 2 * r stepping by +0x70 + 2 * r): sin(phase) *
	// amplitude [WARP_PTR]+0 >> 21 (toward zero). The step words' upper halves (registers / a stack
	// word never written) are masked away by the & 0xFFF.
	uint32_t __cdecl rs_8246D0(uint32_t a1, uint32_t a2)
	{
		(void)a2;
		const uint32_t node = a1;
		static const int32_t PH[6] = { 0x58, 0x5A, 0x5C, 0x68, 0x6A, 0x6C };
		for (int k = 0; k < 6; k++)
		{
			const uint32_t r = x::CrtRand();
			U16(node, PH[k]) = (uint16_t)((U16(node, PH[k]) + (r & 0xFF) + 0x100) & 0xFFF);
		}
		for (uint32_t row = 0; row < 2; row++)
		{
			const uint16_t step = U16(node, 0x60 + row * 2);
			uint32_t ph = U16(node, 0x58 + row * 2);
			for (uint32_t k = 0; k < 8; k++)
			{
				const int32_t s = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)ph);
				const int32_t v = mul32(s, (int32_t)S16(MEM<uint32_t>(WARP_PTR), 0));
				U16(XWAVE + row * 2 + k * 4, 0) = (uint16_t)(add32(v, (v >> 31) & 0x1FFFFF) >> 21);
				ph = (ph + step) & 0xFFF;
			}
		}
		for (uint32_t row = 0; row < 2; row++)
		{
			uint32_t ph = U16(node, 0x68 + row * 2);
			const uint16_t step = U16(node, 0x70 + row * 2);
			for (uint32_t k = 0; k < 0xB; k++)
			{
				const int32_t s = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)ph);
				ph = (ph + step) & 0xFFF;
				const int32_t v = mul32(s, (int32_t)S16(MEM<uint32_t>(WARP_PTR), 0));
				U16(YWAVE + row * 2 + k * 4, 0) = (uint16_t)(add32(v, (v >> 31) & 0x1FFFFF) >> 21);
			}
		}
		return 0; // void
	}

	// 0x8248A0 (sub_8248A0): warp state 2 - amplitude += 0x100 up to 0x1000 (then next state), draws
	uint32_t __cdecl rs_8248A0(uint32_t a1)
	{
		const uint32_t w = MEM<uint32_t>(WARP_PTR);
		U16(w, 0) = (uint16_t)(U16(w, 0) + 0x100);
		if (S16(w, 0) >= 0x1000)
		{
			U16(w, 0) = 0x1000;
			next_state(a1);
		}
		rs_8244E0(a1, 5);
		return rs_824410();
	}

	// 0x8248E0 (sub_8248E0): warp state 3 - next state after its tick 0x38, draws
	uint32_t __cdecl rs_8248E0(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x38)
			next_state(a1);
		rs_8244E0(a1, 5);
		return rs_824410();
	}

	// 0x824900 (sub_824900): warp state 4 - amplitude -= 0x80 down to 0 (then next state), draws
	uint32_t __cdecl rs_824900(uint32_t a1)
	{
		const uint32_t w = MEM<uint32_t>(WARP_PTR);
		U16(w, 0) = (uint16_t)(U16(w, 0) - 0x80);
		if (S16(w, 0) <= 0)
		{
			U16(w, 0) = 0;
			next_state(a1);
		}
		rs_8244E0(a1, 5);
		return rs_824410();
	}

	// ====================================================================================
	// the hit task (0x824970 = Siren's 0x73F6A0 body, node 0x80): the target's model reacts
	// ====================================================================================

	// 0x8249E0 (sub_8249E0): hit state 0 - once the phase has run 0x1C ticks: 0x824A10, next state
	uint32_t __cdecl rs_8249E0(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0x1C)
		{
			rs_824A10(a1);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x824A10 (sub_824A10): the target's (+0x2D) model plays animation 4 or 5 (rand & 1, kept in +0x78)
	uint32_t __cdecl rs_824A10(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2D));
		const uint32_t r = x::CrtRand() & 1;
		if (r == 0)
			U16(a1, 0x78) = 4;
		else
			U16(a1, 0x78) = 5;
		return x::queueChainTransformationConditional(ent, (uint32_t)(int32_t)S16(a1, 0x78));
	}

	// 0x824A70 (sub_824A70): hit state 1 - once the target's model no longer plays +0x78 (its
	// animation byte [entity +0x74] differs): 0x824AB0, next state
	uint32_t __cdecl rs_824A70(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2D));
		if (U16(a1, 0x78) != (uint16_t)MEM<uint8_t>(U32(ent, 0x74)))
		{
			rs_824AB0(a1);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x824AB0 (sub_824AB0): the target's model plays animation 6 (+0x78 = 6)
	uint32_t __cdecl rs_824AB0(uint32_t a1)
	{
		const uint32_t slot = U8(a1, 0x2D);
		U16(a1, 0x78) = 6;
		return x::queueChainTransformationConditional(ENT(slot), 6);
	}

	// 0x824AE0 / 0x824B00 (sub_824AE0 / sub_824B00): hit states 2 / 3 - next state once the phase has
	// run 0x37 / 0x4B ticks
	uint32_t __cdecl rs_824AE0(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0x37)
			next_state(a1);
		return 0; // void
	}
	uint32_t __cdecl rs_824B00(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0x4B)
			next_state(a1);
		return 0; // void
	}

	// 0x824B20 (sub_824B20): hit state 4 - once the target's model no longer plays animation 6: applies
	// the damage of this target (0x506690 with the action's target record +0x2B), finished, next state
	uint32_t __cdecl rs_824B20(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2D));
		if (U16(a1, 0x78) == (uint16_t)MEM<uint8_t>(U32(ent, 0x74)))
			return 0; // void
		const uint32_t act = U32(U32(a1, 0xC), 4) + (uint32_t)((int32_t)S8(a1, 0x2A) * 20);
		x::ApplyActionResultToTarget(U32(act, 8) + (uint32_t)((int32_t)S8(a1, 0x2B) * 24));
		U8(a1, 0x26) |= 1;
		next_state(a1);
		return 0; // void
	}

	// 0x824B90 (MAG_049_sub_824B90): emitter state - next state once the director's damage cue +0x48 is 1
	uint32_t __cdecl rs_824B90(uint32_t a1)
	{
		if (U16(t1_data(), 0x48) == 1)
			next_state(a1);
		return 0; // void
	}

	// ====================================================================================
	// the library functions of this module whose code matches a Stare / Granaldo / Doom / Curse /
	// Tonberry / Siren port up to the module's addresses and callees (the port's text with the
	// module's addresses; "copy of" names the source)
	// ====================================================================================

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl rs_821800(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8218C0 (MAG_049_sub_8218C0; Psycho Blast 0x893450 = Granaldo 0x8A36A0): emitter state 0 - sets up the
	// director block (0x8218F0), spawns the director 0x821970 (0x48 bytes, director queue), next state
	uint32_t __cdecl rs_8218C0(uint32_t a1)
	{
		rs_8218F0(a1);
		x::Effect_AddTaskAndInitFromCtx(0x26465B8, 0x821970, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8218F0 (MAG_049_sub_8218F0; Psycho Blast 0x893480): emitter a1 - clears the director block [0x15ADAF0] (0x54 bytes),
	// +8 / +0xC = the target's (+0x2D) position words, origin +0..+4 = 0, facing +0x4A = the angle of
	// the target seen from the origin - 0x800 (12 bits)
	uint32_t __cdecl rs_8218F0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t ent = ENT(U8(a1, 0x2D));
		U32(d, 8) = U32(ent, 0x1C);
		U32(d, 0xC) = U32(ent, 0x20);
		U16(d, 0) = 0;
		U16(d, 2) = 0;
		U16(d, 4) = 0;
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)S16(d, 0xC));
		const uint32_t r = (ang - 0x800) & 0xFFF;
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)r;
		return r;
	}

	// 0x836AC0 (MAG_045_sub_836AC0; Psycho Blast pb_893500 with a 5-state table): TASK director -
	// phase bookkeeping (0x836B30) every tick, then its states {0x836B80 cue 1 (a_73B790),
	// 0x836BD0 sound once the phase runs, 0x836C20 damage cue after 0x20 ticks of the phase,
	// 0x836C40 finish (a_7475A0), ret}
	uint32_t __cdecl rs_821970(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x821A30;
		states[1] = 0x821A80;
		states[2] = 0x821AC0;
		states[3] = 0x821AF0;
		states[4] = 0x821B10; // nullsub (ret)
		rs_8219E0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl rs_8219E0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			rs_821B30(a1);
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

	// 0x763B90 (module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl rs_821A80(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x836EB0 (sub_836EB0; Boko b_72A720 with the module's blend buffer, no pause and the depth
	// offset +0x88 in the block): builds the prim-model effect matrix (rotations +0x62/+0x60/+0x64,
	// scale +0x50, position +0x1C, camera) and the 0x5C parameter block, then plays the prim-model
	// layout +0x94 with the draw callback 0x836FE0; returns the player result (0 = play finished)
	uint32_t __cdecl rs_821D40(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = 0x2647D20 (vertex lerp scratch),
		// +0x4C..+0x58 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88. +0x20..+0x37,
		// +0x3C..+0x43, +0x5A are never written (the callback 0x821E70 does not read them)
		alignas(4) uint8_t blk[0x5C] = {};
		const uint32_t L = P(blk);
		const uint32_t node = a1;
		x::MAG_022_sub_8DD770(L);
		int16_t ang = S16(node, 0x62);
		if (ang != 0)
			x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)ang);
		ang = S16(node, 0x60);
		if (ang != 0)
			x::sub_8DD7E0(L, (uint32_t)(int32_t)ang);
		ang = S16(node, 0x64);
		if (ang != 0)
			x::sub_8DD960(L, (uint32_t)(int32_t)ang);
		x::scale3DMatrix(L, node + 0x50);
		S32(L, 0x14) = S16(node, 0x1C);
		S32(L, 0x18) = S16(node, 0x1E);
		S32(L, 0x1C) = S16(node, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(node, 0x86);
		U32(L, 0x44) = U32(node, 0x7C);
		U32(L, 0x38) = U32(node, 0x70);
		U16(L, 0x4C) = U16(node, 0x8C);
		U16(L, 0x52) = U16(node, 0x92);
		U16(L, 0x4E) = U16(node, 0x8E);
		U16(L, 0x50) = U16(node, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(node, 0x80);
		U16(L, 0x58) = U16(node, 0x88);
		// 30 fps layer: see mag049_resonance_held.inc
		FX_HELD(held_note_play(node);)
		return prim_play(node + 0x94, PRIM_CB, L, 0);
	}

	// 0x8A4830 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8A4960, 0x8A4B80, 0x8A4E10, 0x8A5290, a_73D770, a_73D9C0, 0x8A5D40,
	// 0x8A6240; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl rs_8220B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { rs_8221E0, rs_822400, rs_822690, rs_822B10, a_73D770, a_73D9C0, rs_8235C0, rs_823AC0 };
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
	uint32_t __cdecl rs_8221E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rs_822400(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rs_822690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rs_822B10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rs_8235C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rs_823AC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x72AAF0 (097 sub_72AAF0 and copies 0x731570/0x731920/0x731A20, 098/099/100 copies):
	// prim-model task state handler - plays one frame (b_72A720); when the play ended (0) sets
	// the finished flag +0x26 bit0 and advances +0x29
	uint32_t __cdecl rs_824110(uint32_t a1)
	{
		const uint32_t r = rs_821D40(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x72AAF0 (097 sub_72AAF0 and copies 0x731570/0x731920/0x731A20, 098/099/100 copies):
	// prim-model task state handler - plays one frame (b_72A720); when the play ended (0) sets
	// the finished flag +0x26 bit0 and advances +0x29
	uint32_t __cdecl rs_824230(uint32_t a1)
	{
		const uint32_t r = rs_821D40(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x73F6A0 (module 095): TASK, model task D: state table {0x73F710, 0x73F760, 0x73F770,
	// 0x73F790, 0x73F7C0, 0x73F7F0}; frame counter++, ends when finished and no children
	uint32_t __cdecl rs_824970(uint32_t a1)
	{
		const uint32_t tab[6] = { 0x8249E0, 0x824A70, 0x824AE0, 0x824B00, 0x824B20, 0x824B80 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// ---- generated (gendesc.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8217E0, (void *)a_73A0D0, "049 MAG_049_sub_8217E0" },
		{ 0x8217F0, (void *)a_73A0D0, "049 MAG_049_sub_8217F0" },
		{ 0x821810, (void *)a_73A170, "049 MAG_049_sub_821810" },
		{ 0x821840, (void *)a_73A1A0, "049 MAG_049_sub_821840" },
		{ 0x821A30, (void *)a_73B790, "049 sub_821A30" },
		{ 0x821A50, (void *)a_73B640, "049 sub_821A50" },
		{ 0x821AA0, (void *)a_73B7E0, "049 sub_821AA0" },
		{ 0x821B10, (void *)a_73A6D0, "049 nullsub_1545" },
		{ 0x821B20, (void *)a_73A6D0, "049 nullsub_1546" },
		{ 0x821C50, (void *)a_73A380, "049 sub_821C50" },
		{ 0x823090, (void *)a_73D770, "049 sub_823090" },
		{ 0x8232E0, (void *)a_73D9C0, "049 sub_8232E0" },
		{ 0x824130, (void *)a_73A6D0, "049 nullsub_1547" },
		{ 0x824140, (void *)a_73A380, "049 sub_824140" },
		{ 0x824250, (void *)a_73A6D0, "049 nullsub_1548" },
		{ 0x824940, (void *)a_7475A0, "049 sub_824940" },
		{ 0x824960, (void *)a_73A6D0, "049 nullsub_1549" },
		{ 0x824B80, (void *)a_73A6D0, "049 nullsub_1550" },
		{ 0x824BB0, (void *)a_7474B0, "049 MAG_049_sub_824BB0" },
		{ 0x824BD0, (void *)a_7474D0, "049 MAG_049_sub_824BD0" },
		{ 0x824BF0, (void *)a_73A6D0, "049 nullsub_1551" },
		{ 0x824C00, (void *)a_747500, "049 MAG_049_sub_824C00" },
		{ 0x824C40, (void *)a_73A0D0, "049 MAG_049_sub_824C40" },
		{ 0x824C50, (void *)a_747550, "049 MAG_049_sub_824C50" },
		{ 0x824C60, (void *)a_747560, "049 MAG_049_sub_824C60" },
		{ 0x824C90, (void *)a_747590, "049 MAG_049_sub_824C90" },
		{ 0x824CA0, (void *)a_7475A0, "049 MAG_049_sub_824CA0" },
		{ 0x824CC0, (void *)a_73A6D0, "049 nullsub_1544" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x821690, (void *)rs_821690, "049 rs_821690" },
		{ 0x821800, (void *)rs_821800, "049 rs_821800" },
		{ 0x8218C0, (void *)rs_8218C0, "049 rs_8218C0" },
		{ 0x8218F0, (void *)rs_8218F0, "049 rs_8218F0" },
		{ 0x821970, (void *)rs_821970, "049 rs_821970" },
		{ 0x8219E0, (void *)rs_8219E0, "049 rs_8219E0" },
		{ 0x821A80, (void *)rs_821A80, "049 rs_821A80" },
		{ 0x821AC0, (void *)rs_821AC0, "049 rs_821AC0" },
		{ 0x821AF0, (void *)rs_821AF0, "049 rs_821AF0" },
		{ 0x821B30, (void *)rs_821B30, "049 rs_821B30" },
		{ 0x821C00, (void *)rs_821C00, "049 rs_821C00" },
		{ 0x821CB0, (void *)rs_821CB0, "049 rs_821CB0" },
		{ 0x821D40, (void *)rs_821D40, "049 rs_821D40" },
		{ 0x821E70, (void *)rs_821E70, "049 rs_821E70" },
		{ 0x8220B0, (void *)rs_8220B0, "049 rs_8220B0" },
		{ 0x8221E0, (void *)rs_8221E0, "049 rs_8221E0" },
		{ 0x822400, (void *)rs_822400, "049 rs_822400" },
		{ 0x822690, (void *)rs_822690, "049 rs_822690" },
		{ 0x822B10, (void *)rs_822B10, "049 rs_822B10" },
		{ 0x8235C0, (void *)rs_8235C0, "049 rs_8235C0" },
		{ 0x823AC0, (void *)rs_823AC0, "049 rs_823AC0" },
		{ 0x824110, (void *)rs_824110, "049 rs_824110" },
		{ 0x8241A0, (void *)rs_8241A0, "049 rs_8241A0" },
		{ 0x824230, (void *)rs_824230, "049 rs_824230" },
		{ 0x824260, (void *)rs_824260, "049 rs_824260" },
		{ 0x8242E0, (void *)rs_8242E0, "049 rs_8242E0" },
		{ 0x824300, (void *)rs_824300, "049 rs_824300" },
		{ 0x8243B0, (void *)rs_8243B0, "049 rs_8243B0" },
		{ 0x824410, (void *)rs_824410, "049 rs_824410" },
		{ 0x8244E0, (void *)rs_8244E0, "049 rs_8244E0" },
		{ 0x824520, (void *)rs_824520, "049 rs_824520" },
		{ 0x8245B0, (void *)rs_8245B0, "049 rs_8245B0" },
		{ 0x8246D0, (void *)rs_8246D0, "049 rs_8246D0" },
		{ 0x8248A0, (void *)rs_8248A0, "049 rs_8248A0" },
		{ 0x8248E0, (void *)rs_8248E0, "049 rs_8248E0" },
		{ 0x824900, (void *)rs_824900, "049 rs_824900" },
		{ 0x824970, (void *)rs_824970, "049 rs_824970" },
		{ 0x8249E0, (void *)rs_8249E0, "049 rs_8249E0" },
		{ 0x824A10, (void *)rs_824A10, "049 rs_824A10" },
		{ 0x824A70, (void *)rs_824A70, "049 rs_824A70" },
		{ 0x824AB0, (void *)rs_824AB0, "049 rs_824AB0" },
		{ 0x824AE0, (void *)rs_824AE0, "049 rs_824AE0" },
		{ 0x824B00, (void *)rs_824B00, "049 rs_824B00" },
		{ 0x824B20, (void *)rs_824B20, "049 rs_824B20" },
		{ 0x824B90, (void *)rs_824B90, "049 rs_824B90" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag049_resonance()
	{
		act::register_module(49);
		for (const act::resonance::ModPort *p = act::resonance::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(49, p->addr, p->port, p->name);
		for (const act::resonance::ModPort *p = act::resonance::PORTS; p->addr; p++)
			act::register_module_port(49, p->addr, p->port, p->name);
		// 30 fps layer: see mag049_resonance_held.inc
		FX_HELD(register_mag049_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag049_resonance_held.inc"
#endif
