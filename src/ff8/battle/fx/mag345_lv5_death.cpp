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

// Effect 345: LV5 Death (enemy attack 124 of kernel.bin; Omega Weapon c0m114, Torama c0m057; MAG_345_*), cast by an
// enemy on the party: a copy of the actor/heal effect library (see act_engine.h), LV?Death 74 (mag074_lvdeath_quistis.cpp,
// itself Death 14 relocated) relocated once more.
//
// The module 0x705A60..0x70D370 is LV?Death's module 0x7C4440..0x7CBD90 function for function (near4 w\p345_74.txt,
// w\amap_345.txt: code -0xBE9E0 up to 0x7C4A90, -0xBEA20 after it; exe data -0x5DA10 (reaper data / prim model / camera
// animation 0x1524774), -0x5CBE8 (reaper model container 0x1525754, spark tables), -0x12E8C4 (actor data files, the
// reaper's TIMs 0x164A678 / 0x1652A98), module globals -0x95478 up to the scratch stack pointer 0x255EA84, -0x95498
// from the actor pool pointer 0x255EA88 on: no bind block, as Death). 113 of its 121 functions are identical to
// LV?Death's up to addresses, 6 more differ only in their switch jump tables (addresses); the one that differs:
//   0x7060B0 (LV?Death 0x7C4A90, director state 0) = Death's 0x8B6EF0: the first emitter's director uploads the
//     reaper's two TIMs and every director waits 1 tick before its reaper (no caster animation set, no chain
//     transformation, no 0xC wait).
// So, as in LV?Death, every target gets its own emitter (master loop 0x70D2A0 per TARGET), director (target list = the
// ONE target of its emitter, 0x706B10), reaper (placed in front of it), dark actor system, spark bursts and damage
// (0x70D210: action +0x2A, target +0x2B); they share the reaper data block [0x1524658].
//
// Setup MAG_345_UNKNOWN 0x705A80 (runs once, not ported; file loader MAG_345_UNKNOWN_FL 0x705A60 = the effect's data
// file named at 0x15297D8 "mag013-073-344.tim" into 0x25570A0, packet arenas at file + 0xE000 / + 0x1C000; camera
// animation 0x1524774 unless the context's flag +1 bit 0) creates the root queue 0x25572D8 with the master and seven
// task pools: 0x255DA98 (4 x 0x58 emitters), 0x25571E0 (3 x 0x48 directors), 0x255EA30 (3 x 0x140 creature), 0x255D7F8
// (4 x 0x2A4 actor systems), 0x25572E8 (10 x 0x70 stage wobbles), 0x2557760 (0x32 x 0x118 dark sparks), 0x255DA88
// (10 x 0x40, unused).
//   Master (0x705C00) - camera copy 0x2793E58 (the module scratch stack 0x255EA84 grows down from it), packet arena by
//     tick parity (cursor 0x25570A4), bone follow, 11-state table: 0x705DA0 carves the actor pools after the arena,
//     0x705E20 one emitter per target (loop 0x70D2A0), then waits, restores the battle textures (0x508630) and ends.
//   Emitter (0x705E50, engine task a_73A1A0): 0x705ED0 places the reaper in front of the target (0x705F00, data block
//     [0x1524658]) and starts the director; 0x70D210 applies the damage once the reaper struck (data +0x48), 0x70D250
//     waits for the director / reaper's end (+0x4C).
//   Director (0x705FF0): state 0 0x7060B0 (above), then after the wait spawns the reaper creature (0x7060F0 -> 0x706130,
//     model container 0x1525754, animation 3).
//   Reaper (0x706130): state machine {appear 0x706940, 0x70C8B0, strike 0x70C900, vanish 0x70D030, 0x70D090}; drawn
//     each tick (0x7061F0) either as the engine's creature (a_746C10) or, while it fades in (+0x26 bit 3), with the
//     module's own dark renderer (0x706450 / 0x706550). At the strike it spawns the dark actor system (0x706A80, data
//     0x164808C) and the dark spark bursts (0x70C9E0 -> 0x70CD70: 4 sparks each, drawn as prim models through 0x707820);
//     at its appearance (first target only: +0x2E == 0) the aura actor system (data 0x1643698) and the stage wobble in
//     (0x70C720), at the last target's end the stage wobble out (0x70D0E0).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Entity writes: the effect light block 0x1D99AB0 (reaper); the stage wobble words 0x1D98992 + k * 0x2C (stage wobble
// tasks). Camera: the setup's camera animation 0x1524774 (engine camera script); the module only reads the camera (copy
// 0x2793E58 per tick, 0x1D97778 in its draws).
// Module globals: 0x25570A0..0x255EAC0 (file pointer 0x25570A0, packet cursor 0x25570A4, pools, queues, arenas, actor
// state 0x255EA94, scratch stack pointer 0x255EA84, reaper render context 0x25570B0 / 0x255EA98 and its vertex buffer
// 0x2557770), the module scratch stack below the camera copy 0x2793E58; the pointers 0x1524658 / 0x15297A0 (exe data)
// lead to the reaper data. The reaper model container (0x1525754..) and the actor data files (0x1643698.., 0x164808C..,
// 0x164A338..) in exe data are rewritten in place (skeleton, relocation, fade).
// Fujin's described LV5 Death (effect_345.json) runs this code as its pieces (Doom's kind): its module memory through
// R() (act_engine.h), the reaper data through the descriptor (G_1533010), the other data word's value moved with it,
// the tasks it spawns the program's or thunks of their ports (Spawn); its parameters (Lv5DeathParams: the sounds, the
// reapers' fades and frames, the two actor systems, the spark mesh, the reapers' TIMs, the sparks' sizes) are set from
// the program's.

