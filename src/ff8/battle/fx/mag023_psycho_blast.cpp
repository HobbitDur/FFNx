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

// Effect 23: Psycho Blast (enemy attack 168 of kernel.bin, MAG_023_PSYCHO_BLAST: NORG c0m077; no other
// kernel entry uses effect 23): a copy of the actor/heal effect library (see act_engine.h) built like Raldo
// Throw, plus the module's own prim-model tasks (the psychic orb, a ring and the burst) and debris.
//
// Setup MAG_023_PSYCHO_BLAST_Init 0x893020 (runs once, not ported; file loader 0x893000 = the effect's
// data file -> 0x27011B8, its two packet arenas at file + 0 / + 0x8000 by tick parity, arena cursor
// 0x27044D4 = file + 0x10000; camera animation 0x1614384 and the file's TIM upload unless the cast
// flags +1 bit 0) creates the root queue 0x27013D0 with the master and six task pools: 0x2704038
// (4 x 0x58: emitters), 0x27012A0 (3 x 0x48: director), 0x2703D98 (1 x 0x2A4: the actor system),
// 0x2703AD8 (3 x 0x180: prim-model tasks), 0x27013E0 (50 x 0xB0: debris spawner and debris),
// 0x2704028 (10 x 0x40: the engine's stage wobble task 0x8DDC30).
//   Master (0x893190) - camera copy 0x2793E58 (the module scratch stack 0x27044C8 grows down from it),
//     packet arena by tick parity (cursor 0x27011BC), bone follow, 11-state table: 0x893320 carves
//     the actor pools after the arena (0x438 + 0xD200 bytes: particles and 80 actors of 0x2A0),
//     0x8933A0 one emitter per action (engine loop 0x89A5C0), then waits for the queues to empty.
//   Emitter (0x8933D0, engine task a_73A1A0), node +0x2C = caster, +0x2D = target: 0x893480 clears
//     the director block [0x1614284] (0x54 bytes), keeps the target's position and the facing from
//     the origin to it (+0x4A) and starts the director; 0x89A530 applies the damage once the burst
//     started (director +0x48 == 1).
//   Director (0x893500): phase bookkeeping 0x893580 (the phase the caster's animation requests in
//     director +0x42 / +0x44) - phase 1 (0x893830) starts the actor system (0x896250 = engine 0x73A380
//     on {0x8962B0, 0x899B50, ret}: actor data 0x17EA4F0, start tick 0, placed at the orb point), the
//     ring (0x896150, prim layout 0x17EE254), the orb (0x893950, prim layout 0x17F01B0) and the stage
//     wobble task 0x8DDC30 (data 0x16146C8, 0x27 ticks); at cue 1 (0x8935D0) the geometry block
//     [0x16146C4] is set (0x893630: the caster's facing matrix, two points in front of and above the
//     caster (+0x20, +0x28: the orb), the target's anchor 0xF0 (+0x30), the matrix aiming the orb at
//     the target (0x8DDAC0)); 4 ticks into the phase the sound 0x1614288; then cue 2 and the end.
//   Prim-model tasks (0x180 bytes, 0x893900; drawn by 0x893A30 = the prim-model player 0x701970 at
//     node +0x94 with the callback 0x893B20 and the module renderer 0x893DE0): the ring and the orb
//     appear 0x23 ticks after their spawn with the block's matrix at the orb point; the orb then flies
//     to 0x200 short of the target (+0x556 / 0x1000 a tick), spawns the burst (0x895FF0, prim layout
//     0x17F1744, which sets the director's hit flag +0x48) and the debris spawner (0x899B80), and
//     flattens (y scale 0x1000 -> 0x800 -> 0x400) at the target.
//   Debris (0x899B80): 8 debris tasks (0x899CC0) on its first tick, 4 on its second, each with 4
//     sprites (flipbook 0x161428C, colour 0xFFC0) thrown from the target at a random angle and speed
//     (velocity -1/8 a tick, position += velocity / 16).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x27011B8..0x27044E0 (file pointer 0x27011B8, packet cursor 0x27011BC, pools,
// queues, arenas, actor state 0x27044D8, scratch stack pointer 0x27044C8, vertex blend buffer
// 0x2704220), the director block [0x1614284], the geometry block [0x16146C4], the module scratch
// stack below the camera copy 0x2793E58; the actor data 0x17EA4F0.. in exe data is rewritten in place.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace psycho
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	// (F_73B640 = 0x893600, the director cue gate writing +0x44; gendesc also saw 0x893780 there, as in Raldo Throw)
	static const Mod MOD_023 = { "psycho", 23, 0x893000, 0x89A690,
		{ 0x0, 0x0, 0x1614284, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x27011B8, 0x0, 0x0, 0x0, 0x2701304, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2703D98, 0x0, 0x2704038, 0x0, 0x0, 0x0, 0x0, 0x0, 0x270420C, 0x0, 0x0, 0x27044C0, 0x0, 0x27044C8, 0x0, 0x27044D8 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x896E70, 0x896ED0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x897030, 0x897170, 0x8971B0, 0x898440, 0x898490, 0x8984C0, 0x898540, 0x898C30, 0x898C80, 0x898D20, 0x898DE0, 0x899630, 0x899A30, 0x899AC0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8933D0, 0x893450, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x893600, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8964B0, 0x896590, 0x896730, 0x896800, 0x896920, 0x896B80, 0x8970F0, 0x897240, 0x8978A0, 0x8978F0, 0x897980, 0x897A50, 0x8981E0, 0x8982F0, 0x898370, 0x8983A0, 0x898BC0, 0x898BF0, 0x898CD0, 0x898DF0, 0x898F40, 0x0, 0x899600, 0x8997E0, 0x899920, 0x89A030, 0x896DE0, 0x89A530, 0x89A570, 0x89A590, 0x89A5B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x27011BC;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x27044D8;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x27044C8;      // module scratch stack pointer
	static const uint32_t DIR_PTR = 0x1614284;         // director block pointer (0x54 bytes)
	static const uint32_t GEO_PTR = 0x16146C4;         // geometry block pointer (matrix, orb point, target)
	static const uint32_t Q_EMITTER = 0x2704038, Q_DIRECTOR = 0x27012A0, Q_ACTORS = 0x2703D98, Q_PRIM = 0x2703AD8, Q_DEBRIS = 0x27013E0, Q_STAGE = 0x2704028;
	static const uint32_t ORIG_Master = 0x893190;
	static const uint32_t ORIG_Orb = 0x893950, ORIG_Ring = 0x896150, ORIG_Burst = 0x895FF0;
	static const uint32_t ORIG_Spawner = 0x899B80, ORIG_Debris = 0x899CC0;
	static const uint32_t PRIM_CB = 0x893B20;          // the prim-model player's object callback
	static const uint32_t BLEND_BUF = 0x2704220;       // vertex blend buffer of the prim callback
	static const uint32_t SOUND_Blast = 0x1614288;
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t sub_8DDAC0(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DDAC0)(a1, a2, a3); }
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

	// director block and state helpers of the Tonberry / Death copies (the module's director block [0x1614284])
	static inline uint32_t t1_data() { return MEM<uint32_t>(DIR_PTR); }
	static inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl pb_893190(uint32_t a1);
	uint32_t __cdecl pb_893320(uint32_t a1);
	uint32_t __cdecl pb_893360(void);
	uint32_t __cdecl pb_893450(uint32_t a1);
	uint32_t __cdecl pb_893480(uint32_t a1);
	uint32_t __cdecl pb_893500(uint32_t a1);
	uint32_t __cdecl pb_893580(uint32_t a1);
	uint32_t __cdecl pb_8935D0(uint32_t a1);
	uint32_t __cdecl pb_893630(uint32_t a1);
	uint32_t __cdecl pb_893760(uint32_t a1);
	uint32_t __cdecl pb_8937A0(uint32_t a1);
	uint32_t __cdecl pb_893830(uint32_t a1);
	uint32_t __cdecl pb_893900(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl pb_893950(uint32_t a1);
	uint32_t __cdecl pb_8939B0(uint32_t a1);
	uint32_t __cdecl pb_893A30(uint32_t a1);
	uint32_t __cdecl pb_893B20(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl pb_893DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_893F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_894130(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_8943C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_894840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_8952F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_8957F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl pb_895E40(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_895ED0(uint32_t a1);
	uint32_t __cdecl pb_896050(uint32_t a1);
	uint32_t __cdecl pb_8960C0(uint32_t a1);
	uint32_t __cdecl pb_8960F0(uint32_t a1);
	uint32_t __cdecl pb_8961B0(uint32_t a1);
	uint32_t __cdecl pb_896220(uint32_t a1);
	uint32_t __cdecl pb_8962B0(uint32_t a1);
	uint32_t __cdecl pb_8964B0(uint32_t a1);
	uint32_t __cdecl pb_896730(void);
	uint32_t __cdecl pb_896DE0(uint32_t a1);
	uint32_t __cdecl pb_896E70(void);
	uint32_t __cdecl pb_896ED0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_897030(void);
	uint32_t __cdecl pb_8970F0(uint32_t a1);
	uint32_t __cdecl pb_897170(void);
	uint32_t __cdecl pb_8971B0(uint32_t a1);
	uint32_t __cdecl pb_8981E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_8982F0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_898370(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_8983A0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl pb_898440(uint32_t a1);
	uint32_t __cdecl pb_8984C0(uint32_t a1);
	uint32_t __cdecl pb_898540(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl pb_898D20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_898F40(uint32_t a1);
	uint32_t __cdecl pb_899A30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl pb_899AC0(void);
	uint32_t __cdecl pb_899B50(uint32_t a1);
	uint32_t __cdecl pb_899B80(uint32_t a1);
	uint32_t __cdecl pb_899BD0(uint32_t a1);
	uint32_t __cdecl pb_899CC0(uint32_t a1);
	uint32_t __cdecl pb_899DD0(uint32_t a1);
	uint32_t __cdecl pb_89A4A0(uint32_t a1);
	uint32_t __cdecl pb_89A4C0(uint32_t a1);
	uint32_t __cdecl pb_89A530(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag023_psycho_blast_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace psycho
{
	// ====================================================================================
	// the library functions of this module whose code matches a Raldo Throw / NORG Pod / Death / Magma Breath /
	// Wind Blast / Doom / Tonberry / Siren / Shell port up to the module's addresses and callees (the
	// port's text with the module's addresses; "copy of" names the source), then the module's own code
	// ====================================================================================

	// 0x893320 (copy of rt_8A3570 0x8A3570: copy of dk_856CC0 0x856CC0: copy of dw_8B0BC0 0x8B0BC0: copy of dm_8C4CE0 0x8C4CE0: copy of d_8B6BE0 0x8B6BE0: MAG_014_sub_8B6BE0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x27044D4] (0x438 + 0xD200 bytes) and cleared, next state
	uint32_t __cdecl pb_893320(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x27044D4);
		MEM<uint32_t>(0x2704210) = p;
		p += 0x438;
		MEM<uint32_t>(0x27044CC) = p;
		p += 0xD200;
		MEM<uint32_t>(0x27044D4) = p;
		pb_893360();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x893360 (copy of rt_8A35B0 0x8A35B0: copy of dk_856D00 0x856D00: copy of dw_8B0C00 0x8B0C00: copy of dm_8C4D20 0x8C4D20: copy of d_8B6C20 0x8B6C20: MAG_014_sub_8B6C20): clears the two actor pools (0x438 bytes at [0x2704210], 0xD200
	// at [0x27044CC]) and the pool cursors / counters
	uint32_t __cdecl pb_893360(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2704210), 0x438);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x27044CC), 0xD200);
		MEM<uint16_t>(0x2703650) = 0;
		MEM<uint16_t>(0x270421C) = 0;
		MEM<uint16_t>(0x27041B0) = 0;
		MEM<uint16_t>(0x27011C0) = 0;
		return 0; // void
	}

	// 0x893450 (copy of rt_8A36A0 0x8A36A0: copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - sets up the director block (0x893480), spawns the director
	// 0x893500 (0x48 bytes, director queue), next state
	uint32_t __cdecl pb_893450(uint32_t a1)
	{
		pb_893480(a1);
		x::Effect_AddTaskAndInitFromCtx(0x27012A0, 0x893500, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x893500 (copy of rt_8A3760 0x8A3760: MAG_019_sub_8A3760): TASK director - phase bookkeeping (0x893580) every tick, then
	// its 6-state table {0x8935D0 cue 1: geometry, 0x893760 cue 1, 0x8937A0 sound at 4 ticks,
	// 0x8937D0 cue 2, 0x8937F0 finish, ret}
	uint32_t __cdecl pb_893500(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x8935D0;
		states[1] = 0x893760;
		states[2] = 0x8937A0;
		states[3] = 0x8937D0;
		states[4] = 0x8937F0;
		states[5] = 0x893810; // nullsub (ret)
		pb_893580(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x893580 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via 0x893830); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl pb_893580(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			pb_893830(a1);
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

	// 0x8935D0 (copy of t_764100 0x764100: module 090 sub_764100): director state - at director cue 1 (a_73B640) sets up the geometry
	// block (0x893630), next state.
	uint32_t __cdecl pb_8935D0(uint32_t a1)
	{
		if (a_73B640(1) != 0)
		{
			pb_893630(a1);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x893760 (copy of t_763B90 0x763B90: module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl pb_893760(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x893900 (copy of h_86B340 0x86B340: MAG_033_sub_86B340): spawns a prim-model task `fn` (0x180 bytes, prim queue)
	// under `parent`: +0x74 model data, +0x78 animation word (sign-extended), +0x80 / +0x82 modes;
	// returns the node
	uint32_t __cdecl pb_893900(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x180, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x893950 (copy of s_73F250 0x73F250: module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl pb_893950(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x8939B0, 0x895ED0, 0x8960F0, 0x896140 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return s3_task_end(a1, status);
	}

	// 0x893DE0 (copy of np_8A0BC0 0x8A0BC0: copy of Raldo Throw rt_8A4830 0x8A4830 = copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x893F10, 0x894130, 0x8943C0, 0x894840, a_73D770, a_73D9C0, 0x8952F0,
	// 0x8957F0; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl pb_893DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { pb_893F10, pb_894130, pb_8943C0, pb_894840, a_73D770, a_73D9C0, pb_8952F0, pb_8957F0 };
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

	// 0x893F10 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl pb_893F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x894130 (copy of t_764890 0x764890: module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl pb_894130(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8943C0 (copy of np_8A11A0 0x8A11A0: copy of Raldo Throw rt_8A4E10 0x8A4E10 = copy of s_73CD70 0x73CD70: module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl pb_8943C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x894840 (copy of np_8A1620 0x8A1620: copy of Raldo Throw rt_8A5290 0x8A5290 = copy of s_73D1F0 0x73D1F0: module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl pb_894840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8952F0 (copy of np_8A20D0 0x8A20D0: copy of Raldo Throw rt_8A5D40 0x8A5D40 = sub_8A5D40; same code as Siren's 0x73DCA0 / Drink Magic's 0x85D990): prim-list block
	// "Gouraud-textured triangles, UV scroll" of the module renderer 0x8A0BC0: 0x1C-byte records
	// (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C, uv1+tpage +0x10, rgb1
	// +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face (NCLIP, unless ctx+0x14
	// bit 0x20) and off-screen culling, optional lighting (bit 0x80), z = OTZ + ctx+0x10 (>= 0) >>
	// a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the UVs are scrolled (wrapped by
	// ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window packets (ctx+0x1C set, ctx+0x24
	// restore): 0x40 bytes; else the poly alone (0x28 bytes; unlike Tonberry's 0x765B30, no
	// draw-mode packet). The original keeps seven packet-field pointers apart from the cursor and
	// advances them together (always cursor + fixed offsets). Returns the new packet cursor; ctx+0x2C
	// advances past the block.
	uint32_t __cdecl pb_8952F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8957F0 (copy of np_8A25D0 0x8A25D0: copy of Raldo Throw rt_8A6240 0x8A6240 = copy of s_73E1A0 0x73E1A0: module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl pb_8957F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8960C0 (copy of dm_8C8840 0x8C8840: copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl pb_8960C0(uint32_t a1)
	{
		const uint32_t r = pb_893A30(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x896220 (copy of dm_8C8840 0x8C8840: copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl pb_896220(uint32_t a1)
	{
		const uint32_t r = pb_893A30(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x8964B0 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl pb_8964B0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x27044D8);
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

	// 0x896730 (copy of mb_825830 0x825830: copy of db_846EA0 0x846EA0: copy of cg_87E620 0x87E620: sub_87E620; = Confuse c_85FFC0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl pb_896730(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x27044D8);
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

	// 0x896DE0 (copy of r_852560 0x852560: copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl pb_896DE0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x27044D8, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			pb_899AC0();
			pb_897170();
			// 30 fps layer: see mag023_psycho_blast_held.inc
			FX_HELD(held_note_system(sys);)
			pb_897030();
			pb_896E70();
			sys = U32(0x27044D8, 0);
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
		U16(0x27044C0, 0) = (uint16_t)(U16(0x27044C0, 0) + c14);
		U16(0x270420C, 0) = (uint16_t)(U16(0x270420C, 0) + c16);
		return done;
	}

	// 0x896E70 (copy of rt_8A4670 0x8A4670: copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl pb_896E70(void)
	{
		uint32_t act = U32(U32(0x27044D8, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x27044D8, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					pb_896ED0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x896ED0 (copy of rt_8A46D0 0x8A46D0: copy of dm_8C9530 0x8C9530: copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl pb_896ED0(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = pb_893DE0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = pb_893DE0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
			}
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x897030 (copy of mb_827650 0x827650: copy of db_848D70 0x848D70: copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl pb_897030(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x27044D8), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x27044D8);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						pb_8970F0(act);   // (the original also pushes the bone entry, unused)
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
							pb_8970F0(act);
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

	// 0x8970F0 (copy of mb_827710 0x827710: copy of db_848E30 0x848E30: copy of cg_880450 0x880450: sub_880450; = Confuse c_861DF0): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl pb_8970F0(uint32_t a1)
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

	// 0x897170 (copy of mb_827790 0x827790: copy of db_848EB0 0x848EB0: copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl pb_897170(void)
	{
		uint32_t act = U32(U32(0x27044D8, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				pb_898440(act);
			else if (type == 1)
				pb_8971B0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8971B0 (copy of rt_8A6A10 0x8A6A10: copy of dw_8B3110 0x8B3110: copy of dm_8C9810 0x8C9810: copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl pb_8971B0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x27044D8);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (pb_8982F0(act, bone) == 0)
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
			st = MEM<uint32_t>(0x27044D8);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x8981E0 (copy of mb_828BA0 0x828BA0: copy of db_849F20 0x849F20: copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl pb_8981E0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x27044D8, 0);                  // actor state block
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

	// 0x8982F0 (copy of rt_8A7B50 0x8A7B50: copy of dw_8B4250 0x8B4250: copy of dm_8CA950 0x8CA950: copy of mb_828CB0 0x828CB0: copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl pb_8982F0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				pb_898370(a1, a2);
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
			pb_898370(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			pb_898370(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x898370 (copy of rt_8A7BD0 0x8A7BD0: copy of dw_8B42D0 0x8B42D0: copy of dm_8CA9D0 0x8CA9D0: copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl pb_898370(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return pb_8983A0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x8983A0 (copy of rt_8A7C00 0x8A7C00: copy of dw_8B4300 0x8B4300: copy of dm_8CAA00 0x8CAA00: copy of d_8BBC10 0x8BBC10: sub_8BBC10): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 10 records
	uint32_t __cdecl pb_8983A0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x2704210);
		int32_t idx = MEM<int16_t>(0x2703650);
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
			MEM<uint16_t>(0x2703650) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x2703650) = 0;
		return slot;
	}

	// 0x898440 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl pb_898440(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x898DF0, a1);
		else if (mode == 4)
			callp(0x898F40, a1);
		pb_8984C0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8984C0 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl pb_8984C0(uint32_t a1)
	{
		uint32_t sys = U32(0x27044D8, 0);
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
				pb_898540(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			pb_898540(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x898540 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl pb_898540(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x27044C8, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x27044C8, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = pb_898D20(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x27044D8, 0);
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
		pb_899A30(node, desc);
		U32(0x27044C8, 0) = U32(0x27044C8, 0) + 0x50;
		return 0; // void
	}

	// 0x898D20 (copy of wb_87CB40 0x87CB40: sub_87CB40; = Curaga cg_882080 with this module's pool of 0x50 records): the
	// engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes from the pool
	// [0x26D74E4] starting at the cursor 0x26D74D8, cleared, owner a1, definition index a2; the
	// cursor wraps at 0x4F)
	uint32_t __cdecl pb_898D20(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x27044CC);
		int32_t i = MEM<int16_t>(0x270421C);
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
			MEM<uint16_t>(0x270421C) = 0;
		else
			MEM<uint16_t>(0x270421C) = (uint16_t)i;
		return slot;
	}

	// 0x898F40 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl pb_898F40(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x27044D8, 0);            // ecx
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
				uint32_t g = U32(0x27044D8, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x27044D8, 0);
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

	// 0x899A30 (copy of rt_8A9290 0x8A9290: copy of dw_8B5990 0x8B5990: copy of dm_8CC090 0x8CC090: copy of mb_82A3F0 0x82A3F0: copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl pb_899A30(uint32_t a1, uint32_t a2)
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
			pb_8983A0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			pb_8983A0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			pb_8983A0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			pb_8983A0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x899AC0 (copy of rt_8A9320 0x8A9320: copy of dw_8B5A20 0x8B5A20: copy of dm_8CC120 0x8CC120: copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via pb_8983A0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl pb_899AC0(void)
	{
		uint32_t dir = MEM<uint32_t>(0x27044D8);  // eax (re-read only after the calls)
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
					pb_8983A0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x27044D8);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				pb_8983A0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x27044D8);
			}
		}
		return 0; // void
	}

	// 0x899B50 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl pb_899B50(uint32_t a1)
	{
		if (pb_896DE0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x899B80 (copy of mb_82A540 0x82A540: copy of c_85FA30 0x85FA30: MAG_036_sub_85FA30): CAMERA SHAKE task - state {0x85FA80 shake step, 0x85FAB0 ret}
	uint32_t __cdecl pb_899B80(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = 0x899BD0;
		states[1] = 0x89A520; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x899DD0 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl pb_899DD0(uint32_t a1)
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
		const uint32_t cursor = MEM<uint32_t>(0x27011BC);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x27011BC) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x89A4A0 (copy of np_8A2CB0 0x8A2CB0: sub_8A2CB0; Raldo Throw rt_8AA9B0 0x8AA9B0 with other values): debris state 0 - sprite +0x4C =
	// 0x161428C, +0x56 = -0x40, +0x52 = 8, advance.
	uint32_t __cdecl pb_89A4A0(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x161428C;
		st++;
		U16(node, 0x56) = 0xFFC0;
		U16(node, 0x52) = 8;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x89A4C0 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl pb_89A4C0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x89A530 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl pb_89A530(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1614284);
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
	// the module's own code
	// ====================================================================================

	// 0x893190 (MAG_023_sub_893190; Raldo Throw rt_8A33F0 with this module's table and six queues):
	// MASTER task - camera copy, packet arena by tick parity, bone follow, 11-state table
	// {0x893300, 0x893310 (a_73A0D0), 0x893320 carve, 0x8933A0 emitters, 0x89A5C0 .. 0x89A660, ret},
	// the six queues (live task count -> node +0x5E; the actor counters 0x27044C0 / 0x270420C are
	// cleared before the queues run)
	uint32_t __cdecl pb_893190(uint32_t a1)
	{
		g_mod = &MOD_023;
		// 30 fps layer: see mag023_psycho_blast_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x2701304) = 0x2793E58;
		states[0] = 0x893300;
		states[1] = 0x893310;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x893320;
		states[3] = 0x8933A0;
		states[4] = 0x89A5C0;
		states[5] = 0x89A600;
		states[6] = 0x89A610;
		states[7] = 0x89A620;
		states[8] = 0x89A650;
		states[9] = 0x89A660;
		states[10] = 0x89A680; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x27041AC);
			const uint32_t v2 = MEM<uint32_t>(0x2703AEC);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2701300) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x27041A8);
			const uint32_t v2 = MEM<uint32_t>(0x2703AE8);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2701300) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x27044C0) = 0;
		MEM<uint16_t>(0x270420C) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DEBRIS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PRIM));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x893480 (MAG_023_sub_893480): emitter a1 - clears the director block [0x1614284] (0x54 bytes),
	// +8 / +0xC = the target's (+0x2D) position words, origin +0..+4 = 0, facing +0x4A = the angle of
	// the target seen from the origin - 0x800 (12 bits)
	uint32_t __cdecl pb_893480(uint32_t a1)
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

	// 0x893630 (sub_893630): director cue-1 set-up of the geometry block [0x16146C4]: the caster's (+0x2C)
	// facing matrix (entity +0x0E, 0x8DD770 / 0x8DD8A0) turns (0, -0xE00, -0x400) and (0, -0xE00,
	// -0x978): +0x20 / +0x28 = the caster's position words + those (the orb point is +0x28); +0x30 =
	// the target's (+0x2D) effect anchor 0xF0 (0x502170); the block's matrix +0 aims from +0x28 at
	// +0x30 (0x8DDAC0)
	uint32_t __cdecl pb_893630(uint32_t a1)
	{
		// [esp+0x10] frame: +0 SVECTOR (the +6 pad is never written: matrixMultiplyVector reads and
		// writes x, y, z only), +8 the 0x20-byte rotation matrix
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t V = P(loc);
		const uint32_t M = V + 8;
		const uint32_t caster = ENT(U8(a1, 0x2C));
		const uint32_t target = ENT(U8(a1, 0x2D));
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(caster, 0xE));
		U16(V, 0) = 0;
		U16(V, 2) = 0xF200;
		U16(V, 4) = 0xFC00;
		x::matrixMultiplyVector(M, V, V);
		uint32_t b = MEM<uint32_t>(GEO_PTR);
		U32(b, 0x20) = U32(caster, 0x1C);
		U32(b, 0x24) = U32(caster, 0x20);
		U16(b, 0x20) = (uint16_t)(U16(b, 0x20) + U16(V, 0));
		U16(b, 0x22) = (uint16_t)(U16(b, 0x22) + U16(V, 2));
		U16(b, 0x24) = (uint16_t)(U16(b, 0x24) + U16(V, 4));
		U16(V, 0) = 0;
		U16(V, 2) = 0xF200;
		U16(V, 4) = 0xF688;
		x::matrixMultiplyVector(M, V, V);
		b = MEM<uint32_t>(GEO_PTR);
		U32(b, 0x28) = U32(caster, 0x1C);
		U32(b, 0x2C) = U32(caster, 0x20);
		U16(b, 0x28) = (uint16_t)(U16(b, 0x28) + U16(V, 0));
		U16(b, 0x2A) = (uint16_t)(U16(b, 0x2A) + U16(V, 2));
		U16(b, 0x2C) = (uint16_t)(U16(b, 0x2C) + U16(V, 4));
		x::GetEffectSpawnPosition(target, 0xF0, 0, b + 0x30);
		b = MEM<uint32_t>(GEO_PTR);
		return gx::sub_8DDAC0(b, b + 0x28, b + 0x30);
	}

	// 0x8937A0 (sub_8937A0): director state - 4 ticks into the phase plays the sound 0x1614288
	// (BdPlaySE, volume 0x80), next state
	uint32_t __cdecl pb_8937A0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) < 4)
			return 0; // void
		x::BdPlaySE(SOUND_Blast, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x893830 (sub_893830): phase start (from 0x893580) - phase 1 starts the actor system task 0x896250
	// (a_73C100: data 0x17EA4F0, start tick 0, +0x29C = 4, +0x29E = 0x28), the ring 0x896150 (prim
	// layout 0x17EE254, 0xD8) and the orb 0x893950 (prim layout 0x17F01B0, 0x58, +0x80 = 1) prim-model
	// tasks (0x893900), and the engine's stage wobble task 0x8DDC30 (stage queue, +0x34 = script
	// 0x16146C8, +0x38 = 0x27 ticks)
	uint32_t __cdecl pb_893830(uint32_t a1)
	{
		const int32_t ph = (int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x40) - 1;
		if (ph != 0)
			return (uint32_t)ph;
		a_73C100(a1, 0x896250, 0x17EA4F0, 0, 0x28, 4);
		pb_893900(a1, ORIG_Ring, 0x17EE254, 0xD8, 0, 0);
		pb_893900(a1, ORIG_Orb, 0x17F01B0, 0x58, 1, 0);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8DDC30, 0x40, a1);
		U32(t, 0x34) = 0x16146C8;
		U16(t, 0x38) = 0x27;
		return t;
	}

	// 0x8939B0 (sub_8939B0): orb state 0 - from tick 0x23: decodes the prim layout (+0x74 -> +0x94,
	// +0x78), scale 1.0, the block's matrix (+0x30), position = the orb point (+0x1C and start +0x16C),
	// the flight's end point +0x174 (0x895E40), draws (0x893A30), next state
	uint32_t __cdecl pb_8939B0(uint32_t a1)
	{
		const uint32_t n = a1;
		if (S16(n, 0x24) < 0x23)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(n, 0x74), n + 0x94, U32(n, 0x78));
		U32(n, 0x58) = 0x1000;
		U32(n, 0x54) = 0x1000;
		U32(n, 0x50) = 0x1000;
		const uint32_t b = MEM<uint32_t>(GEO_PTR);
		const uint32_t w2c = U32(b, 0x2C);
		memcpy((void *)(n + 0x30), (const void *)b, 0x20); // rep movsd
		const uint32_t w28 = U32(b, 0x28);
		U32(n, 0x1C) = w28;
		U32(n, 0x16C) = w28;
		U32(n, 0x20) = w2c;
		U32(n, 0x170) = w2c;
		pb_895E40(n, n + 0x174);
		pb_893A30(n);
		U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		return 0; // void
	}

	// 0x893A30 (sub_893A30; engine a_73C280 with the node's matrix +0x30 instead of its angles and no
	// pause): draws the node's prim model: matrix = camera * (matrix +0x30 scaled by +0x50,
	// translation +0x1C..0x20), plays the prim player at +0x94 with callback 0x893B20 and a parameter
	// block (matrix + node colours/params, blend buffer 0x2704220); returns the player's result
	uint32_t __cdecl pb_893A30(uint32_t a1)
	{
		// stack frame 0x5C: [0..0x1F] matrix, [0x38] a1+0x70, [0x44] a1+0x7C, [0x48] blend buffer,
		// [0x4C..0x59] words 8C 8E 90 92 86 80 88 (0x20..0x37, 0x3C..0x43, 0x5A are not written;
		// the callback 0x893B20 does not read them)
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		memcpy((void *)L, (const void *)(a1 + 0x30), 0x20); // rep movsd
		x::scale3DMatrix(L, a1 + 0x50);
		S32(L, 0x14) = S16(a1, 0x1C);
		S32(L, 0x18) = S16(a1, 0x1E);
		S32(L, 0x1C) = S16(a1, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(a1, 0x86);
		U32(L, 0x44) = U32(a1, 0x7C);
		U32(L, 0x38) = U32(a1, 0x70);
		U16(L, 0x4C) = U16(a1, 0x8C);
		U16(L, 0x4E) = U16(a1, 0x8E);
		U16(L, 0x52) = U16(a1, 0x92);
		U16(L, 0x50) = U16(a1, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(a1, 0x80);
		U16(L, 0x58) = U16(a1, 0x88);
		// 30 fps layer: see mag023_psycho_blast_held.inc
		FX_HELD(held_note_play(a1);)
		return prim_play(a1 + 0x94, PRIM_CB, L, 0);
	}

	// 0x893B20 (sub_893B20; Doom dm_8C5B10 without the 0x400 rotation variant, plus flag 0x2000):
	// prim-model player draw callback (layout a1, record a2, block a3) - picks the object's vertex
	// frame (lerped by MAG_017_sub_701390 into the block's buffer +0x48), builds its matrix (rotation
	// 0x701310; parent-rotated position unless record flag 0x200; scale: a diagonal matrix product
	// 0x56C220 with flag 0x100, else scale3DMatrix), fills a 0x68-byte Field_Alloc render header (flags
	// 0x2000 / 0x2030, |0xC with record flag 0x2000, |0xC0 with alpha, colour, depth offset block
	// +0x58, scale words 0x100) and draws it with 0x893DE0 into OT base+0x44 (shift 2) at the module
	// packet cursor 0x27011BC
	uint32_t __cdecl pb_893B20(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten, never read), +8 scale (3 x int32, or the 3x3
		// s16 diagonal matrix of the 0x100 path), +0x28 Mat4x3 object matrix (+0x3C translation)
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
				U16(S, 8) = U16(rec, 0x1A);
				U16(S, 0) = U16(rec, 0x18);
				U16(S, 0xA) = 0;
				U16(S, 4) = 0;
				U16(S, 0xE) = 0;
				U16(S, 2) = 0;
				U16(S, 0xC) = 0;
				U16(S, 6) = 0;
				U16(S, 0x10) = U16(rec, 0x1C);
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
		if ((flags & 0x2000) != 0)
			U32(hdr, 0x14) = U32(hdr, 0x14) | 0xC;
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
		MEM<uint32_t>(PACKET_CURSOR) = pb_893DE0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x895E40 (sub_895E40): a2 (3 words) = the target anchor (block +0x30..+0x34) + (0, 0, -0x200)
	// turned by the node's matrix +0x30 scaled by +0x50 (the point short of the target the orb
	// flies to)
	uint32_t __cdecl pb_895E40(uint32_t a1, uint32_t a2)
	{
		// [esp+8] frame: +0 SVECTOR (+6 pad never written), +8 the 0x20-byte matrix
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t V = P(loc);
		const uint32_t M = V + 8;
		U16(V, 0) = 0;
		U16(V, 2) = 0;
		memcpy((void *)M, (const void *)(a1 + 0x30), 0x20); // rep movsd
		U16(V, 4) = 0xFE00;
		x::scale3DMatrix(M, a1 + 0x50);
		x::matrixMultiplyVector(M, V, V);
		const uint32_t b = MEM<uint32_t>(GEO_PTR);
		U16(a2, 0) = (uint16_t)(U16(b, 0x30) + U16(V, 0));
		const uint16_t y = (uint16_t)(U16(b, 0x32) + U16(V, 2));
		const uint16_t z = (uint16_t)(U16(b, 0x34) + U16(V, 4));
		U16(a2, 2) = y;
		U16(a2, 4) = z;
		return z; // eax = the last sum (ax)
	}

	// 0x895ED0 (sub_895ED0): orb state 1 - flight: progress +0x17C += 0x556; below 0x1000 the position
	// +0x1C..+0x20 = start +0x16C.. + (end +0x174.. - start) * progress / 0x1000 (per axis, C
	// division); at 0x1000: position = the end point (0x895E40), spawns the burst (0x893900: 0x895FF0,
	// prim layout 0x17F1744, 0xD8, +0x80 = 2) and the debris spawner (0x899B80, 0xB0, debris queue)
	// there, next state; draws (0x893A30)
	uint32_t __cdecl pb_895ED0(uint32_t a1)
	{
		const uint32_t n = a1;
		U16(n, 0x17C) = (uint16_t)(U16(n, 0x17C) + 0x556);
		const int16_t t = S16(n, 0x17C);
		if (t >= 0x1000)
		{
			const uint32_t b = MEM<uint32_t>(GEO_PTR);
			U32(n, 0x1C) = U32(b, 0x28);
			U16(n, 0x17C) = 0x1000;
			U32(n, 0x20) = U32(b, 0x2C);
			pb_895E40(n, n + 0x1C);
			const uint32_t burst = pb_893900(n, ORIG_Burst, 0x17F1744, 0xD8, 2, 0);
			const uint32_t w1c = U32(n, 0x1C);
			const uint32_t w20 = U32(n, 0x20);
			U32(burst, 0x1C) = w1c;
			U32(burst, 0x20) = w20;
			x::Effect_AddTaskAndInitFromCtx(Q_DEBRIS, ORIG_Spawner, 0xB0, n);
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
			return pb_893A30(n);
		}
		for (int k = 0; k < 3; k++)
		{
			const int32_t s = S16(n, 0x16C + 2 * k);
			const int32_t e = S16(n, 0x174 + 2 * k);
			const int32_t p = mul32(e - s, (int32_t)t);
			U16(n, 0x1C + 2 * k) = (uint16_t)(p / 0x1000 + s);
		}
		return pb_893A30(n);
	}

	// 0x896050 (sub_896050): burst state 0 - decodes the prim layout, sets the director's hit flag
	// (+0x48 = 1: the emitter applies the damage), scale 1.0, the block's matrix (+0x30), position =
	// the target anchor (block +0x30), draws (0x893A30), next state
	uint32_t __cdecl pb_896050(uint32_t a1)
	{
		const uint32_t n = a1;
		x::Effect_DecodeModelPrimLayout(U32(n, 0x74), n + 0x94, U32(n, 0x78));
		U16(MEM<uint32_t>(DIR_PTR), 0x48) = 1;
		U32(n, 0x58) = 0x1000;
		U32(n, 0x54) = 0x1000;
		U32(n, 0x50) = 0x1000;
		const uint32_t b = MEM<uint32_t>(GEO_PTR);
		const uint32_t w34 = U32(b, 0x34);
		memcpy((void *)(n + 0x30), (const void *)b, 0x20); // rep movsd
		U32(n, 0x1C) = U32(b, 0x30);
		U32(n, 0x20) = w34;
		pb_893A30(n);
		U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		return 0; // void
	}

	// 0x8960F0 (sub_8960F0): orb state 2 - flattens at the end point: y scale +0x58 0x1000 -> 0x800 ->
	// 0x400 -> finished (+0x26 |= 1, next state); position = the end point (0x895E40, with the new
	// scale), draws (0x893A30)
	uint32_t __cdecl pb_8960F0(uint32_t a1)
	{
		const uint32_t n = a1;
		const uint32_t sy = U32(n, 0x58);
		if (sy == 0x1000)
			U32(n, 0x58) = 0x800;
		else if (sy == 0x800)
			U32(n, 0x58) = 0x400;
		else if (sy == 0x400)
		{
			U8(n, 0x26) |= 1;
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		}
		pb_895E40(n, n + 0x1C);
		return pb_893A30(n);
	}

	// 0x8961B0 (sub_8961B0): ring state 0 - from tick 0x23: decodes the prim layout, scale 1.0, the
	// block's matrix (+0x30), position = the orb point (block +0x28), draws (0x893A30), next state
	uint32_t __cdecl pb_8961B0(uint32_t a1)
	{
		const uint32_t n = a1;
		if (S16(n, 0x24) < 0x23)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(n, 0x74), n + 0x94, U32(n, 0x78));
		U32(n, 0x58) = 0x1000;
		U32(n, 0x54) = 0x1000;
		U32(n, 0x50) = 0x1000;
		const uint32_t b = MEM<uint32_t>(GEO_PTR);
		const uint32_t w2c = U32(b, 0x2C);
		memcpy((void *)(n + 0x30), (const void *)b, 0x20); // rep movsd
		U32(n, 0x1C) = U32(b, 0x28);
		U32(n, 0x20) = w2c;
		pb_893A30(n);
		U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		return 0; // void
	}

	// 0x8962B0 (sub_8962B0): actor system state 0 - once the frame counter +0x24 reaches +0x298: the
	// system's position +0x290 = block +0x20 (8 bytes), a_73F990 and one step of the actor system
	// (0x896DE0), next state
	uint32_t __cdecl pb_8962B0(uint32_t a1)
	{
		const uint32_t n = a1;
		if (S16(n, 0x24) < S16(n, 0x298))
			return 0; // void
		const uint32_t b = MEM<uint32_t>(GEO_PTR);
		U32(n, 0x290) = U32(b, 0x20);
		U32(n, 0x294) = U32(b, 0x24);
		a_73F990(n);
		pb_896DE0(n);
		U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		return 0; // void
	}

	// 0x899BD0 (sub_899BD0): debris spawner state 0 - 8 debris tasks (0x899CC0, 0xB0 bytes, debris queue)
	// on its first tick, 4 on its second (then finished, next state); each carries 4 sprites at the
	// target anchor (block +0x30, 8 bytes each at +0x6C) with the velocity (+0x8C) (0, 0x400 +
	// (rand & 0xFFF), 0) turned by the block's matrix rotated by rand & 0xFFF (0x8DD960)
	uint32_t __cdecl pb_899BD0(uint32_t a1)
	{
		const uint32_t n = a1;
		int32_t count = 0;
		const uint16_t tick = U16(n, 0x24);
		if (tick == 0)
			count = 8;
		else if (tick == 1)
		{
			U8(n, 0x26) |= 1;
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
			count = 4;
		}
		if (count <= 0)
			return 0; // void
		// [esp+0x14]: the 0x20-byte matrix
		alignas(4) uint8_t mbuf[0x20] = {};
		const uint32_t M = P(mbuf);
		do
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_DEBRIS, ORIG_Debris, 0xB0, n);
			uint32_t v = t + 0x8C;
			for (int k = 4; k != 0; k--, v += 8)
			{
				const uint32_t b = MEM<uint32_t>(GEO_PTR);
				U32(v, -0x20) = U32(b, 0x30);
				U32(v, -0x1C) = U32(b, 0x34);
				U16(v, 0) = 0;
				const uint32_t r = x::CrtRand();
				U16(v, 2) = (uint16_t)((r & 0xFFF) + 0x400);
				U16(v, 4) = 0;
				memcpy((void *)M, (const void *)MEM<uint32_t>(GEO_PTR), 0x20); // rep movsd
				const uint32_t r2 = x::CrtRand() & 0xFFF;
				x::sub_8DD960(M, (uint32_t)(int32_t)(int16_t)r2);
				x::matrixMultiplyVector(M, v, v);
			}
		} while (--count != 0);
		return 0; // void
	}

	// 0x899CC0 (sub_899CC0): TASK debris - 3 states {0x89A4A0 set-up, 0x89A4C0 flipbook, ret}, then for
	// each of its 4 sprites: velocity -= velocity / 8 (per axis), position += velocity / 16; each sprite
	// drawn (0x899DD0) at its position (copied to +0x1C..+0x23)
	uint32_t __cdecl pb_899CC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x89A4A0;
		states[1] = 0x89A4C0;
		states[2] = 0x89A510; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		for (int k = 0; k < 4; k++)
		{
			const uint32_t v = node + 0x8C + 8 * k;
			const uint32_t p = node + 0x6C + 8 * k;
			for (int c = 0; c < 3; c++)
			{
				const int16_t s = S16(v, 2 * c);
				U16(v, 2 * c) = (uint16_t)(s - s / 8);
			}
			for (int c = 0; c < 3; c++)
				U16(p, 2 * c) = (uint16_t)(U16(p, 2 * c) + (uint16_t)(S16(v, 2 * c) / 16));
		}
		// 30 fps layer: see mag023_psycho_blast_held.inc
		FX_HELD(held_note_debris(node);)
		for (int k = 0; k < 4; k++)
		{
			const uint32_t p = node + 0x6C + 8 * k;
			U32(node, 0x1C) = U32(p, 0);
			U32(node, 0x20) = U32(p, 4);
			pb_899DD0(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x893300, (void *)a_73A0D0, "023 MAG_023_sub_893300" },
		{ 0x893310, (void *)a_73A0D0, "023 MAG_023_sub_893310" },
		{ 0x8933A0, (void *)a_73A170, "023 MAG_023_sub_8933A0" },
		{ 0x8933D0, (void *)a_73A1A0, "023 MAG_023_sub_8933D0" },
		{ 0x893600, (void *)a_73B640, "023 sub_893600" },
		{ 0x893780, (void *)a_73B7E0, "023 sub_893780" },
		{ 0x8937D0, (void *)a_73B8A0, "023 sub_8937D0" },
		{ 0x8937F0, (void *)a_7475A0, "023 sub_8937F0" },
		{ 0x893810, (void *)a_73A6D0, "023 nullsub_1734" },
		{ 0x893820, (void *)a_73A6D0, "023 nullsub_1735" },
		{ 0x8938B0, (void *)a_73C100, "023 sub_8938B0" },
		{ 0x894DC0, (void *)a_73D770, "023 sub_894DC0" },
		{ 0x895010, (void *)a_73D9C0, "023 sub_895010" },
		{ 0x895FF0, (void *)a_73A380, "023 sub_895FF0" },
		{ 0x8960E0, (void *)a_73A6D0, "023 nullsub_1736" },
		{ 0x896140, (void *)a_73A6D0, "023 nullsub_1737" },
		{ 0x896150, (void *)a_73A380, "023 sub_896150" },
		{ 0x896240, (void *)a_73A6D0, "023 nullsub_1738" },
		{ 0x896250, (void *)a_73A380, "023 sub_896250" },
		{ 0x896300, (void *)a_73F990, "023 sub_896300" },
		{ 0x896590, (void *)a_73FC20, "023 sub_896590" },
		{ 0x896800, (void *)a_73FE90, "023 sub_896800" },
		{ 0x896920, (void *)a_73FFB0, "023 sub_896920" },
		{ 0x896B80, (void *)a_740210, "023 sub_896B80" },
		{ 0x897240, (void *)a_740910, "023 sub_897240" },
		{ 0x8978A0, (void *)a_740F70, "023 sub_8978A0" },
		{ 0x8978F0, (void *)a_740FC0, "023 sub_8978F0" },
		{ 0x897980, (void *)a_741050, "023 sub_897980" },
		{ 0x897A50, (void *)a_741120, "023 sub_897A50" },
		{ 0x898490, (void *)a_741B60, "023 sub_898490" },
		{ 0x898BC0, (void *)a_742290, "023 au_re__rand_62" },
		{ 0x898BF0, (void *)a_7422C0, "023 sub_898BF0" },
		{ 0x898C30, (void *)a_742300, "023 sub_898C30" },
		{ 0x898C80, (void *)a_742350, "023 sub_898C80" },
		{ 0x898CD0, (void *)a_7423A0, "023 au_re__rand_62_0" },
		{ 0x898DE0, (void *)a_7424B0, "023 sub_898DE0" },
		{ 0x898DF0, (void *)a_7424C0, "023 sub_898DF0" },
		{ 0x8995D0, (void *)a_742CA0, "023 sub_8995D0" },
		{ 0x899600, (void *)a_742CD0, "023 sub_899600" },
		{ 0x899630, (void *)a_742D00, "023 sub_899630" },
		{ 0x8997E0, (void *)a_742EB0, "023 sub_8997E0" },
		{ 0x899920, (void *)a_742FF0, "023 sub_899920" },
		{ 0x899B70, (void *)a_73A6D0, "023 nullsub_1739" },
		{ 0x899EF0, (void *)a_7435E0, "023 InitEffectSequenceFromData_c14" },
		{ 0x89A030, (void *)a_743720, "023 sub_89A030" },
		{ 0x89A4E0, (void *)a_743C20, "023 sub_89A4E0" },
		{ 0x89A510, (void *)a_73A6D0, "023 nullsub_1740" },
		{ 0x89A520, (void *)a_73A6D0, "023 nullsub_1741" },
		{ 0x89A570, (void *)a_7474B0, "023 MAG_023_sub_89A570" },
		{ 0x89A590, (void *)a_7474D0, "023 MAG_023_sub_89A590" },
		{ 0x89A5B0, (void *)a_73A6D0, "023 nullsub_1742" },
		{ 0x89A5C0, (void *)a_747500, "023 MAG_023_sub_89A5C0" },
		{ 0x89A600, (void *)a_73A0D0, "023 MAG_023_sub_89A600" },
		{ 0x89A610, (void *)a_747550, "023 MAG_023_sub_89A610" },
		{ 0x89A620, (void *)a_747560, "023 MAG_023_sub_89A620" },
		{ 0x89A650, (void *)a_747590, "023 MAG_023_sub_89A650" },
		{ 0x89A660, (void *)a_7475A0, "023 MAG_023_sub_89A660" },
		{ 0x89A680, (void *)a_73A6D0, "023 nullsub_1733" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x893190, (void *)pb_893190, "023 pb_893190" },
		{ 0x893320, (void *)pb_893320, "023 pb_893320" },
		{ 0x893360, (void *)pb_893360, "023 pb_893360" },
		{ 0x893450, (void *)pb_893450, "023 pb_893450" },
		{ 0x893480, (void *)pb_893480, "023 pb_893480" },
		{ 0x893500, (void *)pb_893500, "023 pb_893500" },
		{ 0x893580, (void *)pb_893580, "023 pb_893580" },
		{ 0x8935D0, (void *)pb_8935D0, "023 pb_8935D0" },
		{ 0x893630, (void *)pb_893630, "023 pb_893630" },
		{ 0x893760, (void *)pb_893760, "023 pb_893760" },
		{ 0x8937A0, (void *)pb_8937A0, "023 pb_8937A0" },
		{ 0x893830, (void *)pb_893830, "023 pb_893830" },
		{ 0x893900, (void *)pb_893900, "023 pb_893900" },
		{ 0x893950, (void *)pb_893950, "023 pb_893950" },
		{ 0x8939B0, (void *)pb_8939B0, "023 pb_8939B0" },
		{ 0x893A30, (void *)pb_893A30, "023 pb_893A30" },
		{ 0x893B20, (void *)pb_893B20, "023 pb_893B20" },
		{ 0x893DE0, (void *)pb_893DE0, "023 pb_893DE0" },
		{ 0x893F10, (void *)pb_893F10, "023 pb_893F10" },
		{ 0x894130, (void *)pb_894130, "023 pb_894130" },
		{ 0x8943C0, (void *)pb_8943C0, "023 pb_8943C0" },
		{ 0x894840, (void *)pb_894840, "023 pb_894840" },
		{ 0x8952F0, (void *)pb_8952F0, "023 pb_8952F0" },
		{ 0x8957F0, (void *)pb_8957F0, "023 pb_8957F0" },
		{ 0x895E40, (void *)pb_895E40, "023 pb_895E40" },
		{ 0x895ED0, (void *)pb_895ED0, "023 pb_895ED0" },
		{ 0x896050, (void *)pb_896050, "023 pb_896050" },
		{ 0x8960C0, (void *)pb_8960C0, "023 pb_8960C0" },
		{ 0x8960F0, (void *)pb_8960F0, "023 pb_8960F0" },
		{ 0x8961B0, (void *)pb_8961B0, "023 pb_8961B0" },
		{ 0x896220, (void *)pb_896220, "023 pb_896220" },
		{ 0x8962B0, (void *)pb_8962B0, "023 pb_8962B0" },
		{ 0x8964B0, (void *)pb_8964B0, "023 pb_8964B0" },
		{ 0x896730, (void *)pb_896730, "023 pb_896730" },
		{ 0x896DE0, (void *)pb_896DE0, "023 pb_896DE0" },
		{ 0x896E70, (void *)pb_896E70, "023 pb_896E70" },
		{ 0x896ED0, (void *)pb_896ED0, "023 pb_896ED0" },
		{ 0x897030, (void *)pb_897030, "023 pb_897030" },
		{ 0x8970F0, (void *)pb_8970F0, "023 pb_8970F0" },
		{ 0x897170, (void *)pb_897170, "023 pb_897170" },
		{ 0x8971B0, (void *)pb_8971B0, "023 pb_8971B0" },
		{ 0x8981E0, (void *)pb_8981E0, "023 pb_8981E0" },
		{ 0x8982F0, (void *)pb_8982F0, "023 pb_8982F0" },
		{ 0x898370, (void *)pb_898370, "023 pb_898370" },
		{ 0x8983A0, (void *)pb_8983A0, "023 pb_8983A0" },
		{ 0x898440, (void *)pb_898440, "023 pb_898440" },
		{ 0x8984C0, (void *)pb_8984C0, "023 pb_8984C0" },
		{ 0x898540, (void *)pb_898540, "023 pb_898540" },
		{ 0x898D20, (void *)pb_898D20, "023 pb_898D20" },
		{ 0x898F40, (void *)pb_898F40, "023 pb_898F40" },
		{ 0x899A30, (void *)pb_899A30, "023 pb_899A30" },
		{ 0x899AC0, (void *)pb_899AC0, "023 pb_899AC0" },
		{ 0x899B50, (void *)pb_899B50, "023 pb_899B50" },
		{ 0x899B80, (void *)pb_899B80, "023 pb_899B80" },
		{ 0x899BD0, (void *)pb_899BD0, "023 pb_899BD0" },
		{ 0x899CC0, (void *)pb_899CC0, "023 pb_899CC0" },
		{ 0x899DD0, (void *)pb_899DD0, "023 pb_899DD0" },
		{ 0x89A4A0, (void *)pb_89A4A0, "023 pb_89A4A0" },
		{ 0x89A4C0, (void *)pb_89A4C0, "023 pb_89A4C0" },
		{ 0x89A530, (void *)pb_89A530, "023 pb_89A530" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag023_psycho_blast()
	{
		act::register_module(23);
		for (const act::psycho::ModPort *p = act::psycho::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(23, p->addr, p->port, p->name);
		for (const act::psycho::ModPort *p = act::psycho::PORTS; p->addr; p++)
			act::register_module_port(23, p->addr, p->port, p->name);
		// 30 fps layer: see mag023_psycho_blast_held.inc
		FX_HELD(register_mag023_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag023_psycho_blast_held.inc"
#endif
