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

// Effect 92: Angelo Search (kernel.bin non-junctionable GF attack {Angelo} Search, section 9 entry 14, effect id +2 = 92: Rinoa's
// Angelo command; MAG_092_ANGELO_SEARCH*): a copy of the actor/heal effect library (see act_engine.h) built like Angelo Recover 93
// (mag093_angelo_recover.cpp: director, the Angelo creature, the prim-model play) with its own master and parts: Angelo walks in,
// sniffs (ring particles and dust puffs), walks to Rinoa, who plays her chain transformations 1 / 2, and leaves the found item's
// play behind. Module 0x755110..0x759350 (74 functions: 32 served by engine ports, 24 the relocated text of a ported library
// function (Angelo Rush 91 / Angelo Reverse 94, Doom 8, Siren 95, Tonberry 90), 16 hand-ported, the loader and the setup not
// ported).
//
// Setup MAG_092_ANGELO_SEARCH_Init 0x755130 (runs once, not ported; loader 0x755110) creates the root queue with the master
// and seven task pools, uploads the module textures and starts the engine camera animation.
//   Master (0x7552B0, hand port) - camera copy 0x2793E58 (pointers 0x2598EF0 / 0x2594EA0), packet arenas by tick parity
//     (module cursor 0x2594D9C / 0x2594E9C), target bone follow, states: 0x755440 waits for the children, 0x755450 starts the
//     emitter 0x755480, then the library's finish states; executes the seven module queues.
//   Emitter (0x755480) - 0x755500: the director block (0x755530: the caster's position) and the DIRECTOR 0x7555B0 (phase
//     bookkeeping 0x755620: phase 1 spawns 0x755770; states: TIM upload 0x16C12C8, cue 1, gate 1, 0x755720: from the
//     director's frame 0x9B the done flag +0x48 = 1, finished).
//   Phase 1 (0x755770) - the ANGELO creature 0x7557B0 (node 0x154, model container 0x153D59C bound with animation 3): binds
//     Rinoa's animation set (com data 0x16C96E8) and queues her chain transformation 1, walks in from behind her (+0x150 walk
//     factor), sniffs: the task 0x755C70 bursts (0x755CE0, ticks 0x25..0x32) two ring particles 0x7581D0 (prim sequence
//     0x153D1E8 drawn by 0x7582A0 through a_7435E0) and two dust puffs 0x755EE0 (model drawn by 0x755FC0) a tick at his
//     spawn points 0x1E / 0x21; walks to Rinoa (her chain transformation 2), on animation 4 frame 0xB leaves the found item's
//     play 0x758CB0 (layout 0x16C0DB8, object callback 0x758EA0) at his spawn point 0x18 on the ground, walks off and hides.
//     Drawn by the creature renderer 0x7558A0 = a_746C10 into the module packet pool, root matrix published to 0x1D99AB0.
//   Damage state 0x7591F0 (director done flag): ApplyActionResultToTarget (the found item).
// Writes outside the module: Rinoa's entity record (BS_BindEntityModelFromComData 0x5022C0 binds the animation set,
// QueueChainTransformation 0x505C00 chain transformations 1 and 2), the root-matrix slot 0x1D99AB0 (Angelo), the camera copy
// 0x2793E58, action results (0x506690); the camera itself is the engine's camera animation (started by the setup) - the module
// writes no camera word.
// Module globals: 0x2594D98..0x2598F20 (+ camera copy 0x2793E58, 0x20 bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace angelo92
{
	// ---- generated (desc92.py): the module descriptor of this copy of the library ----
	static const Mod MOD_092 = { "angelosearch", 92, 0x755110, 0x759350,
		{ 0x0, 0x0, 0x153D1D8, 0x0, 0x0, 0x2594E9C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2594D98, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25981A8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x755480, 0x755500, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7556B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x758500, 0x0, 0x7591F0, 0x759230, 0x759250, 0x759270, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x153D1D8;        // pointer to the director block (phase +0x40 / requested +0x42 / next +0x44, frame +0x46, done +0x48)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = 0x2594D9C;  // module packet cursor (tick parity: [0x259831C] / [0x2598318])
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t BdPlayStream(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x501460)(a1, a2, a3, a4); }
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
	uint32_t __cdecl as_7552B0(uint32_t a1);
	uint32_t __cdecl as_755440(uint32_t a1);
	uint32_t __cdecl as_755500(uint32_t a1);
	uint32_t __cdecl as_755530(uint32_t a1);
	uint32_t __cdecl as_7555B0(uint32_t a1);
	uint32_t __cdecl as_755620(uint32_t a1);
	uint32_t __cdecl as_755670(uint32_t a1);
	uint32_t __cdecl as_7556E0(uint32_t a1);
	uint32_t __cdecl as_755720(uint32_t a1);
	uint32_t __cdecl as_755770(uint32_t a1);
	uint32_t __cdecl as_7557B0(uint32_t a1);
	uint32_t __cdecl as_755A80(uint32_t a1);
	uint32_t __cdecl as_755B70(uint32_t a1);
	uint32_t __cdecl as_755CD0(uint32_t a1);
	uint32_t __cdecl as_755CE0(uint32_t a1);
	uint32_t __cdecl as_755EE0(uint32_t a1);
	uint32_t __cdecl as_755FC0(uint32_t a1);
	uint32_t __cdecl as_7560D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_756200(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_756420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_7566B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_756B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_7575E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_757AE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl as_758130(uint32_t a1);
	uint32_t __cdecl as_758170(uint32_t a1);
	uint32_t __cdecl as_7581D0(uint32_t a1);
	uint32_t __cdecl as_7582A0(uint32_t a1);
	uint32_t __cdecl as_758970(uint32_t a1);
	uint32_t __cdecl as_758990(uint32_t a1);
	uint32_t __cdecl as_758A00(uint32_t a1);
	uint32_t __cdecl as_758AA0(uint32_t a1);
	uint32_t __cdecl as_758BB0(uint32_t a1);
	uint32_t __cdecl as_758C60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl as_758D10(uint32_t a1);
	uint32_t __cdecl as_758D70(uint32_t a1);
	uint32_t __cdecl as_758EA0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl as_7590E0(uint32_t a1);
	uint32_t __cdecl as_759110(uint32_t a1);
	uint32_t __cdecl as_7591F0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag092_angelo_search_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace angelo92
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Angelo Rush / Angelo Reverse / Doom / Siren / Tonberry port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x7552B0 (MAG_092_sub_7552B0): MASTER task - snapshots the camera matrix into 0x2793E58 (pointers 0x2598EF0 / 0x2594EA0),
	// picks the double-buffered packet arenas by tick parity (module cursor 0x2594D9C = [0x259831C] / [0x2598318], second cursor
	// 0x2594E9C = [0x2597F14] / [0x2597F10]), follows the target's bones, runs the 11-state master table {0x755420, 0x755430
	// (a_73A0D0), 0x755440 (wait for the children), 0x755450 (a_73A170: starts the emitter), the library's finish states
	// 0x759280.., ret}, then executes the seven module queues (live task count -> node +0x5E)
	uint32_t __cdecl as_7552B0(uint32_t a1)
	{
		g_mod = &MOD_092;
		// 30 fps layer: see mag092_angelo_search_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x2598EF0) = 0x2793E58;
		MEM<uint32_t>(0x2594EA0) = 0x2793E58;
		states[0] = 0x755420;
		states[1] = 0x755430;
		states[2] = 0x755440;
		states[3] = 0x755450;
		states[4] = 0x759280;
		states[5] = 0x7592C0;
		states[6] = 0x7592D0;
		states[7] = 0x7592E0;
		states[8] = 0x759310;
		states[9] = 0x759320;
		states[10] = 0x759340; // nullsub (ret)
		if ((U8(node, 0x5C) & 1) != 0)
		{
			MEM<uint32_t>(0x2594D9C) = MEM<uint32_t>(0x259831C);
			MEM<uint32_t>(0x2594E9C) = MEM<uint32_t>(0x2597F14);
		}
		else
		{
			MEM<uint32_t>(0x2594D9C) = MEM<uint32_t>(0x2598318);
			MEM<uint32_t>(0x2594E9C) = MEM<uint32_t>(0x2597F10);
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		static const uint32_t queues[7] = { 0x25981A8, 0x2594E78, 0x2598ED0, 0x2594F80, 0x2596618, 0x2597F00, 0x2598198 };
		for (int i = 0; i < 7; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag092_angelo_search_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl as_755440(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8218C0 (MAG_049_sub_8218C0; Psycho Blast 0x893450 = Granaldo 0x8A36A0): emitter state 0 - sets up the
	// director block (0x8218F0), spawns the director 0x821970 (0x48 bytes, director queue), next state
	uint32_t __cdecl as_755500(uint32_t a1)
	{
		as_755530(a1);
		x::Effect_AddTaskAndInitFromCtx(0x2594E78, 0x7555B0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8218F0 (MAG_049_sub_8218F0; Psycho Blast 0x893480): emitter a1 - clears the director block [0x15ADAF0] (0x54 bytes),
	// +8 / +0xC = the target's (+0x2D) position words, origin +0..+4 = 0, facing +0x4A = the angle of
	// the target seen from the origin - 0x800 (12 bits)
	uint32_t __cdecl as_755530(uint32_t a1)
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
	uint32_t __cdecl as_7555B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x755670;
		states[1] = 0x755690;
		states[2] = 0x7556E0;
		states[3] = 0x755720;
		states[4] = 0x755750; // nullsub (ret)
		as_755620(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl as_755620(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			as_755770(a1);
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
	uint32_t __cdecl as_755670(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x16C12C8);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x763B90 (module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl as_7556E0(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x755720 (sub_755720): director state 3 - from the director's frame +0x46 = 0x9B: done flag +0x48 = 1, finished
	// (+0x26 |= 1, a 16-bit or), next state
	uint32_t __cdecl as_755720(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) >= 0x9B)
		{
			U16(d, 0x48) = 1;
			U16(a1, 0x26) = (uint16_t)(U16(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x755770 (MAG_092_SpawnAngeloCreature): the tasks of a new director phase ([DIR_PTR] +0x40): phase 1 = the ANGELO actor
	// 0x7557B0 (queue 0x2598ED0, node 0x154) bound to the model container 0x153D59C animation 3
	uint32_t __cdecl as_755770(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x40) == 1)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2598ED0, 0x7557B0, 0x154, a1);
			x::Effect_BindModelContainerSetAnim(t, 0x153D59C, 3);
		}
		return 0; // void
	}

	// 0x7557B0 (sub_7557B0): ANGELO actor task (node 0x154) - states {0x755A80 place (binds Rinoa's animation set), 0x755B70
	// walk in (spawns the sniff task), 0x758A00 (to Rinoa), 0x758AA0 (walk to Rinoa: her chain transformation 2), 0x758BB0 (the
	// found item's play), 0x759110 (walk off, hidden), ret}; then position += velocity (+0x124 / +0x126 / +0x128); unless hidden
	// (+0x26 bit 2) the model is drawn by the creature renderer 0x7558A0 (a_746C10, state block 0x2598320) into the module packet
	// pool; frame +0x24; root matrix (+0x70) published to 0x1D99AB0, position to +0x84/+0x88/+0x8C (s32); released once finished
	// with no child alive
	uint32_t __cdecl as_7557B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x755A80;
		states[1] = 0x755B70;
		states[2] = 0x758A00;
		states[3] = 0x758AA0;
		states[4] = 0x758BB0;
		states[5] = 0x759110;
		states[6] = 0x7591E0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + U16(node, 0x128));
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + U16(node, 0x124));
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + U16(node, 0x126));
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag092_angelo_search_held.inc
			FX_HELD(held_note_draw(HK_ANGELO, node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, 0x2598320, MEM<uint32_t>(PACKET_CURSOR));
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

	// 0x755A80 (sub_755A80): Angelo state 0 - binds Rinoa's animation set (com data 0x16C96E8, 0x20-byte block 0x2598EF8) to the
	// caster (+0x2D, BS_BindEntityModelFromComData 0x5022C0) and queues her chain transformation 1 (QueueChainTransformation
	// 0x505C00); walk: from her position + (-0x180, 0, -0x600) + z 0x1000 (+0x140..) to her position + (-0x180, 0, -0x600)
	// (+0x148..), placed at the start; scale 0x1000^3, animation 0, sound 0x153D1DC, stream 0x16CB3F4, next state
	uint32_t __cdecl as_755A80(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2D));
		gx::BindEntityModelFromComData(e, 0x2598EF8, 0x16C96E8);
		gx::QueueChainTransformation(e, 1);
		U32(node, 0x148) = U32(e, 0x1C);
		U32(node, 0x14C) = U32(e, 0x20);
		U16(node, 0x148) = (uint16_t)(U16(node, 0x148) + 0xFE80);
		U16(node, 0x14C) = (uint16_t)(U16(node, 0x14C) + 0xFA00);
		U16(node, 0x14A) = 0;
		U32(node, 0x140) = U32(node, 0x148);
		U32(node, 0x144) = U32(node, 0x14C);
		U16(node, 0x144) = (uint16_t)(U16(node, 0x144) + 0x1000);
		U32(node, 0x4C) = U32(node, 0x140);
		U32(node, 0x50) = U32(node, 0x144);
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		x::BdPlaySE(0x153D1DC, 0, 0x80);
		gx::BdPlayStream(0x16CB3F4, 0x80, 1, 0x7F);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// Angelo's walk: position +0x4C.. = from (+0x140..) + (to (+0x148..) - from) * t / 4096 (each axis, s16, t = +0x150)
	static inline void as_walk(uint32_t node, int32_t t)
	{
		for (uint32_t k = 0; k < 6; k += 2)
		{
			const int16_t a = S16(node, 0x140 + k);
			U16(node, 0x4C + k) = (uint16_t)((int32_t)(S16(node, 0x148 + k) - a) * t / 4096 + a);
		}
	}

	// 0x755B70 (sub_755B70): Angelo state 1 - animation step (0x8DD1C0); walk +0x150 += 0x100; at 0x1000: at the end point,
	// animation 1, the sniff task 0x755C70 (queue 0x2594F80, node 0x90: at his position, +0x88 = his model node + 0x30), next
	// state
	uint32_t __cdecl as_755B70(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = node + 0x30;
		x::sub_8DD1C0(node);
		U16(node, 0x150) = (uint16_t)(U16(node, 0x150) + 0x100);
		const int16_t t = S16(node, 0x150);
		if (t >= 0x1000)
		{
			U32(mdl, 0x1C) = U32(node, 0x148);
			U16(node, 0x150) = 0x1000;
			U32(mdl, 0x20) = U32(node, 0x14C);
			x::au_re_Battle_ReadAnimation_7(node, 1);
			const uint32_t s = x::Effect_AddTaskAndInitFromCtx(0x2594F80, 0x755C70, 0x90, node);
			U32(s, 0x1C) = U32(mdl, 0x1C);
			U32(s, 0x20) = U32(mdl, 0x20);
			U32(s, 0x88) = mdl;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			return 0; // void
		}
		as_walk(node, t);
		return 0; // void
	}

	// 0x755CD0 (sub_755CD0): sniff task state 0 - next state from frame 0x25
	uint32_t __cdecl as_755CD0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x25)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x755CE0 (sub_755CE0): sniff task state 1 - every tick two ring particles 0x7581D0 (queue 0x2594F80, node 0x90) and two
	// dust puffs 0x755EE0 (queue 0x2596618, node 0x8C) at Angelo's spawn points 0x1E / 0x21 (model +0x88) +-0x40 in x / z:
	// rings velocity +0x78.. = (0, 0, 0x200..0xBFF) turned by a random yaw +-0x100, rise -(0..0x5F) per tick +0x82; puffs
	// velocity +0x74.. = (0, 0, 0x200..0x9FF) turned by a random yaw +-0x100 and pitch 0x100..0x27F; from frame 0x32 finished,
	// next state
	uint32_t __cdecl as_755CE0(uint32_t a1)
	{
		const uint32_t mdl = U32(a1, 0x88);
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		for (int i = 0; i < 2; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2594F80, 0x7581D0, 0x90, a1);
			x::GetEffectSpawnPosition(mdl, i == 0 ? 0x1E : 0x21, 0, t + 0x1C);
			const int32_t r1 = (int32_t)x::CrtRand();
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0x7F) - 0x40));
			const int32_t r2 = (int32_t)x::CrtRand();
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0x7F) - 0x40));
			x::MAG_022_sub_8DD770(M);
			const int32_t r3 = (int32_t)x::CrtRand();
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r3 & 0x1FF) - 0x100) & 0xFFF));
			const int32_t r4 = (int32_t)x::CrtRand();
			U16(t, 0x78) = 0;
			U16(t, 0x7A) = 0;
			U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
			const int32_t r5 = (int32_t)x::CrtRand();
			U16(t, 0x7C) = (uint16_t)(r5 % 0xA00 + 0x200);
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		}
		for (int i = 0; i < 2; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2596618, 0x755EE0, 0x8C, a1);
			x::GetEffectSpawnPosition(mdl, i == 0 ? 0x1E : 0x21, 0, t + 0x1C);
			const int32_t r1 = (int32_t)x::CrtRand();
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0x7F) - 0x40));
			const int32_t r2 = (int32_t)x::CrtRand();
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0x7F) - 0x40));
			x::MAG_022_sub_8DD770(M);
			const int32_t r3 = (int32_t)x::CrtRand();
			x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r3 & 0x1FF) - 0x100) & 0xFFF));
			const int32_t r4 = (int32_t)x::CrtRand();
			x::sub_8DD7E0(M, (uint32_t)(int32_t)(int16_t)(((r4 % 0x180) + 0x100) & 0xFFF));
			U16(t, 0x76) = 0;
			U16(t, 0x74) = 0;
			const int32_t r5 = (int32_t)x::CrtRand();
			U16(t, 0x78) = (uint16_t)(r5 % 0x800 + 0x200);
			x::matrixMultiplyVector(M, t + 0x74, t + 0x74);
		}
		if (S16(a1, 0x24) >= 0x32)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x755EE0 (sub_755EE0): dust puff task (node 0x8C) - states {0x758130 set-up, 0x758170 roll / shrink, ret}; velocity
	// +0x74.. damped by 1/16 (y pulled up by 0x80), position += velocity / 16; drawn by 0x755FC0; frame +0x24; released once
	// finished with no child alive
	uint32_t __cdecl as_755EE0(uint32_t a1)
	{
		const uint32_t c = a1;
		uint32_t states[3];
		states[0] = 0x758130;
		states[1] = 0x758170;
		states[2] = 0x7581C0; // nullsub (ret)
		callp(states[S8(c, 0x29)], c);
		int16_t vx = S16(c, 0x74);
		int16_t vy = (int16_t)(S16(c, 0x76) + 0x80);
		int16_t vz = S16(c, 0x78);
		vx = (int16_t)(vx - vx / 16);
		U16(c, 0x74) = (uint16_t)vx;
		vy = (int16_t)(vy - vy / 16);
		U16(c, 0x76) = (uint16_t)vy;
		vz = (int16_t)(vz - vz / 16);
		U16(c, 0x78) = (uint16_t)vz;
		U16(c, 0x1C) = (uint16_t)(U16(c, 0x1C) + (uint16_t)(vx / 16));
		U16(c, 0x1E) = (uint16_t)(U16(c, 0x1E) + (uint16_t)(vy / 16));
		U16(c, 0x20) = (uint16_t)(U16(c, 0x20) + (uint16_t)(vz / 16));
		as_755FC0(c);
		U16(c, 0x24) = (uint16_t)(U16(c, 0x24) + 1);
		if ((U8(c, 0x26) & 1) != 0 && U8(c, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(c);
			return 2;
		}
		return 0;
	}

	// 0x788700 (sub_788700): a model of the module (definition +0x4C) drawn unless hidden (+0x26 bit 2): the camera rotation
	// (UnpackRotationMatrix 0x1D97778) turned by +0x48 (z), at +0x1C.., scaled by +0x30.., through the camera; header 0x68
	// (+8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0, both windows 256 x 256) through the module renderer
	// 0x785A90 into the frame arena 0x1D8E054
	uint32_t __cdecl as_755FC0(uint32_t a1)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		// 30 fps layer: see mag092_angelo_search_held.inc
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
		MEM<uint32_t>(0x1D8E054) = as_7560D0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 as_7566B0, FT4 as_756B30,
	// a_73D770, a_73D9C0, 0x73DCA0, as_757AE0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl as_7560D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = as_756200(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = as_756420(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = as_7566B0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = as_756B30(ctx, a2, a3, cur);
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
			cur = as_7575E0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return as_757AE0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl as_756200(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_756420(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_7566B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_756B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_7575E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_757AE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl as_758130(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x153D2C4;
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U16(a1, 0x48) = (uint16_t)(x::CrtRand() & 0xFFF);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x758170 (sub_758170): dust puff state 1 - roll +0x40, y kept on / above the ground; from frame 0xA the scale shrinks by
	// 0x400 a tick (at 0: finished, scale 0x10, next state)
	uint32_t __cdecl as_758170(uint32_t a1)
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
		return 0; // void
	}

	// 0x7581D0 (sub_7581D0): ring particle task (node 0x90) - states {0x758970 set-up, 0x758990 sequence step, ret}; y velocity
	// +0x7A += +0x82, x / z velocity damped by 1/8, position += velocity / 16; drawn by 0x7582A0; frame +0x24; released once
	// finished with no child alive
	uint32_t __cdecl as_7581D0(uint32_t a1)
	{
		const uint32_t n = a1;
		uint32_t states[3];
		states[0] = 0x758970;
		states[1] = 0x758990;
		states[2] = 0x7589E0; // nullsub (ret)
		callp(states[S8(n, 0x29)], n);
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
		as_7582A0(n);
		U16(n, 0x24) = (uint16_t)(U16(n, 0x24) + 1);
		if ((U8(n, 0x26) & 1) != 0 && U8(n, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(n);
			return 2;
		}
		return 0;
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl as_7582A0(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag092_angelo_search_held.inc
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
		const uint32_t cursor = MEM<uint32_t>(0x2594D9C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x2594D9C) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x7618E0 (sub_7618E0): ring particle state 0 - prim sequence 0x15430E8, colour +0x56 = 0xFFC0, +0x52 = 7, next state
	uint32_t __cdecl as_758970(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x153D1E8;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl as_758990(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x758A00 (sub_758A00): Angelo state 2 - sound 0x153D1E0 on animation frame +0x136 = 6; when the animation ends (0x8DD220):
	// animation 2, walk from his position to Rinoa's (+0x1C.. of the caster's entity record) - 0x280 in z, +0x3E = 0xA0, next
	// state
	uint32_t __cdecl as_758A00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2D));
		if (S16(node, 0x136) == 6)
			x::BdPlaySE(0x153D1E0, 1, 0x80);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 2);
			U32(node, 0x140) = U32(node, 0x4C);
			U32(node, 0x144) = U32(node, 0x50);
			U32(node, 0x148) = U32(e, 0x1C);
			U32(node, 0x14C) = U32(e, 0x20);
			U16(node, 0x14C) = (uint16_t)(U16(node, 0x14C) + 0xFD80);
			U16(node, 0x3E) = 0xA0;
			U16(node, 0x150) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x758AA0 (sub_758AA0): Angelo state 3 - animation step; walk +0x150 += 0x2AB; at 0x1000: at Rinoa's position, her chain
	// transformation 2 (QueueChainTransformation 0x505C00), animation 3, +0x3E = 0, next state
	uint32_t __cdecl as_758AA0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t e = ENT(U8(node, 0x2D));
		x::sub_8DD1C0(node);
		U16(node, 0x150) = (uint16_t)(U16(node, 0x150) + 0x2AB);
		const int16_t t = S16(node, 0x150);
		if (t >= 0x1000)
		{
			U16(node, 0x150) = 0x1000;
			U32(node, 0x4C) = U32(e, 0x1C);
			U32(node, 0x50) = U32(e, 0x20);
			gx::QueueChainTransformation(e, 2);
			x::au_re_Battle_ReadAnimation_7(node, 3);
			U16(node, 0x3E) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			return 0; // void
		}
		as_walk(node, t);
		return 0; // void
	}

	// 0x758BB0 (sub_758BB0): Angelo state 4 - when the animation ends: animation 4, stream 0x16CB3F8, walk off from his position
	// to + z 0x1000, next state; on animation frame +0x136 = 0xB the found item's play 0x758CB0 (0x758C60: layout 0x16C0DB8 /
	// 0xE0) at his spawn point 0x18 on the ground
	uint32_t __cdecl as_758BB0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 4);
			gx::BdPlayStream(0x16CB3F8, 0x80, 1, 0x7F);
			const uint32_t p = U32(node, 0x4C), q = U32(node, 0x50);
			U32(node, 0x140) = p;
			U32(node, 0x148) = p;
			U32(node, 0x14C) = q;
			U16(node, 0x14C) = (uint16_t)(U16(node, 0x14C) + 0x1000);
			U32(node, 0x144) = q;
			U16(node, 0x150) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		if (S16(node, 0x136) == 0xB)
		{
			const uint32_t t = as_758C60(node, 0x758CB0, 0x16C0DB8, 0xE0, 0, 0);
			x::GetEffectSpawnPosition(node + 0x30, 0x18, 0, t + 0x1C);
			U16(t, 0x1E) = 0;
		}
		return 0; // void
	}

	// 0x754900 (sub_754900): spawns task a2 (queue 0x2593AD8, node 0x17C) from context a1: +0x74 = a3 (prim layout),
	// +0x78 = (s16) a4, +0x80 = a5, +0x82 = a6; returns the node
	uint32_t __cdecl as_758C60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2597F00, a2, 0x17C, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x758D10 (sub_758D10): item play state 0 (task 0x758CB0) - from frame 8: decodes the prim layout (+0x74, count +0x78) into
	// +0x94, scale 0x1000^3, colour +0x86 = 0xFF00, plays the first frame (0x758D70), sound 0x153D1E4, next state
	uint32_t __cdecl as_758D10(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 8)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		S32(node, 0x58) = 0x1000;
		S32(node, 0x54) = 0x1000;
		S32(node, 0x50) = 0x1000;
		U16(node, 0x86) = 0xFF00;
		as_758D70(node);
		x::BdPlaySE(0x153D1E4, 1, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x754A60 (sub_754A60): heal particle play step (MiniMog 0x734450 shape) - builds the effect matrix (rotations
	// +0x62 Y / +0x60 X / +0x64 Z, scale +0x50, position +0x1C/+0x1E/+0x20, camera 0x1D97778) and plays the node's
	// prim layout (+0x94) through the callback 0x754B90. Stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3,
	// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = [0x2594D74] (vertex lerp scratch), +0x4C..+0x58 words = node
	// +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88; +0x20..+0x37 and +0x3C..+0x43 are never written (the callback does not
	// read them; zeroed here). Not paused (0)
	uint32_t __cdecl as_758D70(uint32_t a1)
	{
		// 30 fps layer: see mag092_angelo_search_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
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
		U32(L, 0x48) = MEM<uint32_t>(0x2598EE8);
		U32(L, 0x44) = U32(node, 0x7C);
		U16(L, 0x4C) = U16(node, 0x8C);
		U32(L, 0x38) = U32(node, 0x70);
		U16(L, 0x52) = U16(node, 0x92);
		U16(L, 0x4E) = U16(node, 0x8E);
		U16(L, 0x50) = U16(node, 0x90);
		U16(L, 0x56) = U16(node, 0x80);
		U16(L, 0x58) = U16(node, 0x88);
		return prim_play(node + 0x94, 0x758EA0, L, 0);
	}

	// 0x7EBBB0 (sub_7EBBB0; Curse's cu_82B870 drawing at the module packet cursor 0x260407C instead of
	// the frame arena cursor 0x1D8E054): prim-model player draw callback (layout a1, record a2, block
	// a3) - as Doom's 0x8C5B10 without its record flags 0x400 / 0x100 / 0x4000: picks the object's vertex
	// frame (lerped by MAG_017_sub_701390), builds its matrix (rotation 0x701310; parent-rotated
	// position unless record flag 0x200; scale3DMatrix), fills a 0x68-byte Field_Alloc render
	// header (flags 0x2030, 0x20F0 with alpha + colour, depth offset block +0x58, scale words 0x100)
	// and draws it with 0x7EBDF0 into OT base+0x44 (shift 2) at the module packet cursor 0x260407C
	uint32_t __cdecl as_758EA0(uint32_t a1, uint32_t a2, uint32_t a3)
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
		MEM<uint32_t>(PACKET_CURSOR) = as_7560D0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl as_7590E0(uint32_t a1)
	{
		const uint32_t r = as_758D70(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x759110 (sub_759110): Angelo state 5 - animation step; walk +0x150 += 0x100; at 0x1000: at the end point, hidden +
	// finished, next state
	uint32_t __cdecl as_759110(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		U16(node, 0x150) = (uint16_t)(U16(node, 0x150) + 0x100);
		const int16_t t = S16(node, 0x150);
		if (t >= 0x1000)
		{
			U32(node, 0x4C) = U32(node, 0x148);
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 5);
			U32(node, 0x50) = U32(node, 0x14C);
			U16(node, 0x150) = 0x1000;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			return 0; // void
		}
		as_walk(node, t);
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl as_7591F0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x153D1D8);
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

	// ---- generated (desc92.py / cls92.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x755420, (void *)a_73A0D0, "092 MAG_092_sub_755420" },
		{ 0x755430, (void *)a_73A0D0, "092 MAG_092_sub_755430" },
		{ 0x755450, (void *)a_73A170, "092 MAG_092_sub_755450" },
		{ 0x755480, (void *)a_73A1A0, "092 MAG_092_sub_755480" },
		{ 0x755690, (void *)a_73B790, "092 sub_755690" },
		{ 0x7556B0, (void *)a_73B640, "092 sub_7556B0" },
		{ 0x755700, (void *)a_73B7E0, "092 sub_755700" },
		{ 0x755750, (void *)a_73A6D0, "092 nullsub_1228" },
		{ 0x755760, (void *)a_73A6D0, "092 nullsub_1229" },
		{ 0x7558A0, (void *)a_746C10, "092 sub_7558A0" },
		{ 0x755C70, (void *)a_73A380, "092 sub_755C70" },
		{ 0x7570B0, (void *)a_73D770, "092 sub_7570B0" },
		{ 0x757300, (void *)a_73D9C0, "092 sub_757300" },
		{ 0x7581C0, (void *)a_73A6D0, "092 nullsub_1230" },
		{ 0x7583C0, (void *)a_7435E0, "092 InitEffectSequenceFromData_c3" },
		{ 0x758500, (void *)a_743720, "092 sub_758500" },
		{ 0x7589B0, (void *)a_743C20, "092 sub_7589B0" },
		{ 0x7589E0, (void *)a_73A6D0, "092 nullsub_1231" },
		{ 0x7589F0, (void *)a_73A6D0, "092 nullsub_1232" },
		{ 0x758CB0, (void *)a_73A380, "092 sub_758CB0" },
		{ 0x759100, (void *)a_73A6D0, "092 nullsub_1233" },
		{ 0x7591E0, (void *)a_73A6D0, "092 nullsub_1234" },
		{ 0x759230, (void *)a_7474B0, "092 MAG_092_sub_759230" },
		{ 0x759250, (void *)a_7474D0, "092 MAG_092_sub_759250" },
		{ 0x759270, (void *)a_73A6D0, "092 nullsub_1235" },
		{ 0x759280, (void *)a_747500, "092 MAG_092_sub_759280" },
		{ 0x7592C0, (void *)a_73A0D0, "092 MAG_092_sub_7592C0" },
		{ 0x7592D0, (void *)a_747550, "092 MAG_092_sub_7592D0" },
		{ 0x7592E0, (void *)a_747560, "092 MAG_092_sub_7592E0" },
		{ 0x759310, (void *)a_747590, "092 MAG_092_sub_759310" },
		{ 0x759320, (void *)a_7475A0, "092 MAG_092_sub_759320" },
		{ 0x759340, (void *)a_73A6D0, "092 nullsub_1227" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x7552B0, (void *)as_7552B0, "092 as_7552B0" },
		{ 0x755440, (void *)as_755440, "092 as_755440" },
		{ 0x755500, (void *)as_755500, "092 as_755500" },
		{ 0x755530, (void *)as_755530, "092 as_755530" },
		{ 0x7555B0, (void *)as_7555B0, "092 as_7555B0" },
		{ 0x755620, (void *)as_755620, "092 as_755620" },
		{ 0x755670, (void *)as_755670, "092 as_755670" },
		{ 0x7556E0, (void *)as_7556E0, "092 as_7556E0" },
		{ 0x755720, (void *)as_755720, "092 as_755720" },
		{ 0x755770, (void *)as_755770, "092 as_755770" },
		{ 0x7557B0, (void *)as_7557B0, "092 as_7557B0" },
		{ 0x755A80, (void *)as_755A80, "092 as_755A80" },
		{ 0x755B70, (void *)as_755B70, "092 as_755B70" },
		{ 0x755CD0, (void *)as_755CD0, "092 as_755CD0" },
		{ 0x755CE0, (void *)as_755CE0, "092 as_755CE0" },
		{ 0x755EE0, (void *)as_755EE0, "092 as_755EE0" },
		{ 0x755FC0, (void *)as_755FC0, "092 as_755FC0" },
		{ 0x7560D0, (void *)as_7560D0, "092 as_7560D0" },
		{ 0x756200, (void *)as_756200, "092 as_756200" },
		{ 0x756420, (void *)as_756420, "092 as_756420" },
		{ 0x7566B0, (void *)as_7566B0, "092 as_7566B0" },
		{ 0x756B30, (void *)as_756B30, "092 as_756B30" },
		{ 0x7575E0, (void *)as_7575E0, "092 as_7575E0" },
		{ 0x757AE0, (void *)as_757AE0, "092 as_757AE0" },
		{ 0x758130, (void *)as_758130, "092 as_758130" },
		{ 0x758170, (void *)as_758170, "092 as_758170" },
		{ 0x7581D0, (void *)as_7581D0, "092 as_7581D0" },
		{ 0x7582A0, (void *)as_7582A0, "092 as_7582A0" },
		{ 0x758970, (void *)as_758970, "092 as_758970" },
		{ 0x758990, (void *)as_758990, "092 as_758990" },
		{ 0x758A00, (void *)as_758A00, "092 as_758A00" },
		{ 0x758AA0, (void *)as_758AA0, "092 as_758AA0" },
		{ 0x758BB0, (void *)as_758BB0, "092 as_758BB0" },
		{ 0x758C60, (void *)as_758C60, "092 as_758C60" },
		{ 0x758D10, (void *)as_758D10, "092 as_758D10" },
		{ 0x758D70, (void *)as_758D70, "092 as_758D70" },
		{ 0x758EA0, (void *)as_758EA0, "092 as_758EA0" },
		{ 0x7590E0, (void *)as_7590E0, "092 as_7590E0" },
		{ 0x759110, (void *)as_759110, "092 as_759110" },
		{ 0x7591F0, (void *)as_7591F0, "092 as_7591F0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag092_angelo_search()
	{
		act::register_module(92);
		for (const act::angelo92::ModPort *p = act::angelo92::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(92, p->addr, p->port, p->name);
		for (const act::angelo92::ModPort *p = act::angelo92::PORTS; p->addr; p++)
			act::register_module_port(92, p->addr, p->port, p->name);
		// 30 fps layer: see mag092_angelo_search_held.inc
		FX_HELD(register_mag092_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag092_angelo_search_held.inc"
#endif
