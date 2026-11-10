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

// Effect 9: Doom Activation (MAG_009_DOOM_ACTIVATION*; no kernel.bin magic / enemy attack and no monster uses it: the battle
// plays it when a Doom countdown reaches 0 - computeTimerStatus 0x483470 queues AI section 5 (AI_SECTION_INDEX_DOOM_EXPIRY),
// MonsterAI_DispatchSection 0x4877F0 runs the system command 0xF6 (COMMAND_DOOM_FINISHED, ability 0x2B) on the doomed slot,
// computeCommandAction 0x48D200 case 0xF6 (0x48D78C) gives it the magic / effect id 9; caster = target = the doomed slot, party
// member or monster): a copy of the actor/heal effect library (see act_engine.h) built like Stare 45 (mag045_stare.cpp: master
// + director stepping phases, prim-model tasks drawn by the eight primitive-list renderers, the stage darkening) with Angelo
// Search's master (92) and sprite particles (Siren's sequence draw) and its own sinking target. Module 0x8C0360..0x8C4960 (87
// functions: 41 served by engine ports, 24 the relocated text of a ported library function (Angelo Search 92 / Angelo
// Reverse 94, Doom 8, Tonberry 90, Angelo Rush 91, Ultimecia 79 / 80, Boko 99, Siren 95, Angelo Cannon 86), 20 hand-ported,
// the loader and the setup not ported).
//
// Setup MAG_009_DOOM_ACTIVATION_Init 0x8C0380 (runs once, not ported; file loader MAG_009_DOOM_ACTIVATION_FL 0x8C0360 = the
// effect's data file 0x1634F10 -> 0x274EFE8, packet arenas at file + 0 / + 0x10000 / + 0x20000) plays the camera animation
// 0x1634CB0, uploads the file's TIM (both unless the cast context's flag +1 bit 0) and creates the root queue 0x274F220 with
// the master and seven task pools: 0x2757450 (4 x 0x58 emitters), 0x274F128 (3 x 0x48 director), 0x2758160 (1 x 0x140,
// unused), 0x274F230 (8 x 0x7C phase tasks), 0x274F628 (0x32 x 0x5C particles), 0x27571A0 (3 x 0x3E4 prim-model tasks) and
// 0x2757440 (10 x 0x40, the engine's screen tints).
//   Master (0x8C0500 = Angelo Search's master with this module's globals) - camera copy 0x2793E58, packet cursors 0x274EFEC /
//     0x274F14C by tick parity, bone follow, 11-state table: 0x8C06A0 one emitter per action, then the engine's end states.
//   Emitter (0x8C06D0 = the engine's a_73A1A0): state 0x8C0750 sets up the director block [0x1634B7C] (0x8C0780: the target's
//     position, the facing towards it) and spawns the director; 0x8C4800 applies the action result (the KO) once the director's
//     done flag +0x48 is 1.
//   Director (0x8C0800): phase bookkeeping 0x8C08A0 (a new phase spawns its tasks: 0x8C0AB0), 11 states: phase 1, frame 5,
//     phase 2, the sound 0x1634B80, frame 0x20, phase 3, wait for phase 3 / 4 (phase 4 is requested by the sinking task),
//     phase 5, finish.
//   Phase 1: the stage darkening in (0x8C32D0: the stage group words 0x1D98992 + k * 0x2C ramp to 0x800 by 0x180 and stay).
//   Phase 2: the doom prim model at the target's anchor 0xF1 (0x8C0BC0: layout 0x18155F0, played by 0x8C0C90 with the object
//     callback 0x8C0DC0 and the module renderer 0x8C1080 = Doom's eight primitive-list renderers, into the frame arena
//     0x1D8E054) and a screen tint (engine task 0x8DDC30, colour script 0x1634F00).
//   Phase 3: the target SINKS (0x8C3490): hidden from the battle's model pass (entity word +0 bit 3), drawn by the module
//     (0x8C3520 / 0x8C3620: render context 0x274EFF0, its shadow and both model parts as semi-transparent Gouraud polygons, a
//     grey level 0x40 that fades out across a band above the middle of the model) 0x28 lower every tick for 16 ticks, then
//     sinking on while the level falls by 0x10 a tick; at 0 the model is no longer drawn, the entity bit is put back and the
//     director's done flag set (the damage); the anchor 0x27571B8 sinks with it 10 ticks more. With it a prim model (0x8C3C30:
//     layout 0x1817EE8) follows the anchor and a spawner (0x8C3D40, count table 0x1634EF4) emits particles (0x8C3E00: sprite
//     sequence 0x1634B84 drawn by 0x8C3F00 = Siren's sprite draw, turning around the anchor at 0xC0 on random axes).
//   Phase 4: the stage darkening out (0x8C33C0: from its tick 0x19, 0x800 back to 0 by 0x100).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Writes outside the module: the target's entity word +0 bit 3, the stage group words 0x1D98992 + k * 0x2C, the camera copy
// 0x2793E58, action results (0x506690); the camera itself is the engine's camera animation (started by the setup).
// Module globals: 0x274EFE8..0x27584F8 (file pointer 0x274EFE8, packet cursors 0x274EFEC / 0x274F14C, render context
// 0x274EFF0, vertex scratch 0x274F638, pools, queues, arena pointers, sinking anchor 0x27571B8, blend buffer 0x2758178, render
// block 0x27584D0) + camera copy 0x2793E58.

