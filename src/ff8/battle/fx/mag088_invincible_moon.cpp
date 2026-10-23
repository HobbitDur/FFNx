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

// Effect 88: Invincible Moon (kernel.bin Rinoa limit break part 2 {Combine} Invincible Moon, section 25 entry 2, effect id +2 = 88;
// MAG_088_INVINCIBLE_MOON*): a copy of the actor/heal effect library (see act_engine.h) with its own parts: Angelo howls at the
// moon with Rinoa's double, the moon and its glows rise behind the party, a halo, dust rings, a screen flash, the moon's prim
// play and an actor system; the party becomes invincible. Module 0x7684D0..0x773850 (234 functions: 99 served by engine ports,
// 62 the relocated text of a ported library function (Angelo Recover 93 / Reverse 94 / Rush 91 / Search 92, Doom 8, Siren 95,
// Boko 97..99, Tonberry 90, Ultimecia 80, Laser Eye 71, Bio 20), 71 hand-ported (the master is the module's own port of the
// engine master a_739F40), the loader and the setup not ported).
//
// Setup MAG_088_INVINCIBLE_MOON 0x7684F0 (runs once, not ported; loader 0x7684D0) creates the root queue with the master and
// the module's task pools and starts the engine camera animation.
//   Master (0x768690 = the engine master a_739F40) - camera copy 0x2793E58 (pointers 0x25ACD0C / 0x25A4EA8), packet arenas by
//     tick parity (module cursor 0x25A4D9C / 0x25A4EA4), target bone follow, 11 states (0x768970: the director block 0x768E20,
//     the module data block 0x7689C0, the emitter 0x768AC0 and the DIRECTOR 0x768ED0), the nine module queues.
//   Director (0x768ED0, 15 states) - TIM uploads 0x16F3000 / 0x16FB420, camera return armed (0x50A730), the CAMERA task 0x769060
//     (a_732A00: camera script chain 0x1555D60.. through the library camera stepper a_73AE10 / a_73AB20, from the director block
//     or the party's centre 0x769A90); cues 1..6, the stage dimming 0x769DD0 / 0x769E90, done flag +0x48 at frame 0x23.
//   Phase 1 (0x76A000) - ANGELO 0x771DB0 (node 0x160, model container 0x154F91C) and RINOA'S DOUBLE 0x7718D0 (container
//     0x155563C), both drawn by the creature renderer 0x7719A0 = a_746C10 (state block 0x25AB8B8) into the module packet pool.
//     Angelo binds Rinoa's animation set (com data 0x15560C0) and queues her chain transformations 1, 2 and 4; dust rings
//     0x772060 / 0x772990 (prim sequence drawn by 0x7720C0) on his foot-down frames; the howl play 0x772C50 (layout 0x16F1F8C,
//     callback 0x76A380); the halo 0x772D90 (model drawn by 0x772E30); the screen flash 0x773090 (the engine's 0x8DDEC0 with the
//     module pause word 0x25ACD34; colour script 0x1557CAC).
//   Phase 3 - the moon and its two glows 0x771660 (models drawn by 0x7716C0, x drift).
//   Phase 4 - the actor system 0x76C700 (a_73C100: data [block +0x104]), the moon play 0x76A180 (layout [block +0x128], node
//     0x474, callback 0x76A380) and the stage-light task 0x771510 (light words 0x1D98992 + k * 0x2C up to 0xA00).
//   Damage state 0x773730.. (the library's finish states): ApplyActionResultToTarget at the director's done flag.
// Writes outside the module: Rinoa's entity record (BS_BindEntityModelFromComData 0x5022C0 binds the animation set,
// QueueChainTransformation 0x505C00 chain transformations 1, 2 and 4), the camera words 0xB8B7F0.. through the library camera
// script (a_73AB20 / a_73AE10, camera block [0x154F918]) and the armed camera return, the stage light words 0x1D98992.., the
// root-matrix slot 0x1D99AB0 (Angelo), the camera copy 0x2793E58, action results (0x506690).
// Module globals: 0x25A4D98..0x25ACEAC (+ camera copy 0x2793E58, 0x20 bytes, and the module scratch stack below it).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace moon88
{
	// ---- generated (desc88.py): the module descriptor of this copy of the library ----
	static const Mod MOD_088 = { "invinciblemoon", 88, 0x7684D0, 0x773850,
		{ 0x0, 0x0, 0x154ECE0, 0x154F918, 0x0, 0x25A4EA4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25A4D98, 0x25A4D9C, 0x25A4E80, 0x25A4EA4, 0x25A4EA8, 0x25A6BC0, 0x0, 0x25A6BBC, 0x25AA9F0, 0x25A4F88, 0x25AAA00, 0x25AAA04, 0x25AB498, 0x25AB728, 0x25AB738, 0x25AB8A8, 0x25AB8AC, 0x25AB8B0, 0x25AC3A8, 0x25AC6C8, 0x25AC6D8, 0x0, 0x25AC750, 0x25ACCF0, 0x25ACCF8, 0x25ACD0C, 0x25ACD34, 0x25ACD3C },
		{ 0x0, 0x769120, 0x769150, 0x769950, 0x769980, 0x7699C0, 0x7699F0, 0x769A20, 0x769A50, 0x769B20, 0x769B60, 0x769B90, 0x769BC0, 0x769BF0, 0x769C10, 0x769C30, 0x769C60, 0x0, 0x0, 0x0, 0x0, 0x0, 0x76D5E0, 0x76D7F0, 0x76DA70, 0x76DCB0, 0x76DF50, 0x76E180, 0x76E440, 0x76E6A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x768820, 0x768830, 0x768840, 0x7688C0, 0x7688F0, 0x768970, 0x0, 0x768CC0, 0x768CF0, 0x768D20, 0x768D30, 0x768D40, 0x768D50, 0x768D60, 0x0, 0x0, 0x769D10, 0x0, 0x0, 0x7691A0, 0x7694F0, 0x0, 0x0, 0x0, 0x769030, 0x769C50, 0x0, 0x76A380, 0x0, 0x0, 0x0, 0x0, 0x0, 0x76C940, 0x76CA20, 0x76CBC0, 0x76CC90, 0x76CDB0, 0x76D010, 0x0, 0x0, 0x76F1F0, 0x76F240, 0x0, 0x0, 0x76FB30, 0x0, 0x0, 0x0, 0x770510, 0x770540, 0x770620, 0x770740, 0x770890, 0x0, 0x770F50, 0x771130, 0x771270, 0x772320, 0x0, 0x7736C0, 0x773730, 0x773750, 0x773770, 0x773780, 0x7737C0, 0x7737D0, 0x7737E0, 0x773810, 0x773820, 0x773840 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x154ECE0;        // pointer to the director block (phase +0x40 / requested +0x42 / next +0x44, frame +0x46, done +0x48)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = 0x25A4D9C;  // module packet cursor (tick parity: [0x25AB8AC] / [0x25AB8A8])
	static const uint32_t SCRATCH_SP = 0x25ACD0C;     // module scratch stack pointer (the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25ACD3C;    // current actor system state block (pool header)
	static const uint32_t FLASH_PAUSE = 0x25ACD34;    // the module's pause word (the screen flash holds its script while set)
	static const uint32_t CREATURE_ARG = 0x25AB8B8;   // the creature renderer's state block (Angelo and Rinoa's double)
	static const uint32_t STAGE_LIGHT = 0x1D98992;    // the four stage draw entries' light words (+ k * 0x2C)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot
	// the state advance of the Boko / Doom texts (b3_ / b103_ helpers)
	static inline void b3_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void b103_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t BindEntityModelFromComData(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5022C0)(a1, a2, a3); } // BS_BindEntityModelFromComData (entity, 0x20-byte block, com data)
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); } // (entity, chain transformation id)
		static inline uint32_t BS_Camera_ArmReturnAfterEffect() { return fn<uint32_t (__cdecl *)()>(0x50A730)(); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
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
	uint32_t __cdecl im_768690(uint32_t a1);
	uint32_t __cdecl im_768840(uint32_t a1);
	uint32_t __cdecl im_768880();
	uint32_t __cdecl im_768970(uint32_t a1);
	uint32_t __cdecl im_7689C0();
	uint32_t __cdecl im_768E20(uint32_t a1);
	uint32_t __cdecl im_768ED0(uint32_t a1);
	uint32_t __cdecl im_768F90(uint32_t a1);
	uint32_t __cdecl im_768FE0(uint32_t a1);
	uint32_t __cdecl im_769120(uint32_t a1);
	uint32_t __cdecl im_769150(uint32_t a1);
	uint32_t __cdecl im_769950(uint32_t a1);
	uint32_t __cdecl im_769980(uint32_t a1);
	uint32_t __cdecl im_7699C0(uint32_t a1);
	uint32_t __cdecl im_7699F0(uint32_t a1);
	uint32_t __cdecl im_769A20(uint32_t a1);
	uint32_t __cdecl im_769A50(uint32_t a1);
	uint32_t __cdecl im_769A90(uint32_t a1);
	uint32_t __cdecl im_769B20(uint32_t a1);
	uint32_t __cdecl im_769B60(uint32_t a1);
	uint32_t __cdecl im_769B90(uint32_t a1);
	uint32_t __cdecl im_769BC0(uint32_t a1);
	uint32_t __cdecl im_769C10(uint32_t a1);
	uint32_t __cdecl im_769C70(uint32_t a1);
	uint32_t __cdecl im_769D90(uint32_t a1);
	uint32_t __cdecl im_769DD0(void);
	uint32_t __cdecl im_769E10(uint32_t a1);
	uint32_t __cdecl im_769E40(uint32_t a1);
	uint32_t __cdecl im_769E90(void);
	uint32_t __cdecl im_769ED0(uint32_t a1);
	uint32_t __cdecl im_769EF0(uint32_t a1);
	uint32_t __cdecl im_769F20(uint32_t a1);
	uint32_t __cdecl im_769F80(uint32_t a1);
	uint32_t __cdecl im_769FA0(uint32_t a1);
	uint32_t __cdecl im_769FC0(uint32_t a1);
	uint32_t __cdecl im_76A000(uint32_t a1);
	uint32_t __cdecl im_76A130(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl im_76A180(uint32_t a1);
	uint32_t __cdecl im_76A1F0(uint32_t a1);
	uint32_t __cdecl im_76A380(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl im_76A650(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76A780(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76A9A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76AC30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76B0B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76BB60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76C060(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76C6B0(uint32_t a1);
	uint32_t __cdecl im_76C760(uint32_t a1);
	uint32_t __cdecl im_76C940(uint32_t a1);
	uint32_t __cdecl im_76CBC0(void);
	uint32_t __cdecl im_76D270(uint32_t a1);
	uint32_t __cdecl im_76D300(void);
	uint32_t __cdecl im_76D360(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_76D5E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76D7F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76DF50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76E180(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76E440(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76E6A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_76E980(void);
	uint32_t __cdecl im_76EA40(uint32_t a1);
	uint32_t __cdecl im_76EAC0(void);
	uint32_t __cdecl im_76EB00(uint32_t a1);
	uint32_t __cdecl im_76FB30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_76FC40(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_76FCC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_76FCF0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl im_76FD90(uint32_t a1);
	uint32_t __cdecl im_76FE10(uint32_t a1);
	uint32_t __cdecl im_76FE90(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl im_770670(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_770890(uint32_t a1);
	uint32_t __cdecl im_771380(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_771410(void);
	uint32_t __cdecl im_7714A0(uint32_t a1);
	uint32_t __cdecl im_7714D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl im_7715C0(uint32_t a1);
	uint32_t __cdecl im_771600(uint32_t a1);
	uint32_t __cdecl im_771610(uint32_t a1);
	uint32_t __cdecl im_771660(uint32_t a1);
	uint32_t __cdecl im_7716C0(uint32_t a1);
	uint32_t __cdecl im_7717A0(uint32_t a1);
	uint32_t __cdecl im_771880(uint32_t a1);
	uint32_t __cdecl im_7718D0(uint32_t a1);
	uint32_t __cdecl im_771B80(uint32_t a1);
	uint32_t __cdecl im_771BD0(uint32_t a1);
	uint32_t __cdecl im_771C00(uint32_t a1);
	uint32_t __cdecl im_771C30(uint32_t a1);
	uint32_t __cdecl im_771C60(uint32_t a1);
	uint32_t __cdecl im_771C90(uint32_t a1);
	uint32_t __cdecl im_771CC0(uint32_t a1);
	uint32_t __cdecl im_771CF0(uint32_t a1);
	uint32_t __cdecl im_771D20(uint32_t a1);
	uint32_t __cdecl im_771D50(uint32_t a1);
	uint32_t __cdecl im_771D70(uint32_t a1);
	uint32_t __cdecl im_771DB0(uint32_t a1);
	uint32_t __cdecl im_771F50(uint32_t a1);
	uint32_t __cdecl im_772060(uint32_t a1);
	uint32_t __cdecl im_7720C0(uint32_t a1);
	uint32_t __cdecl im_772790(uint32_t a1);
	uint32_t __cdecl im_7727B0(uint32_t a1);
	uint32_t __cdecl im_772880(uint32_t a1);
	uint32_t __cdecl im_772990(uint32_t a1);
	uint32_t __cdecl im_7729F0(uint32_t a1);
	uint32_t __cdecl im_772A10(uint32_t a1);
	uint32_t __cdecl im_772AB0(uint32_t a1);
	uint32_t __cdecl im_772B40(uint32_t a1);
	uint32_t __cdecl im_772B80(uint32_t a1);
	uint32_t __cdecl im_772BE0(uint32_t a1);
	uint32_t __cdecl im_772CB0(uint32_t a1);
	uint32_t __cdecl im_772D10(uint32_t a1);
	uint32_t __cdecl im_772D40(uint32_t a1);
	uint32_t __cdecl im_772D90(uint32_t a1);
	uint32_t __cdecl im_772E30(uint32_t a1);
	uint32_t __cdecl im_772F50(uint32_t a1);
	uint32_t __cdecl im_772FB0(uint32_t a1);
	uint32_t __cdecl im_772FE0(uint32_t a1);
	uint32_t __cdecl im_773010(uint32_t a1);
	uint32_t __cdecl im_773040(uint32_t a1);
	uint32_t __cdecl im_773090(uint32_t a1);
	uint32_t __cdecl im_773100(uint32_t a1);
	uint32_t __cdecl im_773130(uint32_t a1);
	uint32_t __cdecl im_7732F0(uint32_t a1);
	uint32_t __cdecl im_773320(uint32_t a1);
	uint32_t __cdecl im_7733B0(uint32_t a1);
	uint32_t __cdecl im_773400(uint32_t a1, uint32_t a2);
	uint32_t __cdecl im_773520(uint32_t a1);
	uint32_t __cdecl im_773560(uint32_t a1);
	uint32_t __cdecl im_773590(uint32_t a1);
	uint32_t __cdecl im_773600(uint32_t a1);
	uint32_t __cdecl im_773630(uint32_t a1);
	uint32_t __cdecl im_773670(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag088_invincible_moon_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace moon88
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Angelo Recover / Rush / Search / Doom / Siren / MiniMog / Boko / Tonberry / The End port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x768690 (MAG_088_sub_768690): MASTER task - the engine's act-library master (a_739F40: camera matrix snapshot into
	// 0x2793E58, double-buffered packet arenas by tick parity, target bone follow, 11-state table, the nine module queues) as the
	// module's own port, with the held seam after the queues
	uint32_t __cdecl im_768690(uint32_t a1)
	{
		g_mod = &MOD_088;
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_master();)

		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		MEM<uint32_t>(G(G_258FB68)) = 0x2793E58;
		MEM<uint32_t>(G(G_257F9B0)) = 0x2793E58;
		uint32_t states[11] = {
			F(F_73A0D0), F(F_73A0E0), F(F_73A0F0), F(F_73A170), F(F_747500), F(F_747540),
			F(F_747550), F(F_747560), F(F_747590), F(F_7475A0), F(F_7475C0),
		};
		if (U8(a1, 0x5C) & 1) // +0x5C tick counter parity -> buffer 1
		{
			const uint32_t v1 = MEM<uint32_t>(G(G_258BFA4));
			const uint32_t v2 = MEM<uint32_t>(G(G_258A024));
			MEM<uint32_t>(G(G_257F8A4)) = v1;
			MEM<uint32_t>(G(G_257F9AC)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(G(G_258BFA0));
			const uint32_t v2 = MEM<uint32_t>(G(G_258A020));
			MEM<uint32_t>(G(G_257F8A4)) = v1;
			MEM<uint32_t>(G(G_257F9AC)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(a1);
		callp(states[S8(a1, 0x29)], a1); // +0x29 state index

		U16(a1, 0x5E) = 0; // live task count of this tick
		U16(G(G_258FB40), 0) = 0;
		U16(G(G_258EC50), 0) = 0;
		static const int queues[9] = { G_258BE30, G_257F988, G_258FB48, G_258EC40, G_258BA90,
			G_258A010, G_257FA90, G_2585EF0, G_258BE20 };
		for (int i = 0; i < 9; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(G(queues[i]));
			U16(a1, 0x5E) = (uint16_t)(U16(a1, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) + 1);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1); // frame counter
		return task_end(a1, status);
	}

	// 0x74E6D0 (MAG_093_sub_74E6D0): master state 2 - once no child is alive: the two pools are carved from the file arena
	// cursor [0x2594D8C] (0x870 bytes at 0x2594D64, 0xB7C0 bytes at 0x2594D84) and cleared (0x74E710), next state
	uint32_t __cdecl im_768840(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25ACD38);
		MEM<uint32_t>(0x25AC6DC) = p;
		p += 0x870;
		MEM<uint32_t>(0x25ACD30) = p;
		p += 0xB7C0;
		MEM<uint32_t>(0x25ACD38) = p;
		im_768880();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x74E710 (MAG_093_sub_74E710): clears the two pools (0x870 / 0xB7C0 bytes) and the pool counters 0x2593508 /
	// 0x2594D70 / 0x2594BB0 / 0x2592550
	uint32_t __cdecl im_768880()
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25AC6DC), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25ACD30), 0xB7C0);
		MEM<uint16_t>(0x25A6BB8) = 0;
		MEM<uint16_t>(0x25AC6E8) = 0;
		MEM<uint16_t>(0x25AC3A4) = 0;
		MEM<uint16_t>(0x25A4DA0) = 0;
		return 0;
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl im_768970(uint32_t a1)
	{
		im_768E20(a1);
		im_7689C0();
		x::Effect_AddTaskAndInitFromCtx(0x25ACCF8, 0x768AC0, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25A4E80, 0x768ED0, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x7689C0 (MAG_088_sub_7689C0): module data block set-up - [0x25AB8B0] = 0x154F898, the block [0x25AC3A8] = 0x25ACD40
	// (0x16C bytes cleared): +0x100 = the file arena cursor [0x25ACD38] (then + 0xA000), the model / data pointers +0x104..+0x160
	// (0x19ED018 default; +0x128 the moon play's layout 0x19F3A8C, +0x144 the halo model 0x19EF0BC, +0x148 the moon model
	// 0x19F1B5C), +0x164 / +0x166 = 0
	uint32_t __cdecl im_7689C0()
	{
		MEM<uint32_t>(0x25AB8B0) = 0x154F898;
		MEM<uint32_t>(0x25AC3A8) = 0x25ACD40;
		x::MAG_007_sub_8DCC00(0x25ACD40, 0x16C);
		const uint32_t m = MEM<uint32_t>(0x25AC3A8);
		const uint32_t cur = MEM<uint32_t>(0x25ACD38);
		U32(m, 0x100) = cur;
		MEM<uint32_t>(0x25ACD38) = cur + 0xA000;
		for (uint32_t o = 0x104; o <= 0x160; o += 4)
			U32(m, o) = 0x19ED018;
		U32(m, 0x128) = 0x19F3A8C;
		U32(m, 0x144) = 0x19EF0BC;
		U32(m, 0x148) = 0x19F1B5C;
		U16(m, 0x164) = 0;
		U16(m, 0x166) = 0;
		return 0; // void
	}

	// 0x768E20 (MAG_088_sub_768E20): director data set-up - clears the director block [DIR_PTR] (0x54 bytes), +8.. = the
	// target's (+0x2D) position (entity +0x1C / +0x20), +0.. = the caster's (+0x2C), facing +0x4A = the angle from the caster
	// to the target - 0x800 (12 bits), +0x4E = the target's size (+0x26)
	uint32_t __cdecl im_768E20(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t t = ENT(U8(a1, 0x2D)), c = ENT(U8(a1, 0x2C));
		U32(d, 8) = U32(t, 0x1C);
		U32(d, 0xC) = U32(t, 0x20);
		U32(d, 0) = U32(c, 0x1C);
		U32(d, 4) = U32(c, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		const uint32_t d2 = MEM<uint32_t>(DIR_PTR);
		U16(d2, 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		U16(d2, 0x4E) = U16(ENT(U8(a1, 0x2D)), 0x26);
		return 0; // void
	}

	// 0x8C5350 (MAG_008_sub_8C5350): DIRECTOR task - phase bookkeeping (0x8C5410), then its
	// 15-state table
	uint32_t __cdecl im_768ED0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[15] = { 0x768FE0, 0x769C70, 0x769CF0, 0x769D50, 0x769D70, 0x769D90,
			0x769E10, 0x769E40, 0x769ED0, 0x769EF0, 0x769F20, 0x769F80, 0x769FA0, 0x769FC0,
			0x769FE0 };
		im_768F90(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl im_768F90(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			im_76A000(a1);
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

	// 0x768FE0 (MAG_088_UploadAngeloTIM): director state 0 - queues the TIMs 0x16F3000 / 0x16FB420, arms the camera return
	// (0x50A730), spawns the camera task 0x769060 (queue 0x25A4F88, node 0x90); next state at director sync point 1 (a_73B640)
	uint32_t __cdecl im_768FE0(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x16F3000);
		x::Battle_QueueTIMUpload_GetEOF(0x16FB420);
		gx::BS_Camera_ArmReturnAfterEffect();
		x::Effect_AddTaskAndInitFromCtx(0x25A4F88, 0x769060, 0x90, a1);
		if (a_73B640(1) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// the camera task's script chains (0x769060's states): when the running camera script ends (a_73AE10 returns 1) the next
	// script starts from the director block (a_73AB20), next state
	static inline void im_cam_chain(uint32_t a1, uint32_t script)
	{
		if (a_73AE10() == 1)
		{
			a_73AB20(script, MEM<uint32_t>(DIR_PTR), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
	}

	// 0x762E60 (module 090 sub_762E60): master state - a_73AB00, next state.
	uint32_t __cdecl im_769120(uint32_t a1)
	{
		a_73AB00();
		t1_next_state(a1);
		return 0; // void
	}

	// 0x769150 (sub_769150): camera state 1 - steps the camera script; at director sync point 1 (a_73AAE0) starts 0x1555D60
	uint32_t __cdecl im_769150(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(1) != 0)
		{
			a_73AB20(0x1555D60, MEM<uint32_t>(DIR_PTR), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x769950 (sub_769950): camera state 2 - when the script ends starts 0x1555DA8
	uint32_t __cdecl im_769950(uint32_t a1) { im_cam_chain(a1, 0x1555DA8); return 0; } // void

	// 0x769980 (sub_769980): camera state 3 - when the script ends starts 0x1555DF0 or 0x1555E38 (random bit 0)
	uint32_t __cdecl im_769980(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t r = x::CrtRand();
			a_73AB20((r & 1) ? 0x1555DF0 : 0x1555E38, MEM<uint32_t>(DIR_PTR), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7699C0 (sub_7699C0): camera state 4 - when the script ends starts 0x1555E80
	uint32_t __cdecl im_7699C0(uint32_t a1) { im_cam_chain(a1, 0x1555E80); return 0; } // void

	// 0x7699F0 (sub_7699F0): camera state 5 - when the script ends starts 0x1555EC8
	uint32_t __cdecl im_7699F0(uint32_t a1) { im_cam_chain(a1, 0x1555EC8); return 0; } // void

	// 0x73B320 (module; Siren sub_73B320): camera state: steps the camera path (a_73AE10); once cut
	// >= 3 is requested, sets up camera path 0x1533CD0 against the script block itself (no +8),
	// advances the state
	uint32_t __cdecl im_769A20(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(3);
		if (r == 0) return 0;
		uint32_t blk = U32(0x154ECE0, 0);
		a_73AB20(0x1555F10, blk, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x769A50 (sub_769A50): camera state 7 - steps the camera script; at director sync point 4 starts 0x1555F58 from the
	// party's centre (0x769A90)
	uint32_t __cdecl im_769A50(uint32_t a1)
	{
		alignas(4) uint8_t pt[8] = {};
		a_73AE10();
		if (a_73AAE0(4) != 0)
		{
			im_769A90(P(pt));
			a_73AB20(0x1555F58, P(pt), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x769A90 (sub_769A90): the party's centre - the middle of the x / z ranges of the party entities 0..2 that are present
	// (record -0x20 bit 1; the engine's 0x73A720 does the same over the monsters 3..6), y 0, into a1 (s16 x3)
	uint32_t __cdecl im_769A90(uint32_t a1)
	{
		uint32_t count = 0;
		int16_t max_z = 0, min_z = 0, max_x = 0, min_x = 0;
		for (uint32_t e = 0x1D972E0; e < 0x1D974B4; e += 0x9C)
		{
			if (!(U8(e, -0x20) & 2))
				continue;
			if ((uint16_t)count == 0)
			{
				max_x = S16(e, -4);
				max_z = S16(e, 0);
				min_x = max_x;
				min_z = max_z;
			}
			else
			{
				const int16_t x = S16(e, -4);
				if (x < min_x) min_x = x;
				else if (x > max_x) max_x = x;
				const int16_t z = S16(e, 0);
				if (z < min_z) min_z = z;
				else if (z > max_z) max_z = z;
			}
			count++;
		}
		const int32_t cx = ((int32_t)max_x + (int32_t)min_x) / 2;
		U16(a1, 2) = 0;
		U16(a1, 0) = (uint16_t)cx;
		const int32_t cz = ((int32_t)max_z + (int32_t)min_z) / 2;
		U16(a1, 4) = (uint16_t)cz;
		return a1;
	}

	// 0x769B20 (sub_769B20): camera state 8 - when the script ends starts 0x1555FA0 from the party's centre
	uint32_t __cdecl im_769B20(uint32_t a1)
	{
		alignas(4) uint8_t pt[8] = {};
		if (a_73AE10() == 1)
		{
			im_769A90(P(pt));
			a_73AB20(0x1555FA0, P(pt), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x769B60 (sub_769B60): camera state 9 - steps the camera script; at director sync point 5 starts 0x1555FE8
	uint32_t __cdecl im_769B60(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(5) != 0)
		{
			a_73AB20(0x1555FE8, MEM<uint32_t>(DIR_PTR), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x769B90 (sub_769B90): camera state 10 - when the script ends starts 0x1556030
	uint32_t __cdecl im_769B90(uint32_t a1) { im_cam_chain(a1, 0x1556030); return 0; } // void

	// 0x769BC0 (sub_769BC0): camera state 11 - when the script ends starts 0x1556078
	uint32_t __cdecl im_769BC0(uint32_t a1) { im_cam_chain(a1, 0x1556078); return 0; } // void

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73AAE0(6), then advances the state index +0x29
	uint32_t __cdecl im_769C10(uint32_t a1)
	{
		const uint32_t r = a_73AAE0(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5710 (sub_7B5710): director state 1 - once cut >= 1: hides the monsters (0x7B5750), next state
	uint32_t __cdecl im_769C70(uint32_t a1)
	{
		if (a_73B7E0(1) == 0)
			return 0;
		a_73B9F0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x769D90 (sub_769D90): director state 5 - at director cue 3 (a_73B7E0): a_746A10, the stage dimming 0x769DD0, next state
	uint32_t __cdecl im_769D90(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			a_746A10();
			im_769DD0();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7B5750 (sub_7B5750): for the battle entities 0..2 (0x1D972C0, stride 0x9C, 3 entries) with flag
	// bit 1: saves the flag word to director +0x20[i] and sets flag bit 2
	uint32_t __cdecl im_769DD0(void)
	{
		uint32_t save = MEM<uint32_t>(0x154ECE0) + 0x20;
		for (uint32_t e = 0x1D972C0; e < 0x1D97494; e += 0x9C, save += 4)
		{
			const uint16_t f = U16(e, 0);
			if (f & 2)
			{
				U32(save, 0) = (uint32_t)f;
				U16(e, 0) = (uint16_t)(f | 4);
			}
		}
		return 0; // void
	}

	// 0x769E10 (sub_769E10): director state 6 - from the director's frame 0x15, at sync point 4 (a_73B640): next state
	uint32_t __cdecl im_769E10(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x15 && a_73B640(4) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x769E40 (sub_769E40): director state 7 - at director cue 4: a_746AE0, 0x769E90, next state
	uint32_t __cdecl im_769E40(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			a_746AE0();
			im_769E90();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7B5930 (sub_7B5930): restores flag bit 2 of the battle entities 0..2 (flag bit 1 set) from
	// the words saved by 0x7B5750
	uint32_t __cdecl im_769E90(void)
	{
		uint32_t save = MEM<uint32_t>(0x154ECE0) + 0x20;
		for (uint32_t e = 0x1D972C0; e < 0x1D97494; e += 0x9C, save += 4)
		{
			const uint16_t f = U16(e, 0);
			if (f & 2)
			{
				const uint32_t v = ((uint32_t)(uint8_t)(U8(save, 0) ^ (uint8_t)f) & 4) ^ (uint32_t)f;
				U16(e, 0) = (uint16_t)v;
			}
		}
		return 0; // void
	}

	// 0x769ED0 (sub_769ED0): director state 8 - from the director's frame 0x23: done flag +0x48 = 1, next state
	uint32_t __cdecl im_769ED0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) >= 0x23)
		{
			U16(d, 0x48) = 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x769EF0 (sub_769EF0): director state 9 - from the director's frame 0x34, at sync point 5: next state
	uint32_t __cdecl im_769EF0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x34 && a_73B640(5) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x73BA70 (module; Siren sub_73BA70): director state: once cut >= 5 runs, calls a_73BA90
	// (pass over the battle entities) and advances
	uint32_t __cdecl im_769F20(uint32_t a1)
	{
		uint32_t r = a_73B7E0(5);
		if (r == 0) return 0;
		a_73BA90();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x769F80 (sub_769F80): director state 11 - from the director's frame 0x14: next state
	uint32_t __cdecl im_769F80(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x14)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73B640(6), then advances the state index +0x29
	uint32_t __cdecl im_769FA0(uint32_t a1)
	{
		const uint32_t r = a_73B640(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x769FC0 (sub_769FC0): director state 13 - at director cue 6: finished, next state
	uint32_t __cdecl im_769FC0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x76A000 (MAG_088_SpawnAngeloCreature): the tasks of a new director phase ([DIR_PTR] +0x40): phase 1 = the two creatures
	// (0x7714D0: ANGELO 0x771DB0 with the model container 0x154F91C and RINOA'S DOUBLE 0x7718D0 with 0x155563C, animation 3);
	// phase 3 = the three moon models 0x771660 (queue 0x25A6BC0, node 0x9C, +0x98 = 0 / 1 / 2); phase 4 = the actor system
	// 0x76C700 (a_73C100: data [block +0x104], 8 / 0xF / 1), the moon play 0x76A180 (0x76A130: layout [block +0x128], 0x3C4)
	// and the stage-light task 0x771510 (queue 0x25A4F88, node 0x90)
	uint32_t __cdecl im_76A000(uint32_t a1)
	{
		const int32_t phase = S16(MEM<uint32_t>(DIR_PTR), 0x40);
		if (phase == 1)
		{
			im_7714D0(a1, 0x771DB0, 0x154F91C, 3);
			im_7714D0(a1, 0x7718D0, 0x155563C, 3);
		}
		else if (phase == 3)
		{
			for (uint32_t i = 0; i < 3; i++)
			{
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25A6BC0, 0x771660, 0x9C, a1);
				U16(t, 0x98) = (uint16_t)i;
			}
		}
		else if (phase == 4)
		{
			a_73C100(a1, 0x76C700, U32(MEM<uint32_t>(0x25AC3A8), 0x104), 8, 0xF, 1);
			im_76A130(a1, 0x76A180, U32(MEM<uint32_t>(0x25AC3A8), 0x128), 0x3C4, 1, 0);
			x::Effect_AddTaskAndInitFromCtx(0x25A4F88, 0x771510, 0x90, a1);
		}
		return 0; // void
	}

	// 0x76A130 (sub_76A130): spawns the play task a2 (queue 0x25AA9F0, node 0x474) from context a1: +0x74 = a3 (prim layout),
	// +0x78 = (s16) a4, +0x80 = a5, +0x82 = a6; returns the node
	uint32_t __cdecl im_76A130(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AA9F0, a2, 0x474, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x76A180 (sub_76A180): MOON PLAY task (node 0x474) - states {0x76A1F0 set-up + first frame, 0x76C6B0 play until cue 5 or
	// the end, ret}; +0x8E = (+0x8E - 2) & 0x7F (the callback's scroll phase); frame +0x24; released once finished with no child
	uint32_t __cdecl im_76A180(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x76A1F0;
		states[1] = 0x76C6B0;
		states[2] = 0x76C6F0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x8E) = (uint16_t)((uint8_t)(U8(node, 0x8E) - 2) & 0x7F);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x76A1F0 (sub_76A1F0): moon play state 0 - from frame 4: decodes the prim layout (+0x74, count +0x78) into +0x94, placed
	// at the party's centre (0x769A90) y 0x100, scale 0x1000^3, +0x88 = 0xF800, plays the first frame (0x76A250), next state
	uint32_t __cdecl im_76A1F0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 4)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		im_769A90(node + 0x1C);
		U16(node, 0x1E) = 0x100;
		S32(node, 0x58) = 0x1000;
		S32(node, 0x54) = 0x1000;
		S32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xF800;
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x76A380 (sub_76A380): prim-model player draw callback of the module's plays (layout a1, record a2, block a3) - as Angelo
	// Rush's 0x761CE0 (vertex frame lerped by MAG_017_sub_701390 into the block's +0x48 buffer, parent-rotated position + block
	// translation) with: record flag 0x100 = the scale as a diagonal 3x3 matrix (MatrixMultiply3x3) instead of scale3DMatrix;
	// flags 0x2000 when record flag 0x4000 (else 0x2030), alpha -> | 0xC0 with the colour; scale words 0x100 (block +0x56 = 1 on
	// object 0: +0x1A = block +0x4E (the scroll), +0x28 / +0x2A = 0x80); drawn by the module renderer 0x76A650 into OT base +0x44
	// (shift 2) at the frame arena cursor 0x1D8E054
	uint32_t __cdecl im_76A380(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position, +8 scale (3 x int32) / diagonal matrix (3x3 s16), +0x28 Mat4x3 object matrix (+0x3C translation)
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
		x::GTE_MatrixMultiply(blk, M);
		S32(M, 0x14) = add32(S32(M, 0x14), S32(blk, 0x14));
		S32(M, 0x18) = add32(S32(M, 0x18), S32(blk, 0x18));
		S32(M, 0x1C) = add32(S32(M, 0x1C), S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			if ((U32(rec, 4) & 0x100) != 0)
			{
				for (uint32_t o = 0; o < 0x12; o += 2) U16(S, o) = 0;
				U16(S, 0) = U16(rec, 0x18);
				U16(S, 8) = U16(rec, 0x1A);
				U16(S, 0x10) = U16(rec, 0x1C);
				x::sub_56C220(M, S); // MatrixMultiply3x3
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
		U32(hdr, 0x14) = (U32(rec, 4) & 0x4000) != 0 ? 0x2000 : 0x2030;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 8) = U32(rec, 0x20);   // colour
			U8(hdr, 0x14) = (uint8_t)(U8(hdr, 0x14) | 0xC0);
		}
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x24) = 0;
		if (U16(blk, 0x56) == 1 && U16(rec, 2) == 0)
		{
			U16(hdr, 0x1A) = U16(blk, 0x4E);
			U16(hdr, 0x28) = 0x80;
			U16(hdr, 0x2A) = 0x80;
		}
		else
		{
			U16(hdr, 0x1A) = 0;
			U16(hdr, 0x2A) = 0x100;
			U16(hdr, 0x28) = 0x100;
		}
		MEM<uint32_t>(0x1D8E054) = im_76A650(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 im_76AC30, FT4 im_76B0B0,
	// a_73D770, a_73D9C0, 0x73DCA0, im_76C060) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl im_76A650(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = im_76A780(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = im_76A9A0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = im_76AC30(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = im_76B0B0(ctx, a2, a3, cur);
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
			cur = im_76BB60(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return im_76C060(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl im_76A780(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl im_76A9A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl im_76AC30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl im_76B0B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl im_76BB60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl im_76C060(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x76C6B0 (sub_76C6B0): moon play state 1 - at director cue 5 finished; else plays (0x76A250), finished when it ends
	uint32_t __cdecl im_76C6B0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_73B7E0(5) == 0)
		{
			// 30 fps layer: see mag088_invincible_moon_held.inc
			FX_HELD(held_note_draw(HK_PLAY, node);)
			if (a_73C280(node) != 0)
				return 0; // void
		}
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl im_76C760(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			im_76D270(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl im_76C940(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25ACD3C);
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
	uint32_t __cdecl im_76CBC0(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25ACD3C);
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
	uint32_t __cdecl im_76D270(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25ACD3C, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			im_771410();
			im_76EAC0();
			// 30 fps layer: see mag088_invincible_moon_held.inc
			FX_HELD(held_note_system(sys);)
			im_76E980();
			im_76D300();
			sys = U32(0x25ACD3C, 0);
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
		U16(0x25ACCF0, 0) = (uint16_t)(U16(0x25ACCF0, 0) + c14);
		U16(0x25AC6D8, 0) = (uint16_t)(U16(0x25AC6D8, 0) + c16);
		return done;
	}

	// 0x7D7240 (copy of wb_87A440: 0x87A440 (copy of Curaga cg_87ED10: 0x87ED10 (sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440))): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl im_76D300(void)
	{
		uint32_t act = U32(U32(0x25ACD3C, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25ACD3C, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					im_76D360(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x7D72A0 (copy of wb_87A4A0: 0x87A4A0 (copy of Curaga cg_87ED70: 0x87ED70 (sub_87ED70; = Confuse c_860710))): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl im_76D360(uint32_t a1, uint32_t a2)
	{
		uint32_t ctx = MEM<uint32_t>(SCRATCH_SP) - 0x58;
		const bool def3a = U16(a2, 0x3A) != 0;
		const uint32_t model = U32(a1, 0x170);
		MEM<uint32_t>(SCRATCH_SP) = ctx;
		const uint32_t saved_ctx = ctx;
		U32(ctx, 0) = model;
		U32(ctx, 0x1C) = 0;
		if (!def3a)
			U32(ctx, 0x1C) = 0x30;
		const uint16_t col = U16(a1, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(a1, 0x16C);
			const uint32_t fl = U32(ctx, 0x1C);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x1C) = fl | 0xC0;
		}
		const int8_t copies = S8(a1, 0x1D8);
		if (copies == 1)
		{
			const uint32_t m = a1 + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
			const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			MEM<uint32_t>(PACKET_CURSOR) = a_7355E0(ctx, ot, 2, cursor);
		}
		else if (copies > 0)
		{
			const uint32_t m = a1 + 0xAC;
			uint32_t pos = a1 + 0x19E;
			int32_t i = 0;
			int32_t n;
			do
			{
				S32(a1, 0xC0) = S16(pos, -2);
				S32(a1, 0xC4) = S16(pos, 0);
				S32(a1, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
				const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
				const uint32_t r = a_7355E0(saved_ctx, ot, 2, cursor);
				n = S8(a1, 0x1D8);
				i++;
				pos += 8;
				MEM<uint32_t>(PACKET_CURSOR) = r;
			} while (i < n);
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x58;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x72BA00 (097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl im_76D5E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // edi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x4000000;          // tag: 4 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					x::GTE_AVSZ3();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x14;
						pk += 0x14;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// 0x72BC10 (097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl im_76D7F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // esi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x5000000;          // tag: 5 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					const uint32_t v3 = vbase + (uint32_t)U16(face, 0xA) * 4;
					x::GTE_LoadV0(v3);
					x::GTE_RTPS();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					x::GTE_ReadSXY2(pk + 8);
					x::GTE_AVSZ4();
					if (b1_out(S16(pk, 8), 0xA00)) clip |= 8;
					if (b1_out(S16(pk, 0xA), 0x6C0)) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x18;
						pk += 0x18;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// 0x72C370 (Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl im_76DF50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x10]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a2 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;                // esi = pkt + 0x10 (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x6000000;       // tag: 6 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x1C;
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x72C5A0 (Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl im_76E180(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x14]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x8000000;       // tag: 8 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x20))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, rec + 0x14);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 0x1C);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10), c3 = U32(rec, 0x14);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
							U32(pkt, 0x1C) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x72C860 (Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl im_76E440(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebp
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;          // tag: 9 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t uv2 = U32(rec, 8) >> 16;
			U32(pkt, 0x18) = uv1 + toff;
			U32(pkt, 0x24) = uv2 + toff;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x72CAC0 (Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl im_76E6A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebx
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;          // tag: 12 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t w23 = (toff << 16) + toff;
			uv1 += toff;
			w23 += U32(rec, 0x14);
			U32(pkt, 0x24) = w23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = w23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x14]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
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
							uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x8C9690 (copy of mb_827650 0x827650: copy of db_848D70 0x848D70: copy of cg_880390 0x880390: sub_880390; = Confuse c_861D30, copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl im_76E980(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25ACD3C), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25ACD3C);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						im_76EA40(act);   // (the original also pushes the bone entry, unused)
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
							im_76EA40(act);
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
	uint32_t __cdecl im_76EA40(uint32_t a1)
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
	uint32_t __cdecl im_76EAC0(void)
	{
		uint32_t act = U32(U32(0x25ACD3C, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				im_76FD90(act);
			else if (type == 1)
				im_76EB00(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl im_76EB00(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25ACD3C);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (im_76FC40(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25ACD3C);
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
	uint32_t __cdecl im_76FB30(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25ACD3C, 0);                  // actor state block
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

	// 0x828CB0 (copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl im_76FC40(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				im_76FCC0(a1, a2);
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
			im_76FCC0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			im_76FCC0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x828D30 (copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl im_76FCC0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return im_76FCF0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x828D60 (copy of db_84A0E0 0x84A0E0: copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl im_76FCF0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25AC6DC);
		int32_t idx = MEM<int16_t>(0x25A6BB8);
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
			MEM<uint16_t>(0x25A6BB8) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25A6BB8) = 0;
		return slot;
	}

	// 0x8CAAA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl im_76FD90(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x770740, a1);
		else if (mode == 4)
			callp(0x770890, a1);
		im_76FE10(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8CAB20 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl im_76FE10(uint32_t a1)
	{
		uint32_t sys = U32(0x25ACD3C, 0);
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
				im_76FE90(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			im_76FE90(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8CABA0 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl im_76FE90(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25ACD0C, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25ACD0C, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = im_770670(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25ACD3C, 0);
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
		im_771380(node, desc);
		U32(0x25ACD0C, 0) = U32(0x25ACD0C, 0) + 0x50;
		return 0; // void
	}

	// 0x8A8580 (copy of d_8BC590 0x8BC590: sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x46 records
	uint32_t __cdecl im_770670(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25ACD30);
		int32_t i = MEM<int16_t>(0x25AC6E8);
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
			if (i >= 0x45)
				i = 0;
			tries++;
			if (tries >= 0x46)
				break;
		}
		i++;
		if (i >= 0x45)
			MEM<uint16_t>(0x25AC6E8) = 0;
		else
			MEM<uint16_t>(0x25AC6E8) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl im_770890(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25ACD3C, 0);            // ecx
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
				uint32_t g = U32(0x25ACD3C, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25ACD3C, 0);
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

	// 0x82A3F0 (copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl im_771380(uint32_t a1, uint32_t a2)
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
			im_76FCF0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			im_76FCF0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			im_76FCF0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			im_76FCF0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x82A480 (copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via im_76FCF0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl im_771410(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25ACD3C);  // eax (re-read only after the calls)
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
					im_76FCF0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25ACD3C);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				im_76FCF0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25ACD3C);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl im_7714A0(uint32_t a1)
	{
		if (im_76D270(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7714D0 (sub_7714D0): spawns the creature task a2 (queue 0x25AC6C8, node 0x160) from context a1, bound to the model
	// container a3 with animation a4 (0x8DD0F0); returns the node
	uint32_t __cdecl im_7714D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AC6C8, a2, 0x160, a1);
		x::Effect_BindModelContainerSetAnim(t, a3, a4);
		return t;
	}

	// 0x7715C0 (sub_7715C0): stage-light task state 0 - +0x1C += 0x180 up to 0xA00 (then next state); the four stage draw
	// entries' light words 0x1D98992 + k * 0x2C = +0x1C
	uint32_t __cdecl im_7715C0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x180);
		if (S16(a1, 0x1C) >= 0xA00)
		{
			U16(a1, 0x1C) = 0xA00;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		const uint16_t v = U16(a1, 0x1C);
		for (uint32_t k = 0; k < 4; k++)
			MEM<uint16_t>(STAGE_LIGHT + k * 0x2C) = v;
		return 0; // void
	}

	// 0x771600 (sub_771600): stage-light task state 1 - next state after frame 0x1E
	uint32_t __cdecl im_771600(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x1E)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl im_771610(uint32_t a1)
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

	// 0x7211D0 (module 099 sub_7211D0): TASK - state machine {721320, 721360, nop}, then
	// im_7716C0 (model draw), task tail.
	uint32_t __cdecl im_771660(uint32_t a1)
	{
		const uint32_t states[3] = { 0x7717A0, 0x771880, 0x7718C0 };
		callp(states[S8(a1, 0x29)], a1);
		im_7716C0(a1);
		return task_end(a1);
	}

	// 0x7716C0 (sub_7716C0): moon model draw (unless hidden): identity rotation at +0x1C.., scaled by +0x30..; header 0x68
	// (model +0x4C, +8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0, windows 256 x 256) through the module
	// renderer 0x76A650 into OT base +0x44 (shift 2) at the module packet cursor 0x25A4D9C
	uint32_t __cdecl im_7716C0(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		x::MAG_022_sub_8DD770(M);
		S32(M, 0x14) = S16(a1, 0x1C);
		S32(M, 0x18) = S16(a1, 0x1E);
		S32(M, 0x1C) = S16(a1, 0x20);
		x::scale3DMatrix(M, a1 + 0x30);
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t hdr = x::Field_Alloc(0x68);
		U32(hdr, 0) = U32(a1, 0x4C);
		U32(hdr, 8) = U32(a1, 0x40);
		U32(hdr, 0xC) = (uint32_t)(int32_t)S16(a1, 0x50);
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x24) = 0;
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(a1, 0x52);
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		U32(hdr, 0x14) = 0xF0;
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_MOON, a1);)
		MEM<uint32_t>(PACKET_CURSOR) = im_76A650(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x7717A0 (sub_7717A0): moon model state 0 - by +0x98: 0 = the moon (model [block +0x148], depth 0x800), 1 / 2 = its two
	// glows (models 0x154EF74 / 0x154F404, depth 0x700 / 0x600, x +-0x60, y -+0x60, x drift +0x7C = 0x14 / -0x20); z = twice
	// [[0x154F918] +0xE0]; +0x9A = x * 16; scale 0x1000^3; next state
	uint32_t __cdecl im_7717A0(uint32_t a1)
	{
		switch (S16(a1, 0x98))
		{
		case 0:
			U16(a1, 0x52) = 0x800;
			U32(a1, 0x4C) = U32(MEM<uint32_t>(0x25AC3A8), 0x148);
			U16(a1, 0x20) = (uint16_t)(U16(MEM<uint32_t>(0x154F918), 0xE0) << 1);
			break;
		case 1:
			U32(a1, 0x4C) = 0x154EF74;
			U16(a1, 0x52) = 0x700;
			U16(a1, 0x1C) = 0x60;
			U16(a1, 0x1E) = 0xFFA0;
			U16(a1, 0x20) = (uint16_t)(U16(MEM<uint32_t>(0x154F918), 0xE0) << 1);
			U16(a1, 0x7C) = 0x14;
			break;
		case 2:
			U32(a1, 0x4C) = 0x154F404;
			U16(a1, 0x52) = 0x600;
			U16(a1, 0x1C) = 0xFFA0;
			U16(a1, 0x1E) = 0x60;
			U16(a1, 0x20) = (uint16_t)(U16(MEM<uint32_t>(0x154F918), 0xE0) << 1);
			U16(a1, 0x7C) = 0xFFE0;
			break;
		default:
			break;
		}
		U16(a1, 0x9A) = (uint16_t)(U16(a1, 0x1C) << 4);
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		S32(a1, 0x38) = 0x1000;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x771880 (sub_771880): moon model state 1 - x drift: +0x9A += +0x7C, x = +0x9A / 16; at director cue 4 hidden + finished
	uint32_t __cdecl im_771880(uint32_t a1)
	{
		U16(a1, 0x9A) = (uint16_t)(U16(a1, 0x9A) + U16(a1, 0x7C));
		U16(a1, 0x1C) = (uint16_t)(S16(a1, 0x9A) / 16);
		if (a_73B7E0(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7718D0 (sub_7718D0): RINOA'S DOUBLE creature task (node 0x160) - states {0x771B80 placed on Rinoa hidden, 0x771BD0
	// shown from frame 0xC (animation 0), 0x771C00, 0x771C30, 0x771C60, 0x771C90, 0x771CC0, 0x771CF0 (hidden at cue 4),
	// 0x771D20 (shown at cue 5), 0x771D50, 0x771D70 (hidden + finished), ret}; unless hidden drawn by the creature renderer
	// 0x7719A0 (a_746C10, state block 0x25AB8B8) into the module packet pool; frame +0x24; released once finished with no child
	uint32_t __cdecl im_7718D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[12];
		states[0] = 0x771B80;
		states[1] = 0x771BD0;
		states[2] = 0x771C00;
		states[3] = 0x771C30;
		states[4] = 0x771C60;
		states[5] = 0x771C90;
		states[6] = 0x771CC0;
		states[7] = 0x771CF0;
		states[8] = 0x771D20;
		states[9] = 0x771D50;
		states[10] = 0x771D70;
		states[11] = 0x771DA0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag088_invincible_moon_held.inc
			FX_HELD(held_note_draw(HK_DOUBLE, node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, CREATURE_ARG, MEM<uint32_t>(PACKET_CURSOR));
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x771B80 (sub_771B80): double state 0 - hidden, at Rinoa's position (the caster +0x2C), scale 0x1000^3, next state
	uint32_t __cdecl im_771B80(uint32_t a1)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 4);
		U32(a1, 0x4C) = U32(e, 0x1C);
		U32(a1, 0x11C) = 0x1000;
		U32(a1, 0x118) = 0x1000;
		U32(a1, 0x114) = 0x1000;
		U32(a1, 0x50) = U32(e, 0x20);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x771BD0 (sub_771BD0): double state 1 - from frame 0xC shown, animation 0, next state
	uint32_t __cdecl im_771BD0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xC)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
			x::au_re_Battle_ReadAnimation_7(a1, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771C00 (sub_771C00): double state 2 - when the animation ends (0x8DD220): velocity z 0, animation 1, next state
	uint32_t __cdecl im_771C00(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			U16(a1, 0x128) = 0;
			x::au_re_Battle_ReadAnimation_7(a1, 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771C30 (sub_771C30): double state 3 - animation step (0x8DD1C0); from animation frame +0x136 = 0x14 animation 2, next state
	uint32_t __cdecl im_771C30(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 0x14)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 2);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x730F50 (Boko shared; 097:730F50 098:728AB0): creature state - when the animation ended
	// (0x8DD220 == 1): anim 5, next state.
	uint32_t __cdecl im_771C60(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(node);
		if (r != 1)
			return r;
		x::au_re_Battle_ReadAnimation_7(node, 5);
		b3_next_state(node);
		return 0; // void
	}

	// 0x771C90 (sub_771C90): double state 5 - animation step; at director cue 3 animation 5, next state
	uint32_t __cdecl im_771C90(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (a_73B7E0(3) != 0)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 5);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771CC0 (sub_771CC0): double state 6 - animation step; from animation frame 6 animation 3, next state
	uint32_t __cdecl im_771CC0(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 6)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 3);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771CF0 (sub_771CF0): double state 7 - animation step (0x8DD220); at director cue 4 hidden, next state
	uint32_t __cdecl im_771CF0(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		if (a_73B7E0(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 4);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771D20 (sub_771D20): double state 8 - at director cue 5 shown, animation 4, next state
	uint32_t __cdecl im_771D20(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
			x::au_re_Battle_ReadAnimation_7(a1, 4);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771D50 (sub_771D50): double state 9 - when the animation ends next state
	uint32_t __cdecl im_771D50(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x771D70 (sub_771D70): double state 10 - when the animation ends hidden + finished, next state
	uint32_t __cdecl im_771D70(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x771DB0 (sub_771DB0): ANGELO creature task (node 0x160) - 17 states {0x772AB0 (binds Rinoa's animation set, her chain
	// transformation 1, placed behind her), 0x772B40 (her chain transformation 2 from frame 0xB), 0x772B80, 0x772BE0 (the
	// howl play 0x772C50), 0x772D40 (the halo 0x772D90), 0x773010, 0x773040 (the flash 0x773090), 0x7732F0, 0x773320,
	// 0x7733B0, 0x773520, 0x773560 (hidden at cue 4), 0x773590 (shown at cue 5: her chain transformation 4), 0x773600, 0x773630,
	// 0x773670 (hidden + finished), ret}; position += velocity (+0x124 / +0x126 / +0x128); unless hidden drawn by the creature
	// renderer 0x7719A0 (a_746C10, state block 0x25AB8B8) and, on the frames that put a foot down (animation 1 frames 0xC..0x11,
	// 5 frames 2..7: 0x771F50; 3 frames 2..5: 0x772880), the dust rings; frame +0x24; root matrix (+0x70) published to
	// 0x1D99AB0, position to +0x84/+0x88/+0x8C (s32); released once finished with no child alive
	uint32_t __cdecl im_771DB0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[17];
		states[0] = 0x772AB0;
		states[1] = 0x772B40;
		states[2] = 0x772B80;
		states[3] = 0x772BE0;
		states[4] = 0x772D40;
		states[5] = 0x773010;
		states[6] = 0x773040;
		states[7] = 0x7732F0;
		states[8] = 0x773320;
		states[9] = 0x7733B0;
		states[10] = 0x773520;
		states[11] = 0x773560;
		states[12] = 0x773590;
		states[13] = 0x773600;
		states[14] = 0x773630;
		states[15] = 0x773670;
		states[16] = 0x7736B0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + U16(node, 0x128));
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + U16(node, 0x124));
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + U16(node, 0x126));
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag088_invincible_moon_held.inc
			FX_HELD(held_note_draw(HK_ANGELO, node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, CREATURE_ARG, MEM<uint32_t>(PACKET_CURSOR));
			if ((U8(node, 0x26) & 4) == 0)
			{
				const uint8_t anim = U8(node, 0x9C), fr = U8(node, 0xA2);
				if (anim == 1)
				{
					if (fr >= 0xC && fr <= 0x11)
						im_771F50(node);
				}
				else if (anim == 3)
				{
					if (fr >= 2 && fr <= 5)
						im_772880(node);
				}
				else if (anim == 5)
				{
					if (fr >= 2 && fr <= 7)
						im_771F50(node);
				}
			}
		}
		const int32_t py = S16(node, 0x4E);
		const int32_t px = S16(node, 0x4C);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 8 * 4); // rep movsd
		const int32_t pz = S16(node, 0x50);
		S32(node, 0x88) = py;
		S32(node, 0x84) = px;
		S32(node, 0x8C) = pz;
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// Angelo's dust rings (0x771F50 / 0x772880): n ring particles of task f (queue 0x25A4F88, node 0x90) at his bone point 0xF1
	// on the ground, pushed out by a random yaw and 0x40..0x13F, velocity +0x78.. = (0, 0, 0x200 + rand & mask) turned by the
	// same yaw, rise -(0..0x5F) per tick +0x82
	static inline void im_dust_rings(uint32_t a1, int n, uint32_t f, uint32_t mask)
	{
		alignas(4) uint8_t m[0x20];
		alignas(4) uint8_t v[8] = {};
		const uint32_t M = P(m), Vp = P(v);
		for (int i = 0; i < n; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25A4F88, f, 0x90, a1);
			x::GetEffectSpawnPosition(a1 + 0x30, 0xF1, 0, t + 0x1C);
			U16(t, 0x1E) = 0;
			const uint32_t r1 = x::CrtRand() & 0xFFF;
			x::MAG_022_sub_8DD770(M);
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)r1);
			U16(Vp, 0) = 0;
			U16(Vp, 2) = 0;
			U16(Vp, 4) = (uint16_t)((x::CrtRand() & 0xFF) + 0x40);
			x::matrixMultiplyVector(M, Vp, Vp);
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + U16(Vp, 0));
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + U16(Vp, 4));
			const int32_t r4 = (int32_t)x::CrtRand();
			U16(t, 0x78) = 0;
			U16(t, 0x7A) = 0;
			U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
			U16(t, 0x7C) = (uint16_t)((x::CrtRand() & mask) + 0x200);
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		}
	}

	// 0x771F50 (sub_771F50): four dust rings 0x772060 (speed 0x200..0x5FF)
	uint32_t __cdecl im_771F50(uint32_t a1) { im_dust_rings(a1, 4, 0x772060, 0x3FF); return 0; } // void

	// 0x767290 (module 090 sub_767290): ring particle task (states 0x767410/0x767430) drawn by
	// im_7720C0; ends when finished and no child alive.
	uint32_t __cdecl im_772060(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x772790;
		states[1] = 0x7727B0;
		states[2] = 0x772870;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		im_7720C0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl im_7720C0(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag088_invincible_moon_held.inc
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
		const uint32_t cursor = MEM<uint32_t>(0x25A4D9C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x25A4D9C) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x7618E0 (sub_7618E0): ring particle state 0 - prim sequence 0x15430E8, colour +0x56 = 0xFFC0, +0x52 = 7, next state
	uint32_t __cdecl im_772790(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x154ECEC;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x761900 (sub_761900): ring particle state 1 - sequence step (0x761990 = a_743C20; at its end hidden + finished, next
	// state); y velocity +0x7A += +0x82, x / z velocity damped by 1/8, position += velocity / 16
	uint32_t __cdecl im_7727B0(uint32_t a1)
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

	// 0x772880 (sub_772880): three dust rings 0x772990 (speed 0x200..0x9FF)
	uint32_t __cdecl im_772880(uint32_t a1) { im_dust_rings(a1, 3, 0x772990, 0x7FF); return 0; } // void

	// 0x767290 (module 090 sub_767290): ring particle task (states 0x767410/0x767430) drawn by
	// im_7720C0; ends when finished and no child alive.
	uint32_t __cdecl im_772990(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7729F0;
		states[1] = 0x772A10;
		states[2] = 0x772AA0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		im_7720C0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7618E0 (sub_7618E0): ring particle state 0 - prim sequence 0x15430E8, colour +0x56 = 0xFFC0, +0x52 = 7, next state
	uint32_t __cdecl im_7729F0(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x154EE30;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x761900 (sub_761900): ring particle state 1 - sequence step (0x761990 = a_743C20; at its end hidden + finished, next
	// state); y velocity +0x7A += +0x82, x / z velocity damped by 1/8, position += velocity / 16
	uint32_t __cdecl im_772A10(uint32_t a1)
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

	// 0x772AB0 (sub_772AB0): Angelo state 0 - the director's +0x18 = this node; binds Rinoa's animation set (com data
	// 0x15560C0, 0x20-byte block 0x25ACD10) to the caster (+0x2C, BS_BindEntityModelFromComData 0x5022C0), queues her chain
	// transformation 1 (0x505C00); placed at her position + z 0x1200, animation 0, scale 0x1000^3, velocity z -0x100, next state
	uint32_t __cdecl im_772AB0(uint32_t a1)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		U32(MEM<uint32_t>(DIR_PTR), 0x18) = a1;
		gx::BindEntityModelFromComData(e, 0x25ACD10, 0x15560C0);
		gx::QueueChainTransformation(e, 1);
		U32(a1, 0x4C) = U32(e, 0x1C);
		U32(a1, 0x50) = U32(e, 0x20);
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x1200);
		x::au_re_Battle_ReadAnimation_7(a1, 0);
		U32(a1, 0x11C) = 0x1000;
		U32(a1, 0x118) = 0x1000;
		U32(a1, 0x114) = 0x1000;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x128) = 0xFF00;
		return 0; // void
	}

	// 0x772B40 (sub_772B40): Angelo state 1 - animation step (0x8DD220); from frame 0xB Rinoa's chain transformation 2, next state
	uint32_t __cdecl im_772B40(uint32_t a1)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		x::au_re_Battle_ReadAnimation_8(a1);
		if (S16(a1, 0x24) >= 0xB)
		{
			gx::QueueChainTransformation(e, 2);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x772B80 (sub_772B80): Angelo state 2 - when the animation ends: velocity z 0, animation 1, at Rinoa's position, next state
	uint32_t __cdecl im_772B80(uint32_t a1)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			U16(a1, 0x128) = 0;
			x::au_re_Battle_ReadAnimation_7(a1, 1);
			U32(a1, 0x4C) = U32(e, 0x1C);
			U32(a1, 0x50) = U32(e, 0x20);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x772BE0 (sub_772BE0): Angelo state 3 - sound 0x154ECE4 on animation frame 8; when the animation ends: animation 2, the
	// howl play 0x772C50 (0x76A130: layout 0x16F1F8C / 0xB0, +0x458 = his model node + 0x30), next state
	uint32_t __cdecl im_772BE0(uint32_t a1)
	{
		if (S16(a1, 0x136) == 8)
			x::BdPlaySE(0x154ECE4, 0, 0x80);
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 2);
			const uint32_t t = im_76A130(a1, 0x772C50, 0x16F1F8C, 0xB0, 0, 0);
			U32(t, 0x458) = a1 + 0x30;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x772CB0 (sub_772CB0): howl play state 0 (task 0x772C50) - decodes the prim layout (+0x74, count +0x78) into +0x94, placed
	// at Angelo's bone point 0xF1 (+0x458) on the ground, scale 0x1000^3, plays the first frame (0x76A250), next state
	uint32_t __cdecl im_772CB0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = U32(node, 0x458);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(mdl, 0xF1, 0, node + 0x1C);
		U16(node, 0x1E) = 0;
		S32(node, 0x58) = 0x1000;
		S32(node, 0x54) = 0x1000;
		S32(node, 0x50) = 0x1000;
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl im_772D10(uint32_t a1)
	{
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x772D40 (sub_772D40): Angelo state 4 - animation step; from animation frame 0x14: animation 3, the halo 0x772D90 (queue
	// 0x25A6BC0, node 0x9C, +0x8C = his model node + 0x30), next state
	uint32_t __cdecl im_772D40(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 0x14)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 3);
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25A6BC0, 0x772D90, 0x9C, a1);
			U32(t, 0x8C) = a1 + 0x30;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x772D90 (sub_772D90): HALO task (node 0x9C) - states {0x772F50 set-up, 0x772FB0 fade-in (+0x50 down to 0), 0x772FE0 (hidden
	// + finished at cue 3), ret}; spin +0x46 += 0x100; height +0x34 = - the y of Angelo's bone point 0x1D; drawn by 0x772E30;
	// frame +0x24; released once finished with no child alive
	uint32_t __cdecl im_772D90(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x772F50;
		states[1] = 0x772FB0;
		states[2] = 0x772FE0;
		states[3] = 0x773000; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		alignas(4) uint8_t pt[8] = {};
		U16(node, 0x46) = (uint16_t)((U16(node, 0x46) + 0x100) & 0xFFF);
		x::GetEffectSpawnPosition(U32(node, 0x8C), 0x1D, 0, P(pt));
		S32(node, 0x34) = -(int32_t)S16(P(pt), 2);
		im_772E30(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x8A0AA0 (sub_8A0AA0): draws a chunk piece (unless hidden, +0x26 bit 2): matrix from the angles
	// +0x46 (y) / +0x44 (x) / +0x48 (z) (0x8DD770 / 0x8DD8A0 / 0x8DD7E0 / 0x8DD960) at the position
	// +0x1C..+0x20, scaled by +0x30, composed with the camera 0x1D97778, into the GTE; a 0x68-byte
	// render header (model +0x4C, +0x40, +0x50, +0x52, flags 0xF0, unit scales 0x100) drawn by the
	// module's prim-model renderer 0x8A0BC0 into the effect OT from the module packet cursor
	uint32_t __cdecl im_772E30(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_HALO, node);)
		// local matrix [esp+4] (3x3 s16 + pad + translation at +0x14); written by 0x8DD770 and the
		// translation stores (pad bytes +0x12/+0x13 never written, zeroed here, never read)
		uint32_t mbuf[8] = {};
		const uint32_t m = P(mbuf);
		x::MAG_022_sub_8DD770(m);
		x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0x46));
		x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0x48));
		S32(m, 0x14) = S16(node, 0x1C);
		S32(m, 0x18) = S16(node, 0x1E);
		S32(m, 0x1C) = S16(node, 0x20);
		x::scale3DMatrix(m, node + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		x::GTE_SetRotMatrix_W(m);
		x::GTE_SetTransVector_W(m);
		const uint32_t h = x::Field_Alloc(0x68);
		U32(h, 0) = U32(node, 0x4C);
		U32(h, 8) = U32(node, 0x40);
		S32(h, 0xC) = S16(node, 0x50);
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		S32(h, 0x10) = S16(node, 0x52);
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U32(h, 0x14) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = im_76A650(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x772F50 (sub_772F50): halo state 0 - model [block +0x144], scale 0x800 / 0x1000 / 0x800, depth +0x52 = -0x200, fade +0x50
	// = 0x1000, at Angelo's bone point 0xF1 on the ground, next state
	uint32_t __cdecl im_772F50(uint32_t a1)
	{
		const uint32_t mdlp = U32(MEM<uint32_t>(0x25AC3A8), 0x144);
		S32(a1, 0x30) = 0x800;
		S32(a1, 0x38) = 0x800;
		U32(a1, 0x4C) = mdlp;
		S32(a1, 0x34) = 0x1000;
		U16(a1, 0x52) = 0xFE00;
		U16(a1, 0x50) = 0x1000;
		x::GetEffectSpawnPosition(U32(a1, 0x8C), 0xF1, 0, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1E) = 0;
		return 0; // void
	}

	// 0x772FB0 (sub_772FB0): halo state 1 - fade +0x50 -= 0x400; at 0 (or below) 0, next state
	uint32_t __cdecl im_772FB0(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFC00);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x772FE0 (sub_772FE0): halo state 2 - at director cue 3 hidden + finished, next state
	uint32_t __cdecl im_772FE0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773010 (sub_773010): Angelo state 5 - when the animation ends animation 8, next state
	uint32_t __cdecl im_773010(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 8);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773040 (sub_773040): Angelo state 6 - animation step; from animation frame 6: countdown +0x134 = 9, the screen flash
	// 0x773090 (queue 0x25AB728, node 0x40: colour script 0x1557CAC, delay 0), next state
	uint32_t __cdecl im_773040(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 6)
		{
			U16(a1, 0x134) = 9;
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AB728, 0x773090, 0x40, a1);
			U32(t, 0x34) = 0x1557CAC;
			U16(t, 0x38) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773090 (sub_773090): SCREEN FLASH task (node 0x40; the engine's 0x8DDEC0 with the module's pause word 0x25ACD34) -
	// states {0x773100 delay, 0x773130 script step + draw, ret}; its tick counter +0x3C counts unless paused; frame +0x24;
	// released once finished with no child alive
	uint32_t __cdecl im_773090(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x773100;
		states[1] = 0x773130;
		states[2] = 0x7732E0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if (MEM<uint32_t>(FLASH_PAUSE) == 0)
			U16(node, 0x3C) = (uint16_t)(U16(node, 0x3C) + 1);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x773100 (sub_773100): flash state 0 - unless paused the delay +0x38 counts down; at 0 next state
	uint32_t __cdecl im_773100(uint32_t a1)
	{
		if (MEM<uint32_t>(FLASH_PAUSE) != 0)
			return 0; // void
		U16(a1, 0x38) = (uint16_t)(U16(a1, 0x38) - 1);
		if (S16(a1, 0x38) <= 0)
		{
			U16(a1, 0x38) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773130 (sub_773130): flash state 1 - the colour script step (entry +0x34[+0x3A]: 0xFF.. = finished; 0xFE nn .. = wait
	// until the tick counter +0x3C reaches nn, then the next entry; else the entry is the colour +0x30, index + 1; no step while
	// paused), then the draw: a draw-mode packet (semi-transparent mode 2), four screen quads (TILE 0x2A, colour +0x30, 320 x 240
	// in 160 x 120 quarters) and a second draw-mode packet into OT base +0x1C at the module packet cursor
	uint32_t __cdecl im_773130(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t idx = U16(node, 0x3A);
		const uint32_t script = U32(node, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
			return 0; // void
		}
		if (op == 0xFE)
		{
			if (S16(node, 0x3C) >= (int16_t)(uint16_t)b2 && MEM<uint32_t>(FLASH_PAUSE) == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(node, 0x3A) = ni;
				U32(node, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(node, 0x30) = ent;
			if (MEM<uint32_t>(FLASH_PAUSE) == 0)
				U16(node, 0x3A) = (uint16_t)(idx + 1);
		}
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(held_note_draw(HK_FLASH, node);)
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		uint32_t pkt = MEM<uint32_t>(PACKET_CURSOR);
		x::sub_45BFC0(pkt, 0, 0, x::sub_45C690(0, 2, 0x280, 0) & 0xFFFF, 0);
		x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
		pkt += 0xC;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = U32(node, 0x30);
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++)
				U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		x::sub_45BFC0(pkt, 0, 0, x::sub_45C690(0, 2, 0x280, 0) & 0xFFFF, 0);
		x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
		MEM<uint32_t>(PACKET_CURSOR) = pkt + 0xC;
		return 0; // void
	}

	// 0x7179C0 (module 100 sub_7179C0): creature state: steps the animation (sub_8DD1C0), counts
	// timer +0x134 down, next state at 0
	uint32_t __cdecl im_7732F0(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) - 1);
		if (S16(a1, 0x134) <= 0)
			b103_next_state(a1);
		return 0; // void
	}

	// 0x773320 (sub_773320): Angelo state 8 - animation step; at director sync point 3 (a_73B640): a_73BB60(0), next state; else
	// a_73BB60(1)
	uint32_t __cdecl im_773320(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (a_73B640(3) != 0)
		{
			a_73BB60(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		else
			a_73BB60(1);
		return 0; // void
	}

	// 0x7733B0 (sub_7733B0): Angelo state 9 - animation step; at director cue 3: sound 0x154ECE8, animation 8, glow level +0x13A
	// = 0x800 (0x773400), next state
	uint32_t __cdecl im_7733B0(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (a_73B7E0(3) != 0)
		{
			x::BdPlaySE(0x154ECE8, 1, 0x80);
			x::au_re_Battle_ReadAnimation_7(a1, 8);
			U16(a1, 0x13A) = 0x800;
			im_773400(a1, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8CD310 (copy of t_7678B0 0x7678B0: module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl im_773400(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x773520 (sub_773520): Angelo state 10 - animation step; from animation frame 6: animation 4, glow level 0x800, next state
	uint32_t __cdecl im_773520(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 6)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 4);
			U16(a1, 0x13A) = 0x800;
			im_773400(a1, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773560 (sub_773560): Angelo state 11 - animation step (0x8DD220); at director cue 4 hidden, next state
	uint32_t __cdecl im_773560(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		if (a_73B7E0(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 4);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773590 (sub_773590): Angelo state 12 - at director cue 5: shown, glow level 0, animation 5, Rinoa's chain transformation
	// 4, at her position, next state
	uint32_t __cdecl im_773590(uint32_t a1)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		if (a_73B7E0(5) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
			U16(a1, 0x13A) = 0;
			im_773400(a1, 0);
			x::au_re_Battle_ReadAnimation_7(a1, 5);
			gx::QueueChainTransformation(e, 4);
			U32(a1, 0x4C) = U32(e, 0x1C);
			U32(a1, 0x50) = U32(e, 0x20);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773600 (sub_773600): Angelo state 13 - when the animation ends animation 6, next state
	uint32_t __cdecl im_773600(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 6);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x773630 (sub_773630): Angelo state 14 - when the animation ends: animation 7, next state, velocity z 0x100
	uint32_t __cdecl im_773630(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 7);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			U16(a1, 0x128) = 0x100;
		}
		return 0; // void
	}

	// 0x773670 (sub_773670): Angelo state 15 - animation step; from animation frame 0x12: animation 3, hidden + finished, next state
	uint32_t __cdecl im_773670(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 0x12)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 3);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// ---- generated (desc88.py / cls88.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x768820, (void *)a_73A0D0, "088 MAG_088_sub_768820" },
		{ 0x768830, (void *)a_73A0D0, "088 MAG_088_sub_768830" },
		{ 0x7688C0, (void *)a_73A170, "088 MAG_088_sub_7688C0" },
		{ 0x7688F0, (void *)a_73A1A0, "088 MAG_088_sub_7688F0" },
		{ 0x768AC0, (void *)a_73A380, "088 MAG_088_sub_768AC0" },
		{ 0x768B20, (void *)a_73A3E0, "088 sub_768B20" },
		{ 0x768CC0, (void *)a_73A580, "088 sub_768CC0" },
		{ 0x768CF0, (void *)a_73A5B0, "088 sub_768CF0" },
		{ 0x768D20, (void *)a_73A5E0, "088 sub_768D20" },
		{ 0x768D30, (void *)a_73A5E0, "088 sub_768D30" },
		{ 0x768D40, (void *)a_73A5E0, "088 sub_768D40" },
		{ 0x768D50, (void *)a_73A5E0, "088 sub_768D50" },
		{ 0x768D60, (void *)a_73A620, "088 sub_768D60" },
		{ 0x768DA0, (void *)a_73A660, "088 sub_768DA0" },
		{ 0x768E10, (void *)a_73A6D0, "088 nullsub_1264" },
		{ 0x769030, (void *)a_73B640, "088 sub_769030" },
		{ 0x769060, (void *)a_732A00, "088 sub_769060" },
		{ 0x769130, (void *)a_73AB00, "088 sub_769130" },
		{ 0x769180, (void *)a_73AAE0, "088 sub_769180" },
		{ 0x7691A0, (void *)a_73AE10, "088 sub_7691A0" },
		{ 0x7694F0, (void *)a_73B160, "088 sub_7694F0" },
		{ 0x769660, (void *)a_73AB20, "088 sub_769660" },
		{ 0x769BF0, (void *)a_73B2D0, "088 sub_769BF0" },
		{ 0x769C30, (void *)a_73B750, "088 sub_769C30" },
		{ 0x769C50, (void *)a_73A6D0, "088 nullsub_1265" },
		{ 0x769C60, (void *)a_73A6D0, "088 nullsub_1266" },
		{ 0x769C90, (void *)a_73B7E0, "088 sub_769C90" },
		{ 0x769CB0, (void *)a_73B9F0, "088 sub_769CB0" },
		{ 0x769CF0, (void *)a_73A930, "088 sub_769CF0" },
		{ 0x769D10, (void *)a_73A950, "088 sub_769D10" },
		{ 0x769D50, (void *)a_733AC0, "088 sub_769D50" },
		{ 0x769D70, (void *)a_73B8A0, "088 sub_769D70" },
		{ 0x769DB0, (void *)a_746A10, "088 sub_769DB0" },
		{ 0x769E60, (void *)a_746AE0, "088 sub_769E60" },
		{ 0x769F40, (void *)a_73BA90, "088 sub_769F40" },
		{ 0x769FE0, (void *)a_73A6D0, "088 nullsub_1267" },
		{ 0x769FF0, (void *)a_73A6D0, "088 nullsub_1268" },
		{ 0x76A0E0, (void *)a_73C100, "088 sub_76A0E0" },
		{ 0x76A250, (void *)a_73C280, "088 sub_76A250" },
		{ 0x76B630, (void *)a_73D770, "088 sub_76B630" },
		{ 0x76B880, (void *)a_73D9C0, "088 sub_76B880" },
		{ 0x76C6F0, (void *)a_73A6D0, "088 nullsub_1269" },
		{ 0x76C700, (void *)a_73A380, "088 sub_76C700" },
		{ 0x76C790, (void *)a_73F990, "088 sub_76C790" },
		{ 0x76CA20, (void *)a_73FC20, "088 sub_76CA20" },
		{ 0x76CC90, (void *)a_73FE90, "088 sub_76CC90" },
		{ 0x76CDB0, (void *)a_73FFB0, "088 sub_76CDB0" },
		{ 0x76D010, (void *)a_740210, "088 sub_76D010" },
		{ 0x76D4A0, (void *)a_7355E0, "088 sub_76D4A0" },
		{ 0x76DA70, (void *)a_735BB0, "088 sub_76DA70" },
		{ 0x76DCB0, (void *)a_735DF0, "088 sub_76DCB0" },
		{ 0x76EB90, (void *)a_740910, "088 sub_76EB90" },
		{ 0x76F1F0, (void *)a_740F70, "088 sub_76F1F0" },
		{ 0x76F240, (void *)a_740FC0, "088 sub_76F240" },
		{ 0x76F2D0, (void *)a_741050, "088 sub_76F2D0" },
		{ 0x76F3A0, (void *)a_741120, "088 sub_76F3A0" },
		{ 0x76FDE0, (void *)a_741B60, "088 sub_76FDE0" },
		{ 0x770510, (void *)a_742290, "088 MAG_088_RandRange" },
		{ 0x770540, (void *)a_7422C0, "088 sub_770540" },
		{ 0x770580, (void *)a_742300, "088 sub_770580" },
		{ 0x7705D0, (void *)a_742350, "088 sub_7705D0" },
		{ 0x770620, (void *)a_7423A0, "088 MAG_088_RandAngleJitter" },
		{ 0x770730, (void *)a_7424B0, "088 sub_770730" },
		{ 0x770740, (void *)a_7424C0, "088 sub_770740" },
		{ 0x770F20, (void *)a_742CA0, "088 sub_770F20" },
		{ 0x770F50, (void *)a_742CD0, "088 sub_770F50" },
		{ 0x770F80, (void *)a_742D00, "088 sub_770F80" },
		{ 0x771130, (void *)a_742EB0, "088 sub_771130" },
		{ 0x771270, (void *)a_742FF0, "088 sub_771270" },
		{ 0x7714C0, (void *)a_73A6D0, "088 nullsub_1270" },
		{ 0x771510, (void *)a_73F110, "088 sub_771510" },
		{ 0x771580, (void *)a_7335D0, "088 sub_771580" },
		{ 0x771650, (void *)a_73A6D0, "088 nullsub_1271" },
		{ 0x7718C0, (void *)a_73A6D0, "088 nullsub_1272" },
		{ 0x7719A0, (void *)a_746C10, "088 sub_7719A0" },
		{ 0x771DA0, (void *)a_73A6D0, "088 nullsub_1273" },
		{ 0x7721E0, (void *)a_7435E0, "088 InitEffectSequenceFromData_c6" },
		{ 0x772320, (void *)a_743720, "088 sub_772320" },
		{ 0x772840, (void *)a_743C20, "088 sub_772840" },
		{ 0x772870, (void *)a_73A6D0, "088 nullsub_1274" },
		{ 0x772AA0, (void *)a_73A6D0, "088 nullsub_1275" },
		{ 0x772C50, (void *)a_73A380, "088 sub_772C50" },
		{ 0x772D30, (void *)a_73A6D0, "088 nullsub_1276" },
		{ 0x773000, (void *)a_73A6D0, "088 nullsub_1277" },
		{ 0x7732E0, (void *)a_73A6D0, "088 nullsub_1278" },
		{ 0x773360, (void *)a_73BB60, "088 sub_773360" },
		{ 0x773420, (void *)a_733950, "088 sub_773420" },
		{ 0x7736B0, (void *)a_73A6D0, "088 nullsub_1279" },
		{ 0x7736C0, (void *)a_747440, "088 MAG_088_sub_7736C0" },
		{ 0x773730, (void *)a_7474B0, "088 MAG_088_sub_773730" },
		{ 0x773750, (void *)a_7474D0, "088 MAG_088_sub_773750" },
		{ 0x773770, (void *)a_73A6D0, "088 nullsub_1280" },
		{ 0x773780, (void *)a_747500, "088 MAG_088_sub_773780" },
		{ 0x7737C0, (void *)a_73A0D0, "088 MAG_088_sub_7737C0" },
		{ 0x7737D0, (void *)a_747550, "088 MAG_088_sub_7737D0" },
		{ 0x7737E0, (void *)a_747560, "088 MAG_088_sub_7737E0" },
		{ 0x773810, (void *)a_747590, "088 MAG_088_sub_773810" },
		{ 0x773820, (void *)a_7475A0, "088 MAG_088_sub_773820" },
		{ 0x773840, (void *)a_73A6D0, "088 nullsub_1263" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x768690, (void *)im_768690, "088 im_768690" },
		{ 0x768840, (void *)im_768840, "088 im_768840" },
		{ 0x768880, (void *)im_768880, "088 im_768880" },
		{ 0x768970, (void *)im_768970, "088 im_768970" },
		{ 0x7689C0, (void *)im_7689C0, "088 im_7689C0" },
		{ 0x768E20, (void *)im_768E20, "088 im_768E20" },
		{ 0x768ED0, (void *)im_768ED0, "088 im_768ED0" },
		{ 0x768F90, (void *)im_768F90, "088 im_768F90" },
		{ 0x768FE0, (void *)im_768FE0, "088 im_768FE0" },
		{ 0x769120, (void *)im_769120, "088 im_769120" },
		{ 0x769150, (void *)im_769150, "088 im_769150" },
		{ 0x769950, (void *)im_769950, "088 im_769950" },
		{ 0x769980, (void *)im_769980, "088 im_769980" },
		{ 0x7699C0, (void *)im_7699C0, "088 im_7699C0" },
		{ 0x7699F0, (void *)im_7699F0, "088 im_7699F0" },
		{ 0x769A20, (void *)im_769A20, "088 im_769A20" },
		{ 0x769A50, (void *)im_769A50, "088 im_769A50" },
		{ 0x769A90, (void *)im_769A90, "088 im_769A90" },
		{ 0x769B20, (void *)im_769B20, "088 im_769B20" },
		{ 0x769B60, (void *)im_769B60, "088 im_769B60" },
		{ 0x769B90, (void *)im_769B90, "088 im_769B90" },
		{ 0x769BC0, (void *)im_769BC0, "088 im_769BC0" },
		{ 0x769C10, (void *)im_769C10, "088 im_769C10" },
		{ 0x769C70, (void *)im_769C70, "088 im_769C70" },
		{ 0x769D90, (void *)im_769D90, "088 im_769D90" },
		{ 0x769DD0, (void *)im_769DD0, "088 im_769DD0" },
		{ 0x769E10, (void *)im_769E10, "088 im_769E10" },
		{ 0x769E40, (void *)im_769E40, "088 im_769E40" },
		{ 0x769E90, (void *)im_769E90, "088 im_769E90" },
		{ 0x769ED0, (void *)im_769ED0, "088 im_769ED0" },
		{ 0x769EF0, (void *)im_769EF0, "088 im_769EF0" },
		{ 0x769F20, (void *)im_769F20, "088 im_769F20" },
		{ 0x769F80, (void *)im_769F80, "088 im_769F80" },
		{ 0x769FA0, (void *)im_769FA0, "088 im_769FA0" },
		{ 0x769FC0, (void *)im_769FC0, "088 im_769FC0" },
		{ 0x76A000, (void *)im_76A000, "088 im_76A000" },
		{ 0x76A130, (void *)im_76A130, "088 im_76A130" },
		{ 0x76A180, (void *)im_76A180, "088 im_76A180" },
		{ 0x76A1F0, (void *)im_76A1F0, "088 im_76A1F0" },
		{ 0x76A380, (void *)im_76A380, "088 im_76A380" },
		{ 0x76A650, (void *)im_76A650, "088 im_76A650" },
		{ 0x76A780, (void *)im_76A780, "088 im_76A780" },
		{ 0x76A9A0, (void *)im_76A9A0, "088 im_76A9A0" },
		{ 0x76AC30, (void *)im_76AC30, "088 im_76AC30" },
		{ 0x76B0B0, (void *)im_76B0B0, "088 im_76B0B0" },
		{ 0x76BB60, (void *)im_76BB60, "088 im_76BB60" },
		{ 0x76C060, (void *)im_76C060, "088 im_76C060" },
		{ 0x76C6B0, (void *)im_76C6B0, "088 im_76C6B0" },
		{ 0x76C760, (void *)im_76C760, "088 im_76C760" },
		{ 0x76C940, (void *)im_76C940, "088 im_76C940" },
		{ 0x76CBC0, (void *)im_76CBC0, "088 im_76CBC0" },
		{ 0x76D270, (void *)im_76D270, "088 im_76D270" },
		{ 0x76D300, (void *)im_76D300, "088 im_76D300" },
		{ 0x76D360, (void *)im_76D360, "088 im_76D360" },
		{ 0x76D5E0, (void *)im_76D5E0, "088 im_76D5E0" },
		{ 0x76D7F0, (void *)im_76D7F0, "088 im_76D7F0" },
		{ 0x76DF50, (void *)im_76DF50, "088 im_76DF50" },
		{ 0x76E180, (void *)im_76E180, "088 im_76E180" },
		{ 0x76E440, (void *)im_76E440, "088 im_76E440" },
		{ 0x76E6A0, (void *)im_76E6A0, "088 im_76E6A0" },
		{ 0x76E980, (void *)im_76E980, "088 im_76E980" },
		{ 0x76EA40, (void *)im_76EA40, "088 im_76EA40" },
		{ 0x76EAC0, (void *)im_76EAC0, "088 im_76EAC0" },
		{ 0x76EB00, (void *)im_76EB00, "088 im_76EB00" },
		{ 0x76FB30, (void *)im_76FB30, "088 im_76FB30" },
		{ 0x76FC40, (void *)im_76FC40, "088 im_76FC40" },
		{ 0x76FCC0, (void *)im_76FCC0, "088 im_76FCC0" },
		{ 0x76FCF0, (void *)im_76FCF0, "088 im_76FCF0" },
		{ 0x76FD90, (void *)im_76FD90, "088 im_76FD90" },
		{ 0x76FE10, (void *)im_76FE10, "088 im_76FE10" },
		{ 0x76FE90, (void *)im_76FE90, "088 im_76FE90" },
		{ 0x770670, (void *)im_770670, "088 im_770670" },
		{ 0x770890, (void *)im_770890, "088 im_770890" },
		{ 0x771380, (void *)im_771380, "088 im_771380" },
		{ 0x771410, (void *)im_771410, "088 im_771410" },
		{ 0x7714A0, (void *)im_7714A0, "088 im_7714A0" },
		{ 0x7714D0, (void *)im_7714D0, "088 im_7714D0" },
		{ 0x7715C0, (void *)im_7715C0, "088 im_7715C0" },
		{ 0x771600, (void *)im_771600, "088 im_771600" },
		{ 0x771610, (void *)im_771610, "088 im_771610" },
		{ 0x771660, (void *)im_771660, "088 im_771660" },
		{ 0x7716C0, (void *)im_7716C0, "088 im_7716C0" },
		{ 0x7717A0, (void *)im_7717A0, "088 im_7717A0" },
		{ 0x771880, (void *)im_771880, "088 im_771880" },
		{ 0x7718D0, (void *)im_7718D0, "088 im_7718D0" },
		{ 0x771B80, (void *)im_771B80, "088 im_771B80" },
		{ 0x771BD0, (void *)im_771BD0, "088 im_771BD0" },
		{ 0x771C00, (void *)im_771C00, "088 im_771C00" },
		{ 0x771C30, (void *)im_771C30, "088 im_771C30" },
		{ 0x771C60, (void *)im_771C60, "088 im_771C60" },
		{ 0x771C90, (void *)im_771C90, "088 im_771C90" },
		{ 0x771CC0, (void *)im_771CC0, "088 im_771CC0" },
		{ 0x771CF0, (void *)im_771CF0, "088 im_771CF0" },
		{ 0x771D20, (void *)im_771D20, "088 im_771D20" },
		{ 0x771D50, (void *)im_771D50, "088 im_771D50" },
		{ 0x771D70, (void *)im_771D70, "088 im_771D70" },
		{ 0x771DB0, (void *)im_771DB0, "088 im_771DB0" },
		{ 0x771F50, (void *)im_771F50, "088 im_771F50" },
		{ 0x772060, (void *)im_772060, "088 im_772060" },
		{ 0x7720C0, (void *)im_7720C0, "088 im_7720C0" },
		{ 0x772790, (void *)im_772790, "088 im_772790" },
		{ 0x7727B0, (void *)im_7727B0, "088 im_7727B0" },
		{ 0x772880, (void *)im_772880, "088 im_772880" },
		{ 0x772990, (void *)im_772990, "088 im_772990" },
		{ 0x7729F0, (void *)im_7729F0, "088 im_7729F0" },
		{ 0x772A10, (void *)im_772A10, "088 im_772A10" },
		{ 0x772AB0, (void *)im_772AB0, "088 im_772AB0" },
		{ 0x772B40, (void *)im_772B40, "088 im_772B40" },
		{ 0x772B80, (void *)im_772B80, "088 im_772B80" },
		{ 0x772BE0, (void *)im_772BE0, "088 im_772BE0" },
		{ 0x772CB0, (void *)im_772CB0, "088 im_772CB0" },
		{ 0x772D10, (void *)im_772D10, "088 im_772D10" },
		{ 0x772D40, (void *)im_772D40, "088 im_772D40" },
		{ 0x772D90, (void *)im_772D90, "088 im_772D90" },
		{ 0x772E30, (void *)im_772E30, "088 im_772E30" },
		{ 0x772F50, (void *)im_772F50, "088 im_772F50" },
		{ 0x772FB0, (void *)im_772FB0, "088 im_772FB0" },
		{ 0x772FE0, (void *)im_772FE0, "088 im_772FE0" },
		{ 0x773010, (void *)im_773010, "088 im_773010" },
		{ 0x773040, (void *)im_773040, "088 im_773040" },
		{ 0x773090, (void *)im_773090, "088 im_773090" },
		{ 0x773100, (void *)im_773100, "088 im_773100" },
		{ 0x773130, (void *)im_773130, "088 im_773130" },
		{ 0x7732F0, (void *)im_7732F0, "088 im_7732F0" },
		{ 0x773320, (void *)im_773320, "088 im_773320" },
		{ 0x7733B0, (void *)im_7733B0, "088 im_7733B0" },
		{ 0x773400, (void *)im_773400, "088 im_773400" },
		{ 0x773520, (void *)im_773520, "088 im_773520" },
		{ 0x773560, (void *)im_773560, "088 im_773560" },
		{ 0x773590, (void *)im_773590, "088 im_773590" },
		{ 0x773600, (void *)im_773600, "088 im_773600" },
		{ 0x773630, (void *)im_773630, "088 im_773630" },
		{ 0x773670, (void *)im_773670, "088 im_773670" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag088_invincible_moon()
	{
		act::register_module(88);
		for (const act::moon88::ModPort *p = act::moon88::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(88, p->addr, p->port, p->name);
		for (const act::moon88::ModPort *p = act::moon88::PORTS; p->addr; p++)
			act::register_module_port(88, p->addr, p->port, p->name);
		// 30 fps layer: see mag088_invincible_moon_held.inc
		FX_HELD(register_mag088_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag088_invincible_moon_held.inc"
#endif