#include "act_engine.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace act
{
namespace lv5_death
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_345 = { "lv5_death", 345, 0x705A60, 0x70D370,
		{ 0x0, 0x0, 0x1524658, 0x0, 0x0, 0x2557204, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25570A0, 0x0, 0x0, 0x0, 0x2557208, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x255D7F8, 0x0, 0x255DA98, 0x0, 0x0, 0x0, 0x0, 0x0, 0x255EA40, 0x0, 0x0, 0x255EA7C, 0x0, 0x255EA84, 0x0, 0x255EA94 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706B10, 0x7075D0, 0x707660, 0x7076C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x709A40, 0x709B80, 0x709BC0, 0x70AE50, 0x70AEA0, 0x70AED0, 0x70AF50, 0x70B640, 0x70B690, 0x70B730, 0x70B7F0, 0x70C040, 0x70C440, 0x70C4D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x705E50, 0x705ED0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706CA0, 0x706D80, 0x706F20, 0x706FF0, 0x707110, 0x707370, 0x709B00, 0x709C50, 0x70A2B0, 0x70A300, 0x70A390, 0x70A460, 0x70ABF0, 0x70AD00, 0x70AD80, 0x70ADB0, 0x70B5D0, 0x70B600, 0x70B6E0, 0x70B800, 0x70B950, 0x0, 0x70C010, 0x70C1F0, 0x70C330, 0x0, 0x7075D0, 0x70D210, 0x70D250, 0x70D270, 0x70D290, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x25570A4;   // module packet cursor (the draws' arena)
	static const uint32_t ACTOR_STATE = 0x255EA94;     // current actor system state block
	static const uint32_t SCRATCH_SP = 0x255EA84;      // module scratch stack pointer
	static const uint32_t REAPER_DATA = 0x1524658;     // -> the reaper's placement / phase block
	static const uint32_t ORIG_Master = 0x705C00;
	static const uint32_t ORIG_Reaper = 0x706130;
	static const uint32_t ORIG_ActorSystem = 0x706A80;
	static const uint32_t ORIG_Spark = 0x70CD70;
	static const uint32_t REAPER_CTX = 0x25570B0;      // render context of the dark reaper draw
	static const uint32_t SOUND_APPEAR = 0x152465C, SOUND_STRIKE = 0x1524660;
	static const uint32_t REAPER_TIM_A = 0x164A678, REAPER_TIM_B = 0x1652A98;   // the reapers' TIMs (in the actor data files)
	static const uint32_t SPARK_TABLE = 0x1529720;     // the sparks' scale x / y / z and fade by tick (4 x 16 words)

	// ------------------------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------------------------
	struct Lv5DeathParams
	{
		struct { uint32_t appear, strike; } sound;
		struct { int16_t fade_in, dark_until, strike_frame, vanish_frame, fade_out; } reaper;   // its fades, its animation frames
		struct { int16_t length, mode; } aura, strike;                                           // the two actor systems
		struct { uint32_t at_1524664; } models; // the prim models drawn (meshes)
		struct { uint32_t tim_a, tim_b; } res; // its resources (data): the reapers' TIMs
		struct { uint32_t spark; } tables; // the sparks' scale and fade by tick
	};
	static const Lv5DeathParams GAME = { { SOUND_APPEAR, SOUND_STRIKE }, { 0x400, 0x14, 5, 0xB, 0x200 }, { 0x14, 2 }, { 0x14, 0 }, { 0x1524664 },
		{ REAPER_TIM_A, REAPER_TIM_B }, { SPARK_TABLE } };
	static Lv5DeathParams live = GAME;
	static const Lv5DeathParams &PD() { return live; }

	// engine functions not in act::x
	namespace dx
	{
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2E0)(a1); }
		static inline uint32_t GTE_StoreSXY012_PolyFT3_2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E320)(a1); }
	}
	using namespace dx;

	// helpers of the library code (as in act_engine.cpp / mag090_tonberry.cpp / mag097_boko.cpp)
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
	// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
	static inline void b1_copy16(uint32_t dst, uint32_t src)
	{
		for (int i = 0; i < 16; i += 4)
			U32(dst, i) = U32(src, i);
	}
	static inline int32_t b2_fx(uint32_t v)
	{
		int32_t s = (int32_t)v;
		return add32(s, (s >> 31) & 0xFFFF) >> 16;
	}
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
	static inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }

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
	uint32_t __cdecl d_705DE0(void);
	uint32_t __cdecl d_705DA0(uint32_t a1);
	uint32_t __cdecl d_705F00(uint32_t a1);
	uint32_t __cdecl d_705ED0(uint32_t a1);
	uint32_t __cdecl d_706060(uint32_t a1);
	uint32_t __cdecl d_705FF0(uint32_t a1);
	uint32_t __cdecl d_7060B0(uint32_t a1);
	uint32_t __cdecl d_7060F0(uint32_t a1);
	uint32_t __cdecl d_706550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_706450(uint32_t a1);
	uint32_t __cdecl d_7061F0(uint32_t a1);
	uint32_t __cdecl d_706130(uint32_t a1);
	uint32_t __cdecl d_70C8A0(uint32_t a1);
	uint32_t __cdecl d_70C840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_70C810(uint32_t a1);
	uint32_t __cdecl d_706940(uint32_t a1);
	uint32_t __cdecl d_70C8B0(uint32_t a1);
	uint32_t __cdecl d_70C900(uint32_t a1);
	uint32_t __cdecl d_70D030(uint32_t a1);
	uint32_t __cdecl d_70D090(uint32_t a1);
	uint32_t __cdecl d_70C780(uint32_t a1);
	uint32_t __cdecl d_70C7C0(uint32_t a1);
	uint32_t __cdecl d_70D140(uint32_t a1);
	uint32_t __cdecl d_70CA40(uint32_t a1);
	uint32_t __cdecl d_70CA80(uint32_t a1);
	uint32_t __cdecl d_70CEC0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70CD70(uint32_t a1);
	uint32_t __cdecl d_70CFC0(uint32_t a1);
	uint32_t __cdecl d_70CFF0(uint32_t a1);
	uint32_t __cdecl d_7076C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_709B00(uint32_t a1);
	uint32_t __cdecl d_70ADB0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl d_70B730(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_705C00(uint32_t a1);
	uint32_t __cdecl d_706AE0(uint32_t a1);
	uint32_t __cdecl d_706CA0(uint32_t a1);
	uint32_t __cdecl d_706F20(void);
	uint32_t __cdecl d_7075D0(uint32_t a1);
	uint32_t __cdecl d_707660(void);
	uint32_t __cdecl d_707820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_707950(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_707B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_707E00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_7082F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_708E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_709370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl d_709A40(void);
	uint32_t __cdecl d_709B80(void);
	uint32_t __cdecl d_709BC0(uint32_t a1);
	uint32_t __cdecl d_70ABF0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70AD00(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70AD80(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70AE50(uint32_t a1);
	uint32_t __cdecl d_70AED0(uint32_t a1);
	uint32_t __cdecl d_70AF50(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl d_70B950(uint32_t a1);
	uint32_t __cdecl d_70C440(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70C4D0(void);
	uint32_t __cdecl d_70C560(uint32_t a1);
	uint32_t __cdecl d_70C590(uint32_t a1, uint32_t a2);
	uint32_t __cdecl d_70C6B0(uint32_t a1);
	uint32_t __cdecl d_70D160(uint32_t a1);
	uint32_t __cdecl d_70D210(uint32_t a1);
	uint32_t __cdecl d_706B10(uint32_t a1);
	uint32_t __cdecl d_70D2A0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag345_lv5_death_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace lv5_death
{
	// ====================================================================================
	// master states, emitter, director, reaper (module code)
	// ====================================================================================

	// 0x705DE0 (MAG_345_sub_705DE0): clears the two actor pools (0x870 bytes at [0x255EA44], 0x9D80
	// at [0x255EA88]) and the pool cursors / counters
	uint32_t __cdecl d_705DE0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(R(0x255EA44)), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(R(0x255EA88)), 0x9D80);
		MEM<uint16_t>(R(0x2557758)) = 0;
		MEM<uint16_t>(R(0x255EA50)) = 0;
		MEM<uint16_t>(R(0x255E610)) = 0;
		MEM<uint16_t>(R(0x25570A8)) = 0;
		return 0; // void
	}

	// 0x705DA0 (MAG_345_sub_705DA0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x255EA90] (0x870 + 0x9D80 bytes) and cleared, next state
	uint32_t __cdecl d_705DA0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(R(0x255EA90));
		MEM<uint32_t>(R(0x255EA44)) = p;
		p += 0x870;
		MEM<uint32_t>(R(0x255EA88)) = p;
		p += 0x9D80;
		MEM<uint32_t>(R(0x255EA90)) = p;
		d_705DE0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x705F00 (MAG_345_sub_705F00; Death 0x8B6D40): clears the reaper data block [0x1524658]
	// (0x54 bytes), +8 = the target's effect bone 0xF1; +0 / +4 = the target's position with the
	// reaper 0x200 + size * 125 / 256 in front of it (towards -z for a party target, +z for an
	// enemy), +0x4A its yaw facing the target (angle of +8 - +0 in x / z, minus 0x800)
	uint32_t __cdecl d_705F00(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(G(G_1533010)), 0x54);
		const uint32_t slot0 = U8(a1, 0x2D);
		x::GetEffectSpawnPosition(0x1D972C0 + slot0 * 0x9C, 0xF1, 0, MEM<uint32_t>(G(G_1533010)) + 8);
		const uint8_t slot = U8(a1, 0x2D);
		const uint32_t d = MEM<uint32_t>(G(G_1533010));
		U16(d, 0xA) = 0;
		const uint32_t ent = 0x1D972C0 + (uint32_t)slot * 0x9C;
		U32(d, 0) = U32(ent, 0x1C);
		U32(d, 4) = U32(ent, 0x20);
		U16(d, 2) = 0;
		const int32_t size125 = mul32(S16(ent, 0x26), 125);
		const int32_t off = shl32(size125, 4) >> 12;
		if (slot <= 2)
			U16(d, 4) = (uint16_t)(U16(d, 4) + (uint16_t)(-0x200 - off));
		else
			U16(d, 4) = (uint16_t)(U16(d, 4) + (uint16_t)(off + 0x200));
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dxv = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t angle = x::CartesianToGameAngle((uint32_t)(int32_t)dxv, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(G(G_1533010)), 0x4A) = (uint16_t)((angle - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x705ED0 (MAG_345_sub_705ED0): emitter state 0 - places the reaper, spawns the director
	// 0x705FF0 (0x48 bytes, director queue), next state
	uint32_t __cdecl d_705ED0(uint32_t a1)
	{
		d_705F00(a1);
		Spawn(R(0x25571E0), 0x705FF0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x706060 (sub_706060): the reaper data's phase bookkeeping - ++ticks in phase (+0x46); a new
	// requested phase (+0x42) becomes current (+0x40, timer reset); the queued phase (+0x44) becomes
	// the requested one (the per-phase hooks of this copy are empty)
	uint32_t __cdecl d_706060(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(G(G_1533010));
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			a_73A6D0(); // nullsub 0x70D200 (a1 pushed, unused)
			d = MEM<uint32_t>(G(G_1533010));
		}
		const uint16_t next = U16(d, 0x44);
		if (U16(d, 0x42) != next)
		{
			U16(d, 0x42) = next;
			a_73A6D0(); // nullsub 0x70D1F0 (a1 pushed, unused)
		}
		return a1;
	}

	// 0x705FF0 (MAG_345_sub_705FF0): DIRECTOR task - phase bookkeeping, state {0x7060B0 TIM uploads,
	// 0x7060F0 spawn the reaper, finished, ret}
	uint32_t __cdecl d_705FF0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7060B0;
		states[1] = 0x7060F0;
		states[2] = 0x70D1C0;
		states[3] = 0x70D1E0; // nullsub (ret)
		d_706060(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7060B0 (MAG_345_UploadReaperTIMs; Death 0x8B6EF0, identical): director state 0 - the first emitter (+0x2E == 0)
	// uploads the reaper's two TIMs; wait counter +0x44 = 1, next state (LV?Death 74 binds the caster's animation set
	// here and waits 0xC)
	uint32_t __cdecl d_7060B0(uint32_t a1)
	{
		if (U8(a1, 0x2E) == 0)
		{
			x::Battle_QueueTIMUpload_GetEOF(fxp::tim(PD().res.tim_a, REAPER_TIM_A));
			x::Battle_QueueTIMUpload_GetEOF(fxp::tim(PD().res.tim_b, REAPER_TIM_B));
		}
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x44) = 1;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7060F0 (MAG_345_SpawnReaperCreature; Death 0x8B6F30): director state 1 - after the wait, the reaper creature
	// 0x706130 (0x140 bytes, creature queue) bound to the model container 0x1525754, animation 3
	uint32_t __cdecl d_7060F0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) > 0)
			return 0; // void
		const uint32_t n = Spawn(R(0x255EA30), ORIG_Reaper, 0x140, a1);
		x::Effect_BindModelContainerSetAnim(n, 0x1525754, 3);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x706550 (sub_706550): the module's dark model renderer (model a1, OT a2; a3 unused; render
	// context a4: +0x20 camera * model matrix, +0x44 -> {+4 vertex buffer}, +0x48 the model block
	// (entity layout: +0x28 colour, +0x7C object mask), +0x4C -> packet cursor). Per visible
	// object: its vertices through their bone matrices as the GTE light matrix / background vector
	// (world space) into the vertex buffer; then its textured triangles and quads whose vertices are
	// all at or above the ground (y <= 0), front-facing, as flat-textured packets (colour = entity
	// colour, codes 0x24 / 0x2C), OT z = OTZ >> 2. Header (0x5C bytes, Field_Alloc): +0x28 vertex /
	// IR scratch, +0x38..+0x44 vertex indices, +0x48 / +0x4C packet words, +0x50 object mask, +0x54
	// NCLIP, +0x58 OTZ.
	uint32_t __cdecl d_706550(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t ctx = a4;
		const uint32_t blk = U32(ctx, 0x48);
		uint32_t cursor = MEM<uint32_t>(U32(ctx, 0x4C));        // [esp+0x14]
		const uint32_t vbuf = U32(U32(ctx, 0x44), 4);            // [esp+0x10]
		uint32_t table = U32(a1, 4);
		const uint32_t bones = U32(a1, 0) + 0x10;                // [esp+0x28]
		const int32_t count = S32(table, 0);                     // [esp+0x24]
		const uint32_t hdr = x::Field_Alloc(0x5C);
		const uint32_t mask = U32(blk, 0x7C);
		const uint32_t colour = U32(blk, 0x28) & 0xFFFFFF;
		table += 4;
		U32(hdr, 0x50) = mask;
		U32(hdr, 0x4C) = colour | 0x2C000000;
		U32(hdr, 0x48) = colour | 0x24000000;
		x::GTE_SetRotMatrix(ctx + 0x20);
		x::GTE_SetTransVector(ctx + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t v = vbuf;
			table += 4;
			uint32_t obj = U32(a1, 4) + U32(table, -4);
			if (!((U32(hdr, 0x50) >> (i & 31)) & 1))
				continue;
			int32_t groups = S16(obj, 0);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = bones + (uint32_t)(bone * 0x30) + 0x10;
				GTE_SetLightMatrix(m);
				GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					U16(hdr, 0x28) = U16(obj, 0);
					U16(hdr, 0x2A) = U16(obj, 2);
					U16(hdr, 0x2C) = U16(obj, 4);
					obj += 6;
					x::GTE_LoadV0(hdr + 0x28);
					GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(hdr + 0x28);
					// the 4th word (+0x2E) is what the scratch held (the original copies it along)
					U32(v, 0) = U32(hdr, 0x28);
					U32(v, 4) = U32(hdr, 0x2C);
					v += 8;
				}
			}
			obj = (obj + 3) & ~3u;
			const int32_t n3 = S16(obj, 0);
			int32_t n4 = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			uint32_t pkt = cursor;
			for (int32_t k = n3; k > 0; k--, rec += 0x10)
			{
				const uint32_t i0 = U16(rec, 0) & 0xFFF, i1 = U16(rec, 2) & 0xFFF, i2 = U16(rec, 4) & 0xFFF;
				U32(hdr, 0x38) = i0;
				U32(hdr, 0x3C) = i1;
				U32(hdr, 0x40) = i2;
				if (S16(vbuf + i0 * 8, 2) > 0 || S16(vbuf + i1 * 8, 2) > 0 || S16(vbuf + i2 * 8, 2) > 0)
					continue;
				x::GTE_LoadV012(vbuf + i0 * 8, vbuf + i1 * 8, vbuf + i2 * 8);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(hdr + 0x54);
				if (S32(hdr, 0x54) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(hdr + 0x58);
				U32(hdr, 0x58) = U32(hdr, 0x58) >> 2;
				GTE_StoreSXY012_PolyFT3(pkt);
				U32(pkt, 0xC) = U32(rec, 8);
				U32(pkt, 0x14) = U32(rec, 0xC);
				U16(pkt, 0x1C) = U16(rec, 6);
				U32(pkt, 4) = U32(hdr, 0x48);
				U32(pkt, 0) = 0x7000000;
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(hdr, 0x58) * 4, pkt);
				pkt += 0x20;
			}
			if (n4 > 0)
			{
				rec += 4;
				for (; n4 > 0; n4--, rec += 0x14)
				{
					const uint32_t i0 = U16(rec, -4) & 0xFFF, i1 = U16(rec, -2) & 0xFFF, i2 = U16(rec, 0) & 0xFFF;
					U32(hdr, 0x38) = i0;
					U32(hdr, 0x3C) = i1;
					U32(hdr, 0x40) = i2;
					if (S16(vbuf + i0 * 8, 2) > 0 || S16(vbuf + i1 * 8, 2) > 0 || S16(vbuf + i2 * 8, 2) > 0)
						continue;
					// quirk 0x70681D: tests the 4th vertex of the PREVIOUS quad (+0x44 is set below;
					// for the first quad it is what the header scratch held)
					if (S16(vbuf + U32(hdr, 0x44) * 8, 2) > 0)
						continue;
					x::GTE_LoadV012(vbuf + i0 * 8, vbuf + i1 * 8, vbuf + i2 * 8);
					x::GTE_RTPT();
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(hdr + 0x54);
					if (S32(hdr, 0x54) <= 0)
						continue;
					GTE_StoreSXY012_PolyFT3_2(pkt);
					const uint32_t i3 = U16(rec, 2) & 0xFFF;
					U32(hdr, 0x44) = i3;
					x::GTE_LoadV0(vbuf + i3 * 8);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(hdr + 0x58);
					const uint32_t z = U32(hdr, 0x58) >> 2;
					U32(hdr, 0x58) = z;
					U32(pkt, 0xC) = U32(rec, 4);
					U16(pkt, 0x1C) = U16(rec, 0xC);
					U16(pkt, 0x24) = U16(rec, 0xE);
					U32(pkt, 0x14) = U32(rec, 8);
					U32(pkt, 0) = 0x9000000;
					U32(pkt, 4) = U32(hdr, 0x4C);
					x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, pkt);
					pkt += 0x28;
				}
			}
			cursor = pkt;
		}
		const uint32_t out = U32(ctx, 0x4C);
		x::Field_Free(0x5C);
		MEM<uint32_t>(out) = cursor;
		return out;
	}

	// 0x706450 (sub_706450): the dark reaper draw of render context a1 (0x25570B0): matrix = the
	// model block's (+0x48 -> +0x40) rotation through the camera, its translation through the camera
	// (MAC -> +0x34); the block's shadow (0x5088A0), both models (+0x64 and the second +0x78) through
	// 0x706550, then the bone matrices back from the pose
	uint32_t __cdecl d_706450(uint32_t a1)
	{
		const uint32_t ctx = a1;
		const uint32_t blk = U32(ctx, 0x48);
		memcpy((void *)ctx, (const void *)(blk + 0x40), 32);
		const uint32_t anim = blk + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(anim);
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
		const uint32_t shadow = x::sub_5088A0(blk, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, MEM<uint32_t>(U32(ctx, 0x4C)));
		MEM<uint32_t>(U32(ctx, 0x4C)) = shadow;
		d_706550(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t second = U32(blk, 0x78);
		if (second != 0)
			d_706550(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		return 0; // void
	}

	// 0x7061F0 (sub_7061F0): the reaper's draw (hidden: +0x26 bit 2) - dark (bit 3): matrix of the
	// rotation +0x3C / position +0x4C / scale +0x114, drawn by 0x706450; else the engine's creature
	// draw (a_746C10) into the module arena
	uint32_t __cdecl d_7061F0(uint32_t a1)
	{
		const uint16_t f = U16(a1, 0x26);
		if (f & 4)
			return 0; // void
		if (f & 8)
		{
			x::ComposeZYXRotationMatrix(a1 + 0x3C, a1 + 0x70);
			S32(a1, 0x84) = S16(a1, 0x4C);
			S32(a1, 0x88) = S16(a1, 0x4E);
			S32(a1, 0x8C) = S16(a1, 0x50);
			x::scale3DMatrix(a1 + 0x70, a1 + 0x114);
			return d_706450(R(REAPER_CTX));
		}
		MEM<uint32_t>(R(PACKET_CURSOR)) = a_746C10(a1, R(0x255DC10), MEM<uint32_t>(R(PACKET_CURSOR)));
		return 0; // void
	}

	// 0x706130 (sub_706130): REAPER creature task - state {0x706940 appear, 0x70C8B0, 0x70C900
	// strike, 0x70D030 vanish, 0x70D090 end, ret}; the effect light block 0x1D99AB0 = its matrix
	// +0x70, translation +0x84.. = position +0x4C..; drawn unless hidden (0x7061F0)
	uint32_t __cdecl d_706130(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x706940;
		states[1] = 0x70C8B0;
		states[2] = 0x70C900;
		states[3] = 0x70D030;
		states[4] = 0x70D090;
		states[5] = 0x70D1B0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const int32_t pz = S16(node, 0x50);
		const int32_t py = S16(node, 0x4E);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 32);
		const int32_t px = S16(node, 0x4C);
		S32(node, 0x8C) = pz;
		const uint8_t f = U8(node, 0x26);
		S32(node, 0x84) = px;
		S32(node, 0x88) = py;
		if (!(f & 4))
		{
			// 30 fps layer: see mag345_lv5_death_held.inc
			FX_HELD(held_note_reaper(node);)
			d_7061F0(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x70C8A0 (sub_70C8A0): [0x15297A0] +0x20 = 0
	uint32_t __cdecl d_70C8A0(uint32_t a1)
	{
		(void)a1;
		U16(R(MEM<uint32_t>(0x15297A0)), 0x20) = 0;
		return 0; // void
	}

	// 0x70C840 (sub_70C840): render context a1 of the dark draw: cleared (0x54 bytes), +0x40 = the
	// vertex buffer a2, +0x44 = the object block a3 (+4 = a2, screen box 0 / 0 / 0x140 / 0, colour
	// 0x80 x 3, mask all, +0x24 0), +0x48 = the model block a4, +0x4C / +0x50 -> the packet cursor
	uint32_t __cdecl d_70C840(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x54);
		U32(a1, 0x48) = a4;
		U32(a1, 0x40) = a2;
		U32(a1, 0x4C) = R(PACKET_CURSOR);
		U32(a1, 0x50) = R(PACKET_CURSOR);
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

	// 0x70C810 (sub_70C810): the reaper's dark draw context (0x25570B0, vertex buffer 0x2557770,
	// object block 0x255EA98, model block = the reaper node +0x30); dark flag +0x26 bit 3
	uint32_t __cdecl d_70C810(uint32_t a1)
	{
		d_70C840(R(REAPER_CTX), R(0x2557770), R(0x255EA98), a1 + 0x30);
		U8(a1, 0x26) |= 8;
		return 0; // void
	}

	// 0x706940 (sub_706940): reaper state 0 - position / yaw from the reaper data, scale 1:1:1
	// (+0x114, +0x60 -> it); first action: animation 5, dark draw, the aura actor system (data
	// 0x1643698), the stage wobble in (0x70C720), sound 0x152465C, next state; later actions:
	// animation 6, fully shown (+0x13A / +0x13C = 0x1000), sound 0x1524660, two states on
	uint32_t __cdecl d_706940(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(G(G_1533010));
		U32(a1, 0x4C) = U32(d, 0);
		const uint32_t w4 = U32(d, 4);
		const uint16_t yaw = U16(d, 0x4A);
		U32(a1, 0x60) = a1 + 0x114;
		U32(a1, 0x50) = w4;
		U16(a1, 0x3E) = yaw;
		U32(a1, 0x11C) = 0x1000;
		U32(a1, 0x118) = 0x1000;
		U32(a1, 0x114) = 0x1000;
		d_70C8A0(a1);
		if (U8(a1, 0x2E) == 0)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 5);
			d_70C810(a1);
			a_73C100(a1, ORIG_ActorSystem, 0x1643698, 0, (uint16_t)PD().aura.length, (uint16_t)PD().aura.mode);
			Spawn(R(0x25572E8), 0x70C720, 0x70, a1);
			x::BdPlaySE(fxp::sound(PD().sound.appear, SOUND_APPEAR), 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		x::au_re_Battle_ReadAnimation_7(a1, 6);
		U16(a1, 0x13A) = 0x1000;
		U16(a1, 0x13C) = 0x1000;
		d_70C590(a1, 1);
		d_70C6B0(a1);
		x::BdPlaySE(fxp::sound(PD().sound.strike, SOUND_STRIKE), 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 2);
		return 0; // void
	}

	// 0x70C8B0 (sub_70C8B0): reaper state 1 - the dark draw ends at animation frame 0x14 (+0x136);
	// when the appearance animation ends: animation 6, sound 0x1524660, next state
	uint32_t __cdecl d_70C8B0(uint32_t a1)
	{
		if (S16(a1, 0x136) == PD().reaper.dark_until)
			U8(a1, 0x26) &= 0xF7;
		if (x::au_re_Battle_ReadAnimation_8(a1) == 1)
		{
			U8(a1, 0x26) &= 0xF7;
			x::au_re_Battle_ReadAnimation_7(a1, 6);
			x::BdPlaySE(fxp::sound(PD().sound.strike, SOUND_STRIKE), 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x70C900 (sub_70C900): reaper state 2 (strike) - visibility +0x13A down by 0x400 per tick (to
	// 0, copied to +0x13C, applied by 0x70C590 / 0x70C6B0); at animation frame 5: the reaper data's
	// strike flag (+0x48) and, unless the target is immune (target record +3 bit 2), the dark actor
	// system (data 0x164808C) and the spark bursts (0x70C9E0); next state when the animation ends
	// (the last action) or from frame 0xB (other actions)
	uint32_t __cdecl d_70C900(uint32_t a1)
	{
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) - PD().reaper.fade_in);
		if (S16(a1, 0x13A) <= 0)
			U16(a1, 0x13A) = 0;
		U16(a1, 0x13C) = U16(a1, 0x13A);
		d_70C590(a1, 1);
		d_70C6B0(a1);
		if (S16(a1, 0x136) == PD().reaper.strike_frame)
		{
			const int32_t action = S8(a1, 0x2A);
			U16(MEM<uint32_t>(G(G_1533010)), 0x48) = 1;
			const uint32_t targets = U32(U32(U32(a1, 0xC), 4) + (uint32_t)(action * 20), 8);
			const int32_t target = S8(a1, 0x2B);
			if (!(U8(targets + (uint32_t)(target * 24), 3) & 4))
			{
				a_73C100(a1, ORIG_ActorSystem, 0x164808C, 0, (uint16_t)PD().strike.length, (uint16_t)PD().strike.mode);
				Spawn(R(0x2557760), 0x70C9E0, 0x118, a1);
			}
		}
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(a1);
		if (U8(a1, 0x2E) == U8(a1, 0x2F))
		{
			if ((uint16_t)r == 1)
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (S16(a1, 0x136) >= PD().reaper.vanish_frame)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x70D030 (sub_70D030): reaper state 3 (vanish) - animation step; visibility +0x13A up by 0x200
	// per tick to 0x1000 (then hidden: +0x26 bit 2, next state), copied to +0x13C and applied
	uint32_t __cdecl d_70D030(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + PD().reaper.fade_out);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 4;
			U16(a1, 0x13A) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		d_70C590(a1, 1);
		d_70C6B0(a1);
		return 0; // void
	}

	// 0x70D090 (sub_70D090): reaper state 4 - the last action starts the stage wobble out
	// (0x70D0E0); finished, the reaper data's end flag (+0x4C), next state
	uint32_t __cdecl d_70D090(uint32_t a1)
	{
		if (U8(a1, 0x2E) == U8(a1, 0x2F))
			Spawn(R(0x25572E8), 0x70D0E0, 0x70, a1);
		const uint32_t d = MEM<uint32_t>(G(G_1533010));
		U16(a1, 0x26) |= 1;
		U16(d, 0x4C) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x70C780 (sub_70C780): stage wobble state 0 - from tick 0x1C: level +0x1C = 0 and the four
	// stage wobble blocks cleared (words 0x1D98992 + k * 0x2C, bytes +0x28..+0x2A), next state
	uint32_t __cdecl d_70C780(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x1C)
			return 0; // void
		U16(a1, 0x1C) = 0;
		uint32_t p = 0x1D989BA;
		for (int i = 4; i != 0; i--, p += 0x2C)
		{
			MEM<uint16_t>(p - 0x28) = 0;
			MEM<uint8_t>(p) = 0;
			MEM<uint8_t>(p - 1) = 0;
			MEM<uint8_t>(p - 2) = 0;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x70C7C0 (sub_70C7C0): stage wobble state 1 - level +0x1C up by 0x200 per tick to 0x600 (then
	// finished, next state), written to the four stage wobble words 0x1D98992 + k * 0x2C
	uint32_t __cdecl d_70C7C0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
		if (S16(a1, 0x1C) >= 0x600)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x70D140 (sub_70D140): stage wobble out state 0 - level +0x1C = 0x600, next state
	uint32_t __cdecl d_70D140(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0)
			return a1;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x600;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x70CA40 (sub_70CA40): spark burst state 0 - position = the target's effect bone 0xF1, next state
	uint32_t __cdecl d_70CA40(uint32_t a1)
	{
		const uint32_t slot = U8(a1, 0x2D);
		x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0xF1, 0, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// one group of four sparks (0x70CD70, 0x118 bytes, spark queue) from the burst a1: each spark k
	// starts at the burst position (+0xD4 + 8k), with the base orientation `base` turned by a random
	// yaw (0..0xFFF) and pitch (`pitch_mask` / `pitch_add` of a random word) as its matrix (+0x54 +
	// 0x20k), and a velocity (+0xF4 + 8k) of -(0x1000 + random 0..0x1FFF) along y through it
	static void SparkGroup(uint32_t a1, uint32_t base, uint32_t m2, uint32_t pitch_mask, uint32_t pitch_add, int32_t pitch_sub)
	{
		const uint32_t t = Spawn(R(0x2557760), ORIG_Spark, 0x118, a1);
		uint32_t vel = t + 0xF4;
		uint32_t mat = t + 0x54;
		for (int k = 4; k != 0; k--, mat += 0x20, vel += 8)
		{
			U32(vel, -0x20) = U32(a1, 0x1C);
			U32(vel, -0x1C) = U32(a1, 0x20);
			const int16_t yaw = (int16_t)(x::CrtRand() & 0xFFF);
			const uint32_t r = x::CrtRand();
			memcpy((void *)m2, (const void *)base, 32);
			const uint32_t pitch = ((r & pitch_mask) + (uint32_t)pitch_add - (uint32_t)pitch_sub) & 0xFFF;
			x::MAG_022_sub_8DD8A0(m2, (uint32_t)(int32_t)yaw);
			x::sub_8DD7E0(m2, (uint32_t)(int32_t)(int16_t)pitch);
			memcpy((void *)mat, (const void *)m2, 32);
			const uint32_t r3 = x::CrtRand() & 0x1FFF;
			U16(vel, 2) = (uint16_t)(0xFFFFF000u - r3);
			x::matrixMultiplyVector(m2, vel, vel);
		}
	}

	// 0x70CA80 (sub_70CA80): spark burst state 1 - base orientation = the target's yaw (+0xE) then
	// roll 0x155 / pitch 0xEAB; per tick 1..4: 4 / 3 / 2 / 1 groups of steep sparks and 2 / 2 / 1 / 1
	// groups of flatter ones; finished (next state) from tick 4
	uint32_t __cdecl d_70CA80(uint32_t a1)
	{
		alignas(4) uint8_t base[32] = {}, m2[32] = {};
		const uint32_t slot = U8(a1, 0x2D);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		x::MAG_022_sub_8DD770(P(base));
		x::MAG_022_sub_8DD8A0(P(base), (uint32_t)(int32_t)S16(ent, 0xE));
		x::sub_8DD960(P(base), 0x155);
		x::sub_8DD7E0(P(base), 0xEAB);
		const int16_t c = S16(a1, 0x24);
		int32_t n = 0;
		if (c == 1) n = 4;
		else if (c == 2) n = 3;
		else if (c == 3) n = 2;
		else if (c == 4) n = 1;
		for (; n > 0; n--)
			SparkGroup(a1, P(base), P(m2), 0x1FF, 0, 0x100);
		n = 0;
		if (c == 1 || c == 2) n = 2;
		else if (c == 3 || c == 4) n = 1;
		for (; n > 0; n--)
			SparkGroup(a1, P(base), P(m2), 0xFF, 0x380, 0);
		if (S16(a1, 0x24) >= 4)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x70CEC0 (sub_70CEC0): draws one spark (prim model +0x4C, colour +0x40, fade +0x50, depth bias
	// +0x52) with matrix a2 scaled by +0x30.. at the position +0x1C.., through the camera, via the
	// module's prim renderer 0x707820 (header 0x68 bytes on the Field_Alloc stack) into the arena
	uint32_t __cdecl d_70CEC0(uint32_t a1, uint32_t a2)
	{
		if (U8(a1, 0x26) & 4)
			return 0; // void
		alignas(4) uint8_t mb[32];
		const uint32_t M = P(mb);
		memcpy(mb, (const void *)a2, 32);
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
		U32(h, 0xC) = (uint32_t)(int32_t)S16(a1, 0x50);
		U16(h, 0x22) = 0x100;
		U16(h, 0x20) = 0x100;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		U32(h, 0x10) = (uint32_t)(int32_t)S16(a1, 0x52);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0;
		U16(h, 0x1E) = 0;
		U16(h, 0x1C) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U32(h, 0x14) = 0xF0;
		MEM<uint32_t>(R(PACKET_CURSOR)) = d_707820(h, ot, 2, MEM<uint32_t>(R(PACKET_CURSOR)));
		x::Field_Free(0x68);
		return 0; // void
	}

	// the sparks' motion: velocity (+0xF4 + 8k, s16) loses a quarter, position (+0xD4 + 8k) moves by
	// velocity / 16 (C divisions)
	static void SparkMove(uint32_t node)
	{
		uint32_t v = node + 0xF4;
		for (int k = 4; k != 0; k--, v += 8)
		{
			for (int j = 0; j < 3; j++)
			{
				const int16_t s = S16(v, 2 * j);
				S16(v, 2 * j) = (int16_t)(s - (int16_t)(s / 4));
			}
			for (int j = 0; j < 3; j++)
				S16(v, -0x20 + 2 * j) = (int16_t)(S16(v, -0x20 + 2 * j) + (int16_t)(S16(v, 2 * j) / 16));
		}
	}

	// the sparks' scale (+0x30.. from the tables 0x1529720 / 948 / 968 by tick) and fade (+0x50,
	// 0x1529780), then the four draws
	static void SparkDraw(uint32_t node)
	{
		const int32_t e = S16(node, 0x24) * 2;
		const uint32_t table = fxp::data(PD().tables.spark, SPARK_TABLE, 0x80);   // (ticks 0..14 of its 16)
		S32(node, 0x30) = MEM<int16_t>(table + e);
		S32(node, 0x34) = MEM<int16_t>(table + 0x20 + e);
		S32(node, 0x38) = MEM<int16_t>(table + 0x40 + e);
		U16(node, 0x50) = MEM<uint16_t>(table + 0x60 + e);
		uint32_t pos = node + 0xD4;
		uint32_t mat = node + 0x54;
		for (int k = 4; k != 0; k--, pos += 8, mat += 0x20)
		{
			U32(node, 0x1C) = U32(pos, 0);
			U32(node, 0x20) = U32(pos, 4);
			d_70CEC0(node, mat);
		}
	}

	// 0x70CD70 (sub_70CD70): SPARK task (four sparks) - state {0x70CFC0 set-up, 0x70CFF0 finished at
	// tick 14, ret}, motion, draw
	uint32_t __cdecl d_70CD70(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x70CFC0;
		states[1] = 0x70CFF0;
		states[2] = 0x70D010; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		SparkMove(node);
		// 30 fps layer: see mag345_lv5_death_held.inc
		FX_HELD(held_note_spark(node);)
		SparkDraw(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x70CFC0 (sub_70CFC0): spark state 0 - scale 1:1:1, prim model 0x1524664, colour (0x28, 0,
	// 0x0A), next state
	uint32_t __cdecl d_70CFC0(uint32_t a1)
	{
		U32(a1, 0x38) = 0x1000;
		U32(a1, 0x34) = 0x1000;
		U32(a1, 0x30) = 0x1000;
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = fxp::model(PD().models.at_1524664, 0x1524664);
		U8(a1, 0x40) = 0x28;
		U8(a1, 0x41) = 0;
		U8(a1, 0x42) = 0xA;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x70CFF0 (sub_70CFF0): spark state 1 - finished from tick 14, next state
	uint32_t __cdecl d_70CFF0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xE)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// actor system drawing and allocation (the engine's code with the module's own packet cursor
	// 0x25570A4, draw header and pool sizes)
	// ====================================================================================

	// 0x7076C0 (sub_7076C0): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x707820
	uint32_t __cdecl d_7076C0(uint32_t a1, uint32_t a2)
	{
		const uint32_t h = MEM<uint32_t>(R(SCRATCH_SP)) - 0x68;
		U32(h, 0) = U32(a1, 0x170);
		const bool def3a = U16(a2, 0x3A) != 0;
		MEM<uint32_t>(R(SCRATCH_SP)) = h;
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
			MEM<uint32_t>(R(PACKET_CURSOR)) = d_707820(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(R(PACKET_CURSOR)));
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
				MEM<uint32_t>(R(PACKET_CURSOR)) = d_707820(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(R(PACKET_CURSOR)));
			}
		}
		const uint32_t top = MEM<uint32_t>(R(SCRATCH_SP)) + 0x68;
		MEM<uint32_t>(R(SCRATCH_SP)) = top;
		return top;
	}

	// 0x709B00 (sub_709B00): the engine's 0x7407C0 (sprite sequence of an actor: header on the module
	// scratch stack, sequence +0x170, variant +0x1D0, matrix +0xAC) drawing into the module arena
	uint32_t __cdecl d_709B00(uint32_t a1)
	{
		const uint32_t act = a1;
		const uint32_t hdr = MEM<uint32_t>(R(SCRATCH_SP)) - 0xB4;
		MEM<uint32_t>(R(SCRATCH_SP)) = hdr;
		const uint32_t mtx = act + 0xAC;
		x::GTE_SetRotMatrix(mtx);
		x::GTE_SetTransVector(mtx);
		const uint32_t seq = U32(act, 0x170);
		const uint32_t cursor = MEM<uint32_t>(R(PACKET_CURSOR));
		const int16_t variant = (int16_t)S8(act, 0x1D0);
		U32(hdr, 0) = seq;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		S16(hdr, 4) = variant;
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(R(PACKET_CURSOR)) = x::InitEffectSequenceFromData_c19(hdr, ot, 2, cursor);
		MEM<uint32_t>(R(SCRATCH_SP)) = MEM<uint32_t>(R(SCRATCH_SP)) + 0xB4;
		return 0; // void
	}

	// 0x70ADB0 (sub_70ADB0): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x255EA44], cursor 0x2557758) with the module's pool of 0x14 records
	uint32_t __cdecl d_70ADB0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(R(0x255EA44));
		int32_t idx = MEM<int16_t>(R(0x2557758));
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				const uint32_t dir = MEM<uint32_t>(R(ACTOR_STATE));
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
			MEM<uint16_t>(R(0x2557758)) = (uint16_t)idx;
		else
			MEM<uint16_t>(R(0x2557758)) = 0;
		return slot;
	}

	// 0x70B730 (sub_70B730): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x255EA88], cursor 0x255EA50) with the module's pool of 0x3C records
	uint32_t __cdecl d_70B730(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(R(0x255EA88));
		int32_t i = MEM<int16_t>(R(0x255EA50));
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
				const uint32_t sys = MEM<uint32_t>(R(ACTOR_STATE));
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
			MEM<uint16_t>(R(0x255EA50)) = 0;
		else
			MEM<uint16_t>(R(0x255EA50)) = (uint16_t)i;
		return slot;
	}

	// ====================================================================================
	// the library functions of this module whose code matches an engine / Tonberry / Boko /
	// MiniMog port up to the module's addresses and callees (the port's text with the module's
	// addresses)
	// ====================================================================================
	// 0x705C00 (copy of b_729BD0 0x729BD0; Boko master task, 097 MAG_097_sub_729BD0, 098 0x7219D0): copies the camera matrix,
	// picks this tick's double-buffered globals, runs the state handler +0x29, then the 7 task queues
	uint32_t __cdecl d_705C00(uint32_t a1)
	{
		g_mod = &MOD_345;
		g_reloc = 0;   // (its own memory: Fujin's described copies move it)
		// 30 fps layer: see act_engine_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(R(0x255EA84)) = 0x2793E58;
		MEM<uint32_t>(R(0x2557208)) = 0x2793E58;
		states[0] = 0x705D80;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x705D90;
		states[2] = 0x705DA0;
		states[3] = 0x705E20;
		states[4] = 0x70D2A0;
		states[5] = 0x70D2E0;
		states[6] = 0x70D2F0;
		states[7] = 0x70D300;
		states[8] = 0x70D330;
		states[9] = 0x70D340;
		states[10] = 0x70D360;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(R(0x255DC0C));
			const uint32_t v2 = MEM<uint32_t>(R(0x255CD64));
			MEM<uint32_t>(R(0x25570A4)) = v1;
			MEM<uint32_t>(R(0x2557204)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(R(0x255DC08));
			const uint32_t v2 = MEM<uint32_t>(R(0x255CD60));
			MEM<uint32_t>(R(0x25570A4)) = v1;
			MEM<uint32_t>(R(0x2557204)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(R(0x255EA7C)) = 0;
		MEM<uint16_t>(R(0x255EA40)) = 0;
		const uint32_t queues[7] = { R(0x255DA98), R(0x25571E0), R(0x255EA30), R(0x255D7F8),
			R(0x25572E8), R(0x2557760), R(0x255DA88) };
		for (int i = 0; i < 7; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x706AE0 (copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs the director set-up 0x706B10 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl d_706AE0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			d_706B10(a1);
			d_7075D0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x706CA0 (copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl d_706CA0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(R(0x255EA94));
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

	// 0x706F20 (copy of b_72AFE0 0x72AFE0; 097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl d_706F20(void)
	{
		const uint32_t arena = MEM<uint32_t>(R(0x255EA94));
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

	// 0x7075D0 (copy of a_7353B0 0x7353B0; engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl d_7075D0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(R(0x255EA94), 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			d_70C4D0();
			d_709B80();
			// 30 fps layer: see mag345_lv5_death_held.inc
			FX_HELD(held_note_system(sys);)
			d_709A40();
			d_707660();
			sys = U32(R(0x255EA94), 0);
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
		U16(R(0x255EA7C), 0) = (uint16_t)(U16(R(0x255EA7C), 0) + c14);
		U16(R(0x255EA40), 0) = (uint16_t)(U16(R(0x255EA40), 0) + c16);
		return done;
	}

	// 0x707660 (copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl d_707660(void)
	{
		uint32_t act = U32(U32(R(0x255EA94), 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(R(0x255EA94), 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					d_7076C0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x707820 (copy of t_764540 0x764540; module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri 0x707950, 0x707B70, 0x707E00, 0x7082F0, a_73D770, a_73D9C0, 0x708E10,
	// 0x709370; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl d_707820(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { d_707950, d_707B70, d_707E00, d_7082F0, a_73D770, a_73D9C0, d_708E10, d_709370 };
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

	// 0x707950 (copy of t_764670 0x764670; module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl d_707950(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x707B70 (copy of t_764890 0x764890; module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl d_707B70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x707E00 (copy of t_764B20 0x764B20; module 090 sub_764B20): draws the textured-triangle list of render context a1
	// (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage,
	// uv2 in the high half of +8): RTPT, rejects on GTE FLAG / back-face / fully off-screen, optional
	// depth cue, emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ(AVSZ3) + bias) >> a3.
	// With a UV scroll (+0x18/+0x1A) the texel bytes are scrolled (wrapped by +0x28/+0x2A) and the
	// poly is bracketed by two 0xE2 texture-window prims (0xC B each, windows +0x1C / +0x24);
	// without one a DR_MODE prim (0xC B, tpage abr 1) is inserted before the poly.
	uint32_t __cdecl d_707E00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x7082F0 (copy of t_765010 0x765010; module 090 sub_765010): draws the textured-quad list of render context a1 (count,
	// then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16):
	// RTPT + RTPS, rejects on GTE FLAG / back-face / fully off-screen, optional depth cue, emits one
	// POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. With a UV scroll the texel
	// bytes are scrolled and the poly is bracketed by two 0xE2 texture-window prims; without one a
	// DR_MODE prim (tpage abr 1) is inserted before the poly.
	uint32_t __cdecl d_7082F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x708E10 (copy of t_765B30 0x765B30; module 090 sub_765B30): prim-list block "Gouraud-textured triangles, UV scroll":
	// 0x1C-byte records (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, rgb1 +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face
	// (NCLIP, unless ctx+0x14 bit 0x20) and off-screen culling, optional lighting (bit 0x80),
	// z = OTZ + ctx+0x10 (>= 0) >> a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the
	// UVs are scrolled (wrapped by ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window
	// packets (ctx+0x1C set, ctx+0x24 restore); else followed by one draw-mode packet (tpage abr 1).
	// Returns the new packet cursor; ctx+0x2C advances past the block.
	uint32_t __cdecl d_708E10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x709370 (copy of t_766090 0x766090; module 090 sub_766090): prim-list block "Gouraud-textured quads, UV scroll":
	// 0x24-byte records (code/rgb0 +0, vertex indices +4/+6/+8/+0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, uv2 +0x14 / uv3 +0x16, rgb1..3 +0x18/+0x1C/+0x20) -> POLY_GT4 (tag
	// 0x0C000000), RTPT + RTPS for the 4th corner, AVSZ4, same culling / lighting / z / scroll /
	// texture-window framing as 0x765B30. Returns the new packet cursor; ctx+0x2C advances.
	uint32_t __cdecl d_709370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x709A40 (copy of a_740700 0x740700; engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl d_709A40(void)
	{
		uint32_t act = U32(MEM<uint32_t>(R(0x255EA94)), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(R(0x255EA94));
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						d_709B00(act);   // (the original also pushes the bone entry, unused)
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
							d_709B00(act);
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

	// 0x709B80 (copy of a_736C00 0x736C00; engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl d_709B80(void)
	{
		uint32_t act = U32(U32(R(0x255EA94), 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				d_70AE50(act);
			else if (type == 1)
				d_709BC0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x709BC0 (copy of a_740880 0x740880; engine; Siren sub_740880): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl d_709BC0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(R(0x255EA94));
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (d_70AD00(act, bone) == 0)
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
			st = MEM<uint32_t>(R(0x255EA94));
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x70ABF0 (copy of b_72E220 0x72E220; Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl d_70ABF0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(R(0x255EA94), 0);                  // actor state block
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
				dx::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				dx::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				dx::sub_45E0B0(in2);
				dx::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x70AD00 (copy of a_7419C0 0x7419C0; engine; Siren sub_7419C0, in 095-100): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl d_70AD00(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				d_70AD80(a1, a2);
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
			d_70AD80(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			d_70AD80(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x70AD80 (copy of a_741A40 0x741A40; engine; Siren sub_741A40, in 095-100): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl d_70AD80(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return d_70ADB0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x70AE50 (copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl d_70AE50(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x70B800, a1);
		else if (mode == 4)
			callp(0x70B950, a1);
		d_70AED0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x70AED0 (copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl d_70AED0(uint32_t a1)
	{
		uint32_t sys = U32(R(0x255EA94), 0);
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
				d_70AF50(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			d_70AF50(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x70AF50 (copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl d_70AF50(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(R(0x255EA84), 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(R(0x255EA84), 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = d_70B730(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(R(0x255EA94), 0);
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
		d_70C440(node, desc);
		U32(R(0x255EA84), 0) = U32(R(0x255EA84), 0) + 0x50;
		return 0; // void
	}

	// 0x70B950 (copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl d_70B950(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(R(0x255EA94), 0);            // ecx
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
				uint32_t g = U32(R(0x255EA94), 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(R(0x255EA94), 0);
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

	// ====================================================================================
	// part e5
	// ====================================================================================
	// Part e5: actor state-machine engine helpers 0x743100-0x7475A0 (Siren canonical addresses):
	// sprite-strip sequence init/draw (0x7435E0/0x743720), creature model draw (0x746C10), prim-model
	// draw (0x7458E0), sub-effect spawners (0x743100/0x743190) and the small state-table handlers.
	// 0x743100 (engine; Siren sub_743100): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl d_70C440(uint32_t a1, uint32_t a2)
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
			d_70ADB0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			d_70ADB0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			d_70ADB0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			d_70ADB0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x70C4D0 (copy of a_743190 0x743190; engine; Siren sub_743190): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via d_70ADB0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl d_70C4D0(void)
	{
		uint32_t dir = MEM<uint32_t>(R(0x255EA94));  // eax (re-read only after the calls)
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
					d_70ADB0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(R(0x255EA94));
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				d_70ADB0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(R(0x255EA94));
			}
		}
		return 0; // void
	}

	// 0x70C560 (copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl d_70C560(uint32_t a1)
	{
		if (d_7075D0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x70C590 (copy of t_7678B0 0x7678B0; module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl d_70C590(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x70C6B0 (copy of t_768130 0x768130; module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl d_70C6B0(uint32_t a1)
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

	// 0x70D160 (copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl d_70D160(uint32_t a1)
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

	// 0x70D210 (copy of t_768370 0x768370; module 090 MAG_090_sub_768370): damage state - when [[0x1524658]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl d_70D210(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(G(G_1533010));
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

	// ====================================================================================
	// LV?Death's own versions of two engine functions (Death serves them with a_73F990 / a_747500)
	// ====================================================================================
	// 0x706B10 (sub_706B10; Death 0x8B7950 = a_73F990 0x73F990): actor director set-up for node a1, as
	// a_73F990 but with ONE target: *0x255EA94 = state block node+0x34; copies node+0x29A / 0x29C (mode)
	// into it, points its 4 tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once
	// by 0x707370, flag data+0), the caster (+0x2C) and the target list = count 1, the slot of target
	// +0x2B of action +0x2A of the cast context, +0x1A = node+0x29E; start / per-target / bounds positions
	// (0x706D80, 0x707110; modes 1 / 3 / 4 extra set-ups) and the caster-to-target distance in state+0.
	uint32_t __cdecl d_706B10(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		const uint16_t w29a = U16(node, 0x29A);
		const uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint16_t caster = (uint16_t)U8(node, 0x2C); // bx (movzx)
		U16(st, 0x22) = w29a;
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		const uint32_t relocated = U32(data, 0);
		MEM<uint32_t>(R(ACTOR_STATE)) = st;
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(R(ACTOR_STATE));
			U32(data, 0) = 1;
		}
		const int32_t action = S8(node, 0x2A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = 1;
		const uint32_t rec = U32(U32(U32(node, 0x0C), 4) + (uint32_t)mul32(action, 5) * 4, 8);
		const int32_t target = S8(node, 0x2B);
		U32(st, 4) = U8(rec, (int32_t)mul32(target, 3) * 8);   // target slot
		U16(st, 0x1A) = U16(node, 0x29E);
		a_73FC20();
		st = MEM<uint32_t>(R(ACTOR_STATE));
		if (U16(st, 0x24) == 4)
			callp(F(F_73FB40), node);
		a_73FFB0();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(R(ACTOR_STATE));
		if (U16(st, 0x24) == 1)
		{
			callp(F(F_73FDC0), node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(R(ACTOR_STATE));
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(R(ACTOR_STATE));
		}
		// distance caster entity -> the target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		const int32_t s1 = S16(st, 0x1E);
		const uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		const uint32_t d0c = U32(e1, 0x1C);
		const uint32_t d10 = U32(e1, 0x20);
		const uint32_t s2 = U32(st, 4);
		const uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		const uint32_t d14 = U32(e2, 0x1C);
		const int32_t dx = (int16_t)(uint16_t)((uint16_t)d0c - (uint16_t)d14);
		const int32_t dy = (int16_t)(uint16_t)((uint16_t)(d0c >> 16) - (uint16_t)(d14 >> 16));
		const int32_t dz = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)U32(e2, 0x20));
		const int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		const uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(R(ACTOR_STATE));
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x70D2A0 (MAG_345_sub_70D2A0; Death 0x8BE100 = a_747500 0x747500): master state 4 - unless paused
	// (+0x63): while the target index +0x2B < target count +0x5A - 1: next target (+0x2B), ++emitter count
	// +0x2E and back to the spawn state (+0x29 - 1); then next state
	uint32_t __cdecl d_70D2A0(uint32_t a1)
	{
		if (U8(a1, 0x63) != 0)
			return 0; // void
		const int8_t t = S8(a1, 0x2B);
		if ((int32_t)t < (int32_t)S16(a1, 0x5A) - 1)
		{
			U8(a1, 0x2B) = (uint8_t)(t + 1);
			U8(a1, 0x2E) = (uint8_t)(U8(a1, 0x2E) + 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) - 1);
			return 0; // void
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	namespace table
	{
		using fxp::Field;
#define DT(part, member, label, doc, lo, hi) { part, #member, label, doc, (uint32_t)offsetof(Lv5DeathParams, member), fxp::I16, lo, hi }
		static const fxp::Part PARTS[] = {
			{ 0, "reaper", "Reaper", "A reaper in front of each target: fading in in the dark, striking, vanishing; its sounds" },
			{ 0, "actors", "Actor systems", "The aura's actor system at its appearance, the dark one at the strike (their actor data the exe's)" },
		};
		static const fxp::Group GROUPS[] = {
			{ "aura", "Aura", "Its actor system at the reaper's appearance: Ticks, Mode (2: the reaper data's fixed positions)" },
			{ "strike", "Strike", "The dark actor system at the strike: Ticks, Mode (0: the bones)" },
		};
		static const Field FIELDS[] = {
			{ "reaper", "sound.appear", "Appear sound", "Played when it appears (the first target)", (uint32_t)offsetof(Lv5DeathParams, sound.appear), fxp::Sound, 0, 0x7FFFFFFF },
			{ "reaper", "sound.strike", "Strike sound", "Played when its dark draw ends", (uint32_t)offsetof(Lv5DeathParams, sound.strike), fxp::Sound, 0, 0x7FFFFFFF },
			DT("reaper", reaper.fade_in, "Fade in", "Its fade down by this a tick (4096: fully shown)", 1, 0x1000),
			DT("reaper", reaper.dark_until, "Dark until frame", "Drawn dark until this frame of its animation", 0, 200),
			DT("reaper", reaper.strike_frame, "Strike frame", "It strikes (the damage, the dark actor system, the sparks) at this frame", 0, 200),
			DT("reaper", reaper.vanish_frame, "Vanish from frame", "It vanishes from this frame", 0, 200),
			DT("reaper", reaper.fade_out, "Fade out", "Its fade up by this a tick", 1, 0x1000),
			DT("actors", aura.length, "Ticks", "", 0, 1000),
			DT("actors", aura.mode, "Mode", "", 0, 4),
			DT("actors", strike.length, "Ticks", "", 0, 1000),
			DT("actors", strike.mode, "Mode", "", 0, 4),
			{ "reaper", "res.tim_a", "TIM image a", "The TIM image it uploads as it is (a palette when the game's has one)", (uint32_t)offsetof(Lv5DeathParams, res.tim_a), fxp::Tim, 0, 0x7FFFFFFF },
			{ "reaper", "res.tim_b", "TIM image b", "The TIM image it uploads as it is (a palette when the game's has one)", (uint32_t)offsetof(Lv5DeathParams, res.tim_b), fxp::Tim, 0, 0x7FFFFFFF },
			{ "reaper", "tables.spark", "Spark sizes", "The dark sparks' scale x, y, z and fade (4096: 1) at each tick of their 15 (128 bytes: an edited one keeps its length)", (uint32_t)offsetof(Lv5DeathParams, tables.spark), fxp::Data, 0, 0x7FFFFFFF },
			{ "reaper", "models.at_1524664", "Model 1524664", "The prim model at 0x1524664 (a mesh of 9 vertices, 8 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(Lv5DeathParams, models.at_1524664), fxp::Model, 0, 0x7FFFFFFF },
		};
#undef DT
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x705D80, (void *)a_73A0D0, "345 MAG_345_sub_705D80" },
		{ 0x705D90, (void *)a_73A0D0, "345 MAG_345_sub_705D90" },
		{ 0x705E20, (void *)a_73A170, "345 MAG_345_sub_705E20" },
		{ 0x705E50, (void *)a_73A1A0, "345 MAG_345_sub_705E50" },
		{ 0x706270, (void *)a_746C10, "345 sub_706270" },
		{ 0x706A30, (void *)a_73C100, "345 sub_706A30" },
		{ 0x706A80, (void *)a_73A380, "345 sub_706A80" },
		{ 0x706D80, (void *)a_73FC20, "345 sub_706D80" },
		{ 0x706FF0, (void *)a_73FE90, "345 sub_706FF0" },
		{ 0x707110, (void *)a_73FFB0, "345 sub_707110" },
		{ 0x707370, (void *)a_740210, "345 sub_707370" },
		{ 0x7088E0, (void *)a_73D770, "345 sub_7088E0" },
		{ 0x708B30, (void *)a_73D9C0, "345 sub_708B30" },
		{ 0x709C50, (void *)a_740910, "345 sub_709C50" },
		{ 0x70A2B0, (void *)a_740F70, "345 sub_70A2B0" },
		{ 0x70A300, (void *)a_740FC0, "345 sub_70A300" },
		{ 0x70A390, (void *)a_741050, "345 sub_70A390" },
		{ 0x70A460, (void *)a_741120, "345 sub_70A460" },
		{ 0x70AEA0, (void *)a_741B60, "345 sub_70AEA0" },
		{ 0x70B5D0, (void *)a_742290, "345 au_re__rand_2" },
		{ 0x70B600, (void *)a_7422C0, "345 sub_70B600" },
		{ 0x70B640, (void *)a_742300, "345 sub_70B640" },
		{ 0x70B690, (void *)a_742350, "345 sub_70B690" },
		{ 0x70B6E0, (void *)a_7423A0, "345 au_re__rand_2_0" },
		{ 0x70B7F0, (void *)a_7424B0, "345 sub_70B7F0" },
		{ 0x70B800, (void *)a_7424C0, "345 sub_70B800" },
		{ 0x70BFE0, (void *)a_742CA0, "345 sub_70BFE0" },
		{ 0x70C010, (void *)a_742CD0, "345 sub_70C010" },
		{ 0x70C040, (void *)a_742D00, "345 sub_70C040" },
		{ 0x70C1F0, (void *)a_742EB0, "345 sub_70C1F0" },
		{ 0x70C330, (void *)a_742FF0, "345 sub_70C330" },
		{ 0x70C580, (void *)a_73A6D0, "345 nullsub_1058" },
		{ 0x70C5B0, (void *)a_733950, "345 sub_70C5B0" },
		{ 0x70C720, (void *)a_73A380, "345 sub_70C720" },
		{ 0x70C800, (void *)a_73A6D0, "345 nullsub_1059" },
		{ 0x70C9E0, (void *)a_73A380, "345 sub_70C9E0" },
		{ 0x70D010, (void *)a_73A6D0, "345 nullsub_1060" },
		{ 0x70D020, (void *)a_73A6D0, "345 nullsub_1061" },
		{ 0x70D0E0, (void *)a_73A380, "345 sub_70D0E0" },
		{ 0x70D1A0, (void *)a_73A6D0, "345 nullsub_1062" },
		{ 0x70D1B0, (void *)a_73A6D0, "345 nullsub_1063" },
		{ 0x70D1C0, (void *)a_7475A0, "345 sub_70D1C0" },
		{ 0x70D1E0, (void *)a_73A6D0, "345 nullsub_1064" },
		{ 0x70D1F0, (void *)a_73A6D0, "345 nullsub_1065" },
		{ 0x70D200, (void *)a_73A6D0, "345 nullsub_1066" },
		{ 0x70D250, (void *)a_7474B0, "345 MAG_345_sub_70D250" },
		{ 0x70D270, (void *)a_7474D0, "345 MAG_345_sub_70D270" },
		{ 0x70D290, (void *)a_73A6D0, "345 nullsub_1067" },
		{ 0x70D2E0, (void *)a_73A0D0, "345 MAG_345_sub_70D2E0" },
		{ 0x70D2F0, (void *)a_747550, "345 MAG_345_sub_70D2F0" },
		{ 0x70D300, (void *)a_747560, "345 MAG_345_sub_70D300" },
		{ 0x70D330, (void *)a_747590, "345 MAG_345_sub_70D330" },
		{ 0x70D340, (void *)a_7475A0, "345 MAG_345_sub_70D340" },
		{ 0x70D360, (void *)a_73A6D0, "345 nullsub_1057" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x705DE0, (void *)d_705DE0, "d_705DE0" },
		{ 0x705DA0, (void *)d_705DA0, "d_705DA0" },
		{ 0x705F00, (void *)d_705F00, "d_705F00" },
		{ 0x705ED0, (void *)d_705ED0, "d_705ED0" },
		{ 0x706060, (void *)d_706060, "d_706060" },
		{ 0x705FF0, (void *)d_705FF0, "d_705FF0" },
		{ 0x7060B0, (void *)d_7060B0, "d_7060B0" },
		{ 0x7060F0, (void *)d_7060F0, "d_7060F0" },
		{ 0x706550, (void *)d_706550, "d_706550" },
		{ 0x706450, (void *)d_706450, "d_706450" },
		{ 0x7061F0, (void *)d_7061F0, "d_7061F0" },
		{ 0x706130, (void *)d_706130, "d_706130" },
		{ 0x70C8A0, (void *)d_70C8A0, "d_70C8A0" },
		{ 0x70C840, (void *)d_70C840, "d_70C840" },
		{ 0x70C810, (void *)d_70C810, "d_70C810" },
		{ 0x706940, (void *)d_706940, "d_706940" },
		{ 0x70C8B0, (void *)d_70C8B0, "d_70C8B0" },
		{ 0x70C900, (void *)d_70C900, "d_70C900" },
		{ 0x70D030, (void *)d_70D030, "d_70D030" },
		{ 0x70D090, (void *)d_70D090, "d_70D090" },
		{ 0x70C780, (void *)d_70C780, "d_70C780" },
		{ 0x70C7C0, (void *)d_70C7C0, "d_70C7C0" },
		{ 0x70D140, (void *)d_70D140, "d_70D140" },
		{ 0x70CA40, (void *)d_70CA40, "d_70CA40" },
		{ 0x70CA80, (void *)d_70CA80, "d_70CA80" },
		{ 0x70CEC0, (void *)d_70CEC0, "d_70CEC0" },
		{ 0x70CD70, (void *)d_70CD70, "d_70CD70" },
		{ 0x70CFC0, (void *)d_70CFC0, "d_70CFC0" },
		{ 0x70CFF0, (void *)d_70CFF0, "d_70CFF0" },
		{ 0x7076C0, (void *)d_7076C0, "d_7076C0" },
		{ 0x709B00, (void *)d_709B00, "d_709B00" },
		{ 0x70ADB0, (void *)d_70ADB0, "d_70ADB0" },
		{ 0x70B730, (void *)d_70B730, "d_70B730" },
		{ 0x705C00, (void *)d_705C00, "d_705C00" },
		{ 0x706AE0, (void *)d_706AE0, "d_706AE0" },
		{ 0x706CA0, (void *)d_706CA0, "d_706CA0" },
		{ 0x706F20, (void *)d_706F20, "d_706F20" },
		{ 0x7075D0, (void *)d_7075D0, "d_7075D0" },
		{ 0x707660, (void *)d_707660, "d_707660" },
		{ 0x707820, (void *)d_707820, "d_707820" },
		{ 0x707950, (void *)d_707950, "d_707950" },
		{ 0x707B70, (void *)d_707B70, "d_707B70" },
		{ 0x707E00, (void *)d_707E00, "d_707E00" },
		{ 0x7082F0, (void *)d_7082F0, "d_7082F0" },
		{ 0x708E10, (void *)d_708E10, "d_708E10" },
		{ 0x709370, (void *)d_709370, "d_709370" },
		{ 0x709A40, (void *)d_709A40, "d_709A40" },
		{ 0x709B80, (void *)d_709B80, "d_709B80" },
		{ 0x709BC0, (void *)d_709BC0, "d_709BC0" },
		{ 0x70ABF0, (void *)d_70ABF0, "d_70ABF0" },
		{ 0x70AD00, (void *)d_70AD00, "d_70AD00" },
		{ 0x70AD80, (void *)d_70AD80, "d_70AD80" },
		{ 0x70AE50, (void *)d_70AE50, "d_70AE50" },
		{ 0x70AED0, (void *)d_70AED0, "d_70AED0" },
		{ 0x70AF50, (void *)d_70AF50, "d_70AF50" },
		{ 0x70B950, (void *)d_70B950, "d_70B950" },
		{ 0x70C440, (void *)d_70C440, "d_70C440" },
		{ 0x70C4D0, (void *)d_70C4D0, "d_70C4D0" },
		{ 0x70C560, (void *)d_70C560, "d_70C560" },
		{ 0x70C590, (void *)d_70C590, "d_70C590" },
		{ 0x70C6B0, (void *)d_70C6B0, "d_70C6B0" },
		{ 0x70D160, (void *)d_70D160, "d_70D160" },
		{ 0x70D210, (void *)d_70D210, "d_70D210" },
		{ 0x706B10, (void *)d_706B10, "d_706B10" },
		{ 0x70D2A0, (void *)d_70D2A0, "d_70D2A0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag345_lv5_death()
	{
		fxp::register_params({ 345, "LV5 Death", &act::lv5_death::live, &act::lv5_death::GAME,
		                       sizeof(act::lv5_death::Lv5DeathParams),
		                       act::lv5_death::table::PARTS, (int)(sizeof(act::lv5_death::table::PARTS) / sizeof(act::lv5_death::table::PARTS[0])),
		                       act::lv5_death::table::FIELDS, (int)(sizeof(act::lv5_death::table::FIELDS) / sizeof(act::lv5_death::table::FIELDS[0])),
		                       act::lv5_death::table::GROUPS, (int)(sizeof(act::lv5_death::table::GROUPS) / sizeof(act::lv5_death::table::GROUPS[0])) });
		act::register_mod(&act::lv5_death::MOD_345);
		act::register_module(345);
		for (const act::lv5_death::ModPort *p = act::lv5_death::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(345, p->addr, p->port, p->name);
		for (const act::lv5_death::ModPort *p = act::lv5_death::PORTS; p->addr; p++)
			act::register_module_port(345, p->addr, p->port, p->name);
		// 30 fps layer: see mag345_lv5_death_held.inc
		FX_HELD(register_mag345_held();)
	}
	FX_LAB_MODULE(register_mag345_lv5_death);
}

#ifdef FF8_FX_HELD
#include "mag345_lv5_death_held.inc"
#endif
