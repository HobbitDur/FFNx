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

// Effect 60: Raijin's strike (enemy attack 294 of kernel.bin, no name in kernel.bin section 34; used by
// Raijin c0m137; MAG_060_*; no other kernel.bin entry uses effect 60): a copy of the actor/heal effect
// library (see act_engine.h) built like Psycho Blast (mag023_psycho_blast.cpp: master, emitter,
// director stepping phases, prim-model tasks, debris) with two actor systems, a strike that pushes
// the target back and the stage darkening.
//
// Setup MAG_060_UNK1 0x7EB090 (runs once, not ported; file loader MAG_060_UNK1_FL 0x7EB070 = mag059.tim ->
// [0x2604078], TIM uploaded and camera animation 0x159750C played unless the cast flags +1 bit 0; packet
// arenas at file + 0 / + 0x8000 by tick parity (cursor 0x260407C, second cursor 0x2604184), the actor
// pools carved from file + 0x10000) creates the root queue 0x2604258 with the master and six task
// pools: 0x2606A80 (4 x 0x58: emitters), 0x2604160 (3 x 0x48: director), 0x26067E0 (2 x 0x2A4: actor
// systems), 0x2604268 (0x32 x 0x8C: debris spawner, debris, stage darkening), 0x2606280 (3 x 0x18C:
// prim-model tasks), 0x2606A70 (10 x 0x40, unused).
//   Master (0x7EB200 = Heartbreak's 0x86CB50) - camera copy 0x2793E58 (the module scratch stack
//     0x2607048 grows down from it), packet arena by tick parity, bone follow, 11-state table: 0x7EB390
//     carves the actor pools, 0x7EB410 one emitter per action, then waits for the queues to empty.
//   Emitter (0x7EB440, engine task a_73A1A0), node +0x2C = caster, +0x2D = target: 0x7EB4C0 clears the
//     director block [0x1597358] (0x54 bytes), keeps the caster's / target's positions and the facing
//     from the caster to the target (+0x4A) and starts the director; 0x7F2690 applies the damage once
//     the director's cue +0x48 is set.
//   Director (0x7EB580): phase bookkeeping 0x7EB600 (a new phase spawns its tasks: 0x7EB790), states
//     0x7EB650 cue to phase 1, 0x7EB6A0 the sound 0x159735C, 0x7EB6F0 phase tick 4, 0x7EB710 the damage
//     cue +0x48 = 1 at phase tick 0x24, 0x7EB730 cue to phase 2, finish.
//   Phase 1 (0x7EB790): actor system 0x7EE0F0 (actor data 0x1797CC0, from its tick 6) and actor system
//     0x7F1A30 (actor data 0x179C998, from its tick 0x24), both at the caster's bone point (0x19,
//     0x400) (engine task a_73A380 on 0x7EE150 / 0x7F1A90: the director set-up a_73F990 and the
//     system step 0x7EEC90 = Psycho Blast's 0x896DE0); the strike task 0x7EB8C0 (prim layout
//     0x17A0D04), the debris spawner 0x7F1C70 and the stage darkening 0x7F1B20.
//   Strike (0x7EB8C0, 0x18C bytes; drawn by 0x7EBAC0 = the prim-model player 0x701970 at node +0x94 with
//     the callback 0x7EBBB0 (Curse's 0x82B870 at the module cursor) and the module renderer 0x7EBDF0):
//     tick 6 (0x7EB920) turns the target to face the caster and pushes it back by 0x200 (keeping its
//     position and its entity words +0x0C / +0x10); tick 0x24 (0x7EBA10) places the strike at the
//     caster's bone point with the caster's matrix and spawns the impact 0x7EDE50 (prim layout
//     0x17A3150 at the target's ground point 0xF1, turned 0x800); then (0x7EDF70) the target slides
//     back by the curve 0x1597820 (0 .. 0x1000 in 7 ticks), the strike follows it by 3/2, and when the
//     layout ends the target's words +0x0C / +0x10 are restored.
//   Debris (spawner 0x7F1C70 = engine a_73A380 on 0x7F1CD0 / 0x7F1D20): from tick 0x24 to 0x2C two
//     debris tasks 0x7F1E50 a tick at the target, thrown along the target's facing (rand); each one a
//     sprite (0x1597360, 16 frames) moved by velocity / 16 (velocity - velocity / 8, gravity on y).
//   Stage darkening (0x7F1B20 = engine a_73F110): the four stage light words 0x1D98992 + k * 0x2C from
//     tick 5 to 0x600 by 0x100 a tick, held to tick 0x36, back to 0 (0x7F1C20 = Doom's 0x8CC350).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2604078..0x260705C (file pointer 0x2604078, packet cursor 0x260407C, pools, queues,
// arena bases, actor state 0x2607058, scratch stack pointer 0x2607048, vertex blend buffer 0x2606C78,
// the target's saved words 0x2606C58 / 0x2606C5C), the director block [0x1597358], the module scratch
// stack below the camera copy 0x2793E58, the entities (the target's position / facing / words
// +0x0C / +0x10), the stage light words; the actor data 0x1797CC0..0x17A0CF4 in exe data is rewritten
// in place.
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS, among them 0x7EF900 = a_741120, a function missing from
// functions.json); the others are ported below (rj_XXXXXX), the library ones the text of the
// matching Heartbreak / Psycho Blast / Raldo Throw / Doom / Wind Blast / Aqua Breath / Curse port with this
// module's addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace raijin
{
	static const Mod MOD_060 = { "raijin", 60, 0x7EB070, 0x7F27F0,
		{ 0x0, 0x0, 0x1597358, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2604078, 0x0, 0x0, 0x0, 0x2604188, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26067E0, 0x0, 0x2606A80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2606C60, 0x0, 0x0, 0x2607040, 0x0, 0x2607048, 0x0, 0x2607058 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7EB440, 0x7EB4C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7EB670, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7EE360, 0x7EE440, 0x7EE5E0, 0x7EE6B0, 0x7EE7D0, 0x7EEA30, 0x0, 0x0, 0x7EF750, 0x7EF7A0, 0x0, 0x0, 0x7F0090, 0x0, 0x0, 0x0, 0x7F0A70, 0x7F0AA0, 0x7F0B80, 0x7F0CA0, 0x7F0DF0, 0x0, 0x7F14B0, 0x7F1690, 0x7F17D0, 0x7F2190, 0x0, 0x7F2690, 0x7F26D0, 0x7F26F0, 0x7F2710, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x260407C;   // module packet cursor (tick parity: file + 0x8000 / + 0x10000)
	static const uint32_t DIR_PTR = 0x1597358;         // director block pointer (0x54 bytes)
	static const uint32_t SCRATCH_SP = 0x2607048;      // module scratch stack pointer
	static const uint32_t ACTOR_STATE = 0x2607058;     // current actor system state block
	static const uint32_t BLEND_BUF = 0x2606C78;       // vertex blend buffer of the prim callback
	static const uint32_t PRIM_CB = 0x7EBBB0;          // the prim-model player's object callback
	static const uint32_t Q_EMITTER = 0x2606A80, Q_DIRECTOR = 0x2604160, Q_ACTORS = 0x26067E0;
	static const uint32_t Q_TASKS = 0x2604268, Q_MODELS = 0x2606280, Q_STAGE = 0x2606A70;
	static const uint32_t ORIG_Master = 0x7EB200;
	static const uint32_t ORIG_ActorsA = 0x7EE0F0;     // actor system (engine task a_73A380)
	static const uint32_t ORIG_ActorsB = 0x7F1A30;     // actor system (engine task a_73A380)
	static const uint32_t ORIG_Strike = 0x7EB8C0;      // strike model task
	static const uint32_t ORIG_Impact = 0x7EDE50;      // impact model task (engine task a_73A380)
	static const uint32_t ORIG_DebrisSpawner = 0x7F1C70; // (engine task a_73A380)
	static const uint32_t ORIG_Debris = 0x7F1E50;
	static const uint32_t ORIG_Darken = 0x7F1B20;      // stage darkening (engine task a_73F110)
	static const uint32_t TGT_WORD0C = 0x2606C58;      // the target's entity +0x0C / +0x10 words, kept while it is pushed
	static const uint32_t TGT_WORD10 = 0x2606C5C;
	static const uint32_t SLIDE_CURVE = 0x1597820;     // the target's slide curve (s16 0..0x1000 per tick, exe data)
	static const uint32_t STAGE_LIGHT = 0x1D98992;     // the four stage light words (+ k * 0x2C)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x (the primitive-list renderers)
	namespace gx
	{
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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl rj_7EB200(uint32_t a1);
	uint32_t __cdecl rj_7EB390(uint32_t a1);
	uint32_t __cdecl rj_7EB3D0(void);
	uint32_t __cdecl rj_7EB4C0(uint32_t a1);
	uint32_t __cdecl rj_7EB4F0(uint32_t a1);
	uint32_t __cdecl rj_7EB580(uint32_t a1);
	uint32_t __cdecl rj_7EB600(uint32_t a1);
	uint32_t __cdecl rj_7EB6A0(uint32_t a1);
	uint32_t __cdecl rj_7EB6F0(uint32_t a1);
	uint32_t __cdecl rj_7EB710(uint32_t a1);
	uint32_t __cdecl rj_7EB790(uint32_t a1);
	uint32_t __cdecl rj_7EB870(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl rj_7EB8C0(uint32_t a1);
	uint32_t __cdecl rj_7EB920(uint32_t a1);
	uint32_t __cdecl rj_7EBA10(uint32_t a1);
	uint32_t __cdecl rj_7EBAC0(uint32_t a1);
	uint32_t __cdecl rj_7EBBB0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rj_7EBDF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7EBF20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7EC140(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7EC3D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7EC850(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7ED300(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7ED800(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl rj_7EDEB0(uint32_t a1);
	uint32_t __cdecl rj_7EDF40(uint32_t a1);
	uint32_t __cdecl rj_7EDF70(uint32_t a1);
	uint32_t __cdecl rj_7EE150(uint32_t a1);
	uint32_t __cdecl rj_7EE360(uint32_t a1);
	uint32_t __cdecl rj_7EE5E0(void);
	uint32_t __cdecl rj_7EEC90(uint32_t a1);
	uint32_t __cdecl rj_7EED20(void);
	uint32_t __cdecl rj_7EED80(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7EEEE0(void);
	uint32_t __cdecl rj_7EEFA0(uint32_t a1);
	uint32_t __cdecl rj_7EF020(void);
	uint32_t __cdecl rj_7EF060(uint32_t a1);
	uint32_t __cdecl rj_7F0090(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7F01A0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7F0220(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7F0250(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rj_7F02F0(uint32_t a1);
	uint32_t __cdecl rj_7F0370(uint32_t a1);
	uint32_t __cdecl rj_7F03F0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl rj_7F0BD0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7F0DF0(uint32_t a1);
	uint32_t __cdecl rj_7F18E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl rj_7F1970(void);
	uint32_t __cdecl rj_7F1A00(uint32_t a1);
	uint32_t __cdecl rj_7F1A90(uint32_t a1);
	uint32_t __cdecl rj_7F1AF0(uint32_t a1);
	uint32_t __cdecl rj_7F1B90(uint32_t a1);
	uint32_t __cdecl rj_7F1BD0(uint32_t a1);
	uint32_t __cdecl rj_7F1C10(uint32_t a1);
	uint32_t __cdecl rj_7F1C20(uint32_t a1);
	uint32_t __cdecl rj_7F1CD0(uint32_t a1);
	uint32_t __cdecl rj_7F1D20(uint32_t a1);
	uint32_t __cdecl rj_7F1E50(uint32_t a1);
	uint32_t __cdecl rj_7F1F30(uint32_t a1);
	uint32_t __cdecl rj_7F2600(uint32_t a1);
	uint32_t __cdecl rj_7F2620(uint32_t a1);
	uint32_t __cdecl rj_7F2690(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag060_raijin_attack_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace raijin
{
	// ====================================================================================
	// the module's own code
	// ====================================================================================

	// 0x7EB710 (sub_7EB710): director state 3 - once the phase has run 0x24 ticks: the damage cue
	// (director block +0x48 = 1, read by the emitter's 0x7F2720), next state
	uint32_t __cdecl rj_7EB710(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) < 0x24)
			return d;
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7EB790 (sub_7EB790): the tasks of a new phase (phase 1 only): two actor systems (0x7EB820 =
	// engine a_73C100: 0x7EE0F0 on the actor data 0x1797CC0 from tick 6, 0x7F1A30 on 0x179C998 from
	// tick 0x24), the strike model task 0x7EB8C0 (prim layout 0x17A0D04, 0x18C bytes, model queue),
	// the debris spawner 0x7F1C70 and the stage darkening 0x7F1B20 (0x8C bytes, task queue)
	uint32_t __cdecl rj_7EB790(uint32_t a1)
	{
		const int32_t ph = (int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x40) - 1;
		if (ph != 0)
			return (uint32_t)ph;
		const uint32_t node = a1;
		a_73C100(node, ORIG_ActorsA, 0x1797CC0, 6, 0x28, 4);
		a_73C100(node, ORIG_ActorsB, 0x179C998, 0x24, 0x28, 4);
		rj_7EB870(node, ORIG_Strike, 0x17A0D04, 0xE4, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_DebrisSpawner, 0x8C, node);
		return x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Darken, 0x8C, node);
	}

	// 0x7EB870 (sub_7EB870): spawns model task a2 (0x18C bytes, model queue) as a child of a1 with
	// +0x74 = prim layout a3, +0x78 = (s16) a4, +0x80 = a5, +0x82 = a6 (words); returns the node
	uint32_t __cdecl rj_7EB870(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_MODELS, a2, 0x18C, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x7EB920 (sub_7EB920): strike task state 0 - from tick 6: keeps the target's +0x0C / +0x10
	// words (0x2606C58 / 0x2606C5C) and position (+0x180 / +0x184), turns the target to face the
	// caster (director +0x4A - 0x800), pushes it back (its facing matrix times (0, 0, -0x200)) and
	// keeps the pushed position (+0x178 / +0x17C), next state
	uint32_t __cdecl rj_7EB920(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 6)
			return 0; // void
		// stack: +0 SVECTOR (+6 pad never written; matrixMultiplyVector reads and writes 3 words),
		// +8 the 0x20-byte matrix
		alignas(4) uint8_t fr[0x28] = {};
		const uint32_t L = P(fr);
		const uint32_t M = L + 8;
		const uint32_t ent = ENT(U8(node, 0x2D));
		MEM<uint32_t>(TGT_WORD10) = U32(ent, 0x10);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		MEM<uint32_t>(TGT_WORD0C) = U32(ent, 0xC);
		U32(node, 0x180) = U32(ent, 0x1C);
		U32(node, 0x184) = U32(ent, 0x20);
		U16(ent, 0xE) = (uint16_t)((U16(d, 0x4A) - 0x800) & 0xFFF);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(ent, 0xE));
		U16(L, 0) = 0;
		U16(L, 2) = 0;
		U16(L, 4) = 0xFE00;
		x::matrixMultiplyVector(M, L, L);
		U16(ent, 0x1C) = (uint16_t)(U16(ent, 0x1C) + U16(L, 0));
		U16(ent, 0x1E) = (uint16_t)(U16(ent, 0x1E) + U16(L, 2));
		U16(ent, 0x20) = (uint16_t)(U16(ent, 0x20) + U16(L, 4));
		U32(node, 0x178) = U32(ent, 0x1C);
		U32(node, 0x17C) = U32(ent, 0x20);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7EBA10 (sub_7EBA10): strike task state 1 - at tick 0x24: decodes its prim layout (+0x74,
	// player +0x94, start +0x78), the caster's matrix (entity +0x40) as its own, scale 0x1000, colour
	// word +0x88 = 0xFE00, at the caster's bone point (0x19, 0x400), draws it (0x7EBAC0), spawns the
	// impact model task 0x7EDE50 (prim layout 0x17A3150, start 0x24, +0x80 = 1), clears +0x84, next state
	uint32_t __cdecl rj_7EBA10(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x24)
			return 0; // void
		const uint32_t ent = ENT(U8(node, 0x2C));
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		memcpy((void *)(node + 0x30), (const void *)(ent + 0x40), 0x20); // rep movsd
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFE00;
		x::GetEffectSpawnPosition(ent, 0x19, 0x400, node + 0x1C);
		rj_7EBAC0(node);
		rj_7EB870(node, ORIG_Impact, 0x17A3150, 0x24, 1, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		U16(node, 0x84) = 0;
		return 0; // void
	}

	// 0x7EDEB0 (sub_7EDEB0): impact model task state 0 - decodes its prim layout, scale 0x1000, the
	// target's matrix (entity +0x40) turned by 0x800 about y, colour word 0xFE00, at the target's
	// point 0xF1 on the ground (y = 0), draws it (0x7EBAC0), next state (0x7EDF40 = Psycho Blast's
	// 0x8960C0: drawn until its layout ends)
	uint32_t __cdecl rj_7EDEB0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		memcpy((void *)(node + 0x30), (const void *)(ent + 0x40), 0x20); // rep movsd
		U16(node, 0x88) = 0xFE00;
		x::MAG_022_sub_8DD8A0(node + 0x30, 0x800);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		U16(node, 0x1E) = 0;
		rj_7EBAC0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7EDF70 (sub_7EDF70): strike task state 2 - the target slides from the pushed position
	// (+0x178) back to its own (+0x180) by the curve 0x1597820[+0x84] (0..0x1000; at 0x1000 it is
	// put back exactly and +0x84 stops), the strike model follows the target's move by 3/2, drawn
	// (0x7EBAC0); when its layout ends the target's +0x0C / +0x10 words are restored, finished, next state
	uint32_t __cdecl rj_7EDF70(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		const uint16_t k = U16(node, 0x84);
		const int16_t ox = S16(ent, 0x1C);   // the target's position before the move ([esp+0xC] / +0x10)
		const int16_t oy = S16(ent, 0x1E);
		const int16_t oz = S16(ent, 0x20);
		const uint16_t f = MEM<uint16_t>(SLIDE_CURVE + (uint32_t)((int32_t)(int16_t)k * 2));
		U16(node, 0x188) = f;
		if (f == 0x1000)
		{
			U32(ent, 0x1C) = U32(node, 0x180);
			U32(ent, 0x20) = U32(node, 0x184);
		}
		else
		{
			const int32_t t = S16(node, 0x188);
			U16(node, 0x84) = (uint16_t)(k + 1);
			const int16_t sx = S16(node, 0x178);
			U16(ent, 0x1C) = (uint16_t)((((int32_t)S16(node, 0x180) - sx) * t) / 4096 + sx);
			const int16_t sy = S16(node, 0x17A);
			U16(ent, 0x1E) = (uint16_t)((((int32_t)S16(node, 0x182) - sy) * t) / 4096 + sy);
			const int16_t sz = S16(node, 0x17C);
			U16(ent, 0x20) = (uint16_t)((((int32_t)S16(node, 0x184) - sz) * t) / 4096 + sz);
		}
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)((((int32_t)S16(ent, 0x1C) - ox) * 3) / 2));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + (uint16_t)((((int32_t)S16(ent, 0x1E) - oy) * 3) / 2));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)((((int32_t)S16(ent, 0x20) - oz) * 3) / 2));
		if (rj_7EBAC0(node) == 0)
		{
			U8(node, 0x26) |= 1;
			U32(ent, 0xC) = MEM<uint32_t>(TGT_WORD0C);
			U32(ent, 0x10) = MEM<uint32_t>(TGT_WORD10);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7EE150 (sub_7EE150) and 0x7F1A90 (sub_7F1A90, the same code): actor system state 0 - once its
	// start tick (+0x298) is reached: the caster's bone point (0x19, 0x400) -> +0x290, the actor
	// director set-up (0x7EE1B0 = engine a_73F990), the first actor step (0x7EEC90), next state
	uint32_t __cdecl rj_7EE150(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < S16(node, 0x298))
			return 0; // void
		x::GetEffectSpawnPosition(ENT(U8(node, 0x2C)), 0x19, 0x400, node + 0x290);
		a_73F990(node);
		rj_7EEC90(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}
	uint32_t __cdecl rj_7F1A90(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < S16(node, 0x298))
			return 0; // void
		x::GetEffectSpawnPosition(ENT(U8(node, 0x2C)), 0x19, 0x400, node + 0x290);
		a_73F990(node);
		rj_7EEC90(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// ---- the stage darkening (0x7F1B20 = engine a_73F110 on {0x7F1B90, 0x7F1BD0, 0x7F1C10, 0x7F1C20, ret})

	// 0x7F1B90 (sub_7F1B90): darkening state 0 - at tick 5: level +0x1C = 0, the four stage light
	// words 0x1D98992 + k * 0x2C and their three colour bytes +0x26..+0x28 cleared, next state
	uint32_t __cdecl rj_7F1B90(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 5)
			return 0; // void
		U16(node, 0x1C) = 0;
		uint32_t p = STAGE_LIGHT + 0x28;
		for (int k = 4; k != 0; k--)
		{
			MEM<uint16_t>(p - 0x28) = 0;
			MEM<uint8_t>(p) = 0;
			MEM<uint8_t>(p - 1) = 0;
			MEM<uint8_t>(p - 2) = 0;
			p += 0x2C;
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7F1BD0 (sub_7F1BD0): darkening state 1 - level +0x1C += 0x100 up to 0x600 (then next
	// state), written to the four stage light words
	uint32_t __cdecl rj_7F1BD0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + 0x100);
		if (S16(node, 0x1C) >= 0x600)
		{
			U16(node, 0x1C) = 0x600;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		const uint16_t v = U16(node, 0x1C);
		for (int k = 0; k < 4; k++)
			MEM<uint16_t>(STAGE_LIGHT + (uint32_t)k * 0x2C) = v;
		return 0; // void
	}

	// 0x7F1C10 (sub_7F1C10): darkening state 2 - held until tick 0x36, next state (0x7F1C20 = Doom's
	// 0x8CC350: level -= 0x100 down to 0, finished)
	uint32_t __cdecl rj_7F1C10(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x36)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// ---- the debris (0x7F1C70 = engine a_73A380 on {0x7F1CD0, 0x7F1D20, ret})

	// 0x7F1CD0 (sub_7F1CD0): debris spawner state 0 - at tick 0x24: matrix +0x58 = the target's
	// facing (entity +0x0E about y), next state
	uint32_t __cdecl rj_7F1CD0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		if (S16(node, 0x24) < 0x24)
			return 0; // void
		x::MAG_022_sub_8DD770(node + 0x58);
		x::MAG_022_sub_8DD8A0(node + 0x58, (uint32_t)(int32_t)S16(ent, 0xE));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7F1D20 (sub_7F1D20): debris spawner state 1 - every tick two debris tasks 0x7F1E50 (0x8C
	// bytes, task queue) at the target's position on the ground, each moved by the facing matrix
	// times (rand 0..0x1FF - 0x100, 0, rand 0..0xFF - 0x80), +0x82 = -(rand & 0x7F) (gravity), velocity
	// +0x78 = the facing matrix times (rand & 0x1FF - 0x100, 0, -0x200 - (rand & 0x7FF)); finished
	// from tick 0x2C
	uint32_t __cdecl rj_7F1D20(uint32_t a1)
	{
		const uint32_t node = a1;
		// stack: +0 SVECTOR (+6 pad never written; matrixMultiplyVector reads and writes 3 words);
		// the loop count lives in the argument's stack slot
		alignas(4) uint8_t fr[8] = {};
		const uint32_t L = P(fr);
		const uint32_t M = node + 0x58;
		const uint32_t ent = ENT(U8(node, 0x2D));
		U32(node, 0x1C) = U32(ent, 0x1C);
		U32(node, 0x20) = U32(ent, 0x20);
		U16(node, 0x1E) = 0;
		int32_t n = 2;
		do
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Debris, 0x8C, node);
			U32(t, 0x1C) = U32(node, 0x1C);
			U32(t, 0x20) = U32(node, 0x20);
			const uint32_t r0 = x::CrtRand();
			U16(t, 0x82) = (uint16_t)(0u - (r0 & 0x7F));
			const uint32_t r1 = x::CrtRand();
			U16(L, 2) = 0;
			U16(L, 0) = (uint16_t)((r1 & 0x1FF) - 0x100);
			const uint32_t r2 = x::CrtRand();
			U16(L, 4) = (uint16_t)((r2 & 0xFF) - 0x80);
			x::matrixMultiplyVector(M, L, L);
			U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + U16(L, 0));
			U16(t, 0x1E) = (uint16_t)(U16(t, 0x1E) + U16(L, 2));
			U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + U16(L, 4));
			const uint32_t r3 = x::CrtRand();
			U16(t, 0x7A) = 0;
			U16(t, 0x78) = (uint16_t)((r3 & 0x1FF) - 0x100);
			const uint32_t r4 = x::CrtRand();
			U16(t, 0x7C) = (uint16_t)(0xFFFFFE00u - (r4 & 0x7FF));
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
			n--;
		} while (n != 0);
		if (S16(node, 0x24) >= 0x2C)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7F1E50 (sub_7F1E50): DEBRIS task - state {0x7F2600 (= Aqua Breath's 0x80C540), 0x7F2620
	// (= Doom's 0x8CDE60), ret}; velocity +0x78 (+ gravity +0x82 on y) loses 1/8 a tick, position
	// += velocity / 16, drawn by 0x7F1F30 (= Doom's 0x8CD770); ends when finished and no child
	uint32_t __cdecl rj_7F1E50(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7F2600;
		states[1] = 0x7F2620;
		states[2] = 0x7F2670; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		int16_t vx = S16(node, 0x78);
		int16_t vy = (int16_t)(U16(node, 0x82) + U16(node, 0x7A));
		int16_t vz = S16(node, 0x7C);
		vx = (int16_t)(vx - vx / 8);
		U16(node, 0x78) = (uint16_t)vx;
		vy = (int16_t)(vy - vy / 8);
		U16(node, 0x7A) = (uint16_t)vy;
		vz = (int16_t)(vz - vz / 8);
		U16(node, 0x7C) = (uint16_t)vz;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)(vx / 16));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + (uint16_t)(vy / 16));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)(vz / 16));
		// 30 fps layer: see mag060_raijin_attack_held.inc
		FX_HELD(held_note_debris(node);)
		rj_7F1F30(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}


	// 0x7EBBB0 (sub_7EBBB0; Curse's cu_82B870 drawing at the module packet cursor 0x260407C instead of
	// the frame arena cursor 0x1D8E054): prim-model player draw callback (layout a1, record a2, block
	// a3) - as Doom's 0x8C5B10 without its record flags 0x400 / 0x100 / 0x4000: picks the object's vertex
	// frame (lerped by MAG_017_sub_701390), builds its matrix (rotation 0x701310; parent-rotated
	// position unless record flag 0x200; scale3DMatrix), fills a 0x68-byte Field_Alloc render
	// header (flags 0x2030, 0x20F0 with alpha + colour, depth offset block +0x58, scale words 0x100)
	// and draws it with 0x7EBDF0 into OT base+0x44 (shift 2) at the module packet cursor 0x260407C
	uint32_t __cdecl rj_7EBBB0(uint32_t a1, uint32_t a2, uint32_t a3)
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
		MEM<uint32_t>(PACKET_CURSOR) = rj_7EBDF0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// ====================================================================================
	// the library functions of this module whose code matches a Heartbreak / Psycho Blast /
	// Raldo Throw / Doom / Wind Blast / Aqua Breath / Curse port up to the module's
	// addresses and callees (the port's text with the module's addresses; the comments describe the
	// source original)
	// ====================================================================================

	// 0x86CB50 (copy of pb_893190 0x893190: MAG_023_sub_893190; Raldo Throw rt_8A33F0 with this module's table and six queues):
	// MASTER task - camera copy, packet arena by tick parity, bone follow, 11-state table
	// {0x893300, 0x893310 (a_73A0D0), 0x893320 carve, 0x8933A0 emitters, 0x89A5C0 .. 0x89A660, ret},
	// the six queues (live task count -> node +0x5E; the actor counters 0x27044C0 / 0x270420C are
	// cleared before the queues run)
	uint32_t __cdecl rj_7EB200(uint32_t a1)
	{
		g_mod = &MOD_060;
		// 30 fps layer: see mag060_raijin_attack_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x2604188) = 0x2793E58;
		states[0] = 0x7EB370;
		states[1] = 0x7EB380;
		const uint8_t parity = U8(node, 0x5C);
		states[2] = 0x7EB390;
		states[3] = 0x7EB410;
		states[4] = 0x7F2720;
		states[5] = 0x7F2760;
		states[6] = 0x7F2770;
		states[7] = 0x7F2780;
		states[8] = 0x7F27B0;
		states[9] = 0x7F27C0;
		states[10] = 0x7F27E0; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2606BF4);
			const uint32_t v2 = MEM<uint32_t>(0x2606294);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2604184) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2606BF0);
			const uint32_t v2 = MEM<uint32_t>(0x2606290);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2604184) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x2607040) = 0;
		MEM<uint16_t>(0x2606C60) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_ACTORS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_TASKS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_MODELS));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x893320 (copy of rt_8A3570 0x8A3570: copy of dk_856CC0 0x856CC0: copy of dw_8B0BC0 0x8B0BC0: copy of dm_8C4CE0 0x8C4CE0: copy of d_8B6BE0 0x8B6BE0: MAG_014_sub_8B6BE0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x27044D4] (0x438 + 0xD200 bytes) and cleared, next state
	uint32_t __cdecl rj_7EB390(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x2607054);
		MEM<uint32_t>(0x2606C64) = p;
		p += 0x438;
		MEM<uint32_t>(0x260704C) = p;
		p += 0xD200;
		MEM<uint32_t>(0x2607054) = p;
		rj_7EB3D0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x893360 (copy of rt_8A35B0 0x8A35B0: copy of dk_856D00 0x856D00: copy of dw_8B0C00 0x8B0C00: copy of dm_8C4D20 0x8C4D20: copy of d_8B6C20 0x8B6C20: MAG_014_sub_8B6C20): clears the two actor pools (0x438 bytes at [0x2704210], 0xD200
	// at [0x27044CC]) and the pool cursors / counters
	uint32_t __cdecl rj_7EB3D0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x2606C64), 0x438);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x260704C), 0xD200);
		MEM<uint16_t>(0x2605DD0) = 0;
		MEM<uint16_t>(0x2606C70) = 0;
		MEM<uint16_t>(0x2606BF8) = 0;
		MEM<uint16_t>(0x2604080) = 0;
		return 0; // void
	}

	// 0x8A36A0 (copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - places the reaper, spawns the director
	// 0x8B6E30 (0x48 bytes, director queue), next state
	uint32_t __cdecl rj_7EB4C0(uint32_t a1)
	{
		rj_7EB4F0(a1);
		x::Effect_AddTaskAndInitFromCtx(0x2604160, 0x7EB580, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A36D0 (MAG_019_sub_8A36D0): clears the director block [0x1629FA8] (0x54 bytes), keeps the
	// positions of the caster (node +0x2C: +0x00 / +0x04) and of the target (node +0x2D: +0x08 /
	// +0x0C) and the caster's facing towards the target (CartesianToGameAngle of the x / z delta,
	// - 0x800, 12 bits) in +0x4A
	uint32_t __cdecl rj_7EB4F0(uint32_t a1)
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

	// 0x808DD0 (MAG_055_sub_808DD0): DIRECTOR task - phase bookkeeping (0x808E50), then the state
	// {0x808EA0 stage darkening, 0x809020 cue to phase 1, 0x809080 sound, 0x8090D0 damage cue,
	// 0x8090F0 cue to phase 2, 0x809110 finish, ret}
	uint32_t __cdecl rj_7EB580(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x7EB650;
		states[1] = 0x7EB6A0;
		states[2] = 0x7EB6F0;
		states[3] = 0x7EB710;
		states[4] = 0x7EB730;
		states[5] = 0x7EB750;
		states[6] = 0x7EB770; // nullsub (ret)
		rj_7EB600(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl rj_7EB600(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			rj_7EB790(a1);
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

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl rj_7EB6A0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x159735C, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x8A38C0 (copy of s_73B880 0x73B880: module; Siren sub_73B880): director state: advances once the cut frame count
	// (+0x46) reaches 4
	uint32_t __cdecl rj_7EB6F0(uint32_t a1)
	{
		uint32_t blk = U32(0x1597358, 0);
		if (S16(blk, 0x46) < 4) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x893950 (copy of s_73F250 0x73F250: module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl rj_7EB8C0(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x7EB920, 0x7EBA10, 0x7EDF70, 0x7EE0E0 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x893A30 (sub_893A30; engine a_73C280 with the node's matrix +0x30 instead of its angles and no
	// pause): draws the node's prim model: matrix = camera * (matrix +0x30 scaled by +0x50,
	// translation +0x1C..0x20), plays the prim player at +0x94 with callback 0x893B20 and a parameter
	// block (matrix + node colours/params, blend buffer 0x2704220); returns the player's result
	uint32_t __cdecl rj_7EBAC0(uint32_t a1)
	{
		// stack frame 0x5C: [0..0x1F] matrix, [0x38] a1+0x70, [0x44] a1+0x7C, [0x48] blend buffer,
		// [0x4C..0x59] words 8C 8E 90 92 86 80 88 (0x20..0x37, 0x3C..0x43, 0x5A are not written;
		// the callback 0x7EBBB0 does not read them)
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
		// 30 fps layer: see mag060_raijin_attack_held.inc
		FX_HELD(held_note_play(a1);)
		return prim_play(a1 + 0x94, PRIM_CB, L, 0);
	}

	// 0x8A4830 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8A4960, 0x8A4B80, 0x8A4E10, 0x8A5290, a_73D770, a_73D9C0, 0x8A5D40,
	// 0x8A6240; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl rj_7EBDF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { rj_7EBF20, rj_7EC140, rj_7EC3D0, rj_7EC850, a_73D770, a_73D9C0, rj_7ED300, rj_7ED800 };
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
	uint32_t __cdecl rj_7EBF20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7EC140(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7EC3D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7EC850(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7ED300(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7ED800(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl rj_7EDF40(uint32_t a1)
	{
		const uint32_t r = rj_7EBAC0(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl rj_7EE360(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x2607058);
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
	uint32_t __cdecl rj_7EE5E0(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x2607058);
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
	uint32_t __cdecl rj_7EEC90(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x2607058, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			rj_7F1970();
			rj_7EF020();
			// 30 fps layer: see mag060_raijin_attack_held.inc
			FX_HELD(held_note_system(sys);)
			rj_7EEEE0();
			rj_7EED20();
			sys = U32(0x2607058, 0);
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
		U16(0x2607040, 0) = (uint16_t)(U16(0x2607040, 0) + c14);
		U16(0x2606C60, 0) = (uint16_t)(U16(0x2606C60, 0) + c16);
		return done;
	}

	// 0x896E70 (copy of rt_8A4670 0x8A4670: copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl rj_7EED20(void)
	{
		uint32_t act = U32(U32(0x2607058, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x2607058, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					rj_7EED80(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x896ED0 (copy of rt_8A46D0 0x8A46D0: copy of dm_8C9530 0x8C9530: copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl rj_7EED80(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = rj_7EBDF0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = rj_7EBDF0(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
	uint32_t __cdecl rj_7EEEE0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x2607058), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x2607058);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						rj_7EEFA0(act);   // (the original also pushes the bone entry, unused)
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
							rj_7EEFA0(act);
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
	uint32_t __cdecl rj_7EEFA0(uint32_t a1)
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
	uint32_t __cdecl rj_7EF020(void)
	{
		uint32_t act = U32(U32(0x2607058, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				rj_7F02F0(act);
			else if (type == 1)
				rj_7EF060(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9810 (copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl rj_7EF060(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x2607058);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (rj_7F01A0(act, bone) == 0)
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
			st = MEM<uint32_t>(0x2607058);
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
	uint32_t __cdecl rj_7F0090(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x2607058, 0);                  // actor state block
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

	// 0x8CA950 (copy of mb_828CB0 0x828CB0: copy of db_84A030 0x84A030: copy of cg_881650 0x881650: sub_881650; = Confuse c_862FF0, copy of a_7419C0 0x7419C0, callee chain to 0x881700): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl rj_7F01A0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				rj_7F0220(a1, a2);
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
			rj_7F0220(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			rj_7F0220(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x8CA9D0 (copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl rj_7F0220(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return rj_7F0250(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x8CAA00 (copy of d_8BBC10 0x8BBC10: sub_8BBC10): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 10 records
	uint32_t __cdecl rj_7F0250(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x2606C64);
		int32_t idx = MEM<int16_t>(0x2605DD0);
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
			MEM<uint16_t>(0x2605DD0) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x2605DD0) = 0;
		return slot;
	}

	// 0x898440 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl rj_7F02F0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x7F0CA0, a1);
		else if (mode == 4)
			callp(0x7F0DF0, a1);
		rj_7F0370(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8984C0 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl rj_7F0370(uint32_t a1)
	{
		uint32_t sys = U32(0x2607058, 0);
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
				rj_7F03F0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			rj_7F03F0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x898540 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl rj_7F03F0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x2607048, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x2607048, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = rj_7F0BD0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x2607058, 0);
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
		rj_7F18E0(node, desc);
		U32(0x2607048, 0) = U32(0x2607048, 0) + 0x50;
		return 0; // void
	}

	// 0x87CB40 (sub_87CB40; = Curaga cg_882080 with this module's pool of 0x50 records): the
	// engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes from the pool
	// [0x26D74E4] starting at the cursor 0x26D74D8, cleared, owner a1, definition index a2; the
	// cursor wraps at 0x4F)
	uint32_t __cdecl rj_7F0BD0(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x260704C);
		int32_t i = MEM<int16_t>(0x2606C70);
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
			MEM<uint16_t>(0x2606C70) = 0;
		else
			MEM<uint16_t>(0x2606C70) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl rj_7F0DF0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x2607058, 0);            // ecx
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
				uint32_t g = U32(0x2607058, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x2607058, 0);
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

	// 0x8CC090 (copy of mb_82A3F0 0x82A3F0: copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl rj_7F18E0(uint32_t a1, uint32_t a2)
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
			rj_7F0250(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			rj_7F0250(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			rj_7F0250(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			rj_7F0250(obj, id, variant);
		}
		return 0; // void
	}

	// 0x8CC120 (copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via rj_7F0250(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl rj_7F1970(void)
	{
		uint32_t dir = MEM<uint32_t>(0x2607058);  // eax (re-read only after the calls)
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
					rj_7F0250(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x2607058);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				rj_7F0250(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x2607058);
			}
		}
		return 0; // void
	}

	// 0x899B50 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl rj_7F1A00(uint32_t a1)
	{
		if (rj_7EEC90(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x899B50 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl rj_7F1AF0(uint32_t a1)
	{
		if (rj_7EEC90(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl rj_7F1C20(uint32_t a1)
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

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl rj_7F1F30(uint32_t a1)
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
		const uint32_t cursor = MEM<uint32_t>(0x260407C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x260407C) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x80C540 (sub_80C540): splash drops state 0 - sprite 0x15A3E5C, depth +0x56 = -0x40, last frame
	// +0x52 = 0xF, next state
	uint32_t __cdecl rj_7F2600(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x1597360;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 0xF;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl rj_7F2620(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl rj_7F2690(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1597358);
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
		{ 0x7EB370, (void *)a_73A0D0, "060 MAG_060_sub_7EB370" },
		{ 0x7EB380, (void *)a_73A0D0, "060 MAG_060_sub_7EB380" },
		{ 0x7EB410, (void *)a_73A170, "060 MAG_060_sub_7EB410" },
		{ 0x7EB440, (void *)a_73A1A0, "060 MAG_060_sub_7EB440" },
		{ 0x7EB650, (void *)a_73B790, "060 sub_7EB650" },
		{ 0x7EB670, (void *)a_73B640, "060 sub_7EB670" },
		{ 0x7EB6D0, (void *)a_73B7E0, "060 sub_7EB6D0" },
		{ 0x7EB730, (void *)a_73B8A0, "060 sub_7EB730" },
		{ 0x7EB750, (void *)a_7475A0, "060 sub_7EB750" },
		{ 0x7EB770, (void *)a_73A6D0, "060 nullsub_1470" },
		{ 0x7EB780, (void *)a_73A6D0, "060 nullsub_1471" },
		{ 0x7EB820, (void *)a_73C100, "060 sub_7EB820" },
		{ 0x7ECDD0, (void *)a_73D770, "060 sub_7ECDD0" },
		{ 0x7ED020, (void *)a_73D9C0, "060 sub_7ED020" },
		{ 0x7EDE50, (void *)a_73A380, "060 sub_7EDE50" },
		{ 0x7EDF60, (void *)a_73A6D0, "060 nullsub_1472" },
		{ 0x7EE0E0, (void *)a_73A6D0, "060 nullsub_1473" },
		{ 0x7EE0F0, (void *)a_73A380, "060 sub_7EE0F0" },
		{ 0x7EE1B0, (void *)a_73F990, "060 sub_7EE1B0" },
		{ 0x7EE440, (void *)a_73FC20, "060 sub_7EE440" },
		{ 0x7EE6B0, (void *)a_73FE90, "060 sub_7EE6B0" },
		{ 0x7EE7D0, (void *)a_73FFB0, "060 sub_7EE7D0" },
		{ 0x7EEA30, (void *)a_740210, "060 sub_7EEA30" },
		{ 0x7EF0F0, (void *)a_740910, "060 sub_7EF0F0" },
		{ 0x7EF750, (void *)a_740F70, "060 sub_7EF750" },
		{ 0x7EF7A0, (void *)a_740FC0, "060 sub_7EF7A0" },
		{ 0x7EF830, (void *)a_741050, "060 sub_7EF830" },
		{ 0x7EF900, (void *)a_741120, "060 sub_7EF900" },
		{ 0x7F0340, (void *)a_741B60, "060 sub_7F0340" },
		{ 0x7F0A70, (void *)a_742290, "060 au_re__rand_33" },
		{ 0x7F0AA0, (void *)a_7422C0, "060 sub_7F0AA0" },
		{ 0x7F0AE0, (void *)a_742300, "060 sub_7F0AE0" },
		{ 0x7F0B30, (void *)a_742350, "060 sub_7F0B30" },
		{ 0x7F0B80, (void *)a_7423A0, "060 au_re__rand_33_0" },
		{ 0x7F0C90, (void *)a_7424B0, "060 sub_7F0C90" },
		{ 0x7F0CA0, (void *)a_7424C0, "060 sub_7F0CA0" },
		{ 0x7F1480, (void *)a_742CA0, "060 sub_7F1480" },
		{ 0x7F14B0, (void *)a_742CD0, "060 sub_7F14B0" },
		{ 0x7F14E0, (void *)a_742D00, "060 sub_7F14E0" },
		{ 0x7F1690, (void *)a_742EB0, "060 sub_7F1690" },
		{ 0x7F17D0, (void *)a_742FF0, "060 sub_7F17D0" },
		{ 0x7F1A20, (void *)a_73A6D0, "060 nullsub_1474" },
		{ 0x7F1A30, (void *)a_73A380, "060 sub_7F1A30" },
		{ 0x7F1B10, (void *)a_73A6D0, "060 nullsub_1475" },
		{ 0x7F1B20, (void *)a_73F110, "060 sub_7F1B20" },
		{ 0x7F1C60, (void *)a_73A6D0, "060 nullsub_1476" },
		{ 0x7F1C70, (void *)a_73A380, "060 sub_7F1C70" },
		{ 0x7F2050, (void *)a_7435E0, "060 InitEffectSequenceFromData_c12" },
		{ 0x7F2190, (void *)a_743720, "060 sub_7F2190" },
		{ 0x7F2640, (void *)a_743C20, "060 sub_7F2640" },
		{ 0x7F2670, (void *)a_73A6D0, "060 nullsub_1477" },
		{ 0x7F2680, (void *)a_73A6D0, "060 nullsub_1478" },
		{ 0x7F26D0, (void *)a_7474B0, "060 MAG_060_sub_7F26D0" },
		{ 0x7F26F0, (void *)a_7474D0, "060 MAG_060_sub_7F26F0" },
		{ 0x7F2710, (void *)a_73A6D0, "060 nullsub_1479" },
		{ 0x7F2720, (void *)a_747500, "060 MAG_060_sub_7F2720" },
		{ 0x7F2760, (void *)a_73A0D0, "060 MAG_060_sub_7F2760" },
		{ 0x7F2770, (void *)a_747550, "060 MAG_060_sub_7F2770" },
		{ 0x7F2780, (void *)a_747560, "060 MAG_060_sub_7F2780" },
		{ 0x7F27B0, (void *)a_747590, "060 MAG_060_sub_7F27B0" },
		{ 0x7F27C0, (void *)a_7475A0, "060 MAG_060_sub_7F27C0" },
		{ 0x7F27E0, (void *)a_73A6D0, "060 nullsub_1469" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x7EB200, (void *)rj_7EB200, "060 rj_7EB200" },
		{ 0x7EB390, (void *)rj_7EB390, "060 rj_7EB390" },
		{ 0x7EB3D0, (void *)rj_7EB3D0, "060 rj_7EB3D0" },
		{ 0x7EB4C0, (void *)rj_7EB4C0, "060 rj_7EB4C0" },
		{ 0x7EB4F0, (void *)rj_7EB4F0, "060 rj_7EB4F0" },
		{ 0x7EB580, (void *)rj_7EB580, "060 rj_7EB580" },
		{ 0x7EB600, (void *)rj_7EB600, "060 rj_7EB600" },
		{ 0x7EB6A0, (void *)rj_7EB6A0, "060 rj_7EB6A0" },
		{ 0x7EB6F0, (void *)rj_7EB6F0, "060 rj_7EB6F0" },
		{ 0x7EB710, (void *)rj_7EB710, "060 rj_7EB710" },
		{ 0x7EB790, (void *)rj_7EB790, "060 rj_7EB790" },
		{ 0x7EB870, (void *)rj_7EB870, "060 rj_7EB870" },
		{ 0x7EB8C0, (void *)rj_7EB8C0, "060 rj_7EB8C0" },
		{ 0x7EB920, (void *)rj_7EB920, "060 rj_7EB920" },
		{ 0x7EBA10, (void *)rj_7EBA10, "060 rj_7EBA10" },
		{ 0x7EBAC0, (void *)rj_7EBAC0, "060 rj_7EBAC0" },
		{ 0x7EBBB0, (void *)rj_7EBBB0, "060 rj_7EBBB0" },
		{ 0x7EBDF0, (void *)rj_7EBDF0, "060 rj_7EBDF0" },
		{ 0x7EBF20, (void *)rj_7EBF20, "060 rj_7EBF20" },
		{ 0x7EC140, (void *)rj_7EC140, "060 rj_7EC140" },
		{ 0x7EC3D0, (void *)rj_7EC3D0, "060 rj_7EC3D0" },
		{ 0x7EC850, (void *)rj_7EC850, "060 rj_7EC850" },
		{ 0x7ED300, (void *)rj_7ED300, "060 rj_7ED300" },
		{ 0x7ED800, (void *)rj_7ED800, "060 rj_7ED800" },
		{ 0x7EDEB0, (void *)rj_7EDEB0, "060 rj_7EDEB0" },
		{ 0x7EDF40, (void *)rj_7EDF40, "060 rj_7EDF40" },
		{ 0x7EDF70, (void *)rj_7EDF70, "060 rj_7EDF70" },
		{ 0x7EE150, (void *)rj_7EE150, "060 rj_7EE150" },
		{ 0x7EE360, (void *)rj_7EE360, "060 rj_7EE360" },
		{ 0x7EE5E0, (void *)rj_7EE5E0, "060 rj_7EE5E0" },
		{ 0x7EEC90, (void *)rj_7EEC90, "060 rj_7EEC90" },
		{ 0x7EED20, (void *)rj_7EED20, "060 rj_7EED20" },
		{ 0x7EED80, (void *)rj_7EED80, "060 rj_7EED80" },
		{ 0x7EEEE0, (void *)rj_7EEEE0, "060 rj_7EEEE0" },
		{ 0x7EEFA0, (void *)rj_7EEFA0, "060 rj_7EEFA0" },
		{ 0x7EF020, (void *)rj_7EF020, "060 rj_7EF020" },
		{ 0x7EF060, (void *)rj_7EF060, "060 rj_7EF060" },
		{ 0x7F0090, (void *)rj_7F0090, "060 rj_7F0090" },
		{ 0x7F01A0, (void *)rj_7F01A0, "060 rj_7F01A0" },
		{ 0x7F0220, (void *)rj_7F0220, "060 rj_7F0220" },
		{ 0x7F0250, (void *)rj_7F0250, "060 rj_7F0250" },
		{ 0x7F02F0, (void *)rj_7F02F0, "060 rj_7F02F0" },
		{ 0x7F0370, (void *)rj_7F0370, "060 rj_7F0370" },
		{ 0x7F03F0, (void *)rj_7F03F0, "060 rj_7F03F0" },
		{ 0x7F0BD0, (void *)rj_7F0BD0, "060 rj_7F0BD0" },
		{ 0x7F0DF0, (void *)rj_7F0DF0, "060 rj_7F0DF0" },
		{ 0x7F18E0, (void *)rj_7F18E0, "060 rj_7F18E0" },
		{ 0x7F1970, (void *)rj_7F1970, "060 rj_7F1970" },
		{ 0x7F1A00, (void *)rj_7F1A00, "060 rj_7F1A00" },
		{ 0x7F1A90, (void *)rj_7F1A90, "060 rj_7F1A90" },
		{ 0x7F1AF0, (void *)rj_7F1AF0, "060 rj_7F1AF0" },
		{ 0x7F1B90, (void *)rj_7F1B90, "060 rj_7F1B90" },
		{ 0x7F1BD0, (void *)rj_7F1BD0, "060 rj_7F1BD0" },
		{ 0x7F1C10, (void *)rj_7F1C10, "060 rj_7F1C10" },
		{ 0x7F1C20, (void *)rj_7F1C20, "060 rj_7F1C20" },
		{ 0x7F1CD0, (void *)rj_7F1CD0, "060 rj_7F1CD0" },
		{ 0x7F1D20, (void *)rj_7F1D20, "060 rj_7F1D20" },
		{ 0x7F1E50, (void *)rj_7F1E50, "060 rj_7F1E50" },
		{ 0x7F1F30, (void *)rj_7F1F30, "060 rj_7F1F30" },
		{ 0x7F2600, (void *)rj_7F2600, "060 rj_7F2600" },
		{ 0x7F2620, (void *)rj_7F2620, "060 rj_7F2620" },
		{ 0x7F2690, (void *)rj_7F2690, "060 rj_7F2690" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag060_raijin_attack()
	{
		act::register_module(60);
		for (const act::raijin::ModPort *p = act::raijin::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(60, p->addr, p->port, p->name);
		for (const act::raijin::ModPort *p = act::raijin::PORTS; p->addr; p++)
			act::register_module_port(60, p->addr, p->port, p->name);
		// 30 fps layer: see mag060_raijin_attack_held.inc
		FX_HELD(register_mag060_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag060_raijin_attack_held.inc"
#endif
