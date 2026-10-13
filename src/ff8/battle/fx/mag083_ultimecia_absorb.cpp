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

// Effect 83: Ultimecia attack (enemy attack 336 of kernel.bin, no name; Dummy c0m000, Ultimecia c0m121 /
// c0m124 / c0m126 and c0m122; no other kernel.bin entry uses effect 83; MAG_083_ABSORBED_INTO_TIME, texture
// file magic/mag082.tim): a copy of the actor/heal effect library (see act_engine.h), the build of effects
// 81 / 82 (mag081_ultimecia_unk4.cpp, mag082_ultimecia_blow_away.cpp) with Boko's master, Doom's director
// bookkeeping and actor-system step, and its own creature, target absorption and stage light tasks.
//
// Setup MAG_083_ABSORBED_INTO_TIME_Init 0x7976C0 (runs once, not ported; file loader 0x7976A0 = the texture
// file -> [0x25C9F70], TIM uploaded by the setup unless the cast context's flag +1 bit 0, camera animation
// 0x1578444; packet arenas at file + 0x8000 / + 0x10000 (0x25CF3EC / 0x25CF3E8, 0x25CED2C / 0x25CED28) by
// tick parity, the file arena cursor 0x25CFA4C = file + 0x10000) creates the root queue 0x25CA150 with the
// master and seven task pools.
//   Master (0x797840 = Boko's master 0x729BD0 with 11 states) - camera copy 0x2793E58 (pointers 0x25CFA40 /
//     0x25CA080), packet arenas by tick parity (sprite cursor 0x25C9F74 / 0x25CA07C), bone follow, states:
//     0x7979C0 / 0x7979D0 next, 0x7979E0 reserves the actor pools after the file arena, 0x797A60 starts the
//     emitter 0x797A90, 0x79ED40.. next action / finish.
//   Emitter (0x797A90) - places the director data (0x797B40: the target's spawn position 0xF1, the angle to
//     it, the effect light/position block 0x1D99AB0) and starts the director 0x797BF0, then applies the
//     damage when the director sets +0x48 (0x79ECB0, ApplyActionResultToTarget).
//   Director (0x797BF0, 7 states; cuts in the director block [0x157843C]) - uploads the creature's TIM
//     0x1738C08 (0x797CC0), cut 1 (0x797E00): the creature 0x79E690 (model container 0x15786D4: appears
//     at y -0x580 and sinks, fades in, two animations, rises, fades out), the actor system 0x797ED0
//     (sequence 0x172C818: sprite and model actors), the target absorption 0x79D9B0 and the stage light
//     0x79D860 (light words 0x1D98992 + k * 0x2C: cleared, up by 0x200 per tick to 0x800, kept until tick
//     0x50, down by 0x100 per tick to 0); plays the sound 0x1578440 at cut 1 (0x797D30), the damage cue at
//     cut frame 0x6E (0x797D80), waits for cut 2 (0x797DA0).
//   Target absorption (0x79D9B0) - from tick 0x37 hides the target's own model (entity +0 bit 2) and draws
//     it itself pulled apart polygon by polygon (0x79DA20 / 0x79DB00, polygon states 0x209FAB8): polygons
//     beyond a shrinking radius threshold spin round the target, fall and shrink; the target's shadow off
//     (entity +0 bit 5) 30 ticks in, its model shown again (bit 2 cleared) after 0x5A ticks.
// It writes the target's battle entity flags (+0 bits 2 and 5), not its position, and not the battle camera
// (the camera animation is the setup's 0x5099A0 call).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace u083
{
	static const Mod MOD_083 = { "absorbed83", 83, 0x7976A0, 0x79EE10,
		{ 0x0, 0x0, 0x157843C, 0x0, 0x0, 0x25CA07C, 0x25C9F78, 0x25CF878, 0x25CFA38, 0x25CFA44, 0x25CFA4C, 0x0, 0x25C9F70, 0x0, 0x0, 0x0, 0x25CA080, 0x0, 0x25CA850, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25CEFD8, 0x0, 0x25CF278, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25CFA28, 0x25CFA2C, 0x0, 0x25CFA3A, 0x0, 0x25CFA40, 0x0, 0x25CFA50 },
		{ 0x797A20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x79AE90, 0x79C120, 0x79C170, 0x79C1A0, 0x79C220, 0x79C910, 0x79C960, 0x79CA00, 0x79CAC0, 0x79D310, 0x79D710, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x797A90, 0x797B10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x797D00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x798110, 0x7981F0, 0x798390, 0x798460, 0x798580, 0x7987E0, 0x0, 0x79AF20, 0x79B580, 0x79B5D0, 0x79B660, 0x79B730, 0x79BEC0, 0x79BFD0, 0x79C050, 0x79C080, 0x79C8A0, 0x79C8D0, 0x79C9B0, 0x79CAD0, 0x79CC20, 0x79D2B0, 0x79D2E0, 0x79D4C0, 0x79D600, 0x0, 0x0, 0x79ECB0, 0x79ECF0, 0x79ED10, 0x79ED30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x157843C;        // pointer to the director block (cut +0x40/+0x42/+0x44, frame +0x46)
	static const uint32_t PACKET_CURSOR = 0x25C9F74;  // module sprite packet cursor (tick parity: file + 0x8000 / + 0)
	static const uint32_t SCRATCH_SP = 0x25CFA40;     // module scratch stack pointer (below the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25CFA50;    // current actor system state block
	static const uint32_t STAGE_LIGHT = 0x1D98992;    // the four stage light words (+ k * 0x2C)
	static const uint32_t Q_EMITTER = 0x25CF278;      // master queues, in run order: emitter (0x58 bytes x 4)
	static const uint32_t Q_DIRECTOR = 0x25CA058;     // director (0x48 x 3)
	static const uint32_t Q_CREATURE = 0x25CFA18;     // creature 0x79E690 (0x140 x 1)
	static const uint32_t Q_ACTORS = 0x25CEFD8;       // actor systems (0x2A4 x 1)
	static const uint32_t Q_LIGHT = 0x25CA160;        // stage light 0x79D860 (0xB0 x 0xA)
	static const uint32_t Q_ABSORB = 0x25CA858;       // target absorption 0x79D9B0 (0xC8 x 8)
	static const uint32_t Q_LAST = 0x25CF268;         // (0x40 x 0xA, nothing spawned into it)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity of a slot

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_LoadV012Dwords(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E020)(a1); } // sub_45E020: V0 / V1 / V2 = 6 dwords
		static inline uint32_t GTE_StoreSXY012_PolyFT3(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2E0)(a1); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3_2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E320)(a1); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t sub_571B70() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); } // returns 0x209FAB8
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

	// one polygon state step of 0x79DB00 (inline there, the same for triangles and quads; a quad also moves
	// its 4th vertex): scratch s (vertices +0x54.. as loaded, threshold +0x90), state st (0x20 bytes)
	static inline void absorb_step(uint32_t s, uint32_t st, bool quad)
	{
		const uint16_t phase = U16(st, 6);
		if (phase == 0)
		{
			const uint16_t x0 = U16(s, 0x54), y0 = U16(s, 0x56), z0 = U16(s, 0x58);
			U16(st, 0xA) = (uint16_t)(U16(s, 0x5C) - x0);
			U16(st, 0x10) = (uint16_t)(U16(s, 0x5E) - y0);
			U16(st, 0) = y0;
			U16(st, 0x16) = (uint16_t)(U16(s, 0x60) - z0);
			U16(st, 0xC) = (uint16_t)(U16(s, 0x64) - x0);
			U16(st, 8) = (uint16_t)(U16(st, 8) + 1);
			U16(st, 0x12) = (uint16_t)(U16(s, 0x66) - y0);
			U16(st, 0x18) = (uint16_t)(U16(s, 0x68) - z0);
			const int32_t x = S16(s, 0x54), z = S16(s, 0x58);
			U16(st, 4) = (uint16_t)(x::CartesianToGameAngle((uint32_t)x, (uint32_t)z) & 0xFFF);
			const uint32_t r2 = (uint32_t)(x * x) + (uint32_t)(z * z);
			U32(s, 0x88) = r2;
			const uint32_t r = x::Sqrt(r2);
			U16(st, 6) = (uint16_t)(U16(st, 6) + 1);
			U16(st, 2) = (uint16_t)r;
			U16(st, 0x1C) = 0x1000;
			U16(st, 8) = 0;
			return;
		}
		if (phase == 1)
		{
			if (S16(st, 2) >= S16(s, 0x90))
			{
				U16(st, 8) = 0;
				U16(st, 6) = 2;
			}
			return;
		}
		U16(st, 8) = (uint16_t)(U16(st, 8) + 1);
		U16(st, 0x1E) = (uint16_t)(U16(st, 0x1E) + 0x10);
		const int16_t cnt = S16(st, 8);
		if (S16(st, 0x1E) > 0x100)
			U16(st, 0x1E) = 0x100;
		const uint32_t ang = (uint32_t)(uint16_t)(U16(st, 4) + U16(st, 0x1E)) & 0xFFF;
		U16(st, 4) = (uint16_t)ang;
		if (cnt >= 0xF)
		{
			U16(st, 0x1C) = (uint16_t)(U16(st, 0x1C) - 0x20);
			if (S16(st, 0x1C) < 0x100)
				U16(st, 0x1C) = 0x100;
			const int32_t sc = S16(st, 0x1C);
			U16(s, 0x8E) = (uint16_t)sc;
			static const int32_t OFF[9] = { 0xA, 0x10, 0x16, 0xC, 0x12, 0x18, 0xE, 0x14, 0x1A };
			for (int k = 0; k < (quad ? 9 : 6); k++)
				S16(st, OFF[k]) = (int16_t)((S16(st, OFF[k]) * sc) / 4096);
		}
		const int32_t r = S16(st, 2);
		S32(s, 0x7C) = (int32_t)x::computeSin(ang) * r / 4096;
		const int32_t cs = (int32_t)x::computeCosine((uint32_t)(int32_t)S16(st, 4));
		U16(st, 0) = (uint16_t)(U16(st, 0) - 0x30);
		S32(s, 0x84) = cs * r / 4096;
		const int32_t y = S16(st, 0);
		S32(s, 0x80) = y;
		const uint16_t px = U16(s, 0x7C), py = (uint16_t)y, pz = U16(s, 0x84);
		U16(s, 0x54) = px;
		U16(s, 0x56) = py;
		U16(s, 0x58) = pz;
		U16(s, 0x5C) = (uint16_t)(U16(st, 0xA) + px);
		U16(s, 0x5E) = (uint16_t)(U16(st, 0x10) + py);
		U16(s, 0x60) = (uint16_t)(U16(st, 0x16) + pz);
		U16(s, 0x64) = (uint16_t)(U16(st, 0xC) + px);
		U16(s, 0x66) = (uint16_t)(U16(st, 0x12) + py);
		U16(s, 0x68) = (uint16_t)(U16(st, 0x18) + pz);
		if (quad)
		{
			U16(s, 0x6C) = (uint16_t)(U16(st, 0xE) + px);
			U16(s, 0x6E) = (uint16_t)(U16(st, 0x14) + py);
			U16(s, 0x70) = (uint16_t)(U16(st, 0x1A) + pz);
		}
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ud_797840(uint32_t a1);
	uint32_t __cdecl ud_797B10(uint32_t a1);
	uint32_t __cdecl ud_797B40(uint32_t a1);
	uint32_t __cdecl ud_797BF0(uint32_t a1);
	uint32_t __cdecl ud_797C70(uint32_t a1);
	uint32_t __cdecl ud_797CC0(uint32_t a1);
	uint32_t __cdecl ud_797D30(uint32_t a1);
	uint32_t __cdecl ud_797D80(uint32_t a1);
	uint32_t __cdecl ud_797E00(uint32_t a1);
	uint32_t __cdecl ud_797F30(uint32_t a1);
	uint32_t __cdecl ud_798110(uint32_t a1);
	uint32_t __cdecl ud_798390(void);
	uint32_t __cdecl ud_798A40(uint32_t a1);
	uint32_t __cdecl ud_798AD0(void);
	uint32_t __cdecl ud_798B30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_798CB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_798DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_799000(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_799290(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_799710(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_79A1C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_79A6C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_79AD10(void);
	uint32_t __cdecl ud_79ADD0(uint32_t a1);
	uint32_t __cdecl ud_79BEC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_79CC20(uint32_t a1);
	uint32_t __cdecl ud_79D830(uint32_t a1);
	uint32_t __cdecl ud_79D910(uint32_t a1);
	uint32_t __cdecl ud_79D950(uint32_t a1);
	uint32_t __cdecl ud_79D960(uint32_t a1);
	uint32_t __cdecl ud_79D9B0(uint32_t a1);
	uint32_t __cdecl ud_79DA20(uint32_t a1);
	uint32_t __cdecl ud_79DB00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_79E530(uint32_t a1);
	uint32_t __cdecl ud_79E590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_79E5F0(void);
	uint32_t __cdecl ud_79E610(uint32_t a1);
	uint32_t __cdecl ud_79E690(uint32_t a1);
	uint32_t __cdecl ud_79E960(uint32_t a1);
	uint32_t __cdecl ud_79E9E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_79EB00(uint32_t a1);
	uint32_t __cdecl ud_79EB70(uint32_t a1);
	uint32_t __cdecl ud_79EBE0(uint32_t a1);
	uint32_t __cdecl ud_79EC20(uint32_t a1);
	uint32_t __cdecl ud_79EC40(uint32_t a1);
	uint32_t __cdecl ud_79ECB0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag083_ultimecia_absorb_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace u083
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code
	// matches an Ultimecia attack 337 (effect 82) / Doom / Siren / Boko / Raijin Attack port up to the
	// module's addresses and callees (the port's text with the module's addresses; their
	// comments describe the source original)
	// ====================================================================================

	// 0x797840 (MAG_083_sub_797840): MASTER task (Boko's master 0x729BD0 with this module's addresses) - camera
	// copy 0x2793E58 (pointers 0x25CFA40 / 0x25CA080), packet arenas by tick parity (sprite cursor 0x25C9F74 /
	// 0x25CA07C), bone follow, 11-state table, the seven queues (live task count -> node +0x5E; the actor
	// counters 0x25CFA3A / 0x25CFA28 are cleared before the queues run)
	uint32_t __cdecl ud_797840(uint32_t a1)
	{
		g_mod = &MOD_083;
		// 30 fps layer: see mag083_ultimecia_absorb_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x25CA080) = 0x2793E58;
		states[0] = 0x7979C0;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x7979D0;
		states[2] = 0x7979E0;
		states[3] = 0x797A60;
		states[4] = 0x79ED40;
		states[5] = 0x79ED80;
		states[6] = 0x79ED90;
		states[7] = 0x79EDA0;
		states[8] = 0x79EDD0;
		states[9] = 0x79EDE0;
		states[10] = 0x79EE00; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25CF3EC);
			const uint32_t v2 = MEM<uint32_t>(0x25CED2C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x25CA07C) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25CF3E8);
			const uint32_t v2 = MEM<uint32_t>(0x25CED28);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x25CA07C) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x25CFA3A) = 0;
		MEM<uint16_t>(0x25CFA28) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_CREATURE));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_LIGHT));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ABSORB));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_LAST));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A36A0 (copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - places the reaper, spawns the director
	// 0x8B6E30 (0x48 bytes, director queue), next state
	uint32_t __cdecl ud_797B10(uint32_t a1)
	{
		ud_797B40(a1);
		x::Effect_AddTaskAndInitFromCtx(0x25CA058, 0x797BF0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x797B40 (MAG_083_sub_797B40): emitter placement - clears the director block [0x157843C] (0x54 bytes),
	// its +8 = the spawn position 0xF1 of the target (+0x2D), +0 / +2 / +4 / +0xA = 0, +0x4A = the angle of
	// (x - 0, z) - 0x800, then the effect light/position block 0x1D99AB0 turned to that angle
	uint32_t __cdecl ud_797B40(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2D));
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, MEM<uint32_t>(DIR_PTR) + 8);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U16(d, 0xA) = 0;
		U16(d, 0) = 0;
		U16(d, 2) = 0;
		U16(d, 4) = 0;
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)S16(d, 0xC));
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		x::MAG_022_sub_8DD770(0x1D99AB0);
		x::MAG_022_sub_8DD8A0(0x1D99AB0, (uint32_t)(int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x4A));
		return 0; // void
	}

	// 0x808DD0 (MAG_055_sub_808DD0): DIRECTOR task - phase bookkeeping (0x808E50), then the state
	// {0x808EA0 stage darkening, 0x809020 cue to phase 1, 0x809080 sound, 0x8090D0 damage cue,
	// 0x8090F0 cue to phase 2, 0x809110 finish, ret}
	uint32_t __cdecl ud_797BF0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x797CC0;
		states[1] = 0x797CE0;
		states[2] = 0x797D30;
		states[3] = 0x797D80;
		states[4] = 0x797DA0;
		states[5] = 0x797DC0;
		states[6] = 0x797DE0; // nullsub (ret)
		ud_797C70(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ud_797C70(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ud_797E00(a1);
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

	// 0x797CC0 (MAG_083_UploadCherubTIM): director state 0 - queues the creature's TIM 0x1738C08 for upload,
	// next state
	uint32_t __cdecl ud_797CC0(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x1738C08);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl ud_797D30(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x1578440, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x797D80 (sub_797D80): director state 3 - at cut frame 0x6E: director +0x48 = 1 (the damage cue of the
	// emitter 0x79ECB0), next state
	uint32_t __cdecl ud_797D80(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) < 0x6E)
			return d;
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x797E00 (MAG_083_SpawnCherubCreature): director cut spawner (called by 0x797C70 on a new cut) - cut 1:
	// the creature 0x79E690 (0x140 bytes, model container 0x15786D4 animation 3), the actor system 0x797ED0
	// (sequence 0x172C818, mode 0, 0xF, 0; 0x797E80), the target absorption 0x79D9B0 (0xC8 bytes) and the
	// stage light 0x79D860 (0xB0 bytes)
	uint32_t __cdecl ud_797E00(uint32_t a1)
	{
		const int32_t cut = S16(MEM<uint32_t>(DIR_PTR), 0x40);
		if (cut - 1 != 0)
			return (uint32_t)(cut - 1);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_CREATURE, 0x79E690, 0x140, a1);
		x::Effect_BindModelContainerSetAnim(t, 0x15786D4, 3);
		a_73C100(a1, 0x797ED0, 0x172C818, 0, 0xF, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_ABSORB, 0x79D9B0, 0xC8, a1);
		return x::Effect_AddTaskAndInitFromCtx(Q_LIGHT, 0x79D860, 0xB0, a1);
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl ud_797F30(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			ud_798A40(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl ud_798110(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25CFA50);
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
	uint32_t __cdecl ud_798390(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25CFA50);
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
	uint32_t __cdecl ud_798A40(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25CFA50, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			a_743190();
			a_736C00();
			// 30 fps layer: see mag083_ultimecia_absorb_held.inc
			FX_HELD(held_note_system(sys);)
			ud_79AD10();
			ud_798AD0();
			sys = U32(0x25CFA50, 0);
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
		U16(0x25CFA3A, 0) = (uint16_t)(U16(0x25CFA3A, 0) + c14);
		U16(0x25CFA28, 0) = (uint16_t)(U16(0x25CFA28, 0) + c16);
		return done;
	}

	// 0x8C94D0 (copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl ud_798AD0(void)
	{
		uint32_t act = U32(U32(0x25CFA50, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25CFA50, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					ud_798B30(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x798B30 (sub_798B30): draw of a prim-model actor a1 (definition a2): effect 82's 0x7A01F0 (header 0x68
	// bytes on the module scratch stack, model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE /
	// colour +0x16C, scale words 0x100, one copy or one per sub-position +0x19C..) plus header +0x10 = 0x100
	// for the definition indices (+0x1D6) 4 and 10; module renderer 0x798CB0 into the frame arena 0x1D8E054
	uint32_t __cdecl ud_798B30(uint32_t a1, uint32_t a2)
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
		const uint8_t k = U8(a1, 0x1D6);
		if (k == 4 || k == 0xA)
			U32(h, 0x10) = 0x100;
		else
			U32(h, 0x10) = 0;
		const int8_t copies = S8(a1, 0x1D8);
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		if (copies == 1)
		{
			x::GTE_SetRotMatrix(a1 + 0xAC);
			x::GTE_SetTransVector(a1 + 0xAC);
			MEM<uint32_t>(0x1D8E054) = ud_798CB0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = ud_798CB0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
			}
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 ud_799290, FT4 ud_799710,
	// a_73D770, a_73D9C0, 0x73DCA0, ud_79A6C0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl ud_798CB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = ud_798DE0(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_799000(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_799290(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_799710(ctx, a2, a3, cur);
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
			cur = ud_79A1C0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return ud_79A6C0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl ud_798DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_799000(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_799290(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_799710(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_79A1C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_79A6C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8C9690 (copy of mb_827650 0x827650: copy of db_848D70 0x848D70: copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl ud_79AD10(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25CFA50), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25CFA50);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						ud_79ADD0(act);   // (the original also pushes the bone entry, unused)
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
							ud_79ADD0(act);
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
	uint32_t __cdecl ud_79ADD0(uint32_t a1)
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

	// 0x8CA840 (copy of mb_828BA0 0x828BA0: copy of db_849F20 0x849F20: copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl ud_79BEC0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25CFA50, 0);                  // actor state block
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

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl ud_79CC20(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25CFA50, 0);            // ecx
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
				uint32_t g = U32(0x25CFA50, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25CFA50, 0);
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

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl ud_79D830(uint32_t a1)
	{
		if (ud_798A40(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7AB360 (sub_7AB360): stage wobble state 1 - level +0x1C up by 0x200 per tick to 0x800 (then
	// next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl ud_79D910(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
		if (S16(a1, 0x1C) >= 0x800)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0x800;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = STAGE_LIGHT;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return p;
	}

	// 0x73F430 (module 095): wait state: next state at frame 0x50
	uint32_t __cdecl ud_79D950(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x50)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl ud_79D960(uint32_t a1)
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

	// 0x79D9B0 (sub_79D9B0): TARGET ABSORPTION task (0xC8 bytes, queue 0x25CA858) - states {0x79E530 start,
	// 0x79E610 countdown, ret}, then, while +0x26 bit 3, the target's model drawn by 0x79DA20 from the draw
	// block +0x6C
	uint32_t __cdecl ud_79D9B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x79E530;
		states[1] = 0x79E610;
		states[2] = 0x79E680; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 8) != 0)
		{
			// 30 fps layer: see mag083_ultimecia_absorb_held.inc
			FX_HELD(held_note_absorb(node);)
			ud_79DA20(node + 0x6C);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x79DA20 (sub_79DA20): draw of the target's model (draw block a1 = node +0x6C: +0x48 = the target's
	// battle entity): its world matrix (+0x40) copied to +0, its bone matrices from its pose (+0x60), the
	// view matrix +0x20 = camera * world (rotation columns, then the translation +0x14 -> +0x34), then the
	// body (+0x64) and the weapon (+0x78 -> +4) by 0x79DB00 into OT base + 0x44 (shift 4), the bone matrices
	// rebuilt from the pose
	uint32_t __cdecl ud_79DA20(uint32_t a1)
	{
		const uint32_t m = a1;
		const uint32_t ent = U32(m, 0x48);
		memcpy((void *)m, (const void *)(ent + 0x40), 8 * 4); // rep movsd
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(m);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(m + 0x20);
		x::GTE_LoadIRFromMatrixColumn(m + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(m + 0x22);
		x::GTE_LoadIRFromMatrixColumn(m + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(m + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(m + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(m + 0x34);
		ud_79DB00(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, m);
		// 30 fps layer: see mag083_ultimecia_absorb_held.inc
		FX_HELD(held_note_absorb_body();)
		const uint32_t weapon = U32(ent, 0x78);
		if (weapon != 0)
			ud_79DB00(U32(weapon, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, m);
		return x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
	}

	// 0x79DB00 (sub_79DB00): one model object a1 (bone groups of vertices, then flat triangles and quads)
	// pulled apart: every vertex lit by its bone (light matrix + background vector) into the vertex buffer
	// [[a4 +0x44] +4] (16-byte stride), then every polygon, with its 0x20-byte state in the buffer [0x25CF3F0]
	// (+0x20 per polygon, from the start for every object), drawn into OT a2 (packets at [[a4 +0x4C]], colour
	// = entity +0x28 | 0x24 / 0x2C code). Polygon state: +6 phase (0: offsets of v1 / v2 from v0, angle and
	// radius of v0 in xz; 1: waits until its radius >= the draw block's threshold +0x54; 2: spins round y,
	// speed +0x1E += 0x10 up to 0x100, falls 0x30 per tick and, from its 15th tick, shrinks its offsets by
	// +0x1C / 4096 (+0x1C -= 0x20 down to 0x100); drawn semi-transparent); a quad's 4th vertex has no offset
	// (it collapses onto v0 while spinning). Field_Alloc scratch 0x94 bytes.
	uint32_t __cdecl ud_79DB00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t m = a4;
		uint32_t cursor = MEM<uint32_t>(U32(m, 0x4C));
		const uint32_t verts = U32(U32(m, 0x44), 4);
		const uint32_t ent = U32(m, 0x48);
		const uint32_t table = U32(a1, 4);
		const uint32_t bones = U32(a1, 0) + 0x10;
		const int32_t count = S32(table, 0);
		const uint32_t s = x::Field_Alloc(0x94);
		uint32_t offs = table + 4;
		const uint32_t col = U32(ent, 0x28) & 0xFFFFFF;
		const uint32_t mask = U32(ent, 0x7C);
		uint32_t st = MEM<uint32_t>(0x25CF3F0);
		U32(s, 0x48) = col | 0x24000000;
		U16(s, 0x90) = U16(m, 0x54);
		U32(s, 0x50) = mask;
		U32(s, 0x4C) = col | 0x2C000000;
		U16(s, 0x8C) = 0;
		x::GTE_SetRotMatrix(m + 0x20);
		x::GTE_SetTransVector(m + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t dst = verts;
			uint32_t p = U32(a1, 4) + U32(offs, 0);
			offs += 4;
			if (((U32(s, 0x50) >> (i & 31)) & 1) == 0)
				continue;
			// vertices, grouped by bone
			const int32_t groups = S16(p, 0);
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t bone = S16(p, 0);
				p += 2;
				const uint32_t bm = (uint32_t)(bone * 0x30) + bones + 0x10;
				gx::GTE_SetLightMatrix(bm);
				gx::GTE_SetBackgroundVector(U32(bm, 0x14), U32(bm, 0x18), U32(bm, 0x1C));
				const int32_t nv = S16(p, 0);
				p += 2;
				for (int32_t k = nv; k > 0; k--)
				{
					U16(s, 0x28) = U16(p, 0);
					U16(s, 0x2A) = U16(p, 2);
					U16(s, 0x2C) = U16(p, 4);
					p += 6;
					x::GTE_LoadV0(s + 0x28);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(s + 0x28);
					U32(dst, 0) = U32(s, 0x28);
					U32(dst, 4) = U32(s, 0x2C);
					dst += 0x10;
				}
			}
			// polygons: header (triangle count, quad count, 8 bytes), 16-byte triangles, 20-byte quads
			p = (p + 3) & ~3u;
			const int32_t tris = S16(p, 0);
			const int32_t quads = S16(p, 2);
			uint32_t poly = p + 0xC;
			for (int32_t t = tris; t > 0; t--, poly += 0x10)
			{
				const uint32_t i0 = U16(poly, 0) & 0xFFF;
				const uint32_t i1 = U16(poly, 2) & 0xFFF;
				const uint32_t i2 = U16(poly, 4) & 0xFFF;
				U32(s, 0x38) = i0;
				U32(s, 0x3C) = i1;
				U32(s, 0x40) = i2;
				U32(s, 0x54) = U32(verts + i0 * 16, 0);
				U32(s, 0x58) = U32(verts + i0 * 16, 4);
				U32(s, 0x5C) = U32(verts + i1 * 16, 0);
				U16(s, 0x8C) = (uint16_t)(U16(s, 0x8C) + 1);
				U32(s, 0x60) = U32(verts + i1 * 16, 4);
				st += 0x20;
				U32(s, 0x64) = U32(verts + i2 * 16, 0);
				U32(s, 0x68) = U32(verts + i2 * 16, 4);
				absorb_step(s, st, false);
				gx::GTE_LoadV012Dwords(s + 0x54);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(s + 0x74);
				if (S32(s, 0x74) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(s + 0x78);
				const uint32_t z = U32(s, 0x78) >> 2;
				U32(s, 0x78) = z;
				gx::GTE_StoreSXY012_PolyFT3(cursor);
				U32(cursor, 0) = 0x7000000;
				U32(cursor, 0xC) = U32(poly, 8);
				U32(cursor, 0x14) = U32(poly, 0xC);
				U16(cursor, 0x1C) = U16(poly, 6);
				U32(cursor, 4) = U32(s, 0x48);
				if ((U8(poly, 0xF) & 2) != 0)
					U8(cursor, 7) |= 2;
				if (S16(st, 6) >= 2)
					U8(cursor, 7) |= 2;
				x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, cursor);
				cursor += 0x20;
			}
			uint32_t pq = poly + 4;
			for (int32_t q = quads; q > 0; q--, pq += 0x14)
			{
				const uint32_t i0 = U16(pq, -4) & 0xFFF;
				const uint32_t i1 = U16(pq, -2) & 0xFFF;
				const uint32_t i2 = U16(pq, 0) & 0xFFF;
				const uint32_t i3 = U16(pq, 2) & 0xFFF;
				U32(s, 0x38) = i0;
				U32(s, 0x3C) = i1;
				U32(s, 0x54) = U32(verts + i0 * 16, 0);
				U32(s, 0x40) = i2;
				U32(s, 0x58) = U32(verts + i0 * 16, 4);
				U32(s, 0x5C) = U32(verts + i1 * 16, 0);
				U16(s, 0x8C) = (uint16_t)(U16(s, 0x8C) + 1);
				U32(s, 0x64) = U32(verts + i2 * 16, 0);
				U32(s, 0x60) = U32(verts + i1 * 16, 4);
				const uint32_t v2hi = U32(verts + i2 * 16, 4);
				st += 0x20;
				U32(s, 0x44) = i3;
				U32(s, 0x6C) = U32(verts + i3 * 16, 0);
				U32(s, 0x70) = U32(verts + i3 * 16, 4);
				U32(s, 0x68) = v2hi;
				absorb_step(s, st, true);
				gx::GTE_LoadV012Dwords(s + 0x54);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(s + 0x74);
				if (S32(s, 0x74) <= 0)
					continue;
				gx::GTE_StoreSXY012_PolyFT3_2(cursor);
				x::GTE_LoadV0(s + 0x6C);
				x::GTE_RTPS();
				x::GTE_ReadSXY2(cursor + 0x20);
				x::GTE_AVSZ4();
				x::GTE_ReadOTZ(s + 0x78);
				const uint32_t z = U32(s, 0x78) >> 2;
				U32(s, 0x78) = z;
				U32(cursor, 0) = 0x9000000;
				U32(cursor, 0xC) = U32(pq, 4);
				U32(cursor, 0x14) = U32(pq, 8);
				U16(cursor, 0x1C) = U16(pq, 0xC);
				U16(cursor, 0x24) = U16(pq, 0xE);
				U32(cursor, 4) = U32(s, 0x4C);
				if ((U8(pq, 0xB) & 2) != 0)
					U8(cursor, 7) |= 2;
				if (S16(st, 6) >= 2)
					U8(cursor, 7) |= 2;
				x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, cursor);
				cursor += 0x28;
			}
		}
		MEM<uint32_t>(U32(m, 0x4C)) = cursor;
		return x::Field_Free(0x94);
	}

	// 0x79E530 (sub_79E530): absorption state 0 - from tick 0x37: the draw block +0x6C set up (0x79E590:
	// vertex buffer 0x25CA868, its descriptor 0x25CFA58, the target's entity; packet cursor 0x25C9F74), the
	// polygon states [0x25CF3F0] = 0x209FAB8 cleared (0x79E5F0), draw on (+0x26 bit 3), the target's own
	// model hidden (entity +0 bit 2), threshold +0xC0 = 0x300, countdown +0xC4 = 0x5A, next state
	uint32_t __cdecl ud_79E530(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t m = node + 0x6C;
		const uint32_t ent = ENT(U8(node, 0x2D));
		if (S16(node, 0x24) < 0x37)
			return 0; // void
		ud_79E590(m, 0x25CA868, 0x25CFA58, ent);
		ud_79E5F0();
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 8);
		U8(ent, 0) = (uint8_t)(U8(ent, 0) | 4);
		U16(m, 0x54) = 0x300;
		U16(node, 0xC4) = 0x5A;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x79E590 (sub_79E590): draw block a1 (0x58 bytes) cleared; +0x48 = entity a4, +0x40 = vertex buffer a2,
	// +0x4C / +0x50 = the module packet cursor 0x25C9F74, +0x44 = descriptor a3 (+4 = a2, +0x14.. = 0, 0,
	// 0x140, 0, colour 0x80 x3, -1, 0)
	uint32_t __cdecl ud_79E590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x58);
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

	// 0x79E5F0 (sub_79E5F0): the polygon state buffer [0x25CF3F0] = 0x209FAB8 (sub_571B70), 0x7D00 bytes cleared
	uint32_t __cdecl ud_79E5F0(void)
	{
		const uint32_t b = gx::sub_571B70();
		MEM<uint32_t>(0x25CF3F0) = b;
		return x::MAG_007_sub_8DCC00(b, 0x7D00);
	}

	// 0x79E610 (sub_79E610): absorption state 1 - threshold +0xC0 down by 0x10 per tick (to 0), countdown
	// +0xC4: at 0x3C the target's shadow off (entity +0 bit 5), at 0 the target's model shown again (entity
	// +0 bit 2 cleared), draw off, finished, next state
	uint32_t __cdecl ud_79E610(uint32_t a1)
	{
		const uint32_t ent = ENT(U8(a1, 0x2D));
		U16(a1, 0xC0) = (uint16_t)(U16(a1, 0xC0) - 0x10);
		if (S16(a1, 0xC0) < 0)
			U16(a1, 0xC0) = 0;
		U16(a1, 0xC4) = (uint16_t)(U16(a1, 0xC4) - 1);
		const int16_t c = S16(a1, 0xC4);
		if (c == 0x3C)
		{
			U8(ent, 0) = (uint8_t)(U8(ent, 0) | 0x20);
			return 0; // void
		}
		if (c > 0)
			return 0; // void
		U16(ent, 0) = (uint16_t)(U16(ent, 0) & 0xFFFB);
		U16(a1, 0x26) = (uint16_t)((U16(a1, 0x26) & 0xFFF7) | 1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x79E690 (sub_79E690): CREATURE task (0x140 bytes, queue 0x25CFA18; model container 0x15786D4) - states
	// {0x79E960 appear, 0x79EB70 fade in (animation 0), 0x79EBE0 animation 1, 0x79EC20 animation 2 to frame
	// 0x15, 0x79EC40 fade out, ret}, position +0x4C += velocity +0x124, drawn (a_746C10 into the module packet
	// cursor) unless hidden (+0x26 bit 2), its matrix +0x70 copied to the effect light/position block
	// 0x1D99AB0, its translation +0x84 = the position
	uint32_t __cdecl ud_79E690(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x79E960;
		states[1] = 0x79EB70;
		states[2] = 0x79EBE0;
		states[3] = 0x79EC20;
		states[4] = 0x79EC40;
		states[5] = 0x79ECA0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + U16(node, 0x128));
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + U16(node, 0x124));
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + U16(node, 0x126));
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag083_ultimecia_absorb_held.inc
			FX_HELD(held_note_creature(node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, 0x25CF3F8, MEM<uint32_t>(PACKET_CURSOR));
		}
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 8 * 4); // rep movsd
		S32(node, 0x88) = S16(node, 0x4E);
		S32(node, 0x84) = S16(node, 0x4C);
		const uint8_t status = U8(node, 0x26);
		S32(node, 0x8C) = S16(node, 0x50);
		return task_end(node, status);
	}

	// 0x79E960 (sub_79E960): creature state 0 - animation 0, position = director +8 / +0xC with y = -0x580,
	// placement flag +0x30 bit 5, y velocity +0x126 = 0x20, scale 0x1000, fade +0x13A / +0x13C = 0x1000
	// (0x79E9E0 mode 3, colour 0x79EB00), next state
	uint32_t __cdecl ud_79E960(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_7(a1, 0);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t xz = U32(d, 8);
		const uint32_t zz = U32(d, 0xC);
		U32(a1, 0x4C) = xz;
		U16(a1, 0x30) = (uint16_t)(U16(a1, 0x30) | 0x20);
		U32(a1, 0x50) = zz;
		U16(a1, 0x4E) = 0xFA80;
		U16(a1, 0x126) = 0x20;
		U32(a1, 0x11C) = 0x1000;
		U32(a1, 0x118) = 0x1000;
		U32(a1, 0x114) = 0x1000;
		U16(a1, 0x13A) = 0x1000;
		U16(a1, 0x13C) = 0x1000;
		ud_79E9E0(a1, 3);
		ud_79EB00(a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8CD310 (copy of t_7678B0 0x7678B0: module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl ud_79E9E0(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x8CD430 (copy of t_768130 0x768130: module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl ud_79EB00(uint32_t a1)
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

	// 0x79EB70 (sub_79EB70): creature state 1 - fade +0x13A down by 0x100 per tick (to 0, +0x13C = it),
	// animation step; at its end animation 1, stops (+0x126 = 0), next state
	uint32_t __cdecl ud_79EB70(uint32_t a1)
	{
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0xFF00);
		if (S16(a1, 0x13A) <= 0)
			U16(a1, 0x13A) = 0;
		U16(a1, 0x13C) = U16(a1, 0x13A);
		ud_79E9E0(a1, 3);
		ud_79EB00(a1);
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(a1);
		if (r == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, r);
			U16(a1, 0x126) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x79EBE0 (sub_79EBE0): creature state 2 - animation step; at its end animation 2, rises (+0x126 =
	// -0x20), next state
	uint32_t __cdecl ud_79EBE0(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 2);
			U16(a1, 0x126) = 0xFFE0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x79EC20 (sub_79EC20): creature state 3 - animation step (0x8DD1C0) until its frame +0x136 >= 0x15
	uint32_t __cdecl ud_79EC20(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 0x15)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x79EC40 (sub_79EC40): creature state 4 - animation step, fade +0x13A up by 0x100 per tick; at 0x1000
	// hidden and finished (+0x26 |= 5), next state
	uint32_t __cdecl ud_79EC40(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0x100);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U16(a1, 0x13A) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		ud_79E9E0(a1, 3);
		ud_79EB00(a1);
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl ud_79ECB0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x157843C);
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

	// ---- generated (gendesc.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7979C0, (void *)a_73A0D0, "083 MAG_083_sub_7979C0" },
		{ 0x7979D0, (void *)a_73A0D0, "083 MAG_083_sub_7979D0" },
		{ 0x7979E0, (void *)a_732120, "083 MAG_083_sub_7979E0" },
		{ 0x797A20, (void *)a_732160, "083 MAG_083_sub_797A20" },
		{ 0x797A60, (void *)a_73A170, "083 MAG_083_sub_797A60" },
		{ 0x797A90, (void *)a_73A1A0, "083 MAG_083_sub_797A90" },
		{ 0x797CE0, (void *)a_73B790, "083 sub_797CE0" },
		{ 0x797D00, (void *)a_73B640, "083 sub_797D00" },
		{ 0x797D60, (void *)a_73B7E0, "083 sub_797D60" },
		{ 0x797DA0, (void *)a_73B8A0, "083 sub_797DA0" },
		{ 0x797DC0, (void *)a_7475A0, "083 sub_797DC0" },
		{ 0x797DE0, (void *)a_73A6D0, "083 nullsub_1343" },
		{ 0x797DF0, (void *)a_73A6D0, "083 nullsub_1344" },
		{ 0x797E80, (void *)a_73C100, "083 sub_797E80" },
		{ 0x797ED0, (void *)a_73A380, "083 sub_797ED0" },
		{ 0x797F60, (void *)a_73F990, "083 sub_797F60" },
		{ 0x7981F0, (void *)a_73FC20, "083 sub_7981F0" },
		{ 0x798460, (void *)a_73FE90, "083 sub_798460" },
		{ 0x798580, (void *)a_73FFB0, "083 sub_798580" },
		{ 0x7987E0, (void *)a_740210, "083 sub_7987E0" },
		{ 0x799C90, (void *)a_73D770, "083 sub_799C90" },
		{ 0x799EE0, (void *)a_73D9C0, "083 sub_799EE0" },
		{ 0x79AE50, (void *)a_736C00, "083 sub_79AE50" },
		{ 0x79AE90, (void *)a_740880, "083 sub_79AE90" },
		{ 0x79AF20, (void *)a_740910, "083 sub_79AF20" },
		{ 0x79B580, (void *)a_740F70, "083 sub_79B580" },
		{ 0x79B5D0, (void *)a_740FC0, "083 sub_79B5D0" },
		{ 0x79B660, (void *)a_741050, "083 sub_79B660" },
		{ 0x79B730, (void *)a_741120, "083 sub_79B730" },
		{ 0x79BFD0, (void *)a_7419C0, "083 sub_79BFD0" },
		{ 0x79C050, (void *)a_741A40, "083 sub_79C050" },
		{ 0x79C080, (void *)a_741A70, "083 sub_79C080" },
		{ 0x79C120, (void *)a_737ED0, "083 sub_79C120" },
		{ 0x79C170, (void *)a_741B60, "083 sub_79C170" },
		{ 0x79C1A0, (void *)a_737F50, "083 sub_79C1A0" },
		{ 0x79C220, (void *)a_737FD0, "083 sub_79C220" },
		{ 0x79C8A0, (void *)a_742290, "083 au_re__rand_22" },
		{ 0x79C8D0, (void *)a_7422C0, "083 sub_79C8D0" },
		{ 0x79C910, (void *)a_742300, "083 sub_79C910" },
		{ 0x79C960, (void *)a_742350, "083 sub_79C960" },
		{ 0x79C9B0, (void *)a_7423A0, "083 au_re__rand_22_0" },
		{ 0x79CA00, (void *)a_7387B0, "083 sub_79CA00" },
		{ 0x79CAC0, (void *)a_7424B0, "083 sub_79CAC0" },
		{ 0x79CAD0, (void *)a_7424C0, "083 sub_79CAD0" },
		{ 0x79D2B0, (void *)a_742CA0, "083 sub_79D2B0" },
		{ 0x79D2E0, (void *)a_742CD0, "083 sub_79D2E0" },
		{ 0x79D310, (void *)a_742D00, "083 sub_79D310" },
		{ 0x79D4C0, (void *)a_742EB0, "083 sub_79D4C0" },
		{ 0x79D600, (void *)a_742FF0, "083 sub_79D600" },
		{ 0x79D710, (void *)a_743100, "083 sub_79D710" },
		{ 0x79D7A0, (void *)a_743190, "083 sub_79D7A0" },
		{ 0x79D850, (void *)a_73A6D0, "083 nullsub_1345" },
		{ 0x79D860, (void *)a_73F110, "083 sub_79D860" },
		{ 0x79D8D0, (void *)a_7335D0, "083 sub_79D8D0" },
		{ 0x79D9A0, (void *)a_73A6D0, "083 nullsub_1346" },
		{ 0x79E680, (void *)a_73A6D0, "083 nullsub_1347" },
		{ 0x79E780, (void *)a_746C10, "083 sub_79E780" },
		{ 0x79EA00, (void *)a_733950, "083 sub_79EA00" },
		{ 0x79ECA0, (void *)a_73A6D0, "083 nullsub_1348" },
		{ 0x79ECF0, (void *)a_7474B0, "083 MAG_083_sub_79ECF0" },
		{ 0x79ED10, (void *)a_7474D0, "083 MAG_083_sub_79ED10" },
		{ 0x79ED30, (void *)a_73A6D0, "083 nullsub_1349" },
		{ 0x79ED40, (void *)a_747500, "083 MAG_083_sub_79ED40" },
		{ 0x79ED80, (void *)a_73A0D0, "083 MAG_083_sub_79ED80" },
		{ 0x79ED90, (void *)a_747550, "083 MAG_083_sub_79ED90" },
		{ 0x79EDA0, (void *)a_747560, "083 MAG_083_sub_79EDA0" },
		{ 0x79EDD0, (void *)a_747590, "083 MAG_083_sub_79EDD0" },
		{ 0x79EDE0, (void *)a_7475A0, "083 MAG_083_sub_79EDE0" },
		{ 0x79EE00, (void *)a_73A6D0, "083 nullsub_1342" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x797840, (void *)ud_797840, "083 ud_797840" },
		{ 0x797B10, (void *)ud_797B10, "083 ud_797B10" },
		{ 0x797B40, (void *)ud_797B40, "083 ud_797B40" },
		{ 0x797BF0, (void *)ud_797BF0, "083 ud_797BF0" },
		{ 0x797C70, (void *)ud_797C70, "083 ud_797C70" },
		{ 0x797CC0, (void *)ud_797CC0, "083 ud_797CC0" },
		{ 0x797D30, (void *)ud_797D30, "083 ud_797D30" },
		{ 0x797D80, (void *)ud_797D80, "083 ud_797D80" },
		{ 0x797E00, (void *)ud_797E00, "083 ud_797E00" },
		{ 0x797F30, (void *)ud_797F30, "083 ud_797F30" },
		{ 0x798110, (void *)ud_798110, "083 ud_798110" },
		{ 0x798390, (void *)ud_798390, "083 ud_798390" },
		{ 0x798A40, (void *)ud_798A40, "083 ud_798A40" },
		{ 0x798AD0, (void *)ud_798AD0, "083 ud_798AD0" },
		{ 0x798B30, (void *)ud_798B30, "083 ud_798B30" },
		{ 0x798CB0, (void *)ud_798CB0, "083 ud_798CB0" },
		{ 0x798DE0, (void *)ud_798DE0, "083 ud_798DE0" },
		{ 0x799000, (void *)ud_799000, "083 ud_799000" },
		{ 0x799290, (void *)ud_799290, "083 ud_799290" },
		{ 0x799710, (void *)ud_799710, "083 ud_799710" },
		{ 0x79A1C0, (void *)ud_79A1C0, "083 ud_79A1C0" },
		{ 0x79A6C0, (void *)ud_79A6C0, "083 ud_79A6C0" },
		{ 0x79AD10, (void *)ud_79AD10, "083 ud_79AD10" },
		{ 0x79ADD0, (void *)ud_79ADD0, "083 ud_79ADD0" },
		{ 0x79BEC0, (void *)ud_79BEC0, "083 ud_79BEC0" },
		{ 0x79CC20, (void *)ud_79CC20, "083 ud_79CC20" },
		{ 0x79D830, (void *)ud_79D830, "083 ud_79D830" },
		{ 0x79D910, (void *)ud_79D910, "083 ud_79D910" },
		{ 0x79D950, (void *)ud_79D950, "083 ud_79D950" },
		{ 0x79D960, (void *)ud_79D960, "083 ud_79D960" },
		{ 0x79D9B0, (void *)ud_79D9B0, "083 ud_79D9B0" },
		{ 0x79DA20, (void *)ud_79DA20, "083 ud_79DA20" },
		{ 0x79DB00, (void *)ud_79DB00, "083 ud_79DB00" },
		{ 0x79E530, (void *)ud_79E530, "083 ud_79E530" },
		{ 0x79E590, (void *)ud_79E590, "083 ud_79E590" },
		{ 0x79E5F0, (void *)ud_79E5F0, "083 ud_79E5F0" },
		{ 0x79E610, (void *)ud_79E610, "083 ud_79E610" },
		{ 0x79E690, (void *)ud_79E690, "083 ud_79E690" },
		{ 0x79E960, (void *)ud_79E960, "083 ud_79E960" },
		{ 0x79E9E0, (void *)ud_79E9E0, "083 ud_79E9E0" },
		{ 0x79EB00, (void *)ud_79EB00, "083 ud_79EB00" },
		{ 0x79EB70, (void *)ud_79EB70, "083 ud_79EB70" },
		{ 0x79EBE0, (void *)ud_79EBE0, "083 ud_79EBE0" },
		{ 0x79EC20, (void *)ud_79EC20, "083 ud_79EC20" },
		{ 0x79EC40, (void *)ud_79EC40, "083 ud_79EC40" },
		{ 0x79ECB0, (void *)ud_79ECB0, "083 ud_79ECB0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag083_ultimecia_absorb()
	{
		act::register_module(83);
		for (const act::u083::ModPort *p = act::u083::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(83, p->addr, p->port, p->name);
		for (const act::u083::ModPort *p = act::u083::PORTS; p->addr; p++)
			act::register_module_port(83, p->addr, p->port, p->name);
		// 30 fps layer: see mag083_ultimecia_absorb_held.inc
		FX_HELD(register_mag083_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag083_ultimecia_absorb_held.inc"
#endif
