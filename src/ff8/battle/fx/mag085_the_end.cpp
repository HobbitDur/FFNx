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

// Effect 85: The End (kernel.bin magic The End +4 = 85: Selphie's Slot; MAG_085_THE_END*, texture file 0x1572098): a copy of
// the actor/heal effect library (see act_engine.h) built like MiniMog (mag096_minimog.cpp: master, the director / cut
// machinery of Tonberry / Doom, the prim-model tasks and renderers of Doom / Siren, the actor systems) plus the module's own
// parts. Module 0x784480..0x78D790 (201 functions: 92 served by engine ports, 50 the relocated text of a ported library
// function (Doom 8, Siren 95, MiniMog 96, Ultimecia 80 / 82 / 83, Tonberry 90, Resonance 49, Heartbreak 31, Boko 99),
// 57 hand-ported, the loader and the setup not ported).
//
// Setup MAG_085_THE_END_Init 0x7844A0 (runs once, not ported; file loader 0x784480 = the texture file 0x1572098 ->
// [0x25BC390], TIM uploaded and the camera animation 0x1571CBC started unless the cast context flag +1 bit 0; packet arenas at
// file + 0x8000 / + 0x10000 by tick parity (0x25C1B5C / 0x25C1B58, 0x25C0F54 / 0x25C0F50), the file arena cursor
// 0x25C21D8 = file + 0x10000) creates the root queue 0x25BC570 with the master and eight task pools.
//   Master (0x784630 = MiniMog's master 0x731F70 with 11 states) - camera copy 0x2793E58 (pointers 0x25C21CC / 0x25BC4A0),
//     packet arenas by tick parity (sprite cursor 0x25BC394 / 0x25BC49C), target bone follow, states: 0x7847E0 carves the
//     two pools of the file arena (0x798 + 0x9D80 bytes), 0x784860 starts the cut machinery 0x784890, then the library's
//     finish states; executes the eight module queues.
//   Cut machinery (0x784890) - 0x784910: the director block (0x784DC0) and the file loader block (0x784960), the LOAD
//     SCRIPT task 0x784A60 (0x1571E70: pre_LoadBattleFile of the battle files 0x192..0x194 into 0x19CF018 / 0x19EB018 /
//     0x19D2560, a stream via BdTransSummonStream; the callbacks 0x784C60.. queue the TIM uploads) and the DIRECTOR 0x784E30
//     (23 states: battle UI off / on (0x4A8480), sounds 0x156C7F4 / 0x156C7F8, the music volume hook 0x7850C0, the screen
//     tint task 0x8DDC30 (script 0x1571FE8), cuts 1..6).
//   Cuts (0x7853C0): 1 - the stage fade 0x787E20 (the four stage draw entries' level words 0x1D98992 + 0x2C k), the stage
//     wobble 0x7882E0, the darkening 0x78D410, the MUSIC FADE 0x787FD0 (Music_SetVolumeImmediate(0, 0x7F .. 0x30)) and the
//     meteor emitter 0x788420 (meteors 0x788580: models 0x156C7FC falling from random points, the impact prim play 0x788A90
//     and actor system 0x788B90); 2 - the ground 0x78C4C0 (model 0x156C8BC through the module's own gouraud-textured quad
//     renderer 0x78C660 / 0x78C6D0 with a grey-level fade); 3 - the prim plays 0x785560 (layout 0x1718EE4, with the battle
//     entities' SHADOWS 0x785620: BattleEntity_DrawShadow of entities 3..6) and two orbiting plays 0x787B50 (layout
//     0x171C42C); 4 - the petal emitter 0x78CE80 (petal groups 0x78CFA0: models 0x156C85C swinging down); 5 - the glow /
//     letterbox task 0x78D250 (the glow quad 0x78D2E0, the black bars 0x78D4D0, both at OT + 0x14).
//   The prim plays draw through the object callback 0x785790 and the module renderer 0x785A90 (Siren's 0x73C790 family:
//   lists 0x785BC0 / 0x785DE0 / 0x786070 / 0x7864F0 / 0x786FA0 / 0x7874A0); the actor systems (0x788C20 = a_73F990
//   family) are the library's.
// ENTITY / CAMERA / ENGINE WRITES: the four stage draw entries' level words (0x1D98992 + 0x2C k) and colour bytes; the
// battle UI (0x4A8480); the music volume (Music_SetVolumeImmediate); battle file loads (pre_LoadBattleFile) and a summon
// stream (BdTransSummonStream); TIM uploads; the screen tint (0x8DDC30); the entities' shadows are drawn (not written); the
// camera animation 0x1571CBC (setup) and the target bone follow (Effect_UpdateTargetPosFromBones); action results through
// the library (0x506690).
// Module globals: 0x25BC390..0x25C234C (texture file 0x25BC390, cursors, queues and pools, the director block 0x25C1C60,
// the loader block 0x25C21E0); data: 0x156C7F0.. (director / loader pointers, models, sounds), 0x1571E70 (load script),
// layouts 0x1715D28 / 0x1718EE4 / 0x171C42C, actor data 0x17151EC.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace theend
{
	// ---- generated (desc85.py): the module descriptor of this copy of the library ----
	static const Mod MOD_085 = { "theend", 85, 0x784480, 0x78D790,
		{ 0x0, 0x0, 0x156C7F0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25BC390, 0x0, 0x0, 0x0, 0x25BC4A0, 0x0, 0x0, 0x25BC894, 0x0, 0x0, 0x0, 0x0, 0x25C1748, 0x0, 0x25C19E8, 0x0, 0x0, 0x25C1B60, 0x25C1C58, 0x0, 0x0, 0x0, 0x25C1D28, 0x0, 0x0, 0x0, 0x25C21D4, 0x25C21DC },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x784890, 0x784910, 0x0, 0x784C60, 0x784C90, 0x784CC0, 0x784CD0, 0x784CE0, 0x784CF0, 0x784D00, 0x0, 0x0, 0x785040, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x784FA0, 0x0, 0x785000, 0x785790, 0x0, 0x0, 0x0, 0x0, 0x0, 0x788DD0, 0x788EB0, 0x789050, 0x789120, 0x789240, 0x7894A0, 0x0, 0x0, 0x78A1C0, 0x78A210, 0x0, 0x0, 0x78AB00, 0x0, 0x0, 0x0, 0x78B4E0, 0x78B510, 0x78B5F0, 0x78B710, 0x78B860, 0x0, 0x78BF20, 0x78C100, 0x78C240, 0x0, 0x0, 0x78D600, 0x78D670, 0x78D690, 0x78D6B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x156C7F0;        // pointer to the director block 0x25C1C60 (cut +0x40/+0x42/+0x44, frame +0x46)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t SPRITE_CURSOR = 0x25BC394;  // module sprite packet cursor (tick parity: file + 0x8000 / + 0)
	static const uint32_t PACKET_CURSOR = SPRITE_CURSOR;
	static const uint32_t FA_CURSOR = 0x1D8E054;      // frame arena packet cursor
	static const uint32_t SCRATCH_SP = 0x25C21CC;     // module scratch stack pointer (the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25C21DC;    // current actor system state block (pool header)
	static const uint32_t LOADER_PTR = 0x25C1C58;     // pointer to the file loader block 0x25C21E0
	static const uint32_t SCRIPT_CUR = 0x25C1B60;     // load script cursor (0x1571E70, 0x10-byte records)

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t sub_4A8480(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x4A8480)(a1); }          // Battle_4A8480: battle UI on / off
		static inline uint32_t MAG_063_sub_7015B0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x7015B0)(a1, a2); } // MAG_063_RotationFromAnglesYXZ
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

	// the one-argument task tail of the Boko text (status read here)
	static inline uint32_t task_end(uint32_t node) { const uint8_t st = U8(node, 0x26); U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1); return task_end(node, st); }

	// helpers of 0x78C6D0: the texture-window word (GP0 0xE2) of the {x, y, w, h} block at ctx + o; screen bounds; the u (o 0)
	// / v (o 1) bytes of the 4 corners += d, wrapped by the window size byte when any sum exceeds 0xFF
	static inline uint32_t te_texwin(uint32_t ctx, int32_t o)
	{
		uint32_t c = ((uint32_t)U8(ctx, o + 2) & 0xF8) | 0xFFFE2000u;
		c = (c << 5) | ((uint32_t)U8(ctx, o) & 0xF8);
		c = (c << 5) | (~((uint32_t)U16(ctx, o + 6) - 1u) & 0xF8);
		c = (c << 2) | ((~((uint32_t)U16(ctx, o + 4) - 1u) >> 3) & 0x1F);
		return c;
	}
	static inline bool te_offx(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool te_offy(int16_t v) { return v < 0 || v > 0x6C0; }
	static inline void te_scroll(uint32_t pk, int32_t o, uint32_t d, uint8_t w)
	{
		const uint32_t a = U8(pk, 0xC + o) + d, b = U8(pk, 0x18 + o) + d, c = U8(pk, 0x24 + o) + d, e = U8(pk, 0x30 + o) + d;
		if ((int32_t)(e | c | b | a) > 0xFF)
		{
			U8(pk, 0xC + o) = (uint8_t)(a - w);
			U8(pk, 0x18 + o) = (uint8_t)(b - w);
			U8(pk, 0x24 + o) = (uint8_t)(c - w);
			U8(pk, 0x30 + o) = (uint8_t)(e - w);
		}
		else
		{
			U8(pk, 0xC + o) = (uint8_t)a;
			U8(pk, 0x18 + o) = (uint8_t)b;
			U8(pk, 0x24 + o) = (uint8_t)c;
			U8(pk, 0x30 + o) = (uint8_t)e;
		}
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl te_784630(uint32_t a1);
	uint32_t __cdecl te_7847E0(uint32_t a1);
	uint32_t __cdecl te_784820();
	uint32_t __cdecl te_784910(uint32_t a1);
	uint32_t __cdecl te_784960();
	uint32_t __cdecl te_784DC0(uint32_t a1);
	uint32_t __cdecl te_784E30(uint32_t a1);
	uint32_t __cdecl te_784F30(uint32_t a1);
	uint32_t __cdecl te_784F80(uint32_t a1);
	uint32_t __cdecl te_784FD0(uint32_t a1);
	uint32_t __cdecl te_785080(uint32_t a1);
	uint32_t __cdecl te_785110(uint32_t a1);
	uint32_t __cdecl te_785170(uint32_t a1);
	uint32_t __cdecl te_785200(uint32_t a1);
	uint32_t __cdecl te_785230(uint32_t a1);
	uint32_t __cdecl te_785250(uint32_t a1);
	uint32_t __cdecl te_785290(uint32_t a1);
	uint32_t __cdecl te_7852D0(uint32_t a1);
	uint32_t __cdecl te_7852F0(uint32_t a1);
	uint32_t __cdecl te_785310(uint32_t a1);
	uint32_t __cdecl te_785330(uint32_t a1);
	uint32_t __cdecl te_785350(uint32_t a1);
	uint32_t __cdecl te_7853C0(uint32_t a1);
	uint32_t __cdecl te_785510(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl te_7855C0(uint32_t a1);
	uint32_t __cdecl te_785620();
	uint32_t __cdecl te_785790(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_785A90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_785BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_785DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_786070(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_7864F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_786FA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_7874A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_787AF0(uint32_t a1);
	uint32_t __cdecl te_787B50(uint32_t a1);
	uint32_t __cdecl te_787BB0(uint32_t a1);
	uint32_t __cdecl te_787BE0(uint32_t a1);
	uint32_t __cdecl te_787C90(uint32_t a1);
	uint32_t __cdecl te_787E90(uint32_t a1);
	uint32_t __cdecl te_787ED0(uint32_t a1);
	uint32_t __cdecl te_787F30(uint32_t a1);
	uint32_t __cdecl te_787F80(uint32_t a1);
	uint32_t __cdecl te_788040(uint32_t a1);
	uint32_t __cdecl te_788060(uint32_t a1);
	uint32_t __cdecl te_7880A0(void);
	uint32_t __cdecl te_7880E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_788220(uint32_t a1);
	uint32_t __cdecl te_788250(void);
	uint32_t __cdecl te_788290(uint32_t a1);
	uint32_t __cdecl te_788350(uint32_t a1);
	uint32_t __cdecl te_788370(uint32_t a1);
	uint32_t __cdecl te_7883B0(uint32_t a1);
	uint32_t __cdecl te_7883D0(uint32_t a1);
	uint32_t __cdecl te_788480(uint32_t a1);
	uint32_t __cdecl te_788490(uint32_t a1);
	uint32_t __cdecl te_788580(uint32_t a1);
	uint32_t __cdecl te_788700(uint32_t a1);
	uint32_t __cdecl te_788810(uint32_t a1);
	uint32_t __cdecl te_788960(uint32_t a1);
	uint32_t __cdecl te_788AF0(uint32_t a1);
	uint32_t __cdecl te_788B40(uint32_t a1);
	uint32_t __cdecl te_788BF0(uint32_t a1);
	uint32_t __cdecl te_788DD0(uint32_t a1);
	uint32_t __cdecl te_789050(void);
	uint32_t __cdecl te_789700(uint32_t a1);
	uint32_t __cdecl te_789790(void);
	uint32_t __cdecl te_7897F0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_789950(void);
	uint32_t __cdecl te_789A10(uint32_t a1);
	uint32_t __cdecl te_789A90(void);
	uint32_t __cdecl te_789AD0(uint32_t a1);
	uint32_t __cdecl te_78AB00(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78AC10(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78AC90(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78ACC0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_78AD60(uint32_t a1);
	uint32_t __cdecl te_78ADE0(uint32_t a1);
	uint32_t __cdecl te_78AE60(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_78B640(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78B860(uint32_t a1);
	uint32_t __cdecl te_78C350(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78C3E0(void);
	uint32_t __cdecl te_78C470(uint32_t a1);
	uint32_t __cdecl te_78C4C0(uint32_t a1);
	uint32_t __cdecl te_78C530(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78C660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78C6D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78CE00(uint32_t a1);
	uint32_t __cdecl te_78CE30(uint32_t a1);
	uint32_t __cdecl te_78CE80(uint32_t a1);
	uint32_t __cdecl te_78CED0(uint32_t a1);
	uint32_t __cdecl te_78CFA0(uint32_t a1);
	uint32_t __cdecl te_78D150(uint32_t a1);
	uint32_t __cdecl te_78D180(uint32_t a1);
	uint32_t __cdecl te_78D1C0(uint32_t a1);
	uint32_t __cdecl te_78D1F0(uint32_t a1);
	uint32_t __cdecl te_78D250(uint32_t a1);
	uint32_t __cdecl te_78D2B0(uint32_t a1);
	uint32_t __cdecl te_78D2E0(uint32_t a1);
	uint32_t __cdecl te_78D3A0(uint32_t a1);
	uint32_t __cdecl te_78D3D0(uint32_t a1);
	uint32_t __cdecl te_78D480(uint32_t a1);
	uint32_t __cdecl te_78D4A0(uint32_t a1);
	uint32_t __cdecl te_78D4D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_78D590(uint32_t a1);
	uint32_t __cdecl te_78D5C0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag085_the_end_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace theend
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Doom / Siren / MiniMog / Tonberry / Ultimecia / Resonance / Heartbreak / Boko port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x731F70 (module 096 MAG_096_sub_731F70): MiniMog MASTER task - snapshots the camera matrix
	// into 0x2793E58, picks the double-buffered pools by tick parity, runs the 11-state master
	// state table, then executes the 8 module task queues (live task count -> node +0x5E).
	uint32_t __cdecl te_784630(uint32_t a1)
	{
		g_mod = &MOD_085;
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25C21CC) = 0x2793E58;
		MEM<uint32_t>(0x25BC4A0) = 0x2793E58;
		states[0] = 0x7847C0;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x7847D0;
		states[2] = 0x7847E0;
		states[3] = 0x784860;
		states[4] = 0x78D6C0;
		states[5] = 0x78D700;
		states[6] = 0x78D710;
		states[7] = 0x78D720;
		states[8] = 0x78D750;
		states[9] = 0x78D760;
		states[10] = 0x78D780;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25C1B5C);
			const uint32_t v2 = MEM<uint32_t>(0x25C0F54);
			MEM<uint32_t>(0x25BC394) = v1;
			MEM<uint32_t>(0x25BC49C) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25C1B58);
			const uint32_t v2 = MEM<uint32_t>(0x25C0F50);
			MEM<uint32_t>(0x25BC394) = v1;
			MEM<uint32_t>(0x25BC49C) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x25C21B0) = 0;
		MEM<uint16_t>(0x25C1CB4) = 0;
		static const uint32_t queues[8] = { 0x25C19E8, 0x25BC478, 0x25C21B8, 0x25C1748, 0x25BC580, 0x25BC898, 0x25C0F40, 0x25C19D8 };
		for (int i = 0; i < 8; i++)
		{
			// 30 fps layer: see mag085_the_end_held.inc
			FX_HELD(if (i == 7) held_note_tints(queues[i]);)
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7847E0 (MAG_085_sub_7847E0): master state 2 - once no child is alive: the two pools are carved from the file arena
	// cursor [0x25C21D8] (0x798 bytes at 0x25C1CB8, 0x9D80 bytes at 0x25C21D0) and cleared (0x784820), next state
	uint32_t __cdecl te_7847E0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25C21D8);
		MEM<uint32_t>(0x25C1CB8) = p;
		p += 0x798;
		MEM<uint32_t>(0x25C21D0) = p;
		p += 0x9D80;
		MEM<uint32_t>(0x25C21D8) = p;
		te_784820();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x784820 (MAG_085_sub_784820): clears the two pools (0x798 / 0x9D80 bytes) and the pool counters 0x25BC890 /
	// 0x25C1CC4 / 0x25C1C54 / 0x25BC398
	uint32_t __cdecl te_784820()
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25C1CB8), 0x798);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25C21D0), 0x9D80);
		MEM<uint16_t>(0x25BC890) = 0;
		MEM<uint16_t>(0x25C1CC4) = 0;
		MEM<uint16_t>(0x25C1C54) = 0;
		MEM<uint16_t>(0x25BC398) = 0;
		return 0;
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl te_784910(uint32_t a1)
	{
		te_784DC0(a1);
		te_784960();
		x::Effect_AddTaskAndInitFromCtx(0x25C21B8, 0x784A60, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25BC478, 0x784E30, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x784960 (MAG_085_sub_784960): the file loader block (0x25C21E0, 0x16C bytes; pointer 0x25C1C58) and the load script
	// cursor 0x25C1B60 = 0x1571E70: +0x100 = an arena of 0xA000 bytes from the file arena cursor, +0x104.. the load
	// destinations (0x19CF018, +0x114 0x19EB018, +0x12C 0x19D2560), +0x164 / +0x166 loading / script-done words cleared
	uint32_t __cdecl te_784960()
	{
		MEM<uint32_t>(0x25C1B60) = 0x1571E70;
		MEM<uint32_t>(0x25C1C58) = 0x25C21E0;
		x::MAG_007_sub_8DCC00(0x25C21E0, 0x16C);
		const uint32_t b = MEM<uint32_t>(0x25C1C58);
		uint32_t c = MEM<uint32_t>(0x25C21D8);
		U32(b, 0x100) = c;
		c += 0xA000;
		MEM<uint32_t>(0x25C21D8) = c;
		U32(b, 0x114) = 0x19EB018;
		for (int32_t o = 0x104; o <= 0x160; o += 4)
			if (o != 0x114 && o != 0x12C) U32(b, o) = 0x19CF018;
		U32(b, 0x12C) = 0x19D2560;
		U16(b, 0x164) = 0;
		U16(b, 0x166) = 0;
		return b;
	}

	// 0x784DC0 (MAG_085_sub_784DC0): director data set-up - clears the director block [0x156C7F0] (0x54 bytes), +0x04 =
	// 0x1000 (the other position words 0), +0x4A = the angle of (+8 - +0, +0xC - 0x1000) - 0x800
	uint32_t __cdecl te_784DC0(uint32_t a1)
	{
		(void)a1;
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x156C7F0), 0x54);
		const uint32_t d = MEM<uint32_t>(0x156C7F0);
		U16(d, 8) = 0;
		U16(d, 0xA) = 0;
		U16(d, 0xC) = 0;
		U16(d, 0) = 0;
		U16(d, 2) = 0;
		U16(d, 4) = 0x1000;
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const int16_t dz = (int16_t)(U16(d, 0xC) - 0x1000);
		const uint32_t a = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(0x156C7F0), 0x4A) = (uint16_t)((a - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x784E30 (MAG_085_sub_784E30): DIRECTOR task with 23 states - cut bookkeeping (0x784F30), then {0x784F80 cut 1 +
	// battle UI off, 0x784FD0 sound 0x156C7F4, 0x785020 load script done, 0x785080 file / stream wait, 0x785110 frame 0x4B,
	// 0x785130 / 0x785150 cut 2, 0x785170 screen tint at frame 0xE, 0x7851C0 wait, 0x7851E0 / 0x785200 cut 3 + sound
	// 0x156C7F8, 0x785230 frame 0x3C, 0x785250 / 0x785270 cut 4, 0x785290 frame 0x2D, 0x7852B0 / 0x7852D0 cut 5, 0x7852F0
	// frame 0x2D, 0x785310 / 0x785330 cut 6, 0x785350 battle UI on at frame 0xA, 0x785380 finish, 0x7853A0 ret}
	uint32_t __cdecl te_784E30(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[23];
		states[0] = 0x784F80;
		states[1] = 0x784FD0;
		states[2] = 0x785020;
		states[3] = 0x785080;
		states[4] = 0x785110;
		states[5] = 0x785130;
		states[6] = 0x785150;
		states[7] = 0x785170;
		states[8] = 0x7851C0;
		states[9] = 0x7851E0;
		states[10] = 0x785200;
		states[11] = 0x785230;
		states[12] = 0x785250;
		states[13] = 0x785270;
		states[14] = 0x785290;
		states[15] = 0x7852B0;
		states[16] = 0x7852D0;
		states[17] = 0x7852F0;
		states[18] = 0x785310;
		states[19] = 0x785330;
		states[20] = 0x785350;
		states[21] = 0x785380;
		states[22] = 0x7853A0; // nullsub (ret)
		te_784F30(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl te_784F30(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			te_7853C0(a1);
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

	// 0x784F80 (sub_784F80): director state 0 - battle UI off (0x4A8480(0)); cut 1 requested -> next state
	uint32_t __cdecl te_784F80(uint32_t a1)
	{
		gx::sub_4A8480(0);
		if (a_73B640(1) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl te_784FD0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x156C7F4, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x785080 (sub_785080): director state 3 - when the load script is done (loader +0x166 == 0): the music volume hook
	// 0x7850C0 (0) and next state; else from frame 0x4A the hook (1)
	uint32_t __cdecl te_785080(uint32_t a1)
	{
		if (U16(MEM<uint32_t>(0x25C1C58), 0x166) == 0)
		{
			a_73BB60(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (S16(MEM<uint32_t>(0x156C7F0), 0x46) >= 0x4A)
			a_73BB60(1);
		return 0; // void
	}

	// 0x824AE0 / 0x824B00 (sub_824AE0 / sub_824B00): hit states 2 / 3 - next state once the phase has
	// run 0x37 / 0x4B ticks
	uint32_t __cdecl rs_824AE0(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0x37)
			next_state(a1);
		return 0; // void
	}
	uint32_t __cdecl te_785110(uint32_t a1)
	{
		if (S16(t1_data(), 0x46) >= 0x4B)
			next_state(a1);
		return 0; // void
	}

	// 0x785170 (sub_785170): director state 7 - from frame 0xE: wait 6 ticks (+0x44), the screen tint task 0x8DDC30 (script
	// 0x1571FE8) in the queue 0x25C19D8, next state
	uint32_t __cdecl te_785170(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(0x156C7F0), 0x46) < 0xE)
			return 0; // void
		U16(a1, 0x44) = 6;
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25C19D8, 0x8DDC30, 0x40, a1);
		U32(t, 0x34) = 0x1571FE8;
		U16(t, 0x38) = 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x785200 (sub_785200): director state 10 - cut 3 current: sound 0x156C7F8, next state
	uint32_t __cdecl te_785200(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			x::BdPlaySE(0x156C7F8, 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x785230 (sub_785230): director state 11 - next state from frame 0x3C
	uint32_t __cdecl te_785230(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(0x156C7F0), 0x46) >= 0x3C)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x73BA50 (module; Siren sub_73BA50): director state: asks for cut 4, advances when accepted
	uint32_t __cdecl te_785250(uint32_t a1)
	{
		uint32_t r = a_73B640(4);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x82B400 (sub_82B400): director state - next state from phase tick 0x36
	uint32_t __cdecl te_785290(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(PHASE_DATA), 0x46) >= 0x2D)
			next_state(a1);
		return 0; // void
	}

	// 0x73B620 (engine): state helper - director gate 5 (0x73B640); ++state when passed
	uint32_t __cdecl te_7852D0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(5);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x82B400 (sub_82B400): director state - next state from phase tick 0x36
	uint32_t __cdecl te_7852F0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(PHASE_DATA), 0x46) >= 0x2D)
			next_state(a1);
		return 0; // void
	}

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73B640(6), then advances the state index +0x29
	uint32_t __cdecl te_785310(uint32_t a1)
	{
		const uint32_t r = a_73B640(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73B7E0(6), then advances the state index +0x29
	uint32_t __cdecl te_785330(uint32_t a1)
	{
		const uint32_t r = a_73B7E0(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x785350 (sub_785350): director state 20 - from frame 0xA: director +0x48 = 1, battle UI on (0x4A8480(1)), next state
	uint32_t __cdecl te_785350(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x156C7F0);
		if (S16(d, 0x46) < 0xA)
			return 0; // void
		U16(d, 0x48) = 1;
		gx::sub_4A8480(1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7853C0 (sub_7853C0): director new cut (current cut 1..5, jump table 0x7854F8) - cut 1: the ground task 0x787E20,
	// the wobble 0x7882E0, the stage fade 0x78D410, the music fade 0x787FD0 (queue 0x25BC580, 0x60) and the meteor emitter
	// 0x788420 (queue 0x25BC898, 0xE4); cut 2: the flower emitter 0x78C4C0; cut 3: the prim plays 0x785560 (layout
	// 0x1718EE4) and twice 0x787B50 (layout 0x171C42C, +0x8A = 0 / 1); cut 4: the petal emitter 0x78CE80; cut 5: the
	// letterbox 0x78D250
	uint32_t __cdecl te_7853C0(uint32_t a1)
	{
		const int32_t cut = S16(MEM<uint32_t>(0x156C7F0), 0x40);
		switch (cut - 1)
		{
		case 0:
			x::Effect_AddTaskAndInitFromCtx(0x25BC580, 0x787E20, 0x60, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25BC580, 0x7882E0, 0x60, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25BC580, 0x78D410, 0x60, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25BC580, 0x787FD0, 0x60, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25BC898, 0x788420, 0xE4, a1);
			break;
		case 1:
			x::Effect_AddTaskAndInitFromCtx(0x25BC898, 0x78C4C0, 0xE4, a1);
			break;
		case 2:
		{
			te_785510(a1, 0x785560, 0x1718EE4, 0x20, 1, 0);
			const uint32_t t1 = te_785510(a1, 0x787B50, 0x171C42C, 0x50, 2, 0);
			U16(t1, 0x8A) = 0;
			const uint32_t t2 = te_785510(a1, 0x787B50, 0x171C42C, 0x50, 2, 0);
			U16(t2, 0x8A) = 1;
			break;
		}
		case 3:
			x::Effect_AddTaskAndInitFromCtx(0x25BC898, 0x78CE80, 0xE4, a1);
			break;
		case 4:
			x::Effect_AddTaskAndInitFromCtx(0x25BC580, 0x78D250, 0x60, a1);
			break;
		default:
			break;
		}
		return 0; // void
	}

	// 0x785510 (sub_785510): a prim-play task fn in the queue 0x25C0F40 (0x2DC bytes): +0x74 = layout data, +0x78 = its
	// size (sign-extended word), +0x80 / +0x82 = the two words; returns the node
	uint32_t __cdecl te_785510(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25C0F40, a2, 0x2DC, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x7855C0 (sub_7855C0): prim-play task state 0 - decodes the layout (+0x74, size +0x78) into +0x94, position 0, scale
	// 0x1000, +0x88 = 0x400, the first play (0x785660) and the battle models' shadows (0x785620), next state
	uint32_t __cdecl te_7855C0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0;
		U16(a1, 0x20) = 0;
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		U16(a1, 0x88) = 0x400;
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		a_73C280(a1);
		FX_HELD(held_note_draw(HK_SHADOW, a1);)
		te_785620();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x785620 (sub_785620): the shadows of the battle entities 3..6 whose flags +0 have bit 1 (BattleEntity_DrawShadow
	// 0x5088A0, OT + 0x44, mode 4) into the module sprite arena (cursor 0x25BC394)
	uint32_t __cdecl te_785620()
	{
		uint32_t cur = MEM<uint32_t>(0x25BC394);
		for (uint32_t e = 0x1D97494; e < 0x1D97704; e += 0x9C)
		{
			if (!(U8(e, 0) & 2)) continue;
			cur = x::sub_5088A0(e, MEM<uint32_t>(0x1D8E04C) + 0x44, 4, cur);
			MEM<uint32_t>(0x25BC394) = cur;
		}
		return cur;
	}

	// 0x785790 (sub_785790): the prim plays' object CALLBACK (cb(layout, record, block)) - an object with no scale and no
	// fade-out is skipped; header 0x68: the object's model (layout data + [data + 8 + 4 * object]), its vertex frame (f0 / f1
	// equal or blend 0 / 0x1000, else MAG_017_sub_701390 into the block's morph buffer +0x48); rotation from the record angles
	// (flag 0x400: ZYX 0x701430, 0x40000: YXZ 0x7015B0, else 0x701310); flag 0x200: the offset as the translation, else the
	// offset turned by the block matrix (GTE) and the rotation composed with it (0x56C270); + the block translation; the scale
	// (flag 0x100: 3x3 product, else scale3DMatrix); flags 0x2030 (0x4000: 0x2000; 0x2000: | 0xC; a fade: | 0xC0 and its
	// colour); depth = the block +0x58 (- 0x100 for object 2 when the block +0x56 is 0); both windows 256 x 256; the module
	// renderer 0x785A90 into the frame arena 0x1D8E054
	uint32_t __cdecl te_785790(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t r = a2;
		if (((int32_t)S16(r, 0x1C) | S32(r, 0x18)) == 0)
			return 0; // void
		if (S16(r, 0x24) >= 0x1000 && U32(r, 0x20) == 0)
			return 0; // void
		uint8_t sp[0x48];
		const uint32_t s = P(sp);
		const uint32_t h = x::Field_Alloc(0x68);
		const uint32_t blk = a3;
		const uint32_t data = U32(a1, 0);
		const uint32_t model = data + U32(data, (uint32_t)(int32_t)S16(r, 2) * 4 + 8);
		const int16_t f1 = S16(r, 0x2A), f0 = S16(r, 0x28);
		U32(h, 0) = model;
		auto frame = [&](int16_t f) { U32(h, 4) = f == 0 ? model + 0xC : model + (uint32_t)mul32(S32(model, 4), f) * 8 + 0xC; };
		if (f0 == f1) frame(f0);
		else
		{
			const int16_t t = S16(r, 0x26);
			if (t == 0) frame(f0);
			else if (t == 0x1000) frame(f1);
			else
			{
				x::MAG_017_sub_701390(model, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, U32(blk, 0x48));
				U32(h, 4) = U32(blk, 0x48);
			}
		}
		const uint32_t R = s + 0x28;   // matrix (rotation +0, translation +0x14)
		memset(sp + 0x28, 0, 0x20);    // (pad +0x12 never written in the original: GTE R33 word only)
		uint32_t flags = U32(r, 4);
		if (flags & 0x400) x::MAG_117_sub_701430(r + 0x10, R);
		else if (flags & 0x40000) gx::MAG_063_sub_7015B0(r + 0x10, R);
		else x::MAG_017_sub_701310(r + 0x10, R);
		flags = U32(r, 4);
		U16(s, 0) = U16(r, 8);
		U16(s, 2) = U16(r, 0xA);
		U16(s, 4) = U16(r, 0xC); // (word +6 never written: GTE VZ0 high half only)
		int32_t tx, ty, tz;
		if (flags & 0x200)
		{
			tx = S16(s, 0);
			ty = S16(s, 2);
			tz = S16(s, 4);
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(s);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(R + 0x14);
			x::GTE_MatrixMultiply(blk, R);
			tx = S32(R, 0x14);
			ty = S32(R, 0x18);
			tz = S32(R, 0x1C);
		}
		S32(R, 0x14) = add32(tx, S32(blk, 0x14));
		S32(R, 0x18) = add32(ty, S32(blk, 0x18));
		S32(R, 0x1C) = add32(tz, S32(blk, 0x1C));
		if (!(U32(r, 0x18) == 0x10001000 && S16(r, 0x1C) == 0x1000))
		{
			if (U32(r, 4) & 0x100)
			{
				const uint32_t D = s + 8;
				U16(D, 0) = U16(r, 0x18);
				U16(D, 8) = U16(r, 0x1A);
				U16(D, 0xA) = 0;
				U16(D, 4) = 0;
				U16(D, 0xE) = 0;
				U16(D, 2) = 0;
				U16(D, 0xC) = 0;
				U16(D, 6) = 0;
				U16(D, 0x10) = U16(r, 0x1C);
				x::sub_56C220(R, D);
			}
			else
			{
				S32(s, 8) = S16(r, 0x18);
				S32(s, 0xC) = S16(r, 0x1A);
				S32(s, 0x10) = S16(r, 0x1C);
				x::scale3DMatrix(R, s + 8);
			}
		}
		x::GTE_SetRotMatrix_W(R);
		x::GTE_SetTransVector_W(R);
		const uint32_t fl = U32(r, 4);
		U32(h, 0x14) = (fl & 0x4000) ? 0x2000u : 0x2030u;
		if (fl & 0x2000) U32(h, 0x14) |= 0xC;
		S32(h, 0xC) = S16(r, 0x24);
		if (S32(h, 0xC) != 0)
		{
			U32(h, 0x14) |= 0xC0;
			U32(h, 8) = U32(r, 0x20);
		}
		if (U16(blk, 0x56) == 0 && S16(r, 2) == 2) S32(h, 0x10) = S16(blk, 0x58) - 0x100;
		else S32(h, 0x10) = S16(blk, 0x58);
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		MEM<uint32_t>(0x1D8E054) = te_785A90(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 te_786070, FT4 te_7864F0,
	// a_73D770, a_73D9C0, 0x73DCA0, te_7874A0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl te_785A90(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = te_785BC0(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_785DE0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_786070(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_7864F0(ctx, a2, a3, cur);
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
			cur = te_786FA0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return te_7874A0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl te_785BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_785DE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_786070(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_7864F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_786FA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_7874A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x787AF0 (sub_787AF0): prim-play task state 1 - cut 6 current: finished; else the shadows, then the play (0x785660):
	// finished when it played its last frame
	uint32_t __cdecl te_787AF0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_SHADOW, a1);)
		te_785620();
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		if (a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x73F250 (module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl te_787B50(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x787BB0, 0x787BE0, 0x787C90, 0x787E10 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x787BB0 (sub_787BB0): orbiting play state 0 - wait 8 ticks (+0x8A == 0) or 0xE ticks (the second one)
	uint32_t __cdecl te_787BB0(uint32_t a1)
	{
		if (U16(a1, 0x8A) == 0)
		{
			if (S16(a1, 0x24) >= 8) U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		else if (S16(a1, 0x24) >= 0xE)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x787BE0 (sub_787BE0): orbiting play state 1 - decodes the layout, scale 0x1000, the orbit (+0x2CC.. centre, +0x2D4
	// tick, +0x2D6 angle / +0x2D8 step / +0x2DA radius by +0x8A, +0x2C4 tilt 0x280, +0x2C6 yaw 0xC00), next state
	uint32_t __cdecl te_787BE0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		U16(a1, 0x2CC) = 0xF500;
		U16(a1, 0x2CE) = 0xF400;
		U16(a1, 0x2D0) = 0;
		if (U16(a1, 0x8A) == 0)
		{
			U16(a1, 0x2D6) = 0;
			U16(a1, 0x2D8) = 0xE0;
			U16(a1, 0x2DA) = 0xFB00;
		}
		else
		{
			U16(a1, 0x2D6) = 0x800;
			U16(a1, 0x2D8) = 0x120;
			U16(a1, 0x2DA) = 0xFA40;
		}
		U16(a1, 0x2D4) = 0;
		U16(a1, 0x2C6) = 0xC00;
		U16(a1, 0x2C4) = 0x280;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x787C90 (sub_787C90): orbiting play state 2 - from tick 0x1A the tilt +0x2C4 falls by 0x40 to 0; the orbit centre
	// +0x2CC.. moves by (0, 0, radius +0x2DA) through yaw / tilt (/ 16); the angle +0x2D6 advances by +0x2D8; the play's
	// position = centre + (0, -0xC0, 0) turned by the angle (z) and the yaw / tilt, +0x62 = yaw; cut 6 or the last frame
	// of the play: finished
	uint32_t __cdecl te_787C90(uint32_t a1)
	{
		uint8_t sp[0x38];
		const uint32_t s = P(sp);
		U16(a1, 0x2D4) = (uint16_t)(U16(a1, 0x2D4) + 1);
		if (S16(a1, 0x2D4) >= 0x1A)
		{
			U16(a1, 0x2C4) = (uint16_t)(U16(a1, 0x2C4) - 0x40);
			if (S16(a1, 0x2C4) <= 0) U16(a1, 0x2C4) = 0;
		}
		const uint16_t r = U16(a1, 0x2DA);
		U16(s, 0) = 0;
		U16(s, 2) = 0;
		U16(s, 4) = r; // (word +6 never written: matrixMultiplyVector reads 3)
		x::MAG_022_sub_8DD770(s + 0x18);
		x::MAG_022_sub_8DD8A0(s + 0x18, (uint32_t)(int32_t)S16(a1, 0x2C6));
		x::sub_8DD7E0(s + 0x18, (uint32_t)(int32_t)S16(a1, 0x2C4));
		x::matrixMultiplyVector(s + 0x18, s, a1 + 0x68);
		U16(s, 8) = 0;
		U16(s, 0xA) = 0xFF40;
		U16(s, 0xC) = 0; // (word +0xE never written)
		U16(a1, 0x2CC) = (uint16_t)(U16(a1, 0x2CC) + (uint16_t)(S16(a1, 0x68) / 16));
		U16(a1, 0x2CE) = (uint16_t)(U16(a1, 0x2CE) + (uint16_t)(S16(a1, 0x6A) / 16));
		U16(a1, 0x2D0) = (uint16_t)(U16(a1, 0x2D0) + (uint16_t)(S16(a1, 0x6C) / 16));
		const uint16_t ang = (uint16_t)((U16(a1, 0x2D8) + U16(a1, 0x2D6)) & 0xFFF);
		U16(a1, 0x2D6) = ang;
		x::sub_8DD960(s + 0x18, (uint32_t)(int32_t)(int16_t)ang);
		x::matrixMultiplyVector(s + 0x18, s + 8, s + 0x10);
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x2CC) + U16(s, 0x10));
		U16(a1, 0x1E) = (uint16_t)(U16(s, 0x12) + U16(a1, 0x2CE));
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x2D0) + U16(s, 0x14));
		U16(a1, 0x62) = U16(a1, 0x2C6);
		if (a_73B7E0(6) == 0)
		{
			// 30 fps layer: see mag085_the_end_held.inc
			FX_HELD(held_note_draw(HK_PLAY, a1);)
			if (a_73C280(a1) != 0)
				return 0; // void
		}
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x787E90 (sub_787E90): stage fade state 0 - from tick 0: +0x1C = 0, the four stage draw entries' level words
	// (0x1D98992 + 0x2C k) and colour bytes (+0x26..+0x28) cleared, next state
	uint32_t __cdecl te_787E90(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0)
			return 0; // void
		U16(a1, 0x1C) = 0;
		uint32_t e = 0x1D989BA;
		for (int k = 0; k < 4; k++, e += 0x2C)
		{
			U16(e, -0x28) = 0;
			U8(e, 0) = 0;
			U8(e, -1) = 0;
			U8(e, -2) = 0;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x787ED0 (sub_787ED0): stage fade state 1 - the level +0x1C rises by 0x100 to 0x1000 (then 0x787F10 and next state);
	// the four stage level words = the level
	uint32_t __cdecl te_787ED0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x1000)
		{
			U16(a1, 0x1C) = 0x1000;
			a_746A10();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		const uint16_t v = U16(a1, 0x1C);
		for (uint32_t e = 0x1D98992, k = 0; k < 4; k++, e += 0x2C)
			MEM<uint16_t>(e) = v;
		return 0; // void
	}

	// 0x787F30 (sub_787F30): stage fade state 2 - cut 6 current: 0x787F50, next state
	uint32_t __cdecl te_787F30(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			a_746AE0();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl te_787F80(uint32_t a1)
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

	// 0x788040 (sub_788040): wobble state 0 - from tick 0: +0x1C = 0, next state
	uint32_t __cdecl te_788040(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0)
		{
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x788060 (sub_788060): wobble state 1 - the amplitude +0x1C rises by 0x200 to 0x1000 (then 0x7880A0 and next state);
	// 0x7880E0 (amplitude, 1)
	uint32_t __cdecl te_788060(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
		if (S16(a1, 0x1C) >= 0x1000)
		{
			U16(a1, 0x1C) = 0x1000;
			te_7880A0();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		te_7880E0((uint32_t)U16(a1, 0x1C), 1);
		return 0; // void
	}

	// 0x7B5750 (sub_7B5750): for the battle entities 0..2 (0x1D972C0, stride 0x9C, 3 entries) with flag
	// bit 1: saves the flag word to director +0x20[i] and sets flag bit 2
	uint32_t __cdecl te_7880A0(void)
	{
		uint32_t save = MEM<uint32_t>(0x156C7F0) + 0x20;
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

	// 0x733910 (module 096 sub_733910): for each of the 4 battle entities 0x1D97494 + k*0x9C with
	// flag bit1 set: sets flag 0x800 and applies level a1 / mode a2 to it (a_733950).
	uint32_t __cdecl te_7880E0(uint32_t a1, uint32_t a2)
	{
		for (uint32_t ent = 0x1D972C0; ent < 0x1D97494; ent += 0x9C)
		{
			uint16_t flags = U16(ent, 0);
			if ((flags & 2) != 0)
			{
				flags = (uint16_t)(flags | 0x800);
				U16(ent, 0) = flags;
				a_733950(ent, a1, a2);
			}
		}
		return 0; // void
	}

	// 0x788220 (sub_788220): wobble state 2 - cut 6 current: 0x788250, +0x1C = 0x1000, next state
	uint32_t __cdecl te_788220(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			te_788250();
			U16(a1, 0x1C) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7B5930 (sub_7B5930): restores flag bit 2 of the battle entities 0..2 (flag bit 1 set) from
	// the words saved by 0x7B5750
	uint32_t __cdecl te_788250(void)
	{
		uint32_t save = MEM<uint32_t>(0x156C7F0) + 0x20;
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

	// 0x788290 (sub_788290): wobble state 3 - the amplitude falls by 0x100 to 0 (finished); 0x7880E0 (amplitude, 1)
	uint32_t __cdecl te_788290(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			U16(a1, 0x1C) = 0;
		}
		te_7880E0((uint32_t)U16(a1, 0x1C), 1);
		return 0; // void
	}

	// 0x788350 (sub_788350): music fade state 0 - from tick 0: volume +0x1C = 0x7F, next state
	uint32_t __cdecl te_788350(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0)
		{
			U16(a1, 0x1C) = 0x7F;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x788370 (sub_788370): music fade state 1 - the volume falls by 8 to 0x30 (then next state); the music volume
	// (Music_SetVolumeImmediate(0, volume))
	uint32_t __cdecl te_788370(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) - 8);
		if (S16(a1, 0x1C) <= 0x30)
		{
			U16(a1, 0x1C) = 0x30;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(a1, 0x1C));
		return 0; // void
	}

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73B7E0(6), then advances the state index +0x29
	uint32_t __cdecl te_7883B0(uint32_t a1)
	{
		const uint32_t r = a_73B7E0(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x733780 (module 096 sub_733780): music fade-in state - volume +0x1C += 8 up to 0x7F (then
	// finished, next state); sets the music volume every tick (Music_SetVolumeImmediate(0, vol)).
	uint32_t __cdecl te_7883D0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 8);
		if (S16(a1, 0x1C) >= 0x7F)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x7F;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(a1, 0x1C));
		return 0; // void
	}

	// 0x872980 (sub_872980): ring state 0 - waits for frame 0x10, next state
	uint32_t __cdecl te_788480(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x10)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1; // void (eax = node)
	}

	// 0x788490 (sub_788490): meteor emitter state - spawns the meteor tasks 0x788580 (queue 0x25BC898, 0xE4) with their kind
	// +0xDE: spawn 0 kind 0, spawn 8 kind 1, spawn 0xE kind 2, every other call kind 0x7F; cut 3 current: finished
	uint32_t __cdecl te_788490(uint32_t a1)
	{
		const int32_t n = S16(a1, 0xDC);
		uint16_t kind;
		if (n == 0) kind = 0;
		else if (n == 8) kind = 1;
		else if (n == 0xE) kind = 2;
		else
		{
			if (a_73B7E0(3) != 0)
			{
				U8(a1, 0x26) |= 1;
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
				U16(a1, 0xDC) = (uint16_t)(U16(a1, 0xDC) + 1);
				return 0; // void
			}
			kind = 0x7F;
		}
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25BC898, 0x788580, 0xE4, a1);
		U16(a1, 0xDC) = (uint16_t)(U16(a1, 0xDC) + 1);
		U16(t, 0xDE) = kind;
		return 0; // void
	}

	// 0x788580 (sub_788580): METEOR task - states {0x788810 set-up, 0x788960 fall, 0x78C4A0 ret}; then the swing +0xB4 +=
	// 0x20 (its tilt +0x94 = sin * 0x300 / 4096), the step (0, +0xD4, 0) turned by the yaw +0x96 / tilt moves +0x54.. (/ 16),
	// the position +0x1C.. = +0x54.., the scale +0x30.. from the height (y >= 0: 2y + 0x1000 up to 0x4000, else y + 0x1000
	// down to 0x800); draw (0x788700)
	uint32_t __cdecl te_788580(uint32_t a1)
	{
		uint8_t sp[0x2C];
		const uint32_t s = P(sp);
		uint32_t states[3];
		states[0] = 0x788810;
		states[1] = 0x788960;
		states[2] = 0x78C4A0; // nullsub (ret)
		callp(states[S8(a1, 0x29)], a1);
		const uint16_t sw = (uint16_t)((U16(a1, 0xB4) + 0x20) & 0xFFF);
		U16(a1, 0xB4) = sw;
		const int32_t sn = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)sw);
		U16(s, 0) = 0;
		U16(s, 4) = 0;
		U16(a1, 0x94) = (uint16_t)(mul32(sn, 0x300) / 4096);
		U16(s, 2) = U16(a1, 0xD4); // (word +6 never written: matrixMultiplyVector reads 3)
		x::MAG_022_sub_8DD770(s + 0xC);
		x::MAG_022_sub_8DD8A0(s + 0xC, (uint32_t)(int32_t)S16(a1, 0x96));
		x::sub_8DD7E0(s + 0xC, (uint32_t)(int32_t)S16(a1, 0x94));
		x::matrixMultiplyVector(s + 0xC, s, a1 + 0x74);
		U16(a1, 0x54) = (uint16_t)(U16(a1, 0x54) + (uint16_t)(S16(a1, 0x74) / 16));
		U16(a1, 0x56) = (uint16_t)(U16(a1, 0x56) + (uint16_t)(S16(a1, 0x76) / 16));
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + (uint16_t)(S16(a1, 0x78) / 16));
		U32(a1, 0x1C) = U32(a1, 0x54);
		U32(a1, 0x20) = U32(a1, 0x58);
		const int32_t y = S16(a1, 0x20);
		if (y >= 0)
		{
			S32(a1, 0x38) = 2 * y + 0x1000;
			if (S32(a1, 0x38) > 0x4000) S32(a1, 0x38) = 0x4000;
		}
		else
		{
			S32(a1, 0x38) = y + 0x1000;
			if (S32(a1, 0x38) < 0x800) S32(a1, 0x38) = 0x800;
		}
		S32(a1, 0x34) = S32(a1, 0x38);
		S32(a1, 0x30) = S32(a1, 0x38);
		te_788700(a1);
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		return task_end(a1, status);
	}

	// 0x788700 (sub_788700): a model of the module (definition +0x4C) drawn unless hidden (+0x26 bit 2): the camera rotation
	// (UnpackRotationMatrix 0x1D97778) turned by +0x48 (z), at +0x1C.., scaled by +0x30.., through the camera; header 0x68
	// (+8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0, both windows 256 x 256) through the module renderer
	// 0x785A90 into the frame arena 0x1D8E054
	uint32_t __cdecl te_788700(uint32_t a1)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		// 30 fps layer: see mag085_the_end_held.inc
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
		MEM<uint32_t>(0x1D8E054) = te_785A90(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x788810 (sub_788810): meteor state 0 - model 0x156C7FC, scale / fade 0x1000; by kind +0xDE: 0 / 1 / 2 a fixed start
	// (swing / yaw / position), else random (swing, yaw rand & 0xFFF, x -0x1000..0xFFF, y -0x400 - rand & 0xFFF,
	// z -0x1400..0xBFF); step +0xD4 = 0x400; next state
	uint32_t __cdecl te_788810(uint32_t a1)
	{
		const int32_t kind = S16(a1, 0xDE);
		U32(a1, 0x4C) = 0x156C7FC;
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U16(a1, 0x50) = 0x1000;
		if (kind == 0)
		{
			U16(a1, 0xB4) = 0;
			U16(a1, 0xD4) = 0x400;
			U16(a1, 0x96) = 0x400;
			U16(a1, 0x54) = 0xF900;
			U16(a1, 0x56) = 0xFA00;
			U16(a1, 0x58) = 0x200;
		}
		else if (kind == 1)
		{
			U16(a1, 0xD4) = 0x400;
			U16(a1, 0xB4) = 0;
			U16(a1, 0x96) = 0xC00;
			U16(a1, 0x54) = 0x500;
			U16(a1, 0x56) = 0xFA00;
			U16(a1, 0x58) = 0;
		}
		else if (kind == 2)
		{
			U16(a1, 0x54) = 0xFC80;
			U16(a1, 0xD4) = 0x400;
			U16(a1, 0xB4) = 0x400;
			U16(a1, 0x96) = 0x400;
			U16(a1, 0x56) = 0xFA00;
			U16(a1, 0x58) = 0;
		}
		else
		{
			U16(a1, 0xD4) = 0x400;
			U16(a1, 0xB4) = (uint16_t)(x::CrtRand() & 0xFFF);
			U16(a1, 0x96) = (uint16_t)(x::CrtRand() & 0xFFF);
			U16(a1, 0x54) = (uint16_t)((x::CrtRand() & 0x1FFF) - 0x1000);
			U16(a1, 0x56) = (uint16_t)(0xFFFFFC00u - (x::CrtRand() & 0xFFF));
			U16(a1, 0x58) = (uint16_t)((x::CrtRand() & 0x1FFF) - 0x1400);
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x788960 (sub_788960): meteor state 1 - cut 3 current: finished; once it reaches the ground (y +0x1E >= 0): y = 0 and
	// for kinds 0..2 the impact (prim play 0x788A90, layout 0x1715D28; the actor system 0x788B90, data 0x17151EC, mode 0,
	// 0xF, 3, at the impact point) then finished and hidden; else (kinds > 2) finished and hidden; while falling the fade
	// +0x50 falls by 0x200 to 0 up to tick 0x20, then rises by 0x200 to 0x1000 (finished and hidden)
	uint32_t __cdecl te_788960(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (S16(a1, 0x1E) >= 0)
		{
			const bool impact = S16(a1, 0xDE) <= 2;
			U16(a1, 0x1E) = 0;
			if (impact)
			{
				te_785510(a1, 0x788A90, 0x1715D28, 0x230, 0, 0);
				const uint32_t t = a_73C100(a1, 0x788B90, 0x17151EC, 0, 0xF, 3);
				U32(t, 0x288) = U32(a1, 0x1C);
				U32(t, 0x28C) = U32(a1, 0x20);
			}
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (S16(a1, 0x24) <= 0x20)
		{
			U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
			if (S16(a1, 0x50) < 0) U16(a1, 0x50) = 0;
			return 0; // void
		}
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) < 0x1000)
			return 0; // void
		U16(a1, 0x50) = 0x1000;
		U8(a1, 0x26) |= 5;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x788AF0 (sub_788AF0): impact play state 0 - from tick 5: decodes the layout, scale 0x1000, the first play (0x785660),
	// next state
	uint32_t __cdecl te_788AF0(uint32_t a1)
	{
		if (S16(a1, 0x24) < 5)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		a_73C280(a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x766760 (module 090 sub_766760): state "wait": when a_73B7E0(3) is set, or else when
	// a_73C280(node) returns 0, marks the node finished (+0x26 |= 1) and advances the state (+0x29).
	uint32_t __cdecl te_788B40(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_73B7E0(3) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return node;   // eax = node
		}
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		const uint32_t r = a_73C280(node);
		if (r == 0)
		{
			const uint8_t st = (uint8_t)(U8(node, 0x29) + 1);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = st;
			return st;     // eax = 0 with al = new state
		}
		return r;
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl te_788BF0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			te_789700(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl te_788DD0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25C21DC);
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
	uint32_t __cdecl te_789050(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25C21DC);
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
	uint32_t __cdecl te_789700(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25C21DC, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			te_78C3E0();
			te_789A90();
			// 30 fps layer: see mag085_the_end_held.inc
			FX_HELD(held_note_system(sys);)
			te_789950();
			te_789790();
			sys = U32(0x25C21DC, 0);
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
		U16(0x25C21B0, 0) = (uint16_t)(U16(0x25C21B0, 0) + c14);
		U16(0x25C1CB4, 0) = (uint16_t)(U16(0x25C1CB4, 0) + c16);
		return done;
	}

	// 0x896E70 (copy of rt_8A4670 0x8A4670: copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl te_789790(void)
	{
		uint32_t act = U32(U32(0x25C21DC, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25C21DC, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					te_7897F0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x896ED0 (copy of rt_8A46D0 0x8A46D0: copy of dm_8C9530 0x8C9530: copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl te_7897F0(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = te_785A90(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = te_785A90(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
	uint32_t __cdecl te_789950(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25C21DC), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25C21DC);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						te_789A10(act);   // (the original also pushes the bone entry, unused)
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
							te_789A10(act);
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
	uint32_t __cdecl te_789A10(uint32_t a1)
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
	uint32_t __cdecl te_789A90(void)
	{
		uint32_t act = U32(U32(0x25C21DC, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				te_78AD60(act);
			else if (type == 1)
				te_789AD0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9810 (copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl te_789AD0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25C21DC);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (te_78AC10(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25C21DC);
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
	uint32_t __cdecl te_78AB00(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25C21DC, 0);                  // actor state block
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
	uint32_t __cdecl te_78AC10(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				te_78AC90(a1, a2);
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
			te_78AC90(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			te_78AC90(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x8CA9D0 (copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl te_78AC90(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return te_78ACC0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x78ACC0 (sub_78ACC0): a free actor slot of the pool [0x25C1CB8] (17 x 0x6C, in use: +0x69), from the round-robin
	// index 0x25BC890 (18 tries): cleared, +0x5C = a1, +0x6A / +0x6B = a2 / a3, in use, the pool header count
	// [0x25C21DC] + 0x14 + 1, 0x78BEF0 (slot, 0); the index moves past it; returns the slot (0: none)
	uint32_t __cdecl te_78ACC0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25C1CB8);
		int32_t i = MEM<int16_t>(0x25BC890);
		uint32_t slot = 0;
		for (int32_t k = 0; k < 0x12; k++)
		{
			const uint32_t r = pool + (uint32_t)i * 0x6C;
			if (U8(r, 0x69) == 0)
			{
				slot = r;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				U8(slot, 0x69) = 1;
				U16(MEM<uint32_t>(0x25C21DC), 0x14) = (uint16_t)(U16(MEM<uint32_t>(0x25C21DC), 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			i++;
			if (i >= 0x11) i = 0;
		}
		i++;
		if (i >= 0x11) MEM<uint16_t>(0x25BC890) = 0;
		else MEM<uint16_t>(0x25BC890) = (uint16_t)i;
		return slot;
	}

	// 0x8CAAA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl te_78AD60(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x78B710, a1);
		else if (mode == 4)
			callp(0x78B860, a1);
		te_78ADE0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8CAB20 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl te_78ADE0(uint32_t a1)
	{
		uint32_t sys = U32(0x25C21DC, 0);
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
				te_78AE60(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			te_78AE60(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8CABA0 (copy of mb_828F00 0x828F00: copy of db_84A280 0x84A280: copy of cg_8818A0 0x8818A0: sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl te_78AE60(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25C21CC, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25C21CC, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = te_78B640(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25C21DC, 0);
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
		te_78C350(node, desc);
		U32(0x25C21CC, 0) = U32(0x25C21CC, 0) + 0x50;
		return 0; // void
	}

	// 0x8BC590 (sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x3C records
	uint32_t __cdecl te_78B640(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25C21D0);
		int32_t i = MEM<int16_t>(0x25C1CC4);
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
			if (i >= 0x3B)
				i = 0;
			tries++;
			if (tries >= 0x3C)
				break;
		}
		i++;
		if (i >= 0x3B)
			MEM<uint16_t>(0x25C1CC4) = 0;
		else
			MEM<uint16_t>(0x25C1CC4) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl te_78B860(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25C21DC, 0);            // ecx
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
				uint32_t g = U32(0x25C21DC, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25C21DC, 0);
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
	uint32_t __cdecl te_78C350(uint32_t a1, uint32_t a2)
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
			te_78ACC0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			te_78ACC0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			te_78ACC0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			te_78ACC0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x8CC120 (copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via te_78ACC0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl te_78C3E0(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25C21DC);  // eax (re-read only after the calls)
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
					te_78ACC0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25C21DC);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				te_78ACC0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25C21DC);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl te_78C470(uint32_t a1)
	{
		if (te_789700(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x78C4C0 (sub_78C4C0): GROUND task (flowers) - states {0x78CE00 set-up, 0x78CE30 wait, 0x78CE70 ret}, then the
	// ground model draw 0x78C530 (fade level +0xE0)
	uint32_t __cdecl te_78C4C0(uint32_t a1)
	{
		uint32_t states[3];
		states[0] = 0x78CE00;
		states[1] = 0x78CE30;
		states[2] = 0x78CE70; // nullsub (ret)
		callp(states[S8(a1, 0x29)], a1);
		te_78C530(a1, (uint32_t)U16(a1, 0xE0));
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		return task_end(a1, status);
	}

	// 0x78C530 (sub_78C530): the ground model (definition +0x4C) drawn unless hidden (+0x26 bit 2): rotation y +0x46, x +0x44,
	// z +0x48, at +0x1C.., scaled by +0x30.., through the camera; header 0x68 as in 0x788700 plus the fade level +0x64 = a2,
	// through the ground renderer 0x78C660 into the frame arena 0x1D8E054
	uint32_t __cdecl te_78C530(uint32_t a1, uint32_t a2)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_GROUND, a1);)
		uint8_t m[0x20];
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
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		U16(h, 0x18) = 0;
		S32(h, 0x10) = S16(a1, 0x52);
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x64) = (uint16_t)a2;
		U32(h, 0x14) = 0xF0;
		MEM<uint32_t>(0x1D8E054) = te_78C660(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x78C660 (sub_78C660): the ground renderer - vertices = model + 8 unless given (flags +0x14 bit 0x2000), the list =
	// model + [model] + 0x1C, the far colour = header +8..+0xA; a non-empty list goes to 0x78C6D0, else the cursor stays
	uint32_t __cdecl te_78C660(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		if (!(U32(a1, 0x14) & 0x2000))
			U32(a1, 4) = U32(a1, 0) + 8;
		U32(a1, 0x2C) = U32(a1, 0) + U32(U32(a1, 0), 0);
		x::someCameraWork_45DD60(U8(a1, 8), U8(a1, 9), U8(a1, 0xA));
		U32(a1, 0x2C) += 0x1C;
		if (U32(U32(a1, 0x2C), 0) != 0)
			return te_78C6D0(a1, a2, a3, a4);
		U32(a1, 0x2C) += 4;
		return a4;
	}

	// 0x78C6D0 (sub_78C6D0): the ground gouraud-textured quads (0x24-byte records -> POLY_GT4): RTPT / RTPS, backface cull
	// (NCLIP < 0 unless flags bit 0x20), screen clip codes (a quad wholly left / right / above / below is dropped), colours
	// depth-cued (flags bit 0x80) or from the record, OTZ + bias (>= 0); with a UV scroll (+0x18 / +0x1A) the four u / v
	// bytes scroll (wrapped by the window size bytes +0x28 / +0x2A) and the quad goes between two texture-window packets
	// (windows +0x1C / +0x24); without one the colours become grey levels (byte - fade +0x64, 0..8, << 4) and a quad with a
	// level below 0 is dropped. Returns the packet cursor.
	uint32_t __cdecl te_78C6D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pk = a4;
		uint32_t rec = U32(ctx, 0x2C);
		const int32_t n = S32(rec, 0);
		rec += 4;
		U32(ctx, 0x2C) = rec;
		const uint32_t verts = U32(ctx, 4);
		if (n <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pk;
		}
		for (int32_t k = 0; k < n; k++, rec += 0x24)
		{
			x::GTE_LoadV012(verts + ((uint32_t)U16(rec, 4) << 2), verts + ((uint32_t)U16(rec, 6) << 2), verts + ((uint32_t)U16(rec, 8) << 2));
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			U32(pk, 0) = 0x0C000000;
			U32(pk, 4) = U32(rec, 0);
			if (flags & 2) U32(pk, 4) |= 0x2000000;
			if (flags & 8) U32(pk, 4) &= 0xFDFFFFFF;
			U32(pk, 0xC) = U32(rec, 0xC);
			const uint32_t uv23 = U32(rec, 0x14);
			U32(pk, 0x24) = uv23;
			U32(pk, 0x18) = U32(rec, 0x10);
			U32(pk, 0x30) = uv23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (U32(ctx, 0x3C) & 0x60000) continue;
			x::GTE_NCLIP();
			uint32_t clip = 0;
			x::GTE_ReadMAC0(ctx + 0x30);
			if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x20)) continue;
			x::GTE_ReadSXY012_Split(pk + 8, pk + 0x14, pk + 0x20);
			x::GTE_LoadV0(verts + ((uint32_t)U16(rec, 0xA) << 2));
			x::GTE_RTPS();
			if (te_offx(S16(pk, 8))) clip = 1;
			if (te_offx(S16(pk, 0x14))) clip |= 2;
			if (te_offx(S16(pk, 0x20))) clip |= 4;
			if (te_offy(S16(pk, 0xA))) clip |= 0x10;
			if (te_offy(S16(pk, 0x16))) clip |= 0x20;
			if (te_offy(S16(pk, 0x22))) clip |= 0x40;
			x::GTE_ReadSXY2(pk + 0x2C);
			x::GTE_AVSZ4();
			if (te_offx(S16(pk, 0x2C))) clip |= 8;
			if (te_offy(S16(pk, 0x2E))) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x80)
			{
				x::sub_45E120(rec + 0x18, rec + 0x1C, rec + 0x20);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F4C0();
				x::sub_45E370(pk + 0x10, pk + 0x1C, pk + 0x28);
				x::set_unk_1CA8A28(pk + 4);
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x18);
				U32(pk, 0x1C) = U32(rec, 0x1C);
				U32(pk, 0x28) = U32(rec, 0x20);
			}
			S32(ctx, 0x38) = add32(S32(ctx, 0x38), S32(ctx, 0x10));
			if (S32(ctx, 0x38) < 0) S32(ctx, 0x38) = 0;
			const uint32_t bucket = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
			const uint16_t us = U16(ctx, 0x18), vs = U16(ctx, 0x1A);
			if ((uint16_t)(us | vs) != 0)
			{
				if (us != 0) te_scroll(pk, 0, us, U8(ctx, 0x28));
				if (vs != 0) te_scroll(pk, 1, vs, U8(ctx, 0x2A));
				const uint32_t w1 = pk + 0x34;
				U32(w1, 0) = 0x2000000;
				U32(w1, 4) = te_texwin(ctx, 0x1C);
				U32(w1, 8) = 0;
				x::SSIGPU_InsertPrimAutoDepth(bucket, w1);
				x::SSIGPU_InsertPrimAutoDepth(bucket, pk);
				const uint32_t w2 = pk + 0x40;
				U32(w2, 0) = 0x2000000;
				U32(w2, 4) = te_texwin(ctx, 0x24);
				U32(w2, 8) = 0;
				x::SSIGPU_InsertPrimAutoDepth(bucket, w2);
				pk += 0x4C;
				continue;
			}
			const int16_t f = S16(ctx, 0x64);
			int16_t l0 = (int16_t)(U8(pk, 4) - f), l1 = (int16_t)(U8(pk, 0x10) - f), l2 = (int16_t)(U8(pk, 0x1C) - f), l3 = (int16_t)(U8(pk, 0x28) - f);
			if (l0 < 0 || l1 < 0 || l2 < 0 || l3 < 0) continue;
			if (l0 > 8) l0 = 8;
			if (l1 > 8) l1 = 8;
			if (l2 > 8) l2 = 8;
			if (l3 > 8) l3 = 8;
			const uint8_t g0 = (uint8_t)(l0 << 4), g1 = (uint8_t)(l1 << 4), g2 = (uint8_t)(l2 << 4), g3 = (uint8_t)(l3 << 4);
			U8(pk, 6) = g0; U8(pk, 5) = g0; U8(pk, 4) = g0;
			U8(pk, 0x12) = g1; U8(pk, 0x11) = g1; U8(pk, 0x10) = g1;
			U8(pk, 0x1E) = g2; U8(pk, 0x1D) = g2; U8(pk, 0x1C) = g2;
			U8(pk, 0x2A) = g3; U8(pk, 0x29) = g3; U8(pk, 0x28) = g3;
			x::SSIGPU_InsertPrimAutoDepth(bucket, pk);
			pk += 0x34;
		}
		U32(ctx, 0x2C) = rec;
		return pk;
	}

	// 0x78CE00 (sub_78CE00): ground state 0 - scale 0x1000, model 0x156C8BC, fade level +0xE0 = 0x80, next state
	uint32_t __cdecl te_78CE00(uint32_t a1)
	{
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U32(a1, 0x4C) = 0x156C8BC;
		U16(a1, 0xE0) = 0x80;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x78CE30 (sub_78CE30): ground state 1 - the fade level falls by 1 to 0x6C; cut 3 current: finished and hidden
	uint32_t __cdecl te_78CE30(uint32_t a1)
	{
		U16(a1, 0xE0) = (uint16_t)(U16(a1, 0xE0) - 1);
		if (S16(a1, 0xE0) < 0x6C) U16(a1, 0xE0) = 0x6C;
		if (a_73B7E0(3) != 0)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x720790 (module 099 sub_720790): TASK - state machine {7207E0, nop}, task tail (no draw).
	uint32_t __cdecl te_78CE80(uint32_t a1)
	{
		const uint32_t states[2] = { 0x78CED0, 0x78D240 };
		callp(states[S8(a1, 0x29)], a1);
		return task_end(a1);
	}

	// 0x78CED0 (sub_78CED0): petal emitter state - a petal-group task 0x78CFA0 (queue 0x25BC898, 0xE4) with four petals:
	// position +0x54 + 8 k (x -0x4000..0x3FFF, y 0x400 - rand & 0x1FFF (at most 0), z -0x2000..0x5FFF), fall step +0xD4 + 2 k
	// 0x400..0x13FF, swing +0xB8 + 8 k; count +0xDE; cut 6 current: finished
	uint32_t __cdecl te_78CED0(uint32_t a1)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25BC898, 0x78CFA0, 0xE4, a1);
		uint32_t st = t + 0xD4, p = t + 0x56;
		for (int k = 0; k < 4; k++, st += 2, p += 8)
		{
			U16(p, -2) = (uint16_t)((x::CrtRand() & 0x7FFF) - 0x4000);
			const uint16_t y = (uint16_t)(0x400 - (x::CrtRand() & 0x1FFF));
			U16(p, 0) = y;
			if ((int16_t)y > 0) U16(p, 0) = 0;
			U16(p, 2) = (uint16_t)((x::CrtRand() & 0x7FFF) - 0x2000);
			U16(st, 0) = (uint16_t)((x::CrtRand() & 0xFFF) + 0x400);
			U16(p, 0x62) = (uint16_t)(x::CrtRand() & 0xFFF);
		}
		U16(a1, 0xDE) = (uint16_t)(U16(a1, 0xDE) + 1);
		if (a_73B7E0(6) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x78CFA0 (sub_78CFA0): PETAL-GROUP task - states {0x78D150 set-up, 0x78D180 fade in, 0x78D1C0 wait, 0x78D1F0 fade out,
	// 0x78D230 ret}; each of the four petals: its swing +0xB8 + 8 k += 0x40, its tilt +0x98 + 8 k = sin * 256 / 4096 - 0xC0,
	// its step (+0xD4 + 2 k, 0, 0) turned by the tilt (z) into +0x74 + 8 k moves +0x54 + 8 k (/ 16; y kept <= 0); then each
	// petal drawn (0x788700) at its position with the scale from its height (y >= 0: y + 0x1000 up to 0x2000, else down to
	// 0x400)
	uint32_t __cdecl te_78CFA0(uint32_t a1)
	{
		uint8_t sp[0x28];
		const uint32_t s = P(sp);
		uint32_t states[5];
		states[0] = 0x78D150;
		states[1] = 0x78D180;
		states[2] = 0x78D1C0;
		states[3] = 0x78D1F0;
		states[4] = 0x78D230; // nullsub (ret)
		callp(states[S8(a1, 0x29)], a1);
		uint32_t p = a1 + 0x98, st = a1 + 0xD4;
		for (int k = 0; k < 4; k++, st += 2, p += 8)
		{
			const uint16_t sw = (uint16_t)((U16(p, 0x20) + 0x40) & 0xFFF);
			U16(p, 0x20) = sw;
			const int32_t sn = (int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)sw);
			U16(p, 0) = (uint16_t)(shl32(sn, 8) / 4096 - 0xC0);
			U16(s, 0) = U16(st, 0);
			U16(s, 2) = 0;
			U16(s, 4) = 0; // (word +6 never written: matrixMultiplyVector reads 3)
			x::MAG_022_sub_8DD770(s + 8);
			x::sub_8DD960(s + 8, (uint32_t)(int32_t)S16(p, 0));
			const uint32_t v = p - 0x24;
			x::matrixMultiplyVector(s + 8, s, v);
			U16(p, -0x44) = (uint16_t)(U16(p, -0x44) + (uint16_t)(S16(v, 0) / 16));
			U16(p, -0x42) = (uint16_t)(U16(p, -0x42) + (uint16_t)(S16(v, 2) / 16));
			U16(p, -0x40) = (uint16_t)(U16(p, -0x40) + (uint16_t)(S16(v, 4) / 16));
			if (S16(p, -0x42) >= 0) U16(p, -0x42) = 0;
		}
		uint32_t q = a1 + 0x54;
		for (int k = 0; k < 4; k++, q += 8)
		{
			U32(a1, 0x1C) = U32(q, 0);
			U32(a1, 0x20) = U32(q, 4);
			const int32_t y = S16(a1, 0x20);
			if (y >= 0)
			{
				S32(a1, 0x38) = y + 0x1000;
				if (S32(a1, 0x38) > 0x2000) S32(a1, 0x38) = 0x2000;
			}
			else
			{
				S32(a1, 0x38) = y + 0x1000;
				if (S32(a1, 0x38) < 0x400) S32(a1, 0x38) = 0x400;
			}
			S32(a1, 0x34) = S32(a1, 0x38);
			S32(a1, 0x30) = S32(a1, 0x38);
			te_788700(a1);
		}
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		return task_end(a1, status);
	}

	// 0x78D150 (sub_78D150): petal-group state 0 - scale 0x1000, fade +0x50 = 0x1000, model 0x156C85C, next state
	uint32_t __cdecl te_78D150(uint32_t a1)
	{
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U16(a1, 0x50) = 0x1000;
		U32(a1, 0x4C) = 0x156C85C;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x78D180 (sub_78D180): petal-group state 1 - cut 6 current: finished and hidden; the fade falls by 0x200 to 0 (next state)
	uint32_t __cdecl te_78D180(uint32_t a1)
	{
		if (a_73B7E0(6) != 0) { U8(a1, 0x26) |= 5; return 0; } // void
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x78D1C0 (sub_78D1C0): petal-group state 2 - cut 6 current: finished and hidden; next state from tick 0x18
	uint32_t __cdecl te_78D1C0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0) { U8(a1, 0x26) |= 5; return 0; } // void
		if (S16(a1, 0x24) >= 0x18) U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x78D1F0 (sub_78D1F0): petal-group state 3 - cut 6 current: finished and hidden; the fade rises by 0x200 to 0x1000
	// (finished and hidden)
	uint32_t __cdecl te_78D1F0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0) { U8(a1, 0x26) |= 5; return 0; } // void
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) >= 0x1000)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			U16(a1, 0x50) = 0x1000;
		}
		return 0; // void
	}

	// 0x73F250 (module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl te_78D250(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x78D2B0, 0x78D3A0, 0x78D3D0, 0x78D400 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x78D2B0 (sub_78D2B0): screen-glow state 0 - the width +0x58 grows by 8 to 0x80 (next state); the glow quad 0x78D2E0
	uint32_t __cdecl te_78D2B0(uint32_t a1)
	{
		U16(a1, 0x58) = (uint16_t)(U16(a1, 0x58) + 8);
		if (S16(a1, 0x58) >= 0x80)
		{
			U16(a1, 0x58) = 0x80;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		te_78D2E0(a1);
		return 0; // void
	}

	// 0x78D2E0 (sub_78D2E0): the glow quad - a semi-transparent gouraud-textured quad (POLY_GT4 0x3E, tpage 0xB6, clut 0x3D54)
	// x 0xBA..0xBA + width +0x58, y 0xA0..0xC0, uv u 0..width / v 0xE0..0xFF, colours 0x80 on the left, the level +0x5A on the
	// right, at the OT entry +0x14 (alternate viewport), in the frame arena 0x1D8E054
	uint32_t __cdecl te_78D2E0(uint32_t a1)
	{
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_GLOW, a1);)
		const uint32_t pk = MEM<uint32_t>(0x1D8E054);
		U8(pk, 0x24) = 0;
		U8(pk, 0xC) = 0;
		U32(pk, 0) = 0xC000000;
		U8(pk, 0x30) = U8(a1, 0x58);
		U8(pk, 0x18) = U8(a1, 0x58);
		U8(pk, 7) = 0x3E;
		U8(pk, 0x19) = 0xE0;
		U8(pk, 0xD) = 0xE0;
		U16(pk, 0x1A) = 0xB6;
		U8(pk, 0x31) = 0xFF;
		U8(pk, 0x25) = 0xFF;
		U16(pk, 0xE) = 0x3D54;
		const uint16_t x1 = (uint16_t)(U16(a1, 0x58) + 0xBA);
		U16(pk, 0x20) = 0xBA;
		U16(pk, 0x2C) = x1;
		U16(pk, 0x14) = x1;
		U16(pk, 8) = 0xBA;
		U16(pk, 0x16) = 0xA0;
		U16(pk, 0xA) = 0xA0;
		U16(pk, 0x2E) = 0xC0;
		U16(pk, 0x22) = 0xC0;
		U8(pk, 0x1E) = 0x80; U8(pk, 0x1D) = 0x80; U8(pk, 0x1C) = 0x80;
		U8(pk, 6) = 0x80; U8(pk, 5) = 0x80; U8(pk, 4) = 0x80;
		const uint8_t l = U8(a1, 0x5A);
		U8(pk, 0x2A) = l; U8(pk, 0x29) = l; U8(pk, 0x28) = l;
		U8(pk, 0x12) = l; U8(pk, 0x11) = l; U8(pk, 0x10) = l;
		x::SSIGPU_InsertPrimAltViewport(MEM<uint32_t>(0x1D8E04C) + 0x14, pk);
		MEM<uint32_t>(0x1D8E054) = pk + 0x34;
		return pk + 0x34;
	}

	// 0x78D3A0 (sub_78D3A0): screen-glow state 1 - the right level +0x5A rises by 0x10 to 0x80 (next state); the glow quad
	uint32_t __cdecl te_78D3A0(uint32_t a1)
	{
		U16(a1, 0x5A) = (uint16_t)(U16(a1, 0x5A) + 0x10);
		if (S16(a1, 0x5A) >= 0x80)
		{
			U16(a1, 0x5A) = 0x80;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		te_78D2E0(a1);
		return 0; // void
	}

	// 0x78D3D0 (sub_78D3D0): screen-glow state 2 - cut 6 current: finished; else the glow quad
	uint32_t __cdecl te_78D3D0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		te_78D2E0(a1);
		return 0; // void
	}

	// 0x78D480 (sub_78D480): letterbox state 0 - from tick 0x10: bar height +0x1E = 0x18, next state
	uint32_t __cdecl te_78D480(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x10)
		{
			U16(a1, 0x1E) = 0x18;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x78D4A0 (sub_78D4A0): letterbox state 1 - cut 6 current: wait 4 ticks (+0x5C), next state; the bars (0x78D4D0)
	uint32_t __cdecl te_78D4A0(uint32_t a1)
	{
		if (a_73B7E0(6) != 0)
		{
			U16(a1, 0x5C) = 4;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		te_78D4D0(a1, (uint32_t)U16(a1, 0x1E));
		return 0; // void
	}

	// 0x78D4D0 (sub_78D4D0): the letterbox - two black semi-transparent tiles (POLY_F4 0x29) 320 wide: y 0..a2 and
	// 0xD8 - a2..0xD8, at the OT entry +0x14 (alternate viewport), in the frame arena 0x1D8E054
	uint32_t __cdecl te_78D4D0(uint32_t a1, uint32_t a2)
	{
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(held_note_draw(HK_BARS, a1);)
		(void)a1;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x14;
		uint32_t pk = MEM<uint32_t>(0x1D8E054);
		const uint16_t b = (uint16_t)a2;
		U32(pk, 0) = 0x5000000;
		U8(pk, 7) = 0x29;
		U16(pk, 0x10) = 0;
		U16(pk, 8) = 0;
		U16(pk, 0x14) = 0x140;
		U16(pk, 0xC) = 0x140;
		U16(pk, 0xE) = 0;
		U16(pk, 0xA) = 0;
		U16(pk, 0x16) = b;
		U16(pk, 0x12) = b;
		U8(pk, 6) = 0;
		U8(pk, 5) = 0;
		U8(pk, 4) = 0;
		x::SSIGPU_InsertPrimAltViewport(ot, pk);
		pk += 0x18;
		const uint16_t y0 = (uint16_t)(0xD8 - b);
		U16(pk, 0x14) = 0x140;
		U16(pk, 0xC) = 0x140;
		U32(pk, 0) = 0x5000000;
		U8(pk, 7) = 0x29;
		U16(pk, 0x10) = 0;
		U16(pk, 8) = 0;
		U16(pk, 0xE) = y0;
		U16(pk, 0xA) = y0;
		U16(pk, 0x16) = 0xD8;
		U16(pk, 0x12) = 0xD8;
		U8(pk, 6) = 0;
		U8(pk, 5) = 0;
		U8(pk, 4) = 0;
		x::SSIGPU_InsertPrimAltViewport(ot, pk);
		pk += 0x18;
		MEM<uint32_t>(0x1D8E054) = pk;
		return pk;
	}

	// 0x78D590 (sub_78D590): letterbox state 2 - after the wait (+0x5C) next state; the bars
	uint32_t __cdecl te_78D590(uint32_t a1)
	{
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
		if (S16(a1, 0x5C) <= 0) U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		te_78D4D0(a1, (uint32_t)U16(a1, 0x1E));
		return 0; // void
	}

	// 0x78D5C0 (sub_78D5C0): letterbox state 3 - the bars shrink by 4 (gone: finished, no draw)
	uint32_t __cdecl te_78D5C0(uint32_t a1)
	{
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) - 4);
		if (S16(a1, 0x1E) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		te_78D4D0(a1, (uint32_t)U16(a1, 0x1E));
		return 0; // void
	}

	// ---- generated (desc85.py / cls85.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7847C0, (void *)a_73A0D0, "085 MAG_085_sub_7847C0" },
		{ 0x7847D0, (void *)a_73A0D0, "085 MAG_085_sub_7847D0" },
		{ 0x784860, (void *)a_73A170, "085 MAG_085_sub_784860" },
		{ 0x784890, (void *)a_73A1A0, "085 MAG_085_sub_784890" },
		{ 0x784A60, (void *)a_73A380, "085 MAG_085_sub_784A60" },
		{ 0x784AC0, (void *)a_73A3E0, "085 sub_784AC0" },
		{ 0x784C60, (void *)a_73A580, "085 MAG_085_THE_END_RequeueTIMUpload" },
		{ 0x784C90, (void *)a_73A5B0, "085 sub_784C90" },
		{ 0x784CC0, (void *)a_73A5E0, "085 sub_784CC0" },
		{ 0x784CD0, (void *)a_73A5E0, "085 sub_784CD0" },
		{ 0x784CE0, (void *)a_73A5E0, "085 sub_784CE0" },
		{ 0x784CF0, (void *)a_73A5E0, "085 sub_784CF0" },
		{ 0x784D00, (void *)a_73A620, "085 sub_784D00" },
		{ 0x784D40, (void *)a_73A660, "085 sub_784D40" },
		{ 0x784DB0, (void *)a_73A6D0, "085 nullsub_1311" },
		{ 0x784FA0, (void *)a_73B640, "085 sub_784FA0" },
		{ 0x785000, (void *)a_73B7E0, "085 sub_785000" },
		{ 0x785020, (void *)a_73A930, "085 sub_785020" },
		{ 0x785040, (void *)a_73A950, "085 sub_785040" },
		{ 0x7850C0, (void *)a_73BB60, "085 sub_7850C0" },
		{ 0x785130, (void *)a_73B8A0, "085 sub_785130" },
		{ 0x785150, (void *)a_73B8C0, "085 sub_785150" },
		{ 0x7851C0, (void *)a_73BC40, "085 sub_7851C0" },
		{ 0x7851E0, (void *)a_733B70, "085 sub_7851E0" },
		{ 0x785270, (void *)a_733760, "085 sub_785270" },
		{ 0x7852B0, (void *)a_73B620, "085 sub_7852B0" },
		{ 0x785380, (void *)a_7475A0, "085 sub_785380" },
		{ 0x7853A0, (void *)a_73A6D0, "085 nullsub_1312" },
		{ 0x7853B0, (void *)a_73A6D0, "085 nullsub_1313" },
		{ 0x785560, (void *)a_73A380, "085 sub_785560" },
		{ 0x785660, (void *)a_73C280, "085 sub_785660" },
		{ 0x786A70, (void *)a_73D770, "085 sub_786A70" },
		{ 0x786CC0, (void *)a_73D9C0, "085 sub_786CC0" },
		{ 0x787B40, (void *)a_73A6D0, "085 nullsub_1314" },
		{ 0x787E10, (void *)a_73A6D0, "085 nullsub_1315" },
		{ 0x787E20, (void *)a_73F110, "085 sub_787E20" },
		{ 0x787F10, (void *)a_746A10, "085 sub_787F10" },
		{ 0x787F50, (void *)a_746AE0, "085 sub_787F50" },
		{ 0x787FC0, (void *)a_73A6D0, "085 nullsub_1316" },
		{ 0x787FD0, (void *)a_73F110, "085 sub_787FD0" },
		{ 0x788120, (void *)a_733950, "085 sub_788120" },
		{ 0x7882D0, (void *)a_73A6D0, "085 nullsub_1317" },
		{ 0x7882E0, (void *)a_73F110, "085 sub_7882E0" },
		{ 0x788410, (void *)a_73A6D0, "085 nullsub_1318" },
		{ 0x788420, (void *)a_73A380, "085 sub_788420" },
		{ 0x788A40, (void *)a_73C100, "085 sub_788A40" },
		{ 0x788A90, (void *)a_73A380, "085 sub_788A90" },
		{ 0x788B80, (void *)a_73A6D0, "085 nullsub_1326" },
		{ 0x788B90, (void *)a_73A380, "085 sub_788B90" },
		{ 0x788C20, (void *)a_73F990, "085 sub_788C20" },
		{ 0x788EB0, (void *)a_73FC20, "085 sub_788EB0" },
		{ 0x789120, (void *)a_73FE90, "085 sub_789120" },
		{ 0x789240, (void *)a_73FFB0, "085 sub_789240" },
		{ 0x7894A0, (void *)a_740210, "085 sub_7894A0" },
		{ 0x789B60, (void *)a_740910, "085 sub_789B60" },
		{ 0x78A1C0, (void *)a_740F70, "085 sub_78A1C0" },
		{ 0x78A210, (void *)a_740FC0, "085 sub_78A210" },
		{ 0x78A2A0, (void *)a_741050, "085 sub_78A2A0" },
		{ 0x78A370, (void *)a_741120, "085 sub_78A370" },
		{ 0x78ADB0, (void *)a_741B60, "085 sub_78ADB0" },
		{ 0x78B4E0, (void *)a_742290, "085 MAG_085_RandRange" },
		{ 0x78B510, (void *)a_7422C0, "085 sub_78B510" },
		{ 0x78B550, (void *)a_742300, "085 sub_78B550" },
		{ 0x78B5A0, (void *)a_742350, "085 sub_78B5A0" },
		{ 0x78B5F0, (void *)a_7423A0, "085 MAG_085_RandAngleJitter" },
		{ 0x78B700, (void *)a_7424B0, "085 sub_78B700" },
		{ 0x78B710, (void *)a_7424C0, "085 sub_78B710" },
		{ 0x78BEF0, (void *)a_742CA0, "085 sub_78BEF0" },
		{ 0x78BF20, (void *)a_742CD0, "085 sub_78BF20" },
		{ 0x78BF50, (void *)a_742D00, "085 sub_78BF50" },
		{ 0x78C100, (void *)a_742EB0, "085 sub_78C100" },
		{ 0x78C240, (void *)a_742FF0, "085 sub_78C240" },
		{ 0x78C490, (void *)a_73A6D0, "085 nullsub_1327" },
		{ 0x78C4A0, (void *)a_73A6D0, "085 nullsub_1328" },
		{ 0x78C4B0, (void *)a_73A6D0, "085 nullsub_1319" },
		{ 0x78CE70, (void *)a_73A6D0, "085 nullsub_1320" },
		{ 0x78D230, (void *)a_73A6D0, "085 nullsub_1321" },
		{ 0x78D240, (void *)a_73A6D0, "085 nullsub_1322" },
		{ 0x78D400, (void *)a_73A6D0, "085 nullsub_1323" },
		{ 0x78D410, (void *)a_73F110, "085 sub_78D410" },
		{ 0x78D5F0, (void *)a_73A6D0, "085 nullsub_1324" },
		{ 0x78D600, (void *)a_747440, "085 MAG_085_sub_78D600" },
		{ 0x78D670, (void *)a_7474B0, "085 MAG_085_sub_78D670" },
		{ 0x78D690, (void *)a_7474D0, "085 MAG_085_sub_78D690" },
		{ 0x78D6B0, (void *)a_73A6D0, "085 nullsub_1325" },
		{ 0x78D6C0, (void *)a_747500, "085 MAG_085_sub_78D6C0" },
		{ 0x78D700, (void *)a_73A0D0, "085 MAG_085_sub_78D700" },
		{ 0x78D710, (void *)a_747550, "085 MAG_085_sub_78D710" },
		{ 0x78D720, (void *)a_747560, "085 MAG_085_sub_78D720" },
		{ 0x78D750, (void *)a_747590, "085 MAG_085_sub_78D750" },
		{ 0x78D760, (void *)a_7475A0, "085 MAG_085_sub_78D760" },
		{ 0x78D780, (void *)a_73A6D0, "085 nullsub_1310" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x784630, (void *)te_784630, "085 te_784630" },
		{ 0x7847E0, (void *)te_7847E0, "085 te_7847E0" },
		{ 0x784820, (void *)te_784820, "085 te_784820" },
		{ 0x784910, (void *)te_784910, "085 te_784910" },
		{ 0x784960, (void *)te_784960, "085 te_784960" },
		{ 0x784DC0, (void *)te_784DC0, "085 te_784DC0" },
		{ 0x784E30, (void *)te_784E30, "085 te_784E30" },
		{ 0x784F30, (void *)te_784F30, "085 te_784F30" },
		{ 0x784F80, (void *)te_784F80, "085 te_784F80" },
		{ 0x784FD0, (void *)te_784FD0, "085 te_784FD0" },
		{ 0x785080, (void *)te_785080, "085 te_785080" },
		{ 0x785110, (void *)te_785110, "085 te_785110" },
		{ 0x785170, (void *)te_785170, "085 te_785170" },
		{ 0x785200, (void *)te_785200, "085 te_785200" },
		{ 0x785230, (void *)te_785230, "085 te_785230" },
		{ 0x785250, (void *)te_785250, "085 te_785250" },
		{ 0x785290, (void *)te_785290, "085 te_785290" },
		{ 0x7852D0, (void *)te_7852D0, "085 te_7852D0" },
		{ 0x7852F0, (void *)te_7852F0, "085 te_7852F0" },
		{ 0x785310, (void *)te_785310, "085 te_785310" },
		{ 0x785330, (void *)te_785330, "085 te_785330" },
		{ 0x785350, (void *)te_785350, "085 te_785350" },
		{ 0x7853C0, (void *)te_7853C0, "085 te_7853C0" },
		{ 0x785510, (void *)te_785510, "085 te_785510" },
		{ 0x7855C0, (void *)te_7855C0, "085 te_7855C0" },
		{ 0x785620, (void *)te_785620, "085 te_785620" },
		{ 0x785790, (void *)te_785790, "085 te_785790" },
		{ 0x785A90, (void *)te_785A90, "085 te_785A90" },
		{ 0x785BC0, (void *)te_785BC0, "085 te_785BC0" },
		{ 0x785DE0, (void *)te_785DE0, "085 te_785DE0" },
		{ 0x786070, (void *)te_786070, "085 te_786070" },
		{ 0x7864F0, (void *)te_7864F0, "085 te_7864F0" },
		{ 0x786FA0, (void *)te_786FA0, "085 te_786FA0" },
		{ 0x7874A0, (void *)te_7874A0, "085 te_7874A0" },
		{ 0x787AF0, (void *)te_787AF0, "085 te_787AF0" },
		{ 0x787B50, (void *)te_787B50, "085 te_787B50" },
		{ 0x787BB0, (void *)te_787BB0, "085 te_787BB0" },
		{ 0x787BE0, (void *)te_787BE0, "085 te_787BE0" },
		{ 0x787C90, (void *)te_787C90, "085 te_787C90" },
		{ 0x787E90, (void *)te_787E90, "085 te_787E90" },
		{ 0x787ED0, (void *)te_787ED0, "085 te_787ED0" },
		{ 0x787F30, (void *)te_787F30, "085 te_787F30" },
		{ 0x787F80, (void *)te_787F80, "085 te_787F80" },
		{ 0x788040, (void *)te_788040, "085 te_788040" },
		{ 0x788060, (void *)te_788060, "085 te_788060" },
		{ 0x7880A0, (void *)te_7880A0, "085 te_7880A0" },
		{ 0x7880E0, (void *)te_7880E0, "085 te_7880E0" },
		{ 0x788220, (void *)te_788220, "085 te_788220" },
		{ 0x788250, (void *)te_788250, "085 te_788250" },
		{ 0x788290, (void *)te_788290, "085 te_788290" },
		{ 0x788350, (void *)te_788350, "085 te_788350" },
		{ 0x788370, (void *)te_788370, "085 te_788370" },
		{ 0x7883B0, (void *)te_7883B0, "085 te_7883B0" },
		{ 0x7883D0, (void *)te_7883D0, "085 te_7883D0" },
		{ 0x788480, (void *)te_788480, "085 te_788480" },
		{ 0x788490, (void *)te_788490, "085 te_788490" },
		{ 0x788580, (void *)te_788580, "085 te_788580" },
		{ 0x788700, (void *)te_788700, "085 te_788700" },
		{ 0x788810, (void *)te_788810, "085 te_788810" },
		{ 0x788960, (void *)te_788960, "085 te_788960" },
		{ 0x788AF0, (void *)te_788AF0, "085 te_788AF0" },
		{ 0x788B40, (void *)te_788B40, "085 te_788B40" },
		{ 0x788BF0, (void *)te_788BF0, "085 te_788BF0" },
		{ 0x788DD0, (void *)te_788DD0, "085 te_788DD0" },
		{ 0x789050, (void *)te_789050, "085 te_789050" },
		{ 0x789700, (void *)te_789700, "085 te_789700" },
		{ 0x789790, (void *)te_789790, "085 te_789790" },
		{ 0x7897F0, (void *)te_7897F0, "085 te_7897F0" },
		{ 0x789950, (void *)te_789950, "085 te_789950" },
		{ 0x789A10, (void *)te_789A10, "085 te_789A10" },
		{ 0x789A90, (void *)te_789A90, "085 te_789A90" },
		{ 0x789AD0, (void *)te_789AD0, "085 te_789AD0" },
		{ 0x78AB00, (void *)te_78AB00, "085 te_78AB00" },
		{ 0x78AC10, (void *)te_78AC10, "085 te_78AC10" },
		{ 0x78AC90, (void *)te_78AC90, "085 te_78AC90" },
		{ 0x78ACC0, (void *)te_78ACC0, "085 te_78ACC0" },
		{ 0x78AD60, (void *)te_78AD60, "085 te_78AD60" },
		{ 0x78ADE0, (void *)te_78ADE0, "085 te_78ADE0" },
		{ 0x78AE60, (void *)te_78AE60, "085 te_78AE60" },
		{ 0x78B640, (void *)te_78B640, "085 te_78B640" },
		{ 0x78B860, (void *)te_78B860, "085 te_78B860" },
		{ 0x78C350, (void *)te_78C350, "085 te_78C350" },
		{ 0x78C3E0, (void *)te_78C3E0, "085 te_78C3E0" },
		{ 0x78C470, (void *)te_78C470, "085 te_78C470" },
		{ 0x78C4C0, (void *)te_78C4C0, "085 te_78C4C0" },
		{ 0x78C530, (void *)te_78C530, "085 te_78C530" },
		{ 0x78C660, (void *)te_78C660, "085 te_78C660" },
		{ 0x78C6D0, (void *)te_78C6D0, "085 te_78C6D0" },
		{ 0x78CE00, (void *)te_78CE00, "085 te_78CE00" },
		{ 0x78CE30, (void *)te_78CE30, "085 te_78CE30" },
		{ 0x78CE80, (void *)te_78CE80, "085 te_78CE80" },
		{ 0x78CED0, (void *)te_78CED0, "085 te_78CED0" },
		{ 0x78CFA0, (void *)te_78CFA0, "085 te_78CFA0" },
		{ 0x78D150, (void *)te_78D150, "085 te_78D150" },
		{ 0x78D180, (void *)te_78D180, "085 te_78D180" },
		{ 0x78D1C0, (void *)te_78D1C0, "085 te_78D1C0" },
		{ 0x78D1F0, (void *)te_78D1F0, "085 te_78D1F0" },
		{ 0x78D250, (void *)te_78D250, "085 te_78D250" },
		{ 0x78D2B0, (void *)te_78D2B0, "085 te_78D2B0" },
		{ 0x78D2E0, (void *)te_78D2E0, "085 te_78D2E0" },
		{ 0x78D3A0, (void *)te_78D3A0, "085 te_78D3A0" },
		{ 0x78D3D0, (void *)te_78D3D0, "085 te_78D3D0" },
		{ 0x78D480, (void *)te_78D480, "085 te_78D480" },
		{ 0x78D4A0, (void *)te_78D4A0, "085 te_78D4A0" },
		{ 0x78D4D0, (void *)te_78D4D0, "085 te_78D4D0" },
		{ 0x78D590, (void *)te_78D590, "085 te_78D590" },
		{ 0x78D5C0, (void *)te_78D5C0, "085 te_78D5C0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag085_the_end()
	{
		act::register_module(85);
		for (const act::theend::ModPort *p = act::theend::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(85, p->addr, p->port, p->name);
		for (const act::theend::ModPort *p = act::theend::PORTS; p->addr; p++)
			act::register_module_port(85, p->addr, p->port, p->name);
		// 30 fps layer: see mag085_the_end_held.inc
		FX_HELD(register_mag085_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag085_the_end_held.inc"
#endif
