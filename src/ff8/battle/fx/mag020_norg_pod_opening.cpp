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

// Effect 20: NORG Pod opening (enemy attack 287 of kernel.bin, no name; used by the NORG Pod c0m068:
// its shell opens - MAG_020_NORG_POD_OPENING_*): a Drain-style master / emitter with the module's
// own pod lid, debris and chunks; the module's prim-model renderer is the actor library's (the
// Raldo Throw / Death copy), sprites and small state helpers are served by the actor-engine ports
// (see act_engine.h).
//
// Setup MAG_020_NORG_POD_OPENING 0x89FBC0 (runs once, not ported; file loader
// MAG_020_NORG_POD_OPENING_FL 0x89FBA0 = the effect's data file 0x1629F9C (mag019.tim, shared with
// Raldo Throw) -> 0x2721590, TIM uploaded and camera animation 0x1629E60 played unless the cast
// flags +1 bit 0; two packet arenas pairs at file + 0 / + 0xA000 and file + 0xA000 / + 0x14000 by
// tick parity) creates the root queue 0x2721680 with the master and four task pools: 0x2726770
// (4 x 0x58: emitters), 0x2721690 (0x28 x 0x78: lids and debris), 0x2722968 (0x64 x 0x98: chunks)
// and 0x2726760 (10 x 0x40: the stage light ramps and the stage wobble task 0x8DDC30).
//   Master (0x89FD00) - camera copy 0x2793E58, packet arena by tick parity (cursor 0x2721594),
//     bone follow, 11-state table: once no child is alive the stage light ramp up (0x89FE90: the
//     four stage light words 0x1D98992 + k * 0x2C from 0 to 0x400 by 0x100 per tick), one emitter
//     per action (engine 0x89FF80; loop 0x8A30E0 once the emitter releases the master's +0x63
//     hold), 15 ticks after the last one the light ramp down (0x8A3150), then waits for the queues.
//   Emitter (0x89FFB0 = Drink Magic 0x856D70), one per action: bone follow + model bounds, sound
//     0x1629C50 at its first tick; state 0 the pod lid (0x8A0070), state 1 releases the master at
//     tick 30, state 2 applies the damage of the action's current target (0x506690) from tick 30.
//   Lid (0x8A0070): the caster's model bone 1 drawn apart (0x8A2D90: RenderGeometry with that bone
//     raised by 0x1140 and moved by the lid's offset / angles; the caster's own draw is switched
//     off, entity +0x7C bit 1). Voice slot 0x17F36A0 at its start; from tick 34 the stage wobble
//     (0x8DDC30, script 0x1629F14), the chain transformation 0xC of entity 0x1D97668 and 24 ticks of
//     flight (velocity (0x120, -0x140, 0x20), gravity 0x28, spin x -0x40 / z +0x30); at the end
//     setMonsterPresence(6) and the voice slot released. Every tick its debris spawner (0x8A00E0):
//     at the ticks of the table 0x1629F88, 4 debris at random offsets of the table 0x1629F28.
//   Debris (0x8A01B0): a sprite sequence (flipbook 0x1629C54 of 15 frames, 0x8A0230) that throws
//     one chunk at its ticks 0 and 1 (0x8A0900).
//   Chunk (0x8A0990): two pieces of the prim model 0x1629E00 (random velocities, gravity, drag 1/8,
//     spin +0x20 a tick, shrinking from tick 8), drawn by 0x8A0AA0 through the module renderer.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2721590..0x2726900 (file pointer 0x2721590, packet cursors 0x2721594 /
// 0x27215AC, arenas 0x27268E0 / 0x27268E4 / 0x27264D8 / 0x27264DC, camera copy pointers 0x27268F4 /
// 0x27215B0, pools and queues).
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the renderer is the text of the Raldo Throw port with this
// module's addresses; the others are ported below (np_XXXXXX).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace norgpod
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_020 = { "norgpod", 20, 0x89FBA0, 0x8A3280,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2726770, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x89FFB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A0490, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2721594;   // module packet cursor (the draws' arena)
	static const uint32_t Q_EMITTER = 0x2726770, Q_POD = 0x2721690, Q_CHUNK = 0x2722968, Q_STAGE = 0x2726760;
	static const uint32_t ORIG_Master = 0x89FD00;
	static const uint32_t ORIG_Lid = 0x8A0070;
	static const uint32_t ORIG_Debris = 0x8A01B0;
	static const uint32_t ORIG_Chunk = 0x8A0990;
	static const uint32_t SOUND_Open = 0x1629C50;
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); }
		static inline uint32_t setMonsterPresence(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x487670)(a1); }
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

	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	// chunk piece k (b = node + 8 * k): velocity +0x74..+0x78, position +0x54..+0x58 (0x8A09C6..0x8A0A43)
	static inline void chunk_move(uint32_t b)
	{
		U16(b, 0x76) = (uint16_t)(U16(b, 0x76) + 0x100);
		for (int32_t o = 0x74; o <= 0x78; o += 2)
		{
			const int16_t v = S16(b, o);
			S16(b, o) = (int16_t)(v - (int32_t)v / 8);
		}
		for (int32_t o = 0; o <= 4; o += 2)
			S16(b, 0x54 + o) = (int16_t)(S16(b, 0x54 + o) + (int32_t)S16(b, 0x74 + o) / 16);
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl np_89FD00(uint32_t a1);
	uint32_t __cdecl np_89FE60(uint32_t a1);
	uint32_t __cdecl np_89FF30(uint32_t a1);
	uint32_t __cdecl np_89FFB0(uint32_t a1);
	uint32_t __cdecl np_8A0040(uint32_t a1);
	uint32_t __cdecl np_8A0070(uint32_t a1);
	uint32_t __cdecl np_8A00E0(uint32_t a1);
	uint32_t __cdecl np_8A01B0(uint32_t a1);
	uint32_t __cdecl np_8A0230(uint32_t a1);
	uint32_t __cdecl np_8A0900(uint32_t a1, uint32_t a2);
	uint32_t __cdecl np_8A0990(uint32_t a1);
	uint32_t __cdecl np_8A0AA0(uint32_t a1);
	uint32_t __cdecl np_8A0BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A0CF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A0F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A11A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A1620(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A20D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A25D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl np_8A2C20(uint32_t a1);
	uint32_t __cdecl np_8A2C60(uint32_t a1);
	uint32_t __cdecl np_8A2CB0(uint32_t a1);
	uint32_t __cdecl np_8A2CD0(uint32_t a1);
	uint32_t __cdecl np_8A2D30(uint32_t a1);
	uint32_t __cdecl np_8A2D90(uint32_t a1);
	uint32_t __cdecl np_8A2F70(uint32_t a1);
	uint32_t __cdecl np_8A2FE0(uint32_t a1);
	uint32_t __cdecl np_8A3070(uint32_t a1);
	uint32_t __cdecl np_8A3090(uint32_t a1);
	uint32_t __cdecl np_8A30E0(uint32_t a1);
	uint32_t __cdecl np_8A3120(uint32_t a1);
	uint32_t __cdecl np_8A31B0(uint32_t a1);
	uint32_t __cdecl np_8A31D0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag020_norg_pod_opening_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace norgpod
{
	// ====================================================================================
	// master, stage light ramps, emitter (module code; the Drain / Death copies with this module's
	// addresses)
	// ====================================================================================

	// 0x89FD00 (MAG_020_sub_89FD00): MASTER task - camera copy 0x2793E58 (also stored in 0x27268F4 /
	// 0x27215B0), the two packet arenas by tick parity (+0x5C bit 0: cursors 0x2721594 / 0x27215AC),
	// bone follow, 11-state table, the four queues (live task count -> node +0x5E)
	uint32_t __cdecl np_89FD00(uint32_t a1)
	{
		g_mod = &MOD_020;

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x27268F4) = 0x2793E58;
		MEM<uint32_t>(0x27215B0) = 0x2793E58;
		states[0] = 0x89FE40;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x89FE50;
		states[2] = 0x89FE60;
		states[3] = 0x89FF80;
		states[4] = 0x8A30E0;
		states[5] = 0x8A3120;
		states[6] = 0x8A3220;
		states[7] = 0x8A3230;
		states[8] = 0x8A3240;
		states[9] = 0x8A3250;
		states[10] = 0x8A3270; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x27268E4);
			const uint32_t v2 = MEM<uint32_t>(0x27264DC);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x27215AC) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x27268E0);
			const uint32_t v2 = MEM<uint32_t>(0x27264D8);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x27215AC) = v2;
		}
		// 30 fps layer: see mag020_norg_pod_opening_held.inc
		FX_HELD(held_note_master();)
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_POD));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_CHUNK));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x89FE60 (MAG_020_sub_89FE60): master state - once no child is alive, the stage light ramp up
	// (0x89FE90, 0x40 bytes, stage queue), next state
	uint32_t __cdecl np_89FE60(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x89FE90, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x89FF30 (MAG_020_sub_89FF30; copy of Drain r_8517B0 0x8517B0): stage light ramp state 1 - level +0x1C
	// up by 0x100 per tick to 0x400 (then finished, next state), written to the four stage light words
	// 0x1D98992 + k * 0x2C
	uint32_t __cdecl np_89FF30(uint32_t a1)
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

	// 0x89FFB0 (MAG_020_sub_89FFB0; copy of Drink Magic dk_856D70 0x856D70 = Wind Blast 0x8797C0): EMITTER task
	// (one per action) - bone-follow anchor and model bounds, state {0x8A0040 the pod lid, 0x8A3070
	// release the master, 0x8A3090 damage, ret}, sound 0x1629C50 at its first tick
	uint32_t __cdecl np_89FFB0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x8A0040;
		states[1] = 0x8A3070;
		states[2] = 0x8A3090;
		states[3] = 0x8A30D0; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(SOUND_Open, 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A0040 (MAG_020_sub_8A0040): emitter state 0 - the pod lid task (0x8A0070, 0x78 bytes, pod
	// queue), next state
	uint32_t __cdecl np_8A0040(uint32_t a1)
	{
		x::Effect_AddTaskAndInitFromCtx(Q_POD, ORIG_Lid, 0x78, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A3070 (MAG_020_sub_8A3070; copy of Drain r_8567C0 0x8567C0): emitter state 1 - at tick 30 releases the
	// master (root +0x63 = 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl np_8A3070(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x1E)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x8A3090 (MAG_020_sub_8A3090; copy of Drain r_8567E0 0x8567E0): emitter state 2 - from tick 30: damage
	// (action result) of the action's current target (+0x2A action, +0x2B target record of 24
	// bytes), finished, next state
	uint32_t __cdecl np_8A3090(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x1E)
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

	// 0x8A30E0 (MAG_020_sub_8A30E0; copy of Drain r_856830 0x856830 with a 15-tick wait): master state - action
	// loop: while the master is not held (+0x63) and actions remain (+0x2A < +0x58), the next action
	// (+0x2A / +0x2E up, state back to the emitter spawn); after the last one waits 15 ticks (+0x60)
	// and goes on
	uint32_t __cdecl np_8A30E0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return a1; // void (eax = node)
		const uint8_t idx = U8(node, 0x2A);
		if ((int16_t)(int8_t)idx < S16(node, 0x58))
		{
			const uint8_t loops = U8(node, 0x2E);
			U8(node, 0x2A) = (uint8_t)(idx + 1);
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x2E) = (uint8_t)(loops + 1);
			U8(node, 0x29) = (uint8_t)(st - 1);
			return a1; // void
		}
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x60) = 0xF;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return a1; // void
	}

	// 0x8A3120 (MAG_020_sub_8A3120; copy of Drain r_856870 0x856870): master state - counts +0x60 down; at 0
	// starts the stage light ramp down (0x8A3150, 0x40 bytes, stage queue), next state
	uint32_t __cdecl np_8A3120(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8A3150, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A31B0 (MAG_020_sub_8A31B0; copy of Drain r_856900 0x856900): stage light ramp down state 0 - level
	// +0x1C = 0x400, next state
	uint32_t __cdecl np_8A31B0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x8A31D0 (MAG_020_sub_8A31D0; copy of Death d_8BDFC0 0x8BDFC0): stage light ramp down state 1 - level +0x1C
	// -= 0x100 down to 0 (then finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl np_8A31D0(uint32_t a1)
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

	// ====================================================================================
	// the pod lid (module code)
	// ====================================================================================

	// 0x8A0070 (MAG_020_sub_8A0070): TASK pod lid (one per emitter) - 4 states {0x8A2D30 set-up,
	// 0x8A2F70 wait, 0x8A2FE0 flight, ret}, then the debris spawner 0x8A00E0
	uint32_t __cdecl np_8A0070(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x8A2D30;
		states[1] = 0x8A2F70;
		states[2] = 0x8A2FE0;
		states[3] = 0x8A3060; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		np_8A00E0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A2D30 (sub_8A2D30): lid state 0 - voice slot 0x17F36A0 (-> +0x74), the lid at the caster's
	// position (+0x2C) raised by 0x1140, the caster's own draw off (entity +0x7C bit 1), drawn
	// (0x8A2D90), next state
	uint32_t __cdecl np_8A2D30(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2C));
		const uint32_t slot = x::BdSound_ClaimVoiceSlot(0x17F36A0, 1, 0x80);
		U16(node, 0x74) = (uint16_t)slot;
		U32(node, 0x1C) = U32(ent, 0x1C);
		U32(node, 0x20) = U32(ent, 0x20);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xEEC0);
		U32(ent, 0x7C) = U32(ent, 0x7C) & 0xFFFFFFFDu;
		np_8A2D90(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8A2D90 (sub_8A2D90): draws the lid - the caster's model with its bone 1 (bone matrices
	// [[entity +0x64]] + 0x50) raised by 0x1140 and moved by the lid's offset +0x58..+0x5C and angles
	// +0x68 (x) / +0x6A (y) / +0x6C (z), through RenderGeometry (mode 4) into the effect OT; the pose
	// is rebuilt before and after (entity +0x7C = 2 for the draw, then put back)
	uint32_t __cdecl np_8A2D90(uint32_t a1)
	{
		const uint32_t node = a1;
		// 30 fps layer: see mag020_norg_pod_opening_held.inc
		FX_HELD(held_note_lid(node);)
		const uint32_t ent = ENT(U8(node, 0x2C));
		uint32_t bones = U32(U32(ent, 0x64), 0) + 0x40;   // edi
		const uint32_t blk = x::Field_Alloc(0x8C);        // ebx
		const uint32_t saved7c = U32(ent, 0x7C);
		U32(ent, 0x7C) = 2;
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
		x::MAG_022_sub_8DD770(blk + 0x40);
		S32(blk, 0x54) = S16(node, 0x58);
		const int32_t oy = S16(node, 0x5A);
		S32(blk, 0x5C) = S16(node, 0x5C);
		U32(bones, 0x28) = U32(bones, 0x28) + 0x1140;
		S32(blk, 0x58) = oy - 0x1140;
		x::MAG_022_sub_8DD8A0(blk + 0x40, (uint32_t)(int32_t)S16(node, 0x6A));
		x::sub_8DD7E0(blk + 0x40, (uint32_t)(int32_t)S16(node, 0x68));
		x::sub_8DD960(blk + 0x40, (uint32_t)(int32_t)S16(node, 0x6C));
		bones += 0x10;
		x::ComposeAffineTransform(blk + 0x40, bones, bones);
		memcpy((void *)blk, (const void *)(ent + 0x40), 8 * 4); // rep movsd: the entity matrix
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(blk);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x20);
		x::GTE_LoadIRFromMatrixColumn(blk + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x22);
		x::GTE_LoadIRFromMatrixColumn(blk + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(blk + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(blk + 0x34);
		x::ComposeAffineTransform(blk + 0x20, bones, bones);
		U32(blk, 0x64) = MEM<uint32_t>(0x1D98B3C);
		const uint8_t shade = U8(ent, 7);
		U8(blk, 0x8A) = shade;
		U8(blk, 0x89) = shade;
		U8(blk, 0x88) = shade;
		U32(blk, 0x7C) = U32(ent, 0x28);
		U16(blk, 0x74) = 0;
		U16(blk, 0x76) = 0;
		U16(blk, 0x84) = 0;
		U32(blk, 0x80) = U32(ent, 0x7C);
		U16(blk, 0x78) = 0x140;
		U16(blk, 0x7A) = 0xD8;
		U32(blk, 0x70) = MEM<uint32_t>(0x1D969A8);
		MEM<uint32_t>(0x1D8E054) = x::RenderGeometry(U32(ent, 0x64), blk + 0x60, MEM<uint32_t>(0x1D8E04C) + 0x44, 4, MEM<uint32_t>(0x1D8E054));
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
		U32(ent, 0x7C) = saved7c;
		x::Field_Free(0x8C);
		return 0; // void
	}

	// 0x8A2F70 (sub_8A2F70): lid state 1 - from tick 34: the stage wobble (engine task 0x8DDC30, 0x40
	// bytes, stage queue: script 0x1629F14, +0x38 = 1), the lid's velocity (0x120, -0x140, 0x20),
	// the chain transformation 0xC of entity 0x1D97668, 24 flight ticks (+0x70), next state; the lid
	// is drawn (0x8A2D90) every tick
	uint32_t __cdecl np_8A2F70(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x22)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8DDC30, 0x40, node);
			U32(t, 0x34) = 0x1629F14;
			U16(t, 0x38) = 1;
			U16(node, 0x60) = 0x120;
			U16(node, 0x62) = 0xFEC0;
			U16(node, 0x64) = 0x20;
			gx::QueueChainTransformation(0x1D97668, 0xC);
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x70) = 0x18;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		np_8A2D90(node);
		return 0; // void
	}

	// 0x8A2FE0 (sub_8A2FE0): lid state 2 - flight: gravity +0x62 += 0x28, offset += velocity, angles
	// x -= 0x40 / z += 0x30 (12 bits); when the 24 ticks (+0x70) are over: setMonsterPresence(6), the
	// voice slot +0x74 released (0x4A2940), finished, next state; the lid is drawn (0x8A2D90) every
	// tick
	uint32_t __cdecl np_8A2FE0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t ax0 = U16(node, 0x68);
		U16(node, 0x62) = (uint16_t)(U16(node, 0x62) + 0x28);
		const uint16_t cz0 = U16(node, 0x6C);
		const uint16_t vx = U16(node, 0x60);
		U16(node, 0x58) = (uint16_t)(U16(node, 0x58) + vx);
		U16(node, 0x68) = (uint16_t)((uint16_t)(ax0 - 0x40) & 0xFFF);
		U16(node, 0x5A) = (uint16_t)(U16(node, 0x5A) + U16(node, 0x62));
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + U16(node, 0x64));
		const uint16_t cz = (uint16_t)((uint16_t)(cz0 + 0x30) & 0xFFF);
		U16(node, 0x70) = (uint16_t)(U16(node, 0x70) - 1);
		const bool over = S16(node, 0x70) <= 0;
		U16(node, 0x6C) = cz;
		if (over)
		{
			gx::setMonsterPresence(6);
			x::sub_4A2940((uint32_t)(int32_t)S16(node, 0x74));
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		np_8A2D90(node);
		return 0; // void
	}

	// ====================================================================================
	// debris (sprites) and chunks (prim models) (module code)
	// ====================================================================================

	// 0x8A00E0 (sub_8A00E0): debris spawner of the lid, every tick - when the lid's tick +0x24 reaches
	// the next entry of the tick table 0x1629F88 (index +0x72): 4 debris tasks (0x8A01B0, 0x78 bytes,
	// pod queue) at the lid + a random one of the 12 offsets 0x1629F28 (rand() % 12) + a random
	// jitter of -0x80..0x7F per axis (x negated for every other one)
	uint32_t __cdecl np_8A00E0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t k = U16(node, 0x72);
		if (MEM<uint16_t>(0x1629F88 + (uint32_t)(int32_t)(int16_t)k * 2) != U16(node, 0x24))
			return 0; // void
		U16(node, 0x72) = (uint16_t)(k + 1);
		for (int32_t i = 0; i < 4; i++)
		{
			const int32_t r = (int32_t)x::CrtRand();
			const uint32_t off = 0x1629F28 + (uint32_t)(r % 12) * 8;
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_POD, ORIG_Debris, 0x78, node);
			U32(t, 0x1C) = U32(node, 0x1C);
			U32(t, 0x20) = U32(node, 0x20);
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + MEM<uint16_t>(off));
			U16(t, 0x1E) = (uint16_t)(U16(t, 0x1E) + MEM<uint16_t>(off + 2));
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + MEM<uint16_t>(off + 4));
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((x::CrtRand() & 0xFF) - 0x80));
			U16(t, 0x1E) = (uint16_t)(U16(t, 0x1E) + (uint16_t)((x::CrtRand() & 0xFF) - 0x80));
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((x::CrtRand() & 0xFF) - 0x80));
			if ((i & 1) != 0)
				U16(t, 0x1C) = (uint16_t)(0 - U16(t, 0x1C));
		}
		return 0; // void
	}

	// 0x8A01B0 (sub_8A01B0): TASK debris - 3 states {0x8A2CB0 flipbook 0x1629C54 of 15 frames, 0x8A2CD0
	// frame step, ret}; at its ticks 0 and 1 one chunk (0x8A0900); drawn as a sprite sequence
	// (0x8A0230)
	uint32_t __cdecl np_8A01B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x8A2CB0;
		states[1] = 0x8A2CD0;
		states[2] = 0x8A2D20; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint16_t tick = U16(node, 0x24);
		if (tick == 0 || tick == 1)
			np_8A0900(node, 1);
		np_8A0230(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A2CB0 (sub_8A2CB0; Raldo Throw rt_8AA9B0 0x8AA9B0 with other values): debris state 0 - sprite +0x4C =
	// 0x1629C54, +0x56 = -0x200, +0x52 = 15, advance.
	uint32_t __cdecl np_8A2CB0(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x1629C54;
		st++;
		U16(node, 0x56) = 0xFE00;
		U16(node, 0x52) = 0xF;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x8A2CD0 (copy of Raldo Throw rt_8AA9D0 0x8AA9D0 = t_767430 0x767430: module 090 sub_767430): particle
	// state 1 - steps the prim animation (0x8A2CF0 = a_743C20); at its end hides and finishes the
	// task (+0x26 |= 5), next state.
	uint32_t __cdecl np_8A2CD0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8A0230 (copy of Doom dm_8CD770 0x8CD770 = t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a
	// particle node (unless hidden, +0x26 bit2): builds camera rotation * position in a 0xD8-byte
	// scratch block, fills the prim parameter block (layout +0x4C, frame +0x50, colour +0x56) and
	// emits it with 0x8A0350 (= a_7435E0) into the effect OT (OT base +0x44) from the module packet
	// cursor [0x2721594].
	uint32_t __cdecl np_8A0230(uint32_t a1)
	{
		const uint32_t node = a1;
		// 30 fps layer: see mag020_norg_pod_opening_held.inc
		FX_HELD(held_note_debris(node);)
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
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(PACKET_CURSOR) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x8A0900 (sub_8A0900): spawns a2 (s16) chunk tasks (0x8A0990, 0x98 bytes, chunk queue) at the
	// debris a1: each carries two pieces at the debris position (+0x54 / +0x5C, 8 bytes each) with
	// random velocities (+0x74 / +0x7C): x and z = (rand & 0x3FFF) - 0x2000, y = -0x100 - (rand & 0xFFF)
	uint32_t __cdecl np_8A0900(uint32_t a1, uint32_t a2)
	{
		int32_t n = (int16_t)a2;
		if (n <= 0)
			return 0; // void
		const uint32_t src = a1;
		do
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_CHUNK, ORIG_Chunk, 0x98, src);
			uint32_t v = t + 0x74;
			for (int k = 2; k != 0; k--, v += 8)
			{
				U32(v, -0x20) = U32(src, 0x1C);
				U32(v, -0x1C) = U32(src, 0x20);
				U16(v, 0) = (uint16_t)((x::CrtRand() & 0x3FFF) - 0x2000);
				U16(v, 4) = (uint16_t)((x::CrtRand() & 0x3FFF) - 0x2000);
				const uint32_t r = x::CrtRand() & 0xFFF;
				U16(v, 2) = (uint16_t)(0xFFFFFF00u - r);
			}
		} while (--n != 0);
		return 0; // void
	}

	// 0x8A0990 (sub_8A0990): TASK chunk - 3 states {0x8A2C20 set-up, 0x8A2C60 spin / shrink, ret}, then
	// for each of its two pieces: velocity y += 0x100, velocity -= velocity / 8 (per axis), position
	// += velocity / 16; each piece drawn (0x8A0AA0) at its position (copied to +0x1C..+0x23)
	uint32_t __cdecl np_8A0990(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x8A2C20;
		states[1] = 0x8A2C60;
		states[2] = 0x8A2CA0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		for (int k = 0; k < 2; k++)
			chunk_move(node + 8 * k);
		// 30 fps layer: see mag020_norg_pod_opening_held.inc
		FX_HELD(held_note_chunk(node);)
		for (int k = 0; k < 2; k++)
		{
			const uint32_t p = node + 0x54 + 8 * k;
			U32(node, 0x1C) = U32(p, 0);
			U32(node, 0x20) = U32(p, 4);
			np_8A0AA0(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A2C20 (sub_8A2C20): chunk state 0 - prim model 0x1629E00, scale 0x300 (+0x30..+0x38), angle
	// +0x48 = rand & 0xFFF, next state
	uint32_t __cdecl np_8A2C20(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = 0x1629E00;
		U32(node, 0x38) = 0x300;
		U32(node, 0x34) = 0x300;
		U32(node, 0x30) = 0x300;
		U16(node, 0x48) = (uint16_t)(x::CrtRand() & 0xFFF);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8A2C60 (sub_8A2C60): chunk state 1 - angle +0x48 += 0x20; from tick 8 the scale shrinks by
	// 0xC0 a tick (at <= 0: scale 0x10, finished, next state)
	uint32_t __cdecl np_8A2C60(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x48) = (uint16_t)(U16(node, 0x48) + 0x20);
		if (S16(node, 0x24) < 8)
			return 0; // void
		const int32_t s = (int32_t)(U32(node, 0x38) + 0xFFFFFF40u);
		S32(node, 0x38) = s;
		if (s <= 0)
		{
			U8(node, 0x26) |= 1;
			U32(node, 0x38) = 0x10;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		const uint32_t sc = U32(node, 0x38);
		U32(node, 0x34) = sc;
		U32(node, 0x30) = sc;
		return 0; // void
	}

	// 0x8A0AA0 (sub_8A0AA0): draws a chunk piece (unless hidden, +0x26 bit 2): matrix from the angles
	// +0x46 (y) / +0x44 (x) / +0x48 (z) (0x8DD770 / 0x8DD8A0 / 0x8DD7E0 / 0x8DD960) at the position
	// +0x1C..+0x20, scaled by +0x30, composed with the camera 0x1D97778, into the GTE; a 0x68-byte
	// render header (model +0x4C, +0x40, +0x50, +0x52, flags 0xF0, unit scales 0x100) drawn by the
	// module's prim-model renderer 0x8A0BC0 into the effect OT from the module packet cursor
	uint32_t __cdecl np_8A0AA0(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
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
		MEM<uint32_t>(PACKET_CURSOR) = np_8A0BC0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// ====================================================================================
	// the module's prim-model renderer (Raldo Throw's ports with this module's addresses)
	// ====================================================================================

	// 0x8A0BC0 (copy of Raldo Throw rt_8A4830 0x8A4830 = copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8A0CF0, 0x8A0F10, 0x8A11A0, 0x8A1620, a_73D770, a_73D9C0, 0x8A20D0,
	// 0x8A25D0; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl np_8A0BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { np_8A0CF0, np_8A0F10, np_8A11A0, np_8A1620, a_73D770, a_73D9C0, np_8A20D0, np_8A25D0 };
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

	// 0x8A0CF0 (copy of Raldo Throw rt_8A4960 0x8A4960 = copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl np_8A0CF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A0F10 (copy of Raldo Throw rt_8A4B80 0x8A4B80 = copy of t_764890 0x764890: module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl np_8A0F10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A11A0 (copy of Raldo Throw rt_8A4E10 0x8A4E10 = copy of s_73CD70 0x73CD70: module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl np_8A11A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A1620 (copy of Raldo Throw rt_8A5290 0x8A5290 = copy of s_73D1F0 0x73D1F0: module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl np_8A1620(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A20D0 (copy of Raldo Throw rt_8A5D40 0x8A5D40 = sub_8A5D40; same code as Siren's 0x73DCA0 / Drink Magic's 0x85D990): prim-list block
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
	uint32_t __cdecl np_8A20D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A25D0 (copy of Raldo Throw rt_8A6240 0x8A6240 = copy of s_73E1A0 0x73E1A0: module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl np_8A25D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x89FE40, (void *)a_73A0D0, "020 MAG_020_sub_89FE40" },
		{ 0x89FE50, (void *)a_73A0D0, "020 MAG_020_sub_89FE50" },
		{ 0x89FE90, (void *)a_73A380, "020 MAG_020_sub_89FE90" },
		{ 0x89FEF0, (void *)a_7335D0, "020 MAG_020_sub_89FEF0" },
		{ 0x89FF70, (void *)a_73A6D0, "020 nullsub_1769" },
		{ 0x89FF80, (void *)a_73A170, "020 MAG_020_sub_89FF80" },
		{ 0x8A0350, (void *)a_7435E0, "020 InitEffectSequenceFromData_c15" },
		{ 0x8A0490, (void *)a_743720, "020 sub_8A0490" },
		{ 0x8A1BA0, (void *)a_73D770, "020 sub_8A1BA0" },
		{ 0x8A1DF0, (void *)a_73D9C0, "020 sub_8A1DF0" },
		{ 0x8A2CA0, (void *)a_73A6D0, "020 nullsub_1770" },
		{ 0x8A2CF0, (void *)a_743C20, "020 sub_8A2CF0" },
		{ 0x8A2D20, (void *)a_73A6D0, "020 nullsub_1771" },
		{ 0x8A3060, (void *)a_73A6D0, "020 nullsub_1772" },
		{ 0x8A30D0, (void *)a_73A6D0, "020 nullsub_1773" },
		{ 0x8A3150, (void *)a_73A380, "020 MAG_020_sub_8A3150" },
		{ 0x8A3210, (void *)a_73A6D0, "020 nullsub_1775" },
		{ 0x8A3220, (void *)a_747550, "020 MAG_020_sub_8A3220" },
		{ 0x8A3230, (void *)a_73A0D0, "020 MAG_020_sub_8A3230" },
		{ 0x8A3240, (void *)a_73A0D0, "020 MAG_020_sub_8A3240" },
		{ 0x8A3250, (void *)a_7475A0, "020 MAG_020_sub_8A3250" },
		{ 0x8A3270, (void *)a_73A6D0, "020 nullsub_1774" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x89FD00, (void *)np_89FD00, "020 np_89FD00" },
		{ 0x89FE60, (void *)np_89FE60, "020 np_89FE60" },
		{ 0x89FF30, (void *)np_89FF30, "020 np_89FF30" },
		{ 0x89FFB0, (void *)np_89FFB0, "020 np_89FFB0" },
		{ 0x8A0040, (void *)np_8A0040, "020 np_8A0040" },
		{ 0x8A0070, (void *)np_8A0070, "020 np_8A0070" },
		{ 0x8A00E0, (void *)np_8A00E0, "020 np_8A00E0" },
		{ 0x8A01B0, (void *)np_8A01B0, "020 np_8A01B0" },
		{ 0x8A0230, (void *)np_8A0230, "020 np_8A0230" },
		{ 0x8A0900, (void *)np_8A0900, "020 np_8A0900" },
		{ 0x8A0990, (void *)np_8A0990, "020 np_8A0990" },
		{ 0x8A0AA0, (void *)np_8A0AA0, "020 np_8A0AA0" },
		{ 0x8A0BC0, (void *)np_8A0BC0, "020 np_8A0BC0" },
		{ 0x8A0CF0, (void *)np_8A0CF0, "020 np_8A0CF0" },
		{ 0x8A0F10, (void *)np_8A0F10, "020 np_8A0F10" },
		{ 0x8A11A0, (void *)np_8A11A0, "020 np_8A11A0" },
		{ 0x8A1620, (void *)np_8A1620, "020 np_8A1620" },
		{ 0x8A20D0, (void *)np_8A20D0, "020 np_8A20D0" },
		{ 0x8A25D0, (void *)np_8A25D0, "020 np_8A25D0" },
		{ 0x8A2C20, (void *)np_8A2C20, "020 np_8A2C20" },
		{ 0x8A2C60, (void *)np_8A2C60, "020 np_8A2C60" },
		{ 0x8A2CB0, (void *)np_8A2CB0, "020 np_8A2CB0" },
		{ 0x8A2CD0, (void *)np_8A2CD0, "020 np_8A2CD0" },
		{ 0x8A2D30, (void *)np_8A2D30, "020 np_8A2D30" },
		{ 0x8A2D90, (void *)np_8A2D90, "020 np_8A2D90" },
		{ 0x8A2F70, (void *)np_8A2F70, "020 np_8A2F70" },
		{ 0x8A2FE0, (void *)np_8A2FE0, "020 np_8A2FE0" },
		{ 0x8A3070, (void *)np_8A3070, "020 np_8A3070" },
		{ 0x8A3090, (void *)np_8A3090, "020 np_8A3090" },
		{ 0x8A30E0, (void *)np_8A30E0, "020 np_8A30E0" },
		{ 0x8A3120, (void *)np_8A3120, "020 np_8A3120" },
		{ 0x8A31B0, (void *)np_8A31B0, "020 np_8A31B0" },
		{ 0x8A31D0, (void *)np_8A31D0, "020 np_8A31D0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag020_norg_pod_opening()
	{
		act::register_module(20);
		for (const act::norgpod::ModPort *p = act::norgpod::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(20, p->addr, p->port, p->name);
		for (const act::norgpod::ModPort *p = act::norgpod::PORTS; p->addr; p++)
			act::register_module_port(20, p->addr, p->port, p->name);
		// 30 fps layer: see mag020_norg_pod_opening_held.inc
		FX_HELD(register_mag020_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag020_norg_pod_opening_held.inc"
#endif
