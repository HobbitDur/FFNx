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

// Effect 86: Angelo Cannon (kernel.bin Rinoa limit break part 2 {Combine} Angelo Cannon, section 25 entry 0, effect id +2 = 86;
// MAG_086_ANGELO_CANNON*): a copy of the actor/heal effect library (see act_engine.h) with its own parts: Rinoa loads Angelo
// into her launcher, a muzzle glow flares, Angelo is fired at the enemies (dash, glow, two prim plays), strikes them (a third
// prim play at the monsters' centre) and jumps back, raising dust rings and puffs. Module 0x77F230..0x784480 (117 functions in
// IDA; 120 with the four starts the IDB lacks: 0x77F780 / 0x77F7E0 / 0x77F980 / 0x77FA60 inside 0x77F680 / 0x77FA20, and without
// the bogus start 0x77F852 inside 0x77F7E0's switch: 55 served by engine ports, 25 the relocated text of a ported library
// function (Angelo Search 92 / Rush 91 / Reverse 94, Invincible Moon 88, Doom 8, Siren 95, Tonberry 90, The End 85), 38
// hand-ported, the loader and the setup not ported).
//
// Setup MAG_086_ANGELO_CANNON_Init 0x77F250 (runs once, not ported; loader 0x77F230) creates the root queue with the master and
// the module's task pools and starts the engine camera animation 0x1565C90.
//   Master (0x77F3E0) - camera copy 0x2793E58 (pointers 0x25BC1F4 / 0x25B4DD0), packet arenas by tick parity (module cursor
//     0x25B4CCC / 0x25B4DCC), target bone follow, 11 states (0x77F630: the director block 0x77FAE0, the module data block
//     0x77F680, the load script task 0x77F780 (a_73A380 / a_73A3E0, script 0x1565EB0) and the DIRECTOR 0x77FBF0), the eight
//     module queues.
//   Director (0x77FBF0, 14 states) - Angelo's TIM 0x170CDCC, sync points 1..4, cues 1..4, the load script step, done flag +0x48
//     at frame 0x32.
//   Phase 1 (0x77FF50) - ANGELO 0x77FF90 (node 0x160, model container 0x1565EF4), drawn by the creature renderer 0x780130 =
//     a_746C10 (state block 0x25BB240) into the module packet pool. He binds Rinoa's animation set (com data 0x156AA60) and
//     queues her chain transformations 1, 2 and 3, sets bit 0x200 of her entity word +0 (put back after the dash); dust rings
//     0x782930 (prim sequence drawn by 0x782990) and dust puffs 0x780640 (model drawn by 0x7806A0) on his foot-down frames
//     (0x780310 / 0x783150); the muzzle glow 0x783350 (at cue 2, placed in front of the camera eye) and the dash glow 0x783DC0
//     (models drawn by 0x7833D0); three prim plays (0x783690: node 0x450, callback 0x7838C0): 0x7836E0 (layout 0x1706398) and
//     0x783C90 (layout 0x1706CC0) at his point 0xF1, paused while he waits for cue 3 (a_73BB60), 0x783F80 (layout 0x1709404) at
//     the monsters' centre with a texture scroll (+0x8C).
//   Damage state 0x7842F0.. (the library's finish states): ApplyActionResultToTarget at the director's done flag.
// Writes outside the module: Rinoa's entity record (BS_BindEntityModelFromComData 0x5022C0 binds the animation set,
// QueueChainTransformation 0x505C00 chain transformations 1, 2 and 3, entity word +0 bit 0x200 set by Angelo's state 0 and put
// back by his state 5), the root-matrix slot 0x1D99AB0 (Angelo), the camera copy 0x2793E58, action results (0x506690). No
// camera word is written (the setup's engine camera animation; the muzzle glow reads the eye's z 0xB8B7F4).
// Module globals: 0x25B4CC8..0x25BC38C (+ camera copy 0x2793E58, 0x20 bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace cannon86
{
	// ---- generated (desc86.py): the module descriptor of this copy of the library ----
	static const Mod MOD_086 = { "angelocannon", 86, 0x77F230, 0x784480,
		{ 0x0, 0x0, 0x1561770, 0x0, 0x0, 0x25B4DCC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25B4CC8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25B7080, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25BB0C0, 0x0, 0x0, 0x25BB238, 0x25BBD2C, 0x0, 0x0, 0x0, 0x25BBF60, 0x0, 0x0, 0x0, 0x25BC218, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x77F5B0, 0x77F630, 0x0, 0x77F980, 0x77F9B0, 0x77F9E0, 0x77F9F0, 0x77FA00, 0x77FA10, 0x77FA20, 0x0, 0x0, 0x77FDB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x77FD20, 0x0, 0x77FD70, 0x7838C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x782BF0, 0x0, 0x7842F0, 0x784360, 0x784380, 0x7843A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x1561770;        // pointer to the director block (phase +0x40 / requested +0x42 / next +0x44, frame +0x46, done +0x48)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = 0x25B4CCC;  // module packet cursor (tick parity: [0x25BB234] / [0x25BB230])
	static const uint32_t PLAY_PAUSE = 0x25B7080;     // the plays' pause word (a_73BB60, with the switch word 0x25BC218)
	static const uint32_t CREATURE_ARG = 0x25BB240;   // the creature renderer's state block (Angelo)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t BindEntityModelFromComData(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5022C0)(a1, a2, a3); } // BS_BindEntityModelFromComData (entity, 0x20-byte block, com data)
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); } // (entity, chain transformation id)
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

	// director block and state helpers of the Tonberry / Death copies (the module's director block [0x1597358])
	static inline uint32_t t1_data() { return MEM<uint32_t>(DIR_PTR); }
	static inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// the one-argument task tail of the Boko text (status read here)
	static inline uint32_t task_end(uint32_t node) { const uint8_t st = U8(node, 0x26); U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1); return task_end(node, st); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ac_77F3E0(uint32_t a1);
	uint32_t __cdecl ac_77F570(uint32_t a1);
	uint32_t __cdecl ac_77F630(uint32_t a1);
	uint32_t __cdecl ac_77F680();
	uint32_t __cdecl ac_77FAE0(uint32_t a1);
	uint32_t __cdecl ac_77FBF0(uint32_t a1);
	uint32_t __cdecl ac_77FCB0(uint32_t a1);
	uint32_t __cdecl ac_77FD00(uint32_t a1);
	uint32_t __cdecl ac_77FD50(uint32_t a1);
	uint32_t __cdecl ac_77FDF0(uint32_t a1);
	uint32_t __cdecl ac_77FE90(uint32_t a1);
	uint32_t __cdecl ac_77FEB0(uint32_t a1);
	uint32_t __cdecl ac_77FED0(uint32_t a1);
	uint32_t __cdecl ac_77FEF0(uint32_t a1);
	uint32_t __cdecl ac_77FF10(uint32_t a1);
	uint32_t __cdecl ac_77FF50(uint32_t a1);
	uint32_t __cdecl ac_77FF90(uint32_t a1);
	uint32_t __cdecl ac_780310(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ac_780640(uint32_t a1);
	uint32_t __cdecl ac_7806A0(uint32_t a1);
	uint32_t __cdecl ac_7807B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_7808E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_780B00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_780D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_781210(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_781CC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_7821C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_782810(uint32_t a1);
	uint32_t __cdecl ac_782850(uint32_t a1);
	uint32_t __cdecl ac_782930(uint32_t a1);
	uint32_t __cdecl ac_782990(uint32_t a1);
	uint32_t __cdecl ac_783060(uint32_t a1);
	uint32_t __cdecl ac_783080(uint32_t a1);
	uint32_t __cdecl ac_783150(uint32_t a1);
	uint32_t __cdecl ac_783230(uint32_t a1);
	uint32_t __cdecl ac_7832D0(uint32_t a1);
	uint32_t __cdecl ac_783350(uint32_t a1);
	uint32_t __cdecl ac_7833D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ac_783500(uint32_t a1);
	uint32_t __cdecl ac_783550(uint32_t a1);
	uint32_t __cdecl ac_783590(uint32_t a1);
	uint32_t __cdecl ac_7835E0(uint32_t a1);
	uint32_t __cdecl ac_783640(uint32_t a1);
	uint32_t __cdecl ac_783690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl ac_783740(uint32_t a1);
	uint32_t __cdecl ac_7838C0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ac_783B70(uint32_t a1);
	uint32_t __cdecl ac_783BA0(uint32_t a1);
	uint32_t __cdecl ac_783CF0(uint32_t a1);
	uint32_t __cdecl ac_783D40(uint32_t a1);
	uint32_t __cdecl ac_783DC0(uint32_t a1);
	uint32_t __cdecl ac_783E40(uint32_t a1);
	uint32_t __cdecl ac_783E90(uint32_t a1);
	uint32_t __cdecl ac_783EC0(uint32_t a1);
	uint32_t __cdecl ac_783F00(uint32_t a1);
	uint32_t __cdecl ac_783F80(uint32_t a1);
	uint32_t __cdecl ac_783FF0(uint32_t a1);
	uint32_t __cdecl ac_784040(uint32_t a1);
	uint32_t __cdecl ac_784070(uint32_t a1);
	uint32_t __cdecl ac_784150(uint32_t a1);
	uint32_t __cdecl ac_7841E0(uint32_t a1);
	uint32_t __cdecl ac_784270(uint32_t a1);
	uint32_t __cdecl ac_7842B0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag086_angelo_cannon_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace cannon86
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Angelo Search / Rush / Reverse / Invincible Moon / Doom / Siren / Tonberry / The End port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x77F3E0 (MAG_086_sub_77F3E0): MASTER task - snapshots the camera matrix into 0x2793E58 (pointers 0x25BC1F4 / 0x25B4DD0),
	// picks the double-buffered packet arenas by tick parity (module cursor 0x25B4CCC = [0x25BB234] / [0x25BB230], second cursor
	// 0x25B4DCC = [0x25BAE2C] / [0x25BAE28]), follows the target's bones, runs the 11-state master table {0x77F550 / 0x77F560
	// (a_73A0D0), 0x77F570 (wait for the children), 0x77F580 (a_73A170: starts the emitter), the library's finish states
	// 0x7843B0.., ret}, then executes the eight module queues (live task count -> node +0x5E)
	uint32_t __cdecl ac_77F3E0(uint32_t a1)
	{
		g_mod = &MOD_086;
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25BC1F4) = 0x2793E58;
		MEM<uint32_t>(0x25B4DD0) = 0x2793E58;
		states[0] = 0x77F550;
		states[1] = 0x77F560;
		states[2] = 0x77F570;
		states[3] = 0x77F580;
		states[4] = 0x7843B0;
		states[5] = 0x7843F0;
		states[6] = 0x784400;
		states[7] = 0x784410;
		states[8] = 0x784440;
		states[9] = 0x784450;
		states[10] = 0x784470; // nullsub (ret)
		if ((U8(node, 0x5C) & 1) != 0)
		{
			MEM<uint32_t>(0x25B4CCC) = MEM<uint32_t>(0x25BB234);
			MEM<uint32_t>(0x25B4DCC) = MEM<uint32_t>(0x25BAE2C);
		}
		else
		{
			MEM<uint32_t>(0x25B4CCC) = MEM<uint32_t>(0x25BB230);
			MEM<uint32_t>(0x25B4DCC) = MEM<uint32_t>(0x25BAE28);
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		static const uint32_t queues[8] = { 0x25BB0C0, 0x25B4DA8, 0x25BC1E0, 0x25BBEE8, 0x25B4EB0, 0x25B7088, 0x25BAE18, 0x25BB0B0 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl ac_77F570(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl ac_77F630(uint32_t a1)
	{
		ac_77FAE0(a1);
		ac_77F680();
		x::Effect_AddTaskAndInitFromCtx(0x25BC1E0, 0x77F780, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25B4DA8, 0x77FBF0, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x784960 (MAG_085_sub_784960): the file loader block (0x25C21E0, 0x16C bytes; pointer 0x25C1C58) and the load script
	// cursor 0x25C1B60 = 0x1571E70: +0x100 = an arena of 0xA000 bytes from the file arena cursor, +0x104.. the load
	// destinations (0x19CF018, +0x114 0x19EB018, +0x12C 0x19D2560), +0x164 / +0x166 loading / script-done words cleared
	uint32_t __cdecl ac_77F680()
	{
		MEM<uint32_t>(0x25BB238) = 0x1565EB0;
		MEM<uint32_t>(0x25BBD2C) = 0x25BC220;
		x::MAG_007_sub_8DCC00(0x25BC220, 0x16C);
		const uint32_t b = MEM<uint32_t>(0x25BBD2C);
		uint32_t c = MEM<uint32_t>(0x25BC21C);
		U32(b, 0x100) = c;
		c += 0xA000;
		MEM<uint32_t>(0x25BC21C) = c;
		U32(b, 0x114) = 0x19EB018;
		for (int32_t o = 0x104; o <= 0x160; o += 4)
			if (o != 0x114 && o != 0x12C) U32(b, o) = 0x19CF018;
		U32(b, 0x12C) = 0x19D2560;
		U16(b, 0x164) = 0;
		U16(b, 0x166) = 0;
		return b;
	}

	// 0x77FAE0 (MAG_086_sub_77FAE0): director data set-up - clears the director block [DIR_PTR] (0x54 bytes), +8.. = the centre
	// of the visible monsters (0x77FB60 = a_73A720), its y +0xA = 0, +0.. = the caster's (+0x2C) position (entity +0x1C / +0x20),
	// facing +0x4A = the angle from the caster to that centre - 0x800 (12 bits)
	uint32_t __cdecl ac_77FAE0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		a_73A720(MEM<uint32_t>(DIR_PTR) + 8);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t c = ENT(U8(a1, 0x2C));
		U16(d, 0xA) = 0;
		U32(d, 0) = U32(c, 0x1C);
		U32(d, 4) = U32(c, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		const uint32_t r = (ang - 0x800) & 0xFFF;
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)r;
		return r;
	}

	// 0x77FBF0 (MAG_086_sub_77FBF0): TASK director (node 0x48) - phase bookkeeping (0x77FCB0) every tick, then its 14 states
	// {0x77FD00 TIM 0x170CDCC + sync point 1, 0x77FD50 cue 1, 0x77FD90 (a_73A930: the next load-script step), 0x77FDF0 frame 0xF,
	// 0x77FE10 (a_73B8A0: sync point 2), 0x77FE30 (a_73B8C0: cue 2), 0x77FE50 (a_733AC0: loads done), 0x77FE70 (a_733B70: sync
	// point 3), 0x77FE90 cue 3, 0x77FEB0 done flag at frame 0x32, 0x77FED0 frame 0x3C, 0x77FEF0 sync point 4, 0x77FF10 cue 4 ->
	// finished, ret}; frame +0x24; released once finished with no child alive
	uint32_t __cdecl ac_77FBF0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[14];
		states[0] = 0x77FD00;
		states[1] = 0x77FD50;
		states[2] = 0x77FD90;
		states[3] = 0x77FDF0;
		states[4] = 0x77FE10;
		states[5] = 0x77FE30;
		states[6] = 0x77FE50;
		states[7] = 0x77FE70;
		states[8] = 0x77FE90;
		states[9] = 0x77FEB0;
		states[10] = 0x77FED0;
		states[11] = 0x77FEF0;
		states[12] = 0x77FF10;
		states[13] = 0x77FF30; // nullsub (ret)
		ac_77FCB0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ac_77FCB0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ac_77FF50(a1);
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

	// 0x77FD00 (MAG_086_UploadAngeloTIM): director state 0 - queues Angelo's TIM 0x170CDCC (Battle_QueueTIMUpload_GetEOF); next
	// state at director sync point 1 (a_73B640)
	uint32_t __cdecl ac_77FD00(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x170CDCC);
		if (a_73B640(1) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x763B90 (module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl ac_77FD50(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x77FDF0 (sub_77FDF0): director state 3 - next state from the director's frame +0x46 = 0xF
	uint32_t __cdecl ac_77FDF0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0xF)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73B7E0).
	uint32_t __cdecl ac_77FE90(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x77FEB0 (sub_77FEB0): director state 9 - from the director's frame +0x46 = 0x32: done flag +0x48 = 1 (the damage of the
	// library's finish states), next state
	uint32_t __cdecl ac_77FEB0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) >= 0x32)
		{
			U16(d, 0x48) = 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x785230 (sub_785230): director state 11 - next state from frame 0x3C
	uint32_t __cdecl ac_77FED0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(0x1561770), 0x46) >= 0x3C)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x73BA50 (module; Siren sub_73BA50): director state: asks for cut 4, advances when accepted
	uint32_t __cdecl ac_77FEF0(uint32_t a1)
	{
		uint32_t r = a_73B640(4);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x77FF10 (sub_77FF10): director state 12 - at director cue 4 (a_73B7E0): finished (+0x26 |= 1), next state
	uint32_t __cdecl ac_77FF10(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77FF50 (MAG_086_SpawnAngeloCreature): the tasks of a new director phase ([DIR_PTR] +0x40): phase 1 = the ANGELO actor
	// 0x77FF90 (queue 0x25BBEE8, node 0x160) bound to the model container 0x1565EF4 animation 3
	uint32_t __cdecl ac_77FF50(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x40) == 1)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25BBEE8, 0x77FF90, 0x160, a1);
			x::Effect_BindModelContainerSetAnim(t, 0x1565EF4, 3);
		}
		return 0; // void
	}

	// 0x77FF90 (sub_77FF90): ANGELO actor task (node 0x160) - states {0x783230 place (binds Rinoa's animation set), 0x7832D0
	// (sound, waits for cue 2: shown, the muzzle glow 0x783350), 0x7835E0 (Rinoa's chain transformation 2), 0x783640 (the first
	// play 0x7836E0), 0x783BA0 (waits for cue 3 / the plays' pause: the second play 0x783C90 and the glow 0x783DC0), 0x783F00
	// (dash to the target, the third play 0x783F80), 0x784150, 0x7841E0 (jump back, chain transformation 3), 0x784270, 0x7842B0
	// (hidden + finished), ret}; then velocity: z +0x50 += +0x128, y +0x4E += +0x126; unless hidden (+0x26 bit 2) the model is
	// drawn by the creature renderer 0x780130 (a_746C10, state block 0x25BB240) into the module packet pool; then, on the
	// animation frames that put a foot down (animation +0x9C / frame +0xA2), the bursts of 0x780310 at the foot points (and
	// 0x783150 at his point 0xF1); frame +0x24; root matrix (+0x70) published to 0x1D99AB0, position to +0x84/+0x88/+0x8C (s32);
	// released once finished with no child alive
	uint32_t __cdecl ac_77FF90(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x783230;
		states[1] = 0x7832D0;
		states[2] = 0x7835E0;
		states[3] = 0x783640;
		states[4] = 0x783BA0;
		states[5] = 0x783F00;
		states[6] = 0x784150;
		states[7] = 0x7841E0;
		states[8] = 0x784270;
		states[9] = 0x7842B0;
		states[10] = 0x7842E0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint16_t vz = U16(node, 0x128);
		const uint16_t vy = U16(node, 0x126);
		const uint8_t st = U8(node, 0x26);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + vz);
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + vy);
		if ((st & 4) == 0)
		{
			// 30 fps layer: see mag086_angelo_cannon_held.inc
			FX_HELD(held_note_draw(HK_ANGELO, node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, CREATURE_ARG, MEM<uint32_t>(PACKET_CURSOR));
			if ((U8(node, 0x26) & 4) == 0)
			{
				const uint8_t anim = U8(node, 0x9C), frame = U8(node, 0xA2);
				if (anim == 0)
				{
					if (frame == 1)
						ac_780310(node, 2);
					else if (frame == 2)
					{
						ac_780310(node, 1);
						ac_780310(node, 3);
					}
					else if (frame == 4)
						ac_780310(node, 0);
				}
				else if (anim == 1)
				{
					if (frame <= 5)
					{
						ac_780310(node, 4);
						ac_780310(node, 5);
					}
				}
				else if (anim == 5)
				{
					if (frame <= 5)
						ac_783150(node);
				}
			}
		}
		const int32_t pz = S16(node, 0x50);
		const int32_t py = S16(node, 0x4E);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 8 * 4); // rep movsd
		const int32_t px = S16(node, 0x4C);
		S32(node, 0x8C) = pz;
		const uint8_t status = U8(node, 0x26);
		S32(node, 0x84) = px;
		S32(node, 0x88) = py;
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x780310 (sub_780310): a burst at foot point a2 (0..3 -> spawn points 0x1E / 0x21 / 0x1C / 0x1D of Angelo's model +0x30; 4 /
	// 5 -> 0x1C / 0x1D): a2 0..3 = four ring particles 0x782930 (queue 0x25B4EB0, node 0x90: on the ground, +-0x80 x / z, velocity
	// +0x78.. = the vector (0, 0, 0x200..0x11FF) turned by a random yaw +-0x200, rise -(0..0x5F) per tick +0x82) and two dust puffs
	// 0x780640 (queue 0x25B7088, node 0x98: velocity +0x7C.. = (0, 0, 0x200..0x9FF) turned by a random yaw +-0x100 and pitch
	// 0x100..0x27F); a2 4 / 5 = two ring particles at the point (+-0x40 x / z, height scaled by 0xC0..0xFF / 256, velocity 0 turned,
	// rise -(0..0x5F))
	uint32_t __cdecl ac_780310(uint32_t a1, uint32_t a2)
	{
		static const uint32_t PTS[6] = { 0x1E, 0x21, 0x1C, 0x1D, 0x1C, 0x1D };
		const uint32_t mdl = a1 + 0x30;
		const int16_t k = (int16_t)a2;
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		const int n = k > 3 ? 2 : 4;
		for (int i = 0; i < n; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B4EB0, 0x782930, 0x90, a1);
			if ((uint32_t)(int32_t)k <= 5)
				x::GetEffectSpawnPosition(mdl, PTS[k], 0, t + 0x1C);
			if (k <= 3)
			{
				const int32_t r1 = (int32_t)x::CrtRand();
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0xFF) - 0x80));
				const int32_t r2 = (int32_t)x::CrtRand();
				U16(t, 0x1E) = 0;
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0xFF) - 0x80));
				x::MAG_022_sub_8DD770(M);
				const int32_t r3 = (int32_t)x::CrtRand();
				x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r3 & 0x3FF) - 0x200) & 0xFFF));
				const int32_t r4 = (int32_t)x::CrtRand();
				U16(t, 0x78) = 0;
				U16(t, 0x7A) = 0;
				U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(t, 0x7C) = (uint16_t)((r5 & 0xFFF) + 0x200);
			}
			else
			{
				const int32_t r1 = (int32_t)x::CrtRand();
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0x7F) - 0x40));
				const int32_t r2 = (int32_t)x::CrtRand();
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0x7F) - 0x40));
				const int32_t r3 = (int32_t)x::CrtRand();
				const int32_t f = (int32_t)(int16_t)((r3 & 0x3F) + 0xC0);
				U16(t, 0x1E) = (uint16_t)((int32_t)S16(t, 0x1E) * f / 256);
				x::MAG_022_sub_8DD770(M);
				const int32_t r4 = (int32_t)x::CrtRand();
				x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r4 & 0x3FF) - 0x200) & 0xFFF));
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(t, 0x78) = 0;
				U16(t, 0x7A) = 0;
				U16(t, 0x7C) = 0;
				U16(t, 0x82) = (uint16_t)(-(r5 % 0x60));
			}
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		}
		if (k > 3)
			return 0; // void
		for (int i = 0; i < 2; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B7088, 0x780640, 0x98, a1);
			if ((uint32_t)(int32_t)k <= 3)
				x::GetEffectSpawnPosition(mdl, PTS[k], 0, t + 0x1C);
			U16(t, 0x1E) = 0;
			x::MAG_022_sub_8DD770(M);
			const int32_t r1 = (int32_t)x::CrtRand();
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r1 & 0x1FF) - 0x100) & 0xFFF));
			const int32_t r2 = (int32_t)x::CrtRand();
			x::sub_8DD7E0(M, (uint32_t)(int32_t)(int16_t)(((r2 % 0x180) + 0x100) & 0xFFF));
			U16(t, 0x7E) = 0;
			U16(t, 0x7C) = 0;
			const int32_t r3 = (int32_t)x::CrtRand();
			U16(t, 0x80) = (uint16_t)((r3 % 0x800) + 0x200);
			x::matrixMultiplyVector(M, t + 0x7C, t + 0x7C);
		}
		return 0; // void
	}

	// 0x7211D0 (module 099 sub_7211D0): TASK - state machine {721320, 721360, nop}, then
	// ac_7806A0 (model draw), task tail.
	uint32_t __cdecl ac_780640(uint32_t a1)
	{
		const uint32_t states[3] = { 0x782810, 0x782850, 0x782920 };
		callp(states[S8(a1, 0x29)], a1);
		ac_7806A0(a1);
		return task_end(a1);
	}

	// 0x788700 (sub_788700): a model of the module (definition +0x4C) drawn unless hidden (+0x26 bit 2): the camera rotation
	// (UnpackRotationMatrix 0x1D97778) turned by +0x48 (z), at +0x1C.., scaled by +0x30.., through the camera; header 0x68
	// (+8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0, both windows 256 x 256) through the module renderer
	// 0x785A90 into the frame arena 0x1D8E054
	uint32_t __cdecl ac_7806A0(uint32_t a1)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_MODEL, a1);)
		uint8_t m[0x20];
		const uint32_t M = P(m);
		x::UnpackRotationMatrix(0x1D97778, M);
		x::sub_8DD960(M, (uint32_t)(int32_t)S16(a1, 0x48));
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
		S32(h, 0xC) = S16(a1, 0x50);
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		S32(h, 0x10) = S16(a1, 0x52);
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U32(h, 0x14) = 0xF0;
		MEM<uint32_t>(0x1D8E054) = ac_7807B0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 ac_780D90, FT4 ac_781210,
	// a_73D770, a_73D9C0, 0x73DCA0, ac_7821C0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl ac_7807B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		if (!(U32(ctx, 0x14) & 0x2000))
			U32(ctx, 4) = U32(ctx, 0) + 8;
		const uint32_t model = U32(ctx, 0);
		const uint32_t fb = U8(ctx, 0xA);
		const uint32_t lists = U32(model, 0) + model;
		const uint32_t fg = U8(ctx, 9);
		U32(ctx, 0x2C) = lists;  // +0x2C cursor in the primitive lists
		const uint32_t fr = U8(ctx, 8);
		x::someCameraWork_45DD60(fr, fg, fb);

		uint32_t cur = a4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ac_7808E0(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ac_780B00(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ac_780D90(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ac_781210(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D770(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D9C0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ac_781CC0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return ac_7821C0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl ac_7808E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ac_780B00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x73CD70 (module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl ac_780D90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x73D1F0 (module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl ac_781210(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ac_781CC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x73E1A0 (module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl ac_7821C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x761090 (sub_761090): dust puff state 0 - model 0x154322C, scale 0x1000^3, random roll +0x48, next state
	uint32_t __cdecl ac_782810(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x15618C0;
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U16(a1, 0x48) = (uint16_t)(x::CrtRand() & 0xFFF);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x782850 (sub_782850): dust puff state 1 - roll +0x48 += 0x40, y kept on / above the ground; from frame 0xA the scale shrinks
	// by 0x400 a tick (at 0: finished, scale 0x10, next state); velocity +0x7C.. damped by 1/16 (y pulled up by 0x80), position
	// += velocity / 16
	uint32_t __cdecl ac_782850(uint32_t a1)
	{
		const uint32_t c = a1;
		U16(c, 0x48) = (uint16_t)(U16(c, 0x48) + 0x40);
		if (S16(c, 0x1E) >= 0)
			U16(c, 0x1E) = 0;
		if (S16(c, 0x24) >= 0xA)
		{
			const int32_t s = S32(c, 0x38) - 0x400;
			S32(c, 0x38) = s;
			if (s <= 0)
			{
				U8(c, 0x26) = (uint8_t)(U8(c, 0x26) | 1);
				U8(c, 0x29) = (uint8_t)(U8(c, 0x29) + 1);
				S32(c, 0x38) = 0x10;
			}
			S32(c, 0x34) = S32(c, 0x38);
			S32(c, 0x30) = S32(c, 0x38);
		}
		int16_t vx = S16(c, 0x7C);
		int16_t vy = (int16_t)(S16(c, 0x7E) + 0x80);
		int16_t vz = S16(c, 0x80);
		vx = (int16_t)(vx - vx / 16);
		U16(c, 0x7C) = (uint16_t)vx;
		vy = (int16_t)(vy - vy / 16);
		U16(c, 0x7E) = (uint16_t)vy;
		vz = (int16_t)(vz - vz / 16);
		U16(c, 0x80) = (uint16_t)vz;
		U16(c, 0x1C) = (uint16_t)(U16(c, 0x1C) + (uint16_t)(vx / 16));
		U16(c, 0x1E) = (uint16_t)(U16(c, 0x1E) + (uint16_t)(vy / 16));
		U16(c, 0x20) = (uint16_t)(U16(c, 0x20) + (uint16_t)(vz / 16));
		return 0; // void
	}

	// 0x767290 (module 090 sub_767290): ring particle task (states 0x767410/0x767430) drawn by
	// ac_782990; ends when finished and no child alive.
	uint32_t __cdecl ac_782930(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x783060;
		states[1] = 0x783080;
		states[2] = 0x783140;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		ac_782990(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl ac_782990(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_RING, node);)
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
		const uint32_t cursor = MEM<uint32_t>(0x25B4CCC);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x25B4CCC) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x7618E0 (sub_7618E0): ring particle state 0 - prim sequence 0x15430E8, colour +0x56 = 0xFFC0, +0x52 = 7, next state
	uint32_t __cdecl ac_783060(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x156177C;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x761900 (sub_761900): ring particle state 1 - sequence step (0x761990 = a_743C20; at its end hidden + finished, next
	// state); y velocity +0x7A += +0x82, x / z velocity damped by 1/8, position += velocity / 16
	uint32_t __cdecl ac_783080(uint32_t a1)
	{
		const uint32_t n = a1;
		if (a_743C20(n) != 0)
		{
			U8(n, 0x26) = (uint8_t)(U8(n, 0x26) | 5);
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		}
		U16(n, 0x7A) = (uint16_t)(U16(n, 0x7A) + U16(n, 0x82));
		int16_t vx = S16(n, 0x78);
		int16_t vz = S16(n, 0x7C);
		const int16_t vy = S16(n, 0x7A);
		vx = (int16_t)(vx - vx / 8);
		U16(n, 0x78) = (uint16_t)vx;
		vz = (int16_t)(vz - vz / 8);
		U16(n, 0x7C) = (uint16_t)vz;
		U16(n, 0x1C) = (uint16_t)(U16(n, 0x1C) + (uint16_t)(vx / 16));
		U16(n, 0x1E) = (uint16_t)(U16(n, 0x1E) + (uint16_t)(vy / 16));
		U16(n, 0x20) = (uint16_t)(U16(n, 0x20) + (uint16_t)(vz / 16));
		return 0; // void
	}

	// 0x783150 (sub_783150): two ring particles 0x782930 (queue 0x25B4EB0, node 0x90) at Angelo's point 0xF1 on the ground, +-0x80
	// x / z, velocity +0x78.. = the vector (0, 0, 0x200..0x11FF) turned by a random yaw, rise -(0..0x5F) per tick +0x82
	uint32_t __cdecl ac_783150(uint32_t a1)
	{
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		for (int i = 0; i < 2; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B4EB0, 0x782930, 0x90, a1);
			x::GetEffectSpawnPosition(a1 + 0x30, 0xF1, 0, t + 0x1C);
			U16(t, 0x1E) = 0;
			const int32_t r1 = (int32_t)x::CrtRand();
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0xFF) - 0x80));
			const int32_t r2 = (int32_t)x::CrtRand();
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0xFF) - 0x80));
			x::MAG_022_sub_8DD770(M);
			const int32_t r3 = (int32_t)x::CrtRand();
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(r3 & 0xFFF));
			const int32_t r4 = (int32_t)x::CrtRand();
			U16(t, 0x78) = 0;
			U16(t, 0x7A) = 0;
			U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
			const int32_t r5 = (int32_t)x::CrtRand();
			U16(t, 0x7C) = (uint16_t)((r5 & 0xFFF) + 0x200);
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		}
		return 0; // void
	}

	// 0x783230 (sub_783230): Angelo state 0 - binds Rinoa's animation set (com data 0x156AA60, 0x20-byte block 0x25BC1F8) to the
	// caster (+0x2C, BS_BindEntityModelFromComData 0x5022C0) and queues her chain transformation 1 (QueueChainTransformation
	// 0x505C00); hidden; at the caster's position (director +0..) z + 0x1200 on the ground, scale 0x1000^3, animation 0; Rinoa's
	// entity word +0 gets bit 0x200 (its old value kept in +0x150), counter +0x134 = 0, next state
	uint32_t __cdecl ac_783230(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2C));
		gx::BindEntityModelFromComData(e, 0x25BC1F8, 0x156AA60);
		gx::QueueChainTransformation(e, 1);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 4);
		U32(node, 0x4C) = U32(d, 0);
		U32(node, 0x50) = U32(d, 4);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 0x1200);
		U16(node, 0x4E) = 0;
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		const uint16_t w = U16(e, 0);
		U16(e, 0) = (uint16_t)(w | 0x200);
		U32(node, 0x150) = w;
		U16(node, 0x134) = 0;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7832D0 (sub_7832D0): Angelo state 1 - counter +0x134; at 4 the sound 0x1561774; at director cue 2 (a_73B7E0): the muzzle
	// glow 0x783350 (queue 0x25B7088, node 0x98, +0x8C = Angelo's model), shown, velocity z +0x128 = -0x100, next state
	uint32_t __cdecl ac_7832D0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x134) = (uint16_t)(U16(node, 0x134) + 1);
		if (U16(node, 0x134) == 4)
			x::BdPlaySE(0x1561774, 1, 0x80);
		if (a_73B7E0(2) != 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B7088, 0x783350, 0x98, node);
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) & 0xFB);
			U32(t, 0x8C) = node + 0x30;
			U16(node, 0x128) = 0xFF00;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x783350 (sub_783350): muzzle glow task (node 0x98) - states {0x783500 set-up, 0x783590 fade, ret}; then its texture scroll
	// +0x92 -= 0x10 (7 bits) and the draw 0x7833D0 (node, block +0x54, scroll u +0x90, v +0x92); frame +0x24; released once
	// finished with no child alive
	uint32_t __cdecl ac_783350(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x783500;
		states[1] = 0x783590;
		states[2] = 0x7835D0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint32_t v = (uint32_t)(uint8_t)(U8(node, 0x92) - 0x10) & 0x7F;
		const uint32_t u = U16(node, 0x90);
		U16(node, 0x92) = (uint16_t)v;
		ac_7833D0(node, node + 0x54, u, v);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7833D0 (sub_7833D0): a glow model (definition +0x4C) drawn unless hidden (+0x26 bit 2): rotation y +0x46, x +0x44, z +0x48,
	// at +0x1C.., scaled by +0x30.., through the camera; header 0x68 (+8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, texture
	// scroll a3 / a4, the four words of block a2, flags 0xF0, both windows 256 x 256) through the module renderer 0x7807B0 into the
	// frame arena 0x1D8E054
	uint32_t __cdecl ac_7833D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_GLOW, a1);)
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(a1, 0x46));
		x::sub_8DD7E0(M, (uint32_t)(int32_t)S16(a1, 0x44));
		x::sub_8DD960(M, (uint32_t)(int32_t)S16(a1, 0x48));
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
		S32(h, 0xC) = S16(a1, 0x50);
		U16(h, 0x1A) = (uint16_t)a4;
		U16(h, 0x18) = (uint16_t)a3;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U32(h, 0x14) = 0xF0;
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U32(h, 0x24) = U32(a2, 0);
		U32(h, 0x28) = U32(a2, 4);
		S32(h, 0x10) = S16(a1, 0x52);
		MEM<uint32_t>(0x1D8E054) = ac_7807B0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x783500 (sub_783500): muzzle glow state 0 - placed (0x783550), block +0x54.. = (0, 0, 0x80, 0x80), model 0x1561920, scale
	// 0x1000^3, fade +0x50 = 0x1000, next state
	uint32_t __cdecl ac_783500(uint32_t a1)
	{
		const uint32_t node = a1;
		ac_783550(node);
		U16(node, 0x56) = 0;
		U16(node, 0x54) = 0;
		U32(node, 0x4C) = 0x1561920;
		U16(node, 0x58) = 0x80;
		U16(node, 0x5A) = 0x80;
		S32(node, 0x38) = 0x1000;
		S32(node, 0x34) = 0x1000;
		S32(node, 0x30) = 0x1000;
		U16(node, 0x50) = 0x1000;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x783550 (sub_783550): the muzzle glow placed at Angelo's point 0xF1 (model +0x8C, GetEffectSpawnPosition), y = -0x100, z =
	// the camera eye's z (0xB8B7F4) + 0x600
	uint32_t __cdecl ac_783550(uint32_t a1)
	{
		const uint32_t node = a1;
		x::GetEffectSpawnPosition(U32(node, 0x8C), 0xF1, 0, node + 0x1C);
		const uint32_t z = MEM<uint32_t>(0xB8B7F4) + 0x600;
		U16(node, 0x1E) = 0xFF00;
		U16(node, 0x20) = (uint16_t)z;
		return z;
	}

	// 0x783590 (sub_783590): muzzle glow state 1 - fade +0x50 -= 0x100 down to 0x800; placed (0x783550); from frame 0x10
	// finished, next state
	uint32_t __cdecl ac_783590(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 0xFF00);
		if (S16(node, 0x50) < 0x800)
			U16(node, 0x50) = 0x800;
		ac_783550(node);
		if (S16(node, 0x24) >= 0x10)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7835E0 (sub_7835E0): Angelo state 2 - animation step (0x8DD1C0); from animation frame +0x136 = 0x12: animation 1, Rinoa's
	// chain transformation 2, velocity z 0, next state
	uint32_t __cdecl ac_7835E0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2C));
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x12)
		{
			x::au_re_Battle_ReadAnimation_7(node, 1);
			gx::QueueChainTransformation(e, 2);
			U16(node, 0x128) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x783640 (sub_783640): Angelo state 3 - animation step (0x8DD220); at animation frame +0x136 = 0xC the first play 0x7836E0
	// (layout 0x1706398 / 0x38, 0x783690) at his point 0xF1, next state
	uint32_t __cdecl ac_783640(uint32_t a1)
	{
		const uint32_t node = a1;
		x::au_re_Battle_ReadAnimation_8(node);
		if (U16(node, 0x136) == 0xC)
		{
			const uint32_t t = ac_783690(node, 0x7836E0, 0x1706398, 0x38, 0, 0);
			x::GetEffectSpawnPosition(node + 0x30, 0xF1, 0, t + 0x1C);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x783690 (sub_783690): spawns the play task a2 (queue 0x25BAE18, node 0x450) from context a1: +0x74 = a3 (prim layout),
	// +0x78 = (s16) a4 (its count), +0x80 = a5 (the callback's mode word), +0x82 = a6; returns the node
	uint32_t __cdecl ac_783690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25BAE18, a2, 0x450, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x783740 (sub_783740): first play state 0 (task 0x7836E0 = a_73A380) - decodes the prim layout (+0x74, count +0x78) into
	// +0x94, scale 0x1000^3, +0x88 = 0xFF00, plays the first frame (0x783790 = a_73C280), next state
	uint32_t __cdecl ac_783740(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		S32(node, 0x58) = 0x1000;
		S32(node, 0x54) = 0x1000;
		S32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFF00;
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7838C0 (sub_7838C0): prim-model player draw callback of the module's plays (layout a1, record a2, block a3) - as Angelo
	// Search's 0x758EA0 (vertex frame lerped by MAG_017_sub_701390 into the block's +0x48 buffer; flags 0x2030, 0x20F0 with alpha +
	// colour; depth offset block +0x58; scale words 0x100) with: record flag 0x200 = the position as is (no parent rotation, no
	// matrix product); else the parent-rotated position, and the parent's rotation applied unless record flag 0x8000; the third
	// play (block +0x56 = node +0x80 = 2) draws its model 3 with the texture scroll u = block +0x4C (node +0x8C) and the v window
	// word +0x28 = 0x80; drawn with 0x7807B0 into OT base+0x44 (shift 2) at the module packet cursor 0x25B4CCC
	uint32_t __cdecl ac_7838C0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten), +8 scale (3 x int32), +0x18 Mat4x3 object matrix (+0x2C translation)
		alignas(4) uint8_t loc[0x38] = {};
		const uint32_t L = P(loc);
		const uint32_t V = L;
		const uint32_t S = L + 8;
		const uint32_t M = L + 0x18;
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
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		if ((U8(rec, 5) & 2) != 0)
		{
			// record flag 0x200: the position as is (no parent rotation, no matrix product)
			S32(M, 0x14) = (int16_t)vx;
			S32(M, 0x18) = (int16_t)vy;
			S32(M, 0x1C) = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(V);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			if ((U32(rec, 4) & 0x8000) == 0)
				x::GTE_MatrixMultiply(blk, M);
		}
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
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0x14) = 0x2030;
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x14) = 0x20F0;
			U32(hdr, 8) = U32(rec, 0x20);   // colour
		}
		if (U16(blk, 0x56) == 2 && U16(rec, 2) == 3)
		{
			// the third play's model 3: texture scroll u = block +0x4C, v window 0x80
			U16(hdr, 0x1A) = 0;
			U16(hdr, 0x18) = U16(blk, 0x4C);
			U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
			U16(hdr, 0x1E) = 0;
			U16(hdr, 0x1C) = 0;
			U16(hdr, 0x22) = 0x100;
			U16(hdr, 0x20) = 0x100;
			U16(hdr, 0x26) = 0;
			U16(hdr, 0x24) = 0;
			U16(hdr, 0x28) = 0x80;
			U16(hdr, 0x2A) = 0x100;
		}
		else
		{
			U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
			U16(hdr, 0x18) = 0;
			U16(hdr, 0x1A) = 0;
			U16(hdr, 0x1E) = 0;
			U16(hdr, 0x1C) = 0;
			U16(hdr, 0x22) = 0x100;
			U16(hdr, 0x20) = 0x100;
			U16(hdr, 0x26) = 0;
			U16(hdr, 0x24) = 0;
			U16(hdr, 0x2A) = 0x100;
			U16(hdr, 0x28) = 0x100;
		}
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		MEM<uint32_t>(PACKET_CURSOR) = ac_7807B0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ac_783B70(uint32_t a1)
	{
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x783BA0 (sub_783BA0): Angelo state 4 - animation step (0x8DD220); when the animation ends: before director cue 3 the
	// plays pause (0x783D70 = a_73BB60(1)); at cue 3 they resume (a_73BB60(0)), the sound 0x1561778, animation 2, the dash from
	// his position (+0x140..) to the director's point +8.. (+0x148..) at +0x156 = 0x201 a tick (+0x154 = 0), the second play
	// 0x783C90 (layout 0x1706CC0 / 0x1A0, mode 1) at his point 0xF1 and the glow 0x783DC0 (queue 0x25B7088, node 0x98, +0x8C =
	// his model), next state
	uint32_t __cdecl ac_783BA0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = node + 0x30;
		if (x::au_re_Battle_ReadAnimation_8(node) != 1)
			return 0; // void
		if (a_73B7E0(3) == 0)
		{
			a_73BB60(1);
			return 0; // void
		}
		a_73BB60(0);
		x::BdPlaySE(0x1561778, 1, 0x80);
		x::au_re_Battle_ReadAnimation_7(node, 2);
		U32(node, 0x140) = U32(mdl, 0x1C);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U32(node, 0x148) = U32(d, 8);
		U32(node, 0x144) = U32(mdl, 0x20);
		U32(node, 0x14C) = U32(d, 0xC);
		U16(node, 0x154) = 0;
		U16(node, 0x156) = 0x201;
		const uint32_t t = ac_783690(node, 0x783C90, 0x1706CC0, 0x1A0, 1, 0);
		x::GetEffectSpawnPosition(mdl, 0xF1, 0, t + 0x1C);
		const uint32_t g = x::Effect_AddTaskAndInitFromCtx(0x25B7088, 0x783DC0, 0x98, node);
		U32(g, 0x8C) = mdl;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x783CF0 (sub_783CF0; Doom 0x8C87F0's code): second play state 0 (task 0x783C90 = a_73A380) - decodes the prim layout,
	// scale 0x1000^3, +0x88 = 0xFE00, plays the first frame (0x783790 = a_73C280), next state
	uint32_t __cdecl ac_783CF0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFE00;
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		next_state(node);
		return 0; // void
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ac_783D40(uint32_t a1)
	{
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x783DC0 (sub_783DC0): dash glow task (node 0x98) - states {0x783E40 set-up, 0x783EC0 follow, ret}; then its texture scroll
	// +0x92 -= 0x10 (7 bits) and the draw 0x7833D0 (node, block +0x54, scroll u +0x90, v +0x92); frame +0x24; released once
	// finished with no child alive
	uint32_t __cdecl ac_783DC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x783E40;
		states[1] = 0x783EC0;
		states[2] = 0x783EF0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint32_t v = (uint32_t)(uint8_t)(U8(node, 0x92) - 0x10) & 0x7F;
		const uint32_t u = U16(node, 0x90);
		U16(node, 0x92) = (uint16_t)v;
		ac_7833D0(node, node + 0x54, u, v);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x783E40 (sub_783E40): dash glow state 0 - placed (0x783E90), block +0x56.. = 0x80, scale 0x1000^3, model 0x15638C8, yaw
	// +0x46 = 0x800, +0x54 = 0, next state
	uint32_t __cdecl ac_783E40(uint32_t a1)
	{
		const uint32_t node = a1;
		ac_783E90(node);
		U16(node, 0x56) = 0x80;
		U16(node, 0x58) = 0x80;
		U16(node, 0x5A) = 0x80;
		S32(node, 0x38) = 0x1000;
		S32(node, 0x34) = 0x1000;
		S32(node, 0x30) = 0x1000;
		U32(node, 0x4C) = 0x15638C8;
		U16(node, 0x46) = 0x800;
		U16(node, 0x54) = 0;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x783E90 (sub_783E90): the dash glow placed at Angelo's point 0xF1 (model +0x8C), y + 0x40
	uint32_t __cdecl ac_783E90(uint32_t a1)
	{
		x::GetEffectSpawnPosition(U32(a1, 0x8C), 0xF1, 0, a1 + 0x1C);
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0x40);
		return 0; // void
	}

	// 0x783EC0 (sub_783EC0): dash glow state 1 - placed (0x783E90); from frame 0x10 finished, next state
	uint32_t __cdecl ac_783EC0(uint32_t a1)
	{
		ac_783E90(a1);
		if (S16(a1, 0x24) >= 0x10)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x783F00 (sub_783F00): Angelo state 5 - animation step (0x8DD1C0) and the dash (0x784070); at its end (+0x154 = 0x1000):
	// animation 3, the third play 0x783F80 (layout 0x1709404 / 0x3A4, mode 2), Rinoa's entity word +0 bit 0x200 put back (from
	// +0x150), next state
	uint32_t __cdecl ac_783F00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2C));
		x::sub_8DD1C0(node);
		ac_784070(node);
		if (U16(node, 0x154) == 0x1000)
		{
			x::au_re_Battle_ReadAnimation_7(node, 3);
			ac_783690(node, 0x783F80, 0x1709404, 0x3A4, 2, 0);
			const uint16_t d = (uint16_t)((U16(node, 0x150) ^ U16(e, 0)) & 0x200);
			U16(e, 0) = (uint16_t)(U16(e, 0) ^ d);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x783F80 (sub_783F80): third play task (node 0x450) - states {0x783FF0 set-up, 0x784040 play, ret}; then its texture scroll
	// +0x8C += 0x10 (7 bits); frame +0x24; released once finished with no child alive
	uint32_t __cdecl ac_783F80(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x783FF0;
		states[1] = 0x784040;
		states[2] = 0x784060; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint32_t s = (uint32_t)(uint8_t)(U8(node, 0x8C) + 0x10) & 0x7F;
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		U16(node, 0x8C) = (uint16_t)s;
		return task_end(node, status);
	}

	// 0x783FF0 (sub_783FF0; Boko 0x731520's code with the module's director): third play state 0 - decodes the prim layout of
	// +0x74 (count +0x78) into +0x94, at the director's point +8.., scale 0x1000^3, plays the first frame (0x783790 = a_73C280),
	// next state
	uint32_t __cdecl ac_783FF0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U32(node, 0x1C) = U32(d, 8);
		U32(node, 0x20) = U32(d, 0xC);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ac_784040(uint32_t a1)
	{
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x784070 (sub_784070): Angelo's dash - +0x154 += +0x156; position +0x4C.. = from (+0x140..) + (to (+0x148..) - from) * t /
	// 4096 (each axis, s16, t = +0x154; at 0x1000 the end point, t clamped), then y += +0x158
	uint32_t __cdecl ac_784070(uint32_t a1)
	{
		const uint32_t c = a1;
		U16(c, 0x154) = (uint16_t)(U16(c, 0x154) + U16(c, 0x156));
		const int16_t t = S16(c, 0x154);
		if (t >= 0x1000)
		{
			U32(c, 0x4C) = U32(c, 0x148);
			U32(c, 0x50) = U32(c, 0x14C);
			U16(c, 0x154) = 0x1000;
			U16(c, 0x4E) = (uint16_t)(U16(c, 0x4E) + U16(c, 0x158));
			return 0; // void
		}
		for (uint32_t k = 0; k < 6; k += 2)
		{
			const int16_t a = S16(c, 0x140 + k);
			U16(c, 0x4C + k) = (uint16_t)((int32_t)(S16(c, 0x148 + k) - a) * (int32_t)t / 4096 + a);
		}
		U16(c, 0x4E) = (uint16_t)(U16(c, 0x4E) + U16(c, 0x158));
		return 0; // void
	}

	// 0x784150 (sub_784150): Angelo state 6 - when the animation ends (0x8DD220): animation 4, the jump back (from and to swapped,
	// +0x154 = 0 at +0x156 = 0x112 a tick, height +0x158 = 0, its velocity +0x15A = -0x200, gravity +0x15C = 0x40), next state
	uint32_t __cdecl ac_784150(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::au_re_Battle_ReadAnimation_8(node) != 1)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(node, 4);
		const uint32_t to0 = U32(node, 0x148), from0 = U32(node, 0x140), from1 = U32(node, 0x144), to1 = U32(node, 0x14C);
		U32(node, 0x140) = to0;
		U32(node, 0x148) = from0;
		U16(node, 0x154) = 0;
		U16(node, 0x158) = 0;
		U32(node, 0x144) = to1;
		U32(node, 0x14C) = from1;
		U16(node, 0x156) = 0x112;
		U16(node, 0x15A) = 0xFE00;
		U16(node, 0x15C) = 0x40;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7841E0 (sub_7841E0): Angelo state 7 - height velocity +0x15A += +0x15C, height +0x158 += it (kept <= 0); animation step
	// (0x8DD1C0) and the jump (0x784070); at its end: animation 5, Rinoa's chain transformation 3, next state
	uint32_t __cdecl ac_7841E0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2C));
		U16(node, 0x15A) = (uint16_t)(U16(node, 0x15A) + U16(node, 0x15C));
		U16(node, 0x158) = (uint16_t)(U16(node, 0x158) + U16(node, 0x15A));
		if (S16(node, 0x158) > 0)
			U16(node, 0x158) = 0;
		x::sub_8DD1C0(node);
		ac_784070(node);
		if (U16(node, 0x154) == 0x1000)
		{
			x::au_re_Battle_ReadAnimation_7(node, 5);
			gx::QueueChainTransformation(e, 3);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x784270 (sub_784270): Angelo state 8 - when the animation ends (0x8DD220): animation 6, velocity z +0x128 = 0x100, next
	// state
	uint32_t __cdecl ac_784270(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 6);
			U16(node, 0x128) = 0x100;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7842B0 (sub_7842B0): Angelo state 9 - animation step (0x8DD1C0); from animation frame +0x136 = 0x12 hidden + finished, next
	// state
	uint32_t __cdecl ac_7842B0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x12)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 5);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// ---- generated (desc86.py / cls86.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x77F550, (void *)a_73A0D0, "086 MAG_086_sub_77F550" },
		{ 0x77F560, (void *)a_73A0D0, "086 MAG_086_sub_77F560" },
		{ 0x77F580, (void *)a_73A170, "086 MAG_086_sub_77F580" },
		{ 0x77F5B0, (void *)a_73A1A0, "086 MAG_086_sub_77F5B0" },
		{ 0x77F780, (void *)a_73A380, "086 MAG_086_LoadScriptTask" },
		{ 0x77F7E0, (void *)a_73A3E0, "086 MAG_086_LoadScriptCommand" },
		{ 0x77F980, (void *)a_73A580, "086 MAG_086_LoadCallbackTIM" },
		{ 0x77F9B0, (void *)a_73A5B0, "086 sub_77F9B0" },
		{ 0x77F9E0, (void *)a_73A5E0, "086 sub_77F9E0" },
		{ 0x77F9F0, (void *)a_73A5E0, "086 sub_77F9F0" },
		{ 0x77FA00, (void *)a_73A5E0, "086 sub_77FA00" },
		{ 0x77FA10, (void *)a_73A5E0, "086 sub_77FA10" },
		{ 0x77FA20, (void *)a_73A620, "086 sub_77FA20" },
		{ 0x77FA60, (void *)a_73A660, "086 MAG_086_LoadScriptWait" },
		{ 0x77FAD0, (void *)a_73A6D0, "086 nullsub_1055" },
		{ 0x77FB60, (void *)a_73A720, "086 sub_77FB60" },
		{ 0x77FD20, (void *)a_73B640, "086 sub_77FD20" },
		{ 0x77FD70, (void *)a_73B7E0, "086 sub_77FD70" },
		{ 0x77FD90, (void *)a_73A930, "086 sub_77FD90" },
		{ 0x77FDB0, (void *)a_73A950, "086 sub_77FDB0" },
		{ 0x77FE10, (void *)a_73B8A0, "086 sub_77FE10" },
		{ 0x77FE30, (void *)a_73B8C0, "086 sub_77FE30" },
		{ 0x77FE50, (void *)a_733AC0, "086 sub_77FE50" },
		{ 0x77FE70, (void *)a_733B70, "086 sub_77FE70" },
		{ 0x77FF30, (void *)a_73A6D0, "086 nullsub_1299" },
		{ 0x77FF40, (void *)a_73A6D0, "086 nullsub_1300" },
		{ 0x780130, (void *)a_746C10, "086 sub_780130" },
		{ 0x781790, (void *)a_73D770, "086 sub_781790" },
		{ 0x7819E0, (void *)a_73D9C0, "086 sub_7819E0" },
		{ 0x782920, (void *)a_73A6D0, "086 nullsub_1301" },
		{ 0x782AB0, (void *)a_7435E0, "086 InitEffectSequenceFromData_c8" },
		{ 0x782BF0, (void *)a_743720, "086 sub_782BF0" },
		{ 0x783110, (void *)a_743C20, "086 sub_783110" },
		{ 0x783140, (void *)a_73A6D0, "086 nullsub_1302" },
		{ 0x7835D0, (void *)a_73A6D0, "086 nullsub_1303" },
		{ 0x7836E0, (void *)a_73A380, "086 sub_7836E0" },
		{ 0x783790, (void *)a_73C280, "086 sub_783790" },
		{ 0x783B90, (void *)a_73A6D0, "086 nullsub_1304" },
		{ 0x783C90, (void *)a_73A380, "086 sub_783C90" },
		{ 0x783D60, (void *)a_73A6D0, "086 nullsub_1305" },
		{ 0x783D70, (void *)a_73BB60, "086 sub_783D70" },
		{ 0x783EF0, (void *)a_73A6D0, "086 nullsub_1306" },
		{ 0x784060, (void *)a_73A6D0, "086 nullsub_1307" },
		{ 0x7842E0, (void *)a_73A6D0, "086 nullsub_1308" },
		{ 0x7842F0, (void *)a_747440, "086 MAG_086_sub_7842F0" },
		{ 0x784360, (void *)a_7474B0, "086 MAG_086_sub_784360" },
		{ 0x784380, (void *)a_7474D0, "086 MAG_086_sub_784380" },
		{ 0x7843A0, (void *)a_73A6D0, "086 nullsub_1309" },
		{ 0x7843B0, (void *)a_747500, "086 MAG_086_sub_7843B0" },
		{ 0x7843F0, (void *)a_73A0D0, "086 MAG_086_sub_7843F0" },
		{ 0x784400, (void *)a_747550, "086 MAG_086_sub_784400" },
		{ 0x784410, (void *)a_747560, "086 MAG_086_sub_784410" },
		{ 0x784440, (void *)a_747590, "086 MAG_086_sub_784440" },
		{ 0x784450, (void *)a_7475A0, "086 MAG_086_sub_784450" },
		{ 0x784470, (void *)a_73A6D0, "086 nullsub_1298" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x77F3E0, (void *)ac_77F3E0, "086 ac_77F3E0" },
		{ 0x77F570, (void *)ac_77F570, "086 ac_77F570" },
		{ 0x77F630, (void *)ac_77F630, "086 ac_77F630" },
		{ 0x77F680, (void *)ac_77F680, "086 ac_77F680" },
		{ 0x77FAE0, (void *)ac_77FAE0, "086 ac_77FAE0" },
		{ 0x77FBF0, (void *)ac_77FBF0, "086 ac_77FBF0" },
		{ 0x77FCB0, (void *)ac_77FCB0, "086 ac_77FCB0" },
		{ 0x77FD00, (void *)ac_77FD00, "086 ac_77FD00" },
		{ 0x77FD50, (void *)ac_77FD50, "086 ac_77FD50" },
		{ 0x77FDF0, (void *)ac_77FDF0, "086 ac_77FDF0" },
		{ 0x77FE90, (void *)ac_77FE90, "086 ac_77FE90" },
		{ 0x77FEB0, (void *)ac_77FEB0, "086 ac_77FEB0" },
		{ 0x77FED0, (void *)ac_77FED0, "086 ac_77FED0" },
		{ 0x77FEF0, (void *)ac_77FEF0, "086 ac_77FEF0" },
		{ 0x77FF10, (void *)ac_77FF10, "086 ac_77FF10" },
		{ 0x77FF50, (void *)ac_77FF50, "086 ac_77FF50" },
		{ 0x77FF90, (void *)ac_77FF90, "086 ac_77FF90" },
		{ 0x780310, (void *)ac_780310, "086 ac_780310" },
		{ 0x780640, (void *)ac_780640, "086 ac_780640" },
		{ 0x7806A0, (void *)ac_7806A0, "086 ac_7806A0" },
		{ 0x7807B0, (void *)ac_7807B0, "086 ac_7807B0" },
		{ 0x7808E0, (void *)ac_7808E0, "086 ac_7808E0" },
		{ 0x780B00, (void *)ac_780B00, "086 ac_780B00" },
		{ 0x780D90, (void *)ac_780D90, "086 ac_780D90" },
		{ 0x781210, (void *)ac_781210, "086 ac_781210" },
		{ 0x781CC0, (void *)ac_781CC0, "086 ac_781CC0" },
		{ 0x7821C0, (void *)ac_7821C0, "086 ac_7821C0" },
		{ 0x782810, (void *)ac_782810, "086 ac_782810" },
		{ 0x782850, (void *)ac_782850, "086 ac_782850" },
		{ 0x782930, (void *)ac_782930, "086 ac_782930" },
		{ 0x782990, (void *)ac_782990, "086 ac_782990" },
		{ 0x783060, (void *)ac_783060, "086 ac_783060" },
		{ 0x783080, (void *)ac_783080, "086 ac_783080" },
		{ 0x783150, (void *)ac_783150, "086 ac_783150" },
		{ 0x783230, (void *)ac_783230, "086 ac_783230" },
		{ 0x7832D0, (void *)ac_7832D0, "086 ac_7832D0" },
		{ 0x783350, (void *)ac_783350, "086 ac_783350" },
		{ 0x7833D0, (void *)ac_7833D0, "086 ac_7833D0" },
		{ 0x783500, (void *)ac_783500, "086 ac_783500" },
		{ 0x783550, (void *)ac_783550, "086 ac_783550" },
		{ 0x783590, (void *)ac_783590, "086 ac_783590" },
		{ 0x7835E0, (void *)ac_7835E0, "086 ac_7835E0" },
		{ 0x783640, (void *)ac_783640, "086 ac_783640" },
		{ 0x783690, (void *)ac_783690, "086 ac_783690" },
		{ 0x783740, (void *)ac_783740, "086 ac_783740" },
		{ 0x7838C0, (void *)ac_7838C0, "086 ac_7838C0" },
		{ 0x783B70, (void *)ac_783B70, "086 ac_783B70" },
		{ 0x783BA0, (void *)ac_783BA0, "086 ac_783BA0" },
		{ 0x783CF0, (void *)ac_783CF0, "086 ac_783CF0" },
		{ 0x783D40, (void *)ac_783D40, "086 ac_783D40" },
		{ 0x783DC0, (void *)ac_783DC0, "086 ac_783DC0" },
		{ 0x783E40, (void *)ac_783E40, "086 ac_783E40" },
		{ 0x783E90, (void *)ac_783E90, "086 ac_783E90" },
		{ 0x783EC0, (void *)ac_783EC0, "086 ac_783EC0" },
		{ 0x783F00, (void *)ac_783F00, "086 ac_783F00" },
		{ 0x783F80, (void *)ac_783F80, "086 ac_783F80" },
		{ 0x783FF0, (void *)ac_783FF0, "086 ac_783FF0" },
		{ 0x784040, (void *)ac_784040, "086 ac_784040" },
		{ 0x784070, (void *)ac_784070, "086 ac_784070" },
		{ 0x784150, (void *)ac_784150, "086 ac_784150" },
		{ 0x7841E0, (void *)ac_7841E0, "086 ac_7841E0" },
		{ 0x784270, (void *)ac_784270, "086 ac_784270" },
		{ 0x7842B0, (void *)ac_7842B0, "086 ac_7842B0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag086_angelo_cannon()
	{
		act::register_module(86);
		for (const act::cannon86::ModPort *p = act::cannon86::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(86, p->addr, p->port, p->name);
		for (const act::cannon86::ModPort *p = act::cannon86::PORTS; p->addr; p++)
			act::register_module_port(86, p->addr, p->port, p->name);
		// 30 fps layer: see mag086_angelo_cannon_held.inc
		FX_HELD(register_mag086_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag086_angelo_cannon_held.inc"
#endif