#include "act_engine.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace act
{
namespace doomact
{
	// ---- generated (desc9.py): the module descriptor of this copy of the library ----
	static const Mod MOD_009 = { "doomactivation", 9, 0x8C0360, 0x8C4960,
		{ 0x0, 0x0, 0x1634B7C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x274EFE8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2757450, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C06D0, 0x8C0750, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C0910, 0x0, 0x8C09B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8C4160, 0x0, 0x8C4800, 0x8C4840, 0x8C4860, 0x8C4880, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x1634B7C;        // pointer to the director block (phase +0x40 / requested +0x42 / next +0x44, frame +0x46, done +0x48)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = 0x274EFEC;  // module packet cursor (tick parity: [0x27575C4] / [0x27575C0]; the sinking model's draws)
	static const uint32_t FRAME_CURSOR = 0x1D8E054;   // frame arena cursor (the prim-model and sprite draws)
	static const uint32_t SOUND = 0x1634B80;          // the director's sound (phase 2)
	static const uint32_t BLEND_BUF = 0x2758178;      // vertex blend buffer of the prim callback
	static const uint32_t PRIM_CB = 0x8C0DC0;         // the prim-model player's object callback
	static const uint32_t ANCHOR = 0x27571B8;         // the sinking anchor (x, y, z s16; the particles and the second prim model follow it)
	static const uint32_t BODY_CTX = 0x274EFF0;       // render context of the sinking target (0x5C bytes: +0x54 height, +0x56 level, +0x58 / +0x5A band)
	static const uint32_t BODY_VERTS = 0x274F638;     // its vertex scratch (12-byte records)
	static const uint32_t BODY_BLOCK = 0x27584D0;     // its block (+4 -> the vertex scratch)
	static const uint32_t Q_EMITTER = 0x2757450, Q_DIRECTOR = 0x274F128, Q_PHASE = 0x274F230, Q_PARTICLE = 0x274F628, Q_PRIM = 0x27571A0, Q_TINT = 0x2757440;
	static const uint32_t ORIG_Master = 0x8C0500;
	static const uint32_t ORIG_DarkenIn = 0x8C32D0;   // stage darkening in (engine task a_73A380)
	static const uint32_t ORIG_DoomModel = 0x8C0BC0;  // the doom prim model at the target (engine task a_73A380)
	static const uint32_t ORIG_Sink = 0x8C3490;       // the target's sinking
	static const uint32_t ORIG_DarkenOut = 0x8C33C0;  // stage darkening out (engine task a_73A380)
	static const uint32_t ORIG_SinkModel = 0x8C3C30;  // the prim model following the sinking anchor (engine task a_73A380)
	static const uint32_t ORIG_Spawner = 0x8C3D40;    // the particle spawner
	static const uint32_t ORIG_Particle = 0x8C3E00;   // a particle
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_LoadV012Dwords(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E020)(a1); } // sub_45E020: V0 / V1 / V2 = 6 dwords
		static inline uint32_t GTE_StoreSXY012_PolyGT3(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E300)(a1); }
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

	// the sinking states' step: the model's height offset (context +0x54) and the anchor's y += the speed +0x6E
	static inline void da_sink_step(uint32_t node)
	{
		const uint16_t v = U16(node, 0x6E);
		MEM<uint16_t>(BODY_CTX + 0x54) = (uint16_t)(MEM<uint16_t>(BODY_CTX + 0x54) + v);
		MEM<uint16_t>(ANCHOR + 2) = (uint16_t)(MEM<uint16_t>(ANCHOR + 2) + v);
	}
	// a particle's place (0x8C3E29..0x8C3EBA): the anchor + (0, 0, 0xC0) turned by Rz(+0x44) Rx(+0x40) Ry(+0x42)
	// (frame of 0x8C3E00: matrix at +0x18, in-vector +4 (its pad never written), out-vector +0xC)
	static inline void da_particle_place(uint32_t node)
	{
		alignas(4) uint8_t loc[0x38] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x18, vin = L + 4, vout = L + 0xC;
		x::MAG_022_sub_8DD770(M);
		x::sub_8DD960(M, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD7E0(M, (uint32_t)(int32_t)S16(node, 0x40));
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(node, 0x42));
		U16(vin, 0) = 0;
		U16(vin, 2) = 0;
		U16(vin, 4) = 0xC0;
		x::matrixMultiplyVector(M, vin, vout);
		U16(node, 0x1C) = (uint16_t)(MEM<uint16_t>(ANCHOR) + U16(vout, 0));
		U16(node, 0x1E) = (uint16_t)(MEM<uint16_t>(ANCHOR + 2) + U16(vout, 2));
		U16(node, 0x20) = (uint16_t)(MEM<uint16_t>(ANCHOR + 4) + U16(vout, 4));
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl da_8C0500(uint32_t a1);
	uint32_t __cdecl da_8C0690(uint32_t a1);
	uint32_t __cdecl da_8C0750(uint32_t a1);
	uint32_t __cdecl da_8C0780(uint32_t a1);
	uint32_t __cdecl da_8C0800(uint32_t a1);
	uint32_t __cdecl da_8C08A0(uint32_t a1);
	uint32_t __cdecl da_8C0940(uint32_t a1);
	uint32_t __cdecl da_8C0980(uint32_t a1);
	uint32_t __cdecl da_8C09D0(uint32_t a1);
	uint32_t __cdecl da_8C0A10(uint32_t a1);
	uint32_t __cdecl da_8C0AB0(uint32_t a1);
	uint32_t __cdecl da_8C0B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl da_8C0C20(uint32_t a1);
	uint32_t __cdecl da_8C0C90(uint32_t a1);
	uint32_t __cdecl da_8C0DC0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl da_8C1080(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C11B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C13D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C1660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C1B50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C2670(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C2BD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C32A0(uint32_t a1);
	uint32_t __cdecl da_8C3370(uint32_t a1);
	uint32_t __cdecl da_8C3420(uint32_t a1);
	uint32_t __cdecl da_8C3440(uint32_t a1);
	uint32_t __cdecl da_8C3490(uint32_t a1);
	uint32_t __cdecl da_8C3520(uint32_t a1);
	uint32_t __cdecl da_8C3620(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C3B30(uint32_t a1);
	uint32_t __cdecl da_8C3B80(uint32_t a1);
	uint32_t __cdecl da_8C3C90(uint32_t a1);
	uint32_t __cdecl da_8C3CF0(uint32_t a1);
	uint32_t __cdecl da_8C3D40(uint32_t a1);
	uint32_t __cdecl da_8C3D90(uint32_t a1);
	uint32_t __cdecl da_8C3E00(uint32_t a1);
	uint32_t __cdecl da_8C3F00(uint32_t a1);
	uint32_t __cdecl da_8C45D0(uint32_t a1);
	uint32_t __cdecl da_8C4620(uint32_t a1);
	uint32_t __cdecl da_8C4690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl da_8C46F0(uint32_t a1);
	uint32_t __cdecl da_8C4720(uint32_t a1);
	uint32_t __cdecl da_8C47A0(uint32_t a1);
	uint32_t __cdecl da_8C47D0(uint32_t a1);
	uint32_t __cdecl da_8C4800(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag009_doom_activation_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace doomact
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Angelo Search / Angelo Reverse / Doom / Tonberry / Angelo Rush / Ultimecia / Boko / Siren / Angelo Cannon port up to
	// the module addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x7552B0 (MAG_092_sub_7552B0): MASTER task - snapshots the camera matrix into 0x2793E58 (pointers 0x2598EF0 / 0x2594EA0),
	// picks the double-buffered packet arenas by tick parity (module cursor 0x2594D9C = [0x259831C] / [0x2598318], second cursor
	// 0x2594E9C = [0x2597F14] / [0x2597F10]), follows the target's bones, runs the 11-state master table {0x755420, 0x755430
	// (a_73A0D0), 0x755440 (wait for the children), 0x755450 (a_73A170: starts the emitter), the library's finish states
	// 0x759280.., ret}, then executes the seven module queues (live task count -> node +0x5E)
	uint32_t __cdecl da_8C0500(uint32_t a1)
	{
		g_mod = &MOD_009;
		// 30 fps layer: see mag009_doom_activation_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x27584C4) = 0x2793E58;
		MEM<uint32_t>(0x274F150) = 0x2793E58;
		states[0] = 0x8C0670;
		states[1] = 0x8C0680;
		states[2] = 0x8C0690;
		states[3] = 0x8C06A0;
		states[4] = 0x8C4890;
		states[5] = 0x8C48D0;
		states[6] = 0x8C48E0;
		states[7] = 0x8C48F0;
		states[8] = 0x8C4920;
		states[9] = 0x8C4930;
		states[10] = 0x8C4950; // nullsub (ret)
		if ((U8(node, 0x5C) & 1) != 0)
		{
			MEM<uint32_t>(0x274EFEC) = MEM<uint32_t>(0x27575C4);
			MEM<uint32_t>(0x274F14C) = MEM<uint32_t>(0x27571B4);
		}
		else
		{
			MEM<uint32_t>(0x274EFEC) = MEM<uint32_t>(0x27575C0);
			MEM<uint32_t>(0x274F14C) = MEM<uint32_t>(0x27571B0);
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		static const uint32_t queues[7] = { 0x2757450, 0x274F128, 0x2758160, 0x274F230, 0x274F628, 0x27571A0, 0x2757440 };
		for (int i = 0; i < 7; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag009_doom_activation_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl da_8C0690(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8218C0 (MAG_049_sub_8218C0; Psycho Blast 0x893450 = Granaldo 0x8A36A0): emitter state 0 - sets up the
	// director block (0x8218F0), spawns the director 0x821970 (0x48 bytes, director queue), next state
	uint32_t __cdecl da_8C0750(uint32_t a1)
	{
		da_8C0780(a1);
		x::Effect_AddTaskAndInitFromCtx(0x274F128, 0x8C0800, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8218F0 (MAG_049_sub_8218F0; Psycho Blast 0x893480): emitter a1 - clears the director block [0x15ADAF0] (0x54 bytes),
	// +8 / +0xC = the target's (+0x2D) position words, origin +0..+4 = 0, facing +0x4A = the angle of
	// the target seen from the origin - 0x800 (12 bits)
	uint32_t __cdecl da_8C0780(uint32_t a1)
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

	// 0x7B55B0 (MAG_079_sub_7B55B0): DIRECTOR task - cut bookkeeping (0x7B5650), then its 11-state
	// table {0x7B56A0 claim voice + music fade, 0x7B5710 cut 1: hide the monsters, 0x7B5790 load
	// step, 0x7B57F0 toggle at cut frame 0x62, 0x7B5880 frame 0x63, 0x7B58A0 cut 2, 0x7B58C0,
	// 0x7B58E0 damage cue at frame 0x5A, 0x7B5900 cut 3: show the monsters + release the voice,
	// 0x7B5970 finish, 0x7B5990 ret}
	uint32_t __cdecl da_8C0800(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x8C08F0;
		states[1] = 0x8C0940;
		states[2] = 0x8C0960;
		states[3] = 0x8C0980;
		states[4] = 0x8C09D0;
		states[5] = 0x8C09F0;
		states[6] = 0x8C0A10;
		states[7] = 0x8C0A30;
		states[8] = 0x8C0A50;
		states[9] = 0x8C0A70;
		states[10] = 0x8C0A90;
		da_8C08A0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl da_8C08A0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			da_8C0AB0(a1);
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

	// 0x8C0940 (sub_8C0940): director state 1 - when the phase frame +0x46 reaches 5: next state
	uint32_t __cdecl da_8C0940(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 5)
			next_state(a1);
		return 0; // void
	}

	// 0x8C0980 (au_re_BdLinkTask_0_6): director state 3 - once phase 2 runs (a_73B7E0): the sound 0x1634B80, next state
	uint32_t __cdecl da_8C0980(uint32_t a1)
	{
		if (a_73B7E0(2) == 0)
			return 0; // void
		x::BdPlaySE(SOUND, 0, 0x80);
		next_state(a1);
		return 0; // void
	}

	// 0x8C09D0 (sub_8C09D0): director state 4 - when the phase frame +0x46 reaches 0x20: next state
	uint32_t __cdecl da_8C09D0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x20)
			next_state(a1);
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73B7E0).
	uint32_t __cdecl da_8C0A10(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x8C0AB0 (sub_8C0AB0): phase start (from the bookkeeping 0x8C08A0), by the new phase (jump table 0x8C0B5C):
	// 1 the stage darkening in (0x8C32D0), 2 the doom prim model at the target (0x8C0BC0, layout 0x18155F0) and a
	// screen tint (engine task 0x8DDC30, colour script 0x1634F00, 0x11), 3 the target's sinking (0x8C3490), 4 the
	// stage darkening out (0x8C33C0); the phase tasks (0x7C bytes) go to the queue 0x274F230
	uint32_t __cdecl da_8C0AB0(uint32_t a1)
	{
		const int32_t ph = (int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x40) - 1;
		switch (ph)
		{
		case 0:
			return x::Effect_AddTaskAndInitFromCtx(Q_PHASE, ORIG_DarkenIn, 0x7C, a1);
		case 1:
		{
			da_8C0B70(a1, ORIG_DoomModel, 0x18155F0, 0x348, 0, 0);
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_TINT, 0x8DDC30, 0x40, a1);
			U32(t, 0x34) = 0x1634F00;
			U16(t, 0x38) = 0x11;
			return t;
		}
		case 2:
			return x::Effect_AddTaskAndInitFromCtx(Q_PHASE, ORIG_Sink, 0x7C, a1);
		case 3:
			return x::Effect_AddTaskAndInitFromCtx(Q_PHASE, ORIG_DarkenOut, 0x7C, a1);
		default:
			return (uint32_t)ph;
		}
	}

	// 0x8C0B70 (sub_8C0B70; Doom dm_8C5860 / Stare st_836D10 with this module's pool): spawns task a2 (0x3E4 bytes,
	// prim-model queue 0x27571A0, parent a1) and sets its model data +0x74 = a3, +0x78 = (s16)a4, +0x80 = a5, +0x82 = a6;
	// returns the node
	uint32_t __cdecl da_8C0B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x3E4, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x8C0C20 (sub_8C0C20; Stare st_8395F0 without its start tick): doom prim model state 0 - decodes the prim layout
	// (+0x74 -> +0x94, +0x78), position = the target's (+0x2D) anchor 0xF1 (0x502170), scale 1.0, depth offset +0x88 =
	// -0x200, plays (0x8C0C90), next state
	uint32_t __cdecl da_8C0C20(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ENT(U8(node, 0x2D)), 0xF1, 0, node + 0x1C);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFE00;
		da_8C0C90(node);
		next_state(node);
		return 0; // void
	}

	// 0x836EB0 (sub_836EB0; Boko b_72A720 with the module's blend buffer, no pause and the depth
	// offset +0x88 in the block): builds the prim-model effect matrix (rotations +0x62/+0x60/+0x64,
	// scale +0x50, position +0x1C, camera) and the 0x5C parameter block, then plays the prim-model
	// layout +0x94 with the draw callback 0x836FE0; returns the player result (0 = play finished)
	uint32_t __cdecl da_8C0C90(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = 0x2758178 (vertex lerp scratch),
		// +0x4C..+0x58 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88. +0x20..+0x37,
		// +0x3C..+0x43, +0x5A are never written (the callback 0x8C0DC0 does not read them)
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
		// 30 fps layer: see mag009_doom_activation_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		return prim_play(node + 0x94, PRIM_CB, L, 0);
	}

	// 0x8C5B10 (sub_8C5B10): prim-model player draw callback (layout a1, record a2, block a3) -
	// picks the object's vertex frame (lerped by MAG_017_sub_701390), builds its matrix (rotation
	// 0x701310, or 0x701430 with record flag 0x400; parent-rotated position unless record flag
	// 0x200; scale: a diagonal matrix product 0x56C220 with flag 0x100, else scale3DMatrix), fills a
	// 0x68-byte Field_Alloc render header (flags 0x2000 / 0x2030 |0xC0 with alpha, colour, depth
	// offset block +0x58, scale words 0x100) and draws it with 0x8C5DD0 into OT base+0x44 (shift 2)
	// at the frame arena cursor 0x1D8E054 (Doom 0x8C5B10 draws at its module packet cursor)
	uint32_t __cdecl da_8C0DC0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten), +8 scale (3 x int32, or the 3x3 s16
		// diagonal matrix of the 0x100 path), +0x28 Mat4x3 object matrix (+0x3C translation)
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

		if ((U32(rec, 4) & 0x400) != 0)
			x::MAG_117_sub_701430(rec + 0x10, M);
		else
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
				U16(S, 0) = U16(rec, 0x18);
				U16(S, 8) = U16(rec, 0x1A);
				U16(S, 0x10) = U16(rec, 0x1C);
				U16(S, 0xA) = 0;
				U16(S, 4) = 0;
				U16(S, 0xE) = 0;
				U16(S, 2) = 0;
				U16(S, 0xC) = 0;
				U16(S, 6) = 0;
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
		const uint32_t cursor = MEM<uint32_t>(FRAME_CURSOR);
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		MEM<uint32_t>(FRAME_CURSOR) = da_8C1080(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8C5DD0 (copy of t_764540 0x764540: module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x8C5F00, 0x8C6120, 0x8C63B0, 0x8C68A0, a_73D770, a_73D9C0, 0x8C73C0,
	// 0x8C7920; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl da_8C1080(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { da_8C11B0, da_8C13D0, da_8C1660, da_8C1B50, a_73D770, a_73D9C0, da_8C2670, da_8C2BD0 };
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
	uint32_t __cdecl da_8C11B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl da_8C13D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl da_8C1660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl da_8C1B50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl da_8C2670(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl da_8C2BD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x72AAF0 (097 sub_72AAF0 and copies 0x731570/0x731920/0x731A20, 098/099/100 copies):
	// prim-model task state handler - plays one frame (b_72A720); when the play ended (0) sets
	// the finished flag +0x26 bit0 and advances +0x29
	uint32_t __cdecl da_8C32A0(uint32_t a1)
	{
		const uint32_t r = da_8C0C90(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8C3370 (sub_8C3370): stage darkening in, state 1 - level +0x1C += 0x180 up to 0x800 (then finished: the stage
	// stays dark until phase 4), written to the 4 stage words 0x1D98992 + k * 0x2C
	uint32_t __cdecl da_8C3370(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x180);
		if (S16(a1, 0x1C) >= 0x800)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x800;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x8C3420 (sub_8C3420): stage darkening out, state 0 - after tick 0x18 of its task: level +0x1C = 0x800, next state
	// (0x8C3440 brings it back to 0 by 0x100 a tick)
	uint32_t __cdecl da_8C3420(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x18)
		{
			U16(a1, 0x1C) = 0x800;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl da_8C3440(uint32_t a1)
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

	// 0x8C3490 (sub_8C3490): the target's SINKING task (phase 3, node 0x7C) - 7-state table {0x8C3B30 hide the target,
	// 0x8C3B80 its model drawn by the module + the prim model + the particles, 0x8C46F0 sink 16 ticks, 0x8C4720 sink and fade
	// out, 0x8C47A0 sink 10 ticks more, 0x8C47D0 phase 4, ret}; while +0x26 bit 3 is set the target's model is drawn
	// (0x8C3520, render context 0x274EFF0) after the state
	uint32_t __cdecl da_8C3490(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[7] = { 0x8C3B30, 0x8C3B80, 0x8C46F0, 0x8C4720, 0x8C47A0, 0x8C47D0, 0x8C47F0 };
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 8) != 0)
		{
			// 30 fps layer: see mag009_doom_activation_held.inc
			FX_HELD(held_note_draw(HK_BODY, node);)
			da_8C3520(BODY_CTX);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C3520 (sub_8C3520): draws the target's model through the render context a1 (0x274EFF0: +0x48 entity, +0x4C ->
	// packet cursor 0x274EFEC, +0x54 height offset, +0x56 level, +0x58 / +0x5A the fade band): the entity matrix +0x40 is
	// copied to +0x00 with y + the height offset, its bone matrices rebuilt from its pose (0x508C90), the copy turned by the
	// camera (columns -> +0x20, translation -> +0x34); its shadow (0x5088A0, OT base+0x4040, shift 0x10), both model parts
	// (entity +0x64, and [+0x78]+4 when present) drawn by 0x8C3620 into OT base+0x44 (shift 4); the bone matrices are
	// rebuilt again after
	uint32_t __cdecl da_8C3520(uint32_t a1)
	{
		const uint32_t ctx = a1;
		const uint32_t ent = U32(ctx, 0x48);
		const int32_t dy = S16(ctx, 0x54);
		memcpy((void *)ctx, (const void *)(ent + 0x40), 0x20); // rep movsd
		S32(ctx, 0x18) = add32(S32(ctx, 0x18), dy);
		const uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(ctx);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x20);
		x::GTE_LoadIRFromMatrixColumn(ctx + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x22);
		x::GTE_LoadIRFromMatrixColumn(ctx + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(ctx + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(ctx + 0x34);
		const uint32_t cur = U32(ctx, 0x4C);
		const uint32_t ot_shadow = MEM<uint32_t>(0x1D8E04C) + 0x4040;
		U32(cur, 0) = x::sub_5088A0(ent, ot_shadow, 0x10, U32(cur, 0));
		da_8C3620(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			da_8C3620(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x8C3620 (sub_8C3620): renderer of a battle model part (a1 = geometry {+0 -> bone matrices - 0x10, +4 -> object table
	// {count, offsets}}, a2 = OT, a3 = depth shift (unused: OTZ >> 2), a4 = render context): Field_Alloc scratch 0x84 bytes;
	// per visible object (entity +0x7C mask) every vertex is put in world space by its bone (light matrix + background
	// vector) and given a grey level: the context level +0x56 below the band start +0x58, fading to 0 across the band
	// (+0x58..+0x5A) above it (12-byte records {x, y, z, rgb} in the vertex scratch [+0x44]+4); then projected with the
	// context matrix +0x20 (RTPT), back faces culled (NCLIP), the triangles as semi-transparent Gouraud textured POLY_GT3
	// (code 0x36) and the quads as POLY_GT4 (code 0x3E) with the vertex levels, OTZ / 4, inserted at the packet cursor
	// [+0x4C]
	uint32_t __cdecl da_8C3620(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t ctx = a4;
		uint32_t cursor = U32(U32(ctx, 0x4C), 0);
		const uint32_t verts = U32(U32(ctx, 0x44), 4);      // edi
		const uint32_t bones = U32(a1, 0) + 0x10;           // local +0x2C
		uint32_t tab = U32(a1, 4);
		const int32_t count = S32(tab, 0);                  // local +0x28
		const uint32_t ent = U32(ctx, 0x48);
		const uint32_t w = x::Field_Alloc(0x84);            // esi
		U32(w, 0x48) = U32(ent, 0x7C);                      // visible objects
		tab += 4;
		const uint16_t lo = U16(ctx, 0x58);
		const uint16_t range = (uint16_t)(U16(ctx, 0x5A) - lo);
		const uint16_t level = U16(ctx, 0x56);
		U16(w, 0x7C) = lo;
		U16(w, 0x7A) = level;
		U8(w, 0x81) = 0x3E;
		U8(w, 0x80) = 0x36;
		const int32_t lv = (int16_t)level;
		U16(w, 0x7E) = range;
		U32(w, 0x74) = (uint32_t)((lv * 0x100 + lv) * 0x100 + lv);
		if ((int16_t)range <= 0)
			U16(w, 0x7E) = 1;
		x::GTE_SetRotMatrix(ctx + 0x20);
		x::GTE_SetTransVector(ctx + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(a1, 4) + U32(tab, 0);
			tab += 4;
			if (((U32(w, 0x48) >> (i & 31)) & 1) == 0)
				continue;
			// pass 1: the world vertices and their levels
			uint32_t wv = verts;
			int32_t groups = S16(obj, 0);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = (uint32_t)(bone * 0x30) + bones + 0x10;
				gx::GTE_SetLightMatrix(m);
				gx::GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					const uint16_t vx = U16(obj, 0);
					const uint16_t vy = U16(obj, 2);
					obj += 4;
					U16(w, 0x28) = vx;
					U16(w, 0x2A) = vy;
					U16(w, 0x2C) = U16(obj, 0);
					obj += 2;
					x::GTE_LoadV0(w + 0x28);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(w + 0x28);
					const int16_t y = S16(w, 0x2A);
					const int16_t thr = S16(w, 0x7C);
					if (y > thr)
					{
						const int16_t c = S16(w, 0x7A);
						const int32_t t = mul32((int32_t)y - (int32_t)thr, (int32_t)c) / (int32_t)S16(w, 0x7E);
						U16(w, 0x78) = (uint16_t)t;
						if ((int16_t)t > c)
							U16(w, 0x78) = (uint16_t)c;
						else if ((int16_t)t < 0)
							U16(w, 0x78) = 0;
						const int16_t g = (int16_t)(c - S16(w, 0x78));
						U16(w, 0x78) = (uint16_t)g;
						const int32_t gi = g;
						U32(wv, 8) = (uint32_t)((gi * 0x100 + gi) * 0x100 + gi);
					}
					else
						U32(wv, 8) = U32(w, 0x74);
					U32(wv, 0) = U32(w, 0x28);
					U32(wv, 4) = U32(w, 0x2C);
					wv += 0xC;
				}
			}
			// pass 2: the triangles and quads
			obj = (obj + 3) & ~3u;
			const int32_t ntri = S16(obj, 0);
			const int32_t nquad = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			for (int32_t k = ntri; k > 0; k--, rec += 0x10)
			{
				const uint32_t i0 = U16(rec, 0) & 0xFFF, i1 = U16(rec, 2) & 0xFFF, i2 = U16(rec, 4) & 0xFFF;
				U32(w, 0x3C) = i1;
				U32(w, 0x38) = i0;
				U32(w, 0x40) = i2;
				U32(w, 0x4C) = U32(verts + i0 * 12, 0);
				U32(w, 0x50) = U32(verts + i0 * 12, 4);
				U32(w, 0x54) = U32(verts + i1 * 12, 0);
				U32(w, 0x58) = U32(verts + i1 * 12, 4);
				U32(w, 0x5C) = U32(verts + i2 * 12, 0);
				U32(w, 0x60) = U32(verts + i2 * 12, 4);
				gx::GTE_LoadV012Dwords(w + 0x4C);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(w + 0x6C);
				if (S32(w, 0x6C) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(w + 0x70);
				U32(w, 0x70) = U32(w, 0x70) >> 2;
				gx::GTE_StoreSXY012_PolyGT3(cursor);
				U32(cursor, 0x18) = U32(rec, 0xC);
				U32(cursor, 0xC) = U32(rec, 8);
				U16(cursor, 0x24) = U16(rec, 6);
				U32(cursor, 0) = 0x09000000;
				U32(cursor, 4) = U32(verts + U32(w, 0x38) * 12, 8);
				U32(cursor, 0x10) = U32(verts + U32(w, 0x3C) * 12, 8);
				U32(cursor, 0x1C) = U32(verts + U32(w, 0x40) * 12, 8);
				U8(cursor, 7) = U8(w, 0x80);
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(w, 0x70) * 4, cursor);
				cursor += 0x28;
			}
			rec += 4;
			for (int32_t k = nquad; k > 0; k--, rec += 0x14)
			{
				const uint32_t i0 = U16(rec, -4) & 0xFFF, i1 = U16(rec, -2) & 0xFFF, i2 = U16(rec, 0) & 0xFFF;
				U32(w, 0x3C) = i1;
				U32(w, 0x38) = i0;
				U32(w, 0x40) = i2;
				U32(w, 0x4C) = U32(verts + i0 * 12, 0);
				U32(w, 0x50) = U32(verts + i0 * 12, 4);
				U32(w, 0x54) = U32(verts + i1 * 12, 0);
				U32(w, 0x58) = U32(verts + i1 * 12, 4);
				U32(w, 0x5C) = U32(verts + i2 * 12, 0);
				U32(w, 0x60) = U32(verts + i2 * 12, 4);
				// the fourth vertex of the PREVIOUS quad (+0x44 is set below): stored, never loaded
				const uint32_t p3 = verts + U32(w, 0x44) * 12;
				U32(w, 0x64) = U32(p3, 0);
				U32(w, 0x68) = U32(p3, 4);
				gx::GTE_LoadV012Dwords(w + 0x4C);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(w + 0x6C);
				if (S32(w, 0x6C) <= 0)
					continue;
				x::GTE_StoreSXY012_PolyGT3_2(cursor);
				const uint32_t i3 = U16(rec, 2) & 0xFFF;
				U32(w, 0x44) = i3;
				x::GTE_LoadV0(verts + i3 * 12);
				x::GTE_RTPS();
				x::GTE_ReadSXY2(cursor + 0x2C);
				x::GTE_AVSZ4();
				x::GTE_ReadOTZ(w + 0x70);
				U32(cursor, 0xC) = U32(rec, 4);
				U16(cursor, 0x24) = U16(rec, 0xC);
				U32(cursor, 0x18) = U32(rec, 8);
				U16(cursor, 0x30) = U16(rec, 0xE);
				U32(cursor, 4) = U32(verts + U32(w, 0x38) * 12, 8);
				U32(cursor, 0) = 0x0C000000;
				U32(cursor, 0x10) = U32(verts + U32(w, 0x3C) * 12, 8);
				U32(w, 0x70) = U32(w, 0x70) >> 2;
				U32(cursor, 0x1C) = U32(verts + U32(w, 0x40) * 12, 8);
				U32(cursor, 0x28) = U32(verts + U32(w, 0x44) * 12, 8);
				U8(cursor, 7) = U8(w, 0x81);
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(w, 0x70) * 4, cursor);
				cursor += 0x34;
			}
		}
		U32(U32(ctx, 0x4C), 0) = cursor;
		return x::Field_Free(0x84);
	}

	// 0x8C3B30 (sub_8C3B30): sinking state 0 - the target's (+0x2D) entity word +0 saved in +0x74, its bit 3 set (hidden from
	// the battle's model pass: the module draws it), the sinking anchor 0x27571B8 = its anchor 0xF1 (0x502170), next state
	uint32_t __cdecl da_8C3B30(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		const uint16_t w = U16(ent, 0);
		U32(node, 0x74) = w;
		U16(ent, 0) = (uint16_t)(w | 8);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, ANCHOR);
		next_state(node);
		return 0; // void
	}

	// 0x8C3B80 (sub_8C3B80): sinking state 1 - the render context 0x274EFF0 for the target (0x8C4690: vertex scratch
	// 0x274F638, block 0x27584D0), +0x26 bit 3 (the model is drawn from now on), level 0x40, fade band from the middle of the
	// entity's +0x36 / +0x3C to +0x3C - +0x1E above it; sinking speed +0x6E = -0x28; spawns the prim model 0x8C3C30 (layout
	// 0x1817EE8, 0x80) and the particle spawner 0x8C3D40 (queue 0x274F628); 0x10 ticks (+0x78), next state
	uint32_t __cdecl da_8C3B80(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		da_8C4690(BODY_CTX, BODY_VERTS, BODY_BLOCK, ent);
		const uint16_t top = U16(ent, 0x3C);
		U8(node, 0x26) |= 8;
		const int32_t sum = (int32_t)S16(ent, 0x36) + (int32_t)(int16_t)top;
		MEM<uint16_t>(BODY_CTX + 0x5A) = (uint16_t)(top - U16(ent, 0x1E));
		MEM<uint16_t>(BODY_CTX + 0x58) = (uint16_t)(sum / 2);
		MEM<uint16_t>(BODY_CTX + 0x56) = 0x40;
		U16(node, 0x6E) = 0xFFD8;
		da_8C0B70(node, ORIG_SinkModel, 0x1817EE8, 0x80, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_PARTICLE, ORIG_Spawner, 0x5C, node);
		U16(node, 0x78) = 0x10;
		next_state(node);
		return 0; // void
	}

	// 0x8C3C90 (sub_8C3C90): sinking prim model state 0 - decodes the prim layout (+0x74 -> +0x94, +0x78), position +0x1C =
	// the node's +0x3DC / +0x3E0, scale 1.0, depth offset +0x88 = -0x200, plays (0x8C0C90), next state (0x8C3CF0 follows the
	// sinking anchor)
	uint32_t __cdecl da_8C3C90(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U32(node, 0x1C) = U32(node, 0x3DC);
		U32(node, 0x20) = U32(node, 0x3E0);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFE00;
		da_8C0C90(node);
		next_state(node);
		return 0; // void
	}

	// 0x7B8170 (sub_7B8170): prim-model sequence task 0x7B80B0, state 1 - follows the anchor dwords
	// 0x25DF3C0 / 0x25DF3C4, draws (0x7B5C50); when the sequence ends: finished, next state
	uint32_t __cdecl da_8C3CF0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t w1c = MEM<uint32_t>(0x27571B8);
		const uint32_t w20 = MEM<uint32_t>(0x27571BC);
		U32(node, 0x1C) = w1c;
		U32(node, 0x20) = w20;
		const uint32_t r = da_8C0C90(node);
		if (r == 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x720790 (module 099 sub_720790): TASK - state machine {7207E0, nop}, task tail (no draw).
	uint32_t __cdecl da_8C3D40(uint32_t a1)
	{
		const uint32_t states[2] = { 0x8C3D90, 0x8C4680 };
		callp(states[S8(a1, 0x29)], a1);
		return task_end(a1);
	}

	// 0x8C3D90 (sub_8C3D90): particle spawner state 0 - n = the byte table 0x1634EF4 at the task's tick; once phase 4 runs
	// (a_73B7E0) or at n = 0xFF: finished, next state; else n particles 0x8C3E00 (0x5C bytes, queue 0x274F628)
	uint32_t __cdecl da_8C3D90(uint32_t a1)
	{
		const uint32_t node = a1;
		int32_t n = MEM<uint8_t>(0x1634EF4 + (uint32_t)(int32_t)S16(node, 0x24));
		if (a_73B7E0(4) != 0 || n == 0xFF)
		{
			U8(node, 0x26) |= 1;
			next_state(node);
			return 0; // void
		}
		for (; n > 0; n--)
			x::Effect_AddTaskAndInitFromCtx(Q_PARTICLE, ORIG_Particle, 0x5C, node);
		return 0; // void
	}

	// 0x8C3E00 (sub_8C3E00): PARTICLE task (node 0x5C) - states {0x8C45D0 init, 0x8C4620 sequence step (a_743C20), ret};
	// then turns +0x42 by 0x100 and stands at the sinking anchor 0x27571B8 + (0, 0, 0xC0) turned by Rz(+0x44) Rx(+0x40)
	// Ry(+0x42), drawn (0x8C3F00: the sprite sequence 0x1634B84)
	uint32_t __cdecl da_8C3E00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t states[3] = { 0x8C45D0, 0x8C4620, 0x8C4670 };
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x42) = (uint16_t)(U16(node, 0x42) + 0x100);
		da_particle_place(node);
		da_8C3F00(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7434C0 (module Siren, sub_7434C0): draws one sprite of a sparkle node (unless +0x26 bit 2):
	// 0xD8-byte scratch header, camera matrix (unpacked, rotated by itself) projects the node
	// position +0x1C/+0x1E/+0x20 (-> header +0xCC), sprite data ptr +0x4C, frame +0x50, depth
	// +0x56 -> a_7435E0(header, OT+0x44, 2, packet cursor), cursor updated
	uint32_t __cdecl da_8C3F00(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag009_doom_activation_held.inc
		FX_HELD(held_note_draw(HK_SPARK, a1);)
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

	// 0x8C45D0 (sub_8C45D0): particle state 0 - sprite sequence 0x1634B84 (+0x4C), depth +0x52 = 0xA, three random angles
	// +0x40 / +0x42 / +0x44 (rand & 0xFFF), next state
	uint32_t __cdecl da_8C45D0(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = 0x1634B84;
		U16(node, 0x52) = 0xA;
		U16(node, 0x40) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(node, 0x42) = (uint16_t)(x::CrtRand() & 0xFFF);
		U16(node, 0x44) = (uint16_t)(x::CrtRand() & 0xFFF);
		next_state(node);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl da_8C4620(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x8C4690 (sub_8C4690): render context a1 (0x5C bytes cleared) for the entity a4: +0x40 = a2 (vertex scratch), +0x44 = a3
	// (block: +4 = a2, +0x18 = 0x140, +0x1C..+0x1E = 0x80, +0x20 = -1), +0x48 = a4, +0x4C / +0x50 -> the packet cursor 0x274EFEC
	uint32_t __cdecl da_8C4690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		x::MAG_007_sub_8DCC00(ctx, 0x5C);
		U32(ctx, 0x48) = a4;
		U32(ctx, 0x40) = a2;
		U32(ctx, 0x4C) = PACKET_CURSOR;
		U32(ctx, 0x50) = PACKET_CURSOR;
		U32(a3, 4) = a2;
		U32(ctx, 0x44) = a3;
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

	// 0x8C46F0 (sub_8C46F0): sinking state 2 - the model's height offset 0x274F044 and the anchor's y 0x27571BA += the speed
	// +0x6E; after +0x78 ticks next state
	uint32_t __cdecl da_8C46F0(uint32_t a1)
	{
		da_sink_step(a1);
		U16(a1, 0x78) = (uint16_t)(U16(a1, 0x78) - 1);
		if (S16(a1, 0x78) <= 0)
			next_state(a1);
		return 0; // void
	}

	// 0x8C4720 (sub_8C4720): sinking state 3 - sinks on; the level 0x274F046 -= 0x10; at 0: the model is no longer drawn
	// (+0x26 bit 3 cleared), the target's entity bit 3 back to its saved value (+0x74), 10 ticks (+0x78), the director's
	// done flag +0x48 = 1 (the damage: 0x8C4800), next state
	uint32_t __cdecl da_8C4720(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2D));
		da_sink_step(node);
		const int16_t level = (int16_t)(MEM<uint16_t>(BODY_CTX + 0x56) - 0x10);
		MEM<uint16_t>(BODY_CTX + 0x56) = (uint16_t)level;
		if (level > 0)
			return 0; // void
		const uint16_t keep = (uint16_t)((U8(node, 0x74) ^ U8(ent, 0)) & 8);
		U8(node, 0x26) &= 0xF7;
		MEM<uint16_t>(BODY_CTX + 0x56) = 0;
		U16(node, 0x78) = 0xA;
		U16(ent, 0) = (uint16_t)(U16(ent, 0) ^ keep);
		next_state(node);
		U16(MEM<uint32_t>(DIR_PTR), 0x48) = 1;
		return 0; // void
	}

	// 0x8C47A0 (sub_8C47A0): sinking state 4 - the anchor sinks on (with the hidden model's offset) +0x78 ticks, next state
	uint32_t __cdecl da_8C47A0(uint32_t a1)
	{
		da_sink_step(a1);
		U16(a1, 0x78) = (uint16_t)(U16(a1, 0x78) - 1);
		if (S16(a1, 0x78) <= 0)
			next_state(a1);
		return 0; // void
	}

	// 0x77FF10 (sub_77FF10): director state 12 - at director cue 4 (a_73B640): finished (+0x26 |= 1), next state
	uint32_t __cdecl da_8C47D0(uint32_t a1)
	{
		if (a_73B640(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl da_8C4800(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1634B7C);
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

	// ---- generated (desc9.py / cls9.txt): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x8C0670, (void *)a_73A0D0, "009 MAG_009_sub_8C0670" },
		{ 0x8C0680, (void *)a_73A0D0, "009 MAG_009_sub_8C0680" },
		{ 0x8C06A0, (void *)a_73A170, "009 MAG_009_sub_8C06A0" },
		{ 0x8C06D0, (void *)a_73A1A0, "009 MAG_009_sub_8C06D0" },
		{ 0x8C08F0, (void *)a_73B790, "009 sub_8C08F0" },
		{ 0x8C0910, (void *)a_73B640, "009 sub_8C0910" },
		{ 0x8C0960, (void *)a_73B8A0, "009 sub_8C0960" },
		{ 0x8C09B0, (void *)a_73B7E0, "009 sub_8C09B0" },
		{ 0x8C09F0, (void *)a_733B70, "009 sub_8C09F0" },
		{ 0x8C0A30, (void *)a_733760, "009 sub_8C0A30" },
		{ 0x8C0A50, (void *)a_73B620, "009 sub_8C0A50" },
		{ 0x8C0A70, (void *)a_7475A0, "009 sub_8C0A70" },
		{ 0x8C0A90, (void *)a_73A6D0, "009 nullsub_1823" },
		{ 0x8C0AA0, (void *)a_73A6D0, "009 nullsub_1824" },
		{ 0x8C0BC0, (void *)a_73A380, "009 sub_8C0BC0" },
		{ 0x8C2140, (void *)a_73D770, "009 sub_8C2140" },
		{ 0x8C2390, (void *)a_73D9C0, "009 sub_8C2390" },
		{ 0x8C32C0, (void *)a_73A6D0, "009 nullsub_1825" },
		{ 0x8C32D0, (void *)a_73A380, "009 sub_8C32D0" },
		{ 0x8C3330, (void *)a_7335D0, "009 sub_8C3330" },
		{ 0x8C33B0, (void *)a_73A6D0, "009 nullsub_1826" },
		{ 0x8C33C0, (void *)a_73A380, "009 sub_8C33C0" },
		{ 0x8C3480, (void *)a_73A6D0, "009 nullsub_1827" },
		{ 0x8C3C30, (void *)a_73A380, "009 sub_8C3C30" },
		{ 0x8C3D30, (void *)a_73A6D0, "009 nullsub_1828" },
		{ 0x8C4020, (void *)a_7435E0, "009 InitEffectSequenceFromData_c17" },
		{ 0x8C4160, (void *)a_743720, "009 sub_8C4160" },
		{ 0x8C4640, (void *)a_743C20, "009 sub_8C4640" },
		{ 0x8C4670, (void *)a_73A6D0, "009 nullsub_1829" },
		{ 0x8C4680, (void *)a_73A6D0, "009 nullsub_1830" },
		{ 0x8C47F0, (void *)a_73A6D0, "009 nullsub_1831" },
		{ 0x8C4840, (void *)a_7474B0, "009 MAG_009_sub_8C4840" },
		{ 0x8C4860, (void *)a_7474D0, "009 MAG_009_sub_8C4860" },
		{ 0x8C4880, (void *)a_73A6D0, "009 nullsub_1832" },
		{ 0x8C4890, (void *)a_747500, "009 MAG_009_sub_8C4890" },
		{ 0x8C48D0, (void *)a_73A0D0, "009 MAG_009_sub_8C48D0" },
		{ 0x8C48E0, (void *)a_747550, "009 MAG_009_sub_8C48E0" },
		{ 0x8C48F0, (void *)a_747560, "009 MAG_009_sub_8C48F0" },
		{ 0x8C4920, (void *)a_747590, "009 MAG_009_sub_8C4920" },
		{ 0x8C4930, (void *)a_7475A0, "009 MAG_009_sub_8C4930" },
		{ 0x8C4950, (void *)a_73A6D0, "009 nullsub_1822" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x8C0500, (void *)da_8C0500, "009 da_8C0500" },
		{ 0x8C0690, (void *)da_8C0690, "009 da_8C0690" },
		{ 0x8C0750, (void *)da_8C0750, "009 da_8C0750" },
		{ 0x8C0780, (void *)da_8C0780, "009 da_8C0780" },
		{ 0x8C0800, (void *)da_8C0800, "009 da_8C0800" },
		{ 0x8C08A0, (void *)da_8C08A0, "009 da_8C08A0" },
		{ 0x8C0940, (void *)da_8C0940, "009 da_8C0940" },
		{ 0x8C0980, (void *)da_8C0980, "009 da_8C0980" },
		{ 0x8C09D0, (void *)da_8C09D0, "009 da_8C09D0" },
		{ 0x8C0A10, (void *)da_8C0A10, "009 da_8C0A10" },
		{ 0x8C0AB0, (void *)da_8C0AB0, "009 da_8C0AB0" },
		{ 0x8C0B70, (void *)da_8C0B70, "009 da_8C0B70" },
		{ 0x8C0C20, (void *)da_8C0C20, "009 da_8C0C20" },
		{ 0x8C0C90, (void *)da_8C0C90, "009 da_8C0C90" },
		{ 0x8C0DC0, (void *)da_8C0DC0, "009 da_8C0DC0" },
		{ 0x8C1080, (void *)da_8C1080, "009 da_8C1080" },
		{ 0x8C11B0, (void *)da_8C11B0, "009 da_8C11B0" },
		{ 0x8C13D0, (void *)da_8C13D0, "009 da_8C13D0" },
		{ 0x8C1660, (void *)da_8C1660, "009 da_8C1660" },
		{ 0x8C1B50, (void *)da_8C1B50, "009 da_8C1B50" },
		{ 0x8C2670, (void *)da_8C2670, "009 da_8C2670" },
		{ 0x8C2BD0, (void *)da_8C2BD0, "009 da_8C2BD0" },
		{ 0x8C32A0, (void *)da_8C32A0, "009 da_8C32A0" },
		{ 0x8C3370, (void *)da_8C3370, "009 da_8C3370" },
		{ 0x8C3420, (void *)da_8C3420, "009 da_8C3420" },
		{ 0x8C3440, (void *)da_8C3440, "009 da_8C3440" },
		{ 0x8C3490, (void *)da_8C3490, "009 da_8C3490" },
		{ 0x8C3520, (void *)da_8C3520, "009 da_8C3520" },
		{ 0x8C3620, (void *)da_8C3620, "009 da_8C3620" },
		{ 0x8C3B30, (void *)da_8C3B30, "009 da_8C3B30" },
		{ 0x8C3B80, (void *)da_8C3B80, "009 da_8C3B80" },
		{ 0x8C3C90, (void *)da_8C3C90, "009 da_8C3C90" },
		{ 0x8C3CF0, (void *)da_8C3CF0, "009 da_8C3CF0" },
		{ 0x8C3D40, (void *)da_8C3D40, "009 da_8C3D40" },
		{ 0x8C3D90, (void *)da_8C3D90, "009 da_8C3D90" },
		{ 0x8C3E00, (void *)da_8C3E00, "009 da_8C3E00" },
		{ 0x8C3F00, (void *)da_8C3F00, "009 da_8C3F00" },
		{ 0x8C45D0, (void *)da_8C45D0, "009 da_8C45D0" },
		{ 0x8C4620, (void *)da_8C4620, "009 da_8C4620" },
		{ 0x8C4690, (void *)da_8C4690, "009 da_8C4690" },
		{ 0x8C46F0, (void *)da_8C46F0, "009 da_8C46F0" },
		{ 0x8C4720, (void *)da_8C4720, "009 da_8C4720" },
		{ 0x8C47A0, (void *)da_8C47A0, "009 da_8C47A0" },
		{ 0x8C47D0, (void *)da_8C47D0, "009 da_8C47D0" },
		{ 0x8C4800, (void *)da_8C4800, "009 da_8C4800" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag009_doom_activation()
	{
		act::register_module(9);
		for (const act::doomact::ModPort *p = act::doomact::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(9, p->addr, p->port, p->name);
		for (const act::doomact::ModPort *p = act::doomact::PORTS; p->addr; p++)
			act::register_module_port(9, p->addr, p->port, p->name);
		// 30 fps layer: see mag009_doom_activation_held.inc
		FX_HELD(register_mag009_held();)
	}
	FX_LAB_MODULE(register_mag009_doom_activation);
}

#ifdef FF8_FX_HELD
#include "mag009_doom_activation_held.inc"
#endif
