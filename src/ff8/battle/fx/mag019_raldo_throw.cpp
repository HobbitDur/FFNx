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

// Effect 19: Raldo Ball / Raldo Throw (enemy attacks 244 / 245 / 246 of kernel.bin, MAG_019_*: Granaldo
// c0m095 throwing one of its Raldos - one attack per Raldo slot; its AI abilities 1-3 "Raldo Ball",
// 4-6 "Raldo Throw" all play this effect): a copy of the actor/heal effect library (see act_engine.h)
// built like Death / Dribble, plus the module's own Raldo ball (Dribble's ball renderer with the
// module's block layout) and debris.
//
// Setup MAG_019_UNK0 0x8A32A0 (runs once, not ported; file loader 0x8A3280 = the effect's data file
// -> 0x2726900, its two packet arenas at file + 0 / + 0x4000 by tick parity, arena cursor 0x2730490 =
// file + 0x8000; camera animation 0x162A204 and the file's TIM upload unless the cast flags +1 bit 0)
// creates the root queue 0x272B9C8 with the master and five task pools: 0x2730270 (4 x 0x58:
// emitters), 0x2726AB0 (3 x 0x48: director), 0x272E090 (1 x 0x2A4: the ball actor system), 0x272B9D8
// (50 x 0xB8: the Raldo, debris spawners and debris), 0x2730260 (10 x 0x40: the engine's stage wobble
// task 0x8DDC30).
//   Master (0x8A33F0) - camera copy 0x2793E58 (the module scratch stack 0x2730484 grows down from it),
//     packet arena by tick parity (cursor 0x2726904), bone follow, 11-state table: 0x8A3570 carves
//     the actor pools after the arena (0x438 + 0xB7C0 bytes: particles and 70 actors of 0x2A0),
//     0x8A35F0 one emitter per action (engine loop 0x8AB070), then waits for the queues to empty.
//   Emitter (0x8A3620, engine task a_73A1A0), node +0x2C = caster, +0x2D = target: 0x8A36A0 keeps
//     both positions and the caster's facing in the director block [0x1629FA8] (0x8A36D0) and
//     starts the director; 0x8AAFE0 applies the damage once the ball hit (director +0x48 == 1).
//   Director (0x8A3760): phase bookkeeping 0x8A37E0 (the phase the caster's animation requests in
//     director +0x42 / +0x44) - phase 1 starts (0x8A3940) the ball actor system (0x8A3A30 = engine
//     0x73A380 on {0x8A3A90, 0x8A93B0, ret}: actor data 0x17F37BC, start tick 0x1C, placed at the
//     caster's x / z, y -0x300) and the Raldo task, whose entity is the entity of kind byte 0x70
//     standing at the caster (the Raldo it carries); then waits for the cues and the phase ticks.
//   Raldo (0x8A93E0): 10 states on the ball block 0x2726910 (0xC4 bytes: the Dribble ball block
//     with the entity slot at +0xAC, morph +0xA4, radii +0xA6..+0xAA) - set-up (the Raldo's
//     position / rotation kept, turned to the caster's facing), sound 0x1629FAC at tick 0x1C, the
//     take (its own draw hidden, pivot = its bone 0xF1, radii 0x1C0, destination = the target's
//     bone 0xF1, stage wobble task 0x8DDC30 (data 0x162A470), debris), the throw (morph into a
//     ball +0x200 a tick, spin, progress +0x200 a tick along start -> destination, a fall of +0x80
//     a tick; at the end the hit flag for the emitter), the rebound back (progress -0x100 a tick,
//     two bounces with debris), unmorph (-0x300 a tick), the Raldo given back, 8 ticks later its
//     rotation; while the ball is shown the Raldo entity is placed at the ball and drawn by the
//     module's ball renderer (0x8A94E0 / 0x8A95F0 = Dribble's 0x84BA60 / 0x84BB70).
//   Debris spawner (0x8A9F10, +0xB6 ticks): each tick a low burst (0x8AAA30, flipbook 0x162A0C0 of
//     11 frames) and a high burst (0x8AA1D0, flipbook 0x1629FB0 of 9 frames) of 4 particles each at
//     the ball (random angle, radius and speed), drawn as sprite sequences (0x8AA2E0) then slowed by
//     a quarter a tick with gravity.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2726900..0x2730498 (file pointer 0x2726900, packet cursor 0x2726904, pools,
// queues, arenas, ball block 0x2726910, actor state 0x2730494, scratch stack pointer 0x2730484,
// geometry descriptor 0x27303E8, vertex scratch 0x272E0A0), the director block [0x1629FA8], the
// module scratch stack below the camera copy 0x2793E58; the actor data 0x17F37BC.. in exe data is
// rewritten in place.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace raldo
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	// (F_73B640 = 0x8A3850, the director cue gate writing +0x44; gendesc also saw 0x8A38A0 there)
	static const Mod MOD_019 = { "granaldo", 19, 0x8A3280, 0x8AB120,
		{ 0x0, 0x0, 0x1629FA8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x272B8F8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x272E090, 0x0, 0x2730270, 0x0, 0x0, 0x0, 0x0, 0x0, 0x273046C, 0x0, 0x0, 0x273047E, 0x0, 0x2730484, 0x0, 0x2730494 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A4670, 0x8A46D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A6890, 0x8A69D0, 0x8A6A10, 0x8A7CA0, 0x8A7CF0, 0x8A7D20, 0x8A7DA0, 0x8A8490, 0x8A84E0, 0x8A8580, 0x8A8640, 0x8A8E90, 0x8A9290, 0x8A9320, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A3620, 0x8A36A0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A3850, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8A3CB0, 0x8A3D90, 0x8A3F30, 0x8A4000, 0x8A4120, 0x8A4380, 0x8A6950, 0x8A6AA0, 0x8A7100, 0x8A7150, 0x8A71E0, 0x8A72B0, 0x8A7A40, 0x8A7B50, 0x8A7BD0, 0x8A7C00, 0x8A8420, 0x8A8450, 0x8A8530, 0x8A8650, 0x8A87A0, 0x0, 0x8A8E60, 0x8A9040, 0x8A9180, 0x8AA540, 0x8A45E0, 0x8AAFE0, 0x8AB020, 0x8AB040, 0x8AB060, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2726904;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x2730494;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x2730484;      // module scratch stack pointer
	static const uint32_t DIR_PTR = 0x1629FA8;         // director block pointer (0x54 bytes)
	static const uint32_t BLK = 0x2726910;             // the ball block (0xC4 bytes)
	static const uint32_t Q_EMITTER = 0x2730270, Q_DIRECTOR = 0x2726AB0, Q_BALL = 0x272E090, Q_RALDO = 0x272B9D8, Q_STAGE = 0x2730260;
	static const uint32_t ORIG_Master = 0x8A33F0;
	static const uint32_t ORIG_Ball = 0x8A3A30;        // the ball actor system (engine 0x73A380)
	static const uint32_t ORIG_Raldo = 0x8A93E0;
	static const uint32_t ORIG_DebrisHigh = 0x8AA1D0, ORIG_DebrisLow = 0x8AAA30;
	static const uint32_t BALL_DATA = 0x17F37BC;       // the ball actor system's data (exe data)
	static const uint32_t SOUND_Throw = 0x1629FAC;
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
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

	// director block and state helpers of the Tonberry / Death copies (the module's director block [0x1629FA8])
	static inline uint32_t t1_data() { return MEM<uint32_t>(DIR_PTR); }
	static inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl rt_8A33F0(uint32_t a1);
	uint32_t __cdecl rt_8A3570(uint32_t a1);
	uint32_t __cdecl rt_8A35B0(void);
	uint32_t __cdecl rt_8A36A0(uint32_t a1);
	uint32_t __cdecl rt_8A36D0(uint32_t a1);
	uint32_t __cdecl rt_8A3760(uint32_t a1);
	uint32_t __cdecl rt_8A37E0(uint32_t a1);
	uint32_t __cdecl rt_8A3880(uint32_t a1);
	uint32_t __cdecl rt_8A38C0(uint32_t a1);
	uint32_t __cdecl rt_8A38E0(uint32_t a1);
	uint32_t __cdecl rt_8A3940(uint32_t a1);
	uint32_t __cdecl rt_8A3A90(uint32_t a1);
	uint32_t __cdecl rt_8A3CB0(uint32_t a1);
	uint32_t __cdecl rt_8A3F30(void);
	uint32_t __cdecl rt_8A45E0(uint32_t a1);
	uint32_t __cdecl rt_8A4670(void);
	uint32_t __cdecl rt_8A46D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A4830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A4960(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A4B80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A4E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A5290(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A5D40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A6240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A69D0(void);
	uint32_t __cdecl rt_8A6A10(uint32_t a1);
	uint32_t __cdecl rt_8A7A40(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A7B50(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A7BD0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A7C00(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rt_8A7CA0(uint32_t a1);
	uint32_t __cdecl rt_8A7D20(uint32_t a1);
	uint32_t __cdecl rt_8A7DA0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rt_8A8580(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A87A0(uint32_t a1);
	uint32_t __cdecl rt_8A9290(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A9320(void);
	uint32_t __cdecl rt_8A93B0(uint32_t a1);
	uint32_t __cdecl rt_8A93E0(uint32_t a1);
	uint32_t __cdecl rt_8A94E0(uint32_t a1);
	uint32_t __cdecl rt_8A95F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rt_8A9CA0(uint32_t a1);
	uint32_t __cdecl rt_8A9D20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl rt_8A9DA0(uint32_t a1);
	uint32_t __cdecl rt_8A9DD0(uint32_t a1);
	uint32_t __cdecl rt_8A9EC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8A9F10(uint32_t a1);
	uint32_t __cdecl rt_8A9F60(uint32_t a1);
	uint32_t __cdecl rt_8AA1D0(uint32_t a1);
	uint32_t __cdecl rt_8AA2E0(uint32_t a1);
	uint32_t __cdecl rt_8AA9B0(uint32_t a1);
	uint32_t __cdecl rt_8AA9D0(uint32_t a1);
	uint32_t __cdecl rt_8AAA30(uint32_t a1);
	uint32_t __cdecl rt_8AAB40(uint32_t a1);
	uint32_t __cdecl rt_8AAB60(uint32_t a1);
	uint32_t __cdecl rt_8AABA0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rt_8AAC50(uint32_t a1);
	uint32_t __cdecl rt_8AAD90(uint32_t a1);
	uint32_t __cdecl rt_8AAEC0(uint32_t a1);
	uint32_t __cdecl rt_8AAF00(uint32_t a1);
	uint32_t __cdecl rt_8AAF60(uint32_t a1);
	uint32_t __cdecl rt_8AAFE0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag019_raldo_throw_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace raldo
{
	// ====================================================================================
	// the library functions of this module whose code matches a Death / Dribble / Draw / Drink Magic /
	// Drain / Magma Breath / Doom / Tonberry / Siren port up to the module's addresses and callees (the
	// port's text with the module's addresses; "copy of" names the source), then the module's own code
	// ====================================================================================

	// 0x8A33F0 (copy of mb_824E40 0x824E40: copy of cl_873740 0x873740: MAG_030_sub_873740): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the five queues (live task count -> node +0x5E; the actor counters
	// 0x26D6A22 / 0x26D6A12 are cleared before the queues run)
	uint32_t __cdecl rt_8A33F0(uint32_t a1)
	{
		g_mod = &MOD_019;
		// 30 fps layer: see mag019_raldo_throw_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x272B8F8) = 0x2793E58;
		states[0] = 0x8A3550;
		states[1] = 0x8A3560;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x8A3570;
		states[3] = 0x8A35F0;
		states[4] = 0x8AB070;
		states[5] = 0x8AB0B0;
		states[6] = 0x8AB0C0;
		states[7] = 0x8AB0D0;
		states[8] = 0x8AB0E0;
		states[9] = 0x8AB0F0;
		states[10] = 0x8AB110; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x27303E4);
			const uint32_t v2 = MEM<uint32_t>(0x272DDE4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x272B8F4) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x27303E0);
			const uint32_t v2 = MEM<uint32_t>(0x272DDE0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x272B8F4) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x273047E) = 0;
		MEM<uint16_t>(0x273046C) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_BALL));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_RALDO));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A3570 (copy of dk_856CC0 0x856CC0: copy of dw_8B0BC0 0x8B0BC0: copy of dm_8C4CE0 0x8C4CE0: copy of d_8B6BE0 0x8B6BE0: MAG_014_sub_8B6BE0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x2742870] (0x438 + 0xB7C0 bytes) and cleared, next state
	uint32_t __cdecl rt_8A3570(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x2730490);
		MEM<uint32_t>(0x2730470) = p;
		p += 0x438;
		MEM<uint32_t>(0x2730488) = p;
		p += 0xB7C0;
		MEM<uint32_t>(0x2730490) = p;
		rt_8A35B0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A35B0 (copy of dk_856D00 0x856D00: copy of dw_8B0C00 0x8B0C00: copy of dm_8C4D20 0x8C4D20: copy of d_8B6C20 0x8B6C20: MAG_014_sub_8B6C20): clears the two actor pools (0x438 bytes at [0x275F3EC], 0xB7C0
	// at [0x275F980]) and the pool cursors / counters
	uint32_t __cdecl rt_8A35B0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2730470), 0x438);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2730488), 0xB7C0);
		MEM<uint16_t>(0x272DDD8) = 0;
		MEM<uint16_t>(0x273047C) = 0;
		MEM<uint16_t>(0x2730414) = 0;
		MEM<uint16_t>(0x2726908) = 0;
		return 0; // void
	}

	// 0x8A36A0 (copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - places the reaper, spawns the director
	// 0x8B6E30 (0x48 bytes, director queue), next state
	uint32_t __cdecl rt_8A36A0(uint32_t a1)
	{
		rt_8A36D0(a1);
		x::Effect_AddTaskAndInitFromCtx(0x2726AB0, 0x8A3760, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A37E0 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl rt_8A37E0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			rt_8A3940(a1);
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

	// 0x8A3880 (copy of t_763B90 0x763B90: module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl rt_8A3880(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8A38C0 (copy of s_73B880 0x73B880: module; Siren sub_73B880): director state: advances once the cut frame count
	// (+0x46) reaches 4
	uint32_t __cdecl rt_8A38C0(uint32_t a1)
	{
		uint32_t blk = U32(0x1629FA8, 0);
		if (S16(blk, 0x46) < 4) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8A38E0 (copy of dm_8C5610 0x8C5610: copy of s_73BA30 0x73BA30: module; Siren sub_73BA30): director state: advances after cut frame 0x2D
	uint32_t __cdecl rt_8A38E0(uint32_t a1)
	{
		uint32_t blk = U32(0x1629FA8, 0);
		if (S16(blk, 0x46) <= 0x2D) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8A3CB0 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl rt_8A3CB0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x2730494);
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

	// 0x8A3F30 (copy of mb_825830 0x825830: copy of db_846EA0 0x846EA0: copy of cg_87E620 0x87E620: sub_87E620; = Confuse c_85FFC0, copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl rt_8A3F30(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x2730494);
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

	// 0x8A45E0 (copy of r_852560 0x852560: copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl rt_8A45E0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x2730494, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			rt_8A9320();
			rt_8A69D0();
			// 30 fps layer: see mag019_raldo_throw_held.inc
			FX_HELD(held_note_system(sys);)
			a_740700();
			rt_8A4670();
			sys = U32(0x2730494, 0);
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
		U16(0x273047E, 0) = (uint16_t)(U16(0x273047E, 0) + c14);
		U16(0x273046C, 0) = (uint16_t)(U16(0x273046C, 0) + c16);
		return done;
	}

	// 0x8A4670 (copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl rt_8A4670(void)
	{
		uint32_t act = U32(U32(0x2730494, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x2730494, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					rt_8A46D0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8A46D0 (copy of dm_8C9530 0x8C9530: copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl rt_8A46D0(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = rt_8A4830(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = rt_8A4830(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
			}
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// 0x8A4830 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8A4960, 0x8A4B80, 0x8A4E10, 0x8A5290, a_73D770, a_73D9C0, 0x8A5D40,
	// 0x8A6240; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl rt_8A4830(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { rt_8A4960, rt_8A4B80, rt_8A4E10, rt_8A5290, a_73D770, a_73D9C0, rt_8A5D40, rt_8A6240 };
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

	// 0x8A4960 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl rt_8A4960(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A4B80 (copy of t_764890 0x764890: module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl rt_8A4B80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rt_8A4E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rt_8A5290(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A6240 (copy of s_73E1A0 0x73E1A0: module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl rt_8A6240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A69D0 (copy of mb_827790 0x827790: copy of db_848EB0 0x848EB0: copy of cg_8804D0 0x8804D0: Effect_ParticleEmitter_UpdateParticleList; = Confuse c_861E70, copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl rt_8A69D0(void)
	{
		uint32_t act = U32(U32(0x2730494, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				rt_8A7CA0(act);
			else if (type == 1)
				rt_8A6A10(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8A6A10 (copy of dw_8B3110 0x8B3110: copy of dm_8C9810 0x8C9810: copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl rt_8A6A10(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x2730494);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (rt_8A7B50(act, bone) == 0)
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
			st = MEM<uint32_t>(0x2730494);
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x8A7A40 (copy of mb_828BA0 0x828BA0: copy of db_849F20 0x849F20: copy of cg_881540 0x881540: sub_881540; = Confuse c_862EE0, copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl rt_8A7A40(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x2730494, 0);                  // actor state block
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

	// 0x8A7B50 (copy of dw_8B4250 0x8B4250: copy of dm_8CA950 0x8CA950: copy of mb_828CB0 0x828CB0: copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl rt_8A7B50(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				rt_8A7BD0(a1, a2);
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
			rt_8A7BD0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			rt_8A7BD0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x8A7BD0 (copy of dw_8B42D0 0x8B42D0: copy of dm_8CA9D0 0x8CA9D0: copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl rt_8A7BD0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return rt_8A7C00(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x8A7C00 (copy of dw_8B4300 0x8B4300: copy of dm_8CAA00 0x8CAA00: copy of d_8BBC10 0x8BBC10: sub_8BBC10): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 10 records
	uint32_t __cdecl rt_8A7C00(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x2730470);
		int32_t idx = MEM<int16_t>(0x272DDD8);
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
			MEM<uint16_t>(0x272DDD8) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x272DDD8) = 0;
		return slot;
	}

	// 0x8A7CA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl rt_8A7CA0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x8A8650, a1);
		else if (mode == 4)
			callp(0x8A87A0, a1);
		rt_8A7D20(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8A7D20 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl rt_8A7D20(uint32_t a1)
	{
		uint32_t sys = U32(0x2730494, 0);
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
				rt_8A7DA0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			rt_8A7DA0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8A7DA0 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl rt_8A7DA0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x2730484, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x2730484, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = rt_8A8580(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x2730494, 0);
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
		rt_8A9290(node, desc);
		U32(0x2730484, 0) = U32(0x2730484, 0) + 0x50;
		return 0; // void
	}

	// 0x8A8580 (copy of d_8BC590 0x8BC590: sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x46 records
	uint32_t __cdecl rt_8A8580(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x2730488);
		int32_t i = MEM<int16_t>(0x273047C);
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
			MEM<uint16_t>(0x273047C) = 0;
		else
			MEM<uint16_t>(0x273047C) = (uint16_t)i;
		return slot;
	}

	// 0x8A87A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl rt_8A87A0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x2730494, 0);            // ecx
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
				uint32_t g = U32(0x2730494, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x2730494, 0);
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

	// 0x8A9290 (copy of dw_8B5990 0x8B5990: copy of dm_8CC090 0x8CC090: copy of mb_82A3F0 0x82A3F0: copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl rt_8A9290(uint32_t a1, uint32_t a2)
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
			rt_8A7C00(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			rt_8A7C00(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			rt_8A7C00(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			rt_8A7C00(obj, id, variant);
		}
		return 0; // void
	}

	// 0x8A9320 (copy of dw_8B5A20 0x8B5A20: copy of dm_8CC120 0x8CC120: copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via rt_8A7C00(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl rt_8A9320(void)
	{
		uint32_t dir = MEM<uint32_t>(0x2730494);  // eax (re-read only after the calls)
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
					rt_8A7C00(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x2730494);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				rt_8A7C00(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x2730494);
			}
		}
		return 0; // void
	}

	// 0x8A93B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl rt_8A93B0(uint32_t a1)
	{
		if (rt_8A45E0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x8A94E0 (copy of db_84BA60 0x84BA60: sub_84BA60; = ChocoBocle 0x716CD0 with the entity index at +0xB0 (here +0xAC)): draws the target
	// entity (block a1 = 0x2726910): copies the entity's 0x20-byte world matrix (+0x40) to a1
	// +0x20, builds its bone matrices, composes camera x world into a1 +0x40 (rotation) / +0x54
	// (translation), draws the entity shadow (sub_5088A0 into OT +0x4040, frame arena cursor a1
	// +0x6C), then the body (entity +0x64) and the weapon (entity +0x78 -> +4) with the ball
	// renderer 0x84BB70, and rebuilds the bone matrices
	uint32_t __cdecl rt_8A94E0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)((int32_t)S16(a1, 0xAC) * 0x9C);
		memcpy((void *)(a1 + 0x20), (const void *)(ent + 0x40), 0x20);
		const uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x40);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x42);
		x::GTE_LoadIRFromMatrixColumn(a1 + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(a1 + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(a1 + 0x54);
		uint32_t cp = U32(a1, 0x6C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x4040;
		const uint32_t r = x::sub_5088A0(ent, ot, 0x10, U32(cp, 0));
		cp = U32(a1, 0x6C);
		const uint32_t ot44 = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(cp, 0) = r;
		rt_8A95F0(U32(ent, 0x64), ot44, 4, a1);
		const uint32_t wpn = U32(ent, 0x78);
		if (wpn != 0)
			rt_8A95F0(U32(wpn, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, a1);
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x8A95F0 (copy of db_84BB70 0x84BB70: sub_84BB70): ball renderer of a battle model (a1 = geometry {+0 bone matrices -
	// 0x10, +4 object table}, a2 = OT, a3 = depth shift, a4 = block 0x2726910 {+0 ball rotation,
	// +0x40 camera matrix, +0x64 -> {+4 vertex scratch}, +0x68 entity, +0x6C packet cursor pointer,
	// +0x74 pivot, +0x9C / +0x9E / +0xA0 ball angles, +0xA8 morph, +0xAA..+0xAE ball radii; here +0xA4 / +0xA6..+0xAA}): per
	// visible object (entity +0x7C mask) every vertex is put in world space by its bone (light
	// matrix + background vector), taken relative to the pivot, and morphed between itself (weight
	// 0x1000 - morph) and its direction scaled by the radii (weight morph) through the GTE
	// interpolation (0x45E0B0 / 0x45E9D0 / 0x45EBF0), turned by the ball rotation (colour matrix,
	// ColorV0) and put back at the pivot (20-byte scratch records {x, y, z, pad, sxy, sz, -});
	// projected (RTPS), then back-face-culled textured triangles (POLY_FT3, 7 words) and quads
	// (POLY_FT4, 9 words) tinted with the entity colour (+0x28), inserted with
	// SSIGPU_InsertPrimDepthKeys
	uint32_t __cdecl rt_8A95F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t blk = a4;
		const uint32_t verts = U32(U32(blk, 0x64), 4);  // vertex scratch (ebx)
		const uint32_t ent = U32(blk, 0x68);
		uint32_t cursor = U32(U32(blk, 0x6C), 0);       // packet cursor (local +0x1C)
		const uint32_t bones = U32(a1, 0) + 0x10;       // local +0x30
		uint32_t tab = U32(a1, 4);                      // object table {count, offsets[count]}
		const int32_t count = S32(tab, 0);              // local +0x2C
		const uint32_t w = x::Field_Alloc(0xA8);        // scratch block (ebp)
		tab += 4;
		U16(w, 0x9E) = U16(blk, 0xA4);                  // morph weight of the ball
		U16(w, 0x9C) = (uint16_t)(0x1000 - U16(blk, 0xA4));
		U32(w, 0x5C) = U32(blk, 0x74);                  // pivot x, y
		U32(w, 0x60) = U32(blk, 0x78);                  // pivot z (+ pad)
		U16(w, 0xA0) = U16(blk, 0xA6);                  // radii
		U16(w, 0xA4) = U16(blk, 0xAA);
		U32(w, 0x98) = U32(ent, 0x7C);                  // visible objects
		const uint32_t rgb = U32(ent, 0x28) & 0xFFFFFF;
		memcpy((void *)w, (const void *)blk, 0x20);     // rep movsd: the ball rotation
		U16(w, 0xA2) = U16(blk, 0xA8);
		U32(w, 0x90) = rgb | 0x24000000;                // POLY_FT3 code + colour
		U32(w, 0x94) = rgb | 0x2C000000;                // POLY_FT4 code + colour
		if (U16(blk, 0x9C) != 0)
			x::sub_8DD7E0(w, (uint32_t)(int32_t)S16(blk, 0x9C));
		if (U16(blk, 0xA0) != 0)
			x::sub_8DD960(w, (uint32_t)(int32_t)S16(blk, 0xA0));
		if (U16(blk, 0x9E) != 0)
			x::MAG_022_sub_8DD8A0(w, (uint32_t)(int32_t)S16(blk, 0x9E));
		gx::sub_45DEA0(w);
		x::GTE_SetRotMatrix(blk + 0x40);
		x::GTE_SetTransVector(blk + 0x40);
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t obj = U32(a1, 4) + U32(tab, 0);
			tab += 4;
			if (((U32(w, 0x98) >> (i & 31)) & 1) == 0)
				continue;
			// pass 1: world vertex, relative to the pivot, morphed to the ball, turned, back
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
					wv += 0x14;
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
					vp += 0x14;
					zp += 0x14;
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
				const uint32_t s2 = U32(verts + i2 * 20, 8);
				const uint32_t s1 = U32(verts + i1 * 20, 8);
				const uint32_t s0 = U32(verts + i0 * 20, 8);
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
				const int32_t z2 = S32(verts + U32(w, 0x88) * 20, 0xC);
				const int32_t z1 = S32(verts + U32(w, 0x84) * 20, 0xC);
				const int32_t z0 = S32(verts + U32(w, 0x80) * 20, 0xC);
				const int32_t sum = add32(add32(z1, z0), z2);
				// sum / 3 (magic 0x55555556: high dword + its sign bit)
				int32_t hq = (int32_t)(((int64_t)sum * 0x55555556LL) >> 32);
				hq = add32(hq, (int32_t)((uint32_t)hq >> 31));
				const int32_t key = hq >> (a3 & 31);
				U32(w, 0x78) = (uint32_t)key;
				x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, 0);
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
					const uint32_t s2 = U32(verts + i2 * 20, 8);
					const uint32_t s1 = U32(verts + i1 * 20, 8);
					const uint32_t s0 = U32(verts + i0 * 20, 8);
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
					U32(pk, 0) = 0x9000000;
					const uint32_t v3 = verts + i3 * 20;
					const uint32_t s3 = U32(v3, 8);
					U32(w, 0x30) = s3;
					U32(pk, 0x20) = s3;
					U32(pk, 0x18) = U32(w, 0x2C);
					U32(pk, 0xC) = U32(f, 4);
					U32(pk, 0x14) = U32(f, 8);
					U16(pk, 0x1C) = U16(f, 0xC);
					U16(pk, 0x24) = U16(f, 0xE);
					U32(pk, 4) = U32(w, 0x94);
					if (U8(f, 0xB) & 2)
						U8(pk, 7) |= 2;
					const int32_t z3 = S32(v3, 0xC);
					const int32_t z2 = S32(verts + U32(w, 0x88) * 20, 0xC);
					const int32_t z1 = S32(verts + U32(w, 0x84) * 20, 0xC);
					const int32_t z0 = S32(verts + U32(w, 0x80) * 20, 0xC);
					const int32_t sum = add32(add32(add32(z0, z3), z1), z2);
					const int32_t key = sum >> ((a3 + 2) & 31);
					U32(w, 0x78) = (uint32_t)key;
					x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, (uint32_t)z3);
					pk += 0x28;
				}
			}
			cursor = pk;
		}
		U32(U32(blk, 0x6C), 0) = cursor;
		x::Field_Free(0xA8);
		return 0; // void
	}

	// 0x8A9D20 (copy of db_84C2D0 0x84C2D0: sub_84C2D0): clears the ball block a1 (0xC4 bytes) and sets it up: +0x60 = a2,
	// +0x64 = geometry descriptor a3 (+4 = vertex scratch a4, +0x14..+0x24 defaults), +0x68 =
	// entity a5, +0x6C / +0x70 = the frame arena cursor pointer 0x1D8E054, +0xAC = slot a6
	uint32_t __cdecl rt_8A9D20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		x::MAG_007_sub_8DCC00(a1, 0xC4);
		U32(a1, 0x68) = a5;
		U32(a1, 0x60) = a2;
		U32(a1, 0x6C) = 0x1D8E054;
		U32(a1, 0x70) = 0x1D8E054;
		U16(a1, 0xAC) = (uint16_t)a6;
		U32(a3, 4) = a4;
		U32(a1, 0x64) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFF;
		U16(a3, 0x24) = 0;
		return a1;
	}

	// 0x8A9F10 (copy of mb_82A540 0x82A540: copy of c_85FA30 0x85FA30: MAG_036_sub_85FA30): CAMERA SHAKE task - state {0x85FA80 shake step, 0x85FAB0 ret}
	uint32_t __cdecl rt_8A9F10(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[2];
		states[0] = 0x8A9F60;
		states[1] = 0x8AAB90; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8AA2E0 (copy of s_7434C0 0x7434C0: module Siren, sub_7434C0): draws one sprite of a sparkle node (unless +0x26 bit 2):
	// 0xD8-byte scratch header, camera matrix (unpacked, rotated by itself) projects the node
	// position +0x1C/+0x1E/+0x20 (-> header +0xCC), sprite data ptr +0x4C, frame +0x50, depth
	// +0x56 -> a_7435E0(header, OT+0x44, 2, packet cursor), cursor updated
	uint32_t __cdecl rt_8AA2E0(uint32_t a1)
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

	// 0x8AA9B0 (copy of s_747320 0x747320: module 095): bubble state 0: sprite +0x4C = 0x15334A0, +0x56 = -16, +0x52 = 9, advance.
	uint32_t __cdecl rt_8AA9B0(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x1629FB0;
		st++;
		U16(node, 0x56) = 0xfff0;
		U16(node, 0x52) = 9;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x8AA9D0 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl rt_8AA9D0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8AAB40 (copy of s_747320 0x747320: module 095): bubble state 0: sprite +0x4C = 0x15334A0, +0x56 = -16, +0x52 = 11, advance.
	uint32_t __cdecl rt_8AAB40(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x162A0C0;
		st++;
		U16(node, 0x56) = 0xfff0;
		U16(node, 0x52) = 0xB;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x8AAB60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl rt_8AAB60(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8AAFE0 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl rt_8AAFE0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1629FA8);
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

	// ------------------------------------------------------------------------------------
	// the module's own code (swing, throw trigger, ball start, Raldo and its debris)
	// ------------------------------------------------------------------------------------

	// 0x8A36D0 (MAG_019_sub_8A36D0): clears the director block [0x1629FA8] (0x54 bytes), keeps the
	// positions of the caster (node +0x2C: +0x00 / +0x04) and of the target (node +0x2D: +0x08 /
	// +0x0C) and the caster's facing towards the target (CartesianToGameAngle of the x / z delta,
	// - 0x800, 12 bits) in +0x4A
	uint32_t __cdecl rt_8A36D0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint32_t tgt = ENT(U8(a1, 0x2D));
		U32(d, 8) = U32(tgt, 0x1C);
		U32(d, 0xC) = U32(tgt, 0x20);
		const uint32_t cst = ENT(U8(a1, 0x2C));
		U32(d, 0) = U32(cst, 0x1C);
		U32(d, 4) = U32(cst, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(DIR_PTR), 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x8A3760 (MAG_019_sub_8A3760): TASK director - phase bookkeeping (0x8A37E0) every tick, then
	// its 6-state table {0x8A3830 cue 1, 0x8A3880 cue 1, 0x8A38C0 4 ticks, 0x8A38E0 tick 0x2D,
	// 0x8A3900 finish, ret}
	uint32_t __cdecl rt_8A3760(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x8A3830;
		states[1] = 0x8A3880;
		states[2] = 0x8A38C0;
		states[3] = 0x8A38E0;
		states[4] = 0x8A3900;
		states[5] = 0x8A3920; // nullsub (ret)
		rt_8A37E0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A3940 (sub_8A3940): the director's phase-1 start (from 0x8A37E0): spawns the ball actor
	// system (0x8A39E0 = engine 0x73C100: queue 0x272E090, task 0x8A3A30, actor data 0x17F37BC, start
	// tick 0x1C, mode 4, flag 0) and the Raldo task 0x8A93E0 (0xB8 bytes, queue 0x272B9D8), whose
	// entity +0x2C becomes the last battle entity 0..6 with flag 2 and kind byte +4 == 0x70 standing
	// exactly at the caster's position (the node's +0x2C, copied from the director)
	uint32_t __cdecl rt_8A3940(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x40) != 1)
			return 0; // void
		const uint32_t node = a1;
		a_73C100(node, ORIG_Ball, BALL_DATA, 0x1C, 0, 4);
		const uint32_t cst = ENT(U8(node, 0x2C));
		const uint32_t raldo = x::Effect_AddTaskAndInitFromCtx(Q_RALDO, ORIG_Raldo, 0xB8, node);
		uint8_t slot = 0;
		for (uint32_t e = ENT(0); e < ENT(7); e += 0x9C, slot++)
		{
			if ((U8(e, 0) & 2) == 0 || U8(e, 4) != 0x70)
				continue;
			if (U16(e, 0x1C) != U16(cst, 0x1C) || U16(e, 0x1E) != U16(cst, 0x1E) || U16(e, 0x20) != U16(cst, 0x20))
				continue;
			U8(raldo, 0x2C) = slot;
		}
		return 0; // void
	}

	// 0x8A3A90 (sub_8A3A90): ball actor system state 0 - at its start tick (+0x298) its origin
	// +0x290..+0x294 = the entity +0x2C's x / z with y = -0x300, then the actor-system start
	// (0x8A3B00 = engine 0x73F990) and its first step (0x8A45E0), next state
	uint32_t __cdecl rt_8A3A90(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < S16(node, 0x298))
			return 0; // void
		const uint32_t e = ENT(U8(node, 0x2C));
		const uint32_t xy = (U32(e, 0x1C) & 0xFFFFu) | 0xFD000000u;  // [esp+8] dword, high word 0xFD00
		const uint32_t z = U32(e, 0x20);
		U32(node, 0x290) = xy;
		U32(node, 0x294) = z;
		a_73F990(node);
		rt_8A45E0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
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
	uint32_t __cdecl rt_8A5D40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8A93E0 (sub_8A93E0): TASK Raldo - 10 states {0x8A9CA0 set-up, 0x8A9DA0 sound at tick 0x1C,
	// 0x8A9DD0 take the Raldo, 0x8AAC50 throw, 0x8AAD90 rebound, 0x8AAEC0 unmorph, 0x8AAF00 give it
	// back, 0x8AAF60 turn it back after 8 ticks, 0x8AAFB0 finish, ret}; while +0x26 bit 3 is set the
	// Raldo entity (+0x2C) is placed at the ball (block 0x2726910: +0x7C position - +0x74 pivot,
	// into the entity position +0x1C and its matrix translation +0x54) and drawn by 0x8A94E0
	uint32_t __cdecl rt_8A93E0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = 0x8A9CA0;
		states[1] = 0x8A9DA0;
		states[2] = 0x8A9DD0;
		states[3] = 0x8AAC50;
		states[4] = 0x8AAD90;
		states[5] = 0x8AAEC0;
		states[6] = 0x8AAF00;
		states[7] = 0x8AAF60;
		states[8] = 0x8AAFB0;
		states[9] = 0x8AAFD0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 8) != 0)
		{
			const uint32_t ent = ENT(U8(node, 0x2C));
			const int16_t px = (int16_t)(MEM<uint16_t>(BLK + 0x7C) - MEM<uint16_t>(BLK + 0x74));
			S16(ent, 0x1C) = px;
			S32(ent, 0x54) = px;
			const int16_t py = (int16_t)(MEM<uint16_t>(BLK + 0x7E) - MEM<uint16_t>(BLK + 0x76));
			S16(ent, 0x1E) = py;
			S32(ent, 0x58) = py;
			const int16_t pz = (int16_t)(MEM<uint16_t>(BLK + 0x80) - MEM<uint16_t>(BLK + 0x78));
			S16(ent, 0x20) = pz;
			S32(ent, 0x5C) = pz;
			// 30 fps layer: see mag019_raldo_throw_held.inc
			FX_HELD(held_note_ball(node);)
			rt_8A94E0(BLK);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8A9CA0 (sub_8A9CA0): Raldo state 0 - sets up the ball block for the Raldo entity (0x8A9D20:
	// block 0x2726910, 0x2726AC0, geometry descriptor 0x27303E8, vertex scratch 0x272E0A0), keeps its
	// position (0x27269C0 / 0x27269C4) and rotation words +0x0C / +0x10 (0x27269C8 / 0x27269CC), turns
	// it to the director's facing (+0x4A -> +0x0E), next state
	uint32_t __cdecl rt_8A9CA0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint8_t slot = U8(node, 0x2C);
		const uint32_t e = ENT(slot);
		rt_8A9D20(BLK, 0x2726AC0, 0x27303E8, 0x272E0A0, e, slot);
		const uint32_t z = U32(e, 0x20);
		const uint32_t r0 = U32(e, 0xC);
		const uint32_t xy = U32(e, 0x1C);
		MEM<uint32_t>(0x27269C4) = z;
		MEM<uint32_t>(0x27269C8) = r0;
		MEM<uint32_t>(0x27269C0) = xy;
		const uint32_t r1 = U32(e, 0x10);
		U16(e, 0xE) = U16(MEM<uint32_t>(DIR_PTR), 0x4A);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		MEM<uint32_t>(0x27269CC) = r1;
		return 0; // void
	}

	// 0x8A9DA0 (sub_8A9DA0): Raldo state 1 - at tick 0x1C the sound 0x1629FAC, next state
	uint32_t __cdecl rt_8A9DA0(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x1C)
			return 0; // void
		x::BdPlaySE(SOUND_Throw, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A9DD0 (sub_8A9DD0): Raldo state 2 - the custom draw on (+0x26 bit 3), the entity's own draw
	// off (flag 4), pivot (0x8AABA0), radii 0x1C0, ball rotation reset (0x8DD770), ball position =
	// the entity + pivot y, start (0x272699C / 0x27269A0) = there, destination (0x2726994) = the
	// target's spawn point (bone 0xF1), vertical speed -0x180, bounce 0, one debris spawner of 2 ticks
	// (0x8A9EC0), the stage wobble task 0x8DDC30 (queue 0x2730260, data 0x162A470, 9), next state
	uint32_t __cdecl rt_8A9DD0(uint32_t a1)
	{
		const uint32_t node = a1;
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 8);
		const uint32_t e = ENT(U8(node, 0x2C));
		U8(e, 0) = (uint8_t)(U8(e, 0) | 4);
		rt_8AABA0(node, BLK);
		MEM<uint16_t>(0x27269B8) = 0x1C0;
		MEM<uint16_t>(0x27269BA) = 0x1C0;
		MEM<uint16_t>(0x27269B6) = 0x1C0;
		x::MAG_022_sub_8DD770(BLK);
		const uint32_t xy = U32(e, 0x1C);
		const uint16_t py = MEM<uint16_t>(0x2726986);
		MEM<uint32_t>(0x272698C) = xy;
		MEM<uint16_t>(0x272698E) = (uint16_t)(MEM<uint16_t>(0x272698E) + py);
		const uint32_t z = U32(e, 0x20);
		const uint32_t tgt = ENT(U8(node, 0x2D));
		const uint32_t pos = MEM<uint32_t>(0x272698C);
		MEM<uint32_t>(0x2726990) = z;
		MEM<uint32_t>(0x272699C) = pos;
		MEM<uint32_t>(0x27269A0) = z;
		x::GetEffectSpawnPosition(tgt, 0xF1, 0, 0x2726994);
		MEM<uint16_t>(0x27269A6) = 0xFE80;
		MEM<uint16_t>(0x27269D2) = 0;
		rt_8A9EC0(node, 2);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x8DDC30, 0x40, node);
		U32(t, 0x34) = 0x162A470;
		U16(t, 0x38) = 9;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8A9EC0 (sub_8A9EC0): spawns a debris spawner (task 0x8A9F10, queue 0x272B9D8) at the ball
	// position 0x272698C / 0x2726990 (y 0) running a2 ticks (+0xB6)
	uint32_t __cdecl rt_8A9EC0(uint32_t a1, uint32_t a2)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_RALDO, 0x8A9F10, 0xB8, a1);
		const uint16_t n = (uint16_t)a2;
		const uint32_t xy = MEM<uint32_t>(0x272698C);
		U16(t, 0xB6) = n;
		const uint32_t z = MEM<uint32_t>(0x2726990);
		U32(t, 0x1C) = xy;
		U32(t, 0x20) = z;
		U16(t, 0x1E) = 0;
		return t;
	}

	// one debris burst of 0x8A9F60: 4 particles (8-byte positions +0x6C, velocities +0x8C, gravity
	// words +0xAC) at the spawner's position, pushed out by a random angle (rand % 0xFFF) over
	// radius r0 + (rand & 0xFF), speed v0 + (rand & 0xFFF)
	static inline void rt_debris_burst(uint32_t t, uint32_t src, int32_t r0, int32_t v0, bool up)
	{
		uint32_t g = t + 0xAC;
		uint32_t p = t + 0x6C;
		for (int k = 4; k != 0; k--)
		{
			const int32_t r1 = (int32_t)x::CrtRand();
			const int32_t ang = (int32_t)(int16_t)(r1 % 0xFFF);
			const int32_t r2 = (int32_t)x::CrtRand();
			const int32_t rad = (int32_t)(int16_t)((r2 & 0xFF) + r0);
			const int32_t r3 = (int32_t)x::CrtRand();
			const int32_t spd = (int32_t)(int16_t)((r3 & 0xFFF) + v0);
			U32(p, 0) = U32(src, 0x1C);
			U32(p, 4) = U32(src, 0x20);
			U16(p, 0) = (uint16_t)(U16(p, 0) + (uint16_t)(mul32((int32_t)x::computeSin((uint32_t)ang), rad) / 4096));
			U16(p, 4) = (uint16_t)(U16(p, 4) + (uint16_t)(mul32((int32_t)x::computeCosine((uint32_t)ang), rad) / 4096));
			U16(p, 0x20) = (uint16_t)(mul32((int32_t)x::computeSin((uint32_t)ang), spd) / 4096);
			U16(p, 0x24) = (uint16_t)(mul32((int32_t)x::computeCosine((uint32_t)ang), spd) / 4096);
			if (up)
			{
				const int32_t r4 = (int32_t)x::CrtRand();
				U16(p, 0x22) = (uint16_t)(-0x800 - (r4 & 0xFFF));
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(g, 0) = (uint16_t)((r5 & 0x7F) + 0xF0);
			}
			else
			{
				const int32_t r4 = (int32_t)x::CrtRand();
				U16(g, 0) = (uint16_t)(-0x30 - (r4 & 0x1F));
			}
			g += 2;
			p += 8;
		}
	}

	// 0x8A9F60 (sub_8A9F60): debris spawner state 0 - every tick one low burst (task 0x8AAA30:
	// radius 0x40.., speed 0x100.., rising gravity) and one high burst (task 0x8AA1D0: radius 0x80..,
	// speed 0x800.., thrown up -0x800.., gravity 0xF0..); after +0xB6 ticks the spawner finishes
	uint32_t __cdecl rt_8A9F60(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t t1 = x::Effect_AddTaskAndInitFromCtx(Q_RALDO, 0x8AAA30, 0xB8, node);
		rt_debris_burst(t1, node, 0x40, 0x100, false);
		const uint32_t t2 = x::Effect_AddTaskAndInitFromCtx(Q_RALDO, 0x8AA1D0, 0xB8, node);
		rt_debris_burst(t2, node, 0x80, 0x800, true);
		if ((int32_t)S16(node, 0x24) >= (int32_t)S16(node, 0xB6) - 1)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// the debris motion of 0x8AA1D0 / 0x8AAA30 for particle k (after its draw): gravity +0xAC[k]
	// added to the y velocity, the velocity loses a quarter (C / 4), the position moves by velocity / 16
	static inline void rt_debris_move(uint32_t node, int k)
	{
		const uint32_t v = node + 0x8C + 8 * k;
		const uint32_t p = node + 0x6C + 8 * k;
		U16(v, 2) = (uint16_t)(U16(v, 2) + U16(node, 0xAC + 2 * k));
		for (int j = 0; j < 3; j++)
		{
			const int16_t c = S16(v, 2 * j);
			S16(v, 2 * j) = (int16_t)(c - c / 4);
		}
		for (int j = 0; j < 3; j++)
			U16(p, 2 * j) = (uint16_t)(U16(p, 2 * j) + (uint16_t)(S16(v, 2 * j) / 16));
	}

	// 0x8AA1D0 (sub_8AA1D0): TASK high debris - 3 states {0x8AA9B0 flipbook 0x1629FB0 of 9 frames,
	// 0x8AA9D0 frame step, ret}, then its 4 particles drawn (0x8AA2E0) and moved
	uint32_t __cdecl rt_8AA1D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x8AA9B0;
		states[1] = 0x8AA9D0;
		states[2] = 0x8AAA20; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag019_raldo_throw_held.inc
		FX_HELD(held_note_debris(node);)
		for (int k = 0; k < 4; k++)
		{
			U32(node, 0x1C) = U32(node, 0x6C + 8 * k);
			U32(node, 0x20) = U32(node, 0x70 + 8 * k);
			rt_8AA2E0(node);
			rt_debris_move(node, k);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8AAA30 (sub_8AAA30): TASK low debris - as 0x8AA1D0 with {0x8AAB40 flipbook 0x162A0C0 of 11
	// frames, 0x8AAB60 frame step, ret}
	uint32_t __cdecl rt_8AAA30(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x8AAB40;
		states[1] = 0x8AAB60;
		states[2] = 0x8AAB80; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag019_raldo_throw_held.inc
		FX_HELD(held_note_debris(node);)
		for (int k = 0; k < 4; k++)
		{
			U32(node, 0x1C) = U32(node, 0x6C + 8 * k);
			U32(node, 0x20) = U32(node, 0x70 + 8 * k);
			rt_8AA2E0(node);
			rt_debris_move(node, k);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8AABA0 (sub_8AABA0): the ball pivot a2+0x74 = the spawn point (bone 0xF1) of the entity +0x2C
	// of a1, taken with the entity at the origin and its matrix reset (0x8DD770), relative to the
	// entity; position and matrix put back
	uint32_t __cdecl rt_8AABA0(uint32_t a1, uint32_t a2)
	{
		const uint32_t e = ENT(U8(a1, 0x2C));
		uint32_t mat[8];
		const uint32_t xy = U32(e, 0x1C);
		memcpy(mat, (const void *)(e + 0x40), 0x20);    // rep movsd
		const uint32_t z = U32(e, 0x20);
		U16(e, 0x1C) = 0;
		U16(e, 0x1E) = 0;
		U16(e, 0x20) = 0;
		x::MAG_022_sub_8DD770(e + 0x40);
		const uint32_t blk = a2;
		x::GetEffectSpawnPosition(e, 0xF1, 0, blk + 0x74);
		const uint16_t ex = U16(e, 0x1C), ey = U16(e, 0x1E), ez = U16(e, 0x20);
		U16(blk, 0x74) = (uint16_t)(U16(blk, 0x74) - ex);
		U16(blk, 0x76) = (uint16_t)(U16(blk, 0x76) - ey);
		U16(blk, 0x78) = (uint16_t)(U16(blk, 0x78) - ez);
		memcpy((void *)(e + 0x40), mat, 0x20);          // rep movsd
		U32(e, 0x1C) = xy;
		U32(e, 0x20) = z;
		return 0; // void
	}

	// the ball position between the start 0x272699C / 0x27269A0 and the destination 0x2726994 /
	// 0x2726998 at progress t (0..0x1000) of 0x8AAC50 / 0x8AAD90: x -> 0x272698C, z -> 0x2726990
	static inline void rt_ball_xz(int32_t t)
	{
		const int32_t dx = mul32((int32_t)MEM<int16_t>(0x2726994) - (int32_t)MEM<int16_t>(0x272699C), t) / 4096;
		MEM<uint16_t>(0x272698C) = (uint16_t)add32(dx, (int32_t)MEM<uint32_t>(0x272699C));
		const int32_t dz = mul32((int32_t)MEM<int16_t>(0x2726998) - (int32_t)MEM<int16_t>(0x27269A0), t) / 4096;
		MEM<uint16_t>(0x2726990) = (uint16_t)add32(dz, (int32_t)MEM<uint32_t>(0x27269A0));
	}

	// 0x8AAC50 (sub_8AAC50): Raldo state 3 - the throw: pivot, morph +0x200 a tick (to 0x1000), spin
	// x +0x200 (angle z masked), progress 0x27269D0 +0x200 a tick; at 0x1000 the director's hit flag
	// (+0x48 = 1), vertical speed -0x260 and next state; x / z along the progress, y by the vertical
	// speed (+0x80 a tick); on reaching the ground (pivot y) it stays there, the bounce count goes to
	// 1 and a debris spawner of 1 tick starts
	uint32_t __cdecl rt_8AAC50(uint32_t a1)
	{
		const uint32_t node = a1;
		rt_8AABA0(node, BLK);
		uint16_t m = (uint16_t)(MEM<uint16_t>(0x27269B4) + 0x200);
		MEM<uint16_t>(0x27269B4) = m;
		if ((int16_t)m >= 0x1000)
			MEM<uint16_t>(0x27269B4) = 0x1000;
		uint16_t t = MEM<uint16_t>(0x27269D0);
		MEM<uint16_t>(0x27269AC) = (uint16_t)(MEM<uint16_t>(0x27269AC) + 0x200);
		MEM<uint16_t>(0x27269B0) = (uint16_t)(MEM<uint16_t>(0x27269B0) & 0xFFF);
		t = (uint16_t)(t + 0x200);
		MEM<uint16_t>(0x27269D0) = t;
		uint16_t vy;
		if ((int16_t)t >= 0x1000)
		{
			t = 0x1000;
			MEM<uint16_t>(0x27269D0) = t;
			vy = 0xFDA0;
			U16(MEM<uint32_t>(DIR_PTR), 0x48) = 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		else
			vy = MEM<uint16_t>(0x27269A6);
		vy = (uint16_t)(vy + 0x80);
		MEM<uint16_t>(0x27269A6) = vy;
		rt_ball_xz((int32_t)(int16_t)t);
		const uint16_t ground = MEM<uint16_t>(0x2726986);
		const uint16_t y = (uint16_t)(MEM<uint16_t>(0x272698E) + vy);
		MEM<uint16_t>(0x272698E) = y;
		if ((int16_t)y >= (int16_t)ground)
		{
			MEM<uint16_t>(0x272698E) = ground;
			if (MEM<uint16_t>(0x27269D2) == 0)
				MEM<uint16_t>(0x27269D2) = 1;
			rt_8A9EC0(node, 1);
		}
		return 0; // void
	}

	// 0x8AAD90 (sub_8AAD90): Raldo state 4 - the rebound back to the caster: pivot, spin x -0x200,
	// progress -0x100 a tick (to 0: next state), x / z along the progress, y by the vertical speed
	// (+0x80 a tick); on the ground: bounce 1 -> 2 (speed -0x180, debris of 4 ticks), 2 -> 3, else
	// debris of 1 tick
	uint32_t __cdecl rt_8AAD90(uint32_t a1)
	{
		const uint32_t node = a1;
		rt_8AABA0(node, BLK);
		uint16_t t = MEM<uint16_t>(0x27269D0);
		MEM<uint16_t>(0x27269AC) = (uint16_t)(MEM<uint16_t>(0x27269AC) + 0xFE00);
		MEM<uint16_t>(0x27269B0) = (uint16_t)(MEM<uint16_t>(0x27269B0) & 0xFFF);
		t = (uint16_t)(t + 0xFF00);
		MEM<uint16_t>(0x27269D0) = t;
		if (!((int16_t)t > 0))
		{
			t = 0;
			MEM<uint16_t>(0x27269D0) = t;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		rt_ball_xz((int32_t)(int16_t)t);
		const uint16_t vy = (uint16_t)(MEM<uint16_t>(0x27269A6) + 0x80);
		MEM<uint16_t>(0x27269A6) = vy;
		const uint16_t y = (uint16_t)(MEM<uint16_t>(0x272698E) + vy);
		const uint16_t ground = MEM<uint16_t>(0x2726986);
		MEM<uint16_t>(0x272698E) = y;
		if ((int16_t)y >= (int16_t)ground)
		{
			MEM<uint16_t>(0x272698E) = ground;
			const uint16_t b = MEM<uint16_t>(0x27269D2);
			if (b == 1)
			{
				MEM<uint16_t>(0x27269D2) = 2;
				MEM<uint16_t>(0x27269A6) = 0xFE80;
				rt_8A9EC0(node, 4);
				return 0; // void
			}
			if (b == 2)
				MEM<uint16_t>(0x27269D2) = 3;
			rt_8A9EC0(node, 1);
		}
		return 0; // void
	}

	// 0x8AAEC0 (sub_8AAEC0): Raldo state 5 - pivot, morph -0x300 a tick; at 0 next state
	uint32_t __cdecl rt_8AAEC0(uint32_t a1)
	{
		rt_8AABA0(a1, BLK);
		const uint16_t m = (uint16_t)(MEM<uint16_t>(0x27269B4) + 0xFD00);
		MEM<uint16_t>(0x27269B4) = m;
		if (!((int16_t)m > 0))
		{
			MEM<uint16_t>(0x27269B4) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8AAF00 (sub_8AAF00): Raldo state 6 - custom draw off, 8-tick wait (+0xB4), the entity's own
	// draw back (flag 4 off), its position put back (y from +0x24 first, overwritten by the kept
	// position dword), next state
	uint32_t __cdecl rt_8AAF00(uint32_t a1)
	{
		const uint32_t node = a1;
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) & 0xF7);
		U16(node, 0xB4) = 8;
		const uint32_t e = ENT(U8(node, 0x2C));
		U16(e, 0) = (uint16_t)(U16(e, 0) & 0xFFFB);
		U16(e, 0x1E) = U16(e, 0x24);
		U32(e, 0x1C) = MEM<uint32_t>(0x27269C0);
		U32(e, 0x20) = MEM<uint32_t>(0x27269C4);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8AAF60 (sub_8AAF60): Raldo state 7 - after 8 ticks its rotation words +0x0C / +0x10 put back,
	// next state
	uint32_t __cdecl rt_8AAF60(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0xB4) = (uint16_t)(U16(node, 0xB4) - 1);
		if (S16(node, 0xB4) > 0)
			return 0; // void
		const uint32_t e = ENT(U8(node, 0x2C));
		U32(e, 0xC) = MEM<uint32_t>(0x27269C8);
		U32(e, 0x10) = MEM<uint32_t>(0x27269CC);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8A3550, (void *)a_73A0D0, "019 MAG_019_sub_8A3550" },
		{ 0x8A3560, (void *)a_73A0D0, "019 MAG_019_sub_8A3560" },
		{ 0x8A35F0, (void *)a_73A170, "019 MAG_019_sub_8A35F0" },
		{ 0x8A3620, (void *)a_73A1A0, "019 MAG_019_sub_8A3620" },
		{ 0x8A3830, (void *)a_73B790, "019 sub_8A3830" },
		{ 0x8A3850, (void *)a_73B640, "019 sub_8A3850" },
		{ 0x8A38A0, (void *)a_73B7E0, "019 sub_8A38A0" },
		{ 0x8A3900, (void *)a_7475A0, "019 sub_8A3900" },
		{ 0x8A3920, (void *)a_73A6D0, "019 nullsub_1777" },
		{ 0x8A3930, (void *)a_73A6D0, "019 nullsub_1778" },
		{ 0x8A39E0, (void *)a_73C100, "019 sub_8A39E0" },
		{ 0x8A3A30, (void *)a_73A380, "019 sub_8A3A30" },
		{ 0x8A3B00, (void *)a_73F990, "019 sub_8A3B00" },
		{ 0x8A3D90, (void *)a_73FC20, "019 sub_8A3D90" },
		{ 0x8A4000, (void *)a_73FE90, "019 sub_8A4000" },
		{ 0x8A4120, (void *)a_73FFB0, "019 sub_8A4120" },
		{ 0x8A4380, (void *)a_740210, "019 sub_8A4380" },
		{ 0x8A5810, (void *)a_73D770, "019 sub_8A5810" },
		{ 0x8A5A60, (void *)a_73D9C0, "019 sub_8A5A60" },
		{ 0x8A6890, (void *)a_740700, "019 sub_8A6890" },
		{ 0x8A6950, (void *)a_7407C0, "019 sub_8A6950" },
		{ 0x8A6AA0, (void *)a_740910, "019 sub_8A6AA0" },
		{ 0x8A7100, (void *)a_740F70, "019 sub_8A7100" },
		{ 0x8A7150, (void *)a_740FC0, "019 sub_8A7150" },
		{ 0x8A71E0, (void *)a_741050, "019 sub_8A71E0" },
		{ 0x8A72B0, (void *)a_741120, "019 sub_8A72B0" },
		{ 0x8A7CF0, (void *)a_741B60, "019 sub_8A7CF0" },
		{ 0x8A8420, (void *)a_742290, "019 au_re__rand_65" },
		{ 0x8A8450, (void *)a_7422C0, "019 sub_8A8450" },
		{ 0x8A8490, (void *)a_742300, "019 sub_8A8490" },
		{ 0x8A84E0, (void *)a_742350, "019 sub_8A84E0" },
		{ 0x8A8530, (void *)a_7423A0, "019 au_re__rand_70_0" },
		{ 0x8A8640, (void *)a_7424B0, "019 sub_8A8640" },
		{ 0x8A8650, (void *)a_7424C0, "019 sub_8A8650" },
		{ 0x8A8E30, (void *)a_742CA0, "019 sub_8A8E30" },
		{ 0x8A8E60, (void *)a_742CD0, "019 sub_8A8E60" },
		{ 0x8A8E90, (void *)a_742D00, "019 sub_8A8E90" },
		{ 0x8A9040, (void *)a_742EB0, "019 sub_8A9040" },
		{ 0x8A9180, (void *)a_742FF0, "019 sub_8A9180" },
		{ 0x8A93D0, (void *)a_73A6D0, "019 nullsub_1779" },
		{ 0x8AA400, (void *)a_7435E0, "019 InitEffectSequenceFromData_c16" },
		{ 0x8AA540, (void *)a_743720, "019 sub_8AA540" },
		{ 0x8AA9F0, (void *)a_743C20, "019 sub_8AA9F0" },
		{ 0x8AAA20, (void *)a_73A6D0, "019 nullsub_1780" },
		{ 0x8AAB80, (void *)a_73A6D0, "019 nullsub_1781" },
		{ 0x8AAB90, (void *)a_73A6D0, "019 nullsub_1782" },
		{ 0x8AAFB0, (void *)a_7475A0, "019 sub_8AAFB0" },
		{ 0x8AAFD0, (void *)a_73A6D0, "019 nullsub_1783" },
		{ 0x8AB020, (void *)a_7474B0, "019 MAG_019_sub_8AB020" },
		{ 0x8AB040, (void *)a_7474D0, "019 MAG_019_sub_8AB040" },
		{ 0x8AB060, (void *)a_73A6D0, "019 nullsub_1784" },
		{ 0x8AB070, (void *)a_747500, "019 MAG_019_sub_8AB070" },
		{ 0x8AB0B0, (void *)a_73A0D0, "019 MAG_019_sub_8AB0B0" },
		{ 0x8AB0C0, (void *)a_747550, "019 MAG_019_sub_8AB0C0" },
		{ 0x8AB0D0, (void *)a_73A0D0, "019 MAG_019_sub_8AB0D0" },
		{ 0x8AB0E0, (void *)a_73A0D0, "019 MAG_019_sub_8AB0E0" },
		{ 0x8AB0F0, (void *)a_7475A0, "019 MAG_019_sub_8AB0F0" },
		{ 0x8AB110, (void *)a_73A6D0, "019 nullsub_1776" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x8A33F0, (void *)rt_8A33F0, "019 rt_8A33F0" },
		{ 0x8A3570, (void *)rt_8A3570, "019 rt_8A3570" },
		{ 0x8A35B0, (void *)rt_8A35B0, "019 rt_8A35B0" },
		{ 0x8A36A0, (void *)rt_8A36A0, "019 rt_8A36A0" },
		{ 0x8A36D0, (void *)rt_8A36D0, "019 rt_8A36D0" },
		{ 0x8A3760, (void *)rt_8A3760, "019 rt_8A3760" },
		{ 0x8A37E0, (void *)rt_8A37E0, "019 rt_8A37E0" },
		{ 0x8A3880, (void *)rt_8A3880, "019 rt_8A3880" },
		{ 0x8A38C0, (void *)rt_8A38C0, "019 rt_8A38C0" },
		{ 0x8A38E0, (void *)rt_8A38E0, "019 rt_8A38E0" },
		{ 0x8A3940, (void *)rt_8A3940, "019 rt_8A3940" },
		{ 0x8A3A90, (void *)rt_8A3A90, "019 rt_8A3A90" },
		{ 0x8A3CB0, (void *)rt_8A3CB0, "019 rt_8A3CB0" },
		{ 0x8A3F30, (void *)rt_8A3F30, "019 rt_8A3F30" },
		{ 0x8A45E0, (void *)rt_8A45E0, "019 rt_8A45E0" },
		{ 0x8A4670, (void *)rt_8A4670, "019 rt_8A4670" },
		{ 0x8A46D0, (void *)rt_8A46D0, "019 rt_8A46D0" },
		{ 0x8A4830, (void *)rt_8A4830, "019 rt_8A4830" },
		{ 0x8A4960, (void *)rt_8A4960, "019 rt_8A4960" },
		{ 0x8A4B80, (void *)rt_8A4B80, "019 rt_8A4B80" },
		{ 0x8A4E10, (void *)rt_8A4E10, "019 rt_8A4E10" },
		{ 0x8A5290, (void *)rt_8A5290, "019 rt_8A5290" },
		{ 0x8A5D40, (void *)rt_8A5D40, "019 rt_8A5D40" },
		{ 0x8A6240, (void *)rt_8A6240, "019 rt_8A6240" },
		{ 0x8A69D0, (void *)rt_8A69D0, "019 rt_8A69D0" },
		{ 0x8A6A10, (void *)rt_8A6A10, "019 rt_8A6A10" },
		{ 0x8A7A40, (void *)rt_8A7A40, "019 rt_8A7A40" },
		{ 0x8A7B50, (void *)rt_8A7B50, "019 rt_8A7B50" },
		{ 0x8A7BD0, (void *)rt_8A7BD0, "019 rt_8A7BD0" },
		{ 0x8A7C00, (void *)rt_8A7C00, "019 rt_8A7C00" },
		{ 0x8A7CA0, (void *)rt_8A7CA0, "019 rt_8A7CA0" },
		{ 0x8A7D20, (void *)rt_8A7D20, "019 rt_8A7D20" },
		{ 0x8A7DA0, (void *)rt_8A7DA0, "019 rt_8A7DA0" },
		{ 0x8A8580, (void *)rt_8A8580, "019 rt_8A8580" },
		{ 0x8A87A0, (void *)rt_8A87A0, "019 rt_8A87A0" },
		{ 0x8A9290, (void *)rt_8A9290, "019 rt_8A9290" },
		{ 0x8A9320, (void *)rt_8A9320, "019 rt_8A9320" },
		{ 0x8A93B0, (void *)rt_8A93B0, "019 rt_8A93B0" },
		{ 0x8A93E0, (void *)rt_8A93E0, "019 rt_8A93E0" },
		{ 0x8A94E0, (void *)rt_8A94E0, "019 rt_8A94E0" },
		{ 0x8A95F0, (void *)rt_8A95F0, "019 rt_8A95F0" },
		{ 0x8A9CA0, (void *)rt_8A9CA0, "019 rt_8A9CA0" },
		{ 0x8A9D20, (void *)rt_8A9D20, "019 rt_8A9D20" },
		{ 0x8A9DA0, (void *)rt_8A9DA0, "019 rt_8A9DA0" },
		{ 0x8A9DD0, (void *)rt_8A9DD0, "019 rt_8A9DD0" },
		{ 0x8A9EC0, (void *)rt_8A9EC0, "019 rt_8A9EC0" },
		{ 0x8A9F10, (void *)rt_8A9F10, "019 rt_8A9F10" },
		{ 0x8A9F60, (void *)rt_8A9F60, "019 rt_8A9F60" },
		{ 0x8AA1D0, (void *)rt_8AA1D0, "019 rt_8AA1D0" },
		{ 0x8AA2E0, (void *)rt_8AA2E0, "019 rt_8AA2E0" },
		{ 0x8AA9B0, (void *)rt_8AA9B0, "019 rt_8AA9B0" },
		{ 0x8AA9D0, (void *)rt_8AA9D0, "019 rt_8AA9D0" },
		{ 0x8AAA30, (void *)rt_8AAA30, "019 rt_8AAA30" },
		{ 0x8AAB40, (void *)rt_8AAB40, "019 rt_8AAB40" },
		{ 0x8AAB60, (void *)rt_8AAB60, "019 rt_8AAB60" },
		{ 0x8AABA0, (void *)rt_8AABA0, "019 rt_8AABA0" },
		{ 0x8AAC50, (void *)rt_8AAC50, "019 rt_8AAC50" },
		{ 0x8AAD90, (void *)rt_8AAD90, "019 rt_8AAD90" },
		{ 0x8AAEC0, (void *)rt_8AAEC0, "019 rt_8AAEC0" },
		{ 0x8AAF00, (void *)rt_8AAF00, "019 rt_8AAF00" },
		{ 0x8AAF60, (void *)rt_8AAF60, "019 rt_8AAF60" },
		{ 0x8AAFE0, (void *)rt_8AAFE0, "019 rt_8AAFE0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag019_raldo_throw()
	{
		act::register_module(19);
		for (const act::raldo::ModPort *p = act::raldo::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(19, p->addr, p->port, p->name);
		for (const act::raldo::ModPort *p = act::raldo::PORTS; p->addr; p++)
			act::register_module_port(19, p->addr, p->port, p->name);
		// 30 fps layer: see mag019_raldo_throw_held.inc
		FX_HELD(register_mag019_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag019_raldo_throw_held.inc"
#endif
