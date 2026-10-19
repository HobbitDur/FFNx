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

// Effect 7: Mega Phoenix (battle item Mega Phoenix: kernel.bin battle item +4 = 7; MAG_007_*): a copy of the
// actor/heal effect library (see act_engine.h), built like Mega-Potion (mag053_megapotion.cpp: same master,
// emitter, stage fade, director set-up with ONE target per actor system and actor-system code with the
// module's own globals, its pool sizes / timings below) with its own prim-model actor draw 0x8CF4C0 (Raldo
// Throw's 0x8A46D0 render context with a texture V scroll on definition index 0, drawn through Raldo Throw's
// primitive-list dispatcher / list drawers: rh-identical copies, ported from mag019_raldo_throw.cpp).
//
// Setup MAG_007_MEGA_PHOENIX 0x8CE3D0 (runs once, not ported; file loader MAG_007_MEGA_PHOENIX_FL 0x8CE3B0 =
// the effect's data file 0x163E29C -> [0x275FB80], TIM uploaded by the setup unless the cast context's flag +1
// bit 0, its packet arenas at file + 0 / + 0xA000 (second arena word + 0xA000 / + 0x14000), file arena cursor
// 0x2760B5C = file + 0x14000) creates the root queue 0x275FC70 with the master and three task pools: 0x27609C0
// (4 x 0x58: emitters), 0x2760720 (4 x 0x2A4: actor systems), 0x27609B0 (10 x 0x40: stage fade).
//   Master (0x8CE4F0 = Full-life's 0x883240) - camera copy 0x2793E58 (the module scratch stack 0x2760B50 grows
//     down from it), packet arena by tick parity (cursor 0x275FB84), bone follow, 11-state table: 0x8CE650
//     reserves the actor pools after the arena (0xA8C + 0xD200 bytes = 25 particles of 0x6C and 80 actors of
//     0x2A0, cleared by 0x8CE6A0) and starts the stage fade in (0x8CE6E0: level 0x1D98992 + k * 0x2C up to
//     0x400), 0x8CE7D0 one emitter per target of the action (action loop 0x8D4280: next target record +0x2B
//     once the emitter releases the master's +0x63 hold), 0x8D42D0 after 13 more ticks the stage fade out
//     (0x8D4300), then waits for the queues to empty.
//   Emitter (0x8CE800), one per target (each KO'd ally): bone follow + model bounds (CURE_Emitter helpers),
//     sound 0x163ACD0 at its first tick; state 0 spawns the actor system (0x8CE890 -> 0x8CE8C0 with the actor
//     data 0x163E198, mode 0x2D), state 1 releases the master at tick 7, state 2 applies the action result to
//     its target (the revive) from tick 10.
//   Actor system (0x8CE910 -> 0x8CE970 / 0x8D41E0): the engine's particle actors (emitters and particles,
//     sprite sequences 0x8D1780 / prim models 0x8CF4C0 drawn into the module arena) placed by the actor data's
//     key positions (caster / its one target), taken once at the director set-up 0x8CE9A0. The prim-model
//     draw advances the actor system's texture scroll counter (state +0x2A: +8, 7-bit wrap) at every draw of
//     an actor of definition index 0.
//   Stage fade (0x8CE6E0 in: 0x8CE740 clears the 4 colour/fade layers of 0x1D9898C, 0x8CE780 level +0x100 per
//     tick to 0x400; 0x8D4300 out: 0x8D4360 level 0x400, 0x8D4380 -0x100 per tick to 0): the level word +6 of
//     each layer (0x1D98992 + k * 0x2C).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x275FB80..0x2760B64 (file pointer 0x275FB80, packet cursor 0x275FB84, pools, queues, arenas,
// actor state 0x2760B60, scratch stack pointer 0x2760B50), the module scratch stack below the camera copy
// 0x2793E58, the fade layers 0x1D98992 + k * 0x2C (words) / 0x1D989B8 + k * 0x2C (bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace megaphoenix
{
	// ---- generated (gen53.py): the module descriptor of this copy of the library ----
	static const Mod MOD_007 = { "megaphoenix", 7, 0x8CE3B0, 0x8D4430,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x275FBA4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2760720, 0x0, 0x27609C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2760B60 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8CE800, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8D1F30, 0x8D1F80, 0x0, 0x0, 0x8D2870, 0x0, 0x0, 0x0, 0x8D3250, 0x8D3280, 0x8D3360, 0x8D3480, 0x8D35D0, 0x0, 0x8D3C90, 0x8D3E70, 0x8D3FB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x275FB84;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x2760B60;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x2760B50;      // module scratch stack pointer
	static const uint32_t Q_EMITTER = 0x27609C0, Q_ACTORS = 0x2760720, Q_STAGE = 0x27609B0;
	static const uint32_t ORIG_Master = 0x8CE4F0;
	static const uint32_t ORIG_Emitter = 0x8CE800;
	static const uint32_t ORIG_StageIn = 0x8CE6E0;
	static const uint32_t ORIG_StageOut = 0x8D4300;
	static const uint32_t ORIG_ActorSystem = 0x8CE910;
	static const void *const SOUND = (const void *)0x163ACD0; // the emitter's sound

	// engine functions not in act::x
	namespace mfx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
	}

	// helpers of the library code (as in act_engine.cpp / mag097_boko.cpp / mag039_drain.cpp)
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
	// helpers of the prim-model list drawers (from mag019_raldo_throw.cpp, whose drawers these are copies of)
	// the prim-model renderer helpers (as in mag014_death.cpp / mag090_tonberry.cpp / mag019_raldo_throw.cpp)
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


	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl mf_8CE4F0(uint32_t a1);
	uint32_t __cdecl mf_8CE650(uint32_t a1);
	uint32_t __cdecl mf_8CE6A0(void);
	uint32_t __cdecl mf_8CE780(uint32_t a1);
	uint32_t __cdecl mf_8CE800(uint32_t a1);
	uint32_t __cdecl mf_8CE890(uint32_t a1);
	uint32_t __cdecl mf_8CE970(uint32_t a1);
	uint32_t __cdecl mf_8CE9A0(uint32_t a1);
	uint32_t __cdecl mf_8CEB30(uint32_t a1);
	uint32_t __cdecl mf_8CEC10(void);
	uint32_t __cdecl mf_8CED70(void);
	uint32_t __cdecl mf_8CEF60(void);
	uint32_t __cdecl mf_8CF3D0(uint32_t a1);
	uint32_t __cdecl mf_8CF460(void);
	uint32_t __cdecl mf_8CF4C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8CF660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8CF790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8CF9B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8CFC40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8D00C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8D0B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8D1070(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl mf_8D16C0(void);
	uint32_t __cdecl mf_8D1780(uint32_t a1);
	uint32_t __cdecl mf_8D1800(void);
	uint32_t __cdecl mf_8D1840(uint32_t a1);
	uint32_t __cdecl mf_8D2870(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8D2980(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8D2A00(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8D2A30(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl mf_8D2AD0(uint32_t a1);
	uint32_t __cdecl mf_8D2B50(uint32_t a1);
	uint32_t __cdecl mf_8D2BD0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl mf_8D33B0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8D35D0(uint32_t a1);
	uint32_t __cdecl mf_8D40C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl mf_8D4150(void);
	uint32_t __cdecl mf_8D41E0(uint32_t a1);
	uint32_t __cdecl mf_8D4210(uint32_t a1);
	uint32_t __cdecl mf_8D4230(uint32_t a1);
	uint32_t __cdecl mf_8D4280(uint32_t a1);
	uint32_t __cdecl mf_8D42D0(uint32_t a1);
	uint32_t __cdecl mf_8D4360(uint32_t a1);
	uint32_t __cdecl mf_8D4380(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag007_mega_phoenix_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace megaphoenix
{
	// ====================================================================================
	// the module's functions: the text of the matching Full-life port (Mighty Guard / Counter Laser-Eye for the
	// pool set-up / allocator / emitter states / action loop; Psycho Blast for 0x8D33B0, Raldo Throw for the
	// primitive-list dispatcher and list drawers) with this module's addresses and callees and Mega-Potion's
	// variant constants (the comments describe the source original); the director set-up and the prim-model
	// actor draw 0x8CF4C0 are ported by hand
	// ====================================================================================


	// 0x8CE4F0 (MAG_007_sub_8CE4F0): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the three queues (live task count -> node +0x5E; the actor counters
	// 0x2760B4A / 0x2760B3A are cleared before the queues run)
	uint32_t __cdecl mf_8CE4F0(uint32_t a1)
	{
		g_mod = &MOD_007;
		// 30 fps layer: see mag007_mega_phoenix_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x275FBA4) = 0x2793E58;
		states[0] = 0x8CE630;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x8CE640;
		states[2] = 0x8CE650;
		states[3] = 0x8CE7D0;
		states[4] = 0x8D4280;
		states[5] = 0x8D42D0;
		states[6] = 0x8D43D0;
		states[7] = 0x8D43E0;
		states[8] = 0x8D43F0;
		states[9] = 0x8D4400;
		states[10] = 0x8D4420; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2760B34);
			const uint32_t v2 = MEM<uint32_t>(0x275FC8C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x275FBA0) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2760B30);
			const uint32_t v2 = MEM<uint32_t>(0x275FC88);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x275FBA0) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x2760B4A) = 0;
		MEM<uint16_t>(0x2760B3A) = 0;
		static const uint32_t queues[3] = { Q_EMITTER, Q_ACTORS, Q_STAGE };
		for (int i = 0; i < 3; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x8CE650 (MAG_007_sub_8CE650): master state 2 - once no child is alive: the two actor pools
	// are carved from the file arena cursor [0x2760B5C] (0xA8C bytes = 25 particles of 0x6C at
	// [0x2760B3C], 0xD200 bytes = 80 actors of 0x2A0 at [0x2760B54]) and cleared (0x8CE6A0), the
	// stage fade task 0x8CE6E0 (0x40 bytes, stage queue) starts, next state
	uint32_t __cdecl mf_8CE650(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x2760B5C);
		MEM<uint32_t>(0x2760B3C) = p;
		p += 0xA8C;
		MEM<uint32_t>(0x2760B54) = p;
		p += 0xD200;
		MEM<uint32_t>(0x2760B5C) = p;
		mf_8CE6A0();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, ORIG_StageIn, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8CE6A0 (MAG_007_sub_8CE6A0): clears the two actor pools (0xA8C bytes at [0x2760B3C], 0xD200
	// at [0x2760B54]) and the pool cursors / counters
	uint32_t __cdecl mf_8CE6A0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2760B3C), 0xA8C);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2760B54), 0xD200);
		MEM<uint16_t>(0x275FC80) = 0;
		MEM<uint16_t>(0x2760B48) = 0;
		MEM<uint16_t>(0x2760B38) = 0;
		MEM<uint16_t>(0x275FB88) = 0;
		return 0; // void
	}


	// ====================================================================================
	// the library functions of this module whose code matches Drain's / an engine port's up to
	// the module's addresses and callees (the port's text with the module's addresses)
	// ====================================================================================
	// 0x8CE780 (MAG_007_sub_8CE780), written to the four stage fade words 0x1D98992 + k * 0x2C
	uint32_t __cdecl mf_8CE780(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x400)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x400;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}


	// 0x8CE800 (MAG_007_sub_8CE800): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x8CE890 actor system, 0x8D4210 release the master, 0x8D4230 action result,
	// ret}, sound at its first tick
	uint32_t __cdecl mf_8CE800(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x8CE890;
		states[1] = 0x8D4210;
		states[2] = 0x8D4230;
		states[3] = 0x8D4270; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}


	// 0x8CE890 (MAG_007_sub_8CE890): emitter state 0 - the actor system (0x8CE910, actor data
	// 0x163E198, start tick 0, mode 0x2D) through the engine spawner 0x8CE8C0, next state
	uint32_t __cdecl mf_8CE890(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, 0x163E198, 0, 0x2D, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8CE970 (sub_8CE970, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl mf_8CE970(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			mf_8CE9A0(a1);
			mf_8CF3D0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8CE9A0 (sub_8CE9A0): actor director set-up for node a1 (Full-life's 0x8836F0 with ONE target: the action's current
	// target record +0x2B instead of the action's whole target list): *ACTOR_STATE = state block node+0x34; copies
	// node+0x29A/0x29C(mode)/0x29E into it, points its 4 tables (+0x224..+0x230) into the loaded actor data
	// (node+0x30; relocated once by 0x8CF170, flag data+0), target count 1 = the action's record +0x2B target slot,
	// sets start / per-target / bounds positions (0x8CEC10, 0x8CEF60; modes 1/3/4 extra set-ups) and stores the
	// caster-to-target distance in state+0.
	uint32_t __cdecl mf_8CE9A0(uint32_t a1)
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
		const uint32_t tab = U32(U32(node, 0x0C), 4);
		const int32_t ti = S8(node, 0x2B);
		const uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
		U32(st, 4) = U8(rec, (uint32_t)mul32(ti, 3) * 8);   // target slot (byte, ebx cleared)
		U16(st, 0x1A) = U16(node, 0x29E);
		mf_8CEC10();
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 4)
			callp(0x8CEB30, node);
		mf_8CEF60();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 1)
		{
			callp(0x8CED70, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		// distance caster entity -> target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		const int32_t s1 = S16(st, 0x1E);
		const uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		const uint32_t d10 = U32(e1, 0x1C);
		const uint32_t d14 = U32(e1, 0x20);
		const uint32_t s2 = U32(st, 4);
		const uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		const uint32_t d18 = U32(e2, 0x1C);
		const int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		const int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		const int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		const int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		const uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(ACTOR_STATE);
		U32(st, 0) = dist;
		return 0; // void
	}


	// 0x8CEB30 (sub_8CEB30, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl mf_8CEB30(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x2760B60);
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


	// 0x8CEC10 (sub_8CEC10): the actor system's reference points from the caster (slot state +0x1E):
	// +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same on the ground (y 0),
	// +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C (16.16 each);
	// mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl mf_8CEC10(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		const int32_t slot = S16(st, 0x1E);
		const uint32_t ent = 0x1D972C0 + (uint32_t)mul32(slot, 0x9C);
		// [esp+4] s16 x, y, z: never-written stack when mode 2 skips the bone reads (0 here)
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		const int32_t px = S16(pp, 0);
		U16(pp, 2) = U16(ent, 0x24);
		S32(st, 0x1C4) = shl32(px, 16);
		S32(st, 0x1C8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1CC) = shl32(S16(pp, 4), 16);
		U32(st, 0x1D4) = U32(st, 0x1C4);
		U32(st, 0x1D8) = 0;
		U32(st, 0x1DC) = U32(st, 0x1CC);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		S32(st, 0x1E4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1E8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1EC) = shl32(S16(pp, 4), 16);
		if (U16(st, 0x24) != 2)
		{
			x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		S32(st, 0x1F4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1F8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1FC) = shl32(S16(pp, 4), 16);
		const uint32_t sx = U32(st, 0x1C4);
		const int32_t h = shl32(S16(ent, 0x3C), 16);
		U32(st, 0x204) = sx;
		S32(st, 0x208) = h;
		U32(st, 0x20C) = U32(st, 0x1CC);
		return 0; // void
	}


	// 0x8CED70 (sub_8CED70, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl mf_8CED70(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x2760B60);
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


	// 0x8CEF60 (sub_8CEF60): per target of the actor system (slots state +4.., count +0x1C) the
	// target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its ground point),
	// +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride 0x10;
	// then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl mf_8CEF60(void)
	{
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);
		// [esp+0x18] s16 x, y, z: never-written stack when mode 2 skips the bone reads (0 here)
		alignas(4) uint8_t pos[8] = {};
		const uint32_t pp = P(pos);
		if (S16(st, 0x1C) > 0)
		{
			uint32_t o = 0;
			for (int32_t i = 0; ; )
			{
				const uint32_t slot = U32(st, 4 + 4 * i);
				const uint32_t ent = 0x1D972C0 + slot * 0x9C;
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				const int32_t px = S16(pp, 0);
				U16(pp, 2) = U16(ent, 0x24);
				S32(st, o + 0x54) = shl32(px, 16);
				S32(st, o + 0x58) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x5C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x94) = U32(st, o + 0x54);
				U32(st, o + 0x98) = 0;
				U32(st, o + 0x9C) = U32(st, o + 0x5C);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				S32(st, o + 0xD4) = shl32(S16(pp, 0), 16);
				S32(st, o + 0xD8) = shl32(S16(pp, 2), 16);
				S32(st, o + 0xDC) = shl32(S16(pp, 4), 16);
				if (U16(st, 0x24) != 2)
				{
					x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
					st = MEM<uint32_t>(ACTOR_STATE);
				}
				S32(st, o + 0x114) = shl32(S16(pp, 0), 16);
				S32(st, o + 0x118) = shl32(S16(pp, 2), 16);
				S32(st, o + 0x11C) = shl32(S16(pp, 4), 16);
				U32(st, o + 0x154) = U32(st, o + 0x54);
				S32(st, o + 0x158) = shl32(S16(ent, 0x3C), 16);
				U32(st, o + 0x15C) = U32(st, o + 0x5C);
				o += 0x10;
				i++;
				if (i >= S16(st, 0x1C)) break;
			}
		}
		// centre of the x / z range of the targets' +0x54 / +0x5C
		const int32_t n = S16(st, 0x1C);
		int32_t min_x = 0, max_x = 0, max_z = 0, min_z = 0;
		for (int32_t i = 0; i < n; i++)
		{
			const int32_t vx = S32(st, 0x54 + 0x10 * i);
			const int32_t vz = S32(st, 0x5C + 0x10 * i);
			if ((uint16_t)i == 0)
			{
				max_x = vx;
				max_z = vz;
				min_x = vx;
				min_z = vz;
				continue;
			}
			if (vx < min_x) min_x = vx;
			else if (vx > max_x) max_x = vx;
			if (vz < min_z) min_z = vz;
			else if (vz > max_z) max_z = vz;
		}
		S32(st, 0x194) = add32(min_x, max_x) / 2;
		U32(st, 0x198) = 0;
		S32(st, 0x19C) = add32(max_z, min_z) / 2;
		return 0; // void
	}


	// 0x8CF3D0 (sub_8CF3D0, copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl mf_8CF3D0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x2760B60, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			mf_8D4150();
			mf_8D1800();
			// 30 fps layer: see mag007_mega_phoenix_held.inc
			FX_HELD(held_note_system(sys);)
			mf_8D16C0();
			mf_8CF460();
			sys = U32(0x2760B60, 0);
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
		U16(0x2760B4A, 0) = (uint16_t)(U16(0x2760B4A, 0) + c14);
		U16(0x2760B3A, 0) = (uint16_t)(U16(0x2760B3A, 0) + c16);
		return done;
	}


	// 0x8CF460 (sub_8CF460, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl mf_8CF460(void)
	{
		uint32_t act = U32(U32(0x2760B60, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x2760B60, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					mf_8CF4C0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8CF4C0 (sub_8CF4C0): draw of a prim-model actor a1 (definition a2) (Raldo Throw's 0x8A46D0 with the texture
	// scroll of Esuna's 0x88E090, at another definition index and step): a 0x68-byte render context on the
	// module scratch stack (model +0x170, flags +0x14: 0x30 unless definition +0x3A, fade +0x1CE -> far colour
	// +0x16C / level, flags | 0xC0), texture windows +0x1C / +0x24 (256 x 256; the actors of definition
	// index 0 get a 128 x 128 window B and a V scroll +0x1A that advances by 8 (7-bit wrap) at every such
	// draw: actor system state +0x2A), drawn through the primitive-list dispatcher 0x8CF660 into the
	// module arena (OT base + 0x44, depth shift 2): once, or once per sub-position +0x19C.. (+0x1D8 copies)
	uint32_t __cdecl mf_8CF4C0(uint32_t a1, uint32_t a2)
	{
		const uint32_t act = a1;
		const uint32_t ctx = MEM<uint32_t>(SCRATCH_SP) - 0x68;
		U32(ctx, 0) = U32(act, 0x170);
		MEM<uint32_t>(SCRATCH_SP) = ctx;
		U32(ctx, 0x14) = 0;
		if (U16(a2, 0x3A) == 0)
			U32(ctx, 0x14) = 0x30;
		const uint16_t col = U16(act, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(act, 0x16C);
			const uint32_t fl = U32(ctx, 0x14);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x14) = fl | 0xC0;
		}
		const uint8_t def_idx = U8(act, 0x1D6);
		U32(ctx, 0x10) = 0;
		U16(ctx, 0x1E) = 0;
		U16(ctx, 0x1C) = 0;
		U16(ctx, 0x22) = 0x100;
		U16(ctx, 0x20) = 0x100;
		if (def_idx == 0)
		{
			const uint32_t st = MEM<uint32_t>(ACTOR_STATE);
			U16(ctx, 0x28) = 0x80;
			U16(ctx, 0x2A) = 0x80;
			const uint16_t scroll = (uint16_t)((uint8_t)(U8(st, 0x2A) + 8) & 0x7F);
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x18) = 0;
			U16(st, 0x2A) = scroll;
			U16(ctx, 0x1A) = scroll;
		}
		else
		{
			U16(ctx, 0x18) = 0;
			U16(ctx, 0x1A) = 0;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x2A) = 0x100;
			U16(ctx, 0x28) = 0x100;
		}
		const int8_t copies = S8(act, 0x1D8);
		if (copies == 1)
		{
			const uint32_t m = act + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
			const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			MEM<uint32_t>(PACKET_CURSOR) = mf_8CF660(ctx, ot, 2, cursor);
		}
		else if (copies > 0)
		{
			const uint32_t m = act + 0xAC;
			uint32_t pos = act + 0x19E;
			int32_t i = 0;
			do
			{
				S32(act, 0xC0) = S16(pos, -2);
				S32(act, 0xC4) = S16(pos, 0);
				S32(act, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
				const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
				const uint32_t r = mf_8CF660(ctx, ot, 2, cursor);
				i++;
				pos += 8;
				MEM<uint32_t>(PACKET_CURSOR) = r;
			} while (i < S8(act, 0x1D8));
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}


	// 0x8CF660 (sub_8CF660 of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8CF790, 0x8CF9B0, 0x8CFC40, 0x8D00C0, a_73D770, a_73D9C0, 0x8D0B70,
	// 0x8D1070; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl mf_8CF660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { mf_8CF790, mf_8CF9B0, mf_8CFC40, mf_8D00C0, a_73D770, a_73D9C0, mf_8D0B70, mf_8D1070 };
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


	// 0x8CF790 (sub_8CF790 of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl mf_8CF790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8CF9B0 (sub_8CF9B0 of t_764890 0x764890: module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl mf_8CF9B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8CFC40 (sub_8CFC40 of s_73CD70 0x73CD70: module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl mf_8CFC40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8D00C0 (sub_8D00C0 of s_73D1F0 0x73D1F0: module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl mf_8D00C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8D0B70 (sub_8D0B70; same code as Siren's 0x73DCA0 / Drink Magic's 0x85D990): prim-list block
	// "Gouraud-textured triangles, UV scroll" of the module renderer 0x8CF660: 0x1C-byte records
	// (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C, uv1+tpage +0x10, rgb1
	// +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face (NCLIP, unless ctx+0x14
	// bit 0x20) and off-screen culling, optional lighting (bit 0x80), z = OTZ + ctx+0x10 (>= 0) >>
	// a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the UVs are scrolled (wrapped by
	// ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window packets (ctx+0x1C set, ctx+0x24
	// restore): 0x40 bytes; else the poly alone (0x28 bytes; unlike Tonberry's 0x765B30, no
	// draw-mode packet). The original keeps seven packet-field pointers apart from the cursor and
	// advances them together (always cursor + fixed offsets). Returns the new packet cursor; ctx+0x2C
	// advances past the block.
	uint32_t __cdecl mf_8D0B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8D1070 (sub_8D1070 of s_73E1A0 0x73E1A0: module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl mf_8D1070(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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


	// 0x8D16C0 (sub_8D16C0, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl mf_8D16C0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x2760B60), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x2760B60);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						mf_8D1780(act);   // (the original also pushes the bone entry, unused)
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
							mf_8D1780(act);
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


	// 0x8D1780 (sub_8D1780): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl mf_8D1780(uint32_t a1)
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


	// 0x8D1800 (sub_8D1800, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl mf_8D1800(void)
	{
		uint32_t act = U32(U32(0x2760B60, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				mf_8D2AD0(act);
			else if (type == 1)
				mf_8D1840(act);
			act = U32(act, 4);
		}
		return 0; // void
	}


	// 0x8D1840 (sub_8D1840, copy of a_740880 0x740880, callee chain to the module's particle pool 0x8D2A30): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl mf_8D1840(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x2760B60);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (mf_8D2980(act, bone) == 0)
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
			st = MEM<uint32_t>(0x2760B60);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}


	// 0x8D2870 (sub_8D2870, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl mf_8D2870(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x2760B60, 0);                  // actor state block
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
				mfx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				mfx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				mfx::sub_45E0B0(in2);
				mfx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}


	// 0x8D2980 (sub_8D2980, copy of a_7419C0 0x7419C0, callee chain to the module's particle pool 0x8D2A30): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl mf_8D2980(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				mf_8D2A00(a1, a2);
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
			mf_8D2A00(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			mf_8D2A00(a1, a2);
			return 1;
		}
		return 0;
	}


	// 0x8D2A00 (sub_8D2A00, copy of a_741A40 0x741A40, callee chain to the module's particle pool 0x8D2A30): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl mf_8D2A00(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return mf_8D2A30(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}


	// 0x8D2A30 (sub_8D2A30): the engine's 0x741A70 with the module's particle pool: allocates a free
	// 0x6C particle slot from the pool [0x2760B3C] (round robin from the cursor 0x275FC80, which
	// wraps at 24: slot 24 of the 25 is never used; +0x69 in use), zeroes it, sets owner +0x5C = a1,
	// descriptor index +0x6A = a2, anchor +0x6B = a3, counts it in the actor state (+0x14) and
	// appends it to the particle list with state 0 (0x8D3C60); returns the slot or 0
	uint32_t __cdecl mf_8D2A30(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x2760B3C);
		int32_t idx = (int32_t)MEM<int16_t>(0x275FC80);
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t st = MEM<uint32_t>(ACTOR_STATE);
				U8(slot, 0x69) = 1;
				U16(st, 0x14) = (uint16_t)(U16(st, 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			idx++;
			if (idx >= 0x18)
				idx = 0;
			tries++;
		} while (tries < 0x19);
		idx++;
		if (idx < 0x18)
			MEM<uint16_t>(0x275FC80) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x275FC80) = 0;
		return slot;
	}


	// 0x8D2AD0 (sub_8D2AD0, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl mf_8D2AD0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x8D3480, a1);
		else if (mode == 4)
			callp(0x8D35D0, a1);
		mf_8D2B50(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}


	// 0x8D2B50 (sub_8D2B50, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl mf_8D2B50(uint32_t a1)
	{
		uint32_t sys = U32(0x2760B60, 0);
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
				mf_8D2BD0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			mf_8D2BD0(a1, def, (uint32_t)rest);
		return 0; // void
	}


	// 0x8D2BD0 (sub_8D2BD0, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl mf_8D2BD0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x2760B50, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x2760B50, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = mf_8D33B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x2760B60, 0);
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
		mf_8D40C0(node, desc);
		U32(0x2760B50, 0) = U32(0x2760B50, 0) + 0x50;
		return 0; // void
	}


	// 0x8D33B0 (sub_8D33B0 of wb_87CB40 0x87CB40: sub_87CB40; = Curaga cg_882080 with this module's pool of 0x50 records): the
	// engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes from the pool
	// [0x26D74E4] starting at the cursor 0x26D74D8, cleared, owner a1, definition index a2; the
	// cursor wraps at 0x4F)
	uint32_t __cdecl mf_8D33B0(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x2760B54);
		int32_t i = MEM<int16_t>(0x2760B48);
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
			if (i >= 0x4F)
				i = 0;
			tries++;
			if (tries >= 0x50)
				break;
		}
		i++;
		if (i >= 0x4F)
			MEM<uint16_t>(0x2760B48) = 0;
		else
			MEM<uint16_t>(0x2760B48) = (uint16_t)i;
		return slot;
	}


	// 0x8D35D0 (sub_8D35D0, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl mf_8D35D0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x2760B60, 0);            // ecx
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
				uint32_t g = U32(0x2760B60, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x2760B60, 0);
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


	// 0x8D40C0 (sub_8D40C0, copy of a_743100 0x743100, callee chain to the module's particle pool 0x8D2A30): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl mf_8D40C0(uint32_t a1, uint32_t a2)
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
			mf_8D2A30(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			mf_8D2A30(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			mf_8D2A30(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			mf_8D2A30(obj, id, variant);
		}
		return 0; // void
	}


	// 0x8D4150 (sub_8D4150, copy of a_743190 0x743190, callee chain to the module's particle pool 0x8D2A30): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via mf_8D2A30(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl mf_8D4150(void)
	{
		uint32_t dir = MEM<uint32_t>(0x2760B60);  // eax (re-read only after the calls)
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
					mf_8D2A30(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x2760B60);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				mf_8D2A30(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x2760B60);
			}
		}
		return 0; // void
	}


	// 0x8D41E0 (sub_8D41E0, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl mf_8D41E0(uint32_t a1)
	{
		if (mf_8CF3D0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}


	// 0x8D4210 (MAG_007_sub_8D4210): emitter state 1 - at tick 7 releases the master (root +0x63
	// = 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl mf_8D4210(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x7)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}


	// 0x8D4230 (MAG_007_sub_8D4230): emitter state 2 - from tick 10: the action result (the heal) of the
	// action's current target (+0x2A action, +0x2B target record of 24 bytes), finished, next state
	uint32_t __cdecl mf_8D4230(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0xA)
			return 0; // void
		const int32_t ai = S8(node, 0x2A);
		const int32_t ti = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t recs = U32(acts + (uint32_t)(ai * 5) * 4, 8);
		x::ApplyActionResultToTarget(recs + (uint32_t)(ti * 3) * 8);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}


	// 0x8D4280 (MAG_007_sub_8D4280): master state 4 - action loop: once the emitter released the
	// master (+0x63 = 0): the next target record (+0x2B) and action counter +0x2E, back to the
	// emitter state; after the last one: wait 13 ticks (+0x60), next state
	uint32_t __cdecl mf_8D4280(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return node;
		const int8_t t = S8(node, 0x2B);
		if ((int32_t)t < (int32_t)S16(node, 0x5A) - 1)
		{
			U8(node, 0x2B) = (uint8_t)(t + 1);
			U8(node, 0x2E) = (uint8_t)(U8(node, 0x2E) + 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) - 1);
			return node;
		}
		U16(node, 0x60) = 0xD;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return node;
	}


	// 0x8D42D0 (MAG_007_sub_8D42D0): master state - counts +0x60 down; at 0 starts the stage fade
	// out (0x8D4300, 0x40 bytes, stage queue), next state
	uint32_t __cdecl mf_8D42D0(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8D4300, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}


	// 0x8D4360 (MAG_007_sub_8D4360): stage fade out state 0 - level +0x1C = 0x400, next state
	uint32_t __cdecl mf_8D4360(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}


	// 0x8D4380 (MAG_007_sub_8D4380, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl mf_8D4380(uint32_t a1)
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

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8CE630, (void *)a_73A0D0, "007 MAG_007_sub_8CE630" },
		{ 0x8CE640, (void *)a_73A0D0, "007 MAG_007_sub_8CE640" },
		{ 0x8CE6E0, (void *)a_73A380, "007 MAG_007_sub_8CE6E0" },
		{ 0x8CE740, (void *)a_7335D0, "007 MAG_007_sub_8CE740" },
		{ 0x8CE7C0, (void *)a_73A6D0, "007 nullsub_1849" },
		{ 0x8CE7D0, (void *)a_73A170, "007 MAG_007_sub_8CE7D0" },
		{ 0x8CE8C0, (void *)a_73C100, "007 MAG_007_sub_8CE8C0" },
		{ 0x8CE910, (void *)a_73A380, "007 MAG_007_sub_8CE910" },
		{ 0x8CEE40, (void *)a_73FE90, "007 sub_8CEE40" },
		{ 0x8CF170, (void *)a_740210, "007 sub_8CF170" },
		{ 0x8D0640, (void *)a_73D770, "007 sub_8D0640" },
		{ 0x8D0890, (void *)a_73D9C0, "007 sub_8D0890" },
		{ 0x8D18D0, (void *)a_740910, "007 sub_8D18D0" },
		{ 0x8D1F30, (void *)a_740F70, "007 sub_8D1F30" },
		{ 0x8D1F80, (void *)a_740FC0, "007 sub_8D1F80" },
		{ 0x8D2010, (void *)a_741050, "007 sub_8D2010" },
		{ 0x8D20E0, (void *)a_741120, "007 sub_8D20E0" },
		{ 0x8D2B20, (void *)a_741B60, "007 sub_8D2B20" },
		{ 0x8D3250, (void *)a_742290, "007 au_re__rand_70" },
		{ 0x8D3280, (void *)a_7422C0, "007 sub_8D3280" },
		{ 0x8D32C0, (void *)a_742300, "007 sub_8D32C0" },
		{ 0x8D3310, (void *)a_742350, "007 sub_8D3310" },
		{ 0x8D3360, (void *)a_7423A0, "007 au_re__rand_65_0" },
		{ 0x8D3470, (void *)a_7424B0, "007 sub_8D3470" },
		{ 0x8D3480, (void *)a_7424C0, "007 sub_8D3480" },
		{ 0x8D3C60, (void *)a_742CA0, "007 sub_8D3C60" },
		{ 0x8D3C90, (void *)a_742CD0, "007 sub_8D3C90" },
		{ 0x8D3CC0, (void *)a_742D00, "007 sub_8D3CC0" },
		{ 0x8D3E70, (void *)a_742EB0, "007 sub_8D3E70" },
		{ 0x8D3FB0, (void *)a_742FF0, "007 sub_8D3FB0" },
		{ 0x8D4200, (void *)a_73A6D0, "007 nullsub_1850" },
		{ 0x8D4270, (void *)a_73A6D0, "007 nullsub_1851" },
		{ 0x8D4300, (void *)a_73A380, "007 MAG_007_sub_8D4300" },
		{ 0x8D43C0, (void *)a_73A6D0, "007 nullsub_1852" },
		{ 0x8D43D0, (void *)a_747550, "007 MAG_007_sub_8D43D0" },
		{ 0x8D43E0, (void *)a_73A0D0, "007 MAG_007_sub_8D43E0" },
		{ 0x8D43F0, (void *)a_73A0D0, "007 MAG_007_sub_8D43F0" },
		{ 0x8D4400, (void *)a_7475A0, "007 MAG_007_sub_8D4400" },
		{ 0x8D4420, (void *)a_73A6D0, "007 nullsub_1853" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x8CE4F0, (void *)mf_8CE4F0, "007 MAG_007_sub_8CE4F0" },
		{ 0x8CE650, (void *)mf_8CE650, "007 MAG_007_sub_8CE650" },
		{ 0x8CE6A0, (void *)mf_8CE6A0, "007 MAG_007_sub_8CE6A0" },
		{ 0x8CE780, (void *)mf_8CE780, "007 MAG_007_sub_8CE780" },
		{ 0x8CE800, (void *)mf_8CE800, "007 MAG_007_sub_8CE800" },
		{ 0x8CE890, (void *)mf_8CE890, "007 MAG_007_sub_8CE890" },
		{ 0x8CE970, (void *)mf_8CE970, "007 sub_8CE970" },
		{ 0x8CE9A0, (void *)mf_8CE9A0, "007 sub_8CE9A0" },
		{ 0x8CEB30, (void *)mf_8CEB30, "007 sub_8CEB30" },
		{ 0x8CEC10, (void *)mf_8CEC10, "007 sub_8CEC10" },
		{ 0x8CED70, (void *)mf_8CED70, "007 sub_8CED70" },
		{ 0x8CEF60, (void *)mf_8CEF60, "007 sub_8CEF60" },
		{ 0x8CF3D0, (void *)mf_8CF3D0, "007 sub_8CF3D0" },
		{ 0x8CF460, (void *)mf_8CF460, "007 sub_8CF460" },
		{ 0x8CF4C0, (void *)mf_8CF4C0, "007 sub_8CF4C0" },
		{ 0x8CF660, (void *)mf_8CF660, "007 sub_8CF660" },
		{ 0x8CF790, (void *)mf_8CF790, "007 sub_8CF790" },
		{ 0x8CF9B0, (void *)mf_8CF9B0, "007 sub_8CF9B0" },
		{ 0x8CFC40, (void *)mf_8CFC40, "007 sub_8CFC40" },
		{ 0x8D00C0, (void *)mf_8D00C0, "007 sub_8D00C0" },
		{ 0x8D0B70, (void *)mf_8D0B70, "007 sub_8D0B70" },
		{ 0x8D1070, (void *)mf_8D1070, "007 sub_8D1070" },
		{ 0x8D16C0, (void *)mf_8D16C0, "007 sub_8D16C0" },
		{ 0x8D1780, (void *)mf_8D1780, "007 sub_8D1780" },
		{ 0x8D1800, (void *)mf_8D1800, "007 sub_8D1800" },
		{ 0x8D1840, (void *)mf_8D1840, "007 sub_8D1840" },
		{ 0x8D2870, (void *)mf_8D2870, "007 sub_8D2870" },
		{ 0x8D2980, (void *)mf_8D2980, "007 sub_8D2980" },
		{ 0x8D2A00, (void *)mf_8D2A00, "007 sub_8D2A00" },
		{ 0x8D2A30, (void *)mf_8D2A30, "007 sub_8D2A30" },
		{ 0x8D2AD0, (void *)mf_8D2AD0, "007 sub_8D2AD0" },
		{ 0x8D2B50, (void *)mf_8D2B50, "007 sub_8D2B50" },
		{ 0x8D2BD0, (void *)mf_8D2BD0, "007 sub_8D2BD0" },
		{ 0x8D33B0, (void *)mf_8D33B0, "007 sub_8D33B0" },
		{ 0x8D35D0, (void *)mf_8D35D0, "007 sub_8D35D0" },
		{ 0x8D40C0, (void *)mf_8D40C0, "007 sub_8D40C0" },
		{ 0x8D4150, (void *)mf_8D4150, "007 sub_8D4150" },
		{ 0x8D41E0, (void *)mf_8D41E0, "007 sub_8D41E0" },
		{ 0x8D4210, (void *)mf_8D4210, "007 MAG_007_sub_8D4210" },
		{ 0x8D4230, (void *)mf_8D4230, "007 MAG_007_sub_8D4230" },
		{ 0x8D4280, (void *)mf_8D4280, "007 MAG_007_sub_8D4280" },
		{ 0x8D42D0, (void *)mf_8D42D0, "007 MAG_007_sub_8D42D0" },
		{ 0x8D4360, (void *)mf_8D4360, "007 MAG_007_sub_8D4360" },
		{ 0x8D4380, (void *)mf_8D4380, "007 MAG_007_sub_8D4380" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag007_mega_phoenix()
	{
		act::register_module(7);
		for (const act::megaphoenix::ModPort *p = act::megaphoenix::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(7, p->addr, p->port, p->name);
		for (const act::megaphoenix::ModPort *p = act::megaphoenix::PORTS; p->addr; p++)
			act::register_module_port(7, p->addr, p->port, p->name);
		// 30 fps layer: see mag007_mega_phoenix_held.inc
		FX_HELD(register_mag007_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag007_mega_phoenix_held.inc"
#endif
