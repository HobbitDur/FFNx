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

// Effect 80: Ultimecia junctions to Griever (enemy attack 352 of kernel.bin, no name; Ultimecia c0m123;
// no other kernel.bin entry uses effect 80; MAG_080_ULTIMECIA_JUNCTIONING_TO_GRIEVER, texture file
// magic/mag079.tim): a copy of the actor/heal effect library (see act_engine.h) built like MiniMog
// (mag096_minimog.cpp: master + Tonberry's director, the prim-model tasks of Doom / Resonance, the
// actor system) plus the module's own parts.
//
// Setup MAG_080_ULTIMECIA_JUNCTIONING_TO_GRIEVER_Init 0x7AB560 (runs once, not ported; file loader
// 0x7AB540 = the texture file 0x1580B10 -> [0x25D4938], TIM uploaded by the setup unless the cast
// context's flag +1 bit 0, camera animation 0x157B62C; packet arenas at file + 0x8000 / + 0x10000
// (0x25DE424/0x25DE420, 0x25DD81C/0x25DD818) by tick parity, the file arena cursor 0x25DF304 = file +
// 0x10000) creates the root queue 0x25DAFD0 with the master and eight task pools.
//   Master (0x7AB6F0 = MiniMog's master 0x731F70 with 11 states) - camera copy 0x2793E58 (pointers
//     0x25DF2F8 / 0x25DAF00), packet arenas by tick parity (sprite cursor 0x25D493C / 0x25DAEFC), bone
//     follow, states: 0x7AB880 next, 0x7AB890 PUTS THE CASTER ENTITY at the target's position, 0x6E8
//     higher (+0x1C..+0x20, +0x54..+0x5C), 0x7AB8F0 reserves the actor pools after the file arena
//     (0x870 + 0x10680 bytes), 0x7AB970 starts the director, 0x7B4AE0.. next action / finish.
//   Director (0x7ABAF0, Tonberry's 0x762C60 with 13 states) - cuts 1..4 (director block [0x157B2E0]):
//     claims a voice, hides the monsters (flag bit 2) cut 1..4, the damage cue at cut frame 0xA0, shows
//     them again and releases the voice. Cut 1 starts the junction task 0x7AF390 and the stage wobble
//     0x7AF1D0 (level words 0x1D98992 + k * 0x2C), cut 2 the prim model 0x7ABF90 (rising on the caster
//     by the height table 0x157B8C4) and the particle emitter 0x7AEFA0 (particles 0x7AF030 orbiting the
//     model), cut 3 the wobble fade-out 0x7AE690 / 0x7AF2C0.
//   Junction task (0x7AF390, 10 states) - copies the TARGET's model into the block 0x25D4948
//     (0x7B4480), hides the target and the caster, plays the actor sequences 0x1749C64 / 0x174F0BC /
//     0x1750660 (actor system 0x7B0AE0), the junction prim model 0x7B4540 at cue 3, then grows the
//     copy (scale 0x25D49EA..EE), spins (0x25D49E0) and shakes it (jitter 0x25D49FC..0x25D4A00) while
//     its morph level 0x25D4A0A falls (0x7AFFB0: Raldo's ball renderer, block 0x10 bytes shorter than
//     effect 79's) and the target's chain transformation 0x1A is queued; while drawn (flag 8) it
//     WRITES THE TARGET ENTITY's position (+0x1C/+0x1E/+0x20 and +0x54/+0x58/+0x5C) every tick; 0x7B4750
//     resets the target's matrix / position for the bone query and puts them back.
//     Every tick it draws and advances the bolts (0x7AF4D0 / 0x7AF9F0, a copy of Diablos' 0x657F00 /
//     0x6583E0 lightning: chains of segments at [0x1580A6C], module LCG 0x25DF324, frame arena
//     0x1D8E054) and from tick 0x50 to 0x96 spawns two bolts around the caster (0x7B06A0 / 0x7B0860).
//   Particles 0x7AE7D0 (flakes on the target) and 0x7AF030 are sprite sequences drawn by 0x7AE840
//     (module sprite cursor 0x25D493C); the actor systems (0x7B0B70, a_73F990 family) and the prim
//     plays (0x7AC0B0, callback 0x7AC1E0) are the library's.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace ujunct
{
	static const Mod MOD_080 = { "ultijunction", 80, 0x7AB540, 0x7B4BB0,
		{ 0x0, 0x0, 0x157B2E0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25DF018, 0x25DF2FC, 0x0, 0x0, 0x25D4938, 0x0, 0x0, 0x0, 0x25DAF00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25DE010, 0x0, 0x25DE2B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25DF008, 0x0, 0x0, 0x25DF2F0, 0x0, 0x25DF2F8, 0x0, 0x25DF308 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7AB9A0, 0x7ABA20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7ABC40, 0x0, 0x7ABC90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7B0D20, 0x7B0E00, 0x7B0FA0, 0x7B1070, 0x7B1190, 0x7B13F0, 0x0, 0x0, 0x7B2110, 0x7B2160, 0x0, 0x0, 0x7B2A50, 0x0, 0x0, 0x0, 0x7B3430, 0x7B3460, 0x7B3540, 0x7B3660, 0x7B37B0, 0x7B3E40, 0x7B3E70, 0x7B4050, 0x7B4190, 0x7AEAA0, 0x0, 0x7B4A50, 0x7B4A90, 0x7B4AB0, 0x7B4AD0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x157B2E0;        // pointer to the director block (cut +0x40/+0x42/+0x44, frame +0x46)
	static const uint32_t SPRITE_CURSOR = 0x25D493C;  // module sprite packet cursor (tick parity: file + 0x8000 / + 0)
	static const uint32_t MODEL_BLOCK = 0x25D4948;    // the target-model copy block (0xC4 bytes)
	static const uint32_t BOLT_BASE = 0x1580A6C;      // pointer to the bolt buffers (segments +0, chains +0x3000)
	static const uint32_t Q_PRIM = 0x25DD808;         // prim-model task queue
	static const uint32_t BLEND_BUF = 0x25DF020;      // prim-model vertex lerp scratch
	static const uint32_t PRIM_CB = 0x7AC1E0;         // the prim-model player's object callback
	static const uint32_t FA_CURSOR = 0x1D8E054;      // frame arena packet cursor
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = SPRITE_CURSOR;
	static const uint32_t SCRATCH_SP = 0x25DF2F8;     // module scratch stack pointer (below the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25DF308;    // current actor system state block

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t GTE_MVMVA_ColorV0() { return fn<uint32_t (__cdecl *)()>(0x4607D0)(); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t sub_45DEA0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DEA0)(a1); }
		static inline uint32_t sub_45E0E0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0E0)(a1); }
		static inline uint32_t sub_45E410(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E410)(a1); }
		static inline uint32_t GTE_ReadOTZ_SZ3div4(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E250)(a1); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E160(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a1, a2, a3); }
		static inline uint32_t sub_45E220(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E220)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t sub_56BDE0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(a1, a2); }
		static inline uint32_t Music_SetVolumeTrans(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x46BBC0)(a1, a2, a3); }
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); }
		static inline uint32_t BS_R0win_QueueVictoryFanfare(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x501A20)(a1, a2); }
		static inline uint32_t BS_Music_SetChannelVolumeCommand(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x501B10)(a1, a2); }
		static inline uint32_t sub_8DE8F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x8DE8F0)(a1, a2, a3, a4); }
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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ud_7AB6F0(uint32_t a1);
	uint32_t __cdecl ud_7AB890(uint32_t a1);
	uint32_t __cdecl ud_7AB8F0(uint32_t a1);
	uint32_t __cdecl ud_7AB930(void);
	uint32_t __cdecl ud_7ABA20(uint32_t a1);
	uint32_t __cdecl ud_7ABA50(uint32_t a1);
	uint32_t __cdecl ud_7ABAF0(uint32_t a1);
	uint32_t __cdecl ud_7ABBA0(uint32_t a1);
	uint32_t __cdecl ud_7ABBF0(uint32_t a1);
	uint32_t __cdecl ud_7ABC70(uint32_t a1);
	uint32_t __cdecl ud_7ABCB0(void);
	uint32_t __cdecl ud_7ABCF0(uint32_t a1);
	uint32_t __cdecl ud_7ABD50(uint32_t a1);
	uint32_t __cdecl ud_7ABD90(uint32_t a1);
	uint32_t __cdecl ud_7ABDB0(uint32_t a1);
	uint32_t __cdecl ud_7ABDD0(uint32_t a1);
	uint32_t __cdecl ud_7ABE00(void);
	uint32_t __cdecl ud_7ABE80(uint32_t a1);
	uint32_t __cdecl ud_7ABF40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl ud_7ABF90(uint32_t a1);
	uint32_t __cdecl ud_7AC000(uint32_t a1);
	uint32_t __cdecl ud_7AC060(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7AC0B0(uint32_t a1);
	uint32_t __cdecl ud_7AC1E0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7AC420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7AC550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7AC770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7ACA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7ACEF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7ADA10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7ADF70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7AE640(uint32_t a1);
	uint32_t __cdecl ud_7AE6F0(uint32_t a1);
	uint32_t __cdecl ud_7AE710(uint32_t a1);
	uint32_t __cdecl ud_7AE740(uint32_t a1);
	uint32_t __cdecl ud_7AE7D0(uint32_t a1);
	uint32_t __cdecl ud_7AE840(uint32_t a1);
	uint32_t __cdecl ud_7AEF10(uint32_t a1);
	uint32_t __cdecl ud_7AEF30(uint32_t a1);
	uint32_t __cdecl ud_7AEFA0(uint32_t a1);
	uint32_t __cdecl ud_7AEFF0(uint32_t a1);
	uint32_t __cdecl ud_7AF030(uint32_t a1);
	uint32_t __cdecl ud_7AF130(uint32_t a1);
	uint32_t __cdecl ud_7AF190(uint32_t a1);
	uint32_t __cdecl ud_7AF270(uint32_t a1);
	uint32_t __cdecl ud_7AF320(uint32_t a1);
	uint32_t __cdecl ud_7AF340(uint32_t a1);
	uint32_t __cdecl ud_7AF390(uint32_t a1);
	uint32_t __cdecl ud_7AF4C0(void);
	uint32_t __cdecl ud_7AF4D0(void);
	uint32_t __cdecl ud_7AF5E0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7AF860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7AF9F0(void);
	uint32_t __cdecl ud_7AFD80(void);
	uint32_t __cdecl ud_7AFDD0(uint32_t a1);
	uint32_t __cdecl ud_7AFEA0(void);
	uint32_t __cdecl ud_7AFEC0(uint32_t a1);
	uint32_t __cdecl ud_7AFFB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B06A0(uint32_t a1);
	uint32_t __cdecl ud_7B0840(uint32_t a1);
	uint32_t __cdecl ud_7B0860(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7B09E0(uint32_t a1);
	uint32_t __cdecl ud_7B0B40(uint32_t a1);
	uint32_t __cdecl ud_7B0D20(uint32_t a1);
	uint32_t __cdecl ud_7B0FA0(void);
	uint32_t __cdecl ud_7B1650(uint32_t a1);
	uint32_t __cdecl ud_7B16E0(void);
	uint32_t __cdecl ud_7B1740(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B18A0(void);
	uint32_t __cdecl ud_7B1960(uint32_t a1);
	uint32_t __cdecl ud_7B19E0(void);
	uint32_t __cdecl ud_7B1A20(uint32_t a1);
	uint32_t __cdecl ud_7B2A50(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B2B60(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B2BE0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B2C10(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7B2CB0(uint32_t a1);
	uint32_t __cdecl ud_7B2D30(uint32_t a1);
	uint32_t __cdecl ud_7B2DB0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7B37B0(uint32_t a1);
	uint32_t __cdecl ud_7B42A0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B4330(void);
	uint32_t __cdecl ud_7B43C0(uint32_t a1);
	uint32_t __cdecl ud_7B43F0(void);
	uint32_t __cdecl ud_7B4400(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7B4480(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5);
	uint32_t __cdecl ud_7B44F0(uint32_t a1);
	uint32_t __cdecl ud_7B4540(uint32_t a1);
	uint32_t __cdecl ud_7B45B0(uint32_t a1);
	uint32_t __cdecl ud_7B4620(uint32_t a1);
	uint32_t __cdecl ud_7B4650(uint32_t a1);
	uint32_t __cdecl ud_7B4670(uint32_t a1);
	uint32_t __cdecl ud_7B4750(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B4800(uint32_t a1);
	uint32_t __cdecl ud_7B4820(uint32_t a1);
	uint32_t __cdecl ud_7B4880(uint32_t a1);
	uint32_t __cdecl ud_7B49F0(uint32_t a1);
	uint32_t __cdecl ud_7B4A50(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag080_ultimecia_junction_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace ujunct
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code
	// matches a Doom / Death / Siren / Tonberry / Raijin Attack / Sigh / Magma Breath / Esuna /
	// Mighty Guard port up to the module's addresses and callees (the port's text with the
	// module's addresses; their comments describe the source original)
	// ====================================================================================

	// 0x731F70 (module 096 MAG_096_sub_731F70): MiniMog MASTER task - snapshots the camera matrix
	// into 0x2793E58, picks the double-buffered pools by tick parity, runs the 11-state master
	// state table, then executes the 8 module task queues (live task count -> node +0x5E).
	uint32_t __cdecl ud_7AB6F0(uint32_t a1)
	{
		g_mod = &MOD_080;
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25DF2F8) = 0x2793E58;
		MEM<uint32_t>(0x25DAF00) = 0x2793E58;
		states[0] = 0x7AB880;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x7AB890;
		states[2] = 0x7AB8F0;
		states[3] = 0x7AB970;
		states[4] = 0x7B4AE0;
		states[5] = 0x7B4B20;
		states[6] = 0x7B4B30;
		states[7] = 0x7B4B40;
		states[8] = 0x7B4B70;
		states[9] = 0x7B4B80;
		states[10] = 0x7B4BA0;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25DE424);
			const uint32_t v2 = MEM<uint32_t>(0x25DD81C);
			MEM<uint32_t>(0x25D493C) = v1;
			MEM<uint32_t>(0x25DAEFC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25DE420);
			const uint32_t v2 = MEM<uint32_t>(0x25DD818);
			MEM<uint32_t>(0x25D493C) = v1;
			MEM<uint32_t>(0x25DAEFC) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x25DF2F0) = 0;
		MEM<uint16_t>(0x25DF008) = 0;
		static const uint32_t queues[8] = { 0x25DE2B0, 0x25D4AE8, 0x25DEFF8, 0x25DE010, 0x25DD808, 0x25DAFE0, 0x25DCF38, 0x25DE2A0 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7AB890 (MAG_080_sub_7AB890): master state 1 - puts the CASTER entity (+0x2C) at the target
	// entity's (+0x2D) position, 0x6E8 higher (y - 0x6E8), and its float copy +0x54/+0x58/+0x5C,
	// next state
	uint32_t __cdecl ud_7AB890(uint32_t a1)
	{
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		U32(c, 0x1C) = U32(t, 0x1C);
		U32(c, 0x20) = U32(t, 0x20);
		U16(c, 0x1E) = (uint16_t)(U16(c, 0x1E) + 0xF918);
		S32(c, 0x54) = S16(c, 0x1C);
		S32(c, 0x58) = S16(c, 0x1E);
		S32(c, 0x5C) = S16(c, 0x20);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x839BB0 (MAG_043_sub_839BB0; = Wind Blast 0x879710 with this module's pool sizes): master state -
	// once no child is alive: the two actor pools are carved from the file arena cursor
	// [0x2656224] (0x870 bytes = 20 particles of 0x6C, 0x10680 bytes = 100 actors of 0x2A0) and
	// cleared (0x839BF0), next state
	uint32_t __cdecl ud_7AB8F0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25DF304);
		MEM<uint32_t>(0x25DF00C) = p;
		p += 0x870;
		MEM<uint32_t>(0x25DF2FC) = p;
		p += 0x10680;
		MEM<uint32_t>(0x25DF304) = p;
		ud_7AB930();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x825010 (MAG_048_sub_825010; = Counter Laser-Eye 0x873910 with 100 actors): clears the
	// particle pool 0x26495C4 (0x870) and the actor pool 0x26495DC (0x10680), zeroes the particle
	// cursor 0x26489A8, the actor cursor 0x26495D0 and the words 0x26495C0 / 0x26481BC
	uint32_t __cdecl ud_7AB930(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25DF00C), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25DF2FC), 0x10680);
		MEM<uint16_t>(0x25DCF30) = 0;
		MEM<uint16_t>(0x25DF018) = 0;
		MEM<uint16_t>(0x25DEE58) = 0;
		MEM<uint16_t>(0x25D4940) = 0;
		return 0; // void
	}

	// 0x8A36A0 (copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - places the reaper, spawns the director
	// 0x8B6E30 (0x48 bytes, director queue), next state
	uint32_t __cdecl ud_7ABA20(uint32_t a1)
	{
		ud_7ABA50(a1);
		x::Effect_AddTaskAndInitFromCtx(0x25D4AE8, 0x7ABAF0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7ABA50 (MAG_080_sub_7ABA50): director data set-up - clears the director block [0x157B2E0]
	// (0x54 bytes), +0x1C = the target entity, +0x08/+0x0C = its position, +0x00/+0x04 = the
	// caster's, +0x4A = the caster -> target angle - 0x800
	uint32_t __cdecl ud_7ABA50(uint32_t a1)
	{
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U32(d, 0x1C) = t;
		U32(d, 8) = U32(t, 0x1C);
		U32(d, 0xC) = U32(t, 0x20);
		U32(d, 0) = U32(c, 0x1C);
		U32(d, 4) = U32(c, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t a = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)((a - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x7ABAF0 (MAG_080_sub_7ABAF0): DIRECTOR task (Tonberry's 0x762C60 with 13 states) - cut
	// bookkeeping (0x7ABBA0), then {0x7ABBF0 claim voice, 0x7ABC20 next, 0x7ABC70 cut 1: hide the
	// monsters, 0x7ABCF0 frame 0x2D, 0x7ABD10 / 0x7ABD30 cut 2, 0x7ABD50 frame 0x76, 0x7ABD70 /
	// 0x7ABD90 cut 3, 0x7ABDB0 damage cue at frame 0xA0, 0x7ABDD0 cut 4: show the monsters +
	// release the voice, 0x7ABE40 finish, 0x7ABE60 ret}
	uint32_t __cdecl ud_7ABAF0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[13];
		states[0] = 0x7ABBF0;
		states[1] = 0x7ABC20;
		states[2] = 0x7ABC70;
		states[3] = 0x7ABCF0;
		states[4] = 0x7ABD10;
		states[5] = 0x7ABD30;
		states[6] = 0x7ABD50;
		states[7] = 0x7ABD70;
		states[8] = 0x7ABD90;
		states[9] = 0x7ABDB0;
		states[10] = 0x7ABDD0;
		states[11] = 0x7ABE40;
		states[12] = 0x7ABE60; // nullsub (ret)
		ud_7ABBA0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ud_7ABBA0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ud_7ABE80(a1);
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

	// 0x7ABBF0 (sub_7ABBF0): director state 0 - claims a voice slot (0x175AAE0, 1, 0x80) into
	// director +0x52, next state
	uint32_t __cdecl ud_7ABBF0(uint32_t a1)
	{
		const uint32_t v = x::BdSound_ClaimVoiceSlot(0x175AAE0, 1, 0x80);
		U16(MEM<uint32_t>(DIR_PTR), 0x52) = (uint16_t)v;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7B5710 (sub_7B5710): director state 1 - once cut >= 1: hides the monsters (0x7B5750), next state
	uint32_t __cdecl ud_7ABC70(uint32_t a1)
	{
		if (a_73B7E0(1) == 0)
			return 0;
		ud_7ABCB0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5750 (sub_7B5750): for the battle entities 0..2 (0x1D972C0, stride 0x9C, 3 entries) with flag
	// bit 1: saves the flag word to director +0x20[i] and sets flag bit 2
	uint32_t __cdecl ud_7ABCB0(void)
	{
		uint32_t save = MEM<uint32_t>(0x157B2E0) + 0x20;
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

	// 0x82B400 (sub_82B400): director state - next state from phase tick 0x36
	uint32_t __cdecl ud_7ABCF0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(PHASE_DATA), 0x46) >= 0x2D)
			next_state(a1);
		return 0; // void
	}

	// 0x82B400 (sub_82B400): director state - next state from phase tick 0x36
	uint32_t __cdecl ud_7ABD50(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(PHASE_DATA), 0x46) >= 0x76)
			next_state(a1);
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73B7E0).
	uint32_t __cdecl ud_7ABD90(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x7B58E0 (sub_7B58E0): director state 7 - from cut frame 0x5A: the damage cue (director
	// +0x48 = 1), next state
	uint32_t __cdecl ud_7ABDB0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x157B2E0);
		if (S16(d, 0x46) < 0xA0)
			return d;
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5900 (sub_7B5900): director state 8 - at cut 3 (0x7B56E0 gate): restores the entities'
	// flag bit 2 (0x7B5930), releases the voice slot of director +0x52 (sub_4A2940), next state
	uint32_t __cdecl ud_7ABDD0(uint32_t a1)
	{
		if (a_73B640(4) == 0)
			return 0;
		ud_7ABE00();
		x::sub_4A2940((uint32_t)(int32_t)S16(MEM<uint32_t>(0x157B2E0), 0x52));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5930 (sub_7B5930): restores flag bit 2 of the battle entities 0..2 (flag bit 1 set) from
	// the words saved by 0x7B5750
	uint32_t __cdecl ud_7ABE00(void)
	{
		uint32_t save = MEM<uint32_t>(0x157B2E0) + 0x20;
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

	// 0x7ABE80 (sub_7ABE80): a new cut (director +0x40): cut 1 starts the junction task 0x7AF390
	// and the stage wobble 0x7AF1D0, cut 2 the prim-model task 0x7ABF90 (model 0x1754B2C, anim
	// 0xC4, mode 0 / 1) and the particle emitter 0x7AEFA0, cut 3 the stage wobble out 0x7AE690 and
	// 0x7AF2C0 (0xA0-byte tasks, queue 0x25DAFE0)
	uint32_t __cdecl ud_7ABE80(uint32_t a1)
	{
		const int32_t cut = S16(MEM<uint32_t>(DIR_PTR), 0x40);
		if (cut == 1)
		{
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AF390, 0xA0, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AF1D0, 0xA0, a1);
		}
		else if (cut == 2)
		{
			ud_7ABF40(a1, 0x7ABF90, 0x1754B2C, 0xC4, 0, 1);
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AEFA0, 0xA0, a1);
		}
		else if (cut == 3)
		{
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AE690, 0xA0, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AF2C0, 0xA0, a1);
		}
		return 0; // void
	}

	// 0x893900 (copy of h_86B340 0x86B340: MAG_033_sub_86B340): spawns a prim-model task `fn` (0x180 bytes, prim queue)
	// under `parent`: +0x74 model data, +0x78 animation word (sign-extended), +0x80 / +0x82 modes;
	// returns the node
	uint32_t __cdecl ud_7ABF40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x180, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x7ABF90 (sub_7ABF90): prim-model TASK (the cut-2 model) - states {0x7AC000 set-up, 0x7AE640
	// play, 0x7AE680 ret}, then director +0x10/+0x14 = its position
	uint32_t __cdecl ud_7ABF90(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7AC000;
		states[1] = 0x7AE640;
		states[2] = 0x7AE680; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t w0 = U32(node, 0x1C);
		const uint32_t w1 = U32(node, 0x20);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		U32(d, 0x10) = w0;
		U32(d, 0x14) = w1;
		const uint8_t status = U8(node, 0x26);
		return task_end(node, status);
	}

	// 0x7AC000 (sub_7AC000): prim-model state 0 - decodes the prim layout, scale 1.0, position =
	// the caster raised by the height table at step +0x84 (0x7AC060), depth offset +0x88 = 0, first
	// model step (0x7AC0B0), next state
	uint32_t __cdecl ud_7AC000(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		ud_7AC060(node, node + 0x1C, (uint32_t)U16(node, 0x84));
		U16(node, 0x88) = 0;
		ud_7AC0B0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7AC060 (sub_7AC060): a2 (SVECTOR + pad) = the caster entity's position, y + the height table
	// 0x157B8C4[min(a3, 0x77)]
	uint32_t __cdecl ud_7AC060(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		int16_t k = (int16_t)a3;
		if (k >= 0x78)
			k = 0x77;
		U32(a2, 0) = U32(c, 0x1C);
		U32(a2, 4) = U32(c, 0x20);
		U16(a2, 2) = (uint16_t)(U16(a2, 2) + MEM<uint16_t>(0x157B8C4 + (uint32_t)((int32_t)k * 2)));
		return a2;
	}

	// 0x836EB0 (sub_836EB0; Boko b_72A720 with the module's blend buffer, no pause and the depth
	// offset +0x88 in the block): builds the prim-model effect matrix (rotations +0x62/+0x60/+0x64,
	// scale +0x50, position +0x1C, camera) and the 0x5C parameter block, then plays the prim-model
	// layout +0x94 with the draw callback 0x836FE0; returns the player result (0 = play finished)
	uint32_t __cdecl ud_7AC0B0(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = 0x25DF020 (vertex lerp scratch),
		// +0x4C..+0x58 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88. +0x20..+0x37,
		// +0x3C..+0x43, +0x5A are never written (the callback 0x7AC1E0 does not read them)
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
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(held_note_play(node);)
		return prim_play(node + 0x94, PRIM_CB, L, 0);
	}

	// 0x82B870 (sub_82B870): prim-model player draw callback (layout a1, record a2, block a3) - as
	// Doom's 0x8C5B10 without its record flags 0x400 / 0x100 / 0x4000: picks the object's vertex
	// frame (lerped by MAG_017_sub_701390), builds its matrix (rotation 0x701310; parent-rotated
	// position unless record flag 0x200; scale3DMatrix), fills a 0x68-byte Field_Alloc render
	// header (flags 0x2030, 0x20F0 with alpha + colour, depth offset block +0x58, scale words 0x100)
	// and draws it with 0x82BAB0 into OT base+0x44 (shift 2) at the frame arena cursor 0x1D8E054
	uint32_t __cdecl ud_7AC1E0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten), +8 scale (3 x int32), +0x18 Mat4x3 object
		// matrix (+0x2C translation)
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
		const uint32_t cursor = MEM<uint32_t>(FA_CURSOR);
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
		MEM<uint32_t>(FA_CURSOR) = ud_7AC420(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8C5DD0 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8C5F00, 0x8C6120, 0x8C63B0, 0x8C68A0, a_73D770, a_73D9C0, 0x8C73C0,
	// 0x8C7920; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl ud_7AC420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { ud_7AC550, ud_7AC770, ud_7ACA00, ud_7ACEF0, a_73D770, a_73D9C0, ud_7ADA10, ud_7ADF70 };
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
	uint32_t __cdecl ud_7AC550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7AC770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7ACA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7ACEF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7ADA10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7ADF70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7AE640 (sub_7AE640): prim-model state 1 - director +0x50 += 1, position = the caster raised
	// by the height table at that step (0x7AC060), model step (0x7AC0B0); when the play ended (0)
	// finished, next state
	uint32_t __cdecl ud_7AE640(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U16(d, 0x50) = (uint16_t)(U16(d, 0x50) + 1);
		ud_7AC060(node, node + 0x1C, (uint32_t)U16(d, 0x50));
		if (ud_7AC0B0(node) == 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x892F30 (MAG_024_sub_892F30; same code as Drain r_856900): stage wobble out state 0 - amplitude
	// +0x1C = 0x400, next state
	uint32_t __cdecl ud_7AE6F0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x9C) = 0x26;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x7BCBC0 (sub_7BCBC0): task 0x7BCB40 state 1 - one flake per tick (0x7BCBF0) for +0xB0 ticks,
	// then finished, next state
	uint32_t __cdecl ud_7AE710(uint32_t a1)
	{
		ud_7AE740(a1);
		U16(a1, 0x9C) = (uint16_t)(U16(a1, 0x9C) - 1);
		if (S16(a1, 0x9C) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BCBF0 (sub_7BCBF0): flake - the node at the caster's bone 0xF1 (GetEffectSpawnPosition ->
	// +0x1C), one flake task 0x7BCC80 (0xB4 bytes, queue 0x25E5100) at that position plus
	// ((rand & 0x7FF) - 0x400, (rand & 0x7FF) - 0x400, (rand & 0x1FF) - 0x100) (+0x6C/+0x6E/+0x70)
	uint32_t __cdecl ud_7AE740(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2D);
		x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0xF1, 0, node + 0x1C);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AE7D0, 0xA0, node);
		const uint32_t w1 = U32(node, 0x20);
		const uint32_t w0 = U32(node, 0x1C);
		U32(t, 0x5C) = w0;
		U32(t, 0x60) = w1;
		U16(t, 0x5C) = (uint16_t)(U16(t, 0x5C) + (uint16_t)((x::CrtRand() & 0x7FF) - 0x400));
		U16(t, 0x5E) = (uint16_t)(U16(t, 0x5E) + (uint16_t)((x::CrtRand() & 0x7FF) - 0x400));
		U16(t, 0x60) = (uint16_t)(U16(t, 0x60) + (uint16_t)((x::CrtRand() & 0x1FF) - 0x100));
		return 0; // void
	}

	// 0x7BCC80 (sub_7BCC80): flake TASK - state table {0x7BCCF0 sprite set-up, 0x7BCD10 frame step,
	// 0x7BCD30 ret}, drawn at +0x6C/+0x70 (-> +0x1C/+0x20, 0x7BBEB0)
	uint32_t __cdecl ud_7AE7D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7AEF10;
		states[1] = 0x7AEF30;
		states[2] = 0x7AEF80;
		callp(states[S8(node, 0x29)], node);
		U32(node, 0x1C) = U32(node, 0x5C);
		U32(node, 0x20) = U32(node, 0x60);
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(held_note_flake(node);)
		ud_7AE840(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl ud_7AE840(uint32_t a1)
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
		const uint32_t cursor = MEM<uint32_t>(0x25D493C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x25D493C) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x8AAB40 (copy of s_747320 0x747320: module 095): bubble state 0: sprite +0x4C = 0x15334A0, +0x56 = -16, +0x52 = 11, advance.
	uint32_t __cdecl ud_7AEF10(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x157B4E8;
		st++;
		U16(node, 0x56) = 0xff80;
		U16(node, 0x52) = 0xB;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ud_7AEF30(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x7AEFA0 (sub_7AEFA0): particle emitter TASK - states {0x7AEFF0 spawn, 0x7AF1C0 ret}, no draw
	uint32_t __cdecl ud_7AEFA0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = 0x7AEFF0;
		states[1] = 0x7AF1C0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7AEFF0 (sub_7AEFF0): particle emitter state 0 - until tick 0x78: one particle task 0x7AF030
	// (0xA0 bytes, queue 0x25DAFE0) every even tick; then finished, next state
	uint32_t __cdecl ud_7AEFF0(uint32_t a1)
	{
		const uint16_t t = U16(a1, 0x24);
		if ((int16_t)t >= 0x78)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if ((t & 1) == 0)
			x::Effect_AddTaskAndInitFromCtx(0x25DAFE0, 0x7AF030, 0xA0, a1);
		return 0; // void
	}

	// 0x7AF030 (sub_7AF030): particle TASK - states {0x7AF130 set-up, 0x7AF190 sequence step,
	// 0x7AF1B0 ret}; turns its angles (+0x42 += 0x40), places itself at the director's model position
	// (+0x10) plus (0, 0, radius +0x9E) rotated by Y +0x44, X +0x40, Z +0x42 (MAG_022 matrix helpers),
	// draws its sprite (0x7AE840)
	uint32_t __cdecl ud_7AF030(uint32_t a1)
	{
		// stack (entry esp - 0x34): +0 SVECTOR (pad +6 never written, never read), +8 output SVECTOR,
		// +0x14 MATRIX (rotation only read)
		alignas(4) uint8_t loc[0x34] = {};
		const uint32_t L = P(loc);
		const uint32_t V = L;
		const uint32_t O = L + 8;
		const uint32_t M = L + 0x14;
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7AF130;
		states[1] = 0x7AF190;
		states[2] = 0x7AF1B0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x42) = (uint16_t)(U16(node, 0x42) + 0x40);
		x::MAG_022_sub_8DD770(M);
		x::sub_8DD960(M, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD7E0(M, (uint32_t)(int32_t)S16(node, 0x40));
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(node, 0x42));
		U16(V, 4) = U16(node, 0x9E);
		U16(V, 0) = 0;
		U16(V, 2) = 0;
		x::matrixMultiplyVector(M, V, O);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U16(node, 0x1C) = (uint16_t)(U16(d, 0x10) + U16(O, 0));
		U16(node, 0x1E) = (uint16_t)(U16(d, 0x12) + U16(O, 2));
		U16(node, 0x20) = (uint16_t)(U16(d, 0x14) + U16(O, 4));
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(held_note_particle(node);)
		ud_7AE840(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7AF130 (sub_7AF130): particle set-up - sequence 0x157B2EC, 0x12 frames, random angles
	// +0x40 / +0x42 / +0x44 (0..0xFFF), radius +0x9E = 0x180..0x27F, next state
	uint32_t __cdecl ud_7AF130(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = 0x157B2EC;
		U16(node, 0x52) = 0x12;
		U16(node, 0x40) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(node, 0x42) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(node, 0x44) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(node, 0x9E) = (uint16_t)((x::CrtRand() & 0xFF) + 0x180);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ud_7AF190(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8CC280 (copy of d_8BD620 0x8BD620: sub_8BD620): stage wobble state 1 - level +0x1C up by 0xA0 per tick to 0xA00 (then
	// finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl ud_7AF270(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xA0);
		if (S16(a1, 0x1C) >= 0xA00)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0xA00;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x8BDFA0 (sub_8BDFA0): stage wobble out state 0 - level +0x1C = 0xA00, next state
	uint32_t __cdecl ud_7AF320(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x96)
			return a1;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0xA00;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl ud_7AF340(uint32_t a1)
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

	// 0x7AF390 (sub_7AF390): JUNCTION task - its 10 states {0x7B09E0 set-up, 0x7B44F0 at cue 3 the
	// prim model 0x7B4540, 0x7B4650 wait, 0x7B4670 the target's model copy, 0x7B4800 wait, 0x7B4820
	// copy grows, 0x7B4880 copy shakes + morph, 0x7B49F0 target shown, 0x7B4A20 finish, ret}, then
	// the bolts (0x7AF4C0: draw + advance); while flag 8 the TARGET entity's position (+0x1C/+0x1E/
	// +0x20 and +0x54/+0x58/+0x5C) follows the copy's (0x25D49C0 - 0x25D49B8 + jitter 0x25D49FC) and
	// the copy is drawn (0x7AFEC0); the bolt emitter (0x7B06A0)
	uint32_t __cdecl ud_7AF390(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = 0x7B09E0;
		states[1] = 0x7B44F0;
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		states[2] = 0x7B4650;
		states[3] = 0x7B4670;
		states[4] = 0x7B4800;
		states[5] = 0x7B4820;
		states[6] = 0x7B4880;
		states[7] = 0x7B49F0;
		states[8] = 0x7B4A20;
		states[9] = 0x7B4A40; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(held_note_bolts();)
		ud_7AF4C0();
		if ((U8(node, 0x26) & 8) != 0)
		{
			const uint16_t x0 = (uint16_t)(MEM<uint16_t>(0x25D49C0) - MEM<uint16_t>(0x25D49B8) + MEM<uint16_t>(0x25D49FC));
			U16(t, 0x1C) = x0;
			S32(t, 0x54) = (int16_t)x0;
			const uint16_t y0 = (uint16_t)(MEM<uint16_t>(0x25D49C2) - MEM<uint16_t>(0x25D49BA) + MEM<uint16_t>(0x25D49FE));
			U16(t, 0x1E) = y0;
			S32(t, 0x58) = (int16_t)y0;
			const uint16_t z0 = (uint16_t)(MEM<uint16_t>(0x25D49C4) - MEM<uint16_t>(0x25D49BC) + MEM<uint16_t>(0x25D4A00));
			U16(t, 0x20) = z0;
			S32(t, 0x5C) = (int16_t)z0;
			// 30 fps layer: see mag080_ultimecia_junction_held.inc
			FX_HELD(held_note_copy(node);)
			ud_7AFEC0(MODEL_BLOCK);
		}
		ud_7B06A0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7AF4C0 (unknown_libname_90): the bolts: draw (0x7AF4D0), then advance (0x7AF9F0)
	uint32_t __cdecl ud_7AF4C0(void)
	{
		ud_7AF4D0();
		return ud_7AF9F0();
	}

	// ------------------------------------------------------------------------------------
	// Bolts (a copy of Diablos' 0x657F00 / 0x6583E0 system): segments [0x1580A6C] + 0 (0x200 x 0x18:
	// position +0, width +6 (-1 = free), velocity +8, gradient +0x10 / step +0x12, next +0x14),
	// chains [0x1580A6C] + 0x3000 (0x40 x 0x30: first / last segment +0 / +4, head +8 (width +0xE),
	// end point +0x10, progress +0x16, drift +0x18, progress step +0x1E, spread +0x20, branches left
	// +0x26, colour word +0x28, segment count +0x2C, fade +0x2E); module LCG 0x25DF324, jitter
	// shift / keep / half 0x25DF31C / 0x25DF320 / 0x25DF31E, depth 0x25DF322, cursors 0x25DF314 /
	// 0x25DF318, flag byte 0x25DF310
	// ------------------------------------------------------------------------------------

	// 0x7AF4D0 (sub_7AF4D0): draws every chain with two segments or more (0x2B0-byte Field_Alloc
	// header: OT base + 0x44, gradient 0x80F0F0 / 0x40F0 / 0x80F0F0 / 0 at +0x10..+0x1C (+0x0C), faded
	// copy +0x1C..+0x28 (IR0 = fade when < 0x1000), depth +0x2C), camera rotation / translation
	uint32_t __cdecl ud_7AF4D0(void)
	{
		uint32_t c = MEM<uint32_t>(BOLT_BASE) + 0x3000;
		const uint32_t h = x::Field_Alloc(0x2B0);
		U32(h, 0) = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(h, 0x18) = 0x80F0F0;
		U32(h, 0x14) = 0x40F0;
		U32(h, 0x10) = 0x80F0F0;
		U32(h, 0xC) = 0;
		S32(h, 0x2C) = MEM<int16_t>(0x25DF322);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_SetTransVector(0x1D97778);
		for (int n = 0x40; n != 0; n--, c += 0x30)
		{
			if (U32(c, 0) == 0 || S16(c, 0x2C) < 2)
				continue;
			const int16_t fade = S16(c, 0x2E);
			if (fade < 0x1000)
			{
				x::set_dword_1CA8A30((uint32_t)(int32_t)fade);
				uint32_t o = h + 0x28;
				for (int k = 4; k != 0; k--, o -= 4)
				{
					gx::sub_45E0E0(o - 0x10);
					gx::sub_45E9D0();
					gx::sub_45E410(o);
				}
			}
			else
			{
				uint32_t o = h + 0x28;
				for (int k = 4; k != 0; k--, o -= 4)
					U32(o, 0) = U32(o, -0x10);
			}
			ud_7AF5E0(h, U32(c, 0), (uint32_t)(int32_t)S16(c, 0x2C));
		}
		x::Field_Free(0x2B0);
		return 0; // void
	}

	// 0x7AF5E0 (sub_7AF5E0): one chain (header a1, first segment a2, count a3): each segment projected
	// (+0x30 + 0x14 i: screen xy, OTZ, width along a half sine 0x157FA68 stepped 0x800 / count * 2,
	// colour from the header's faded gradient by the segment's +0x10), edges (0x7AF860), then two
	// semi-transparent quads per pair (frame arena 0x1D8E054, OT depth (z0 + z1) / 8 + header +0x2C)
	uint32_t __cdecl ud_7AF5E0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t h = a1;
		const int32_t count = (int32_t)a3;
		const int32_t step = 0x800 / count * 2;
		uint32_t seg = a2;
		uint32_t wave = 0x157FA68;
		uint32_t o = h + 0x30;
		do
		{
			x::GTE_LoadV0(seg);
			x::GTE_RTPS();
			x::GTE_ReadSXY2(o);
			gx::GTE_ReadOTZ_SZ3div4(o + 4);
			const int32_t w = mul32(S16(seg, 6), MEM<int16_t>(wave));
			wave += (uint32_t)step;
			const int32_t g = S16(seg, 0x10);
			const int32_t f = g & 0xFFF;
			U16(o, 6) = (uint16_t)(w >> 12);
			const int32_t i = g >> 12;
			x::set_dword_1CA8A30((uint32_t)(0x1000 - f));
			gx::sub_45E0E0(h + (uint32_t)(i * 4) + 0x1C);
			gx::sub_45E9D0();
			x::set_dword_1CA8A30((uint32_t)f);
			gx::sub_45E0E0(h + (uint32_t)(i * 4) + 0x20);
			gx::sub_45EBF0();
			gx::sub_45E410(o + 0x10);
			seg = U32(seg, 0x14);
			o += 0x14;
		} while (seg != 0);
		const uint32_t dir = h + 4; // (Field_Alloc memory: not written when the first segment is cut)
		uint32_t s = h + 0x30;
		ud_7AF860(s, s + 0x14, 0, dir);
		s += 0x14;
		for (int32_t k = count - 2; k != 0; k--, s += 0x14)
			ud_7AF860(s, s + 0x14, dir, dir);
		ud_7AF860(s, 0, dir, 0);
		uint32_t a = h + 0x30;
		for (int32_t k = count - 1; k != 0; k--, a += 0x14)
		{
			const uint32_t b = a + 0x14;
			const int16_t za = S16(a, 4);
			if (za <= 0)
				continue;
			const int16_t zb = S16(b, 4);
			if (zb <= 0)
				continue;
			uint32_t p = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = p + 0x34;
			U32(p, 8) = U32(a, 0);
			U32(p, 0x14) = U32(a, 8);
			U32(p, 0x20) = U32(b, 0);
			U32(p, 0x2C) = U32(b, 8);
			U32(p, 0x10) = U32(a, 0x10);
			U32(p, 4) = U32(a, 0x10);
			U32(p, 0xC) = 0x3E54F808;
			U32(p, 0x28) = U32(b, 0x10);
			U32(p, 0x1C) = U32(b, 0x10);
			int32_t d = (((int32_t)za + (int32_t)zb) >> 3) + S32(h, 0x2C);
			U32(p, 0x18) = 0xB9F838;
			U16(p, 0x24) = 0xF808;
			U16(p, 0x30) = 0xF838;
			U32(p, 0) = 0xC000000;
			U8(p, 7) = 0x3E;
			if (d < 0)
				d = 0;
			x::SSIGPU_InsertPrimDepthKeys(U32(h, 0) + (uint32_t)d * 4, p, 0, 0, 0, 0);
			p = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = p + 0x34;
			U32(p, 0x14) = U32(a, 8);
			U32(p, 0x2C) = U32(b, 8);
			U32(p, 8) = U32(a, 0);
			U32(p, 0x10) = U32(a, 0x10);
			U32(p, 4) = U32(a, 0x10);
			U32(p, 0x20) = U32(b, 0);
			U32(p, 0x28) = U32(b, 0x10);
			U32(p, 0x1C) = U32(b, 0x10);
			d = (((int32_t)S16(a, 4) + (int32_t)S16(b, 4)) >> 3) + S32(h, 0x2C);
			U32(p, 0xC) = 0x3E54F908;
			U32(p, 0x18) = 0xD9F938;
			U16(p, 0x24) = 0xF908;
			U16(p, 0x30) = 0xF938;
			U32(p, 0) = 0xC000000;
			U8(p, 7) = 0x3E;
			if (d < 0)
				d = 0;
			x::SSIGPU_InsertPrimDepthKeys(U32(h, 0) + (uint32_t)d * 4, p, 0, 0, 0, 0);
		}
		return 0; // void
	}

	// 0x7AF860 (sub_7AF860): edges of projected segment s (screen xy +0, OTZ +4, width +6) towards
	// n: the unit normal of s -> n (length from the table 0x157BA68), mitred with the previous
	// direction prev (dir receives the new one); s +0 / +8 = the two edge points (y +2 / +0xA);
	// an OTZ < 0 is skipped; no prev and no length: OTZ = -1
	uint32_t __cdecl ud_7AF860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t s = a1, n = a2, prev = a3, dir = a4;
		if (S16(s, 4) < 0)
			return 0; // void
		int32_t ox = 0, oy = 0;
		if (n != 0)
		{
			if (S16(n, 4) < 0)
				return 0; // void
			const int32_t dx = (int32_t)S16(n, 0) - (int32_t)S16(s, 0);
			const int32_t ndy = (int32_t)S16(s, 2) - (int32_t)S16(n, 2);
			int32_t adx = dx < 0 ? -dx : dx;
			if (adx >= 0x80)
				adx = 0x7F;
			int32_t ady = ndy < 0 ? -ndy : ndy;
			if (ady >= 0x80)
				ady = 0x7F;
			const int32_t len = MEM<uint8_t>(0x157BA68 + (uint32_t)(shl32(ady, 7) + adx));
			int32_t px, py;
			if (prev != 0)
			{
				if (len == 0)
				{
					const int16_t p0 = S16(prev, 0);
					const int16_t p1 = S16(prev, 2);
					U16(dir, 0) = (uint16_t)p0;
					U16(dir, 2) = (uint16_t)p1;
					px = p0;
					py = p1;
				}
				else
				{
					const int32_t cx = shl32(ndy, 12) / len;
					const int32_t cy = shl32(dx, 12) / len;
					int32_t sx = cx + S16(prev, 0);
					int32_t sy = cy + S16(prev, 2);
					U16(dir, 0) = (uint16_t)cx;
					U16(dir, 2) = (uint16_t)cy;
					if (sx == 0 && sy == 0)
					{
						sx = cx * 2;
						sy = cy * 2;
					}
					const int32_t sq = (int32_t)((uint32_t)mul32(sx, sx) + (uint32_t)mul32(sy, sy));
					const int32_t k = 0x4000 - (sq >> 13);
					px = mul32(k, sx) >> 13;
					py = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0)
				{
					U16(s, 4) = 0xFFFF;
					return 0; // void
				}
				px = shl32(ndy, 12) / len;
				py = shl32(dx, 12) / len;
				U16(dir, 0) = (uint16_t)px;
				U16(dir, 2) = (uint16_t)py;
			}
			const int32_t w = (int32_t)S16(n, 6) / ((int32_t)S16(s, 4) + 0x80);
			ox = mul32(w, px) >> 12;
			oy = mul32(w, py) >> 12;
		}
		const uint16_t x0 = U16(s, 0);
		U16(s, 0) = (uint16_t)(x0 - (uint16_t)ox);
		U16(s, 8) = (uint16_t)(x0 + (uint16_t)ox);
		const uint16_t y0 = U16(s, 2);
		U16(s, 0xA) = (uint16_t)(y0 + (uint16_t)oy);
		U16(s, 2) = (uint16_t)(y0 - (uint16_t)oy);
		return 0; // void
	}

	// 0x7AF9F0 (sub_7AF9F0): advances every chain: fade -0x100 per tick (0: the chain and its segments
	// freed); while the head's progress +0x16 < 0x1000: the head drifts (+0x18.., jittered by the
	// module LCG, sometimes kicked), a new segment at the head's point between its start and end
	// (GTE GPF / GPL lerp) with the spread +0x20 scaled by sin(progress / 2), a branch chain (0x7AFDD0)
	// while progress < 0xC00 and the LCG allows (branches left +0x26); segments move by their velocity
	// and fade (+0x10 += +0x12, floor 0); leading segments whose gradient fell to 0x400 are freed
	uint32_t __cdecl ud_7AF9F0(void)
	{
		// stack: +0 the new segment's point (at[3] never written by the GTE path, overwritten by the
		// head's width), +0x10 the branch record (+0x2C never written, never read)
		alignas(4) uint8_t loc[0x40] = {};
		const uint32_t AT = P(loc);
		const uint32_t R = AT + 0x10 - 8;
		uint32_t c = MEM<uint32_t>(BOLT_BASE) + 0x3000;
		for (int n = 0x40; n != 0; n--, c += 0x30)
		{
			uint32_t s0 = U32(c, 0);
			if (s0 == 0)
				continue;
			U16(c, 0x2E) = (uint16_t)(U16(c, 0x2E) + 0xFF00);
			if (S16(c, 0x2E) <= 0)
			{
				uint32_t s = s0;
				do
				{
					MEM<uint32_t>(0x25DF314) = s0;
					U16(s, 6) = 0xFFFF;
					s = U32(s, 0x14);
				} while (s != 0);
				U32(c, 0) = 0;
				MEM<uint32_t>(0x25DF318) = c;
				continue;
			}
			if (S16(c, 0x16) < 0x1000)
			{
				const uint16_t prog = (uint16_t)(U16(c, 0x16) + U16(c, 0x1E));
				U16(c, 8) = (uint16_t)(U16(c, 8) + U16(c, 0x18));
				U16(c, 0xA) = (uint16_t)(U16(c, 0xA) + U16(c, 0x1A));
				U16(c, 0x16) = prog;
				U16(c, 0xC) = (uint16_t)(U16(c, 0xC) + U16(c, 0x1C));
				for (uint32_t o = 0x18; o <= 0x1C; o += 2)
				{
					const int32_t r = (int32_t)ud_7AFEA0();
					U16(c, o) = (uint16_t)(U16(c, o) + (uint16_t)((r >> (MEM<uint8_t>(0x25DF31C) & 31)) - (int32_t)MEM<uint16_t>(0x25DF31E)));
				}
				if ((int32_t)ud_7AFEA0() < (int32_t)MEM<int16_t>(0x25DF320))
				{
					for (uint32_t o = 8; o <= 0xC; o += 2)
					{
						const int32_t r = (int32_t)ud_7AFEA0();
						const uint32_t sh = (uint32_t)(MEM<uint8_t>(0x25DF31C) - 2) & 31;
						U16(c, o) = (uint16_t)(U16(c, o) + (uint16_t)((r >> sh) - (int32_t)(uint16_t)(MEM<uint16_t>(0x25DF31E) << 2)));
					}
				}
				if (S16(c, 0x16) >= 0x1000)
				{
					U16(c, 0x16) = 0x1000;
					U32(AT, 0) = U32(c, 0x10);
					U32(AT, 4) = U32(c, 0x14);
				}
				else
				{
					x::set_dword_1CA8A30((uint32_t)(0x1000 - (int32_t)S16(c, 0x16)));
					gx::sub_45E0B0(c + 8);
					gx::sub_45E9D0();
					x::set_dword_1CA8A30((uint32_t)(int32_t)S16(c, 0x16));
					gx::sub_45E0B0(c + 0x10);
					gx::sub_45EBF0();
					x::GTE_StoreIR123(AT);
				}
				const uint32_t s = ud_7AFD80();
				if (s == 0)
				{
					U16(c, 0x16) = 0x1000;
					return 0; // void
				}
				U32(s, 0x14) = 0;
				U32(s, 0) = U32(AT, 0);
				U32(s, 4) = U32(AT, 4);
				U16(s, 6) = U16(c, 0xE);
				U32(s, 0x10) = U32(c, 0x28);
				U32(U32(c, 4), 0x14) = s;
				const int32_t sn = MEM<int16_t>(0x157FA68 + (uint32_t)((S16(c, 0x16) >> 1) * 2));
				U32(c, 4) = s;
				x::set_dword_1CA8A30((uint32_t)sn);
				gx::sub_45E0B0(c + 0x20);
				gx::sub_45E9D0();
				U16(c, 0x2C) = (uint16_t)(U16(c, 0x2C) + 1);
				x::GTE_StoreIR123(s + 8);
				if (S16(c, 0x16) < 0xC00 && (int32_t)ud_7AFEA0() < shl32(S16(c, 0x26), 13))
				{
					U32(R, 8) = U32(AT, 0);
					U32(R, 0xC) = U32(AT, 4);
					U16(R, 0xE) = U16(c, 0xE);
					U16(R, 0x10) = U16(c, 0x10);
					U16(R, 0x12) = U16(c, 0x12);
					U16(R, 0x14) = U16(c, 0x14);
					U16(R, 0x16) = U16(c, 0x16);
					U16(R, 0x18) = (uint16_t)((ud_7AFEA0() & 0x1FF) + U16(c, 0x18) - 0x100);
					U16(R, 0x1A) = (uint16_t)((ud_7AFEA0() & 0x1FF) + U16(c, 0x1A) - 0x100);
					const uint32_t r = ud_7AFEA0();
					U16(R, 0x24) = U16(c, 0x24);
					U32(R, 0x20) = U32(c, 0x20);
					U16(R, 0x1C) = (uint16_t)((r & 0x1FF) + U16(c, 0x1C) - 0x100);
					U16(R, 0x1E) = U16(c, 0x1E);
					U16(R, 0x26) = 0;
					U32(R, 0x28) = U32(c, 0x28);
					U16(R, 0x2E) = U16(c, 0x2E);
					const uint32_t b = ud_7AFDD0(R);
					if (b != 0)
					{
						U32(b, 8) = U32(s, 8);
						U32(b, 0xC) = U32(s, 0xC);
					}
					U16(c, 0x26) = (uint16_t)(U16(c, 0x26) - 1);
				}
			}
			for (uint32_t s = U32(c, 0); s != 0; s = U32(s, 0x14))
			{
				U16(s, 0x10) = (uint16_t)(U16(s, 0x10) + U16(s, 0x12));
				if (S16(s, 0x10) < 0)
					U16(s, 0x10) = 0;
				U16(s, 0) = (uint16_t)(U16(s, 0) + U16(s, 8));
				U16(s, 2) = (uint16_t)(U16(s, 2) + U16(s, 0xA));
				U16(s, 4) = (uint16_t)(U16(s, 4) + U16(s, 0xC));
			}
			uint32_t s = U32(c, 0);
			if (S16(s, 0x10) > 0x400)
				continue;
			for (;;)
			{
				const uint32_t nx = U32(s, 0x14);
				if (S16(nx, 0x10) > 0x400)
					break;
				U16(c, 0x2C) = (uint16_t)(U16(c, 0x2C) - 1);
				MEM<uint32_t>(0x25DF314) = s;
				U16(s, 6) = 0xFFFF;
				U32(c, 0) = nx;
				if (S16(c, 0x2C) < 2)
				{
					U32(c, 0) = 0;
					MEM<uint32_t>(0x25DF318) = c;
					break;
				}
				s = nx;
				if (S16(s, 0x10) > 0x400)
					break;
			}
		}
		return 0; // void
	}

	// 0x7AFD80 (sub_7AFD80): a free segment (width -1) from the cursor 0x25DF314 on (else from the
	// first; 0 when none of the 0x200 is free), the cursor moves past it
	uint32_t __cdecl ud_7AFD80(void)
	{
		uint32_t p = MEM<uint32_t>(0x25DF314);
		const uint32_t base = MEM<uint32_t>(BOLT_BASE);
		if (S16(p, 6) != -1)
		{
			p = base;
			if (S16(base, 6) != -1)
			{
				int32_t n = 0x200;
				for (;;)
				{
					p += 0x18;
					if (--n == 0)
						return 0;
					if (S16(p, 6) == -1)
						break;
				}
			}
		}
		MEM<uint32_t>(0x25DF314) = p >= base + 0x2FE8 ? base : p + 0x18;
		return p;
	}

	// 0x7AFDD0 (sub_7AFDD0): a new chain from the record a1 (+8 head / start, +0x10 end, +0x18 drift,
	// +0x1C, +0x20 spread, +0x24 / +0x26, +0x28 colour, +0x2E fade) with its first segment; returns
	// the segment (0 when no chain or segment is free); flag byte 0x25DF310 = 2
	uint32_t __cdecl ud_7AFDD0(uint32_t a1)
	{
		const uint32_t base = MEM<uint32_t>(BOLT_BASE);
		uint32_t c = MEM<uint32_t>(0x25DF318);
		if (U32(c, 0) != 0)
		{
			c = base + 0x3000;
			if (U32(c, 0) != 0)
			{
				int32_t n = 0x40;
				for (;;)
				{
					c += 0x30;
					if (--n == 0)
						return 0;
					if (U32(c, 0) == 0)
						break;
				}
			}
		}
		MEM<uint32_t>(0x25DF318) = c >= base + 0x3BD0 ? base + 0x3000 : c + 0x30;
		const uint32_t s = ud_7AFD80();
		if (s == 0)
			return 0;
		const uint32_t rec = a1;
		U32(s, 0x14) = 0;
		U32(s, 8) = 0;
		U32(s, 0) = U32(rec, 8);
		U32(c, 8) = U32(rec, 8);
		U32(s, 0xC) = 0;
		U32(c, 0x10) = U32(rec, 0x10);
		U32(c, 0x14) = U32(rec, 0x14);
		U32(c, 0x18) = U32(rec, 0x18);
		U32(s, 4) = U32(rec, 0xC);
		U32(c, 0x1C) = U32(rec, 0x1C);
		U32(c, 0x20) = U32(rec, 0x20);
		U32(c, 0xC) = U32(rec, 0xC);
		U16(c, 0x2C) = 1;
		U32(c, 4) = s;
		U32(c, 0) = s;
		U32(c, 0x28) = U32(rec, 0x28);
		U32(c, 0x24) = U32(rec, 0x24);
		U16(c, 0x2E) = U16(rec, 0x2E);
		U32(s, 0x10) = U32(rec, 0x28);
		MEM<uint8_t>(0x25DF310) = 2;
		return s;
	}

	// 0x7AFEA0 (sub_7AFEA0): the module LCG 0x25DF324 = (seed * 125 + 14) & 0x7FFF
	uint32_t __cdecl ud_7AFEA0(void)
	{
		const uint32_t v = (MEM<uint32_t>(0x25DF324) * 125u + 0xE) & 0x7FFF;
		MEM<uint32_t>(0x25DF324) = v;
		return v;
	}

	// 0x7BD020 (sub_7BD020): draws the model copy of block a1 (0x25DF348): the entity (s16)+0xA8's
	// matrix +0x40 is copied to +0x20, its bone matrices rebuilt from its pose (0x508C90), the copy
	// turned by the camera (columns -> +0x40, translation -> +0x54); both model parts (entity +0x64,
	// and [+0x78]+4 when present) drawn by 0x7BD110 into OT base+0x44 (shift 4); the bone matrices
	// are rebuilt again after
	uint32_t __cdecl ud_7AFEC0(uint32_t a1)
	{
		const uint32_t blk = a1;
		const int32_t idx = S16(blk, 0xA8);
		const uint32_t ent = 0x1D972C0 + (uint32_t)(idx * 0x9C);
		memcpy((void *)(blk + 0x20), (const void *)(ent + 0x40), 0x20); // rep movsd
		const uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x40);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x42);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(blk + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(blk + 0x54);
		ud_7AFFB0(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			ud_7AFFB0(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x7BD110 (sub_7BD110; Raldo rt_8A95F0 with the block 4 bytes lower, 16-byte vertex records and a
	// depth bias): morph renderer of a battle model (a1 = geometry {+0 bone matrices - 0x10, +4 object
	// table}, a2 = OT, a3 = depth shift, a4 = block 0x25DF348 {+0 rotation, +0x40 camera matrix, +0x60
	// -> {+4 vertex scratch}, +0x64 entity, +0x68 packet cursor pointer, +0x70 pivot, +0x98 / +0x9A /
	// +0x9C angles, +0xA0 morph, +0xA2..+0xA6 radii, +0xD0 depth bias}): per visible object (entity +0x7C
	// mask) every vertex is put in world space by its bone (light matrix + background vector), taken
	// relative to the pivot, and morphed between itself (weight 0x1000 - morph) and its direction
	// scaled by the radii (weight morph) through the GTE interpolation (0x45E0B0 / 0x45E9D0 /
	// 0x45EBF0), turned by the rotation (colour matrix, ColorV0) and put back at the pivot (16-byte
	// scratch records {x, y, z, pad, sxy, sz}); projected (RTPS), then back-face-culled textured
	// triangles (POLY_FT3, 7 words) and quads (POLY_FT4, 9 words) tinted with the entity colour
	// (+0x28), OT key + bias clamped at 0, inserted with SSIGPU_InsertPrimDepthKeys
	uint32_t __cdecl ud_7AFFB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t blk = a4;
		const uint32_t ent = U32(blk, 0x64);
		const uint32_t verts = U32(U32(blk, 0x60), 4);  // vertex scratch (ebx)
		uint32_t cursor = U32(U32(blk, 0x68), 0);       // packet cursor (local +0x1C)
		const uint32_t bones = U32(a1, 0) + 0x10;       // local +0x30
		uint32_t tab = U32(a1, 4);                      // object table {count, offsets[count]}
		const int32_t count = S32(tab, 0);              // local +0x2C
		const uint32_t w = x::Field_Alloc(0xA8);        // scratch block (ebp)
		tab += 4;
		U16(w, 0x9E) = U16(blk, 0xA0);                  // morph weight
		U16(w, 0x9C) = (uint16_t)(0x1000 - U16(blk, 0xA0));
		U32(w, 0x5C) = U32(blk, 0x70);                  // pivot x, y
		U32(w, 0x60) = U32(blk, 0x74);                  // pivot z (+ pad)
		U16(w, 0xA4) = U16(blk, 0xA6);                  // radii
		U16(w, 0xA0) = U16(blk, 0xA2);
		U16(w, 0xA6) = U16(blk, 0xC0);                  // depth bias
		const uint32_t rgb = U32(ent, 0x28) & 0xFFFFFF;
		U16(w, 0xA2) = U16(blk, 0xA4);
		U32(w, 0x94) = rgb | 0x2C000000;                // POLY_FT4 code + colour
		memcpy((void *)w, (const void *)blk, 0x20);     // rep movsd: the rotation
		U32(w, 0x90) = rgb | 0x24000000;                // POLY_FT3 code + colour
		U32(w, 0x98) = U32(ent, 0x7C);                  // visible objects
		if (U16(blk, 0x98) != 0)
			x::sub_8DD7E0(w, (uint32_t)(int32_t)S16(blk, 0x98));
		if (U16(blk, 0x9C) != 0)
			x::sub_8DD960(w, (uint32_t)(int32_t)S16(blk, 0x9C));
		if (U16(blk, 0x9A) != 0)
			x::MAG_022_sub_8DD8A0(w, (uint32_t)(int32_t)S16(blk, 0x9A));
		gx::sub_45DEA0(w);
		x::GTE_SetRotMatrix(blk + 0x40);
		x::GTE_SetTransVector(blk + 0x40);
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t obj = U32(a1, 4) + U32(tab, 0);
			tab += 4;
			if (((U32(w, 0x98) >> (i & 31)) & 1) == 0)
				continue;
			// pass 1: world vertex, relative to the pivot, morphed, turned, back
			const uint32_t groups = obj + 2;
			uint32_t p = groups;
			uint32_t wv = verts;
			for (int32_t g = (int32_t)S16(obj, 0); g > 0; g--)
			{
				const int32_t bone = (int32_t)S16(p, 0);
				p += 2;
				const uint32_t m = (uint32_t)(bone * 3 * 16) + bones + 0x10;
				gx::GTE_SetLightMatrix(m);
				gx::GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				const int32_t nv = (int32_t)S16(p, 0);
				p += 2;
				const uint32_t v = w + 0x4C;
				for (int32_t k = nv; k > 0; k--)
				{
					const uint16_t vx = U16(p, 0);
					const uint16_t vy = U16(p, 2);
					p += 4;
					U16(v, 0) = vx;
					U16(w, 0x4E) = vy;
					U16(w, 0x50) = U16(p, 0);
					p += 2;
					x::GTE_LoadV0(v);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(v);
					U16(v, 0) = (uint16_t)(U16(v, 0) - U16(w, 0x5C));
					U16(w, 0x4E) = (uint16_t)(U16(w, 0x4E) - U16(w, 0x5E));
					U16(w, 0x50) = (uint16_t)(U16(w, 0x50) - U16(w, 0x60));
					U32(w, 0x54) = U32(v, 0);
					U32(w, 0x58) = U32(v, 4);
					gx::sub_56BDE0(v, v);
					S16(v, 0) = (int16_t)(mul32(S16(w, 0xA0), S16(v, 0)) / 4096);
					S16(w, 0x4E) = (int16_t)(mul32(S16(w, 0xA2), S16(w, 0x4E)) / 4096);
					S16(w, 0x50) = (int16_t)(mul32(S16(w, 0xA4), S16(w, 0x50)) / 4096);
					x::set_dword_1CA8A30((uint32_t)(int32_t)S16(w, 0x9C));
					gx::sub_45E0B0(w + 0x54);
					gx::sub_45E9D0();
					x::set_dword_1CA8A30((uint32_t)(int32_t)S16(w, 0x9E));
					gx::sub_45E0B0(v);
					gx::sub_45EBF0();
					x::GTE_StoreIR123(v);
					x::GTE_LoadV0(v);
					gx::GTE_MVMVA_ColorV0();
					x::GTE_StoreIR123(v);
					U16(v, 0) = (uint16_t)(U16(v, 0) + U16(w, 0x5C));
					U16(w, 0x4E) = (uint16_t)(U16(w, 0x4E) + U16(w, 0x5E));
					U16(w, 0x50) = (uint16_t)(U16(w, 0x50) + U16(w, 0x60));
					U32(wv, 0) = U32(v, 0);
					U32(wv, 4) = U32(v, 4);
					wv += 0x10;
				}
			}
			// pass 2: project every vertex (sxy -> +8, sz -> +0xC)
			uint32_t q = groups;
			uint32_t vp = verts;
			for (int32_t g = (int32_t)S16(obj, 0); g > 0; g--)
			{
				const int32_t nv = (int32_t)S16(q, 2);
				q += 4;
				if (nv <= 0)
					continue;
				const uint32_t next = q + (uint32_t)(nv * 3 * 2);
				uint32_t zp = vp + 0xC;
				for (int32_t k = nv; k > 0; k--)
				{
					U32(w, 0x34) = U32(vp, 0);
					U32(w, 0x38) = U32(vp, 4);
					x::GTE_LoadV0(w + 0x34);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(zp - 4);
					gx::sub_45E220(zp);
					vp += 0x10;
					zp += 0x10;
				}
				q = next;
			}
			// faces (4-aligned): u16 tri count, u16 quad count, 8 bytes, then the records
			uint32_t f = (q + 3) & 0xFFFFFFFCu;
			const int32_t ntri = (int32_t)S16(f, 0);
			const int32_t nquad = (int32_t)S16(f, 2);
			f += 0xC;
			uint32_t pk = cursor;
			// triangles: 16-byte records {u16 i0,i1,i2, uv2, u32 uv0+clut, u32 uv1+tpage}
			for (int32_t k = ntri; k > 0; k--, f += 0x10)
			{
				const uint32_t i2 = U16(f, 4) & 0xFFF;
				const uint32_t i1 = U16(f, 2) & 0xFFF;
				const uint32_t i0 = U16(f, 0) & 0xFFF;
				U32(w, 0x88) = i2;
				U32(w, 0x84) = i1;
				U32(w, 0x80) = i0;
				const uint32_t s2 = U32(verts + i2 * 16, 8);
				const uint32_t s1 = U32(verts + i1 * 16, 8);
				const uint32_t s0 = U32(verts + i0 * 16, 8);
				U32(w, 0x24) = s0;
				U32(w, 0x28) = s1;
				U32(w, 0x2C) = s2;
				gx::sub_45E160(s0, s1, s2);
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(w + 0x7C);
				if (S32(w, 0x7C) <= 0)
					continue;
				U32(pk, 8) = U32(w, 0x24);
				U32(pk, 0x10) = U32(w, 0x28);
				U32(pk, 0x18) = U32(w, 0x2C);
				U32(pk, 0xC) = U32(f, 8);
				U32(pk, 0) = 0x7000000;
				U32(pk, 0x14) = U32(f, 0xC);
				U16(pk, 0x1C) = U16(f, 6);
				U32(pk, 4) = U32(w, 0x90);
				if (U8(f, 0xF) & 2)
					U8(pk, 7) |= 2;
				const int32_t z1 = S32(verts + U32(w, 0x84) * 16, 0xC);
				const int32_t z0 = S32(verts + U32(w, 0x80) * 16, 0xC);
				const int32_t z2 = S32(verts + U32(w, 0x88) * 16, 0xC);
				const int32_t sum = add32(add32(z1, z0), z2);
				// sum / 3 (magic 0x55555556: high dword + its sign bit)
				int32_t hq = (int32_t)(((int64_t)sum * 0x55555556LL) >> 32);
				hq = add32(hq, (int32_t)((uint32_t)hq >> 31));
				int32_t key = add32(hq >> (a3 & 31), (int32_t)S16(w, 0xA6));
				U32(w, 0x78) = (uint32_t)key;
				if (key < 0)
					U32(w, 0x78) = 0;
				x::SSIGPU_InsertPrimDepthKeys(a2 + U32(w, 0x78) * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, 0);
				pk += 0x20;
			}
			// quads: 20-byte records {u16 i0,i1,i2,i3, u32 uv0+clut, u32 uv1+tpage, u16 uv2, u16 uv3}
			if (nquad > 0)
			{
				f += 4;
				for (int32_t k = nquad; k > 0; k--, f += 0x14)
				{
					const uint32_t i2 = U16(f, 0) & 0xFFF;
					const uint32_t i1 = U16(f, -2) & 0xFFF;
					const uint32_t i0 = U16(f, -4) & 0xFFF;
					U32(w, 0x88) = i2;
					U32(w, 0x84) = i1;
					U32(w, 0x80) = i0;
					const uint32_t s2 = U32(verts + i2 * 16, 8);
					const uint32_t s1 = U32(verts + i1 * 16, 8);
					const uint32_t s0 = U32(verts + i0 * 16, 8);
					U32(w, 0x24) = s0;
					U32(w, 0x28) = s1;
					U32(w, 0x2C) = s2;
					gx::sub_45E160(s0, s1, s2);
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(w + 0x7C);
					if (S32(w, 0x7C) <= 0)
						continue;
					const uint32_t i3 = U16(f, 2) & 0xFFF;
					U32(pk, 8) = U32(w, 0x24);
					U32(w, 0x8C) = i3;
					U32(pk, 0x10) = U32(w, 0x28);
					U32(pk, 0x18) = U32(w, 0x2C);
					U32(pk, 0) = 0x9000000;
					const uint32_t v3 = verts + i3 * 16;
					const uint32_t s3 = U32(v3, 8);
					U32(pk, 0x14) = U32(f, 8);
					U32(w, 0x30) = s3;
					U32(pk, 0x20) = s3;
					U32(pk, 0xC) = U32(f, 4);
					U16(pk, 0x1C) = U16(f, 0xC);
					U16(pk, 0x24) = U16(f, 0xE);
					U32(pk, 4) = U32(w, 0x94);
					if (U8(f, 0xB) & 2)
						U8(pk, 7) |= 2;
					const int32_t z3 = S32(v3, 0xC);
					const int32_t z2 = S32(verts + U32(w, 0x88) * 16, 0xC);
					const int32_t z1 = S32(verts + U32(w, 0x84) * 16, 0xC);
					const int32_t z0 = S32(verts + U32(w, 0x80) * 16, 0xC);
					const int32_t sum = add32(add32(add32(z0, z3), z1), z2);
					int32_t key = add32(sum >> ((a3 + 2) & 31), (int32_t)S16(w, 0xA6));
					U32(w, 0x78) = (uint32_t)key;
					if (key < 0)
						U32(w, 0x78) = 0;
					x::SSIGPU_InsertPrimDepthKeys(a2 + U32(w, 0x78) * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, (uint32_t)z3);
					pk += 0x28;
				}
			}
			cursor = pk;
		}
		U32(U32(blk, 0x68), 0) = cursor;
		x::Field_Free(0xA8);
		return 0; // void
	}

	// 0x7B06A0 (sub_7B06A0): bolt emitter - from tick 0x50 to 0x96 when the table 0x1580A70[tick -
	// 0x52] is set (ticks 0x50 / 0x51 read the words below it): a start point (0, 0, 0x200) turned by
	// random Y / X angles around the caster, an end offset (0, 0, 0x100) turned the same way from
	// the caster raised by the height table at director +0x50 + h / 512 (h = -height(+0x50)); when
	// h / 512 > 0 two bolts between them (0x7B0860)
	uint32_t __cdecl ud_7B06A0(uint32_t a1)
	{
		// stack (entry esp - 0x38): +0 V1 start (pad never written, never read), +8 V2 offset,
		// +0x10 V3 end, +0x18 MATRIX (rotation only read)
		alignas(4) uint8_t loc[0x38] = {};
		const uint32_t L = P(loc);
		const uint32_t V1 = L, V2 = L + 8, V3 = L + 0x10, M = L + 0x18;
		const uint32_t node = a1;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		const int16_t t = S16(node, 0x24);
		if (t < 0x50 || t > 0x96)
			return 0; // void
		const int16_t k = (int16_t)(t - 0x52);
		if (MEM<int16_t>(0x1580A70 + (uint32_t)((int32_t)k * 2)) == 0)
			return 0; // void
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, x::CrtRand() & 0xFFF);
		x::sub_8DD7E0(M, x::CrtRand() & 0xFFF);
		U16(V1, 0) = 0;
		U16(V1, 2) = 0;
		U16(V1, 4) = 0x200;
		x::matrixMultiplyVector(M, V1, V1);
		U16(V1, 0) = (uint16_t)(U16(V1, 0) + U16(c, 0x1C));
		U16(V1, 2) = (uint16_t)(U16(V1, 2) + U16(c, 0x1E));
		U16(V1, 4) = (uint16_t)(U16(V1, 4) + U16(c, 0x20));
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, x::CrtRand() & 0xFFF);
		x::sub_8DD7E0(M, x::CrtRand() & 0xFFF);
		U16(V2, 0) = 0;
		U16(V2, 2) = 0;
		U16(V2, 4) = 0x100;
		x::matrixMultiplyVector(M, V2, V2);
		const int16_t hv = (int16_t)ud_7B0840((uint32_t)U16(MEM<uint32_t>(DIR_PTR), 0x50));
		const int32_t q = hv / 512;
		const uint16_t step = (uint16_t)(U16(MEM<uint32_t>(DIR_PTR), 0x50) + (uint16_t)q);
		ud_7AC060(node, V3, (uint32_t)step);
		U16(V3, 0) = (uint16_t)(U16(V3, 0) + U16(V2, 0));
		U16(V3, 2) = (uint16_t)(U16(V3, 2) + U16(V2, 2));
		U16(V3, 4) = (uint16_t)(U16(V3, 4) + U16(V2, 4));
		if ((int16_t)q > 0)
			ud_7B0860(V1, V3, (uint32_t)q);
		return 0; // void
	}

	// 0x7B0840 (sub_7B0840): -0x157B8C4[min(a1, 0x77)] (16 bits)
	uint32_t __cdecl ud_7B0840(uint32_t a1)
	{
		int16_t k = (int16_t)a1;
		if (k >= 0x78)
			k = 0x77;
		return (uint32_t)(uint16_t)(-(int32_t)MEM<int16_t>(0x157B8C4 + (uint32_t)((int32_t)k * 2)));
	}

	// 0x7B0860 (sub_7B0860): two bolts from a1 to a2 (progress step 0x1000 / max(a3, 1)): each a
	// chain record with a random sideways drift (radius 0x400..0x7FF at a random angle, scaled by
	// rand / 128 / 4096), random spread (+-0x80 per axis), width 0xBB8, 2 branches, colour word
	// 0xFE003000, fade 0x1000 (0x7AFDD0); rand() order: angle, radius, scale, spread x / y / z
	uint32_t __cdecl ud_7B0860(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// the record (entry esp - 0x30): +0..+7 and +0x2C never written (not read by 0x7AFDD0)
		alignas(4) uint8_t rec[0x30] = {};
		const uint32_t R = P(rec);
		int16_t k = (int16_t)a3;
		if (k <= 0)
			k = 1;
		const int32_t step = 0x1000 / (int32_t)k;
		for (int i = 2; i != 0; i--)
		{
			const uint32_t ang = x::CrtRand();
			const int32_t rad = (int32_t)(x::CrtRand() & 0x3FF) + 0x400;
			int32_t sx = mul32((int32_t)x::computeCosine(ang), rad) / 4096;
			int32_t sz = mul32((int32_t)x::computeSin(ang), rad) / 4096;
			U16(R, 0xA) = U16(a1, 2);
			U16(R, 8) = U16(a1, 0);
			U16(R, 0xC) = U16(a1, 4);
			U16(R, 0x10) = U16(a2, 0);
			U16(R, 0x12) = U16(a2, 2);
			U16(R, 0x14) = U16(a2, 4);
			const int32_t m = shl32((int32_t)x::CrtRand(), 8) >> 15;
			sx = mul32(sx, m) >> 12;
			sz = mul32(sz, m) >> 12;
			U16(R, 0x18) = (uint16_t)sx;
			U16(R, 0x1A) = 0;
			U16(R, 0x1C) = (uint16_t)sz;
			U16(R, 0x24) = 0;
			U16(R, 0x22) = 0;
			U16(R, 0x20) = 0;
			U16(R, 0xE) = 0xBB8;
			U16(R, 0x16) = 0;
			U16(R, 0x1E) = (uint16_t)step;
			U16(R, 0x26) = 2;
			U16(R, 0x28) = 0x3000;
			U16(R, 0x2A) = 0xFE00;
			U16(R, 0x20) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
			U16(R, 0x22) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
			U16(R, 0x24) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
			U16(R, 0x2E) = 0x1000;
			ud_7AFDD0(R);
		}
		return 0; // void
	}

	// 0x7B09E0 (sub_7B09E0): junction state 0 - the target's model copy block 0x25D4948 (0x7B4480:
	// arena 0x25D4AF8, scratch 0x25DE428), target hidden (flag bit 2) and flag bit 11 on, caster shown
	// (bit 2 cleared) and bit 11 on, the bolts reset (0x7B43F0), the actor sequence 0x1749C64
	// (0x7B0A90: task 0x7B0AE0) at the caster's position (+0x290/+0x294), sound 0x157B2E4, next state
	uint32_t __cdecl ud_7B09E0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint8_t ti = U8(node, 0x2D);
		const uint32_t t = 0x1D972C0 + (uint32_t)ti * 0x9C;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		ud_7B4480(MODEL_BLOCK, 0x25D4AF8, 0x25DE428, t, (uint32_t)ti);
		U8(t, 0) |= 4;
		U16(c, 0) &= 0xFFFB;
		U8(t, 1) |= 8;
		U8(c, 1) |= 8;
		ud_7B43F0();
		const uint32_t k = a_73C100(node, 0x7B0AE0, 0x1749C64, 0, 0xF, 4);
		U32(k, 0x290) = U32(c, 0x1C);
		U32(k, 0x294) = U32(c, 0x20);
		x::BdPlaySE(0x157B2E4, 0, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl ud_7B0B40(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			ud_7B1650(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl ud_7B0D20(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25DF308);
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
	uint32_t __cdecl ud_7B0FA0(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25DF308);
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
	uint32_t __cdecl ud_7B1650(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25DF308, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			ud_7B4330();
			ud_7B19E0();
			// 30 fps layer: see mag080_ultimecia_junction_held.inc
			FX_HELD(held_note_system(sys);)
			ud_7B18A0();
			ud_7B16E0();
			sys = U32(0x25DF308, 0);
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
		U16(0x25DF2F0, 0) = (uint16_t)(U16(0x25DF2F0, 0) + c14);
		U16(0x25DF008, 0) = (uint16_t)(U16(0x25DF008, 0) + c16);
		return done;
	}

	// 0x8C94D0 (copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl ud_7B16E0(void)
	{
		uint32_t act = U32(U32(0x25DF308, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25DF308, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					ud_7B1740(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9530 (copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl ud_7B1740(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = ud_7AC420(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = ud_7AC420(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
	uint32_t __cdecl ud_7B18A0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25DF308), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25DF308);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						ud_7B1960(act);   // (the original also pushes the bone entry, unused)
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
							ud_7B1960(act);
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
	uint32_t __cdecl ud_7B1960(uint32_t a1)
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

	// 0x848EB0 (copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl ud_7B19E0(void)
	{
		uint32_t act = U32(U32(0x25DF308, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				ud_7B2CB0(act);
			else if (type == 1)
				ud_7B1A20(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl ud_7B1A20(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25DF308);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (ud_7B2B60(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25DF308);
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
	uint32_t __cdecl ud_7B2A50(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25DF308, 0);                  // actor state block
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
	uint32_t __cdecl ud_7B2B60(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				ud_7B2BE0(a1, a2);
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
			ud_7B2BE0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			ud_7B2BE0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x828D30 (copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl ud_7B2BE0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return ud_7B2C10(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x828D60 (copy of db_84A0E0 0x84A0E0: copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl ud_7B2C10(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25DF00C);
		int32_t idx = MEM<int16_t>(0x25DCF30);
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
			MEM<uint16_t>(0x25DCF30) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25DCF30) = 0;
		return slot;
	}

	// 0x828E00 (copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl ud_7B2CB0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x7B3660, a1);
		else if (mode == 4)
			callp(0x7B37B0, a1);
		ud_7B2D30(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x828E80 (copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl ud_7B2D30(uint32_t a1)
	{
		uint32_t sys = U32(0x25DF308, 0);
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
				ud_7B2DB0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			ud_7B2DB0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x828F00 (copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl ud_7B2DB0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25DF2F8, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25DF2F8, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = a_7387B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25DF308, 0);
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
		ud_7B42A0(node, desc);
		U32(0x25DF2F8, 0) = U32(0x25DF2F8, 0) + 0x50;
		return 0; // void
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl ud_7B37B0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25DF308, 0);            // ecx
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
				uint32_t g = U32(0x25DF308, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25DF308, 0);
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
	uint32_t __cdecl ud_7B42A0(uint32_t a1, uint32_t a2)
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
			ud_7B2C10(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			ud_7B2C10(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			ud_7B2C10(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			ud_7B2C10(obj, id, variant);
		}
		return 0; // void
	}

	// 0x82A480 (copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via ud_7B2C10(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl ud_7B4330(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25DF308);  // eax (re-read only after the calls)
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
					ud_7B2C10(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25DF308);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				ud_7B2C10(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25DF308);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl ud_7B43C0(uint32_t a1)
	{
		if (ud_7B1650(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7B43F0 (sub_7B43F0): the bolts reset with jitter shift 5, keep 0, depth -0x20 (0x7B4400)
	uint32_t __cdecl ud_7B43F0(void)
	{
		return ud_7B4400(5, 0, (uint32_t)-0x20);
	}

	// 0x7B4400 (sub_7B4400): the bolt jitter parameters (shift word 0x25DF31C = 15 - a1, keep
	// 0x25DF320 = a2, half 0x25DF31E = (1 << a1) / 2, depth 0x25DF322 = a3), flag byte 0x25DF310 = 0,
	// every segment and chain free, both cursors at their first entry
	uint32_t __cdecl ud_7B4400(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		MEM<uint8_t>(0x25DF310) = 0;
		MEM<uint16_t>(0x25DF31C) = (uint16_t)(0xF - a1);
		const int32_t one = (int32_t)(1u << (a1 & 31));
		MEM<uint16_t>(0x25DF320) = (uint16_t)a2;
		MEM<uint16_t>(0x25DF322) = (uint16_t)a3;
		const uint32_t base = MEM<uint32_t>(BOLT_BASE);
		MEM<uint16_t>(0x25DF31E) = (uint16_t)((one - (one >> 31)) >> 1);
		MEM<uint32_t>(0x25DF314) = base;
		uint32_t p = base + 6;
		for (int n = 0x200; n != 0; n--, p += 0x18)
			MEM<uint16_t>(p) = 0xFFFF;
		uint32_t c = base + 0x3000;
		MEM<uint32_t>(0x25DF318) = c;
		for (int n = 0x40; n != 0; n--, c += 0x30)
			U32(c, 0) = 0;
		return c;
	}

	// 0x7BD890 (sub_7BD890): model block set-up - a1 (0xC4 bytes) cleared; +0x64 = entity a4, +0x68 /
	// +0x6C = the battle packet cursor's address 0x1D8E054, +0xA8 = (u16)a5 entity index, +0x60 = the
	// render context a3 (+4 = a2, depth offset words +0x14..+0x1A = 0, 0, 0x140, 0, colour bytes
	// +0x1C..+0x1E = 0x80, +0x20 = -1, +0x24 = 0)
	uint32_t __cdecl ud_7B4480(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		x::MAG_007_sub_8DCC00(a1, 0xC4);
		U32(a1, 0x64) = a4;
		U32(a1, 0x68) = 0x1D8E054;
		U32(a1, 0x6C) = 0x1D8E054;
		U16(a1, 0xA8) = (uint16_t)a5;
		U32(a3, 4) = a2;
		U32(a1, 0x60) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFFu;
		U16(a3, 0x24) = 0;
		return 0; // void
	}

	// 0x7B44F0 (sub_7B44F0): junction state 1 - at director cue 3 (0x7ABC90): the prim-model task
	// 0x7B4540 (model 0x1757B2C, anim 0xD8, mode 1 / 0, 0x7ABF40), wait +0x9C = 2, next state
	uint32_t __cdecl ud_7B44F0(uint32_t a1)
	{
		if (a_73B7E0(3) == 0)
			return 0; // void
		ud_7ABF40(a1, 0x7B4540, 0x1757B2C, 0xD8, 1, 0);
		U16(a1, 0x9C) = 2;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7B4540 (sub_7B4540): prim-model TASK (the junction model) - states {0x7B45B0 set-up,
	// 0x7B4620 play, 0x7B4640 ret}, then director +0x10/+0x14 = its position
	uint32_t __cdecl ud_7B4540(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7B45B0;
		states[1] = 0x7B4620;
		states[2] = 0x7B4640; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t w0 = U32(node, 0x1C);
		const uint32_t w1 = U32(node, 0x20);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		U32(d, 0x10) = w0;
		U32(d, 0x14) = w1;
		const uint8_t status = U8(node, 0x26);
		return task_end(node, status);
	}

	// 0x7B45B0 (sub_7B45B0): junction model state 0 - decodes the prim layout, scale 1.0, at the
	// caster's position, depth offset +0x88 = -0x400, first model step (0x7AC0B0), next state
	uint32_t __cdecl ud_7B45B0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t w0 = U32(c, 0x1C);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		const uint32_t w1 = U32(c, 0x20);
		U32(node, 0x1C) = w0;
		U32(node, 0x20) = w1;
		U16(node, 0x88) = 0xFC00;
		ud_7AC0B0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x72AAF0 (097 sub_72AAF0 and copies 0x731570/0x731920/0x731A20, 098/099/100 copies):
	// prim-model task state handler - plays one frame (b_72A720); when the play ended (0) sets
	// the finished flag +0x26 bit0 and advances +0x29
	uint32_t __cdecl ud_7B4620(uint32_t a1)
	{
		const uint32_t r = ud_7AC0B0(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8CE1E0 (copy of t_7680B0 0x7680B0: module 090 sub_7680B0): creature state 11 - counts +0x54 down, next state at 0.
	uint32_t __cdecl ud_7B4650(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x9C) = (uint16_t)(U16(node, 0x9C) - 1);
		if (S16(node, 0x9C) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x7B4670 (sub_7B4670): junction state 3 - flag 8 (the copy is drawn), the target hidden (bit 2),
	// the copy's bone anchor (0x7B4750), copy scale 0x200 (0x25D49EA/EC/EE), its matrix reset
	// (MAG_022_sub_8DD770), the caster hidden, the copy's anchor 0x25D49C0/C4 = the caster's
	// position, morph level 0x25D4A0A = colour level 0x25D49E8 = 0x1000, the actor sequence
	// 0x174F0BC (0x7B0A90) at the caster, sound 0x157B2E8, wait +0x9C = 0x1E, next state
	uint32_t __cdecl ud_7B4670(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		U8(node, 0x26) |= 8;
		U8(t, 0) |= 4;
		ud_7B4750(node, MODEL_BLOCK);
		MEM<uint16_t>(0x25D49EC) = 0x200;
		MEM<uint16_t>(0x25D49EE) = 0x200;
		MEM<uint16_t>(0x25D49EA) = 0x200;
		x::MAG_022_sub_8DD770(MODEL_BLOCK);
		const uint32_t w0 = U32(c, 0x1C);
		const uint32_t w1 = U32(c, 0x20);
		U8(c, 0) |= 4;
		MEM<uint32_t>(0x25D49C0) = w0;
		MEM<uint32_t>(0x25D49C4) = w1;
		MEM<uint16_t>(0x25D4A0A) = 0x1000;
		MEM<uint16_t>(0x25D49E8) = 0x1000;
		const uint32_t k = a_73C100(node, 0x7B0AE0, 0x174F0BC, 0xC, 0xF, 4);
		U32(k, 0x290) = U32(c, 0x1C);
		U32(k, 0x294) = U32(c, 0x20);
		x::BdPlaySE(0x157B2E8, 0, 0x80);
		U16(node, 0x9C) = 0x1E;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BD9B0 (sub_7BD9B0; Raldo rt_8AABA0 with the pivot at +0x70): a2+0x70 = the spawn point (bone
	// 0xF1) of the entity +0x2C of a1, taken with the entity at the origin and its matrix reset
	// (0x8DD770), relative to the entity; position and matrix put back
	uint32_t __cdecl ud_7B4750(uint32_t a1, uint32_t a2)
	{
		const uint32_t e = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		uint32_t mat[8];
		const uint32_t xy = U32(e, 0x1C);
		memcpy(mat, (const void *)(e + 0x40), 0x20);    // rep movsd
		const uint32_t z = U32(e, 0x20);
		U16(e, 0x1C) = 0;
		U16(e, 0x1E) = 0;
		U16(e, 0x20) = 0;
		x::MAG_022_sub_8DD770(e + 0x40);
		const uint32_t blk = a2;
		x::GetEffectSpawnPosition(e, 0xF1, 0, blk + 0x70);
		const uint16_t ex = U16(e, 0x1C), ey = U16(e, 0x1E), ez = U16(e, 0x20);
		U16(blk, 0x70) = (uint16_t)(U16(blk, 0x70) - ex);
		U16(blk, 0x72) = (uint16_t)(U16(blk, 0x72) - ey);
		U16(blk, 0x74) = (uint16_t)(U16(blk, 0x74) - ez);
		memcpy((void *)(e + 0x40), mat, 0x20);          // rep movsd
		U32(e, 0x1C) = xy;
		U32(e, 0x20) = z;
		return 0; // void
	}

	// 0x8CE1E0 (copy of t_7680B0 0x7680B0: module 090 sub_7680B0): creature state 11 - counts +0x54 down, next state at 0.
	uint32_t __cdecl ud_7B4800(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x9C) = (uint16_t)(U16(node, 0x9C) - 1);
		if (S16(node, 0x9C) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x7B4820 (sub_7B4820): junction state 5 - the copy's bone anchor (0x7B4750), its scale grows
	// by 0x40 per tick to 0x500 (then +0x9C = 0, next state)
	uint32_t __cdecl ud_7B4820(uint32_t a1)
	{
		ud_7B4750(a1, MODEL_BLOCK);
		uint16_t v = (uint16_t)(MEM<uint16_t>(0x25D49EC) + 0x40);
		MEM<uint16_t>(0x25D49EC) = v;
		if ((int16_t)v >= 0x500)
		{
			v = 0x500;
			MEM<uint16_t>(0x25D49EC) = v;
			U16(a1, 0x9C) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		MEM<uint16_t>(0x25D49EE) = v;
		MEM<uint16_t>(0x25D49EA) = v;
		return 0; // void
	}

	// 0x7B4880 (sub_7B4880): junction state 6 - +0x9C += 1; up to 0x2D the copy spins (0x25D49E0 +=
	// 0x5B), then 0; at 0x2D the target's chain transformation 0x1A (QueueChainTransformation) and
	// the actor sequence 0x1750660 (0x7B0A90) at the caster; every tick a random jitter (+-0x80,
	// 0x25D49FC/FE/0x25D4A00), the bone anchor (0x7B4750), the scale grows to 0x600, the morph level
	// 0x25D4A0A -= 0x40 (colour level 0x25D49E8 = it; at 0: everything 0, next state)
	uint32_t __cdecl ud_7B4880(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t c = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		U16(node, 0x9C) = (uint16_t)(U16(node, 0x9C) + 1);
		const int16_t k = S16(node, 0x9C);
		if (k <= 0x2D)
			MEM<uint16_t>(0x25D49E0) = (uint16_t)((MEM<uint16_t>(0x25D49E0) + 0x5B) & 0xFFF);
		else
			MEM<uint16_t>(0x25D49E0) = 0;
		if (k == 0x2D)
		{
			gx::QueueChainTransformation(t, 0x1A);
			const uint32_t q = a_73C100(node, 0x7B0AE0, 0x1750660, 0xE, 0xF, 4);
			U32(q, 0x290) = U32(c, 0x1C);
			U32(q, 0x294) = U32(c, 0x20);
		}
		MEM<uint16_t>(0x25D49FC) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
		MEM<uint16_t>(0x25D4A00) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
		MEM<uint16_t>(0x25D49FE) = (uint16_t)((x::CrtRand() & 0xFF) - 0x80);
		ud_7B4750(node, MODEL_BLOCK);
		uint16_t v = (uint16_t)(MEM<uint16_t>(0x25D49EC) + 0x40);
		MEM<uint16_t>(0x25D49EC) = v;
		if ((int16_t)v >= 0x600)
		{
			v = 0x600;
			MEM<uint16_t>(0x25D49EC) = v;
		}
		MEM<uint16_t>(0x25D49EE) = v;
		MEM<uint16_t>(0x25D49EA) = v;
		const uint16_t m = (uint16_t)(MEM<uint16_t>(0x25D4A0A) + 0xFFC0);
		MEM<uint16_t>(0x25D4A0A) = m;
		if ((int16_t)m <= 0)
		{
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			MEM<uint16_t>(0x25D4A0A) = 0;
			MEM<uint16_t>(0x25D49E8) = 0;
			MEM<uint16_t>(0x25D49FC) = 0;
			MEM<uint16_t>(0x25D49FE) = 0;
			MEM<uint16_t>(0x25D4A00) = 0;
			MEM<uint16_t>(0x25D49E0) = 0;
			return 0; // void
		}
		MEM<uint16_t>(0x25D49E8) = m;
		return 0; // void
	}

	// 0x7B49F0 (sub_7B49F0): junction state 7 - flag 8 off (the copy no longer drawn), the target
	// shown (flag bit 2 cleared), next state
	uint32_t __cdecl ud_7B49F0(uint32_t a1)
	{
		const uint32_t t = 0x1D972C0 + (uint32_t)U8(a1, 0x2D) * 0x9C;
		U8(a1, 0x26) &= 0xF7;
		U16(t, 0) &= 0xFFFB;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl ud_7B4A50(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x157B2E0);
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
		{ 0x7AB880, (void *)a_73A0D0, "080 MAG_080_sub_7AB880" },
		{ 0x7AB970, (void *)a_73A170, "080 MAG_080_sub_7AB970" },
		{ 0x7AB9A0, (void *)a_73A1A0, "080 MAG_080_sub_7AB9A0" },
		{ 0x7ABC20, (void *)a_73B790, "080 sub_7ABC20" },
		{ 0x7ABC40, (void *)a_73B640, "080 sub_7ABC40" },
		{ 0x7ABC90, (void *)a_73B7E0, "080 sub_7ABC90" },
		{ 0x7ABD10, (void *)a_73B8A0, "080 sub_7ABD10" },
		{ 0x7ABD30, (void *)a_73B8C0, "080 sub_7ABD30" },
		{ 0x7ABD70, (void *)a_733B70, "080 sub_7ABD70" },
		{ 0x7ABE40, (void *)a_7475A0, "080 sub_7ABE40" },
		{ 0x7ABE60, (void *)a_73A6D0, "080 nullsub_1363" },
		{ 0x7ABE70, (void *)a_73A6D0, "080 nullsub_1364" },
		{ 0x7AD4E0, (void *)a_73D770, "080 sub_7AD4E0" },
		{ 0x7AD730, (void *)a_73D9C0, "080 sub_7AD730" },
		{ 0x7AE680, (void *)a_73A6D0, "080 nullsub_1365" },
		{ 0x7AE690, (void *)a_73A380, "080 sub_7AE690" },
		{ 0x7AE960, (void *)a_7435E0, "080 InitEffectSequenceFromData_c9" },
		{ 0x7AEAA0, (void *)a_743720, "080 sub_7AEAA0" },
		{ 0x7AEF50, (void *)a_743C20, "080 sub_7AEF50" },
		{ 0x7AEF80, (void *)a_73A6D0, "080 nullsub_1366" },
		{ 0x7AEF90, (void *)a_73A6D0, "080 nullsub_1367" },
		{ 0x7AF1B0, (void *)a_73A6D0, "080 nullsub_1375" },
		{ 0x7AF1C0, (void *)a_73A6D0, "080 nullsub_1368" },
		{ 0x7AF1D0, (void *)a_73A380, "080 sub_7AF1D0" },
		{ 0x7AF230, (void *)a_7335D0, "080 sub_7AF230" },
		{ 0x7AF2B0, (void *)a_73A6D0, "080 nullsub_1369" },
		{ 0x7AF2C0, (void *)a_73A380, "080 sub_7AF2C0" },
		{ 0x7AF380, (void *)a_73A6D0, "080 nullsub_1370" },
		{ 0x7B0A90, (void *)a_73C100, "080 sub_7B0A90" },
		{ 0x7B0AE0, (void *)a_73A380, "080 sub_7B0AE0" },
		{ 0x7B0B70, (void *)a_73F990, "080 sub_7B0B70" },
		{ 0x7B0E00, (void *)a_73FC20, "080 sub_7B0E00" },
		{ 0x7B1070, (void *)a_73FE90, "080 sub_7B1070" },
		{ 0x7B1190, (void *)a_73FFB0, "080 sub_7B1190" },
		{ 0x7B13F0, (void *)a_740210, "080 sub_7B13F0" },
		{ 0x7B1AB0, (void *)a_740910, "080 sub_7B1AB0" },
		{ 0x7B2110, (void *)a_740F70, "080 sub_7B2110" },
		{ 0x7B2160, (void *)a_740FC0, "080 sub_7B2160" },
		{ 0x7B21F0, (void *)a_741050, "080 sub_7B21F0" },
		{ 0x7B22C0, (void *)a_741120, "080 sub_7B22C0" },
		{ 0x7B2D00, (void *)a_741B60, "080 sub_7B2D00" },
		{ 0x7B3430, (void *)a_742290, "080 au_re__rand_25" },
		{ 0x7B3460, (void *)a_7422C0, "080 sub_7B3460" },
		{ 0x7B34A0, (void *)a_742300, "080 sub_7B34A0" },
		{ 0x7B34F0, (void *)a_742350, "080 sub_7B34F0" },
		{ 0x7B3540, (void *)a_7423A0, "080 au_re__rand_25_0" },
		{ 0x7B3590, (void *)a_7387B0, "080 sub_7B3590" },
		{ 0x7B3650, (void *)a_7424B0, "080 sub_7B3650" },
		{ 0x7B3660, (void *)a_7424C0, "080 sub_7B3660" },
		{ 0x7B3E40, (void *)a_742CA0, "080 sub_7B3E40" },
		{ 0x7B3E70, (void *)a_742CD0, "080 sub_7B3E70" },
		{ 0x7B3EA0, (void *)a_742D00, "080 sub_7B3EA0" },
		{ 0x7B4050, (void *)a_742EB0, "080 sub_7B4050" },
		{ 0x7B4190, (void *)a_742FF0, "080 sub_7B4190" },
		{ 0x7B43E0, (void *)a_73A6D0, "080 nullsub_1371" },
		{ 0x7B4640, (void *)a_73A6D0, "080 nullsub_1372" },
		{ 0x7B4A20, (void *)a_7475A0, "080 sub_7B4A20" },
		{ 0x7B4A40, (void *)a_73A6D0, "080 nullsub_1373" },
		{ 0x7B4A90, (void *)a_7474B0, "080 MAG_080_sub_7B4A90" },
		{ 0x7B4AB0, (void *)a_7474D0, "080 MAG_080_sub_7B4AB0" },
		{ 0x7B4AD0, (void *)a_73A6D0, "080 nullsub_1374" },
		{ 0x7B4AE0, (void *)a_747500, "080 MAG_080_sub_7B4AE0" },
		{ 0x7B4B20, (void *)a_73A0D0, "080 MAG_080_sub_7B4B20" },
		{ 0x7B4B30, (void *)a_747550, "080 MAG_080_sub_7B4B30" },
		{ 0x7B4B40, (void *)a_747560, "080 MAG_080_sub_7B4B40" },
		{ 0x7B4B70, (void *)a_747590, "080 MAG_080_sub_7B4B70" },
		{ 0x7B4B80, (void *)a_7475A0, "080 MAG_080_sub_7B4B80" },
		{ 0x7B4BA0, (void *)a_73A6D0, "080 nullsub_1362" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x7AB6F0, (void *)ud_7AB6F0, "080 ud_7AB6F0" },
		{ 0x7AB890, (void *)ud_7AB890, "080 ud_7AB890" },
		{ 0x7AB8F0, (void *)ud_7AB8F0, "080 ud_7AB8F0" },
		{ 0x7AB930, (void *)ud_7AB930, "080 ud_7AB930" },
		{ 0x7ABA20, (void *)ud_7ABA20, "080 ud_7ABA20" },
		{ 0x7ABA50, (void *)ud_7ABA50, "080 ud_7ABA50" },
		{ 0x7ABAF0, (void *)ud_7ABAF0, "080 ud_7ABAF0" },
		{ 0x7ABBA0, (void *)ud_7ABBA0, "080 ud_7ABBA0" },
		{ 0x7ABBF0, (void *)ud_7ABBF0, "080 ud_7ABBF0" },
		{ 0x7ABC70, (void *)ud_7ABC70, "080 ud_7ABC70" },
		{ 0x7ABCB0, (void *)ud_7ABCB0, "080 ud_7ABCB0" },
		{ 0x7ABCF0, (void *)ud_7ABCF0, "080 ud_7ABCF0" },
		{ 0x7ABD50, (void *)ud_7ABD50, "080 ud_7ABD50" },
		{ 0x7ABD90, (void *)ud_7ABD90, "080 ud_7ABD90" },
		{ 0x7ABDB0, (void *)ud_7ABDB0, "080 ud_7ABDB0" },
		{ 0x7ABDD0, (void *)ud_7ABDD0, "080 ud_7ABDD0" },
		{ 0x7ABE00, (void *)ud_7ABE00, "080 ud_7ABE00" },
		{ 0x7ABE80, (void *)ud_7ABE80, "080 ud_7ABE80" },
		{ 0x7ABF40, (void *)ud_7ABF40, "080 ud_7ABF40" },
		{ 0x7ABF90, (void *)ud_7ABF90, "080 ud_7ABF90" },
		{ 0x7AC000, (void *)ud_7AC000, "080 ud_7AC000" },
		{ 0x7AC060, (void *)ud_7AC060, "080 ud_7AC060" },
		{ 0x7AC0B0, (void *)ud_7AC0B0, "080 ud_7AC0B0" },
		{ 0x7AC1E0, (void *)ud_7AC1E0, "080 ud_7AC1E0" },
		{ 0x7AC420, (void *)ud_7AC420, "080 ud_7AC420" },
		{ 0x7AC550, (void *)ud_7AC550, "080 ud_7AC550" },
		{ 0x7AC770, (void *)ud_7AC770, "080 ud_7AC770" },
		{ 0x7ACA00, (void *)ud_7ACA00, "080 ud_7ACA00" },
		{ 0x7ACEF0, (void *)ud_7ACEF0, "080 ud_7ACEF0" },
		{ 0x7ADA10, (void *)ud_7ADA10, "080 ud_7ADA10" },
		{ 0x7ADF70, (void *)ud_7ADF70, "080 ud_7ADF70" },
		{ 0x7AE640, (void *)ud_7AE640, "080 ud_7AE640" },
		{ 0x7AE6F0, (void *)ud_7AE6F0, "080 ud_7AE6F0" },
		{ 0x7AE710, (void *)ud_7AE710, "080 ud_7AE710" },
		{ 0x7AE740, (void *)ud_7AE740, "080 ud_7AE740" },
		{ 0x7AE7D0, (void *)ud_7AE7D0, "080 ud_7AE7D0" },
		{ 0x7AE840, (void *)ud_7AE840, "080 ud_7AE840" },
		{ 0x7AEF10, (void *)ud_7AEF10, "080 ud_7AEF10" },
		{ 0x7AEF30, (void *)ud_7AEF30, "080 ud_7AEF30" },
		{ 0x7AEFA0, (void *)ud_7AEFA0, "080 ud_7AEFA0" },
		{ 0x7AEFF0, (void *)ud_7AEFF0, "080 ud_7AEFF0" },
		{ 0x7AF030, (void *)ud_7AF030, "080 ud_7AF030" },
		{ 0x7AF130, (void *)ud_7AF130, "080 ud_7AF130" },
		{ 0x7AF190, (void *)ud_7AF190, "080 ud_7AF190" },
		{ 0x7AF270, (void *)ud_7AF270, "080 ud_7AF270" },
		{ 0x7AF320, (void *)ud_7AF320, "080 ud_7AF320" },
		{ 0x7AF340, (void *)ud_7AF340, "080 ud_7AF340" },
		{ 0x7AF390, (void *)ud_7AF390, "080 ud_7AF390" },
		{ 0x7AF4C0, (void *)ud_7AF4C0, "080 ud_7AF4C0" },
		{ 0x7AF4D0, (void *)ud_7AF4D0, "080 ud_7AF4D0" },
		{ 0x7AF5E0, (void *)ud_7AF5E0, "080 ud_7AF5E0" },
		{ 0x7AF860, (void *)ud_7AF860, "080 ud_7AF860" },
		{ 0x7AF9F0, (void *)ud_7AF9F0, "080 ud_7AF9F0" },
		{ 0x7AFD80, (void *)ud_7AFD80, "080 ud_7AFD80" },
		{ 0x7AFDD0, (void *)ud_7AFDD0, "080 ud_7AFDD0" },
		{ 0x7AFEA0, (void *)ud_7AFEA0, "080 ud_7AFEA0" },
		{ 0x7AFEC0, (void *)ud_7AFEC0, "080 ud_7AFEC0" },
		{ 0x7AFFB0, (void *)ud_7AFFB0, "080 ud_7AFFB0" },
		{ 0x7B06A0, (void *)ud_7B06A0, "080 ud_7B06A0" },
		{ 0x7B0840, (void *)ud_7B0840, "080 ud_7B0840" },
		{ 0x7B0860, (void *)ud_7B0860, "080 ud_7B0860" },
		{ 0x7B09E0, (void *)ud_7B09E0, "080 ud_7B09E0" },
		{ 0x7B0B40, (void *)ud_7B0B40, "080 ud_7B0B40" },
		{ 0x7B0D20, (void *)ud_7B0D20, "080 ud_7B0D20" },
		{ 0x7B0FA0, (void *)ud_7B0FA0, "080 ud_7B0FA0" },
		{ 0x7B1650, (void *)ud_7B1650, "080 ud_7B1650" },
		{ 0x7B16E0, (void *)ud_7B16E0, "080 ud_7B16E0" },
		{ 0x7B1740, (void *)ud_7B1740, "080 ud_7B1740" },
		{ 0x7B18A0, (void *)ud_7B18A0, "080 ud_7B18A0" },
		{ 0x7B1960, (void *)ud_7B1960, "080 ud_7B1960" },
		{ 0x7B19E0, (void *)ud_7B19E0, "080 ud_7B19E0" },
		{ 0x7B1A20, (void *)ud_7B1A20, "080 ud_7B1A20" },
		{ 0x7B2A50, (void *)ud_7B2A50, "080 ud_7B2A50" },
		{ 0x7B2B60, (void *)ud_7B2B60, "080 ud_7B2B60" },
		{ 0x7B2BE0, (void *)ud_7B2BE0, "080 ud_7B2BE0" },
		{ 0x7B2C10, (void *)ud_7B2C10, "080 ud_7B2C10" },
		{ 0x7B2CB0, (void *)ud_7B2CB0, "080 ud_7B2CB0" },
		{ 0x7B2D30, (void *)ud_7B2D30, "080 ud_7B2D30" },
		{ 0x7B2DB0, (void *)ud_7B2DB0, "080 ud_7B2DB0" },
		{ 0x7B37B0, (void *)ud_7B37B0, "080 ud_7B37B0" },
		{ 0x7B42A0, (void *)ud_7B42A0, "080 ud_7B42A0" },
		{ 0x7B4330, (void *)ud_7B4330, "080 ud_7B4330" },
		{ 0x7B43C0, (void *)ud_7B43C0, "080 ud_7B43C0" },
		{ 0x7B43F0, (void *)ud_7B43F0, "080 ud_7B43F0" },
		{ 0x7B4400, (void *)ud_7B4400, "080 ud_7B4400" },
		{ 0x7B4480, (void *)ud_7B4480, "080 ud_7B4480" },
		{ 0x7B44F0, (void *)ud_7B44F0, "080 ud_7B44F0" },
		{ 0x7B4540, (void *)ud_7B4540, "080 ud_7B4540" },
		{ 0x7B45B0, (void *)ud_7B45B0, "080 ud_7B45B0" },
		{ 0x7B4620, (void *)ud_7B4620, "080 ud_7B4620" },
		{ 0x7B4650, (void *)ud_7B4650, "080 ud_7B4650" },
		{ 0x7B4670, (void *)ud_7B4670, "080 ud_7B4670" },
		{ 0x7B4750, (void *)ud_7B4750, "080 ud_7B4750" },
		{ 0x7B4800, (void *)ud_7B4800, "080 ud_7B4800" },
		{ 0x7B4820, (void *)ud_7B4820, "080 ud_7B4820" },
		{ 0x7B4880, (void *)ud_7B4880, "080 ud_7B4880" },
		{ 0x7B49F0, (void *)ud_7B49F0, "080 ud_7B49F0" },
		{ 0x7B4A50, (void *)ud_7B4A50, "080 ud_7B4A50" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag080_ultimecia_junction()
	{
		act::register_module(80);
		for (const act::ujunct::ModPort *p = act::ujunct::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(80, p->addr, p->port, p->name);
		for (const act::ujunct::ModPort *p = act::ujunct::PORTS; p->addr; p++)
			act::register_module_port(80, p->addr, p->port, p->name);
		// 30 fps layer: see mag080_ultimecia_junction_held.inc
		FX_HELD(register_mag080_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag080_ultimecia_junction_held.inc"
#endif
