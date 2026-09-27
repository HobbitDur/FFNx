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

// Effect 24: Esuna (spell, MAG_024_*): a copy of the actor/heal effect library (see act_engine.h),
// built like Drain (039) without its screen flashes.
//
// Setup MAG_024_ESUNA_Init 0x88CF70 (runs once, not ported; file loader 0x88CF50 = the effect's
// data file [0x2700470], its two packet arenas at file + 0 / + 0xA000 / + 0x14000 by tick parity,
// the file arena cursor 0x27011AC = file + 0x14000; camera animation 0x161374C) creates the root
// queue 0x2700560 with the master and three task pools: 0x2701010 (4 x 0x58: emitters), 0x2700D70
// (3 x 0x2A4: actor systems), 0x2701000 (10 x 0x40: stage wobble).
//   Master (0x88D0A0) - camera copy 0x2793E58 (the module scratch stack 0x27011A0 grows down from
//     it), packet arena by tick parity (cursor 0x2700474), bone follow, 11-state table: 0x88D200
//     reserves the actor pools after the file arena (0x10E0 + 0x10680 bytes, cleared by 0x88D250)
//     and starts the stage wobble in (0x88D290: amplitude 0x1D98992 + k * 0x2C up to 0x400),
//     0x88D380 one emitter per action (it holds the master: root +0x63; loop 0x892E60 once the
//     emitter releases it), 0x892EA0 after 20 more ticks the wobble out (0x892ED0), then waits for
//     the queues to empty (0x892FA0).
//   Emitter (0x88D3B0, IDA "Overlay"), one per action: bone follow + model bounds (CURE_Emitter
//     helpers), sound 0x1613748 at its first tick; state 0 spawns the actor system (0x88D440 ->
//     0x88D470: task 0x88D4C0 with the actor data 0x17E48E4, start tick 0, run length 0x2D ticks,
//     mode 0), state 1 releases the master at tick 20, state 2 applies the action result (heal /
//     status cure) from tick 30. It draws nothing.
//   Actor system (0x88D4C0, IDA "Emitter" -> 0x88D520 / 0x892DC0): the engine's particle actors
//     (sprite sequences 0x890360 and prim models 0x88E090 drawn into the module arena through the
//     module's primitive-list renderers 0x88E240..) around the targets (the actor data's key
//     positions); its kind-5 models morph their vertex blocks in the actor data (0x891450).
//   Stage wobble out (0x892ED0, IDA "Glint"): amplitude 0x400 down by 0x100 per tick to 0.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2700470..0x27011B4 (file pointer 0x2700470, packet cursor 0x2700474, pools,
// queues, arenas, pool cursors / counters, scratch stack pointer 0x27011A0, actor state 0x27011B0),
// the module scratch stack below the camera copy 0x2793E58, the stage wobble words 0x1D98992 +
// k * 0x2C, the actor data 0x17E48E4 (relocated once) and its morph vertex blocks (exe data).
// Not ported: 0x88F750 (the primitive-list kind 6 renderer, the Siren 0x73DCA0 copy): Esuna's
// models have no kind-6 list, 0x88E240 calls it through its original address.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace esuna
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_024 = { "esuna", 24, 0x88CF50, 0x893000,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2700478, 0x2701188, 0x2701198, 0x27011A4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2700494, 0x0, 0x2700570, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2700D70, 0x0, 0x2701010, 0x0, 0x0, 0x0, 0x0, 0x0, 0x270118A, 0x270118C, 0x0, 0x270119A, 0x0, 0x27011A0, 0x0, 0x27011B0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x88D550, 0x88DFA0, 0x88E030, 0x88E090, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8902A0, 0x8903E0, 0x890420, 0x8916B0, 0x891700, 0x891730, 0x8917B0, 0x891EA0, 0x891EF0, 0x891F90, 0x892050, 0x8928A0, 0x892CA0, 0x892D30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x88D3B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x88D700, 0x88D7E0, 0x88D940, 0x88DA10, 0x88DB30, 0x88DD40, 0x890360, 0x8904B0, 0x890B10, 0x890B60, 0x890BF0, 0x890CC0, 0x891450, 0x891560, 0x8915E0, 0x891610, 0x891E30, 0x891E60, 0x891F40, 0x892060, 0x8921B0, 0x892840, 0x892870, 0x892A50, 0x892B90, 0x0, 0x88DFA0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2700474;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x27011B0;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x27011A0;      // module scratch stack pointer
	static const uint32_t Q_EMITTER = 0x2701010, Q_ACTORS = 0x2700D70, Q_STAGE = 0x2701000;
	static const uint32_t ORIG_Master = 0x88D0A0;
	static const uint32_t ORIG_Emitter = 0x88D3B0;
	static const uint32_t ORIG_ActorSystem = 0x88D4C0;
	static const void *const SOUND_Esuna = (const void *)0x1613748;

	// engine functions not in act::x
	namespace rx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
	}

	// helpers of the library code (as in mag039_drain.cpp / mag014_death.cpp / mag095_siren.cpp)
	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}
	// screen-space clip tests of the flat-triangle list (x 0..0xA00, y 0..0x6C0)
	static inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
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
	// GP0 0xE2 texture-window word built from a RECT {x,y,w,h} (u16 each) at ctx+b
	// (0x88EB32-0x88EB71 / 0x88EBD4-0x88EC13; same sequence at 0x88F09A / 0x88F146)
	static uint32_t s2_twin(uint32_t ctx, int32_t b)
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
	// PSX texture-window word (GP0 0xE2) built from a {u8 x, pad, u8 y, pad, u16 w, u16 h}
	// block at ctx+o, exactly as 0x8900D5..0x890114 / 0x89019F..0x8901DE compute it
	static uint32_t s3_texwin(uint32_t ctx, int32_t o)
	{
		uint32_t c = ((uint32_t)U8(ctx, o + 2) & 0xF8) | 0xFFFE2000u;
		c = (c << 5) | ((uint32_t)U8(ctx, o) & 0xF8);
		c = (c << 5) | (~((uint32_t)U16(ctx, o + 6) - 1u) & 0xF8);
		c = (c << 2) | ((~((uint32_t)U16(ctx, o + 4) - 1u) >> 3) & 0x1F);
		return c;
	}

	// 0x88FF50..0x88FFBB / 0x88FFC7..0x890032: add d to the 4 texcoord bytes pkt+o, +o+0xC,
	// +o+0x18, +o+0x24 (u or v of the 4 corners); when any sum exceeds 0xFF all four are
	// wrapped back by the window size byte at ctx+wo
	static void s3_scroll_uv(uint32_t pkt, int32_t o, uint32_t d, uint32_t ctx, int32_t wo)
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

	static inline bool s3_off(int16_t v, int16_t lim) { return v < 0 || v > lim; }

	// one iteration of the 0x88FCBF loop: projects one quad record (0x24 bytes at data) into the
	// POLY_GT4 at pkt; returns the packet cursor after what it emitted (pkt itself when culled)
	static uint32_t s3_quad(uint32_t ctx, uint32_t data, uint32_t vbase, uint32_t ot_base, uint32_t shift, uint32_t pkt)
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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl e_88D0A0(uint32_t a1);
	uint32_t __cdecl e_88D200(uint32_t a1);
	uint32_t __cdecl e_88D3B0(uint32_t a1);
	uint32_t __cdecl e_892DF0(uint32_t a1);
	uint32_t __cdecl e_892E60(uint32_t a1);
	uint32_t __cdecl e_88E090(uint32_t a1, uint32_t a2);
	uint32_t __cdecl e_88D330(uint32_t a1);
	uint32_t __cdecl e_88D440(uint32_t a1);
	uint32_t __cdecl e_88D520(uint32_t a1);
	uint32_t __cdecl e_88D550(uint32_t a1);
	uint32_t __cdecl e_88D700(uint32_t a1);
	uint32_t __cdecl e_88D7E0(void);
	uint32_t __cdecl e_88D940(void);
	uint32_t __cdecl e_88DB30(void);
	uint32_t __cdecl e_88DFA0(uint32_t a1);
	uint32_t __cdecl e_88E030(void);
	uint32_t __cdecl e_88E240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_88E370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_88E590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_88E820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_88ECA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_88FC50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl e_8902A0(void);
	uint32_t __cdecl e_890360(uint32_t a1);
	uint32_t __cdecl e_891450(uint32_t a1, uint32_t a2);
	uint32_t __cdecl e_8921B0(uint32_t a1);
	uint32_t __cdecl e_892DC0(uint32_t a1);
	uint32_t __cdecl e_892E10(uint32_t a1);
	uint32_t __cdecl e_892EA0(uint32_t a1);
	uint32_t __cdecl e_892F30(uint32_t a1);
	uint32_t __cdecl e_892F50(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag024_esuna_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace esuna
{
	// ====================================================================================
	// master, emitter, stage wobble (module code)
	// ====================================================================================

	// 0x88D0A0 (MAG_024_ESUNA_Tick): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the three queues (live task count -> node +0x5E; the actor counters
	// 0x270119A / 0x270118A are cleared before the queues run)
	uint32_t __cdecl e_88D0A0(uint32_t a1)
	{
		g_mod = &MOD_024;
		// 30 fps layer: see mag024_esuna_held.inc
		FX_HELD(held_note_master();)
		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(SCRATCH_SP) = 0x2793E58;
		MEM<uint32_t>(0x2700494) = 0x2793E58;
		states[0] = 0x88D1E0;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x88D1F0;
		states[2] = 0x88D200;
		states[3] = 0x88D380;
		states[4] = 0x892E60;
		states[5] = 0x892EA0;
		states[6] = 0x892FA0;
		states[7] = 0x892FB0;
		states[8] = 0x892FC0;
		states[9] = 0x892FD0;
		states[10] = 0x892FF0; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x2701184);
			const uint32_t v2 = MEM<uint32_t>(0x270057C);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2700490) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x2701180);
			const uint32_t v2 = MEM<uint32_t>(0x2700578);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2700490) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		MEM<uint16_t>(0x270119A) = 0;
		MEM<uint16_t>(0x270118A) = 0;
		static const uint32_t queues[3] = { Q_EMITTER, Q_ACTORS, Q_STAGE };
		for (int i = 0; i < 3; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x88D200 (MAG_024_sub_88D200): master state - once no child is alive: the two actor pools
	// are carved from the file arena cursor [0x27011AC] (0x10E0 + 0x10680 bytes) and cleared
	// (0x88D250), the stage wobble task 0x88D290 (0x40 bytes, stage queue) starts, next state
	uint32_t __cdecl e_88D200(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x27011AC);
		MEM<uint32_t>(0x270118C) = p;
		p += 0x10E0;
		MEM<uint32_t>(0x27011A4) = p;
		p += 0x10680;
		MEM<uint32_t>(0x27011AC) = p;
		a_732160();
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x88D290, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x88D3B0 (MAG_024_ESUNA_Overlay_Tick): EMITTER task (one per action) - bone-follow anchor and
	// model bounds, state {0x88D440 actor system, 0x892DF0 release the master, 0x892E10 heal, ret},
	// sound at its first tick
	uint32_t __cdecl e_88D3B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x88D440;
		states[1] = 0x892DF0;
		states[2] = 0x892E10;
		states[3] = 0x892E50; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Esuna), 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x892DF0 (MAG_024_sub_892DF0): emitter state 1 - at tick 20 releases the master (root +0x63 =
	// 0: its action loop may spawn the next emitter), next state
	uint32_t __cdecl e_892DF0(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x14)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x892E60 (MAG_024_sub_892E60): master state - action loop: while the master is not held
	// (+0x63) and actions remain (+0x2A < +0x58), the next action (+0x2A / +0x2E up, state back
	// to the emitter spawn); after the last one waits 20 ticks (+0x60) and goes on
	uint32_t __cdecl e_892E60(uint32_t a1)
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
		U16(node, 0x60) = 0x14;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return a1; // void
	}

	// ====================================================================================
	// actor system: prim-model actor draw (module code)
	// ====================================================================================

	// 0x88E090 (sub_88E090): draw of a prim-model actor a1 (definition a2): a 0x68-byte render
	// context on the module scratch stack (model +0x170, flags +0x14: 0x30 unless definition
	// +0x3A, fade +0x1CE -> far colour +0x16C / level, flags | 0xC0), texture windows +0x1C / +0x24
	// (256 x 256; the actors of definition index 1 and 2 get a 128 x 128 window B and a V scroll
	// +0x1A that advances by 2 (7-bit wrap) at every such draw: actor state +0x2A), drawn through
	// the primitive-list dispatcher 0x88E240 into the module arena (OT base + 0x44, depth shift 2):
	// once, or once per sub-position +0x19C.. (+0x1D8 copies)
	uint32_t __cdecl e_88E090(uint32_t a1, uint32_t a2)
	{
		const uint32_t act = a1;
		const uint32_t ctx = MEM<uint32_t>(SCRATCH_SP) - 0x68;
		MEM<uint32_t>(SCRATCH_SP) = ctx;
		U32(ctx, 0) = U32(act, 0x170);
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
		if (def_idx == 1 || def_idx == 2)
		{
			const uint32_t st = MEM<uint32_t>(ACTOR_STATE);
			U16(ctx, 0x28) = 0x80;
			U16(ctx, 0x2A) = 0x80;
			const uint16_t scroll = (uint16_t)((uint8_t)(U8(st, 0x2A) + 2) & 0x7F);
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
			MEM<uint32_t>(PACKET_CURSOR) = e_88E240(ctx, ot, 2, cursor);
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
				const uint32_t r = e_88E240(ctx, ot, 2, cursor);
				i++;
				pos += 8;
				MEM<uint32_t>(PACKET_CURSOR) = r;
			} while (i < S8(act, 0x1D8));
		}
		const uint32_t top = MEM<uint32_t>(SCRATCH_SP) + 0x68;
		MEM<uint32_t>(SCRATCH_SP) = top;
		return top;
	}

	// ====================================================================================
	// the library functions of this module whose code matches an engine / module port up to the
	// module's addresses and callees (the port's text with the module's addresses)
	// ====================================================================================

	// 0x88D330 (MAG_024_sub_88D330; same code as Drain r_8517B0): stage wobble state 1 - amplitude
	// +0x1C up by 0x100 per tick to 0x400 (then finished, next state), written to the four stage wobble
	// words 0x1D98992 + k * 0x2C
	uint32_t __cdecl e_88D330(uint32_t a1)
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

	// 0x88D440 (MAG_024_sub_88D440; same code as Drain r_851A00): emitter state 0 - the actor system
	// (task 0x88D4C0, actor data 0x17E48E4, start tick +0x298 = 0, run length +0x29E = 0x2D ticks,
	// mode +0x29C = 0) through the engine spawner 0x88D470 (a_73C100), next state
	uint32_t __cdecl e_88D440(uint32_t a1)
	{
		a_73C100(a1, ORIG_ActorSystem, 0x17E48E4, 0, 0x2D, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x88D520 (sub_88D520; same code as a_7348A0 / Drain r_851AE0): actor system state 0 - once the
	// frame counter +0x24 reaches the start tick +0x298: director set-up (0x88D550) and the first step
	// of the actor system (0x88DFA0), next state
	uint32_t __cdecl e_88D520(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			e_88D550(a1);
			e_88DFA0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x88D550 (MAG_024_ESUNA_Emitter_SetupWorkspace; same code as Drain r_851B10, the a_73F990 copy with
	// the module's own callees): actor director set-up for node a1: [0x27011B0] = state block node+0x34;
	// copies node+0x29A/0x29C(mode)/0x29E into it, points its 4 tables (+0x224..+0x230) into the loaded
	// actor data (node+0x30; relocated once by 0x88DD40, flag data+0), fills the target slot list (+4..,
	// count = root +0x5A) from the cast context's action record, sets the caster / per-target reference
	// points (0x88D7E0, 0x88DB30; modes 1/3/4: 0x88D940 / 0x88DA10 / 0x88D700) and stores the
	// caster-to-first-target distance in state+0.
	uint32_t __cdecl e_88D550(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(ACTOR_STATE) = st;
		U16(st, 0x22) = w29a;
		uint16_t caster = (uint16_t)U8(node, 0x2C);   // bp (movzx)
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		uint32_t relocated = U32(data, 0);
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(ACTOR_STATE);
			U32(data, 0) = 1;
		}
		uint16_t ntargets = U16(root, 0x5A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = ntargets;
		if ((int16_t)ntargets > 0)
		{
			int32_t i = 0;
			do
			{
				int32_t action = S8(node, 0x2A);
				uint32_t tab = U32(U32(node, 0x0C), 4);
				uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
				U32(st, 4 + 4 * i) = U8(rec, 0x18 * i);   // target slot
				i++;
			} while (i < S16(st, 0x1C));
		}
		U16(st, 0x1A) = U16(node, 0x29E);
		e_88D7E0();
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 4)
			callp(0x88D700, node);
		e_88DB30();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(ACTOR_STATE);
		if (U16(st, 0x24) == 1)
		{
			callp(0x88D940, node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(ACTOR_STATE);
		}
		// distance caster entity -> first target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		int32_t s1 = S16(st, 0x1E);
		uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		uint32_t d10 = U32(e1, 0x1C);
		uint32_t d14 = U32(e1, 0x20);
		uint32_t s2 = U32(st, 4);
		uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		uint32_t d18 = U32(e2, 0x1C);
		int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(ACTOR_STATE);
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x88D700 (sub_88D700; same code as Boko b_72AD60, mode 4): sets the state [0x27011B0] +0x1B4
	// vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that 16-byte vector to the
	// reference points +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (Esuna's data does not use mode 4)
	uint32_t __cdecl e_88D700(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(ACTOR_STATE);
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

	// 0x88D7E0 (sub_88D7E0; same code as Drain r_851DA0): the actor system's reference points from the
	// caster (slot state +0x1E): +0x1C4 its effect bone 0xF1 (y = its height word +0x24), +0x1D4 the same
	// on the ground (y 0), +0x1E4 bone 0xF1 again, +0x1F4 bone 0xF0, +0x204 x / z with y = its +0x3C
	// (16.16 each); mode 2 (+0x24) takes no bone position (the stack words keep what they held)
	uint32_t __cdecl e_88D7E0(void)
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

	// 0x88D940 (sub_88D940; same code as Boko b_72AFE0, mode 1): for i < count (state [0x27011B0] +0x1C,
	// re-read each pass) copies the 16-byte vector state +0x194 into state +0x54/+0x94/+0xD4/+0x114/
	// +0x154 + 0x10*i (Esuna's data does not use mode 1)
	uint32_t __cdecl e_88D940(void)
	{
		const uint32_t arena = MEM<uint32_t>(ACTOR_STATE);
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

	// 0x88DB30 (sub_88DB30; same code as Drain r_8520F0): per target of the actor system (slots state
	// +4.., count +0x1C) the target reference points +0x54 (bone 0xF1, y = height word +0x24), +0x94 (its
	// ground point), +0xD4 (bone 0xF1), +0x114 (bone 0xF0), +0x154 (x / z, y = +0x3C), 16.16, stride
	// 0x10; then the centre of the targets' x / z range at +0x194 (y 0). Mode 2 (+0x24) takes no bone
	// position (the stack words keep what they held)
	uint32_t __cdecl e_88DB30(void)
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

	// 0x88DFA0 (Effect_ParticleEmitter_RunAndCheckDone; same code as a_7353B0 / Drain r_852560): one step
	// of the actor system at node+0x34 (made current in [0x27011B0]): phase +0x20: 0 -> 1; 1 -> updates
	// the actors (0x892D30, 0x8903E0) and draws them (sprite sequences 0x8902A0, models 0x88E030),
	// counts +0x18 up to +0x1A and moves to phase 2 when nothing is pending (+0x14/+0x16 == 0); 2 ->
	// returns 1 (done). Always adds +0x14/+0x16 to the module counters 0x270119A / 0x270118A
	uint32_t __cdecl e_88DFA0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		MEM<uint32_t>(ACTOR_STATE) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			a_743190();
			a_736C00();
			// 30 fps layer: see mag024_esuna_held.inc
			FX_HELD(held_note_system(sys);)
			e_8902A0();
			e_88E030();
			sys = MEM<uint32_t>(ACTOR_STATE);
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
		MEM<uint16_t>(0x270119A) = (uint16_t)(MEM<uint16_t>(0x270119A) + c14);
		MEM<uint16_t>(0x270118A) = (uint16_t)(MEM<uint16_t>(0x270118A) + c16);
		return done;
	}

	// 0x88E030 (sub_88E030; same code as a_735440 / Drain r_8525F0): draw pass of the actor system's
	// models: for every actor of the list (state +0x2C, next +4) of type 1 whose definition (state +0x224
	// table, index +0x1D6) has kind 4 or 5 and which has a model (+0x170), draws it (0x88E090)
	uint32_t __cdecl e_88E030(void)
	{
		uint32_t act = U32(MEM<uint32_t>(ACTOR_STATE), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = MEM<uint32_t>(ACTOR_STATE);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					e_88E090(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x88E240 (sub_88E240; same code as Siren s_73C790): primitive-list dispatcher of render context
	// a1: resets the frame vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C),
	// calls someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (flat triangles 0x88E370, flat quads 0x88E590, textured
	// triangles 0x88E820, textured quads 0x88ECA0, 0x88F220 (a_73D770), 0x88F470 (a_73D9C0), 0x88F750
	// (not ported: no such list in Esuna's models), 0x88FC50) into OT a2 (depth shift a3) or skips the
	// empty count; returns the new packet cursor (starting from a4).
	uint32_t __cdecl e_88E240(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = e_88E370( ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = e_88E590( ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = e_88E820(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = e_88ECA0(ctx, a2, a3, cur);
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
			cur = callo(0x88F750, ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return e_88FC50(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x88E370 (sub_88E370; same code as Tonberry t_764670): prim-list block 'flat triangles' (12-byte
	// records: code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-
	// transparency from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip
	// rejects, optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >>
	// a3, InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl e_88E370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x88E590 (sub_88E590; same code as Tonberry t_764890): draws the flat-quad list of render context
	// a1 (count, then records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE
	// FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40),
	// emits one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl e_88E590(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x88E820 (sub_88E820; same code as Siren s_73CD70): draws the textured-triangle list of render
	// context a1 (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut,
	// uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl e_88E820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x88EA8F)
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

	// 0x88ECA0 (sub_88ECA0; same code as Siren s_73D1F0): draws the textured-quad list of render
	// context a1 (count, then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut,
	// uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl e_88ECA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x88EFB8)
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

	// 0x88FC50 (sub_88FC50; same code as Siren s_73E1A0): quad-list emitter: for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls (GTE
	// FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with optional
	// depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll (ctx+0x18/0x1A)
	// the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl e_88FC50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t data = U32(ctx, 0x2C);
		int32_t count = S32(data, 0);
		data += 4;
		// quirk 0x88FC72: the vertex base overwrites the caller's a4 argument slot and 0x88FD49
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

	// 0x8902A0 (sub_8902A0; same code as Drain r_853C70, the a_740700 copy): draws the sprite sequences
	// of the director's actor list (state +0x2C, next +4): every actor of kind +8 == 1 whose definition
	// kind (+0x16) is 0/1/2 and that has a sequence (+0x170): once (+0x1D8 == 1) or once per
	// sub-position (s16 x/y/z at +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x890360.
	uint32_t __cdecl e_8902A0(void)
	{
		uint32_t act = U32(MEM<uint32_t>(ACTOR_STATE), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(ACTOR_STATE);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						e_890360(act);   // (the original also pushes the bone entry, unused)
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
							e_890360(act);
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

	// 0x890360 (sub_890360; same code as Drain r_853D30, the a_7407C0 copy): sprite sequence of an actor
	// (header on the module scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawn into the
	// module arena (OT base + 0x44, depth shift 2)
	uint32_t __cdecl e_890360(uint32_t a1)
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

	// 0x891450 (sub_891450; same code as Boko b_72E220): vertex morph of a model for frame S16(a1+0x1CA)
	// of actor data a2: destination vertex block = state +0x22C table [a2+0x12C[frame]], sources =
	// state+0x230 table [a2+0x130[frame]] and [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 =
	// second source); writes dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 /
	// 0x45EBF0), using an 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl e_891450(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = MEM<uint32_t>(ACTOR_STATE);                  // actor state block
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
				rx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				rx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				rx::sub_45E0B0(in2);
				rx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x8921B0 (sub_8921B0; same code as Boko b_72EF80): actor position for its current key a1+0x60:
	// actor data = state+0x224[S8 a1+0x6A]; start point A = reference point (kind data+0x100[key]: 1 =
	// state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's target slot S8 a1+0x6B points
	// +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the offset (data+0x110/0x114/0x118) rotated
	// by the actor matrix a1+0x2C; end point B likewise (kind +0x104, index +0x10C, offset
	// +0x11C/0x120/0x124; unknown kind = the rotated first offset); a1+0x4C/0x50/0x54 = (A * 4096 +
	// (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl e_8921B0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = MEM<uint32_t>(ACTOR_STATE);            // ecx
		int32_t aidx = S8(act, 0x6A);
		int32_t key = S16(act, 0x60);                          // ebx
		const uint32_t data = U32(U32(st, 0x224), aidx * 4);   // edi / [esp+0x10]
		uint8_t sel = U8(U32(data, 0x108), key);               // bp (movzx)
		uint8_t kind = U8(U32(data, 0x100), key);
		// UNINIT 0x8923FE: the start point is a stack SVECTOR ([esp+0x14] x, [esp+0x16] y,
		// [esp+0x18] z) that only the switch cases fill; an unknown kind / index > 3 leaves it
		// uninitialised (0 here)
		int16_t ax = 0, ay = 0, az = 0;
		uint32_t base = 0;
		bool have = false;
		if (kind == 0)
		{
			if (sel <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // (switch case)
				base = st + (uint32_t)((int32_t)S8(act, 0x6B) << 4) + offs0[sel];
				have = true;
			}
		}
		else if (kind == 1)
		{
			if (sel <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // (switch case)
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
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // (switch case)
				uint32_t g = MEM<uint32_t>(ACTOR_STATE);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // (switch case)
				uint32_t g = MEM<uint32_t>(ACTOR_STATE);
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
			// quirk 0x89271A: an unknown end kind / index > 3 reuses the rotated first offset (the IR
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

	// 0x892DC0 (MAG_024_ESUNA_Emitter_State1_RunUntilDone; same code as a_7395E0 / Drain r_856790):
	// actor system state 1: steps the actor system (0x88DFA0); when it reports done, sets the finished
	// flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl e_892DC0(uint32_t a1)
	{
		if (e_88DFA0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x892E10 (MAG_024_sub_892E10; same code as Drain r_8567E0): emitter state 2 - from tick 30: the
	// action result (heal / status cure) of its target, finished, next state
	uint32_t __cdecl e_892E10(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x1E)
			return 0; // void
		const int32_t action = S8(node, 0x2A);
		const int32_t target = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t targets = U32(acts + (uint32_t)(action * 20), 8);
		x::ApplyActionResultToTarget(targets + (uint32_t)(target * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x892EA0 (MAG_024_ESUNA_State5_SpawnGlintsAfterDelay; same code as Drain r_856870): master state -
	// counts +0x60 down; at 0 starts the stage wobble out (0x892ED0, 0x40 bytes, stage queue), next state
	uint32_t __cdecl e_892EA0(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x892ED0, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x892F30 (MAG_024_sub_892F30; same code as Drain r_856900): stage wobble out state 0 - amplitude
	// +0x1C = 0x400, next state
	uint32_t __cdecl e_892F30(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x892F50 (MAG_024_sub_892F50; same code as MiniMog m_733C90): stage wobble out state 1 - amplitude
	// +0x1C down by 0x100 per tick to 0 (then finished, next state), written to the four stage wobble
	// words 0x1D98992 + k * 0x2C
	uint32_t __cdecl e_892F50(uint32_t a1)
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
		{ 0x88D1E0, (void *)a_73A0D0, "024 MAG_024_sub_88D1E0" },
		{ 0x88D1F0, (void *)a_73A0D0, "024 MAG_024_sub_88D1F0" },
		{ 0x88D250, (void *)a_732160, "024 MAG_024_sub_88D250" },
		{ 0x88D290, (void *)a_73A380, "024 MAG_024_sub_88D290" },
		{ 0x88D2F0, (void *)a_7335D0, "024 MAG_024_sub_88D2F0" },
		{ 0x88D370, (void *)a_73A6D0, "024 nullsub_1728" },
		{ 0x88D380, (void *)a_73A170, "024 MAG_024_sub_88D380" },
		{ 0x88D470, (void *)a_73C100, "024 MAG_024_sub_88D470" },
		{ 0x88D4C0, (void *)a_73A380, "024 MAG_024_ESUNA_Emitter_Tick" },
		{ 0x88DA10, (void *)a_73FE90, "024 sub_88DA10" },
		{ 0x88DD40, (void *)a_740210, "024 Effect_ParticleEmitter_RelocateDescriptor" },
		{ 0x88F220, (void *)a_73D770, "024 sub_88F220" },
		{ 0x88F470, (void *)a_73D9C0, "024 sub_88F470" },
		{ 0x8903E0, (void *)a_736C00, "024 sub_8903E0" },
		{ 0x890420, (void *)a_740880, "024 sub_890420" },
		{ 0x8904B0, (void *)a_740910, "024 sub_8904B0" },
		{ 0x890B10, (void *)a_740F70, "024 sub_890B10" },
		{ 0x890B60, (void *)a_740FC0, "024 sub_890B60" },
		{ 0x890BF0, (void *)a_741050, "024 sub_890BF0" },
		{ 0x890CC0, (void *)a_741120, "024 sub_890CC0" },
		{ 0x891560, (void *)a_7419C0, "024 sub_891560" },
		{ 0x8915E0, (void *)a_741A40, "024 sub_8915E0" },
		{ 0x891610, (void *)a_741A70, "024 sub_891610" },
		{ 0x8916B0, (void *)a_737ED0, "024 sub_8916B0" },
		{ 0x891700, (void *)a_741B60, "024 sub_891700" },
		{ 0x891730, (void *)a_737F50, "024 sub_891730" },
		{ 0x8917B0, (void *)a_737FD0, "024 sub_8917B0" },
		{ 0x891E30, (void *)a_742290, "024 au_re__rand_61" },
		{ 0x891E60, (void *)a_7422C0, "024 sub_891E60" },
		{ 0x891EA0, (void *)a_742300, "024 sub_891EA0" },
		{ 0x891EF0, (void *)a_742350, "024 sub_891EF0" },
		{ 0x891F40, (void *)a_7423A0, "024 au_re__rand_61_0" },
		{ 0x891F90, (void *)a_7387B0, "024 sub_891F90" },
		{ 0x892050, (void *)a_7424B0, "024 sub_892050" },
		{ 0x892060, (void *)a_7424C0, "024 sub_892060" },
		{ 0x892840, (void *)a_742CA0, "024 sub_892840" },
		{ 0x892870, (void *)a_742CD0, "024 sub_892870" },
		{ 0x8928A0, (void *)a_742D00, "024 sub_8928A0" },
		{ 0x892A50, (void *)a_742EB0, "024 sub_892A50" },
		{ 0x892B90, (void *)a_742FF0, "024 sub_892B90" },
		{ 0x892CA0, (void *)a_743100, "024 sub_892CA0" },
		{ 0x892D30, (void *)a_743190, "024 sub_892D30" },
		{ 0x892DE0, (void *)a_73A6D0, "024 nullsub_1729" },
		{ 0x892E50, (void *)a_73A6D0, "024 nullsub_1730" },
		{ 0x892ED0, (void *)a_73A380, "024 MAG_024_ESUNA_Glint_Tick" },
		{ 0x892F90, (void *)a_73A6D0, "024 nullsub_1732" },
		{ 0x892FA0, (void *)a_747550, "024 MAG_024_ESUNA_State6_WaitSubtasksDone" },
		{ 0x892FB0, (void *)a_73A0D0, "024 MAG_024_sub_892FB0" },
		{ 0x892FC0, (void *)a_73A0D0, "024 MAG_024_sub_892FC0" },
		{ 0x892FD0, (void *)a_7475A0, "024 MAG_024_sub_892FD0" },
		{ 0x892FF0, (void *)a_73A6D0, "024 nullsub_1731" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x88D0A0, (void *)e_88D0A0, "024 e_88D0A0" },
		{ 0x88D200, (void *)e_88D200, "024 e_88D200" },
		{ 0x88D3B0, (void *)e_88D3B0, "024 e_88D3B0" },
		{ 0x892DF0, (void *)e_892DF0, "024 e_892DF0" },
		{ 0x892E60, (void *)e_892E60, "024 e_892E60" },
		{ 0x88E090, (void *)e_88E090, "024 e_88E090" },
		{ 0x88D330, (void *)e_88D330, "024 e_88D330" },
		{ 0x88D440, (void *)e_88D440, "024 e_88D440" },
		{ 0x88D520, (void *)e_88D520, "024 e_88D520" },
		{ 0x88D550, (void *)e_88D550, "024 e_88D550" },
		{ 0x88D700, (void *)e_88D700, "024 e_88D700" },
		{ 0x88D7E0, (void *)e_88D7E0, "024 e_88D7E0" },
		{ 0x88D940, (void *)e_88D940, "024 e_88D940" },
		{ 0x88DB30, (void *)e_88DB30, "024 e_88DB30" },
		{ 0x88DFA0, (void *)e_88DFA0, "024 e_88DFA0" },
		{ 0x88E030, (void *)e_88E030, "024 e_88E030" },
		{ 0x88E240, (void *)e_88E240, "024 e_88E240" },
		{ 0x88E370, (void *)e_88E370, "024 e_88E370" },
		{ 0x88E590, (void *)e_88E590, "024 e_88E590" },
		{ 0x88E820, (void *)e_88E820, "024 e_88E820" },
		{ 0x88ECA0, (void *)e_88ECA0, "024 e_88ECA0" },
		{ 0x88FC50, (void *)e_88FC50, "024 e_88FC50" },
		{ 0x8902A0, (void *)e_8902A0, "024 e_8902A0" },
		{ 0x890360, (void *)e_890360, "024 e_890360" },
		{ 0x891450, (void *)e_891450, "024 e_891450" },
		{ 0x8921B0, (void *)e_8921B0, "024 e_8921B0" },
		{ 0x892DC0, (void *)e_892DC0, "024 e_892DC0" },
		{ 0x892E10, (void *)e_892E10, "024 e_892E10" },
		{ 0x892EA0, (void *)e_892EA0, "024 e_892EA0" },
		{ 0x892F30, (void *)e_892F30, "024 e_892F30" },
		{ 0x892F50, (void *)e_892F50, "024 e_892F50" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag024_esuna()
	{
		act::register_module(24);
		for (const act::esuna::ModPort *p = act::esuna::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(24, p->addr, p->port, p->name);
		for (const act::esuna::ModPort *p = act::esuna::PORTS; p->addr; p++)
			act::register_module_port(24, p->addr, p->port, p->name);
		// 30 fps layer: see mag024_esuna_held.inc
		FX_HELD(register_mag024_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag024_esuna_held.inc"
#endif
